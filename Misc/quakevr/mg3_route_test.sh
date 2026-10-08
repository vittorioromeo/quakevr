#!/bin/bash
# Dawn of the Machine's (MG3) full-campaign route sweep, headless (mg3_route_test.py: the legs, steps and checks).
#   bash mg3_route_test.sh <agent> [legs...]     (legs: main bn exits, default all three; env: TMO seconds, default 2400)
# Runs the entity checker (all 22 BSPs), the language-gate checks (static and the owned table), then each leg on the owned rerelease MG3 data
# (vr_campaign_native mg3, -nomapindex -noaddons, developer 1); keeps each log in scratch/mg3route_<leg>.log and
# prints its FAIL lines and totals. Exit status: the number of failed parts.
NAME=${1:?agent}; shift; LEGS=${*:-main bn exits}
KIT="C:/OHWorkspace/qvr-kit"; T="C:/OHWorkspace/qvr-agents/$NAME"; mkdir -p "$T/scratch"
PY="$T/Misc/quakevr/mg3_route_test.py"; BAD=0
python "$T/Misc/quakevr/check_mg3_entities.py" | tail -1
python "$T/Misc/quakevr/check_mg3_entities.py" | tail -1 | grep -q "missing 0 placements 0 fields 0" || BAD=$((BAD+1))
python "$PY" lang || BAD=$((BAD+1))
# The owned language table resolves every gated identifier (the endings' texts included).
bash "$KIT/run.sh" "$NAME" -ExtraArgs "-nomapindex -noaddons" -Timeout 300 -Filter "VR language mg3"     -Script "vr_campaign_native mg3;wait10;vr_campaign_status;toggleconsole;quit" > "$T/scratch/mg3route_lang.log" 2>&1
if grep -q "VR language mg3: 0 missing" "$T/scratch/mg3route_lang.log"; then echo "mg3route: PASS VR language mg3: 0 missing identifiers"
else echo "mg3route: FAIL $(grep "VR language mg3" "$T/scratch/mg3route_lang.log" | head -1)"; BAD=$((BAD+1)); fi
for LEG in $LEGS; do
    case $LEG in main) FIRST=start;; bn) FIRST=hub;; exits) FIRST=start;; *) echo "unknown leg $LEG"; exit 2;; esac
    python "$PY" write "$LEG" || exit 2
    LOG="$T/scratch/mg3route_$LEG.log"
    bash "$KIT/run.sh" "$NAME" -ExtraArgs "-nomapindex -noaddons" -Timeout ${TMO:-2400} \
        -Filter "mg3route|mg3test|mg3ctest: (total|FAIL)|mg3shubtest: (total|FAIL)|Credits:|Host_Error|ENGINE|TIMEOUT|exit=|load failed|No spawn function|is not a field|PR_ExecuteProgram|SV_Error|runaway" \
        -Script "developer 1;vr_campaign_native mg3;skill 1;vr_mg_route_stage 1;map $FIRST" > "$LOG" 2>&1
    python "$PY" check "$LEG" "$LOG" > "$LOG.check"; [ $? -eq 0 ] || BAD=$((BAD+1))
    grep -v "PASS step [0-9]* (" "$LOG.check"
done
exit $BAD
