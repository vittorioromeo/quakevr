"""Capture Quake-only VTune software hotspots with Release symbols.

Uses perf_suite's disposable base and fixtures. Sampling perturbs timing;
use the original unprofiled runs for throughput comparisons.
"""
import argparse
import csv
import hashlib
import json
import os
import re
from pathlib import Path
import subprocess
import time

from perf_suite import script


def export(vtune, output, env):
    result = output / "vtune"
    reports = [("summary", "summary", []),
               ("hotspots", "hotspots", ["-group-by", "function", "-limit", "0"]),
               ("top-down", "top-down", ["-limit", "0"]),
               ("threads", "hotspots", ["-group-by", "thread,function", "-limit", "0"])]
    for name, report, extra in reports:
        with (output / f"{name}.csv").open("w") as file:
            subprocess.run([str(vtune), "-report", report, "-r", str(result),
                            "-format", "csv", "-csv-delimiter", ",", *extra],
                           env=env, stdout=file, check=True, timeout=180)
    with (output / "summary.csv").open() as file:
        elapsed = next(float(row[2]) for row in csv.reader(file)
                       if len(row) > 2 and row[1] == "Elapsed Time")
    # A late window separates async model baking/load work from steady-state.
    window = f"{max(4, elapsed - 12):.3f}:{elapsed - 1:.3f}"
    for report, extra in (("hotspots", ["-group-by", "thread,function"]), ("top-down", [])):
        with (output / f"late-{report}.csv").open("w") as file:
            subprocess.run([str(vtune), "-report", report, "-r", str(result),
                            "-format", "csv", "-csv-delimiter", ",", "-limit", "0",
                            "-time-filter", window, *extra],
                           env=env, stdout=file, check=True, timeout=180)
    (output / "late-window.txt").write_text(window + "\n")


def run(args):
    base, exe, output, vtune = (Path(p).resolve() for p in
                               (args.base, args.exe, args.output, args.vtune))
    if "build-cmake" not in base.parts:
        raise ValueError("A disposable base under build-cmake is required")
    output.mkdir(parents=True, exist_ok=False)
    config = script(args.scene, args.frames, args.eye, 0, False)
    if args.cvar:
        config = config.replace('\nvr_profile 1\n', '\n' + '\n'.join(args.cvar) + '\nvr_profile 1\n', 1)
    (base / "quakevr/autoexec.cfg").write_text(config)
    (output / "autoexec.cfg").write_text(config)
    env = dict(os.environ, QVR_TEST_HIDDEN="1", QVR_TEST_BACKGROUND="1",
               QVR_NO_ERROR_DIALOG="1")
    result = output / "vtune"
    command = [str(vtune), "-collect", "hotspots", "-knob", "sampling-mode=sw",
               "-knob", "enable-stack-collection=true", "-knob",
               "enable-characterization-insights=false", "-result-dir", str(result),
               "-search-dir", str(exe.parent), "-source-search-dir", str(Path.cwd()),
               "-app-working-dir", str(base), "-resume-after", "4",
               "-return-app-exitcode", "--", str(exe), "-basedir", str(base),
               "-game", "hipnotic", "-game", "rogue", "-game", "quakevr",
               "-condebug", "-window", "-width", "960", "-height", "540",
               "-nosound", "-noconfigwrite", "-noaddons"]
    started = time.monotonic()
    with (output / "collection.log").open("w") as log:
        subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT,
                       check=True, timeout=600)
    console = (base / "qconsole.log").read_bytes()
    (output / "qconsole.log").write_bytes(console)
    if b"BENCH_DONE" not in console or re.search(rb"(?m)^(?:Host_Error|Sys_Error):", console):
        raise RuntimeError("Game fixture failed; see qconsole.log")
    export(vtune, output, env)
    metadata = dict(scene=args.scene, frames=args.frames, eye=args.eye, cvars=args.cvar,
                    sampling="VTune software hotspots, stacks, 10 ms, delay 4 s",
                    wall_seconds=time.monotonic() - started, command=command,
                    exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
                    progs_sha256=hashlib.sha256((base / "quakevr/progs.dat").read_bytes()).hexdigest())
    (output / "metadata.json").write_text(json.dumps(metadata, indent=2))
    print(f"Captured {args.scene} in {metadata['wall_seconds']:.1f}s: {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vtune", default="C:/Program Files (x86)/Intel/oneAPI/vtune/2024.3/bin64/vtune.exe")
    parser.add_argument("--base", required=True)
    parser.add_argument("--exe", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--scene", choices=("props_active", "props_active_1000", "props_settled", "decals_stream", "decals_blood_4096_stream", "enemies"), required=True)
    parser.add_argument("--frames", type=int, default=18000)
    parser.add_argument("--eye", type=int, default=2048)
    parser.add_argument("--cvar", action="append", default=[])
    args = parser.parse_args()
    if any(not re.fullmatch(r"vr_[a-zA-Z0-9_]+ [-+0-9.eE]+", value) for value in args.cvar):
        parser.error("--cvar requires a VR cvar name and numeric value")
    run(args)
