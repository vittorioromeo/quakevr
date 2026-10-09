#!/bin/bash
# toolgun_joint_menu_test.sh <agent> [--debug] -- the toolgun's Joint choice walked on its menu, headless (the author's
# crash report of 2026-10-09 18:53, "selecting a joint type": docs/vr-port/ROUND21.md, "Toolgun: the joint choice
# crash"). vrfiringrange, three crates ahead; the menu on the gun, the other hand's mock laser opens the Joint row's list
# (7 choices: a drop-down list, vr_menu_dropdown 4) and picks. Each hand holding the gun in turn (main: the off hand's
# laser; off: the main hand's):
#   1. the physgun holding a crate while every ordered pair of joint choices is picked (an Euler circuit of the 7
#      choices, each from each, and each again over itself: 50 picks), the gun swung meanwhile; then the Tool list:
#      Joint and back to Physgun mid-drag;
#   2. the map again, two crates, a joint half made (the first crate shot) while the choice is walked again, then for each joint kind: its choice
#      picked between the two shots ("joined"), and Unjoin last ("removed");
# HANDS="main" or "off": one hand only; VERBOSE=1: the run's lines.
# --debug: the Debug build (Zancle asserts, the debug CRT heap; build.sh <agent> --debug first).
# PASS: every pick sets vr_toolgun_joint to the choice clicked, every kind joined, the unjoin removes them, no crash.
AGENT=${1:-tgcrash}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
DBG=; [ "$2" = "--debug" ] && DBG=-Debug
fail=0
for gun in ${HANDS:-main off}; do
    S=$(python - "$gun" <<'EOF'
import sys
gun = sys.argv[1]
laser = 'off' if gun == 'main' else 'main'
hx = '0.1' if gun == 'main' else '-0.1'
s = []
a = s.append
cur = [0]
count = [0]
def menu():
    a(f'vr_mock_button {gun} secondary 1;wait2;vr_mock_button {gun} secondary 0;wait6')
def click(x, y):
    a(f'vr_mock_laser {x} {y};wait2;vr_mock_button {laser} trigger 1;wait2;vr_mock_button {laser} trigger 0;wait3')
def pick(k):
    # The Joint row (its top at 98: menu_vr rows) opens the list with the current choice level with the row.
    click(220, 102)
    click(230, 98 - 8 * cur[0] + 8 * k + 4)
    cur[0] = k
    count[0] += 1
    a(f'echo PICK {k};vr_toolgun_joint')
def aim(pitch, yaw):
    a(f'vr_mock_hand {gun} {hx} 1.35 -0.3 {pitch} {yaw} 0;wait3')
def shoot():
    a(f'vr_mock_button {gun} trigger 1;wait3;vr_mock_button {gun} trigger 0;wait3')
# Every ordered pair (i, j), i != j, once: an Euler circuit of the complete directed graph on 7 (Hierholzer), from 0.
edges = {i: [j for j in range(7) if j != i] for i in range(7)}
stack, circuit = [0], []
while stack:
    v = stack[-1]
    if edges[v]:
        stack.append(edges[v].pop())
    else:
        circuit.append(stack.pop())
circuit.reverse()
walk = []
for v in circuit[1:]:
    walk.append(v)
    if v not in walk[:-1]:
        walk.append(v) # (and each over itself once)
a('map vrfiringrange;wait60;god;notarget;vr_crate_health 0;vr_toolgun_joint 0')
a('impulse 169' if gun == 'main' else 'impulse 189')
a('vr_weapon_grip_mode 1;vr_physics_spawn vr_crate 96 0;vr_physics_spawn vr_crate 96 48;vr_physics_spawn vr_crate 96 -48;wait60')
# 1. The physgun holding a crate.
aim(50, 0)
a('vr_toolgun_tool 2;wait3')
a(f'vr_mock_button {gun} trigger 1;wait10;echo HOLD;vr_toolgun_status')
menu()
for n, k in enumerate(walk):
    pick(k)
    if n % 5 == 0:
        aim(45 + n % 10, -20 + (n * 7) % 40)
click(220, -12); click(230, -16 - 8 * 2 + 8 * 4 + 4) # the Tool list: Joint
a('echo TOOL;vr_toolgun_tool')
click(220, -12); click(230, -16 - 8 * 4 + 8 * 2 + 4) # and Physgun again
a('echo TOOL;vr_toolgun_tool')
menu()
a(f'vr_mock_button {gun} trigger 0;wait30')
# 2. Joints: one half made while the choice is walked, then each kind made.
a('map vrfiringrange;wait60;god;notarget;vr_crate_health 0') # (the map again: the swung crate may have pushed you)
a('impulse 169' if gun == 'main' else 'impulse 189')
aim(50, 0); a(f'vr_mock_hand {laser} {"-0.2" if laser == "off" else "0.2"} 1.0 -0.2 0 0 0;wait30') # (the laser hand down: the body faces ahead again)
a('vr_weapon_grip_mode 1;vr_physics_spawn vr_crate 96 0;vr_physics_spawn vr_crate 96 48;wait60')
a('vr_toolgun_tool 4;wait3')
aim(50, 0); shoot()
menu()
for k in walk[:14]:
    pick(k)
for k in range(7):
    pick(k)
    menu(); aim(50, 0); shoot(); aim(50, 25); shoot(); a('wait5'); menu()
a('wait20;echo END %d;vr_toolgun_status' % count[0])
menu()
a('wait5;toggleconsole;quit')
print(';'.join(s))
EOF
)
    out=$(bash $KIT/run.sh $AGENT $DBG -Script "$S" -Timeout 900 -Filter 'PICK|TOOL|HOLD|END|vr_toolgun_joint|vr_toolgun_tool|toolgun: (physgun|Weld|Ball|Hinge|Slider|Rope|Spring|removed|joints)|ENGINE|CRASH|ssert|exit=|TIMEOUT' 2>&1)
    verdict=$(echo "$out" | python -c "
import sys, re
t = sys.stdin.read()
picks = re.findall(r'PICK (\d)\s*\n\"vr_toolgun_joint\" is \"(\d)\"', t)
bad = [p for p in picks if p[0] != p[1]]
tools = re.findall(r'\"vr_toolgun_tool\" is \"(\d)\"', t)
kinds = set(re.findall(r'toolgun: (Weld|Ball|Hinge|Slider|Rope|Spring) \d+ to \d+: joined', t))
removed = re.findall(r'toolgun: removed (\d+) joints of', t)
held = 'physgun holds' in t
crash = re.search(r'ENGINE CRASH|ssert|TIMEOUT|exit=[1-9]', t)
want = re.search(r'END (\d+)', t)
ok = want is not None and len(picks) == int(want.group(1)) and not bad and tools == ['4', '2'] and len(kinds) == 6 and removed and int(removed[0]) >= 1 and held and not crash
print(f'{len(picks)} picks ({len(bad)} wrong), tool list {\"/\".join(tools)}, kinds joined {len(kinds)}/6, unjoin removed {removed[0] if removed else \"-\"}, physgun held {held}, crash {bool(crash)}: ' + ('PASS' if ok else 'FAIL'))")
    [ -n "$VERBOSE" ] && echo "$out" | grep -v -E "^PICK|is \"[0-9]\"|GL" | tail -40 # (VERBOSE=1: the run's lines)
    echo "gun in the $gun hand: $verdict"
    case "$verdict" in *PASS) ;; *) fail=1; echo "$out" | grep -E "CRASH|ssert|#[0-9]|exit=" | head -20 ;; esac
done
[ $fail -eq 0 ] && echo "PASS" || echo "FAIL"
