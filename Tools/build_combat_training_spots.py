"""Find real corner-peek pairs on the delivered map, without adding cover."""
import bpy, json, math, os, sys
from mathutils import Vector
from mathutils.bvhtree import BVHTree

out = os.path.abspath(sys.argv[sys.argv.index('--') + 1])
objects = [o for o in bpy.context.scene.objects if o.type == 'MESH']

def tree_for(items):
    vertices, faces = [], []
    for obj in items:
        base = len(vertices)
        vertices.extend([obj.matrix_world @ v.co for v in obj.data.vertices])
        faces.extend([tuple(base + i for i in p.vertices) for p in obj.data.polygons])
    return BVHTree.FromPolygons(vertices, faces)

world = tree_for(objects)
floors = tree_for([o for o in objects if o.get('combat_category') == 'Floors_Stairs'])

def floor_at(x, y, hint):
    hit = floors.ray_cast(Vector((x, y, hint + .40)), Vector((0, 0, -1)), 1.3)
    if hit[0] is None or abs(hit[1].z) < .65: return None
    return hit[0]

def clear_segment(a, b):
    d = b - a
    if d.length < .001: return True
    return world.ray_cast(a, d.normalized(), max(0, d.length - .02))[0] is None

def standing_clear(p):
    # Probe a standing capsule's side clearance at legs, chest and head.
    for z in [.25, .9, 1.55, 1.82]:
        origin = p + Vector((0, 0, z))
        for d in [(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0)]:
            if world.ray_cast(origin, Vector(d), .28)[0] is not None: return False
    return clear_segment(p + Vector((0, 0, .12)), p + Vector((0, 0, 1.82)))

def visible(player, enemy):
    return clear_segment(player + Vector((0, 0, 1.6)), enemy + Vector((0, 0, 1.6)))

def reachable(a, b):
    # A straight walking peek needs continuous ground and capsule clearance.
    for i in range(9):
        p = a.lerp(b, i / 8)
        hit = floor_at(p.x, p.y, p.z)
        if hit is None or abs(hit.z - p.z) > .18 or not standing_clear(hit): return False
    for z in [.4, .9, 1.6]:
        for offset in [-.24, 0, .24]:
            d = (b - a).normalized()
            side = Vector((-d.y, d.x, 0)) * offset
            if not clear_segment(a + side + Vector((0, 0, z)), b + side + Vector((0, 0, z))): return False
    return True

# Hints are on source combat floors. Search nearby rather than trust a guessed point.
profiles = [
    ('a_main', 'A大入口', 'A', (66, 53, 4), (64, 69, 5)),
    ('a_tree', '树房进A', 'A', (48, 60, 4), (58, 67, 5)),
    ('a_site', 'A点箱体', 'A', (62, 65, 5), (66, 75, 5)),
    ('a_heaven', 'A二楼', 'A', (61, 79, 8), (63, 75, 5)),
    ('mid_top', '中路高台', 'Mid', (27, 30, 4), (27, 52, 4)),
    ('mid_courtyard', '中路庭院', 'Mid', (13, 37, 2), (6, 56, 2)),
    ('mid_market', '中路市场', 'Mid', (4, 58, 2), (-7, 58, 2)),
    ('mid_catwalk', '中路小道', 'Mid', (29, 50, 4), (36, 57, 4)),
    ('b_main', 'B大入口', 'B', (-25, 60, 1), (-31, 75, -1)),
    ('b_market', '市场进B', 'B', (-3, 72, 3), (-18, 79, 1)),
    ('b_boat', 'B点船屋', 'B', (-27, 77, -1), (-40, 81, -1)),
    ('b_stairs', 'B点楼梯', 'B', (-14, 82, 1), (-27, 80, -1)),
]
points, missing = [], []
for ident, label, region, player_hint, enemy_hint in profiles:
    enemy_candidates = []
    for ox, oy in [(0, 0), (1, 0), (-1, 0), (0, 1), (0, -1), (2, 0), (-2, 0), (0, 2), (0, -2), (3, 0), (-3, 0)]:
        p = floor_at(enemy_hint[0] + ox, enemy_hint[1] + oy, enemy_hint[2])
        if p is not None and standing_clear(p): enemy_candidates.append(p)
    candidates = []
    for ox in range(-6, 7):
        for oy in range(-6, 7):
            p = floor_at(player_hint[0] + ox * .65, player_hint[1] + oy * .65, player_hint[2])
            if p is not None and standing_clear(p): candidates.append(p)
    best = None
    for enemy in enemy_candidates:
        hidden = [p for p in candidates if (p - enemy).length > 3 and not visible(p, enemy)]
        for p in hidden:
            for ox, oy in [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, 1), (1, -1), (-1, -1)]:
                peek = floor_at(p.x + ox * 1.3, p.y + oy * 1.3, p.z)
                if peek is None or not standing_clear(peek) or not visible(peek, enemy): continue
                if not reachable(p, peek): continue
                score = (p - Vector(player_hint)).length + .25 * (enemy - Vector(enemy_hint)).length
                if best is None or score < best[0]: best = (score, p, enemy, peek)
    if not best:
        missing.append(ident)
        print('[combat-spots] no valid corner for', ident, 'standable player/enemy candidates', len(candidates), len(enemy_candidates), flush=True)
        continue
    _, player, enemy, peek = best
    points.append({'id': 'ascent_' + ident, 'name': label, 'region': region,
                   'player_m': list(player), 'enemy_m': list(enemy), 'peek_m': list(peek),
                   'validation': {'hidden_at_start': True, 'visible_after_peek': True,
                                  'walking_path_clear': True, 'standing_clearance_m': .28}})
    print('[combat-spots]', ident, 'player', tuple(player), 'enemy', tuple(enemy), flush=True)
report = {'source_blend': bpy.data.filepath, 'coordinate_units': 'meters', 'points': points,
          'missing_profiles': missing, 'validated_points': len(points)}
assert all(any(p['region'] == r for p in points) for r in ['A', 'B', 'Mid']), 'Missing a combat region'
with open(out, 'w', encoding='utf8') as f: json.dump(report, f, ensure_ascii=False, indent=2)
print('[combat-spots] generated', len(points), 'verified original-map corner pairs', flush=True)
