---
feature: ascent-playable-map
status: delivered
updated: 2026-10-05
branch: master
commits: fec6688..3dfd147
---

# Ascent 可玩练枪图（筛选保留 + 体块补全）

## Report

**What was built** — 从原 `亚海悬城reclear.blend`（6969 物体 / 240 万面）筛选出可玩几何（地板/墙/掩体/门/楼梯），丢弃 Vista、植被、VFX、小摆件等装饰，并体块补全：外圈封闭墙、墙厚 0.4m、楼梯（步高 ≤0.20m）、24 个 headglitch 箱、18 条对枪线（站位+掩体成对）。导出 `AscentPlayable.blend`、`AscentPlayable_UE.fbx`（Scale=100）、`playable_report.json` 与验证渲染图。

**Verification** — `blender -b 亚海悬城reclear.blend -P Tools/build_playable_ascent.py`：
- `pass_poly_budget=true`，315954 面 ≤ 40 万
- `pass_duel_lines=true`，18 条对枪线（stand+cover 成对）
- `pass_connectivity=true`，BFS 自 Atk 可达 A/B/Mid/Market/Def（11 节点全连通）
- `pass_geometry=true`，实测 Seal 220×2×40、墙厚 0.4m、headglitch 高 1.15m

**Journey log** —
1. 原模型是视觉美术场景，不可直接当练枪碰撞图。
2. `bmesh.create_cube(size=1)+scale(size/2)` 会得到半尺寸几何；已改为 `size=2` 并加实测校验。
3. 连通检测不能用自放的 `PB_Corr_*` 自证，改为排除补块后的地板图 BFS。
4. 「对枪点」必须是站位↔掩体成对的线，不能只数箱子。
5. **地面不是一块平板**：原图 14+ 级 topZ；统一 slab 会毁掉高低差与对枪线。已改为保留原地板 + 局部填缝（Uiana/ValorantPorting 同思路：保 transform、不抹平标高）。
6. 环境禁止 `git worktree add`，规格与实现在 master 工作区完成。

## [S1] Problem

练枪软件需要可走、可挡、可对枪的地图。现有 `H:\baiduyun\亚海悬城reclear.blend`（约 240 万面 / 6969 物体）是完整 Ascent 视觉场景：布局与标高可信，但含大量装饰，且缺规范碰撞、墙厚封闭、楼梯连通与对枪用 headglitch 体。

## [S2] Design

### 2.1 路径

**筛选保留 + 体块补全**：保留可玩几何 → 删装饰 → 补封闭墙厚 / 楼梯 / 对枪 headglitch 箱。

### 2.2 筛选规则

保留：`*Floor*/*Ground*/*Platform*/*Wall*/*Shell_*/*Crate*/*Stair*/*Door*` 等 + 体积过滤。  
丢弃：Vista / 植被 / VFX / 摆件 / 贴花 / 天线等。  
区域：play AABB `(-155,-130,40)-(40,70,120)`（覆盖六区+出生点+通道）。

### 2.3 可玩性补全

1. **地面标高必须多层**（参考 Uiana / ValorantPorting：保留原始 actor transform，禁止统一平面）。  
   - 原图可玩区 topZ 约 **61.2–72.9**（报告 `floor_levels`）。  
   - `PB_FL_*` 仅作缝隙填补，高度取该点实测地板顶面。  
   - 禁止全图 `Playfield_Slab` 一类的大平板。
2. 墙厚 ≥0.3m（实现 0.4m），外圈 Seal 封闭。  
3. 楼梯步高 ≈0.2m，按**实测高差**生成（如 MidN 66.42→Heaven 72.90）。  
4. Navmesh 走廊：Atk→A/B/Mid→Market→Def 图连通（BFS，排除自建补块）。  
5. ≥12 条对枪线 = 站位 + headglitch 箱（箱高 1.1–1.3m；眼高 1.60 / 蹲 1.10）。

### 2.4 输出

`Saved/AscentPlayable/AscentPlayable.blend`  
`Saved/AscentPlayable/AscentPlayable_UE.fbx`  
`Saved/AscentPlayable/playable_report.json`  
`Tools/build_playable_ascent.py`

### 2.5 验收数据

面数 ≤40 万；墙厚/掩体高度实测；连通=true；≥12 对枪线；爆头高度写入报告。

### 2.6 工作区约束

环境禁止 `git worktree add`。在 master 工作区交付。

## [S3] Out of Scope

材质/光照美化、Nanite/LOD、游戏模式逻辑、远景美术、原 471MB blend 入库、UE 内 RecastNavMesh 实机烘焙（需编辑器）。

## Tasks

- [x] T1: 实现筛选脚本 — 从原 blend 导出可玩子集并统计 (covers: S2.2)
- [x] T2: 体块补全墙厚与外圈封闭 — Seal + 0.4m 墙，实测通过 (covers: S2.3.1)
- [x] T3: 楼梯/斜坡与连通走廊 — BFS 连通 true (covers: S2.3.2; S2.3.3)
- [x] T4: 对枪点位 + headglitch 箱 — 18 条对枪线成对 (covers: S2.3.4)
- [x] T5: 导出 FBX + playable_report.json + 渲染验证图 (covers: S2.4; S2.5; depends: T1,T2,T3,T4)
