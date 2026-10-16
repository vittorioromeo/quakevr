// vr_ragdoll_diag.cpp -- vr_drawn_motion_test (the drawn motion of ragdoll parts, props and hands: its recorder and report) and
// vr_ragdoll_info (a rig). Split from vr_ragdoll.cpp (a pure move); registered in vr_box3d_diag.cpp; what they share is in
// vr_ragdoll_internal.hpp.

#include "vr_modelmetadata.hpp"
#include "vr_ragdoll_internal.hpp"
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
using namespace qvr::ragdoll;
using namespace qvr::ragdoll::detail;

namespace
{

// vr_drawn_motion_test: what it follows and for how long (the client's frames), and the places drawn each frame.
struct MotionTest
{
    int left{0}; // frames still to record
    int rag{0};  // the ragdoll's entity (0 none)
    int prop{0}; // the prop's (0 none)
    int frames{0};
};
MotionTest motionTest;

struct MotionPoints
{
    za::Vector<glm::vec3> rag;  // a frame's maxBones (the parts')
    za::Vector<glm::vec3> prop; // a frame's one
    za::Vector<glm::vec3> hand; // a frame's two (the off hand, the main)
    auto members() { return mem::list(rag, prop, hand); }
};
mem::Cache<MotionPoints> motionPoints{"drawn motion test", mem::MapChange};

// How evenly a point moved over the frames (`stride` apart in `pts`): its mean step a frame (units), the mean change of
// the step's length from frame to frame and of the step itself (the motion's jerk), both over the mean step, and the
// frames it (nearly) stood still in. Smooth motion: both near 0; drawn at the server's 72 Hz in 90 Hz frames: every
// fifth frame still (uneven about 0.4).
struct Evenness
{
    float step{0.f}, uneven{0.f}, jerk{0.f};
    int stalls{0};
};

[[nodiscard]] Evenness evenness(const glm::vec3* pts, int frames, int stride)
{
    Evenness e;
    if(frames < 3)
    {
        return e;
    }
    for(int i = 1; i < frames; i++)
    {
        e.step += glm::distance(pts[i * stride], pts[(i - 1) * stride]);
    }
    e.step /= static_cast<float>(frames - 1);
    if(e.step < 0.01f)
    {
        return e;
    }
    for(int i = 2; i < frames; i++)
    {
        const glm::vec3 d1 = pts[i * stride] - pts[(i - 1) * stride];
        const glm::vec3 d0 = pts[(i - 1) * stride] - pts[(i - 2) * stride];
        e.uneven += za::abs(glm::length(d1) - glm::length(d0));
        e.jerk += glm::length(d1 - d0);
    }
    for(int i = 1; i < frames; i++)
    {
        e.stalls += glm::distance(pts[i * stride], pts[(i - 1) * stride]) < 0.25f * e.step;
    }
    e.uneven /= e.step * static_cast<float>(frames - 2);
    e.jerk /= e.step * static_cast<float>(frames - 2);
    return e;
}

void printEvenness(const char* what, const Evenness& e)
{
    if(e.step < 0.01f)
    {
        Con_Printf("  %s: still\n", what);
        return;
    }
    Con_Printf("  %s: %.2f units a frame, uneven %.3f, jerk %.3f, %d stalls\n", what, e.step, e.uneven, e.jerk, e.stalls);
}

void reportMotion()
{
    const int frames = motionTest.frames;
    Con_Printf("vr_drawn_motion_test: %d frames of %.1f ms (the server's: %.1f ms), vr_ragdoll_smooth %g, vr_ragdoll_held_local %g\n",
        frames, 1000.0 * (cl.time - cl.oldtime), 1000.0 * (cl.mtime[0] - cl.mtime[1]), vr_ragdoll_smooth.value,
        vr_ragdoll_held_local.value);
    if(motionTest.rag > 0)
    {
        char what[64];
        const int held = motionTest.rag < static_cast<int>(draw.byNum.size()) ? draw.byNum[static_cast<za::SizeT>(motionTest.rag)].heldBy : 0;
        q_snprintf(what, sizeof(what), "ragdoll %d (held by hands %d) pelvis", motionTest.rag, held);
        printEvenness(what, evenness(motionPoints.rag.data(), frames, maxBones));
        // Its parts: the mean and the most uneven moving one.
        float uneven = 0.f, jerk = 0.f, worst = 0.f;
        int moving = 0, worstPart = -1;
        for(int b = 0; b < maxBones; b++)
        {
            const Evenness e = evenness(motionPoints.rag.data() + b, frames, maxBones);
            if(e.step < 0.01f)
            {
                continue;
            }
            uneven += e.uneven;
            jerk += e.jerk;
            moving++;
            if(e.uneven > worst)
            {
                worst = e.uneven;
                worstPart = b;
            }
        }
        if(moving)
        {
            Con_Printf("  ragdoll %d parts: %d moving, uneven %.3f, jerk %.3f (means), the most uneven %.3f (part %d)\n",
                motionTest.rag, moving, uneven / static_cast<float>(moving), jerk / static_cast<float>(moving), worst, worstPart);
        }
    }
    if(motionTest.prop > 0)
    {
        char what[64];
        q_snprintf(what, sizeof(what), "prop %d", motionTest.prop);
        printEvenness(what, evenness(motionPoints.prop.data(), frames, 1));
    }
    printEvenness("off hand", evenness(motionPoints.hand.data(), frames, 2));
    printEvenness("main hand", evenness(motionPoints.hand.data() + 1, frames, 2));
}

} // namespace

namespace qvr::ragdoll
{

namespace detail
{

// vr_drawn_motion_test's frame: the places drawn (after the relink and the held objects in the hands).
void recordMotion()
{
    if(motionTest.left <= 0)
    {
        return;
    }
    const int rag = motionTest.rag;
    const bool ragShown = rag > 0 && rag < static_cast<int>(draw.byNum.size()) && draw.byNum[static_cast<za::SizeT>(rag)].rig;
    if(ragShown)
    {
        drawnPose(rag);
    }
    const Published* p = ragShown ? &draw.byNum[static_cast<za::SizeT>(rag)] : nullptr;
    for(int b = 0; b < maxBones; b++)
    {
        motionPoints.rag.pushBack(p && b < p->bodies ? p->drawPos[static_cast<za::SizeT>(b)] : glm::vec3{0.f});
    }
    const int prop = motionTest.prop;
    const bool propShown = prop > 0 && prop < cl.num_entities;
    motionPoints.prop.pushBack(propShown ? glm::vec3{cl_entities[prop].origin[0], cl_entities[prop].origin[1], cl_entities[prop].origin[2]}
                                         : glm::vec3{0.f});
    const hands::State& hs = hands::current();
    motionPoints.hand.pushBack(hs.pos[0]);
    motionPoints.hand.pushBack(hs.pos[1]);
    if(p && vr_debug_ragdoll.value >= 2.f)
    {
        // Each frame: where it is drawn between its steps, the steps' times, the lead the hands gave it, its pelvis.
        Con_Printf("  frame %d: cl.time %.4f, steps %.4f %.4f, blend %.2f, lead %.2f, pelvis %.2f %.2f %.2f\n", motionTest.frames,
            cl.time, p->prevTime, p->time, p->drawBlend, glm::length(p->drawLead), p->drawPos[0].x, p->drawPos[0].y, p->drawPos[0].z);
    }
    motionTest.frames++;
    if(--motionTest.left == 0)
    {
        reportMotion();
    }
}

} // namespace detail

void motionTest_f()
{
    if(Cmd_Argc() < 2 || !sv.active)
    {
        Con_Printf("usage: vr_drawn_motion_test <frames> [<entity> | nearest | held] (a listen server)\n");
        return;
    }
    motionTest = MotionTest{};
    motionPoints.rag.clear();
    motionPoints.prop.clear();
    motionPoints.hand.clear();
    // The ragdoll nearest you.
    const entity_t& me = cl_entities[cl.viewentity];
    const glm::vec3 from{me.origin[0], me.origin[1], me.origin[2]};
    float best = 1e30f;
    for(int num = 1; num < static_cast<int>(draw.byNum.size()); num++)
    {
        const Published& p = draw.byNum[static_cast<za::SizeT>(num)];
        if(p.rig && glm::distance(p.pos[0], from) < best)
        {
            best = glm::distance(p.pos[0], from);
            motionTest.rag = num;
        }
    }
    if(Cmd_Argc() > 2)
    {
        if(!q_strcasecmp(Cmd_Argv(2), "nearest"))
        {
            motionTest.prop = box3d::nearestProp();
        }
        else if(!q_strcasecmp(Cmd_Argv(2), "held"))
        {
            const int main = cl.stats[protocol::STAT_QVR_CARRYMAIN];
            motionTest.prop = main ? main : cl.stats[protocol::STAT_QVR_CARRYOFF];
        }
        else
        {
            motionTest.prop = Q_atoi(Cmd_Argv(2));
        }
    }
    motionTest.left = za::clamp(Q_atoi(Cmd_Argv(1)), 3, 2000);
    Con_Printf("vr_drawn_motion_test: ragdoll %d, prop %d, %d frames\n", motionTest.rag, motionTest.prop, motionTest.left);
}

void info_f()
{
    qmodel_t* model = Mod_ForName("progs/soldier.mdl", false);
    if(Cmd_Argc() > 1)
    {
        model = Mod_ForName(Cmd_Argv(1), false);
    }
    if(!model)
    {
        Con_Printf("vr_ragdoll_info: no such model\n");
        return;
    }
    const Rig* rig = rigFor(model);
    if(!rig)
    {
        Con_Printf("vr_ragdoll_info: %s has no ragdoll (vr_ragdoll 1: the grunt's)\n", model->name);
        return;
    }
    Con_Printf("vr_ragdoll_info: %s: %d bones, %d vertices, %d poses; motion clusters %.2f units rms, bones %.2f; derived in %.1f ms\n",
        model->name, rig->numBones, rig->numVerts, rig->numPoses, rig->clusterRms, rig->boneRms, rig->deriveMs);
    for(int b = 0; b < rig->numBones; b++)
    {
        const Bone& bone = rig->bones[b];
        int n = 0;
        for(const uint8_t vb : rig->vertBone)
        {
            n += vb == b ? 1 : 0;
        }
        static constexpr const char* kinds[] = {"root", "ball", "hinge", "loose"};
        Con_Printf("  %2d %-11s parent %2d %-5s %3d verts (%2d places) pivot %5.1f %5.1f %5.1f", b, bone.name, bone.parent,
            kinds[static_cast<int>(bone.joint)], n, static_cast<int>(bone.points.size()), bone.pivot.x, bone.pivot.y, bone.pivot.z);
        int hidden = 0, first = -1;
        for(int pose = 0; pose < rig->numPoses; pose++)
        {
            if(collapsed(*rig, pose, b))
            {
                hidden++;
                first = first < 0 ? pose : first;
            }
        }
        if(hidden > 0)
        {
            Con_Printf(" hidden in %d poses (from %d)", hidden, first);
        }
        if(bone.joint == Joint::Ball)
        {
            Con_Printf(" cone %.0f twist %.0f", bone.cone / deg, bone.twist / deg);
        }
        else if(bone.joint == Joint::Hinge)
        {
            Con_Printf(" axis %.2f %.2f %.2f range %.0f..%.0f", bone.hinge.x, bone.hinge.y, bone.hinge.z, bone.lower / deg, bone.upper / deg);
        }
        Con_Printf("\n");
    }
}

} // namespace qvr::ragdoll
