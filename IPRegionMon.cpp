#include "pch.h"
#include "IPRegionMon.h"
#include "DataManager.h"
#include "OptionsDlg.h"
#include <thread>

CIPRegionMon CIPRegionMon::m_instance;

CIPRegionMon::CIPRegionMon() {}

CIPRegionMon& CIPRegionMon::Instance() { return m_instance; }

IPluginItem* CIPRegionMon::GetItem(int index)
{
    switch (index)
    {
    case 0: return &m_direct_item;   // 直
    case 1: return &m_proxy_item;    // 代
    default: return nullptr;
    }
}

void CIPRegionMon::DataRequired() { g_data.UpdateIpInfoIfNeeded(); }

const wchar_t* CIPRegionMon::GetInfo(PluginInfoIndex index)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    static CString str;
    switch (index)
    {
    case TMI_NAME: str.LoadString(IDS_PLUGIN_NAME); return str.GetString();
    case TMI_DESCRIPTION: str.LoadString(IDS_PLUGIN_DESCRIPTION); return str.GetString();
    case TMI_AUTHOR: return L"HiTech_NinJa";
    case TMI_COPYRIGHT: return L"Copyright (C) by HiTech_NinJa 2025";
    case TMI_VERSION: return L"1.0";
    case ITMPlugin::TMI_URL: return L"https://github.com/AHLLX/IPRegionMon-For-TrafficMonitor";
    default: return L"";
    }
}

ITMPlugin::OptionReturn CIPRegionMon::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    COptionsDlg dlg(CWnd::FromHandle((HWND)hParent));
    dlg.m_data = CDataManager::Instance().m_setting_data;
    if (dlg.DoModal() == IDOK)
    { CDataManager::Instance().m_setting_data = dlg.m_data; return ITMPlugin::OR_OPTION_CHANGED; }
    return ITMPlugin::OR_OPTION_UNCHANGED;
}

void CIPRegionMon::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    switch (index)
    {
    case ITMPlugin::EI_CONFIG_DIR:
        g_data.LoadConfig(std::wstring(data));
        std::thread([]{ g_data.UpdateIpInfoNow(); }).detach();
        break;
    default: break;
    }
}

const wchar_t* CIPRegionMon::GetTooltipInfo() { return g_data.m_tooltip.c_str(); }

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CIPRegionMon::Instance();
}
