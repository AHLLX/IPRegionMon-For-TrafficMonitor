#include "../../ModuleTask.h"
#include <stdexcept>

struct Completion
{
    HANDLE event;
    ~Completion() { SetEvent(event); }
};

extern "C" __declspec(dllexport) BOOL StartTask(
    HANDLE started, HANDLE release, HANDLE cleaned, BOOL fail)
{
    return ModuleTask::Start([=]() {
        Completion completion{ cleaned };
        SetEvent(started);
        WaitForSingleObject(release, 10000);
        if (fail)
            throw std::runtime_error("expected worker failure");
    });
}
