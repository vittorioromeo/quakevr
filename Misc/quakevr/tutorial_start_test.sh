#!/bin/bash
# tutorial_start_test.sh -- the tutorial started after a multiplayer test scene is a single-player game of Quake's
# campaign (the author, 2026-11-23: it kept the scene's 4 client slots; VR_TutorialSinglePlayer, vr_gamedir.cpp), and
# its lesson 13 works there: the SPIKE shooter's spike battable, the GRENADE's grenade catchable, the pit deadly within
# 1 s (vr_pickup_test 25, QC vr_pickup_test.qc VR_TutPitTest_Start).
#   bash Misc/quakevr/tutorial_start_test.sh [agent]
# Ends on its verdict line "tutorial_start: N passed, M failed".
# (The waits: the scene's dm3 loaded and placed, 1.5 s of game time after its spawn; the tutorial's spawn and its first
# frames. Fixed frames: the same game time on any machine.)
AGENT=${1:-${AGENT:-mp}}
source "$(dirname "$0")/qvrtest.sh"
F='^SCNMP|^"(maxplayers|deathmatch|coop|teamplay|vr_activestartpaknameidx|vr_campaign)" is|^VR: the tutorial|^tutpit test|ENGINE CRASH|Host_Error'
S='vr_activestartpaknameidx 2;vr_scenario mp_dm_passive;wait300;skill 0;map vrtutorial;wait120;'
S="$S maxplayers;deathmatch;coop;teamplay;vr_activestartpaknameidx;vr_campaign;vr_pickup_test 25"
log=$(bash "$KIT/run.sh" "$AGENT" -Timeout 300 -QuitOn "tutpit test: [0-9]+ passed" -Script "$S" -Filter "$F" 2>&1 | grep -aE "$F")
echo "$log"
has() { grep -qaE "$1" <<<"$log" && echo 1 || echo 0; }
check "$(has '^SCNMP\|mp_dm_passive\|dm\|4 slots')" "the multiplayer scene ran first (4 slots, deathmatch)"
check "$(has '^"maxplayers" is "1"')" "the tutorial: one client slot"
check "$(has '^"deathmatch" is "0"')" "the tutorial: deathmatch 0"
check "$(has '^"coop" is "0"')" "the tutorial: coop 0"
check "$(has '^"teamplay" is "0"')" "the tutorial: teamplay 0"
check "$(has '^"vr_activestartpaknameidx" is "0"')" "the hub's chosen campaign: Quake (0)"
check "$(has '^"vr_campaign" is "0"')" "the game folders: Quake's campaign (0)"
expect_verdict "$log" "tutpit test" "lesson 13 in the tutorial (vr_pickup_test 25)"
verdict; exit $fail
