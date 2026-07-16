#pragma once
#include <afxcmn.h>    // CTabCtrl
#include "resource.h"
#include "DataManager.h"

// ────────────────────────────────────────────────────────────────────────
// Tab 0: 常规 / 网络 & 代理 设置
// ────────────────────────────────────────────────────────────────────────
class CTabProxyApiDlg : public CDialog
{
    DECLARE_DYNAMIC(CTabProxyApiDlg)
public:
    explicit CTabProxyApiDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_TAB_PROXY_API };

    void GetData(SettingData& out) const;

protected:
    virtual BOOL OnInitDialog() override;
    virtual void DoDataExchange(CDataExchange* pDX) override;
    // 子页不拦截 IDOK/IDCANCEL，交由父对话框处理
    virtual void OnOK() override {}
    virtual void OnCancel() override {}

    afx_msg void OnBnClickedUseProxy();
    afx_msg void OnBnClickedRefreshNow();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    DECLARE_MESSAGE_MAP()

private:
    void UpdateProxyEditState();

    CButton m_chkUseProxy;
    CEdit   m_edtProxy;
    CEdit   m_edtDomesticApi;
    CEdit   m_edtForeignApi;
    CEdit   m_edtInterval;
    CButton m_btnRefresh;
    CStatic m_stcStatus;
};

// ────────────────────────────────────────────────────────────────────────
// Tab 1: 延迟目标（3 行固定 Edit 对，彻底替代 SysListView32）
// ────────────────────────────────────────────────────────────────────────
class CTabLatencyDlg : public CDialog
{
    DECLARE_DYNAMIC(CTabLatencyDlg)
public:
    explicit CTabLatencyDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_TAB_LATENCY_LIST };

    void GetData(SettingData& out) const;

protected:
    virtual BOOL OnInitDialog() override;
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() override {}
    virtual void OnCancel() override {}

    afx_msg void OnBnClickedDel0();
    afx_msg void OnBnClickedDel1();
    afx_msg void OnBnClickedDel2();
    DECLARE_MESSAGE_MAP()

private:
    // 删除指定行并将后续行上移
    void DeleteRow(int row);

    CEdit   m_edtName[3];
    CEdit   m_edtUrl[3];
    CButton m_btnDel[3];
};

// ────────────────────────────────────────────────────────────────────────
// Tab 2: 当前状态（只读展示 tooltip 内容）
// ────────────────────────────────────────────────────────────────────────
class CTabStatusDlg : public CDialog
{
    DECLARE_DYNAMIC(CTabStatusDlg)
public:
    explicit CTabStatusDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_TAB_STATUS_INFO };

protected:
    virtual BOOL OnInitDialog() override;
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() override {}
    virtual void OnCancel() override {}
    DECLARE_MESSAGE_MAP()

private:
    CEdit m_edtStatus;
};

// ────────────────────────────────────────────────────────────────────────
// 主对话框：Tab 容器
// ────────────────────────────────────────────────────────────────────────
class COptionsDlg : public CDialog
{
    DECLARE_DYNAMIC(COptionsDlg)
public:
    explicit COptionsDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_OPTIONS_DIALOG };

protected:
    virtual BOOL OnInitDialog() override;
    virtual void OnOK() override;
    virtual void DoDataExchange(CDataExchange* pDX) override;

    afx_msg void OnTcnSelchangeTabMain(NMHDR* pNMHDR, LRESULT* pResult);
    DECLARE_MESSAGE_MAP()

private:
    void SwitchToPage(int idx);
    void PositionPages();

    CTabCtrl        m_tabCtrl;
    CTabProxyApiDlg m_pageProxyApi;
    CTabLatencyDlg  m_pageLatency;
    CTabStatusDlg   m_pageStatus;
    CDialog*        m_pages[3];
    int             m_curPage{ 0 };
};
