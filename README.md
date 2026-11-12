# OriPadMod 0.1.2

《奥日与萤火意志》Windows / Steam 版的原生 C++ 手柄改键 Mod。在游戏里按 **F8** 打开中文面板，点击右侧绑定后按手柄按钮，立即生效；点击“保存配置”保留到下次启动。

## 安装与使用

1. 退出游戏，运行发布包里的 `Install.cmd`。默认游戏目录为 `C:\Game\Steam\steamapps\common\Ori and the Will of the Wisps`。其他目录可运行 `powershell -ExecutionPolicy Bypass -File Install.ps1 -GameDir "你的游戏目录"`。
2. 正常从 Steam 启动游戏。首次启动使用原版映射。
3. **先暂停游戏，再按 F8**。面板不会自动暂停世界；面板打开期间手柄输出会被置空，防止捕获时操作角色。
4. 点击要修改的原版按键，松开手柄所有按钮，再按新按钮。默认自动交换冲突绑定，例如 A 改为 X 时，原来的 X 同时改为 A。
5. 点击“保存配置”，按 F8 / Esc 或右上角 × 关闭。捕获期间 Esc 只取消捕获。

面板只需要用鼠标点击，不要求你先猜出当前的手柄菜单确认键。恢复默认后也需要点击保存，才会更新磁盘配置。

改键表从左到右为“原版功能 / 游戏原按键 / 实际按键 / 清除”。悬停原版功能可看已核对的游戏、菜单和地图用途。X/Y/B 显示为可装备的能力槽，不固定写成某个武器；功能说明始终对应原版按钮，不随右侧的物理按键绑定变化。

0.1.2 的绑定说明已改为从本机 Steam Build 5845748 的原生初始化、动作消费者和简中资源核对。原版用词包括“冲刺/挖掘”“聚焦奥里”“能力 1/2/3”“抓住”。十字键也有角色方向输入用途；R3 未分配默认常用指令，但仍有底层调试输入注册。逐键方法 RVA、资源对象 ID 和静态核对边界见 [本机输入核对记录](docs/local-input-audit.md)。

## 范围与限制

- 支持 XInput 手柄，包括 Xbox，以及切换到 XInput 模式的北通 / 八位堂等。PS 手柄需通过 Steam Input / DS4Windows 等转为 XInput。原生 DirectInput 不由此 Mod 处理。
- 支持 A/B/X/Y、LB/RB、LT/RT、L3/R3、Back/Start 和十字键。模拟摇杆轴保持原样，震动原接口不变。按钮映射到扳机时产生满幅扳机；扳机互换保留模拟量。
- **这是手柄按键层重映射**。原版同一按钮控制的多个动作会一起改变，不能在本版中将这些动作拆开单独绑定。菜单同样使用映射，游戏原有按键图标不会自动更新。
- 同一配置应用于全部 XInput 手柄；“捕获手柄”选择的是录入来源。若重复绑定为有意设置，可取消“自动交换冲突绑定”。
- 面板使用 D3D11。若游戏被设置成其他渲染后端，可在 Steam 启动选项添加 `-force-d3d11`。不支持运行时卸载 DLL；安装、更新、卸载前退出游戏。
- 不依赖 Mono、BepInEx 或 UE4SS。原版 EXE、GameAssembly.dll、存档与官方按键配置不被修改。

## 配置与卸载

配置：`游戏目录\OriPadMod\bindings.ini`。每行含义为 **游戏原按键 = 实际手柄按键**，例如：

```ini
Enabled=True
TriggerThreshold=30
A=X
X=A
```

未列出的项使用默认映射；`None` 表示不绑定。面板内“重新读取”可以加载手工修改的文件。无效配置整份拒绝，继续使用当前配置。重启后无效文件会使用默认映射，并在日志里留下诊断。

日志：`游戏目录\OriPadMod\OriPadMod.log`。遇到问题可先确认日志是否有 `XInput hook enabled` 与 `Overlay attached to game D3D11 swap chain`。

运行 `Uninstall.cmd` 移除 Mod DLL。配置与日志保留，便于重装。也可在退出游戏后手动删除本 Mod 安装的 `version.dll`。安装脚本不会覆盖其他 Mod 的同名 DLL；同样不会卸载已被其他工具替换的 DLL。

## 开发与验证

工具链：Visual Studio 2022（x64 C++ 与 Windows SDK）、CMake 3.24+。

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release -j 6
ctest --test-dir build -C Release --output-on-failure
```

`build\Release\OriPadHarness.exe` 是完全隐藏的 D3D11 集成验证程序，工作目录应设为 `build\Release`。它不启动游戏，不发送系统输入、不操作用户窗口；验证代理导出、真实系统 XInput 调用经过钩子、面板渲染、配置拒绝与重新读取、ResizeBuffers 和关闭，并生成 `harness.log` 与 `panel.bmp`。

当前验证：映射穷举测试及隐藏 D3D11 集成测试通过。**尚未完成真实游戏中实机手柄、Steam Overlay 共存和长时间游戏验收**，请按此边界使用第一版。

架构和参考来源见 [docs/design.md](docs/design.md)。
