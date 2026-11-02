#pragma once
#include <afxwin.h>
#include <afxcmn.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <functional>

// 现代 Win11 Fluent 视觉配色常量
namespace FluentTheme
{
    const COLORREF WindowBg      = RGB(243, 243, 243); // 窗口柔和底色 (Mica-like)
    const COLORREF CardBg        = RGB(255, 255, 255); // 卡片纯白背景
    const COLORREF CardBorder    = RGB(229, 229, 229); // 卡片细腻微边框
    const COLORREF TextPrimary   = RGB(32, 32, 32);     // 主标题与正文
    const COLORREF TextSecondary = RGB(110, 110, 110); // 次级说明文本
    const COLORREF Accent        = RGB(0, 103, 192);   // Win11 强调蓝
    const COLORREF AccentHover   = RGB(25, 117, 210);  // 鼠标悬停蓝
    const COLORREF AccentPressed = RGB(0, 90, 158);    // 按下深蓝
    const COLORREF BtnBg         = RGB(253, 253, 253); // 次级按钮浅色底
    const COLORREF BtnHover      = RGB(243, 243, 243); // 次级按钮悬停底
    const COLORREF BtnBorder     = RGB(214, 214, 214); // 次级按钮边框
}

// GDI+ 自动生命周期守卫与通用卡片自绘函数
class CGdiplusScope
{
public:
    static void EnsureInit();
    static void DrawModernCard(Gdiplus::Graphics& g, const Gdiplus::RectF& rect, const wchar_t* title = nullptr);
    static Gdiplus::GraphicsPath* CreateRoundRectPath(const Gdiplus::RectF& rect, float radius);
};

// 现代扁平圆角按钮（支持强调色主按钮与卡片次按钮）
class CModernButton : public CButton
{
public:
    CModernButton();
    virtual ~CModernButton() = default;

    void SetAccent(bool isAccent) { m_isAccent = isAccent; if (m_hWnd) Invalidate(); }
    void SetCornerRadius(int radius) { m_radius = radius; if (m_hWnd) Invalidate(); }

protected:
    virtual void PreSubclassWindow() override;
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) override;

    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    DECLARE_MESSAGE_MAP()

private:
    bool m_isAccent{ false };
    bool m_isHovered{ false };
    bool m_isTracking{ false };
    int  m_radius{ 4 };
    CFont m_font;
};

// 现代顶栏分段导航条（Segmented Navigation Bar）
class CModernTabBar : public CWnd
{
#ifdef IPREGIONMON_TESTING
    friend struct LocalizationTestAccess;
#endif
public:
    CModernTabBar();
    virtual ~CModernTabBar() = default;

    void AddTab(const std::wstring& title);
    int  GetCurSel() const { return m_curSel; }
    void SetCurSel(int sel);

    // 绑定选卡切换回调
    void SetOnSelChanged(std::function<void(int)> cb) { m_onSelChanged = cb; }

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();

    DECLARE_MESSAGE_MAP()

private:
    std::vector<std::wstring> m_tabs;
    int m_curSel{ 0 };
    int m_hoverSel{ -1 };
    bool m_isTracking{ false };
    CFont m_font;
    std::function<void(int)> m_onSelChanged;
};

// 卡片描述结构体 (DLU 坐标)
struct ModernCardDef
{
    int leftDlu;
    int topDlu;
    int rightDlu;
    int bottomDlu;
    std::wstring title;
};

// 现代子对话框基类：支持自动卡片绘制、DPI 感知、背景管理与控件主题美化
class CModernSubDlg : public CDialog
{
#ifdef IPREGIONMON_TESTING
    friend struct LocalizationTestAccess;
#endif
    DECLARE_DYNAMIC(CModernSubDlg)
public:
    CModernSubDlg(UINT nIDTemplate, CWnd* pParent = nullptr);
    virtual ~CModernSubDlg();
    BOOL Create(UINT templateId, CWnd* parent);

    void AddCard(int leftDlu, int topDlu, int rightDlu, int bottomDlu, const std::wstring& title);

protected:
    virtual BOOL OnInitDialog() override;
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

    DECLARE_MESSAGE_MAP()

    CBrush m_brWindowBg;
    CBrush m_brCardBg;
    CFont  m_defaultUiFont;
    std::vector<ModernCardDef> m_cards;
};
