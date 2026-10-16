#!/usr/bin/env python3
"""Reject incomplete gore_headpop_24 profiles before using them as benchmarks."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any


def validate(data: dict[str, Any], expected_frames: int, expected_kills: int = 1,
             expected_deaths: int = 1, console: str = "", expected_heads_popped: int = 0) -> dict[str, Any]:
    counts = data.get("counts", {})
    monsters = counts.get("monsters_alive", {})
    start = int(monsters.get("start", 0))
    end = int(monsters.get("end", 0))
    deaths = start - end
    events = data.get("kill_events", [])
    marks = data.get("marks", [])
    blow = next((m for m in marks if m.get("label") == "blow"), None)
    frame_count = int(data.get("frames", 0))
    blow_start = int(blow.get("frame", -1)) if blow else -1
    blow_end = blow_start + int(blow.get("frames", 0)) if blow else -1
    kills_during_blow = [
        event for event in events
        if blow and blow_start <= int(event.get("frame", -1)) < blow_end
    ]
    headpop = re.search(
        r"goretest: crowd vr_decap_test 14 on ([\d.]+) monsters within [\d.]+: .*?heads cut ([\d.]+) popped ([\d.]+)",
        console,
    )
    observed_heads = int(float(headpop.group(3))) if headpop else 0
    observed_targets = int(float(headpop.group(1))) if headpop else 0
    checks = {
        "profile_frame_count": frame_count >= expected_frames,
        "headset_absence_does_not_pause_simulation": str(
            data.get("settings", {}).get("vr_xr_unfocused_pause", "")
        ) == "0",
        "kill_telemetry_enabled": str(data.get("settings", {}).get("vr_bench_kills", "")) == "1",
        "kill_events_recorded": len(events) >= expected_kills,
        "target_monsters_died": deaths >= expected_deaths,
        "kill_happened_in_blow_window": len(kills_during_blow) >= expected_kills,
    }
    if expected_heads_popped:
        checks["minimum_head_pops_observed"] = observed_heads >= expected_heads_popped
        checks["minimum_target_count_observed"] = observed_targets >= expected_heads_popped
    return {
        "scenario": data.get("name"),
        "valid": all(checks.values()),
        "checks": checks,
        "frames": frame_count,
        "expected_frames": expected_frames,
        "kill_event_count": len(events),
        "kill_events_in_blow_window": len(kills_during_blow),
        "monster_count_start": start,
        "monster_count_end": end,
        "monster_deaths_observed": deaths,
        "blow_window_frames": [blow_start, blow_end],
        "headpop_targets_observed": observed_targets,
        "head_pops_observed": observed_heads,
        "vr_xr_unfocused_pause": data.get("settings", {}).get("vr_xr_unfocused_pause"),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profile", type=Path)
    parser.add_argument("report", type=Path)
    parser.add_argument("--frames", type=int, required=True)
    parser.add_argument("--expected-kills", type=int, default=1)
    parser.add_argument("--expected-deaths", type=int, default=1)
    parser.add_argument("--console", type=Path, help="qconsole.log.test, required for head-pop count validation")
    parser.add_argument("--expected-heads-popped", type=int, default=0)
    args = parser.parse_args()

    if not args.profile.is_file():
        report = {"valid": False, "reason": f"benchmark profile missing: {args.profile}"}
    else:
        data = json.loads(args.profile.read_text(encoding="utf-8"))
        if args.expected_heads_popped and (not args.console or not args.console.is_file()):
            parser.error("--expected-heads-popped requires an existing --console file")
        console = args.console.read_text(encoding="utf-8", errors="replace") if args.console else ""
        report = validate(data, args.frames, args.expected_kills, args.expected_deaths,
                          console, args.expected_heads_popped)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    if not report.get("valid", False):
        print("INVALID WORKLOAD: do not report this capture as a gore/combat profile")
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
