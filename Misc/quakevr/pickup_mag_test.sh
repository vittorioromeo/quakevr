#!/bin/bash
# pickup_mag_test.sh <agent> -- a map's spinning weapon pickup draws its magazine on it (the author's note
# e1m1_2026-10-07_22-39-10: the nailgun's came off: the magazine wasn't drawn with the pickup's model_offset). Headless,
# immersive reloading on e1m1, vr_pickup_test 1 (every pickup ahead, each with the same weapon dropped beside it) and 3
# (walked along them); vr_reload_debug 1 prints, once a second, each lying gun's magazine seat as drawn against the gun's.
# Prints PASS/FAIL per check; exits 1 on a failure.
AGENT=$1; KIT=${KIT:-C:/OHWorkspace/qvr-kit}
fail=0
check() { if [ "$1" = "1" ]; then echo "PASS $2"; else echo "FAIL $2"; fail=1; fi; }
S="vr_holster_mode 0;vr_reload_mode 3;map e1m1;wait60;god;notarget;vr_pickup_test 1;wait120;developer 1;vr_reload_debug 1"
for k in 1 2 3 4 5 6 7 8; do S="$S;vr_pickup_test 3;wait100"; done
log=$(bash $KIT/run.sh $AGENT -Script "$S;toggleconsole;quit" -Filter "magazine \(entity|rror" 2>&1)
spin=$(echo "$log" | grep "spinning)" | sed -E 's/.*lying ([^ ]+)'"'"'s.* ([0-9.]+) units.*/\1 \2/' | sort -u)
dropped=$(echo "$log" | grep "magazine (entity [0-9]*)" | sed -E 's/.*lying ([^ ]+)'"'"'s.* ([0-9.]+) units.*/\1 \2/' | sort -u)
echo "spinning: $(echo $spin)"; echo "dropped: $(echo $dropped)"
check $(echo "$spin" | grep -q "v_nail.mdl" && echo "$spin" | grep -q "v_nail2.mdl" && echo 1 || echo 0) "both nailguns' pickups seen"
check $(echo "$spin $dropped" | awk '{for(i=2;i<=NF;i+=2) if($i+0 > 0.05) bad=1} END{print bad ? 0 : 1}') "every magazine on its gun (0 units off)"
check $(echo "$log" | grep -q "rror" && echo 0 || echo 1) "no error"
exit $fail
