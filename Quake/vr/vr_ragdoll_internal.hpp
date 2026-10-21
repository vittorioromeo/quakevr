#pragma once

// vr_ragdoll_internal.hpp -- what vr_ragdoll.cpp (see vr_ragdoll.hpp) shares with its diagnostics, vr_ragdoll_diag.cpp.
// Internal: included by those two alone. What was the file's anonymous namespace and both use is namespace
// ragdoll::detail: its types and variables here (each variable one object: inline), its functions declared here
// and defined in the file that owns them.

#include "vr_modelmetadata.hpp"
#include "vr_ragdoll.hpp"
#include "vr_tracy.h"
#include "vr_api_render.h"
#include "vr_cvars.hpp"
#include "vr_box3d.hpp"
#include "vr_decals.hpp"
#include "vr_diskcache.hpp"
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

namespace qvr::ragdoll::detail
{

struct Published
{
    const Rig* rig{nullptr};
    int bodies{0}; // the bones from this on are hidden
    uint32_t cut{0}; // the bones cut off (its head: ROUND21.md, "Decapitation"), drawn at the neck
    bool bleeds{true}; // its limbs flung fast leave blood trails (publish's `bleeds`: not an unwounded live one's)
    float scale{1.f};
    bool drawn{false}; // the client has drawn it (its first frame: compared with the animated mesh, vr_debug_ragdoll)
    qmodel_t* skinned{nullptr}; // its skinned model as last found (skinnedOf: checked before each use, found again if stale)
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

inline mem::Cache<DrawState> draw{"ragdolls drawn", mem::MapChange};

inline constexpr float deg = 0.017453292f;

void drawnPose(int num);
void recordMotion(); // vr_drawn_motion_test's frame (vr_ragdoll_diag.cpp)

} // namespace qvr::ragdoll::detail
