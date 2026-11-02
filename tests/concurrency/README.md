# Concurrency and module lifetime tests

From the repository root:

    .\tests\concurrency\Run-Tests.ps1

Requires MSVC x64/x86, MFC and a Windows SDK. All artifacts are written to a new
ignored directory under Bin. No installed plugin, user configuration or
TrafficMonitor process is used. The test DLL has extra exports enabled only by
IPRegionMonConcurrencyTests=true; normal plugin builds do not contain them.

The module test uses the same ModuleTask.h helper as the plugin. It starts two
blocked workers, releases the host's LoadLibrary reference, checks that the
module remains loaded, then releases the workers one at a time. The last worker
throws; its stack and callable must still be destroyed before the DLL unloads.
101 load/unload cycles also check that the process handle count is unchanged.

The state test builds the real plugin with a test-access fixture. A loopback
HTTP server returns controlled IP results and blocks a response on an event.
64 callers race to start the actual update; only one request may begin. Thread
creation failure and an exception are injected at ModuleTask's test-only factory.
An exception injected inside the real update must also release the busy gate.
Settings must remain writable while the response is blocked. That worker must
finish with its initial API/target snapshot, even after the settings change.
Eight readers then inspect snapshots and getter strings during 20 updates.
Previously returned pointers on the host thread must remain unchanged.
Finally, the host releases its DLL reference during a real blocked HTTP request
and checks eventual unloading after releasing the response.

Both actual settings pages are also created as hidden test windows. Their real
test buttons start requests against the loopback server (a local HTTP proxy for
the proxy page). The UI thread destroys each page before releasing the response.
The page must release its result state while the worker still owns it, and the
DLL must unload after the result has been discarded. The workers capture no
HWND/CWnd and post no pointer-bearing messages. A short WM_TIMER poll runs only
while the page owns an active result; existing displayed results are unchanged.
The proxy page's real timer handler is also exercised with a completed loopback
request and an injected launch failure; its status and button must be restored.

Each native test process has a 30-second watchdog and separate stdout/stderr
logs. A deadlock fails the test rather than hanging the runner indefinitely.

No public internet endpoints are used by these successful tests. They do not
call LoadConfig or SaveConfig and do not read or write a user's INI.

## Data and pointer lifetime

Settings and results are copied under CDataManager's mutex. Network work uses
local copies. A completed result is published as one snapshot; the most recent
display format is applied under the short publication lock.

Each calling OS thread has separate Direct, Proxy and tooltip return buffers
owned by CDataManager. A pointer survives updates and calls from other threads,
and calls to other getters. It is valid until that thread calls the same getter
again, exits, or the DLL unloads. There is no host API for retaining pointers
indefinitely or releasing them explicitly. Reader buffers are retained until
unload, with one cache entry per distinct reader thread ID. Fixed TrafficMonitor
reader threads therefore do not accumulate a string for every update.

The buffers deliberately do not use non-trivial thread_local destructors that
could execute after a DLL has unloaded. Resource strings are immutable once
inserted into their protected map.

Configuration writes remain in the existing explicit UI save paths. The static
CDataManager destructor no longer performs configuration I/O under loader lock.

## Module lifetime contract

ModuleTask acquires its own checked GetModuleHandleEx reference before creating a
native thread. The callable and its captures are destroyed before
FreeLibraryAndExitThread releases that reference and exits the worker. This uses
the dynamically linked CRT (/MD or /MDd), which performs per-thread cleanup on
native thread exit. There is no join, wait or I/O from a DLL destructor.

An ordinary FreeLibrary request therefore defers unloading until existing tasks
finish. It does not itself cancel an in-flight request. GET and HEAD now have
per-request deadlines and drain WinHTTP callbacks before releasing task state;
see [network tests](../network/README.md). An entire sequential update may take
longer than one request budget because it includes fallback sources and targets.
The host must stop issuing plugin calls before releasing its own reference.
Pointers returned by the plugin cannot survive actual module unloading.
Forced process termination is not a graceful task-shutdown mechanism.

The installed MSVC std::thread implementation also calls _beginthreadex, and the
inspected UCRT implementation already retains the procedure's module. The
explicit helper avoids depending on that implementation detail or using detach
without a locally checked lifetime guarantee.

References:

- [GetModuleHandleEx](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandleexw)
- [FreeLibraryAndExitThread](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-freelibraryandexitthread)
- [DLL best practices and loader lock](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices)
