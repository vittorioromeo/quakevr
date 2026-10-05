"""Repeatable stereo CPU/GPU stress suite. Run only against a disposable game base."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import shutil
import subprocess
import time


def waits(n):
    return ["wait"] * n


def scene(name, frames):
    setup, warm, body = [], 180, waits(frames)
    if name.startswith("portal"):
        setup = ["setpos 544 1320 24 0 90 0", "vr_mock_look 0 0"]
        if name == "portal_off":
            setup += ["vr_slipgates 0"]
        if "one" in name:
            setup += ["vr_portals_maxviews 1"]
        if "enemies" in name:
            setup += waits(3)
            setup += [f"vr_physics_spawn monster_army {320 + (i // 4) * 40} {(i % 4 - 1.5) * 24}" for i in range(24)]
            setup += waits(30) + ["notarget 0", "vr_portals_ai_test 1"] + waits(2)
            if "ai_off" in name:
                setup += ["vr_portals_ai 0"]
            setup += ["vr_portals_ai_test 12"]
    elif name.startswith("props"):
        count = int(name.rsplit("_", 1)[-1]) if name.rsplit("_", 1)[-1].isdigit() else 500
        setup = [f"vr_physics_bigpile mixed {count}"]
        warm = 10 if "active" in name else 900
        if "single" in name:
            setup += ["vr_box3d_threads 0"]
        if "active" in name:
            body = []
            for _ in range(frames // 90):
                body += ["vr_physics_blast 130 -556 40 80"] + waits(90)
    elif name.startswith("enemies"):
        setup = ["notarget 0"]
        types = ("monster_army", "monster_ogre", "monster_knight", "monster_wizard")
        for i in range(48):
            setup += [f"vr_physics_spawn {types[i % 4]} {180 + (i // 8) * 48} {(i % 8 - 3.5) * 48}"]
        if "ai_off" in name:
            setup += ["vr_portals_ai 0"]
        if "no_decals" in name:
            setup += ["vr_decals 0"]
        if "fine" in name:
            setup += ["vr_profile_detail 2"]
        if "hitch" in name:
            setup += ["vr_profile_hitch 1"]
        warm = 90
    elif name.startswith("ragdolls"):
        setup = ["vr_ragdoll_max 32"]
        for i in range(32):
            setup += [f"vr_physics_spawn monster_army {100 + (i // 8) * 48} {(i % 8 - 3.5) * 40}"]
        setup += waits(30)
        for row in range(4):
            for side in (-120, -40, 40, 120):
                setup += [f"vr_physics_blast {316 - 100 - row * 48} {-556 - side} 40 60"]
        warm = 10 if "active" in name else 900
        if "active" in name:
            body = []
            for _ in range(frames // 90):
                body += ["vr_physics_blast 130 -556 40 20"] + waits(90)
    elif name.startswith("fire"):
        setup += ["vr_walltorch 1"]
        setup += [f"vr_physics_spawn light_torch_small_walltorch {96 + (i // 8) * 48} {(i % 8 - 3.5) * 40}" for i in range(32)]
        setup += ["vr_fire_particles 0" if "off" in name else "vr_fire_particles 1"]
    elif name.startswith(("particles", "explosions")) or name == "combined":
        if name == "particles_off":
            setup += ["vr_particles 0", "vr_explosion_debris 0"]
        if "half" in name or "full_nonretro" in name:
            setup += ["vr_retro_particles 0"]
        if "half" in name:
            setup += ["vr_particle_halfres 1"]
        if "full_nonretro" in name:
            setup += ["vr_particle_halfres 0"]
        if "palette_off" in name:
            setup += ["vr_retro_particles_palette 0"]
        if "hard" in name:
            setup += ["vr_retro_particles_soft 0"]
        if "snap_off" in name:
            setup += ["vr_retro_particles_snap 0"]
        if "no_debris" in name:
            setup += ["vr_explosion_debris 0"]
        if "count4" in name:
            setup += ["vr_explosion_particles 4"]
        body = []
        rng = random.Random(7)
        interval = 24 if name.startswith("explosions") and "storm" not in name else 4
        for i in range(frames // interval):
            x, y = 316 - rng.uniform(90, 300), -556 + rng.uniform(-150, 150)
            body += ["vr_explosion_debris_test normal" if name.startswith("explosions") else f"vr_physics_blast {x:.0f} {y:.0f} 40 1"]
            if not name.startswith("explosions"):
                body += [f"vr_particle_test {(4, 1, 11)[i % 3]} 80"]
            body += waits(interval)
    elif name.startswith("decals"):
        limit = 4096 if "4096" in name else 1024
        setup += [f"vr_decal_max {limit}", "vr_explosion_debris 0"]
        if "nonretro" in name:
            setup += ["vr_retro_decals 0"]
        if "mesh" in name:
            setup += ["vr_decals_world 0"]
        if "off" in name:
            setup += ["vr_decals 0"]
        rng = random.Random(11)
        if "blood" in name:
            setup += ["vr_particles 0", "vr_decal_life 1200"]
            for _ in range(96):
                setup += ["vr_decal_stress 64 128", "wait"]
        else:
            for _ in range(5800 if limit == 4096 else 1400):
                x, y = 316 - rng.uniform(60, 320), -556 + rng.uniform(-200, 200)
                setup += [f"vr_physics_fire 10 {x:.0f} {y:.0f} -40", "wait"]
        setup += ["vr_decal_count"]
        if "stream" in name:
            setup += ["vr_particles 0"]
            body = []
            for _ in range(frames // 4):
                x, y = 316 - rng.uniform(60, 320), -556 + rng.uniform(-200, 200)
                body += (["vr_decal_stress 1 128"] if "blood" in name else [f"vr_physics_fire 10 {x:.0f} {y:.0f} -40"]) + waits(4)
    elif name.startswith("lights"):
        setup += [f"vr_light_test 400 120 {48 + i * 12}" for i in range(32)]
        if "no_shadows" in name:
            setup += ["vr_shadow_dlights 0", "vr_shadow_maplights 0"]
    if name == "combined":
        setup += ["vr_physics_bigpile mixed 300"]
        setup += [f"vr_physics_spawn monster_army {160 + (i // 6) * 48} {(i % 6 - 2.5) * 40}" for i in range(24)]
        setup += ["notarget 0"]
        setup += [f"vr_light_test 400 120 {64 + i * 16}" for i in range(16)]
    return setup, warm, body


def script(name, frames, eye, gpu, screenshot):
    commands = ["vr_backend mock", "vr_enabled 1", "vr_mock_fast 1", "vr_fixed_frames 1",
                "vr_fixed_frames_rate 72", f"vr_mock_eye_size {eye}", "vid_vsync 0", "host_maxfps 0",
                "con_notifytime 0", "sv_autosave 0", "vr_tips 0", "developer 0", "vr_mirror 0", "vr_render_scale 1",
                "vr_particle_seed 7", "vr_profile_interval 0", "vr_profile_detail 1",
                f"vr_profile_gpu {gpu}", "vr_profile_hitch 0", "vr_roomscale_move_mult 0"]
    commands += waits(80) + [f"map {'start' if name.startswith('portal') else 'vrfiringrange'}"] + waits(150)
    commands += ["god 1", "notarget 1", "vr_slipgates 1", "vr_portals 1", "vr_portals_walk 1",
                 "vr_portals_ai 1", "vr_particles 1", "vr_explosion_debris 1", "vr_decals 1"]
    setup, warm, body = scene(name, frames)
    commands += setup + ["vr_profile 1", "vr_profile_csv 1"] + waits(warm)
    commands += ["echo BENCH_WARM_END", "vr_profile_dump", "vr_physics_frametime warm"]
    commands += ["echo BENCH_MEASURE_BEGIN"] + body
    commands += ["echo BENCH_MEASURE_END", "vr_profile_report", "vr_profile_dump",
                 "vr_physics_frametime measured", "vr_decal_count", "vr_explosion_debris_stats", "vr_fire_particles_stats", "vr_ragdoll_list",
                 "vr_profile_csv 0", "vr_profile 0"]
    if screenshot:
        commands += ["vr_mirror 1", "wait", "wait", "screenshot"]
    commands += ["echo BENCH_DONE", "disconnect"] + waits(5) + ["quit"]
    return "\n".join(commands) + "\n"


def run(args):
    allowed = (
        'combined', 'decals', 'decals_off',
        'decals_stream', 'decals_stream_off', 'enemies',
        'enemies_fine', 'enemies_fine_hitch', 'enemies_no_decals',
        'explosions', 'explosions_count4', 'explosions_no_debris',
        'explosions_storm', 'explosions_storm_no_debris', 'fire',
        'fire_off', 'idle', 'lights',
        'lights_no_shadows', 'particles', 'particles_full_nonretro',
        'particles_half', 'particles_no_debris', 'particles_off',
        'portal', 'portal_enemies', 'portal_enemies_ai_off',
        'portal_off', 'portal_one', 'props_active',
        'props_active_single', 'props_settled', 'ragdolls_active',
        'ragdolls_active_audit', 'ragdolls_settled',
    )
    allowed += (
        'particles_palette_off', 'particles_hard', 'particles_hard_palette_off',
        'particles_snap_off', 'decals_4096', 'decals_4096_stream',
        'decals_4096_stream_nonretro', 'decals_4096_stream_mesh',
    )
    allowed += (
        'props_active_100', 'props_active_1000', 'props_settled_100',
        'props_settled_1000', 'decals_blood_4096', 'decals_blood_4096_stream',
        'decals_blood_4096_stream_nonretro', 'decals_blood_4096_stream_mesh',
    )
    if any(name not in allowed for name in args.scenes.split(",")):
        raise ValueError("Unknown scene; supported: " + ",".join(allowed))
    base, output, exe = map(lambda p: Path(p).resolve(), (args.base, args.output, args.exe))
    if "build-cmake" not in base.parts:
        raise ValueError("Use a disposable base under build-cmake, never the user's game directory")
    output.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, QVR_TEST_HIDDEN="1", QVR_TEST_BACKGROUND="1", QVR_NO_ERROR_DIALOG="1")
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    for rep in range(args.reps):
        names = args.scenes.split(",")
        # Interleave reversed order on alternate repeats to reduce drift bias.
        if rep % 2:
            names.reverse()
        for name in names:
            tag = f"{name}_eye{args.eye}_gpu{args.gpu}_r{rep + 1}"
            destination = output / tag
            if (destination / "metadata.json").exists():
                print(f"skip {tag}", flush=True)
                continue
            destination.mkdir(exist_ok=True)
            config = script(name, args.frames, args.eye, args.gpu, rep == 0)
            (base / "quakevr/autoexec.cfg").write_text(config)
            (destination / "autoexec.cfg").write_text(config)
            before = {p: (p.stat().st_mtime_ns, p.stat().st_size) for folder in ("profile", "screenshots")
                      for p in (base / "quakevr" / folder).glob("*") if p.is_file()}
            start = time.monotonic()
            process = subprocess.Popen([str(exe), "-basedir", str(base), "-game", "hipnotic", "-game", "rogue",
                                        "-game", "quakevr", "-condebug", "-window", "-width", "960", "-height", "540",
                                        "-nosound", "-noconfigwrite", "-noaddons"], cwd=base, env=env, startupinfo=startup)
            try:
                code = process.wait(timeout=180)
            except subprocess.TimeoutExpired:
                process.terminate()
                process.wait()
                raise RuntimeError(f"Benchmark timed out: {tag}")
            shutil.copy2(base / "qconsole.log", destination / "qconsole.log")
            for folder in ("profile", "screenshots"):
                for p in (base / "quakevr" / folder).glob("*"):
                    if p.is_file() and before.get(p) != (p.stat().st_mtime_ns, p.stat().st_size):
                        shutil.copy2(p, destination / p.name)
            log = (destination / "qconsole.log").read_text(errors="replace")
            if code or "BENCH_DONE" not in log or "Host_Error" in log or "Sys_Error" in log:
                raise RuntimeError(f"Benchmark failed: {tag}, exit {code}")
            metadata = {"scene": name, "eye": args.eye, "gpu_every": args.gpu, "rep": rep + 1,
                        "frames_requested": args.frames, "wall_seconds": time.monotonic() - start,
                        "exe_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
                        "progs_sha256": hashlib.sha256((base / "quakevr/progs.dat").read_bytes()).hexdigest()}
            (destination / "metadata.json").write_text(json.dumps(metadata, indent=2))
            print(f"done {tag}: {metadata['wall_seconds']:.1f}s", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True)
    parser.add_argument("--exe", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--scenes", default="idle,portal_off,portal,props_active,props_settled,enemies,ragdolls_active,ragdolls_settled,particles,decals,lights,combined")
    parser.add_argument("--reps", type=int, default=3)
    parser.add_argument("--eye", type=int, default=2048)
    parser.add_argument("--gpu", type=int, default=16)
    parser.add_argument("--frames", type=int, default=900)
    args = parser.parse_args()
    if args.reps < 1 or args.frames < 90 or args.eye < 1 or args.gpu < 0:
        parser.error("reps/eye must be positive, frames at least 90, and gpu nonnegative")
    run(args)
