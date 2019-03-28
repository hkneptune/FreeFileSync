// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef GUID_H_80425780237502345
#define GUID_H_80425780237502345

    #include <fcntl.h> //open
    #include <unistd.h> //close
    #include <zen/sys_error.h>
    //#include <uuid/uuid.h> -> uuid_generate(), uuid_unparse(); avoid additional dependency for "sudo apt-get install uuid-dev"


namespace zen
{
inline
std::string generateGUID() //creates a 16-byte GUID
{
    std::string guid(16, '\0');
#if __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 25) //getentropy() requires glibc 2.25 (ldd --version) PS: CentOS 7 is on 2.17
    if (::getentropy(&guid[0], guid.size()) != 0)  //"The maximum permitted value for the length argument is 256"
        throw std::runtime_error(std::string(__FILE__) + "[" + numberTo<std::string>(__LINE__) + "] Failed to generate GUID." +
                                 "\n" + utfTo<std::string>(formatSystemError(L"getentropy", errno)));
#else
    class RandomGeneratorPosix
    {
    public:
        RandomGeneratorPosix()
        {
            if (fd_ == -1)
                throw std::runtime_error(std::string(__FILE__) + "[" + numberTo<std::string>(__LINE__) + "] Failed to generate GUID." +
                                         "\n" + utfTo<std::string>(formatSystemError(L"open", errno)));
        }

        ~RandomGeneratorPosix() { ::close(fd_); }

        void getBytes(void* buf, size_t size)
        {
            for (size_t offset = 0; offset < size; )
            {
                const ssize_t bytesRead = ::read(fd_, static_cast<char*>(buf) + offset, size - offset);
                if (bytesRead < 1) //0 means EOF => error in this context (should check for buffer overflow, too?)
                    throw std::runtime_error(std::string(__FILE__) + "[" + numberTo<std::string>(__LINE__) + "] Failed to generate GUID." +
                                             "\n" + utfTo<std::string>(formatSystemError(L"read", bytesRead < 0 ? errno : EIO)));
                offset += bytesRead;
                assert(offset <= size);
            }
        }

    private:
        const int fd_ = ::open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    };
    thread_local RandomGeneratorPosix gen;
    gen.getBytes(&guid[0], guid.size());
#endif
    return guid;

}
}

#endif //GUID_H_80425780237502345
