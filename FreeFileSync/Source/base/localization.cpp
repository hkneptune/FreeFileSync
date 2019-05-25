// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "localization.h"
#include <unordered_map>
#include <map>
#include <list>
#include <iterator>
#include <zen/string_tools.h>
#include <zen/file_traverser.h>
#include <zen/file_io.h>
#include <zen/i18n.h>
#include <zen/format_unit.h>
#include <wx/intl.h>
#include <wx/log.h>
#include "parse_plural.h"
#include "parse_lng.h"
#include "ffs_paths.h"

    #include <wchar.h> //wcscasecmp


using namespace zen;
using namespace fff;


namespace
{
class FFSTranslation : public TranslationHandler
{
public:
    FFSTranslation(const Zstring& lngFilePath, wxLanguage langId); //throw lng::ParsingError, plural::ParsingError

    wxLanguage getLangId() const { return langId_; }

    std::wstring translate(const std::wstring& text) const override
    {
        //look for translation in buffer table
        auto it = transMapping_.find(text);
        if (it != transMapping_.end() && !it->second.empty())
            return it->second;
        return text; //fallback
    }

    std::wstring translate(const std::wstring& singular, const std::wstring& plural, int64_t n) const override
    {
        auto it = transMappingPl_.find({ singular, plural });
        if (it != transMappingPl_.end())
        {
            const size_t formNo = pluralParser_->getForm(n);
            assert(formNo < it->second.size());
            if (formNo < it->second.size())
                return replaceCpy(it->second[formNo], L"%x", formatNumber(n));
        }
        return replaceCpy(std::abs(n) == 1 ? singular : plural, L"%x", formatNumber(n)); //fallback
    }

private:
    using Translation       = std::unordered_map<std::wstring, std::wstring>; //hash_map is 15% faster than std::map on GCC
    using TranslationPlural = std::map<std::pair<std::wstring, std::wstring>, std::vector<std::wstring>>;

    Translation       transMapping_; //map original text |-> translation
    TranslationPlural transMappingPl_;
    std::unique_ptr<plural::PluralForm> pluralParser_; //bound!
    const wxLanguage langId_;
};


FFSTranslation::FFSTranslation(const Zstring& lngFilePath, wxLanguage langId) : langId_(langId) //throw lng::ParsingError, plural::ParsingError
{
    std::string inputStream;
    try
    {
        inputStream = loadBinContainer<std::string>(lngFilePath,  nullptr /*notifyUnbufferedIO*/); //throw FileError
    }
    catch (const FileError& e)
    {
        throw lng::ParsingError({ e.toString(), 0, 0 });
        //passing FileError is too high a level for Parsing error, OTOH user is unlikely to see this since file I/O issues are sorted out by getExistingTranslations()!
    }

    lng::TransHeader          header;
    lng::TranslationMap       transUtf;
    lng::TranslationPluralMap transPluralUtf;
    lng::parseLng(inputStream, header, transUtf, transPluralUtf); //throw ParsingError

    pluralParser_ = std::make_unique<plural::PluralForm>(header.pluralDefinition); //throw plural::ParsingError

    for (const auto& [original, translation] : transUtf)
        transMapping_.emplace(utfTo<std::wstring>(original),
                              utfTo<std::wstring>(translation));

    for (const auto& [singAndPlural, pluralForms] : transPluralUtf)
    {
        std::vector<std::wstring> transPluralForms;
        for (const std::string& pf : pluralForms)
            transPluralForms.push_back(utfTo<std::wstring>(pf));

        transMappingPl_.insert(
        {
            {
                utfTo<std::wstring>(singAndPlural.first),
                utfTo<std::wstring>(singAndPlural.second)
            },
            std::move(transPluralForms) });
    }
}


std::vector<TranslationInfo> loadTranslations()
{
    std::vector<TranslationInfo> locMapping;
    {
        //default entry:
        TranslationInfo newEntry;
        newEntry.languageID     = wxLANGUAGE_ENGLISH_US;
        newEntry.languageName   = std::wstring(L"English (US)") + LTR_MARK; //handle weak ")" for bidi-algorithm
        newEntry.translatorName = L"Zenju";
        newEntry.languageFlag   = L"flag_usa.png";
        newEntry.langFilePath   = Zstr("");
        locMapping.push_back(newEntry);
    }

    //search language files available
    std::vector<Zstring> lngFilePaths;

    traverseFolder(fff::getResourceDirPf() + Zstr("Languages"), [&](const FileInfo& fi) //FileInfo is ambiguous on OS X
    {
        if (endsWith(fi.fullPath, Zstr(".lng")))
            lngFilePaths.push_back(fi.fullPath);
    }, nullptr, nullptr, [&](const std::wstring& errorMsg) { assert(false); }); //errors are not really critical in this context

    for (const Zstring& filePath : lngFilePaths)
    {
        try
        {
            const std::string stream = loadBinContainer<std::string>(filePath, nullptr /*notifyUnbufferedIO*/); //throw FileError

            lng::TransHeader lngHeader;
            lng::parseHeader(stream, lngHeader); //throw ParsingError

            assert(!lngHeader.languageName  .empty());
            assert(!lngHeader.translatorName.empty());
            assert(!lngHeader.localeName    .empty());
            assert(!lngHeader.flagFile      .empty());
            /*
            Some ISO codes are used by multiple wxLanguage IDs which can lead to incorrect mapping by wxLocale::FindLanguageInfo()!!!
            => Identify by description, e.g. "Chinese (Traditional)". The following ids are affected:
                wxLANGUAGE_CHINESE_TRADITIONAL
                wxLANGUAGE_ENGLISH_UK
                wxLANGUAGE_SPANISH //non-unique, but still mapped correctly (or is it incidentally???)
                wxLANGUAGE_SERBIAN //
            */
            if (const wxLanguageInfo* locInfo = wxLocale::FindLanguageInfo(utfTo<wxString>(lngHeader.localeName)))
            {
                TranslationInfo newEntry;
                newEntry.languageID     = static_cast<wxLanguage>(locInfo->Language);
                newEntry.languageName   = utfTo<std::wstring>(lngHeader.languageName);
                newEntry.translatorName = utfTo<std::wstring>(lngHeader.translatorName);
                newEntry.languageFlag   = utfTo<std::wstring>(lngHeader.flagFile);
                newEntry.langFilePath   = filePath;
                locMapping.push_back(newEntry);
            }
            else assert(false);
        }
        catch (FileError&) { assert(false); }
        catch (lng::ParsingError&) { assert(false); } //better not show an error message here; scenario: batch jobs
    }

    std::sort(locMapping.begin(), locMapping.end(), [](const TranslationInfo& lhs, const TranslationInfo& rhs)
    {
        return LessNaturalSort()(utfTo<Zstring>(lhs.languageName),
                                 utfTo<Zstring>(rhs.languageName)); //use a more "natural" sort: ignore case and diacritics
    });
    return locMapping;
}


wxLanguage mapLanguageDialect(wxLanguage language)
{
    switch (static_cast<int>(language)) //avoid enumeration value wxLANGUAGE_*' not handled in switch [-Wswitch-enum]
    {
        //variants of wxLANGUAGE_ARABIC
        case wxLANGUAGE_ARABIC_ALGERIA:
        case wxLANGUAGE_ARABIC_BAHRAIN:
        case wxLANGUAGE_ARABIC_EGYPT:
        case wxLANGUAGE_ARABIC_IRAQ:
        case wxLANGUAGE_ARABIC_JORDAN:
        case wxLANGUAGE_ARABIC_KUWAIT:
        case wxLANGUAGE_ARABIC_LEBANON:
        case wxLANGUAGE_ARABIC_LIBYA:
        case wxLANGUAGE_ARABIC_MOROCCO:
        case wxLANGUAGE_ARABIC_OMAN:
        case wxLANGUAGE_ARABIC_QATAR:
        case wxLANGUAGE_ARABIC_SAUDI_ARABIA:
        case wxLANGUAGE_ARABIC_SUDAN:
        case wxLANGUAGE_ARABIC_SYRIA:
        case wxLANGUAGE_ARABIC_TUNISIA:
        case wxLANGUAGE_ARABIC_UAE:
        case wxLANGUAGE_ARABIC_YEMEN:
            return wxLANGUAGE_ARABIC;

        //variants of wxLANGUAGE_CHINESE_SIMPLIFIED
        case wxLANGUAGE_CHINESE:
        case wxLANGUAGE_CHINESE_SINGAPORE:
            return wxLANGUAGE_CHINESE_SIMPLIFIED;

        //variants of wxLANGUAGE_CHINESE_TRADITIONAL
        case wxLANGUAGE_CHINESE_TAIWAN:
        case wxLANGUAGE_CHINESE_HONGKONG:
        case wxLANGUAGE_CHINESE_MACAU:
            return wxLANGUAGE_CHINESE_TRADITIONAL;

        //variants of wxLANGUAGE_DUTCH
        case wxLANGUAGE_DUTCH_BELGIAN:
            return wxLANGUAGE_DUTCH;

        //variants of wxLANGUAGE_ENGLISH_UK
        case wxLANGUAGE_ENGLISH_AUSTRALIA:
        case wxLANGUAGE_ENGLISH_NEW_ZEALAND:
        case wxLANGUAGE_ENGLISH_TRINIDAD:
        case wxLANGUAGE_ENGLISH_CARIBBEAN:
        case wxLANGUAGE_ENGLISH_JAMAICA:
        case wxLANGUAGE_ENGLISH_BELIZE:
        case wxLANGUAGE_ENGLISH_EIRE:
        case wxLANGUAGE_ENGLISH_SOUTH_AFRICA:
        case wxLANGUAGE_ENGLISH_ZIMBABWE:
        case wxLANGUAGE_ENGLISH_BOTSWANA:
        case wxLANGUAGE_ENGLISH_DENMARK:
            return wxLANGUAGE_ENGLISH_UK;

        //variants of wxLANGUAGE_ENGLISH_US
        case wxLANGUAGE_ENGLISH:
        case wxLANGUAGE_ENGLISH_CANADA:
        case wxLANGUAGE_ENGLISH_PHILIPPINES:
            return wxLANGUAGE_ENGLISH_US;

        //variants of wxLANGUAGE_FRENCH
        case wxLANGUAGE_FRENCH_BELGIAN:
        case wxLANGUAGE_FRENCH_CANADIAN:
        case wxLANGUAGE_FRENCH_LUXEMBOURG:
        case wxLANGUAGE_FRENCH_MONACO:
        case wxLANGUAGE_FRENCH_SWISS:
            return wxLANGUAGE_FRENCH;

        //variants of wxLANGUAGE_GERMAN
        case wxLANGUAGE_GERMAN_AUSTRIAN:
        case wxLANGUAGE_GERMAN_BELGIUM:
        case wxLANGUAGE_GERMAN_LIECHTENSTEIN:
        case wxLANGUAGE_GERMAN_LUXEMBOURG:
        case wxLANGUAGE_GERMAN_SWISS:
            return wxLANGUAGE_GERMAN;

        //variants of wxLANGUAGE_ITALIAN
        case wxLANGUAGE_ITALIAN_SWISS:
            return wxLANGUAGE_ITALIAN;

        //variants of wxLANGUAGE_NORWEGIAN_BOKMAL
        case wxLANGUAGE_NORWEGIAN_NYNORSK:
            return wxLANGUAGE_NORWEGIAN_BOKMAL;

        //variants of wxLANGUAGE_ROMANIAN
        case wxLANGUAGE_MOLDAVIAN:
            return wxLANGUAGE_ROMANIAN;

        //variants of wxLANGUAGE_RUSSIAN
        case wxLANGUAGE_RUSSIAN_UKRAINE:
            return wxLANGUAGE_RUSSIAN;

        //variants of wxLANGUAGE_SERBIAN
        case wxLANGUAGE_SERBIAN_CYRILLIC:
        case wxLANGUAGE_SERBIAN_LATIN:
        case wxLANGUAGE_SERBO_CROATIAN:
            return wxLANGUAGE_SERBIAN;

        //variants of wxLANGUAGE_SPANISH
        case wxLANGUAGE_SPANISH_ARGENTINA:
        case wxLANGUAGE_SPANISH_BOLIVIA:
        case wxLANGUAGE_SPANISH_CHILE:
        case wxLANGUAGE_SPANISH_COLOMBIA:
        case wxLANGUAGE_SPANISH_COSTA_RICA:
        case wxLANGUAGE_SPANISH_DOMINICAN_REPUBLIC:
        case wxLANGUAGE_SPANISH_ECUADOR:
        case wxLANGUAGE_SPANISH_EL_SALVADOR:
        case wxLANGUAGE_SPANISH_GUATEMALA:
        case wxLANGUAGE_SPANISH_HONDURAS:
        case wxLANGUAGE_SPANISH_MEXICAN:
        case wxLANGUAGE_SPANISH_MODERN:
        case wxLANGUAGE_SPANISH_NICARAGUA:
        case wxLANGUAGE_SPANISH_PANAMA:
        case wxLANGUAGE_SPANISH_PARAGUAY:
        case wxLANGUAGE_SPANISH_PERU:
        case wxLANGUAGE_SPANISH_PUERTO_RICO:
        case wxLANGUAGE_SPANISH_URUGUAY:
        case wxLANGUAGE_SPANISH_US:
        case wxLANGUAGE_SPANISH_VENEZUELA:
            return wxLANGUAGE_SPANISH;

        //variants of wxLANGUAGE_SWEDISH
        case wxLANGUAGE_SWEDISH_FINLAND:
            return wxLANGUAGE_SWEDISH;

        //languages without variants:
        //case wxLANGUAGE_BULGARIAN:
        //case wxLANGUAGE_CROATIAN:
        //case wxLANGUAGE_CZECH:
        //case wxLANGUAGE_DANISH:
        //case wxLANGUAGE_FINNISH:
        //case wxLANGUAGE_GREEK:
        //case wxLANGUAGE_HINDI:
        //case wxLANGUAGE_HEBREW:
        //case wxLANGUAGE_HUNGARIAN:
        //case wxLANGUAGE_JAPANESE:
        //case wxLANGUAGE_KOREAN:
        //case wxLANGUAGE_LITHUANIAN:
        //case wxLANGUAGE_POLISH:
        //case wxLANGUAGE_PORTUGUESE:
        //case wxLANGUAGE_PORTUGUESE_BRAZILIAN:
        //case wxLANGUAGE_SCOTS_GAELIC:
        //case wxLANGUAGE_SLOVAK:
        //case wxLANGUAGE_SLOVENIAN:
        //case wxLANGUAGE_TURKISH:
        //case wxLANGUAGE_UKRAINIAN:
        default:
            return language;
    }
}


//we need to interface with wxWidgets' translation handling for a few translations used in their internal source files
// => since there is no better API: dynamically generate a MO file and feed it to wxTranslation
class MemoryTranslationLoader : public wxTranslationsLoader
{
public:
    MemoryTranslationLoader(wxLanguage langId, std::map<std::string, std::wstring>&& transMapping) :
        canonicalName_(wxLocale::GetLanguageCanonicalName(langId))
    {
        //https://www.gnu.org/software/gettext/manual/html_node/MO-Files.html
        transMapping[""] = L"Content-Type: text/plain; charset=UTF-8\n";

        const int headerSize = 28;
        writeNumber<uint32_t>(moBuf_, 0x950412de); //magic number
        writeNumber<uint32_t>(moBuf_, 0); //format version
        writeNumber<uint32_t>(moBuf_, transMapping.size()); //string count
        writeNumber<uint32_t>(moBuf_, headerSize);                           //string references offset: original
        writeNumber<uint32_t>(moBuf_, headerSize + 8 * transMapping.size()); //string references offset: translation
        writeNumber<uint32_t>(moBuf_, 0); //size of hashing table
        writeNumber<uint32_t>(moBuf_, 0); //offset of hashing table

        const int stringsOffset = headerSize + 2 * 8 * transMapping.size();
        std::string stringsList;

        for (const auto& [original, translation] : transMapping)
        {
            writeNumber<uint32_t>(moBuf_, original.size()); //string length
            writeNumber<uint32_t>(moBuf_, stringsOffset + stringsList.size()); //string offset
            stringsList.append(original.c_str(), original.size() + 1); //include 0-termination
        }

        for (const auto& item : transMapping)
        {
            const auto& translation = utfTo<std::string>(item.second);
            writeNumber<uint32_t>(moBuf_, translation.size()); //string length
            writeNumber<uint32_t>(moBuf_, stringsOffset + stringsList.size()); //string offset
            stringsList.append(translation.c_str(), translation.size() + 1); //include 0-termination
        }

        writeArray(moBuf_, stringsList.c_str(), stringsList.size());
    }

    wxMsgCatalog* LoadCatalog(const wxString& domain, const wxString& lang) override
    {
        //"lang" is NOT (exactly) what we return from GetAvailableTranslations(), but has a little "extra", e.g.: de_DE.WINDOWS-1252 or ar.WINDOWS-1252
        if (equalAsciiNoCase(extractIsoLangCode(lang), extractIsoLangCode(canonicalName_)))
            return wxMsgCatalog::CreateFromData(wxScopedCharBuffer::CreateNonOwned(moBuf_.ref().c_str(), moBuf_.ref().size()), domain);
        assert(false);
        return nullptr;
    }

    wxArrayString GetAvailableTranslations(const wxString& domain) const override
    {
        wxArrayString available;
        available.push_back(canonicalName_);
        return available;
    }

private:
    static wxString extractIsoLangCode(wxString langCode)
    {
        langCode = beforeLast(langCode, L".", IF_MISSING_RETURN_ALL);
        return beforeLast(langCode, L"_", IF_MISSING_RETURN_ALL);
    }

    const wxString canonicalName_;
    MemoryStreamOut<std::string> moBuf_;
};


//global wxWidgets localization: sets up C localization runtime as well!
class wxWidgetsLocale
{
public:
    static wxWidgetsLocale& getInstance()
    {
        static wxWidgetsLocale inst;
        return inst;
    }

    void init(wxLanguage lng)
    {
        locale_.reset(); //avoid global locale lifetime overlap! wxWidgets cannot handle this and will crash!
        locale_ = std::make_unique<wxLocale>();

        const wxLanguageInfo* sysLngInfo = wxLocale::GetLanguageInfo(wxLocale::GetSystemLanguage());
        const wxLanguageInfo* selLngInfo = wxLocale::GetLanguageInfo(lng);

        const bool sysLangIsRTL      = sysLngInfo ? sysLngInfo->LayoutDirection == wxLayout_RightToLeft : false;
        const bool selectedLangIsRTL = selLngInfo ? selLngInfo->LayoutDirection == wxLayout_RightToLeft : false;

        wxLogNull dummy; //rather than implementing a reasonable error handling wxWidgets decides to shows a modal dialog in wxLocale::Init -> at least we can shut it up!
        if (sysLangIsRTL == selectedLangIsRTL)
            locale_->Init(wxLANGUAGE_DEFAULT); //use sys-lang to preserve sub-language specific rules (e.g. german swiss number punctation)
        else
            locale_->Init(lng); //have to use the supplied language to enable RTL layout different than user settings

        locLng_ = lng;
    }

    void tearDown() { locale_.reset(); locLng_ = wxLANGUAGE_UNKNOWN; }

    wxLanguage getLanguage() const { return locLng_; }

private:
    wxWidgetsLocale() {}
    ~wxWidgetsLocale() { assert(!locale_); }

    std::unique_ptr<wxLocale> locale_;
    wxLanguage locLng_ = wxLANGUAGE_UNKNOWN;
};
}


const std::vector<TranslationInfo>& fff::getExistingTranslations()
{
    static const std::vector<TranslationInfo> translations = loadTranslations();
    return translations;
}


void fff::releaseWxLocale()
{
    wxWidgetsLocale::getInstance().tearDown();
    setTranslator(nullptr); //good place for clean up rather than some time during static destruction: is this an actual benefit???
}


void fff::setLanguage(wxLanguage lng) //throw FileError
{
    if (getLanguage() == lng && wxWidgetsLocale::getInstance().getLanguage() == lng)
        return; //support polling

    //(try to) retrieve language file
    Zstring langFilePath;

    for (const TranslationInfo& e : getExistingTranslations())
        if (e.languageID == lng)
        {
            langFilePath = e.langFilePath;
            break;
        }

    //load language file into buffer
    if (langFilePath.empty()) //if languageFile is empty, texts will be english by default
    {
        setTranslator(nullptr);
        lng = wxLANGUAGE_ENGLISH_US;
    }
    else
        try
        {
            setTranslator(std::make_unique<FFSTranslation>(langFilePath, lng)); //throw lng::ParsingError, plural::ParsingError
        }
        catch (lng::ParsingError& e)
        {
            throw FileError(replaceCpy(replaceCpy(replaceCpy(_("Error parsing file %x, row %y, column %z."),
                                                             L"%x", fmtPath(langFilePath)),
                                                  L"%y", numberTo<std::wstring>(e.row + 1)),
                                       L"%z", numberTo<std::wstring>(e.col + 1))
                            + L"\n\n" + e.msg);
        }
        catch (plural::ParsingError&)
        {
            throw FileError(L"Invalid plural form definition: " + fmtPath(langFilePath)); //user should never see this!
        }

    //handle RTL swapping: we need wxWidgets to do this
    wxWidgetsLocale::getInstance().init(lng);

    //add translation for wxWidgets-internal strings:
    assert(wxTranslations::Get()); //already initialized by wxLocale
    if (wxTranslations* wxtrans = wxTranslations::Get())
    {
        std::map<std::string, std::wstring> transMapping =
        {
        };
        wxtrans->SetLanguage(lng); //!= wxLocale's language, which could be wxLANGUAGE_DEFAULT (see wxWidgetsLocale)
        wxtrans->SetLoader(new MemoryTranslationLoader(lng, std::move(transMapping)));
        [[maybe_unused]] const bool catalogAdded = wxtrans->AddCatalog(wxString());
        assert(catalogAdded || lng == wxLANGUAGE_ENGLISH_US);
    }
}


wxLanguage fff::getLanguage()
{
    std::shared_ptr<const TranslationHandler> t = getTranslator();
    const FFSTranslation* loc = dynamic_cast<const FFSTranslation*>(t.get());
    return loc ? loc->getLangId() : wxLANGUAGE_ENGLISH_US;
}


wxLanguage fff::getSystemLanguage()
{
    return mapLanguageDialect(static_cast<wxLanguage>(wxLocale::GetSystemLanguage()));
}
