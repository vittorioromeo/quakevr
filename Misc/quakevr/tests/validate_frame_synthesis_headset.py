#!/usr/bin/env python3
"""Validate Steam Frame headset presence and runtime depth/motion receipt.

This checks app logs plus Valve's SteamVR performance-recording summary. It
cannot prove the runtime generated a particular intermediate image; the
in-headset depth/motion texture view and wearer observation remain required.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import sys
from pathlib import Path


def parse_openxr_log(path: Path) -> dict:
    diag, layers, synthesis_content, hands = [], [], [], []
    for line in path.read_text(errors="replace").splitlines():
        for marker, target in (("FRAME_DIAG ", diag), ("FRAME_LAYERS ", layers),
                               ("FRAME_SYNTHESIS_CONTENT ", synthesis_content), ("FRAME_HAND ", hands)):
            if marker in line:
                payload = line.split(marker, 1)[1]
                target.append(dict(re.findall(r"([A-Za-z_]+)=([^\s]+)", payload)))
                break

    present = [x for x in diag if x.get("present") == "1" and x.get("head") == "1"]
    synthesis = [x for x in layers if x.get("projection") == "1" and x.get("stereo_views") == "2"
                 and x.get("synthesis") == "1" and x.get("held") == "0"]
    submitted_content = [x for x in synthesis_content
                         if x.get("fresh") == "1/1" and x.get("submitted") == "1"
                         and x.get("app_delta_shared") == "1"]
    valid_hands = [x for x in hands if x.get("valid") == "1"]
    return {
        "frame_diag_samples": len(diag),
        "present_with_valid_head_samples": len(present),
        "frame_layer_samples": len(layers),
        "fresh_synthesis_layer_samples": len(synthesis),
        "synthesis_content_samples": len(synthesis_content),
        "fresh_stereo_synthesis_content_submitted": len(submitted_content),
        "valid_controller_pose_samples": len(valid_hands),
        "valid_controller_sides": sorted({x.get("side") for x in valid_hands if x.get("side")}),
        "session_states": sorted({x.get("state", "unknown") for x in diag}),
    }


def _key(value: str) -> str:
    return re.sub(r"[^a-z0-9]", "", value.casefold().lstrip("#"))


def parse_recording(path: Path) -> dict:
    """Read SteamVR's CSV summary whether it is key/value or a summary table."""
    expected = {"appid", "appname", "totalframes", "frameswithdepth",
                "frameswithmotionvectors", "reprojectedframes"}
    summary: dict[str, str] = {}
    with path.open(newline="", encoding="utf-8-sig", errors="replace") as stream:
        rows = list(csv.reader(stream))
    for index, row in enumerate(rows):
        clean = [cell.strip() for cell in row]
        normalized = [_key(cell) for cell in clean]
        if len(clean) == 2 and normalized[0] in expected:
            summary[normalized[0]] = clean[1]
            continue
        if len(clean) > 2 and expected.intersection(normalized) and index + 1 < len(rows):
            values = [cell.strip() for cell in rows[index + 1]]
            for key, value in zip(normalized, values):
                if key in expected and value:
                    summary[key] = value
    metrics = {}
    for name in expected:
        raw = summary.get(name)
        if raw is not None:
            try:
                metrics[name] = int(float(raw))
            except ValueError:
                metrics[name] = raw
    refresh_values = set()
    for index, row in enumerate(rows):
        normalized = [_key(cell.strip()) for cell in row]
        if "refreshhz" in normalized and index + 1 < len(rows):
            for column, name in enumerate(normalized):
                if name == "refreshhz" and column < len(rows[index + 1]):
                    try:
                        refresh_values.add(float(rows[index + 1][column].strip()))
                    except ValueError:
                        pass
        elif len(row) == 2 and normalized[0] == "refreshhz":
            try:
                refresh_values.add(float(row[1].strip()))
            except ValueError:
                pass
    metrics["observed_refresh_hz"] = sorted(refresh_values)
    return metrics


def validate(openxr: dict, recording: dict, args) -> dict:
    checks = {
        "headset_present_and_head_pose_valid": openxr["present_with_valid_head_samples"] >= args.min_present_samples,
        "fresh_stereo_synthesis_content_submitted": (
            openxr["fresh_stereo_synthesis_content_submitted"] >= args.min_synthesis_samples
        ),
        "runtime_recording_contains_frames": recording.get("totalframes", 0) > 0,
        "runtime_reported_depth_frames": recording.get("frameswithdepth", 0) > 0,
        "runtime_reported_motion_vector_frames": recording.get("frameswithmotionvectors", 0) > 0,
    }
    if args.require_controllers:
        checks["both_controller_poses_valid"] = (
            set(openxr["valid_controller_sides"]) == {"left", "right"}
            and openxr["valid_controller_pose_samples"] >= 2 * args.min_controller_samples
        )
    if args.require_focused:
        checks["openxr_session_focused"] = "FOCUSED" in openxr["session_states"]
    if args.expected_app:
        checks["runtime_recording_identifies_expected_app"] = bool(
            re.search(args.expected_app, str(recording.get("appname", "")))
        )
    return {
        "valid": all(checks.values()),
        "checks": checks,
        "openxr": openxr,
        "steamvr_recording": recording,
        "limitations": [
            "SteamVR reprojectedFrames is not proof of XR_EXT_frame_synthesis output.",
            "The headset depth/motion texture view and wearer-visible result still require manual inspection.",
            "This validates receipt and submission metadata, not motion-vector correctness or synthesized display frames.",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("openxr_log", type=Path)
    parser.add_argument("performance_csv", type=Path, help="SteamVR perfrecording-*.csv")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--min-present-samples", type=int, default=3)
    parser.add_argument("--min-synthesis-samples", type=int, default=3)
    parser.add_argument("--require-controllers", action="store_true")
    parser.add_argument("--require-focused", action="store_true",
                        help="require at least one OpenXR FOCUSED session sample")
    parser.add_argument("--min-controller-samples", type=int, default=1)
    parser.add_argument("--expected-app", default=r"(?i)(ironwail|quake|unleashed)")
    args = parser.parse_args()
    for path in (args.openxr_log, args.performance_csv):
        if not path.is_file():
            parser.error(f"file not found: {path}")
    result = validate(parse_openxr_log(args.openxr_log), parse_recording(args.performance_csv), args)
    rendered = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    sys.stdout.write(rendered)
    return 0 if result["valid"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
