// vr_gunvariants.inc -- one model per gun (ROUND25.md, "onemodel"; BACKLOG.md 8): a gun's other-ammo look is its base
// model with another skin, not a model of its own. QVR_GUN_VARIANT(variant, base, skin): the name the game draws for
// the look (QC's weapon table; quakevr/progs/<variant>.mdl is written by Misc/quakevr/make_gun_variants.py build: the
// base's model and parts with `skin`, their normal maps the base's) and the source skin (an indexed PNG on the base's
// skin layout). Everything the engine keeps per model is the base's for a variant (gunvariants::base): its weapons.txt
// slot inherits all of the base's (anchors, hand places, barrel mask and reload settings too: the same triangles), and
// its screen, sights and magazine entries are the base's. A variant's slot keeps only his differences as its own.
// Parsed by make_gun_variants.py too: one call a line, as written.
QVR_GUN_VARIANT("progs/v_lava.mdl", "progs/v_nail.mdl", "Misc/quakevr/src_models/blend/v_nail/v_nail_skin_v_lava.png")
QVR_GUN_VARIANT("progs/v_lava2.mdl", "progs/v_nail2.mdl", "Misc/quakevr/src_models/blend/v_nail2/v_nail2_skin_v_lava2.png")
QVR_GUN_VARIANT("progs/v_multi2.mdl", "progs/v_rock2.mdl", "Misc/quakevr/src_models/blend/v_rock2/v_rock2_skin_v_multi2.png")
QVR_GUN_VARIANT("progs/v_light_plasma.mdl", "progs/v_light.mdl",
    "Misc/quakevr/src_models/blend/v_light/v_light_skin_v_light_plasma.png")
