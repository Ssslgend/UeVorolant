# Environment Material Rebuild Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace broken imported environment materials in `/Game/AscentDuelRegions` with a small, original, warm old-city training-oriented material palette and verify every static-mesh material slot is classified or reported for review.

**Architecture:** A pure-Python rule module owns normalization and keyword classification so it can be unit-tested outside Unreal. UE editor scripts import that module to export a unique slot audit CSV/JSON, create the environment material library, apply only known classifications to static-mesh assets, and write a post-apply QA report. The palette uses warm worn brick, weathered timber, dusty ground, muted foliage, and dark metal, while keeping value contrast low enough for clear target recognition. Unclassified, translucent, decal, and emissive slots remain untouched and are explicitly reported.

**Tech Stack:** Python 3 standard library, Unreal Engine 5.3 Python API, Unreal Material Editor assets, CSV/JSON reports.

---

### Task 1: Testable material-family rules

**Files:**
- Create: `Tools/environment_material_rules.py`
- Create: `Tools/tests/test_environment_material_rules.py`

- [ ] **Step 1: Write the failing tests**

```python
from environment_material_rules import classify_slot_name

def test_classifies_stone_keywords_case_insensitively():
    assert classify_slot_name("StoneBrickWall_M0_MI") == "StoneWall"

def test_classifies_stylistic_variants():
    assert classify_slot_name("AScaffold_0_M0_Kit") == "Metal"

def test_leaves_unmatched_names_for_manual_review():
    assert classify_slot_name("KingdomTech_1_M0_Teleporter") is None
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python -m unittest Tools.tests.test_environment_material_rules -v`

Expected: import failure because `environment_material_rules` does not exist.

- [ ] **Step 3: Implement the minimal classifier**

```python
FAMILY_RULES = (("StoneWall", ("stone", "brick", "rock", "cliff", "marble")), ...)

def classify_slot_name(name):
    lowered = (name or "").lower()
    for family, keywords in FAMILY_RULES:
        if any(keyword in lowered for keyword in keywords):
            return family
    return None
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python -m unittest Tools.tests.test_environment_material_rules -v`

Expected: all classifier tests pass.

### Task 2: Material-slot audit

**Files:**
- Create: `Tools/ue_audit_environment_material_slots.py`
- Modify: `Tools/environment_material_rules.py`

- [ ] **Step 1: Write the failing audit expectation**

Add a test asserting `summarize_slot_names(["Brick_A", "brick_a", "Wood_A"])` returns normalized counts `{"brick_a": 2, "wood_a": 1}`.

- [ ] **Step 2: Run the test and observe failure**

Run: `python -m unittest Tools.tests.test_environment_material_rules -v`

Expected: failure because `summarize_slot_names` does not exist.

- [ ] **Step 3: Implement audit output**

The UE script must scan `/Game/AscentDuelRegions`, load only `StaticMesh` assets, record `material_slot_name`, `imported_material_slot_name`, current material path, mesh count, slot occurrence count, automatic family, and action (`replace`, `manual_review`, or `preserve_special`). It writes `Saved/AscentDuelExport/material_slot_audit.csv` and `material_slot_audit.json` sorted by descending occurrence count.

- [ ] **Step 4: Verify the audit**

Run `UnrealEditor-Cmd.exe <project> -run=pythonscript -script=<audit script> -unattended -nop4 -nosplash`.

Expected: both reports exist; the JSON reports one or more static meshes and the CSV has the required headers.

### Task 3: Original material library and family instances

**Files:**
- Create: `Tools/ue_build_environment_material_library.py`
- Create: `Content/Mat/M_Environment_Master.uasset`
- Create: `Content/Mat/MI_StoneWall.uasset`
- Create: `Content/Mat/MI_Plaster.uasset`
- Create: `Content/Mat/MI_Concrete.uasset`
- Create: `Content/Mat/MI_Wood.uasset`
- Create: `Content/Mat/MI_Metal.uasset`
- Create: `Content/Mat/MI_Rubble.uasset`
- Create: `Content/Mat/MI_Ground.uasset`
- Create: `Content/Mat/MI_Foliage.uasset`
- Create: `Content/Mat/MI_Sign.uasset`

- [ ] **Step 1: Create a failing asset-manifest expectation**

Extend the pure-Python tests with `expected_material_instance_names()` and assert it returns the nine required instance names exactly.

- [ ] **Step 2: Run the test and observe failure**

Run: `python -m unittest Tools.tests.test_environment_material_rules -v`

Expected: failure because the manifest function does not exist.

- [ ] **Step 3: Implement the material build script**

Create `/Game/Mat/M_Environment_Master` with parameters `BaseColorTexture`, `NormalTexture`, `ORMTexture`, `UVTiling`, `Tint`, `RoughnessMultiplier`, and static switch `UseWorldAlignedProjection`. Create the nine instances. The foliage instance must be two-sided, masked, and use subsurface color; the sign instance must use a decal domain and expose emissive intensity. Use original, project-owned texture assets rather than game-extracted texture filenames.

- [ ] **Step 4: Verify assets in UE**

Run the builder then an editor Python query that checks the master plus every expected instance exists and has the correct parent. Expected: ten assets exist and all instances point at `/Game/Mat/M_Environment_Master`.

### Task 4: Conservative mesh-material application

**Files:**
- Create: `Tools/ue_apply_environment_material_families.py`
- Modify: `Tools/ue_audit_environment_material_slots.py`

- [ ] **Step 1: Write the failing decision test**

Add tests that classify `Brick_M0_ASiteBasicBrickOld_MI_V` as `StoneWall`, `Grass_M0_Cards_MI_NV` as `Foliage`, and `Glass_M0B_MI_NV` as `None`.

- [ ] **Step 2: Run the test and observe failure**

Run: `python -m unittest Tools.tests.test_environment_material_rules -v`

Expected: the glass-preservation test fails until the special-name exclusion list is added.

- [ ] **Step 3: Implement application**

Use `StaticMesh.set_material(slot_index, material_instance)` for slots with a known non-special family. Do not replace names containing `glass`, `decal`, `emissive`, `flame`, `water`, `cloth`, `light`, `teleporter`, or `screen`; write those to `preserved_special`. Support `--dry-run` and write `Saved/AscentDuelExport/material_apply_report.json` before saving any mesh.

- [ ] **Step 4: Verify dry-run and apply behavior**

Run the script with `--dry-run`, confirm `saved_mesh_count` is zero, then run it normally and confirm `replacement_count > 0`, `unclassified` is reported, and each replacement uses a `/Game/Mat/MI_*` asset.

### Task 5: Post-apply QA

**Files:**
- Create: `Tools/ue_verify_environment_material_rebuild.py`
- Modify: `Tools/ue_verify_duel_regions.py`

- [ ] **Step 1: Write the failing QA-report test**

Add a pure-Python test that `qa_is_complete({"unclassified_replaceable_count": 0, "default_material_slot_count": 0})` is true and becomes false when either value is nonzero.

- [ ] **Step 2: Run the test and observe failure**

Run: `python -m unittest Tools.tests.test_environment_material_rules -v`

Expected: failure because `qa_is_complete` does not exist.

- [ ] **Step 3: Implement QA script**

Scan all static meshes again. Report remaining default materials, missing material interfaces, unresolved replaceable slots, preserved special slots, material-instance usage counts, and total texture references. Save `Saved/AscentDuelExport/environment_material_qa.json` and fail the commandlet only for missing material interfaces or default material slots; unclassified slots remain a review queue.

- [ ] **Step 4: Run all verification**

Run the pure-Python test suite, UE audit, UE material builder, dry-run apply, actual apply, and post-apply QA. Expected: every command completes; all non-special classified slots use the intended material family; remaining review items are listed with names and counts.
