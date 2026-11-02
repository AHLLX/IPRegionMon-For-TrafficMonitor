#pragma once
#include <Windows.h>
#include <functional>
#include <memory>
#include <utility>

#ifndef _DLL
#error ModuleTask requires the dynamically linked CRT (/MD or /MDd).
#endif

// A task owns a DLL reference, not the HWND or the lifetime of its caller.
namespace ModuleTask
{
    namespace Detail
    {
        struct Context
        {
            HMODULE module;
            std::function<void()> work;
        };

        inline DWORD WINAPI Run(void* parameter) noexcept
        {
            const HMODULE module = static_cast<Context*>(parameter)->module;
            {
                std::unique_ptr<Context> context(static_cast<Context*>(parameter));
                try { context->work(); }
                catch (...) { /* Task owners provide their own error/result state. */ }
            }
            // Nothing requiring C++ cleanup may remain alive across this call.
            // /MD provides CRT per-thread cleanup on native thread exit.
            FreeLibraryAndExitThread(module, 0);
        }

#ifdef IPREGIONMON_TESTING
        using ThreadFactory = decltype(&CreateThread);
        inline ThreadFactory& Factory()
        {
            static ThreadFactory factory = &CreateThread;
            return factory;
        }
#endif
    }

    inline bool Start(std::function<void()> work) noexcept
    {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(&Detail::Run), &module))
            return false;
        try
        {
            auto context = std::unique_ptr<Detail::Context>(
                new Detail::Context{ module, std::move(work) });
#ifdef IPREGIONMON_TESTING
            HANDLE thread = Detail::Factory()(nullptr, 0, &Detail::Run, context.get(), 0, nullptr);
#else
            HANDLE thread = CreateThread(nullptr, 0, &Detail::Run, context.get(), 0, nullptr);
#endif
            if (thread)
            {
                context.release();
                CloseHandle(thread);
                return true;
            }
        }
        catch (...) {}
        // The calling host still owns its reference until Start has returned.
        FreeLibrary(module);
        return false;
    }
}
