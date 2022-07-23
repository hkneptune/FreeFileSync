// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef DOM_H_82085720723894567204564256
#define DOM_H_82085720723894567204564256

#include <string>
#include <list>
#include <unordered_map>
#include "cvrt_text.h" //"readText/writeText"


namespace zen
{
class XmlDoc;

/// An XML element
class XmlElement
{
public:
    XmlElement() {}

    //Construct an empty XML element
    explicit XmlElement(std::string name, XmlElement* parent = nullptr) : name_(std::move(name)), parent_(parent) {}

    ///Retrieve the name of this XML element.
    /**
      \returns Name of the XML element.
    */
    const std::string& getName() const { return name_; }

    ///Get the value of this element as a user type.
    /**
      \tparam T Arbitrary user data type: e.g. any string class, all built-in arithmetic numbers, STL container, ...
      \returns "true" if Xml element was successfully converted to value, cannot fail for string-like types
    */
    template <class T>
    bool getValue(T& value) const { return readStruc(*this, value); }

    ///Set the value of this element.
    /**
      \tparam T Arbitrary user data type: e.g. any string-like type, all built-in arithmetic numbers, STL container, ...
    */
    template <class T>
    void setValue(const T& value) { writeStruc(value, *this); }

    void setValue(std::string&& value) { value_ = std::move(value); } //perf

    ///Retrieve an attribute by name.
    /**
      \tparam T String-convertible user data type: e.g. any string class, all built-in arithmetic numbers
      \param name The name of the attribute to retrieve.
      \param value The value of the attribute converted to T.
      \return "true" if value was retrieved successfully.
    */
    template <class T>
    bool getAttribute(const std::string& name, T& value) const
    {
        auto it = attributesByName.find(name);
        return it == attributesByName.end() ? false : readText(it->second->value, value);
    }

    bool hasAttribute(const std::string& name) const { return attributesByName.contains(name); }

    ///Create or update an XML attribute.
    /**
      \tparam T String-convertible user data type: e.g. any string-like type, all built-in arithmetic numbers
      \param name The name of the attribute to create or update.
      \param value The value to set.
    */
    template <class T>
    void setAttribute(std::string name, const T& value)
    {
        std::string attrValue;
        writeText(value, attrValue);

        auto it = attributesByName.find(name);
        if (it != attributesByName.end())
            it->second->value = std::move(attrValue);
        else
        {
            auto itBack = attributes_.insert(attributes_.end(), {name, std::move(attrValue)});
            attributesByName.emplace(std::move(name), itBack);
        }
        static_assert(std::is_same_v<decltype(attributes_), std::list<Attribute>>); //must NOT invalidate references used in "attributesByName"!
    }

    ///Remove the attribute with the given name.
    void removeAttribute(const std::string& name)
    {
        auto it = attributesByName.find(name);
        if (it != attributesByName.end())
        {
            attributes_.erase(it->second);
            attributesByName.erase(it);
        }
    }

    ///Create a new child element and return a reference to it.
    /**
      \param name The name of the child element to be created.
    */
    XmlElement& addChild(std::string name)
    {
        childElements_.emplace_back(name, this);
        XmlElement& newElement = childElements_.back();
        childElementsByName_.emplace(std::move(name), &newElement);

        static_assert(std::is_same_v<decltype(childElements_), std::list<XmlElement>>); //must NOT invalidate references used in "childElementsByName_"!
        return newElement;
    }

    ///Retrieve a child element with the given name.
    /**
      \param name The name of the child element to be retrieved.
      \return A pointer to the child element or nullptr if none was found.
    */
    const XmlElement* getChild(const std::string& name) const
    {
        auto it = childElementsByName_.find(name);
        return it == childElementsByName_.end() ? nullptr : it->second;
    }

    ///\sa getChild
    XmlElement* getChild(const std::string& name)
    {
        return const_cast<XmlElement*>(static_cast<const XmlElement*>(this)->getChild(name));
    }

    template <class IterTy,        //underlying iterator type
              class T,             //target object type
              class AccessPolicy> //access policy: see AccessPtrMap
    class PtrIter : private AccessPolicy //get rid of shared_ptr indirection
    {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer   = T*;
        using reference = T&;

        PtrIter(IterTy it) : it_(it) {}
        //PtrIter(const PtrIter& other) : it_(other.it_) {}
        PtrIter& operator++() { ++it_; return *this; }
        PtrIter operator++(int) { PtrIter tmp(*this); operator++(); return tmp; }
        inline friend bool operator==(const PtrIter& lhs, const PtrIter& rhs) { return lhs.it_ == rhs.it_; }
        T& operator* () const { return  AccessPolicy::template objectRef<T>(it_); }
        T* operator->() const { return &AccessPolicy::template objectRef<T>(it_); }
    private:
        IterTy it_;
    };

    struct AccessMapElement
    {
        template <class T, class IterTy>
        T& objectRef(const IterTy& it) const { return *(it->second); }
    };

    using ChildIter2      = PtrIter<std::multimap<std::string, XmlElement*>::iterator,             XmlElement, AccessMapElement>;
    using ChildIterConst2 = PtrIter<std::multimap<std::string, XmlElement*>::const_iterator, const XmlElement, AccessMapElement>;

    ///Access all child elements with the given name via STL iterators.
    /**
      \code
      auto itPair = elem.getChildren("Item");
      std::for_each(iterPair.first, iterPair.second,
            [](const XmlElement& child) { ... });
      \endcode
      \param name The name of the child elements to be retrieved.
      \return A pair of STL begin/end iterators to access the child elements sequentially.
    */
    std::pair<ChildIterConst2, ChildIterConst2> getChildren(const std::string& name) const { return childElementsByName_.equal_range(name); }

    ///\sa getChildren
    std::pair<ChildIter2, ChildIter2> getChildren(const std::string& name) { return childElementsByName_.equal_range(name); }

    struct AccessListElement
    {
        template <class T, class IterTy>
        T& objectRef(const IterTy& it) const { return *it; }
    };

    using ChildIter      = PtrIter<std::list<XmlElement>::iterator,             XmlElement, AccessListElement>;
    using ChildIterConst = PtrIter<std::list<XmlElement>::const_iterator, const XmlElement, AccessListElement>;

    ///Access all child elements sequentially via STL iterators.
    /**
      \code
      auto itPair = elem.getChildren();
      std::for_each(itPair.first, itPair.second,
            [](const XmlElement& child) { ... });
      \endcode
      \return A pair of STL begin/end iterators to access all child elements sequentially.
    */
    std::pair<ChildIterConst, ChildIterConst> getChildren() const { return {childElements_.begin(), childElements_.end()}; }

    ///\sa getChildren
    std::pair<ChildIter, ChildIter> getChildren() { return {childElements_.begin(), childElements_.end()}; }

    ///Get parent XML element, may be nullptr for root element
    XmlElement* parent() { return parent_; }
    ///Get parent XML element, may be nullptr for root element
    const XmlElement* parent() const { return parent_; }

    struct Attribute
    {
        std::string name;
        std::string value;
    };
    using AttrIter = std::list<Attribute>::const_iterator;

    /* -> disabled documentation extraction
      \brief Get all attributes associated with the element.
      \code
        auto itPair = elem.getAttributes();
        for (auto it = itPair.first; it != itPair.second; ++it)
           std::cout << "name: " << it->name << " value: " << it->value << '\n';
      \endcode
      \return A pair of STL begin/end iterators to access all attributes sequentially as a list of name/value pairs of std::string.   */
    std::pair<AttrIter, AttrIter> getAttributes() const { return {attributes_.begin(), attributes_.end()}; }

    //swap two elements while keeping references to parent.  -> disabled documentation extraction
    void swapSubtree(XmlElement& other) noexcept
    {
        name_               .swap(other.name_);
        value_              .swap(other.value_);
        attributes_         .swap(other.attributes_);
        attributesByName    .swap(other.attributesByName);
        childElements_      .swap(other.childElements_);
        childElementsByName_.swap(other.childElementsByName_);

        for (XmlElement& child : childElements_)
            child.parent_ = this;
        for (XmlElement& child : other.childElements_)
            child.parent_ = &other;
    }

private:
    XmlElement           (const XmlElement&) = delete;
    XmlElement& operator=(const XmlElement&) = delete;

    std::string name_;
    std::string value_;

    std::list<Attribute>                                            attributes_;      //attributes in order of creation
    std::unordered_map<std::string, std::list<Attribute>::iterator> attributesByName; //alternate view for lookup

    std::list<XmlElement>                             childElements_;       //child elements in order of creation
    std::multimap<std::string, XmlElement*> childElementsByName_; //alternate view for lookup
    //alternative: std::unordered_map => but let's keep std::map, so which guarantees consistent order of duplicate items!
    //e.g. std::unordered_map on Linux inserts duplicates in reverse!

    XmlElement* parent_ = nullptr;
};


//XmlElement::setValue<T>() calls zen::writeStruc() which calls XmlElement::setValue() ... => these two specializations end the circle
template <> inline
void XmlElement::setValue(const std::string& value) { value_ = value; }

template <> inline
bool XmlElement::getValue(std::string& value) const { value = value_; return true; }


///The complete XML document
class XmlDoc
{
public:
    ///Default constructor setting up an empty XML document with a standard declaration: <?xml version="1.0" encoding="utf-8" ?>
    XmlDoc() {}

    XmlDoc(XmlDoc&& tmp) noexcept { swap(tmp); }
    XmlDoc& operator=(XmlDoc&& tmp) noexcept { swap(tmp); return *this; }

    //Setup an empty XML document
    /**
      \param rootName The name of the XML document's root element.
    */
    explicit XmlDoc(std::string rootName) : root_(std::move(rootName)) {}

    ///Get a const reference to the document's root element.
    const XmlElement& root() const { return root_; }
    ///Get a reference to the document's root element.
    XmlElement& root() { return root_; }

    ///Get the version used in the XML declaration.
    const std::string& getVersion() const { return version_; }

    ///Set the version used in the XML declaration.
    void setVersion(const std::string& version) { version_ = version; }

    ///Get the encoding used in the XML declaration.
    const std::string& getEncoding() const { return encoding_; }

    ///Set the encoding used in the XML declaration.
    void setEncoding(const std::string& encoding) { encoding_ = encoding; }

    ///Get the standalone string used in the XML declaration.
    const std::string& getStandalone() const { return standalone_; }

    ///Set the standalone string used in the XML declaration.
    void setStandalone(const std::string& standalone) { standalone_ = standalone; }

    //Transactionally swap two elements.  -> disabled documentation extraction
    void swap(XmlDoc& other) noexcept
    {
        version_   .swap(other.version_);
        encoding_  .swap(other.encoding_);
        standalone_.swap(other.standalone_);
        root_.swapSubtree(other.root_);
    }

private:
    XmlDoc           (const XmlDoc&) = delete; //not implemented, thanks to XmlElement::parent_
    XmlDoc& operator=(const XmlDoc&) = delete;

    std::string version_ {"1.0"};
    std::string encoding_{"utf-8"};
    std::string standalone_;

    XmlElement root_{"Root"};
};
}

#endif //DOM_H_82085720723894567204564256
