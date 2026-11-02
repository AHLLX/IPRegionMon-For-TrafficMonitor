#include "../../pch.h"
#include "../../DataManager.h"
#include "../../ModuleTask.h"
#include "../../OptionsDlg.h"
#include <commctrl.h>
#include <stdexcept>
#pragma comment(lib, "comctl32.lib")

struct ConcurrencyTestAccess
{
    static HANDLE WINAPI FailThread(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE,
        LPVOID, DWORD, LPDWORD)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return nullptr;
    }
    static HANDLE WINAPI ThrowThread(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE,
        LPVOID, DWORD, LPDWORD)
    {
        throw std::runtime_error("injected launch failure");
    }
    static void Failure(int mode)
    {
        ModuleTask::Detail::Factory() = mode == 1 ? &FailThread :
            mode == 2 ? &ThrowThread : &CreateThread;
    }
    static void FailUpdate() { g_data.m_fail_update_for_test.store(true); }
    static BOOL CompleteProxy(const wchar_t* base)
    {
        HWND parent = CreateWindowExW(0, L"STATIC", L"test", WS_POPUP,
            0, 0, 600, 400, nullptr, nullptr, nullptr, nullptr);
        if (!parent) return FALSE;
        CWnd parentWindow;
        parentWindow.Attach(parent);
        bool passed = false;
        {
            CTabProxyApiDlg page(&parentWindow);
            if (page.Create(IDD_TAB_PROXY_API, &parentWindow))
            {
                page.m_chkUseProxy.SetCheck(BST_CHECKED);
                page.UpdateProxyEditState();
                page.m_edtProxy.SetWindowTextW(std::wstring(base).substr(7).c_str());
                Failure(1);
                page.OnBnClickedTestProxy();
                passed = !page.m_testResult && page.m_btnTestProxy.IsWindowEnabled();
                Failure(0);
                page.OnBnClickedTestProxy();
                for (int i = 0; i < 500 && page.m_testResult; ++i)
                {
                    Sleep(10);
                    page.OnTimer(3);
                }
                CString message;
                page.m_stcTestProxyStatus.GetWindowTextW(message);
                passed = passed && !page.m_testResult &&
                    page.m_btnTestProxy.IsWindowEnabled() &&
                    message.Find(L"✗ 代理连接失败:") == 0;
                page.DestroyWindow();
            }
        }
        parentWindow.Detach();
        DestroyWindow(parent);
        return passed;
    }
    static BOOL CloseDialog(const wchar_t* base, BOOL proxy, HANDLE received)
    {
        INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_LISTVIEW_CLASSES };
        if (!InitCommonControlsEx(&controls)) return FALSE;
        HWND parent = CreateWindowExW(0, L"STATIC", L"test", WS_POPUP,
            0, 0, 600, 400, nullptr, nullptr, nullptr, nullptr);
        if (!parent) return FALSE;
        CWnd parentWindow;
        parentWindow.Attach(parent);
        bool passed = false;
        if (proxy)
        {
            CTabProxyApiDlg page(&parentWindow);
            if (page.Create(IDD_TAB_PROXY_API, &parentWindow))
            {
                const std::wstring address = std::wstring(base).substr(7);
                page.m_chkUseProxy.SetCheck(BST_CHECKED);
                page.UpdateProxyEditState();
                page.m_edtProxy.SetWindowTextW(address.c_str());
                page.OnBnClickedTestProxy();
                const std::weak_ptr<AsyncTextResult> result = page.m_testResult;
                const bool started = WaitForSingleObject(received, 2000) == WAIT_OBJECT_0;
                page.DestroyWindow();
                passed = started && !page.m_testResult && !result.expired();
            }
        }
        else
        {
            CTabLatencyDlg page(&parentWindow);
            if (page.Create(IDD_TAB_LATENCY_LIST, &parentWindow))
            {
                page.m_listCtrl.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED,
                    LVIS_SELECTED | LVIS_FOCUSED);
                page.OnBnClickedTest();
                const std::weak_ptr<AsyncTextResult> result = page.m_testResult;
                const bool started = WaitForSingleObject(received, 2000) == WAIT_OBJECT_0;
                page.DestroyWindow();
                passed = started && !page.m_testResult && !result.expired();
            }
        }
        parentWindow.Detach();
        DestroyWindow(parent);
        return passed;
    }
};

extern "C" __declspec(dllexport) BOOL TestCloseDialog(
    const wchar_t* base, BOOL proxy, HANDLE received)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return ConcurrencyTestAccess::CloseDialog(base, proxy, received);
}

extern "C" __declspec(dllexport) BOOL TestCompleteProxy(const wchar_t* base)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return ConcurrencyTestAccess::CompleteProxy(base);
}

extern "C" __declspec(dllexport) void TestConfigure(const wchar_t* base, BOOL second)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    auto settings = g_data.GetSettings();
    settings.use_proxy = false;
    settings.api_domestic_url = std::wstring(base) + (second ? L"/ipB" : L"/ipA");
    settings.latency_targets = { { second ? L"B" : L"A",
        std::wstring(base) + (second ? L"/latencyB" : L"/latencyA") } };
    settings.display_format = DF_IP_ONLY;
    g_data.SetSettings(std::move(settings));
}

extern "C" __declspec(dllexport) void TestUpdate() { g_data.ForceUpdateAsync(); }
extern "C" __declspec(dllexport) BOOL TestBusy() { return g_data.IsUpdating(); }
extern "C" __declspec(dllexport) void TestSwap() { g_data.SwapBuffers(); }
extern "C" __declspec(dllexport) void TestFailure(int mode) { ConcurrencyTestAccess::Failure(mode); }
extern "C" __declspec(dllexport) void TestFailUpdate() { ConcurrencyTestAccess::FailUpdate(); }
extern "C" __declspec(dllexport) const wchar_t* TestText() { return g_data.GetFormattedItemText(false); }
extern "C" __declspec(dllexport) const wchar_t* TestTooltip() { return g_data.GetTooltipText(); }
extern "C" __declspec(dllexport) int TestState()
{
    const auto result = g_data.GetResults();
    if (result.m_public_ip_direct.empty()) return 0;
    if (result.m_latency_results.size() != 1) return -1;
    const bool second = result.m_public_ip_direct == L"198.51.100.2";
    if (!second && result.m_public_ip_direct != L"198.51.100.1") return -2;
    const auto& latency = result.m_latency_results.front();
    const std::wstring suffix = second ? L"/latencyB" : L"/latencyA";
    if (latency.name != (second ? L"B" : L"A") ||
        latency.url.size() < suffix.size() ||
        latency.url.compare(latency.url.size() - suffix.size(), suffix.size(), suffix) != 0)
        return -3;
    return second ? 2 : 1;
}
