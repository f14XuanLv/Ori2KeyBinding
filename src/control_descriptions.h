#pragma once
#include "mapping.h"

namespace ori {
struct ControlDescription { const char* summary; const char* details; };
// Verified against local Steam build 5845748: native AddControllerControls and consumers;
// Simplified Chinese terms come from resources.assets/TranslatedMessageProvider.Messages[].Chinese.
// See docs/local-input-audit.md for method RVAs, resource Path IDs, and verification limits.
inline constexpr std::array<ControlDescription, control_count> control_descriptions{{
    {"跳跃 / 菜单选择", "跳跃；菜单“选择”（确认）；推进对话。"},
    {"能力 3 / 返回", "使用 B 装备槽的“能力 3”；菜单返回或关闭；推进或退出对话。具体能力由游戏内装备决定。"},
    {"能力 1 / 操作", "使用 X 装备槽的“能力 1”；操作（互动）；推进对话或选择选项1；切换地图筛选器；主菜单复制存档。"},
    {"能力 2 / 详细信息", "使用 Y 装备槽的“能力 2”；推进对话或选择选项2；显示地图详细信息；主菜单删除存档。"},
    {"猛击 / 抓钩", "猛击（Bash）；抓钩（Grapple）；菜单向左翻页；切换排行榜筛选。"},
    {"冲刺 / 挖掘", "冲刺（Dash）；挖掘（Burrow，沙地能力）；菜单向右翻页。原版简中用词为“挖掘”。"},
    {"展开能力轮盘 / 缩小", "展开能力轮盘；地图缩小。X / Y / B 的能力是可更换的装备槽。"},
    {"滑翔 / 抓住 / 放大", "滑翔（Glide）；抓住（Grab）；地图放大。"},
    {"聚焦奥里 / 聚焦目标", "按下左摇杆：地图聚焦奥里或目标；具体聚焦位置取决于当前任务和地图状态。这里修改的是按下摇杆，不是摇杆方向。"},
    {"未分配常用指令", "本机默认常用指令表未将 R3 绑定到动作；底层仍注册右摇杆按下与调试输入，不能视为完全无功能。"},
    {"地图 / 碎片 / 物品栏", "打开地图 / 碎片 / 物品栏；具体打开哪一页取决于上次菜单状态。"},
    {"暂停菜单", "打开游戏暂停菜单。"},
    {"向上移动 / 菜单导航", "十字键上：角色向上方向输入；菜单向上。具体角色动作由当前场景决定。"},
    {"向下移动 / 菜单导航", "十字键下：角色向下方向输入；菜单下移。具体角色动作由当前场景决定。"},
    {"向左移动 / 菜单导航", "十字键左：角色向左方向输入；菜单向左。"},
    {"向右移动 / 菜单导航", "十字键右：角色向右方向输入；菜单向右。"}
}};
}
