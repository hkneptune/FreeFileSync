// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef XML_H_349578228034572457454554
#define XML_H_349578228034572457454554

#include <set>
#include <zen/file_io.h>
#include <zen/file_access.h>
#include "cvrt_struc.h"
#include "parser.h"


/// The zen::Xml namespace
namespace zen
{
/**
\file
\brief Save and load byte streams from files
*/

///Load XML document from a file
/**
Load and parse XML byte stream. Quick-exit if (potentially large) input file is not an XML.

\param filePath Input file path
\returns The loaded XML document
\throw FileError
*/
namespace
{
XmlDoc loadXml(const Zstring& filePath) //throw FileError
{
    FileInput fileIn(filePath, nullptr /*notifyUnbufferedIO*/); //throw FileError, ErrorFileLocked
    const size_t blockSize = fileIn.getBlockSize();
    const std::string xmlPrefix = "<?xml version=";
    bool xmlPrefixChecked = false;

    std::string buffer;
    for (;;)
    {
        buffer.resize(buffer.size() + blockSize);
        const size_t bytesRead = fileIn.read(&*(buffer.end() - blockSize), blockSize); //throw FileError, ErrorFileLocked, (X); return "bytesToRead" bytes unless end of stream!
        buffer.resize(buffer.size() - blockSize + bytesRead); //caveat: unsigned arithmetics

        //quick test whether input is an XML: avoid loading large binary files up front!
        if (!xmlPrefixChecked && buffer.size() >= xmlPrefix.size() + strLength(BYTE_ORDER_MARK_UTF8))
        {
            xmlPrefixChecked = true;
            if (!startsWith(buffer, xmlPrefix) &&
                !startsWith(buffer, BYTE_ORDER_MARK_UTF8 + xmlPrefix)) //allow BOM!
                throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)));
        }

        if (bytesRead < blockSize) //end of file
            break;
    }

    try
    {
        return parseXml(buffer); //throw XmlParsingError
    }
    catch (const XmlParsingError& e)
    {
        throw FileError(
            replaceCpy(replaceCpy(replaceCpy(_("Error parsing file %x, row %y, column %z."),
                                             L"%x", fmtPath(filePath)),
                                  L"%y", numberTo<std::wstring>(e.row + 1)),
                       L"%z", numberTo<std::wstring>(e.col + 1)));
    }
}
}


///Save XML document to a file
/**
Serialize XML to byte stream and save to file.

\param doc The XML document to save
\param filePath Output file path
\throw FileError
*/
inline
void saveXml(const XmlDoc& doc, const Zstring& filePath) //throw FileError
{
    const std::string stream = serializeXml(doc); //noexcept

    try //only update XML file if there are changes
    {
        if (getFileSize(filePath) == stream.size()) //throw FileError
            if (loadBinContainer<std::string>(filePath, nullptr /*notifyUnbufferedIO*/) == stream) //throw FileError
                return;
    }
    catch (FileError&) {}

    saveBinContainer(filePath, stream, nullptr /*notifyUnbufferedIO*/); //throw FileError
}


///Proxy class to conveniently convert user data into XML structure
class XmlOut
{
public:
    ///Construct an output proxy for an XML document
    /**
      \code
        zen::XmlDoc doc;

        zen::XmlOut out(doc);
        out["elem1"]( 1); //
        out["elem2"]( 2); //write data into XML elements
        out["elem3"](-3); //

        saveXml(doc, "out.xml"); //throw FileError
      \endcode
      Output:
      \verbatim
      <?xml version="1.0" encoding="utf-8"?>
      <Root>
          <elem1>1</elem1>
          <elem2>2</elem2>
          <elem3>-3</elem3>
      </Root>
      \endverbatim
    */
    XmlOut(XmlDoc& doc) : ref_(&doc.root()) {}
    ///Construct an output proxy for a single XML element
    /**
      \sa XmlOut(XmlDoc& doc)
    */
    XmlOut(XmlElement& element) : ref_(&element) {}

    ///Retrieve a handle to an XML child element for writing
    /**
    The child element will be created if it is not yet existing.
    \tparam String Arbitrary string-like type: e.g. std::string, wchar_t*, char[], wchar_t, wxString, MyStringClass, ...
    \param name The name of the child element
    */
    template <class String>
    XmlOut operator[](const String& name) const
    {
        const std::string utf8name = utfTo<std::string>(name);
        XmlElement* child = ref_->getChild(utf8name);
        return child ? *child : ref_->addChild(utf8name);
    }

    ///Write user data to the underlying XML element
    /**
    This conversion requires a specialization of zen::writeText() or zen::writeStruc() for type T.
    \tparam T User type that is converted into an XML element value.
    */
    template <class T>
    void operator()(const T& value) { writeStruc(value, *ref_); }

    ///Write user data to an XML attribute
    /**
    This conversion requires a specialization of zen::writeText() for type T.
      \code
        zen::XmlDoc doc;

        zen::XmlOut out(doc);
        out["elem"].attribute("attr1",  1); //
        out["elem"].attribute("attr2",  2); //write data into XML attributes
        out["elem"].attribute("attr3", -3); //

        saveXml(doc, "out.xml"); //throw FileError
      \endcode
      Output:
      \verbatim
      <?xml version="1.0" encoding="utf-8"?>
      <Root>
          <elem attr1="1" attr2="2" attr3="-3"/>
      </Root>
      \endverbatim

    \tparam String Arbitrary string-like type: e.g. std::string, wchar_t*, char[], wchar_t, wxString, MyStringClass, ...
    \tparam T String-convertible user data type: e.g. any string-like type, all built-in arithmetic numbers
    \sa XmlElement::setAttribute()
    */
    template <class String, class T>
    void attribute(const String& name, const T& value) { ref_->setAttribute(name, value); }

    ///Return a reference to the underlying Xml element
    XmlElement& ref() { return *ref_; }

private:
    XmlElement* ref_; //always bound!
};


///Proxy class to conveniently convert XML structure to user data
class XmlIn
{
    class ErrorLog;

public:
    ///Construct an input proxy for an XML document
    /**
      \code
        zen::XmlDoc doc;
          ... //load document
        zen::XmlIn in(doc);
        in["elem1"](value1); //
        in["elem2"](value2); //read data from XML elements into variables "value1", "value2", "value3"
        in["elem3"](value3); //
      \endcode
    */
    XmlIn(const XmlDoc& doc) { refList_.push_back(&doc.root()); }
    ///Construct an input proxy for a single XML element, may be nullptr
    /**
      \sa XmlIn(const XmlDoc& doc)
    */
    XmlIn(const XmlElement* element) { refList_.push_back(element); }
    ///Construct an input proxy for a single XML element
    /**
      \sa XmlIn(const XmlDoc& doc)
    */
    XmlIn(const XmlElement& element) { refList_.push_back(&element); }

    ///Retrieve a handle to an XML child element for reading
    /**
    It is \b not an error if the child element does not exist, but only later if a conversion to user data is attempted.
    \tparam String Arbitrary string-like type: e.g. std::string, wchar_t*, char[], wchar_t, wxString, MyStringClass, ...
    \param name The name of the child element
    */
    template <class String>
    XmlIn operator[](const String& name) const
    {
        std::vector<const XmlElement*> childList;

        if (refIndex_ < refList_.size())
        {
            auto iterPair = refList_[refIndex_]->getChildren(name);
            std::for_each(iterPair.first, iterPair.second,
            [&](const XmlElement& child) { childList.push_back(&child); });
        }

        return XmlIn(childList, childList.empty() ? getChildNameFormatted(name) : std::string(), log_);
    }

    ///Refer to next sibling element with the same name
    /**
    <b>Example:</b> Loop over all XML child elements named "Item"
    \verbatim
    <?xml version="1.0" encoding="utf-8"?>
    <Root>
        <Item>1</Item>
        <Item>3</Item>
        <Item>5</Item>
    </Root>
    \endverbatim

    \code
    zen::XmlIn in(doc);
    ...
    for (zen::XmlIn child = in["Item"]; child; child.next())
    {
        ...
    }
    \endcode
    */
    void next() { ++refIndex_; }

    ///Read user data from the underlying XML element
    /**
    This conversion requires a specialization of zen::readText() or zen::readStruc() for type T.
    \tparam T User type that receives the data
    \return "true" if data was read successfully
    */
    template <class T>
    bool operator()(T& value) const
    {
        if (refIndex_ < refList_.size())
        {
            bool success = readStruc(*refList_[refIndex_], value);
            if (!success)
                log_.ref().notifyConversionError(getNameFormatted());
            return success;
        }
        else
        {
            log_.ref().notifyMissingElement(getNameFormatted());
            return false;
        }
    }

    ///Read user data from an XML attribute
    /**
    This conversion requires a specialization of zen::readText() for type T.

    \code
        zen::XmlDoc doc;
          ... //load document
        zen::XmlIn in(doc);
        in["elem"].attribute("attr1", value1); //
        in["elem"].attribute("attr2", value2); //read data from XML attributes into variables "value1", "value2", "value3"
        in["elem"].attribute("attr3", value3); //
    \endcode

    \tparam String Arbitrary string-like type: e.g. std::string, wchar_t*, char[], wchar_t, wxString, MyStringClass, ...
    \tparam T String-convertible user data type: e.g. any string-like type, all built-in arithmetic numbers
    \returns "true" if the attribute was found and the conversion to the output value was successful.
    \sa XmlElement::getAttribute()
    */
    template <class String, class T>
    bool attribute(const String& name, T& value) const
    {
        if (refIndex_ < refList_.size())
        {
            bool success = refList_[refIndex_]->getAttribute(name, value);
            if (!success)
                log_.ref().notifyMissingAttribute(getNameFormatted(), utfTo<std::string>(name));
            return success;
        }
        else
        {
            log_.ref().notifyMissingElement(getNameFormatted());
            return false;
        }
    }

    ///Return a pointer to the underlying Xml element, may be nullptr
    const XmlElement* get() const { return refIndex_ < refList_.size() ? refList_[refIndex_] : nullptr; }

    ///Test whether the underlying XML element exists
    /**
      \code
         XmlIn in(doc);
         XmlIn child = in["elem1"];
         if (child)
           ...
      \endcode
      Use member pointer as implicit conversion to bool (C++ Templates - Vandevoorde/Josuttis; chapter 20)
    */
    explicit operator bool() const { return get() != nullptr; }

    ///Notifies errors while mapping the XML to user data
    /**
    Error logging is shared by each hiearchy of XmlIn proxy instances that are created from each other. Consequently it doesn't matter which instance you query for errors:
      \code
        XmlIn in(doc);
        XmlIn inItem = in["item1"];

        int value = 0;
        inItem(value); //let's assume this conversion failed

        assert(in.haveErrors() == inItem.haveErrors());
        assert(in.getErrorsAs<std::string>() == inItem.getErrorsAs<std::string>());
      \endcode

      Note that error logging is \b NOT global, but owned by all instances of a hierarchy of XmlIn proxies.
      Therefore it's safe to use unrelated XmlIn proxies in multiple threads.
      \n\n
      However be aware that the chain of connected proxy instances will be broken once you call XmlIn::get() to retrieve the underlying pointer.
      Errors that occur when working with this pointer are not logged by the original set of related instances.
    */
    bool haveErrors() const { return !log_.ref().elementList().empty(); }

    ///Get a list of XML element and attribute names which failed to convert to user data.
    /**
      \tparam String Arbitrary string class: e.g. std::string, std::wstring, wxString, MyStringClass, ...
      \returns A list of XML element and attribute names, empty list if no errors occured.
    */
    template <class String>
    std::vector<String> getErrorsAs() const
    {
        std::vector<String> output;

		for (const std::string& str : log_.ref().elementList())
			output.push_back(utfTo<String>(str));

        return output;
    }

private:
    XmlIn(const std::vector<const XmlElement*>& siblingList, const std::string& elementNameFmt, const SharedRef<ErrorLog>& sharedlog) :
        refList_(siblingList), formattedName_(elementNameFmt), log_(sharedlog)
    { assert((!siblingList.empty() && elementNameFmt.empty()) || (siblingList.empty() && !elementNameFmt.empty())); }

    static std::string getNameFormatted(const XmlElement& elem) //"<Root> <Level1> <Level2>"
    {
        return (elem.parent() ? getNameFormatted(*elem.parent()) + " " : std::string()) + "<" + elem.getNameAs<std::string>() + ">";
    }

    std::string getNameFormatted() const
    {
        if (refIndex_ < refList_.size())
        {
            assert(formattedName_.empty());
            return getNameFormatted(*refList_[refIndex_]);
        }
        else
            return formattedName_;
    }

    std::string getChildNameFormatted(const std::string& childName) const
    {
        std::string parentName = getNameFormatted();
        return (parentName.empty() ? std::string() : (parentName + " ")) + "<" + childName + ">";
    }

    class ErrorLog
    {
    public:
        void notifyConversionError (const std::string& displayName)  { insert(displayName); }
        void notifyMissingElement  (const std::string& displayName)  { insert(displayName); }
        void notifyMissingAttribute(const std::string& displayName, const std::string& attribName) { insert(displayName + " @" + attribName); }

        const std::vector<std::string>& elementList() const { return failedElements; }

    private:
        void insert(const std::string& newVal)
        {
            if (usedElements.insert(newVal).second)
                failedElements.push_back(newVal);
        }

        std::vector<std::string> failedElements; //unique list of failed elements
        std::set<std::string>    usedElements;
    };

    std::vector<const XmlElement*> refList_; //all sibling elements with same name (all pointers bound!)
    size_t refIndex_ = 0;                    //this sibling's index in refList_
    std::string formattedName_;              //contains full and formatted element name if (and only if) refList_ is empty
    mutable SharedRef<ErrorLog> log_ = makeSharedRef<ErrorLog>();
};


///Check XML input proxy for errors and map to FileError exception
/**
\param xmlInput XML input proxy
\param filePath Input file path
\throw FileError
*/
inline
void checkXmlMappingErrors(const XmlIn& xmlInput, const Zstring& filePath) //throw FileError
{
    if (xmlInput.haveErrors())
    {
        std::wstring msg = _("The following XML elements could not be read:") + L"\n";
        for (const std::wstring& elem : xmlInput.getErrorsAs<std::wstring>())
            msg += L"\n" + elem;

        throw FileError(replaceCpy(_("Configuration file %x is incomplete. The missing elements will be set to their default values."), L"%x", fmtPath(filePath)) + L"\n\n" + msg);
    }
}
}

#endif //XML_H_349578228034572457454554
