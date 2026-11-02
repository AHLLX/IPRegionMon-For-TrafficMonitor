#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <stdexcept>
#pragma comment(lib, "ws2_32.lib")

static void Check(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

class Server
{
    SOCKET listener{ INVALID_SOCKET };
    std::thread worker;
    HANDLE stopping{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
    void Run(const std::string& mode)
    {
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) return;
        DWORD receiveTimeout = 2000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char*>(&receiveTimeout), sizeof(receiveTimeout));
        char request[4096];
        if (recv(client, request, sizeof(request), 0) <= 0) { closesocket(client); return; }
        SetEvent(received);
        const std::string response = mode == "large" ?
            "HTTP/1.1 200 OK\r\nContent-Length: 1048577\r\nConnection: close\r\n\r\n" :
            mode == "redirect" ? "HTTP/1.1 302 Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n" :
            mode == "normal" ?
            "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok" :
            mode == "truncated" ? "HTTP/1.1 200 OK\r\nContent-Length: 99\r\n\r\npartial" :
            "HTTP/1.1 200 OK\r\nContent-Length: 999999\r\nConnection: close\r\n\r\n";
        if (mode != "silent" && mode != "headers")
            send(client, response.data(), static_cast<int>(response.size()), 0);
        if (mode == "normal" || mode == "truncated" || mode == "large" || mode == "redirect")
        { closesocket(client); return; }
        const ULONGLONG start = GetTickCount64();
        size_t index = 0;
        while (WaitForSingleObject(stopping, 50) == WAIT_TIMEOUT && GetTickCount64() - start < 6500)
        {
            if (mode != "silent")
            {
                const char byte = mode == "headers" ? response[index++ % response.size()] : ' ';
                if (send(client, &byte, 1, 0) <= 0) { SetEvent(disconnected); break; }
            }
            fd_set readable;
            FD_ZERO(&readable); FD_SET(client, &readable);
            timeval timeout{};
            if (select(0, &readable, nullptr, nullptr, &timeout) > 0)
            {
                char byte;
                if (recv(client, &byte, 1, MSG_PEEK) <= 0) { SetEvent(disconnected); break; }
            }
        }
        closesocket(client);
    }
public:
    HANDLE received{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
    HANDLE disconnected{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
    std::wstring url;
    explicit Server(const std::string& mode)
    {
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Check(listener != INVALID_SOCKET, "socket");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        Check(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind");
        int size = sizeof(address);
        Check(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size) == 0, "getsockname");
        Check(listen(listener, 4) == 0, "listen");
        url = L"http://127.0.0.1:" + std::to_wstring(ntohs(address.sin_port)) + L"/test?value=1";
        worker = std::thread([this, mode]() { Run(mode); });
    }
    ~Server()
    {
        SetEvent(stopping);
        closesocket(listener);
        worker.join();
        CloseHandle(stopping); CloseHandle(received); CloseHandle(disconnected);
    }
};

using Request = BOOL (*)(const wchar_t*, BOOL, DWORD*, DWORD*);
using Start = BOOL (*)(const wchar_t*, HANDLE);

static void Test(Request request, const std::string& mode, bool head)
{
    Server server(mode);
    DWORD error = 0, bytes = 0;
    const ULONGLONG start = GetTickCount64();
    const bool ok = request(server.url.c_str(), head, &error, &bytes) != FALSE;
    const ULONGLONG elapsed = GetTickCount64() - start;
    std::printf("%s %s: ok=%d error=%lu bytes=%lu elapsed=%llu ms\n",
        head ? "HEAD" : "GET", mode.c_str(), ok, error, bytes, elapsed);
    if (mode == "normal") Check(ok && (head || bytes == 2), "normal response failed");
    else
    {
        Check(!ok && bytes == 0, "incomplete response accepted or leaked partial output");
        if (mode == "large") Check(error == ERROR_FILE_TOO_LARGE, "oversized body was not rejected");
        else if (mode == "redirect") Check(error == 12156, "unfollowed redirect accepted");
        else if (mode != "truncated")
        {
            Check(elapsed >= (head ? 900ULL : 3500ULL) && elapsed < (head ? 2200ULL : 5500ULL),
                "full request deadline not enforced");
            Check(error == 12002, "deadline error is not ERROR_WINHTTP_TIMEOUT");
            Check(WaitForSingleObject(server.disconnected, 1000) == WAIT_OBJECT_0, "server did not observe cancellation");
        }
    }
}

static void WaitUnloaded(const wchar_t* path)
{
    const ULONGLONG start = GetTickCount64();
    while (GetModuleHandleW(path) && GetTickCount64() - start < 6500) Sleep(10);
    Check(!GetModuleHandleW(path), "DLL did not unload after cancelled request/callback drain");
}

int wmain(int argc, wchar_t** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc < 2) return 2;
    try
    {
        WSADATA data{};
        Check(WSAStartup(MAKEWORD(2, 2), &data) == 0, "WSAStartup");
        HMODULE module = LoadLibraryW(argv[1]);
        Check(module != nullptr, "LoadLibrary");
        auto request = reinterpret_cast<Request>(GetProcAddress(module, "TestNetworkRequest"));
        auto start = reinterpret_cast<Start>(GetProcAddress(module, "TestNetworkStart"));
        Check(request && start, "test exports missing");
        if (argc == 5 && std::wstring(argv[2]) == L"--probe")
        {
            std::wstring expectation = argv[4];
            const bool getOnly = expectation.size() > 4 &&
                expectation.substr(expectation.size() - 4) == L"-get";
            if (getOnly) expectation.resize(expectation.size() - 4);
            for (BOOL head : { FALSE, TRUE })
            {
                if (head && getOnly) continue;
                DWORD error = 0, bytes = 0;
                const ULONGLONG before = GetTickCount64();
                const bool ok = request(argv[3], head, &error, &bytes) != FALSE;
                std::printf("%s probe: ok=%d error=%lu bytes=%lu elapsed=%llu ms\n",
                    head ? "HEAD" : "GET", ok, error, bytes, GetTickCount64() - before);
                if (expectation == L"invalid-cert")
                    Check(!ok && error == 12175 && bytes == 0, "invalid certificate was not rejected as a TLS error");
                else if (expectation == L"no-downgrade")
                    Check(!ok && error == 12156 && bytes == 0, "HTTPS downgrade was not rejected as a redirect error");
                else Check(ok, "valid HTTPS probe failed");
            }
            FreeLibrary(module);
            return 0;
        }
        if (argc == 5 && std::wstring(argv[2]) == L"--exit-child")
        {
            HANDLE completed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            HANDLE exit = OpenEventW(SYNCHRONIZE, FALSE, argv[4]);
            Check(exit != nullptr, "open exit command");
            Check(start(argv[3], completed) != FALSE, "start exit worker");
            Check(WaitForSingleObject(exit, 10000) == WAIT_OBJECT_0, "exit command missing");
            CloseHandle(exit);
            // A normal EXE return while the DLL worker is still reading. The
            // parent sends the command only after its server sees the request.
            return 0;
        }
        Test(request, "body", false);
        if (argc == 3 && std::wstring(argv[2]) == L"--repro") return 0;
        Test(request, "normal", false); Test(request, "normal", true);
        Test(request, "headers", false); Test(request, "headers", true);
        Test(request, "silent", false); Test(request, "silent", true);
        Test(request, "truncated", false);
        Test(request, "large", false);
        Test(request, "redirect", false); Test(request, "redirect", true);
        DWORD error = 0, bytes = 0;
        const ULONGLONG before = GetTickCount64();
        Check(!request(L"http://127.0.0.1:1/", FALSE, &error, &bytes) && bytes == 0,
            "unavailable server unexpectedly succeeded");
        std::printf("GET refused: error=%lu elapsed=%llu ms\n", error, GetTickCount64() - before);
        FreeLibrary(module);
        WaitUnloaded(argv[1]);
        for (int i = 0; i < 5; ++i)
        {
            Server server("body");
            module = LoadLibraryW(argv[1]);
            Check(module != nullptr, "reload");
            start = reinterpret_cast<Start>(GetProcAddress(module, "TestNetworkStart"));
            HANDLE completed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            Check(start(server.url.c_str(), completed) != FALSE, "start unload request");
            Check(WaitForSingleObject(server.received, 2000) == WAIT_OBJECT_0, "active request missing");
            FreeLibrary(module);
            Check(GetModuleHandleW(argv[1]) != nullptr, "premature unload");
            Check(WaitForSingleObject(completed, 5500) == WAIT_OBJECT_0, "cancelled worker did not finish");
            WaitUnloaded(argv[1]);
            Check(WaitForSingleObject(server.disconnected, 1000) == WAIT_OBJECT_0, "unload request not cancelled");
            CloseHandle(completed);
        }
        {
            Server server("normal");
            module = LoadLibraryW(argv[1]);
            Check(module != nullptr, "callback-drain reload");
            start = reinterpret_cast<Start>(GetProcAddress(module, "TestNetworkStart"));
            using Hold = void (*)(HANDLE, HANDLE);
            auto hold = reinterpret_cast<Hold>(GetProcAddress(module, "TestNetworkHoldCallback"));
            Check(hold != nullptr, "callback test export missing");
            HANDLE entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            HANDLE release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            HANDLE completed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            hold(entered, release);
            Check(start(server.url.c_str(), completed) != FALSE, "start callback-drain worker");
            Check(WaitForSingleObject(entered, 2000) == WAIT_OBJECT_0, "EOF callback missing");
            FreeLibrary(module);
            const bool pending = WaitForSingleObject(completed, 200) == WAIT_TIMEOUT && GetModuleHandleW(argv[1]);
            SetEvent(release);
            Check(pending, "worker/DLL finished before callback returned");
            Check(WaitForSingleObject(completed, 2000) == WAIT_OBJECT_0, "callback drain did not finish");
            WaitUnloaded(argv[1]);
            CloseHandle(entered); CloseHandle(release); CloseHandle(completed);
        }
        wchar_t executable[32768]{};
        Check(GetModuleFileNameW(nullptr, executable, 32768) != 0, "host path");
        for (int i = 0; i < 3; ++i)
        {
            Server server("body");
            const std::wstring eventName = L"Local\\IPRegionMon-NetworkExit-" +
                std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(i);
            HANDLE exit = CreateEventW(nullptr, TRUE, FALSE, eventName.c_str());
            std::wstring command = L"\"" + std::wstring(executable) + L"\" \"" + argv[1] +
                L"\" --exit-child \"" + server.url + L"\" \"" + eventName + L"\"";
            STARTUPINFOW startup{ sizeof(startup) };
            PROCESS_INFORMATION child{};
            Check(CreateProcessW(executable, &command[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                nullptr, nullptr, &startup, &child) != FALSE, "start exit host");
            const bool active = WaitForSingleObject(server.received, 2000) == WAIT_OBJECT_0;
            SetEvent(exit);
            const bool exited = WaitForSingleObject(child.hProcess, 3000) == WAIT_OBJECT_0;
            DWORD code = 1;
            GetExitCodeProcess(child.hProcess, &code);
            // Kill only our standalone child if its shutdown hangs.
            if (!exited) { TerminateProcess(child.hProcess, 99); WaitForSingleObject(child.hProcess, 1000); }
            CloseHandle(child.hThread); CloseHandle(child.hProcess); CloseHandle(exit);
            Check(active && exited && code == 0, "normal process exit during request failed");
            Check(WaitForSingleObject(server.disconnected, 1000) == WAIT_OBJECT_0, "exit did not close connection");
        }
        std::puts("PASS: deadlines, partial rejection, 5 active unloads, callback drain, 3 normal process exits during active requests");
        WSACleanup();
        return 0;
    }
    catch (const std::exception& error) { std::printf("FAIL: %s\n", error.what()); return 1; }
}
