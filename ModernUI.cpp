#include "pch.h"
#include "ModernUI.h"
#include "UiLanguage.h"
#include <dwmapi.h>
#include <uxtheme.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

using namespace Gdiplus;

void CGdiplusScope::EnsureInit()
{
    static ULONG_PTR s_token = 0;
    if (s_token == 0)
    {
        GdiplusStartupInput input;
        GdiplusStartup(&s_token, &input, nullptr);
    }
}

GraphicsPath* CGdiplusScope::CreateRoundRectPath(const RectF& rect, float radius)
{
    GraphicsPath* path = new GraphicsPath();
    float diameter = radius * 2.0f;
    if (diameter > rect.Width) diameter = rect.Width;
    if (diameter > rect.Height) diameter = rect.Height;

    path->AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
    path->AddArc(rect.X + rect.Width - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
    path->AddArc(rect.X + rect.Width - diameter, rect.Y + rect.Height - diameter, diameter, diameter, 0.0f, 90.0f);
    path->AddArc(rect.X, rect.Y + rect.Height - diameter, diameter, diameter, 90.0f, 90.0f);
    path->CloseFigure();
    return path;
}

void CGdiplusScope::DrawModernCard(Graphics& g, const RectF& rect, const wchar_t* title)
{
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    RectF rF(rect.X + 0.5f, rect.Y + 0.5f, rect.Width - 1.0f, rect.Height - 1.0f);
    GraphicsPath* path = CreateRoundRectPath(rF, 6.0f);

    SolidBrush bgBrush(Color(255, 255, 255));
    g.FillPath(&bgBrush, path);

    Pen borderPen(Color(229, 229, 229), 1.0f);
    g.DrawPath(&borderPen, path);
    delete path;

    if (title && wcslen(title) > 0)
    {
        FontFamily fontFamily(UiLanguage::Text(IDS_UI_FONT).c_str());
        Gdiplus::Font font(&fontFamily, 9.5f, FontStyleBold, UnitPoint);
        SolidBrush textBrush(Color(32, 32, 32));

        PointF origin(rect.X + 10.0f, rect.Y + 7.0f);
        g.DrawString(title, -1, &font, origin, &textBrush);
    }
}

// ====================================================================
// CModernButton 实现
// ====================================================================
CModernButton::CModernButton()
{
    CGdiplusScope::EnsureInit();
}

void CModernButton::PreSubclassWindow()
{
    CButton::PreSubclassWindow();
    ModifyStyle(0, BS_OWNERDRAW);

    if (!m_font.GetSafeHandle())
    {
        m_font.CreatePointFont(90, UiLanguage::Text(IDS_UI_FONT).c_str());
    }
}

BEGIN_MESSAGE_MAP(CModernButton, CButton)
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL CModernButton::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE; // 防闪烁
}

void CModernButton::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_isTracking)
    {
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, m_hWnd, 0 };
        TrackMouseEvent(&tme);
        m_isTracking = true;
    }
    if (!m_isHovered)
    {
        m_isHovered = true;
        Invalidate();
    }
    CButton::OnMouseMove(nFlags, point);
}

void CModernButton::OnMouseLeave()
{
    m_isHovered = false;
    m_isTracking = false;
    Invalidate();
    CButton::OnMouseLeave();
}

void CModernButton::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);
    CRect rc = lpDrawItemStruct->rcItem;

    CDC memDC;
    memDC.CreateCompatibleDC(pDC);
    CBitmap memBmp;
    memBmp.CreateCompatibleBitmap(pDC, rc.Width(), rc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&memBmp);

    // 默认背景使用白色或窗口底色（父卡片背景通常为白色）
    memDC.FillSolidRect(0, 0, rc.Width(), rc.Height(), FluentTheme::CardBg);

    Graphics g(memDC.GetSafeHdc());
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    bool isPressed  = (lpDrawItemStruct->itemState & ODS_SELECTED) != 0;
    bool isDisabled = (lpDrawItemStruct->itemState & ODS_DISABLED) != 0;

    RectF rF(0.5f, 0.5f, (float)rc.Width() - 1.0f, (float)rc.Height() - 1.0f);
    GraphicsPath* path = CGdiplusScope::CreateRoundRectPath(rF, (float)m_radius);

    Color bgCol, borderCol, textCol;

    if (isDisabled)
    {
        bgCol     = Color(240, 240, 240);
        borderCol = Color(225, 225, 225);
        textCol   = Color(160, 160, 160);
    }
    else if (m_isAccent)
    {
        if (isPressed)        bgCol = Color(0, 90, 158);
        else if (m_isHovered) bgCol = Color(25, 117, 210);
        else                  bgCol = Color(0, 103, 192);
        borderCol = bgCol;
        textCol   = Color(255, 255, 255);
    }
    else
    {
        if (isPressed)        bgCol = Color(230, 230, 230);
        else if (m_isHovered) bgCol = Color(243, 243, 243);
        else                  bgCol = Color(253, 253, 253);
        borderCol = Color(214, 214, 214);
        textCol   = Color(32, 32, 32);
    }

    SolidBrush brush(bgCol);
    g.FillPath(&brush, path);

    Pen pen(borderCol, 1.0f);
    g.DrawPath(&pen, path);
    delete path;

    // 绘制按钮文字
    CString text;
    GetWindowTextW(text);

    FontFamily fontFamily(UiLanguage::Text(IDS_UI_FONT).c_str());
    Gdiplus::Font font(&fontFamily, 9.0f, m_isAccent ? FontStyleBold : FontStyleRegular, UnitPoint);

    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);

    SolidBrush textBrush(textCol);
    g.DrawString(text.GetString(), -1, &font, rF, &sf, &textBrush);

    pDC->BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

// ====================================================================
// CModernTabBar 实现
// ====================================================================
CModernTabBar::CModernTabBar()
{
    CGdiplusScope::EnsureInit();
}

void CModernTabBar::AddTab(const std::wstring& title)
{
    m_tabs.push_back(title);
    if (m_hWnd) Invalidate();
}

void CModernTabBar::SetCurSel(int sel)
{
    if (sel >= 0 && sel < (int)m_tabs.size())
    {
        m_curSel = sel;
        if (m_hWnd) Invalidate();
        if (m_onSelChanged) m_onSelChanged(sel);
    }
}

BEGIN_MESSAGE_MAP(CModernTabBar, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
END_MESSAGE_MAP()

BOOL CModernTabBar::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

void CModernTabBar::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_isTracking)
    {
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, m_hWnd, 0 };
        TrackMouseEvent(&tme);
        m_isTracking = true;
    }

    int oldHover = m_hoverSel;
    m_hoverSel = -1;

    CRect rc; GetClientRect(&rc);
    if (!m_tabs.empty())
    {
        int tabWidth = rc.Width() / (int)m_tabs.size();
        int idx = point.x / tabWidth;
        if (idx >= 0 && idx < (int)m_tabs.size())
            m_hoverSel = idx;
    }

    if (oldHover != m_hoverSel)
        Invalidate();

    CWnd::OnMouseMove(nFlags, point);
}

void CModernTabBar::OnMouseLeave()
{
    m_hoverSel = -1;
    m_isTracking = false;
    Invalidate();
    CWnd::OnMouseLeave();
}

void CModernTabBar::OnLButtonDown(UINT nFlags, CPoint point)
{
    CRect rc; GetClientRect(&rc);
    if (!m_tabs.empty())
    {
        int tabWidth = rc.Width() / (int)m_tabs.size();
        int idx = point.x / tabWidth;
        if (idx >= 0 && idx < (int)m_tabs.size())
        {
            SetCurSel(idx);
        }
    }
    CWnd::OnLButtonDown(nFlags, point);
}

void CModernTabBar::OnPaint()
{
    CPaintDC dc(this);
    CRect rc; GetClientRect(&rc);

    CDC memDC; memDC.CreateCompatibleDC(&dc);
    CBitmap memBmp; memBmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&memBmp);

    // 填充底框胶囊背景 (Soft Grey)
    memDC.FillSolidRect(0, 0, rc.Width(), rc.Height(), FluentTheme::WindowBg);

    Graphics g(memDC.GetSafeHdc());
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    RectF containerRect(0.5f, 0.5f, (float)rc.Width() - 1.0f, (float)rc.Height() - 1.0f);
    GraphicsPath* containerPath = CGdiplusScope::CreateRoundRectPath(containerRect, 6.0f);
    SolidBrush contBrush(Color(234, 234, 234));
    g.FillPath(&contBrush, containerPath);
    delete containerPath;

    if (!m_tabs.empty())
    {
        float tabW = (float)rc.Width() / (float)m_tabs.size();
        FontFamily fontFamily(UiLanguage::Text(IDS_UI_FONT).c_str());

        for (int i = 0; i < (int)m_tabs.size(); ++i)
        {
            RectF tabR(tabW * i + 3.0f, 3.0f, tabW - 6.0f, (float)rc.Height() - 6.0f);
            bool isSelected = (i == m_curSel);
            bool isHovered  = (i == m_hoverSel);

            if (isSelected)
            {
                // 纯白圆角浮起卡片底
                GraphicsPath* pill = CGdiplusScope::CreateRoundRectPath(tabR, 5.0f);
                SolidBrush pillBrush(Color(255, 255, 255));
                g.FillPath(&pillBrush, pill);

                // 底部指示条 (Accent line)
                Pen indPen(Color(0, 103, 192), 2.5f);
                g.DrawLine(&indPen, tabR.X + 10.0f, tabR.Y + tabR.Height - 1.0f, tabR.X + tabR.Width - 10.0f, tabR.Y + tabR.Height - 1.0f);
                delete pill;
            }
            else if (isHovered)
            {
                GraphicsPath* pill = CGdiplusScope::CreateRoundRectPath(tabR, 5.0f);
                SolidBrush hoverBrush(Color(245, 245, 245));
                g.FillPath(&hoverBrush, pill);
                delete pill;
            }

            // 绘制文字
            Gdiplus::Font font(&fontFamily, 9.5f, isSelected ? FontStyleBold : FontStyleRegular, UnitPoint);
            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);

            SolidBrush textBrush(isSelected ? Color(0, 103, 192) : Color(70, 70, 70));
            g.DrawString(m_tabs[i].c_str(), -1, &font, tabR, &sf, &textBrush);
        }
    }

    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

// ====================================================================
// CModernSubDlg 实现
// ====================================================================
IMPLEMENT_DYNAMIC(CModernSubDlg, CDialog)

CModernSubDlg::CModernSubDlg(UINT nIDTemplate, CWnd* pParent)
    : CDialog(nIDTemplate, pParent)
{
    CGdiplusScope::EnsureInit();
    m_brWindowBg.CreateSolidBrush(FluentTheme::WindowBg);
    m_brCardBg.CreateSolidBrush(FluentTheme::CardBg);
}

CModernSubDlg::~CModernSubDlg()
{
}

BOOL CModernSubDlg::Create(UINT templateId, CWnd* parent)
{
    const auto resource = UiLanguage::Dialog(templateId);
    return resource && CDialog::CreateIndirect(resource, parent);
}

void CModernSubDlg::AddCard(int leftDlu, int topDlu, int rightDlu, int bottomDlu, const std::wstring& title)
{
    m_cards.push_back({ leftDlu, topDlu, rightDlu, bottomDlu, title });
}

BEGIN_MESSAGE_MAP(CModernSubDlg, CDialog)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

BOOL CModernSubDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    if (!m_defaultUiFont.GetSafeHandle())
    {
        m_defaultUiFont.CreatePointFont(90, UiLanguage::Text(IDS_UI_FONT).c_str());
    }

    // 统一设置所有子控件字体为微软雅黑
    CWnd* pChild = GetWindow(GW_CHILD);
    while (pChild)
    {
        pChild->SetFont(&m_defaultUiFont);
        pChild = pChild->GetWindow(GW_HWNDNEXT);
    }

    return TRUE;
}

BOOL CModernSubDlg::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE; // 防闪烁，由 OnPaint 统一在内存 DC 填充
}

void CModernSubDlg::OnPaint()
{
    CPaintDC dc(this);
    CRect clientRc;
    GetClientRect(&clientRc);

    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap memBmp;
    memBmp.CreateCompatibleBitmap(&dc, clientRc.Width(), clientRc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&memBmp);

    // 1. 窗口柔和底色填充
    memDC.FillSolidRect(&clientRc, FluentTheme::WindowBg);

    // 2. GDI+ 卡片绘制
    Graphics g(memDC.GetSafeHdc());
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    for (const auto& card : m_cards)
    {
        CRect dluRc(card.leftDlu, card.topDlu, card.rightDlu, card.bottomDlu);
        MapDialogRect(&dluRc);

        RectF cardRectF((float)dluRc.left, (float)dluRc.top, (float)dluRc.Width(), (float)dluRc.Height());
        CGdiplusScope::DrawModernCard(g, cardRectF, card.title.c_str());
    }

    dc.BitBlt(0, 0, clientRc.Width(), clientRc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

HBRUSH CModernSubDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (nCtlColor == CTLCOLOR_STATIC)
    {
        pDC->SetBkMode(TRANSPARENT);
        pDC->SetTextColor(FluentTheme::TextPrimary);
        return (HBRUSH)m_brCardBg.GetSafeHandle();
    }
    else if (nCtlColor == CTLCOLOR_EDIT || nCtlColor == CTLCOLOR_LISTBOX)
    {
        pDC->SetBkColor(FluentTheme::CardBg);
        pDC->SetTextColor(FluentTheme::TextPrimary);
        return (HBRUSH)m_brCardBg.GetSafeHandle();
    }
    else if (nCtlColor == CTLCOLOR_DLG)
    {
        return (HBRUSH)m_brWindowBg.GetSafeHandle();
    }

    return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}
