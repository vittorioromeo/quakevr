#!/usr/bin/env python3
"""Targeted crate ledge/carry regression, headless. Usage: python .../prop_grab_test.py <agent> [directions limits empty heavy disabled body explobox]."""
from pathlib import Path
import re
import subprocess
import sys

name = sys.argv[1]
selected = set(sys.argv[2:])

def wanted(case):
    return not selected or case in selected
root = Path('C:/OHWorkspace/qvr-agents') / name
logs = root / 'scratch' / 'grab_test'
logs.mkdir(parents=True, exist_ok=True)
bash = 'C:/Program Files/Git/bin/bash.exe'
base = 'map vrclimb;wait30;god;notarget;developer 1;vr_climb 1;vr_movement_mode 1;vr_stick_swap 0;vr_carry_move_min 0.35;vr_carry_move_mass 100;vr_carry_move_size 1;vr_carry_body_collision 1;setpos -300 -100 24 0 90 0;noclip;wait20;'

def run(case, commands):
    result = subprocess.run([bash, 'C:/OHWorkspace/qvr-kit/run.sh', name, '-Script', base + commands + ';toggleconsole;quit',
        '-Filter', 'climbtry|carryloadtest|carry: stopped|carry: taken|carry: both hands|not put|ENGINE|ERROR', '-Timeout', '90'],
        capture_output=True, text=True)
    (logs / (case + '.log')).write_text(result.stdout + result.stderr)
    assert result.returncode == 0 and 'exit=0' in result.stdout, (case, result.stdout[-800:])
    assert 'FAIL' not in result.stdout, (case, result.stdout[-800:])
    return result.stdout

if wanted('directions'):
    commands = ['vr_test_spawn_dist 40', 'vr_test_spawn 120', 'impulse 241', 'wait90']
    for px, py, yaw, lipx, lipy, dx, dy in [(-300,-100,90,-300,-80,0,1), (-340,-60,0,-320,-60,1,0),
            (-300,-20,270,-300,-40,0,-1), (-260,-60,180,-280,-60,-1,0)]:
        commands += [f'setpos {px} {py} 24 0 {yaw} 0', 'noclip', 'wait15']
        for inset in (0,4,10,14):
            commands.append(f'vr_climb_try {lipx+dx*inset} {lipy+dy*inset} 48')
    log = run('directions', ';'.join(commands))
    assert len(re.findall(r'main: exact hold', log)) == 16, log
    print('PASS ledges: 4 directions x 4 extensions (0,4,10,14 units) = 16 exact holds')

if wanted('limits'):
    run('limits', 'vr_test_spawn_dist 40;vr_test_spawn 120;impulse 241;wait90;vr_climb_try -300 -60 48;vr_climb_try -300 -80 30')
    log = (logs / 'limits.log').read_text()
    assert len(re.findall(r'main: none', log)) == 2, log
    print('PASS limits: unreachable centre and deep-under-lip rejected')

hold = 'vr_mock_hand main 0.12 1.8 -1.0 0 0 0;wait15;+grabmain;vr_test_spawn_hold 1;vr_test_spawn 108;impulse 241;wait20;'
for case, fixture, expected in [('empty','',320.0), ('heavy',hold,320/1.65), ('disabled',hold+'vr_carry_move_min 1;',320.0)]:
    if not wanted(case):
        continue
    log = run(case, fixture+'vr_mock_stick off 0 1;wait20;vr_carry_load_test;vr_mock_stick off 0 0')
    scale, speed = map(float, re.search(r'current speed scale ([0-9.]+), horizontal speed ([0-9.]+)', log).groups())
    assert abs(speed-expected)<1, (case, scale, speed)
    if case=='heavy':
        assert '1 solid props' in log and 'physical 0.0000 drawn 0.0000' in log, log
        assert 'same prop in both hands scale 0.606' in log, log
    print(f'PASS {case}: full stick {speed:.3f} units/s, scale {scale:.3f}')

if wanted('body'):
    log = run('body', hold+'vr_mock_hand_to main -300 -120 72;wait20;vr_carry_load_test;vr_mock_hand_to main -300 -180 72;wait30;vr_carry_load_test')
    assert 'physical 0.0000 drawn 0.0000' in log, log
    assert 'carry: stopped at the body, let go' in log and '0 solid props' in log, log
    print('PASS body: no physical/drawn penetration; long pull releases from the torso stop')
if wanted('explobox'):
    fixture = 'vr_climb 0;vr_mock_hand main 0.12 1.8 -1.0 0 0 0;vr_mock_hand off -0.86 1.8 -0.95 0 0 0;wait15;vr_physics_spawn misc_explobox 100;wait60;vr_rigid_place misc_explobox main -3 0 -3 0 90 0;+grabmain;vr_mock_button main grip 1;wait20;+graboff;vr_mock_button off grip 1;wait20;'
    log = run('explobox', fixture+'vr_mock_stick off 0 1;wait20;vr_carry_load_test;vr_mock_stick off 0 0')
    scale, speed = map(float, re.search(r'current speed scale ([0-9.]+), horizontal speed ([0-9.]+)', log).groups())
    assert 'carry: both hands' in log and '1 solid props' in log, log
    assert 'physical 0.0000 drawn 0.0000' in log and 0.35 < scale < 0.8 and abs(speed-320*scale)<1, log
    print(f'PASS explosive box in both hands: full stick {speed:.3f} units/s, scale {scale:.3f}, zero torso overlap')
print(f'PASS requested prop-grab checks; logs: {logs}')
