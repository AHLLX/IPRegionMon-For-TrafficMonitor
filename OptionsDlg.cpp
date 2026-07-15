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
// COptionsDlg: 配置主对话框类实现
// ==========================================
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
}

BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
    ON_BN_CLICKED(IDC_SHOW_SECOND_CHECK, &COptionsDlg::OnBnClickedShowSecondCheck)
    ON_BN_CLICKED(IDC_REFRESH_NOW_BUTTON, &COptionsDlg::OnBnClickedRefreshNow)
    ON_BN_CLICKED(IDC_LATENCY_ADD_BUTTON, &COptionsDlg::OnBnClickedLatencyAdd)
    ON_BN_CLICKED(IDC_LATENCY_EDIT_BUTTON, &COptionsDlg::OnBnClickedLatencyEdit)
    ON_BN_CLICKED(IDC_LATENCY_DEL_BUTTON, &COptionsDlg::OnBnClickedLatencyDel)
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_MAIN, &COptionsDlg::OnTcnSelchangeTabMain)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_MESSAGE(WM_IP_UPDATE_DONE, &COptionsDlg::OnIpUpdateDone)
END_MESSAGE_MAP()

BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // 1. 系统默认字体统一应用到所有控件（美化排版）
    if (CFont* f = GetFont())
    {
        const int ids[] = { IDOK, IDCANCEL, IDC_TAB_MAIN, IDC_USE_PROXY_CHECK, IDC_PROXY_LABEL, IDC_PROXY_EDIT,
                            IDC_API_DOMESTIC_LABEL, IDC_API_DOMESTIC_EDIT,
                            IDC_API_FOREIGN_LABEL, IDC_API_FOREIGN_EDIT,
                            IDC_REFRESH_INTERVAL_LABEL, IDC_REFRESH_INTERVAL_EDIT, IDC_REFRESH_NOW_BUTTON,
                            IDC_STATUS_STATIC, IDC_GRP_PROXY, IDC_GRP_LATENCY, 
                            IDC_LATENCY_LIST, IDC_LATENCY_ADD_BUTTON, IDC_LATENCY_EDIT_BUTTON, IDC_LATENCY_DEL_BUTTON,
                            IDC_STATUS_INFO_EDIT };
        for (int id : ids)
        {
            if (auto p = GetDlgItem(id))
                p->SetFont(f);
        }
    }

    // 2. 绑定 Tab 控件及插入页签
    m_tabCtrl.SubclassDlgItem(IDC_TAB_MAIN, this);
    m_tabCtrl.InsertItem(0, L"常规设置");
    m_tabCtrl.InsertItem(1, L"延迟目标");
    m_tabCtrl.InsertItem(2, L"当前状态");

    // 3. 绑定并初始化延迟列表 (CListCtrl)
    m_latencyList.SubclassDlgItem(IDC_LATENCY_LIST, this);
    m_latencyList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_latencyList.InsertColumn(0, L"目标名称", LVCFMT_LEFT, 100);
    m_latencyList.InsertColumn(1, L"测试地址/URL", LVCFMT_LEFT, 230);

    // 填充延迟列表数据
    for (int i = 0; i < (int)m_data.latency_targets.size(); ++i)
    {
        int idx = m_latencyList.InsertItem(i, m_data.latency_targets[i].name.c_str());
        m_latencyList.SetItemText(idx, 1, m_data.latency_targets[i].url.c_str());
    }

    // 4. 加载常规设置参数
    CheckDlgButton(IDC_SHOW_SECOND_CHECK, m_data.show_second);
    CheckDlgButton(IDC_USE_PROXY_CHECK, m_data.use_proxy ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemTextW(IDC_PROXY_EDIT, m_data.proxy_address.c_str());
    SetDlgItemTextW(IDC_API_DOMESTIC_EDIT, m_data.api_domestic_url.c_str());
    SetDlgItemTextW(IDC_API_FOREIGN_EDIT, m_data.api_foreign_url.c_str());

    wchar_t buf[32]{};
    swprintf_s(buf, L"%d", m_data.refresh_interval_sec);
    SetDlgItemTextW(IDC_REFRESH_INTERVAL_EDIT, buf);

    // 5. 显示第一个 Tab 页并排版
    ShowTabControls(0);
    CRect rc; GetWindowRect(&rc); m_minTrackSize = rc.Size();
    SetDlgItemTextW(IDC_STATUS_STATIC, g_data.GetLastUpdateStatusString().c_str());
    
    m_layoutInitialized = true;
    LayoutAll();
    UpdateCurrentInfoUI();

    return TRUE;
}

void COptionsDlg::ShowTabControls(int tab)
{
    const int tab0_ids[] = {
        IDC_GRP_PROXY, IDC_USE_PROXY_CHECK, IDC_PROXY_LABEL, IDC_PROXY_EDIT,
        IDC_GRP_LATENCY, IDC_API_DOMESTIC_LABEL, IDC_API_DOMESTIC_EDIT,
        IDC_API_FOREIGN_LABEL, IDC_API_FOREIGN_EDIT,
        IDC_REFRESH_INTERVAL_LABEL, IDC_REFRESH_INTERVAL_EDIT, IDC_REFRESH_NOW_BUTTON,
        IDC_STATUS_STATIC
    };
    const int tab1_ids[] = {
        IDC_LATENCY_LIST, IDC_LATENCY_ADD_BUTTON, IDC_LATENCY_EDIT_BUTTON, IDC_LATENCY_DEL_BUTTON
    };
    const int tab2_ids[] = {
        IDC_STATUS_INFO_EDIT
    };

    auto ShowGroup = [&](const int* ids, int count, bool show) {
        for (int i = 0; i < count; ++i)
        {
            if (auto p = GetDlgItem(ids[i]))
                p->ShowWindow(show ? SW_SHOW : SW_HIDE);
        }
    };

    ShowGroup(tab0_ids, _countof(tab0_ids), tab == 0);
    ShowGroup(tab1_ids, _countof(tab1_ids), tab == 1);
    ShowGroup(tab2_ids, _countof(tab2_ids), tab == 2);
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

    // 1. 排版 Tab 控件主容器
    CRect tabRect(7, 7, rcCli.right - 7, rcCli.bottom - 36);
    if (::IsWindow(m_tabCtrl.GetSafeHwnd()))
        m_tabCtrl.MoveWindow(tabRect);

    auto MoveCtrl = [&](int id, const CRect& r){ if (auto p = GetDlgItem(id)) p->MoveWindow(r); };

    // 2. 底部的 确定/取消 按钮对齐
    int btnBottom = rcCli.bottom - 10;
    CRect canR; GetDlgItem(IDCANCEL)->GetWindowRect(&canR); ScreenToClient(&canR);
    CRect okR; GetDlgItem(IDOK)->GetWindowRect(&okR); ScreenToClient(&okR);
    MoveCtrl(IDCANCEL, CRect(rcCli.right - 10 - canR.Width(), btnBottom - canR.Height(), rcCli.right - 10, btnBottom));
    MoveCtrl(IDOK,     CRect(rcCli.right - 10 - canR.Width() - 8 - okR.Width(), btnBottom - okR.Height(), rcCli.right - 10 - canR.Width() - 8, btnBottom));

    // 计算 Tab 内部可用 client 宽度
    int innerWidth = tabRect.Width() - 16;
    if (innerWidth < 100) innerWidth = 100;

    // =====================================
    // Tab 0: 常规设置面板排版 (完全基于 Tab 坐标计算，无漂移)
    // =====================================
    MoveCtrl(IDC_GRP_PROXY, CRect(tabRect.left + 8, tabRect.top + 22, tabRect.left + 8 + innerWidth, tabRect.top + 22 + 56));
    MoveCtrl(IDC_GRP_LATENCY, CRect(tabRect.left + 8, tabRect.top + 84, tabRect.left + 8 + innerWidth, tabRect.top + 84 + 56));

    MoveCtrl(IDC_USE_PROXY_CHECK, CRect(tabRect.left + 16, tabRect.top + 34, tabRect.left + 16 + 100, tabRect.top + 34 + 10));
    MoveCtrl(IDC_PROXY_LABEL, CRect(tabRect.left + 16, tabRect.top + 52, tabRect.left + 16 + 50, tabRect.top + 52 + 10));
    MoveCtrl(IDC_PROXY_EDIT, CRect(tabRect.left + 70, tabRect.top + 50, tabRect.left + 8 + innerWidth - 8, tabRect.top + 50 + 12));

    MoveCtrl(IDC_API_DOMESTIC_LABEL, CRect(tabRect.left + 16, tabRect.top + 96, tabRect.left + 16 + 50, tabRect.top + 96 + 10));
    MoveCtrl(IDC_API_DOMESTIC_EDIT, CRect(tabRect.left + 70, tabRect.top + 94, tabRect.left + 8 + innerWidth - 8, tabRect.top + 94 + 12));
    MoveCtrl(IDC_API_FOREIGN_LABEL, CRect(tabRect.left + 16, tabRect.top + 114, tabRect.left + 16 + 50, tabRect.top + 114 + 10));
    MoveCtrl(IDC_API_FOREIGN_EDIT, CRect(tabRect.left + 70, tabRect.top + 112, tabRect.left + 8 + innerWidth - 8, tabRect.top + 112 + 12));

    MoveCtrl(IDC_REFRESH_INTERVAL_LABEL, CRect(tabRect.left + 16, tabRect.top + 152, tabRect.left + 16 + 70, tabRect.top + 152 + 10));
    MoveCtrl(IDC_REFRESH_INTERVAL_EDIT, CRect(tabRect.left + 90, tabRect.top + 150, tabRect.left + 90 + 40, tabRect.top + 150 + 12));
    MoveCtrl(IDC_REFRESH_NOW_BUTTON, CRect(tabRect.left + 8 + innerWidth - 68, tabRect.top + 149, tabRect.left + 8 + innerWidth, tabRect.top + 149 + 14));
    MoveCtrl(IDC_STATUS_STATIC, CRect(tabRect.left + 16, tabRect.top + 178, tabRect.left + 8 + innerWidth - 8, tabRect.top + 178 + 10));

    // =====================================
    // Tab 1: 延迟目标面板排版 (CListCtrl 自动拉伸)
    // =====================================
    MoveCtrl(IDC_LATENCY_LIST, CRect(tabRect.left + 8, tabRect.top + 22, tabRect.left + 8 + innerWidth, tabRect.top + 166));
    int btnY = tabRect.top + 176;
    MoveCtrl(IDC_LATENCY_DEL_BUTTON, CRect(tabRect.left + 8 + innerWidth - 50, btnY, tabRect.left + 8 + innerWidth, btnY + 14));
    MoveCtrl(IDC_LATENCY_EDIT_BUTTON, CRect(tabRect.left + 8 + innerWidth - 108, btnY, tabRect.left + 8 + innerWidth - 58, btnY + 14));
    MoveCtrl(IDC_LATENCY_ADD_BUTTON, CRect(tabRect.left + 8 + innerWidth - 166, btnY, tabRect.left + 8 + innerWidth - 116, btnY + 14));

    // =====================================
    // Tab 2: 当前状态面板排版 (动态匹配 Tab 大小)
    // =====================================
    MoveCtrl(IDC_STATUS_INFO_EDIT, CRect(tabRect.left + 8, tabRect.top + 22, tabRect.left + 8 + innerWidth, tabRect.top + 22 + 168));
}

void COptionsDlg::OnBnClickedShowSecondCheck()
{
    m_data.show_second = (IsDlgButtonChecked(IDC_SHOW_SECOND_CHECK) != 0);
}

void COptionsDlg::CollectLatencyTargets()
{
    m_data.latency_targets.clear();
    for (int i = 0; i < m_latencyList.GetItemCount(); ++i)
    {
        LatencyTarget t;
        t.name = m_latencyList.GetItemText(i, 0).GetString();
        t.url = m_latencyList.GetItemText(i, 1).GetString();
        if (!t.url.empty())
            m_data.latency_targets.push_back(t);
    }
}

void COptionsDlg::OnOK()
{
    wchar_t buf[1024]{};
    if (GetDlgItemTextW(IDC_PROXY_EDIT, buf, _countof(buf)) > 0) m_data.proxy_address = buf;
    if (GetDlgItemTextW(IDC_API_DOMESTIC_EDIT, buf, _countof(buf)) > 0) m_data.api_domestic_url = buf;
    if (GetDlgItemTextW(IDC_API_FOREIGN_EDIT, buf, _countof(buf)) > 0) m_data.api_foreign_url = buf;
    if (GetDlgItemTextW(IDC_REFRESH_INTERVAL_EDIT, buf, _countof(buf)) > 0) { m_data.refresh_interval_sec = _wtoi(buf); if (m_data.refresh_interval_sec < 5) m_data.refresh_interval_sec = 5; }
    
    CollectLatencyTargets();
    m_data.use_proxy = (IsDlgButtonChecked(IDC_USE_PROXY_CHECK) == BST_CHECKED);
    
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
    if (auto p = GetDlgItem(IDC_REFRESH_NOW_BUTTON)) { p->EnableWindow(FALSE); p->SetWindowTextW(L"正在刷新..."); }
    wchar_t buf[1024]{};
    if (GetDlgItemTextW(IDC_PROXY_EDIT, buf, _countof(buf)) > 0) m_data.proxy_address = buf;
    if (GetDlgItemTextW(IDC_API_DOMESTIC_EDIT, buf, _countof(buf)) > 0) m_data.api_domestic_url = buf;
    if (GetDlgItemTextW(IDC_API_FOREIGN_EDIT, buf, _countof(buf)) > 0) m_data.api_foreign_url = buf;
    if (GetDlgItemTextW(IDC_REFRESH_INTERVAL_EDIT, buf, _countof(buf)) > 0) { m_data.refresh_interval_sec = _wtoi(buf); if (m_data.refresh_interval_sec < 5) m_data.refresh_interval_sec = 5; }
    
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
        int idx = m_latencyList.GetItemCount();
        int n = m_latencyList.InsertItem(idx, dlg.m_name.c_str());
        m_latencyList.SetItemText(n, 1, dlg.m_url.c_str());
    }
}

void COptionsDlg::OnBnClickedLatencyEdit()
{
    int sel = m_latencyList.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0)
    {
        MessageBox(L"请先在列表中选择要修改的目标！", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }
    CLatencyEditDlg dlg(this);
    dlg.m_name = m_latencyList.GetItemText(sel, 0).GetString();
    dlg.m_url = m_latencyList.GetItemText(sel, 1).GetString();
    if (dlg.DoModal() == IDOK)
    {
        m_latencyList.SetItemText(sel, 0, dlg.m_name.c_str());
        m_latencyList.SetItemText(sel, 1, dlg.m_url.c_str());
    }
}

void COptionsDlg::OnBnClickedLatencyDel()
{
    int sel = m_latencyList.GetNextItem(-1, LVNI_SELECTED);
    if (sel < 0)
    {
        MessageBox(L"请先在列表中选择要删除的目标！", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }
    m_latencyList.DeleteItem(sel);
}

void COptionsDlg::UpdateCurrentInfoUI()
{
    CString text = g_data.m_tooltip.c_str();
    if (text.IsEmpty()) text = L"尚无数据，请点击“立即刷新”";
    if (::IsWindow(GetDlgItem(IDC_STATUS_INFO_EDIT)->GetSafeHwnd()))
    {
        GetDlgItem(IDC_STATUS_INFO_EDIT)->SetWindowTextW(text);
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
    SetDlgItemTextW(IDC_STATUS_STATIC, status);
    UpdateCurrentInfoUI();
    if (auto p = GetDlgItem(IDC_REFRESH_NOW_BUTTON)) { p->EnableWindow(TRUE); p->SetWindowTextW(L"立即刷新"); }
    return 0;
}
