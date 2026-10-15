#!/bin/bash
# toolgun_fire_test.sh <agent> [out.png] -- the toolgun's Fire tab (docs/vr-port/ROUND22.md, "Torches and fires from the
# toolgun"), in vrfiringrange, made by the spawn tool's trigger as a player would (vr_toolgun_select, the trigger):
#   1. the Wall Torch aimed level at the prop wall (a func_wall, its face at x -599): hung on it 10 units out (x -589),
#      facing out of it (yaw 0), takeable (vr_walltorch: its handtouch), its crackle on it ("walltorch: made live");
#   2. the Large, Small and White Flames and Rogue's Lantern (when Rogue's data is there) aimed at the floor ahead:
#      props hanging where placed ("flameprop: ..."); every flame's crackle plays on its entity (snd_show 2: fire1.wav
#      [L] on each one's ENT) and each has its torch light (vr_torch_lights_list: "entity N");
#   3. the large flame taken by the off hand's grip ("carry: light_flame_large_yellow held"), let go of it falls (its
#      light's z lower);
#   4. the wall torch taken by the grip (vr_test_walltorch_shot 24: "walltorch grip result: detached=1 held=1").
#   Then the screenshots (composed into out.png, under the worktree's scratch/ if relative): the wall torch, the flame.
# PASS: each line as said; the summary at the end says PASS.
AGENT=${1:-torches}; OUT=${2:-toolgun_fire.png}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
LOG=C:/OHWorkspace/qvr-agents/$AGENT/scratch/toolgun_fire.log
TRIG="vr_mock_button main trigger 1;wait3;vr_mock_button main trigger 0;wait10"
P="map vrfiringrange;wait60;god;notarget;developer 1;impulse 169;vr_weapon_grip_mode 1;wait10;vr_toolgun_tool 0"
# 1. The wall torch.
P="$P;setpos -540 -715 40 0 180 0;vr_mock_hand off -0.35 1.1 -0.2 0 0 0;vr_mock_hand main 0.15 1.4 -0.3 70 0 0;wait20"
P="$P;vr_toolgun_select 46;wait3;echo STEP walltorch;vr_toolgun_status;$TRIG"
# 2. The free-standing flames, on the floor ahead.
N=0
for e in 47:large 48:small 49:white 51:lantern; do
  i=${e%%:*}; y=$(( -650 + N * 40 )); N=$(( N + 1 ))
  P="$P;setpos -540 $y 40 0 180 0;vr_mock_hand main 0.15 1.4 -0.3 5 0 0;wait20;vr_toolgun_select $i;wait3;echo STEP ${e#*:};vr_toolgun_status;$TRIG"
done
P="$P;setpos -520 -640 40 0 180 0;wait30;echo STEP lights;vr_torch_lights_list;snd_show 2;wait1;snd_show 0"
# Screenshots: the wall torch, then the large flame (the toolgun put away; then taken out again).
P="$P;impulse 1;vr_mock_hand main 0.35 1.1 -0.2 0 0 0;vr_mock_hand off -0.35 1.1 -0.2 0 0 0;setpos -550 -710 50 0 180 0;wait30;screenshot"
P="$P;setpos -500 -645 40 0 180 0;vr_mock_look 30 0;wait30;screenshot;vr_mock_look 0 0;impulse 169;wait10"
# 3. The large flame taken by the off hand, let go of.
P="$P;setpos -520 -640 40 0 180 0;vr_mock_hand off -0.35 1.1 -0.2 0 0 0;wait20;echo STEP grab;vr_mock_hand_to off -548 -645 40;wait6;vr_mock_hand_to off -544 -645 44;wait6;+graboff;vr_mock_button off grip 1;wait20;vr_mock_hand_to off -552 -665 60;wait20"
P="$P;-graboff;vr_mock_button off grip 0;wait90;echo STEP dropped;vr_torch_lights_list"
# 4. The wall torch taken by a hand (the hand's reach to it is walltorch_grab_test.sh's): the grip's own test, the main
#    hand gripping the wall torch nearest (vr_test_walltorch_shot 24: "walltorch grip result: detached=1 held=1").
P="$P;setpos -566 -710 40 0 180 0;wait20;echo STEP walltorchgrip;vr_test_walltorch_shot 24;wait10;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Sound -Clean -Script "$P" -Out "$OUT" -Filter "STEP|toolgun: (spawned|selection|aim)|walltorch: (made live|taken)|walltorch (test|grip result)|flameprop|torch light|fire1|carry:|rror" -Timeout 300 > $LOG 2>&1
grep -v "^$" $LOG | head -80
python "$(dirname "$0")/toolgun_fire_check.py" $LOG
