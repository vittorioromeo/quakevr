#!/bin/bash
# hz_reach.sh <agent> -- the melee hitbox size slider (vr_hit_melee_scale, Combat > Hit Detection > Melee Hitbox
# Size): the distance at which a melee blow still connects. The player stands `d` units from the training dummy and
# punches straight at it (the same motion in every case); prints, per scale, the largest `d` that still landed. The
# engine's melee probe is vr_hit_tolerance_melee (6) * this, so the expected shift is 6*(scale-1): -4.5 at 0.25,
# +18 at 4. The striking points' own 3-unit thickness and the broad-phase box are not scaled, so the shift is not
# proportional to the scale.
AGENT=${1:-hitzones}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
DX=192.3; DY=-656.7
P="map vrfiringrange;wait60;god;notarget;developer 1;vr_melee_positional 0;vr_hit_precise 1;setpos 219 $DY 41 0 180 0;wait40;+grabright;vr_mock_fingers main 1 1;vr_mock_button main grip 1;wait40;"
for s in 0.25 1 4; do
    for d in 48 52 56 60 64 68 72 76 80 84 88 92 96; do
        X=$(python -c "print(round($DX+$d,1))")
        P="$P echo SW scale $s dist $d;vr_hit_melee_scale $s;setpos $X $DY 41 0 180 0;wait12;"
        P="$P vr_mock_hand_to main $(python -c "print(round($X-24,1))") $DY 55;wait6;"
        for k in 30 36 42 48 54; do
            P="$P vr_mock_hand_to main $(python -c "print(round($X-$k,1))") $DY 55;wait2;"
        done
        P="$P wait20;"
    done
done
P="$P vr_hit_melee_scale 1;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Timeout 900 -Script "$P" -Filter "SW scale|Dummy: " 2>&1 | grep "SW scale\|Dummy: " > Misc/quakevr/scratch/hz_reach.txt
python - <<'PY'
last = {}
for l in open("Misc/quakevr/scratch/hz_reach.txt"):
    if l.startswith("SW scale"):
        p = l.split(); last[(p[2], int(p[4]))] = False
    elif "Dummy: " in l and last:
        last[list(last)[-1]] = True
for s in ("0.25", "1", "4"):
    hit = sorted(d for (ss, d), ok in last.items() if ss == s and ok)
    print(f"scale {s}: landed up to {max(hit) if hit else 'none'} units from the dummy (of {sorted(d for (ss,d) in last if ss==s)})")
PY
