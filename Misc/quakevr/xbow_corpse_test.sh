#!/bin/bash
KIT=C:/OHWorkspace/qvr-kit
PRE="skill 1;vr_campaign_native ad;wait30;map ad_tfuma;wait60;god;notarget;developer 1;vr_decap_pop_roll 0;vr_test_spawn_dist 55;r_fullbright 1;wait2"
bash $KIT/run.sh tghost -Script "$PRE;vr_test_spawn 202;impulse 241;wait60;vr_mock_look 30 0;vr_ad_mtest 3;wait8;screenshot;wait12;screenshot;wait40;screenshot;wait300;screenshot;vr_ad_mtest 2;toggleconsole;quit" -Filter "admtest|hidden in|rror|nknown" -Out xbow_$1_fb.png
