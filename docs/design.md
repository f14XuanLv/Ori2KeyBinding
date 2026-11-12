# OriPadMod 技术记录（2026-11-11）

## 0.1.2 功能语义：本机文件为依据

改键表最左侧为“原版功能”，保留“游戏原按键”和“实际按键”。0.1.2 已使用实际下载的 Il2CppDumper 和 AssetRipper，以及本机 IDA，重新从本机 Steam Build 5845748 的 GameAssembly、metadata 和 resources.assets 核对。逐键原生调用点 RVA、资源 Path ID、原版简中字段值、动作消费者及尚未闭环项见 [本机输入语义核对](local-input-audit.md)。0.1.1 的社区默认表参考已被这份本机证据替代。

L3 对应 MapFocusOri / MapFocusObjective；R3 未分配默认普通指令，但仍存在原始/调试输入注册，因此只写“未分配常用指令”。X/Y/B 是可装备的 Ability1/2/3，采用本机资源术语“能力”。十字键补充方向输入；原版简中使用“冲刺/挖掘”“聚焦奥里”“抓住”。A→DialogueOption3 虽存在初始化，但默认 Awake 未接入每帧刷新字典，已撤掉该未闭环提示。仅修改功能说明，未改变映射规则和用户已保存配置。

## 本地事实

- 游戏安装目录含 `GameAssembly.dll` 与 `oriwotw_Data/il2cpp_data/Metadata/global-metadata.dat`，属于 Unity IL2CPP 后端。
- 官方 `%LOCALAPPDATA%/Ori and the Will of The Wisps/ControllerButtonRemaps.txt` 明确注明 `Only for DirectInput controllers`，不能据此宣称 XInput 重绑定已开放。
- `UnityPlayer.dll` 导入 `VERSION.dll` 的 GetFileVersionInfoA、GetFileVersionInfoSizeA、VerQueryValueA，并包含 XInput 1.4 / 1.3 / 9.1.0 与 XInputGetState 动态解析字符串。
- `oriwotw_Data/Plugins/XInputInterface.dll` 导出 XInputGamePadGetState / XInputGamePadSetState，并动态加载 xinput1_3.dll。

## ELMP 参考

按用户要求只读查看 `D:/CodeT2/ELMP`：

- `native/input/xinput.cpp`：系统真实手柄状态和游戏接收状态分离，通过 XInputGetState 的导入表转接做路由。
- `native/input/bindings.cpp`：按键与 LT/RT 伪位转换为动作；扳机阈值 30。
- `native/party/manager.cpp`：按键边沿捕获、保存设置。
- `native/overlay/manager_draw.cpp`、`render.cpp`：独立 ImGui 覆盖层、D3D11 Present / ResizeBuffers 接缝。

Ori2 不使用 UE4SS / UE 反射，不能搬用 ELMP 的动作注入与角色指令接口。本实现独立编写，沿用“实际输入 → 绑定 → 游戏输入；覆盖层修改绑定”的设计。它当前提供按钮层映射而非动作分拆。

## 实现

1. 游戏目录 `version.dll` 是本 Mod 加载入口，完整保留系统版 17 项导出的名称与序号。MASM 转发桩保存参数寄存器与栈参数，再尾跳到系统实现。系统版通过绝对 System32 路径加载，防止 basename 缓存把代理加载回自身。
2. DllMain 只在 `oriwotw.exe` 或验证程序内启动工作线程。工作线程初始化配置、MinHook 和 D3D11；其他进程只获得系统 version 转发。模块被固定到进程退出，避免游戏继续调用已卸载钩子。
3. 对系统 XInput 1.4 / 1.3 / 9.1.0 的 GetState，以及存在时的序号 100 扩展 GetState 安装钩子。按真实目标地址去重；线程局部重入标志防止跨 DLL 转发导致重复映射。不改 SetState，所以震动仍走原接口。
4. 输入映射使用目标按钮到物理按钮的表。原始状态一次读取，所有目标都从原始状态计算，交换不会递归。每个玩家索引独立维护映射后的 packet number，配置切换与面板置空也能产生状态变化。
5. 按 F8 打开 D3D11 面板。工作线程的引导窗口从不显示，仅用于发现公共 DXGI vtable。实际渲染仅接受 UnityWndClass 或验证窗口的 swap chain。窗口消息只入队，ImGui 仅在渲染线程使用。ResizeBuffers 前释放后台缓冲引用，绘制后恢复原始 render targets。
6. 开面板期间对游戏返回中性手柄状态；捕获直接调用未映射的系统 trampoline，等待全部按钮松开再接收新按钮。不会向系统发送模拟按键，也不会创建虚拟手柄。
7. 配置写入临时文件后 MoveFileEx 原子替换。读取先完整验证再替换当前配置。没有在输入钩子里读写磁盘。断连手柄列表仅面板开启时每秒扫描。

## 依赖

- Dear ImGui v1.91.9b，commit `f5befd2d29e66809cd1110a152e375a7f1981f06`，MIT：[上游](https://github.com/ocornut/imgui/tree/v1.91.9b)。
- MinHook v1.3.4，commit `c3fcafdc10146beb5919319d0683e44e3c30d537`，BSD 2-Clause：[上游](https://github.com/TsudaKageyu/minhook/tree/v1.3.4)。
- 中文字体读取 Windows 自带微软雅黑，不分发字体文件。

## 验证边界

`mapping_tests.cpp` 穷举所有 65,536 种数字按键状态验证默认映射无损；另覆盖交换、去重、扳机阈值、扳机模拟值互换、清空与关闭映射。

`harness.cpp` 在隐藏窗口实测系统 version 查询转发、系统 XInput 真实 API 进入钩子、D3D11 Present 挂载、中文面板实际绘制、非法配置拒绝、合法配置重载、面板打开时 ResizeBuffers、关闭和窗口始终隐藏。实际截图 `build/Release/panel.png` 已人工视觉检查。

以上不代表已验证 Ori2 的实机控制、Steam Overlay 或其它图形注入 Mod 共存。未启动正式游戏，也未读取或修改存档内容。
