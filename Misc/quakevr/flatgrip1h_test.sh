#!/bin/bash
# flatgrip1h_test.sh <agent> -- flatgrip3 (his flat item 7, 2026-11-26: "always force 1H grip for daggers and axes"),
# flat-screen play, headless, fast mode, one run ending on its own last line (-QuitOn):
#   with vr_flat_1h_dagger / vr_flat_1h_axe at their defaults (1): the axe, the super axe and the dagger taken in the main hand,
#   the other hand empty: no two-handed stance (share 0, not asked) and the free hand not on the grip, held still and in the
#   parry's guard (+offhandattack: no blade grip either); the knight's sword the same way: the stance and the grip (the
#   control: the report sees a two-handed hold); then vr_flat_1h_axe 0: the axe in the stance again, the grip held.
# Counted waits: the stance eases in over vr_flat_melee_2h_time; 90 frames (1.25 s) as flatmelee2h_test.
# Prints "flatgrip1h_test: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
QT_NAME=flatgrip1h_test
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatgrip1h
mkdir -p "$S"
L="$S/run.log"
R="vr_flat_melee_2h_report"
G="+offhandattack;wait40;$R;-offhandattack;wait30"
DAGGER="vr_test_give_wid 13;vr_test_give_flags 3072;impulse 149"
SC="map vrtesthall;wait60;god;notarget;developer 1;tune vr_flat_melee_2h 1;setpos 0 300 25 0 90 0"
SC="$SC;impulse 152;wait90;echo fg1h: axe;$R;echo fg1h: axe guard;$G"
SC="$SC;impulse 168;wait90;echo fg1h: superaxe;$R;echo fg1h: superaxe guard;$G"
SC="$SC;$DAGGER;wait90;echo fg1h: dagger;$R;echo fg1h: dagger guard;$G"
SC="$SC;impulse 163;wait90;echo fg1h: sword;$R"
SC="$SC;vr_flat_1h_axe 0;impulse 152;wait90;echo fg1h: axe2h;$R"
SC="$SC;echo fg1h done;toggleconsole;quit"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$SC" -QuitOn "^fg1h done" -Timeout 300 \
  -Filter '^fg1h|^flat2h|ERROR|CRASH' > "$L" 2>&1
# The report line that follows a marker (the console wraps it: joined up to its last word, "grip").
rep() { awk -v m="fg1h: $1" '$0==m || $0==m" "{on=1;next} on && /^flat2h: drawn hands/{got=1} got{sub(/ +$/, ""); s=s $0 " "; if ($0 ~ /grip$/) {print s; exit}}' "$L"; }
check $(grep -aq "^fg1h done" "$L" && echo 1 || echo 0) "the run reached its end"
for w in axe superaxe dagger; do
  for st in "$w" "$w guard"; do
    r=$(rep "$st")
    check $(grep -aq "share main 0.00 off 0.00, asked 0 0; the free hand does not hold the grip" <<<"$r" && echo 1 || echo 0) \
      "$st: one-handed (no stance, the free hand off the grip)"
  done
done
check $(rep sword | grep -aq "share main 1.00.*holds the grip" && echo 1 || echo 0) "sword: still two-handed (the control)"
check $(rep axe2h | grep -aq "share main 1.00.*holds the grip" && echo 1 || echo 0) "vr_flat_1h_axe 0: the axe two-handed again"
verdict
exit $fail
