#!/bin/bash
# mapflameburn_test.sh <agent> [case regex]: real e1m2 flame contacts and portable torch controls.
# Logs stay in the agent worktree's scratch/mapflameburn-results (git-ignored); no saves or id assets are written by this check.
set -e
export PATH="/c/Program Files/Git/usr/bin:$PATH"
name=${1:-mapflameburn}
case_filter=${2:-.*}
cd "C:/OHWorkspace/qvr-agents/$name"
mkdir -p scratch/mapflameburn-results
run_case() {
 label="$1"; before="$2"; body="$3"
 [[ "$label" =~ $case_filter ]] || return 0
 prefix='vr_fixed_frames 1; developer 1; vr_debug_shots 1; vr_weapon_grip_mode 1; vr_body_collide 0; vr_burn_self 1; vr_burn_self_time 0.6; vr_burn_touch 1; vr_burn_drop 1; vr_walltorch 1; vr_mock_hand main 0.1 1.2 -0.35 70 0 0; vr_mock_hand off -0.1 1.2 -0.35 70 0 0'
 script="$prefix; $before; map e1m2; wait60; notarget; impulse 150; wait5; echo CASE_$label; $body; toggleconsole; quit"
 bash C:/OHWorkspace/qvr-kit/run.sh "$name" -Script "$script" -Filter 'CASE_|CHECK_|burning: map|burning: your torch|burning: player burns|walltorch grip result|taken immediately|burning: test 7|health |ENGINE|[Ee]rror' -Timeout 120 -ExtraArgs '-nomapindex -noaddons' > "scratch/mapflameburn-results/$label.stdout"
 cp C:/OHWorkspace/qvr-kit/bases/$name/qbase/qconsole.log "scratch/mapflameburn-results/$label.log"
 python - "$label" <<'PY'
from pathlib import Path
import sys,re
s=Path('scratch/mapflameburn-results/'+sys.argv[1]+'.log').read_text(errors='replace');s=s.split('CASE_'+sys.argv[1],1)[-1]
label=sys.argv[1]
w=re.findall(r"burning: (?:map flame|your torch's flame).*warning ([\d.]+)",s)
fires=re.findall(r"burning: (?:map flame|your torch's flame).*for ([\d.]+) s: you catch fire",s)
hp=[int(x) for x in re.findall(r'^health\s+(\d+)',s,re.M)]
assert hp and 'ENGINE CRASH' not in s and 'Host_Error' not in s, label
if label in ('brazier_main','brazier_off','wall_hand','static_wall_hand','portable_held','warning_time'):
    assert len(w)>=2 and fires and hp[-1]<100, label
    delay=1.5 if label=='warning_time' else 0.6
    assert delay<=float(fires[0])<=delay+0.15, (label,fires)
    assert s.index('warning')<s.index('you catch fire'), label
    if label!='static_wall_hand': assert hp[0]==100, (label,hp)
    if label=='portable_held': assert 'burning: map flame' not in s, label
elif label in ('brazier_body','wall_body'):
    assert not w and fires and float(fires[0])<=0.12 and hp[-1]<100, label
elif label=='portable_dropped':
    assert hp[-1]<100 and 'burning: map flame' not in s, label
else:
    assert not fires and all(x==100 for x in hp), (label,hp)
    if label=='withdraw_early': assert w, label
    else: assert not w, label
    if label=='detach': assert 'detached=1 held=1' in s and '(held)' in s, label
print('PASS', label, 'warnings='+str(len(w)), 'ignition='+str(fires[:1]), 'health='+str(hp))
PY
}
hand='setpos -96 595 375 0 90 0; wait30; vr_mock_hand_to main -96 634 414; wait20; echo CHECK_EARLY; edict 1; wait60; vr_mock_hand_to main -96 550 412; wait120; echo CHECK_DAMAGE; edict 1'
run_case brazier_main '' "$hand"
run_case brazier_off '' 'setpos -96 595 375 0 90 0; wait30; vr_mock_hand_to off -96 634 414; wait20; echo CHECK_EARLY; edict 1; wait60; vr_mock_hand_to off -96 550 412; wait120; echo CHECK_DAMAGE; edict 1'
run_case wall_hand '' 'setpos 1714 -170 312 0 243 0; wait30; vr_mock_hand_to main 1706 -206 324; wait20; echo CHECK_EARLY; edict 1; wait60; vr_mock_hand_to main 1750 -150 324; wait120; echo CHECK_DAMAGE; edict 1'
run_case brazier_body '' 'setpos -24 -232 428 0 -73 0; wait30; echo CHECK_DAMAGE; edict 1; wait90; edict 1'
run_case wall_body '' 'setpos 1706 -206 330 0 243 0; wait120; echo CHECK_DAMAGE; edict 1'
run_case static_wall_hand 'vr_walltorch 0' 'setpos 1714 -170 312 0 243 0; wait30; vr_mock_hand_to main 1706 -206 324; wait80; vr_mock_hand_to main 1750 -150 324; wait120; edict 1'
run_case hand_off 'vr_burn_self 0' "$hand"
run_case body_off 'vr_burn_touch 0' 'setpos -24 -232 428 0 -73 0; wait120; edict 1'
run_case withdraw_early '' 'setpos -96 595 375 0 90 0; wait30; vr_mock_hand_to main -96 634 414; wait20; vr_mock_hand_to main -96 550 412; wait150; edict 1'
run_case wall_stick '' 'setpos 1714 -170 312 0 243 0; wait30; vr_mock_hand_to main 1706 -206 292; wait150; edict 1'
run_case away '' 'setpos -96 545 375 0 90 0; wait150; edict 1'
take='setpos 1714 -170 312 0 243 0; vr_mock_hand main 0.4 1.2 -0.1 70 0 0; +grabright; vr_mock_button main grip 1; wait10; vr_test_walltorch_shot 24; wait20'
run_case detach '' "$take; vr_burn_test 7; wait5; vr_mock_hand_to off 1706 -206 324; wait150; edict 1"
run_case portable_held '' "$take; vr_burn_test 7; wait5; vr_mock_hand_to off 1701.8 -166.6 341.5; wait20; edict 1; wait60; vr_mock_hand_to off 1750 -150 324; wait120; edict 1"
run_case portable_dropped 'sv_gravity 0' "$take; -grabright; vr_mock_button main grip 0; wait10; vr_rigid_place 52 1800 -300 330 0 0 0; setpos 1800 -300 312 0 0 0; wait120; edict 1"
run_case portable_unlit 'vr_walltorch_die_time 0.1; vr_walltorch_relight 0' "$take; -grabright; vr_mock_button main grip 0; wait40; sv_gravity 0; vr_rigid_place 52 1800 -300 330 0 0 0; setpos 1800 -300 312 0 0 0; wait120; edict 1"
run_case warning_time 'vr_burn_self_time 1.5' 'setpos -96 595 375 0 90 0; wait30; vr_mock_hand_to main -96 634 414; wait80; edict 1; wait50; vr_mock_hand_to main -96 550 412; wait120; edict 1'
