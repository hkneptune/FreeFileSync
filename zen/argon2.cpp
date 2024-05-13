// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

/*  The code in this file, except for zen::zargon2(), is from PuTTY:

    PuTTY is copyright 1997-2022 Simon Tatham.

    Portions copyright Robert de Bath, Joris van Rantwijk, Delian
    Delchev, Andreas Schultz, Jeroen Massar, Wez Furlong, Nicolas Barry,
    Justin Bradford, Ben Harris, Malcolm Smith, Ahmad Khalifa, Markus
    Kuhn, Colin Watson, Christopher Staite, Lorenz Diener, Christian
    Brabandt, Jeff Smith, Pavel Kryukov, Maxim Kuznetsov, Svyatoslav
    Kuzmich, Nico Williams, Viktor Dukhovni, Josh Dersch, Lars Brinkhoff,
    and CORE SDI S.A.

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation files
    (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge,
    publish, distribute, sublicense, and/or sell copies of the Software,
    and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE
    FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.           */

#include "argon2.h"
#include <cassert>
//#include <cstring>
#include <cstdint>
//#include <cstdlib>

#if   defined __GNUC__ //including clang
    #pragma GCC diagnostic ignored "-Wimplicit-fallthrough" //"this statement may fall through"
    #pragma GCC diagnostic ignored "-Wcast-align" //"cast from 'char *' to 'blake2b *' increases required alignment from 1 to 8"
#endif

/*
 * Implementation of the Argon2 password hash function.
 *
 * My sources for the algorithm description and test vectors (the latter in
 * test/cryptsuite.py) were the reference implementation on Github, and also
 * the Internet-Draft description:
 *
 *   https://github.com/P-H-C/phc-winner-argon2
 *   https://datatracker.ietf.org/doc/html/draft-irtf-cfrg-argon2-13
 */


/* ----------------------------------------------------------------------
 *
 * A sort of 'abstract base class' or 'interface' or 'trait' which is
 * the common feature of all types that want to accept data formatted
 * using the SSH binary conventions of uint32, string, mpint etc.
 */
typedef struct BinarySink BinarySink;

struct BinarySink
{
    void (*write)(BinarySink* sink, const void* data, size_t len);
    void (*writefmtv)(BinarySink* sink, const char* fmt, va_list ap);
    BinarySink* binarysink_;
};

#define BinarySink_INIT(obj, writefn) \
    ((obj)->binarysink_->write = (writefn), \
     (obj)->binarysink_->writefmtv = NULL, \
     (obj)->binarysink_->binarysink_ = (obj)->binarysink_)

#define BinarySink_DELEGATE_IMPLEMENTATION BinarySink *binarysink_

#define BinarySink_DELEGATE_INIT(obj, othersink) ((obj)->binarysink_ = BinarySink_UPCAST(othersink))

#define BinarySink_DOWNCAST(object, type)                               \
    TYPECHECK((object) == ((type *)0)->binarysink_,                     \
              ((type *)(((char *)(object)) - offsetof(type, binarysink_))))

#define BinarySink_IMPLEMENTATION BinarySink binarysink_[1]

/* Return a pointer to the object of structure type 'type' whose field
 * with name 'field' is pointed at by 'object'. */
#define container_of(object, type, field)                               \
    TYPECHECK(object == &((type *)0)->field,                            \
              ((type *)(((char *)(object)) - offsetof(type, field))))


static void no_op(void* /*ptr*/, size_t /*size*/) {}

static void (*const volatile maybe_read)(void* ptr, size_t size) = no_op;

void smemclr(void* b, size_t n)
{
    if (b && n > 0)
    {
        /*
         * Zero out the memory.
         */
        memset(b, 0, n);

        /*
         * Call the above function pointer, which (for all the
         * compiler knows) might check that we've really zeroed the
         * memory.
         */
        maybe_read(b, n);
    }
}

void* safemalloc(size_t factor1, size_t factor2, size_t addend)
{
    if (factor1 > SIZE_MAX / factor2)
        return nullptr;
    size_t product = factor1 * factor2;

    if (addend > SIZE_MAX)
        return nullptr;
    if (product > SIZE_MAX - addend)
        return nullptr;
    size_t size = product + addend;

    if (size == 0)
        size = 1;

    return malloc(size);
}

void safefree(void* ptr)
{
    if (ptr)
        free(ptr);
}


#define snmalloc safemalloc
#define smalloc(z) safemalloc(z,1,0)

#define snewn(n, type) ((type *)snmalloc((n), sizeof(type), 0))
#define snew(type)     ((type *) smalloc (sizeof (type)) )

#define sfree safefree


/*
 * A small structure wrapping up a (pointer, length) pair so that it
 * can be conveniently passed to or from a function.
 */
typedef struct ptrlen
{
    const void* ptr;
    size_t len;
} ptrlen;


struct ssh_hash
{
    //const ssh_hashalg* vt;
    BinarySink_DELEGATE_IMPLEMENTATION;
};


static inline void PUT_32BIT_LSB_FIRST(void* vp, uint32_t value)
{
    uint8_t* p = (uint8_t*)vp;
    p[0] = (uint8_t)((value      ) & 0xff);
    p[1] = (uint8_t)((value >>  8) & 0xff);
    p[2] = (uint8_t)((value >> 16) & 0xff);
    p[3] = (uint8_t)((value >> 24) & 0xff);
}


static inline uint64_t GET_64BIT_LSB_FIRST(const void* vp)
{
    const uint8_t* p = (const uint8_t*)vp;
    return (((uint64_t)p[0]      ) | ((uint64_t)p[1] <<  8) |
            ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24) |
            ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) |
            ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56));
}


static inline void PUT_64BIT_LSB_FIRST(void* vp, uint64_t value)
{
    uint8_t* p = (uint8_t*)vp;
    p[0] = (uint8_t)((value      ) & 0xff);
    p[1] = (uint8_t)((value >>  8) & 0xff);
    p[2] = (uint8_t)((value >> 16) & 0xff);
    p[3] = (uint8_t)((value >> 24) & 0xff);
    p[4] = (uint8_t)((value >> 32) & 0xff);
    p[5] = (uint8_t)((value >> 40) & 0xff);
    p[6] = (uint8_t)((value >> 48) & 0xff);
    p[7] = (uint8_t)((value >> 56) & 0xff);
}


static void BinarySink_put_uint32_le(BinarySink* bs, unsigned long val)
{
    unsigned char data[4];
    PUT_32BIT_LSB_FIRST(data, val);
    bs->write(bs, data, sizeof(data));
}

static void BinarySink_put_stringpl_le(BinarySink* bs, ptrlen pl)
{
    /* Check that the string length fits in a uint32, without doing a
     * potentially implementation-defined shift of more than 31 bits */
    assert((pl.len >> 31) < 2);

    BinarySink_put_uint32_le(bs, pl.len);
    bs->write(bs, pl.ptr, pl.len);
}


#define TYPECHECK(to_check, to_return)                  \
    (sizeof(to_check) ? (to_return) : (to_return))


#define BinarySink_UPCAST(object)                                       \
    TYPECHECK((object)->binarysink_ == (BinarySink *)0,                 \
              (object)->binarysink_)

#define put_uint32_le(bs, val) \
    BinarySink_put_uint32_le(BinarySink_UPCAST(bs), val)
#define put_stringpl_le(bs, val) \
    BinarySink_put_stringpl_le(BinarySink_UPCAST(bs), val)


static inline uint32_t GET_32BIT_LSB_FIRST(const void* vp)
{
    const uint8_t* p = (const uint8_t*)vp;
    return (((uint32_t)p[0]      ) | ((uint32_t)p[1] <<  8) |
            ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}


void memxor(uint8_t* out, const uint8_t* in1, const uint8_t* in2, size_t size)
{
    switch (size & 15)
    {
        case 0:
            while (size >= 16)
            {
                size -= 16;
                *out++ = *in1++ ^ *in2++;
            case 15:
                *out++ = *in1++ ^ *in2++;
            case 14:
                *out++ = *in1++ ^ *in2++;
            case 13:
                *out++ = *in1++ ^ *in2++;
            case 12:
                *out++ = *in1++ ^ *in2++;
            case 11:
                *out++ = *in1++ ^ *in2++;
            case 10:
                *out++ = *in1++ ^ *in2++;
            case 9:
                *out++ = *in1++ ^ *in2++;
            case 8:
                *out++ = *in1++ ^ *in2++;
            case 7:
                *out++ = *in1++ ^ *in2++;
            case 6:
                *out++ = *in1++ ^ *in2++;
            case 5:
                *out++ = *in1++ ^ *in2++;
            case 4:
                *out++ = *in1++ ^ *in2++;
            case 3:
                *out++ = *in1++ ^ *in2++;
            case 2:
                *out++ = *in1++ ^ *in2++;
            case 1:
                *out++ = *in1++ ^ *in2++;
            }
    }
}


/* RFC 7963 section 2.1 */
enum { R1 = 32, R2 = 24, R3 = 16, R4 = 63 };

/* RFC 7693 section 2.6 */
static const uint64_t iv[] =
{
    0x6a09e667f3bcc908,                /* floor(2^64 * frac(sqrt(2)))  */
    0xbb67ae8584caa73b,                /* floor(2^64 * frac(sqrt(3)))  */
    0x3c6ef372fe94f82b,                /* floor(2^64 * frac(sqrt(5)))  */
    0xa54ff53a5f1d36f1,                /* floor(2^64 * frac(sqrt(7)))  */
    0x510e527fade682d1,                /* floor(2^64 * frac(sqrt(11))) */
    0x9b05688c2b3e6c1f,                /* floor(2^64 * frac(sqrt(13))) */
    0x1f83d9abfb41bd6b,                /* floor(2^64 * frac(sqrt(17))) */
    0x5be0cd19137e2179,                /* floor(2^64 * frac(sqrt(19))) */
};

/* RFC 7693 section 2.7 */
static const unsigned char sigma[][16] =
{
    { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15},
    {14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3},
    {11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4},
    { 7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8},
    { 9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13},
    { 2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9},
    {12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11},
    {13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10},
    { 6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5},
    {10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0},
    /* This array recycles if you have more than 10 rounds. BLAKE2b
     * has 12, so we repeat the first two rows again. */
    { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15},
    {14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3},
};

static inline uint64_t ror(uint64_t x, unsigned rotation)
{
    unsigned lshift = 63 & -rotation, rshift = 63 & rotation;
    return (x << lshift) | (x >> rshift);
}

static inline void g_half(uint64_t v[16], unsigned a, unsigned b, unsigned c,
                          unsigned d, uint64_t x, unsigned r1, unsigned r2)
{
    v[a] += v[b] + x;
    v[d] ^= v[a];
    v[d] = ror(v[d], r1);
    v[c] += v[d];
    v[b] ^= v[c];
    v[b] = ror(v[b], r2);
}

static inline void g(uint64_t v[16], unsigned a, unsigned b, unsigned c,
                     unsigned d, uint64_t x, uint64_t y)
{
    g_half(v, a, b, c, d, x, R1, R2);
    g_half(v, a, b, c, d, y, R3, R4);
}

static inline void f(uint64_t h[8], uint64_t m[16], uint64_t offset_hi,
                     uint64_t offset_lo, unsigned final)
{
    uint64_t v[16];
    memcpy(v, h, 8 * sizeof(*v));
    memcpy(v + 8, iv, 8 * sizeof(*v));
    v[12] ^= offset_lo;
    v[13] ^= offset_hi;
    v[14] ^= -(uint64_t)final;
    for (unsigned round = 0; round < 12; round++)
    {
        const unsigned char* s = sigma[round];
        g(v,  0,  4,  8, 12, m[s[ 0]], m[s[ 1]]);
        g(v,  1,  5,  9, 13, m[s[ 2]], m[s[ 3]]);
        g(v,  2,  6, 10, 14, m[s[ 4]], m[s[ 5]]);
        g(v,  3,  7, 11, 15, m[s[ 6]], m[s[ 7]]);
        g(v,  0,  5, 10, 15, m[s[ 8]], m[s[ 9]]);
        g(v,  1,  6, 11, 12, m[s[10]], m[s[11]]);
        g(v,  2,  7,  8, 13, m[s[12]], m[s[13]]);
        g(v,  3,  4,  9, 14, m[s[14]], m[s[15]]);
    }
    for (unsigned i = 0; i < 8; i++)
        h[i] ^= v[i] ^ v[i+8];
    smemclr(v, sizeof(v));
}


static inline void f_outer(uint64_t h[8], uint8_t blk[128], uint64_t offset_hi,
                           uint64_t offset_lo, unsigned final)
{
    uint64_t m[16];
    for (unsigned i = 0; i < 16; i++)
        m[i] = GET_64BIT_LSB_FIRST(blk + 8*i);
    f(h, m, offset_hi, offset_lo, final);
    smemclr(m, sizeof(m));
}


typedef struct blake2b
{
    uint64_t h[8];
    unsigned hashlen;

    uint8_t block[128];
    size_t used;
    uint64_t lenhi, lenlo;

    BinarySink_IMPLEMENTATION;
    ssh_hash hash;
} blake2b;


static void blake2b_reset(ssh_hash* hash)
{
    blake2b* s = container_of(hash, blake2b, hash);

    /* Initialise the hash to the standard IV */
    memcpy(s->h, iv, sizeof(s->h));

    /* XOR in the parameters: secret key length (here always 0) in
     * byte 1, and hash length in byte 0. */
    s->h[0] ^= 0x01010000 ^ s->hashlen;

    s->used = 0;
    s->lenhi = s->lenlo = 0;
}


static void blake2b_digest(ssh_hash* hash, uint8_t* digest)
{
    blake2b* s = container_of(hash, blake2b, hash);

    memset(s->block + s->used, 0, sizeof(s->block) - s->used);
    f_outer(s->h, s->block, s->lenhi, s->lenlo, 1);

    uint8_t hash_pre[128];
    for (unsigned i = 0; i < 8; i++)
        PUT_64BIT_LSB_FIRST(hash_pre + 8*i, s->h[i]);
    memcpy(digest, hash_pre, s->hashlen);
    smemclr(hash_pre, sizeof(hash_pre));
}


static void blake2b_free(ssh_hash* hash)
{
    blake2b* s = container_of(hash, blake2b, hash);

    smemclr(s, sizeof(*s));
    sfree(s);
}


static void blake2b_write(BinarySink* bs, const void* vp, size_t len)
{
    blake2b* s = BinarySink_DOWNCAST(bs, blake2b);
    const uint8_t* p = (const uint8_t*)vp;

    while (len > 0)
    {
        if (s->used == sizeof(s->block))
        {
            f_outer(s->h, s->block, s->lenhi, s->lenlo, 0);
            s->used = 0;
        }

        size_t chunk = sizeof(s->block) - s->used;
        if (chunk > len)
            chunk = len;

        memcpy(s->block + s->used, p, chunk);
        s->used += chunk;
        p += chunk;
        len -= chunk;

        s->lenlo += chunk;
        s->lenhi += (s->lenlo < chunk);
    }
}


static inline ssh_hash* ssh_hash_reset(ssh_hash* h)
{
    blake2b_reset(h);
    return h;
}


static ssh_hash* blake2b_new_inner(unsigned hashlen)
{
    assert(hashlen <= 64);

    blake2b* s = snew(struct blake2b);
    //s->hash.vt = &ssh_blake2b;
    s->hashlen = hashlen;
    BinarySink_INIT(s, blake2b_write);
    BinarySink_DELEGATE_INIT(&s->hash, s);
    return &s->hash;
}


ssh_hash* blake2b_new_general(unsigned hashlen)
{
    ssh_hash* h = blake2b_new_inner(hashlen);
    ssh_hash_reset(h);
    return h;
}

/* ----------------------------------------------------------------------
 * Argon2 defines a hash-function family that's an extension of BLAKE2b to
 * generate longer output digests, by repeatedly outputting half of a BLAKE2
 * hash output and then re-hashing the whole thing until there are 64 or fewer
 * bytes left to output. The spec calls this H' (a variant of the original
 * hash it calls H, which is the unmodified BLAKE2b).
 */

static ssh_hash* hprime_new(unsigned length)
{
    ssh_hash* h = blake2b_new_general(length > 64 ? 64 : length);
    put_uint32_le(h, length);
    return h;
}

void BinarySink_put_data(BinarySink* bs, const void* data, size_t len)
{
    bs->write(bs, data, len);
}

#define put_data(bs, val, len) BinarySink_put_data(BinarySink_UPCAST(bs), val, len)

static inline void ssh_hash_final(ssh_hash* h, unsigned char* out)
{
    blake2b_digest(h, out);
    blake2b_free(h);
}

static void hprime_final(ssh_hash* h, unsigned length, void* vout)
{
    uint8_t* out = (uint8_t*)vout;

    while (length > 64)
    {
        uint8_t hashbuf[64];
        ssh_hash_final(h, hashbuf);

        memcpy(out, hashbuf, 32);
        out += 32;
        length -= 32;

        h = blake2b_new_general(length > 64 ? 64 : length);
        put_data(h, hashbuf, 64);

        smemclr(hashbuf, sizeof(hashbuf));
    }

    ssh_hash_final(h, out);
}

/* ----------------------------------------------------------------------
 * Argon2's own mixing function G, which operates on 1Kb blocks of data.
 *
 * The definition of G in the spec takes two 1Kb blocks as input and produces
 * a 1Kb output block. The first thing that happens to the input blocks is
 * that they get XORed together, and then only the XOR output is used, so you
 * could perfectly well regard G as a 1Kb->1Kb function.
 */

static inline uint64_t trunc32(uint64_t x)
{
    return x & 0xFFFFFFFF;
}

/* Internal function similar to the BLAKE2b round, which mixes up four 64-bit
 * words */
static inline void GB(uint64_t* a, uint64_t* b, uint64_t* c, uint64_t* d)
{
    *a += *b + 2 * trunc32(*a) * trunc32(*b);
    *d = ror(*d ^ *a, 32);
    *c += *d + 2 * trunc32(*c) * trunc32(*d);
    *b = ror(*b ^ *c, 24);
    *a += *b + 2 * trunc32(*a) * trunc32(*b);
    *d = ror(*d ^ *a, 16);
    *c += *d + 2 * trunc32(*c) * trunc32(*d);
    *b = ror(*b ^ *c, 63);
}

/* Higher-level internal function which mixes up sixteen 64-bit words. This is
 * applied to different subsets of the 128 words in a kilobyte block, and the
 * API here is designed to make it easy to apply in the circumstances the spec
 * requires. In every call, the sixteen words form eight pairs adjacent in
 * memory, whose addresses are in arithmetic progression. So the 16 input
 * words are in[0], in[1], in[instep], in[instep+1], ..., in[7*instep],
 * in[7*instep+1], and the 16 output words similarly. */
static inline void P(uint64_t* out, unsigned outstep,
                     uint64_t* in, unsigned instep)
{
    for (unsigned i = 0; i < 8; i++)
    {
        out[i*outstep] = in[i*instep];
        out[i*outstep+1] = in[i*instep+1];
    }

    GB(out+0*outstep+0, out+2*outstep+0, out+4*outstep+0, out+6*outstep+0);
    GB(out+0*outstep+1, out+2*outstep+1, out+4*outstep+1, out+6*outstep+1);
    GB(out+1*outstep+0, out+3*outstep+0, out+5*outstep+0, out+7*outstep+0);
    GB(out+1*outstep+1, out+3*outstep+1, out+5*outstep+1, out+7*outstep+1);

    GB(out+0*outstep+0, out+2*outstep+1, out+5*outstep+0, out+7*outstep+1);
    GB(out+0*outstep+1, out+3*outstep+0, out+5*outstep+1, out+6*outstep+0);
    GB(out+1*outstep+0, out+3*outstep+1, out+4*outstep+0, out+6*outstep+1);
    GB(out+1*outstep+1, out+2*outstep+0, out+4*outstep+1, out+7*outstep+0);
}

/* The full G function, taking input blocks X and Y. The result of G is most
 * often XORed into an existing output block, so this API is designed with
 * that in mind: the mixing function's output is always XORed into whatever
 * 1Kb of data is already at 'out'. */
static void G_xor(uint8_t* out, const uint8_t* X, const uint8_t* Y)
{
    uint64_t R[128], Q[128], Z[128];

    for (unsigned i = 0; i < 128; i++)
        R[i] = GET_64BIT_LSB_FIRST(X + 8*i) ^ GET_64BIT_LSB_FIRST(Y + 8*i);

    for (unsigned i = 0; i < 8; i++)
        P(Q+16*i, 2, R+16*i, 2);

    for (unsigned i = 0; i < 8; i++)
        P(Z+2*i, 16, Q+2*i, 16);

    for (unsigned i = 0; i < 128; i++)
        PUT_64BIT_LSB_FIRST(out + 8*i,
                            GET_64BIT_LSB_FIRST(out + 8*i) ^ R[i] ^ Z[i]);

    smemclr(R, sizeof(R));
    smemclr(Q, sizeof(Q));
    smemclr(Z, sizeof(Z));
}

/* ----------------------------------------------------------------------
 * The main Argon2 function.
 */

static void argon2_internal(uint32_t p, uint32_t T, uint32_t m, uint32_t t,
                            uint32_t y, ptrlen P, ptrlen S, ptrlen K, ptrlen X,
                            uint8_t* out)
{
    /*
     * Start by hashing all the input data together: the four string arguments
     * (password P, salt S, optional secret key K, optional associated data
     * X), plus all the parameters for the function's memory and time usage.
     *
     * The output of this hash is the sole input to the subsequent mixing
     * step: Argon2 does not preserve any more entropy from the inputs, it
     * just makes it extra painful to get the final answer.
     */
    uint8_t h0[64];
    {
        ssh_hash* h = blake2b_new_general(64);
        put_uint32_le(h, p);
        put_uint32_le(h, T);
        put_uint32_le(h, m);
        put_uint32_le(h, t);
        put_uint32_le(h, 0x13);        /* hash function version number */
        put_uint32_le(h, y);
        put_stringpl_le(h, P);
        put_stringpl_le(h, S);
        put_stringpl_le(h, K);
        put_stringpl_le(h, X);
        ssh_hash_final(h, h0);
    }

    struct blk { uint8_t data[1024]; };

    /*
     * Array of 1Kb blocks. The total size is (approximately) m, the
     * caller-specified parameter for how much memory to use; the blocks are
     * regarded as a rectangular array of p rows ('lanes') by q columns, where
     * p is the 'parallelism' input parameter (the lanes can be processed
     * concurrently up to a point) and q is whatever makes the product pq come
     * to m.
     *
     * Additionally, each row is divided into four equal 'segments', which are
     * important to the way the algorithm decides which blocks to use as input
     * to each step of the function.
     *
     * The term 'slice' refers to a whole set of vertically aligned segments,
     * i.e. slice 0 is the whole left quarter of the array, and slice 3 the
     * whole right quarter.
     */
    size_t SL = m / (4*p); /* segment length: # of 1Kb blocks in a segment */
    size_t q = 4 * SL;     /* width of the array: 4 segments times SL */
    size_t mprime = q * p; /* total size of the array, approximately m */

    /* Allocate the memory. */
    struct blk* B = snewn(mprime, struct blk);
    memset(B, 0, mprime * sizeof(struct blk));

    /*
     * Initial setup: fill the first two full columns of the array with data
     * expanded from the starting hash h0. Each block is the result of using
     * the long-output hash function H' to hash h0 itself plus the block's
     * coordinates in the array.
     */
    for (size_t i = 0; i < p; i++)
    {
        ssh_hash* h = hprime_new(1024);
        put_data(h, h0, 64);
        put_uint32_le(h, 0);
        put_uint32_le(h, i);
        hprime_final(h, 1024, B[i].data);
    }
    for (size_t i = 0; i < p; i++)
    {
        ssh_hash* h = hprime_new(1024);
        put_data(h, h0, 64);
        put_uint32_le(h, 1);
        put_uint32_le(h, i);
        hprime_final(h, 1024, B[i+p].data);
    }

    /*
     * Declarations for the main loop.
     *
     * The basic structure of the main loop is going to involve processing the
     * array one whole slice (vertically divided quarter) at a time. Usually
     * we'll write a new value into every single block in the slice, except
     * that in the initial slice on the first pass, we've already written
     * values into the first two columns during the initial setup above. So
     * 'jstart' indicates the starting index in each segment we process; it
     * starts off as 2 so that we don't overwrite the initial setup, and then
     * after the first slice is done, we set it to 0, and it stays there.
     *
     * d_mode indicates whether we're being data-dependent (true) or
     * data-independent (false). In the hybrid Argon2id mode, we start off
     * independent, and then once we've mixed things up enough, switch over to
     * dependent mode to force long serial chains of computation.
     */
    size_t jstart = 2;
    bool d_mode = (y == 0);
    struct blk out2i, tmp2i, in2i;

    /* Outermost loop: t whole passes from left to right over the array */
    for (size_t pass = 0; pass < t; pass++)
    {

        /* Within that, we process the array in its four main slices */
        for (unsigned slice = 0; slice < 4; slice++)
        {

            /* In Argon2id mode, if we're half way through the first pass,
             * this is the moment to switch d_mode from false to true */
            if (pass == 0 && slice == 2 && y == 2)
                d_mode = true;

            /* Loop over every segment in the slice (i.e. every row). So i is
             * the y-coordinate of each block we process. */
            for (size_t i = 0; i < p; i++)
            {

                /* And within that segment, process the blocks from left to
                 * right, starting at 'jstart' (usually 0, but 2 in the first
                 * slice). */
                for (size_t jpre = jstart; jpre < SL; jpre++)
                {

                    /* j is the x-coordinate of each block we process, made up
                     * of the slice number and the index 'jpre' within the
                     * segment. */
                    size_t j = slice * SL + jpre;

                    /* jm1 is j-1 (mod q) */
                    uint32_t jm1 = (j == 0 ? q-1 : j-1);

                    /*
                     * Construct two 32-bit pseudorandom integers J1 and J2.
                     * This is the part of the algorithm that varies between
                     * the data-dependent and independent modes.
                     */
                    uint32_t J1, J2;
                    if (d_mode)
                    {
                        /*
                         * Data-dependent: grab the first 64 bits of the block
                         * to the left of this one.
                         */
                        J1 = GET_32BIT_LSB_FIRST(B[i + p * jm1].data);
                        J2 = GET_32BIT_LSB_FIRST(B[i + p * jm1].data + 4);
                    }
                    else
                    {
                        /*
                         * Data-independent: generate pseudorandom data by
                         * hashing a sequence of preimage blocks that include
                         * all our input parameters, plus the coordinates of
                         * this point in the algorithm (array position and
                         * pass number) to make all the hash outputs distinct.
                         *
                         * The hash we use is G itself, applied twice. So we
                         * generate 1Kb of data at a time, which is enough for
                         * 128 (J1,J2) pairs. Hence we only need to do the
                         * hashing if our index within the segment is a
                         * multiple of 128, or if we're at the very start of
                         * the algorithm (in which case we started at 2 rather
                         * than 0). After that we can just keep picking data
                         * out of our most recent hash output.
                         */
                        if (jpre == jstart || jpre % 128 == 0)
                        {
                            /*
                             * Hash preimage is mostly zeroes, with a
                             * collection of assorted integer values we had
                             * anyway.
                             */
                            memset(in2i.data, 0, sizeof(in2i.data));
                            PUT_64BIT_LSB_FIRST(in2i.data +  0, pass);
                            PUT_64BIT_LSB_FIRST(in2i.data +  8, i);
                            PUT_64BIT_LSB_FIRST(in2i.data + 16, slice);
                            PUT_64BIT_LSB_FIRST(in2i.data + 24, mprime);
                            PUT_64BIT_LSB_FIRST(in2i.data + 32, t);
                            PUT_64BIT_LSB_FIRST(in2i.data + 40, y);
                            PUT_64BIT_LSB_FIRST(in2i.data + 48, jpre / 128 + 1);

                            /*
                             * Now apply G twice to generate the hash output
                             * in out2i.
                             */
                            memset(tmp2i.data, 0, sizeof(tmp2i.data));
                            G_xor(tmp2i.data, tmp2i.data, in2i.data);
                            memset(out2i.data, 0, sizeof(out2i.data));
                            G_xor(out2i.data, out2i.data, tmp2i.data);
                        }

                        /*
                         * Extract J1 and J2 from the most recent hash output
                         * (whether we've just computed it or not).
                         */
                        J1 = GET_32BIT_LSB_FIRST(
                                 out2i.data + 8 * (jpre % 128));
                        J2 = GET_32BIT_LSB_FIRST(
                                 out2i.data + 8 * (jpre % 128) + 4);
                    }

                    /*
                     * Now convert J1 and J2 into the index of an existing
                     * block of the array to use as input to this step. This
                     * is fairly fiddly.
                     *
                     * The easy part: the y-coordinate of the input block is
                     * obtained by reducing J2 mod p, except that at the very
                     * start of the algorithm (processing the first slice on
                     * the first pass) we simply use the same y-coordinate as
                     * our output block.
                     *
                     * Note that it's safe to use the ordinary % operator
                     * here, without any concern for timing side channels: in
                     * data-independent mode J2 is not correlated to any
                     * secrets, and in data-dependent mode we're going to be
                     * giving away side-channel data _anyway_ when we use it
                     * as an array index (and by assumption we don't care,
                     * because it's already massively randomised from the real
                     * inputs).
                     */
                    uint32_t index_l = (pass == 0 && slice == 0) ? i : J2 % p;

                    /*
                     * The hard part: which block in this array row do we use?
                     *
                     * First, we decide what the possible candidates are. This
                     * requires some case analysis, and depends on whether the
                     * array row is the same one we're writing into or not.
                     *
                     * If it's not the same row: we can't use any block from
                     * the current slice (because the segments within a slice
                     * have to be processable in parallel, so in a concurrent
                     * implementation those blocks are potentially in the
                     * process of being overwritten by other threads). But the
                     * other three slices are fair game, except that in the
                     * first pass, slices to the right of us won't have had
                     * any values written into them yet at all.
                     *
                     * If it is the same row, we _are_ allowed to use blocks
                     * from the current slice, but only the ones before our
                     * current position.
                     *
                     * In both cases, we also exclude the individual _column_
                     * just to the left of the current one. (The block
                     * immediately to our left is going to be the _other_
                     * input to G, but the spec also says that we avoid that
                     * column even in a different row.)
                     *
                     * All of this means that we end up choosing from a
                     * cyclically contiguous interval of blocks within this
                     * lane, but the start and end points require some thought
                     * to get them right.
                     */

                    /* Start position is the beginning of the _next_ slice
                     * (containing data from the previous pass), unless we're
                     * on pass 0, where the start position has to be 0. */
                    uint32_t Wstart = (pass == 0 ? 0 : (slice + 1) % 4 * SL);

                    /* End position splits up by cases. */
                    uint32_t Wend;
                    if (index_l == i)
                    {
                        /* Same lane as output: we can use anything up to (but
                         * not including) the block immediately left of us. */
                        Wend = jm1;
                    }
                    else
                    {
                        /* Different lane from output: we can use anything up
                         * to the previous slice boundary, or one less than
                         * that if we're at the very left edge of our slice
                         * right now. */
                        Wend = SL * slice;
                        if (jpre == 0)
                            Wend = (Wend + q-1) % q;
                    }

                    /* Total number of blocks available to choose from */
                    uint32_t Wsize = (Wend + q - Wstart) % q;

                    /* Fiddly computation from the spec that chooses from the
                     * available blocks, in a deliberately non-uniform
                     * fashion, using J1 as pseudorandom input data. Output is
                     * zz which is the index within our contiguous interval. */
                    uint32_t x = ((uint64_t)J1 * J1) >> 32;
                    uint32_t y2 = ((uint64_t)Wsize * x) >> 32;
                    uint32_t zz = Wsize - 1 - y2;

                    /* And index_z is the actual x coordinate of the block we
                     * want. */
                    uint32_t index_z = (Wstart + zz) % q;

                    /* Phew! Combine that block with the one immediately to
                     * our left, and XOR over the top of whatever is already
                     * in our current output block. */
                    G_xor(B[i + p * j].data, B[i + p * jm1].data,
                          B[index_l + p * index_z].data);
                }
            }

            /* We've finished processing a slice. Reset jstart to 0. It will
             * onily _not_ have been 0 if this was pass 0 slice 0, in which
             * case it still had its initial value of 2 to avoid the starting
             * data. */
            jstart = 0;
        }
    }

    /*
     * The main output is all done. Final output works by taking the XOR of
     * all the blocks in the rightmost column of the array, and then using
     * that as input to our long hash H'. The output of _that_ is what we
     * deliver to the caller.
     */

    struct blk C = B[p * (q-1)];
    for (size_t i = 1; i < p; i++)
        memxor(C.data, C.data, B[i + p * (q-1)].data, 1024);

    {
        ssh_hash* h = hprime_new(T);
        put_data(h, C.data, 1024);
        hprime_final(h, T, out);
    }

    /*
     * Clean up.
     */
    smemclr(out2i.data, sizeof(out2i.data));
    smemclr(tmp2i.data, sizeof(tmp2i.data));
    smemclr(in2i.data, sizeof(in2i.data));
    smemclr(C.data, sizeof(C.data));
    smemclr(B, mprime * sizeof(struct blk));
    sfree(B);
}


std::string zen::zargon2(zen::Argon2Flavor flavour, uint32_t mem, uint32_t passes, uint32_t parallel, uint32_t taglen,
                         const std::string_view password, const std::string_view salt)
{
    std::string output(taglen, '\0');
    argon2_internal(parallel, taglen, mem, passes, static_cast<uint32_t>(flavour),
    {.ptr = password.data(), .len = password.size()},
    {.ptr = salt    .data(), .len = salt    .size()},
    {.ptr = "", .len = 0},
    {.ptr = "", .len = 0}, reinterpret_cast<uint8_t*>(output.data()));
    return output;
}
