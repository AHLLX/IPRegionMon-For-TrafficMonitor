# IPRegionMon 配置界面设计说明文档

为了抛弃先前存在拉伸错位与缩放异常的原生 MFC 单页面骨架，我们已经从工程实现中移除了原有的 `COptionsDlg` 类及其硬编码布局代码。

本说明旨在记录**当前插件支持的所有功能设置项**，并为未来接入现代可扩展 UI 框架（如 WinUI 3、ImGui、Qt 或更规范的 MFC TabCtrl 分页架构）提供功能划分与实现的依据。

## 1. 核心接口说明

在 `IPRegionMon.cpp` 中，主程序调用设置页面的入口函数为 `ShowOptionsDialog`：

```cpp
ITMPlugin::OptionReturn CIPRegionMon::ShowOptionsDialog(void* hParent)
{
    // 目前返回 OR_OPTION_NOT_PROVIDED，以禁用旧版存在问题的设置弹窗。
    // 在重新接入新的现代 UI 框架后，在此处实例化新框架的窗体，
    // 获取 CDataManager::Instance().m_setting_data 的引用或拷贝以进行交互绑定。
    return ITMPlugin::OR_OPTION_NOT_PROVIDED;
}
```

所有的配置数据统一存储于 `SettingData` 结构体中（位于 `DataManager.h`）。在构建新的配置页面时，应与这些字段一一对应并提供修改途径：

```cpp
struct SettingData
{
    bool show_second;             // 是否显示秒数（暂无实际应用，作为占位/扩展）
    bool use_proxy;               // 是否启用代理
    std::wstring proxy_address;   // 代理服务器地址（如：127.0.0.1:7890）
    std::wstring api_domestic_url;// 国内线路查询 API
    std::wstring api_foreign_url; // 代理/国外线路查询 API
    int refresh_interval_sec;     // 后台自动刷新数据的时间间隔（秒），默认300秒
    bool show_ip;                 // 是否在数值文本中显示具体的IP地址
    // 延迟测试目标列表
    std::vector<LatencyTarget> latency_targets;
};
```

## 2. 现代化页面分页设计（推荐方案）

针对以上功能，为了实现“符合工程实现的、可扩展的”现代 UI，建议将其设计为带有左侧/顶部 Tab 导航的多页面视图。推荐分页结构如下：

### 📌 页面 1：常规设置 (General)
此页面包含基础的展示与基础参数设置，适用于大部分普通用户的常规调整。
- **自动刷新**：提供一个数字输入框 (Number Box)，用于设定 `refresh_interval_sec`，单位为秒（默认：300秒）。
- **展示偏好**：
  - 复选框 (Checkbox)：是否在任务栏显示具体 IP 地址 (`show_ip`)。

### 📌 页面 2：网络与代理 (Network & Proxy)
专注于网络连接、代理绕过以及探测源站 API 设置。这部分涉及输入长度较长的 URL 和 IP，在旧版 UI 中极易造成输入框被挤压成一条线。在新的分页框架中，这部分应享有充分的横向展示空间。
- **代理设置**：
  - 拨动开关/复选框 (Toggle/Checkbox)：启用代理服务器 (`use_proxy`)。
  - 文本输入框 (TextBox)：代理地址 (`proxy_address`)，占满行宽。当启用代理关闭时，此项应处于禁用/置灰状态。
- **探测 API 设置**：
  - 文本输入框 (TextBox)：直连环境探测 API (`api_domestic_url`)，要求有足够宽度展示长 URL。
  - 文本输入框 (TextBox)：代理环境探测 API (`api_foreign_url`)，同样需要占据足够的展示宽度。

### 📌 页面 3：延迟测试目标 (Latency Targets)
用于设定用户自定义的 Ping 或 HTTP 延迟测试目标，具有列表动态增删属性。
- **目标列表**：使用支持动态调整大小的列表视图 (ListView / DataGrid) 渲染 `latency_targets` 集合。
- 列表包含两列：
  1. 别名 (Name) - 例如 "Google"
  2. 测试地址 (Address) - 例如 "www.google.com"
- **操作项**：提供“添加”、“编辑”、“删除”按钮管理该数组的内容。

---

## 3. UI 框架迁移建议

鉴于 TrafficMonitor 是基于 C++ 的 Win32 / MFC 项目，建议新框架选择以下一种以彻底解决拉伸和对齐问题：

1. **动态响应式 MFC TabCtrl**：这是最不需要引入外部依赖的方案。使用一个 `CTabCtrl` 作为主容器，上述三个页面分别作为独立的子 `CDialog`。因为页面分离，不需要复杂的相对坐标计算函数 `LayoutAll()`，由资源文件定义对话框的原始坐标，在调整大小时只整体放大各个子对话框即可。
2. **WinUI 3 (XAML Islands)**：能实现类似 Windows 11 原生风格的现代感设置界面。但需要引入复杂的 COM 初始化和 Windows App SDK 依赖，工程量较大。
3. **轻量级 Direct2D 或 ImGui**：脱离 Windows 控件流，直接在弹出的空窗口中绘制渲染。

**当前状态总结：**
已完成旧版不兼容 `OptionsDlg` 骨架的删除，核心获取配置、自动存储至 `.ini`、后台静默刷新的逻辑完全保留且正常运行。在开发出新的 UI 页面前，用户依然可以通过直接编辑插件目录下的 `.ini` 配置文件来配置以上列出的所有功能。
