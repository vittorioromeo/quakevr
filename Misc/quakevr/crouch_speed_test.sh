#!/bin/bash
# crouch_speed_test.sh <agent>: crouched, slower and silent (vr_crouch_speed; ROUND22 "Crouched, slower and silent").
# On the start map, the speed button held (cl_alwaysrun 0), a grunt 150 units off in the dark (vr_stealth_light_force 0),
# the stick runs for 1 s: crouched (head at 0.9 m) the speed peaks at 240 (0.75 x 320) and the grunt hears no step noise;
# crouched with vr_crouch_speed_scale 1 it peaks at 252 (90% of vr_stealth_run_speed 280), unheard; standing (1.69 m, the
# mock's own height) 320 and heard. Prints one PASS/FAIL line a case.
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
FAILS=0

runcase() # <name> <head m> <extra commands> <speed wanted> <heard wanted 0|1>
{
    local out
    out=$(bash $KIT/run.sh "$AGENT" -Filter "stealthtest: probe|hears noise 1" -Script "map start;wait60;god;cl_alwaysrun 0;+speed;vr_stealth_light_force 0;vr_stealth_debug 2;vr_test_spawn 0;vr_test_spawn_dist 150;impulse 241;wait20;$3;vr_mock_look 0 90;vr_mock_hand head 0 $2 0;wait30;vr_mock_stick off 0 1;vr_stealth_test 22;wait70;vr_mock_stick off 0 0;wait60;toggleconsole;quit" 2>&1)
    local top heard
    top=$(echo "$out" | grep -o "speed [0-9]*" | awk '{ if ($2 > m) m = $2 } END { print m + 0 }')
    heard=$(echo "$out" | grep -c "hears noise 1")
    [ "$heard" -gt 0 ] && heard=1
    if [ "$top" = "$4" ] && [ "$heard" = "$5" ]; then
        echo "crouchspeed: $1 PASS (top speed $top, heard $heard)"
    else
        echo "crouchspeed: $1 FAIL (top speed $top, want $4; heard $heard, want $5)"
        FAILS=$((FAILS + 1))
    fi
}

runcase crouched 0.9 "echo" 240 0
runcase crouched_scale1 0.9 "vr_crouch_speed_scale 1" 252 0
runcase standing 1.69 "echo" 320 1
runcase off_crouched 0.9 "vr_crouch_speed 0" 320 1
echo "crouchspeed: done, $FAILS failed"
