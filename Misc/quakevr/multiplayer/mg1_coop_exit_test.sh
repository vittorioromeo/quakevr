#!/bin/bash
# bash mg1_coop_exit_test.sh <agent> [tag] [map]   (env: SVWAIT, CLWAIT, AFTERWAIT frames; TMO s)
# MG1 Horde coop arena exit + intermission over UDP on this machine: a listen server (instance 1, the host) and a
# remote client (instance 2). After both are in horde1, the host walks into the arena's real exit (vr_mg_hub_test 30:
# the engine's own trigger touch), the coop intermission runs, the presser's real jump presses leave it, and the
# arena rotation loads the next arena with both players; mg1route101.cfg (vr_mg_route_stage) then reports there.
NAME=${1:?agent}; TAG=${2:-a}; MAP=${3:-horde1}
KIT="C:/OHWorkspace/qvr-kit"; T="C:/OHWorkspace/qvr-agents/$NAME"; W="$T/scratch"
source "$KIT/slots.sh" "$NAME" mp
acquire_many 2
trap 'release' EXIT
R="vr_mg_horde_test 15"
SV="developer 1;coop 1;deathmatch 0;vr_campaign_native mg1;wait120;skill 1;vr_mg_route_stage 100;map $MAP"
printf '%s
' "vr_mg_route_stage 101" "god 1" "notarget 1" $(printf 'wait %.0s' $(seq 1 ${SVWAIT:-2400})) "echo MARK both in" "$R" "wait" "wait"     "vr_mg_hub_test 3" "wait" "wait" "vr_mg_hub_test 30" | sed 's/^wait $/wait/' > "$T/quakevr/mg1route100.cfg"
printf '%s\n' "vr_mg_route_stage 0" $(printf 'wait %.0s' $(seq 1 300)) "echo MARK next arena" "$R" "wait" "wait" "vr_mg_hub_test 3" \
    $(printf 'wait %.0s' $(seq 1 ${AFTERWAIT:-900})) "echo MARK still both" "$R" "wait" "wait" "toggleconsole" "quit" \
    | sed 's/^wait $/wait/' > "$T/quakevr/mg1route101.cfg"
CL="developer 1;wait${CLWAIT:-1200};vr_campaign_native mg1;wait200;connect 127.0.0.1:26010;wait700;echo MARK client in"
CL="$CL;wait${CLSTAY:-6000};echo MARK client leaving;disconnect;wait200;toggleconsole;quit"
powershell -NoProfile -File "$KIT/run.ps1" -Name $NAME -Instance 1 -RealTime -Timeout ${TMO:-420} -ExtraArgs "-listen 2 -port 26010 -ip 127.0.0.1 -nomapindex" -Script "$SV" > "$W/mg1coop_sv_$TAG.log" 2>&1 &
powershell -NoProfile -File "$KIT/run.ps1" -Name $NAME -Instance 2 -RealTime -Timeout ${TMO:-420} -ExtraArgs "-port 26011 -ip 127.0.0.1 -nomapindex" -Script "$CL" > "$W/mg1coop_cl_$TAG.log" 2>&1 &
wait
echo "-- server"
grep -a -E "MARK|mghordeplayer|mghubtest: (walk|intermission|report|player|press)|exited the level|Host_Error|ENGINE|TIMEOUT|exit=" "$W/mg1coop_sv_$TAG.log" | cut -c1-170 | head -40
echo "-- client"
grep -a -E "MARK|Host_Error|ENGINE|rror|TIMEOUT|exit=|horde[0-9]|exited the level" "$W/mg1coop_cl_$TAG.log" | cut -c1-170 | head -20
