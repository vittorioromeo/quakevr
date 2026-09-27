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

#pragma once

#include "vr_engine.hpp"

#include <array>
#include <vector>

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

struct Pose
{
    float curl[FingerCount][jointsPerFinger]{}; // curl frames, per finger (Finger) and joint (0 the knuckle)
    glm::quat metacarpal{1.f, 0.f, 0.f, 0.f};   // the thumb's turn at its base, relative to the palm
    glm::vec3 shift[FingerCount]{};             // each finger's move from its bind place (its settings)
};

// A pose: each finger's root (0: its move) and segments (1..3), each joint's turn, every MD5 joint (from rest to
// the pose) as transforms of rig space, and the finger vertices where they are.
struct Posed
{
    Rigid segment[FingerCount][jointsPerFinger + 1];
    glm::quat turn[FingerCount][jointsPerFinger]; // relative to the segment before
    std::array<Rigid, data::numJoints> joint;
    std::array<glm::vec3, data::numVertices> vertex;
};

void pose(const Pose& p, Posed& out);

// Only the segments of that finger (no vertices: the grasp solver's fast path).
void fingerSegments(const Pose& p, int finger, const float curls[jointsPerFinger], Rigid out[jointsPerFinger + 1]);

// The largest turn (radians) a joint makes per curl frame, over its path.
[[nodiscard]] float jointRate(int finger, int joint);

// A palm vertex (data::palmVertices) where the pose puts it (the thenar following the metacarpal).
[[nodiscard]] glm::vec3 palmVertex(const Posed& posed, int i);

// Skinning matrices (3x4 row-major, as bonepose_t) for progs/hand_rig.md5mesh's joints.
void skin(const Posed& posed, float out[data::numJoints * 12]);

// Forgets the model checked (a game directory change reuses its slot).
void reset();

// Whether `model` is the rig with the joints these tables expect (else the six models are drawn).
[[nodiscard]] bool usable(qmodel_t* model);

} // namespace qvr::handrig
