// OptionsDlg.cpp: 实现文件
//

#include "pch.h"
#include "IPRegionMon.h"
#include "OptionsDlg.h"
#include "afxdialogex.h"
#include <thread>

// 限制最大3行
static const int kMaxLatencyRows = 3;

IMPLEMENT_DYNAMIC(COptionsDlg, CDialog)

COptionsDlg::COptionsDlg(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_OPTIONS_DIALOG, pParent)
{
}

COptionsDlg::~COptionsDlg() {}

void COptionsDlg::DoDataExchange(CDataExchange* pDX) { CDialog::DoDataExchange(pDX); }

BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
    ON_BN_CLICKED(IDC_SHOW_SECOND_CHECK, &COptionsDlg::OnBnClickedShowSecondCheck)
    ON_BN_CLICKED(IDC_REFRESH_NOW_BUTTON, &COptionsDlg::OnBnClickedRefreshNow)
    ON_BN_CLICKED(IDC_LATENCY_ADD_BUTTON, &COptionsDlg::OnBnClickedLatencyAdd)
    ON_COMMAND_RANGE(IDC_LATENCY_DEL_BASE, IDC_LATENCY_DEL_BASE + 999, &COptionsDlg::OnLatencyDelClicked)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_MESSAGE(WM_IP_UPDATE_DONE, &COptionsDlg::OnIpUpdateDone)
END_MESSAGE_MAP()

LRESULT COptionsDlg::OnIpUpdateDone(WPARAM, LPARAM)
{
    CString status = g_data.GetLastUpdateStatusString().c_str();
    if (!m_data.use_proxy) status += L"  （未使用代理）";
    SetDlgItemTextW(IDC_STATUS_STATIC, status);
    UpdateCurrentInfoUI();
    if (auto p = GetDlgItem(IDC_REFRESH_NOW_BUTTON)) { p->EnableWindow(TRUE); p->SetWindowTextW(L"立即刷新"); }
    return 0;
}

void COptionsDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    if (m_minTrackSize.cx == 0) { CRect rc; GetWindowRect(&rc); m_minTrackSize = rc.Size(); }
    lpMMI->ptMinTrackSize.x = m_minTrackSize.cx;
    lpMMI->ptMinTrackSize.y = m_minTrackSize.cy;
}

void COptionsDlg::AddLatencyRow(const LatencyTarget& t)
{
    if (m_rows.size() >= kMaxLatencyRows) return;
    auto row = std::make_unique<RowCtrl>();
    row->name_edit.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER, CRect(0, 0, 0, 0), this, 0);
    row->name_edit.SetWindowTextW(t.name.c_str());
    row->url_edit.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER, CRect(0, 0, 0, 0), this, 0);
    row->url_edit.SetWindowTextW(t.url.c_str());
    row->del_btn.Create(L"-", WS_CHILD | WS_VISIBLE | WS_TABSTOP, CRect(0, 0, 0, 0), this, IDC_LATENCY_DEL_BASE);
    if (CFont* f = GetFont()) { row->name_edit.SetFont(f); row->url_edit.SetFont(f); row->del_btn.SetFont(f); }
    m_rows.push_back(std::move(row));
}

void COptionsDlg::RefreshLatencyListUI()
{
    if (auto pAdd = GetDlgItem(IDC_LATENCY_ADD_BUTTON))
    {
        if (auto pRefresh = GetDlgItem(IDC_REFRESH_NOW_BUTTON))
        { CRect rc; pRefresh->GetWindowRect(&rc); ScreenToClient(&rc); int h = rc.Height(); pAdd->MoveWindow(rc.right + 8, rc.top, h, h); pAdd->ShowWindow(SW_SHOW); }
    }

    CRect grp; if (auto g = GetDlgItem(IDC_GRP_LATENCY)) { g->GetWindowRect(&grp); ScreenToClient(&grp); }
    CRect refEdit; if (auto e = GetDlgItem(IDC_REFRESH_INTERVAL_EDIT)) { e->GetWindowRect(&refEdit); ScreenToClient(&refEdit); }
    CRect refBtn; if (auto b = GetDlgItem(IDC_REFRESH_NOW_BUTTON)) { b->GetWindowRect(&refBtn); ScreenToClient(&refBtn); }

    int leftPad = refEdit.left - grp.left; int nameW = 110, gap = 8, delW = 24; int rowH = refEdit.Height();
    int top = max(refBtn.bottom, refEdit.bottom) + 10; int urlRight = refBtn.left - 8 - delW - gap;

    for (size_t i = 0; i < min(m_rows.size(), (size_t)kMaxLatencyRows); ++i)
    {
        auto& row = *m_rows[i].get();
        row.name_edit.MoveWindow(CRect(leftPad + grp.left, top, leftPad + grp.left + nameW, top + rowH));
        row.url_edit.MoveWindow(CRect(leftPad + grp.left + nameW + gap, top, urlRight, top + rowH));
        row.del_btn.MoveWindow(CRect(urlRight + gap, top, urlRight + gap + delW, top + rowH));
        row.del_btn.SetDlgCtrlID(IDC_LATENCY_DEL_BASE + static_cast<int>(i));
        top += rowH + 6;
    }
    m_latencyBottomY = top;
    UpdateCurrentInfoUI();
}

void COptionsDlg::CollectLatencyTargets()
{
    m_data.latency_targets.clear();
    for (auto& uptr : m_rows)
    {
        wchar_t nbuf[256]{}; uptr->name_edit.GetWindowTextW(nbuf, _countof(nbuf));
        wchar_t ubuf[512]{}; uptr->url_edit.GetWindowTextW(ubuf, _countof(ubuf));
        if (wcslen(ubuf) > 0) m_data.latency_targets.push_back({ nbuf, ubuf });
    }
}

void COptionsDlg::LayoutAll()
{
    CRect rcCli; GetClientRect(&rcCli);

    auto rcOf = [&](int id){ CRect r; GetDlgItem(id)->GetWindowRect(&r); ScreenToClient(&r); return r; };
    CRect grpProxy = rcOf(IDC_GRP_PROXY), grpLatency = rcOf(IDC_GRP_LATENCY);
    CRect useProxy = rcOf(IDC_USE_PROXY_CHECK), proxyLabel = rcOf(IDC_PROXY_LABEL), proxyEdit = rcOf(IDC_PROXY_EDIT);
    CRect refLabel = rcOf(IDC_REFRESH_INTERVAL_LABEL), refEdit = rcOf(IDC_REFRESH_INTERVAL_EDIT), refBtn = rcOf(IDC_REFRESH_NOW_BUTTON), addBtn = rcOf(IDC_LATENCY_ADD_BUTTON);

    const int margin = 7, vgap = 8;
    CRect grpProxyN(margin, margin, rcCli.right - margin, margin + grpProxy.Height());
    CRect grpLatencyN(margin, grpProxyN.bottom + vgap, rcCli.right - margin, rcCli.bottom - 48);

    auto MoveCtrl = [&](int id, const CRect& r){ if (auto p = GetDlgItem(id)) p->MoveWindow(r); };
    MoveCtrl(IDC_GRP_PROXY, grpProxyN); MoveCtrl(IDC_GRP_LATENCY, grpLatencyN);

    auto leftOf=[&](CRect c,CRect p){return c.left-p.left;}; auto topOf=[&](CRect c,CRect p){return c.top-p.top;}; auto rightM=[&](CRect c,CRect p){return p.right-c.right;};

    MoveCtrl(IDC_USE_PROXY_CHECK, CRect(grpProxyN.left + leftOf(useProxy, grpProxy), grpProxyN.top + topOf(useProxy, grpProxy), grpProxyN.left + leftOf(useProxy, grpProxy) + useProxy.Width(), grpProxyN.top + topOf(useProxy, grpProxy) + useProxy.Height()));
    MoveCtrl(IDC_PROXY_LABEL,      CRect(grpProxyN.left + leftOf(proxyLabel, grpProxy), grpProxyN.top + topOf(proxyLabel, grpProxy), grpProxyN.left + leftOf(proxyLabel, grpProxy) + proxyLabel.Width(), grpProxyN.top + topOf(proxyLabel, grpProxy) + proxyLabel.Height()));
    MoveCtrl(IDC_PROXY_EDIT,       CRect(grpProxyN.left + leftOf(proxyEdit, grpProxy),  grpProxyN.top + topOf(proxyEdit, grpProxy),  grpProxyN.right - rightM(proxyEdit, grpProxy), grpProxyN.top + topOf(proxyEdit, grpProxy) + proxyEdit.Height()));

    MoveCtrl(IDC_REFRESH_INTERVAL_LABEL, CRect(grpLatencyN.left + leftOf(refLabel, grpLatency), grpLatencyN.top + topOf(refLabel, grpLatency), grpLatencyN.left + leftOf(refLabel, grpLatency) + refLabel.Width(), grpLatencyN.top + topOf(refLabel, grpLatency) + refLabel.Height()));
    MoveCtrl(IDC_REFRESH_INTERVAL_EDIT,  CRect(grpLatencyN.left + leftOf(refEdit, grpLatency),  grpLatencyN.top + topOf(refEdit, grpLatency),  grpLatencyN.left + leftOf(refEdit, grpLatency) + refEdit.Width(), grpLatencyN.top + topOf(refEdit, grpLatency) + refEdit.Height()));

    MoveCtrl(IDC_REFRESH_NOW_BUTTON, CRect(grpLatencyN.right - rightM(refBtn, grpLatency) - refBtn.Width(), grpLatencyN.top + topOf(refBtn, grpLatency), grpLatencyN.right - rightM(refBtn, grpLatency), grpLatencyN.top + topOf(refBtn, grpLatency) + refBtn.Height()));
    MoveCtrl(IDC_LATENCY_ADD_BUTTON, CRect(grpLatencyN.right - rightM(addBtn, grpLatency) - addBtn.Width(), grpLatencyN.top + topOf(addBtn, grpLatency), grpLatencyN.right - rightM(addBtn, grpLatency), grpLatencyN.top + topOf(addBtn, grpLatency) + addBtn.Height()));

    RefreshLatencyListUI();

    // OK / 取消
    CRect okR; GetDlgItem(IDOK)->GetWindowRect(&okR); ScreenToClient(&okR);
    CRect canR; GetDlgItem(IDCANCEL)->GetWindowRect(&canR); ScreenToClient(&canR);
    int btnBottom = rcCli.bottom - 10;
    MoveCtrl(IDCANCEL, CRect(rcCli.right - 10 - canR.Width(), btnBottom - canR.Height(), rcCli.right - 10, btnBottom));
    MoveCtrl(IDOK,     CRect(rcCli.right - 10 - canR.Width() - 8 - okR.Width(), btnBottom - okR.Height(), rcCli.right - 10 - canR.Width() - 8, btnBottom));
}

BOOL COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    if (CFont* f = GetFont()) { if (auto p = GetDlgItem(IDOK)) p->SetFont(f); if (auto p = GetDlgItem(IDCANCEL)) p->SetFont(f); }

    CheckDlgButton(IDC_SHOW_SECOND_CHECK, m_data.show_second);
    CheckDlgButton(IDC_USE_PROXY_CHECK, m_data.use_proxy ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemTextW(IDC_PROXY_EDIT, m_data.proxy_address.c_str());

    wchar_t buf[32]{}; swprintf_s(buf, L"%d", m_data.refresh_interval_sec); SetDlgItemTextW(IDC_REFRESH_INTERVAL_EDIT, buf);

    m_rows.clear();
    auto initTargets = m_data.latency_targets; if (initTargets.empty()) initTargets = { {L"中国", L"https://www.baidu.com"}, {L"美国", L"https://www.google.com"}, {L"日本", L"https://www.yahoo.co.jp"} };
    if (initTargets.size() > kMaxLatencyRows) initTargets.resize(kMaxLatencyRows);
    for (const auto& t : initTargets) AddLatencyRow(t);

    RefreshLatencyListUI();
    CRect rc; GetWindowRect(&rc); m_minTrackSize = rc.Size();
    SetDlgItemTextW(IDC_STATUS_STATIC, g_data.GetLastUpdateStatusString().c_str());
    UpdateCurrentInfoUI();
    return TRUE;
}

void COptionsDlg::OnBnClickedShowSecondCheck() { m_data.show_second = (IsDlgButtonChecked(IDC_SHOW_SECOND_CHECK) != 0); }

void COptionsDlg::OnOK()
{
    wchar_t buf[1024]{};
    if (GetDlgItemTextW(IDC_PROXY_EDIT, buf, _countof(buf)) > 0) m_data.proxy_address = buf;
    if (GetDlgItemTextW(IDC_REFRESH_INTERVAL_EDIT, buf, _countof(buf)) > 0) { m_data.refresh_interval_sec = _wtoi(buf); if (m_data.refresh_interval_sec < 5) m_data.refresh_interval_sec = 5; }
    CollectLatencyTargets();
    m_data.use_proxy = (IsDlgButtonChecked(IDC_USE_PROXY_CHECK) == BST_CHECKED);
    g_data.m_setting_data = m_data; g_data.SaveConfig();
    std::thread([]{ g_data.UpdateIpInfoNow(); }).detach();
    CDialog::OnOK();
}

void COptionsDlg::OnBnClickedRefreshNow()
{
    if (auto p = GetDlgItem(IDC_REFRESH_NOW_BUTTON)) { p->EnableWindow(FALSE); p->SetWindowTextW(L"正在刷新..."); }
    CollectLatencyTargets(); g_data.m_setting_data = m_data; g_data.SaveConfig();
    auto hwnd = m_hWnd; std::thread([hwnd]{ g_data.UpdateIpInfoNow(); ::PostMessage(hwnd, WM_IP_UPDATE_DONE, 0, 0); }).detach();
}

void COptionsDlg::OnBnClickedLatencyAdd()
{
    if ((int)m_rows.size() >= kMaxLatencyRows) return; AddLatencyRow({ L"", L"" }); RefreshLatencyListUI();
}

void COptionsDlg::OnLatencyDelClicked(UINT nID)
{
    int idx = (int)nID - IDC_LATENCY_DEL_BASE; if (idx < 0 || idx >= (int)m_rows.size()) return;
    m_rows[idx]->name_edit.DestroyWindow(); m_rows[idx]->url_edit.DestroyWindow(); m_rows[idx]->del_btn.DestroyWindow();
    m_rows.erase(m_rows.begin() + idx); RefreshLatencyListUI();
}

void COptionsDlg::UpdateCurrentInfoUI()
{
    CString text = g_data.m_tooltip.c_str(); if (text.IsEmpty()) text = L"尚无数据，请点击‘立即刷新’";
    CRect grp; if (auto g = GetDlgItem(IDC_GRP_LATENCY)) { g->GetWindowRect(&grp); ScreenToClient(&grp); }
    const int margin = 10; int safeTop = max(m_latencyBottomY + 6, grp.top + 40);
    CRect rcArea(grp.left + margin, safeTop, grp.right - margin, grp.bottom - margin); if (rcArea.bottom < rcArea.top + 60) rcArea.bottom = rcArea.top + 60;
    if (!::IsWindow(m_infoEdit.m_hWnd)) { m_infoEdit.Create(WS_CHILD | WS_VISIBLE | ES_AUTOVSCROLL | ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_BORDER, rcArea, this, 0); m_infoEdit.SetFont(GetFont()); }
    else { m_infoEdit.MoveWindow(rcArea); }
    m_infoEdit.SetWindowTextW(text);
}
