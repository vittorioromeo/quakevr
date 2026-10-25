"""Make the painted nails' masks (vr_nails; docs/vr-port/GLOVES.md, "Painted nails") on the jointed hand's skin layout,
twice the skin's resolution (1024 x 1024, 3 texels a mm, as the gloves' painted leather):
  progs/hand_rig_0N_00_nails.png  greyscale, the polish's cover: 255 on each nail plate, 0 off it, a soft edge (~0.3 mm)
                                  round it. It follows the plate as make_hand_rig.py paints it (the back of each
                                  finger's last segment): its proximal edge the cuticle's curve (the middle nearest the
                                  knuckle, the corners rounded), its sides just inside the nail's dark folds, out to the
                                  free edge; nothing on the fingertip pads or the skin round the nail. Each bloodied skin's
                                  static blood (hand_rig_01..03_00) is left out (the blood stays over the polish).
The shader (vr_glsl.h QVR_ALIAS_FS_LIGHT) tints the nail by the chosen colour inside it, keeping the hand's own
shading, and adds the polish's gloss. Deterministic. Run with numpy and Pillow (py 3.13):
  py -3.13 Misc/quakevr/make_nail_mask.py [--preview scratch/nails_preview.png]
--preview writes the clean skin with the mask over it in red (for a quick look, not shipped).
Guarded (genguard.py): once a mask is edited by hand it is not overwritten (--keep-edited, --force)."""
from pathlib import Path
import sys
import numpy as np
from PIL import Image
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import genguard
import make_hand_rig as hand
from make_skin_masks import grow, spread, lmp
from mdlpolish import palette

PROGS = HERE.parent.parent / 'quakevr/progs'
SCALE = 2  # texels to the skin's (512: 1.5 a mm) -> 3 a mm
MM = 1.0 / 12.0  # hand units a mm (make_skin_masks.py: HEM 0.25 = 3 mm)
# The plate, in make_hand_rig.py finger_levels' terms: `a` from 40% of the last segment (0) towards its tip (1 at 62%
# of it on), `side` how much the normal faces the back. Its painted dark rim: side < 0.63 or a < 0.08.
CUTICLE_A = 0.10  # the cuticle's middle, just past the painted rim
CUTICLE_BOW = 0.22 # how much further out its corners are (the curve's rise at the plate's sides)
SIDE_EDGE = 0.66  # the plate's sides (just inside the painted folds at 0.63)
TIP_A = 1.0  # the free edge (the painted nail runs to 1.05: the finger's end)
TIP_SIDE = 0.5  # ... where the tip turns down to its pad (the painted nail's own limit)
SOFT_MM = 0.3  # the soft edge's half width


def outputs():
    return [PROGS / f'hand_rig_{n:02d}_00_nails.png' for n in range(4)]


def plate(rig, name, p, n):
    """A finger's texels' polish cover (0..1)."""
    seg, along, side, lateral = hand.finger_coords(rig, name, p, n)
    pts = rig.fingers[name]['points']
    _, x, y, z = rig.fingers[name]['segments'][2]
    l3 = np.linalg.norm(pts[3] - pts[2])
    a = (along - 0.40 * l3) / (0.62 * l3)
    rel = p - pts[2]
    radius = np.maximum(np.sqrt((rel @ y) ** 2 + (rel @ z) ** 2), 0.2)  # from the segment's axis
    # off the back's middle round the finger (the normal across it: at the tip, where the normals turn forward, the
    # plate still runs out to its free edge)
    ny, nz = -(n @ y), n @ z
    ang = np.abs(np.arctan2(nz, ny))
    edge = np.arccos(SIDE_EDGE)
    t = np.clip(ang / edge, 0, 1)
    cut = CUTICLE_A + CUTICLE_BOW * t ** 2  # the cuticle's curve, the corners further out
    d_side = (edge - ang) * radius  # hand units inside the sides
    d_cut = (a - cut) * 0.62 * l3  # ... past the cuticle
    d_tip = np.minimum((TIP_A - a) * 0.62 * l3, (side - TIP_SIDE) * radius)  # ... before the free edge (round the tip)
    d = np.minimum(np.minimum(d_side, d_cut), d_tip)
    # round the corners: where both edges are near, their distances combined
    near = (d_side < 2 * SOFT_MM * MM * 3) & (d_cut < 2 * SOFT_MM * MM * 3)
    r = 3 * SOFT_MM * MM
    corner = r - np.sqrt(np.maximum(r - d_side, 0) ** 2 + np.maximum(r - d_cut, 0) ** 2)
    d = np.where(near, np.minimum(d, corner), d)
    cover = np.clip(0.5 + d / (2 * SOFT_MM * MM), 0, 1)
    cover = cover * cover * (3 - 2 * cover)
    return np.where((seg == 2) & (side > 0.0) & (ny * ny + nz * nz > 0.04), cover, 0.0)


def main():
    preview = None
    if '--preview' in sys.argv:
        preview = sys.argv[sys.argv.index('--preview') + 1]
    guard = genguard.Guard("make_nail_mask.py", [str(o) for o in outputs()])
    models, shifts, rig, mesh, islands = hand.build(str(PROGS))
    layout, size = hand.layout_islands(islands)
    layout = {k: (x * SCALE, y * SCALE, d * SCALE) for k, (x, y, d) in layout.items()}
    size = (size[0] * SCALE, size[1] * SCALE)
    pos, norm, part = hand.rasterize(mesh, layout, size)
    covered = part >= 0
    cover = np.zeros(covered.shape)
    for i, name in enumerate(hand.FINGERS):
        m = part == i + 1
        cover[m] = plate(rig, name, pos[m], norm[m])
    cover = np.clip(spread(cover, covered, 6), 0, 1)
    clean = lmp(PROGS / 'hand_rig_00_00.lmp')
    for n, out in enumerate(outputs()):
        idx = lmp(PROGS / f'hand_rig_{n:02d}_00.lmp')
        kept = ~grow(~(idx == clean))  # static blood (and a texel round it) left out
        kept = np.repeat(np.repeat(kept, SCALE, 0), SCALE, 1)
        mask = cover * kept
        Image.fromarray(np.round(mask * 255).astype(np.uint8), 'L').save(out, optimize=True)
        print(f'nails {n}: {(mask > 0.5).sum()} texels covered, {((mask > 0.02) & (mask < 0.98)).sum()} on the edge')
    guard.finish()
    if preview:
        pal = palette()
        skin = pal[np.repeat(np.repeat(clean, SCALE, 0), SCALE, 1)] / 255.0
        red = np.array([1.0, 0.1, 0.15])
        img = skin * (1 - 0.7 * cover[..., None]) + red * 0.7 * cover[..., None]
        Image.fromarray(np.round(img * 255).astype(np.uint8), 'RGB').save(preview)


if __name__ == '__main__':
    main()
