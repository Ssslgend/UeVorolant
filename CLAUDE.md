# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Unreal Engine 5.3 project recreating Valorant's "Ascent" map. This is a **content-only project** — the C++ source is the default module scaffold; all real work happens in the UE Editor via Blueprints and level composition.

- **Engine**: UE 5.3
- **Module**: `ascent1` (Runtime, in `Source/ascent1/`)
- **Render features**: DX12, Lumen (software ray tracing), Virtual Shadow Maps, Nanite, Custom Depth
- **Input**: Enhanced Input system (`EnhancedPlayerInput` / `EnhancedInputComponent`)

## Build Targets

| Target | C# file |
|--------|---------|
| Game   | `Source/ascent1.Target.cs` |
| Editor | `Source/ascent1Editor.Target.cs` |

Build via UAT or the `.sln` file (`ascent1.sln`) in Visual Studio. No custom build scripts are present.

## Project Structure

```
Source/ascent1/           # C++ module (boilerplate only)
├── ascent1.Build.cs      # Module dependencies: Core, CoreUObject, Engine, InputCore
├── ascent1.h / .cpp      # Module impl
├── ascent1.Target.cs     # Game target
└── ascent1Editor.Target.cs

Config/
├── DefaultEngine.ini     # Renderer settings, Lumen/Nanite/VSM config, map defaults
├── DefaultInput.ini      # Enhanced Input defaults, axis/controller mappings
├── DefaultGame.ini       # Project ID
└── DefaultEditor.ini     # Editor preferences

Content/
├── Ascent.umap           # Main Ascent level
├── ascent1.umap          # Alternate/default level
├── *_WF_Asc_Base_V.uasset    # Wall Fragment static meshes (Base LOD)
├── *_WF_Asc_Vista_V.uasset   # Wall Fragment static meshes (Vista LOD)
├── __ExternalActors__/Ascent/ # OFPA external actor data for the Ascent level
├── valorant/ascent/          # Valorant asset subdirectory
├── Collections/              # Editor asset collections
└── Developers/41100/         # Developer-specific content
```

## Level Architecture

The Ascent map uses **One File Per Actor (OFPA)** — individual actors are stored as external `.uasset` files under `Content/__ExternalActors__/Ascent/`. The wall geometry is split into numbered wall fragments (`WF_Asc_Base_V` for base/building geometry, `WF_Asc_Vista_V` for vista/background geometry).

## Key Config Notes

- `DefaultEngine.ini` line 4: `GameDefaultMap=/Engine/Maps/Templates/OpenWorld` — consider pointing this to `Ascent.umap` for correct one-click launch
- `r.CustomDepth=1` is enabled — the map likely uses custom depth for gameplay mechanics (ability outlines, etc.)
- `r.DynamicGlobalIlluminationMethod=1` and `r.ReflectionMethod=1` use Lumen for GI and reflections
- `.vsconfig` and `.sln` files are present for Visual Studio integration
