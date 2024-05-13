// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef XML_H_349578228034572457454554
#define XML_H_349578228034572457454554

//#include <set>
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
    FileInputPlain fileIn(filePath); //throw FileError, ErrorFileLocked
    std::string headBuf;
    const size_t headSizeMin = BYTE_ORDER_MARK_UTF8.size() + strLength("<?xml?>");

    const std::string buf = unbufferedLoad<std::string>([&](void* buffer, size_t bytesToRead)
    {
        const size_t bytesRead = fileIn.tryRead(buffer, bytesToRead); //throw FileError; may return short, only 0 means EOF! =>  CONTRACT: bytesToRead > 0!

        //quick test whether input is an XML: avoid loading large binary files up front!
        if (headBuf.size() < headSizeMin)
        {
            headBuf.append(static_cast<const char*>(buffer), std::min(headSizeMin - headBuf.size(), bytesRead));

            if (headBuf.size() == headSizeMin)
            {
                std::string_view header = headBuf;
                if (startsWith(header, BYTE_ORDER_MARK_UTF8))
                    header.remove_prefix(BYTE_ORDER_MARK_UTF8.size()); //keep headBuf.size()!

                if (!startsWith(header, "<?xml ") &&
                    !startsWith(header, "<?xml?>"))
                    throw FileError(replaceCpy(_("File %x does not contain a valid configuration."), L"%x", fmtPath(filePath)));
            }
        }
        return bytesRead;
    },
    fileIn.getBlockSize()); //throw FileError

    try
    {
        return parseXml(buf); //throw XmlParsingError
    }
    catch (const XmlParsingError& e)
    {
        throw FileError(
            replaceCpy(replaceCpy(replaceCpy(_("Error parsing file %x, row %y, column %z."),
                                             L"%x", fmtPath(filePath)),
                                  L"%y", formatNumber(e.row + 1)),
                       L"%z", formatNumber(e.col + 1)));
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
            if (getFileContent(filePath, nullptr /*notifyUnbufferedIO*/) == stream) //throw FileError
                return;
    }
    catch (FileError&) {}

    setFileContent(filePath, stream, nullptr /*notifyUnbufferedIO*/); //throw FileError
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
    explicit XmlOut(XmlDoc& doc) : ref_(doc.root()) {}

    ///Retrieve a handle to an XML child element for writing
    /**
    The child element will be created if it is not yet existing.
    \param name The name of the child element
    */
    XmlOut operator[](std::string name) const
    {
        XmlElement* child = ref_.getChild(name);
        return XmlOut(child ? *child : ref_.addChild(std::move(name)));
    }

    ///Retrieve a handle to an XML child element for writing
    /**
    The child element will be added, allowing for multiple elements with the same name.
    \tparam String Arbitrary string-like type: e.g. std::string, wchar_t*, char[], wchar_t, wxString, MyStringClass, ...
    \param name The name of the child element
    */
    XmlOut addChild(std::string name) const
    {
        return XmlOut(ref_.addChild(std::move(name)));
    }

    ///Write user data to the underlying XML element
    /**
    This conversion requires a specialization of zen::writeText() or zen::writeStruc() for type T.
    \tparam T User type that is converted into an XML element value.
    */
    template <class T>
    void operator()(const T& value) { writeStruc(value, ref_); }

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

    \tparam T String-convertible user data type: e.g. any string-like type, all built-in arithmetic numbers
    \sa XmlElement::setAttribute()
    */
    template <class T>
    void attribute(std::string name, const T& value) { ref_.setAttribute(std::move(name), value); }

private:
    ///Construct an output proxy for a single XML element
    /**
      \sa XmlOut(XmlDoc& doc)
    */
    explicit XmlOut(XmlElement& element) : ref_(element) {}

    XmlElement& ref_;
};


///Proxy class to conveniently convert XML structure to user data
class XmlIn
{
    struct ErrorLog;

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
    explicit XmlIn(const XmlDoc& doc) : XmlIn(&doc.root(), '<' + doc.root().getName() + '>', makeSharedRef<ErrorLog>()) {}

    ///Retrieve a handle to an XML child element for reading
    /**
    It is \b not an error if the child element does not exist, but only later if a conversion to user data is attempted.
    \param name The name of the child element
    */
    XmlIn operator[](const std::string& name) const
    {
        return XmlIn(elem_ ? elem_->getChild(name) : nullptr, elementNameFmt_ + " <" + name + '>', log_);
    }

    ///Iterate over XML child elements
    /**
    <b>Example:</b> Loop over all XML child elements
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
        in.visitChildren([&](const XmlIn& inChild)
        {
            ...
        });
    \endcode
    */
    template <class Function>
    void visitChildren(Function fun)
    {
        if (!elem_)
            logMissingElement();
        else if (std::string value; elem_->getValue(value) && !value.empty())
            logConversionError(); //have XML value element, not container!
        else
        {
            auto [it, itEnd] = elem_->getChildren();
            size_t childIdx = 0;
            std::for_each(it, itEnd, [&](const XmlElement& child)
            {
                fun(XmlIn(&child, elementNameFmt_ + " <" + child.getName() + ">[" + numberTo<std::string>(++childIdx) + ']', log_));
            });
        }
    }

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
    explicit operator bool() const { return elem_; }

    ///Read user data from the underlying XML element
    /**
    This conversion requires a specialization of zen::readText() or zen::readStruc() for type T.
    \tparam T User type that receives the data
    \return "true" if data was read successfully
    */
    template <class T>
    bool operator()(T& value) const
    {
        if (elem_)
        {
            if (readStruc(*elem_, value))
                return true;

            logConversionError();
        }
        else
            logMissingElement();

        return false;
    }

    bool hasAttribute(const std::string& name) const
    {
        return elem_ && elem_->hasAttribute(name);
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
    template <class T>
    bool attribute(const std::string& name, T& value) const
    {
        if (elem_)
        {
            if (elem_->getAttribute(name, value))
                return true;

            logMissingAttribute(name);
        }
        else
            logMissingElement();

        return false;
    }

    ///Notifies errors while mapping the XML to user data
    /**
    Error logging is shared by each hiearchy of XmlIn proxy instances that are created from each other. Consequently it doesn't matter which instance you query for errors:
    \code
        XmlIn in(doc);
        XmlIn inItem = in["item1"];

        int value = 0;
        inItem(value); //let's assume this conversion failed

        assert(in.getErrors() == inItem.getErrors());
    \endcode

    Note that error logging is \b NOT global, but owned by all instances of a hierarchy of XmlIn proxies.
    Therefore it's safe to use unrelated XmlIn proxies in different threads.
    */

    ///Get a list of XML element and attribute names which failed to convert to user data.
    /**
      \returns A list of XML element and attribute names, empty if no errors occured.
    */
    const std::wstring& getErrors() const { return log_.ref().failedElements; }

    ///Retrieve the name of this XML element.
    /**
      \returns Name of the XML element.
    */
    const std::string* getName() const
    {
        if (elem_)
            return &elem_->getName();
        return nullptr;
    }

private:
    XmlIn(const XmlElement* elem,
          const std::string& elementNameFmt,
          const SharedRef<ErrorLog>& sharedlog) : log_(sharedlog), elem_(elem), elementNameFmt_(elementNameFmt) {}

    struct ErrorLog
    {
        std::wstring failedElements; //unique list of failed elements
        std::unordered_set<std::string> usedElements;
    };

    void logElementError(const std::string& elementName) const
    {
        if (const auto [it, inserted] = log_.ref().usedElements.insert(elementName);
            inserted)
        {
            if (!log_.ref().failedElements.empty())
                log_.ref().failedElements += L'\n';
            log_.ref().failedElements += utfTo<std::wstring>(elementName);
        }
    }

    void logConversionError() const                               { logElementError(elementNameFmt_); }
    void logMissingElement() const                                { logElementError(elementNameFmt_); }
    void logMissingAttribute(const std::string& attribName) const { logElementError(elementNameFmt_ + " @" + attribName); }

    mutable SharedRef<ErrorLog> log_;
    const XmlElement* elem_;
    std::string elementNameFmt_; //e.g. "<Root> <Child> <List>[1]"
};
}

#endif //XML_H_349578228034572457454554
