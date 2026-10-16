#!/usr/bin/env python3
"""Build a fixed-window Quake config that profiles playback of a recorded demo."""
from __future__ import annotations

import argparse
import re
from pathlib import Path


def build(tag: str, frames: int, hz: int, demo: str, demo_map: str) -> list[str]:
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", tag):
        raise ValueError("benchmark tag contains unsupported characters")
    if not re.fullmatch(r"[A-Za-z0-9_-]+", demo):
        raise ValueError("demo must be a basename using letters, numbers, _ or -")
    if not re.fullmatch(r"[A-Za-z0-9_-]+", demo_map):
        raise ValueError("map must be a basename using letters, numbers, _ or -")
    if frames < 1 or hz < 1:
        raise ValueError("frames and Hz must be positive")
    return [
        "vr_enabled 1", "vr_fixed_frames 1", f"vr_fixed_frames_rate {hz}",
        "vid_vsync 0", f"host_maxfps {hz}", "con_notifytime 0", "vr_tips 0",
        "developer 0", "sv_autosave 0", "vr_profile 0", "vr_particle_seed 7",
        f"map {demo_map}", *(["wait"] * 120), "vr_ao_finish", "echo BENCH_SETUP_DONE",
        f"vr_bench_begin {tag} {frames}", f"playdemo {demo}",
        *(["wait"] * (frames + 20)), "vr_bench_end", "echo BENCH_DONE", "quit",
    ]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--frames", type=int, required=True)
    parser.add_argument("--hz", type=int, default=72)
    parser.add_argument("--demo", required=True)
    parser.add_argument("--map", dest="demo_map", default="e1m1")
    args = parser.parse_args()
    try:
        commands = build(args.tag, args.frames, args.hz, args.demo, args.demo_map)
    except ValueError as exc:
        parser.error(str(exc))
    args.out.write_text("\n".join(commands) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
