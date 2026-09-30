#!/bin/bash
# throw_grace_test.sh <agent> [out dir] -- the throw grace's checks (ROUND21.md, "Throws leave the hand clean"), each with
# the grace off (vr_box3d_throw_grace 0: the throw only watched) and on (0.2 s); prints the "box3d: ... thrown by" and
# "throw grace over / watched" lines (the velocity and spin changed by the time the grace ends, gravity taken out):
#   weapon  the axe thrown flat forward from the main hand past the open off hand held in its path
#   prop    a health box thrown overarm, let go of early (the hand still speeding up: it catches the box)
#   snap    a health box thrown with a wrist snap overhead (down across the body: vr_box3d_throw_grace_body)
#   palm    a gib let go of still on the palm-up off hand: a slow release, it rests on the palm (z about 77.6)
# vr_throw_spin_drag 0 in every run, so that the spin keeps what it left with.
AGENT=$1; OUT=${2:-$(mktemp -d)}; HERE=$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd -W 2>/dev/null || pwd)
python - "$HERE" "$OUT" <<'PY'
import sys, importlib.util
spec = importlib.util.spec_from_file_location("tp", sys.argv[1] + "/throw_plays.py"); tp = importlib.util.module_from_spec(spec); spec.loader.exec_module(tp)
def play(name, keys, rel):
    L = [f"0.000 main {keys[0][1]:.4f} {keys[0][2]:.4f} {keys[0][3]:.4f} {keys[0][4] + 70:.2f} 0 0"]
    L += [f"{0.3 + k[0]:.4f} main {k[1]:.4f} {k[2]:.4f} {k[3]:.4f} {k[4] + 70:.2f} 0 0" for k in keys]
    L += [f"{0.3 + rel:.4f} cmd -grabright", f"{0.3 + rel:.4f} button main grip 0"]
    open(f"{sys.argv[2]}/{name}.txt", "w", newline="\n").write("\n".join(L) + "\n")
keys, rel = tp.line_throw("flat", 5, 5.0, 0.30); play("flat", keys, rel)
keys, rel = tp.arc_throw("overhand", 150, 40, 95, 35, -45, 0.30); play("early", keys, 0.3 * keys[-1][0])
keys, rel = tp.arc_throw("snap", 100, 90, 95, 40, -60, 0.25); play("snap", keys, rel)
PY
run() { # <play> <grace> <off hand pose> <what to hold>
    local start=$(head -1 "$OUT/$1.txt" | cut -d' ' -f3-)
    bash $KIT/run.sh $AGENT -Script "map vrfiringrange;wait60;god;notarget;vr_debug_box3d 1;vr_box3d_throw_grace $2;vr_weapon_grip_mode 0;vr_throw_spin_drag 0;vr_mock_hand off $3;vr_mock_hand main $start;wait10;impulse 9;wait5;+grabright;vr_mock_button main grip 1;$4;wait40;vr_mock_play $OUT/$1.txt;wait150;toggleconsole;quit" -Filter "thrown by|throw (grace|watched)" 2>&1 | grep -v "^$\|^exit"
}
for g in 0 0.2; do
    echo "== weapon, grace $g"; run flat $g "0.2 1.25 -1.0 0 0 0" "impulse 152"
    echo "== prop, grace $g"; run early $g "-0.35 1.1 -0.2 0 0 0" "wait5;vr_rigid_place item_health main 0 3 0"
    echo "== snap, grace $g"; run snap $g "-0.35 1.1 -0.2 0 0 0" "wait5;vr_rigid_place item_health main 0 3 0"
done
echo "== palm (still release)"
bash $KIT/run.sh $AGENT -Script "vr_debug_box3d 1;map vrfiringrange;wait60;vr_mock_hand main 0.35 1.1 -0.2 0 0 0;vr_mock_hand off -0.1 1.2 -0.45 0 -13 90;wait20;+graboff;vr_mock_button off grip 1;impulse 252;wait30;-graboff;vr_mock_button off grip 0;wait320;vr_physics_list props;toggleconsole;quit" -Filter "thrown by|^  [0-9]+  " 2>&1 | grep -v "^$\|^exit"
