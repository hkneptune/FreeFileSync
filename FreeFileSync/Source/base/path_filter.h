// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef HARD_FILTER_H_825780275842758345
#define HARD_FILTER_H_825780275842758345

#include <vector>
#include <memory>
#include <zen/zstring.h>


namespace fff
{
//------------------------------------------------------------------
/*
Semantics of PathFilter:
1. using it creates a NEW folder hierarchy! -> must be considered by <Two way> variant!
2. it applies equally to both sides => it always matches either both sides or none! => can be used while traversing a single folder!

    class hierarchy:

           PathFilter (interface)
               /|\
       _________|_____________
      |         |             |
NullFilter  NameFilter  CombinedFilter
*/
class PathFilter;
using FilterRef = zen::SharedRef<const PathFilter>; //always bound by design! Thread-safety: internally synchronized!

const Zchar FILTER_ITEM_SEPARATOR = Zstr('|');


class PathFilter //interface for filtering
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
    friend bool operator<(const PathFilter& lhs, const PathFilter& rhs);

    virtual bool cmpLessSameType(const PathFilter& other) const = 0; //typeid(*this) == typeid(other) in this context!
};

bool operator<(const PathFilter& lhs, const PathFilter& rhs); //GCC: friend-declaration is not a "proper" declaration
inline bool operator==(const PathFilter& lhs, const PathFilter& rhs) { return !(lhs < rhs) && !(rhs < lhs); }
inline bool operator!=(const PathFilter& lhs, const PathFilter& rhs) { return !(lhs == rhs); }


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
    bool cmpLessSameType(const PathFilter& other) const override;
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
    bool cmpLessSameType(const PathFilter& other) const override;

    std::vector<Zstring> includeMasksFileFolder; //
    std::vector<Zstring> includeMasksFolder;     //upper-case + Unicode-normalized by construction
    std::vector<Zstring> excludeMasksFileFolder; //
    std::vector<Zstring> excludeMasksFolder;     //
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
    bool cmpLessSameType(const PathFilter& other) const override;

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
bool NullFilter::cmpLessSameType(const PathFilter& other) const
{
    assert(typeid(*this) == typeid(other)); //always given in this context!
    return false;
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
bool CombinedFilter::cmpLessSameType(const PathFilter& other) const
{
    assert(typeid(*this) == typeid(other)); //always given in this context!

    const CombinedFilter& otherCombFilt = static_cast<const CombinedFilter&>(other);

    if (first_ != otherCombFilt.first_)
        return first_ < otherCombFilt.first_;

    return second_ < otherCombFilt.second_;
}


inline
FilterRef constructFilter(const Zstring& includePhrase,
                          const Zstring& excludePhrase,
                          const Zstring& includePhrase2,
                          const Zstring& excludePhrase2)
{
    if (NameFilter::isNull(includePhrase, Zstring()))
    {
        auto filterTmp = zen::makeSharedRef<NameFilter>(includePhrase2, excludePhrase + Zstr("\n") + excludePhrase2);
        if (filterTmp.ref().isNull())
            return zen::makeSharedRef<NullFilter>();

        return filterTmp;
    }
    else
    {
        if (NameFilter::isNull(includePhrase2, Zstring()))
            return zen::makeSharedRef<NameFilter>(includePhrase, excludePhrase + Zstr("\n") + excludePhrase2);
        else
            return zen::makeSharedRef<CombinedFilter>(NameFilter(includePhrase, excludePhrase + Zstr("\n") + excludePhrase2), NameFilter(includePhrase2, Zstring()));
    }
}


std::vector<Zstring> splitByDelimiter(const Zstring& filterPhrase); //keep external linkage for unit test
}

#endif //HARD_FILTER_H_825780275842758345
