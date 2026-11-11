#!/bin/bash
# screenhands_test.sh <agent> -- a gun's ammo screen sits at the same place in its housing in either hand (ROUND24.md,
# "The ammo screen in the left hand"). Flat-screen play, headless, fast mode; per gun one run: the gun in the right
# hand, vr_debug_screen_where (its glass's centre in the model's own units), the hands swapped (impulse 48: the gun in
# the left hand, drawn mirrored), vr_debug_screen_offhand 1 and the same report. Each run ends on its own last line
# (-QuitOn). The centres must agree within 0.01 model units (before the fix the grenade launcher's was 0.256 apart: the
# housing style's offset along the glass's right was not turned round with the mirrored model). Prints
# "screenhands: N passed, M failed".
AGENT=${1:?agent}
source "$(dirname "$0")/qvrtest.sh"
S=C:/OHWorkspace/qvr-agents/$AGENT/scratch/screenhands
mkdir -p "$S"

# gun <tag> <test impulse>: the two screenwhere lines in $S/<tag>.log, then one check.
gun() { local tag=$1 imp=$2 sc d
    sc="map e1m1; wait30; god; notarget; impulse $imp; wait60; vr_debug_screen_offhand 0; vr_debug_screen_where 1; wait5"
    sc="$sc; impulse 48; wait120; vr_debug_screen_offhand 1; vr_debug_screen_where 1; wait5; vr_debug_screen_offhand 0"
    sc="$sc; echo screenhands: $tag done"
    bash $KIT/run.sh $AGENT -ExtraArgs "-flatscreenmode" -Script "$sc" -QuitOn "^screenhands: $tag done" -Timeout 120 \
        -Filter "^(screenwhere|screenhands)" > "$S/$tag.log" 2>&1
    d=$(awk '/^screenwhere:/{n++; x[n]=$4; y[n]=$5; z[n]=$6; m=$2}
        END{if(n<2){print "none"; exit} dx=x[1]-x[2]; dy=y[1]-y[2]; dz=z[1]-z[2]; if(dx<0)dx=-dx; if(dy<0)dy=-dy; if(dz<0)dz=-dz;
            e=dx; if(dy>e)e=dy; if(dz>e)e=dz; printf "%s %.4f (right %s %s %s, left %s %s %s)", m, e, x[1], y[1], z[1], x[2], y[2], z[2]}' "$S/$tag.log")
    check $(awk -v d="$d" 'BEGIN{split(d, a, " "); print (a[2] != "" && a[2] + 0 < 0.01) ? 1 : 0}') "$tag: the glass's centre in either hand: $d"; }

gun shotgun 154
gun nailgun 156
gun grenade 158
gun rocket 160

verdict; exit $fail
