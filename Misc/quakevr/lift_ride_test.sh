#!/bin/bash
# lift_ride_test.sh <agent> [vr|flat|both] -- riders on a moving lift (vr_stealth_test 131 on e1m1: two plats, the
# player and a grunt riding each up and down; each ride's worst sink held to 0.5 units), in the headset (the mock) and
# flat. Prints each case, then `lift_ride: N passed, M failed`.
AGENT=$1; WHICH=${2:-both}
KIT=C:/OHWorkspace/qvr-kit
W=C:/OHWorkspace/qvr-agents/$AGENT
mkdir -p "$W/scratch"
pass=0; fail=0
run() {
    local tag=$1 pre=$2
    local log="$W/scratch/lift_ride_$tag.log"
    bash "$KIT/run.sh" "$AGENT" -Script "vr_fixed_frames 1;sv_random_seed 1;$pre;map e1m1;wait60;god;notarget;vr_stealth_test 131" -QuitOn "lifttest: [0-9]+ passed" -Filter "${LIFT_FILTER:-lifttest|rror}" -Timeout 300 > "$log" 2>&1
    grep "lifttest: " "$log" | sed "s/^/$tag: /"
    local p f
    p=$(grep -c "lifttest: PASS" "$log")
    f=$(grep -c "lifttest: FAIL" "$log")
    grep -qE "lifttest: [0-9]+ passed" "$log" || { echo "$tag: lifttest FAIL (never finished)"; f=$((f + 1)); }
    pass=$((pass + p)); fail=$((fail + f))
}
if [ "$WHICH" = vr ] || [ "$WHICH" = both ]; then run vr "${LIFT_PRE:-}"; fi
if [ "$WHICH" = flat ] || [ "$WHICH" = both ]; then run flat "vr_enabled 0;${LIFT_PRE:-}"; fi
echo "lift_ride: $pass passed, $fail failed"
[ "$fail" = 0 ]
