# Training Environment Closure And Lighting Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make all six duel-training regions visually enclosed at their crop boundaries, consistently bright, grounded, and free of missing/default material references while preserving open courtyards with a visible sky.

**Architecture:** Deliver the repair in two independently testable tracks. Track A adds a reusable UE runtime boundary shell, visible floor, sky atmosphere, controlled fog, and material fallbacks so the training software becomes usable without another expensive Blender export. Track B fixes Blender region selection to retain ground and neighboring context, exports to a staging asset root, and replaces the current region meshes only after geometry, normal, collision, material, and visual QA pass.

**Tech Stack:** Unreal Engine 5.3 C++, UE Automation Tests, UE Python, Blender 5.1 Python API, Python `unittest`, JSON/CSV QA reports.

---

## File Map

- Modify `Source/ascent1/Training/TrainingHubBuilder.h`: expose deterministic shell, floor, and environment settings for tests.
- Modify `Source/ascent1/Training/TrainingHubBuilder.cpp`: spawn visible floor, four crop-boundary walls, SkyAtmosphere, Skylight, and light fog.
- Modify `Source/ascent1/Training/TrainingStatsTests.cpp`: test shell dimensions, player containment, cubemap loading, and playable lighting bounds.
- Create `Tools/environment_material_rules.py`: promote the tested material classifier from the isolated material worktree into the project.
- Create `Tools/ue_audit_environment_material_slots.py`: produce the remaining material review queue.
- Create `Tools/ue_apply_environment_materials.py`: apply only approved family mappings and write a dry-run/apply report.
- Create `Tools/ue_verify_environment_material_replacement.py`: fail on missing/default/wrong family references.
- Create `Tools/region_export_rules.py`: pure-Python AABB and ground-preservation rules independent of Blender.
- Create `Tools/tests/test_region_export_rules.py`: test crop margins and low-center ground inclusion.
- Modify `Tools/ascent_region_config.json`: add context margins and explicit ground-preservation patterns.
- Modify `Tools/blender_export_regions.py`: use the tested selection rules and write exclusion reasons into the export report.
- Modify `Tools/ue_import_duel_regions.py`: support a staging destination root.
- Create `Tools/ue_compare_duel_region_rebuild.py`: compare current and staged mesh bounds, materials, and collision readiness.
- Create `Tools/analyze_training_screenshots.py`: calculate near-black pixel ratio for six gameplay screenshots.

---

## Track A: Make The Existing Training Build Usable

### Task 1: Deterministic boundary-shell geometry

**Files:**
- Modify: `Source/ascent1/Training/TrainingHubBuilder.h`
- Modify: `Source/ascent1/Training/TrainingHubBuilder.cpp`
- Test: `Source/ascent1/Training/TrainingStatsTests.cpp`

- [ ] **Step 1: Write failing shell tests**

Add tests for a region whose imported mesh bounds are `BoxExtent=(50,49,22)` at scale `100`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTrainingBoundaryShellTest,
    "ascent1.Training.Environment.BoundaryShellContainsRegion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingBoundaryShellTest::RunTest(const FString& Parameters)
{
    const FBoxSphereBounds Bounds(FVector::ZeroVector, FVector(50.0, 49.0, 22.0), 75.0);
    const TArray<FTransform> Walls = ATrainingHubBuilder::GetBoundaryWallTransforms(
        FVector::ZeroVector, Bounds, 100.0f);

    TestEqual(TEXT("Four lateral boundary walls are generated"), Walls.Num(), 4);
    TestTrue(TEXT("Player spawn remains inside shell"),
        FMath::Abs(ATrainingHubBuilder::GetTrainingPlayerOffset().Y) < 4900.0f + 600.0f);
    TestTrue(TEXT("Boundary wall is tall enough to hide crop edge"),
        ATrainingHubBuilder::GetBoundaryWallHalfHeight() >= 2500.0f);
    return true;
}
```

- [ ] **Step 2: Verify the test fails**

Run:

```powershell
& 'H:\Game\EpicGame\UE_5.3\UE_5.3\Engine\Build\BatchFiles\Build.bat' ascent1Editor Win64 Development 'F:\project\gitprojects\UE\valorant\ascent1\ascent1.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected: compile failure because `GetBoundaryWallTransforms` and `GetBoundaryWallHalfHeight` do not exist.

- [ ] **Step 3: Implement the shell math**

Add public static helpers and use these constants:

```cpp
static constexpr float BoundaryPadding = 600.0f;
static constexpr float BoundaryWallHalfThickness = 50.0f;
static constexpr float BoundaryWallHalfHeight = 2500.0f;
static constexpr float VisibleFloorDrop = 10.0f;
```

`GetBoundaryWallTransforms` must return north, south, east, and west cube transforms. Wall centers are derived from `MeshBounds.BoxExtent * MeshScale + BoundaryPadding`; no hand-entered per-region positions are allowed. Keep the top open so outdoor regions show the sky.

- [ ] **Step 4: Build and run the focused test**

Run the editor build, then:

```powershell
& 'H:\Game\EpicGame\UE_5.3\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'F:\project\gitprojects\UE\valorant\ascent1\ascent1.uproject' -ExecCmds='Automation RunTests ascent1.Training.Environment.BoundaryShellContainsRegion;Quit' -unattended -nop4 -nosplash -nullrhi -log
```

Expected: one test found and completed with `Result={成功}`.

- [ ] **Step 5: Commit**

```powershell
git add Source/ascent1/Training/TrainingHubBuilder.h Source/ascent1/Training/TrainingHubBuilder.cpp Source/ascent1/Training/TrainingStatsTests.cpp
git commit -m "feat: define training region boundary shell"
```

### Task 2: Visible floor and boundary-wall actors

**Files:**
- Modify: `Source/ascent1/Training/TrainingHubBuilder.h`
- Modify: `Source/ascent1/Training/TrainingHubBuilder.cpp`
- Test: `Source/ascent1/Training/TrainingStatsTests.cpp`

- [ ] **Step 1: Replace the old hidden-floor expectation with a failing visible-floor test**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTrainingFallbackFloorVisibilityTest,
    "ascent1.Training.Environment.FallbackFloorVisibleInGame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingFallbackFloorVisibilityTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Fallback floor fills holes at crop boundary"),
        ATrainingHubBuilder::ShouldShowFallbackFloorInGame());
    TestTrue(TEXT("Visible floor is dropped to avoid z-fighting"),
        ATrainingHubBuilder::GetVisibleFloorDrop() >= 5.0f);
    return true;
}
```

- [ ] **Step 2: Run and verify RED**

Expected: compile failure because the new helpers do not exist, or assertion failure while the current floor remains hidden.

- [ ] **Step 3: Spawn the visual shell**

Add `SpawnBoundaryShell(const FTrainingRegionDefinition&, const FBoxSphereBounds&)` and a shared `SpawnShellBox` helper. Use `/Engine/BasicShapes/Cube.Cube`; assign `/Game/Mat/MI_Ground` to the floor and `/Game/Mat/MI_Plaster` to the four walls. Set collision to `BlockAll`, place the floor top `10 cm` below gameplay floor, and call `SpawnBoundaryShell` immediately after the region mesh is spawned.

The floor remains a fallback surface: imported map floors render above it where present, while crop holes reveal warm ground rather than black void.

- [ ] **Step 4: Run environment and placement tests**

Run:

```powershell
& 'H:\Game\EpicGame\UE_5.3\UE_5.3\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'F:\project\gitprojects\UE\valorant\ascent1\ascent1.uproject' -ExecCmds='Automation RunTests ascent1.Training.Environment;Automation RunTests ascent1.Training.RegionPlacement;Quit' -unattended -nop4 -nosplash -nullrhi -log
```

Expected: all environment and region-placement tests pass.

- [ ] **Step 5: Commit**

```powershell
git add Source/ascent1/Training/TrainingHubBuilder.h Source/ascent1/Training/TrainingHubBuilder.cpp Source/ascent1/Training/TrainingStatsTests.cpp
git commit -m "feat: add visible training region shell"
```

### Task 3: Visible sky and controlled ambient light

**Files:**
- Modify: `Source/ascent1/Training/TrainingHubBuilder.h`
- Modify: `Source/ascent1/Training/TrainingHubBuilder.cpp`
- Test: `Source/ascent1/Training/TrainingStatsTests.cpp`

- [ ] **Step 1: Add failing environment-setting tests**

```cpp
TestTrue(TEXT("Fog stays subtle"), ATrainingHubBuilder::GetTrainingFogDensity() <= 0.005f);
TestTrue(TEXT("Fog separates black distance"), ATrainingHubBuilder::GetTrainingFogDensity() >= 0.001f);
TestTrue(TEXT("Sky atmosphere is enabled"), ATrainingHubBuilder::ShouldSpawnSkyAtmosphere());
```

- [ ] **Step 2: Run and verify RED**

Expected: compile failure because the three environment settings do not exist.

- [ ] **Step 3: Implement sky and fog**

In `SpawnLighting`, keep the verified Daylight Ambient Cubemap, then spawn exactly one `ASkyAtmosphere` and one `AExponentialHeightFog`. Use fog density `0.0025`, fog height falloff `0.2`, and a warm-neutral directional light color near `FLinearColor(1.0f, 0.92f, 0.82f)`. Keep the existing fixed exposure and bloom disabled so targets remain readable.

Required includes:

```cpp
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
```

- [ ] **Step 4: Build and run all lighting tests**

Expected: `UsesLoadableAmbientCubemap`, `UsesPlayableTrainingBrightness`, and the new sky/fog test all pass.

- [ ] **Step 5: Commit**

```powershell
git add Source/ascent1/Training/TrainingHubBuilder.h Source/ascent1/Training/TrainingHubBuilder.cpp Source/ascent1/Training/TrainingStatsTests.cpp
git commit -m "fix: add visible sky and ambient separation"
```

### Task 4: Resolve the remaining structural material queue

**Files:**
- Create: `Tools/environment_material_rules.py`
- Create: `Tools/tests/test_environment_material_rules.py`
- Create: `Tools/ue_audit_environment_material_slots.py`
- Create: `Tools/ue_apply_environment_materials.py`
- Create: `Tools/ue_verify_environment_material_replacement.py`
- Modify: `Content/Mat/*`

- [ ] **Step 1: Promote the already-tested scripts from the isolated worktree**

Copy the verified scripts from:

```text
C:\Users\41100\.config\superpowers\worktrees\ascent1\codex\environment-material-rebuild\Tools
```

Do not copy the diagnostic `ue_probe_world_aligned_material_function.py` into the main project.

- [ ] **Step 2: Run the current tests and audit before adding rules**

Run `python -m unittest Tools.tests.test_environment_material_rules -v`, then run the UE audit. Preserve the baseline counts in `Saved/AscentDuelExport/material_slot_audit.json`: `879` replaced occurrences, `200` special occurrences, and `498` manual-review occurrences.

- [ ] **Step 3: Add failing tests for approved structural prefixes**

Add this ordered override table after special-material detection and before the generic family rules:

```python
APPROVED_STRUCTURAL_OVERRIDES = (
    ("Wood", (r"^door_", r"^defarch_\d+_m0_door", r"^vino_.*winebarrel", r"shutters")),
    ("Metal", (
        r"^wallsupport_", r"^wallsconce(?!.*glow)", r"^wires_", r"^wallelectricbox_",
        r"^doorbellbox_", r"^techelement_", r"^tools_", r"^vent_", r"^firehydrant_",
        r"^windowframekit_(?!.*panes)", r"^defarch_.*grate",
    )),
    ("Plaster", (
        r"^colorpalette_", r"^abuilding_", r"^balconya_", r"^columnwall_",
        r"^bmainarchway_", r"^trim_",
    )),
    ("StoneWall", (r"^terracotta_",)),
)
```

Add tests for the exact audit names `Door_0_M0_Panel_MI_NV`, `Wires_0_MI_NV`, `TechElement_33_M0_RoofVentsA_MI_NV`, `ABuilding_0_M0_TrimKitA_MI_NV`, and `Terracotta_M1_MI_NV`. Also test that `WallSconce_0_Glow_MI_NV`, `WindowFrameKit_0_Panes_MI_NV`, all `Windows_*`, `SwitchRoom_0_Tarp_MI_NV`, `Rope_0_MI_NV`, `Plushie_*`, and `Fish_*` remain `preserve_special`.

- [ ] **Step 4: Apply only after dry-run review**

Run the apply script without `UE_ENVIRONMENT_MATERIAL_APPLY`; require `saved_mesh_count=0`. Review every newly mapped prefix in the JSON. Then run with `UE_ENVIRONMENT_MATERIAL_APPLY=1` and save all six meshes.

- [ ] **Step 5: Verify material acceptance criteria**

The verification commandlet must report:

```text
missing_material_interface_count = 0
default_material_slot_count = 0
wrong_family_reference_count = 0
manual_review_occurrences <= 100
```

The remaining review queue may contain only intentionally preserved specialty materials or genuinely ambiguous decorative props.

- [ ] **Step 6: Commit scripts and source textures**

```powershell
git add Tools/environment_material_rules.py Tools/tests Tools/ue_audit_environment_material_slots.py Tools/ue_apply_environment_materials.py Tools/ue_verify_environment_material_replacement.py Content/Mat/SourceTextures
git commit -m "fix: resolve structural environment materials"
```

---

## Track B: Improve The Blender Region Exports

### Task 5: Testable ground and context selection rules

**Files:**
- Create: `Tools/region_export_rules.py`
- Create: `Tools/tests/test_region_export_rules.py`
- Modify: `Tools/ascent_region_config.json`
- Modify: `Tools/blender_export_regions.py`

- [ ] **Step 1: Write failing pure-Python tests**

```python
def test_ground_bypasses_center_height_filter():
    decision = selection_decision(
        name="Ground_StoneFloor_A",
        collections={"Ascent_Gameplay"},
        bbox_min=(-10, -10, 0),
        bbox_max=(10, 10, 1),
        region_center=(0, 0, 70),
        region_extent=(42, 38, 35),
        context_margin=(8, 8, 4),
        min_center_z=40,
        ground_patterns=("ground", "floor", "road", "asphalt"),
    )
    assert decision.include
    assert decision.reason == "ground_override"

def test_context_margin_includes_neighboring_wall():
    assert expanded_extent((42, 38, 35), (8, 8, 4)) == (50, 46, 39)
```

- [ ] **Step 2: Run and verify RED**

Run: `python -m unittest Tools.tests.test_region_export_rules -v`

Expected: import failure because `region_export_rules.py` does not exist.

- [ ] **Step 3: Implement selection rules and configuration**

Add this shared configuration:

```json
"context_margin": [8.0, 8.0, 4.0],
"ground_name_patterns": ["ground", "floor", "road", "asphalt", "stonefloor", "bomb_site_outline"],
"ground_collections": ["Ascent_Gameplay", "Key A", "Key B"]
```

Keep `SkyDome`, `LightShaft`, and `Waterfall` excluded because UE now supplies the sky. Low-center meshes are included only when both the name/collection ground rule and expanded AABB intersection pass.

- [ ] **Step 4: Make Blender export report every decision class**

`blender_export_regions.py` must write per-region counts for `included_normal`, `included_ground_override`, `excluded_collection`, `excluded_name`, `excluded_height`, and `excluded_bounds`. It must continue copying objects into a temporary named collection and must not modify source objects or shared mesh data.

- [ ] **Step 5: Run tests and Blender inventory/export dry run**

Run pure tests, then Blender 5.1 headless with the source `.blend` and the updated config. Expected: all six regions contain at least one `included_ground_override`; negative-scale object count and zero-area face count remain zero.

- [ ] **Step 6: Commit**

```powershell
git add Tools/region_export_rules.py Tools/tests/test_region_export_rules.py Tools/ascent_region_config.json Tools/blender_export_regions.py
git commit -m "fix: preserve ground and context in region exports"
```

### Task 6: Import rebuilt meshes into staging

**Files:**
- Modify: `Tools/ue_import_duel_regions.py`
- Create: `Tools/ue_compare_duel_region_rebuild.py`

- [ ] **Step 1: Add a staging-root argument**

Read `UE_DUEL_REGION_DESTINATION` from the environment, defaulting to `/Game/AscentDuelRegions_Rebuild`. Refuse `/Game/AscentDuelRegions` unless `UE_ALLOW_REGION_OVERWRITE=1` is also set.

- [ ] **Step 2: Import all six rebuilt FBX files**

Import into `/Game/AscentDuelRegions_Rebuild/<Region>/<Region>`. Do not edit current runtime paths yet.

- [ ] **Step 3: Compare staged and current assets**

For each region, report old/new bounds, vertex count, section count, material-slot count, collision availability, and source-floor coverage. Fail if:

```text
new bounds extent is zero
new vertex count is zero
new material interface is missing
new floor coverage does not contain the configured player spawn
```

- [ ] **Step 4: Visual inspection in UE Editor**

Open each staged mesh in Static Mesh Editor. Inspect top, bottom, front, back, left, and right views. Confirm retained ground touches architecture, there are no inverted faces, and expanded neighboring objects do not float or obstruct the intended aim lane.

- [ ] **Step 5: Swap only validated assets**

Back up the six current `.uasset` files under `Saved/MaterialRebuildBackup/<timestamp>-pre-region-v2`, then replace runtime region assets. Run the material dry-run and apply pass on the rebuilt assets because FBX reimport recreates material slots.

### Task 7: End-to-end gameplay and image QA

**Files:**
- Create: `Tools/analyze_training_screenshots.py`
- Modify: `Source/ascent1/Training/TrainingStatsTests.cpp`

- [ ] **Step 1: Run all automated tests**

Run the editor build and `Automation RunTests ascent1.Training`. Expected: all tests pass, including grounding, player bounds, visible floor, boundary shell, cubemap, sky/fog, and statistics.

- [ ] **Step 2: Run UE asset QA**

Run material-library verification, material-replacement verification, and staged/current region comparison. Expected: zero errors and six valid runtime meshes.

- [ ] **Step 3: Capture six gameplay views**

Launch the game, visit regions with keys `1` through `6`, and save one `HighResShot 1` image per region while standing at the configured player spawn and facing the aim lane.

- [ ] **Step 4: Analyze darkness**

`analyze_training_screenshots.py` must use Pillow to report mean luminance and the percentage of pixels below luminance `12/255`, excluding the HUD border. Acceptance per screenshot:

```text
mean_luminance >= 45/255
near_black_pixel_ratio <= 25%
```

- [ ] **Step 5: Manual visual acceptance**

For all six screenshots confirm:

- no black void is visible at eye level;
- crop boundaries are hidden by warm neutral walls;
- imported map architecture remains the dominant visual layer;
- open top shows sky rather than a ceiling or black background;
- floor is visible and continuous without z-fighting;
- targets remain clearly separated from the background;
- glass, foliage, signs, and emissive surfaces do not show opaque rectangular cards.

- [ ] **Step 6: Runtime smoke test**

Run for ten minutes, switch through all six regions, shoot targets, and verify the pawn never falls below the floor and never exits through a crop boundary. Confirm the log contains `TrainingHubBuilder: spawned 6 training regions` and contains no missing mesh, missing material, or shader errors.

- [ ] **Step 7: Final commit**

```powershell
git add Source/ascent1/Training Tools docs/superpowers/plans/2026-07-18-training-environment-closure-lighting-fix.md
git commit -m "fix: complete enclosed and lit training environments"
```

---

## Completion Gate

The repair is complete only when all of the following are true:

- UE Track A is usable before Blender Track B is merged.
- All six regions have visible floor collision and four lateral crop-boundary walls.
- Outdoor openings show SkyAtmosphere; no eye-level black void remains.
- All classified structural slots reference `/Game/Mat/MI_*`; missing/default material counts are zero.
- Blender V2 exports preserve ground and add context without negative scales or zero-area faces.
- Rebuilt assets pass staging comparison before replacing runtime assets.
- The full `ascent1.Training` automation suite passes.
- Six screenshots satisfy the luminance thresholds and manual visual checklist.
- The ten-minute six-region runtime smoke test has no falls, boundary escapes, or asset/shader errors.
