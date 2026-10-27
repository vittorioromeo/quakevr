// vr_rig2_game.cpp -- the game on the authored rig (vr_rig2 1; vr_rig2.hpp, "Stage 2"; ROUND22.md, "Grunt rig, stage 2").
//
// With vr_rig2 1 (read when a map is spawned: latch) the grunt's ragdoll rig is the authored one (vr_ragdoll.cpp
// fromAuthored: its bones, joints and limits, its blended weights, its frames' fits), so everything made from his rig
// (ragdolls, knockdowns, hit reactions, an armed one's arm and leg solves, limb cuts, wounds) is on the authored bones,
// and a live one is drawn here from the same bones with the blended skin ("<model>#rig2", rig2::drawLive):
// - live: each bone's local motion between the frame lerp's two poses (their fits; a bone's turn about its joint, not a
//   straight line between its places), its hit reaction's kick (hitreact::boneKicks, per authored bone), a hidden bone
//   collapsed (his dropped gun);
// - armed (hitreact.cpp relinkSkinned, liveArmed): the bones as that solve has them (the frame, legs, upper body, turns,
//   kicks), the arms from agarms' solve laid on the authored arm bones within their limits (armsOnRig: the clavicle aimed
//   at its shoulder, the upper arm at its elbow with its twist the solve's, the forearm's hinge at its wrist, the hand to
//   its grip frame's turn), its weapons carried with the hands as drawn; a take's legs (aglegs) on the leg bones
//   (legsOnRig).

#include "vr_rig2.hpp"
#include "vr_agarms.hpp"
#include "vr_agbody.hpp"
#include "vr_aglegs.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hitreact.hpp"
#include "vr_mem.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rigpose.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string.h>

namespace qvr::rig2
{

namespace
{

constexpr float pi = 3.14159265358979f;

[[nodiscard]] rigpose::Rigid compose(const rigpose::Rigid& a, const rigpose::Rigid& b) // a after b
{
    return rigpose::Rigid{glm::normalize(a.rot * b.rot), a.rot * b.pos + a.pos};
}

[[nodiscard]] rigpose::Rigid inverse(const rigpose::Rigid& a)
{
    const glm::quat r = glm::inverse(a.rot);
    return rigpose::Rigid{r, -(r * a.pos)};
}

[[nodiscard]] glm::vec3 unit(const glm::vec3& v, const glm::vec3& fallback)
{
    const float l = glm::length(v);
    return l > 1e-6f ? v / l : fallback;
}

// The shortest turn taking direction a to b.
[[nodiscard]] glm::quat between(const glm::vec3& a, const glm::vec3& b)
{
    const glm::vec3 u = unit(a, glm::vec3{0.f, 0.f, 1.f}), v = unit(b, glm::vec3{0.f, 0.f, 1.f});
    const float c = glm::dot(u, v);
    if(c < -0.9999f)
    {
        glm::vec3 axis = glm::cross(u, glm::vec3{1.f, 0.f, 0.f});
        axis = glm::length(axis) < 1e-3f ? glm::cross(u, glm::vec3{0.f, 1.f, 0.f}) : axis;
        return glm::angleAxis(pi, glm::normalize(axis));
    }
    const glm::vec3 axis = glm::cross(u, v);
    return glm::normalize(glm::quat{1.f + c, axis.x, axis.y, axis.z});
}

// The turn taking a1 to b1 exactly and a2 (across a1) as near b2 as it can.
[[nodiscard]] glm::quat align(const glm::vec3& a1, const glm::vec3& a2, const glm::vec3& b1, const glm::vec3& b2)
{
    const glm::vec3 x0 = unit(a1, glm::vec3{1.f, 0.f, 0.f}), x1 = unit(b1, glm::vec3{1.f, 0.f, 0.f});
    const glm::vec3 y0 = unit(a2 - x0 * glm::dot(a2, x0), glm::vec3{0.f}), y1 = unit(b2 - x1 * glm::dot(b2, x1), glm::vec3{0.f});
    if(glm::length(y0) < 0.5f || glm::length(y1) < 0.5f)
    {
        return between(a1, b1);
    }
    const glm::mat3 m0{x0, y0, glm::cross(x0, y0)}, m1{x1, y1, glm::cross(x1, y1)};
    return glm::normalize(glm::quat_cast(m1 * glm::transpose(m0)));
}

// Bone b's motion from its parent's and its local turn about its pivot.
[[nodiscard]] rigpose::Rigid child(const Bone& bone, const rigpose::Rigid& Fp, const glm::quat& local)
{
    return compose(Fp, rigpose::Rigid{local, bone.pivot - local * bone.pivot});
}

// Bone b turned in the world to `want` (rest -> now) on its parent's `Fp`, as near as its limits let it when `clamp`
// (else as wanted); how far past its limits that is (degrees) added to `held`.
[[nodiscard]] rigpose::Rigid limited(const Rig& a, int b, const rigpose::Rigid& Fp, const glm::quat& want, float* held,
    bool clamp = true)
{
    const glm::quat local = glm::normalize(glm::inverse(Fp.rot) * want);
    const glm::vec3 angles = anglesOf(a, b, local);
    const glm::vec3 kept = clampAngles(a, b, angles);
    if(held)
    {
        *held += glm::length(kept - angles);
    }
    return child(a.bones[b], Fp, clamp ? localOf(a, b, kept) : local);
}

[[nodiscard]] float turnDeg(const glm::quat& a, const glm::quat& b)
{
    return 2.f * za::acos(za::clamp(za::abs(glm::dot(glm::normalize(a), glm::normalize(b))), 0.f, 1.f)) * (180.f / pi);
}

// The arms as laid on the authored rig (vr_rig2_arms: the worst and the mean since it last printed).
struct ArmStats
{
    int n{0};
    float handOff{0.f}, handOffMost{0.f};   // the hand's grip from the solve's (units)
    float gripTurn{0.f}, gripTurnMost{0.f}; // its turn from the solve's (degrees)
    float held{0.f}, heldMost{0.f};         // the limits' hold on its joints (degrees, summed)
    float part[4]{}, partMost[4]{};         // ... each joint's (clavicle, shoulder, elbow, wrist)
    float elbowOff{0.f}, elbowOffMost{0.f}; // the elbow from the solve's (units)
};
ArmStats armStats;

// The authored bones' indices of a side (0 right, 1 left), the rig's (by name, once per rig).
struct Side
{
    int clav{-1}, upper{-1}, fore{-1}, hand{-1}, thigh{-1}, shin{-1}, foot{-1};
};
struct Bones
{
    const Rig* rig{nullptr};
    int generation{-1};
    int chest{-1}, pelvis{-1}, weapon{-1};
    Side side[2];
};
Bones bonesCache; // (the last rig's: one authored rig in the game)

[[nodiscard]] const Bones& bonesOf(const Rig& a)
{
    if(bonesCache.rig == &a && bonesCache.generation == a.generation)
    {
        return bonesCache;
    }
    Bones& o = bonesCache;
    o = Bones{};
    o.rig = &a;
    o.generation = a.generation;
    o.chest = boneByName(a, "chest");
    o.pelvis = boneByName(a, "pelvis");
    o.weapon = boneByName(a, "weapon");
    for(int s = 0; s < 2; s++)
    {
        const char* x = s == 0 ? "r" : "l";
        char n[32];
        const auto find = [&](const char* what) {
            q_snprintf(n, sizeof(n), "%s_%s", what, x);
            return boneByName(a, n);
        };
        Side& d = o.side[s];
        d.clav = find("clavicle");
        d.upper = find("upperarm");
        d.fore = find("forearm");
        d.hand = find("hand");
        d.thigh = find("thigh");
        d.shin = find("shin");
        d.foot = find("foot");
    }
    return o;
}

} // namespace

// ---- The latch's reset ----

void resetGame()
{
    // What was made from the grunt's rig (the other one now): made again as it is next needed.
    ragdoll::retireAuthored();
    agarms::resetModels();
    aglegs::resetModels();
    agbody::resetModels();
    hitreact::resetModels();
    mem::on(mem::ModelReload); // (the animation smoothing's per-model data: vr_animsmooth.cpp)
}

// ---- The arms and legs laid on the authored rig ----

void armsOnRig(const Rig& a, const agarms::Placed& placed, rigpose::Rigid* F, rigpose::Rigid* weaponCorr)
{
    const Bones& k = bonesOf(a);
    if(k.chest < 0)
    {
        return;
    }
    for(int s = 0; s < 2; s++)
    {
        weaponCorr[s] = rigpose::Rigid{};
        const Side& d = k.side[s];
        if(d.clav < 0 || d.upper < 0 || d.fore < 0 || d.hand < 0)
        {
            continue;
        }

        const Bone& C = a.bones[d.clav];
        const Bone& U = a.bones[d.upper];
        const Bone& Fo = a.bones[d.fore];
        const Bone& H = a.bones[d.hand];
        // The clavicle aimed at the shoulder the solve put (its turn on the chest: the chest's twist kept).
        const rigpose::Rigid& Fch = F[C.parent];
        const glm::vec3 cp = rigpose::apply(Fch, C.pivot);
        const glm::vec3 cr = Fch.rot * (U.pivot - C.pivot);
        float hp[4]{};
        const bool clampArms = vr_rig2_arm_limits.value != 0.f; // (else the solve's turns: its own limits)
        F[d.clav] = limited(a, d.clav, Fch, glm::normalize(between(cr, placed.shoulder[s] - cp) * Fch.rot), &hp[0]);
        // The upper arm: the solve's turn (its twist: the elbow's plane) re-aimed from the shoulder as now at its elbow.
        const glm::vec3 S = rigpose::apply(F[d.clav], U.pivot);
        const glm::quat uRot = placed.upperM[s].rot;
        const glm::vec3 uDir = uRot * (Fo.pivot - U.pivot);
        F[d.upper] = limited(a, d.upper, F[d.clav], glm::normalize(between(uDir, placed.elbow[s] - S) * uRot), &hp[1], clampArms);
        // The forearm's hinge bent toward the wrist the solve put.
        const glm::vec3 W = rigpose::apply(placed.handM[s], H.pivot);
        const glm::vec3 E = rigpose::apply(F[d.upper], Fo.pivot);
        const glm::vec3 now = anglesOf(a, d.fore, glm::normalize(glm::inverse(F[d.upper].rot) * placed.foreM[s].rot));
        const glm::vec3 bend = aimAngles(a, d.fore, F[d.upper], W - E, now);
        hp[2] += za::abs(clampAngles(a, d.fore, now).x - now.x);
        F[d.fore] = child(Fo, F[d.upper], localOf(a, d.fore, bend));
        {
            // The hand's twist about the forearm half the forearm's (its two bones turning: the skin twists along it,
            // not all at the wrist), as the arms' own skin shares it (Spec::twistShare); its hinge's bend as it was.
            const glm::vec3 axis = glm::normalize(H.pivot - Fo.pivot); // (rest space)
            const glm::quat hl = glm::normalize(glm::inverse(F[d.fore].rot) * placed.handM[s].rot);
            const float tw = 2.f * za::atan2(glm::dot(glm::vec3{hl.x, hl.y, hl.z}, axis), hl.w);
            const float wrapped = tw > pi ? tw - 2.f * pi : tw < -pi ? tw + 2.f * pi : tw;
            const glm::quat turn = glm::angleAxis(wrapped * za::clamp(vr_rig2_forearm_twist.value, 0.f, 1.f), axis);
            F[d.fore] = compose(F[d.fore], rigpose::Rigid{turn, Fo.pivot - turn * Fo.pivot});
        }
        // The hand to its grip frame's turn.
        F[d.hand] = limited(a, d.hand, F[d.fore], placed.handM[s].rot, &hp[3], clampArms);
        if(s == 0 && k.weapon >= 0)
        {
            F[k.weapon] = F[d.hand]; // (fixed on the right hand)
        }
        // Its weapon carried as its hand is drawn; the numbers.
        weaponCorr[s] = compose(F[d.hand], inverse(placed.handM[s]));
        const float off = glm::length(rigpose::apply(F[d.hand], H.end) - rigpose::apply(placed.handM[s], H.end));
        const float turn = turnDeg(F[d.hand].rot, placed.handM[s].rot);
        const float elbowOff = glm::length(E - placed.elbow[s]);
        ArmStats& t = armStats;
        t.n++;
        t.handOff += off;
        t.handOffMost = za::max(t.handOffMost, off);
        t.gripTurn += turn;
        t.gripTurnMost = za::max(t.gripTurnMost, turn);
        const float held = hp[0] + hp[1] + hp[2] + hp[3];
        t.held += held;
        t.heldMost = za::max(t.heldMost, held);
        for(int i = 0; i < 4; i++)
        {
            t.part[i] += hp[i];
            t.partMost[i] = za::max(t.partMost[i], hp[i]);
        }
        t.elbowOff += elbowOff;
        t.elbowOffMost = za::max(t.elbowOffMost, elbowOff);
    }
}

void legsOnRig(const Rig& a, const aglegs::Placed& lp, rigpose::Rigid* F)
{
    const Bones& k = bonesOf(a);
    const float on = za::clamp(lp.on, 0.f, 1.f);
    if(on <= 0.f)
    {
        return;
    }
    for(int s = 0; s < 2; s++)
    {
        const Side& d = k.side[s];
        if(d.thigh < 0 || d.shin < 0)
        {
            continue;
        }
        const rigpose::Rigid before[3] = {F[d.thigh], F[d.shin], d.foot >= 0 ? F[d.foot] : rigpose::Rigid{}};
        const Bone& T = a.bones[d.thigh];
        const Bone& Sh = a.bones[d.shin];
        const rigpose::Rigid& Fp = F[T.parent];
        // The thigh at the knee, the knee's hinge across the leg's bend (the bend's plane: hip, knee, ankle).
        const glm::vec3 hip = rigpose::apply(Fp, T.pivot);
        const glm::vec3 t0 = Sh.pivot - T.pivot;
        const glm::vec3 A0 = Sh.axis;
        const glm::vec3 up = lp.knee[s] - hip, low = lp.ankle[s] - lp.knee[s];
        const glm::vec3 n = glm::cross(up, low);
        const glm::vec3 n1 = glm::length(n) > 1e-3f * glm::length(up) * glm::length(low) ? n : F[d.thigh].rot * A0;
        rigpose::Rigid solved[3];
        solved[0] = limited(a, d.thigh, Fp, align(t0, A0, up, n1), nullptr);
        const glm::vec3 knee = rigpose::apply(solved[0], Sh.pivot);
        const glm::vec3 bend = aimAngles(a, d.shin, solved[0], lp.ankle[s] - knee, glm::vec3{0.f});
        solved[1] = child(Sh, solved[0], localOf(a, d.shin, bend));
        if(d.foot >= 0)
        {
            const Bone& Ft = a.bones[d.foot];
            const glm::vec3 ankle = rigpose::apply(solved[1], Ft.pivot);
            const glm::quat footNow = glm::normalize(glm::inverse(before[1].rot) * before[2].rot); // (its frame's, on its shin)
            const glm::vec3 cur = anglesOf(a, d.foot, footNow);
            solved[2] = child(Ft, solved[1], localOf(a, d.foot, aimAngles(a, d.foot, solved[1], lp.toe[s] - ankle, cur)));
        }
        const int bones[3] = {d.thigh, d.shin, d.foot};
        for(int i = 0; i < 3; i++)
        {
            if(bones[i] < 0)
            {
                continue;
            }
            F[bones[i]].rot = glm::normalize(glm::slerp(before[i].rot, solved[i].rot, on));
            F[bones[i]].pos = glm::mix(before[i].pos, solved[i].pos, on);
        }
    }
}

// ---- Recording poses from the game (vr_rig2_record; vr_rig2_check's "recorded") ----

namespace
{
struct Recorder
{
    int mask{0};  // 1 ragdolls, 2 armed ones' drawn poses
    int most{50}; // poses kept each
    float every{0.5f};   // seconds between an entity's kept poses (armed)
    bool checkFull{false}; // vr_rig2_check recorded run once full (a test's definite end)
    za::Vector<Pose> poses[2];
    za::Vector<int> lastEnt[2];
    za::Vector<Pose> last[2];      // (each entity's last kept: a settled body not kept twice)
    za::Vector<double> lastAt[2];
};
Recorder recorder;

// The angles of each joint in bones `F` (rest -> now, the rig's order; their locals' turns).
[[nodiscard]] Pose poseOf(const Rig& a, const rigpose::Rigid* F)
{
    Pose p = restPose(a);
    for(int b = 0; b < a.numBones; b++)
    {
        const int par = a.bones[b].parent;
        if(par >= 0 && (a.bones[b].joint == Joint::Ball || a.bones[b].joint == Joint::Hinge))
        {
            p.a[b] = anglesOf(a, b, glm::normalize(glm::inverse(F[par].rot) * F[b].rot));
        }
    }
    return p;
}

void keep(int set, int ent, const Rig& a, const rigpose::Rigid* F, double now, double every)
{
    if(!(recorder.mask & (1 << set)) || static_cast<int>(recorder.poses[set].size()) >= recorder.most)
    {
        return;
    }
    int at = -1;
    for(za::SizeT i = 0; i < recorder.lastEnt[set].size(); i++)
    {
        at = recorder.lastEnt[set][i] == ent ? static_cast<int>(i) : at;
    }
    if(at >= 0 && now - recorder.lastAt[set][static_cast<za::SizeT>(at)] < every && now >= recorder.lastAt[set][static_cast<za::SizeT>(at)])
    {
        return;
    }
    const Pose p = poseOf(a, F);
    if(at >= 0)
    {
        float moved = 0.f;
        for(int b = 0; b < a.numBones; b++)
        {
            moved = za::max(moved, glm::length(p.a[b] - recorder.last[set][static_cast<za::SizeT>(at)].a[b]));
        }
        recorder.lastAt[set][static_cast<za::SizeT>(at)] = now;
        if(moved < 3.f)
        {
            return; // (lying still)
        }
        recorder.last[set][static_cast<za::SizeT>(at)] = p;
    }
    else
    {
        recorder.lastEnt[set].pushBack(ent);
        recorder.last[set].pushBack(p);
        recorder.lastAt[set].pushBack(now);
    }
    recorder.poses[set].pushBack(p);
    if(static_cast<int>(recorder.poses[set].size()) == recorder.most)
    {
        Con_Printf("rig2 record: %d %s poses kept (all asked for)\n", recorder.most, set == 0 ? "ragdoll" : "armed");
        if(recorder.checkFull)
        {
            recorder.checkFull = false;
            Cbuf_AddText(va("vr_rig2_arms;vr_rig2_check %s 0 recorded\n", a.bones[0].name[0] ? "progs/vrgrunt.mdl" : ""));
        }
    }
}
} // namespace

void notePublished(int num, const ragdoll::Rig& rig, int bodies, const glm::quat* rot)
{
    if(!(recorder.mask & 1) || !rig.authored || bodies < rig.numBones - 1)
    {
        return;
    }
    const Rig* a = rigFor(rig.model);
    if(!a || a->numBones != rig.numBones)
    {
        return;
    }
    rigpose::Rigid F[maxBones];
    for(int b = 0; b < rig.numBones; b++)
    {
        F[b].rot = b < bodies ? rot[b] : rot[0];
    }
    keep(0, num, *a, F, cl.time, 0.25);
}

const za::Vector<Pose>& recordedPoses(int set)
{
    return recorder.poses[set & 1];
}

void record_f()
{
    const char* what = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!strcmp(what, "ragdoll") || !strcmp(what, "armed") || !strcmp(what, "all"))
    {
        recorder.mask = !strcmp(what, "ragdoll") ? 1 : !strcmp(what, "armed") ? 2 : 3;
        recorder.most = Cmd_Argc() > 2 ? za::max(atoi(Cmd_Argv(2)), 1) : 50;
        recorder.every = Cmd_Argc() > 3 ? za::max(static_cast<float>(atof(Cmd_Argv(3))), 0.05f) : 0.5f;
        recorder.checkFull = Cmd_Argc() > 4 && !strcmp(Cmd_Argv(4), "check");
    }
    else if(!strcmp(what, "off"))
    {
        recorder.mask = 0;
    }
    else if(!strcmp(what, "clear"))
    {
        const int mask = recorder.mask, most = recorder.most;
        recorder = Recorder{};
        recorder.mask = mask;
        recorder.most = most;
    }
    else if(*what)
    {
        Con_Printf("vr_rig2_record [ragdoll|armed|all [most [every [check]]]|off|clear]: poses from the game (vr_rig2 1) for vr_rig2_check's "
                   "\"recorded\"\n");
    }
    Con_Printf("rig2 record: %s; %d ragdoll poses, %d armed poses kept (at most %d each)\n",
        recorder.mask == 0 ? "off" : recorder.mask == 1 ? "ragdolls" : recorder.mask == 2 ? "armed" : "both",
        static_cast<int>(recorder.poses[0].size()), static_cast<int>(recorder.poses[1].size()), recorder.most);
}

// ---- Live ----

// The bones between the frame lerp's two poses: each bone's local motion (on its parent) blended, carried down.
void posedBetween(const ragdoll::Rig& rig, int pose1, int pose2, float blend, rigpose::Rigid* out)
{
    const float t = pose1 == pose2 ? 0.f : za::clamp(blend, 0.f, 1.f);
    for(int b = 0; b < rig.numBones; b++)
    {
        const ragdoll::Bone& bone = rig.bones[b];
        const rigpose::Rigid k1 = rigpose::key(rig, pose1, b), k2 = rigpose::key(rig, pose2, b);
        if(bone.parent < 0 || bone.parent >= b || bone.joint == ragdoll::Joint::Loose)
        {
            out[b].rot = glm::normalize(glm::slerp(k1.rot, k2.rot, t));
            out[b].pos = glm::mix(k1.pos, k2.pos, t);
            continue;
        }
        const rigpose::Rigid l1 = compose(inverse(rigpose::key(rig, pose1, bone.parent)), k1);
        const rigpose::Rigid l2 = compose(inverse(rigpose::key(rig, pose2, bone.parent)), k2);
        // (about its pivot: the joint's turn, its small gap from the fits lerped)
        const glm::quat r = glm::normalize(glm::slerp(l1.rot, l2.rot, t));
        const glm::vec3 g1 = rigpose::apply(l1, bone.pivot) - bone.pivot, g2 = rigpose::apply(l2, bone.pivot) - bone.pivot;
        const rigpose::Rigid l{r, bone.pivot - r * bone.pivot + glm::mix(g1, g2, t)};
        out[b] = compose(out[bone.parent], l);
    }
}

namespace
{
// Each live entity on the authored rig's frame lerp this frame (advanceLerps: R_SetupAliasFrame's, its .mdl drawn no
// more), by entity number.
struct LerpNow
{
    int frame{-1};
    int pose1{0}, pose2{0};
    float blend{0.f};
};
za::Vector<LerpNow> lerps;

// R_SetupAliasFrame's lerp, read (hitreact.cpp drawnPoses').
void lerpOf(const entity_t* e, const aliashdr_t* hdr, int& pose1, int& pose2, float& blend)
{
    const int num = static_cast<int>(e - cl_entities);
    if(num >= 0 && num < static_cast<int>(lerps.size()) && lerps[static_cast<za::SizeT>(num)].frame >= host_framecount - 1)
    {
        const LerpNow& l = lerps[static_cast<za::SizeT>(num)];
        pose1 = l.pose1;
        pose2 = l.pose2;
        blend = l.blend;
        return;
    }
    const int frame = (e->frame >= 0 && e->frame < hdr->numframes) ? e->frame : 0;
    int posenum = hdr->frames[frame].firstpose;
    if(hdr->frames[frame].numposes > 1)
    {
        posenum += static_cast<int>(cl.time / hdr->frames[frame].interval) % hdr->frames[frame].numposes;
    }
    const bool valid = e->currentpose >= 0 && e->currentpose < hdr->numposes && e->previouspose >= 0 &&
                       e->previouspose < hdr->numposes;
    if(!valid || e->currentpose != posenum || !r_lerpmodels.value)
    {
        pose1 = pose2 = valid ? e->currentpose : posenum;
        blend = 0.f;
        return;
    }
    pose1 = e->previouspose;
    pose2 = e->currentpose;
    blend = za::clamp(static_cast<float>(cl.time - e->lerpstart) / za::max(e->lerptime, 1e-3f), 0.f, 1.f);
}

[[nodiscard]] uint32_t hiddenOf(const ragdoll::Rig& rig, int pose1, int pose2, float blend)
{
    uint32_t hidden = 0;
    const int pose = blend < 0.5f ? pose1 : pose2;
    for(int b = 0; b < rig.numBones; b++)
    {
        hidden |= ragdoll::collapsed(rig, pose, b) ? 1u << b : 0u;
    }
    return hidden;
}
} // namespace

void liveArmed(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, const rigpose::Rigid* kick,
    agarms::Placed& placed, int pose1, int pose2, float blend)
{
    const Rig* a = rigFor(rig.model);
    if(!a || a->numBones != rig.numBones || !e || e->model != rig.model || vr_rig2_look.value == 1.f)
    {
        return;
    }
    rigpose::Rigid F[maxBones];
    for(int b = 0; b < rig.numBones; b++)
    {
        F[b] = kick ? compose(kick[b], posed[b]) : posed[b];
    }
    const int num = static_cast<int>(e - cl_entities);
    aglegs::Placed lp;
    if(aglegs::placed(num, lp))
    {
        legsOnRig(*a, lp, F);
    }
    rigpose::Rigid corr[2];
    armsOnRig(*a, placed, F, corr);
    for(int s = 0; s < 2; s++)
    {
        placed.weapon[s] = compose(corr[s], placed.weapon[s]);
    }
    keep(1, num, *a, F, cl.time, recorder.every); // (vr_rig2_record armed)
    drawLive(num, rig.model, F, rig.numBones, vr_rig2_look.value == 2.f ? ~0u : hiddenOf(rig, pose1, pose2, blend));
}

void advanceLerps()
{
    if(!gameOn() || cls.state != ca_connected || !cl_entities)
    {
        return;
    }
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        entity_t* e = cl_visedicts[i];
        if(!e || !e->model || e->model->type != mod_alias || strchr(e->model->name, '#'))
        {
            continue;
        }
        const int num = static_cast<int>(e - cl_entities);
        const ragdoll::Rig* rig = num > 0 && num < cl_max_edicts && num != cl.viewentity && !ragdoll::published(num) ? ragdoll::rigFor(e->model) : nullptr;
        if(!rig || !rig->authored)
        {
            continue;
        }
        if(static_cast<int>(lerps.size()) <= num)
        {
            lerps.resize(static_cast<za::SizeT>(num + 1));
        }
        LerpNow& l = lerps[static_cast<za::SizeT>(num)];
        VR_AliasLerpNow(e, &l.pose1, &l.pose2, &l.blend);
        l.frame = host_framecount;
    }
}

void liveAll()
{
    if(!gameOn() || cls.state != ca_connected || !cl_entities || vr_rig2_look.value == 1.f)
    {
        return;
    }
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        entity_t* e = cl_visedicts[i];
        if(!e || !e->model || e->model->type != mod_alias || strchr(e->model->name, '#'))
        {
            continue;
        }
        const int num = static_cast<int>(e - cl_entities);
        if(num <= 0 || num >= cl_max_edicts || num == cl.viewentity || drawnNow(num) || ragdoll::published(num))
        {
            continue;
        }
        const ragdoll::Rig* rig = ragdoll::rigFor(e->model);
        const auto* hdr = rig && rig->authored ? static_cast<const aliashdr_t*>(Mod_Extradata(e->model)) : nullptr;
        if(!hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || rig->numPoses != hdr->numposes)
        {
            continue;
        }
        int pose1 = 0, pose2 = 0;
        float blend = 0.f;
        lerpOf(e, hdr, pose1, pose2, blend);
        rigpose::Rigid posed[ragdoll::maxBones];
        posedBetween(*rig, pose1, pose2, blend, posed);
        rigpose::Rigid kick[ragdoll::maxBones];
        const bool kicked = vr_hit_react.value == 1.f && hitreact::reacting(e) && hitreact::boneKicks(e, *rig, posed, kick);
        for(int b = 0; kicked && b < rig->numBones; b++)
        {
            posed[b] = compose(kick[b], posed[b]);
        }
        drawLive(num, e->model, posed, rig->numBones, vr_rig2_look.value == 2.f ? ~0u : hiddenOf(*rig, pose1, pose2, blend));
    }
}

// vr_rig2_frames [model]: the model's frames against the authored joints as the ragdoll has them (Box3D's frames): each
// joint's worst swing past its cone and twist past its limit over all its frames, and the same in the rig's own angles
// (anglesOf against clampAngles): the two must agree (the frames' conversion), and a frame far past a limit is a ragdoll
// that snaps as it is made.
void framesCheck_f()
{
    const char* name = Cmd_Argc() > 1 ? Cmd_Argv(1) : "progs/soldier.mdl";
    qmodel_t* model = Mod_ForName(name, false);
    const ragdoll::Rig* rig = model ? ragdoll::rigFor(model) : nullptr;
    const Rig* a = model ? rigFor(model) : nullptr;
    if(!rig || !rig->authored || !a)
    {
        Con_Printf("rig2 frames: %s has no authored ragdoll rig (vr_rig2 1 at the map's spawn?)\n", name);
        return;
    }
    const auto swingTwist = [](const glm::quat& q, float& swingAz, float& swing, float& twist) {
        const glm::quat r = q.w < 0.f ? -q : q;
        twist = 2.f * za::atan2(r.z, r.w);
        const glm::quat tw = glm::angleAxis(twist, glm::vec3{0.f, 0.f, 1.f});
        const glm::quat s = glm::normalize(r * glm::inverse(tw));
        const glm::vec3 z = s * glm::vec3{0.f, 0.f, 1.f};
        swing = za::acos(za::clamp(z.z, -1.f, 1.f));
        swingAz = za::atan2(z.y, z.x);
    };
    for(int b = 1; b < rig->numBones; b++)
    {
        const ragdoll::Bone& bone = rig->bones[b];
        if(!bone.framed || bone.parent < 0)
        {
            continue;
        }
        float worstSwing = -1e9f, worstTwist = -1e9f, worstRig = 0.f;
        int atSwing = 0, atTwist = 0;
        for(int p = 0; p < rig->numPoses; p++)
        {
            glm::quat rp, rb;
            glm::vec3 tp, tb;
            ragdoll::bonePose(*rig, p, bone.parent, rp, tp);
            ragdoll::bonePose(*rig, p, b, rb, tb);
            const glm::quat rel = glm::normalize(glm::inverse(rp) * rb);
            const glm::quat j = glm::normalize(glm::inverse(bone.frameA) * rel * bone.frameB);
            const glm::vec3 ang = anglesOf(*a, b, rel);
            worstRig = za::max(worstRig, glm::length(clampAngles(*a, b, ang) - ang));
            if(bone.joint == ragdoll::Joint::Ball)
            {
                float az, sw, tw;
                swingTwist(j, az, sw, tw);
                const float ca = bone.cone, cb = bone.coneY > 0.f ? bone.coneY : bone.cone;
                const float c = za::cos(az), s = za::sin(az);
                const float lim = ca * cb / za::max(za::sqrt((cb * c) * (cb * c) + (ca * s) * (ca * s)), 1e-6f);
                if(sw - lim > worstSwing)
                {
                    worstSwing = sw - lim;
                    atSwing = p;
                }
                if(za::abs(tw) - bone.twist > worstTwist)
                {
                    worstTwist = za::abs(tw) - bone.twist;
                    atTwist = p;
                }
            }
            else
            {
                const float tw = 2.f * za::atan2(j.z, j.w);
                const float past = za::max(bone.lower - tw, tw - bone.upper);
                if(past > worstSwing)
                {
                    worstSwing = past;
                    atSwing = p;
                }
            }
        }
        Con_Printf("rig2 frames: %-11s worst %s %+6.1f deg (pose %d), twist %+6.1f (pose %d); the rig's angles past by %.1f\n",
            bone.name, bone.joint == ragdoll::Joint::Ball ? "swing past its cone" : "hinge past its range", worstSwing / 0.0174533f,
            atSwing, bone.joint == ragdoll::Joint::Ball ? worstTwist / 0.0174533f : 0.f, atTwist, worstRig);
    }
}

// vr_rig2_live_check: each live entity drawn on the authored rig this frame, its mesh as drawn (the blend of its bones)
// against its frames' (the lerp's two poses, as Quake draws it): units rms and at most, the vertex worst off and its
// heaviest bone (its gun hidden or not: the weapon bone's vertices skipped where collapsed).
void liveCheck_f()
{
    int n = 0;
    for(int i = 1; i < cl.num_entities; i++)
    {
        entity_t copy;
        if(!unswapped(i, copy))
        {
            continue;
        }
        const entity_t* e = &copy;
        const int num = i;
        if(!e->model || e->model->type != mod_alias || strchr(e->model->name, '#'))
        {
            continue;
        }
        const ragdoll::Rig* rig = ragdoll::rigFor(e->model);
        const auto* hdr = rig && rig->authored ? static_cast<const aliashdr_t*>(Mod_Extradata(e->model)) : nullptr;
        if(!hdr)
        {
            if(rig)
            {
                Con_Printf("rig2 live: ent %d %s on its derived rig (vr_rig2 %d at the map's spawn; an authored one %s)\n", num,
                    e->model->name, gameOn() ? 1 : 0, rigFor(e->model) ? "read" : "none");
                n++;
            }
            continue;
        }
        rigpose::Rigid F[maxBones];
        bool ok = true;
        for(int b = 0; b < rig->numBones; b++)
        {
            ok = ok && drawnBone(num, b, F[b].rot, F[b].pos);
        }
        if(!ok)
        {
            Con_Printf("rig2 live: ent %d %s (frame %d) not drawn on the authored rig lately (drawn now %d)\n", num,
                e->model->name, e->frame, drawnNow(num) ? 1 : 0);
            n++;
            continue;
        }
        int pose1 = 0, pose2 = 0;
        float blend = 0.f;
        lerpOf(e, hdr, pose1, pose2, blend);
        const uint32_t hidden = hiddenOf(*rig, pose1, pose2, blend);
        const auto* tv = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
        const auto at = [&](int pose, int v) {
            const trivertx_t& t = tv[pose * hdr->numverts + v];
            return glm::vec3{t.v[0] * hdr->scale[0] + hdr->scale_origin[0], t.v[1] * hdr->scale[1] + hdr->scale_origin[1],
                t.v[2] * hdr->scale[2] + hdr->scale_origin[2]};
        };
        double sum = 0.0;
        float most = 0.f;
        int worst = 0, count = 0;
        for(int v = 0; v < rig->numVerts && v < static_cast<int>(rig->weights.size()); v++)
        {
            const ragdoll::VertWeights& w = rig->weights[static_cast<za::SizeT>(v)];
            if((hidden >> w.bone[0]) & 1u)
            {
                continue;
            }
            glm::vec3 q{0.f};
            for(int k = 0; k < w.n; k++)
            {
                q += w.w[k] * rigpose::apply(F[w.bone[k]], at(0, v));
            }
            const glm::vec3 want = glm::mix(at(pose1, v), at(pose2, v), pose1 == pose2 ? 0.f : blend);
            const float d = glm::length(q - want);
            sum += static_cast<double>(d * d);
            worst = d > most ? v : worst;
            most = za::max(most, d);
            count++;
        }
        Con_Printf("rig2 live: ent %d %s frame %d (poses %d..%d at %.2f): drawn %.2f units rms off its frames, %.2f at most "
                   "(vertex %d, on %s)\n",
            num, e->model->name, e->frame, pose1, pose2, blend, za::sqrt(sum / za::max(count, 1)), most, worst,
            rig->bones[rig->weights[static_cast<za::SizeT>(worst)].bone[0]].name);
        n++;
    }
    if(!n)
    {
        Con_Printf("rig2 live: nothing drawn on the authored rig now\n");
    }
}

// vr_rig2_parts: each ragdoll on an authored rig, each part's middle as drawn (world units) and how far its joint is from
// its parent's (a joint pulled apart: an unstable ragdoll).
void partsReport_f()
{
    for(int num = 1; num < cl.num_entities; num++)
    {
        glm::quat r0;
        glm::vec3 p0;
        float scale = 1.f;
        const ragdoll::Rig* rig = nullptr;
        if(!ragdoll::drawnPart(num, 0, r0, p0, scale, &rig) || !rig || !rig->authored)
        {
            continue;
        }
        Con_Printf("rig2 parts: ent %d\n", num);
        for(int b = 0; b < rig->numBones; b++)
        {
            glm::quat r, rp;
            glm::vec3 p, pp;
            float s = 1.f;
            if(!ragdoll::drawnPart(num, b, r, p, s))
            {
                continue;
            }
            const ragdoll::Bone& bone = rig->bones[b];
            const glm::vec3 mid = r * ((bone.pivot + bone.end) * 0.5f * s) + p;
            float gap = 0.f;
            if(bone.parent >= 0 && ragdoll::drawnPart(num, bone.parent, rp, pp, s))
            {
                gap = glm::length((r * (bone.pivot * s) + p) - (rp * (bone.pivot * s) + pp));
            }
            Con_Printf("  %-11s at %7.1f %7.1f %7.1f, joint apart %.2f\n", bone.name, mid.x, mid.y, mid.z, gap);
        }
    }
}

void armsReport_f()
{
    const ArmStats& t = armStats;
    const float n = static_cast<float>(za::max(t.n, 1));
    Con_Printf("rig2 arms: %d arms laid; hand off its solve %.2f mean, %.2f most (units); grip turned %.1f mean, %.1f most "
               "(deg); elbow off %.2f mean, %.2f most (units); limits held %.1f mean, %.1f most (deg)\n",
        t.n, t.handOff / n, t.handOffMost, t.gripTurn / n, t.gripTurnMost, t.elbowOff / n, t.elbowOffMost, t.held / n, t.heldMost);
    Con_Printf("rig2 arms: limits held per joint (mean / most, deg): clavicle %.1f / %.1f, shoulder %.1f / %.1f, elbow %.1f / %.1f, "
               "wrist %.1f / %.1f\n",
        t.part[0] / n, t.partMost[0], t.part[1] / n, t.partMost[1], t.part[2] / n, t.partMost[2], t.part[3] / n, t.partMost[3]);
    armStats = ArmStats{};
}

} // namespace qvr::rig2
