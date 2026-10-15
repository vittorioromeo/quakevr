#!/bin/bash
# toolgun_panel_test.sh <agent> [--shots] -- the toolgun's own panel and its controls, headless (docs/vr-port/ROUND22.md,
# "The toolgun's own panel"; docs/vr-port/TOOLGUN.md). vrfiringrange, the gun in the main hand, the off hand's mock laser
# (vr_toolgun_mock_laser) on the panel and its trigger clicking:
#   1. the panel opened by B; the laser at the Props tab's middle and the Barrel tile's by their panel pixels (the
#      layout's 256 x 192: the tiles at their new size), each clicked: the spawn choice is the barrel;
#   2. the tools cycled by the gun's stick press (the gun's screen says each tool), and once back with X/A held;
#   3. the Joint tool's tile, its Hinge tile: vr_toolgun_joint 2; More: God Mode switched on and off; the Panel Size
#      slider dragged;
#   4. the physgun holding a crate while the mock hand rolls 45 degrees, then turns 30 to its left: the crate's angles
#      follow the hand (roll, then yaw); no button, both sticks push it away, turn it and raise it, unsnapped, while
#      the player neither walks nor turns; each stick's press: swallowed (still the physgun, still holding); X/A held,
#      its angles snap to 15 degree steps (the stick's press swallowed too), and stay snapped when X/A is let go; let
#      go of the crate, the off stick walks again;
#   5. a grunt made by the spawn tool, held by the physgun while the hand rolls and turns: only its yaw follows; the hand
#      pointing up, then tilted on over the top: its yaw barely turns (not the Euler yaw's half turn);
#   6. Remove's "Clear Props Made Since the Map Loaded" found by name; More open, the stick press: More's page closes.
# --shots: one picture of the panel's pages (Spawn's props, Joint, Physgun, More), scratch/toolgun_panel.png.
# PASS: every step as said, no crash.
AGENT=${1:-toolgun2}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
SHOTS=; [ "$2" = "--shots" ] && SHOTS=1
S=$(SHOTS=$SHOTS python - <<'EOF'
import os
shots = bool(os.environ.get('SHOTS'))
s = []
a = s.append
def click():
    a('wait2;vr_mock_button off trigger 1;wait2;vr_mock_button off trigger 0;wait3')
def at(x, y):
    a(f'vr_toolgun_mock_laser {x} {y}'); click()
def name(n):
    a(f'vr_toolgun_mock_laser {n}'); click()
def shot():
    if shots:
        a('wait5;screenshot')
def hand(pitch, yaw, roll, y=1.5, z=-0.3):
    a(f'vr_mock_hand main 0.0 {y} {z} {pitch} {yaw} {roll};wait20')
a('map vrfiringrange;wait60;god;notarget;vr_crate_health 0;vr_toolgun_tool 0;vr_toolgun_joint 0;vr_toolgun_panel_size 6')
a('impulse 169;vr_weapon_grip_mode 1;wait10')
hand(70, 0, 0)
# 1. The panel; the Props tab (x 67..126, y 47..60) and the third tile of its first row (x 130..189, y 63..115).
a('vr_mock_button main secondary 1;wait2;vr_mock_button main secondary 0;wait10')
at(96, 53)
at(160, 89)
a('echo STEP1;vr_toolgun_status')
shot()
# 2. The tools cycled by the gun's stick press, and one back with X/A held.
for i in range(6):
    a('vr_mock_button main stickclick 1;wait2;vr_mock_button main stickclick 0;wait3;echo CYCLE;vr_toolgun_status')
a('vr_mock_button main primary 1;wait2;vr_mock_button main stickclick 1;wait2;vr_mock_button main stickclick 0;wait2')
a('vr_mock_button main primary 0;wait3;echo BACK;vr_toolgun_status')
# 3. Joint, Hinge; More, God Mode twice; the Panel Size slider (its cells from x 96: 3 at x 100, 12 at x 172).
name('Joint'); name('Hinge')
a('echo JOINT;vr_toolgun_joint')
shot()
name('Physgun')
shot()
name('More')
name('God Mode')
a('echo GOD1;vr_toolgun_panel_dump')
shot()
name('God Mode')
a('vr_toolgun_mock_laser 100 180;wait2;vr_mock_button off trigger 1;wait3;vr_toolgun_mock_laser 136 180;wait3')
a('vr_toolgun_mock_laser 172 180;wait3;vr_mock_button off trigger 0;wait3;echo SIZE;vr_toolgun_panel_size;vr_toolgun_panel_size 6')
name('More')
a('vr_mock_button main secondary 1;wait2;vr_mock_button main secondary 0;wait5;echo CLOSED;vr_toolgun_status')
# 4. The physgun and a crate ahead on the floor.
a('vr_toolgun_mock_laser off;vr_toolgun_tool 2;vr_physics_spawn vr_crate 96 0;wait60')
hand(50, 0, 0, 1.35)
a('vr_mock_button main trigger 1;wait20;echo HOLD0;vr_toolgun_status')
hand(50, 0, 45, 1.35)
a('echo ROLL45;vr_toolgun_status')
hand(50, 30, 45, 1.35)
a('echo YAW30;vr_toolgun_status')
hand(50, 30, 37, 1.35)
# (no button: both sticks push, turn, raise and slide the crate; the player neither walks nor turns; nothing snaps)
a('echo PRESTICK;viewpos;vr_toolgun_status')
a('vr_mock_stick main 0.8 1;vr_mock_stick off 1 1;wait30;vr_mock_stick main 0 0;vr_mock_stick off 0 0;wait5')
a('echo STICKED;viewpos;vr_toolgun_status')
# (each stick's press, no X/A: swallowed while the crate is held, no tool change, no run or bullet time)
a('vr_mock_button off stickclick 1;wait2;vr_mock_button off stickclick 0;wait2')
a('vr_mock_button main stickclick 1;wait2;vr_mock_button main stickclick 0;wait3;echo PRESSHOLD;vr_toolgun_status')
a('vr_mock_button main primary 1;wait5;echo SNAP;vr_toolgun_status')
# (the stick pushed hard while snapping: its press, swallowed while the trigger holds the crate)
a('vr_mock_button main stickclick 1;wait2;vr_mock_button main stickclick 0;wait3;echo STICKHOLD;vr_toolgun_status')
# (X/A let go: the snapped angles stay)
a('vr_mock_button main primary 0;wait5;echo SNAPKEPT;vr_toolgun_status')
a('vr_mock_button main trigger 0;wait10;vr_toolgun_icons')
# (let go: the off stick walks again)
a('echo PREWALK;viewpos;vr_mock_stick off 0 1;wait30;vr_mock_stick off 0 0;wait5;echo WALKED;viewpos')
# 5. A grunt made by the spawn tool where the gun points, held by the physgun while the hand rolls: it turns only about
#    the up axis (its pitch and roll stay).
a('vr_scene_clean props;wait10;vr_toolgun_select 0;vr_toolgun_tool 0;wait5')
hand(40, 0, 0, 1.35)
a('vr_mock_button main trigger 1;wait3;vr_mock_button main trigger 0;wait30;vr_toolgun_tool 2;wait5')
a('vr_mock_button main trigger 1;wait20;echo MONSTER0;vr_toolgun_status')
hand(40, 25, 45, 1.35)
a('echo MONSTER1;vr_toolgun_status')
# (the controller pitched 80, then tilted 20 degrees on over its top: pitch 100 is Euler (80, 180, 180), its yaw a half
# turn away; its way barely turns)
hand(80, 0, 0, 1.35)
a('echo MONSTER2;vr_toolgun_status')
hand(100, 0, 0, 1.35)
a('echo MONSTER3;vr_toolgun_status;vr_mock_button main trigger 0;wait10')
# 6. Remove's long button found by its whole name; More open, the stick press changes the tool and closes More.
a('vr_toolgun_tool 1;vr_mock_button main secondary 1;wait2;vr_mock_button main secondary 0;wait10')
a('vr_toolgun_mock_laser Clear Props Made Since the Map Loaded;wait3;echo LONGNAME')
name('More')
a('echo MORE1;vr_toolgun_panel_dump')
a('vr_mock_button main stickclick 1;wait2;vr_mock_button main stickclick 0;wait5;echo MORE2;vr_toolgun_panel_dump;echo MOREEND')
a('wait5;toggleconsole;quit')
print(';'.join(s))
EOF
)
OUT=; [ -n "$SHOTS" ] && OUT="-Clean -Out toolgun_panel.png"
out=$(bash $KIT/run.sh $AGENT $OUT -Script "$S" -Timeout 600 -Filter 'STICK|WALK|Player pos|STEP|CYCLE|BACK|JOINT|GOD|SIZE|CLOSED|HOLD|ROLL|YAW|SNAP|MONSTER|LONGNAME|MORE|God Mode|toolgun|vr_toolgun|ENGINE|CRASH|exit=|TIMEOUT|composed' 2>&1)
[ -n "$VERBOSE" ] && echo "$out" | tail -80
echo "$out" | python -c "
import sys, re
t = sys.stdin.read()
def after(tag, pat):
    m = re.search(tag + r'.*?' + pat, t, re.S)
    return m
fails = []
if 'toolgun: panel open' not in t: fails.append('panel not opened')
if not after('toolgun: spawn Barrel', 'STEP1'): fails.append('Barrel not chosen by its tile')
screens = re.findall(r'CYCLE\s*\ntoolgun: hand 1, tool (\w+), panel \w+, adjust \d, screen \"([^\"]*)\"', t)
want = ['Remove', 'Physgun', 'Scale', 'Joint', 'Brush', 'Spawn']
if [x[0] for x in screens] != want: fails.append(f'cycle {screens}')
if any(x[0] != 'Spawn' and not x[1].upper().startswith(x[0].upper()) and not x[1].startswith('Weld') for x in screens): fails.append(f'screens {screens}')
if not re.search(r'BACK\s*\ntoolgun: hand 1, tool Brush', t): fails.append('X/A + stick press: not back to Brush')
if not re.search(r'JOINT\s*\n\"vr_toolgun_joint\" is \"2\"', t): fails.append('Hinge tile')
if 'toolgun: God Mode' not in t: fails.append('God Mode button')
m = re.search(r'SIZE\s*\n\"vr_toolgun_panel_size\" is \"([\d.]+)\"', t)
if not m or abs(float(m.group(1)) - 12) > 0.01: fails.append(f'slider dragged to {m.group(1) if m else None}')
if not re.search(r'CLOSED\s*\ntoolgun: hand 1, tool \w+, panel closed', t): fails.append('panel not closed by B')
def angles(tag):
    m = re.search(tag + r'.*?physgun holds \d+ at [-\d ]+\(distance \d+\), angles ([-\d.]+) ([-\d.]+) ([-\d.]+)', t, re.S)
    return [float(v) for v in m.groups()] if m else None
h0, r45, y30, sn = angles('HOLD0'), angles('ROLL45'), angles('YAW30'), angles('SNAP')
print('angles held', h0, 'rolled', r45, 'turned', y30, 'snapped', sn)
import math
def d(a, b): return (a - b + 180) % 360 - 180
def axes(ang):
    # An alias model's axes from its angles (its pitch inverted, as R_EntityMatrix turns it): forward, left, up.
    p, y, r = math.radians(-ang[0]), math.radians(ang[1]), math.radians(ang[2])
    sp, cp, sy, cy, sr, cr = math.sin(p), math.cos(p), math.sin(y), math.cos(y), math.sin(r), math.cos(r)
    f = (cp * cy, cp * sy, -sp)
    rt = (-sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp)
    u = (cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp)
    return [f, tuple(-v for v in rt), u]
def turned(a0, a1):
    # The angle of the rotation from one orientation to the other (degrees).
    m0, m1 = axes(a0), axes(a1)
    tr = sum(m1[i][k] * m0[i][k] for i in range(3) for k in range(3))
    return math.degrees(math.acos(max(-1.0, min(1.0, (tr - 1) / 2))))
if not (h0 and r45 and y30 and sn): fails.append('physgun angles missing')
else:
    print(f'turned by the roll {turned(h0, r45):.1f}, by the yaw {turned(r45, y30):.1f}')
    if abs(turned(h0, r45) - 45) > 5: fails.append('roll did not follow the hand')
    if abs(turned(r45, y30) - 30) > 5: fails.append('yaw did not follow the hand')
    if any(abs(d(v, round(v / 15) * 15)) > 0.05 for v in sn): fails.append('not snapped')
m0, m1 = angles('MONSTER0'), angles('MONSTER1')
print('monster held', m0, 'turned', m1)
if not (m0 and m1) or abs(m1[0] - m0[0]) > 0.01 or abs(m1[2] - m0[2]) > 0.01 or abs(d(m1[1], m0[1])) < 5:
    fails.append('the monster did not turn about its up axis alone')
m2, m3 = angles('MONSTER2'), angles('MONSTER3')
print('monster pointing up', m2, 'tilted over the top', m3)
if not (m2 and m3) or abs(d(m3[1], m2[1])) > 10:
    fails.append('the monster spun as the hand went over the top')
for tag in ('STICKHOLD', 'PRESSHOLD'):
    if not re.search(tag + r'\s*\ntoolgun: hand 1, tool Physgun[^\n]*\n(toolgun: (?!physgun)[^\n]*\n)*toolgun: physgun holds', t):
        fails.append(f'{tag}: stick press while holding: the tool changed or the crate was dropped')
def pos(tag):
    m = re.search(tag + r'.*?Player pos: \(([-\d]+) ([-\d]+) ([-\d]+)\) [-\d]+ ([-\d]+)', t, re.S)
    return [float(v) for v in m.groups()] if m else None
def held(tag):
    m = re.search(tag + r'.*?physgun holds \d+ at ([-\d]+) ([-\d]+) ([-\d]+) \(distance (\d+)\)', t, re.S)
    return [float(v) for v in m.groups()] if m else None
p0, p1, w0, w1 = pos('PRESTICK'), pos('STICKED'), pos('PREWALK'), pos('WALKED')
c0, c1 = held('PRESTICK'), held('STICKED')
st, kept = angles('STICKED'), angles('SNAPKEPT')
print('player', p0, p1, 'crate', c0, c1, 'angles', angles('PRESTICK'), st, 'kept', kept, 'walk', w0, w1)
if not (p0 and p1 and c0 and c1 and st and w0 and w1 and kept): fails.append('stick step prints missing')
else:
    if max(abs(a - b) for a, b in zip(p0, p1)) > 1: fails.append('the player moved or turned while the physgun held')
    if c1[3] - c0[3] < 5: fails.append('the gun stick did not push the crate away')
    if c1[2] - c0[2] < 5: fails.append('the other stick did not raise the crate')
    if turned(angles('PRESTICK'), st) < 5: fails.append('the gun stick did not turn the crate')
    if all(abs(d(v, round(v / 15) * 15)) < 0.5 for v in st): fails.append('snapped without X/A')
    if any(abs(d(a, b)) > 0.5 for a, b in zip(kept, sn or kept)): fails.append(f'snap not kept on letting go of X/A {kept} {sn}')
    if abs(w1[0] - w0[0]) + abs(w1[1] - w0[1]) < 10: fails.append('the off stick did not walk after the physgun let go')
if re.search(r'no \"Clear Props Made Since the Map Loaded\"', t): fails.append('long widget name not found')
m = re.search(r'MORE1(.*?)MORE2(.*?)MOREEND', t, re.S)
if not m or 'God Mode' not in m.group(1) or 'God Mode' in m.group(2): fails.append('More not closed by the stick press')
if re.search(r'ENGINE CRASH|ENGINE ERROR|TIMEOUT|exit=[1-9]', t): fails.append('crash')
print('FAIL: ' + '; '.join(fails) if fails else 'PASS')
"
