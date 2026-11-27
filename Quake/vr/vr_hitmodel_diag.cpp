// vr_hitmodel_diag.cpp -- vr_hitmodel_stats, vr_hitmodel_check, vr_hitmodel_bench and vr_hitzones_check: the hit models' reports and tests.
// Split from vr_hitmodel.cpp (a pure move); what they share is in vr_hitmodel_internal.hpp.

#include "vr_geom.hpp"
#include "vr_hitmodel_internal.hpp"
#include "vr_animsmooth.hpp"
#include "vr_ad.hpp" // AD support: small pellets retain precise model/corpse impacts.
#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "vr_held.hpp"
#include "vr_jobs.hpp"
#include "vr_lines.hpp"
#include "vr_modelcollide.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_props.hpp"
#include "vr_mem.hpp"
#include "vr_ragdoll.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Algorithm/Iota.hpp"
#include "Zancle/Algorithm/NthElement.hpp"
#include "Zancle/Base/BitCast.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/Pair.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"
#include "vr_zancle.hpp"

namespace qvr::hitmodel
{
using namespace detail;


void zonesCheck_f()
{
    if(!sv.active || cls.state != ca_connected)
    {
        Con_Printf("vr_hitzones_check: no local game\n"); return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    const func_t fn = progs::findFunction("PositionalPointRegion");
    int checked = 0, mismatch = 0, boundary = 0;
    if(fn)
    {
        for(const auto& sample : zoneSamples)
        {
            if(sample.num <= 0 || sample.num >= qcvm->num_edicts) { continue; }
            edict_t* ent = EDICT_NUM(sample.num);
            if(ent->free) { continue; }
            const float yaw[3]{0.f, ent->v.angles[1], 0.f};
            const glm::vec3 rest = vec(ent->v.origin) + held::axesFromAngles(yaw, false) * sample.rest;
            G_INT(OFS_PARM0) = EDICT_TO_PROG(ent);
            G_FLOAT(OFS_PARM1 + 0) = rest.x; G_FLOAT(OFS_PARM1 + 1) = rest.y; G_FLOAT(OFS_PARM1 + 2) = rest.z;
            G_FLOAT(OFS_PARM2) = static_cast<float>(sample.part); // (its triangle's body part: hitmodel_part's)
            PR_ExecuteProgram(fn);
            ++checked;
            if(static_cast<int>(G_FLOAT(OFS_RETURN)) != sample.region)
            {
                bool nearBoundary = false;
                for(const Zone& z : zones)
                {
                    if(z.num == sample.num && z.zone == 1 &&
                        za::fabs(glm::distance(sample.rest, z.lo) - z.radius) < .021f)
                    {
                        nearBoundary = true; break;
                    }
                }
                if(nearBoundary) { ++boundary; }
                else { ++mismatch; }
            }
        }
    }
    PR_PopQCVM(oldvm);
    Con_Printf("vr_hitzones_check: models=%d fallback=%d body=%d head=%d limbs=%d legs=%d decap_models=%d decap=%d samples=%d "
        "checked=%d mismatch=%d boundary=%d pose=%08x priority=%d xray=%d classifier=%d\n", zoneModels, zoneFallbacks,
        zonePieces[0], zonePieces[1], zonePieces[2], zonePieces[3], zoneDecapModels, zonePieces[4], int(zoneSamples.size()), checked, mismatch,
        boundary, unsigned(zonePoseHash), int(vr_hit_head_priority.value), int(vr_debug_hitzones_xray.value), int(fn != 0));
}

void stats_f()
{
    Con_Printf("precise hit detection: tolerances: guns %.1f, grapple %.1f, melee %.1f, thrown %.1f\n",
        tolerance(Class::Guns), tolerance(Class::Grapple), tolerance(Class::Melee), tolerance(Class::Thrown));
    Con_Printf("  %lld broad tests, %lld model tests: %lld hits (%lld started inside), %lld through the box\n", stats.clips,
        stats.narrow, stats.hits, stats.insides, stats.throughs);
    if(stats.narrow)
    {
        Con_Printf("  a model test: %.2f us, %.1f nodes, %.1f triangles\n", stats.ns / 1000. / static_cast<double>(stats.narrow),
            static_cast<double>(stats.nodes) / static_cast<double>(stats.narrow), static_cast<double>(stats.tris) / static_cast<double>(stats.narrow));
    }
    Con_Printf("  %lld hierarchies made at map loads, %.1f ms in all; %lld refit to skinned bodies\n", stats.builds, stats.buildMs,
        stats.refits);
    Con_Printf("  drawn shapes (vr_hitmodel_drawn %g): %lld made, %lld drawn (spline or rebuild), %.1f us each\n",
        static_cast<double>(vr_hitmodel_drawn.value), stats.shapes, stats.shapesDrawn,
        stats.shapes ? stats.shapeNs / 1000. / static_cast<double>(stats.shapes) : 0.);
    za::SizeT bytes = 0, count = 0;
    for(const auto& [name, mp] : meshes)
    {
        const Mesh& m = *mp;
        if(m.valid)
        {
            count++;
            bytes += m.bounds.size() * sizeof(Bounds) + m.tris.size() * sizeof(m.tris[0]) + m.nodes.size() * sizeof(Node);
        }
    }
    Con_Printf("  %zu models' hierarchies kept: %.2f MB\n", count, static_cast<double>(bytes) / (1024. * 1024.));
    if(Cmd_Argc() > 1 && !ZA_STRCMP(Cmd_Argv(1), "reset"))
    {
        stats = Stats{};
    }
}

// vr_hitmodel_check: each monster's model as the hit test poses it against the model as the client draws it this frame
// (vr_modelcollide.cpp's drawnTriangles: the renderer's lerp): the mean and the largest distance between the same
// vertices. Accumulated over the calls since `vr_hitmodel_check reset`; printed each call with `vr_hitmodel_check print`.
namespace
{

// vr_hitmodel_check's totals since its last `reset` (a debug command's own).
struct CheckTotals
{
    double sum{0.}, worst{0.}, sumRaw{0.}, worstRaw{0.};
    long long count{0}, calls{0};
    za::String worstName;
};
CheckTotals checkTotals;

} // namespace

void check_f()
{
    if(Cmd_Argc() > 1 && !ZA_STRCMP(Cmd_Argv(1), "reset"))
    {
        checkTotals = CheckTotals{};
        return;
    }
    auto& [sum, worst, sumRaw, worstRaw, count, calls, worstName] = checkTotals;
    if(!sv.active || cls.state != ca_connected)
    {
        Con_Printf("vr_hitmodel_check: no local game\n");
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    calls++;
    za::Vector<glm::vec3> drawn; // (a debug command's: made each call)
    for(int num = 1; num < qcvm->num_edicts && num < cl_max_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        const entity_t& e = cl_entities[num];
        Drawn d;
        if(!target(ent) || e.model != modelOf(ent) || !drawnOf(ent, d) || !modelcollide::drawnTriangles(e, num, drawn))
        {
            continue;
        }
        const auto* base = reinterpret_cast<const byte*>(d.hdr);
        const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + d.hdr->meshdesc);
        const auto* indexes = reinterpret_cast<const unsigned short*>(base + d.hdr->indexes);
        const Verts vs = vertsOf(d);
        Drawn raw; // without the lerp: its frame, at its origin
        checkNoLerp = true;
        drawnOf(ent, raw);
        checkNoLerp = false;
        const Verts rs = vertsOf(raw);
        for(za::SizeT i = 0; i < drawn.size() && static_cast<int>(i) < d.hdr->numindexes; i++)
        {
            const float distRaw = glm::length(rs.at(raw, mesh[indexes[i]].vertindex, 0.f) - drawn[i]);
            sumRaw += distRaw;
            worstRaw = za::max(worstRaw, static_cast<double>(distRaw));
            const float dist = glm::length(vs.at(d, mesh[indexes[i]].vertindex, 0.f) - drawn[i]);
            sum += dist;
            count++;
            if(dist > worst)
            {
                worst = dist;
                const glm::vec3 cl3{e.origin[0], e.origin[1], e.origin[2]};
                worstName = va("%s, frame %d, poses %d-%d at %.2f, origin %.1f %.1f %.1f, the hit test's %.1f %.1f %.1f, the client entity's %.1f %.1f %.1f, movetype %d, flags %d",
                    PR_GetString(ent->v.classname), static_cast<int>(ent->v.frame), d.pose1, d.pose2, d.blend, ent->v.origin[0],
                    ent->v.origin[1], ent->v.origin[2], d.origin.x, d.origin.y, d.origin.z, cl3.x, cl3.y, cl3.z,
                    static_cast<int>(ent->v.movetype), static_cast<int>(ent->v.flags));
                const Track& tr = tracks[static_cast<za::SizeT>(num)];
                worstName += va("; client poses %d-%d, lerp from %.3f (flags %d), cl.time %.3f; server time %.3f, the track's poses %d-%d from %.3f for %.3f, seen %.3f",
                    e.previouspose, e.currentpose, e.lerpstart, e.lerpflags, cl.time, qcvm->time, tr.prevPose, tr.pose, tr.poseStart,
                    tr.poseDur, tr.seen);
                worstName += va("; angles %.1f %.1f, the hit test's %.1f %.1f, the client's %.1f %.1f (current %.1f, previous %.1f, move lerp from %.3f); the client's origins %.1f %.1f -> %.1f %.1f; the track's %.1f %.1f -> %.1f %.1f from %.3f",
                    ent->v.angles[0], ent->v.angles[1], d.angles.x, d.angles.y, e.angles[0], e.angles[1], e.currentangles[1], e.previousangles[1],
                    e.movelerpstart, e.previousorigin[0], e.previousorigin[1], e.currentorigin[0], e.currentorigin[1], tr.prevOrigin.x,
                    tr.prevOrigin.y, tr.origin.x, tr.origin.y, tr.moveStart);
            }
        }
    }
    PR_PopQCVM(oldvm);
    if(Cmd_Argc() > 1 && !ZA_STRCMP(Cmd_Argv(1), "print"))
    {
        Con_Printf("vr_hitmodel_check: %lld calls, %lld vertices: the hit model %.2f units from the drawn one on average, "
                   "%.2f at worst (%s); without the lerp %.2f, %.2f at worst\n", calls, count,
            count ? sum / static_cast<double>(count) : 0., worst, worstName.cStr(), count ? sumRaw / static_cast<double>(count) : 0.,
            worstRaw);
    }
}

// vr_hitmodel_bench [rays]: for each monster in the map, rays from all round aimed at random points in its box: how
// many meet the box, how many of those the model (the empty part of the box), and each test's cost.
void bench_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_hitmodel_bench: no server\n");
        return;
    }
    const int rays = Cmd_Argc() > 1 ? za::max(1, Q_atoi(Cmd_Argv(1))) : 2000;
    const float tol = Cmd_Argc() > 2 ? Q_atof(Cmd_Argv(2)) : tolerance(Class::Guns);
    const bool newest = Cmd_Argc() > 3 && !ZA_STRCMP(Cmd_Argv(3), "newest"); // only the monster spawned last
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    za::FastNonCryptoRng rng{1234u}; // the same rays every run
    const auto uni = [&rng] { return rng.getF(0.f, 1.f); };
    const float saveDebug = vr_debug_hits.value;
    vr_debug_hits.value = 0.f;
    Con_Printf("vr_hitmodel_bench: %d rays a monster, tolerance %.1f\n", rays, tol);
    ankerl::unordered_dense::map<za::String, bool> seen;
    int only = -1;
    for(int num = 1; newest && num < qcvm->num_edicts; num++)
    {
        only = target(EDICT_NUM(num)) ? num : only;
    }
    for(int num = 1; num < qcvm->num_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        if(!target(ent) || (newest && num != only))
        {
            continue;
        }
        const za::String name = PR_GetString(ent->v.classname);
        if(seen[name + (static_cast<int>(ent->v.solid) == SOLID_NOT_BUT_TOUCHABLE ? " (corpse)" : "")])
        {
            continue;
        }
        seen[name + (static_cast<int>(ent->v.solid) == SOLID_NOT_BUT_TOUCHABLE ? " (corpse)" : "")] = true;
        const glm::vec3 lo = vec(ent->v.absmin) + glm::vec3{1.f}, hi = vec(ent->v.absmax) - glm::vec3{1.f};
        const glm::vec3 c = (lo + hi) * 0.5f;
        const float reach = glm::length(hi - lo) * 2.f + 32.f;
        int boxHits = 0, modelHits = 0;
        za::U64 hitHash = 1469598103934665603ull; // FNV-1a of every ray's model hit (where, which triangle): the same rays, the same hits
        double boxNs = 0., modelNs = 0.;
        const Stats before = stats;
        vec3_t zero{0.f, 0.f, 0.f};
        for(int r = 0; r < rays; r++)
        {
            const float z = uni() * 2.f - 1.f, phi = uni() * 6.2831853f, s = za::sqrt(za::max(0.f, 1.f - z * z));
            const glm::vec3 from = c + glm::vec3{s * za::cos(phi), s * za::sin(phi), z} * reach;
            const glm::vec3 aim = lo + (hi - lo) * glm::vec3{uni(), uni(), uni()};
            const glm::vec3 to = from + (aim - from) * 2.f;
            vec3_t a{from.x, from.y, from.z}, b{to.x, to.y, to.z};
            auto t0 = za::Clock::nowNanoseconds();
            const trace_t tr = SV_ClipMoveToEntity(ent, a, zero, zero, b);
            boxNs += static_cast<double>(za::Clock::nowNanoseconds() - t0);
            if(tr.fraction < 1.f || tr.startsolid)
            {
                boxHits++;
            }
            t0 = za::Clock::nowNanoseconds();
            Hit h;
            const bool hit = clip(ent, from, to, glm::vec3{0.f}, glm::vec3{0.f}, tol, Class::Guns, 1.f, h);
            modelNs += static_cast<double>(za::Clock::nowNanoseconds() - t0);
            modelHits += hit;
            const za::U32 words[]{hit, ZA_BIT_CAST(za::U32, h.t), static_cast<za::U32>(h.tri), ZA_BIT_CAST(za::U32, h.normal.x),
                ZA_BIT_CAST(za::U32, h.normal.y), ZA_BIT_CAST(za::U32, h.normal.z)};
            for(const za::U32 w : words)
            {
                hitHash = (hitHash ^ w) * 1099511628211ull;
            }
        }
        const long long narrow = stats.narrow - before.narrow;
        const aliashdr_t* hdr = nullptr;
        const Mesh* m = meshOf(static_cast<int>(ent->v.modelindex), modelOf(ent), hdr);
        // How far its model as drawn now reaches out of its box.
        float overhang = 0.f;
        Drawn d;
        if(drawnOf(ent, d))
        {
            const glm::vec3 blo = vec(ent->v.origin) + vec(ent->v.mins), bhi = vec(ent->v.origin) + vec(ent->v.maxs);
            for(const int p : {d.pose1, d.pose2})
            {
                const trivertx_t* v = posesOf(hdr) + static_cast<za::SizeT>(p) * m->numverts;
                for(int k = 0; k < m->numverts; k++)
                {
                    const glm::vec3 w = d.A * raw(v[k]) + d.b;
                    overhang = za::max(overhang, glm::length(w - glm::clamp(w, blo, bhi)));
                }
            }
        }
        Con_Printf("  %-22s %s: %4zu triangles, %3d poses (built in %.2f ms); box %d%%, model %d%% of the rays (%d%% of the box's); "
                   "box %.2f us, model %.2f us (%.1f nodes, %.1f triangles a test); the model reaches %.1f out of its box\n",
            name.cStr(), static_cast<int>(ent->v.solid) == SOLID_NOT_BUT_TOUCHABLE ? "corpse" : "alive", m->tris.size(), m->numposes, m->buildMs,
            boxHits * 100 / rays, modelHits * 100 / rays, boxHits ? modelHits * 100 / boxHits : 0, boxNs / 1000. / rays,
            modelNs / 1000. / rays, narrow ? static_cast<double>(stats.nodes - before.nodes) / narrow : 0.,
            narrow ? static_cast<double>(stats.tris - before.tris) / narrow : 0., overhang);
        Con_Printf("  hits %016llx %s\n", static_cast<unsigned long long>(hitHash), name.cStr());
    }
    vr_debug_hits.value = saveDebug;
    PR_PopQCVM(oldvm);
}

// vr_drawnlerp_watch <classname | 0 [label]>: a monster's drawn body, frame by frame (the test: vr_agarmor_test 21;
// ROUND26.md, "doglerp"). Each drawn frame, the renderer's lerp of the newest edict of that classname (R_AliasTransform:
// its poses, blend, origin, angles: VR_DrawnLerpSample) is laid on its model (relerp), against two others: the lerp the
// beasts' armour is placed from (clientLerp, read before the draw: they must be the same) and the hit model's own
// (drawnOf: what shots meet). Measured: how far the drawn vertices moved since the last frame (a frame drawn still while
// the hit model moved: a stall; a step over 3 times both its neighbours': a jump), the armour's lerp's distance from the
// drawn body (its offset: none) and the hit model's (its spread over the watch: the drift). `0` ends the watch and
// prints it ("drawnlerp: ..."); vr_drawnlerp_result is then 1 (smooth and in step), 0 (not) or -1 (too few frames).
namespace
{

struct LerpWatch
{
    int num{0};
    double lastTime{-1.0};
    za::Vector<glm::vec3> mesh, hit;                  // the last frame's vertices: drawn, the hit model's
    za::Vector<float> meshSteps, hitSteps, offsets; // per frame: mean vertex step (drawn, hit model), mean drawn-hit gap
    float armour{0.f};                              // the armour's lerp's largest mean distance from the drawn body
    // The armour's vertices (drawnVertexPoints with the client's lerp: what its pieces are placed from) against the shape
    // the shader drew (animsmooth::capturedPoints: the draw's own inputs replayed), each frame (ROUND26 "armourrebuild")
    za::Vector<int> all;                  // 0 .. numverts - 1
    za::Vector<glm::vec3> armourPts, shader; // the last frame's armour points (world); scratch
    glm::mat3 drawnA{1.f};                // the last frame's drawn place (model bytes to world)
    glm::vec3 drawnB{0.f};
    za::Vector<float> shapeMeans;         // per frame: the armour's mean distance from the drawn shape
    float shapeMost{0.f};                 // ... the largest one vertex's
    // The hit model's vertices (what shots meet: the server's drawn shape, vr_hitmodel_drawn) against the same (ROUND26
    // "drawnhits"): its lerp is the server's, so a fraction of a frame's motion stays in it (the clocks)
    za::Vector<glm::vec3> hitPts;
    za::Vector<float> hitMeans;
    float hitMost{0.f};
};
LerpWatch lerpWatch;

constexpr float lerpJumpRatio = 3.f;    // a step over this times both its neighbours' (and lerpJumpSlack more): a jump
constexpr float lerpJumpSlack = 0.25f;  // units
constexpr float lerpStallHit = 0.2f;    // units: the hit model moved this much in the frame ...
constexpr float lerpStallMesh = 0.02f;  // ... and the drawn body under this: a stall
constexpr float lerpArmourMost = 0.05f; // units: the armour's lerp off the drawn body
constexpr float lerpShapeMost = 0.05f;  // units: any armour vertex off the shape the shader drew
constexpr float lerpDriftMost = 2.5f;   // units: the drawn-hit gap's spread over the watch (the client's clock is a
                                        // fraction of a frame behind the server's: a lerp begun out of turn differs)
constexpr int lerpLeastFrames = 8;

void drawnLerpWatch_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_drawnlerp_watch <classname> | 0 [label]\n");
        return;
    }
    LerpWatch& w = lerpWatch;
    if(ZA_STRCMP(Cmd_Argv(1), "0") != 0)
    {
        w = LerpWatch{};
        if(!sv.active)
        {
            return;
        }
        const progs::ServerVm vm;
        for(int num = qcvm->num_edicts - 1; num > 0 && w.num == 0; num--)
        {
            edict_t* ent = EDICT_NUM(num);
            if(!ent->free && !ZA_STRCMP(PR_GetString(ent->v.classname), Cmd_Argv(1)))
            {
                w.num = num;
            }
        }
        animsmooth::captureDraws(w.num > 0 && w.num < cl_max_edicts ? &cl_entities[w.num] : nullptr);
        Cvar_Set("vr_drawnlerp_result", "-1");
        return;
    }
    animsmooth::captureDraws(nullptr);
    const char* label = Cmd_Argc() > 2 ? Cmd_Argv(2) : "watch";
    const za::SizeT n = w.meshSteps.size();
    int stalls = 0, jumps = 0;
    float most = 0.f, lo = 1e30f, hi = -1e30f;
    for(za::SizeT i = 0; i < n; i++)
    {
        const float s = w.meshSteps[i];
        stalls += w.hitSteps[i] > lerpStallHit && s < lerpStallMesh ? 1 : 0;
        const float before = i > 0 ? w.meshSteps[i - 1] : s;
        const float after = i + 1 < n ? w.meshSteps[i + 1] : s;
        jumps += i > 0 && i + 1 < n && s > lerpJumpRatio * za::max(before, after) + lerpJumpSlack ? 1 : 0;
        most = za::max(most, s);
        lo = za::min(lo, w.offsets[i]);
        hi = za::max(hi, w.offsets[i]);
    }
    za::Vector<float> sorted = w.meshSteps;
    float median = 0.f;
    if(n > 0)
    {
        za::nthElement(sorted.begin(), sorted.begin() + n / 2, sorted.end());
        median = sorted[n / 2];
    }
    const float drift = n > 0 ? hi - lo : 0.f;
    // The armour against the shape drawn: its mean distance a frame (lowest, mean, highest: the spread is how much the
    // pieces move on the drawn body) and one vertex's most
    float shapeLo = 0.f, shapeHi = 0.f, shapeMean = 0.f;
    const za::SizeT sn = w.shapeMeans.size();
    for(za::SizeT i = 0; i < sn; i++)
    {
        const float s = w.shapeMeans[i];
        shapeLo = i == 0 ? s : za::min(shapeLo, s);
        shapeHi = i == 0 ? s : za::max(shapeHi, s);
        shapeMean += s / static_cast<float>(sn);
    }
    const bool shapeOk = sn > 0 && w.shapeMost < lerpShapeMost;
    const int result = static_cast<int>(n) < lerpLeastFrames ? -1
                       : (stalls == 0 && jumps == 0 && w.armour < lerpArmourMost && drift < lerpDriftMost && shapeOk) ? 1 : 0;
    Con_Printf("drawnlerp: %s: %d frames, drawn step median %.2f, largest %.2f (%.1fx); stalls %d, jumps %d; armour off the "
               "body %.3f; drawn to hit model %.2f .. %.2f units (drift %.2f): %s\n", label, static_cast<int>(n),
        static_cast<double>(median), static_cast<double>(most), static_cast<double>(median > 1e-4f ? most / median : 0.f), stalls,
        jumps, static_cast<double>(w.armour), static_cast<double>(n ? lo : 0.f), static_cast<double>(n ? hi : 0.f),
        static_cast<double>(drift), result == 1 ? "smooth" : result == 0 ? "NOT SMOOTH" : "too few frames");
    Con_Printf("drawnlerp shape: %s: %d frames, the armour off the shape drawn: mean a frame %.3f .. %.3f (mean %.3f, spread "
               "%.3f), one vertex's most %.3f units (vr_armour_on_drawn %g): %s\n", label, static_cast<int>(sn),
        static_cast<double>(shapeLo), static_cast<double>(shapeHi), static_cast<double>(shapeMean),
        static_cast<double>(shapeHi - shapeLo), static_cast<double>(w.shapeMost), static_cast<double>(vr_armour_on_drawn.value),
        shapeOk ? "on the body" : sn ? "OFF THE BODY" : "NO DRAWS CAPTURED");
    // What shots meet against the shape drawn (informational: the server's lerp, a fraction of a frame from the client's)
    float hitLo = 0.f, hitHi = 0.f, hitMean = 0.f;
    const za::SizeT hn = w.hitMeans.size();
    for(za::SizeT i = 0; i < hn; i++)
    {
        const float s = w.hitMeans[i];
        hitLo = i == 0 ? s : za::min(hitLo, s);
        hitHi = i == 0 ? s : za::max(hitHi, s);
        hitMean += s / static_cast<float>(hn);
    }
    Con_Printf("drawnlerp hits: %s: %d frames, the hit model off the shape drawn: mean a frame %.3f .. %.3f (mean %.3f), one "
               "vertex's most %.3f units (vr_hitmodel_drawn %g)\n", label, static_cast<int>(hn), static_cast<double>(hitLo),
        static_cast<double>(hitHi), static_cast<double>(hitMean), static_cast<double>(w.hitMost),
        static_cast<double>(vr_hitmodel_drawn.value));
    Cvar_Set("vr_drawnlerp_result", va("%d", result));
    w = LerpWatch{};
}

} // namespace

void registerDiagCommands()
{
    Cmd_AddCommand("vr_drawnlerp_watch", drawnLerpWatch_f);
    Cmd_AddCommand("vr_hitmodel_bench", bench_f);
    Cmd_AddCommand("vr_hitmodel_stats", stats_f);
    Cmd_AddCommand("vr_hitmodel_check", check_f);
    Cmd_AddCommand("vr_hitzones_check", zonesCheck_f);
}

} // namespace qvr::hitmodel

// R_AliasTransform: entity `e` drawn with this lerp (vr_drawnlerp_watch's frame: once a client time, the first draw).
extern "C" void VR_DrawnLerpSample(const entity_t* e, int pose1, int pose2, float blend, const float* origin, const float* angles)
{
    using namespace qvr::hitmodel;
    using namespace qvr::hitmodel::detail;
    LerpWatch& w = lerpWatch;
    if(w.num <= 0 || w.num >= cl_max_edicts || e != &cl_entities[w.num] || cl.time == w.lastTime || !sv.active)
    {
        return;
    }
    const qvr::progs::ServerVm serverVm;
    edict_t* ent = w.num < qcvm->num_edicts ? EDICT_NUM(w.num) : nullptr;
    Drawn d;
    if(!ent || ent->free || !drawnOf(ent, d) || (d.skinned && !d.shaped))
    {
        return; // (a ragdoll: not the frames' lerp)
    }
    // (the hit model's lerp of the bytes, for the drift: the client's lerp against the server's, the shape apart)
    const Drawn shaped = d;
    if(d.shaped)
    {
        d.skinned = d.skinnedN = nullptr;
        d.shaped = false;
        d.build = 0u;
    }
    // The drawn lerp, and the armour's (read again as the armour read it before the draw), laid on the model.
    DrawnLerp drawn;
    drawn.pose1 = pose1;
    drawn.pose2 = pose2;
    drawn.blend = blend;
    drawn.origin = glm::vec3{origin[0], origin[1], origin[2]};
    drawn.angles = glm::vec3{angles[0], angles[1], angles[2]};
    DrawnLerp armour;
    Drawn m = d, a = d;
    if(!relerp(m, drawn) || !clientLerp(const_cast<entity_t*>(e), armour) || !relerp(a, armour))
    {
        return;
    }
    const Verts vm = vertsOf(m), vh = vertsOf(d), va = vertsOf(a);
    const int nv = d.mesh->numverts;
    const bool steps = w.lastTime >= 0.0 && static_cast<int>(w.mesh.size()) == nv && cl.time - w.lastTime < 0.1;
    // The armour against the shape the shader drew in the last frame (its draw comes after this sample: compared a frame
    // late, both of that frame), then this frame's armour points (what its pieces are placed from)
    if(static_cast<int>(w.all.size()) != nv)
    {
        w.all.resize(static_cast<za::SizeT>(nv));
        for(int i = 0; i < nv; i++)
        {
            w.all[static_cast<za::SizeT>(i)] = i;
        }
    }
    w.shader.resize(static_cast<za::SizeT>(nv));
    if(static_cast<int>(w.armourPts.size()) == nv && w.lastTime >= 0.0 &&
        qvr::animsmooth::capturedPoints(w.lastTime, w.all.data(), nv, w.shader.data()))
    {
        double sum = 0.;
        for(za::SizeT i = 0; i < static_cast<za::SizeT>(nv); i++)
        {
            const float off = glm::length(w.drawnA * w.shader[i] + w.drawnB - w.armourPts[i]);
            sum += off;
            w.shapeMost = za::max(w.shapeMost, off);
        }
        w.shapeMeans.pushBack(static_cast<float>(sum / nv));
        if(static_cast<int>(w.hitPts.size()) == nv)
        {
            double hsum = 0.;
            for(za::SizeT i = 0; i < static_cast<za::SizeT>(nv); i++)
            {
                const float off = glm::length(w.drawnA * w.shader[i] + w.drawnB - w.hitPts[i]);
                hsum += off;
                w.hitMost = za::max(w.hitMost, off);
            }
            w.hitMeans.pushBack(static_cast<float>(hsum / nv));
        }
    }
    {
        const Verts vs = vertsOf(shaped);
        w.hitPts.resize(static_cast<za::SizeT>(nv));
        for(int i = 0; i < nv; i++)
        {
            w.hitPts[static_cast<za::SizeT>(i)] = vs.at(shaped, i, 0.f);
        }
    }
    w.armourPts.resize(static_cast<za::SizeT>(nv));
    if(!drawnVertexPoints(ent, armour, w.all.data(), nv, w.armourPts.data()))
    {
        w.armourPts.clear();
    }
    w.drawnA = m.A;
    w.drawnB = m.b;
    w.mesh.resize(static_cast<za::SizeT>(nv));
    w.hit.resize(static_cast<za::SizeT>(nv));
    double meshStep = 0., hitStep = 0., gap = 0., armourGap = 0.;
    for(int i = 0; i < nv; i++)
    {
        const glm::vec3 pm = vm.at(m, i, 0.f), ph = vh.at(d, i, 0.f);
        armourGap += glm::length(va.at(a, i, 0.f) - pm);
        glm::vec3& lm = w.mesh[static_cast<za::SizeT>(i)];
        glm::vec3& lh = w.hit[static_cast<za::SizeT>(i)];
        meshStep += glm::length(pm - lm);
        hitStep += glm::length(ph - lh);
        gap += glm::length(pm - ph);
        lm = pm;
        lh = ph;
    }
    if(steps)
    {
        w.meshSteps.pushBack(static_cast<float>(meshStep / nv));
        w.hitSteps.pushBack(static_cast<float>(hitStep / nv));
        w.offsets.pushBack(static_cast<float>(gap / nv));
        w.armour = za::max(w.armour, static_cast<float>(armourGap / nv));
        if(qvr::vr_drawnlerp_log.value != 0.f)
        {
            Con_Printf("drawnlerp frame: cl %.4f sv %.4f poses %d-%d %.3f, hit %d-%d %.3f; origin %.2f %.2f %.2f, hit %.2f %.2f %.2f; "
                       "step %.2f, hit %.2f, gap %.2f\n", cl.time, qcvm->time, pose1, pose2, static_cast<double>(blend), d.pose1,
                d.pose2, static_cast<double>(d.blend), static_cast<double>(origin[0]), static_cast<double>(origin[1]),
                static_cast<double>(origin[2]), static_cast<double>(d.origin.x), static_cast<double>(d.origin.y),
                static_cast<double>(d.origin.z), meshStep / nv, hitStep / nv, gap / nv);
        }
    }
    w.lastTime = cl.time;
}
