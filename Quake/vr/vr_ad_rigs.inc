// Arcane Dimensions' monsters' ragdoll rigs (AD support; docs/vr-port/ARCANE_DIMENSIONS.md section 17): their seed
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

// AD's zombie knight (progs/mon_zombiek.mdl: 161 vertices; 24 clusters, bones 0.87: mon_zombiek_bones.json); its
// knees and right hip set by hand (low-poly legs: the fit put them at the feet).
constexpr Seed adZombiekSeeds[] = {
    {"pelvis", -1, Joint::Root, {-0.7f, 0.3f, -0.1f}, {-0.7f, 0.3f, -0.1f}, {-2.1f, 1.4f, 7.2f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {0.6f, 0.8f, 16.2f}, {-2.1f, 1.4f, 7.2f}, {2.1f, 4.5f, 19.5f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {4.7f, 5.3f, 21.8f}, {2.1f, 4.5f, 19.5f}, {7.3f, 6.f, 24.2f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {2.f, 7.9f, 11.3f}, {2.2f, 3.9f, 14.1f}, {1.5f, 10.3f, 8.2f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {-0.4f, 9.5f, -2.4f}, {1.5f, 10.3f, 8.2f}, {-2.4f, 8.8f, -13.1f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {-2.6f, -7.9f, 12.2f}, {1.2f, -5.2f, 16.8f}, {-5.2f, -11.7f, 6.f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {1.3f, -9.8f, 1.1f}, {-5.2f, -11.7f, 6.f}, {7.9f, -7.9f, -3.9f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {0.6f, 7.1f, -12.f}, {-1.6f, 6.4f, -4.9f}, {-3.5f, 6.5f, -18.f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {-4.2f, 7.2f, -22.9f}, {-3.5f, 6.5f, -18.f}, {-5.f, 7.f, -24.f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {3.f, -4.9f, -10.f}, {2.f, -5.f, -5.f}, {2.5f, -5.5f, -18.f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {3.9f, -5.3f, -23.f}, {2.5f, -5.5f, -18.f}, {4.5f, -6.f, -24.f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's crossbow knight (progs/mon_dcrossbow.mdl: 1120 vertices; 24 clusters, bones 0.88): the crossbow in the right
// hand (its pivot at the wrist, set by hand: the left hand holds its front). The crossbow is loose (dropped in his
// deaths: vr_monstermods.cpp), so the hand's middle is the fist's (it was the bow's: no cluster went to it).
constexpr Seed adDcrossbowSeeds[] = {
    {"pelvis", -1, Joint::Root, {1.7f, -0.5f, 0.7f}, {1.7f, -0.5f, 0.7f}, {2.f, 0.9f, 9.4f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {0.4f, -4.4f, 17.6f}, {2.f, 0.9f, 9.4f}, {5.f, -1.1f, 18.8f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {6.3f, -1.3f, 21.3f}, {5.f, -1.1f, 18.8f}, {7.7f, -1.5f, 23.7f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {9.6f, 4.5f, 15.4f}, {4.6f, 1.6f, 17.1f}, {10.9f, 4.f, 9.f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {10.2f, -1.8f, 1.3f}, {10.9f, 4.f, 9.f}, {9.4f, -7.5f, -6.5f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {-7.2f, -9.9f, 13.8f}, {-3.f, -7.6f, 18.5f}, {-4.4f, -13.f, 8.f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {0.2f, -10.5f, 6.3f}, {-4.4f, -13.f, 8.f}, {2.5f, -10.f, 5.f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 6, Joint::Ball, {3.5f, -7.5f, -2.5f}, {2.5f, -10.f, 5.f}, {27.1f, -17.1f, -1.6f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {4.9f, 3.4f, -7.7f}, {1.3f, 2.6f, -0.4f}, {3.6f, 3.5f, -16.f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 8, Joint::Hinge, {6.3f, 5.f, -22.2f}, {3.6f, 3.5f, -16.f}, {5.7f, 4.7f, -21.f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {1.f, -6.9f, -7.2f}, {0.4f, -2.f, 1.5f}, {1.f, -5.4f, -15.6f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 10, Joint::Hinge, {-0.2f, -9.6f, -22.2f}, {1.f, -5.4f, -15.6f}, {0.f, -8.9f, -21.f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's hunter ogre, the cleaver's (progs/mon_hogre.mdl: 1017 vertices; 24 clusters, bones 1.04): the long hammer in
// the right hand (its pivot at the grip, set by hand), the pauldron on the chest.
constexpr Seed adHogreSeeds[] = {
    {"pelvis", -1, Joint::Root, {1.f, 0.f, 5.5f}, {1.f, 0.f, 5.5f}, {2.f, -0.2f, 13.f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {7.3f, 5.9f, 23.4f}, {2.f, -0.2f, 13.f}, {6.6f, -1.2f, 24.4f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {10.8f, -0.5f, 27.f}, {6.6f, -1.2f, 24.4f}, {15.1f, 0.2f, 29.6f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {3.7f, 14.3f, 17.3f}, {6.5f, 12.4f, 22.9f}, {2.f, 17.4f, 7.6f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {9.1f, 14.3f, 1.2f}, {2.f, 17.4f, 7.6f}, {16.3f, 11.2f, -5.2f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {5.6f, -14.3f, 14.f}, {6.4f, -11.3f, 20.4f}, {7.8f, -16.1f, 1.8f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {2.2f, -17.9f, -2.9f}, {7.8f, -16.1f, 1.8f}, {2.5f, -17.5f, -6.5f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 6, Joint::Ball, {-6.f, -18.f, -10.f}, {2.5f, -17.5f, -6.5f}, {-32.3f, -18.9f, -18.3f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {5.3f, 7.1f, -10.4f}, {-1.5f, 5.f, 2.4f}, {0.4f, 10.5f, -18.8f}, 4.f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 8, Joint::Hinge, {3.6f, 8.4f, -22.3f}, {0.4f, 10.5f, -18.8f}, {1.8f, 9.6f, -20.3f}, 3.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {5.3f, -7.1f, -10.4f}, {-0.9f, -7.8f, 2.4f}, {0.6f, -4.8f, -19.7f}, 4.f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 10, Joint::Hinge, {3.6f, -8.4f, -22.3f}, {0.6f, -4.8f, -19.7f}, {1.3f, -5.7f, -20.3f}, 3.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's death guard (progs/mon_dguard.mdl: 585 vertices; 24 clusters, bones 0.72): the sword in the right hand, loose
// (dropped in its deaths: vr_monstermods.cpp): the hand bone the fist (vr_ragdoll.cpp adoptPastPivot).
constexpr Seed adDguardSeeds[] = {
    {"pelvis", -1, Joint::Root, {2.1f, -0.5f, 0.8f}, {2.1f, -0.5f, 0.8f}, {1.4f, -0.1f, 8.1f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {3.3f, -0.2f, 15.4f}, {1.4f, -0.1f, 8.1f}, {5.5f, -0.1f, 19.4f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {6.8f, -0.1f, 22.f}, {5.5f, -0.1f, 19.4f}, {8.1f, -0.1f, 24.6f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {3.1f, 8.6f, 17.f}, {4.f, 5.2f, 16.9f}, {1.6f, 7.8f, 10.8f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {5.2f, 12.1f, 3.3f}, {1.6f, 7.8f, 10.8f}, {8.8f, 16.4f, -4.2f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {3.3f, -9.1f, 17.1f}, {4.4f, -5.3f, 18.3f}, {1.9f, -11.f, 12.9f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {6.2f, -13.9f, 4.2f}, {1.9f, -11.f, 12.9f}, {10.2f, -11.8f, 0.7f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 6, Joint::Ball, {13.7f, -15.7f, 5.1f}, {10.2f, -11.8f, 0.7f}, {17.2f, -19.6f, 9.4f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {1.1f, 5.1f, -7.5f}, {-0.2f, 2.3f, 0.6f}, {-0.3f, 5.9f, -15.1f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 8, Joint::Hinge, {1.5f, 7.7f, -22.4f}, {-0.3f, 5.9f, -15.1f}, {1.2f, 7.4f, -21.2f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {4.7f, -5.8f, -8.6f}, {1.7f, -3.f, 0.9f}, {3.5f, -7.5f, -15.f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 10, Joint::Hinge, {5.4f, -8.7f, -22.4f}, {3.5f, -7.5f, -15.f}, {5.1f, -8.5f, -21.2f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's hunter ogre, the mace's (progs/mon_hogremac.mdl: 1224 vertices; 24 clusters, bones 1.09): the mace in the right
// hand, loose (dropped in his deaths: vr_monstermods.cpp): the hand bone the fist (vr_ragdoll.cpp adoptPastPivot).
constexpr Seed adHogremacSeeds[] = {
    {"pelvis", -1, Joint::Root, {1.2f, 0.6f, 6.f}, {1.2f, 0.6f, 6.f}, {2.1f, 0.7f, 14.1f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {7.4f, -0.5f, 23.f}, {2.1f, 0.7f, 14.1f}, {5.7f, -0.1f, 23.7f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {10.3f, -0.8f, 26.3f}, {5.7f, -0.1f, 23.7f}, {14.9f, -1.6f, 28.9f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {3.6f, 14.4f, 17.3f}, {6.6f, 12.f, 23.f}, {1.7f, 17.9f, 7.2f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {9.1f, 14.3f, 1.2f}, {1.7f, 17.9f, 7.2f}, {16.6f, 10.8f, -4.9f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {5.3f, -14.8f, 13.2f}, {5.9f, -12.2f, 19.1f}, {4.3f, -15.7f, 2.8f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {6.2f, -17.7f, -4.4f}, {4.3f, -15.7f, 2.8f}, {4.6f, -18.7f, -4.1f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 6, Joint::Ball, {-11.8f, -15.f, -12.5f}, {4.6f, -18.7f, -4.1f}, {-28.2f, -11.3f, -20.8f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {5.3f, 7.2f, -10.3f}, {-1.7f, 4.3f, 2.6f}, {0.4f, 10.f, -18.9f}, 4.f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 8, Joint::Hinge, {3.4f, 8.4f, -22.3f}, {0.4f, 10.f, -18.9f}, {1.6f, 9.3f, -20.3f}, 3.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {4.2f, -7.2f, -7.1f}, {-1.f, -5.8f, 3.1f}, {1.6f, -6.4f, -18.1f}, 4.f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 10, Joint::Hinge, {3.4f, -8.4f, -22.3f}, {1.6f, -6.4f, -18.1f}, {2.6f, -7.4f, -20.3f}, 3.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's spiders (progs/mon_spiderg.mdl: 346 vertices; whole pieces, 18 clusters, bones 3.2: its legs bend, each one
// bone): its body (abdomen, head and fangs: a head bone of its own flew apart, unstable) the pelvis; the legs' pivots
// at their roots, by hand.
constexpr Seed adSpiderSeeds[] = {
    {"pelvis", -1, Joint::Root, {-5.1f, -0.1f, -14.8f}, {-5.1f, -0.1f, -14.8f}, {9.f, 0.f, -15.f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"leg_fl", 0, Joint::Ball, {8.4f, 7.1f, -14.f}, {5.f, 2.5f, -14.f}, {14.1f, 12.2f, -9.7f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_ml", 0, Joint::Ball, {2.7f, 9.1f, -13.5f}, {2.f, 3.f, -14.f}, {5.f, 16.f, -8.4f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_ml2", 0, Joint::Ball, {-1.8f, 9.9f, -13.9f}, {-1.f, 3.f, -14.f}, {-1.3f, 17.f, -9.3f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_bl", 0, Joint::Ball, {-9.2f, 7.7f, -13.7f}, {-4.f, 2.5f, -14.f}, {-17.6f, 10.9f, -10.7f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_fr", 0, Joint::Ball, {6.1f, -6.9f, -13.4f}, {5.f, -2.5f, -14.f}, {9.4f, -13.4f, -7.8f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_mr", 0, Joint::Ball, {1.8f, -9.2f, -13.5f}, {2.f, -3.f, -14.f}, {3.2f, -15.f, -8.5f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_mr2", 0, Joint::Ball, {-2.5f, -9.3f, -13.6f}, {-1.f, -3.f, -14.f}, {-3.1f, -16.3f, -8.8f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_br", 0, Joint::Ball, {-7.2f, -8.5f, -13.3f}, {-4.f, -2.5f, -14.f}, {-11.9f, -13.4f, -8.3f}, 0.f, 60.f, 25.f, 0.f, {}},
};

// AD's skull wizard (progs/mon_skullwiz.mdl: 632 vertices, its first frame its death's; 18 clusters, bones 1.74): a
// robe, no legs: its lower half one bone (thigh_l).
constexpr Seed adSkullwizSeeds[] = {
    {"pelvis", -1, Joint::Root, {-1.6f, -4.4f, -11.4f}, {-1.6f, -4.4f, -11.4f}, {1.f, 2.5f, -0.2f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {1.9f, -0.4f, 9.f}, {1.f, 2.5f, -0.2f}, {3.3f, -1.7f, 17.7f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {2.2f, -3.3f, 23.1f}, {3.3f, -1.7f, 17.7f}, {1.1f, -4.8f, 28.5f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {8.3f, 8.1f, 13.7f}, {2.3f, 5.6f, 15.9f}, {12.1f, 8.3f, 17.5f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {18.6f, 5.6f, 13.7f}, {12.1f, 8.3f, 17.5f}, {25.1f, 2.8f, 9.9f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {-2.1f, -10.6f, 17.5f}, {-2.7f, -14.4f, 13.7f}, {-7.f, -9.8f, 6.f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {-7.5f, -6.1f, -2.f}, {-7.f, -9.8f, 6.f}, {-7.9f, -2.5f, -10.1f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {11.2f, 5.f, -12.1f}, {5.1f, 3.9f, 0.9f}, {17.3f, 6.2f, -25.1f}, 0.f, 30.f, 20.f, 0.f, {}},
};

// AD's hunter ogre, the hammer's (progs/mon_hogreham.mdl: 1306 vertices; 24 clusters, bones 1.30): the hammer on his
// shoulder in the right hand, loose (dropped in his deaths: vr_monstermods.cpp): the hand bone the fist
// (vr_ragdoll.cpp adoptPastPivot).
constexpr Seed adHogrehamSeeds[] = {
    {"pelvis", -1, Joint::Root, {-0.3f, -0.5f, 6.6f}, {-0.3f, -0.5f, 6.6f}, {-0.2f, 0.2f, 13.9f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {3.5f, 0.f, 25.1f}, {-0.2f, 0.2f, 13.9f}, {1.2f, 2.1f, 22.f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {5.1f, -0.3f, 24.4f}, {1.2f, 2.1f, 22.f}, {9.1f, -2.8f, 26.8f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {1.4f, 13.5f, 17.f}, {2.6f, 12.1f, 23.1f}, {0.6f, 16.6f, 7.9f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {6.8f, 14.9f, -0.3f}, {0.6f, 16.6f, 7.9f}, {13.1f, 13.1f, -8.4f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {3.1f, -13.f, 19.1f}, {2.4f, -12.f, 20.7f}, {6.8f, -17.9f, 12.4f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {14.2f, -13.5f, 11.3f}, {6.8f, -17.9f, 12.4f}, {15.3f, -8.9f, 12.9f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 6, Joint::Ball, {8.8f, -9.7f, 30.7f}, {15.3f, -8.9f, 12.9f}, {2.3f, -10.5f, 48.5f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {1.5f, 5.9f, -7.5f}, {-2.4f, 4.f, 3.4f}, {-2.5f, 9.5f, -16.7f}, 4.f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 8, Joint::Hinge, {-0.4f, 5.9f, -22.4f}, {-2.5f, 9.5f, -16.7f}, {-1.1f, 7.1f, -20.5f}, 3.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {2.7f, -6.1f, -7.3f}, {-1.9f, -5.3f, 3.5f}, {-0.2f, -6.8f, -17.7f}, 4.f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 10, Joint::Hinge, {1.5f, -6.1f, -22.4f}, {-0.2f, -6.8f, -17.7f}, {0.8f, -6.4f, -20.5f}, 3.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's minotaur (progs/mon_minotaur.mdl: 514 vertices, its first frame a walk's; 24 clusters, bones 1.79): a spine bone
// between pelvis and chest (it bends); hips, spine and right knee set by hand.
constexpr Seed adMinotaurSeeds[] = {
    {"pelvis", -1, Joint::Root, {-4.6f, 1.3f, 14.2f}, {-4.6f, 1.3f, 14.2f}, {-6.f, -3.f, 24.f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"spine", 0, Joint::Ball, {-10.6f, -7.1f, 35.2f}, {-6.f, -3.f, 24.f}, {-10.7f, -4.f, 49.2f}, 0.f, 30.f, 20.f, 0.f, {}},
    {"chest", 1, Joint::Ball, {-5.1f, -8.6f, 61.5f}, {-10.7f, -4.f, 49.2f}, {6.9f, -11.8f, 63.3f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 2, Joint::Ball, {15.f, -12.5f, 57.f}, {6.9f, -11.8f, 63.3f}, {23.2f, -13.2f, 50.7f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 2, Joint::Ball, {2.4f, 10.6f, 53.3f}, {-1.5f, 4.8f, 57.3f}, {7.3f, 20.5f, 50.1f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 4, Joint::Hinge, {23.3f, 28.f, 61.7f}, {7.3f, 20.5f, 50.1f}, {39.3f, 35.4f, 73.3f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 2, Joint::Ball, {-7.9f, -21.2f, 48.4f}, {-12.f, -15.5f, 56.5f}, {-10.5f, -28.6f, 36.7f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 6, Joint::Hinge, {-2.5f, -27.6f, 18.f}, {-10.5f, -28.6f, 36.7f}, {5.5f, -26.5f, -0.8f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {-16.8f, 2.2f, -0.1f}, {-12.f, 5.f, 10.f}, {-15.1f, 4.5f, -2.1f}, 5.f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 8, Joint::Hinge, {-17.4f, 6.7f, -11.6f}, {-15.1f, 4.5f, -2.1f}, {-18.f, 7.4f, -14.3f}, 4.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {-6.6f, -12.5f, 8.1f}, {-8.f, -8.f, 14.f}, {-22.f, -12.f, 0.f}, 5.f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 10, Joint::Hinge, {-24.1f, -11.f, -5.8f}, {-22.f, -12.f, 0.f}, {-20.4f, -11.9f, -16.9f}, 4.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"foot_r", 11, Joint::Ball, {-14.8f, -12.5f, -21.f}, {-20.4f, -11.9f, -16.9f}, {-9.2f, -13.1f, -25.1f}, 0.f, 35.f, 15.f, 0.f, {}},
};

// AD's death fury (progs/mon_dfury.mdl: 1376 vertices; 24 clusters, bones 1.20): a sword in each hand (each forearm
// bone with its hand and sword); hips and knees set by hand.
constexpr Seed adDfurySeeds[] = {
    {"pelvis", -1, Joint::Root, {0.9f, 0.4f, 6.6f}, {0.9f, 0.4f, 6.6f}, {0.9f, -0.2f, 16.3f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {0.3f, -0.7f, 28.f}, {0.9f, -0.2f, 16.3f}, {2.2f, -0.5f, 27.6f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {3.9f, -0.6f, 35.8f}, {2.2f, -0.5f, 27.6f}, {5.5f, -0.6f, 44.1f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {3.6f, 13.7f, 10.9f}, {-3.f, 10.3f, 25.3f}, {6.8f, 12.4f, 8.6f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {14.5f, 8.6f, 12.9f}, {6.8f, 12.4f, 8.6f}, {22.2f, 4.9f, 17.3f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {1.9f, -13.5f, 18.6f}, {3.2f, -12.1f, 25.8f}, {6.1f, -17.f, 7.4f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {11.9f, -12.2f, 1.f}, {6.1f, -17.f, 7.4f}, {17.7f, -7.4f, -5.3f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {-4.9f, 6.f, -8.7f}, {-1.f, 4.f, 2.f}, {-4.5f, 7.f, -15.f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {-4.5f, 9.3f, -21.9f}, {-4.5f, 7.f, -15.f}, {-4.4f, 8.7f, -21.3f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {6.6f, -5.7f, -4.8f}, {2.f, -4.f, 2.f}, {9.f, -7.f, -15.f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {11.4f, -7.7f, -21.9f}, {9.f, -7.f, -15.f}, {10.9f, -8.8f, -21.3f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's enforcer, id's model (progs/mon_enforcer.mdl: 222 vertices; 18 clusters, bones 1.07): the defender and the
// eliminator; its muzzle effect and gun hidden on death. The left forearm is refit without the gun; the hips set by hand,
// the knees as Quake VR's enforcer's (rig.py's 'knee': the ring's lower edge, z -11.7 and -11.5, not -16; the shins
// down to the ankle, keepHinge).
constexpr Seed adEnforcerSeeds[] = {
    {"pelvis", -1, Joint::Root, {-5.9f, -0.7f, 5.f}, {-5.9f, -0.7f, 5.f}, {-5.9f, 2.9f, 6.f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-9.2f, -0.2f, 18.f}, {-5.9f, 2.9f, 6.f}, {-5.7f, -1.6f, 25.3f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {-3.9f, -1.f, 28.4f}, {-5.7f, -1.6f, 25.3f}, {-2.1f, -0.4f, 31.5f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-3.9f, 14.2f, 13.6f}, {-5.6f, 11.4f, 15.3f}, {0.6f, 13.4f, 12.1f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {7.1f, 12.2f, 13.2f}, {0.6f, 13.4f, 12.1f}, {13.5f, 11.f, 14.3f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {-3.2f, -16.2f, 17.6f}, {-6.7f, -13.1f, 22.9f}, {0.4f, -17.3f, 14.7f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {2.7f, -8.5f, 12.1f}, {0.4f, -17.3f, 14.7f}, {5.1f, 0.4f, 9.4f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {-3.9f, 6.1f, -10.7f}, {-4.8f, 2.8f, -5.3f}, {-3.9f, 6.2f, -11.7f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {-2.8f, 7.9f, -22.2f}, {-3.9f, 6.2f, -11.7f}, {-6.7f, 7.8f, -21.3f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}, true},
    {"thigh_r", 0, Joint::Ball, {-4.4f, -7.7f, -10.7f}, {-3.7f, -8.3f, -5.f}, {-4.4f, -7.9f, -11.5f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {-4.1f, -9.7f, -22.1f}, {-4.4f, -7.9f, -11.5f}, {-8.f, -9.3f, -21.3f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}, true},
};

// AD's gargoyle (progs/mon_gargoyle.mdl: 338 vertices, its first frame its death's; 18 clusters, bones 2.15): two wing
// bones; chest and hips set by hand.
constexpr Seed adGargoyleSeeds[] = {
    {"pelvis", -1, Joint::Root, {-14.7f, -0.7f, 2.1f}, {-14.7f, -0.7f, 2.1f}, {-11.f, 0.f, 8.f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-5.8f, 2.9f, 15.9f}, {-11.f, 0.f, 8.f}, {1.4f, -4.6f, 18.3f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {9.4f, -1.6f, 19.9f}, {1.4f, -4.6f, 18.3f}, {17.4f, 1.3f, 21.5f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"wing_l", 1, Joint::Ball, {-15.2f, 22.3f, 40.2f}, {-3.2f, 12.1f, 28.7f}, {-27.2f, 32.4f, 51.6f}, 0.f, 40.f, 20.f, 0.f, {}},
    {"wing_r", 1, Joint::Ball, {-16.3f, -25.1f, 40.4f}, {-1.8f, -14.4f, 30.8f}, {-30.8f, -35.8f, 50.f}, 0.f, 40.f, 20.f, 0.f, {}},
    {"forearm_l", 1, Joint::Ball, {6.4f, 10.1f, 3.4f}, {-4.5f, 11.7f, 11.1f}, {17.3f, 8.6f, -4.3f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"upperarm_r", 1, Joint::Ball, {-4.8f, -12.1f, 11.f}, {-2.8f, -8.f, 14.1f}, {1.1f, -17.4f, 8.8f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 6, Joint::Hinge, {11.f, -17.f, 1.4f}, {1.1f, -17.4f, 8.8f}, {20.9f, -16.5f, -6.f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {-15.5f, 4.5f, -12.8f}, {-15.f, 3.f, -4.f}, {-29.8f, 3.5f, -20.3f}, 2.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 8, Joint::Hinge, {-27.f, 4.2f, -23.5f}, {-29.8f, 3.5f, -20.3f}, {-24.3f, 4.8f, -26.6f}, 2.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {-12.3f, -7.2f, -13.1f}, {-14.f, -4.f, -4.f}, {-20.9f, -13.9f, -15.2f}, 2.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 10, Joint::Hinge, {-24.8f, -7.8f, -20.6f}, {-20.9f, -13.9f, -15.2f}, {-28.6f, -1.7f, -26.f}, 2.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's gaunt (progs/mon_gaunt.mdl: 161 vertices, its first frame perched; 24 clusters, bones 4.92: its flight
// deforms it): wings of two bones, a tail.
constexpr Seed adGauntSeeds[] = {
    {"pelvis", -1, Joint::Root, {-4.4f, -0.1f, -5.6f}, {-4.4f, -0.1f, -5.6f}, {-2.f, -2.2f, 3.9f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"head", 0, Joint::Ball, {9.4f, -0.1f, 0.9f}, {-2.f, -2.2f, 3.9f}, {20.8f, 2.f, -2.1f}, 0.f, 40.f, 40.f, 0.f, {}},
    {"wingroot_l", 0, Joint::Ball, {-5.2f, 9.8f, 8.4f}, {-10.9f, -2.1f, 14.8f}, {-1.5f, 20.7f, 0.f}, 0.f, 40.f, 20.f, 0.f, {}},
    {"wing_l", 2, Joint::Ball, {12.f, 19.8f, 26.9f}, {-1.5f, 20.7f, 0.f}, {25.5f, 18.9f, 53.7f}, 0.f, 40.f, 20.f, 0.f, {}},
    {"wingroot_r", 0, Joint::Ball, {-5.4f, -9.f, 7.9f}, {-12.4f, 2.4f, 13.5f}, {2.1f, -20.7f, 2.f}, 0.f, 40.f, 20.f, 0.f, {}},
    {"wing_r", 4, Joint::Ball, {12.4f, -20.f, 27.4f}, {2.1f, -20.7f, 2.f}, {22.6f, -19.3f, 52.7f}, 0.f, 40.f, 20.f, 0.f, {}},
    {"tail1", 0, Joint::Ball, {-29.1f, 0.1f, -23.3f}, {-12.4f, 0.7f, -23.6f}, {-45.8f, -0.5f, -22.9f}, 2.f, 40.f, 20.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {5.5f, 8.8f, -18.3f}, {-17.7f, 37.4f, -15.6f}, {-11.7f, 5.7f, -5.5f}, 1.5f, 60.f, 25.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {4.1f, 3.1f, -19.2f}, {-11.7f, 5.7f, -5.5f}, {7.5f, 2.5f, -22.1f}, 1.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {3.f, -6.7f, -10.f}, {-3.3f, -7.8f, -17.f}, {-1.8f, -5.9f, -18.1f}, 1.5f, 60.f, 25.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {5.8f, -5.5f, -22.6f}, {-1.8f, -5.9f, -18.1f}, {5.f, -5.6f, -22.1f}, 1.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's swampling and voreling (one mesh: 80 vertices; 18 clusters, bones 0.89): three legs of two bones and a head on
// top; the legs' roots and the neck set by hand.
constexpr Seed adSwamplingSeeds[] = {
    {"pelvis", -1, Joint::Root, {1.5f, 0.f, -13.9f}, {1.5f, 0.f, -13.9f}, {0.f, 0.f, -8.f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"head", 0, Joint::Ball, {-0.5f, 0.f, -4.7f}, {0.f, 0.f, -8.f}, {-3.5f, -0.9f, 0.f}, 0.f, 40.f, 40.f, 0.f, {}},
    {"leg_l", 0, Joint::Ball, {6.1f, 10.4f, -8.7f}, {3.f, 4.f, -12.f}, {5.9f, 11.8f, -12.3f}, 1.f, 60.f, 25.f, 0.f, {}},
    {"foot_l", 2, Joint::Hinge, {8.2f, 13.3f, -18.6f}, {5.9f, 11.8f, -12.3f}, {10.3f, 14.7f, -24.1f}, 1.f, 0.f, 0.f, 120.f, {0.f, 1.f, 0.f}},
    {"leg_r", 0, Joint::Ball, {6.2f, -10.9f, -10.2f}, {3.f, -4.f, -12.f}, {1.8f, -15.3f, -12.3f}, 1.f, 60.f, 25.f, 0.f, {}},
    {"foot_r", 4, Joint::Hinge, {7.5f, -12.1f, -19.3f}, {1.8f, -15.3f, -12.3f}, {11.5f, -9.8f, -24.3f}, 1.f, 0.f, 0.f, 120.f, {0.f, 1.f, 0.f}},
    {"leg_b", 0, Joint::Ball, {-8.9f, 0.1f, -10.8f}, {-5.f, 0.f, -12.f}, {-12.1f, -0.6f, -13.6f}, 1.f, 60.f, 25.f, 0.f, {}},
    {"foot_b", 6, Joint::Hinge, {-11.6f, 0.f, -19.f}, {-12.1f, -0.6f, -13.6f}, {-11.1f, 0.5f, -23.9f}, 1.f, 0.f, 0.f, 120.f, {0.f, 1.f, 0.f}},
};

// AD's centurion (progs/mon_centurion.mdl: 284 vertices; 18 clusters, bones 1.10): its hover board one bone on the
// pelvis (pivot and end set by hand). The gun, muzzle effect and hover board are hidden on death.
constexpr Seed adCenturionSeeds[] = {
    {"pelvis", -1, Joint::Root, {-2.1f, -0.7f, 12.9f}, {-2.1f, -0.7f, 12.9f}, {-2.1f, 3.f, 14.f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-5.2f, -0.4f, 26.1f}, {-2.1f, 3.f, 14.f}, {-1.7f, -1.6f, 33.3f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {0.1f, -1.f, 36.4f}, {-1.7f, -1.6f, 33.3f}, {1.9f, -0.5f, 39.4f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {0.1f, 14.3f, 21.5f}, {-1.6f, 11.4f, 23.3f}, {4.6f, 13.4f, 20.1f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {11.1f, 12.2f, 21.2f}, {4.6f, 13.4f, 20.1f}, {17.5f, 11.f, 22.3f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {1.2f, -16.2f, 24.6f}, {-2.7f, -12.8f, 30.5f}, {4.7f, -15.9f, 20.7f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {7.4f, -7.4f, 19.3f}, {4.7f, -15.9f, 20.7f}, {10.2f, 1.2f, 18.f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {0.1f, 6.1f, -2.7f}, {-0.4f, 1.6f, 1.6f}, {-2.1f, 7.8f, -12.8f}, 3.f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {1.1f, 7.8f, -14.3f}, {-2.1f, 7.8f, -12.8f}, {-0.1f, 7.8f, -13.7f}, 2.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {-0.2f, -8.9f, -9.6f}, {1.3f, -8.9f, 0.3f}, {-0.8f, -8.9f, -13.2f}, 3.f, 70.f, 30.f, 0.f, {}},
};

// AD's drole (progs/mon_drole.mdl: 204 vertices; 18 clusters, bones 1.90): four arms (two raised scythes, two lower
// claws); hips and knees set by hand.
constexpr Seed adDroleSeeds[] = {
    {"pelvis", -1, Joint::Root, {8.3f, -1.6f, 23.5f}, {8.3f, -1.6f, 23.5f}, {2.5f, 2.f, 29.3f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-1.5f, 7.3f, 35.5f}, {2.5f, 2.f, 29.3f}, {5.5f, 1.5f, 50.2f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {16.8f, -0.3f, 51.1f}, {5.5f, 1.5f, 50.2f}, {28.1f, -2.1f, 51.9f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-8.5f, 23.8f, 50.6f}, {-0.6f, 20.3f, 41.3f}, {-10.8f, 24.4f, 68.2f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {-4.f, 16.3f, 75.4f}, {-10.8f, 24.4f, 68.2f}, {2.9f, 8.2f, 82.6f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {-3.4f, -17.7f, 42.3f}, {1.7f, -10.4f, 40.3f}, {-9.5f, -27.5f, 53.2f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {-4.8f, -21.9f, 69.8f}, {-9.5f, -27.5f, 53.2f}, {-0.1f, -16.4f, 86.4f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"lowerarm_l", 0, Joint::Ball, {1.2f, 22.4f, -2.8f}, {3.3f, 17.4f, 17.6f}, {-0.6f, 26.8f, -20.8f}, 3.f, 60.f, 30.f, 0.f, {}},
    {"lowerarm_r", 0, Joint::Ball, {4.3f, -21.f, -2.7f}, {4.f, -18.9f, 14.2f}, {4.6f, -23.1f, -19.6f}, 3.f, 60.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {6.2f, 10.f, 2.7f}, {4.f, 6.f, 8.f}, {2.f, 9.f, -10.f}, 4.f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 9, Joint::Hinge, {1.8f, 12.1f, -21.4f}, {2.f, 9.f, -10.f}, {1.7f, 11.5f, -20.3f}, 3.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {5.9f, -9.7f, 2.3f}, {4.f, -6.f, 8.f}, {2.f, -9.f, -10.f}, 4.f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 11, Joint::Hinge, {1.7f, -10.7f, -21.5f}, {2.f, -9.f, -10.f}, {1.4f, -9.4f, -20.3f}, 3.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's scorpion (progs/mon_scorpion.mdl: 267 vertices; 18 clusters, bones 2.11): a tail of three, two claws, five
// legs each one bone.
constexpr Seed adScorpionSeeds[] = {
    {"pelvis", -1, Joint::Root, {3.4f, 0.8f, -13.1f}, {3.4f, 0.8f, -13.1f}, {-7.8f, -0.5f, -13.9f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"tail1", 0, Joint::Ball, {-19.4f, 0.9f, -7.8f}, {-7.8f, -0.5f, -13.9f}, {-22.8f, 0.1f, -1.4f}, 3.f, 40.f, 20.f, 0.f, {}},
    {"tail2", 1, Joint::Ball, {-26.2f, 0.8f, 6.5f}, {-22.8f, 0.1f, -1.4f}, {-20.2f, 1.3f, 15.4f}, 2.5f, 45.f, 20.f, 0.f, {}},
    {"tail3", 2, Joint::Ball, {-15.4f, 0.9f, 16.9f}, {-20.2f, 1.3f, 15.4f}, {-10.6f, 0.4f, 18.3f}, 2.f, 50.f, 20.f, 0.f, {}},
    {"arm_l", 0, Joint::Ball, {11.5f, 17.3f, -9.1f}, {5.8f, 6.6f, -14.7f}, {17.2f, 28.1f, -3.5f}, 0.f, 60.f, 30.f, 0.f, {}},
    {"arm_r", 0, Joint::Ball, {14.5f, -14.2f, -6.9f}, {7.2f, -7.1f, -13.f}, {21.8f, -21.3f, -0.7f}, 0.f, 60.f, 30.f, 0.f, {}},
    {"leg_fl", 0, Joint::Ball, {4.1f, 11.6f, -13.8f}, {5.6f, 4.6f, -13.8f}, {2.6f, 18.7f, -13.7f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_ml", 0, Joint::Ball, {-4.4f, 15.4f, -13.6f}, {-2.1f, 6.2f, -14.9f}, {-6.7f, 24.6f, -12.2f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_bl", 0, Joint::Ball, {-14.9f, 14.5f, -13.7f}, {-8.f, 6.f, -14.f}, {-29.4f, 14.6f, -12.8f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_mr", 0, Joint::Ball, {0.f, -13.7f, -13.6f}, {3.9f, -6.9f, -13.3f}, {-4.f, -20.6f, -13.8f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_br", 0, Joint::Ball, {-14.1f, -12.5f, -13.5f}, {-6.6f, -7.6f, -13.4f}, {-21.5f, -17.3f, -13.6f}, 0.f, 60.f, 25.f, 0.f, {}},
};

// AD's stone golem (progs/mon_golem.mdl: 427 vertices, 105 units tall; 24 clusters, bones 1.90); hips and knees set
// by hand. The ice golem's mesh is the same x1.5 (adIcegolemSeeds).
constexpr Seed adGolemSeeds[] = {
    {"pelvis", -1, Joint::Root, {3.4f, -0.1f, 34.4f}, {3.4f, -0.1f, 34.4f}, {2.1f, -0.1f, 41.5f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {6.7f, 2.2f, 61.7f}, {2.1f, -0.1f, 41.5f}, {7.f, -1.6f, 66.f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {10.2f, -0.5f, 73.6f}, {7.f, -1.6f, 66.f}, {13.4f, 0.5f, 81.3f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {3.1f, 24.8f, 49.f}, {5.1f, 20.6f, 58.8f}, {7.9f, 24.3f, 47.5f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {23.1f, 23.3f, 41.2f}, {7.9f, 24.3f, 47.5f}, {38.4f, 22.3f, 35.f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {1.9f, -24.1f, 53.4f}, {6.9f, -16.7f, 64.f}, {4.f, -27.8f, 46.5f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {17.3f, -28.5f, 34.7f}, {4.f, -27.8f, 46.5f}, {30.5f, -29.3f, 22.9f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {18.f, 9.2f, 14.1f}, {8.f, 8.f, 26.f}, {16.f, 10.f, -2.f}, 6.f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {14.5f, 11.f, -17.7f}, {16.f, 10.f, -2.f}, {14.9f, 8.9f, -19.7f}, 5.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {1.4f, -14.7f, 10.4f}, {3.f, -8.f, 26.f}, {-6.f, -17.f, -2.f}, 6.f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {-15.1f, -19.7f, -17.3f}, {-6.f, -17.f, -2.f}, {-16.1f, -19.6f, -20.3f}, 5.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's ice golem (progs/mon_bossicegolem.mdl): the golem's mesh x1.5, 12.3 units up (its vertices against the golem's
// in four frames, least squares: x1.499, within 1.2 units), so the golem's seeds so scaled (the golem's own put its
// pelvis's middle in his thighs: no cluster went to it).
constexpr Seed adIcegolemSeeds[] = {
    {"pelvis", -1, Joint::Root, {5.2f, -0.2f, 63.8f}, {5.2f, -0.2f, 63.8f}, {3.2f, -0.2f, 74.5f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {10.1f, 3.3f, 104.7f}, {3.2f, -0.2f, 74.5f}, {10.6f, -2.4f, 111.2f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {15.4f, -0.8f, 122.6f}, {10.6f, -2.4f, 111.2f}, {20.2f, 0.7f, 134.1f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {4.7f, 37.2f, 85.7f}, {7.7f, 30.9f, 100.4f}, {11.9f, 36.4f, 83.5f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {34.7f, 34.9f, 74.f}, {11.9f, 36.4f, 83.5f}, {57.6f, 33.4f, 64.7f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {2.9f, -36.1f, 92.3f}, {10.4f, -25.f, 108.2f}, {6.1f, -41.7f, 82.f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {26.f, -42.7f, 64.3f}, {6.1f, -41.7f, 82.f}, {45.8f, -43.9f, 46.6f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {27.1f, 13.8f, 33.4f}, {12.1f, 12.f, 51.2f}, {24.1f, 15.f, 9.3f}, 9.f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {21.8f, 16.5f, -14.3f}, {24.1f, 15.f, 9.3f}, {22.4f, 13.3f, -17.3f}, 7.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {2.2f, -22.f, 27.8f}, {4.6f, -12.f, 51.2f}, {-8.9f, -25.5f, 9.3f}, 9.f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {-22.5f, -29.5f, -13.7f}, {-8.9f, -25.5f, 9.3f}, {-24.f, -29.4f, -18.2f}, 7.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's death sergeant (progs/mon_dsergeant.mdl: 1265 vertices; 24 clusters, bones 1.10): the flail in the right hand
// (vertices 870..1264) no bone of its own (its chain and ball were one, dangling, before it dropped the flail: a loose
// piece now, as the hunter ogres' weapons, held alive and hidden in its deaths: vr_monstermods.cpp).
constexpr Seed adDsergeantSeeds[] = {
    {"pelvis", -1, Joint::Root, {2.2f, -0.5f, 7.2f}, {2.2f, -0.5f, 7.2f}, {1.7f, 0.1f, 15.8f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {5.4f, 0.5f, 26.4f}, {1.7f, 0.1f, 15.8f}, {6.4f, 0.1f, 26.8f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {8.6f, -0.2f, 32.1f}, {6.4f, 0.1f, 26.8f}, {10.8f, -0.4f, 37.4f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {0.9f, 13.1f, 14.1f}, {4.6f, 11.9f, 23.f}, {4.1f, 15.5f, 10.6f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {9.6f, 15.1f, 3.8f}, {4.1f, 15.5f, 10.6f}, {15.1f, 14.7f, -3.1f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {1.9f, -14.1f, 17.f}, {4.4f, -11.4f, 25.1f}, {5.6f, -16.2f, 8.1f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {11.1f, -17.2f, 5.2f}, {5.6f, -16.2f, 8.1f}, {17.6f, -13.f, 14.6f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 6, Joint::Ball, {13.5f, -17.6f, 7.f}, {17.6f, -13.f, 14.6f}, {17.9f, -18.6f, 11.1f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {1.5f, 5.8f, -4.4f}, {0.f, 0.4f, 6.f}, {-0.1f, 5.3f, -13.5f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 8, Joint::Hinge, {2.f, 9.3f, -22.f}, {-0.1f, 5.3f, -13.5f}, {1.8f, 8.9f, -21.2f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {5.3f, -6.4f, -3.6f}, {2.f, -0.3f, 7.3f}, {4.3f, -7.9f, -12.9f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 10, Joint::Hinge, {6.5f, -10.f, -21.9f}, {4.3f, -7.9f, -12.9f}, {6.4f, -9.8f, -21.2f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's bog lord (progs/mon_bossboglord.mdl): the shambler's mesh x1.3 (seedscale.py: shamblerSeeds x1.3, 6.7 up).
constexpr Seed adBoglordSeeds[] = {
    {"pelvis", -1, Joint::Root, {-15.9f, -1.6f, 28.3f}, {-15.9f, -1.6f, 28.3f}, {-14.4f, -2.5f, 41.3f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-4.f, -2.3f, 63.3f}, {-14.4f, -2.5f, 41.3f}, {8.7f, -2.9f, 70.7f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {22.f, -3.9f, 69.1f}, {8.7f, -2.9f, 70.7f}, {35.2f, -4.8f, 67.5f}, 0.f, 30.f, 30.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-2.5f, 35.6f, 57.5f}, {-3.6f, 18.9f, 74.4f}, {-4.9f, 43.4f, 44.8f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {11.7f, 43.9f, 35.7f}, {-4.9f, 43.4f, 44.8f}, {24.4f, 45.6f, 30.8f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_l", 4, Joint::Ball, {38.1f, 38.4f, 30.1f}, {24.4f, 45.6f, 30.8f}, {51.6f, 31.1f, 29.4f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"upperarm_r", 1, Joint::Ball, {-6.9f, -33.5f, 59.6f}, {-5.7f, -23.5f, 70.9f}, {-10.3f, -40.4f, 43.9f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 6, Joint::Hinge, {-4.4f, -45.8f, 31.f}, {-10.3f, -40.4f, 43.9f}, {10.f, -51.5f, 17.6f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 7, Joint::Ball, {23.5f, -52.f, 12.7f}, {10.f, -51.5f, 17.6f}, {37.1f, -52.4f, 7.7f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {-5.2f, 16.5f, 7.9f}, {-15.f, 12.5f, 25.9f}, {-9.1f, 18.7f, -3.f}, 7.2f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 9, Joint::Hinge, {-13.f, 17.7f, -16.2f}, {-9.1f, 18.7f, -3.f}, {-13.4f, 17.7f, -17.7f}, 6.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {-2.3f, -18.7f, 7.4f}, {-12.9f, -17.3f, 24.9f}, {-3.5f, -18.5f, -9.f}, 7.2f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 11, Joint::Hinge, {-3.6f, -21.7f, -18.3f}, {-3.5f, -18.5f, -9.f}, {-3.6f, -21.6f, -17.7f}, 6.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's zombie (progs/mon_zombie.mdl: id's zombie): Rogue's mummy's mesh x0.833 (mummySeeds x0.833, 3.9 down): its knees
// too (the ring's lower edge, keepHinge, flex 118).
constexpr Seed adZombieSeeds[] = {
    {"pelvis", -1, Joint::Root, {-0.2f, 0.2f, 3.8f}, {-0.2f, 0.2f, 3.8f}, {-0.7f, -0.2f, 7.3f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-1.1f, 0.3f, 14.2f}, {-0.7f, -0.2f, 7.3f}, {-2.1f, 0.7f, 20.3f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {-1.7f, 0.7f, 22.5f}, {-2.1f, 0.7f, 20.3f}, {-1.4f, 0.7f, 24.7f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-0.5f, 6.8f, 12.7f}, {-0.7f, 6.1f, 17.7f}, {0.4f, 7.2f, 7.8f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {0.8f, 7.7f, 1.3f}, {0.4f, 7.2f, 7.8f}, {1.2f, 8.2f, -5.2f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {-2.4f, -6.2f, 12.3f}, {-2.3f, -5.7f, 17.7f}, {-2.4f, -6.2f, 6.9f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {-2.4f, -6.5f, -0.5f}, {-2.4f, -6.2f, 6.9f}, {-2.5f, -6.7f, -7.9f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {4.4f, 5.3f, -11.1f}, {5.2f, 5.9f, -8.6f}, {4.3f, 5.5f, -12.f}, 2.7f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {6.4f, 6.7f, -22.8f}, {4.3f, 5.5f, -12.f}, {4.f, 6.7f, -21.7f}, 2.3f, 0.f, 0.f, 118.f, {0.f, 1.f, 0.f}, true},
    {"thigh_r", 0, Joint::Ball, {-2.4f, -4.7f, -12.4f}, {0.8f, -3.1f, -9.2f}, {-2.8f, -4.8f, -13.1f}, 2.7f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {-4.2f, -5.9f, -22.8f}, {-2.8f, -4.8f, -13.1f}, {-6.5f, -5.7f, -21.7f}, 2.3f, 0.f, 0.f, 118.f, {0.f, 1.f, 0.f}, true},
};

// AD's seeker (progs/mon_seeker.mdl: 317 vertices; 24 clusters, bones 4.0: its arms swing wide in its firing);
// knees set by hand.
constexpr Seed adSeekerSeeds[] = {
    {"pelvis", -1, Joint::Root, {-9.3f, 0.5f, 25.2f}, {-9.3f, 0.5f, 25.2f}, {-9.7f, 2.6f, 31.f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {3.6f, 0.7f, 54.1f}, {-9.7f, 2.6f, 31.f}, {23.2f, 3.8f, 54.4f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {23.8f, 3.3f, 48.6f}, {23.2f, 3.8f, 54.4f}, {24.4f, 2.8f, 42.7f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-9.1f, 31.3f, 40.7f}, {-14.f, 23.f, 50.5f}, {3.9f, 31.4f, 31.7f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {17.4f, 34.9f, 15.6f}, {3.9f, 31.4f, 31.7f}, {30.9f, 38.5f, -0.4f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {10.6f, -29.8f, 33.6f}, {1.4f, -21.9f, 41.8f}, {28.5f, -24.9f, 22.6f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {40.9f, -21.2f, 6.6f}, {28.5f, -24.9f, 22.6f}, {53.3f, -17.4f, -9.4f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {-0.6f, 13.f, 5.8f}, {-9.4f, 9.8f, 20.2f}, {2.f, 13.f, -8.f}, 5.f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {4.4f, 13.3f, -20.1f}, {2.f, 13.f, -8.f}, {4.4f, 13.f, -19.9f}, 4.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {-12.5f, -10.2f, -4.1f}, {-8.4f, 26.f, 20.8f}, {-14.9f, -31.6f, -18.9f}, 5.f, 70.f, 30.f, 0.f, {}},
};

// AD's pyro and fumigator (one mesh: 277 vertices; 18 clusters, bones 1.35): id's enforcer's build with a tank on
// the back; the gun, muzzle effect and hose are hidden on death. Left arm refit without the gun; hips set by hand; the
// knees as AD's enforcer's (rig.py's 'knee', z -11.6 and -11.5; keepHinge).
constexpr Seed adPyroSeeds[] = {
    {"pelvis", -1, Joint::Root, {-5.9f, -0.7f, 6.6f}, {-5.9f, -0.7f, 6.6f}, {-5.4f, -1.2f, 6.6f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-10.4f, -0.9f, 16.7f}, {-5.4f, -1.2f, 6.6f}, {-5.5f, -1.4f, 26.1f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {-4.1f, -1.2f, 28.3f}, {-5.5f, -1.4f, 26.1f}, {-2.6f, -1.1f, 30.4f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-3.8f, 14.2f, 13.7f}, {-5.5f, 11.4f, 15.4f}, {0.7f, 13.4f, 12.2f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {7.2f, 12.2f, 13.3f}, {0.7f, 13.4f, 12.2f}, {13.6f, 11.f, 14.4f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {-1.5f, -17.7f, 15.2f}, {-4.2f, -14.9f, 16.5f}, {0.8f, -11.7f, 10.7f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {4.f, -5.6f, 12.3f}, {0.8f, -11.7f, 10.7f}, {7.2f, 0.4f, 13.9f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {-4.f, 6.1f, -10.6f}, {-5.2f, -0.6f, 2.2f}, {-3.9f, 6.2f, -11.6f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {-2.8f, 7.9f, -22.2f}, {-3.9f, 6.2f, -11.6f}, {-6.6f, 7.7f, -21.2f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}, true},
    {"thigh_r", 0, Joint::Ball, {-4.5f, -7.6f, -10.6f}, {-4.2f, -2.6f, 2.f}, {-4.5f, -7.8f, -11.5f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {-4.2f, -9.8f, -22.1f}, {-4.5f, -7.8f, -11.5f}, {-7.9f, -9.4f, -21.2f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}, true},
};

// AD's Freddie (progs/mon_freddie.mdl: 660 vertices; 18 clusters, bones 5.4: machine legs of many parts): hips set by
// hand.
constexpr Seed adFreddieSeeds[] = {
    {"pelvis", -1, Joint::Root, {7.6f, -0.2f, 16.9f}, {7.6f, -0.2f, 16.9f}, {7.3f, 1.1f, 26.2f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {7.f, 2.1f, 32.6f}, {7.3f, 1.1f, 26.2f}, {7.2f, 4.f, 44.6f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {7.6f, 2.8f, 49.6f}, {7.2f, 4.f, 44.6f}, {8.1f, 1.6f, 54.5f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {11.7f, 14.4f, 28.3f}, {10.f, 15.6f, 42.f}, {24.f, 12.5f, 22.1f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {20.3f, 12.7f, 23.6f}, {24.f, 12.5f, 22.1f}, {16.6f, 12.9f, 25.1f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {5.3f, -19.f, 26.8f}, {7.5f, -12.6f, 46.4f}, {5.5f, -19.7f, 23.7f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {14.5f, -17.7f, 22.8f}, {5.5f, -19.7f, 23.7f}, {23.5f, -15.6f, 22.f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {4.7f, 12.6f, 5.2f}, {3.f, 10.f, 12.f}, {13.9f, 12.1f, 8.8f}, 4.f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {28.4f, 12.6f, -3.2f}, {13.9f, 12.1f, 8.8f}, {35.8f, 12.9f, -9.4f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {-5.3f, -12.5f, 12.1f}, {-2.f, -10.f, 12.f}, {-9.5f, -13.6f, 9.9f}, 4.f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {-16.9f, -12.5f, -16.5f}, {-9.5f, -13.6f, 9.9f}, {-18.6f, -12.3f, -22.6f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's justice knight (progs/mon_justice.mdl: 1008 vertices; 24 clusters, bones 1.68): the sword in the right hand.
constexpr Seed adJusticeSeeds[] = {
    {"pelvis", -1, Joint::Root, {6.4f, -4.f, 18.9f}, {6.4f, -4.f, 18.9f}, {8.f, -3.3f, 21.1f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {0.3f, -3.8f, 34.8f}, {8.f, -3.3f, 21.1f}, {3.6f, -3.4f, 39.4f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {3.5f, -3.7f, 45.f}, {3.6f, -3.4f, 39.4f}, {3.4f, -3.9f, 50.6f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-0.4f, 10.5f, 25.4f}, {2.5f, 7.3f, 32.6f}, {-2.1f, 14.9f, 21.6f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {2.4f, 18.5f, 10.2f}, {-2.1f, 14.9f, 21.6f}, {6.9f, 22.2f, -1.2f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {3.f, -19.7f, 25.8f}, {2.8f, -14.7f, 32.9f}, {5.2f, -24.1f, 21.2f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {13.4f, -29.3f, 16.5f}, {5.2f, -24.1f, 21.2f}, {14.5f, -27.7f, 18.2f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 6, Joint::Ball, {23.5f, -35.7f, 20.9f}, {14.5f, -27.7f, 18.2f}, {32.5f, -43.6f, 23.6f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {6.3f, 5.7f, 6.6f}, {8.1f, 2.4f, 14.3f}, {6.4f, 6.4f, -3.2f}, 4.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 8, Joint::Hinge, {2.4f, 12.3f, -16.9f}, {6.4f, 6.4f, -3.2f}, {1.4f, 13.8f, -20.3f}, 4.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {7.7f, -13.8f, 4.9f}, {6.4f, -8.9f, 13.9f}, {7.6f, -16.4f, -3.7f}, 4.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 10, Joint::Hinge, {4.4f, -17.6f, -17.7f}, {7.6f, -16.4f, -3.7f}, {3.8f, -17.8f, -20.3f}, 4.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's hell knight, id's model (progs/mon_hknight.mdl: 250 vertices; 18 clusters, bones 1.18): the point knight.
constexpr Seed adHknightSeeds[] = {
    {"pelvis", -1, Joint::Root, {0.4f, -0.4f, 9.3f}, {0.4f, -0.4f, 9.3f}, {-0.4f, 0.2f, 16.8f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {2.5f, -3.f, 26.7f}, {-0.4f, 0.2f, 16.8f}, {3.5f, -0.1f, 26.6f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {8.6f, -1.f, 33.f}, {3.5f, -0.1f, 26.6f}, {13.8f, -1.9f, 39.4f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-0.8f, 12.2f, 21.3f}, {1.3f, 9.1f, 26.f}, {-0.9f, 13.9f, 17.2f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {5.3f, 14.7f, 8.8f}, {-0.9f, 13.9f, 17.2f}, {11.5f, 15.5f, 0.4f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {1.6f, -17.5f, 13.1f}, {0.4f, -12.5f, 22.1f}, {5.8f, -20.f, 9.6f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {11.2f, -23.4f, 14.6f}, {5.8f, -20.f, 9.6f}, {16.7f, -26.8f, 19.5f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {4.7f, 8.8f, -9.1f}, {2.2f, 8.8f, 0.4f}, {0.8f, 11.7f, -19.8f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {3.5f, 11.9f, -21.9f}, {0.8f, 11.7f, -19.8f}, {2.3f, 11.8f, -20.9f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {4.9f, -8.6f, -9.3f}, {1.9f, -8.f, -0.2f}, {1.2f, -10.3f, -21.5f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {4.f, -11.1f, -22.3f}, {1.2f, -10.3f, -21.5f}, {0.8f, -10.2f, -21.3f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's eel (progs/mon_eel.mdl: 148 vertices; 18 clusters, bones 0.58): a chain of six.
constexpr Seed adEelSeeds[] = {
    {"pelvis", -1, Joint::Root, {2.1f, 5.9f, 0.3f}, {2.1f, 5.9f, 0.3f}, {4.6f, 6.5f, 1.9f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {9.5f, 2.f, 0.9f}, {4.6f, 6.5f, 1.9f}, {12.8f, -0.3f, 0.5f}, 0.f, 30.f, 20.f, 0.f, {}},
    {"head", 1, Joint::Ball, {17.6f, 0.f, -0.4f}, {12.8f, -0.3f, 0.5f}, {22.5f, 0.3f, -1.3f}, 0.f, 30.f, 20.f, 0.f, {}},
    {"tail1", 0, Joint::Ball, {-4.6f, -0.1f, 1.2f}, {-1.6f, 5.f, 0.4f}, {-7.8f, -4.f, 6.4f}, 0.f, 30.f, 20.f, 0.f, {}},
    {"tail2", 3, Joint::Ball, {-10.8f, -5.8f, 2.f}, {-7.8f, -4.f, 6.4f}, {-13.9f, -6.7f, 3.2f}, 0.f, 30.f, 20.f, 0.f, {}},
    {"tail3", 4, Joint::Ball, {-16.4f, -5.1f, 2.5f}, {-13.9f, -6.7f, 3.2f}, {-18.8f, -3.4f, 1.8f}, 0.f, 30.f, 20.f, 0.f, {}},
};

// AD's judicator (progs/mon_judge.mdl: 1016 vertices; 24 clusters, bones 2.24): the sword in the right hand; hip and
// knees set by hand.
constexpr Seed adJudgeSeeds[] = {
    {"pelvis", -1, Joint::Root, {8.1f, -0.1f, 27.2f}, {8.1f, -0.1f, 27.2f}, {5.2f, 4.6f, 41.1f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-2.5f, -4.8f, 49.5f}, {5.2f, 4.6f, 41.1f}, {-0.3f, -4.4f, 55.4f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {5.3f, -4.8f, 62.1f}, {-0.3f, -4.4f, 55.4f}, {11.f, -5.2f, 68.7f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-0.5f, 14.5f, 37.5f}, {2.f, 11.2f, 47.2f}, {-2.8f, 20.8f, 32.6f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {3.8f, 24.6f, 17.8f}, {-2.8f, 20.8f, 32.6f}, {10.4f, 28.4f, 2.9f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {4.9f, -26.5f, 38.3f}, {2.9f, -21.4f, 47.1f}, {9.6f, -32.6f, 31.9f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {18.9f, -38.5f, 26.4f}, {9.6f, -32.6f, 31.9f}, {22.7f, -35.4f, 31.7f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 6, Joint::Ball, {33.9f, -48.1f, 36.4f}, {22.7f, -35.4f, 31.7f}, {45.1f, -60.8f, 41.1f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {5.f, 9.9f, 1.8f}, {6.f, 8.f, 14.f}, {4.f, 13.f, -8.f}, 5.f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 8, Joint::Hinge, {3.4f, 17.7f, -19.8f}, {4.f, 13.f, -8.f}, {4.3f, 19.2f, -20.3f}, 4.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {9.1f, -17.f, 13.7f}, {8.1f, -10.f, 24.9f}, {7.f, -20.f, -2.f}, 5.f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 10, Joint::Hinge, {5.f, -22.6f, -17.5f}, {7.f, -20.f, -2.f}, {4.4f, -22.8f, -20.3f}, 4.5f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's Nour (progs/mon_bossnour.mdl: 224 vertices; 18 clusters, bones 1.22): six arms, a tail of two.
constexpr Seed adNourSeeds[] = {
    {"pelvis", -1, Joint::Root, {0.1f, -2.2f, 9.3f}, {0.1f, -2.2f, 9.3f}, {4.4f, 0.4f, 21.8f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"head", 0, Joint::Ball, {8.4f, -0.4f, 30.6f}, {4.4f, 0.4f, 21.8f}, {12.5f, -1.1f, 39.4f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"tail1", 0, Joint::Ball, {-17.f, -0.4f, -25.5f}, {-7.1f, 1.3f, -19.5f}, {-30.3f, -0.1f, -31.9f}, 4.f, 40.f, 20.f, 0.f, {}},
    {"tail2", 2, Joint::Ball, {-41.8f, -0.4f, -20.6f}, {-30.3f, -0.1f, -31.9f}, {-53.2f, -0.7f, -9.3f}, 3.f, 45.f, 20.f, 0.f, {}},
    {"upperarm_l", 0, Joint::Ball, {-20.4f, 28.4f, 28.6f}, {-2.9f, 7.4f, 15.1f}, {-18.9f, 41.2f, 38.8f}, 0.f, 70.f, 40.f, 0.f, {}},
    {"forearm_l", 4, Joint::Ball, {-11.9f, 44.1f, 40.8f}, {-18.9f, 41.2f, 38.8f}, {-4.9f, 47.1f, 42.8f}, 0.f, 60.f, 30.f, 0.f, {}},
    {"upperarm_r", 0, Joint::Ball, {-24.f, -26.4f, 27.3f}, {-3.f, -6.8f, 14.9f}, {-24.3f, -38.8f, 38.f}, 0.f, 70.f, 40.f, 0.f, {}},
    {"forearm_r", 6, Joint::Ball, {-17.4f, -43.1f, 38.7f}, {-24.3f, -38.8f, 38.f}, {-10.4f, -47.5f, 39.5f}, 0.f, 60.f, 30.f, 0.f, {}},
    {"arm_ml", 0, Joint::Ball, {-0.9f, 21.7f, 14.4f}, {-0.2f, 9.6f, 16.1f}, {-1.5f, 33.9f, 12.7f}, 0.f, 60.f, 30.f, 0.f, {}},
    {"arm_mr", 0, Joint::Ball, {-1.2f, -26.f, 11.2f}, {-0.1f, -14.9f, 15.4f}, {-2.2f, -37.2f, 7.f}, 0.f, 60.f, 30.f, 0.f, {}},
    {"arm_ll", 0, Joint::Ball, {-20.5f, 22.9f, -2.6f}, {-1.9f, 5.2f, 0.4f}, {-22.3f, 36.4f, -5.6f}, 0.f, 60.f, 30.f, 0.f, {}},
    {"hand_ll", 10, Joint::Ball, {-17.6f, 39.9f, -8.2f}, {-22.3f, 36.4f, -5.6f}, {-12.8f, 43.3f, -10.8f}, 0.f, 60.f, 30.f, 0.f, {}},
    {"arm_lr", 0, Joint::Ball, {-10.5f, -38.4f, -7.8f}, {-4.9f, -12.9f, -4.6f}, {-16.2f, -63.9f, -11.f}, 0.f, 60.f, 30.f, 0.f, {}},
};

// AD's scrag, id's model (progs/mon_wizard.mdl: 80 vertices; 18 clusters, bones 0.86): arms, a tail of three.
constexpr Seed adWizardSeeds[] = {
    {"pelvis", -1, Joint::Root, {-3.7f, -0.7f, -8.9f}, {-3.7f, -0.7f, -8.9f}, {1.3f, -8.9f, -3.6f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {2.2f, 0.6f, 8.4f}, {1.3f, -8.9f, -3.6f}, {2.f, -2.5f, 20.1f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {6.8f, 0.f, 25.3f}, {2.f, -2.5f, 20.1f}, {11.6f, 2.5f, 30.5f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-1.f, 15.9f, 12.5f}, {0.2f, 7.8f, 14.1f}, {-2.2f, 24.f, 10.9f}, 0.f, 70.f, 40.f, 0.f, {}},
    {"upperarm_r", 1, Joint::Ball, {-0.3f, -16.4f, 12.9f}, {-0.9f, -6.1f, 12.1f}, {0.3f, -26.6f, 13.6f}, 0.f, 70.f, 40.f, 0.f, {}},
    {"tail1", 0, Joint::Ball, {-16.f, 0.8f, -19.7f}, {-8.8f, -0.3f, -18.9f}, {-22.f, 1.1f, -20.2f}, 3.f, 40.f, 20.f, 0.f, {}},
    {"tail2", 5, Joint::Ball, {-26.3f, 1.1f, -20.2f}, {-22.f, 1.1f, -20.2f}, {-33.8f, 0.6f, -17.5f}, 2.5f, 40.f, 20.f, 0.f, {}},
    {"tail3", 6, Joint::Ball, {-39.6f, -1.8f, -14.f}, {-33.8f, 0.6f, -17.5f}, {-45.4f, -4.2f, -10.6f}, 2.f, 40.f, 20.f, 0.f, {}},
};

// AD's knight, id's model (progs/mon_knight.mdl: 268 vertices, 163 frames; 18 clusters, bones 0.79:
// mon_knight_bones.json): the sword loose (dropped in his deaths: vr_monstermods.cpp). The waist and the left knee set by
// hand; the pelvis's and thighs' centres moved so the hips' clusters go to the thighs.
constexpr Seed adKnightSeeds[] = {
    {"pelvis", -1, Joint::Root, {0.5f, 0.f, 4.f}, {0.5f, 0.f, 4.f}, {0.f, 0.f, 7.5f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {0.4f, -0.2f, 15.1f}, {0.f, 0.f, 7.5f}, {1.1f, 0.1f, 18.8f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {3.f, -0.1f, 22.1f}, {1.1f, 0.1f, 18.8f}, {4.8f, -0.3f, 25.3f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-5.5f, 7.7f, 9.7f}, {-1.2f, 5.7f, 14.9f}, {-4.f, 10.2f, 5.8f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {-0.9f, 11.5f, 1.1f}, {-4.f, 10.2f, 5.8f}, {2.2f, 12.8f, -3.5f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {2.6f, -10.1f, 8.8f}, {3.f, -6.9f, 14.1f}, {7.f, -10.8f, 3.7f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {10.3f, -9.3f, 2.2f}, {7.f, -10.8f, 3.7f}, {13.6f, -7.8f, 0.7f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {-1.3f, 5.2f, -7.f}, {-0.3f, 3.6f, -0.6f}, {-3.5f, 5.f, -13.f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {-6.5f, 6.3f, -19.9f}, {-3.5f, 5.f, -13.f}, {-7.4f, 7.f, -21.4f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {4.5f, -5.2f, -6.f}, {-0.2f, -4.6f, 0.f}, {8.9f, -5.9f, -10.3f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {8.3f, -6.1f, -19.5f}, {8.9f, -5.9f, -10.3f}, {8.2f, -6.1f, -21.4f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's ogre, id's model (progs/mon_ogre.mdl: 169 vertices, 147 frames; 18 clusters, bones 0.98: mon_ogre_bones.json):
// the nail ogre and the rest of AD's ogres (the fishing ogre turns into it as it wakes). The chainsaw in the right hand is
// part of its forearm (it keeps it); knees set by hand (low-poly legs: the fit put them at the feet).
constexpr Seed adOgreSeeds[] = {
    {"pelvis", -1, Joint::Root, {0.9f, -0.4f, 6.9f}, {0.9f, -0.4f, 6.9f}, {0.4f, 0.3f, 10.5f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {4.5f, -2.3f, 22.3f}, {0.4f, 0.3f, 10.5f}, {8.6f, -2.3f, 24.9f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {11.5f, -2.7f, 28.4f}, {8.6f, -2.3f, 24.9f}, {14.3f, -3.1f, 31.9f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {7.2f, 16.f, 17.2f}, {6.4f, 11.3f, 22.4f}, {7.9f, 18.7f, 9.5f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {14.2f, 18.4f, 0.1f}, {7.9f, 18.7f, 9.5f}, {20.5f, 18.1f, -9.3f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {2.1f, -19.1f, 14.9f}, {3.3f, -16.5f, 20.7f}, {1.8f, -22.7f, 3.f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {-11.9f, -23.1f, -8.1f}, {1.8f, -22.7f, 3.f}, {-25.7f, -23.5f, -19.1f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {3.6f, 8.4f, -8.4f}, {2.3f, 9.9f, 0.5f}, {3.f, 9.6f, -14.5f}, 4.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {3.4f, 10.2f, -21.7f}, {3.f, 9.6f, -14.5f}, {3.4f, 10.2f, -20.4f}, 4.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {2.3f, -10.4f, -8.6f}, {3.4f, -13.2f, -0.2f}, {2.8f, -11.f, -14.5f}, 4.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {1.f, -12.5f, -21.6f}, {2.8f, -11.f, -14.5f}, {1.f, -12.5f, -19.9f}, 4.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// AD's deflector (progs/mon_deflector.mdl: 267 vertices; id's enforcer's body, 61-221, with a gun, a pack and field
// rings of its own; 18 clusters, bones 1.17: mon_deflector_bones.json): its muzzle effect and three field rings (tucked in
// its chest but in its field frames) hidden on death (vr_monstermods.cpp). The gun a bone of its own on the right hand
// (it moves on it, the left hand on its front); the knees as AD's enforcer's (its legs' mesh: the ring 0.2 off); the
// pelvis's, chest's and head's centres moved so the waist's and the back pack's clusters go to the chest.
constexpr Seed adDeflectorSeeds[] = {
    {"pelvis", -1, Joint::Root, {-6.3f, -0.4f, 1.5f}, {-6.3f, -0.4f, 4.5f}, {-6.2f, 0.5f, 8.f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-9.5f, -1.5f, 20.f}, {-6.2f, 0.5f, 8.f}, {-5.5f, -1.2f, 25.1f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {0.f, -0.5f, 30.f}, {-5.5f, -1.2f, 25.1f}, {-2.3f, -1.4f, 31.4f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-5.9f, 13.1f, 17.5f}, {-6.5f, 9.3f, 21.8f}, {-4.f, 13.5f, 12.9f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {3.8f, 12.7f, 13.4f}, {-4.f, 13.5f, 12.9f}, {11.6f, 12.f, 13.9f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {-2.6f, -16.4f, 16.6f}, {-6.5f, -12.8f, 21.7f}, {1.f, -15.9f, 12.9f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {3.5f, -7.3f, 11.4f}, {1.f, -15.9f, 12.9f}, {3.6f, -7.7f, 14.7f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"gun", 6, Joint::Ball, {4.5f, 9.5f, 16.1f}, {3.6f, -7.7f, 14.7f}, {5.3f, 26.8f, 17.6f}, 0.f, 25.f, 15.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {-3.8f, 5.9f, -10.7f}, {-4.8f, 2.8f, -5.3f}, {-3.9f, 6.2f, -11.7f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 8, Joint::Hinge, {-2.8f, 8.f, -22.3f}, {-3.9f, 6.2f, -11.7f}, {-6.7f, 7.8f, -21.3f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}, true},
    {"thigh_r", 0, Joint::Ball, {-4.3f, -7.7f, -10.6f}, {-3.7f, -8.3f, -5.f}, {-4.4f, -7.9f, -11.5f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 10, Joint::Hinge, {-4.f, -9.7f, -22.2f}, {-4.4f, -7.9f, -11.5f}, {-8.f, -9.3f, -21.3f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}, true},
};

#else

// A row: the model's id, its vertex count, its seeds, its death animations (1 or 2: first and last frames of each), its
// motion clusters (rig.py's k: 18, more when its bones were made from more).
#define QVR_AD_RIG(id, verts, seeds, deaths, firstA, firstB, lastA, lastB, k) \
    {modelmeta::Id::id, verts, seeds, static_cast<int>(sizeof(seeds) / sizeof(seeds[0])), deaths, {firstA, firstB}, {lastA, lastB}, k},
// ... a model of separate pieces (legs) moving alike: each piece one bone's (SeedTable::wholePieces).
#define QVR_AD_RIG_PIECES(id, verts, seeds, deaths, firstA, firstB, lastA, lastB, k) \
    {modelmeta::Id::id, verts, seeds, static_cast<int>(sizeof(seeds) / sizeof(seeds[0])), deaths, {firstA, firstB}, {lastA, lastB}, k, true},
// ... a model whose loose pieces are held apart (the death fury's two swords, one in each hand): one loose bone each
// (SeedTable::loosePerPiece), each welded to its own hand knocked down (it floated, both welded as one: 2026-10-23).
#define QVR_AD_RIG_SPLIT(id, verts, seeds, deaths, firstA, firstB, lastA, lastB, k) \
    {modelmeta::Id::id, verts, seeds, static_cast<int>(sizeof(seeds) / sizeof(seeds[0])), deaths, {firstA, firstB}, {lastA, lastB}, k, false, false, true},
// ... a model whose weapon no frame hides (the zombie knight's blade: its falls are its knockdown's): loose all the same
// (SeedTable::looseKept), held alive, gone from its dead ragdoll (it drops the blade: QC VR_ADMelee_Drop).
#define QVR_AD_RIG_KEPT(id, verts, seeds, deaths, firstA, firstB, lastA, lastB, k) \
    {modelmeta::Id::id, verts, seeds, static_cast<int>(sizeof(seeds) / sizeof(seeds[0])), deaths, {firstA, firstB}, {lastA, lastB}, k, false, false, false, true},
QVR_AD_RIG(AdDknight, 911, adDknightSeeds, 2, 212, 224, 223, 243, 24)
QVR_AD_RIG(AdDprince, 911, adDknightSeeds, 2, 212, 224, 223, 243, 24)
// AD's soldiers (progs/mon_soldier.mdl: Quake VR's grunt's mesh, 555 vertices, id's 114 frames: Quake VR's appends 6):
// the grunt's seeds, his deaths 8-17, 18-28.
QVR_AD_RIG(AdSoldier, 555, gruntSeeds, 2, 8, 18, 17, 28, 18)
// The zombie knight has no death (AD's zombies die gibbed): its two falls (paine 121-132, paind 98-109).
QVR_AD_RIG_KEPT(AdZombiek, 161, adZombiekSeeds, 2, 121, 98, 132, 109, 24)
QVR_AD_RIG(AdDcrossbow, 1120, adDcrossbowSeeds, 2, 225, 236, 235, 255, 24)
// The hunter ogres' deaths A and B (the cleaver's rare third, S 154-163, goes limp as a corpse).
QVR_AD_RIG(AdHogre, 1017, adHogreSeeds, 2, 164, 174, 173, 187, 24)
QVR_AD_RIG(AdHogremac, 1224, adHogremacSeeds, 2, 132, 142, 141, 155, 24)
// The spiders: one mesh, green, brown and the steel claw (346 vertices, 134 frames; death 114-133).
QVR_AD_RIG_PIECES(AdSpiderg, 346, adSpiderSeeds, 1, 114, 0, 133, 0, 18)
QVR_AD_RIG_PIECES(AdSpiderb, 346, adSpiderSeeds, 1, 114, 0, 133, 0, 18)
QVR_AD_RIG_PIECES(AdSteelclaw, 346, adSpiderSeeds, 1, 114, 0, 133, 0, 18)
// The death guard's deaths A and B (C 227-240 and D 241-255 go limp as a corpse).
QVR_AD_RIG(AdDguard, 585, adDguardSeeds, 2, 195, 207, 206, 226, 24)
QVR_AD_RIG(AdSkullwiz, 632, adSkullwizSeeds, 1, 0, 0, 14, 0, 18)
QVR_AD_RIG(AdHogreham, 1306, adHogrehamSeeds, 2, 137, 147, 146, 160, 24)
QVR_AD_RIG(AdMinotaur, 514, adMinotaurSeeds, 1, 36, 0, 60, 0, 24)
QVR_AD_RIG_SPLIT(AdDfury, 1376, adDfurySeeds, 2, 224, 241, 240, 254, 24)
// AD's enforcer is id's model (progs/mon_enforcer.mdl: 222 vertices; Quake VR's own enforcer.mdl is another mesh).
QVR_AD_RIG(AdEnforcer, 222, adEnforcerSeeds, 2, 41, 55, 54, 65, 18)
QVR_AD_RIG(AdGargoyle, 338, adGargoyleSeeds, 1, 0, 0, 13, 0, 18)
QVR_AD_RIG(AdGaunt, 161, adGauntSeeds, 2, 93, 101, 100, 108, 24)
// The swampling's mesh is the voreling's (80 vertices, 73 frames, the same deaths) and their purple skins' models'.
QVR_AD_RIG(AdSwampling, 80, adSwamplingSeeds, 2, 58, 64, 63, 70, 18)
QVR_AD_RIG(AdSwamplingp, 80, adSwamplingSeeds, 2, 58, 64, 63, 70, 18)
QVR_AD_RIG(AdVoreling, 80, adSwamplingSeeds, 2, 58, 64, 63, 70, 18)
QVR_AD_RIG(AdVorelingp, 80, adSwamplingSeeds, 2, 58, 64, 63, 70, 18)
QVR_AD_RIG(AdCenturion, 284, adCenturionSeeds, 2, 18, 32, 31, 42, 18)
QVR_AD_RIG(AdDrole, 204, adDroleSeeds, 1, 169, 0, 194, 0, 18)
QVR_AD_RIG(AdScorpion, 267, adScorpionSeeds, 1, 74, 0, 94, 0, 18)
// The golem's mesh is the ice golem's too (progs/mon_bossicegolem.mdl, the same frames, x1.5: its own seeds).
QVR_AD_RIG(AdGolem, 427, adGolemSeeds, 1, 157, 0, 178, 0, 24)
QVR_AD_RIG(AdIcegolem, 427, adIcegolemSeeds, 1, 157, 0, 178, 0, 24)
QVR_AD_RIG(AdDsergeant, 1265, adDsergeantSeeds, 2, 223, 235, 234, 254, 24)
// The bog lord: the shambler's mesh x1.3, his frames (seedscale: the shambler's seeds scaled, 6.7 units up).
QVR_AD_RIG(AdBoglord, 648, adBoglordSeeds, 1, 83, 0, 93, 0, 24)
// AD's shambler is Quake VR's (the same mesh and frames): his own seeds.
QVR_AD_RIG(AdShambler, 648, shamblerSeeds, 1, 83, 0, 93, 0, 24)
// AD's zombie (its mummy too): id's zombie, Rogue's mummy's mesh x0.833 (its seeds scaled, 3.9 down); no deaths (they
// die gibbed): its falls, painb 103-116, paine 162-178, as the mummy's table.
QVR_AD_RIG(AdZombie, 177, adZombieSeeds, 2, 103, 162, 116, 178, 18)
QVR_AD_RIG(AdSeeker, 317, adSeekerSeeds, 2, 59, 185, 70, 196, 24)
// The pyro's mesh is the fumigator's (277 vertices, 83 frames).
QVR_AD_RIG(AdPyro, 277, adPyroSeeds, 2, 41, 55, 54, 65, 18)
QVR_AD_RIG(AdFumigator, 277, adPyroSeeds, 2, 41, 55, 54, 65, 18)
QVR_AD_RIG(AdFreddie, 660, adFreddieSeeds, 1, 112, 0, 146, 0, 18)
QVR_AD_RIG(AdJustice, 1008, adJusticeSeeds, 2, 112, 103, 121, 111, 24)
QVR_AD_RIG(AdEel, 148, adEelSeeds, 1, 6, 0, 20, 0, 18)
// AD's hell knight is id's model (progs/mon_hknight.mdl: 250 vertices; Quake VR's own hknight.mdl is another mesh).
QVR_AD_RIG(AdHknight, 250, adHknightSeeds, 2, 42, 54, 53, 62, 18)
// Bosses: the judicator (the justice knight's frame list, his deaths), Nour (a flyer; death 39-47). The eidolon has no
// death frames (his death is scripted): no rig.
QVR_AD_RIG(AdJudge, 1016, adJudgeSeeds, 2, 112, 103, 121, 111, 24)
QVR_AD_RIG(AdNour, 224, adNourSeeds, 1, 39, 0, 47, 0, 18)
// AD's scrag is id's model (progs/mon_wizard.mdl: 80 vertices; Quake VR's own wizard.mdl is another mesh).
QVR_AD_RIG(AdWizard, 80, adWizardSeeds, 1, 46, 0, 53, 0, 18)
QVR_AD_RIG(AdKnight, 268, adKnightSeeds, 2, 66, 76, 75, 86, 18)
QVR_AD_RIG(AdOgre, 169, adOgreSeeds, 2, 112, 126, 125, 135, 18)
// The death lord and Quoth's death guard: id's hell knight's mesh (within 0.2 units in every frame; AD's frame list,
// the death guard without some of its plates), his seeds; their swords loose (dropped in their deaths).
QVR_AD_RIG(AdDlord, 245, adHknightSeeds, 2, 42, 54, 53, 62, 18)
QVR_AD_RIG(AdDguardq, 176, adHknightSeeds, 2, 42, 54, 53, 62, 18)
QVR_AD_RIG(AdDeflector, 267, adDeflectorSeeds, 2, 51, 65, 64, 75, 18)
// No rig (no ragdoll): the fishing ogre (it never dies as itself: hurt, it wakes as AD's ogre, above), the floyd (its
// frame 0 is its skin's layout, not a pose: the rest pose a rig is measured in; it explodes when rolling or gibbed), and
// those that die exploding or vanish: the boil, jim, the lost soul, the sentinel, the wraith (hidden after its death).
#undef QVR_AD_RIG
#undef QVR_AD_RIG_PIECES
#undef QVR_AD_RIG_SPLIT
#undef QVR_AD_RIG_KEPT

#endif
