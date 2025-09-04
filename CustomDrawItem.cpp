#include "pch.h"
#include "CustomDrawItem.h"
#include "DataManager.h"

const wchar_t* CCustomDrawItem::GetItemName() const
{
    // 显示设置中的名称：区分直/代
    return g_data.StringRes(m_is_proxy ? IDS_PROXY_ITEM_NAME : IDS_DIRECT_ITEM_NAME);
}

const wchar_t* CCustomDrawItem::GetItemId() const
{
    // 两个不同的ID，确保可以分别在“显示项目”里选择
    return m_is_proxy ? L"4Tc21hGS_proxy" : L"4Tc21hGS_direct";
}

const wchar_t* CCustomDrawItem::GetItemLableText() const
{
    return m_is_proxy ? L"代:" : L"直:";
}

const wchar_t* CCustomDrawItem::GetItemValueText() const
{
    const std::wstring& place = m_is_proxy ? g_data.m_place_proxy : g_data.m_place_direct;
    const std::wstring& ip    = m_is_proxy ? g_data.m_public_ip_proxy : g_data.m_public_ip_direct;
    if (!place.empty()) return place.c_str();
    return ip.c_str();
}

const wchar_t* CCustomDrawItem::GetItemValueSampleText() const
{
    // 示例用于计算宽度：只显示地区即可
    return m_is_proxy ? L"US CA Los Angeles" : L"CN 上海";
}
