#pragma once
#include <afxcmn.h>
#include "resource.h"
#include "DataManager.h"
#include "ModernUI.h"
#include "AsyncTextResult.h"
#include <vector>

// ────────────────────────────────────────────────────────────────────────
// Tab 0: 常规 / 网络 & 代理 设置 (现代圆角卡片)
// ────────────────────────────────────────────────────────────────────────
class CTabProxyApiDlg : public CModernSubDlg
{
#ifdef IPREGIONMON_TESTING
    friend struct ConcurrencyTestAccess;
#endif
    DECLARE_DYNAMIC(CTabProxyApiDlg)
public:
    explicit CTabProxyApiDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_TAB_PROXY_API };

    void GetData(SettingData& out) const;

protected:
    virtual BOOL OnInitDialog() override;
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() override {}
    virtual void OnCancel() override {}

    afx_msg void OnBnClickedUseProxy();
    afx_msg void OnBnClickedRefreshNow();
    afx_msg void OnBnClickedTestProxy();
    afx_msg void OnCbnSelchangeDisplayFormat();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnDestroy();
    void OnTestProxyDone(const std::wstring& message);
    DECLARE_MESSAGE_MAP()

private:
    void UpdateProxyEditState();
    std::shared_ptr<AsyncTextResult> m_testResult;

    CComboBox     m_cmbDisplayFormat;
    CButton       m_chkUseProxy;
    CEdit         m_edtProxy;
    CEdit         m_edtDomesticApi;
    CEdit         m_edtForeignApi;
    CEdit         m_edtInterval;
    CModernButton m_btnRefresh;
    CStatic       m_stcStatus;
    CModernButton m_btnTestProxy;
    CStatic       m_stcTestProxyStatus;
};

// ────────────────────────────────────────────────────────────────────────
// Tab 1: 延迟目标设置（现代卡片 + 自适应 Explorer 风格列表）
// ────────────────────────────────────────────────────────────────────────
class CTabLatencyDlg : public CModernSubDlg
{
#ifdef IPREGIONMON_TESTING
    friend struct ConcurrencyTestAccess;
#endif
    DECLARE_DYNAMIC(CTabLatencyDlg)
public:
    explicit CTabLatencyDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_TAB_LATENCY_LIST };

    void GetData(SettingData& out) const;
    void AdjustColumnWidths();

protected:
    virtual BOOL OnInitDialog() override;
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() override {}
    virtual void OnCancel() override {}

    afx_msg void OnBnClickedAdd();
    afx_msg void OnBnClickedDel();
    afx_msg void OnBnClickedUp();
    afx_msg void OnBnClickedDown();
    afx_msg void OnBnClickedTest();
    afx_msg void OnNMClickList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnNMDblclkList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnDestroy();
    void OnTestLatencyDone(const std::wstring& message);
    DECLARE_MESSAGE_MAP()

private:
    void RefreshList();
    void LoadSelectedToEdit();
    std::shared_ptr<AsyncTextResult> m_testResult;

    CListCtrl     m_listCtrl;
    CEdit         m_edtName;
    CEdit         m_edtUrl;
    CModernButton m_btnAdd;
    CModernButton m_btnDel;
    CModernButton m_btnUp;
    CModernButton m_btnDown;
    CModernButton m_btnTest;
    std::vector<LatencyTarget> m_targets;
};

// ────────────────────────────────────────────────────────────────────────
// Tab 2: 当前状态与安全检测报告（现代状态卡片 + 对齐复制）
// ────────────────────────────────────────────────────────────────────────
class CTabStatusDlg : public CModernSubDlg
{
    DECLARE_DYNAMIC(CTabStatusDlg)
public:
    explicit CTabStatusDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_TAB_STATUS_INFO };

    void UpdateStatusView();

protected:
    virtual BOOL OnInitDialog() override;
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnOK() override {}
    virtual void OnCancel() override {}

    afx_msg void OnBnClickedCopyDirect();
    afx_msg void OnBnClickedCopyProxy();
    afx_msg void OnBnClickedCopyAll();
    DECLARE_MESSAGE_MAP()

private:
    void CopyToClipboard(const std::wstring& text);

    CStatic       m_stcDirectIp;
    CStatic       m_stcDirectPlace;
    CStatic       m_stcDirectIsp;
    CStatic       m_stcDirectThreat;
    CModernButton m_btnCopyDirect;

    CStatic       m_stcProxyIp;
    CStatic       m_stcProxyPlace;
    CStatic       m_stcProxyIsp;
    CStatic       m_stcProxyThreat;
    CModernButton m_btnCopyProxy;

    CEdit         m_edtStatus;
    CModernButton m_btnCopyAll;
};

// ────────────────────────────────────────────────────────────────────────
// 主对话框：现代顶栏分段导航 + Fluent 容器
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

    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    DECLARE_MESSAGE_MAP()

private:
    void SwitchToPage(int idx);
    void PositionPages();

    CModernTabBar   m_tabBar;
    CModernButton   m_btnOK;
    CModernButton   m_btnCancel;
    CBrush          m_brWindowBg;

    CTabProxyApiDlg m_pageProxyApi;
    CTabLatencyDlg  m_pageLatency;
    CTabStatusDlg   m_pageStatus;
    CDialog*        m_pages[3];
    int             m_curPage{ 0 };
};
