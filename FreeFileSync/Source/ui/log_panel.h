// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef LOG_PANEL_3218470817450193
#define LOG_PANEL_3218470817450193

#include <zen/error_log.h>
#include "gui_generated.h"
#include <wx+/grid.h>


namespace fff
{
class MessageView;

class LogPanel : public LogPanelGenerated
{
public:
    explicit LogPanel(wxWindow* parent);

    void setLog(const std::shared_ptr<const zen::ErrorLog>& log);

private:
    MessageView& getDataView();
    void updateGrid();

    void onErrors  (wxCommandEvent& event) override;
    void onWarnings(wxCommandEvent& event) override;
    void onInfo    (wxCommandEvent& event) override;
    void onMsgGridContext (zen::GridContextMenuEvent& event);
    void onGridKeyEvent (wxKeyEvent& event);
    void onLocalKeyEvent(wxKeyEvent& event);

    void copySelectionToClipboard();

    bool processingKeyEventHandler_ = false;
};
}

#endif //LOG_PANEL_3218470817450193
