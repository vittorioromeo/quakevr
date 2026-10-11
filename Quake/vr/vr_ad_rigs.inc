// Arcane Dimensions' monsters' ragdoll rigs (AD support; docs/vr-port/ARCANE_DIMENSIONS.md section 16): their seed
// tables, included twice by vr_ragdoll.cpp: with QVR_AD_RIG_SEEDS the seeds (inside its anonymous namespace, as its own
// tables), without it their rows of seedTables[]. AD's models are read from AD's paks (progs/mon_*.mdl, model ids in
// vr_ad_modelids.inc). Made as Quake VR's own (vr_ragdoll.cpp, "The seed tables"): RIG_PAK=<a pak of AD's monster
// models> Misc/quakevr/ragdoll/rig.py mon_<name> [k], then rig.py mon_<name> mon_<name>_bones.json; the death frames
// from AD's $frame lists (QC/ad/ad_mon_<name>.qc). The rest pose is frame 0 ($stand1): x forward, y left, z up.
#ifdef QVR_AD_RIG_SEEDS

// AD's death knight (progs/mon_dknight.mdl: 911 vertices, 244 frames; deaths 212-223, 224-243): 24 clusters (rig.py
// mon_dknight 24, clusters 0.61 units rms, bones 1.09: mon_dknight_bones.json); the pauldrons on the chest (they move
// with it); the right arm two bones, the sword hand the second (his sword arm barely bends: a forearm bone of its own
// got no vertices). The death prince's model is the same mesh (progs/mon_dprince.mdl, the same frames).
constexpr Seed adDknightSeeds[] = {
    {"pelvis", -1, Joint::Root, {1.9f, -0.6f, 7.7f}, {1.9f, -0.6f, 7.7f}, {1.6f, 0.1f, 15.9f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {5.1f, -0.2f, 26.5f}, {1.6f, 0.1f, 15.9f}, {6.1f, -0.1f, 26.9f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {8.2f, -0.3f, 32.3f}, {6.1f, -0.1f, 26.9f}, {10.4f, -0.5f, 37.7f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {0.5f, 12.7f, 14.3f}, {4.3f, 11.2f, 23.1f}, {4.2f, 14.9f, 10.5f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {9.2f, 14.7f, 4.f}, {4.2f, 14.9f, 10.5f}, {14.1f, 14.5f, -2.4f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {5.8f, -15.8f, 10.4f}, {3.1f, -11.8f, 23.6f}, {12.8f, -11.7f, 5.6f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {16.1f, -18.4f, 10.4f}, {12.8f, -11.7f, 5.6f}, {19.4f, -25.1f, 15.2f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {1.6f, 5.5f, -4.f}, {-0.2f, 0.3f, 6.2f}, {1.3f, 6.2f, -11.1f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {-1.5f, 8.2f, -20.1f}, {1.3f, 6.2f, -11.1f}, {2.1f, 10.1f, -23.6f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"foot_l", 8, Joint::Ball, {6.7f, 9.6f, -23.1f}, {2.1f, 10.1f, -23.6f}, {11.3f, 9.1f, -22.6f}, 0.f, 35.f, 15.f, 0.f, {}},
    {"thigh_r", 0, Joint::Ball, {5.f, -6.4f, -3.5f}, {1.8f, -0.5f, 7.4f}, {4.2f, -7.7f, -12.5f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 10, Joint::Hinge, {6.3f, -10.f, -21.9f}, {4.2f, -7.7f, -12.5f}, {6.1f, -9.8f, -21.1f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

#else

// A row: the model's id, its vertex count, its seeds, its death animations (1 or 2: first and last frames of each), its
// motion clusters (rig.py's k: 18, more when its bones were made from more).
#define QVR_AD_RIG(id, verts, seeds, deaths, firstA, firstB, lastA, lastB, k) \
    {modelmeta::Id::id, verts, seeds, static_cast<int>(sizeof(seeds) / sizeof(seeds[0])), deaths, {firstA, firstB}, {lastA, lastB}, k},
QVR_AD_RIG(AdDknight, 911, adDknightSeeds, 2, 212, 224, 223, 243, 24)
QVR_AD_RIG(AdDprince, 911, adDknightSeeds, 2, 212, 224, 223, 243, 24)
// AD's soldiers (progs/mon_soldier.mdl: Quake VR's grunt's mesh, 555 vertices, id's 114 frames: Quake VR's appends 6):
// the grunt's seeds, his deaths 8-17, 18-28.
QVR_AD_RIG(AdSoldier, 555, gruntSeeds, 2, 8, 18, 17, 28, 18)
#undef QVR_AD_RIG

#endif
