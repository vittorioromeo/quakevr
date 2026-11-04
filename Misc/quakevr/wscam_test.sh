#!/bin/bash
# wscam_test.sh <agent> -- the Weapon Studio's drawn camera (Quake/vr/vr_weaponstudio.cpp's camera block, hooked into
# Quake/view.c's V_CalcRefdef), headless flatscreen (-flatscreenmode: the window is a desktop tool). One run opens its
# window on a held shotgun (two hands on it) and, in order: prints the camera while it is `off` (the default); flies it
# out with the game's own +forward held for 120 frames (~2 s at 60 fps; in_forward is the button W is to a hand);
# prints it; measures the held weapon with `align both`; stands the camera 60 units off his eyes and measures the
# weapon again; stands it 200 units straight ahead and drags it (`camera drag 60 0` -- the very orbit a middle-drag
# calls); stands it at 80/40/20; puts it back with `vr_weaponstudio camera reset`, waits, and prints it; closes the
# window; and asks the tool for one more print, which is where the run ends.
#
# The numbers come from the tool's own print line and from the mode's existing probes -- flatview (the drawn weapon in
# the drawn camera's frame), flatarm (a drawn hand against the arm its body poses for it), flatcam (the player he
# really stands at) -- with vr_flatplay_debug 2. Nothing was added to instrument this. The console wraps a print that
# runs past its width, and a wrapped half can begin with a word, so records are re-joined on their tags before reading.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=${1:-fixgame3}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
source "$(dirname "$0")/qvrtest.sh"
HERE=$(dirname "$0"); mkdir -p "$HERE/scratch"

log=$HERE/scratch/wscam_cam.log
bash $KIT/run.sh $AGENT -Low -ExtraArgs "-flatscreenmode" -Script \
"map e1m1;wait60;god;notarget;developer 1;vr_flatplay_debug 2;vr_weaponstudio_fly_speed 120;impulse 154;wait90;vr_weaponstudio open;wait90;vr_weaponstudio camera print;vr_weaponstudio align both;wait30;+forward;wait120;-forward;vr_weaponstudio camera print;vr_weaponstudio camera 60 0 0;vr_weaponstudio align both;vr_weaponstudio camera 200 0 0;vr_weaponstudio camera drag 60 0;vr_weaponstudio camera 80 40 20;vr_weaponstudio camera reset;wait20;vr_weaponstudio camera print;vr_weaponstudio close;wait60;vr_weaponstudio camera print" \
  -Filter '^(weaponstudio|flat)' -QuitOn 'its camera belongs' -Timeout 420 > "$log" 2>&1

check $(grep -qa "^weaponstudio: open " "$log" && grep -qa "^weaponstudio: closed" "$log" && echo 1 || echo 0) "its window opened on a held weapon and closed again (its camera belongs to the open window)"

# The records, then their numbers. cam counts its print lines in the order the script asked for them: 1 off, 2 flown by
# the keys, 3 at 60 units, 4 at 200 units, 5 after the drag, 6 at 80/40/20, 7 after the reset. iv counts the frames
# between them (1 is the window open with its camera off; p is the frames after it closed), and keeps the last flatview
# / flatarm / flatcam read of each stretch.
vals=$(gawk '
function num(s) { return s + 0 }
function cam(line) {
    n++
    if (match(line, /eye (-?[0-9.]+)[ ]+(-?[0-9.]+)[ ]+(-?[0-9.]+)[ ]+turn (-?[0-9.]+)[ ]+(-?[0-9.]+)/, m)) {
        print "cam" n "_ex", num(m[1]); print "cam" n "_ey", num(m[2]); print "cam" n "_ez", num(m[3])
        print "cam" n "_pitch", num(m[4]); print "cam" n "_yaw", num(m[5])
    }
    if (match(line, /off his head (-?[0-9.]+)/, m)) print "cam" n "_off", num(m[1])
    if (match(line, /his head (-?[0-9.]+)[ ]+(-?[0-9.]+)[ ]+(-?[0-9.]+)/, m)) {
        print "cam" n "_hx", num(m[1]); print "cam" n "_hy", num(m[2]); print "cam" n "_hz", num(m[3])
    }
    if (match(line, /game.s eye (-?[0-9.]+)[ ]+(-?[0-9.]+)[ ]+(-?[0-9.]+)/, m)) {
        print "cam" n "_gx", num(m[1]); print "cam" n "_gy", num(m[2]); print "cam" n "_gz", num(m[3])
    }
    if (match(line, /apart (-?[0-9.]+)/, m)) print "cam" n "_apart", num(m[1])
    if (match(line, /fly (-?[0-9.]+) u\/s/, m)) print "cam" n "_fly", num(m[1])
    print "cam" n "_state", (line ~ /[ ]flown[ ]/ ? "flown" : "off")
}
function probe(line,   k, m) {
    k = closed ? "p" : iv
    if (line ~ /^flatview: /) {
        if (match(line, /local (-?[0-9.]+)[ ]+(-?[0-9.]+)[ ]+(-?[0-9.]+)/, m)) { fa[k] = num(m[1]); nfv[k]++ }
        if (match(line, /[ ]view (-?[0-9.]+)[ ]+(-?[0-9.]+)/, m)) fv[k] = num(m[1])
        return
    }
    if (line ~ /^flatarm: hand main/) {
        if (match(line, /off-wrist (-?[0-9.]+)[ ]+u/, m)) aw[k] = num(m[1])
        if (match(line, /past its arm (-?[0-9.]+)/, m)) pa[k] = num(m[1])
        if (match(line, /shoulder to hand (-?[0-9.]+)/, m)) sh[k] = num(m[1])
        if (match(line, /[ ]ik[ ]+(-?[0-9.]+)[ ]+speed/, m)) ik[k] = num(m[1])
        if (match(line, /[ ]bend (-?[0-9.]+)/, m)) bd[k] = num(m[1])
        na[k]++
        return
    }
    if (line ~ /^flatcam: /) {
        if (match(line, /origin (-?[0-9.]+)[ ]+(-?[0-9.]+)[ ]+(-?[0-9.]+)/, m)) { ox[k] = num(m[1]); oy[k] = num(m[2]); nz[k]++ }
        return
    }
    if (line ~ /^weaponstudio align: main hand/) {
        if (match(line, /sight line is (-?[0-9.]+) deg .* shot pitch (-?[0-9.]+), shot[ ]+yaw (-?[0-9.]+) .* is (-?[0-9.]+) cm off that line, (-?[0-9.]+) cm/, m)) {
            al_n++
            for (i = 1; i <= 5; i++) al[al_n "_" i] = num(m[i])
            al[al_n "_iv"] = k
        }
        return
    }
}
function eat(line) {
    if (line ~ /^weaponstudio: open /)  { iv = 1; return }
    if (line ~ /^weaponstudio: closed/) { closed = 1; return }
    if (line ~ /^weaponstudio camera /) { cam(line); iv++; return }
    probe(line)
}
{ if ($0 ~ /^(weaponstudio|flat)/) { if (buf != "") eat(buf); buf = $0 } else if (buf != "") buf = buf " " $0; next }
END {
    if (buf != "") eat(buf)
    for (k = 1; k <= 8; k++) {
        if (nfv[k]) { print "fv" k "_x", fa[k]; print "fv" k "_view", fv[k]; print "fv" k "_n", nfv[k] }
        if (na[k])  { print "arm" k "_off", aw[k]; print "arm" k "_past", pa[k]; print "arm" k "_sh", sh[k]; print "arm" k "_ik", ik[k]; print "arm" k "_bend", bd[k]; print "arm" k "_n", na[k] }
        if (nz[k])  { print "his" k "_x", ox[k]; print "his" k "_y", oy[k]; print "his" k "_n", nz[k] }
    }
    if (nfv["p"]) { print "fvp_x", fa["p"]; print "fvp_view", fv["p"]; print "fvp_n", nfv["p"] }
    if (na["p"])  { print "armp_off", aw["p"]; print "armp_past", pa["p"]; print "armp_ik", ik["p"]; print "armp_n", na["p"] }
    if (nz["p"])  { print "hisp_x", ox["p"]; print "hisp_y", oy["p"]; print "hisp_n", nz["p"] }
    for (a = 1; a <= al_n; a++) print "align", al[a "_iv"], al[a "_1"], al[a "_2"], al[a "_3"], al[a "_4"], al[a "_5"]
}
' "$log")
v() { grep -m1 -E "^$1 " <<<"$vals" | cut -d' ' -f2-; }
far() { gawk -v a="$1" -v b="$2" -v t="$3" 'BEGIN { d = a - b; if (d < 0) d = -d; print (d <= t) ? 1 : 0 }'; }
yes() { [ "$1" = "1" ] && echo 1 || echo 0; }

echo "  its camera, as its own print says:"; grep -aE "^weaponstudio camera " "$log" | sed 's/^/   /'

# ---- 1. `off` is his own eyes, and it is what the window opens with --------------------------------------------
check $(yes "$([ "$(v cam1_state)" = "off" ] && echo 1 || echo 0)") "its camera is off when the window opens -- its print says off his eyes, not flown"
check $([ -n "$(v cam1_apart)" ] && far "$(v cam1_apart)" 0 0.1) "with its camera off the drawn view is his own eye: apart $(v cam1_apart) -- the little that the mode's own eye sits off his head eye, which this feature neither adds nor changes"
check $(far "$(v cam1_off)" 0 0.001) "and its own eye stands on his head: off his head $(v cam1_off) (vr_weaponstudio camera print)"

# ---- 2. its keys fly it out; his own walking is paused for those frames --------------------------------------
check $(yes "$([ "$(v cam2_state)" = "flown" ] && echo 1 || echo 0)") "+forward held for 120 frames flew it out (in_forward is the game's own button; W is the same to a hand)"
check $(awk -v d="$(v cam2_off)" 'BEGIN { print (d >= 40) ? 1 : 0 }') "about 2 s at the default speed puts it 40 or more units off his head: $(v cam2_off) units, and its print says how fast it flies: $(v cam2_fly) u/s"
check $(far "$(v cam1_fly)" 120 0.001) "vr_weaponstudio_fly_speed is there and at its default: $(v cam1_fly) units a second"
check $(far "$(v cam2_hx)" "$(v cam1_hx)" 0.1 && far "$(v cam2_hy)" "$(v cam1_hy)" 0.1 && far "$(v cam2_hz)" "$(v cam1_hz)" 0.1 && echo 1 || echo 0) "he did not move while its window was open: his head stood at $(v cam1_hx) $(v cam1_hy) $(v cam1_hz) and still stood at $(v cam2_hx) $(v cam2_hy) $(v cam2_hz) through the fly"
# flatview measures the drawn weapon in the drawn camera's frame. Out past the mode's keep-near distance the weapon
# stays with the body, so the flown eye finds it behind it instead of in front of it.
check $(yes "$([ -n "$(v fv1_x)" ] && far "$(v fv1_x)" 4 1 && echo 1 || echo 0)") "before the fly the held weapon hangs $(v fv1_x) units ahead of the eye it is drawn for (flatview)"
check $(awk -v a="$(v fv1_x)" -v b="$(v fv2_x)" 'BEGIN { print (b < a - 40) ? 1 : 0 }') "flown out, that same weapon is $(v fv2_x) units from the eye: it stayed with the body while the view moved away from it"

# ---- 3. the hands stay on the body's arms while the view is flown out (the mode's own probes) -----------------
check $(yes "$([ -n "$(v arm1_off)" ] && [ -n "$(v arm2_off)" ] && echo 1 || echo 0)") "its hands were measured before the fly and during it (flatarm with vr_flatplay_debug 2: the mode's own probe, nothing added)"
check $(far "$(v arm2_off)" "$(v arm1_off)" 0.2 && far "$(v arm2_past)" "$(v arm1_past)" 0.2 && far "$(v arm2_ik)" "$(v arm1_ik)" 0.2 && far "$(v arm2_sh)" "$(v arm1_sh)" 0.2 && far "$(v arm2_bend)" "$(v arm1_bend)" 0.5 && echo 1 || echo 0) "while the view is flown out the drawn hand is still on its arm: off-wrist $(v arm1_off) to $(v arm2_off), past its arm $(v arm1_past) to $(v arm2_past), shoulder to hand $(v arm1_sh) to $(v arm2_sh), ik $(v arm1_ik) to $(v arm2_ik), bend $(v arm1_bend) to $(v arm2_bend)"
check $(far "$(v arm2_ik)" 0 1) "and the shipped arms solve leaves it in its socket while the camera is out: ik $(v arm2_ik)"

# ---- 4. the aligner does not care where the view is: the same measures either side of a 60-unit fly ----------
a1=$(grep -m1 -aE "^align [0-9p]+ " <<<"$vals" | cut -d' ' -f3-)
a2=$(grep -aE "^align [0-9p]+ " <<<"$vals" | tail -1 | cut -d' ' -f3-)
check $(yes "$([ -n "$a1" ] && [ "$(grep -acE '^align [0-9p]+ ' <<<"$vals")" -ge 2 ] && echo 1 || echo 0)") "vr_weaponstudio align both measured the held weapon with the camera off, and again with it 60 units off his eyes"
check $(gawk -v a="$a1" -v b="$a2" 'BEGIN { n = split(a, x, " "); m = split(b, y, " "); if (n != m || n < 5) { print 0; exit } for (i = 1; i <= n; i++) { d = x[i] - y[i]; if (d < 0) d = -d; if (d > 0.051) { print 0; exit } } print 1 }') "its measures are the same either side of a 60-unit fly -- sight line, shot pitch, shot yaw, muzzle off the sight line, muzzle off the model's own: $(a1) against $(a2)"

# ---- 5. the orbit: a drag round him turns the view, and it stands on him -------------------------------------
dxy=$(gawk -v ex="$(v cam4_ex)" -v ey="$(v cam4_ey)" -v ez="$(v cam4_ez)" -v hx="$(v cam4_hx)" -v hy="$(v cam4_hy)" -v hz="$(v cam4_hz)" 'BEGIN {
    dx = hx - ex; dy = hy - ey; dz = hz - ez; pi = 3.14159265358979
    print atan2(dy, dx) * 180 / pi, -atan2(dz, sqrt(dx * dx + dy * dy)) * 180 / pi }')
check $(awk -v a="$(v cam4_yaw)" -v b="$(v cam5_yaw)" 'BEGIN { d = a - b; if (d < 0) d = -d; print (d > 1) ? 1 : 0 }') "camera drag <right> <down> -- the very orbit a middle-drag calls -- turned the view 200 units off his head: yaw $(v cam4_yaw) to $(v cam5_yaw)"
check $(yes "$([ -n "$dxy" ] && gawk -v y="$(v cam5_yaw)" -v p="$(v cam5_pitch)" -v e="$dxy" 'BEGIN { split(e, t, " "); dy = y - t[1]; while (dy > 180) dy -= 360; while (dy < -180) dy += 360; dp = p - t[2]; if (dy < 0) dy = -dy; if (dp < 0) dp = -dp; print (dy <= 1.5 && dp <= 1.5) ? 1 : 0 }')") "and an orbit keeps him in it: after the drag its turn $(v cam5_pitch) $(v cam5_yaw) is the direction from its eye to his head, $(v cam5_off) units away"

# ---- 6. the scripting path: numbers off his eyes, in his own view's frame ------------------------------------
check $(far "$(v cam6_off)" 91.65 0.5) "vr_weaponstudio camera 80 40 20 stands it ahead, its right and up in his view's own frame: $(v cam6_off) units off his eyes against 91.65 measured"

# ---- 7. reset, and the close, give the view back to his eyes ---------------------------------------------
check $(yes "$([ "$(v cam7_state)" = "off" ] && far "$(v cam7_apart)" 0 0.1 && echo 1 || echo 0)") "vr_weaponstudio camera reset brought the view back to his eye without closing the window: apart $(v cam7_apart)"
check $(grep -qa "camera back to his eyes" "$log" && echo 1 || echo 0) "and it said so (its reset prints; the window's top row asks for the same thing)"
check $(yes "$([ -n "$(v fvp_x)" ] && far "$(v fvp_x)" "$(v fv1_x)" 0.5 && far "$(v fvp_view)" "$(v fv1_view)" 0.5 && echo 1 || echo 0)") "closing the window returned the drawn view to his eyes: the held weapon's place in it $(v fvp_x) units ahead as before the fly ($(v fv1_x)), its turn $(v fvp_view) as before ($(v fv1_view))"
check $(far "$(v hisp_x)" "$(v his1_x)" 0.2 && far "$(v hisp_y)" "$(v his1_y)" 0.2 && echo 1 || echo 0) "and he is standing where he stood before its window opened: $(v his1_x) $(v his1_y) then $(v hisp_x) $(v hisp_y) -- a hold given back, not taken"
check $(grep -qa "its camera belongs to the open window" "$log" && echo 1 || echo 0) "and after the close the tool refuses to move a camera any more (vr_weaponstudio close gives the game back)"

verdict
exit $fail
