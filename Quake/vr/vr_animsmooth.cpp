// vr_animsmooth.cpp -- smoother monster animations (r_lerpmodels 3; ROUND22.md, "Smoother animations"), drawn by the
// client only: the server's frames, their times and every gameplay test stay as they are.
//
// Quake's frame lerp (r_lerpmodels 1, FitzQuake's) moves each vertex in a straight line from the last pose to the new one
// over the frame's tenth of a second: a limb swinging through a big angle shrinks mid-swing (its chord), and the speed
// changes at once at every frame. Here, between the same two poses:
// - A, the timing: each vertex goes along a cubic spline through four poses, the lerp's two, the one before them (the lerp
//   that ended where this one starts: entity_t's animpose0, R_AnimHistoryPush) and the one expected after (the next frame
//   of its animation by the frames' names, or the one seen after it last time: a loop's first), its tangents the
//   non-uniform Catmull-Rom ones by the frames' times (the speed carried through each frame; vr_anim_smooth_tension 1
//   eases to rest at each). The four poses' weights are the instance's (InstanceData's Smooth, SmoothW).
// - B, the limbs (a monster with a ragdoll rig: vr_ragdoll.cpp's): each bone's rigid motion between the poses (its fit to
//   each frame, Rig::poseRot / posePos) turned along the same spline in rotation, its joint carried by its parent, less
//   the chord that motion's spline would draw (the same weights): a swing (3x4, per bone and instance) added to each
//   vertex's place in its bone's rest pose. A vertex then goes round its joint at full length, its own residual from the
//   fit blended along the spline. A bone fitted badly in any of the four poses (vr_anim_smooth_rig_error, or folded away:
//   the grunt's dropped gun) or turning more than 150 degrees isn't swung; a vertex far off its bone swings less.
// At a pose itself (the lerp's start, its end, a frame held) nothing of this is drawn: the instance is drawn as
// r_lerpmodels 1 draws it, its vertices exactly the frame's.

#include "vr_animsmooth.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
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

using namespace qvr;

namespace qvr::animsmooth
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
    bool namesTell{false};           // the frames' names tell its animations apart (not the shambler's: "From ASE" all)
    za::Vector<int16_t> frameOfPose; // per pose: its frame (-1: a framegroup's)
    // B: the rig, if it has one (and it fits the model).
    bool rigTried{false};
    const ragdoll::Rig* rig{nullptr};
    float rigError{-1.f};             // the vr_anim_smooth_rig_error the data below was made with
    za::Vector<uint32_t> unfit;       // per pose: the bones (bits) fitted worse than rigError, or folded away, in it
    za::Vector<glm::vec4> rest;       // per VBO vertex: its rest place (units), its bone + half its share of the swing
    glm::vec3 invScale{1.f};          // the poses' bytes per unit
    int swungVerts{0}, fadedVerts{0}; // vertices swung fully, partly (vr_anim_smooth_info)
    // The rest data's upload this frame (VR_AnimSmoothBind).
    unsigned restSerial{0};
    GLuint restBuf{0};
    GLbyte* restOfs{nullptr};
};

[[nodiscard]] za::SizeT heldBytes(const SmoothModel& m)
{
    return mem::heldBytes(m.next) + mem::heldBytes(m.prev) + mem::heldBytes(m.seen) + mem::heldBytes(m.votes) + mem::heldBytes(m.frameOfPose) +
           mem::heldBytes(m.unfit) + mem::heldBytes(m.rest);
}

} // namespace qvr::animsmooth

namespace
{

using animsmooth::SmoothModel;

// The models' data (their slots: a game dir change; a model reload makes the VR models again).
struct SmoothCache
{
    ankerl::unordered_dense::map<const qmodel_t*, za::UniquePtr<SmoothModel>> models;
    auto members() { return mem::list(models); }
};
mem::Cache<SmoothCache> smoothCache{"anim smooth", mem::GameDirChange | mem::ModelReload};

// The swings of the instances in the batch being filled (channel 0: R_DrawAliasModel_Real's batches; 1: a wound paint's
// one instance), 3 rows per bone, bound with it (VR_AnimSmoothBind). Filled from a batch's first instance on (the
// renderer's thread); meaningless past the draw.
struct SmoothBatches
{
    za::Vector<glm::vec4> swing0;
    za::Vector<glm::vec4> swing1;
    auto members() { return mem::list(swing0, swing1); }
};
mem::Scratch<SmoothBatches> batches{"anim smooth"};

[[nodiscard]] za::Vector<glm::vec4>& swingsOf(int channel)
{
    return channel ? batches.swing1 : batches.swing0;
}

// vr_anim_smooth_pin: an entity drawn between two frames at a fixed blend (the comparison strips: ROUND22.md), its
// spline's other two keys given or by the names. The renderer's thread.
struct Pin
{
    int ent{-1};
    int frame[4]{0, 0, 0, 0};
    float blend{0.f};
};
Pin pin;

// The splines worked out since the last vr_anim_smooth_info (an entity's each frame it is drawn mid-lerp): with a key
// before (the lerp history), a key after (a frame expected next), limbs swung. The renderer's thread.
struct Stats
{
    int splines{0}, history{0}, ahead{0}, swung{0};
};
Stats stats;

// What an entity's spline was last worked out for, and what came of it: drawn again so (the other eye, each shadow
// map's light, the next frame paused) it is copied. By the entity (the renderer's thread; a map change frees it).
struct Key
{
    int poses[4]{-1, -1, -1, -1};
    float span[3]{0.f, 0.f, 0.f};
    float blend{-1.f};
    float tension{-1.f};
    float rigError{-2.f}; // (-1: not swung)
    [[nodiscard]] bool operator==(const Key& o) const
    {
        return !memcmp(poses, o.poses, sizeof(poses)) && !memcmp(span, o.span, sizeof(span)) && blend == o.blend &&
               tension == o.tension && rigError == o.rigError;
    }
};
struct Memo
{
    Key key;
    float learnt{-1.f}; // the lerp (its start) whose frames were learnt (SmoothModel::seen)
    float aheadStart{-1.f}; // the lerp (its start, its pose) whose frame expected after is `ahead`
    int aheadFrom{-1};
    int ahead{0};
    rigpose::Spline spline;
    bool swung{false};
    int numRows{0};
    glm::vec4 rows[3 * ragdoll::maxBones];
};
struct SmoothMemos
{
    ankerl::unordered_dense::map<const entity_t*, Memo> entities;
    auto members() { return mem::list(entities); }
};
mem::Cache<SmoothMemos> memos{"anim smooth entities", mem::MapChange};

constexpr float pinSpan = 0.1f; // s: a pinned spline's frames' time each
constexpr float maxTurn = 0.2588190f; // cos(150 / 2 degrees): a bone turning further between the lerp's poses isn't swung

// A frame's name split into its animation's name and its number in it ("run12": "run", 12; none: -1).
void splitName(const char* name, char* prefix, int& number)
{
    int n = static_cast<int>(strnlen(name, 16));
    int digits = n;
    while(digits > 0 && name[digits - 1] >= '0' && name[digits - 1] <= '9')
    {
        digits--;
    }
    memcpy(prefix, name, static_cast<size_t>(digits));
    prefix[digits] = '\0';
    number = digits < n ? atoi(name + digits) : -1;
}

[[nodiscard]] SmoothModel& modelData(const qmodel_t* model, const aliashdr_t* hdr)
{
    za::UniquePtr<SmoothModel>& slot = smoothCache.models[model];
    if(slot && slot->numFrames == hdr->numframes && slot->numPoses == hdr->numposes)
    {
        return *slot;
    }
    slot = za::makeUnique<SmoothModel>();
    SmoothModel& m = *slot;
    m.numFrames = hdr->numframes;
    m.numPoses = hdr->numposes;
    m.next.resize(static_cast<za::SizeT>(m.numFrames), -1);
    m.prev.resize(static_cast<za::SizeT>(m.numFrames), -1);
    m.seen.resize(static_cast<za::SizeT>(m.numFrames), -1);
    m.votes.resize(static_cast<za::SizeT>(m.numFrames), 0);
    m.frameOfPose.resize(static_cast<za::SizeT>(m.numPoses), -1);
    for(int f = 0; f < m.numFrames; f++)
    {
        const maliasframedesc_t& d = hdr->frames[f];
        if(d.numposes > 1)
        {
            m.next[static_cast<za::SizeT>(f)] = -2;
            continue;
        }
        if(d.firstpose >= 0 && d.firstpose < m.numPoses)
        {
            m.frameOfPose[static_cast<za::SizeT>(d.firstpose)] = static_cast<int16_t>(f);
        }
        if(f + 1 >= m.numFrames || hdr->frames[f + 1].numposes > 1)
        {
            continue;
        }
        char a[17], b[17];
        int na = -1, nb = -1;
        splitName(d.name, a, na);
        splitName(hdr->frames[f + 1].name, b, nb);
        m.namesTell = m.namesTell || strcmp(a, b);
        // The same animation, numbered on (or unnamed, or unnumbered: the shambler's: the file's order)
        const bool on = !strcmp(a, b) && (na < 0 || nb < 0 || nb == na + 1);
        if(on)
        {
            m.next[static_cast<za::SizeT>(f)] = static_cast<int16_t>(f + 1);
            m.prev[static_cast<za::SizeT>(f + 1)] = static_cast<int16_t>(f);
        }
    }
    // A rigged monster's deaths end where its seed table says (its last frame held: nothing after it)
    if(const ragdoll::Rig* rig = ragdoll::eligible(model) ? ragdoll::rigFor(const_cast<qmodel_t*>(model)) : nullptr)
    {
        for(int i = 0; i < rig->deaths; i++)
        {
            if(rig->deathLast[i] >= 0 && rig->deathLast[i] < m.numFrames)
            {
                m.next[static_cast<za::SizeT>(rig->deathLast[i])] = -1;
            }
            if(rig->deathFirst[i] >= 0 && rig->deathFirst[i] < m.numFrames)
            {
                m.prev[static_cast<za::SizeT>(rig->deathFirst[i])] = -1;
            }
        }
    }
    return m;
}

// The model's rig data (B), made when first needed and again when vr_anim_smooth_rig_error changes; false: no rig.
bool rigData(SmoothModel& m, qmodel_t* model, const aliashdr_t* hdr)
{
    const float error = za::max(vr_anim_smooth_rig_error.value, 0.01f);
    if(!m.rigTried)
    {
        m.rigTried = true;
        const ragdoll::Rig* rig = ragdoll::rigFor(model);
        if(rig && rig->numVerts == hdr->numverts && rig->numPoses == hdr->numposes && rig->numBones <= ragdoll::maxBones &&
            hdr->poseverttype == aliashdr_t::PV_QUAKE1 && !Mod_NextSurface(const_cast<aliashdr_t*>(hdr)))
        {
            m.rig = rig;
        }
    }
    if(!m.rig)
    {
        return false;
    }
    if(m.rigError == error)
    {
        return true;
    }
    const ragdoll::Rig& rig = *m.rig;
    m.rigError = error;
    m.invScale = glm::vec3{1.f / hdr->scale[0], 1.f / hdr->scale[1], 1.f / hdr->scale[2]};
    const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const glm::vec3 scale{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
    const glm::vec3 origin{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
    const auto at = [&](int pose, int v) {
        const trivertx_t& t = base[static_cast<za::SizeT>(pose) * static_cast<za::SizeT>(hdr->numverts) + static_cast<za::SizeT>(v)];
        return glm::vec3{t.v[0], t.v[1], t.v[2]} * scale + origin;
    };
    // Each bone's fit in each pose, each vertex's over the poses (rms, units)
    za::Vector<float> boneSum(static_cast<za::SizeT>(rig.numBones * m.numPoses), 0.f);
    za::Vector<int> boneCount(static_cast<za::SizeT>(rig.numBones), 0);
    za::Vector<float> vertSum(static_cast<za::SizeT>(hdr->numverts), 0.f);
    za::Vector<int> vertCount(static_cast<za::SizeT>(hdr->numverts), 0);
    for(int v = 0; v < hdr->numverts; v++)
    {
        boneCount[rig.vertBone[static_cast<za::SizeT>(v)]]++;
    }
    for(int p = 0; p < m.numPoses; p++)
    {
        rigpose::Rigid keys[ragdoll::maxBones];
        for(int b = 0; b < rig.numBones; b++)
        {
            keys[b] = rigpose::key(rig, p, b);
        }
        for(int v = 0; v < hdr->numverts; v++)
        {
            const int b = rig.vertBone[static_cast<za::SizeT>(v)];
            const glm::vec3 d = at(p, v) - rigpose::apply(keys[b], at(0, v));
            const float e2 = glm::dot(d, d);
            boneSum[static_cast<za::SizeT>(p * rig.numBones + b)] += e2;
            if(!ragdoll::collapsed(rig, p, b))
            {
                vertSum[static_cast<za::SizeT>(v)] += e2;
                vertCount[static_cast<za::SizeT>(v)]++;
            }
        }
    }
    m.unfit.clear();
    m.unfit.resize(static_cast<za::SizeT>(m.numPoses), 0u);
    for(int p = 0; p < m.numPoses; p++)
    {
        for(int b = 0; b < rig.numBones; b++)
        {
            const float rms = za::sqrt(boneSum[static_cast<za::SizeT>(p * rig.numBones + b)] /
                                       static_cast<float>(za::max(boneCount[static_cast<za::SizeT>(b)], 1)));
            if(rms > error || ragdoll::collapsed(rig, p, b))
            {
                m.unfit[static_cast<za::SizeT>(p)] |= 1u << b;
            }
        }
    }
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
    m.rest.clear();
    m.rest.resize(static_cast<za::SizeT>(hdr->numverts_vbo));
    m.swungVerts = m.fadedVerts = 0;
    for(int i = 0; i < hdr->numverts_vbo; i++)
    {
        const int v = za::clamp(static_cast<int>(desc[i].vertindex), 0, hdr->numverts - 1);
        const float rms = za::sqrt(vertSum[static_cast<za::SizeT>(v)] / static_cast<float>(za::max(vertCount[static_cast<za::SizeT>(v)], 1)));
        const float share = za::clamp(2.f - rms / error, 0.f, 1.f);
        m.swungVerts += share >= 1.f ? 1 : 0;
        m.fadedVerts += share > 0.f && share < 1.f ? 1 : 0;
        m.rest[static_cast<za::SizeT>(i)] = glm::vec4{at(0, v), static_cast<float>(rig.vertBone[static_cast<za::SizeT>(v)]) + 0.5f * za::min(share, 0.999f)};
    }
    return true;
}

// The frame expected after the one showing pose `pose` (its pose): -1 none known.
[[nodiscard]] int nextPose(const SmoothModel& m, const aliashdr_t* hdr, int pose)
{
    const int frame = m.frameOfPose[static_cast<za::SizeT>(pose)];
    if(frame < 0)
    {
        // A framegroup's pose: the next of its cycle
        for(int f = 0; f < m.numFrames; f++)
        {
            const maliasframedesc_t& d = hdr->frames[f];
            if(d.numposes > 1 && pose >= d.firstpose && pose < d.firstpose + d.numposes)
            {
                return d.firstpose + (pose - d.firstpose + 1) % d.numposes;
            }
        }
        return -1;
    }
    int next = m.next[static_cast<za::SizeT>(frame)];
    const int seen = m.seen[static_cast<za::SizeT>(frame)];
    if(next == -1 || (!m.namesTell && seen >= 0))
    {
        next = seen;
    }
    return next >= 0 ? hdr->frames[next].firstpose : -1;
}

[[nodiscard]] bool pinned(const entity_t* e)
{
    return pin.ent >= 0 && pin.ent < cl.num_entities && e == &cl_entities[pin.ent];
}

[[nodiscard]] int framePose(const aliashdr_t* hdr, int frame)
{
    return hdr->frames[za::clamp(frame, 0, hdr->numframes - 1)].firstpose;
}

// The swings of `m`'s bones between poses `poses` along `s`, into `out` (3 rows a bone): false if none swings.
bool swings(const SmoothModel& m, const int poses[4], const rigpose::Spline& s, glm::vec4* out)
{
    const ragdoll::Rig& rig = *m.rig;
    uint32_t still = 0;
    for(int i = 0; i < 4; i++)
    {
        still |= m.unfit[static_cast<za::SizeT>(poses[i])];
    }
    rigpose::Rigid keys[4][ragdoll::maxBones];
    for(int i = 0; i < 4; i++)
    {
        for(int b = 0; b < rig.numBones; b++)
        {
            keys[i][b] = rigpose::key(rig, poses[i], b);
        }
    }
    for(int b = 0; b < rig.numBones; b++)
    {
        if(za::abs(glm::dot(keys[1][b].rot, keys[2][b].rot)) < maxTurn)
        {
            still |= 1u << b;
        }
    }
    if((still & ((1u << rig.numBones) - 1u)) == ((1u << rig.numBones) - 1u))
    {
        return false;
    }
    rigpose::Rigid now[ragdoll::maxBones];
    rigpose::along(rig, poses, s, still, now);
    for(int b = 0; b < rig.numBones; b++)
    {
        glm::vec4* rows = out + 3 * b;
        if(still & (1u << b))
        {
            rows[0] = rows[1] = rows[2] = glm::vec4{0.f};
            continue;
        }
        // The bone's motion now less its keys' blend by the spline's weights (the chord): what the spline misses
        glm::mat3 r = glm::mat3_cast(now[b].rot);
        glm::vec3 t = now[b].pos;
        for(int i = 0; i < 4; i++)
        {
            r -= s.w[i] * glm::mat3_cast(keys[i][b].rot);
            t -= s.w[i] * keys[i][b].pos;
        }
        for(int row = 0; row < 3; row++)
        {
            rows[row] = glm::vec4{r[0][row], r[1][row], r[2][row], t[row]} * m.invScale[row];
        }
    }
    return true;
}

} // namespace

namespace
{

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
};
Trace trace;

void traceDraw(const entity_t* e, const aliashdr_t* hdr, int pose1, int pose2, float blend, int on, const int smooth[4],
    const float weights[4])
{
    if(trace.ent < 0 || e != &cl_entities[trace.ent] || cl.time == trace.lastTime || hdr->poseverttype != aliashdr_t::PV_QUAKE1)
    {
        return;
    }
    trace.lastTime = cl.time;
    const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
    const glm::vec3 scale{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
    const int count = hdr->numverts_vbo;
    const auto at = [&](int pose, int i) {
        const trivertx_t& t = base[static_cast<za::SizeT>(pose) * static_cast<za::SizeT>(hdr->numverts) + desc[i].vertindex];
        return glm::vec3{t.v[0], t.v[1], t.v[2]};
    };
    const auto mit = smoothCache.models.find(e->model);
    const SmoothModel* m = mit != smoothCache.models.end() ? mit->second.get() : nullptr;
    const glm::vec4* rows = on && smooth[2] > 0 ? batches.swing0.data() + (smooth[2] - 1) : nullptr;
    trace.prev2 = trace.prev;
    trace.prev = trace.now;
    trace.now.resize(static_cast<za::SizeT>(count));
    for(int i = 0; i < count; i++)
    {
        glm::vec3 p = glm::mix(at(pose1, i), at(pose2, i), pose1 == pose2 ? 0.f : blend);
        if(on)
        {
            p = weights[0] * at(smooth[0], i) + weights[1] * at(pose1, i) + weights[2] * at(pose2, i) + weights[3] * at(smooth[1], i);
            if(rows && m && static_cast<za::SizeT>(i) < m->rest.size())
            {
                const glm::vec4 r = m->rest[static_cast<za::SizeT>(i)];
                const float bone = za::floor(r.w);
                const glm::vec4* row = rows + 3 * static_cast<int>(bone);
                const glm::vec4 q{r.x, r.y, r.z, 1.f};
                p += (r.w - bone) * 2.f * glm::vec3{glm::dot(row[0], q), glm::dot(row[1], q), glm::dot(row[2], q)};
            }
        }
        trace.now[static_cast<za::SizeT>(i)] = p * scale;
    }
    if(++trace.filled >= 3 && trace.prev2.size() == trace.now.size())
    {
        float sum = 0.f, most = 0.f;
        for(za::SizeT i = 0; i < trace.now.size(); i++)
        {
            const glm::vec3 d = trace.now[i] - 2.f * trace.prev[i] + trace.prev2[i];
            const float a = za::sqrt(glm::dot(d, d));
            sum += a;
            most = za::max(most, a);
        }
        trace.accel.pushBack(sum / static_cast<float>(za::max<za::SizeT>(trace.now.size(), 1)));
        trace.peak.pushBack(most);
    }
    if(--trace.framesLeft > 0)
    {
        return;
    }
    // The frames' changes of speed, sorted: their median, 95th percentile and most (the jolts at the frames' ends)
    za::Vector<float> a = trace.accel, p = trace.peak;
    za::quickSort(a.begin(), a.end());
    za::quickSort(p.begin(), p.end());
    const auto pct = [](const za::Vector<float>& v, float f) {
        return v.empty() ? 0.f : v[static_cast<za::SizeT>(f * static_cast<float>(v.size() - 1))];
    };
    Con_Printf("animsmooth: trace ent %d (%s, r_lerpmodels %g): %d frames; speed change a frame, the vertices' mean: median "
               "%.3f, p95 %.3f, most %.3f; one vertex's most: p95 %.3f, most %.3f units\n", trace.ent, e->model->name,
        r_lerpmodels.value, static_cast<int>(a.size()), pct(a, 0.5f), pct(a, 0.95f), pct(a, 1.f), pct(p, 0.95f), pct(p, 1.f));
    trace = Trace{};
}

[[nodiscard]] int smoothInstance(const entity_t* e, const void* aliashdr, int pose1, int pose2, float blend, int zeroblend,
    int batchindex, int channel, int smooth[4], float weights[4])
{
    za::Vector<glm::vec4>& swing = swingsOf(channel);
    if(batchindex == 0)
    {
        swing.clear();
    }
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    if(r_lerpmodels.value != 3.f || !e->model || hdr->poseverttype == aliashdr_t::PV_IQM || pose1 == pose2 || !(blend > 0.f) ||
        !(blend < 1.f) || (cls.demoplayback && cls.demospeed < 0.f))
    {
        return 0;
    }
    if(pose1 < 0 || pose1 >= hdr->numposes || pose2 < 0 || pose2 >= hdr->numposes)
    {
        return 0;
    }
    SmoothModel& m = modelData(e->model, hdr);
    Memo& memo = memos.entities[e];

    // The four keys and their times
    int poses[4] = {pose1, pose1, pose2, pose2};
    float span[3] = {pinSpan, pinSpan, pinSpan};
    if(pinned(e))
    {
        poses[0] = framePose(hdr, pin.frame[0]);
        poses[3] = framePose(hdr, pin.frame[3]);
    }
    else
    {
        const int frame = za::clamp(e->frame, 0, hdr->numframes - 1);
        float d1 = (e->lerpflags & LERP_FINISH) && hdr->frames[frame].numposes == 1 ? R_FrameLerpFinish(e) - e->lerpstart : e->lerptime;
        d1 = d1 > 0.001f ? d1 : 0.1f;
        span[1] = span[2] = d1;
        span[0] = d1;
        if(e->animfrom == pose1 && e->animspan0 > 0.f && e->animpose0 >= 0 && e->animpose0 < hdr->numposes)
        {
            poses[0] = e->animpose0;
            span[0] = za::clamp(e->animspan0, d1 * 0.5f, d1 * 2.f);
        }
        // (learnt once a lerp: the frame seen after its first, where the names don't say; a vote against what was
        // seen before, an interruption, a pain, doesn't replace it at once)
        const int f1 = m.frameOfPose[static_cast<za::SizeT>(pose1)], f2 = m.frameOfPose[static_cast<za::SizeT>(pose2)];
        if(f1 >= 0 && f2 >= 0 && e->animfrom == pose1 && memo.learnt != e->lerpstart)
        {
            memo.learnt = e->lerpstart;
            int16_t& seen = m.seen[static_cast<za::SizeT>(f1)];
            uint8_t& votes = m.votes[static_cast<za::SizeT>(f1)];
            if(seen == f2)
            {
                votes = static_cast<uint8_t>(za::min(votes + 1, 3));
            }
            else if(votes > 0)
            {
                votes--;
            }
            else
            {
                seen = static_cast<int16_t>(f2);
                votes = 1;
            }
        }
        // (the frame expected after, kept for the lerp: what is learnt meanwhile changes the next one's, not this one's)
        if(memo.aheadStart != e->lerpstart || memo.aheadFrom != pose2)
        {
            const int next = nextPose(m, hdr, pose2);
            memo.aheadStart = e->lerpstart;
            memo.aheadFrom = pose2;
            memo.ahead = next >= 0 && next < hdr->numposes ? next : pose2;
        }
        poses[3] = memo.ahead;
    }
    const float tension = za::clamp(vr_anim_smooth_tension.value, 0.f, 1.f);
    const bool rigged = vr_anim_smooth_rig.value != 0.f && !zeroblend && rigData(m, e->model, hdr);
    const Key key{{poses[0], poses[1], poses[2], poses[3]}, {span[0], span[1], span[2]}, blend, tension, rigged ? m.rigError : -1.f};
    if(!(memo.key == key))
    {
        // Not drawn so yet (another view, another light: the same)
        memo.key = key;
        memo.spline = rigpose::spline(blend, span, tension);
        memo.swung = rigged && swings(m, poses, memo.spline, memo.rows);
        memo.numRows = rigged ? 3 * m.rig->numBones : 0;
        stats.splines++;
        stats.history += poses[0] != poses[1] ? 1 : 0;
        stats.ahead += poses[3] != poses[2] ? 1 : 0;
        stats.swung += memo.swung ? 1 : 0;
    }
    smooth[0] = poses[0];
    smooth[1] = poses[3];
    smooth[2] = 0;
    smooth[3] = 1;
    for(int i = 0; i < 4; i++)
    {
        weights[i] = memo.spline.w[i];
    }
    if(memo.swung)
    {
        // B: the limbs' swings
        const za::SizeT at = swing.size();
        swing.resize(at + static_cast<za::SizeT>(memo.numRows));
        memcpy(swing.data() + at, memo.rows, sizeof(glm::vec4) * static_cast<size_t>(memo.numRows));
        smooth[2] = 1 + static_cast<int>(at);
    }
    return 1;
}

} // namespace

extern "C" int VR_AnimSmooth(const entity_t* e, const void* aliashdr, int pose1, int pose2, float blend, int zeroblend,
    int batchindex, int channel, int smooth[4], float weights[4])
{
    const int on = smoothInstance(e, aliashdr, pose1, pose2, blend, zeroblend, batchindex, channel, smooth, weights);
    if(trace.ent >= 0 && channel == 0)
    {
        traceDraw(e, static_cast<const aliashdr_t*>(aliashdr), pose1, pose2, blend, on, smooth, weights);
    }
    return on;
}

extern "C" void VR_AnimSmoothBind(const entity_t* e, int channel)
{
    za::Vector<glm::vec4>& swing = swingsOf(channel);
    if(swing.empty() || !e || !e->model)
    {
        return;
    }
    const auto it = smoothCache.models.find(e->model);
    if(it == smoothCache.models.end() || !it->second || it->second->rest.empty())
    {
        return;
    }
    SmoothModel& m = *it->second;
    const GLsizeiptr restSize = static_cast<GLsizeiptr>(m.rest.size() * sizeof(glm::vec4));
    if(m.restSerial != gl_frameres_serial || !m.restBuf)
    {
        GL_Upload(GL_SHADER_STORAGE_BUFFER, m.rest.data(), static_cast<size_t>(restSize), &m.restBuf, &m.restOfs);
        m.restSerial = gl_frameres_serial;
    }
    GLuint swingBuf;
    GLbyte* swingOfs;
    const GLsizeiptr swingSize = static_cast<GLsizeiptr>(swing.size() * sizeof(glm::vec4));
    GL_Upload(GL_SHADER_STORAGE_BUFFER, swing.data(), static_cast<size_t>(swingSize), &swingBuf, &swingOfs);
    GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 8, m.restBuf, reinterpret_cast<GLintptr>(m.restOfs), restSize);
    GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 9, swingBuf, reinterpret_cast<GLintptr>(swingOfs), swingSize);
}

// R_SetupAliasFrame's end: a pinned entity's lerp (vr_anim_smooth_pin); false if it isn't the pinned one.
extern "C" int VR_AnimSmoothPinned(const entity_t* e, const void* aliashdr, short* pose1, short* pose2, float* blend)
{
    if(!pinned(e))
    {
        return 0;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    *pose1 = static_cast<short>(framePose(hdr, pin.frame[1]));
    *pose2 = static_cast<short>(framePose(hdr, pin.frame[2]));
    *blend = pin.blend;
    return 1;
}

namespace
{

// An animated model's entity: `which` its number, else the nearest to you whose model's name has `which` ("": any); -1 none.
[[nodiscard]] int pickEntity(const char* which)
{
    int ent = which[0] >= '0' && which[0] <= '9' ? atoi(which) : -1;
    if(ent < 0)
    {
        const glm::vec3 me = glm::make_vec3(cl_entities[cl.viewentity].origin);
        float best = 1e30f;
        for(int i = 1; i < cl.num_entities; i++)
        {
            const entity_t& e = cl_entities[i];
            if(i == cl.viewentity || !e.model || e.model->type != mod_alias ||
                static_cast<const aliashdr_t*>(Mod_Extradata(e.model))->numframes < 10 || !strstr(e.model->name, which))
            {
                continue;
            }
            const glm::vec3 d = glm::make_vec3(e.origin) - me;
            if(glm::dot(d, d) < best)
            {
                best = glm::dot(d, d);
                ent = i;
            }
        }
    }
    if(ent < 0 || ent >= cl.num_entities || !cl_entities[ent].model || cl_entities[ent].model->type != mod_alias)
    {
        return -1;
    }
    return ent;
}

// vr_anim_smooth_trace <frames> [<entity> | <model>]
void trace_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_anim_smooth_trace <frames> [<entity> | <model>]: the nearest monster's (or <entity>'s, or the nearest "
                   "whose model's name has <model>) drawn animation over the next <frames> frames: how much its vertices' "
                   "speed changes frame to frame (r_lerpmodels 1 against 3; vr_fixed_frames 1)\n");
        return;
    }
    trace = Trace{};
    trace.ent = pickEntity(Cmd_Argc() >= 3 ? Cmd_Argv(2) : "");
    trace.framesLeft = za::max(atoi(Cmd_Argv(1)), 3);
    if(trace.ent < 0)
    {
        Con_Printf("animsmooth: no model to trace\n");
    }
}

// vr_anim_smooth_pin off | <frame1> <frame2> <blend> [<frame0> <frame3> [<entity> | <model>]]
void pin_f()
{
    if(Cmd_Argc() >= 2 && !strcmp(Cmd_Argv(1), "off"))
    {
        pin = Pin{};
        Con_Printf("animsmooth: pin off\n");
        return;
    }
    if(Cmd_Argc() < 4)
    {
        Con_Printf("vr_anim_smooth_pin off | <frame1> <frame2> <blend> [<frame0> <frame3> [<entity> | <model>]]: the nearest "
                   "monster (or <entity>, or the nearest whose model's name has <model>) drawn between two frames at "
                   "<blend>, its spline's other keys <frame0> and <frame3> (-1 or left out: by its frames' names)\n");
        return;
    }
    const int ent = pickEntity(Cmd_Argc() >= 7 ? Cmd_Argv(6) : "");
    if(ent < 0)
    {
        Con_Printf("animsmooth: no model to pin\n");
        return;
    }
    const entity_t& e = cl_entities[ent];
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e.model));
    const SmoothModel& m = modelData(e.model, hdr);
    const auto clampFrame = [hdr](int f) { return za::clamp(f, 0, hdr->numframes - 1); };
    pin.ent = ent;
    pin.frame[1] = clampFrame(atoi(Cmd_Argv(1)));
    pin.frame[2] = clampFrame(atoi(Cmd_Argv(2)));
    pin.blend = static_cast<float>(atof(Cmd_Argv(3)));
    const int f0 = Cmd_Argc() >= 5 ? atoi(Cmd_Argv(4)) : -1, f3 = Cmd_Argc() >= 6 ? atoi(Cmd_Argv(5)) : -1;
    const int before = m.prev[static_cast<za::SizeT>(pin.frame[1])], after = m.next[static_cast<za::SizeT>(pin.frame[2])];
    pin.frame[0] = f0 >= 0 ? clampFrame(f0) : before >= 0 ? before : pin.frame[1];
    pin.frame[3] = f3 >= 0 ? clampFrame(f3) : after >= 0 ? after : pin.frame[2];
    Con_Printf("animsmooth: pinned ent %d (%s at %.0f %.0f %.0f) frames %d %d %d %d (%s to %s) at %.3f\n", ent, e.model->name, e.origin[0], e.origin[1], e.origin[2], pin.frame[0],
        pin.frame[1], pin.frame[2], pin.frame[3], hdr->frames[pin.frame[1]].name, hdr->frames[pin.frame[2]].name, pin.blend);
}

// vr_anim_smooth_info [model]: the models drawn smoothed, their predictions and rigs.
void info_f()
{
    Con_Printf("animsmooth: %d splines drawn since the last info: %d with the lerp before, %d with a frame expected after, "
               "%d with limbs swung\n", stats.splines, stats.history, stats.ahead, stats.swung);
    stats = Stats{};
    for(const auto& [model, mp] : smoothCache.models)
    {
        if(!mp || (Cmd_Argc() >= 2 && !strstr(model->name, Cmd_Argv(1))))
        {
            continue;
        }
        const SmoothModel& m = *mp;
        int named = 0, seen = 0, groups = 0;
        for(int f = 0; f < m.numFrames; f++)
        {
            named += m.next[static_cast<za::SizeT>(f)] >= 0 ? 1 : 0;
            groups += m.next[static_cast<za::SizeT>(f)] == -2 ? 1 : 0;
            seen += m.next[static_cast<za::SizeT>(f)] == -1 && m.seen[static_cast<za::SizeT>(f)] >= 0 ? 1 : 0;
        }
        Con_Printf("animsmooth: %s: %d frames (%d go on by name, %d learnt, %d framegroups), %d poses", model->name,
            m.numFrames, named, seen, groups, m.numPoses);
        if(Cmd_Argc() >= 3) // (and its frames: each one's name, then the frame expected after it)
        {
            const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
            Con_Printf("\n");
            for(int f = 0; f < m.numFrames; f++)
            {
                Con_Printf("animsmooth:   %d %s > %d%s", f, hdr->frames[f].name, m.next[static_cast<za::SizeT>(f)], f % 6 == 5 ? "\n" : ";");
            }
        }
        if(!m.rig)
        {
            Con_Printf(", no rig\n");
            continue;
        }
        Con_Printf(", rig %d bones (error %.2f): %d vertices swung fully, %d partly, of %d\n", m.rig->numBones, m.rigError,
            m.swungVerts, m.fadedVerts, static_cast<int>(m.rest.size()));
        for(int b = 0; b < m.rig->numBones; b++)
        {
            int unfit = 0;
            for(const uint32_t u : m.unfit)
            {
                unfit += static_cast<int>((u >> b) & 1u);
            }
            Con_Printf("animsmooth:   bone %2d %-10s unfit in %d of %d poses\n", b, m.rig->bones[b].name, unfit, m.numPoses);
        }
    }
}

// A bone's vertices' spread about their middle (rms, units): a limb's size, the same in every pose if it is rigid.
[[nodiscard]] float spread(const za::Vector<glm::vec3>& pts, const ragdoll::Rig& rig, const aliasmesh_t* desc, int count, int b)
{
    glm::vec3 mid{0.f};
    int n = 0;
    for(int i = 0; i < count; i++)
    {
        if(rig.vertBone[desc[i].vertindex] == b)
        {
            mid += pts[static_cast<za::SizeT>(i)];
            n++;
        }
    }
    if(n < 3)
    {
        return 0.f;
    }
    mid /= static_cast<float>(n);
    float sum = 0.f;
    for(int i = 0; i < count; i++)
    {
        if(rig.vertBone[desc[i].vertindex] == b)
        {
            const glm::vec3 d = pts[static_cast<za::SizeT>(i)] - mid;
            sum += glm::dot(d, d);
        }
    }
    return za::sqrt(sum / static_cast<float>(n));
}

// vr_anim_smooth_check [model ...]: each pair of frames that follow by name in each rigged model (default: those loaded),
// drawn as the shader draws them (positions in the poses' bytes): the spline's weights at the keys, how near each key the
// drawn shape is just off it, and how much each limb (a bone's vertices) shrinks mid-frame: Quake's lerp, A, A and B.
void check_f()
{
    za::Vector<qmodel_t*> models;
    for(int i = 1; i < Cmd_Argc(); i++)
    {
        if(qmodel_t* model = Mod_ForName(Cmd_Argv(i), false))
        {
            models.pushBack(model);
        }
    }
    if(models.empty())
    {
        for(int i = 1; i < MAX_MODELS && cl.model_precache[i]; i++)
        {
            if(cl.model_precache[i]->type == mod_alias && ragdoll::eligible(cl.model_precache[i]))
            {
                models.pushBack(cl.model_precache[i]);
            }
        }
    }
    const float tension = za::clamp(vr_anim_smooth_tension.value, 0.f, 1.f);
    const float zero[4] = {0.f, 1.f, 0.f, 0.f}, one[4] = {0.f, 0.f, 1.f, 0.f};
    const float spans[3] = {pinSpan, pinSpan, pinSpan};
    const rigpose::Spline s0 = rigpose::spline(0.f, spans, tension), s1 = rigpose::spline(1.f, spans, tension);
    Con_Printf("animsmooth: check: weights at the keys %s\n",
        !memcmp(s0.w, zero, sizeof(zero)) && !memcmp(s1.w, one, sizeof(one)) ? "exact" : "NOT EXACT");
    for(qmodel_t* model : models)
    {
        const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
        if(!hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1)
        {
            continue;
        }
        SmoothModel& m = modelData(model, hdr);
        const bool rigged = rigData(m, model, hdr);
        const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
        const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
        const int count = hdr->numverts_vbo;
        const glm::vec3 scale{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
        const auto at = [&](int pose, int i) {
            const trivertx_t& t = base[static_cast<za::SizeT>(pose) * static_cast<za::SizeT>(hdr->numverts) + desc[i].vertindex];
            return glm::vec3{t.v[0], t.v[1], t.v[2]};
        };
        za::Vector<glm::vec4> rows(static_cast<za::SizeT>(3 * ragdoll::maxBones));
        za::Vector<glm::vec3> lin(static_cast<za::SizeT>(count)), spl(static_cast<za::SizeT>(count)), swung(static_cast<za::SizeT>(count));
        za::Vector<glm::vec3> key1(static_cast<za::SizeT>(count)), key2(static_cast<za::SizeT>(count));
        int pairs = 0, swungPairs = 0, limbs = 0;
        float keyGap = 0.f, offLinear = 0.f;
        int mostAt[2] = {0, 0};
        float worst[3] = {0.f, 0.f, 0.f}, sum[3] = {0.f, 0.f, 0.f};
        double swingSeconds = 0.0;
        for(int f = 0; f < hdr->numframes; f++)
        {
            const int g = m.next[static_cast<za::SizeT>(f)];
            if(g < 0)
            {
                continue;
            }
            const int before = m.prev[static_cast<za::SizeT>(f)], after = m.next[static_cast<za::SizeT>(g)];
            const int poses[4] = {framePose(hdr, before >= 0 ? before : f), framePose(hdr, f), framePose(hdr, g),
                framePose(hdr, after >= 0 ? after : g)};
            if(poses[1] == poses[2])
            {
                continue;
            }
            pairs++;
            for(const float t : {1e-3f, 0.5f, 1.f - 1e-3f})
            {
                const rigpose::Spline s = rigpose::spline(t, spans, tension);
                const double t0 = Sys_DoubleTime();
                const bool any = rigged && swings(m, poses, s, rows.data());
                swingSeconds += Sys_DoubleTime() - t0;
                swungPairs += any && t == 0.5f ? 1 : 0;
                for(int i = 0; i < count; i++)
                {
                    const za::SizeT k = static_cast<za::SizeT>(i);
                    const glm::vec3 p0 = at(poses[0], i), p1 = at(poses[1], i), p2 = at(poses[2], i), p3 = at(poses[3], i);
                    key1[k] = p1 * scale;
                    key2[k] = p2 * scale;
                    lin[k] = glm::mix(p1, p2, t) * scale;
                    glm::vec3 a = s.w[0] * p0 + s.w[1] * p1 + s.w[2] * p2 + s.w[3] * p3;
                    spl[k] = a * scale;
                    if(any)
                    {
                        const glm::vec4 r = m.rest[k];
                        const float bone = za::floor(r.w);
                        const glm::vec4* row = rows.data() + 3 * static_cast<int>(bone);
                        const glm::vec4 q{r.x, r.y, r.z, 1.f};
                        a += (r.w - bone) * 2.f * glm::vec3{glm::dot(row[0], q), glm::dot(row[1], q), glm::dot(row[2], q)};
                    }
                    swung[k] = a * scale;
                    const glm::vec3 d = swung[k] - (t == 0.5f ? lin[k] : t < 0.5f ? key1[k] : key2[k]);
                    const float gap = za::sqrt(glm::dot(d, d));
                    if(t == 0.5f && gap > offLinear)
                    {
                        mostAt[0] = f;
                        mostAt[1] = g;
                    }
                    float& most = t == 0.5f ? offLinear : keyGap;
                    most = za::max(most, gap);
                }
                if(t != 0.5f || !rigged)
                {
                    continue;
                }
                uint32_t unfit = 0;
                for(const int pose : poses)
                {
                    unfit |= m.unfit[static_cast<za::SizeT>(pose)];
                }
                for(int b = 0; b < m.rig->numBones; b++)
                {
                    // (the limbs swung, turning 30 degrees or more between the frames)
                    const float turn = za::abs(glm::dot(rigpose::key(*m.rig, poses[1], b).rot, rigpose::key(*m.rig, poses[2], b).rot));
                    if((unfit & (1u << b)) || turn > 0.9659258f /* cos(30 / 2 degrees) */ || turn < maxTurn)
                    {
                        continue;
                    }
                    const float k1 = spread(key1, *m.rig, desc, count, b), k2 = spread(key2, *m.rig, desc, count, b);
                    if(k1 < 1.f || k2 < 1.f)
                    {
                        continue;
                    }
                    const float mid = 0.5f * (k1 + k2);
                    const float shrink[3] = {1.f - spread(lin, *m.rig, desc, count, b) / mid,
                        1.f - spread(spl, *m.rig, desc, count, b) / mid, 1.f - spread(swung, *m.rig, desc, count, b) / mid};
                    for(int k = 0; k < 3; k++)
                    {
                        worst[k] = za::max(worst[k], shrink[k]);
                        sum[k] += shrink[k];
                    }
                    limbs++;
                }
            }
        }
        const float n = static_cast<float>(za::max(limbs, 1));
        Con_Printf("animsmooth: check %s: %d frame pairs (%d swung); 0.001 off a key: at most %.3f units from it; mid-frame at "
                   "most %.2f units off Quake's lerp (frames %d to %d)\n", model->name, pairs, swungPairs, keyGap, offLinear,
            mostAt[0], mostAt[1]);
        if(rigged)
        {
            Con_Printf("animsmooth: check %s: limbs turning 30-150 degrees (%d), their shrink mid-frame: linear %.1f%% (worst %.1f%%), A %.1f%% (%.1f%%), "
                       "A+B %.1f%% (%.1f%%); swings %.2f us an instance\n", model->name, limbs, 100.f * sum[0] / n,
                100.f * worst[0], 100.f * sum[1] / n, 100.f * worst[1], 100.f * sum[2] / n, 100.f * worst[2],
                1e6 * swingSeconds / static_cast<double>(za::max(pairs * 3, 1)));
        }
    }
}

} // namespace

namespace qvr::animsmooth
{

void registerCommands()
{
    Cmd_AddCommand("vr_anim_smooth_pin", pin_f);
    Cmd_AddCommand("vr_anim_smooth_info", info_f);
    Cmd_AddCommand("vr_anim_smooth_check", check_f);
    Cmd_AddCommand("vr_anim_smooth_trace", trace_f);
}

} // namespace qvr::animsmooth
