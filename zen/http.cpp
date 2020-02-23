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
         const std::string* postBuf /*issue POST if bound, GET otherwise*/,
         const Zstring& contentType, //required for POST
         bool disableGetCache /*not relevant for POST (= never cached)*/,
         const Zstring& userAgent,
         const Zstring* caCertFilePath /*optional: enable certificate validation*/,
         const IOCallback& notifyUnbufferedIO) : //throw SysError, X
        notifyUnbufferedIO_(notifyUnbufferedIO)
    {
        ZEN_ON_SCOPE_FAIL(cleanup(); /*destructor call would lead to member double clean-up!!!*/);

        //may be sending large POST: call back first
        if (notifyUnbufferedIO_) notifyUnbufferedIO_(0); //throw X

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

        assert(postBuf || contentType.empty());

        std::map<std::string, std::string, LessAsciiNoCase> headers;

        if (postBuf && !contentType.empty())
            headers["Content-Type"] = utfTo<std::string>(contentType);

        if (useTls) //HTTP default port: 443, see %WINDIR%\system32\drivers\etc\services
        {
            socket_ = std::make_unique<Socket>(server, Zstr("https")); //throw SysError
            tlsCtx_ = std::make_unique<TlsContext>(socket_->get(), server, caCertFilePath); //throw SysError
        }
        else //HTTP default port: 80, see %WINDIR%\system32\drivers\etc\services
            socket_ = std::make_unique<Socket>(server, Zstr("http")); //throw SysError

        //we don't support "chunked and gzip transfer encoding" => HTTP 1.0
        headers["Host"      ] = utfTo<std::string>(server); //only required for HTTP/1.1 but a few servers expect it even for HTTP/1.0
        headers["User-Agent"] = utfTo<std::string>(userAgent);
        headers["Accept"    ] = "*/*"; //won't hurt?

        if (!postBuf /*HTTP GET*/ && disableGetCache)
            headers["Pragma"] = "no-cache"; //HTTP 1.0 only! superseeded by "Cache-Control"

        if (postBuf)
            headers["Content-Length"] = numberTo<std::string>(postBuf->size());

        //https://www.w3.org/Protocols/HTTP/1.0/spec.html#Request-Line
        std::string msg = (postBuf ? "POST " : "GET ") + utfTo<std::string>(page) + " HTTP/1.0\r\n";
        for (const auto& [name, value] : headers)
            msg += name + ": " + value + "\r\n";
        msg += "\r\n";
        if (postBuf)
            msg += *postBuf;

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

        //let's not get too finicky: at least report the logical amount of bytes sent/received (excluding HTTP headers)
        if (notifyUnbufferedIO_) notifyUnbufferedIO_(postBuf ? postBuf->size() : 0); //throw X
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
                                                           const std::string* postBuf /*issue POST if bound, GET otherwise*/,
                                                           const Zstring& contentType, //required for POST
                                                           const Zstring& userAgent,
                                                           const Zstring* caCertFilePath /*optional: enable certificate validation*/,
                                                           const IOCallback& notifyUnbufferedIO) //throw SysError, X
{
    Zstring urlRed = url;
    //"A user agent should not automatically redirect a request more than five times, since such redirections usually indicate an infinite loop."
    for (int redirects = 0; redirects < 6; ++redirects)
    {
        auto response = std::make_unique<HttpInputStream::Impl>(urlRed, postBuf, contentType, false /*disableGetCache*/, userAgent, caCertFilePath, notifyUnbufferedIO); //throw SysError, X

        //https://en.wikipedia.org/wiki/List_of_HTTP_status_codes#3xx_Redirection
        const int httpStatusCode = response->getStatusCode();
        if (httpStatusCode / 100 == 3) //e.g. 301, 302, 303, 307... we're not too greedy since we check location, too!
        {
            const std::string* value = response->getHeader("Location");
            if (!value || value->empty())
                throw SysError(L"Unresolvable redirect. No target Location.");

            urlRed = utfTo<Zstring>(*value);
        }
        else
        {
            if (httpStatusCode != 200) //HTTP_STATUS_OK(200)
                throw SysError(formatHttpStatusCode(httpStatusCode)); //e.g. HTTP_STATUS_NOT_FOUND(404)

            return response;
        }
    }
    throw SysError(L"Too many redirects.");
}


//encode for "application/x-www-form-urlencoded"
std::string urlencode(const std::string& str)
{
    std::string output;
    for (const char c : str) //follow PHP spec: https://github.com/php/php-src/blob/e99d5d39239c611e1e7304e79e88545c4e71a073/ext/standard/url.c#L455
        if (c == ' ')
            output += '+';
        else if (('0' <= c && c <= '9') ||
                 ('A' <= c && c <= 'Z') ||
                 ('a' <= c && c <= 'z') ||
                 c == '-' || c == '.' || c == '_') //note: "~" is encoded by PHP!
            output += c;
        else
        {
            const auto [high, low] = hexify(c);
            output += '%';
            output += high;
            output += low;
        }
    return output;
}


std::string urldecode(const std::string& str)
{
    std::string output;
    for (size_t i = 0; i < str.size(); ++i)
    {
        const char c = str[i];
        if (c == '+')
            output += ' ';
        else if (c == '%' && str.size() - i >= 3 &&
                 isHexDigit(str[i + 1]) &&
                 isHexDigit(str[i + 2]))
        {
            output += unhexify(str[i + 1], str[i + 2]);
            i += 2;
        }
        else
            output += c;
    }
    return output;
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


HttpInputStream zen::sendHttpGet(const Zstring& url, const Zstring& userAgent, const Zstring* caCertFilePath, const IOCallback& notifyUnbufferedIO) //throw SysError, X
{
    return sendHttpRequestImpl(url, nullptr /*postBuf*/, Zstr("") /*contentType*/, userAgent, caCertFilePath, notifyUnbufferedIO); //throw SysError, X, X
}


HttpInputStream zen::sendHttpPost(const Zstring& url, const std::vector<std::pair<std::string, std::string>>& postParams,
                                  const Zstring& userAgent, const Zstring* caCertFilePath, const IOCallback& notifyUnbufferedIO) //throw SysError, X
{
    return sendHttpPost(url, xWwwFormUrlEncode(postParams), Zstr("application/x-www-form-urlencoded"), userAgent, caCertFilePath, notifyUnbufferedIO); //throw SysError, X
}



HttpInputStream zen::sendHttpPost(const Zstring& url, const std::string& postBuf, const Zstring& contentType,
                                  const Zstring& userAgent, const Zstring* caCertFilePath, const IOCallback& notifyUnbufferedIO) //throw SysError, X
{
    return sendHttpRequestImpl(url, &postBuf, contentType, userAgent, caCertFilePath, notifyUnbufferedIO); //throw SysError, X
}


bool zen::internetIsAlive() //noexcept
{
    try
    {
        auto response = std::make_unique<HttpInputStream::Impl>(Zstr("http://www.google.com/"),
                                                                nullptr /*postParams*/,
                                                                Zstr("") /*contentType*/,
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


std::wstring zen::formatHttpStatusCode(int sc)
{
    const wchar_t* statusText = [&] //https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
    {
        switch (sc)
        {
			//*INDENT-OFF*
			case 300: return L"Multiple choices.";
			case 301: return L"Moved permanently.";
			case 302: return L"Moved temporarily.";
			case 303: return L"See other";
			case 304: return L"Not modified.";
			case 305: return L"Use proxy.";
			case 306: return L"Switch proxy.";
			case 307: return L"Temporary redirect.";
			case 308: return L"Permanent redirect.";

			case 400: return L"Bad request.";
			case 401: return L"Unauthorized.";
			case 402: return L"Payment required.";
			case 403: return L"Forbidden.";
			case 404: return L"Not found.";
			case 405: return L"Method not allowed.";
			case 406: return L"Not acceptable.";
			case 407: return L"Proxy authentication required.";
			case 408: return L"Request timeout.";
			case 409: return L"Conflict.";
			case 410: return L"Gone.";
			case 411: return L"Length required.";
			case 412: return L"Precondition failed.";
			case 413: return L"Payload too large.";
			case 414: return L"URI too long.";
			case 415: return L"Unsupported media type.";
			case 416: return L"Range not satisfiable.";
			case 417: return L"Expectation failed.";
			case 418: return L"I'm a teapot.";
			case 421: return L"Misdirected request.";
			case 422: return L"Unprocessable entity.";
			case 423: return L"Locked.";
			case 424: return L"Failed dependency.";
			case 425: return L"Too early.";
			case 426: return L"Upgrade required.";
			case 428: return L"Precondition required.";
			case 429: return L"Too many requests.";
			case 431: return L"Request header fields too large.";
			case 451: return L"Unavailable for legal reasons.";

			case 500: return L"Internal server error.";
			case 501: return L"Not implemented.";
			case 502: return L"Bad gateway.";
			case 503: return L"Service unavailable.";
			case 504: return L"Gateway timeout.";
			case 505: return L"HTTP version not supported.";
			case 506: return L"Variant also negotiates.";
			case 507: return L"Insufficient storage.";
			case 508: return L"Loop detected.";
			case 510: return L"Not extended.";
			case 511: return L"Network authentication required.";

			//Cloudflare errors regarding origin server:
			case 520: return L"Unknown error (Cloudflare)";
			case 521: return L"Web server is down (Cloudflare)";
			case 522: return L"Connection timed out (Cloudflare)";
			case 523: return L"Origin is unreachable (Cloudflare)";
			case 524: return L"A timeout occurred (Cloudflare)";
			case 525: return L"SSL handshake failed (Cloudflare)";
			case 526: return L"Invalid SSL certificate (Cloudflare)";
			case 527: return L"Railgun error (Cloudflare)";
			case 530: return L"Origin DNS error (Cloudflare)";

			default:  return L"";
			//*INDENT-ON*
        }
    }();

    if (strLength(statusText) == 0)
        return trimCpy(replaceCpy<std::wstring>(L"HTTP status %x.", L"%x", numberTo<std::wstring>(sc)));
    else
        return trimCpy(replaceCpy<std::wstring>(L"HTTP status %x: ", L"%x", numberTo<std::wstring>(sc)) + statusText);
}


bool zen::isValidEmail(const Zstring& email)
{
    //https://en.wikipedia.org/wiki/Email_address#Syntax
    //https://tools.ietf.org/html/rfc3696 => note errata! https://www.rfc-editor.org/errata_search.php?rfc=3696
    //https://tools.ietf.org/html/rfc5321
    std::string local  = utfTo<std::string>(beforeLast(email, Zstr('@'), IF_MISSING_RETURN_NONE));
    std::string domain = utfTo<std::string>( afterLast(email, Zstr('@'), IF_MISSING_RETURN_NONE));
    //consider: "t@st"@email.com t\@st@email.com"

    auto stripComments = [](std::string& part)
    {
        if (startsWith(part, '('))
            part = afterFirst(part, ')', IF_MISSING_RETURN_NONE);

        if (endsWith(part, ')'))
            part = beforeLast(part, '(', IF_MISSING_RETURN_NONE);
    };
    stripComments(local);
    stripComments(domain);

    if (local .empty() || local .size() > 63 || // 64 octets ->  63 ASCII chars: https://devblogs.microsoft.com/oldnewthing/20120412-00/?p=7873
        domain.empty() || domain.size() > 253)  //255 octets -> 253 ASCII chars
        return false;
    //---------------------------------------------------------------------

    const bool quoted = (startsWith(local, '"') && endsWith(local, '"')) ||
                        contains(local, '\\'); //e.g. "t\@st@email.com"
    if (!quoted) //I'm not going to parse and validate this!
        for (const std::string& comp : split(local, '.', SplitType::ALLOW_EMPTY))
            if (comp.empty() || !std::all_of(comp.begin(), comp.end(), [](char c)
        {
            const char printable[] = "!#$%&'*+-/=?^_`{|}~";
                return isAsciiAlpha(c) || isDigit(c) || makeUnsigned(c) >= 128 ||
                       std::find(std::begin(printable), std::end(printable), c) != std::end(printable);
            }))
    return false;
    //---------------------------------------------------------------------

    //e.g. jsmith@[192.168.2.1]  jsmith@[IPv6:2001:db8::1]
    const bool likelyIp = startsWith(domain, '[') && endsWith(domain, ']');
    if (!likelyIp) //not interested in parsing IPs!
    {
        if (!contains(domain, '.'))
            return false;

        for (const std::string& comp : split(domain, '.', SplitType::ALLOW_EMPTY))
            if (comp.empty() || comp.size() > 63 ||
            !std::all_of(comp.begin(), comp.end(), [](char c) { return isAsciiAlpha(c) ||isDigit(c) || makeUnsigned(c) >= 128 || c ==  '-'; }))
        return false;
    }

    return true;
}


std::string zen::htmlSpecialChars(const std::string& str)
{
    //mirror PHP: https://github.com/php/php-src/blob/e99d5d39239c611e1e7304e79e88545c4e71a073/ext/standard/html_tables.h#L6189
    std::string output;
    for (const char c : str)
        switch (c)
        {
            //*INDENT-OFF*
            case '&': output += "&amp;" ; break;
            case '"': output += "&quot;"; break;
            case '<': output += "&lt;"  ; break;
            case '>': output += "&gt;"  ; break;
            //case '\'': output += "&apos;"; break; -> not encoded by default (needs ENT_QUOTES)
            default: output += c; break;
            //*INDENT-ON*
        }
    return output;
}
