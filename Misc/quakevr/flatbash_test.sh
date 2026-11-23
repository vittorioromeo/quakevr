#!/bin/bash
# flatbash_test.sh <agent> -- flat-screen play: the fire button in the parry's guard bashes (vr_flat_bash) and a sword's
# free hand takes its blade near the tip in the guard (vr_flat_parry_blade); his notes 2026-11-23 (FLATPLAY.md, "Parry
# guard"). One headless run in fast mode, vrtesthall, a plain grunt spawned ahead for each weapon:
#   the knight's sword in the main hand: the right mouse button held, the free (off) hand goes onto the blade's hotspot
#   (vr_twohand takes the half-sword grip: "took the blade"; the status's posed and drawn hand within a few units of it);
#   the left button then bashes, not swings: the engine's bash, QC's "melee event: bash" with a hit on the grunt (two hands:
#   the blade grip is the headset's two-handed hold), its knockback and stagger ("bash hit: monster_army"); the guard let go,
#   the blade let go too and the share back to 0;
#   the axe (no Blade hotspot): the guard leaves the free hand where it was (no blade grip), and its bash is one hand's.
# Ends on its own last line (-QuitOn); prints "flatbash: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/flatbash
mkdir -p "$S"
L="$S/run.log"
ST="vr_flatplay_status"
SCRIPT="map vrtesthall;wait60;god;notarget;developer 1;vr_flatplay_debug 1;vr_debug_2h_grip 1;vr_agrunt_repl 0;vr_parry_stamina 0;vr_shove_stamina 0"
for w in "sword 163" "axe 152"; do
  set -- $w
  # (The guard: 0.05 s, the blade 0.12 s more and vr_twohand's take 0.2 s: 40 frames are plenty. The bash: 0.39 s, 28 frames.)
  # (impulse 244: a grunt 64 units ahead; then a step up to it, 44 units between them: a bash's reach from the guard.)
  SCRIPT="$SCRIPT;setpos 0 300 25 0 90 0;impulse $2;wait40;impulse 244;wait5;setpos 0 320 25 0 90 0;wait15"
  # (vr_flat_melee_2h_report: the drawn hands' most move and turn a frame since the last report, through the take and let-go.)
  R="vr_flat_melee_2h_report"
  SCRIPT="$SCRIPT;echo fbt: $1 guard;$R;+offhandattack;wait40;$R;$ST;echo fbt: $1 bash;+attack;wait3;-attack;wait40;$ST"
  SCRIPT="$SCRIPT;echo fbt: $1 let go;$R;-offhandattack;wait30;$R;$ST;wait70"
done
SCRIPT="$SCRIPT;echo flatbash done;toggleconsole;quit"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$SCRIPT" -QuitOn "^flatbash done" -Timeout 240 \
  -Filter '^fbt:|^flatbash|^flat2h:|flatplay: bash|flatplay: (main|off) punch|melee event: (flat )?bash|bash hit:|blade grip:|guard bash:|2h grip:|flatblade:|ERROR|CRASH' > "$L" 2>&1
# The lines of one block: from its marker to the next.
block() { awk -v m="fbt: $1" '$0==m || $0==m" "{on=1;next} /^fbt:|^flatbash done/{on=0} on' "$L"; }
# The posed / drawn hand's distance off the blade's hotspot in a block's status ("off the hotspot (drawn N u)").
posed() { block "$1" | grep -a "blade grip: the off hand" | grep -ao 'hand -\?[0-9.]* u off' | grep -ao -- '-\?[0-9.]*' | head -1; }
drawn() { block "$1" | grep -a "blade grip: the off hand" | grep -ao 'drawn -\?[0-9.]* u' | grep -ao -- '-\?[0-9.]*' | head -1; }
check $(grep -aq "^flatbash done" "$L" && echo 1 || echo 0) "the run reached its end"

# The sword: the free hand on the blade's hotspot in the guard.
check $(block "sword guard" | grep -aq "2h grip: off hand took the blade" && echo 1 || echo 0) "sword: the guard's free hand took the blade"
g=$(block "sword guard" | grep -a "blade grip: main" | head -1)
check $(grep -aq "asked for, share 1.00" <<<"$g" && grep -aq "held by its blade" <<<"$g" && ! grep -aq "not held" <<<"$g" && echo 1 || echo 0) "sword: in the guard the blade grip is up and held (${g# *})"
# The off hand's most turn a frame as it goes onto the blade, and as it lets go (the drawn hand turned over the grip's
# blend, not at once: 73 and 40 degrees in a frame before).
turned() { block "$1" | grep -a "^flat2h:" | sed -n 2p | grep -ao 'off [0-9.]* units [0-9.]*' | grep -ao '[0-9.]*$'; }
for k in "sword guard" "sword let go"; do
  t=$(turned "$k")
  check $([ -n "$t" ] && awk -v t="$t" 'BEGIN{exit !(t <= 20)}' && echo 1 || echo 0) "$k: the drawn off hand turned at most ${t:-?} degrees a frame (20)"
done
# Its palm down over the blade (vr_flat_parry_blade_palm; his note 2026-11-23: it was up, under the blade: z 0.88).
pz=$(block "sword guard" | grep -a "blade grip: the off hand" | grep -ao 'palm z -\?[0-9.]*' | grep -ao -- '-\?[0-9.]*$' | head -1)
check $([ -n "$pz" ] && awk -v z="$pz" 'BEGIN{exit !(z <= -0.7)}' && echo 1 || echo 0) "sword: the drawn free hand's palm down on the blade (palm z ${pz:-?}, -1 straight down)"
# Its fingers away from him, over the blade (his second note 2026-11-23: they pointed back at him).
fo=$(block "sword guard" | grep -a "blade grip: the off hand" | grep -ao 'fingers out -\?[0-9.]*' | grep -ao -- '-\?[0-9.]*$' | head -1)
check $([ -n "$fo" ] && awk -v f="$fo" 'BEGIN{exit !(f >= 0.5)}' && echo 1 || echo 0) "sword: the drawn free hand's fingers point away from him (fingers out ${fo:-?}, 1 straight away)"
p=$(posed "sword guard"); d=$(drawn "sword guard")
check $([ -n "$p" ] && awk -v p="$p" 'BEGIN{exit !(p >= 0 && p <= 1)}' && echo 1 || echo 0) "sword: the posed free hand on the hotspot (${p:-?} u)"
check $([ -n "$d" ] && awk -v d="$d" 'BEGIN{exit !(d >= 0 && d <= 6)}' && echo 1 || echo 0) "sword: the drawn free hand on the blade at the hotspot (${d:-?} u, the half-sword grip's slide along the blade)"
# Its bash: the engine's, not a swing; QC's bash with a hit; the grunt knocked back and staggered.
b=$(block "sword bash")
check $(grep -aq "flatplay: bash (the main hand's fire button): main hand" <<<"$b" && ! grep -aqE "flatplay: (main|off) punch" <<<"$b" && echo 1 || echo 0) "sword: the fire button in the guard bashes, no swing"
e=$(grep -a "melee event: bash" <<<"$b" | head -1)
check $(grep -aqE "bash, hand 1, two hands, [1-9][0-9]* hits?" <<<"$e" && echo 1 || echo 0) "sword: QC's two-handed bash lands (${e:-none})"
h=$(grep -a "bash hit: monster_army" <<<"$b" | head -1)
check $([ -n "$h" ] && echo 1 || echo 0) "sword: the grunt bashed (${h:-no bash hit})"
check $(block "sword bash" | grep -a "blade grip: main" | grep -aq ", held by its blade" && echo 1 || echo 0) "sword: the blade still held after the bash"
# Let go: the blade too.
l=$(block "sword let go" | grep -a "blade grip: main" | head -1)
check $(grep -aq "not asked for, share 0.00" <<<"$l" && grep -aq "not held by its blade" <<<"$l" && echo 1 || echo 0) "sword: the guard let go, the blade let go (${l# *})"
check $(block "sword let go" | grep -aq "2h grip: off hand let go (the flat guard ended" && echo 1 || echo 0) "sword: vr_twohand let go as the guard ended"

# The axe: no blade grip in the guard, a one-handed bash.
a=$(block "axe guard" | grep -a "blade grip: main" | head -1)
check $(grep -aq "no blade hotspot, not asked for, share 0.00" <<<"$a" && grep -aq "not held by its blade" <<<"$a" && ! block "axe guard" | grep -aq "took the blade" && echo 1 || echo 0) "axe: no blade grip in the guard (${a# *})"
ea=$(block "axe bash" | grep -a "melee event: bash" | head -1)
check $(grep -aqE "bash, hand 1, one hand, [1-9][0-9]* hits?" <<<"$ea" && block "axe bash" | grep -aq "bash hit: monster_army" && echo 1 || echo 0) "axe: a one-handed bash on the grunt (${ea:-none})"
verdict
