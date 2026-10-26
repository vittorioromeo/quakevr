// vr_agarms.cpp -- armed humanoids' arms posed and skinned (vr_agarms.hpp; ROUND22.md, "Armed humanoids' arms: posed
// and skinned").

#include <type_traits>
#include <string.h>
#include "vr_agarms.hpp"
#include "vr_agbody.hpp"
#include "vr_agglitch.hpp"
#include "vr_poselive.hpp"
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
constexpr float wristBand = 0.16f;   // the wrist's: this share of the forearm's, mostly on the forearm's side
constexpr float shoulderIn = 0.12f;  // the shoulder's blend into the body: from this share of the upper arm inside it
constexpr float shoulderOut = 0.6f;  // ... to this share along it (a shoulder pad, an enforcer's, turns partly with the chest)
constexpr float jointIn = 0.3f;      // vr_agarms_shoulder_skin: the blend from this share of the upper arm above the joint
constexpr float jointOut = 0.3f;     // ... to this share below it
constexpr float clavicleAt = 0.2f;   // vr_agarms_clavicle: its pivot this share of the way from between the shoulders to its own
constexpr float clavicleFrom = 60.f; // ... lifted with the arm above this (degrees from hanging)
constexpr float clavicleFwd = 30.f;  // ... brought forward with a reach forward of the side by more than this (degrees)
constexpr glm::vec3 coneAxis{0.5f, 0.7f, -0.3f}; // vr_agarms_limits: the shoulder's range's middle (the left's; chest space)
constexpr float limitWeight = 30.f;  // ... an elbow's cost of each radian past them squared (10 degrees: 0.9, a wrist's 55)
constexpr float armClearShare = 0.4f; // vr_agarms_arm_clear: the upper arm kept this share of its skin's radius out of the torso
constexpr float armClearSlack = 0.5f; // ... still in by more than this (units) after its elbow's choice: its wrist turned out,
constexpr float armClearMost = 20.f;  // ... at most this much (degrees)
constexpr float handReach = 1.5f;    // the hand's segment (wrist to the forearm bone's end) taken this far on (its fingers)
constexpr float armRadius = 0.7f;    // an arm's vertex: within this share of its upper arm's length of its bones' lines
constexpr float reachMost = 0.97f;   // a wrist within this share of the arm's length of its shoulder (straighter: snapping)
constexpr float elbowMost = 155.f;   // degrees: an elbow's most bend
constexpr float onIn = 0.12f;        // seconds: posed arms easing in from the animation's
constexpr float onOut = 0.2f;        // ... and back out (a death, a stale pose)
constexpr int numSamples = 36;       // the elbow's places tried round its circle
constexpr glm::vec3 hangDir{0.3f, 0.15f, -1.f};    // a free arm's wrist from its shoulder (chest space; the left's, y mirrored)
constexpr float hangOut = 0.35f;                    // ... its y with vr_agarms_arm_clear (the rest pose's upper arms' 18 degrees out)
constexpr glm::vec3 elbowPref{-0.3f, 0.45f, -1.f}; // an elbow's way from its shoulder: down, out, a little back (the left's)
constexpr float bodyClear = 0.5f;   // vr_agarms_body: the arms kept this clear of the body's proxies (units),
constexpr float bodyWeight = 0.3f;  // ... the elbow's cost of each unit in squared (a unit in: as much as 30 degrees of wrist)
constexpr float handClear = 1.0f;   // ... a hand (its wrist and grip) pushed out to this clear of them
constexpr float secondSlack = 0.05f; // vr_agarms_second_hold: a second grip held is let go only this share of the arm further,
constexpr float secondBendSlack = 15.f; // ... or its wrist bent this much (degrees) past where it is taken
constexpr float jumpEase = 2.f;        // vr_agarms_jump_ease: a goal's wrist jumping more than this (its second difference on the chest, units at 90 Hz)
constexpr float jumpEaseRate = 100.f;  // ... eased into over its size over this (seconds: units / this),
constexpr float jumpEaseLeast = 0.06f; // ... at least (a kick stays crisp)
constexpr float jumpEaseMost = 0.25f;  // ... at most
constexpr float secondDwell = 0.5f; // ... nor taken or let go again within this (seconds)
constexpr float secondEase = 0.25f; // ... taken and let go over this (seconds; the hand round the shoulder, not through the body)
constexpr float reachFar = 1.6f;    // vr_agarms_reach_ease: a weapon lying this many arm's lengths off: the hand hanging,
constexpr float reachNear = 1.15f;  // ... this near: reaching for it wholly

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
    glm::vec3 K{0.f};                           // its clavicle's pivot (rest; vr_agarms_clavicle)
    float armR{0.f};                            // its upper arm's skin's radius about its bone (units)
    za::Vector<int> upper;                      // its upper arm's skin below the joint (the torso's measure, vr_agglitch)
    za::Vector<int> shEdges;                    // ... the edges about its shoulder (vertex pairs: its stretch measured)
};

struct Skin
{
    const qmodel_t* model{nullptr};
    bool ok{false};
    Side side[2];
    za::Vector<glm::vec4> row;   // each vertex's rest row (vr_agarms.hpp restRow, w set by it)
    za::Vector<glm::vec3> rest;  // rest places
    // vr_agarms_dump: why each vertex is where it is: its nearest bone, its place along the nearest arm's upper arm (its
    // shares of it; -9 none), how far out from that shoulder (units, + away from the body), its motion off the arm's over
    // its motion off the chest's (under 0.7: the arm's).
    za::Vector<glm::vec4> why;
    za::Vector<float> clav;      // each vertex's share on its side's clavicle (vr_agarms_clavicle: restRow's w's fraction)
    int chest{-1};
    glm::vec3 c0{0.f}, c1{0.f};  // the chest bone's line at rest
    float torso{0.f};            // the torso's radius about it
    int anchor{-1};              // the chest vertex nearest between the shoulders (QC's anchor: vr_agarms_info)
    float torsoSides{1.f};       // Spec's: its torso proxy's sides from the lower chest (agbody::of)
    float twistShare{0.5f};      // Spec's: the forearm's share of the hand's twist
    float wristMore{0.f};        // Spec's: its wrist's extra bend
};

struct Err
{
    int n{0};
    float gap{0.f};           // the joints' gaps (units; 0 by construction)
    float handRms{0.f};       // the hands' edges against rest (sum of squared ratio - 1, for the rms), most
    float handMost{0.f};
    float armLong{1.f};       // any arm edge's most stretch, most squash (ratios)
    float armShort{1.f};
    float grow{0.f};          // ... the most an edge grew (units: a flap)
    float torso{0.f};         // the elbows and forearms inside the torso (units)
    float miss[2]{0.f, 0.f};  // the hands' grips off their goals (reach)
    float second{0.f};        // the off hand off its second grip
    float slid{0.f};          // ... slid back along the weapon from its hotspot (out of its reach)
    float bend{0.f};          // the wrists' bend (degrees; the right, the left: bendL)
    float bendL{0.f};
    int held2{0};             // frames the off hand held the second grip (of those wanting it)
    int want2{0};
    int worst[2]{-1, -1}; // the most stretched arm edge's vertices
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
    int rigBone[agarms::numBones]{-1, -1, -1, -1, -1, -1}; // ... the rig's bone each is (drawnArmBone)
    // A ragdoll made from these arms (drawnArmBone): each hand's turn on its forearm then (a rig with no hand bone: the
    // ragdoll's hand is its forearm's, ragdollWrist), until it is posed again.
    bool ragArms{false};
    rigpose::Rigid ragWrist[2];
    rigpose::Rigid chest; // the chest as drawn (rest -> now): the shoulders' blend
    rigpose::Rigid clav[2]; // each clavicle as drawn (rest -> now; vr_agarms_clavicle: the chest's when none)
    glm::vec3 elbow[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // the last elbows (chest space, from the shoulder)
    bool haveElbow[2]{false, false};
    rigpose::Rigid left;  // the off hand's grip frame wanted, eased (its goal changing)
    bool haveLeft{false};
    bool held2{false};    // vr_agarms_second_hold: the second grip held (its hysteresis),
    float grip2w{0.f};    // ... its share eased in and out
    double held2At{0.0};  // ... when it last changed
    rigpose::Rigid grip2Rel; // ... the off hand on it last (chest space): let go by QC, eased out from there
    bool haveGrip2{false};
    // vr_agarms_jump_ease: each hand's goal's wrist on the chest (the last two), its drawn hand on the chest, its ease
    rigpose::Rigid drawnRel[2];
    bool haveDrawn[2]{false, false};
    glm::vec3 relW[2][2]{{glm::vec3{0.f}, glm::vec3{0.f}}, {glm::vec3{0.f}, glm::vec3{0.f}}};
    int relN[2]{0, 0};
    rigpose::Rigid easeFrom[2];
    float easeT[2]{0.f, 0.f}, easeDur[2]{0.f, 0.f};
    int leftMode{-1};     // its goal's kind (Goal, 3 the second grip, 4 a cup) and since when
    double leftModeAt{0.0};
};

struct State
{
    Skin skins[maxModels];
    Ent ents[maxEnts];
    Err errs[errTags];
    Err total;
    double costUs{0.0}; // vr_agarms_cost: the solves' time since it last printed, their count
    int costN{0};
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

// Between hand motions `a` and `b` (rest -> now) by `w`: their wrists (the rest wrist `W` under each) round the shoulder
// `S` (their ways from it turned, their distances blended: a path about the body, not straight through it), the hands'
// turns slerped.
[[nodiscard]] rigpose::Rigid blendAbout(const glm::vec3& S, const glm::vec3& W, const rigpose::Rigid& a, const rigpose::Rigid& b, float w)
{
    if(w <= 0.f)
    {
        return a;
    }
    if(w >= 1.f)
    {
        return b;
    }
    const glm::vec3 wa = rigpose::apply(a, W) - S, wb = rigpose::apply(b, W) - S;
    const float la = glm::length(wa), lb = glm::length(wb);
    const glm::vec3 da = norm(wa, glm::vec3{1.f, 0.f, 0.f}), db = norm(wb, da);
    const float th = za::acos(za::clamp(glm::dot(da, db), -1.f, 1.f));
    const glm::vec3 d = th < 1e-3f ? norm(da + (db - da) * w, da)
                                   : (za::sin((1.f - w) * th) * da + za::sin(w * th) * db) / za::sin(th);
    rigpose::Rigid r;
    r.rot = glm::normalize(glm::slerp(a.rot, b.rot, w));
    r.pos = S + d * (la + (lb - la) * w) - r.rot * W;
    return r;
}

// vr_agarms_shoulder_skin: a vertex nearest an upper arm's bone that its frames move with the chest (the frames never
// raise the arms) is the arm's all the same if it is the top of the upper arm: from a little above its shoulder's joint to
// its middle (`along`: its share of the upper arm's length), not inside the joint towards the body (`out`: units out from
// the joint, away from the body; the chest's front and the flank under the armpit are further in).
[[nodiscard]] bool shoulderOwns(float along, float out, float Lu)
{
    return along >= -0.45f && along <= 0.45f && out >= -0.1f * Lu;
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

// A row's body share and frame share (its y: 2 * round(frame share * 1000) + body share).
[[nodiscard]] float bodyShare(const glm::vec4& row)
{
    return row.y - 2.f * za::floor(row.y * 0.5f);
}
[[nodiscard]] float frameShare(const glm::vec4& row)
{
    return row.x < 0.5f ? 1.f : za::floor(row.y * 0.5f) * 0.001f;
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
    const bool shoulderSkin = vr_agarms_shoulder_skin.value != 0.f;
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
    m.torsoSides = spec.torsoSides;
    m.twistShare = spec.twistShare;
    m.wristMore = spec.wristMore;
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
    // How far vertex v strays from bone b's rigid motion over the frames (root mean square, units).
    za::Vector<rigpose::Rigid> fromRest(static_cast<za::SizeT>(rig.numPoses * rig.numBones));
    for(int p = 0; p < rig.numPoses; p++)
    {
        for(int b = 0; b < rig.numBones; b++)
        {
            fromRest[static_cast<za::SizeT>(p * rig.numBones + b)] = compose(rigpose::key(rig, p, b), inverse(rigpose::key(rig, 0, b)));
        }
    }
    const auto motionMiss = [&](int v, int b) {
        float sum = 0.f;
        for(int p = 0; p < rig.numPoses; p++)
        {
            const glm::vec3 d = rigpose::apply(fromRest[static_cast<za::SizeT>(p * rig.numBones + b)], m.rest[static_cast<za::SizeT>(v)]) -
                                poseVertex(hdr, p, p, 0.f, v);
            sum += glm::dot(d, d);
        }
        return za::sqrt(sum / static_cast<float>(za::max(rig.numPoses, 1)));
    };
    // Each vertex: the nearest of the rig's bones' lines (rest); an arm's if its upper arm's or forearm's (its hand's line
    // taken on past the forearm bone's end, its fingers) within reach of it and moving with it.
    za::Vector<int8_t> sideOf(static_cast<za::SizeT>(nv), static_cast<int8_t>(-1));
    m.why.resize(static_cast<za::SizeT>(nv), glm::vec4{-1.f, -9.f, 0.f, 0.f});
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
        for(int s = 0; s < 2; s++)
        {
            // Nearest to that arm's lines and close to them, and moving with that arm through the frames clearly more than
            // with the chest (the rest pose's arms touch the body: the chest's front by a hanging upper arm, the flank by
            // a bent elbow lie as near them, but stay with the chest).
            const bool nearest = bestBone == spec.upper[s] || bestBone == spec.fore[s];
            glm::vec4& why = m.why[static_cast<za::SizeT>(v)];
            why.x = static_cast<float>(bestBone);
            if(!nearest || best > 1.4f * armRadius * m.side[s].Lu)
            {
                continue;
            }
            const Side& sd = m.side[s];
            const float along = glm::dot(p - sd.S, sd.E - sd.S) / (sd.Lu * sd.Lu);
            const float out = (p.y - sd.S.y) * (s == 0 ? -1.f : 1.f);
            const float onArm = za::min(motionMiss(v, spec.upper[s]), motionMiss(v, spec.fore[s]));
            const float onChest = motionMiss(v, spec.chest);
            why.y = along;
            why.z = out;
            why.w = onArm / za::max(onChest, 1e-4f);
            if(onArm < 0.7f * onChest)
            {
                sideOf[static_cast<za::SizeT>(v)] = static_cast<int8_t>(s);
            }
            else if(shoulderSkin && bestBone == spec.upper[s] && shoulderOwns(along, out, sd.Lu))
            {
                // (vr_agarms_shoulder_skin: the top of the upper arm, its deltoid or pauldron, the arm's though its frames
                // move it with the chest: the frames never raise the arm, the posed arm does)
                sideOf[static_cast<za::SizeT>(v)] = static_cast<int8_t>(s);
            }
        }
    }
    // An arm's vertex none of whose neighbours is its arm's (a stray), or a body's all of whose are: the other's.
    {
        const auto* desc0 = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
        const auto* idx0 = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
        za::Vector<int> same(static_cast<za::SizeT>(nv), 0), other(static_cast<za::SizeT>(nv), 0);
        za::Vector<int8_t> otherSide(static_cast<za::SizeT>(nv), static_cast<int8_t>(-1));
        for(int i = 0; i + 2 < hdr->numindexes; i += 3)
        {
            int c[3];
            for(int k = 0; k < 3; k++)
            {
                c[k] = za::clamp(static_cast<int>(desc0[idx0[i + k]].vertindex), 0, nv - 1);
            }
            for(int k = 0; k < 3; k++)
            {
                for(int j = 1; j < 3; j++)
                {
                    const int a = c[k], b = c[(k + j) % 3];
                    if(sideOf[static_cast<za::SizeT>(a)] == sideOf[static_cast<za::SizeT>(b)])
                    {
                        same[static_cast<za::SizeT>(a)]++;
                    }
                    else
                    {
                        other[static_cast<za::SizeT>(a)]++;
                        otherSide[static_cast<za::SizeT>(a)] = sideOf[static_cast<za::SizeT>(b)];
                    }
                }
            }
        }
        for(int v = 0; v < nv; v++)
        {
            if(same[static_cast<za::SizeT>(v)] == 0 && other[static_cast<za::SizeT>(v)] > 0 && !(spec.isGun && spec.isGun(spec.row, v)))
            {
                sideOf[static_cast<za::SizeT>(v)] = otherSide[static_cast<za::SizeT>(v)];
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
        // The elbow: by how near the vertex is to each bone's line (the inside of the rest pose's bent elbow, a web from
        // the forearm to the upper arm, halfway: it opens out evenly as the arm straightens).
        const float bW = wristBand * sd.Lf;
        const float u4 = za::max(dU, 0.2f) * za::max(dU, 0.2f), f4 = za::max(dF, 0.2f) * za::max(dF, 0.2f);
        const float wF = (u4 * u4) / (u4 * u4 + f4 * f4);
        const float wH = smoothstep(sd.Lu + sd.Lf - 1.4f * bW, sd.Lu + sd.Lf + 0.4f * bW, along);
        // (vr_agarms_shoulder_skin: none of the forearm's turn on the upper arm's top half: a vertex with any is drawn by
        // the elbow's blend, its share of the body moved straight towards the chest, and an arm raised folded the
        // shoulder's skin flat; the shoulder's blend turns it about the joint)
        const float c = wH > 0.f ? 1.f + wH : wF * (shoulderSkin ? smoothstep(0.45f * sd.Lu, 0.75f * sd.Lu, along) : 1.f);
        row.x = 1.f + static_cast<float>(s) * 4.f + za::min(c, 2.f);
        // (vr_agarms_shoulder_skin: blended into the body about the joint itself, not over the upper arm's top half)
        row.y = shoulderSkin ? 1.f - smoothstep(-jointIn * sd.Lu, jointOut * sd.Lu, along)
                             : 1.f - smoothstep(-shoulderIn * sd.Lu, shoulderOut * sd.Lu, along);
        sd.verts.pushBack(v);
        if(wH >= 1.f)
        {
            sd.hand.pushBack(v);
            handMid[s] += p;
            handCount[s][za::clamp(static_cast<int>(rig.vertBone[static_cast<za::SizeT>(v)]), 0, ragdoll::maxBones - 1)]++;
        }
    }
    // The shoulder's seam with the body smoothed: an upper arm's vertex takes a share of the body from its neighbours
    // (three rounds), so the skin between the arm and the chest stretches over a few edges, not one.
    {
        const auto* desc1 = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
        const auto* idx1 = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
        za::Vector<float> sum(static_cast<za::SizeT>(nv)), next(static_cast<za::SizeT>(nv));
        za::Vector<int> count(static_cast<za::SizeT>(nv));
        for(int round = 0; round < 3; round++)
        {
            for(int v = 0; v < nv; v++)
            {
                sum[static_cast<za::SizeT>(v)] = 0.f;
                count[static_cast<za::SizeT>(v)] = 0;
            }
            for(int i = 0; i + 2 < hdr->numindexes; i += 3)
            {
                int c[3];
                for(int k = 0; k < 3; k++)
                {
                    c[k] = za::clamp(static_cast<int>(desc1[idx1[i + k]].vertindex), 0, nv - 1);
                }
                for(int k = 0; k < 3; k++)
                {
                    for(int j = 1; j < 3; j++)
                    {
                        sum[static_cast<za::SizeT>(c[k])] += m.row[static_cast<za::SizeT>(c[(k + j) % 3])].y;
                        count[static_cast<za::SizeT>(c[k])]++;
                    }
                }
            }
            for(int v = 0; v < nv; v++)
            {
                const glm::vec4& row = m.row[static_cast<za::SizeT>(v)];
                const float c = row.x - 1.f - 4.f * za::floor((row.x - 1.f) * 0.25f);
                const bool shoulder = row.x > 0.5f && c < 0.5f;
                next[static_cast<za::SizeT>(v)] = shoulder && count[static_cast<za::SizeT>(v)] > 0
                                                      ? za::max(row.y, 0.5f * row.y + 0.5f * sum[static_cast<za::SizeT>(v)] /
                                                                                            static_cast<float>(count[static_cast<za::SizeT>(v)]))
                                                      : row.y;
            }
            for(int v = 0; v < nv; v++)
            {
                m.row[static_cast<za::SizeT>(v)].y = next[static_cast<za::SizeT>(v)];
            }
        }
    }
    // The body about the arms off its frames: the frames move the shoulders and the chest's sides with the animation's
    // arms (a run's swing, a pain's flail), which the posed arms no longer follow. The arms' vertices and the body's
    // within three rings of them take the chest's motion instead (all of it next to the arm, fading out), the rest the
    // frames'. Packed in the row's y with the body's share: 2 * round(frame share * 1000) + body share.
    {
        const auto* desc2 = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
        const auto* idx2 = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
        za::Vector<int8_t> ring(static_cast<za::SizeT>(nv), static_cast<int8_t>(99));
        za::Vector<int8_t> ringSide(static_cast<za::SizeT>(nv), static_cast<int8_t>(-1));
        za::Vector<float> contact(static_cast<za::SizeT>(nv), 0.f), contactCode(static_cast<za::SizeT>(nv), 0.f);
        for(int v = 0; v < nv; v++)
        {
            if(sideOf[static_cast<za::SizeT>(v)] >= 0)
            {
                ring[static_cast<za::SizeT>(v)] = 0;
                ringSide[static_cast<za::SizeT>(v)] = sideOf[static_cast<za::SizeT>(v)];
            }
        }
        for(int r = 1; r <= 3; r++)
        {
            for(int i = 0; i + 2 < hdr->numindexes; i += 3)
            {
                int c[3];
                for(int k = 0; k < 3; k++)
                {
                    c[k] = za::clamp(static_cast<int>(desc2[idx2[i + k]].vertindex), 0, nv - 1);
                }
                for(int k = 0; k < 3; k++)
                {
                    for(int j = 1; j < 3; j++)
                    {
                        const za::SizeT a = static_cast<za::SizeT>(c[k]), b = static_cast<za::SizeT>(c[(k + j) % 3]);
                        if(ring[b] == r - 1 && ring[a] > r && !(spec.isGun && spec.isGun(spec.row, c[k])))
                        {
                            ring[a] = static_cast<int8_t>(r);
                            ringSide[a] = ringSide[b];
                        }
                        // (a body's vertex the rest pose's elbow or forearm touches: half on that arm's place there,
                        // the web from the arm to the body stretched over two edges, not one)
                        const glm::vec4& rb = m.row[b];
                        const float cb = rb.x - 1.f - 4.f * za::floor((rb.x - 1.f) * 0.25f);
                        if(r == 1 && ring[a] == 1 && ring[b] == 0 && rb.x > 0.5f && cb >= 0.3f && cb > contact[a])
                        {
                            contact[a] = cb;
                            contactCode[a] = rb.x;
                        }
                    }
                }
            }
        }
        constexpr float ringFrame[4] = {0.f, 0.f, 0.35f, 0.7f};
        for(int v = 0; v < nv; v++)
        {
            glm::vec4& row = m.row[static_cast<za::SizeT>(v)];
            const int r = ring[static_cast<za::SizeT>(v)];
            if(r > 3)
            {
                continue;
            }
            if(r > 0)
            {
                row.x = 1.f + static_cast<float>(ringSide[static_cast<za::SizeT>(v)]) * 4.f; // (its side's upper arm: all body)
                row.y = 1.f;
                if(contact[static_cast<za::SizeT>(v)] > 0.f)
                {
                    row.x = contactCode[static_cast<za::SizeT>(v)];
                    row.y = 0.5f;
                }
            }
            row.y = 2.f * za::floor(ringFrame[r] * 1000.f + 0.5f) + row.y;
        }
    }
    // The clavicles (vr_agarms_clavicle): each pivots near the neck, a little way from between the shoulders towards its
    // own; the shoulder's top (the arm's vertices about the joint and the body's next to them, above the armpit) is
    // carried by it by a share (packed in restRow's w), all of it at the joint, none an upper arm's length off.
    m.clav.resize(static_cast<za::SizeT>(nv), 0.f);
    const glm::vec3 between = (m.side[0].S + m.side[1].S) * 0.5f;
    for(int s = 0; s < 2; s++)
    {
        Side& sd = m.side[s];
        sd.K = between + (sd.S - between) * clavicleAt;
        float rSum = 0.f;
        int rN = 0;
        for(const int v : sd.verts)
        {
            const glm::vec3& p = m.rest[static_cast<za::SizeT>(v)];
            float t = 0.f;
            const float d = segmentDistance(p, sd.S, sd.E, &t);
            if(t > 0.5f && t < 0.9f)
            {
                rSum += d;
                rN++;
            }
            if(t > 0.25f && bodyShare(m.row[static_cast<za::SizeT>(v)]) < 0.5f && m.row[static_cast<za::SizeT>(v)].x - 1.f - 4.f * static_cast<float>(s) < 1.f)
            {
                sd.upper.pushBack(v);
            }
        }
        sd.armR = rN > 0 ? rSum / static_cast<float>(rN) : 0.3f * sd.Lu;
    }
    if(shoulderSkin)
    {
        for(int v = 0; v < nv; v++)
        {
            const glm::vec4& row = m.row[static_cast<za::SizeT>(v)];
            if(row.x < 0.5f || row.x > 8.5f)
            {
                continue;
            }
            const int s = za::clamp(static_cast<int>(za::floor((row.x - 1.f) * 0.25f)), 0, 1);
            const float c = row.x - 1.f - 4.f * static_cast<float>(s);
            if(c >= 1.f) // (the forearm's and the hand's: on the arm)
            {
                continue;
            }
            const Side& sd = m.side[s];
            const glm::vec3& p = m.rest[static_cast<za::SizeT>(v)];
            const glm::vec3 ks = sd.S - sd.K;
            const float t = glm::dot(p - sd.K, ks) / za::max(glm::dot(ks, ks), 1e-4f);
            m.clav[static_cast<za::SizeT>(v)] = (1.f - smoothstep(0.5f * sd.Lu, 1.f * sd.Lu, glm::length(p - sd.S))) *
                                                smoothstep(-0.2f, 0.6f, t) * smoothstep(sd.S.z - 0.6f * sd.Lu, sd.S.z - 0.2f * sd.Lu, p.z);
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
            // (an arm's edges, and those from it to the body: a flap pulled off the body shows there)
            const int es = su >= 0 ? su : sw;
            if(u < w && es >= 0 && (su == es || su < 0) && (sw == es || sw < 0))
            {
                m.side[es].edges.pushBack(u);
                m.side[es].edges.pushBack(w);
            }
        }
    }
    // (the shoulder's edges: those within an upper arm's length of its joint, both ends on its arm's skin or the body
    // carried by the chest by it, where the CPU's copy is RigArm's)
    const auto sideCode = [&](int v) {
        const glm::vec4& r = m.row[static_cast<za::SizeT>(v)];
        return r.x > 0.5f && r.x < 8.5f && r.y < 1.5f ? za::clamp(static_cast<int>(za::floor((r.x - 1.f) * 0.25f)), 0, 1) : -1;
    };
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        for(int k = 0; k < 3; k++)
        {
            const int u = corner(i + k), w = corner(i + (k + 1) % 3);
            const int s = sideCode(u) == sideCode(w) ? sideCode(u) : -1;
            if(u >= w || s < 0)
            {
                continue;
            }
            // (its cap: above the armpit and out from the joint; the armpit's web stretches as an arm rises, as it should)
            const Side& sd = m.side[s];
            const auto cap = [&](int v) {
                const glm::vec3& p = m.rest[static_cast<za::SizeT>(v)];
                return glm::length(p - sd.S) < sd.Lu && p.z > sd.S.z - 0.35f * sd.Lu && (p.y - sd.S.y) * (s == 0 ? -1.f : 1.f) > -0.1f * sd.Lu;
            };
            if(cap(u) && cap(w))
            {
                m.side[s].shEdges.pushBack(u);
                m.side[s].shEdges.pushBack(w);
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
    float bend{0.f};  // the wrist's bend wanted (degrees)
    float turned{0.f}; // ... past its most: the hand turned back by this (degrees)
    glm::vec3 elbow{0.f};
    float poleOff{0.f}; // its elbow off its pole (degrees), the cost that most held it (agarms::Limit), held by the rate
    int poleWhy{0};
    bool rated{false};
    rigpose::Rigid K;   // its clavicle (rest -> now; the chest's with vr_agarms_clavicle 0)
    glm::vec3 S{0.f};   // its shoulder joint now (on the clavicle)
    float swing{0.f};   // its upper arm's angle off its shoulder's cone's axis, its twist from its rest pose's (degrees,
    float twist{0.f};   // ... either way)
    float limited{0.f}; // its wrist brought towards its shoulder's cone (units: out of reach within its limits)
    float armIn{0.f};   // vr_agarms_arm_clear: its upper arm still in the torso (units past its clearance), the way out,
    glm::vec3 armPush{0.f}; // ... how far from the shoulder that is (units)
    float armAt{0.f};
    float cleared{0.f}; // ... its wrist turned out of it (units)
    glm::vec3 W{0.f};   // its wrist as solved
};

// The robust solve's limits each frame (ROUND22.md, "Armed humanoids in motion"): the body's proxies the elbow keeps out
// of (null: none), how far the elbow may swing round its reach line since its last place (radians; 0: anywhere).
struct Limits
{
    const agbody::Posed* body{nullptr};
    float maxStep{0.f};
    float twistShare{0.5f}; // the forearm's share of the hand's twist (Skin's; vr_agarms_twist_share overrides)
    float wristMore{0.f};   // degrees its wrist bends past vr_agarms_wrist_most (Skin's)
    float clavShare{0.f};   // vr_agarms_clavicle, _most (degrees)
    float clavMost{0.f};
    bool limits{false};     // vr_agarms_limits: the shoulder's cone and twist (radians) kept
    float cone{0.f}, twistMost{0.f};
    bool armClear{false};   // vr_agarms_arm_clear (`rest` the skin's rest places)
    const glm::vec3* rest{nullptr};
};

// The shoulder's cone's axis now (model space): its range's middle on clavicle turn `kq`.
[[nodiscard]] glm::vec3 coneAxisNow(int s, const glm::quat& kq)
{
    return kq * glm::normalize(glm::vec3{coneAxis.x, coneAxis.y * (s == 0 ? -1.f : 1.f), coneAxis.z});
}

// How far upper arm turn `U` (rest -> now) is past the shoulder's cone and twist limit on clavicle turn `kq` (radians;
// with cone and twistMost 0 the raw angles).
void shoulderPast(const Side& sd, int s, const glm::quat& kq, const glm::quat& U, float cone, float twistMost, float& swing, float& twist)
{
    const glm::vec3 u0 = glm::normalize(sd.E - sd.S);
    const glm::vec3 u1 = U * u0;
    swing = za::max(0.f, za::acos(za::clamp(glm::dot(u1, coneAxisNow(s, kq)), -1.f, 1.f)) - cone);
    twist = za::max(0.f, za::abs(twistAbout(glm::inverse(kq) * U, u0)) - twistMost);
}

// One arm (rest side `sd`, its shoulder `S1` now, the chest's turn `cq`) to the hand motion `H` (rest -> now: its wrist
// goes where H takes the rest wrist), its elbow chosen round its circle; `prevElbow` the last (chest space), `pole` a
// strike's wanted pole (its length its share).
ArmOut solveArmOnce(const Side& sd, int s, const glm::vec3& S0, const glm::quat& cq, rigpose::Rigid H, const glm::vec3* prevElbow,
    const glm::vec3& pole, const Limits& lim = Limits{})
{
    ArmOut o;
    const float a = sd.Lu, b = sd.Lf;
    const float sideSign = s == 0 ? -1.f : 1.f;
    // The clavicle (vr_agarms_clavicle): turned up and forward about its pivot by a share of the arm's lift above level
    // and of its reach ahead of its side (towards the wrist wanted), at most vr_agarms_clavicle_most; the shoulder joint
    // on it (`S0` its place on the chest).
    {
        glm::quat rl{1.f, 0.f, 0.f, 0.f};
        if(lim.clavShare > 0.f)
        {
            const glm::vec3 dc = glm::inverse(cq) * norm(rigpose::apply(H, sd.W) - S0, cq * glm::vec3{1.f, 0.f, 0.f});
            const float lift = za::atan2(dc.z, za::sqrt(dc.x * dc.x + dc.y * dc.y)) * (180.f / pi) + 90.f; // (from hanging)
            const float ahead = za::atan2(dc.x, dc.y * sideSign) * (180.f / pi); // (0 out to its side, 90 ahead, 180 across)
            const float up = za::min(lim.clavMost, lim.clavShare * za::max(0.f, lift - clavicleFrom));
            const float fwd = za::min(lim.clavMost, lim.clavShare * za::max(0.f, ahead - clavicleFwd) * (1.f - za::abs(dc.z)));
            rl = glm::angleAxis(-sideSign * fwd * (pi / 180.f), glm::vec3{0.f, 0.f, 1.f}) *
                 glm::angleAxis(sideSign * up * (pi / 180.f), glm::vec3{1.f, 0.f, 0.f});
        }
        o.K.rot = glm::normalize(cq * rl);
        o.K.pos = S0 - cq * sd.S + cq * (sd.K - rl * sd.K);
    }
    const glm::vec3 S1 = rigpose::apply(o.K, sd.S);
    o.S = S1;
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
    glm::vec3 dir = dir0;
    const float cosA = za::clamp((a * a + dl * dl - b * b) / (2.f * a * dl), -1.f, 1.f);
    const float sinA = za::sqrt(za::max(0.f, 1.f - cosA * cosA));
    const glm::vec3 axisNow = coneAxisNow(s, o.K.rot);
    if(lim.limits)
    {
        // vr_agarms_limits: a wrist so far off the shoulder's range that no elbow round its circle keeps the upper arm in
        // the cone (its reach line further off the cone's axis than the cone and the upper arm's angle off the reach line)
        // is brought round towards it at its distance: the hand eases off its goal (its weapon with it), the arm whole.
        const float off = za::acos(za::clamp(glm::dot(dir, axisNow), -1.f, 1.f));
        const float most2 = lim.cone + za::acos(cosA) - 0.02f;
        const glm::vec3 across = glm::cross(dir, axisNow);
        if(off > most2 && glm::length(across) > 1e-4f)
        {
            dir = glm::angleAxis(off - most2, glm::normalize(across)) * dir;
            const glm::vec3 shift = S1 + dir * dl - W1;
            o.limited = glm::length(shift);
            o.miss += o.limited;
            H.pos += shift;
            W1 += shift;
        }
    }
    const glm::vec3 C = S1 + dir * (a * cosA);
    const float rho = a * sinA;
    // The circle's basis: e1 the preferred elbow's way across the reach.
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
    // (a pole past a strike's own share, 0.8: the Pose Studio's keys, vr_agpose.hpp, set by hand: weighed up to 9 times
    // as much, its elbow where it was put as far as its wrist allows)
    // (vr_agarms_pole_key: a key's pole, its share 1, weighed that much more: it stays where the studio put it)
    const float poleWeight = 0.5f + za::max(0.f, vr_agarms_pole_key.value) * za::clamp((poleShare - 0.8f) / 0.2f, 0.f, 1.f);
    // The elbow's costs at `phi` round its circle: its wrist's comfort, its pole, its shoulder's (up past it, in across the
    // chest), its last place's, the body's proxies.
    struct Terms
    {
        float wrist, pole, shoulder, prev, body, limit;
    };
    // (vr_agarms_limits: the rest's upper arm and hinge on the clavicle now: an elbow's twist is its hinge's angle about
    // the upper arm from the rest's carried along the shortest arc onto it, as shoulderPast's swing-twist, cheaper)
    const glm::vec3 uK = o.K.rot * glm::normalize(sd.E - sd.S), hK = o.K.rot * sd.hinge;
    const auto terms = [&](float phi) {
        Terms t{};
        const glm::vec3 E = C + rho * (za::cos(phi) * e1 + za::sin(phi) * e2);
        if(lim.limits)
        {
            // (the upper arm past its shoulder's cone or twisted past its limit)
            const glm::vec3 u1 = norm(E - S1, dir), f1 = norm(W1 - E, dir);
            const glm::vec3 h1 = norm(glm::cross(u1, f1), norm(glm::cross(u1, e1), sd.hinge));
            const float sw = za::max(0.f, za::acos(za::clamp(glm::dot(u1, axisNow), -1.f, 1.f)) - lim.cone);
            const glm::vec3 cr = glm::cross(uK, u1);
            const float cd = glm::dot(uK, u1);
            float tw = pi;
            if(cd > -0.999f)
            {
                const glm::vec3 hs = hK + glm::cross(cr, hK) + glm::cross(cr, glm::cross(cr, hK)) / (1.f + cd);
                tw = za::abs(za::atan2(glm::dot(glm::cross(hs, h1), u1), glm::dot(hs, h1)));
            }
            tw = za::max(0.f, tw - lim.twistMost);
            t.limit = limitWeight * (sw * sw + tw * tw);
        }
        const glm::vec3 f = norm(W1 - E, fWant);
        const float wrist = za::acos(za::clamp(glm::dot(f, fWant), -1.f, 1.f));
        t.wrist = wrist * wrist;
        const glm::vec3 eDir = norm(E - S1, pref);
        t.pole = poleWeight * (1.f - glm::dot(eDir, pref));
        // An elbow up past its shoulder, or in across the chest.
        const float up = glm::dot(E - S1, chestUp);
        t.shoulder = up > 0.f ? 0.02f * up * up : 0.f;
        const float in = glm::dot(E - S1, inward);
        t.shoulder += in > 0.f ? 0.03f * in * in : 0.f;
        if(prevElbow)
        {
            const glm::vec3 pe = S1 + cq * *prevElbow;
            const glm::vec3 dd = E - pe;
            t.prev = 0.01f * glm::dot(dd, dd);
        }
        if(lim.body)
        {
            // (vr_agarms_body: the elbow, the forearm's middle and the upper arm's lower half out of the torso, the
            // pelvis and the head, a little clear of them)
            float dep = agbody::depth(*lim.body, E, bodyClear);
            dep = za::max(dep, agbody::depth(*lim.body, (E + W1) * 0.5f, bodyClear));
            dep = za::max(dep, agbody::depth(*lim.body, S1 + (E - S1) * 0.75f, bodyClear));
            if(lim.armClear)
            {
                // (vr_agarms_arm_clear: the upper arm's skin, not only its bone's line: its middle and lower part kept
                // most of its radius out of the torso)
                dep = za::max(dep, agbody::depth(*lim.body, S1 + (E - S1) * 0.45f, armClearShare * sd.armR));
                dep = za::max(dep, agbody::depth(*lim.body, S1 + (E - S1) * 0.8f, armClearShare * sd.armR));
            }
            t.body = dep > 0.f ? bodyWeight * dep * dep : 0.f;
        }
        return t;
    };
    const auto cost = [&](float phi) {
        const Terms t = terms(phi);
        return t.wrist + t.pole + t.shoulder + t.prev + t.body + t.limit;
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
    float phi = 2.f * pi * (static_cast<float>(best) + off) / static_cast<float>(numSamples);
    if(prevElbow && lim.maxStep > 0.f && rho > 0.05f * a)
    {
        // vr_agarms_elbow_rate: from its last place round the circle at most maxStep towards the best, the way round
        // whose worst place is the better (never through the chest to get there faster): no flips.
        const glm::vec3 pe = cq * *prevElbow + S1 - C;
        const float phi0 = za::atan2(glm::dot(pe, e2), glm::dot(pe, e1));
        float delta = phi - phi0;
        delta -= 2.f * pi * za::floor((delta + pi) / (2.f * pi)); // (-pi..pi)
        float way = delta;
        if(za::abs(delta) > lim.maxStep)
        {
            const float other = delta > 0.f ? delta - 2.f * pi : delta + 2.f * pi;
            const auto barrier = [&](float span) {
                float worst = 0.f;
                for(int k = 1; k < 12; k++)
                {
                    worst = za::max(worst, cost(phi0 + span * static_cast<float>(k) / 12.f));
                }
                return worst;
            };
            way = barrier(delta) <= barrier(other) ? delta : other;
            phi = phi0 + (way > 0.f ? lim.maxStep : -lim.maxStep);
            o.rated = true;
        }
    }
    const glm::vec3 E1 = C + rho * (za::cos(phi) * e1 + za::sin(phi) * e2);
    o.elbow = E1;
    {
        // (the Pose Studio's: how far off its pole the elbow is, and the cost that most held it there)
        o.poleOff = za::acos(za::clamp(glm::dot(norm(E1 - S1, pref), pref), -1.f, 1.f)) * (180.f / pi);
        const Terms t = terms(phi);
        o.poleWhy = o.rated ? agarms::LimitRate
                    : t.limit >= t.wrist && t.limit >= t.body && t.limit >= t.shoulder && t.limit > 0.f ? agarms::LimitJoint
                    : t.body >= t.wrist && t.body >= t.shoulder && t.body > 0.f ? agarms::LimitBody
                    : t.shoulder >= t.wrist && t.shoulder > 0.f                 ? agarms::LimitShoulder
                                                                                : agarms::LimitWrist;
    }
    // The bones: the upper arm and the forearm turned onto their lines about the elbow's hinge (its rest hinge to now's),
    // the forearm twisted towards the hand by its model's twist share (Spec::twistShare: the rest at the wrist).
    const glm::vec3 u0 = glm::normalize(sd.E - sd.S), f0 = glm::normalize(sd.W - sd.E);
    const glm::vec3 u1 = norm(E1 - S1, dir), f1 = norm(W1 - E1, dir);
    const glm::vec3 h1 = norm(glm::cross(u1, f1), norm(glm::cross(u1, e1), sd.hinge));
    o.U.rot = alignRot(u0, sd.hinge, u1, h1);
    o.U.pos = S1 - o.U.rot * sd.S;
    {
        // (the glitch finder's: the upper arm's angle off the cone's axis and its twist, kept or not)
        float sw = 0.f, tw = 0.f;
        shoulderPast(sd, s, o.K.rot, o.U.rot, 0.f, 0.f, sw, tw);
        o.swing = sw * (180.f / pi);
        o.twist = tw * (180.f / pi);
    }
    glm::quat rf = alignRot(f0, sd.hinge, f1, h1);
    const float tw = twistAbout(glm::inverse(rf) * H.rot, f0);
    rf = glm::normalize(rf * glm::angleAxis(tw * lim.twistShare, f0));
    o.F.rot = rf;
    o.F.pos = E1 - rf * sd.E;
    o.H = H;
    o.bend = angleDeg(glm::inverse(rf) * H.rot);
    // A wrist past its most (a swing's twist the elbow can't take): the hand, and its weapon with it, turned back
    // towards the forearm about the wrist (the wrist stays).
    const float bendMost = za::max(vr_agarms_wrist_most.value + lim.wristMore, 10.f);
    if(o.bend > bendMost)
    {
        o.H.rot = glm::normalize(glm::slerp(rf, H.rot, bendMost / o.bend));
        o.H.pos = W1 - o.H.rot * sd.W;
        o.turned = o.bend - bendMost;
    }
    if(lim.body && lim.armClear && lim.rest)
    {
        // (vr_agarms_arm_clear: how far the chosen upper arm's skin is still in the torso, and the way out: solveArm)
        for(const int v : sd.upper)
        {
            glm::vec3 push{0.f};
            const glm::vec3 p = rigpose::apply(o.U, lim.rest[static_cast<za::SizeT>(v)]);
            const float dep = agbody::depth(*lim.body, p, 0.f, &push);
            if(dep > o.armIn)
            {
                o.armIn = dep;
                o.armPush = push;
                o.armAt = za::max(glm::dot(p - S1, u1), 0.f);
            }
        }
    }
    o.W = W1;
    return o;
}

// An arm solved (solveArmOnce); vr_agarms_arm_clear: one whose upper arm is still in the torso however its elbow turns
// (a straight arm by its side, a hand across the chest) has its wrist turned about the shoulder out of it, the hand easing
// off its goal (its weapon with it), and is solved again.
ArmOut solveArm(const Side& sd, int s, const glm::vec3& S0, const glm::quat& cq, rigpose::Rigid H, const glm::vec3* prevElbow,
    const glm::vec3& pole, const Limits& lim = Limits{})
{
    ArmOut o = solveArmOnce(sd, s, S0, cq, H, prevElbow, pole, lim);
    if(o.armIn <= armClearSlack || o.armAt < 0.5f)
    {
        return o;
    }
    const glm::vec3 d = o.W - o.S;
    const glm::vec3 axis = glm::cross(d, o.armPush);
    if(glm::length(axis) < 1e-4f)
    {
        return o;
    }
    const float turn = za::min(za::atan2(o.armIn, o.armAt), armClearMost * (pi / 180.f));
    const glm::vec3 W2 = o.S + glm::angleAxis(turn, glm::normalize(axis)) * d;
    rigpose::Rigid H2 = o.H;
    H2.rot = H.rot;
    H2.pos = H.pos + (W2 - o.W);
    ArmOut o2 = solveArmOnce(sd, s, S0, cq, H2, prevElbow, pole, lim);
    o2.cleared = glm::length(W2 - o.W);
    o2.miss += o2.cleared;
    return o2;
}

[[nodiscard]] glm::quat nlerpQ(const glm::quat& a, glm::quat b, float f)
{
    if(glm::dot(a, b) < 0.f)
    {
        b = -b;
    }
    return glm::normalize(glm::quat{a.w + (b.w - a.w) * f, a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f});
}

// The vertex `v` of side `s` as RigArm draws it (model units): the CPU's copy (the numbers).
[[nodiscard]] glm::vec3 skinned(const Skin& m, const rigpose::Rigid* bones, const rigpose::Rigid& chest0, const rigpose::Rigid* clav, int v,
    const glm::vec3& body)
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
    const float wB = bodyShare(r);
    // (the shoulder's top on its clavicle by its share, as RigArm)
    const float kc = m.clav[static_cast<za::SizeT>(v)];
    rigpose::Rigid chest = chest0;
    glm::vec3 chestAt = rigpose::apply(chest0, rest);
    if(kc > 0.001f)
    {
        chestAt = glm::mix(chestAt, rigpose::apply(clav[s], rest), kc);
        chest.rot = nlerpQ(chest0.rot, clav[s].rot, kc);
    }
    const glm::vec3 bodyAt = chestAt + frameShare(r) * (body - chestAt); // (near the arms the chest's, further its frames')
    glm::vec3 p;
    if(wB >= 0.9995f)
    {
        return bodyAt;
    }
    if(bone == 0 && f < 0.0005f && wB > 0.0005f)
    {
        // (the shoulder: RigArm's turns blended about it, the body's move off the chest by its share)
        const glm::vec3 s0 = m.side[s].S;
        const glm::quat q = nlerpQ(A.rot, chest.rot, wB);
        return q * (rest - s0) + rigpose::apply(A, s0) + wB * (bodyAt - chestAt);
    }
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
    return glm::mix(p, bodyAt, wB);
}

void noteErr(const Skin& m, const Ent& st, const aliashdr_t* hdr, int pose1, int pose2, float blend, const ArmOut* arms,
    const rigpose::Rigid& chest, float second, float slid, bool want2, bool held2)
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
        (s == 0 ? e.bend : e.bendL) = za::max(s == 0 ? e.bend : e.bendL, arms[s].bend - arms[s].turned); // (as drawn)
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
            const glm::vec3 pu = skinned(m, st.bones, chest, st.clav, u, poseVertex(hdr, pose1, pose2, blend, u));
            const glm::vec3 pw = skinned(m, st.bones, chest, st.clav, w, poseVertex(hdr, pose1, pose2, blend, w));
            const float r0 = glm::length(m.rest[static_cast<za::SizeT>(u)] - m.rest[static_cast<za::SizeT>(w)]);
            if(r0 < 0.05f)
            {
                continue;
            }
            const float ratio = glm::length(pu - pw) / r0;
            e.grow = za::max(e.grow, glm::length(pu - pw) - r0);
            const bool hand = m.row[static_cast<za::SizeT>(u)].x - 1.f - 4.f * static_cast<float>(s) >= 2.f &&
                              m.row[static_cast<za::SizeT>(w)].x - 1.f - 4.f * static_cast<float>(s) >= 2.f;
            if(hand)
            {
                e.handRms += (ratio - 1.f) * (ratio - 1.f);
                e.handMost = za::max(e.handMost, za::abs(ratio - 1.f));
            }
            // (the shoulder's blend into the animated body is the animation's: not an arm edge's stretch)
            if(bodyShare(m.row[static_cast<za::SizeT>(u)]) < 0.99f || bodyShare(m.row[static_cast<za::SizeT>(w)]) < 0.99f)
            {
                if(ratio > e.armLong)
                {
                    e.worst[0] = u;
                    e.worst[1] = w;
                }
                e.armLong = za::max(e.armLong, ratio);
                e.armShort = za::min(e.armShort, ratio);
            }
        }
    }
    e.second = za::max(e.second, second);
    e.slid = za::max(e.slid, held2 ? slid : 0.f);
    e.want2 += want2 ? 1 : 0;
    e.held2 += held2 ? 1 : 0;
}

// The glitch finder's right way for an elbow (model space): outward and down from its shoulder (its hemisphere: one
// past 135 degrees off it points up and in, the wrong way), or a strike's or a key's pole where one is sent.
[[nodiscard]] glm::vec3 preferredPole(int s, const glm::quat& cq, const glm::vec3& pole)
{
    const float sideSign = s == 0 ? -1.f : 1.f;
    const glm::vec3 pref = cq * glm::normalize(glm::vec3{0.f, sideSign, -1.f});
    const float poleShare = za::min(glm::length(pole), 1.f);
    return poleShare > 1e-3f ? norm(pref * (1.f - poleShare) + pole, pref) : pref;
}

// The glitch finder's frame (vr_agglitch.hpp): the arms as drawn, how far they are in the body.
void noteGlitch(const Skin& m, const Ent& st, const ragdoll::Rig& rig, const rigpose::Rigid* posed, const rigpose::Rigid* kick,
    const agarms::Targets& t, const agarms::Placed& out, const ArmOut* arms, const rigpose::Rigid& C, int num, float second,
    bool held2, const aliashdr_t* hdr, int pose1, int pose2, float blend)
{
    agglitch::Sample g;
    g.ent = num;
    const agbody::Proxies* px = agbody::of(m.model, rig, m.chest, m.torsoSides);
    const agbody::Posed body = px ? agbody::pose(*px, posed, kick) : agbody::Posed{};
    for(int s = 0; s < 2; s++)
    {
        const Side& sd = m.side[s];
        const rigpose::Rigid* b = st.bones + 3 * s;
        agglitch::Arm& a = g.arm[s];
        a.S = arms[s].S;
        a.E = rigpose::apply(b[1], sd.E);
        a.W = rigpose::apply(b[2], sd.W);
        a.grip = rigpose::apply(b[2], sd.grip.pos);
        a.pole = preferredPole(s, C.rot, t.pole[s]);
        a.miss = arms[s].miss;
        a.turned = arms[s].turned;
        a.pen = -1e9f;
        for(const glm::vec3& p : {a.E, (a.E + a.W) * 0.5f, a.W, a.grip})
        {
            int part = -1;
            const float d = agbody::depth(body, p, 0.f, nullptr, &part);
            if(d > a.pen)
            {
                a.pen = d;
                a.penPart = part;
            }
        }
        g.gap = za::max(g.gap, za::max(glm::length(rigpose::apply(b[0], sd.E) - rigpose::apply(b[1], sd.E)),
                                   glm::length(rigpose::apply(b[1], sd.W) - rigpose::apply(b[2], sd.W))));
        // The shoulder (ROUND22.md, "Armed humanoids' shoulders"): its upper arm's skin in the torso, its limits, its
        // skin's stretch, as drawn.
        a.swing = arms[s].swing;
        a.twist = arms[s].twist;
        for(const int v : sd.upper)
        {
            const glm::vec3 p = skinned(m, st.bones, C, st.clav, v, poseVertex(hdr, pose1, pose2, blend, v));
            int part = -1;
            const float d = agbody::depth(body, p, 0.f, nullptr, &part);
            if(part == agbody::Torso && d > 0.f)
            {
                a.armPen = za::max(a.armPen, d);
                a.armPenN += d > 0.5f ? 1 : 0;
            }
        }
        for(za::SizeT i = 0; i + 1 < sd.shEdges.size(); i += 2)
        {
            const int u = sd.shEdges[i], w = sd.shEdges[i + 1];
            const float r0 = glm::length(m.rest[static_cast<za::SizeT>(u)] - m.rest[static_cast<za::SizeT>(w)]);
            if(r0 < 0.3f)
            {
                continue;
            }
            const glm::vec3 pu = skinned(m, st.bones, C, st.clav, u, poseVertex(hdr, pose1, pose2, blend, u));
            const glm::vec3 pw = skinned(m, st.bones, C, st.clav, w, poseVertex(hdr, pose1, pose2, blend, w));
            const float ratio = za::max(glm::length(pu - pw), 1e-3f) / r0;
            a.stretch = za::max(a.stretch, za::max(ratio, 1.f / ratio) - 1.f);
        }
    }
    g.chestUp = C.rot * glm::vec3{0.f, 0.f, 1.f};
    g.want2 = t.second;
    g.held2 = held2;
    g.second = second;
    if(t.goal[0] == agarms::Goal::Frame && out.on[0] && !t.reaching) // (reaching for a weapon lying: none held)
    {
        g.aim = za::acos(za::clamp(glm::dot(t.weapon[0].rot * glm::vec3{1.f, 0.f, 0.f}, out.weapon[0].rot * glm::vec3{1.f, 0.f, 0.f}),
                    -1.f, 1.f)) *
                (180.f / pi);
    }
    if(poselive::measuring(num))
    {
        poselive::sample(g);
    }
    if(agglitch::watching(num))
    {
        agglitch::note(g);
    }
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
    // (its clavicle's share in w's fraction: RigArm's uint() of w drops it)
    out.w = static_cast<float>(3 * rig.numBones) + 0.99f * za::clamp(m->clav[static_cast<za::SizeT>(v)], 0.f, 1.f);
    return true;
}

bool solve(const entity_t* e, const aliashdr_t* hdr, const ragdoll::Rig& rig, const Spec& spec, const rigpose::Rigid* posed,
    const rigpose::Rigid* kick, const Targets& t, Placed& out)
{
    out = Placed{};
    const double solveStart = Sys_DoubleTime();
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
    st->ragArms = false;
    for(int s = 0; s < 2; s++)
    {
        st->rigBone[3 * s] = spec.upper[s];
        st->rigBone[3 * s + 1] = spec.fore[s];
        st->rigBone[3 * s + 2] = m->side[s].handBone;
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
    float slid = 0.f;
    bool held2 = false;
    // The robust solve's limits (ROUND22.md, "Armed humanoids in motion"): the body's proxies (vr_agarms_body), the
    // elbow's swing a frame (vr_agarms_elbow_rate).
    const agbody::Proxies* px = vr_agarms_body.value != 0.f ? agbody::of(e->model, rig, m->chest, m->torsoSides) : nullptr;
    const agbody::Posed bodyNow = px ? agbody::pose(*px, posed, kick) : agbody::Posed{};
    Limits lim;
    lim.body = px ? &bodyNow : nullptr;
    lim.twistShare = za::clamp(vr_agarms_twist_share.value > 0.f ? vr_agarms_twist_share.value : m->twistShare, 0.f, 1.f);
    lim.wristMore = m->wristMore;
    lim.maxStep = vr_agarms_elbow_rate.value > 0.f && dt > 0.f ? vr_agarms_elbow_rate.value * (pi / 180.f) * dt : 0.f;
    lim.clavShare = za::clamp(vr_agarms_clavicle.value, 0.f, 1.f);
    lim.clavMost = za::clamp(vr_agarms_clavicle_most.value, 0.f, 60.f);
    lim.limits = vr_agarms_limits.value != 0.f && vr_agarms_shoulder_cone.value > 0.f;
    lim.cone = za::clamp(vr_agarms_shoulder_cone.value, 10.f, 180.f) * (pi / 180.f);
    lim.twistMost = za::clamp(vr_agarms_shoulder_twist.value, 0.f, 180.f) * (pi / 180.f);
    lim.armClear = vr_agarms_arm_clear.value != 0.f;
    lim.rest = m->rest.data();
    // A hand's wrist and grip pushed out of the body (its weapon with it).
    float pushedBy[2]{0.f, 0.f}; // (the Pose Studio's: Placed::pushed)
    const auto pushOut = [&](int s, rigpose::Rigid& H) {
        float by = 0.f;
        for(int round = 0; round < 2 && lim.body; round++)
        {
            glm::vec3 pw, pg;
            const float dw = agbody::depth(bodyNow, rigpose::apply(H, m->side[s].W), handClear, &pw);
            const float dg = agbody::depth(bodyNow, rigpose::apply(H, m->side[s].grip.pos), handClear, &pg);
            if(dw <= 0.f && dg <= 0.f)
            {
                break;
            }
            H.pos += dw > dg ? pw : pg;
            by += glm::length(dw > dg ? pw : pg);
        }
        pushedBy[s] = za::max(pushedBy[s], by);
    };
    // The goals' hand motions (rest -> now).
    rigpose::Rigid Hm[2], free[2];
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
        // Hanging free: its wrist low by its side, the fist forward.
        rigpose::Rigid hang;
        {
            // (vr_agarms_arm_clear: out from the body as the rest pose's upper arm, not along the torso's side)
            const float out = lim.armClear ? hangOut : hangDir.y;
            const glm::vec3 wrist = S1 + C.rot * glm::normalize(glm::vec3{hangDir.x, out * sideSign, hangDir.z}) *
                                             (0.8f * (sd.Lu + sd.Lf)); // (its elbow bent as at rest: no web stretched)
            const glm::quat hr = frameRot(C.rot * glm::vec3{0.6f, 0.f, -0.8f}, C.rot * glm::vec3{0.f, -sideSign * 0.3f, 1.f});
            hang.rot = glm::normalize(hr * glm::inverse(sd.grip.rot));
            hang.pos = wrist - hang.rot * sd.W;
        }
        Hm[s] = haveWant ? compose(want, inverse(sd.grip)) : hang;
        if(t.fk[s])
        {
            Hm[s] = t.fkHand[s]; // (the Rig Studio's animation: the hand where its arm bones have it, vr_rigseq.cpp)
        }
        else if(s == 0 && t.reaching && vr_agarms_reach_ease.value != 0.f)
        {
            // (a weapon lying out of its reach: the hand comes from hanging towards it as it nears, not stretched at
            // it from afar)
            const float away = glm::length(rigpose::apply(Hm[s], sd.W) - S1) / (sd.Lu + sd.Lf);
            Hm[s] = blendAbout(S1, sd.W, hang, Hm[s], 1.f - smoothstep(reachNear, reachFar, away));
        }
        pushOut(s, Hm[s]);
        // vr_agarms_jump_ease: a goal that jumps (on its chest: a weapon taken up into the carry, a kick, a key's
        // change) eased into from where the hand was drawn, round the shoulder, over a time as long as the jump.
        const rigpose::Rigid rel = compose(inverse(C), Hm[s]);
        const glm::vec3 relW = rigpose::apply(rel, sd.W);
        if(vr_agarms_jump_ease.value != 0.f && st->relN[s] >= 2 && dt > 0.f && st->haveDrawn[s])
        {
            const float k = (1.f / 90.f) / dt;
            const float jump = glm::length(relW - 2.f * st->relW[s][0] + st->relW[s][1]) * k * k;
            if(jump > jumpEase)
            {
                st->easeFrom[s] = st->drawnRel[s];
                st->easeT[s] = 0.f;
                st->easeDur[s] = za::clamp(jump / jumpEaseRate, jumpEaseLeast, jumpEaseMost);
            }
        }
        st->relW[s][1] = st->relW[s][0];
        st->relW[s][0] = relW;
        st->relN[s] = za::min(st->relN[s] + 1, 2);
        if(st->easeT[s] < st->easeDur[s] && vr_agarms_jump_ease.value != 0.f)
        {
            st->easeT[s] += dt;
            const float w = smoothstep(0.f, 1.f, st->easeT[s] / za::max(st->easeDur[s], 1e-3f));
            const glm::quat turn = Hm[s].rot;
            Hm[s] = blendAbout(S1, sd.W, compose(C, st->easeFrom[s]), Hm[s], w);
            if(vr_agarms_aim_line.value != 0.f && g == Goal::Frame && !(s == 0 && t.reaching))
            {
                // vr_agarms_aim_line: a weapon's turn never eased (its drawn line the server's, where its shots go): the
                // hand's place eased, turned as its weapon wants about its grip
                const glm::vec3 gp = rigpose::apply(Hm[s], sd.grip.pos);
                Hm[s].rot = turn;
                Hm[s].pos = gp - turn * sd.grip.pos;
            }
            pushOut(s, Hm[s]);
        }
        free[s] = Hm[s];
    }
    // An arm solved: a weapon's hand turned as its weapon wants; a fist (a point) or a free hand with its wrist straight
    // (the hand as the rest pose has it on its forearm: turned with the forearm, a few rounds).
    const auto prevOf = [&](int s) { return st->haveElbow[s] ? &st->elbow[s] : nullptr; };
    const auto solveSide = [&](int s, rigpose::Rigid H, Goal g) {
        const Side& sd = m->side[s];
        const glm::vec3 S1 = rigpose::apply(C, sd.S);
        ArmOut a = solveArm(sd, s, S1, C.rot, H, prevOf(s), t.pole[s], lim);
        if(g == Goal::Frame)
        {
            return a;
        }
        const glm::vec3 grip = rigpose::apply(H, sd.grip.pos), wrist = rigpose::apply(H, sd.W);
        for(int round = 0; round < 3; round++)
        {
            H.rot = a.F.rot;
            H.pos = g == Goal::Point ? grip - H.rot * sd.grip.pos : wrist - H.rot * sd.W;
            a = solveArm(sd, s, S1, C.rot, H, prevOf(s), t.pole[s], lim);
        }
        return a;
    };
    // The right arm, then its weapon where its hand is.
    arms[0] = solveSide(0, Hm[0], t.goal[0]);
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
    bool leftDone = false;
    bool releasing = false;
    if(!t.second || !out.on[0])
    {
        // (vr_agarms_second_release: QC letting the second grip go, a one-handed swing, eased out round the shoulder
        // from where the hand held it, as the hold's own letting go)
        const bool release = vr_agarms_second_release.value != 0.f && vr_agarms_second_hold.value != 0.f && st->haveGrip2 &&
                             st->grip2w > 0.f && dt > 0.f && t.goal[1] != Goal::Frame && !t.cup;
        st->grip2w = release ? za::max(0.f, st->grip2w - dt / secondEase) : 0.f;
        st->held2 = false;
        const float w = smoothstep(0.f, 1.f, st->grip2w);
        if(release && w > 0.001f)
        {
            const glm::vec3 S1 = rigpose::apply(C, m->side[1].S);
            Hm[1] = blendAbout(S1, m->side[1].W, free[1], compose(C, st->grip2Rel), w);
            pushOut(1, Hm[1]);
            arms[1] = solveArm(m->side[1], 1, S1, C.rot, Hm[1], prevOf(1), t.pole[1], lim);
            leftDone = true;
            releasing = true;
        }
        else
        {
            st->grip2w = 0.f;
            st->haveGrip2 = false;
        }
    }
    if(t.second && out.on[0])
    {
        // (beyond its reach the hand slides back along the weapon towards the right hand, as a short arm's does: at most
        // to 3 units before the grip)
        const glm::vec3 S1 = rigpose::apply(C, m->side[1].S);
        const float armLen = m->side[1].Lu + m->side[1].Lf;
        rigpose::Rigid at = t.secondAt;
        rigpose::Rigid hl;
        float reach = 0.f;
        for(int step = 0; step < 12; step++)
        {
            hl = compose(compose(out.weapon[0], at), inverse(m->side[1].grip));
            reach = glm::length(rigpose::apply(hl, m->side[1].W) - S1);
            if(reach <= vr_agarms_second.value * armLen || at.pos.x <= 3.f)
            {
                break;
            }
            at.pos.x = za::max(3.f, at.pos.x - 1.f);
        }
        slid = t.secondAt.pos.x - at.pos.x;
        const ArmOut tryL = solveArm(m->side[1], 1, S1, C.rot, hl, prevOf(1), t.pole[1], lim);
        const bool hold = vr_agarms_second_hold.value != 0.f;
        // (vr_agarms_second_hold: a grip held is let go a little further than it is taken, no toggling at its edge)
        const bool keep = hold && st->held2;
        bool can = reach <= (vr_agarms_second.value + (keep ? secondSlack : 0.f)) * armLen &&
                   tryL.bend <= vr_agarms_wrist.value + (keep ? secondBendSlack : 0.f);
        if(hold && can != st->held2)
        {
            // (... and kept taken or let go a moment before it changes again)
            if(cl.time - st->held2At < secondDwell)
            {
                can = st->held2;
            }
            else
            {
                st->held2At = cl.time;
            }
        }
        st->held2 = can;
        if(!hold)
        {
            held2 = can;
            if(held2)
            {
                Hm[1] = hl;
                arms[1] = tryL;
                second = tryL.miss;
            }
        }
        else
        {
            // ... and taken and let go over secondEase, the hand going round its shoulder between its free place and
            // the grip (pushed out of the body), not along the straight line through it.
            st->grip2w = dt > 0.f ? za::clamp(st->grip2w + (can ? dt : -dt) / secondEase, 0.f, 1.f) : (can ? 1.f : st->grip2w);
            const float w = smoothstep(0.f, 1.f, st->grip2w);
            st->grip2Rel = compose(inverse(C), hl);
            st->haveGrip2 = true;
            held2 = w >= 0.5f;
            if(w >= 0.999f)
            {
                Hm[1] = hl;
                arms[1] = tryL;
                second = tryL.miss;
                leftDone = true;
            }
            else if(w > 0.001f)
            {
                Hm[1] = blendAbout(S1, m->side[1].W, free[1], hl, w);
                pushOut(1, Hm[1]);
                arms[1] = solveArm(m->side[1], 1, S1, C.rot, Hm[1], prevOf(1), t.pole[1], lim);
                second = held2 ? glm::length(rigpose::apply(arms[1].H, m->side[1].grip.pos) - rigpose::apply(hl, m->side[1].grip.pos)) : 0.f;
                leftDone = true;
            }
        }
    }
    if(held2 && !leftDone)
    {
        leftDone = true; // (vr_agarms_second_hold 0: held as it was)
    }
    if(!leftDone)
    {
        arms[1] = solveSide(1, Hm[1], t.cup && !t.second ? Goal::Point : t.second ? Goal::None : t.goal[1]);
    }
    out.held2 = held2;
    // The off hand's goal changing (a grip let go, taken): eased a moment.
    {
        const int mode = held2 || releasing ? 3 : t.cup ? 4 : static_cast<int>(t.goal[1]);
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
                arms[1] = solveArm(m->side[1], 1, rigpose::apply(C, m->side[1].S), C.rot, eased, prevOf(1), t.pole[1], lim);
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
    // (a degenerate solve: the last kept, nothing placed)
    for(int s = 0; s < 2; s++)
    {
        for(const rigpose::Rigid* r : {&arms[s].U, &arms[s].F, &arms[s].H})
        {
            const float sum = r->pos.x + r->pos.y + r->pos.z + r->rot.x + r->rot.y + r->rot.z + r->rot.w;
            uint32_t bits = 0;
            memcpy(&bits, &sum, sizeof(bits));
            if((bits & 0x7f800000u) == 0x7f800000u || za::abs(sum) > 1e5f) // (NaN or infinite: exponent all ones)
            {
                out = Placed{};
                return false;
            }
        }
    }
    for(int s = 0; s < 2; s++)
    {
        out.hand[s] = compose(arms[s].H, m->side[s].grip);
        out.elbow[s] = arms[s].elbow;
        out.shoulder[s] = arms[s].S;
        out.clavicle[s] = rigpose::apply(C, m->side[s].K);
        out.chest = C;
        out.reach[s] = arms[s].miss;
        out.pushed[s] = pushedBy[s];
        out.wrist[s] = arms[s].turned;
        out.poleOff[s] = arms[s].poleOff;
        out.poleWhy[s] = arms[s].poleWhy;
        st->bones[3 * s] = arms[s].U;
        st->bones[3 * s + 1] = arms[s].F;
        st->bones[3 * s + 2] = arms[s].H;
        st->chest = C;
        st->clav[s] = arms[s].K;
        st->elbow[s] = glm::inverse(C.rot) * (arms[s].elbow - arms[s].S);
        st->haveElbow[s] = true;
        st->drawnRel[s] = compose(inverse(C), arms[s].H);
        st->haveDrawn[s] = true;
    }
    // vr_agarms_cost: the solves' time (each armed humanoid's arms a frame; not the measures below)
    state.costUs += (Sys_DoubleTime() - solveStart) * 1e6;
    state.costN++;
    const bool glitch = agglitch::watching(num) || poselive::measuring(num); // (the Pose Studio's Live tape measures its subject too)
    if(glitch || vr_agrunt_ik_tag.value >= 0.f)
    {
        // (the frames as drawn, near enough: the lerp's poses)
        const int frame = (e->frame >= 0 && e->frame < hdr->numframes) ? e->frame : 0;
        const int pose1 = e->previouspose >= 0 && e->previouspose < hdr->numposes ? e->previouspose : hdr->frames[frame].firstpose;
        const int pose2 = e->currentpose >= 0 && e->currentpose < hdr->numposes ? e->currentpose : pose1;
        const float blend = za::clamp(static_cast<float>(cl.time - e->lerpstart) / za::max(e->lerptime, 1e-3f), 0.f, 1.f);
        if(glitch)
        {
            noteGlitch(*m, *st, rig, posed, kick, t, out, arms, C, num, second, held2, hdr, pose1, pose2, blend);
        }
        if(vr_agrunt_ik_tag.value >= 0.f)
        {
            noteErr(*m, *st, hdr, pose1, pose2, blend, arms, C, second, slid, t.second, held2);
        }
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

bool drawnArmBone(int num, const qmodel_t* model, const ragdoll::Rig& rig, int bone, glm::quat& rot, glm::vec3& pos)
{
    Ent* st = num > 0 && bone >= 0 ? entOf(num, false) : nullptr;
    // (posed last frame or the one before: a ragdoll made on the server's frame after the client's drawing)
    if(!st || st->model != model || st->on <= 0.f || st->frame < 0 || host_framecount - st->frame > 2)
    {
        return false;
    }
    for(int k = 0; k < numBones; k++) // (a hand on its forearm's bone: the forearm's, found first)
    {
        if(st->rigBone[k] != bone)
        {
            continue;
        }
        const rigpose::Rigid drawn = compose(st->bones[k], rigpose::key(rig, 0, bone));
        const float w = st->on * st->on * (3.f - 2.f * st->on); // (as drawn: rows' ease)
        rot = glm::normalize(glm::slerp(rot, drawn.rot, w));
        pos = glm::mix(pos, drawn.pos, w);
        if(k % 3 == 1 && st->rigBone[k + 1] == bone)
        {
            // (its hand on this forearm's bone: the hand's turn on it kept for its weapon, eased as drawn)
            const int side = k / 3;
            const rigpose::Rigid wrist = compose(inverse(st->bones[k]), st->bones[k + 1]);
            st->ragWrist[side].rot = glm::normalize(glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, wrist.rot, w));
            st->ragWrist[side].pos = wrist.pos * w;
            st->ragArms = true;
        }
        return true;
    }
    return false;
}

bool ragdollWrist(int num, const qmodel_t* model, int side, rigpose::Rigid& out)
{
    const Ent* st = num > 0 && side >= 0 && side < 2 ? entOf(num, false) : nullptr;
    if(!st || st->model != model || !st->ragArms)
    {
        return false;
    }
    out = st->ragWrist[side];
    return true;
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
        out[18] = glm::vec4{st->chest.rot.x, st->chest.rot.y, st->chest.rot.z, st->chest.rot.w};
        out[19] = glm::vec4{st->chest.pos, 0.f};
        for(int s = 0; s < 2; s++)
        {
            const rigpose::Rigid& k = st->clav[s];
            out[22 + 2 * s] = glm::vec4{k.rot.x, k.rot.y, k.rot.z, k.rot.w};
            out[23 + 2 * s] = glm::vec4{k.pos, 0.f};
        }
    }
    for(int s = 0; s < 2; s++)
    {
        out[14 + 2 * s] = glm::vec4{m->side[s].E, 0.f};
        out[15 + 2 * s] = glm::vec4{m->side[s].W, 0.f};
        out[20 + s] = glm::vec4{m->side[s].S, 0.f};
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
        t.slid = za::max(t.slid, e.slid);
        t.bend = za::max(t.bend, za::max(e.bend, e.bendL));
        t.want2 += e.want2;
        t.held2 += e.held2;
        if(vr_agrunt_ik_debug.value >= 1.f || e.miss[0] > 0.5f || e.miss[1] > 0.5f || e.torso > 1.f)
        {
            // (short: the console wraps its lines. Tag; edges' squash and stretch; in the torso; reach short right, left;
            // the off hand slid back; wrist bends right, left; second grip held of wanted; the worst edge)
            Con_Printf("agt %d e %.2f %.2f g %.1f t %.1f s %.1f %.1f sl %.0f b %.0f %.0f h %d/%d w %d %d\n", i, e.armShort, e.armLong, e.grow,
                e.torso, e.miss[0], e.miss[1], e.slid, e.bend, e.bendL, e.held2, e.want2, e.worst[0], e.worst[1]);
        }
    }
    Con_Printf("agarms: %d tags, %d frames: joint gap %.3f units (most), hand shape %.4f off (most edge, ratio), arm edges "
               "%.2f..%.2f of rest, in the torso %.2f units, reach short right %.2f / left %.2f units, off hand on its "
               "second grip %.2f units, wrist bend %.0f deg (most), second grip held %d of %d frames\n",
        tags, t.n, t.gap, t.handMost, t.armShort, t.armLong, t.torso, t.miss[0], t.miss[1], t.second, t.bend, t.held2, t.want2);
}

int saveState(int ent, unsigned char* out)
{
    static_assert(sizeof(Ent) <= stateBytes, "the Live tape's room for its state");
    static_assert(std::is_trivially_copyable_v<Ent>, "copied as bytes");
    const Ent* st = entOf(ent, false);
    if(!st)
    {
        return 0;
    }
    memcpy(out, st, sizeof(Ent));
    return static_cast<int>(sizeof(Ent));
}

void loadState(int ent, const unsigned char* in, int len)
{
    Ent* st = entOf(ent, len > 0);
    if(!st)
    {
        return;
    }
    if(len != static_cast<int>(sizeof(Ent)))
    {
        *st = Ent{}; // (none then: dropped)
        return;
    }
    memcpy(st, in, sizeof(Ent));
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
        const glm::vec3 gf = m.side[0].grip.rot * glm::vec3{1.f, 0.f, 0.f}, gu = m.side[0].grip.rot * glm::vec3{0.f, 0.f, 1.f};
        Con_Printf("  its rest gun: forward %.3f %.3f %.3f, up %.3f %.3f %.3f\n", gf.x, gf.y, gf.z, gu.x, gu.y, gu.z);
        for(int s = 0; s < 2; s++)
        {
            const Side& sd = m.side[s];
            Con_Printf("  %s: shoulder %.1f %.1f %.1f, elbow %.1f %.1f %.1f, wrist %.1f %.1f %.1f, grip %.1f %.1f %.1f; upper "
                       "%.1f fore %.1f; %d vertices, hand %d (bone %d), %d edges\n",
                s == 0 ? "right" : "left", sd.S.x, sd.S.y, sd.S.z, sd.E.x, sd.E.y, sd.E.z, sd.W.x, sd.W.y, sd.W.z, sd.grip.pos.x,
                sd.grip.pos.y, sd.grip.pos.z, sd.Lu, sd.Lf, static_cast<int>(sd.verts.size()), static_cast<int>(sd.hand.size()),
                sd.handBone, static_cast<int>(sd.edges.size() / 2));
        }
        const ragdoll::Rig* rig = ragdoll::rigFor(const_cast<qmodel_t*>(m.model));
        const agbody::Proxies* px = rig ? agbody::of(m.model, *rig, m.chest, m.torsoSides) : nullptr;
        if(px)
        {
            // (the rest pose's arms against its body: how far in each joint is as made)
            rigpose::Rigid rest[ragdoll::maxBones];
            for(int b = 0; b < rig->numBones; b++)
            {
                rest[b] = rigpose::key(*rig, 0, b);
            }
            const agbody::Posed q = agbody::pose(*px, rest, nullptr);
            for(int s = 0; s < 2; s++)
            {
                const Side& sd = m.side[s];
                float upperIn = 0.f; // (its upper arm's skin's deepest in the torso as made: the glitch finder's armpen at rest)
                for(const int v : sd.upper)
                {
                    upperIn = za::max(upperIn, agbody::depth(q, m.rest[static_cast<za::SizeT>(v)], 0.f));
                }
                Con_Printf("  %s at rest in the body: elbow %.1f, forearm %.1f, wrist %.1f, grip %.1f, upper arm's skin %.1f units; "
                           "clavicle %.1f %.1f %.1f, upper arm radius %.1f, %d cap edges\n",
                    s == 0 ? "right" : "left", agbody::depth(q, sd.E, 0.f), agbody::depth(q, (sd.E + sd.W) * 0.5f, 0.f),
                    agbody::depth(q, sd.W, 0.f), agbody::depth(q, sd.grip.pos, 0.f), upperIn, sd.K.x, sd.K.y, sd.K.z, sd.armR,
                    static_cast<int>(sd.shEdges.size() / 2));
            }
        }
    }
    agbody::print();
}

// vr_agarms_dump: each armed model's vertices (its rest place, its arm row) to the console (agv lines: the skin's check,
// scratch tools plot them).
void dump_f()
{
    for(const Skin& m : state.skins)
    {
        if(!m.ok)
        {
            continue;
        }
        Con_Printf("agvmodel %s\n", m.model->name);
        for(za::SizeT v = 0; v < m.rest.size(); v++)
        {
            Con_Printf("agv %d %.2f %.2f %.2f %.3f %.3f %.0f %.2f %.2f %.2f %.3f\n", static_cast<int>(v), m.rest[v].x, m.rest[v].y,
                m.rest[v].z, m.row[v].x, m.row[v].y, m.why[v].x, m.why[v].y, m.why[v].z, m.why[v].w, m.row[v].w);
        }
    }
}

// vr_agarms_cost: the arms' solves' time since the last (us each: one armed humanoid's arms a frame), then reset.
void cost_f()
{
    Con_Printf("agarms cost: %d solves, %.2f us each\n", state.costN, state.costN > 0 ? state.costUs / state.costN : 0.0);
    state.costUs = 0.0;
    state.costN = 0;
}

} // namespace

bool qvr::agarms::shoulderLimits(const qmodel_t* model, int bone, ShoulderInfo& out)
{
    for(const Skin& m : state.skins)
    {
        if(m.model != model || !m.ok)
        {
            continue;
        }
        const ragdoll::Rig* rig = ragdoll::rigFor(const_cast<qmodel_t*>(model));
        for(int s = 0; s < 2 && rig; s++)
        {
            const Side& sd = m.side[s];
            // (its upper arm: the rig's bone whose pivot is its shoulder)
            if(bone < 0 || bone >= rig->numBones || glm::length(rig->bones[bone].pivot - sd.S) > 0.5f)
            {
                continue;
            }
            out.side = s;
            out.chest = m.chest;
            out.clavicle = sd.K;
            out.shoulder = sd.S;
            out.axis = glm::normalize(glm::vec3{coneAxis.x, coneAxis.y * (s == 0 ? -1.f : 1.f), coneAxis.z});
            out.cone = vr_agarms_shoulder_cone.value;
            out.twist = vr_agarms_shoulder_twist.value;
            out.on = vr_agarms_limits.value != 0.f;
            return true;
        }
    }
    return false;
}

void qvr::agarms::registerCommands()
{
    Cmd_AddCommand("vr_agarms_cost", cost_f);
    Cmd_AddCommand("vr_agarms_info", info_f);
    Cmd_AddCommand("vr_agarms_dump", dump_f);
    agglitch::registerCommands();
}
