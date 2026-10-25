// vr_sightalign_table.inc -- the sight lines of the guns without painted sights (vr_sightalign.cpp): model, rear, front
// (model units, as the frames' vertices). A line on the model's middle, parallel to its barrel (its x), from above the
// middle of the fist to the muzzle, 0.05 over the highest part of the mesh between (Misc/quakevr/sightline_table.py); the
// grapple's ends before its claws, the laser cannon's runs along its barrel (below the handle and frame).
// The grunts' burst gun and the enforcers' rifle: their modelled sights (make_enemyguns.py), the rifle's zeroed at 10 m.
{"progs/v_grpple.mdl", {8.09f, 0.f, 8.83f}, {20.00f, 0.f, 8.83f}},
{"progs/v_nail.mdl", {-2.05f, 0.f, 6.08f}, {20.21f, 0.f, 6.08f}},
{"progs/v_lava.mdl", {-2.05f, 0.f, 6.08f}, {20.21f, 0.f, 6.08f}},
{"progs/v_nail2.mdl", {2.46f, 0.f, 7.66f}, {40.08f, 0.f, 7.66f}},
{"progs/v_lava2.mdl", {2.46f, 0.f, 7.69f}, {40.08f, 0.f, 7.69f}},
{"progs/v_rock.mdl", {-1.32f, 0.f, 9.70f}, {30.80f, 0.f, 9.70f}},
{"progs/v_multi.mdl", {-1.32f, 0.f, 9.76f}, {30.80f, 0.f, 9.76f}},
{"progs/v_prox.mdl", {-1.58f, 0.f, 9.72f}, {30.79f, 0.f, 9.72f}},
{"progs/v_rock2.mdl", {5.70f, 0.f, 10.70f}, {56.11f, 0.f, 10.70f}},
{"progs/v_multi2.mdl", {5.70f, 0.f, 10.77f}, {56.11f, 0.f, 10.77f}},
{"progs/v_laserg.mdl", {50.00f, 0.f, -7.86f}, {105.17f, 0.f, -7.86f}},
{"progs/v_gruntgun.mdl", {-3.50f, 0.f, 14.350f}, {22.40f, 0.f, 14.450f}},
{"progs/v_enfrifle.mdl", {6.30f, 0.f, 10.660f}, {23.00f, 0.f, 10.523f}},
// The nail pistol: its modelled sights (build_nailpistol.py, the Body's frame), the rear ears' tops level with the front post's.
{"progs/v_nailpistol.mdl", {-1.25f, 0.f, 6.376f}, {9.07f, 0.f, 6.376f}},
// The flare gun: its modelled sights (build_flaregun.py, the Body's frame), the rear notch's bottom and the front blade's
// top on one line parallel to both bores.
{"progs/v_flaregun.mdl", {1.15f, 0.f, 6.132f}, {11.75f, 0.f, 6.132f}},
// AD support: AD's crossbow (turned, vr_adguns.cpp): along its rail, just over the string's latch (measured on AD 1.80p1).
{"progs/w_dcrossbow.mdl#gun0", {0.00f, 0.f, 4.39f}, {26.00f, 0.f, -0.64f}},
{"progs/w_dcrossbow.mdl#gun", {0.00f, 0.f, 4.39f}, {26.00f, 0.f, -0.64f}},
