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
std::vector<Zstring> parseCommandline(const Zstring& cmdLine);


DEFINE_NEW_SYS_ERROR(SysErrorTimeOut)
[[nodiscard]] std::pair<int /*exit code*/, std::wstring> consoleExecute(const Zstring& cmdLine, std::optional<int> timeoutMs); //throw SysError, SysErrorTimeOut
/* limitations: Windows:     cmd.exe returns exit code 1 if file not found (instead of throwing SysError) => nodiscard!
                Linux/macOS: SysErrorTimeOut leaves zombie process behind                  */

void openWithDefaultApp(const Zstring& itemPath); //throw FileError
}

#endif //SHELL_EXECUTE_H_23482134578134134
