#!/usr/bin/env python
# ragperf_scene.py (ROUND25.md, "ragperf") <n> <close|far> <fists|axe> <label> [extra cmds] -- ragperf: n grunts standing in a fan at arm's length
# (close) or 300 units out (far), god + notarget; the mock hands jab (fists) or chop (the axe) across the fan for
# `frames` frames: the blows shove, stagger, knock down and kill (active ragdolls). Prints the profile CSV's key paths,
# the physics phases, the active-ragdoll frames and the ragperf counters into rp_out_<label>.txt.
import os
import subprocess
import sys
from pathlib import Path

AGENT = os.environ.get("RP_AGENT", "dagger")
HERE = Path(os.environ.get("RP_OUT", f"C:/OHWorkspace/qvr-agents/{AGENT}/scratch"))
n = int(sys.argv[1])
where = sys.argv[2]
weapon = sys.argv[3]
label = sys.argv[4]
extra = sys.argv[5] if len(sys.argv) > 5 else ""
frames = int(os.environ.get("RP_FRAMES", "720"))
flat = os.environ.get("RP_FLAT", "") == "1"


def motion(seconds):
    # each hand in turn (0.6 s a blow, the off hand half a beat later), sweeping left, middle, right across the fan
    lines = [] if os.environ.get("RP_OPEN") == "1" else ["0.000 grip off 1"] + ([] if weapon == "axe" else ["0.000 grip main 1"])
    t = 0.0
    k = 0
    while t < seconds:
        sweep = (-0.45, 0.0, 0.45)[k % 3]
        for hand, x, dt in (("main", 0.15, 0.0), ("off", -0.15, 0.3)):
            if weapon == "axe" and hand == "main":
                lines += [f"{t + dt:.3f} {hand} {x + sweep:.2f} 1.75 -0.20 -70 0 0",
                          f"{t + dt + 0.15:.3f} {hand} {x + sweep:.2f} 0.95 -0.70 40 0 0",
                          f"{t + dt + 0.3:.3f} {hand} {x + sweep:.2f} 1.75 -0.20 -70 0 0"]
            else:
                lines += [f"{t + dt:.3f} {hand} {x + sweep:.2f} 1.25 -0.25 0 0 0",
                          f"{t + dt + 0.07:.3f} {hand} {x + sweep:.2f} {(1.30 if k % 2 == 0 else 0.45):.2f} -0.80 0 0 0",
                          f"{t + dt + 0.3:.3f} {hand} {x + sweep:.2f} 1.25 -0.25 0 0 0"]
        t += 0.6
        k += 1
    return "\n".join(lines) + "\n"


mpath = HERE / f"rp_motion_{label}.txt"
mpath.write_text(motion(frames / 72 + 4))
dist = 40 if where == "close" else 300
s = ["sv_random_seed 7", "map vrfiringrange", "wait30", "god", "notarget", "developer 1"]
for i in range(n):
    left = (i - (n - 1) / 2) * (24 if where == "close" else 40)
    s += [f"vr_physics_spawn monster_army {dist} {left:g}"]
s += ["wait40"] + (["impulse 152", "wait20"] if weapon == "axe" else [])
if extra:
    s += [extra]
# The grunts' edicts (seed 7, spawned in this order on vrfiringrange: the same for every n, a prefix of these)
ents = [267, 276, 279, 286, 293, 298][:n]
act = os.environ.get("RP_ACT", "stagger")
period = int(os.environ.get("RP_PERIOD", "120"))
timeline = []  # (frame from the warm-up's start, command); the window starts at frame 60
total = 60 + frames
for f0 in range(0, total, period):
    for i, e in enumerate(ents):
        timeline.append((f0 + 4 * i, f"vr_ragdoll_act {act} {e}"))
if flat:  # flat-screen: the punch button every 0.6 s, the hands alternating
    for k, f0 in enumerate(range(0, total, 43)):
        timeline.append((f0, f"vr_flat_punch {'main' if k % 2 == 0 else 'off'}"))
timeline.append((60, "vr_ragdoll_active_list;vr_profile 1;vr_physics_steptime;vr_physics_frametime;profile 0;vr_hitmodel_stats"))
timeline.append((total, "profile 12;vr_hitmodel_stats;vr_ragdoll_active_list;vr_physics_frametime m;vr_physics_steptime bins"))
timeline.sort(key=lambda x: x[0])
s += [] if flat else [f'vr_mock_play "{mpath.as_posix()}"']
now = 0
for f, c in timeline:
    if f > now:
        s += [f"wait{f - now}"]
        now = f
    s += [c]
s += ["vr_profile_dump", "vr_knockdown_test 5", "wait2", "echo rp_done", "toggleconsole", "quit"]
cmd = ["C:/Program Files/Git/bin/bash.exe", "C:/OHWorkspace/qvr-kit/run.sh", AGENT, "-Script", ";".join(s), "-QuitOn",
       "rp_done", "-Timeout", "400", "-Filter", "^ *[0-9]+ [A-Za-z_]|broad tests|a model test|refit to|ragdoll_act|ragperf|vr_physics_|vr_ragdoll_active_list|knockdown|profile|rror|CRASH|low-impact"]
if flat:
    cmd[3:3] = ["-ExtraArgs", "-flatscreenmode"]
out = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace").stdout
csv = ""
for line in out.splitlines():
    if "-> " in line and "profile_" in line:
        csv = line.split("-> ")[1].strip()
summary = []
if csv:
    want = ("frame", "frame/server", "frame/server/SV_Physics/quakec", "frame/server/SV_Physics/box3d",
            "frame/server/SV_Physics/box3d/box3d sync", "frame/server/SV_Physics/box3d/box3d sync/box3d hands",
            "frame/server/SV_Physics/box3d/box3d water and hits", "frame/server/SV_Physics/box3d/box3d step",
            "frame/server/SV_Physics/box3d/box3d write", "frame/server/SV_Physics/vr hand touches")
    rows = {}
    for line in Path(csv).read_text(errors="replace").splitlines():
        f = line.split(",")
        if len(f) > 11 and f[0].isdigit():  # (every interval, weighted by its frames)
            fr = float(f[2])
            c, a, t = rows.get(f[8], (0.0, 0.0, 0.0))
            rows[f[8]] = (c + float(f[9]) * fr, a + float(f[10]) * fr, t)
        if len(f) > 11 and f[0].isdigit() and f[8] == "frame":
            totals = rows.get("_frames", (0.0, 0.0, 0.0))
            rows["_frames"] = (totals[0] + float(f[2]), 0.0, 0.0)
    nfr = rows.get("_frames", (1.0, 0, 0))[0] or 1.0
    for w in want:
        if w in rows:
            summary.append(f"prof {w}: calls {rows[w][0] / nfr:.2f} avg {rows[w][1] / nfr:.3f} ms")
    summary.append(f"csv {csv}")
(HERE / f"rp_out_{label}.txt").write_text(out + "\n" + "\n".join(summary) + "\n")
keep = [l for l in out.splitlines() if any(k in l for k in ("counts:", "profile total", "broad tests", "a model test", "steptime profile", "steptime: 7", "ragperf", "active_list", "m box3d:", "m solver:", "m step:",
                                                             "m server physics", "frames in", "low-impact", "rror"))]
print("\n".join(keep + summary))
