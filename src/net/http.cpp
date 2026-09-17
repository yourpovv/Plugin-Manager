#include "http.h"
#include "text.h"
#include "win32.h"
#include <winhttp.h>

static std::wstring defaultUserAgent() {
    return L"EquicordPluginManager/1.0";
}

bool http::isRemoteUrl(const std::string& url) {
    std::string lower = toLowerAscii(url);
    return startsWith(lower, "https://") || startsWith(lower, "http://");
}

struct InternetHandle {
    HINTERNET handle = nullptr;
    InternetHandle() = default;
    explicit InternetHandle(HINTERNET value) : handle(value) {}
    ~InternetHandle() {
        if (handle)
            WinHttpCloseHandle(handle);
    }
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    bool valid() const { return handle != nullptr; }
};

static http::Response failResponse(const std::string& userMessage, const std::string& detail, int status = 0) {
    http::Response response;
    response.succeeded = false;
    response.status = status;
    response.userMessage = userMessage;
    response.detail = detail;
    return response;
}

static bool crackUrl(const std::string& url, URL_COMPONENTS& parts, std::wstring& host, std::wstring& path, std::wstring& extra) {
    std::wstring wide = wideFromUtf8(url);
    memset(&parts, 0, sizeof(parts));
    parts.dwStructSize = sizeof(parts);
    host.assign(256, L'\0');
    path.assign(2048, L'\0');
    extra.assign(1024, L'\0');
    parts.lpszHostName = host.data();
    parts.dwHostNameLength = (DWORD)host.size();
    parts.lpszUrlPath = path.data();
    parts.dwUrlPathLength = (DWORD)path.size();
    parts.lpszExtraInfo = extra.data();
    parts.dwExtraInfoLength = (DWORD)extra.size();
    return WinHttpCrackUrl(wide.c_str(), (DWORD)wide.size(), 0, &parts) == TRUE;
}

http::Response http::get(const std::string& url, size_t maxBytes) {
    if (!isRemoteUrl(url))
        return failResponse("That download address is not an http or https URL.", url);

    URL_COMPONENTS parts;
    std::wstring host, path, extra;
    if (!crackUrl(url, parts, host, path, extra)) {
        return failResponse(
            "The download URL couldn't be read. Check plugins.json, then click Refresh.",
            url
        );
    }

    InternetHandle session(WinHttpOpen(
        defaultUserAgent().c_str(),
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    ));
    if (!session.valid())
        return failResponse("Couldn't start a network request. Check your internet connection, then try again.", "WinHttpOpen");

    WinHttpSetTimeouts(session.handle, 15000, 15000, 30000, 120000);

    InternetHandle connect(WinHttpConnect(session.handle, host.c_str(), parts.nPort, 0));
    if (!connect.valid())
        return failResponse("Couldn't reach the plugin host. Check your internet connection, then try again.", url);

    std::wstring object = path.c_str();
    object += extra.c_str();
    DWORD flags = (parts.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    InternetHandle request(WinHttpOpenRequest(
        connect.handle,
        L"GET",
        object.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    ));
    if (!request.valid())
        return failResponse("Couldn't open the download request. Try again in a moment.", url);

    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(request.handle, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));

    if (!WinHttpSendRequest(request.handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        || !WinHttpReceiveResponse(request.handle, nullptr)) {
        return failResponse(
            "The download couldn't finish because the connection dropped. Reconnect, then try again.",
            url + " err=" + std::to_string(GetLastError())
        );
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(
        request.handle,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX
    );

    std::string body;
    body.reserve(4096);
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request.handle, &available) && available > 0) {
        if (body.size() + available > maxBytes) {
            return failResponse(
                "The download was larger than this app will accept, so it was cancelled.",
                std::to_string(maxBytes)
            );
        }
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request.handle, chunk.data(), available, &read))
            break;
        chunk.resize(read);
        body += chunk;
    }

    Response response;
    response.status = (int)status;
    response.body = body;
    response.detail = url;
    if (status < 200 || status >= 300) {
        response.succeeded = false;
        response.userMessage =
            "The download failed because the server returned " + std::to_string(status)
            + ". Check the URL in plugins.json, then click Refresh.";
        return response;
    }
    response.succeeded = true;
    return response;
}
