#!/usr/bin/env python3
"""Headless repro of the Quake VR dummy-kill GPU crash on the Steam Frame (no headset needed).

Run ON the Frame (copy it over with scp, then `ssh frame python3 run-dummy-kill-repro.py ...`).
It works in a scratch copy of the installed game (so the real config, saves and binary are untouched), starts Quake with
the mock VR backend (or OpenXR with no wearer), loads the firing range and kills the training dummy over and over.
The dump watcher (qvr-devcore-watch.service) should be running: a GPU crash then leaves a devcoredump in ~/devcore-dumps.

Scenarios:
  test3  vr_dummy_test 3: a lethal hit from you (T_Damage, the same kill path as a blow), no weapon thrown
  axe    a synthetic axe thrown at the dummy (impulse 209, vr_test_axe_at 1): the path of the crashing headset runs
  both   alternates the two
  replay plays a recorded Motion Recorder take (--take <file>, docs/vr-port/MOTIONS.md) with `vr_motion_play ... xr`: the take's head, hands
         and buttons drive the game while the real runtime (--backend openxr) renders, paces and composites as it does for a wearer
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path.home() / "Games/QuakeVR-Unleashed-native"
REAL_GAME = ROOT / "game"
WORK = ROOT / "performance-results/dummy-kill-repro"
SCRATCH = WORK / "game"
DUMPS = Path.home() / "devcore-dumps"


# The controllers are live on the Frame even with nobody wearing it: the game's hit and shot buzzes would vibrate them all
# through a run.
NO_HAPTICS = ["vr_disablehaptics 1", "vr_haptics_strength 0"]


def waits(n):
    return ["wait"] * n


def take_rows(path):
    """Host frames in a take: the lines that are neither comments nor the column header."""
    with open(path, errors="replace") as f:
        return max(0, sum(1 for l in f if l.strip() and not l.startswith("#")) - 1)


def find_take(name):
    p = Path(name)
    for c in (p, SCRATCH / "quakevr/motions" / name, REAL_GAME / "quakevr/motions" / name):
        if c.is_file():
            return c
    sys.exit(f"no take {name} (looked in the scratch and installed game's quakevr/motions)")


def build_replay_cfg(args):
    take = find_take(args.take)
    scratch_take = SCRATCH / "quakevr/motions" / take.name
    if take.resolve() != scratch_take.resolve():
        scratch_take.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(take, scratch_take)
    rows = take_rows(take)
    lines = list(NO_HAPTICS)
    for extra in args.cvar:
        lines.append(extra.replace("=", " ", 1))
    lines += [f"map {args.map}"] + waits(args.settle)
    lines += ["god 1", "notarget 1", "developer 1"] + waits(30)
    play = f"vr_motion_play {take.name} " + ("xr " if args.backend == "openxr" else "") + " ".join(args.play_arg)
    for k in range(1, args.repeat + 1):
        lines += [f"echo KILL_BEGIN {k} replay", play]
        lines += waits(rows + args.replay_margin)
        lines.append(f"echo KILL_END {k}")
    lines += ["echo REPLAY_END", "echo KILL_DONE", "quit"]
    return "\n".join(lines) + "\n", rows


def build_cfg(args):
    if args.scenario == "replay":
        return build_replay_cfg(args)[0]
    lines = list(NO_HAPTICS)
    for extra in args.cvar:
        lines.append(extra.replace("=", " ", 1))
    lines += ["map vrfiringrange"] + waits(args.settle)
    lines += ["god 1", "notarget 1", "developer 1", "setpos 221.2 -656.7 41 0 180 0"] + waits(30)
    for k in range(1, args.kills + 1):
        scen = args.scenario if args.scenario != "both" else ("test3" if k % 2 else "axe")
        lines.append(f"echo KILL_BEGIN {k} {scen}")
        if scen == "test3":
            lines += ["vr_dummy_test 3"]
        else:
            lines += ["vr_test_axe 0", "vr_test_axe_loft 1", "vr_test_axe_damage 1", "vr_test_axe_at 1",
                      "vr_test_axe_dist 70", "vr_test_axe_speed 10", "impulse 209"]
        lines += waits(args.after)
        lines.append(f"echo KILL_END {k}")
    lines += ["echo KILL_DONE", "quit"]
    return "\n".join(lines) + "\n"


def dump_names():
    return set(p.name for p in DUMPS.glob("*.bin")) if DUMPS.exists() else set()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tag", required=True)
    ap.add_argument("--backend", choices=("mock", "openxr"), default="mock")
    ap.add_argument("--scenario", choices=("test3", "axe", "both", "replay"), default="test3")
    ap.add_argument("--take", help="replay: a Motion Recorder take (name in quakevr/motions, or a path)")
    ap.add_argument("--map", default="vrfiringrange", help="replay: the map the take was recorded in")
    ap.add_argument("--play-arg", action="append", default=[], help="replay: extra vr_motion_play option (noplace, yaw 90, target vr_dummy ...)")
    ap.add_argument("--repeat", type=int, default=1, help="replay: plays of the take, one after the other in the same map (one kill each for a kill take)")
    ap.add_argument("--replay-margin", type=int, default=300, help="replay: frames to wait beyond the take's rows (setup, wind-down)")
    ap.add_argument("--kills", type=int, default=6)
    ap.add_argument("--settle", type=int, default=120, help="frames to wait after the map loads")
    ap.add_argument("--after", type=int, default=240, help="frames to wait after each kill")
    ap.add_argument("--timeout", type=int, default=240)
    ap.add_argument("--env", action="append", default=[], help="KEY=VALUE for the Quake process (e.g. ZINK_DESCRIPTORS=lazy)")
    ap.add_argument("--cvar", action="append", default=[], help="NAME=VALUE console variable set before the map")
    ap.add_argument("--arg", action="append", default=[], help="extra Quake command-line argument")
    ap.add_argument("--exe", default="ironwail", help="engine binary inside the scratch game dir (e.g. ironwail-test)")
    args = ap.parse_args()
    if args.scenario == "replay" and not args.take:
        sys.exit("--scenario replay needs --take")

    if subprocess.run(["pgrep", "^ironwail"], capture_output=True).returncode == 0:
        sys.exit("Quake is already running; refusing a second session")
    WORK.mkdir(parents=True, exist_ok=True)
    if not SCRATCH.exists():
        print("creating the scratch game copy once ...", flush=True)
        shutil.copytree(REAL_GAME, SCRATCH, symlinks=True)
    out = WORK / args.tag
    if out.exists():
        sys.exit(f"{out} exists; pick a new --tag")
    out.mkdir()
    cfg_name = f"killrepro-{args.tag}.cfg"
    (SCRATCH / "quakevr" / cfg_name).write_text(build_cfg(args), encoding="ascii")

    env = os.environ.copy()
    env.update(DISPLAY=":1", SDL_VIDEODRIVER="x11", XDG_RUNTIME_DIR="/run/user/1000", QVR_TEST_HIDDEN="1",
               QVR_TEST_BACKGROUND="1", QVR_NO_ERROR_DIALOG="1", MESA_VK_ABORT_ON_DEVICE_LOSS="1")
    if args.backend == "openxr":
        env["XR_RUNTIME_JSON"] = "/opt/steamvr/steamxr_linuxarm64.json"
    for kv in args.env:
        k, v = kv.split("=", 1)
        env[k] = v
    cmd = [str(SCRATCH / args.exe), "-window", "-basedir", str(SCRATCH), "-game", "quakevr", "-nobindless",
           "-noconfigwrite", "-nomapindex", "-nosound", "-nosteam", "-condebug"]
    if args.backend == "mock":
        cmd.append("-vrmock")
    cmd += args.arg + ["+exec", cfg_name]

    before = dump_names()
    start_wall = time.strftime("%Y-%m-%d %H:%M:%S")
    t0 = time.time()
    log = out / "launcher.log"
    with log.open("w") as f:
        try:
            proc = subprocess.run(cmd, cwd=SCRATCH, env=env, stdout=f, stderr=subprocess.STDOUT, timeout=args.timeout)
            rc = proc.returncode
        except subprocess.TimeoutExpired:
            rc = "timeout"
            subprocess.run(["pkill", "-INT", "^ironwail"])
            time.sleep(3)
            subprocess.run(["pkill", "-KILL", "^ironwail"])
    secs = time.time() - t0
    time.sleep(3)  # the watcher polls once a second
    console = SCRATCH / "qconsole.log"
    text = console.read_text(errors="replace") if console.exists() else ""
    if console.exists():
        shutil.copy2(console, out / "qconsole.log")
    kernel = subprocess.run(["journalctl", "-k", "--no-pager", "--since", start_wall], capture_output=True,
                            text=True).stdout
    (out / "kernel.txt").write_text(kernel)
    new_dumps = sorted(dump_names() - before)
    for name in new_dumps:
        shutil.copy2(DUMPS / name, out / name)
    kills_begun = text.count("KILL_BEGIN")
    result = {
        "tag": args.tag, "backend": args.backend, "scenario": args.scenario, "env": args.env, "cvar": args.cvar,
        "exit": rc, "seconds": round(secs, 1), "kills_begun": kills_begun, "kills_ended": text.count("KILL_END"),
        "done": "KILL_DONE" in text,
        "replay_finished": "frames against" in text, "replay_stopped": "vr_motion_play: stopped" in text,
        "replay_errors": [l for l in text.splitlines() if l.startswith("vr_motion_play:") and "frames against" not in l][:6], "dummy_killed_lines": text.count("killed as its enemy") + text.count("- killed"),
        "gpu_fault_lines": sum(1 for l in kernel.splitlines() if "gpu fault" in l.lower() or "fault" in l.lower() and "msm" in l.lower()),
        "sp_fault_lines": sum(1 for l in kernel.splitlines() if "gpu fault" in l.lower() and "source=br_sp" in l.lower()),
        "hangcheck_lines": sum(1 for l in kernel.splitlines() if "hangcheck" in l.lower()),
        "new_dumps": new_dumps,
    }
    (out / "result.json").write_text(json.dumps(result, indent=1))
    print(json.dumps(result))


if __name__ == "__main__":
    main()
