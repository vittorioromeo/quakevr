#!/bin/bash
# gibprops_test.sh <agent> <map> <alive|corpse> <choice ...> -- every monster's head and gibs are Quake VR's props
# (ROUND22.md, "Gib props"; QC vr_gibprops_test.qc), headless: for each monster the debug spawner makes (vr_test_spawn
# <choice>, impulse 241; AD's from 200 in an AD map: PRE="vr_campaign_native ad"), gibbed alive (alive: past its gib
# health) or killed whole and its corpse gibbed (corpse); its head's prop checks: grabbable and destroyable, a Box3D body
# once landed, taken by the main hand's grip (the carry's hand touch with the mock's grip held), held (its held body),
# let go (a loose body again), shot (armed, burst, the small gibs made); its gibs grabbable and destroyable; AD's own gibs
# left AD's. Prints the table (Misc/quakevr/gibprops_table.py); the raw lines go to scratch/gibprops_<map>_<mode>.txt.
AGENT=${1:?agent}; MAP=${2:?map}; MODE=${3:?alive or corpse}; shift 3
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
TREE=C:/OHWorkspace/qvr-agents/$AGENT
S="${PRE:-}${PRE:+;}skill 1;wait30;map $MAP;wait60;god;notarget;developer 1;vr_test_spawn_dist 110;vr_decap_pop_roll 0;wait2"
for c in "$@"; do
    S="$S;echo == $c;vr_test_spawn $c;impulse 241;wait60"
    if [ "$MODE" = corpse ]; then
        S="$S;vr_gibprops_test 2;wait300;vr_gibprops_test 3"
    else
        S="$S;vr_gibprops_test 1"
    fi
    S="$S;wait150;vr_gibprops_test 8;wait2;+grabright;vr_mock_button main grip 1;wait3;vr_gibprops_test 4;wait15;vr_gibprops_test 5;wait2"
    S="$S;-grabright;vr_mock_button main grip 0;wait15;vr_gibprops_test 7;wait2;vr_gibprops_test 6;wait10"
done
OUT=$TREE/scratch/gibprops_${MAP}_${MODE}.txt
mkdir -p "$TREE/scratch"
bash "$KIT/run.sh" "$AGENT" -Script "$S;wait5;toggleconsole;quit" -Timeout 900 \
    -Filter "^gibprops|^== |rror|CRASH|SV_Error|Host_Error" > "$OUT" 2>&1
grep -E "rror|CRASH" "$OUT" | head -5
python "$TREE/Misc/quakevr/gibprops_table.py" "$OUT"
