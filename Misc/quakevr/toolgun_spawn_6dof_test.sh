#!/bin/bash
# toolgun_spawn_6dof_test.sh <agent> -- the spawn tool's ghost held by the hands instead of by a stick
# (docs/vr-port/TOOLGUN.md, "Spawn"; docs/vr-port/ROUND22.md, "The spawn tool's ghost, held by a hand").
# vrfiringrange, the toolgun in the main hand, the spawn tool with a grunt chosen:
#   1. the off hand's A/X held: its translation moves the ghost 1:1 along the gun's level forward, right and up (the
#      ghost does not jump at the press, the offsets are what the hand went by), turning that hand does nothing, the off
#      stick still walks, letting go keeps the offsets, and the trigger places the thing where that ghost stands;
#   2. the sticks are the player's with the spawn tool in hand: the off one walks, the gun's turns, and the ghost's
#      offsets and turn are untouched by either;
#   3. the gun hand's A/X held: the hand's way turns the ghost (its yaw only: 90 degrees of the hand's yaw, and every
#      step's turn equal to how far the gun's aim swung), its pitch and twist move nothing; the gun's stick still turns
#      the player and its press still changes the tool (A/X held: the one before);
#   4. Reset clears the offsets and the turn; with nothing chosen A/X is the game's again, with another tool in hand it
#      is the sticks' (vr_debug_buttons says who took each press);
#   5. the gun in the off hand: the same with the hands swapped.
# PASS: every step as said, no crash.
AGENT=${1:-tghost}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
S=$(python - <<'EOF'
s = []
a = s.append
def st(tag):
    a(f'echo {tag};vr_toolgun_status')
def look(tag):
    a(f'echo {tag};viewpos')
a('map vrfiringrange;wait60;god;notarget;vr_crate_health 0;vr_debug_buttons 1;vr_toolgun_tool 0;vr_toolgun_joint 0')
a('impulse 169;vr_weapon_grip_mode 1;wait10')
a('vr_mock_hand main 0.0 1.5 -0.3 40 0 0;wait20')
a('vr_toolgun_select 0;wait5')
st('S0')
# 1. The off hand's A/X held: 20 along the gun's forward, 10 to its right, 15 up (the player faces west, so the world
#    move is -20 +10 +15).
a('vr_mock_button off primary 1;wait3')
st('HOLD0')
a('vr_mock_hand_glide 1.0;vr_mock_hand_to off by -20 10 15;wait80')
st('MOVED')
a('vr_mock_hand_turn off 0 35 25;wait20')
st('HANDTURN')
look('LW0')
a('vr_mock_stick off 0 1;wait20;vr_mock_stick off 0 0;wait20')
look('LW1')
st('HELD')
a('vr_mock_button off primary 0;wait5')
st('KEPT')
a('vr_mock_button main trigger 1;wait3;vr_mock_button main trigger 0;wait20')
st('PLACED')
# 2. The sticks are the player's: the off one walks, the gun's turns, neither touches the ghost.
look('W0')
a('vr_mock_stick off 0 1;wait20;vr_mock_stick off 0 0;wait20')
look('W1')
look('T0')
a('vr_mock_stick main 1 0;wait20;vr_mock_stick main 0 0;wait20')
look('T1')
st('STICKS')
# 3. The gun hand's A/X held: its way turns the ghost.
a('vr_mock_button main primary 1;wait3')
st('TURN0')
a('vr_mock_hand main 0.0 1.5 -0.3 40 90 0;wait20')
st('YAW90')
a('vr_mock_hand main 0.0 1.5 -0.3 75 90 0;wait20')
st('PITCH')
a('vr_mock_hand main 0.0 1.5 -0.3 75 90 40;wait20')
st('TWIST')
look('LS0')
a('vr_mock_stick main 1 0;wait20;vr_mock_stick main 0 0;wait20')
look('LS1')
a('vr_mock_button main stickclick 1;wait2;vr_mock_button main stickclick 0;wait5')
st('PREVTOOL')
a('vr_mock_button main primary 0;wait5')
# 4. Reset; and A/X back to the game and to the sticks.
a('vr_toolgun_tool 0;wait5')
st('BACK')
a('vr_toolgun_reset;wait5')
st('RESET')
a('vr_toolgun_select clear;wait5')
a('vr_mock_button off primary 1;wait3;echo NOSEL;vr_mock_button off primary 0;wait5')
a('vr_toolgun_tool 2;vr_mock_button main primary 1;wait3')
st('ADJUST')
a('vr_mock_button main primary 0;wait5')
# 5. The gun in the off hand: the same with the hands swapped (a fresh map: impulse 189 gives it to the off hand only
# when the main hand does not already hold it).
a('map vrfiringrange;wait60;god;notarget;vr_crate_health 0;vr_toolgun_tool 0;vr_toolgun_joint 0')
a('impulse 189;vr_weapon_grip_mode 1;wait10')
a('vr_mock_hand off 0.0 1.5 -0.3 40 0 0;wait20')
a('vr_toolgun_select 0;wait5')
st('OFF0')
a('vr_mock_button main primary 1;vr_mock_hand_glide 1.0;vr_mock_hand_to main by -20 10 15;wait80')
st('OFFMOVED')
a('vr_mock_button main primary 0;wait5')
a('vr_mock_button off primary 1;vr_mock_hand off 0.0 1.5 -0.3 40 90 0;wait20')
st('OFFYAW')
a('vr_mock_button off primary 0;wait5')
a('wait5;toggleconsole;quit')
print(';'.join(s))
EOF
)
out=$(bash $KIT/run.sh $AGENT -Script "$S" -Timeout 600 -Filter 'toolgun|Player pos|VR buttons|^S0|^LW[01]|^W[01]|^T[01]|STICKS|^HOLD0|^MOVED|^HANDTURN|^HELD|^KEPT|^PLACED|^TURN0|^YAW90|^PITCH|^TWIST|^LS[01]|PREVTOOL|^BACK|^RESET|^NOSEL|^ADJUST|^OFF0|^OFFMOVED|^OFFYAW|ENGINE|CRASH|exit=|TIMEOUT' 2>&1)
[ -n "$VERBOSE" ] && echo "$out" | tail -140
echo "$out" | python -c "
import sys, re, math
t = sys.stdin.read()
fails = []
tags = ['S0', 'HOLD0', 'MOVED', 'HANDTURN', 'HELD', 'KEPT', 'PLACED', 'STICKS', 'TURN0', 'YAW90', 'PITCH', 'TWIST',
        'PREVTOOL', 'BACK', 'RESET', 'ADJUST', 'OFF0', 'OFFMOVED', 'OFFYAW']
nxt = dict(zip(tags, tags[1:] + ['ZZZ']))
def block(tag):
    m = re.search(r'(?m)^' + tag + r'\b(.*?)(?=(?:^' + nxt[tag] + r'\b)|\Z)', t, re.S)
    return m.group(1) if m else None
def offs(b):
    m = re.search(r'ghost offsets ([-\d.]+) ([-\d.]+) ([-\d.]+), turn ([-\d.]+) ([-\d.]+) ([-\d.]+), held (move:\w+) (turn:\w+)', b)
    return [float(v) for v in m.groups()[:6]] + list(m.groups()[6:]) if m else None
def aim(b):
    m = re.search(r'aim from [-\d. ]+ dir ([-\d.]+) ([-\d.]+) ([-\d.]+)', b)
    return [float(v) for v in m.groups()] if m else None
def ghost(b):
    m = re.search(r'ghost (shown|none) at ([-\d.]+) ([-\d.]+) ([-\d.]+) yaw ([-\d.]+)', b)
    return (m.group(1), [float(v) for v in m.groups()[1:]]) if m else None
def pos(tag):
    m = re.search(r'(?m)^' + tag + r'\b.*?Player pos: \(([-\d]+) ([-\d]+) ([-\d]+)\) [-\d]+ ([-\d]+)', t, re.S)
    return [float(v) for v in m.groups()] if m else None
def tool(b):
    m = re.search(r'tool (\w+), panel \w+, adjust (\d)', b)
    return (m.group(1), int(m.group(2))) if m else None
def d(a, b):
    return (a - b + 180) % 360 - 180
def azimuth(v):
    return math.degrees(math.atan2(v[1], v[0]))
def level(v):
    f = (v[0], v[1], 0.0)
    n = math.hypot(f[0], f[1])
    f = (f[0] / n, f[1] / n) if n > 1e-3 else (1.0, 0.0)
    return f, (f[1], -f[0])
def same(a, b, tol=0.001):
    return bool(a) and bool(b) and max(abs(x - y) for x, y in zip(a[:6], b[:6])) <= tol
MOVE = (-20.0, 10.0, 15.0)
b = {k: block(k) for k in tags}
if any(v is None for v in b.values()):
    fails.append('missing steps ' + ','.join(k for k in tags if b[k] is None))
else:
    # The A/X presses, in order: who took each one.
    presses = re.findall(r'VR buttons: (main|off) hand\'s lower face button \([AX]\) pressed([^\n]*)', t)
    got = [(h, 'taken by the toolgun' in rest) for h, rest in presses]
    want = [('off', True), ('main', True), ('off', False), ('main', True), ('main', True), ('off', True)]
    print('A/X presses:', got)
    if got != want: fails.append(f'who takes A/X {got}')
    # 1. The off hand's A/X moves the ghost with it.
    o0 = offs(b['S0'])
    if not o0 or max(abs(v) for v in o0[:6]) > 0.001: fails.append('the ghost starts with offsets or a turn')
    h = offs(b['HOLD0'])
    if not h or h[6] != 'move:off' or h[7] != 'turn:none': fails.append(f'the hold is not the off hand moving {h}')
    g0, g1 = ghost(b['S0']), ghost(b['HOLD0'])
    if not g0 or not g1 or g0[0] != 'shown' or g1[0] != 'shown' or max(abs(x - y) for x, y in zip(g0[1][:3], g1[1][:3])) > 2:
        fails.append(f'the ghost moved when A/X was pressed {g0} {g1}')
    a0, o1 = aim(b['HOLD0']), offs(b['MOVED'])
    fwd, right = level(a0)
    want_off = (MOVE[0] * fwd[0] + MOVE[1] * right[0], MOVE[0] * fwd[1] + MOVE[1] * right[1], MOVE[2])
    print('moved by', MOVE, 'aim', a0, 'forward', fwd, 'right', right, 'want offsets', [round(v, 1) for v in want_off], 'got', o1)
    if not o1 or max(abs(x - y) for x, y in zip(o1[:3], want_off)) > 1.0:
        fails.append(f'the ghost did not follow the hand {want_off} {o1}')
    if any(v != 0.0 for v in o1[3:6]): fails.append('the hand moved the ghost turn too')
    o2 = offs(b['HANDTURN'])
    print('that hand turned:', o2)
    if not same(o2, o1): fails.append('turning the holding hand moved or turned the ghost')
    lw0, lw1 = pos('LW0'), pos('LW1')
    if not lw0 or not lw1 or abs(lw1[0] - lw0[0]) + abs(lw1[1] - lw0[1]) < 10:
        fails.append('the off stick did not walk while its A/X was held')
    o3 = offs(b['HELD'])
    if not o3 or max(abs(x - y) for x, y in zip(o3[:3], o1[:3])) > 1.0: fails.append('walking moved the ghost offsets')
    o4 = offs(b['KEPT'])
    if not o4 or max(abs(x - y) for x, y in zip(o4[:3], o1[:3])) > 1.0: fails.append('letting go did not keep the offsets')
    if o4[6] != 'move:none': fails.append('the hold did not end with the button')
    g2 = ghost(b['KEPT'])
    m = re.search(r'toolgun: spawned \d+ \S+ at ([-\d.]+) ([-\d.]+) ([-\d.]+)', b['KEPT'])
    print('placed', m.groups() if m else None, 'where the ghost stood', g2[1][:3] if g2 else None)
    if not m or not g2 or g2[0] != 'shown' or max(abs(x - y) for x, y in zip([float(v) for v in m.groups()], g2[1][:3])) > 8:
        fails.append('the trigger did not place it where the held ghost stood')
    # 2. The sticks walk and turn the player, and leave the ghost alone.
    w0, w1, t0, t1 = pos('W0'), pos('W1'), pos('T0'), pos('T1')
    print('walk', w0, w1, 'turn', t0, t1)
    if None in (w0, w1, t0, t1): fails.append('viewpos missing')
    else:
        if abs(w1[0] - w0[0]) + abs(w1[1] - w0[1]) < 10: fails.append('the off stick did not walk with the spawn tool in hand')
        if abs(d(t1[3], t0[3])) < 5: fails.append('the gun stick did not turn the player with the spawn tool in hand')
    if not same(offs(b['STICKS']), o4): fails.append('a stick moved the ghost')
    # 3. The gun hand's A/X turns the ghost with its way, and nothing else.
    u = offs(b['TURN0'])
    if not u or u[7] != 'turn:main' or u[6] != 'move:none': fails.append(f'the hold is not the gun hand turning {u}')
    for name, p, c in (('yaw 90', 'TURN0', 'YAW90'), ('pitch', 'YAW90', 'PITCH'), ('twist', 'PITCH', 'TWIST')):
        pa, ca, po, co = aim(b[p]), aim(b[c]), offs(b[p]), offs(b[c])
        dz = d(azimuth(ca), azimuth(pa))
        print(name, ': aim swung', round(dz, 1), 'turn', po[4], '->', co[4], 'offsets', po[:3], '->', co[:3])
        if not co or max(abs(x - y) for x, y in zip(co[:3], po[:3])) > 0.001:
            fails.append(f'the hand s {name} moved the ghost offsets')
        if abs(d(co[4] - po[4], dz)) > 1.0:
            fails.append(f'the ghost turn did not follow the hand way ({name}): {round(dz, 1)} vs {co[4] - po[4]}')
        if co[3] != po[3] or co[5] != po[5]: fails.append(f'the hand s {name} pitched or rolled the ghost')
    if abs(d(offs(b['YAW90'])[4] - offs(b['TURN0'])[4], 90.0)) > 2.0: fails.append('90 degrees of hand yaw did not turn the ghost')
    if abs(d(offs(b['PITCH'])[4], offs(b['YAW90'])[4])) > 5.0: fails.append('the hand pitch turned the ghost')
    ls0, ls1 = pos('LS0'), pos('LS1')
    if not ls0 or not ls1 or abs(d(ls1[3], ls0[3])) < 5: fails.append('the gun stick did not turn the player while its A/X was held')
    if tool(b['PREVTOOL'])[0] != 'Brush': fails.append('the stick press with A/X held: ' + str(tool(b['PREVTOOL'])))
    # 4. Reset, and A/X back to the game and the sticks.
    if tool(b['BACK'])[0] != 'Spawn': fails.append('not back with the spawn tool')
    bo = offs(b['BACK'])
    print('after the tool change: offsets', offs(b['TWIST'])[:3], '->', bo[:3], 'turn', offs(b['TWIST'])[4], '->', bo[4])
    if not bo or max(abs(x - y) for x, y in zip(bo[:3], offs(b['TWIST'])[:3])) > 0.001:
        fails.append('the offsets were lost by changing the tool')
    r = offs(b['RESET'])
    print('reset:', r)
    if not r or any(v != 0.0 for v in r[:6]): fails.append('Reset did not clear the offsets and the turn')
    ad = tool(b['ADJUST'])
    print('physgun, A/X held:', ad)
    if not ad or ad[0] != 'Physgun' or ad[1] != 1: fails.append('with another tool in hand A/X is not the sticks as before')
    # 5. The gun in the off hand.
    if not offs(b['OFF0']) or offs(b['OFF0'])[6:8] != ['move:none', 'turn:none']: fails.append('the off-hand gun starts held')
    om, oy = offs(b['OFFMOVED']), offs(b['OFFYAW'])
    print('off-hand gun: moved', om, 'turned', oy)
    if not om or om[6] != 'move:main': fails.append('the other hand does not move the ghost with the gun in the off hand')
    fw, rt = level(aim(b['OFF0']))
    want_off = (MOVE[0] * fw[0] + MOVE[1] * rt[0], MOVE[0] * fw[1] + MOVE[1] * rt[1], MOVE[2])
    if max(abs(x - y) for x, y in zip(om[:3], want_off)) > 1.0:
        fails.append(f'the off-hand gun: the ghost did not follow the hand {want_off} {om}')
    if not oy or oy[7] != 'turn:off': fails.append('the gun hand does not turn the ghost with the gun in the off hand')
    dz = d(azimuth(aim(b['OFFYAW'])), azimuth(aim(b['OFFMOVED'])))
    print('off-hand gun: aim swung', round(dz, 1), 'turn', om[4], '->', oy[4])
    if abs(d(oy[4] - om[4], dz)) > 1.0 or max(abs(x - y) for x, y in zip(oy[:3], om[:3])) > 0.001:
        fails.append('the off-hand gun: its hand turns the ghost wrong')
if re.search(r'ENGINE CRASH|ENGINE ERROR|TIMEOUT|exit=[1-9]', t): fails.append('crash')
print('FAIL: ' + '; '.join(fails) if fails else 'PASS')
"
