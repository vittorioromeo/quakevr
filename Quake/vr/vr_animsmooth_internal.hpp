#pragma once

// vr_animsmooth_internal.hpp -- what vr_animsmooth.cpp (see vr_animsmooth.hpp) shares with its diagnostics,
// vr_animsmooth_diag.cpp. Internal: included by those two alone. What was the file's anonymous namespace and both use
// is namespace animsmooth::detail: its types and variables here (each variable one object: inline), its functions
// declared here and defined in the file that owns them.

#include "vr_animsmooth.hpp"
#include "vr_hitreact.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_mdldequant.hpp"
#include "vr_mem.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rigpose.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdlib.h>
#include <string.h>

namespace qvr::animsmooth::detail
{

// A model's animation data for the smoothing, made when it is first drawn smoothed.
struct SmoothModel
{
    int numFrames{0};
    int numPoses{0};
    za::Vector<int16_t> next;        // per frame: the frame its animation goes on to by the names (-1: none known, -2: a framegroup)
    za::Vector<int16_t> prev;        // ... and comes from (vr_anim_smooth_pin's default first key)
    za::Vector<int16_t> seen;        // ... the frame seen after it most (-1 none): where the names don't say (a loop's end)
    za::Vector<uint8_t> votes;       // ... how sure that is (each time it is seen again: up, another: down, replaced at 0)
    za::Vector<uint8_t> namedNext; // each numbered successor is reliable; anonymous frames learn their loops
    za::Vector<int16_t> frameOfPose; // per pose: its frame (-1: a framegroup's)
    // B: the rig, if it has one (and it fits the model).
    bool rigTried{false};
    const ragdoll::Rig* rig{nullptr};
    float rigError{-1.f};             // the vr_anim_smooth_rig_error the data below was made with
    za::Vector<uint32_t> unfit;       // per pose: the bones (bits) fitted worse than rigError, or folded away, in it
    // per VBO vertex, restStride vec4s: its rest place (units), its bone + half its share of the swing; its rebuild place
    // (the mean of its places on its bone over the poses that bone fits: units, pose 0's frame) and share; its normal so
    za::Vector<glm::vec4> rest;
    glm::vec3 invScale{1.f};          // the poses' bytes per unit
    glm::vec3 origin{0.f};            // the poses' byte 0 (units)
    int swungVerts{0}, fadedVerts{0}; // vertices swung fully, partly (vr_anim_smooth_info)
    // The rebuild (vr_anim_smooth_rebuild): made with this vr_anim_smooth_rebuild_error, its vertices rebuilt fully,
    // partly, their fits' rms (median, 90th percentile, most: units); off for the model if too few are.
    float rebuildError{-1.f};
    int rebuildDequant{-1};                // whether the poses were dequantised (vr_mdl_dequant) when it was made
    za::Vector<rigpose::Rigid> rebuildKeys; // [pose * numBones + bone]: the bones refitted to the drawn (dequantised) poses
    int rebuiltVerts{0}, rebuildFaded{0};
    float rebuildMedian{0.f}, rebuildP90{0.f}, rebuildMost{0.f};
    bool rebuildOk{false};
    bool restUploaded{false};         // `rest` is in the model's restBuffers buffer (VR_AnimSmoothBind)
    bool armSkin{false};              // its rest data has arm skin rows (vr_agarms): each instance's kicks then carry its arm rows
    bool legSkin{false};              // ... and leg skin rows (vr_aglegs): its leg rows after the arm rows
};

[[nodiscard]] inline za::SizeT heldBytes(const SmoothModel& m)
{
    return mem::heldBytes(m.next) + mem::heldBytes(m.prev) + mem::heldBytes(m.seen) + mem::heldBytes(m.votes) + mem::heldBytes(m.frameOfPose) +
           mem::heldBytes(m.namedNext) + mem::heldBytes(m.unfit) + mem::heldBytes(m.rest) + mem::heldBytes(m.rebuildKeys);
}

// vr_anim_smooth_pin: an entity drawn between two frames at a fixed blend (the comparison strips: ROUND22.md), its
// spline's other two keys given or by the names. The renderer's thread.
struct Pin
{
    int ent{-1};
    int frame[4]{0, 0, 0, 0};
    float blend{0.f};
};

// vr_anim_smooth_trace: one entity's drawn shape followed frame by frame (in its model's space: its animation alone), how
// much each vertex's speed changes from frame to frame (the second difference, units), for r_lerpmodels 1 and 3 to
// compare (vr_fixed_frames 1: even frames). The renderer's thread.
struct Trace
{
    int ent{-1};
    int framesLeft{0};
    double lastTime{-1.0};
    int filled{0};
    za::Vector<glm::vec3> now, prev, prev2;
    za::Vector<float> accel; // per frame: the vertices' mean
    za::Vector<float> peak;  // ... their most
    float joltLog{0.f};      // > 0: each frame one vertex's change passes, printed with the keys of it and the two before
    struct Keys
    {
        double time{0.0};
        int frame{-1};
        int poses[4]{-1, -1, -1, -1};
        float blend{0.f};
        float w[4]{0.f, 0.f, 0.f, 0.f};
    };
    Keys keys[3]; // [0] this frame's, [1] the one before, [2] ...
};
[[nodiscard]] inline za::SizeT heldBytes(const Trace& t) // (vr_mem.hpp)
{
    return mem::heldBytes(t.now) + mem::heldBytes(t.prev) + mem::heldBytes(t.prev2) + mem::heldBytes(t.accel) +
           mem::heldBytes(t.peak);
}

// Both name an entity by its number: off at a map change (another map's entity has it).
struct SmoothDebug
{
    Pin pin;
    Trace trace;
    auto members() { return mem::list(pin, trace); }
};
inline mem::Cache<SmoothDebug> smoothDebug{"anim smooth debug", mem::MapChange};
inline Pin& pin = smoothDebug.pin;
inline Trace& trace = smoothDebug.trace;

// The model's animation data (made when first needed).
[[nodiscard]] SmoothModel& modelData(const qmodel_t* model, const aliashdr_t* hdr);

// The models' data (their slots: a game dir change; a model reload makes the VR models again).
struct SmoothCache
{
    ankerl::unordered_dense::map<const qmodel_t*, za::UniquePtr<SmoothModel>> models;
    auto members() { return mem::list(models); }
};
inline mem::Cache<SmoothCache> smoothCache{"anim smooth", mem::GameDirChange | mem::ModelReload};

// The splines worked out since the last vr_anim_smooth_info (an entity's each frame it is drawn mid-lerp): with a key
// before (the lerp history), a key after (a frame expected next), limbs swung. The renderer's thread.
struct Stats
{
    int splines{0}, history{0}, ahead{0}, swung{0};
    int rebuilt{0};          // ... instances drawn rebuilt (vr_anim_smooth_rebuild), worked out anew
    int calls{0};            // VR_AnimSmooth's calls, and their time (s)
    double seconds{0.0};
};
inline Stats stats;

inline constexpr int restStride = 4;           // SmoothModel::rest's vec4s per vertex (the shader's SmoothRest; the fourth: an armed humanoid's arm skin, vr_agarms.cpp)

inline constexpr float pinSpan = 0.1f; // s: a pinned spline's frames' time each

bool rigData(SmoothModel& m, qmodel_t* model, const aliashdr_t* hdr);
[[nodiscard]] int framePose(const aliashdr_t* hdr, int frame);

inline constexpr float maxTurn = 0.2588190f; // cos(150 / 2 degrees): a bone turning further between the lerp's poses isn't swung
bool swings(const SmoothModel& m, const int poses[4], const rigpose::Spline& s, glm::vec4* out, int stride = 3,
    rigpose::Rigid* nowOut = nullptr);

} // namespace qvr::animsmooth::detail
