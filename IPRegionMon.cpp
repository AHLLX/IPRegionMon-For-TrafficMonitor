#include "pch.h"
#include "IPRegionMon.h"
#include "DataManager.h"

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

void CIPRegionMon::DataRequired()
{
    g_data.SwapBuffers();
    g_data.UpdateIpInfoIfNeeded();
}

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
    MessageBox(CWnd::FromHandle((HWND)hParent)->GetSafeHwnd(), 
        L"此插件已移除配置界面以确保极致稳定，请直接修改插件目录下的 IPRegionMon.ini 文件来更改配置，修改后重启 TrafficMonitor 即可生效。", 
        L"配置提示", MB_OK | MB_ICONINFORMATION);
    return ITMPlugin::OR_OPTION_UNCHANGED;
}

void CIPRegionMon::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    switch (index)
    {
    case ITMPlugin::EI_CONFIG_DIR:
        g_data.LoadConfig(std::wstring(data));
        g_data.m_is_updating = true;
        std::thread([]{
            g_data.UpdateIpInfoNow();
            g_data.m_is_updating = false;
        }).detach();
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
