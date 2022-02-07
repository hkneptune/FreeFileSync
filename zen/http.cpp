// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "http.h"

    #include <libcurl/curl_wrap.h> //DON'T include <curl/curl.h> directly!
    #include "stream_buffer.h"
    #include "thread.h"

using namespace zen;

constexpr std::chrono::seconds HTTP_ACCESS_TIME_OUT(20);




class HttpInputStream::Impl
{
public:
    Impl(const Zstring& url,
         const std::string* postBuf, //issue POST if bound, GET otherwise
         const std::string& contentType, //required for POST
         bool disableGetCache, //not relevant for POST (= never cached)
         const Zstring& userAgent,
         const Zstring& caCertFilePath, //optional: enable certificate validation
         const IoCallback& notifyUnbufferedIO /*throw X*/) : //throw SysError, X
        notifyUnbufferedIO_(notifyUnbufferedIO)
    {
        ZEN_ON_SCOPE_FAIL(cleanup()); //destructor call would lead to member double clean-up!!!

        //may be sending large POST: call back first
        if (notifyUnbufferedIO_) notifyUnbufferedIO_(0); //throw X

        const Zstring urlFmt =              afterFirst(url, Zstr("://"), IfNotFoundReturn::none);
        const Zstring server =            beforeFirst(urlFmt, Zstr('/'), IfNotFoundReturn::all);
        const Zstring page   = Zstr('/') + afterFirst(urlFmt, Zstr('/'), IfNotFoundReturn::none);

        const bool useTls = [&]
        {
            if (startsWithAsciiNoCase(url, "http://"))
                return false;
            if (startsWithAsciiNoCase(url, "https://"))
                return true;
            throw SysError(L"URL uses unexpected protocol.");
        }();

        std::map<std::string, std::string, LessAsciiNoCase> headers;

        assert(postBuf || contentType.empty());
        if (postBuf && !contentType.empty())
            headers["Content-Type"] = contentType;

        if (!postBuf /*=> HTTP GET*/ && disableGetCache) //libcurl doesn't cache internally, so it should be enough to set this header
            headers["Cache-Control"] = "no-cache"; //= similar to WinInet's INTERNET_FLAG_RELOAD
        //caveat: INTERNET_FLAG_RELOAD issues "Pragma: no-cache" instead if "request is going through a proxy"


        auto promiseHeader = std::make_shared<std::promise<std::string>>();
        std::future<std::string> futHeader = promiseHeader->get_future();

        worker_ = InterruptibleThread([asyncStreamOut = this->asyncStreamIn_, promiseHeader, headers = std::move(headers),
                                                      server, useTls, caCertFilePath, userAgent = utfTo<std::string>(userAgent),
                                                      postBuf = postBuf ? std::optional<std::string>(*postBuf) : std::nullopt, //[!] life-time!
                                                      serverRelPath = utfTo<std::string>(page)]
        {
            setCurrentThreadName(Zstr("HttpInputStream ") + server);

            bool headerReceived = false;
            try
            {
                std::vector<std::string> curlHeaders;
                for (const auto& [name, value] : headers)
                    curlHeaders.push_back(name + ": " + value);

                std::vector<CurlOption> extraOptions {{CURLOPT_USERAGENT, userAgent.c_str()}};
                //CURLOPT_FOLLOWLOCATION already off by default :)
                if (postBuf)
                {
                    extraOptions.emplace_back(CURLOPT_POSTFIELDS,          postBuf->c_str());
                    extraOptions.emplace_back(CURLOPT_POSTFIELDSIZE_LARGE, postBuf->size()); //postBuf not necessarily null-terminated!
                }

                //carefully with these callbacks! First receive HTTP header without blocking,
                //and only then allow AsyncStreamBuffer::write() which can block!

                std::string headerBuf;
                auto onHeaderData = [&](const std::string_view& headerLine)
                {
                    if (headerReceived)
                        throw SysError(L"Unexpected header data after end of HTTP header.");

                    //"The callback will be called once for each header and only complete header lines are passed on to the callback" (including \r\n at the end)
                    headerBuf += headerLine;

                    if (headerLine == "\r\n")
                    {
                        headerReceived = true;
                        promiseHeader->set_value(std::move(headerBuf));
                    }
                };

                HttpSession httpSession(server, useTls, caCertFilePath, HTTP_ACCESS_TIME_OUT); //throw SysError

                auto writeResponse = [&](std::span<const char> buf)
                {
                    if (!headerReceived)
                        throw SysError(L"Received HTTP body without header.");

                    return asyncStreamOut->write(buf.data(), buf.size()); //throw ThreadStopRequest
                };

                httpSession.perform(serverRelPath, //throw SysError, ThreadStopRequest
                                    curlHeaders, extraOptions,
                                    writeResponse /*throw ThreadStopRequest*/,
                                    nullptr /*readRequest*/,
                                    onHeaderData /*throw SysError*/);

                if (!headerReceived)
                    throw SysError(L"HTTP response is missing header.");

                asyncStreamOut->closeStream();
            }
            catch (SysError&) //let ThreadStopRequest pass through!
            {
                if (!headerReceived)
                    promiseHeader->set_exception(std::current_exception());

                asyncStreamOut->setWriteError(std::current_exception());
            }
        });

        const std::string headBuf = futHeader.get(); //throw SysError
        //parse header: https://www.w3.org/Protocols/HTTP/1.0/spec.html#Request-Line
        const std::string& statusBuf  = beforeFirst(headBuf, "\r\n", IfNotFoundReturn::all);
        const std::string& headersBuf = afterFirst (headBuf, "\r\n", IfNotFoundReturn::none);

        const std::vector<std::string> statusItems = split(statusBuf, ' ', SplitOnEmpty::allow); //HTTP-Version SP Status-Code SP Reason-Phrase CRLF
        if (statusItems.size() < 2 || !startsWith(statusItems[0], "HTTP/"))
            throw SysError(L"Invalid HTTP response: \"" + utfTo<std::wstring>(statusBuf) + L'"');

        statusCode_ = stringTo<int>(statusItems[1]);

        for (const std::string& line : split(headersBuf, "\r\n", SplitOnEmpty::skip))
            responseHeaders_[trimCpy(beforeFirst(line, ':', IfNotFoundReturn::all))] =
                /**/         trimCpy(afterFirst (line, ':', IfNotFoundReturn::none));

        /* let's NOT consider "Content-Length" header:
            - may be unavailable ("Transfer-Encoding: chunked")
            - may refer to compressed data size ("Content-Encoding: gzip") */

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
        const size_t bytesRead = asyncStreamIn_->read(buffer, bytesToRead); //throw SysError
        reportBytesProcessed(); //throw X
        return bytesRead;
        //no need for asyncStreamIn_->checkWriteErrors(): once end of stream is reached, asyncStreamOut->closeStream() was called => no errors occured
    }

    size_t getBlockSize() const { return 64 * 1024; }

private:
    Impl           (const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    void reportBytesProcessed() //throw X
    {
        const int64_t totalBytesDownloaded = asyncStreamIn_->getTotalBytesWritten();
        if (notifyUnbufferedIO_) notifyUnbufferedIO_(totalBytesDownloaded - totalBytesReported_); //throw X
        totalBytesReported_ = totalBytesDownloaded;
    }

    void cleanup()
    {
        asyncStreamIn_->setReadError(std::make_exception_ptr(ThreadStopRequest()));
    }

    std::shared_ptr<AsyncStreamBuffer> asyncStreamIn_ = std::make_shared<AsyncStreamBuffer>(512 * 1024);
    InterruptibleThread worker_;
    int64_t totalBytesReported_ = 0;
    int statusCode_ = 0;
    std::map<std::string, std::string, LessAsciiNoCase> responseHeaders_;

    const IoCallback notifyUnbufferedIO_; //throw X
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
                                                           const std::string& contentType, //required for POST
                                                           const Zstring& userAgent,
                                                           const Zstring& caCertFilePath /*optional: enable certificate validation*/,
                                                           const IoCallback& notifyUnbufferedIO) //throw SysError, X
{
    Zstring urlRed = url;
    //"A user agent should not automatically redirect a request more than five times, since such redirections usually indicate an infinite loop."
    for (int redirects = 0; redirects < 6; ++redirects)
    {
        auto response = std::make_unique<HttpInputStream::Impl>(urlRed, postBuf, contentType, false /*disableGetCache*/, userAgent, caCertFilePath, notifyUnbufferedIO); //throw SysError, X

        //https://en.wikipedia.org/wiki/List_of_HTTP_status_codes#3xx_Redirection
        const int httpStatus = response->getStatusCode();
        if (httpStatus / 100 == 3) //e.g. 301, 302, 303, 307... we're not too greedy since we check location, too!
        {
            const std::string* value = response->getHeader("Location");
            if (!value || value->empty())
                throw SysError(L"Unresolvable redirect. No target Location.");

            urlRed = utfTo<Zstring>(*value);
        }
        else
        {
            if (httpStatus != 200) //HTTP_STATUS_OK
                throw SysError(formatHttpError(httpStatus)); //e.g. "HTTP status 404: Not found."

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

    for (const std::string& nvPair : split(str, '&', SplitOnEmpty::skip))
        output.emplace_back(urldecode(beforeFirst(nvPair, '=', IfNotFoundReturn::all)),
                            urldecode(afterFirst (nvPair, '=', IfNotFoundReturn::none)));
    return output;
}


HttpInputStream zen::sendHttpGet(const Zstring& url, const Zstring& userAgent, const Zstring& caCertFilePath, const IoCallback& notifyUnbufferedIO) //throw SysError, X
{
    return sendHttpRequestImpl(url, nullptr /*postBuf*/, "" /*contentType*/, userAgent, caCertFilePath, notifyUnbufferedIO); //throw SysError, X, X
}


HttpInputStream zen::sendHttpPost(const Zstring& url, const std::vector<std::pair<std::string, std::string>>& postParams,
                                  const Zstring& userAgent, const Zstring& caCertFilePath, const IoCallback& notifyUnbufferedIO) //throw SysError, X
{
    return sendHttpPost(url, xWwwFormUrlEncode(postParams), "application/x-www-form-urlencoded", userAgent, caCertFilePath, notifyUnbufferedIO); //throw SysError, X
}



HttpInputStream zen::sendHttpPost(const Zstring& url, const std::string& postBuf, const std::string& contentType,
                                  const Zstring& userAgent, const Zstring& caCertFilePath, const IoCallback& notifyUnbufferedIO) //throw SysError, X
{
    return sendHttpRequestImpl(url, &postBuf, contentType, userAgent, caCertFilePath, notifyUnbufferedIO); //throw SysError, X
}


bool zen::internetIsAlive() //noexcept
{
    try
    {
        auto response = std::make_unique<HttpInputStream::Impl>(Zstr("http://www.google.com/"),
                                                                nullptr /*postParams*/,
                                                                "" /*contentType*/,
                                                                true /*disableGetCache*/,
                                                                Zstr("FreeFileSync"),
                                                                Zstring() /*caCertFilePath*/,
                                                                nullptr /*notifyUnbufferedIO*/); //throw SysError
        const int statusCode = response->getStatusCode();

        //attention: http://www.google.com/ might redirect to "https" => don't follow, just return "true"!!!
        return statusCode / 100 == 2 || //e.g. 200
               statusCode / 100 == 3;   //e.g. 301, 302, 303, 307... when in doubt, consider internet alive!
    }
    catch (SysError&) { return false; }
}


std::wstring zen::formatHttpError(int sc)
{
    const wchar_t* statusDescr = [&] //https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
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

    return formatSystemError("", L"HTTP status " + numberTo<std::wstring>(sc), statusDescr);
}


bool zen::isValidEmail(const std::string& email)
{
    //https://en.wikipedia.org/wiki/Email_address#Syntax
    //https://tools.ietf.org/html/rfc3696 => note errata! https://www.rfc-editor.org/errata_search.php?rfc=3696
    //https://tools.ietf.org/html/rfc5321
    std::string local  = beforeLast(email, '@', IfNotFoundReturn::none);
    std::string domain =  afterLast(email, '@', IfNotFoundReturn::none);
    //consider: "t@st"@email.com t\@st@email.com"

    auto stripComments = [](std::string& part)
    {
        if (startsWith(part, '('))
            part = afterFirst(part, ')', IfNotFoundReturn::none);

        if (endsWith(part, ')'))
            part = beforeLast(part, '(', IfNotFoundReturn::none);
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
        for (const std::string& comp : split(local, '.', SplitOnEmpty::allow))
            if (comp.empty() || !std::all_of(comp.begin(), comp.end(), [](char c)
        {
            const char printable[] = "!#$%&'*+-/=?^_`{|}~";
                return isAsciiAlpha(c) || isDigit(c) || !isAsciiChar(c) ||
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

        for (const std::string& comp : split(domain, '.', SplitOnEmpty::allow))
            if (comp.empty() || comp.size() > 63 ||
            !std::all_of(comp.begin(), comp.end(), [](char c) { return isAsciiAlpha(c) ||isDigit(c) || !isAsciiChar(c) || c ==  '-'; }))
        return false;
    }

    return true;
}


std::string zen::htmlSpecialChars(const std::string_view& str)
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
