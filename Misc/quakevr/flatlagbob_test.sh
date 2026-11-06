#!/bin/bash
# flatlagbob_test.sh <agent> -- the flatscreen turn's lag and the walk's forward bob, through vr_flat_walk_probe (FLATPLAY.md,
# "Bob and sway"). Fast mode; one run, each probe a counted 6 + 36 + 100 frames, the run ends on its own last line.
#   Turning right while he walks (+right): the main hand's yaw off the view (flatwalkfwd's "main yaw" range, degrees, to the
#   right). vr_flat_turnlag 0: locked; 1: it trails (further left); -1: it leads (further right); vr_flat_turnlag_cap 0.5:
#   the trail capped smaller.
#   Walking straight: the hands' and shoulders' range along the camera's forward with vr_flat_bob_fwd 0 and 2, level and
#   looking 40 down (the axis is the view's own forward): the body and arms ride it with the gun.
AGENT=${1:-pickup}
source "$(dirname "$0")/qvrtest.sh"
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"
log=$HERE/scratch/flatlagbob.log
# (Each probe from e1m1's start, as flatwalk_test.sh: 6 + 36 + 72 frames, waited out with a few to spare.)
M="map e1m1; god; notarget; r_norefresh 0; vr_test_give_wid 4; impulse 149; wait30"
# (+forward held here: the probe's own goes to the end of the command buffer, behind this script's waits.)
P() { echo "$M; +forward; vr_flat_walk_probe $1 72 0 $2; wait130; -forward"; }
T() { echo "$M; +forward; +right; vr_flat_walk_probe $1 72; wait130; -forward; -right"; }
S="developer 1; tune vr_flat_turnlag 0; $(T lag0)"
S="$S; tune vr_flat_turnlag 1; $(T lag1); tune vr_flat_turnlag -1; $(T lagneg)"
S="$S; tune vr_flat_turnlag 1; tune vr_flat_turnlag_cap 0.5; $(T lagcap); tune_reset"
S="$S; $(P fwd0 0); tune vr_flat_bob_fwd 2; $(P fwd2 0); tune vr_flat_bob_fwd 0; $(P fwd0p 40); tune vr_flat_bob_fwd 2; $(P fwd2p 40)"
S="$S; tune_reset; echo flatlagbob done; toggleconsole; quit"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$S" -Filter "^flatwalkfwd|^flatwalk: |^flatlagbob" -QuitOn "flatlagbob done" -Timeout 300 > "$log" 2>&1
grep -a "^flatwalkfwd" "$log" | sed 's/^/  /'
f() { awk -v l="$1" -v c="$2" '$1 == "flatwalkfwd:" && $2 == l { print $c }' "$log"; }
# (Fields: 5 6 the hands' forward range, off and main; 9 10 the shoulders'; 13 14 the main hand's yaw, least and most.)
lo0=$(f lag0 13); lo1=$(f lag1 13); hi0=$(f lag0 14); hin=$(f lagneg 14); loc=$(f lagcap 13)
echo "  turning right: main hand yaw least $lo0 (locked), $lo1 (lag 1), $loc (cap 0.5); most $hi0 (locked), $hin (lag -1)"
check $(awk -v a="$lo0" -v b="$lo1" 'BEGIN { exit !(a != "" && b != "" && b < a - 0.5) }' && echo 1 || echo 0) "vr_flat_turnlag 1: the hands trail a turn"
check $(awk -v a="$hi0" -v b="$hin" 'BEGIN { exit !(a != "" && b != "" && b > a + 0.5) }' && echo 1 || echo 0) "vr_flat_turnlag -1: they lead it instead"
check $(awk -v a="$lo1" -v b="$loc" -v z="$lo0" 'BEGIN { exit !(b != "" && b > a + 0.2 && b < z + 0.1) }' && echo 1 || echo 0) "vr_flat_turnlag_cap 0.5: the trail capped smaller"
for p in "" p; do
  h0=$(f fwd0$p 6); h2=$(f fwd2$p 6); s0=$(f fwd0$p 10); s2=$(f fwd2$p 10)
  echo "  forward bob${p:+ (looking 40 down)}: main hand's forward range $h0 -> $h2, its shoulder's $s0 -> $s2 (vr_flat_bob_fwd 0 -> 2)"
  check $(awk -v a="$h0" -v b="$h2" -v c="$s0" -v d="$s2" 'BEGIN { exit !(b != "" && d != "" && b > a + 1.0 && d > c + 1.0) }' && echo 1 || echo 0) \
    "vr_flat_bob_fwd 2${p:+ looking 40 down}: the gun bobs forward and back along the view, the shoulder with it"
done
verdict; exit $fail
