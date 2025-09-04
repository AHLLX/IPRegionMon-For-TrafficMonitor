#include "pch.h"
#include "DataManager.h"
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

// declare __ImageBase to get current module path when used inside DLL
extern "C" IMAGE_DOS_HEADER __ImageBase;

CDataManager CDataManager::m_instance;

static std::wstring Utf8ToWide(const std::string& utf8)
{
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    std::wstring w; w.resize(len);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &w[0], len);
    return w;
}

static std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s; s.resize(len);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], len, nullptr, nullptr);
    return s;
}

static std::wstring Utf8HexEncode(const std::wstring& w)
{
    if (w.empty()) return L"";
    std::string u8 = WideToUtf8(w);
    static const wchar_t* hex = L"0123456789ABCDEF";
    std::wstring out; out.reserve(4 + u8.size() * 2);
    out += L"#u8:";
    for (unsigned char c : u8)
    {
        out.push_back(hex[(c >> 4) & 0xF]);
        out.push_back(hex[c & 0xF]);
    }
    return out;
}

static std::wstring Utf8HexDecode(const std::wstring& s)
{
    if (s.size() < 4 || s.substr(0, 4) != L"#u8:") return s;
    auto hex2 = [](wchar_t ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        return 0;
    };
    std::string bytes; bytes.reserve((s.size() - 4) / 2);
    for (size_t i = 4; i + 1 < s.size(); i += 2)
    {
        unsigned char b = (unsigned char)((hex2(s[i]) << 4) | hex2(s[i + 1]));
        bytes.push_back((char)b);
    }
    return Utf8ToWide(bytes);
}

// 朴素JSON字段提取（不处理转义，足够用于简单公共IP服务）
static std::wstring GetJsonStringValue(const std::wstring& json, const wchar_t* key)
{
    std::wstring keyQuoted = L"\"" + std::wstring(key) + L"\"";
    size_t kpos = json.find(keyQuoted);
    if (kpos == std::wstring::npos) return L"";
    size_t colon = json.find(L':', kpos + keyQuoted.size());
    if (colon == std::wstring::npos) return L"";
    // 跳过空白
    size_t pos = colon + 1;
    while (pos < json.size() && iswspace(json[pos])) ++pos;
    if (pos >= json.size()) return L"";
    if (json[pos] == L'"')
    {
        size_t firstQuote = pos;
        size_t endQuote = json.find(L'"', firstQuote + 1);
        if (endQuote == std::wstring::npos) return L"";
        return json.substr(firstQuote + 1, endQuote - firstQuote - 1);
    }
    // 如果不是字符串（例如 true/false/number），读取到逗号或结束
    size_t end = pos;
    while (end < json.size() && json[end] != L',' && json[end] != L'}' && json[end] != L']') ++end;
    return json.substr(pos, end - pos);
}

// 通过 WinHTTP 进行 GET 请求
// proxy 为空且 no_proxy 为 true 时，强制直连（绕过系统代理）
static bool HttpGet(const std::wstring& url, const std::wstring& proxy, bool no_proxy, std::string& out)
{
    URL_COMPONENTSW uc{}; uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{}; wchar_t path[2048]{}; wchar_t scheme[16]{};
    uc.lpszHostName = host; uc.dwHostNameLength = _countof(host);
    uc.lpszUrlPath = path; uc.dwUrlPathLength = _countof(path);
    uc.lpszScheme = scheme; uc.dwSchemeLength = _countof(scheme);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
        return false;
    bool https = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    DWORD accessType = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
    if (no_proxy && proxy.empty())
        accessType = WINHTTP_ACCESS_TYPE_NO_PROXY;

    HINTERNET hs = WinHttpOpen(L"TrafficMonitor-IPPlugin/1.0",
        accessType, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hs) return false;

    // 设置超时，避免UI卡死（单位ms）
    WinHttpSetTimeouts(hs, 5000, 5000, 5000, 5000);

    if (!proxy.empty())
    {
        std::wstring s = L"http=" + proxy + L";https=" + proxy;
        WINHTTP_PROXY_INFO pi{}; pi.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
        pi.lpszProxy = const_cast<wchar_t*>(s.c_str());
        WinHttpSetOption(hs, WINHTTP_OPTION_PROXY, &pi, sizeof(pi));
    }

    HINTERNET hc = WinHttpConnect(hs, uc.lpszHostName, uc.nPort, 0);
    if (!hc) { WinHttpCloseHandle(hs); return false; }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hr = WinHttpOpenRequest(hc, L"GET", uc.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hr) { WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return false; }

    BOOL ok = WinHttpSendRequest(hr, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hr, nullptr);

    if (ok)
    {
        DWORD sz = 0;
        do {
            DWORD got = 0;
            if (!WinHttpQueryDataAvailable(hr, &sz) || sz == 0) break;
            std::string buf; buf.resize(sz);
            if (!WinHttpReadData(hr, &buf[0], sz, &got)) break;
            buf.resize(got);
            out += buf;
        } while (sz > 0);
    }

    WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
    return !out.empty();
}

static bool MeasureLatencyMs(const std::wstring& url_or_host, const std::wstring& proxy, bool no_proxy, double& out_ms)
{
    out_ms = -1.0;
    // 解析主机与路径，允许传入域名或完整URL
    std::wstring url = url_or_host;
    if (url.find(L"://") == std::wstring::npos)
        url = L"https://" + url; // 默认https
    URL_COMPONENTSW uc{}; uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{}; wchar_t path[2048]{}; wchar_t scheme[16]{};
    uc.lpszHostName = host; uc.dwHostNameLength = _countof(host);
    uc.lpszUrlPath = path; uc.dwUrlPathLength = _countof(path);
    uc.lpszScheme = scheme; uc.dwSchemeLength = _countof(scheme);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
        return false;
    bool https = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    DWORD accessType = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
    if (no_proxy && proxy.empty())
        accessType = WINHTTP_ACCESS_TYPE_NO_PROXY;

    HINTERNET hs = WinHttpOpen(L"TrafficMonitor-IPPlugin/1.0",
        accessType, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hs) return false;

    // 设置超时
    WinHttpSetTimeouts(hs, 3000, 3000, 3000, 3000);

    if (!proxy.empty())
    {
        std::wstring s = L"http=" + proxy + L";https=" + proxy;
        WINHTTP_PROXY_INFO pi{}; pi.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
        pi.lpszProxy = const_cast<wchar_t*>(s.c_str());
        WinHttpSetOption(hs, WINHTTP_OPTION_PROXY, &pi, sizeof(pi));
    }

    HINTERNET hc = WinHttpConnect(hs, uc.lpszHostName, uc.nPort, 0);
    if (!hc) { WinHttpCloseHandle(hs); return false; }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hr = WinHttpOpenRequest(hc, L"HEAD", uc.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hr) { WinHttpCloseHandle(hc); WinHttpCloseHandle(hs); return false; }

    ULONGLONG t0 = GetTickCount64();
    BOOL ok = WinHttpSendRequest(hr, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hr, nullptr);
    ULONGLONG t1 = GetTickCount64();

    if (ok)
        out_ms = static_cast<double>(t1 - t0);

    WinHttpCloseHandle(hr); WinHttpCloseHandle(hc); WinHttpCloseHandle(hs);
    return ok == TRUE;
}

CDataManager::CDataManager()
{
    // 默认 API：使用 http 并包含 message 字段，确保返回失败原因
    const wchar_t* kDefaultApi = L"http://ip-api.com/json/?fields=status,message,country,regionName,city,isp,query&lang=zh-CN";
    m_setting_data.api_domestic_url = kDefaultApi;
    m_setting_data.api_foreign_url  = kDefaultApi;
    m_setting_data.use_proxy = false;
    m_setting_data.refresh_interval_sec = 300;
    m_setting_data.show_ip = true;

    m_ip_update_interval_ms = m_setting_data.refresh_interval_sec * 1000u;
}

CDataManager::~CDataManager()
{
    SaveConfig();
}

CDataManager& CDataManager::Instance()
{
    return m_instance;
}

void CDataManager::LoadConfig(const std::wstring& config_dir)
{
    //获取模块的路径
    HMODULE hModule = reinterpret_cast<HMODULE>(&__ImageBase);
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(hModule, path, MAX_PATH);
    std::wstring module_path = path;
    m_config_path = module_path;
    if (!config_dir.empty())
    {
        size_t index = module_path.find_last_of(L"\\/");
        std::wstring module_file_name = module_path.substr(index + 1);
        m_config_path = config_dir + module_file_name;
    }
    m_config_path += L".ini";

    m_setting_data.show_second = GetPrivateProfileIntW(L"config", L"show_second", 0, m_config_path.c_str());
    m_setting_data.use_proxy = GetPrivateProfileIntW(L"ip", L"use_proxy", 0, m_config_path.c_str()) != 0;

    wchar_t buff[1024]{};
    GetPrivateProfileStringW(L"ip", L"proxy_address", L"127.0.0.1:7890", buff, _countof(buff), m_config_path.c_str());
    m_setting_data.proxy_address = buff;

    m_setting_data.refresh_interval_sec = GetPrivateProfileIntW(L"ip", L"refresh_interval_sec", 300, m_config_path.c_str());
    m_setting_data.show_ip = GetPrivateProfileIntW(L"ip", L"show_ip", 1, m_config_path.c_str()) != 0;

    GetPrivateProfileStringW(L"ip", L"api_url", L"", buff, _countof(buff), m_config_path.c_str());
    m_setting_data.api_url = buff;

    GetPrivateProfileStringW(L"ip", L"api_domestic_url", L"", buff, _countof(buff), m_config_path.c_str());
    std::wstring domestic = buff;
    GetPrivateProfileStringW(L"ip", L"api_foreign_url", L"", buff, _countof(buff), m_config_path.c_str());
    std::wstring foreign = buff;

    // 新增：读取延迟目标
    // 旧键兼容：latency_target（单项）
    GetPrivateProfileStringW(L"ip", L"latency_target", L"", buff, _countof(buff), m_config_path.c_str());
    std::wstring single_latency = Utf8HexDecode(buff);
    m_setting_data.latency_targets.clear();
    // 多项：latency_targets_n 与 latency_name_i/latency_url_i
    int n = GetPrivateProfileIntW(L"ip", L"latency_targets_n", (single_latency.empty() ? 0 : 1), m_config_path.c_str());
    if (n <= 0 && !single_latency.empty())
    {
        m_setting_data.latency_targets.push_back({ L"默认", single_latency });
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            wchar_t key_name[64]{}; swprintf_s(key_name, L"latency_name_%d", i);
            wchar_t key_url[64]{};  swprintf_s(key_url,  L"latency_url_%d", i);
            GetPrivateProfileStringW(L"ip", key_name, L"", buff, _countof(buff), m_config_path.c_str());
            std::wstring name = Utf8HexDecode(buff);
            GetPrivateProfileStringW(L"ip", key_url, L"", buff, _countof(buff), m_config_path.c_str());
            std::wstring url = Utf8HexDecode(buff);
            if (!url.empty())
                m_setting_data.latency_targets.push_back({ name, url });
        }
    }

    if (!domestic.empty()) m_setting_data.api_domestic_url = domestic; else if (!m_setting_data.api_url.empty()) m_setting_data.api_domestic_url = m_setting_data.api_url;
    if (!foreign.empty())  m_setting_data.api_foreign_url  = foreign;  else if (!m_setting_data.api_url.empty()) m_setting_data.api_foreign_url  = m_setting_data.api_url;

    // 如果仍为空，回退到 http + message 的默认接口
    const wchar_t* kDefaultApi = L"http://ip-api.com/json/?fields=status,message,country,regionName,city,isp,query&lang=zh-CN";
    if (m_setting_data.api_domestic_url.empty()) m_setting_data.api_domestic_url = kDefaultApi;
    if (m_setting_data.api_foreign_url.empty())  m_setting_data.api_foreign_url  = kDefaultApi;

    if (m_setting_data.refresh_interval_sec < 5) m_setting_data.refresh_interval_sec = 5;
    m_ip_update_interval_ms = (unsigned int)m_setting_data.refresh_interval_sec * 1000u;
}

static void WritePrivateProfileInt(const wchar_t* app, const wchar_t* key, int v, const wchar_t* file)
{
    wchar_t b[32]; swprintf_s(b, L"%d", v); WritePrivateProfileStringW(app, key, b, file);
}

void CDataManager::SaveConfig() const
{
    WritePrivateProfileInt(L"config", L"show_second", m_setting_data.show_second, m_config_path.c_str());
    WritePrivateProfileInt(L"ip", L"use_proxy", m_setting_data.use_proxy ? 1 : 0, m_config_path.c_str());
    WritePrivateProfileStringW(L"ip", L"proxy_address", m_setting_data.proxy_address.c_str(), m_config_path.c_str());
    WritePrivateProfileStringW(L"ip", L"api_domestic_url", m_setting_data.api_domestic_url.c_str(), m_config_path.c_str());
    WritePrivateProfileStringW(L"ip", L"api_foreign_url", m_setting_data.api_foreign_url.c_str(), m_config_path.c_str());
    WritePrivateProfileInt(L"ip", L"refresh_interval_sec", m_setting_data.refresh_interval_sec, m_config_path.c_str());
    WritePrivateProfileInt(L"ip", L"show_ip", m_setting_data.show_ip ? 1 : 0, m_config_path.c_str());

    // 保存延迟目标（新版多项）
    WritePrivateProfileStringW(L"ip", L"latency_target", NULL, m_config_path.c_str()); // 清理旧键
    WritePrivateProfileInt(L"ip", L"latency_targets_n", (int)m_setting_data.latency_targets.size(), m_config_path.c_str());
    for (int i = 0; i < (int)m_setting_data.latency_targets.size(); ++i)
    {
        wchar_t key_name[64]{}; swprintf_s(key_name, L"latency_name_%d", i);
        wchar_t key_url[64]{};  swprintf_s(key_url,  L"latency_url_%d", i);
        std::wstring encName = Utf8HexEncode(m_setting_data.latency_targets[i].name);
        std::wstring encUrl  = Utf8HexEncode(m_setting_data.latency_targets[i].url);
        WritePrivateProfileStringW(L"ip", key_name, encName.c_str(), m_config_path.c_str());
        WritePrivateProfileStringW(L"ip", key_url,  encUrl.c_str(),  m_config_path.c_str());
    }
}

const wchar_t* CDataManager::StringRes(UINT id)
{
    auto it = m_string_table.find(id);
    if (it != m_string_table.end()) return it->second.c_str();
#ifdef AFX_MANAGE_STATE
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    HINSTANCE hResInst = AfxGetResourceHandle();
#else
    HINSTANCE hResInst = reinterpret_cast<HINSTANCE>(&__ImageBase);
#endif
    wchar_t buf[256]{};
    if (::LoadStringW(hResInst, id, buf, _countof(buf)) > 0) { m_string_table[id] = buf; return m_string_table[id].c_str(); }
    static const wchar_t* empty = L""; return empty;
}

// signature update
bool CDataManager::FetchIpInfoByUrl(const std::wstring& url, bool use_proxy, std::wstring& out_place, std::wstring& out_ip, std::wstring& out_isp)
{
    std::string resp;
    std::wstring proxy = use_proxy ? m_setting_data.proxy_address : L"";
    bool no_proxy = !use_proxy; // 直连时强制绕过系统代理
    if (!HttpGet(url, proxy, no_proxy, resp)) return false;

    std::wstring w = Utf8ToWide(resp);
    std::wstring status = GetJsonStringValue(w, L"status");
    std::wstring message = GetJsonStringValue(w, L"message");
    std::wstring ip = GetJsonStringValue(w, L"query"); if (ip.empty()) ip = GetJsonStringValue(w, L"ip");
    std::wstring country = GetJsonStringValue(w, L"country"); if (country.empty()) country = GetJsonStringValue(w, L"country_name");
    std::wstring region = GetJsonStringValue(w, L"regionName"); if (region.empty()) region = GetJsonStringValue(w, L"region");
    std::wstring city = GetJsonStringValue(w, L"city");
    std::wstring isp = GetJsonStringValue(w, L"isp");

    if (status == L"fail" || status == L"error")
    {
        out_ip.clear(); out_place = message; out_isp.clear();
        return false;
    }

    if (ip.empty()) return false;

    out_ip = ip; out_isp = isp;
    std::wstring place;
    if (!country.empty()) place += country;
    if (!region.empty()) { if (!place.empty()) place += L" "; place += region; }
    if (!city.empty())   { if (!place.empty()) place += L" "; place += city; }
    out_place = place;
    return true;
}

static void AppendWrappedList(std::wstring& out, const std::vector<std::pair<std::wstring, int>>& items,
                              const wchar_t* prefix, size_t perLine)
{
    std::wstring indent(prefix);
    std::wstring pad; pad.resize(indent.size(), L' ');

    size_t n = items.size(); size_t i = 0; bool firstLine = true;
    while (i < n)
    {
        if (!firstLine) { out += pad; } else { out += indent; }
        size_t cnt = 0;
        while (i < n && cnt < perLine)
        {
            if (cnt > 0) out += L" | ";
            out += items[i].first; out += L" - ";
            wchar_t b[32]{}; swprintf_s(b, L"%dms", items[i].second); out += b;
            ++i; ++cnt;
        }
        out += L"\n";
        firstLine = false;
    }
    if (n == 0)
    {
        out += indent; out += L"-\n";
    }
}

void CDataManager::UpdateStatusStrings(bool updated, bool is_domestic)
{
    if (!updated) { m_tooltip.clear(); return; }

    SYSTEMTIME st{}; GetLocalTime(&st); m_last_update_time = st;
    wchar_t tb[64]{}; swprintf_s(tb, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    m_last_source_domestic = is_domestic;

    // 鼠标提示：直/代 + 地区
    m_tooltip.clear();
    if (!m_public_ip_direct.empty())
    {
        m_tooltip += L"直: ";
        m_tooltip += m_public_ip_direct;
        if (!m_place_direct.empty()) { m_tooltip += L"  ["; m_tooltip += m_place_direct; m_tooltip += L"]"; }
        if (!m_isp_direct.empty())   { m_tooltip += L"  "; m_tooltip += m_isp_direct; }
        m_tooltip += L"\n";
    }
    if (!m_public_ip_proxy.empty())
    {
        m_tooltip += L"代: ";
        m_tooltip += m_public_ip_proxy;
        if (!m_place_proxy.empty()) { m_tooltip += L"  ["; m_tooltip += m_place_proxy; m_tooltip += L"]"; }
        if (!m_isp_proxy.empty())   { m_tooltip += L"  "; m_tooltip += m_isp_proxy; }
        m_tooltip += L"\n";
    }

    // 若未启用代理，给出提示
    if (!m_setting_data.use_proxy)
    {
        m_tooltip += L"（未使用代理）\n";
    }

    // 延迟：按“直: 名称 - xxms | ...”和“代: 名称 - xxms | ...”，每行固定数量避免超长
    if (!m_latency_results.empty())
    {
        std::vector<std::pair<std::wstring,int>> ds, ps;
        for (const auto& it : m_latency_results)
        {
            if (it.direct_ms >= 0) ds.emplace_back(it.name.empty()?L"目标":it.name, (int)(it.direct_ms+0.5));
            if (it.proxy_ms  >= 0) ps.emplace_back(it.name.empty()?L"目标":it.name, (int)(it.proxy_ms +0.5));
        }
        m_tooltip += L"延迟:\n";
        AppendWrappedList(m_tooltip, ds, L"直: ", 3); // 每行最多3项
        if (m_setting_data.use_proxy)
            AppendWrappedList(m_tooltip, ps, L"代: ", 3);
        else
            m_tooltip += L"代: -\n";
    }

    m_tooltip += L"最后更新时间: "; m_tooltip += tb;
}

void CDataManager::UpdateIpInfoNow()
{
    std::wstring place, ip, isp;
    bool any_ok = false;

    // 使用短超时进行请求，防止UI卡顿已在 HttpGet 设置

    // 直连（绕过系统代理）
    if (FetchIpInfoByUrl(m_setting_data.api_domestic_url, false, place, ip, isp))
    {
        m_public_ip_direct = ip; m_place_direct = place; m_isp_direct = isp; any_ok = true;
    }
    else { m_public_ip_direct.clear(); m_place_direct.clear(); m_isp_direct.clear(); }

    // 代理
    if (m_setting_data.use_proxy)
    {
        if (FetchIpInfoByUrl(m_setting_data.api_foreign_url, true, place, ip, isp))
        {
            m_public_ip_proxy = ip; m_place_proxy = place; m_isp_proxy = isp; any_ok = true;
        }
        else { m_public_ip_proxy.clear(); m_place_proxy.clear(); m_isp_proxy.clear(); }
    }
    else
    {
        // 未启用代理时，清理之前的代理结果，避免旧数据残留
        m_public_ip_proxy.clear(); m_place_proxy.clear(); m_isp_proxy.clear();
    }

    // 测试延迟（若用户未配置，则给出一个默认的国内目标，避免悬浮窗没有延迟行）
    m_latency_results.clear();
    std::vector<LatencyTarget> targets = m_setting_data.latency_targets;
    if (targets.empty()) targets.push_back({ L"中国", L"https://www.baidu.com" });
    for (const auto& tgt : targets)
    {
        LatencyResult r; r.name = tgt.name.empty() ? tgt.url : tgt.name;
        MeasureLatencyMs(tgt.url, L"", true, r.direct_ms);
        if (m_setting_data.use_proxy)
            MeasureLatencyMs(tgt.url, m_setting_data.proxy_address, false, r.proxy_ms);
        m_latency_results.push_back(r);
    }

    // 任务栏显示文本构造
    m_ip_region_display.clear();
    if (!m_place_direct.empty()) { m_ip_region_display += L"直:"; m_ip_region_display += m_place_direct; }
    else if (!m_public_ip_direct.empty()) { m_ip_region_display += L"直:"; m_ip_region_display += m_public_ip_direct; }
    if (!m_place_proxy.empty()) { if (!m_ip_region_display.empty()) m_ip_region_display += L" | "; m_ip_region_display += L"代:"; m_ip_region_display += m_place_proxy; }
    else if (!m_public_ip_proxy.empty()) { if (!m_ip_region_display.empty()) m_ip_region_display += L" | "; m_ip_region_display += L"代:"; m_ip_region_display += m_public_ip_proxy; }

    m_last_ip_update_tick = GetTickCount64();
    UpdateStatusStrings(any_ok, true);
}

void CDataManager::UpdateIpInfoIfNeeded()
{
    m_ip_update_interval_ms = (unsigned int)m_setting_data.refresh_interval_sec * 1000u;
    ULONGLONG now = GetTickCount64();
    if (m_last_ip_update_tick != 0 && (now - m_last_ip_update_tick) < m_ip_update_interval_ms)
        return;

    UpdateIpInfoNow();
}

std::wstring CDataManager::GetLastUpdateStatusString() const
{
    const SYSTEMTIME& st = m_last_update_time; if (st.wYear == 0) return L"";
    wchar_t tb[64]{}; swprintf_s(tb, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    std::wstring s = L"最后更新时间: "; s += tb; return s;
}
