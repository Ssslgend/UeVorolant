"""Semantic filtering for the unmodified original Ascent scene.

Coordinates below are the original source's coordinates, not AscentPlayable's.
No combat walls, floors, or covers are fabricated by this policy.
"""
import re

REGIONS = {
    'A_Site': (38, 85, 48, 101),
    'A_Main': (36, 98, 10, 56),
    'Tree_Link': (27, 49, 49, 79),
    'Mid': (-3, 46, 14, 91),
    'B_Site': (-53, 4, 63, 95),
    'B_Main_Market': (-43, 8, 12, 67),
    'Attack_Routes': (-17, 58, -16, 31),
    'Defend_Routes': (-8, 45, 83, 116),
}

# Highest priority: existing combat floors, doors, cover and target-height edges.
COVER = re.compile(r'(?i)(stonebrickcover|^generator_|^midbench_.*gardenbox|'
                   r'^shard_.*box|^crate_[0-9]_wood|^woodbox|^ftbox_|^tarpcrate_|'
                   r'^boat_1_boxsternedrowboat|^boatshop_0_mainbarricade|'
                   r'^abombsite_0_tarpcrates|^ascaffold_1_covera(?!pipes)|'
                   r'^kingdom.*(?:crate|box))')
FLOOR = re.compile(r'(?i)(floor|ground|stairs?|ramp|platform|catwalk|underpath|'
                   r'^atkbridge_.*(?:base|floor|walkway))')
CORE = re.compile(r'(?i)(^shell_|wall|archway|building|castle|ceiling|'
                  r'foundation|^midcolumn|^column|^switchhouse|'
                  r'^abombsite_0_(?:arch|kingdomdoor)|'
                  r'^door_|^gate_0_door|^bswitchroom|^bmaininrerior|'
                  r'^ascaffold_.*(?:wall|woodpanel|concrete|floor|platform)|'
                  r'^atkstairs|^kingdom_.*reactor)')
REFERENCE = re.compile(r'(?i)(trim|^stonethin_|wallfence|wallsupport)')
# Functional structures can have names also used by the decoration filter.
# These are placed source meshes, not replacements fabricated from boxes.
FUNCTIONAL_STRUCTURE = re.compile(
    r'(?i)(^kingdom_0_midramprailing|^midfloor_0_flowerrailing|'
    r'^bsite_0_boathouse(?:base|chunk)|^boatshop_0_(mainwalls|mainbeams|archway)|'
    r'^railing_0_boathouserailing|^pizza_0_rollupdoor|^defarch_2_door|'
    r'^atktower_1_archcolumn)')
DECOR = re.compile(r'(?i)(foliage|tree(?!room)|grass|flower|ivy|leaves|plant|pot_|'
                   r'window|shutter|rainpipe|pipe|wire|wiring|cable|decal|sign|'
                   r'poster|banner|flag|light|lamp|antenna|gravel|rubble|'
                   r'wine|vino|fruit|fish|boatshop|baitshop|gondola|oar|'
                   r'curtain|drape|clothwrap|tiles|terracotta|shingle|chimney|'
                   r'dormer|topper|doorbell|vent|tools|trash|papers|rug|'
                   r'teleporter|moderntech|techelement|conduit|rafter|'
                   r'balcony|wall(sconce|electric)|awning|telephone|spice|'
                   r'plushie|canalpole|roof_[0-9]|midroof|roofstrip|'
                   r'roof.*detail|reactordetail|keystone|brick_[0-9]|'
                   r'woodplanks|woodenbeam|interiorwoodsupport|railing|'
                   r'cloth|rope|^dock_|^prop_|^detail_|^floater_|^rubble)')

def classify(row, visibility):
    name = row['name']
    cols = row['collections']
    center = row['bbox_center']
    size = row['bbox_size']
    mn, mx = row['bbox_min'], row['bbox_max']
    if visibility.get('hide_render') or visibility.get('hide_viewport'):
        return None, 'hidden_source_template'
    if any('Vista' in c or 'VFX' in c or 'Lighting' in c for c in cols):
        return None, 'vista_effects_lighting'
    if not any(c.startswith('Ascent_Art_') or c in {'Ascent_Gameplay', 'Door A', 'Door B', 'Key A', 'Key B'} for c in cols):
        return None, 'outside_gameplay_collections'
    in_region = any(x0 <= center[0] <= x1 and y0 <= center[1] <= y1
                    for x0, x1, y0, y1 in REGIONS.values())
    structure_overlaps = (COVER.search(name) or CORE.search(name) or FUNCTIONAL_STRUCTURE.search(name)) and any(
        mx[0] >= x0 and mn[0] <= x1 and mx[1] >= y0 and mn[1] <= y1
        for x0, x1, y0, y1 in REGIONS.values())
    if not in_region and not structure_overlaps:
        return None, 'outside_combat_routes'
    if mn[2] > 13 or mx[2] < -3:
        return None, 'above_or_below_playable_area'
    if 'Ascent_Gameplay' in cols:
        if abs(center[0]) < 0.1 and abs(center[1]) < 0.1:
            return None, 'unplaced_bombsite_outline_template'
        return 'Site_Markers', 'original_bombsite_outline'
    if 'Door A' in cols or 'Door B' in cols:
        return 'Doors', 'original_combat_door'
    if 'Key A' in cols or 'Key B' in cols:
        return None, 'decorative_door_switch'
    if COVER.search(name):
        if center[2] > 10.8:
            return None, 'inaccessible_rooftop_prop'
        return 'Cover', 'original_cover'
    if FUNCTIONAL_STRUCTURE.search(name):
        return 'Walls_Structures', 'original_functional_structure'
    if name.startswith('AtkStairs_'):
        return 'Floors_Stairs', 'original_combat_stairs'
    # Keep horizontal original wall bands even when they are small.
    if REFERENCE.search(name) and 'decal' not in name.lower():
        if max(size[0], size[1]) > 0.7 and size[2] < 0.65 and center[2] < 8.8:
            return 'Reference_Edges', 'original_horizontal_height_reference'
    if DECOR.search(name):
        if FLOOR.search(name) and name.lower().startswith(('ground', 'aground', 'aramp', 'gardenlevel', 'courtyardground')):
            return 'Floors_Stairs', 'original_walkable_surface'
        return None, 'decoration'
    if FLOOR.search(name):
        if mn[2] > 10:
            return None, 'inaccessible_rooftop_floor'
        return 'Floors_Stairs', 'original_walkable_surface'
    if CORE.search(name):
        if len(name.split('_')) > 1 and max(size) < 0.6:
            return None, 'small_architecture_detail'
        # Remove isolated rooftop silhouettes while retaining tunnel ceilings.
        if 'roof' in name.lower() and mn[2] > 10:
            return None, 'noncombat_roof'
        if 'boathousechunk' in name.lower():
            return None, 'exterior_boathouse_scenery'
        return 'Walls_Structures', 'original_combat_structure'
    return None, 'nonessential_model'

def region_of(row):
    cols = row['collections']
    mapping = [('Ascent_Art_A', 'A_Site'), ('Ascent_Art_AtkPathA', 'A_Main'),
               ('Ascent_Art_APathMid', 'Tree_Link'), ('Ascent_Art_B', 'B_Site'),
               ('Ascent_Art_AtkPathB', 'B_Main_Market'), ('Ascent_Art_DefPathB', 'B_Link'),
               ('Ascent_Art_DefPathA', 'A_Heaven_Link'), ('Ascent_Art_Mid', 'Mid'),
               ('Ascent_Art_Atk', 'Attack_Routes'), ('Ascent_Art_Def', 'Defend_Routes')]
    return next((region for col, region in mapping if col in cols), 'Gameplay')
