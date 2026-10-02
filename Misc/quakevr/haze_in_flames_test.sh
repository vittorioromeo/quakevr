#!/bin/bash
# haze_in_flames_test.sh <agent> ["<extra commands>"] -- the heat haze with the eye in a burning corpse's flames
# (ROUND21.md, "Black patches standing in flames"). vrfiringrange: a grunt's corpse set on fire, the player standing on
# it; at 8 head poses an eye image with the haze on, then one with it off. Prints, per eye and pose, the share of
# pixels differing by more than 48 (of 255): the haze alone moves a few pixels; a patch of one colour over the view
# shows as tens of percent. Run it again with `vr_heat_haze 0` in both (sed) for the frames' own noise.
KIT="C:/OHWorkspace/qvr-kit"
NAME="$1"
P="vr_heat_haze 1;wait2;vr_eyeshot 1;wait1;vr_heat_haze 0;wait2;vr_eyeshot 1;wait1;vr_heat_haze 1"
SHOTS="$KIT/bases/$NAME/qbase/quakevr/eyeshots"
bash "$KIT/run.sh" "$NAME" -Clean -Script "map vrfiringrange;wait60;god;notarget;developer 1;vr_particle_seed 7;$2;vr_test_spawn 0;vr_test_spawn_dist 60;vr_test_spawn_dead 1;setpos -92 -455 41 0 57 0;wait5;impulse 241;wait60;vr_burn_test 1;wait30;setpos -59 -405 41 0 57 0;vr_mock_look 60 0;wait40;$P;wait7;vr_mock_look 30 40;$P;wait7;vr_mock_look 10 -40;$P;wait7;vr_mock_look 75 0;$P;vr_mock_hand head 0 0.5 0 0 0 0;vr_mock_look 30 0;wait20;$P;wait7;vr_mock_look 0 90;$P;vr_mock_hand head 0 1.0 0 0 0 0;vr_mock_look 20 180;wait7;$P;wait5;vr_mock_look 45 -90;$P;toggleconsole;quit" -Filter "rror" 2>&1 | grep -v vr_ao | tail -5
python - "$SHOTS" <<'EOF'
import glob, os, sys
import numpy as np
from PIL import Image
for eye in "LR":
    fs = sorted(glob.glob(f"{sys.argv[1]}/*_{eye}.png"), key=os.path.getmtime)[-16:]
    out = []
    for a, b in zip(fs[0::2], fs[1::2]):
        A = np.asarray(Image.open(a).convert("RGB"), dtype=np.float32)
        B = np.asarray(Image.open(b).convert("RGB"), dtype=np.float32)
        out.append(f"{(np.abs(A - B).mean(axis=2) > 48).mean() * 100:.2f}%")
    print(eye, " ".join(out))
EOF
