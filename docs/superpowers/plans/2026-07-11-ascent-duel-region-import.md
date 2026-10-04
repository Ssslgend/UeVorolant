# Ascent Duel Region Import Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Export and import six lightweight Ascent duel-practice regions from the full Blender map into the UE 5.3 project: A site, B site, Mid, A Main, B Main, and Market/Door.

**Architecture:** Use Blender in headless mode to inspect the `.blend` scene, locate relevant objects by name and spatial grouping, export each region as an independent FBX, then use UE Python import automation to bring the FBX files into `/Game/AscentDuelRegions`. Keep source FBX files outside generated `.uasset` internals so failed imports can be repeated without touching existing map assets.

**Tech Stack:** Blender CLI (`blender.exe -b`), Python scripts under `Tools/`, Unreal Editor Python, UE 5.3 static mesh FBX import.

---

### Task 1: Scene Inventory

**Files:**
- Create: `Tools/blender_scene_inventory.py`
- Output: `Saved/AscentDuelExport/scene_inventory.json`

- [ ] **Step 1: Create a Blender inventory script**

The script opens inside Blender, walks all mesh objects, records names, collections, material slots, bounding boxes, and dimensions, and writes JSON to `Saved/AscentDuelExport/scene_inventory.json`.

- [ ] **Step 2: Run Blender inventory**

Run:
```powershell
& 'F:\Program Files\blender\blender.exe' -b 'H:\baiduyun\亚海悬城reclear.blend' -P Tools\blender_scene_inventory.py -- Saved\AscentDuelExport\scene_inventory.json
```

Expected: Blender exits with code 0 and the JSON contains thousands of mesh entries.

### Task 2: Region Selection

**Files:**
- Create: `Tools/ascent_region_config.json`
- Create: `Tools/blender_export_regions.py`

- [ ] **Step 1: Define six region names**

Use stable output names:
```json
[
  "A_Site_Duel",
  "B_Site_Duel",
  "Mid_Duel",
  "A_Main_Entry",
  "B_Main_Entry",
  "Market_Door_Duel"
]
```

- [ ] **Step 2: Build region filters from object metadata**

Use object names and collection names first. If naming is insufficient, use bounding-box centers after inspecting the inventory output. Each region export must include nearby floor, walls, cover, and sightline blockers, but exclude far vista/background meshes and decorative clutter that does not affect duel practice.

### Task 3: FBX Export

**Files:**
- Output: `Saved/AscentDuelExport/FBX/*.fbx`
- Output: `Saved/AscentDuelExport/export_report.json`

- [ ] **Step 1: Export six selected FBX files**

Run:
```powershell
& 'F:\Program Files\blender\blender.exe' -b 'H:\baiduyun\亚海悬城reclear.blend' -P Tools\blender_export_regions.py -- Tools\ascent_region_config.json Saved\AscentDuelExport\FBX Saved\AscentDuelExport\export_report.json
```

Expected: six FBX files are created and each is much smaller than the full-map FBX.

- [ ] **Step 2: Verify FBX files**

Check file existence and size. Any missing or tiny file means the region filter is wrong and must be corrected before UE import.

### Task 4: UE Import

**Files:**
- Create: `Tools/ue_import_duel_regions.py`
- UE destination: `/Game/AscentDuelRegions`

- [ ] **Step 1: Import region FBX files into UE**

Run the UE Python script in the editor or via UE commandlet with project `ascent1.uproject`. Import each FBX as a static mesh under `/Game/AscentDuelRegions/<RegionName>`.

- [ ] **Step 2: Save imported assets**

The script must save imported packages and log the imported asset paths.

### Task 5: Verification

**Files:**
- Output: `Saved/AscentDuelExport/ue_import_report.json`

- [ ] **Step 1: Verify six imported region assets exist**

The report must list six region assets with valid UE asset paths.

- [ ] **Step 2: Verify import health**

For each region, confirm the imported mesh has vertices/triangles, bounds are non-zero, and generated collision exists or the script logs that collision should be added manually with blocking volumes.

- [ ] **Step 3: Report completion evidence**

Completion requires current evidence for all six named regions: source FBX exists, UE asset exists, and import metadata is non-empty.
