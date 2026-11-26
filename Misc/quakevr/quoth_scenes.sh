#!/bin/bash
# quoth_scenes.sh <agent> [present|absent] -- the Quoth checklist scenes (quakevr/scenarios/quoth_*.txt) read and run
# headless: vr_scenario validate, then vr_scenario runall on the quoth_ scenes (each started, held 3 s of game time).
# metmon, lpqsp1 and ne_marb are Map Library packages (each scene's `package` line), not in Quoth's folder:
#   present (default): the test base has them installed (qvr_addons/<sha16>/ and cache/maps_installed.txt, as
#                      quoth_setup.py or the Map Library leaves them): every quoth_ scene SCNOK and SCNRUN OK.
#   absent:            none of them installed: those three scenes SCNERR with "is not installed: install it from the Map
#                      Library" (never "no map maps/..."), not started; the kellbase1 ones (Quoth's own pak) still run.
# Quoth itself must be in the base (the kit's links.ps1 mirrors it). Ends on runall's SCNRUNSUM line (the game quits
# itself); prints "quoth_scenes: N passed, M failed" and exits 1 on a failure.
AGENT=${1:?worktree name}
MODE=${2:-present}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
QT_NAME="quoth_scenes ($MODE)"
source "$(dirname "$0")/qvrtest.sh"
PKG="quoth_metmon quoth_lpqsp1 quoth_ne_marb"
OWN="quoth_kellbase_field quoth_kellbase_buttons quoth_kellbase_ladder"
OUT=$(bash $KIT/run.sh $AGENT -ExtraArgs "-loopbackonly" -Games "-game hipnotic -game rogue -game ad -game quakevr" \
    -Script "vr_scenario validate;vr_scenario runall quit $PKG $OWN" -QuitOn "^SCNRUNSUM" -Timeout 600 \
    -Filter "^SCN|not started|vr_scenario: |maps: |rror|CRASH|TIMEOUT|exit=" 2>&1 | grep -v "^$")
echo "$OUT" > $TREE/scratch/quoth_scenes_$MODE.log
has() { echo "$OUT" | grep -q -- "$1" && echo 1 || echo 0; }
hasnt() { echo "$OUT" | grep -q -- "$1" && echo 0 || echo 1; }
for id in $OWN; do
    check $(has "^SCNOK|$id|") "$id read"
    check $(has "^SCNRUN|$id|.*OK") "$id ran"
done
for id in $PKG; do
    check $(hasnt "^SCNERR|$id|.*no map maps/") "$id: no 'no map' error"
    if [ "$MODE" = absent ]; then
        check $(has "^SCNERR|$id|.*is not installed: install it from the Map Library") "$id says which package to install"
        check $(has "vr_scenario: $id not started: .*Map Library package") "$id not started, naming its package"
    else
        check $(has "^SCNOK|$id|") "$id read"
        check $(has "^SCNRUN|$id|.*OK") "$id ran"
        check $(hasnt "vr_scenario: $id not started") "$id started"
    fi
done
check $(has "^SCNRUNSUM") "runall finished (its SCNRUNSUM line)"
check $(hasnt "CRASH\|TIMEOUT\|ENGINE") "no crash or timeout"
echo "$OUT" | grep "^SCNRUNSUM\|not started" | cut -c1-240
verdict
exit $fail
