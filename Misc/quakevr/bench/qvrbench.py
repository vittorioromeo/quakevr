"""Quake VR's benchmark scenarios: scripts, summaries and comparisons (docs/vr-port/BENCHMARKS.md).

    python qvrbench.py list [--group G]                       the scenarios, their groups and what each stresses
    python qvrbench.py script <scenario> --out <cfg> [opts]  the scenario's console script (bench.sh execs it)
    python qvrbench.py summarize <results dir>              summary.csv / summary.md over the runs in it
    python qvrbench.py compare <baseline dir> <new dir>     per-scenario deltas, flagged past the noise
    python qvrbench.py validate <results dir>               each scenario ran, wrote its JSON, and set up the same
                                                            counts in every repeat (determinism)
    python qvrbench.py brief <run.json>                     one line of a run
    python qvrbench.py loads <results dir>                  the map loads by phase, the window parts (marks)

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
    """map None: no map before the window (a loading scenario's cold load is the process's first). frames: the
    window's own length, whatever --frames says (a fixed sequence of events; 0: as long as the body, vr_bench_end ending
    it). loads: the map loads its window must record (validate checks them)."""
    def __init__(self, name, groups, stresses, map, purpose, pos=None, look=(0, 0), setup=(), body=None, warm=180,
                 base="qbase", flat=False, hostile=False, header=(), motion=None, load_wait=120, frames=None, loads=0):
        self.name, self.groups, self.stresses, self.map, self.purpose = name, groups, stresses, map, purpose
        self.pos, self.look, self.setup, self.body, self.warm = pos, look, list(setup), body, warm
        self.base, self.flat, self.hostile, self.header, self.motion = base, flat, hostile, list(header), motion
        self.load_wait, self.frames, self.loads = load_wait, frames, loads


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


def events(*evs):
    """A body: at each (frame, label, commands) a vr_bench_mark of the label, then the commands; the frames between
    waited. The JSON's "marks" then give each part's worst and mean frame (the last part runs to the window's end)."""
    def body(frames):
        out, t = [], 0
        for at, label, cmds in evs:
            out += waits(at - t) + [f"vr_bench_mark {label}"] + list(cmds)
            t = at
        return out
    return body


def loads_body(cmds, each=150):
    """A body: each (label, command) a map load then `each` frames (the second after it and more); marked
    (vr_bench_mark), the new map's monsters kept still (notarget, once the player is there). The window is the body's
    (frames=0): a load's frames (the VR runtime's loading frames among them) are not known in advance. (Two frames
    first: the load is not in vr_bench_begin's own frame, which is not recorded.)"""
    def body(frames):
        out = waits(2)
        for label, cmd in cmds:
            out += [f"vr_bench_mark {label}", cmd] + waits(10) + ["god 1", "notarget 1"] + waits(each - 10)
        return out
    return body


# The rigged monsters by the firing range's dispenser number (vr_test_spawn; tarbaby 11 has no limbs).
RIGGED = [(0, "army"), (1, "ogre"), (2, "zombie"), (3, "shambler"), (4, "wizard"), (5, "knight"), (6, "hknight"),
          (7, "dog"), (8, "enforcer"), (9, "demon"), (10, "shalrath"), (12, "gremlin"), (13, "scourge"), (14, "mummy")]


def first_cuts_body(frames):
    """Each rigged kind spawned ahead (impulse 241), then killed by a slash at a limb (vr_limb_test 4) 25 frames later;
    then all again: marks spawn<round>_<kind> and cut<round>_<kind>, so the first cut of each kind (its limb models,
    its ragdoll's first use) shows as that part's worst frame, and the second round is the same cut warm."""
    out = []
    for rnd, dist in ((1, 90), (2, 150)):
        for i, (k, name) in enumerate(RIGGED):
            out += [f"vr_mock_look 0 {(i * 25 + (rnd - 1) * 12) % 360}", f"vr_test_spawn_dist {dist}", f"vr_test_spawn {k}",
                    f"vr_bench_mark spawn{rnd}_{name}", "impulse 241"] + waits(25)
            out += [f"vr_bench_mark cut{rnd}_{name}", "vr_limb_test 4"] + waits(35)
    return out


GORE = ["vr_ragdoll 1", "vr_gore_test_crowd 1024"]  # (the crowd tests: every monster at once, one line)
GRUNTS = ("monster_army",)
CROWD_MIX = ("monster_army", "monster_knight", "monster_enforcer", "monster_ogre")


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
    # ---- gore: crowds dismembered at once (the limb and head tests on every monster: vr_gore_test_crowd), the limbs
    # lying about, the first cut of each kind. Marks: before, the blow's frames, after (the parts falling).
    blow = lambda *cmds: events((0, "before", []), (10, "blow", cmds), (16, "after", []))
    add("gore_slash_32", ["gore", "core"], "32 grunts each killed by a slash at a limb in one frame", RANGE,
        "vr_limb_test 4 on 32 grunts at once (vr_gore_test_crowd): 32 limbs cut off, 32 ragdolls made, blood; then "
        "the bodies and limbs falling.", setup=GORE + ["vr_ragdoll_max 32", "vr_limbs_max 64"]
        + spawn_grid(32, GRUNTS, x0=100, dy=40), warm=60, body=blow("vr_limb_test 4"))
    add("gore_dismember_16", ["gore"], "16 grunt corpses cut apart at once (every limb and the head)", RANGE,
        "16 grunts killed by a slash at a limb in the set-up and left to settle; then vr_limb_test 3 on all of them in "
        "one frame (whole limbs, then the head: ~80 parts thrown).", setup=GORE + ["vr_ragdoll_max 32", "vr_limbs_max 128"]
        + spawn_grid(16, GRUNTS, x0=100, dy=48) + waits(30) + ["vr_limb_test 4"], warm=300, body=blow("vr_limb_test 3"))
    add("gore_blast_crowd_32", ["gore", "physics"], "4 rockets' blasts in a crowd of 32 (limbs popped)", RANGE,
        "32 grunts, knights, enforcers and ogres; four 120-damage explosions among them in one frame "
        "(vr_physics_blast, vr_limbs_blast 1): kills, limbs popped near each blast, gibs, ragdolls.",
        setup=GORE + ["vr_ragdoll_max 32", "vr_limbs_max 64", "vr_limbs_blast 1"] + spawn_grid(32, CROWD_MIX, x0=100, dy=40),
        warm=60, body=blow(*[f"vr_physics_blast {x} {y} 40 120" for x in (192, 96) for y in (-636, -476)]))
    for g, what in ((1, "their limbs besides Quake's gibs"), (2, "their limbs instead of the meat gibs"),
                    (0, "Quake's gibs only (control)")):
        add(f"gore_gib_limbs{g}_24", ["gore"] + (["control"] if g == 0 else []), f"24 grunts gibbed at once: {what}",
            RANGE, f"vr_limb_test 10 on 24 grunts in one frame with vr_gib_limbs {g}: {what}.",
            setup=GORE + [f"vr_gib_limbs {g}", "vr_limbs_max 128"] + spawn_grid(24, GRUNTS, x0=100, dy=40), warm=60,
            body=blow("vr_limb_test 10"))
    add("gore_headpop_24", ["gore"], "24 heads popped at once (super shotgun headshots)", RANGE,
        "vr_decap_test 14 on 24 grunts in one frame (a super shotgun blast at each head at health 1; vr_decap_pop_roll 0: "
        "every head pops): head gibs, brains, blood.", setup=GORE + ["vr_decap_pop_roll 0"]
        + spawn_grid(24, GRUNTS, x0=100, dy=40), warm=60, body=blow("vr_decap_test 14"))
    limbs64 = ["vr_ragdoll 1", "vr_limbs_max 64", "vr_physics_spawn monster_army 90 0"] + waits(20) + ["vr_limb_test 11"]
    add("gore_limbs_64", ["gore", "physics"], "64 cut-off limbs lying about (Most Limbs 64)", RANGE,
        "vr_limb_test 11 with vr_limbs_max 64: 128 limbs thrown, the 64 newest kept; left to land and settle: what "
        "many limbs lying about cost.", setup=limbs64, warm=400)
    add("gore_limbs_cap", ["gore", "physics"], "128 limbs thrown every half second over a cap of 64", RANGE,
        "gore_limbs_64, then vr_limb_test 11 every 45 frames: 128 more thrown each time, the oldest removed past "
        "Most Limbs (the cap's churn).", setup=limbs64, warm=120,
        body=blasts_every(45, lambda i, r: ["vr_limb_test 11"]))
    add("gore_first_cuts", ["gore"], "the first limb cut of each of 14 monster kinds, then each again (hitches)", RANGE,
        "Each rigged kind spawned ahead and slashed at a limb 25 frames later, twice round (marks spawn1_<kind>, "
        "cut1_<kind>, ... cut2_<kind>): the first cut's worst frame against the second's (limb models made, ragdoll "
        "first used: whatever the build does at the first cut).", setup=GORE[:1] + ["vr_gore_test_crowd 400",
        "vr_ragdoll_max 32", "vr_limbs_max 64"], warm=30, frames=0, body=first_cuts_body)
    # ---- lights
    add("lights_32", ["lights", "core"], "32 overlapping shadow-casting dynamic lights", RANGE,
        "vr_light_test x32 ahead: dynamic lights, their shadow maps (vr_shadow_dlights' slots), lit models.",
        setup=[f"vr_light_test 400 120 {48 + i * 12}" for i in range(32)])
    add("lights_32_noshadows", ["lights", "control"], "control: the same lights, shadows off", RANGE,
        "lights_32 with vr_shadow_dlights 0 and vr_shadow_maplights 0: the shadows' share.",
        setup=[f"vr_light_test 400 120 {48 + i * 12}" for i in range(32)] + ["vr_shadow_dlights 0", "vr_shadow_maplights 0"])
    add("flashlight_e1m1", ["lights", "features"], "the flashlight on, casting shadows, over monsters", "e1m1",
        "E1M1's start with the flashlight in the off hand, on and pointed ahead (its spot light's shadow tile: shadow_dlights 1).",
        setup=["vr_flashlight 1", "vr_flashlight_shadows 1", "vr_flashlight_give left", "wait", "vr_flashlight_toggle", "vr_mock_hand off -0.2 1.3 -0.35 0 0 0"])
    # ---- liquids and surfaces
    add("water_range_surface", ["liquids"], "a large water surface from above (refraction, warp)", RANGE,
        "Standing at the firing range's pool, looking down at the water.", pos="612 474 2 30 0 0")
    add("water_range_under", ["liquids"], "underwater (r_waterwarp, the underwater pass)", RANGE,
        "Under the firing range's pool (setpos 600 450 -150).", pos="600 450 -150 0 0 0")
    add("slime_e1m1", ["liquids"], "beside E1M1's slime pool", "e1m1",
        "At round 21's slime test spot (setpos 200 2820 -60: the pool and two grunts in view; not under the surface).",
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
    # Map loads (the JSON's "loads": each one's total from its command to its first frame drawn, its stages and its
    # kinds of work, the worst frame of the second after it). No map before the window: the first load is the process's
    # first of that map (cold: the engine's caches empty; the OS's file cache as it is); the second the same map again
    # (warm: the models and their skins kept); restart the server's own reload. Frames unpaced (host_maxfps 0) so the
    # load's frames hold no frame cap's waits.
    LOADS = ["host_maxfps 0"]
    add("load_e1m1", ["loading", "core"], "E1M1 loaded cold, again warm, then restarted (map load times)", None,
        "`map e1m1` with nothing loaded before (cold), `map e1m1` again (warm), `restart`: each load's stages "
        "(BSP, lightmaps, textures, models, QuakeC spawn, VR prepare, first frame) and the second after it.",
        setup=LOADS, warm=30, body=loads_body([("cold", "map e1m1"), ("warm", "map e1m1"), ("restart", "restart")]), loads=3, frames=0)
    add("load_e1m1_qrp", ["loading", "textures"], "E1M1 loaded cold and warm on QRP (external textures, normal maps)",
        None, "load_e1m1's cold and warm loads on the QRP base: the replacement textures' and their material maps' share.",
        base="qrp", setup=LOADS, warm=30, body=loads_body([("cold", "map e1m1"), ("warm", "map e1m1")]), loads=2, frames=0)
    add("load_e4m7", ["loading"], "E4M7, id's biggest BSP, loaded cold and warm", None,
        "`map e4m7` cold, then again: the largest of id's maps (1.5 MB BSP).", setup=LOADS, warm=30,
        body=loads_body([("cold", "map e4m7"), ("warm", "map e4m7")]), loads=2, frames=0)
    add("load_hip1m1", ["loading"], "a campaign switch: Scourge of Armagon's first map, again, then back to E1M1", None,
        "`map hip1m1` from Quake's campaign (its game folders rebuilt: the campaign stage), again (no switch), then "
        "`map e1m1` (the switch back).", setup=LOADS, warm=30,
        body=loads_body([("switch", "map hip1m1"), ("warm", "map hip1m1"), ("back", "map e1m1")]), loads=3, frames=0)
    add("load_changelevel", ["loading"], "in-game level changes: E1M1 to E1M2 and back (changelevel)", "e1m1",
        "`changelevel e1m2` from E1M1 (the in-game path: spawn parms kept), then `changelevel e1m1` (its models "
        "warm); each load and the second after it (first-use hitches).", setup=LOADS, warm=60,
        body=loads_body([("e1m2", "changelevel e1m2"), ("e1m1", "changelevel e1m1")]), loads=2, frames=0)
    for m in ("warden", "ad_grendel"):
        add(f"load_{m}", ["loading", "maps"], f"the custom map {m} loaded cold and warm ({TOUR_SOURCES[m]})", None,
            f"`map {m}` cold (its map package mounted), then again: heavy geometry, many entities and lights.",
            setup=LOADS, warm=30, body=loads_body([("cold", f"map {m}"), ("warm", f"map {m}")]), loads=2, frames=0)
    add("timedemo_demo1_flat", ["loading", "flat"], "id's demo1 played as fast as it goes (timedemo), flat", "start",
        "The classic `timedemo demo1` in flat mode inside the window: a replayed game's frames (its own fps line too).",
        flat=True, body=lambda frames: ["timedemo demo1"] + waits(frames), warm=30, loads=1)  # (the demo's map)
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
    # A graphics profile first (a whole ironwail.cfg will do), then what the suite pins whatever it says.
    lines = [f'exec "{settings}"'] if settings else []
    lines += ["vr_enabled 0" if sc.flat else "vr_enabled 1", "vr_fixed_frames 1", f"vr_fixed_frames_rate {hz}",
              f"vr_mock_eye_size {eye}", "vid_vsync 0", f"host_maxfps {hz if realtime else 0}", "con_notifytime 0",
              "vr_tips 0", "developer 0", "sv_autosave 0", "vr_profile 0", "vr_particle_seed 7", "vr_mock_look 0 0"]
    lines += sc.header + waits(10)
    if sc.map:
        lines += ["vr_bench_seed 7", f"map {sc.map}"] + waits(sc.load_wait)
        lines += ["god 1", "notarget 0" if sc.hostile else "notarget 1"]
        if sc.pos:
            lines += [f"setpos {sc.pos}"]
    lines += [f"vr_mock_look {sc.look[0]} {sc.look[1]}", "vr_bench_seed 7"] + sc.setup
    if sc.motion:
        lines += [f'vr_mock_play "{motion_path}"']
    if sc.frames == 0:
        # The window lasts the body (vr_bench_end ends it): a sequence whose frames are not known in advance (a load's
        # frames: the VR runtime's loading frames among them).
        lines += waits(sc.warm) + ["echo BENCH_SETUP_DONE", f"vr_bench_begin {tag}"] + sc.body(frames) + waits(20)
    else:
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
    # The window's parts (vr_bench_mark): each one's worst frame (median over the repeats) and its mean.
    for i, m in enumerate(rs[0].get("marks", [])):
        same = [r["marks"][i] for r in rs if len(r.get("marks", [])) > i and r["marks"][i]["label"] == m["label"]]
        out[f"mark.{m['label']}.max_ms"] = statistics.median(x["max_ms"] for x in same)
        out[f"mark.{m['label']}.avg_ms"] = statistics.median(x["avg_ms"] for x in same)
    # The map loads: each one's total, its first frame, the worst frame of the second after it, and its stages.
    for i, ld in enumerate(rs[0].get("loads", [])):
        same = [r["loads"][i] for r in rs if len(r.get("loads", [])) > i]
        k = f"load{i + 1}"
        out[f"{k}.what"] = ld["what"]
        out[f"{k}.total_ms"] = statistics.median(x["total_ms"] for x in same)
        out[f"{k}.first_frame_ms"] = statistics.median(x["first_frame"]["max_ms"] for x in same)
        out[f"{k}.after_max_ms"] = statistics.median(x["after"]["max_ms"] for x in same)
        for cat, ms in load_categories(ld).items():
            out[f"{k}.{cat}_ms"] = statistics.median(load_categories(x).get(cat, 0.0) for x in same)
    return out


# A load's stages (vr_startup_times' marks) gathered into the phases a reader looks for; the rest stays "other".
LOAD_PHASES = [("command", ("map:", "command:", "load:")), ("bsp", ("server: world model",)),
               ("qc_spawn", ("server: entities spawned",)), ("physics_init", ("server: 2 frames",)),
               ("server_other", ("server:",)), ("models", ("client: models",)), ("sounds", ("client: sounds",)),
               ("lightmaps", ("R_NewMap: lightmaps",)), ("renderer", ("R_NewMap", "client: R_NewMap")),
               ("vr_prepare", ("VR_NewMap",)), ("signon", ("frames to the signon",)), ("first_frame", ("first frame drawn",))]
# Kinds of work across the stages (VR_TimeAdd sums), by the start of their names.
LOAD_WORK = [("textures", "textures processed and uploaded"), ("images", "image files found"),
             ("alias_models", "alias models (.mdl) loaded"), ("normal_maps", "normal maps made"),
             ("extmaps", "external material maps"), ("ragdoll_rigs", "ragdoll: the map's rigs"),
             ("hulls", "hull: the map's brushes"), ("box3d_mesh", "box3d: the world's mesh")]


def load_categories(ld):
    out = {}
    for name, ms in ld["stages"]:
        cat = next((c for c, prefixes in LOAD_PHASES if name.startswith(prefixes)), "other")
        out[cat] = out.get(cat, 0.0) + ms
    for name, ms, _ in ld["work"]:
        for cat, prefix in LOAD_WORK:
            if name.startswith(prefix):
                out[f"work_{cat}"] = out.get(f"work_{cat}", 0.0) + ms
    return out


def loads_report(root):
    """Each loading scenario's loads: the phases (median over the repeats, ms) and the second after."""
    runs = load_runs(root)
    phases = [c for c, _ in LOAD_PHASES] + ["other"] + [f"work_{c}" for c, _ in LOAD_WORK]
    lines = []
    for name, rs in sorted(runs.items()):
        if not rs[0].get("loads"):
            continue
        agg = aggregate(rs)
        lines.append(f"### {name}\n")
        lines.append("| load | total | " + " | ".join(phases) + " | worst after |")
        lines.append("|" + "---|" * (len(phases) + 3))
        for i in range(len(rs[0]["loads"])):
            k = f"load{i + 1}"
            cells = [f"{agg.get(f'{k}.{c}_ms', 0.0):.1f}" for c in phases]
            lines.append(f"| {agg[f'{k}.what']} | {agg[f'{k}.total_ms']:.1f} | " + " | ".join(cells)
                         + f" | {agg[f'{k}.after_max_ms']:.1f} |")
        lines.append("")
    return "\n".join(lines)


def marks_report(root):
    """Each scenario's window parts (vr_bench_mark): the worst and mean frame (median over the repeats)."""
    lines = []
    for name, rs in sorted(load_runs(root).items()):
        marks = rs[0].get("marks", [])
        if not marks:
            continue
        agg = aggregate(rs)
        parts = ", ".join(f"{m['label']} {agg['mark.' + m['label'] + '.max_ms']:.2f}" for m in marks)
        lines.append(f"- {name} (worst frame, ms): {parts}")
    return "\n".join(lines)


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
    m = manifest(root)
    md = [f"{k}: {m[k]}  " for k in ("label", "tree", "mode", "reps", "settings") if k in m] + [""] + md
    marks, loads = marks_report(root), loads_report(root)
    if marks:
        md += ["", "## Window parts (vr_bench_mark)", "", marks]
    if loads:
        md += ["", "## Map loads (ms; stages gathered into phases, work summed across them)", "", loads]
    (Path(root) / "summary.md").write_text("\n".join(md) + "\n")
    print("\n".join(md))


def manifest(root):
    path = Path(root) / "manifest.txt"
    return dict(l.split(" ", 1) for l in path.read_text().splitlines() if " " in l) if path.exists() else {}


def compare(base, new, threshold):
    a, b = load_runs(base), load_runs(new)
    ma, mb = manifest(base), manifest(new)
    for key in ("reps", "mode", "settings", "exe_sha256", "progs_sha256", "tree"):
        if ma.get(key) != mb.get(key):
            print(f"note: {key}: baseline {ma.get(key)!r}, new {mb.get(key)!r}")
    for name in sorted(set(a) & set(b)):  # the settings each run recorded (another eye size, a cvar changed...)
        sa, sb = a[name][0]["settings"], b[name][0]["settings"]
        diff = [f"{k} {sa.get(k)}->{sb.get(k)}" for k in sorted(set(sa) | set(sb)) if sa.get(k) != sb.get(k)]
        if a[name][0]["eye_resolution"] != b[name][0]["eye_resolution"]:
            diff.append(f"eyes {a[name][0]['eye_resolution']}->{b[name][0]['eye_resolution']}")
        if diff:
            print(f"note: {name}: settings differ: {', '.join(diff)}")
    print(f"| scenario | metric | baseline | new | change |\n|---|---|---|---|---|")
    flagged = 0
    for name in sorted(set(a) & set(b)):
        ra, rb = aggregate(a[name]), aggregate(b[name])
        extra = [tuple(k.rsplit(".", 1)) for k in ra if (k.startswith("mark.") and k.endswith(".max_ms"))
                 or (k.startswith("load") and k.endswith((".total_ms", ".first_frame_ms", ".after_max_ms")))]
        for series, stat in KEYS + extra:
            k = f"{series}.{stat}"
            if k not in ra or k not in rb:
                continue
            x, y = ra[k], rb[k]
            pct = (y - x) / x * 100 if x else 0.0
            # Past the noise: the relative change and an absolute floor (sub-0.05 ms moves are timer noise).
            flag = abs(pct) >= threshold and abs(y - x) >= (0.05 if series.endswith("_ms") or stat.endswith("_ms") else 0.5)
            flagged += flag
            print(f"| {name} | {k} | {x:.3f} | {y:.3f} | {pct:+.1f}%{' **' if flag else ''} |")
    for name in sorted(set(a) ^ set(b)):
        print(f"| {name} | (only in {'baseline' if name in a else 'new'}) | | | |")
    print(f"\n{flagged} changes past {threshold}% (marked **)")


def validate(root):
    """Each scenario's repeats started their windows with the same edicts, monsters alive and Box3D bodies (exact),
    and decals within 5% (blood and bullet marks of a fight in the set-up land a few more or fewer: they follow the
    threads' timing, not only the seeded random numbers); the same marks; and the map loads the scenario makes."""
    runs = load_runs(root)
    table = scenarios()
    bad = 0
    for name, rs in sorted(runs.items()):
        exact = [tuple(r["counts"][c]["start"] for c in ("edicts", "monsters_alive", "box3d_bodies")) for r in rs]
        decals = [r["counts"]["decals"]["start"] for r in rs]
        same = all(e == exact[0] for e in exact)
        close = max(decals) - min(decals) <= max(2, 0.05 * max(decals))
        frames = [r["frame"]["frame_ms"]["n"] for r in rs]
        marks = [len(r.get("marks", [])) for r in rs]
        loads = [len(r.get("loads", [])) for r in rs]
        want = table[name].loads if name in table else 0
        fixed = not (name in table and table[name].frames == 0)
        ok = (same and close and (not fixed or all(f == rs[0]["frames"] for f in frames)) and all(m == marks[0] for m in marks)
              and all(n == want for n in loads))
        bad += not ok
        print(f"{name:28} runs={len(rs)} frames={frames} edicts/monsters/bodies={exact[0]} decals={decals} "
              f"marks={marks} loads={loads}/{want} {'same' if same and close else 'DIFFERENT'}{'' if ok else '  <- FAIL'}")
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
    lr = sub.add_parser("loads", help="the loading scenarios' loads by phase, and every scenario's marks")
    lr.add_argument("root")
    a = p.parse_args()
    if a.cmd == "list":
        table = scenarios()
        for n in select(a.group):
            x = table[n]
            print(n if a.names else f"{n:28} {x.base:5} {x.map or "(none)":14} {'/'.join(x.groups):24} {x.stresses}")
    elif a.cmd == "script":
        sc = scenarios().get(a.scenario)
        if not sc:
            raise SystemExit(f"qvrbench: no scenario {a.scenario}")
        if a.warm is not None:
            sc.warm = a.warm
        if sc.frames is not None:
            a.frames = sc.frames  # (a fixed sequence of events: its own length; 0 the body's)
        out = Path(a.out)
        motion = out.with_suffix(".motion.txt")
        if sc.motion:
            motion.write_text(sc.motion(a.frames / a.hz + sc.warm / a.hz + 5))
        out.write_text(script(sc, a.tag or sc.name, a.frames, a.hz, a.eye, a.realtime, a.settings,
                              motion.as_posix(), a.shot))
        if a.info:
            print(f"{sc.base} {sc.map or '-'}")
    elif a.cmd == "summarize":
        summarize(a.root)
    elif a.cmd == "compare":
        compare(a.base, a.new, a.threshold)
    elif a.cmd == "brief":
        r = json.loads(Path(a.json).read_text())
        f, c, g = r["frame"]["frame_ms"], r["frame"]["cpu_busy_ms"], r["frame"]["gpu_3d_ms"]
        n = {k: v["start"] for k, v in r["counts"].items() if k in ("edicts", "monsters_alive", "box3d_bodies")}
        extra = ""
        if r.get("loads"):
            extra += "; loads " + ", ".join(f"{x['what']} {x['total_ms']:.0f} ms" for x in r["loads"])
        if r.get("marks"):
            worst = max(r["marks"], key=lambda x: x["max_ms"])
            extra += f"; {len(r['marks'])} marks, worst {worst['label']} {worst['max_ms']:.2f} ms"
        print(f"{r['frames']} frames on {r['map'] or '-'}: frame p50 {f['p50']:.3f} p99 {f['p99']:.3f} max {f['max']:.2f}, "
              f"cpu {c['avg']:.3f}, gpu 3D {g['avg']:.3f} ms; start {n}{extra}")
    elif a.cmd == "loads":
        print(marks_report(a.root))
        print(loads_report(a.root))
    elif a.cmd == "validate":
        sys.exit(1 if validate(a.root) else 0)


if __name__ == "__main__":
    main()
