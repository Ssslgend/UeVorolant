# Training Software MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the imported Ascent duel regions into a playable local UE training program with first-person movement, shooting, targets, scoring, and six selectable practice regions.

**Architecture:** Keep the MVP mostly C++ so the project remains reproducible from source, and use UE Python only for editor asset assembly. Existing classes (`AAscCharacter`, `UAscWeaponComponent`, `AEnemyTarget`, `ATrainingManager`) are retained and hardened instead of replaced.

**Tech Stack:** UE 5.3 C++, Enhanced Input, UMG-compatible C++ widgets, UE Python editor automation, UE Automation tests.

---

### Task 1: Verify Existing Build

- [ ] Build the current project with UE 5.3 command line.
- [ ] Record current compile failures before changing production code.

### Task 2: Training Stats TDD

- [ ] Add automation tests proving shot count, hit rate, headshot rate, average reaction time, best reaction time, and reset behavior.
- [ ] Fix `FTrainingSession` so duplicate reaction times are counted and summary values initialize deterministically.

### Task 3: Gameplay Wiring

- [ ] Make weapon shots record misses and hits through game state.
- [ ] Make enemy targets report hit region instead of silently destroying/resetting.
- [ ] Ensure the crosshair/stat widget hooks are optional so the game remains playable without Blueprint widget assets.

### Task 4: Input and Default Game

- [ ] Provide robust legacy fallback input bindings for WASD, mouse look, jump, crouch, walk, and fire.
- [ ] Set project defaults to the training game mode and training map.

### Task 5: Region Training Maps

- [ ] Generate or assemble a training hub/map with six imported region meshes.
- [ ] Add player starts, lights, simple bot spawn markers, and `ATrainingManager`.
- [ ] Keep each region independently addressable for future menu selection.

### Task 6: Verification

- [ ] Run UE automation tests.
- [ ] Build the editor target.
- [ ] Run UE commandlet validation for imported region assets and generated training setup.
