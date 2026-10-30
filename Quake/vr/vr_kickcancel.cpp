#include "vr_alloccount.h"
// vr_kickcancel.cpp -- see vr_kickcancel.hpp.

#include "vr_kickcancel.hpp"
#include "vr_anchor.hpp"
#include "vr_cvars.hpp"
#include "vr_mdlbuild.hpp"
#include "vr_mem.hpp"
#include "vr_rigpose.hpp"
#include "vr_testkit.hpp"
#include "vr_units.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string.h>

namespace qvr::kickcancel
{
namespace
{

using weapons::Key;

// The weapons whose models kick in their firing frames: the shotgun, super shotgun, nailgun, super nailgun, grenade and
// rocket launchers, lightning gun; the laser cannon and proximity gun (Scourge of Armagon); the lava nailguns, multi
// grenade and rocket launchers and plasma gun (Dissolution of Eternity).
// (Not the Widowmaker's: its own model's frames, though numbered as the super shotgun's, turn its barrels only.)
constexpr int bakedSlots[] = {1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15};

// A body vertex's residual allowed by the fit, in the model's byte steps (each pose's and the rest pose's rounding, and
// a little turn between them); the robust fit's random triples, the vertices it scores them on, its refits.
constexpr float floorSteps = 3.f;
constexpr int triples = 96;
constexpr za::SizeT maxSample = 600;
constexpr int trimPasses = 3;
constexpr float maxRoll = 8.f * (3.14159265f / 180.f); // radians a kick may roll the gun (kickLike)
// The check's: a body vertex drawn this many byte steps from where the rest pose draws it at most, and its centre moving
// this much from one drawn frame to the next (the body's poses are the rest pose's own bytes: only float rounding).
constexpr float jitterSteps = 0.02f;
// A pose whose cancel moves no point of the gun more than this (model units) is at rest: no cancel drawn.
constexpr float restMotion = 1e-3f;

// One pose's cancel: the rigid motion (model space: scale and scale_origin applied) taking the body as posed back to
// the rest pose, and how far its body's vertices stay from rest after it (model units).
struct PoseFit
{
    glm::quat rot{1.f, 0.f, 0.f, 0.f};
    glm::vec3 pos{0.f};
    bool rest{true};
    float maxErr{0.f};
    float rmsErr{0.f};
};

// A model's fit (by the name it was made for: a model's slot is reused for another after a game change). `listed`:
// one of bakedSlots' models with Quake vertices; the rest have no poses.
struct ModelKick
{
    za::String name;
    bool listed{false};
    int slot{-1};
    int numverts{0};
    float step{0.f};               // the largest byte step (model units)
    za::Vector<PoseFit> poses;     // every pose's
    za::Vector<unsigned char> body; // each vertex: 1 the gun's body (fitted), 0 a part moving on it
    int bodyCount{0};
    // Every pose's vertices as the model has them, and with the kick taken out (ensure: the one the model's data holds,
    // by vr_weapon_kick_cancel). Empty until the model is first made ready (ensure).
    za::Vector<trivertx_t> original;
    za::Vector<trivertx_t> cancelled;
    int probe{-1}; // the first pose the cancel changes (-1: none): whose bytes tell which the data holds (ensure)
};
[[nodiscard]] za::SizeT heldBytes(const ModelKick& m) // (vr_mem.hpp)
{
    return mem::heldBytes(m.name) + mem::heldBytes(m.poses) + mem::heldBytes(m.body) + mem::heldBytes(m.original) +
           mem::heldBytes(m.cancelled);
}

// Model -> its fit (the main thread: the view setup, the renderer, the anchors). Never emptied on the models' events: a
// reloaded model's data is found to be neither of its entry's (ensure) and fitted again; an entry dropped while the data
// still held its cancelled poses would fit those (no kick found, the originals lost).
struct KickCache
{
    ankerl::unordered_dense::map<const qmodel_t*, ModelKick> models;
    auto members() { return mem::list(models); }
};
mem::Cache<KickCache> kickCache{"kick cancel", mem::Never};

// The fit's buffers (made once a model, the main thread).
struct FitScratch
{
    za::Vector<glm::vec3> rest;  // the rest pose (model units)
    za::Vector<glm::vec3> posed; // the pose fitted
    za::Vector<glm::vec3> a;     // the kept vertices, posed
    za::Vector<glm::vec3> b;     // and at rest
    za::Vector<float> err;       // each vertex's residual
    za::Vector<float> weight;    // each vertex's say in the fit (near the hand: more)
    za::Vector<unsigned char> keep; // this pose's kept vertices
    za::Vector<glm::vec3> drawnRest; // drawnBody's (byte space)
    za::Vector<glm::vec3> drawnNow;
    auto members() { return mem::list(rest, posed, a, b, err, weight, keep, drawnRest, drawnNow); }
};
mem::Scratch<FitScratch> scratch{"kick cancel fit"};

[[nodiscard]] bool listedSlot(int slot)
{
    for(const int s : bakedSlots)
    {
        if(s == slot)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] const aliashdr_t* quakeHeader(const qmodel_t* model)
{
    if(!model || model->type != mod_alias)
    {
        return nullptr;
    }
    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || !hdr->vertexes || hdr->numverts <= 0 || hdr->numposes <= 0)
    {
        return nullptr;
    }
    return hdr;
}

// Pose `pose`'s vertices in model units (the stored bytes, scale and scale_origin applied): the model's data now, or
// `from` (every pose's, as the model's data holds them).
void poseVertices(const aliashdr_t* hdr, int pose, za::Vector<glm::vec3>& out, const trivertx_t* from = nullptr)
{
    const trivertx_t* verts =
        from ? from : reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const trivertx_t* v = verts + static_cast<za::SizeT>(pose) * hdr->numverts;
    out.resize(hdr->numverts);
    for(int i = 0; i < hdr->numverts; i++)
    {
        for(int k = 0; k < 3; k++)
        {
            out[i][k] = hdr->scale_origin[k] + hdr->scale[k] * static_cast<float>(v[i].v[k]);
        }
    }
}

// The rigid motion taking `posed` to `rest` over the vertices marked in `use`.
[[nodiscard]] rigpose::Rigid fitOver(const za::Vector<glm::vec3>& posed, const za::Vector<glm::vec3>& rest,
    const za::Vector<unsigned char>& use)
{
    za::Vector<glm::vec3>& a = scratch.a;
    za::Vector<glm::vec3>& b = scratch.b;
    a.clear();
    b.clear();
    for(za::SizeT i = 0; i < posed.size(); i++)
    {
        if(use[i])
        {
            a.pushBack(posed[i]);
            b.pushBack(rest[i]);
        }
    }
    return rigpose::fitRigid(a.data(), b.data(), static_cast<int>(a.size()));
}

// Each vertex's distance from rest after `r` (into scratch.err).
void residuals(const rigpose::Rigid& r, const za::Vector<glm::vec3>& posed, const za::Vector<glm::vec3>& rest)
{
    za::Vector<float>& err = scratch.err;
    err.resize(posed.size());
    for(za::SizeT i = 0; i < posed.size(); i++)
    {
        err[i] = glm::distance(rigpose::apply(r, posed[i]), rest[i]);
    }
}

// Whether `r` could be a gun's kick: back, up, a little sideways, hardly any roll about its length (+x). A spinning
// drum or Gatling barrels turn about it, and may be as much of the model as the body is (the super nailgun's, the
// multi grenade launcher's).
[[nodiscard]] bool kickLike(const rigpose::Rigid& r)
{
    const float roll = 2.f * za::atan2(za::abs(r.rot.x), za::abs(r.rot.w));
    return roll <= maxRoll;
}

// How much of every `stride`th vertex `r` brings within `tol` of rest, each by its weight (scratch.weight).
[[nodiscard]] float inliers(const rigpose::Rigid& r, const za::Vector<glm::vec3>& posed, const za::Vector<glm::vec3>& rest,
    float tol, za::SizeT stride)
{
    float n = 0.f;
    const float tol2 = tol * tol;
    for(za::SizeT i = 0; i < posed.size(); i += stride)
    {
        const glm::vec3 d = rigpose::apply(r, posed[i]) - rest[i];
        n += glm::dot(d, d) <= tol2 ? scratch.weight[i] : 0.f;
    }
    return n;
}

// The vertices of `posed` that move with the gun (into scratch.keep), whatever share of the model its parts are (the
// lava nailguns' turning barrels are half of it): the rigid motion a sample of vertices agree on most (within `tol`;
// those near the hand count most: scratch.weight) among the fit over all, none, `previous` (the last pose's) and the
// fits of random triples (deterministic), then refitted over those it brings within `tol` a few times (a pump, barrels
// or a flash moving on the gun fall out).
void robustBody(const za::Vector<glm::vec3>& posed, const za::Vector<glm::vec3>& rest, float tol,
    const rigpose::Rigid& previous)
{
    const za::SizeT n = posed.size();
    const za::SizeT stride = za::max<za::SizeT>(1, n / maxSample);
    za::Vector<unsigned char>& keep = scratch.keep;
    keep.clear();
    keep.resize(n, 1);
    rigpose::Rigid best;
    float bestScore = inliers(best, posed, rest, tol, stride);
    const auto consider = [&](const rigpose::Rigid& r)
    {
        if(!kickLike(r))
        {
            return; // (a drum or barrels turning about the gun's length: a part)
        }
        const float c = inliers(r, posed, rest, tol, stride);
        if(c > bestScore)
        {
            bestScore = c;
            best = r;
        }
    };
    consider(fitOver(posed, rest, keep));
    consider(previous);
    uint32_t seed = 0x9E3779B9u;
    for(int t = 0; t < triples && n >= 3; t++)
    {
        glm::vec3 a[3], b[3];
        for(int k = 0; k < 3; k++)
        {
            seed = seed * 1664525u + 1013904223u;
            const za::SizeT i = (seed >> 8) % n;
            a[k] = posed[i];
            b[k] = rest[i];
        }
        consider(rigpose::fitRigid(a, b, 3));
    }
    for(int pass = 0; pass < trimPasses; pass++)
    {
        residuals(best, posed, rest);
        for(za::SizeT i = 0; i < n; i++)
        {
            keep[i] = scratch.err[i] <= tol ? 1 : 0;
        }
        best = fitOver(posed, rest, keep);
    }
    residuals(best, posed, rest);
    for(za::SizeT i = 0; i < n; i++)
    {
        keep[i] = scratch.err[i] <= tol ? 1 : 0;
    }
}

// Where the hand holds `model` in its model space (scale and scale_origin applied, Scale 1): the drawn weapon's
// origin, its offset's point (vr_weapons.hpp hotspots: S(k) T(Offset) before T(scale_origin) S(scale) S(Scale)). False
// out of a Quake VR game (no offsets).
[[nodiscard]] bool gripPoint(const qmodel_t* model, const aliashdr_t* hdr, glm::vec3& out)
{
    const weapons::ModelTransform t = weapons::modelTransform(model);
    if(!t.active || t.scale.x <= 0.f)
    {
        return false;
    }
    const glm::vec3 so{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
    out = so + (-t.offset - so) / t.scale.x;
    return true;
}

// The fit of `model` (its slot `slot`, listed): the body (the vertices kept by every pose's trimmed fit; if too few,
// every vertex), then each pose's cancel over it.
void fit(const qmodel_t* model, const aliashdr_t* hdr, ModelKick& m)
{
    m.numverts = hdr->numverts;
    m.step = za::max(hdr->scale[0], za::max(hdr->scale[1], hdr->scale[2]));
    za::Vector<glm::vec3>& rest = scratch.rest;
    za::Vector<glm::vec3>& posed = scratch.posed;
    poseVertices(hdr, hdr->frames[0].firstpose, rest);
    // Each vertex's say in which motion is the gun's: the nearer the hand, the more (the body is what the hand holds:
    // a launcher's barrel moving on it is a part); all alike out of a game.
    za::Vector<float>& weight = scratch.weight;
    weight.clear();
    weight.resize(hdr->numverts, 1.f);
    if(glm::vec3 grip; gripPoint(model, hdr, grip))
    {
        float extent = 0.f;
        for(const glm::vec3& v : rest)
        {
            extent = za::max(extent, glm::distance(v, grip));
        }
        const float near = za::max(0.2f * extent, 1e-3f);
        for(int i = 0; i < hdr->numverts; i++)
        {
            const float d = glm::distance(rest[i], grip) / near;
            weight[i] = 1.f / (1.f + d * d);
        }
    }
    m.body.clear();
    m.body.resize(hdr->numverts, 1);
    rigpose::Rigid previous;
    for(int p = 0; p < hdr->numposes; p++)
    {
        poseVertices(hdr, p, posed);
        robustBody(posed, rest, floorSteps * m.step, previous);
        previous = fitOver(posed, rest, scratch.keep);
        for(int i = 0; i < hdr->numverts; i++)
        {
            m.body[i] = m.body[i] && scratch.keep[i] ? 1 : 0;
        }
    }
    m.bodyCount = 0;
    for(const unsigned char b : m.body)
    {
        m.bodyCount += b;
    }
    if(m.bodyCount < za::max(8, hdr->numverts / 5))
    {
        Con_DPrintf("kick cancel: %s: only %d of %d vertices kept their places: fitted over all\n", m.name.cStr(),
            m.bodyCount, hdr->numverts);
        for(unsigned char& b : m.body)
        {
            b = 1;
        }
        m.bodyCount = hdr->numverts;
    }

    // The body's extent: a turn's motion measured at its farthest vertex.
    float reach = 0.f;
    glm::vec3 centre{0.f};
    for(int i = 0; i < hdr->numverts; i++)
    {
        centre += m.body[i] ? rest[i] : glm::vec3{0.f};
    }
    centre /= static_cast<float>(m.bodyCount);
    for(int i = 0; i < hdr->numverts; i++)
    {
        reach = m.body[i] ? za::max(reach, glm::distance(rest[i], centre)) : reach;
    }

    m.poses.clear();
    m.poses.resize(hdr->numposes);
    for(int p = 0; p < hdr->numposes; p++)
    {
        poseVertices(hdr, p, posed);
        const rigpose::Rigid r = fitOver(posed, rest, m.body);
        residuals(r, posed, rest);
        PoseFit& f = m.poses[p];
        f.rot = r.rot;
        f.pos = r.pos;
        float worst = 0.f, sum = 0.f;
        for(int i = 0; i < hdr->numverts; i++)
        {
            if(m.body[i])
            {
                worst = za::max(worst, scratch.err[i]);
                sum += scratch.err[i] * scratch.err[i];
            }
        }
        f.maxErr = worst;
        f.rmsErr = za::sqrt(sum / static_cast<float>(m.bodyCount));
        const float turn = 2.f * za::sqrt(za::max(0.f, 1.f - za::min(1.f, za::abs(r.rot.w)) * za::abs(r.rot.w)));
        f.rest = glm::length(rigpose::apply(r, centre) - centre) + turn * reach < restMotion;
    }
}

[[nodiscard]] trivertx_t* posesOf(const aliashdr_t* hdr)
{
    return reinterpret_cast<trivertx_t*>(const_cast<byte*>(reinterpret_cast<const byte*>(hdr)) + hdr->vertexes);
}

[[nodiscard]] za::SizeT poseBytes(const aliashdr_t* hdr)
{
    return static_cast<za::SizeT>(hdr->numposes) * static_cast<za::SizeT>(hdr->numverts) * sizeof(trivertx_t);
}

// Whether every vertex of slot `slot`'s model is drawn at rest through its kick, its parts too: the grenade launchers
// (the grenade launcher, Armagon's proximity gun, Dissolution's multi grenade launcher), whose upper shells only lurch
// on their bodies as they kick (no mechanism of theirs works there: drawn moving, they went through the ammo screen).
[[nodiscard]] bool wholeAtRest(int slot)
{
    return slot < 0; // (none yet)
}

// `m.cancelled` from `m.original` (the model's own poses): each kicked pose with its kick taken out. The body's
// vertices are the rest pose's (exactly: no byte's rounding left to shimmer, the lighting's normals the same), a part's
// are its pose's put through the pose's cancel (rounded to the bytes again, its normal turned with it); a pose at rest
// is the model's own.
void buildCancelled(const aliashdr_t* hdr, ModelKick& m)
{
    const int restPose = hdr->frames[0].firstpose;
    const za::SizeT nv = static_cast<za::SizeT>(hdr->numverts);
    const trivertx_t* rest = m.original.data() + static_cast<za::SizeT>(restPose) * nv;
    const glm::vec3 so{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
    const glm::vec3 s{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
    const bool whole = wholeAtRest(m.slot);
    m.cancelled = m.original;
    m.probe = -1;
    for(int p = 0; p < hdr->numposes; p++)
    {
        const PoseFit& f = m.poses[static_cast<za::SizeT>(p)];
        if(f.rest || p == restPose)
        {
            continue;
        }
        trivertx_t* out = m.cancelled.data() + static_cast<za::SizeT>(p) * nv;
        const rigpose::Rigid r{f.rot, f.pos};
        for(za::SizeT i = 0; i < nv; i++)
        {
            if(whole || m.body[i])
            {
                out[i] = rest[i];
                continue;
            }
            const trivertx_t& v = out[i];
            const glm::vec3 at = rigpose::apply(r, so + s * glm::vec3{v.v[0], v.v[1], v.v[2]});
            const glm::vec3 b = (at - so) / s;
            const float* n = r_avertexnormals[v.lightnormalindex % NUMVERTEXNORMALS];
            trivertx_t moved;
            for(int k = 0; k < 3; k++)
            {
                moved.v[k] = static_cast<byte>(za::clamp(static_cast<int>(b[k] + 0.5f), 0, 255));
            }
            moved.lightnormalindex = mdlbuild::nearestNormal(f.rot * glm::vec3{n[0], n[1], n[2]});
            out[i] = moved;
        }
    }
    for(int p = 0; p < hdr->numposes && m.probe < 0; p++)
    {
        const za::SizeT at = static_cast<za::SizeT>(p) * nv;
        m.probe = memcmp(m.cancelled.data() + at, m.original.data() + at, nv * sizeof(trivertx_t)) != 0 ? p : -1;
    }
}

// The kick cancel's state of the model's data now (by `m`).
enum class Held
{
    Original,
    Cancelled,
    Other, // neither: the model read again (another game's, a reloaded file) or never seen
};

// Read on the probe pose alone (a model is read again whole, never one pose of it; once a frame for the held guns), the
// whole data compared only where that pose is neither.
[[nodiscard]] Held heldNow(const aliashdr_t* hdr, const ModelKick& m)
{
    const za::SizeT bytes = poseBytes(hdr);
    if(m.original.size() * sizeof(trivertx_t) != bytes || m.probe < 0)
    {
        return Held::Other;
    }
    const za::SizeT nv = static_cast<za::SizeT>(hdr->numverts), at = static_cast<za::SizeT>(m.probe) * nv;
    const za::SizeT poseSize = nv * sizeof(trivertx_t);
    if(memcmp(posesOf(hdr) + at, m.cancelled.data() + at, poseSize) == 0)
    {
        return Held::Cancelled;
    }
    if(memcmp(posesOf(hdr) + at, m.original.data() + at, poseSize) == 0)
    {
        return memcmp(posesOf(hdr), m.original.data(), bytes) == 0 ? Held::Original : Held::Other;
    }
    return Held::Other;
}

// The fit of `model` made from its data now (its own poses: a fresh entry).
[[nodiscard]] ModelKick& refit(const qmodel_t* model, const aliashdr_t* hdr)
{
    ModelKick& m = kickCache.models[model];
    m = ModelKick{};
    m.name = model->name;
    m.numverts = hdr->numverts;
    m.slot = weapons::slotForModel(model);
    m.listed = listedSlot(m.slot);
    if(m.listed)
    {
        const double start = Sys_DoubleTime();
        fit(model, hdr, m);
        m.original.resize(static_cast<za::SizeT>(hdr->numposes) * static_cast<za::SizeT>(hdr->numverts));
        memcpy(m.original.data(), posesOf(hdr), poseBytes(hdr));
        buildCancelled(hdr, m);
        Con_DPrintf("kick cancel: %s fitted (%d poses, body %d of %d vertices) in %.1f ms\n", m.name.cStr(),
            hdr->numposes, m.bodyCount, m.numverts, (Sys_DoubleTime() - start) * 1000.0);
    }
    return m;
}

// `model`'s data made to hold its cancelled poses (vr_weapon_kick_cancel; 0: its own), fitted first if it is new, and
// uploaded again when that changed it (its dequantised offsets made again for the poses it now has).
void ensure(qmodel_t* model, aliashdr_t* hdr)
{
    ModelKick* m = nullptr;
    if(const auto it = kickCache.models.find(model); it != kickCache.models.end() && it->second.name == model->name &&
                                                      it->second.numverts == hdr->numverts)
    {
        m = &it->second;
        if(!m->listed)
        {
            return; // (not a kicking gun's model)
        }
        if(m->probe < 0 && m->original.size() * sizeof(trivertx_t) == poseBytes(hdr))
        {
            return; // (no pose the cancel changes)
        }
    }
    Held held = m ? heldNow(hdr, *m) : Held::Other;
    if(held == Held::Other)
    {
        m = &refit(model, hdr);
        held = Held::Original;
    }
    if(!m->listed || m->probe < 0)
    {
        return;
    }
    const bool want = vr_weapon_kick_cancel.value != 0.f;
    if((held == Held::Cancelled) == want)
    {
        return;
    }
    memcpy(posesOf(hdr), (want ? m->cancelled : m->original).data(), poseBytes(hdr));
    hdr->dequantready = 0;
    if(model->meshvbo)
    {
        GLMesh_LoadVertexBuffer(model, hdr);
    }
}

// `model`'s fit, its data made ready first (ensure; nullptr: none, not a listed weapon's model).
[[nodiscard]] const ModelKick* kickOf(const qmodel_t* model, const aliashdr_t* hdr)
{
    ensure(const_cast<qmodel_t*>(model), const_cast<aliashdr_t*>(hdr));
    const auto it = kickCache.models.find(model);
    if(it == kickCache.models.end())
    {
        return nullptr;
    }
    const ModelKick& m = it->second;
    return m.listed && static_cast<int>(m.poses.size()) == hdr->numposes ? &m : nullptr;
}

// The baked motion of pose `p` (the cancel's inverse), as the recoil settings read it: cm the body's centre goes back
// (model space -x; drawn at the slot's scale), degrees its muzzle tips up (+x towards +z).
struct Motion
{
    float backUnits{0.f};
    float riseDeg{0.f};
};
[[nodiscard]] Motion motionOf(const PoseFit& f, const glm::vec3& centre)
{
    const glm::quat inv = glm::inverse(f.rot);
    const glm::vec3 moved = inv * (centre - f.pos);
    const glm::vec3 fwd = inv * glm::vec3{1.f, 0.f, 0.f};
    return {centre.x - moved.x, za::atan2(fwd.z, fwd.x) * (180.f / 3.14159265f)};
}

// A model's rest centre of its body (model units).
[[nodiscard]] glm::vec3 bodyCentre(const aliashdr_t* hdr, const ModelKick& m)
{
    za::Vector<glm::vec3>& rest = scratch.rest;
    poseVertices(hdr, hdr->frames[0].firstpose, rest);
    glm::vec3 c{0.f};
    for(int i = 0; i < hdr->numverts; i++)
    {
        c += m.body[i] ? rest[i] : glm::vec3{0.f};
    }
    return c / static_cast<float>(za::max(m.bodyCount, 1));
}

// Model units to drawn cm at the slot's size.
[[nodiscard]] float cmPerUnit(const qmodel_t* model)
{
    const weapons::ModelTransform t = weapons::modelTransform(model);
    const float k = t.active ? t.k * t.scale.x : 1.f;
    return k * 100.f / units::metresToUnits();
}

// The pose of frame `f` (its first).
[[nodiscard]] const char* frameOfPose(const aliashdr_t* hdr, int pose)
{
    for(int f = 0; f < hdr->numframes; f++)
    {
        if(pose >= hdr->frames[f].firstpose && pose < hdr->frames[f].firstpose + hdr->frames[f].numposes)
        {
            return hdr->frames[f].name;
        }
    }
    return "?";
}

// How many of the vertices left out of the body move (more than the tolerance from rest in some cancelled pose): the
// parts (none of a model drawn whole at rest).
[[nodiscard]] int movingParts(const aliashdr_t* hdr, const ModelKick& m)
{
    za::Vector<glm::vec3>& rest = scratch.rest;
    za::Vector<glm::vec3>& posed = scratch.posed;
    poseVertices(hdr, hdr->frames[0].firstpose, rest, m.cancelled.data());
    za::Vector<unsigned char>& moved = scratch.keep;
    moved.clear();
    moved.resize(hdr->numverts, 0);
    for(int p = 0; p < hdr->numposes; p++)
    {
        poseVertices(hdr, p, posed, m.cancelled.data());
        for(int i = 0; i < hdr->numverts; i++)
        {
            if(!m.body[i] && glm::distance(posed[i], rest[i]) > floorSteps * m.step)
            {
                moved[i] = 1;
            }
        }
    }
    int n = 0;
    for(const unsigned char v : moved)
    {
        n += v;
    }
    return n;
}

void printFit(const qmodel_t* model)
{
    const aliashdr_t* hdr = quakeHeader(model);
    const ModelKick* m = hdr ? kickOf(model, hdr) : nullptr;
    if(!m)
    {
        Con_Printf("%s: not a listed weapon's model (no kick cancel)\n", model->name);
        return;
    }
    // The motion measured where the hand holds it (the model's point the drawn weapon turns about: its offset's), as the
    // programmatic recoil moves it; out of a game, at its body's centre.
    glm::vec3 grip;
    if(!gripPoint(model, hdr, grip))
    {
        grip = bodyCentre(hdr, *m);
    }
    const float cm = cmPerUnit(model);
    Con_Printf("%s (slot %d): body %d of %d vertices, %d moving parts' vertices; byte step %.3f; %.2f cm a unit; "
               "measured at %.1f %.1f %.1f\n",
        model->name, m->slot, m->bodyCount, m->numverts, movingParts(hdr, *m), m->step, cm, grip.x, grip.y, grip.z);
    {
        // Where the body and the rest are at rest (model units), to see which is which.
        za::Vector<glm::vec3>& rest = scratch.rest;
        poseVertices(hdr, hdr->frames[0].firstpose, rest);
        glm::vec3 lo[2]{glm::vec3{1e9f}, glm::vec3{1e9f}}, hi[2]{glm::vec3{-1e9f}, glm::vec3{-1e9f}};
        for(int i = 0; i < hdr->numverts; i++)
        {
            const int k = m->body[i] ? 0 : 1;
            lo[k] = glm::min(lo[k], rest[i]);
            hi[k] = glm::max(hi[k], rest[i]);
        }
        Con_Printf("  body %.1f %.1f %.1f .. %.1f %.1f %.1f; the rest %.1f %.1f %.1f .. %.1f %.1f %.1f\n", lo[0].x, lo[0].y,
            lo[0].z, hi[0].x, hi[0].y, hi[0].z, lo[1].x, lo[1].y, lo[1].z, hi[1].x, hi[1].y, hi[1].z);
    }
    for(int p = 0; p < hdr->numposes; p++)
    {
        const PoseFit& f = m->poses[p];
        const Motion mo = motionOf(f, grip);
        Con_Printf("  pose %2d %-10s back %6.3f u (%5.2f cm) rise %6.2f deg; body after cancel: max %.3f rms %.3f%s\n", p,
            frameOfPose(hdr, p), mo.backUnits, mo.backUnits * cm, mo.riseDeg, f.maxErr, f.rmsErr, f.rest ? " (rest)" : "");
    }
}

// vr_kickcancel_info [model]: the fit of a model (a name; none: the main hand's weapon).
void info_f()
{
    const qmodel_t* model = nullptr;
    if(Cmd_Argc() >= 2)
    {
        model = Mod_ForName(Cmd_Argv(1), false);
    }
    else if(const int slot = weapons::heldSlot(0); slot >= 0)
    {
        model = Mod_ForName(weapons::cvar(slot, Key::ID)->string, false);
    }
    if(!model)
    {
        Con_Printf("usage: vr_kickcancel_info [model] (none: the main hand's weapon)\n");
        return;
    }
    printFit(model);
}

// vr_kickcancel_test: kickcancel::check's verdict alone.
void test_f()
{
    testkit::Tally t;
    check(t);
    t.verdict("vr_kickcancel_test");
}

} // namespace

bool drawnBody(const entity_t& e, glm::vec3& now, glm::vec3& rest, float& worst)
{
    const aliashdr_t* hdr = quakeHeader(e.model);
    const ModelKick* m = hdr ? kickOf(e.model, hdr) : nullptr;
    za::Vector<glm::vec3>& r = scratch.drawnRest;
    za::Vector<glm::vec3>& n = scratch.drawnNow;
    if(!m || !anchor::posedVertices(e, 0.f, r, n))
    {
        return false;
    }
    const glm::vec3 scale{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
    now = rest = glm::vec3{0.f};
    worst = 0.f;
    const bool whole = wholeAtRest(m->slot);
    for(int i = 0; i < hdr->numverts; i++)
    {
        if(m->body[i])
        {
            now += n[i];
            rest += r[i];
        }
        if(m->body[i] || whole)
        {
            worst = za::max(worst, glm::length((n[i] - r[i]) * scale));
        }
    }
    const float count = static_cast<float>(za::max(m->bodyCount, 1));
    const glm::vec3 so{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
    now = now / count * scale + so;
    rest = rest / count * scale + so;
    return true;
}

void prepare(const qmodel_t* model)
{
    if(const aliashdr_t* hdr = quakeHeader(model))
    {
        ensure(const_cast<qmodel_t*>(model), const_cast<aliashdr_t*>(hdr));
    }
}

void check(testkit::Tally& t)
{
    constexpr const char* name = "vr_kickcancel_test";
    int models = 0;
    for(const int slot : bakedSlots)
    {
        const char* modelName = weapons::cvar(slot, Key::ID)->string;
        const qmodel_t* model = Mod_ForName(modelName, false);
        const aliashdr_t* hdr = quakeHeader(model);
        if(!hdr)
        {
            Con_Printf("%s: slot %d's %s not here (skipped)\n", name, slot, modelName);
            continue;
        }
        const ModelKick* m = kickOf(model, hdr);
        if(!t.check(m != nullptr, name, va("slot %d's %s has no fit", slot, modelName)))
        {
            continue;
        }
        models++;

        // Its firing frames (1 up to its last kicked one, then back to 0) played as the game plays them, a frame each
        // 0.1 s, and drawn through the anchors' path as the renderer blends them, at 72 and 90 frames a second: each drawn
        // frame's body (every vertex for one drawn whole at rest) at rest, and its centre not moving from one drawn frame
        // to the next (a step there is jitter on the hand's smooth recoil).
        int last = 0, movedPoses = 0;
        float kicked = 0.f;
        for(int f = 1; f < hdr->numframes; f++)
        {
            const PoseFit& pf = m->poses[hdr->frames[f].firstpose];
            if(!pf.rest)
            {
                last = f;
                movedPoses++;
                kicked = za::max(kicked, za::abs(motionOf(pf, bodyCentre(hdr, *m)).backUnits) / m->step);
            }
        }
        float worst = 0.f, jump = 0.f, screen = 0.f;
        int drawn = 0;
        // (its ammo screen's anchor: where the screen is put on the gun as drawn, vr_view_weapon.cpp queueWeaponText)
        const char* screenAnchor = weapons::anchor(slot, Key::WpnTextAnchor);
        for(const float rate : {72.f, 90.f})
        {
            entity_t e{};
            e.model = const_cast<qmodel_t*>(model);
            const glm::vec3 screenRest = anchor::posedAnchor(e, screenAnchor, 0.f);
            glm::vec3 previous{0.f};
            const int count = static_cast<int>(0.1f * static_cast<float>(last + 2) * rate);
            for(int k = 0; k <= count; k++)
            {
                const float time = static_cast<float>(k) / rate;
                const int step = static_cast<int>(time / 0.1f);
                const auto frameAt = [&](int s) { return s >= 0 && s < last ? s + 1 : 0; };
                e.frame = frameAt(step);
                e.currentpose = hdr->frames[e.frame].firstpose;
                e.previouspose = hdr->frames[frameAt(step - 1)].firstpose;
                e.lerptime = 0.1f;
                e.lerpstart = cl.time - static_cast<double>(time - 0.1f * static_cast<float>(step));
                glm::vec3 now, rest;
                float far = 0.f;
                if(!drawnBody(e, now, rest, far))
                {
                    continue;
                }
                drawn++;
                const glm::vec3 scale{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
                screen = za::max(screen, glm::length((anchor::posedAnchor(e, screenAnchor, 0.f) - screenRest) * scale) / m->step);
                worst = za::max(worst, far / m->step);
                jump = k > 0 ? za::max(jump, glm::distance(now, previous) / m->step) : jump;
                previous = now;
            }
        }
        t.check(drawn > 0 && worst <= jitterSteps, name,
            va("slot %d's %s: a body vertex %.3f byte steps from rest as drawn through its firing frames (want <= %.2f)",
                slot, modelName, worst, jitterSteps));
        t.check(drawn > 0 && jump <= jitterSteps, name,
            va("slot %d's %s: its body's centre moved %.3f byte steps between two drawn frames (want <= %.2f)", slot,
                modelName, jump, jitterSteps));
        // (Where it sits on a part it rides that part: the laser cannon's on its sliding barrel.)
        t.check(drawn > 0 && (screen <= jitterSteps || !wholeAtRest(slot)), name,
            va("slot %d's %s: its ammo screen's anchor %.3f byte steps from rest as drawn through its firing frames (want "
               "<= %.2f)", slot, modelName, screen, jitterSteps));
        t.check(m->bodyCount * 5 >= m->numverts, name,
            va("slot %d's %s: the body only %d of %d vertices", slot, modelName, m->bodyCount, m->numverts));
        Con_Printf("%s: slot %2d %-22s body %4d/%4d%s, %3d parts' vertices moving, %2d kicked poses (back up to %.1f steps), "
                   "%d frames drawn at 72 and 90 Hz: body within %.3f steps of rest, its centre's step %.3f, screen %.3f\n",
            name, slot, modelName, m->bodyCount, m->numverts, wholeAtRest(slot) ? " (whole at rest)" : "",
            movingParts(hdr, *m), movedPoses, kicked, drawn, worst, jump, screen);
    }
    t.check(models > 0, name, "no listed weapon's model found");
}

void registerCommands()
{
    Cmd_AddCommand("vr_kickcancel_info", info_f);
    Cmd_AddCommand("vr_kickcancel_test", test_f);
}

} // namespace qvr::kickcancel
