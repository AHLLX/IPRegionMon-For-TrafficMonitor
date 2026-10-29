#include <windows.h>
#include <cstdio>
#include <cwchar>
#include "PluginInterface.h"

static int Probe(ITMPlugin* plugin, bool fixture)
{
    __try
    {
        if (!plugin || plugin->GetAPIVersion() != 8)
            return 20;
        std::printf("API=8\n");

        const wchar_t* ids[] = { L"4Tc21hGS_direct", L"4Tc21hGS_proxy" };
        const int count = fixture ? 1 : 2;
        for (int index = 0; index < count; ++index)
        {
            IPluginItem* item = plugin->GetItem(index);
            if (!item || !item->GetItemId())
                return 21;
            const wchar_t* expected = fixture ? L"fixture" : ids[index];
            if (std::wcscmp(item->GetItemId(), expected) != 0)
                return 22;
            std::printf("item[%d]: ID OK\n", index);

            // Current TrafficMonitor calls this slot for every API v8 item.
            const int result = item->IsDoubleLineExclusive();
            std::printf("item[%d]: IsDoubleLineExclusive=%d\n", index, result);
            if (result != 0)
                return 12;
        }
        return plugin->GetItem(count) == nullptr ? 0 : 23;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        std::printf("SEH_EXCEPTION=0x%08lX\n", GetExceptionCode());
        return 10;
    }
}

int wmain(int argc, wchar_t** argv)
{
    // A broken negative-control DLL must not open a crash dialog.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc != 3 ||
        (std::wcscmp(argv[2], L"fixture") != 0 && std::wcscmp(argv[2], L"plugin") != 0))
        return 2;

    HMODULE module = LoadLibraryW(argv[1]);
    if (!module)
    {
        std::printf("LOAD_ERROR=%lu\n", GetLastError());
        return 3;
    }
    auto getPlugin = reinterpret_cast<ITMPlugin* (*)()>(
        GetProcAddress(module, "TMPluginGetInstance"));
    const int result = getPlugin
        ? Probe(getPlugin(), std::wcscmp(argv[2], L"fixture") == 0) : 4;
    // Do not call OnInitialize, EI_CONFIG_DIR, DataRequired or ShowOptionsDialog.
    FreeLibrary(module);
    return result;
}
