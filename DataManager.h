#pragma once
#include <string>
#include <map>
#include <vector>
#include <Windows.h>
#include <atomic>
#include <mutex>

#define g_data CDataManager::Instance()

struct IpThreatInfo {
    bool is_proxy{false};
    bool is_vpn{false};
    bool is_datacenter{false}; // Hosting / Datacenter
    bool is_tor{false};
    bool is_mobile{false};     // Cellular
    bool is_abuser{false};     // 恶意/黑名单
    std::wstring risk_score;   // 欺诈分/风险分
};

struct LatencyTarget
{
    std::wstring name;   // 展示名称
    std::wstring url;    // 测试地址（域名或URL）
};

enum DisplayFormat
{
    DF_REGION_ONLY = 0,        // 仅地区 (例如: CN 上海)
    DF_REGION_AND_IP = 1,      // 地区 + IP (例如: CN 上海 (1.2.3.4))
    DF_IP_ONLY = 2,            // 仅 IP (例如: 1.2.3.4)
    DF_REGION_AND_LATENCY = 3, // 地区 + 延迟 (例如: CN 上海 15ms)
    DF_FULL = 4                // 完整 (例如: CN 上海 电信 (1.2.3.4))
};

struct SettingData
{
    bool show_second{};
    // 是否使用代理访问（用于国外API）
    bool use_proxy{};
    // 代理地址，例如：127.0.0.1:7890 （HTTP 代理）
    std::wstring proxy_address;
    // 国内与国外 API 地址
    std::wstring api_domestic_url;   // 国内线路 API
    std::wstring api_foreign_url;    // 国外线路 API
    // 兼容旧版（单一 URL），若不为空作为两个 URL 的默认值
    std::wstring api_url;
    // 刷新间隔（秒）
    int refresh_interval_sec{ 300 }; // 默认 5 分钟
    // 是否在数值文本中显示IP（兼容旧版设置）
    bool show_ip{ true };
    // 显示格式 (0: 仅地区, 1: 地区+IP, 2: 仅IP, 3: 地区+延迟, 4: 完整信息)
    int display_format{ DF_REGION_AND_IP };
    // 延迟目标（动态列表）
    std::vector<LatencyTarget> latency_targets;
};

struct LatencyResult
{
    std::wstring name, url;
    double direct_ms{ -1 }, proxy_ms{ -1 };
};

struct ResultSnapshot
{
    std::wstring m_public_ip_direct, m_public_ip_proxy;
    std::wstring m_place_direct, m_place_proxy;
    std::wstring m_isp_direct, m_isp_proxy;
    IpThreatInfo m_threat_direct, m_threat_proxy;
    std::vector<LatencyResult> m_latency_results;
    std::wstring m_tooltip, m_ip_region_display;
    std::wstring m_direct_item_text, m_proxy_item_text;
};

class CDataManager
{
#ifdef IPREGIONMON_TESTING
    friend struct ConcurrencyTestAccess;
#endif
    CDataManager();
    ~CDataManager() = default;
public:
    using LatencyResult = ::LatencyResult;
    struct Snapshot { SettingData settings; ResultSnapshot results; };
    static CDataManager& Instance();

    SettingData GetSettings() const;
    void SetSettings(SettingData settings);
    ResultSnapshot GetResults() const;
    Snapshot GetSnapshot() const;
    void LoadConfig(const std::wstring& config_dir);
    void SaveConfig() const;
    const wchar_t* StringRes(UINT id);

    void UpdateIpInfoIfNeeded();
    void ForceUpdateAsync();
    bool IsUpdating() const;
    void SwapBuffers();
    void ReformatTaskbarTexts();
    int GetRefreshCountdownSec() const;
    std::wstring GetLastUpdateStatusString() const;
    std::wstring FormatItemText(bool is_proxy) const;
    // Valid until this thread calls the same getter again, exits, or DLL unloads.
    const wchar_t* GetFormattedItemText(bool is_proxy) const;
    const wchar_t* GetTooltipText() const;

    bool TestProxyConnectivity(const std::wstring& proxy, std::wstring& out_message, double& out_ms);
    static bool MeasureLatencyMs(const std::wstring& url_or_host, const std::wstring& proxy, bool no_proxy, double& out_ms);
    void LogInfo(const wchar_t* format, ...);
private:
    void UpdateIpInfoNow();
    bool FetchIpInfoByUrl(const std::wstring& url, bool use_proxy, const std::wstring& proxy_address,
        std::wstring& out_place, std::wstring& out_ip, std::wstring& out_isp, IpThreatInfo& out_threat);
    bool FetchIpInfoWithFallback(const std::wstring& preferredUrl, bool use_proxy, const std::wstring& proxy_address,
        std::wstring& out_place, std::wstring& out_ip, std::wstring& out_isp, IpThreatInfo& out_threat);
    void RotateLogIfNeeded(const std::wstring& log_path);

    static CDataManager m_instance;
    SettingData m_setting_data;
    ResultSnapshot m_results, m_pending;
    std::wstring m_config_path;
    std::map<UINT, std::wstring> m_string_table;
    struct ReaderText { std::wstring direct, proxy, tooltip; };
    // No non-trivial thread_local destructors may outlive an unloaded DLL.
    mutable std::map<DWORD, ReaderText> m_reader_texts;
    mutable std::mutex m_swap_mutex;
    mutable std::mutex m_config_mutex;
    std::mutex m_log_mutex;
    bool m_has_new_data{ false };
    std::atomic<bool> m_is_updating{ false };
    std::atomic<ULONGLONG> m_last_ip_update_tick{ 0 };
    SYSTEMTIME m_last_update_time{};
    int m_direct_fail_count{ 0 }, m_proxy_fail_count{ 0 };
#ifdef IPREGIONMON_TESTING
    std::atomic<bool> m_fail_update_for_test{ false };
#endif
};
