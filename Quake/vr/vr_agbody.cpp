// vr_agbody.cpp -- armed humanoids' body proxies (vr_agbody.hpp; ROUND22.md, "Armed humanoids in motion: the glitch
// finder and a robust solve").

#include <type_traits>
#include <string.h>
#include "vr_agbody.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

using namespace qvr;

namespace
{

constexpr int maxModels = 4;
constexpr float fitShare = 0.8f; // a radius: this share of its bone's vertices within it along that axis
constexpr int maxUppers = 64;
constexpr float upperEase = 0.25f;   // seconds the procedural upper body takes to come in or go (a pickup's crouch)
constexpr float breathPeriod = 3.4f; // seconds a breath
constexpr float upperTilt = 35.f;   // degrees the hips tip (from their rest) past which the frames' chest is kept (falls, deaths)
constexpr float upperLag = 0.04f;    // seconds the upper body takes to follow its hips (an unlerped frame's jump smoothed)

// The procedural upper body of one armed humanoid.
struct Upper
{
    int ent{0};
    int frame{-1};    // host_framecount laid on
    double time{0.0}; // cl.time then
    float w{0.f};     // its share (eased)
    rigpose::Rigid corr;
    bool on{false};
    rigpose::Rigid chest; // the chest wanted (rest -> now), followed
    bool have{false};
};

struct State
{
    agbody::Proxies models[maxModels];
    int next{0};
    za::Vector<float> scratch[6]; // (the fit's distances, per axis and side)
    Upper uppers[maxUppers];
};
State state;

[[nodiscard]] rigpose::Rigid composeR(const rigpose::Rigid& a, const rigpose::Rigid& b)
{
    rigpose::Rigid r;
    r.rot = glm::normalize(a.rot * b.rot);
    r.pos = a.rot * b.pos + a.pos;
    return r;
}

[[nodiscard]] rigpose::Rigid inverseR(const rigpose::Rigid& a)
{
    rigpose::Rigid r;
    r.rot = glm::inverse(a.rot);
    r.pos = -(r.rot * a.pos);
    return r;
}

[[nodiscard]] Upper* upperOf(int ent, bool make)
{
    Upper* free = nullptr;
    for(Upper& u : state.uppers)
    {
        if(u.ent == ent)
        {
            return &u;
        }
        if(!free && (u.ent == 0 || u.time < cl.time - 1.0 || u.time > cl.time + 1.0))
        {
            free = &u;
        }
    }
    if(!make || !free)
    {
        return nullptr;
    }
    *free = Upper{};
    free->ent = ent;
    return free;
}

[[nodiscard]] glm::vec3 closest(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b)
{
    const glm::vec3 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    const float t = len2 > 1e-6f ? za::clamp(glm::dot(p - a, ab) / len2, 0.f, 1.f) : 0.f;
    return a + t * ab;
}

bool fit(const ragdoll::Rig& rig, int bone, agbody::Proxy& out, float sides = 1.f)
{
    if(bone < 0 || bone >= rig.numBones || rig.bones[bone].points.size() < 8)
    {
        return false;
    }
    const ragdoll::Bone& b = rig.bones[bone];
    out.bone = bone;
    out.a = b.pivot;
    out.b = b.end;
    for(za::Vector<float>& s : state.scratch)
    {
        s.clear();
    }
    const glm::vec3 ab = out.b - out.a;
    const float len2 = za::max(glm::dot(ab, ab), 1e-6f);
    for(const glm::vec3& p : b.points)
    {
        const glm::vec3 q = p - closest(p, out.a, out.b);
        const bool low = sides >= 1.f || glm::dot(p - out.a, ab) / len2 <= sides;
        for(int i = 0; i < 3; i++)
        {
            // (each side its own: a chest, a backpack; the sides and back from the lower part only when `sides` < 1)
            if(low || (i == 0 && q[i] >= 0.f) || i == 2)
            {
                state.scratch[q[i] >= 0.f ? i : 3 + i].pushBack(za::abs(q[i]));
            }
        }
    }
    // (a side with a few points, 1 to 3, its farthest: the enforcer's chest front under its rifle has 2 (the rifle the
    // weapon's), and its other side's radius was its power pack's, 15 units ahead of its chest)
    const auto radius = [](za::Vector<float>& s) {
        za::quickSort(s.begin(), s.end());
        return s.empty() ? -1.f : s.size() < 4 ? za::max(0.5f, s[s.size() - 1]) : za::max(0.5f, s[static_cast<za::SizeT>(fitShare * static_cast<float>(s.size() - 1))]);
    };
    for(int i = 0; i < 3; i++)
    {
        const float pos = radius(state.scratch[i]), neg = radius(state.scratch[3 + i]);
        out.radii[i] = pos > 0.f ? pos : za::max(neg, 0.5f);
        out.back[i] = neg > 0.f ? neg : out.radii[i];
    }
    // (along the bone the segment's ends are capped as round as its thinner side)
    const glm::vec3 axis = glm::normalize(out.b - out.a + glm::vec3{0.f, 0.f, 1e-4f});
    for(int i = 0; i < 3; i++)
    {
        if(za::abs(axis[i]) > 0.7f)
        {
            const float thin = za::min(za::min(out.radii[(i + 1) % 3], out.radii[(i + 2) % 3]),
                za::min(out.back[(i + 1) % 3], out.back[(i + 2) % 3]));
            out.radii[i] = thin;
            out.back[i] = thin;
        }
    }
    return true;
}

} // namespace

namespace qvr::agbody
{

const Proxies* of(const qmodel_t* model, const ragdoll::Rig& rig, int chest, float sides)
{
    sides = vr_agbody_torso_sides.value > 0.f ? vr_agbody_torso_sides.value : sides; // (the tuning's override)
    sides = za::clamp(sides, 0.2f, 1.f);
    Proxies* again = nullptr;
    for(Proxies& m : state.models)
    {
        if(m.model == model)
        {
            if(m.sides == sides)
            {
                return m.ok ? &m : nullptr;
            }
            again = &m; // (fitted with another share: fitted again in place)
            break;
        }
    }
    Proxies& m = again ? *again : state.models[state.next];
    if(!again)
    {
        state.next = (state.next + 1) % maxModels;
    }
    m = Proxies{};
    m.model = model;
    m.sides = sides;
    if(chest < 0 || chest >= rig.numBones)
    {
        return nullptr;
    }
    const int bones[maxProxies] = {chest, rig.bones[chest].parent, rig.head};
    for(int i = 0; i < maxProxies; i++)
    {
        if(fit(rig, bones[i], m.p[m.n], i == Torso ? sides : 1.f))
        {
            m.n++;
        }
        else if(i == Torso)
        {
            return nullptr;
        }
        else
        {
            m.p[m.n] = Proxy{}; // (a part it lacks: none there)
            m.n++;
        }
    }
    m.ok = true;
    return &m;
}

Posed pose(const Proxies& px, const rigpose::Rigid* posed, const rigpose::Rigid* kick)
{
    Posed q;
    q.n = px.n;
    for(int i = 0; i < px.n; i++)
    {
        const Proxy& p = px.p[i];
        if(p.bone < 0)
        {
            q.radii[i] = glm::vec3{0.f};
            continue;
        }
        rigpose::Rigid r = posed[p.bone];
        if(kick)
        {
            const rigpose::Rigid& k = kick[p.bone];
            r.rot = glm::normalize(k.rot * r.rot);
            r.pos = k.rot * r.pos + k.pos;
        }
        q.a[i] = rigpose::apply(r, p.a);
        q.b[i] = rigpose::apply(r, p.b);
        q.rot[i] = r.rot;
        q.radii[i] = p.radii;
        q.back[i] = p.back;
    }
    return q;
}

float depth(const Posed& q, const glm::vec3& p, float margin, glm::vec3* push, int* part)
{
    float most = -1e9f;
    if(push)
    {
        *push = glm::vec3{0.f};
    }
    for(int i = 0; i < q.n; i++)
    {
        if(q.radii[i].x <= 0.f)
        {
            continue;
        }
        const glm::vec3 c = closest(p, q.a[i], q.b[i]);
        const glm::vec3 d = p - c;
        const glm::vec3 l = glm::inverse(q.rot[i]) * d; // (its rest axes)
        const glm::vec3 r = glm::vec3{l.x >= 0.f ? q.radii[i].x : q.back[i].x, l.y >= 0.f ? q.radii[i].y : q.back[i].y,
                                l.z >= 0.f ? q.radii[i].z : q.back[i].z} +
                            glm::vec3{margin};
        const float e = za::sqrt((l.x / r.x) * (l.x / r.x) + (l.y / r.y) * (l.y / r.y) + (l.z / r.z) * (l.z / r.z));
        const float len = glm::length(d);
        // (its depth: from it out to the surface along the line from the axis; on the axis, the thinnest radius)
        const float dep = e > 1e-4f ? len * (1.f / e - 1.f) : za::min(r.x, za::min(r.y, r.z));
        if(dep > most)
        {
            most = dep;
            if(part)
            {
                *part = i;
            }
            if(push)
            {
                const glm::vec3 dir = len > 1e-4f ? d / len : q.rot[i] * glm::vec3{1.f, 0.f, 0.f};
                *push = dep > 0.f ? dir * dep : glm::vec3{0.f};
            }
        }
    }
    return most;
}

bool upper(int ent, const ragdoll::Rig& rig, int chest, const rigpose::Rigid* posed, bool keepFrames, rigpose::Rigid& corr)
{
    corr = rigpose::Rigid{};
    int pelvis = chest >= 0 && chest < rig.numBones ? rig.bones[chest].parent : -1;
    while(rig.authored && pelvis >= 0 && rig.bones[pelvis].parent >= 0)
    {
        pelvis = rig.bones[pelvis].parent; // (vr_rig2 1: the authored chest on its spine on the pelvis: the hips the root's)
    }
    Upper* u = vr_agrunt_upper.value != 0.f && pelvis >= 0 ? upperOf(ent, true) : nullptr;
    if(!u)
    {
        if(Upper* old = upperOf(ent, false))
        {
            old->on = false;
        }
        return false;
    }
    if(u->frame == host_framecount)
    {
        corr = u->corr;
        return u->on;
    }
    const float dt = u->frame < 0 ? 0.f : static_cast<float>(za::clamp(cl.time - u->time, 0.0, 0.25));
    // (hips tipped well over, a fall's or a death's frames: theirs, readable)
    const rigpose::Rigid hips = composeR(posed[pelvis], inverseR(rigpose::key(rig, 0, pelvis)));
    const float tilt = za::acos(za::clamp(glm::dot(hips.rot * glm::vec3{0.f, 0.f, 1.f}, glm::vec3{0.f, 0.f, 1.f}), -1.f, 1.f)) * (180.f / 3.14159265f);
    const bool frames = keepFrames || tilt > upperTilt;
    u->w = u->frame < 0 ? (frames ? 0.f : 1.f) : za::clamp(u->w + (frames ? -dt : dt) / upperEase, 0.f, 1.f);
    u->frame = host_framecount;
    u->time = cl.time;
    // The chest upright as at rest (pose 0; turned with the hips by vr_agrunt_upper_hips of theirs), its waist where the
    // hips have it; followed over upperLag (a frame drawn unlerped, a shot's, jumps the hips: not the arms and the gun).
    const rigpose::Rigid rest = rigpose::key(rig, 0, chest);
    const rigpose::Rigid onHips = composeR(posed[pelvis], composeR(inverseR(rigpose::key(rig, 0, pelvis)), rest));
    rigpose::Rigid want;
    want.rot = glm::normalize(glm::slerp(rest.rot, onHips.rot, za::clamp(vr_agrunt_upper_hips.value, 0.f, 1.f)));
    const glm::vec3 waistNow = rigpose::apply(onHips, rig.bones[chest].pivot);
    want.pos = waistNow - want.rot * rig.bones[chest].pivot;
    if(u->have && dt > 0.f)
    {
        const float k = 1.f - za::exp(-dt / upperLag);
        want.rot = glm::normalize(glm::slerp(u->chest.rot, want.rot, k));
        want.pos = u->chest.pos + (want.pos - u->chest.pos) * k;
    }
    u->chest = want;
    u->have = true;
    const glm::vec3 waist = rigpose::apply(want, rig.bones[chest].pivot);
    const float breath = (vr_agrunt_freeze.value != 0.f ? 0.f : vr_agrunt_upper_breath.value) * (3.14159265f / 180.f) * // (held still: none)
                         za::sin(static_cast<float>(cl.time) * (6.2831853f / breathPeriod) + 0.37f * static_cast<float>(ent));
    rigpose::Rigid lean;
    lean.rot = glm::angleAxis(breath, want.rot * glm::vec3{0.f, 1.f, 0.f});
    lean.pos = waist - lean.rot * waist;
    const rigpose::Rigid full = composeR(composeR(lean, want), inverseR(posed[chest]));
    // (its share: the turn's angle so much about the frames' waist, the waist moved so much)
    const float w = u->w * u->w * (3.f - 2.f * u->w);
    const glm::vec3 j = rigpose::apply(posed[chest], rig.bones[chest].pivot);
    corr.rot = glm::normalize(glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, full.rot, w));
    corr.pos = j + (rigpose::apply(full, j) - j) * w - corr.rot * j;
    u->corr = corr;
    u->on = w > 0.f;
    return u->on;
}

bool upperNow(int ent, rigpose::Rigid& corr)
{
    const Upper* u = upperOf(ent, false);
    corr = u && u->frame == host_framecount && u->on ? u->corr : rigpose::Rigid{};
    return u && u->frame == host_framecount && u->on;
}

void print()
{
    for(const Proxies& m : state.models)
    {
        if(!m.model || !m.ok)
        {
            continue;
        }
        for(int i = 0; i < m.n; i++)
        {
            const Proxy& p = m.p[i];
            Con_Printf("agbody %s %s: bone %d, %.1f %.1f %.1f to %.1f %.1f %.1f, radii %.1f %.1f %.1f, back %.1f %.1f %.1f%s\n", m.model->name,
                i == Torso ? "torso" : i == Pelvis ? "pelvis" : "head", p.bone, p.a.x, p.a.y, p.a.z, p.b.x, p.b.y, p.b.z,
                p.radii.x, p.radii.y, p.radii.z, p.back.x, p.back.y, p.back.z,
                i == Torso && m.sides < 1.f ? va(" (sides from its lower %.2f)", static_cast<double>(m.sides)) : "");
        }
    }
}

float segmentDepth(const Posed& q, const glm::vec3& p0, const glm::vec3& p1, int steps, float margin)
{
    float most = -1e9f;
    for(int k = 0; k <= steps; k++)
    {
        most = za::max(most, depth(q, p0 + (p1 - p0) * (static_cast<float>(k) / static_cast<float>(za::max(steps, 1))), margin));
    }
    return most;
}

int saveState(int ent, unsigned char* out)
{
    static_assert(sizeof(Upper) <= stateBytes, "the Live tape's room for its state");
    static_assert(std::is_trivially_copyable_v<Upper>, "copied as bytes");
    const Upper* st = upperOf(ent, false);
    if(!st)
    {
        return 0;
    }
    memcpy(out, st, sizeof(Upper));
    return static_cast<int>(sizeof(Upper));
}

void loadState(int ent, const unsigned char* in, int len)
{
    Upper* st = upperOf(ent, len > 0);
    if(!st)
    {
        return;
    }
    if(len != static_cast<int>(sizeof(Upper)))
    {
        *st = Upper{}; // (none then: dropped)
        return;
    }
    memcpy(st, in, sizeof(Upper));
}

void resetModels()
{
    state = State{};
}

} // namespace qvr::agbody
