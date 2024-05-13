// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PARSER_H_81248670213764583021432
#define PARSER_H_81248670213764583021432

//#include <cstdio>
#include <cstddef> //ptrdiff_t; req. on Linux
#include <zen/string_tools.h>
#include "dom.h"


namespace zen
{
/**
\file
\brief Convert an XML document object model (class XmlDoc) to and from a byte stream representation.
*/

///Save XML document as a byte stream
/**
\param doc Input XML document
\param lineBreak Line break, default: carriage return + new line
\param indent Indentation, default: four space characters
\return Output byte stream
*/
std::string serializeXml(const XmlDoc& doc,
                         const std::string& lineBreak = "\r\n",
                         const std::string& indent    = "    "); //noexcept


///Exception thrown due to an XML parsing error
struct XmlParsingError
{
    XmlParsingError(size_t rowNo, size_t colNo) : row(rowNo), col(colNo) {}
    ///Input file row where the parsing error occured (zero-based)
    const size_t row; //beginning with 0
    ///Input file column where the parsing error occured (zero-based)
    const size_t col; //
};

///Load XML document from a byte stream
/**
\param stream Input byte stream
\returns Output XML document
\throw XmlParsingError
*/
XmlDoc parseXml(const std::string& stream); //throw XmlParsingError




















//---------------------------- implementation ----------------------------
//see: https://www.w3.org/TR/xml/

namespace xml_impl
{
template <class Predicate> inline
std::string normalize(const std::string_view& str, Predicate pred) //pred: unary function taking a char, return true if value shall be encoded as hex
{
    std::string output;
    for (const char c : str)
        switch (c)
        {
			//*INDENT-OFF*
			case '&': output += "&amp;"; break; //
			case '<': output +=  "&lt;"; break; //normalization mandatory: https://www.w3.org/TR/xml/#syntax
			case '>': output +=  "&gt;"; break; //
			default:
				if (pred(c))
				{
					if      (c == '\'') output += "&apos;";
					else if (c ==  '"') output += "&quot;";
					else
					{
						output += "&#x";
						const auto [high, low] = hexify(c);
						output += high;
						output += low;
						output += ';';
					}
				}
				else
					output += c;
				break;
			//*INDENT-ON*
        }
    return output;
}

inline
std::string normalizeName(const std::string& str)
{
    /*const*/ std::string nameFmt = normalize(str, [](char c) { return isWhiteSpace(c) || c == '=' || c == '/' || c == '\'' || c == '"'; });
    assert(!nameFmt.empty());
    return nameFmt;
}

inline
std::string normalizeElementValue(const std::string& str)
{
    return normalize(str, [](char c) { return static_cast<unsigned char>(c) < 32; });
}

inline
std::string normalizeAttribValue(const std::string& str)
{
    return normalize(str, [](char c) { return static_cast<unsigned char>(c) < 32 || c == '\'' || c == '"'; });
}


template <class CharIterator, size_t N> inline
bool checkEntity(CharIterator& first, CharIterator last, const char (&placeholder)[N])
{
    assert(placeholder[N - 1] == 0);
    const ptrdiff_t strLen = N - 1; //don't count null-terminator
    if (last - first >= strLen && std::equal(first, first + strLen, placeholder))
    {
        first += strLen - 1;
        return true;
    }
    return false;
}


namespace
{
std::string denormalize(const std::string_view& str)
{
    std::string output;
    for (auto it = str.begin(); it != str.end(); ++it)
    {
        const char c = *it;

        if (c == '&')
        {
            if (checkEntity(it, str.end(), "&amp;"))
                output += '&';
            else if (checkEntity(it, str.end(), "&lt;"))
                output += '<';
            else if (checkEntity(it, str.end(), "&gt;"))
                output += '>';
            else if (checkEntity(it, str.end(), "&apos;"))
                output += '\'';
            else if (checkEntity(it, str.end(), "&quot;"))
                output += '"';
            else if (str.end() - it >= 6 &&
                     it[1] == '#' &&
                     it[2] == 'x' &&
                     isHexDigit(it[3]) &&
                     isHexDigit(it[4]) &&
                     it[5] == ';')
            {
                output += unhexify(it[3], it[4]);
                it += 5;
            }
            else
                output += c; //unexpected char!
        }
        else if (c == '\r') //map all end-of-line characters to \n https://www.w3.org/TR/xml/#sec-line-ends
        {
            auto itNext = it + 1;
            if (itNext != str.end() && *itNext == '\n')
                ++it;
            output += '\n';
        }
        else
            output += c;
    }
    return output;
}


void serialize(const XmlElement& element, std::string& stream,
               const std::string& lineBreak,
               const std::string& indent,
               size_t indentLevel)
{
    const std::string& nameFmt = normalizeName(element.getName());

    for (size_t i = 0; i < indentLevel; ++i)
        stream += indent;

    stream += '<' + nameFmt;

    auto attr = element.getAttributes();
    for (auto it = attr.first; it != attr.second; ++it)
        stream += ' ' + normalizeName(it->name) + "=\"" + normalizeAttribValue(it->value) + '"';

    auto [it, itEnd] = element.getChildren();
    if (it != itEnd) //structured element
    {
        //no support for mixed-mode content
        stream += '>' + lineBreak;

        std::for_each(it, itEnd, [&](const XmlElement& el)
        { serialize(el, stream, lineBreak, indent, indentLevel + 1); });

        for (size_t i = 0; i < indentLevel; ++i)
            stream += indent;
        stream += "</" + nameFmt + '>' + lineBreak;
    }
    else
    {
        std::string value;
        element.getValue(value);

        if (!value.empty()) //value element
            stream += '>' + normalizeElementValue(value) + "</" + nameFmt + '>' + lineBreak;
        else //empty element
            stream += "/>" + lineBreak;
    }
}
}
}

inline
std::string serializeXml(const XmlDoc& doc,
                         const std::string& lineBreak,
                         const std::string& indent)
{
    std::string output = "<?xml";

    const std::string& version = doc.getVersion();
    if (!version.empty())
        output += " version=\"" + xml_impl::normalizeAttribValue(version) + '"';

    const std::string& encoding = doc.getEncoding();
    if (!encoding.empty())
        output += " encoding=\"" + xml_impl::normalizeAttribValue(encoding) + '"';

    const std::string& standalone = doc.getStandalone();
    if (!standalone.empty())
        output += " standalone=\"" + xml_impl::normalizeAttribValue(standalone) + '"';

    output += "?>" + lineBreak;

    xml_impl::serialize(doc.root(), output, lineBreak, indent, 0 /*indentLevel*/);
    return output;
}

/*
Grammar for XML parser
-------------------------------
document-expression:
    <?xml version="1.0" encoding="utf-8" standalone="yes"?>
    element-expression:

element-expression:
    <string attributes-expression/>
    <string attributes-expression> pm-expression </string>

element-list-expression:
    <empty>
    element-expression element-list-expression

attributes-expression:
    <empty>
    string="string" attributes-expression

pm-expression:
    string
    element-list-expression
*/

namespace xml_impl
{
struct Token
{
    enum Type
    {
        TK_LESS,
        TK_GREATER,
        TK_LESS_SLASH,
        TK_SLASH_GREATER,
        TK_EQUAL,
        TK_QUOTE,
        TK_DECL_BEGIN,
        TK_DECL_END,
        TK_NAME,
        TK_END
    };

    Token(Type t) : type(t) {}
    Token(const std::string&  txt) : type(TK_NAME), name(txt) {}
    Token(      std::string&& txt) : type(TK_NAME), name(std::move(txt)) {}

    Type type;
    std::string name; //filled if type == TK_NAME
};

class Scanner
{
public:
    explicit Scanner(const std::string& stream) : stream_(stream), pos_(stream_.begin())
    {
        if (zen::startsWith(stream_, BYTE_ORDER_MARK_UTF8))
            pos_ += BYTE_ORDER_MARK_UTF8.size();
    }

    Token getNextToken() //throw XmlParsingError
    {
        //skip whitespace
        pos_ = std::find_if_not(pos_, stream_.end(), isWhiteSpace<char>);

        if (pos_ == stream_.end())
            return Token::TK_END;

        //skip XML comments
        if (startsWith(xmlCommentBegin_))
        {
            auto it = std::search(pos_ + xmlCommentBegin_.size(), stream_.end(), xmlCommentEnd_.begin(), xmlCommentEnd_.end());
            if (it != stream_.end())
            {
                pos_ = it + xmlCommentEnd_.size();
                return getNextToken(); //throw XmlParsingError
            }
        }

        for (auto it = tokens_.begin(); it != tokens_.end(); ++it)
            if (startsWith(it->first))
            {
                pos_ += it->first.size();
                return it->second;
            }

        const auto itNameEnd = std::find_if(pos_, stream_.end(), [](char c)
        {
            return c == '<'  ||
                   c == '>'  ||
                   c == '='  ||
                   c == '/'  ||
                   c == '\'' ||
                   c == '"'  ||
                   isWhiteSpace(c);
        });

        if (itNameEnd != pos_)
        {
            const std::string_view name = makeStringView(pos_, itNameEnd);
            pos_ = itNameEnd;
            return denormalize(name);
        }

        //unknown token
        throw XmlParsingError(posRow(), posCol());
    }

    std::string extractElementValue()
    {
        auto it = std::find_if(pos_, stream_.end(), [](char c)
        {
            return c == '<'  ||
                   c == '>';
        });
        const std::string_view output = makeStringView(pos_, it);
        pos_ = it;
        return denormalize(output);
    }

    std::string extractAttributeValue()
    {
        auto it = std::find_if(pos_, stream_.end(), [](char c)
        {
            return c == '<'  ||
                   c == '>'  ||
                   c == '\'' ||
                   c == '"';
        });
        const std::string_view output = makeStringView(pos_, it);
        pos_ = it;
        return denormalize(output);
    }

    size_t posRow() const //current row beginning with 0
    {
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
            if (isLineBreak(*it))
                return pos_ - it - 1;
        }
        return pos_ - stream_.begin();
    }

private:
    Scanner           (const Scanner&) = delete;
    Scanner& operator=(const Scanner&) = delete;

    bool startsWith(const std::string& prefix) const
    {
        return zen::startsWith(makeStringView(pos_, stream_.end()), prefix);
    }

    using TokenList = std::vector<std::pair<std::string, Token::Type>>;
    const TokenList tokens_
    {
        {"<?xml", Token::TK_DECL_BEGIN   },
        {"?>",    Token::TK_DECL_END     },
        {"</",    Token::TK_LESS_SLASH   },
        {"/>",    Token::TK_SLASH_GREATER},
        {"<",     Token::TK_LESS         }, //evaluate after TK_DECL_BEGIN!
        {">",     Token::TK_GREATER      },
        {"=",     Token::TK_EQUAL        },
        {"\"",    Token::TK_QUOTE        },
        {"\'",    Token::TK_QUOTE        },
    };

    const std::string xmlCommentBegin_ = "<!--";
    const std::string xmlCommentEnd_   = "-->";

    const std::string stream_;
    std::string::const_iterator pos_;
};


class XmlParser
{
public:
    explicit XmlParser(const std::string& stream) :
        scn_(stream),
        tk_(scn_.getNextToken()) {} //throw XmlParsingError

    XmlDoc parse() //throw XmlParsingError
    {
        XmlDoc doc;

        //declaration (optional)
        if (token().type == Token::TK_DECL_BEGIN)
        {
            nextToken(); //throw XmlParsingError

            while (token().type == Token::TK_NAME)
            {
                std::string attribName = token().name;
                nextToken(); //throw XmlParsingError

                consumeToken(Token::TK_EQUAL); //throw XmlParsingError
                expectToken (Token::TK_QUOTE); //
                std::string attribValue = scn_.extractAttributeValue();
                nextToken(); //throw XmlParsingError

                consumeToken(Token::TK_QUOTE); //throw XmlParsingError

                if (attribName == "version")
                    doc.setVersion(attribValue);
                else if (attribName == "encoding")
                    doc.setEncoding(attribValue);
                else if (attribName == "standalone")
                    doc.setStandalone(attribValue);
            }
            consumeToken(Token::TK_DECL_END); //throw XmlParsingError
        }

        XmlElement dummy;
        parseChildElements(dummy);

        auto [it, itEnd] = dummy.getChildren();
        if (it != itEnd)
            doc.root().swapSubtree(*it);

        expectToken(Token::TK_END); //throw XmlParsingError
        return doc;
    }

private:
    XmlParser           (const XmlParser&) = delete;
    XmlParser& operator=(const XmlParser&) = delete;

    void parseChildElements(XmlElement& parent)
    {
        while (token().type == Token::TK_LESS)
        {
            nextToken(); //throw XmlParsingError

            expectToken(Token::TK_NAME); //throw XmlParsingError
            const std::string elementName = token().name;
            nextToken(); //throw XmlParsingError

            XmlElement& newElement = parent.addChild(elementName);

            parseAttributes(newElement);

            if (token().type == Token::TK_SLASH_GREATER) //empty element
            {
                nextToken(); //throw XmlParsingError
                continue;
            }

            expectToken(Token::TK_GREATER); //throw XmlParsingError
            std::string elementValue = scn_.extractElementValue();
            nextToken(); //throw XmlParsingError

            //no support for mixed-mode content
            if (token().type == Token::TK_LESS) //structure-element
                parseChildElements(newElement);
            else                                //value-element
                newElement.setValue(std::move(elementValue));

            consumeToken(Token::TK_LESS_SLASH); //throw XmlParsingError

            expectToken(Token::TK_NAME); //throw XmlParsingError
            if (token().name != elementName)
                throw XmlParsingError(scn_.posRow(), scn_.posCol());
            nextToken(); //throw XmlParsingError

            consumeToken(Token::TK_GREATER); //throw XmlParsingError
        }
    }

    void parseAttributes(XmlElement& element)
    {
        while (token().type == Token::TK_NAME)
        {
            const std::string attribName = token().name;
            nextToken(); //throw XmlParsingError

            consumeToken(Token::TK_EQUAL); //throw XmlParsingError
            expectToken (Token::TK_QUOTE); //
            std::string attribValue = scn_.extractAttributeValue();
            nextToken(); //throw XmlParsingError

            consumeToken(Token::TK_QUOTE); //throw XmlParsingError
            element.setAttribute(attribName, attribValue);
        }
    }

    const Token& token() const { return tk_; }

    void nextToken() { tk_ = scn_.getNextToken(); } //throw XmlParsingError

    void expectToken(Token::Type t) //throw XmlParsingError
    {
        if (token().type != t)
            throw XmlParsingError(scn_.posRow(), scn_.posCol());
    }

    void consumeToken(Token::Type t) //throw XmlParsingError
    {
        expectToken(t); //throw XmlParsingError
        nextToken();    //
    }

    Scanner scn_;
    Token tk_;
};
}

inline
XmlDoc parseXml(const std::string& stream) //throw XmlParsingError
{
    return xml_impl::XmlParser(stream).parse(); //throw XmlParsingError
}
}

#endif //PARSER_H_81248670213764583021432
