#!/usr/bin/env python3
"""The headless test suite before a release (docs/vr-port/RELEASING.md, "Tests before publishing"): the Misc/quakevr
test scripts that give a verdict, one at a time, through the kit (run.sh with the mock headset) on a kit worktree.

    python Misc/release/run_test_suite.py <agent> [--build] [--only REGEX] [--skip REGEX] [--log-dir DIR]
                                                   [--allow-flaky] [--flaky NAME,...] [--list]

<agent> is a kit worktree (C:/OHWorkspace/qvr-agents/<agent>, made by the kit's new_agent.sh) holding the commit under
test; --build builds it first (kit build.sh). A test fails on a non-zero exit, a FAIL / "FAILURES n" / "n failed" line
in its output, or its timeout. Each test's whole output goes to <log-dir>/<name>.log (default: the agent's
scratch/test_suite/<time>), the summary to <log-dir>/summary.txt. Exit 0 when nothing failed (with --allow-flaky, the
known-flaky tests below and --flaky ones only warn), 1 when something did, 2 for a usage problem.
"""

import argparse
import datetime
import os
import re
import shutil
import subprocess
import sys
import time

KIT = os.environ.get("QVR_KIT", "C:/OHWorkspace/qvr-kit")

# name, script (under Misc/quakevr), extra arguments after the agent, timeout (s), known flaky. The scripts that give a
# verdict (exit code or FAIL lines) and run headless on one worktree; the ones that print numbers or pictures for a
# person, need another agent's folder, the expansions' data, two instances or a benchmark base are not here.
SUITE = [
    ("autopump", "autopump_test.sh", [], 1200, False),
    ("cellcord", "cellcord_test.sh", [], 1200, False),
    ("collectfx", "collectfx_test.sh", [], 1200, False),
    ("dead_gear", "dead_gear_test.sh", [], 1200, False),
    ("empty_melee", "empty_melee_test.sh", [], 1200, False),
    ("explosion_debris", "explosion_debris_test.sh", [], 1200, False),
    ("fist_alert", "fist_alert_test.sh", [], 1200, False),
    ("held_over_props", "held_over_props_test.sh", [], 1800, False),
    ("mapflameburn", "mapflameburn_test.sh", [], 1200, False),
    ("melee_phase", "melee_phase_test.sh", [], 1800, False),
    ("pickup_mag", "pickup_mag_test.sh", [], 1200, False),
    ("slider_step", "slider_step_test.sh", [], 1200, False),
    ("spentshake", "spentshake_test.sh", [], 1200, False),
    ("stats_levelchange", "stats_levelchange_test.sh", [], 1200, False),
    ("swim_sound", "swim/swim_sound_test.sh", [], 1200, False),
    ("weapon_catch", "weapon_catch_test.sh", [], 1200, False),
    ("vrtrailer", "vrtrailer_test.sh", [], 1200, False),
    ("slopes", "climb/slopes_test.sh", [], 1200, False),
    ("throw_spin", "throw_spin_test.sh", [], 1800, False),
    ("reload_contact", "reload/contact_test.sh", [], 1200, False),
    ("reload_eject", "reload/eject_test.sh", [], 1200, False),
    ("reload_front", "reload/front_test.sh", [], 1200, False),
    ("reload_gunshape", "reload/gunshape_test.sh", [], 1200, False),
    ("reload_magcollide", "reload/magcollide_test.sh", [], 1200, False),
    ("reload_pouchgren", "reload/pouchgren_test.sh", [], 1200, False),
    ("reload_worldparts", "reload/worldparts_test.sh", [], 1200, False),
    ("reload", "reload/reload_test.sh", [], 3600, False),
    ("xr_runtime", "xr_runtime_test.sh", [], 1800, False),
    ("grip_gap", "carry/grip_gap_test.py", [], 1800, False),
    ("flash_grab", "flashgrab/flash_grab_test.py", [], 3600, False),
    ("parryinterrupt", "parryinterrupt/test.py", [], 1800, False),
    ("secret_hits", "secret_hits_test.py", [], 1200, False),
    ("grip_state", "twohand/grip_state_test.py", [], 1200, False),
    ("update_notice", "update_notice_test.py", [], 1200, False),
    ("turned_box", "grapple/turned_box_test.py", [], 1200, False),
    ("shot_shape", "shot_shape_test.py", [], 1200, False),
    ("parry_pose", "parry_pose_test.sh", [], 3600, False),
    # Quake's AI is random (ROUND21.md, "Test suite pass (2026-10-09)"): a dog or fiend may miss the gate in 3 runs.
    ("teleporters_chase", "teleporters/teleporters_test.sh", ["chase"], 2400, True),
    ("stealth", "stealth_tests.sh", [], 5400, False),
]

FAIL_LINE = re.compile(r"^\s*(FAIL\b|PROBLEM\b)|FAILURES [1-9]|\b[1-9][0-9]* failed\b", re.M)


def find_bash():
    """Git's bash (never Windows' WSL bash.exe in System32)."""
    env = os.environ.get("QVR_BASH")
    if env and os.path.isfile(env):
        return env
    w = shutil.which("bash")
    if w and "system32" not in w.lower():
        return w
    for p in (r"C:\Program Files\Git\bin\bash.exe", r"C:\Program Files\Git\usr\bin\bash.exe"):
        if os.path.isfile(p):
            return p
    git = shutil.which("git")
    if git:
        p = os.path.join(os.path.dirname(os.path.dirname(git)), "bin", "bash.exe")
        if os.path.isfile(p):
            return p
    return None


def tree_of(agent):
    return "C:/OHWorkspace/quakevr-iw-cleanup" if agent == "cleanup" else f"C:/OHWorkspace/qvr-agents/{agent}"


def kill_tree(proc):
    subprocess.run(["taskkill", "/T", "/F", "/PID", str(proc.pid)], capture_output=True)


def run(argv, log_path, timeout, env=None):
    """argv's output into log_path; (exit code or None on timeout, output, seconds)."""
    start = time.time()
    with open(log_path, "w", encoding="utf-8", errors="replace") as log:
        log.write("$ " + " ".join(argv) + "\n")
        log.flush()
        proc = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env,
                                creationflags=getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0))
        try:
            out, _ = proc.communicate(timeout=timeout)
            code = proc.returncode
        except subprocess.TimeoutExpired:
            kill_tree(proc)
            out, _ = proc.communicate()
            code = None
        text = out.decode("utf-8", errors="replace").replace("\r\n", "\n")
        log.write(text)
        log.write(f"\n[exit {code if code is not None else 'TIMEOUT'} after {time.time() - start:.0f} s]\n")
    return code, text, time.time() - start


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("agent", nargs="?", default="")
    ap.add_argument("--build", action="store_true", help="kit build.sh <agent> first")
    ap.add_argument("--only", default="", help="run the tests whose name matches this regex")
    ap.add_argument("--skip", default="", help="leave out the tests whose name matches this regex")
    ap.add_argument("--log-dir", default="")
    ap.add_argument("--allow-flaky", action="store_true", help="the known-flaky tests (and --flaky ones) only warn")
    ap.add_argument("--flaky", default="", help="more test names (comma-separated) that only warn with --allow-flaky")
    ap.add_argument("--list", action="store_true", help="print the tests that would run, and nothing else")
    a = ap.parse_args()

    tests = [t for t in SUITE if (not a.only or re.search(a.only, t[0])) and not (a.skip and re.search(a.skip, t[0]))]
    extra_flaky = {n.strip() for n in a.flaky.split(",") if n.strip()}
    unknown = extra_flaky - {t[0] for t in SUITE}
    if unknown:
        print(f"--flaky: no such test: {', '.join(sorted(unknown))}")
        return 2
    flaky = {t[0] for t in SUITE if t[4]} | extra_flaky
    if a.list:
        for name, script, args, timeout, _ in tests:
            print(f"{name:20} Misc/quakevr/{script}{' ' + ' '.join(args) if args else ''}  (timeout {timeout // 60} min"
                  f"{', known flaky' if name in flaky else ''})")
        print(f"{len(tests)} tests")
        return 0
    if not a.agent:
        print("usage: run_test_suite.py <agent> [...] (a kit worktree: C:/OHWorkspace/qvr-agents/<agent>)")
        return 2
    tree = tree_of(a.agent)
    if not os.path.isdir(tree):
        print(f"no kit worktree {tree} (the kit's new_agent.sh {a.agent} <commit> makes one)")
        return 2
    bash = find_bash()
    if not bash:
        print("no Git bash found (QVR_BASH=<bash.exe>)")
        return 2
    log_dir = a.log_dir or os.path.join(tree, "scratch", "test_suite", datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))
    os.makedirs(log_dir, exist_ok=True)
    head = subprocess.run(["git", "-C", tree, "log", "-1", "--format=%h %s"], capture_output=True, text=True,
                          encoding="utf-8", errors="replace").stdout.strip()[:100]
    print(f"test suite: {len(tests)} tests on {tree} ({head}); logs {log_dir}", flush=True)

    env = dict(os.environ, KIT=KIT)
    if a.build:
        code, text, secs = run([bash, f"{KIT}/build.sh", a.agent], os.path.join(log_dir, "_build.log"), 3600, env)
        if code != 0 or "BUILD FAILED" in text:
            print(f"build FAILED ({secs:.0f} s; {os.path.join(log_dir, '_build.log')}):")
            print("\n".join(text.strip().splitlines()[-15:]))
            return 1
        print(f"built ({secs:.0f} s)", flush=True)

    results = []
    total_start = time.time()
    for i, (name, script, args, timeout, _) in enumerate(tests, 1):
        path = f"{tree}/Misc/quakevr/{script}"
        argv = ([sys.executable, path] if script.endswith(".py") else [bash, path]) + [a.agent] + args
        print(f"[{i:2}/{len(tests)}] {name:20} ", end="", flush=True)
        log_path = os.path.join(log_dir, f"{name}.log")
        if not os.path.isfile(path):
            verdict, why, secs = "FAIL", "no such script", 0.0
        else:
            code, text, secs = run(argv, log_path, timeout, env)
            fails = FAIL_LINE.findall(text)
            if code is None:
                verdict, why = "FAIL", f"timeout ({timeout} s)"
            elif code != 0:
                verdict, why = "FAIL", f"exit {code}"
            elif fails:
                verdict, why = "FAIL", f"{len(fails)} FAIL line(s)"
            else:
                verdict, why = "PASS", ""
        if verdict == "FAIL" and a.allow_flaky and name in flaky:
            verdict = "FLAKY"
        results.append((name, verdict, why, secs, log_path))
        print(f"{verdict:5} {secs:5.0f} s{'  ' + why if why else ''}", flush=True)
        if verdict != "PASS" and os.path.isfile(log_path):
            with open(log_path, encoding="utf-8", errors="replace") as f:
                bad = [l.rstrip() for l in f if FAIL_LINE.search(l)]
            for l in (bad or ["(no FAIL line: see the log)"])[:8]:
                print(f"        {l[:160]}")
            print(f"        log: {log_path}", flush=True)

    passed = sum(1 for r in results if r[1] == "PASS")
    failed = [r for r in results if r[1] == "FAIL"]
    flaky_failed = [r for r in results if r[1] == "FLAKY"]
    lines = [f"test suite on {tree} ({head}): {passed} passed, {len(failed)} failed, {len(flaky_failed)} flaky "
             f"(warned), {len(results)} run in {(time.time() - total_start) / 60:.1f} min"]
    lines += [f"  FAIL  {n}: {w} ({l})" for n, _, w, _, l in failed]
    lines += [f"  FLAKY {n}: {w} ({l})" for n, _, w, _, l in flaky_failed]
    with open(os.path.join(log_dir, "summary.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
        f.writelines(f"{n:20} {v:5} {s:5.0f} s {w}\n" for n, v, w, s, _ in results)
    print("\n".join(lines))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
