# IPRegionMon-For-TrafficMonitor

[简体中文](#简体中文) | [English](#english)

---

<a name="简体中文"></a>
# IPRegionMon-For-TrafficMonitor (简体中文)

一个用于 TrafficMonitor 的插件，显示当前公网 IP 与地区信息，支持代理出口对比与自定义测延迟目标。

- 插件主类：`CIPRegionMon`
- 输出项目：
  - 直：直连出口 IP/地区
  - 代：代理出口 IP/地区（可选）
- 支持版本：TrafficMonitor 插件 API v2+（兼容更高版本）

## 预览
- 任务栏展示：
  
  <img width="600" height="57" alt="image" src="https://github.com/user-attachments/assets/50a8f5fc-4a52-4051-8378-bc267fb93dec" />

- 设置对话框：
  
  <img width="735" height="618" alt="image" src="https://github.com/user-attachments/assets/d331d9d5-6193-404c-9162-d46c8ffdc852" />

- 悬浮提示：
  
  <img width="530" height="358" alt="image" src="https://github.com/user-attachments/assets/448c05ab-a06b-4a1d-ae99-ac88b2728fb6" />

## 功能特性
- **公网 IP 信息**：分别获取直连与代理出口的 IP、国家/地区/城市、ISP。
- **代理支持**：勾选“使用代理”并填写 `HTTP` 代理地址（如 `127.0.0.1:7890`）。
- **自定义测延迟目标（最多 3 个）**：名称 + URL/域名，支持同时测直连/代理延迟。
- **刷新间隔**：默认 300 秒，可配置。
- **悬浮提示美化**：
  - IP/地区信息分行显示。
  - 延迟块显示为：
    - `延迟:`
    - `直: 名称 - xxms | 名称 - xxms | ...`
    - `代: 名称 - xxms | 名称 - xxms | ...`
- **全新选项界面**：采用紧凑的 3-Tab 标签页布局（常规/网络、延迟目标、当前状态），底层移除 ListCtrl 彻底解决 TrafficMonitor 死锁卡死问题。
- **纯净度检测**：精准识别 IP 类型（普通宽带、IDC机房、代理、VPN、Tor），防止节点欺骗。
- **全异步无感刷新**：彻底重构网络请求逻辑为后台线程执行，双缓冲无锁刷新，绝对不卡顿主界面。
- **配置持久化**：INI 文件保存，中文名称使用 `#u8:HEX` 编码，彻底避免中文逐次截断问题。

## 安装
1. 构建 Release 版本（推荐 x64）。
2. 将生成的 `IPRegionMon.dll` 复制到 TrafficMonitor 的 `plugins` 目录。
3. 重启 TrafficMonitor，在“插件管理”里启用并选择显示项目。

> 也可直接拷贝现成的 `IPRegionMon.dll` 到 `plugins` 目录，无需其他依赖。

## 使用说明
- 在插件选项（“IP 监控设置”）中：
  - **代理**：勾选“使用代理”，填入 `127.0.0.1:7890` 之类的 HTTP 代理地址。
  - **刷新/延迟**：设置刷新间隔；添加测延迟目标名称与地址（最多 3 个）。
  - 点击“立即刷新”开始获取，执行期间按钮会显示“正在刷新…”。

## 注意事项
- 首次加载插件后会在后台刷新一次数据。
- 代理要求 HTTP 代理格式（`host:port`），直/代延迟会分开测量与展示。
- 插件会访问公共 IP API（默认 ip-api.com）与自定义测延迟目标，请确保网络可达。
- **兼容性**：Windows 10/11；仅依赖 WinHTTP（系统组件）。
- **隐私**：插件不收集任何用户隐私信息。

## 二次开发（VS2022/VS2026 + MFC）
- **先决条件**：
  - Windows 10/11，Visual Studio 2022 或 2026。
  - 安装“使用 C++ 的桌面开发”工作负载，并勾选“适用于 x64 的 MFC”以及“最新 MSVC 的 ATL/MFC”。
  - Windows 10/11 SDK、MSVC v143/v145 工具集。
- **打开工程**：
  - 直接打开项目 `IPRegionMon.vcxproj`（推荐）。
  - 项目已包含自适应的 `include/PluginInterface.h`，支持克隆后开箱即用。
- **构建与输出**：
  - 选择 `Release | x64` 配置，生成项目。
  - 将生成的 `IPRegionMon.dll` 复制到 TrafficMonitor 可执行文件同级的 `plugins` 目录。
  - 可在项目“生成事件 > 生成后事件”添加复制命令：
    - `xcopy /y "$(TargetPath)" "$(SolutionDir)plugins\"`
- **调试插件**：
  - 在项目 `IPRegionMon` 属性 > 调试：
    - 启动外部程序：选择 `TrafficMonitor.exe`。
    - 工作目录：`TrafficMonitor.exe` 所在目录。
  - 按 F5 启动，主程序会自动加载插件，打断点即可调试。
- **入口/关键文件**：
  - 插件入口：`IPRegionMon.h/cpp`（类 `CIPRegionMon`，导出 `TMPluginGetInstance`）。
  - 设置对话框：`OptionsDlg.*`
  - 数据与配置：`DataManager.*`
  - 任务栏显示项：`CustomDrawItem.*`
  - 资源：`IPRegionMon.rc`、`resource.h`

## 目录结构（详细）
```
./
├─ .agents/
│  └─ AGENTS.md                         # AI 助手开发规则文档
├─ include/
│  └─ PluginInterface.h                 # TrafficMonitor 插件接口定义文件
├─ CustomDrawItem.cpp / CustomDrawItem.h
├─ DataManager.cpp / DataManager.h
├─ IPRegionMon.cpp / IPRegionMon.h
├─ IPRegionMon.rc
├─ OptionsDlg.cpp / OptionsDlg.h
├─ IPRegionMon.vcxproj / .filters / .user
├─ README.md
├─ framework.h
├─ pch.cpp / pch.h
└─ resource.h
```

<img width="430" height="589" alt="image" src="https://github.com/user-attachments/assets/56df8284-bd80-46bd-a9e1-f27405ef3640" />

## 许可证
- 本插件以 MIT 协议发布。作者：HiTech_NinJa。

---

<a name="english"></a>
# IPRegionMon-For-TrafficMonitor (English)

[Back to Top / 返回顶部](#简体中文)

A plugin for TrafficMonitor to display current public IP and location information. Supports comparing proxy exit IP and custom latency testing targets.

- Main plugin class: `CIPRegionMon`
- Output items:
  - Direct: Direct exit IP/location
  - Proxy: Proxy exit IP/location (optional)
- Supported versions: TrafficMonitor Plugin API v2+ (compatible with higher versions)

## Preview
- Taskbar View:
  
  <img width="600" height="57" alt="image" src="https://github.com/user-attachments/assets/50a8f5fc-4a52-4051-8378-bc267fb93dec" />

- Settings Dialog:
  
  <img width="735" height="618" alt="image" src="https://github.com/user-attachments/assets/d331d9d5-6193-404c-9162-d46c8ffdc852" />

- Tooltip Info:
  
  <img width="530" height="358" alt="image" src="https://github.com/user-attachments/assets/448c05ab-a06b-4a1d-ae99-ac88b2728fb6" />

## Features
- **Public IP Info**: Retrieve public IP, country/region/city, and ISP for both direct and proxy connections separately.
- **Proxy Support**: Check "Use proxy" and specify HTTP proxy address (e.g. `127.0.0.1:7890`).
- **Custom Latency Targets (up to 3)**: Name + URL/domain, supports measuring both direct and proxy latency simultaneously.
- **Refresh Interval**: Default 300 seconds, configurable.
- **Aesthetic Tooltip Info**:
  - Displays IP/location info in separate lines.
  - Latency blocks are displayed as:
    - `Latency:`
    - `Direct: Name - xxms | Name - xxms | ...`
    - `Proxy: Name - xxms | Name - xxms | ...`
- **New Settings UI**: Uses a compact 3-tab layout (General/Network, Latency Targets, Current Status) and removes ListCtrl to completely fix TrafficMonitor deadlock issues.
- **IP Purity Detection**: Accurately identifies IP types (Residential, Datacenter, Proxy, VPN, Tor) to prevent node spoofing.
- **Asynchronous Refresh**: Completely refactored network requests into background threads with double-buffered, lock-free UI updates, ensuring the main interface never freezes.
- **Persistent Configuration**: Saves to INI files; uses `#u8:HEX` encoding for Chinese characters to fully prevent truncation issues under non-unicode settings.

## Installation
1. Build the Release version (x64 recommended).
2. Copy the generated `IPRegionMon.dll` to the `plugins` directory of TrafficMonitor.
3. Restart TrafficMonitor, enable the plugin in "Plugin Management" and choose the display items.

> You can also directly copy a pre-built `IPRegionMon.dll` to the `plugins` directory without any other dependencies.

## Usage
- In plugin options ("IP Monitor Settings"):
  - **Proxy**: Check "Use proxy", fill in HTTP proxy address such as `127.0.0.1:7890`.
  - **Refresh/Latency**: Set refresh interval; add latency target names and addresses (up to 3).
  - Click "Refresh now" to start fetching. The button will display "Refreshing..." during execution.

## Notes
- The plugin refreshes data in the background once after it is first loaded.
- Proxy requires HTTP format (`host:port`). Direct and proxy latency will be measured and displayed separately.
- The plugin accesses public IP APIs (default ip-api.com) and custom latency targets. Please ensure network connectivity.
- **Compatibility**: Windows 10/11; relies only on WinHTTP (system component).
- **Privacy**: The plugin does not collect any user private information.

## Secondary Development (VS2022/VS2026 + MFC)
- **Prerequisites**:
  - Windows 10/11, Visual Studio 2022 or 2026.
  - Install "Desktop development with C++" workload, and check "C++ MFC for x64/x86" and "C++ ATL for x64/x86 (latest MSVC)".
  - Windows 10/11 SDK, MSVC v143/v145 toolset.
- **Open Project**:
  - Open project `IPRegionMon.vcxproj` directly (recommended).
  - The repository contains a self-contained `include/` directory with `PluginInterface.h`, allowing it to compile directly out-of-the-box.
- **Build & Output**:
  - Select `Release | x64` configuration, build the project.
  - Copy the generated `IPRegionMon.dll` to the `plugins` directory adjacent to TrafficMonitor.exe.
  - You can add a post-build event command to copy automatically:
    - `xcopy /y "$(TargetPath)" "$(SolutionDir)plugins\"`
- **Debug Plugin**:
  - Under project `IPRegionMon` Properties > Debugging:
    - Command: select `TrafficMonitor.exe`.
    - Working directory: directory where `TrafficMonitor.exe` is located.
  - Press F5 to start. The main application will load the plugin and you can hit breakpoints to debug.
- **Key Files**:
  - Plugin Entry: `IPRegionMon.h/cpp` (class `CIPRegionMon`, exporting `TMPluginGetInstance`).
  - Settings Dialog: `OptionsDlg.*`
  - Data & Config: `DataManager.*`
  - Taskbar Display Item: `CustomDrawItem.*`
  - Resources: `IPRegionMon.rc`, `resource.h`

## Directory Structure (Detailed)
```
./
├─ .agents/
│  └─ AGENTS.md                         # Agent instruction and project rules
├─ include/
│  └─ PluginInterface.h                 # TrafficMonitor plugin interface definition
├─ CustomDrawItem.cpp / CustomDrawItem.h
├─ DataManager.cpp / DataManager.h
├─ IPRegionMon.cpp / IPRegionMon.h
├─ IPRegionMon.rc
├─ OptionsDlg.cpp / OptionsDlg.h
├─ IPRegionMon.vcxproj / .filters / .user
├─ README.md
├─ framework.h
├─ pch.cpp / pch.h
└─ resource.h
```

<img width="430" height="589" alt="image" src="https://github.com/user-attachments/assets/56df8284-bd80-46bd-a9e1-f27405ef3640" />

## License
- This plugin is released under the MIT License. Author: HiTech_NinJa.
