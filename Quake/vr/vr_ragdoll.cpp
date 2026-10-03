// vr_ragdoll.cpp -- ragdolls (experimental: the monsters in seedTables): the rig derived from a .mdl's vertex animation, the skinned
// model made from it in memory, and the client's drawing of the server's ragdolls. See vr_ragdoll.hpp; the bodies and
// joints are vr_box3d.cpp's ("Ragdolls"); ROUND21.md, "Ragdolls".

#include "vr_ragdoll.hpp"
#include "vr_api_render.h"
#include "vr_cvars.hpp"
#include "vr_decals.hpp"
#include "vr_mem.hpp"

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
    za::SizeT n = mem::heldBytes(r.vertBone) + mem::heldBytes(r.poseRot) + mem::heldBytes(r.posePos) + mem::heldBytes(r.poseHidden);
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
    const char* model;
    int numVerts; // the model the table was made for (Quake VR's grunt): another one is not rigged
    const Seed* seeds;
    int count;
    int deaths;
    int deathFirst[2], deathLast[2];
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

constexpr SeedTable seedTables[] = {
    {"progs/soldier.mdl", 555, gruntSeeds, static_cast<int>(sizeof(gruntSeeds) / sizeof(gruntSeeds[0])), 2, {8, 18}, {17, 28}},
    {"progs/knight.mdl", 655, knightSeeds, static_cast<int>(sizeof(knightSeeds) / sizeof(knightSeeds[0])), 2, {76, 86}, {85, 96}},
    {"progs/ogre.mdl", 497, ogreSeeds, static_cast<int>(sizeof(ogreSeeds) / sizeof(ogreSeeds[0])), 2, {112, 126}, {125, 135}},
    {"progs/enforcer.mdl", 479, enforcerSeeds, static_cast<int>(sizeof(enforcerSeeds) / sizeof(enforcerSeeds[0])), 2, {41, 55}, {54, 65}},
    {"progs/hknight.mdl", 538, hknightSeeds, static_cast<int>(sizeof(hknightSeeds) / sizeof(hknightSeeds[0])), 2, {42, 54}, {53, 62}},
    {"progs/dog.mdl", 655, dogSeeds, static_cast<int>(sizeof(dogSeeds) / sizeof(dogSeeds[0])), 2, {8, 17}, {16, 25}},
    {"progs/wizard.mdl", 310, wizardSeeds, static_cast<int>(sizeof(wizardSeeds) / sizeof(wizardSeeds[0])), 1, {46, 0}, {53, 0}},
};

[[nodiscard]] const SeedTable* tableOf(const qmodel_t* model)
{
    if(!model || model->type != mod_alias)
    {
        return nullptr;
    }
    for(const SeedTable& t : seedTables)
    {
        if(!strcmp(model->name, t.model))
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

constexpr int clusterCount = 18; // the motion clusters (more than the bones: the seeds gather them)

bool derive(qmodel_t* model, const SeedTable& table, Rig& rig)
{
    const double t0 = Sys_DoubleTime();
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || Mod_NextSurface(const_cast<aliashdr_t*>(hdr)) ||
        hdr->numverts != table.numVerts || hdr->numposes < 2 || table.count > maxBones)
    {
        Con_DPrintf("ragdoll: %s is not the model its seed table was made for\n", model->name);
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
        if(hidden)
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
    while(static_cast<int>(centres.size()) < clusterCount && static_cast<int>(centres.size()) < static_cast<int>(moving.size()))
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

    // The clusters to the seeds' bones: each to the bone whose seed is nearest its middle in the rest pose.
    za::Vector<int> boneOfCluster(static_cast<za::SizeT>(k), 0);
    for(int c = 0; c < k; c++)
    {
        glm::vec3 sum{0.f};
        int n = 0;
        for(const int v : moving)
        {
            if(label[static_cast<za::SizeT>(v)] == c)
            {
                sum += m.at(0, v);
                n++;
            }
        }
        if(n == 0)
        {
            continue;
        }
        const glm::vec3 mid = sum / static_cast<float>(n);
        float bestD = 1e30f;
        for(int b = 0; b < table.count; b++)
        {
            const glm::vec3 d = mid - table.seeds[b].centre;
            if(glm::dot(d, d) < bestD)
            {
                bestD = glm::dot(d, d);
                boneOfCluster[static_cast<za::SizeT>(c)] = b;
            }
        }
    }
    for(const int v : moving)
    {
        label[static_cast<za::SizeT>(v)] = boneOfCluster[static_cast<za::SizeT>(label[static_cast<za::SizeT>(v)])];
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
            Con_Printf("ragdoll: %s: bone %s got %d vertices: no ragdoll\n", model->name, table.seeds[b].name,
                t.members[static_cast<za::SizeT>(b)]);
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
    rig.deaths = tb.deaths;
    for(int i = 0; i < tb.deaths; i++)
    {
        rig.deathFirst[i] = tb.deathFirst[i];
        rig.deathLast[i] = tb.deathLast[i];
    }
    rig.deriveMs = (Sys_DoubleTime() - t0) * 1000.0;
    Con_DPrintf("ragdoll: %s rigged: %d bones (%d loose), clusters %.2f, bones %.2f units rms, %.1f ms\n", model->name, numBones,
        static_cast<int>(loosePieces.size()), rig.clusterRms, rig.boneRms, rig.deriveMs);
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

struct Published
{
    const Rig* rig{nullptr};
    int bodies{0}; // the bones from this on are hidden
    float scale{1.f};
    bool drawn{false}; // the client has drawn it (its first frame: compared with the animated mesh, vr_debug_ragdoll)
    za::Array<glm::quat, maxBones> rot{};
    za::Array<glm::vec3, maxBones> pos{};
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
    const float span = (e->lerpflags & LERP_FINISH) ? e->lerpfinish - e->lerpstart : e->lerptime;
    blend = span > 0.f ? za::clamp(static_cast<float>(cl.time - e->lerpstart) / span, 0.f, 1.f) : 1.f;
    pose1 = blend >= 1.f ? e->currentpose : e->previouspose;
    pose2 = e->currentpose;
}

// The animated model of `e` as it would be drawn now (its lerp; its place last frame; the .mdl's vertices in the world), for
// comparing a ragdoll's first frame with it (vr_debug_ragdoll).
void animatedVertices(const entity_t* e, za::Vector<glm::vec3>& out, int& pose1, int& pose2, float& blend)
{
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e->model));
    animatedPoses(e, hdr, pose1, pose2, blend);
    float m16[16];
    vec3_t origin, angles;
    // Where it was drawn last frame: the message before this frame's (the server moved its origin to follow the pelvis
    // as the ragdoll was made: writeRagdoll; a listen server sends one a frame).
    const bool moved = e->msgtime == cl.mtime[0];
    VectorCopy(moved ? e->msg_origins[1] : e->origin, origin);
    VectorCopy(moved ? e->msg_angles[1] : e->angles, angles);
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
    if(!derive(model, *table, *r))
    {
        rigCache.failed.pushBack(model);
        return nullptr;
    }
    Rig* made = r.get();
    rigCache.rigs.pushBack(static_cast<za::UniquePtr<Rig>&&>(r));
    return made;
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

bool collapsed(const Rig& rig, int pose, int b)
{
    pose = za::clamp(pose, 0, rig.numPoses - 1);
    return rig.poseHidden[static_cast<za::SizeT>(pose * rig.numBones + b)] != 0;
}

void publish(int num, const Rig* rig, int bodies, const glm::quat* rot, const glm::vec3* pos, float scale)
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
    if(p.rig != rig)
    {
        p.drawn = false; // (a new one)
    }
    p.rig = rig;
    p.bodies = bodies;
    p.scale = scale;
    for(int b = 0; b < bodies; b++)
    {
        p.rot[static_cast<za::SizeT>(b)] = rot[b];
        p.pos[static_cast<za::SizeT>(b)] = pos[b];
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

void swapModels()
{
    draw.swapped.clear();
    if(!sv.active || cls.state != ca_connected)
    {
        return;
    }
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
                if(skinnedVertices(num, drawScratch.skinned) && drawScratch.skinned.size() == drawScratch.animated.size())
                {
                    // (Its drawn vertices: not a hidden bone's, collapsed in both: the shotgun he dropped.)
                    float sum = 0.f, most = 0.f;
                    int n = 0;
                    for(za::SizeT i = 0; i < drawScratch.skinned.size(); i++)
                    {
                        if(p.rig->vertBone[i] >= p.bodies)
                        {
                            continue;
                        }
                        const float d = glm::distance(drawScratch.skinned[i], drawScratch.animated[i]);
                        sum += d * d;
                        most = za::max(most, d);
                        n++;
                    }
                    Con_Printf("ragdoll: %d first drawn: the animated mesh (poses %d..%d at %.2f, frame %d) to the ragdoll's: "
                               "%.2f units rms, %.2f at most (%d vertices)\n",
                        num, pose1, pose2, blend, e->frame, za::sqrt(sum / static_cast<float>(za::max(n, 1))), most, n);
                }
            }
        }
        Swapped s;
        s.num = num;
        s.original = e->model;
        s.previousPose = e->previouspose;
        s.currentPose = e->currentpose;
        s.lerpStart = e->lerpstart;
        s.lerpTime = e->lerptime;
        s.lerpFlags = e->lerpflags;
        s.skinned = skinned;
        s.ref = p.pos[0];
        s.bones = p.rig->numBones;
        for(int b = 0; b < s.bones; b++)
        {
            // (A hidden bone: all its vertices at the pelvis, its triangles gone.)
            const bool shown = b < p.bodies;
            const glm::mat3 r = shown ? glm::mat3_cast(p.rot[static_cast<za::SizeT>(b)]) * p.scale : glm::mat3{0.f};
            const glm::vec3 t = shown ? p.pos[static_cast<za::SizeT>(b)] - s.ref : glm::vec3{0.f};
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
            if(bone.joint != Joint::Loose && bone.parent >= 0)
            {
                decals::limbTrail(num * maxBones + b, p.rot[static_cast<za::SizeT>(b)] * (bone.end * p.scale) + p.pos[static_cast<za::SizeT>(b)]);
            }
        }
        e->model = skinned;
        e->previouspose = e->currentpose = 0; // (its one pose: no lerp from the corpse's)
        e->lerpflags &= static_cast<byte>(~(LERP_RESETANIM | LERP_RESETANIM2));
        draw.swapped.pushBack(s);
    }
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

bool skinnedVertices(int num, za::Vector<glm::vec3>& out, za::Vector<glm::vec3>* normals)
{
    if(num < 0 || num >= static_cast<int>(draw.byNum.size()) || !draw.byNum[static_cast<za::SizeT>(num)].rig)
    {
        return false;
    }
    const Published& p = draw.byNum[static_cast<za::SizeT>(num)];
    const Rig& rig = *p.rig;
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
        out[static_cast<za::SizeT>(v)] = b < p.bodies ? p.rot[static_cast<za::SizeT>(b)] * (r * p.scale) + p.pos[static_cast<za::SizeT>(b)] : p.pos[0];
        if(normals)
        {
            const glm::vec3 n{r_avertexnormals[t.lightnormalindex][0], r_avertexnormals[t.lightnormalindex][1],
                r_avertexnormals[t.lightnormalindex][2]};
            (*normals)[static_cast<za::SizeT>(v)] = b < p.bodies ? p.rot[static_cast<za::SizeT>(b)] * n : glm::vec3{0.f};
        }
    }
    return true;
}

bool drawnPart(int num, int part, glm::quat& rot, glm::vec3& pos, float& scale, const Rig** rig)
{
    if(num < 0 || num >= static_cast<int>(draw.byNum.size()) || !draw.byNum[static_cast<za::SizeT>(num)].rig || part < 0)
    {
        return false;
    }
    const Published& p = draw.byNum[static_cast<za::SizeT>(num)];
    if(part >= p.bodies)
    {
        return false;
    }
    if(rig)
    {
        *rig = p.rig;
    }
    // Swapped this frame: its skinning matrix (what is drawn), else the bodies published.
    for(const Swapped& s : draw.swapped)
    {
        if(s.num == num && part < s.bones)
        {
            const float* m = &s.skin[static_cast<za::SizeT>(part * 12)];
            glm::mat3 r;
            for(int row = 0; row < 3; row++)
            {
                for(int c = 0; c < 3; c++)
                {
                    r[c][row] = m[row * 4 + c];
                }
            }
            scale = glm::length(r[0]);
            if(scale <= 0.f)
            {
                return false;
            }
            rot = glm::normalize(glm::quat_cast(r / scale));
            pos = glm::vec3{m[3], m[7], m[11]} + s.ref;
            return true;
        }
    }
    rot = p.rot[static_cast<za::SizeT>(part)];
    pos = p.pos[static_cast<za::SizeT>(part)];
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
    if(len <= suffix || strcmp(mod->name + len - suffix, skinnedSuffix) != 0)
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
