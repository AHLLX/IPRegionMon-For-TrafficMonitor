# Workspace Rules & Guidelines for IPRegionMon

Welcome agent! This file defines the styling guidelines, behavioral constraints, and technical rules for developing and optimizing the **IPRegionMon-For-TrafficMonitor** plugin. Please follow these instructions strictly during implementation.

## 1. Environment & Compilation
* **Toolchain & IDE**: The project uses **Visual Studio 2026 Build Tools** (PlatformToolset `v145`) and **Windows SDK (10.0.26100.0)**.
* **MFC & ATL**: C++ MFC and ATL must be enabled.
* **Compilation Command**: Verify compilation by running:
  ```powershell
  & "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe" IPRegionMon.vcxproj /p:Configuration=Release /p:Platform=x64
  ```
* **Include Folder**: The project has a self-contained `include/` folder containing `PluginInterface.h` to allow direct, out-of-the-box cloning and compilation. Do not break or bypass this include path.

## 2. Coding & Architecture Rules
* **Unicode / Wide Strings**:
  * Always use Unicode/Wide Strings (`std::wstring`, `CString`, `wchar_t*`) for GUI text, pathing, and string manipulation.
  * Use UTF-8 internally only when interacting with APIs (network requests, JSON parsing) and convert immediately to wide strings via helper functions (`Utf8ToWide`).
* **No Third-Party Dependencies**:
  * Avoid adding external libraries, DLLs, or complex dependency systems (like curl, nlohmann/json).
  * Use Windows native `WinHTTP` (`winhttp.dll`) for networking.
  * Use the hand-rolled helper `GetJsonStringValue` for parsing simple API JSON responses.
* **INI Encoding Workaround**:
  * Windows standard INI API (`GetPrivateProfileStringW`, `WritePrivateProfileStringW`) does not handle UTF-8/Unicode characters robustly without localized code pages.
  * Any Chinese strings or Unicode characters written to the INI config must be hex-encoded using `Utf8HexEncode` (saving as `#u8:HEX_STRING`) and decoded using `Utf8HexDecode` on read.
* **Asynchronous Networking (UI Responsiveness)**:
  * **CRITICAL**: Never execute network requests or latency measurements on the main/UI thread. Doing so will freeze the TrafficMonitor application.
  * All networking tasks must run on background threads (e.g., `std::thread`).
  * Communication back to UI components (such as updating option dialog fields) should be done asynchronously using Windows messages (e.g., `PostMessage` with `WM_IP_UPDATE_DONE`).

## 3. Reorganizing and Formatting Code
* Keep code files clean and documented.
* Maintain consistency with native C++ and MFC conventions (e.g., `CWnd` methods, MFC Message Maps, DDX/DDV).
* Before finishing any task, run an MSBuild check to ensure there are no compilation errors or new warnings.
