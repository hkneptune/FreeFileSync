// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "http.h"

    #include <libcurl/curl_wrap.h> //DON'T include <curl/curl.h> directly!
    #include "stream_buffer.h"

using namespace zen;


const int HTTP_ACCESS_TIMEOUT_SEC = 20;

const size_t HTTP_BLOCK_SIZE_DOWNLOAD = 64 * 1024; //libcurl returns blocks of only 16 kB as returned by recv() even if we request larger blocks via CURLOPT_BUFFERSIZE
//- InternetReadFile() is buffered + prefetching
//- libcurl returns blocks of only 16 kB as returned by recv() even if we request larger blocks via CURLOPT_BUFFERSIZE
    const size_t HTTP_STREAM_BUFFER_SIZE = 1024 * 1024; //unit: [byte]
    //stream buffer should be big enough to facilitate prefetching during alternating read/write operations => e.g. see serialize.h::unbufferedStreamCopy()




class HttpInputStream::Impl
{
public:
    Impl(const Zstring& url,
         const std::string* postBuf, //issue POST if bound, GET otherwise
         const std::string& contentType, //required for POST
         const IoCallback& onPostBytesSent /*throw X*/,
         bool disableGetCache, //not relevant for POST (= never cached)
         const Zstring& userAgent,
         const Zstring& caCertFilePath /*optional: enable certificate validation*/) //throw SysError, X
    {
        ZEN_ON_SCOPE_FAIL(cleanup()); //destructor call would lead to member double clean-up!!!

        assert(postBuf || !onPostBytesSent);

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

        std::unordered_map<std::string, std::string, StringHashAsciiNoCase, StringEqualAsciiNoCase> headers;

        assert(postBuf || contentType.empty());
        if (postBuf && !contentType.empty())
            headers["Content-Type"] = contentType;

        if (!postBuf /*=> HTTP GET*/ && disableGetCache) //libcurl doesn't cache internally, so it should be enough to set this header
            headers["Cache-Control"] = "no-cache"; //= similar to WinInet's INTERNET_FLAG_RELOAD
        //caveat: INTERNET_FLAG_RELOAD issues "Pragma: no-cache" instead if "request is going through a proxy"


        auto promHeader = std::make_shared<std::promise<std::string>>();
        std::future<std::string> futHeader = promHeader->get_future();

        auto postBytesSent = std::make_shared<std::atomic<int64_t>>(0);

        worker_ = InterruptibleThread([asyncStreamOut = this->asyncStreamIn_, promHeader, headers = std::move(headers), postBytesSent,
                                                      server, useTls, caCertFilePath, userAgent = utfTo<std::string>(userAgent),
                                                      postBuf = postBuf ? std::optional<std::string>(*postBuf) : std::nullopt, //[!] life-time!
                                                      serverRelPath = utfTo<std::string>(page)]
        {
            setCurrentThreadName(Zstr("Istream ") + server);

            bool headerReceived = false;
            try
            {
                std::vector<std::string> curlHeaders;
                for (const auto& [name, value] : headers)
                    curlHeaders.push_back(name + ": " + value);

                std::vector<CurlOption> extraOptions {{CURLOPT_USERAGENT, userAgent.c_str()}};
                //CURLOPT_FOLLOWLOCATION already off by default :)

                std::function<size_t(std::span<char> buf)> readRequest;
                if (postBuf)
                {
                    readRequest = [&, postBufStream{MemoryStreamIn(*postBuf)}](std::span<char> buf) mutable
                    {
                        const size_t bytesRead = postBufStream.read(buf.data(), buf.size());
                        * postBytesSent += bytesRead;
                        return bytesRead;
                    };
                    extraOptions.emplace_back(CURLOPT_POST, 1);
                    extraOptions.emplace_back(CURLOPT_POSTFIELDSIZE_LARGE, postBuf->size()); //avoid HTTP chunked transfer encoding?
                }

                //careful with these callbacks! First receive HTTP header without blocking,
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
                        promHeader->set_value(std::move(headerBuf));
                    }
                };

                HttpSession httpSession(server, useTls, caCertFilePath); //throw SysError

                auto writeResponse = [&](std::span<const char> buf)
                {
                    if (!headerReceived)
                        throw SysError(L"Received HTTP body without header.");

                    asyncStreamOut->write(buf.data(), buf.size()); //throw ThreadStopRequest
                };

                httpSession.perform(serverRelPath,
                                    curlHeaders, extraOptions,
                                    writeResponse /*throw ThreadStopRequest*/,
                                    readRequest,
                                    onHeaderData /*throw SysError*/,
                                    HTTP_ACCESS_TIMEOUT_SEC); //throw SysError, ThreadStopRequest

                if (!headerReceived)
                    throw SysError(L"HTTP response is missing header.");

                asyncStreamOut->closeStream();
            }
            catch (SysError&) //let ThreadStopRequest pass through!
            {
                if (!headerReceived)
                    promHeader->set_exception(std::current_exception());

                asyncStreamOut->setWriteError(std::current_exception());
            }
        });

        //------------------------------------------------------------------------------------
        if (postBuf && onPostBytesSent)
        {
            int64_t bytesReported = 0;
            while (futHeader.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout)
            {
                const int64_t bytesDelta = *postBytesSent /*atomic shared access!*/- bytesReported;
                bytesReported += bytesDelta;
                onPostBytesSent(bytesDelta); //throw X
            }
        }
        //------------------------------------------------------------------------------------

        const std::string headBuf = futHeader.get(); //throw SysError
        //parse header: https://www.w3.org/Protocols/HTTP/1.0/spec.html#Request-Line
        const std::string_view& statusBuf  = beforeFirst<std::string_view>(headBuf, "\r\n", IfNotFoundReturn::all);
        const std::string_view& headersBuf = afterFirst <std::string_view>(headBuf, "\r\n", IfNotFoundReturn::none);

        const std::vector<std::string_view> statusItems = splitCpy(statusBuf, ' ', SplitOnEmpty::allow); //HTTP-Version SP Status-Code SP Reason-Phrase CRLF
        if (statusItems.size() < 2 || !startsWith(statusItems[0], "HTTP/"))
            throw SysError(L"Invalid HTTP response: \"" + utfTo<std::wstring>(statusBuf) + L'"');

        statusCode_ = stringTo<int>(statusItems[1]);

        split(headersBuf, '\n', [&](const std::string_view line)
        {
            if (!line.empty()) //careful: actual line separator is "\r\n"!
                responseHeaders_.emplace(trimCpy(beforeFirst(line, ':', IfNotFoundReturn::all)),
                                         trimCpy(afterFirst (line, ':', IfNotFoundReturn::none)));
        });
        /* let's NOT consider "Content-Length" header:
            - may be unavailable ("Transfer-Encoding: chunked")
            - may refer to compressed data size ("Content-Encoding: gzip") */
    }

    ~Impl() { cleanup(); }

    const int getStatusCode() const { return statusCode_; }

    const std::string* getHeader(const std::string& name) const
    {
        auto it = responseHeaders_.find(name);
        return it != responseHeaders_.end() ? &it->second : nullptr;
    }

    size_t getBlockSize() const { return HTTP_BLOCK_SIZE_DOWNLOAD; }

    size_t tryRead(void* buffer, size_t bytesToRead) //throw SysError; may return short; only 0 means EOF! CONTRACT: bytesToRead > 0!
    {
        return asyncStreamIn_->tryRead(buffer, bytesToRead); //throw SysError
        //no need for asyncStreamIn_->checkWriteErrors(): once end of stream is reached, asyncStreamOut->closeStream() was called => no errors occured
    }

private:
    Impl           (const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    void cleanup()
    {
        asyncStreamIn_->setReadError(std::make_exception_ptr(ThreadStopRequest()));
    }

    std::shared_ptr<AsyncStreamBuffer> asyncStreamIn_ = std::make_shared<AsyncStreamBuffer>(HTTP_STREAM_BUFFER_SIZE);
    InterruptibleThread worker_;
    int statusCode_ = 0;
    std::unordered_map<std::string, std::string, StringHashAsciiNoCase, StringEqualAsciiNoCase> responseHeaders_;
};


HttpInputStream::HttpInputStream(std::unique_ptr<Impl>&& pimpl) : pimpl_(std::move(pimpl)) {}

HttpInputStream::~HttpInputStream() {}

size_t HttpInputStream::tryRead(void* buffer, size_t bytesToRead) { return pimpl_->tryRead(buffer, bytesToRead); }

size_t HttpInputStream::getBlockSize() const { return pimpl_->getBlockSize(); }

std::string HttpInputStream::readAll(const IoCallback& notifyUnbufferedIO /*throw X*/) //throw SysError, X
{
    return unbufferedLoad<std::string>([&](void* buffer, size_t bytesToRead)
    {
        const size_t bytesRead = pimpl_->tryRead(buffer, bytesToRead); //throw SysError; may return short, only 0 means EOF! =>  CONTRACT: bytesToRead > 0!
        if (notifyUnbufferedIO) notifyUnbufferedIO(bytesRead); //throw X!
        return bytesRead;
    },
    pimpl_->getBlockSize()); //throw SysError, X
}


namespace
{
std::unique_ptr<HttpInputStream::Impl> sendHttpRequestImpl(const Zstring& url,
                                                           const std::string* postBuf /*issue POST if bound, GET otherwise*/,
                                                           const std::string& contentType, //required for POST
                                                           const IoCallback& onPostBytesSent /*throw X*/,
                                                           const Zstring& userAgent,
                                                           const Zstring& caCertFilePath /*optional: enable certificate validation*/) //throw SysError, X
{
    Zstring urlRed = url;
    //"A user agent should not automatically redirect a request more than five times, since such redirections usually indicate an infinite loop."
    for (int redirects = 0; redirects < 6; ++redirects)
    {
        auto response = std::make_unique<HttpInputStream::Impl>(urlRed, postBuf, contentType, onPostBytesSent, false /*disableGetCache*/,
                                                                userAgent, caCertFilePath); //throw SysError, X

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
            {
#if 0 //beneficial to add error details?
                std::wstring errorDetails;
                try
                {
                    errorDetails = utfTo<std::wstring>(HttpInputStream(std::move(response)).readAll(nullptr /*notifyUnbufferedIO*/)); //throw SysError
                }
                catch (const SysError& e) { errorDetails = e.toString(); }
#endif
                throw SysError(formatHttpError(httpStatus) /*+ L' ' + errorDetails*/); //e.g. "HTTP status 404: Not found."
            }

            return response;
        }
    }
    throw SysError(L"Too many redirects.");
}


//encode for "application/x-www-form-urlencoded"
std::string urlencode(const std::string_view& str)
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


std::string urldecode(const std::string_view& str)
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


std::vector<std::pair<std::string, std::string>> zen::xWwwFormUrlDecode(const std::string_view str)
{
    std::vector<std::pair<std::string, std::string>> output;

    split(str, '&', [&](const std::string_view nvPair)
    {
        if (!nvPair.empty())
            output.emplace_back(urldecode(beforeFirst(nvPair, '=', IfNotFoundReturn::all)),
                                urldecode(afterFirst (nvPair, '=', IfNotFoundReturn::none)));
    });
    return output;
}


HttpInputStream zen::sendHttpGet(const Zstring& url, const Zstring& userAgent, const Zstring& caCertFilePath) //throw SysError
{
    return sendHttpRequestImpl(url, nullptr /*postBuf*/, "" /*contentType*/, nullptr /*onPostBytesSent*/, userAgent,  caCertFilePath); //throw SysError
}


HttpInputStream zen::sendHttpPost(const Zstring& url, const std::vector<std::pair<std::string, std::string>>& postParams,
                                  const IoCallback& notifyUnbufferedIO /*throw X*/,
                                  const Zstring& userAgent,
                                  const Zstring& caCertFilePath) //throw SysError, X
{
    return sendHttpPost(url, xWwwFormUrlEncode(postParams), "application/x-www-form-urlencoded", notifyUnbufferedIO, userAgent, caCertFilePath); //throw SysError, X
}



HttpInputStream zen::sendHttpPost(const Zstring& url,
                                  const std::string& postBuf,
                                  const std::string& contentType,
                                  const IoCallback& notifyUnbufferedIO /*throw X*/,
                                  const Zstring& userAgent,
                                  const Zstring& caCertFilePath) //throw SysError, X
{
    return sendHttpRequestImpl(url, &postBuf, contentType, notifyUnbufferedIO, userAgent, caCertFilePath); //throw SysError, X
}


bool zen::internetIsAlive() //noexcept
{
    try
    {
        auto response = std::make_unique<HttpInputStream::Impl>(Zstr("https://www.google.com/"), //https more appropriate than http for testing? (different ports!)
                                                                nullptr /*postParams*/,
                                                                "" /*contentType*/,
                                                                nullptr /*onPostBytesSent*/,
                                                                true /*disableGetCache*/,
                                                                Zstr("FreeFileSync"),
                                                                Zstring() /*caCertFilePath*/); //throw SysError
        const int statusCode = response->getStatusCode();

        //attention: google.com might redirect to https://consent.google.com => don't follow, just return "true"!!!
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


bool zen::isValidEmail(const std::string_view& email)
{
    //https://en.wikipedia.org/wiki/Email_address#Syntax
    //https://tools.ietf.org/html/rfc3696 => note errata! https://www.rfc-editor.org/errata_search.php?rfc=3696
    //https://tools.ietf.org/html/rfc5321
    std::string_view local  = beforeLast(email, '@', IfNotFoundReturn::none);
    std::string_view domain =  afterLast(email, '@', IfNotFoundReturn::none);
    //consider: "t@st"@email.com t\@st@email.com"

    auto stripComments = [](std::string_view& part)
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

    //we're not going to parse and validate this!
    const bool quoted = (startsWith(local, '"') && endsWith(local, '"')) ||
                        contains(local, '\\'); //e.g. "t\@st@email.com"
    if (!quoted)
        for (const std::string_view& comp : splitCpy(local, '.', SplitOnEmpty::allow))
            if (comp.empty() || !std::all_of(comp.begin(), comp.end(), [](char c)
        {
            constexpr std::string_view printable("!#$%&'*+-/=?^_`{|}~");
                return isAsciiAlpha(c) || isDigit(c) || !isAsciiChar(c) ||
                       contains(printable, c);
            }))
    return false;
    //---------------------------------------------------------------------

    //e.g. jsmith@[192.168.2.1]  jsmith@[IPv6:2001:db8::1]
    const bool likelyIp = startsWith(domain, '[') && endsWith(domain, ']');
    if (!likelyIp) //not interested in parsing IPs!
    {
        if (!contains(domain, '.'))
            return false;

        for (const std::string_view& comp : splitCpy(domain, '.', SplitOnEmpty::allow))
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
