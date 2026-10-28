// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************
#pragma once

#include <cassert>
#include <cmath>
#include <numbers>
#include "type_traits.h"

namespace numeric
{
template <class T> auto dist(T a, T b);
template <class T> int sign(T value); //returns one of {-1, 0, 1}
template <class T> bool isNull(T value); //...definitively fishy...

template <class T, class InputIterator> //precondition: range must be sorted!
auto roundToGrid(T val, InputIterator first, InputIterator last);

template <class N, class D> auto intDivRound(N numerator, D denominator);
template <class N, class D> auto intDivCeil (N numerator, D denominator);
template <class N, class D> auto intDivFloor(N numerator, D denominator);

template <size_t N, class T>
constexpr T power(T value);

double radToDeg(double rad);    //convert unit [rad] into [°]
double degToRad(double degree); //convert unit [°] into [rad]

template <class InputIterator>
double arithmeticMean(InputIterator first, InputIterator last);

template <class RandomAccessIterator>
double median(RandomAccessIterator first, RandomAccessIterator last); //note: invalidates input range!

template <class InputIterator>
double stdDeviation(InputIterator first, InputIterator last, double* mean = nullptr); //estimate standard deviation (and thereby arithmetic mean)

//median absolute deviation: "mad / 0.6745" is a robust measure for standard deviation of a normal distribution
template <class RandomAccessIterator>
double mad(RandomAccessIterator first, RandomAccessIterator last); //note: invalidates input range!

template <class InputIterator>
double norm2(InputIterator first, InputIterator last);

//----------------------------------------------------------------------------------













//################# inline implementation #########################
template <class T> inline
auto dist(T a, T b) //return type might be different than T, e.g. std::chrono::duration instead of std::chrono::time_point
{
    return a > b ? a - b : b - a;
}


template <class T> inline
int sign(T value) //returns one of {-1, 0, 1}
{
    static_assert(std::is_signed_v<T>);
    return value < 0 ? -1 : (value > 0 ? 1 : 0);
}


#if 0 //part of C++11 now!
//by factor 1.5 to 3 faster than boost::minmax_element (=two-step algorithm) for built-in types!
template <class InputIterator, class Compare> inline
std::pair<InputIterator, InputIterator> minMaxElement(InputIterator first, InputIterator last, Compare compLess)
{
    InputIterator itMin = first;
    InputIterator itMax = first;

    if (first != last)
    {
        auto minVal = *itMin; //nice speedup on 64 bit!
        auto maxVal = *itMax; //
        for (;;)
        {
            ++first;
            if (first == last)
                break;
            const auto val = *first;

            if (compLess(maxVal, val))
            {
                itMax = first;
                maxVal  = val;
            }
            else if (compLess(val, minVal))
            {
                itMin = first;
                minVal = val;
            }
        }
    }
    return {itMin, itMax};
}


template <class InputIterator> inline
std::pair<InputIterator, InputIterator> minMaxElement(InputIterator first, InputIterator last)
{
    return minMaxElement(first, last, std::less());
}
#endif


template <class T, class BiDiIterator> inline
auto roundToGrid(T val, BiDiIterator first, BiDiIterator last)
{
    assert(std::is_sorted(first, last));
    if (first == last)
        return static_cast<std::iter_value_t<BiDiIterator>>(val);

    BiDiIterator it = std::lower_bound(first, last, val);
    if (it == last)
        return *--last;
    if (it == first)
        return *first;

    const auto nextVal = *it;
    const auto prevVal = *--it;
    return val - prevVal < nextVal - val ? prevVal : nextVal;
}


template <class T> inline
bool isNull(T value)
{
    static_assert(zen::isFloat<T>);
    return std::abs(value) < std::numeric_limits<T>::epsilon();
}


template <class N, class D> inline
auto intDivRound(N num, D den)
{
    using namespace zen;
    static_assert(isInteger<N>&& isInteger<D>);
    static_assert(isSignedInt<N> == isSignedInt<D>); //until further
    assert(den != 0);
#if 1 //avoid overflow/underflow + bonus: no UB due to std::abs(INT_MIN) because |r| < |den| <= |INT_MIN|
    const auto q = num / den; //single idiv/div
    const auto r = num % den; //

    if constexpr (isSignedInt<N>)
    {
        auto r2 = std::abs(r);
        if (den < 0)
            r2 += den;

        const decltype(q) shift = r2 >= den - r2;

        return q + ((num < 0) == (den < 0) ? shift : -shift);
    }
    else
        return q + (r >= den - r);
#else
    const auto half = den / 2;

    if constexpr (isSignedInt<N>)
        return (num + ((num < 0) == (den < 0) ? half : -half)) / den;
    else
        return (num + half) / den;
#endif
}


template <class N, class D> inline
auto intDivCeil(N num, D den)
{
    using namespace zen;
    static_assert(isInteger<N>&& isInteger<D>);
    static_assert(isSignedInt<N> == isSignedInt<D>); //until further
    assert(den != 0);
#if 1 //avoids overflow/underflow
    const auto q = num / den; //single idiv/div
    const auto r = num % den; //

    if constexpr (isSignedInt<N>)
        return q + (r != 0 && (num < 0) == (den < 0));
    else
        return q + (r != 0);
#else
    if constexpr (isSignedInt<N>)
    {
        if ((num < 0) != (den < 0))
            return num / den;

        return (num + den + (num < 0 ? 1 : - 1)) / den;
    }
    else
        return (num + den - 1) / den;
#endif
}


template <class N, class D> inline
auto intDivFloor(N num, D den)
{
    using namespace zen;
    static_assert(isInteger<N>&& isInteger<D>);
    static_assert(isSignedInt<N> == isSignedInt<D>); //until further
    assert(den != 0);
    if constexpr (isSignedInt<N>)
    {
#if 1 //avoid overflow/underflow
        const auto q = num / den; //single idiv
        const auto r = num % den; //

        return q - (r != 0 && (num < 0) != (den < 0));
#else
        if ((num < 0) != (den < 0))
            return (num - den + (num < 0 ? 1 : - 1)) / den;
        else
            return num / den;
#endif
    }
    else
        return num / den;
}


template <size_t N, class T> inline
constexpr T power(T value)
{
    if constexpr (N == 2)
        return value * value;
    else if constexpr (N == 3)
        return value * value * value;
    else //need more!?
        static_assert(false);
}


inline
double radToDeg(double rad)
{
    return rad * (180.0 / std::numbers::pi);
}


inline
double degToRad(double degree)
{
    return degree / (180.0 / std::numbers::pi);
}


template <class InputIterator> inline
double arithmeticMean(InputIterator first, InputIterator last)
{
    size_t n      = 0; //avoid random-access requirement for iterator!
    double sum_xi = 0;

    for (; first != last; ++first, ++n)
        sum_xi += *first;

    return n == 0 ? 0 : sum_xi / n;
}


template <class RandomAccessIterator> inline
double median(RandomAccessIterator first, RandomAccessIterator last) //note: invalidates input range!
{
    const size_t n = last - first;
    if (n == 0)
        return 0;

    std::nth_element(first, first + n / 2, last); //complexity: O(n)
    const double midVal = *(first + n / 2);

    if (n % 2 != 0)
        return midVal;
    else //n is even and >= 2 in this context: return mean of two middle values
        return 0.5 * (*std::max_element(first, first + n / 2) + midVal); //this operation is the reason why median() CANNOT support a comparison predicate!!!
}


template <class RandomAccessIterator> inline
double mad(RandomAccessIterator first, RandomAccessIterator last) //note: invalidates input range!
{
    //https://en.wikipedia.org/wiki/Median_absolute_deviation
    const size_t n = last - first;
    if (n == 0)
        return 0;

    const double m = median(first, last);

    //the second median needs to operate on absolute residuals => avoid transforming input range which may have less than double precision!
    auto lessMedAbs = [m](double lhs, double rhs) { return std::abs(lhs - m) < std::abs(rhs - m); };

    std::nth_element(first, first + n / 2, last, lessMedAbs); //complexity: O(n)
    const double midVal = std::abs(*(first + n / 2) - m);

    if (n % 2 != 0)
        return midVal;
    else //n is even and >= 2 in this context: return mean of two middle values
        return 0.5 * (std::abs(*std::max_element(first, first + n / 2, lessMedAbs) - m) + midVal);
}


template <class InputIterator> inline
double stdDeviation(InputIterator first, InputIterator last, double* arithMean)
{
    //implementation minimizing rounding errors, see: https://en.wikipedia.org/wiki/Standard_deviation
    //combined with technique avoiding overflow, see: https://www.netlib.org/blas/dnrm2.f -> only 10% performance degradation

    size_t n     = 0;
    double mean  = 0;
    double q     = 0;
    double scale = 1;

    for (; first != last; ++first)
    {
        ++n;
        const double val = *first - mean;

        if (std::abs(val) > scale)
        {
            q = (n - 1.0) / n + q * power<2>(scale / val);
            scale = std::abs(val);
        }
        else
            q += (n - 1.0) * power<2>(val / scale) / n;

        mean += val / n;
    }

    if (arithMean)
        *arithMean = mean;

    return n <= 1 ? 0 : std::sqrt(q / (n - 1)) * scale;
}


template <class InputIterator> inline
double norm2(InputIterator first, InputIterator last)
{
    double result = 0;
    double scale  = 1;
    for (; first != last; ++first)
    {
        const double tmp = std::abs(*first);
        if (tmp > scale)
        {
            result = 1 + result * power<2>(scale / tmp);
            scale = tmp;
        }
        else
            result += power<2>(tmp / scale);
    }
    return std::sqrt(result) * scale;
}
}
