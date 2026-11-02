#include "pch.h"
#include "OptionsDlg.h"
#include "UiLanguage.h"

#include <uxtheme.h>

// ====================================================================
// CTabProxyApiDlg —— Tab 0: 常规 / 网络 & 代理
// ====================================================================
IMPLEMENT_DYNAMIC(CTabProxyApiDlg, CModernSubDlg)

CTabProxyApiDlg::CTabProxyApiDlg(CWnd* pParent)
    : CModernSubDlg(IDD_TAB_PROXY_API, pParent) {}

void CTabProxyApiDlg::DoDataExchange(CDataExchange* pDX)
{
    CModernSubDlg::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_DISPLAY_FORMAT_COMBO,   m_cmbDisplayFormat);
    DDX_Control(pDX, IDC_USE_PROXY_CHECK,        m_chkUseProxy);
    DDX_Control(pDX, IDC_PROXY_EDIT,             m_edtProxy);
    DDX_Control(pDX, IDC_API_DOMESTIC_EDIT,      m_edtDomesticApi);
    DDX_Control(pDX, IDC_API_FOREIGN_EDIT,       m_edtForeignApi);
    DDX_Control(pDX, IDC_REFRESH_INTERVAL_EDIT,  m_edtInterval);
    DDX_Control(pDX, IDC_REFRESH_NOW_BUTTON,     m_btnRefresh);
    DDX_Control(pDX, IDC_STATUS_STATIC,          m_stcStatus);
    DDX_Control(pDX, IDC_TEST_PROXY_BUTTON,      m_btnTestProxy);
    DDX_Control(pDX, IDC_TEST_PROXY_STATUS,      m_stcTestProxyStatus);
}

BEGIN_MESSAGE_MAP(CTabProxyApiDlg, CModernSubDlg)
    ON_BN_CLICKED(IDC_USE_PROXY_CHECK,    &CTabProxyApiDlg::OnBnClickedUseProxy)
    ON_BN_CLICKED(IDC_REFRESH_NOW_BUTTON, &CTabProxyApiDlg::OnBnClickedRefreshNow)
    ON_BN_CLICKED(IDC_TEST_PROXY_BUTTON,  &CTabProxyApiDlg::OnBnClickedTestProxy)
    ON_CBN_SELCHANGE(IDC_DISPLAY_FORMAT_COMBO, &CTabProxyApiDlg::OnCbnSelchangeDisplayFormat)
    ON_WM_TIMER()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CTabProxyApiDlg::OnInitDialog()
{
    CModernSubDlg::OnInitDialog();

    // 注册三大现代圆角卡片 (DLU 坐标)
    AddCard(0, 0, 426, 54, UiLanguage::Text(IDS_CARD_DISPLAY).c_str());
    AddCard(0, 60, 426, 120, UiLanguage::Text(IDS_CARD_PROXY).c_str());
    AddCard(0, 126, 426, 212, UiLanguage::Text(IDS_CARD_API).c_str());

    // 初始化显示格式下拉框
    m_cmbDisplayFormat.ResetContent();
    m_cmbDisplayFormat.AddString(UiLanguage::Text(IDS_FORMAT_REGION).c_str());
    m_cmbDisplayFormat.AddString(UiLanguage::Text(IDS_FORMAT_REGION_IP).c_str());
    m_cmbDisplayFormat.AddString(UiLanguage::Text(IDS_FORMAT_IP).c_str());
    m_cmbDisplayFormat.AddString(UiLanguage::Text(IDS_FORMAT_REGION_LATENCY).c_str());
    m_cmbDisplayFormat.AddString(UiLanguage::Text(IDS_FORMAT_FULL).c_str());
    m_cmbDisplayFormat.AddString(UiLanguage::Text(IDS_FORMAT_COUNTRY_CODE).c_str());

    const SettingData d = g_data.GetSettings();
    int fmt = d.display_format;
    if (fmt < 0 || fmt > DF_COUNTRY_CODE) fmt = DF_COUNTRY_CODE;
    m_cmbDisplayFormat.SetCurSel(fmt);

    m_chkUseProxy.SetCheck(d.use_proxy ? BST_CHECKED : BST_UNCHECKED);
    m_edtProxy.SetWindowTextW(d.proxy_address.c_str());
    m_edtDomesticApi.SetWindowTextW(d.api_domestic_url.c_str());
    m_edtForeignApi.SetWindowTextW(d.api_foreign_url.c_str());

    wchar_t buf[16]{};
    swprintf_s(buf, L"%d", d.refresh_interval_sec);
    m_edtInterval.SetWindowTextW(buf);

    UpdateProxyEditState();

    // 启动 1 秒心跳定时器用于自动刷新倒计时动态可视化
    SetTimer(2, 1000, nullptr);

    return TRUE;
}

void CTabProxyApiDlg::UpdateProxyEditState()
{
    BOOL useProxy = (m_chkUseProxy.GetCheck() == BST_CHECKED);
    GetDlgItem(IDC_PROXY_LABEL)->EnableWindow(useProxy);
    m_edtProxy.EnableWindow(useProxy);
    m_btnTestProxy.EnableWindow(useProxy);
    GetDlgItem(IDC_API_FOREIGN_LABEL)->EnableWindow(useProxy);
    m_edtForeignApi.EnableWindow(useProxy);
}

void CTabProxyApiDlg::OnBnClickedUseProxy()
{
    UpdateProxyEditState();
}

void CTabProxyApiDlg::OnCbnSelchangeDisplayFormat()
{
    int fmt = m_cmbDisplayFormat.GetCurSel();
    if (fmt >= 0 && fmt <= DF_COUNTRY_CODE)
    {
        auto settings = g_data.GetSettings();
        settings.display_format = fmt;
        g_data.SetSettings(std::move(settings));
        g_data.ReformatTaskbarTexts();
        g_data.SaveConfig();
    }
}

void CTabProxyApiDlg::OnBnClickedRefreshNow()
{
    // 1. 立即同步当前界面所有配置到全局数据并重绘任务栏
    auto settings = g_data.GetSettings();
    GetData(settings);
    g_data.SetSettings(std::move(settings));
    g_data.SaveConfig();
    g_data.ReformatTaskbarTexts();

    // 2. 启动后台并发探测
    g_data.ForceUpdateAsync();
    m_btnRefresh.EnableWindow(FALSE);
    m_stcStatus.SetWindowTextW(UiLanguage::Text(IDS_REFRESH_RUNNING).c_str());
    SetTimer(1, 300, nullptr);
}

void CTabProxyApiDlg::OnBnClickedTestProxy()
{
    CString proxy;
    m_edtProxy.GetWindowTextW(proxy);
    std::wstring proxy_str = proxy.GetString();
    m_btnTestProxy.EnableWindow(FALSE);
    m_stcTestProxyStatus.SetWindowTextW(UiLanguage::Text(IDS_PROXY_TESTING).c_str());
    try
    {
        m_testResult = AsyncTextResult::Start([proxy_str]() {
            std::wstring msg;
            double ms = -1.0;
            bool ok = g_data.TestProxyConnectivity(proxy_str, msg, ms);
            return ok ? UiLanguage::Format(IDS_PROXY_TEST_OK, ms) :
                UiLanguage::Format(IDS_PROXY_TEST_FAILED, msg.c_str());
        });
    }
    catch (...) { m_testResult.reset(); }
    if (!m_testResult || !SetTimer(3, 100, nullptr))
    {
        m_testResult.reset();
        OnTestProxyDone(UiLanguage::Text(IDS_PROXY_TASK_FAILED).c_str());
    }
}

void CTabProxyApiDlg::OnTestProxyDone(const std::wstring& message)
{
    m_stcTestProxyStatus.SetWindowTextW(message.empty()
        ? UiLanguage::Text(IDS_PROXY_BACKGROUND_FAILED).c_str() : message.c_str());
    m_btnTestProxy.EnableWindow(TRUE);
}

void CTabProxyApiDlg::OnDestroy()
{
    KillTimer(3);
    m_testResult.reset();
    CModernSubDlg::OnDestroy();
}
void CTabProxyApiDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 3)
    {
        std::wstring message;
        if (m_testResult && m_testResult->Take(message))
        {
            KillTimer(3);
            m_testResult.reset();
            OnTestProxyDone(message);
        }
        return;
    }
    if (nIDEvent == 1)
    {
        // 探测完成检测
        if (!g_data.IsUpdating())
        {
            KillTimer(1);
            m_btnRefresh.EnableWindow(TRUE);
        }
    }
    else if (nIDEvent == 2)
    {
        // 自动刷新倒计时动态可视化
        if (g_data.IsUpdating())
        {
            m_stcStatus.SetWindowTextW(UiLanguage::Text(IDS_REFRESH_RUNNING).c_str());
        }
        else
        {
            int remainSec = g_data.GetRefreshCountdownSec();
            wchar_t tip[128];
            if (remainSec > 0)
            {
                swprintf_s(tip, UiLanguage::Text(IDS_REFRESH_COUNTDOWN).c_str(), remainSec);
            }
            else
            {
                swprintf_s(tip, UiLanguage::Text(IDS_REFRESH_SOON).c_str());
            }
            m_stcStatus.SetWindowTextW(tip);
        }
    }
    CModernSubDlg::OnTimer(nIDEvent);
}

void CTabProxyApiDlg::GetData(SettingData& out) const
{
    out.display_format = m_cmbDisplayFormat.GetCurSel();
    out.use_proxy = (m_chkUseProxy.GetCheck() == BST_CHECKED);

    CString s;
    m_edtProxy.GetWindowTextW(s);
    out.proxy_address = s.GetString();

    m_edtDomesticApi.GetWindowTextW(s);
    out.api_domestic_url = s.GetString();

    m_edtForeignApi.GetWindowTextW(s);
    out.api_foreign_url = s.GetString();

    m_edtInterval.GetWindowTextW(s);
    int interval = _wtoi(s.GetString());
    if (interval < 5)   interval = 5;
    if (interval > 3600) interval = 3600;
    out.refresh_interval_sec = interval;
}

// ====================================================================
// CTabLatencyDlg —— Tab 1: 延迟目标设置
// ====================================================================
IMPLEMENT_DYNAMIC(CTabLatencyDlg, CModernSubDlg)

CTabLatencyDlg::CTabLatencyDlg(CWnd* pParent)
    : CModernSubDlg(IDD_TAB_LATENCY_LIST, pParent) {}

void CTabLatencyDlg::DoDataExchange(CDataExchange* pDX)
{
    CModernSubDlg::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LATENCY_LIST,       m_listCtrl);
    DDX_Control(pDX, IDC_LAT_NAME_EDIT,      m_edtName);
    DDX_Control(pDX, IDC_LAT_URL_EDIT,       m_edtUrl);
    DDX_Control(pDX, IDC_LAT_ADD_BUTTON,     m_btnAdd);
    DDX_Control(pDX, IDC_LAT_DEL_BUTTON,     m_btnDel);
    DDX_Control(pDX, IDC_LAT_UP_BUTTON,      m_btnUp);
    DDX_Control(pDX, IDC_LAT_DOWN_BUTTON,    m_btnDown);
    DDX_Control(pDX, IDC_LAT_TEST_BUTTON,    m_btnTest);
}

BEGIN_MESSAGE_MAP(CTabLatencyDlg, CModernSubDlg)
    ON_BN_CLICKED(IDC_LAT_ADD_BUTTON,  &CTabLatencyDlg::OnBnClickedAdd)
    ON_BN_CLICKED(IDC_LAT_DEL_BUTTON,  &CTabLatencyDlg::OnBnClickedDel)
    ON_BN_CLICKED(IDC_LAT_UP_BUTTON,   &CTabLatencyDlg::OnBnClickedUp)
    ON_BN_CLICKED(IDC_LAT_DOWN_BUTTON, &CTabLatencyDlg::OnBnClickedDown)
    ON_BN_CLICKED(IDC_LAT_TEST_BUTTON, &CTabLatencyDlg::OnBnClickedTest)
    ON_NOTIFY(NM_CLICK,   IDC_LATENCY_LIST, &CTabLatencyDlg::OnNMClickList)
    ON_NOTIFY(NM_DBLCLK,  IDC_LATENCY_LIST, &CTabLatencyDlg::OnNMDblclkList)
    ON_WM_SIZE()
    ON_WM_TIMER()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CTabLatencyDlg::OnInitDialog()
{
    CModernSubDlg::OnInitDialog();

    // 注册卡片 (DLU 坐标)
    AddCard(0, 0, 426, 142, UiLanguage::Text(IDS_CARD_TARGETS).c_str());
    AddCard(0, 148, 426, 212, UiLanguage::Text(IDS_CARD_TARGET_EDIT).c_str());

    // 突出主操作按钮
    m_btnAdd.SetAccent(true);

    // 启用现代 Explorer 风格列表
    SetWindowTheme(m_listCtrl.m_hWnd, L"Explorer", NULL);
    m_listCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    m_listCtrl.InsertColumn(0, UiLanguage::Text(IDS_COLUMN_NAME).c_str(),     LVCFMT_LEFT, 75);
    m_listCtrl.InsertColumn(1, UiLanguage::Text(IDS_COLUMN_URL).c_str(), LVCFMT_LEFT, 235);
    m_listCtrl.InsertColumn(2, UiLanguage::Text(IDS_COLUMN_DIRECT).c_str(), LVCFMT_RIGHT, 85);
    m_listCtrl.InsertColumn(3, UiLanguage::Text(IDS_COLUMN_PROXY).c_str(), LVCFMT_RIGHT, 85);

    m_targets = g_data.GetSettings().latency_targets;
    RefreshList();
    AdjustColumnWidths();

    return TRUE;
}

void CTabLatencyDlg::AdjustColumnWidths()
{
    if (!m_listCtrl.GetSafeHwnd()) return;
    CRect rc;
    m_listCtrl.GetClientRect(&rc);
    int totalW = rc.Width();
    if (totalW <= 80) return;

    int availW = totalW - 4; // 扣除轻微边缘留白

    // 比例分配：名称 20%，直连延迟 17%，代理延迟 17%，剩余全部自适应给 地址/URL
    int w0 = availW * 20 / 100;
    if (w0 < 65) w0 = 65;

    int w2 = availW * 17 / 100;
    if (w2 < 65) w2 = 65;

    int w3 = availW * 17 / 100;
    if (w3 < 65) w3 = 65;

    int w1 = availW - w0 - w2 - w3;
    if (w1 < 90) w1 = 90;

    m_listCtrl.SetColumnWidth(0, w0);
    m_listCtrl.SetColumnWidth(1, w1);
    m_listCtrl.SetColumnWidth(2, w2);
    m_listCtrl.SetColumnWidth(3, w3);
}

void CTabLatencyDlg::OnSize(UINT nType, int cx, int cy)
{
    CModernSubDlg::OnSize(nType, cx, cy);
    AdjustColumnWidths();
}

void CTabLatencyDlg::RefreshList()
{
    const auto results = g_data.GetResults();
    m_listCtrl.DeleteAllItems();
    for (size_t i = 0; i < m_targets.size(); ++i)
    {
        const auto& t = m_targets[i];
        int idx = m_listCtrl.InsertItem((int)i, t.name.c_str());
        m_listCtrl.SetItemText(idx, 1, t.url.c_str());

        // 读当前的实时测速结果
        double dMs = -1.0, pMs = -1.0;
        for (const auto& r : results.m_latency_results)
        {
            if (r.url == t.url || r.name == t.name)
            {
                dMs = r.direct_ms;
                pMs = r.proxy_ms;
                break;
            }
        }

        wchar_t dBuf[32] = L"-";
        wchar_t pBuf[32] = L"-";
        if (dMs >= 0) swprintf_s(dBuf, L"%.0f ms", dMs);
        if (pMs >= 0) swprintf_s(pBuf, L"%.0f ms", pMs);

        m_listCtrl.SetItemText(idx, 2, dBuf);
        m_listCtrl.SetItemText(idx, 3, pBuf);
    }
}

void CTabLatencyDlg::LoadSelectedToEdit()
{
    POSITION pos = m_listCtrl.GetFirstSelectedItemPosition();
    if (pos)
    {
        int idx = m_listCtrl.GetNextSelectedItem(pos);
        if (idx >= 0 && idx < (int)m_targets.size())
        {
            m_edtName.SetWindowTextW(m_targets[idx].name.c_str());
            m_edtUrl.SetWindowTextW(m_targets[idx].url.c_str());
        }
    }
}

void CTabLatencyDlg::OnNMClickList(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    LoadSelectedToEdit();
    *pResult = 0;
}

void CTabLatencyDlg::OnNMDblclkList(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    LoadSelectedToEdit();
    *pResult = 0;
}

void CTabLatencyDlg::OnBnClickedAdd()
{
    CString name, url;
    m_edtName.GetWindowTextW(name);
    m_edtUrl.GetWindowTextW(url);

    if (name.IsEmpty() || url.IsEmpty())
    {
        AfxMessageBox(UiLanguage::Text(IDS_TARGET_REQUIRED).c_str(), MB_ICONWARNING);
        return;
    }

    POSITION pos = m_listCtrl.GetFirstSelectedItemPosition();
    if (pos)
    {
        int idx = m_listCtrl.GetNextSelectedItem(pos);
        if (idx >= 0 && idx < (int)m_targets.size())
        {
            m_targets[idx].name = name.GetString();
            m_targets[idx].url  = url.GetString();
            RefreshList();
            AdjustColumnWidths();
            return;
        }
    }

    LatencyTarget target;
    target.name = name.GetString();
    target.url  = url.GetString();
    m_targets.push_back(target);
    RefreshList();
    AdjustColumnWidths();
}

void CTabLatencyDlg::OnBnClickedDel()
{
    POSITION pos = m_listCtrl.GetFirstSelectedItemPosition();
    if (pos)
    {
        int idx = m_listCtrl.GetNextSelectedItem(pos);
        if (idx >= 0 && idx < (int)m_targets.size())
        {
            m_targets.erase(m_targets.begin() + idx);
            RefreshList();
            AdjustColumnWidths();
        }
    }
}

void CTabLatencyDlg::OnBnClickedUp()
{
    POSITION pos = m_listCtrl.GetFirstSelectedItemPosition();
    if (pos)
    {
        int idx = m_listCtrl.GetNextSelectedItem(pos);
        if (idx > 0 && idx < (int)m_targets.size())
        {
            std::swap(m_targets[idx], m_targets[idx - 1]);
            RefreshList();
            AdjustColumnWidths();
            m_listCtrl.SetItemState(idx - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
    }
}

void CTabLatencyDlg::OnBnClickedDown()
{
    POSITION pos = m_listCtrl.GetFirstSelectedItemPosition();
    if (pos)
    {
        int idx = m_listCtrl.GetNextSelectedItem(pos);
        if (idx >= 0 && idx + 1 < (int)m_targets.size())
        {
            std::swap(m_targets[idx], m_targets[idx + 1]);
            RefreshList();
            AdjustColumnWidths();
            m_listCtrl.SetItemState(idx + 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
    }
}

void CTabLatencyDlg::OnBnClickedTest()
{
    POSITION pos = m_listCtrl.GetFirstSelectedItemPosition();
    if (!pos)
    {
        AfxMessageBox(UiLanguage::Text(IDS_TARGET_SELECT).c_str(), MB_ICONINFORMATION);
        return;
    }
    int idx = m_listCtrl.GetNextSelectedItem(pos);
    if (idx < 0 || idx >= (int)m_targets.size()) return;

    std::wstring url = m_targets[idx].url;
    const auto settings = g_data.GetSettings();
    std::wstring proxy = settings.use_proxy ? settings.proxy_address : L"";

    m_btnTest.EnableWindow(FALSE);
    try
    {
        m_testResult = AsyncTextResult::Start([url, proxy]() {
            double dMs = -1.0, pMs = -1.0;
            CDataManager::MeasureLatencyMs(url, L"", true, dMs);
            if (!proxy.empty())
                CDataManager::MeasureLatencyMs(url, proxy, false, pMs);
            return UiLanguage::Format(IDS_LATENCY_TEST_DONE, dMs, pMs);
        });
    }
    catch (...) { m_testResult.reset(); }
    if (!m_testResult || !SetTimer(3, 100, nullptr))
    {
        m_testResult.reset();
        OnTestLatencyDone(UiLanguage::Text(IDS_TASK_START_FAILED).c_str());
    }
}

void CTabLatencyDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 3)
    {
        std::wstring message;
        if (m_testResult && m_testResult->Take(message))
        {
            KillTimer(3);
            m_testResult.reset();
            OnTestLatencyDone(message);
        }
        return;
    }
    CModernSubDlg::OnTimer(nIDEvent);
}

void CTabLatencyDlg::OnDestroy()
{
    KillTimer(3);
    m_testResult.reset();
    CModernSubDlg::OnDestroy();
}

void CTabLatencyDlg::OnTestLatencyDone(const std::wstring& message)
{
    m_btnTest.EnableWindow(TRUE);
    AfxMessageBox(message.empty() ? UiLanguage::Text(IDS_BACKGROUND_FAILED).c_str() : message.c_str(), MB_ICONINFORMATION);
    RefreshList();
    AdjustColumnWidths();
}
void CTabLatencyDlg::GetData(SettingData& out) const
{
    out.latency_targets = m_targets;
}

// ====================================================================
// CTabStatusDlg —— Tab 2: 当前状态与安全检测报告
// ====================================================================
IMPLEMENT_DYNAMIC(CTabStatusDlg, CModernSubDlg)

CTabStatusDlg::CTabStatusDlg(CWnd* pParent)
    : CModernSubDlg(IDD_TAB_STATUS_INFO, pParent) {}

void CTabStatusDlg::DoDataExchange(CDataExchange* pDX)
{
    CModernSubDlg::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_STAT_DIRECT_IP,     m_stcDirectIp);
    DDX_Control(pDX, IDC_STAT_DIRECT_PLACE,  m_stcDirectPlace);
    DDX_Control(pDX, IDC_STAT_DIRECT_ISP,    m_stcDirectIsp);
    DDX_Control(pDX, IDC_STAT_DIRECT_THREAT, m_stcDirectThreat);
    DDX_Control(pDX, IDC_BTN_COPY_DIRECT,    m_btnCopyDirect);

    DDX_Control(pDX, IDC_STAT_PROXY_IP,      m_stcProxyIp);
    DDX_Control(pDX, IDC_STAT_PROXY_PLACE,   m_stcProxyPlace);
    DDX_Control(pDX, IDC_STAT_PROXY_ISP,     m_stcProxyIsp);
    DDX_Control(pDX, IDC_STAT_PROXY_THREAT,  m_stcProxyThreat);
    DDX_Control(pDX, IDC_BTN_COPY_PROXY,     m_btnCopyProxy);

    DDX_Control(pDX, IDC_STATUS_INFO_EDIT,   m_edtStatus);
    DDX_Control(pDX, IDC_BTN_COPY_ALL,       m_btnCopyAll);
}

BEGIN_MESSAGE_MAP(CTabStatusDlg, CModernSubDlg)
    ON_BN_CLICKED(IDC_BTN_COPY_DIRECT, &CTabStatusDlg::OnBnClickedCopyDirect)
    ON_BN_CLICKED(IDC_BTN_COPY_PROXY,  &CTabStatusDlg::OnBnClickedCopyProxy)
    ON_BN_CLICKED(IDC_BTN_COPY_ALL,    &CTabStatusDlg::OnBnClickedCopyAll)
END_MESSAGE_MAP()

BOOL CTabStatusDlg::OnInitDialog()
{
    CModernSubDlg::OnInitDialog();

    // 注册卡片 (DLU 坐标)
    AddCard(0, 0, 426, 56, UiLanguage::Text(IDS_CARD_DIRECT).c_str());
    AddCard(0, 62, 426, 118, UiLanguage::Text(IDS_CARD_PROXY_STATUS).c_str());
    AddCard(0, 124, 426, 212, UiLanguage::Text(IDS_CARD_REPORT).c_str());

    // 突出主操作复制全部按钮
    m_btnCopyAll.SetAccent(true);

    UpdateStatusView();
    return TRUE;
}

void CTabStatusDlg::UpdateStatusView()
{
    const auto snapshot = g_data.GetSnapshot();
    const auto& dIp = snapshot.results.m_public_ip_direct;
    const auto& dPlace = snapshot.results.m_place_direct;
    const auto& dIsp = snapshot.results.m_isp_direct;
    const auto& dThreat = snapshot.results.m_threat_direct;

    const auto& pIp = snapshot.results.m_public_ip_proxy;
    const auto& pPlace = snapshot.results.m_place_proxy;
    const auto& pIsp = snapshot.results.m_isp_proxy;
    const auto& pThreat = snapshot.results.m_threat_proxy;

    // 1. 直连卡片文本
    m_stcDirectIp.SetWindowTextW((UiLanguage::Text(IDS_PUBLIC_IP_PREFIX).c_str() + (dIp.empty() ? L"-" : dIp)).c_str());
    m_stcDirectPlace.SetWindowTextW((UiLanguage::Text(IDS_REGION_PREFIX).c_str() + (dPlace.empty() ? L"-" : dPlace)).c_str());
    m_stcDirectIsp.SetWindowTextW((UiLanguage::Text(IDS_ISP_PREFIX).c_str() + (dIsp.empty() ? L"-" : dIsp)).c_str());

    std::wstring dt = UiLanguage::Text(IDS_TYPE_PREFIX).c_str();
    if (dThreat.is_mobile) dt += UiLanguage::Text(IDS_TYPE_MOBILE).c_str();
    else if (dThreat.is_datacenter) dt += UiLanguage::Text(IDS_TYPE_DC_DETAIL).c_str();
    else dt += UiLanguage::Text(IDS_TYPE_NORMAL).c_str();

    dt += UiLanguage::Text(IDS_STATUS_PREFIX).c_str();
    if (dThreat.is_abuser) dt += UiLanguage::Text(IDS_STATUS_RISK).c_str();
    else if (dThreat.is_proxy || dThreat.is_vpn || dThreat.is_tor) dt += UiLanguage::Text(IDS_STATUS_PROXY).c_str();
    else dt += UiLanguage::Text(IDS_THREAT_CLEAN).c_str();
    m_stcDirectThreat.SetWindowTextW(dt.c_str());

    // 2. 代理卡片文本
    if (snapshot.settings.use_proxy)
    {
        m_stcProxyIp.SetWindowTextW((UiLanguage::Text(IDS_EGRESS_IP_PREFIX).c_str() + (pIp.empty() ? L"-" : pIp)).c_str());
        m_stcProxyPlace.SetWindowTextW((UiLanguage::Text(IDS_REGION_PREFIX).c_str() + (pPlace.empty() ? L"-" : pPlace)).c_str());
        m_stcProxyIsp.SetWindowTextW((UiLanguage::Text(IDS_ISP_PREFIX).c_str() + (pIsp.empty() ? L"-" : pIsp)).c_str());

        std::wstring pt = UiLanguage::Text(IDS_TYPE_PREFIX).c_str();
        if (pThreat.is_mobile) pt += UiLanguage::Text(IDS_TYPE_MOBILE).c_str();
        else if (pThreat.is_datacenter) pt += UiLanguage::Text(IDS_TYPE_DC_DETAIL).c_str();
        else pt += UiLanguage::Text(IDS_TYPE_NORMAL).c_str();

        pt += UiLanguage::Text(IDS_STATUS_PREFIX).c_str();
        if (pThreat.is_abuser) pt += UiLanguage::Text(IDS_STATUS_RISK).c_str();
        else if (pThreat.is_proxy || pThreat.is_vpn || pThreat.is_tor) pt += UiLanguage::Text(IDS_STATUS_PROXY).c_str();
        else pt += UiLanguage::Text(IDS_THREAT_CLEAN).c_str();
        m_stcProxyThreat.SetWindowTextW(pt.c_str());
    }
    else
    {
        m_stcProxyIp.SetWindowTextW(UiLanguage::Text(IDS_PROXY_DISABLED).c_str());
        m_stcProxyPlace.SetWindowTextW(UiLanguage::Text(IDS_REGION_EMPTY).c_str());
        m_stcProxyIsp.SetWindowTextW(UiLanguage::Text(IDS_ISP_EMPTY).c_str());
        m_stcProxyThreat.SetWindowTextW(UiLanguage::Text(IDS_TYPE_EMPTY).c_str());
    }

    // 3. 完整诊断报告展示框
    std::wstring report = snapshot.results.m_tooltip;
    std::wstring crlfReport;
    crlfReport.reserve(report.size() + 64);
    for (size_t i = 0; i < report.size(); ++i)
    {
        if (report[i] == L'\n' && (i == 0 || report[i - 1] != L'\r'))
        {
            crlfReport += L"\r\n";
        }
        else
        {
            crlfReport += report[i];
        }
    }
    m_edtStatus.SetWindowTextW(crlfReport.c_str());
}

void CTabStatusDlg::CopyToClipboard(const std::wstring& text)
{
    if (text.empty()) return;
    if (!OpenClipboard()) return;
    EmptyClipboard();

    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
    if (hGlob)
    {
        wchar_t* pBuf = (wchar_t*)GlobalLock(hGlob);
        if (pBuf)
        {
            wcscpy_s(pBuf, text.size() + 1, text.c_str());
            GlobalUnlock(hGlob);
            SetClipboardData(CF_UNICODETEXT, hGlob);
        }
    }
    CloseClipboard();
    AfxMessageBox(UiLanguage::Text(IDS_COPIED).c_str(), MB_ICONINFORMATION);
}

void CTabStatusDlg::OnBnClickedCopyDirect()
{
    const auto snapshot = g_data.GetSnapshot();
    const auto& ip = snapshot.results.m_public_ip_direct;
    if (ip.empty())
    {
        AfxMessageBox(UiLanguage::Text(IDS_NO_DIRECT_IP).c_str(), MB_ICONWARNING);
        return;
    }
    std::wstring s = UiLanguage::Text(IDS_COPY_DIRECT_PREFIX).c_str() + ip;
    if (!snapshot.results.m_place_direct.empty())
        s += L" [" + snapshot.results.m_place_direct + L"]";
    if (!snapshot.results.m_isp_direct.empty())
        s += L" " + snapshot.results.m_isp_direct;
    CopyToClipboard(s);
}

void CTabStatusDlg::OnBnClickedCopyProxy()
{
    const auto snapshot = g_data.GetSnapshot();
    const auto& ip = snapshot.results.m_public_ip_proxy;
    if (ip.empty())
    {
        AfxMessageBox(UiLanguage::Text(IDS_NO_PROXY_IP).c_str(), MB_ICONWARNING);
        return;
    }
    std::wstring s = UiLanguage::Text(IDS_COPY_PROXY_PREFIX).c_str() + ip;
    if (!snapshot.results.m_place_proxy.empty())
        s += L" [" + snapshot.results.m_place_proxy + L"]";
    if (!snapshot.results.m_isp_proxy.empty())
        s += L" " + snapshot.results.m_isp_proxy;
    CopyToClipboard(s);
}

void CTabStatusDlg::OnBnClickedCopyAll()
{
    const auto snapshot = g_data.GetSnapshot();
    CString s;
    m_edtStatus.GetWindowTextW(s);
    if (s.IsEmpty())
    {
        s = snapshot.results.m_tooltip.c_str();
    }
    CopyToClipboard(s.GetString());
}

// ====================================================================
// COptionsDlg —— 主对话框容器 (现代 Fluent 顶栏分段导航 + 柔和底色)
// ====================================================================
IMPLEMENT_DYNAMIC(COptionsDlg, CDialog)

COptionsDlg::COptionsDlg(CWnd* pParent)
    : CDialog()
    , m_pageProxyApi(this)
    , m_pageLatency(this)
    , m_pageStatus(this)
{
    InitModalIndirect(UiLanguage::Dialog(IDD_OPTIONS_DIALOG), pParent);
    m_pages[0] = &m_pageProxyApi;
    m_pages[1] = &m_pageLatency;
    m_pages[2] = &m_pageStatus;
    m_brWindowBg.CreateSolidBrush(FluentTheme::WindowBg);
}

void COptionsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
    ON_WM_ERASEBKGND()
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // 1. 初始化顶部现代分段胶囊栏
    m_tabBar.SubclassDlgItem(IDC_TAB_MAIN, this);
    m_tabBar.AddTab(UiLanguage::Text(IDS_TAB_NETWORK).c_str());
    m_tabBar.AddTab(UiLanguage::Text(IDS_TAB_TARGETS).c_str());
    m_tabBar.AddTab(UiLanguage::Text(IDS_TAB_STATUS).c_str());
    m_tabBar.SetOnSelChanged([this](int sel) {
        SwitchToPage(sel);
    });

    // 2. 初始化底部确定/取消按钮为现代圆角卡片按钮
    m_btnOK.SubclassDlgItem(IDOK, this);
    m_btnOK.SetAccent(true); // 强调蓝色主操作按钮

    m_btnCancel.SubclassDlgItem(IDCANCEL, this);

    // 3. 创建三大子对话框卡片页
    m_pageProxyApi.Create(IDD_TAB_PROXY_API, this);
    m_pageLatency.Create(IDD_TAB_LATENCY_LIST, this);
    m_pageStatus.Create(IDD_TAB_STATUS_INFO, this);

    PositionPages();
    SwitchToPage(0);

    return TRUE;
}

BOOL COptionsDlg::OnEraseBkgnd(CDC* pDC)
{
    CRect rc;
    GetClientRect(&rc);
    pDC->FillSolidRect(&rc, FluentTheme::WindowBg);
    return TRUE;
}

HBRUSH COptionsDlg::OnCtlColor(CDC* /*pDC*/, CWnd* /*pWnd*/, UINT nCtlColor)
{
    if (nCtlColor == CTLCOLOR_DLG || nCtlColor == CTLCOLOR_STATIC)
    {
        return (HBRUSH)m_brWindowBg.GetSafeHandle();
    }
    return (HBRUSH)m_brWindowBg.GetSafeHandle();
}

void COptionsDlg::PositionPages()
{
    CRect barRc;
    m_tabBar.GetWindowRect(&barRc);
    ScreenToClient(&barRc);

    CRect okRc;
    m_btnOK.GetWindowRect(&okRc);
    ScreenToClient(&okRc);

    // 子页面放置在分段胶囊栏与确定按钮之间，并保持呼吸感外边距
    CRect pageRect(barRc.left, barRc.bottom + 8, barRc.right, okRc.top - 8);

    for (auto* p : m_pages)
    {
        if (p && p->GetSafeHwnd())
        {
            p->MoveWindow(pageRect);
        }
    }
    // 让延迟列表根据定位后的真实窗口宽度重新自适应
    m_pageLatency.AdjustColumnWidths();
}

void COptionsDlg::SwitchToPage(int idx)
{
    for (int i = 0; i < 3; ++i)
    {
        if (m_pages[i] && m_pages[i]->GetSafeHwnd())
            m_pages[i]->ShowWindow(i == idx ? SW_SHOW : SW_HIDE);
    }
    m_curPage = idx;

    if (idx == 1)
    {
        m_pageLatency.AdjustColumnWidths();
    }
    else if (idx == 2)
    {
        m_pageStatus.UpdateStatusView();
    }
}

void COptionsDlg::OnOK()
{
    SettingData d = g_data.GetSettings();
    m_pageProxyApi.GetData(d);
    m_pageLatency.GetData(d);
    g_data.SetSettings(std::move(d));

    g_data.SaveConfig();
    g_data.ReformatTaskbarTexts();

    CDialog::OnOK();
}
