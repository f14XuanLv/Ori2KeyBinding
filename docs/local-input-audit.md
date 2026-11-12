# 本机 Ori 2 输入语义核对：Steam Build 5845748

日期：2026-11-11。结论用于 OriPadMod 0.1.2 的“原版功能”列。此记录的语义证据全部来自本机游戏文件；网页只用于下载工具，未使用社区绑定表或在线译名作证明。下述 RVA 均相对 `GameAssembly.dll` 的映像基址 `0x180000000`。

## 游戏身份与原始文件

安装目录：`C:\Game\Steam\steamapps\common\Ori and the Will of the Wisps`。`C:\Game\Steam\steamapps\appmanifest_1057090.acf` 中 `buildid` 与 `TargetBuildID` 均为 `5845748`。相邻 `common\Ori` 未参与本次分析。

| 原始文件 | SHA-256 |
|---|---|
| `GameAssembly.dll` | `475827a37396bdfbc6678b8c430f02bcfb6fcae47162d570266911647a341a89` |
| `oriwotw_Data/il2cpp_data/Metadata/global-metadata.dat` | `c3704eeaab71f0199cfda92dd209799a168bf0f9229e8eb8becd0446d52b35be` |
| `oriwotw_Data/resources.assets` | `533f90c72525cbb0aad29e4ea835ecc0dcaa4f3a4e8284d7d984a5ecafa7dabc` |

原始文件大小、Steam manifest 和工具压缩包的哈希见 [provenance.json](../build/local-evidence/provenance.json)。分析均为读取；IDA 数据库和资源副本写在仓库 `build/local-evidence`，未改原版 EXE、GameAssembly、metadata 或资源文件。

## 实际使用的工具与产物

| 工具 | 本机位置与用途 |
|---|---|
| Il2CppDumper **6.7.46，net6 Windows** | `D:\CodeT2\Ori2\tools\external\Il2CppDumper-6.7.46\Il2CppDumper.exe`；实际处理上述 DLL 与 metadata，输出版本为 **24.1**。来自 [官方 Release](https://github.com/Perfare/Il2CppDumper/releases/tag/v6.7.46)。 |
| IDA Professional **9.3** / Hex-Rays **9.3.0.251224** | `C:\Program Files\IDA Professional 9.3\idat.exe`；实际反编译初始化、消费者与上下文判断。脚本为 [ida_input_evidence.py](../tools/ida_input_evidence.py)，数据库 `build/local-evidence/ori2-input.i64`。 |
| AssetRipper **2.0.0 x64** | `D:\CodeT2\Ori2\tools\external\AssetRipper-2.0.0\AssetRipper.GUI.Free.exe`；实际以 `--headless --port 57741` 启动，通过其本地 OpenAPI 读取资源 JSON。来自 [官方 Release](https://github.com/AssetRipper/AssetRipper/releases/tag/2.0.0)。 |

Il2CppDumper 命令：

```powershell
& .\tools\external\Il2CppDumper-6.7.46\Il2CppDumper.exe `
  'C:\Game\Steam\steamapps\common\Ori and the Will of the Wisps\GameAssembly.dll' `
  'C:\Game\Steam\steamapps\common\Ori and the Will of the Wisps\oriwotw_Data\il2cpp_data\Metadata\global-metadata.dat' `
  'D:\CodeT2\Ori2\build\local-evidence\il2cpp'
```

输出 `dump.cs`、`script.json`、`il2cpp.h`、`stringliteral.json` 与 95 个 DummyDll。CodeRegistration VA 为 `0x183BAA660`，MetadataRegistration VA 为 `0x183BAAD50`。

IDA 使用 `-A -a -Opdb:off -S<脚本>`，在数据库副本中逐函数 `plan_and_wait`，导出伪代码、完整指令文本和带调用点 RVA 的 `.calls.json`。完整可回查资料在 [IDA 导出目录](../build/local-evidence/ida)。类型字段由本次 `dump.cs` 导入，用于解释原生对象偏移，未把“存在某个类型名”当作功能已实现的依据。

AssetRipper 读取的是 `build/local-evidence/asset-stage/oriwotw_Data` 中与安装版哈希一致的 `resources.assets`、两个 globalgamemanagers 文件和本次 DummyDll。DummyDll 只提供自定义字段结构，因而分析副本被 AssetRipper 识别成 Mono；**原版游戏仍是 IL2CPP**。部分无关 AnimationClip / ParticleSystem 类型解析报错；本记录引用的 `TranslatedMessageProvider` 均成功解出了非空 `m_Structure.Messages`。未以这些无关错误资产作证据。

## 原版按钮到逻辑输入的链路

1. `SmartInput.XboxControllerInput.GetButton`，**RVA `0x6CE020`**，XInput 已连接分支直接按枚举读取控制器状态位：A=`0x1000`、X=`0x4000`、Y=`0x8000`、B=`0x2000`、LB=`0x0100`、RB=`0x0200`、L3=`0x0040`、R3=`0x0080`、Back/Select=`0x0020`、Start=`0x0010`。LT/RT 在该分支判断模拟值 **大于 20**。这不是拿枚举名称猜物理键。
2. `ControllerButtonInput.GetValue`，**`0x6CB500`**，读取对象 `Button` 字段（`+0x18`）并调用上述 GetButton；`.ctor` 在 **`0x6CB4D0`** 保存枚举。`CompoundButtonInput.Add` 在 **`0x6CABE0`** 把输入对象加入对应逻辑按钮集合，`GetValue` 在 **`0x6CA8E0`** 聚合这些来源。
3. `PlayerInput.AddControllerControls`，**`0x1411A70`**，实际创建输入对象并调用 Add。`PlayerInputRebinding..cctor`，**`0x14261D0`**，在 **`0x1426276`** 把 `USE_NEW_BINDINGS_TEST` 设为 1，在 **`0x1426288`** 把 `DEBUG_CONTROLS_ENABLED` 设为 1。下表采用前者为真的默认分支，未混入 legacy else 分支。
4. `PlayerInput.FixedUpdate`，**`0x1416A40`**，刷新输入缓存并更新 `InputButtonProcessor`。`InputButtonProcessor.Update`，**`0x11FE830`**，维护 WasPressed（`+0x10`）、IsPressed（`+0x11`）；`OnPressedNotUsed`，**`0x11FE880`**，检查按下沿和 Used（`+0x12`）。因此同一物理键的多个逻辑输入不是所有场景无条件同时执行。

默认分支的全部 Add 边已提取为 [native-bindings.json](../build/local-evidence/native-bindings.json)。提取辅助脚本是 [summarize_local_input.py](../tools/summarize_local_input.py)；它明确只解析此构建的默认分支，不跨 legacy else 分支合并 Hex-Rays 复用的局部变量。该 JSON 用于审阅，原始指令和调用点仍为核对依据。

## 逐键核对表

“调用点”均为 `PlayerInput.AddControllerControls` 内的原生 **Add 调用 RVA**，不是仅凭类型或方法名建立的关联。资源对象均在本机 `oriwotw_Data/resources.assets`；`#数字` 为 **原始 Path ID**，本地化字段均为 `m_Structure.Messages[0].Chinese`。完整字段 JSON 见 [localization 目录](../build/local-evidence/localization)，精简索引见 [localization-summary.json](../build/local-evidence/localization-summary.json)。

| 原按钮 | 本机默认逻辑输入及调用点 | 本机资源名称、Path ID 与中文值 | 面板说明及已证实场景 |
|---|---|---|---|
| A | Jump `0x14134A9`；MenuSelect `0x141373D`；DialogueAdvance `0x14135BD`。DialogueOption3 另有初始化 `0x141365F`，但刷新链未闭合 | `jumpTextMessageProvider` #24275=`跳跃`；`menuSelectTextMessageProvider` #24173=`选择`；选项3 #24134=`选项3` | **跳跃 / 菜单选择**。保留推进对话。A→选项3仅确认了初始化，默认 Awake 未将 DialogueOption3 接入处理器字典；面板已撤掉“选项3”这一未闭环用途。 |
| B | Ability3 `0x14134FA`；MenuBack `0x1413758`；MenuClose `0x1413773`；DialogueExit `0x141367A`；DialogueAdvance `0x14135D8` | `ability3TextMessageProvider` #24270=`能力 3`；`menuBackTextMessageProvider` #24164=`返回`；`menuCloseTextMessageProvider` #24165=`关闭` | **能力 3 / 返回**。普通界面取消/返回和关闭，对话退出；游戏中是可装备 B 槽。 |
| X | Ability1 `0x14134C4`；Interact `0x141348E`；MainMenuSaveCopy `0x1413458`；MapFilter `0x14138C4`；DialogueOption1 `0x1413629`；DialogueAdvance `0x14135F3` | `ability1TextMessageProvider` #24268=`能力 1`；`interactTextMessageProvider` #24151=`操作`；`mapFilterTextMessageProvider` #24156=`地图筛选器`；选项1 #24132 | **能力 1 / 操作**。悬停列出对话、地图筛选和主菜单复制存档用途。复制存档的具体后续流程未另行执行。 |
| Y | Ability2 `0x14134DF`；MainMenuSaveDelete `0x1413473`；MapDetails `0x14138DF`；DialogueOption2 `0x1413644`；DialogueAdvance `0x141360E`；LiveSignIn `0x14136EB` | `ability2TextMessageProvider` #24269=`能力 2`；`mapDetailsTextMessageProvider` #24154=`详细信息`；选项2 #24133 | **能力 2 / 详细信息**。悬停说明对话和主菜单删除存档。LiveSignIn 只确认初始化绑定，未验证平台登录可达性，因此不在主要功能中宣传。 |
| LB | Bash `0x1413586`；Grapple `0x14135A2`；MenuPageLeft `0x1413872`；LeaderboardCycleFilter `0x14138A9` | `grappleAndBashTextMessageProvider` #24274=`猛击/抓钩` | **猛击 / 抓钩**；菜单左翻页、排行榜筛选为附加逻辑输入。不能将两种角色动作当作可在此 Mod 中单独拆绑。 |
| RB | Dash `0x141354E`；Burrow `0x141356A`；MenuPageRight `0x141388E` | `dashAndBurrowTextMessageProvider` #24271=`冲刺/挖掘` | **冲刺 / 挖掘**。原标签“钻沙”改为资源原词“挖掘”；菜单右翻页。冲刺消费者已直接读到 Dash；挖掘的整个状态机未穷尽。 |
| LT | OpenWeaponWheel `0x14136B5`；MapZoomOut `0x1413722`；原始 LeftTrigger `0x1413303` | `toggleWeaponWheelTextMessageProvider` #24185=`展开能力轮盘`；`mapZoomOutTextMessageProvider` #24163=`缩小` | **展开能力轮盘 / 缩小**。装备 UI 也读取原始 LeftTrigger 切换类别，见后文消费者。 |
| RT | Glide `0x1413516`；Grab `0x1413532`；MapZoomIn `0x1413707`；原始 RightTrigger `0x141331F` | `glideTextMessageProvider` #24273=`滑翔`；`justgrabTextMessageProvider` #24152=`抓住`；`mapZoomInTextMessageProvider` #24162=`放大` | **滑翔 / 抓住 / 放大**；Grab 的语义按资源改为“抓住”。装备 UI 也读取原始 RightTrigger 切换类别。 |
| L3 | 原始 LeftStick `0x1413372`；MapFocusOri `0x14138FF`；共用尾部 MapFocusObjective `0x1413934`，均来自 Button 枚举 8 | `mapFocusOriTextMessageProvider` #24158=`聚焦奥里`；`mapFocusObjectiveTextMessageProvider` #24157=`聚焦目标` | **聚焦奥里 / 聚焦目标**。原词是“奥里”；具体位置取决于当前任务/地图状态。只改摇杆按下，不改轴。 |
| R3 | 原始 RightStick 在 `0x141338F` 注册，枚举 9；未作为默认分支普通 Cmd Add 的来源 | 无普通动作译名；`Input.Dbg.RightStick` 静态字段偏移 `0x60`，由 PlayerInput.Awake `0x1414C70` 接入原始处理器 | **未分配常用指令**。只证明该默认 Cmd 表没有普通指令绑定；底层/调试输入确实存在。不能据此声称 R3 在整个游戏所有代码路径完全无用途。 |
| Back / Select | OpenMapsShardsInventory `0x1413695`；原始 Select `0x141333A` | `openMapShardsTextMessageProvider` #24181=`打开地图 / 碎片 / 物品栏` | **地图 / 碎片 / 物品栏**。消费者使用 OpenLastMenu 按下沿，受当前/上次页面状态影响。 |
| Start | PauseScreen `0x14136D0`；原始 Start `0x1413355` | `pauseTextMessageProvider` #24184=`暂停`；`openInventoryTextMessageProvider` #24278=`主菜单` | **暂停菜单**。消费者读 OpenPauseScreen；不是全局无条件暂停，CanOpenMenus 还有场景限制。 |
| 十字键上 | AxisButtonInput：DpadY 枚举5、GreaterThan 枚举1、阈值 `+0.5` → MenuUp `0x14137C7`；方向轴也进入角色输入 | `menuUpTextMessageProvider` #24174=`菜单上` | **向上移动 / 菜单导航**。“移动”指向上方向输入，不保证任意场景有向上位移。 |
| 十字键下 | DpadY 枚举5、LessThan 枚举0、阈值 `-0.5` → MenuDown `0x141378F`；方向轴也进入角色输入 | `menuDownTextMessageProvider` #24168=`菜单下移` | **向下移动 / 菜单导航**。没有把下蹲、下砸等全部场景动作追加成已穷尽功能清单。 |
| 十字键左 | DpadX 枚举4、LessThan 枚举0、阈值 `-0.5` → MenuLeft `0x14137FF`；方向轴也进入角色输入 | `menuLeftTextMessageProvider` #24169=`左菜单` | **向左移动 / 菜单导航**。面板组合语句采用可读的方向描述，不照搬原译中的“左菜单”语序。 |
| 十字键右 | DpadX 枚举4、GreaterThan 枚举1、阈值 `+0.5` → MenuRight `0x1413837`；方向轴也进入角色输入 | `menuRightTextMessageProvider` #24172=`右菜单` | **向右移动 / 菜单导航**。同上。 |

## 已检查的消费者与上下文判断

- **菜单确认/取消**：`CleverMenuItemSelectionManager.FixedUpdate` **`0x12B6F50`** 消费 `MenuSelect` / `MenuBack` 和四个 Menu 方向处理器，再调用 `PressCurrentItem` **`0x12BAB60`** / `OnBackPressed`。处理器读取包含 IsPressed、WasPressed 与 Used 的检查。`GameMapUI.HandleInput` **`0x6AA310`** 对未消费的 MenuBack 按下沿退出地图或回到普通地图状态，并将该输入标记 Used。
- **可装备槽位**：本机构建 `SpellInventory.Binding` 为 ButtonX=0、ButtonY=1、ButtonB=2；`SpellInventory.BindingToButton` **`0xEFA160`** 将其返回成 Input.Command 的 Ability1=3、Ability2=4、Ability3=5。`SpellUIEquipScreen.BindButtonIfPressed` **`0x94B370`** 先 `Core.Input.GetCommand(command)`，检查输入，再调用 `SpellInventory.UpdateBinding`（**`0xEF8F80` / `0xEF91A0`**，重载）。这证明是可换装备槽，而非固定剑/弓等武器。资源原词是“能力 1/2/3”，“槽”是对已证实机制的解释。
- **角色能力**：`SeinBashAttack.UpdateNormalState` **`0xD8C6D0`** 读 Bash 处理器并调用 `get_CanBash` **`0xD86420`**；`SeinSpiritLeashAbility.get_ActivationButton` **`0x5E5E10`** 返回 Grapple；`SeinDashNew.get_WantsDash` **`0x6F42E0`** 读 Dash；`SeinGlide.UpdateCharacterState` **`0xAE3630`** 读 Glide；`SeinGrabBlock.ShouldLetGo` **`0x14B2450`** 读 Grab。已核对这些消费者而非只据类名推断，但未穷尽每项能力所有可执行条件。
- **地图定位**：`GameMapUI.NormalInput` **`0x6AA4D0`** 分别检查 MapFocusOri 和 MapFocusObjective 的未消费按下沿，共用后续分支；读取当前 ActiveQuest、任务字段和当前焦点位置后计算目标。因此 L3 标签列出两个逻辑用途，并明确受当前任务/地图状态影响。
- **打开界面**：`MenuScreenManager.get_MainMenuOpenKeyPressed` **`0x9DA8F0`** 读 OpenPauseScreen；`get_OpenLastMenuKeyPressed` **`0x9DA9C0`** 读 OpenMapsShardsInventory；`get_EquipmentWheelKeyDown` **`0x9DAD00`** 读 OpenWeaponWheel。`CanOpenMenus` **`0x9DCD60`** 有多处早退条件，所以标签不承诺在剧情等任何状态下都能打开菜单。
- **十字键角色输入**：本机 PlayerInput 的 `ALLOW_D_PAD_FOR_CONTROLLER_MOVEMENT=True` 声明之外，已实际检查 `FixedUpdate` **`0x1416A40`**：Controller 方案（枚举0）把 Dpad 轴舍入后并入 HorizontalKey / VerticalKey。`RefreshControls` **`0x1417880`** 再与模拟摇杆合成 Horizontal / Vertical，并更新 Down / Up / Left / Right。这是补上“移动”用途的原生实现证据，而非仅依据常量名。
- **同键多用的其他消费者**：`InteractiveMessageBox.UpdateComplete` **`0x63C790`** 和 `NPCMessageBox.UpdateComplete` **`0x880910`** 已反编译核对对话处理；`SpellUIEquipScreen.UpdateCategory` **`0x94AC60`** 读取原始 LeftTrigger / RightTrigger。RT/LT 还存在上下文用途，面板悬停说明是已检查用途的摘要，不是全部代码路径的穷尽列表。
- **对话选项3的断点**：虽然 AddControllerControls 给 `DialogueOption3` 加了 A，且 NPCMessageBox.UpdateComplete 有读取该 Cmd 的分支，但 `PlayerInput.Awake` **`0x1414C70`** 的当前默认分支只把 DialogueOption1、DialogueOption2 和 DialogueExit 接进 `m_InputProcessorPairs`，未接入 DialogueOption3；FixedUpdate 正是遍历此字典刷新处理器。本次仅能确认初始化和消费者存在，不能确认该手柄路径实际生效。0.1.2 撤掉 A 悬停中的“选项3”，此项留待进一步代码路径或运行实测。其余默认普通 Add 字段均在该 Awake 分支找到了对应接线。

## 修正、验证与尚未确认的范围

从 0.1.1 修正为 0.1.2：使用本机简中原词“能力”“操作”“挖掘”“抓住”“聚焦奥里”；补充十字键的方向输入；R3 保留“没有默认常用指令”的有限结论，并注明底层/调试输入存在；撤掉尚未闭环的 A→对话选项3说明。未修改任何用户已保存映射、输入转换规则或扳机阈值。原版 XInput 阈值是大于20；本 Mod 的默认捕获/数字转换阈值仍为30，二者不是同一项设置。

已通过 x64 Release 编译和隐藏 D3D11 集成程序；重新生成并目视检查了中文面板截图。这证明新增说明可正常显示，不能替代游戏行为验收。

尚未通过真实游戏输入验证：每个剧情/战斗/地图状态下的触发结果、全部隐藏调试用途、平台登录可达性，以及同键动作在所有状态中的优先级。本次没有对用户运行中的隔离游戏发送输入，也没有执行复制/删除存档流程。上述所有“默认”“未分配”结论限定于所记录本机构建和已经分析的初始化/消费者范围。

证据原始文件仅保存在开发工作区，不随 Mod 发布包分发游戏二进制、DummyDll 或完整反编译代码。
