// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef APPLICATION_H_081568741942010985702395
#define APPLICATION_H_081568741942010985702395

#include <vector>
#include <zen/zstring.h>
#include <wx/app.h>
#include "config.h"
#include "return_codes.h"


namespace fff //avoid name clash with "int ffs()" for fuck's sake! (maxOS, Linux issue only: <string> internally includes <strings.h>, WTF!)
{
class Application : public wxApp
{
private:
    bool OnInit() override;
    int  OnRun () override;
    int  OnExit() override;
    wxLayoutDirection GetLayoutDirection() const override;
    void onEnterEventLoop();

    void runGuiMode  (const Zstring& globalConfigFile);
    void runGuiMode  (const Zstring& globalConfigFile, const XmlGuiConfig& guiCfg, const std::vector<Zstring>& cfgFilePaths, bool startComparison);
    void runBatchMode(const Zstring& globalConfigFile, const XmlBatchConfig& batchCfg, const Zstring& cfgFilePath);

    FfsExitCode exitCode_ = FfsExitCode::success;
};
}

#endif //APPLICATION_H_081568741942010985702395
