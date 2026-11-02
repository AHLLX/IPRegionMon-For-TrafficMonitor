#include "pch.h"
#include "DataManager.h"
#include <winhttp.h>
#include <stdarg.h>
#include <fstream>
#include "ModuleTask.h"
#include "BoundedHttp.h"

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

static std::wstring DecodeJsonString(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == L'\\' && i + 1 < s.size())
        {
            wchar_t ch = s[i + 1];
            if (ch == L'u' && i + 5 < s.size())
            {
                wchar_t val = 0;
                for (int j = 0; j < 4; ++j)
                {
                    wchar_t hc = s[i + 2 + j];
                    val <<= 4;
                    if (hc >= L'0' && hc <= L'9') val |= (hc - L'0');
                    else if (hc >= L'A' && hc <= L'F') val |= (hc - L'A' + 10);
                    else if (hc >= L'a' && hc <= L'f') val |= (hc - L'a' + 10);
                }
                out.push_back(val);
                i += 5;
            }
            else
            {
                if (ch == L'"') out.push_back(L'"');
                else if (ch == L'\\') out.push_back(L'\\');
                else if (ch == L'/') out.push_back(L'/');
                else if (ch == L'b') out.push_back(L'\b');
                else if (ch == L'f') out.push_back(L'\f');
                else if (ch == L'n') out.push_back(L'\n');
                else if (ch == L'r') out.push_back(L'\r');
                else if (ch == L't') out.push_back(L'\t');
                else out.push_back(ch);
                i += 1;
            }
        }
        else
        {
            out.push_back(s[i]);
        }
    }
    return out;
}

// 朴素JSON字段提取（支持转义字符与Unicode解码）
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
        size_t start = pos + 1;
        size_t end = start;
        while (end < json.size())
        {
            if (json[end] == L'"')
            {
                size_t slashes = 0;
                size_t p = end - 1;
                while (p >= start && json[p] == L'\\') { ++slashes; --p; }
                if (slashes % 2 == 0) break;
            }
            ++end;
        }
        if (end >= json.size()) return L"";
        return DecodeJsonString(json.substr(start, end - start));
    }
    size_t end = pos;
    while (end < json.size() && json[end] != L',' && json[end] != L'}' && json[end] != L']') ++end;
    std::wstring raw = json.substr(pos, end - pos);
    size_t trim_s = 0, trim_e = raw.size();
    while (trim_s < raw.size() && iswspace(raw[trim_s])) ++trim_s;
    while (trim_e > trim_s && iswspace(raw[trim_e - 1])) --trim_e;
    return DecodeJsonString(raw.substr(trim_s, trim_e - trim_s));
}

static bool GetJsonBoolValue(const std::wstring& json, const wchar_t* key)
{
    std::wstring val = GetJsonStringValue(json, key);
    return (val == L"true" || val == L"1");
}

static std::wstring GetJsonObjectValue(const std::wstring& json, const wchar_t* key)
{
    std::wstring keyQuoted = L"\"" + std::wstring(key) + L"\"";
    size_t kpos = json.find(keyQuoted);
    if (kpos == std::wstring::npos) return L"";

    size_t colon = json.find(L':', kpos + keyQuoted.size());
    if (colon == std::wstring::npos) return L"";

    size_t pos = colon + 1;
    while (pos < json.size() && iswspace(json[pos])) ++pos;
    if (pos >= json.size() || json[pos] != L'{') return L"";

    int depth = 1;
    size_t end = pos + 1;
    while (end < json.size() && depth > 0)
    {
        if      (json[end] == L'{') ++depth;
        else if (json[end] == L'}') --depth;
        ++end;
    }
    return json.substr(pos, end - pos);
}

static std::vector<std::wstring> GetJsonArrayStrings(const std::wstring& json, const wchar_t* key)
{
    std::vector<std::wstring> result;
    std::wstring keyQuoted = L"\"" + std::wstring(key) + L"\"";
    size_t kpos = json.find(keyQuoted);
    if (kpos == std::wstring::npos) return result;

    size_t colon = json.find(L':', kpos + keyQuoted.size());
    if (colon == std::wstring::npos) return result;

    size_t pos = colon + 1;
    while (pos < json.size() && iswspace(json[pos])) ++pos;
    if (pos >= json.size() || json[pos] != L'[') return result;

    size_t end = json.find(L']', pos + 1);
    if (end == std::wstring::npos) return result;

    size_t cur = pos + 1;
    while (cur < end)
    {
        while (cur < end && json[cur] != L'"') ++cur;
        if (cur >= end) break;
        size_t strStart = cur + 1;
        size_t strEnd = strStart;
        while (strEnd < end)
        {
            if (json[strEnd] == L'"')
            {
                size_t slashes = 0;
                size_t p = strEnd - 1;
                while (p >= strStart && json[p] == L'\\') { ++slashes; --p; }
                if (slashes % 2 == 0) break;
            }
            ++strEnd;
        }
        if (strEnd <= end)
        {
            result.push_back(DecodeJsonString(json.substr(strStart, strEnd - strStart)));
            cur = strEnd + 1;
        }
        else break;
    }
    return result;
}


// 通过 WinHTTP 进行 GET 请求
static bool HttpGet(const std::wstring& url, const std::wstring& proxy, bool no_proxy, std::string& out)
{
    double unused;
    return BoundedHttp::Perform(url, proxy, no_proxy, false, out, unused);
}

bool CDataManager::MeasureLatencyMs(const std::wstring& url_or_host, const std::wstring& proxy, bool no_proxy, double& out_ms)
{
    std::wstring url = url_or_host;
    if (url.find(L"://") == std::wstring::npos) url = L"https://" + url;
    std::string unused;
    return BoundedHttp::Perform(url, proxy, no_proxy, true, unused, out_ms);
}

#ifdef IPREGIONMON_TESTING
extern "C" __declspec(dllexport) BOOL TestNetworkRequest(const wchar_t* url, BOOL head,
    DWORD* error, DWORD* bytes)
{
    std::string body;
    double latency;
    const bool ok = head ? CDataManager::MeasureLatencyMs(url, L"", true, latency) :
        HttpGet(url, L"", true, body);
    *error = ok ? ERROR_SUCCESS : GetLastError();
    *bytes = static_cast<DWORD>(body.size());
    return ok;
}

extern "C" __declspec(dllexport) BOOL TestNetworkStart(const wchar_t* url, HANDLE completed)
{
    return ModuleTask::Start([url = std::wstring(url), completed]() {
        std::string body;
        HttpGet(url, L"", true, body);
        SetEvent(completed);
    });
}

extern "C" __declspec(dllexport) void TestNetworkHoldCallback(HANDLE entered, HANDLE release)
{
    BoundedHttp::CallbackTest().release = release;
    BoundedHttp::CallbackTest().entered = entered;
}
#endif

bool CDataManager::TestProxyConnectivity(const std::wstring& proxy, std::wstring& out_message, double& out_ms)
{
    out_ms = -1.0;
    if (proxy.empty())
    {
        out_message = L"代理地址为空！";
        return false;
    }

    // 探测 www.cloudflare.com
    bool ok = MeasureLatencyMs(L"https://www.cloudflare.com", proxy, false, out_ms);
    if (!ok || out_ms < 0)
    {
        ok = MeasureLatencyMs(L"https://www.google.com", proxy, false, out_ms);
    }

    if (ok && out_ms >= 0)
    {
        wchar_t buf[64]{};
        swprintf_s(buf, L"连接成功 (延迟: %.0f ms)", out_ms);
        out_message = buf;
        return true;
    }
    else
    {
        out_message = L"连接失败：无法通过该代理访问目标网络";
        return false;
    }
}

CDataManager::CDataManager()
{
    const wchar_t* kDomesticApi = L"https://ipwho.is/";
    const wchar_t* kForeignApi  = L"https://ipwho.is/";
    m_setting_data.api_domestic_url = kDomesticApi;
    m_setting_data.api_foreign_url  = kForeignApi;
    m_setting_data.use_proxy = false;
    m_setting_data.refresh_interval_sec = 300;
    m_setting_data.show_ip = true;
    m_setting_data.display_format = DF_REGION_AND_IP;

    // 默认测试目标
    m_setting_data.latency_targets.push_back({ L"百度", L"https://www.baidu.com" });
    m_setting_data.latency_targets.push_back({ L"Google", L"https://www.google.com" });
    m_setting_data.latency_targets.push_back({ L"Cloudflare", L"https://1.1.1.1" });
}

CDataManager& CDataManager::Instance()
{
    return m_instance;
}

void CDataManager::LoadConfig(const std::wstring& config_dir)
{
    std::lock_guard<std::mutex> io_lock(m_config_mutex);
    SettingData settings = GetSettings();
    std::wstring config_path;
    HMODULE hModule = reinterpret_cast<HMODULE>(&__ImageBase);
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(hModule, path, MAX_PATH);
    std::wstring module_path = path;
    config_path = module_path;
    if (!config_dir.empty())
    {
        size_t index = module_path.find_last_of(L"\\/");
        std::wstring module_file_name = module_path.substr(index + 1);
        config_path = config_dir + module_file_name;
    }
    config_path += L".ini";

    settings.show_second = GetPrivateProfileIntW(L"config", L"show_second", 0, config_path.c_str());
    settings.use_proxy = GetPrivateProfileIntW(L"ip", L"use_proxy", 0, config_path.c_str()) != 0;

    wchar_t buff[1024]{};
    GetPrivateProfileStringW(L"ip", L"proxy_address", L"127.0.0.1:7890", buff, _countof(buff), config_path.c_str());
    settings.proxy_address = buff;

    settings.refresh_interval_sec = GetPrivateProfileIntW(L"ip", L"refresh_interval_sec", 300, config_path.c_str());
    settings.show_ip = GetPrivateProfileIntW(L"ip", L"show_ip", 1, config_path.c_str()) != 0;

    // 读取显示格式（若无则根据旧版 show_ip 推断）
    int def_format = settings.show_ip ? DF_REGION_AND_IP : DF_REGION_ONLY;
    settings.display_format = GetPrivateProfileIntW(L"ip", L"display_format", def_format, config_path.c_str());

    GetPrivateProfileStringW(L"ip", L"api_url", L"", buff, _countof(buff), config_path.c_str());
    settings.api_url = buff;

    GetPrivateProfileStringW(L"ip", L"api_domestic_url", L"", buff, _countof(buff), config_path.c_str());
    std::wstring domestic = buff;
    GetPrivateProfileStringW(L"ip", L"api_foreign_url", L"", buff, _countof(buff), config_path.c_str());
    std::wstring foreign = buff;

    // 读取延迟目标列表
    settings.latency_targets.clear();
    int n = GetPrivateProfileIntW(L"ip", L"latency_targets_n", 0, config_path.c_str());
    if (n > 0)
    {
        for (int i = 0; i < n; ++i)
        {
            wchar_t key_name[64]{}; swprintf_s(key_name, L"latency_name_%d", i);
            wchar_t key_url[64]{};  swprintf_s(key_url,  L"latency_url_%d", i);
            GetPrivateProfileStringW(L"ip", key_name, L"", buff, _countof(buff), config_path.c_str());
            std::wstring name = Utf8HexDecode(buff);
            GetPrivateProfileStringW(L"ip", key_url, L"", buff, _countof(buff), config_path.c_str());
            std::wstring url = Utf8HexDecode(buff);
            if (!url.empty())
                settings.latency_targets.push_back({ name, url });
        }
    }
    else
    {
        // 兼容单项 latency_target 或默认
        GetPrivateProfileStringW(L"ip", L"latency_target", L"", buff, _countof(buff), config_path.c_str());
        std::wstring single_latency = Utf8HexDecode(buff);
        if (!single_latency.empty())
        {
            settings.latency_targets.push_back({ L"默认", single_latency });
        }
        else
        {
            settings.latency_targets.push_back({ L"百度", L"https://www.baidu.com" });
            settings.latency_targets.push_back({ L"Google", L"https://www.google.com" });
            settings.latency_targets.push_back({ L"Cloudflare", L"https://1.1.1.1" });
        }
    }

    if (!domestic.empty()) settings.api_domestic_url = domestic; else if (!settings.api_url.empty()) settings.api_domestic_url = settings.api_url;
    if (!foreign.empty())  settings.api_foreign_url  = foreign;  else if (!settings.api_url.empty()) settings.api_foreign_url  = settings.api_url;

    const wchar_t* kDomesticApi = L"https://ipwho.is/";
    const wchar_t* kForeignApi  = L"https://ipwho.is/";
    if (settings.api_domestic_url.empty() || settings.api_domestic_url.find(L"ipip.net") != std::wstring::npos)
        settings.api_domestic_url = kDomesticApi;
    if (settings.api_foreign_url.empty())  settings.api_foreign_url  = kForeignApi;

    if (settings.refresh_interval_sec < 5) settings.refresh_interval_sec = 5;
    {
        std::lock_guard<std::mutex> lock(m_swap_mutex);
        m_setting_data = std::move(settings);
        m_config_path = std::move(config_path);
    }
}

static void WritePrivateProfileInt(const wchar_t* app, const wchar_t* key, int v, const wchar_t* file)
{
    wchar_t b[32]; swprintf_s(b, L"%d", v); WritePrivateProfileStringW(app, key, b, file);
}

void CDataManager::SaveConfig() const
{
    std::lock_guard<std::mutex> io_lock(m_config_mutex);
    SettingData settings;
    std::wstring config_path;
    {
        std::lock_guard<std::mutex> lock(m_swap_mutex);
        settings = m_setting_data;
        config_path = m_config_path;
    }
    if (config_path.empty()) return;
    WritePrivateProfileInt(L"config", L"show_second", settings.show_second, config_path.c_str());
    WritePrivateProfileInt(L"ip", L"use_proxy", settings.use_proxy ? 1 : 0, config_path.c_str());
    WritePrivateProfileStringW(L"ip", L"proxy_address", settings.proxy_address.c_str(), config_path.c_str());
    WritePrivateProfileStringW(L"ip", L"api_domestic_url", settings.api_domestic_url.c_str(), config_path.c_str());
    WritePrivateProfileStringW(L"ip", L"api_foreign_url", settings.api_foreign_url.c_str(), config_path.c_str());
    WritePrivateProfileInt(L"ip", L"refresh_interval_sec", settings.refresh_interval_sec, config_path.c_str());
    WritePrivateProfileInt(L"ip", L"show_ip", settings.show_ip ? 1 : 0, config_path.c_str());
    WritePrivateProfileInt(L"ip", L"display_format", settings.display_format, config_path.c_str());

    WritePrivateProfileStringW(L"ip", L"latency_target", NULL, config_path.c_str());
    WritePrivateProfileInt(L"ip", L"latency_targets_n", (int)settings.latency_targets.size(), config_path.c_str());
    for (int i = 0; i < (int)settings.latency_targets.size(); ++i)
    {
        wchar_t key_name[64]{}; swprintf_s(key_name, L"latency_name_%d", i);
        wchar_t key_url[64]{};  swprintf_s(key_url,  L"latency_url_%d", i);
        std::wstring encName = Utf8HexEncode(settings.latency_targets[i].name);
        std::wstring encUrl  = Utf8HexEncode(settings.latency_targets[i].url);
        WritePrivateProfileStringW(L"ip", key_name, encName.c_str(), config_path.c_str());
        WritePrivateProfileStringW(L"ip", key_url,  encUrl.c_str(),  config_path.c_str());
    }
}

const wchar_t* CDataManager::StringRes(UINT id)
{
    std::lock_guard<std::mutex> lock(m_swap_mutex);
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

bool CDataManager::FetchIpInfoByUrl(const std::wstring& url, bool use_proxy, const std::wstring& proxy_address, std::wstring& out_place, std::wstring& out_ip, std::wstring& out_isp, IpThreatInfo& out_threat)
{
    std::string resp;
    std::wstring proxy = use_proxy ? proxy_address : L"";
    bool no_proxy = !use_proxy;
    if (!HttpGet(url, proxy, no_proxy, resp))
    {
        out_place = L"请求失败（连接超时或网络未达）";
        out_ip.clear();
        out_isp.clear();
        out_threat = IpThreatInfo{};
        return false;
    }

    std::wstring w = Utf8ToWide(resp);
    std::wstring status = GetJsonStringValue(w, L"status");
    std::wstring message = GetJsonStringValue(w, L"message");
    std::wstring ip = GetJsonStringValue(w, L"query");
    if (ip.empty()) ip = GetJsonStringValue(w, L"ip");
    if (ip.empty()) ip = GetJsonStringValue(w, L"ip_address");
    if (ip.empty()) ip = GetJsonStringValue(w, L"origin");

    std::wstring country, region, city, isp;
    {
        std::wstring locObj = GetJsonObjectValue(w, L"location");
        if (!locObj.empty())
        {
            country = GetJsonStringValue(locObj, L"country");
            region  = GetJsonStringValue(locObj, L"regionName");
            if (region.empty()) region = GetJsonStringValue(locObj, L"region");
            if (region.empty()) region = GetJsonStringValue(locObj, L"state");
            city    = GetJsonStringValue(locObj, L"city");
        }
        else
        {
            std::vector<std::wstring> locArray = GetJsonArrayStrings(w, L"location");
            if (!locArray.empty())
            {
                if (locArray.size() > 0 && !locArray[0].empty()) country = locArray[0];
                if (locArray.size() > 1 && !locArray[1].empty()) region  = locArray[1];
                if (locArray.size() > 2 && !locArray[2].empty()) city    = locArray[2];
                if (locArray.size() > 4 && !locArray[4].empty())
                {
                    std::wstring rawIsp = locArray[4];
                    if (rawIsp.find(L"telecom") != std::wstring::npos) isp = L"中国电信";
                    else if (rawIsp.find(L"unicom") != std::wstring::npos) isp = L"中国联通";
                    else if (rawIsp.find(L"mobile") != std::wstring::npos) isp = L"中国移动";
                    else if (rawIsp.find(L"cernet") != std::wstring::npos) isp = L"教育网";
                    else if (rawIsp.find(L"cstnet") != std::wstring::npos) isp = L"科技网";
                    else isp = rawIsp;
                }
            }
        }
    }
    if (country.empty()) country = GetJsonStringValue(w, L"country");
    if (country.empty()) country = GetJsonStringValue(w, L"country_name");
    if (country.empty()) country = GetJsonStringValue(w, L"c");
    if (region.empty())  region  = GetJsonStringValue(w, L"regionName");
    if (region.empty())  region  = GetJsonStringValue(w, L"region");
    if (region.empty())  region  = GetJsonStringValue(w, L"province");
    if (region.empty())  region  = GetJsonStringValue(w, L"state");
    if (region.empty())  region  = GetJsonStringValue(w, L"pro");
    if (city.empty())    city    = GetJsonStringValue(w, L"city");

    if (isp.empty()) isp = GetJsonStringValue(w, L"isp");
    if (isp.empty()) isp = GetJsonStringValue(w, L"organization");
    if (isp.empty()) isp = GetJsonStringValue(w, L"name");
    if (isp.empty()) isp = GetJsonStringValue(w, L"org");
    if (isp.empty()) isp = GetJsonStringValue(w, L"operator");
    if (isp.empty())
    {
        std::wstring connObj = GetJsonObjectValue(w, L"connection");
        if (!connObj.empty())
        {
            isp = GetJsonStringValue(connObj, L"isp");
            if (isp.empty()) isp = GetJsonStringValue(connObj, L"org");
        }
    }

    out_threat = IpThreatInfo{};
    out_threat.is_proxy      = GetJsonBoolValue(w, L"is_proxy")      || GetJsonBoolValue(w, L"proxy");
    out_threat.is_vpn        = GetJsonBoolValue(w, L"is_vpn")        || GetJsonBoolValue(w, L"vpn");
    out_threat.is_datacenter = GetJsonBoolValue(w, L"is_datacenter") || GetJsonBoolValue(w, L"hosting");
    out_threat.is_tor        = GetJsonBoolValue(w, L"is_tor")        || GetJsonBoolValue(w, L"tor");
    out_threat.is_mobile     = GetJsonBoolValue(w, L"is_mobile")     || GetJsonBoolValue(w, L"mobile");
    out_threat.is_abuser     = GetJsonBoolValue(w, L"is_abuser")     || GetJsonBoolValue(w, L"abuser");

    {
        std::wstring asnObj = GetJsonObjectValue(w, L"asn");
        if (!asnObj.empty())
        {
            std::wstring asnType = GetJsonStringValue(asnObj, L"type");
            std::wstring t_lc   = asnType;
            for (auto& c : t_lc) c = towlower(c);

            if (t_lc == L"hosting" || t_lc == L"datacenter" || t_lc == L"cdn")
                out_threat.is_datacenter = true;
            else if (t_lc == L"mobile" || t_lc == L"wireless")
                out_threat.is_mobile = true;
        }
    }

    out_threat.risk_score = GetJsonStringValue(w, L"abuser_score");
    if (out_threat.risk_score.empty()) out_threat.risk_score = GetJsonStringValue(w, L"fraud_score");
    if (out_threat.risk_score.empty()) out_threat.risk_score = GetJsonStringValue(w, L"risk");

    if (status == L"fail" || status == L"error")
    {
        out_ip.clear(); out_place = message.empty() ? L"接口返回错误" : message; out_isp.clear();
        return false;
    }

    if (ip.empty())
    {
        out_place = L"解析失败（无效的数据格式）";
        out_ip.clear();
        out_isp.clear();
        return false;
    }

    out_ip = ip; out_isp = isp;
    std::wstring place;
    if (!country.empty()) place += country;
    if (!region.empty() && region != country)  { if (!place.empty()) place += L" "; place += region; }
    if (!city.empty() && city != region && city != country) { if (!place.empty()) place += L" "; place += city; }
    if (place.empty()) place = GetJsonStringValue(w, L"addr");
    if (place.empty()) place = GetJsonStringValue(w, L"address");
    out_place = place;
    return true;
}

bool CDataManager::FetchIpInfoWithFallback(const std::wstring& preferredUrl, bool use_proxy, const std::wstring& proxy_address, std::wstring& out_place, std::wstring& out_ip, std::wstring& out_isp, IpThreatInfo& out_threat)
{
    std::vector<std::wstring> sources;
    if (!preferredUrl.empty())
    {
        sources.push_back(preferredUrl);
    }

    // 默认高可用源候选池
    const wchar_t* fallbackPool[] = {
        L"https://ipwho.is/",
        L"https://api.ip.sb/geoip",
        L"https://ipinfo.io/json"
    };

    for (const auto* p : fallbackPool)
    {
        bool exists = false;
        for (const auto& s : sources)
        {
            if (s == p) { exists = true; break; }
        }
        if (!exists) sources.push_back(p);
    }

    for (size_t i = 0; i < sources.size(); ++i)
    {
        const auto& url = sources[i];
        std::wstring l_place, l_ip, l_isp;
        IpThreatInfo l_threat;
        if (FetchIpInfoByUrl(url, use_proxy, proxy_address, l_place, l_ip, l_isp, l_threat))
        {
            if (!l_ip.empty())
            {
                out_place = l_place;
                out_ip = l_ip;
                out_isp = l_isp;
                out_threat = l_threat;
                LogInfo(L"FetchIpInfo: [%ls] Succeeded via source #%d (%ls): IP=%ls, Place=%ls",
                    use_proxy ? L"Proxy" : L"Direct", (int)i + 1, url.c_str(), out_ip.c_str(), out_place.c_str());
                return true;
            }
        }
        else
        {
            LogInfo(L"FetchIpInfo: [%ls] Source #%d failed (%ls)", use_proxy ? L"Proxy" : L"Direct", (int)i + 1, url.c_str());
        }
    }

    LogInfo(L"FetchIpInfo: [%ls] All %d sources failed.", use_proxy ? L"Proxy" : L"Direct", (int)sources.size());
    out_place = L"请求失败（连接超时或网络未达）";
    out_ip.clear();
    out_isp.clear();
    out_threat = IpThreatInfo{};
    return false;
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

static std::wstring FormatItemTextHelper(bool is_proxy, const std::wstring& place, const std::wstring& ip, const std::wstring& isp, const std::vector<CDataManager::LatencyResult>& lats, int fmt);
void CDataManager::UpdateIpInfoNow()
{
#ifdef IPREGIONMON_TESTING
    if (m_fail_update_for_test.exchange(false)) throw std::bad_alloc();
#endif
    const auto snapshot = GetSnapshot();
    const auto& settings = snapshot.settings;
    const auto& previous = snapshot.results;
    ULONGLONG start_tick = GetTickCount64();
    LogInfo(L"UpdateIpInfoNow: Background update thread started.");

    std::wstring l_public_ip_direct, l_place_direct, l_isp_direct;
    IpThreatInfo l_threat_direct;
    std::wstring l_public_ip_proxy, l_place_proxy, l_isp_proxy;
    IpThreatInfo l_threat_proxy;

    bool ok_direct = false;
    bool ok_proxy = false;
    std::vector<LatencyResult> l_latency;

    std::vector<LatencyTarget> targets = settings.latency_targets;
    if (targets.empty())
    {
        targets.push_back({ L"百度", L"https://www.baidu.com" });
        targets.push_back({ L"Google", L"https://www.google.com" });
    }

    bool use_proxy = settings.use_proxy;
    std::wstring proxy_addr = settings.proxy_address;

    // 1. 直连探测（高可用多源轮询）
    LogInfo(L"UpdateIpInfoNow: Fetching direct IP...");
    ok_direct = FetchIpInfoWithFallback(settings.api_domestic_url, false, proxy_addr, l_place_direct, l_public_ip_direct, l_isp_direct, l_threat_direct);
    LogInfo(L"UpdateIpInfoNow: Direct query done (ok=%d).", ok_direct ? 1 : 0);

    // 2. 代理探测（高可用多源轮询）
    if (use_proxy)
    {
        LogInfo(L"UpdateIpInfoNow: Fetching proxy IP...");
        ok_proxy = FetchIpInfoWithFallback(settings.api_foreign_url, true, proxy_addr, l_place_proxy, l_public_ip_proxy, l_isp_proxy, l_threat_proxy);
        LogInfo(L"UpdateIpInfoNow: Proxy query done (ok=%d).", ok_proxy ? 1 : 0);
    }

    // 3. 延迟测速
    LogInfo(L"UpdateIpInfoNow: Testing latencies for %d targets...", (int)targets.size());
    for (const auto& tgt : targets)
    {
        LogInfo(L"UpdateIpInfoNow: Testing target %ls...", tgt.name.c_str());
        LatencyResult r;
        r.name = tgt.name.empty() ? tgt.url : tgt.name;
        r.url  = tgt.url;
        try {
            MeasureLatencyMs(tgt.url, L"", true, r.direct_ms);
        } catch (...) {
            r.direct_ms = -1;
        }
        if (use_proxy)
        {
            try {
                MeasureLatencyMs(tgt.url, proxy_addr, false, r.proxy_ms);
            } catch (...) {
                r.proxy_ms = -1;
            }
        }
        l_latency.push_back(r);
    }
    LogInfo(L"UpdateIpInfoNow: Latency tests done.");

    // 平滑缓存保留：若偶发单次刷新失败且此前有成功记录，保留旧值，防止网络波动导致任务栏频繁红字闪烁
    if (ok_direct)
    {
        m_direct_fail_count = 0;
    }
    else
    {
        m_direct_fail_count++;
        if (!previous.m_public_ip_direct.empty() && m_direct_fail_count <= 3)
        {
            LogInfo(L"UpdateIpInfoNow: Direct query failed (%d/3), retaining previous IP: %ls", m_direct_fail_count, previous.m_public_ip_direct.c_str());
            l_public_ip_direct = previous.m_public_ip_direct;
            l_place_direct     = previous.m_place_direct;
            l_isp_direct       = previous.m_isp_direct;
            l_threat_direct    = previous.m_threat_direct;
            ok_direct = true;
        }
        else
        {
            l_place_direct = L"请求失败（连接超时或网络未达）";
            l_public_ip_direct.clear();
            l_isp_direct.clear();
            l_threat_direct = IpThreatInfo{};
        }
    }

    if (use_proxy)
    {
        if (ok_proxy)
        {
            m_proxy_fail_count = 0;
        }
        else
        {
            m_proxy_fail_count++;
            if (!previous.m_public_ip_proxy.empty() && m_proxy_fail_count <= 3)
            {
                LogInfo(L"UpdateIpInfoNow: Proxy query failed (%d/3), retaining previous IP: %ls", m_proxy_fail_count, previous.m_public_ip_proxy.c_str());
                l_public_ip_proxy = previous.m_public_ip_proxy;
                l_place_proxy     = previous.m_place_proxy;
                l_isp_proxy       = previous.m_isp_proxy;
                l_threat_proxy    = previous.m_threat_proxy;
                ok_proxy = true;
            }
            else
            {
                l_place_proxy = L"请求失败（连接超时或网络未达）";
                l_public_ip_proxy.clear();
                l_isp_proxy.clear();
                l_threat_proxy = IpThreatInfo{};
            }
        }
    }

    bool any_ok = ok_direct || ok_proxy;
    // 任务栏汇总文本
    std::wstring l_ip_region_display;
    if (!l_place_direct.empty()) { l_ip_region_display += L"直:"; l_ip_region_display += l_place_direct; }
    else if (!l_public_ip_direct.empty()) { l_ip_region_display += L"直:"; l_ip_region_display += l_public_ip_direct; }
    if (!l_place_proxy.empty()) { if (!l_ip_region_display.empty()) l_ip_region_display += L" | "; l_ip_region_display += L"代:"; l_ip_region_display += l_place_proxy; }
    else if (!l_public_ip_proxy.empty()) { if (!l_ip_region_display.empty()) l_ip_region_display += L" | "; l_ip_region_display += L"代:"; l_ip_region_display += l_public_ip_proxy; }

    // 构造 Tooltip
    auto FormatThreatStr = [](const IpThreatInfo& t) -> std::wstring {
        std::wstring type;
        if (t.is_datacenter) { if (!type.empty()) type += L" + "; type += L"IDC机房"; }
        if (t.is_mobile)     { if (!type.empty()) type += L" + "; type += L"移动网络"; }
        if (t.is_vpn)        { if (!type.empty()) type += L" + "; type += L"VPN"; }
        if (t.is_proxy)      { if (!type.empty()) type += L" + "; type += L"代理"; }
        if (t.is_tor)        { if (!type.empty()) type += L" + "; type += L"Tor"; }
        if (type.empty()) type = L"普通宽带";

        std::wstring status = t.is_abuser ? L"恶意/黑名单" : L"纯净";
        if (!t.risk_score.empty()) { status += L" (欺诈分: " + t.risk_score + L")"; }
        return L"    类型: " + type + L" | 状态: " + status + L"\n";
    };

    std::wstring l_tooltip;
    if (any_ok)
    {
        SYSTEMTIME st{}; GetLocalTime(&st);
        wchar_t tb[64]{}; swprintf_s(tb, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        
        if (!l_public_ip_direct.empty())
        {
            l_tooltip += L"直: " + l_public_ip_direct;
            if (!l_place_direct.empty()) l_tooltip += L"  [" + l_place_direct + L"]";
            if (!l_isp_direct.empty()) l_tooltip += L"  " + l_isp_direct;
            l_tooltip += L"\n" + FormatThreatStr(l_threat_direct);
        }
        if (!l_public_ip_proxy.empty())
        {
            l_tooltip += L"代: " + l_public_ip_proxy;
            if (!l_place_proxy.empty()) l_tooltip += L"  [" + l_place_proxy + L"]";
            if (!l_isp_proxy.empty()) l_tooltip += L"  " + l_isp_proxy;
            l_tooltip += L"\n" + FormatThreatStr(l_threat_proxy);
        }
        if (!settings.use_proxy) l_tooltip += L"（未启用代理）\n";

        if (!l_latency.empty())
        {
            std::vector<std::pair<std::wstring,int>> ds, ps;
            for (const auto& it : l_latency)
            {
                if (it.direct_ms >= 0) ds.emplace_back(it.name, (int)(it.direct_ms + 0.5));
                if (it.proxy_ms  >= 0) ps.emplace_back(it.name, (int)(it.proxy_ms  + 0.5));
            }
            l_tooltip += L"延迟:\n";
            AppendWrappedList(l_tooltip, ds, L"直: ", 3);
            if (settings.use_proxy) AppendWrappedList(l_tooltip, ps, L"代: ", 3);
            else l_tooltip += L"代: -\n";
        }
        l_tooltip += L"最后更新: "; l_tooltip += tb;
    }

    m_last_ip_update_tick = GetTickCount64();

    ResultSnapshot next;
    next.m_public_ip_direct = std::move(l_public_ip_direct);
    next.m_place_direct = std::move(l_place_direct);
    next.m_isp_direct = std::move(l_isp_direct);
    next.m_threat_direct = std::move(l_threat_direct);
    next.m_public_ip_proxy = std::move(l_public_ip_proxy);
    next.m_place_proxy = std::move(l_place_proxy);
    next.m_isp_proxy = std::move(l_isp_proxy);
    next.m_threat_proxy = std::move(l_threat_proxy);
    next.m_latency_results = std::move(l_latency);
    next.m_ip_region_display = std::move(l_ip_region_display);
    next.m_tooltip = std::move(l_tooltip);
    {
        std::lock_guard<std::mutex> lock(m_swap_mutex);
        const int format = m_setting_data.display_format;
        next.m_direct_item_text = FormatItemTextHelper(false, next.m_place_direct,
            next.m_public_ip_direct, next.m_isp_direct, next.m_latency_results, format);
        next.m_proxy_item_text = FormatItemTextHelper(true, next.m_place_proxy,
            next.m_public_ip_proxy, next.m_isp_proxy, next.m_latency_results, format);
        m_pending = std::move(next);
        m_has_new_data = true;
    }
    LogInfo(L"UpdateIpInfoNow: Background update completed in %.2f s.", (GetTickCount64() - start_tick) / 1000.0);

}

static std::wstring FormatItemTextHelper(bool is_proxy, const std::wstring& place, const std::wstring& ip, const std::wstring& isp, const std::vector<CDataManager::LatencyResult>& lats, int fmt)
{
    switch (fmt)
    {
    case DF_REGION_ONLY:
        if (!place.empty()) return place;
        return ip.empty() ? L"..." : ip;

    case DF_IP_ONLY:
        if (!ip.empty()) return ip;
        return place.empty() ? L"..." : place;

    case DF_REGION_AND_LATENCY:
    {
        std::wstring out = place.empty() ? ip : place;
        for (const auto& r : lats)
        {
            double ms = is_proxy ? r.proxy_ms : r.direct_ms;
            if (ms >= 0)
            {
                wchar_t b[32]{}; swprintf_s(b, L" %dms", (int)(ms + 0.5));
                out += b;
                break;
            }
        }
        return out.empty() ? L"..." : out;
    }

    case DF_FULL:
    {
        std::wstring out = place;
        if (!isp.empty()) { if (!out.empty()) out += L" "; out += isp; }
        if (!ip.empty())  { out += L" (" + ip + L")"; }
        return out.empty() ? L"..." : out;
    }

    case DF_REGION_AND_IP:
    default:
    {
        if (!place.empty() && !ip.empty())
            return place + L" (" + ip + L")";
        if (!place.empty()) return place;
        if (!ip.empty())    return ip;
        return L"...";
    }
    }
}

SettingData CDataManager::GetSettings() const
{
    std::lock_guard<std::mutex> lock(m_swap_mutex);
    return m_setting_data;
}

void CDataManager::SetSettings(SettingData settings)
{
    std::lock_guard<std::mutex> lock(m_swap_mutex);
    m_setting_data = std::move(settings);
}

ResultSnapshot CDataManager::GetResults() const
{
    std::lock_guard<std::mutex> lock(m_swap_mutex);
    return m_results;
}

CDataManager::Snapshot CDataManager::GetSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_swap_mutex);
    return { m_setting_data, m_results };
}

std::wstring CDataManager::FormatItemText(bool is_proxy) const
{
    std::lock_guard<std::mutex> lock(m_swap_mutex);
    return is_proxy ? m_results.m_proxy_item_text : m_results.m_direct_item_text;
}

const wchar_t* CDataManager::GetFormattedItemText(bool is_proxy) const
{
    std::lock_guard<std::mutex> lock(m_swap_mutex);
    auto& reader = m_reader_texts[GetCurrentThreadId()];
    auto& text = is_proxy ? reader.proxy : reader.direct;
    const auto& value = is_proxy ? m_results.m_proxy_item_text : m_results.m_direct_item_text;
    text = value.empty() ? L"..." : value;
    return text.c_str();
}

const wchar_t* CDataManager::GetTooltipText() const
{
    std::lock_guard<std::mutex> lock(m_swap_mutex);
    auto& text = m_reader_texts[GetCurrentThreadId()].tooltip;
    text = m_results.m_tooltip;
    return text.c_str();
}

bool CDataManager::IsUpdating() const { return m_is_updating.load(); }

void CDataManager::ReformatTaskbarTexts()
{
    std::lock_guard<std::mutex> lock(m_swap_mutex);
    auto format = [this](ResultSnapshot& result) {
        result.m_direct_item_text = FormatItemTextHelper(false, result.m_place_direct,
            result.m_public_ip_direct, result.m_isp_direct, result.m_latency_results, m_setting_data.display_format);
        result.m_proxy_item_text = FormatItemTextHelper(true, result.m_place_proxy,
            result.m_public_ip_proxy, result.m_isp_proxy, result.m_latency_results, m_setting_data.display_format);
    };
    format(m_results);
    if (m_has_new_data) format(m_pending);
}

int CDataManager::GetRefreshCountdownSec() const
{
    if (m_is_updating) return 0;
    const ULONGLONG last = m_last_ip_update_tick.load();
    if (!last) return 0;
    int seconds;
    {
        std::lock_guard<std::mutex> lock(m_swap_mutex);
        seconds = m_setting_data.refresh_interval_sec;
    }
    const ULONGLONG now = GetTickCount64();
    const ULONGLONG interval = static_cast<ULONGLONG>(seconds) * 1000ULL;
    return now >= last + interval ? 0 : static_cast<int>((last + interval - now + 999) / 1000);
}

void CDataManager::UpdateIpInfoIfNeeded()
{
    int seconds;
    {
        std::lock_guard<std::mutex> lock(m_swap_mutex);
        seconds = m_setting_data.refresh_interval_sec;
    }
    const ULONGLONG last = m_last_ip_update_tick.load();
    if (last && GetTickCount64() - last < static_cast<ULONGLONG>(seconds) * 1000ULL)
        return;
    ForceUpdateAsync();
}

void CDataManager::ForceUpdateAsync()
{
    bool expected = false;
    if (!m_is_updating.compare_exchange_strong(expected, true))
        return;
    try
    {
        if (ModuleTask::Start([this]() {
            struct Reset {
                std::atomic<bool>& flag;
                ~Reset() { flag.store(false); }
            } reset{ m_is_updating };
            try { UpdateIpInfoNow(); }
            catch (...) {
                try { LogInfo(L"ForceUpdateAsync: Unhandled exception caught in update thread."); }
                catch (...) {}
            }
        }))
            return;
    }
    catch (...) {}
    m_is_updating.store(false);
}

std::wstring CDataManager::GetLastUpdateStatusString() const
{
    std::lock_guard<std::mutex> lock(m_swap_mutex);
    const SYSTEMTIME& st = m_last_update_time;
    if (st.wYear == 0) return L"";
    wchar_t tb[64]{};
    swprintf_s(tb, L"%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return L"最后更新: " + std::wstring(tb);
}

void CDataManager::SwapBuffers()
{
    std::lock_guard<std::mutex> lock(m_swap_mutex);
    if (!m_has_new_data) return;
    m_results = std::move(m_pending);
    m_has_new_data = false;
}
void CDataManager::RotateLogIfNeeded(const std::wstring& log_path)
{
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (GetFileAttributesExW(log_path.c_str(), GetFileExInfoStandard, &fad))
    {
        LARGE_INTEGER size;
        size.LowPart = fad.nFileSizeLow;
        size.HighPart = fad.nFileSizeHigh;
        if (size.QuadPart > 1024 * 1024) // 超过 1MB 自动滚动
        {
            std::wstring bak_path = log_path + L".bak";
            DeleteFileW(bak_path.c_str());
            MoveFileExW(log_path.c_str(), bak_path.c_str(), MOVEFILE_REPLACE_EXISTING);
        }
    }
}

void CDataManager::LogInfo(const wchar_t* format, ...)
{
    std::wstring log_path;
    {
        std::lock_guard<std::mutex> lock(m_swap_mutex);
        log_path = m_config_path;
    }
    if (log_path.empty()) return;
    std::lock_guard<std::mutex> log_lock(m_log_mutex);
    size_t dot_pos = log_path.find_last_of(L".");
    if (dot_pos != std::wstring::npos)
        log_path = log_path.substr(0, dot_pos);
    log_path += L"_debug.log";

    RotateLogIfNeeded(log_path);

    va_list args;
    va_start(args, format);
    int len = _vscwprintf(format, args) + 1;
    std::vector<wchar_t> buf(len);
    vswprintf_s(buf.data(), len, format, args);
    va_end(args);

    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t time_str[64]{};
    swprintf_s(time_str, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    std::wstring full_msg = time_str + std::wstring(buf.data()) + L"\n";
    std::string utf8_msg = WideToUtf8(full_msg);

    std::ofstream log_file(log_path, std::ios::out | std::ios::app | std::ios::binary);
    if (log_file.is_open())
    {
        log_file.write(utf8_msg.data(), utf8_msg.size());
        log_file.flush();
    }
}
