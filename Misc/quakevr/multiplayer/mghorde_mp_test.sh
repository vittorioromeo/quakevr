#!/bin/bash
# MG1 Horde coop over UDP on this machine: a listen server (instance 1, host = player 1) and a remote client
# (instance 2), both on the owned rerelease MG1 data (docs/vr-port/EXPANSIONS.md, "Native MG1 Horde").
#   bash mghorde_mp_test.sh <agent> [tag] [map]     (env: SVWAIT, CLWAIT frames; TMO s)
# Checks, from the server's log (scratch/mghorde_sv_<tag>.log, "mghorde" lines):
#   - the wave budget with two living players (player scalar 1.25; check_horde_waves.py),
#   - a dead client waits while the host lives, then the wave boundary revives it (deadflag 0, health 100),
#   - the boss wave's key is shared currency (both players carry the key bit),
#   - a team wipe (vr_mg_horde_test 17) and the host's press restart the arena for both players,
#   - a client that leaves no longer counts as alive (health 0, "alive 1").
NAME=${1:?agent}; TAG=${2:-a}; MAP=${3:-horde1}
KIT="C:/OHWorkspace/qvr-kit"; W="C:/OHWorkspace/qvr-agents/$NAME/scratch"; mkdir -p "$W"
source "$KIT/slots.sh" "$NAME" mp
acquire_many 2
trap 'release' EXIT
R="vr_mg_horde_test 15"
SV="developer 1;coop 1;deathmatch 0;vr_campaign_native mg1;wait120;skill 1;map $MAP;wait30;god;notarget"
SV="$SV;vr_mg_horde_test 13;wait${SVWAIT:-2400};echo MARK both in;$R"
# Wave 1..3 with the client dead from wave 1 on: it must stay dead until a wave ends, then come back.
SV="$SV;wait600;echo MARK client should be dead;$R;wait300;vr_mg_horde_test 3;wait120;echo MARK after wave end;$R"
for i in 1 2 3 4 5 6; do SV="$SV;wait500;vr_mg_horde_test 3;wait5;vr_mg_horde_test 5"; done
SV="$SV;wait60;echo MARK after boss key;$R"
# Team wipe: both players die; the host's release-then-press restarts the arena for both (fresh wave 0, no keys).
# The restart is queued after the rest of this script, so the server continues from mghorde_after.cfg.
SV="$SV;vr_mg_horde_test 17;wait200;echo MARK team wiped;$R;wait5;+attack;wait3;-attack;wait5;vr_mg_horde_test 16"
AFTER="wait600;echo MARK after wipe;$R;wait${LEAVEWAIT:-3000};echo MARK client left;$R;wait30;toggleconsole;quit"
mkdir -p "$KIT/bases/$NAME/qbase.1/id1"
python -c "import re,sys; open(sys.argv[1],'w').write(re.sub(r'wait([0-9]+)',lambda m:';'.join(['wait']*int(m.group(1))),sys.argv[2])+chr(10))" "$KIT/bases/$NAME/qbase.1/id1/mghorde_after.cfg" "$AFTER"
CL="developer 1;wait${CLWAIT:-1200};vr_campaign_native mg1;wait200;connect 127.0.0.1:26010;wait700;god;wait300;echo MARK client in"
CL="$CL;god;kill;wait60;echo MARK client killed itself;wait${CLLEAVE:-7000};echo MARK client leaving;disconnect;wait200;toggleconsole;quit"
powershell -NoProfile -File "$KIT/run.ps1" -Name $NAME -Instance 1 -RealTime -Timeout ${TMO:-480} -ExtraArgs "-listen 2 -port 26010 -ip 127.0.0.1 -nomapindex" -Script "$SV" > "$W/mghorde_sv_$TAG.log" 2>&1 &
powershell -NoProfile -File "$KIT/run.ps1" -Name $NAME -Instance 2 -RealTime -Timeout ${TMO:-480} -ExtraArgs "-port 26011 -ip 127.0.0.1 -nomapindex" -Script "$CL" > "$W/mghorde_cl_$TAG.log" 2>&1 &
wait
grep -a -E "MARK|mghordeplayer|mghordetest|Host_Error|ENGINE|rror" "$W/mghorde_sv_$TAG.log" | grep -v "actual killed\|key collected" | head -60
python "C:/OHWorkspace/qvr-agents/$NAME/Misc/quakevr/check_horde_waves.py" "$W/mghorde_sv_$TAG.log" --min-waves 2 | tail -4
grep -a -E "MARK|rror|Host_Error|ENGINE" "$W/mghorde_cl_$TAG.log" | head -12
