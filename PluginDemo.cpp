#include "pch.h"
#include "PluginDemo.h"
#include "DataManager.h"
#include "OptionsDlg.h"

CPluginDemo CPluginDemo::m_instance;

CPluginDemo::CPluginDemo()
{
}

CPluginDemo& CPluginDemo::Instance()
{
    return m_instance;
}

IPluginItem* CPluginDemo::GetItem(int index)
{
    switch (index)
    {
    case 0:
        return &m_custom_draw_item; // 仅显示 IP 区域
    default:
        break;
    }
    return nullptr;
}

void CPluginDemo::DataRequired()
{
    // 定时更新 IP 区域
    g_data.UpdateIpInfoIfNeeded();
}

const wchar_t* CPluginDemo::GetInfo(PluginInfoIndex index)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    static CString str;
    switch (index)
    {
    case TMI_NAME:
        str.LoadString(IDS_PLUGIN_NAME);
        return str.GetString();
    case TMI_DESCRIPTION:
        str.LoadString(IDS_PLUGIN_DESCRIPTION);
        return str.GetString();
    case TMI_AUTHOR:
        return L"HiTech_NinJa";
    case TMI_COPYRIGHT:
        return L"Copyright (C) by HiTech_NinJa 2025";
    case TMI_VERSION:
        return L"1.0";
    case ITMPlugin::TMI_URL:
        return L"https://github.com/HiTech_NinJa/TrafficMonitor";
        break;
    default:
        break;
    }
    return L"";
}

ITMPlugin::OptionReturn CPluginDemo::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    COptionsDlg dlg(CWnd::FromHandle((HWND)hParent));
    dlg.m_data = CDataManager::Instance().m_setting_data;
    if (dlg.DoModal() == IDOK)
    {
        CDataManager::Instance().m_setting_data = dlg.m_data;
        return ITMPlugin::OR_OPTION_CHANGED;
    }
    return ITMPlugin::OR_OPTION_UNCHANGED;
}


void CPluginDemo::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    switch (index)
    {
    case ITMPlugin::EI_CONFIG_DIR:
        //从配置文件读取配置
        g_data.LoadConfig(std::wstring(data));

        break;
    default:
        break;
    }
}

const wchar_t* CPluginDemo::GetTooltipInfo()
{
    return g_data.m_tooltip.c_str();
}

ITMPlugin* TMPluginGetInstance()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    return &CPluginDemo::Instance();
}
