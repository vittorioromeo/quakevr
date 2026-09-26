// vr_handrig.hpp -- the jointed hand (round 21, "fitted hands"): the drawn hand as one skinned model
// (progs/hand_rig.md5mesh, Misc/quakevr/make_hand_rig.py) with three joints per finger and an opposable
// thumb, instead of the palm and five finger models turned by their six curl frames.
//
// Each joint has its own curl, in the finger models' curl frames (0 open .. 4 the tightest fist; 5 is 3's
// shape, as vr_view.cpp's full grip draws): at whole frames with every joint of a finger at the same curl
// the hand is exactly the old models' (the rig keeps their hand-animated shapes); between them and with
// the joints at different curls (a grasp) it is continuous. The thumb's first joint (the metacarpal, at
// the wrist's end of the palm) turns it across the palm (opposition) and carries the thenar.
//
// Space ("rig space"): hand_base.mdl's model space, hand model units (+x towards the fingers, +y the
// palm's side, +z the thumb's and index finger's side). The engine draws the rig through the body's
// skeletal path (VR_AliasBonePoses): the palm is the entity, each finger vertex has a joint of its own
// whose matrix is computed here from the pose.

#pragma once

#include "vr_engine.hpp"

#include <array>

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

constexpr int jointsPerFinger = 3;
constexpr const char* modelName = "progs/hand_rig.mdl";

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

// A pose's fingers: each finger's root (0: its move) and segments (1..3) as transforms of rig space, and
// the finger vertices where they are.
struct Posed
{
    Rigid segment[FingerCount][jointsPerFinger + 1];
    std::array<glm::vec3, data::numVertices> vertex;
    std::array<glm::vec3, data::numVertices> normal; // the models' normals, in their segments' frames
};

void pose(const Pose& p, Posed& out);

// The same finger at `curls` (its three joints), the rest of `p` as it is: the segments and its
// vertices only (data::firstVertex[finger] ..).
void poseFinger(const Pose& p, int finger, const float curls[jointsPerFinger], Posed& out);

// The palm's vertices (data::palmVertices) with the thenar following the metacarpal.
[[nodiscard]] glm::vec3 palmVertex(const Posed& posed, int i);

// Skinning matrices (3x4 row-major, as bonepose_t) for progs/hand_rig.md5mesh's joints.
void skin(const Posed& posed, float out[data::numJoints * 12]);

// Whether `model` is the rig with the joints these tables expect (else the six models are drawn).
[[nodiscard]] bool usable(qmodel_t* model);

} // namespace qvr::handrig
