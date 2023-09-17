// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "multi_rename.h"
#include <zen/string_tools.h>

using namespace zen;
using namespace fff;


namespace
{
std::wstring_view findLongestSubstring(const std::vector<std::wstring_view>& strings)
{
    if (strings.empty())
        return {};

    const std::wstring_view strMin = *std::min_element(strings.begin(), strings.end(),
    /**/[](const std::wstring_view lhs, const std::wstring_view rhs) { return lhs.size() < rhs.size(); });

    for (size_t sz = strMin.size(); sz > 0; --sz) //iterate over size, descending
        for (size_t i = 0; i + sz <= strMin.size(); ++i)
        {
            const std::wstring_view substr(strMin.data() + i, sz);
            //perf: duplicate substrings, especially für size = 1?

            const bool isCommon = [&]
            {
                for (const std::wstring_view str : strings)
                    if (str.data() != strMin.data()) //sufficient check: an extension of strMin wouldn't prune anyway
                        if (!contains(str, substr))
                            return false;
                return true;
            }();

            if (isCommon)
                return substr; //*first* occuring substring of maximum size
        }

    return {};
}


struct StringPart
{
    std::vector<std::wstring_view> diff; //may be empty, but only at beginning
    std::wstring_view common;            //may be empty, but only at end
};

std::vector<StringPart> getStringParts(std::vector<std::wstring_view>&& strings)
{
    std::wstring_view substr = findLongestSubstring(strings);
    if (!substr.empty())
    {
        std::vector<std::wstring_view> head;
        std::vector<std::wstring_view> tail;

        for (const std::wstring_view str : strings)
        {
            head.push_back(beforeFirst(str, substr, IfNotFoundReturn::none));
            tail.push_back(afterFirst (str, substr, IfNotFoundReturn::none));
        }

        std::vector<StringPart> np = getStringParts(std::move(head));
        assert(np.empty() || np.back().common.empty()); //otherwise we could construct an even longer substring!

        if (np.empty())
            np.push_back({{}, substr});
        else
            np.back().common = substr;

        const std::vector<StringPart> npTail = getStringParts(std::move(tail));
        assert(npTail.empty() || !npTail.front().diff.empty()); //otherwise we could construct an even longer substring!

        append(np, npTail);
        return np;
    }
    else
    {
        if (std::all_of(strings.begin(), strings.end(), [](const std::wstring_view str) { return str.empty(); }))
        /**/return {};

        return {{std::move(strings), {}}};
    }
}


constexpr wchar_t placeholders[] = //http://xahlee.info/comp/unicode_circled_numbers.html
{
    //L'\u24FF', //⓿ <- rendered bigger than the rest (same for ⓫) on Centos Linux
    L'\u2776', //❶
    L'\u2777', //❷
    L'\u2778', //❸
    L'\u2779', //❹
    L'\u277A', //❺
    L'\u277B', //❻
    L'\u277C', //❼
    L'\u277D', //❽
    L'\u277E', //❾
    L'\u277F', //❿ -> last one is special: represents "all the rest"
};


inline
size_t getPlaceholderIndex(wchar_t c)
{
    static_assert(std::size(placeholders) == 10);
    if (placeholders[0] <= c && c <= placeholders[9])
        return static_cast<size_t>(c - placeholders[0]);

    return static_cast<size_t>(-1);
}
}


bool fff::isRenamePlaceholderChar(wchar_t c) { return getPlaceholderIndex(c) < std::size(placeholders); }


struct fff::RenameBuf
{
    explicit RenameBuf(const std::vector<std::wstring>& s) : strings(s) {}

    std::vector<std::wstring> strings;
    std::vector<StringPart> parts = getStringParts({strings.begin(), strings.end()});
};


//e.g. "Season ❶, Episode ❷ - ❸.avi"
std::pair<std::wstring /*phrase*/, SharedRef<const RenameBuf>> fff::getPlaceholderPhrase(const std::vector<std::wstring>& strings)
{
    auto renameBuf = makeSharedRef<const RenameBuf>(strings);

    std::wstring phrase;
    size_t placeIdx = 0;

    for (const StringPart& p : renameBuf.ref().parts)
    {
        if (!p.diff.empty())
        {
            phrase += placeholders[placeIdx++];

            if (placeIdx >= std::size(placeholders))
                break; //represent "all the rest" with last placeholder
        }
        phrase += p.common; //TODO: what if common part already contains placeholder character!?
    }
    return {phrase, renameBuf};
}


const std::vector<std::wstring> fff::resolvePlaceholderPhrase(const std::wstring_view phrase, const RenameBuf& buf)
{
    std::vector<std::vector<std::wstring_view>> diffByIdx;

    for (const StringPart& p : buf.parts)
        if (!p.diff.empty())
            diffByIdx.push_back(std::move(p.diff)), assert(diffByIdx.back().size() == buf.strings.size());

    std::vector<std::wstring> output;

    for (size_t i = 0; i < buf.strings.size(); ++i)
    {
        std::wstring resolved;

        for (const wchar_t c : phrase)
            if (const size_t placeIdx = getPlaceholderIndex(c);
                placeIdx < diffByIdx.size())
            {
                if (placeIdx == std::size(placeholders) - 1) //last placeholder represents "all the rest"
                    resolved.append(diffByIdx[placeIdx][i].data(), buf.strings[i].data() + buf.strings[i].size());
                else
                    resolved += diffByIdx[placeIdx][i];
            }
            else
                resolved += c;

        output.push_back(std::move(resolved));
    }

    return output;
}
