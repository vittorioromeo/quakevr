#!/bin/bash
# slider_step_test.sh <agent> -- the VR menus' plain slider steps land on round values, the fine modifier's on its grid
# (the author's note vrfiringrange_2026-10-07_22-23-40: with vr_menu_fine_step 0.0999 plain steps showed 0.6998, 0.7997).
# Headless, vr_menu_slider_step on Put-Away End Size (vr_collect_fx_size, step 0.05, "%.2fx").
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
C=vr_collect_fx_size
log=$(bash $KIT/run.sh $AGENT -Script "vr_menu_fine_step 0.0999;$C 0.6;vr_menu_slider_step $C 3;echo FINE;vr_menu_slider_step $C 2 fine;echo PLAIN;vr_menu_slider_step $C 1;vr_menu_slider_step $C -2;toggleconsole;quit" -Filter "slider_step|FINE|PLAIN" 2>&1)
vals() { echo "$log" | sed -n "$1" | grep slider_step | sed -E 's/.*"([^"]+)" shown (.*)/\1=\2/' | tr '\n' ' '; }
plain1=$(vals '1,/FINE/p'); fine=$(vals '/FINE/,/PLAIN/p'); plain2=$(vals '/PLAIN/,$p')
echo "plain: $plain1| fine: $fine| plain again: $plain2"
check $([ "$plain1" = "0.65=0.65x 0.7=0.70x 0.75=0.75x " ] && echo 1 || echo 0) "plain steps: round values, shown as before"
check $(echo "$fine" | awk '{print ($1 ~ /^0\.75[0-9]+=0\.75[0-9]+x$/) ? 1 : 0}') "fine steps: a fine step's value, its decimals shown"
check $([ "$plain2" = "0.8=0.80x 0.75=0.75x 0.7=0.70x " ] && echo 1 || echo 0) "plain steps after fine ones: back on round values"
exit $fail
