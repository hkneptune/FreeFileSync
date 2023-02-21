// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#ifndef PARSE_PLURAL_H_180465845670839576
#define PARSE_PLURAL_H_180465845670839576

#include <zen/string_tools.h>


namespace plural
{
//expression interface
struct Expression { virtual ~Expression() {} };

template <class T>
struct Expr : public Expression
{
    virtual T eval() const = 0;
};


class ParsingError {};

class PluralForm
{
public:
    explicit PluralForm(const std::string& stream); //throw ParsingError
    size_t getForm(int64_t n) const { n_ = std::abs(n) ; return static_cast<size_t>(expr_->eval()); }

private:
    std::shared_ptr<Expr<int64_t>> expr_;
    mutable int64_t n_ = 0;
};


//validate plural form
class InvalidPluralForm {};

class PluralFormInfo
{
public:
    PluralFormInfo(const std::string& definition, int pluralCount); //throw InvalidPluralForm

    size_t getCount() const { return forms_.size(); }
    bool isSingleNumberForm(size_t index) const { return index < forms_.size() ? forms_[index].count == 1 : false; }
    int  getFirstNumber    (size_t index) const { return index < forms_.size() ? forms_[index].firstNumber : -1; }

private:
    struct FormInfo
    {
        int count       = 0;
        int firstNumber = 0; //which maps to the plural form index position
    };
    std::vector<FormInfo> forms_;
};





//--------------------------- implementation ---------------------------
/*  https://www.gnu.org/software/hello/manual/gettext/Plural-forms.html
    https://translate.sourceforge.net/wiki/l10n/pluralforms

    Grammar for Plural forms parser
    -------------------------------
    expression:
        conditional-expression

    conditional-expression:
        logical-or-expression
        logical-or-expression ? expression : expression

    logical-or-expression:
        logical-and-expression
        logical-or-expression || logical-and-expression

    logical-and-expression:
        equality-expression
        logical-and-expression && equality-expression

    equality-expression:
        relational-expression
        relational-expression == relational-expression
        relational-expression != relational-expression

    relational-expression:
        multiplicative-expression
        multiplicative-expression >  multiplicative-expression
        multiplicative-expression <  multiplicative-expression
        multiplicative-expression >= multiplicative-expression
        multiplicative-expression <= multiplicative-expression

    multiplicative-expression:
        pm-expression
        multiplicative-expression % pm-expression

    pm-expression:
        variable-number-n-expression
        constant-number-expression
        ( expression )


    .po format,e.g.: (n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2)             */

namespace impl
{
template <class BinaryOp, class ParamType, class ResultType>
struct BinaryExp : public Expr<ResultType>
{
    using ExpLhs = std::shared_ptr<Expr<ParamType>>;
    using ExpRhs = std::shared_ptr<Expr<ParamType>>;

    BinaryExp(const ExpLhs& lhs, const ExpRhs& rhs) : lhs_(lhs), rhs_(rhs) { assert(lhs && rhs); }
    ResultType eval() const override { return BinaryOp()(lhs_->eval(), rhs_->eval()); }
private:
    ExpLhs lhs_;
    ExpRhs rhs_;
};


template <class BinaryOp, class ParamType> inline
std::shared_ptr<Expression> makeBiExp(const std::shared_ptr<Expression>& lhs, const std::shared_ptr<Expression>& rhs) //throw ParsingError
{
    auto exLeft  = std::dynamic_pointer_cast<Expr<ParamType>>(lhs);
    auto exRight = std::dynamic_pointer_cast<Expr<ParamType>>(rhs);
    if (!exLeft || !exRight)
        throw ParsingError();

    using ResultType = decltype(BinaryOp()(std::declval<ParamType>(), std::declval<ParamType>()));
    return std::make_shared<BinaryExp<BinaryOp, ParamType, ResultType>>(exLeft, exRight);
}


template <class T>
struct ConditionalExp : public Expr<T>
{
    ConditionalExp(const std::shared_ptr<Expr<bool>>& ifExp,
                   const std::shared_ptr<Expr<T>>& thenExp,
                   const std::shared_ptr<Expr<T>>& elseExp) : ifExp_(ifExp), thenExp_(thenExp), elseExp_(elseExp) { assert(ifExp && thenExp && elseExp); }

    T eval() const override { return ifExp_->eval() ? thenExp_->eval() : elseExp_->eval(); }
private:
    std::shared_ptr<Expr<bool>> ifExp_;
    std::shared_ptr<Expr<T>> thenExp_;
    std::shared_ptr<Expr<T>> elseExp_;
};


struct ConstNumberExp : public Expr<int64_t>
{
    ConstNumberExp(int64_t n) : n_(n) {}
    int64_t eval() const override { return n_; }
private:
    int64_t n_;
};


struct VariableNumberNExp : public Expr<int64_t>
{
    VariableNumberNExp(int64_t& n) : n_(n) {}
    int64_t eval() const override { return n_; }
private:
    int64_t& n_;
};

//-------------------------------------------------------------------------------

enum class TokenType
{
    ternaryQuest,
    ternaryColon,
    logicOr,
    logicAnd,
    equal,
    notEqual,
    less,
    lessEqual,
    greater,
    greaterEqual,
    modulus,
    variableN,
    constNumber,
    bracketLeft,
    bracketRight,
    end,
};

struct Token
{
    Token(TokenType t) : type(t) {}
    Token(int64_t num) : number(num) {}

    TokenType type = TokenType::constNumber;
    int64_t number = 0; //if type == TokenType::constNumber
};

class Scanner
{
public:
    explicit Scanner(const std::string& stream) : stream_(stream), pos_(stream_.begin()) {}

    Token getNextToken() //throw ParsingError
    {
        //skip whitespace
        pos_ = std::find_if_not(pos_, stream_.end(), zen::isWhiteSpace<char>);

        if (pos_ == stream_.end())
            return TokenType::end;

        for (const auto& [tokenString, tokenEnum] : tokens_)
            if (startsWith(tokenString))
            {
                pos_ += tokenString.size();
                return Token(tokenEnum);
            }

        auto digitEnd = std::find_if_not(pos_, stream_.end(), zen::isDigit<char>);
        if (pos_ == digitEnd)
            throw ParsingError(); //unknown token

        auto number = zen::stringTo<int64_t>(std::string(pos_, digitEnd));
        pos_ = digitEnd;
        return number;
    }

private:
    bool startsWith(const std::string& prefix) const
    {
        return zen::startsWith(zen::makeStringView(pos_, stream_.end()), prefix);
    }

    using TokenList = std::vector<std::pair<std::string, TokenType>>;
    const TokenList tokens_
    {
        {"?",  TokenType::ternaryQuest},
        {":",  TokenType::ternaryColon},
        {"||", TokenType::logicOr     },
        {"&&", TokenType::logicAnd    },
        {"==", TokenType::equal       },
        {"!=", TokenType::notEqual    },
        {"<=", TokenType::lessEqual   },
        {"<",  TokenType::less        },
        {">=", TokenType::greaterEqual},
        {">",  TokenType::greater     },
        {"%",  TokenType::modulus     },
        {"n",  TokenType::variableN   },
        {"N",  TokenType::variableN   },
        {"(",  TokenType::bracketLeft },
        {")",  TokenType::bracketRight},
    };

    const std::string stream_;
    std::string::const_iterator pos_;
};

//-------------------------------------------------------------------------------

class Parser
{
public:
    Parser(const std::string& stream, int64_t& n) :
        scn_(stream),
        tk_(scn_.getNextToken()), //throw ParsingError
        n_(n) {}

    std::shared_ptr<Expr<int64_t>> parse() //throw ParsingError; return value always bound!
    {
        auto e = std::dynamic_pointer_cast<Expr<int64_t>>(parseExpression()); //throw ParsingError
        if (!e)
            throw ParsingError();
        expectToken(TokenType::end); //throw ParsingError
        return e;
    }

private:
    std::shared_ptr<Expression> parseExpression() { return parseConditional(); }//throw ParsingError

    std::shared_ptr<Expression> parseConditional() //throw ParsingError
    {
        std::shared_ptr<Expression> e = parseLogicalOr();

        if (token().type == TokenType::ternaryQuest)
        {
            nextToken(); //throw ParsingError

            auto ifExp   = std::dynamic_pointer_cast<Expr<bool>>(e);
            auto thenExp = std::dynamic_pointer_cast<Expr<int64_t>>(parseExpression()); //associativity: <-

            consumeToken(TokenType::ternaryColon); //throw ParsingError

            auto elseExp = std::dynamic_pointer_cast<Expr<int64_t>>(parseExpression()); //
            if (!ifExp || !thenExp || !elseExp)
                throw ParsingError();
            return std::make_shared<ConditionalExp<int64_t>>(ifExp, thenExp, elseExp);
        }
        return e;
    }

    std::shared_ptr<Expression> parseLogicalOr()
    {
        std::shared_ptr<Expression> e = parseLogicalAnd();
        while (token().type == TokenType::logicOr) //associativity: ->
        {
            nextToken(); //throw ParsingError

            std::shared_ptr<Expression> rhs = parseLogicalAnd();
            e = makeBiExp<std::logical_or<>, bool>(e, rhs); //throw ParsingError
        }
        return e;
    }

    std::shared_ptr<Expression> parseLogicalAnd()
    {
        std::shared_ptr<Expression> e = parseEquality();
        while (token().type == TokenType::logicAnd) //associativity: ->
        {
            nextToken(); //throw ParsingError
            std::shared_ptr<Expression> rhs = parseEquality();

            e = makeBiExp<std::logical_and<>, bool>(e, rhs); //throw ParsingError
        }
        return e;
    }

    std::shared_ptr<Expression> parseEquality()
    {
        std::shared_ptr<Expression> e = parseRelational();

        TokenType t = token().type;
        if (t == TokenType::equal || //associativity: n/a
            t == TokenType::notEqual)
        {
            nextToken(); //throw ParsingError
            std::shared_ptr<Expression> rhs = parseRelational();

            if (t == TokenType::equal)    return makeBiExp<std::    equal_to<>, int64_t>(e, rhs); //throw ParsingError
            if (t == TokenType::notEqual) return makeBiExp<std::not_equal_to<>, int64_t>(e, rhs); //
        }
        return e;
    }

    std::shared_ptr<Expression> parseRelational()
    {
        std::shared_ptr<Expression> e = parseMultiplicative();

        TokenType t = token().type;
        if (t == TokenType::less      || //associativity: n/a
            t == TokenType::lessEqual ||
            t == TokenType::greater   ||
            t == TokenType::greaterEqual)
        {
            nextToken(); //throw ParsingError
            std::shared_ptr<Expression> rhs = parseMultiplicative();

            if (t == TokenType::less)         return makeBiExp<std::less         <>, int64_t>(e, rhs); //
            if (t == TokenType::lessEqual)    return makeBiExp<std::less_equal   <>, int64_t>(e, rhs); //throw ParsingError
            if (t == TokenType::greater)      return makeBiExp<std::greater      <>, int64_t>(e, rhs); //
            if (t == TokenType::greaterEqual) return makeBiExp<std::greater_equal<>, int64_t>(e, rhs); //
        }
        return e;
    }

    std::shared_ptr<Expression> parseMultiplicative()
    {
        std::shared_ptr<Expression> e = parsePrimary();

        while (token().type == TokenType::modulus) //associativity: ->
        {
            nextToken(); //throw ParsingError
            std::shared_ptr<Expression> rhs = parsePrimary();

            //"compile-time" check: n % 0
            if (auto literal = std::dynamic_pointer_cast<ConstNumberExp>(rhs))
                if (literal->eval() == 0)
                    throw ParsingError();

            e = makeBiExp<std::modulus<>, int64_t>(e, rhs); //throw ParsingError
        }
        return e;
    }

    std::shared_ptr<Expression> parsePrimary()
    {
        if (token().type == TokenType::variableN)
        {
            nextToken(); //throw ParsingError
            return std::make_shared<VariableNumberNExp>(n_);
        }
        else if (token().type == TokenType::constNumber)
        {
            const int64_t number = token().number;
            nextToken(); //throw ParsingError
            return std::make_shared<ConstNumberExp>(number);
        }
        else if (token().type == TokenType::bracketLeft)
        {
            nextToken(); //throw ParsingError
            std::shared_ptr<Expression> e = parseExpression();

            expectToken(TokenType::bracketRight); //throw ParsingError
            nextToken();                          //
            return e;
        }
        else
            throw ParsingError();
    }

    const Token& token() const { return tk_; }

    void nextToken() { tk_ = scn_.getNextToken(); } //throw ParsingError

    void expectToken(TokenType t) //throw ParsingError
    {
        if (token().type != t)
            throw ParsingError();
    }

    void consumeToken(TokenType t) //throw ParsingError
    {
        expectToken(t); //throw ParsingError
        nextToken();
    }

    Scanner scn_;
    Token tk_;
    int64_t& n_;
};
}


inline
PluralFormInfo::PluralFormInfo(const std::string& definition, int pluralCount) //throw InvalidPluralForm
{
    if (pluralCount < 1)
        throw InvalidPluralForm();

    forms_.resize(pluralCount);
    try
    {
        PluralForm pf(definition); //throw ParsingError
        //PERF_START

        //perf: 80ns per iteration max (for arabic)
        //=> 1000 iterations should be fast enough and still detect all "single number forms"
        for (int j = 0; j < 1000; ++j)
            if (const size_t formNo = pf.getForm(j);
                formNo < forms_.size())
            {
                if (forms_[formNo].count == 0)
                    forms_[formNo].firstNumber = j;
                ++forms_[formNo].count;
            }
            else
                throw InvalidPluralForm();
    }
    catch (const plural::ParsingError&)
    {
        throw InvalidPluralForm();
    }

    //ensure each form is used at least once:
    if (!std::all_of(forms_.begin(), forms_.end(), [](const FormInfo& fi) { return fi.count >= 1; }))
    throw InvalidPluralForm();
}


inline
PluralForm::PluralForm(const std::string& stream) : expr_(impl::Parser(stream, n_).parse()) {} //throw ParsingError
}

#endif //PARSE_PLURAL_H_180465845670839576
