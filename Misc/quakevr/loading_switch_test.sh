#!/bin/bash
# loading_switch_test.sh <agent> -- the "Loading..." notice across campaign switches (ROUND21.md, "The Loading... crash,
# root cause"), headless with sound on: vr_loading_notice 2 (the mock headset shows it too) and vr_loading_hold 0.5 (many
# frames between the switch and its map), rockets fired before each switch (particles, lights, decals, Steam Audio's
# voices playing when the switch stops the sounds). Two runs:
#   select  every campaign selected in turn (vr_campaign_select: the menus'), then the hub from a pack (vr_campaign_hub:
#           the main menu's VR Hub);
#   portals the hub's teleporter to each campaign (its lectern's choice, vr_activestartpaknameidx, then the teleporter's
#           changelevel start) and the way back (changelevel vrstart: the campaigns' hub portal).
# PASS: every level change put off ("Loading...: ... put off") and then spawned in order, no engine error or crash,
# exit 0. Before the fix the first switch ended in Sys_Error "Mod_PointInLeaf: bad model" (VR_SndListener's outOfSolid
# on the world model the switch had reset).
AGENT=${1:-fix101}; shift
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
FIRE="god;give all;impulse 7;+attack;wait40;-attack"
PRE="developer 1;vr_loading_notice 2;vr_loading_hold 0.5;map vrstart;wait60;$FIRE"

check() # <name> <expected maps> <run.sh output>
{
    echo "$3" | grep -v "^  0000" | grep -v "SpawnServer\|put off" | head -30
    echo "$3" | NAME="$1" EXPECT="$2" python -c "
import os, re, sys
t = sys.stdin.read()
expect = os.environ['EXPECT'].split()
lines = [l.strip() for l in t.splitlines()]
puts, spawned = [], []
for i, l in enumerate(lines):
    m = re.search(r'\"(?:map|changelevel) (\S+)\" put off', l)
    if m:
        puts.append(m.group(1))
        nxt = [s for s in lines[i + 1:] if s.startswith('SpawnServer:')]
        spawned.append(nxt[0].split()[1] if nxt else '-')
bad = [l for l in lines if 'ENGINE' in l or 'bad model' in l]
ok = puts == expect and spawned == expect and not bad and 'exit=0' in t
print(os.environ['NAME'] + ': put off', ' '.join(puts) + '; spawned', ' '.join(spawned))
print('  ' + ('PASS' if ok else 'FAIL' + (': ' + bad[0] if bad else ' (expected ' + ' '.join(expect) + ')')))
sys.exit(0 if ok else 1)
"
}

S="$PRE"
EXPECT=""
for c in hipnotic:start rogue:start dopa:e5start mg1:start mg3:start id1:start hipnotic:start; do
    S="$S;vr_campaign_select ${c%%:*};wait150;$FIRE"
    EXPECT="$EXPECT ${c##*:}"
done
S="$S;vr_campaign_hub;wait150;$FIRE;wait20;toggleconsole;quit"
EXPECT="$EXPECT vrstart"
out=$(bash $KIT/run.sh $AGENT -Sound -Timeout 400 -Script "$S" -Filter "put off|SpawnServer|ENGINE|bad model" "$@")
check select "$EXPECT" "$out"; r1=$?

S="$PRE"
EXPECT=""
for c in 3:e5start 4:start 5:start 1:start 2:start 0:start; do
    S="$S;vr_activestartpaknameidx ${c%%:*};changelevel start;wait150;$FIRE;changelevel vrstart;wait150;$FIRE"
    EXPECT="$EXPECT ${c##*:} vrstart"
done
S="$S;wait20;toggleconsole;quit"
out=$(bash $KIT/run.sh $AGENT -Sound -Timeout 400 -Script "$S" -Filter "put off|SpawnServer|ENGINE|bad model" "$@")
check portals "$EXPECT" "$out"; r2=$?
[ $r1 -eq 0 ] && [ $r2 -eq 0 ] && echo PASS || { echo FAIL; exit 1; }
