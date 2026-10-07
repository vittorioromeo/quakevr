#!/bin/bash
# slipgate_edges_test.sh <agent> [clip|push|held|cross|grab|cull|particles|head|recursion|all]: headless checks of the slipgate edge cases in
# vrslipgates (ROUND21.md, "Slipgates: exits on their gates, props through, held objects, the force grab's beam"), with
# the agent kit (C:/OHWorkspace/qvr-kit). Each section prints one line or a few, with what it must say.
#   clip   a crate resting where a gate's exit used to stand (48 out of the north gallery's wall), paired exits off and
#          on: never drawn cut (0 frames); one straddling the flush player gate: cut (a positive control)
#   push   a small crate slid at 250 u/s, and kept sliding at 60 u/s (a steady push), into the 8-deep flush player gate;
#          the big crate at 60 u/s through the player, 48-deep large and wide gates: each ends in the north room (y > 928)
#   held   a shells box held 16 units ahead, walked into the flush player gate: its middle (in the room it is in) frame
#          by frame, the largest step (a few units: the walk's frame pacing) and frames drawn whole inside the wall (0)
#   cross  the player walked into the flush player gate: where he is carried from and to (640 -> 928: its face onto
#          the other gate's face) and his largest step a tick across the crossing
#   grab   a health box in the north gallery aimed at from the south one through the flush large gate: the force
#          grab's beam ends at the box's image (y 712), not at the box (y 1000)
#   cull   a full-size shells box half through T's north gate, seen from U with the entrance behind the camera: the
#          pixels its half out of U's gate makes (thousands; 0 when brush props were culled by their own place)
#   head   facing T's west gate (the loop: it shows your back; 2048-pixel eyes): the left eye's pixels your head makes (in a box round it), drawn
#          (vr_slipgate_self_head 1) against not (0): through the gate (hundreds), and with the views off (0: the eye's
#          own view never draws it)
AGENT=${1:?agent}; WHAT=${2:-all}
KIT=C:/OHWorkspace/qvr-kit
HERE=$(cd "$(dirname "$0")" && pwd)
TREE=$(cd "$HERE/../../.." && pwd -W)
LOG=$KIT/bases/$AGENT/qbase/qconsole.log
PY=python; for p in /c/Python313/python python3; do "$p" -c "import PIL" 2>/dev/null && PY=$p && break; done
mkdir -p "$TREE/scratch"
run() { bash $KIT/run.sh $AGENT "$@" > /dev/null; }
START="developer 1;map vrslipgates;wait60;god;notarget"
want() { [ "$WHAT" = "$1" ] || [ "$WHAT" = all ]; }

# The last place of entity 221 (the first thing spawned) in vr_portals_debug_split's lines: its middle's y.
lastY() { grep -E "^portal split: frame [0-9]+ ent 221 " "$LOG" | tail -1 | awk '{for (i = 1; i < NF; i++) if ($i == "middle") print $(i+2)}'; }

if want clip; then
    for pe in 0 1; do
        run -Script "$START;vr_slipgate_pair_exits $pe;setpos 352 1032 24 0 270 0;wait5;noclip 0;vr_test_spawn 107;vr_test_spawn_dist 56;impulse 241;wait60;vr_portals_debug_split 1;wait10;vr_portals_debug_split 0;toggleconsole;quit"
        echo "clip: pair exits $pe: a crate resting 48 out of a gate drawn cut in $(grep -c 'ent 221 .*cut by the plane' "$LOG") frames (0)"
    done
    run -Script "$START;setpos -256 600 24 0 90 0;wait5;noclip 0;vr_test_spawn 107;vr_test_spawn_dist 40;impulse 241;wait60;vr_portals_debug_split 1;wait10;vr_portals_debug_split 0;toggleconsole;quit"
    echo "clip: a crate straddling the flush player gate drawn cut in $(grep -c 'ent 221 .*cut by the plane' "$LOG") frames (every frame: over 0)"
fi

if want push; then
    slide() { # slide <spawn> <x> <speed> <flings>: from 64 units in front of FA's gate at x, kept at speed
        local S="$START;setpos $2 536 24 0 90 0;wait5;noclip 0;vr_test_spawn $1;vr_test_spawn_dist 40;impulse 241;wait30;setpos $2 336 24 0 90 0;wait5;vr_portals_debug_split 221"
        for i in $(seq 1 $4); do S="$S;vr_physics_fling 221 $3 90;wait3"; done
        run -Script "$S;wait60;toggleconsole;quit"
        echo "push: $( [ $1 = 107 ] && echo small || echo big ) crate at $3 u/s ($4 pushes) into the gate at x $2: ends at y $(lastY) (> 928: through)"
    }
    slide 107 -256 250 1; slide 107 -256 60 60; slide 108 -256 60 60; slide 108 0 60 60; slide 108 352 60 60
fi

if want held; then
    S="$START;setpos -256 560 24 0 90 0;wait5;noclip 0;vr_mock_hand main 0.15 1.25 -0.40 0 0 0;wait20;+grabright;vr_mock_button main grip 1;wait5;vr_test_spawn 101;vr_test_spawn_hold 1;impulse 241;wait30;vr_portals_debug_split -1;vr_mock_stick off 0 0.5"
    for i in $(seq 1 50); do S="$S;wait2"; done
    run -Script "$S;toggleconsole;quit"
    "$PY" - "$LOG" <<'PYEOF'
import re, sys
pts = []
for l in open(sys.argv[1], errors='replace'):
    m = re.match(r'portal split: frame (\d+) ent 221 \S* ?at (\S+) (\S+) (\S+) (.*) middle (\S+) (\S+) (\S+)', l)
    if m: pts.append((int(m.group(1)), [float(v) for v in m.group(2, 3, 4)], m.group(5).startswith('whole'), [float(v) for v in m.group(6, 7, 8)]))
d = lambda a, b: sum((x - y) ** 2 for x, y in zip(a, b)) ** 0.5
# each step: the smaller of the drawn place's (jumps when the player is carried) and the middle's in its room (jumps
# when the middle crosses a plane); a hitch moves both
steps = [(b[0], min(d(a[1], b[1]), d(a[3], b[3]))) for a, b in zip(pts, pts[1:]) if b[0] == a[0] + 1]
inwall = sum(1 for p in pts if p[2] and 640 < p[3][1] < 928)
big = max(steps, key=lambda s: s[1]) if steps else (0, 0)
print(f"held: {len(pts)} frames, the largest step {big[1]:.1f} units (frame {big[0]}), drawn whole inside the wall {inwall} (0), ends at y {pts[-1][3][1] if pts else 0:.0f} (> 928)")
PYEOF
fi

if want cross; then
    S="$START;setpos -256 576 24 0 90 0;wait5;noclip 0;wait5;vr_mock_stick off 0 1"
    for i in $(seq 1 30); do S="$S;wait1;viewpos"; done
    run -Script "$S;toggleconsole;quit"
    from=$(grep -o "carried edict 1 through side [0-9]*: .*" "$LOG" | head -1 | cut -d: -f2)
    # (the steps of the three ticks either side of the crossing: elsewhere a headless run's uncapped frames now and
    # then make a tick's step several times a walk's)
    step=$(awk -F'[()]' '/^Player pos:/ {split($2, p, " "); y[n++] = p[2]} END {for (i = 1; i < n; i++) if (y[i] - y[i-1] > 200) c = i
        for (i = c - 3; i <= c + 3; i++) if (i > 0 && i < n) {dy = y[i] - y[i-1]; if (dy > 200) dy -= 288; if (dy < 0) dy = -dy; if (dy > m) m = dy}; print m + 0}' "$LOG")
    echo "cross: carried$from (-256 640 24 -> -256 928 24, a few units on: the face onto the other gate's face); the largest step of the ticks round it, less the gates' 288: $step units (a walk's 4-6)"
fi

if want grab; then
    run -Script "$START;setpos 0 940 24 0 90 0;wait5;noclip 0;vr_test_spawn 100;vr_test_spawn_dist 60;impulse 241;wait30;setpos 0 590 24 0 90 0;wait5;noclip 0;vr_mock_hand main 0.000 1.300 -0.450 56 0 0;wait10;vr_portals_debug_split 1;+attack;wait3;vr_portals_debug_split 0;-attack;toggleconsole;quit"
    echo "grab: $(grep 'force grab beam' "$LOG" | head -1) (to y 712, its image; its middle y 1000)"
    # ... and pulled through (ragdoll_test.sh's take: the trigger locks, a flick up pulls): it crosses and is caught
    G=$KIT/bases/$AGENT/qbase/id1/slipgate_pull.txt
    printf "%s\n" "0.000 main 0.000 1.300 -0.450 56 0 0" "0.500 cmd +attack" "1.000 main 0.000 1.300 -0.450 56 0 0" \
        "1.100 main 0.000 1.600 -0.450 56 0 0" "1.150 cmd +grabright" "1.150 button main grip 1" "1.200 cmd -attack" > $G
    run -Script "$START;setpos 0 940 24 0 90 0;wait5;noclip 0;vr_test_spawn 100;vr_test_spawn_dist 60;impulse 241;wait30;setpos 0 590 24 0 90 0;wait5;noclip 0;wait10;vr_mock_play $G;wait300;toggleconsole;quit"
    echo "grab: pulled through: $(grep -c 'force grab: crossed slipgate' "$LOG") crossing (1), $(grep -c 'force grab: caught' "$LOG") caught (1)"
fi

if want cull; then
    for sp in 1 0; do
        SPAWN="vr_test_spawn 101;vr_test_spawn_dist 76;impulse 241"; [ $sp = 0 ] && SPAWN="wait1"
        bash $KIT/run.sh $AGENT -Clean -Out "slipgate_cull_$sp.png" -Script "vr_forcegrabbable_box_scale 1;map vrslipgates;wait60;god;notarget;setpos -1280 690 24 0 90 0;wait5;noclip 0;$SPAWN;wait60;setpos -1040 1408 24 0 0 0;wait5;noclip 0;vr_mirror 2;vr_window_view 0;vr_mock_look 10 0;wait20;screenshot;wait5;toggleconsole;quit" > /dev/null
    done
    "$PY" - "$TREE/scratch" <<'PYEOF'
import sys
from PIL import Image, ImageChops
a = Image.open(sys.argv[1] + '/slipgate_cull_1.png').convert('RGB'); b = Image.open(sys.argv[1] + '/slipgate_cull_0.png').convert('RGB')
d = ImageChops.difference(a, b).convert('L').point(lambda v: 255 if v > 24 else 0)
print(f"cull: the box's half out of the exit makes {d.histogram()[255]} pixels (thousands; 0: culled)")
PYEOF
fi

# particles: explosion particles behind start's underwater slipgate (a free-standing sheet), seen from in front of it
# with the author's translucent gate surface (vr_slipgate_surface_opacity 0.3: no depth written): the pixels they make
# over the gate (a few at most; 539 before the particles were hidden behind shown gates), and with the views off (the
# shimmer only: they show through it, a control)
if want particles; then
    for c in "1 0.3" "0 1"; do
        set -- $c
        for sp in 1 0; do
            P="vr_particle_test 2 400"; [ $sp = 0 ] && P="wait1"
            bash $KIT/run.sh $AGENT -Clean -Out "slipgate_particles_$sp.png" -Script "map start;wait90;god;notarget;noclip;vr_portals $1;vr_slipgate_surface_opacity $2;setpos 1040 1700 -330 0 270 0;wait5;$P;setpos 1040 1830 -330 0 270 0;vr_mock_look 0 270;vr_mirror 2;vr_window_view 0;wait3;screenshot;wait5;toggleconsole;quit" > /dev/null
        done
        "$PY" - "$TREE/scratch" "$1" <<'PYEOF'
import sys
from PIL import Image, ImageChops
a = Image.open(sys.argv[1] + '/slipgate_particles_1.png').convert('RGB'); b = Image.open(sys.argv[1] + '/slipgate_particles_0.png').convert('RGB')
d = ImageChops.difference(a, b).convert('L').point(lambda v: 255 if v > 30 else 0)
print(f"particles: views {sys.argv[2]}: particles behind the gate make {d.histogram()[255]} pixels" + (" (a few at most)" if sys.argv[2] == '1' else " (a control: over 100)"))
PYEOF
    done
fi

# The left eye's image (vr_eyeshot) of a run, copied to scratch/<name>.png.
eyeshot() { local out=$1; shift; rm_old=$TREE/quakevr/eyeshots/vrslipgates_000_L.png; : > "$rm_old"
    run -Script "$*;vr_eyeshot 1;wait5;toggleconsole;quit"; cp "$rm_old" "$TREE/scratch/$out.png"; }
# Pixels over a threshold of difference between two of those images.
# (A box "x0,y0,x1,y1": only there; the runs' own noise, the lights' flicker, lies elsewhere.)
differ() { "$PY" - "$TREE/scratch/$1.png" "$TREE/scratch/$2.png" "${3:-24}" "${4:-}" <<'PYEOF'
import sys
from PIL import Image, ImageChops
a = Image.open(sys.argv[1]).convert('RGB'); b = Image.open(sys.argv[2]).convert('RGB'); t = int(sys.argv[3])
d = ImageChops.difference(a, b).convert('L')
if sys.argv[4]: d = d.crop(tuple(int(v) for v in sys.argv[4].split(',')))
print(d.point(lambda v: 255 if v > t else 0).histogram()[255])
PYEOF
}

if want head; then
    for v in 1 0; do
        for h in 1 0; do
            eyeshot "slipgate_head_${v}_$h" "vr_mock_eye_size 2048;$START;vr_portals $v;vr_slipgate_self_head $h;setpos -1560 560 24 0 180 0;wait5;noclip 0;wait20"
        done
        echo "head: views $v: your head makes $(differ slipgate_head_${v}_1 slipgate_head_${v}_0 6 1000,995,1045,1045) pixels in the left eye"             "$( [ $v = 1 ] && echo '(through the loop gate: over 30)' || echo '(no gate view: 0)')"
    done
fi

# recursion: facing T's west gate 40 units out (4096-pixel eyes): the loop's green signs stacked in the middle of the
# left eye, one more each gate deeper (vr_portals_recursion 0, 1, 2: 1, 2, 3 signs), and the views drawn at each depth
if want recursion; then
    for r in 0 1 2; do
        eyeshot "slipgate_recursion_$r" "vr_mock_eye_size 4096;$START;vr_portals_recursion $r;setpos -1560 560 24 0 180 0;wait5;noclip 0;wait20;vr_portals_view;wait2"
        echo "recursion $r: $("$PY" "$HERE/slipgate_signs.py" "$TREE/scratch/slipgate_recursion_$r.png" | awk '{print $2}') signs ($((r + 1)));" \
            "$(grep -E '^  (last camera|within the last view)' "$LOG" | sed -E 's/^ +//; s/ rendered \(limit [0-9]+\)//; s/, the first through side -?[0-9]+//' | tr '\n' ';' | sed 's/;$//')"
    done
fi
