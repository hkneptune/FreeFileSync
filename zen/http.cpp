// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "http.h"

    #include "socket.h"
    #include "open_ssl.h"

using namespace zen;


class HttpInputStream::Impl
{
public:
    Impl(const Zstring& url,
         const std::vector<std::pair<std::string, std::string>>* postParams, //issue POST if bound, GET otherwise
         bool disableGetCache /*not relevant for POST (= never cached)*/,
         const Zstring& userAgent,
         const Zstring* caCertFilePath /*optional: enable certificate validation*/,
         const IOCallback& notifyUnbufferedIO) : //throw SysError
        notifyUnbufferedIO_(notifyUnbufferedIO)
    {
        ZEN_ON_SCOPE_FAIL(cleanup(); /*destructor call would lead to member double clean-up!!!*/);

        const Zstring urlFmt =              afterFirst(url, Zstr("://"), IF_MISSING_RETURN_NONE);
        const Zstring server =            beforeFirst(urlFmt, Zstr('/'), IF_MISSING_RETURN_ALL);
        const Zstring page   = Zstr('/') + afterFirst(urlFmt, Zstr('/'), IF_MISSING_RETURN_NONE);

        const bool useTls = [&]
        {
            if (startsWithAsciiNoCase(url, Zstr("http://")))
                return false;
            if (startsWithAsciiNoCase(url, Zstr("https://")))
                return true;
            throw SysError(L"URL uses unexpected protocol.");
        }();

        if (useTls) //HTTP default port: 443, see %WINDIR%\system32\drivers\etc\services
        {
            socket_ = std::make_unique<Socket>(server, Zstr("https")); //throw SysError
            tlsCtx_ = std::make_unique<TlsContext>(socket_->get(), server, caCertFilePath); //throw SysError
        }
        else //HTTP default port: 80, see %WINDIR%\system32\drivers\etc\services
            socket_ = std::make_unique<Socket>(server, Zstr("http")); //throw SysError

		//we don't support "chunked and gzip transfer encoding" => HTTP 1.0
        std::map<std::string, std::string, LessAsciiNoCase> headers;
        headers["Host"      ] = utfTo<std::string>(server); //only required for HTTP/1.1 but a few servers expect it even for HTTP/1.0
        headers["User-Agent"] = utfTo<std::string>(userAgent);
        headers["Accept"    ] = "*/*"; //won't hurt?

        const std::string postBuf = postParams ? xWwwFormUrlEncode(*postParams) : "";

        if (!postParams /*HTTP GET*/ && disableGetCache)
            headers["Pragma"] = "no-cache"; //HTTP 1.0 only! superseeded by "Cache-Control"
        else //HTTP POST
        {
            headers["Content-Type"] = "application/x-www-form-urlencoded";
            headers["Content-Length"] = numberTo<std::string>(postBuf.size());
        }

        //https://www.w3.org/Protocols/HTTP/1.0/spec.html#Request-Line
        std::string msg = (postParams ? "POST " : "GET ") + utfTo<std::string>(page) + " HTTP/1.0\r\n";
        for (const auto& [name, value] : headers)
            msg += name + ": " + value + "\r\n";
        msg += "\r\n";
        msg += postBuf;

        //send request
        for (size_t bytesToSend = msg.size(); bytesToSend > 0;)
            bytesToSend -= tlsCtx_ ?
                           tlsCtx_->tryWrite(             &*(msg.end() - bytesToSend), bytesToSend) : //throw SysError
                           tryWriteSocket(socket_->get(), &*(msg.end() - bytesToSend), bytesToSend);  //throw SysError

        //shutdownSocketSend(socket_->get()); //throw SysError
        //NO! Sending TCP FIN before receiving response (aka "TCP Half Closed") is not always supported! e.g. Cloudflare server will immediately end connection: recv() returns 0.
        //"clients SHOULD NOT half-close their TCP connections": https://github.com/httpwg/http-core/issues/22

        //receive response:
        std::string headBuf;
        const std::string headerDelim = "\r\n\r\n";
        for (std::string buf;;)
        {
            const size_t blockSize = std::min(static_cast<size_t>(1024), memBuf_.size()); //smaller block size: try to only read header part
            buf.resize(buf.size() + blockSize);
            const size_t bytesReceived = tryRead(&*(buf.end() - blockSize), blockSize); //throw SysError
            buf.resize(buf.size() - blockSize + bytesReceived); //caveat: unsigned arithmetics

            if (contains(buf, headerDelim))
            {
                headBuf                   = beforeFirst(buf, headerDelim, IF_MISSING_RETURN_NONE);
                const std::string bodyBuf = afterFirst (buf, headerDelim, IF_MISSING_RETURN_NONE);
                //put excess bytes into instance buffer for body retrieval
                assert(bufPos_ == 0 && bufPosEnd_ == 0);
                bufPosEnd_ = bodyBuf.size();
                std::copy(bodyBuf.begin(), bodyBuf.end(), reinterpret_cast<char*>(&memBuf_[0]));
                break;
            }
            if (bytesReceived == 0)
                break;
        }
        //parse header
        const std::string statusBuf  = beforeFirst(headBuf, "\r\n", IF_MISSING_RETURN_ALL);
        const std::string headersBuf = afterFirst (headBuf, "\r\n", IF_MISSING_RETURN_NONE);

        const std::vector<std::string> statusItems = split(statusBuf, ' ', SplitType::ALLOW_EMPTY); //HTTP-Version SP Status-Code SP Reason-Phrase CRLF
        if (statusItems.size() < 2 || !startsWith(statusItems[0], "HTTP/"))
            throw SysError(L"Invalid HTTP response: \"" + utfTo<std::wstring>(statusBuf) + L'"');

        statusCode_ = stringTo<int>(statusItems[1]);

        for (const std::string& line : split(headersBuf, "\r\n", SplitType::SKIP_EMPTY))
            responseHeaders_[trimCpy(beforeFirst(line, ":", IF_MISSING_RETURN_ALL))] =
                /**/         trimCpy(afterFirst (line, ":", IF_MISSING_RETURN_NONE));

        //try to get "Content-Length" header if available
        if (const std::string* value = getHeader("Content-Length"))
            contentRemaining_ = stringTo<int64_t>(*value) - (bufPosEnd_ - bufPos_);
    }

    ~Impl() { cleanup(); }


    const int getStatusCode() const { return statusCode_; }

    const std::string* getHeader(const std::string& name) const
    {
        auto it = responseHeaders_.find(name);
        return it != responseHeaders_.end() ? &it->second : nullptr;
    }

    size_t read(void* buffer, size_t bytesToRead) //throw SysError, X; return "bytesToRead" bytes unless end of stream!
    {
        const size_t blockSize = getBlockSize();
        assert(memBuf_.size() >= blockSize);
        assert(bufPos_ <= bufPosEnd_ && bufPosEnd_ <= memBuf_.size());

        auto       it    = static_cast<std::byte*>(buffer);
        const auto itEnd = it + bytesToRead;
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
        return it - static_cast<std::byte*>(buffer);
    }

    size_t getBlockSize() const { return 64 * 1024; }

private:
    size_t tryRead(void* buffer, size_t bytesToRead) //throw SysError; may return short, only 0 means EOF!
    {
        assert(bytesToRead <= getBlockSize()); //block size might be 1000 while reading HTTP header

        if (contentRemaining_ >= 0)
        {
            if (contentRemaining_ == 0)
                return 0;
            bytesToRead = static_cast<size_t>(std::min(static_cast<int64_t>(bytesToRead), contentRemaining_)); //[!] contentRemaining_ > 4 GB possible!
        }
        const size_t bytesReceived = tlsCtx_ ?
                                     tlsCtx_->tryRead(                buffer, bytesToRead) : //throw SysError; may return short, only 0 means EOF!
                                     tryReadSocket   (socket_->get(), buffer, bytesToRead);  //
        if (contentRemaining_ >= 0)
            contentRemaining_ -= bytesReceived;

        if (bytesReceived == 0 && contentRemaining_ > 0)
            throw SysError(replaceCpy<std::wstring>(L"HttpInputStream::tryRead: incomplete server response; %x more bytes expected.", L"%x", numberTo<std::wstring>(contentRemaining_)));

        return bytesReceived; //"zero indicates end of file"
    }

    void cleanup()
    {
    }

    Impl           (const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    std::unique_ptr<Socket> socket_;     //*bound* after constructor has run
    std::unique_ptr<TlsContext> tlsCtx_; //optional: support HTTPS
    int statusCode_ = 0;
    std::map<std::string, std::string, LessAsciiNoCase> responseHeaders_;

    int64_t contentRemaining_ = -1; //consider "Content-Length" if available

    const IOCallback notifyUnbufferedIO_; //throw X

    std::vector<std::byte> memBuf_ = std::vector<std::byte>(getBlockSize());
    size_t bufPos_    = 0; //buffered I/O; see file_io.cpp
    size_t bufPosEnd_ = 0; //
};


HttpInputStream::HttpInputStream(std::unique_ptr<Impl>&& pimpl) : pimpl_(std::move(pimpl)) {}

HttpInputStream::~HttpInputStream() {}

size_t HttpInputStream::read(void* buffer, size_t bytesToRead) { return pimpl_->read(buffer, bytesToRead); } //throw SysError, X; return "bytesToRead" bytes unless end of stream!

size_t HttpInputStream::getBlockSize() const { return pimpl_->getBlockSize(); }

std::string HttpInputStream::readAll() { return bufferedLoad<std::string>(*pimpl_); } //throw SysError, X


namespace
{
std::unique_ptr<HttpInputStream::Impl> sendHttpRequestImpl(const Zstring& url,
                                                           const std::vector<std::pair<std::string, std::string>>* postParams /*issue POST if bound, GET otherwise*/,
                                                           const Zstring& userAgent,
                                                           const Zstring* caCertFilePath /*optional: enable certificate validation*/,
                                                           const IOCallback& notifyUnbufferedIO) //throw SysError
{
    Zstring urlRed = url;
    //"A user agent should not automatically redirect a request more than five times, since such redirections usually indicate an infinite loop."
    for (int redirects = 0; redirects < 6; ++redirects)
    {
        auto response = std::make_unique<HttpInputStream::Impl>(urlRed, postParams, false /*disableGetCache*/, userAgent, caCertFilePath, notifyUnbufferedIO); //throw SysError

        //https://en.wikipedia.org/wiki/List_of_HTTP_status_codes#3xx_Redirection
        const int statusCode = response->getStatusCode();
        if (statusCode / 100 == 3) //e.g. 301, 302, 303, 307... we're not too greedy since we check location, too!
        {
            const std::string* value = response->getHeader("Location");
            if (!value || value->empty())
                throw SysError(L"Unresolvable redirect. No target Location.");

            urlRed = utfTo<Zstring>(*value);
        }
        else
        {
            if (statusCode != 200) //HTTP_STATUS_OK
                throw SysError(replaceCpy<std::wstring>(L"HTTP status code %x.", L"%x", numberTo<std::wstring>(statusCode)));
            //e.g. 404 - HTTP_STATUS_NOT_FOUND

            return response;
        }
    }
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
			const auto [high, low] = hexify(c);
            out += '%';
            out += high;
            out += low;
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
    for (const auto& [name, value] : paramPairs)
        output += urlencode(name) + '=' + urlencode(value) + '&';
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


HttpInputStream zen::sendHttpPost(const Zstring& url, const std::vector<std::pair<std::string, std::string>>& postParams,
	const Zstring& userAgent, const Zstring* caCertFilePath, const IOCallback& notifyUnbufferedIO) //throw SysError
{
    return sendHttpRequestImpl(url, &postParams, userAgent, caCertFilePath, notifyUnbufferedIO); //throw SysError
}


HttpInputStream zen::sendHttpGet(const Zstring& url, const Zstring& userAgent, const Zstring* caCertFilePath, const IOCallback& notifyUnbufferedIO) //throw SysError
{
    return sendHttpRequestImpl(url, nullptr /*postParams*/, userAgent, caCertFilePath, notifyUnbufferedIO); //throw SysError
}


bool zen::internetIsAlive() //noexcept
{
    try
    {
        auto response = std::make_unique<HttpInputStream::Impl>(Zstr("http://www.google.com/"),
                                                                nullptr /*postParams*/,
                                                                true /*disableGetCache*/,
                                                                Zstr("FreeFileSync"),
                                                                nullptr /*caCertFilePath*/,
                                                                nullptr /*notifyUnbufferedIO*/); //throw SysError
        const int statusCode = response->getStatusCode();

        //attention: http://www.google.com/ might redirect to "https" => don't follow, just return "true"!!!
        return statusCode / 100 == 2 || //e.g. 200
               statusCode / 100 == 3;   //e.g. 301, 302, 303, 307... when in doubt, consider internet alive!
    }
    catch (SysError&) { return false; }
}
