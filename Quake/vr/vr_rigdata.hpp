// vr_rigdata.hpp -- the rigs' and animations' override data (the Rig Studio's: docs/vr-port/RIGSTUDIO.md): what is
// edited over a model's computed rig (vr_ragdoll.cpp's, derived from the .mdl) and over its frames, kept in readable text
// files and read back at a map load:
// - quakevr/rigs/<model>.rig.txt: bone overrides (the rest pose's joints and ends, the joints' limits), vertex weights,
//   named poses, and override tracks (keyed per-bone turns laid on one of the model's own animations, by its frames);
// - quakevr/anims/<model>/<name>.anim.txt: a new keyed sequence (its length, fps, loop, blends, events, its bones' keys).
// <model> is the model's file name without its folder and extension (progs/vrgrunt.mdl: vrgrunt).
//
// The pose model: each bone's place is its parent's carried by a local motion in the pivot form, p' = R (p - c) + c + o
// (c its rest pivot; R a rotation, o an offset): a bone's world motion F_b = F_parent L_b (the root's: L_root alone),
// every L the identity being the rest pose (the .mdl's first frame). A frame of the model gives each bone's rest -> pose
// motion P_b (the rig's fit), its locals L_b = P_parent^-1 P_b. An override track is a delta D_b laid after the frame's
// own local (F_b = F_parent L_b D_b); a sequence's keys are locals themselves (absolute, from the rest pose).
//
// Numbers are written with %.6g and read back with strtof: a file read and written again comes out byte for byte the
// same (the turns are kept as written, rotation vectors in degrees, not as quaternions).
//
// With no files nothing here is loaded or applied: the computed rig and the frames are drawn as they always were.

#pragma once

#include "vr_engine.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rigpose.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace qvr::rigdata
{

inline constexpr int maxBones = ragdoll::maxBones;
inline constexpr int maxWeights = 4; // influences a vertex

// A bone's local motion (the pivot form): `rot` a rotation vector in degrees (its axis times its angle), `off` units.
struct Local
{
    glm::vec3 rot{0.f};
    glm::vec3 off{0.f};
    [[nodiscard]] bool operator==(const Local& o) const { return rot == o.rot && off == o.off; }
};
[[nodiscard]] glm::quat toQuat(const glm::vec3& rotDeg);
[[nodiscard]] glm::vec3 toRotDeg(const glm::quat& q);
// As a rigid motion about pivot `c`, and back.
[[nodiscard]] rigpose::Rigid toRigid(const Local& l, const glm::vec3& c);
[[nodiscard]] Local fromRigid(const rigpose::Rigid& r, const glm::vec3& c);
[[nodiscard]] rigpose::Rigid compose(const rigpose::Rigid& a, const rigpose::Rigid& b); // a after b
[[nodiscard]] rigpose::Rigid inverse(const rigpose::Rigid& a);
[[nodiscard]] rigpose::Rigid lerp(const rigpose::Rigid& a, const rigpose::Rigid& b, float t);

// A whole pose: each bone's local, `mask` the bones it holds (a pose saved from some bones).
struct Pose
{
    Local bone[maxBones];
    uint32_t mask{0};
};

// A rest-pose override of one bone (`on`: the bone has one). Its fields replace the rig's.
struct BoneEdit
{
    bool on{false};
    glm::vec3 pivot{0.f}, end{0.f};
    float cone{0.f}, twist{0.f};   // degrees (a ball joint)
    float lower{0.f}, upper{0.f};  // degrees (a hinge)
    glm::vec3 hinge{0.f, 1.f, 0.f};
};

struct VertWeights
{
    int v{0};
    int n{0};
    int bone[maxWeights]{};
    float w[maxWeights]{};
};

enum class Interp : int
{
    Smooth = 0, // Catmull-Rom through the keys (eased at the ends)
    Linear = 1,
    Step = 2,   // held to the next key
};
[[nodiscard]] const char* interpName(Interp i);

struct Key
{
    float frame{0.f};
    Local value;
    Interp interp{Interp::Smooth}; // from this key to the next
};

// One bone's keys, in frame order.
struct Track
{
    int bone{0};
    za::Vector<Key> keys;
};

// The override tracks laid on one of the model's animations (a frame group by its frames' names: "load" for load1..
// load11; its frames numbered from 0).
struct AnimOverride
{
    za::String anim;
    za::Vector<Track> tracks;
};

struct NamedPose
{
    za::String name;
    Pose pose;
};

struct Event
{
    int frame{0};
    za::String name;
};

struct Sequence
{
    za::String name;
    float fps{10.f};
    int length{10};          // frames (its last key at length - 1 for a loop's seam, or the end)
    bool loop{false};
    float blendIn{0.2f};     // seconds from what it was doing
    float blendOut{0.2f};    // ... back to it
    bool arms{false};        // its arm bones drive an armed humanoid's arms (their hands' places: the arms' IK onto them)
    bool hold{true};         // played over a held frame: the monster's own frame changing ends it (blended out)
    za::String base;         // a frame drawn under it (by name; "": the model's first)
    za::Vector<Event> events;
    za::Vector<Track> tracks;
};
[[nodiscard]] float seconds(const Sequence& s);

// What one model's files hold.
struct ModelData
{
    za::String model;        // its path in the game (progs/vrgrunt.mdl)
    za::String base;         // vrgrunt
    // The rig file
    za::String boneName[maxBones]; // the files' bone names by the rig's index (filled when the rig is known)
    BoneEdit bones[maxBones];
    za::Vector<VertWeights> weights; // by vertex, ascending
    za::Vector<NamedPose> poses;
    za::Vector<AnimOverride> overrides;
    // Its sequences (one file each)
    za::Vector<Sequence> seqs;
    bool haveRigFile{false};
    // For the authored rig (vr_rig2 1: ragdoll::Rig::authored): its own files (rigs/<model>.rig2anim.txt: poses and
    // override tracks, no bone overrides or weights: the authored rig's file has those; anims/<model>/<name>.anim2.txt),
    // their keys written in each bone's own frame (rig2::keyAxes), held within its joint's limits.
    bool authored{false};
    // Read from the files' bone names before a rig is known: kept by name, put to indices on apply.
    za::Vector<za::String> pendingNames; // (internal)
};

// The base name of a model ("progs/vrgrunt.mdl" -> "vrgrunt").
void baseName(const char* model, char* out, int size);

// The folder the files are read from and written to: com_gamedir, or vr_rigdata_dir under it (the tests' own).
void rootDir(char* out, int size);
// (`authored`: the authored rig's files: <model>.rig2anim.txt, <name>.anim2.txt)
void rigPath(const char* base, char* out, int size, bool authored = false);
void animDir(const char* base, char* out, int size);
void animPath(const char* base, const char* name, char* out, int size, bool authored = false);

// Text form (the files): the whole file's text. Bones by `rig`'s names.
void writeRig(const ModelData& m, const ragdoll::Rig& rig, za::String& out);
void writeSequence(const ModelData& m, const Sequence& s, const ragdoll::Rig& rig, za::String& out);
// Read: false (said why in `error`) if the text is not one. Bones by `rig`'s names (unknown ones skipped, said).
bool readRig(const char* text, const ragdoll::Rig& rig, ModelData& m, za::String& error);
bool readSequence(const char* text, const ragdoll::Rig& rig, Sequence& s, za::String& error);

// The registry: each model's data, read from its files when first asked for (once a map: a map load reads them again,
// and the studio's save puts its copy here). Null: the model has no files (or no rig).
[[nodiscard]] const ModelData* of(const qmodel_t* model);
// The studio's working copy made the model's (live: the game draws it at once).
void publish(const qmodel_t* model, const ModelData& m);
// Read again from the files (a map load; the studio's Revert).
void reload(const qmodel_t* model);
// Whether any model has tracks or sequences loaded (the game's layer looks at nothing otherwise).
[[nodiscard]] bool anyAnimation();
// A map's load (VR_NewMap): the registry emptied; each model read again when next asked for.
void newMap();

// The rig derivation's end (vr_ragdoll.cpp rigFor, warmRigs; the main thread): `rig` with the model's rig file's bone
// overrides and weights laid on it (its fits refitted for the bones whose vertices changed). False: no file, untouched.
bool applyToRig(qmodel_t* model, const aliashdr_t* hdr, ragdoll::Rig& rig);

// The authored rig's limits on a key (rig.authored; else untouched, false): bone b's local `L` (rest -> pose, its
// parent's rest space: a sequence's key, or the frame's local and an override's delta laid on it) with its turn held
// within its joint's limits; a jointed bone has no offset of its own (the root keeps its). False: it was within them.
bool holdLocal(const ragdoll::Rig& rig, int b, rigpose::Rigid& L, const glm::vec3& pivot);
// ... a key: `base` the local it is laid on (an override's frame; identity for a sequence's absolute key).
bool holdKey(const ragdoll::Rig& rig, int b, const rigpose::Rigid& base, Local& key, const glm::vec3& pivot);
// Evaluation
// A track at `frame` (its keys' interpolation; before the first key the first's value, after the last the last's; a
// loop of `length` frames wraps from the last key to the first). False: no keys.
bool sample(const Track& t, float frame, float loopLength, Local& out);
// The model's animation frame groups (by the frames' names, numbered from 0): the group of frame `frame`, its index.
struct AnimGroup
{
    char name[16]{};
    int first{0};
    int count{0};
};
int animGroups(const aliashdr_t* hdr, AnimGroup* out, int max);
bool groupOfFrame(const aliashdr_t* hdr, int frame, AnimGroup& out, int& index);
[[nodiscard]] int frameOfName(const aliashdr_t* hdr, const char* name); // its own name, or its group's and number (stand1)
void frameName(const aliashdr_t* hdr, int frame, char* out, int size);   // its group's name and number in it
[[nodiscard]] int poseOfFrame(const aliashdr_t* hdr, int frame);
[[nodiscard]] int frameOfPose(const aliashdr_t* hdr, int pose); // (-1: a pose inside a framegroup)

// FK: the bones' world (model space) motions from their locals (`F`: numBones of them; rest pivots from `pivots`).
void forward(const ragdoll::Rig& rig, const glm::vec3* pivots, const Local* locals, rigpose::Rigid* F);
// The locals of a frame's fits (P_b: rest -> pose).
void localsOf(const ragdoll::Rig& rig, const glm::vec3* pivots, const rigpose::Rigid* P, Local* out);
// The bones' rest pivots and ends with `m`'s overrides (m null: the rig's).
void restOf(const ragdoll::Rig& rig, const ModelData* m, glm::vec3* pivots, glm::vec3* ends);

void registerCommands(); // vr_rigdata_info, vr_rigdata_reload

} // namespace qvr::rigdata
