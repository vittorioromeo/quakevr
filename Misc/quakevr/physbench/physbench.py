#!/usr/bin/env python3
"""Box3D integration benchmark: matched physics scenes on vrfiringrange, timed by phase (vr_physics_frametime).

Each scene is a script (quakevr/physbench_<scene>.cfg) run by its own game, with a fixed game clock (vr_fixed_frames
1: every run the same frames and the same physics, whatever the machine's pace) and, on the mock headset, without
drawing (vr_mock_fast 2): only the CPU's work is timed. vr_fixed_frames_rate 120 runs the host at 120 Hz, so the
server (host_netinterval, 1/72 s at most) runs every second frame, at 60 Hz: as in the headset at 120 Hz.

  python physbench.py gen <gamedir>                       # writes the scenes' scripts
  python physbench.py run --exe A=<path> --exe B=<path> --basedir <quake> [--reps 5] [--scenes a,b] > out.txt
  python physbench.py parse < out.txt                     # each exe's medians over the repeats, by scene and window

`run` interleaves the executables (A B A B ...: drift and turbo hit both alike), each with -noconfigwrite (a run's
+cvars would otherwise be saved into ironwail.cfg for the next ones; the saved config is still read: move it aside to
bench the defaults), and needs a display for the game's window (on Linux, Xvfb: DISPLAY=:5). Each window prints, per phase (vr_box3d.cpp framePhaseNames), the mean, median, 95th
and 99th percentiles and the worst of its server frames, and the host frames' wall-clock time; the scene ends with
vr_physics_hash piles (the state of the props the scene spawned, bit for bit: a change meant not to alter the physics leaves
it as it was; the map's own props are left out: their start depends on the wall clock).
"""
import argparse
import os
import re
import statistics
import subprocess
import sys
import time

# The first player's start on vrfiringrange (info_player_start: 316 -556 56, facing west); vr_physics_bigpile piles
# ahead of him: a pile of 500 is centred about 220 units west.
PILE_CENTRE = (130, -556, 40)
# Water: vrcalibration's pool (x 224 to 544, y -416 to -192; its surface at z -10, its floor at -130). (vrfiringrange's
# water has no floor in the level's collision where it is deep enough: props sinking there fall out of the world.)
POOL_ROCKS = (240, -350, -122)
POOL_CRATES = (240, -240, -100)


def waits(n):
    out = []
    for big, name in ((60, "w60"), (10, "w10"), (1, "wait")):
        while n >= big:
            out.append(name)
            n -= big
    return ";".join(out)


def window(label, frames):
    return [waits(frames), f"vr_physics_frametime {label}"]


def pile(kind, count, settle=600, measure=300):
    """A pile toppling (the fall: 3 s), settling, then lying still (`measure` frames)."""
    return [f"vr_physics_bigpile {kind} {count}", "wait;wait;vr_physics_frametime start", *window("fall", 180),
            waits(settle), "vr_physics_frametime -", *window("settled", measure)]


def blasts(count=500, every=60, times=6):
    """A settled pile blown up again and again (a rocket's blast at its middle): bursts of collisions."""
    lines = [f"vr_physics_bigpile rocks {count}", waits(780), "vr_physics_frametime -"]
    for _ in range(times):
        lines += [f"vr_physics_blast {PILE_CENTRE[0]} {PILE_CENTRE[1]} {PILE_CENTRE[2]} 200", waits(every)]
    lines.append("vr_physics_frametime blasts")
    return lines


def water(rocks=120, crates=24):
    """vrcalibration's pool: rocks lying on its floor, crates let go under its surface, rising and floating."""
    rx, ry, rz = POOL_ROCKS
    cx, cy, cz = POOL_CRATES
    return [f"vr_physics_bigpile rocks {rocks}", f"vr_physics_bigpile crates {crates}", "wait;wait",
            f"vr_physics_pile vr_rock 6 {rx} {ry} {rz} 16", f"vr_physics_pile vr_crate 3 {cx} {cy} {cz} 34",
            "wait;wait;vr_physics_frametime start", waits(900), "vr_physics_frametime -", *window("settled", 300)]


def ragdolls(count=24, ragdoll_max=8):
    """Grunts killed in a row (vr_ragdoll_max of them ragdolls, the rest corpses), lying dead."""
    lines = ["god", "notarget", f"vr_ragdoll_max {ragdoll_max}"]
    for i in range(count):
        lines.append(f"vr_physics_spawn monster_army {120 + (i // 6) * 48} {(i % 6 - 2.5) * 48}")
    lines += ["wait;wait;wait"]
    # Killed by blasts along the rows (not gibbed: 60 at the middle).
    for row in range(count // 6 + 1):
        d = 120 + row * 48
        for side in (-96, 0, 96):
            # (Facing west: ahead is -x; left is -y.)
            lines.append(f"vr_physics_blast {316 - d} {-556 + side} 40 60")
    lines += ["wait;wait;vr_physics_frametime start", *window("dying", 300), waits(600), "vr_physics_frametime -",
              *window("dead", 300)]
    return lines


MAPS = {"water": "vrcalibration"}  # (the others: vrfiringrange)

SCENES = {
    "rocks500": lambda: pile("rocks", 500),
    "mixed500": lambda: pile("mixed", 500),
    "debris1000": lambda: pile("debris", 1000, settle=900),
    "blasts": blasts,
    "water": water,
    "ragdolls": ragdolls,
    "ragdolls32": lambda: ragdolls(32, 32),
}


def gen(a):
    for name, scene in SCENES.items():
        lines = ['alias w10 "wait;wait;wait;wait;wait;wait;wait;wait;wait;wait"', 'alias w60 "w10;w10;w10;w10;w10;w10"',
                 "developer 0", waits(60), "vr_physics_frametime start", *scene(), "vr_physics_frametime end",
                 "vr_physics_hash piles", f"echo PHYSBENCH {name} done", "toggleconsole", "quit"]
        with open(os.path.join(a.gamedir, f"physbench_{name}.cfg"), "w", newline="\n") as f:
            f.write("\n".join(lines) + "\n")
    print(f"{len(SCENES)} scenes: {', '.join(SCENES)}")


def run(a):
    exes = [e.split("=", 1) for e in a.exe]
    scenes = a.scenes.split(",") if a.scenes else list(SCENES)
    for r in range(a.reps):
        for scene in scenes:
            for tag, exe in exes:
                cmd = [exe, "-basedir", a.basedir, "-game", "quakevr", "-vrmock", "-noconfigwrite", "-window", "-width", "320", "-height",
                       "240", "-nosound", "+vr_fixed_frames", "1", "+vr_fixed_frames_rate", str(a.rate), "+vr_mock_fast",
                       "2", *a.extra, "+map", MAPS.get(scene, "vrfiringrange"), "+exec", f"physbench_{scene}.cfg"]
                t0 = time.time()
                p = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=a.timeout)
                print(f"PB run {tag} {scene} {r} {time.time() - t0:.2f}s exit={p.returncode}", flush=True)
                for line in p.stdout.splitlines():
                    if line.startswith(("vr_physics_frametime", "vr_physics_hash", "PHYSBENCH", "Host_Error")):
                        print(f"PB {tag} {scene} {r} {line}", flush=True)


HEAD = re.compile(r"PB (\S+) (\S+) (\d+) vr_physics_frametime (\S+): (\d+) frames, (\d+) host frames in ([\d.]+) s "
                  r"\(([\d.]+) ms a host frame\), ([\d.]+) awake, (\d+) bodies, (\d+) contacts")
PHASE = re.compile(r"PB (\S+) (\S+) (\d+) vr_physics_frametime (\S+) (.+): mean ([\d.]+) median ([\d.]+) p95 ([\d.]+) "
                   r"p99 ([\d.]+) worst ([\d.]+)")
HASH = re.compile(r"PB (\S+) (\S+) (\d+) vr_physics_hash: (\d+) bodies, (\w+)")


def parse(a):
    heads, phases, hashes = {}, {}, {}
    for line in sys.stdin:
        line = line.rstrip()
        if m := HEAD.match(line):
            tag, scene, r, label = m.group(1, 2, 3, 4)
            if label in ("start", "-", "end"):
                continue
            heads.setdefault((scene, label, tag), []).append((float(m.group(8)), float(m.group(9)), int(m.group(11))))
        elif m := PHASE.match(line):
            tag, scene, r, label, phase = m.group(1, 2, 3, 4, 5)
            if label in ("start", "-", "end"):
                continue
            vals = tuple(float(x) for x in m.group(6, 7, 8, 9, 10))
            phases.setdefault((scene, label, tag, phase), []).append(vals)
        elif m := HASH.match(line):
            hashes.setdefault((m.group(2), m.group(1)), []).append(m.group(5))
    tags = sorted({k[2] for k in heads}, key=lambda t: [k[2] for k in heads].index(t))
    med = statistics.median
    for scene, label in sorted({(k[0], k[1]) for k in heads}, key=lambda x: list(SCENES).index(x[0]) if x[0] in SCENES else 99):
        print(f"\n== {scene} / {label}")
        for tag in tags:
            h = heads.get((scene, label, tag))
            if not h:
                continue
            print(f"  {tag:>8}: host frame {med([x[0] for x in h]):.4f} ms, {med([x[1] for x in h]):.1f} awake, "
                  f"{med([x[2] for x in h])} contacts ({len(h)} runs)")
        print(f"  {'phase':>16} " + " ".join(f"{t + ' mean':>12} {t + ' p95':>11} {t + ' p99':>11}" for t in tags))
        for phase in ("server physics", "box3d", "sync", "before", "step", "solver", "write", "after"):
            row = []
            for tag in tags:
                v = phases.get((scene, label, tag, phase))
                if not v:
                    row.append(f"{'-':>12} {'-':>11} {'-':>11}")
                    continue
                row.append(f"{med([x[0] for x in v]):12.4f} {med([x[2] for x in v]):11.4f} {med([x[3] for x in v]):11.4f}")
            print(f"  {phase:>16} " + " ".join(row))
    print("\n== hashes (vr_physics_hash at each scene's end)")
    for scene in sorted({k[0] for k in hashes}):
        print(f"  {scene}: " + "  ".join(f"{tag} {','.join(sorted(set(hashes.get((scene, tag), []))))}" for tag in tags))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    g = sub.add_parser("gen")
    g.add_argument("gamedir")
    r = sub.add_parser("run")
    r.add_argument("--exe", action="append", required=True, help="TAG=path (several: interleaved)")
    r.add_argument("--basedir", required=True)
    r.add_argument("--reps", type=int, default=5)
    r.add_argument("--scenes", default="")
    r.add_argument("--rate", type=int, default=120)
    r.add_argument("--timeout", type=int, default=600)
    r.add_argument("extra", nargs="*", help="more +cvar value pairs for every run (after --)")
    sub.add_parser("parse")
    a = ap.parse_args()
    {"gen": gen, "run": run, "parse": parse}[a.cmd](a)


if __name__ == "__main__":
    main()
