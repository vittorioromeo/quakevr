#!/usr/bin/env python3
"""Validate one opt-in stereo producer capture before using it as an oracle."""

from __future__ import annotations

import argparse
import json
import math
import struct
from pathlib import Path
from typing import Any


def check_png(path: Path, width: int, height: int) -> bool:
    try:
        data = path.read_bytes()
    except OSError:
        return False
    return (len(data) >= 24 and data[:8] == b"\x89PNG\r\n\x1a\n" and
            struct.unpack(">II", data[16:24]) == (width, height))


def check_float_file(path: Path, width: int, height: int, channels: int) -> dict[str, Any]:
    expected = width * height * channels * 4
    try:
        data = path.read_bytes()
    except OSError as exc:
        return {"exists": False, "error": str(exc)}
    if len(data) != expected:
        return {"exists": True, "size_bytes": len(data), "expected_bytes": expected,
                "size_matches": False, "all_finite": False}
    values = (item[0] for item in struct.iter_unpack("<f", data))
    minimum, maximum, finite = math.inf, -math.inf, 0
    total = 0
    for value in values:
        total += 1
        if math.isfinite(value):
            finite += 1
            minimum = min(minimum, value)
            maximum = max(maximum, value)
    return {"exists": True, "size_bytes": len(data), "expected_bytes": expected,
            "size_matches": True, "float_count": total, "finite_count": finite,
            "all_finite": finite == total, "minimum": minimum if finite else None,
            "maximum": maximum if finite else None}


def validate(directory: Path) -> dict[str, Any]:
    sidecars = sorted(directory.glob("*.json"))
    frames: list[dict[str, Any]] = []
    for sidecar in sidecars:
        data = json.loads(sidecar.read_text(encoding="utf-8"))
        synth_width, synth_height = data["synthesis_size"]
        out_width, out_height = data["export_size"]
        files = data["files"]
        row = {"sidecar": sidecar.name, "frame_id": data.get("frame_id"), "eye": data.get("eye"),
               "performance_valid_is_false": data.get("performance_valid") is False,
               "raw_float_encoding_is_explicit": data.get("raw_float_encoding") == "IEEE754 binary32 little-endian",
               "current_color_png": check_png(directory / Path(files["color"]).name, synth_width, synth_height),
               "previous_color_png": check_png(directory / Path(files["previous_color"]).name,
                                                synth_width, synth_height)}
        for key, name, width, height, channels in (
            ("depth", "depth", synth_width, synth_height, 1),
            ("camera_seed", "camera_seed", synth_width, synth_height, 4),
            ("flow", "flow", synth_width, synth_height, 4),
            ("export_motion", "export_motion", out_width, out_height, 4),
            ("export_depth", "export_depth", out_width, out_height, 1),
        ):
            row[key] = check_float_file(directory / Path(files[name]).name, width, height, channels)
        depth = row["depth"]
        row["depth_values_in_unit_range"] = bool(depth.get("all_finite") and
            depth.get("minimum", -1) >= -0.001 and depth.get("maximum", 2) <= 1.001)
        row["ui_depth_mismatch_present"] = (data.get("color_includes_ui") is True and
            data.get("depth_source") == "scene depth before postprocess and UI")
        frames.append(row)
    identities = {(row.get("frame_id"), row.get("eye")) for row in frames}
    checks = {
        "exactly_two_eye_sidecars": len(frames) == 2,
        "same_frame_id": len({row.get("frame_id") for row in frames}) == 1 and len(frames) == 2,
        "left_and_right_eye": {row.get("eye") for row in frames} == {"L", "R"},
        "capture_frames_excluded_from_performance": all(row["performance_valid_is_false"] for row in frames),
        "raw_float_encoding_is_explicit": all(row["raw_float_encoding_is_explicit"] for row in frames),
        "all_color_images_valid": all(row["current_color_png"] and row["previous_color_png"] for row in frames),
        "all_float_images_finite_and_sized": all(
            all(row[key].get("size_matches") and row[key].get("all_finite") for key in
                ("depth", "camera_seed", "flow", "export_motion", "export_depth")) for row in frames),
        "depth_in_unit_range": all(row["depth_values_in_unit_range"] for row in frames),
        "ui_depth_mismatch_is_explicit": all(row["ui_depth_mismatch_present"] for row in frames),
    }
    return {"valid": all(checks.values()), "checks": checks,
            "frame_ids_and_eyes": sorted([list(item) for item in identities]), "eyes": frames}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_dir", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = validate(args.capture_dir)
    encoded = json.dumps(report, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    return 0 if report["valid"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
