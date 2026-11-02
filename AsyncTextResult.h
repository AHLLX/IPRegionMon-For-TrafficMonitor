#pragma once
#include "ModuleTask.h"
#include <atomic>
#include <string>

// One worker publishes once; only the owning UI thread consumes the result.
// Closing a window merely releases its shared_ptr, without a callback into it.
class AsyncTextResult
{
    std::wstring m_text;
    std::atomic<bool> m_ready{ false };
public:
    static std::shared_ptr<AsyncTextResult> Start(std::function<std::wstring()> work) noexcept
    {
        try
        {
            auto result = std::make_shared<AsyncTextResult>();
            if (ModuleTask::Start([result, work = std::move(work)]() {
                try { result->m_text = work(); }
                catch (...) {}
                result->m_ready.store(true, std::memory_order_release);
            }))
                return result;
        }
        catch (...) {}
        return {};
    }

    bool Take(std::wstring& text)
    {
        if (!m_ready.load(std::memory_order_acquire))
            return false;
        text = std::move(m_text);
        return true;
    }
};
