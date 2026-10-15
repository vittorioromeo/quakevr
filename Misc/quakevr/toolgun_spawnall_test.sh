#!/bin/bash
# toolgun_spawnall_test.sh <agent> [<first> [<last>]] [-- <run.sh options>] -- every entry of the toolgun's spawn panel
# made once, headless (docs/vr-port/ROUND22.md, "Every expansion's entities in the toolgun's spawn panel"): vrfiringrange,
# god and notarget; eight entries at a time on a ring around the player (vr_toolgun_spawn_test <first> <last>), a
# moment later each checked still there and removed (vr_toolgun_spawn_test check). Prints each entry's result
# (spawned and alive / spawned but gone / failed / unavailable) and the totals.
# PASS: no entry failed or gone (an unavailable one: its pack's data or its rules are not here), no crash.
AGENT=${1:-spawnall}
FIRST=${2:-0}
LAST=${3:-199}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
# (MAPCMD="vr_campaign_native mg3;map map1": in Dawn of the Machine's own campaign, for its upgrades and gas flare)
MAPCMD=${MAPCMD:-map vrfiringrange}
S=$(FIRST=$FIRST LAST=$LAST MAPCMD="$MAPCMD" python -c "
import os
first, last = int(os.environ['FIRST']), int(os.environ['LAST'])
s = [os.environ['MAPCMD'] + ';wait60;god;notarget;vr_toolgun_spawnlist;wait5']
for a in range(first, last + 1, 8):
    s.append(f'vr_toolgun_spawn_test {a} {min(a + 7, last)};wait40;vr_toolgun_spawn_test check;wait10')
s.append('echo SPAWNALLEND;wait5;toggleconsole;quit')
print(';'.join(s))
")
out=$(bash $KIT/run.sh $AGENT -Script "$S" -Timeout 900 -Filter 'spawntest|toolgun spawns|SPAWNALLEND|ENGINE|CRASH|Host_Error|exit=|TIMEOUT|unavailable .* entity' 2>&1)
[ -n "$VERBOSE" ] && echo "$out"
echo "$out" | python -c "
import sys, re
t = sys.stdin.read()
rows = {}
for m in re.finditer(r'spawntest: (\d+) (\w+) \"([^\"]+)\" (\S+): (spawned \d+|failed|unavailable)', t):
    rows[int(m.group(1))] = [m.group(2), m.group(3), m.group(4), m.group(5).split()[0], '']
for m in re.finditer(r'spawntest: (\d+) \"[^\"]+\" (alive|gone) (.*)', t):
    n = int(m.group(1))
    if n in rows: rows[n][4] = m.group(2) + ('' if m.group(2) == 'alive' else ' ' + m.group(3).strip())
fails = []
for n in sorted(rows):
    sec, label, cls, res, chk = rows[n]
    r = res if res != 'spawned' else ('spawned, ' + (chk or 'unchecked'))
    print(f'{n:3d} {sec:8s} {label:32s} {cls:30s} {r}')
    if res == 'failed' or (res == 'spawned' and chk != 'alive'): fails.append(label)
cnt = {k: sum(1 for r in rows.values() if r[3] == k) for k in ('spawned', 'failed', 'unavailable')}
print('totals', cnt, 'entries', len(rows))
if 'SPAWNALLEND' not in t: fails.append('did not finish')
if re.search(r'ENGINE CRASH|ENGINE ERROR|Host_Error|TIMEOUT|exit=[1-9]', t): fails.append('crash')
print('FAIL: ' + '; '.join(fails) if fails else 'PASS')
"
