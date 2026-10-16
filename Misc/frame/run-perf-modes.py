#!/usr/bin/env python3
"""GPU/frame-time cost of the Turnip/Zink switches that avoid the dummy-kill crash (run ON the Frame, no headset needed).

Runs the Frame's stereo benchmark scenario (the performance-inputs/run-frame-baseline.py one: mock tracking, the firing range with
48 combatants, 720 fixed frames) once for each variant (an environment for the Quake process), ABBA-ordered over the rounds, and prints the
frame and GPU phase times. The results are the benchmark's own JSON (`quakevr/profile/bench/frame_<scenario>.json`) copied to
~/perf-modes-<name>/<variant>-r<round>.json.

  python3 run-perf-modes.py <name> <rounds> [scenario] [variant,variant,...]
"""
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path.home() / "Games/QuakeVR-Unleashed-native"
GAME = ROOT / "release-kit-check/archive-install-test"
EXE = ROOT / "performance-results/dummy-kill-repro/game" / os.environ.get("PERF_EXE", "ironwail-head2")  # PERF_EXE: another build (default: the HEAD build the crash was reproduced on)
OPENXR = os.environ.get("PERF_BACKEND") == "openxr"  # PERF_BACKEND=openxr: the OpenXR backend instead of the mock one

VARIANTS = {
    "base": {},
    "sysmem": {"TU_DEBUG": "sysmem"},
    "gmem": {"TU_DEBUG": "gmem"},
    "noubwc": {"TU_DEBUG": "noubwc"},
    "atsys": {"TU_AUTOTUNE_ALGO": "prefer_sysmem"},
    "lazy": {"ZINK_DESCRIPTORS": "lazy"},
    # origin/next's permanent decals (vr_permdecals, default 1): their cost, on and off, with and without the sysmem switch
    "pd0": {"_args": ["+vr_permdecals", "0"]},
    "sys_pd0": {"TU_DEBUG": "sysmem", "_args": ["+vr_permdecals", "0"]},
}


def run_once(scenario, env_extra, out):
    env = dict(os.environ, DISPLAY=":1", SDL_VIDEODRIVER="x11", QVR_TEST_HIDDEN="1", QVR_TEST_BACKGROUND="1",
               XDG_RUNTIME_DIR="/run/user/1000", QVR_NO_ERROR_DIALOG="1")
    env_extra = dict(env_extra)
    extra_args = env_extra.pop("_args", [])  # command-line arguments of the variant, not environment
    env.update(env_extra)
    if OPENXR:  # the real OpenXR path (the null headset: no wearer), the foveated-density pass included
        env["XR_RUNTIME_JSON"] = "/opt/steamvr/steamxr_linuxarm64.json"
        env["MESA_VK_ABORT_ON_DEVICE_LOSS"] = "1"
    backend = ["+vr_xr_unfocused", "0", "+vr_xr_unfocused_pause", "0"] if OPENXR else ["-vrmock"]
    cmd = [str(EXE), "-window", "-width", "640", "-height", "480", "-basedir", str(GAME), "-game", "quakevr"] + backend + [
           "-nomapindex", "-nobindless", "-noconfigwrite", "-condebug", "-nosound", "-nosteam"] + extra_args + ["+exec", scenario + ".cfg"]
    data = GAME / "quakevr/profile/bench" / f"frame_{scenario}.json"
    if data.exists():
        data.unlink()
    with out.with_suffix(".log").open("w") as log:
        try:
            run = subprocess.run(cmd, cwd=GAME, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=240)
            rc = run.returncode
        except subprocess.TimeoutExpired:
            subprocess.run(["pkill", "-KILL", "^ironwail"])
            rc = "timeout"
    text = out.with_suffix(".log").read_text(errors="replace")
    if rc != 0 or "BENCH_DONE" not in text or not data.exists():
        return rc, None
    shutil.copy2(data, out.with_suffix(".json"))
    return rc, json.loads(data.read_text())


def main():
    name, rounds = sys.argv[1], int(sys.argv[2])
    scenario = sys.argv[3] if len(sys.argv) > 3 else "combat_48"
    names = sys.argv[4].split(",") if len(sys.argv) > 4 else list(VARIANTS)
    work = Path.home() / f"perf-modes-{name}"
    work.mkdir(exist_ok=True)
    rows = []
    for r in range(1, rounds + 1):
        order = names if r % 2 else list(reversed(names))
        for v in order:
            if subprocess.run(["pgrep", "^ironwail"], capture_output=True).returncode == 0:
                sys.exit("Quake is already running")
            rc, d = run_once(scenario, VARIANTS[v], work / f"{v}-r{r}")
            if d is None:
                print(f"{v}-r{r} FAILED rc={rc}", flush=True)
                continue
            f, g = d["frame"]["frame_ms"], d.get("gpu_phase_ms", {})
            row = {"variant": v, "round": r, "frame_avg": f["avg"], "frame_p95": f["p95"], "frame_p99": f["p99"],
                   "gpu_3D": g.get("3D"), "gpu_world": g.get("world+brush"), "gpu_eyeL": g.get("eye L"), "gpu_eyeR": g.get("eye R"),
                   "secs": d.get("seconds")}
            rows.append(row)
            print(json.dumps(row), flush=True)
            time.sleep(2)
    print("SUMMARY (mean over rounds)")
    for v in names:
        rs = [x for x in rows if x["variant"] == v]
        if not rs:
            continue
        mean = lambda k: sum(x[k] for x in rs if x[k] is not None) / max(1, len([x for x in rs if x[k] is not None]))
        print(f"{v:8s} n={len(rs)} frame_avg={mean('frame_avg'):.2f} p95={mean('frame_p95'):.2f} gpu_3D={mean('gpu_3D'):.2f} "
              f"world={mean('gpu_world'):.2f} eyeL={mean('gpu_eyeL'):.2f} eyeR={mean('gpu_eyeR'):.2f}")
    print("PERF_DONE")


if __name__ == "__main__":
    main()
