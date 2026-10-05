"""Summarize perf_suite.py captures without summing nested inclusive scopes."""
import csv
import io
import json
from pathlib import Path
import re
import statistics
import sys


root = Path(sys.argv[1])
records = []
for meta_path in root.glob("*/metadata.json"):
    meta = json.loads(meta_path.read_text())
    folder = meta_path.parent
    files = list(folder.glob("profile_*.csv"))
    if len(files) != 1:
        raise RuntimeError(f"Expected one scope capture in {folder}")
    lines = files[0].read_text().splitlines()
    rows = list(csv.DictReader(io.StringIO("\n".join(l for l in lines if not l.startswith("#")))))
    measured = [r for r in rows if r["interval"] == "2"]
    frame = next(r for r in measured if r["scope"] == "frame")
    if meta["scene"].startswith("portal") and meta["scene"] != "portal_off":
        assert any(r["scope"] == "portal" for r in measured), f"Portal view missing in {folder}"
    if meta["scene"] == "portal_enemies":
        assert "physical=0 pvs=0 gate=8" in (folder / "qconsole.log").read_text(), f"NPC was not across the portal: {folder}"
    log = (folder / "qconsole.log").read_text(errors="replace")
    if "4096" in meta["scene"]:
        assert re.search(r"4096 decals:", log), f"Expected full 4096-mark pool in {folder}"
    if meta["scene"] == "fire":
        fire = re.search(r"fireparticles: static=\d+ dynamic=\d+ torches=(\d+)", log)
        assert fire and int(fire[1]) >= 32, f"Fire emitters missing in {folder}"
    if meta["scene"].startswith("explosions") and "no_debris" not in meta["scene"]:
        assert re.search(r"explosiondebris: live=\d+ made=[1-9]", log), f"Debris did not spawn in {folder}"
    summary = re.findall(r"host frame ([\d.]+) ms, CPU busy ([\d.]+), GPU ([\d.]+)", log)[-1]
    phase = {}
    for name, mean, median, p95, p99, worst in re.findall(
        r"vr_physics_frametime measured ([\w ]+): mean ([\d.]+) median ([\d.]+) p95 ([\d.]+) p99 ([\d.]+) worst ([\d.]+)", log):
        phase[name] = dict(zip(("mean", "median", "p95", "p99", "worst"), map(float, (mean, median, p95, p99, worst))))
    if not phase:
        raise RuntimeError(f"No simulation frames measured in {folder}; check console/pause state")
    scopes = {}
    for r in measured:
        if r["scope"] in ("frame", "frame period"):
            continue
        name = r["scope"]
        values = scopes.setdefault(name, {"cpu": 0, "gpu": 0})
        values["cpu"] += float(r["cpu_self_ms"] or 0)
        values["gpu"] += float(r["gpu_self_ms"] or 0)
    records.append(dict(meta, cpu_ms=float(frame["cpu_avg_ms"]), gpu_ms=float(frame["gpu_avg_ms"] or 0),
                        cpu_max_ms=float(frame["cpu_max_ms"]), gpu_max_ms=float(frame["gpu_max_ms"] or 0),
                        gpu_frames=int(frame["gpu_frames"]), measured_frames=int(frame["frames"]),
                        cpu_busy_ms=float(summary[1]), scopes=scopes, physics=phase,
                        counts=re.findall(r"counts a frame \(avg/max\): (.*)", log)[-1]))

groups = {}
for r in records:
    groups.setdefault((r["scene"], r["eye"], r["gpu_every"]), []).append(r)
aggregates = []
for (scene, eye, gpu), runs in sorted(groups.items()):
    med = lambda key: statistics.median(r[key] for r in runs)
    out = dict(scene=scene, eye=eye, gpu_every=gpu, repeats=len(runs), cpu_ms=med("cpu_ms"), gpu_ms=med("gpu_ms"),
               cpu_min_ms=min(r["cpu_ms"] for r in runs), cpu_max_mean_ms=max(r["cpu_ms"] for r in runs),
               worst_cpu_ms=max(r["cpu_max_ms"] for r in runs), worst_gpu_ms=max(r["gpu_max_ms"] for r in runs),
               gpu_samples=sum(r["gpu_frames"] for r in runs), measured_frames=sum(r["measured_frames"] for r in runs),
               counts=runs[-1]["counts"])
    names = {name for r in runs for name in r["scopes"]}
    out["scopes"] = {name: {kind: statistics.median(r["scopes"].get(name, {}).get(kind, 0) for r in runs)
                            for kind in ("cpu", "gpu")} for name in names}
    phases = {name for r in runs for name in r["physics"]}
    out["physics"] = {name: {key: (max if key == "worst" else statistics.median)(r["physics"].get(name, {}).get(key, 0) for r in runs)
                              for key in ("mean", "median", "p95", "p99", "worst")} for name in phases}
    aggregates.append(out)

(root / "summary.json").write_text(json.dumps(aggregates, indent=2))
(root / "runs.json").write_text(json.dumps(records, indent=2))
with (root / "summary.csv").open("w", newline="") as file:
    fields = [k for k in aggregates[0] if k not in ("scopes", "physics")]
    writer = csv.DictWriter(file, fieldnames=fields, extrasaction="ignore")
    writer.writeheader()
    writer.writerows(aggregates)
for r in aggregates:
    print(f"{r['scene']:24} {r['eye']:4} gpu/{r['gpu_every']:2} n={r['repeats']} CPU={r['cpu_ms']:.3f} GPU={r['gpu_ms']:.3f} "
          f"physics={r['physics']['server physics']['mean']:.3f} step_p95={r['physics']['step']['p95']:.3f}")
    for kind in ("cpu", "gpu"):
        biggest = sorted(r["scopes"].items(), key=lambda item: -item[1][kind])[:6]
        print("  " + kind + ": " + ", ".join(f"{name} {v[kind]:.3f}" for name, v in biggest))
