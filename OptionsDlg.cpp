#include "pch.h"
#include "OptionsDlg.h"
#include "PluginInterface.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// =========================================================
// CProxyApiPage
// =========================================================
IMPLEMENT_DYNAMIC(CProxyApiPage, CDialog)

CProxyApiPage::CProxyApiPage(CWnd* pParent /*=nullptr*/)
    : CDialog(IDD_TAB_PROXY_API, pParent)
{
}

CProxyApiPage::~CProxyApiPage() {}

void CProxyApiPage::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CProxyApiPage, CDialog)
    ON_BN_CLICKED(IDC_USE_PROXY_CHECK, &CProxyApiPage::OnBnClickedUseProxyCheck)
    ON_BN_CLICKED(IDC_REFRESH_NOW_BUTTON, &CProxyApiPage::OnBnClickedRefreshNow)
END_MESSAGE_MAP()

BOOL CProxyApiPage::OnInitDialog()
{
    CDialog::OnInitDialog();

    CheckDlgButton(IDC_USE_PROXY_CHECK, m_data.use_proxy ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemText(IDC_PROXY_EDIT, m_data.proxy_address.c_str());
    SetDlgItemText(IDC_API_DOMESTIC_EDIT, m_data.api_domestic_url.c_str());
    SetDlgItemText(IDC_API_FOREIGN_EDIT, m_data.api_foreign_url.c_str());
    SetDlgItemInt(IDC_REFRESH_INTERVAL_EDIT, m_data.refresh_interval_sec, FALSE);

    OnBnClickedUseProxyCheck(); // 更新控件可用性
    return TRUE;
}

void CProxyApiPage::OnBnClickedUseProxyCheck()
{
    BOOL useProxy = (IsDlgButtonChecked(IDC_USE_PROXY_CHECK) == BST_CHECKED);
    CWnd* pEdit = GetDlgItem(IDC_PROXY_EDIT);
    if (pEdit)
    {
        pEdit->EnableWindow(useProxy);
    }
}

void CProxyApiPage::OnBnClickedRefreshNow()
{
    // 即时生效并刷新
    // 我们先把当前输入的数据更新到临时结构里，但不破坏取消操作（由于只是触发一次刷新，也可以直接调起任务）
    // 为了简单，我们发送自定义消息给主进程或者直接设置一个标志位
    // 不过在此简单调用：
    CDataManager::Instance().UpdateIpInfoNow();
    SetDlgItemText(IDC_STATUS_STATIC, L"已触发后台刷新，请稍后查看状态页...");
}

// =========================================================
// CLatencyEditDlg
// =========================================================
IMPLEMENT_DYNAMIC(CLatencyEditDlg, CDialog)
CLatencyEditDlg::CLatencyEditDlg(CWnd* pParent) : CDialog(IDD_LATENCY_EDIT_DIALOG, pParent) {}
CLatencyEditDlg::~CLatencyEditDlg() {}

void CLatencyEditDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_LATENCY_NAME_EDIT, m_strName);
    DDX_Text(pDX, IDC_LATENCY_URL_EDIT, m_strUrl);
}

BEGIN_MESSAGE_MAP(CLatencyEditDlg, CDialog)
END_MESSAGE_MAP()


// =========================================================
// CLatencyListPage
// =========================================================
IMPLEMENT_DYNAMIC(CLatencyListPage, CDialog)

CLatencyListPage::CLatencyListPage(CWnd* pParent)
    : CDialog(IDD_TAB_LATENCY_LIST, pParent)
{
}

CLatencyListPage::~CLatencyListPage() {}

void CLatencyListPage::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LATENCY_LIST, m_list);
}

BEGIN_MESSAGE_MAP(CLatencyListPage, CDialog)
    ON_BN_CLICKED(IDC_LATENCY_ADD_BUTTON, &CLatencyListPage::OnBnClickedAdd)
    ON_BN_CLICKED(IDC_LATENCY_EDIT_BUTTON, &CLatencyListPage::OnBnClickedEdit)
    ON_BN_CLICKED(IDC_LATENCY_DEL_BUTTON, &CLatencyListPage::OnBnClickedDel)
END_MESSAGE_MAP()

BOOL CLatencyListPage::OnInitDialog()
{
    CDialog::OnInitDialog();

    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_list.InsertColumn(0, L"别名 (Name)", LVCFMT_LEFT, 100);
    m_list.InsertColumn(1, L"测试地址 (URL/Host)", LVCFMT_LEFT, 200);

    RefreshList();
    return TRUE;
}

void CLatencyListPage::RefreshList()
{
    m_list.DeleteAllItems();
    for (int i = 0; i < (int)m_targets.size(); ++i)
    {
        m_list.InsertItem(i, m_targets[i].name.c_str());
        m_list.SetItemText(i, 1, m_targets[i].url.c_str());
    }
}

void CLatencyListPage::OnBnClickedAdd()
{
    CLatencyEditDlg dlg;
    if (dlg.DoModal() == IDOK)
    {
        if (!dlg.m_strName.IsEmpty() && !dlg.m_strUrl.IsEmpty())
        {
            LatencyTarget t; t.name = dlg.m_strName; t.url = dlg.m_strUrl;
            m_targets.push_back(t);
            RefreshList();
        }
    }
}

void CLatencyListPage::OnBnClickedEdit()
{
    POSITION pos = m_list.GetFirstSelectedItemPosition();
    if (!pos) return;
    int idx = m_list.GetNextSelectedItem(pos);

    CLatencyEditDlg dlg;
    dlg.m_strName = m_targets[idx].name.c_str();
    dlg.m_strUrl = m_targets[idx].url.c_str();

    if (dlg.DoModal() == IDOK)
    {
        if (!dlg.m_strName.IsEmpty() && !dlg.m_strUrl.IsEmpty())
        {
            m_targets[idx].name = dlg.m_strName;
            m_targets[idx].url = dlg.m_strUrl;
            RefreshList();
        }
    }
}

void CLatencyListPage::OnBnClickedDel()
{
    POSITION pos = m_list.GetFirstSelectedItemPosition();
    if (!pos) return;
    int idx = m_list.GetNextSelectedItem(pos);
    m_targets.erase(m_targets.begin() + idx);
    RefreshList();
}


// =========================================================
// CStatusInfoPage
// =========================================================
IMPLEMENT_DYNAMIC(CStatusInfoPage, CDialog)

CStatusInfoPage::CStatusInfoPage(CWnd* pParent)
    : CDialog(IDD_TAB_STATUS_INFO, pParent)
{
}

CStatusInfoPage::~CStatusInfoPage() {}

void CStatusInfoPage::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CStatusInfoPage, CDialog)
END_MESSAGE_MAP()

BOOL CStatusInfoPage::OnInitDialog()
{
    CDialog::OnInitDialog();
    RefreshReport();
    return TRUE;
}

void CStatusInfoPage::RefreshReport()
{
    // 抓取当前最新的 Tooltip 以及延迟信息作为报告
    std::wstring report = L"【安全状态与网络类型报告】\r\n";
    report += CDataManager::Instance().m_tooltip;
    report += L"\r\n\r\n【连通性延迟报告】\r\n";
    
    auto& latencies = CDataManager::Instance().m_latency_results;
    for (const auto& lr : latencies)
    {
        wchar_t buf[256];
        swprintf_s(buf, L"%s - 直连: %.1f ms | 代理: %.1f ms\r\n", lr.name.c_str(), lr.direct_ms, lr.proxy_ms);
        report += buf;
    }

    report += L"\r\n(提示：此状态为后台异步刷新数据。如果数据为空，请先在常规设置页面点击“立即刷新”)";

    if (GetSafeHwnd())
    {
        SetDlgItemText(IDC_STATUS_INFO_EDIT, report.c_str());
    }
}


// =========================================================
// COptionsDlg
// =========================================================
IMPLEMENT_DYNAMIC(COptionsDlg, CDialog)

COptionsDlg::COptionsDlg(CWnd* pParent /*=nullptr*/)
    : CDialog(IDD_OPTIONS_DIALOG, pParent)
{
}

COptionsDlg::~COptionsDlg()
{
}

void COptionsDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TAB_MAIN, m_tab);
}

BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_MAIN, &COptionsDlg::OnTcnSelchangeTabMain)
END_MESSAGE_MAP()

BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // 1. 拷贝现有数据给子页
    m_pageProxyApi.m_data = CDataManager::Instance().m_setting_data;
    m_pageLatencyList.m_targets = CDataManager::Instance().m_setting_data.latency_targets;

    // 2. 初始化 Tab
    m_tab.InsertItem(0, L"网络与接口 (API)");
    m_tab.InsertItem(1, L"延迟测速列表");
    m_tab.InsertItem(2, L"当前状态");

    // 3. 创建子对话框
    m_pageProxyApi.Create(IDD_TAB_PROXY_API, &m_tab);
    m_pageLatencyList.Create(IDD_TAB_LATENCY_LIST, &m_tab);
    m_pageStatusInfo.Create(IDD_TAB_STATUS_INFO, &m_tab);

    m_pages.push_back(&m_pageProxyApi);
    m_pages.push_back(&m_pageLatencyList);
    m_pages.push_back(&m_pageStatusInfo);

    // 4. 调整子页面的位置到 Tab 的客户区
    CRect rectTab;
    m_tab.GetClientRect(&rectTab);
    m_tab.AdjustRect(FALSE, &rectTab);
    
    // 留点边距
    rectTab.DeflateRect(2, 2);

    for (auto pPage : m_pages)
    {
        pPage->MoveWindow(&rectTab);
        pPage->ShowWindow(SW_HIDE);
    }

    // 默认显示第一个页签
    m_pages[0]->ShowWindow(SW_SHOW);
    m_tab.SetCurSel(0);

    return TRUE;
}

void COptionsDlg::OnTcnSelchangeTabMain(NMHDR *pNMHDR, LRESULT *pResult)
{
    int sel = m_tab.GetCurSel();
    for (int i = 0; i < (int)m_pages.size(); ++i)
    {
        if (i == sel)
            m_pages[i]->ShowWindow(SW_SHOW);
        else
            m_pages[i]->ShowWindow(SW_HIDE);
    }
    
    // 如果切到了状态页，强制刷新一次显示
    if (sel == 2)
    {
        m_pageStatusInfo.RefreshReport(); // 重新加载报告文本
    }

    *pResult = 0;
}

void COptionsDlg::OnOK()
{
    // 从子页面收集数据
    CString strVal;

    SettingData sd = m_pageProxyApi.m_data;
    sd.use_proxy = (m_pageProxyApi.IsDlgButtonChecked(IDC_USE_PROXY_CHECK) == BST_CHECKED);
    
    m_pageProxyApi.GetDlgItemText(IDC_PROXY_EDIT, strVal);
    sd.proxy_address = strVal;

    m_pageProxyApi.GetDlgItemText(IDC_API_DOMESTIC_EDIT, strVal);
    sd.api_domestic_url = strVal;

    m_pageProxyApi.GetDlgItemText(IDC_API_FOREIGN_EDIT, strVal);
    sd.api_foreign_url = strVal;

    sd.refresh_interval_sec = m_pageProxyApi.GetDlgItemInt(IDC_REFRESH_INTERVAL_EDIT, NULL, FALSE);
    if (sd.refresh_interval_sec < 5) sd.refresh_interval_sec = 5;

    // 写回数据
    CDataManager::Instance().m_setting_data = sd;
    CDataManager::Instance().m_setting_data.latency_targets = m_pageLatencyList.m_targets;
    CDataManager::Instance().SaveConfig();

    CDialog::OnOK();
}
