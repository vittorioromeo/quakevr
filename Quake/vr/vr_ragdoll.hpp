// vr_ragdoll.hpp -- ragdolls (experimental, the grunt only; ROUND21.md, "Ragdolls"): a dead monster's body as a few
// jointed Box3D bodies (vr_box3d.cpp, "Ragdolls") with its mesh skinned to them.
//
// A Quake .mdl has no skeleton: only each frame's vertex positions. The rig is derived from them when it is first
// needed (rigFor), from the model's own file (nothing of it is written anywhere):
// - The vertices welded (the same place in every pose: the UV seams' copies) give the mesh's pieces; a piece that leaves
//   the body in the animations (the grunt's shotgun, which falls out of his hands as he dies) is a loose bone of its own.
// - The body's vertices are clustered by how they move: those that move together, rigidly, in all the poses (K-means of
//   rigid transforms, after James and Twigg, "Skinning mesh animations", 2005): each cluster's rigid transform per pose
//   (the best fit: Horn's quaternion method), each vertex to the neighbouring cluster whose transforms carry it best.
// - The clusters become the bones of the model's seed table (by the nearest seed point: pelvis, chest, head, upper and
//   lower arms and legs), then each vertex goes once more to the neighbouring bone that carries it best.
// - The joints (their places, kinds and limits) and the bones' capsules are the seed table's, in the rest pose's space.
// Each bone then has its rigid transform in every pose: the rest pose (0) to pose p.
//
// Drawn: a model made in memory from the .mdl ("<name>#rag", VR_SyntheticModel): its rest pose as a skeletal model
// (one bone per vertex, the bones' bind pose the rest pose itself), drawn through the renderer's skeletal path with
// the ragdoll's bodies as the bones' matrices (VR_AliasBonePoses). The client swaps the corpse's model for it while it
// draws a frame (a listen server's ragdolls: the server's bodies are read directly).

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"

namespace qvr::ragdoll
{

inline constexpr int maxBones = 16;

enum class Joint : uint8_t
{
    Root,  // the pelvis: no joint
    Ball,  // a cone and a twist limit (spherical)
    Hinge, // one axis, a range (revolute): knees, elbows
    Loose  // its own body, joined to nothing (the grunt's shotgun)
};

struct Bone
{
    char name[16]{};
    int parent{-1};
    Joint joint{Joint::Root};
    // In the rest pose's model space (units): the joint with the parent (the root: its middle) and the far end.
    glm::vec3 pivot{0.f}, end{0.f};
    glm::vec3 hinge{0.f, 1.f, 0.f}; // Hinge: its axis, bending it turns the bone that way (rest space)
    float cone{0.f}, twist{0.f};    // Ball: the cone's half angle about the bone's rest direction, the twist either way (rad)
    float lower{0.f}, upper{0.f};   // Hinge: its range from the rest pose (rad)
    float capsule{0.f};             // a capsule from the pivot to the end of this radius (units) besides its hull; 0: none
    za::Vector<glm::vec3> points;   // its vertices in the rest pose, each place once (its hull's)
};

struct Rig
{
    const qmodel_t* model{nullptr};
    int numBones{0};
    Bone bones[maxBones];
    int numVerts{0};                // the .mdl's (hdr->numverts)
    za::Vector<uint8_t> vertBone;   // each .mdl vertex's bone
    int numPoses{0};
    za::Vector<glm::quat> poseRot;  // [pose * numBones + bone]: rest -> pose (model space, units)
    za::Vector<glm::vec3> posePos;
    za::Vector<uint8_t> poseHidden; // [pose * numBones + bone]: all its vertices at one point in that pose
    // The death animations' frames (first, last), from the seed table.
    int deaths{0};
    int deathFirst[4]{}, deathLast[4]{};
    // How the derivation went (vr_ragdoll_info).
    float clusterRms{0.f}, boneRms{0.f};
    double deriveMs{0.0};
};

// Whether `model` has a seed table (the grunt's; vr_ragdoll 1): cheap, by its name.
[[nodiscard]] bool eligible(const qmodel_t* model);

// The rig of `model`, derived on first use (tens of milliseconds) and kept: nullptr if it has none or the derivation
// failed (the model isn't the one the seed table was made for).
[[nodiscard]] const Rig* rigFor(qmodel_t* model);

// Bone `b`'s transform from the rest pose to pose `pose` (the model's space, units): p' = rot * p + pos.
void bonePose(const Rig& rig, int pose, int b, glm::quat& rot, glm::vec3& pos);

// Whether bone `b`'s vertices are all at one point in pose `pose` (hidden in that frame: vr_monstermods.cpp's dropped
// guns).
[[nodiscard]] bool collapsed(const Rig& rig, int pose, int b);

// The pose a frame of the model shows (a framegroup's first).
[[nodiscard]] int poseOfFrame(const qmodel_t* model, int frame);

// Where `frame` is in the death animation it belongs to: 0 its first frame .. 1 its last; -1 not a death frame.
[[nodiscard]] float deathProgress(const Rig& rig, int frame);

// The server's ragdolls as the client draws them (a listen server's: vr_box3d.cpp publishes after each step). Each bone's
// body in the world: p_world = rot * (scale * p_rest) + pos (units); the bones from `bodies` on have none (hidden).
void publish(int num, const Rig* rig, int bodies, const glm::quat* rot, const glm::vec3* pos, float scale);
void unpublish(int num);
void unpublishAll();

// The client, around a frame's drawing: the corpses that are ragdolls drawn with their skinned model (CL_RelinkEntities'
// end), and their own models put back before the server's messages are read (CL_ReadFromServer).
void swapModels();
void restoreModels();

// The model a swapped entity (a ragdoll drawn with its skinned model) has of its own (the .mdl: its triangles, its skin's
// layout); null if it isn't one.
[[nodiscard]] const qmodel_t* sourceModel(const entity_t* e);

// The skinning matrices (3x4 rows) of a swapped entity, its bone count; 0 if it isn't one.
[[nodiscard]] int bonePoses(const entity_t* e, const float** matrices);
// Its drawn model matrix (VR_AliasPostTransform): a translation to where its bones are given from; false if it isn't one.
[[nodiscard]] bool drawMatrix(const entity_t* e, float matrix[16]);

// A ragdoll's mesh as drawn now, in the world (units): its .mdl vertices skinned to its bodies (rigid: one bone each),
// and their normals (the rest pose's turned with them) if `normals`. False if edict `num` has no ragdoll published.
bool skinnedVertices(int num, za::Vector<glm::vec3>& out, za::Vector<glm::vec3>* normals = nullptr);

// vr_ragdoll_info: the rig of the nearest dead monster's model (or the grunt's), its bones and how it was derived.
void info_f();

} // namespace qvr::ragdoll
