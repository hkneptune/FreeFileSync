// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef XML_PROC_H_0813748158321813490
#define XML_PROC_H_0813748158321813490

#include <vector>
#include <zen/zstring.h>
#include <wx/language.h>

namespace rts
{
struct XmlRealConfig
{
    std::vector<Zstring> directories;
    Zstring commandline;
    unsigned int delay = 10;
};

std::pair<XmlRealConfig, std::wstring /*warningMsg*/>  readConfig(const Zstring& filePath); //throw FileError
void writeConfig(const XmlRealConfig& config, const Zstring& filePath); //throw FileError


//reuse (some of) FreeFileSync's xml files
std::pair<XmlRealConfig, std::wstring /*warningMsg*/> readRealOrBatchConfig(const Zstring& filePath); //throw FileError

wxLanguage getProgramLanguage(); //throw FileError
}

#endif //XML_PROC_H_0813748158321813490
