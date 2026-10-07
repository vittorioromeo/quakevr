#!/bin/bash
# melee_phase_test.sh <agent> [out dir] -- Swing Through Enemies (vr_melee_phase; ROUND21.md, "Melee phasing"). Mock
# punches (vr_mock_play, a closed fist) in vrfiringrange, vr_debug_model_collide 1 and developer 1 logging:
# (vr_fixed_frames 1 at 90 Hz; vr_melee_push 0: the grunt stays where the punches go; vr_melee_dmg_multiplier 0.25: nothing dies;
# vr_melee_phase 0 until a case sets it: on by default since config 102, the setup's moves would phase.)
#   dummy_fast_off  a fast punch (5.3 m/s) through the training dummy, phasing off: drawn stopped at it, one hit
#   dummy_fast_on   the same, phasing on: drawn through it (no push while phasing), the same one hit
#   dummy_slow_on   a slow push (0.5 m/s) into it, phasing on: still drawn stopped at it, no hit, no phasing
#   grunt_fast_off  a fast punch into a grunt, ending inside it and held there, phasing off: one hit
#   grunt_fast_on   the same, phasing on: one hit; after the phasing the hand is eased back out (no snap: ~0.1 s)
#   grunt_gun_hold0 the shotgun swung fast into the grunt with vr_melee_phase_hold 0: not phased (a gun)
#   grunt_gun_hold1 the same with vr_melee_phase_hold 1: phased
AGENT=$1; OUT=${2:-C:/OHWorkspace/qvr-agents/$AGENT/scratch/melee_phase}
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd -W 2>/dev/null || pwd)
# Tracking space: metres, +x right, +y up, -z forward. The dummy is 0.95 m ahead of the first setpos.
printf '0.000 main -0.10 1.15 -0.20 0 0 0\n0.300 main -0.10 1.15 -0.20 0 0 0\n0.450 main -0.10 1.15 -1.00 0 0 0\n1.200 main -0.10 1.15 -1.00 0 0 0\n2.000 main -0.10 1.15 -0.20 0 0 0\n' > "$OUT/dummy_fast.txt"
printf '0.000 main -0.10 1.15 -0.20 0 0 0\n0.300 main -0.10 1.15 -0.20 0 0 0\n1.700 main -0.10 1.15 -0.90 0 0 0\n2.200 main -0.10 1.15 -0.90 0 0 0\n3.000 main -0.10 1.15 -0.20 0 0 0\n' > "$OUT/dummy_slow.txt"
printf '0.000 main 0.00 1.15 -0.20 0 0 0\n0.300 main 0.00 1.15 -0.20 0 0 0\n0.450 main 0.00 1.15 -1.05 0 0 0\n1.500 main 0.00 1.15 -1.05 0 0 0\n2.300 main 0.00 1.15 -0.20 0 0 0\n' > "$OUT/grunt_fast.txt"
P="vr_mock_play $OUT"
CASE() { echo "echo PHASE_CASE $1;$2;$P/$3.txt;wait${4:-260};echo PHASE_END $1"; }
S="vr_fixed_frames 1;vr_fixed_frames_rate 90;map vrfiringrange;wait60;god;notarget;developer 1;vr_debug_model_collide 1;vr_melee_dmg_multiplier 0.25;vr_melee_push 0;vr_dummy_attacks 0;vr_melee_phase 0"
S="$S;impulse 150;wait5;setpos 221.2 -656.7 41 0 180 0;wait30;vr_mock_hand main -0.10 1.15 -0.20 0 0 0;+grabmain;vr_mock_button main grip 1;wait20"
S="$S;$(CASE dummy_fast_off 'vr_melee_phase 0' dummy_fast)"
S="$S;$(CASE dummy_fast_on 'vr_melee_phase 1' dummy_fast)"
S="$S;$(CASE dummy_slow_on 'vr_melee_phase 1' dummy_slow 330)"
S="$S;setpos 316 -556 56 0 180 0;wait20;vr_test_spawn 0;vr_test_spawn_dist 36;impulse 241;wait60;vr_mock_hand main 0.00 1.15 -0.20 0 0 0;wait20"
S="$S;$(CASE grunt_fast_off 'vr_melee_phase 0' grunt_fast)"
S="$S;$(CASE grunt_fast_on 'vr_melee_phase 1' grunt_fast)"
S="$S;-grabmain;vr_mock_button main grip 0;vr_weapon_grip_mode 1;impulse 154;wait20"
S="$S;$(CASE grunt_gun_hold0 'vr_melee_phase 1;vr_melee_phase_hold 0' grunt_fast)"
S="$S;$(CASE grunt_gun_hold1 'vr_melee_phase_hold 1' grunt_fast)"
S="$S;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "PHASE_|melee event: (punch|slash|stab|blunt|pommel|butt|swing)|^model collide main|melee phase main|rror|ENGINE" > "$OUT/log.txt" 2>&1
python - "$OUT/log.txt" <<'PY'
import re, sys
cases, cur = {}, None
lines = [] # (the console wraps long lines: joined again)
for line in open(sys.argv[1], errors="replace"):
    line = line.rstrip()
    if re.match(r"(PHASE_|melee |model collide|exit=|ENGINE|Dummy)", line) or not lines: lines.append(line)
    else: lines[-1] += line
for line in lines:
    m = re.match(r"PHASE_(CASE|END) (\S+)", line)
    if m:
        cur = m.group(2) if m.group(1) == "CASE" else None
        if cur: cases[cur] = {"hits": 0, "lines": []}
        continue
    if cur is None: continue
    c = cases[cur]
    if line.startswith("melee event:"): c["hits"] += 1
    m = re.match(r"model collide main: t ([\d.]+) push .* given ([\d.]+) drawn ([\d.]+) .*planes(.*)", line)
    if m: c["lines"].append((float(m.group(1)), float(m.group(2)), float(m.group(3)), m.group(4).strip()))
    m = re.match(r"melee phase main: (on|off) at t ([\d.]+)", line)
    if m: c.setdefault("phase", []).append((m.group(1), float(m.group(2))))
ok = n = 0
def check(name, cond, info):
    global ok, n
    n += 1; ok += bool(cond)
    print(f"{'PASS' if cond else 'FAIL'} {name}: {info}")
def summary(k):
    c = cases.get(k, {"hits": -1, "lines": []})
    L = c["lines"]
    maxd = max([d for _, _, d, _ in L], default=0.0)
    maxdp = max([d for _, _, d, f in L if "phasing" in f], default=0.0)
    return c, L, maxd, maxdp
c, L, maxd, _ = summary("dummy_fast_off")
check("dummy_fast_off", c["hits"] == 1 and maxd > 0.5 and "phase" not in c, f"{c['hits']} hit, drawn held out up to {maxd:.2f} units")
# After the phasing, the hand left inside: eased out (each "released" frame closes at most a quarter of the gap to where
# it is pushed: easeIn would close 60% at 90 Hz), until it is out where the push puts it (drawn within 10% of it).
def released(c):
    L = c["lines"]
    off_t = next((t for s, t in (c.get("phase") or []) if s == "off"), None)
    rel = [x for x in L if off_t is not None and x[0] >= off_t]
    worst, frames, out = 0.0, 0, None
    for a, b in zip(rel, rel[1:]):
        if "released" in b[3] and b[1] > a[2] + 0.05:
            frames += 1
            worst = max(worst, (b[2] - a[2]) / (b[1] - a[2]))
    out = next((x[0] - off_t for x in rel if x[1] > 0.5 and x[2] >= 0.9 * x[1]), None)
    return frames, worst, out
c, L, maxd, maxdp = summary("dummy_fast_on")
fr, worst, out = released(c)
check("dummy_fast_on", c["hits"] == 1 and maxdp < 0.05 and c.get("phase") and fr >= 3 and worst <= 0.25 and out is not None,
      f"{c['hits']} hit, phase {c.get('phase')}, drawn while phasing at most {maxdp:.2f} units; then eased out over {fr} frames "
      f"(each at most {worst:.0%} of the gap), out {out if out is None else round(out, 3)} s after")
c, L, maxd, _ = summary("dummy_slow_on")
check("dummy_slow_on", c["hits"] == 0 and maxd > 0.5 and "phase" not in c, f"{c['hits']} hits, drawn held out up to {maxd:.2f} units, phase {c.get('phase')}")
c, L, maxd, _ = summary("grunt_fast_off")
check("grunt_fast_off", c["hits"] == 1 and maxd > 0.5, f"{c['hits']} hit, drawn held out up to {maxd:.2f} units")
c, L, maxd, maxdp = summary("grunt_fast_on")
fr, worst, out = released(c)
check("grunt_fast_on", c["hits"] == 1 and maxdp < 0.05 and c.get("phase") and worst <= 0.25 and out is not None,
      f"{c['hits']} hit, drawn while phasing at most {maxdp:.2f} units; then eased out over {fr} frames "
      f"(each at most {worst:.0%} of the gap), out {out if out is None else round(out, 3)} s after")
c, L, maxd, maxdp = summary("grunt_gun_hold0")
check("grunt_gun_hold0", "phase" not in c and maxd > 0.5, f"{c['hits']} hits, phase {c.get('phase')}, drawn held out up to {maxd:.2f} units")
c, L, maxd, maxdp = summary("grunt_gun_hold1")
check("grunt_gun_hold1", c.get("phase") and maxdp < 0.05, f"{c['hits']} hits, phase {c.get('phase')}, drawn while phasing at most {maxdp:.2f} units")
print(f"melee phase test: {ok} of {n}")
PY
