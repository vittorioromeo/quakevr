# sights.py -- a model's painted sight line, as the engine reads it (Quake/vr/vr_sightalign.cpp sightTexels and
# fromTexels: the sight texels, palette 224-239, 252, 253, as points on frame 0's surface, split along the barrel into
# the rear sight and the front post). Weapon Studio's align and Sight Align measure against this line; the source
# .blend's export (source.py) checks an edit keeps it.

import math

# The models with painted sights and how their rear sight is read (vr_sightalign.cpp `painted`).
PAINTED = {"v_shot.mdl": "notch", "v_shot2.mdl": "ring", "v_light.mdl": "notch", "v_plasma.mdl": "notch"}


def is_sight_index(i):
    return 224 <= i <= 239 or i in (252, 253)


def sight_texels(model, pose=0):
    """The sight texels as points on the pose's surface (model space)."""
    sw, sh = model.skin_size
    skin = model.skins[0][2][0]
    pb = model.pose_verts(model.poses()[pose])
    org, sc = model.origin, model.scale

    def vertex(i):
        return tuple(org[k] + sc[k] * pb[4 * i + k] for k in range(3))

    pts = []
    for ff, a, b, c in model.tris:
        vi = (a, b, c)
        uv = []
        for v in vi:
            onseam, s, t = model.st[v]
            s = float(s)
            if onseam and not ff:
                s += sw // 2
            uv.append((s + 0.5, t + 0.5))
        den = (uv[1][1] - uv[2][1]) * (uv[0][0] - uv[2][0]) + (uv[2][0] - uv[1][0]) * (uv[0][1] - uv[2][1])
        if abs(den) < 1e-9:
            continue
        p = [vertex(v) for v in vi]
        x0 = max(0, int(math.floor(min(u[0] for u in uv))) - 1)
        x1 = min(sw - 1, int(math.ceil(max(u[0] for u in uv))) + 1)
        y0 = max(0, int(math.floor(min(u[1] for u in uv))) - 1)
        y1 = min(sh - 1, int(math.ceil(max(u[1] for u in uv))) + 1)
        for ty in range(y0, y1 + 1):
            for tx in range(x0, x1 + 1):
                if not is_sight_index(skin[ty * sw + tx]):
                    continue
                px, py = tx + 0.5, ty + 0.5
                l0 = ((uv[1][1] - uv[2][1]) * (px - uv[2][0]) + (uv[2][0] - uv[1][0]) * (py - uv[2][1])) / den
                l1 = ((uv[2][1] - uv[0][1]) * (px - uv[2][0]) + (uv[0][0] - uv[2][0]) * (py - uv[2][1])) / den
                l2 = 1.0 - l0 - l1
                if l0 < -1e-5 or l1 < -1e-5 or l2 < -1e-5:
                    continue
                pts.append(tuple(l0 * p[0][k] + l1 * p[1][k] + l2 * p[2][k] for k in range(3)))
    return pts


def sight_line(model, kind=None):
    """(rear, front) points of the painted sights, or None (vr_sightalign.cpp fromTexels)."""
    kind = kind or PAINTED.get(model.name.lower())
    if kind is None:
        return None
    pts = sorted(sight_texels(model), key=lambda p: p[0])
    if len(pts) < 8:
        return None
    rear_end = 1
    while rear_end < len(pts) and pts[rear_end][0] - pts[rear_end - 1][0] <= 1.5:
        rear_end += 1
    front_begin = len(pts) - 1
    while front_begin > 0 and pts[front_begin][0] - pts[front_begin - 1][0] <= 1.5:
        front_begin -= 1
    if rear_end > front_begin:
        return None
    fr = pts[front_begin:]
    mid = [sum(p[k] for p in fr) / len(fr) for k in range(3)]
    front = (mid[0], mid[1], max(p[2] for p in fr))
    rear = [p for p in pts[:rear_end] if abs(p[1] - front[1]) < 1.5]
    if len(rear) < 4:
        return None
    x = sum(p[0] for p in rear) / len(rear)
    if kind == "notch":
        ys = sorted(p[1] for p in rear)
        gap, gap_mid = 0.0, 0.0
        for i in range(1, len(ys)):
            if ys[i] - ys[i - 1] > gap:
                gap, gap_mid = ys[i] - ys[i - 1], 0.5 * (ys[i] + ys[i - 1])
        if gap < 0.15:
            return None
        rear_pt = (x, gap_mid, max(p[2] for p in rear))
    else:
        import numpy as np
        A = np.array([[p[1], p[2], 1.0] for p in rear])
        rhs = np.array([p[1] * p[1] + p[2] * p[2] for p in rear])
        sol = np.linalg.lstsq(A, rhs, rcond=None)[0]
        rear_pt = (x, 0.5 * sol[0], 0.5 * sol[1])
    if front[0] - rear_pt[0] <= 2.0:
        return None
    return rear_pt, front


def angle_deg(line):
    """The sight line's (up, left) angles off the model's x, degrees (vr_sight_lines prints them)."""
    rear, front = line
    d = [front[k] - rear[k] for k in range(3)]
    n = math.sqrt(sum(c * c for c in d))
    u = [c / n for c in d]
    return math.degrees(math.asin(u[2])), math.degrees(math.atan2(u[1], u[0]))
