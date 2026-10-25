# toolgun_fire_check.py <log> -- toolgun_fire_test.sh's verdict (see there).
import re, sys

lines = open(sys.argv[1], encoding='utf-8', errors='replace').read().splitlines()
fails = []
def check(ok, what):
    print(('ok   ' if ok else 'FAIL ') + what)
    if not ok:
        fails.append(what)

spawned = {}  # classname -> entity
for l in lines:
    m = re.search(r'toolgun: spawned (\d+) (\S+)', l)
    if m:
        spawned[m.group(2)] = int(m.group(1))
step, lights, sounds = '', {}, {}
for l in lines:
    m = re.match(r'STEP (\w+)', l)
    if m:
        step = m.group(1)
        continue
    m = re.match(r'torch light: entity (\d+) at (-?[\d.]+) (-?[\d.]+) (-?[\d.]+)', l)
    if m:
        lights.setdefault(step, {})[int(m.group(1))] = tuple(float(v) for v in m.group(2, 3, 4))
    m = re.search(r'ENT:\s*(\d+) CH:\s*\d+ \| ambience/fire1\.wav', l)
    if m:
        sounds.setdefault(step, set()).add(int(m.group(1)))

# 1. The wall torch.
m = next((re.search(r'made live at (\S+) (\S+) (\S+), facing (\S+), crackle (\S+), takeable (\S+)', l) for l in lines
          if 'walltorch: made live' in l), None)
check(m is not None, 'wall torch made live')
if m:
    x, face, crackle, take = float(m.group(1)), float(m.group(4)), float(m.group(5)), float(m.group(6))
    check(abs(x - -590) < 2.5, f'wall torch hung 10 units off the wall (x {x:.1f}, the wall -600)')
    check(abs(face) < 1, f'wall torch faces out of the wall (yaw {face:g})')
    check(crackle == 1 and take == 1, f'wall torch crackle due {crackle:g}, takeable {take:g}')
# 2. The flames: props, each with its light and (but the lantern) its crackle.
kinds = {'light_flame_large_yellow': 2, 'light_flame_small_yellow': 1, 'light_flame_small_white': 1, 'light_lantern': 0}
for cls, kind in kinds.items():
    if cls == 'light_lantern' and cls not in spawned:
        print('skip lantern (no Rogue data)')
        continue
    check(any(f'flameprop: {cls} ({kind})' in l for l in lines), f'{cls}: a flame prop (kind {kind})')
everything = dict(spawned)
for cls, ent in everything.items():
    check(ent in lights.get('lights', {}), f'{cls} ({ent}): its torch light')
    if cls != 'light_lantern':
        check(ent in sounds.get('lights', set()), f'{cls} ({ent}): its crackle (fire1.wav on it)')
# 3. The large flame taken, let go of: it falls.
check(any('carry: light_flame_large_yellow held' in l for l in lines), 'the large flame taken by the off hand')
big = spawned.get('light_flame_large_yellow')
before, after = lights.get('lights', {}).get(big), lights.get('dropped', {}).get(big)
check(before is not None and after is not None and (after[0], after[1]) != (before[0], before[1]) or
      (before and after and after[2] < before[2] - 4), f'the large flame moved once taken and let go of ({before} -> {after})')
# 4. The wall torch taken by the grip (the one made here: the nearest).
m = next((re.search(r"at the wall torch at '?\s*(-?[\d.]+)\s+(-?[\d.]+)", l) for l in lines if 'walltorch test: kind 24' in l), None)
check(m is not None and abs(float(m.group(1)) + 590) < 2.5 and abs(float(m.group(2)) + 710) < 2.5, 'the grip test took the wall torch made here')
check(any('walltorch grip result: detached=1 held=1' in l for l in lines), 'the wall torch taken off its wall into the hand')
print('PASS' if not fails else f'FAIL ({len(fails)})')
