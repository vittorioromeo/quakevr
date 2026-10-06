"""Quake VR's benchmark scenarios: scripts, summaries and comparisons (docs/vr-port/BENCHMARKS.md).

    python qvrbench.py list [--group G]                       the scenarios, their groups and what each stresses
    python qvrbench.py script <scenario> --out <cfg> [opts]  the scenario's console script (bench.sh execs it)
    python qvrbench.py summarize <results dir>              summary.csv / summary.md over the runs in it
    python qvrbench.py compare <baseline dir> <new dir>     per-scenario deltas, flagged past the noise
    python qvrbench.py validate <results dir>               each scenario ran, wrote its JSON, and set up the same
                                                            counts in every repeat (determinism)
    python qvrbench.py brief <run.json>                     one line of a run

A scenario is a fixed map and view (setpos and the mock head's look), a fixed random seed (vr_bench_seed before the
map and before the set-up, vr_particle_seed), a set-up of existing test commands (vr_physics_spawn, vr_physics_bigpile,
vr_light_test, vr_particle_test, ...), a warm-up, then a measured window of a fixed number of frames recorded by the
engine's vr_bench_begin (one JSON per run in <gamedir>/profile/bench/<tag>.json). The game's clock is fixed
(vr_fixed_frames: every frame the same game time), so the simulation is the same at any speed: bench.sh runs it paced
like a headset (-RealTime, host_maxfps = --hz) for timings, or unpaced (fast mode) for validation.
"""
import argparse
import csv
import json
import math
from pathlib import Path
import random
import statistics
import sys


def waits(n):
    return ["wait"] * int(n)


# The firing range: the spawn looks down the range (-x); the perf suite's fixtures (perf_suite.py) are round it.
RANGE = "vrfiringrange"
MIXED = ("monster_army", "monster_ogre", "monster_knight", "monster_wizard")

# Tours: waypoints of the custom maps (bsp_waypoints.py: the spawn, then pickups spread over the map; x y z yaw).
TOURS = {
    "warden": [(-2128, -2208, -2000, 30), (-258, -2110, -1488, 90), (-552, -1584, -1640, 1), (-1128, -1112, -232, 1),
               (-992, -1088, 24, 1), (-1824, -1536, 48, 0)],
    "apsp3": [(672, 2912, -240, 270), (-384, -896, -936, 0), (744, -820, -872, 3), (232, 1536, -1608, 3),
              (1392, -432, -136, 356), (424, -920, -488, 350)],
    "ad_grendel": [(-576, 0, 112, 360), (0, 1024, 152, 0), (-520, 568, 1000, 0), (720, -1608, 584, 0),
                   (-2320, -80, 248, 0), (-552, -272, 1152, 0)],
    "ad_soltower1e": [(0, 416, 176, 270), (0, 480, 1328, 0), (712, 280, 600, 0), (-728, 120, 856, 0),
                      (768, 8, 920, 0), (368, 96, 600, 0)],
    "basetohell": [(-16, 0, 56, 0), (-56, 128, 24, 0), (2272, -88, 88, 0), (1728, 960, -808, 0),
                   (1888, -144, 184, 0), (1424, -176, -1224, 0)],
    "vanisch01": [(448, -328, 104, 225), (732, -640, 64, 0), (916, -436, 74, 180), (1056, -1072, 104, 0),
                  (1268, -832, 444, 90), (112, -912, 408, 0)],
}
TOUR_SOURCES = {"warden": "author's checkout quakevr/maps (not in git)", "apsp3": "author's checkout quakevr/maps (not in git)",
                "ad_grendel": "Map Library package", "ad_soltower1e": "Map Library package",
                "basetohell": "Map Library package", "vanisch01": "Map Library package"}


class Scenario:
    def __init__(self, name, groups, stresses, map, purpose, pos=None, look=(0, 0), setup=(), body=None, warm=180,
                 base="qbase", flat=False, hostile=False, header=(), motion=None, load_wait=120):
        self.name, self.groups, self.stresses, self.map, self.purpose = name, groups, stresses, map, purpose
        self.pos, self.look, self.setup, self.body, self.warm = pos, look, list(setup), body, warm
        self.base, self.flat, self.hostile, self.header, self.motion = base, flat, hostile, list(header), motion
        self.load_wait = load_wait


def blasts_every(interval, cmd):
    """A body: `cmd(i, rng)` every `interval` frames, for the whole window."""
    def body(frames):
        rng = random.Random(7)
        out = []
        for i in range(math.ceil(frames / interval)):
            out += cmd(i, rng) + waits(interval)
        return out
    return body


def tour_body(points):
    """A body: each waypoint in turn, looking four ways (setpos's yaw, +90, +180, +270), an equal share of the frames."""
    def body(frames):
        views = [(p, k) for p in points for k in range(4)]
        share = max(1, frames // len(views))
        out = []
        for (x, y, z, yaw), k in views:
            out += [f"setpos {x} {y} {z} 0 {(yaw + 90 * k) % 360} 0"] + waits(share)
        return out
    return body


def spawn_grid(count, kinds, x0=180, dx=48, per_row=8, dy=48):
    return [f"vr_physics_spawn {kinds[i % len(kinds)]} {x0 + (i // per_row) * dx} {(i % per_row - (per_row - 1) / 2) * dy:g}"
            for i in range(count)]


def punch_motion(seconds):
    """vr_mock_play keyframes: both hands jabbing forward in turn (0.6 s a jab each, the off hand half a beat later)."""
    lines = []
    t = 0.0
    while t < seconds:
        for hand, x, dt in (("main", 0.2, 0.0), ("off", -0.2, 0.3)):
            lines += [f"{t + dt:.3f} {hand} {x} 1.25 -0.25 0 0 0", f"{t + dt + 0.12:.3f} {hand} {x} 1.35 -0.72 0 0 0",
                      f"{t + dt + 0.3:.3f} {hand} {x} 1.25 -0.25 0 0 0"]
        t += 0.6
    return "\n".join(lines) + "\n"


def scenarios():
    s = []
    add = lambda *a, **k: s.append(Scenario(*a, **k))
    # ---- idle: nothing going on
    add("idle_e1m1", ["idle", "core"], "baseline: an id map, monsters asleep, no effects", "e1m1",
        "The floor every other scenario adds to: E1M1's start, nothing moving (notarget), both eyes drawn.")
    add("idle_e1m1_flat", ["idle", "flat"], "baseline, flat (no headset): one 960x540 view", "e1m1",
        "The same view in flat mode (vr_enabled 0): what the desktop game costs, and the VR overhead by difference.",
        flat=True)
    add("idle_e1m1_qrp", ["idle", "textures"], "baseline with the QRP texture pack (high-res textures)", "e1m1",
        "idle_e1m1 on the QRP base: texture memory and sampling cost of the replacement pack.", base="qrp")
    add("idle_range", ["idle", "core"], "Quake VR's own populated map: props, boards, signs", RANGE,
        "The firing range at its spawn: Quake VR's content (props, text boards, decals) without action.")
    add("idle_start_flat", ["idle", "flat"], "the hub in flat mode", "start",
        "The start map's hall in flat mode (its slipgates in view), for the flat build's own baseline.", flat=True)
    # ---- slipgates
    add("slipgate_start", ["slipgates", "core"], "a slipgate's portal view: destination drawn per eye", "start",
        "Looking through the episode slipgate from its doorway: the destination scene, lights and entities per eye.",
        pos="544 1320 24 0 90 0")
    add("slipgate_start_off", ["slipgates", "control"], "control: the same view, slipgates off", "start",
        "slipgate_start with vr_slipgates 0: the portal's whole cost by difference.",
        pos="544 1320 24 0 90 0", setup=["vr_slipgates 0"])
    add("slipgate_start_flat", ["slipgates", "flat"], "a slipgate's portal view in flat mode", "start",
        "slipgate_start in flat mode.", pos="544 1320 24 0 90 0", flat=True)
    add("slipgate_ai_24", ["slipgates", "combat"], "24 grunts seeing and shooting through a slipgate (portal AI)",
        "start", "Monsters' one-hop portal perception and fire across the gate (the perf suite's portal_enemies).",
        pos="544 1320 24 0 90 0", hostile=True,
        setup=waits(3) + [f"vr_physics_spawn monster_army {320 + (i // 4) * 40} {(i % 4 - 1.5) * 24:g}" for i in range(24)]
        + waits(30) + ["vr_portals_ai_test 1"] + waits(2) + ["vr_portals_ai_test 12"], warm=90)
    # ---- combat
    add("combat_48", ["combat", "core"], "48 mixed monsters fighting you and each other", RANGE,
        "Grunts, ogres, knights and scrags against a god-mode player: AI, traces, missiles, gore, decals, sounds.",
        hostile=True, setup=spawn_grid(48, MIXED), warm=90)
    add("ai_crowd_64", ["combat", "server"], "64 monsters' AI and movement, models not drawn", RANGE,
        "The server's side alone: 64 awake monsters chasing and shooting (r_drawentities 0, looking up): AI, "
        "movetogoal, traces, QuakeC; the rendering left out.", hostile=True, look=(-60, 0),
        setup=spawn_grid(64, MIXED) + ["r_drawentities 0"], warm=90)
    add("combat_48_spectator", ["combat", "features"], "combat_48 with the spectator camera (a third view)", RANGE,
        "combat_48 with vr_window_view 2 (the recording camera drawn every frame): the spectator view's cost.",
        hostile=True, setup=spawn_grid(48, MIXED) + ["vr_window_view 2", "vr_spectator_rate 1"], warm=90)
    add("combat_48_bullettime", ["combat", "features"], "combat_48 in bullet time (slowed world, its effects)", RANGE,
        "combat_48 with bullet time running all through: the time scale, the slowed sounds and the colour pass.",
        hostile=True, setup=spawn_grid(48, MIXED) + ["vr_bullettime_duration 600", "vr_bullettime_recharge 0.1",
                                                     "vr_bullettime"], warm=90)
    add("melee_punch_8", ["combat", "melee"], "8 grunts at arm's length, both hands jabbing (melee)", RANGE,
        "Scripted jabs (vr_mock_play, no recorded takes needed) into a ring of grunts: the melee and hit systems, "
        "knockdowns, wounds.", hostile=True, setup=[f"vr_physics_spawn monster_army 40 {(i - 3.5) * 14:g}" for i in range(8)],
        motion=punch_motion, warm=30)
    # ---- physics
    add("props_500_active", ["physics", "core"], "500 mixed props blasted every second (Box3D awake)", RANGE,
        "vr_physics_bigpile mixed 500 with a blast every 90 frames: Box3D step, write-back, prop rendering.",
        setup=["vr_physics_bigpile mixed 500"], warm=10,
        body=blasts_every(90, lambda i, r: ["vr_physics_blast 130 -556 40 80"]))
    add("props_500_settled", ["physics"], "500 mixed props asleep", RANGE,
        "The same pile left to settle (900 frames): what sleeping bodies still cost.",
        setup=["vr_physics_bigpile mixed 500"], warm=900)
    add("ragdolls_32_active", ["physics", "core"], "32 ragdolls tossed by blasts", RANGE,
        "32 grunts killed by blasts (vr_ragdoll_max 32), then blasted every 90 frames: ragdoll joints, gore.",
        setup=["vr_ragdoll_max 32"] + spawn_grid(32, ("monster_army",), x0=100, dy=40) + waits(30)
        + [f"vr_physics_blast {216 - row * 48} {-556 - side} 40 60" for row in range(4) for side in (-120, -40, 40, 120)],
        warm=10, body=blasts_every(90, lambda i, r: ["vr_physics_blast 130 -556 40 20"]))
    add("ragdolls_32_settled", ["physics"], "32 ragdolls at rest", RANGE,
        "The same ragdolls left to settle.", setup=["vr_ragdoll_max 32"] + spawn_grid(32, ("monster_army",), x0=100, dy=40)
        + waits(30) + [f"vr_physics_blast {216 - row * 48} {-556 - side} 40 60" for row in range(4) for side in (-120, -40, 40, 120)],
        warm=900)
    # ---- effects
    add("particles_dense", ["vfx", "core"], "dense smoke/blood/big smoke 64 units ahead (overdraw)", RANGE,
        "80 particles of smoke, blood or big smoke every 4 frames plus small blasts: particle fill rate, the "
        "retro particle path.", body=blasts_every(4, lambda i, r: [
            f"vr_physics_blast {316 - r.uniform(90, 300):.0f} {-556 + r.uniform(-150, 150):.0f} 40 1",
            f"vr_particle_test {(4, 1, 11)[i % 3]} 80"]))
    add("explosions_3s", ["vfx"], "3 explosions a second with incandescent debris", RANGE,
        "vr_explosion_debris_test every 24 frames: explosion sprites, debris chunks and their lights.",
        body=blasts_every(24, lambda i, r: ["vr_explosion_debris_test normal"]))
    add("explosions_storm", ["vfx", "physics"], "18 explosions a second (debris, lights, shake)", RANGE,
        "vr_explosion_debris_test every 4 frames: the debris pool full, many short lights.",
        body=blasts_every(4, lambda i, r: ["vr_explosion_debris_test normal"]))
    rng = random.Random(11)
    shots = [f"vr_physics_fire 10 {316 - rng.uniform(60, 320):.0f} {-556 + rng.uniform(-200, 200):.0f} -40" for _ in range(1400)]
    add("decals_1024_stream", ["vfx", "core"], "1024 bullet marks kept, new shots streaming in", RANGE,
        "1400 pellets fill the 1024-mark pool, then a shot every 4 frames: decal insertion, the world decal grid.",
        setup=["vr_decal_max 1024", "vr_explosion_debris 0"] + [x for sh in shots for x in (sh, "wait")] + ["vr_decal_count"],
        warm=30, body=blasts_every(4, lambda i, r: [shots[i % len(shots)]]))
    add("decals_blood_4096", ["vfx"], "4096 large blood marks", RANGE,
        "vr_decal_stress fills a 4096-mark pool with large blood marks, then adds one every 4 frames.",
        setup=["vr_decal_max 4096", "vr_particles 0", "vr_decal_life 1200"] + [x for _ in range(96) for x in ("vr_decal_stress 64 128", "wait")]
        + ["vr_decal_count"], warm=30, body=blasts_every(4, lambda i, r: ["vr_decal_stress 1 128"]))
    add("torches_32", ["vfx", "lights"], "32 wall torches: fire particles and flickering lights", RANGE,
        "32 more wall torches (35 emitters): fire particles, their lights.",
        setup=["vr_walltorch 1", "vr_fire_particles 1"] + [f"vr_physics_spawn light_torch_small_walltorch {96 + (i // 8) * 48} {(i % 8 - 3.5) * 40:g}" for i in range(32)])
    # ---- lights
    add("lights_32", ["lights", "core"], "32 overlapping shadow-casting dynamic lights", RANGE,
        "vr_light_test x32 ahead: dynamic lights, their shadow maps (vr_shadow_dlights' slots), lit models.",
        setup=[f"vr_light_test 400 120 {48 + i * 12}" for i in range(32)])
    add("lights_32_noshadows", ["lights", "control"], "control: the same lights, shadows off", RANGE,
        "lights_32 with vr_shadow_dlights 0 and vr_shadow_maplights 0: the shadows' share.",
        setup=[f"vr_light_test 400 120 {48 + i * 12}" for i in range(32)] + ["vr_shadow_dlights 0", "vr_shadow_maplights 0"])
    add("flashlight_e1m1", ["lights", "features"], "the flashlight on, casting shadows, over monsters", "e1m1",
        "E1M1's start with the flashlight given and on (its spot light's shadow tile).",
        setup=["vr_flashlight 1", "vr_flashlight_shadows 1", "vr_flashlight_give", "wait", "vr_flashlight_toggle"])
    # ---- liquids and surfaces
    add("water_range_surface", ["liquids"], "a large water surface from above (refraction, warp)", RANGE,
        "Standing at the firing range's pool, looking down at the water.", pos="612 474 2 30 0 0")
    add("water_range_under", ["liquids"], "underwater (r_waterwarp, the underwater pass)", RANGE,
        "Under the firing range's pool (setpos 600 450 -150).", pos="600 450 -150 0 0 0")
    add("slime_e1m1", ["liquids"], "E1M1's slime pool", "e1m1", "In E1M1's slime pool (setpos 200 2820 -60).",
        pos="200 2820 -60 0 0 0")
    add("lava_e1m7", ["liquids"], "E1M7's lava, light from below", "e1m7", "Facing E1M7's lava (setpos -50 48 20).",
        pos="-50 48 20 0 0 0")
    add("parallax_e1m1_qrp", ["textures"], "parallax occlusion on QRP's walls at grazing angles", "e1m1",
        "QRP base, parallax steps 32, looking along a wall: the parallax shader's worst case.",
        base="qrp", look=(0, 60), setup=["vr_parallax 1", "vr_parallax_steps 32", "vr_normalmaps 1"])
    add("parallax_e1m1_qrp_off", ["textures", "control"], "control: the same view, parallax off", "e1m1",
        "parallax_e1m1_qrp with vr_parallax 0.", base="qrp", look=(0, 60), setup=["vr_parallax 0"])
    # ---- menus and loading
    add("menu_open_e1m1", ["features"], "the VR menu open over E1M1 (text panels)", "e1m1",
        "idle_e1m1 with the VR settings menu open: the 3D text and panels' cost.", setup=["menu_vr"], warm=60)
    add("mapload_e1m2", ["loading"], "a map change: E1M1 to E1M2 inside the window (the load hitch)", "e1m1",
        "The measured window spans `map e1m2`: the worst frame is the load; then E1M2's first seconds (precache, "
        "first-use hitches).", body=lambda frames: ["map e1m2"] + waits(frames), warm=60)
    add("timedemo_demo1_flat", ["loading", "flat"], "id's demo1 played as fast as it goes (timedemo), flat", "start",
        "The classic `timedemo demo1` in flat mode inside the window: a replayed game's frames (its own fps line too).",
        flat=True, body=lambda frames: ["timedemo demo1"] + waits(frames), warm=30)
    # ---- complex custom maps
    for m, pts in TOURS.items():
        add(f"tour_{m}", ["maps"], f"complex custom map: 6 places x 4 directions ({TOUR_SOURCES[m]})", m,
            f"{m}: the spawn and five pickups' places, each looked at four ways: heavy geometry, many lights and "
            "entities. Monsters asleep (notarget).", body=tour_body(pts), warm=60, load_wait=180)
    add("tour_warden_flat", ["maps", "flat"], "warden's tour in flat mode", "warden", "tour_warden, flat.",
        body=tour_body(TOURS["warden"]), warm=60, flat=True, load_wait=180)
    add("combined", ["combat", "physics", "vfx", "lights", "core"], "300 props + 24 grunts + 16 lights + dense particles",
        RANGE, "Everything at once (the perf suite's combined): the frame's worst realistic mix.", hostile=True,
        setup=["vr_physics_bigpile mixed 300"] + [f"vr_physics_spawn monster_army {160 + (i // 6) * 48} {(i % 6 - 2.5) * 40:g}" for i in range(24)]
        + [f"vr_light_test 400 120 {64 + i * 16}" for i in range(16)], warm=90,
        body=blasts_every(4, lambda i, r: [
            f"vr_physics_blast {316 - r.uniform(90, 300):.0f} {-556 + r.uniform(-150, 150):.0f} 40 1",
            f"vr_particle_test {(4, 1, 11)[i % 3]} 80"]))
    return {x.name: x for x in s}


def select(spec):
    """'all', a group, scenario names, or a comma list of either."""
    table = scenarios()
    out = []
    for item in spec.split(","):
        if item == "all":
            out += list(table)
        elif item in table:
            out.append(item)
        else:
            group = [n for n, x in table.items() if item in x.groups]
            if not group:
                raise SystemExit(f"qvrbench: no scenario or group {item!r} (list: python qvrbench.py list)")
            out += group
    return list(dict.fromkeys(out))


def script(sc, tag, frames, hz, eye, realtime, settings, motion_path, shot=False):
    lines = ["vr_enabled 0" if sc.flat else "vr_enabled 1", "vr_fixed_frames 1", f"vr_fixed_frames_rate {hz}",
             f"vr_mock_eye_size {eye}", "vid_vsync 0", f"host_maxfps {hz if realtime else 0}", "con_notifytime 0",
             "vr_tips 0", "developer 0", "sv_autosave 0", "vr_profile 0", "vr_particle_seed 7", "vr_mock_look 0 0"]
    if settings:
        lines += [f'exec "{settings}"']
    lines += sc.header + waits(10) + ["vr_bench_seed 7", f"map {sc.map}"] + waits(sc.load_wait)
    lines += ["god 1", "notarget 0" if sc.hostile else "notarget 1"]
    if sc.pos:
        lines += [f"setpos {sc.pos}"]
    lines += [f"vr_mock_look {sc.look[0]} {sc.look[1]}", "vr_bench_seed 7"] + sc.setup
    if sc.motion:
        lines += [f'vr_mock_play "{motion_path}"']
    lines += waits(sc.warm) + ["echo BENCH_SETUP_DONE", f"vr_bench_begin {tag} {frames}"]
    body = sc.body(frames) if sc.body else []
    lines += body + waits(max(0, frames - sum(1 for x in body if x == "wait")) + 20)
    # (the window has ended by itself by now: a screenshot here costs it nothing)
    lines += ["vr_bench_end"] + (["screenshot", "wait"] if shot else []) + ["echo BENCH_DONE", "disconnect"]
    lines += waits(5) + ["quit"]
    return "\n".join(lines) + "\n"


# ---- results

KEYS = [("frame_ms", "p50"), ("frame_ms", "p99"), ("cpu_busy_ms", "avg"), ("cpu_busy_ms", "p99"),
        ("gpu_eyes_ms", "avg"), ("gpu_eyes_ms", "p99"), ("gpu_3d_ms", "avg"), ("heap_allocs", "avg")]


def load_runs(root):
    runs = {}
    for path in sorted(Path(root).glob("*/r*.json")):
        try:
            runs.setdefault(path.parent.name, []).append(json.loads(path.read_text()))
        except json.JSONDecodeError as e:
            print(f"qvrbench: bad JSON {path}: {e}", file=sys.stderr)
    return runs


def aggregate(rs):
    """Median over the repeats of each statistic (the worst frame: the worst over them)."""
    out = {"runs": len(rs), "map": rs[0]["map"], "frames": rs[0]["frames"]}
    for series in ("frame_ms", "host_ms", "cpu_busy_ms", "gpu_eyes_ms", "gpu_3d_ms", "heap_allocs", "draw_calls",
                   "traces"):
        if series not in rs[0]["frame"]:
            continue
        for stat in ("avg", "p50", "p95", "p99"):
            out[f"{series}.{stat}"] = statistics.median(r["frame"][series][stat] for r in rs)
        out[f"{series}.max"] = max(r["frame"][series]["max"] for r in rs)
    for h in rs[0]["hitches"]:
        out[f"hitches.{h}"] = statistics.median(r["hitches"][h] for r in rs)
    for phase in rs[0]["gpu_phase_ms"]:
        out[f"gpu.{phase}"] = statistics.median(r["gpu_phase_ms"][phase] for r in rs)
    for phase in rs[0]["cpu_phase_ms"]:
        out[f"cpu.{phase}"] = statistics.median(r["cpu_phase_ms"][phase] for r in rs)
    for c in rs[0]["counts"]:
        out[f"count.{c}"] = statistics.median(r["counts"][c]["avg"] for r in rs)
    return out


def summarize(root):
    runs = load_runs(root)
    if not runs:
        raise SystemExit(f"qvrbench: no runs in {root}")
    rows = {name: aggregate(rs) for name, rs in runs.items()}
    fields = list(dict.fromkeys(k for r in rows.values() for k in r))
    with open(Path(root) / "summary.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["scenario"] + fields)
        for name, r in rows.items():
            w.writerow([name] + [r.get(k, "") for k in fields])
    head = ("scenario", "n", "frame p50", "frame p99", "frame max", "cpu avg", "cpu p99", "gpu eyes avg",
            "gpu eyes p99", "gpu 3D avg", ">2x med", "heap/fr")
    md = ["| " + " | ".join(head) + " |", "|" + "---|" * len(head)]
    for name, r in rows.items():
        md.append(f"| {name} | {r['runs']} | {r['frame_ms.p50']:.3f} | {r['frame_ms.p99']:.3f} | {r['frame_ms.max']:.2f} | "
                  f"{r['cpu_busy_ms.avg']:.3f} | {r['cpu_busy_ms.p99']:.3f} | {r['gpu_eyes_ms.avg']:.3f} | "
                  f"{r['gpu_eyes_ms.p99']:.3f} | {r.get('gpu_3d_ms.avg', 0):.3f} | {r['hitches.over_2x_median']:.0f} | "
                  f"{r['heap_allocs.avg']:.1f} |")
    (Path(root) / "summary.md").write_text("\n".join(md) + "\n")
    print("\n".join(md))


def compare(base, new, threshold):
    a, b = load_runs(base), load_runs(new)
    print(f"| scenario | metric | baseline | new | change |\n|---|---|---|---|---|")
    flagged = 0
    for name in sorted(set(a) & set(b)):
        ra, rb = aggregate(a[name]), aggregate(b[name])
        for series, stat in KEYS:
            k = f"{series}.{stat}"
            if k not in ra or k not in rb:
                continue
            x, y = ra[k], rb[k]
            pct = (y - x) / x * 100 if x else 0.0
            # Past the noise: the relative change and an absolute floor (sub-0.05 ms moves are timer noise).
            flag = abs(pct) >= threshold and abs(y - x) >= (0.05 if series.endswith("_ms") else 0.5)
            flagged += flag
            print(f"| {name} | {k} | {x:.3f} | {y:.3f} | {pct:+.1f}%{' **' if flag else ''} |")
    for name in sorted(set(a) ^ set(b)):
        print(f"| {name} | (only in {'baseline' if name in a else 'new'}) | | | |")
    print(f"\n{flagged} changes past {threshold}% (marked **)")


def validate(root):
    runs = load_runs(root)
    bad = 0
    for name, rs in sorted(runs.items()):
        starts = [tuple(sorted((c, v["start"]) for c, v in r["counts"].items()
                               if c in ("edicts", "monsters_alive", "box3d_bodies", "decals"))) for r in rs]
        same = all(s == starts[0] for s in starts)
        frames = [r["frame"]["frame_ms"]["n"] for r in rs]
        ok = same and all(f == rs[0]["frames"] for f in frames) and len(rs) >= 1
        bad += not ok
        print(f"{name:28} runs={len(rs)} frames={frames} start={dict(starts[0])} {'same' if same else 'DIFFERENT'}"
              f"{'' if ok else '  <- FAIL'}")
    print(f"validate: {len(runs)} scenarios, {bad} failing")
    return bad


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    l = sub.add_parser("list")
    l.add_argument("--group", default="all")
    l.add_argument("--names", action="store_true", help="names only, one a line")
    sp = sub.add_parser("script")
    sp.add_argument("scenario")
    sp.add_argument("--out", required=True)
    sp.add_argument("--tag")
    sp.add_argument("--frames", type=int, default=900)
    sp.add_argument("--hz", type=int, default=90)
    sp.add_argument("--eye", type=int, default=2048)
    sp.add_argument("--realtime", action="store_true")
    sp.add_argument("--settings", default="", help="a cfg exec'd before the map (a graphics profile)")
    sp.add_argument("--warm", type=int, help="override the scenario's warm-up frames")
    sp.add_argument("--info", action="store_true", help="print the scenario's base and map (for bench.sh)")
    sp.add_argument("--shot", action="store_true", help="a screenshot after the window (validation)")
    sm = sub.add_parser("summarize")
    sm.add_argument("root")
    c = sub.add_parser("compare")
    c.add_argument("base")
    c.add_argument("new")
    c.add_argument("--threshold", type=float, default=5.0)
    v = sub.add_parser("validate")
    v.add_argument("root")
    br = sub.add_parser("brief")
    br.add_argument("json")
    a = p.parse_args()
    if a.cmd == "list":
        table = scenarios()
        for n in select(a.group):
            x = table[n]
            print(n if a.names else f"{n:28} {x.base:5} {x.map:14} {'/'.join(x.groups):24} {x.stresses}")
    elif a.cmd == "script":
        sc = scenarios().get(a.scenario)
        if not sc:
            raise SystemExit(f"qvrbench: no scenario {a.scenario}")
        if a.warm is not None:
            sc.warm = a.warm
        out = Path(a.out)
        motion = out.with_suffix(".motion.txt")
        if sc.motion:
            motion.write_text(sc.motion(a.frames / a.hz + sc.warm / a.hz + 5))
        out.write_text(script(sc, a.tag or sc.name, a.frames, a.hz, a.eye, a.realtime, a.settings,
                              motion.as_posix(), a.shot))
        if a.info:
            print(f"{sc.base} {sc.map}")
    elif a.cmd == "summarize":
        summarize(a.root)
    elif a.cmd == "compare":
        compare(a.base, a.new, a.threshold)
    elif a.cmd == "brief":
        r = json.loads(Path(a.json).read_text())
        f, c, g = r["frame"]["frame_ms"], r["frame"]["cpu_busy_ms"], r["frame"]["gpu_3d_ms"]
        n = {k: v["start"] for k, v in r["counts"].items() if k in ("edicts", "monsters_alive", "box3d_bodies")}
        print(f"{r['frames']} frames on {r['map']}: frame p50 {f['p50']:.3f} p99 {f['p99']:.3f} max {f['max']:.2f}, "
              f"cpu {c['avg']:.3f}, gpu 3D {g['avg']:.3f} ms; start {n}")
    elif a.cmd == "validate":
        sys.exit(1 if validate(a.root) else 0)


if __name__ == "__main__":
    main()
