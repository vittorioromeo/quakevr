#!/bin/bash
# embeddual_test.sh <agent> -- vr_embed_test 52 (QC/vr_embeddual_test.qc): flat dual wielding, an axe stuck in a grunt keeps its hand
# (drawn on it, the gun one-handed, no ironsight); out into the hand: dual again; let go: the gun two-handed and the ironsight back.
# Headless, fast mode, -flatscreenmode; ends on `embeddual: N passed, M failed` (docs/vr-port/ROUND25.md, "embeddual").
AGENT=${1:?agent}
KIT=C:/OHWorkspace/qvr-kit
bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode -loopbackonly" -Timeout 180 \
    -Script "map vrtesthall; god; notarget; wait5; setpos 0 300 25 0 90 0; wait5; vr_embed_test 52" \
    -QuitOn "^embeddual: [0-9]+ passed" -Filter "^(embeddual|flatroute|FAIL|PASS)"
