#!/bin/bash
# usage: xbow_test.sh <tag>   -- crossbow knight corpse (dead pose) and downkill ragdoll screenshots
KIT=C:/OHWorkspace/qvr-kit
PRE="skill 1;vr_campaign_native ad;wait30;map start;wait60;god;notarget;developer 1;vr_decap_pop_roll 0;vr_test_spawn_dist 90;vr_knockdown_test 20;wait2"
bash $KIT/run.sh tghost -Script "$PRE;vr_test_spawn 202;impulse 241;wait60;vr_ad_mtest 3;wait200;screenshot;toggleconsole;quit" -Filter "admtest|hidden in|rror" -Out xbow_$1_dead.png
bash $KIT/run.sh tghost -Script "$PRE;vr_test_spawn 202;impulse 241;wait60;vr_knockdown_test 0;wait30;vr_knockdown_test 3;wait90;screenshot;toggleconsole;quit" -Filter "admtest|hidden in|rror" -Out xbow_$1_rag.png
