"""Generate skin masks without changing authored diffuse textures (progs/hand_rig_NN_00_skinmask.tga,
progs/vrbody_NN_00_skinmask.tga; vr_render.cpp VR_AliasSkinMask, the alias shader's SkinMaskTex). Channels:
  R  the skin (vr_skin_tone): excludes nails, clothing and static blood;
  G  the fingerless glove (vr_gloves 1): the back and palm of the hand, the fingers' first segment to about its middle
     (the thumb's long first segment and half of its second), static blood left out;
  B  the full glove (vr_gloves 2): the whole hand, nails and all, static blood left out;
  A  the glove's own relief (0.5 flat; none of the skin's painted detail): the seams sewn along the fingers' sides
     (fourchettes) and across their palm side's base, and round the thumb's base, each a groove (down to 0.15) with a
     row of raised stitches (0.8) either side; the shader's leather takes its colour and its bumps from it, its grain
     its own (vr_glsl.h QVR_ALIAS_FS_LIGHT).
G and B are 0 off the glove, 0.55 at its edge rising to 1 a hem's width inside it (the shader darkens that band: the hem).
The body's G and B are 0: its glove cuff over a bare forearm is drawn from the texture coordinates (vr_view.cpp
gloveLook). Run with numpy and Pillow (py 3.13)."""
from pathlib import Path
import numpy as np
from PIL import Image
import sys, struct
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import make_hand_rig as hand
from mdlpolish import palette
PROGS = HERE.parent.parent / 'quakevr/progs'
FINGERLESS = {"thumb": (1, 0.5)}  # the last segment covered and how much of it (the thumb's first is its long base)
FINGERLESS_OTHER = (0, 0.55)  # the other fingers: their first segment to a little past its middle
HEM = 0.25  # hand units (3 mm): the glove's edge band


def grow(m):
    p = np.pad(m, 1)
    return np.logical_or.reduce([p[y:y + m.shape[0], x:x + m.shape[1]] for y in range(3) for x in range(3)])


def spread(val, covered, steps=4):
    """`val` carried out of `covered` into the texels round it (the islands' pads), the mean of the covered
    neighbours, so that filtering at an island's edge reads the island's values."""
    val = val.copy()
    covered = covered.copy()
    H, W = covered.shape
    for _ in range(steps):
        pv = np.pad(val * covered, 1)
        pc = np.pad(covered.astype(float), 1)
        tot = sum(pv[y:y + H, x:x + W] for y in range(3) for x in range(3))
        cnt = sum(pc[y:y + H, x:x + W] for y in range(3) for x in range(3))
        new = ~covered & (cnt > 0)
        val[new] = tot[new] / cnt[new]
        covered = covered | new
    return val


def edge_value(d):
    """A glove texel's value by its distance inside the edge (hand units): 0.55 at the edge, 1 a hem inside."""
    return 0.55 + 0.45 * np.clip(d / HEM, 0, 1)


SEAM = 0.07  # hand units: a seam's groove's half width (under 1 mm)
STITCH_OFF, STITCH_W, STITCH_STEP = 0.17, 0.05, 0.26  # the stitch rows' distance from the seam, their half width, the stitches' pitch


def seam_relief(d, run):
    """A's relief by the distance `d` across a seam's line and `run` along it (hand units): its groove, its stitches."""
    groove = np.clip(1.0 - np.abs(d) / SEAM, 0, 1) ** 1.5
    dots = np.clip(1.0 - np.abs(np.abs(d) - STITCH_OFF) / STITCH_W, 0, 1) * (np.mod(run / STITCH_STEP, 1.0) < 0.6)
    return -0.35 * groove + 0.3 * dots


def seam_lines(rig, name, p, n):
    """A finger's seams at its texels: [(distance across the seam's line, distance along it, weight 0..1)] (hand
    units), and whether the first is the palm side's base seam (the later ones the sides'), for seams() and
    make_glove_paint.py (its stitches along the same lines)."""
    seg, along, side, lateral = hand.finger_coords(rig, name, p, n)
    pts = rig.fingers[name]["points"]
    segs = rig.fingers[name]["segments"]
    r = np.zeros(len(p))
    for k in range(3):
        m = seg == k
        x = segs[k][1]
        q = p[m] - pts[k]
        r[m] = np.linalg.norm(q - np.outer(q @ x, x), axis=1)
    theta = np.arctan2(lateral, side)  # 0 the back, +-pi/2 the sides, +-pi the palm
    lengths = [np.linalg.norm(pts[k + 1] - pts[k]) for k in range(3)]
    run = along + np.where(seg > 0, lengths[0], 0) + np.where(seg > 1, lengths[1], 0)  # along the whole finger
    tip = np.where(seg == 2, np.clip((0.85 * lengths[2] - along) / 0.3, 0, 1), 1.0)  # the seams end before the tip
    if name == "thumb":  # round its base, where it is set into the palm
        return [(along - 0.45 * lengths[0], theta * r, (seg == 0) * 1.0)], False
    sides = [((theta - c) * r, run, tip) for c in (np.pi * 0.58, -np.pi * 0.58)]  # the sides, towards the palm
    base = (along - 0.35, theta * r, (seg == 0) * (np.abs(theta) > np.pi * 0.6) * 1.0)  # across the palm side's base
    return [base] + sides, True


def seams(rig, name, p, n):
    """A finger's texels' relief (0 flat): its seams."""
    lines, has_base = seam_lines(rig, name, p, n)
    if not has_base:
        d, run, w = lines[0]
        return seam_relief(d, run) * w
    out = sum(seam_relief(d, run) for d, run, _ in lines[1:]) * lines[1][2]
    d, run, w = lines[0]
    base = seam_relief(d, run) * w
    return np.where(np.abs(base) > np.abs(out), base, out)


def save(path, r, g, b, a):
    img = np.stack([r, g, b, a], -1)
    Image.fromarray(np.clip(np.round(img * 255), 0, 255).astype(np.uint8), 'RGBA').save(path, compression='tga_rle')


def lmp(path):
    data = path.read_bytes()
    w, h = struct.unpack('<ii', data[:8])
    return np.frombuffer(data[8:], np.uint8).reshape(h, w)


def main():
    models, shifts, rig, mesh, islands = hand.build(str(PROGS))
    layout, size = hand.layout_islands(islands)
    pos, norm, part = hand.rasterize(mesh, layout, size)
    covered = part >= 0
    nails = np.zeros_like(covered)
    relief = np.zeros(covered.shape)
    fingerless = np.where(part == 0, 1.0, 0.0)  # the palm's island: the back and palm of the hand
    for i, name in enumerate(hand.FINGERS):
        m = part == i + 1
        relief[m] = seams(rig, name, pos[m], norm[m])
        seg, along, side, _ = hand.finger_coords(rig, name, pos[m], norm[m])
        pts = rig.fingers[name]['points']
        length = np.linalg.norm(pts[3] - pts[2])
        a = (along - .40 * length) / (.62 * length)
        nails[m] = (seg == 2) & (a > 0) & (a < 1.05) & (side > .5)
        last, share = FINGERLESS.get(name, FINGERLESS_OTHER)
        cut = share * np.linalg.norm(pts[last + 1] - pts[last])
        d = np.where(seg < last, 1e3, np.where(seg == last, cut - along, -1.0))
        fingerless[m] = np.where(d > 0, edge_value(d), 0.0)
    fingerless = spread(fingerless, covered)
    full = spread(np.where(covered, 1.0, 0.0), covered)
    leather = spread(np.where(covered, 0.5 + relief, 0.5), covered)
    skin = grow(grow(covered)) & ~grow(nails)


    pal = palette() / 255.0
    clean = lmp(PROGS / 'hand_rig_00_00.lmp')
    for n in range(4):
        idx = lmp(PROGS / f'hand_rig_{n:02d}_00.lmp')
        kept = ~grow(~(idx == clean))  # static blood (and a texel round it) left out
        mask = skin & kept
        save(PROGS / f'hand_rig_{n:02d}_00_skinmask.tga', mask * 1.0, fingerless * kept, full * kept, leather)
        print(f'hand {n}: {mask.sum()} skin texels; nails excluded {nails.sum()}; fingerless glove '
              f'{(fingerless * kept > 0.3).sum()}, full {(full * kept > 0.3).sum()}; seam texels {(relief < -0.1).sum()}')
    for n in range(16):
        img = np.array(Image.open(PROGS / f'vrbody_{n:02d}_00.tga').convert('RGB'))
        base = np.array(Image.open(PROGS / f'vrbody_{n // 4 * 4:02d}_00.tga').convert('RGB'))
        h, w = img.shape[:2]
        yy, xx = np.mgrid[:h, :w]
        mask = (xx < w * .5) & (yy < h * .4) & ~grow(np.any(img != base, axis=2))
        zero = np.zeros(mask.shape)
        save(PROGS / f'vrbody_{n:02d}_00_skinmask.tga', mask * 1.0, zero, zero, zero + 0.5)  # A flat: the cuff's plain leather
        assert not mask[yy >= h * .4].any()
        print(f'body {n}: {mask.sum()} skin texels, clothing/static blood excluded')


if __name__ == '__main__':
    main()
