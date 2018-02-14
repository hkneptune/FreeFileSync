// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef MAIN_DLG_H_2384790842252445
#define MAIN_DLG_H_2384790842252445

#include "gui_generated.h"
#include <vector>
#include <memory>
#include <zen/zstring.h>
#include <wx+/async_task.h>
#include <wx+/file_drop.h>
#include <wx/timer.h>
#include "folder_selector2.h"


namespace rts
{
struct XmlRealConfig;
class DirectoryPanel;


class MainDialog: public MainDlgGenerated
{
public:
    static void create(const Zstring& cfgFile);

    void onQueryEndSession(); //last chance to do something useful before killing the application!

private:
    MainDialog(wxDialog* dlg, const Zstring& cfgFileName);
    ~MainDialog();

    void loadConfig(const Zstring& filepath);

    void OnClose          (wxCloseEvent&  event ) override  { Destroy(); }
    void OnShowHelp       (wxCommandEvent& event) override;
    void OnMenuAbout      (wxCommandEvent& event) override;
    void OnAddFolder      (wxCommandEvent& event) override;
    void OnRemoveFolder   (wxCommandEvent& event);
    void OnRemoveTopFolder(wxCommandEvent& event) override;
    void OnKeyPressed     (wxKeyEvent&     event);
    void OnStart          (wxCommandEvent& event) override;
    void OnConfigNew      (wxCommandEvent& event) override;
    void OnConfigSave     (wxCommandEvent& event) override;
    void OnConfigLoad     (wxCommandEvent& event) override;
    void OnMenuQuit       (wxCommandEvent& event) override { Close(); }
    void onFilesDropped(zen::FileDropEvent& event);

    void setConfiguration(const XmlRealConfig& cfg);
    XmlRealConfig getConfiguration();
    void setLastUsedConfig(const Zstring& filepath);

    void addFolder(const std::vector<Zstring>& newFolders, bool addFront = false);
    void removeAddFolder(size_t pos);
    void clearAddFolders();

    std::unique_ptr<FolderSelector2> dirpathFirst;
    std::vector<DirectoryPanel*> dirpathsExtra; //additional pairs to the standard pair


    const Zstring lastRunConfigPath_;
    Zstring activeConfigFile_;

    zen::AsyncGuiQueue guiQueue_; //schedule and run long-running tasks asynchronously, but process results on GUI queue
};
}

#endif //MAIN_DLG_H_2384790842252445
