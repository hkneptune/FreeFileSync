// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef HARD_FILTER_H_825780275842758345
#define HARD_FILTER_H_825780275842758345

//#include <vector>
//#include <memory>
#include <unordered_set>
#include <zen/zstring.h>


namespace fff
{
/*  Semantics of PathFilter:
    1. using it creates a NEW folder hierarchy! -> must be considered by <Two way> variant!
    2. it applies equally to both sides => it always matches either both sides or none! => can be used while traversing a single folder!

                  PathFilter (interface)
                      /|\
           ____________|_____________
          |            |             |
    NullFilter    NameFilter  CombinedFilter                         */

class PathFilter;
using FilterRef = zen::SharedRef<const PathFilter>;

std::strong_ordering operator<=>(const FilterRef& lhs, const FilterRef& rhs); //fix GCC warning: "... has not been declared within ?fff

const Zchar FILTER_ITEM_SEPARATOR = Zstr('|');

class PathFilter
{
public:
    virtual ~PathFilter() {}

    virtual bool passFileFilter(const Zstring& relFilePath) const = 0;
    virtual bool passDirFilter (const Zstring& relDirPath, bool* childItemMightMatch) const = 0;
    //childItemMightMatch: file/dir in subdirectories could(!) match
    //note: this hint is only set if passDirFilter returns false!

    virtual bool isNull() const = 0; //filter is equivalent to NullFilter

    virtual FilterRef copyFilterAddingExclusion(const Zstring& excludePhrase) const = 0;

private:
    friend std::strong_ordering operator<=>(const FilterRef& lhs, const FilterRef& rhs);

    virtual std::strong_ordering compareSameType(const PathFilter& other) const = 0; //assumes typeid(*this) == typeid(other)!
};


//small helper method: merge two hard filters (thereby remove Null-filters)
FilterRef combineFilters(const FilterRef& first, const FilterRef& second);


class NullFilter : public PathFilter //no filtering at all
{
public:
    bool passFileFilter(const Zstring& relFilePath) const override { return true; }
    bool passDirFilter(const Zstring& relDirPath, bool* childItemMightMatch) const override;
    bool isNull() const override { return true; }
    FilterRef copyFilterAddingExclusion(const Zstring& excludePhrase) const override;

private:
    std::strong_ordering compareSameType(const PathFilter& other) const override { assert(typeid(*this) == typeid(other)); return std::strong_ordering::equal; }
};


class NameFilter : public PathFilter //filter by base-relative file path
{
public:
    NameFilter(const Zstring& includePhrase, const Zstring& excludePhrase);

    void addExclusion(const Zstring& excludePhrase);

    bool passFileFilter(const Zstring& relFilePath) const override;
    bool passDirFilter(const Zstring& relDirPath, bool* childItemMightMatch) const override;

    bool isNull() const override;
    static bool isNull(const Zstring& includePhrase, const Zstring& excludePhrase); //*fast* check without expensive NameFilter construction!
    FilterRef copyFilterAddingExclusion(const Zstring& excludePhrase) const override;

private:
    friend class CombinedFilter;
    std::strong_ordering compareSameType(const PathFilter& other) const override;

    class MaskMatcher
    {
    public:
        void insert(const Zstring& mask); //expected: upper-case + Unicode-normalized!
        bool matches(const ZstringView relPath) const;
        bool matchesBegin(const ZstringView relPath) const;

        inline friend std::strong_ordering operator<=>(const MaskMatcher& lhs, const MaskMatcher& rhs)
        {
            return std::tie(lhs.realMasks_, lhs.relPathsCmp_) <=>
                   std::tie(rhs.realMasks_, rhs.relPathsCmp_);
        }
        //can't "= default" because std::unordered_set doesn't support <=>!
        //CAVEAT: when operator<=> is not "default" we also don't get operator== for free! declare manually:
        bool operator==(const MaskMatcher&) const;
        //why declare, but not define? if undeclared, "std::tie <=> std::tie" incorrectly deduces std::weak_ordering
        //=> bug? no, looks like "C++ standard nonsense": https://cplusplus.github.io/LWG/issue3431
        //std::three_way_comparable requires __WeaklyEqualityComparableWith!! this is stupid on first sight. And on second. And on third.

    private:
        std::set<Zstring> realMasks_; //always containing ? or *       (use std::set<> to scrap duplicates!)
        std::unordered_set<Zstring, zen::StringHash, zen::StringEqual> relPaths_; //never containing ? or *
        std::set<Zstring>                                              relPathsCmp_; //req. for operator<=> only :(
    };

    struct FilterSet
    {
        MaskMatcher fileMasks;
        MaskMatcher folderMasks;

        std::strong_ordering operator<=>(const FilterSet&) const = default;
    };

    static void parseFilterPhrase(const Zstring& filterPhrase, FilterSet& filter);

    FilterSet includeFilter;
    FilterSet excludeFilter;
};


class CombinedFilter : public PathFilter //combine two filters to match if and only if both match
{
public:
    CombinedFilter(const NameFilter& first, const NameFilter& second) : first_(first), second_(second) { assert(!first.isNull() && !second.isNull()); } //if either is null, then wy use CombinedFilter?

    bool passFileFilter(const Zstring& relFilePath) const override;
    bool passDirFilter(const Zstring& relDirPath, bool* childItemMightMatch) const override;
    bool isNull() const override;
    FilterRef copyFilterAddingExclusion(const Zstring& excludePhrase) const override;

private:
    std::strong_ordering compareSameType(const PathFilter& other) const override;

    const NameFilter first_;
    const NameFilter second_;
};






//--------------- inline implementation ---------------------------------------
inline
bool NullFilter::passDirFilter(const Zstring& relDirPath, bool* childItemMightMatch) const
{
    assert(!childItemMightMatch || *childItemMightMatch); //check correct usage
    return true;
}


inline
FilterRef NullFilter::copyFilterAddingExclusion(const Zstring& excludePhrase) const
{
    auto filter = zen::makeSharedRef<NameFilter>(Zstr("*"), excludePhrase);
    if (filter.ref().isNull())
        return zen::makeSharedRef<const NullFilter>();
    return filter;
}


inline
FilterRef NameFilter::copyFilterAddingExclusion(const Zstring& excludePhrase) const
{
    auto tmp = zen::makeSharedRef<NameFilter>(*this);
    tmp.ref().addExclusion(excludePhrase);
    return tmp;
}


inline
bool CombinedFilter::passFileFilter(const Zstring& relFilePath) const
{
    return first_ .passFileFilter(relFilePath) && //short-circuit behavior
           second_.passFileFilter(relFilePath);
}


inline
bool CombinedFilter::passDirFilter(const Zstring& relDirPath, bool* childItemMightMatch) const
{
    if (first_.passDirFilter(relDirPath, childItemMightMatch))
        return second_.passDirFilter(relDirPath, childItemMightMatch);
    else
    {
        if (childItemMightMatch && *childItemMightMatch)
            second_.passDirFilter(relDirPath, childItemMightMatch);
        return false;
    }
}


inline
bool CombinedFilter::isNull() const
{
    return first_.isNull() && second_.isNull();
}


inline
FilterRef CombinedFilter::copyFilterAddingExclusion(const Zstring& excludePhrase) const
{
    NameFilter tmp(first_);
    tmp.addExclusion(excludePhrase);

    return zen::makeSharedRef<CombinedFilter>(tmp, second_);
}


inline
std::strong_ordering CombinedFilter::compareSameType(const PathFilter& other) const
{
    assert(typeid(*this) == typeid(other)); //always given in this context!

    const CombinedFilter& lhs = *this;
    const CombinedFilter& rhs = static_cast<const CombinedFilter&>(other);

    if (const std::strong_ordering cmp = lhs.first_.compareSameType(rhs.first_);
        cmp != std::strong_ordering::equal)
        return cmp;

    return lhs.second_.compareSameType(rhs.second_);
}


inline
FilterRef constructFilter(const Zstring& includePhrase,
                          const Zstring& excludePhrase,
                          const Zstring& includePhrase2,
                          const Zstring& excludePhrase2)
{
    if (NameFilter::isNull(includePhrase, Zstring()))
    {
        auto filterTmp = zen::makeSharedRef<NameFilter>(includePhrase2, excludePhrase + Zstr('\n') + excludePhrase2);
        if (filterTmp.ref().isNull())
            return zen::makeSharedRef<NullFilter>();

        return filterTmp;
    }
    else
    {
        if (NameFilter::isNull(includePhrase2, Zstring()))
            return zen::makeSharedRef<NameFilter>(includePhrase, excludePhrase + Zstr('\n') + excludePhrase2);
        else
            return zen::makeSharedRef<CombinedFilter>(NameFilter(includePhrase, excludePhrase + Zstr('\n') + excludePhrase2), NameFilter(includePhrase2, Zstring()));
    }
}
}

#endif //HARD_FILTER_H_825780275842758345
