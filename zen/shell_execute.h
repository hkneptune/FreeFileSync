// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SHELL_EXECUTE_H_23482134578134134
#define SHELL_EXECUTE_H_23482134578134134

#include "file_error.h"

    #include <unistd.h> //fork()
    #include <stdlib.h> //::system()


namespace zen
{
//launch commandline and report errors via popup dialog
//Windows: COM needs to be initialized before calling this function!
enum class ExecutionType
{
    SYNC,
    ASYNC
};

namespace
{


void shellExecute(const Zstring& command, ExecutionType type, bool hideConsole) //throw FileError
{
    /*
    we cannot use wxExecute due to various issues:
    - screws up encoding on OS X for non-ASCII characters
    - does not provide any reasonable error information
    - uses a zero-sized dummy window as a hack to keep focus which leaves a useless empty icon in ALT-TAB list in Windows
    */
    if (type == ExecutionType::SYNC)
    {
        //Posix ::system() - execute a shell command
        const int rv = ::system(command.c_str()); //do NOT use std::system as its documentation says nothing about "WEXITSTATUS(rv)", etc...
        if (rv == -1 || WEXITSTATUS(rv) == 127)
            throw FileError(_("Incorrect command line:") + L"\n" + utfTo<std::wstring>(command));
        //http://linux.die.net/man/3/system "In case /bin/sh could not be executed, the exit status will be that of a command that does exit(127)"
        //Bonus: For an incorrect command line /bin/sh also returns with 127!
    }
    else
    {
        //follow implemenation of ::system() except for waitpid():
        const pid_t pid = ::fork();
        if (pid < 0) //pids are never negative, empiric proof: https://linux.die.net/man/2/wait
            THROW_LAST_FILE_ERROR(_("Incorrect command line:") + L"\n" + utfTo<std::wstring>(command), L"fork");

        if (pid == 0) //child process
        {
            const char* argv[] = { "sh", "-c", command.c_str(), nullptr };
            /*int rv =*/::execv("/bin/sh", const_cast<char**>(argv));
            //safe to cast away const: http://pubs.opengroup.org/onlinepubs/9699919799/functions/exec.html
            //  "The statement about argv[] and envp[] being constants is included to make explicit to future
            //   writers of language bindings that these objects are completely constant. Due to a limitation of
            //   the ISO C standard, it is not possible to state that idea in standard C."

            //"execv() only returns if an error has occurred. The return value is -1, and errno is set to indicate the error."
            ::_exit(127); //[!] avoid flushing I/O buffers or doing other clean up from child process like with "exit(127)"!
        }
        //else //parent process
    }
}
}


inline
void openWithDefaultApplication(const Zstring& itemPath) //throw FileError
{
    shellExecute("xdg-open \"" + itemPath +  '"', ExecutionType::ASYNC, false/*hideConsole*/); //
}
}

#endif //SHELL_EXECUTE_H_23482134578134134
