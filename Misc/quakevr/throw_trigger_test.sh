#!/bin/bash
# throw_trigger_test.sh <agent> [out dir] -- a weapon thrown with the trigger held leaves the hand as one thrown without
# (ROUND21.md, "Throws with the trigger held; wrenched out falls slower"). Prints each run's "thrown: velocity ...
# released ... scale ... of ..." line (developer 1): "released" is how long after the throw's estimate it left the hand
# (the same, a few hundredths, with the trigger held as without), "of" the velocity it would have had thrown.
#   grunt     the grunt's shotgun, its last round fired at the throw's start: the trigger then dry-fires (clicks)
#   nailgun   the nailgun firing through the throw (its fire animation's chain)
# Then a weapon wrenched out by the overhand throw's wrist flick, the grip held (every weapon heavy enough, the same limit
# for all: vr_weight_drop_from 0.1, vr_weight_drop_curve 0, vr_weight_drop_speed 800), at vr_weight_drop_velocity 1 and
# 0.4: its horizontal velocity is that times the "of" (the vertical, less gravity over its age; the spin, that times
# the hand's). (Where in the flick it is wrenched out varies from run to run: the turn is timed in real time.)
AGENT=$1; OUT=${2:-$(mktemp -d)}; HERE=$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd -W 2>/dev/null || pwd)
python - "$HERE" "$OUT" <<'PY'
import sys, importlib.util
spec = importlib.util.spec_from_file_location("tp", sys.argv[1] + "/throw_plays.py"); tp = importlib.util.module_from_spec(spec); spec.loader.exec_module(tp)
def play(name, keys, rel, trigger, grip=True):
    L = [f"0.000 main {keys[0][1]:.4f} {keys[0][2]:.4f} {keys[0][3]:.4f} {keys[0][4] + 70:.2f} 0 0"]
    if trigger:
        L += ["0.000 cmd +attack", "0.000 button main trigger 1"]
    L += [f"{0.3 + k[0]:.4f} main {k[1]:.4f} {k[2]:.4f} {k[3]:.4f} {k[4] + 70:.2f} 0 0" for k in keys]
    if grip:
        L += [f"{0.3 + rel:.4f} cmd -grabright", f"{0.3 + rel:.4f} button main grip 0"]
    open(f"{sys.argv[2]}/{name}.txt", "w", newline="\n").write("\n".join(L) + "\n")
keys, rel = tp.line_throw("flat", 5, 5.0, 0.30)
play("plain", keys, rel, False); play("trigger", keys, rel, True)
keys, rel = tp.arc_throw("overhand", 150, 40, 95, 35, -45, 0.30); play("wrench", keys, rel, False, False)
PY
run() { # <play> <give> <settings>
    local start=$(head -1 "$OUT/$1.txt" | cut -d' ' -f3-)
    bash $KIT/run.sh $AGENT -Script "map vrfiringrange;wait60;god;notarget;developer 1;vr_weapon_grip_mode 0;$3;vr_mock_hand main $start;wait10;impulse 9;wait5;+grabright;vr_mock_button main grip 1;$2;wait40;vr_mock_play $OUT/$1.txt;wait150;toggleconsole;quit" -Filter "^thrown: |wrenched out of" 2>&1 | grep -v "^$\|^exit"
}
echo "== grunt, no trigger"; run plain "impulse 165;wait2;impulse 214" ""
echo "== grunt, trigger held"; run trigger "impulse 165;wait2;impulse 214" ""
echo "== nailgun, no trigger"; run plain "impulse 156" ""
echo "== nailgun, trigger held"; run trigger "impulse 156" ""
W="vr_weight_drop 1;vr_weight_drop_from 0.1;vr_weight_drop_curve 0;vr_weight_drop_speed 800"
echo "== wrenched out, velocity 1"; run wrench "impulse 165" "$W;vr_weight_drop_velocity 1"
echo "== wrenched out, velocity 0.4"; run wrench "impulse 165" "$W;vr_weight_drop_velocity 0.4"
