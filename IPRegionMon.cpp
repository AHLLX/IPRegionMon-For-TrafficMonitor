#include "pch.h"
#include "IPRegionMon.h"
#include "DataManager.h"
#include "OptionsDlg.h"
#include "UiLanguage.h"

#include <thread>

CIPRegionMon CIPRegionMon::m_instance;

CIPRegionMon::CIPRegionMon() {}

CIPRegionMon& CIPRegionMon::Instance() { return m_instance; }

IPluginItem* CIPRegionMon::GetItem(int index)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    switch (index)
    {
    case 0: return &m_direct_item;   // 直
    case 1: return &m_proxy_item;    // 代
    default: return nullptr;
    }
}

void CIPRegionMon::DataRequired()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    g_data.SwapBuffers();
    g_data.UpdateIpInfoIfNeeded();
}

const wchar_t* CIPRegionMon::GetInfo(PluginInfoIndex index)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    switch (index)
    {
    case TMI_NAME: return g_data.StringRes(IDS_PLUGIN_NAME);
    case TMI_DESCRIPTION: return g_data.StringRes(IDS_PLUGIN_DESCRIPTION);
    case TMI_AUTHOR: return L"HiTech_NinJa";
    case TMI_COPYRIGHT: return L"Copyright (C) by HiTech_NinJa 2025-2026";
    case TMI_VERSION: return L"2.1.1";
    case ITMPlugin::TMI_URL: return L"https://github.com/AHLLX/IPRegionMon-For-TrafficMonitor";
    default: return L"";
    }
}

ITMPlugin::OptionReturn CIPRegionMon::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    // 跨模块 HWND 包装：Attach/Detach 创建临时 CWnd 壳，不触碰 MFC 句柄映射
    CWnd parentWnd;
    parentWnd.Attach((HWND)hParent);
    COptionsDlg dlg(&parentWnd);
    INT_PTR result = dlg.DoModal();
    parentWnd.Detach(); // 归还所有权，不析构原窗口

    return (result == IDOK)
        ? ITMPlugin::OR_OPTION_CHANGED
        : ITMPlugin::OR_OPTION_UNCHANGED;
}

void CIPRegionMon::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    switch (index)
    {
    case ITMPlugin::EI_DRAW_TASKBAR_WND:
        g_data.SetDrawContext(data && data[0] == L'1');
        break;
    case ITMPlugin::EI_CONFIG_DIR:
        g_data.LoadConfig(std::wstring(data));
        m_config_ready = true;
        if (m_initialized) g_data.ForceUpdateAsync();
        break;
    default: break;
    }
}

const wchar_t* CIPRegionMon::GetTooltipInfo()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return g_data.GetTooltipText();
}

void CIPRegionMon::OnInitialize(ITrafficMonitor* app)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    UiLanguage::Initialize(app ? app->GetLanguageId() : UiLanguage::English);
    // v7+ hosts send EI_CONFIG_DIR before OnInitialize. Bind the UI language
    // before the first worker can format errors or tooltip text.
    m_initialized = true;
    if (m_config_ready) g_data.ForceUpdateAsync();
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CIPRegionMon::Instance();
}
