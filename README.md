# IPRegionMon-For-TrafficMonitor
一个用于 TrafficMonitor 的插件，显示当前公网 IP 与地区信息，支持代理出口对比与自定义测延迟目标。

- 插件主类：`CIPRegionMon`
- 输出项目：
  - 直：直连出口 IP/地区
  - 代：代理出口 IP/地区（可选）
- 支持版本：TrafficMonitor 插件 API v2+（兼容更高版本）

## 预览（占位图）
> 将下列占位图替换为你的实际截图，路径可保持不变。

- 任务栏展示：
  
<img width="600" height="57" alt="image" src="https://github.com/user-attachments/assets/50a8f5fc-4a52-4051-8378-bc267fb93dec" />

- 设置对话框：
  
<img width="735" height="618" alt="image" src="https://github.com/user-attachments/assets/d331d9d5-6193-404c-9162-d46c8ffdc852" />


- 悬浮提示：
  
<img width="530" height="358" alt="image" src="https://github.com/user-attachments/assets/448c05ab-a06b-4a1d-ae99-ac88b2728fb6" />

## 功能特性
- 公网 IP 信息：分别获取直连与代理出口的 IP、国家/地区/城市、ISP。
- 代理支持：勾选“使用代理”并填写 `HTTP` 代理地址（如 `127.0.0.1:7890`）。
- 自定义测延迟目标（最多 3 个）：名称 + URL/域名，支持同时测直连/代理延迟。
- 刷新间隔：默认 300 秒，可配置。
- 悬浮提示美化：
  - IP/地区信息分行显示。
  - 延迟块显示为：
    - `延迟:`
    - `直: 名称 - xxms | 名称 - xxms | ...`
    - `代: 名称 - xxms | 名称 - xxms | ...`
- UI 自适应：对话框支持拉伸，控件自动布局。
- 配置持久化：INI 文件保存，中文名称使用 `#u8:HEX` 编码，彻底避免中文逐次截断问题。

## 安装
1. 构建 Release 版本（推荐 x64）。
2. 将生成的 `IPRegionMon.dll` 复制到 TrafficMonitor 的 `plugins` 目录。
3. 重启 TrafficMonitor，在“插件管理”里启用并选择显示项目。

> 也可直接拷贝现成的 `IPRegionMon.dll` 到 `plugins` 目录，无需其他依赖。

## 使用说明
- 在插件选项（“IP 监控设置”）中：
  - 代理：勾选“使用代理”，填入 `127.0.0.1:7890` 之类的 HTTP 代理地址。
  - 刷新/延迟：设置刷新间隔；添加测延迟目标名称与地址（最多 3 个）。
  - 点击“立即刷新”开始获取，执行期间按钮会显示“正在刷新…”。

## 注意事项
- 首次加载插件后会在后台刷新一次数据。
- 代理要求 HTTP 代理格式（`host:port`），直/代延迟会分开测量与展示。
- 插件会访问公共 IP API（默认 ip-api.com）与自定义测延迟目标，请确保网络可达。
- 兼容性：Windows 10/11；仅依赖 WinHTTP（系统组件）。
- 隐私：插件不收集任何用户隐私信息。

## 二次开发（VS2022 + MFC）
- 先决条件：
  - Windows 10/11，Visual Studio 2022（17.x）。
  - 安装“使用 C++ 的桌面开发”工作负载，并勾选“适用于 x64 的 MFC”。
  - Windows 10 SDK、MSVC v143 工具集。
- 打开工程：
  - 直接打开解决方案 `TrafficMonitor.sln`（推荐），在解决方案资源管理器中找到项目 `PluginDemo`，其下包含本插件源文件。
  - 或使用“文件 > 打开 > 文件夹”选择仓库根目录。
- 构建与输出：
  - 选择 `Release | x64` 配置，生成项目。
  - 将生成的 `IPRegionMon.dll` 复制到 TrafficMonitor 可执行文件同级的 `plugins` 目录。
  - 可在项目“生成事件 > 生成后事件”添加复制命令：
    - `xcopy /y "$(TargetPath)" "$(SolutionDir)plugins\"`
- 调试插件：
  - 在项目 `PluginDemo` 属性 > 调试：
    - 启动外部程序：选择 `TrafficMonitor.exe`。
    - 工作目录：`TrafficMonitor.exe` 所在目录。
  - 按 F5 启动，主程序会自动加载插件，打断点即可调试。
- 入口/关键文件：
  - 插件入口：`IPRegionMon.h/cpp`（类 `CIPRegionMon`，导出 `TMPluginGetInstance`）。
  - 设置对话框：`OptionsDlg.*`
  - 数据与配置：`DataManager.*`
  - 任务栏显示项：`CustomDrawItem.*`
  - 资源：`IPRegionMon.rc`、`resource.h`

## 目录结构（详细）
```
PluginDemo/
├─ IPRegionMon.h / IPRegionMon.cpp     # 插件入口（类 CIPRegionMon，导出 TMPluginGetInstance）
├─ DataManager.h / DataManager.cpp     # 数据获取、INI 配置、悬浮提示与延迟格式化
├─ OptionsDlg.h / OptionsDlg.cpp       # 插件设置对话框（MFC，自适应布局、异步刷新）
├─ CustomDrawItem.h / CustomDrawItem.cpp # 直/代两个显示项（名称、值、示例）
├─ IPRegionMon.rc                      # 资源与字符串表
├─ resource.h                          # 资源 ID 定义
├─ pch.h / pch.cpp, framework.h        # 预编译头与框架头
└─ README.md                           # 本说明文档

include/
└─ PluginInterface.h                   # TrafficMonitor 插件 API 头文件
```

## 许可证
- 本插件以 MIT 协议发布。作者：HiTech_NinJa。
