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
#   <weapon>   (flatxhair2, his flat notes 2026-11-26 item 6) the sword, the dagger, the axe, the hammer, the mace and the
#              crowbar, each upright and flipped ("rev"), 12 units from the wall: its swing lands where its crosshair said
#              (under 6 units; the flipped dagger's swing leaves no mark on a wall: not checked)
#   axe/sword  (flatxhair2, item 1) a second run, vrtutorial: the throw key held, its crosshair is shown on where the throw
#              lands (the arc from the frame before it met the wall, within 12 units on the wall)
#   grenb/grenatk/grenoff  (grenaim, his flat note 2026-11-27 item 3) the quick grenade wound up by B, by the main hand's
#              attack button and by the off hand's: each one's crosshair on where it lands, the same check
# Prints PASS/FAIL lines and "flatxhair_melee_test: N passed, M failed"; exits 1 on a failure. The runs end on
# "flatxhair_melee_test: end" and "flatxhair_melee_test: throws end" (-QuitOn).
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
# (flatxhair2: every melee weapon's swing in both grips, 12 units from the wall: wid, flags (its kind * 256, +8192
# flipped), and where he stands if not y 740. The flipped dagger (13:11264) is left out: its swing marks no wall.)
WEAPONS="sword:13:0 swordrev:13:8192 dagger:13:3072 axe:2:0 axerev:2:8192 hammer:13:1280 hammerrev:13:9472 mace:13:1536 macerev:13:9728 crowbar:17:0 crowbarrev:17:8192"
for w in $WEAPONS; do
    IFS=: read -r tag wid flags y <<< "$w"
    S="$S; $(give $wid $flags); $(blow $tag ${y:-740})"
done
S="$S; echo flatxhair_melee_test: end"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Script "$S; toggleconsole; quit" -QuitOn "flatxhair_melee_test: end" \
    -Timeout 200 -Filter "^flatxhair|^flatplay: main punch|^spear:" > "$log" 2>&1
# (flatxhair2: the throw key's aim. vrtutorial's lesson 12 south wall (y -814), as flatthrow_test's `wall`: the axe from
# 294 units and the sword from 164, B held 95 frames (charged), probed in the wind-up, let go: where it first meets the wall
# against where the crosshair said.)
tlog=$TREE/scratch/flatxhair_melee_test_throw.log
throw() { echo "setpos 2000 $2 -120 0 270 0; wait5; impulse $3; wait30; vr_mock_key b down; wait95; echo flatxhair_melee_test: $1 aim; $X; vr_mock_key b up; wait200; echo flatxhair_melee_test: $1 after"; }
# (grenaim, his flat note 2026-11-27 item 3: the quick grenade from 294 units, wound up 95 frames by B (grenb), by the
# main hand's attack button (grenatk: a press arms it, the next held winds up, vr_flat_grenade_arm_mode 1) and, a gun in
# the main hand, by the off hand's (grenoff): each one's crosshair on where it lands, as the axe's.)
G="setpos 2000 -520 -120 0 270 0; wait5; give r 20; vr_mock_key g down; wait2; vr_mock_key g up; wait50"
gren() { echo "$G; $2; wait3; $3; wait3; $2; wait95; echo flatxhair_melee_test: $1 aim; $X; $3; wait200; echo flatxhair_melee_test: $1 after"; }
GB="$G; vr_mock_key b down; wait95; echo flatxhair_melee_test: grenb aim; $X; vr_mock_key b up; wait200; echo flatxhair_melee_test: grenb after"
T="map vrtutorial; god; notarget; developer 1; vr_flat_xhair_smooth 0; vr_flat_grenade_arm_mode 1; wait10; $(throw axe -520 152); $(throw sword -650 163)"
T="$T; $GB; $(gren grenatk +attack -attack); vr_test_give_wid 6; impulse 149; wait30; $(gren grenoff +offhandattack -offhandattack)"
T="$T; echo flatxhair_melee_test: throws end"
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Script "$T; toggleconsole; quit" -QuitOn "flatxhair_melee_test: throws end" \
    -Timeout 200 -Filter "^flatxhair|^flat throw: (first hit|hit from|landed)|^flat grenade: (thrown|the|winding)" > "$tlog" 2>&1
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
for c in axe sword grenb grenatk grenoff; do
    sec() { awk -v t="flatxhair_melee_test: $c $1" '/^flatxhair_melee_test: /{on = index($0, t) == 1; next} on' "$tlog"; }
    # (The throwing hand's: the only one with a throw aim.)
    hand=$(sec aim | grep -a " throw aim " | head -1 | awk '{ print $2 }')
    aim=$(sec aim | grep -a "${hand:-main} throw aim" | head -1 | awk '{ print $5, $6, $7 }')
    shown=$(sec aim | grep -a "${hand:-main} shown" | head -1 | awk '{ print $4 }')
    # (The frame before it first met the wall: its middle, its velocity; carried on along them to the aim's wall plane, y.
    # Its origin itself stops short of the wall by its shape: a stuck sword's grip 20 units off it. Within 12: the thrown
    # axe's body flies about 10 units over the hand's arc, the same launch and fall: ROUND26.md "flatxhair2".)
    hit=$(sec aim | grep -a "flat throw: hit from" | head -1 | tr -d "'" | awk '{ print $5, $6, $7, $9, $10, $11 }')
    printf '  %-7s %s hand aim: %s  before the hit (place, velocity): %s\n' "$c" "${hand:-no}" "${aim:-none}" "${hit:-none}"
    d=$(echo "$aim $hit" | awk 'NF == 9 && $8 != 0 { k = ($2 - $5) / $8; dx = $1 - ($4 + $7 * k); dz = $3 - ($6 + $9 * k); printf "%.1f", sqrt(dx * dx + dz * dz) }')
    check $(ok "${shown:-0} == 1 && ${d:-99} < 12") "$c: the wound-up throw's crosshair is shown on where it lands (${d:-?} units apart on the wall)"
done
verdict
exit $fail
