// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PARSE_LNG_H_46794693622675638
#define PARSE_LNG_H_46794693622675638

#include <unordered_map>
#include <unordered_set>
#include <zen/utf.h>
#include <zen/ring_buffer.h>
#include "parse_plural.h"


namespace lng
{
//singular forms
using TranslationMap = std::unordered_map<std::string, std::string>; //orig |-> translation

//plural forms
using SingularPluralPair   = std::pair<std::string, std::string>; //1 house | %x houses
using PluralForms          = std::vector<std::string>; //1 dom | 2 domy | %x domów
using TranslationPluralMap = std::unordered_map<SingularPluralPair, PluralForms>; //(sing/plu) |-> pluralforms

struct TransHeader
{
    std::string languageName;     //display name: "English (UK)"
    std::string translatorName;   //"Zenju"
    std::string localeName;       //ISO 639 language code + ISO 3166 country code, e.g. "en_GB", or "en_US"
    std::string flagFile;         //"england.png"
    int pluralCount = 0;          //2
    std::string pluralDefinition; //"n == 1 ? 0 : 1"
};

struct ParsingError
{
    std::wstring msg;
    size_t row = 0; //starting with 0
    size_t col = 0; //
};
TransHeader parseHeader(const std::string& fileStream); //throw ParsingError
void parseLng(const std::string& fileStream, TransHeader& header, TranslationMap& out, TranslationPluralMap& pluralOut); //throw ParsingError

class TranslationUnorderedList; //unordered list of unique translation items
std::string generateLng(const TranslationUnorderedList& in, const TransHeader& header);



















//--------------------------- implementation ---------------------------
}

template<> struct std::hash<lng::SingularPluralPair>
{
    size_t operator()(const lng::SingularPluralPair& str) const
    {
        zen::FNV1aHash<size_t> hash2; //shut up "GCC: shadow declaration"
        for (const char c : str.first ) hash2.add(c);
        for (const char c : str.second) hash2.add(c);
        return hash2.get();
    }
};

namespace lng
{
enum class TranslationNewItemPos
{
    rel,
    top
};

class TranslationUnorderedList //unordered list of unique translation items
{
public:
    TranslationUnorderedList(TranslationNewItemPos newItemPos, TranslationMap&& transOld, TranslationPluralMap&& transPluralOld) :
        newItemPos_(newItemPos), transOld_(std::move(transOld)), transPluralOld_(std::move(transPluralOld)) {}

    void addItem(const std::string& orig)
    {
        if (!transUnique_.insert(orig).second) return;
        auto it = transOld_.find(orig);
        if (it != transOld_.end() && !it->second.empty()) //preserve old translation from .lng file if existing
            sequence_.push_back(std::make_shared<RegularItem>(std::make_pair(orig, it->second)));
        else
            switch (newItemPos_)
            {
                case TranslationNewItemPos::rel:
                    sequence_.push_back(std::make_shared<RegularItem>(std::make_pair(orig, std::string())));
                    break;
                case TranslationNewItemPos::top:
                    sequence_.push_front(std::make_shared<RegularItem>(std::make_pair(orig, std::string()))); //put untranslated items to the front of the .lng filebreak;
                    break;
            }
    }

    void addItem(const SingularPluralPair& orig)
    {
        if (!pluralUnique_.insert(orig).second) return;
        auto it = transPluralOld_.find(orig);
        if (it != transPluralOld_.end() && !it->second.empty()) //preserve old translation from .lng file if existing
            sequence_.push_back(std::make_shared<PluralItem>(std::make_pair(orig, it->second)));
        else
            switch (newItemPos_)
            {
                case TranslationNewItemPos::rel:
                    sequence_.push_back(std::make_shared<PluralItem>(std::make_pair(orig, PluralForms())));
                    break;
                case TranslationNewItemPos::top:
                    sequence_.push_front(std::make_shared<PluralItem>(std::make_pair(orig, PluralForms()))); //put untranslated items to the front of the .lng file
                    break;
            }
    }

    bool untranslatedTextExists() const { return std::any_of(sequence_.begin(), sequence_.end(), [](const std::shared_ptr<Item>& item) { return !item->hasTranslation(); }); }

    template <class Function, class Function2>
    void visitItems(Function onTrans, Function2 onPluralTrans) const //onTrans takes (const TranslationMap::value_type&), onPluralTrans takes (const TranslationPluralMap::value_type&)
    {
        for (const std::shared_ptr<Item>& item : sequence_)
            if (auto regular = dynamic_cast<const RegularItem*>(item.get()))
                onTrans(regular->value);
            else if (auto plural = dynamic_cast<const PluralItem*>(item.get()))
                onPluralTrans(plural->value);
            else assert(false);
    }

private:
    struct Item { virtual ~Item() {} virtual bool hasTranslation() const = 0; };
    struct RegularItem : public Item { RegularItem(const TranslationMap      ::value_type& val) : value(val) {} bool hasTranslation() const override { return !value.second.empty(); } TranslationMap      ::value_type value; };
    struct PluralItem  : public Item { PluralItem (const TranslationPluralMap::value_type& val) : value(val) {} bool hasTranslation() const override { return !value.second.empty(); } TranslationPluralMap::value_type value; };

    const TranslationNewItemPos newItemPos_;
    zen::RingBuffer<std::shared_ptr<Item>> sequence_; //ordered list of translation elements

    std::unordered_set<TranslationMap      ::key_type> transUnique_;  //check uniqueness
    std::unordered_set<TranslationPluralMap::key_type> pluralUnique_; //

    const TranslationMap transOld_;             //reuse existing translation
    const TranslationPluralMap transPluralOld_; //
};


enum class TokenType
{
    //header information
    headerBegin,
    headerEnd,
    langNameBegin,
    langNameEnd,
    transNameBegin,
    transNameEnd,
    localeNameBegin,
    localeNameEnd,
    flagFileBegin,
    flagFileEnd,
    pluralCountBegin,
    pluralCountEnd,
    pluralDefBegin,
    pluralDefEnd,

    //item level
    srcBegin,
    srcEnd,
    trgBegin,
    trgEnd,
    text,
    pluralBegin,
    pluralEnd,
    end,
};

struct Token
{
    Token(TokenType t) : type(t) {}

    TokenType type;
    std::string text;
};


class KnownTokens
{
public:
    KnownTokens() {} //clang wants it, clang gets it

    using TokenMap = std::unordered_map<TokenType, std::string>;

    const TokenMap& getList() const { return tokens_; }

    std::string text(TokenType t) const
    {
        auto it = tokens_.find(t);
        if (it != tokens_.end())
            return it->second;
        assert(false);
        return std::string();
    }

private:
    const TokenMap tokens_ =
    {
        //header information
        {TokenType::headerBegin,      "<header>"},
        {TokenType::headerEnd,        "</header>"},
        {TokenType::langNameBegin,    "<language>"},
        {TokenType::langNameEnd,      "</language>"},
        {TokenType::transNameBegin,   "<translator>"},
        {TokenType::transNameEnd,     "</translator>"},
        {TokenType::localeNameBegin,  "<locale>"},
        {TokenType::localeNameEnd,    "</locale>"},
        {TokenType::flagFileBegin,    "<image>"},
        {TokenType::flagFileEnd,      "</image>"},
        {TokenType::pluralCountBegin, "<plural_count>"},
        {TokenType::pluralCountEnd,   "</plural_count>"},
        {TokenType::pluralDefBegin,   "<plural_definition>"},
        {TokenType::pluralDefEnd,     "</plural_definition>"},

        //item level
        {TokenType::srcBegin,    "<source>"},
        {TokenType::srcEnd,      "</source>"},
        {TokenType::trgBegin,    "<target>"},
        {TokenType::trgEnd,      "</target>"},
        {TokenType::pluralBegin, "<pluralform>"},
        {TokenType::pluralEnd,   "</pluralform>"},
    };
};


class Scanner
{
public:
    Scanner(const std::string& byteStream) : stream_(byteStream), pos_(stream_.begin())
    {
        if (zen::startsWith(stream_, zen::BYTE_ORDER_MARK_UTF8))
            pos_ += zen::strLength(zen::BYTE_ORDER_MARK_UTF8);
    }

    Token getNextToken()
    {
        //skip whitespace
        pos_ = std::find_if_not(pos_, stream_.end(), zen::isWhiteSpace<char>);

        if (pos_ == stream_.end())
            return Token(TokenType::end);

        for (const auto& [tokenEnum, tokenString] : tokens_.getList())
            if (startsWith(tokenString))
            {
                pos_ += tokenString.size();
                return Token(tokenEnum);
            }

        //rest must be "text"
        auto itBegin = pos_;
        while (pos_ != stream_.end() && !startsWithKnownTag())
            pos_ = std::find(pos_ + 1, stream_.end(), '<');

        std::string text(itBegin, pos_);

        normalize(text); //remove whitespace from end etc.

        if (text.empty() && pos_ == stream_.end())
            return Token(TokenType::end);

        Token out(TokenType::text);
        out.text = std::move(text);
        return out;
    }

    size_t posRow() const //current row beginning with 0
    {
        //count line endings
        const size_t crSum = std::count(stream_.begin(), pos_, '\r'); //carriage returns
        const size_t nlSum = std::count(stream_.begin(), pos_, '\n'); //new lines
        assert(crSum == 0 || nlSum == 0 || crSum == nlSum);
        return std::max(crSum, nlSum); //be compatible with Linux/Mac/Win
    }

    size_t posCol() const //current col beginning with 0
    {
        //seek beginning of line
        for (auto it = pos_; it != stream_.begin(); )
        {
            --it;
            if (zen::isLineBreak(*it))
                return pos_ - it - 1;
        }
        return pos_ - stream_.begin();
    }

private:
    bool startsWithKnownTag() const
    {
        return std::any_of(tokens_.getList().begin(), tokens_.getList().end(),
        [&](const KnownTokens::TokenMap::value_type& p) { return startsWith(p.second); });
    }

    bool startsWith(const std::string& prefix) const
    {
        return zen::startsWith(zen::makeStringView(pos_, stream_.end()), prefix);
    }

    static void normalize(std::string& text)
    {
        zen::trim(text); //remove whitespace from both ends

        //Delimiter:
        //----------
        //Linux: 0xA        \n
        //Mac:   0xD        \r
        //Win:   0xD 0xA    \r\n    <- language files are in Windows format
        zen::replace(text, "\r\n", '\n'); //
        zen::replace(text, '\r',   '\n'); //ensure c-style line breaks
    }

    const std::string stream_;
    std::string::const_iterator pos_;
    const KnownTokens tokens_; //no need for static non-POD!
};


class LngParser
{
public:
    explicit LngParser(const std::string& fileStream) : scn_(fileStream), tk_(scn_.getNextToken()) {}

    void parse(TranslationMap& out, TranslationPluralMap& pluralOut, TransHeader& header)
    {
        parseHeader(header);

        try
        {
            plural::PluralFormInfo pi(header.pluralDefinition, header.pluralCount);

            //items
            while (token().type != TokenType::end)
                parseRegular(out, pluralOut, pi);
        }
        catch (const plural::InvalidPluralForm&)
        {
            throw ParsingError({L"Invalid plural form definition", scn_.posRow(), scn_.posCol()});
        }
    }

    void parseHeader(TransHeader& header)
    {
        consumeToken(TokenType::headerBegin); //throw ParsingError

        consumeToken(TokenType::langNameBegin); //throw ParsingError
        header.languageName = token().text;
        consumeToken(TokenType::text);        //throw ParsingError
        consumeToken(TokenType::langNameEnd); //

        consumeToken(TokenType::transNameBegin); //throw ParsingError
        header.translatorName = token().text;
        consumeToken(TokenType::text);         //throw ParsingError
        consumeToken(TokenType::transNameEnd); //

        consumeToken(TokenType::localeNameBegin); //throw ParsingError
        header.localeName = token().text;
        consumeToken(TokenType::text);          //throw ParsingError
        consumeToken(TokenType::localeNameEnd); //

        consumeToken(TokenType::flagFileBegin); //throw ParsingError
        header.flagFile = token().text;
        consumeToken(TokenType::text);        //throw ParsingError
        consumeToken(TokenType::flagFileEnd); //

        consumeToken(TokenType::pluralCountBegin); //throw ParsingError
        header.pluralCount = zen::stringTo<int>(token().text);
        consumeToken(TokenType::text);           //throw ParsingError
        consumeToken(TokenType::pluralCountEnd); //

        consumeToken(TokenType::pluralDefBegin); //throw ParsingError
        header.pluralDefinition = token().text;
        consumeToken(TokenType::text);         //throw ParsingError
        consumeToken(TokenType::pluralDefEnd); //

        consumeToken(TokenType::headerEnd); //throw ParsingError
    }

private:
    void parseRegular(TranslationMap& out, TranslationPluralMap& pluralOut, const plural::PluralFormInfo& pluralInfo)
    {
        consumeToken(TokenType::srcBegin); //throw ParsingError

        if (token().type == TokenType::pluralBegin)
            return parsePlural(pluralOut, pluralInfo);

        std::string original = token().text;
        consumeToken(TokenType::text);   //throw ParsingError
        consumeToken(TokenType::srcEnd); //

        consumeToken(TokenType::trgBegin); //throw ParsingError
        std::string translation;
        if (token().type == TokenType::text)
        {
            translation = token().text;
            nextToken();
        }
        validateTranslation(original, translation); //throw ParsingError
        consumeToken(TokenType::trgEnd);            //

        out.emplace(original, translation);
    }

    void parsePlural(TranslationPluralMap& pluralOut, const plural::PluralFormInfo& pluralInfo)
    {
        //TokenType::srcBegin already consumed

        consumeToken(TokenType::pluralBegin); //throw ParsingError
        std::string engSingular = token().text;
        consumeToken(TokenType::text);      //throw ParsingError
        consumeToken(TokenType::pluralEnd); //

        consumeToken(TokenType::pluralBegin); //throw ParsingError
        std::string engPlural = token().text;
        consumeToken(TokenType::text);      //throw ParsingError
        consumeToken(TokenType::pluralEnd); //

        consumeToken(TokenType::srcEnd); //throw ParsingError
        const SingularPluralPair original(engSingular, engPlural);

        consumeToken(TokenType::trgBegin); //throw ParsingError

        PluralForms pluralList;
        while (token().type == TokenType::pluralBegin)
        {
            consumeToken(TokenType::pluralBegin); //throw ParsingError
            std::string pluralForm = token().text;
            consumeToken(TokenType::text);      //throw ParsingError
            consumeToken(TokenType::pluralEnd); //
            pluralList.push_back(pluralForm);
        }
        validateTranslation(original, pluralList, pluralInfo);
        consumeToken(TokenType::trgEnd); //throw ParsingError

        pluralOut.emplace(original, pluralList);
    }

    void validateTranslation(const std::string& original, const std::string& translation) //throw ParsingError
    {
        using namespace zen;

        if (original.empty())
            throw ParsingError({L"Translation source text is empty", scn_.posRow(), scn_.posCol()});

        if (!isValidUtf(original))
            throw ParsingError({L"Translation source text contains UTF-8 encoding error", scn_.posRow(), scn_.posCol()});
        if (!isValidUtf(translation))
            throw ParsingError({L"Translation text contains UTF-8 encoding error", scn_.posRow(), scn_.posCol()});

        if (!translation.empty())
        {
            //if original contains placeholder, so must translation!
            auto checkPlaceholder = [&](const std::string& placeholder)
            {
                if (contains(original, placeholder) &&
                    !contains(translation, placeholder))
                    throw ParsingError({replaceCpy<std::wstring>(L"Placeholder %x missing in translation", L"%x", utfTo<std::wstring>(placeholder)), scn_.posRow(), scn_.posCol()});
            };
            checkPlaceholder("%x");
            checkPlaceholder("%y");
            checkPlaceholder("%z");

            //if source is a one-liner, so should be the translation
            if (!contains(original, '\n') && contains(translation, '\n'))
                throw ParsingError({L"Source text is a one-liner, but translation consists of multiple lines", scn_.posRow(), scn_.posCol()});

            //if source contains ampersand to mark menu accellerator key, so must translation
            const size_t ampCount = ampersandTokenCount(original);
            if (ampCount > 1 || ampCount != ampersandTokenCount(translation))
                throw ParsingError({L"Source and translation both need exactly one & character to mark a menu item access key or none at all", scn_.posRow(), scn_.posCol()});

            //ampersand at the end makes buggy wxWidgets crash miserably
            if (endsWithSingleAmp(original) || endsWithSingleAmp(translation))
                throw ParsingError({L"The & character to mark a menu item access key must not occur at the end of a string", scn_.posRow(), scn_.posCol()});

            //if source ends with colon, so must translation (note: character seems to be universally used, even for asian and arabic languages)
            if (endsWith(original, ':') && !endsWithColon(translation))
                throw ParsingError({L"Source text ends with a colon character \":\", but translation does not", scn_.posRow(), scn_.posCol()});

            //if source ends with a period, so must translation (note: character seems to be universally used, even for asian and arabic languages)
            if (endsWithSingleDot(original) && !endsWithSingleDot(translation))
                throw ParsingError({L"Source text ends with a punctuation mark character \".\", but translation does not", scn_.posRow(), scn_.posCol()});

            //if source ends with an ellipsis, so must translation (note: character seems to be universally used, even for asian and arabic languages)
            if (endsWithEllipsis(original) && !endsWithEllipsis(translation))
                throw ParsingError({L"Source text ends with an ellipsis \"...\", but translation does not", scn_.posRow(), scn_.posCol()});

            //check for not-to-be-translated texts
            for (const char* fixedStr : {"FreeFileSync", "RealTimeSync", "ffs_gui", "ffs_batch", "ffs_tmp", "GlobalSettings.xml"})
                if (contains(original, fixedStr) && !contains(translation, fixedStr))
                    throw ParsingError({replaceCpy<std::wstring>(L"Misspelled \"%x\" in translation", L"%x", utfTo<std::wstring>(fixedStr)), scn_.posRow(), scn_.posCol()});

            //some languages (French!) put a space before punctuation mark => must be a no-brake space!
            for (const char punctChar : std::string(".!?:;$#"))
                if (contains(original,    std::string(" ") + punctChar) ||
                    contains(translation, std::string(" ") + punctChar))
                    throw ParsingError({replaceCpy<std::wstring>(L"Text contains a space before the \"%x\" character. Are line-breaks really allowed here?"
                                                                 " Maybe this should be a \"non-breaking space\" (Windows: Alt 0160    UTF8: 0xC2 0xA0)?",
                                                                 L"%x", utfTo<std::wstring>(punctChar)), scn_.posRow(), scn_.posCol()});
        }
    }

    void validateTranslation(const SingularPluralPair& original, const PluralForms& translation, const plural::PluralFormInfo& pluralInfo) //throw ParsingError
    {
        using namespace zen;

        if (original.first.empty() || original.second.empty())
            throw ParsingError({L"Translation source text is empty", scn_.posRow(), scn_.posCol()});

        const std::vector<std::string> allTexts = [&]
        {
            std::vector<std::string> at{original.first, original.second};
            at.insert(at.end(), translation.begin(), translation.end());
            return at;
        }();

        for (const std::string& str : allTexts)
            if (!isValidUtf(str))
                throw ParsingError({L"Text contains UTF-8 encoding error", scn_.posRow(), scn_.posCol()});

        //check the primary placeholder is existing at least for the second english text
        if (!contains(original.second, "%x"))
            throw ParsingError({L"Plural form source text does not contain %x placeholder", scn_.posRow(), scn_.posCol()});

        if (!translation.empty())
        {
            //check for invalid number of plural forms
            if (pluralInfo.getCount() != translation.size())
                throw ParsingError({replaceCpy(replaceCpy<std::wstring>(L"Invalid number of plural forms; actual: %x, expected: %y",
                                                                        L"%x", formatNumber(translation.size())),
                                               L"%y", formatNumber(pluralInfo.getCount())), scn_.posRow(), scn_.posCol()});

            //check for duplicate plural form translations (catch copy & paste errors for single-number form translations)
            for (auto it = translation.begin(); it != translation.end(); ++it)
                if (!contains(*it, "%x"))
                {
                    auto it2 = std::find(it + 1, translation.end(), *it);
                    if (it2 != translation.end())
                        throw ParsingError({replaceCpy<std::wstring>(L"Duplicate plural form translation at index position %x",
                                                                     L"%x", formatNumber(it2 - translation.begin())), scn_.posRow(), scn_.posCol()});
                }

            for (size_t pos = 0; pos < translation.size(); ++pos)
                if (pluralInfo.isSingleNumberForm(pos))
                {
                    //translation needs to use decimal number if english source does so (e.g. frequently changing text like statistics)
                    if (contains(original.first, "%x") ||
                        contains(original.first, "1"))
                    {
                        const int firstNumber = pluralInfo.getFirstNumber(pos);
                        if (!contains(translation[pos], "%x") &&
                            !contains(translation[pos], numberTo<std::string>(firstNumber)))
                            throw ParsingError({replaceCpy<std::wstring>(replaceCpy<std::wstring>(L"Plural form translation at index position %y needs to use the decimal number %z or the %x placeholder",
                                                                                                  L"%y", formatNumber(pos)), L"%z", formatNumber(firstNumber)), scn_.posRow(), scn_.posCol()});
                    }
                }
                else
                {
                    //ensure the placeholder is used when needed
                    if (!contains(translation[pos], "%x"))
                        throw ParsingError({replaceCpy<std::wstring>(L"Plural form at index position %y is missing the %x placeholder", L"%y", formatNumber(pos)), scn_.posRow(), scn_.posCol()});
                }

            auto checkSecondaryPlaceholder = [&](const std::string& placeholder)
            {
                //make sure secondary placeholder is used for both source texts (or none) and all plural forms
                if (contains(original.first,  placeholder) ||
                    contains(original.second, placeholder))
                    for (const std::string& str : allTexts)
                        if (!contains(str, placeholder))
                            throw ParsingError({zen::replaceCpy<std::wstring>(L"Placeholder %x missing in text", L"%x", zen::utfTo<std::wstring>(placeholder)), scn_.posRow(), scn_.posCol()});
            };
            checkSecondaryPlaceholder("%y");
            checkSecondaryPlaceholder("%z");

            //if source is a one-liner, so should be the translation
            if (!contains(original.first, '\n') && !contains(original.second, '\n') &&
            /**/std::any_of(translation.begin(), translation.end(), [](const std::string& pform) { return contains(pform, '\n'); }))
            /**/throw ParsingError({L"Source text is a one-liner, but at least one plural form translation consists of multiple lines", scn_.posRow(), scn_.posCol()});

            //if source contains ampersand to mark menu accellerator key, so must translation
            const size_t ampCount = ampersandTokenCount(original.first);
            for (const std::string& str : allTexts)
                if (ampCount > 1 || ampersandTokenCount(str) != ampCount)
                    throw ParsingError({L"Source and translation both need exactly one & character to mark a menu item access key or none at all", scn_.posRow(), scn_.posCol()});

            //ampersand at the end makes buggy wxWidgets crash miserably
            for (const std::string& str : allTexts)
                if (endsWithSingleAmp(str))
                    throw ParsingError({L"The & character to mark a menu item access key must not occur at the end of a string", scn_.posRow(), scn_.posCol()});

            //if source ends with colon, so must translation (note: character seems to be universally used, even for asian and arabic languages)
            if (endsWith(original.first, ':') || endsWith(original.second, ':'))
                for (const std::string& str : allTexts)
                    if (!endsWithColon(str))
                        throw ParsingError({L"Source text ends with a colon character \":\", but translation does not", scn_.posRow(), scn_.posCol()});

            //if source ends with a period, so must translation (note: character seems to be universally used, even for asian and arabic languages)
            if (endsWithSingleDot(original.first) || endsWithSingleDot(original.second))
                for (const std::string& str : allTexts)
                    if (!endsWithSingleDot(str))
                        throw ParsingError({L"Source text ends with a punctuation mark character \".\", but translation does not", scn_.posRow(), scn_.posCol()});

            //if source ends with an ellipsis, so must translation (note: character seems to be universally used, even for asian and arabic languages)
            if (endsWithEllipsis(original.first) || endsWithEllipsis(original.second))
                for (const std::string& str : allTexts)
                    if (!endsWithEllipsis(str))
                        throw ParsingError({L"Source text ends with an ellipsis \"...\", but translation does not", scn_.posRow(), scn_.posCol()});

            //check for not-to-be-translated texts
            for (const char* fixedStr : {"FreeFileSync", "RealTimeSync", "ffs_gui", "ffs_batch", "ffs_tmp", "GlobalSettings.xml"})
                if (contains(original.first, fixedStr) || contains(original.second, fixedStr))
                    for (const std::string& str : allTexts)
                        if (!contains(str, fixedStr))
                            throw ParsingError({replaceCpy<std::wstring>(L"Misspelled \"%x\" in translation", L"%x", utfTo<std::wstring>(fixedStr)), scn_.posRow(), scn_.posCol()});

            //some languages (French!) put a space before punctuation mark => must be a no-brake space!
            for (const char punctChar : std::string(".!?:;$#"))
                for (const std::string& str : allTexts)
                    if (contains(str, std::string(" ") + punctChar))
                        throw ParsingError({replaceCpy<std::wstring>(L"Text contains a space before the \"%x\" character. Are line-breaks really allowed here?"
                                                                     " Maybe this should be a \"non-breaking space\" (Windows: Alt 0160    UTF8: 0xC2 0xA0)?",
                                                                     L"%x", utfTo<std::wstring>(punctChar)), scn_.posRow(), scn_.posCol()});
        }
    }

    //helper
    static size_t ampersandTokenCount(const std::string& str)
    {
        using namespace zen;
        const std::string tmp = replaceCpy(str, "&&", ""); //make sure to not catch && which windows resolves as just one & for display!
        return std::count(tmp.begin(), tmp.end(), '&');
    }

    static bool endsWithSingleAmp(const std::string& s)
    {
        using namespace zen;
        return endsWith(s, "&") && !endsWith(s, "&&");
    }

    static bool endsWithEllipsis(const std::string& s)
    {
        using namespace zen;
        return endsWith(s, "...") ||
               endsWith(s, "\xe2\x80\xa6"); //narrow ellipsis (spanish?)
    }

    static bool endsWithColon(const std::string& s)
    {
        using namespace zen;
        return endsWith(s, ':') ||
               endsWith(s, "\xef\xbc\x9a"); //chinese colon
    }

    static bool endsWithSingleDot(const std::string& s)
    {
        using namespace zen;
        return (endsWith(s, ".")            ||
                endsWith(s, "\xe0\xa5\xa4") || //hindi period
                endsWith(s, "\xe3\x80\x82")) //chinese period
               &&
               (!endsWith(s, "..")                      &&
                !endsWith(s, "\xe0\xa5\xa4\xe0\xa5\xa4") && //hindi period
                !endsWith(s, "\xe3\x80\x82\xe3\x80\x82")); //chinese period
    }


    const Token& token() const { return tk_; }

    void nextToken() { tk_ = scn_.getNextToken(); }

    void expectToken(TokenType t) //throw ParsingError
    {
        if (token().type != t)
            throw ParsingError({L"Unexpected token", scn_.posRow(), scn_.posCol()});
    }

    void consumeToken(TokenType t) //throw ParsingError
    {
        expectToken(t); //throw ParsingError
        nextToken();
    }

    Scanner scn_;
    Token tk_;
};


inline
void parseLng(const std::string& fileStream, TransHeader& header, TranslationMap& out, TranslationPluralMap& pluralOut) //throw ParsingError
{
    out.clear();
    pluralOut.clear();

    LngParser(fileStream).parse(out, pluralOut, header);
}


inline
TransHeader parseHeader(const std::string& fileStream) //throw ParsingError
{
    TransHeader header;
    LngParser(fileStream).parseHeader(header);
    return header;
}


inline
void formatMultiLineText(std::string& text)
{
    assert(!zen::contains(text, "\r\n"));

    if (zen::contains(text, '\n')) //multiple lines
    {
        if (*text.begin() != '\n')
            text = '\n' + text;
        if (*text.rbegin() != '\n')
            text += '\n';
    }
}


inline
std::string generateLng(const TranslationUnorderedList& in, const TransHeader& header)
{
    const KnownTokens tokens; //no need for static non-POD!

    std::string out;
    //header
    out += tokens.text(TokenType::headerBegin) + '\n';

    out += '\t' + tokens.text(TokenType::langNameBegin);
    out += header.languageName;
    out += tokens.text(TokenType::langNameEnd) + '\n';

    out += '\t' + tokens.text(TokenType::transNameBegin);
    out += header.translatorName;
    out += tokens.text(TokenType::transNameEnd) + '\n';

    out += '\t' + tokens.text(TokenType::localeNameBegin);
    out += header.localeName;
    out += tokens.text(TokenType::localeNameEnd) + '\n';

    out += '\t' + tokens.text(TokenType::flagFileBegin);
    out += header.flagFile;
    out += tokens.text(TokenType::flagFileEnd) + '\n';

    out += '\t' + tokens.text(TokenType::pluralCountBegin);
    out += zen::numberTo<std::string>(header.pluralCount);
    out += tokens.text(TokenType::pluralCountEnd) + '\n';

    out += '\t' + tokens.text(TokenType::pluralDefBegin);
    out += header.pluralDefinition;
    out += tokens.text(TokenType::pluralDefEnd) + '\n';

    out += tokens.text(TokenType::headerEnd) + '\n';

    out += '\n';


    in.visitItems([&](const TranslationMap::value_type& trans)
    {
        std::string original    = trans.first;
        std::string translation = trans.second;

        formatMultiLineText(original);
        formatMultiLineText(translation);

        out += tokens.text(TokenType::srcBegin);
        out += original;
        out += tokens.text(TokenType::srcEnd) + '\n';

        out += tokens.text(TokenType::trgBegin);
        out += translation;
        out += tokens.text(TokenType::trgEnd) + '\n' + '\n';
    },
    [&](const TranslationPluralMap::value_type& transPlural)
    {
        std::string engSingular  = transPlural.first.first;
        std::string engPlural    = transPlural.first.second;
        const PluralForms& forms = transPlural.second;

        formatMultiLineText(engSingular);
        formatMultiLineText(engPlural);

        out += tokens.text(TokenType::srcBegin) + '\n';
        out += tokens.text(TokenType::pluralBegin);
        out += engSingular;
        out += tokens.text(TokenType::pluralEnd) + '\n';
        out += tokens.text(TokenType::pluralBegin);
        out += engPlural;
        out += tokens.text(TokenType::pluralEnd) + '\n';
        out += tokens.text(TokenType::srcEnd) + '\n';

        out += tokens.text(TokenType::trgBegin);
        if (!forms.empty()) //translators will be searching for "<target></target>"
            out += '\n';
        for (std::string plForm : forms)
        {
            formatMultiLineText(plForm);

            out += tokens.text(TokenType::pluralBegin);
            out += plForm;
            out += tokens.text(TokenType::pluralEnd) + '\n';
        }
        out += tokens.text(TokenType::trgEnd) + '\n' + '\n';
    });

    assert(!zen::contains(out, "\r\n") && !zen::contains(out, "\r"));
    return zen::replaceCpy(out, '\n', "\r\n"); //back to win line endings
}
}

#endif //PARSE_LNG_H_46794693622675638
