#!/bin/bash
# flatxhair_melee_test.sh <agent> -- flatscreen play's visor crosshair with melee weapons (flatxhair, his flat notes 2026-11-26
# items 9 and 10; ROUND26.md "flatxhair"): one headless flat run of e1m1 in fast mode, facing the wall north of the start
# corridor (y 768), reading vr_flat_xhair_probe at rest, as each blow's strike begins and after it:
#   flip16/24  the spear in a flipped grip, 16 and 24 units from the wall: its fire button's thrust (the solver's, down out
#              of the raised pose) lands where the crosshair said as it began ("melee said ... apart": under 6 units)
#   old16      the same with vr_flat_xhair_thrust_path 0 (the reversed pole's shot line, as before): it said nowhere near
#   flipfar    40 units from the wall, out of its reach: the crosshair is on the tip's end in the air, not on the wall
#   up16       the spear held upright: lands where its crosshair said (under 6 units)
#   bloom      each blow's kick: the crosshair's lines pushed out as its strike begins (melee kick > 3 px, spread > 1 px),
#              back in after it (under a fifth); a fist's blow kicks less than the spear's (vr_flat_xhair_melee_fist)
# Prints PASS/FAIL lines and "flatxhair_melee_test: N passed, M failed"; exits 1 on a failure. The run ends on
# "flatxhair_melee_test: end" (-QuitOn).
AGENT=${1:?agent}
fail=0
QT_NAME=flatxhair_melee_test
source "$(dirname "$0")/qvrtest.sh"
log=$TREE/scratch/flatxhair_melee_test.log
X="vr_flat_xhair_probe"
give() { echo "vr_test_give_wid $1; vr_test_give_flags $2; impulse 149; wait30"; }
# `blow <tag> <y>`: stand at y (the wall 16 + its distance further), probe at rest, thrust (the fire button), probe as the
# strike begins (18 frames: its windup and hold are 0.21 s) and after it.
blow() { echo "setpos 480 $2 88 0 90 0; wait40; echo flatxhair_melee_test: $1 rest; $X; vr_flat_punch main; wait18; echo flatxhair_melee_test: $1 strike; $X; wait60; echo flatxhair_melee_test: $1 after; $X"; }
S="map e1m1; god; notarget; developer 1; vr_flatplay_debug 1; vr_flat_xhair_smooth 0; wait40"
S="$S; $(give 13 11520); $(blow flip16 736); $(blow flip24 728); $(blow flipfar 712)"
S="$S; tune vr_flat_xhair_thrust_path 0; $(blow old16 736); tune_reset vr_flat_xhair_thrust_path"
S="$S; $(give 13 3328); $(blow up16 736)"
S="$S; $(give 0 0); $(blow fist 736)"
# (flatxhair2: every melee weapon's swing in both grips, 12 units from the wall (the flipped dagger 6): wid, flags (its kind
# * 256, +8192 flipped), where he stands.)
WEAPONS="sword:13:0 swordrev:13:8192 dagger:13:3072 daggerrev:13:11264:746 axe:2:0 axerev:2:8192 hammer:13:1280 hammerrev:13:9472 mace:13:1536 macerev:13:9728 crowbar:17:0 crowbarrev:17:8192"
for w in $WEAPONS; do
    IFS=: read -r tag wid flags y <<< "$w"
    S="$S; $(give $wid $flags); $(blow $tag ${y:-740})"
done
S="$S; echo flatxhair_melee_test: end"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Script "$S; toggleconsole; quit" -QuitOn "flatxhair_melee_test: end" \
    -Timeout 200 -Filter "^flatxhair|^flatplay: main punch|^spear:" > "$log" 2>&1
# `field <tag> <phase> <line pattern> <field>`: the number after <field> on the first probe line matching the pattern in
# that phase's section.
field() {
    awk -v t="flatxhair_melee_test: $1 $2" '/^flatxhair_melee_test: /{on = index($0, t) == 1; next} on' "$log" |
        grep -a "$3" | head -1 | grep -ao " $4 [-0-9.]*" | head -1 | awk '{ print $2 + 0 }'
}
ok() { awk "BEGIN { exit !(${1}) }" && echo 1 || echo 0; }
for c in flip16 flip24 old16 up16 fist; do
    printf '  %-8s rest: %s\n  %-8s said:  %s\n' "$c" "$(awk -v t="flatxhair_melee_test: $c rest" '/^flatxhair_melee_test: /{on = index($0, t) == 1; next} on' "$log" | grep -a "main raw" | head -1)" \
        "$c" "$(awk -v t="flatxhair_melee_test: $c after" '/^flatxhair_melee_test: /{on = index($0, t) == 1; next} on' "$log" | grep -a "main melee said" | head -1)"
done
for c in flip16 flip24 up16; do
    a=$(field $c after "main melee said" apart)
    check $(ok "${a:-99} < 6") "$c: the blow lands where the crosshair said (${a:-no landing} units apart)"
done
for w in $WEAPONS; do
    c=${w%%:*}
    # (A quick swing lands before the strike's probe, 18 frames in: its landing is printed there.)
    ph=after
    [ -z "$(field $c after "main melee said" apart)" ] && ph=strike
    a=$(field $c $ph "main melee said" apart); k=$(field $c $ph "main melee said" said)
    check $(ok "${k:-0} == 1 && ${a:-99} < 6") "$c: its swing lands where the crosshair said (${a:-no landing} units apart)"
done
a=$(field old16 after "main melee said" apart)
check $(ok "${a:-0} > 20") "old16: the old shot line said nowhere near (${a:-no landing} units apart)"
yy=$(awk -v t="flatxhair_melee_test: flipfar rest" '/^flatxhair_melee_test: /{on = index($0, t) == 1; next} on' "$log" | grep -a "main raw" | head -1 | awk '{ for(i = 1; i <= NF; i++) if($i == "hit") print $(i + 2) + 0 }')
check $(ok "${yy:-999} < 766 && ${yy:-0} > 720") "flipfar: out of reach, the crosshair is on the tip's end in the air (y ${yy:-?}, the wall 768)"
for c in flip16 up16 fist; do
    k=$(field $c strike "main melee kick" kick); s=$(field $c strike "main kick" spread); k2=$(field $c after "main melee kick" kick)
    eval "kick_$c=${k:-0}"
    check $(ok "${k:-0} > 3 && ${s:-0} > 1 && ${k2:-99} < ${k:-0} * 0.2") "$c: the blow kicks the crosshair (${k:-?} px, spread ${s:-?}) and it eases back (${k2:-?})"
done
check $(ok "${kick_fist:-99} < ${kick_flip16:-0}") "a fist's blow kicks less than the spear's (${kick_fist:-?} against ${kick_flip16:-?})"
verdict
exit $fail
