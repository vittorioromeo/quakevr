"""shub_telefrag_test.py <agent> [run.sh options] -- end.bsp's telefrag of Shub-Niggurath after a load (ROUND22.md,
"Shub's teleporter after a load").

The teleporter at (-960 2672 268) leads to the misc_teleporttrain (the spiked ball, edict 53) that circles the arena
through Shub (edict 19); stepping in while the ball is inside her telefrags her, the only way to kill her. The script
loads end, sets the ball going (the trigger_once by the start), saves, loads the save, then walks the player into that
teleporter again and again (every 2 seconds, god mode), printing where the ball is and where the player came out each
time, until Shub dies. Flat: add -ExtraArgs "+vr_enabled 0". PARSE_ONLY=1 reads the last run's log again.

Expected: the exits land on the ball (under 200 units from it, 'live'; one may fall off it beside Shub, 'elsewhere'),
none 'at gate', and Shub dies ('SHUB DEAD'). Before the fix the VR gate code (vr_portals.cpp, build) took the ball's
point at the load as the gate's destination and walked the player there; with no room for him there it held him at the
gate's face ('at gate', every attempt), Quake's teleport left out, and Shub never died.
"""
import math, os, re, subprocess, sys

agent = sys.argv[1]
kit = os.environ.get('KIT', 'C:/OHWorkspace/qvr-kit')
w = 'C:/OHWorkspace/qvr-agents/' + agent
TRAIN, SHUB, ATTEMPTS = 53, 19, 30


def wait(n):
    return ['w10'] * (n // 10) + ['wait'] * (n % 10)


out = ['alias w10 "wait;wait;wait;wait;wait;wait;wait;wait;wait;wait"', 'sv_random_seed 7', 'map end'] + wait(30)
# the trigger_once by the start (*4) sets the ball going; save once it has gone a way along its path
out += ['setpos -448 640 72 0 90 0', 'noclip'] + wait(150) + ['echo ATT -1', 'edict %d' % TRAIN]
out += ['save shubtele'] + wait(10) + ['load shubtele'] + wait(200) + ['god', 'notarget', 'vr_portals_info']
for i in range(ATTEMPTS):
    out += ['echo ATT %d' % i, 'edict %d' % TRAIN, 'setpos -1010 2672 256 0 0 0', 'noclip', 'vr_mock_stick off 0 1']
    out += wait(30) + ['vr_mock_stick off 0 0', 'echo EXIT %d' % i, 'edict 1', 'edict %d' % TRAIN, 'edict %d' % SHUB]
    out += wait(110)
out += ['echo DONE', 'toggleconsole', 'quit']
os.makedirs(w + '/scratch', exist_ok=True)
with open(w + '/quakevr/scratch_shubtele.cfg', 'w', newline='\n') as f:
    f.write('\n'.join(out) + '\n')

log = w + '/scratch/shub_telefrag_test.txt'
# Git's bash (a plain 'bash' on Windows can be WSL's)
bash = next((b for b in ['C:/Program Files/Git/bin/bash.exe', 'C:/Program Files/Git/usr/bin/bash.exe'] if os.path.exists(b)), 'bash')
if not os.environ.get('PARSE_ONLY'):  # PARSE_ONLY=1: read the last run's log again
  with open(log, 'w') as f:
    subprocess.run([bash, kit + '/run.sh', agent, '-Timeout', '400'] + sys.argv[2:] +
                   ['-Script', 'exec scratch_shubtele.cfg', '-Filter', r'^(ATT|EXIT|DONE|EDICT|origin|health)|cached dest|ENGINE|TIMEOUT'],
                   stdout=f, stderr=subprocess.STDOUT)

vec = lambda s: [float(x) for x in s.split("'")[1].split()]
state, ent, cur, rows, dead, done = None, None, {}, [], False, False
for line in open(log, errors='replace'):
    line = line.rstrip()
    if 'cached dest' in line or 'ENGINE' in line or 'TIMEOUT' in line:
        print(line.strip())
    m = re.match(r'(ATT|EXIT) (\d+)', line)
    if m:
        state, ent = m.group(1), None
        if state == 'ATT':
            cur = {'n': int(m.group(2))}
            rows.append(cur)
        continue
    if line.startswith('DONE'):
        done = True
    m = re.match(r'EDICT (\d+):', line)
    if m:
        ent = int(m.group(1))
    elif line.startswith('origin') and state:
        key = {TRAIN: 'ball', 1: 'player'}.get(ent)
        if key:
            cur[state + key] = vec(line)
    elif line.startswith('health') and state == 'EXIT' and ent == SHUB:
        cur['shub'] = float(line.split()[1])
live = stale = 0
for r in rows:
    if r['n'] < 0:
        print('ball at the save: (%.0f %.0f %.0f)' % tuple(r.get('ATTball', [0, 0, 0])))
        continue
    if r.get('shub', 1) <= 0:
        # finale_1 removes the ball and moves the player to the intermission spot
        print('attempt %2d: ball at walk-in (%.0f %.0f %.0f), shub %.0f' % (r['n'], *r.get('ATTball', [0, 0, 0]), r['shub']))
        dead = True
        break
    if 'EXITplayer' not in r or 'EXITball' not in r:
        continue
    p, b = r['EXITplayer'], r['EXITball']
    d = math.dist(p[:2], b[:2])
    tag = 'at gate' if math.dist(p[:2], [-968, 2672]) < 40 else 'live' if d < 200 else 'elsewhere'
    live += tag == 'live'
    stale += tag == 'at gate'
    print('attempt %2d: ball at walk-in (%.0f %.0f %.0f), player out at (%.0f %.0f %.0f), ball now (%.0f %.0f %.0f), xy %.0f %s, shub %.0f' % (
        r['n'], *r.get('ATTball', [0, 0, 0]), *p, *b, d, tag, r.get('shub', -1)))
print('SHUB DEAD' if dead else 'SHUB ALIVE', '- live exits %d, held at the gate %d%s' % (live, stale, '' if done else ', script did not finish'))
