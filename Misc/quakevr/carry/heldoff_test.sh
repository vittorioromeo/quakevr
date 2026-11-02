#!/bin/bash
# heldoff_test.sh <agent> -- a hand drawn off its tracked place by what it holds strikes nothing with its ghost (his note,
# 2026-11-01: "the actual drawn position of the hand should be the dominant one"; QC VR_Hand_HeldOff, .vr_hand_heldoff,
# engine progs::handHeldOff; ROUND23.md, "Held-off hands"). vrmelee-recording, the training dummy 0.95 m ahead, the main
# hand's fist closed (grip), a mock punch through the dummy at 5.3 m/s (melee_phase_test.sh's dummy_fast), developer 1:
#   free     the free main hand punches: a blow ("melee event: punch")
#   ragdoll  the main hand holds a dead grunt's limb (taken beside you, then turned to the dummy), the same punch from
#            the tracked hand: held off (handslot: held off 2), no blow
# Ends on "heldoff: N passed, M failed" (exit 1 on a failure).
AGENT=${1:?worktree name}; OUT=C:/OHWorkspace/qvr-agents/$AGENT/scratch/heldoff
KIT=${KIT:-C:/OHWorkspace/qvr-kit}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd -W 2>/dev/null || pwd)
printf '0.000 main -0.10 1.15 -0.20 0 0 0\n0.300 main -0.10 1.15 -0.20 0 0 0\n0.450 main -0.10 1.15 -1.00 0 0 0\n1.200 main -0.10 1.15 -1.00 0 0 0\n2.000 main -0.10 1.15 -0.20 0 0 0\n' > "$OUT/punch.txt"
P="vr_mock_play $OUT/punch.txt"
SPOT="setpos 3.3 -31.2 24 0 90 0"
TAKE="vr_mock_hand_to main ragdoll near 1;wait2;vr_mock_hand_to main ragdoll near 1;+grabmain;vr_mock_button main grip 1"
S="vr_fixed_frames 1;vr_fixed_frames_rate 90;map vrmelee-recording;wait60;god;notarget;developer 1;vr_ragdoll 1;vr_melee_dmg_multiplier 0.25;vr_melee_push 0;vr_dummy_attacks 0;vr_melee_phase_speed 100"
S="$S;impulse 150;wait5;$SPOT;wait30;vr_mock_hand main -0.10 1.15 -0.20 0 0 0;+grabmain;vr_mock_button main grip 1;wait20"
S="$S;echo HELDOFF_CASE free;$P;wait260;echo HELDOFF_END free"
S="$S;-grabmain;vr_mock_button main grip 0;wait10;setpos 3.3 -31.2 24 0 ${YAW:-0} 0;wait10;vr_test_spawn 0;vr_test_spawn_dist 28;vr_test_spawn_dead 1;impulse 241;wait3;vr_test_spawn_dead 0;wait300"
S="$S;$TAKE;wait10;vr_mock_hand_to main by 0 0 20;wait30;$SPOT;wait30"
S="$S;echo HELDOFF_CASE ragdoll;$P;wait260;echo HELDOFF_END ragdoll;-grabmain;vr_mock_button main grip 0;wait5;toggleconsole;quit"
bash $KIT/run.sh $AGENT -Script "$S" -Filter "HELDOFF_|melee event: (punch|slap|slash|stab|blunt|pommel|butt|swing)|handslot|rror|ENGINE" -QuitOn "HELDOFF_END ragdoll" -Timeout 300 > "$OUT/log.txt" 2>&1
python - "$OUT/log.txt" <<'PY'
import re, sys
cases, cur, ok, n = {}, None, 0, 0
held_main = False
for line in open(sys.argv[1], errors="replace"):
    line = line.rstrip()
    m = re.search(r"handslot: held off (\d+)", line)
    if m:
        held_main = (int(m.group(1)) & 2) != 0
    m = re.search(r"HELDOFF_(CASE|END) (\S+)", line)
    if m:
        cur = m.group(2) if m.group(1) == "CASE" else None
        if cur: cases[cur] = {"blows": 0, "held": held_main}
        continue
    if cur and re.search(r"melee event: ", line):
        cases[cur]["blows"] += 1
    if cur and held_main:
        cases[cur]["held"] = True
def check(name, cond, info):
    global ok, n
    n += 1; ok += bool(cond)
    print(f"{'PASS' if cond else 'FAIL'} {name}: {info}")
f = cases.get("free", {"blows": -1, "held": None})
r = cases.get("ragdoll", {"blows": -1, "held": None})
check("free", f["blows"] >= 1 and not f["held"], f"{f['blows']} blow(s), held off {f['held']}")
check("ragdoll", r["blows"] == 0 and r["held"], f"{r['blows']} blow(s), held off {r['held']}")
print(f"heldoff: {ok} passed, {n - ok} failed")
sys.exit(0 if ok == n else 1)
PY
