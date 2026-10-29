// vr_hitmodel_diag.cpp -- vr_hitmodel_stats, vr_hitmodel_check, vr_hitmodel_bench and vr_hitzones_check: the hit models' reports and tests.
// Split from vr_hitmodel.cpp (a pure move); what they share is in vr_hitmodel_internal.hpp.

#include "vr_geom.hpp"
#include "vr_hitmodel_internal.hpp"
#include "vr_ad.hpp" // AD support: small pellets retain precise model/corpse impacts.
#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_cvars.hpp"
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
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Base/BitCast.hpp"
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
    Con_Printf("precise hit detection: %s; tolerances: guns %.1f, grapple %.1f, melee %.1f, thrown %.1f\n",
        enabled() ? "on" : "off", tolerance(Class::Guns), tolerance(Class::Grapple), tolerance(Class::Melee), tolerance(Class::Thrown));
    Con_Printf("  %lld broad tests, %lld model tests: %lld hits (%lld started inside), %lld through the box\n", stats.clips,
        stats.narrow, stats.hits, stats.insides, stats.throughs);
    if(stats.narrow)
    {
        Con_Printf("  a model test: %.2f us, %.1f nodes, %.1f triangles\n", stats.ns / 1000. / static_cast<double>(stats.narrow),
            static_cast<double>(stats.nodes) / static_cast<double>(stats.narrow), static_cast<double>(stats.tris) / static_cast<double>(stats.narrow));
    }
    Con_Printf("  %lld hierarchies made at map loads, %.1f ms in all\n", stats.builds, stats.buildMs);
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
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "reset"))
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
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "reset"))
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
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "print"))
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
    const bool newest = Cmd_Argc() > 3 && !strcmp(Cmd_Argv(3), "newest"); // only the monster spawned last
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    const bool wasOn = enabled();
    const za::String preciseWas{vr_hit_precise.string};
    if(!wasOn)
    {
        Cvar_SetQuick(&vr_hit_precise, "1");
    }
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
    if(!wasOn)
    {
        Cvar_SetQuick(&vr_hit_precise, preciseWas.cStr());
    }
    PR_PopQCVM(oldvm);
}

void registerDiagCommands()
{
    Cmd_AddCommand("vr_hitmodel_bench", bench_f);
    Cmd_AddCommand("vr_hitmodel_stats", stats_f);
    Cmd_AddCommand("vr_hitmodel_check", check_f);
    Cmd_AddCommand("vr_hitzones_check", zonesCheck_f);
}

} // namespace qvr::hitmodel
