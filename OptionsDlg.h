#pragma once
#include "DataManager.h"
#include <vector>
#include <afxcmn.h> // Required for CTabCtrl and CListCtrl

// 自定义消息：刷新完成
#ifndef WM_IP_UPDATE_DONE
#define WM_IP_UPDATE_DONE (WM_APP + 101)
#endif

class COptionsDlg : public CDialog
{
	DECLARE_DYNAMIC(COptionsDlg)

public:
	COptionsDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~COptionsDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_OPTIONS_DIALOG };
#endif

    SettingData m_data;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();
    afx_msg void OnBnClickedShowSecondCheck();
    virtual void OnOK();
    afx_msg void OnBnClickedRefreshNow();
    afx_msg void OnBnClickedLatencyAdd();
    afx_msg void OnBnClickedLatencyEdit();
    afx_msg void OnBnClickedLatencyDel();
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
    CListCtrl m_latencyList;

    CSize m_minTrackSize{ 0, 0 };
    bool m_layoutInitialized{ false };
};
