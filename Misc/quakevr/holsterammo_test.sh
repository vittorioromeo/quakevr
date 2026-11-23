#!/bin/bash
# holsterammo_test.sh <agent> [flat] -- a holstered gun keeps its loaded rounds, every way into a holster and out (QC
# vr_holsterammo_test.qc, `vr_reload_test 49; impulse 125`; docs/vr-port/ROUND25.md, "holsterammo"). For reloading modes
# 1, 2 and 3: a nailgun with 17 rounds let go at a holster and drawn there, 5 and 2, the weapon wheel's EMPTY HAND and
# slot, the wheel's trade with a holstered shotgun, the holster menu's swap: the same rounds, its magazine in, the reserve
# untouched; the holsters' reload conserving clip + reserve; Quick Slots leaving the reserve alone. One frame, fast mode;
# the run ends on the verdict line. "flat": the same in flat-screen play (-flatscreenmode, the holsters on).
# (The wheel's own button, B/Y, being the magazine's eject: weaponwheel_test.sh.)
AGENT=${1:?agent}
MODE=${2:-vr}
KIT=C:/OHWorkspace/qvr-kit
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/holsterammo
mkdir -p "$S"
EXTRA=""
if [ "$MODE" = "flat" ]; then
    EXTRA="-ExtraArgs -flatscreenmode"
fi
SCRIPT="vr_flat_holsters 1; map vrtesthall; wait30; god; notarget; vr_reload_test 49; impulse 125"
bash $KIT/run.sh $AGENT $EXTRA -Script "$SCRIPT" -QuitOn "^holsterammo_test: [0-9]+ passed" -Timeout 120 \
    -Filter "^holsterammo_test" > "$S/run_$MODE.log" 2>&1
grep -E "^holsterammo_test" "$S/run_$MODE.log" | tail -30
