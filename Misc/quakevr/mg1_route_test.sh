#!/bin/bash
# Dimension of the Machine's whole campaign route, headless (mg1_route_test.py: the steps and the checks).
#   bash mg1_route_test.sh <agent> [skill]      (env: TMO seconds, default 2400)
# Writes quakevr/mg1route*.cfg, runs start -> hub -> the five episodes -> the final gate -> mgend -> the credits on
# the owned rerelease MG1 data (vr_campaign_native mg1, -nomapindex), keeps the log in scratch/mg1route_s<skill>.log
# and checks it (every step in its map, the hub's runes/indicators/gate and fresh equipment, the seeded hand and
# holster magazines and ids carried through levels, a load and a death's autoload; Nightmare: health 50/50).
NAME=${1:?agent}; SKILL=${2:-1}
KIT="C:/OHWorkspace/qvr-kit"; T="C:/OHWorkspace/qvr-agents/$NAME"; mkdir -p "$T/scratch"
python "$T/Misc/quakevr/mg1_route_test.py" write || exit 2
LOG="$T/scratch/mg1route_s$SKILL.log"
bash "$KIT/run.sh" "$NAME" -ExtraArgs "-nomapindex" -Timeout ${TMO:-2400} \
    -Filter "mghubtest|mg1route|Host_Error|ENGINE|TIMEOUT|exit=|load failed|Saving game|Loading game" \
    -Script "developer 1;vr_campaign_native mg1;skill $SKILL;vr_mg_route_stage 1;map start" > "$LOG" 2>&1
python "$T/Misc/quakevr/mg1_route_test.py" check "$LOG" --skill "$SKILL" | grep -v "PASS step [0-9]* (" 
