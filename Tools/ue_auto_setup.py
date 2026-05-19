"""
UE Editor Utility - Auto Setup Valorant Aim Trainer
====================================================
Creates Enhanced Input assets, Input Mapping Context, Blueprint subclasses,
and sets the default GameMode.

Prerequisites: Enable "Python Editor Script Plugin" in Edit -> Plugins
Usage: Run from UE5 Python console or Tools -> Execute Python Script
"""

import unreal


INPUT_PATH = "/Game/Input/Actions"
IMC_PATH = "/Game/Input/Mappings"
BP_PATH = "/Game/Blueprints"

ACTION_DEFS = [
    ("Move",         "axis2d"),
    ("Look",         "axis2d"),
    ("Walk",         "bool"),
    ("CrouchToggle", "bool"),
    ("Jump",         "bool"),
    ("Fire",         "bool"),
]

KEY_BINDINGS = [
    ("IA_Move",         "W"),
    ("IA_Move",         "S"),
    ("IA_Move",         "A"),
    ("IA_Move",         "D"),
    ("IA_Look",         "Mouse2D"),
    ("IA_Walk",         "LeftShift"),
    ("IA_CrouchToggle", "LeftControl"),
    ("IA_Jump",         "SpaceBar"),
    ("IA_Fire",         "LeftMouseButton"),
]

BLUEPRINT_DEFS = {
    "BP_AscGameMode":  "/Script/ascent1.AscGameMode",
    "BP_Crosshair":    "/Script/ascent1.CrosshairWidget",
    "BP_StatsPanel":   "/Script/ascent1.StatsPanelWidget",
    "BP_EnemyTarget":  "/Script/ascent1.EnemyTarget",
}

FALLBACK_CLASSES = {
    "BP_AscGameMode":  unreal.GameModeBase,
    "BP_Crosshair":    unreal.UserWidget,
    "BP_StatsPanel":   unreal.UserWidget,
    "BP_EnemyTarget":  unreal.Actor,
}

VALUE_TYPE_MAP = {
    "bool":     unreal.InputActionValueType.BOOLEAN,
    "axis1d":   unreal.InputActionValueType.AXIS1D,
    "axis2d":   unreal.InputActionValueType.AXIS2D,
    "axis3d":   unreal.InputActionValueType.AXIS3D,
}


def ensure_dirs():
    for p in [INPUT_PATH, IMC_PATH, BP_PATH]:
        if not unreal.EditorAssetLibrary.does_directory_exist(p):
            unreal.EditorAssetLibrary.make_directory(p)


def create_input_action(name, value_type_str):
    asset_path = f"{INPUT_PATH}/IA_{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log(f"  [SKIP] {asset_path} already exists")
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    factory = unreal.InputActionFactory()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    action = asset_tools.create_asset(f"IA_{name}", INPUT_PATH, None, factory)
    if not action:
        unreal.log_error(f"  [FAIL] Could not create {asset_path}")
        return None

    action.set_editor_property("value_type", VALUE_TYPE_MAP[value_type_str])
    unreal.EditorAssetLibrary.save_asset(asset_path)
    unreal.log(f"  [OK] Created {asset_path}")
    return action


def create_input_mapping_context():
    asset_path = f"{IMC_PATH}/IMC_Default"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log(f"  [SKIP] {asset_path} already exists")
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    factory = unreal.InputMappingContextFactory()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    imc = asset_tools.create_asset("IMC_Default", IMC_PATH, None, factory)
    if not imc:
        unreal.log_error(f"  [FAIL] Could not create {asset_path}")
        return None

    unreal.EditorAssetLibrary.save_asset(asset_path)
    unreal.log(f"  [OK] Created {asset_path}")
    return imc


def resolve_parent_class(bp_name):
    path = BLUEPRINT_DEFS.get(bp_name)
    if path:
        cls = unreal.load_class(None, path)
        if cls:
            return cls
        unreal.log_warning(f"  C++ class {path} not found, using fallback for {bp_name}")

    fallback = FALLBACK_CLASSES.get(bp_name)
    return fallback.static_class() if fallback else None


def create_blueprint(name, parent_class, directory=None):
    target_dir = directory if directory else BP_PATH
    asset_path = f"{target_dir}/{name}"

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log(f"  [SKIP] {asset_path} already exists")
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    bp = asset_tools.create_asset(name, target_dir, None, factory)
    if bp:
        unreal.EditorAssetLibrary.save_asset(asset_path)
        unreal.log(f"  [OK] Created {asset_path}")
        return bp

    unreal.log_error(f"  [FAIL] Could not create {asset_path}")
    return None


def build_mappings(imc, actions):
    existing = imc.get_editor_property("mappings")
    if existing and len(existing) > 0:
        unreal.log(f"  [SKIP] IMC_Default already has {len(existing)} mappings, skipping bindings")
        return

    mappings = unreal.Array(unreal.InputActionKeyMapping)

    for action_name, key_name in KEY_BINDINGS:
        action = actions.get(action_name)
        if not action:
            continue

        key = unreal.InputCoreTypes.get_key(key_name)
        if not key or not key.is_valid():
            unreal.log_warning(f"  Could not resolve key '{key_name}'")
            continue

        mapping = unreal.InputActionKeyMapping()
        mapping.set_editor_property("action", action)
        mapping.set_editor_property("key", key)
        mappings.append(mapping)

    imc.set_editor_property("mappings", mappings)
    unreal.EditorAssetLibrary.save_loaded_asset(imc)
    unreal.log(f"  [OK] Bound {len(mappings)} key-action pairs in IMC_Default")


def set_default_gamemode():
    bp_path = f"{BP_PATH}/BP_AscGameMode"

    if not unreal.EditorAssetLibrary.does_asset_exist(bp_path):
        unreal.log_error(f"  [FAIL] BP_AscGameMode not found at {bp_path}")
        return False

    bp_class = unreal.EditorAssetLibrary.load_blueprint_class(bp_path)
    if not bp_class:
        unreal.log_error(f"  [FAIL] Could not load Blueprint class from {bp_path}")
        return False

    try:
        game_maps = unreal.get_default_object(unreal.GameMapsSettings)
        game_maps.set_editor_property("global_default_game_mode", bp_class)
        game_maps.save_config()
        unreal.log(f"  [OK] Default GameMode set to {bp_path}")
        return True
    except Exception as exc:
        unreal.log_warning(f"  API method failed ({exc}), writing INI directly...")

    try:
        config_dir = unreal.Paths.project_config_dir()
        ini_path = config_dir + "DefaultEngine.ini"

        section = "[/Script/EngineSettings.GameMapsSettings]"
        entry = f"GlobalDefaultGameMode={bp_path}.BP_AscGameMode_C"

        if not __import__("os").path.exists(ini_path):
            unreal.log_error(f"  Config not found: {ini_path}")
            return False

        with open(ini_path, "r", encoding="utf-8") as f:
            lines = f.readlines()

        in_section = False
        found_key = False
        out_lines = []
        for line in lines:
            if line.strip() == section:
                in_section = True
                out_lines.append(line)
                continue
            if in_section and line.startswith("["):
                if not found_key:
                    out_lines.append(entry + "\n")
                in_section = False
            if in_section and line.startswith("GlobalDefaultGameMode="):
                out_lines.append(entry + "\n")
                found_key = True
                continue
            out_lines.append(line)

        if not found_key:
            if not any(section in l for l in out_lines):
                out_lines.append("\n" + section + "\n")
                out_lines.append(entry + "\n")
            elif in_section:
                out_lines.append(entry + "\n")

        with open(ini_path, "w", encoding="utf-8") as f:
            f.writelines(out_lines)

        unreal.log(f"  [OK] Wrote GlobalDefaultGameMode to DefaultEngine.ini")
        return True
    except Exception as exc:
        unreal.log_error(f"  [FAIL] Could not set default GameMode: {exc}")
        return False


def main():
    unreal.log("=" * 60)
    unreal.log("Valorant Aim Trainer - Auto Setup")
    unreal.log("=" * 60)

    ensure_dirs()

    # ---- Step 1: Create Input Actions ----
    unreal.log("\n[1/4] Creating Enhanced Input Actions...")
    actions = {}
    for name, vtype in ACTION_DEFS:
        actions[f"IA_{name}"] = create_input_action(name, vtype)

    # ---- Step 2: Create IMC and bind keys ----
    unreal.log("\n[2/4] Creating Input Mapping Context...")
    imc = create_input_mapping_context()
    if imc:
        build_mappings(imc, actions)

    # ---- Step 3: Create Blueprint subclasses ----
    unreal.log("\n[3/4] Creating Blueprint subclasses...")
    created = {}
    for bp_name in ["BP_AscGameMode", "BP_Crosshair", "BP_StatsPanel", "BP_EnemyTarget"]:
        parent = resolve_parent_class(bp_name)
        if parent:
            created[bp_name] = create_blueprint(bp_name, parent)

    # ---- Step 4: Set default GameMode ----
    unreal.log("\n[4/4] Configuring default GameMode...")
    set_default_gamemode()

    unreal.log("\n" + "=" * 60)
    unreal.log("SETUP COMPLETE")
    unreal.log("=" * 60)
    unreal.log("Next steps:")
    unreal.log("  1. Open BP_AscCharacter and assign IA_* actions + IMC_Default")
    unreal.log("  2. Open Ascent.umap, verify PrefireArena actor is present")
    unreal.log("  3. Verify SpotEditor exports prefire_spots.json to Content/Data/")
    unreal.log("  4. Play (Alt+P) to test")
    unreal.log("=" * 60)


if __name__ == "__main__":
    main()
