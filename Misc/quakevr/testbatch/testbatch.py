#!/usr/bin/env python3
"""testbatch.py -- a test suite's headless game runs played as batches: many cases in one game, the cases split
between parallel copies of the game (docs/vr-port/TESTING.md, "Batched test cases").

A case is what a suite passed to the kit's run.sh after the agent's name (-Script, -Filter, -Timeout, -Base, -Games,
-ExtraArgs, ...). Cases with the same game options (-Base, -Games, -ExtraArgs, -Sound, -Debug, -Tracy) share games:
each copy of the game (kit run.ps1 -Instance <k>, its own game folder) runs its share one after another, each case
from its own cfg file between markers:
    echo @@TB_BEGIN <n>; vr_test_reset; <the case's commands>; disconnect; echo @@TB_END <n>
after "vr_fixed_frames 1; vr_test_snapshot" (TB_FIXED=0: real-time frames, as run.sh's). vr_test_reset (engine,
vr_testbatch.cpp) puts the cvars, the +buttons and the mock headset back to the batch's start; the case's own "map"
does the rest. Each case's output is then what run.sh would have printed for it alone: "exit=0" (or the copy's
TIMEOUT / ENGINE ERROR / ENGINE CRASH lines if the case was the one that ended it) and its own console lines through
its -Filter (Select-String's: a regex, case-insensitive). A case that never started (its copy ended early) runs again
in a new copy. A case that runs longer than its -Timeout (default 120 s) is stopped as run.sh would ("TIMEOUT after
<n> s"), and the cases after it run again.

Not batched (run alone by run.sh, by the caller): -Out, -Clean, -RealTime, -Visible, -Full, and a script that takes a
screenshot (its files would land in the copy's folder).

Bash suites use testbatch.sh (tb_main / tb_run_sh, two passes over the suite); Python suites use Batch below:
    import testbatch
    b = testbatch.Batch(agent, "mysuite")
    h1 = b.case(["-Script", "map e1m1;wait60;...;toggleconsole;quit", "-Filter", "^foo"])
    b.run()                  # the batches (kit slots, the agent's lock), or each case by run.sh with TB_SLOW=1
    out = b.output(h1)       # what run.sh would have printed
Command line (testbatch.sh and run_batch.sh call it):
    testbatch.py exec <dir> <agent> <copies>     run <dir>/case_<n>.args (NUL-separated run.sh arguments) as
                                                 batches; out_<n>.txt for each batched case
    testbatch.py batchable <dir> <n>             exit 0 if case n is batched
Environment: TB_FIXED (1), TB_JOBS (copies wanted: 8), TB_SLOW (1: every case by run.sh, as before), TB_KEEP_ORDER.
"""
import json
import os
import re
import subprocess
import sys
import threading
import time

KIT = os.environ.get("KIT", "C:/OHWorkspace/qvr-kit")
HERE = os.path.dirname(os.path.abspath(__file__))
NOISE = re.compile(r"^(FindFile: can't find|W_GetLump|Couldn't find a cdrip|\s+[0-9]+\.[0-9] ms  )")
HEADER = re.compile(r"^(exit=|TIMEOUT after|ENGINE ERROR|ENGINE CRASH|run\.ps1:)")
UNBATCHED_FLAGS = {"-Out", "-Clean", "-RealTime", "-Visible", "-Full"}
VALUE_OPTS = {"-Script", "-Filter", "-Timeout", "-Base", "-Games", "-ExtraArgs", "-Out", "-Affinity", "-Instance", "-Name"}
LAUNCH_OPTS = {"-Base", "-Games", "-ExtraArgs", "-Sound", "-Debug", "-Tracy"}
DEFAULT_GAMES = "-game hipnotic -game rogue -game quakevr"


def parse_args(args):
    """run.sh's arguments (after the agent) -> dict: script, filter, timeout, launch (tuple of game options), flags."""
    c = {"script": "quit", "filter": "", "timeout": 120, "launch": [], "flags": set(), "base": "qbase",
         "games": DEFAULT_GAMES}
    i = 0
    while i < len(args):
        a = args[i]
        key = next((k for k in VALUE_OPTS | LAUNCH_OPTS | UNBATCHED_FLAGS if k.lower() == a.lower()), None)
        if key in VALUE_OPTS:
            v = args[i + 1] if i + 1 < len(args) else ""
            i += 2
            if key == "-Script":
                c["script"] = v
            elif key == "-Filter":
                c["filter"] = v
            elif key == "-Timeout":
                c["timeout"] = int(v)
            elif key in LAUNCH_OPTS:
                c["launch"] += [key, v]
                if key == "-Base":
                    c["base"] = v
                elif key == "-Games":
                    c["games"] = v
            else:
                c["flags"].add(key)
            continue
        if key:
            (c["launch"].append(key) if key in LAUNCH_OPTS else c["flags"].add(key))
        else:
            c["flags"].add(a)  # unknown: run alone
        i += 1
    c["launch"] = tuple(c["launch"])
    return c


def batchable(c):
    return not (c["flags"] & UNBATCHED_FLAGS or c["flags"] - UNBATCHED_FLAGS or
                re.search(r"(^|;)\s*screenshot", c["script"], re.I))


def case_lines(script):
    """The case's commands as run.ps1 writes them (waitN expanded), without its closing toggleconsole/quit."""
    cmds = [s.strip() for s in script.split(";")]
    cmds = [s for s in cmds if s]
    while cmds and cmds[-1].lower() == "quit":
        cmds.pop()
        if cmds and cmds[-1].lower() == "toggleconsole":
            cmds.pop()
    out = []
    for s in cmds:
        m = re.match(r"^wait(\d+)$", s)
        if m:
            out += ["wait"] * int(m.group(1))
        elif s.lower() == "quit":
            out += ["disconnect"]  # (a quit mid-script: this case ends there)
        else:
            out.append(s)
    return out


def cost(c):
    lines = case_lines(c["script"])
    return 60 + sum(1 for l in lines if l == "wait") + 150 * sum(1 for l in lines if re.match(r"(map|changelevel) ", l))


def game_dir(agent, base, k):
    return f"{KIT}/bases/{agent}/{base}.{k}"


def link_extra_games(agent, base, k):
    """run.ps1 links an instance's id1 files, hipnotic, rogue and quakevr; the agent's other game folders (ad: -Games
    or vr_campaign_native) are linked here the same way (a junction to where the agent's points)."""
    own = f"{KIT}/bases/{agent}/{base}"
    os.makedirs(game_dir(agent, base, k), exist_ok=True)
    for g in os.listdir(own):
        if g.lower() in ("hipnotic", "rogue", "quakevr", "id1"):
            continue
        src, dst = os.path.join(own, g), os.path.join(game_dir(agent, base, k), g)
        if os.path.isdir(src) and not os.path.exists(dst):
            import _winapi
            _winapi.CreateJunction(os.path.realpath(src), dst)


def kill_game(gdir):
    gd = gdir.replace("/", "\\")
    subprocess.run(["powershell", "-NoProfile", "-Command",
                    "Get-CimInstance Win32_Process -Filter \"Name='ironwail.exe'\" | Where-Object { $_.CommandLine -like "
                    f"'*{gd}*' }} | ForEach-Object {{ Stop-Process -Id $_.ProcessId -Force }}"],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def run_copy(agent, k, launch, base, games, cases, result):
    """One copy of the game playing `cases` [(n, parsed)] in order; result[k] = (stdout lines, {n: timed out s})."""
    gdir = game_dir(agent, base, k)
    os.makedirs(f"{gdir}/id1/tb", exist_ok=True)
    link_extra_games(agent, base, k)
    # Unpaced frames of the real-time run's length: a real-time test run draws host_maxfps (the test baseline's 250)
    # frames a second over the server's 72 Hz ticks (host_fixedtick), and the client's per-frame work (the hands, their
    # hits) is tuned on that; vr_fixed_frames_rate 72 (a tick a frame) is faster still but changes such results. Set
    # for each case after its reset (the snapshot keeps the baseline's), unless the case sets the clock itself (its
    # own vr_fixed_frames: at its rate, as alone).
    rate = os.environ.get("TB_RATE", "250")
    fixed = ["vr_fixed_frames 1", f"vr_fixed_frames_rate {rate}"] if os.environ.get("TB_FIXED", "1") != "0" else []
    execs = []
    for n, c in cases:
        own = re.search(r"(^|;)\s*vr_fixed_frames\b", c["script"]) is not None
        body = ([f"echo @@TB_BEGIN {n}", "vr_test_reset"] + ([] if own else fixed) + case_lines(c["script"]) +
                ["disconnect", f"echo @@TB_END {n}"])
        with open(f"{gdir}/id1/tb/c{n}.cfg", "w", encoding="ascii", errors="replace", newline="\n") as f:
            f.write("\n".join(body) + "\n")
        execs.append(f"exec tb/c{n}.cfg")
    pre = ["vr_mock_fast 2"] if os.environ.get("TB_NODRAW") == "1" else []  # (unpaced frames not drawn)
    script = ";".join(pre + ["vr_test_snapshot"] + execs + ["toggleconsole", "quit"])
    total = 120 + sum(c["timeout"] for _, c in cases)
    argv = ["powershell", "-NoProfile", "-File", f"{KIT}/run.ps1", "-Name", agent, "-Instance", str(k), "-Low",
            "-Timeout", str(total), "-Script", script] + list(launch)
    proc = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    chunks = []
    reader = threading.Thread(target=lambda: chunks.append(proc.stdout.read()))
    reader.start()
    # The watchdog: a case past its -Timeout ends the game (run.ps1 then prints its exit).
    limits = {n: c["timeout"] for n, c in cases}
    log = f"{gdir}/qconsole.log"
    timed_out = {}
    cur, since, pos, partial = None, None, 0, ""
    time.sleep(0.5)
    while proc.poll() is None:
        try:
            if os.path.getsize(log) < pos:
                pos, partial = 0, ""  # (the game's new log; the one read was the folder's last game's)
            with open(log, "rb") as f:
                f.seek(pos)
                data = f.read()
                pos += len(data)
        except OSError:
            data = b""
        if data:
            partial += data.decode("utf-8", "replace")
            *done, partial = partial.split("\n")
            for line in done:
                m = re.match(r"@@TB_(BEGIN|END) (\d+)", line.strip())
                if m and int(m.group(2)) in limits:  # (another shard's: the folder's last game's log)
                    cur, since = (int(m.group(2)), time.time()) if m.group(1) == "BEGIN" else (None, None)
        if cur is not None and time.time() - since > limits[cur]:
            timed_out[cur] = limits[cur]
            kill_game(gdir)
            cur = None
        time.sleep(0.2)
    reader.join()
    text = b"".join(chunks).decode("utf-8", "replace")
    result[k] = (text.replace("\r\n", "\n").split("\n"), timed_out)


def split_copy(lines, timed_out):
    """A copy's run.ps1 output -> (header lines, {n: case lines}, {n: ended})."""
    i = next((j for j, l in enumerate(lines) if "execing autoexec.cfg" in l), len(lines))
    header = [l for l in lines[:i] if l.strip()]
    per, ended, cur = {}, {}, None
    for l in lines[i:]:
        m = re.match(r"^@@TB_(BEGIN|END) (\d+)\s*$", l)
        if m:
            n = int(m.group(2))
            if m.group(1) == "BEGIN":
                cur = n
                per[n], ended[n] = [], False
            else:
                ended[n] = True
                cur = None
            continue
        if cur is not None:
            per[cur].append(l)
    return header, per, ended


def render(c, header, lines):
    """What run.sh prints for the case: the process's lines, then its log lines (through -Filter)."""
    out = list(header)
    if c["filter"]:
        rx = re.compile(c["filter"], re.I)
        out += [""] + [l for l in lines if rx.search(l)] + ["", ""]
    else:
        out += lines
    return "\r\n".join(out) + "\r\n"


def exec_dir(d, agent, copies):
    cases = {}
    for name in os.listdir(d):
        m = re.match(r"^case_(\d+)\.args$", name)
        if m:
            with open(os.path.join(d, name), "rb") as f:
                data = f.read()
            if not data:
                continue  # (an earlier run's, emptied)
            c = parse_args([a.decode("utf-8", "replace") for a in data.split(b"\0")[:-1]])
            if batchable(c):
                cases[int(m.group(1))] = c
    t0 = time.time()
    stats = {"games": 0, "cases": len(cases), "rounds": 0}
    pending = dict(cases)
    while pending:
        stats["rounds"] += 1
        groups = {}
        for n, c in sorted(pending.items()):
            groups.setdefault((c["launch"], c["base"], c["games"]), []).append((n, c))
        progressed = False
        for (launch, base, games), members in groups.items():
            k = max(1, min(copies, len(members)))
            if os.environ.get("TB_ISOLATE") == "1":
                # a game for each case (still unpaced and in parallel): a suite whose cases can't share a game
                shards = [[nc] for nc in sorted(members, key=lambda nc: -cost(nc[1]))]
            else:
                shards, load = [[] for _ in range(k)], [0] * k
                for n, c in sorted(members, key=lambda nc: -cost(nc[1])):
                    j = load.index(min(load))
                    load[j] += cost(c)
                    shards[j].append((n, c))
            for sh in shards:
                sh.sort(key=lambda nc: nc[0])
            # k copies at once (each its own game folder), each taking the next shard when its game ends
            result, queue, lock = {}, list(range(len(shards))), threading.Lock()

            def worker(inst):
                while True:
                    with lock:
                        if not queue:
                            return
                        j = queue.pop(0)
                    got = {}
                    try:
                        run_copy(agent, inst, launch, base, games, shards[j], got)
                    except Exception as e:  # (its cases never started: the next round runs them)
                        got[inst] = ([f"testbatch: copy {inst} failed: {e!r}"], {})
                    result[j] = got[inst]
            threads = [threading.Thread(target=worker, args=(i + 1,)) for i in range(k)]
            for t in threads:
                t.start()
            for t in threads:
                t.join()
            stats["games"] += len(shards)
            for j in range(len(shards)):
                lines, timed_out = result[j]
                header, per, ended = split_copy(lines, timed_out)
                for n, c in shards[j]:
                    if n not in per:
                        continue  # never started: again in the next round
                    progressed = True
                    if ended.get(n):
                        hdr = ["exit=0"]
                    elif n in timed_out:
                        hdr = [f"TIMEOUT after {timed_out[n]} s"] + [h for h in header if h.startswith("ENGINE")]
                    else:
                        hdr = header or ["exit=?"]
                    with open(os.path.join(d, f"out_{n}.txt"), "w", encoding="utf-8", newline="") as f:
                        f.write(render(c, hdr, per[n]))
                    del pending[n]
        if not progressed:
            for n, c in pending.items():
                with open(os.path.join(d, f"out_{n}.txt"), "w", encoding="utf-8", newline="") as f:
                    f.write("testbatch: the case never started (its copies of the game ended before it)\r\n")
            break
    stats["seconds"] = round(time.time() - t0, 1)
    with open(os.path.join(d, "stats.json"), "w") as f:
        json.dump(stats, f)


class Batch:
    """A Python suite's cases: case() each, run() once, output() each (TB_SLOW=1: each by run.sh at run())."""

    def __init__(self, agent, name):
        self.agent = agent
        tree = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
        self.dir = os.path.join(tree, "scratch", "testbatch", name)
        os.makedirs(self.dir, exist_ok=True)
        for f in os.listdir(self.dir):
            if re.match(r"^(case_\d+\.args|out_\d+\.txt)$", f):
                open(os.path.join(self.dir, f), "w").close()  # (emptied: no stale case or output read as this run's)
        self.args = []
        self.outs = {}

    def case(self, args):
        n = len(self.args) + 1
        self.args.append(list(args))
        with open(os.path.join(self.dir, f"case_{n}.args"), "wb") as f:
            f.write(b"".join(a.encode("utf-8") + b"\0" for a in args))
        return n

    def _run_sh(self, args):
        bash = os.environ.get("BASH_EXE", "C:/Program Files/Git/bin/bash.exe")
        r = subprocess.run([bash, f"{KIT}/run.sh", self.agent] + list(args), stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT)
        return r.stdout.decode("utf-8", "replace").replace("\r\n", "\n")

    def run(self):
        if os.environ.get("TB_SLOW") == "1" or not self.args:
            for n, a in enumerate(self.args, 1):
                self.outs[n] = self._run_sh(a)
            return
        self.run_batched()
        for n, a in enumerate(self.args, 1):
            if n not in self.outs:
                self.outs[n] = self._run_sh(a)  # not batched (-Out, a screenshot, ...): alone, as before

    def run_batched(self):
        """The batchable cases' outputs (self.outs); the others are left out."""
        if not self.args:
            return
        bash = os.environ.get("BASH_EXE", "C:/Program Files/Git/bin/bash.exe")
        subprocess.run([bash, os.path.join(HERE, "run_batch.sh"), self.agent, self.dir], check=False)
        for n in range(1, len(self.args) + 1):
            p = os.path.join(self.dir, f"out_{n}.txt")
            if os.path.exists(p) and os.path.getsize(p):
                with open(p, encoding="utf-8", errors="replace") as f:  # (newlines as text=True reads them)
                    self.outs[n] = f.read()

    def output(self, n):
        return self.outs[n]


_two = {"pass": None, "batch": None, "n": 0}


def run_sh(agent, args, exclusive=False):
    """In place of running the kit's run.sh [--exclusive] <agent> <args> (its output, stdout and stderr), inside
    two_pass: the first pass collects (returns ""), the second returns the batched output (or runs it alone, as
    testbatch.sh's tb_run_sh does)."""
    bash = os.environ.get("BASH_EXE", "C:/Program Files/Git/bin/bash.exe")
    args = list(args)
    alone = [bash, f"{KIT}/run.sh"] + (["--exclusive"] if exclusive else []) + [agent] + args
    b = _two["batch"]
    if _two["pass"] == "collect" and not exclusive:
        _two["n"] = b.case(args)
        return ""
    if _two["pass"] == "replay" and not exclusive:
        # the first collected run with the same arguments not handed out yet (by arguments, not position: a run
        # retried a different number of times than in the first pass leaves the others where they were)
        used = _two.setdefault("used", set())
        n = next((i for i, a in enumerate(b.args, 1) if a == args and i not in used), None)
        if n is not None:
            used.add(n)
            if n in b.outs:
                return b.outs[n]
        solo = Batch(agent, os.path.basename(b.dir) + "_solo")
        solo.case(args)
        solo.run()
        return solo.output(1)
    r = subprocess.run(alone, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return r.stdout.decode("utf-8", "replace").replace("\r\n", "\n")


def two_pass(agent, name, fn, *a, **kw):
    """Runs fn (a suite's body, its runs through run_sh) twice around one batched run: first collecting its runs (its
    output and any exception thrown away: its checks ran on empty outputs), then for real. TB_SLOW=1: once, each run by
    run.sh as before."""
    if os.environ.get("TB_SLOW") == "1":
        return fn(*a, **kw)
    import contextlib
    import io
    b = Batch(agent, name)
    _two.update({"pass": "collect", "batch": b, "n": 0})
    try:
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            fn(*a, **kw)
    except BaseException:  # (SystemExit too: the first pass's verdict means nothing)
        pass
    _two.update({"pass": None})
    b.run_batched()
    _two.update({"pass": "replay", "n": 0, "used": set()})
    try:
        return fn(*a, **kw)
    finally:
        _two.update({"pass": None})


def main():
    if len(sys.argv) >= 5 and sys.argv[1] == "exec":
        exec_dir(sys.argv[2], sys.argv[3], int(sys.argv[4]))
        return 0
    if len(sys.argv) == 4 and sys.argv[1] == "batchable":
        with open(os.path.join(sys.argv[2], f"case_{sys.argv[3]}.args"), "rb") as f:
            args = [a.decode("utf-8", "replace") for a in f.read().split(b"\0")[:-1]]
        return 0 if batchable(parse_args(args)) else 1
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
