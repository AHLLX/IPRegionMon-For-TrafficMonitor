# Isolated x64 ABI regression test

Run from the repository root with Windows PowerShell 5.1 or PowerShell 7:

    .\tests\abi\Run-AbiTest.ps1

Requires Windows, the MSVC x64/x86 tools and a Windows SDK. MFC is not needed for
the fixture test. Visual Studio is located with vswhere; use -VsInstallPath to
select an installation explicitly. No test framework or project-file changes
are required.

The script creates a unique directory under TEMP for binaries and logs. A custom
-WorkDirectory must be new or empty. It never builds or deploys IPRegionMon.

## Independent reference and negative control

The runner downloads and verifies SHA-256 hashes for two immutable source files:

| Role | Repository and commit | SHA-256 of downloaded bytes |
| --- | --- | --- |
| Official host reference | zhongyang219/TrafficMonitor, 930f17533d6098989ebad62f210aa97d75ef174b | E11A2C07B7E49483779ED9A210702A8307AC336ABB879F62BFBD3AAF3342289C |
| Original broken header | AHLLX/IPRegionMon-For-TrafficMonitor, 935e6a8fcea923a9a038fff736061af2066d34b9 | ABE0564B63849886E03D21750CACB0DD19C75D1EB1BD07C7DA40629436345DE7 |

For offline use, supply -ReferenceHeader and -LegacyHeader pointing to previously
downloaded raw files. Their byte hashes must still match. The working header is
compared after normalizing CRLF to LF so Git's Windows checkout policy is allowed;
other changes, including enums and method signatures/order, fail verification.
See [the interface provenance and license](../../include/README.md).

The runner builds fixture.cpp twice: once against the repository header and once
against the original broken header. The host is compiled independently against
the pinned official reference. It checks API v8 and calls IsDoubleLineExclusive
through the DLL's IPluginItem pointer, matching TrafficMonitor's v8 call site.

- The candidate fixture must return 0 and exit with code 0.
- The legacy negative control must fail: exit 10 means a caught structured
  exception, and exit 12 means an incorrect virtual-call result. Missing a vtable
  slot is undefined behavior, so the precise symptom is not portable.
- The whole script fails if the current header differs, the positive probe fails,
  or the negative control unexpectedly passes.

Own test sources use /W4 /WX. Pinned vendor headers use /external:W0 to avoid
warnings about unused parameters in their default implementations.

## Checking a built IPRegionMon DLL

Pass the absolute path to a separately built Release|x64 plugin, for example:

    .\tests\abi\Run-AbiTest.ps1 -PluginDll 'D:\build\plugins\IPRegionMon.dll'

When building that DLL with MSBuild, always pass
/p:PostBuildEventUseInBuild=false: the existing Release|x64 project has an
automatic copy step targeting an installed TrafficMonitor directory.

The isolated host loads the actual DLL, resolves TMPluginGetInstance, checks
API v8, both item IDs (4Tc21hGS_direct and 4Tc21hGS_proxy), item enumeration and
IsDoubleLineExclusive. It unloads the DLL without calling OnInitialize,
EI_CONFIG_DIR, DataRequired or ShowOptionsDialog.

This is a narrow ABI smoke test. It does not prove full compatibility with every
TrafficMonitor release, exercise IPluginDrawer at runtime, test network/UI
behavior, or verify worker shutdown while background work is active.
