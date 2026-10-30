// vr_animsmooth.cpp -- smoother monster animations (r_lerpmodels 3; ROUND22.md, "Smoother animations"), drawn by the
// client only: the server's frames, their times and every gameplay test stay as they are.
//
// Quake's frame lerp (r_lerpmodels 1, FitzQuake's) moves each vertex in a straight line from the last pose to the new one
// over the frame's tenth of a second: a limb swinging through a big angle shrinks mid-swing (its chord), and the speed
// changes at once at every frame. Here, between the same two poses:
// - A, the timing: each vertex goes along a cubic spline through four poses, the lerp's two, the one before them (the lerp
//   that ended where this one starts: entity_t's animpose0, R_AnimHistoryPush) and the one expected after (the next frame
//   of its animation by the frames' names and the progs' loops (vr_mdldequant.cpp's runs: also the jumps and loops of
//   frames named by numbers alone), or the one seen after it last time: a loop's first), its tangents the
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
// C, the rebuild (vr_anim_smooth_rebuild; any r_lerpmodels, every frame, at the poses too): a rigged monster's vertices
// from its skeleton, so the file's rounding (a byte a 255th of the model's size, per frame) doesn't show. Made once a
// model (rigData): its bones refitted to every pose as drawn (dequantised: vr_mdldequant.cpp; the stored fits follow
// the rounding), each vertex's place on its bone the mean of its poses' places taken back by those fits, its normal
// so, and its share by how far the poses are from it (vr_anim_smooth_rebuild_error; a model with under half its
// vertices rebuilt fully isn't rebuilt). Each instance: its bones' refitted keys along its spline (or Quake's blend),
// 9 rows a bone (rebuildRows; their share the fits of the lerp's two poses, blended: a pose fitted badly isn't
// rebuilt, faded across the lerps round it), and the shader moves each vertex (and normal) from where A and B put it to
// its bone's place by both shares (RigRebuild). The loops' seams are the dequantised poses' (smoothed round the loop). Your weapons are never smoothed (their flashes, mounts and
// shells follow Quake's blend: vr_anchor.cpp), nor skeletal models (IQM, a ragdoll's skinned model). What the game
// tests against a model's shape (vr_hitmodel.cpp, vr_modelcollide.cpp) keeps Quake's blend: the drawn shape between
// frames is a few units off it at most mid-swing (vr_anim_smooth_check).

#include "vr_animsmooth_internal.hpp"
#include "vr_geom.hpp"
#include "vr_agarms.hpp"
#include "vr_aglegs.hpp"
#include "vr_hitreact.hpp"
#include "vr_jobs.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_mdldequant.hpp"
#include "vr_mem.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rigpose.hpp"
#include "vr_rigseq.hpp"
#include "vr_rigstudio.hpp"

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

namespace
{

using namespace animsmooth::detail;

// Each model's rest data on the GPU (SmoothModel::rest), uploaded when it is made (and again when
// vr_anim_smooth_rig_error remakes it): a buffer per model slot, kept (not with smoothCache: its GL buffer would be lost),
// made again when the slot's data is new or another size.
struct RestBuffer
{
    GLuint buf{0};
    GLsizeiptr size{0};
};
struct RestBuffers
{
    ankerl::unordered_dense::map<const qmodel_t*, RestBuffer> models;
    auto members() { return mem::list(models); }
};
mem::Cache<RestBuffers> restBuffers{"anim smooth rest", mem::Never};

// The swings of the instances in the batch being filled (channel 0: R_DrawAliasModel_Real's batches; 1: a wound paint's
// one instance), 3 rows per bone, bound with it (VR_AnimSmoothBind). Filled from a batch's first instance on (the
// renderer's thread); meaningless past the draw.
struct SmoothBatches
{
    za::Vector<glm::vec4> swing0;
    za::Vector<glm::vec4> swing1;
    za::Vector<glm::vec4> kept; // channel 0's rows of the first eye's instances, for the second eye's (VR_AnimSmoothKeep)
    auto members() { return mem::list(swing0, swing1, kept); }
};
mem::Scratch<SmoothBatches> batches{"anim smooth"};

[[nodiscard]] za::Vector<glm::vec4>& swingsOf(int channel)
{
    return channel ? batches.swing1 : batches.swing0;
}

constexpr int rebuildRowsPerBone = 9;   // the rebuild's rows a bone: its swing (3), its motion (3), its turn (3; the share in the first's w)
constexpr float rebuildMinShare = 0.5f; // a model with fewer of its vertices rebuilt fully than this isn't rebuilt

// What an entity's spline was last worked out for, and what came of it: drawn again so (the other eye, each shadow
// map's light, the next frame paused) it is copied. By the entity (the renderer's thread; a map change frees it, and a
// model reload: the rig it was swung by is made again).
struct Key
{
    int poses[4]{-1, -1, -1, -1};
    float span[3]{0.f, 0.f, 0.f};
    float blend{-1.f};
    float tension{-1.f};
    float rigError{-2.f}; // (-1: not swung)
    int mode{-1};         // 0 the spline; 1 the spline, rebuilt; 2 Quake's blend, rebuilt
    float rebuildError{-2.f};
    // The model whose rig made the rows: an entity drawn with another model on the same poses (the toolgun's ghost, a
    // temp entity's slot taken by another choice; an edict reused) must not take the last one's bones.
    const qmodel_t* model{nullptr};
    [[nodiscard]] bool operator==(const Key& o) const
    {
        return !memcmp(poses, o.poses, sizeof(poses)) && !memcmp(span, o.span, sizeof(span)) && blend == o.blend &&
               tension == o.tension && rigError == o.rigError && mode == o.mode && rebuildError == o.rebuildError &&
               model == o.model;
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
    glm::vec4 rows[rebuildRowsPerBone * ragdoll::maxBones];
};
struct SmoothMemos
{
    ankerl::unordered_dense::map<const entity_t*, Memo> entities;
    auto members() { return mem::list(entities); }
};
mem::Cache<SmoothMemos> memos{"anim smooth entities", mem::MapChange | mem::ModelReload};
// The rig data's rebuild (rigData) on the pool: its places by vertex, its refits by pose (vr_jobs_sites).
jobs::Site placesSite{"anim smooth rebuild places"};
jobs::Site refitSite{"anim smooth rebuild refits"};

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

} // namespace
namespace qvr::animsmooth::detail
{
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
    m.namedNext.resize(static_cast<za::SizeT>(m.numFrames), 0);
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
        // (numbers alone, the knight's "1" .. "97", name no animation: the file's order, a guess)
        m.namedNext[static_cast<za::SizeT>(f)] = na >= 0 && nb >= 0 && a[0] && b[0];
        // The same animation, numbered on (or unnamed, or unnumbered: the shambler's: the file's order)
        const bool on = !strcmp(a, b) && (na < 0 || nb < 0 || nb == na + 1);
        if(on)
        {
            m.next[static_cast<za::SizeT>(f)] = static_cast<int16_t>(f + 1);
        }
    }
    // The dequantiser's runs: the progs' loops (knight_stand9 back to knight_stand1: sure, over the names too) and, where
    // the names are only a guess (the file's order: the knight's numbers, the shambler's "From ASE"), the jumps between
    // unrelated animations (none expected after: learnt once seen) and the loops that come back to their first pose. A
    // named animation's end stays unknown until seen (an attack's last frame: its first again would be a guess).
    // (ROUND22, "Smooth's idle knight jolt": the knight's stand9 went on to run1 by the numbers, a 2.5-unit jolt.)
    if(hdr->poseverttype == aliashdr_t::PV_QUAKE1 && m.numPoses > 0)
    {
        za::Vector<short> after(static_cast<za::SizeT>(m.numPoses), -1);
        za::Vector<unsigned char> progs(static_cast<za::SizeT>(m.numPoses), 0);
        mdldequant::poseSuccessors(hdr, model->name, after.data(), progs.data());
        for(int f = 0; f < m.numFrames; f++)
        {
            const za::SizeT fi = static_cast<za::SizeT>(f);
            const int pose = hdr->frames[f].firstpose;
            if(m.next[fi] == -2 || pose < 0 || pose >= m.numPoses)
            {
                continue;
            }
            const int ap = after[static_cast<za::SizeT>(pose)];
            const int af = ap >= 0 ? m.frameOfPose[static_cast<za::SizeT>(ap)] : -1;
            if(progs[static_cast<za::SizeT>(pose)])
            {
                m.next[fi] = static_cast<int16_t>(af);
                m.namedNext[fi] = af >= 0;
            }
            else if(!m.namedNext[fi])
            {
                m.next[fi] = static_cast<int16_t>(af);
                m.namedNext[fi] = 0;
            }
        }
    }
    for(int f = 0; f < m.numFrames; f++)
    {
        const int n = m.next[static_cast<za::SizeT>(f)];
        if(n >= 0 && m.prev[static_cast<za::SizeT>(n)] < 0)
        {
            m.prev[static_cast<za::SizeT>(n)] = static_cast<int16_t>(f);
        }
    }
    // Seed-table boundaries need no derived rig (Smooth Limbs off stays cheap). Never predict the death's first
    // frame from the preceding idle frame: a loop we haven't seen yet holds its end until the successor is learnt.
    for(int f = 0; f < m.numFrames; f++)
    {
        if(ragdoll::deathBoundary(model, hdr, f, false))
        {
            m.next[static_cast<za::SizeT>(f)] = -1;
        }
        if(ragdoll::deathBoundary(model, hdr, f, true))
        {
            m.prev[static_cast<za::SizeT>(f)] = -1;
            if(f > 0 && m.next[static_cast<za::SizeT>(f - 1)] == f)
            {
                m.next[static_cast<za::SizeT>(f - 1)] = -1;
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
    const float rebuildError = za::max(vr_anim_smooth_rebuild_error.value, 0.01f);
    const int dequant = hdr->dequant && hdr->dequantready && vr_mdl_dequant.value != 0.f ? 1 : 0;
    if(m.rigError == error && m.rebuildError == rebuildError && m.rebuildDequant == dequant)
    {
        return true;
    }
    const ragdoll::Rig& rig = *m.rig;
    m.rigError = error;
    m.rebuildError = rebuildError;
    m.rebuildDequant = dequant;
    m.invScale = glm::vec3{1.f / hdr->scale[0], 1.f / hdr->scale[1], 1.f / hdr->scale[2]};
    m.origin = geom::vec(hdr->scale_origin);
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
    // The rebuild: each vertex's place on its bone (pose 0's frame) and its normal so, and each bone's fit to each pose
    // made again to the poses as drawn (dequantised: the stored fits follow the file's rounding, a turn's noise times
    // the limb's length). From the stored fits: a vertex's place the mean over the poses its bone fits of the pose's place
    // taken back by the fit; then, twice, each bone refitted to its vertices' drawn places (weighted by their shares) and
    // the places again. How far the drawn poses are from a vertex rebuilt (rms) is its share.
    za::Vector<glm::vec4> rebuilt(static_cast<za::SizeT>(hdr->numverts) * 2u);
    za::Vector<float> rebuildRms(static_cast<za::SizeT>(hdr->numverts), 0.f);
    {
        const za::SizeT nv = static_cast<za::SizeT>(hdr->numverts);
        const auto drawn = [&](int p, int v) { return mdldequant::poseVertex(hdr, p, v) * scale + origin; };
        za::Vector<glm::vec3> xs(static_cast<za::SizeT>(m.numPoses) * nv);
        for(int p = 0; p < m.numPoses; p++)
        {
            for(int v = 0; v < hdr->numverts; v++)
            {
                xs[static_cast<za::SizeT>(p) * nv + static_cast<za::SizeT>(v)] = drawn(p, v);
            }
        }
        const auto x = [&](int p, int v) -> const glm::vec3& { return xs[static_cast<za::SizeT>(p) * nv + static_cast<za::SizeT>(v)]; };
        za::Vector<rigpose::Rigid>& keys = m.rebuildKeys;
        keys.clear();
        keys.resize(static_cast<za::SizeT>(m.numPoses * rig.numBones));
        for(int p = 0; p < m.numPoses; p++)
        {
            for(int b = 0; b < rig.numBones; b++)
            {
                keys[static_cast<za::SizeT>(p * rig.numBones + b)] = rigpose::key(rig, p, b);
            }
        }
        const auto fits = [&](int p, int b) { return !(m.unfit[static_cast<za::SizeT>(p)] & (1u << b)); };
        za::Vector<glm::vec3> place(nv), normal(nv);
        za::Vector<int> count(nv);
        za::Vector<float> share(nv, 1.f);
        za::Vector<float> sum(nv);
        // Each vertex's place, normal and share from the poses its bone fits (vertices in chunks on the pool: each
        // vertex's sums over the poses in the same order whichever thread runs it, so the same numbers).
        const auto places = [&]() {
            jobs::parallelFor(placesSite, nv, 0, [&](za::SizeT v0, za::SizeT v1) {
                for(za::SizeT k = v0; k < v1; k++)
                {
                    place[k] = normal[k] = glm::vec3{0.f};
                    count[k] = 0;
                    sum[k] = 0.f;
                }
                for(int p = 0; p < m.numPoses; p++)
                {
                    for(int v = static_cast<int>(v0); v < static_cast<int>(v1); v++)
                    {
                        const int b = rig.vertBone[static_cast<za::SizeT>(v)];
                        if(!fits(p, b))
                        {
                            continue;
                        }
                        const rigpose::Rigid& k = keys[static_cast<za::SizeT>(p * rig.numBones + b)];
                        const glm::quat back = glm::conjugate(k.rot);
                        const trivertx_t& t = base[static_cast<za::SizeT>(p) * nv + static_cast<za::SizeT>(v)];
                        const float* n = r_avertexnormals[za::min<int>(t.lightnormalindex, NUMVERTEXNORMALS - 1)];
                        place[static_cast<za::SizeT>(v)] += back * (x(p, v) - k.pos);
                        normal[static_cast<za::SizeT>(v)] += back * geom::vec(n);
                        count[static_cast<za::SizeT>(v)]++;
                    }
                }
                for(int v = static_cast<int>(v0); v < static_cast<int>(v1); v++)
                {
                    const za::SizeT k = static_cast<za::SizeT>(v);
                    place[k] = count[k] ? place[k] / static_cast<float>(count[k]) : at(0, v);
                    const float len = za::sqrt(glm::dot(normal[k], normal[k]));
                    normal[k] = len > 1e-4f ? normal[k] / len : glm::vec3{0.f, 0.f, 1.f};
                }
                for(int p = 0; p < m.numPoses; p++)
                {
                    for(int v = static_cast<int>(v0); v < static_cast<int>(v1); v++)
                    {
                        const int b = rig.vertBone[static_cast<za::SizeT>(v)];
                        if(fits(p, b))
                        {
                            const glm::vec3 d = x(p, v) - rigpose::apply(keys[static_cast<za::SizeT>(p * rig.numBones + b)], place[static_cast<za::SizeT>(v)]);
                            sum[static_cast<za::SizeT>(v)] += glm::dot(d, d);
                        }
                    }
                }
                for(za::SizeT k = v0; k < v1; k++)
                {
                    // (a vertex its bone fits in no pose: not rebuilt)
                    rebuildRms[k] = count[k] ? za::sqrt(sum[k] / static_cast<float>(count[k])) : 1e9f;
                    share[k] = za::clamp(2.f - rebuildRms[k] / rebuildError, 0.f, 1.f);
                }
            });
        };
        // Each bone's vertices, in order (boneFirst[b] .. boneFirst[b + 1] in boneVerts): refit's loops over them alone
        // (the same sums in the same order as over every vertex, the others skipped).
        za::Vector<int> boneFirst(static_cast<za::SizeT>(rig.numBones) + 1u, 0);
        za::Vector<int> boneVerts(nv);
        for(int v = 0; v < hdr->numverts; v++)
        {
            boneFirst[static_cast<za::SizeT>(rig.vertBone[static_cast<za::SizeT>(v)]) + 1u]++;
        }
        for(int b = 0; b < rig.numBones; b++)
        {
            boneFirst[static_cast<za::SizeT>(b) + 1u] += boneFirst[static_cast<za::SizeT>(b)];
        }
        {
            za::Vector<int> fill(boneFirst.begin(), boneFirst.end() - 1);
            for(int v = 0; v < hdr->numverts; v++)
            {
                boneVerts[static_cast<za::SizeT>(fill[static_cast<za::SizeT>(rig.vertBone[static_cast<za::SizeT>(v)])]++)] = v;
            }
        }
        // A bone's fit to its vertices' drawn places in a pose, from the fit it has: Gauss-Newton steps on its turn about
        // the places' weighted middles (a vertex rebuilt counts fully, one not a little: the joints' skin doesn't pull it).
        const auto refit = [&](int p, int b) {
            rigpose::Rigid& k = keys[static_cast<za::SizeT>(p * rig.numBones + b)];
            const int* const vFirst = boneVerts.data() + boneFirst[static_cast<za::SizeT>(b)];
            const int* const vEnd = boneVerts.data() + boneFirst[static_cast<za::SizeT>(b) + 1u];
            glm::vec3 cMid{0.f}, xMid{0.f};
            float wSum = 0.f;
            for(const int* vi = vFirst; vi != vEnd; ++vi)
            {
                const int v = *vi;
                const float w = 0.05f + share[static_cast<za::SizeT>(v)];
                cMid += w * place[static_cast<za::SizeT>(v)];
                xMid += w * x(p, v);
                wSum += w;
            }
            if(wSum <= 0.f)
            {
                return;
            }
            cMid /= wSum;
            xMid /= wSum;
            glm::quat q = k.rot;
            for(int step = 0; step < 3; step++)
            {
                const glm::mat3 r = glm::mat3_cast(q);
                glm::mat3 a{0.f};
                glm::vec3 g{0.f};
                for(const int* vi = vFirst; vi != vEnd; ++vi)
                {
                    const int v = *vi;
                    const float w = 0.05f + share[static_cast<za::SizeT>(v)];
                    const glm::vec3 c = r * (place[static_cast<za::SizeT>(v)] - cMid);
                    a += w * (glm::dot(c, c) * glm::mat3{1.f} - glm::outerProduct(c, c));
                    g += w * glm::cross(c, x(p, v) - xMid);
                }
                if(za::abs(glm::determinant(a)) < 1e-6f)
                {
                    break;
                }
                q = glm::normalize(rigpose::fromVector(glm::inverse(a) * g) * q);
            }
            k.rot = q;
            k.pos = xMid - q * cMid;
        };
        places();
        for(int round = 0; round < 2; round++)
        {
            // (poses in chunks on the pool: each refit writes its own pose's key alone)
            jobs::parallelFor(refitSite, static_cast<za::SizeT>(m.numPoses), 0, [&](za::SizeT p0, za::SizeT p1) {
                for(int p = static_cast<int>(p0); p < static_cast<int>(p1); p++)
                {
                    for(int b = 0; b < rig.numBones; b++)
                    {
                        if(fits(p, b))
                        {
                            refit(p, b);
                        }
                    }
                }
            });
            places();
        }
        for(za::SizeT k = 0; k < nv; k++)
        {
            rebuilt[2u * k] = glm::vec4{place[k], share[k]};
            rebuilt[2u * k + 1u] = glm::vec4{normal[k], 0.f};
        }
    }
    m.rest.clear();
    m.rest.resize(static_cast<za::SizeT>(hdr->numverts_vbo) * restStride);
    m.restUploaded = false;
    m.armSkin = false;
    m.legSkin = false;
    m.swungVerts = m.fadedVerts = 0;
    m.rebuiltVerts = m.rebuildFaded = 0;
    za::Vector<float> rms(static_cast<za::SizeT>(hdr->numverts_vbo));
    for(int i = 0; i < hdr->numverts_vbo; i++)
    {
        const int v = za::clamp(static_cast<int>(desc[i].vertindex), 0, hdr->numverts - 1);
        const float vrms = za::sqrt(vertSum[static_cast<za::SizeT>(v)] / static_cast<float>(za::max(vertCount[static_cast<za::SizeT>(v)], 1)));
        const float share = za::clamp(2.f - vrms / error, 0.f, 1.f);
        m.swungVerts += share >= 1.f ? 1 : 0;
        m.fadedVerts += share > 0.f && share < 1.f ? 1 : 0;
        glm::vec4* r = m.rest.data() + static_cast<za::SizeT>(i) * restStride;
        r[0] = glm::vec4{at(0, v), static_cast<float>(rig.vertBone[static_cast<za::SizeT>(v)]) + 0.5f * za::min(share, 0.999f)};
        r[1] = rebuilt[2u * static_cast<za::SizeT>(v)];
        r[2] = rebuilt[2u * static_cast<za::SizeT>(v) + 1u];
        // An armed humanoid's arm skin (vr_agarms): its fourth row (its arm's bones' weights; the shader's RigArm).
        r[3] = glm::vec4{0.f};
        if(hitreact::armSkinRow(model, hdr, rig, v, r[3]))
        {
            m.armSkin = true;
        }
        // ... and its legs' (vr_aglegs: the codes past the arms', the shader's RigLeg; their rows after the arms').
        if(m.armSkin && r[3].x < 0.5f && aglegs::restRow(model, hdr, rig, v, 3 * rig.numBones + agarms::blockRows, r[3]))
        {
            m.legSkin = true;
        }
        m.rebuiltVerts += r[1].w >= 1.f ? 1 : 0;
        m.rebuildFaded += r[1].w > 0.f && r[1].w < 1.f ? 1 : 0;
        rms[static_cast<za::SizeT>(i)] = za::min(rebuildRms[static_cast<za::SizeT>(v)], 99.f);
    }
    za::quickSort(rms.begin(), rms.end());
    const auto pct = [&](float f) { return rms.empty() ? 0.f : rms[static_cast<za::SizeT>(f * static_cast<float>(rms.size() - 1))]; };
    m.rebuildMedian = pct(0.5f);
    m.rebuildP90 = pct(0.9f);
    m.rebuildMost = pct(1.f);
    m.rebuildOk = hdr->numverts_vbo > 0 &&
                  static_cast<float>(m.rebuiltVerts) >= rebuildMinShare * static_cast<float>(hdr->numverts_vbo);
    return true;
}
} // namespace qvr::animsmooth::detail
namespace
{

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
    if(next == -1 || (!m.namedNext[static_cast<za::SizeT>(frame)] && seen >= 0))
    {
        next = seen;
    }
    return next >= 0 ? hdr->frames[next].firstpose : -1;
}

[[nodiscard]] bool pinned(const entity_t* e)
{
    return pin.ent >= 0 && pin.ent < cl.num_entities && e == &cl_entities[pin.ent];
}

} // namespace
namespace qvr::animsmooth::detail
{
[[nodiscard]] int framePose(const aliashdr_t* hdr, int frame)
{
    return hdr->frames[za::clamp(frame, 0, hdr->numframes - 1)].firstpose;
}

// The swings of `m`'s bones between poses `poses` along `s`, into `out` (3 rows a bone, `stride` rows apart; all zeros
// for a bone not swung): false if none swings (`out` then untouched); `now`, if given, the bones' motion along `s`.
bool swings(const SmoothModel& m, const int poses[4], const rigpose::Spline& s, glm::vec4* out, int stride,
    rigpose::Rigid* nowOut)
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
    const bool none = (still & ((1u << rig.numBones) - 1u)) == ((1u << rig.numBones) - 1u);
    if(none && !nowOut)
    {
        return false;
    }
    rigpose::Rigid now[ragdoll::maxBones];
    rigpose::alongSlots(rig, keys, s, still, now); // (the keys made above: not looked up again per bone)
    if(nowOut)
    {
        memcpy(static_cast<void*>(nowOut), now, sizeof(rigpose::Rigid) * static_cast<size_t>(rig.numBones));
    }
    if(none)
    {
        return false;
    }
    for(int b = 0; b < rig.numBones; b++)
    {
        glm::vec4* rows = out + stride * b;
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
} // namespace qvr::animsmooth::detail
namespace
{

// The rebuild's rows (vr_anim_smooth_rebuild) between poses `poses` along `s`, into `out` (rebuildRowsPerBone a bone):
// its swing (as swings(), when `swing`: the spline's; else zeros), its motion now (its refitted keys along `s`) from its
// rest place to the poses' bytes, its turn (the normal's) with its share in the first's w: the fits of the lerp's two
// poses blended by `blend` (a bone fitted badly in a pose isn't rebuilt there, and fades in and out across the lerps on
// either side of it).
void rebuildRows(const SmoothModel& m, const int poses[4], const rigpose::Spline& s, float blend, bool swing, glm::vec4* out)
{
    const ragdoll::Rig& rig = *m.rig;
    rigpose::Rigid now[ragdoll::maxBones];
    rigpose::along(rig, m.rebuildKeys.data(), poses, s, 0u, now);
    if(!(swing && swings(m, poses, s, out, rebuildRowsPerBone)))
    {
        for(int b = 0; b < rig.numBones; b++)
        {
            glm::vec4* rows = out + rebuildRowsPerBone * b;
            rows[0] = rows[1] = rows[2] = glm::vec4{0.f};
        }
    }
    const uint32_t unfit1 = m.unfit[static_cast<za::SizeT>(poses[1])], unfit2 = m.unfit[static_cast<za::SizeT>(poses[2])];
    for(int b = 0; b < rig.numBones; b++)
    {
        glm::vec4* rows = out + rebuildRowsPerBone * b;
        const glm::mat3 r = glm::mat3_cast(now[b].rot);
        const glm::vec3 t = now[b].pos - m.origin;
        const float share = (1.f - blend) * ((unfit1 >> b) & 1u ? 0.f : 1.f) + blend * ((unfit2 >> b) & 1u ? 0.f : 1.f);
        for(int row = 0; row < 3; row++)
        {
            rows[3 + row] = glm::vec4{r[0][row], r[1][row], r[2][row], t[row]} * m.invScale[row];
            rows[6 + row] = glm::vec4{r[0][row], r[1][row], r[2][row], row == 0 ? share : 0.f};
        }
    }
}

// A VBO vertex as the shader draws it (the poses' bytes): the four poses' places blended by `weights`, plus its swing
// (`rows` 3 a bone) or, rebuilt (`rebuild`: rebuildRowsPerBone a bone), moved to its rebuild place by its share. The
// poses' places as drawn (dequantised: mdldequant::poseVertex).
[[nodiscard]] glm::vec3 drawnVertex(const SmoothModel* m, const aliashdr_t* hdr, int i, const int poses[4], const float weights[4],
    const glm::vec4* rows, bool rebuild)
{
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
    const int v = desc[i].vertindex;
    glm::vec3 p{0.f};
    for(int k = 0; k < 4; k++)
    {
        p += weights[k] == 0.f ? glm::vec3{0.f} : weights[k] * mdldequant::poseVertex(hdr, poses[k], v);
    }
    if(!rows || !m || static_cast<za::SizeT>(i + 1) * restStride > m->rest.size())
    {
        return p;
    }
    const glm::vec4* r = m->rest.data() + static_cast<za::SizeT>(i) * restStride;
    const float bone = za::floor(r[0].w);
    const glm::vec4* row = rows + (rebuild ? rebuildRowsPerBone : 3) * static_cast<int>(bone);
    const glm::vec4 q{r[0].x, r[0].y, r[0].z, 1.f};
    p += (r[0].w - bone) * 2.f * glm::vec3{glm::dot(row[0], q), glm::dot(row[1], q), glm::dot(row[2], q)};
    const float k = rebuild ? r[1].w * row[6].w : 0.f;
    if(k > 0.f)
    {
        const glm::vec4 c{r[1].x, r[1].y, r[1].z, 1.f};
        p = glm::mix(p, glm::vec3{glm::dot(row[3], c), glm::dot(row[4], c), glm::dot(row[5], c)}, k);
    }
    return p;
}

} // namespace

namespace
{

void traceDraw(const entity_t* e, const aliashdr_t* hdr, int pose1, int pose2, float blend, int on, const int smooth[4],
    const float weights[4])
{
    if(trace.ent < 0 || e != &cl_entities[trace.ent] || cl.time == trace.lastTime || hdr->poseverttype != aliashdr_t::PV_QUAKE1)
    {
        return;
    }
    trace.lastTime = cl.time;
    const glm::vec3 scale{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
    const int count = hdr->numverts_vbo;
    const auto mit = smoothCache.models.find(e->model);
    const SmoothModel* m = mit != smoothCache.models.end() ? mit->second.get() : nullptr;
    const bool rebuild = on && smooth[2] < 0;
    const glm::vec4* rows = on && smooth[2] != 0 ? batches.swing0.data() + (rebuild ? -smooth[2] - 1 : smooth[2] - 1) : nullptr;
    const float b = pose1 == pose2 ? 0.f : blend;
    const int poses[4] = {on ? smooth[0] : pose1, pose1, pose2, on ? smooth[1] : pose2};
    const float lerp[4] = {0.f, 1.f - b, b, 0.f};
    trace.keys[2] = trace.keys[1];
    trace.keys[1] = trace.keys[0];
    trace.keys[0] = Trace::Keys{cl.time, e->frame, {poses[0], poses[1], poses[2], poses[3]}, blend, {0.f, 0.f, 0.f, 0.f}};
    for(int k = 0; k < 4; k++)
    {
        trace.keys[0].w[k] = on ? weights[k] : lerp[k];
    }
    trace.prev2 = trace.prev;
    trace.prev = trace.now;
    trace.now.resize(static_cast<za::SizeT>(count));
    for(int i = 0; i < count; i++)
    {
        trace.now[static_cast<za::SizeT>(i)] = drawnVertex(m, hdr, i, poses, on ? weights : lerp, rows, rebuild) * scale;
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
        if(trace.joltLog > 0.f && most > trace.joltLog)
        {
            Con_Printf("animsmooth: jolt %.3f at %.4f\n", most, cl.time);
            for(int k = 2; k >= 0; k--)
            {
                const Trace::Keys& q = trace.keys[k];
                Con_Printf("animsmooth:   t %.4f frame %d keys %d %d %d %d blend %.3f w %.3f %.3f %.3f %.3f\n", q.time, q.frame,
                    q.poses[0], q.poses[1], q.poses[2], q.poses[3], q.blend, q.w[0], q.w[1], q.w[2], q.w[3]);
            }
        }
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

void keysOf(const entity_t* e, const aliashdr_t* hdr, SmoothModel& m, Memo& memo, int pose1, int pose2, int poses[4], float span[3]);

[[nodiscard]] int smoothInstance(const entity_t* e, const void* aliashdr, int pose1, int pose2, float blend, int zeroblend,
    int batchindex, int channel, int smooth[4], float weights[4])
{
    za::Vector<glm::vec4>& swing = swingsOf(channel);
    if(batchindex == 0)
    {
        swing.clear();
    }
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    if(!e->model || hdr->poseverttype == aliashdr_t::PV_IQM)
    {
        return 0;
    }
    // (not your weapons: their muzzle flashes, mounts and shells follow the gun's vertices by Quake's blend: vr_anchor.cpp,
    // the steadied recoil's reference vertex)
    if(zeroblend || VR_IsViewEntity(e))
    {
        return 0;
    }
    if(pose1 < 0 || pose1 >= hdr->numposes || pose2 < 0 || pose2 >= hdr->numposes)
    {
        return 0;
    }
    const bool splined = r_lerpmodels.value == 3.f && pose1 != pose2 && blend > 0.f && blend < 1.f &&
                         !(cls.demoplayback && cls.demospeed < 0.f);
    const bool rebuildOn = vr_anim_smooth_rebuild.value != 0.f && hdr->poseverttype == aliashdr_t::PV_QUAKE1;
    if(!splined && !rebuildOn)
    {
        return 0;
    }
    SmoothModel& m = modelData(e->model, hdr);
    const bool rebuild = rebuildOn && rigData(m, e->model, hdr) && m.rebuildOk;
    if(!splined && !rebuild)
    {
        return 0;
    }
    Memo& memo = memos.entities[e];
    if(!splined)
    {
        // Rebuilt on Quake's blend (any Model Animation but Smooth mid-frame; at a frame itself, Smooth's too): the
        // bones' fits turned from the lerp's first pose to its second, each vertex on its bone
        const float b = pose1 == pose2 ? 0.f : za::clamp(blend, 0.f, 1.f);
        const int poses[4] = {pose1, pose1, pose2, pose2};
        const Key key{{poses[0], poses[1], poses[2], poses[3]}, {0.f, 0.f, 0.f}, b, -1.f, m.rigError, 2, m.rebuildError, e->model};
        if(!(memo.key == key))
        {
            memo.key = key;
            memo.spline = rigpose::Spline{};
            memo.spline.w[1] = 1.f - b;
            memo.spline.w[2] = b;
            memo.spline.rot[1] = b;
            rebuildRows(m, poses, memo.spline, b, false, memo.rows);
            memo.swung = true;
            memo.numRows = rebuildRowsPerBone * m.rig->numBones;
            stats.rebuilt++;
        }
        smooth[0] = pose1;
        smooth[1] = pose2;
        smooth[3] = 1;
        for(int i = 0; i < 4; i++)
        {
            weights[i] = memo.spline.w[i];
        }
        za::Vector<glm::vec4>& rowsOut = swingsOf(channel);
        const za::SizeT at = rowsOut.size();
        rowsOut.resize(at + static_cast<za::SizeT>(memo.numRows));
        memcpy(static_cast<void*>(rowsOut.data() + at), memo.rows, sizeof(glm::vec4) * static_cast<size_t>(memo.numRows));
        smooth[2] = -1 - static_cast<int>(at);
        return 1;
    }

    // The four keys and their times
    int poses[4];
    float span[3];
    keysOf(e, hdr, m, memo, pose1, pose2, poses, span);
    const float tension = za::clamp(vr_anim_smooth_tension.value, 0.f, 1.f);
    const bool rigged = vr_anim_smooth_rig.value != 0.f && rigData(m, e->model, hdr);
    const Key key{{poses[0], poses[1], poses[2], poses[3]}, {span[0], span[1], span[2]}, blend, tension, rigged ? m.rigError : -1.f,
        rebuild ? 1 : 0, rebuild ? m.rebuildError : -1.f, e->model};
    if(!(memo.key == key))
    {
        // Not drawn so yet (another view, another light: the same)
        memo.key = key;
        memo.spline = rigpose::spline(blend, span, tension);
        if(rebuild)
        {
            rebuildRows(m, poses, memo.spline, blend, rigged, memo.rows);
            memo.swung = true;
            memo.numRows = rebuildRowsPerBone * m.rig->numBones;
            stats.rebuilt++;
        }
        else
        {
            memo.swung = rigged && swings(m, poses, memo.spline, memo.rows);
            memo.numRows = rigged ? 3 * m.rig->numBones : 0;
        }
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
        smooth[2] = rebuild ? -1 - static_cast<int>(at) : 1 + static_cast<int>(at);
    }
    return 1;
}

// The spline's four keys and their spans for an entity's lerp (pose1 -> pose2): the one before (its history), the one
// expected after (its animation's next, or learnt); smoothInstance's, and vr_rig2's live drawing's (splineOf).
void keysOf(const entity_t* e, const aliashdr_t* hdr, SmoothModel& m, Memo& memo, int pose1, int pose2, int poses[4], float span[3])
{
    poses[0] = poses[1] = pose1;
    poses[2] = poses[3] = pose2;
    span[0] = span[1] = span[2] = pinSpan;
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
}

// Hit reactions (vr_hitreact.cpp; vr_hit_react 1): a rigged monster's bones kicked, whatever Model Animation is: 3 rows a
// bone after its swings, each the bone's kick less no kick (the delta its posed vertices take, from their rest places),
// added whole by the shader (RigKick; Smooth.w 2 + their first row). Not smoothed (`on` 0), it is drawn on Quake's
// blend's own weights, exactly as it would be.
[[nodiscard]] int kickInstance(const entity_t* e, const void* aliashdr, int pose1, int pose2, float blend, int zeroblend,
    int channel, int on, int smooth[4], float weights[4])
{
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    // (an armed grunt's arms on its weapon's grips too: hitreact::armsWanted)
    if(zeroblend || !e->model || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || VR_IsViewEntity(e) || pose1 < 0 ||
        pose1 >= hdr->numposes || pose2 < 0 || pose2 >= hdr->numposes || (!hitreact::bonesWanted(e) && !rigseq::wants(e)))
    {
        return on;
    }
    SmoothModel& m = modelData(e->model, hdr);
    if(!rigData(m, e->model, hdr))
    {
        if(vr_agrunt_ik_debug.value >= 3.f)
        {
            const ragdoll::Rig* r = ragdoll::rigFor(e->model);
            Con_Printf("agik kick: no rig (%s: rig %d, its vertices %d poses %d bones %d; the model's %d %d, next surface %d)\n",
                e->model->name, r ? 1 : 0, r ? r->numVerts : -1, r ? r->numPoses : -1, r ? r->numBones : -1, hdr->numverts,
                hdr->numposes, Mod_NextSurface(const_cast<aliashdr_t*>(hdr)) ? 1 : 0);
        }
        return on;
    }
    const ragdoll::Rig& rig = *m.rig;
    // The bones as drawn, near enough (the lerp's poses blended: a kick's turn is small)
    const float b = pose1 == pose2 ? 0.f : za::clamp(blend, 0.f, 1.f);
    rigpose::Rigid posed[ragdoll::maxBones];
    for(int i = 0; i < rig.numBones; i++)
    {
        const rigpose::Rigid k1 = rigpose::key(rig, pose1, i), k2 = rigpose::key(rig, pose2, i);
        posed[i].rot = glm::slerp(k1.rot, k2.rot, b);
        posed[i].pos = glm::mix(k1.pos, k2.pos, b);
    }
    rigpose::Rigid kick[ragdoll::maxBones];
    const bool turned = hitreact::boneTurns(e, rig, posed, kick);
    if(!turned)
    {
        for(int i = 0; i < rig.numBones; i++)
        {
            kick[i] = rigpose::Rigid{};
        }
    }
    // The Rig Studio's animations (vr_rigseq.cpp: a sequence QC started, the files' override tracks, the studio's pose)
    // under the turns; none: as before.
    const bool layered = rigseq::wants(e) && rigseq::layer(e, rig, hdr, pose1, pose2, b, posed, kick);
    if(!turned && !layered)
    {
        return on;
    }
    za::Vector<glm::vec4>& swing = swingsOf(channel);
    const za::SizeT at = swing.size();
    swing.resize(at + static_cast<za::SizeT>(3 * rig.numBones + (m.armSkin ? agarms::blockRows : 0) +
                                             (m.legSkin ? aglegs::blockRows : 0)));
    if(m.armSkin)
    {
        // An armed humanoid's arm rows after its bones' (its rest data's fourth row says where: vr_agarms.cpp).
        (void)agarms::rows(e, hdr, swing.data() + at + static_cast<za::SizeT>(3 * rig.numBones));
    }
    if(m.legSkin)
    {
        // ... its leg rows after them (vr_aglegs.cpp).
        aglegs::rows(e, hdr, swing.data() + at + static_cast<za::SizeT>(3 * rig.numBones + agarms::blockRows));
    }
    for(int i = 0; i < rig.numBones; i++)
    {
        // p = R rest + t posed; kicked K p + k: the delta (K - I)(R rest + t) + k
        const glm::mat3 a = glm::mat3_cast(kick[i].rot) - glm::mat3{1.f};
        const glm::mat3 r = a * glm::mat3_cast(posed[i].rot);
        const glm::vec3 t = a * posed[i].pos + kick[i].pos;
        glm::vec4* rows = swing.data() + at + static_cast<za::SizeT>(3 * i);
        for(int row = 0; row < 3; row++)
        {
            rows[row] = glm::vec4{r[0][row], r[1][row], r[2][row], t[row]} * m.invScale[row];
        }
    }
    if(!on)
    {
        smooth[0] = pose1;
        smooth[1] = pose2;
        smooth[2] = 0;
        weights[0] = 0.f;
        weights[1] = 1.f - blend;
        weights[2] = blend;
        weights[3] = 0.f;
    }
    smooth[3] = 2 + static_cast<int>(at);
    return 1;
}

} // namespace

bool animsmooth::splineOf(const entity_s* e, const void* aliashdr, int pose1, int pose2, float blend, int poses[4], float w[4],
    float rot[3])
{
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    if(!e || !e->model || !hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || r_lerpmodels.value != 3.f || pose1 == pose2 ||
        blend <= 0.f || blend >= 1.f || (cls.demoplayback && cls.demospeed < 0.f) || pose1 < 0 || pose2 < 0 ||
        pose1 >= hdr->numposes || pose2 >= hdr->numposes)
    {
        return false;
    }
    SmoothModel& m = modelData(e->model, hdr);
    Memo& memo = memos.entities[e];
    float span[3];
    keysOf(e, hdr, m, memo, pose1, pose2, poses, span);
    const rigpose::Spline s = rigpose::spline(blend, span, za::clamp(vr_anim_smooth_tension.value, 0.f, 1.f));
    for(int i = 0; i < 4; i++)
    {
        w[i] = s.w[i];
    }
    for(int i = 0; i < 3; i++)
    {
        rot[i] = s.rot[i];
    }
    return true;
}

bool animsmooth::rigged(const entity_s* e)
{
    if(!e || !e->model || e->model->type != mod_alias)
    {
        return false;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e->model));
    return hdr && hdr->poseverttype == aliashdr_t::PV_QUAKE1 && rigData(modelData(e->model, hdr), e->model, hdr);
}

void animsmooth::prepare()
{
    const bool rebuildOn = vr_anim_smooth_rebuild.value != 0.f;
    if(r_lerpmodels.value != 3.f && !rebuildOn)
    {
        return;
    }
    const bool rig = rebuildOn || vr_anim_smooth_rig.value != 0.f; // (smoothInstance's and kickInstance's rigData)
    for(int i = 1; i < MAX_MODELS && cl.model_precache[i]; i++)
    {
        qmodel_t* model = cl.model_precache[i];
        if(model->type != mod_alias)
        {
            continue;
        }
        const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
        if(!hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1)
        {
            continue;
        }
        SmoothModel& m = modelData(model, hdr);
        if(rig)
        {
            (void)rigData(m, model, hdr);
        }
    }
}

extern "C" int VR_AnimSmoothRows(int channel)
{
    return static_cast<int>(swingsOf(channel).size());
}

extern "C" void VR_AnimSmoothKeepClear(void)
{
    batches.kept.clear();
}

extern "C" int VR_AnimSmoothKeep(int channel, int first, int count)
{
    const za::Vector<glm::vec4>& swing = swingsOf(channel);
    if(trace.ent >= 0 || first < 0 || count < 0 || static_cast<za::SizeT>(first + count) > swing.size())
    {
        return -1; // (vr_anim_smooth_trace follows each draw: none skipped)
    }
    za::Vector<glm::vec4>& kept = batches.kept;
    const za::SizeT at = kept.size();
    kept.resize(at + static_cast<za::SizeT>(count));
    if(count)
    {
        memcpy(static_cast<void*>(kept.data() + at), swing.data() + first, sizeof(glm::vec4) * static_cast<size_t>(count));
    }
    return static_cast<int>(at);
}

extern "C" int VR_AnimSmoothReplay(int channel, int kept, int count, int newbatch)
{
    za::Vector<glm::vec4>& swing = swingsOf(channel);
    if(newbatch)
    {
        swing.clear();
    }
    const za::SizeT at = swing.size();
    if(count > 0)
    {
        swing.resize(at + static_cast<za::SizeT>(count));
        memcpy(static_cast<void*>(swing.data() + at), batches.kept.data() + kept, sizeof(glm::vec4) * static_cast<size_t>(count));
    }
    return static_cast<int>(at);
}

extern "C" int VR_AnimSmoothRowsEqual(int channel, int first, int kept, int count)
{
    const za::Vector<glm::vec4>& swing = swingsOf(channel);
    if(count <= 0)
    {
        return 1;
    }
    return memcmp(swing.data() + first, batches.kept.data() + kept, sizeof(glm::vec4) * static_cast<size_t>(count)) == 0;
}

extern "C" int VR_AnimSmooth(const entity_t* e, const void* aliashdr, int pose1, int pose2, float blend, int zeroblend,
    int batchindex, int channel, int smooth[4], float weights[4])
{
    const double t0 = Sys_DoubleTime();
    int on = smoothInstance(e, aliashdr, pose1, pose2, blend, zeroblend, batchindex, channel, smooth, weights);
    on = kickInstance(e, aliashdr, pose1, pose2, blend, zeroblend, channel, on, smooth, weights);
    stats.calls++;
    stats.seconds += Sys_DoubleTime() - t0;
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
    RestBuffer& rest = restBuffers.models[e->model];
    if(!m.restUploaded || !rest.buf || rest.size != restSize)
    {
        if(rest.buf)
        {
            GL_DeleteBuffer(rest.buf);
        }
        rest.buf = GL_CreateBuffer(GL_SHADER_STORAGE_BUFFER, GL_STATIC_DRAW, "vr anim smooth rest", static_cast<size_t>(restSize),
            m.rest.data());
        rest.size = restSize;
        m.restUploaded = true;
    }
    GLuint swingBuf;
    GLbyte* swingOfs;
    const GLsizeiptr swingSize = static_cast<GLsizeiptr>(swing.size() * sizeof(glm::vec4));
    GL_Upload(GL_SHADER_STORAGE_BUFFER, swing.data(), static_cast<size_t>(swingSize), &swingBuf, &swingOfs);
    GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 8, rest.buf, 0, restSize);
    GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 9, swingBuf, reinterpret_cast<GLintptr>(swingOfs), swingSize);
}

// R_SetupAliasFrame's end: a pinned entity's lerp (vr_anim_smooth_pin); false if it isn't the pinned one.
extern "C" int VR_AnimSmoothPinned(const entity_t* e, const void* aliashdr, short* pose1, short* pose2, float* blend)
{
    if(rigstudio::pinned(e, aliashdr, pose1, pose2, blend))
    {
        return 1; // (the Rig Studio's subject: drawn at its timeline's frame)
    }
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

namespace qvr::animsmooth
{

void registerCommands()
{
    registerDiagCommands(); // vr_anim_smooth_pin, _info, _check, _trace (vr_animsmooth_diag.cpp)
}

} // namespace qvr::animsmooth
