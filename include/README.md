# TrafficMonitor plugin interface

PluginInterface.h is an unmodified copy of the official TrafficMonitor header:

- Repository: https://github.com/zhongyang219/TrafficMonitor
- Commit: 930f17533d6098989ebad62f210aa97d75ef174b
- File: https://github.com/zhongyang219/TrafficMonitor/blob/930f17533d6098989ebad62f210aa97d75ef174b/include/PluginInterface.h
- SHA-256 of the upstream file bytes: E11A2C07B7E49483779ED9A210702A8307AC336ABB879F62BFBD3AAF3342289C

The original Zhong Yang 2021 attribution is retained in the header. The
TrafficMonitor repository's [Anti 996 License](TrafficMonitor.LICENSE) is copied
without modification from the same commit. That license applies to the vendored
interface; the root MIT license covers IPRegionMon's own code.

The unmodified upstream header includes trailing spaces on two newly imported
comment lines. They are retained, so git diff --check reports those two lines.

## API v8 compatibility

The previous header advertised API v8 but was missing part of that interface.
The replacement preserves API v8 and the existing Direct/Proxy item IDs.

| Interface | Previous virtual methods | Official virtual methods | Change |
| --- | ---: | ---: | --- |
| IPluginItem | 15 | 16 | Adds IsDoubleLineExclusive at the end |
| ITMPlugin | 15 | 15 | Identical signatures and order |
| ITrafficMonitor | 10 | 12 | Appends GetMainWindowHwnd and GetTaskbarWindowHwnd |
| IPluginDrawer | 7 | 10 | Adds initial GetAPIVersion, changes DrawRectOutLine, appends DrawLine and GetHDC |

Do not edit the official header to lower the reported API version or reorder
virtual methods. A version number alone does not establish binary compatibility.

The [ABI regression test](../tests/abi/README.md) compares the complete header
against the pinned upstream copy and exercises the previously missing v8 slot
across a DLL boundary.
