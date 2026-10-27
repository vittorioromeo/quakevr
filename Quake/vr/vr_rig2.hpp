#pragma once

// vr_rig2.hpp -- the authored humanoid rig, stage 1 of the grunt rig rebuild (docs/vr-port/ROUND22.md, "Grunt rig
// rebuild, stage 1"; vr_rig2 1): a skeleton with anatomical joints (pelvis, spine, chest, neck, head, clavicles, upper
// arms, forearms, hands, thighs, shins, feet), each joint's limits (a ball's elliptical swing cone and twist, a hinge's
// range from straight) and blended skin weights (up to 4 bones a vertex), read from quakevr/rigs/<model>.rig2.txt
// (made by Misc/quakevr/make_rig2.py; hand-editable; the Rig Studio's Range of Motion window edits and saves it).
//
// Nothing in the game uses it yet: the Rig Studio draws its subject with it (Range of Motion, vr_rig2 1) and
// vr_rig2_check measures it (every joint swept through its range, pairs of joints, ragdoll-like random poses within the
// limits: see-through holes, flipped and collapsed triangles, folds, the knees' and elbows' bend, stretch, a coarse
// self-intersection count), beside the old derived rig (vr_ragdoll.cpp) on the same poses.
//
// A pose: each joint's angles in degrees: a ball's (swing x toward its `fwd`, swing y toward its `side`, twist about
// the bone), a hinge's (bend from straight, 0, 0). Each bone's motion is its parent's carried by its joint's turn
// about its pivot (the rest pose's angles to the pose's), so a bone's world motion is F_b = F_parent L_b; the vertices
// are blended linearly (p' = sum w_i F_i p) or, with dual quaternions (vr_rig2_dq), without the candy wrapper.

#include "vr_engine.hpp"
#include "vr_rigpose.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace qvr::rig2
{

inline constexpr int maxBones = 32;
inline constexpr int maxWeights = 4;

enum class Joint : uint8_t
{
    Root,  // the pelvis: no joint
    Ball,  // swing (an elliptical cone, each quadrant its own radius) and twist
    Hinge, // one axis, a range of bend from straight
    Fixed, // carried rigidly (the grunt's gun on his right hand)
};

struct Bone
{
    char name[24]{};
    int parent{-1};
    Joint joint{Joint::Root};
    glm::vec3 pivot{0.f}, end{0.f};                // the rest pose (model units)
    glm::vec3 neutral{0.f, 0.f, -1.f};             // Ball: the swing's zero (the bone's direction there)
    glm::vec3 fwd{1.f, 0.f, 0.f}, side{0.f, 1.f, 0.f}; // Ball: the swings' directions (made orthonormal)
    glm::vec2 swingX{0.f}, swingY{0.f}, twist{0.f}; // Ball: (min, max) degrees
    glm::vec3 axis{0.f, 1.f, 0.f};                 // Hinge: bending turns it about this (rest space)
    glm::vec2 range{0.f};                          // Hinge: (min, max) degrees from straight
};

struct Weights
{
    int n{0};
    int bone[maxWeights]{};
    float w[maxWeights]{};
};

struct Rig
{
    za::String model;  // its .mdl ("progs/vrgrunt.mdl")
    za::String path;   // the file it was read from
    int numBones{0};
    Bone bones[maxBones];
    za::Vector<Weights> weights; // per .mdl vertex
    int generation{0};           // bumped on every edit (the drawn model's weights made again)
};

struct Pose
{
    glm::vec3 a[maxBones]{}; // degrees: Ball (swing x, swing y, twist), Hinge (bend, 0, 0)
};

// The rig of `model` (its file read on first use; nullptr: none). Kept for the session; reload() reads it again.
[[nodiscard]] Rig* rigFor(const qmodel_t* model);
void reload();

// A bone by name (-1 none).
[[nodiscard]] int boneByName(const Rig& rig, const char* name);

// Each joint's frame, as derived from its bone (its rest direction, the swing's zero): the rest pose's angles.
[[nodiscard]] glm::vec3 restAngles(const Rig& rig, int b);
[[nodiscard]] Pose restPose(const Rig& rig);
// Within the joint's limits (a ball's swing onto its ellipse, its twist and a hinge's bend clamped).
[[nodiscard]] glm::vec3 clampAngles(const Rig& rig, int b, const glm::vec3& a);
[[nodiscard]] bool withinLimits(const Rig& rig, int b, const glm::vec3& a, float slackDeg = 0.01f);
// A random pose within the limits (a ragdoll's: each angle uniform over its range, a swing uniform over its ellipse).
[[nodiscard]] Pose randomPose(const Rig& rig, uint32_t& seed);

// The bones' motions (rest -> pose, model space) for `pose`: out[numBones].
void forward(const Rig& rig, const Pose& pose, rigpose::Rigid* out);
// The angles that turn bone b's rest direction toward model-space direction `dir` (its parent as posed by `Fparent`),
// within its limits, its twist kept (a limb dragged by its end).
[[nodiscard]] glm::vec3 aimAngles(const Rig& rig, int b, const rigpose::Rigid& Fparent, const glm::vec3& dir,
    const glm::vec3& current);

// Vertices skinned: out[v] = blend of the bones' motions of rest[v] (linear, or dual quaternion with `dq`).
void skin(const Rig& rig, const rigpose::Rigid* F, const glm::vec3* rest, int numVerts, glm::vec3* out, bool dq);
[[nodiscard]] float weightOn(const Rig& rig, int v, int bone);

// The file (text; the format: make_rig2.py's header and the file's own comment lines).
[[nodiscard]] bool read(const char* text, Rig& out, za::String& error);
void write(const Rig& rig, za::String& out);
[[nodiscard]] bool save(Rig& rig, za::String& pathOut);

// The Rig Studio's subject drawn with the rig (vr_rig2 1, Range of Motion: Game display): `entnum` drawn this frame
// with these bones (cleared each frame after drawing: swapModels / restoreModels, beside the ragdolls').
void drawPosed(int entnum, const qmodel_t* model, const rigpose::Rigid* F, int numBones);
void swapModels();
void restoreModels();
[[nodiscard]] int bonePoses(const struct entity_s* e, const float** matrices);
// Mod_LoadModel's "<model>#rig2": the .mdl's rest pose as a skeletal mesh, 4 weights a vertex (the rig's).
[[nodiscard]] bool makeSkinnedModel(qmodel_t* mod);

// vr_rig2_check's measures of one pose (the Rig Studio's live report): of the authored rig, or of the old rig moved alike
// (`old`: each old bone as its namesake). False: no rig.
struct Faults
{
    int seeThrough{0}, flipped{0}, collapsed{0}, folds{0};
    float foldMax{0.f}, stretchMin{1.f}, stretchMax{1.f};
};
[[nodiscard]] bool measurePose(qmodel_t* model, const Pose& pose, bool old, Faults& out);

void init(); // its commands (vr_rig2_check, vr_rig2_info, vr_rig2_reload)

} // namespace qvr::rig2
