---
feature: trainer-mvp-duel-loop
status: delivered
updated: 2026-10-05
branch: master
commits: bd07deb..bd07deb # working-tree delivery; implementation not yet committed
---

# 练枪软件 MVP 可玩闭环（Ascent 真实地图）

## Report

**What was built** — 在 `ascent1`（UE 5.3）落地 MVP-0+1+2 可玩闭环：`ATrainerDuel` 状态机（READY→PEEKING→DUEL→SUCCESS/FAIL→RESULT），Bot 站桩不动、玩家真实走位 peek；T0 为 Bot 眼→玩家头/躯干双通道 LOS 首帧且锁定不清零，`T0+delayMs`（默认 250ms）Bot 秒杀；窗口内爆头才 SUCCESS。点位 JSON schema v1 + 贴地/强制高度 + 坏点位告警跳过；3 个 Ascent 点位映射 `playable_report` 对枪线（A大/Mid/B大），yaw 按玩家→敌人重算。配置 `trainer_settings.json` 驱动速度/头球/延迟/超时；瞬停移动、关鼠标平滑、R 重置、RESULT 冻结武器。地图走用户导出的 `Saved/AscentPlayable/AscentPlayable_UE.fbx`（`Tools/ue_setup_trainer_mvp.py` 导入）。

**Verification** — 命令与结果：
- `Build.bat ascent1Editor Win64 Development` — PASS
- `Automation RunTests ascent1.Trainer`（4：StateTransitions / LosTimerGate / ReactionAndBotFire / DefaultsAndCm360）— PASS
- `Automation RunTests ascent1.Training`（9：Stats + Region/Lighting）— PASS
- 爆头遮挡修复经独立复审 PASS（忽略 Bot 的世界射线 + 全射线解析球体相交）

**Journey log** —
1. Bot 胶囊 r=25 会吞掉头球 r=4 的首击 Visibility；爆头必须用「忽略 Bot 的世界深度 + 全射线解析 ray-sphere」，不能信 ImpactPoint。
2. `AddSP` 不能绑 UObject/Actor 多播；用 `AddUObject`。
3. B 点 JSON yaw 曾按经验写错约 180°；统一 `atan2(Δy, Δx)` 重算。
4. 配置若只 Load 不 Apply 等于没做；速度/头球/冻结都要接到运行时。
5. Live Coding 占用时 UBT 无法编译，需关掉编辑器。

## [S1] Problem

需求文档《无畏契约练枪软件 v1.0》要求单机离线对枪练习：真实走位 peek、Bot 视线首帧起 delayMs 后秒杀、爆头才成功。当前仓库已有 UE 5.3 `ascent1` 脚手架（移动/武器/Bot/点位 JSON/统计）和已导出的可玩 Ascent 地图（`Saved/AscentPlayable/AscentPlayable_UE.fbx`，源自用户导出的 `亚海悬城reclear.blend`），但缺少：

1. 按 D1/D2 的**真实走位 peek + LOS 首帧计时**对枪状态机（现有 Bot 是主动 peek + 固定 FireDelay）；
2. 按 §5.1 的点位 JSON 与 Z 贴地加载；
3. 把用户导出的可玩地图接进训练关卡，并落地 §10 三个 Ascent 点位；
4. MVP-0 手感：瞬停移动、无鼠标平滑、头部球判定、R 重置。

本 feature 交付 **MVP-0 + MVP-1 + MVP-2** 可玩闭环，不包含 React 点位编辑器、灵敏度/准星完整设置面板、移动精度惩罚（F8）、后坐力。

## [S2] Design

### 2.1 宿主与地图（用户决策）

| 项 | 决策 |
| --- | --- |
| 工程 | 继续 `ascent1`（UE 5.3 C++），不新建 5.7 |
| 引擎 | `H:\Game\EpicGame\UE_5.3\UE_5.3` |
| 地图 | **直接使用用户导出的地图**：`Saved/AscentPlayable/AscentPlayable_UE.fbx`（Scale=100，米→厘米）；源 blend `H:\baiduyun\亚海悬城reclear.blend` 不再改几何 |
| 工作区 | master（本环境禁止 `git worktree add`，沿用 `ascent-playable-map` 约定） |
| 范围 | MVP-0+1+2 |

坐标约定：AscentPlayable 报告中 stand/cover 为 **米**；UE 世界为 **厘米**。加载时 `UE = Blender * 100`。点位 JSON 存 UE 厘米。

### 2.2 对枪状态机（§1.3 / D1 / D2）

```
READY ──(玩家开始移动/开火)──► PEEKING
PEEKING ──(LOS 首帧成立)──► DUEL（T0 = LOS 首帧；开始 delayMs）
DUEL ──(玩家爆头命中)──► SUCCESS ──► RESULT ──(R / 1.5s)──► READY
DUEL ──(T0+delayMs 到，Bot 开火)──► FAIL ──► RESULT ──► READY
READY ──(30s 无输入)──► FAIL（挂机）
```

关键契约：

- **Bot 不主动 peek**。Bot 站敌人点、朝向可配；玩家真实走位走出掩体。
- **T0 = Bot 眼睛首次 LOS 看见玩家**。每帧（或 120Hz）从 Bot 眼位置两条 LineTrace：→ 玩家头部球心、→ 躯干上部胶囊中心；**任一无遮挡的首帧**记 T0。
- `delayMs` 默认 250（150–500 可配）；到点 Bot **瞬时秒杀**（忽略玩家是否已开出第二枪之外的判定顺序：玩家爆头优先，同一帧玩家未爆头则 FAIL）。
- LOS 计时**不因瞬间遮挡重置**。
- RESULT 屏冻结玩家武器与 Bot。
- 前摇动画默认关（配置项 `bot.windupMs=0`）。

### 2.3 移动手感（F4 / MVP-0）

- 无惯性：加速度/减速度极大，松键即停（counter-strafe 的物理前提）。
- 速度档位进配置：跑 540 cm/s，走/蹲倍率可配（默认 walk=0.75×、crouch=0.63×）。
- 固定跳跃高度；关鼠标平滑、视角平滑；Raw Input。
- F8 移动扩散**本轮不做**，但碰撞与速度结构预留 `spread` 接口。

### 2.4 判定（F6）

- 头部：球体，半径默认 4 cm（配置 `hit.headRadiusCm`），挂角色头骨高度（站姿眼高 160 cm，头心 ≈160–170 可配）。
- 爆头 = SUCCESS；躯干/腿命中只记统计不算成功；未命中/超时 = FAIL。
- Bot 开火 = 对玩家瞬时判定 FAIL（不模拟弹道）。

### 2.5 点位 JSON（§5.1 子集）

兼容需求 schema 的 MVP 字段：

```json
{
  "version": 1,
  "generator": "manual/1.0",
  "map": {
    "id": "ascent",
    "displayName": "亚海悬城 Ascent",
    "calibration": { "anchors": [ {"uv":[0,0],"world":[0,0]} ] }
  },
  "points": [
    {
      "id": "ascent_a_main_to_site_01",
      "name": "A大→A包点 头线",
      "type": "prefire",
      "player": { "pos": [x,y,z], "yaw": 118.0 },
      "enemy":  { "pos": [x,y,z], "yaw": -62.0, "stance": "stand" },
      "bot": { "delayMs": 250, "fireMode": "instant_kill", "losTrigger": true },
      "difficulty": 1
    }
  ],
  "settings": { "timeoutMs": 30000, "resetDelayMs": 1500 }
}
```

- 加载以 `pos` 为准；`z=0` 时射线贴地（从 z+500 向下）；非 0 为强制偏移。
- 校验失败（射线未命中）→ 日志 + UI 警告并跳过该点，不崩。
- 文件默认 `Content/Data/trainer_spots_v1.json`。

### 2.6 首批 3 点位（§10 ↔ playable_report 对枪线）

Blender 米坐标来自 `playable_report.json`，写入 JSON 前 ×100 成 UE cm。

| 需求 ID | 训练内容 | 映射对枪线 | player (m) | enemy (m) |
| --- | --- | --- | --- | --- |
| `ascent_a_main_to_site_01` | A大宽 peek | `DL_A_Main_to_Default` | (-2.0, -33.6, 67.81) | (17.1, -23.5, 68.84) |
| `ascent_mid_to_market_01` | 中路第一枪 | `DL_Mid_Tiles_Pizza` | (-38.0, -40.0, 67.69) | (-38.0, -20.0, 67.57) |
| `ascent_b_main_to_boat_01` | B大 prefire | `DL_BMain_to_Default` | (-91.5, -22.3, 63.88) | (-110.0, 1.9, 65.67) |

Bot 站在 cover 点后/上方 headglitch 位，眼高 160 cm；玩家站在 stand 点。yaw = 玩家→敌人水平角（度）。

### 2.7 模块职责（复用 + 收敛）

| 模块 | 本轮改动 |
| --- | --- |
| `TrainerMovement` / `AscCharacter` | 瞬停、速度档配置化、关平滑、R 重置钩子 |
| `AscWeaponComponent` | 保留射击；爆头只认头部球；记录落点/离头距离 |
| `EnemyTarget` | **去主动 peek**；改为 LOS 探测 + delayMs 秒杀 + 静止站桩 |
| `TrainerDuel`（新） | 状态机 §2.2、超时、RESULT 窗口 |
| `SpotLoader` / `SpotData` | 对齐 §5.1 schema、贴地、yaw |
| `TrainingStats` | 反应时间 = 玩家开枪时刻 − T0；失败记「离头距离」 |
| `TrainingSettings`（新） | ini/JSON 读取 F18 参数（延迟、头球半径、速度、超时） |
| 关卡 `TrainingHub` | 导入 AscentPlayable 静态网格 + 灯光 + 3 点位 PlayerStart/Bot |

### 2.8 配置（F18）

`Config/TrainerSettings.ini`（或 `Content/Data/trainer_settings.json`）：

- `movement.runSpeed=540`、`walkMultiplier`、`crouchMultiplier`
- `hit.headRadiusCm=4`、`hit.standEyeHeightCm=160`
- `bot.delayMs=250`、`bot.windupMs=0`
- `duel.timeoutMs=30000`、`duel.resetDelayMs=1500`
- `input.disableSmoothing=true`

运行时可读；改文件后重开一局生效（不强制热更）。

### 2.9 验收（MVP-0+1+2）

1. **编译**：`UnrealEditor-ascent1` / `ascent1Editor` 目标构建成功。
2. **自动化**：`TrainingStats` 与状态机单元测试通过（UE Automation）。
3. **地图**：TrainingHub 引用 AscentPlayable 网格；三个点位贴地无告警。
4. **循环**：同一 JSON 点位完成 ≥3 次 READY→…→READY，不卡死、不误判（手测脚本/日志可证）。
5. **计时**：人为遮挡再露出，T0 不重置；delayMs 到点 FAIL；窗口内爆头 SUCCESS。
6. **输入**：WASD 瞬停、无平滑；R 重置当前点位。

## [S3] Out of Scope

- React 点位编辑器、minimap 标点 UI（V1）
- 灵敏度/cm360 设置面板、准星自定义 UI（V1；仅保留 ini 数值）
- F8 移动扩散/急停判定、F15 难度轮换、后坐力
- 多地图、联机、读游戏进程
- 材质/光照美化、Nanite/LOD
- 把 471MB 原 blend 入库

## Tasks

- [x] T1: 对齐点位 JSON schema + 贴地加载 — 写 `trainer_spots_v1.json` 3 点位，坏点位告警跳过 (covers: S2.5; S2.6)
- [x] T2: TrainingSettings 配置读取 — 延迟/头球/速度/超时可配 (covers: S2.8; S2.3)
- [x] T3: 移动瞬停 + 输入平滑关闭 + R 重置 — 手感契约可观察 (covers: S2.3; S2.9.6)
- [x] T4: 头部球判定 + 落点/离头距离 — 爆头才 SUCCESS (covers: S2.4)
- [x] T5: TrainerDuel 状态机 + LOS 双通道 T0 + delayMs 秒杀 — 状态迁移与计时可单测 (covers: S2.2; S2.9.5; depends: T3,T4)
- [x] T6: TrainingHub 接入 AscentPlayable + 3 点位布置 — 贴地无告警 (covers: S2.1; S2.6; S2.9.3; depends: T1)
- [x] T7: 统计落盘 + 验收跑通 — 循环≥3 次与自动化测试 (covers: S2.9.1; S2.9.2; S2.9.4; depends: T5,T6)
