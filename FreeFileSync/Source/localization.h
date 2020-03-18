// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef LOCALIZATION_H_8917342083178321534
#define LOCALIZATION_H_8917342083178321534

#include <vector>
#include <zen/file_error.h>
#include <wx/intl.h>
#include <wx/language.h>


namespace fff
{
struct TranslationInfo
{
    wxLanguage languageID = wxLANGUAGE_UNKNOWN;
    std::wstring languageName;
    std::wstring translatorName;
    std::wstring languageFlag;
    Zstring lngFileName;
    std::string lngStream;
};
const std::vector<TranslationInfo>& getExistingTranslations();

wxLanguage getSystemLanguage();
wxLanguage getLanguage();
wxLayoutDirection getLayoutDirection();

void setLanguage(wxLanguage lng); //throw FileError

void releaseWxLocale(); //wxLocale crashes miserably on wxGTK when destructor runs during global cleanup => call in wxApp::OnExit
//"You should delete all wxWidgets object that you created by the time OnExit finishes. In particular, do not destroy them from application class' destructor!"
}

#endif //LOCALIZATION_H_8917342083178321534
