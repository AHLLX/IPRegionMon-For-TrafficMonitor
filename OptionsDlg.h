#pragma once
#include "DataManager.h"
#include <memory>
#include <vector>

// 自定义消息：刷新完成
#ifndef WM_IP_UPDATE_DONE
#define WM_IP_UPDATE_DONE (WM_APP + 101)
#endif

// COptionsDlg 对话框

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
    afx_msg void OnLatencyDelClicked(UINT nID);
    afx_msg void OnSize(UINT nType, int cx, int cy) { CDialog::OnSize(nType, cx, cy); if (IsWindowVisible()) LayoutAll(); }
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg LRESULT OnIpUpdateDone(WPARAM, LPARAM);

private:
    void RefreshLatencyListUI();
    void AddLatencyRow(const LatencyTarget& t);
    void CollectLatencyTargets();

    // 自适应布局
    void LayoutAll();
    void UpdateCurrentInfoUI();

    // 运行时创建的控件与数据行，避免使用全局静态
    struct RowCtrl { CEdit name_edit; CEdit url_edit; CButton del_btn; };
    std::vector<std::unique_ptr<RowCtrl>> m_rows;
    CEdit m_infoEdit;

    CSize m_minTrackSize{ 0, 0 };
    int m_latencyBottomY{ 0 }; // 动态行布局后的底部Y
};
