#include "pch.h"
#include "OptionsDlg.h"

// ====================================================================
// CTabProxyApiDlg —— Tab 0: 常规 / 网络 & 代理
// ====================================================================
IMPLEMENT_DYNAMIC(CTabProxyApiDlg, CDialog)

CTabProxyApiDlg::CTabProxyApiDlg(CWnd* pParent)
    : CDialog(IDD_TAB_PROXY_API, pParent) {}

void CTabProxyApiDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_USE_PROXY_CHECK,        m_chkUseProxy);
    DDX_Control(pDX, IDC_PROXY_EDIT,             m_edtProxy);
    DDX_Control(pDX, IDC_API_DOMESTIC_EDIT,      m_edtDomesticApi);
    DDX_Control(pDX, IDC_API_FOREIGN_EDIT,       m_edtForeignApi);
    DDX_Control(pDX, IDC_REFRESH_INTERVAL_EDIT,  m_edtInterval);
    DDX_Control(pDX, IDC_REFRESH_NOW_BUTTON,     m_btnRefresh);
    DDX_Control(pDX, IDC_STATUS_STATIC,          m_stcStatus);
}

BEGIN_MESSAGE_MAP(CTabProxyApiDlg, CDialog)
    ON_BN_CLICKED(IDC_USE_PROXY_CHECK,    &CTabProxyApiDlg::OnBnClickedUseProxy)
    ON_BN_CLICKED(IDC_REFRESH_NOW_BUTTON, &CTabProxyApiDlg::OnBnClickedRefreshNow)
    ON_WM_TIMER()
END_MESSAGE_MAP()

BOOL CTabProxyApiDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    const SettingData& d = g_data.m_setting_data;
    m_chkUseProxy.SetCheck(d.use_proxy ? BST_CHECKED : BST_UNCHECKED);
    m_edtProxy.SetWindowTextW(d.proxy_address.c_str());
    m_edtDomesticApi.SetWindowTextW(d.api_domestic_url.c_str());
    m_edtForeignApi.SetWindowTextW(d.api_foreign_url.c_str());

    wchar_t buf[16]{};
    swprintf_s(buf, L"%d", d.refresh_interval_sec);
    m_edtInterval.SetWindowTextW(buf);

    UpdateProxyEditState();
    return TRUE;
}

void CTabProxyApiDlg::UpdateProxyEditState()
{
    BOOL useProxy = (m_chkUseProxy.GetCheck() == BST_CHECKED);
    // 代理地址框：仅启用代理时可用
    GetDlgItem(IDC_PROXY_LABEL)->EnableWindow(useProxy);
    m_edtProxy.EnableWindow(useProxy);
    // 代理 API：仅启用代理时可用
    GetDlgItem(IDC_API_FOREIGN_LABEL)->EnableWindow(useProxy);
    m_edtForeignApi.EnableWindow(useProxy);
}

void CTabProxyApiDlg::OnBnClickedUseProxy()
{
    UpdateProxyEditState();
}

void CTabProxyApiDlg::OnBnClickedRefreshNow()
{
    g_data.ForceUpdateAsync();
    m_btnRefresh.EnableWindow(FALSE);
    m_stcStatus.SetWindowTextW(L"正在刷新…");
    // 每 500ms 轮询一次刷新状态（原子读取 m_is_updating，无锁竞争）
    SetTimer(1, 500, nullptr);
}

void CTabProxyApiDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1 && !g_data.IsUpdating())
    {
        KillTimer(1);
        m_btnRefresh.EnableWindow(TRUE);
        m_stcStatus.SetWindowTextW(L"刷新完成！");
    }
    CDialog::OnTimer(nIDEvent);
}

void CTabProxyApiDlg::GetData(SettingData& out) const
{
    out.use_proxy = (m_chkUseProxy.GetCheck() == BST_CHECKED);

    CString s;
    m_edtProxy.GetWindowTextW(s);       out.proxy_address = s.GetString();
    m_edtDomesticApi.GetWindowTextW(s); out.api_domestic_url = s.GetString();
    m_edtForeignApi.GetWindowTextW(s);  out.api_foreign_url  = s.GetString();

    m_edtInterval.GetWindowTextW(s);
    int secs = _wtoi(s.GetString());
    out.refresh_interval_sec = (secs >= 5) ? secs : 5;
}

// ====================================================================
// CTabLatencyDlg —— Tab 1: 延迟目标（3 行固定 Edit 对）
// ====================================================================
IMPLEMENT_DYNAMIC(CTabLatencyDlg, CDialog)

CTabLatencyDlg::CTabLatencyDlg(CWnd* pParent)
    : CDialog(IDD_TAB_LATENCY_LIST, pParent) {}

void CTabLatencyDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LAT_NAME_0, m_edtName[0]);
    DDX_Control(pDX, IDC_LAT_URL_0,  m_edtUrl[0]);
    DDX_Control(pDX, IDC_LAT_DEL_0,  m_btnDel[0]);
    DDX_Control(pDX, IDC_LAT_NAME_1, m_edtName[1]);
    DDX_Control(pDX, IDC_LAT_URL_1,  m_edtUrl[1]);
    DDX_Control(pDX, IDC_LAT_DEL_1,  m_btnDel[1]);
    DDX_Control(pDX, IDC_LAT_NAME_2, m_edtName[2]);
    DDX_Control(pDX, IDC_LAT_URL_2,  m_edtUrl[2]);
    DDX_Control(pDX, IDC_LAT_DEL_2,  m_btnDel[2]);
}

BEGIN_MESSAGE_MAP(CTabLatencyDlg, CDialog)
    ON_BN_CLICKED(IDC_LAT_DEL_0, &CTabLatencyDlg::OnBnClickedDel0)
    ON_BN_CLICKED(IDC_LAT_DEL_1, &CTabLatencyDlg::OnBnClickedDel1)
    ON_BN_CLICKED(IDC_LAT_DEL_2, &CTabLatencyDlg::OnBnClickedDel2)
END_MESSAGE_MAP()

BOOL CTabLatencyDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    const auto& targets = g_data.m_setting_data.latency_targets;
    for (int i = 0; i < 3; ++i)
    {
        if (i < (int)targets.size())
        {
            m_edtName[i].SetWindowTextW(targets[i].name.c_str());
            m_edtUrl[i].SetWindowTextW(targets[i].url.c_str());
        }
        // 空行保持空白，提示文字由 RC 静态文本提供
    }
    return TRUE;
}

void CTabLatencyDlg::DeleteRow(int row)
{
    // 将 row+1, row+2 依次上移，最后一行清空
    for (int i = row; i < 2; ++i)
    {
        CString name, url;
        m_edtName[i + 1].GetWindowTextW(name);
        m_edtUrl[i + 1].GetWindowTextW(url);
        m_edtName[i].SetWindowTextW(name);
        m_edtUrl[i].SetWindowTextW(url);
    }
    m_edtName[2].SetWindowTextW(L"");
    m_edtUrl[2].SetWindowTextW(L"");
}

void CTabLatencyDlg::OnBnClickedDel0() { DeleteRow(0); }
void CTabLatencyDlg::OnBnClickedDel1() { DeleteRow(1); }
void CTabLatencyDlg::OnBnClickedDel2() { DeleteRow(2); }

void CTabLatencyDlg::GetData(SettingData& out) const
{
    out.latency_targets.clear();
    for (int i = 0; i < 3; ++i)
    {
        CString name, url;
        m_edtName[i].GetWindowTextW(name);
        m_edtUrl[i].GetWindowTextW(url);
        // 地址为空的行忽略；名称留空时用地址代替
        if (!url.IsEmpty())
        {
            std::wstring n = name.IsEmpty() ? std::wstring(url.GetString()) : std::wstring(name.GetString());
            out.latency_targets.push_back({ n, std::wstring(url.GetString()) });
        }
    }
}

// ====================================================================
// CTabStatusDlg —— Tab 2: 当前状态（只读）
// ====================================================================
IMPLEMENT_DYNAMIC(CTabStatusDlg, CDialog)

CTabStatusDlg::CTabStatusDlg(CWnd* pParent)
    : CDialog(IDD_TAB_STATUS_INFO, pParent) {}

void CTabStatusDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_STATUS_INFO_EDIT, m_edtStatus);
}

BEGIN_MESSAGE_MAP(CTabStatusDlg, CDialog)
END_MESSAGE_MAP()

BOOL CTabStatusDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // tooltip 内部使用 \n 换行，Windows Edit 控件需要 \r\n
    // 这里做一次转换再设置
    std::wstring tip = g_data.m_tooltip;
    std::wstring converted;
    converted.reserve(tip.size() + 32);
    for (size_t i = 0; i < tip.size(); ++i)
    {
        if (tip[i] == L'\n' && (i == 0 || tip[i - 1] != L'\r'))
            converted += L'\r';
        converted += tip[i];
    }
    m_edtStatus.SetWindowTextW(
        converted.empty()
            ? L"（尚无数据，请等待后台刷新完成）"
            : converted.c_str());
    return TRUE;
}

// ====================================================================
// COptionsDlg —— 主对话框
// ====================================================================
IMPLEMENT_DYNAMIC(COptionsDlg, CDialog)

COptionsDlg::COptionsDlg(CWnd* pParent)
    : CDialog(IDD_OPTIONS_DIALOG, pParent)
    , m_pageProxyApi(this)
    , m_pageLatency(this)
    , m_pageStatus(this)
{
    m_pages[0] = &m_pageProxyApi;
    m_pages[1] = &m_pageLatency;
    m_pages[2] = &m_pageStatus;
}

void COptionsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TAB_MAIN, m_tabCtrl);
}

BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_MAIN, &COptionsDlg::OnTcnSelchangeTabMain)
END_MESSAGE_MAP()

BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // 添加 Tab 标签
    m_tabCtrl.InsertItem(0, L"常规 / 网络");
    m_tabCtrl.InsertItem(1, L"延迟目标");
    m_tabCtrl.InsertItem(2, L"当前状态");

    // 创建三个子页（无模式子对话框）
    m_pageProxyApi.Create(IDD_TAB_PROXY_API, this);
    m_pageLatency.Create(IDD_TAB_LATENCY_LIST, this);
    m_pageStatus.Create(IDD_TAB_STATUS_INFO, this);

    // 计算 Tab 内容区域并定位子页
    PositionPages();

    // 默认显示第一页
    SwitchToPage(0);

    return TRUE;
}

void COptionsDlg::PositionPages()
{
    // 获取 Tab 控件的客户区矩形，减去 Tab 标签头高度得到内容区
    CRect tabRect;
    m_tabCtrl.GetWindowRect(&tabRect);
    ScreenToClient(&tabRect);
    m_tabCtrl.AdjustRect(FALSE, &tabRect); // 得到内容区（去掉标签头）
    tabRect.DeflateRect(1, 1);             // 留 1px 边距防止子页遮住边框

    for (auto* p : m_pages)
    {
        if (p && p->GetSafeHwnd())
            p->MoveWindow(tabRect);
    }
}

void COptionsDlg::SwitchToPage(int idx)
{
    for (int i = 0; i < 3; ++i)
    {
        if (m_pages[i] && m_pages[i]->GetSafeHwnd())
            m_pages[i]->ShowWindow(i == idx ? SW_SHOW : SW_HIDE);
    }
    m_curPage = idx;
}

void COptionsDlg::OnTcnSelchangeTabMain(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    SwitchToPage(m_tabCtrl.GetCurSel());
    *pResult = 0;
}

void COptionsDlg::OnOK()
{
    // 收集各子页数据并写回单例
    SettingData& d = g_data.m_setting_data;
    m_pageProxyApi.GetData(d);
    m_pageLatency.GetData(d);

    // 持久化到 INI
    g_data.SaveConfig();

    CDialog::OnOK();
}
