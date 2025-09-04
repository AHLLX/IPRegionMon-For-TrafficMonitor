# IPRegionMon-For-TrafficMonitor
一个用于 TrafficMonitor 的插件，显示当前公网 IP 与地区信息，支持代理出口对比与自定义测延迟目标。

# IPRegionMon — IP 地区监控插件
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
  
  <img width="1050" height="886" alt="image" src="https://github.com/user-attachments/assets/71fc62ff-fa21-412e-b294-59914091cae5" />


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

## 目录结构（详细）
```
PluginDemo/
├─ IPRegionMon.h / IPRegionMon.cpp     # 插件入口（类 CIPRegionMon，导出 TMPluginGetInstance）
├─ DataManager.h / DataManager.cpp     # 数据获取、INI 配置、悬浮提示与延迟格式化
├─ OptionsDlg.h / OptionsDlg.cpp       # 插件设置对话框（MFC，自适应布局、异步刷新）
├─ CustomDrawItem.h / CustomDrawItem.cpp # 直/代两个显示项（名称、值、示例）
├─ PluginDemo.rc                       # 资源与字符串表
├─ resource.h                          # 资源 ID 定义
├─ pch.h / pch.cpp, framework.h        # 预编译头与框架头
└─ README.md                           # 本说明文档

include/
└─ PluginInterface.h                   # TrafficMonitor 插件 API 头文件
```

## 许可证
- 本插件以 MIT 协议发布。作者：HiTech_NinJa。
