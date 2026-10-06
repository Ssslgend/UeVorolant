"""Regression checks against real placed objects in the original map inventory."""
import json
from pathlib import Path
from combat_ascent_policy import classify

root = Path(__file__).resolve().parents[1]
out = root / 'Saved/AscentCombatSimple'
rows = {r['name']: r for r in json.loads((out/'source_inventory.json').read_text(encoding='utf8'))['objects']}
visibility = {r['name']: r for r in json.loads((out/'source_visibility.json').read_text(encoding='utf8'))['objects']}
required = {
    'Kingdom_0_MidWall': 'Walls_Structures',
    'Shell_3_MidWallA': 'Walls_Structures',
    'Kingdom_0_MidRampRailing_3.001': 'Walls_Structures',
    'Kingdom_0_MidRampRailing_3.002': 'Walls_Structures',
    'FTBox_0_C_0': 'Cover',
    'Pizza_0_RollUpDoor': 'Walls_Structures',
    'BSite_0_BoathouseBase': 'Walls_Structures',
    'BoatShop_0_MainWalls': 'Walls_Structures',
    'BSite_0_BoatHouseChunkA': 'Walls_Structures',
    'BoatShop_0_MainBarricade': 'Cover',
    'Boat_1_BoxSternedRowboat_55': 'Cover',
    'ABombsite_0_TarpCratesUp': 'Cover',
    'AScaffold_1_CoverAISMA_1': 'Cover',
    'TarpCrate_0_CornerCloth_6': 'Cover',
}
failures = []
for name, want in required.items():
    got, reason = classify(rows[name], visibility[name])
    print(f'{"PASS" if got == want else "FAIL"} {name}: {got} ({reason}); expected {want}')
    if got != want: failures.append(name)
assert not failures, 'Missing combat structures: ' + ', '.join(failures)
