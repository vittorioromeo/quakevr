# common.py -- the logo splash's timeline, layout and camera, shared by the Blender pass (blender_scene.py, run by
# Blender's own Python) and the 2D passes (fx_precompute.py, composite.py). Plain Python: no numpy, no bpy.
#
# Space: Blender metres, Z up. The "wall" (the screen-filling plane the blood lands on, never rendered itself) is the
# plane y = 0; the camera stands at y = -CAM_DIST looking along +y, so a point on the wall maps to the screen by a
# uniform scale. Everything 3D (the grunt, the axe, the gibs, the letters) is in front of the wall (y < 0).

import math
import os

FPS = 60
FRAMES = 720                      # 12 s: the last 3.5 s keep burning

# Where the work and the outputs go (outside git).
OUT_ROOT = os.environ.get("QVR_SPLASH_OUT", r"C:\OHWorkspace\qvr-trailer\logo_splash")
WORK = os.path.join(OUT_ROOT, "work")

# Camera: a 50 mm lens on a 36 mm wide sensor, CAM_DIST in front of the wall.
CAM_DIST = 10.0
FOCAL_MM = 50.0
SENSOR_MM = 36.0
WALL_W = CAM_DIST * SENSOR_MM / FOCAL_MM        # 7.2 m of wall across the frame
WALL_H = WALL_W * 9.0 / 16.0


def wall_to_px(x, z, width):
    """A point on the wall (y = 0) to pixel coordinates (column, row) in a frame `width` pixels wide."""
    s = width / WALL_W
    return width * 0.5 + x * s, width * 9 / 32 - z * s


def project(p, width):
    """A 3D point (x, y, z) to pixel coordinates and its depth from the camera (None behind the camera)."""
    x, y, z = p
    d = y + CAM_DIST
    if d <= 1e-3:
        return None
    k = CAM_DIST / d
    c, r = wall_to_px(x * k, z * k, width)
    return c, r, d


def px_per_m(width):
    return width / WALL_W


# --- The timeline (frames at 60 fps) ---
AXE_START = 50
HIT = 68                          # the axe reaches the grunt: he bursts
GIB_END = 150                     # the last gib has left the frame

# The letters of "QUAKE VR": the frame each lands on (they fall FALL frames).
LETTERS = "QUAKEVR"
LAND = [110, 121, 132, 143, 154, 176, 188]
FALL = 14
LETTERS_END = LAND[-1] + 2        # after this the letters are still: their last frame is reused

FINGER_START = 214                # "Unleashed" written in the blood
FINGER_END = 296
MAT_START = 300                   # ... and materialising
MAT_END = 342
FIRE_START = 352                  # everything bursts into flames
FIRE_FULL = 372

# --- Layout on the wall (metres) ---
GRUNT_Y = -1.0                    # the grunt stands a metre in front of the wall
GRUNT_SCALE = 0.042               # Quake units to metres
GRUNT_FEET_Z = -1.32
CHEST = (0.0, GRUNT_Y, 0.2)      # where the axe hits and the burst starts

# The lettering is the logo's own (trace_logo.py -> letters.json), laid out as in the logo: the title LOGO_WIDTH wide,
# centred, the U's foot on TITLE_Z; "UNLEASHED" and the Q's nail where the logo has them.
LETTERS_JSON = os.path.join(OUT_ROOT, "letters.json")
LOGO_WIDTH = 5.0
TITLE_Z = 0.36
LETTER_DEPTH = 0.13               # extrusion

# The letters start this far in front of the wall (close to the camera: they fill the frame, then slam in).
DROP_FROM = 8.6


def ease_in(t):
    return t * t


def letter_height(i, f):
    """How far letter i is in front of the wall at frame f (0 once it has landed; None before it appears)."""
    land = LAND[i]
    start = land - FALL
    if f < start:
        return None
    if f >= land:
        # The landing: a short dig into the wall and back (weight), in centimetres.
        k = f - land
        return -0.025 * math.exp(-k / 2.0) * math.sin(min(k, 6) / 6.0 * math.pi) if k < 8 else 0.0
    t = (f - start) / FALL
    return DROP_FROM * (1.0 - ease_in(t))


def letter_tilt(i, f):
    """The letter's tilt (rx, rz in radians) while falling: it straightens as it lands."""
    h = letter_height(i, f)
    if h is None or h <= 0:
        return 0.0, 0.0
    k = h / DROP_FROM
    sx = (-1) ** i
    return sx * 0.35 * k, -sx * 0.25 * k * (1 + 0.3 * math.sin(i * 1.7))


# Camera shake: (frame, strength): a damped wobble after each impact (applied in 2D by composite.py).
SHAKES = [(HIT, 1.0)] + [(f, 0.4 if i < 5 else 0.55) for i, f in enumerate(LAND)] + [(FIRE_START, 0.6)]
KICKS = [(f, 1.0 if i < 5 else 1.3) for i, f in enumerate(LAND)]


def shake(f, amount_px):
    """The frame's offset (dx, dy) in pixels and its roll in radians: a subtle damped wobble after each impact, and a
    short downward kick as each letter lands (weight). `amount_px` scales it (about 8 px at 4K)."""
    dx = dy = rot = 0.0
    for k, (f0, s) in enumerate(SHAKES):
        t = (f - f0) / FPS
        if t < 0 or t > 0.6:
            continue
        env = s * math.exp(-t * 9.0)
        dx += env * math.sin(t * 2 * math.pi * 17 + k * 1.3) * amount_px
        dy += env * math.sin(t * 2 * math.pi * 13 + k * 2.1 + 0.7) * amount_px * 1.25
        rot += env * math.sin(t * 2 * math.pi * 9 + k) * 0.0025
    for f0, s in KICKS:
        t = (f - f0) / 1.6                       # in frames: down at once, back over a few frames
        if 0 <= t < 8:
            dy += s * amount_px * 1.6 * t * math.exp(1 - t)
    return dx, dy, rot


def rng(seed):
    """A small deterministic generator usable in both Pythons."""
    import random
    return random.Random(seed)


# --- The axe: thrown from the left, spinning in the camera's view plane, through the grunt and away ---
AXE_FROM = (-5.6, -1.55, 0.62)
AXE_AT = (-0.05, -1.32, 0.16)
AXE_TO = (6.0, -2.3, 1.1)
AXE_OUT = HIT + 14                # out of the frame
AXE_SPIN = 4.2                    # turns a second


def axe_state(f):
    """(position, spin angle in radians) of the axe at frame f, or None when it is not in the shot."""
    if f < AXE_START - 1 or f > AXE_OUT + 1:
        return None
    if f <= HIT:
        t = (f - AXE_START) / (HIT - AXE_START)
        p = tuple(a + (b - a) * t for a, b in zip(AXE_FROM, AXE_AT))
        p = (p[0], p[1], p[2] + 0.35 * math.sin(t * math.pi) * 0.5)
    else:
        t = (f - HIT) / (AXE_OUT - HIT)
        p = tuple(a + (b - a) * t for a, b in zip(AXE_AT, AXE_TO))
    ang = -(f - AXE_START) / FPS * AXE_SPIN * 2 * math.pi
    return p, ang


# --- The gibs: launched from the chest at HIT, flying out (many towards the camera) under gravity ---
GRAVITY = 9.8


def _gib_specs():
    r = rng(1996)
    models = (["progs/gib1.mdl"] * 5 + ["progs/gib2.mdl"] * 4 + ["progs/gib3.mdl"] * 5 + ["progs/h_guard.mdl"]
              + ["brain1", "brain2", "brain3"])
    out = []
    for k, m in enumerate(models):
        a = (k / len(models)) * 2 * math.pi + r.uniform(-0.3, 0.3)
        sp = r.uniform(4.0, 9.5)
        v = (math.cos(a) * sp * 1.25, -r.uniform(0.8, 4.5), math.sin(a) * sp * 0.8 + r.uniform(1.5, 4.0))
        if m == "progs/h_guard.mdl":
            v = (1.4, -3.2, 5.2)           # the head: up and at the camera, tumbling
        p = (CHEST[0] + r.uniform(-0.15, 0.15), CHEST[1] + r.uniform(-0.1, 0.1), CHEST[2] + r.uniform(-0.25, 0.3))
        ax = (r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(-1, 1))
        n = math.sqrt(sum(c * c for c in ax)) or 1.0
        out.append(dict(model=m, p=p, v=v, axis=tuple(c / n for c in ax), spin=r.uniform(9, 22) * r.choice((-1, 1)),
                        scale=GRUNT_SCALE * r.uniform(0.6, 0.8) * (1.25 if m.startswith("brain") else 1.0)))
    return out


GIBS = _gib_specs()


def gib_state(g, f):
    """(position, angle) of gib g at frame f (None before the burst)."""
    if f < HIT:
        return None
    t = (f - HIT) / FPS
    p = (g["p"][0] + g["v"][0] * t, g["p"][1] + g["v"][1] * t, g["p"][2] + g["v"][2] * t - 0.5 * GRAVITY * t * t)
    return p, g["spin"] * t
