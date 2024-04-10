// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef I18_N_H_3843489325044253425456
#define I18_N_H_3843489325044253425456

#include "globals.h"
#include "string_tools.h"
#include "format_unit.h"


//minimal layer enabling text translation - without platform/library dependencies!

#define ZEN_TRANS_CONCAT_SUB(X, Y) X ## Y
#define _(s)        zen::translate(ZEN_TRANS_CONCAT_SUB(L, s))
#define _P(s, p, n) zen::translate(ZEN_TRANS_CONCAT_SUB(L, s), ZEN_TRANS_CONCAT_SUB(L, p), n)
//source and translation are required to use %x as number placeholder
//for plural form, which will be substituted automatically!!!

    static_assert(WXINTL_NO_GETTEXT_MACRO, "...must be defined to deactivate wxWidgets underscore macro");

namespace zen
{
//implement handler to enable program-wide localizations:
struct TranslationHandler
{
    //THREAD-SAFETY: "const" member must model thread-safe access!
    TranslationHandler() {}
    virtual ~TranslationHandler() {}

    //C++11: std::wstring should be thread-safe like an int
    virtual std::wstring translate(const std::wstring& text) const = 0; //simple translation
    virtual std::wstring translate(const std::wstring& singular, const std::wstring& plural, int64_t n) const = 0;

    virtual bool layoutIsRtl() const = 0; //right-to-left? e.g. Hebrew, Arabic

private:
    TranslationHandler           (const TranslationHandler&) = delete;
    TranslationHandler& operator=(const TranslationHandler&) = delete;
};

void setTranslator(std::unique_ptr<const TranslationHandler>&& newHandler); //take ownership
std::shared_ptr<const TranslationHandler> getTranslator();








//######################## implementation ##############################
namespace impl
{
//getTranslator() may be called even after static objects of this translation unit are destroyed!
inline constinit Global<const TranslationHandler> globalTranslationHandler;
}

inline
std::shared_ptr<const TranslationHandler> getTranslator()
{
    return impl::globalTranslationHandler.get();
}


inline
void setTranslator(std::unique_ptr<const TranslationHandler>&& newHandler)
{
    impl::globalTranslationHandler.set(std::move(newHandler));
}


inline
std::wstring translate(const std::wstring& text)
{
    if (std::shared_ptr<const TranslationHandler> t = getTranslator()) //std::shared_ptr => temporarily take (shared) ownership while using the interface!
        return t->translate(text);
    return text;
}


//translate plural forms: "%x day" "%x days"
//returns "1 day" if n == 1; "123 days" if n == 123 for english language
template <class T> inline
std::wstring translate(const std::wstring& singular, const std::wstring& plural, T n)
{
    static_assert(sizeof(n) <= sizeof(int64_t));
    const auto n64 = static_cast<int64_t>(n);

    assert(contains(plural, L"%x"));

    if (std::shared_ptr<const TranslationHandler> t = getTranslator())
    {
        std::wstring translation = t->translate(singular, plural, n64);
        assert(!contains(translation, L"%x"));
        return translation;
    }
    //fallback:
    return replaceCpy(std::abs(n64) == 1 ? singular : plural, L"%x", formatNumber(n));
}


inline
bool languageLayoutIsRtl()
{
    if (std::shared_ptr<const TranslationHandler> t = getTranslator())
        return t->layoutIsRtl();
    return false;
}
}

#endif //I18_N_H_3843489325044253425456
