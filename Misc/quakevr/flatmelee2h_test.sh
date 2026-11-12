#!/bin/bash
# flatmelee2h_test.sh <agent> -- flat-screen play: the two-handed melee stance (vr_flat_melee_2h_*; his notes 2026-11-12:
# a melee weapon with a two-handed grip always held in both hands while the other hand is empty, the one-handed hold when
# dual wielding or holding a prop, smooth both ways; 2H-only parry offsets). One headless run in fast mode, -flatscreenmode,
# ending on its own last line (-QuitOn):
#   1. each melee weapon swung by a handle (axe, Mjolnir, the knight's sword, the crowbar, the super axe) taken in the main
#      hand, no swing: the stance's share 1 and the free hand holding the grip (vr_flat_melee_2h_report);
#   2. the axe: the stance turned off and on again (vr_flat_melee_2h 0 / 1): each way the drawn hands' largest move a frame
#      under 2 units (the main hand) and 4 units (the free hand onto or off the grip), turns under 12 degrees;
#   3. a swing from the stance: back in it after, the grip held;
#   4. the parry's guard in two hands: vr_flat_melee_2h_parry_up 6 raises the guard's hand about 6 units; in one hand
#      (vr_flat_melee_2h 0) it changes nothing;
#   5. dual wielding (an axe in each hand): no stance; a rock pulled into the off hand (E, the scene flat_fg_melee2h): the
#      stance eases out.
# The force grab and the shove from the two-handed hold (release, then the grab): flatfg2h_test.sh.
# Prints "flatmelee2h_test: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
QT_NAME=flatmelee2h_test
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatmelee2h
mkdir -p "$S"
L="$S/run.log"
R="vr_flat_melee_2h_report"
SC="map vrtesthall;wait60;god;notarget;developer 1;vr_debug_2h_grip 1;setpos 0 300 25 0 90 0"
for w in "axe 152" "mjolnir 153" "sword 163" "crowbar 167" "superaxe 168"; do
  set -- $w
  SC="$SC;impulse $2;wait90;echo fm2: $1;$R;wait2"
done
SC="$SC;impulse 152;wait90;$R;vr_flat_melee_2h 0;wait40;echo fm2: off;$R;vr_flat_melee_2h 1;wait60;$R"
SC="$SC;vr_flat_punch main;wait90;echo fm2: swung;$R"
P="+offhandattack;wait30;$R;-offhandattack;wait25"
SC="$SC;echo fm2: guard2h0;$P;vr_flat_melee_2h_parry_up 6;echo fm2: guard2h6;$P"
SC="$SC;vr_flat_melee_2h 0;wait40;echo fm2: guard1h6;$P;vr_flat_melee_2h_parry_up 0;echo fm2: guard1h0;$P;vr_flat_melee_2h 1;wait40"
# (The off hand's axe taken away jumps its hand in the first 3 frames, the stance or not: 6.1 units without it, 5.1 with
# it, 2026-11-12. The stance's own way in is measured after them.)
SC="$SC;impulse 172;wait90;echo fm2: dual;$R;impulse 170;wait3;$R;wait60;echo fm2: in;$R"
# (The rock 160 ahead on the floor: looked at 8 degrees down, +lookdown's 150 a second over 4 frames.)
SC="$SC;vr_scenario flat_fg_melee2h;wait180;+lookdown;wait4;-lookdown;wait30;$R;echo fm2: before prop;$R;vr_flat_fg_press;wait150;echo fm2: prop;$R;vr_flat_fg_report"
SC="$SC;echo fm2 done;toggleconsole;quit"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$SC" -QuitOn "^fm2 done" -Timeout 300 \
  -Filter '^fm2|^flat2h|two-handed stance:|2h grip: .* took|flatfgrab: (sent|grab|press)|ERROR|CRASH' > "$L" 2>&1
# The report line that follows a marker.
# (The console wraps it: its lines joined up to its last word, "grip".)
rep() { awk -v m="fm2: $1" '$0==m || $0==m" "{on=1;next} on && /^flat2h: drawn hands/{got=1} got{sub(/ +$/, ""); s=s $0 " "; if ($0 ~ /grip$/) {print s; exit}}' "$L"; }
num() { grep -ao "$2" <<<"$1" | head -1 | grep -ao '[-0-9.]*' | sed -n "${3:-1}p"; }
check $(grep -aq "^fm2 done" "$L" && echo 1 || echo 0) "the run reached its end"
for w in axe mjolnir sword crowbar superaxe; do
  r=$(rep $w)
  check $(grep -aq "share main 1.00.*holds the grip" <<<"$r" && echo 1 || echo 0) "$w: in the two-handed stance with no swing, the free hand on the grip"
done
# (Into the stance: the off hand's axe gone, the free hand onto the grip, both checked. Out of it: a rock pulled into the off
# hand, the grab's own motion its hand's, so only the weapon's hand is checked.)
for t in in prop; do
  r=$(rep $t)
  m=$(num "$r" 'main [0-9.]* units'); mt=$(num "$r" 'units [0-9.]* deg' 1); o=$(num "$r" 'off [0-9.]* units')
  [ $t = prop ] && o=0
  ok=$(awk -v m="$m" -v o="$o" -v t="$mt" 'BEGIN{print (m != "" && m + 0 < 2 && o + 0 < 4 && t + 0 < 12) ? 1 : 0}')
  check "$ok" "the stance's way $t: no jump (most a frame: main ${m:-?} u ${mt:-?} deg, free hand ${o:-?} u)"
done
check $(rep in | grep -aq "share main 1.00.*holds the grip" && echo 1 || echo 0) "the off hand emptied: into the stance, the grip held"
check $(rep off | grep -aq "share main 0.00" && echo 1 || echo 0) "vr_flat_melee_2h 0: the one-handed hold"
check $(rep swung | grep -aq "share main 1.00.*holds the grip" && echo 1 || echo 0) "a swing from the stance: back in it, the grip held"
z() { rep $1 |grep -ao 'now [-0-9.]* [-0-9.]* [-0-9.]*' | awk '{print $4}'; }
z0=$(z guard2h0); z6=$(z guard2h6); y6=$(z guard1h6); y0=$(z guard1h0)
check $(awk -v a="$z0" -v b="$z6" 'BEGIN{d=b-a; print (a != "" && d > 4.5 && d < 7.5) ? 1 : 0}') "two-handed guard: _parry_up 6 raises the hand (${z0:-?} -> ${z6:-?})"
check $(awk -v a="$y0" -v b="$y6" 'BEGIN{d=b-a; if (d < 0) d = -d; print (a != "" && d < 0.5) ? 1 : 0}') "one-handed guard: _parry_up changes nothing (${y0:-?} / ${y6:-?})"
check $(rep dual | grep -aq "share main 0.00.*asked 0 0" && echo 1 || echo 0) "dual wielding: no stance"
check $(rep "before prop" | grep -aq "share main 1.00" && echo 1 || echo 0) "the scene's axe: in the stance before the grab"
check $(rep prop | grep -aq "share main 0.00.*asked 0 0" && echo 1 || echo 0) "a rock pulled into the off hand: the one-handed hold"
verdict
