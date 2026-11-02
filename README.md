# IPRegionMon-For-TrafficMonitor

[简体中文](#简体中文) | [English](#english)

---

<a name="简体中文"></a>
# IPRegionMon-For-TrafficMonitor (简体中文)

一个专为 TrafficMonitor 设计的 IP 地区与网络监控插件，支持直连与代理出口双链路对比、纯净度检测、自定义延迟测速目标及 Windows 11 原生自绘现代卡片设置界面。

- 插件主类：`CIPRegionMon`
- 输出监控项：
  - 直：直连出口 IP / 地区
  - 代：代理出口 IP / 地区（可选）
- 支持版本：TrafficMonitor 插件 API v2+（兼容更高版本）
- 依赖要求：零外部第三方库依赖（仅使用 Windows 原生 WinHTTP、GDI+、DWM 及 MFC）

## 界面预览

### 任务栏展示效果
<img width="600" height="57" alt="任务栏展示" src="https://github.com/user-attachments/assets/50a8f5fc-4a52-4051-8378-bc267fb93dec" />

### 设置对话框（Windows 11 原生自绘现代卡片风格）

- **常规 / 网络 & 代理设置**
  ![常规与代理设置](images/settings_tab0.png)

- **测速目标与延迟管理（表格宽度自适应）**
  ![延迟目标设置](images/settings_tab1.png)

- **网络状况与安全报告（格式对齐与一键复制）**
  ![当前状态与安全报告](images/settings_tab2.png)

### 悬浮提示（Tooltip）
<img width="530" height="358" alt="悬浮提示" src="https://github.com/user-attachments/assets/448c05ab-a06b-4a1d-ae99-ac88b2728fb6" />

---

## 功能特性

- **Windows 11 原生现代卡片 UI**：
  - 采用 Fluent 视觉调色板与自绘纯白圆角卡片布局（Card），界面呼吸感与留白比例大幅提升。
  - 顶栏采用自绘分段胶囊导航栏（`CModernTabBar`），支持鼠标悬停过渡与高亮指示线。
  - 按钮采用自绘圆角控件（`CModernButton`），主操作提供 Win11 强调蓝视觉反馈。
- **任务栏格式即时切换**：
  - 新设置默认只显示国家代码（如 `GB`、`CN`、`US`）。完整地区、IP 地址及其他诊断信息保留在悬浮提示中。已有显示格式保持不变，可在设置中选择国家代码模式。
  - 调整任务栏显示格式下拉选项时，系统即刻触发内存数据重格式化并更新任务栏，无需等待网络请求。
- **自动刷新动态倒计时**：
  - 常规设置面板状态行提供动态跳动的秒级倒计时，实时掌握下一次自动探测的时间点。
- **表格列宽自动伸缩**：
  - 延迟测速列表应用资源管理器主题（Explorer），列宽按比例自适应卡片实际宽度，在高分屏 DPI 下无横向滚动条。
- **多源探测与缓存**：
  - 新设置的直连与代理 IP API 均默认为 `https://ipwho.is/`。备用源按顺序尝试 `https://ipwho.is/`、`https://api.ip.sb/geoip`、`https://ipinfo.io/json`，跳过与首选 URL 重复的项。
  - 查询失败时，已有成功结果最多可保留三次失败刷新。保留的缓存不代表本次请求成功。
- **HTTPS 与请求时限**：
  - HTTPS 保留 Windows 的证书验证，并拒绝 HTTPS 到 HTTP 的重定向。通过 HTTP 代理发送的 HTTPS 请求使用相同验证，拦截代理提供的证书也必须受 Windows 信任。
  - 显式配置的 HTTP 地址仍可使用，传输层不会将其自动升级为 HTTPS。需要加密时，请在设置中选择支持 HTTPS 的接口。
  - 每个 GET 请求的网络预算为 4000 ms，每个 HEAD 测速请求为 1200 ms。备用源和测速目标顺序执行，整个刷新可能更长；取消请求及等待回调完成也可能增加耗时。参见[网络实现与测试说明](tests/network/README.md)。
- **链路独立探测与代理分流支持**：
  - 支持分别获取直连与代理出口的 IP、国家/地区/城市、ISP 运营商。
  - 直连请求不使用配置的代理；代理线路通过配置的 HTTP 代理发出。
- **纯净度与网络类型识别**：
  - 识别网络类型（普通宽带、蜂窝移动网络、数据中心/机房机房等）及安全状态（纯净原生、代理、VPN、Tor、恶意黑名单）。
- **动态延迟测速**：
  - 支持自由增删排测速目标，并可对选中目标进行单项即时测速。
- **后台刷新**：
  - 刷新任务按顺序查询数据源和测速目标。网络操作在后台执行，完成的结果通过受同步保护的快照发布到界面。
- **配置持久化安全**：
  - INI 文件保存，Unicode 与中文名称使用 `#u8:HEX` 十六进制编码，彻底规避本地代码页截断问题。
- **零外部依赖**：
  - 完全由 Windows 系统内置 API 构建，不引入额外的第三方 DLL 动态链接库。

---

## 安装方法

1. 在 [Releases](https://github.com/AHLLX/IPRegionMon-For-TrafficMonitor/releases) 页面下载对应架构的发布包（通常推荐 x64）。
2. 将压缩包内的 `IPRegionMon.dll` 解压至 TrafficMonitor 根目录下的 `plugins` 文件夹中。
3. 启动或重启 TrafficMonitor，在“插件管理”中勾选启用并配置要显示的项目。

---

## 使用说明

在任务栏监控项上右键选择“选项...”打开设置面板：
- **常规 / 网络**：
  - 设置任务栏显示格式与刷新间隔秒数。
  - 勾选“启用代理服务器”并填入 HTTP 代理地址（例如 `127.0.0.1:7890`）。
  - 点击“测试代理连通性”可在后台发送 HTTP HEAD 请求并报告响应耗时；这不是 ICMP ping 延迟。
  - 点击“立即刷新”可立即应用当前所有改动并触发后台探测。
- **延迟目标**：
  - 选中列表项可载入并更新名称或目标 URL/域名。
  - 支持“测速选中目标”验证单点连通性。
- **当前状态与安全**：
  - 查看直连与代理链路的完整安全报告。
  - 支持单项复制直连 IP、代理 IP，或一键“复制全部文本”。

---

## 二次开发与编译

- **开发环境要求**：
  - Windows 10 / 11 操作系统。
  - Visual Studio 2022 或 Visual Studio 2026（MSVC 工具集 `v143` 或 `v145`）。
  - 安装“使用 C++ 的桌面开发”工作负载，并勾选“C++ MFC for x64/x86”。
  - Windows SDK 10.0.26100.0 或更高版本。
- **编译步骤**：
  1. 直接使用 Visual Studio 打开根目录下的 `IPRegionMon.vcxproj`。
  2. 选择 `Release | x64` 或 `Release | Win32` 配置进行生成。
  3. 亦可通过命令行执行 MSBuild：
     ```powershell
     & "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe" IPRegionMon.vcxproj /p:Configuration=Release /p:Platform=x64
     ```
  4. 生成产物将位于 `Bin\x64\Release\plugins\IPRegionMon.dll`。

---

## 代码目录结构

```
./
├─ .agents/
│  └─ AGENTS.md                         # 开发规范与规则说明
├─ include/
│  └─ PluginInterface.h                 # TrafficMonitor 插件原生接口头文件
├─ images/                              # 说明文档真实界面截图
│  ├─ settings_dialog.png
│  ├─ settings_tab0.png
│  ├─ settings_tab1.png
│  └─ settings_tab2.png
├─ CustomDrawItem.cpp / CustomDrawItem.h # 任务栏自绘与展示逻辑
├─ DataManager.cpp / DataManager.h       # 数据缓存、配置管理、异步网络探测
├─ ModernUI.cpp / ModernUI.h             # Win11 Fluent 自绘卡片、导航条与按钮控件
├─ IPRegionMon.cpp / IPRegionMon.h       # 插件生命周期导出类
├─ OptionsDlg.cpp / OptionsDlg.h         # 现代卡片设置对话框及各子页面实现
├─ IPRegionMon.rc / resource.h           # 中英文资源脚本与控件 ID
├─ IPRegionMon.vcxproj                   # Visual Studio 2026/2022 工程文件
└─ README.md
```

---

## 许可证
- 本插件基于 MIT 协议开源。作者：HiTech_NinJa & 贡献者。

---

<a name="english"></a>
# IPRegionMon-For-TrafficMonitor (English)

[Back to Top / 返回顶部](#简体中文)

A modern IP region and network latency monitoring plugin designed for TrafficMonitor. Supports dual-link direct and proxy egress comparison, IP purity detection, customizable latency targets, and a native Windows 11 Fluent card-styled options dialog.

- Main Plugin Class: `CIPRegionMon`
- Output Items:
  - Direct: Direct exit IP / Location
  - Proxy: Proxy exit IP / Location (Optional)
- Supported Versions: TrafficMonitor Plugin API v2+
- Dependencies: Zero third-party dependencies (built exclusively on WinHTTP, GDI+, DWM, and MFC)

## Preview

### Taskbar Display
<img width="600" height="57" alt="Taskbar View" src="https://github.com/user-attachments/assets/50a8f5fc-4a52-4051-8378-bc267fb93dec" />

### Settings Dialog (Windows 11 Native Custom-Drawn Card Style)

- **General / Network & Proxy**
  ![General and Proxy Settings](images/settings_tab0.png)

- **Latency Targets (Adaptive Column Widths)**
  ![Latency Targets](images/settings_tab1.png)

- **Current Status & Diagnostics (Aligned Layout & One-Click Copy)**
  ![Status and Security Diagnostics](images/settings_tab2.png)

---

## Features

- **Windows 11 Native Modern Card UI**:
  - Utilizes Fluent palette and white rounded cards for clean visual hierarchy.
  - Custom-drawn segmented tab navigation (`CModernTabBar`) with smooth hover feedback and accent indicator lines.
  - Modern rounded button controls (`CModernButton`) featuring Win11 accent blue branding.
- **Instant Taskbar Format Updates**:
  - New settings default to a country code such as `GB`, `CN` or `US`. Hover for the full location, IP address and diagnostics. Existing saved formats are preserved; choose **Country code** in settings to switch an existing installation.
  - Changing display format instantly reformats the taskbar display text from memory without waiting for network requests.
- **Dynamic Refresh Countdown**:
  - Displays a real-time ticking countdown in the options dialog indicating when the next automatic refresh will occur.
- **Adaptive Column Widths**:
  - The latency targets ListView dynamically proportions column widths according to card dimensions with no horizontal scrollbars.
- **Fallback Providers and Cached Results**:
  - New direct and proxy IP settings default to `https://ipwho.is/`. Fallbacks try `https://ipwho.is/`, `https://api.ip.sb/geoip` and `https://ipinfo.io/json` in order, skipping a URL already used as the preferred source.
  - Previously successful results may be retained for up to three failed refreshes. Retained data does not mean the current request succeeded.
- **HTTPS and Request Budgets**:
  - HTTPS retains Windows certificate validation and rejects HTTPS-to-HTTP redirects. HTTPS requests through the HTTP proxy use the same validation, including certificates presented by an intercepting proxy; the plugin does not ignore certificate errors.
  - Explicit HTTP endpoints remain supported. The transport does not automatically upgrade them to HTTPS; choose an HTTPS endpoint in settings when encryption is required.
  - Each GET has a 4000 ms network budget; each HEAD latency request has 1200 ms. Sequential fallback providers and latency targets can make a full refresh take longer, and cancellation/callback cleanup can add time. See the [transport behavior and regression tests](tests/network/README.md).
- **Dual Egress & Proxy Split-Routing Support**:
  - Retrieves public IP, location and ISP for direct and proxy links. Direct requests bypass the configured proxy; proxy requests use the configured HTTP proxy.
- **Purity & Network Classification**:
  - Detects network types (Broadband, Cellular, Datacenter/Hosting) and security flags (Native, Proxy, VPN, Tor, Blacklist).
- **Background Refreshes**:
  - Each refresh queries providers and latency targets sequentially in a background task. Completed results are published to the UI as synchronized snapshots.
- **Safe INI Configuration**:
  - Preserves Unicode and Chinese characters using `#u8:HEX` encoding to eliminate character corruption across different code pages.
- **Zero Third-Party Dependencies**:
  - Fully self-contained implementation with no external runtime DLL requirements.

---

## Installation

1. Download the latest release from the [Releases](https://github.com/AHLLX/IPRegionMon-For-TrafficMonitor/releases) page (x64 recommended).
2. Extract `IPRegionMon.dll` into the `plugins` folder inside the TrafficMonitor directory.
3. Restart TrafficMonitor, enable the plugin in "Plugin Management", and select your desired items.

---

## License
- Licensed under the MIT License. Author: HiTech_NinJa & Contributors.
- The vendored TrafficMonitor interface retains its upstream Anti 996 License; see [interface provenance and license](include/README.md).
