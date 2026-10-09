#!/bin/bash
# Listen server (instance 1, host = client 1) + a remote client (instance 2) over UDP on this machine.
# (docs/vr-port/MULTIPLAYER.md, "Debris and effects", "Server rules"). Both games bind 127.0.0.1 (-ip): without it the
# server binds the host name's address, which a "connect 127.0.0.1" never reaches.
#   bash mp_test.sh <agent> [tag] [server's vr_debris_mp_max]   (env: SVWAIT, CLWAIT frames; CLPOS "x y z"; TMO s; DEV)
# Logs: <worktree>/scratch/mp_server_<tag>.log, mp_client_<tag>.log; the client's menu (Melee, its rows the server's)
# in mp_client_<tag>.png. The host moves off the start spot first (the client would spawn there and telefrag it).
# The explosion step is flaky in multiplayer: impulse 232 flings the nearest rigid body, which can be a player's
# (80.5 kg), and the box may then blow up out of the way (no chunks anywhere). A run where it hits: both clients
# report 16 chunks of their own (2026-10-06). vr_physics_blast's own effect was lost until 2026-10-09 (it ran between frames, and the
# server frame cleared the datagram first; now queued for the next frame: its explosion and chunks reach every client).
NAME=${1:?agent}; TAG=${2:-a}; MPMAX=${3:-64}
KIT="C:/OHWorkspace/qvr-kit"; W="C:/OHWorkspace/qvr-agents/$NAME/scratch"; mkdir -p "$W"
source "$KIT/slots.sh" "$NAME" mp
acquire_many 2
trap 'release' EXIT
SV="developer ${DEV:-0};coop 0;deathmatch 0;vr_debris_mp_max $MPMAX;vr_melee_speed 3;map e1m1;wait30;setpos 480 -250 88 0 90 0;wait${SVWAIT:-3000};vr_debris_list;vr_net_stats reset;wait90;echo MARK idle;vr_net_stats;vr_melee_speed 5;echo MARK rule changed to 5;god;vr_test_spawn 102;vr_test_spawn_dist 160;impulse 241;wait90;vr_net_stats reset;vr_test_fling_at 1;vr_test_fling_speed 25;impulse 232;echo MARK box flung;wait200;vr_test_fling_speed 40;impulse 232;wait100;echo MARK after explosion;vr_net_stats;vr_explosion_debris_stats;vr_physics_spawn vr_crate 80;wait10;vr_crates_list;wait80;vr_net_stats reset;wait30;echo MARK crate standing;vr_net_stats;vr_physics_blast ${BX:-480 -170 72} 150;wait8;echo MARK crate pieces flying;vr_net_stats;vr_crates_list;vr_net_stats reset;wait200;echo MARK pieces at rest;vr_net_stats;wait1800;toggleconsole;quit"
CL="developer ${DEV:-0};vr_melee_speed 2.5;wait${CLWAIT:-1500};connect 127.0.0.1:26010;wait700;god;setpos ${CLPOS:-420 -352 88};wait60;echo MARK client in;vr_serverrules"
for k in $(seq 1 11); do CL="$CL;wait300;echo MARK client tick $k;vr_serverrules;vr_explosion_debris_stats"; done
CL="$CL;menu_vr 5;wait60;screenshot;wait10;toggleconsole;quit"
powershell -NoProfile -File "$KIT/run.ps1" -Name $NAME -Instance 1 -RealTime -Timeout ${TMO:-260} -ExtraArgs "-listen 2 -port 26010 -ip 127.0.0.1" -Script "$SV" > "$W/mp_server_$TAG.log" 2>&1 &
powershell -NoProfile -File "$KIT/run.ps1" -Name $NAME -Instance 2 -RealTime -Timeout ${TMO:-260} -ExtraArgs "-port 26011 -ip 127.0.0.1" -Clean -Out "$W/mp_client_$TAG.png" -Script "$CL" > "$W/mp_client_$TAG.log" 2>&1 &
wait
