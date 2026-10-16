#!/usr/bin/env python3
"""Check the controlled crate-motion replay and report the independent HMD gate."""
from __future__ import annotations

import argparse
import json
import math
import re
import sys
from pathlib import Path


def read_profile(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def parse_positions(console: str) -> tuple[tuple[float, float, float], list[tuple[float, float, float]], int]:
    spawn = re.search(r"vr_physics_spawn:\s+(\d+) misc_explobox at\s+([\d.+-]+)\s+([\d.+-]+)\s+([\d.+-]+)", console)
    if not spawn:
        raise ValueError("missing misc_explobox spawn line")
    entity = int(spawn.group(1))
    initial = tuple(float(spawn.group(i)) for i in (2, 3, 4))
    flings = re.findall(rf"vr_physics_fling:\s+{entity} misc_explobox at\s+[\d.+-]+\s+[\d.+-]+\s+[\d.+-]+", console)
    if not flings:
        raise ValueError("the crate was not flung during the benchmark")
    samples = []
    for line in console.splitlines():
        match = re.search(rf"^\s+{entity} misc_explobox\s+([\d.+-]+)\s+([\d.+-]+)\s+([\d.+-]+)\s+angles\b", line)
        if match:
            samples.append(tuple(float(match.group(i)) for i in (1, 2, 3)))
    if len(samples) < 3:
        raise ValueError("need at least three crate position samples to verify repeated motion")
    return initial, samples, len(flings)


def parse_openxr(path: Path) -> dict:
    submissions = 0
    tracked_samples = 0
    hand_samples: dict[str, int] = {"left": 0, "right": 0}
    for line in path.read_text(errors="replace").splitlines():
        if "FRAME_SYNTHESIS_CONTENT " in line:
            fields = dict(re.findall(r"([A-Za-z_]+)=([^\s]+)", line.split("FRAME_SYNTHESIS_CONTENT ", 1)[1]))
            submissions += fields.get("fresh") == "1/1" and fields.get("submitted") == "1" and fields.get("app_delta_shared") == "1"
        elif "FRAME_DIAG " in line:
            fields = dict(re.findall(r"([A-Za-z_]+)=([^\s]+)", line.split("FRAME_DIAG ", 1)[1]))
            tracked_samples += fields.get("present") == "1" and fields.get("head") == "1"
        elif "FRAME_HAND " in line:
            fields = dict(re.findall(r"([A-Za-z_]+)=([^\s]+)", line.split("FRAME_HAND ", 1)[1]))
            if fields.get("valid") == "1" and fields.get("side") in hand_samples:
                hand_samples[fields["side"]] += 1
    return {"fresh_stereo_submissions": submissions, "tracked_head_samples": tracked_samples,
            "valid_controller_samples": hand_samples}


def validate(profile: dict, console: str, openxr: dict, args) -> dict:
    initial, samples, impulse_count = parse_positions(console)
    sample_displacements = [math.dist(a, b) for a, b in zip(samples, samples[1:])]
    excursion = max((math.dist(initial, sample) for sample in samples), default=0.0)
    horizontal_excursion = max((math.dist(initial[:2], sample[:2]) for sample in samples), default=0.0)
    height_span = max((sample[2] for sample in samples), default=initial[2]) - min(
        (sample[2] for sample in samples), default=initial[2])
    moving_intervals = sum(distance >= args.min_sample_motion for distance in sample_displacements)
    expected_impulses = math.ceil(args.frames / args.motion_interval)
    counts = profile.get("counts", {})
    checks = {
        "exact_benchmark_frames": profile.get("frames") == args.frames,
        "crate_excursion_exceeds_threshold": excursion >= args.min_displacement,
        "crate_stays_in_test_lane": horizontal_excursion <= args.max_horizontal_excursion,
        "crate_stays_near_floor_height": height_span <= args.max_height_span,
        "expected_motion_impulse_count": impulse_count == expected_impulses,
        "position_samples_cover_motion_window": len(samples) >= expected_impulses + 1,
        "crate_moves_in_repeated_windows": moving_intervals >= args.min_moving_intervals,
        "no_live_monsters": counts.get("monsters_alive", {}).get("max") == 0,
        "no_vr_particles": counts.get("vr_particles", {}).get("max") == 0,
        "no_decals": counts.get("decals", {}).get("max") == 0,
        "synthesis_submission_matches_mode": (
            openxr["fresh_stereo_submissions"] >= args.min_submissions
            if args.expect_synthesis
            else openxr["fresh_stereo_submissions"] == 0
        ),
    }
    tracked = openxr["tracked_head_samples"] >= args.min_tracked_samples
    controllers = all(n >= args.min_controller_samples for n in openxr["valid_controller_samples"].values())
    return {
        "app_valid": all(checks.values()),
        "wearer_tracking_observed": tracked,
        "controllers_observed": controllers,
        "checks": checks,
        "motion": {"initial": initial, "samples": samples, "sample_displacements": sample_displacements,
                   "maximum_excursion": excursion, "horizontal_excursion": horizontal_excursion,
                   "height_span": height_span, "moving_intervals": moving_intervals,
                   "impulses": impulse_count, "expected_impulses": expected_impulses},
        "openxr": openxr,
        "expected_synthesis": args.expect_synthesis,
        "profile": {"map": profile.get("map"), "frames": profile.get("frames"),
                    "seconds": profile.get("seconds"), "frame_ms": profile.get("frame", {}).get("frame_ms"),
                    "gpu_eyes_ms": profile.get("frame", {}).get("gpu_eyes_ms")},
        "limitations": [
            "App-side xrEndFrame submission is not proof SteamVR consumed the buffers.",
            "Tracked HMD/controller samples and SteamVR depth/motion recording are separate wearer gates.",
            "Repeated crate impulses and sampled positions verify motion in multiple windows, not every rendered frame.",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profile", type=Path)
    parser.add_argument("console_log", type=Path)
    parser.add_argument("openxr_log", type=Path)
    parser.add_argument("--frames", type=int, default=180)
    parser.add_argument("--min-displacement", type=float, default=12.0)
    parser.add_argument("--min-sample-motion", type=float, default=5.0)
    parser.add_argument("--max-horizontal-excursion", type=float, default=96.0)
    parser.add_argument("--max-height-span", type=float, default=20.0)
    parser.add_argument("--motion-interval", type=int, default=30)
    parser.add_argument("--min-moving-intervals", type=int, default=2)
    parser.add_argument("--min-submissions", type=int, default=3)
    parser.add_argument("--expect-synthesis", type=int, choices=(0, 1), default=1)
    parser.add_argument("--min-tracked-samples", type=int, default=3)
    parser.add_argument("--min-controller-samples", type=int, default=1)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.motion_interval < 1:
        parser.error("--motion-interval must be at least one frame")
    for path in (args.profile, args.console_log, args.openxr_log):
        if not path.is_file():
            parser.error(f"file not found: {path}")
    try:
        args.expect_synthesis = bool(args.expect_synthesis)
        result = validate(read_profile(args.profile), args.console_log.read_text(errors="replace"),
                          parse_openxr(args.openxr_log), args)
    except (ValueError, json.JSONDecodeError) as error:
        result = {"app_valid": False, "wearer_tracking_observed": False, "error": str(error)}
    rendered = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    sys.stdout.write(rendered)
    return 0 if result.get("app_valid") else 1


if __name__ == "__main__":
    raise SystemExit(main())
