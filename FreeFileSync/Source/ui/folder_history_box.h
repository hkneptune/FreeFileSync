// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef FOLDER_HISTORY_BOX_H_08170517045945
#define FOLDER_HISTORY_BOX_H_08170517045945

#include <wx/combobox.h>
#include <memory>
#include <zen/zstring.h>
#include <zen/stl_tools.h>
#include <zen/utf.h>


namespace fff
{
class FolderHistory //combobox with history function + functionality to delete items (DEL)
{
public:
    FolderHistory() {}

    FolderHistory(const std::vector<Zstring>& folderPathPhrases, size_t maxSize) :
        maxSize_(maxSize),
        folderPathPhrases_(folderPathPhrases)
    {
        if (folderPathPhrases_.size() > maxSize_) //keep maximal size of history list
            folderPathPhrases_.resize(maxSize_);
    }

    const std::vector<Zstring>& getList() const { return folderPathPhrases_; }

    static const wxString separationLine() { return wxString(50, EM_DASH); }

    void addItem(const Zstring& folderPathPhrase)
    {
        if (folderPathPhrase.empty() || folderPathPhrase == zen::utfTo<Zstring>(separationLine()))
            return;

        const Zstring nameTmp = zen::trimCpy(folderPathPhrase);

        //insert new folder or put it to the front if already existing
        zen::erase_if(folderPathPhrases_, [&](const Zstring& item) { return equalFilePath(item, nameTmp); });

        folderPathPhrases_.insert(folderPathPhrases_.begin(), nameTmp);

        if (folderPathPhrases_.size() > maxSize_) //keep maximal size of history list
            folderPathPhrases_.resize(maxSize_);
    }

    void delItem(const Zstring& folderPathPhrase) { zen::erase_if(folderPathPhrases_, [&](const Zstring& item) { return equalFilePath(item, folderPathPhrase); }); }

private:
    size_t maxSize_ = 0;
    std::vector<Zstring> folderPathPhrases_;
};


class FolderHistoryBox : public wxComboBox
{
public:
    FolderHistoryBox(wxWindow* parent,
                     wxWindowID id,
                     const wxString& value = {},
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize,
                     int n = 0,
                     const wxString choices[] = nullptr,
                     long style = 0,
                     const wxValidator& validator = wxDefaultValidator,
                     const wxString& name = wxComboBoxNameStr);

    void init(const std::shared_ptr<FolderHistory>& sharedHistory) { sharedHistory_ = sharedHistory; }

    void setValue(const wxString& folderPathPhrase)
    {
        setValueAndUpdateList(folderPathPhrase); //required for setting value correctly; Linux: ensure the dropdown is shown as being populated
    }

    // GetValue

private:
    void OnKeyEvent(wxKeyEvent& event);
    void OnRequireHistoryUpdate(wxEvent& event);
    void setValueAndUpdateList(const wxString& folderPathPhrase);

    std::shared_ptr<FolderHistory> sharedHistory_;
};
}

#endif //FOLDER_HISTORY_BOX_H_08170517045945
