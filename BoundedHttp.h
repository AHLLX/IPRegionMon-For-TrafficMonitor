#pragma once
#include <Windows.h>
#include <winhttp.h>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <string>
#pragma comment(lib, "winhttp.lib")

// The caller remains on its existing ModuleTask worker. Only WinHTTP performs
// async I/O; callbacks publish completion and never start another operation.
namespace BoundedHttp
{
    constexpr DWORD GetBudgetMs = 4000;
    constexpr DWORD HeadBudgetMs = 1200;
    constexpr size_t MaxBodyBytes = 1024 * 1024;
#ifdef IPREGIONMON_TESTING
    struct CallbackTestState { std::atomic<HANDLE> entered{ nullptr }, release{ nullptr }; };
    inline CallbackTestState& CallbackTest() { static CallbackTestState state; return state; }
#endif

    class Request
    {
        HANDLE ready{ CreateEventW(nullptr, FALSE, FALSE, nullptr) };
        HANDLE drained{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
        HINTERNET session{}, connection{}, request{};
        bool notifyInstalled{ false };
        std::atomic<DWORD> status{ 0 }, callbackError{ 0 }, bytesRead{ 0 };
        ULONGLONG started;
        DWORD budget;

        static void CALLBACK OnStatus(HINTERNET, DWORD_PTR context, DWORD notification,
            void* information, DWORD length) noexcept
        {
            auto& self = *reinterpret_cast<Request*>(context);
            if (notification == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR)
                self.callbackError = static_cast<WINHTTP_ASYNC_RESULT*>(information)->dwError;
            else if (notification == WINHTTP_CALLBACK_STATUS_READ_COMPLETE)
                self.bytesRead = length;
            self.status = notification;
            SetEvent(self.ready);
#ifdef IPREGIONMON_TESTING
            if (notification == WINHTTP_CALLBACK_STATUS_READ_COMPLETE && length == 0)
                if (HANDLE entered = CallbackTest().entered.exchange(nullptr))
                {
                    SetEvent(entered);
                    WaitForSingleObject(CallbackTest().release.load(), 5000);
                }
#endif
        }

        DWORD Wait(BOOL accepted, DWORD expected)
        {
            if (!accepted) return GetLastError();
            const ULONGLONG elapsed = GetTickCount64() - started;
            if (elapsed >= budget) return ERROR_WINHTTP_TIMEOUT;
            const DWORD result = WaitForSingleObject(ready, budget - static_cast<DWORD>(elapsed));
            if (result == WAIT_TIMEOUT || GetTickCount64() - started >= budget)
                return ERROR_WINHTTP_TIMEOUT;
            if (result != WAIT_OBJECT_0) return GetLastError();
            if (status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) return callbackError;
            return status == expected ? ERROR_SUCCESS : ERROR_WINHTTP_INTERNAL_ERROR;
        }
    public:
        explicit Request(DWORD timeout) : started(GetTickCount64()), budget(timeout) {}
        Request(const Request&) = delete;
        Request& operator=(const Request&) = delete;
        ~Request()
        {
            // No API call is still on the caller's stack. Closing this async
            // request cancels pending I/O, but does not itself drain callbacks.
            if (request) WinHttpCloseHandle(request);
            if (connection) WinHttpCloseHandle(connection);
            if (session) WinHttpCloseHandle(session);
            // HANDLE_CLOSING alone is too early to unload callback code. This
            // session event is set by WinHTTP AFTER its last callback returns.
            // Never run this local cleanup from DllMain or a static destructor.
            if (notifyInstalled) WaitForSingleObject(drained, INFINITE);
            if (ready) CloseHandle(ready);
            if (drained) CloseHandle(drained);
        }

        DWORD Run(const std::wstring& url, const std::wstring& proxy, bool noProxy,
            bool head, std::string& body, double& latency)
        {
            if (!ready || !drained) return ERROR_NOT_ENOUGH_MEMORY;
            URL_COMPONENTSW parts{};
            parts.dwStructSize = sizeof(parts);
            parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = static_cast<DWORD>(-1);
            if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) return GetLastError();
            if (parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS)
                return ERROR_WINHTTP_UNRECOGNIZED_SCHEME;
            const bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
            const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
            std::wstring path = parts.dwUrlPathLength ?
                std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) : L"/";
            if (parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
            const auto fragment = path.find(L'#');
            if (fragment != std::wstring::npos) path.resize(fragment);
            session = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
                !proxy.empty() ? WINHTTP_ACCESS_TYPE_NAMED_PROXY : noProxy ? WINHTTP_ACCESS_TYPE_NO_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                proxy.empty() ? WINHTTP_NO_PROXY_NAME : proxy.c_str(), WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
            if (!session) return GetLastError();
            if (!WinHttpSetOption(session, WINHTTP_OPTION_UNLOAD_NOTIFY_EVENT, &drained, sizeof(drained)))
                return GetLastError();
            notifyInstalled = true;
            if (!WinHttpSetTimeouts(session, budget, budget, budget, budget)) return GetLastError();
            DWORD retries = 1;
            if (!WinHttpSetOption(session, WINHTTP_OPTION_CONNECT_RETRIES, &retries, sizeof(retries)))
                return GetLastError();
            DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
            if (!WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols)))
            {
                // Older Windows may not support TLS 1.3; never fall back to the
                // OS's potentially older default protocols if this option fails.
                protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
                if (!WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols)))
                    return GetLastError();
            }
            connection = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
            if (!connection) return GetLastError();
            request = WinHttpOpenRequest(connection, head ? L"HEAD" : L"GET", path.c_str(), nullptr,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
            if (!request) return GetLastError();
            // Keep WinHTTP's certificate validation intact, including through
            // an explicit HTTP CONNECT proxy. A trusted corporate CA belongs in
            // Windows trust policy, not in an ignore-certificate-errors flag.
            DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
            if (!WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects)))
                return GetLastError();
            DWORD_PTR context = reinterpret_cast<DWORD_PTR>(this);
            if (!WinHttpSetOption(request, WINHTTP_OPTION_CONTEXT_VALUE, &context, sizeof(context)))
                return GetLastError();
            if (WinHttpSetStatusCallback(request, &OnStatus,
                WINHTTP_CALLBACK_FLAG_SENDREQUEST_COMPLETE | WINHTTP_CALLBACK_FLAG_HEADERS_AVAILABLE |
                WINHTTP_CALLBACK_FLAG_READ_COMPLETE | WINHTTP_CALLBACK_FLAG_REQUEST_ERROR, 0) == WINHTTP_INVALID_STATUS_CALLBACK)
                return GetLastError();
            if (GetTickCount64() - started >= budget) return ERROR_WINHTTP_TIMEOUT;
            const ULONGLONG sent = GetTickCount64();
            DWORD error = Wait(WinHttpSendRequest(request, L"Connection: close\r\n", -1L,
                WINHTTP_NO_REQUEST_DATA, 0, 0, context), WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE);
            if (error) return error;
            error = Wait(WinHttpReceiveResponse(request, nullptr), WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE);
            if (error) return error;
            latency = static_cast<double>(GetTickCount64() - sent);
            DWORD code{}, length = sizeof(code);
            if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &code, &length, WINHTTP_NO_HEADER_INDEX)) return GetLastError();
            // A redirect left unhandled by the policy must not look like a
            // successful latency sample (notably an HTTPS-to-HTTP redirect).
            if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308)
                return ERROR_WINHTTP_REDIRECT_FAILED;
            if (head) return ERROR_SUCCESS;
            if (code < 200 || code >= 300) return ERROR_WINHTTP_INVALID_SERVER_RESPONSE;
            wchar_t contentLength[32]{};
            length = sizeof(contentLength);
            bool hasLength = WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH,
                WINHTTP_HEADER_NAME_BY_INDEX, contentLength, &length, WINHTTP_NO_HEADER_INDEX) != FALSE;
            ULONGLONG expectedLength = 0;
            if (hasLength)
            {
                wchar_t* end;
                errno = 0;
                expectedLength = _wcstoui64(contentLength, &end, 10);
                if (errno || end == contentLength || *end || contentLength[0] == L'-')
                    return ERROR_WINHTTP_INVALID_SERVER_RESPONSE;
                if (expectedLength > MaxBodyBytes) return ERROR_FILE_TOO_LARGE;
            }
            else if (GetLastError() != ERROR_WINHTTP_HEADER_NOT_FOUND) return GetLastError();
            // A fixed buffer avoids allocating an arbitrary server-advertised chunk.
            // It stays alive until cancellation and all callback activity drain.
            for (;;)
            {
                error = Wait(WinHttpReadData(request, buffer, sizeof(buffer), nullptr), WINHTTP_CALLBACK_STATUS_READ_COMPLETE);
                if (error) return error;
                const DWORD read = bytesRead;
                if (!read) return hasLength && body.size() != expectedLength ?
                    ERROR_WINHTTP_CONNECTION_ERROR : ERROR_SUCCESS;
                if (body.size() + read > MaxBodyBytes) return ERROR_FILE_TOO_LARGE;
                body.append(buffer, read);
            }
        }
    private:
        char buffer[8192]{};
    };

    inline bool Perform(const std::wstring& url, const std::wstring& proxy, bool noProxy,
        bool head, std::string& output, double& latency)
    {
        output.clear();
        latency = -1;
        DWORD error;
        std::string body;
        double measured = -1;
        {
            Request request(head ? HeadBudgetMs : GetBudgetMs);
            error = request.Run(url, proxy, noProxy, head, body, measured);
        }
        if (!error)
        {
            output.swap(body);
            latency = measured;
        }
        SetLastError(error);
        return error == ERROR_SUCCESS;
    }
}
