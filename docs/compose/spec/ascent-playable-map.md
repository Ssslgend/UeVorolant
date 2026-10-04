---
feature: ascent-playable-map
status: delivered
updated: 2026-10-05
branch: master
commits: fec6688..3dfd147
---

# Ascent 可玩练枪图（筛选保留 + 体块补全）

## Report

**What was built** — 从原 `亚海悬城reclear.blend` 筛选可玩几何并补全墙厚/楼梯/对枪线；再按「玩家点↔敌人点」裁成 **10 个独立小场景**（`Tools/build_playable_regions.py`）。每区：圆心=中点、半径=点距+8m（≥18m）、删 &lt;0.35m 小道具、薄墙 Solidify 0.15m 并烘焙、合并为 3 灰材质网格、按块生成 UCX 盒 + 台阶斜坡碰撞。导出 `Saved/AscentPlayableRegions/*.fbx` 与 `regions_report.json`。

**Verification** —
- 全图：`build_playable_ascent.py` → 31.6 万面 / 18 对枪线 / BFS 连通 / 多层地面 14 级
- 分区域：`regions_report.json` → `all_pass_objects/polys/materials/collision = true`；每区 visual=3，面 3k–184k，碰撞盒 11–1057（含 ramp）
- 修复后：Solidify 在 merge 前 `new_from_object` 烘焙；碰撞从 pre-merge 块生成（非区域大 AABB）

**Journey log** —
1. 原模型是视觉美术场景，不可直接当练枪碰撞图。
2. `create_cube(size=1)+scale(size/2)` 半尺寸陷阱；改 `size=2` + 实测。
3. 连通不能用自放补块自证；改为排除补块的 BFS。
4. **地面不能压成一块平板**（14 级 topZ）。
5. merge 用 `obj.data.copy()` 会丢掉 Solidify；必须先 `evaluated_get` 烘焙。碰撞必须在 merge **前**按块生成，否则只剩 3 个大盒子。

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

材质/光照美化、Nanite/LOD、游戏模式逻辑、远景美术、原 471MB blend 入库、UE 内 RecastNavMesh 实机烘焙与游戏内 FOV 叠图校准（需编辑器/客户端）。

## [S2b] 分区域裁剪验收（2026-10-05 增补）

以「玩家点↔敌人点」为区域，圆心=中点，半径=点距+8m peek。

| 指标 | 阈值 |
|------|------|
| 单区域物件数 | **&lt; 300** |
| 单区域面数 | **&lt; 200000** |
| 墙/掩体 | 薄片 Solidify **10–20cm**（防穿透） |
| 材质 | **3–4 种灰**（墙/地/掩体/强调） |
| 碰撞 | 独立简化盒（墙/地/掩体）；台阶用**斜坡碰撞** |
| 场景 | 每点位**独立** FBX/场景，不塞一张大图 |
| 标高 | 保留原多层地面 topZ |
| 眼高 | 站立 1.60m / 蹲 1.10m 随局部地面 |

实现：`Tools/build_playable_regions.py`（输入 AscentPlayable 或原 blend + 点位表）。

## Tasks

- [x] T1: 实现筛选脚本 — 从原 blend 导出可玩子集并统计 (covers: S2.2)
- [x] T2: 体块补全墙厚与外圈封闭 — Seal + 0.4m 墙，实测通过 (covers: S2.3.1)
- [x] T3: 楼梯/斜坡与连通走廊 — BFS 连通 true (covers: S2.3.2; S2.3.3)
- [x] T4: 对枪点位 + headglitch 箱 — 18 条对枪线成对 (covers: S2.3.4)
- [x] T5: 导出 FBX + playable_report.json + 渲染验证图 (covers: S2.4; S2.5; depends: T1,T2,T3,T4)
- [x] T6: 分区域裁剪导出 — 每点位独立 FBX，物件&lt;300、面&lt;20万 (covers: S2b)
- [x] T7: Solidify 薄墙 + 合并 + 4 灰材质 (covers: S2b)
- [x] T8: 独立碰撞盒 + 台阶斜坡碰撞 (covers: S2b; depends: T6)
- [x] T9: 分区域 audit 报告全部 PASS (covers: S2b; depends: T6,T7,T8)
