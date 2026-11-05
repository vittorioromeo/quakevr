#!/bin/bash
# burnmark_test.sh <agent> [outdir] -- lightning's burns on a wall are SEEN and last (docs/vr-port/ROUND24.md, "Lightning burn
# marks"). A thunderbolt by the trigger and a shambler's bolts (QC/vr_burnmark_test.qc), flat-screen and in the mock headset,
# on vrtesthall's south wall: a picture of the wall before, one after the bolts have gone and their light with them, and
# one after a save and a load. PASS: at least MIN_DARK pixels went darker, and after the load too (burnmark_diff.py).
# Ends `burnmark: <n> passed, <m> failed`.
AGENT=${1:?agent}
OUT=${2:-C:/OHWorkspace/qvr-kit/scratch/burnmarks}
MODES=${BM_MODES:-flat mock}
CASES=${BM_CASES:-1 2}
KIT=C:/OHWorkspace/qvr-kit
HERE=$(dirname "$0")
SHOTS=$KIT/bases/$AGENT/qbase/quakevr/screenshots
MIN_DARK=${MIN_DARK:-60}
mkdir -p "$OUT"
pass=0
fail=0

for mode in $MODES; do
    extra=()
    [ "$mode" = flat ] && extra=(-ExtraArgs "-flatscreenmode")
    for c in $CASES; do
        tag=${mode}_case$c
        bash $KIT/run.sh "$AGENT" "${extra[@]}" -Clean -Out "$OUT/${tag}_composed.png" -Filter "burnmark|permdecals: |Host_Error|SV_Error|ENGINE" -Timeout 180 \
            -QuitOn "^burnmark: loadeds*$" \
            -Script "vr_test_spawn 5;map vrtesthall;wait30;developer 1;god;notarget;r_drawentities 0;vr_burnmark_test $c" 2>&1 | grep -v "^$\|low-impact\|gadget\|composed" | cut -c1-120 | tail -5
        n=0
        for f in $(ls -tr "$SHOTS"/*.png); do
            n=$((n + 1))
            cp "$f" "$OUT/${tag}_$n.png"
        done
        line=$(python "$HERE/burnmark_diff.py" "$OUT/${tag}_1.png" "$OUT/${tag}_2.png" --out "$OUT/${tag}.png")
        loaded=$(python "$HERE/burnmark_diff.py" "$OUT/${tag}_1.png" "$OUT/${tag}_3.png" --out "$OUT/${tag}_loaded.png")
        d1=$(echo "$line" | sed 's/dark=\([0-9]*\).*/\1/')
        d2=$(echo "$loaded" | sed 's/dark=\([0-9]*\).*/\1/')
        if [ "${d1:-0}" -ge "$MIN_DARK" ] && [ "${d2:-0}" -ge "$MIN_DARK" ]; then
            verdict=PASS
            pass=$((pass + 1))
        else
            verdict=FAIL
            fail=$((fail + 1))
        fi
        echo "burnmark: $verdict $tag: after $line; after a load $loaded"
    done
done
echo "burnmark: $pass passed, $fail failed"
