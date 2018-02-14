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
Semantics of HardFilter:
1. using it creates a NEW folder hierarchy! -> must be considered by <Two way> variant!
2. it applies equally to both sides => it always matches either both sides or none! => can be used while traversing a single folder!

    class hierarchy:

           HardFilter (interface)
               /|\
       _________|_____________
      |         |             |
NullFilter  NameFilter  CombinedFilter
*/

class HardFilter //interface for filtering
{
public:
    virtual ~HardFilter() {}

    //filtering
    virtual bool passFileFilter(const Zstring& relFilePath) const = 0;
    virtual bool passDirFilter (const Zstring& relDirPath, bool* childItemMightMatch) const = 0;
    //childItemMightMatch: file/dir in subdirectories could(!) match
    //note: this hint is only set if passDirFilter returns false!

    virtual bool isNull() const = 0; //filter is equivalent to NullFilter, but may be technically slower

    using FilterRef = std::shared_ptr<const HardFilter>; //always bound by design!

    virtual FilterRef copyFilterAddingExclusion(const Zstring& excludePhrase) const = 0;

private:
    friend bool operator<(const HardFilter& lhs, const HardFilter& rhs);

    virtual bool cmpLessSameType(const HardFilter& other) const = 0; //typeid(*this) == typeid(other) in this context!
};

bool operator<(const HardFilter& lhs, const HardFilter& rhs); //GCC: friend-declaration is not a "proper" declaration
inline bool operator==(const HardFilter& lhs, const HardFilter& rhs) { return !(lhs < rhs) && !(rhs < lhs); }
inline bool operator!=(const HardFilter& lhs, const HardFilter& rhs) { return !(lhs == rhs); }


//small helper method: merge two hard filters (thereby remove Null-filters)
HardFilter::FilterRef combineFilters(const HardFilter::FilterRef& first,
                                     const HardFilter::FilterRef& second);


class NullFilter : public HardFilter  //no filtering at all
{
public:
    bool passFileFilter(const Zstring& relFilePath) const override { return true; }
    bool passDirFilter(const Zstring& relDirPath, bool* childItemMightMatch) const override;
    bool isNull() const override { return true; }
    FilterRef copyFilterAddingExclusion(const Zstring& excludePhrase) const override;

private:
    bool cmpLessSameType(const HardFilter& other) const override;
};


class NameFilter : public HardFilter  //standard filter by filepath
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
    bool cmpLessSameType(const HardFilter& other) const override;

    std::vector<Zstring> includeMasksFileFolder; //
    std::vector<Zstring> includeMasksFolder;     //upper case (windows) + unique items by construction
    std::vector<Zstring> excludeMasksFileFolder; //
    std::vector<Zstring> excludeMasksFolder;     //
};


class CombinedFilter : public HardFilter  //combine two filters to match if and only if both match
{
public:
    CombinedFilter(const NameFilter& first, const NameFilter& second) : first_(first), second_(second) { assert(!first.isNull() && !second.isNull()); } //if either is null, then wy use CombinedFilter?

    bool passFileFilter(const Zstring& relFilePath) const override;
    bool passDirFilter(const Zstring& relDirPath, bool* childItemMightMatch) const override;
    bool isNull() const override;
    FilterRef copyFilterAddingExclusion(const Zstring& excludePhrase) const override;

private:
    bool cmpLessSameType(const HardFilter& other) const override;

    const NameFilter first_;
    const NameFilter second_;
};

const Zchar FILTER_ITEM_SEPARATOR = Zstr('|');





//--------------- inline implementation ---------------------------------------
inline
bool NullFilter::passDirFilter(const Zstring& relDirPath, bool* childItemMightMatch) const
{
    assert(!childItemMightMatch || *childItemMightMatch == true); //check correct usage
    return true;
}


inline
bool NullFilter::cmpLessSameType(const HardFilter& other) const
{
    assert(typeid(*this) == typeid(other)); //always given in this context!
    return false;
}


inline
HardFilter::FilterRef NullFilter::copyFilterAddingExclusion(const Zstring& excludePhrase) const
{
    auto filter = std::make_shared<NameFilter>(Zstr("*"), excludePhrase);
    if (filter->isNull())
        return std::make_shared<NullFilter>();
    return filter;
}


inline
HardFilter::FilterRef NameFilter::copyFilterAddingExclusion(const Zstring& excludePhrase) const
{
    auto tmp = std::make_shared<NameFilter>(*this);
    tmp->addExclusion(excludePhrase);
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
HardFilter::FilterRef CombinedFilter::copyFilterAddingExclusion(const Zstring& excludePhrase) const
{
    NameFilter tmp(first_);
    tmp.addExclusion(excludePhrase);

    return std::make_shared<CombinedFilter>(tmp, second_);
}


inline
bool CombinedFilter::cmpLessSameType(const HardFilter& other) const
{
    assert(typeid(*this) == typeid(other)); //always given in this context!

    const CombinedFilter& otherCombFilt = static_cast<const CombinedFilter&>(other);

    if (first_ != otherCombFilt.first_)
        return first_ < otherCombFilt.first_;

    return second_ < otherCombFilt.second_;
}


inline
HardFilter::FilterRef constructFilter(const Zstring& includePhrase,
                                      const Zstring& excludePhrase,
                                      const Zstring& includePhrase2,
                                      const Zstring& excludePhrase2)
{
    std::shared_ptr<HardFilter> filterTmp;

    if (NameFilter::isNull(includePhrase, Zstring()))
        filterTmp = std::make_shared<NameFilter>(includePhrase2, excludePhrase + Zstr("\n") + excludePhrase2);
    else
    {
        if (NameFilter::isNull(includePhrase2, Zstring()))
            filterTmp = std::make_shared<NameFilter>(includePhrase, excludePhrase + Zstr("\n") + excludePhrase2);
        else
            return std::make_shared<CombinedFilter>(NameFilter(includePhrase, excludePhrase + Zstr("\n") + excludePhrase2), NameFilter(includePhrase2, Zstring()));
    }

    if (filterTmp->isNull())
        return std::make_shared<NullFilter>();

    return filterTmp;
}


std::vector<Zstring> splitByDelimiter(const Zstring& filterString); //keep external linkage for unit test
}

#endif //HARD_FILTER_H_825780275842758345
