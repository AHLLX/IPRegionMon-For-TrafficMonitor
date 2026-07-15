#pragma once
#include "DataManager.h"
#include <vector>
#include <afxcmn.h>

// 自定义消息：刷新完成
#ifndef WM_IP_UPDATE_DONE
#define WM_IP_UPDATE_DONE (WM_APP + 101)
#endif

class COptionsDlg;

// ==========================================
// 页签 1：常规设置 (代理与 API)
// ==========================================
class CPageProxyApi : public CDialog
{
public:
    CPageProxyApi(COptionsDlg* pParent);
protected:
    virtual BOOL OnInitDialog();
    afx_msg void OnBnClickedRefreshNow();
    DECLARE_MESSAGE_MAP()
private:
    COptionsDlg* m_pParent;
};

// ==========================================
// 页签 2：延迟目标 (ListCtrl 列表)
// ==========================================
class CPageLatencyList : public CDialog
{
public:
    CPageLatencyList(COptionsDlg* pParent);
    CListCtrl m_latencyList;
protected:
    virtual BOOL OnInitDialog();
    afx_msg void OnBnClickedLatencyAdd();
    afx_msg void OnBnClickedLatencyEdit();
    afx_msg void OnBnClickedLatencyDel();
    DECLARE_MESSAGE_MAP()
private:
    COptionsDlg* m_pParent;
};

// ==========================================
// 页签 3：当前状态 (只读多行文本)
// ==========================================
class CPageStatusInfo : public CDialog
{
public:
    CPageStatusInfo(COptionsDlg* pParent);
protected:
    virtual BOOL OnInitDialog();
    DECLARE_MESSAGE_MAP()
private:
    COptionsDlg* m_pParent;
};

// ==========================================
// 主配置对话框
// ==========================================
class COptionsDlg : public CDialog
{
	DECLARE_DYNAMIC(COptionsDlg)

public:
	COptionsDlg(CWnd* pParent = nullptr);
	virtual ~COptionsDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_OPTIONS_DIALOG };
#endif

    SettingData m_data;

    // 页面交互事件路由接口
    void OnBnClickedRefreshNow();
    void OnBnClickedLatencyAdd();
    void OnBnClickedLatencyEdit();
    void OnBnClickedLatencyDel();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

	DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();
    afx_msg void OnBnClickedShowSecondCheck();
    virtual void OnOK();
    afx_msg void OnTcnSelchangeTabMain(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnSize(UINT nType, int cx, int cy) { CDialog::OnSize(nType, cx, cy); if (IsWindowVisible()) LayoutAll(); }
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg LRESULT OnIpUpdateDone(WPARAM, LPARAM);

private:
    void CollectLatencyTargets();
    void LayoutAll();
    void UpdateCurrentInfoUI();
    void ShowTabControls(int tab);

    CTabCtrl m_tabCtrl;

    // 子页面对象
    CPageProxyApi m_page1;
    CPageLatencyList m_page2;
    CPageStatusInfo m_page3;

    CSize m_minTrackSize{ 0, 0 };
    bool m_layoutInitialized{ false };
};
