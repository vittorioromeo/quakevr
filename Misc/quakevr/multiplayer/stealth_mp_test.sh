#!/bin/bash
# stealth_mp_test.sh <agent> [tag] -- the stealth AI in coop (QC vr_stealth_test2.qc scene 102; docs/vr-port/STEALTH_PLAN.md):
# a listen server (instance 1, the host) and a remote client (instance 2) over UDP on this machine (both bind 127.0.0.1:
# -ip, as mp_test.sh). The client turns its flashlight on (clipped to its head); the server then checks each client's
# light and lamp reach it, that a grunt facing both players spots the lit one as fast as one player alone (the meter on
# the most suspicious player), and that the client's lamp on the grunt's back makes it Alert at the client's lens.
# Logs: <worktree>/scratch/stealth_mp_server_<tag>.log, stealth_mp_client_<tag>.log. Exits 1 on a FAIL or no result.
NAME=${1:?agent}; TAG=${2:-a}
KIT="C:/OHWorkspace/qvr-kit"; W="C:/OHWorkspace/qvr-agents/$NAME/scratch"; mkdir -p "$W"
source "$KIT/slots.sh" "$NAME" mp
acquire_many 2
trap 'release' EXIT
SV="developer 0;coop 1;deathmatch 0;map e1m1;wait30;god;setpos 480 -250 88 0 90 0;wait${SVWAIT:-2400};echo MARK scene;vr_stealth_debug ${SDEBUG:-0};vr_stealth_test_quitclient 1;vr_stealth_test 102;wait2400;toggleconsole;quit"
CL="developer 0;wait${CLWAIT:-900};connect 127.0.0.1:26010;wait700;god;setpos ${CLPOS:-420 -352 88};wait60;vr_flashlight_clip_head right;wait5;vr_flashlight_toggle;vr_mock_look 12 0;echo MARK client lamp on"
powershell -NoProfile -File "$KIT/run.ps1" -Name $NAME -Instance 1 -RealTime -Timeout ${TMO:-200} -ExtraArgs "-listen 2 -port 26010 -ip 127.0.0.1" -Filter "stealth|rror|ENGINE|MARK" -Script "$SV" > "$W/stealth_mp_server_$TAG.log" 2>&1 &
powershell -NoProfile -File "$KIT/run.ps1" -Name $NAME -Instance 2 -RealTime -Timeout ${TMO:-200} -ExtraArgs "-port 26011 -ip 127.0.0.1" -Filter "rror|ENGINE|MARK|connect" -Script "$CL" > "$W/stealth_mp_client_$TAG.log" 2>&1 &
wait
grep -E "stealthtest|ENGINE" "$W/stealth_mp_server_$TAG.log"
grep -E "ENGINE" "$W/stealth_mp_client_$TAG.log"
grep -q "stealthtest: done" "$W/stealth_mp_server_$TAG.log" || exit 1
grep -qE "FAIL|ENGINE (ERROR|CRASH)" "$W/stealth_mp_server_$TAG.log" "$W/stealth_mp_client_$TAG.log" && exit 1
exit 0
