#pragma once

// vr_rig2.hpp -- the authored humanoid rig of the grunt and the enforcer (docs/vr-port/ROUND22.md, "Grunt rig rebuild,
// stage 1" .. "The authored rig the only one"): a skeleton with anatomical joints (pelvis, spine, chest, neck, head,
// clavicles, upper arms, forearms, hands, thighs, shins, feet), each joint's limits (a ball's elliptical swing cone and
// twist, a hinge's range from straight) and blended skin weights (up to 4 bones a vertex), read from
// quakevr/rigs/<model>.rig2.txt (made by Misc/quakevr/make_rig2.py; hand-editable; the Rig Studio's Range of Motion
// window edits and saves it). The only rig of the models with such a file (the other monsters keep the rig derived from
// their frames, vr_ragdoll.cpp).
//
// vr_rig2_check measures it (every joint swept through its range, pairs of joints, ragdoll-like random poses within the
// limits: see-through holes, flipped and collapsed triangles, folds, the knees' and elbows' bend, stretch, a coarse
// self-intersection count).
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

namespace qvr::ragdoll
{
struct Rig;
}
namespace qvr::agarms
{
struct Placed;
}
namespace qvr::aglegs
{
struct Placed;
}

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

// The IK stretch on the authored rig (agarms::Placed::stretch; agpose::stretch): bone `bone` lengthened by `by` along
// `axis` (unit, rest model space) about `pivot` (its joint): its motion is that scale, then its Rigid. Its children ride
// at its stretched end (armsOnRig; stretchChain).
struct BoneStretch
{
    int bone{-1};
    glm::vec3 pivot{0.f}, axis{0.f};
    float length{0.f}; // (pivot to its child's joint along the axis)
    float by{1.f};
};
inline constexpr int maxStretches = 4;
struct Stretches
{
    BoneStretch b[maxStretches];
    int n{0};
};
// A rest place as bone `s` stretches it (before its Rigid).
[[nodiscard]] inline glm::vec3 stretchedPlace(const BoneStretch& s, const glm::vec3& p)
{
    return p + ((s.by - 1.f) * glm::dot(p - s.pivot, s.axis)) * s.axis;
}
// Bones `F` (forward's, no stretch) with `st` laid on: each stretched bone's descendants moved to its stretched end.
void stretchChain(const Rig& rig, rigpose::Rigid* F, const Stretches& st);

// Vertices skinned: out[v] = blend of the bones' motions of rest[v] (linear, or dual quaternion with `dq`); `st` the IK
// stretch's bones (linear only).
void skin(const Rig& rig, const rigpose::Rigid* F, const glm::vec3* rest, int numVerts, glm::vec3* out, bool dq,
    const Stretches* st = nullptr);
[[nodiscard]] float weightOn(const Rig& rig, int v, int bone);

// The file (text; the format: make_rig2.py's header and the file's own comment lines).
[[nodiscard]] bool read(const char* text, Rig& out, za::String& error);
void write(const Rig& rig, za::String& out);
[[nodiscard]] bool save(Rig& rig, za::String& pathOut);

// The Rig Studio's subject drawn with the rig (Range of Motion: Game display): `entnum` drawn this frame
// with these bones (cleared each frame after drawing: swapModels / restoreModels, beside the ragdolls').
void drawPosed(int entnum, const qmodel_t* model, const rigpose::Rigid* F, int numBones);
void swapModels();
void restoreModels();
[[nodiscard]] int bonePoses(const struct entity_s* e, const float** matrices);
void drawInfo(); // vr_rig2_draw_info: the drawn subject's models (its .mdl, the authored skinned copy), their skins and how often drawn
// The .mdl of an entity drawn now on the authored rig (swapped to "<model>#rig2"), else nullptr: what it is shaded as
// (its occluder, its occlusion group: vr_ao.cpp), as on the old rig.
[[nodiscard]] const qmodel_t* sourceModel(const struct entity_s* e);
// The Rig Studio's subject posed on the authored rig lately (drawPosed this frame or the last), swapped or not: its .mdl,
// else nullptr. (The swap is undone after each frame's drawing, so code before the next swap, VR_RelinkArmedGrunts, sees
// the .mdl: sourceModel misses it there.)
[[nodiscard]] const qmodel_t* studioPosedModel(const struct entity_s* e);
// Mod_LoadModel's "<model>#rig2": the .mdl's rest pose as a skeletal mesh, 4 weights a vertex (the rig's).
[[nodiscard]] bool makeSkinnedModel(qmodel_t* mod);

// vr_rig2_check's measures of one pose (the Rig Studio's live report) of the authored rig. False: no rig.
struct Faults
{
    int seeThrough{0}, flipped{0}, collapsed{0}, folds{0};
    float foldMax{0.f}, stretchMin{1.f}, stretchMax{1.f};
};
[[nodiscard]] bool measurePose(qmodel_t* model, const Pose& pose, Faults& out);

void init(); // its commands (vr_rig2_check, vr_rig2_info, vr_rig2_reload)

// ---- The game on the authored rig (ROUND22.md, "Grunt rig, stage 2"; vr_rig2_game.cpp) ----
// The grunt's and the enforcer's ragdoll rig (vr_ragdoll.cpp rigFor: its bones, joints, limits and blended skin the
// authored rig's), their live drawing (their frames' fits, hit reactions, an armed one's arms and legs, on the authored
// bones, drawn with "<model>#rig2") and everything made from their rig.
void latch(); // VR_Rig2Latch (a map spawned): a rig's file changed since (vr_rig2_reload), what is made from it made again
// A joint's local turn (rest -> pose, the parent's rest space) as its angles (unclamped: forward's inverse).
[[nodiscard]] glm::vec3 anglesOf(const Rig& rig, int b, const glm::quat& local);
// A joint's local turn from its angles (forward's per joint).
[[nodiscard]] glm::quat localOf(const Rig& rig, int b, const glm::vec3& angles);
// The Rig Studio's keys on the authored rig (vr_rigdata.cpp, RIGSTUDIO.md "Animating the authored rig"): bone b's own
// frame the files write its keys' turns and offsets in (columns: x, y, z in the rest pose's model space): a ball's
// swing X axis, swing Y axis and its neutral direction (the key's x, y, z read as Swing X, Swing Y, Twist), a hinge's
// axis, then across and along the bone; the root and a fixed bone the model's axes.
[[nodiscard]] glm::mat3 keyAxes(const Rig& rig, int b);
// A joint's local turn held within its limits (a ball's swing onto its ellipse and its twist, a hinge's bend); the
// root and fixed bones unchanged. False: it was within them (`local` untouched).
bool clampLocal(const Rig& rig, int b, glm::quat& local);
// A joint as Box3D's (vr_box3d_ragdoll.cpp): its frames in the parent's and its own rest space; a ball's elliptical cone
// round its swing's middle (half angles, rad, toward frameA's x and y) and its twist either way (rad, round its middle);
// a hinge's range from the rest pose (rad).
struct PhysJoint
{
    glm::quat frameA{1.f, 0.f, 0.f, 0.f}, frameB{1.f, 0.f, 0.f, 0.f};
    float coneX{0.f}, coneY{0.f}, twist{0.f}, lower{0.f}, upper{0.f};
};
[[nodiscard]] PhysJoint physJoint(const Rig& rig, int b);
// The skinned copy ("<model>#rig2") drawn for `model` now (made again after the rig's file changed, its occlusion caught
// up): a ragdoll's (vr_ragdoll.cpp swapModels) and a live one's. Null: none.
[[nodiscard]] qmodel_t* skinnedFor(const qmodel_t* model);
// A live entity drawn on the authored rig this frame (the game's: vr_rig2_game.cpp): `F` its bones (rest -> drawn, model
// space). The Rig Studio's own pose (drawPosed) wins over it.
void drawLive(int entnum, const qmodel_t* model, const rigpose::Rigid* F, int numBones, uint32_t hidden = 0,
    const Stretches* st = nullptr); // (hidden: bones collapsed; st: the IK stretch's bones, scaled along their axes)
// Entity `num`'s bone `b` as last drawn on the authored rig (rest -> drawn, model space) if drawn in the last frames (a
// ragdoll made of it starts there: vr_box3d_ragdoll.cpp). False: not drawn so lately.
bool drawnBone(int num, int b, glm::quat& rot, glm::vec3& pos);
[[nodiscard]] bool drawnNow(int num); // published for this frame already (drawLive, drawPosed)
// Entity `num` as it is outside a frame's drawing (its own model and lerp, not its swapped copy's). False: none.
bool unswapped(int num, entity_t& out);

// vr_rig2_game.cpp: what the game draws on the authored rig.
void resetGame(); // a rig's file changed: what was made from it made again (ragdoll rigs, arm skins, legs...)
// The bones between the frame lerp's two poses: each bone's local motion on its parent blended (its turn about its
// joint), carried down the rig.
void posedBetween(const ragdoll::Rig& rig, int pose1, int pose2, float blend, rigpose::Rigid* out);
// ... or along r_lerpmodels 3's spline through four poses (vr_animsmooth.cpp's keys; the old path's limbs), else posedBetween.
void posedSmooth(const entity_t* e, const aliashdr_t* hdr, const ragdoll::Rig& rig, int pose1, int pose2, float blend, rigpose::Rigid* out);
// An armed one's arms (agarms' solve) on the authored bones within their limits: `F` every bone as drawn, its arm bones
// (clavicle, upper arm, forearm, hand, the weapon on the right hand) set from `placed`; `weaponCorr` each hand's drawn
// motion against the solve's (its weapon carried by it).
// `stretchOut`: the IK stretch's arm bones (Placed::stretch: the upper arm and forearm lengthened alike, their children
// at the stretched ends), for drawLive.
void armsOnRig(const Rig& a, const agarms::Placed& placed, rigpose::Rigid* F, rigpose::Rigid* weaponCorr, Stretches* stretchOut = nullptr);
// A take's legs (aglegs' solve) on the leg bones (thigh at the knee, the shin's hinge at the ankle, the foot at the toe).
void legsOnRig(const Rig& a, const aglegs::Placed& lp, rigpose::Rigid* F);
// hitreact.cpp relinkSkinned: an armed one drawn on the authored rig, its bones `posed` and kicked as that solve has
// them, its arms and legs laid on; its weapons in `placed` carried by its hands as drawn. `keyedArms`: the Rig Studio's
// animation has its arms (vr_rigseq.cpp): the arm bones as `posed` has them, not the solve's.
void liveArmed(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, const rigpose::Rigid* kick,
    agarms::Placed& placed, int pose1, int pose2, float blend, bool keyedArms = false);
// Every other live entity on an authored rig drawn so (VR_RagdollSwap, before swapModels): its frames, its kicks.
void advanceLerps(); // VR_RagdollSwap (before the armed ones' arms): their frame lerps advanced, their .mdl drawn no more
void liveAll();
// vr_rig2_record: poses from the game kept (set 0 ragdolls' bodies, 1 the armed ones' drawn bones) for vr_rig2_check recorded.
void notePublished(int num, const ragdoll::Rig& rig, int bodies, const glm::quat* rot);
[[nodiscard]] const za::Vector<Pose>& recordedPoses(int set);
[[nodiscard]] const za::Vector<Stretches>& recordedStretches(int set); // (each recorded pose's IK stretch: set 1's)
void record_f();
void armsReport_f(); // vr_rig2_arms: the arms laid on the authored rig since it last printed
void partsReport_f(); // vr_rig2_parts: each authored ragdoll's parts as drawn, its joints' gaps
void liveCheck_f();   // vr_rig2_live_check: the live ones drawn on the authored rig against their frames
void animCheck_f();   // vr_rig2anim_check: the Rig Studio's override keys as drawn on them (vr_agrunt_test 82)
void animCheckInit(); // ... its totals' cvars (vr_rig2anim_check_passed, _failed)
void framesCheck_f(); // vr_rig2_frames [model]: its frames against the authored joints as the ragdoll has them

} // namespace qvr::rig2
