#!/bin/bash
# flathud_test.sh <agent> -- flatscreen play's status bar at hudstyle 1 (-flatscreenmode; docs/vr-port/FLATPLAY.md,
# "The status bar"), headless. Prints PASS/FAIL and a verdict line, exits 1 on a failure.
#   counts  each hand's counter (vr_flathud_report: what the bar draws) is the hand's own: the reserve of its gun's ammo
#           (the shotgun's shells, the revolver's, the nail gun's nails), the clip of a gun whose clip is its only ammo
#           (the grunt's shotgun: a magazine, drained shot by shot), and both hands at once when dual wielding. And the
#           status bar's STAT_AMMO is the main hand's count -- it was the VR progs' ammo *type* (1 with shells, 2 with
#           nails: SV_WriteClientdataToMessage wrote .currentammo; VR_AmmoStat now).
#   pixels  screenshots at hudstyle 1, viewsize 100, scr_sbarscale 2, read by flathud_check.py against the same frame
#           with no status bar (viewsize 120): the portrait and the health in the middle, bullet time's bar over them
#           (cyan ready, yellow running), each hand's counter at its own bottom corner (the left one gone once the left
#           hand is empty), and vr_flat_hud 0 putting Ironwail's own layout back (the portrait at the bottom left).
AGENT=${1:-fixgame3}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"
HERE=$(cd "$(dirname "$0")" && pwd -W)
P="map e1m1; wait60; developer 1; god; notarget; hudstyle 1; vr_flat_visor 0" # (the status bar: the visor is the default HUD, visor_test.sh)
run() { local tag=$1 script=$2 filter=$3; shift 3
        bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Low "$@" -Script "$script; wait5; tune_reset; toggleconsole; quit" -Filter "$filter" > "$S/$tag.log" 2>&1; }
hand() { grep -a "^flathud: $2 hand:" "$S/$1.log" | sed -n "$3p" | sed 's/.*weapon \([0-9]*\) shown \([01]\) count \([0-9-]*\).*/\1 \2 \3/'; }
pool() { grep -a "^flathud: pools" "$S/$1.log" | sed -n "$2p" | sed 's/.*shells \([0-9]*\) nails \([0-9]*\).*STAT_AMMO \([0-9-]*\);.*/\1 \2 \3/'; }

# 1. counts: the shotgun, the revolver, the grunt's shotgun (clip only), then the nail gun in the left hand with them.
R="vr_flathud_report; wait2"
# (vr_flat_reload 0: the counters with no clips, every shot off the reserve; the clips' own numbers: flatreload_test.sh.)
run counts "$P; vr_flat_reload 0; impulse 154; wait10; $R; +attack; wait30; -attack; wait10; $R; vr_test_give_wid 31; impulse 149; wait10; +attack; wait30; -attack; wait10; $R; impulse 165; wait10; +attack; wait30; -attack; wait10; $R; give n 50; impulse 176; wait10; $R" "^flathud:"
s1=$(hand counts right 1); s2=$(hand counts right 2); p1=$(pool counts 1); p2=$(pool counts 2)
check $([ "${s1%% *}" = 4 ] && [ "$(echo $s1 | cut -d' ' -f3)" = "$(echo $p1 | cut -d' ' -f1)" ] && [ "$(echo $s2 | cut -d' ' -f3)" = "$(echo $p2 | cut -d' ' -f1)" ] \
        && [ "$(echo $s2 | cut -d' ' -f3)" -lt "$(echo $s1 | cut -d' ' -f3)" ] && echo 1 || echo 0) \
    "counts: the shotgun's counter is the shells left, before and after firing (weapon shown count: $s1 -> $s2; shells STAT_AMMO: $p1 -> $p2)"
check $([ "$(echo $p1 | cut -d' ' -f3)" = "$(echo $s1 | cut -d' ' -f3)" ] && [ "$(echo $p2 | cut -d' ' -f3)" = "$(echo $s2 | cut -d' ' -f3)" ] && echo 1 || echo 0) \
    "counts: the status bar's STAT_AMMO is the main hand's count, not its ammo type ($p1, $p2 = shells nails STAT_AMMO)"
r3=$(hand counts right 3); p3=$(pool counts 3)
check $([ "${r3%% *}" = 31 ] && [ "$(echo $r3 | cut -d' ' -f3)" = "$(echo $p3 | cut -d' ' -f1)" ] && [ "$(echo $p3 | cut -d' ' -f1)" -lt "$(echo $p2 | cut -d' ' -f1)" ] && echo 1 || echo 0) \
    "counts: the revolver reads the shells left after its shot ($r3; shells $p3)"
r4=$(hand counts right 4)
check $([ "${r4%% *}" = 15 ] && [ "$(echo $r4 | cut -d' ' -f2)" = 1 ] && [ "$(echo $r4 | cut -d' ' -f3)" -lt 30 ] && [ "$(echo $r4 | cut -d' ' -f3)" -gt 0 ] && echo 1 || echo 0) \
    "counts: the grunt's shotgun reads its magazine, drained by its shots ($r4 of 30)"
l5=$(hand counts left 5); r5=$(hand counts right 5); p5=$(pool counts 5)
check $([ "${l5%% *}" = 6 ] && [ "$(echo $l5 | cut -d' ' -f3)" = "$(echo $p5 | cut -d' ' -f2)" ] && [ "$(echo $r5 | cut -d' ' -f3)" = "$(echo $r4 | cut -d' ' -f3)" ] && echo 1 || echo 0) \
    "counts: dual wielding, each hand its own counter (left: $l5 = the nails, $p5; right: $r5, the grunt's shotgun)"

# 2. pixels. Shot 1: no bar (viewsize 120); 2: dual wield, bullet time ready; 3: the right hand emptied (G); 4: bullet
# time running; 5: vr_flat_hud 0.
SET="r_norefresh 0; viewsize 120; scr_sbarscale 2; scr_sbaralpha 1; crosshair 0; vr_crosshair 0; tune vr_flat_bob 0; tune vr_flat_sway 0; vr_bullettime_enabled 1; vr_bullettime_fx 0"
run pixels "$P; $SET; impulse 154; wait5; give n 50; wait2; impulse 176; wait30; vr_flathud_report; screenshot; wait2; viewsize 100; wait3; screenshot; wait2; impulse 47; wait20; vr_flathud_report; viewsize 120; wait3; screenshot; wait2; viewsize 100; wait3; screenshot; vr_bullettime; wait10; vr_flathud_report; screenshot; vr_bullettime; wait5; vr_flat_hud 0; wait3; screenshot" "^flathud:" -Clean -Out "$S/flathud.png"
python "$HERE/flathud_check.py" "$S/flathud.png" "$S/pixels.log" 0,1,0,3,3,3 > "$S/flathudpx.log" 2>&1
px() { awk -v s="$1" -v p="$2" '$2==s && $3==p {print $5; exit}' "$S/flathudpx.log"; }
mean() { awk -v s="$1" -v p="$2" '$2==s && $3==p {print $7, $8, $9; exit}' "$S/flathudpx.log"; }
over() { awk -v v="$1" -v t="$2" 'BEGIN{exit !(v+0 >= t+0)}' && echo 1 || echo 0; }
under() { awk -v v="$1" -v t="$2" 'BEGIN{exit !(v+0 < t+0)}' && echo 1 || echo 0; }
check $([ "$(over "$(px 2 face)" 0.3)$(over "$(px 2 health)" 0.15)" = 11 ] && echo 1 || echo 0) \
    "pixels: the portrait and the health are drawn in the middle over the ammo bar (changed: face $(px 2 face), health $(px 2 health))"
check $([ "$(over "$(px 2 lefthand)" 0.1)$(over "$(px 2 righthand)" 0.1)" = 11 ] && echo 1 || echo 0) \
    "pixels: each hand's counter at its own bottom corner, dual wielding (changed: left $(px 2 lefthand), right $(px 2 righthand))"
b2=$(mean 2 timebar)
check $(echo "$b2" | awk '{exit !($3 > $1 + 60 && $2 > $1 + 40)}' && [ "$(over "$(px 2 timebar)" 0.3)" = 1 ] && echo 1 || echo 0) \
    "pixels: bullet time's bar over them, full and cyan while ready (changed $(px 2 timebar), mean rgb $b2)"
check $([ "$(under "$(px 4 righthand)" 0.05)$(over "$(px 4 lefthand)" 0.1)" = 11 ] && grep -qa "^flathud: right hand: weapon 0 shown 0" "$S/pixels.log" && echo 1 || echo 0) \
    "pixels: the right hand emptied (G), its counter is gone and the left one stays (changed: right $(px 4 righthand), left $(px 4 lefthand))"
b5=$(mean 5 timebar)
check $(echo "$b5" | awk '{exit !($1 > $3 + 60 && $2 > $3 + 40)}' && grep -qa "time bar 1 level [0-9.]* state 2" "$S/pixels.log" && echo 1 || echo 0) \
    "pixels: the bar turns yellow while bullet time runs (mean rgb $b5; the report's state 2)"
check $([ "$(over "$(px 6 oldface)" 0.3)$(under "$(px 6 health)" 0.05)$(under "$(px 6 timebar)" 0.3)" = 111 ] && echo 1 || echo 0) \
    "pixels: vr_flat_hud 0 gives Ironwail's own layout back (the portrait at the bottom left: $(px 6 oldface); no health nor bar in the middle: $(px 6 health), $(px 6 timebar))"

verdict; exit $fail
