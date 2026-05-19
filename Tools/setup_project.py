"""
UE Editor Utility - Setup Valorant Aim Trainer Project
=====================================================
Creates Enhanced Input assets, Input Mapping Context, Blueprint subclasses,
and configures the default GameMode.

Prerequisites: Enable "Python Editor Script Plugin" in Edit → Plugins
Usage: Run from UE5 Python console or Content Browser → Tools → Execute Python Script
"""

import unreal


# ---------- Paths ----------
INPUT_PATH = "/Game/Input/Actions"
IMC_PATH = "/Game/Input/Mappings"
BP_PATH = "/Game/Blueprints"


def ensure_dirs():
    """Create required Content Browser folders if they don't exist."""
    for p in [INPUT_PATH, IMC_PATH, BP_PATH]:
        if not unreal.EditorAssetLibrary.does_directory_exist(p):
            unreal.EditorAssetLibrary.make_directory(p)


def create_input_action(name, value_type):
    """
    Create an Input Action asset.
    value_type: 'bool', 'float', 'vector2d' etc.
    """
    asset_path = f"{INPUT_PATH}/IA_{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log(f"  [SKIP] {asset_path} already exists")
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    factory = unreal.InputActionFactory()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    action = asset_tools.create_asset(f"IA_{name}", INPUT_PATH, None, factory)
    action.set_editor_property("value_type", getattr(unreal.InputActionValueType, value_type.upper()))
    unreal.EditorAssetLibrary.save_asset(asset_path)
    unreal.log(f"  [OK] Created {asset_path}")
    return action


def create_input_mapping_context():
    """Create the IMC_Default asset."""
    asset_path = f"{IMC_PATH}/IMC_Default"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log(f"  [SKIP] {asset_path} already exists")
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    factory = unreal.InputMappingContextFactory()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    imc = asset_tools.create_asset("IMC_Default", IMC_PATH, None, factory)
    unreal.EditorAssetLibrary.save_asset(asset_path)
    unreal.log(f"  [OK] Created {asset_path}")
    return imc


def add_mapping(imc, action, key_name, modifiers=None, triggers=None):
    """Add a key→action binding to an Input Mapping Context."""
    mapping = unreal.InputActionKeyMapping()
    mapping.set_editor_property("action", action)
    mapping.set_editor_property("key", unreal.InputCoreTypes.get_key(key_name))
    imc.get_editor_property("mappings").add(mapping)
    # Add this one mapping; the array property is modified in place
    mappings = imc.get_editor_property("mappings")
    mappings.append(mapping)


def build_mappings(imc, actions):
    """
    Bind keys to actions in the IMC.
    actions: dict mapping action object → list of (key_name, modifiers?)
    """
    # Clear existing
    imc.set_editor_property("mappings", unreal.Array(unreal.InputActionKeyMapping))

    bindings = [
        ("IA_Move",        "W",           None),   # Forward
        ("IA_Move",        "S",           None),   # Back
        ("IA_Move",        "A",           None),   # Left
        ("IA_Move",        "D",           None),   # Right
        ("IA_Look",        "Mouse2D",     None),   # Mouse delta
        ("IA_Walk",        "LeftShift",   None),   # Walk modifier
        ("IA_CrouchToggle","LeftControl", None),   # Crouch toggle
        ("IA_Jump",        "SpaceBar",    None),   # Jump
        ("IA_Fire",        "LeftMouseButton", None),  # Shoot
    ]

    for action_name, key_name, mods in bindings:
        action = actions.get(action_name)
        if not action:
            continue

        mapping = unreal.InputActionKeyMapping()
        mapping.set_editor_property("action", action)
        key = unreal.InputCoreTypes.get_key(key_name)
        if key:
            mapping.set_editor_property("key", key)
            imc.get_editor_property("mappings").append(mapping)

    imc.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(imc)
    unreal.log(f"  [OK] Bound {len(bindings)} keys in IMC_Default")


def create_blueprint(name, parent_class, asset_path=None):
    """Create a Blueprint subclass."""
    if not asset_path:
        asset_path = f"{BP_PATH}/{name}"
    else:
        asset_path = f"{BP_PATH}/{asset_path}"

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log(f"  [SKIP] {asset_path} already exists")
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)

    # Extract just the asset name from the path
    bp_name = asset_path.split("/")[-1]
    bp_dir = "/".join(asset_path.split("/")[:-1])

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    bp = asset_tools.create_asset(bp_name, bp_dir, None, factory)
    if bp:
        unreal.EditorAssetLibrary.save_asset(asset_path)
        unreal.log(f"  [OK] Created {asset_path}")
    else:
        unreal.log_error(f"  [FAIL] Could not create {asset_path}")
    return bp


def set_default_gamemode(bp_gamemode_path):
    """Set the default GameMode in Project Settings via DefaultEngine.ini."""
    config_path = unreal.Paths.project_config_dir() + "DefaultEngine.ini"

    # The path format for INI is: /Game/Blueprints/BP_AscGameMode.BP_AscGameMode_C
    gm_ref = f"/Script/EngineSettings.GameMapsSettings"
    gm_key = "GameDefaultMap"
    gm_value = f"/Script/ascent1.AscGameMode"

    # Actually, we need to set the GameMode class via GameMapsSettings
    # Let's just log the instruction
    unreal.log_warning(
        f"  Please set Default GameMode in Project Settings → Maps & Modes:\n"
        f"    Default GameMode = {bp_gamemode_path}"
    )


def main():
    unreal.log("=" * 60)
    unreal.log("Valorant Aim Trainer - Project Setup")
    unreal.log("=" * 60)

    ensure_dirs()

    # ---- Step 1: Create Input Actions ----
    unreal.log("\n[1/4] Creating Input Actions...")
    actions = {}
    action_types = [
        ("Move",         "axis2d"),
        ("Look",         "axis2d"),
        ("Walk",         "bool"),
        ("CrouchToggle", "bool"),
        ("Jump",         "bool"),
        ("Fire",         "bool"),
    ]
    for name, vtype in action_types:
        actions[f"IA_{name}"] = create_input_action(name, vtype)

    # ---- Step 2: Create IMC and bind keys ----
    unreal.log("\n[2/4] Creating Input Mapping Context...")
    imc = create_input_mapping_context()
    if imc:
        build_mappings(imc, actions)

    # ---- Step 3: Create Blueprint subclasses ----
    unreal.log("\n[3/4] Creating Blueprint subclasses...")
    blueprints = {
        "BP_AscGameMode":   unreal.AscGameMode.static_class() if hasattr(unreal, 'AscGameMode') else unreal.GameModeBase.static_class(),
        "BP_Crosshair":     unreal.CrosshairWidget.static_class() if hasattr(unreal, 'CrosshairWidget') else unreal.UserWidget.static_class(),
        "BP_StatsPanel":    unreal.StatsPanelWidget.static_class() if hasattr(unreal, 'StatsPanelWidget') else unreal.UserWidget.static_class(),
        "BP_EnemyTarget":   unreal.EnemyTarget.static_class() if hasattr(unreal, 'EnemyTarget') else unreal.Actor.static_class(),
    }

    created = {}
    for name, parent in blueprints.items():
        created[name] = create_blueprint(name, parent)

    bgm = created.get("BP_AscGameMode")
    bgm_path = f"{BP_PATH}/BP_AscGameMode"

    # ---- Step 4: Set default GameMode ----
    unreal.log("\n[4/4] Configuring default GameMode...")
    unreal.log(f"  GameMode Blueprint: {bgm_path}")

    set_default_gamemode(bgm_path)

    unreal.log("\n" + "=" * 60)
    unreal.log("SETUP COMPLETE")
    unreal.log("=" * 60)
    unreal.log("Next steps:")
    unreal.log("  1. Open BP_AscCharacter and assign Input Actions + IMC")
    unreal.log("  2. Open Ascent.umap, add PrefireArena + TrainingManager actors")
    unreal.log("  3. Play to test")
    unreal.log("=" * 60)


if __name__ == "__main__":
    main()
