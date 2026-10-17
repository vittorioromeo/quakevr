#!/bin/bash
# magchain_sim_test.sh <agent> [case...] -- ROUND22.md, "Magnetic holster tether: physics, partial LOD". e1m1, the
# shotgun bound to the right hip holster (slot 3), drawn and let go (it never comes back: vr_magholster_return_time 0,
# _distance 0). Each case is seen with the simulated chain (vr_magholster_chain_sim 1) and then the old one (0), from
# the same camera; vr_magholster_info's "magholster 3 tether:" line counts its pieces through the world (PASS: none
# with the simulation). Cases (none given: all):
#   floor   dropped at your feet, you walk 9 m away: the chain lies along the floor (scratch/magchain_floor.png)
#   toss    tossed far down the corridor (scratch/magchain_toss.png)
#   corner  dropped at the start, you walk north up the corridor and then west into the room past its corner: the chain
#           wraps round it (scratch/magchain_corner.png)
#   lod     Full Chain Up To 2 m at 9 m: links 2 m along it, then the line ("+ line"); 0: all a line; 30: all links
#   time    the profiler, one chain then two (the axe in the left hip holster bound too, both dropped), 9 m off:
#           each chain's sim step (ms, eased) and the magholster chains' system (cpu ms a frame, max, share, gpu, max)
# EXCLUSIVE=1: each run alone (for timings). Prints PASS/FAIL; exits 1 on a failure.
AGENT=$1; shift; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
CASES=${*:-floor toss corner lod time}
P="${PRE}map e1m1;wait60;developer 1;god;notarget;vr_magholster_return_time 0;vr_magholster_return_distance 0;vr_magholster_land_time 0;"
BIND="vr_mock_hand_to off holster 3;wait5;vr_mock_button off stickclick 1;wait3;vr_mock_button off stickclick 0;wait3;"
DRAW="vr_mock_hand_to main holster 3;wait5;+grabright;vr_mock_button main grip 1;wait10;"
DROP="vr_mock_hand_glide 0.4;vr_mock_hand_to main by 20 0 15;wait90;vr_mock_hand_glide 0;-grabright;vr_mock_button main grip 0;wait60;vr_mock_hand main 0.3 1.0 -0.2 0 0 0;"
TOSS="vr_mock_hand_to main by 0 30 10;wait5;-grabright;vr_mock_button main grip 0;wait150;vr_mock_hand main 0.3 1.0 -0.2 0 0 0;"
WALK="vr_mock_stick off 0 1;wait28;vr_mock_stick off 0 0;wait60;"
INFO="vr_magholster_info;"
fail=0
pair() { # $1 camera: the simulated chain's info and shot, then the old chain's
    # (Lit flat and from one eye, full size: the thin chain seen.)
    echo "${INFO}r_fullbright 1;vr_eyeshot 1;$1screenshot;vr_mock_camera;vr_magholster_chain_sim 0;wait60;${INFO}$1screenshot;vr_mock_camera;r_fullbright 0;"
}
crossing() { # the "tether:" lines' pieces through the world: simulated, old
    echo "$1" | grep "^magholster 3 tether:" | sed 's/.*nodes, \([0-9]*\) pieces.*/\1/' | tr '\n' ' '
}
for c in $CASES; do
    case $c in
        floor) CAM="vr_mock_camera 0.5 0.9 -0.2 12 180;wait3;"; S="$P$BIND$DRAW$DROP$WALK$(pair "$CAM")" ;;
        toss) CAM="vr_mock_camera 0.3 2.6 0.3 38 0;wait3;"; S="$P$BIND$DRAW$TOSS$(pair "$CAM")" ;;
        corner) CAM="vr_mock_camera 0.5 2.2 0.5 35 160;wait3;"
            S="$P$BIND$DRAW${DROP}vr_mock_walk_to 480 150;wait240;vr_mock_walk_to 270 150;wait150;vr_mock_walk_to off;wait30;$(pair "$CAM")" ;;
        lod) SPLIT="vr_magholster_chain_lod 0.6;wait3;r_fullbright 1;vr_mock_camera 0.45 1.3 0.9 28 0;wait3;screenshot;vr_mock_camera;r_fullbright 0;"
            S="$P$BIND$DRAW$DROP${WALK}${SPLIT}vr_magholster_chain_lod 2;wait3;${INFO}vr_magholster_chain_lod 0;wait3;${INFO}vr_magholster_chain_lod 30;wait3;$INFO" ;;
        time) BIND2="vr_mock_hand_to off holster 2;wait5;vr_mock_button off stickclick 1;wait3;vr_mock_button off stickclick 0;wait3;"
            DRAW2="vr_mock_hand_to off holster 2;wait5;+graboff;vr_mock_button off grip 1;wait10;vr_mock_hand_glide 0.4;vr_mock_hand_to off by 20 0 15;wait90;vr_mock_hand_glide 0;-graboff;vr_mock_button off grip 0;wait60;vr_mock_hand off -0.3 1.0 -0.2 0 0 0;"
            PROF="vr_profile_csv 1;wait400;vr_profile_report 4;wait3;$INFO"
            S="$P$BIND$DRAW$DROP$WALK$PROF$BIND2$DRAW2${WALK}$PROF" ;;
    esac
    OUT=$(bash $KIT/run.sh ${EXCLUSIVE:+--exclusive} $AGENT -Clean -Out magchain_$c.png -Script "${S}toggleconsole;quit" -Filter "^magholster [23]|tether|magholster chains|vr_mock_walk_to:")
    echo "$OUT" | grep -v "^$" | cut -c1-200 > $KIT/../qvr-agents/$AGENT/scratch/magchain_$c.txt
    case $c in
        floor|toss|corner) X=($(crossing "$OUT"))
            if [ "${X[0]}" = "0" ]; then r=PASS; else r=FAIL; fail=1; fi
            echo "$r $c: pieces through the world: simulated ${X[0]:-?}, old ${X[1]:-?}" ;;
        lod) L=$(echo "$OUT" | grep -v "^$" | grep -v "^exit=" | awk '/^magholster 3:/{if(l)print l; l=$0; next}{l=l" "$0}END{print l}')
            echo "$L" | sed -n 1p | grep -q "tether chain [1-9][0-9]* links .* + line [0-9]" && r=PASS || { r=FAIL; fail=1; }
            echo "$r lod 2 m: links then a line: $(echo "$L" | sed -n 1p | grep -o 'tether [a-z]* [0-9]* links [0-9.]* m[ +a-z0-9.]*')"
            echo "$L" | sed -n 2p | grep -q "tether line 0 links" && r=PASS || { r=FAIL; fail=1; }
            echo "$r lod 0: all a line: $(echo "$L" | sed -n 2p | grep -o 'tether [a-z]* [0-9]* links [0-9.]* m[ +a-z0-9.]*')"
            echo "$L" | sed -n 3p | grep "tether chain [1-9][0-9]* links" | grep -qv "+ line" && r=PASS || { r=FAIL; fail=1; }
            echo "$r lod 30: all links: $(echo "$L" | sed -n 3p | grep -o 'tether [a-z]* [0-9]* links [0-9.]* m[ +a-z0-9.]*')" ;;
        time) echo "$OUT" | grep "tether:\|magholster chains" | cut -c1-160 ;;
    esac
done
exit $fail
