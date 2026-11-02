#pragma once
#include "PluginInterface.h"
#include "CustomDrawItem.h"
#include <atomic>

class CIPRegionMon : public ITMPlugin
{
private:
    CIPRegionMon();

public:
    static CIPRegionMon& Instance();

    // 通过 ITMPlugin 继承
    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;
    virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    virtual const wchar_t* GetTooltipInfo() override;
    virtual void OnInitialize(ITrafficMonitor* app) override;

private:
    // 两个项目：直、代
    CCustomDrawItem m_direct_item{ false };
    CCustomDrawItem m_proxy_item{ true };

    static CIPRegionMon m_instance;
    std::atomic<bool> m_initialized{ false }, m_config_ready{ false };
};

#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
