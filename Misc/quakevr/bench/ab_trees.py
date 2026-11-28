"""A/B timing runner (ROUND26.md "pcperf"; BENCHMARKS.md "A/B of two trees"): the same bench scenarios on two kit
trees, interleaved (per scenario and repeat, the trees in alternating order), each run exclusive and paced (bench.sh's
timing mode) after waiting for an idle machine (other workers' builds skew timings by 20% and add 100 ms hitches); flat
scenarios with -flatscreenmode (the real flat-screen game: bench.sh's flat runs are vr_enabled 0 only). Scripts from
this tree's qvrbench.py for both trees (a scene file the older tree lacks: copy it into its quakevr/scenarios).
python Misc/quakevr/bench/ab_trees.py --label L --scenarios a,b,group --trees old,new [--reps 2] [--frames 900]
Results: kit/benchresults/<L>/<tree>/<scenario>/r<k>.json and ab.log; then bench.sh compare <L>/old <L>/new."""
import argparse, os, shutil, subprocess, sys, time

TREE = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")).replace(os.sep, "/")
BASH = "C:/Program Files/Git/bin/bash.exe"
PY = f"{TREE}/Misc/quakevr/bench/qvrbench.py"
sys.path.insert(0, f"{TREE}/Misc/quakevr/bench")
sys.path.insert(0, f"{TREE}/Misc/quakevr")
import qvrbench  # noqa: E402
import qvrtest  # noqa: E402

KIT = qvrtest.KIT

ap = argparse.ArgumentParser()
ap.add_argument("--label", required=True)
ap.add_argument("--scenarios", required=True)
ap.add_argument("--trees", required=True)
ap.add_argument("--reps", type=int, default=2)
ap.add_argument("--frames", type=int, default=900)
ap.add_argument("--hz", type=int, default=90)
ap.add_argument("--eye", type=int, default=2048)
ap.add_argument("--quiet", type=float, default=25.0)
a = ap.parse_args()

scen = qvrbench.scenarios()
names = []
for n in a.scenarios.split(","):
    names += [x.name for x in scen.values() if n in x.groups] if n not in scen else [n]
trees = a.trees.split(",")
out = f"{KIT}/benchresults/{a.label}"
os.makedirs(out, exist_ok=True)


def load():
    r = subprocess.run(["powershell", "-NoProfile", "-Command",
                        "(Get-Counter '\\Processor(_Total)\\% Processor Time' -SampleInterval 2 -MaxSamples 2).CounterSamples"
                        " | ForEach-Object { $_.CookedValue }"], capture_output=True, text=True)
    vals = [float(x.replace(",", ".")) for x in r.stdout.split() if x.strip()]
    return max(vals) if vals else 100.0


def wait_quiet():
    t0 = time.time()
    l = load()
    while l > a.quiet and time.time() - t0 < 900:
        time.sleep(15)
        l = load()
    return l, time.time() - t0


log = open(f"{out}/ab.log", "a")
for k in range(1, a.reps + 1):
    for sc in names:
        order = trees if (k + names.index(sc)) % 2 else list(reversed(trees))
        for t in order:
            D = f"{out}/{t}/{sc}"
            os.makedirs(D, exist_ok=True)
            info = subprocess.run([sys.executable, PY, "script", sc, "--out", f"{D}/script.cfg", "--tag", sc, "--frames",
                                   str(a.frames), "--hz", str(a.hz), "--eye", str(a.eye), "--realtime", "--info"],
                                  capture_output=True, text=True).stdout.strip()
            base = info.split()[0]
            game = f"{KIT}/bases/{t}/{base}"
            shutil.copy(f"{D}/script.cfg", f"{game}/id1/qvrbench.cfg")
            js = f"{game}/quakevr/profile/bench/{sc}.json"
            l, w = wait_quiet()
            start = time.time()
            frames_total = a.frames + 3000
            cmd = [BASH, f"{KIT}/run.sh", "--exclusive", t] + (["-ExtraArgs", "-flatscreenmode"] if scen[sc].flat else []) + \
                  ["-Script", "exec qvrbench.cfg", "-Base", base, "-RealTime", "-Timeout", str(150 + frames_total // a.hz),
                   "-Filter", "vr_bench|BENCH_|rror|ENGINE|TIMEOUT|exit=|Unknown command"]
            r = subprocess.run(cmd, capture_output=True, text=True)
            open(f"{D}/r{k}.log", "w").write(r.stdout + r.stderr)
            if os.path.exists(js) and os.path.getmtime(js) >= start:
                shutil.copy(js, f"{D}/r{k}.json")
                b = subprocess.run([sys.executable, PY, "brief", f"{D}/r{k}.json"], capture_output=True, text=True).stdout.strip()
                line = f"{t} {sc} r{k} (load {l:.0f}% waited {w:.0f}s, after {load():.0f}%): {b[:150]}"
            else:
                line = f"{t} {sc} r{k}: NO RESULT"
            print(line, flush=True)
            log.write(line + "\n")
            log.flush()
for t in trees:
    subprocess.run([sys.executable, PY, "summarize", f"{out}/{t}"], capture_output=True)
print(f"ab done -> {out}", flush=True)
