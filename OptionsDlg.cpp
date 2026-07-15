#include "pch.h"
#include "OptionsDlg.h"
#include "resource.h"
#include <thread>

// ==========================================
// CLatencyEditDlg: 弹出式延迟测试目标编辑框
// ==========================================
class CLatencyEditDlg : public CDialog
{
public:
    CLatencyEditDlg(CWnd* pParent = nullptr) : CDialog(IDD_LATENCY_EDIT_DIALOG, pParent) {}

    std::wstring m_name;
    std::wstring m_url;

protected:
    virtual void DoDataExchange(CDataExchange* pDX)
    {
        CDialog::DoDataExchange(pDX);
        DDX_Text(pDX, IDC_LATENCY_NAME_EDIT, m_name_str);
        DDX_Text(pDX, IDC_LATENCY_URL_EDIT, m_url_str);
    }

    virtual BOOL OnInitDialog()
    {
        m_name_str = m_name.c_str();
        m_url_str = m_url.c_str();
        CDialog::OnInitDialog();

        // 统一对话框字体
        if (CFont* f = GetParent() ? GetParent()->GetFont() : nullptr)
        {
            const int ids[] = { IDC_STATIC, IDC_LATENCY_NAME_EDIT, IDC_LATENCY_URL_EDIT, IDOK, IDCANCEL };
            for (int id : ids)
            {
                if (auto p = GetDlgItem(id))
                    p->SetFont(f);
            }
        }
        return TRUE;
    }

    virtual void OnOK()
    {
        UpdateData(TRUE);
        m_name = m_name_str.GetString();
        m_url = m_url_str.GetString();
        CDialog::OnOK();
    }

private:
    CString m_name_str;
    CString m_url_str;
};

// ==========================================
// CPageProxyApi: 代理与 API 常规设置子页面
// ==========================================
BEGIN_MESSAGE_MAP(CPageProxyApi, CDialog)
    ON_BN_CLICKED(IDC_REFRESH_NOW_BUTTON, &CPageProxyApi::OnBnClickedRefreshNow)
END_MESSAGE_MAP()

CPageProxyApi::CPageProxyApi(COptionsDlg* pParent)
    : CDialog(IDD_TAB_PROXY_API, (CWnd*)pParent), m_pParent(pParent)
{
}

BOOL CPageProxyApi::OnInitDialog()
{
    CDialog::OnInitDialog();
    if (CFont* f = m_pParent->GetFont())
    {
        const int ids[] = { IDC_USE_PROXY_CHECK, IDC_GRP_PROXY, IDC_PROXY_LABEL, IDC_PROXY_EDIT,
                            IDC_GRP_LATENCY, IDC_API_DOMESTIC_LABEL, IDC_API_DOMESTIC_EDIT,
                            IDC_API_FOREIGN_LABEL, IDC_API_FOREIGN_EDIT,
                            IDC_REFRESH_INTERVAL_LABEL, IDC_REFRESH_INTERVAL_EDIT, IDC_REFRESH_NOW_BUTTON,
                            IDC_STATUS_STATIC };
        for (int id : ids)
        {
            if (auto p = GetDlgItem(id))
                p->SetFont(f);
        }
    }
    return TRUE;
}

void CPageProxyApi::OnBnClickedRefreshNow()
{
    m_pParent->OnBnClickedRefreshNow();
}

// ==========================================
// CPageLatencyList: 延迟列表测试目标子页面
// ==========================================
BEGIN_MESSAGE_MAP(CPageLatencyList, CDialog)
    ON_BN_CLICKED(IDC_LATENCY_ADD_BUTTON, &CPageLatencyList::OnBnClickedLatencyAdd)
    ON_BN_CLICKED(IDC_LATENCY_EDIT_BUTTON, &CPageLatencyList::OnBnClickedLatencyEdit)
    ON_BN_CLICKED(IDC_LATENCY_DEL_BUTTON, &CPageLatencyList::OnBnClickedLatencyDel)
END_MESSAGE_MAP()

CPageLatencyList::CPageLatencyList(COptionsDlg* pParent)
    : CDialog(IDD_TAB_LATENCY_LIST, (CWnd*)pParent), m_pParent(pParent)
{
}

BOOL CPageLatencyList::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_latencyList.SubclassDlgItem(IDC_LATENCY_LIST, this);
    m_latencyList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_latencyList.InsertColumn(0, L"目标名称", LVCFMT_LEFT, 100);
    m_latencyList.InsertColumn(1, L"测试地址/URL", LVCFMT_LEFT, 210);

    if (CFont* f = m_pParent->GetFont())
    {
        const int ids[] = { IDC_LATENCY_LIST, IDC_LATENCY_ADD_BUTTON, IDC_LATENCY_EDIT_BUTTON, IDC_LATENCY_DEL_BUTTON };
        for (int id : ids)
        {
            if (auto p = GetDlgItem(id))
                p->SetFont(f);
        }
    }
    return TRUE;
}

void CPageLatencyList::OnBnClickedLatencyAdd() { m_pParent->OnBnClickedLatencyAdd(); }
void CPageLatencyList::OnBnClickedLatencyEdit() { m_pParent->OnBnClickedLatencyEdit(); }
void CPageLatencyList::OnBnClickedLatencyDel() { m_pParent->OnBnClickedLatencyDel(); }

// ==========================================
// CPageStatusInfo: 当前状态子页面
// ==========================================
BEGIN_MESSAGE_MAP(CPageStatusInfo, CDialog)
END_MESSAGE_MAP()

CPageStatusInfo::CPageStatusInfo(COptionsDlg* pParent)
    : CDialog(IDD_TAB_STATUS_INFO, (CWnd*)pParent), m_pParent(pParent)
{
}

BOOL CPageStatusInfo::OnInitDialog()
{
    CDialog::OnInitDialog();
    if (CFont* f = m_pParent->GetFont())
    {
        if (auto p = GetDlgItem(IDC_STATUS_INFO_EDIT))
            p->SetFont(f);
    }
    return TRUE;
}


// ==========================================
// COptionsDlg: 配置主对话框类实现
// ==========================================
IMPLEMENT_DYNAMIC(COptionsDlg, CDialog)

COptionsDlg::COptionsDlg(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_OPTIONS_DIALOG, pParent),
      m_page1(this), m_page2(this), m_page3(this)
{
}

COptionsDlg::~COptionsDlg()
{
}

void COptionsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
    ON_BN_CLICKED(IDC_SHOW_SECOND_CHECK, &COptionsDlg::OnBnClickedShowSecondCheck)
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_MAIN, &COptionsDlg::OnTcnSelchangeTabMain)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_MESSAGE(WM_IP_UPDATE_DONE, &COptionsDlg::OnIpUpdateDone)
END_MESSAGE_MAP()

BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // 1. 系统默认字体统一应用到主按钮
    if (CFont* f = GetFont())
    {
        if (auto p = GetDlgItem(IDOK)) p->SetFont(f);
        if (auto p = GetDlgItem(IDCANCEL)) p->SetFont(f);
        if (auto p = GetDlgItem(IDC_TAB_MAIN)) p->SetFont(f);
    }

    // 2. 绑定 Tab 控件及插入页签
    m_tabCtrl.SubclassDlgItem(IDC_TAB_MAIN, this);
    m_tabCtrl.InsertItem(0, L"常规设置");
    m_tabCtrl.InsertItem(1, L"延迟目标");
    m_tabCtrl.InsertItem(2, L"当前状态");

    // 3. 创建子页面页签窗口
    m_page1.Create(IDD_TAB_PROXY_API, this);
    m_page2.Create(IDD_TAB_LATENCY_LIST, this);
    m_page3.Create(IDD_TAB_STATUS_INFO, this);

    // 4. 填充子页面 1 的常规参数
    m_page1.CheckDlgButton(IDC_USE_PROXY_CHECK, m_data.use_proxy ? BST_CHECKED : BST_UNCHECKED);
    m_page1.SetDlgItemTextW(IDC_PROXY_EDIT, m_data.proxy_address.c_str());
    m_page1.SetDlgItemTextW(IDC_API_DOMESTIC_EDIT, m_data.api_domestic_url.c_str());
    m_page1.SetDlgItemTextW(IDC_API_FOREIGN_EDIT, m_data.api_foreign_url.c_str());

    wchar_t buf[32]{};
    swprintf_s(buf, L"%d", m_data.refresh_interval_sec);
    m_page1.SetDlgItemTextW(IDC_REFRESH_INTERVAL_EDIT, buf);

    // 5. 填充子页面 2 的延迟目标列表 (CListCtrl)
    for (int i = 0; i < (int)m_data.latency_targets.size(); ++i)
    {
        int idx = m_page2.m_latencyList.InsertItem(i, m_data.latency_targets[i].name.c_str());
        m_page2.m_latencyList.SetItemText(idx, 1, m_data.latency_targets[i].url.c_str());
    }

    // 6. 显示初始状态与默认页签
    m_page1.SetDlgItemTextW(IDC_STATUS_STATIC, g_data.GetLastUpdateStatusString().c_str());
    
    ShowTabControls(0);
    CRect rc; GetWindowRect(&rc); m_minTrackSize = rc.Size();
    
    m_layoutInitialized = true;
    LayoutAll();
    UpdateCurrentInfoUI();

    return TRUE;
}

void COptionsDlg::ShowTabControls(int tab)
{
    if (::IsWindow(m_page1.GetSafeHwnd())) m_page1.ShowWindow(tab == 0 ? SW_SHOW : SW_HIDE);
    if (::IsWindow(m_page2.GetSafeHwnd())) m_page2.ShowWindow(tab == 1 ? SW_SHOW : SW_HIDE);
    if (::IsWindow(m_page3.GetSafeHwnd())) m_page3.ShowWindow(tab == 2 ? SW_SHOW : SW_HIDE);
}

void COptionsDlg::OnTcnSelchangeTabMain(NMHDR *pNMHDR, LRESULT *pResult)
{
    int tab = m_tabCtrl.GetCurSel();
    ShowTabControls(tab);
    *pResult = 0;
}

void COptionsDlg::LayoutAll()
{
    if (!m_layoutInitialized) return;

    CRect rcCli; GetClientRect(&rcCli);

    // 1. 调整 Tab 控件大小填满主客户区
    CRect tabRect(7, 7, rcCli.right - 7, rcCli.bottom - 36);
    if (::IsWindow(m_tabCtrl.GetSafeHwnd()))
        m_tabCtrl.MoveWindow(tabRect);

    // 确定与取消按钮定位
    int btnBottom = rcCli.bottom - 10;
    CRect canR; GetDlgItem(IDCANCEL)->GetWindowRect(&canR); ScreenToClient(&canR);
    CRect okR; GetDlgItem(IDOK)->GetWindowRect(&okR); ScreenToClient(&okR);
    if (auto p = GetDlgItem(IDCANCEL)) p->MoveWindow(CRect(rcCli.right - 10 - canR.Width(), btnBottom - canR.Height(), rcCli.right - 10, btnBottom));
    if (auto p = GetDlgItem(IDOK))     p->MoveWindow(CRect(rcCli.right - 10 - canR.Width() - 8 - okR.Width(), btnBottom - okR.Height(), rcCli.right - 10 - canR.Width() - 8, btnBottom));

    // 2. 获取 Tab 内部可用子页面视区
    CRect dispRect = tabRect;
    m_tabCtrl.AdjustRect(FALSE, &dispRect);

    // 3. 对齐子页面视口位置
    if (::IsWindow(m_page1.GetSafeHwnd())) m_page1.MoveWindow(dispRect);
    if (::IsWindow(m_page2.GetSafeHwnd())) m_page2.MoveWindow(dispRect);
    if (::IsWindow(m_page3.GetSafeHwnd())) m_page3.MoveWindow(dispRect);

    // 4. 常规页签内部控件自适应缩放（w = 子视窗宽度）
    if (::IsWindow(m_page1.GetSafeHwnd()))
    {
        CRect rcP1; m_page1.GetClientRect(&rcP1);
        int w = rcP1.Width() - 14;
        if (w < 100) w = 100;
        
        if (auto p = m_page1.GetDlgItem(IDC_GRP_PROXY)) p->SetWindowPos(nullptr, 0, 0, w, 45, SWP_NOMOVE | SWP_NOZORDER);
        if (auto p = m_page1.GetDlgItem(IDC_GRP_LATENCY)) p->SetWindowPos(nullptr, 0, 0, w, 60, SWP_NOMOVE | SWP_NOZORDER);
        
        int editW = w - 55;
        if (editW < 50) editW = 50;
        if (auto p = m_page1.GetDlgItem(IDC_PROXY_EDIT)) p->SetWindowPos(nullptr, 0, 0, editW, 12, SWP_NOMOVE | SWP_NOZORDER);
        if (auto p = m_page1.GetDlgItem(IDC_API_DOMESTIC_EDIT)) p->SetWindowPos(nullptr, 0, 0, editW - 5, 12, SWP_NOMOVE | SWP_NOZORDER);
        if (auto p = m_page1.GetDlgItem(IDC_API_FOREIGN_EDIT)) p->SetWindowPos(nullptr, 0, 0, editW - 5, 12, SWP_NOMOVE | SWP_NOZORDER);

        if (auto p = m_page1.GetDlgItem(IDC_REFRESH_NOW_BUTTON)) p->MoveWindow(CRect(rcP1.right - 7 - 70, rcP1.top + 144, rcP1.right - 7, rcP1.top + 144 + 14));
    }

    // 5. 延迟列表页签控件自适应缩放
    if (::IsWindow(m_page2.GetSafeHwnd()))
    {
        CRect rcP2; m_page2.GetClientRect(&rcP2);
        
        // 列表控件铺满上方空间
        CRect listRect(rcP2.left + 7, rcP2.top + 7, rcP2.right - 7, rcP2.bottom - 28);
        if (auto p = m_page2.GetDlgItem(IDC_LATENCY_LIST)) p->MoveWindow(listRect);

        // 按钮排列在下方
        int btnY = rcP2.bottom - 21;
        if (auto p = m_page2.GetDlgItem(IDC_LATENCY_DEL_BUTTON)) p->MoveWindow(CRect(rcP2.right - 7 - 50, btnY, rcP2.right - 7, btnY + 14));
        if (auto p = m_page2.GetDlgItem(IDC_LATENCY_EDIT_BUTTON)) p->MoveWindow(CRect(rcP2.right - 7 - 105, btnY, rcP2.right - 7 - 55, btnY + 14));
        if (auto p = m_page2.GetDlgItem(IDC_LATENCY_ADD_BUTTON)) p->MoveWindow(CRect(rcP2.right - 7 - 160, btnY, rcP2.right - 7 - 110, btnY + 14));
    }

    // 6. 当前状态多行输入框铺满子页面
    if (::IsWindow(m_page3.GetSafeHwnd()))
    {
        CRect rcP3; m_page3.GetClientRect(&rcP3);
        CRect editRect(rcP3.left + 7, rcP3.top + 7, rcP3.right - 7, rcP3.bottom - 7);
        if (auto p = m_page3.GetDlgItem(IDC_STATUS_INFO_EDIT)) p->MoveWindow(editRect);
    }
}

void COptionsDlg::OnBnClickedShowSecondCheck()
{
    m_data.show_second = (IsDlgButtonChecked(IDC_SHOW_SECOND_CHECK) != 0);
}

void COptionsDlg::CollectLatencyTargets()
{
    m_data.latency_targets.clear();
    for (int i = 0; i < m_page2.m_latencyList.GetItemCount(); ++i)
    {
        LatencyTarget t;
        t.name = m_page2.m_latencyList.GetItemText(i, 0).GetString();
        t.url = m_page2.m_latencyList.GetItemText(i, 1).GetString();
        if (!t.url.empty())
            m_data.latency_targets.push_back(t);
    }
}

void COptionsDlg::OnOK()
{
    wchar_t buf[1024]{};
    if (m_page1.GetDlgItemTextW(IDC_PROXY_EDIT, buf, _countof(buf)) > 0) m_data.proxy_address = buf;
    if (m_page1.GetDlgItemTextW(IDC_API_DOMESTIC_EDIT, buf, _countof(buf)) > 0) m_data.api_domestic_url = buf;
    if (m_page1.GetDlgItemTextW(IDC_API_FOREIGN_EDIT, buf, _countof(buf)) > 0) m_data.api_foreign_url = buf;
    if (m_page1.GetDlgItemTextW(IDC_REFRESH_INTERVAL_EDIT, buf, _countof(buf)) > 0) { m_data.refresh_interval_sec = _wtoi(buf); if (m_data.refresh_interval_sec < 5) m_data.refresh_interval_sec = 5; }
    
    CollectLatencyTargets();
    m_data.use_proxy = (m_page1.IsDlgButtonChecked(IDC_USE_PROXY_CHECK) == BST_CHECKED);
    
    g_data.m_setting_data = m_data; 
    g_data.SaveConfig();
    
    g_data.m_is_updating = true;
    std::thread([]{
        g_data.UpdateIpInfoNow();
        g_data.m_is_updating = false;
    }).detach();

    CDialog::OnOK();
}

void COptionsDlg::OnBnClickedRefreshNow()
{
    if (auto p = m_page1.GetDlgItem(IDC_REFRESH_NOW_BUTTON)) { p->EnableWindow(FALSE); p->SetWindowTextW(L"正在刷新..."); }
    wchar_t buf[1024]{};
    if (m_page1.GetDlgItemTextW(IDC_PROXY_EDIT, buf, _countof(buf)) > 0) m_data.proxy_address = buf;
    if (m_page1.GetDlgItemTextW(IDC_API_DOMESTIC_EDIT, buf, _countof(buf)) > 0) m_data.api_domestic_url = buf;
    if (m_page1.GetDlgItemTextW(IDC_API_FOREIGN_EDIT, buf, _countof(buf)) > 0) m_data.api_foreign_url = buf;
    if (m_page1.GetDlgItemTextW(IDC_REFRESH_INTERVAL_EDIT, buf, _countof(buf)) > 0) { m_data.refresh_interval_sec = _wtoi(buf); if (m_data.refresh_interval_sec < 5) m_data.refresh_interval_sec = 5; }
    
    CollectLatencyTargets(); 
    g_data.m_setting_data = m_data; 
    g_data.SaveConfig();
    
    g_data.m_is_updating = true;
    auto hwnd = m_hWnd;
    std::thread([hwnd]{
        g_data.UpdateIpInfoNow();
        g_data.m_is_updating = false;
        ::PostMessage(hwnd, WM_IP_UPDATE_DONE, 0, 0);
    }).detach();
}

void COptionsDlg::OnBnClickedLatencyAdd()
{
    CLatencyEditDlg dlg(this);
    if (dlg.DoModal() == IDOK)
    {
        int idx = m_page2.m_latencyList.GetItemCount();
        int n = m_page2.m_latencyList.InsertItem(idx, dlg.m_name.c_str());
        m_page2.m_latencyList.SetItemText(n, 1, dlg.m_url.c_str());
    }
}

void COptionsDlg::OnBnClickedLatencyEdit()
{
    int sel = m_page2.m_latencyList.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0)
    {
        MessageBox(L"请先在列表中选择要修改的目标！", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }
    CLatencyEditDlg dlg(this);
    dlg.m_name = m_page2.m_latencyList.GetItemText(sel, 0).GetString();
    dlg.m_url = m_page2.m_latencyList.GetItemText(sel, 1).GetString();
    if (dlg.DoModal() == IDOK)
    {
        m_page2.m_latencyList.SetItemText(sel, 0, dlg.m_name.c_str());
        m_page2.m_latencyList.SetItemText(sel, 1, dlg.m_url.c_str());
    }
}

void COptionsDlg::OnBnClickedLatencyDel()
{
    int sel = m_page2.m_latencyList.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0)
    {
        MessageBox(L"请先在列表中选择要删除的目标！", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }
    m_page2.m_latencyList.DeleteItem(sel);
}

void COptionsDlg::UpdateCurrentInfoUI()
{
    CString text = g_data.m_tooltip.c_str();
    if (text.IsEmpty()) text = L"尚无数据，请点击“立即刷新”";
    if (::IsWindow(m_page3.GetSafeHwnd()))
    {
        if (auto p = m_page3.GetDlgItem(IDC_STATUS_INFO_EDIT))
            p->SetWindowTextW(text);
    }
}

void COptionsDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    CDialog::OnGetMinMaxInfo(lpMMI);
    if (m_minTrackSize.cx > 0 && m_minTrackSize.cy > 0)
    {
        lpMMI->ptMinTrackSize.x = m_minTrackSize.cx;
        lpMMI->ptMinTrackSize.y = m_minTrackSize.cy;
    }
}

LRESULT COptionsDlg::OnIpUpdateDone(WPARAM, LPARAM)
{
    CString status = g_data.GetLastUpdateStatusString().c_str();
    if (!m_data.use_proxy) status += L"  （未使用代理）";
    
    if (::IsWindow(m_page1.GetSafeHwnd()))
        m_page1.SetDlgItemTextW(IDC_STATUS_STATIC, status);
        
    UpdateCurrentInfoUI();
    
    if (::IsWindow(m_page1.GetSafeHwnd()))
    {
        if (auto p = m_page1.GetDlgItem(IDC_REFRESH_NOW_BUTTON))
        {
            p->EnableWindow(TRUE);
            p->SetWindowTextW(L"立即刷新");
        }
    }
    return 0;
}
