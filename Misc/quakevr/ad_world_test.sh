#!/bin/bash
# ad_world_test.sh <agent>: real ad_crucial buttons, bags and rung/gap probes.
set -eu
AGENT=${1:?worker name}
K=C:/OHWorkspace/qvr-kit
TREE=C:/OHWorkspace/qvr-agents/$AGENT
OUT=$TREE/scratch/ad_world_test.log
S='developer 1;sv_random_seed 7;vr_campaign_native ad;wait30;map ad_crucial;wait180;god;notarget;vr_ad_test 61;wait30;vr_ad_test 62;wait30;vr_ad_test 63;wait10;setpos -417 -2420 -107 0 90 0;wait10;vr_climb_leniency 0;vr_climb_leniency_air 0;vr_climb_try -417 -2387 -70 main;vr_climb_try -417 -2387 -60 main;vr_climb_try -417 -2387 -46 main;vr_climb_try -417 -2387 -36 main;vr_climb_try -417 -2387 -22 main;vr_climb_try -417 -2387 -12 main;vr_climb_try -417 -2387 4 main;vr_climb_try -417 -2387 14 main;vr_climb_try -417 -2387 28 main;vr_climb_try -417 -2387 38 main;vr_climb_try -417 -2387 52 main;vr_climb_try -417 -2387 62 main;vr_climb_try -417 -2387 76 main;vr_climb_try -417 -2387 86 main;vr_climb_ladders 0;vr_climb_try -417 -2387 -70 main;vr_climb_ladders 1;vr_ad_test 64;wait10;vr_climb_try -446.5 -2420 -60 main;vr_climb_try -446.5 -2420 -20 main;vr_climb_ladders 0;vr_climb_try -446.5 -2420 -60 main;vr_climb_ladders 1;vr_menu_path_check maps/vrcalibration.map;toggleconsole;quit'
bash "$K/run.sh" "$AGENT" -Games '-game ad -game quakevr' -ExtraArgs '-nomapindex -noaddons' -Script "$S" -Filter '^adworld:|^climbtry|menu paths:|Host_Error|SV_Error|ENGINE ERROR|Fatal error|CRASH|exit=' -Timeout 420 > "$OUT"
grep -q '^exit=0' "$OUT"
grep -c 'button mode .* opened 1 activator 1' "$OUT" | grep -q '^4$'
grep -q 'disabled button stayed shut 1' "$OUT"
grep -q 'walkover ammo 0 bag' "$OUT"
grep -q 'grabbed 1 carry 1' "$OUT"
grep -q 'holster ammo 5 held 0' "$OUT"
grep -q 'restored bag physical 1 same size 1 holster 1' "$OUT"
grep -c 'ladder cfg 0 mode .* velocity 21 0 -17 state 0 gravity 1$' "$OUT" | grep -q '^2$'
# Hand Climbing off (cfg 1) or Hand-Climbed Ladders off (cfg 2): AD's own jump climb still lifts the player.
grep -c 'ladder cfg [12] mode 0 velocity 0 0 160 ' "$OUT" | grep -q '^2$'
grep -q 'flat ladder wall -447.969 -2420 .* normal 1 0' "$OUT"
grep -q 'climbtry -446.50 -2420.00 -60.00 main: exact hold -446.50 -2420.00 -60.00' "$OUT"
grep -q 'climbtry -446.50 -2420.00 -20.00 main: exact hold -446.50 -2420.00 -20.00' "$OUT"
grep -c 'main: exact' "$OUT" | grep -q '^9$'
grep -c 'main: none' "$OUT" | grep -q '^9$'
grep -q 'menu paths: [1-9][0-9]* found, 0 missing' "$OUT"
if grep -qE 'Host_Error|SV_Error|ENGINE ERROR|Fatal error|CRASH' "$OUT"; then exit 1; fi
printf 'adworld PASS: 4 contact modes; walkover/grip/holster/restore; 2 ladder modes x 3 settings; 7 rungs/7 gaps + disabled; flat ladder held x2 + disabled; calibration: 0 missing\n'
