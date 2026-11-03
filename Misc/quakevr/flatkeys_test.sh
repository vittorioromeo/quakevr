#!/bin/bash
# flatkeys_test.sh <agent> -- flatscreen play's keys (-flatscreenmode; docs/vr-port/FLATPLAY.md, "Keys"), headless.
# Every case runs twice over where it matters: once with -flatscreenmode, once without, because the keys belong to the
# flatscreen player alone and a headset player keeps his own ways (Left Ctrl, F, G and the box taken at once are the
# flatscreen's: the game code says so and the engine does nothing). Prints PASS/FAIL, exits 1 on a failure.
#   crouch  Left Ctrl holds and releases (+flatcrouch / -flatcrouch, the console's way): vr_flatkeys_test 1 read standing,
#           held and let go again — his eyes come down vr_flat_crouch_drop (18 units) while the key is held and go back to
#           where they were when it is released, and the crouched box (vr_crouch_hull) follows the key: held crouched, let
#           go standing again, since his eyes stand under the tallest crouched box's top whatever he does. Without the
#           flag the same key does nothing: the game code answers "flat play off".
#   torch   F switches the flashlight (the engine's vr_flashlight_toggle): worn on the head and lit at the start, put out
#           by F (the packed state read back: bit 0 clear), lit again by F.
#   drop    G (impulse 47, weapons.qc, VR_Forcegrab_FlatDrop): the axe in the main hand leaves the hand the way the game
#           drops a weapon ("flat drop:", then the hand weaponless). Without the flag the impulse is refused the same way.
#   collect a box grabbed with E (impulse 46) is collected as soon as it is grabbed — vr_carry_autocollect 0 and the box
#           ahead, taken at once ("carry: taken at once", "You got the shells"). In the headset, with the same setting off,
#           a grabbed box is held and not taken (vr_carry_autocollect stays the headset player's own setting).
#   ctrl    Left Ctrl is the plain game's firing key (Quake/default_cfg.h: bind CTRL +attack) and the flatscreen player's
#           crouch key, so holding it would crouch and fire at once. While flatscreen play runs the mode holds the key
#           itself: bindlist shows CTRL bound to +flatcrouch and nothing driving +attack. The plain game's bind comes back
#           the frame the mode is not running, and every time the engine is about to write its key bindings out
#           (VR_FlatKeysForConfigWrite in Quake/host.c: quitting, the menu closed with a setting changed, host_writeconfig),
#           so the config on disk keeps CTRL "+attack" and a plain run after a flatscreen one is bound as always.
#           vr_flat_crouch_fire 1: Ctrl keeps its own bind, so it fires as it crouches.
AGENT=${1:-fixgame3}; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
source "$(dirname "$0")/qvrtest.sh"
S=$TREE/scratch; mkdir -p "$S"
FLAT=-ExtraArgs\ -flatscreenmode
P="map e1m1; wait60; developer 1; god; notarget"
# The item the tests put ahead of him: a shells box, held where it is spawned (vr_test_spawn_hold), at 48 units.
BOX="vr_test_spawn_dist 48; vr_test_spawn_hold 1; vr_test_spawn 101; impulse 241; wait10"
# The headset's own way of taking it: the mock hand's grip (the pattern holsteritems_test.sh uses).
GRIP="vr_test_spawn_hold 1; vr_mock_hand main 0.25 1.1 -0.3 0 0 0; wait5; +grabmain; vr_mock_button main grip 1; wait2; vr_test_spawn 101; impulse 241; wait10; vr_mock_button main grip 0; -grabmain; wait20"
run() { local tag=$1 flat=$2 script=$3 filter=$4; local a=(); [ "$flat" = flat ] && a=(-ExtraArgs "-flatscreenmode")
        bash $KIT/run.sh $AGENT "${a[@]}" -Low -Script "$script; wait10; toggleconsole; quit" -Filter "$filter" > "$S/$tag.log" 2>&1; }

# 1. crouch: down while the key is held, back up when it is released.
run crouchflat flat "$P; vr_flatkeys_test 1; +flatcrouch; wait40; vr_flatkeys_test 1; -flatcrouch; wait40; vr_flatkeys_test 1" "^flatkeys:"
eyes=$(grep -a "^flatkeys: crouched" "$S/crouchflat.log" | sed 's/.*eye \([0-9.]*\).*/\1/')
n=$(echo "$eyes" | grep -c .)
top=$(echo "$eyes" | head -1); low=$(echo "$eyes" | sort -g | head -1); back=$(echo "$eyes" | tail -1)
deep=$(grep -ac "^flatkeys: crouched 1 stand 0\." "$S/crouchflat.log")
check $([ "$n" = 3 ] && awk -v a="$top" -v b="$low" -v c="$back" 'BEGIN{exit !((a-b)>=15 && c==a)}' && echo 1 || echo 0) \
    "crouch: eyes $top -> $low -> $back units, crouched while held ($deep of 3 readings), the drop at least 15 of the 18 vr_flat_crouch_drop asks for"
# The crouched box follows the key, not his eyes' height: a flatscreen player's eyes stand at Quake's viewheight, which
# is under the tallest crouched box's top, so vr_hull.cpp is asked with his key (flatscreenCrouch) -- held crouched,
# let go standing again (vr_crouch_speed: his full speed) as long as his box fits where he is.
cro=$(grep -a "^flatkeys: crouched" "$S/crouchflat.log" | sed 's/^flatkeys: crouched \([01]\).*/\1/')
c0=$(echo "$cro" | sed -n 1p); c1=$(echo "$cro" | sed -n 2p); c2=$(echo "$cro" | sed -n 3p)
check $([ "$cro" = "$(printf '0\n1\n0')" ] && echo 1 || echo 0) \
    "crouch: the crouched box follows the key (vr_crouch_hull): $c0 standing, $c1 held, $c2 let go -- up again, at his full speed"
check $(grep -qa "torch lit 1" "$S/crouchflat.log" && echo 1 || echo 0) "crouch: the run itself is flatscreen (the torch worn and lit at the start)"
run crouchvr vr "$P; +flatcrouch; wait40; vr_flatkeys_test 1" "^flatkeys:"
check $(grep -qa "flat play off" "$S/crouchvr.log" && ! grep -qa "^flatkeys: crouched" "$S/crouchvr.log" && echo 1 || echo 0) \
    "crouch in the headset: the key does nothing (the game code: flat play off)"

# 2. the flashlight: F out, F back on.
run torchflat flat "$P; vr_flashlight_toggle; wait20; vr_flatkeys_test 1; wait10; vr_flashlight_toggle; wait20; vr_flatkeys_test 1" "^flatkeys: carry"
lit=$(grep -ao "torch lit [01]" "$S/torchflat.log" | sed 's/torch lit //')
first=$(echo "$lit" | head -1); last=$(echo "$lit" | tail -1); cnt=$(echo "$lit" | grep -c .)
check $([ "$cnt" = 2 ] && [ "$first" = 0 ] && [ "$last" = 1 ] && echo 1 || echo 0) \
    "torch: F puts it out (packed state bit 0 clear) and F lights it again ($first then $last)"
run torchvr vr "$P; vr_flashlight_toggle; wait20; vr_flashlight_status; wait10; toggleconsole; quit" "flashlight"
check $(grep -qa "flashlight" "$S/torchvr.log" && echo 1 || echo 0) "torch: the same command in the headset (the head's own torch, untouched by the flatscreen's gate)"

# 3. G drops what a hand holds, and refuses in the headset.
run dropflat flat "$P; impulse 152; wait15; vr_flatkeys_test 2; wait20; vr_flatplay_status" "^(flatkeys|flat drop):"
held=$(grep -a "^flatkeys: G sent" "$S/dropflat.log" | sed 's/.*held weapon \([0-9.]*\).*/\1/')
gone=$(grep -ac "^flatkeys: dropped $held: main hand weaponless 1" "$S/dropflat.log")
check $([ -n "$held" ] && [ "$held" != 0 ] && [ "$gone" = 1 ] && grep -qa "^flat drop:" "$S/dropflat.log" && echo 1 || echo 0) \
    "drop: G frees the main hand (it held weapon $held, then weaponless 1, with the game's own flat drop: line)"
run dropvr vr "$P; impulse 152; wait15; vr_flatkeys_test 2; wait20" "^(flatkeys|flat drop):"
check $(grep -qa "flat play off" "$S/dropvr.log" && ! grep -qa "^flat drop:" "$S/dropvr.log" && echo 1 || echo 0) \
    "drop in the headset: impulse 47 is the flatscreen's and does nothing"

# 4. a box grabbed with E is collected at once, flatscreen; a grabbed box is only held in the headset.
run boxflat flat "$P; vr_carry_autocollect 0; $BOX; impulse 46; wait25; vr_flatkeys_test 1" "^(flatkeys:|carry:|You got)"
got=$(grep -ac "You got the shells" "$S/boxflat.log"); took=$(grep -ac "carry: taken at once" "$S/boxflat.log")
check $([ "$got" = 1 ] && [ "$took" = 1 ] && echo 1 || echo 0) \
    "collect: E's grab takes the box at once with the saved setting off (taken at once $took, got $got)"
run boxvr vr "$P; vr_carry_autocollect 0; vr_test_spawn_dist 48; $GRIP; vr_holster_zones_report" "^(carry:|You got|zone hand main:)"
check $(! grep -qa "You got the shells" "$S/boxvr.log" && ! grep -qa "carry: taken at once" "$S/boxvr.log" && echo 1 || echo 0) \
    "collect in the headset: with the setting off a grabbed box is held, not taken (vr_carry_autocollect stays his setting)"

# 5. Left Ctrl: the crouch key fires nothing, and the plain game's CTRL bind is what the config keeps.
# vr_flatplay_debug 1 first: the whole script lands in autoexec.cfg, run before the map and its waits, so the setting is in
# by the mode's second frame -- and the mode prints each frame it takes Ctrl, with what it found on the key.
run ctrlflat flat "vr_flatplay_debug 1; $P; bind CTRL; bindlist; bind CTRL +lookspin; wait40; bind CTRL; vr_flatplay_status" "CTRL"
# Ctrl's bind, read twice: the mode's own, and the mode's again after a `bind CTRL +lookspin` said on the console took it
# away (a two-word bind answers nothing, so the next reading is the mode's asking for its key back on its next frame).
cb=$(grep -a '^"CTRL" = ' "$S/ctrlflat.log" | sed 's/.*= "\(.*\)"/\1/')
c0=$(echo "$cb" | sed -n 1p); c1=$(echo "$cb" | sed -n 2p); cn=$(echo "$cb" | grep -c .)
check $([ "$cn" = 2 ] && [ "$c0" = "+flatcrouch" ] && [ "$c1" = "+flatcrouch" ] \
        && grep -qa '^   CTRL "+flatcrouch"' "$S/ctrlflat.log" && ! grep -qa 'CTRL "+attack"' "$S/ctrlflat.log" \
        && echo 1 || echo 0) \
    "ctrl: flatscreen play holds Ctrl for the crouch alone ($c0, +lookspin said on the console, $c1 the mode's key taken back; bindlist: $(grep -a '^   CTRL ' "$S/ctrlflat.log" | tr -s ' ' | tr '\n' ' '), no bind of Ctrl sends +attack)"
# What the mode found on Ctrl each time it took it: the config's own +attack (its bind line lands over the mode's a frame
# after the mode's first frame), and what the player bound on the console a moment ago. Either is what it hands back.
check $(grep -qa 'flatplay: CTRL `+attack` -> +flatcrouch' "$S/ctrlflat.log" \
        && grep -qa 'flatplay: CTRL `+lookspin` -> +flatcrouch' "$S/ctrlflat.log" && echo 1 || echo 0) \
    "ctrl: the mode remembers what Ctrl held before it took it, each time it took it ($(grep -a 'flatplay: CTRL' "$S/ctrlflat.log" | sed 's/ (.*//' | tr '\n' '; ' ))"
check $(grep -qa 'Ctrl is bound to `+flatcrouch`' "$S/ctrlflat.log" && echo 1 || echo 0) \
    "ctrl: vr_flatplay_status says which bind Ctrl holds now and what vr_flat_crouch_fire would change"
# The config write (writeconfig: Ironwail's name for it, Quake/host.c): the engine writes its key bindings with the mode's
# key handed back, and the file the game reads says so.
run ctrlwrite flat "$P; writeconfig; bind CTRL; exec ironwail.cfg; bind CTRL; wait40; bind CTRL" "CTRL|Wrote"
wb=$(grep -a '^"CTRL" = ' "$S/ctrlwrite.log" | sed 's/.*= "\(.*\)"/\1/')
w0=$(echo "$wb" | sed -n 1p); w1=$(echo "$wb" | sed -n 2p); w2=$(echo "$wb" | sed -n 3p); wn=$(echo "$wb" | grep -c .)
check $([ "$wn" = 3 ] && grep -qa 'Wrote ironwail.cfg' "$S/ctrlwrite.log" && [ "$w0" = "+attack" ] && [ "$w1" = "+attack" ] \
        && [ "$w2" = "+flatcrouch" ] && echo 1 || echo 0) \
    "ctrl: the config keeps the plain game's Ctrl ($w0 as the engine wrote ironwail.cfg, $w1 read back from the file it wrote, $w2 the mode's bind again on its next frame)"
run ctrlvr vr "$P; bind CTRL; bindlist; wait" "CTRL"
check $(grep -qa '^   CTRL "+attack"' "$S/ctrlvr.log" && ! grep -qa 'CTRL "+flatcrouch"' "$S/ctrlvr.log" && echo 1 || echo 0) \
    "ctrl in the headset and in plain play: Ctrl is the plain game's key still (bindlist: $(grep -a '^   CTRL ' "$S/ctrlvr.log" | tr -s ' ' | tr '\n' ' '), the mode never ran so it never took it)"
# vr_flat_crouch_fire 1: Ctrl keeps its own bind, so it fires as it crouches.
run ctrlfire flat "$P; vr_flat_crouch_fire 1; wait40; bind CTRL; wait40; bind CTRL; vr_flatplay_status" "CTRL"
fb=$(grep -a '^"CTRL" = ' "$S/ctrlfire.log" | sed 's/.*= "\(.*\)"/\1/')
fn=$(echo "$fb" | grep -c .); fbad=$(echo "$fb" | grep -c 'flatcrouch')
check $([ "$fn" = 2 ] && [ "$fbad" = 0 ] && grep -qa 'Ctrl is bound to `+attack`' "$S/ctrlfire.log" \
        && echo 1 || echo 0) \
    "ctrl with vr_flat_crouch_fire 1: Ctrl keeps its firing bind both times ($(echo "$fb" | tr '\n' ' ')) and the status says so"

# 6. The look's window in flatscreen play: vr_flat_pitch_up (how far up) and vr_flat_pitch_down (how far down), both of
# them read the plain game's way -- Quake's pitch is positive looking down, so the view stays between -vr_flat_pitch_up
# and +vr_flat_pitch_down. r_pos prints the view's angles on every drawn frame (Quake/gl_rmain.c), so the extremes it
# printed are the window's own edges. cl_pitchspeed is raised so the look reaches its bound inside the waits given: it
# changes how fast the player turns, not where the bound is. Each vr_flatplay_status opens the next phase of a run (its
# first line starts "flatscreen play:" whether the mode ran or not).
LOOK="r_pos 1; cl_pitchspeed 600; wait10; vr_flatplay_status; +lookdown; wait40; -lookdown; wait10; +lookup; wait40; -lookup;"
LOOK="$LOOK centerview; wait10; vr_flat_pitch_up 0; vr_flat_pitch_down 20; vr_flatplay_status; +lookdown; wait40; -lookdown; wait10; +lookup; wait40; -lookup; wait20"
phases() { awk '/^flatscreen play:/{ph++; next} /\(pitch/{for(i=1;i<=NF;i++) if($i=="(pitch") print ph, $(i+1)+0}' "$1" > "$2"; }
limits() { awk -v p="$2" '$1==p{c++; if(mn==""||$2<mn)mn=$2; if(mx==""||$2>mx)mx=$2} END{print mn+0, mx+0, c+0}' "$1"; }
span() { awk '/\(pitch/{for(i=1;i<=NF;i++) if($i=="(pitch"){v=$(i+1)+0; c++; if(mn==""||v<mn)mn=v; if(mx==""||v>mx)mx=v}} END{print mn+0, mx+0, c+0}' "$1"; }
# "min max frames" from one of those readings: pinned to the window (each end within a degree of it), or reaching it.
pinned() { awk -v w="$1" -v lo="$2" -v hi="$3" 'BEGIN{split(w,x," "); exit !(x[1]>=lo-1 && x[1]<=lo+1 && x[2]>=hi-1 && x[2]<=hi+1 && x[3]>=40)}' && echo 1 || echo 0; }
reaching() { awk -v w="$1" -v lo="$2" -v hi="$3" 'BEGIN{split(w,x," "); exit !(x[1]<=lo && x[2]>=hi && x[3]>=40)}' && echo 1 || echo 0; }

run pitchflat flat "$P; $LOOK" 'pitch|flatscreen play:'
phases "$S/pitchflat.log" "$S/pitchflat.phase"
q1=$(limits "$S/pitchflat.phase" 1); q2=$(limits "$S/pitchflat.phase" 2)
check $(pinned "$q1" -85 85) \
    "pitch: flatscreen play keeps the view inside vr_flat_pitch_up 85 / vr_flat_pitch_down 85 (r_pos ranged $q1 = min max frames, +lookdown then +lookup each held 40 frames)"
check $(pinned "$q2" 0 20) \
    "pitch: the same look inside vr_flat_pitch_up 0 / vr_flat_pitch_down 20 set on the console ($q2) -- the window is the cvars, not a constant baked into the clamp"
check $(grep -qa 'the look: vr_flat_pitch_up 85, vr_flat_pitch_down 85' "$S/pitchflat.log" \
        && grep -qa 'the look: vr_flat_pitch_up 0, vr_flat_pitch_down 20' "$S/pitchflat.log" && echo 1 || echo 0) \
    "pitch: vr_flatplay_status prints the window it is holding, both times ($(grep -a 'the look:' "$S/pitchflat.log" | sed 's/ (.*//' | tr '\n' '; '))"
run pitchwide flat "$P; vr_flat_pitch_up 90; vr_flat_pitch_down 90; $LOOK" 'pitch|flatscreen play:'
phases "$S/pitchwide.log" "$S/pitchwide.phase"
q3=$(limits "$S/pitchwide.phase" 1)
check $(reaching "$q3" -90 90) \
    "pitch: with both cvars at 90 the plain game's full look comes back, bounded by cl_minpitch / cl_maxpitch as it always was ($q3)"
# A run with neither -flatscreenmode nor a VR backend: `vr_backend none` (the engine warns and plays plain,
# Quake/vr/vr_main.cpp) -- the look's cvars sitting at their 85 defaults bound nothing there, which is what the mode's
# gate means for a play the mode never ran in.
run pitchplain vr "vr_backend none; $P; r_pos 1; cl_pitchspeed 600; wait10; +lookdown; wait40; -lookdown; wait10; +lookup; wait40; -lookup; wait20" 'pitch'
q4=$(span "$S/pitchplain.log")
check $(reaching "$q4" -90 90) \
    "pitch in plain play (no -flatscreenmode, vr_backend none): the cvars at 85 bound nothing there (r_pos ranged $q4 = min max frames, the same look keys held 40 frames)"

verdict; exit $fail
