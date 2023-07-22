// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef JSON_H_0187348321748321758934215734
#define JSON_H_0187348321748321758934215734

#include <zen/string_tools.h>


namespace zen
{
//Spec: https://tools.ietf.org/html/rfc8259
//Test: http://seriot.ch/parsing_json.php
struct JsonValue
{
    enum class Type
    {
        null,    //
        boolean, //primitive types
        number,  //
        string,  //
        array,
        object,
    };

    /**/     JsonValue() {}
    explicit JsonValue(Type t)          : type(t) {}
    explicit JsonValue(bool b)          : type(Type::boolean), primVal(b ? "true" : "false") {}
    explicit JsonValue(int num)         : type(Type::number),  primVal(numberTo<std::string>(num)) {}
    explicit JsonValue(int64_t num)     : type(Type::number),  primVal(numberTo<std::string>(num)) {}
    explicit JsonValue(double num)      : type(Type::number),  primVal(numberTo<std::string>(num)) {}
    explicit JsonValue(std::string str) : type(Type::string),  primVal(std::move(str)) {} //unifying assignment
    explicit JsonValue(const char* str) : type(Type::string),  primVal(str) {}
    explicit JsonValue(const void*) = delete; //catch usage errors e.g. const int* -> JsonValue(bool)
    //explicit JsonValue(std::initializer_list<JsonValue> initList) : type(Type::array), arrayVal(initList) {} => empty list is ambiguous
    explicit JsonValue(std::vector<JsonValue> initList) : type(Type::array), arrayVal(std::move(initList)) {} //unifying assignment


    Type type = Type::null;
    std::string                      primVal; //for primitive types
    std::vector<JsonValue>           arrayVal;
    std::map<std::string, JsonValue> objectVal; //"[...] most implementations of JSON libraries do not accept duplicate keys [...]" => fine!
    //alternative: std::unordered_map => but let's keep std::map, so that objectVal is sorted for our unit tests
};


std::string serializeJson(const JsonValue& jval,
                          const std::string& lineBreak = "\n",
                          const std::string& indent    = "    "); //noexcept


struct JsonParsingError
{
    JsonParsingError(size_t rowNo, size_t colNo) : row(rowNo), col(colNo) {}
    const size_t row; //beginning with 0
    const size_t col; //
};
JsonValue parseJson(const std::string& stream); //throw JsonParsingError



//helper functions for JsonValue access:
inline
const JsonValue* getChildFromJsonObject(const JsonValue& jvalue, const std::string& name)
{
    if (jvalue.type != JsonValue::Type::object)
        return nullptr;

    auto it = jvalue.objectVal.find(name);
    if (it == jvalue.objectVal.end())
        return nullptr;

    return &it->second;
}


inline
std::optional<std::string> getPrimitiveFromJsonObject(const JsonValue& jvalue, const std::string& name)
{
    if (const JsonValue* childValue = getChildFromJsonObject(jvalue, name))
        if (childValue->type != JsonValue::Type::object &&
            childValue->type != JsonValue::Type::array)
            return childValue->primVal;
    return std::nullopt;
}





//---------------------- implementation ----------------------
namespace json_impl
{
namespace
{
[[nodiscard]] std::string jsonEscape(const std::string& str)
{
    std::string output;
    for (const char c : str)
        switch (c)
        {
            //*INDENT-OFF*
            case '\\': output += "\\\\"; break; //
            case  '"': output += "\\\""; break; //escaping mandatory

            case '\b': output += "\\b"; break; //
            case '\f': output += "\\f"; break; //
            case '\n': output += "\\n"; break; //prefer compact escaping
            case '\r': output += "\\r"; break; //
            case '\t': output += "\\t"; break; //

            default:
                if (static_cast<unsigned char>(c) < 32)
                {
                    const auto [high, low] = hexify(c);
                    output += "\\u00";
                    output += high;
                    output += low;
                }
                else
                    output += c;
                break;
            //*INDENT-ON*
        }
    return output;
}


[[nodiscard]] std::string jsonUnescape(const std::string& str)
{
    std::string output;
    std::basic_string<impl::Char16> utf16Buf;

    auto flushUtf16 = [&]
    {
        if (!utf16Buf.empty())
        {
            UtfDecoder<impl::Char16> decoder(utf16Buf.c_str(), utf16Buf.size());
            while (std::optional<impl::CodePoint> cp = decoder.getNext())
                codePointToUtf<char>(*cp, [&](char c) { output += c; });
            utf16Buf.clear();
        }
    };
    auto writeOut = [&](char c)
    {
        flushUtf16();
        output += c;
    };

    for (auto it = str.begin(); it != str.end(); ++it)
    {
        const char c = *it;
        if (c == '\\')
        {
            ++it;
            if (it == str.end()) //unexpected end!
            {
                writeOut(c);
                break;
            }

            const char c2 = *it;
            switch (c2)
            {
                //*INDENT-OFF*
                case '\\':
                case '"':
                case '/': writeOut(c2);   break;
                case 'b': writeOut('\b'); break;
                case 'f': writeOut('\f'); break;
                case 'n': writeOut('\n'); break;
                case 'r': writeOut('\r'); break;
                case 't': writeOut('\t'); break;
                default:
                    if (c2 == 'u' &&
                        str.end() - it >= 5 &&
                        isHexDigit(it[1])   &&
                        isHexDigit(it[2])   &&
                        isHexDigit(it[3])   &&
                        isHexDigit(it[4]))
                    {
                        utf16Buf += static_cast<impl::Char16>(static_cast<unsigned char>(unhexify(it[1], it[2])) * 256 +
                                                              static_cast<unsigned char>(unhexify(it[3], it[4])));
                        it += 4;
                    }
                    else //unknown escape sequence!
                    {
                        writeOut(c);
                        writeOut(c2);
                    }
                    break;
                //*INDENT-ON*
            }
        }
        else
            writeOut(c);
    }
    flushUtf16();
    return output;
}


void serialize(const JsonValue& jval, std::string& stream,
               const std::string& lineBreak,
               const std::string& indent,
               size_t indentLevel)
{
    //unlike our XML serialization the caller is repsonsible for line breaks and indentation of *first* line
    auto writeIndent = [&](size_t level)
    {
        for (size_t i = 0; i < level; ++i)
            stream += indent;
    };

    switch (jval.type)
    {
        case JsonValue::Type::null:
            stream += "null";
            break;

        case JsonValue::Type::boolean:
        case JsonValue::Type::number:
            stream += jval.primVal;
            break;

        case JsonValue::Type::string:
            stream += '"' + jsonEscape(jval.primVal) + '"';
            break;

        case JsonValue::Type::object:
            stream += '{';
            if (!jval.objectVal.empty())
            {
                for (auto it = jval.objectVal.begin(); it != jval.objectVal.end(); ++it)
                {
                    const auto& [childName, childValue] = *it;

                    if (it != jval.objectVal.begin())
                        stream += ',';

                    stream += lineBreak;
                    writeIndent(indentLevel + 1);

                    stream += '"' + jsonEscape(childName) + "\":";

                    if ((childValue.type == JsonValue::Type::object && !childValue.objectVal.empty()) ||
                        (childValue.type == JsonValue::Type::array  && !childValue.arrayVal .empty()))
                    {
                        stream += lineBreak;
                        writeIndent(indentLevel + 1);
                    }
                    else if (!indent.empty())
                        stream += ' ';

                    serialize(childValue, stream, lineBreak, indent, indentLevel + 1);
                }
                stream += lineBreak;
                writeIndent(indentLevel);
            }
            stream += '}';
            break;

        case JsonValue::Type::array:
            stream += '[';
            if (!jval.arrayVal.empty())
            {
                for (auto it = jval.arrayVal.begin(); it != jval.arrayVal.end(); ++it)
                {
                    const auto& childValue = *it;

                    if (it != jval.arrayVal.begin())
                        stream += ',';

                    stream += lineBreak;
                    writeIndent(indentLevel + 1);

                    serialize(childValue, stream, lineBreak, indent, indentLevel + 1);
                }
                stream += lineBreak;
                writeIndent(indentLevel);
            }
            stream += ']';
            break;
    }
}
}
}


inline
std::string serializeJson(const JsonValue& jval,
                          const std::string& lineBreak,
                          const std::string& indent) //noexcept
{
    std::string output;
    json_impl::serialize(jval, output, lineBreak, indent, 0);
    output += lineBreak;
    return output;
}


namespace json_impl
{
enum class TokenType
{
    eof,
    curlyOpen,
    curlyClose,
    squareOpen,
    squareClose,
    colon,
    comma,
    string,  //
    number,  //primitive types
    boolean, //
    null,    //
};

struct Token
{
    Token(TokenType t) : type(t) {}

    TokenType type;
    std::string primVal; //for primitive types
};

class Scanner
{
public:
    explicit Scanner(const std::string& stream) : stream_(stream), pos_(stream_.begin())
    {
        if (zen::startsWith(stream_, BYTE_ORDER_MARK_UTF8))
            pos_ += BYTE_ORDER_MARK_UTF8.size();
    }

    Token getNextToken() //throw JsonParsingError
    {
        //skip whitespace
        pos_ = std::find_if_not(pos_, stream_.end(), isJsonWhiteSpace);

        if (pos_ == stream_.end())
            return TokenType::eof;

        if (*pos_ == '{') return ++pos_, TokenType::curlyOpen;
        if (*pos_ == '}') return ++pos_, TokenType::curlyClose;
        if (*pos_ == '[') return ++pos_, TokenType::squareOpen;
        if (*pos_ == ']') return ++pos_, TokenType::squareClose;
        if (*pos_ == ':') return ++pos_, TokenType::colon;
        if (*pos_ == ',') return ++pos_, TokenType::comma;
        if (startsWith("null")) return pos_ += 4, Token(TokenType::null);

        if (startsWith("true"))
        {
            pos_ += 4;
            Token tk(TokenType::boolean);
            tk.primVal = "true";
            return tk;
        }
        if (startsWith("false"))
        {
            pos_ += 5;
            Token tk(TokenType::boolean);
            tk.primVal = "false";
            return tk;
        }

        if (*pos_ == '"')
        {
            for (auto it = ++pos_; it != stream_.end(); ++it)
                if (*it == '"')
                {
                    Token tk(TokenType::string);
                    tk.primVal = jsonUnescape({pos_, it});
                    pos_ = ++it;
                    return tk;
                }
                else if (*it == '\\') //skip next char
                    if (++it == stream_.end())
                        break;

            throw JsonParsingError(posRow(), posCol());
        }

        //expect a number:
        const auto itNumEnd = std::find_if_not(pos_, stream_.end(), isJsonNumDigit);
        if (itNumEnd == pos_)
            throw JsonParsingError(posRow(), posCol());

        Token tk(TokenType::number);
        tk.primVal.assign(pos_, itNumEnd);
        pos_ = itNumEnd;
        return tk;
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

    static bool isJsonWhiteSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
    static bool isJsonNumDigit  (char c) { return ('0' <= c && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e'|| c == 'E'; }

    bool startsWith(const std::string& prefix) const
    {
        return zen::startsWith(makeStringView(pos_, stream_.end()), prefix);
    }

    const std::string stream_;
    std::string::const_iterator pos_;
};


class JsonParser
{
public:
    explicit JsonParser(const std::string& stream) :
        scn_(stream),
        tk_(scn_.getNextToken()) {} //throw JsonParsingError

    JsonValue parse() //throw JsonParsingError
    {
        JsonValue jval = parseValue(); //throw JsonParsingError
        expectToken(TokenType::eof); //
        return jval;
    }

private:
    JsonParser           (const JsonParser&) = delete;
    JsonParser& operator=(const JsonParser&) = delete;

    JsonValue parseValue() //throw JsonParsingError
    {
        if (token().type == TokenType::curlyOpen)
        {
            nextToken(); //throw JsonParsingError

            JsonValue jval(JsonValue::Type::object);

            if (token().type != TokenType::curlyClose)
                for (;;)
                {
                    expectToken(TokenType::string); //throw JsonParsingError
                    std::string name = token().primVal;
                    nextToken(); //throw JsonParsingError

                    consumeToken(TokenType::colon); //throw JsonParsingError

                    JsonValue value = parseValue(); //throw JsonParsingError
                    jval.objectVal.emplace(std::move(name), std::move(value));

                    if (token().type != TokenType::comma)
                        break;
                    nextToken(); //throw JsonParsingError
                }

            consumeToken(TokenType::curlyClose); //throw JsonParsingError
            return jval;
        }
        else if (token().type == TokenType::squareOpen)
        {
            nextToken(); //throw JsonParsingError

            JsonValue jval(JsonValue::Type::array);

            if (token().type != TokenType::squareClose)
                for (;;)
                {
                    JsonValue value = parseValue();  //throw JsonParsingError
                    jval.arrayVal.emplace_back(std::move(value));

                    if (token().type != TokenType::comma)
                        break;
                    nextToken(); //throw JsonParsingError
                }

            consumeToken(TokenType::squareClose); //throw JsonParsingError
            return jval;
        }
        else if (token().type == TokenType::string)
        {
            JsonValue jval(token().primVal);
            nextToken(); //throw JsonParsingError
            return jval;
        }
        else if (token().type == TokenType::number)
        {
            JsonValue jval(JsonValue::Type::number);
            jval.primVal = token().primVal;
            nextToken(); //throw JsonParsingError
            return jval;
        }
        else if (token().type == TokenType::boolean)
        {
            JsonValue jval(JsonValue::Type::boolean);
            jval.primVal = token().primVal;
            nextToken(); //throw JsonParsingError
            return jval;
        }
        else if (token().type == TokenType::null)
        {
            nextToken(); //throw JsonParsingError
            return JsonValue();
        }
        else //unexpected token
            throw JsonParsingError(scn_.posRow(), scn_.posCol());
    }

    const Token& token() const { return tk_; }

    void nextToken() { tk_ = scn_.getNextToken(); } //throw JsonParsingError

    void expectToken(TokenType t) //throw JsonParsingError
    {
        if (token().type != t)
            throw JsonParsingError(scn_.posRow(), scn_.posCol());
    }

    void consumeToken(TokenType t) //throw JsonParsingError
    {
        expectToken(t); //throw JsonParsingError
        nextToken();    //
    }

    Scanner scn_;
    Token tk_;
};
}

inline
JsonValue parseJson(const std::string& stream) //throw JsonParsingError
{
    return json_impl::JsonParser(stream).parse(); //throw JsonParsingError
}
}

#endif //JSON_H_0187348321748321758934215734
