#!/usr/bin/env python3
"""qvrtest.py -- the tests' one library (docs/vr-port/TESTING.md, "Writing a test"): the kit, running a game or a
script, the one verdict grammar, its parser, the known-flaky list and the mock-file builders.

A test script (Misc/quakevr/<dir>/x_test.py):

    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))  # (from a subfolder)
    import qvrtest
    t = qvrtest.Tally("judo")                 # its lines' prefix
    out = qvrtest.run_sh(AGENT, ["-Script", "map vrtesthall;...;toggleconsole;quit", "-Filter", "judo|exit="])
    t.check(ok, "what was checked")           # `judo: PASS what was checked` (or FAIL)
    return t.verdict()                        # `judo: N passed, M failed`; 1 when one failed (the exit status)

The verdict grammar (every test, QC, C++, shell and Python; qvr_checks canonical's test-* families keep new code to it):
  `<name>: PASS <what>` / `<name>: FAIL <what>`  one line per check
  `<name>: N passed, M failed`                    the test's last line: the run ends on it (run.sh -QuitOn)
QC: VR_Test_Begin/Check/Verdict (QC/vr_testutil.qc); C++: testkit::Tally (vr_testkit.hpp); shell: qvrtest.sh.

The runners (Misc/release/run_test_suite.py, the kit's refactor run_baseline.py) read results with the parser below
and take the known-flaky tests from FLAKY (Misc/quakevr/test_flaky.txt).
"""
import os
import re
import shutil
import subprocess
import sys
import time

# ---- where things are

KIT = (os.environ.get("QVR_KIT") or os.environ.get("KIT") or "C:/OHWorkspace/qvr-kit").replace("\\", "/")
HERE = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")  # Misc/quakevr
TREE = os.path.dirname(os.path.dirname(HERE))  # the worktree this file is in


def tree(agent):
    """A kit worktree's folder (the author's cleanup checkout for `cleanup`)."""
    return "C:/OHWorkspace/quakevr-iw-cleanup" if agent == "cleanup" else "C:/OHWorkspace/qvr-agents/%s" % agent


def find_bash():
    """Git's bash (never Windows' WSL bash.exe in System32); QVR_BASH overrides."""
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


BASH = find_bash() or r"C:\Program Files\Git\bin\bash.exe"


def lane():
    """The lane this process runs in (run.sh QVR_LANE: its own game folder and lock), 0 for the agent's own."""
    v = os.environ.get("QVR_LANE", "")
    return int(v) if v.isdigit() else 0


def lane_env(k):
    """The environment of a run in lane k: lane 0 is the agent's own game folder, k >= 1 run.sh's QVR_LANE=k."""
    env = dict(os.environ, KIT=KIT)
    env.pop("QVR_LANE", None)
    if k:
        env["QVR_LANE"] = str(k)
    return env


def game_dir(agent, k=None):
    """The kit's game folder of an agent's lane (default: this process's lane)."""
    k = lane() if k is None else k
    return "%s/bases/%s/qbase%s" % (KIT, agent, ".lane%d" % k if k else "")


# ---- running things

def run(argv, log_path=None, timeout=None, env=None):
    """argv's output (stdout and stderr) into log_path (if given): (exit code, or None on a timeout, text, seconds).
    A timeout kills the whole process tree (the game too)."""
    start = time.time()
    log = open(log_path, "w", encoding="utf-8", errors="replace") if log_path else None
    try:
        if log:
            log.write("$ " + " ".join(argv) + "\n")
            log.flush()
        proc = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                env=env or dict(os.environ, KIT=KIT),
                                creationflags=getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0))
        try:
            out, _ = proc.communicate(timeout=timeout)
            code = proc.returncode
        except subprocess.TimeoutExpired:
            subprocess.run(["taskkill", "/T", "/F", "/PID", str(proc.pid)], capture_output=True)
            out, _ = proc.communicate()
            code = None
        text = out.decode("utf-8", errors="replace").replace("\r\n", "\n")
        if log:
            log.write(text)
    finally:
        if log:
            log.close()
    return code, text, time.time() - start


def run_sh(agent, args, timeout=None, env=None):
    """The kit's run.sh <agent> <args> (a game with the mock headset): its output, CRs dropped."""
    code, text, _ = run([BASH, KIT + "/run.sh", agent] + list(args), None, timeout, env)
    return text.replace("\r", "")


def script_argv(path):
    """The command line that runs a test script (.py, .ps1 or .sh) at path."""
    if path.endswith(".py"):
        return [sys.executable, path]
    if path.endswith(".ps1"):
        return ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", path]
    return [BASH, path]


# ---- parallel lanes: which scripts may run beside which (run_test_suite.py --jobs; the kit's run_baseline.py has its
# own copy of the same rules)

# A script that reads the agent's own game folder by its fixed path (kit bases/<agent>/qbase/..., not the lane's
# qbase.lane<k>: QVR_LANE), the worktree's quakevr/ (a lane's game writes its own), or a fixed network port (the
# multiplayer tests) runs in lane 0 only. Lane-aware: run.sh/run.ps1 (-Instance copies too: qbase.lane<k>.<n>),
# testbatch, and the scripts that build the path with QVR_LANE.
LANE0_RE = re.compile(r'qbase/|"qbase",|(\$W|\$TREE|\{tree\})/quakevr/|qvr-agents/[^/\s"]*/quakevr/|2601[0-9]')
# A suite that plays its cases in copies of the game itself (testbatch: up to 8 copies, kit slots).
BATCHED_RE = re.compile(r"testbatch")
# Timing runs (run.sh --exclusive: every other game, the lanes' too, must end first), unless the script runs them only
# with QVR_TEST_TIMINGS=1 and that is not set.
EXCLUSIVE_RE = re.compile(r"--exclusive")
# Scripts that change a file every game of the worktree reads while they run, in every lane: alone, before the lanes
# (mapgun_pickup: the worktree's quakevr/maps/e1m3.ent).
ALONE_SCRIPTS = {"loadedmapguns/pickup_test.ps1"}
# Scripts that write one shared file of the worktree's (the teleporter tests build maps/vrteleporters.bsp) or play it
# (stealth's gates and props): one at a time ("tele").
TELE_SCRIPTS = {"teleporters/teleporters_test.sh", "teleporters/teleporter_edges_test.sh", "stealth_tests.sh"}
# Scripts whose games must see what an earlier game of theirs wrote to its config (a lane's ironwail.cfg is put back
# from the baseline before every run): lane 0 only.
LANE0_SCRIPTS = {"settings_page_test.py"}
HARNESS_FILES = {"qvrtest.py", "qvrtest.sh", "testbatch.sh", "testbatch.py", "run_batch.sh"}


def _script_text(script, root):
    try:
        with open("%s/%s" % (root, script), encoding="utf-8", errors="replace") as f:
            return re.sub(r"(?m)^\s*#.*$", "", f.read())  # (comments: a path named there is not used)
    except OSError:
        return None


def _uses(script, root, pattern, seen=None):
    """Whether Misc/quakevr/<script> (root: that folder), or a script of root's it calls, matches pattern."""
    seen = seen if seen is not None else set()
    if script in seen:
        return False
    seen.add(script)
    text = _script_text(script, root)
    if text is None:
        return False
    if pattern.search(text):
        return True
    here = os.path.dirname(script)
    for called in set(re.findall(r"([\w./-]+\.(?:sh|py|ps1))\b", text)):
        base = called.split("/")[-1]
        if base in HARNESS_FILES:
            continue  # (the harness itself is lane-aware; its own text names the paths and flags looked for)
        for cand in (os.path.join(here, base).replace("\\", "/").lstrip("/"), called.split("Misc/quakevr/")[-1]):
            if cand != script and os.path.isfile("%s/%s" % (root, cand)) and _uses(cand, root, pattern, seen):
                return True
    return False


def lane_keys(script, root=HERE):
    """A Misc/quakevr script's scheduling keys: "lane0" (lane 0 only), "batch" (a testbatch suite), "alone" (before the
    lanes, nothing beside it), "tele" (one at a time)."""
    keys = set()
    if script in LANE0_SCRIPTS or _uses(script, root, LANE0_RE):
        keys.add("lane0")
    if _uses(script, root, BATCHED_RE):
        keys.add("batch")
    text = _script_text(script, root) or ""
    timings = os.environ.get("QVR_TEST_TIMINGS") == "1" or "QVR_TEST_TIMINGS" not in text
    if script in ALONE_SCRIPTS or (timings and _uses(script, root, EXCLUSIVE_RE)):
        keys.add("alone")
    if script in TELE_SCRIPTS:
        keys.add("tele")
    return keys


def prepare_lanes(agent, lanes, log_dir, timeout=240):
    """Every lane's game folder made (or repaired) before a test uses it: a script may write into the lane's id1 or
    read its qconsole.log before its first game there. One quick game per lane, all at once; the lanes still without
    an id1 folder after it."""
    import threading
    def one(k):
        run([BASH, KIT + "/run.sh", agent, "-Script", "toggleconsole;quit", "-Filter", "^$", "-Timeout", "120"],
            os.path.join(log_dir, "_lane%d_setup.log" % k), timeout, lane_env(k))
    threads = [threading.Thread(target=one, args=(k,)) for k in range(1, lanes)]
    for th in threads:
        th.start()
    for th in threads:
        th.join()
    return [k for k in range(1, lanes) if not os.path.isdir(game_dir(agent, k) + "/id1")]


# ---- the verdict grammar: writing it

class Tally:
    """One test's checks: `<name>: PASS|FAIL <what>` lines and its `<name>: N passed, M failed` verdict."""

    def __init__(self, name):
        self.name = name
        self.passed = 0
        self.failed = 0

    def reset(self):
        """Counts back to 0 (testbatch.two_pass runs a main twice)."""
        self.passed = 0
        self.failed = 0

    def check(self, ok, what):
        print("%s: %s %s" % (self.name, "PASS" if ok else "FAIL", what), flush=True)
        if ok:
            self.passed += 1
        else:
            self.failed += 1
        return bool(ok)

    def verdict(self):
        """Prints the verdict line; the exit status (1 when a check failed)."""
        print("%s: %d passed, %d failed" % (self.name, self.passed, self.failed), flush=True)
        return 1 if self.failed else 0


# ---- the verdict grammar: reading it

# A verdict line's counts (`<name>: N passed, M failed`; the older `N passed M failed` too).
COUNTS = re.compile(r"(\d+) passed,? (\d+) failed")
# A check that failed, for a runner's FAIL/PASS decision: a FAIL or PROBLEM line (bare or after its `<name>: `),
# "FAILURES n", or a verdict with failures.
FAIL_LINE = re.compile(r"^\s*(?:[\w.()/ -]{1,60}:\s+)?(?:FAIL|PROBLEM)\b|FAILURES [1-9]|\b[1-9][0-9]* failed\b", re.M)
# The lines a run_baseline count of failed checks counts: any line with a FAIL or PROBLEM word, or "FAILURES n".
FAIL_ANY = re.compile(r"^\s*(?:\S+:\s*)?.*(\bFAIL\b|PROBLEM\b)|FAILURES [1-9]", re.M)
PASS_WORD = re.compile(r"\bPASS\b")


def counts(text):
    """(passed, failed): the checks' PASS words and FAIL lines in a script's output (a run_baseline row's counts)."""
    return len(PASS_WORD.findall(text)), len(FAIL_ANY.findall(text))


def verdict_near(text, end_re):
    """The counts (passed, failed) of the last verdict line at the run's end condition (a regex on the line that ended
    it, run.sh -QuitOn): None when there is none."""
    end = re.compile(end_re)
    hits = [m for m in COUNTS.finditer(text) if end.search(text[max(0, m.start() - 200):m.end()])]
    return (int(hits[-1].group(1)), int(hits[-1].group(2))) if hits else None


def failed_lines(text):
    return [l.rstrip() for l in text.splitlines() if FAIL_LINE.search(l)]


def load_flaky(path=None):
    """The known-flaky tests (Misc/quakevr/test_flaky.txt): {name: reason}."""
    path = path or os.path.join(HERE, "test_flaky.txt")
    out = {}
    try:
        with open(path, encoding="utf-8") as f:
            for raw in f:
                body, _, why = raw.partition("#")
                for name in body.split():
                    out[name] = why.strip()
    except OSError:
        pass
    return out


FLAKY = load_flaky()


# ---- mock-file builders (the mock headset's console commands, and .mock motion files for vr_mock_play)

def hand(which, x, y, z, pitch=0, yaw=0, roll=0):
    """`vr_mock_hand <main|off> x y z pitch yaw roll` (metres from the head's floor point, degrees)."""
    return "vr_mock_hand %s %s %s %s %s %s %s" % (which, _num(x), _num(y), _num(z), _num(pitch), _num(yaw), _num(roll))


def button(which, name, value, wait=0):
    """`vr_mock_button <main|off> <trigger|grip|...> <value>` (and `wait<n>` after it)."""
    return "vr_mock_button %s %s %s%s" % (which, name, _num(value), ";wait%d" % wait if wait else "")


def grip(which, on):
    """The grip of a mock hand pressed or let go: the grab command and the button (a list of commands)."""
    side = "left" if which == "off" else "right"
    return ["%sgrab%s" % ("+" if on else "-", side), "vr_mock_button %s grip %d" % (which, 1 if on else 0)]


def waits(n):
    """n frames' `wait` commands (a list)."""
    return ["wait"] * n


def motion(path, keys):
    """A .mock motion file for vr_mock_play from (seconds, line) pairs; returns path."""
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        for t, line in keys:
            f.write("%.3f %s\n" % (t, line))
    return path


def _num(v):
    return ("%.4f" % v).rstrip("0").rstrip(".") if isinstance(v, float) else str(v)
