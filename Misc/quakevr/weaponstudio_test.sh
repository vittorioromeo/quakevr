#!/bin/bash
# weaponstudio_test.sh <agent> -- the Weapon Studio (Quake/vr/vr_weaponstudio.cpp, vr_weaponstudio_ui.cpp), headless
# flatscreen (the tool is a flatscreen developer tool: -flatscreenmode, no headset). Three runs:
#   1. its own test (vr_weaponstudio test / vr_weaponstudio_test): the window opens on a 960x540 screen, its pages
#      open, a grip spot narrows its key rows, the filter shows only the keys it matches, one key is written through
#      its stand-in cvar and read back from the store, the aligner measures the held weapon, an armed humanoid is
#      placed and the shipped arms solve reports its pose, and clicking the window's Close button closes it and hands
#      the game back. Every widget it clicks must be on the screen (the window fits it, so its Close button is
#      reachable): the test says so in its own verdict line, which is what ends the run.
#   2. store parity: the Weapon Studio is a new consumer of the store the Weapon Offsets / Weapon Weights menus use --
#      the same stand-in cvar, named vr_wofs_<key>_<slot+1> (zero padded, as the menus and the config write it). The
#      key is changed the way a menu changes it (the cvar set from the console), the tool prints it back from its own
#      print (so it reads the same store), and it is put back through the tool (vr_weaponstudio reset).
#   3. the aligner on three weapons: hold one (impulse 150 + a weapon id), measure it (vr_weaponstudio align both:
#      the sight line's degrees off where the gun shoots), write the line-up (align both apply, which fills only that
#      slot's Shot Pitch / Shot Yaw), re-measure it, and let the tool put the slot back to its defaults. The gate is
#      that no measured sight line ends further off than it started, that the one that was off comes onto the shot,
#      and that all three end lined up (under 0.25 deg). `align` prints its measure line, and `align apply` prints it
#      again before writing, so one weapon gives four records: measure, measure+write, written, measure again.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=${1:-fixgame3}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
source "$(dirname "$0")/qvrtest.sh"
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"

# ---- 1. the tool's own test ---------------------------------------------------------------------------------------
log=$HERE/scratch/ws_self.log
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "map e1m1;wait60;god;notarget;developer 1;vr_mock_eye_size 256;impulse 154;wait20;vr_weaponstudio test;wait400" -Filter "^weaponstudio" -QuitOn "^weaponstudio: [0-9]+ passed" -Timeout 300 > "$log" 2>&1
expect_verdict "$(cat "$log")" "weaponstudio" "the Weapon Studio's own test (its pages, a key through its stand-in cvar, the aligner, a placed humanoid's pose, its Close)"
grep -aE "^weaponstudio test: (click|its pose)" "$log" | sed 's/^/  /'
check $(grep -q "Close gave the game back: ok" "$log" && grep -q "^weaponstudio: closed" "$log" && echo 1 || echo 0) "its Close button closes the window and hands the game back"
check $(grep -q "the window fits the screen: every widget the test clicked was on it: ok" "$log" && echo 1 || echo 0) "every widget it clicked was on the 960x540 screen (the window and its Close button fit it)"
check $(grep -q "still open, clicking Close again" "$log" && echo 0 || echo 1) "its Close took the first click (no retry was needed)"
check $(grep -q "the game's time let go" "$log" && grep -q "arms solve reported its pose: ok" "$log" && echo 1 || echo 0) "a placed humanoid's pose was read on a condition (the tool let go of the held game and the shipped arms solve reported it)"

# ---- 1b. the preview humanoid's record against a map change, and the console path with the window shut --------------
# (The same map loaded over it: its edict number is someone else's now, so the close must remove nothing. Then a placing
# from the console with the window shut, which nothing would ever take away, is refused.)
logp=$HERE/scratch/ws_preview.log
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "map e1m1;wait60;god;notarget;vr_weaponstudio open;wait3;vr_weaponstudio preview 0 4;wait3;map e1m1;wait60;vr_weaponstudio close;wait3;vr_weaponstudio preview 0 4;wait3;toggleconsole;quit" -Filter "^weaponstudio" > "$logp" 2>&1
check $(grep -q "preview: .* placed at" "$logp" && echo 1 || echo 0) "a preview humanoid placed with the window open"
check $(grep -q "subject [0-9]* removed" "$logp" && echo 0 || echo 1) "after a map load over it, the close removes nothing (its edict number is another's now)"
check $(grep -q "open its window first" "$logp" && [ "$(grep -c 'placed at' "$logp")" = 1 ] && echo 1 || echo 0) "with the window shut, the console's placing is refused"

# ---- 2. store parity with the Weapon Offsets / Weapon Weights menus ------------------------------------------------
# (slot 1 is the shotgun; its muzzle point's key is muzzle_x, so the cvar is vr_wofs_muzzle_x_02.)
log2=$HERE/scratch/ws_store.log
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; god; notarget; developer 1; impulse 154; wait20; vr_wofs_muzzle_x_02; vr_wofs_muzzle_x_02 1.5; wait3; vr_weaponstudio print 1; wait3; vr_wofs_muzzle_x_02; vr_weaponstudio reset 1; wait3; vr_wofs_muzzle_x_02; toggleconsole; quit" -Filter "muzzle_x|weaponstudio" > "$log2" 2>&1
reads=$(grep -aE '^"vr_wofs_muzzle_x_02" is' "$log2")
before=$(echo "$reads" | sed -n 1p); mid=$(echo "$reads" | sed -n 2p); after=$(echo "$reads" | sed -n 3p)
echo "  before: $before"; echo "  changed the menus' way: $mid"; echo "  after the tool's reset: $after ($log2)"
grep -aE "^ +(muzzle_x|shot_pitch|shot_yaw) " "$log2" | head -4 | sed 's/^/  the tool prints: /'
check $(echo "$before" | grep -q "(default)" && echo 1 || echo 0) "store: the key starts at its default (vr_wofs_muzzle_x_02, slot 1)"
check $(grep -qE '^ +muzzle_x "1\.5' "$log2" && echo 1 || echo 0) "store: the tool reads back what the menu's own cvar (vr_wofs_muzzle_x_02) was set to"
check $(echo "$mid" | grep -q 'is "1.5' && ! echo "$mid" | grep -q "(default)" && echo 1 || echo 0) "store: the change is the cvar's, not a copy the tool keeps"
check $(echo "$after" | grep -q "(default)" && echo 1 || echo 0) "store: the tool puts the key back through the same cvar"

# ---- 3. the aligner on three weapons ------------------------------------------------------------------------------
# (The aligner measures what a hand holds, so each weapon is held first: 154 the shotgun (slot 1), 158 the rocket
# launcher (slot 5), 159 the proximity gun (slot 10); the slots are put back at the end.)
# The console wraps a long line, so an align record is read as its first line plus the lines that follow it.
recs() { awk '{ if ($0 ~ /^weaponstudio align: /) { if (rec != "") print rec; rec = $0 } else if (rec != "") { rec = rec " " $0 } } END { if (rec != "") print rec }' "$1"; }
# Each record the harness cares about, as: <slot> <degrees> <1 when it is the written line>
alignTrio() { awk '{
    s = -1
    if (match($0, /hand, slot [0-9]+/)) s = substr($0, RSTART + 11) + 0            # "... main hand, slot 10: ..."
    else if (match($0, /align: slot [0-9]+ written/)) s = substr($0, RSTART + 12) + 0
    else next
    d = -1; w = 0
    if (match($0, /sight line is now [0-9.]+/)) { d = substr($0, RSTART + 18) + 0; w = 1 }
    else if (match($0, /sight line is [0-9.]+/)) d = substr($0, RSTART + 14) + 0
    printf "%d %.3f %d\n", s, d, w
}' "$1"; }
log3=$HERE/scratch/ws_align.log
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script "map e1m1; wait60; god; notarget; developer 1; impulse 154; wait25; vr_weaponstudio slot; wait3; vr_wofs_shot_pitch_02; vr_wofs_shot_yaw_02; vr_weaponstudio align both; wait5; vr_weaponstudio align both apply; wait5; vr_weaponstudio align both; wait5; vr_wofs_shot_pitch_02; vr_wofs_shot_yaw_02; impulse 158; wait25; vr_weaponstudio align both; wait5; vr_weaponstudio align both apply; wait5; vr_weaponstudio align both; wait5; impulse 159; wait25; vr_weaponstudio align both; wait5; vr_weaponstudio align both apply; wait5; vr_weaponstudio align both; wait5; vr_wofs_muzzle_reset 1; vr_weaponstudio reset 1; vr_weaponstudio reset 5; vr_weaponstudio reset 10; wait3; vr_wofs_shot_pitch_02; toggleconsole; quit" -Filter "weaponstudio|vr_wofs_shot" > "$log3" 2>&1
grep -aE "^weaponstudio align: (main hand|slot)" "$log3" | sed 's/^/  /'
grep -aE '^"vr_wofs_shot_(pitch|yaw)_02" is' "$log3" | sed 's/^/  /'
triples=$(alignTrio "$log3")
n=$(echo "$triples" | grep -c .)
notWorse=0; improved=0; lined=0; measured=0
for i in 1 5 9; do
  b=$(echo "$triples" | sed -n "${i}p" | awk '{ print $2 }')
  wv=$(echo "$triples" | sed -n "$((i + 2))p" | awk '{ print $2 }')
  a=$(echo "$triples" | sed -n "$((i + 3))p" | awk '{ print $2 }')
  [ -z "$b" ] && continue
  measured=$((measured + 1))
  echo "  weapon $measured: sight line $b deg off the shot before, $wv deg as the tool wrote it, $a deg after the line-up was written"
  notWorse=$((notWorse + $(awk -v x="$b" -v y="$a" 'BEGIN { print (y <= x + 0.01) ? 1 : 0 }')))
  improved=$((improved + $(awk -v x="$b" -v y="$a" 'BEGIN { print (y < x - 0.05) ? 1 : 0 }')))
  lined=$((lined + $(awk -v y="$a" 'BEGIN { print (y < 0.25) ? 1 : 0 }')))
done
check $([ "$n" -ge 12 ] && [ "$measured" -eq 3 ] && echo 1 || echo 0) "align: three held weapons measured (measure, write, measure again)"
check $([ "$notWorse" -eq 3 ] && [ "$improved" -ge 1 ] && echo 1 || echo 0) "align: writing the line-up leaves no sight line further off, and it moves the one that was off (measured above) onto the shot"
check $([ "$lined" -eq 3 ] && echo 1 || echo 0) "align: every measured sight line ends lined up with where the gun shoots (under 0.25 deg)"
check $(grep -q "vr_wofs_shot_pitch_02" "$log3" && grep -q "shot pitch" "$log3" && echo 1 || echo 0) "align: what it writes is the slot's Shot Pitch / Shot Yaw, in the store's own cvars"
check $(grep -q "the sight line is now" "$log3" && echo 1 || echo 0) "align: it re-measures after writing, and says how far off it is now"

verdict
exit $fail
