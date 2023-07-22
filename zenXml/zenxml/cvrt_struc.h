// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef CVRT_STRUC_H_018727409908342709743
#define CVRT_STRUC_H_018727409908342709743

#include "dom.h"


namespace zen
{
/**
\file
\brief Handle conversion of arbitrary types to and from XML elements.
See comments in cvrt_text.h
*/

///Convert XML element to structured user data
/**
  \param input The input XML element.
  \param value Conversion target value.
  \return "true" if value was read successfully.
*/
template <class T> bool readStruc(const XmlElement& input, T& value);
///Convert structured user data into an XML element
/**
  \param value The value to be converted.
  \param output The output XML element.
*/
template <class T> void writeStruc(const T& value, XmlElement& output);












//------------------------------ implementation -------------------------------------
namespace impl_2384343
{
ZEN_INIT_DETECT_MEMBER_TYPE(value_type)
ZEN_INIT_DETECT_MEMBER_TYPE(iterator)
ZEN_INIT_DETECT_MEMBER_TYPE(const_iterator)

ZEN_INIT_DETECT_MEMBER(begin)  //
ZEN_INIT_DETECT_MEMBER(end)    //we don't know the exact declaration of the member attribute: may be in a base class!
ZEN_INIT_DETECT_MEMBER(insert) //
}

template <typename T>
using IsStlContainer = std::bool_constant<
                       impl_2384343::hasMemberType_value_type    <T>&&
                       impl_2384343::hasMemberType_iterator      <T>&&
                       impl_2384343::hasMemberType_const_iterator<T>&&
                       impl_2384343::hasMember_begin             <T>&&
                       impl_2384343::hasMember_end               <T>&&
                       impl_2384343::hasMember_insert            <T>>;


template <class T>
struct IsStlPair
{
private:
    using Yes = char[1];
    using No  = char[2];

    template <class T1, class T2>
    static Yes& isPair(const std::pair<T1, T2>&);
    static  No& isPair(...);
public:
    enum { value = sizeof(isPair(std::declval<T>())) == sizeof(Yes) };
};

//######################################################################################

//Conversion from arbitrary types to an XML element
enum class ValueType
{
    stlContainer,
    stlPair,
    other,
};

template <class T>
using GetValueType = std::integral_constant<ValueType,
      GetTextType   <T>::value != TextType::other ? ValueType::other : //some string classes are also STL containers, so check this first
      IsStlContainer<T>::value ? ValueType::stlContainer :
      IsStlPair     <T>::value ? ValueType::stlPair :
      ValueType::other>;


template <class T, ValueType type>
struct ConvertElement;
/* -> expected interface
{
    void writeStruc(const T& value, XmlElement& output) const;
    bool readStruc(const XmlElement& input, T& value) const;
};
*/


//partial specialization: handle conversion for all STL-container types!
template <class T>
struct ConvertElement<T, ValueType::stlContainer>
{
    void writeStruc(const T& value, XmlElement& output) const
    {
        for (const typename T::value_type& childVal : value)
        {
            XmlElement& newChild = output.addChild("Item");
            zen::writeStruc(childVal, newChild);
        }
    }
    bool readStruc(const XmlElement& input, T& value) const
    {
        value.clear();

        bool success = true;
        auto [it, itEnd] = input.getChildren();

        std::for_each(it, itEnd, [&](const XmlElement& xmlChild)
        {
            typename T::value_type childVal;
            if (zen::readStruc(xmlChild, childVal))
                value.insert(value.end(), std::move(childVal));
            else
                success = false;
            //should we support insertion of partially-loaded struct??
        });
        return success;
    }
};


//partial specialization: handle conversion for std::pair
template <class T>
struct ConvertElement<T, ValueType::stlPair>
{
    void writeStruc(const T& value, XmlElement& output) const
    {
        XmlElement& child1 = output.addChild("one"); //don't use "1st/2nd", this will confuse a few pedantic XML parsers
        zen::writeStruc(value.first, child1);

        XmlElement& child2 = output.addChild("two");
        zen::writeStruc(value.second, child2);
    }
    bool readStruc(const XmlElement& input, T& value) const
    {
        bool success = true;
        const XmlElement* child1 = input.getChild("one");
        if (!child1 || !zen::readStruc(*child1, value.first))
            success = false;

        const XmlElement* child2 = input.getChild("two");
        if (!child2 || !zen::readStruc(*child2, value.second))
            success = false;

        return success;
    }
};


//partial specialization: not a pure structured type, try text conversion (thereby respect user specializations of writeText()/readText())
template <class T>
struct ConvertElement<T, ValueType::other>
{
    void writeStruc(const T& value, XmlElement& output) const
    {
        std::string tmp;
        writeText(value, tmp);
        output.setValue(std::move(tmp));
    }
    bool readStruc(const XmlElement& input, T& value) const
    {
        std::string rawStr;
        input.getValue(rawStr);
        return readText(rawStr, value);
    }
};


template <class T> inline
void writeStruc(const T& value, XmlElement& output)
{
    ConvertElement<T, GetValueType<T>::value>().writeStruc(value, output);
}


template <class T> inline
bool readStruc(const XmlElement& input, T& value)
{
    return ConvertElement<T, GetValueType<T>::value>().readStruc(input, value);
}
}

#endif //CVRT_STRUC_H_018727409908342709743
