//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ 生成的包含文件。
// 供 IPRegionMon.rc 使用
//
#define IDD_DIALOG1                     101
#define IDD_OPTIONS_DIALOG              101
#define IDS_PLUGIN_NAME                 103
#define IDS_PLUGIN_DESCRIPTION          104
#define IDS_TIME                        105
#define IDS_DATE                        106
#define IDS_CUSTOM_DRAW_ITEM            107
#define IDS_DIRECT_ITEM_NAME            108
#define IDS_PROXY_ITEM_NAME             109
#define IDC_SHOW_SECOND_CHECK           1001
#define IDC_SHOW_LABEL_CHECK            1002

// 新增：确保 IDC_STATIC 可用
#ifndef IDC_STATIC
#define IDC_STATIC                      (-1)
#endif

// 新增：IP设置控件ID
#define IDC_USE_PROXY_CHECK             1101
#define IDC_PROXY_EDIT                  1102
#define IDC_API_EDIT                    1103
#define IDC_PROXY_LABEL                 1104
#define IDC_API_LABEL                   1105

// 新增：国内/国外 API 控件ID（替换单一 API）
#define IDC_API_DOMESTIC_EDIT           1110
#define IDC_API_FOREIGN_EDIT            1111
#define IDC_API_DOMESTIC_LABEL          1112
#define IDC_API_FOREIGN_LABEL           1113

// 刷新控制
#define IDC_REFRESH_INTERVAL_LABEL      1120
#define IDC_REFRESH_INTERVAL_EDIT       1121
#define IDC_REFRESH_NOW_BUTTON          1122
#define IDC_STATUS_LABEL                1123
#define IDC_STATUS_STATIC               1124

// 新增：延迟目标
#define IDC_LATENCY_TARGET_LABEL        1125
#define IDC_LATENCY_TARGET_EDIT         1126
#define IDC_LATENCY_ADD_BUTTON          1127

// 新增分组框控件ID
#define IDC_GRP_PROXY                   1130
#define IDC_GRP_LATENCY                 1132

// Tab与列表控件
#define IDC_TAB_MAIN                    1140
#define IDC_LATENCY_LIST                1141
#define IDC_LATENCY_EDIT_BUTTON         1142
#define IDC_LATENCY_DEL_BUTTON          1143
#define IDC_STATUS_INFO_EDIT            1144

// 弹出式延迟编辑对话框与页签对话框
#define IDD_LATENCY_EDIT_DIALOG         105
#define IDD_TAB_PROXY_API               106
#define IDD_TAB_LATENCY_LIST            107
#define IDD_TAB_STATUS_INFO             108
#define IDC_LATENCY_NAME_EDIT           1151
#define IDC_LATENCY_URL_EDIT            1152

// 在末尾追加基础ID（避免冲突）
#define IDC_LATENCY_DEL_BASE            1200

// 延迟目标：固定 3 行 Edit 对（替代 SysListView32，彻底规避表头拖拽卡死问题）
#define IDC_LAT_NAME_0                  1160
#define IDC_LAT_URL_0                   1161
#define IDC_LAT_DEL_0                   1162
#define IDC_LAT_NAME_1                  1163
#define IDC_LAT_URL_1                   1164
#define IDC_LAT_DEL_1                   1165
#define IDC_LAT_NAME_2                  1166
#define IDC_LAT_URL_2                   1167
#define IDC_LAT_DEL_2                   1168
#define IDC_LAT_HINT                    1169

// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        106
#define _APS_NEXT_COMMAND_VALUE         40001
#define _APS_NEXT_CONTROL_VALUE         1160
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif
