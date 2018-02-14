// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "http.h"

    #include <wx/app.h>
    #include <zen/thread.h> //std::thread::id
    #include <wx/protocol/http.h>

using namespace zen;


namespace
{

struct UrlRedirectError
{
    UrlRedirectError(const std::wstring& url) : newUrl(url) {}
    std::wstring newUrl;
};
}


class HttpInputStream::Impl
{
public:
    Impl(const std::wstring& url, const std::wstring& userAgent, const IOCallback& notifyUnbufferedIO, //throw SysError, UrlRedirectError
         const std::string* postParams) : //issue POST if bound, GET otherwise
        notifyUnbufferedIO_(notifyUnbufferedIO)
    {
        ZEN_ON_SCOPE_FAIL( cleanup(); /*destructor call would lead to member double clean-up!!!*/ );

        //assert(!startsWith(url, L"https:", CmpAsciiNoCase())); //not supported by wxHTTP!

        const std::wstring urlFmt =         afterFirst(url, L"://", IF_MISSING_RETURN_NONE);
        const std::wstring server =       beforeFirst(urlFmt, L'/', IF_MISSING_RETURN_ALL);
        const std::wstring page   = L'/' + afterFirst(urlFmt, L'/', IF_MISSING_RETURN_NONE);

        assert(std::this_thread::get_id() == mainThreadId);
        assert(wxApp::IsMainLoopRunning());

        webAccess_.SetHeader(L"User-Agent", userAgent);
        webAccess_.SetTimeout(10 /*[s]*/); //default: 10 minutes: WTF are these wxWidgets people thinking???

        if (!webAccess_.Connect(server)) //will *not* fail for non-reachable url here!
            throw SysError(L"wxHTTP::Connect");

        if (postParams)
            if (!webAccess_.SetPostText(L"application/x-www-form-urlencoded", utfTo<wxString>(*postParams)))
                throw SysError(L"wxHTTP::SetPostText");

        httpStream_.reset(webAccess_.GetInputStream(page)); //pass ownership
        const int sc = webAccess_.GetResponse();

        //http://en.wikipedia.org/wiki/List_of_HTTP_status_codes#3xx_Redirection
        if (sc / 100 == 3) //e.g. 301, 302, 303, 307... we're not too greedy since we check location, too!
        {
            const std::wstring newUrl(webAccess_.GetHeader(L"Location"));
            if (newUrl.empty())
                throw SysError(L"Unresolvable redirect. Empty target Location.");

            throw UrlRedirectError(newUrl);
        }

        if (sc != 200) //HTTP_STATUS_OK
            throw SysError(replaceCpy<std::wstring>(L"HTTP status code %x.", L"%x", numberTo<std::wstring>(sc)));

        if (!httpStream_ || webAccess_.GetError() != wxPROTO_NOERR)
            throw SysError(L"wxHTTP::GetError (" + numberTo<std::wstring>(webAccess_.GetError()) + L")");
    }

    ~Impl() { cleanup(); }

    size_t read(void* buffer, size_t bytesToRead) //throw SysError, X; return "bytesToRead" bytes unless end of stream!
    {
        const size_t blockSize = getBlockSize();
        assert(memBuf_.size() >= blockSize);
        assert(bufPos_ <= bufPosEnd_ && bufPosEnd_ <= memBuf_.size());

        char*       it    = static_cast<char*>(buffer);
        char* const itEnd = it + bytesToRead;
        for (;;)
        {
            const size_t junkSize = std::min(static_cast<size_t>(itEnd - it), bufPosEnd_ - bufPos_);
            std::memcpy(it, &memBuf_[0] + bufPos_, junkSize);
            bufPos_ += junkSize;
            it      += junkSize;

            if (it == itEnd)
                break;
            //--------------------------------------------------------------------
            const size_t bytesRead = tryRead(&memBuf_[0], blockSize); //throw SysError; may return short, only 0 means EOF! => CONTRACT: bytesToRead > 0
            bufPos_ = 0;
            bufPosEnd_ = bytesRead;

            if (notifyUnbufferedIO_) notifyUnbufferedIO_(bytesRead); //throw X

            if (bytesRead == 0) //end of file
                break;
        }
        return it - static_cast<char*>(buffer);
    }

    size_t getBlockSize() const { return 64 * 1024; }

private:
    size_t tryRead(void* buffer, size_t bytesToRead) //throw SysError; may return short, only 0 means EOF!
    {
        if (bytesToRead == 0) //"read() with a count of 0 returns zero" => indistinguishable from end of file! => check!
            throw std::logic_error("Contract violation! " + std::string(__FILE__) + ":" + numberTo<std::string>(__LINE__));
        assert(bytesToRead == getBlockSize());

        httpStream_->Read(buffer, bytesToRead);

        const wxStreamError ec = httpStream_->GetLastError();
        if (ec != wxSTREAM_NO_ERROR && ec != wxSTREAM_EOF)
            throw SysError(L"wxInputStream::GetLastError (" + numberTo<std::wstring>(httpStream_->GetLastError()) + L")");

        const size_t bytesRead = httpStream_->LastRead();
        //"if there are not enough bytes in the stream right now, LastRead() value will be
        // less than size but greater than 0. If it is 0, it means that EOF has been reached."
        assert(bytesRead > 0 || ec == wxSTREAM_EOF);
        if (bytesRead > bytesToRead) //better safe than sorry
            throw SysError(L"InternetReadFile: buffer overflow.");

        return bytesRead; //"zero indicates end of file"
    }

    Impl           (const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    void cleanup()
    {
    }

    wxHTTP webAccess_;
    std::unique_ptr<wxInputStream> httpStream_; //must be deleted BEFORE webAccess is closed

    const IOCallback notifyUnbufferedIO_; //throw X

    std::vector<char> memBuf_ = std::vector<char>(getBlockSize());
    size_t bufPos_    = 0; //buffered I/O; see file_io.cpp
    size_t bufPosEnd_ = 0; //
};


HttpInputStream::HttpInputStream(std::unique_ptr<Impl>&& pimpl) : pimpl_(std::move(pimpl)) {}

HttpInputStream::~HttpInputStream() {}

size_t HttpInputStream::read(void* buffer, size_t bytesToRead) { return pimpl_->read(buffer, bytesToRead); } //throw SysError, X; return "bytesToRead" bytes unless end of stream!

size_t HttpInputStream::getBlockSize() const { return pimpl_->getBlockSize(); }

std::string HttpInputStream::readAll() { return bufferedLoad<std::string>(*pimpl_); } //throw SysError, X;

namespace
{
std::unique_ptr<HttpInputStream::Impl> sendHttpRequestImpl(const std::wstring& url, const std::wstring& userAgent, const IOCallback& notifyUnbufferedIO, //throw SysError
                                                           const std::string* postParams) //issue POST if bound, GET otherwise
{
    std::wstring urlRed = url;
    //"A user agent should not automatically redirect a request more than five times, since such redirections usually indicate an infinite loop."
    for (int redirects = 0; redirects < 6; ++redirects)
        try
        {
            return std::make_unique<HttpInputStream::Impl>(urlRed, userAgent, notifyUnbufferedIO, postParams); //throw SysError, UrlRedirectError
        }
        catch (const UrlRedirectError& e) { urlRed = e.newUrl; }
    throw SysError(L"Too many redirects.");
}


//encode into "application/x-www-form-urlencoded"
std::string urlencode(const std::string& str)
{
    std::string out;
    for (const char c : str) //follow PHP spec: https://github.com/php/php-src/blob/master/ext/standard/url.c#L500
        if (c == ' ')
            out += '+';
        else if (('0' <= c && c <= '9') ||
                 ('A' <= c && c <= 'Z') ||
                 ('a' <= c && c <= 'z') ||
                 c == '-' || c == '.' || c == '_') //note: "~" is encoded by PHP!
            out += c;
        else
        {
            const std::pair<char, char> hex = hexify(c);

            out += '%';
            out += hex.first;
            out += hex.second;
        }
    return out;
}


std::string urldecode(const std::string& str)
{
    std::string out;
    for (size_t i = 0; i < str.size(); ++i)
    {
        const char c = str[i];
        if (c == '+')
            out += ' ';
        else if (c == '%' && str.size() - i >= 3 &&
                 isHexDigit(str[i + 1]) &&
                 isHexDigit(str[i + 2]))
        {
            out += unhexify(str[i + 1], str[i + 2]);
            i += 2;
        }
        else
            out += c;
    }
    return out;
}
}


std::string zen::xWwwFormUrlEncode(const std::vector<std::pair<std::string, std::string>>& paramPairs)
{
    std::string output;
    for (const auto& pair : paramPairs)
        output += urlencode(pair.first) + '=' + urlencode(pair.second) + '&';
    //encode both key and value: https://www.w3.org/TR/html401/interact/forms.html#h-17.13.4.1
    if (!output.empty())
        output.pop_back();
    return output;
}


std::vector<std::pair<std::string, std::string>> zen::xWwwFormUrlDecode(const std::string& str)
{
    std::vector<std::pair<std::string, std::string>> output;

    for (const std::string& nvPair : split(str, '&', SplitType::SKIP_EMPTY))
        output.emplace_back(urldecode(beforeFirst(nvPair, '=', IF_MISSING_RETURN_ALL)),
                            urldecode(afterFirst (nvPair, '=', IF_MISSING_RETURN_NONE)));
    return output;
}


HttpInputStream zen::sendHttpPost(const std::wstring& url, const std::wstring& userAgent, const IOCallback& notifyUnbufferedIO,
                                  const std::vector<std::pair<std::string, std::string>>& postParams) //throw SysError
{
    const std::string encodedParams = xWwwFormUrlEncode(postParams);
    return sendHttpRequestImpl(url, userAgent, notifyUnbufferedIO, &encodedParams); //throw SysError
}


HttpInputStream zen::sendHttpGet(const std::wstring& url, const std::wstring& userAgent, const IOCallback& notifyUnbufferedIO) //throw SysError
{
    return sendHttpRequestImpl(url, userAgent, notifyUnbufferedIO, nullptr); //throw SysError
}


bool zen::internetIsAlive() //noexcept
{
    assert(std::this_thread::get_id() == mainThreadId);

    const wxString server = L"www.google.com";
    const wxString page   = L"/";

    wxHTTP webAccess;
    webAccess.SetTimeout(10 /*[s]*/); //default: 10 minutes: WTF are these wxWidgets people thinking???

    if (!webAccess.Connect(server)) //will *not* fail for non-reachable url here!
        return false;

    std::unique_ptr<wxInputStream> httpStream(webAccess.GetInputStream(page)); //call before checking wxHTTP::GetResponse()
    const int sc = webAccess.GetResponse();
    //attention: http://www.google.com/ might redirect to "https" => don't follow, just return "true"!!!
    return sc / 100 == 2 || //e.g. 200
           sc / 100 == 3;   //e.g. 301, 302, 303, 307... when in doubt, consider internet alive!
}
