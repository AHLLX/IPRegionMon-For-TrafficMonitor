#pragma once
#include <Windows.h>
#include <string>
#include <cwchar>

// Select this DLL's existing resources without changing the host thread locale.
namespace UiLanguage
{
    constexpr LANGID English = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
    constexpr LANGID Chinese = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
    LANGID Get();
    void Initialize(LANGID hostLanguage);
    std::wstring Text(UINT id, LANGID language = Get());
    LPCDLGTEMPLATE Dialog(UINT id);

    template<class... Args>
    std::wstring Format(UINT id, Args... arguments)
    {
        const auto format = Text(id);
        const int length = _scwprintf(format.c_str(), arguments...);
        if (length < 0) return {};
        std::wstring value(static_cast<size_t>(length) + 1, L'\0');
        swprintf_s(&value[0], value.size(), format.c_str(), arguments...);
        value.resize(length);
        return value;
    }
}
