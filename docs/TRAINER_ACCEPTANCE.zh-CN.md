# 交付验收：亚海悬城对枪训练

2026-10-07，UE 5.3，Windows Development 独立程序。参考 SpikeAtlas 的六种训练模式、设置和记录功能，使用用户提供的原地图制作。

| 要求 | 当前证据 |
|---|---|
| 简化原地图，保留战斗地形、主要掩体及原有墙面/箱体高度参照 | `Saved/AscentCombatSimple/simplification_report.json`：643 个部件、289560 三角形，面数减少 92.17%；源文件 SHA256 未变化 |
| 必要墙体与包点障碍物 | 从原地图补回 142 个部件，包括中路坡道侧墙、庭院箱体、卷帘门、B 船屋内外墙及包点箱体/遮挡物；按实际边界覆盖范围保留必要大墙 |
| 几何与坐标保真 | `verification_report.json`：643 个单用户网格、22403 个原始表面采样、FBX 往返尺寸误差最大 0.0000153 米；`ue_cover_update_report.json`：原关卡 6 个 Actor 保留，145 个材质槽，复杂双面碰撞通过 |
| 六种模式 | 最新独立 EXE 的 `packaged_hold_rifle_runtime_report.json`：探索、预瞄、架枪、背闪、防守五波、实战三目标通过 |
| 架枪固定 200 毫秒 | 所有难度固定时限；首次可射击部位露出才计时；剩余时间及进度条可见；及时爆头获胜，超时生命归零，暂停不消耗时限；延迟射击不能利用 Tick 顺序判胜 |
| 第一人称枪模与弹道 | 原创简化步枪、双手、枪口闪光、枪体后坐与行走起伏；模型与弹道无碰撞；弹道起点为枪口，终点与真实身体/墙体命中一致，敌人开枪有弹道和受伤红边 |
| 原地图内移动 | 上一轮地图恢复版的 `packaged_cover_hit_runtime_report.json`：七条路线、740 个检查点，使用 CharacterMovement 输入验证，跨目标不传送；本轮地图不变，复查探索和回合结束行走，未重复长路线 |
| 敌人碰撞与多角度防守 | 七个守点各有至少两条通过 UE 胶囊和视线检查的路线；实际五波从不同方向出枪，全部完成 |
| 命中、爆头、反应、胜负和暂停 | 最新报告：手臂一枪加腿部两枪共 108 伤害击杀、躯干三枪、一枪爆头、墙体遮挡、重练/下一点、暂停和历史磁盘重新加载均通过 |
| 身体与爆头反馈 | `BodyHitFeedback`：身体命中 40、剩余 60、独立材质闪白和世界火花；`HeadshotKillFeedback`：头部 160、爆头击杀徽记；独立身体/头部/击杀/受伤音效资源均加载成功，截图已查看 |
| 对枪计数与受伤 | `CombatConfirmedHit1..5`：身体三枪后命中 3、击杀 1，随后两次爆头后命中 5、击杀 3；`CombatPlayerHurtFeedback`：玩家生命 60、红边和扣血提示；三次击杀均触发对应徽记 |
| 反馈与实际判定一致 | 墙体挡住的子弹不触发命中确认；重练清除反馈；回合结束后标记/徽记仍正常淡出；世界火花无碰撞，地图必要墙体 14 项碰撞探针再次通过 |
| 失败回合提示 | `FailedRoundClearsEnemy`：失败后敌人清除，停止射击和计分；`packaged_failed_round_preview.png`：中央显示“本轮失败”和 R 重练 / N 下一点 / Esc 菜单，截图已查看 |
| 闪光反馈 | 实际朝向两次起爆得到正面 1.0、背面 0.12 的强度；起爆前射击不能完成；`flash_facing.png` 显示白屏反馈 |
| 设置和导入/导出 | 原生窗口中 0.4→0.5，cm360 40.82→32.66，导出文件为 0.5；导入 0.4 后立即显示 40.82；无效 9999 输入保持原配置 |
| 重启保存读取 | 设置中的粉色准星保存后退出，重新启动仍选中粉色；训练记录保留实际探索射击 1 次；随后恢复默认青色并归档验收记录 |
| 个人点位 | 原生 F5/F6/F7 采集脚下地面并保存；完全退出后重新启动，菜单出现“个人点位 1”，选择后实际进入训练。截图 `packaged_personal_saved.png`、`packaged_personal_restart.png`、`packaged_personal_play.png`；另有缺失采集拒绝及新会话读取检查 |
| 快捷键与复杂地形 | 移除本项目 F5/F6/F7 的 UE 默认调试绑定，F5 采集保持正常着色；采集使用真实地面命中位置，避免胶囊离地间隙造成保存拒绝。原生快捷键已验证 |
| 中文界面、输入和小地图 | `packaged_menu.png`、`packaged_fire.png`、`packaged_settings_restart.png`、`packaged_history_restart.png`；开始、鼠标开火、Esc、退出、设置按钮均实际操作 |
| 编译、测试、打包 | `TrainerHoldRifleFinalPackaging.log`：BUILD SUCCESSFUL / ExitCode 0；`HoldRifleFinalGreen/index.json`：25 项成功、0 失败；`packaged_hold_rifle_runtime_report.json`：104 项成功、0 失败 |
| 回合结束后的行走 | 最新 `CompletedRoundStillWalks`：一枪爆头完成本轮后，实际行走 62.45 厘米，保持 Walking；枪数保持 1，结束后不再射击或计分 |
| 敌人头部 | 头部直径 18 厘米，与爆头判定一致；添加连接身体的颈部；自动化检查站姿和蹲姿的头部、颈部连接，独立程序 `LiveEnemyHeadSize` 和爆头击杀通过，截图 `packaged_enemy_head_preview.png` |

证据目录：`Saved/TrainerVerification`。最新打包程序目录：`Saved/TrainerBuild_HoldRifle/Windows`，入口为该目录的 `ascent1.exe`；旧包保留。完整中文操作说明：`docs/TRAINER_README.zh-CN.md`，随程序附带。

初版独立程序验证于 2026-10-06 14:55 完成：65 项全部通过，耗时 121.52 秒。躯干伤害检查从独立的新回合开始，避免消耗前一个探身检查的反应时间。初版游戏 EXE 的 SHA256：`7a023b2c20616b2c7a4c8bedd53cbd5d16ab3e94f9f2168a68c649c1e0684f86`。

2026-10-06 18:10 鼠标修复：UE 已将屏幕竖直位移转换为向上为正的 MouseY，项目 LookUp 的 -1 映射又将其反向。映射改为 +1，默认灵敏度和 cm/360 不变。新增四方向回归检查先复现上下方向失败，再与其余测试一起通过（19 项成功、0 失败）。重新打包成功，独立程序本次 56 项检查通过，用户存档恢复检查通过；地图未变化，本次未重复跨地图长路线检查。

本次证据：`MouseMappingRed`、`MouseMappingGreen`、`packaged_mouse_runtime_report.json`、`TrainerMousePackaging.log`、`TrainerMouseStartup.log`。新版游戏 EXE SHA256：`795475958e0c50f760c6a6bb5866df1f71502e5cd14bec4421d0de9f6e5f16e6`。

2026-10-06 18:47 行走与头部修复验收完成。原问题是回合结束时把 CharacterMovement 切为 MOVE_None，实机回归先复现行走距离 0；修复后仅停止当时的速度，保留角色移动模式。敌人原有的小头部扩大到 9 厘米半径，并添加无碰撞的颈部，站姿及蹲姿中心高度保持对应参照。

20 项自动化检查全部通过；最终交付入口启动的独立程序 67 项全部通过，耗时 124.05 秒，包括七条路线的 744 个连续行走检查点、防守五波、实战三目标、暂停和原存档字节恢复。头部截图等待敌人从掩体后探出；截图已查看。打包过程中一次启动器资源写入返回错误 5，重新暂存后资源写入成功，最终从根目录 `ascent1.exe` 启动并完成上述全部检查。

本次证据：`movement_locked_report.json`、`MovementHeadFinal/index.json`、`packaged_movement_head_runtime_report.json`、`packaged_enemy_head_preview.png`、`TrainerMovementHeadFinalRuntime.log`、`TrainerMovementHeadFinalPackaging.log`、`TrainerMovementHeadRestage.log`。最终游戏 EXE SHA256：`0c85512113b73e00042771a9e75b26aca7a2a8fea5763854759e8ea408af3bb3`。本次保留地图几何、用户设置、个人点位及训练存档。

实战命中流程的自动化会明确标注测试定位；地形可达性另由七项连续角色行走检查证明。没有用这些测试定位替代软件内的玩家移动。软件运行时切换目标保持玩家实际位置。

2026-10-06 19:30 命中和必要墙体修复验收完成。旧游戏日志显示架枪失败时 shots=0、hits=0，回合结束后敌人仍显示，而武器已停止射击；另有仅用头部与胸部小球判定、遗漏可见手臂/腿/颈部的问题。新增真实射线与可见身体圆柱相交，按头部 160、躯干/手臂/颈部 40、腿部 34 计伤害，敌人生命值 100；结束时清除敌人并显示中央重练提示。

地图恢复直接使用原始模型部件。旧筛选把中路坡道侧墙、B 船屋基础/墙体、木箱和必要遮挡物按名称视为装饰；还把中心位于战斗区外、实际延伸到战斗区内的大墙排除。现在优先保留功能墙体与掩体，并按实际边界范围保留跨界部件。补回 142 个部件，最终 643 个部件、289560 个三角形；保留原始表面与爆头高度参照。两个敌人站位随恢复后的掩体调整，七个点位及各点五条防守路线通过几何和 UE 校验。

回归先复现身体命中遗漏、失败后残留敌人和必要模型筛除，再修复。21 项引擎自动化检查通过。最终根目录启动器启动的独立程序 84 项检查全部通过，耗时 124.31 秒，包括 14 处原墙体/掩体碰撞、七条路线的 740 个连续行走检查点、真实手臂/腿部三枪击杀、失败回合清除敌人、六种模式和原存档字节恢复。运行截图已检查。用户原包中的 preferences.json 与 TrainingData.sav 已复制到新包，源/目标 SHA256 一致；本次原包不存在个人点位文件。

本次证据：`HitVolumeRed/index.json`、`failed_round_red_report.json`、`CombatRetentionRed.log`、`CombatBoundaryRed.log`、`CombatRetentionFinal.log`、`CoverHitGreen/index.json`、`packaged_cover_hit_runtime_report.json`、`packaged_failed_round_preview.png`、`cover_hit_user_data_copy.json`、`TrainerCoverHitFinalPackaging.log`、`TrainerCoverHitFinalRuntime.log`。游戏 EXE SHA256：`a13a1a91f82f9bddbc6280876042f0195fafe83475d2718cdff7f0e9a8a9911a`。

2026-10-06 19:58 受击反馈版验收完成。旧版身体命中仅播放通用音效，未改变模型颜色，也没有屏幕命中标记、部位伤害提示或爆头击杀徽记。先用 `HitFeedbackRed/index.json` 复现身体闪白缺失，`hit_feedback_red_runtime_report.json` 复现身体与爆头反馈状态为空。对枪原计数在旧版实际命中时可正常增加，因此本次保留原伤害/计数规则，补足即时视觉和听觉确认，并增加每次真实命中的部位、伤害和计数日志。

新反馈使用每个敌人独立的闪白动态材质、无碰撞的短生命周期火花、白色身体/金色头部命中标记与伤害数字；击杀徽记在画面下方显示，爆头使用金色圆环和红色标记。身体与头部使用不同的原创合成音效，另有击杀和玩家受伤音效。受到敌人伤害时，画面边缘变红并显示剩余生命。反馈依据实际命中判定，不以准星位置或射击动画代替命中；回合完成后继续淡出，重练清除。

22 项自动化检查成功，0 失败。最终入口启动的独立程序 91 项检查全部通过，耗时 25.11 秒。对枪实际过程：玩家先受伤，生命降为 60；身体三次命中使敌人剩余生命 60→20→0，命中 3、击杀 1；随后两次爆头，最终命中 5、爆头 2、击杀 3、胜利，玩家位置在新目标生成时保持连续。战斗测试为明确标注的自动化定位，本次不据此宣称跨地图长路线检查。

本次证据：`HitFeedbackRed/index.json`、`hit_feedback_red_runtime_report.json`、`HitFeedbackFullGreen/index.json`、`packaged_hit_feedback_runtime_report.json`、`packaged_body_hit_feedback.png`、`packaged_headshot_kill_feedback.png`、`packaged_player_hurt_feedback.png`、`hit_feedback_user_data_copy.json`、`TrainerHitFeedbackFinalPackaging.log`、`TrainerHitFeedbackFinalRuntime.log`。最终游戏 EXE SHA256：`4495745a946b446276e42a56d2d842bbcec5558217cf0734bf5b1e079c7597f9`。原包设置和训练存档按最近修改时间选取后复制到新包，源/目标 SHA256 核对一致。

反馈风格参考 [Riot 命中判定说明](https://playvalorant.com/en-gb/news/dev/the-state-of-hit-registration/) 和 [1.07 更新说明](https://playvalorant.com/en-gb/news/game-updates/valorant-patch-notes-1-07/)。音效和徽记为本项目制作，未使用原游戏素材。

2026-10-07 架枪与持枪版验收完成。用户确认“0.2ms”指 0.2 秒，即 200 毫秒。架枪从假人第一次露出可被射击的部位开始，所有难度的截止时间固定 200 毫秒；暂停冻结倒计时。倒计时不会因掩体遮挡重新开始；完全被掩体挡住时不穿墙击杀玩家，重新露出后仍使用原截止时间。超时检查同时进入射击处理，避免同一帧中开枪先于敌人 Tick 而误判获胜。

先用 `HoldRifleRed/index.json` 复现旧架枪时限 650/400/250 毫秒、无枪模、无可见弹道。新增 77 厘米简化步枪组件和持枪双手，枪口闪光、枪体后坐、轻微行走起伏；全部无碰撞、仅所有者可见。使用短生命周期视觉弹道显示实际命中，枪口靠墙时裁剪显示起点，保留原准星射线伤害规则。`Tools/ue_setup_rifle_material.py` 可重新生成独立枪模材质，未修改地图与光照。

最终 25 项引擎自动化、104 项根目录 EXE 启动的原生检查通过，独立程序验证耗时 31.79 秒。首次架枪爆头反应 7.909 毫秒获胜；暂停 1.204 秒前后均剩余 73.886 毫秒。截止后下一次游戏帧在 216.569 毫秒触发敌人开枪，玩家生命归零；另一个明确禁用会话 Tick 的测试在 215 毫秒后射击，仍判负且不增加命中、击杀。14 处必要墙体/掩体碰撞和探索/回合结束实际行走继续通过；本轮未修改地图或重复七条长路线。

最终截图已查看：`packaged_hold_countdown.png` 显示 200 毫秒预算及剩余 158 毫秒；`packaged_hold_deadline.png` 显示生命 0、受伤红边和失败提示；`packaged_hold_rifle_body_hit.png` 显示枪模、双手、枪口效果和真实身体命中弹道。证据另有 `HoldRifleFinalGreen/index.json`、`packaged_hold_rifle_runtime_report.json`、`hold_rifle_user_data_copy.json`、`TrainerHoldRifleFinalPackaging.log`、`TrainerHoldRifleFinalRuntime.log`。最终游戏 EXE SHA256：`5e743277cae94b5f031d7d2a0565998599e3a00d71019d82d8c816199363cd71`。

原始文件：`H:\baiduyun\亚海悬城 - 副本.blend`。此前读取的 SHA256：`fff802f03769b19136bcfdf67b2aa147e10d3adafc0fe8d53c39d0ffae2a5b04`。旧地图和用户已有的工程存档保留。
