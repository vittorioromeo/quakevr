#!/bin/bash
# xbow_corpse_test.sh <agent> -- AD's monster 202 on ad_tfuma, vr_ad_mtest 3 then 2; four fullbright screenshots
# composed into <agent>'s scratch/xbow_<agent>_fb.png. Runs the given agent's own game (it ran tghost's before).
KIT=C:/OHWorkspace/qvr-kit
AGENT=${1:?agent}
PRE="skill 1;vr_campaign_native ad;wait30;map ad_tfuma;wait60;god;notarget;developer 1;vr_decap_pop_roll 0;vr_test_spawn_dist 55;r_fullbright 1;wait2"
bash $KIT/run.sh "$AGENT" -Script "$PRE;vr_test_spawn 202;impulse 241;wait60;vr_mock_look 30 0;vr_ad_mtest 3;wait8;screenshot;wait12;screenshot;wait40;screenshot;wait300;screenshot;vr_ad_mtest 2;toggleconsole;quit" -Filter "admtest|hidden in|rror|nknown" -Out "xbow_${AGENT}_fb.png"
