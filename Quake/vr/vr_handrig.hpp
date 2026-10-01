// vr_handrig.hpp -- the jointed hand (round 21, "fitted hands"; remodelled in "Hands remodelled"): the drawn hand
// as one skinned model (progs/hand_rig.md5mesh, made by Misc/quakevr/make_hand_rig.py) with three hinged joints
// per finger and a thumb that turns at its carpometacarpal joint near the wrist, instead of the palm and five
// finger models turned by their six curl frames (vr_hand_rig 0 still draws those).
//
// Each joint has its own curl, in the old models' curl frames (0 open .. 4 the tightest fist; 5 is 3's shape, as
// vr_view.cpp's full grip draws): the generator gives each joint its turn per frame, and a curl between frames
// turns it between them. The thumb's first joint (its metacarpal, at the wrist's end of the palm) also takes the
// grasp's opposition turn (Pose::metacarpal) and carries the ball of the thumb.
//
// Skinning: the MD5's joints are the palm, the 15 segments, a helper at each joint turned half as far (the ring of
// vertices there: mitred joints, no candy-wrapping) and two more at the thumb's base (the thenar follows a share
// of its turn). Their matrices are computed here from the pose, and the GPU skins the mesh (the body's path,
// VR_AliasBonePoses); the same blend gives the vertices on the CPU (collisions, dumps).
//
// Space ("rig space"): hand_base.mdl's model space, hand model units (+x towards the fingers, +y the palm's side,
// +z the thumb's and index finger's side). The entity is placed where the palm model's origin is drawn.
//
// The rig is read from the file (round 21, "Hand editable in Blender"): progs/hand_rig.md5mesh gives the mesh, its
// weights and the joints' pivots, and everything else the hand needs is derived from it when it is loaded (rig()):
// each hinge turns with its finger's new direction, and the grasp solver's spheres follow the mesh around them (see
// vr_handrig.cpp). The engine checks the file as it loads the model (VR_ModelReplacementOk) and vr_hand_reload loads
// it again; a file it can't use is refused with the reason, and the hand it had is kept. The tables compiled in
// (vr_handrig_data.inc, the shipped hand) are the rig until a file is read, and the reference the derivation
// measures edits against. Fixed whatever the file says: the palm's frame (rig space itself) and the placement
// constants (baseScaleOrigin, the fingers' offsets, palmCentre: weapons and cups are placed from them), the wrist, the
// curl frames' angles.

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"


namespace qvr::handrig
{

namespace data
{
#include "vr_handrig_data.inc"
}

enum Finger : int
{
    Thumb,
    Index,
    Middle,
    Ring,
    Pinky,
    FingerCount
};

inline constexpr int jointsPerFinger = 3;
inline constexpr const char* modelName = "progs/hand_rig.mdl";
inline constexpr const char* meshFile = "progs/hand_rig.md5mesh";
inline constexpr const char* animFile = "progs/hand_rig.md5anim";

// A rigid transform of rig space.
struct Rigid
{
    glm::mat3 r{1.f};
    glm::vec3 t{0.f};

    [[nodiscard]] glm::vec3 operator()(const glm::vec3& p) const { return r * p + t; }
    [[nodiscard]] Rigid operator*(const Rigid& o) const { return {r * o.r, r * o.t + t}; }
    [[nodiscard]] Rigid inverse() const
    {
        const glm::mat3 rt = glm::transpose(r);
        return {rt, -(rt * t)};
    }
};

// A vertex of the hand at rest: its place and its joints (up to 4) with their weights.
struct Vertex
{
    glm::vec3 pos{0.f};
    int count{1};
    short joint[4]{};
    float weight[4]{};
};

struct SegmentSphere
{
    int finger{0}, bone{1}; // bone: the segment, 1..3
    glm::vec3 c{0.f};
    float r{0.f};
};

struct Sphere
{
    glm::vec3 c{0.f};
    float r{0.f};
};

// The rig in use: the compiled one, or the one read from progs/hand_rig.md5mesh.
struct Rig
{
    za::String source{"compiled"};
    glm::vec3 pivot[FingerCount][jointsPerFinger]{};
    glm::quat turns[FingerCount][data::numFrames][jointsPerFinger]; // per curl frame, relative to the segment before
    za::Vector<Vertex> vertices;                                  // the whole mesh
    za::Vector<za::Array<int, 3>> triangles;                     // clockwise seen from outside
    za::Vector<SegmentSphere> segmentSpheres;                     // the grasp solver's, at rest
    za::Vector<Sphere> palmSpheres, thenarSpheres;
};

[[nodiscard]] const Rig& rig();

// Changes whenever another rig is put in use (a grasp solved with the last one is solved again).
[[nodiscard]] unsigned generation();

struct Pose
{
    float curl[FingerCount][jointsPerFinger]{}; // curl frames, per finger (Finger) and joint (0 the knuckle)
    glm::quat metacarpal{1.f, 0.f, 0.f, 0.f};   // the thumb's turn at its base, relative to the palm
    glm::vec3 shift[FingerCount]{};             // each finger's move from its bind place (its settings)
};

// A pose: each finger's root (0: its move) and segments (1..3), each joint's turn, and every MD5 joint (from rest to
// the pose) as transforms of rig space.
struct Posed
{
    Rigid segment[FingerCount][jointsPerFinger + 1];
    glm::quat turn[FingerCount][jointsPerFinger]; // relative to the segment before
    za::Array<Rigid, data::numJoints> joint;
};

void pose(const Pose& p, Posed& out);

// Only the segments of that finger (no vertices: the grasp solver's fast path).
void fingerSegments(const Pose& p, int finger, const float curls[jointsPerFinger], Rigid out[jointsPerFinger + 1]);

// The largest turn (radians) a joint makes per curl frame, over its path.
[[nodiscard]] float jointRate(int finger, int joint);

// Every vertex of the mesh (rig().vertices) where the pose puts it (the GPU's blend, on the CPU).
void vertices(const Posed& posed, za::Vector<glm::vec3>& out);

// Skinning matrices (3x4 row-major, as bonepose_t) for progs/hand_rig.md5mesh's joints.
void skin(const Posed& posed, float out[data::numJoints * 12]);

// Forgets the model checked (a game directory change reuses its slot).
void reset();

// Whether `model` is the rig with the joints these tables expect (else the six models are drawn).
[[nodiscard]] bool usable(qmodel_t* model);

// vr_hand_reload: reads the hand's files again (edited in Blender: Misc/quakevr/blender) and, if they are usable,
// draws and grasps with them; else says why and keeps the hand it had.
void reload_f();

// vr_hand_rig_info: where the rig in use came from, and how it differs from the compiled one.
void info_f();

} // namespace qvr::handrig
