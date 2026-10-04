"""Add exact gameplay floors, cover, and headshot lines (run via Blender MCP execute_code)."""
import bpy
from mathutils import Matrix
import bmesh

root = bpy.data.collections.get("SimplifiedAscent")
if root is None:
    root = bpy.data.collections.new("SimplifiedAscent")
    bpy.context.scene.collection.children.link(root)


def ensure_collection(name):
    coll = bpy.data.collections.get(name)
    if coll is None:
        coll = bpy.data.collections.new(name)
        root.children.link(coll)
    return coll


def make_mat(name, color, rough=0.55, metallic=0.0, emission=None):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    if bsdf is None:
        bsdf = nodes.new("ShaderNodeBsdfPrincipled")
        out = nodes.get("Material Output") or nodes.new("ShaderNodeOutputMaterial")
        links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metallic
    if emission is not None and "Emission Color" in bsdf.inputs:
        bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
        bsdf.inputs["Emission Strength"].default_value = 5.0
    return mat


def add_box(name, location, size, material_name, collection, bevel=0.04):
    old = bpy.data.objects.get(name)
    if old:
        mesh = old.data
        bpy.data.objects.remove(old, do_unlink=True)
        if mesh and mesh.users == 0:
            bpy.data.meshes.remove(mesh)
    mat = bpy.data.materials.get(material_name)
    mesh = bpy.data.meshes.new(name + "_mesh")
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bm.to_mesh(mesh)
    bm.free()
    mesh.transform(Matrix.Diagonal((size[0] / 2, size[1] / 2, size[2] / 2, 1.0)))
    obj = bpy.data.objects.new(name, mesh)
    obj.location = location
    if bevel > 0:
        mod = obj.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 2
    if mat:
        mesh.materials.append(mat)
    ensure_collection(collection).objects.link(obj)
    return obj


make_mat("MAT_Floor_Play", (0.72, 0.66, 0.55), 0.8)
make_mat("MAT_Cover_Stone", (0.62, 0.58, 0.52), 0.75)
make_mat("MAT_HS_Stand", (1.0, 0.15, 0.15), 0.4, 0.1, emission=(1.0, 0.1, 0.1))
make_mat("MAT_HS_Crouch", (1.0, 0.55, 0.05), 0.4, 0.1, emission=(1.0, 0.45, 0.05))
make_mat("MAT_HS_Head", (0.1, 0.9, 1.0), 0.35, 0.15, emission=(0.1, 0.85, 1.0))

# Floors: name, xy, topZ (original inventory), size_xy
FLOORS = [
    ("FL_B_Site", (-114.9, 4.7), 62.71, (32.8, 28.1)),
    ("FL_B_Main", (-91.5, -22.3), 63.88, (29.2, 12.7)),
    ("FL_B_Main_1", (-102.3, -29.3), 63.88, (10.4, 13.8)),
    ("FL_Market", (-93.6, -49.2), 63.88, (27.6, 25.8)),
    ("FL_Switch_Ground", (-97.4, -0.7), 64.52, (46.0, 30.5)),
    ("FL_Market_W", (-78.9, -25.6), 65.15, (19.9, 21.4)),
    ("FL_B_Platform", (-77.6, 0.9), 66.74, (6.4, 12.7)),
    ("FL_Mid_West", (-54.0, -39.9), 65.33, (23.3, 48.0)),
    ("FL_Mid_North", (-48.9, 1.3), 66.42, (20.5, 29.0)),
    ("FL_Mid_Well", (-66.8, -3.2), 66.42, (15.2, 15.2)),
    ("FL_Mid_South", (-35.0, -49.0), 67.69, (28.0, 48.3)),
    ("FL_A_Garden", (6.3, -19.7), 67.69, (29.2, 12.7)),
    ("FL_A_TreeRoom", (-19.4, -19.8), 67.70, (22.3, 10.2)),
    ("FL_A_Main", (-0.7, -33.6), 67.81, (33.6, 15.0)),
    ("FL_A_Main_S", (-12.1, -52.0), 67.69, (15.5, 23.7)),
    ("FL_A_Main_FarS", (-14.0, -69.7), 67.69, (16.5, 11.8)),
    ("FL_A_Grass", (8.2, -14.8), 68.85, (25.4, 2.9)),
    ("FL_Atk_W", (-72.8, -89.1), 65.15, (31.7, 22.0)),
    ("FL_Atk_E", (-27.2, -87.9), 67.69, (15.6, 29.5)),
    ("FL_Atk_Mid", (-46.0, -89.0), 66.42, (22.0, 21.9)),
    ("FL_Atk_Court", (-64.4, -69.6), 65.15, (30.8, 19.7)),
    ("FL_Def_Spawn", (-46.6, 32.3), 66.45, (30.8, 32.9)),
    ("FL_Def_PathA", (-21.6, -2.2), 70.90, (21.7, 22.4)),
    ("FL_A_Heaven", (0.0, 11.5), 72.90, (28.0, 16.0)),
]

for name, xy, top_z, size in FLOORS:
    th = 0.25
    add_box(name, (xy[0], xy[1], top_z - th / 2), (size[0], size[1], th), "MAT_Floor_Play", "80_GameplayFloors", bevel=0.02)

# Key cover (original positions/heights) — headglitch relevant
COVERS = [
    ("Cov_A_St1", (17.1, -24.8, 69.92 - 1.15), (2.6, 2.6, 2.3)),
    ("Cov_A_St2", (17.1, -22.3, 69.92 - 1.15), (2.6, 2.6, 2.3)),
    ("Cov_A_St3", (19.6, -22.3, 69.92 - 1.15), (2.6, 2.6, 2.3)),
    ("Cov_A_St4", (19.5, -24.8, 69.92 - 1.15), (2.6, 2.6, 2.3)),
    ("Cov_A_St5", (19.6, -22.3, 72.46 - 1.15), (2.6, 2.6, 2.3)),
    ("Cov_A_St6", (19.6, -24.8, 72.46 - 1.15), (2.6, 2.6, 2.3)),
    ("Cov_A_St7", (13.3, -5.1, 71.19 - 1.15), (2.6, 2.6, 2.3)),
    ("Cov_A_St8", (1.9, 5.6, 71.19 - 1.15), (2.7, 2.6, 2.3)),
    ("Cov_A_Box", (10.7, -4.5, 71.51 - 1.25), (2.5, 2.5, 2.5)),
    ("Cov_Mid_S1", (-36.3, -65.5, 70.23 - 1.25), (2.5, 2.5, 2.5)),
    ("Cov_Mid_S2", (-47.7, -65.5, 70.23 - 1.25), (2.5, 2.5, 2.5)),
    ("Cov_Mid_S3", (-47.7, -65.5, 72.78 - 1.25), (2.5, 2.5, 2.5)),
    ("Cov_Mid_Tall1", (-37.7, -54.0, 74.39 - 2.0), (4.4, 3.6, 4.0)),
    ("Cov_Mid_Tall2", (-37.7, -57.3, 74.39 - 2.0), (4.4, 3.6, 4.0)),
    ("Cov_Mid_Tall3", (-37.7, -60.8, 74.39 - 2.0), (4.4, 3.6, 4.0)),
    ("Cov_AMain_S1", (-7.7, -60.4, 70.23 - 1.25), (2.5, 2.5, 2.5)),
    ("Cov_AMain_S2", (-7.7, -42.6, 70.23 - 1.25), (2.6, 2.6, 2.5)),
    ("Cov_B_S1", (-110.0, 1.9, 63.88 - 1.25), (2.5, 2.5, 2.5)),
    ("Cov_B_S2", (-109.3, -0.7, 63.88 - 1.25), (2.5, 2.5, 2.5)),
    ("Cov_B_S3", (-106.2, 13.3, 63.88 - 1.25), (2.5, 2.5, 2.5)),
    ("Cov_B_S4", (-106.2, 13.3, 66.42 - 1.25), (2.5, 2.5, 2.5)),
    ("Cov_B_S5", (-109.5, 0.7, 66.42 - 1.80), (3.6, 3.6, 2.5)),
    ("Cov_B_S6", (-101.1, -17.2, 66.40 - 1.25), (2.5, 2.5, 2.5)),
    ("Cov_B_S7", (-88.7, 16.7, 66.38 - 1.25), (2.5, 2.5, 2.5)),
    ("Cov_B_S8", (-99.8, -7.7, 63.87 - 1.25), (2.5, 2.5, 2.5)),
    ("Cov_B_S9", (-99.2, -5.7, 63.97 - 1.40), (2.8, 3.0, 2.8)),
    ("Cov_Tree", (-12.0, -12.3, 72.83 - 1.35), (3.2, 3.3, 2.7)),
]

for name, loc, size in COVERS:
    add_box(name, loc, size, "MAT_Cover_Stone", "81_GameplayCover", bevel=0.06)

# Headshot lines — UE/Valorant: Eye=1.60m, StandHalfHeight=0.915m, HeadRadius=0.12m
# Head hit sphere center ≈ feet + 1.71m; crouch head ≈ 1.10m
EYE = 1.60
CROUCH_HEAD = 1.10
HEAD_TOP = 1.71


def head_line(name, x, y, floor_top, length, axis="x", height=EYE, mat="MAT_HS_Stand"):
    thick = 0.08
    size = (length, thick, thick) if axis == "x" else (thick, length, thick)
    add_box(name, (x, y, floor_top + height), size, mat, "82_HeadshotLines", bevel=0.01)


def head_dot(name, x, y, floor_top, height=HEAD_TOP):
    add_box(name, (x, y, floor_top + height), (0.25, 0.25, 0.25), "MAT_HS_Head", "82_HeadshotLines", bevel=0.02)


T = {
    "A_site": 67.69,
    "A_main": 67.81,
    "B_site": 62.71,
    "B_main": 63.88,
    "B_plat": 66.74,
    "Mid_s": 67.69,
    "Market": 63.88,
    "Atk": 65.15,
    "Def": 66.45,
    "A_heaven": 72.90,
    "DefA": 70.90,
}

# A site holds
head_line("HS_A_FromMain", 8.0, -5.0, T["A_site"], 20.0, "x", EYE, "MAT_HS_Stand")
head_dot("HSDOT_A_Plant", 8.2, -4.5, T["A_site"], HEAD_TOP)
head_line("HS_A_Crouch", 8.0, -5.0, T["A_site"], 16.0, "x", CROUCH_HEAD, "MAT_HS_Crouch")
head_line("HS_A_FromHeaven", 10.0, -20.0, T["A_site"], 18.0, "y", EYE, "MAT_HS_Stand")
head_dot("HSDOT_A_BoxStack", 19.6, -23.5, 72.46, HEAD_TOP)

# A main
head_line("HS_AMain", -2.0, -33.6, T["A_main"], 22.0, "x", EYE, "MAT_HS_Stand")
head_dot("HSDOT_AMain", -2.0, -33.6, T["A_main"], HEAD_TOP)

# Mid
head_line("HS_Mid_Main", -38.0, -30.0, T["Mid_s"], 24.0, "y", EYE, "MAT_HS_Stand")
head_dot("HSDOT_Mid", -38.0, -30.0, T["Mid_s"], HEAD_TOP)
head_line("HS_Mid_Crouch", -38.0, -30.0, T["Mid_s"], 20.0, "y", CROUCH_HEAD, "MAT_HS_Crouch")
head_dot("HSDOT_Mid_Tall", -37.7, -57.3, 74.39, HEAD_TOP)

# B site
head_line("HS_B_Site", -110.0, 3.0, T["B_site"], 18.0, "x", EYE, "MAT_HS_Stand")
head_dot("HSDOT_B_Plant", -109.4, 2.8, T["B_site"], HEAD_TOP)
head_line("HS_B_Crouch", -110.0, 3.0, T["B_site"], 16.0, "x", CROUCH_HEAD, "MAT_HS_Crouch")
head_line("HS_B_FromPlat", -95.0, 1.0, T["B_site"], 20.0, "x", EYE, "MAT_HS_Stand")
head_dot("HSDOT_B_Plat", -77.6, 0.9, T["B_plat"], HEAD_TOP)

# B main / market
head_line("HS_B_Main", -91.5, -22.3, T["B_main"], 18.0, "x", EYE, "MAT_HS_Stand")
head_dot("HSDOT_B_Main", -91.5, -22.3, T["B_main"], HEAD_TOP)
head_line("HS_Market", -93.6, -42.0, T["Market"], 16.0, "x", EYE, "MAT_HS_Stand")
head_dot("HSDOT_Market", -93.6, -42.0, T["Market"], HEAD_TOP)

# Heaven / def / spawn
head_line("HS_Heaven", -5.0, 12.0, T["A_heaven"], 14.0, "x", EYE, "MAT_HS_Stand")
head_dot("HSDOT_Heaven", -5.0, 12.0, T["A_heaven"], HEAD_TOP)
head_line("HS_DefA", -21.6, -2.2, T["DefA"], 16.0, "x", EYE, "MAT_HS_Stand")
head_dot("HSDOT_DefA", -21.6, -2.2, T["DefA"], HEAD_TOP)
head_line("HS_Atk", -55.0, -90.0, T["Atk"], 20.0, "x", EYE, "MAT_HS_Stand")
head_line("HS_Def", -46.6, 32.3, T["Def"], 18.0, "x", EYE, "MAT_HS_Stand")

# Height reference pole at A plant (1.83 stand / 1.60 eye / 1.71 head / 1.10 crouch)
add_box("Ref_Pole_A", (8.2, -8.5, T["A_site"] + 0.915), (0.15, 0.15, 1.83), "MAT_HS_Head", "82_HeadshotLines", bevel=0.01)
add_box("Ref_Eye_A", (8.2, -8.5, T["A_site"] + EYE), (0.55, 0.12, 0.12), "MAT_HS_Stand", "82_HeadshotLines", bevel=0.01)
add_box("Ref_Head_A", (8.2, -8.5, T["A_site"] + HEAD_TOP), (0.55, 0.12, 0.12), "MAT_HS_Head", "82_HeadshotLines", bevel=0.01)
add_box("Ref_Crouch_A", (8.2, -8.5, T["A_site"] + CROUCH_HEAD), (0.55, 0.12, 0.12), "MAT_HS_Crouch", "82_HeadshotLines", bevel=0.01)

print("GAMEPLAY_OK meshes=", sum(1 for o in bpy.data.objects if o.type == "MESH"))
