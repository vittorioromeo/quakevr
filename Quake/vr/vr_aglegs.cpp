// vr_aglegs.cpp -- armed humanoids' legs posed for a crouch (vr_aglegs.hpp; ROUND22.md, "Armed humanoids' legs: the
// pickup's kneel").

#include <type_traits>
#include <string.h>
#include "vr_mdldequant.hpp"
#include "vr_aglegs.hpp"
#include "vr_client.hpp"
#include "vr_agpose.hpp"
#include "vr_cvars.hpp"
#include "vr_geom.hpp"

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

#include <glm/gtc/quaternion.hpp>

#include <string.h>

using namespace qvr;

namespace
{

using geom::pi;
constexpr float d2r = pi / 180.f;
constexpr int maxModels = 4;
constexpr int maxEnts = 64;
constexpr int numLegBones = 6; // thigh, shin, foot; the right's then the left's

// The kneel at its lowest (the take's weight 1), for a right hand's reach (the left knee down; mirrored for a left's).
// Places are shares of its hips' height over its soles (rest): the same on any kind.
constexpr float pelvisDrop = 0.38f;     // the pelvis lowered so far (not sitting on its heel)
constexpr float pelvisForward = 0.1f;   // ... and moved forward
constexpr float pelvisPitch = 12.f;     // degrees forward
constexpr float pelvisYaw = 12.f;       // degrees: the kneeling hip back
constexpr float pelvisRoll = -4.f;      // degrees: ... and lower
constexpr float plantReach = 0.9f;      // the planted foot ahead: this share of where its shin stands upright
constexpr float plantShin = 0.97f;      // ... its knee this share of its shin over its ankle
constexpr float plantOut = 0.15f;       // its knee's pole out (of the thigh's length)
constexpr float kneelBack = 0.1f;       // the kneeling knee this share of the thigh behind its hip
constexpr float kneelGround = 0.1f;     // ... this high over the floor (of the hips' height: the knee's round)
constexpr float kneelToes = 0.55f;     // its ankle this share of its foot's length over its ankle's own height
constexpr float stepLift = 0.1f;        // a foot lifted this much (of the hips' height) as it steps
constexpr float toeOver = 0.3f;         // units: the kneeling foot's toes over the floor
constexpr float onUp = 0.12f;           // the legs fully on by this take weight
constexpr float onOut = 0.2f;           // seconds: back to the frame's legs when no take comes (a death, a stale pose)
constexpr float reachMost = 60.f;       // degrees: the pelvis bent at most this much further for the hand's reach
constexpr float reachShare = 0.95f;     // ... until its shoulder is within this share of its arm of the handle
constexpr float reachRate = 120.f;      // ... changing at most so fast (degrees a second)
constexpr float floorReach = 8.f;      // units (the model's): a floor traced this far above or below its soles at rest

struct LegRest
{
    int thigh{-1}, shin{-1};
    glm::vec3 H{0.f}, K{0.f}, A{0.f}, T{0.f}; // rest (pose 0, model units): hip, knee, ankle, toe
    float sole{0.f};                          // its foot's lowest (z)
    float L1{0.f}, L2{0.f};                   // thigh (H-K), shin (K-A)
    glm::vec3 hinge{0.f, -1.f, 0.f};          // its knee's way's normal (rest)
    glm::vec3 fwd{1.f, 0.f, 0.f};             // its foot's way (level)
};

struct Model
{
    const qmodel_t* model{nullptr};
    bool ok{false};
    LegRest side[2];
    int pelvis{-1};
    glm::vec3 P0{0.f};       // the pelvis's pivot (rest)
    float hipH{0.f};         // its hips over its soles (rest)
    za::Vector<glm::vec4> row; // each vertex's rest row (w set by restRow)
    za::Vector<glm::vec3> rest;
    za::Vector<int> verts;   // its legs' vertices
};

struct Ent
{
    int ent{0};
    const qmodel_t* model{nullptr};
    int frame{-1}; // host_framecount solved on
    double time{0.0};
    float on{0.f};
    float w{0.f};
    int side{0};   // the reaching hand
    int kind{0};
    rigpose::Rigid pelvis;      // rest -> now: the pose's
    rigpose::Rigid drawnPelvis; // ... as drawn (its on-share's)
    rigpose::Rigid bones[numLegBones];
    glm::vec3 hip[2]{}, knee[2]{}, ankle[2]{}, toe[2]{}, pole[2]{};
    float floorZ[2]{0.f, 0.f}; // the floor under each foot at its lowest (model z)
    float reachBend{0.f};      // the pelvis's further bend for the hand's reach now (degrees)
    bool planted{false}; // the planted foot down where it stays (its drift measured from plantAt, world)
    glm::vec3 plantAt{0.f};
};

struct Stats
{
    int frames{0};
    float drift{0.f}; // the planted foot's most drift once down (world units)
    float under{0.f}; // the most a leg vertex went under its floor (units)
    float drop{0.f};  // the pelvis's most drop (units)
    float kneeOver{1e9f}; // the kneeling knee's least height over its floor (units)
    float wild{0.f};      // the most a leg vertex is off its leg's joints' line (units), which
    int wildVert{-1};
};

// The strafe lean (vr_agrunt_strafe_lean) of one armed humanoid: its sideways speed (eased) and its lean.
constexpr float leanSpeed = 80.f;  // u/s: the full lean at this sideways speed (a strafe round you: 60-70)
constexpr float leanEase = 0.15f;  // seconds the sideways speed is eased over (a monster's 0.1 s steps)
constexpr float leanSettle = 0.12f; // seconds the lean follows its target over (in and out: a take, a stop)
constexpr float turnSettle = 0.18f; // seconds the strafe turn (vr_agrunt_strafe_turn) follows its target over
constexpr float leanJump = 64.f;   // units a frame: more is a teleport (no speed from it)
constexpr int leanBins = 5;        // the measure's bins: |sideways speed| by 20 u/s (the last 80 and more)

struct Lean
{
    int ent{0};
    int frame{-1}; // host_framecount laid on
    double time{0.0};
    glm::vec3 last{0.f}; // its drawn place then (world)
    bool have{false};
    float lat{0.f};      // its sideways speed (u/s, + to its left), eased
    float deg{0.f};      // its lean now (degrees, + to its left)
    float twist{0.f};    // its strafe turn now (vr_agrunt_strafe_turn: degrees of yaw, + to its left)
    rigpose::Rigid turn; // the turn laid on its bones (model space: the lean, then the strafe turn)
};

struct LeanStats
{
    int n[leanBins]{};
    float lean[leanBins]{};   // the sums of |lean| (degrees),
    float pelvis[leanBins]{}; // ... of |its pelvis's roll by it| (as solved),
    float chest[leanBins]{};  // ... of |its chest's roll| (as solved: the upper body's hold)
    float twist[leanBins]{};  // ... of its pelvis's yaw by the strafe turn, toward the way it goes (+: toward),
    float chestYaw[leanBins]{}; // ... of its chest's yaw as solved (signed the same way: the upper body kept aimed)
};

struct State
{
    Model models[maxModels];
    Ent ents[maxEnts];
    Stats stats[agpose::numKinds];
    Lean leans[maxEnts];
    LeanStats leanStats;
};
State state;

[[nodiscard]] glm::vec3 norm(const glm::vec3& v, const glm::vec3& fallback)
{
    const float l = glm::length(v);
    return l > 1e-5f ? v / l : fallback;
}

[[nodiscard]] rigpose::Rigid compose(const rigpose::Rigid& a, const rigpose::Rigid& b)
{
    return rigpose::Rigid{glm::normalize(a.rot * b.rot), a.rot * b.pos + a.pos};
}

[[nodiscard]] rigpose::Rigid inverse(const rigpose::Rigid& a)
{
    rigpose::Rigid r;
    r.rot = glm::inverse(a.rot);
    r.pos = -(r.rot * a.pos);
    return r;
}

// A frame (x along `fw`, z towards `upHint`) as a turn.
[[nodiscard]] glm::quat frameRot(const glm::vec3& fw, const glm::vec3& upHint)
{
    const glm::vec3 x = norm(fw, glm::vec3{1.f, 0.f, 0.f});
    glm::vec3 z = upHint - x * glm::dot(upHint, x);
    z = glm::length(z) > 1e-4f ? glm::normalize(z) : norm(glm::cross(x, glm::vec3{0.f, 1.f, 0.f}), glm::vec3{0.f, 0.f, 1.f});
    const glm::vec3 y = glm::cross(z, x);
    return glm::normalize(glm::quat_cast(glm::mat3{x, y, z}));
}

[[nodiscard]] glm::quat alignRot(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d)
{
    return glm::normalize(frameRot(c, d) * glm::inverse(frameRot(a, b)));
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

[[nodiscard]] glm::vec3 restNormal(const aliashdr_t* hdr, int v)
{
    const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const int n = base[static_cast<za::SizeT>(v)].lightnormalindex;
    return n < NUMVERTEXNORMALS ? glm::vec3{r_avertexnormals[n][0], r_avertexnormals[n][1], r_avertexnormals[n][2]}
                                : glm::vec3{0.f, 0.f, 1.f};
}

[[nodiscard]] int boneNamed(const ragdoll::Rig& rig, const char* name)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        if(strcmp(rig.bones[b].name, name) == 0)
        {
            return b;
        }
    }
    return -1;
}

// The knee's way across the hip-to-ankle line (u): towards `pole`, else forward.
[[nodiscard]] glm::vec3 kneeWay(const glm::vec3& u, const glm::vec3& towards)
{
    glm::vec3 v = towards - u * glm::dot(towards, u);
    if(glm::length(v) < 0.3f)
    {
        v = glm::vec3{1.f, 0.f, 0.f} - u * u.x;
    }
    return norm(v, glm::vec3{1.f, 0.f, 0.f});
}

// One leg's vertex weights (its row's x, y) at rest place `r`.
void weigh(const LegRest& L, int side, const glm::vec3& r, glm::vec4& row)
{
    const glm::vec3 dT = norm(L.K - L.H, glm::vec3{0.f, 0.f, -1.f});
    const glm::vec3 dS = norm(L.A - L.K, glm::vec3{0.f, 0.f, -1.f});
    const glm::vec3 nk = norm(dT + dS, dS);
    const float bandK = 0.16f * L.L2 + 0.5f;
    const float fK = geom::smoothstep(-bandK, bandK, glm::dot(r - L.K, nk), 1e-4f);
    const float bandA = 0.25f * (L.A.z - L.sole) + 0.3f;
    const float fF = geom::smoothstep(-bandA, bandA, L.A.z - r.z, 1e-4f);
    float bone = 0.f, f = 0.f, hip = 0.f;
    if(fF > 0.0005f)
    {
        bone = fF >= 0.9995f ? 2.f : 1.f;
        f = bone > 1.5f ? 0.f : fF;
    }
    else if(fK > 0.0005f)
    {
        bone = fK >= 0.9995f ? 1.f : 0.f;
        f = bone > 0.5f ? 0.f : fK;
    }
    else
    {
        hip = 1.f - geom::smoothstep(-0.1f * L.L1, 0.35f * L.L1, glm::dot(r - L.H, dT), 1e-4f);
    }
    row.x = 9.f + static_cast<float>(side) * 4.f + bone + za::min(f, 0.999f);
    row.y = hip;
}

// `model`'s legs, found once a model (null: none).
const Model* modelOf(const qmodel_t* model, const aliashdr_t* hdr, const ragdoll::Rig& rig)
{
    Model* slot = nullptr;
    for(Model& m : state.models)
    {
        if(m.model == model)
        {
            return m.ok ? &m : nullptr;
        }
        slot = (!slot && !m.model) ? &m : slot;
    }
    if(!slot || !hdr || hdr->numverts != rig.numVerts || rig.numPoses < 1)
    {
        return nullptr;
    }
    Model& m = *slot;
    m = Model{};
    m.model = model;
    mdldequant::prepare(model); // (its rest places as drawn: dequantised, whether or not it was uploaded yet)
    m.pelvis = boneNamed(rig, "pelvis");
    const char* thighs[2] = {"thigh_r", "thigh_l"};
    const char* shins[2] = {"shin_r", "shin_l"};
    const za::SizeT nv = static_cast<za::SizeT>(hdr->numverts);
    m.rest.resize(nv);
    for(za::SizeT v = 0; v < nv; v++)
    {
        m.rest[v] = mdldequant::modelVertex(hdr, 0, 0, 0.f, static_cast<int>(v));
    }
    bool ok = m.pelvis >= 0 && rig.bones[m.pelvis].parent < 0;
    float hips = 0.f;
    for(int s = 0; s < 2 && ok; s++)
    {
        LegRest& L = m.side[s];
        L.thigh = boneNamed(rig, thighs[s]);
        L.shin = boneNamed(rig, shins[s]);
        ok = L.thigh >= 0 && L.shin >= 0 && rig.bones[L.thigh].parent == m.pelvis && rig.bones[L.shin].parent == L.thigh;
        if(!ok)
        {
            break;
        }
        L.H = rig.bones[L.thigh].pivot;
        L.K = rig.bones[L.shin].pivot;
        // (the authored rig's foot a bone of its own, on the shin: its boot the shin's here, as the derived rigs' boots are)
        const int foot = rig.authored ? boneNamed(rig, s == 0 ? "foot_r" : "foot_l") : -1;
        const auto lower = [&](za::SizeT v) { return rig.vertBone[v] == L.shin || (foot >= 0 && rig.vertBone[v] == foot); };
        // (the ankle and the toe: the shin's vertices low down, its foot)
        float zmin = 1e9f;
        for(za::SizeT v = 0; v < nv; v++)
        {
            if(lower(v))
            {
                zmin = za::min(zmin, m.rest[v].z);
            }
        }
        const float len = L.K.z - zmin;
        if(zmin > 1e8f || len < 2.f)
        {
            ok = false;
            break;
        }
        // (its boot: the shin's vertices low down, heel to toe along the body's forward; the ankle over its back third)
        L.sole = zmin;
        L.fwd = glm::vec3{1.f, 0.f, 0.f};
        float heel = 1e9f, toe = -1e9f, ys = 0.f;
        int nf = 0;
        for(za::SizeT v = 0; v < nv; v++)
        {
            const glm::vec3& r = m.rest[v];
            if(lower(v) && r.z < zmin + 0.35f * len)
            {
                heel = za::min(heel, r.x);
                toe = za::max(toe, r.x);
                ys += r.y;
                nf++;
            }
        }
        if(nf == 0)
        {
            ok = false;
            break;
        }
        const float y = ys / static_cast<float>(nf);
        L.A = glm::vec3{heel + 0.3f * (toe - heel), y, zmin + 0.28f * len};
        L.T = glm::vec3{toe, y, zmin};
        L.L1 = glm::length(L.K - L.H);
        L.L2 = glm::length(L.A - L.K);
        const glm::vec3 u = norm(L.A - L.H, glm::vec3{0.f, 0.f, -1.f});
        L.hinge = norm(glm::cross(u, kneeWay(u, L.K - L.H)), glm::vec3{0.f, -1.f, 0.f});
        hips += (L.H.z - L.sole) * 0.5f;
        ok = L.L1 > 1.f && L.L2 > 1.f;
    }
    m.hipH = hips;
    m.P0 = m.pelvis >= 0 ? rig.bones[m.pelvis].pivot : glm::vec3{0.f};
    m.row.resize(nv);
    const int feet[2] = {rig.authored ? boneNamed(rig, "foot_r") : -1, rig.authored ? boneNamed(rig, "foot_l") : -1};
    const auto lowerOf = [&](int s, za::SizeT v) {
        return rig.vertBone[v] == m.side[s].shin || (feet[s] >= 0 && rig.vertBone[v] == feet[s]);
    };
    for(za::SizeT v = 0; v < nv && ok; v++)
    {
        glm::vec4 row{0.f};
        for(int s = 0; s < 2; s++)
        {
            const LegRest& L = m.side[s];
            if(rig.vertBone[v] == L.thigh || lowerOf(s, v))
            {
                weigh(L, s, m.rest[v], row);
                row.z = octPack(restNormal(hdr, static_cast<int>(v)));
                m.verts.pushBack(static_cast<int>(v));
            }
        }
        m.row[v] = row;
    }
    m.ok = ok && m.hipH > 4.f;
    if(!m.ok)
    {
        Con_DPrintf("aglegs: %s: no legs found (hips %.1f over the soles)\n", model->name, m.hipH);
    }
    return m.ok ? &m : nullptr;
}

[[nodiscard]] const Model* modelFor(const qmodel_t* model)
{
    for(const Model& m : state.models)
    {
        if(m.model == model && m.ok)
        {
            return &m;
        }
    }
    return nullptr;
}

Ent* entOf(int ent, bool make)
{
    Ent* free = nullptr;
    for(Ent& s : state.ents)
    {
        if(s.ent == ent)
        {
            return &s;
        }
        if(!free && (s.ent == 0 || s.frame < host_framecount - 600))
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

// The floor under model point `p` (model z; `fallback` none within reach).
[[nodiscard]] float floorAt(const glm::vec3& p, float fallback, const glm::vec3& origin, float yawDeg, float scale)
{
    const float c = za::cos(yawDeg * d2r), s = za::sin(yawDeg * d2r);
    const glm::vec3 w = origin + glm::vec3{c * p.x - s * p.y, s * p.x + c * p.y, p.z} * scale;
    vec3_t from{w.x, w.y, origin.z + (fallback + floorReach) * scale};
    vec3_t to{w.x, w.y, origin.z + (fallback - floorReach) * scale};
    vec3_t hit;
    TraceLine(from, to, hit);
    if(hit[2] <= to[2] + 0.01f || !cl.worldmodel)
    {
        return fallback;
    }
    return (hit[2] - origin.z) / scale;
}

// Two-bone IK: hip `H`, ankle wanted `A` (brought within the leg's reach), lengths, the knee's pole: the knee.
[[nodiscard]] glm::vec3 kneeOf(const glm::vec3& H, glm::vec3& A, float L1, float L2, const glm::vec3& pole, glm::vec3& u, glm::vec3& v)
{
    glm::vec3 d = A - H;
    float len = glm::length(d);
    u = norm(d, glm::vec3{0.f, 0.f, -1.f});
    const float most = (L1 + L2) * 0.999f, least = za::abs(L1 - L2) + 0.01f;
    len = za::clamp(len, least, most);
    A = H + u * len;
    const float a = (L1 * L1 - L2 * L2 + len * len) / (2.f * len);
    const float h = za::sqrt(za::max(0.f, L1 * L1 - a * a));
    v = kneeWay(u, pole - H);
    return H + u * a + v * h;
}

// The pose's key (agpose: its legs'), mirrored for a left hand's reach.
[[nodiscard]] agpose::Key legKey(int kind, bool mirror)
{
    agpose::Key k = agpose::value(agpose::legCode(za::clamp(kind, 0, agpose::numKinds - 1), agpose::Pickup));
    if(mirror)
    {
        for(int p = 0; p < agpose::NumParts; p++)
        {
            if(p == agpose::LegPelvisTurn)
            {
                k.v[p] = glm::vec3{-k.v[p].x, k.v[p].y, -k.v[p].z};
            }
            else
            {
                k.v[p].y = -k.v[p].y;
            }
        }
    }
    return k;
}

[[nodiscard]] glm::quat turnQuat(const glm::vec3& t)
{
    return glm::normalize(glm::angleAxis(t.x * d2r, glm::vec3{0.f, 0.f, 1.f}) * glm::angleAxis(t.y * d2r, glm::vec3{0.f, 1.f, 0.f}) *
                          glm::angleAxis(t.z * d2r, glm::vec3{1.f, 0.f, 0.f}));
}

// The skinned place of leg vertex `v` (its row) for `st` (the CPU's copy of RigLeg, wholly on).
[[nodiscard]] glm::vec3 skinned(const Model& m, const Ent& st, int v)
{
    const glm::vec4& row = m.row[static_cast<za::SizeT>(v)];
    const float code = row.x - 9.f;
    const int side = za::clamp(static_cast<int>(za::floor(code * 0.25f)), 0, 1);
    const float c = code - static_cast<float>(side) * 4.f;
    const int bone = za::min(static_cast<int>(za::floor(c)), 2);
    const float f = c - static_cast<float>(bone);
    const rigpose::Rigid& a = st.bones[side * 3 + bone];
    const glm::vec3 r = m.rest[static_cast<za::SizeT>(v)];
    glm::quat q = a.rot;
    glm::vec3 j{0.f};
    if(f > 0.0005f && bone < 2)
    {
        glm::quat qb = st.bones[side * 3 + bone + 1].rot;
        qb = glm::dot(q, qb) < 0.f ? -qb : qb;
        q = glm::normalize(glm::mix(q, qb, f));
        j = bone == 0 ? m.side[side].K : m.side[side].A;
    }
    else if(bone == 0 && row.y > 0.0005f)
    {
        glm::quat qp = st.drawnPelvis.rot;
        qp = glm::dot(q, qp) < 0.f ? -qp : qp;
        q = glm::normalize(glm::mix(q, qp, row.y));
        j = m.side[side].H;
    }
    return q * (r - j) + a.rot * j + a.pos;
}

// The body above the legs carried by the pelvis: each bone as the rest pose has it on the pose's pelvis (the take
// frames' own crouch and hunch dropped: the chest's bend is QC's), by the on-share; the legs' own: none.
void carries(const Model& m, const Ent& st, const ragdoll::Rig& rig, const rigpose::Rigid* posed, rigpose::Rigid* out)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        const bool leg = b == m.side[0].thigh || b == m.side[0].shin || b == m.side[1].thigh || b == m.side[1].shin;
        if(leg)
        {
            out[b] = rigpose::Rigid{};
            continue;
        }
        const rigpose::Rigid D = compose(st.pelvis, inverse(posed[b]));
        out[b].rot = glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, D.rot, st.on);
        out[b].pos = D.pos * st.on;
    }
}

void pose(const Model& m, Ent& st, const aglegs::Take& take, float dt, const glm::vec3& origin, float yawDeg, float scale)
{
    const float w = st.w;
    const int kneel = st.side == 0 ? 1 : 0; // (the side opposite the reaching hand)
    const int plant = 1 - kneel;
    const bool mirror = kneel == 0;
    const float my = mirror ? -1.f : 1.f; // (the kneel's y on the left: the key's and the defaults')
    const agpose::Key k = legKey(st.kind, mirror);
    const float hh = m.hipH;
    // The pelvis: down, forward and turned about its pivot (rest -> now), at its lowest; eased by the weight.
    const glm::vec3 off1 = glm::vec3{pelvisForward * hh, 0.f, -pelvisDrop * hh} + k.v[agpose::LegPelvis];
    const glm::vec3 turn1 = glm::vec3{pelvisYaw * my, pelvisPitch, pelvisRoll * my} + k.v[agpose::LegPelvisTurn];
    const glm::quat q1 = turnQuat(turn1);
    const auto pelvisAt = [&](float x) {
        const glm::quat q = glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, q1, x);
        return rigpose::Rigid{q, m.P0 + off1 * x - q * m.P0};
    };
    st.pelvis = pelvisAt(w);
    const rigpose::Rigid low = pelvisAt(1.f);
    // The hand's reach: the pelvis bent further forward about its pivot (from the hips: the body above with it) as far
    // as the reaching shoulder needs to bring the handle within its arm (QC's torso turn on top, about the chest's
    // joint); held through the grab, eased out over the lift.
    const auto bent = [&m](const rigpose::Rigid& p, float deg) {
        const glm::vec3 pivot = rigpose::apply(p, m.P0);
        const glm::quat q = glm::angleAxis(deg * d2r, glm::vec3{0.f, 1.f, 0.f});
        return rigpose::Rigid{glm::normalize(q * p.rot), pivot + q * (p.pos - pivot)};
    };
    float want = 0.f;
    if(take.reach && take.armLen > 1.f)
    {
        float best = 1e9f;
        want = 0.f;
        const glm::quat tq = turnQuat(take.torso);
        for(float e = 0.f; e <= reachMost + 0.01f; e += 2.f)
        {
            const rigpose::Rigid p = bent(st.pelvis, e);
            const glm::vec3 c = rigpose::apply(p, take.chest);
            const glm::vec3 sh = c + tq * (rigpose::apply(p, take.shoulder) - c);
            const float d = glm::length(sh - take.target) - take.armLen * reachShare;
            if(d < best)
            {
                best = d;
                want = e;
            }
            if(d <= 0.f)
            {
                break;
            }
        }
    }
    const float step = reachRate * dt;
    st.reachBend = st.reachBend + za::clamp(want - st.reachBend, -step, step);
    st.pelvis = bent(st.pelvis, st.reachBend);
    for(int s = 0; s < 2; s++)
    {
        const LegRest& L = m.side[s];
        const glm::vec3 H = rigpose::apply(st.pelvis, L.H);
        const glm::vec3 H1 = rigpose::apply(low, L.H);
        const float ankleH = L.A.z - L.sole;
        const float footLen = glm::length(glm::vec2{L.T.x - L.A.x, L.T.y - L.A.y});
        glm::vec3 A1, pole1;
        float stepAt = 0.f, stepEnd = 0.45f;
        float floor1 = L.sole;
        if(s == plant)
        {
            // Planted ahead: its knee over its ankle (the shin near upright), the thigh near level.
            const float out = s == 0 ? -1.f : 1.f;
            floor1 = floorAt(glm::vec3{H1.x + L.L1 * plantReach, L.A.y, L.sole}, L.sole, origin, yawDeg, scale);
            const float kz = floor1 + ankleH + L.L2 * plantShin;
            const float dz = H1.z - kz;
            A1 = glm::vec3{H1.x + za::sqrt(za::max(0.f, L.L1 * L.L1 - dz * dz)) * plantReach, L.A.y, floor1 + ankleH} +
                 k.v[agpose::LegPlantFoot];
            pole1 = glm::vec3{A1.x + 0.3f * L.L2, A1.y + out * plantOut * L.L1, kz} + k.v[agpose::LegPlantKnee];
        }
        else
        {
            // Kneeling: its knee down under its hip (behind it as far as the hip is too low for its thigh upright; over the
            // floor as far as it is too high), its shin back to its foot on its toes.
            floor1 = floorAt(glm::vec3{H1.x - kneelBack * L.L1, H1.y, L.sole}, L.sole, origin, yawDeg, scale);
            const float dz = za::clamp(floor1 + kneelGround * hh - H1.z, -L.L1, L.L1 * 0.5f);
            const float back0 = za::sqrt(za::max(0.f, L.L1 * L.L1 - dz * dz));
            const glm::vec3 kk{H1.x - za::max(back0, kneelBack * L.L1), H1.y, H1.z + dz};
            const float az = floor1 + ankleH + kneelToes * footLen;
            const float back = za::sqrt(za::max(0.f, L.L2 * L.L2 - (az - kk.z) * (az - kk.z)));
            A1 = glm::vec3{kk.x - back, kk.y, az} + k.v[agpose::LegKneelFoot];
            pole1 = kk + glm::vec3{0.2f * L.L1, 0.f, 0.f} + k.v[agpose::LegKneelKnee];
            stepAt = 0.05f;
            stepEnd = 0.6f;
        }
        // (stepping there from its rest place: lifted on the way)
        const float sp = geom::smoothstep(stepAt, stepEnd, w, 1e-4f);
        glm::vec3 A = glm::mix(L.A, A1, sp);
        A.z += stepLift * hh * za::sin(pi * sp);
        const glm::vec3 restPole = rigpose::apply(st.pelvis, L.K);
        const glm::vec3 pole = glm::mix(restPole, pole1, sp);
        glm::vec3 u, v;
        const glm::vec3 K = kneeOf(H, A, L.L1, L.L2, pole, u, v);
        const glm::vec3 hingeNow = norm(glm::cross(u, v), L.hinge);
        rigpose::Rigid& thigh = st.bones[s * 3];
        rigpose::Rigid& shin = st.bones[s * 3 + 1];
        rigpose::Rigid& foot = st.bones[s * 3 + 2];
        thigh.rot = alignRot(L.K - L.H, L.hinge, K - H, hingeNow);
        thigh.pos = H - thigh.rot * L.H;
        shin.rot = alignRot(L.A - L.K, L.hinge, A - K, hingeNow);
        shin.pos = K - shin.rot * L.K;
        // The foot: flat as at rest (planted, stepping), or pitched down onto its toes (kneeling).
        glm::quat fq{1.f, 0.f, 0.f, 0.f};
        if(s == kneel)
        {
            const glm::vec3 d = L.T - L.A;
            const float dx = glm::dot(d, L.fwd), dz = d.z;
            const float R = za::max(glm::length(glm::vec2{dx, dz}), 1e-3f);
            const float want = floor1 + toeOver - A.z;
            const float beta = za::atan2(dx, dz);
            const float phi = za::clamp(za::acos(za::clamp(want / R, -1.f, 1.f)) - beta, 0.f, 80.f * d2r) * sp;
            fq = glm::angleAxis(phi, norm(glm::cross(glm::vec3{0.f, 0.f, 1.f}, L.fwd), glm::vec3{0.f, 1.f, 0.f}));
        }
        foot.rot = fq;
        foot.pos = A - fq * L.A;
        st.hip[s] = H;
        st.knee[s] = K;
        st.ankle[s] = A;
        st.toe[s] = rigpose::apply(foot, L.T);
        st.pole[s] = pole;
        st.floorZ[s] = floor1;
        if(s == plant)
        {
            // (its drift once down: world)
            const float c = za::cos(yawDeg * d2r), sn = za::sin(yawDeg * d2r);
            const glm::vec3 wa = origin + glm::vec3{c * A.x - sn * A.y, sn * A.x + c * A.y, A.z} * scale;
            Stats& t = state.stats[za::clamp(st.kind, 0, agpose::numKinds - 1)];
            if(sp >= 0.999f)
            {
                if(!st.planted)
                {
                    st.planted = true;
                    st.plantAt = wa;
                }
                t.drift = za::max(t.drift, glm::length(wa - st.plantAt));
            }
            else
            {
                st.planted = false;
            }
        }
        else if(sp >= 0.999f)
        {
            Stats& t = state.stats[za::clamp(st.kind, 0, agpose::numKinds - 1)];
            t.kneeOver = za::min(t.kneeOver, K.z - floor1);
        }
    }
}

} // namespace

namespace qvr::aglegs
{

bool enabled()
{
    return vr_aglegs.value != 0.f;
}

bool restRow(const qmodel_t* model, const aliashdr_t* hdr, const ragdoll::Rig& rig, int v, int rowsAt, glm::vec4& out)
{
    const Model* m = modelOf(model, hdr, rig);
    if(!m || v < 0 || static_cast<za::SizeT>(v) >= m->row.size() || m->row[static_cast<za::SizeT>(v)].x < 8.5f)
    {
        return false;
    }
    out = m->row[static_cast<za::SizeT>(v)];
    out.w = static_cast<float>(rowsAt);
    return true;
}

bool solve(const entity_t* e, const aliashdr_t* hdr, const ragdoll::Rig& rig, int kind, const rigpose::Rigid* posed,
    const Take& take, const glm::vec3& origin, float yawDeg, float scale, rigpose::Rigid* carryOut)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        carryOut[b] = rigpose::Rigid{};
    }
    const int num = client::entityNumber(e);
    const Model* m = num > 0 ? modelFor(e->model) : nullptr;
    if(!m && num > 0 && rig.authored)
    {
        m = modelOf(e->model, hdr, rig); // (drawn skinned, its rest rows never asked for: found here)
    }
    if(!m || !hdr || hdr->numverts != rig.numVerts)
    {
        return false;
    }
    const bool want = enabled() && take.w > 0.002f;
    Ent* st = entOf(num, want);
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
    if(want)
    {
        st->w = za::clamp(take.w, 0.f, 1.f);
        st->side = take.side;
        st->kind = kind;
    }
    const float target = want ? geom::smoothstep(0.f, onUp, st->w, 1e-4f) : 0.f;
    st->on = za::max(target, st->on - dt / onOut);
    if(st->on <= 0.f)
    {
        st->frame = -1;
        st->planted = false;
        st->reachBend = 0.f;
        return false;
    }
    pose(*m, *st, take, dt, origin, yawDeg, za::max(scale, 0.01f));
    carries(*m, *st, rig, posed, carryOut);
    st->drawnPelvis = compose(carryOut[m->pelvis], posed[m->pelvis]);
    st->frame = host_framecount;
    // (the numbers: vr_aglegs_info)
    Stats& t = state.stats[za::clamp(kind, 0, agpose::numKinds - 1)];
    t.frames++;
    t.drop = za::max(t.drop, m->P0.z - rigpose::apply(st->pelvis, m->P0).z);
    if(st->on >= 0.999f)
    {
        for(const int v : m->verts)
        {
            const glm::vec4& row = m->row[static_cast<za::SizeT>(v)];
            const int side = row.x >= 13.f ? 1 : 0;
            const glm::vec3 p = skinned(*m, *st, v);
            t.under = za::max(t.under, st->floorZ[side] - p.z);
            // (a vertex flung off its leg: its distance from the leg's joints' line now)
            const auto seg = [&](const glm::vec3& a, const glm::vec3& b) {
                const glm::vec3 ab = b - a;
                const float u = za::clamp(glm::dot(p - a, ab) / za::max(glm::dot(ab, ab), 1e-6f), 0.f, 1.f);
                return glm::length(p - (a + ab * u));
            };
            const float off = za::min(za::min(seg(st->hip[side], st->knee[side]), seg(st->knee[side], st->ankle[side])),
                seg(st->ankle[side], st->toe[side]));
            if(off > t.wild)
            {
                t.wild = off;
                t.wildVert = v;
            }
        }
    }
    return true;
}

bool carry(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, rigpose::Rigid* out)
{
    const int num = client::entityNumber(e);
    const Model* m = num > 0 ? modelFor(e->model) : nullptr;
    const Ent* st = m ? entOf(num, false) : nullptr;
    if(!st || st->frame != host_framecount || st->model != e->model || st->on <= 0.f)
    {
        return false;
    }
    carries(*m, *st, rig, posed, out);
    return true;
}

void rows(const entity_t* e, const aliashdr_t* hdr, glm::vec4* out)
{
    for(int i = 0; i < blockRows; i++)
    {
        out[i] = glm::vec4{0.f};
    }
    out[0] = glm::vec4{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2], 0.f};
    out[1] = glm::vec4{1.f / hdr->scale[0], 1.f / hdr->scale[1], 1.f / hdr->scale[2], 0.f};
    const int num = client::entityNumber(e);
    const Model* m = num > 0 ? modelFor(e->model) : nullptr;
    const Ent* st = m ? entOf(num, false) : nullptr;
    if(!st || st->frame != host_framecount || st->model != e->model)
    {
        return;
    }
    out[0].w = st->on * st->on * (3.f - 2.f * st->on);
    for(int k = 0; k < numLegBones; k++)
    {
        const rigpose::Rigid& b = st->bones[k];
        out[2 + 2 * k] = glm::vec4{b.rot.x, b.rot.y, b.rot.z, b.rot.w};
        out[3 + 2 * k] = glm::vec4{b.pos, 0.f};
    }
    for(int s = 0; s < 2; s++)
    {
        out[14 + 2 * s] = glm::vec4{m->side[s].K, 0.f};
        out[15 + 2 * s] = glm::vec4{m->side[s].A, 0.f};
        out[20 + s] = glm::vec4{m->side[s].H, 0.f};
    }
    out[18] = glm::vec4{st->drawnPelvis.rot.x, st->drawnPelvis.rot.y, st->drawnPelvis.rot.z, st->drawnPelvis.rot.w};
    out[19] = glm::vec4{st->drawnPelvis.pos, 0.f};
}

bool placed(int ent, Placed& out)
{
    const Ent* st = entOf(ent, false);
    const Model* m = st ? modelFor(st->model) : nullptr;
    if(!m || st->frame < host_framecount - 2 || st->on <= 0.f)
    {
        return false;
    }
    out.on = st->on;
    out.w = st->w;
    out.kneel = st->side == 0 ? 1 : 0;
    out.pelvis = rigpose::Rigid{st->pelvis.rot, rigpose::apply(st->pelvis, m->P0)};
    for(int s = 0; s < 2; s++)
    {
        out.hip[s] = st->hip[s];
        out.knee[s] = st->knee[s];
        out.ankle[s] = st->ankle[s];
        out.toe[s] = st->toe[s];
        out.pole[s] = st->pole[s];
    }
    return true;
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

void resetModels()
{
    state = State{};
}

bool lean(const entity_t* e, const ragdoll::Rig& rig, const glm::vec3& origin, float yawDeg, bool hold, rigpose::Rigid& out)
{
    out = rigpose::Rigid{};
    const int num = client::entityNumber(e);
    if(num <= 0)
    {
        return false;
    }
    Lean* free = nullptr;
    Lean* st = nullptr;
    for(Lean& l : state.leans)
    {
        if(l.ent == num)
        {
            st = &l;
            break;
        }
        if(!free && (l.ent == 0 || l.frame < host_framecount - 600))
        {
            free = &l;
        }
    }
    const float most = za::max(vr_agrunt_strafe_lean.value, 0.f);
    const float mostTurn = za::clamp(vr_agrunt_strafe_turn.value, 0.f, 60.f);
    if(!st)
    {
        if((most <= 0.f && mostTurn <= 0.f) || !free)
        {
            return false;
        }
        *free = Lean{};
        free->ent = num;
        st = free;
    }
    if(st->frame == host_framecount)
    {
        out = st->turn;
        return st->deg != 0.f || st->twist != 0.f;
    }
    const float dt = st->have ? static_cast<float>(za::clamp(cl.time - st->time, 0.0, 0.25)) : 0.f;
    if(dt > 0.f)
    {
        const glm::vec3 d = origin - st->last;
        if(glm::length(d) < leanJump)
        {
            // (its motion across its facing: + to its left)
            const float yaw = yawDeg * d2r;
            const float side = (-za::sin(yaw) * d.x + za::cos(yaw) * d.y) / dt;
            st->lat += (side - st->lat) * (1.f - za::exp(-dt / leanEase));
        }
    }
    st->have = true;
    st->last = origin;
    st->time = cl.time;
    st->frame = host_framecount;
    const bool still = hold || vr_agrunt_freeze.value != 0.f;
    const float share = za::clamp(st->lat / leanSpeed, -1.f, 1.f);
    const float want = still ? 0.f : most * share;
    st->deg = dt > 0.f ? st->deg + (want - st->deg) * (1.f - za::exp(-dt / leanSettle)) : st->deg;
    if(za::abs(st->deg) < 0.01f && want == 0.f)
    {
        st->deg = 0.f;
    }
    // The strafe turn (vr_agrunt_strafe_turn): its pelvis and legs turned toward the way it goes by its sideways speed
    // alone (backing off to a side: the same turn, never flipped round); eased like the lean, a little slower.
    const float wantTurn = still ? 0.f : mostTurn * share;
    st->twist = dt > 0.f ? st->twist + (wantTurn - st->twist) * (1.f - za::exp(-dt / turnSettle)) : st->twist;
    if(za::abs(st->twist) < 0.01f && wantTurn == 0.f)
    {
        st->twist = 0.f;
    }
    // About the floor under its middle (its rest's lowest point): the top toward its left for a lean to its left.
    float floorZ = 1e9f;
    for(int b = 0; b < rig.numBones; b++)
    {
        floorZ = za::min(floorZ, za::min(rig.bones[b].pivot.z, rig.bones[b].end.z));
    }
    const glm::vec3 pivot{0.f, 0.f, floorZ < 1e8f ? floorZ : 0.f};
    // (the strafe turn about the upright through its middle, over the lean)
    st->turn.rot = glm::angleAxis(st->twist * d2r, glm::vec3{0.f, 0.f, 1.f}) *
                   glm::angleAxis(-st->deg * d2r, glm::vec3{1.f, 0.f, 0.f});
    st->turn.pos = pivot - st->turn.rot * pivot;
    out = st->turn;
    return st->deg != 0.f || st->twist != 0.f;
}

glm::quat twistNow(const entity_t* e)
{
    const int num = client::entityNumber(e);
    for(const Lean& l : state.leans)
    {
        if(num > 0 && l.ent == num && l.frame == host_framecount)
        {
            return glm::angleAxis(l.twist * d2r, glm::vec3{0.f, 0.f, 1.f});
        }
    }
    return glm::quat{1.f, 0.f, 0.f, 0.f};
}

bool leanNow(const entity_t* e, rigpose::Rigid& out)
{
    const int num = client::entityNumber(e);
    for(const Lean& l : state.leans)
    {
        if(num > 0 && l.ent == num && l.frame == host_framecount && (l.deg != 0.f || l.twist != 0.f))
        {
            out = l.turn;
            return true;
        }
    }
    out = rigpose::Rigid{};
    return false;
}

void leanMeasured(const entity_t* e, float pelvisRoll, float chestRoll, float pelvisYaw, float chestYaw)
{
    const int num = client::entityNumber(e);
    for(const Lean& l : state.leans)
    {
        if(num > 0 && l.ent == num && l.frame == host_framecount)
        {
            const int bin = za::clamp(static_cast<int>(za::abs(l.lat) / 20.f), 0, leanBins - 1);
            const float toward = l.lat < 0.f ? -1.f : 1.f;
            LeanStats& s = state.leanStats;
            s.n[bin]++;
            s.lean[bin] += za::abs(l.deg);
            s.pelvis[bin] += za::abs(pelvisRoll);
            s.chest[bin] += za::abs(chestRoll);
            s.twist[bin] += pelvisYaw * toward;
            s.chestYaw[bin] += chestYaw * toward;
            return;
        }
    }
}

} // namespace qvr::aglegs

namespace
{

// vr_aglegs_info [clear]: each armed model's legs (rest), the takes' numbers since the last clear.
void info_f()
{
    if(Cmd_Argc() > 1 && strcmp(Cmd_Argv(1), "clear") == 0)
    {
        for(Stats& s : state.stats)
        {
            s = Stats{};
        }
        Con_Printf("vr_aglegs_info: cleared\n");
        return;
    }
    for(const Model& m : state.models)
    {
        if(!m.model)
        {
            continue;
        }
        Con_Printf("aglegs model %s: %s, %d leg vertices, hips %.1f over the soles\n", m.model->name, m.ok ? "ok" : "none",
            static_cast<int>(m.verts.size()), static_cast<double>(m.hipH));
        for(int s = 0; s < 2 && m.ok; s++)
        {
            const LegRest& L = m.side[s];
            Con_Printf("  %s: hip %.1f %.1f %.1f knee %.1f %.1f %.1f ankle %.1f %.1f %.1f toe %.1f %.1f %.1f thigh %.1f shin %.1f\n",
                s == 0 ? "right" : "left", static_cast<double>(L.H.x), static_cast<double>(L.H.y), static_cast<double>(L.H.z),
                static_cast<double>(L.K.x), static_cast<double>(L.K.y), static_cast<double>(L.K.z), static_cast<double>(L.A.x),
                static_cast<double>(L.A.y), static_cast<double>(L.A.z), static_cast<double>(L.T.x), static_cast<double>(L.T.y),
                static_cast<double>(L.T.z), static_cast<double>(L.L1), static_cast<double>(L.L2));
        }
        for(const int v : m.verts)
        {
            // (a leg vertex far off its leg's line: the rig's misfit)
            const glm::vec4& row = m.row[static_cast<za::SizeT>(v)];
            const LegRest& L = m.side[row.x >= 13.f ? 1 : 0];
            const glm::vec3 r = m.rest[static_cast<za::SizeT>(v)];
            const auto seg = [&](const glm::vec3& a, const glm::vec3& b) {
                const glm::vec3 ab = b - a;
                const float t = za::clamp(glm::dot(r - a, ab) / za::max(glm::dot(ab, ab), 1e-6f), 0.f, 1.f);
                return glm::length(r - (a + ab * t));
            };
            const float d = za::min(za::min(seg(L.H, L.K), seg(L.K, L.A)), seg(L.A, L.T));
            if(Cmd_Argc() > 1 && strcmp(Cmd_Argv(1), "feet") == 0 && r.z < L.A.z + 3.f)
            {
                Con_Printf("  foot vertex %d (%s): %.1f %.1f %.1f, row %.3f\n", v, row.x >= 13.f ? "left" : "right",
                    static_cast<double>(r.x), static_cast<double>(r.y), static_cast<double>(r.z), static_cast<double>(row.x));
            }
            if(d > 5.f)
            {
                Con_Printf("  vertex %d off its leg by %.1f: rest %.1f %.1f %.1f, row %.3f %.2f\n", v, static_cast<double>(d),
                    static_cast<double>(r.x), static_cast<double>(r.y), static_cast<double>(r.z), static_cast<double>(row.x),
                    static_cast<double>(row.y));
            }
        }
    }
    for(const Ent& st : state.ents)
    {
        const Model* m = st.ent > 0 ? modelFor(st.model) : nullptr;
        if(m)
        {
            Con_Printf("aglegs ent %d: weight %.2f on %.2f reach %d bend %.0f, solved %d frames ago, pelvis drop %.1f, knees %.1f "
                       "%.1f, ankles %.1f %.1f over the floor\n",
                st.ent, static_cast<double>(st.w), static_cast<double>(st.on), st.side, static_cast<double>(st.reachBend),
                host_framecount - st.frame,
                static_cast<double>(m->P0.z - rigpose::apply(st.pelvis, m->P0).z), static_cast<double>(st.knee[0].z - st.floorZ[0]),
                static_cast<double>(st.knee[1].z - st.floorZ[1]), static_cast<double>(st.ankle[0].z - st.floorZ[0]),
                static_cast<double>(st.ankle[1].z - st.floorZ[1]));
        }
    }
    for(int k = 0; k < agpose::numKinds; k++)
    {
        const Stats& s = state.stats[k];
        Con_Printf("aglegs %s: %d frames posed, planted foot drift %.3f, legs under the floor %.2f, pelvis drop %.1f, "
                   "kneeling knee over the floor %.1f, a vertex off its leg %.1f (%d)\n",
            k == 0 ? "grunt" : "enforcer", s.frames, static_cast<double>(s.drift), static_cast<double>(s.under),
            static_cast<double>(s.drop), static_cast<double>(s.kneeOver > 1e8f ? -1.f : s.kneeOver), static_cast<double>(s.wild),
            s.wildVert);
    }
}

// vr_agrunt_lean_info [clear]: the strafe lean (vr_agrunt_strafe_lean) by sideways speed since the last clear: its
// frames, their mean lean, its pelvis's roll by it and its chest's roll as solved (the upper body held aimed); then
// each armed humanoid's now.
void leanInfo_f()
{
    if(Cmd_Argc() > 1 && strcmp(Cmd_Argv(1), "clear") == 0)
    {
        state.leanStats = LeanStats{};
        Con_Printf("vr_agrunt_lean_info: cleared\n");
        return;
    }
    const LeanStats& s = state.leanStats;
    for(int b = 0; b < leanBins; b++)
    {
        const float n = static_cast<float>(za::max(s.n[b], 1));
        Con_Printf("aglean: sideways %d-%s u/s: %d frames, lean %.2f deg, pelvis roll %.2f deg, chest roll %.2f deg, "
                   "pelvis yaw toward %.2f deg, chest yaw %.2f deg\n",
            b * 20, b == leanBins - 1 ? "up" : va("%d", b * 20 + 20), s.n[b], static_cast<double>(s.lean[b] / n),
            static_cast<double>(s.pelvis[b] / n), static_cast<double>(s.chest[b] / n), static_cast<double>(s.twist[b] / n),
            static_cast<double>(s.chestYaw[b] / n));
    }
    {
        // The strafe turn's verdict (vr_agrunt_test 87): at a strafe's speed (80 and up) the pelvis turned toward the
        // way it goes most of vr_agrunt_strafe_turn (its ease's lag aside), at every speed the chest's yaw within 2 deg.
        const int top = leanBins - 1;
        const float want = za::clamp(vr_agrunt_strafe_turn.value, 0.f, 60.f);
        const float got = s.twist[top] / static_cast<float>(za::max(s.n[top], 1));
        float chestMost = 0.f;
        for(int b = 0; b < leanBins; b++)
        {
            chestMost = za::max(chestMost, za::abs(s.chestYaw[b] / static_cast<float>(za::max(s.n[b], 1))));
        }
        const bool ok = s.n[top] > 0 && got >= want * 0.7f && got <= want * 1.05f + 0.5f && chestMost <= 2.f;
        Con_Printf("aglean twist: vr_agrunt_strafe_turn %.1f, pelvis yaw toward at 80+ u/s %.2f, chest yaw at most %.2f: %s\n",
            static_cast<double>(want), static_cast<double>(got), static_cast<double>(chestMost), ok ? "PASS" : "FAIL");
    }
    for(const Lean& l : state.leans)
    {
        if(l.ent > 0 && l.frame >= host_framecount - 2)
        {
            Con_Printf("aglean: ent %d: sideways %.1f u/s, lean %.2f deg, turn %.2f deg\n", l.ent, static_cast<double>(l.lat),
                static_cast<double>(l.deg), static_cast<double>(l.twist));
        }
    }
}

} // namespace

void qvr::aglegs::registerCommands()
{
    Cmd_AddCommand("vr_aglegs_info", info_f);
    Cmd_AddCommand("vr_agrunt_lean_info", leanInfo_f);
}
