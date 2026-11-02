#include <Windows.h>
#include <cstdio>

static bool WaitUntilUnloaded(const wchar_t* path)
{
    for (int i = 0; i < 500; ++i)
    {
        if (!GetModuleHandleW(path))
            return true;
        Sleep(10);
    }
    return false;
}

static bool Cycle(const wchar_t* path)
{
    HMODULE module = LoadLibraryW(path);
    if (!module)
        return false;
    using Start = BOOL (*)(HANDLE, HANDLE, HANDLE, BOOL);
    auto start = reinterpret_cast<Start>(GetProcAddress(module, "StartTask"));
    HANDLE started[] = { CreateEventW(nullptr, TRUE, FALSE, nullptr), CreateEventW(nullptr, TRUE, FALSE, nullptr) };
    HANDLE release[] = { CreateEventW(nullptr, TRUE, FALSE, nullptr), CreateEventW(nullptr, TRUE, FALSE, nullptr) };
    HANDLE cleaned[] = { CreateEventW(nullptr, TRUE, FALSE, nullptr), CreateEventW(nullptr, TRUE, FALSE, nullptr) };
    if (!start || !start(started[0], release[0], cleaned[0], FALSE) ||
        !start(started[1], release[1], cleaned[1], TRUE) ||
        WaitForMultipleObjects(2, started, TRUE, 5000) != WAIT_OBJECT_0)
        return false;

    FreeLibrary(module);
    if (!GetModuleHandleW(path))
        return false;
    SetEvent(release[0]);
    if (WaitForSingleObject(cleaned[0], 5000) != WAIT_OBJECT_0 || !GetModuleHandleW(path))
        return false;
    SetEvent(release[1]);
    const bool passed = WaitForSingleObject(cleaned[1], 5000) == WAIT_OBJECT_0 && WaitUntilUnloaded(path);
    for (HANDLE handle : started) CloseHandle(handle);
    for (HANDLE handle : release) CloseHandle(handle);
    for (HANDLE handle : cleaned) CloseHandle(handle);
    return passed;
}

int wmain(int argc, wchar_t** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc != 2 || !Cycle(argv[1]))
        return 1;
    DWORD before = 0, after = 0;
    GetProcessHandleCount(GetCurrentProcess(), &before);
    for (int i = 0; i < 100; ++i)
        if (!Cycle(argv[1]))
            return 2;
    GetProcessHandleCount(GetCurrentProcess(), &after);
    std::printf("PASS: 101 unload cycles, two workers/cycle, exception cleanup; handles %lu -> %lu\n", before, after);
    return before == after ? 0 : 3;
}
