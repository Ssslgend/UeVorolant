import os

import unreal


DEST_LEVEL = "/Game/Training/TrainingHub"
REGIONS = [
    ("A_Site_Duel", "A Site", (-45000.0, 0.0, 0.0)),
    ("B_Site_Duel", "B Site", (-27000.0, 0.0, 0.0)),
    ("Mid_Duel", "Mid", (-9000.0, 0.0, 0.0)),
    ("A_Main_Entry", "A Main", (9000.0, 0.0, 0.0)),
    ("B_Main_Entry", "B Main", (27000.0, 0.0, 0.0)),
    ("Market_Door_Duel", "Market Door", (45000.0, 0.0, 0.0)),
]


def load_class(path):
    cls = unreal.load_class(None, path)
    if not cls:
        raise RuntimeError(f"Failed to load class: {path}")
    return cls


def spawn_actor(actor_class, location, rotation=(0.0, 0.0, 0.0), label=None):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class,
        unreal.Vector(*location),
        unreal.Rotator(*rotation),
    )
    if label:
        actor.set_actor_label(label)
    return actor


def spawn_region_mesh(region_name, label, location):
    mesh_path = f"/Game/AscentDuelRegions/{region_name}/{region_name}.{region_name}"
    mesh = unreal.load_asset(mesh_path)
    if not mesh:
        raise RuntimeError(f"Missing region mesh: {mesh_path}")

    actor = unreal.EditorLevelLibrary.spawn_actor_from_object(mesh, unreal.Vector(*location), unreal.Rotator(0, 0, 0))
    actor.set_actor_label(f"Region_{label}")
    actor.set_actor_scale3d(unreal.Vector(100.0, 100.0, 100.0))
    return actor


def add_lights():
    directional = spawn_actor(unreal.DirectionalLight, (0, -12000, 12000), (-45, 0, 0), "Training_DirectionalLight")
    directional.get_editor_property("light_component").set_editor_property("intensity", 3.0)

    sky = spawn_actor(unreal.SkyLight, (0, 0, 4000), (0, 0, 0), "Training_SkyLight")
    sky.get_editor_property("light_component").set_editor_property("intensity", 1.0)


def add_training_actors():
    player_start_class = unreal.PlayerStart
    target_class = load_class("/Script/ascent1.EnemyTarget")
    manager_class = load_class("/Script/ascent1.TrainingManager")

    spawn_actor(manager_class, (0, -3500, 250), (0, 0, 0), "TrainingManager")

    for region_name, label, base in REGIONS:
        x, y, z = base
        spawn_actor(player_start_class, (x, -6500, 250), (0, 0, 0), f"PlayerStart_{label}")
        target_a = spawn_actor(target_class, (x - 900, -1800, 250), (0, 0, 180), f"Bot_{label}_Left")
        target_b = spawn_actor(target_class, (x + 900, -1800, 250), (0, 0, 180), f"Bot_{label}_Right")
        target_c = spawn_actor(target_class, (x, 600, 250), (0, 0, 180), f"Bot_{label}_Deep")
        for target in (target_a, target_b, target_c):
            target.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))


def configure_world():
    game_mode = load_class("/Script/ascent1.AscGameMode")
    world = unreal.EditorLevelLibrary.get_editor_world()
    world_settings = world.get_world_settings()
    world_settings.set_editor_property("default_game_mode", game_mode)


def main():
    unreal.EditorLevelLibrary.new_level(DEST_LEVEL)
    for region_name, label, location in REGIONS:
        spawn_region_mesh(region_name, label, location)
    add_lights()
    add_training_actors()
    configure_world()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.log(f"[TrainingHub] Created {DEST_LEVEL}")


main()
