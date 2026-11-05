#!/bin/bash
# waterzap_test.sh <agent> [outdir] [--maps "a b"] [--modes "flat mock"] [--cases "1 2 3 4"] -- lightning into water is SEEN
# (docs/vr-port/ROUND24.md, "Lightning on water"). Each case (QC/vr_waterzap_test.qc: a thunderbolt by the trigger, steep
# and glancing; Mjolnir by the trigger; a shambler's bolt) on each map, flat-screen (-flatscreenmode) and in the mock
# headset, runs twice with the arcs' width (vr_lg_water_arcs) 1 and 0 on the same frames, only the arcs told apart. PASS: at least MIN_BLUE pixels went brighter and bluer
# on the screen (waterzap_diff.py). Ends `waterzap: <n> passed, <m> failed`.
AGENT=${1:?agent}
OUT=${2:-C:/OHWorkspace/qvr-kit/scratch/waterzap}
shift 2
MAPS="vrfiringrange vrstart"
MODES="flat mock"
CASES="1 2 3 4"
while [ $# -gt 0 ]; do
    case "$1" in
        --maps) MAPS=$2 ;;
        --modes) MODES=$2 ;;
        --cases) CASES=$2 ;;
    esac
    shift 2
done
KIT=C:/OHWorkspace/qvr-kit
HERE=$(dirname "$0")
SHOTS=$KIT/bases/$AGENT/qbase/quakevr/screenshots
# (Between the two runs a bolt's own model and particles differ: 1300 blue pixels at most, next's flat frames; the arcs from 3000.)
MIN_BLUE=${MIN_BLUE:-2000}
mkdir -p "$OUT"
pass=0
fail=0

# One run: <map> <mode> <case> <vr_lg_water_arcs> <out png>. His settings by default: r_oit 1, r_wateralpha 0.3 (WZ_WATER).
shoot() {
    local extra=()
    [ "$2" = flat ] && extra=(-ExtraArgs "-flatscreenmode")
    # (Mjolnir's chain of beams, the knight and its blood differ between runs by more than the arcs: not drawn for this case.)
    local hide=""
    [ "$3" = 3 ] && hide="r_drawentities 0;"
    bash $KIT/run.sh "$AGENT" "${extra[@]}" -Clean -Out "$5" -Filter "waterzap: case|lgwater: electrified at|melee event|Dummy|Host_Error|SV_Error|ENGINE|no open water" -Timeout 120 \
        -QuitOn "waterzap: .*the screenshot's taken" \
        -Script "vr_test_spawn 5;map $1;wait30;developer 1;god;notarget;${WZ_WATER:-r_oit 1;r_wateralpha 0.3;}${hide}tune vr_lg_water_arcs $4;vr_waterzap_test $3" 2>&1 |
        grep -c "lgwater: electrified at"
    ls -t "$SHOTS"/*.png 2>/dev/null | head -1
}

for map in $MAPS; do
    for mode in $MODES; do
        for c in $CASES; do
            tag=${map}_${mode}_case$c
            on=$(shoot "$map" "$mode" "$c" 1 "$OUT/${tag}_on_composed.png")
            onzaps=$(echo "$on" | head -1)
            onshot=$(echo "$on" | tail -1)
            cp "$onshot" "$OUT/${tag}_on.png"
            off=$(shoot "$map" "$mode" "$c" 0 "$OUT/${tag}_off_composed.png")
            cp "$(echo "$off" | tail -1)" "$OUT/${tag}_off.png"
            line=$(python "$HERE/waterzap_diff.py" "$OUT/${tag}_on.png" "$OUT/${tag}_off.png" --out "$OUT/${tag}.png")
            blue=$(echo "$line" | sed 's/.*blue=\([0-9]*\).*/\1/')
            if [ "${blue:-0}" -ge "$MIN_BLUE" ]; then
                verdict=PASS
                pass=$((pass + 1))
            else
                verdict=FAIL
                fail=$((fail + 1))
            fi
            echo "waterzap: $verdict $tag: $line, $onzaps electrified lines"
        done
    done
done
echo "waterzap: $pass passed, $fail failed"
