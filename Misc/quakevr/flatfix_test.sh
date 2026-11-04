#!/bin/bash
# flatfix_test.sh <agent> -- the two flatscreen groups this round is about, pinned with the numbers the shipped build
# prints (docs/vr-port/FLATPLAY.md; headless, -flatscreenmode). Written 2026-11-04 by agent/flatfix.
#
# Group 1, the ironsight's ease (vr_flat_aim, Quake/vr/vr_flatsight.cpp). What this suite holds the engine to is not a
# particular frame count -- it is that the travel takes the seconds vr_flat_aim_time says and the field of view comes
# off by the degrees vr_flat_aim_fov says, whatever those two say today. His tuning moved both defaults (29577aa8e,
# 2026-11-03: vr_flat_aim_fov 12 -> 40, vr_flat_aim_time 0.25 -> 0.15), so the shipped pair is measured against the
# numbers vr_flatplay_status prints for the very same run, and four tuned times and three tuned degrees are measured
# against the values put on the console. The frames come from what the debug prints drew (vr_flatplay_debug 2's
# "flatsight:" line: its t, its fov x pair); the seconds are its own clock, so they do not depend on the frame rate.
# Depth is read across fov_x: at vr_mock_eye_size 256 the drawn field of view is 106.26 x 73.74, so the degrees taken
# off the view show up as 1.06 of that across fov_x (vr_flat_aim_fov 40 measured 42.52) -- the gates carry that much,
# no more, and none of them was widened to make a number pass.
#
# Group 2, the free hand and the held weapon's grip hotspot (vr_flatsight.cpp offers it, vr_twohand.cpp decides). The
# shipped pose is pinned weapon by weapon with the pose number that decides it, and the one knob that moves the
# shotgun's number is put in its range with nothing else touched. That makes the question the round leaves to him --
# keep vr_flat_gun_fwd 5 as he promoted it, narrow the support-pose rule, or move the shotgun's grip hotspot -- a
# measured choice instead of a guess. No engine rule and no vr_flat_* default is changed by this suite.
AGENT=${1:-armor}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"
P="map e1m1; wait60; developer 1; god; notarget; r_norefresh 0; vr_mock_eye_size 256"
run() # run <tag> <script> <filter>: one flatscreen run; its console kept through -Filter, tuning never carried over.
# FLATFIX_DRY=1 re-reads a run already captured in scratch/ instead of starting the game (to re-check the gates
# against numbers already taken; the logs are the same files a real run writes).
{
    local tag=$1 script=$2 filter=$3
    if [ "${FLATFIX_DRY:-0}" = 1 ] && [ -f "$S/$tag.log" ]; then echo "flatfix: dry $tag ($S/$tag.log)"; return 0; fi
    bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Low \
        -Script "$script; wait10; tune_reset; toggleconsole; quit" -Filter "$filter" > "$S/$tag.log" 2>&1
}
# ease <log> <n> -> "wide tight frames seconds" for the nth time the run pressed the ironsight: the field of view
# before the press, the narrowest it reached, how many drawn frames the closing took, and its own seconds. A press
# cycle is the rising edge of the debug line's "ads on", which stays on for every frame the ironsight is held.
ease()
{
    awk -v want="$2" '
    /^flatsight: / {
        n = split($0, a, " "); t = -1; f = -1; on = ""; adseen = 0
        for (j = 1; j <= n; j++) {
            if (t < 0 && a[j] == "t") { t = a[j + 1] + 0; continue }
            if (f < 0 && a[j] == "fov") { f = a[j + 1] + 0; continue }
            if (!adseen && a[j] == "ads") { on = a[j + 1]; adseen = 1 }
        }
        if (t < 0 || f < 0) next
        if (on == "on" && prevOn != "on") { cyc++; wide[cyc] = f; minv[cyc] = f; frames[cyc] = 0; started[cyc] = 0; closed[cyc] = 0 }
        prevOn = on
        c = cyc
        if (c < 1) next
        if (on == "off") { closed[c] = 1; next }
        if (closed[c]) next
        if (!started[c] && f < wide[c] - 0.5) { started[c] = 1; frames[c] = 1; t0[c] = t; tlast[c] = t; minv[c] = f }
        else if (started[c] && f < minv[c] - 0.001) { frames[c]++; minv[c] = f; tlast[c] = t }
    }
    END {
        if (cyc + 0 < want + 0) { print "0 0 0 0"; exit }
        print wide[want] + 0, minv[want] + 0, frames[want] + 0, (tlast[want] - t0[want]) + 0
    }' "$1"
}
depth() { awk -v w="$1" 'BEGIN{split(w,x," "); printf "%.2f", x[1] - x[2]}'; }
sec() { awk -v w="$1" 'BEGIN{split(w,x," "); printf "%.4f", x[4]}'; }
fr() { awk -v w="$1" 'BEGIN{split(w,x," "); print x[3]}'; }
# inq <value> <lo> <hi>: is a printed number inside a gate -- silent, its status is the answer, so a chain of them
# reads as one word: `check $(inq a 1 2 && inq b 3 4 && echo 1 || echo 0) "..."`.
inq() { awk -v v="$1" -v lo="$2" -v hi="$3" 'BEGIN{exit !(v + 0 >= lo + 0 && v + 0 <= hi + 0)}'; }
# fwdOf <log|text> <n>: the nth "hotspot: ... (fwd F, ..." line's F, with its sign.
fwdOf() { awk '/hotspot: / { if (k++ == '"$2"') { if (match($0, /fwd -?[0-9.]+/)) print substr($0, RSTART + 4, RLENGTH - 4) } }' <<<"$1"; }
gate() { local lo=$1 hi=$2 v; shift 2; for v in "$@"; do inq "$v" "$lo" "$hi" || return 1; done; return 0; }
# head1 <text>: the phrase before its first bracket, one line (a status line the console wrapped).
head1() { awk -v s="$1" 'BEGIN{ sub(/^ +/, "", s); i = index(s, " ("); print (i ? substr(s, 1, i - 1) : s) }'; }
# keep1 <text>: a status line up to and including the bracket the console wrapped after (a "two hands:" print ends
# with "(... a second hand can take)", the debug's own brackets after that belong to the next line).
keep1() { awk -v s="$1" 'BEGIN{ if (match(s, /\) \(/)) print substr(s, 1, RSTART); else print s }'; }

# ---- 1. what the shipped pair does, measured against the numbers the same run's vr_flatplay_status prints.
# A press held 60 frames, let go, then 60 more: long enough for a 0.15 s travel to close and for the release to show.
run ease_ship "$P; vr_flatplay_debug 2; impulse 154; wait20; vr_flat_aim 1; wait60; vr_flat_aim 0; wait60; vr_flatplay_status" \
    '^flatsight: |^  ironsight: |^    held weapon |^    an empty hand |^  his eye on the screen:'
e1=$(ease "$S/ease_ship.log" 1)
istat=$(grep -a '^  ironsight: ' "$S/ease_ship.log" | head -1)
poseSay=$(grep -a '^    held weapon ' "$S/ease_ship.log" | head -1)
fovSay=$(awk -v s="$istat" 'BEGIN{ if (match(s, /vr_flat_aim_fov [0-9.]+/)) print substr(s, RSTART + 16, RLENGTH - 16) }')
timeSay=$(awk -v s="$istat" 'BEGIN{ if (match(s, /taken over [0-9.]+/)) print substr(s, RSTART + 11, RLENGTH - 11) }')
eyeSay=$(grep -a '^  his eye on the screen:' "$S/ease_ship.log" | head -1)
fovLo=$(awk -v f="$fovSay" 'BEGIN{printf "%.2f", f - 4}'); fovHi=$(awk -v f="$fovSay" 'BEGIN{printf "%.2f", f + 4}')
tLo=$(awk -v t="$timeSay" 'BEGIN{printf "%.3f", t - 0.05}'); tHi=$(awk -v t="$timeSay" 'BEGIN{printf "%.3f", t + 0.05}')
check $(inq "$(depth "$e1")" "$fovLo" "$fovHi" && echo 1 || echo 0) \
    "shipped ironsight ease: the view came down $(depth "$e1") deg across fov_x, the vr_flat_aim_fov $fovSay the mode prints for it in the same run ($e1 = wide tight frames seconds)"
check $(inq "$(sec "$e1")" "$tLo" "$tHi" && inq "$(fr "$e1")" 5 30 && echo 1 || echo 0) \
    "shipped ironsight ease: its travel took $(sec "$e1") s of the game's own clock, the vr_flat_aim_time $timeSay it prints for it, across $(fr "$e1") drawn frames"
check $([ -n "$poseSay" ] && [ -n "$fovSay" ] && [ -n "$timeSay" ] && echo 1 || echo 0) \
    "the pose and the pair the run measured: $(head1 "$poseSay") / $(head1 "$istat") / $(head1 "$eyeSay")"

# ---- 2. the travel takes the seconds the cvar says: four values in one run, one press cycle each.
# Cycle order = the order the tunes are given: 0.05, 0.25, 1, 0. A press is let go 60 frames after it is put on, so the
# 1 s travel is counted while it is still closing (60 frames at the ~72 fps these runs draw = 0.83 s of its second).
CYC="vr_flat_aim 1; wait60; vr_flat_aim 0; wait40"
run ease_time "$P; vr_flatplay_debug 2; impulse 154; wait20; tune vr_flat_aim_fov 12; tune vr_flat_aim_time 0.05; $CYC; tune vr_flat_aim_time 0.25; $CYC; tune vr_flat_aim_time 1; $CYC; tune vr_flat_aim_time 0; $CYC" \
    '^flatsight: '
c05=$(ease "$S/ease_time.log" 1); c25=$(ease "$S/ease_time.log" 2); c1=$(ease "$S/ease_time.log" 3); c0=$(ease "$S/ease_time.log" 4)
check $(inq "$(sec "$c05")" 0.03 0.07 && echo 1 || echo 0) "vr_flat_aim_time 0.05, its floor: $(sec "$c05") s over $(fr "$c05") frames ($c05)"
check $(inq "$(sec "$c25")" 0.20 0.30 && echo 1 || echo 0) "vr_flat_aim_time 0.25, the value flatkeys' gates were written against: $(sec "$c25") s over $(fr "$c25") frames ($c25)"
check $(inq "$(sec "$c1")" 0.70 1.05 && echo 1 || echo 0) "vr_flat_aim_time 1, its top: $(sec "$c1") s of travel over $(fr "$c1") frames before the press was let go 60 frames after it ($c1)"
check $(inq "$(sec "$c0")" 0 0.03 && inq "$(fr "$c0")" 0 2 && echo 1 || echo 0) "vr_flat_aim_time 0 is at once: $(sec "$c0") s, $(fr "$c0") frame ($c0)"
check $(awk -v a="$(fr "$c05")" -v b="$(fr "$c25")" -v c="$(fr "$c1")" 'BEGIN{exit !(a + 0 >= 2 && a + 0 < b + 0 && b + 0 < c + 0)}' && echo 1 || echo 0) \
    "the ramp's frame count follows the cvar too: $(fr "$c05") frames at 0.05 s, $(fr "$c25") at 0.25 s, $(fr "$c1") at 1 s"
check $(inq "$(depth "$c25")" 8 16 && echo 1 || echo 0) "vr_flat_aim_fov 12 with a travel that finishes: $(depth "$c25") deg taken off fov_x ($c25)"

# ---- 3. the degrees come off the view the way the cvar says: three values in one run, the travel pinned at 0.25 s.
run ease_fov "$P; vr_flatplay_debug 2; impulse 154; wait20; tune vr_flat_aim_time 0.25; tune vr_flat_aim_fov 0; $CYC; tune vr_flat_aim_fov 12; $CYC; tune vr_flat_aim_fov 45; $CYC" \
    '^flatsight: '
d0=$(ease "$S/ease_fov.log" 1); d12=$(ease "$S/ease_fov.log" 2); d45=$(ease "$S/ease_fov.log" 3)
check $(inq "$(depth "$d0")" 0 2 && echo 1 || echo 0) "vr_flat_aim_fov 0 takes nothing off the view: $(depth "$d0") deg ($d0)"
check $(inq "$(depth "$d12")" 8 16 && echo 1 || echo 0) "vr_flat_aim_fov 12: $(depth "$d12") deg ($d12)"
check $(inq "$(depth "$d45")" 41 49 && echo 1 || echo 0) "vr_flat_aim_fov 45, its top: $(depth "$d45") deg ($d45)"
check $(awk -v a="$(depth "$d0")" -v b="$(depth "$d12")" -v c="$(depth "$d45")" 'BEGIN{exit !(a + 0 < b + 0 && b + 0 < c + 0)}' && echo 1 || echo 0) \
    "the depth follows the cvar in order: $(depth "$d0") < $(depth "$d12") < $(depth "$d45") deg as vr_flat_aim_fov goes 0, 12, 45"

# ---- 4. the grip at the pose the game ships: the shotgun (154), the rocket launcher (158), the proximity gun (159).
GRIPTAIL="vr_debug_2h_grip 1; impulse 154; wait40; vr_flatplay_status; wait5; impulse 158; wait40; vr_flatplay_status; wait5; impulse 159; wait60; vr_flatplay_status"
run grip_ship "$P; $GRIPTAIL" '^  two hands:|^  grip:|^  hotspot:|^2h grip: |^    an empty hand '
hs=$(grep -a '^  hotspot: ' "$S/grip_ship.log")
hsList=$(sed 's/^  hotspot: //' <<<"$hs" | tr '\n' ';')
nhs=$(grep -ac '^  hotspot: ' "$S/grip_ship.log")
f1=$(fwdOf "$hs" 0); f2=$(fwdOf "$hs" 1); f3=$(fwdOf "$hs" 2)
holds=$(grep -ac 'on its grip hotspot (held holding: helpKind 1' "$S/grip_ship.log")
check $([ "$nhs" = 3 ] && gate 5 20 "$f1" "$f2" "$f3" && echo 1 || echo 0) \
    "at the shipped pose all three guns put the grip hotspot nearest the free hand ahead of the held hand: $hsList"
check $([ "$holds" = 3 ] && grep -qa '2h grip: off hand took it' "$S/grip_ship.log" && echo 1 || echo 0) \
    "and vr_twohand takes all three holds: $holds of 3 status prints say the off hand is on its grip hotspot (helpKind 1)"

# ---- 5. the one knob that moves the shotgun's number: vr_flat_gun_fwd, inside its range, nothing else touched.
run grip_gun17 "$P; tune vr_flat_gun_fwd 17; $GRIPTAIL" '^  two hands:|^  grip:|^  hotspot:|^2h grip: |^    an empty hand '
hs17=$(grep -a '^  hotspot: ' "$S/grip_gun17.log")
nhs17=$(grep -ac '^  hotspot: ' "$S/grip_gun17.log")
g1=$(fwdOf "$hs17" 0); g2=$(fwdOf "$hs17" 1)
left=$(grep -ac "off hand where the mode's pose puts it" "$S/grip_gun17.log")
two17=$(keep1 "$(grep -a '^  two hands: ' "$S/grip_gun17.log" | head -1)")
poseA=$(grep -a '^    an empty hand ' "$S/grip_ship.log" | head -1)
poseB=$(grep -a '^    an empty hand ' "$S/grip_gun17.log" | head -1)
check $([ "$nhs17" = 3 ] && inq "$g1" -8 -1 && [ "$left" = 1 ] && echo 1 || echo 0) \
    "vr_flat_gun_fwd 17 alone puts the shotgun's grip behind the held hand and the mode leaves it to its own support pose: $(sed 's/^  //' <<<"$hs17" | head -1) -- $two17"
check $(awk -v a="$f1" -v b="$g1" 'BEGIN{exit !((a + 0) - (b + 0) >= 10)}' && echo 1 || echo 0) \
    "that move is $(awk -v a="$f1" -v b="$g1" 'BEGIN{printf "%.1f", (a + 0) - (b + 0)}') units along vr_flat_gun_fwd's own 12 (17 -> 5): the shotgun's hotspot reads fwd $g1 at 17 and fwd $f1 at the shipped 5"
check $([ "$(grep -ac 'on its grip hotspot (held holding: helpKind 1' "$S/grip_gun17.log")" = 2 ] && gate 5 20 "$g2" "$f2" && echo 1 || echo 0) \
    "and it is the shotgun alone: at the same 17 the rocket launcher still stands ahead (fwd $g2, $f2 at 5) and its hold is still taken, as the proximity gun's is"
check $([ -n "$poseA" ] && [ "$poseA" = "$poseB" ] && echo 1 || echo 0) \
    "no support pose was changed to show it: both runs print the same place for an empty and a supporting hand $(sed 's/^ *//' <<<"$poseA")"

verdict
exit $fail
