#!/usr/bin/env python3
"""The headless test suite before a release (docs/vr-port/RELEASING.md, "Tests before publishing"): the Misc/quakevr
test scripts (.sh, .py, .ps1) that give a verdict, through the kit (run.sh with the mock headset) on a kit worktree, in
parallel lanes (--jobs; each lane its own game folder and lock, kit run.sh QVR_LANE).

    python Misc/release/run_test_suite.py <agent> [--build] [--profile full|beta] [--jobs N] [--only REGEX]
                                                   [--skip REGEX] [--log-dir DIR] [--allow-flaky] [--flaky NAME,...]
                                                   [--list]

--profile: "full" (default: every test, final releases) or "beta" (the short smoke set of PROFILES, the betas).
--jobs: tests at once, 0 (default) one lane per 8 CPUs, at most 4 (2 while the author's game runs); 1: one at a time,
as before 2026-11-22. A test that must see the agent's own game folder runs in lane 0; one with timing runs or that
changes a file every game of the worktree reads (qvrtest.lane_keys: "alone") runs first, by itself; at most
--batch-jobs testbatch suites (each plays its cases in up to 8 copies of the game itself) at once. The results are the
same as one at a time: the summary lists the tests in the suite's order, each log is its own.

<agent> is a kit worktree (C:/OHWorkspace/qvr-agents/<agent>, made by the kit's new_agent.sh) holding the commit under
test; --build builds it first (kit build.sh). A test fails on a non-zero exit, a FAIL / "<name>: FAIL" / "FAILURES n" /
"n failed" line in its output (qvrtest.FAIL_LINE), or its timeout. Each test's whole output goes to <log-dir>/<name>.log
(default: the agent's scratch/test_suite/<time>), the summary to <log-dir>/summary.txt. Exit 0 when nothing failed (with
--allow-flaky, the known-flaky tests of Misc/quakevr/test_flaky.txt and --flaky ones only warn), 1 when something did,
2 for a usage problem.
"""

import argparse
import datetime
import os
import re
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "quakevr"))
import qvrtest  # noqa: E402  (the tests' library: run, the verdict parser, the flaky list)

KIT = qvrtest.KIT

# name, script (under Misc/quakevr), extra arguments after the agent, timeout (s), known flaky (also: test_flaky.txt).
# The scripts that give a verdict (exit code or FAIL lines) and run headless on one worktree; the ones that print
# numbers or pictures for a person, need another agent's folder, the expansions' data, two instances or a benchmark
# base are not here.
SUITE = [
    ("autopump", "autopump_test.sh", [], 1200, False),
    ("cellcord", "cellcord_test.sh", [], 1200, False),
    ("changelevel_signon", "changelevel_signon_test.sh", [], 600, False),
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
    ("reload_ssgtorch", "reload/ssgtorch_test.sh", [], 1200, False),
    ("reload", "reload/reload_test.sh", [], 3600, False),
    ("mapgun_pickup", "loadedmapguns/pickup_test.ps1", [], 1800, False),
    ("mapgun_transfer", "loadedmapguns/transfer_test.ps1", [], 1200, False),
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
    ("selfchecks", "selfchecks_test.sh", [], 1200, False),
    ("settings_page", "settings_page_test.py", [], 1200, False),  # Setup's settings page: the manifest not stale, the cfg run once
    ("animsmooth", "animsmooth_test.sh", [], 1200, False),
    ("teleporters_chase", "teleporters/teleporters_test.sh", ["chase"], 2400, False),  # (flaky: test_flaky.txt)
    ("stealth", "stealth_tests.sh", [], 5400, False),
    ("scenario_check", "scenario_check.sh", [], 1200, False),  # every checklist Test scene started (AD's folders)
]

# The profiles (--profile): "full" every test above (final releases); "beta" a short smoke set for the betas
# (make_release.ps1 -Beta; about 10 min beside the build): the checklist scenes, the engine's self-checks, a level
# change and its stats, the settings page, the update notice, and a handful of core gameplay tests (a reload, a pickup,
# a throw, a mapper's gun, a grip, the climb), each under ~2 min alone.
PROFILES = {
    "full": None,
    "beta": ["selfchecks", "scenario_check", "changelevel_signon", "stats_levelchange", "settings_page",
             "update_notice", "pickup_mag", "reload_eject", "throw_spin", "mapgun_pickup", "grip_state", "slopes"],
}

FAIL_LINE = qvrtest.FAIL_LINE  # (run_baseline.py's parted cases read it here)
find_bash = qvrtest.find_bash
tree_of = qvrtest.tree


def auto_jobs():
    """--jobs 0: a lane per 8 CPUs, at most 4 (the kit's 16 game slots are shared with the other agents, and a testbatch
    suite takes up to 8 of them itself); 2 while the author's game runs (run.ps1's low-impact mode then)."""
    n = max(1, min(4, (os.cpu_count() or 8) // 8))
    r = subprocess.run(["powershell", "-NoProfile", "-Command",
                        "@(Get-Process ironwail -ErrorAction SilentlyContinue | Where-Object { $_.Path -match "
                        "'quakevr-iw\\\\Windows' }).Count"], capture_output=True, text=True)
    return min(n, 2) if r.stdout.strip() not in ("", "0") else n


def run(argv, log_path, timeout, env=None):
    """argv's output into log_path, its exit line last; (exit code or None on timeout, output, seconds)."""
    code, text, secs = qvrtest.run(argv, log_path, timeout, env)
    with open(log_path, "a", encoding="utf-8", errors="replace") as log:
        log.write(f"\n[exit {code if code is not None else 'TIMEOUT'} after {secs:.0f} s]\n")
    return code, text, secs


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
    ap.add_argument("--profile", default="full", choices=sorted(PROFILES), help="full (default) or beta: the smoke set")
    ap.add_argument("--jobs", type=int, default=0, help="tests at once, each in its own lane (0: by the CPU count)")
    ap.add_argument("--batch-jobs", type=int, default=2, help="testbatch suites at once (each plays up to 8 games)")
    a = ap.parse_args()

    chosen = PROFILES[a.profile]
    tests = [t for t in SUITE if (chosen is None or t[0] in chosen) and (not a.only or re.search(a.only, t[0]))
             and not (a.skip and re.search(a.skip, t[0]))]
    extra_flaky = {n.strip() for n in a.flaky.split(",") if n.strip()}
    unknown = extra_flaky - {t[0] for t in SUITE}
    if unknown:
        print(f"--flaky: no such test: {', '.join(sorted(unknown))}")
        return 2
    flaky = {t[0] for t in SUITE if t[4] or t[0] in qvrtest.FLAKY} | extra_flaky
    if a.list:
        root = os.path.join(qvrtest.TREE, "Misc", "quakevr").replace("\\", "/")
        for name, script, args, timeout, _ in tests:
            keys = ", ".join(sorted(qvrtest.lane_keys(script, root)))
            print(f"{name:20} Misc/quakevr/{script}{' ' + ' '.join(args) if args else ''}  (timeout {timeout // 60} min"
                  f"{', known flaky' if name in flaky else ''}{', ' + keys if keys else ''})")
        print(f"{len(tests)} tests (profile {a.profile})")
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

    jobs = max(1, min(a.jobs or auto_jobs(), len(tests) or 1))
    root = f"{tree}/Misc/quakevr"
    keyed = [(t, qvrtest.lane_keys(t[1], root)) for t in tests]
    if jobs > 1:
        bad = qvrtest.prepare_lanes(a.agent, jobs, log_dir)
        if bad:
            print(f"lanes {bad} have no id1 folder after their set-up run ({log_dir}/_lane<k>_setup.log): "
                  f"lanes 0-{min(bad) - 1} only", flush=True)
            jobs = min(bad)
    print(f"{jobs} lane(s) at once (profile {a.profile})", flush=True)

    results = {}
    lock = threading.Lock()
    total_start = time.time()

    def one(test, lane):
        name, script, args, timeout, _ = test
        path = f"{root}/{script}"
        argv = qvrtest.script_argv(path)
        if argv[0] == qvrtest.BASH:
            argv[0] = bash
        argv += [a.agent] + args
        log_path = os.path.join(log_dir, f"{name}.log")
        if not os.path.isfile(path):
            verdict, why, secs = "FAIL", "no such script", 0.0
        else:
            code, text, secs = run(argv, log_path, timeout, dict(qvrtest.lane_env(lane), KIT=KIT))
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
        with lock:
            results[name] = (name, verdict, why, secs, log_path)
            print(f"[{len(results):2}/{len(tests)}] {name:20} {verdict:5} {secs:5.0f} s  (lane {lane})"
                  f"{'  ' + why if why else ''}", flush=True)
            if verdict != "PASS" and os.path.isfile(log_path):
                with open(log_path, encoding="utf-8", errors="replace") as f:
                    bad = [l.rstrip() for l in f if FAIL_LINE.search(l)]
                for l in (bad or ["(no FAIL line: see the log)"])[:8]:
                    print(f"        {l[:160]}")
                print(f"        log: {log_path}", flush=True)

    # First the "alone" ones, one after another (nothing else runs); then the lanes, each taking the longest test it
    # may (by its timeout): lane 0 the lane-0 ones first; at most --batch-jobs testbatch suites and one "tele" at once.
    for t, keys in keyed:
        if "alone" in keys:
            one(t, 0)
    queue = sorted([(t, k) for t, k in keyed if "alone" not in k], key=lambda tk: -tk[0][3])
    limits = {"batch": max(1, a.batch_jobs), "tele": 1}
    running = {}
    cond = threading.Condition()

    def pick(lane):
        for want_lane0 in ((True, False) if lane == 0 else (False,)):
            for i, (t, keys) in enumerate(queue):
                if ("lane0" in keys) == want_lane0 and all(running.get(k, 0) < limits.get(k, jobs) for k in keys):
                    return queue.pop(i)
        return None

    def worker(lane):
        while True:
            with cond:
                got = None
                while queue:
                    got = pick(lane)
                    if got:
                        break
                    cond.wait(5)
                if not got:
                    return
                for k in got[1]:
                    running[k] = running.get(k, 0) + 1
            try:
                one(got[0], lane)
            except Exception as e:  # (a harness problem: that test fails, the others go on)
                with lock:
                    results[got[0][0]] = (got[0][0], "FAIL", f"harness: {e!r}", 0.0, "")
            with cond:
                for k in got[1]:
                    running[k] -= 1
                cond.notify_all()

    threads = [threading.Thread(target=worker, args=(k,)) for k in range(jobs)]
    for th in threads:
        th.start()
    for th in threads:
        th.join()

    results = [results[t[0]] for t in tests if t[0] in results]  # (the suite's order)
    passed = sum(1 for r in results if r[1] == "PASS")
    failed = [r for r in results if r[1] == "FAIL"]
    flaky_failed = [r for r in results if r[1] == "FLAKY"]
    lines = [f"test suite on {tree} ({head}): {passed} passed, {len(failed)} failed, {len(flaky_failed)} flaky "
             f"(warned), {len(results)} run in {(time.time() - total_start) / 60:.1f} min ({jobs} lanes, profile "
             f"{a.profile})"]
    lines += [f"  FAIL  {n}: {w} ({l})" for n, _, w, _, l in failed]
    lines += [f"  FLAKY {n}: {w} ({l})" for n, _, w, _, l in flaky_failed]
    with open(os.path.join(log_dir, "summary.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
        f.writelines(f"{n:20} {v:5} {s:5.0f} s {w}\n" for n, v, w, s, _ in results)
    print("\n".join(lines))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
