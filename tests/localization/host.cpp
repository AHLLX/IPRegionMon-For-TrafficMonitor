#include <winsock2.h>
#include <Windows.h>
#pragma warning(push)
#pragma warning(disable: 4100) // Unused parameters in the unmodified upstream interface.
#include "../../include/PluginInterface.h"
#pragma warning(pop)
#include <cstdio>
#include <string>
#include <stdexcept>
#include <atomic>
#include <thread>
#pragma comment(lib, "ws2_32.lib")

static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

class Server
{
    SOCKET listener{ INVALID_SOCKET };
    std::thread worker;
    std::atomic<bool> stopping{ false };
public:
    std::wstring base;
    std::atomic<int> received{ 0 };
    std::atomic<int> countryCase{ 0 };
    Server()
    {
        WSADATA data{};
        Check(WSAStartup(MAKEWORD(2, 2), &data) == 0, "WSAStartup");
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        Check(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind");
        int size = sizeof(address);
        Check(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size) == 0, "getsockname");
        Check(listen(listener, 4) == 0, "listen");
        base = L"http://127.0.0.1:" + std::to_wstring(ntohs(address.sin_port));
        worker = std::thread([this]() {
            while (!stopping)
            {
                SOCKET client = accept(listener, nullptr, nullptr);
                if (client == INVALID_SOCKET) break;
                char request[4096]{};
                const int read = recv(client, request, sizeof(request) - 1, 0);
                if (read > 0) ++received;
                const bool get = read > 0 && std::string(request, read).find("GET ") == 0;
                const char* countries[]{
                    "\"country_code\":\"CN\",\"country\":\"\\u4e2d\\u56fd \\u4e0a\\u6d77\"",
                    "\"country_code\":\"gb\",\"country\":\"United Kingdom\",\"region\":\"England\",\"city\":\"Worksop\"",
                    "\"countryCode\":\"GB\",\"country\":\"United Kingdom\",\"regionName\":\"England\",\"city\":\"Worksop\"",
                    "\"country\":\"United Kingdom\",\"region\":\"England\",\"city\":\"Worksop\"",
                    "\"country\":\"GB\",\"region\":\"England\",\"city\":\"Worksop\"",
                    "\"country\":\"Atlantis\",\"city\":\"Unknown city\"",
                    "\"country\":\"\\u4e2d\\u56fd\",\"city\":\"\\u4e0a\\u6d77\""
                };
                const std::string body = get ?
                    std::string("{\"status\":\"success\",\"query\":\"198.51.100.1\",") +
                    countries[countryCase.load()] + ",\"isp\":\"Test ISP\"}" : "";
                const std::string response = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " +
                    std::to_string(body.size()) + "\r\n\r\n" + body;
                send(client, response.c_str(), static_cast<int>(response.size()), 0);
                closesocket(client);
            }
        });
    }
    ~Server() { stopping = true; closesocket(listener); worker.join(); WSACleanup(); }
};

class Host : public ITrafficMonitor
{
public:
    unsigned short language{ 0x0409 };
    int GetAPIVersion() override { return 0; }
    const wchar_t* GetVersion() override { return L"1.86"; }
    double GetMonitorValue(MonitorItem) override { return 0; }
    const wchar_t* GetMonitorValueString(MonitorItem, int) override { return L""; }
    void ShowNotifyMessage(const wchar_t*) override {}
    unsigned short GetLanguageId() const override { return language; }
    const wchar_t* GetPluginConfigDir() const override { return L""; }
    int GetDPI(DPIType) const override { return 96; }
    unsigned int GetThemeColor() const override { return 0; }
    const wchar_t* GetStringRes(const wchar_t*, const wchar_t*) override { return L""; }
    void* GetMainWindowHwnd() override { return nullptr; }
    void* GetTaskbarWindowHwnd() override { return nullptr; }
};

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2 && argc != 3) return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    try
    {
        Server server;
        const HMODULE module = LoadLibraryW(argv[1]);
        Check(module != nullptr, "LoadLibrary");
        using Instance = ITMPlugin* (*)();
        using Smoke = BOOL (*)(BOOL);
        using Configure = void (*)(const wchar_t*, BOOL);
        using Update = void (*)();
        using Busy = BOOL (*)();
        using Width = BOOL (*)();
        const auto instance = reinterpret_cast<Instance>(GetProcAddress(module, "TMPluginGetInstance"));
        const auto smoke = reinterpret_cast<Smoke>(GetProcAddress(module, "TestLocalizedDialogs"));
        const auto configure = reinterpret_cast<Configure>(GetProcAddress(module, "TestConfigure"));
        const auto update = reinterpret_cast<Update>(GetProcAddress(module, "TestUpdate"));
        const auto busy = reinterpret_cast<Busy>(GetProcAddress(module, "TestBusy"));
        const auto countryMode = reinterpret_cast<Update>(GetProcAddress(module, "TestUseCountryFormat"));
        const auto countryNames = reinterpret_cast<Width>(GetProcAddress(module, "TestCountryNames"));
        Check(countryMode && countryNames && countryNames(), "country name/code resolution");
        const auto swap = reinterpret_cast<Update>(GetProcAddress(module, "TestSwap"));
        const auto width = reinterpret_cast<Width>(GetProcAddress(module, "TestTaskbarWidths"));
        Check(instance && smoke && configure && update && busy && swap && width, "exports");
        ITMPlugin* plugin = instance();
        if (argc == 3)
        {
            Check(std::wstring(argv[2]) == L"--startup", "unknown test mode");
            // Only this fresh process receives EI_CONFIG_DIR. Keep the INI in
            // the runner's new artifact directory, away from user settings.
            Check(CreateDirectoryW(L"startup", nullptr) != FALSE, "create test config directory");
            wchar_t directory[MAX_PATH]{};
            Check(GetFullPathNameW(L"startup\\", MAX_PATH, directory, nullptr) != 0, "test config path");
            const std::wstring ini = std::wstring(directory) + L"IPRegionMon.dll.ini";
            const auto write = [&](const wchar_t* key, const std::wstring& value) {
                Check(WritePrivateProfileStringW(L"ip", key, value.c_str(), ini.c_str()) != FALSE, "write test INI");
            };
            write(L"use_proxy", L"0");
            write(L"api_domestic_url", server.base + L"/json");
            write(L"api_foreign_url", server.base + L"/json");
            write(L"latency_targets_n", L"1");
            write(L"latency_name_0", L"loopback");
            write(L"latency_url_0", server.base + L"/latency");
            plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, directory);
            Sleep(100);
            Check(!busy() && server.received == 0, "configured worker started before host language initialization");
            Host host;
            host.language = 0x0804;
            plugin->OnInitialize(&host);
            for (int index = 0; index < 500 && busy(); ++index) Sleep(10);
            Check(!busy() && server.received >= 2, "configured startup update did not complete");
            swap();
            const std::wstring tooltip = plugin->GetTooltipInfo();
            Check(tooltip.find(L"直: ") == 0 && tooltip.find(L"最后更新:") != std::wstring::npos,
                "first configured update used the wrong UI language");
            Check(tooltip.find(L"中国 上海") != std::wstring::npos, "startup geography lost");
            Check(std::wstring(plugin->GetItem(0)->GetItemValueText()) == L"CN",
                "new profile did not default to country code");
            for (const wchar_t* savedFormat : { L"1", L"5" })
            {
                write(L"display_format", savedFormat);
                plugin->OnExtenedInfo(ITMPlugin::EI_CONFIG_DIR, directory);
                for (int attempt = 0; attempt < 500 && busy(); ++attempt) Sleep(10);
                Check(!busy(), "saved-format reload timeout");
                swap();
                const std::wstring value = plugin->GetItem(0)->GetItemValueText();
                Check(savedFormat[0] == L'5' ? value == L"CN" :
                    value.find(L"198.51.100.1") != std::wstring::npos, "saved display format was replaced");
            }
            std::printf("PASS: EI_CONFIG_DIR before OnInitialize, first configured worker uses host language\n");
            std::puts("PASS: default country format; legacy and country formats survive INI reload");
            FreeLibrary(module);
            return 0;
        }
        const wchar_t* before = plugin->GetInfo(ITMPlugin::TMI_NAME);
        const std::wstring retained(before);
        Host host;
        const unsigned short languages[]{ 0x0409, 0x0804, 0x0404, 0x0419 };
        for (const auto language : languages)
        {
            host.language = language;
            const bool chinese = PRIMARYLANGID(language) == LANG_CHINESE;
            SetThreadUILanguage(chinese ? 0x0409 : 0x0804);
            const LANGID threadLanguage = GetThreadUILanguage();
            plugin->OnInitialize(&host);
            Check(plugin->GetInfo(ITMPlugin::TMI_NAME) ==
                std::wstring(chinese ? L"IP 地区监控插件" : L"IP Region Monitor"), "metadata language");
            Check(retained == before, "resource pointer invalidated by initialization");
            Check(!busy(), "OnInitialize without config started a request");
            Check(plugin->GetItem(0)->GetItemLableText() == std::wstring(chinese ? L"直:" : L"Direct:"), "item label language");
            Check(plugin->GetItem(1)->GetItemLableText() == std::wstring(chinese ? L"代:" : L"Proxy:"), "proxy item label language");
            configure(server.base.c_str(), FALSE);
            update();
            for (int index = 0; index < 500 && busy(); ++index) Sleep(10);
            Check(!busy(), "loopback update timeout");
            swap();
            const std::wstring tooltip = plugin->GetTooltipInfo();
            Check(tooltip.find(chinese ? L"直: " : L"Direct: ") == 0, "tooltip language");
            Check(tooltip.find(L"中国 上海") != std::wstring::npos, "API geography was translated");
            Check(tooltip.find(chinese ? L"最后更新:" : L"Last updated:") != std::wstring::npos, "tooltip status language");
            Check(smoke(chinese) != FALSE, "native dialog resource selection");
            Check(width() != FALSE, "taskbar width or compact text");
            Check(GetThreadUILanguage() == threadLanguage, "host thread language changed");
            std::printf("PASS: host language %04x, native templates, metadata, retained pointer, thread locale\n", language);
        }
        host.language = 0x0409;
        plugin->OnInitialize(&host);
        configure(server.base.c_str(), FALSE);
        countryMode();
        for (int index = 1; index <= 6; ++index)
        {
            server.countryCase = index;
            update();
            for (int attempt = 0; attempt < 500 && busy(); ++attempt) Sleep(10);
            Check(!busy(), "country update timeout");
            swap();
            plugin->OnExtenedInfo(ITMPlugin::EI_DRAW_TASKBAR_WND, L"1");
            auto* item = plugin->GetItem(0);
            const std::wstring sample = item->GetItemValueSampleText();
            const std::wstring expected = index == 5 ? L"--" : index == 6 ? L"CN" : L"GB";
            Check(sample == expected && item->GetItemValueText() == expected,
                "taskbar country code differs from provider/name fallback");
            const std::wstring tooltip = plugin->GetTooltipInfo();
            Check(tooltip.find(L"198.51.100.1") != std::wstring::npos, "country view lost tooltip IP");
            const std::wstring location = index == 5 ? L"Atlantis Unknown city" :
                index == 6 ? L"中国 上海" : L"United Kingdom England Worksop";
            Check(tooltip.find(location) != std::wstring::npos, "country view truncated full tooltip location");
            Check(smoke(FALSE), "country format was not preserved by settings controls");
        }
        std::puts("PASS: country-code fields, legacy country names, code-only provider, unknown country; full tooltip; settings roundtrip");
        FreeLibrary(module);
        return 0;
    }
    catch (const std::exception& error) { std::printf("FAIL: %s\n", error.what()); return 1; }
}
