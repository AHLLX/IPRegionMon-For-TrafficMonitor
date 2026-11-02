#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
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
    std::atomic<bool> stopping{ false }, block{ false };
public:
    HANDLE received{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
    HANDLE release{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
    std::atomic<int> ipRequests{ 0 };
    std::wstring base;
    Server()
    {
        WSADATA data{};
        Check(WSAStartup(MAKEWORD(2, 2), &data) == 0, "WSAStartup");
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Check(listener != INVALID_SOCKET, "socket");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        Check(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind");
        int length = sizeof(address);
        Check(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) == 0, "getsockname");
        Check(listen(listener, 16) == 0, "listen");
        base = L"http://127.0.0.1:" + std::to_wstring(ntohs(address.sin_port));
        worker = std::thread([this]() { Run(); });
    }
    ~Server()
    {
        stopping = true;
        SetEvent(release);
        closesocket(listener);
        worker.join();
        CloseHandle(received);
        CloseHandle(release);
        WSACleanup();
    }
    void Arm()
    {
        ResetEvent(received);
        ResetEvent(release);
        block = true;
    }
private:
    void Run()
    {
        while (!stopping)
        {
            SOCKET client = accept(listener, nullptr, nullptr);
            if (client == INVALID_SOCKET) break;
            char buffer[4096]{};
            const int count = recv(client, buffer, sizeof(buffer) - 1, 0);
            const std::string request = count > 0 ? std::string(buffer, count) : "";
            const bool ip = request.find("/ip") != std::string::npos;
            if (ip) ++ipRequests;
            if (block.exchange(false))
            {
                SetEvent(received);
                WaitForSingleObject(release, 3000);
            }
            const bool second = request.find("/ipB") != std::string::npos;
            const std::string body = ip ?
                std::string("{\"status\":\"success\",\"query\":\"198.51.100.") +
                (second ? "2" : "1") + "\",\"country\":\"test\",\"isp\":\"test\"}" : "";
            const std::string response = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " +
                std::to_string(body.size()) + "\r\n\r\n" + body;
            send(client, response.c_str(), static_cast<int>(response.size()), 0);
            closesocket(client);
        }
    }
};

struct Api
{
    HMODULE module;
    void (*configure)(const wchar_t*, BOOL);
    void (*update)();
    BOOL (*busy)();
    void (*swap)();
    void (*failure)(int);
    void (*failUpdate)();
    int (*state)();
    const wchar_t* (*text)();
    const wchar_t* (*tooltip)();
    BOOL (*closeDialog)(const wchar_t*, BOOL, HANDLE);
    BOOL (*completeProxy)(const wchar_t*);
    explicit Api(const wchar_t* path)
    {
        module = LoadLibraryW(path);
        Check(module != nullptr, "LoadLibrary");
#define RESOLVE(member, name) member = reinterpret_cast<decltype(member)>(GetProcAddress(module, name)); Check(member != nullptr, name)
        RESOLVE(configure, "TestConfigure"); RESOLVE(update, "TestUpdate");
        RESOLVE(busy, "TestBusy"); RESOLVE(swap, "TestSwap");
        RESOLVE(failure, "TestFailure"); RESOLVE(state, "TestState");
        RESOLVE(failUpdate, "TestFailUpdate");
        RESOLVE(text, "TestText"); RESOLVE(tooltip, "TestTooltip");
        RESOLVE(closeDialog, "TestCloseDialog");
        RESOLVE(completeProxy, "TestCompleteProxy");
#undef RESOLVE
    }
    void Finish()
    {
        for (int i = 0; i < 1000 && busy(); ++i) Sleep(10);
        Check(!busy(), "worker timeout");
        swap();
    }
};

static void WaitUnloaded(const wchar_t* path)
{
    for (int i = 0; i < 500 && GetModuleHandleW(path); ++i) Sleep(10);
    Check(!GetModuleHandleW(path), "DLL did not unload after worker completion");
}

int wmain(int argc, wchar_t** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc != 2) return 2;
    try
    {
        Server server;
        Api api(argv[1]);
        api.configure(server.base.c_str(), FALSE);
        for (int mode : { 1, 2 })
        {
            api.failure(mode);
            api.update();
            Check(!api.busy(), "launch failure left busy flag set");
        }
        api.failure(0);
        api.failUpdate();
        api.update();
        api.Finish();
        Check(server.ipRequests == 0, "injected worker exception was not exercised");
        server.Arm();
        HANDLE go = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        std::vector<std::thread> callers;
        for (int i = 0; i < 64; ++i)
            callers.emplace_back([&]() { WaitForSingleObject(go, 5000); api.update(); });
        SetEvent(go);
        for (auto& caller : callers) caller.join();
        CloseHandle(go);
        Check(WaitForSingleObject(server.received, 2000) == WAIT_OBJECT_0, "blocked request missing");
        Check(api.busy() && server.ipRequests == 1, "concurrent updates overlapped");

        // This must finish while the network response remains blocked.
        HANDLE changed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        std::thread writer([&]() { api.configure(server.base.c_str(), TRUE); SetEvent(changed); });
        const bool unlocked = WaitForSingleObject(changed, 1000) == WAIT_OBJECT_0;
        SetEvent(server.release);
        writer.join();
        CloseHandle(changed);
        Check(unlocked, "network operation held the settings mutex");
        api.Finish();
        Check(api.state() == 1, "worker mixed settings generations");

        const wchar_t* heldText = api.text();
        const wchar_t* heldTooltip = api.tooltip();
        const std::wstring textCopy(heldText), tooltipCopy(heldTooltip);
        std::atomic<bool> stop{ false }, valid{ true };
        std::vector<std::thread> readers;
        for (int i = 0; i < 8; ++i)
            readers.emplace_back([&]() {
                while (!stop)
                {
                    if (api.state() < 0) valid = false;
                    api.text(); api.tooltip();
                    std::this_thread::yield();
                }
            });
        for (int i = 0; i < 20; ++i)
        {
            api.configure(server.base.c_str(), i % 2);
            api.update();
            api.Finish();
        }
        stop = true;
        for (auto& reader : readers) reader.join();
        Check(valid && textCopy == heldText && tooltipCopy == heldTooltip, "snapshot or retained pointer changed");
        FreeLibrary(api.module);
        WaitUnloaded(argv[1]);

        Api unloading(argv[1]);
        unloading.configure(server.base.c_str(), FALSE);
        server.Arm();
        unloading.update();
        Check(WaitForSingleObject(server.received, 2000) == WAIT_OBJECT_0, "unload request missing");
        FreeLibrary(unloading.module);
        Check(GetModuleHandleW(argv[1]) != nullptr, "DLL unloaded with an active request");
        SetEvent(server.release);
        WaitUnloaded(argv[1]);
        {
            Api completed(argv[1]);
            Check(completed.completeProxy(server.base.c_str()) != FALSE,
                "proxy-page completion or launch failure did not restore its controls");
            FreeLibrary(completed.module);
            WaitUnloaded(argv[1]);
        }
        for (BOOL proxy : { FALSE, TRUE })
        {
            Api window(argv[1]);
            window.configure(server.base.c_str(), FALSE);
            server.Arm();
            Check(window.closeDialog(server.base.c_str(), proxy, server.received) != FALSE,
                "closing the settings page did not detach its active result");
            FreeLibrary(window.module);
            Check(GetModuleHandleW(argv[1]) != nullptr, "settings worker lost its DLL");
            SetEvent(server.release);
            WaitUnloaded(argv[1]);
        }
        std::puts("PASS: 64 simultaneous starts; launch/worker failures; settings snapshot; 8 readers/20 updates; retained strings; real-worker DLL unload; proxy-page result delivery; close both settings pages during requests");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::printf("FAIL: %s\n", error.what());
        return 1;
    }
}
