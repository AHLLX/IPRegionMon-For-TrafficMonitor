#include "pch.h"
#include "UiLanguage.h"
#include <atomic>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    std::atomic<LANGID> language{ UiLanguage::English };
    const void* Resource(LPCWSTR type, UINT id, LANGID selected)
    {
        const auto module = reinterpret_cast<HMODULE>(&__ImageBase);
        HRSRC resource = FindResourceExW(module, type, MAKEINTRESOURCEW(id), selected);
        if (!resource && selected != UiLanguage::English)
            resource = FindResourceExW(module, type, MAKEINTRESOURCEW(id), UiLanguage::English);
        if (!resource) return nullptr;
        return LockResource(LoadResource(module, resource));
    }
}

LANGID UiLanguage::Get() { return language.load(); }

void UiLanguage::Initialize(LANGID hostLanguage)
{
    // Only Simplified Chinese and English templates are shipped. Other host
    // languages use English; Chinese variants reuse the existing Chinese UI.
    language = PRIMARYLANGID(hostLanguage) == LANG_CHINESE ? Chinese : English;
}

std::wstring UiLanguage::Text(UINT id, LANGID selected)
{
    const auto* text = static_cast<const wchar_t*>(Resource(RT_STRING, (id >> 4) + 1, selected));
    if (!text) return {};
    for (UINT index = 0; index < (id & 15); ++index) text += 1 + *text;
    if (!*text && selected != English) return Text(id, English);
    return std::wstring(text + 1, *text);
}

LPCDLGTEMPLATE UiLanguage::Dialog(UINT id)
{
    return static_cast<LPCDLGTEMPLATE>(Resource(RT_DIALOG, id, Get()));
}
