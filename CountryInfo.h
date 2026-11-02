#pragma once
#include <Windows.h>
#include <string>
#include <cwchar>

namespace CountryInfo
{
    struct Value { std::wstring code, name; };

    inline std::wstring Code(std::wstring value)
    {
        const auto first = value.find_first_not_of(L" \t\r\n");
        if (first == std::wstring::npos) return {};
        value = value.substr(first, value.find_last_not_of(L" \t\r\n") - first + 1);
        if (value.size() != 2) return {};
        for (auto& letter : value)
        {
            if (letter >= L'a' && letter <= L'z') letter -= L'a' - L'A';
            if (letter < L'A' || letter > L'Z') return {};
        }
        return value;
    }

    namespace Detail
    {
        struct Search
        {
            const std::wstring& code;
            const std::wstring& name;
            wchar_t foundCode[8]{}, foundName[128]{};
        };

        inline bool Equal(const std::wstring& value, const wchar_t* candidate)
        {
            return CompareStringOrdinal(value.c_str(), -1, candidate, -1, TRUE) == CSTR_EQUAL;
        }

        inline BOOL CALLBACK FindLocale(LPWSTR locale, DWORD, LPARAM parameter) noexcept
        {
            auto& search = *reinterpret_cast<Search*>(parameter);
            wchar_t code[8]{}, english[128]{};
            if (!GetLocaleInfoEx(locale, LOCALE_SISO3166CTRYNAME, code, _countof(code))) return TRUE;
            GetLocaleInfoEx(locale, LOCALE_SENGLISHCOUNTRYNAME, english, _countof(english));
            bool matches = !search.code.empty() && Equal(search.code, code);
            if (search.code.empty())
            {
                wchar_t native[128]{}, localized[128]{};
                GetLocaleInfoEx(locale, LOCALE_SNATIVECOUNTRYNAME, native, _countof(native));
                GetLocaleInfoEx(locale, LOCALE_SLOCALIZEDCOUNTRYNAME, localized, _countof(localized));
                matches = Equal(search.name, english) || Equal(search.name, native) || Equal(search.name, localized);
            }
            if (!matches) return TRUE;
            wcscpy_s(search.foundCode, code);
            wcscpy_s(search.foundName, english);
            return FALSE;
        }
    }

    inline Value Resolve(const std::wstring& providerCode, const std::wstring& country)
    {
        Value result{ Code(providerCode), country };
        const auto countryAsCode = Code(country);
        if (result.code.empty()) result.code = countryAsCode;
        if (!result.code.empty() && !country.empty() && countryAsCode.empty()) return result;
        if (result.code.empty() && country.empty()) return result;

        // Older saved API URLs may omit countryCode. Resolve their country name
        // using Windows locale data; never guess from the first two letters.
        Detail::Search search{ result.code, country };
        EnumSystemLocalesEx(&Detail::FindLocale, LOCALE_SPECIFICDATA,
            reinterpret_cast<LPARAM>(&search), nullptr);
        if (result.code.empty()) result.code = Code(search.foundCode);
        if ((country.empty() || !countryAsCode.empty()) && search.foundName[0])
            result.name = search.foundName;
        return result;
    }
}
