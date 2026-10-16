#!/bin/bash
# late_sprite_touch_test.sh <agent> -- a sprite late-precached from a touch must survive it (docs/vr-port/ROUND22.md,
# "A late sprite freed under a touch"). Needs AD's data in the agent's game folder (qbase/ad). vrfiringrange, god: an
# AD gargoyle made with the toolgun fires at the player; its fireball's explosion (progs/s_exp_small.spr, not
# precached by the map) is late-precached inside the touch, whose list is on the hunk; then many toolgun entries
# (alias models into the cache, over where the sprite was) while it keeps firing.
# PASS: REPROEND reached, no crash. Before the fix: R_DrawSpriteModel_Real crashed within seconds.
AGENT=${1:-spritecrash}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
S="map vrfiringrange;wait30;god;developer 1;vr_toolgun_spawn_test 70 70;wait600"
S="$S;vr_toolgun_spawn_test 0 40;wait400;vr_toolgun_spawn_test 71 120;wait400;vr_toolgun_spawn_test 121 160;wait400"
S="$S;vr_toolgun_spawn_test check;wait5;echo REPROEND;wait5;toggleconsole;quit"
out=$(bash $KIT/run.sh $AGENT -Script "$S" -Timeout 600 -Filter "(?i)REPROEND|ENGINE|CRASH|exit=|#0[0-4]|s_exp_small|held under" 2>&1)
echo "$out" | head -20
if echo "$out" | grep -q "REPROEND" && ! echo "$out" | grep -qE "ENGINE CRASH|ENGINE ERROR|exit=[1-9-]"; then echo PASS; else echo FAIL; fi
