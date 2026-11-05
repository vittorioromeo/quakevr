#!/bin/bash
# sa_shockarcs_test.sh <agent> [outdir] [--maps "a b"] [--modes "flat mock"] [--cases "1 2 3 4"] -- struck by lightning, the arcs
# on your own arms are SEEN (docs/vr-port/ROUND24.md, "The arcs on your arms on a flat screen"). Each case (QC/vr_shockarcs_test.qc:
# a shambler's real bolts from behind: looking level, looking down, crouched, the look swinging) runs twice on the same frames,
# the arcs on and off (vr_shock_self_time; the blue light off in both, only the arcs told apart), SHOTS screenshots each, since
# they are reshaped every frame and some frames have few. PASS: in at least MIN_SHOTS of them at least MIN_BLUE pixels went
# brighter and bluer in the view's lower half, on the arms (sa_shockarcs_diff.py), and the bolts reached the client.
# Ends `shockarcs: <n> passed, <m> failed`.
AGENT=${1:?agent}
OUT=${2:-C:/OHWorkspace/qvr-kit/scratch/shockarcs}
shift 2
MAPS="vrfiringrange"
MODES="flat"
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
MIN_BLUE=${MIN_BLUE:-150}
MIN_SHOTS=${MIN_SHOTS:-3}
mkdir -p "$OUT"
pass=0
fail=0

# One run: <map> <mode> <case> <vr_shock_self_time> <log> <prefix>: its screenshots copied to <prefix>_<k>.png, in the order taken.
shoot() {
    local extra=()
    [ "$2" = flat ] && extra=(-ExtraArgs "-flatscreenmode")
    bash $KIT/run.sh "$AGENT" "${extra[@]}" -Clean -Out "$OUT/composed.png" -Filter "shockarcs: |selfshock: |Host_Error|SV_Error|ENGINE" -Timeout 120 \
        -QuitOn "shockarcs: .*the screenshot's taken" \
        -Script "map $1;wait30;god;notarget;r_oit 1;r_wateralpha 0.3;vr_shock_self_light 0;vr_shock_self_time $4;${SA_EXTRA}vr_shockarcs_test $3" >"$5" 2>&1
    local k=0
    # (Named by the second, an index added within one: in the order taken, by bytes.)
    for f in $(ls "$SHOTS"/*.png | LC_ALL=C sort); do
        k=$((k + 1))
        cp "$f" "$6_$k.png"
    done
    echo "$k"
}

for map in $MAPS; do
    for mode in $MODES; do
        for c in $CASES; do
            tag=${map}_${mode}_case$c
            n=$(shoot "$map" "$mode" "$c" 0.8 "$OUT/${tag}_on.log" "$OUT/${tag}_on")
            shoot "$map" "$mode" "$c" 0 "$OUT/${tag}_off.log" "$OUT/${tag}_off" >/dev/null
            seen=0
            counts=""
            last=""
            for k in $(seq 1 "$n"); do
                last=$(python "$HERE/sa_shockarcs_diff.py" "$OUT/${tag}_on_$k.png" "$OUT/${tag}_off_$k.png" --out "$OUT/${tag}_$k.png")
                blue=$(echo "$last" | sed 's/.*lowblue=\([0-9]*\).*/\1/')
                counts="$counts $blue"
                [ "${blue:-0}" -ge "$MIN_BLUE" ] && seen=$((seen + 1))
            done
            info=$(grep -a "selfshock: arcs=" "$OUT/${tag}_on.log" | tail -1 | sed 's/.*selfshock: //')
            hands=$(grep -a "selfshock: hand " "$OUT/${tag}_on.log" | tail -1 | sed 's/.*selfshock: //')
            state=$(grep -a "screenshot's taken" "$OUT/${tag}_on.log" | tail -1 | sed "s/.*shockarcs: [0-9]*: //; s/: the screenshot's taken//")
            case "$info" in
                *"bolts=0"*|"") bolts=0 ;;
                *) bolts=1 ;;
            esac
            if [ "$seen" -ge "$MIN_SHOTS" ] && [ "$bolts" = 1 ]; then
                verdict=PASS
                pass=$((pass + 1))
            else
                verdict=FAIL
                fail=$((fail + 1))
            fi
            echo "shockarcs: $verdict $tag: $seen of $n frames lit, lower blue pixels:$counts; last $last; $state; $hands; $info"
        done
    done
done
echo "shockarcs: $pass passed, $fail failed"
