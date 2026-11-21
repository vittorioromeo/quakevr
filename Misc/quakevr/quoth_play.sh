#!/bin/bash
# quoth_play.sh [flat]: Quoth play-throughs (docs/vr-port/QUOTH.md), headless, in the VR mock (or, with `flat`,
# -flatscreenmode). Quoth's campaign: start's hub to its episode's first map (its one exit, to e1m1quoth), e1m1quoth's
# flow to its ramp room and its exit (to e1m2quoth), kellbase1's flow (16 kills open its exit lock) and its exit back to
# start; the sample maps' flows: ne_marb's waves, lpqsp1's point triggers and hordes, metmon's stages. Each flow is
# vr_ad_test 212 (vr_quoth_test.qc: every trigger a player walks into touched, every monster awake killed, round by
# round, until the map's last stage is used; "quothplay: N passed, M failed" per map), whose verdict runs the next step
# (the alias qp_next: the steps wait on the flows, not on frames). Exits by vr_goto_exit <slot> <map>.
# quoth_setup.py first (as quoth_maps.sh); AD in the base dir. quoth_play.py: "quoth_play: N passed, M failed".
set -u
K=C:/OHWorkspace/qvr-kit
W=$(cd "$(dirname "$0")/../.." && pwd)
NAME=$(basename "$W")
MODE=${1:-vr}
OUT=$W/scratch/quoth_play
mkdir -p "$OUT"
BASE=$K/bases/$NAME/qbase${QVR_LANE:+.lane$QVR_LANE}
python -I "$W/Misc/quakevr/quoth_setup.py" "$BASE" > /dev/null
EXTRA="-nomapindex -noaddons"
[ "$MODE" = flat ] && EXTRA="$EXTRA -flatscreenmode"
# The steps (quakevr/*.cfg is git-ignored). Each flow sets qp_next to the step after it; its verdict runs qp_next.
{
    # (the engine's wait is one frame: run.sh expands a script's waitN, a cfg's are these aliases)
    echo 'alias w10 "wait;wait;wait;wait;wait;wait;wait;wait;wait;wait"'
    echo 'alias wait30 "w10;w10;w10"'
    echo 'alias wait120 "wait30;wait30;wait30;wait30"'
    echo 'alias qp1 "vr_campaign_native quoth;wait30;map start;wait30;echo quoth_play: hub start;vr_goto_exit 0 e1m1quoth;wait120;echo quoth_play: flow e1m1quoth;alias qp_next qp2;vr_ad_test 212"'
    echo 'alias qp2 "vr_goto_exit 0 e1m2quoth;wait120;echo quoth_play: flow kellbase1;map kellbase1;wait30;alias qp_next qp3;vr_ad_test 212"'
    echo 'alias qp3 "vr_goto_exit 0 start;wait120;echo quoth_play: flow ne_marb;map ne_marb;wait30;alias qp_next qp4;vr_ad_test 212"'
    echo 'alias qp4 "echo quoth_play: flow lpqsp1;map lpqsp1;wait30;alias qp_next qp5;vr_ad_test 212"'
    echo 'alias qp5 "echo quoth_play: flow metmon;map metmon;wait30;alias qp_next qp6;vr_ad_test 212"'
    echo 'alias qp6 "echo quoth_play: done;toggleconsole;quit"'
    echo 'qp1'
} > "$W/quakevr/quoth_play.cfg"
bash "$K/run.sh" "$NAME" -ExtraArgs "$EXTRA" -Timeout 900 -QuitOn "quoth_play: done" \
    -Script "developer 0;sv_random_seed 7;skill 1;coop 0;maxplayers 1;exec quoth_play.cfg" \
    -Filter "quoth_play|quothplay|PASS|FAIL|Host_Error|SV_Error|OBJECT ERROR|vr_goto_exit|exit=|TIMEOUT|timed out|Couldn't|Intermission|mapname" \
    > "$OUT/run_$MODE.log" 2>&1
python -I "$W/Misc/quakevr/quoth_play.py" "$OUT/run_$MODE.log"
