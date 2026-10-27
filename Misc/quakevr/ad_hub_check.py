import sys, re
# ad_hub_check.py <log>: ad_hub_test.sh's verdict: AD's hub loadout, then each slipgate's run ended on its map's title.
text = open(sys.argv[1], encoding='latin-1').read().splitlines()
passed = failed = 0
def check(ok, what):
    global passed, failed
    if ok: passed += 1
    else: failed += 1
    print('adhub: %s %s' % ('ok  ' if ok else 'FAIL', what))
# the player's fields in the save made 200 frames into a new game (the edict whose classname is "player")
player = {}
try:
    sav = open(sys.argv[2], encoding='latin-1').read()
    for blk in re.findall(r'\{([^{}]*)\}', sav):
        kv = dict(re.findall(r'"([^"]*)" "([^"]*)"', blk))
        if kv.get('classname') == 'player':
            player = kv; break
except OSError as e:
    print('adhub: no save: %s' % e)
def num(name):
    return float(player.get(name, '0'))
check(num('items') == 12295, 'new game items %g (want 12295: axe, shotgun, super shotgun, nailgun, green armour)' % num('items'))
check(num('health') == 75, 'new game health %g (want 75: start\'s reset_health)' % num('health'))
check(num('armorvalue') == 25, 'new game armour %g (want 25)' % num('armorvalue'))
holsters = [v for k, v in sorted(player.items()) if k.startswith('holsterweaponmodel')]
check('progs/v_shot2.mdl' in holsters and 'progs/v_nail.mdl' in holsters,
      'super shotgun and nailgun holstered: %s' % ' '.join(holsters))
# each gate's section: from its "ADHUB gate" line to the next
sections = []
for l in text:
    m = re.match(r'^ADHUB gate (\S+)', l)
    if m: sections.append([m.group(1), []])
    elif sections: sections[-1][1].append(l)
for gate, lines in sections:
    quit_on = any(l.startswith('exit=quit-on') for l in lines)
    refused = [l for l in lines if 'campaign' in l]
    check(quit_on and not refused, 'slipgate to %s: %s' % (gate, 'its map loaded' if quit_on else (refused[0] if refused else 'no map change (timed out)')))
bad = [l for l in text if re.search(r'Host_Error|SV_Error|ENGINE ERROR|CRASH|TIMEOUT', l)]
check(not bad and len(sections) == 5, '5 gates run, no errors%s' % (': ' + bad[0] if bad else ''))
print('adhub: %d passed, %d failed' % (passed, failed))
sys.exit(1 if failed else 0)
