// vr_ragdoll.cpp -- ragdolls (experimental: the monsters in seedTables): the rig derived from a .mdl's vertex animation, the skinned
// model made from it in memory, and the client's drawing of the server's ragdolls. See vr_ragdoll.hpp; the bodies and
// joints are vr_box3d.cpp's ("Ragdolls"); ROUND21.md, "Ragdolls".

#include "vr_modelmetadata.hpp"
#include "vr_ragdoll.hpp"
#include "vr_api_render.h"
#include "vr_cvars.hpp"
#include "vr_box3d.hpp"
#include "vr_decals.hpp"
#include "vr_hands.hpp"
#include "vr_jobs.hpp"
#include "vr_mem.hpp"
#include "vr_protocol.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include <cstring>

using namespace qvr;

namespace qvr::ragdoll
{

// A rig's heap memory (mem::Cache's count).
[[nodiscard]] za::SizeT heldBytes(const Rig& r)
{
    za::SizeT n = mem::heldBytes(r.vertBone) + mem::heldBytes(r.triBones) + mem::heldBytes(r.poseRot) + mem::heldBytes(r.posePos) + mem::heldBytes(r.poseHidden);
    for(const Bone& b : r.bones)
    {
        n += mem::heldBytes(b.points);
    }
    return n;
}

} // namespace qvr::ragdoll

namespace
{

using ragdoll::Bone;
using ragdoll::Joint;
using ragdoll::Rig;
using ragdoll::maxBones;

constexpr float deg = 0.017453292f;

// ----------------------------------------------------------------------------
// The seed tables: per model, its bones (where they are in the rest pose, pose 0) and joints. Numbers of ours (measured
// on the model, ROUND21.md "Ragdolls"), not the model's.
//
// Decapitation (ROUND21.md, "Decapitation"; vr_box3d.cpp cutHead, QC vr_decap.qc) comes with every table: its bone named
// "head" (Rig::head) is what a slash cuts off, its pivot the neck the stump is at, the bones on it (a rottweiler's jaw)
// go with it. A new monster's table gets it by naming its head "head", its pivot where the head meets the chest; QC's
// VR_Decap_HeadModel names its head gib (else none is thrown) and PositionalHead its head zone.

struct Seed
{
    const char* name;
    int parent;
    Joint joint;
    glm::vec3 centre; // about where the bone's vertices are (the clusters go to the nearest)
    glm::vec3 pivot;  // the joint with the parent (the root: its middle)
    glm::vec3 end;    // the far end
    float capsule;    // a capsule pivot..end of this radius besides its hull (units; 0: none)
    float cone;       // Ball: degrees
    float twist;      // Ball: degrees either way
    float flex;       // Hinge: how far it bends at most (degrees from straight)
    glm::vec3 hinge;  // Hinge: its axis when the rest pose is (nearly) straight; bending turns it that way
    bool keepHinge{false}; // Hinge: `hinge` is its axis whatever the rest pose's bend (a leg bent a little sideways at rest)
};

struct SeedTable
{
    modelmeta::Id model;
    int numVerts; // the model the table was made for (Quake VR's grunt): another one is not rigged
    const Seed* seeds;
    int count;
    int deaths;
    int deathFirst[2], deathLast[2];
    int clusters{18}; // the motion clusters (more than the bones: the seeds gather them); more for a rig whose 18 merge two
                      // of its bones (the fiend's right thigh and shin, rig.py's k)
    bool wholePieces{false}; // every piece but the body (welded meshes of their own) one bone's, the seed nearest its
                             // middle, and the clusters only the body's: the centroid's six legs, arms and pincers move
                             // so alike that the motion clusters mix them (rig.py's "wholepieces", "pN")
    bool looseWeapons{false}; // every piece but the body loose (one loose bone, hidden in the ragdoll: Rig::hideLoose):
                              // the player's axe and gun, in his hands or on his back by the frame (rig.py LOOSE_PIECES)
};

// Quake VR's grunt (quakevr/progs/soldier.mdl: 555 vertices, 120 frames). The rest pose: x forward, y left, z up; he
// crouches a little with the shotgun in both hands. Death frames 8-17 ($death1-10) and 18-28 ($deathc1-11).
constexpr Seed gruntSeeds[] = {
    {"pelvis", -1, Joint::Root, {-7.f, 0.f, 0.f}, {-5.5f, 0.f, -1.f}, {-4.7f, 0.9f, 4.8f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-1.f, 0.f, 12.f}, {-4.7f, 0.9f, 4.8f}, {2.7f, 0.6f, 17.7f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {5.f, 0.f, 21.f}, {2.7f, 0.6f, 17.7f}, {5.f, 0.f, 26.f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {1.6f, 8.8f, 9.6f}, {0.7f, 6.3f, 14.5f}, {3.5f, 10.1f, 4.2f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {9.9f, 6.f, 2.4f}, {3.5f, 10.1f, 4.2f}, {13.f, 6.f, 2.5f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {-0.4f, -9.7f, 12.1f}, {0.1f, -7.7f, 16.3f}, {0.2f, -10.8f, 7.f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {4.7f, -5.6f, 4.9f}, {0.2f, -10.8f, 7.f}, {7.5f, -3.5f, 5.5f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {-4.5f, 7.4f, -13.2f}, {-6.3f, 4.3f, -4.1f}, {-4.5f, 7.5f, -14.f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {-9.7f, 7.6f, -23.f}, {-4.5f, 7.5f, -14.f}, {-9.f, 7.5f, -19.5f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {3.1f, -6.7f, -10.5f}, {-4.5f, -5.3f, -1.5f}, {3.5f, -6.7f, -11.5f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {2.f, -7.2f, -23.f}, {3.5f, -6.7f, -11.5f}, {1.f, -7.f, -20.f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// Quake VR's knight (quakevr/progs/knight.mdl: 655 vertices, 97 frames). The rest pose: x forward, y left, z up; his right
// leg forward, his sword arm bent forward (the sword in its hand), his left arm down at his side. His arms move as one
// piece from the shoulder to the wrist in all his frames (no elbow): an arm and a hand each. Measured on his frames
// (Misc/quakevr/ragdoll/knight_rig.py: the motion clusters, the joints' centres by least squares between the bones'
// motions). His sword is a piece of its own, collapsed in his death frames (he drops it: vr_monstermods.cpp): a loose
// bone, hidden in his ragdoll (as the grunt's shotgun).
// Death frames 76-85 ($death1-10) and 86-96 ($deathb1-11).
constexpr Seed knightSeeds[] = {
    {"pelvis", -1, Joint::Root, {2.8f, 1.8f, 1.8f}, {2.8f, 1.8f, 1.f}, {2.3f, 1.4f, 7.f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {3.5f, 1.9f, 11.8f}, {2.3f, 1.4f, 7.f}, {4.3f, 1.4f, 16.f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {5.7f, 1.7f, 20.1f}, {4.3f, 1.4f, 16.f}, {6.f, 1.7f, 25.f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"arm_l", 1, Joint::Ball, {-2.3f, 12.4f, 8.8f}, {2.f, 10.2f, 13.f}, {-3.8f, 14.1f, 3.5f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"hand_l", 3, Joint::Ball, {-2.3f, 14.1f, 1.f}, {-3.8f, 14.1f, 3.5f}, {-2.5f, 14.5f, -1.6f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"arm_r", 1, Joint::Ball, {9.1f, -9.1f, 5.f}, {5.5f, -7.6f, 12.f}, {11.5f, -7.3f, 1.5f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"hand_r", 5, Joint::Ball, {10.1f, -7.2f, -0.1f}, {11.5f, -7.3f, 1.5f}, {10.3f, -6.5f, -4.5f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {2.6f, 6.8f, -10.1f}, {2.1f, 6.f, -1.5f}, {-1.f, 6.f, -16.3f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {-3.4f, 6.f, -20.1f}, {-1.f, 6.f, -16.3f}, {-4.f, 6.f, -21.5f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {7.1f, -2.6f, -6.4f}, {2.6f, -1.5f, -1.8f}, {10.8f, -2.2f, -12.2f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {9.3f, -2.2f, -18.6f}, {10.8f, -2.2f, -12.2f}, {10.5f, -2.3f, -21.5f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// Quake VR's ogre (quakevr/progs/ogre.mdl: 497 vertices, 149 frames). The rest pose ($stand1): x forward, y left, z up;
// hunched, his left arm forward, his right arm back with the chainsaw. Measured on his frames
// (Misc/quakevr/ragdoll/rig.py ogre ogre_bones.json). His animation bends more than the others' (1.5 units rms with hands
// of their own, 1.8 without). His chainsaw is four pieces (one tied to his hand by a triangle with a repeated corner,
// which joins nothing), collapsed in his death frames (he drops it: vr_monstermods.cpp): the loose bone, hidden.
// Death frames 112-125 ($death1-14) and 126-135 ($bdeath1-10).
constexpr Seed ogreSeeds[] = {
    {"pelvis", -1, Joint::Root, {0.4f, 0.4f, 6.2f}, {0.4f, 0.4f, 6.2f}, {2.f, -2.5f, 13.6f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {2.4f, -0.8f, 21.f}, {2.f, -2.5f, 13.6f}, {6.5f, -5.4f, 26.4f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {10.f, -6.9f, 31.4f}, {6.5f, -5.4f, 26.4f}, {13.4f, -8.5f, 36.4f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {11.2f, 13.2f, 19.f}, {10.4f, 7.5f, 24.4f}, {15.2f, 14.2f, 14.1f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {20.f, 16.3f, 4.4f}, {15.2f, 14.2f, 14.1f}, {25.1f, 13.5f, 1.4f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_l", 4, Joint::Ball, {28.7f, 12.5f, 3.8f}, {25.1f, 13.5f, 1.4f}, {32.3f, 11.5f, 6.3f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"upperarm_r", 1, Joint::Ball, {-9.6f, -13.9f, 21.3f}, {-3.4f, -13.5f, 24.2f}, {-14.f, -17.2f, 17.2f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 6, Joint::Hinge, {-18.1f, -16.1f, 14.7f}, {-14.f, -17.2f, 17.2f}, {-25.6f, -16.4f, 8.4f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 7, Joint::Ball, {-27.3f, -16.6f, 5.7f}, {-25.6f, -16.4f, 8.4f}, {-28.9f, -16.9f, 3.f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {10.2f, 5.2f, -0.6f}, {5.5f, 4.2f, 3.4f}, {14.8f, 2.8f, -6.f}, 4.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 9, Joint::Hinge, {14.2f, 7.3f, -14.5f}, {14.8f, 2.8f, -6.f}, {13.9f, 10.2f, -20.f}, 4.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {-5.1f, -7.1f, -5.f}, {-4.3f, -3.9f, 1.9f}, {-4.6f, -11.2f, -9.9f}, 4.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 11, Joint::Hinge, {-10.1f, -8.7f, -16.4f}, {-4.6f, -11.2f, -9.9f}, {-13.2f, -7.3f, -20.f}, 4.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// Quake VR's enforcer (quakevr/progs/enforcer.mdl: 479 vertices, 108 frames). The rest pose ($stand1): x forward, y left,
// z up; upright, his laser rifle in both hands before him, a pack on his back (on his chest bone). As the grunt's, his
// legs' long triangles have no vertices of their own: a thigh's bone is its knee's ring, a shin's its boot. Measured on
// his frames (Misc/quakevr/ragdoll/rig.py enforcer enforcer_bones.json): 0.97 units rms. His rifle is collapsed in his
// death frames (he drops it: vr_monstermods.cpp, vr_enemyguns.qc): the loose bone, hidden.
// Death frames 41-54 ($death1-14) and 55-65 ($fdeath1-11).
constexpr Seed enforcerSeeds[] = {
    {"pelvis", -1, Joint::Root, {-4.4f, -0.5f, 2.3f}, {-4.4f, -0.5f, 2.3f}, {-5.7f, 1.7f, 6.f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-11.2f, -1.2f, 18.7f}, {-5.7f, 1.7f, 6.f}, {-5.9f, -1.5f, 25.4f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {-2.2f, -1.2f, 28.f}, {-5.9f, -1.5f, 25.4f}, {1.5f, -0.9f, 30.6f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-5.9f, 12.2f, 21.7f}, {-5.9f, 7.2f, 22.8f}, {-5.1f, 13.7f, 14.5f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {2.2f, 13.3f, 13.3f}, {-5.1f, 13.7f, 14.5f}, {9.6f, 12.8f, 12.1f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {-4.3f, -14.6f, 19.4f}, {-6.2f, -11.3f, 21.8f}, {-1.6f, -17.8f, 15.2f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {2.f, -11.4f, 13.f}, {-1.6f, -17.8f, 15.2f}, {5.7f, -5.1f, 10.7f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {-2.4f, 5.8f, -9.2f}, {-4.f, 4.5f, -3.5f}, {-4.7f, 6.4f, -18.6f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {-3.3f, 8.f, -22.3f}, {-4.7f, 6.4f, -18.6f}, {-3.6f, 7.7f, -21.5f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {-3.f, -7.3f, -9.3f}, {-3.1f, -7.8f, -3.7f}, {-6.8f, -7.6f, -17.6f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {-4.6f, -9.6f, -22.3f}, {-6.8f, -7.6f, -17.6f}, {-5.f, -9.2f, -21.5f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// Quake VR's hell knight, the death knight (quakevr/progs/hknight.mdl: 538 vertices, 167 frames). The rest pose
// ($stand1): x forward, y left, z up; upright, his sword in his right hand. A thigh's bone is the hip's piece, a shin's
// the knee's, a foot's the boot (long triangles between them); his right hand is a bone of its own, his left on its
// forearm. Measured on his frames (Misc/quakevr/ragdoll/rig.py hknight hknight_bones.json): 1.29 units rms (his
// pauldrons move as neither his chest nor his arms: 2.5 on the left arm). His blade and its guard are collapsed in his
// death frames (he drops the sword: vr_monstermods.cpp): the loose bone, hidden.
// Death frames 42-53 ($death1-12) and 54-62 ($deathc1-9).
constexpr Seed hknightSeeds[] = {
    {"pelvis", -1, Joint::Root, {-0.3f, -0.3f, 10.2f}, {-0.3f, -0.3f, 10.2f}, {1.f, 0.5f, 12.6f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {0.4f, 0.2f, 19.9f}, {1.f, 0.5f, 12.6f}, {3.9f, 0.7f, 27.6f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {8.7f, -0.9f, 31.7f}, {3.9f, 0.7f, 27.6f}, {13.5f, -2.4f, 35.8f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {1.9f, 10.4f, 26.2f}, {1.9f, 5.1f, 27.f}, {-1.4f, 10.2f, 19.3f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {2.3f, 13.9f, 13.f}, {-1.4f, 10.2f, 19.3f}, {6.1f, 17.5f, 6.6f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {2.3f, -11.5f, 27.2f}, {1.1f, -4.1f, 24.3f}, {0.6f, -12.5f, 21.9f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {-2.4f, -15.2f, 16.1f}, {0.6f, -12.5f, 21.9f}, {3.1f, -18.3f, 11.3f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 6, Joint::Ball, {5.3f, -19.9f, 9.8f}, {3.1f, -18.3f, 11.3f}, {7.5f, -21.6f, 8.3f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {1.6f, 5.7f, 2.1f}, {0.f, 3.4f, 6.5f}, {6.3f, 6.4f, -8.1f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 8, Joint::Hinge, {4.5f, 8.9f, -9.7f}, {6.3f, 6.4f, -8.1f}, {0.9f, 11.5f, -20.2f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"foot_l", 9, Joint::Ball, {3.4f, 11.8f, -21.8f}, {0.9f, 11.5f, -20.2f}, {5.9f, 12.2f, -23.5f}, 0.f, 35.f, 15.f, 0.f, {}},
    {"thigh_r", 0, Joint::Ball, {1.5f, -6.1f, 2.f}, {-0.2f, -3.9f, 7.1f}, {5.9f, -7.7f, -7.2f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 11, Joint::Hinge, {4.8f, -8.6f, -9.8f}, {5.9f, -7.7f, -7.2f}, {1.2f, -9.3f, -21.6f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"foot_r", 12, Joint::Ball, {3.9f, -11.f, -22.f}, {1.2f, -9.3f, -21.6f}, {6.6f, -12.7f, -22.4f}, 0.f, 35.f, 15.f, 0.f, {}},
};

// Quake VR's rottweiler (quakevr/progs/dog.mdl: 655 vertices, 86 frames, numbered: id's order). The rest pose ($attack1):
// x forward, y left, z up; lunging, his mouth open. A quadruped: the pelvis (the hips and the back) the root, the chest
// (shoulders and ribs) on it by the spine (a ball, 30/20), the head on the chest by the neck, his jaw on the head (a hinge
// that only opens, 40 degrees at most), the tail on the pelvis; each leg an upper part (a ball at the shoulder or the hip,
// 60/20) and a lower one: a hinge across him (keepHinge: his legs lean a little sideways), the forearm folding forward
// (140 degrees: lying, his forearms on the ground before him), the shin backward (120). Measured on his frames (Misc/quakevr/ragdoll/rig.py dog
// dog_bones.json): 0.62 units rms. He holds nothing.
// Death frames 8-16 ($death1-9) and 17-25 ($deathb1-9).
constexpr Seed dogSeeds[] = {
    {"pelvis", -1, Joint::Root, {-8.5f, -0.1f, -0.6f}, {-8.5f, -0.1f, -0.6f}, {1.8f, 0.f, 0.6f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {8.8f, -1.1f, -3.2f}, {1.8f, 0.f, 0.6f}, {15.6f, 0.2f, -2.3f}, 0.f, 30.f, 20.f, 0.f, {}},
    {"head", 1, Joint::Ball, {23.6f, -0.1f, 0.5f}, {15.6f, 0.2f, -2.3f}, {31.5f, -0.4f, 3.3f}, 0.f, 50.f, 40.f, 0.f, {}},
    {"jaw", 2, Joint::Hinge, {26.7f, -0.4f, -3.6f}, {21.3f, -0.5f, -2.7f}, {32.1f, -0.4f, -4.4f}, 0.f, 0.f, 0.f, 40.f, {0.f, 1.f, 0.f}},
    {"upperleg_fl", 1, Joint::Ball, {8.4f, 8.6f, -7.1f}, {9.3f, 4.6f, -1.4f}, {6.7f, 10.6f, -13.6f}, 2.f, 60.f, 20.f, 0.f, {}},
    {"lowerleg_fl", 4, Joint::Hinge, {11.5f, 9.7f, -19.8f}, {6.7f, 10.6f, -13.6f}, {13.7f, 9.3f, -22.6f}, 1.6f, 0.f, 0.f, 140.f, {0.f, -1.f, 0.f}, true},
    {"upperleg_fr", 1, Joint::Ball, {14.7f, -7.5f, -11.5f}, {10.7f, -6.f, -3.8f}, {18.f, -6.f, -18.f}, 2.f, 60.f, 20.f, 0.f, {}},
    {"lowerleg_fr", 6, Joint::Hinge, {24.3f, -8.8f, -22.6f}, {18.f, -6.f, -18.f}, {23.9f, -8.6f, -22.3f}, 1.6f, 0.f, 0.f, 140.f, {0.f, -1.f, 0.f}, true},
    {"thigh_bl", 0, Joint::Ball, {-6.3f, 7.2f, -8.6f}, {-9.8f, 6.f, -0.9f}, {-7.3f, 7.5f, -14.9f}, 2.5f, 60.f, 20.f, 0.f, {}},
    {"shin_bl", 8, Joint::Hinge, {-6.8f, 6.3f, -20.4f}, {-7.3f, 7.5f, -14.9f}, {-6.6f, 5.8f, -22.8f}, 1.6f, 0.f, 0.f, 120.f, {0.f, 1.f, 0.f}, true},
    {"thigh_br", 0, Joint::Ball, {-13.1f, -7.1f, -8.8f}, {-10.3f, -6.1f, -0.6f}, {-15.7f, -6.9f, -13.5f}, 2.5f, 60.f, 20.f, 0.f, {}},
    {"shin_br", 10, Joint::Hinge, {-19.4f, -6.6f, -19.9f}, {-15.7f, -6.9f, -13.5f}, {-21.2f, -6.4f, -23.f}, 1.6f, 0.f, 0.f, 120.f, {0.f, 1.f, 0.f}, true},
    {"tail", 0, Joint::Ball, {-17.5f, -0.1f, -1.f}, {-14.8f, -0.6f, 3.5f}, {-20.3f, 0.4f, -5.4f}, 0.f, 50.f, 30.f, 0.f, {}},
};

// Quake VR's scrag (quakevr/progs/wizard.mdl: 310 vertices, 55 frames). The rest pose ($hover1): x forward, y left, z up;
// upright in the air, his arms out, his tail curled back under him. No legs: the pelvis (his ribbed belly) the root, the
// chest, head, arms and hands (his claws) on it, his tail four bones on the pelvis (balls, 40/20). Measured on his frames
// (Misc/quakevr/ragdoll/rig.py wizard wizard_bones.json): 0.90 units rms (his tail's tip bends: 1.5). (His head's centre is set back from its
// vertices' middle, 7.8 0 29.7: the back of his head's cluster was nearer his right arm's.) He flies: dead, the QC lets
// him fall (monster_death_use clears FL_FLY) and his ragdoll falls limp. He holds nothing.
// Death frames 46-53 ($death1-8).
constexpr Seed wizardSeeds[] = {
    {"pelvis", -1, Joint::Root, {-2.3f, -0.7f, 4.1f}, {-2.3f, -0.7f, 4.1f}, {0.f, 1.4f, 11.7f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {2.f, -0.7f, 19.4f}, {0.f, 1.4f, 11.7f}, {6.f, -1.f, 24.7f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {5.f, -0.4f, 29.f}, {6.f, -1.f, 24.7f}, {9.7f, 0.5f, 34.7f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"arm_l", 1, Joint::Ball, {2.9f, 10.3f, 20.9f}, {1.9f, 6.3f, 15.2f}, {1.6f, 11.2f, 17.1f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"hand_l", 3, Joint::Ball, {0.9f, 18.8f, 14.7f}, {1.6f, 11.2f, 17.1f}, {0.1f, 26.3f, 12.2f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"arm_r", 1, Joint::Ball, {0.9f, -6.8f, 22.9f}, {-0.1f, -5.3f, 19.2f}, {3.1f, -11.3f, 16.4f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"hand_r", 5, Joint::Ball, {0.5f, -20.3f, 14.1f}, {3.1f, -11.3f, 16.4f}, {-2.2f, -29.3f, 11.7f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"tail1", 0, Joint::Ball, {-8.f, -0.6f, -12.f}, {-3.3f, -1.8f, -10.6f}, {-10.4f, 0.3f, -19.3f}, 0.f, 40.f, 20.f, 0.f, {}},
    {"tail2", 7, Joint::Ball, {-17.7f, 0.7f, -18.8f}, {-10.4f, 0.3f, -19.3f}, {-23.6f, 1.8f, -19.8f}, 0.f, 40.f, 20.f, 0.f, {}},
    {"tail3", 8, Joint::Ball, {-28.3f, 0.8f, -17.8f}, {-23.6f, 1.8f, -19.8f}, {-34.3f, 0.9f, -17.f}, 0.f, 40.f, 20.f, 0.f, {}},
    {"tail4", 9, Joint::Ball, {-38.3f, -1.3f, -15.2f}, {-34.3f, 0.9f, -17.f}, {-42.3f, -3.5f, -13.4f}, 0.f, 40.f, 20.f, 0.f, {}},
};

// Quake VR's zombie (quakevr/progs/zombie.mdl: 481 vertices, 199 frames). The rest pose ($stand1): x forward, y left, z up;
// upright, his arms hanging at his sides, his left leg forward. His ragdoll is made only when he is beheaded (he is gibbed
// otherwise), from any frame. Measured on his frames (Misc/quakevr/ragdoll/rig.py zombie zombie_bones.json): clusters 0.62
// units rms, bones 0.79. The flesh he throws is a piece collapsed in some frames: the loose bone, hidden. No death frames:
// his falls stand in (their poses' occlusion, Go Limp At), painb1-14 (103-116) and paine1-17 (162-178).
constexpr Seed zombieSeeds[] = {
    {"pelvis", -1, Joint::Root, {0.f, 0.2f, 4.1f}, {0.f, 0.2f, 4.1f}, {-0.6f, -0.3f, 8.2f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-0.6f, 0.2f, 14.1f}, {-0.6f, -0.3f, 8.2f}, {-2.f, 0.9f, 21.4f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {-2.1f, 0.7f, 24.2f}, {-2.f, 0.9f, 21.4f}, {-2.3f, 0.5f, 27.f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-0.4f, 7.1f, 12.7f}, {-1.f, 6.2f, 18.6f}, {0.5f, 7.5f, 6.9f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {1.f, 8.1f, 0.f}, {0.5f, 7.5f, 6.9f}, {1.6f, 8.7f, -6.8f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {-2.5f, -6.5f, 12.2f}, {-2.6f, -5.9f, 18.5f}, {-2.6f, -6.6f, 5.9f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {-2.4f, -6.9f, -2.1f}, {-2.6f, -6.6f, 5.9f}, {-2.3f, -7.2f, -10.2f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {4.1f, 5.2f, -10.f}, {4.9f, 4.7f, -6.4f}, {3.9f, 6.7f, -19.1f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {5.6f, 6.6f, -21.2f}, {3.9f, 6.7f, -19.1f}, {5.6f, 6.6f, -21.2f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {-2.4f, -4.6f, -11.3f}, {-0.1f, -2.2f, -5.f}, {-5.8f, -3.6f, -18.5f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {-4.8f, -5.7f, -21.3f}, {-5.8f, -3.6f, -18.5f}, {-4.8f, -5.7f, -21.2f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// Quake VR's fiend (quakevr/progs/demon.mdl: 1095 vertices, 69 frames). The rest pose ($stand1): x forward, y left, z up;
// crouched low, his head thrust forward, his arms hanging to the ground before him, his tail along it behind. Pelvis,
// chest, head, upper arms, forearms (elbow hinges) and claws, thighs, shins (knee hinges) and feet (balls 35/15), the
// tail one bone (his rig's 16th). Measured on his frames (Misc/quakevr/ragdoll/rig.py demon demon_bones.json, 24
// clusters: 0.74 units rms): bones 0.83 (his ankles and right knee at their boundaries: the fit put them off the leg).
// He holds nothing.
// Death frames 45-53 ($death1-9).
constexpr Seed demonSeeds[] = {
    {"pelvis", -1, Joint::Root, {-3.2f, -0.9f, -4.6f}, {-3.2f, -0.9f, -4.6f}, {0.5f, -1.2f, 1.7f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {11.4f, -0.2f, 1.f}, {0.5f, -1.2f, 1.7f}, {20.1f, 0.9f, -3.6f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {26.7f, 0.9f, -5.4f}, {20.1f, 0.9f, -3.6f}, {33.2f, 0.9f, -7.2f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {16.3f, 13.2f, -2.5f}, {17.8f, 11.f, -1.4f}, {14.3f, 13.8f, -4.5f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {12.9f, 14.5f, -10.6f}, {14.3f, 13.8f, -4.5f}, {11.f, 12.7f, -14.9f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_l", 4, Joint::Ball, {12.3f, 5.1f, -21.4f}, {11.f, 12.7f, -14.9f}, {13.6f, -2.5f, -27.9f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"upperarm_r", 1, Joint::Ball, {14.5f, -12.8f, -7.2f}, {16.3f, -11.f, -1.4f}, {11.5f, -14.f, -10.3f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 6, Joint::Hinge, {15.2f, -13.3f, -14.6f}, {11.5f, -14.f, -10.3f}, {16.3f, -11.4f, -17.5f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 7, Joint::Ball, {20.5f, -2.2f, -22.7f}, {16.3f, -11.4f, -17.5f}, {24.7f, 7.f, -28.f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {-1.8f, 7.3f, -10.1f}, {-3.4f, 3.4f, -5.6f}, {-4.3f, 7.4f, -13.8f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 9, Joint::Hinge, {-5.4f, 7.5f, -14.8f}, {-4.3f, 7.4f, -13.8f}, {-2.5f, 7.9f, -17.2f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"foot_l", 10, Joint::Ball, {1.6f, 7.7f, -20.7f}, {-2.5f, 7.9f, -17.2f}, {5.8f, 7.5f, -24.2f}, 0.f, 35.f, 15.f, 0.f, {}},
    {"thigh_r", 0, Joint::Ball, {-2.5f, -9.1f, -12.1f}, {-1.5f, -8.4f, -6.4f}, {-6.3f, -8.8f, -14.6f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 12, Joint::Hinge, {-6.4f, -9.1f, -15.5f}, {-6.3f, -8.8f, -14.6f}, {-0.8f, -8.8f, -17.9f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"foot_r", 13, Joint::Ball, {1.8f, -9.2f, -21.3f}, {-0.8f, -8.8f, -17.9f}, {4.4f, -9.6f, -24.7f}, 0.f, 35.f, 15.f, 0.f, {}},
    {"tail", 0, Joint::Ball, {-21.8f, -3.4f, -19.f}, {-11.7f, 1.1f, -12.4f}, {-31.9f, -7.9f, -25.6f}, 0.f, 40.f, 20.f, 0.f, {}},
};

// Quake VR's shambler (quakevr/progs/shambler.mdl: 648 vertices, 94 frames, unnamed). The rest pose ($stand1): x forward,
// y left, z up; upright, his arms out and down, his claws spread. Pelvis (his belly), chest (the shoulders' hump), head (the
// face at the hump's front), upper arms (the shoulders on them), forearms (elbow hinges) and claws, thighs and shins (the
// foot on it; knee hinges; capsules 5.5 and 5). Measured on his frames (Misc/quakevr/ragdoll/rig.py shambler
// shambler_bones.json, 24 clusters: 1.21 units rms): bones 1.57 (his hump and claws bend: 2.0). He holds nothing.
// Death frames 83-93 ($death1-11).
constexpr Seed shamblerSeeds[] = {
    {"pelvis", -1, Joint::Root, {-12.2f, -1.2f, 16.6f}, {-12.2f, -1.2f, 16.6f}, {-11.1f, -1.9f, 26.6f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-3.1f, -1.8f, 43.5f}, {-11.1f, -1.9f, 26.6f}, {6.7f, -2.2f, 49.2f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {16.9f, -3.f, 48.f}, {6.7f, -2.2f, 49.2f}, {27.1f, -3.7f, 46.8f}, 0.f, 30.f, 30.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-1.9f, 27.4f, 39.1f}, {-2.8f, 14.5f, 52.1f}, {-3.8f, 33.4f, 29.3f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {9.f, 33.8f, 22.3f}, {-3.8f, 33.4f, 29.3f}, {18.8f, 35.1f, 18.5f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_l", 4, Joint::Ball, {29.3f, 29.5f, 18.f}, {18.8f, 35.1f, 18.5f}, {39.7f, 23.9f, 17.5f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"upperarm_r", 1, Joint::Ball, {-5.3f, -25.8f, 40.7f}, {-4.4f, -18.1f, 49.4f}, {-7.9f, -31.1f, 28.6f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 6, Joint::Hinge, {-3.4f, -35.2f, 18.7f}, {-7.9f, -31.1f, 28.6f}, {7.7f, -39.6f, 8.4f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 7, Joint::Ball, {18.1f, -40.f, 4.6f}, {7.7f, -39.6f, 8.4f}, {28.5f, -40.3f, 0.8f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {-4.f, 12.7f, 0.9f}, {-11.5f, 9.6f, 14.8f}, {-7.f, 14.4f, -7.5f}, 5.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 9, Joint::Hinge, {-10.f, 13.6f, -17.6f}, {-7.f, 14.4f, -7.5f}, {-10.3f, 13.6f, -18.8f}, 5.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {-1.8f, -14.4f, 0.5f}, {-9.9f, -13.3f, 14.f}, {-2.7f, -14.2f, -12.1f}, 5.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 11, Joint::Hinge, {-2.8f, -16.7f, -19.2f}, {-2.7f, -14.2f, -12.1f}, {-2.8f, -16.6f, -18.8f}, 5.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// Hipnotic's gremlin (Scourge of Armagon's progs/grem.mdl: 123 vertices, 179 frames). The rest pose ($stand1): x forward,
// y left, z up; hunched, his head forward, his arms out, his ears up. Few vertices and loose frames: the body one bone (the
// pelvis: belly, back and hump; its seed's centre moved up to 0 2 3 so his hump's cluster isn't the head's), the head on
// it, upper arms and forearms (elbow hinges), thighs and shins (knee hinges; capsules 2 and 1.8). Measured on his frames
// (Misc/quakevr/ragdoll/rig.py grem grem_bones.json, RIG_PAK: Hipnotic's pak0): clusters 1.33 units rms, bones 1.50. The gun he
// steals is a piece of its own, collapsed in his death frames (he drops it: vr_monstermods.cpp): the loose bone, hidden.
// Death frames 104-115 ($death1-12) and 116-123 ($flip1-8: thrown up and back).
constexpr Seed gremSeeds[] = {
    {"pelvis", -1, Joint::Root, {0.f, 2.f, 3.f}, {-2.6f, 1.8f, -0.7f}, {4.8f, 1.6f, 7.9f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"head", 0, Joint::Ball, {10.5f, 0.3f, 7.9f}, {4.8f, 1.6f, 7.9f}, {16.1f, -1.f, 7.8f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 0, Joint::Ball, {3.8f, 13.6f, 0.6f}, {0.f, 8.9f, 4.8f}, {8.2f, 16.9f, -4.1f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 2, Joint::Hinge, {11.4f, 13.5f, -7.8f}, {8.2f, 16.9f, -4.1f}, {14.5f, 10.2f, -11.4f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 0, Joint::Ball, {2.1f, -8.1f, 5.f}, {2.4f, -3.6f, 8.f}, {1.9f, -12.1f, 0.8f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 4, Joint::Hinge, {6.6f, -13.9f, -5.6f}, {1.9f, -12.1f, 0.8f}, {11.2f, -15.7f, -11.9f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {1.9f, 10.7f, -9.6f}, {4.3f, 10.f, -5.2f}, {1.5f, 10.5f, -15.5f}, 2.f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 6, Joint::Hinge, {2.2f, 11.7f, -21.1f}, {1.5f, 10.5f, -15.5f}, {2.3f, 11.8f, -21.4f}, 1.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {2.1f, -9.4f, -5.2f}, {3.1f, -10.1f, -3.7f}, {-4.1f, -9.6f, -11.f}, 2.f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 8, Joint::Hinge, {-5.1f, -11.f, -16.9f}, {-4.1f, -9.6f, -11.f}, {-6.f, -12.1f, -21.7f}, 1.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// Rogue's mummy (Dissolution of Eternity's progs/mummy.mdl: 177 vertices, 192 frames, the zombie's animations). The rest
// pose ($stand1): x forward, y left, z up; upright, his arms at his sides. As the zombie's: pelvis, chest, head, upper arms
// and forearms (elbow hinges), thighs (the knee's ring) and shins (knee hinges). His ragdoll is made only when he is
// beheaded (his death gibs him otherwise: mummy_die). Measured on his frames (Misc/quakevr/ragdoll/rig.py mummy
// mummy_bones.json, RIG_PAK: Rogue's pak0): clusters 0.52 units rms, bones 0.72. The flesh he throws is the loose bone,
// hidden. No death frames: his falls stand in, painb1-14 (103-116) and paine1-17 (162-178), as the zombie's.
constexpr Seed mummySeeds[] = {
    {"pelvis", -1, Joint::Root, {-0.2f, 0.2f, 9.3f}, {-0.2f, 0.2f, 9.3f}, {-0.9f, -0.2f, 13.4f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-1.3f, 0.4f, 21.7f}, {-0.9f, -0.2f, 13.4f}, {-2.5f, 0.9f, 29.1f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {-2.1f, 0.9f, 31.7f}, {-2.5f, 0.9f, 29.1f}, {-1.7f, 0.9f, 34.3f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-0.6f, 8.2f, 19.9f}, {-0.9f, 7.3f, 25.9f}, {0.5f, 8.6f, 14.1f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {1.f, 9.2f, 6.2f}, {0.5f, 8.6f, 14.1f}, {1.4f, 9.9f, -1.6f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {-2.9f, -7.4f, 19.5f}, {-2.8f, -6.9f, 25.9f}, {-2.9f, -7.5f, 13.f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {-2.9f, -7.8f, 4.1f}, {-2.9f, -7.5f, 13.f}, {-3.f, -8.1f, -4.8f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {5.3f, 6.4f, -8.6f}, {6.2f, 7.1f, -5.7f}, {4.3f, 9.3f, -20.f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {7.7f, 8.f, -22.7f}, {4.3f, 9.3f, -20.f}, {6.f, 8.6f, -21.4f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {-2.9f, -5.7f, -10.2f}, {1.f, -3.7f, -6.4f}, {-8.5f, -5.2f, -20.2f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {-5.1f, -7.1f, -22.7f}, {-8.5f, -5.2f, -20.2f}, {-6.8f, -6.2f, -21.4f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// The vore (Quake VR's progs/shalrath.mdl, its own: 371 vertices, 36 frames). The rest pose ($attack1, the model's stand1):
// x forward, y left, z up; upright on three legs (two forward and out, one back), his arms out wide. Pelvis (the round
// belly the legs meet), chest, head, jaw (his two hanging tendrils, meshes of their own: on the head, they go with it when
// it's cut off), upper arms and forearms (elbow hinges folding forward), and each leg a thigh (ball) and a shin (knee
// hinge, from his bent rest). The back leg's thigh is its knee's knob (its long triangles have no vertices of their own):
// its pivot set at his back by hand, as the jaw's (rig.py's fit sits between the tendrils). Measured on his frames
// (Misc/quakevr/ragdoll/rig.py shalrath shalrath_bones.json): clusters 1.20 units rms, bones 1.28. Death frames 16-22
// ($death1-7).
constexpr Seed shalrathSeeds[] = {
    {"pelvis", -1, Joint::Root, {2.8f, -1.1f, 14.1f}, {2.8f, -1.1f, 14.1f}, {0.2f, -1.2f, 21.5f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {1.9f, -0.4f, 31.3f}, {0.2f, -1.2f, 21.5f}, {-0.8f, 0.1f, 40.1f}, 0.f, 30.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {-0.2f, -0.5f, 44.7f}, {-0.8f, 0.1f, 40.1f}, {0.4f, -1.2f, 49.4f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"jaw", 2, Joint::Ball, {5.2f, -0.6f, 39.f}, {4.f, -0.5f, 41.f}, {6.f, -0.5f, 33.5f}, 0.f, 30.f, 20.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {0.4f, 20.6f, 28.2f}, {-2.4f, 12.5f, 36.8f}, {3.f, 29.4f, 23.9f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 4, Joint::Hinge, {4.5f, 36.7f, 20.2f}, {3.f, 29.4f, 23.9f}, {5.9f, 44.1f, 16.5f}, 0.f, 0.f, 0.f, 145.f, {0.f, 0.f, -1.f}},
    {"upperarm_r", 1, Joint::Ball, {0.9f, -19.3f, 27.4f}, {0.4f, -11.6f, 34.3f}, {4.5f, -24.9f, 20.6f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 6, Joint::Hinge, {4.1f, -37.3f, 20.4f}, {4.5f, -24.9f, 20.6f}, {3.6f, -49.8f, 20.2f}, 0.f, 0.f, 0.f, 145.f, {0.f, 0.f, 1.f}},
    {"thigh_fl", 0, Joint::Ball, {11.8f, 18.3f, 20.5f}, {7.9f, 5.3f, 16.5f}, {17.3f, 24.3f, 19.3f}, 2.5f, 60.f, 25.f, 0.f, {}},
    {"shin_fl", 8, Joint::Hinge, {18.7f, 28.9f, 6.f}, {17.3f, 24.3f, 19.3f}, {20.1f, 33.4f, -7.3f}, 2.f, 0.f, 0.f, 140.f, {0.8f, -0.5f, 0.f}},
    {"thigh_fr", 0, Joint::Ball, {12.6f, -23.6f, 17.7f}, {3.f, -10.3f, 17.9f}, {14.5f, -26.2f, 16.8f}, 2.5f, 60.f, 25.f, 0.f, {}},
    {"shin_fr", 10, Joint::Hinge, {12.8f, -29.2f, 3.2f}, {14.5f, -26.2f, 16.8f}, {11.2f, -32.2f, -10.5f}, 2.f, 0.f, 0.f, 140.f, {0.8f, 0.5f, 0.f}},
    {"thigh_b", 0, Joint::Ball, {-20.6f, -0.2f, 18.5f}, {-7.f, -0.2f, 15.5f}, {-23.f, 2.f, 12.9f}, 2.5f, 60.f, 25.f, 0.f, {}},
    {"shin_b", 12, Joint::Hinge, {-25.9f, -0.1f, 4.f}, {-23.f, 2.f, 12.9f}, {-28.8f, -2.2f, -4.9f}, 2.f, 0.f, 0.f, 140.f, {0.f, 1.f, 0.f}},
};

// Hipnotic's centroid (Scourge of Armagon's monster_scourge, progs/scor.mdl: 235 vertices, 41 frames). The rest pose
// ($stand1): x forward, y left, z up; a scorpion's body, its tail curled up over it to the sting, its two arms (gun
// pods) forward and out with a pincer pair at each end, three legs a side. The legs, arms and pincers are meshes of their
// own whose motions are so alike that the motion clusters mix them (one cluster over two legs): wholePieces (each one
// bone's, by its middle; rig.py "wholepieces"). The body: pelvis (its back), head (its front, below: h_scourg.mdl the
// gib), the tail three bones; each arm a bone, its pincers its claw; each leg one bone (16 at most); the tail's and the claws' capsules give them weight (a part's
// mass is its share of the volume: the flat pincers alone weighed 0.3 kg against an arm's 47, and shook). Measured on his
// frames (rig.py scor scor_bones.json, RIG_PAK: Hipnotic's pak0; k 26): clusters 0.99 units rms, bones 1.42. Death
// frames 36-40 ($death1-5).
constexpr Seed scorSeeds[] = {
    {"pelvis", -1, Joint::Root, {-10.1f, -1.1f, -4.1f}, {-10.1f, -1.1f, -4.1f}, {6.9f, -0.1f, -9.f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"head", 0, Joint::Ball, {12.9f, -0.2f, -9.2f}, {6.9f, -0.1f, -9.f}, {19.f, -0.4f, -9.4f}, 0.f, 30.f, 30.f, 0.f, {}},
    {"tail1", 0, Joint::Ball, {-23.7f, 1.1f, 5.6f}, {-20.3f, 2.6f, 0.2f}, {-25.5f, -1.4f, 14.2f}, 3.f, 40.f, 20.f, 0.f, {}},
    {"tail2", 2, Joint::Ball, {-21.2f, -0.2f, 20.f}, {-25.5f, -1.4f, 14.2f}, {-9.8f, -1.f, 23.7f}, 2.5f, 45.f, 20.f, 0.f, {}},
    {"tail3", 3, Joint::Ball, {-5.1f, -0.2f, 16.8f}, {-9.8f, -1.f, 23.7f}, {-1.2f, 0.5f, 10.9f}, 2.f, 50.f, 20.f, 0.f, {}},
    {"arm_l", 0, Joint::Ball, {8.3f, 20.6f, -14.8f}, {8.8f, 0.4f, -9.2f}, {13.2f, 25.9f, -15.7f}, 0.f, 60.f, 30.f, 0.f, {}},
    {"claw_l", 5, Joint::Ball, {12.4f, 30.1f, -15.1f}, {13.2f, 25.9f, -15.7f}, {11.5f, 34.3f, -14.5f}, 2.5f, 40.f, 20.f, 0.f, {}},
    {"arm_r", 0, Joint::Ball, {8.7f, -20.9f, -15.4f}, {5.6f, -0.9f, -7.2f}, {12.7f, -29.6f, -15.9f}, 0.f, 60.f, 30.f, 0.f, {}},
    {"claw_r", 7, Joint::Ball, {12.7f, -31.7f, -16.9f}, {12.7f, -29.6f, -15.9f}, {12.7f, -33.8f, -16.9f}, 2.5f, 40.f, 20.f, 0.f, {}},
    {"leg_bl", 0, Joint::Ball, {-15.9f, 12.4f, -11.7f}, {-9.5f, 3.3f, -9.7f}, {-22.4f, 21.5f, -13.7f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_ml", 0, Joint::Ball, {-8.3f, 12.7f, -13.1f}, {-5.8f, 3.2f, -11.f}, {-10.9f, 22.2f, -15.2f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_fl", 0, Joint::Ball, {-1.7f, 10.4f, -11.8f}, {-0.4f, 2.4f, -9.3f}, {-3.f, 18.3f, -14.2f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_br", 0, Joint::Ball, {-16.2f, -12.9f, -12.f}, {-10.9f, -4.3f, -7.9f}, {-21.5f, -21.4f, -16.f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_mr", 0, Joint::Ball, {-8.3f, -13.1f, -13.2f}, {-6.1f, -3.f, -8.6f}, {-10.6f, -23.3f, -17.9f}, 0.f, 60.f, 25.f, 0.f, {}},
    {"leg_fr", 0, Joint::Ball, {-1.7f, -10.8f, -11.9f}, {-0.7f, -2.9f, -8.2f}, {-2.6f, -18.8f, -15.6f}, 0.f, 60.f, 25.f, 0.f, {}},
};

// Dawn of the Machine's rocket ogre (owned/mg3/progs/ogre_rocket.mdl, read from MG3's pack in place: 982 vertices, 147
// frames in id's ogre's order: its death frames checked against Quake VR's ogre's, ROUND21.md "Dawn of the Machine (MG3):
// monsters"). The rest pose ($stand1): x forward, y left, z up; upright, his left arm down (his rocket launcher on its
// forearm), his chainsaw (a box and a long bar behind him) in his right hand: collapsed in his death frames (he drops
// it: vr_monstermods.cpp), the loose bone, hidden; his right hand's bone on his fist (rig.py had it on the chainsaw).
// Measured on his frames
// (Misc/quakevr/ragdoll/rig.py ogre_rocket ogre_rocket_bones.json, RIG_PAK MG3's pak0.pak): clusters 1.06 units rms,
// bones 1.19. Death frames 112-125 ($death1-14) and 126-135 ($bdeath1-10), as the ogre's.
constexpr Seed ogreRocketSeeds[] = {
    {"pelvis", -1, Joint::Root, {1.4f, -1.1f, 4.5f}, {1.4f, -1.1f, 4.5f}, {-2.4f, -0.6f, 13.f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {3.9f, -0.4f, 23.5f}, {-2.4f, -0.6f, 13.f}, {5.4f, -2.1f, 24.2f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {11.f, -1.6f, 31.4f}, {5.4f, -2.1f, 24.2f}, {16.5f, -1.f, 38.6f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {2.6f, 14.8f, 26.2f}, {3.7f, 9.2f, 25.2f}, {-2.4f, 15.3f, 19.2f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {1.6f, 17.6f, 6.9f}, {-2.4f, 15.3f, 19.2f}, {3.1f, 17.7f, 1.7f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_l", 4, Joint::Ball, {10.3f, 17.4f, -13.7f}, {3.1f, 17.7f, 1.7f}, {17.5f, 17.1f, -29.1f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"upperarm_r", 1, Joint::Ball, {2.8f, -16.5f, 17.3f}, {3.6f, -13.5f, 21.5f}, {0.f, -20.6f, 11.3f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 6, Joint::Hinge, {0.4f, -19.7f, 7.2f}, {0.f, -20.6f, 11.3f}, {1.9f, -20.5f, 0.3f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 7, Joint::Ball, {0.2f, -20.4f, -2.f}, {1.9f, -20.5f, 0.3f}, {-0.9f, -20.6f, -5.f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {2.9f, 6.9f, -5.8f}, {-3.2f, 5.7f, 4.4f}, {4.7f, 12.7f, -10.4f}, 4.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 9, Joint::Hinge, {4.f, 7.4f, -20.9f}, {4.7f, 12.7f, -10.4f}, {4.f, 7.7f, -20.4f}, 4.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {1.3f, -8.6f, -10.6f}, {-0.4f, -11.6f, -2.1f}, {-2.3f, -12.8f, -19.5f}, 4.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 11, Joint::Hinge, {2.1f, -9.5f, -22.6f}, {-2.3f, -12.8f, -19.5f}, {-1.1f, -11.9f, -20.4f}, 4.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// Dawn of the Machine's demo dog (owned/mg3/progs/dog_explosive.mdl, read in place: 915 vertices, 86 frames in id's
// rottweiler's order, checked: frameorder.py dog dog_explosive). The rottweiler's build with a bomb pack on his back
// (its two barrels on the pelvis and the chest); the rest pose $attack1 as the rottweiler's; his bones the rottweiler's
// (keepHinge on the lower legs, as his). Measured on his frames (rig.py dog_explosive dog_explosive_bones.json, RIG_PAK
// MG3's pak0.pak): clusters 0.58 units rms, bones 0.71. Death frames 8-16 ($death1-9) and 17-25 ($deathb1-9), used
// only knocked down or beheaded (he always bursts otherwise: QC vr_mg3_demodog.qc).
constexpr Seed dogExplosiveSeeds[] = {
    {"pelvis", -1, Joint::Root, {-6.7f, -0.1f, 3.8f}, {-6.7f, -0.1f, 3.8f}, {0.8f, -0.1f, 1.9f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {6.1f, -0.3f, 4.6f}, {0.8f, -0.1f, 1.9f}, {15.2f, 0.1f, -2.2f}, 0.f, 30.f, 20.f, 0.f, {}},
    {"head", 1, Joint::Ball, {23.4f, -0.1f, 0.3f}, {15.2f, 0.1f, -2.2f}, {31.6f, -0.3f, 2.8f}, 0.f, 50.f, 40.f, 0.f, {}},
    {"jaw", 2, Joint::Hinge, {26.5f, -0.5f, -3.7f}, {21.2f, -0.4f, -2.8f}, {31.8f, -0.5f, -4.7f}, 0.f, 0.f, 0.f, 40.f, {0.f, 1.f, 0.f}},
    {"upperleg_fl", 1, Joint::Ball, {8.8f, 8.f, -3.4f}, {8.2f, 3.8f, 0.4f}, {7.2f, 10.8f, -7.8f}, 2.f, 60.f, 20.f, 0.f, {}},
    {"lowerleg_fl", 4, Joint::Hinge, {10.8f, 9.5f, -18.f}, {7.2f, 10.8f, -7.8f}, {12.4f, 8.8f, -22.6f}, 1.6f, 0.f, 0.f, 140.f, {0.f, -1.f, 0.f}, true},
    {"upperleg_fr", 1, Joint::Ball, {12.4f, -7.5f, -6.8f}, {10.f, -3.9f, -1.7f}, {14.7f, -9.8f, -16.9f}, 2.f, 60.f, 20.f, 0.f, {}},
    {"lowerleg_fr", 6, Joint::Hinge, {20.5f, -8.3f, -19.4f}, {14.7f, -9.8f, -16.9f}, {26.3f, -6.8f, -21.9f}, 1.6f, 0.f, 0.f, 140.f, {0.f, -1.f, 0.f}, true},
    {"thigh_bl", 0, Joint::Ball, {-6.5f, 6.9f, -8.2f}, {-9.7f, 6.f, -0.9f}, {-7.3f, 8.1f, -14.8f}, 2.5f, 60.f, 20.f, 0.f, {}},
    {"shin_bl", 8, Joint::Hinge, {-6.8f, 6.3f, -20.4f}, {-7.3f, 8.1f, -14.8f}, {-6.6f, 5.5f, -22.9f}, 1.6f, 0.f, 0.f, 120.f, {0.f, 1.f, 0.f}, true},
    {"thigh_br", 0, Joint::Ball, {-13.2f, -6.9f, -8.4f}, {-10.f, -5.8f, -1.f}, {-15.9f, -6.3f, -13.8f}, 2.5f, 60.f, 20.f, 0.f, {}},
    {"shin_br", 10, Joint::Hinge, {-19.4f, -6.5f, -19.9f}, {-15.9f, -6.3f, -13.8f}, {-21.3f, -6.6f, -23.1f}, 1.6f, 0.f, 0.f, 120.f, {0.f, 1.f, 0.f}, true},
    {"tail", 0, Joint::Ball, {-17.5f, -0.1f, -1.1f}, {-14.8f, -0.5f, 3.3f}, {-20.2f, 0.4f, -5.6f}, 0.f, 50.f, 30.f, 0.f, {}},
};

// Dawn of the Machine's ranged knight (owned/mg3/progs/rknight.mdl, read in place: 1155 vertices, 166 frames in id's
// death knight's order, checked: frameorder.py hknight rknight). The rest pose ($stand1): x forward, y left, z up;
// upright, clawed, no weapon: pauldrons on the upper arms, his left hand on its forearm, his right claw a piece of its
// own (the hand). The death knight's bones. Measured on his frames (rig.py rknight rknight_bones.json, RIG_PAK MG3's
// pak0.pak): clusters 0.63 units rms, bones 0.77 (the death knight: 1.02, 1.29).
// Death frames 42-53 ($death1-12) and 54-62 ($deathb1-9).
constexpr Seed rknightSeeds[] = {
    {"pelvis", -1, Joint::Root, {0.4f, -0.4f, 9.7f}, {0.4f, -0.4f, 9.7f}, {0.f, 0.3f, 14.f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {3.9f, -0.6f, 23.8f}, {0.f, 0.3f, 14.f}, {5.7f, -1.1f, 25.6f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {8.8f, -1.4f, 33.f}, {5.7f, -1.1f, 25.6f}, {11.9f, -1.7f, 40.5f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-0.9f, 12.8f, 21.2f}, {1.5f, 9.9f, 26.7f}, {-1.9f, 12.7f, 16.3f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {7.7f, 15.5f, 5.5f}, {-1.9f, 12.7f, 16.3f}, {17.4f, 18.4f, -5.2f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {-0.5f, -14.9f, 19.6f}, {1.3f, -12.1f, 25.2f}, {-1.7f, -16.f, 13.4f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {4.4f, -19.6f, 11.f}, {-1.7f, -16.f, 13.4f}, {5.5f, -20.9f, 9.7f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 6, Joint::Ball, {11.6f, -23.8f, 6.3f}, {5.5f, -20.9f, 9.7f}, {17.7f, -26.7f, 2.8f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {3.f, 7.2f, -2.3f}, {0.6f, 4.7f, 4.5f}, {5.4f, 7.9f, -8.1f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 8, Joint::Hinge, {3.8f, 8.8f, -10.9f}, {5.4f, 7.9f, -8.1f}, {-0.2f, 11.7f, -20.2f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"foot_l", 9, Joint::Ball, {2.3f, 12.f, -22.6f}, {-0.2f, 11.7f, -20.2f}, {4.8f, 12.4f, -25.1f}, 0.f, 35.f, 15.f, 0.f, {}},
    {"thigh_r", 0, Joint::Ball, {2.7f, -7.3f, -0.9f}, {0.2f, -4.5f, 5.4f}, {5.7f, -8.2f, -7.6f}, 3.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 11, Joint::Hinge, {4.7f, -8.9f, -10.4f}, {5.7f, -8.2f, -7.6f}, {0.5f, -11.3f, -20.2f}, 3.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"foot_r", 12, Joint::Ball, {2.7f, -11.4f, -23.f}, {0.5f, -11.3f, -20.2f}, {4.9f, -11.6f, -25.7f}, 0.f, 35.f, 15.f, 0.f, {}},
};

// Dawn of the Machine's super shambler (MG3's progs/shambler_blood.mdl, read in place from the owned pack: 1076 vertices,
// 96 frames, unnamed; the shambler's 94 first, as its QC's $frames). The rest pose ($stand1): x forward, y left, z up;
// taller than Quake VR's shambler (74 up, his face further forward), his claws low. The shambler's bones: pelvis (his
// belly), chest (the hump), head (the face and jaw at the hump's front), upper arms, forearms (elbow hinges) and claws,
// thighs and shins (knee hinges; capsules 5.5 and 5). Measured on his frames (Misc/quakevr/ragdoll/rig.py shambler_blood
// shambler_blood_bones.json, RIG_PAK: MG3's pak0; 24 clusters: 1.29 units rms): bones 1.77. Seed centres set by hand
// where the fit's would give a cluster to another bone (rig.py's warnings): pelvis 7 -1 26 (the belly's front, not the
// head's), chest 12 -3 55, head 31 -3.5 52 (its brow, not the hump's), thigh_l 5.4 12.9 8.4 (the hip's cluster the
// pelvis's). Death frames 83-93 ($death1-11).
constexpr Seed shamblerBloodSeeds[] = {
    {"pelvis", -1, Joint::Root, {7.f, -1.f, 26.f}, {7.6f, -0.9f, 30.1f}, {8.6f, 0.4f, 32.8f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {12.f, -3.f, 55.f}, {8.6f, 0.4f, 32.8f}, {22.2f, -1.1f, 48.f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {31.f, -3.5f, 52.f}, {22.2f, -1.1f, 48.f}, {42.6f, -5.5f, 47.3f}, 0.f, 30.f, 30.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {8.2f, 22.9f, 44.f}, {10.1f, 11.4f, 53.8f}, {8.8f, 30.7f, 32.6f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {25.9f, 33.8f, 19.6f}, {8.8f, 30.7f, 32.6f}, {30.9f, 36.6f, 18.5f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_l", 4, Joint::Ball, {40.8f, 29.4f, 17.7f}, {30.9f, 36.6f, 18.5f}, {50.7f, 22.2f, 16.9f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"upperarm_r", 1, Joint::Ball, {3.9f, -26.7f, 41.3f}, {10.1f, -17.3f, 51.3f}, {0.3f, -30.8f, 28.3f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 6, Joint::Hinge, {13.f, -38.5f, 12.f}, {0.3f, -30.8f, 28.3f}, {17.8f, -45.6f, 11.8f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"hand_r", 7, Joint::Ball, {26.8f, -42.6f, 7.2f}, {17.8f, -45.6f, 11.8f}, {35.9f, -39.6f, 2.7f}, 0.f, 40.f, 30.f, 0.f, {}},
    {"thigh_l", 0, Joint::Ball, {5.4f, 12.9f, 8.4f}, {-0.5f, 5.8f, 20.6f}, {6.3f, 15.1f, 0.2f}, 5.5f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 9, Joint::Hinge, {1.7f, 12.9f, -13.8f}, {6.3f, 15.1f, 0.2f}, {-0.3f, 11.9f, -20.1f}, 5.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {5.9f, -13.7f, 5.2f}, {-1.2f, -8.4f, 19.1f}, {7.1f, -12.6f, -9.3f}, 5.5f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 11, Joint::Hinge, {5.2f, -15.7f, -18.f}, {7.1f, -12.6f, -9.3f}, {4.6f, -16.6f, -20.7f}, 5.f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

// The player (Quake VR's progs/player.mdl: 733 vertices, 144 frames; ROUND21.md, "Player ragdolls"). The rest pose
// ($axrun1, frame 0): x forward, y left, z up; mid-stride, his left leg forward, his right one back, his left arm swung
// back, the axe in his right hand and the gun on his back. His axe and gun are pieces of their own (each in his hands or
// on his back by the frame: rigid with no bone): loose, hidden in his ragdoll (looseWeapons). Measured with
// Misc/quakevr/ragdoll/rig.py (player_bones.json; 18 clusters: 0.70 units rms; bones 0.82). The chest's seed centre moved
// by hand 2 units back and down (cluster 15, the left shoulder blade's, is the chest's: rig.py had it nearer the upper
// arm's). Death frames 50-60 ($deatha1-11) and 61-69 ($deathb1-9) of his six.
constexpr Seed playerSeeds[] = {
    {"pelvis", -1, Joint::Root, {-6.8f, 0.4f, 2.7f}, {-6.8f, 0.4f, 2.7f}, {-3.8f, 0.f, 7.4f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-1.5f, 0.5f, 13.f}, {-3.8f, 0.f, 7.4f}, {3.f, -0.6f, 19.8f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {7.5f, 0.1f, 22.1f}, {3.f, -0.6f, 19.8f}, {12.1f, 0.8f, 24.4f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {-4.6f, 9.7f, 15.2f}, {-0.2f, 6.f, 18.f}, {-8.1f, 12.f, 10.8f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {-11.9f, 14.2f, 4.5f}, {-8.1f, 12.f, 10.8f}, {-15.6f, 16.3f, -1.8f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {2.3f, -10.3f, 12.f}, {3.1f, -8.f, 16.7f}, {2.5f, -11.7f, 5.1f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {6.3f, -11.6f, 1.f}, {2.5f, -11.7f, 5.1f}, {10.2f, -11.6f, -3.2f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {6.1f, 4.2f, -6.8f}, {-2.8f, 5.1f, -0.4f}, {7.8f, 5.7f, -14.3f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {10.3f, 4.3f, -19.f}, {7.8f, 5.7f, -14.3f}, {10.f, 4.5f, -18.5f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {-11.4f, -4.8f, -10.5f}, {-8.1f, -6.7f, 0.4f}, {-19.4f, -4.7f, -11.9f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {-22.4f, -4.5f, -16.3f}, {-19.4f, -4.7f, -11.9f}, {-24.6f, -4.4f, -19.4f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

constexpr SeedTable seedTables[] = {
    {modelmeta::Id::Soldier, 555, gruntSeeds, static_cast<int>(sizeof(gruntSeeds) / sizeof(gruntSeeds[0])), 2, {8, 18}, {17, 28}},
    {modelmeta::Id::Knight, 655, knightSeeds, static_cast<int>(sizeof(knightSeeds) / sizeof(knightSeeds[0])), 2, {76, 86}, {85, 96}},
    {modelmeta::Id::Ogre, 497, ogreSeeds, static_cast<int>(sizeof(ogreSeeds) / sizeof(ogreSeeds[0])), 2, {112, 126}, {125, 135}},
    {modelmeta::Id::Enforcer, 479, enforcerSeeds, static_cast<int>(sizeof(enforcerSeeds) / sizeof(enforcerSeeds[0])), 2, {41, 55}, {54, 65}},
    {modelmeta::Id::Hknight, 538, hknightSeeds, static_cast<int>(sizeof(hknightSeeds) / sizeof(hknightSeeds[0])), 2, {42, 54}, {53, 62}},
    {modelmeta::Id::Dog, 655, dogSeeds, static_cast<int>(sizeof(dogSeeds) / sizeof(dogSeeds[0])), 2, {8, 17}, {16, 25}},
    {modelmeta::Id::Wizard, 310, wizardSeeds, static_cast<int>(sizeof(wizardSeeds) / sizeof(wizardSeeds[0])), 1, {46, 0}, {53, 0}},
    {modelmeta::Id::Zombie, 481, zombieSeeds, static_cast<int>(sizeof(zombieSeeds) / sizeof(zombieSeeds[0])), 2, {103, 162}, {116, 178}},
    {modelmeta::Id::Demon, 1095, demonSeeds, static_cast<int>(sizeof(demonSeeds) / sizeof(demonSeeds[0])), 1, {45, 0}, {53, 0}, 24},
    {modelmeta::Id::Shambler, 648, shamblerSeeds, static_cast<int>(sizeof(shamblerSeeds) / sizeof(shamblerSeeds[0])), 1, {83, 0}, {93, 0}, 24},
    {modelmeta::Id::Grem, 123, gremSeeds, static_cast<int>(sizeof(gremSeeds) / sizeof(gremSeeds[0])), 2, {104, 116}, {115, 123}},
    {modelmeta::Id::Mummy, 177, mummySeeds, static_cast<int>(sizeof(mummySeeds) / sizeof(mummySeeds[0])), 2, {103, 162}, {116, 178}},
    {modelmeta::Id::Shalrath, 371, shalrathSeeds, static_cast<int>(sizeof(shalrathSeeds) / sizeof(shalrathSeeds[0])), 1, {16, 0}, {22, 0}},
    {modelmeta::Id::Scor, 235, scorSeeds, static_cast<int>(sizeof(scorSeeds) / sizeof(scorSeeds[0])), 1, {36, 0}, {40, 0}, 26, true},
    {modelmeta::Id::Mg3DogExplosive, 915, dogExplosiveSeeds, static_cast<int>(sizeof(dogExplosiveSeeds) / sizeof(dogExplosiveSeeds[0])), 2, {8, 17}, {16, 25}},
    {modelmeta::Id::Mg3Rknight, 1155, rknightSeeds, static_cast<int>(sizeof(rknightSeeds) / sizeof(rknightSeeds[0])), 2, {42, 54}, {53, 62}},
    {modelmeta::Id::Mg3OgreRocket, 982, ogreRocketSeeds, static_cast<int>(sizeof(ogreRocketSeeds) / sizeof(ogreRocketSeeds[0])), 2, {112, 126}, {125, 135}},
    {modelmeta::Id::Mg3ShamblerBlood, 1076, shamblerBloodSeeds, static_cast<int>(sizeof(shamblerBloodSeeds) / sizeof(shamblerBloodSeeds[0])), 1, {83, 0}, {93, 0}, 24},
    {modelmeta::Id::Player, 733, playerSeeds, static_cast<int>(sizeof(playerSeeds) / sizeof(playerSeeds[0])), 2, {50, 61}, {60, 69}, 18, false, true},
};

[[nodiscard]] const SeedTable* tableOf(const qmodel_t* model)
{
    if(!model || model->type != mod_alias)
    {
        return nullptr;
    }
    const auto& info = modelmeta::get(model);
    for(const SeedTable& t : seedTables)
    {
        if(info.is(t.model))
        {
            return &t;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// The rigid fit (Horn, "Closed-form solution of absolute orientation using unit quaternions", 1987): the rotation that
// best carries centred points a onto centred points b is the eigenvector of the largest eigenvalue of a symmetric 4x4.

// A symmetric 4x4's eigenvector of its largest eigenvalue (cyclic Jacobi).
glm::quat largestEigenQuat(float n[4][4])
{
    float v[4][4] = {{1.f, 0.f, 0.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 0.f}, {0.f, 0.f, 0.f, 1.f}};
    for(int sweep = 0; sweep < 24; sweep++)
    {
        float off = 0.f;
        for(int p = 0; p < 4; p++)
        {
            for(int q = p + 1; q < 4; q++)
            {
                off += n[p][q] * n[p][q];
            }
        }
        if(off < 1e-18f)
        {
            break;
        }
        for(int p = 0; p < 3; p++)
        {
            for(int q = p + 1; q < 4; q++)
            {
                if(za::abs(n[p][q]) < 1e-20f)
                {
                    continue;
                }
                const float theta = (n[q][q] - n[p][p]) / (2.f * n[p][q]);
                const float t = (theta >= 0.f ? 1.f : -1.f) / (za::abs(theta) + za::sqrt(theta * theta + 1.f));
                const float c = 1.f / za::sqrt(t * t + 1.f), s = t * c;
                for(int k = 0; k < 4; k++)
                {
                    const float a = n[k][p], b = n[k][q];
                    n[k][p] = c * a - s * b;
                    n[k][q] = s * a + c * b;
                }
                for(int k = 0; k < 4; k++)
                {
                    const float a = n[p][k], b = n[q][k];
                    n[p][k] = c * a - s * b;
                    n[q][k] = s * a + c * b;
                }
                for(int k = 0; k < 4; k++)
                {
                    const float a = v[k][p], b = v[k][q];
                    v[k][p] = c * a - s * b;
                    v[k][q] = s * a + c * b;
                }
            }
        }
    }
    int best = 0;
    for(int i = 1; i < 4; i++)
    {
        if(n[i][i] > n[best][best])
        {
            best = i;
        }
    }
    const glm::quat q{v[0][best], v[1][best], v[2][best], v[3][best]}; // (w, x, y, z)
    const float len = glm::length(q);
    return len > 1e-12f ? q / len : glm::quat{1.f, 0.f, 0.f, 0.f};
}

// Accumulates point pairs (rest a, posed b) and gives the best rigid transform b = rot * a + pos.
struct Fit
{
    glm::vec3 sa{0.f}, sb{0.f};
    float s[3][3]{}; // sum of a_i b_j
    int n{0};

    void add(const glm::vec3& a, const glm::vec3& b)
    {
        sa += a;
        sb += b;
        for(int i = 0; i < 3; i++)
        {
            for(int j = 0; j < 3; j++)
            {
                s[i][j] += a[i] * b[j];
            }
        }
        n++;
    }

    void solve(glm::quat& rot, glm::vec3& pos) const
    {
        if(n == 0)
        {
            rot = glm::quat{1.f, 0.f, 0.f, 0.f};
            pos = glm::vec3{0.f};
            return;
        }
        const float inv = 1.f / static_cast<float>(n);
        const glm::vec3 ca = sa * inv, cb = sb * inv;
        float m[3][3];
        for(int i = 0; i < 3; i++)
        {
            for(int j = 0; j < 3; j++)
            {
                m[i][j] = s[i][j] - static_cast<float>(n) * ca[i] * cb[j];
            }
        }
        const float sxx = m[0][0], sxy = m[0][1], sxz = m[0][2], syx = m[1][0], syy = m[1][1], syz = m[1][2], szx = m[2][0],
                    szy = m[2][1], szz = m[2][2];
        float nm[4][4] = {{sxx + syy + szz, syz - szy, szx - sxz, sxy - syx},
            {syz - szy, sxx - syy - szz, sxy + syx, szx + sxz},
            {szx - sxz, sxy + syx, -sxx + syy - szz, syz + szy},
            {sxy - syx, szx + sxz, syz + szy, -sxx - syy + szz}};
        rot = largestEigenQuat(nm);
        pos = cb - rot * ca;
    }
};

// ----------------------------------------------------------------------------
// The derivation.

struct Mesh
{
    const aliashdr_t* hdr{nullptr};
    int nv{0}, np{0};
    za::Vector<glm::vec3> p; // [pose * nv + v]
    za::Vector<int> rep;     // each vertex's welded representative (the first of its place in every pose)
    za::Vector<int> adjStart, adj; // the representatives' neighbours (CSR, by representative)

    [[nodiscard]] const glm::vec3& at(int pose, int v) const { return p[static_cast<za::SizeT>(pose * nv + v)]; }
};

bool loadMesh(const aliashdr_t* hdr, Mesh& m)
{
    m.hdr = hdr;
    m.nv = hdr->numverts;
    m.np = hdr->numposes;
    const auto* tv = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    m.p.resize(static_cast<za::SizeT>(m.nv * m.np));
    for(int pose = 0; pose < m.np; pose++)
    {
        for(int v = 0; v < m.nv; v++)
        {
            const trivertx_t& t = tv[pose * m.nv + v];
            m.p[static_cast<za::SizeT>(pose * m.nv + v)] = glm::vec3{t.v[0] * hdr->scale[0] + hdr->scale_origin[0],
                t.v[1] * hdr->scale[1] + hdr->scale_origin[1], t.v[2] * hdr->scale[2] + hdr->scale_origin[2]};
        }
    }

    // Welded: the same bytes in every pose (the copies along the skin's seams).
    m.rep.resize(static_cast<za::SizeT>(m.nv));
    ankerl::unordered_dense::map<za::U64, int> first;
    for(int v = 0; v < m.nv; v++)
    {
        za::U64 h = 1469598103934665603ull;
        for(int pose = 0; pose < m.np; pose++)
        {
            const trivertx_t& t = tv[pose * m.nv + v];
            h = (h ^ (static_cast<za::U64>(t.v[0]) | static_cast<za::U64>(t.v[1]) << 8 | static_cast<za::U64>(t.v[2]) << 16)) *
                1099511628211ull;
        }
        m.rep[static_cast<za::SizeT>(v)] = v;
        const auto it = first.find(h);
        if(it == first.end())
        {
            first.emplace(h, v);
            continue;
        }
        const int u = it->second;
        bool same = true;
        for(int pose = 0; pose < m.np && same; pose++)
        {
            const trivertx_t& a = tv[pose * m.nv + v];
            const trivertx_t& b = tv[pose * m.nv + u];
            same = a.v[0] == b.v[0] && a.v[1] == b.v[1] && a.v[2] == b.v[2];
        }
        if(same)
        {
            m.rep[static_cast<za::SizeT>(v)] = u;
        }
    }

    // The triangles' edges between representatives.
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
    const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
    za::Vector<za::U64> edges;
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        int c[3];
        for(int k = 0; k < 3; k++)
        {
            c[k] = desc[idx[i + k]].vertindex;
        }
        if(c[0] == c[1] || c[1] == c[2] || c[0] == c[2])
        {
            continue; // (a triangle with a repeated corner draws nothing and joins nothing: the ogre's chainsaw to his hand)
        }
        for(int k = 0; k < 3; k++)
        {
            c[k] = m.rep[static_cast<za::SizeT>(c[k])];
        }
        for(int k = 0; k < 3; k++)
        {
            const int a = c[k], b = c[(k + 1) % 3];
            if(a != b)
            {
                edges.pushBack(static_cast<za::U64>(a) << 32 | static_cast<za::U64>(b));
                edges.pushBack(static_cast<za::U64>(b) << 32 | static_cast<za::U64>(a));
            }
        }
    }
    za::Vector<int> count(static_cast<za::SizeT>(m.nv + 1), 0);
    for(const za::U64 e : edges)
    {
        count[static_cast<za::SizeT>(e >> 32) + 1]++;
    }
    m.adjStart.resize(static_cast<za::SizeT>(m.nv + 1), 0);
    for(int v = 0; v < m.nv; v++)
    {
        m.adjStart[static_cast<za::SizeT>(v + 1)] = m.adjStart[static_cast<za::SizeT>(v)] + count[static_cast<za::SizeT>(v + 1)];
    }
    m.adj.resize(edges.size());
    za::Vector<int> fill(m.adjStart.data(), m.adjStart.data() + m.adjStart.size());
    for(const za::U64 e : edges)
    {
        const int a = static_cast<int>(e >> 32);
        m.adj[static_cast<za::SizeT>(fill[static_cast<za::SizeT>(a)]++)] = static_cast<int>(e & 0xffffffffu);
    }
    return true;
}

// Each label's rigid transform per pose, fitted to its members (the representatives labelled so).
struct Transforms
{
    int labels{0}, np{0};
    za::Vector<glm::quat> rot;
    za::Vector<glm::vec3> pos;
    za::Vector<int> members;

    void fit(const Mesh& m, const za::Vector<int>& reps, const za::Vector<int>& label, int count)
    {
        labels = count;
        np = m.np;
        rot.resize(static_cast<za::SizeT>(count * np));
        pos.resize(static_cast<za::SizeT>(count * np));
        members.clear();
        members.resize(static_cast<za::SizeT>(count), 0);
        for(const int v : reps)
        {
            const int l = label[static_cast<za::SizeT>(v)];
            if(l >= 0 && l < count)
            {
                members[static_cast<za::SizeT>(l)]++;
            }
        }
        za::Vector<Fit> fits(static_cast<za::SizeT>(count));
        for(int pose = 0; pose < np; pose++)
        {
            for(Fit& f : fits)
            {
                f = Fit{};
            }
            for(const int v : reps)
            {
                const int l = label[static_cast<za::SizeT>(v)];
                if(l >= 0 && l < count)
                {
                    fits[static_cast<za::SizeT>(l)].add(m.at(0, v), m.at(pose, v));
                }
            }
            for(int l = 0; l < count; l++)
            {
                fits[static_cast<za::SizeT>(l)].solve(rot[static_cast<za::SizeT>(l * np + pose)], pos[static_cast<za::SizeT>(l * np + pose)]);
            }
        }
    }

    // How badly label l's transforms carry vertex v (the squared distances summed over the poses).
    [[nodiscard]] float error(const Mesh& m, int l, int v) const
    {
        if(members[static_cast<za::SizeT>(l)] < 3)
        {
            return 1e30f;
        }
        const glm::vec3 r = m.at(0, v);
        float e = 0.f;
        for(int pose = 0; pose < np; pose++)
        {
            const za::SizeT i = static_cast<za::SizeT>(l * np + pose);
            const glm::vec3 d = rot[i] * r + pos[i] - m.at(pose, v);
            e += glm::dot(d, d);
        }
        return e;
    }
};

// Reassigns each representative to the label (its own, or a neighbour's) whose transforms carry it best, until none
// moves. `fixed`: labels that never change (loose pieces). Returns the rms error (units).
float refine(const Mesh& m, const za::Vector<int>& reps, za::Vector<int>& label, int count, int maxIterations, Transforms& t)
{
    float total = 0.f;
    for(int it = 0; it < maxIterations; it++)
    {
        t.fit(m, reps, label, count);
        int changed = 0;
        total = 0.f;
        for(const int v : reps)
        {
            const int own = label[static_cast<za::SizeT>(v)];
            if(own < 0 || own >= count)
            {
                continue;
            }
            int best = own;
            float bestE = t.error(m, own, v);
            for(int k = m.adjStart[static_cast<za::SizeT>(v)]; k < m.adjStart[static_cast<za::SizeT>(v) + 1]; k++)
            {
                const int l = label[static_cast<za::SizeT>(m.adj[static_cast<za::SizeT>(k)])];
                if(l == best || l < 0 || l >= count)
                {
                    continue;
                }
                const float e = t.error(m, l, v);
                if(e < bestE)
                {
                    bestE = e;
                    best = l;
                }
            }
            total += bestE < 1e29f ? bestE : 0.f;
            if(best != own)
            {
                label[static_cast<za::SizeT>(v)] = best;
                changed++;
            }
        }
        if(changed == 0)
        {
            break;
        }
    }
    t.fit(m, reps, label, count);
    return za::sqrt(total / static_cast<float>(za::max<za::SizeT>(reps.size(), 1) * static_cast<za::SizeT>(m.np)));
}

// What derive has to say, said by the main thread after it (the rigs are made on the pool: warmRigs).
struct DeriveLog
{
    bool developer{true}; // Con_DPrintf, else Con_Printf
    char text[256]{};
};

void sayLog(const DeriveLog& log)
{
    if(log.text[0])
    {
        if(log.developer)
        {
            Con_DPrintf("%s", log.text);
        }
        else
        {
            Con_Printf("%s", log.text);
        }
    }
}

// Any thread: reads the model's data `hdr` (Mod_Extradata, taken on the main thread: Cache_Check relinks the cache's LRU
// list, which a worker must never touch; the cache does not move while the pool runs this) and writes `rig` and `log`
// only.
bool derive(qmodel_t* model, const aliashdr_t* hdr, const SeedTable& table, Rig& rig, DeriveLog& log)
{
    const double t0 = Sys_DoubleTime();
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || Mod_NextSurface(const_cast<aliashdr_t*>(hdr)) ||
        hdr->numverts != table.numVerts || hdr->numposes < 2 || table.count > maxBones)
    {
        q_snprintf(log.text, sizeof(log.text),
            "ragdoll: %s is not the model its seed table was made for (%d vertices, the table's %d; pose type %d; %d poses; "
            "surfaces %s)\n",
            model->name, hdr ? hdr->numverts : -1, table.numVerts, hdr ? static_cast<int>(hdr->poseverttype) : -1,
            hdr ? hdr->numposes : -1, hdr && Mod_NextSurface(const_cast<aliashdr_t*>(hdr)) ? "several" : "one");
        return false;
    }
    Mesh m;
    loadMesh(hdr, m);

    // The pieces (welded), the largest the body; a piece some pose hides is loose.
    za::Vector<int> piece(static_cast<za::SizeT>(m.nv), -1);
    za::Vector<int> pieceSize;
    za::Vector<int> stack;
    for(int v = 0; v < m.nv; v++)
    {
        if(m.rep[static_cast<za::SizeT>(v)] != v || piece[static_cast<za::SizeT>(v)] >= 0)
        {
            continue;
        }
        const int id = static_cast<int>(pieceSize.size());
        pieceSize.pushBack(0);
        piece[static_cast<za::SizeT>(v)] = id;
        stack.clear();
        stack.pushBack(v);
        while(!stack.empty())
        {
            const int u = stack.back();
            stack.popBack();
            pieceSize[static_cast<za::SizeT>(id)]++;
            for(int k = m.adjStart[static_cast<za::SizeT>(u)]; k < m.adjStart[static_cast<za::SizeT>(u) + 1]; k++)
            {
                const int w = m.adj[static_cast<za::SizeT>(k)];
                if(piece[static_cast<za::SizeT>(w)] < 0)
                {
                    piece[static_cast<za::SizeT>(w)] = id;
                    stack.pushBack(w);
                }
            }
        }
    }
    int body = 0;
    for(int i = 1; i < static_cast<int>(pieceSize.size()); i++)
    {
        body = pieceSize[static_cast<za::SizeT>(i)] > pieceSize[static_cast<za::SizeT>(body)] ? i : body;
    }
    za::Vector<int> reps;
    for(int v = 0; v < m.nv; v++)
    {
        if(m.rep[static_cast<za::SizeT>(v)] == v)
        {
            reps.pushBack(v);
        }
    }
    // The loose pieces: those a frame hides, all of each at one point (the weapon he drops as he dies, which
    // vr_monstermods.cpp collapses in his death frames: the grunt's shotgun, the knight's sword, the ogre's chainsaw in
    // four pieces). One loose bone for them all, after the seeds'. A piece of the body that only parts from it (the
    // knight's right gauntlet) is the body's: clustered with it.
    za::Vector<int> loosePieces; // the loose pieces' ids
    za::Vector<uint8_t> isLoose(pieceSize.size(), 0);
    for(int id = 0; id < static_cast<int>(pieceSize.size()) && table.count < maxBones; id++)
    {
        if(id == body || pieceSize[static_cast<za::SizeT>(id)] < 4)
        {
            continue;
        }
        bool hidden = false;
        for(int pose = 0; pose < m.np && !hidden; pose++)
        {
            glm::vec3 lo{1e30f}, hi{-1e30f};
            for(const int v : reps)
            {
                if(piece[static_cast<za::SizeT>(v)] == id)
                {
                    lo = glm::min(lo, m.at(pose, v));
                    hi = glm::max(hi, m.at(pose, v));
                }
            }
            hidden = za::max(hi.x - lo.x, za::max(hi.y - lo.y, hi.z - lo.z)) < 0.5f;
        }
        if(hidden || table.looseWeapons)
        {
            isLoose[static_cast<za::SizeT>(id)] = 1;
            loosePieces.pushBack(id);
        }
    }

    // The motion clusters: the farthest trajectories as the first centres, then each representative to its nearest
    // centre, then refined by the clusters' rigid motion.
    za::Vector<int> moving; // the representatives not in a loose piece
    for(const int v : reps)
    {
        if(!isLoose[static_cast<za::SizeT>(piece[static_cast<za::SizeT>(v)])])
        {
            moving.pushBack(v);
        }
    }
    const auto trajDist = [&](int a, int b) {
        float d = 0.f;
        for(int pose = 0; pose < m.np; pose++)
        {
            const glm::vec3 e = m.at(pose, a) - m.at(pose, b);
            d += glm::dot(e, e);
        }
        return d;
    };
    za::Vector<int> centres;
    za::Vector<float> nearestD(static_cast<za::SizeT>(m.nv), 1e30f);
    centres.pushBack(moving[0]);
    while(static_cast<int>(centres.size()) < table.clusters && static_cast<int>(centres.size()) < static_cast<int>(moving.size()))
    {
        const int c = centres.back();
        int farthest = -1;
        float farD = -1.f;
        for(const int v : moving)
        {
            float& d = nearestD[static_cast<za::SizeT>(v)];
            d = za::min(d, trajDist(v, c));
            if(d > farD)
            {
                farD = d;
                farthest = v;
            }
        }
        centres.pushBack(farthest);
    }
    const int k = static_cast<int>(centres.size());
    za::Vector<int> label(static_cast<za::SizeT>(m.nv), -1);
    for(const int v : moving)
    {
        int best = 0;
        float bestD = 1e30f;
        for(int c = 0; c < k; c++)
        {
            const float d = trajDist(v, centres[static_cast<za::SizeT>(c)]);
            if(d < bestD)
            {
                bestD = d;
                best = c;
            }
        }
        label[static_cast<za::SizeT>(v)] = best;
    }
    Transforms t;
    rig.clusterRms = refine(m, moving, label, k, 30, t);

    // The clusters to the seeds' bones: each to the bone whose seed is nearest its middle in the rest pose (wholePieces:
    // its middle in the body, and each other piece below).
    const auto nearestSeed = [&](const glm::vec3& at) {
        int best = 0;
        float bestD = 1e30f;
        for(int b = 0; b < table.count; b++)
        {
            const glm::vec3 d = at - table.seeds[b].centre;
            if(glm::dot(d, d) < bestD)
            {
                bestD = glm::dot(d, d);
                best = b;
            }
        }
        return best;
    };
    za::Vector<int> boneOfCluster(static_cast<za::SizeT>(k), 0);
    for(int c = 0; c < k; c++)
    {
        glm::vec3 sum{0.f};
        int n = 0;
        for(const int v : moving)
        {
            if(label[static_cast<za::SizeT>(v)] == c && (!table.wholePieces || piece[static_cast<za::SizeT>(v)] == body))
            {
                sum += m.at(0, v);
                n++;
            }
        }
        if(n == 0)
        {
            continue;
        }
        boneOfCluster[static_cast<za::SizeT>(c)] = nearestSeed(sum / static_cast<float>(n));
    }
    for(const int v : moving)
    {
        label[static_cast<za::SizeT>(v)] = boneOfCluster[static_cast<za::SizeT>(label[static_cast<za::SizeT>(v)])];
    }
    if(table.wholePieces)
    {
        for(int id = 0; id < static_cast<int>(pieceSize.size()); id++)
        {
            if(id == body || isLoose[static_cast<za::SizeT>(id)])
            {
                continue;
            }
            glm::vec3 sum{0.f};
            int n = 0;
            for(const int v : moving)
            {
                if(piece[static_cast<za::SizeT>(v)] == id)
                {
                    sum += m.at(0, v);
                    n++;
                }
            }
            if(n == 0)
            {
                continue;
            }
            const int b = nearestSeed(sum / static_cast<float>(n));
            for(const int v : moving)
            {
                if(piece[static_cast<za::SizeT>(v)] == id)
                {
                    label[static_cast<za::SizeT>(v)] = b;
                }
            }
        }
    }
    for(const int v : reps)
    {
        if(isLoose[static_cast<za::SizeT>(piece[static_cast<za::SizeT>(v)])])
        {
            label[static_cast<za::SizeT>(v)] = table.count;
        }
    }
    rig.boneRms = refine(m, moving, label, table.count, 40, t);
    const int numBones = table.count + (loosePieces.empty() ? 0 : 1);
    for(int b = 0; b < table.count; b++)
    {
        if(t.members[static_cast<za::SizeT>(b)] < 3)
        {
            log.developer = false;
            q_snprintf(log.text, sizeof(log.text), "ragdoll: %s: bone %s got %d vertices: no ragdoll\n", model->name,
                table.seeds[b].name, t.members[static_cast<za::SizeT>(b)]);
            return false;
        }
    }
    t.fit(m, reps, label, numBones); // (the loose pieces' too)

    // The rig.
    rig.model = model;
    rig.numBones = numBones;
    rig.numVerts = m.nv;
    rig.numPoses = m.np;
    rig.vertBone.resize(static_cast<za::SizeT>(m.nv));
    for(int v = 0; v < m.nv; v++)
    {
        rig.vertBone[static_cast<za::SizeT>(v)] = static_cast<uint8_t>(label[static_cast<za::SizeT>(m.rep[static_cast<za::SizeT>(v)])]);
    }
    rig.triBones.clear();
    {
        const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
        const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
        for(int i = 0; i + 2 < hdr->numindexes; i += 3)
        {
            uint32_t bits = 0;
            for(int k = 0; k < 3; k++)
            {
                bits |= 1u << rig.vertBone[static_cast<za::SizeT>(desc[idx[i + k]].vertindex)];
            }
            bool known = false;
            for(const uint32_t b : rig.triBones)
            {
                known = known || b == bits;
            }
            if(!known)
            {
                rig.triBones.pushBack(bits);
            }
        }
    }
    rig.poseRot.resize(static_cast<za::SizeT>(m.np * numBones));
    rig.posePos.resize(static_cast<za::SizeT>(m.np * numBones));
    rig.poseHidden.resize(static_cast<za::SizeT>(m.np * numBones), 0);
    for(int pose = 0; pose < m.np; pose++)
    {
        za::Array<glm::vec3, maxBones> lo, hi;
        for(int b = 0; b < numBones; b++)
        {
            lo[static_cast<za::SizeT>(b)] = glm::vec3{1e30f};
            hi[static_cast<za::SizeT>(b)] = glm::vec3{-1e30f};
        }
        for(const int v : reps)
        {
            const int b = label[static_cast<za::SizeT>(v)];
            lo[static_cast<za::SizeT>(b)] = glm::min(lo[static_cast<za::SizeT>(b)], m.at(pose, v));
            hi[static_cast<za::SizeT>(b)] = glm::max(hi[static_cast<za::SizeT>(b)], m.at(pose, v));
        }
        for(int b = 0; b < numBones; b++)
        {
            const glm::vec3 size = hi[static_cast<za::SizeT>(b)] - lo[static_cast<za::SizeT>(b)];
            rig.poseHidden[static_cast<za::SizeT>(pose * numBones + b)] = za::max(size.x, za::max(size.y, size.z)) < 0.5f ? 1 : 0;
        }
    }
    for(int pose = 0; pose < m.np; pose++)
    {
        for(int b = 0; b < numBones; b++)
        {
            rig.poseRot[static_cast<za::SizeT>(pose * numBones + b)] = t.rot[static_cast<za::SizeT>(b * m.np + pose)];
            rig.posePos[static_cast<za::SizeT>(pose * numBones + b)] = t.pos[static_cast<za::SizeT>(b * m.np + pose)];
        }
    }
    for(int b = 0; b < numBones; b++)
    {
        Bone& bone = rig.bones[b];
        bone.points.clear();
        for(const int v : reps)
        {
            if(label[static_cast<za::SizeT>(v)] == b)
            {
                bone.points.pushBack(m.at(0, v));
            }
        }
        if(b >= table.count)
        {
            glm::vec3 mid{0.f};
            for(const glm::vec3& p : bone.points)
            {
                mid += p;
            }
            mid /= static_cast<float>(za::max<za::SizeT>(bone.points.size(), 1));
            strcpy(bone.name, "loose");
            bone.parent = -1;
            bone.joint = Joint::Loose;
            bone.pivot = bone.end = mid;
            continue;
        }
        const Seed& s = table.seeds[b];
        strncpy(bone.name, s.name, sizeof(bone.name) - 1);
        bone.role = modelmeta::boneRole(s.name);
        if(bone.role == modelmeta::BoneRole::Head)
        {
            rig.head = b;
        }
        bone.parent = s.parent;
        bone.joint = s.joint;
        bone.pivot = s.pivot;
        bone.end = s.end;
        bone.capsule = s.capsule;
        bone.cone = s.cone * deg;
        bone.twist = s.twist * deg;
        if(s.joint == Joint::Hinge)
        {
            // Its axis from how it bends at rest (the parent's direction crossed with its own: bending further turns
            // it that way); nearly straight, the table's.
            const Seed& p = table.seeds[s.parent];
            const glm::vec3 u = glm::normalize(p.end - p.pivot), l = glm::normalize(s.end - s.pivot);
            const glm::vec3 c = glm::cross(u, l);
            const float bend = za::acos(za::clamp(glm::dot(u, l), -1.f, 1.f));
            bone.hinge = glm::length(c) > 0.2f && !s.keepHinge ? glm::normalize(c) : s.hinge;
            float rest = glm::dot(glm::cross(u, l), bone.hinge) >= 0.f ? bend : -bend;
            if(s.keepHinge)
            {
                // (Its bend about that axis: the two directions flattened across it.)
                const glm::vec3 h = glm::normalize(s.hinge), uf = u - h * glm::dot(u, h), lf = l - h * glm::dot(l, h);
                rest = za::atan2(glm::dot(glm::cross(uf, lf), h), glm::dot(uf, lf));
            }
            bone.lower = -za::max(rest - 3.f * deg, 0.f);
            bone.upper = za::max(s.flex * deg - rest, 5.f * deg);
        }
    }
    const SeedTable& tb = table;
    rig.hideLoose = tb.looseWeapons;
    rig.deaths = tb.deaths;
    for(int i = 0; i < tb.deaths; i++)
    {
        rig.deathFirst[i] = tb.deathFirst[i];
        rig.deathLast[i] = tb.deathLast[i];
    }
    rig.deriveMs = (Sys_DoubleTime() - t0) * 1000.0;
    q_snprintf(log.text, sizeof(log.text), "ragdoll: %s rigged: %d bones (%d loose), clusters %.2f, bones %.2f units rms, %.1f ms\n",
        model->name, numBones, static_cast<int>(loosePieces.size()), rig.clusterRms, rig.boneRms, rig.deriveMs);
    return true;
}

// ----------------------------------------------------------------------------
// The rigs (models' slots: a game dir change, a model reload), and the skinned models made from them.

struct RigCache
{
    za::Vector<za::UniquePtr<Rig>> rigs; // made (and kept) for these models (a rig stays where it is: ragdolls point to it)
    za::Vector<const qmodel_t*> failed;  // tried, not rigged
    auto members() { return mem::list(rigs, failed); }
};
mem::Cache<RigCache> rigCache{"ragdoll rigs", mem::GameDirChange}; // (not a model reload: ragdolls point to their rigs)
qvr::jobs::Site warmSite{"ragdoll rigs"}; // (warmRigs' parallelFor: vr_jobs_sites)

[[nodiscard]] Rig* findRig(const qmodel_t* model)
{
    for(za::UniquePtr<Rig>& r : rigCache.rigs)
    {
        if(r->model == model)
        {
            return r.get();
        }
    }
    return nullptr;
}

constexpr const char* skinnedSuffix = "#rag";

// ----------------------------------------------------------------------------
// What the server publishes (its ragdolls' bones in the world) and what the client swapped for a frame. The main thread.

constexpr float teleport = 100.f; // units a part moves in a step: a jump, not lerped (CL_RelinkEntities')
constexpr float heldLeadMost = 24.f; // units: the most a held ragdoll is drawn ahead of its step towards the hand

struct Published
{
    const Rig* rig{nullptr};
    int bodies{0}; // the bones from this on are hidden
    uint32_t cut{0}; // the bones cut off (its head: ROUND21.md, "Decapitation"), drawn at the neck
    float scale{1.f};
    bool drawn{false}; // the client has drawn it (its first frame: compared with the animated mesh, vr_debug_ragdoll)
    za::Array<glm::quat, maxBones> rot{};
    za::Array<glm::vec3, maxBones> pos{};
    // The step before (drawn between the two, as the client lerps a prop between the server's last two messages), and
    // the server times of the messages after each (prevTime == time: no step before, the latest drawn).
    za::Array<glm::quat, maxBones> prevRot{};
    za::Array<glm::vec3, maxBones> prevPos{};
    double time{0.0}, prevTime{0.0};
    // The local player's hands on its limbs at each step (bit 0 the off hand, 1 the main; their places as the client
    // had them then: vr_ragdoll_held_local).
    int heldBy{0}, prevHeldBy{0};
    glm::vec3 hand[2]{}, prevHand[2]{};
    // As drawn this frame (swapModels; drawnFrame: host_framecount).
    za::Array<glm::quat, maxBones> drawRot{};
    za::Array<glm::vec3, maxBones> drawPos{};
    int drawnFrame{-1};
    float drawBlend{1.f};      // between the steps (0 the one before .. 1 the latest)
    glm::vec3 drawLead{0.f};   // moved with the holding hands (units)
};

struct Swapped
{
    int num{0};
    qmodel_t* original{nullptr};
    qmodel_t* skinned{nullptr};
    glm::vec3 ref{0.f};
    int bones{0};
    za::Array<float, maxBones * 12> skin{};
    // The entity's animation lerp, kept while the skinned model (one pose) is drawn: R_SetupAliasFrame would otherwise
    // lerp from the corpse's pose index (past the skinned model's bones: its limbs grew from nothing).
    short previousPose{0}, currentPose{0};
    float lerpStart{0.f}, lerpTime{0.f};
    byte lerpFlags{0};
};

struct DrawState
{
    za::Vector<Published> byNum;  // by edict number (rig null: none)
    za::Vector<Swapped> swapped;  // this frame's (swapModels .. restoreModels)
    auto members() { return mem::list(byNum, swapped); }
};
mem::Cache<DrawState> draw{"ragdolls drawn", mem::MapChange};

// vr_drawn_motion_test: what it follows and for how long (the client's frames), and the places drawn each frame.
struct MotionTest
{
    int left{0}; // frames still to record
    int rag{0};  // the ragdoll's entity (0 none)
    int prop{0}; // the prop's (0 none)
    int frames{0};
};
MotionTest motionTest;

// The death view's (Immersive: hideHeadOf): the ragdoll drawn headless, the camera in its head (0 none).
int hiddenHeadNum = 0;

// vr_knockdown_debug 2 (Combat > Knockdowns, Print Rolls: "And Get-Ups' Motion"): a get-up as the client draws it,
// frame by frame from the moment the monster starts getting up (its ragdoll blended into its animation, then its
// animation): how fast its vertices move (units a second: the frames' lengths vary), the fastest frame against the mean
// (a jump), the frames that went back on the one before (a pose shown again: the jitter) and the frame its animated model
// is drawn again (the switch: its speed against the frames' before). vr_knockdown_debug 3: each frame's.
struct GetupWatch
{
    int num{0};          // the entity (0 none)
    double from{0.0}, until{0.0}; // the server times it is followed from and to
    int frames{0}, moving{0};
    float sumSpeed{0.f}, maxSpeed{0.f}, recentSpeed{0.f}, switchRatio{0.f};
    int maxAt{0}, backs{0}, switchAt{-1};
    bool skinned{false}; // drawn as a ragdoll last frame
    double startTime{0.0}, lastTime{0.0};
};
GetupWatch getupWatch;

struct GetupPoints
{
    za::Vector<glm::vec3> last, step, now;
    auto members() { return mem::list(last, step, now); }
};
mem::Cache<GetupPoints> getupPoints{"get-up motion", mem::MapChange};

struct MotionPoints
{
    za::Vector<glm::vec3> rag;  // a frame's maxBones (the parts')
    za::Vector<glm::vec3> prop; // a frame's one
    za::Vector<glm::vec3> hand; // a frame's two (the off hand, the main)
    auto members() { return mem::list(rag, prop, hand); }
};
mem::Cache<MotionPoints> motionPoints{"drawn motion test", mem::MapChange};

// The client's buffers (its drawing: the main thread).
struct DrawScratch
{
    za::Vector<glm::vec3> animated, skinned;
    za::Vector<int> poses; // (fillAO)
    auto members() { return mem::list(animated, skinned, poses); }
};
mem::Scratch<DrawScratch> drawScratch{"ragdolls drawn"};

// The skinned model's own occlusion (the shaders' PoseAO): its .mdl's baked one (vr_ao.cpp: per pose and vertex) over its
// death animations' poses, negated in each vertex's normal's 4th byte (0: none). False if not baked yet (it is then queued).
bool fillAO(const Rig& rig, iqmvert_t* verts, int numVerts)
{
    auto* src = const_cast<qmodel_t*>(rig.model);
    const auto* sh = static_cast<const aliashdr_t*>(Mod_Extradata(src));
    const unsigned char* vis = sh ? VR_AliasVertexAO(src, sh) : nullptr;
    if(!vis)
    {
        return false;
    }
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(sh) + sh->meshdesc);
    // (Over its death animations' poses: what a ragdoll's limbs are like, nearer its poses than all the model's: 0.05
    // from a death's and a lying pose's on average, all its poses' 0.10-0.12, the rest pose's 0.15.)
    za::Vector<int>& poses = drawScratch.poses;
    poses.clear();
    for(int d = 0; d < rig.deaths; d++)
    {
        for(int f = rig.deathFirst[d]; f <= rig.deathLast[d]; f++)
        {
            poses.pushBack(ragdoll::poseOfFrame(rig.model, f));
        }
    }
    if(poses.empty())
    {
        poses.pushBack(0);
    }
    for(int v = 0; v < numVerts; v++)
    {
        const int s = desc[v].vertindex;
        float sum = 0.f;
        for(const int p : poses)
        {
            sum += static_cast<float>(vis[static_cast<za::SizeT>(p) * static_cast<za::SizeT>(sh->numverts) + static_cast<za::SizeT>(s)]);
        }
        const float ao = sum / (255.f * static_cast<float>(poses.size()));
        verts[v].norm[3] = static_cast<int8_t>(-za::clamp(static_cast<int>(ao * 127.f + 0.5f), 1, 127));
    }
    return true;
}

// The skinned model `skinned` of `rig` given its occlusion once it is baked (made before it was: the map's first ragdoll
// as the map starts), looked for at most once a second.
double aoLookedAt = -1.0;
void catchUpAO(qmodel_t* skinned, const Rig& rig)
{
    auto* hdr = static_cast<aliashdr_t*>(Mod_Extradata(skinned));
    auto* verts = hdr ? reinterpret_cast<iqmvert_t*>(reinterpret_cast<byte*>(hdr) + hdr->vertexes) : nullptr;
    if(!verts || hdr->numverts_vbo <= 0 || verts[0].norm[3] < 0 || (realtime - aoLookedAt < 1.0 && realtime >= aoLookedAt))
    {
        return;
    }
    aoLookedAt = realtime;
    if(fillAO(rig, verts, hdr->numverts_vbo))
    {
        GLMesh_DeleteVertexBuffer(skinned);
        GLMesh_LoadVertexBuffer(skinned, hdr);
    }
}

// The poses (and the blend between them) the animated model of `e` would be drawn with now: R_SetupAliasFrame's, without
// changing the entity.
void animatedPoses(const entity_t* e, const aliashdr_t* hdr, int& pose1, int& pose2, float& blend)
{
    const int frame = za::clamp(e->frame, 0, za::max(hdr->numframes - 1, 0));
    const int posenum = hdr->frames[frame].firstpose;
    if(!r_lerpmodels.value || (e->lerpflags & LERP_RESETANIM))
    {
        pose1 = pose2 = posenum;
        blend = 1.f;
        return;
    }
    if(e->currentpose != posenum)
    {
        pose1 = pose2 = e->currentpose; // (a new frame's lerp starts from where it was)
        blend = 0.f;
        return;
    }
    const float span = (e->lerpflags & LERP_FINISH) ? R_FrameLerpFinish(e) - e->lerpstart : e->lerptime;
    blend = span > 0.f ? za::clamp(static_cast<float>(cl.time - e->lerpstart) / span, 0.f, 1.f) : 1.f;
    pose1 = blend >= 1.f ? e->currentpose : e->previouspose;
    pose2 = e->currentpose;
}

// The animated model's lerp of `e` kept up with its frame while its skinned model is drawn (R_SetupAliasFrame's, which
// doesn't run for it then): a knocked-down monster's get-up plays its frames under the blend from its ragdoll
// (vr_box3d.cpp updateRecoveries), and drawn again it goes on from them. (Not kept, it went on from the frame it was
// knocked down in, a standing one, to its get-up's: a jump of 20 units and back, the get-up's jitter.)
void trackPose(entity_t* e)
{
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e->model));
    if(!hdr || hdr->numframes <= 0)
    {
        return;
    }
    const maliasframedesc_t& f = hdr->frames[za::clamp(e->frame, 0, hdr->numframes - 1)];
    int posenum = f.firstpose;
    if(f.numposes > 1 && f.interval > 0.f)
    {
        posenum += static_cast<int>(cl.time / static_cast<double>(f.interval)) % f.numposes;
    }
    if(e->lerpflags & (LERP_RESETANIM | LERP_RESETANIM2))
    {
        if(e->currentpose == posenum && !(e->lerpflags & LERP_RESETANIM))
        {
            return; // (RESETANIM2: as R_SetupAliasFrame, waits for the pose to change)
        }
        e->lerpstart = 0.f;
        e->animlerpfinish = 0.f;
        e->previouspose = e->currentpose = static_cast<short>(posenum);
        e->lerpflags &= static_cast<byte>(~((e->lerpflags & LERP_RESETANIM) ? LERP_RESETANIM : LERP_RESETANIM2));
        return;
    }
    if(e->currentpose != posenum)
    {
        e->lerpstart = static_cast<float>(cl.time);
        e->animlerpfinish = (e->lerpflags & LERP_FINISH) ? e->lerpfinish : 0.f;
        e->previouspose = e->currentpose;
        e->currentpose = static_cast<short>(posenum);
    }
}

// The animated model of `e` as it would be drawn now (its lerp; its place last frame; the .mdl's vertices in the world), for
// comparing a ragdoll's first frame with it (vr_debug_ragdoll).
void animatedVertices(const entity_t* e, za::Vector<glm::vec3>& out, int& pose1, int& pose2, float& blend, bool now = false)
{
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e->model));
    animatedPoses(e, hdr, pose1, pose2, blend);
    float m16[16];
    vec3_t origin, angles;
    // Where it was drawn last frame: the message before this frame's (the server moved its origin to follow the pelvis
    // as the ragdoll was made: writeRagdoll; a listen server sends one a frame). `now`: where it is drawn this frame.
    const bool moved = !now && e->msgtime == cl.mtime[0];
    VectorCopy(moved ? e->msg_origins[1] : e->origin, origin);
    VectorCopy(moved ? e->msg_angles[1] : e->angles, angles);
    if(now && r_lerpmove.value && (e->lerpflags & LERP_MOVESTEP) && !(e->lerpflags & LERP_RESETMOVE))
    {
        // (A monster's steps lerped as R_SetupEntityTransform does, without changing the entity.)
        const bool changed = !VectorCompare(e->origin, e->currentorigin) || !VectorCompare(e->angles, e->currentangles);
        const float t = changed ? 0.f : R_MoveLerpBlend(e);
        vec3_t start, startA;
        R_MoveLerpStart(e, start, startA); // (a move starting: where the last one is drawn)
        const float* from = changed ? start : e->previousorigin;
        const float* to = e->currentorigin;
        const float* fromA = changed ? startA : e->previousangles;
        const float* toA = e->currentangles;
        for(int i = 0; i < 3; i++)
        {
            float d = toA[i] - fromA[i];
            d = d > 180.f ? d - 360.f : d < -180.f ? d + 360.f : d;
            origin[i] = from[i] + (to[i] - from[i]) * t;
            angles[i] = fromA[i] + d * t;
        }
    }
    R_EntityMatrix(m16, origin, angles, e->scale);
    ApplyTranslation(m16, hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]);
    ApplyScale(m16, hdr->scale[0], hdr->scale[1], hdr->scale[2]);
    glm::mat4 m;
    for(int c = 0; c < 4; c++)
    {
        m[c] = glm::vec4{m16[c * 4], m16[c * 4 + 1], m16[c * 4 + 2], m16[c * 4 + 3]};
    }
    const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const trivertx_t* v1 = base + static_cast<za::SizeT>(pose1) * static_cast<za::SizeT>(hdr->numverts);
    const trivertx_t* v2 = base + static_cast<za::SizeT>(pose2) * static_cast<za::SizeT>(hdr->numverts);
    out.resize(static_cast<za::SizeT>(hdr->numverts));
    for(int i = 0; i < hdr->numverts; i++)
    {
        const glm::vec3 a{v1[i].v[0], v1[i].v[1], v1[i].v[2]}, b{v2[i].v[0], v2[i].v[1], v2[i].v[2]};
        out[static_cast<za::SizeT>(i)] = glm::vec3{m * glm::vec4{a + (b - a) * blend, 1.f}};
    }
}

// Where cut-off bone `b` of `p` (its head, ROUND21.md "Decapitation") is drawn: shrunk to nothing (neckShrink) about the
// neck, the cut's pivot on the part it was cut from, turned with that part (its normals stay whole: a zero matrix gave the
// triangles from the neck to the chest no normal, and the lighting's NaN bloomed red over the screen); with `rots`/`poss`
// the parts' places (as drawn, or the latest step's). Its vertex `v` (rest space) goes to at + rot * ((v - pivot) * k).
struct Neck
{
    glm::vec3 at{0.f};
    glm::quat rot{1.f, 0.f, 0.f, 0.f};
    glm::vec3 pivot{0.f};
};
constexpr float neckShrink = 1e-3f;

template <typename Rots, typename Poss>
[[nodiscard]] Neck neckOf(const Published& p, int b, const Rots& rots, const Poss& poss)
{
    const Rig& rig = *p.rig;
    int root = b;
    while(rig.bones[root].parent >= 0 && (p.cut & (1u << rig.bones[root].parent)))
    {
        root = rig.bones[root].parent; // (the jaw: its head's neck)
    }
    const int parent = za::max(rig.bones[root].parent, 0);
    Neck n;
    n.rot = rots[static_cast<za::SizeT>(parent)];
    n.pivot = rig.bones[root].pivot;
    n.at = n.rot * (n.pivot * p.scale) + poss[static_cast<za::SizeT>(parent)];
    return n;
}

[[nodiscard]] const Swapped* swappedOf(const entity_t* e)
{
    for(const Swapped& s : draw.swapped)
    {
        if(&cl_entities[s.num] == e)
        {
            return &s;
        }
    }
    return nullptr;
}

} // namespace

namespace qvr::ragdoll
{

bool eligible(const qmodel_t* model)
{
    return tableOf(model) != nullptr;
}

const Rig* rigFor(qmodel_t* model)
{
    const SeedTable* table = tableOf(model);
    if(!table)
    {
        return nullptr;
    }
    if(Rig* r = findRig(model))
    {
        return r;
    }
    for(const qmodel_t* f : rigCache.failed)
    {
        if(f == model)
        {
            return nullptr;
        }
    }
    za::UniquePtr<Rig> r = za::makeUnique<Rig>();
    DeriveLog log;
    const bool ok = derive(model, static_cast<const aliashdr_t*>(Mod_Extradata(model)), *table, *r, log);
    sayLog(log);
    if(!ok)
    {
        rigCache.failed.pushBack(model);
        return nullptr;
    }
    Rig* made = r.get();
    rigCache.rigs.pushBack(static_cast<za::UniquePtr<Rig>&&>(r));
    return made;
}

void warmRigs(qmodel_t* const* models, int count)
{
    // Those not made yet (nor failed), their models' data loaded here (the cache: the main thread's).
    struct Job
    {
        qmodel_t* model{nullptr};
        const aliashdr_t* hdr{nullptr}; // (Mod_Extradata here, on the main thread: derive runs on the pool)
        const SeedTable* table{nullptr};
        za::UniquePtr<Rig> rig;
        DeriveLog log;
        bool ok{false};
    };
    za::Vector<Job> work;
    for(int i = 0; i < count; i++)
    {
        qmodel_t* model = models[i];
        const SeedTable* table = model ? tableOf(model) : nullptr;
        if(!table || findRig(model) || za::find(rigCache.failed.begin(), rigCache.failed.end(), model) != rigCache.failed.end())
        {
            continue;
        }
        bool twice = false;
        for(const Job& j : work)
        {
            twice = twice || j.model == model;
        }
        const auto* hdr = twice ? nullptr : static_cast<const aliashdr_t*>(Mod_Extradata(model));
        if(!hdr)
        {
            continue;
        }
        Job j;
        j.model = model;
        j.hdr = hdr;
        j.table = table;
        j.rig = za::makeUnique<Rig>();
        work.pushBack(static_cast<Job&&>(j));
    }
    // The headers again once every model is loaded (a load above may have let an earlier one go from the cache):
    // Cache_Check loads nothing, so these stay put until the pool is done.
    for(za::SizeT k = work.size(); k-- > 0;)
    {
        work[k].hdr = static_cast<const aliashdr_t*>(Cache_Check(&work[k].model->cache));
        if(!work[k].hdr)
        {
            work.erase(work.begin() + static_cast<za::PtrDiffT>(k));
        }
    }
    // Each its own on the pool (tens of milliseconds each: the map's monsters were a quarter of a second in a row).
    qvr::jobs::parallelFor(warmSite, work.size(), 1, [&work](za::SizeT begin, za::SizeT end) {
        for(za::SizeT k = begin; k < end; k++)
        {
            Job& j = work[k];
            j.ok = derive(j.model, j.hdr, *j.table, *j.rig, j.log);
        }
    });
    // Kept in the order asked (as rigFor one by one would have).
    for(Job& j : work)
    {
        sayLog(j.log);
        if(j.ok)
        {
            rigCache.rigs.pushBack(static_cast<za::UniquePtr<Rig>&&>(j.rig));
        }
        else
        {
            rigCache.failed.pushBack(j.model);
        }
    }
}

void bonePose(const Rig& rig, int pose, int b, glm::quat& rot, glm::vec3& pos)
{
    pose = za::clamp(pose, 0, rig.numPoses - 1);
    rot = rig.poseRot[static_cast<za::SizeT>(pose * rig.numBones + b)];
    pos = rig.posePos[static_cast<za::SizeT>(pose * rig.numBones + b)];
}

int poseOfFrame(const qmodel_t* model, int frame)
{
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(!hdr || hdr->numframes <= 0)
    {
        return 0;
    }
    frame = za::clamp(frame, 0, hdr->numframes - 1);
    return hdr->frames[frame].firstpose;
}

float deathProgress(const Rig& rig, int frame)
{
    for(int i = 0; i < rig.deaths; i++)
    {
        if(frame >= rig.deathFirst[i] && frame <= rig.deathLast[i])
        {
            return static_cast<float>(frame - rig.deathFirst[i]) / static_cast<float>(za::max(rig.deathLast[i] - rig.deathFirst[i], 1));
        }
    }
    return -1.f;
}

uint32_t headBones(const Rig& rig)
{
    return rig.head < 0 ? 0u : limbBones(rig, rig.head);
}

bool limbJoint(const Rig& rig, int b)
{
    if(b <= 0 || b >= rig.numBones)
    {
        return false;
    }
    const Bone& bone = rig.bones[b];
    return bone.parent >= 0 && (bone.joint == Joint::Ball || bone.joint == Joint::Hinge) && strcmp(bone.name, "chest") != 0;
}

uint32_t limbBones(const Rig& rig, int b)
{
    if(b < 0 || b >= rig.numBones)
    {
        return 0;
    }
    uint32_t cut = 1u << b;
    for(int c = b + 1; c < rig.numBones; c++) // (a parent comes before its children)
    {
        if(rig.bones[c].parent >= 0 && rig.bones[c].joint != Joint::Loose && (cut & (1u << rig.bones[c].parent)))
        {
            cut |= 1u << c;
        }
    }
    return cut;
}

glm::vec3 limbMiddle(const Rig& rig, uint32_t bones)
{
    glm::vec3 sum{0.f};
    za::SizeT n = 0;
    for(int b = 0; b < rig.numBones; b++)
    {
        if(bones & (1u << b))
        {
            for(const glm::vec3& p : rig.bones[b].points)
            {
                sum += p;
            }
            n += rig.bones[b].points.size();
        }
    }
    return n ? sum / static_cast<float>(n) : glm::vec3{0.f};
}

int uncutParent(const Rig& rig, int b, uint32_t cut)
{
    int p = rig.bones[b].parent;
    while(p >= 0 && (cut & (1u << p)))
    {
        p = rig.bones[p].parent;
    }
    return p;
}

bool collapsed(const Rig& rig, int pose, int b)
{
    pose = za::clamp(pose, 0, rig.numPoses - 1);
    return rig.poseHidden[static_cast<za::SizeT>(pose * rig.numBones + b)] != 0;
}

void publish(int num, const Rig* rig, int bodies, const glm::quat* rot, const glm::vec3* pos, float scale, double time,
    int heldBy, uint32_t cut)
{
    if(num < 0)
    {
        return;
    }
    if(num >= static_cast<int>(draw.byNum.size()))
    {
        draw.byNum.resize(static_cast<za::SizeT>(num) + 32);
    }
    Published& p = draw.byNum[static_cast<za::SizeT>(num)];
    // The step before kept to draw from: not for a new one, one whose parts changed (his shotgun dropped), one a step
    // repeated (a second publish before the message: its creation), or one not published for a while.
    bool keep = p.rig == rig && p.bodies == bodies && time > p.time && time - p.time < 0.1;
    for(int b = 0; keep && b < bodies; b++)
    {
        keep = glm::distance(pos[b], p.pos[static_cast<za::SizeT>(b)]) < teleport; // (as CL_RelinkEntities: a jump not lerped)
    }
    if(p.rig != rig)
    {
        p.drawn = false; // (a new one)
        p.drawnFrame = -1;
    }
    if(keep)
    {
        p.prevRot = p.rot;
        p.prevPos = p.pos;
        p.prevTime = p.time;
        p.prevHeldBy = p.heldBy;
        p.prevHand[0] = p.hand[0];
        p.prevHand[1] = p.hand[1];
    }
    else if(!(p.rig == rig && time == p.time))
    {
        p.prevTime = time;
        p.prevHeldBy = 0;
    }
    p.rig = rig;
    p.bodies = bodies;
    p.cut = cut;
    p.scale = scale;
    p.time = time;
    for(int b = 0; b < bodies; b++)
    {
        p.rot[static_cast<za::SizeT>(b)] = rot[b];
        p.pos[static_cast<za::SizeT>(b)] = pos[b];
    }
    if(p.prevTime == time)
    {
        p.prevRot = p.rot;
        p.prevPos = p.pos;
    }
    // (In a listen server the hands the client sent with this server frame's command: hands::current(), this host frame's.)
    const hands::State& hs = hands::current();
    p.heldBy = hs.valid ? heldBy : 0;
    for(int h = 0; h < 2; h++)
    {
        p.hand[h] = (p.heldBy & (1 << h)) ? hs.pos[h] : glm::vec3{0.f};
    }
}

void unpublish(int num)
{
    if(num >= 0 && num < static_cast<int>(draw.byNum.size()))
    {
        draw.byNum[static_cast<za::SizeT>(num)].rig = nullptr;
    }
}

void unpublishAll()
{
    for(Published& p : draw.byNum)
    {
        p.rig = nullptr;
    }
}

namespace
{

// Ragdoll `num`'s pose as drawn this frame (once a frame): between its last two steps at the client's time, as
// CL_RelinkEntities lerps a prop between the server's last two messages (vr_ragdoll_smooth); held by the local player's
// hands, moved with them since those steps (vr_ragdoll_held_local: a held prop is drawn in the hand each frame).
void drawnPose(int num)
{
    Published& p = draw.byNum[static_cast<za::SizeT>(num)];
    if(p.drawnFrame == host_framecount)
    {
        return;
    }
    p.drawnFrame = host_framecount;
    p.drawLead = glm::vec3{0.f};
    float f = 1.f;
    if(vr_ragdoll_smooth.value != 0.f && !cl_nolerp.value && p.time > p.prevTime)
    {
        f = za::clamp(static_cast<float>((cl.time - p.prevTime) / (p.time - p.prevTime)), 0.f, 1.f);
    }
    p.drawBlend = f;
    for(int b = 0; b < p.bodies; b++)
    {
        const za::SizeT i = static_cast<za::SizeT>(b);
        p.drawPos[i] = glm::mix(p.prevPos[i], p.pos[i], f);
        p.drawRot[i] = glm::slerp(p.prevRot[i], p.rot[i], f);
    }
    const hands::State& hs = hands::current();
    if(vr_ragdoll_held_local.value == 0.f || !p.heldBy || !hs.valid)
    {
        return;
    }
    // Each holding hand's place now from where it was as the drawn steps were (the same blend): the ragdoll moved by
    // their mean, so the limb in the hand moves with it at the frame rate. (The client's own places, then and now: the
    // server's hand and the drawn one differ by a constant offset that cancels.)
    glm::vec3 lead{0.f};
    int n = 0;
    for(int h = 0; h < 2; h++)
    {
        if(p.heldBy & (1 << h))
        {
            const glm::vec3 then = (p.prevHeldBy & (1 << h)) ? glm::mix(p.prevHand[h], p.hand[h], f) : p.hand[h];
            lead += hs.pos[h] - then;
            n++;
        }
    }
    lead /= static_cast<float>(n);
    if(glm::length(lead) > heldLeadMost)
    {
        return; // (a jump: a teleport, the hand taken back)
    }
    p.drawLead = lead;
    for(int b = 0; b < p.bodies; b++)
    {
        p.drawPos[static_cast<za::SizeT>(b)] += lead;
    }
}

// How evenly a point moved over the frames (`stride` apart in `pts`): its mean step a frame (units), the mean change of
// the step's length from frame to frame and of the step itself (the motion's jerk), both over the mean step, and the
// frames it (nearly) stood still in. Smooth motion: both near 0; drawn at the server's 72 Hz in 90 Hz frames: every
// fifth frame still (uneven about 0.4).
struct Evenness
{
    float step{0.f}, uneven{0.f}, jerk{0.f};
    int stalls{0};
};

[[nodiscard]] Evenness evenness(const glm::vec3* pts, int frames, int stride)
{
    Evenness e;
    if(frames < 3)
    {
        return e;
    }
    for(int i = 1; i < frames; i++)
    {
        e.step += glm::distance(pts[i * stride], pts[(i - 1) * stride]);
    }
    e.step /= static_cast<float>(frames - 1);
    if(e.step < 0.01f)
    {
        return e;
    }
    for(int i = 2; i < frames; i++)
    {
        const glm::vec3 d1 = pts[i * stride] - pts[(i - 1) * stride];
        const glm::vec3 d0 = pts[(i - 1) * stride] - pts[(i - 2) * stride];
        e.uneven += za::abs(glm::length(d1) - glm::length(d0));
        e.jerk += glm::length(d1 - d0);
    }
    for(int i = 1; i < frames; i++)
    {
        e.stalls += glm::distance(pts[i * stride], pts[(i - 1) * stride]) < 0.25f * e.step;
    }
    e.uneven /= e.step * static_cast<float>(frames - 2);
    e.jerk /= e.step * static_cast<float>(frames - 2);
    return e;
}

void printEvenness(const char* what, const Evenness& e)
{
    if(e.step < 0.01f)
    {
        Con_Printf("  %s: still\n", what);
        return;
    }
    Con_Printf("  %s: %.2f units a frame, uneven %.3f, jerk %.3f, %d stalls\n", what, e.step, e.uneven, e.jerk, e.stalls);
}

void reportMotion()
{
    const int frames = motionTest.frames;
    Con_Printf("vr_drawn_motion_test: %d frames of %.1f ms (the server's: %.1f ms), vr_ragdoll_smooth %g, vr_ragdoll_held_local %g\n",
        frames, 1000.0 * (cl.time - cl.oldtime), 1000.0 * (cl.mtime[0] - cl.mtime[1]), vr_ragdoll_smooth.value,
        vr_ragdoll_held_local.value);
    if(motionTest.rag > 0)
    {
        char what[64];
        const int held = motionTest.rag < static_cast<int>(draw.byNum.size()) ? draw.byNum[static_cast<za::SizeT>(motionTest.rag)].heldBy : 0;
        q_snprintf(what, sizeof(what), "ragdoll %d (held by hands %d) pelvis", motionTest.rag, held);
        printEvenness(what, evenness(motionPoints.rag.data(), frames, maxBones));
        // Its parts: the mean and the most uneven moving one.
        float uneven = 0.f, jerk = 0.f, worst = 0.f;
        int moving = 0, worstPart = -1;
        for(int b = 0; b < maxBones; b++)
        {
            const Evenness e = evenness(motionPoints.rag.data() + b, frames, maxBones);
            if(e.step < 0.01f)
            {
                continue;
            }
            uneven += e.uneven;
            jerk += e.jerk;
            moving++;
            if(e.uneven > worst)
            {
                worst = e.uneven;
                worstPart = b;
            }
        }
        if(moving)
        {
            Con_Printf("  ragdoll %d parts: %d moving, uneven %.3f, jerk %.3f (means), the most uneven %.3f (part %d)\n",
                motionTest.rag, moving, uneven / static_cast<float>(moving), jerk / static_cast<float>(moving), worst, worstPart);
        }
    }
    if(motionTest.prop > 0)
    {
        char what[64];
        q_snprintf(what, sizeof(what), "prop %d", motionTest.prop);
        printEvenness(what, evenness(motionPoints.prop.data(), frames, 1));
    }
    printEvenness("off hand", evenness(motionPoints.hand.data(), frames, 2));
    printEvenness("main hand", evenness(motionPoints.hand.data() + 1, frames, 2));
}

// vr_drawn_motion_test's frame: the places drawn (after the relink and the held objects in the hands).
void recordMotion()
{
    if(motionTest.left <= 0)
    {
        return;
    }
    const int rag = motionTest.rag;
    const bool ragShown = rag > 0 && rag < static_cast<int>(draw.byNum.size()) && draw.byNum[static_cast<za::SizeT>(rag)].rig;
    if(ragShown)
    {
        drawnPose(rag);
    }
    const Published* p = ragShown ? &draw.byNum[static_cast<za::SizeT>(rag)] : nullptr;
    for(int b = 0; b < maxBones; b++)
    {
        motionPoints.rag.pushBack(p && b < p->bodies ? p->drawPos[static_cast<za::SizeT>(b)] : glm::vec3{0.f});
    }
    const int prop = motionTest.prop;
    const bool propShown = prop > 0 && prop < cl.num_entities;
    motionPoints.prop.pushBack(propShown ? glm::vec3{cl_entities[prop].origin[0], cl_entities[prop].origin[1], cl_entities[prop].origin[2]}
                                         : glm::vec3{0.f});
    const hands::State& hs = hands::current();
    motionPoints.hand.pushBack(hs.pos[0]);
    motionPoints.hand.pushBack(hs.pos[1]);
    if(p && vr_debug_ragdoll.value >= 2.f)
    {
        // Each frame: where it is drawn between its steps, the steps' times, the lead the hands gave it, its pelvis.
        Con_Printf("  frame %d: cl.time %.4f, steps %.4f %.4f, blend %.2f, lead %.2f, pelvis %.2f %.2f %.2f\n", motionTest.frames,
            cl.time, p->prevTime, p->time, p->drawBlend, glm::length(p->drawLead), p->drawPos[0].x, p->drawPos[0].y, p->drawPos[0].z);
    }
    motionTest.frames++;
    if(--motionTest.left == 0)
    {
        reportMotion();
    }
}

void reportGetup()
{
    GetupWatch& g = getupWatch;
    if(g.moving > 0)
    {
        const float mean = g.sumSpeed / static_cast<float>(g.moving);
        Con_Printf("knockdown: %d get-up drawn: %d frames over %.2f s, %.0f units/s, the fastest %.0f (%.1fx, frame %d), %d frames "
                   "back on the one before; the animated model from frame %d (%.1fx the frames' before)\n",
            g.num, g.frames, g.lastTime - g.startTime, mean, g.maxSpeed, mean > 0.f ? g.maxSpeed / mean : 0.f, g.maxAt, g.backs,
            g.switchAt, g.switchRatio);
    }
    g = GetupWatch{};
}

// A frame of the get-up followed (vr_knockdown_debug 2), after the swaps: its vertices as drawn now.
void recordGetup()
{
    GetupWatch& g = getupWatch;
    if(g.num <= 0)
    {
        return;
    }
    if(g.num >= cl.num_entities || !sv.active || cl.mtime[0] > g.until || cl.mtime[0] < g.from - 1.0) // (or a new map)
    {
        reportGetup();
        return;
    }
    entity_t* e = &cl_entities[g.num];
    bool swapped = false;
    for(const Swapped& s : draw.swapped)
    {
        swapped = swapped || s.num == g.num;
    }
    GetupPoints& pts = getupPoints;
    int pose1 = 0, pose2 = 0;
    float blend = 1.f;
    if(swapped ? !skinnedVertices(g.num, pts.now, nullptr, true) : !e->model || e->model->type != mod_alias)
    {
        return;
    }
    if(!swapped)
    {
        animatedVertices(e, pts.now, pose1, pose2, blend, true);
    }
    const double dt = cl.time - g.lastTime;
    const bool same = g.frames > 0 && pts.last.size() == pts.now.size() && dt > 0.0;
    if(g.frames > 0 && !same)
    {
        return; // (no time passed: a frame drawn twice)
    }
    const bool hadStep = same && pts.step.size() == pts.now.size() && g.frames > 1;
    float step = 0.f, back = 0.f, along = 0.f;
    pts.step.resize(pts.now.size());
    for(za::SizeT v = 0; v < pts.now.size(); v++)
    {
        const glm::vec3 d = same ? pts.now[v] - pts.last[v] : glm::vec3{0.f};
        step += glm::length(d);
        if(hadStep)
        {
            back += za::max(-glm::dot(d, pts.step[v]), 0.f);
            along += glm::length(d) * glm::length(pts.step[v]);
        }
        pts.step[v] = d;
    }
    step /= static_cast<float>(za::max(static_cast<int>(pts.now.size()), 1));
    const float speed = same ? step / static_cast<float>(dt) : 0.f;
    const float backShare = along > 1e-6f ? back / along : 0.f;
    if(g.frames == 0)
    {
        g.startTime = cl.time;
    }
    if(!swapped && g.skinned && g.switchAt < 0)
    {
        g.switchAt = g.frames;
        g.switchRatio = g.recentSpeed > 0.f ? speed / g.recentSpeed : 0.f;
    }
    g.skinned = swapped;
    if(same)
    {
        g.sumSpeed += speed;
        g.moving++;
        if(speed > g.maxSpeed)
        {
            g.maxSpeed = speed;
            g.maxAt = g.frames;
        }
        g.backs += backShare > 0.5f && step > 0.05f;
        g.recentSpeed = g.recentSpeed > 0.f ? g.recentSpeed + (speed - g.recentSpeed) * 0.2f : speed;
    }
    if(vr_knockdown_debug.value >= 3.f)
    {
        glm::vec3 mid{0.f};
        for(const glm::vec3& v : pts.now)
        {
            mid += v / static_cast<float>(pts.now.size());
        }
        Con_Printf("knockdown: %d get-up frame %d: cl.time %.4f, %s, frame %d (poses %d..%d at %.2f), %.0f units/s, %.2f back, "
                   "middle %.1f %.1f %.1f\n",
            g.num, g.frames, cl.time, swapped ? "ragdoll" : "animated", e->frame, pose1, pose2, blend, speed, backShare, mid.x,
            mid.y, mid.z);
    }
    pts.last = pts.now;
    g.lastTime = cl.time;
    g.frames++;
}

} // namespace

void watchGetup(int num, double from, double until)
{
    if(getupWatch.num > 0)
    {
        reportGetup();
    }
    getupWatch = GetupWatch{};
    getupWatch.num = num;
    getupWatch.from = from;
    getupWatch.until = until;
    getupPoints.last.clear();
    getupPoints.step.clear();
}

void swapModels()
{
    draw.swapped.clear();
    if(!sv.active || cls.state != ca_connected)
    {
        return;
    }
    recordMotion();
    for(int num = 1; num < static_cast<int>(draw.byNum.size()) && num < cl.num_entities; num++)
    {
        const Published& p = draw.byNum[static_cast<za::SizeT>(num)];
        entity_t* e = &cl_entities[num];
        if(!p.rig || e->model != p.rig->model)
        {
            continue; // (the client shows something else: a head, a gib, nothing yet)
        }
        char name[MAX_QPATH];
        q_snprintf(name, sizeof(name), "%s%s", p.rig->model->name, skinnedSuffix);
        qmodel_t* skinned = Mod_ForName(name, false);
        if(!skinned || skinned->type != mod_alias)
        {
            continue;
        }
        catchUpAO(skinned, *p.rig);
        if(!p.drawn)
        {
            draw.byNum[static_cast<za::SizeT>(num)].drawn = true;
            if(vr_debug_ragdoll.value)
            {
                // Its first frame drawn against what the animated model would have drawn now: the switch unseen.
                int pose1 = 0, pose2 = 0;
                float blend = 0.f;
                animatedVertices(e, drawScratch.animated, pose1, pose2, blend);
                drawnPose(num);
                if(skinnedVertices(num, drawScratch.skinned, nullptr, true) && drawScratch.skinned.size() == drawScratch.animated.size())
                {
                    // (Its drawn vertices: not a hidden bone's, collapsed in both: the shotgun he dropped.)
                    float sum = 0.f, most = 0.f;
                    int n = 0;
                    za::Array<float, maxBones> boneMost{}; // (each bone's worst: which part the switch shows)
                    for(za::SizeT i = 0; i < drawScratch.skinned.size(); i++)
                    {
                        if(p.rig->vertBone[i] >= p.bodies || (p.cut & (1u << p.rig->vertBone[i])))
                        {
                            continue;
                        }
                        const float d = glm::distance(drawScratch.skinned[i], drawScratch.animated[i]);
                        sum += d * d;
                        most = za::max(most, d);
                        boneMost[p.rig->vertBone[i]] = za::max(boneMost[p.rig->vertBone[i]], d);
                        n++;
                    }
                    int worst = 0;
                    for(int b = 0; b < p.rig->numBones && b < maxBones; b++)
                    {
                        worst = boneMost[static_cast<za::SizeT>(b)] > boneMost[static_cast<za::SizeT>(worst)] ? b : worst;
                    }
                    Con_Printf("ragdoll: %d first drawn: the animated mesh (poses %d..%d at %.2f, frame %d) to the ragdoll's: "
                               "%.2f units rms, %.2f at most (%d vertices; the most on %s)\n",
                        num, pose1, pose2, blend, e->frame, za::sqrt(sum / static_cast<float>(za::max(n, 1))), most, n,
                        p.rig->bones[worst].name);
                }
            }
        }
        drawnPose(num);
        trackPose(e);
        Swapped s;
        s.num = num;
        s.original = e->model;
        s.previousPose = e->previouspose;
        s.currentPose = e->currentpose;
        s.lerpStart = e->lerpstart;
        s.lerpTime = e->lerptime;
        s.lerpFlags = e->lerpflags;
        s.skinned = skinned;
        s.ref = p.drawPos[0];
        s.bones = p.rig->numBones;
        const uint32_t viewHidden = num == hiddenHeadNum ? headBones(*p.rig) : 0u; // (the death view's camera in it)
        for(int b = 0; b < s.bones; b++)
        {
            // (A hidden bone: all its vertices at the pelvis, its triangles gone; a cut-off one's at the neck.)
            const bool cut = b < p.bodies && (p.cut & (1u << b)) && !(viewHidden & (1u << b));
            const bool shown = b < p.bodies && !cut && !(viewHidden & (1u << b));
            glm::mat3 r = shown ? glm::mat3_cast(p.drawRot[static_cast<za::SizeT>(b)]) * p.scale : glm::mat3{0.f};
            glm::vec3 t = shown ? p.drawPos[static_cast<za::SizeT>(b)] - s.ref : glm::vec3{0.f};
            if(cut)
            {
                const Neck n = neckOf(p, b, p.drawRot, p.drawPos);
                r = glm::mat3_cast(n.rot) * (p.scale * neckShrink);
                t = n.at - r * n.pivot - s.ref;
            }
            float* out = &s.skin[static_cast<za::SizeT>(b * 12)];
            for(int row = 0; row < 3; row++)
            {
                out[row * 4 + 0] = r[0][row];
                out[row * 4 + 1] = r[1][row];
                out[row * 4 + 2] = r[2][row];
                out[row * 4 + 3] = t[row];
            }
        }
        // Its limbs' ends: blood trails when flung (vr_ragdoll_blood).
        for(int b = 0; b < p.bodies; b++)
        {
            const Bone& bone = p.rig->bones[b];
            if(bone.joint != Joint::Loose && bone.parent >= 0 && !(p.cut & (1u << b)))
            {
                decals::limbTrail(num * maxBones + b, p.drawRot[static_cast<za::SizeT>(b)] * (bone.end * p.scale) + p.drawPos[static_cast<za::SizeT>(b)]);
            }
        }
        e->model = skinned;
        e->previouspose = e->currentpose = 0; // (its one pose: no lerp from the corpse's)
        e->lerpflags &= static_cast<byte>(~(LERP_RESETANIM | LERP_RESETANIM2));
        draw.swapped.pushBack(s);
    }
    recordGetup();
}

void restoreModels()
{
    for(const Swapped& s : draw.swapped)
    {
        entity_t* e = &cl_entities[s.num];
        if(e->model == s.skinned)
        {
            e->model = s.original;
            e->previouspose = s.previousPose;
            e->currentpose = s.currentPose;
            e->lerpstart = s.lerpStart;
            e->lerptime = s.lerpTime;
            e->lerpflags = s.lerpFlags;
        }
    }
    draw.swapped.clear();
}

const qmodel_t* sourceModel(const entity_t* e)
{
    const Swapped* s = swappedOf(e);
    return s && e->model == s->skinned ? s->original : nullptr;
}

int bonePoses(const entity_t* e, const float** matrices)
{
    const Swapped* s = swappedOf(e);
    if(!s || e->model != s->skinned)
    {
        return 0;
    }
    if(matrices)
    {
        *matrices = s->skin.data();
    }
    return s->bones;
}

bool drawMatrix(const entity_t* e, float matrix[16])
{
    const Swapped* s = swappedOf(e);
    if(!s || e->model != s->skinned)
    {
        return false;
    }
    for(int i = 0; i < 16; i++)
    {
        matrix[i] = i % 5 == 0 ? 1.f : 0.f;
    }
    matrix[12] = s->ref.x;
    matrix[13] = s->ref.y;
    matrix[14] = s->ref.z;
    return true;
}

bool skinnedVertices(int num, za::Vector<glm::vec3>& out, za::Vector<glm::vec3>* normals, bool drawn)
{
    if(num < 0 || num >= static_cast<int>(draw.byNum.size()) || !draw.byNum[static_cast<za::SizeT>(num)].rig)
    {
        return false;
    }
    const Published& p = draw.byNum[static_cast<za::SizeT>(num)];
    const Rig& rig = *p.rig;
    const bool asDrawn = drawn && p.drawnFrame >= 0 && host_framecount - p.drawnFrame <= 1; // (this frame's or the last's)
    const auto& rots = asDrawn ? p.drawRot : p.rot;
    const auto& poss = asDrawn ? p.drawPos : p.pos;
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(rig.model)));
    const auto* tv = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    out.resize(static_cast<za::SizeT>(rig.numVerts));
    if(normals)
    {
        normals->resize(static_cast<za::SizeT>(rig.numVerts));
    }
    for(int v = 0; v < rig.numVerts; v++)
    {
        const trivertx_t& t = tv[v]; // the rest pose (0)
        const glm::vec3 r{t.v[0] * hdr->scale[0] + hdr->scale_origin[0], t.v[1] * hdr->scale[1] + hdr->scale_origin[1],
            t.v[2] * hdr->scale[2] + hdr->scale_origin[2]};
        const int b = rig.vertBone[static_cast<za::SizeT>(v)];
        const bool cut = b < p.bodies && (p.cut & (1u << b)); // (its head cut off: at the neck)
        Neck neck;
        if(cut)
        {
            neck = neckOf(p, b, rots, poss);
        }
        out[static_cast<za::SizeT>(v)] = cut ? neck.at + neck.rot * ((r - neck.pivot) * (p.scale * neckShrink))
                                         : b < p.bodies ? rots[static_cast<za::SizeT>(b)] * (r * p.scale) + poss[static_cast<za::SizeT>(b)]
                                                        : poss[0];
        if(normals)
        {
            const glm::vec3 n{r_avertexnormals[t.lightnormalindex][0], r_avertexnormals[t.lightnormalindex][1],
                r_avertexnormals[t.lightnormalindex][2]};
            (*normals)[static_cast<za::SizeT>(v)] = cut ? neck.rot * n : b < p.bodies ? rots[static_cast<za::SizeT>(b)] * n : glm::vec3{0.f};
        }
    }
    return true;
}

bool drawnPart(int num, int part, glm::quat& rot, glm::vec3& pos, float& scale, const Rig** rig)
{
    if(num < 0 || num >= static_cast<int>(draw.byNum.size()) || !draw.byNum[static_cast<za::SizeT>(num)].rig || part < 0 ||
       part >= draw.byNum[static_cast<za::SizeT>(num)].bodies)
    {
        return false;
    }
    // As drawn this frame (drawnPose: between the steps, moved with the holding hands; made now if not drawn yet).
    drawnPose(num);
    const Published& p = draw.byNum[static_cast<za::SizeT>(num)];
    if(rig)
    {
        *rig = p.rig;
    }
    rot = p.drawRot[static_cast<za::SizeT>(part)];
    pos = p.drawPos[static_cast<za::SizeT>(part)];
    scale = p.scale;
    return true;
}

void boneTriangles(const Rig& rig, int bone, za::Vector<glm::vec3>& out)
{
    out.clear();
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(rig.model)));
    if(!hdr || !hdr->vertexes || !hdr->indexes || !hdr->meshdesc)
    {
        return;
    }
    const auto* tv = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes); // the rest pose (0)
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
    const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        int c[3];
        bool on = false;
        for(int k = 0; k < 3; k++)
        {
            c[k] = desc[idx[i + k]].vertindex;
            on = on || (c[k] < rig.numVerts && rig.vertBone[static_cast<za::SizeT>(c[k])] == bone);
        }
        if(!on || c[0] == c[1] || c[1] == c[2] || c[0] == c[2])
        {
            continue;
        }
        for(const int v : c)
        {
            const trivertx_t& t = tv[v];
            out.pushBack(glm::vec3{t.v[0] * hdr->scale[0] + hdr->scale_origin[0], t.v[1] * hdr->scale[1] + hdr->scale_origin[1],
                t.v[2] * hdr->scale[2] + hdr->scale_origin[2]});
        }
    }
}

void motionTest_f()
{
    if(Cmd_Argc() < 2 || !sv.active)
    {
        Con_Printf("usage: vr_drawn_motion_test <frames> [<entity> | nearest | held] (a listen server)\n");
        return;
    }
    motionTest = MotionTest{};
    motionPoints.rag.clear();
    motionPoints.prop.clear();
    motionPoints.hand.clear();
    // The ragdoll nearest you.
    const entity_t& me = cl_entities[cl.viewentity];
    const glm::vec3 from{me.origin[0], me.origin[1], me.origin[2]};
    float best = 1e30f;
    for(int num = 1; num < static_cast<int>(draw.byNum.size()); num++)
    {
        const Published& p = draw.byNum[static_cast<za::SizeT>(num)];
        if(p.rig && glm::distance(p.pos[0], from) < best)
        {
            best = glm::distance(p.pos[0], from);
            motionTest.rag = num;
        }
    }
    if(Cmd_Argc() > 2)
    {
        if(!q_strcasecmp(Cmd_Argv(2), "nearest"))
        {
            motionTest.prop = box3d::nearestProp();
        }
        else if(!q_strcasecmp(Cmd_Argv(2), "held"))
        {
            const int main = cl.stats[protocol::STAT_QVR_CARRYMAIN];
            motionTest.prop = main ? main : cl.stats[protocol::STAT_QVR_CARRYOFF];
        }
        else
        {
            motionTest.prop = Q_atoi(Cmd_Argv(2));
        }
    }
    motionTest.left = za::clamp(Q_atoi(Cmd_Argv(1)), 3, 2000);
    Con_Printf("vr_drawn_motion_test: ragdoll %d, prop %d, %d frames\n", motionTest.rag, motionTest.prop, motionTest.left);
}

void hideHeadOf(int num)
{
    hiddenHeadNum = num;
}

void info_f()
{
    qmodel_t* model = Mod_ForName("progs/soldier.mdl", false);
    if(Cmd_Argc() > 1)
    {
        model = Mod_ForName(Cmd_Argv(1), false);
    }
    if(!model)
    {
        Con_Printf("vr_ragdoll_info: no such model\n");
        return;
    }
    const Rig* rig = rigFor(model);
    if(!rig)
    {
        Con_Printf("vr_ragdoll_info: %s has no ragdoll (vr_ragdoll 1: the grunt's)\n", model->name);
        return;
    }
    Con_Printf("vr_ragdoll_info: %s: %d bones, %d vertices, %d poses; motion clusters %.2f units rms, bones %.2f; derived in %.1f ms\n",
        model->name, rig->numBones, rig->numVerts, rig->numPoses, rig->clusterRms, rig->boneRms, rig->deriveMs);
    for(int b = 0; b < rig->numBones; b++)
    {
        const Bone& bone = rig->bones[b];
        int n = 0;
        for(const uint8_t vb : rig->vertBone)
        {
            n += vb == b ? 1 : 0;
        }
        static constexpr const char* kinds[] = {"root", "ball", "hinge", "loose"};
        Con_Printf("  %2d %-11s parent %2d %-5s %3d verts (%2d places) pivot %5.1f %5.1f %5.1f", b, bone.name, bone.parent,
            kinds[static_cast<int>(bone.joint)], n, static_cast<int>(bone.points.size()), bone.pivot.x, bone.pivot.y, bone.pivot.z);
        int hidden = 0, first = -1;
        for(int pose = 0; pose < rig->numPoses; pose++)
        {
            if(collapsed(*rig, pose, b))
            {
                hidden++;
                first = first < 0 ? pose : first;
            }
        }
        if(hidden > 0)
        {
            Con_Printf(" hidden in %d poses (from %d)", hidden, first);
        }
        if(bone.joint == Joint::Ball)
        {
            Con_Printf(" cone %.0f twist %.0f", bone.cone / deg, bone.twist / deg);
        }
        else if(bone.joint == Joint::Hinge)
        {
            Con_Printf(" axis %.2f %.2f %.2f range %.0f..%.0f", bone.hinge.x, bone.hinge.y, bone.hinge.z, bone.lower / deg, bone.upper / deg);
        }
        Con_Printf("\n");
    }
}

} // namespace qvr::ragdoll

// ----------------------------------------------------------------------------
// The skinned model ("<model>#rag"), made as Mod_LoadModel loads it: the .mdl's rest pose as a skeletal (IQM-style)
// mesh, each vertex on its bone, the bones' bind pose the rest pose itself; every frame shows its one pose. The skins
// are the .mdl's (its textures, not copies).
extern "C" int VR_SyntheticModel(qmodel_t* mod)
{
    const size_t len = strlen(mod->name), suffix = strlen(skinnedSuffix);
    if(len <= suffix || !modelmeta::has(mod, modelmeta::Trait::Ragdoll))
    {
        return false;
    }
    char base[MAX_QPATH];
    q_strlcpy(base, mod->name, za::min(sizeof(base), len - suffix + 1));
    qmodel_t* src = Mod_ForName(base, false);
    const Rig* rig = src ? ragdoll::rigFor(src) : nullptr;
    if(!rig)
    {
        return false;
    }
    const auto* sh = static_cast<const aliashdr_t*>(Mod_Extradata(src));
    const int numVerts = sh->numverts_vbo, numIndexes = sh->numindexes, numBones = rig->numBones, numFrames = za::max(sh->numframes, 1);

    const int start = Hunk_LowMark();
    const size_t hdrSize = sizeof(aliashdr_t) + sizeof(maliasframedesc_t) * static_cast<size_t>(numFrames - 1);
    auto* hdr = static_cast<aliashdr_t*>(Hunk_Alloc(static_cast<int>(hdrSize)));
    auto* bones = static_cast<boneinfo_t*>(Hunk_Alloc(static_cast<int>(sizeof(boneinfo_t)) * numBones));
    auto* bind = static_cast<bonepose_t*>(Hunk_Alloc(static_cast<int>(sizeof(bonepose_t)) * numBones));
    auto* poses = static_cast<bonepose_t*>(Hunk_Alloc(static_cast<int>(sizeof(bonepose_t)) * numBones));
    auto* verts = static_cast<iqmvert_t*>(Hunk_Alloc(static_cast<int>(sizeof(iqmvert_t)) * numVerts));
    auto* indexes = static_cast<unsigned short*>(Hunk_Alloc(static_cast<int>(sizeof(unsigned short)) * numIndexes));

    hdr->ident = sh->ident;
    hdr->version = sh->version;
    hdr->scale[0] = hdr->scale[1] = hdr->scale[2] = 1.f;
    hdr->boundingradius = sh->boundingradius;
    hdr->numskins = sh->numskins;
    hdr->skinwidth = sh->skinwidth;
    hdr->skinheight = sh->skinheight;
    hdr->numverts = numVerts;
    hdr->numverts_vbo = numVerts;
    hdr->numtris = sh->numtris;
    hdr->numframes = numFrames;
    hdr->synctype = sh->synctype;
    hdr->flags = sh->flags;
    hdr->size = sh->size;
    hdr->numindexes = numIndexes;
    hdr->numposes = 1;
    hdr->numbones = numBones;
    hdr->poseverttype = aliashdr_t::PV_IQM;
    hdr->nextsurface = 0;
    for(int f = 0; f < numFrames; f++)
    {
        hdr->frames[f] = sh->frames[za::min(f, sh->numframes - 1)];
        hdr->frames[f].firstpose = 0;
        hdr->frames[f].numposes = 1;
    }
    memcpy(hdr->gltextures, sh->gltextures, sizeof(hdr->gltextures));
    memcpy(hdr->fbtextures, sh->fbtextures, sizeof(hdr->fbtextures));
    memcpy(hdr->texels, sh->texels, sizeof(hdr->texels));
    for(int b = 0; b < numBones; b++)
    {
        q_strlcpy(bones[b].name, rig->bones[b].name, sizeof(bones[b].name));
        bones[b].parent = -1;
        for(int i = 0; i < 12; i++)
        {
            const float id = i % 5 == 0 ? 1.f : 0.f;
            bones[b].inverse.mat[i] = id;
            bind[b].mat[i] = id;
            poses[b].mat[i] = id;
        }
    }
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(sh) + sh->meshdesc);
    const auto* tv = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(sh) + sh->vertexes); // pose 0
    for(int v = 0; v < numVerts; v++)
    {
        const trivertx_t& t = tv[desc[v].vertindex];
        iqmvert_t& o = verts[v];
        for(int i = 0; i < 3; i++)
        {
            o.xyz[i] = t.v[i] * sh->scale[i] + sh->scale_origin[i];
            o.norm[i] = static_cast<int8_t>(127.f * r_avertexnormals[t.lightnormalindex][i]);
        }
        o.norm[3] = 0;
        o.st[0] = (static_cast<float>(desc[v].st[0]) + 0.5f) / static_cast<float>(TexMgr_PadConditional(sh->skinwidth));
        o.st[1] = (static_cast<float>(desc[v].st[1]) + 0.5f) / static_cast<float>(TexMgr_PadConditional(sh->skinheight));
        o.weight[0] = 255;
        o.weight[1] = o.weight[2] = o.weight[3] = 0;
        o.idx[0] = rig->vertBone[desc[v].vertindex];
        o.idx[1] = o.idx[2] = o.idx[3] = 0;
    }
    (void)fillAO(*rig, verts, numVerts); // (else later: catchUpAO)
    memcpy(indexes, reinterpret_cast<const byte*>(sh) + sh->indexes, sizeof(unsigned short) * static_cast<size_t>(numIndexes));
    hdr->boneinfo = reinterpret_cast<byte*>(bones) - reinterpret_cast<byte*>(hdr);
    hdr->bindpose = reinterpret_cast<byte*>(bind) - reinterpret_cast<byte*>(hdr);
    hdr->boneposedata = reinterpret_cast<byte*>(poses) - reinterpret_cast<byte*>(hdr);
    hdr->vertexes = reinterpret_cast<byte*>(verts) - reinterpret_cast<byte*>(hdr);
    hdr->indexes = reinterpret_cast<byte*>(indexes) - reinterpret_cast<byte*>(hdr);

    if(mod->meshvbo)
    {
        GLMesh_DeleteVertexBuffer(mod); // (made again: its cache was given back)
    }
    GLMesh_LoadVertexBuffer(mod, hdr);
    mod->type = mod_alias;
    mod->needload = false;
    mod->numframes = numFrames;
    mod->synctype = src->synctype;
    mod->flags = src->flags;
    mod->path_id = src->path_id;
    // Its bounds: everything it can reach lying about (the renderer doesn't cull a posed entity by them).
    for(int i = 0; i < 3; i++)
    {
        mod->mins[i] = mod->ymins[i] = mod->rmins[i] = -64.f;
        mod->maxs[i] = mod->ymaxs[i] = mod->rmaxs[i] = 64.f;
    }
    const int total = Hunk_LowMark() - start;
    Cache_Alloc(&mod->cache, total, mod->name);
    if(!mod->cache.data)
    {
        Hunk_FreeToLowMark(start);
        return false;
    }
    memcpy(mod->cache.data, hdr, static_cast<size_t>(total));
    Hunk_FreeToLowMark(start);
    Con_DPrintf("ragdoll: %s made (%d vertices, %d bones)\n", mod->name, numVerts, numBones);
    return true;
}

extern "C" void VR_RagdollSwap(void)
{
    ragdoll::swapModels();
}

extern "C" void VR_RagdollRestore(void)
{
    ragdoll::restoreModels();
}
