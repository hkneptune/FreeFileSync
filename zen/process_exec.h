// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef SHELL_EXECUTE_H_23482134578134134
#define SHELL_EXECUTE_H_23482134578134134

#include "file_error.h"


namespace zen
{
Zstring escapeCommandArg(const Zstring& arg);


DEFINE_NEW_SYS_ERROR(SysErrorTimeOut)
[[nodiscard]] std::pair<int /*exit code*/, Zstring> consoleExecute(const Zstring& cmdLine, std::optional<int> timeoutMs); //throw SysError, SysErrorTimeOut
/* Windows: - cmd.exe returns exit code 1 if file not found (instead of throwing SysError) => nodiscard!
            - handles elevation when CreateProcess() would fail with ERROR_ELEVATION_REQUIRED!
            - no support for UNC path and Unicode on Win7; apparently no issue on Win10!
   Linux/macOS: SysErrorTimeOut leaves zombie process behind if timeoutMs is used             */

void openWithDefaultApp(const Zstring& itemPath); //throw FileError
}

#endif //SHELL_EXECUTE_H_23482134578134134
