#!/bin/bash
# flick_slowmo_compare.sh <agent> ["<cvars>"] -- flick_slowmo_test.sh three times at once (full speed; bullet time at
# 0.3x made at real speed; bullet time made slowly with the world, stretch 3.333), the cvars before each, and one table:
# metres at 45 degrees and each bullet-time column over full speed. ROUND21.md, "Wrist flicks in bullet time: the arm
# tells the tempo": every flick in bullet time within 1.3 times its full-speed distance, the arm throws as before.
NAME="$1"; X="${2:-}"; ROOT="C:/OHWorkspace/qvr-agents/$NAME"; D="$ROOT/Misc/quakevr/throw_slowmo"; mkdir -p "$ROOT/scratch"
P="$ROOT/scratch/flickcmp_$(echo "$X" | md5sum | cut -c1-6)"
bash "$D/flick_slowmo_test.sh" "$NAME" "$X" > "${P}_full.txt" 2>&1 &
bash "$D/flick_slowmo_test.sh" "$NAME" "$X;vr_bullettime;wait10" > "${P}_bt.txt" 2>&1 &
bash "$D/flick_slowmo_test.sh" "$NAME" "$X;vr_bullettime;wait10" 3.333 > "${P}_slow.txt" 2>&1 &
wait
python - "$P" <<'PY'
import sys, re
def load(f):
    d = {}
    for l in open(f):
        m = re.match(r'(\S+)\s+([\d.]+) m/s\s+at 45 degrees\s+([\d.]+) m(.*)', l)
        if m: d[m.group(1)] = (float(m.group(3)), m.group(4).strip())
    return d
full, bt, slow = (load(f'{sys.argv[1]}_{k}.txt') for k in ('full', 'bt', 'slow'))
print(f"{'throw':11s} {'full':>6s} {'bullet':>6s} {'ratio':>6s} {'slowly':>6s} {'ratio':>6s}  bullet time's release")
for k, (m, _) in full.items():
    b = bt.get(k, (0, '')); s = slow.get(k, (0, ''))
    print(f"{k:11s} {m:6.2f} {b[0]:6.2f} {b[0] / max(m, 1e-3):6.2f} {s[0]:6.2f} {s[0] / max(m, 1e-3):6.2f}  {b[1]}")
PY
