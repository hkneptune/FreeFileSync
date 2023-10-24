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
    static void create(const Zstring& cfgFilePath);

    void loadConfig(const Zstring& filepath);

private:
    MainDialog(const Zstring& cfgFilePath);
    ~MainDialog();

    void onBeforeSystemShutdown(); //last chance to do something useful before killing the application!

    void onClose          (wxCloseEvent&  event ) override  { Destroy(); }
    void onShowHelp       (wxCommandEvent& event) override { wxLaunchDefaultBrowser(L"https://freefilesync.org/manual.php?topic=realtimesync"); }
    void onMenuAbout      (wxCommandEvent& event) override;
    void onAddFolder      (wxCommandEvent& event) override;
    void onRemoveFolder   (wxCommandEvent& event);
    void onRemoveTopFolder(wxCommandEvent& event) override;
    void onLocalKeyEvent  (wxKeyEvent&     event);
    void onStart          (wxCommandEvent& event) override;
    void onConfigNew      (wxCommandEvent& event) override { loadConfig({}); }
    void onConfigSave     (wxCommandEvent& event) override;
    void onConfigLoad     (wxCommandEvent& event) override;
    void onMenuQuit       (wxCommandEvent& event) override { Close(); }
    void onFilesDropped(zen::FileDropEvent& event);

    void setConfiguration(const XmlRealConfig& cfg);
    XmlRealConfig getConfiguration();
    void setLastUsedConfig(const Zstring& filepath);

    void insertAddFolder(const std::vector<Zstring>& newFolders, size_t pos);
    void removeAddFolder(size_t pos);

    std::unique_ptr<FolderSelector2> firstFolderPanel_;
    std::vector<DirectoryPanel*> additionalFolderPanels_; //additional pairs to the standard pair


    const Zstring lastRunConfigPath_;
    Zstring activeConfigFile_; //optional

    Zstring folderLastSelected_;

    zen::AsyncGuiQueue guiQueue_; //schedule and run long-running tasks asynchronously, but process results on GUI queue

    const zen::SharedRef<std::function<void()>> onBeforeSystemShutdownCookie_ = zen::makeSharedRef<std::function<void()>>([this] { onBeforeSystemShutdown(); });
};
}

#endif //MAIN_DLG_H_2384790842252445
