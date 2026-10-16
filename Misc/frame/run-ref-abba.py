#!/usr/bin/env python3
"""Run spacewarp-ref legs against SteamVR with temporary, restored settings (runs ON the Frame).

Each leg launches the reference app, waits until it owns compositor scene focus, records SteamVR's compositor
frame timings (frame-compositor-timings) and its performance recording, and writes leg-result.json. SteamVR
settings changed for a run (motion smoothing, per-app override, throttle, recordStats) are restored on exit.

  run-ref-abba.py --out DIR --legs "off-a1=--api none;on-b1=--api ext" [--smoothing keep|0|1]
                  [--override N] [--throttle N] [--app-key KEY] [--seconds 15] [--common "--load-ms 10"]

--override: <app key>.motionSmoothingOverride (0 global, 1 enabled, 2 disabled, 3 force always-on)
--throttle: <app key>.framesToThrottle. Both are restored to their previous value (0 when unset).
"""
from __future__ import annotations

import argparse
import json
import os
import shlex
import statistics
import subprocess
import sys
import time
from pathlib import Path

HOME = Path.home()
VRCMD = "/opt/steamvr/bin/linuxarm64/vrcmd"
LOGS = HOME / ".local/share/Steam/logs"
NATIVE = HOME / "Games/QuakeVR-Unleashed-native"
STATE_PROBE = NATIVE / "frame-vr-state.py"
COLLECTOR = HOME / "dev/qvr-next2/frame-compositor-timings"
RECORDING_COLLECTOR = NATIVE / "source-next2-tools/Misc/frame/collect-spacewarp-recording.py"


def vrcmd(*args: str) -> str:
    return subprocess.run([VRCMD, *args], capture_output=True, text=True, timeout=30).stdout.strip()


def get_setting(kind: str, key: str) -> str | None:
    raw = vrcmd(f"--settings-{kind}", key)
    prefix = key + "="
    return raw[len(prefix):] if raw.startswith(prefix) else None


def set_setting(kind: str, key: str, value: int) -> None:
    vrcmd(f"--set-settings-{kind}", key, str(value))
    actual = get_setting(kind, key)
    norm = {"true": "1", "false": "0"}.get(actual or "", actual)
    if norm != str(value):
        raise RuntimeError(f"SteamVR setting {key}: expected {value}, got {actual}")


def vr_state() -> dict:
    try:
        out = subprocess.run([sys.executable, str(STATE_PROBE)], capture_output=True, text=True, timeout=20).stdout
        return json.loads(out)
    except Exception:
        return {}


def appmv_count() -> int:
    try:
        return sum(1 for line in (LOGS / "vrcompositor.txt").read_text(errors="replace").splitlines()
                   if "Using motion encoding AppMVs" in line)
    except OSError:
        return 0


def pct(values: list[float], p: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(round(p / 100 * (len(ordered) - 1))))]


def compositor_stats(path: Path, skip_seconds: float) -> dict:
    rows, summary = [], {}
    for line in path.read_text(errors="replace").splitlines():
        try:
            row = json.loads(line)
        except json.JSONDecodeError:
            continue
        if row.get("type") == "frame":
            rows.append(row)
        elif row.get("type") == "summary":
            summary = row
    if not rows:
        return {"frames": 0, "summary": summary}
    t0 = rows[0]["system_seconds"] + skip_seconds
    rows = [r for r in rows if r["system_seconds"] >= t0] or rows
    span = rows[-1]["system_seconds"] - rows[0]["system_seconds"]
    iv = [r["interval_ms"] for r in rows if r["interval_ms"] > 0]
    gpu = [r["total_gpu_ms"] for r in rows]
    return {
        "frames": len(rows),
        "frames_per_s": len(rows) / span if span > 0 else 0.0,
        "motion_flag_pct": 100.0 * sum(1 for r in rows if r["motion_triggered"]) / len(rows),
        "interval_ms_avg": statistics.mean(iv) if iv else 0.0,
        "interval_ms_p50": statistics.median(iv) if iv else 0.0,
        "gpu_ms_avg": statistics.mean(gpu),
        "gpu_ms_p95": pct(gpu, 95),
        "mispresented": sum(r["mispresented"] for r in rows),
        "dropped": sum(r["dropped"] for r in rows),
        "presents": sum(r["presents"] for r in rows),
        "stop_reason": summary.get("stop_reason"),
        "scene_changed_while_app_alive": summary.get("scene_changed_while_app_alive"),
    }


def run_leg(args: argparse.Namespace, tag: str, app_args: list[str], out: Path) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    before = sorted(p.name for p in LOGS.glob("perfrecording-*.csv"))
    (out / "perfrecording-before.txt").write_text("\n".join(before) + "\n")
    mv_before = appmv_count()
    set_setting("bool", "perfcheck.recordStats", 1)
    started = time.time()
    env = dict(os.environ, XR_RUNTIME_JSON=os.environ.get("XR_RUNTIME_JSON", "/opt/steamvr/steamxr_linuxarm64.json"))
    cmd = [str(args.bin), *shlex.split(args.common), *app_args, "--seconds", str(args.seconds),
           "--warmup-seconds", str(args.warmup), "--log", str(out / "app.log"), "--summary", str(out / "summary.json")]
    (out / "command.txt").write_text(shlex.join(cmd) + "\n")
    app = subprocess.Popen(cmd, env=env, stdout=open(out / "app.stdout", "w"), stderr=subprocess.STDOUT)
    result: dict = {"tag": tag, "app_args": app_args, "pid": app.pid}
    focused = False
    for _ in range(120):
        if app.poll() is not None:
            break
        state = vr_state()
        (out / "focus-state.json").write_text(json.dumps(state))
        if int(state.get("compositor_scene_focus_pid", 0)) == app.pid:
            focused = True
            break
        time.sleep(0.5)
    result["focused"] = focused
    collector = None
    if focused:
        run_for = max(5, min(60, int(args.seconds) - 3))
        collector = subprocess.Popen([str(COLLECTOR), "--run", str(run_for)], stdout=open(out / "compositor-timings.jsonl", "w"),
                                     stderr=open(out / "compositor-timings.stderr", "w"))
    try:
        app.wait(timeout=args.seconds + 60)
    except subprocess.TimeoutExpired:
        app.terminate()
        app.wait(timeout=20)
        result["app_timeout"] = True
    ended = time.time()
    result["app_exit"] = app.returncode
    if collector:
        try:
            collector.wait(timeout=10)
        except subprocess.TimeoutExpired:
            collector.send_signal(2)
            collector.wait(timeout=10)
        result["collector_exit"] = collector.returncode
    set_setting("bool", "perfcheck.recordStats", 0)
    rec = subprocess.run([sys.executable, str(RECORDING_COLLECTOR), "--logs", str(LOGS), "--out", str(out),
                          "--baseline", str(out / "perfrecording-before.txt"), "--started-at", str(started),
                          "--ended-at", str(ended), "--wait-seconds", "20", "--binary", str(args.bin),
                          "--expected-app", args.expected_app], capture_output=True, text=True)
    (out / "steamvr-recording-collection.json").write_text(rec.stdout)
    try:
        result["steamvr_recording"] = json.loads(rec.stdout)
    except json.JSONDecodeError:
        result["steamvr_recording"] = {"error": rec.stderr[-300:]}
    result["appmv_log_lines"] = appmv_count() - mv_before
    cj = out / "compositor-timings.jsonl"
    if cj.exists() and cj.stat().st_size:
        result["compositor"] = compositor_stats(cj, args.skip_seconds)
    sj = out / "summary.json"
    if sj.exists():
        result["app"] = json.loads(sj.read_text())
    (out / "leg-result.json").write_text(json.dumps(result, indent=1))
    return result


def line(result: dict) -> str:
    c, a = result.get("compositor", {}), result.get("app", {})
    fm = a.get("frame_ms", {})
    rec = result.get("steamvr_recording", {})
    return (f"{result['tag']:<8} fps {a.get('fps', 0):5.1f} unfoc {a.get('unfocused_frames', 0):3d} | app frame {fm.get('avg', 0):5.1f} (p95 {fm.get('p95', 0):5.1f}) cpu {a.get('cpu_busy_ms', {}).get('avg', 0):4.1f} "
            f"gpu {a.get('gpu_ms', {}).get('avg', 0):5.1f} | comp flag {c.get('motion_flag_pct', 0):3.0f}% gpu {c.get('gpu_ms_avg', 0):5.1f} "
            f"interval {c.get('interval_ms_avg', 0):5.1f}/{c.get('interval_ms_p50', 0):5.1f} mispres {c.get('mispresented', 0)} "
            f"drop {c.get('dropped', 0)} | AppMVs-log {result.get('appmv_log_lines')} mv-frames {rec.get('frames_with_motion_vectors', rec.get('frameswithmotionvectors', '?'))}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", type=Path, default=HOME / "Games/spacewarp-ref/spacewarp-ref")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--legs", required=True, help="tag=app args;tag=app args")
    ap.add_argument("--common", default="", help="app args added to every leg")
    ap.add_argument("--seconds", type=int, default=18, help="app run length counted from FOCUSED; measured window is warmup..seconds")
    ap.add_argument("--warmup", type=float, default=6.0, help="skips the standby FOCUSED/VISIBLE blip right after connect")
    ap.add_argument("--skip-seconds", type=float, default=None, help="compositor rows skipped at the start (default: --warmup)")
    ap.add_argument("--smoothing", default="keep", choices=["keep", "0", "1"])
    ap.add_argument("--override", default="keep")
    ap.add_argument("--throttle", default="keep")
    ap.add_argument("--app-key", default="system.generated.openxr.spacewarp-ref.spacewarp-ref")
    ap.add_argument("--expected-app", default=r"(?i)(spacewarp|ref)")
    args = ap.parse_args()
    if args.skip_seconds is None:
        args.skip_seconds = args.warmup
    legs = [(t.strip(), shlex.split(a)) for t, a in (leg.split("=", 1) for leg in args.legs.split(";") if leg.strip())]
    args.out.mkdir(parents=True, exist_ok=True)
    original = {
        "rec": get_setting("bool", "perfcheck.recordStats"),
        "smoothing": get_setting("bool", "steamvr.motionSmoothing"),
        "override": get_setting("int", f"{args.app_key}.motionSmoothingOverride"),
        "throttle": get_setting("int", f"{args.app_key}.framesToThrottle"),
    }
    (args.out / "settings-original.json").write_text(json.dumps(original))
    results = []
    try:
        if args.smoothing != "keep":
            set_setting("bool", "steamvr.motionSmoothing", int(args.smoothing))
        if args.override != "keep":
            set_setting("int", f"{args.app_key}.motionSmoothingOverride", int(args.override))
        if args.throttle != "keep":
            set_setting("int", f"{args.app_key}.framesToThrottle", int(args.throttle))
        for tag, app_args in legs:
            result = run_leg(args, tag, app_args, args.out / tag)
            results.append(result)
            try:
                print(line(result), flush=True)
            except Exception as exc:  # a formatting problem must not abort the remaining legs
                print(f"{tag}: result saved, summary line failed: {exc}", flush=True)
    finally:
        def truthy(v: str | None) -> int:
            return 1 if v in ("1", "true") else 0
        set_setting("bool", "perfcheck.recordStats", truthy(original["rec"]))
        if args.smoothing != "keep":
            set_setting("bool", "steamvr.motionSmoothing", truthy(original["smoothing"]))
        if args.override != "keep":
            set_setting("int", f"{args.app_key}.motionSmoothingOverride", int(original["override"] or 0))
        if args.throttle != "keep":
            set_setting("int", f"{args.app_key}.framesToThrottle", int(original["throttle"] or 0))
    (args.out / "results.json").write_text(json.dumps(results, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
