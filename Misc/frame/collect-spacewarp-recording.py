#!/usr/bin/env python3
"""Wait for and copy the one SteamVR recording correlated with a Quake run."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import shutil
import sys
import time
from pathlib import Path


def key(value: str) -> str:
    return re.sub(r"[^a-z0-9]", "", value.casefold().lstrip("#"))


def summary(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    with path.open(newline="", encoding="utf-8-sig", errors="replace") as stream:
        rows = list(csv.reader(stream))
    expected = {"appid", "appname", "totalframes", "frameswithdepth",
                "frameswithmotionvectors", "reprojectedframes"}
    for index, row in enumerate(rows):
        cells = [cell.strip() for cell in row]
        normalized = [key(cell) for cell in cells]
        if len(cells) == 2 and normalized[0] in expected:
            result[normalized[0]] = cells[1]
        elif expected.intersection(normalized) and index + 1 < len(rows):
            values = [cell.strip() for cell in rows[index + 1]]
            for name, value in zip(normalized, values):
                if name in expected and value:
                    result[name] = value
    return result


def refresh_rates(path: Path) -> list[float]:
    rates: set[float] = set()
    with path.open(newline="", encoding="utf-8-sig", errors="replace") as stream:
        rows = list(csv.reader(stream))
    for index, row in enumerate(rows):
        normalized = [key(cell.strip()) for cell in row]
        if "refreshhz" in normalized and index + 1 < len(rows):
            values = [cell.strip() for cell in rows[index + 1]]
            for column, name in enumerate(normalized):
                if name == "refreshhz" and column < len(values):
                    try:
                        rates.add(float(values[column]))
                    except ValueError:
                        pass
        elif len(row) == 2 and normalized[0] == "refreshhz":
            try:
                rates.add(float(row[1].strip()))
            except ValueError:
                pass
    return sorted(rates)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--logs", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True,
                        help="text file containing pre-run recording paths")
    parser.add_argument("--started-at", type=float, required=True,
                        help="Unix time immediately before launching Quake")
    parser.add_argument("--ended-at", type=float, required=True,
                        help="Unix time immediately after the Quake runner exits")
    parser.add_argument("--wait-seconds", type=float, default=20)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--expected-app", default=r"(?i)(ironwail|quake|unleashed)")
    args = parser.parse_args()
    if not args.logs.is_dir() or not args.binary.is_file():
        parser.error("SteamVR log directory and tested binary must exist")
    if args.wait_seconds < 1 or args.wait_seconds > 60:
        parser.error("--wait-seconds must be between 1 and 60")
    try:
        app_pattern = re.compile(args.expected_app)
    except re.error as error:
        parser.error(f"invalid --expected-app pattern: {error}")

    baseline = set(args.baseline.read_text(encoding="utf-8", errors="replace").splitlines())
    deadline = time.monotonic() + args.wait_seconds
    stable: dict[Path, tuple[int, int, int]] = {}
    stable_observations: dict[Path, int] = {}
    selected: Path | None = None
    candidates_seen: list[Path] = []
    while time.monotonic() <= deadline:
        now_candidates = []
        for path in sorted(args.logs.glob("perfrecording-*.csv")):
            try:
                stat = path.stat()
            except FileNotFoundError:
                continue
            if str(path) in baseline or stat.st_mtime < args.started_at - 5:
                continue
            if stat.st_mtime > args.ended_at + args.wait_seconds + 5:
                continue
            now_candidates.append((path, stat.st_size, stat.st_mtime_ns))
        candidates_seen = [item[0] for item in now_candidates]
        if len(now_candidates) > 1:
            print("INDETERMINATE: multiple new SteamVR recordings overlap this run: " +
                  ", ".join(str(item[0]) for item in now_candidates), file=sys.stderr)
            return 3
        if len(now_candidates) == 1:
            path, size, mtime_ns = now_candidates[0]
            previous = stable.get(path)
            if previous == (size, mtime_ns, stat_key(path)):
                stable_observations[path] = stable_observations.get(path, 1) + 1
            else:
                stable[path] = (size, mtime_ns, stat_key(path))
                stable_observations[path] = 1
            if stable_observations[path] >= 3 and size > 0:
                selected = path
                break
        time.sleep(0.5)

    if selected is None:
        print("INDETERMINATE: no unique stable SteamVR CSV completed within the wait window" +
              (f"; candidates: {', '.join(map(str, candidates_seen))}" if candidates_seen else ""),
              file=sys.stderr)
        return 4
    try:
        metrics = summary(selected)
    except (OSError, csv.Error) as error:
        print(f"INDETERMINATE: could not parse SteamVR recording: {error}", file=sys.stderr)
        return 5
    app_name = metrics.get("appname", "")
    app_id = metrics.get("appid", "")
    if not app_pattern.search(app_name):
        print(f"INDETERMINATE: SteamVR CSV appName does not identify Quake/Ironwail: {app_name!r}",
              file=sys.stderr)
        return 6
    summary_id = re.search(r"(?:steam\.app\.)?(\d+)", app_id, re.IGNORECASE)
    file_id = re.search(r"perfrecording-steam\.app\.(\d+)-", selected.name, re.IGNORECASE)
    if summary_id and file_id and summary_id.group(1) != file_id.group(1):
        print(f"INDETERMINATE: CSV appId {app_id!r} does not match its filename {selected.name!r}",
              file=sys.stderr)
        return 8
    try:
        total_frames = int(float(metrics.get("totalframes", "0")))
    except ValueError:
        total_frames = 0
    if total_frames <= 0:
        print("INDETERMINATE: SteamVR CSV has no submitted app frames", file=sys.stderr)
        return 7

    args.out.mkdir(parents=True, exist_ok=True)
    destination = args.out / selected.name
    shutil.copy2(selected, destination)
    identity = {
        "valid": True,
        "source": str(selected),
        "copy": str(destination),
        "steamvr_app_id": app_id,
        "steamvr_app_name": app_name,
        "total_frames": total_frames,
        "observed_refresh_hz": refresh_rates(selected),
        "file_mtime_unix": selected.stat().st_mtime,
        "run_started_unix": args.started_at,
        "run_ended_unix": args.ended_at,
        "binary": str(args.binary),
        "binary_sha256": sha256(args.binary),
        "selection": "new-after-baseline, mtime-correlated, stable-size, appName-verified",
    }
    (args.out / "steamvr-recording-identity.json").write_text(
        json.dumps(identity, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(identity, indent=2))
    return 0


def stat_key(path: Path) -> int:
    """Change detector covering inode replacement as well as file growth."""
    try:
        return path.stat().st_ino
    except FileNotFoundError:
        return -1


if __name__ == "__main__":
    raise SystemExit(main())
