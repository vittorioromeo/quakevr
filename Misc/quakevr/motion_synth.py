#!/usr/bin/env python3
# motion_synth.py -- synthetic motion takes for the melee's tests (docs/vr-port/MOTIONS.md): the motion recorder's
# take format written from simple parametric curves, played like a real take by vr_motion_play and vr_motion_eval.
#
# A take holds the tracking a runtime would report (tracking space: metres, +x right, +y up, -z forward; the
# head standing at the origin, facing -z), the controllers (grip, trigger, stick clicks), the weapons in hand and
# where the target stands. Positions here are given in the player's frame: metres, x forward, y left, z up from
# the floor below the head. Orientations are given by where things point:
#   - a weapon (sword): its blade's direction, and where the back of the hand faces (the hand's "right" for the
#     main hand: the swing's leading side is the thumb's) as a hint;
#   - an empty hand (fist, palm): the hand's forward (the knuckles' way for a fist, the fingers' for a palm) and
#     its up (the thumb's side: the palms face in as a controller is held, the main hand's to its left).
# The controller orientations that give those are worked out from the hand's offsets (vr_gunangle 39.5,
# vr_gunyaw 4, vr_offhandpitch 40.25, vr_offhandyaw -4: the shipped values) and, for the sword, the blade's
# direction measured on the controller (the shipped weapon offsets; SWORD_BLADE below). Other weapons point
# along the hand's forward (a gun's barrel) and are approximate.
#
# Velocities are the curves' own (central differences, as a runtime's IMU-fused ones are smooth); the frame
# rate is fixed (--rate, default 90 Hz); the server frames are left to the engine (72 Hz).
#
# Usage:
#   python Misc/quakevr/motion_synth.py --list
#   python Misc/quakevr/motion_synth.py <preset> [--out quakevr/motions/synth] [--duration S] [--distance M]
#       [--rate HZ] [--world-scale 1.25] [--eye-height 1.646] [--two-handed] [--hand main|off]
#   python Misc/quakevr/motion_synth.py all [--out ...]      (every preset, a test set)
# As a module: build a Take (hold, move, arc segments), then take.write(path). See the presets for examples.

import argparse
import math
import os
import sys
import time

# ---------------------------------------------------------------------------------------------------------
# Vectors and quaternions (w, x, y, z)


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def mul(a, s):
    return tuple(x * s for x in a)


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(a):
    length = math.sqrt(dot(a, a))
    return mul(a, 1.0 / length) if length > 1e-9 else a


def lerp(a, b, s):
    return tuple(x + (y - x) * s for x, y in zip(a, b))


def qmul(a, b):
    aw, ax, ay, az = a
    bw, bx, by, bz = b
    return (aw * bw - ax * bx - ay * by - az * bz, aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx, aw * bz + ax * by - ay * bx + az * bw)


def qconj(q):
    return (q[0], -q[1], -q[2], -q[3])


def qrot(q, v):
    return qmul(qmul(q, (0.0,) + tuple(v)), qconj(q))[1:]


def qaxis(axis, degrees):
    h = math.radians(degrees) * 0.5
    a = norm(axis)
    return (math.cos(h), a[0] * math.sin(h), a[1] * math.sin(h), a[2] * math.sin(h))


def qfrombasis(x, y, z):
    """The rotation taking the unit axes to the orthonormal x, y, z (columns)."""
    m = ((x[0], y[0], z[0]), (x[1], y[1], z[1]), (x[2], y[2], z[2]))
    tr = m[0][0] + m[1][1] + m[2][2]
    if tr > 0:
        s = math.sqrt(tr + 1.0) * 2
        return (0.25 * s, (m[2][1] - m[1][2]) / s, (m[0][2] - m[2][0]) / s, (m[1][0] - m[0][1]) / s)
    if m[0][0] > m[1][1] and m[0][0] > m[2][2]:
        s = math.sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2
        return ((m[2][1] - m[1][2]) / s, 0.25 * s, (m[0][1] + m[1][0]) / s, (m[0][2] + m[2][0]) / s)
    if m[1][1] > m[2][2]:
        s = math.sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2
        return ((m[0][2] - m[2][0]) / s, (m[0][1] + m[1][0]) / s, 0.25 * s, (m[1][2] + m[2][1]) / s)
    s = math.sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2
    return ((m[1][0] - m[0][1]) / s, (m[0][2] + m[2][0]) / s, (m[1][2] + m[2][1]) / s, 0.25 * s)


def orient(local_a, target_a, local_b, target_b):
    """The rotation taking local_a onto target_a, and local_b as near target_b as it can."""
    la = norm(local_a)
    lb = norm(sub(local_b, mul(la, dot(local_b, la))))
    ta = norm(target_a)
    tb = sub(target_b, mul(ta, dot(target_b, ta)))
    if dot(tb, tb) < 1e-8:  # the hint along the axis: any perpendicular
        tb = cross(ta, (0.0, 1.0, 0.0)) if abs(ta[1]) < 0.9 else cross(ta, (1.0, 0.0, 0.0))
    tb = norm(tb)
    local = qfrombasis(la, lb, cross(la, lb))
    target = qfrombasis(ta, tb, cross(ta, tb))
    return qmul(target, qconj(local))


def qslerp(a, b, s):
    d = dot(a, b)
    if d < 0:
        b, d = mul(b, -1.0), -d
    if d > 0.9995:
        return norm(lerp(a, b, s))
    w = math.acos(d)
    return norm(add(mul(a, math.sin((1 - s) * w) / math.sin(w)), mul(b, math.sin(s * w) / math.sin(w))))


# ---------------------------------------------------------------------------------------------------------
# Frames: the player's (metres, x forward, y left, z up from the floor) and tracking space (x right, y up,
# z back)


def track(p):
    return (-p[1], p[2], -p[0])


def track_dir(d):
    return (-d[1], d[2], -d[0])


# The hands' offsets (vr_hands.cpp withHandOffsets): the game's hand is the controller turned by yaw about +y,
# then by -pitch about +x.
HAND_OFFSETS = {"main": (39.5, 4.0), "off": (40.25, -4.0)}  # (pitch, yaw) as shipped


# The settings a take is written for (--settings; configure): name=value pairs written into its header's settings
# line, which playback sets (another hand calibration: vr_gunangle, vr_handcal_*...). Empty: the shipped ones (the
# header's hand angles line). The hands' pitch and yaw are taken from them (HAND_OFFSETS); their move and roll
# (vr_handcal_*) move the game's hand on its controller as they do in the game (the take is the controller's).
# SWORD_BLADE is measured at the shipped settings: the sword presets are for those.
SETTINGS = {}


def configure(settings):
    """Writes the takes for `settings` ({name: value}, e.g. the author's hand calibration)."""
    SETTINGS.clear()
    SETTINGS.update(settings)

    def g(key, default):
        return float(settings.get(key, default))
    main = (g("vr_gunangle", 39.5), g("vr_gunyaw", 4.0))
    HAND_OFFSETS["main"] = main
    if g("vr_handcal_off_mirror", 0.0) != 0.0:
        HAND_OFFSETS["off"] = (main[0], -main[1])  # vr_hands.cpp calibration(): the main hand's, mirrored
    else:
        HAND_OFFSETS["off"] = (g("vr_offhandpitch", 40.25), g("vr_offhandyaw", -4.0))


def hand_offset(hand):
    pitch, yaw = HAND_OFFSETS[hand]
    return qmul(qaxis((0, 1, 0), yaw), qaxis((1, 0, 0), -pitch))


# The sword's blade on the controller (its local direction, from butt to tip), measured in the mock headset with
# the shipped settings (vr_gunangle 39.5, the sword's shipped offsets): about 49 degrees above the controller's
# forward.
SWORD_BLADE = norm((-0.038, 0.650, -0.759))


def hand_pose(hand, forward, up):
    """A controller orientation for the game's hand pointing `forward`, its thumb towards `up` (player frame)."""
    off = hand_offset(hand)
    game = orient((0, 0, -1), track_dir(forward), (0, 1, 0), track_dir(up))
    return qmul(game, qconj(off))


def blade_pose(hand, blade, right):
    """A controller orientation for a sword whose blade points along `blade`, the game hand's right (the back of
    the main hand, the palm of the off hand) towards `right` (player frame)."""
    off = hand_offset(hand)
    right_local = qrot(off, (1, 0, 0))
    return orient(SWORD_BLADE, track_dir(blade), right_local, track_dir(right))


def minjerk(s):
    return 10 * s ** 3 - 15 * s ** 4 + 6 * s ** 5


def bezier(p0, mid, p2, s):
    """A quadratic Bezier through p0, mid (at s = 0.5) and p2."""
    c = tuple(2 * m - 0.5 * (a + b) for a, m, b in zip(p0, mid, p2))
    return tuple((1 - s) ** 2 * a + 2 * s * (1 - s) * cc + s * s * b for a, cc, b in zip(p0, c, p2))


# ---------------------------------------------------------------------------------------------------------
# A take

WEAPONS = {"fist": 0, "axe": 2, "mjolnir": 3, "shotgun": 4, "super_shotgun": 5, "nailgun": 6, "sword": 13}


class Take:
    def __init__(self, label, rate=90.0, world_scale=1.25, eye_height=1.646, main_weapon="fist", off_weapon="fist",
                 target=(1.15, 0.0), note=""):
        """`target`: where the dummy's origin stands, player frame metres (x ahead, y left); None: no target."""
        self.label = label
        self.rate = rate
        self.world_scale = world_scale
        self.eye_height = eye_height
        self.weapons = {"main": WEAPONS[main_weapon], "off": WEAPONS[off_weapon]}
        self.target = target
        self.note = note
        self.frames = []  # (t, phase, {hand: (pos, quat)}, {hand: grip})
        self.pose = {"main": ((0.35, -0.2, eye_height - 0.45), hand_pose("main", (1, 0, 0), (0, 0, 1))),
                     "off": ((0.35, 0.2, eye_height - 0.45), hand_pose("off", (1, 0, 0), (0, 0, 1)))}
        self.grips = {"main": main_weapon != "fist", "off": off_weapon != "fist"}
        self.t = 0.0
        self.phase = "pre"
        self.fists = set()  # empty hands whose grip is a fist of their own (a punch, the flashlight), not a helping grip

    def set(self, hand, pos, quat):
        self.pose[hand] = (tuple(pos), quat)

    def grip(self, hand, on):
        self.grips[hand] = on

    def _emit(self):
        self.frames.append((self.t, self.phase, dict(self.pose), dict(self.grips)))

    def hold(self, seconds, phase=None):
        if phase:
            self.phase = phase
        for _ in range(max(1, int(round(seconds * self.rate)))):
            self._emit()
            self.t += 1.0 / self.rate

    def move(self, seconds, fn, ease=True, phase=None):
        """fn(s) -> {hand: (pos, quat)} for s in 0..1 (eased by a minimum-jerk profile)."""
        if phase:
            self.phase = phase
        n = max(2, int(round(seconds * self.rate)))
        for i in range(1, n + 1):
            s = i / n
            for hand, pq in fn(minjerk(s) if ease else s).items():
                self.pose[hand] = (tuple(pq[0]), pq[1])
            self._emit()
            self.t += 1.0 / self.rate

    def start(self, targets):
        """The poses the take starts in ({hand: (pos, quat)}): before any frame, so that playback settles there
        (its grips are pressed in the first frame's pose: a two-handed grip is taken there)."""
        for hand, (pos, quat) in targets.items():
            self.pose[hand] = (tuple(pos), quat)

    def glide(self, targets, seconds, phase=None):
        """To `targets` ({hand: (pos, quat)}) from the current poses."""
        start = dict(self.pose)
        self.move(seconds, lambda s: {h: (lerp(start[h][0], p, s), qslerp(start[h][1], q, s)) for h, (p, q) in
                                      targets.items()}, phase=phase)

    def write(self, path):
        m2u = 26.2467 * self.world_scale  # (vr_units.hpp: a unit is 1.5 inches at vr_world_scale 1)
        dt = 1.0 / self.rate
        rows = []
        head = {"px": 0.0, "py": self.eye_height, "pz": 0.0}
        first_rec = next((f[0] for f in self.frames if f[1] == "rec"), self.frames[0][0])
        for i, (t, phase, pose, grips) in enumerate(self.frames):
            prev = self.frames[max(0, i - 1)][2]
            nxt = self.frames[min(len(self.frames) - 1, i + 1)][2]
            span = dt * (min(len(self.frames) - 1, i + 1) - max(0, i - 1)) or dt
            row = {"t": "%.5f" % (t - first_rec), "phase": phase, "dt": "%.6f" % dt, "play_yaw": "0"}
            row.update({"raw_head_" + k: "%.5f" % v for k, v in head.items()})
            row.update({"raw_head_qw": "1", "raw_head_qx": "0", "raw_head_qy": "0", "raw_head_qz": "0",
                        "raw_head_valid": "1", "raw_head_vvalid": "1"})
            for hand, p in (("main", "m"), ("off", "o")):
                pos, q = pose[hand]
                tp = track(pos)
                v = mul(sub(track(nxt[hand][0]), track(prev[hand][0])), 1.0 / span)
                dq = qmul(nxt[hand][1], qconj(prev[hand][1]))
                if dq[0] < 0:
                    dq = mul(dq, -1.0)
                angle = 2 * math.acos(max(-1.0, min(1.0, dq[0])))
                axis = norm(dq[1:]) if angle > 1e-6 else (0.0, 0.0, 0.0)
                w = mul(axis, angle / span)
                row.update({"raw_%s_p%s" % (p, a): "%.5f" % x for a, x in zip("xyz", tp)})
                row.update({"raw_%s_q%s" % (p, a): "%.6f" % x for a, x in zip("wxyz", q)})
                row.update({"raw_%s_v%s" % (p, a): "%.5f" % x for a, x in zip("xyz", v)})
                row.update({"raw_%s_w%s" % (p, a): "%.5f" % x for a, x in zip("xyz", w)})
                row.update({"raw_%s_valid" % p: "1", "raw_%s_vvalid" % p: "1", "raw_%s_gvalid" % p: "0"})
                g = grips[hand]
                row.update({"%s_buttons" % p: str(2 if g else 0), "%s_grip" % p: "1" if g else "0", "%s_trigger" % p: "0",
                            "%s_thumb" % p: "1", "%s_stick_x" % p: "0", "%s_stick_y" % p: "0",
                            "%s_wid" % p: str(self.weapons[hand]), "%s_wflags" % p: "0",
                            "%s_helping" % p: "1" if (hand == "off" and g and self.weapons["off"] == 0 and hand not in self.fists) else "0"})
            if self.target:
                row.update({"mon_class": "vr_dummy", "mon_x_u": "%.3f" % (self.target[0] * m2u),
                            "mon_y_u": "%.3f" % (self.target[1] * m2u), "mon_z_u": "0"})
            row["events"] = ""
            rows.append(row)
        columns = list(rows[0].keys())
        category = self.label.split("_")[0]
        for c in ("no_hit", "palm_shove_1h", "palm_shove_2h", "parry_pose", "parry_bash", "hilt_pommel", "gun_strike"):
            if self.label.startswith(c):
                category = c
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        with open(path, "w", newline="\n") as f:
            f.write("# Quake VR motion take (docs/vr-port/MOTIONS.md)\n")
            f.write("# format: 1\n# label: %s\n# category: %s\n# detail: %s\n# note: %s\n" %
                    (self.label, category, self.label[len(category) + 1:], self.note))
            f.write("# date: %s\n# source: synthetic (Misc/quakevr/motion_synth.py)\n" % time.strftime("%Y-%m-%d %H:%M:%S"))
            f.write("# map: vrfiringrange\n# vr_world_scale: %g\n# units per metre: %.4f\n" % (self.world_scale, m2u))
            f.write("# vr_height_calibration: %g\n" % self.eye_height)
            (mp, my), (op, oy) = HAND_OFFSETS["main"], HAND_OFFSETS["off"]
            f.write("# hand angles: vr_gunangle %g vr_gunyaw %g vr_offhandpitch %g vr_offhandyaw %g\n" % (mp, my, op, oy))
            if SETTINGS:
                # With a settings line playback sets only it (not the lines above): the scale and height too.
                full = {"vr_world_scale": "%g" % self.world_scale, "vr_height_calibration": "%g" % self.eye_height}
                full.update(SETTINGS)
                f.write("# settings: %s\n" % " ".join("%s=%s" % kv for kv in full.items()))
            f.write("# target: %s\n" % ("vr_dummy %.2f m ahead, %.2f m left" % self.target if self.target else "none"))
            f.write("# rows: %d\n" % len(rows))
            f.write(",".join(columns) + "\n")
            for r in rows:
                f.write(",".join(r[c] for c in columns) + "\n")


# ---------------------------------------------------------------------------------------------------------
# Presets: the categories' motions, one-handed with the main hand unless said

def shoulder(take):
    return (0.0, -0.2, take.eye_height - 0.2)


def sword_swing(take, p0, p1, p2, b0, b1, b2, duration, two_handed):
    """A sword through p0, p1, p2 (the hand, a Bezier) with its blade along b0, b1, b2, the thumb's side (the edge)
    leading: the back of the hand faces motion x blade."""
    last = {"right": (0.0, -1.0, 0.0)}

    def pose(s):
        hand = bezier(p0, p1, p2, s)
        blade = norm(lerp(b0, b1, s * 2)) if s < 0.5 else norm(lerp(b1, b2, s * 2 - 1))
        motion = sub(bezier(p0, p1, p2, min(1.0, s + 0.01)), bezier(p0, p1, p2, max(0.0, s - 0.01)))
        right = cross(motion, blade)
        if dot(right, right) > 1e-8:
            last["right"] = norm(right)
        out = {"main": (hand, blade_pose("main", blade, last["right"]))}
        if two_handed:
            out["off"] = (sub(hand, mul(blade, 0.12)), blade_pose("off", blade, last["right"]))
        return out
    take.start(pose(0.0))
    take.hold(0.4)
    take.move(duration, pose, phase="rec")
    take.hold(0.4)
    take.hold(0.3, phase="tail")


# Where a held weapon's far end (its muzzle point: an axe's head, a sword's tip) lies in the game hand's frame (forward,
# left, up; metres at vr_world_scale 1.25), measured in the mock headset (developer 3's "melee trace") with the
# author's weapon offsets: weapon_pose points it along a swing's axis.
WEAPON_FAR = {"axe": (0.155, -0.012, 0.209), "mjolnir": (0.159, -0.0025, 0.205), "sword": (0.153, 0.003, 0.875),
              "shotgun": (0.45, -0.037, 0.003)}


def weapon_pose(hand, weapon, axis, right):
    """A controller orientation for `weapon` in `hand` whose far end points along `axis`, the game hand's right towards
    `right` as near as it can (player frame)."""
    f, l, u = WEAPON_FAR[weapon]
    game = orient((-l, u, -f), track_dir(axis), (1, 0, 0), track_dir(right))
    return qmul(game, qconj(hand_offset(hand)))


def weapon_swing(take, hand, weapon, path, axes, duration):
    """`weapon` swung by `hand` through `path` (the hand, a Bezier) with its far end along `axes` (start, middle, end),
    the back of the hand facing motion x axis (as sword_swing)."""
    last = {"right": (0.0, 0.0, -1.0)}

    def pose(s):
        h = bezier(path[0], path[1], path[2], s)
        ax = norm(lerp(axes[0], axes[1], s * 2)) if s < 0.5 else norm(lerp(axes[1], axes[2], s * 2 - 1))
        m = sub(bezier(path[0], path[1], path[2], min(1.0, s + 0.01)), bezier(path[0], path[1], path[2], max(0.0, s - 0.01)))
        r = cross(m, ax)
        if dot(r, r) > 1e-8:
            last["right"] = norm(r)
        return {hand: (h, weapon_pose(hand, weapon, ax, last["right"]))}
    take.start(pose(0.0))
    take.hold(0.4)
    take.move(duration, pose, phase="rec")
    take.hold(0.4)
    take.hold(0.3, phase="tail")


def straight(take, hands, duration):
    """Each hand of `hands` ({hand: (from, to, quat)}) moved in a line."""
    take.start({h: (a, q) for h, (a, b, q) in hands.items()})
    take.hold(0.4)
    take.move(duration, lambda s: {h: (lerp(a, b, s), q) for h, (a, b, q) in hands.items()}, phase="rec")
    take.hold(0.4)
    take.hold(0.3, phase="tail")


def fist_pose(hand, forward=(1, 0, 0)):
    """A fist punching along `forward`, its thumb up (or ahead, punching straight down)."""
    d = norm(forward)
    up = norm(cross(cross(d, (0, 0, 1)), d)) if abs(d[2]) < 0.99 else (1, 0, 0)
    return hand_pose(hand, d, up)


def palm_pose(hand):
    """An open palm facing ahead, fingers up (the main hand's thumb to the left, the off hand's to the right)."""
    return hand_pose(hand, (0, 0, 1), (0, 1, 0) if hand == "main" else (0, -1, 0))


def euler(q):
    """vr_mock_hand's angles (pitch up, yaw left, roll) of a tracking-space orientation (vr_backend_mock.cpp
    mockRotation: yaw about +y, then pitch about +x, then roll)."""
    w, x, y, z = q
    pitch = math.degrees(math.asin(max(-1.0, min(1.0, -2 * (y * z - w * x)))))
    yaw = math.degrees(math.atan2(2 * (x * z + w * y), 1 - 2 * (x * x + y * y)))
    roll = math.degrees(math.atan2(2 * (x * y + w * z), 1 - 2 * (x * x + z * z)))
    return pitch, yaw, roll


def write_mock(take, path, lead=0.3):
    """The take as a vr_mock_play script (the head standing at the eye height; the controls are left to the mock):
    for tests whose state a take can't carry (the flashlight in a hand)."""
    with open(path, "w", newline="\n") as f:
        for (t, phase, pose, grips) in take.frames:
            f.write("%.4f head 0 %.4f 0\n" % (t + lead, take.eye_height))
            for hand, (pos, q) in sorted(pose.items()):
                p = track(pos)
                e = euler(q)
                f.write("%.4f %s %.4f %.4f %.4f %.3f %.3f %.3f\n" % (t + lead, hand, p[0], p[1], p[2], e[0], e[1], e[2]))


def settings_from_cfg(path):
    """The hand settings of a config (an ironwail.cfg): what the takes are written for (configure)."""
    keys = ("vr_gunangle", "vr_gunyaw", "vr_offhandpitch", "vr_offhandyaw", "vr_controller_legacy_pose")
    out = {}
    for line in open(path, encoding="utf-8", errors="replace"):
        k, _, v = line.strip().partition(" ")
        if k in keys or k.startswith("vr_handcal_"):
            out[k] = v.strip().strip('"')
    return out


# The melee fixes' tests (ROUND21.md, "Melee fixes: flashlight, axe on walls, gibs"): a weapon chopped into a wall
# (--weapon; vr_motion_play <take> noplace yaw <heading>, standing in front of one), a fist and a weapon struck down at a
# gib on the floor (0.45 m ahead, 3-5 cm left), and the flashlight's punch and shoves (--mock: vr_mock_play, the torch
# taken in the mock first: a take can't carry it).
CHOPS = {
    "horizontal": (((0.2, -0.45, -0.35), (0.62, -0.05, -0.35), (0.3, 0.4, -0.35)), ((0.2, -1, 0.1), (1, 0, 0.1), (0.2, 1, 0.1))),
    "diagonal": (((0.15, -0.35, 0.05), (0.62, -0.05, -0.3), (0.35, 0.3, -0.7)), ((-0.3, -0.5, 0.8), (1, 0.2, 0), (0.4, 0.5, -0.8))),
    "overhead": (((0.1, -0.15, 0.15), (0.62, -0.12, -0.25), (0.4, -0.1, -0.7)), ((-0.3, 0, 1), (1, 0, 0.1), (0.5, 0, -0.9))),
}
CHOP_REACH = {"axe": 0.62, "mjolnir": 0.62, "sword": 0.3, "shotgun": 0.5}  # the hand's farthest (m)


def fix_preset(name, args):
    eye = args.eye_height
    common = dict(rate=args.rate, world_scale=args.world_scale, eye_height=eye)
    if name.startswith("chop_") and name[len("chop_"):] in CHOPS:
        path, axes = CHOPS[name[len("chop_"):]]
        path = tuple((CHOP_REACH[args.weapon] if i == 1 else p[0], p[1], eye + p[2]) for i, p in enumerate(path))
        take = Take("slash_" + name[len("chop_"):] + "_" + args.weapon, main_weapon=args.weapon, target=None,
                    note="synthetic: into a wall", **common)
        weapon_swing(take, "main", args.weapon, path, axes, args.duration)
        return take
    if name == "punch_down_gib":
        take = Take("punch_overhead", target=None, note="synthetic: a gib on the floor", **common)
        take.grip("main", True)
        a, m, b = (0.2, -0.1, eye - 0.45), (0.4, 0.0, 0.5), (0.45, 0.02, 0.06)
        q = fist_pose("main", sub(b, a))
        take.start({"main": (a, q)})
        take.hold(0.4)
        take.move(args.duration, lambda s: {"main": (bezier(a, m, b, s), q)}, phase="rec")
        take.hold(0.4)
        take.hold(0.3, phase="tail")
        return take
    if name == "chop_down_gib":
        take = Take("slash_overhead_" + args.weapon, main_weapon=args.weapon, target=None,
                    note="synthetic: a gib on the floor", **common)
        weapon_swing(take, "main", args.weapon, ((0.15, -0.1, eye), (0.35, 0.0, 0.9), (0.25, 0.03, 0.25)),
                     ((-0.2, 0, 1), (1, 0, 0.1), (0.5, 0.05, -0.85)), args.duration)
        return take
    rest = {"main": ((0.2, -0.25, eye - 0.6), (0.2, -0.25, eye - 0.6), fist_pose("main"))}
    if name == "punch_straight_off":
        take = Take("punch_straight", target=(0.95, 0.1), note="synthetic: the off hand's fist (the flashlight: --mock)",
                    **common)
        take.grip("off", True)
        take.fists = {"off"}
        straight(take, dict(rest, off=((0.12, 0.15, eye - 0.25), (0.6, 0.12, eye - 0.2), fist_pose("off"))), args.duration)
        return take
    if name in ("palm_shove_2h_torch", "palm_shove_torch_only"):
        both = name == "palm_shove_2h_torch"
        take = Take("palm_shove_2h" if both else "no_hit", target=(0.9, 0.0 if both else 0.1),
                    note="synthetic: the off hand holding the flashlight (--mock)", **common)
        take.grip("off", True)
        take.fists = {"off"}
        hands = {"off": ((0.15, 0.15, eye - 0.3), (0.5, 0.15, eye - 0.3), fist_pose("off") if both else palm_pose("off"))}
        hands["main"] = ((0.15, -0.15, eye - 0.3), (0.5, -0.15, eye - 0.3), palm_pose("main")) if both else rest["main"]
        straight(take, hands, args.duration)
        return take
    return None


def preset(name, args):
    two = args.two_handed
    ws, eye = args.world_scale, args.eye_height
    d = args.distance
    T = args.duration
    swords = {
        # (hand path: start, middle, end; blade: start, middle, end)
        "slash_overhead": (((0.1, -0.12, eye + 0.15), (0.45, -0.1, eye - 0.15), (0.35, -0.08, eye - 0.65)),
                           ((-0.8, 0.0, 0.6), (0.95, 0.0, 0.3), (0.65, 0.0, -0.75))),
        "slash_horizontal_rtl": (((0.05, -0.55, eye - 0.35), (0.55, -0.05, eye - 0.35), (0.2, 0.4, eye - 0.35)),
                                 ((0.3, -0.95, 0.05), (1.0, 0.0, 0.02), (0.3, 0.95, 0.05))),
        "slash_horizontal_ltr": (((0.05, 0.3, eye - 0.35), (0.55, 0.0, eye - 0.35), (0.2, -0.6, eye - 0.35)),
                                 ((0.3, 0.95, 0.05), (1.0, 0.0, 0.02), (0.3, -0.95, 0.05))),
        "slash_diagonal_down_left": (((0.05, -0.3, eye + 0.05), (0.5, -0.02, eye - 0.3), (0.3, 0.28, eye - 0.7)),
                                     ((-0.55, -0.35, 0.75), (0.8, 0.6, 0.05), (0.45, 0.55, -0.7))),
        "stab": (((0.12, -0.15, eye - 0.35), (0.3, -0.13, eye - 0.34), (0.5, -0.12, eye - 0.33)),
                 ((1.0, 0.0, 0.05), (1.0, 0.0, 0.05), (1.0, 0.0, 0.05))),
    }
    if name in swords:
        take = Take(name if not two else name + "_2h", rate=args.rate, world_scale=ws, eye_height=eye,
                    main_weapon="sword", target=(d, 0.0), note="synthetic")
        if two:
            take.grip("off", True)
        (p0, p1, p2), (b0, b1, b2) = swords[name]
        sword_swing(take, p0, p1, p2, norm(b0), norm(b1), norm(b2), T, two)
        return take
    if name in ("punch_straight", "no_hit_slow_punch"):
        slow = name.startswith("no_hit")
        take = Take(name, rate=args.rate, world_scale=ws, eye_height=eye, target=(d, 0.0), note="synthetic")
        take.grip("main", True)  # a closed fist (an open hand doesn't punch)
        fist = hand_pose("main", (1, 0, 0), (0, 0, 1))
        a, b = (0.12, -0.15, eye - 0.25), (0.9, -0.1, eye - 0.2)  # 0.9 m: into the dummy's model (0.6 stopped short of it)
        take.start({"main": (a, fist)})
        take.hold(0.4)
        take.move(0.8 if slow else T, lambda s: {"main": (lerp(a, b, s), fist)}, phase="rec")
        take.hold(0.4)
        take.hold(0.3, phase="tail")
        return take
    if name in ("palm_shove_1h", "palm_shove_2h"):
        take = Take(name, rate=args.rate, world_scale=ws, eye_height=eye, target=(d, 0.0), note="synthetic")
        # Fingers up, the palms ahead: the main hand's thumb to the left, the off hand's to the right.
        palm_m = hand_pose("main", (0, 0, 1), (0, 1, 0))
        palm_o = hand_pose("off", (0, 0, 1), (0, -1, 0))
        hands = {"main": ((0.15, -0.15, eye - 0.3), (0.5, -0.15, eye - 0.3), palm_m)}
        if name.endswith("2h"):
            hands["off"] = ((0.15, 0.15, eye - 0.3), (0.5, 0.15, eye - 0.3), palm_o)
        take.start({h: (a, q) for h, (a, b, q) in hands.items()})
        take.hold(0.4)
        take.move(T, lambda s: {h: (lerp(a, b, s), q) for h, (a, b, q) in hands.items()}, phase="rec")
        take.hold(0.4)
        take.hold(0.3, phase="tail")
        return take
    if name == "no_hit_wave":
        take = Take(name, rate=args.rate, world_scale=ws, eye_height=eye, target=(d, 0.0), note="synthetic")
        q = hand_pose("main", (1, 0, 0.3), (0, 0, 1))
        c = (0.35, -0.15, eye - 0.3)
        take.start({"main": (c, q)})
        take.hold(0.3)
        take.move(2.0, lambda s: {"main": ((c[0], c[1] + 0.15 * math.sin(s * 4 * math.pi), c[2] + 0.05 * math.sin(s * 8 * math.pi)), q)},
                  ease=False, phase="rec")
        take.hold(0.3, phase="tail")
        return take
    take = fix_preset(name, args)
    if take:
        return take
    raise SystemExit("no preset %r (--list)" % name)


FIX_PRESETS = ["chop_horizontal", "chop_diagonal", "chop_overhead", "punch_down_gib", "chop_down_gib",
               "punch_straight_off", "palm_shove_2h_torch", "palm_shove_torch_only"]
PRESETS = ["slash_overhead", "slash_horizontal_rtl", "slash_horizontal_ltr", "slash_diagonal_down_left", "stab",
           "punch_straight", "palm_shove_1h", "palm_shove_2h", "no_hit_slow_punch", "no_hit_wave"]
DEFAULT_DURATION = {"stab": 0.15, "punch_straight": 0.15, "palm_shove_1h": 0.15, "palm_shove_2h": 0.15,
                    "chop_horizontal": 0.22, "chop_diagonal": 0.22, "chop_overhead": 0.22, "punch_down_gib": 0.16,
                    "chop_down_gib": 0.22, "punch_straight_off": 0.15, "palm_shove_2h_torch": 0.15,
                    "palm_shove_torch_only": 0.15}


def main():
    ap = argparse.ArgumentParser(description="Synthetic motion takes (docs/vr-port/MOTIONS.md).")
    ap.add_argument("preset", nargs="?", help="a preset, or 'all'")
    ap.add_argument("--list", action="store_true", help="the presets")
    ap.add_argument("--out", default="quakevr/motions/synth", help="the folder the takes go into")
    ap.add_argument("--duration", type=float, help="the motion's seconds (default: 0.3, a thrust's 0.15)")
    ap.add_argument("--distance", type=float, default=1.15, help="metres from the head to the dummy's middle")
    ap.add_argument("--rate", type=float, default=90.0, help="frames a second")
    ap.add_argument("--world-scale", type=float, default=1.25)
    ap.add_argument("--eye-height", type=float, default=1.646)
    ap.add_argument("--two-handed", action="store_true", help="the sword's with the off hand on the grip")
    ap.add_argument("--weapon", default="axe", choices=sorted(WEAPON_FAR), help="the chop presets' weapon")
    ap.add_argument("--settings-from", help="write the takes for this config's hand settings (vr_gunangle, "
                    "vr_handcal_*...: an ironwail.cfg)")
    ap.add_argument("--mock", action="store_true", help="also a vr_mock_play script of each (<take>.mock)")
    ap.add_argument("--name", help="the file's name (no extension; default: the label and the time)")
    args = ap.parse_args()
    if args.list or not args.preset:
        print("\n".join(PRESETS + FIX_PRESETS))
        return
    if args.settings_from:
        configure(settings_from_cfg(args.settings_from))
    names = PRESETS if args.preset == "all" else [args.preset]
    for name in names:
        a = argparse.Namespace(**vars(args))
        a.duration = args.duration or DEFAULT_DURATION.get(name, 0.3)
        take = preset(name, a)
        stamp = time.strftime("%Y-%m-%d_%H-%M-%S")
        path = os.path.join(args.out, "%s_%s.csv" % (take.label, stamp))
        n = 2
        while os.path.exists(path):
            path = os.path.join(args.out, "%s_%s-%d.csv" % (take.label, stamp, n))
            n += 1
        if args.name:
            path = os.path.join(args.out, args.name + (("_" + name) if len(names) > 1 else "") + ".csv")
        take.write(path)
        print(path)
        if args.mock:
            write_mock(take, path[:-4] + ".mock")


if __name__ == "__main__":
    main()
