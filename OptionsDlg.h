#pragma once

#include "afxdialogex.h"
#include "resource.h"
#include "DataManager.h"

// =========================================================
// CProxyApiPage - 常规网络设置页面
// =========================================================
class CProxyApiPage : public CDialogEx
{
    DECLARE_DYNAMIC(CProxyApiPage)
public:
    CProxyApiPage(CWnd* pParent = nullptr);
    virtual ~CProxyApiPage();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_TAB_PROXY_API };
#endif

public:
    SettingData m_data; // 本页负责的配置

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnBnClickedUseProxyCheck();
    afx_msg void OnBnClickedRefreshNow();
    DECLARE_MESSAGE_MAP()
};

// =========================================================
// CLatencyListPage - 延迟目标列表页面
// =========================================================
class CLatencyListPage : public CDialogEx
{
    DECLARE_DYNAMIC(CLatencyListPage)
public:
    CLatencyListPage(CWnd* pParent = nullptr);
    virtual ~CLatencyListPage();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_TAB_LATENCY_LIST };
#endif

public:
    std::vector<LatencyTarget> m_targets; // 本页负责的目标

private:
    CListCtrl m_list;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    void RefreshList();
    afx_msg void OnBnClickedAdd();
    afx_msg void OnBnClickedEdit();
    afx_msg void OnBnClickedDel();
    DECLARE_MESSAGE_MAP()
};

// =========================================================
// CStatusInfoPage - 纯净度及泄漏报告页面
// =========================================================
class CStatusInfoPage : public CDialogEx
{
    DECLARE_DYNAMIC(CStatusInfoPage)
public:
    CStatusInfoPage(CWnd* pParent = nullptr);
    virtual ~CStatusInfoPage();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_TAB_STATUS_INFO };
#endif

public:
    void RefreshReport();
protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    DECLARE_MESSAGE_MAP()
};


// =========================================================
// COptionsDlg - 主设置对话框
// =========================================================
class COptionsDlg : public CDialogEx
{
    DECLARE_DYNAMIC(COptionsDlg)

public:
    COptionsDlg(CWnd* pParent = nullptr);
    virtual ~COptionsDlg();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_OPTIONS_DIALOG };
#endif

private:
    CTabCtrl m_tab;
    CProxyApiPage m_pageProxyApi;
    CLatencyListPage m_pageLatencyList;
    CStatusInfoPage m_pageStatusInfo;
    
    std::vector<CDialogEx*> m_pages;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnTcnSelchangeTabMain(NMHDR *pNMHDR, LRESULT *pResult);
    virtual void OnOK();
    DECLARE_MESSAGE_MAP()
};

// =========================================================
// CLatencyEditDlg - 简单的编辑弹窗
// =========================================================
class CLatencyEditDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CLatencyEditDlg)
public:
    CLatencyEditDlg(CWnd* pParent = nullptr);
    virtual ~CLatencyEditDlg();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_LATENCY_EDIT_DIALOG };
#endif

    CString m_strName;
    CString m_strUrl;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    DECLARE_MESSAGE_MAP()
};
