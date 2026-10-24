// vr_agarms.cpp -- armed humanoids' arms posed and skinned (vr_agarms.hpp; ROUND22.md, "Armed humanoids' arms: posed
// and skinned").

#include "vr_agarms.hpp"
#include "vr_cvars.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <string.h>

using namespace qvr;

namespace
{

constexpr float pi = 3.14159265f;
constexpr float wristBack = 0.34f;   // the wrist: this share of the forearm bone's length back from its end (its hand)
constexpr float elbowBand = 0.22f;   // the elbow's blend: this share of the upper arm's length either side of it
constexpr float wristBand = 0.16f;   // the wrist's: this share of the forearm's, mostly on the forearm's side
constexpr float shoulderIn = 0.12f;  // the shoulder's blend into the body: from this share of the upper arm inside it
constexpr float shoulderOut = 0.42f; // ... to this share along it
constexpr float handReach = 1.5f;    // the hand's segment (wrist to the forearm bone's end) taken this far on (its fingers)
constexpr float armRadius = 0.7f;    // an arm's vertex: within this share of its upper arm's length of its bones' lines
constexpr float reachMost = 0.97f;   // a wrist within this share of the arm's length of its shoulder (straighter: snapping)
constexpr float elbowMost = 155.f;   // degrees: an elbow's most bend
constexpr float twistShare = 0.5f;   // the hand's twist about the forearm carried by the forearm (the rest at the wrist)
constexpr float onIn = 0.12f;        // seconds: posed arms easing in from the animation's
constexpr float onOut = 0.2f;        // ... and back out (a death, a stale pose)
constexpr int numSamples = 36;       // the elbow's places tried round its circle
constexpr glm::vec3 hangDir{0.12f, 0.14f, -1.f};   // a free arm's wrist from its shoulder (chest space; the left's, y mirrored)
constexpr glm::vec3 elbowPref{-0.3f, 0.45f, -1.f}; // an elbow's way from its shoulder: down, out, a little back (the left's)

constexpr int maxModels = 4;
constexpr int maxEnts = 64;
constexpr int errTags = 1024;

struct Side
{
    glm::vec3 S{0.f}, E{0.f}, W{0.f}, end{0.f}; // rest joints (pose 0, model units): shoulder, elbow, wrist, the hand's end
    float Lu{0.f}, Lf{0.f};                     // upper arm (S-E), forearm (E-W)
    glm::vec3 hinge{0.f, 1.f, 0.f};             // the elbow's rest hinge (cross of the upper arm and the forearm)
    rigpose::Rigid grip;                        // the hand's grip frame at rest: its middle, the rest gun's turn
    int handBone{-1};                           // the rig bone most of its hand's vertices are on (a ragdoll's hand)
    za::Vector<int> verts;                      // its vertices (any weight)
    za::Vector<int> hand;                       // ... wholly its hand's
    za::Vector<int> edges;                      // its triangles' edges (vertex pairs): the stretch measured
};

struct Skin
{
    const qmodel_t* model{nullptr};
    bool ok{false};
    Side side[2];
    za::Vector<glm::vec4> row;   // each vertex's rest row (vr_agarms.hpp restRow, w set by it)
    za::Vector<glm::vec3> rest;  // rest places
    int chest{-1};
    glm::vec3 c0{0.f}, c1{0.f};  // the chest bone's line at rest
    float torso{0.f};            // the torso's radius about it
    int anchor{-1};              // the chest vertex nearest between the shoulders (QC's anchor: vr_agarms_info)
};

struct Err
{
    int n{0};
    float gap{0.f};           // the joints' gaps (units; 0 by construction)
    float handRms{0.f};       // the hands' edges against rest (sum of squared ratio - 1, for the rms), most
    float handMost{0.f};
    float armLong{1.f};       // any arm edge's most stretch, most squash (ratios)
    float armShort{1.f};
    float torso{0.f};         // the elbows and forearms inside the torso (units)
    float miss[2]{0.f, 0.f};  // the hands' grips off their goals (reach)
    float second{0.f};        // the off hand off its second grip
    float bend{0.f};          // the wrists' bend (degrees)
    int held2{0};             // frames the off hand held the second grip (of those wanting it)
    int want2{0};
};

struct Ent
{
    int ent{0};
    const qmodel_t* model{nullptr};
    int frame{-1};      // host_framecount solved on
    int easeFrame{-1};  // ... eased on
    double time{0.0};
    float on{0.f};
    rigpose::Rigid bones[agarms::numBones];
    glm::vec3 elbow[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // the last elbows (chest space, from the shoulder)
    bool haveElbow[2]{false, false};
    rigpose::Rigid left;  // the off hand's grip frame wanted, eased (its goal changing)
    bool haveLeft{false};
    int leftMode{-1};     // its goal's kind (Goal, 3 the second grip, 4 a cup) and since when
    double leftModeAt{0.0};
};

struct State
{
    Skin skins[maxModels];
    Ent ents[maxEnts];
    Err errs[errTags];
    Err total;
};
State state;

[[nodiscard]] float smoothstep(float a, float b, float x)
{
    const float t = za::clamp((x - a) / za::max(b - a, 1e-4f), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

[[nodiscard]] glm::vec3 norm(const glm::vec3& v, const glm::vec3& fallback)
{
    const float l = glm::length(v);
    return l > 1e-5f ? v / l : fallback;
}

[[nodiscard]] float segmentDistance(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, float* tOut = nullptr)
{
    const glm::vec3 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    const float t = len2 > 1e-6f ? za::clamp(glm::dot(p - a, ab) / len2, 0.f, 1.f) : 0.f;
    if(tOut)
    {
        *tOut = t;
    }
    return glm::length(p - (a + t * ab));
}

[[nodiscard]] rigpose::Rigid compose(const rigpose::Rigid& a, const rigpose::Rigid& b)
{
    rigpose::Rigid r;
    r.rot = glm::normalize(a.rot * b.rot);
    r.pos = a.rot * b.pos + a.pos;
    return r;
}

[[nodiscard]] rigpose::Rigid inverse(const rigpose::Rigid& a)
{
    rigpose::Rigid r;
    r.rot = glm::inverse(a.rot);
    r.pos = -(r.rot * a.pos);
    return r;
}

// A frame (x forward, z up) as a turn.
[[nodiscard]] glm::quat frameRot(const glm::vec3& fw, const glm::vec3& upHint)
{
    const glm::vec3 x = norm(fw, glm::vec3{1.f, 0.f, 0.f});
    glm::vec3 z = upHint - x * glm::dot(upHint, x);
    z = glm::length(z) > 1e-4f ? glm::normalize(z) : norm(glm::cross(x, glm::vec3{0.f, 1.f, 0.f}), glm::vec3{0.f, 0.f, 1.f});
    const glm::vec3 y = glm::cross(z, x);
    return glm::normalize(glm::quat_cast(glm::mat3{x, y, z}));
}

// The turn taking frame (a, b) to (c, d): a, c the main axes, b, d hints across them.
[[nodiscard]] glm::quat alignRot(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d)
{
    return glm::normalize(frameRot(c, d) * glm::inverse(frameRot(a, b)));
}

[[nodiscard]] float angleDeg(const glm::quat& q)
{
    const glm::quat t = q.w < 0.f ? -q : q;
    return 2.f * za::atan2(za::sqrt(t.x * t.x + t.y * t.y + t.z * t.z), t.w) * (180.f / pi);
}

// q's twist about `axis` (radians, signed).
[[nodiscard]] float twistAbout(const glm::quat& q, const glm::vec3& axis)
{
    const glm::vec3 v{q.x, q.y, q.z};
    const float p = glm::dot(v, axis);
    float a = 2.f * za::atan2(p, q.w);
    if(a > pi)
    {
        a -= 2.f * pi;
    }
    if(a < -pi)
    {
        a += 2.f * pi;
    }
    return a;
}

[[nodiscard]] float octPack(const glm::vec3& n0)
{
    glm::vec3 n = n0 / za::max(za::abs(n0.x) + za::abs(n0.y) + za::abs(n0.z), 1e-6f);
    glm::vec2 e{n.x, n.y};
    if(n.z < 0.f)
    {
        e = glm::vec2{(1.f - za::abs(n.y)) * (n.x >= 0.f ? 1.f : -1.f), (1.f - za::abs(n.x)) * (n.y >= 0.f ? 1.f : -1.f)};
    }
    const float a = za::floor((za::clamp(e.x, -1.f, 1.f) * 0.5f + 0.5f) * 4095.f + 0.5f);
    const float b = za::floor((za::clamp(e.y, -1.f, 1.f) * 0.5f + 0.5f) * 4095.f + 0.5f);
    return a * 4096.f + b;
}

[[nodiscard]] glm::vec3 restVertex(const aliashdr_t* hdr, int v)
{
    const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const trivertx_t& t = base[static_cast<za::SizeT>(v)];
    return glm::vec3{t.v[0], t.v[1], t.v[2]} * glm::vec3{hdr->scale[0], hdr->scale[1], hdr->scale[2]} +
           glm::vec3{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
}

[[nodiscard]] glm::vec3 restNormal(const aliashdr_t* hdr, int v)
{
    const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const int n = base[static_cast<za::SizeT>(v)].lightnormalindex;
    return n < NUMVERTEXNORMALS ? glm::vec3{r_avertexnormals[n][0], r_avertexnormals[n][1], r_avertexnormals[n][2]}
                                : glm::vec3{0.f, 0.f, 1.f};
}

[[nodiscard]] glm::vec3 poseVertex(const aliashdr_t* hdr, int pose1, int pose2, float blend, int v)
{
    const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const za::SizeT nv = static_cast<za::SizeT>(hdr->numverts);
    const trivertx_t& a = base[static_cast<za::SizeT>(pose1) * nv + static_cast<za::SizeT>(v)];
    const trivertx_t& b = base[static_cast<za::SizeT>(pose2) * nv + static_cast<za::SizeT>(v)];
    const glm::vec3 scale{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
    const glm::vec3 origin{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
    const glm::vec3 pa = glm::vec3{a.v[0], a.v[1], a.v[2]} * scale + origin;
    const glm::vec3 pb = glm::vec3{b.v[0], b.v[1], b.v[2]} * scale + origin;
    return pa + (pb - pa) * blend;
}

// `model`'s skin, made once a model (null: none).
const Skin* skinOf(const qmodel_t* model, const aliashdr_t* hdr, const ragdoll::Rig& rig, const agarms::Spec& spec)
{
    Skin* free = nullptr;
    for(Skin& s : state.skins)
    {
        if(s.model == model)
        {
            return s.ok ? &s : nullptr;
        }
        free = (!free && !s.model) ? &s : free;
    }
    if(!free)
    {
        free = &state.skins[spec.kind % maxModels];
    }
    Skin& m = *free;
    m = Skin{};
    m.model = model;
    const int nv = hdr->numverts;
    if(rig.numVerts != nv || rig.numPoses != hdr->numposes || spec.chest < 0)
    {
        return nullptr;
    }
    for(int s = 0; s < 2; s++)
    {
        if(spec.upper[s] < 0 || spec.fore[s] < 0)
        {
            return nullptr;
        }
    }
    m.rest.resize(static_cast<za::SizeT>(nv));
    m.row.resize(static_cast<za::SizeT>(nv), glm::vec4{0.f});
    for(int v = 0; v < nv; v++)
    {
        m.rest[static_cast<za::SizeT>(v)] = restVertex(hdr, v);
    }
    const auto at0 = [&](int b, const glm::vec3& p) { return rigpose::apply(rigpose::key(rig, 0, b), p); };
    m.chest = spec.chest;
    m.c0 = at0(m.chest, rig.bones[m.chest].pivot);
    m.c1 = at0(m.chest, rig.bones[m.chest].end);
    for(int s = 0; s < 2; s++)
    {
        Side& sd = m.side[s];
        sd.S = at0(spec.upper[s], rig.bones[spec.upper[s]].pivot);
        sd.E = at0(spec.fore[s], rig.bones[spec.fore[s]].pivot);
        const glm::vec3 H = at0(spec.fore[s], rig.bones[spec.fore[s]].end);
        sd.W = sd.E + (H - sd.E) * (1.f - wristBack);
        sd.end = H;
        sd.Lu = glm::length(sd.E - sd.S);
        sd.Lf = glm::length(sd.W - sd.E);
        if(sd.Lu < 1.f || sd.Lf < 1.f)
        {
            return nullptr;
        }
        sd.hinge = norm(glm::cross(sd.E - sd.S, sd.W - sd.E), glm::vec3{0.f, s == 0 ? -1.f : 1.f, 0.f});
        m.torso += 0.5f * 0.75f * segmentDistance(sd.S, m.c0, m.c1);
    }
    // Each vertex: the nearest of the rig's bones' lines (rest); an arm's if its upper arm's or forearm's (its hand's line
    // taken on past the forearm bone's end, its fingers) within armRadius of it, or the rig's fit puts it on that arm.
    za::Vector<int8_t> sideOf(static_cast<za::SizeT>(nv), static_cast<int8_t>(-1));
    for(int v = 0; v < nv; v++)
    {
        if(spec.isGun && spec.isGun(spec.row, v))
        {
            continue;
        }
        const glm::vec3& p = m.rest[static_cast<za::SizeT>(v)];
        float best = 1e30f;
        int bestBone = -1;
        for(int b = 0; b < rig.numBones; b++)
        {
            if(rig.bones[b].joint == ragdoll::Joint::Loose)
            {
                continue;
            }
            glm::vec3 a = at0(b, rig.bones[b].pivot), e = at0(b, rig.bones[b].end);
            for(int s = 0; s < 2; s++)
            {
                if(b == spec.fore[s])
                {
                    e = a + (e - a) * (1.f + wristBack * (handReach - 1.f));
                }
            }
            const float d = segmentDistance(p, a, e);
            if(d < best)
            {
                best = d;
                bestBone = b;
            }
        }
        const int fit = rig.vertBone[static_cast<za::SizeT>(v)];
        for(int s = 0; s < 2; s++)
        {
            const bool onLine = (bestBone == spec.upper[s] || bestBone == spec.fore[s]) && best <= armRadius * m.side[s].Lu;
            const bool fitted = (fit == spec.upper[s] || fit == spec.fore[s]) && (bestBone == spec.upper[s] || bestBone == spec.fore[s] ||
                                                                                   best > armRadius * m.side[s].Lu);
            if(onLine || fitted)
            {
                sideOf[static_cast<za::SizeT>(v)] = static_cast<int8_t>(s);
            }
        }
    }
    // Each arm vertex's place along its chain (shoulder 0 .. the hand's end), its bones' weights.
    int handCount[2][ragdoll::maxBones] = {};
    glm::vec3 handMid[2] = {glm::vec3{0.f}, glm::vec3{0.f}};
    for(int v = 0; v < nv; v++)
    {
        const int s = sideOf[static_cast<za::SizeT>(v)];
        glm::vec4& row = m.row[static_cast<za::SizeT>(v)];
        row.z = octPack(restNormal(hdr, v));
        if(s < 0)
        {
            row.x = 0.f;
            row.y = 1.f;
            continue;
        }
        Side& sd = m.side[s];
        const glm::vec3& p = m.rest[static_cast<za::SizeT>(v)];
        const glm::vec3 handEnd = sd.W + (sd.end - sd.W) * handReach;
        float tU = 0.f, tF = 0.f, tH = 0.f;
        const float dU = segmentDistance(p, sd.S, sd.E, &tU);
        const float dF = segmentDistance(p, sd.E, sd.W, &tF);
        const float dH = segmentDistance(p, sd.W, handEnd, &tH);
        const float Lh = glm::length(handEnd - sd.W);
        float along = 0.f; // along the chain (units from the shoulder)
        if(dU <= dF && dU <= dH)
        {
            along = glm::dot(p - sd.S, sd.E - sd.S) / sd.Lu; // (unclamped at the shoulder: the body's blend)
            along = za::min(along, sd.Lu);
        }
        else if(dF <= dH)
        {
            along = sd.Lu + tF * sd.Lf;
        }
        else
        {
            along = sd.Lu + sd.Lf + tH * Lh;
        }
        const float bE = elbowBand * sd.Lu, bW = wristBand * sd.Lf;
        const float wF = smoothstep(sd.Lu - bE, sd.Lu + bE, along);
        const float wH = smoothstep(sd.Lu + sd.Lf - 1.4f * bW, sd.Lu + sd.Lf + 0.4f * bW, along);
        const float c = wH > 0.f ? 1.f + wH : wF;
        row.x = 1.f + static_cast<float>(s) * 4.f + za::min(c, 2.f);
        row.y = 1.f - smoothstep(-shoulderIn * sd.Lu, shoulderOut * sd.Lu, along);
        sd.verts.pushBack(v);
        if(wH >= 1.f)
        {
            sd.hand.pushBack(v);
            handMid[s] += p;
            handCount[s][za::clamp(static_cast<int>(rig.vertBone[static_cast<za::SizeT>(v)]), 0, ragdoll::maxBones - 1)]++;
        }
    }
    for(int s = 0; s < 2; s++)
    {
        Side& sd = m.side[s];
        if(sd.hand.size() < 3 || sd.verts.size() < 12)
        {
            Con_Printf("agarms: %s: its %s arm not found (%d vertices, hand %d)\n", model->name, s == 0 ? "right" : "left",
                static_cast<int>(sd.verts.size()), static_cast<int>(sd.hand.size()));
            return nullptr;
        }
        sd.grip.pos = handMid[s] / static_cast<float>(sd.hand.size());
        sd.grip.rot = spec.gunRot;
        sd.handBone = 0;
        for(int b = 1; b < ragdoll::maxBones; b++)
        {
            sd.handBone = handCount[s][b] > handCount[s][sd.handBone] ? b : sd.handBone;
        }
    }
    // The arms' edges (the stretch measured), the anchor.
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
    const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
    const auto corner = [&](int i) { return za::clamp(static_cast<int>(desc[idx[i]].vertindex), 0, nv - 1); };
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        for(int k = 0; k < 3; k++)
        {
            const int u = corner(i + k), w = corner(i + (k + 1) % 3);
            const int su = sideOf[static_cast<za::SizeT>(u)], sw = sideOf[static_cast<za::SizeT>(w)];
            if(u < w && su >= 0 && su == sw)
            {
                m.side[su].edges.pushBack(u);
                m.side[su].edges.pushBack(w);
            }
        }
    }
    const glm::vec3 mid = (m.side[0].S + m.side[1].S) * 0.5f;
    float bestA = 1e30f;
    for(int v = 0; v < nv; v++)
    {
        if(sideOf[static_cast<za::SizeT>(v)] >= 0 || (spec.isGun && spec.isGun(spec.row, v)) ||
            rig.vertBone[static_cast<za::SizeT>(v)] != m.chest)
        {
            continue;
        }
        const float d = glm::length(m.rest[static_cast<za::SizeT>(v)] - mid);
        if(d < bestA)
        {
            bestA = d;
            m.anchor = v;
        }
    }
    m.ok = true;
    return &m;
}

[[nodiscard]] Ent* entOf(int ent, bool make)
{
    Ent* free = nullptr;
    for(Ent& s : state.ents)
    {
        if(s.ent == ent)
        {
            return &s;
        }
        if(!free && (s.ent == 0 || s.time < cl.time - 1.0 || s.time > cl.time + 1.0))
        {
            free = &s;
        }
    }
    if(!make || !free)
    {
        return nullptr;
    }
    *free = Ent{};
    free->ent = ent;
    return free;
}

[[nodiscard]] int numberOf(const entity_t* e)
{
    const int n = static_cast<int>(e - cl_entities);
    return (n > 0 && n < cl.num_entities) ? n : -1;
}

// The chest as drawn: rest (pose 0) -> model space now.
[[nodiscard]] rigpose::Rigid chestNow(const ragdoll::Rig& rig, int chest, const rigpose::Rigid* posed, const rigpose::Rigid* kick)
{
    rigpose::Rigid c = compose(posed[chest], inverse(rigpose::key(rig, 0, chest)));
    return kick ? compose(kick[chest], c) : c;
}

struct ArmOut
{
    rigpose::Rigid U, F, H;
    float miss{0.f};  // the wrist's goal out of reach (units it was brought in)
    float bend{0.f};  // the wrist's bend (degrees)
    glm::vec3 elbow{0.f};
};

// One arm (rest side `sd`, its shoulder `S1` now, the chest's turn `cq`) to the hand motion `H` (rest -> now: its wrist
// goes where H takes the rest wrist), its elbow chosen round its circle; `prevElbow` the last (chest space), `pole` a
// strike's wanted pole (its length its share).
ArmOut solveArm(const Side& sd, int s, const glm::vec3& S1, const glm::quat& cq, rigpose::Rigid H, const glm::vec3* prevElbow,
    const glm::vec3& pole)
{
    ArmOut o;
    const float a = sd.Lu, b = sd.Lf;
    glm::vec3 W1 = rigpose::apply(H, sd.W);
    // Reach: the wrist brought within it (the hand and its weapon with it).
    const float most = reachMost * (a + b);
    const float least = za::sqrt(za::max(0.f, a * a + b * b - 2.f * a * b * za::cos((180.f - elbowMost) * (pi / 180.f)))) + 0.05f;
    glm::vec3 d = W1 - S1;
    float dl = glm::length(d);
    const glm::vec3 dir0 = norm(d, cq * glm::vec3{1.f, 0.f, 0.f});
    const float want = za::clamp(dl, least, most);
    if(want != dl)
    {
        const glm::vec3 shift = S1 + dir0 * want - W1;
        o.miss = glm::length(shift);
        H.pos += shift;
        W1 += shift;
        dl = want;
    }
    const glm::vec3 dir = dir0;
    const float cosA = za::clamp((a * a + dl * dl - b * b) / (2.f * a * dl), -1.f, 1.f);
    const float sinA = za::sqrt(za::max(0.f, 1.f - cosA * cosA));
    const glm::vec3 C = S1 + dir * (a * cosA);
    const float rho = a * sinA;
    // The circle's basis: e1 the preferred elbow's way across the reach.
    const float sideSign = s == 0 ? -1.f : 1.f;
    glm::vec3 pref = cq * glm::normalize(glm::vec3{elbowPref.x, elbowPref.y * sideSign, elbowPref.z});
    const float poleShare = za::min(glm::length(pole), 1.f);
    if(poleShare > 1e-3f)
    {
        pref = norm(pref * (1.f - poleShare) + pole, pref);
    }
    glm::vec3 e1 = pref - dir * glm::dot(pref, dir);
    if(glm::length(e1) < 1e-3f)
    {
        e1 = cq * glm::vec3{0.f, 0.f, -1.f};
        e1 -= dir * glm::dot(e1, dir);
    }
    e1 = norm(e1, glm::vec3{0.f, 0.f, -1.f});
    const glm::vec3 e2 = glm::cross(dir, e1);
    // The forearm the hand wants (the rest wrist's angle kept), the torso's line now.
    const glm::vec3 fWant = H.rot * glm::normalize(sd.W - sd.E);
    const glm::vec3 chestUp = cq * glm::vec3{0.f, 0.f, 1.f};
    const glm::vec3 inward = cq * glm::vec3{0.f, -sideSign, 0.f};
    const auto cost = [&](float phi) {
        const glm::vec3 E = C + rho * (za::cos(phi) * e1 + za::sin(phi) * e2);
        const glm::vec3 f = norm(W1 - E, fWant);
        const float wrist = za::acos(za::clamp(glm::dot(f, fWant), -1.f, 1.f));
        float c = wrist * wrist;
        const glm::vec3 eDir = norm(E - S1, pref);
        c += 0.5f * (1.f - glm::dot(eDir, pref));
        // An elbow up past its shoulder, or in across the chest.
        const float up = glm::dot(E - S1, chestUp);
        c += up > 0.f ? 0.02f * up * up : 0.f;
        const float in = glm::dot(E - S1, inward);
        c += in > 0.f ? 0.03f * in * in : 0.f;
        if(prevElbow)
        {
            const glm::vec3 pe = S1 + cq * *prevElbow;
            const glm::vec3 dd = E - pe;
            c += 0.01f * glm::dot(dd, dd);
        }
        return c;
    };
    int best = 0;
    float bestC = 1e30f;
    float cs[numSamples];
    for(int i = 0; i < numSamples; i++)
    {
        cs[i] = cost(2.f * pi * static_cast<float>(i) / static_cast<float>(numSamples));
        if(cs[i] < bestC)
        {
            bestC = cs[i];
            best = i;
        }
    }
    const float c0 = cs[(best + numSamples - 1) % numSamples], c2 = cs[(best + 1) % numSamples];
    const float den = c0 - 2.f * bestC + c2;
    const float off = den > 1e-6f ? za::clamp(0.5f * (c0 - c2) / den, -0.5f, 0.5f) : 0.f;
    const float phi = 2.f * pi * (static_cast<float>(best) + off) / static_cast<float>(numSamples);
    const glm::vec3 E1 = C + rho * (za::cos(phi) * e1 + za::sin(phi) * e2);
    o.elbow = E1;
    // The bones: the upper arm and the forearm turned onto their lines about the elbow's hinge (its rest hinge to now's),
    // the forearm twisted towards the hand by twistShare.
    const glm::vec3 u0 = glm::normalize(sd.E - sd.S), f0 = glm::normalize(sd.W - sd.E);
    const glm::vec3 u1 = norm(E1 - S1, dir), f1 = norm(W1 - E1, dir);
    const glm::vec3 h1 = norm(glm::cross(u1, f1), norm(glm::cross(u1, e1), sd.hinge));
    o.U.rot = alignRot(u0, sd.hinge, u1, h1);
    o.U.pos = S1 - o.U.rot * sd.S;
    glm::quat rf = alignRot(f0, sd.hinge, f1, h1);
    const float tw = twistAbout(glm::inverse(rf) * H.rot, f0);
    rf = glm::normalize(rf * glm::angleAxis(tw * twistShare, f0));
    o.F.rot = rf;
    o.F.pos = E1 - rf * sd.E;
    o.H = H;
    o.bend = angleDeg(glm::inverse(rf) * H.rot);
    return o;
}

// The vertex `v` of side `s` as RigArm draws it (model units): the CPU's copy (the numbers).
[[nodiscard]] glm::vec3 skinned(const Skin& m, const rigpose::Rigid* bones, int v, const glm::vec3& body)
{
    const glm::vec4& r = m.row[static_cast<za::SizeT>(v)];
    if(r.x < 0.5f)
    {
        return body;
    }
    const float code = r.x - 1.f;
    const int s = static_cast<int>(za::floor(code * 0.25f));
    const float c = code - static_cast<float>(s) * 4.f;
    const int bone = za::min(static_cast<int>(za::floor(c)), 2);
    const float f = c - static_cast<float>(bone);
    const glm::vec3& rest = m.rest[static_cast<za::SizeT>(v)];
    const rigpose::Rigid& A = bones[3 * s + bone];
    glm::vec3 p;
    if(f > 0.0005f && bone < 2)
    {
        const rigpose::Rigid& B = bones[3 * s + bone + 1];
        glm::quat qb = B.rot;
        if(glm::dot(A.rot, qb) < 0.f)
        {
            qb = -qb;
        }
        const glm::quat q = glm::normalize(glm::quat{A.rot.w + (qb.w - A.rot.w) * f, A.rot.x + (qb.x - A.rot.x) * f,
            A.rot.y + (qb.y - A.rot.y) * f, A.rot.z + (qb.z - A.rot.z) * f});
        const glm::vec3 j0 = bone == 0 ? m.side[s].E : m.side[s].W;
        p = q * (rest - j0) + rigpose::apply(A, j0);
    }
    else
    {
        p = rigpose::apply(A, rest);
    }
    return glm::mix(p, body, r.y);
}

void noteErr(const Skin& m, const Ent& st, const aliashdr_t* hdr, int pose1, int pose2, float blend, const ArmOut* arms,
    const rigpose::Rigid& chest, float second, bool want2, bool held2)
{
    const int tag = static_cast<int>(vr_agrunt_ik_tag.value);
    if(tag < 0 || tag >= errTags)
    {
        return;
    }
    Err& e = state.errs[tag];
    e.n++;
    const glm::vec3 c0 = rigpose::apply(chest, m.c0), c1 = rigpose::apply(chest, m.c1);
    for(int s = 0; s < 2; s++)
    {
        const Side& sd = m.side[s];
        const rigpose::Rigid* b = st.bones + 3 * s;
        const float gap = za::max(glm::length(rigpose::apply(b[0], sd.E) - rigpose::apply(b[1], sd.E)),
            glm::length(rigpose::apply(b[1], sd.W) - rigpose::apply(b[2], sd.W)));
        e.gap = za::max(e.gap, gap);
        e.miss[s] = za::max(e.miss[s], arms[s].miss);
        e.bend = za::max(e.bend, arms[s].bend);
        // In the torso: the elbow, the forearm's middle.
        const glm::vec3 E = rigpose::apply(b[1], sd.E), Wr = rigpose::apply(b[2], sd.W);
        for(const glm::vec3& p : {E, (E + Wr) * 0.5f})
        {
            e.torso = za::max(e.torso, m.torso - segmentDistance(p, c0, c1));
        }
        // The edges as drawn.
        for(za::SizeT i = 0; i + 1 < sd.edges.size(); i += 2)
        {
            const int u = sd.edges[i], w = sd.edges[i + 1];
            const glm::vec3 pu = skinned(m, st.bones, u, poseVertex(hdr, pose1, pose2, blend, u));
            const glm::vec3 pw = skinned(m, st.bones, w, poseVertex(hdr, pose1, pose2, blend, w));
            const float r0 = glm::length(m.rest[static_cast<za::SizeT>(u)] - m.rest[static_cast<za::SizeT>(w)]);
            if(r0 < 0.05f)
            {
                continue;
            }
            const float ratio = glm::length(pu - pw) / r0;
            const bool hand = m.row[static_cast<za::SizeT>(u)].x - 1.f - 4.f * static_cast<float>(s) >= 2.f &&
                              m.row[static_cast<za::SizeT>(w)].x - 1.f - 4.f * static_cast<float>(s) >= 2.f;
            if(hand)
            {
                e.handRms += (ratio - 1.f) * (ratio - 1.f);
                e.handMost = za::max(e.handMost, za::abs(ratio - 1.f));
            }
            // (the shoulder's blend into the animated body is the animation's: not an arm edge's stretch)
            if(m.row[static_cast<za::SizeT>(u)].y < 0.01f && m.row[static_cast<za::SizeT>(w)].y < 0.01f)
            {
                e.armLong = za::max(e.armLong, ratio);
                e.armShort = za::min(e.armShort, ratio);
            }
        }
    }
    e.second = za::max(e.second, second);
    e.want2 += want2 ? 1 : 0;
    e.held2 += held2 ? 1 : 0;
}

} // namespace

namespace qvr::agarms
{

bool enabled()
{
    return vr_agarms.value != 0.f;
}

bool restRow(const qmodel_t* model, const aliashdr_t* hdr, const ragdoll::Rig& rig, const Spec& spec, int v, glm::vec4& out)
{
    out = glm::vec4{0.f};
    const Skin* m = enabled() ? skinOf(model, hdr, rig, spec) : nullptr;
    if(!m || v < 0 || static_cast<za::SizeT>(v) >= m->row.size())
    {
        return false;
    }
    out = m->row[static_cast<za::SizeT>(v)];
    out.w = static_cast<float>(3 * rig.numBones);
    return true;
}

bool solve(const entity_t* e, const aliashdr_t* hdr, const ragdoll::Rig& rig, const Spec& spec, const rigpose::Rigid* posed,
    const rigpose::Rigid* kick, const Targets& t, Placed& out)
{
    out = Placed{};
    const Skin* m = enabled() ? skinOf(e->model, hdr, rig, spec) : nullptr;
    const int num = numberOf(e);
    Ent* st = (m && num > 0) ? entOf(num, true) : nullptr;
    if(!st)
    {
        return false;
    }
    if(st->model != e->model)
    {
        *st = Ent{};
        st->ent = num;
        st->model = e->model;
    }
    const float dt = static_cast<float>(za::clamp(cl.time - st->time, 0.0, 0.25));
    st->time = cl.time;
    st->frame = host_framecount;
    st->easeFrame = host_framecount;
    st->on = za::min(1.f, st->on + (st->on <= 0.f && dt <= 0.f ? 0.f : dt / onIn));
    if(st->on <= 0.f)
    {
        st->on = 0.01f;
    }
    const rigpose::Rigid C = chestNow(rig, m->chest, posed, kick);
    ArmOut arms[2];
    float second = 0.f;
    bool held2 = false;
    // The goals' hand motions (rest -> now).
    rigpose::Rigid Hm[2];
    for(int s = 0; s < 2; s++)
    {
        const Side& sd = m->side[s];
        const glm::vec3 S1 = rigpose::apply(C, sd.S);
        const float sideSign = s == 0 ? -1.f : 1.f;
        rigpose::Rigid want;
        bool haveWant = true;
        Goal g = t.goal[s];
        if(s == 1 && t.second)
        {
            g = Goal::None; // (worked out after the right hand's weapon, below)
        }
        if(g == Goal::Frame)
        {
            want = compose(t.weapon[s], inverse(t.offset[s]));
        }
        else if(g == Goal::Point)
        {
            want.pos = t.weapon[s].pos;
            const glm::vec3 fw = norm(want.pos - S1, C.rot * glm::vec3{1.f, 0.f, 0.f});
            want.rot = frameRot(fw, C.rot * glm::vec3{0.f, 0.f, 1.f});
        }
        else
        {
            haveWant = false;
        }
        if(haveWant)
        {
            Hm[s] = compose(want, inverse(sd.grip));
        }
        else
        {
            // Hanging free: its wrist low by its side, the fist forward.
            const glm::vec3 wrist = S1 + C.rot * glm::normalize(glm::vec3{hangDir.x, hangDir.y * sideSign, hangDir.z}) *
                                             (0.93f * (sd.Lu + sd.Lf));
            const glm::quat hr = frameRot(C.rot * glm::vec3{0.6f, 0.f, -0.8f}, C.rot * glm::vec3{0.f, -sideSign * 0.3f, 1.f});
            Hm[s].rot = glm::normalize(hr * glm::inverse(sd.grip.rot));
            Hm[s].pos = wrist - Hm[s].rot * sd.W;
        }
    }
    // The right arm, then its weapon where its hand is.
    const auto prevOf = [&](int s) { return st->haveElbow[s] ? &st->elbow[s] : nullptr; };
    arms[0] = solveArm(m->side[0], 0, rigpose::apply(C, m->side[0].S), C.rot, Hm[0], prevOf(0), t.pole[0]);
    out = Placed{};
    if(t.goal[0] == Goal::Frame)
    {
        out.on[0] = true;
        out.weapon[0] = compose(compose(arms[0].H, m->side[0].grip), t.offset[0]);
    }
    // The off hand on the right weapon's second grip (within reach, its wrist turning within reason), or under and round
    // the right hand (a Cup), else as its own goal says.
    if(t.cup && !t.second)
    {
        Hm[1] = rigpose::Rigid{};
        const glm::vec3 at = rigpose::apply(arms[0].H, m->side[0].grip.pos) + C.rot * glm::vec3{0.f, 1.2f, -2.5f};
        rigpose::Rigid want;
        want.pos = at;
        want.rot = frameRot(C.rot * glm::vec3{1.f, 0.f, 0.f}, C.rot * glm::vec3{0.f, 0.f, 1.f});
        Hm[1] = compose(want, inverse(m->side[1].grip));
    }
    if(t.second && out.on[0])
    {
        const rigpose::Rigid want = compose(out.weapon[0], t.secondAt);
        const rigpose::Rigid hl = compose(want, inverse(m->side[1].grip));
        const glm::vec3 S1 = rigpose::apply(C, m->side[1].S);
        const float reach = glm::length(rigpose::apply(hl, m->side[1].W) - S1);
        const float armLen = m->side[1].Lu + m->side[1].Lf;
        const ArmOut tryL = solveArm(m->side[1], 1, S1, C.rot, hl, prevOf(1), t.pole[1]);
        held2 = reach <= vr_agarms_second.value * armLen && tryL.bend <= vr_agarms_wrist.value;
        if(held2)
        {
            Hm[1] = hl;
            arms[1] = tryL;
            second = tryL.miss;
        }
    }
    if(!held2)
    {
        arms[1] = solveArm(m->side[1], 1, rigpose::apply(C, m->side[1].S), C.rot, Hm[1], prevOf(1), t.pole[1]);
    }
    out.held2 = held2;
    // The off hand's goal changing (a grip let go, taken): eased a moment.
    {
        const int mode = held2 ? 3 : t.cup ? 4 : static_cast<int>(t.goal[1]);
        if(mode != st->leftMode)
        {
            st->leftMode = mode;
            st->leftModeAt = cl.time;
        }
        const rigpose::Rigid now = arms[1].H;
        if(st->haveLeft && dt > 0.f && cl.time - st->leftModeAt < 0.3)
        {
            const float k = 1.f - za::exp(-dt / 0.06f);
            rigpose::Rigid eased;
            eased.rot = glm::slerp(st->left.rot, now.rot, k);
            eased.pos = glm::mix(st->left.pos, now.pos, k);
            if(glm::length(eased.pos - now.pos) > 0.3f || angleDeg(glm::inverse(eased.rot) * now.rot) > 4.f)
            {
                arms[1] = solveArm(m->side[1], 1, rigpose::apply(C, m->side[1].S), C.rot, eased, prevOf(1), t.pole[1]);
            }
        }
        st->left = arms[1].H;
        st->haveLeft = true;
    }
    if(t.goal[1] == Goal::Frame && !t.second && !t.cup)
    {
        out.on[1] = true;
        out.weapon[1] = compose(compose(arms[1].H, m->side[1].grip), t.offset[1]);
    }
    for(int s = 0; s < 2; s++)
    {
        st->bones[3 * s] = arms[s].U;
        st->bones[3 * s + 1] = arms[s].F;
        st->bones[3 * s + 2] = arms[s].H;
        st->elbow[s] = glm::inverse(C.rot) * (arms[s].elbow - rigpose::apply(C, m->side[s].S));
        st->haveElbow[s] = true;
    }
    if(vr_agrunt_ik_tag.value >= 0.f)
    {
        // (the frames as drawn, near enough: the lerp's poses)
        const int frame = (e->frame >= 0 && e->frame < hdr->numframes) ? e->frame : 0;
        const int pose1 = e->previouspose >= 0 && e->previouspose < hdr->numposes ? e->previouspose : hdr->frames[frame].firstpose;
        const int pose2 = e->currentpose >= 0 && e->currentpose < hdr->numposes ? e->currentpose : pose1;
        const float blend = za::clamp(static_cast<float>(cl.time - e->lerpstart) / za::max(e->lerptime, 1e-3f), 0.f, 1.f);
        noteErr(*m, *st, hdr, pose1, pose2, blend, arms, C, second, t.second, held2);
    }
    return true;
}

bool active(const entity_t* e)
{
    const int num = numberOf(e);
    Ent* st = num > 0 ? entOf(num, false) : nullptr;
    if(!st || st->model != e->model)
    {
        return false;
    }
    if(st->easeFrame != host_framecount)
    {
        const float dt = static_cast<float>(za::clamp(cl.time - st->time, 0.0, 0.25));
        st->time = cl.time;
        st->easeFrame = host_framecount;
        if(st->frame != host_framecount)
        {
            st->on = za::max(0.f, st->on - dt / onOut);
        }
    }
    return st->on > 0.f;
}

bool rows(const entity_t* e, const aliashdr_t* hdr, glm::vec4* out)
{
    for(int i = 0; i < blockRows; i++)
    {
        out[i] = glm::vec4{0.f};
    }
    out[1] = glm::vec4{1.f / hdr->scale[0], 1.f / hdr->scale[1], 1.f / hdr->scale[2], 0.f};
    out[0] = glm::vec4{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2], 0.f};
    const Skin* m = nullptr;
    for(const Skin& s : state.skins)
    {
        m = (s.model == e->model && s.ok) ? &s : m;
    }
    if(!m)
    {
        return false;
    }
    const int num = numberOf(e);
    const Ent* st = (num > 0 && active(e)) ? entOf(num, false) : nullptr;
    if(st)
    {
        out[0].w = st->on * st->on * (3.f - 2.f * st->on);
        for(int k = 0; k < numBones; k++)
        {
            const rigpose::Rigid& b = st->bones[k];
            out[2 + 2 * k] = glm::vec4{b.rot.x, b.rot.y, b.rot.z, b.rot.w};
            out[3 + 2 * k] = glm::vec4{b.pos, 0.f};
        }
    }
    for(int s = 0; s < 2; s++)
    {
        out[14 + 2 * s] = glm::vec4{m->side[s].E, 0.f};
        out[15 + 2 * s] = glm::vec4{m->side[s].W, 0.f};
    }
    return true;
}

bool ragdollGrip(const qmodel_t* model, int side, rigpose::Rigid& grip, int& bone)
{
    for(const Skin& s : state.skins)
    {
        if(s.model == model && s.ok && side >= 0 && side < 2)
        {
            grip = s.side[side].grip;
            bone = s.side[side].handBone;
            return true;
        }
    }
    return false;
}

void clearErr()
{
    for(Err& e : state.errs)
    {
        e = Err{};
    }
}

void printErr()
{
    Err t;
    int tags = 0;
    for(int i = 0; i < errTags; i++)
    {
        const Err& e = state.errs[i];
        if(e.n == 0)
        {
            continue;
        }
        tags++;
        t.n += e.n;
        t.gap = za::max(t.gap, e.gap);
        t.handRms += e.handRms;
        t.handMost = za::max(t.handMost, e.handMost);
        t.armLong = za::max(t.armLong, e.armLong);
        t.armShort = za::min(t.armShort, e.armShort);
        t.torso = za::max(t.torso, e.torso);
        t.miss[0] = za::max(t.miss[0], e.miss[0]);
        t.miss[1] = za::max(t.miss[1], e.miss[1]);
        t.second = za::max(t.second, e.second);
        t.bend = za::max(t.bend, e.bend);
        t.want2 += e.want2;
        t.held2 += e.held2;
        if(vr_agrunt_ik_debug.value >= 1.f || e.miss[0] > 0.5f || e.miss[1] > 0.5f || e.torso > 1.f)
        {
            Con_Printf("agarms tag %d: %d frames, gap %.3f, hand stretch %.4f, arm edges %.2f..%.2f, in torso %.2f, reach "
                       "short %.2f / %.2f, second %.2f, wrist bend %.0f, second grip held %d of %d\n",
                i, e.n, e.gap, e.handMost, e.armShort, e.armLong, e.torso, e.miss[0], e.miss[1], e.second, e.bend, e.held2,
                e.want2);
        }
    }
    Con_Printf("agarms: %d tags, %d frames: joint gap %.3f units (most), hand shape %.4f off (most edge, ratio), arm edges "
               "%.2f..%.2f of rest, in the torso %.2f units, reach short right %.2f / left %.2f units, off hand on its "
               "second grip %.2f units, wrist bend %.0f deg (most), second grip held %d of %d frames\n",
        tags, t.n, t.gap, t.handMost, t.armShort, t.armLong, t.torso, t.miss[0], t.miss[1], t.second, t.bend, t.held2, t.want2);
}

} // namespace qvr::agarms

namespace
{

// vr_agarms_info: each armed model's skin (its rest joints in its body's space: QC's rows read them).
void info_f()
{
    for(const Skin& m : state.skins)
    {
        if(!m.model)
        {
            continue;
        }
        if(!m.ok)
        {
            Con_Printf("agarms %s: no skin\n", m.model->name);
            continue;
        }
        const glm::vec3 an = m.anchor >= 0 ? m.rest[static_cast<za::SizeT>(m.anchor)] : glm::vec3{0.f};
        Con_Printf("agarms %s: torso %.1f, anchor vertex %d at %.1f %.1f %.1f\n", m.model->name, m.torso, m.anchor, an.x, an.y, an.z);
        for(int s = 0; s < 2; s++)
        {
            const Side& sd = m.side[s];
            Con_Printf("  %s: shoulder %.1f %.1f %.1f, elbow %.1f %.1f %.1f, wrist %.1f %.1f %.1f, grip %.1f %.1f %.1f; upper "
                       "%.1f fore %.1f; %d vertices, hand %d (bone %d), %d edges\n",
                s == 0 ? "right" : "left", sd.S.x, sd.S.y, sd.S.z, sd.E.x, sd.E.y, sd.E.z, sd.W.x, sd.W.y, sd.W.z, sd.grip.pos.x,
                sd.grip.pos.y, sd.grip.pos.z, sd.Lu, sd.Lf, static_cast<int>(sd.verts.size()), static_cast<int>(sd.hand.size()),
                sd.handBone, static_cast<int>(sd.edges.size() / 2));
        }
    }
}

} // namespace

void qvr::agarms::registerCommands()
{
    Cmd_AddCommand("vr_agarms_info", info_f);
}
