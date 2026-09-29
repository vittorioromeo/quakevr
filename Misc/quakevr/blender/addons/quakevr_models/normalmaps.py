# normalmaps.py -- which of Quake VR's models get baked normal maps, where the maps go, and baking them from the model
# files as they are now: Misc/quakevr/bake_normals.py and the add-ons' Bake Normal Map (docs/vr-port/ROUND21.md,
# "Baked normal maps"; MODELS_IN_BLENDER.md, HANDS_IN_BLENDER.md). numpy only.
#
# The engine's names (gl_model.c): an alias model's skins read progs/<model>.mdl_<skin>_norm, else skin 0's (one map
# serves every skin: they share the texture coordinates); an MD5 mesh's skin progs/<shader>_<NN>_00_norm, else the
# nearest skin before it that has one (Mod_MD5SharedNormalMap): the body's skin 0 map serves its clothes (skins 0-3),
# skin 4's the armoured torso (4-15).

import os

import numpy as np

try:
    from . import normalbake as nb
    from . import normalbody
    from . import normaldetail
    from . import normaltiles
except ImportError:
    import normalbake as nb
    import normalbody
    import normaldetail
    import normaltiles

HAND = "hand"
BODY = "body"
BODY_BUILDS = ("vrbody.md5mesh", "vrbody_lean.md5mesh", "vrbody_brawny.md5mesh")
GENERATED = ("vrflashlight.mdl", "vrgadget.mdl", "vrgadget_strap.mdl", "vr_shell.mdl", "legholster.mdl",
             "vrpauldron.mdl", "vrpauldron_arm.mdl", "vrpouch.mdl") + tuple("vr_rock%d.mdl" % k for k in range(1, 6)) + \
            tuple("vr_brick%d.mdl" % k for k in range(1, 5))  # make_debris.py's rocks and bricks
HAND_SCALE = 2   # the hand's skin is 512 x 512: its map 1024 x 1024
BODY_SCALE = 4   # the body's is 256 x 256: 1024 x 1024


def all_targets(progs):
    """Every model this bakes, as found in the progs folder: the hand, the body, the generated models, the view models."""
    out = [HAND, BODY]
    out += [m for m in GENERATED if os.path.exists(os.path.join(progs, m))]
    out += sorted(f for f in os.listdir(progs) if f.lower().startswith("v_") and f.lower().endswith(".mdl"))
    return out


def target_of(name):
    """The target a model file (or a target name) belongs to: hand_rig.md5mesh is the hand, any body build the body."""
    b = os.path.basename(name).lower()
    if b in (HAND, "hand_rig.md5mesh", "hand_rig.mdl"):
        return HAND
    if b == BODY or b in BODY_BUILDS or b.startswith("vrbody"):
        return BODY
    return os.path.basename(name)


def outputs(progs, target):
    """The map files a target writes."""
    if target == HAND:
        return [os.path.join(progs, "hand_rig_00_00_norm.png")]
    if target == BODY:
        return [os.path.join(progs, "vrbody_00_00_norm.png"), os.path.join(progs, "vrbody_04_00_norm.png")]
    return [os.path.join(progs, target + "_0_norm.png")]


def hand_skin(progs):
    md5hand = nb._md5hand()
    w, h, px = md5hand.read_lmp(os.path.join(progs, "hand_rig_00_00.lmp"))
    pal = np.array(md5hand.palette(), np.uint8)
    return pal[np.frombuffer(px, np.uint8)].reshape(h, w, 3)


def body_skin(progs, index):
    """The body's skin `index` (vrbody_NN_00.tga, as the author painted it) as RGB, or None."""
    path = os.path.join(progs, "vrbody_%02d_00.tga" % index)
    return nb.read_tga(path) if os.path.exists(path) else None


def body_low(progs):
    """The athletic body, after checking the lean and brawny builds draw their skins with the same texture
    coordinates (one map serves all three)."""
    lows = [nb.md5_low(os.path.join(progs, b), (normalbody.BODY_SKIN, normalbody.BODY_SKIN))
            for b in BODY_BUILDS if os.path.exists(os.path.join(progs, b))]
    for other in lows[1:]:
        if other.T.shape != lows[0].T.shape or not np.allclose(other.UV[other.T], lows[0].UV[lows[0].T], atol=1e-3):
            raise ValueError("%s: its texture coordinates differ from vrbody.md5mesh's: the builds share one normal "
                             "map (vrbody_NN_00_norm.png), so they must share their UVs" % other.name)
    return lows[0]


def bake(progs, target, base=None, details=True):
    """{output path: RGB(A) (h, w, 3 or 4) uint8, alpha the heights parallax mapping walks} for a target, from its model file as it is now. `base(low, raster)`: the
    shape's normals at each entry (model space), a high poly's baked by Blender (normal_blender.py); by default the
    recipe's. `details`: the recipe's relief laid on that shape."""
    if target == HAND:
        skin = hand_skin(progs)
        low = nb.md5_low(os.path.join(progs, "hand_rig.md5mesh"), (skin.shape[1], skin.shape[0]))
        r = nb.Raster(low, HAND_SCALE * 2)
        n = base(low, r) if base else r.n
        alpha = None
        if details:
            grad, heights = normaldetail.hand_recipe(low, r, skin)
            n = nb.bend(n, grad)
            alpha = nb.finish_heights(r, heights, 2)
        img, _ = nb.finish(r, n, supersample=2)
        return {outputs(progs, target)[0]: nb.to_rgba8(img, alpha)}
    if target == BODY:
        low = body_low(progs)
        a, b = outputs(progs, target)
        return {a: nb.to_rgba8(*normalbody.bake(low, BODY_SCALE, False, base, details, body_skin(progs, 0))),
                b: nb.to_rgba8(*normalbody.bake(low, BODY_SCALE, True, base, details, body_skin(progs, 4)))}
    path = os.path.join(progs, target)
    low = nb.mdl_low(path)
    img, alpha = normaltiles.bake(low, nb.skin_rgb(low.model), normaltiles.recipe(target), base=base, details=details)
    return {outputs(progs, target)[0]: nb.to_rgba8(img, alpha)}
