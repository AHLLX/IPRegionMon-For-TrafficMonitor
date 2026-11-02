#include "../../pch.h"
#include "../../OptionsDlg.h"
#include "../../UiLanguage.h"
#include "../../IPRegionMon.h"
#include "../../CountryInfo.h"
#include <vector>
#include <cstdio>

struct LocalizationTestAccess
{
    static void Seed(int format, const std::wstring& place)
    {
        {
            std::lock_guard<std::mutex> lock(g_data.m_swap_mutex);
            g_data.m_setting_data.display_format = format;
            g_data.m_results.m_place_direct = g_data.m_results.m_place_proxy = place;
            g_data.m_results.m_country_code_direct = L"GB";
            g_data.m_results.m_country_code_proxy = L"US";
            g_data.m_results.m_public_ip_direct = g_data.m_results.m_public_ip_proxy =
                L"2001:db8:1234:5678:90ab:cdef:1234:5678";
            g_data.m_results.m_isp_direct = g_data.m_results.m_isp_proxy = std::wstring(120, L'W');
            g_data.m_results.m_latency_results = { {L"test", L"http://example.test", 1199, 978} };
        }
        g_data.ReformatTaskbarTexts();
    }
    static bool ValidUtf16(const std::wstring& text)
    {
        for (size_t index = 0; index < text.size(); ++index)
        {
            if (text[index] >= 0xd800 && text[index] <= 0xdbff)
            {
                if (++index >= text.size() || text[index] < 0xdc00 || text[index] > 0xdfff) return false;
            }
            else if (text[index] >= 0xdc00 && text[index] <= 0xdfff) return false;
        }
        return true;
    }
    static bool CheckWidth()
    {
        const auto saved = g_data.GetSnapshot();
        const std::wstring places[]{ std::wstring(120, L'W'),
            L"中华人民共和国 黑龙江省 非常长的地区名称及行政区域",
            L"Very long region \U0001F30D with a combining accent e\x0301 and more text" };
        HDC dc = CreateCompatibleDC(nullptr);
        bool passed = dc != nullptr;
        int checks = 0;
        for (int dpi : { 96, 144, 192 })
        for (const wchar_t* face : { L"Segoe UI", L"Microsoft YaHei UI", L"Tahoma" })
        {
            HFONT font = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, face);
            if (!dc || !font) { passed = false; if (font) DeleteObject(font); continue; }
            const HGDIOBJ old = SelectObject(dc, font);
            for (int format = 0; format <= DF_COUNTRY_CODE; ++format)
            for (const auto& place : places)
            {
                Seed(format, place);
                CIPRegionMon::Instance().OnExtenedInfo(ITMPlugin::EI_DRAW_TASKBAR_WND, L"1");
                for (int route = 0; route < 2; ++route)
                {
                    auto* item = CIPRegionMon::Instance().GetItem(route);
                    const std::wstring sample = item->GetItemValueSampleText();
                    const std::wstring value = item->GetItemValueText();
                    SIZE available{}, actual{};
                    GetTextExtentPoint32W(dc, sample.c_str(), static_cast<int>(sample.size()), &available);
                    GetTextExtentPoint32W(dc, value.c_str(), static_cast<int>(value.size()), &actual);
                    ++checks;
                    if (actual.cx > available.cx || !ValidUtf16(value))
                    {
                        std::printf("FAIL width: dpi=%d format=%d route=%d value=%ld sample=%ld\n",
                            dpi, format, route, actual.cx, available.cx);
                        passed = false;
                    }
                    if (format == DF_REGION_AND_LATENCY)
                    {
                        const std::wstring suffix = route ? L" 978ms" : L" 1199ms";
                        passed = passed && value.size() >= suffix.size() &&
                            value.substr(value.size() - suffix.size()) == suffix &&
                            value.find(L'\x2026') != std::wstring::npos && value.size() < 40;
                    }
                    if (format == DF_COUNTRY_CODE)
                        passed = passed && value == (route ? L"US" : L"GB");
                }
                passed = passed && g_data.GetResults().m_place_direct == place;
            }
            SelectObject(dc, old);
            DeleteObject(font);
        }
        if (dc) DeleteDC(dc);
        // Reproduce a publication between the host's measure and paint calls.
        auto* item = CIPRegionMon::Instance().GetItem(0);
        Seed(DF_REGION_AND_LATENCY, L"Short");
        const wchar_t* samplePointer = item->GetItemValueSampleText();
        const std::wstring measured(samplePointer);
        item->GetItemValueText();
        passed = passed && measured == samplePointer;
        Seed(DF_REGION_AND_LATENCY, L"A much longer region published after measurement");
        const wchar_t* valuePointer = item->GetItemValueText();
        const std::wstring held(valuePointer);
        passed = passed && held == measured;
        const std::wstring nextSample(item->GetItemValueSampleText());
        passed = passed && held == valuePointer && nextSample != measured &&
            nextSample == item->GetItemValueText();
        CIPRegionMon::Instance().OnExtenedInfo(ITMPlugin::EI_DRAW_TASKBAR_WND, L"0");
        Seed(DF_REGION_AND_LATENCY, L"Newest main-window data");
        passed = passed && std::wstring(item->GetItemValueText()).find(L"Newest") == 0;
        {
            std::lock_guard<std::mutex> lock(g_data.m_swap_mutex);
            g_data.m_setting_data = saved.settings;
            g_data.m_results = saved.results;
        }
        CIPRegionMon::Instance().OnExtenedInfo(ITMPlugin::EI_DRAW_TASKBAR_WND, L"0");
        std::printf("Taskbar GDI checks: %d\n", checks);
        return passed;
    }
    static bool HasHan(const std::wstring& value)
    {
        for (wchar_t character : value) if (character >= 0x4e00 && character <= 0x9fff) return true;
        return false;
    }
    static BOOL CALLBACK CheckControl(HWND window, LPARAM parameter)
    {
        // These controls contain provider data, which must not be translated.
        const int id = GetDlgCtrlID(window);
        if (id == IDC_STAT_DIRECT_PLACE || id == IDC_STAT_PROXY_PLACE ||
            id == IDC_STAT_DIRECT_ISP || id == IDC_STAT_PROXY_ISP || id == IDC_STATUS_INFO_EDIT)
            return TRUE;
        wchar_t text[1024]{};
        GetWindowTextW(window, text, 1024);
        if (HasHan(text)) *reinterpret_cast<bool*>(parameter) = false;
        return TRUE;
    }
    static bool CheckUi(COptionsDlg& dialog, bool chinese)
    {
        bool passed = true;
        if (!chinese)
        {
            EnumChildWindows(dialog.m_hWnd, &CheckControl, reinterpret_cast<LPARAM>(&passed));
            for (const auto& tab : dialog.m_tabBar.m_tabs) passed = passed && !HasHan(tab);
            for (const auto* page : { static_cast<CModernSubDlg*>(&dialog.m_pageProxyApi),
                static_cast<CModernSubDlg*>(&dialog.m_pageLatency), static_cast<CModernSubDlg*>(&dialog.m_pageStatus) })
                for (const auto& card : page->m_cards) passed = passed && !HasHan(card.title);
        }
        auto* list = static_cast<CListCtrl*>(dialog.m_pageLatency.GetDlgItem(IDC_LATENCY_LIST));
        for (int index = 0; index < 4; ++index)
        {
            wchar_t text[128]{};
            HDITEMW column{};
            column.mask = HDI_TEXT; column.pszText = text; column.cchTextMax = 128;
            passed = passed && list->GetHeaderCtrl()->GetItem(index, &column);
            if (!chinese) passed = passed && !HasHan(text);
        }
        SettingData roundtrip = g_data.GetSettings();
        dialog.m_pageProxyApi.GetData(roundtrip);
        dialog.m_pageLatency.GetData(roundtrip);
        const auto original = g_data.GetSettings();
        passed = passed && roundtrip.display_format == original.display_format;
        passed = passed && roundtrip.latency_targets.size() == original.latency_targets.size();
        for (size_t index = 0; index < original.latency_targets.size(); ++index)
            passed = passed && roundtrip.latency_targets[index].name == original.latency_targets[index].name &&
                roundtrip.latency_targets[index].url == original.latency_targets[index].url;
        return passed;
    }
};

extern "C" __declspec(dllexport) void TestUseCountryFormat()
{
    auto settings = g_data.GetSettings();
    settings.display_format = DF_COUNTRY_CODE;
    g_data.SetSettings(std::move(settings));
    g_data.ReformatTaskbarTexts();
}

extern "C" __declspec(dllexport) BOOL TestCountryNames()
{
    const auto british = CountryInfo::Resolve(L"", L"United Kingdom");
    const auto chinese = CountryInfo::Resolve(L"", L"中国");
    const auto russian = CountryInfo::Resolve(L"ru", L"Russia");
    const auto codeOnly = CountryInfo::Resolve(L"", L"GB");
    const auto unknown = CountryInfo::Resolve(L"1?", L"Unknown provider country");
    return british.code == L"GB" && chinese.code == L"CN" && russian.code == L"RU" &&
        codeOnly.code == L"GB" && codeOnly.name == L"United Kingdom" && unknown.code.empty();
}

extern "C" __declspec(dllexport) BOOL TestTaskbarWidths()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return LocalizationTestAccess::CheckWidth();
}

extern "C" __declspec(dllexport) BOOL TestLocalizedDialogs(BOOL chinese)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    const auto settings = g_data.GetSettings();
    auto custom = settings;
    custom.latency_targets.push_back({ L"用户节点", L"https://example.test/?name=中文" });
    g_data.SetSettings(std::move(custom));
    HWND handle = CreateWindowExW(0, L"STATIC", L"test", WS_POPUP,
        0, 0, 1000, 700, nullptr, nullptr, nullptr, nullptr);
    if (!handle) return FALSE;
    CWnd parent;
    parent.Attach(handle);
    bool passed = false;
    {
        COptionsDlg dialog(&parent);
        if (dialog.CreateIndirect(UiLanguage::Dialog(IDD_OPTIONS_DIALOG), &parent))
        {
            CString caption, ok, cancel;
            dialog.GetWindowTextW(caption);
            dialog.GetDlgItem(IDOK)->GetWindowTextW(ok);
            dialog.GetDlgItem(IDCANCEL)->GetWindowTextW(cancel);
            passed = caption == (chinese ? L"IP 地区与网络监控设置" : L"IP Region & Latency Monitor Settings") &&
                ok == (chinese ? L"确定" : L"OK") && cancel == (chinese ? L"取消" : L"Cancel") &&
                !g_data.IsUpdating() && LocalizationTestAccess::CheckUi(dialog, chinese != FALSE);
            dialog.DestroyWindow();
        }
    }
    parent.Detach();
    DestroyWindow(handle);
    g_data.SetSettings(settings);
    return passed;
}
