# Ascent Training App Implementation Plan

**Goal:** 使用新版战斗区地图交付六种模式的 Windows 对枪训练软件，范围见 `docs/compose/spec/2026-10-06-ascent-training-app.md`。

**Architecture:** 复用现有角色、武器、敌人和对枪计时；新增会话模式控制及原生中文菜单。资产由 UE Python 导入，运行时设置和个人点位保存到 Saved，训练记录由 SaveGame 管理。继续当前工作区，保护现有未提交改动和旧地图。

**Tech Stack:** UE 5.3 C++、UMG/Slate、UE Python、JSON、SaveGame、UE Automation、UAT Win64 Packaging。当前会话 inline 执行，阶段间检查证据。

## 1. 新地图接入

- [x] 创建 `Tools/ue_setup_combat_training.py`，导入 `/Game/CombatTraining/Map/AscentCombat`，保留顶点绝对坐标，三角网格碰撞，创建 `/Game/CombatTraining/TrainerAscent`；正确太阳/天空/固定曝光。
- [x] 创建 Blender 点位分析脚本，验证原始地面、胶囊空间、预瞄 LOS 和可走出掩体的路径，生成 `Content/Data/trainer_combat_spots_v1.json`。
- [x] UE 实际 trace 验证导入后的地面、方向、位置并记录 `Saved/AscentCombatSimple/ue_training_import_report.json`。

执行：
```powershell
& 'H:\Game\EpicGame\UE_5.3\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' ascent1.uproject -run=pythonscript -script=Tools/ue_setup_combat_training.py -unattended -nop4 -nullrhi -stdout
```
预期：新网格及关卡保存成功，验证全部预置点位有可用地面。

## 2. 会话与配置

- [x] 在 `Training/` 新建 `TrainerSession` 管理六模式状态、难度、计分、暂停、波次、随机出枪和背闪。
- [x] 扩展 `TrainingSettings` 配置序列化、本地覆盖和错误输入保护；补齐角色灵敏度应用和急停速度判定。
- [x] 对状态与存储先写 Automation 测试，再实现；保留旧 TrainerDuel/TrainingStats 测试。

编译和测试：
```powershell
& 'H:\Game\EpicGame\UE_5.3\UE_5.3\Engine\Build\BatchFiles\Build.bat' ascent1Editor Win64 Development '-Project=F:\project\gitprojects\UE\valorant\ascent1\ascent1.uproject' -WaitMutex
& 'H:\Game\EpicGame\UE_5.3\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' ascent1.uproject -ExecCmds='Automation RunTests ascent1; Quit' -unattended -nop4 -nullrhi -TestExit='Automation Test Queue Empty' -stdout
```
预期：编译完成；会话、LOS、角度、持久化、命中统计测试成功。

## 3. 中文软件界面

- [x] 新建原生菜单组件，由 `AscPlayerController` 控制开始、返回、暂停和鼠标输入。
- [x] 更新 `TrainingHUD`：可配置准星、模式/血量/波次/结果、地图示意、目标位置。
- [x] 设置页立即应用和保存，设置导入失败不覆盖，历史页读取真实训练记录，个人点位编辑在地图内完成。
- [x] 由 `AscGameMode` 默认进入菜单，以新关卡/新点位开始会话，移除旧六区域展示模式的默认输入。

实际运行中依次验证：六模式入口 → 开始 → 对应敌人行为 → 结果 → 重练/下一点 → Esc 回菜单；设置 → 导出/导入 → 重启；个人点位 → 保存 → 重启读取。

## 4. 打包交付

- [x] 在 `DefaultGame.ini` 只指定新训练关卡及所需数据的 cook/stage 路径，验证 JSON 与地图不遗漏。
- [x] UAT 打包到项目 `Saved/TrainerBuild`，写中文启动/操作说明。
- [x] 启动打包程序测试首页、训练、保存、重启和退出；截图与日志存 `Saved/TrainerVerification`。
- [x] 将验收证据逐项回填 spec/plan，全部通过后才标记整个目标完成。

打包命令：
```powershell
& 'H:\Game\EpicGame\UE_5.3\UE_5.3\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun '-project=F:\project\gitprojects\UE\valorant\ascent1\ascent1.uproject' -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive '-archivedirectory=F:\project\gitprojects\UE\valorant\ascent1\Saved\TrainerBuild' -utf8output
```
预期：归档目录含可运行 EXE，开启后有中文菜单及新版地图，重启仍可读取用户设置与训练记录。

## 完成证据

2026-10-06：Editor/Game 编译和 UAT 打包成功；18 项 Automation 测试成功；独立 EXE 65 项运行检查成功，包括七条跨区域真实角色路线。中文菜单、开火、暂停、退出、设置导入/导出及重启后的设置和记录均经原生窗口操作验证。详见 docs/TRAINER_ACCEPTANCE.zh-CN.md。

14:55 最终复核：F5/F6/F7 原生操作及个人点位跨进程读取通过，采集坐标来自实际地面；独立 EXE 的 65 项检查全部成功。验收产生的个人点位和历史已归档到证据目录，交付版保持默认配置。
