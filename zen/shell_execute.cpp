// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "shell_execute.h"
#include <chrono>
#include "guid.h"
#include "file_access.h"
#include "file_io.h"

    #include <unistd.h> //fork, pipe
    #include <sys/wait.h> //waitpid
    #include <fcntl.h>

using namespace zen;


std::vector<Zstring> zen::parseCommandline(const Zstring& cmdLine)
{
    std::vector<Zstring> args;
    //"Parsing C++ Command-Line Arguments": https://docs.microsoft.com/en-us/cpp/cpp/parsing-cpp-command-line-arguments
    //we do the job ourselves! both wxWidgets and ::CommandLineToArgvW() parse "C:\" "D:\" as single line C:\" D:\"
    //-> "solution": we just don't support protected quotation mark!

    auto itStart = cmdLine.end(); //end() means: no token
    for (auto it = cmdLine.begin(); it != cmdLine.end(); ++it)
        if (*it == Zstr(' ')) //space commits token
        {
            if (itStart != cmdLine.end())
            {
                args.emplace_back(itStart, it);
                itStart = cmdLine.end(); //expect consecutive blanks!
            }
        }
        else
        {
            //start new token
            if (itStart == cmdLine.end())
                itStart = it;

            if (*it == Zstr('"'))
            {
                it = std::find(it + 1, cmdLine.end(), Zstr('"'));
                if (it == cmdLine.end())
                    break;
            }
        }
    if (itStart != cmdLine.end())
        args.emplace_back(itStart, cmdLine.end());

    for (Zstring& str : args)
        if (str.size() >= 2 && startsWith(str, Zstr('"')) && endsWith(str, Zstr('"')))
            str = Zstring(str.c_str() + 1, str.size() - 2);

    return args;
}




std::pair<int /*exit code*/, std::wstring> zen::consoleExecute(const Zstring& cmdLine, std::optional<int> timeoutMs) //throw SysError, SysErrorTimeOut
{
    const Zstring tempFilePath = appendSeparator(getTempFolderPath()) + //throw FileError
                                 Zstr("FFS-") + utfTo<Zstring>(formatAsHexString(generateGUID()));
    /*  can't use popen(): does NOT return the exit code on Linux (despite the documentation!), although it works correctly on macOS
          => use pipes instead: https://linux.die.net/man/2/waitpid
             bonus: no need for "2>&1" to redirect STDERR to STDOUT

             What about premature exit via SysErrorTimeOut?
                Linux: child process' end of the pipe *still works* even after the parent process is gone:
                        There does not seem to be any output buffer size limit + no observable strain on system memory or disk space! :)
                macOS: child process exits if parent end of pipe is closed: fuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu..........

       => solution: buffer output in temporary file

       Unresolved problem: premature exit via SysErrorTimeOut (=> no waitpid()) creates zombie proceses:
                "As long as a zombie is not removed from the system via a wait,
                 it will consume a slot in the kernel process table, and if this table fills,
                 it will not be possible to create further processes."                 */

    const int EC_CHILD_LAUNCH_FAILED = 120; //avoid 127: used by the system, e.g. failure to execute due to missing .so file

    //use O_TMPFILE? sounds nice, but support is probably crap: https://github.com/libvips/libvips/issues/1151
    const int fdTempFile = ::open(tempFilePath.c_str(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC,
                                  S_IRUSR | S_IWUSR); //0600
    if (fdTempFile == -1)
        THROW_LAST_SYS_ERROR("open");
    auto guardTmpFile = makeGuard<ScopeGuardRunMode::onExit>([&] { ::close(fdTempFile); });

    //"deleting while handles are open" == FILE_FLAG_DELETE_ON_CLOSE
    if (::unlink(tempFilePath.c_str()) != 0)
        THROW_LAST_SYS_ERROR("unlink");

    //--------------------------------------------------------------
    //waitpid() is a useless pile of garbage without time out => check EOF from dummy pipe instead
    int pipe[2] = {};
    if (::pipe2(pipe, O_CLOEXEC) != 0)
        THROW_LAST_SYS_ERROR("pipe2");


    const int fdLifeSignR = pipe[0]; //for parent process
    const int fdLifeSignW = pipe[1]; //for child process
    ZEN_ON_SCOPE_EXIT(::close(fdLifeSignR));
    auto guardFdLifeSignW = makeGuard<ScopeGuardRunMode::onExit>([&] { ::close(fdLifeSignW ); });
    //--------------------------------------------------------------

    //follow implemenation of ::system(): https://github.com/lattera/glibc/blob/master/sysdeps/posix/system.c
    const pid_t pid = ::fork();
    if (pid < 0) //pids are never negative, empiric proof: https://linux.die.net/man/2/wait
        THROW_LAST_SYS_ERROR("fork");

    if (pid == 0) //child process
        try
        {
            //first task: set STDOUT redirection in case an error needs to be reported
            if (::dup2(fdTempFile, STDOUT_FILENO) != STDOUT_FILENO) //O_CLOEXEC does NOT propagate with dup2()
                THROW_LAST_SYS_ERROR("dup2(STDOUT)");

            if (::dup2(fdTempFile, STDERR_FILENO) != STDERR_FILENO) //O_CLOEXEC does NOT propagate with dup2()
                THROW_LAST_SYS_ERROR("dup2(STDERR)");

            //avoid blocking scripts waiting for user input
            // => appending " < /dev/null" is not good enough! e.g. hangs for: read -p "still hanging here"; echo fuuuuu...
            const int fdDevNull = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
            if (fdDevNull == -1) //don't check "< 0" -> docu seems to allow "-2" to be a valid file handle
                THROW_LAST_SYS_ERROR("open(/dev/null)");
            ZEN_ON_SCOPE_EXIT(::close(fdDevNull));

            if (::dup2(fdDevNull, STDIN_FILENO) != STDIN_FILENO) //O_CLOEXEC does NOT propagate with dup2()
                THROW_LAST_SYS_ERROR("dup2(STDIN)");

            //*leak* the fd and have it closed automatically on child process exit after execv()
            if (::dup(fdLifeSignW) == -1) //O_CLOEXEC does NOT propagate with dup()
                THROW_LAST_SYS_ERROR("dup(fdLifeSignW)");


            const char* argv[] = { "sh", "-c", cmdLine.c_str(), nullptr };
            /*int rv =*/::execv("/bin/sh", const_cast<char**>(argv)); //only returns if an error occurred
            //safe to cast away const: https://pubs.opengroup.org/onlinepubs/9699919799/functions/exec.html
            //  "The statement about argv[] and envp[] being constants is included to make explicit to future
            //   writers of language bindings that these objects are completely constant. Due to a limitation of
            //   the ISO C standard, it is not possible to state that idea in standard C."
            THROW_LAST_SYS_ERROR("execv");
        }
        catch (const SysError& e)
        {
            ::puts(utfTo<std::string>(e.toString()).c_str());
            ::fflush(stdout); //note: stderr is unbuffered by default
            ::_exit(EC_CHILD_LAUNCH_FAILED); //[!] avoid flushing I/O buffers or doing other clean up from child process like with "exit()"!
        }
    //else: parent process


    if (timeoutMs)
    {
        guardFdLifeSignW.dismiss();
        ::close(fdLifeSignW); //[!] make sure we get EOF when fd is closed by child!

        if (::fcntl(fdLifeSignR, F_SETFL, O_NONBLOCK) != 0)
            THROW_LAST_SYS_ERROR("fcntl(O_NONBLOCK)");

        const auto endTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(*timeoutMs);
        for (;;) //EINTR handling? => allow interrupt!?
        {
            //read until EAGAIN
            char buf[16];
            const ssize_t bytesRead = ::read(fdLifeSignR, buf, sizeof(buf));
            if (bytesRead < 0)
            {
                if (errno != EAGAIN)
                    THROW_LAST_SYS_ERROR("read");
            }
            else if (bytesRead > 0)
                throw SysError(formatSystemError("read", L"", L"Unexpected data."));
            else //bytesRead == 0: EOF
                break;

            //wait for stream input
            const auto now = std::chrono::steady_clock::now();
            if (now > endTime)
                throw SysErrorTimeOut(_P("Operation timed out after 1 second.", "Operation timed out after %x seconds.", *timeoutMs / 1000));

            const auto waitTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - now).count();

            struct ::timeval tv = {};
            tv.tv_sec  = static_cast<long>(waitTimeMs / 1000);
            tv.tv_usec = static_cast<long>(waitTimeMs - tv.tv_sec * 1000) * 1000;

            fd_set rfd = {}; //includes FD_ZERO
            FD_SET(fdLifeSignR, &rfd);

            if (const int rv = ::select(fdLifeSignR + 1, //int nfds,
                                        &rfd,            //fd_set* readfds,
                                        nullptr,         //fd_set* writefds,
                                        nullptr,         //fd_set* exceptfds,
                                        &tv);            //struct timeval* timeout
                rv < 0)
                THROW_LAST_SYS_ERROR("select");
            else if (rv == 0)
                throw SysErrorTimeOut(_P("Operation timed out after 1 second.", "Operation timed out after %x seconds.", *timeoutMs / 1000));
        }
    }

    //https://linux.die.net/man/2/waitpid
    int statusCode = 0;
    if (::waitpid(pid,         //pid_t pid
                  &statusCode, //int* status
                  0) != pid)   //int options
        THROW_LAST_SYS_ERROR("waitpid");


    if (::lseek(fdTempFile, 0, SEEK_SET) != 0)
        THROW_LAST_SYS_ERROR("lseek");

    guardTmpFile.dismiss();
    FileInput streamIn(fdTempFile, tempFilePath, nullptr /*notifyUnbufferedIO*/); //takes ownership!
    const std::wstring output = utfTo<std::wstring>(bufferedLoad<std::string>(streamIn)); //throw FileError


    if (!WIFEXITED(statusCode)) //signalled, crashed?
        throw SysError(formatSystemError("waitpid", WIFSIGNALED(statusCode) ?
                                         L"Killed by signal " + numberTo<std::wstring>(WTERMSIG(statusCode)) :
                                         L"Exit status "      + numberTo<std::wstring>(statusCode),
                                         utfTo<std::wstring>(trimCpy(output))));

    const int exitCode = WEXITSTATUS(statusCode); //precondition: "WIFEXITED() == true"
    if (exitCode == EC_CHILD_LAUNCH_FAILED || //child process should already have provided details to STDOUT
        exitCode == 127) //details should have been streamed to STDERR: used by /bin/sh, e.g. failure to execute due to missing .so file
        throw SysError(utfTo<std::wstring>(trimCpy(output)));

    return { exitCode, output };
}


void zen::openWithDefaultApp(const Zstring& itemPath) //throw FileError
{
    try
    {
        const Zstring cmdTemplate = R"(xdg-open "%x")"; //doesn't block => no need for time out!
        const Zstring cmdLine = replaceCpy(cmdTemplate, Zstr("%x"), itemPath);

        if (const auto& [exitCode, output] = consoleExecute(cmdLine, std::nullopt /*timeoutMs*/); //throw SysError, (SysErrorTimeOut)
            exitCode != 0)
            throw SysError(formatSystemError(utfTo<std::string>(cmdTemplate), replaceCpy(_("Exit code %x"), L"%x", numberTo<std::wstring>(exitCode)), output));
    }
    catch (const SysError& e) { throw FileError(replaceCpy(_("Cannot open file %x."), L"%x", fmtPath(itemPath)), e.toString()); }
}


