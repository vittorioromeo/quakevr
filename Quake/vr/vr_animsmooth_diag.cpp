// vr_animsmooth_diag.cpp -- vr_anim_smooth_pin, _info, _check and _trace: the smoother animations' tests and reports.
// Split from vr_animsmooth.cpp (a pure move); what they share is in vr_animsmooth_internal.hpp.

#include "vr_animsmooth_internal.hpp"
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

using namespace qvr;
using namespace qvr::animsmooth::detail;

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

// vr_anim_smooth_trace <frames> [<entity> | <model> [<jolt>]]
void trace_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_anim_smooth_trace <frames> [<entity> | <model>]: the nearest monster's (or <entity>'s, or the nearest "
                   "whose model's name has <model>) drawn animation over the next <frames> frames: how much its vertices' "
                   "speed changes frame to frame (r_lerpmodels 1 against 3; vr_fixed_frames 1); <jolt> (after <model>): "
                   "each frame one vertex's change passes it, printed with its spline's keys and the two frames' before\n");
        return;
    }
    trace = Trace{};
    trace.ent = pickEntity(Cmd_Argc() >= 3 ? Cmd_Argv(2) : "");
    trace.framesLeft = za::max(atoi(Cmd_Argv(1)), 3);
    trace.joltLog = Cmd_Argc() >= 4 ? static_cast<float>(atof(Cmd_Argv(3))) : 0.f;
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
    Con_Printf("animsmooth: %d splines drawn since the last info, %d with limbs swung: %d with the lerp before, %d with a "
               "frame expected after\n", stats.splines, stats.swung, stats.history, stats.ahead);
    Con_Printf("animsmooth: %d rebuilt (vr_anim_smooth_rebuild %g); %d instances drawn in %.3f ms (%.2f us each)\n",
        stats.rebuilt, vr_anim_smooth_rebuild.value, stats.calls, stats.seconds * 1000.0,
        stats.calls ? stats.seconds * 1e6 / stats.calls : 0.0);
    Con_Printf("animsmooth: pinned entity %d, traced entity %d (-1: none)\n", pin.ent, trace.ent);
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
            seen += m.seen[static_cast<za::SizeT>(f)] >= 0 && m.seen[static_cast<za::SizeT>(f)] != m.next[static_cast<za::SizeT>(f)] ? 1 : 0;
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
            m.swungVerts, m.fadedVerts, static_cast<int>(m.rest.size() / restStride));
        Con_Printf("animsmooth:   rebuild (error %.2f): %d vertices fully, %d partly; fit rms median %.3f, p90 %.3f, most %.3f "
                   "units: %s\n", m.rebuildError, m.rebuiltVerts, m.rebuildFaded, m.rebuildMedian, m.rebuildP90, m.rebuildMost,
            m.rebuildOk ? "rebuilt" : "too few: drawn as before");
        for(int b = 0; b < m.rig->numBones; b++)
        {
            int unfit = 0;
            for(const uint32_t u : m.unfit)
            {
                unfit += static_cast<int>((u >> b) & 1u);
            }
            Con_Printf("animsmooth:   bone %2d %-10s unfit in %d of %d poses", b, m.rig->bones[b].name, unfit, m.numPoses);
            for(int p = 0; Cmd_Argc() >= 3 && p < m.numPoses; p++) // (and which, with the frames)
            {
                if((m.unfit[static_cast<za::SizeT>(p)] >> b) & 1u)
                {
                    Con_Printf(" %d", p);
                }
            }
            Con_Printf("\n");
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
                        const glm::vec4 r = m.rest[k * restStride];
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

void registerDiagCommands()
{
    Cmd_AddCommand("vr_anim_smooth_pin", pin_f);
    Cmd_AddCommand("vr_anim_smooth_info", info_f);
    Cmd_AddCommand("vr_anim_smooth_check", check_f);
    Cmd_AddCommand("vr_anim_smooth_trace", trace_f);
}

} // namespace qvr::animsmooth
