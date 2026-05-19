# 📋 CHANGELOG - 开发记录

> 类似 git commit 的功能记录，每个功能测试通过后记录一条
> 格式：日期 | 功能 | 状态 | 说明

---

## 2026-05-19

### ✅ Phase 0 - 项目基础
- **UE 5.3 项目编译通过** `2026-05-19 21:00`
  - 修复 `TrainingStats.cpp` INFINITY 常量溢出（MSVC 兼容）
  - 修复 `PrefireArena.h` Tick 声明未实现
  - DLL: `Binaries/Win64/UnrealEditor-ascent1.dll`

### ✅ Phase 0.5 - 代码质量修复
- **AscGameState BestReactionTimeMs 未初始化** `2026-05-19 21:30`
  - 垃圾值导致首次射击无法更新最佳反应时间
  - 改用 `std::numeric_limits<float>::infinity()` 初始化

### ✅ Phase 0.6 - FBX 地图导出
- **Blender → UE FBX 导出自动化** `2026-05-19 21:15`
  - 源: `H:\baiduyun\亚海悬城.blend` (467.7MB)
  - 输出: `H:\baiduyun\亚海悬城_UE.fbx` (75.3MB)
  - Scale=100 (米→厘米), FBX_SCALE_ALL
  - 验证通过: 文件头/尺寸/材质完整

### ✅ Phase 0.7 - 项目配置
- **默认地图指向 Ascent** `2026-05-19 21:20`
  - `DefaultEngine.ini`: `GameDefaultMap=/Game/Ascent`
- **Python 插件启用** `2026-05-19 21:25`
  - `.uproject`: 添加 PythonScriptPlugin + EditorScriptingUtilities

### ✅ Phase 0.8 - SpotEditor 依赖
- **npm install 完成** `2026-05-19 21:10`
  - 66 packages, 2 moderate vulnerabilities (不影响)

---

## 待完成 (TODO)

### ⬜ Phase 1 - UE 编辑器资产创建
- [ ] Enhanced Input 资产 (IA_Move, IA_Look, IA_Walk, IA_CrouchToggle, IA_Jump, IA_Fire)
- [ ] Input Mapping Context (IMC_Default)
- [ ] Blueprint 子类 (BP_AscGameMode, BP_Crosshair, BP_StatsPanel, BP_EnemyTarget)
- [ ] 设置默认 GameMode

### ⬜ Phase 2 - 地图导入
- [ ] FBX 导入 UE (ue_import_fbx.py)
- [ ] 验证地图尺寸和材质

### ⬜ Phase 3 - 关卡搭建
- [ ] 在 Ascent.umap 放置 PrefireArena
- [ ] 放置 TrainingManager
- [ ] 放置 EnemySpawner
- [ ] 设置 Enhanced Input 到 BP_AscGameMode

### ⬜ Phase 4 - UI 设计
- [ ] BP_Crosshair 准星 Widget 设计
- [ ] BP_StatsPanel 统计面板设计

### ⬜ Phase 5 - 端到端测试
- [ ] PIE 测试移动/射击
- [ ] 测试 Prefire 模式
- [ ] 测试 SpotEditor 功能

### ⬜ Phase 6 - SpotEditor 启动
- [ ] `npm run dev` 启动 SpotEditor
- [ ] 测试点位编辑功能
