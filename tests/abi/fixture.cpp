#include "PluginInterface.h"

class FixtureItem final : public IPluginItem
{
public:
    const wchar_t* GetItemName() const override { return L"Fixture"; }
    const wchar_t* GetItemId() const override { return L"fixture"; }
    const wchar_t* GetItemLableText() const override { return L"Fixture:"; }
    const wchar_t* GetItemValueText() const override { return L"0"; }
    const wchar_t* GetItemValueSampleText() const override { return L"0"; }
};

class FixturePlugin final : public ITMPlugin
{
    FixtureItem item;

public:
    IPluginItem* GetItem(int index) override { return index == 0 ? &item : nullptr; }
    void DataRequired() override {}
    const wchar_t* GetInfo(PluginInfoIndex) override { return L"Fixture"; }
};

static FixturePlugin plugin;

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    return &plugin;
}
