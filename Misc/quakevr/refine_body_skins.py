#!/usr/bin/env python3
"""Refine the authored body paint; preserve its markings and all other materials.

Usage: python Misc/quakevr/refine_body_skins.py [source progs folder] [output progs folder]
The source is the author's 256-square repaint (24fa2de6d), NOT the procedural stock skin.
Existing refined sources are rejected so detail is never applied twice. Keep the source or
recover it from that commit when rebaking. Rebuild body meshes with --keep-edited and
bake_normals.py body afterwards; make_vrbody.py's guard protects this authored paint.
"""
from pathlib import Path
import shutil
import struct
import sys
import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / 'blender/addons/quakevr_models'))
import normalbake as nb
from make_vrbody import BLOCKS, SKIN_SIZE, SKIN_HEIGHT, PAD

OLD = {
    'skin': (0., 0., .5, .4), 'head': (0., .4, .5, .5),
    'leather': (.5, 0., 1., .5), 'cloth': (0., .5, .5, 1.),
    'boots': (.5, .5, .625, 1.), 'shaft': (.625, .5, .75, 1.),
    'bracer': (.75, .5, 1., 1.),
}

def refine(paint):
    out = np.zeros((SKIN_HEIGHT, SKIN_SIZE, 3), dtype=np.uint8)
    h, w = paint.shape[:2]
    for name, (u0, v0, u1, v1) in BLOCKS.items():
        ys = np.nonzero((np.arange(SKIN_HEIGHT) + .5 >= v0 * SKIN_HEIGHT) &
                        (np.arange(SKIN_HEIGHT) + .5 < v1 * SKIN_HEIGHT))[0]
        xs = np.nonzero((np.arange(SKIN_SIZE) + .5 >= u0 * SKIN_SIZE) &
                        (np.arange(SKIN_SIZE) + .5 < u1 * SKIN_SIZE))[0]
        s, t = (xs[None, :] + .5) / SKIN_SIZE, (ys[:, None] + .5) / SKIN_HEIGHT
        a, b, c, d = OLD[name]
        # Map the padded usable block as well as its margins (mesh UVs use the same PAD).
        old_s = a + PAD + (s - u0 - PAD) / (u1 - u0 - 2 * PAD) * (c - a - 2 * PAD)
        old_t = b + PAD + (t - v0 - PAD) / (v1 - v0 - 2 * PAD) * (d - b - 2 * PAD)
        x = np.clip(old_s * w - .5, a * w, c * w - 1)
        y = np.clip(old_t * h - .5, b * h, d * h - 1)
        if name not in ('skin', 'bracer'):
            # The torso/head/boots/trousers remain the author's exact RGB texels.
            color = paint[np.rint(y).astype(int), np.rint(x).astype(int)]
        else:
            xi, yi = np.floor(x).astype(int), np.floor(y).astype(int)
            fx, fy = (x - xi)[..., None], (y - yi)[..., None]
            xj, yj = np.minimum(xi + 1, int(c * w) - 1), np.minimum(yi + 1, int(d * h) - 1)
            top = paint[yi, xi] * (1 - fx) + paint[yi, xj] * fx
            bottom = paint[yj, xi] * (1 - fx) + paint[yj, xj] * fx
            color = top * (1 - fy) + bottom * fy
            # Fine grain, independent of the old paint's pixel lattice; do not invent new wounds.
            grain = np.sin(xs[None, :] * 12.9898 + ys[:, None] * 78.233) * 43758.5453
            grain = grain - np.floor(grain) - .5
            blood = (color[..., 0] > color[..., 1] * 1.6) & (color[..., 0] > color[..., 2] * 1.5)
            color *= 1 + grain[..., None] * np.where(blood, .10, .04)[..., None]
        out[np.ix_(ys, xs)] = np.clip(np.rint(color), 0, 255).astype(np.uint8)
    return out

def write_tga(path, rgb):
    h, w = rgb.shape[:2]
    header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, w, h, 24, 32)
    path.write_bytes(header + rgb[..., ::-1].tobytes())

def main():
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE.parent.parent / 'quakevr/progs'
    output = Path(sys.argv[2]) if len(sys.argv) > 2 else HERE.parent.parent / 'quakevr/progs'
    paths = [source / ('vrbody_%02d_00.tga' % i) for i in range(16)]
    paints = [nb.read_tga(str(p)) for p in paths]
    if any(p.shape[:2] != (256, 256) for p in paints):
        raise ValueError('Use the 256-square authored originals; do not refine already-refined paint')
    if source.resolve() == output.resolve():
        backup = HERE.parent.parent / 'scratch/animbody_authored_sources'
        backup.mkdir(parents=True, exist_ok=True)
        for p in paths:
            target = backup / p.name
            if not target.exists():
                shutil.copy2(p, target)
        print('original authored paint preserved in', backup)
    for p, paint in zip(paths, paints):
        write_tga(output / p.name, refine(paint))
    print('refined 16 authored skins to %dx%d; only skin/bracer RGB interpolated and detailed' % (SKIN_SIZE, SKIN_HEIGHT))

if __name__ == '__main__':
    main()
