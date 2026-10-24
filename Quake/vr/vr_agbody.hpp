// vr_agbody.hpp -- armed humanoids' body proxies (ROUND22.md, "Armed humanoids in motion: the glitch finder and a
// robust solve"): the torso, the pelvis and the head of an armed rig as ellipsoidal capsules (each fitted once a model
// to its bone's vertices in the rig's rest pose), posed each frame on the bones as drawn. The arms' solve (vr_agarms.cpp)
// keeps its hands and elbows out of them; the glitch finder (vr_agglitch.cpp) measures how far they go in.

#pragma once

#include "vr_ragdoll.hpp"
#include "vr_rigpose.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace qvr::agbody
{

inline constexpr int maxProxies = 3; // the torso, the pelvis, the head

enum Part : int
{
    Torso = 0,
    Pelvis = 1,
    Head = 2,
};

// A proxy: a segment (rest space, model units) swept by an ellipsoid of radii `radii` (its rest axes: x ahead, y left,
// z up), on bone `bone`.
struct Proxy
{
    int bone{-1};
    glm::vec3 a{0.f}, b{0.f};
    glm::vec3 radii{1.f}; // (on each axis's positive side)
    glm::vec3 back{1.f};  // (... on its negative side)
};

struct Proxies
{
    const qmodel_t* model{nullptr};
    bool ok{false};
    int n{0};
    Proxy p[maxProxies];
};

// The model's proxies (its rig's chest, the chest's parent, its head), fitted once. Null: none.
[[nodiscard]] const Proxies* of(const qmodel_t* model, const ragdoll::Rig& rig, int chest);

// The proxies now: each one's segment and turn (model space) on `posed` (rest -> now, a bone each; `kick` on top, or null).
struct Posed
{
    int n{0};
    glm::vec3 a[maxProxies], b[maxProxies];
    glm::quat rot[maxProxies];
    glm::vec3 radii[maxProxies], back[maxProxies];
};
[[nodiscard]] Posed pose(const Proxies& px, const rigpose::Rigid* posed, const rigpose::Rigid* kick);

// How far `p` is inside the proxies (units, the most of them: > 0 inside, its distance out to the surface along its line
// from the axis; <= 0 outside), `margin` added to every radius. `push` (if given): the move that takes it out (zero when
// outside); `part` the deepest's.
float depth(const Posed& q, const glm::vec3& p, float margin, glm::vec3* push = nullptr, int* part = nullptr);

// ... with the segment from `p0` to `p1` sampled (its `steps` + 1 points): the deepest.
float segmentDepth(const Posed& q, const glm::vec3& p0, const glm::vec3& p1, int steps, float margin);

// vr_agarms_info: the proxies fitted so far.
void print();

// The procedural upper body (vr_agrunt_upper; ROUND22.md, "Armed humanoids in motion"): the chest held on the pelvis as
// the rest pose has it (the frames' own chest and shoulder motion in their shots, reloads, pains and runs, which the posed
// arms don't follow, taken out; the legs and hips still the frames'), breathing, eased in and out (`keepFrames`: the
// frames' chest, a pickup's crouch). Once a frame for entity `ent` on `posed` (its bones as the frames draw them): the
// turn to lay on every bone on the chest (on the left: rest -> now becomes corr * posed). False: none (off, no pelvis).
bool upper(int ent, const ragdoll::Rig& rig, int chest, const rigpose::Rigid* posed, bool keepFrames, rigpose::Rigid& corr);
// ... the one laid this frame (the drawing's bone turns read it). False: none.
bool upperNow(int ent, rigpose::Rigid& corr);

// The Pose Studio's Live tape (vr_poselive.hpp): `ent`'s state here (its eases, its last frame's) as bytes, its length
// returned (0: none); and put back (a length of 0: dropped).
inline constexpr int stateBytes = 256;
int saveState(int ent, unsigned char* out);
void loadState(int ent, const unsigned char* in, int len);

} // namespace qvr::agbody
