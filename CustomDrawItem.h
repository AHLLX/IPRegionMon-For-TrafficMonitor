#pragma once
#include "PluginInterface.h"

class CCustomDrawItem : public IPluginItem
{
public:
    explicit CCustomDrawItem(bool is_proxy) : m_is_proxy(is_proxy) {}

    // IPluginItem
    virtual const wchar_t* GetItemName() const override;         // 统一名称
    virtual const wchar_t* GetItemId() const override;           // 唯一ID（直/代不同）
    virtual const wchar_t* GetItemLableText() const override;    // 标签文本：直/代
    virtual const wchar_t* GetItemValueText() const override;    // 地区（无则IP）
    virtual const wchar_t* GetItemValueSampleText() const override; // 示例地区

private:
    bool m_is_proxy{ false };
};
