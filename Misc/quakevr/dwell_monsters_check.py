# dwell_monsters_check.py <log> -- dwell_monsters_test.sh's verdict (DWELL.md P2b): each "== <choice> <case>" section of
# the log against what that monster should show. Prints a FAIL line per failed case and
# "dwell_monsters_test: N passed, M failed".
import re, sys

NAMES = {'70': 'monster_kamikaze', '71': 'monster_brute', '72': 'monster_excavator', '73': 'monster_d1boss'}
# (choice, case) -> regexes that must all match somewhere in its section; a callable for counts
WANT = {
    ('70', 'spawn'): [r'near monster_kamikaze .* mon \d+ head 1 \(0 19 6 .*rig 0'],
    ('70', 'kill'): [r'monster_kamikaze hit: health -?\d+', r'near monster_kamikaze model progs/s_explod\.spr'],
    ('71', 'spawn'): [r'near monster_brute .* head 1 \(5 61 10 .*headmdl progs/e2/h_brute\.mdl corpse 1 kd 1 rig 1'],
    ('71', 'down'): [r'state health \d+ ragdoll 1 knockdown 1', r'knockdown: monster_brute gets up',
                     r'state health \d+ ragdoll 0 knockdown 0 .* solid 3'],
    ('71', 'downkill'): [r'state health -\d+ ragdoll 1 .* corpse 1 dying 1'],
    ('71', 'decap'): [r'decaptest: 1 .* monster_brute: beheaded', r'decaptest: head progs/e2/h_brute\.mdl'],
    ('71', 'limbs'): lambda s: len(re.findall(r'limbtest: .* on monster_brute: (cut [1-9]|cut \d+ popped [1-9])', s)) >= 3, # (the slash, the fist, the explosion: the shotgun's and the bolt's ray can miss a limb at this range)
    ('71', 'corpse'): [r'monster_brute hit: health -\d+ corpse 1', r'state health -\d+ ragdoll 1 .* corpse 2',
                       r'admtest: 0 AD monster classes'],
    ('72', 'spawn'): [r'near monster_excavator .* head 0 .*rig 0'],
    ('72', 'kill'): [r'monster_excavator hit: health -?\d+', r'near monster_excavator model progs/s_explod\.spr'],
    ('73', 'spawn'): [r'near monster_d1boss .* head 1 \(0 125 16 '],
}

text = open(sys.argv[1], encoding='utf-8', errors='replace').read()
parts = re.split(r'^== (\d+) (\w+)\s*$', text, flags=re.M)
passed = failed = 0
for i in range(1, len(parts) - 2, 3):
    key, body = (parts[i], parts[i + 1]), parts[i + 2]
    want = WANT.get(key)
    if want is None:
        continue
    bad = [] if not re.search(r'rror|CRASH', body) else ['an error line']
    if callable(want):
        if not want(body):
            bad.append('too few limbs cut or popped')
    else:
        bad += [w for w in want if not re.search(w, body)]
    if bad:
        failed += 1
        print('FAIL %s %s (%s): %s' % (key[0], key[1], NAMES.get(key[0], '?'), '; '.join(bad)))
    else:
        passed += 1
print('dwell_monsters_test: %d passed, %d failed' % (passed, failed))
