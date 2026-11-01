#!/bin/bash
# ghost_body_test.sh <agent>: a hand holding an enemy shows no ghost hand with vr_ghost_hands_ignore_bodies 1, and the
# same hold does with it 0 (ROUND23.md, "Ghost hands: none while holding a body"). A grunt spawned 36 units ahead, the
# main hand put on its chest (vr_mock_hand_to) and gripped; Ghost Hands on with no fade margin (any offset of the drawn
# hand shows it); 20 frames each phase. Ends on "vr_ghost_body_test: N passed, M failed". (The ghost for a weapon in two
# hands, unchanged: ghost_recoil_test.sh's "counted" phase.)
KIT=C:/OHWorkspace/qvr-kit
AGENT=${1:-posepromo}
S="sv_random_seed 7;map vrfiringrange;wait30;god;notarget;vr_test_spawn 0;vr_test_spawn_dist 36;impulse 241;wait40"
S="$S;vr_ghost_hands 1;vr_ghost_hands_fade_from 0;vr_ghost_hands_fade_range 0.1;vr_ghost_hands_ignore_bodies 1"
S="$S;vr_mock_hand_to main nearest monster_army 8;wait3;+grabmain;wait20;vr_ghost_body_test start;wait20"
S="$S;vr_ghost_body_test hidden;vr_ghost_hands_ignore_bodies 0;wait20;vr_ghost_body_test end;-grabmain;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "vr_ghost_body_test|foegrab" -QuitOn "vr_ghost_body_test: \d+ passed"
