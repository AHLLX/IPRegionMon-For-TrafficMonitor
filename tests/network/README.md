# Network deadlines and lifecycle regression tests

Run from the repository root with MSVC x64, MFC and the Windows SDK installed:

```powershell
.\tests\network\Run-Tests.ps1
```

The script discovers Visual Studio with vswhere, builds a standalone loopback
host and the real Release|x64 plugin with test-only exports, then runs a
90-second watchdog. Artifacts and logs go to a new ignored Bin/network-* folder.
No installed TrafficMonitor, user INI, certificate store or system network
configuration is changed. Deployment post-build events are explicitly disabled.

## Reproduction and expectations

The server advertises a large body and sends one byte every 50 ms for 6.5 seconds.
Before the fix, the actual GET returned success after 6547 ms with an incomplete
116-byte body. The initial failing run is preserved locally under the ignored
Bin directory. `-ReproOnly` runs just
this case, using the same assertions; it should pass on the corrected code.

GET has one 4000 ms budget; HEAD has one 1200 ms budget. Tests require failures
near those deadlines for a slow body, slow headers and a silent server, and
verify that the server sees the connection close. They also check ordinary
GET/HEAD success, refused loopback connections, unhandled redirects, oversized
bodies and rejection of an incomplete Content-Length body with no partial output.
Timing tolerances allow scheduling
and cleanup overhead, not a second full network timeout.

Five cycles release the host's DLL reference during an active slow GET, check
that the module stays loaded, then require cancellation and eventual unloading.
A test-only EOF callback gate proves that the worker and DLL cannot finish
before the final callback returns. Three separate EXE children return normally
while an actual plugin request is active; the parent checks exit code 0 and
socket closure. These children are isolated hosts, not the TrafficMonitor GUI.
They do not demonstrate graceful worker completion during ExitProcess.

## Transport contract

BoundedHttp.h is shared by the real GET and HEAD paths. It uses asynchronous
WinHTTP internally, while the existing ModuleTask worker serially waits for
completion notifications. It starts no application-owned timer or helper thread.
All operations consume the same monotonic deadline, including DNS/connect,
headers, redirects and body transfer. Deadline expiry closes the request only
after the initiating WinHTTP call has returned. Callback code only publishes
completion; it never issues another request operation.

Cleanup closes request, connection and session, then waits for the session's
WINHTTP_OPTION_UNLOAD_NOTIFY_EVENT. Unlike HANDLE_CLOSING, this signals after
the last callback has returned, so ModuleTask may safely release its DLL reference.
Buffers/events stay alive throughout cancellation. If that option cannot be
installed, the request fails before registering a callback. No cleanup is added
to DllMain or static destructors, and there is no detach or forced worker kill.

The deadline bounds network work; OS scheduling and callback cancellation/drain
can add latency. Drain deliberately waits rather than freeing live callback
state on a second timeout. This is not a hard real-time guarantee against an OS
or WinHTTP teardown failure. Each request has its own budget: a whole update may
take longer because fallback providers and configured latency targets are
processed sequentially. FreeLibrary defers unloading rather than signalling
cancellation of the entire update. A fixed 8 KiB buffer and 1 MiB body ceiling
bound allocations for an individual API response. HTTP status errors, read errors,
truncation and timeouts produce no usable partial body.

References:

- [WinHTTP concurrency and cancellation](https://learn.microsoft.com/en-us/windows/win32/winhttp/concurrency-in-winhttp)
- [Closing async request handles](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpclosehandle)
- [Session unload notification](https://github.com/MicrosoftDocs/win32/blob/docs/desktop-src/WinHttp/option-flags.md#winhttp_option_unload_notify_event)

## HTTPS checks

After the network run, pass its printed artifact directory:

```powershell
.\tests\network\Run-HttpsTests.ps1 -NetworkArtifacts .\Bin\network-<id>
# Optional, depends on public DNS, routing, endpoint uptime and the request budgets:
.\tests\network\Run-HttpsTests.ps1 -NetworkArtifacts .\Bin\network-<id> -IncludeInternet
```

Windows PowerShell 5.1 with .NET Framework 4.7.2+ provides the TLS fixture APIs.
The local fixture generates a self-signed localhost certificate, returns HTTP
over SslStream, and verifies that both real GET and HEAD reject it specifically
with ERROR_WINHTTP_SECURE_FAILURE (12175). Before the fix, GET accepted that
certificate and returned the fixture body. The test does not add certificates
to Windows stores or change trust policy. Framework Schannel requires a
temporary imported private-key container; PersistKeySet is not used, and normal
fixture disposal removes it. The runner signals the fixture to stop even when
an assertion fails, and only kills its own process if bounded cleanup fails.

Optional public probes check a valid certificate, expired certificate, wrong
hostname, untrusted issuer and an HTTPS-to-HTTP redirect. They use GET's 4000 ms
budget to separate TLS classification from HEAD's shorter latency budget.
A timeout or connection failure is a failed probe, never a passing certificate
or downgrade check. Logs from every attempted case are preserved and failures
are aggregated. HEAD's local negative test remains part of the deterministic
suite. Successful Internet probes do not establish offline revocation behavior.

Production transport uses WinHTTP's normal chain, hostname, validity-period and
usage checks. No ignore-certificate flags are installed. TLS 1.2/1.3 are selected
explicitly; if the OS rejects TLS 1.3, only TLS 1.2 is retried. Failure to configure
TLS or the no-downgrade redirect policy fails the request. An unfollowed redirect
is rejected for GET and HEAD, rather than counted as a successful latency result.
There is no automatic HTTPS-to-HTTP URL rewrite. Revocation checking is left at
the WinHTTP/Windows default; this change does not force an online CRL check or
add certificate pinning.

New settings use https://ipwho.is/, which was already supported by the parser.
Built-in fallback sources are HTTPS-only: ipwho.is, api.ip.sb and ipinfo.io.
The free ip-api.com endpoint is removed from defaults/fallback because its
provider documents that free HTTPS is unavailable; changing just its scheme
would break compatibility. No saved user URL or latency target is rewritten.
An explicitly configured HTTP URL, including a legacy saved default, still uses
HTTP. Users who need encryption for those URLs must select an HTTPS provider.
Availability, rate limits, localization and threat-data fields vary by provider;
TLS does not make their geolocation or security claims authoritative.

- [WinHTTP TLS and redirect options](https://learn.microsoft.com/en-us/windows/win32/winhttp/option-flags)
- [ip-api free HTTPS limitation](https://ip-api.com/docs/api:json)
- [ipwho.is HTTPS endpoint](https://ipwhois.io/documentation)
