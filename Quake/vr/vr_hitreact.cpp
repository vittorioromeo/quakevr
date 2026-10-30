// vr_hitreact.cpp -- living hit reactions (vr_hitreact.hpp; ROUND22.md, "Active ragdolls: living hit reactions").

#include "vr_mdldequant.hpp"
#include "vr_testkit.hpp"
#include "vr_hitreact.hpp"
#include "vr_hitreact_internal.hpp"
#include "vr_client.hpp"
#include "vr_agarms.hpp"
#include "vr_agbody.hpp"
#include "vr_aglegs.hpp"
#include "vr_aggrip.hpp"
#include "vr_animsmooth.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_mem.hpp"
#include "vr_poselive.hpp"
#include "vr_posestudio.hpp"
#include "vr_rigseq.hpp"
#include "vr_rig2.hpp"
#include "vr_rigstudio.hpp"
#include "vr_view.hpp"
#include "vr_render.hpp"
#include "vr_weapons.hpp"
#include "vr_geom.hpp"

#include "Zancle/Base/GetArraySize.hpp"

#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string.h>
#include <type_traits>

using namespace qvr;

namespace
{

constexpr int maxKicks = 4;      // an entity's kicks at once (the oldest replaced)
constexpr float life = 0.6f;     // s (times vr_hit_react_time): a kick's answer is ~0 past it
constexpr float zeta = 0.6f;     // the spring's damping ratio: a swing back of about a tenth
constexpr float omega = 15.7f;   // its natural frequency (rad/s): the peak at about 74 ms
constexpr float boneTurn = 0.45f; // rad: the struck bone's turn at size 1 (its parents: half each further up)
constexpr float kneeBend = 0.5f;  // rad: a leg hit's knee buckling at size 1
constexpr float leanTurn = 0.2f;  // rad: ... the torso leaning over that leg
constexpr float whiplash = 0.3f;  // the head's turn against the torso's (it lags)
constexpr float tiltTurn = 0.12f; // rad: the whole-model tilt at size 1 (models with no rig)
constexpr glm::vec3 forward{1.f, 0.f, 0.f}; // a model's facing (Quake's: +x)
constexpr glm::vec3 up{0.f, 0.f, 1.f};

// Where a bone is on the body, by its name (the seed tables': vr_ragdoll.cpp)
enum class Region : uint8_t { Body, Head, Arm, Leg };

[[nodiscard]] Region regionOf(const ragdoll::Bone& b)
{
    const auto starts = [&](const char* p) { return ZA_STRNCMP(b.name, p, strlen(p)) == 0; };
    if(b.role == modelmeta::BoneRole::Head || starts("head") || starts("jaw"))
    {
        return Region::Head;
    }
    if(starts("thigh") || starts("shin") || starts("leg") || starts("foot"))
    {
        return Region::Leg;
    }
    if(starts("upperarm") || starts("forearm") || starts("arm") || starts("hand"))
    {
        return Region::Arm;
    }
    return Region::Body;
}

// One blow (QVR_SVC_HITREACT), turned into bones' turns at its first draw (the pose then).
struct Kick
{
    double start{0.0};              // cl.time it came
    glm::vec3 at{0.f}, dir{0.f};    // the point struck and the push (world, as it came)
    int part{-1};                   // PositionalDamage's: 0 body, 1 head, 2 arm, 3 leg; -1 unknown
    float size{0.f};                // its turns' scale (by the share of health the blow took, vr_hit_react_strength)
    bool resolved{false};
    const qmodel_t* model{nullptr}; // the model it was resolved for (another one since: ignored)
    bool rigged{false};             // resolved for the bones (else the tilt)
    int hitBone{-1};
    uint32_t bones{0};                       // the bones turned (bits)
    glm::vec3 turn[ragdoll::maxBones];       // each one's turn (axis times angle, rad) in its own frame (rest space)
    glm::vec3 tilt{0.f};                     // the whole-model tilt (entity space)
    float hold{0.f};                         // a guard's (part guardPart): s its arms stay up (then down over guardOut)
};

// A guard (QC VR_Guard_Raise; vr_ragdoll_active_guard): its arms up before its face from a blow coming from `at`, posed
// afresh each frame (guardTurns), eased in over guardIn, held, eased out over guardOut.
constexpr int guardPart = 9;
constexpr float guardIn = 0.12f;
constexpr float guardOut = 0.25f;

struct Reaction
{
    Kick kicks[maxKicks];
    int count{0};
    double traced{-1.0}; // the cl.time of its last `hittrace` line (vr_hit_react_debug 2)
};

// The entities reacting, by number (the client's: parseEvent on the main thread, read as it draws them, also the
// main thread's).
struct HitReacts
{
    ankerl::unordered_dense::map<int, Reaction> entities;
    auto members() { return mem::list(entities); }
};
mem::Cache<HitReacts> reacts{"hit reactions", mem::MapChange};

// The drawing's cost since the last vr_hit_react_info (boneKicks and tiltModel, every alias model drawn: the lookup
// alone for one not reacting). The main thread.
struct Cost
{
    long long ns{0};
    int calls{0}, frames{0};
    double lastTime{-1.0};
};
Cost cost;

void countCost(long long ns)
{
    cost.ns += ns;
    cost.calls++;
    if(cl.time != cost.lastTime)
    {
        cost.lastTime = cl.time;
        cost.frames++;
    }
}

[[nodiscard]] float timeScale()
{
    return za::clamp(vr_hit_react_time.value, 0.25f, 4.f);
}

// The spring's answer to a blow at age `t` (s): 0 at 0, a peak of 1, a small swing back, 0 past `life`.
[[nodiscard]] float envelope(float t)
{
    t /= timeScale();
    if(!(t > 0.f) || t >= life)
    {
        return 0.f;
    }
    const float root = za::sqrt(1.f - zeta * zeta);
    const float wd = omega * root;
    const float peakT = za::acos(zeta) / wd;
    const float peak = za::exp(-zeta * omega * peakT) * root; // (sin at the peak: root)
    return za::exp(-zeta * omega * t) * za::sin(wd * t) / peak;
}

} // namespace

namespace hitreact_detail
{

// Where the renderer draws it (R_SetupEntityTransform's step lerp; called while it is drawn, after that has run): its
// origin and yaw. Not e->origin: a stepping monster's is where its next step ends, up to a step (15 units) ahead of
// the drawn body (the armed grunts' hands jumped by that at every step: ROUND22, "Armed grunts: presentation").
[[nodiscard]] glm::vec3 drawnOrigin(const entity_t* e, float* yawDeg)
{
    glm::vec3 o, a;
    render::drawnPlace(*e, o, a);
    if(yawDeg)
    {
        *yawDeg = a[1];
    }
    return o;
}

} // namespace hitreact_detail

namespace
{

// The entity's space as drawn (its yaw and scale; a monster has no pitch or roll): world -> model units.
[[nodiscard]] glm::vec3 toLocal(const entity_t* e, const glm::vec3& v, bool point)
{
    float yawDeg = 0.f;
    const glm::vec3 o = drawnOrigin(e, &yawDeg);
    glm::vec3 d = point ? v - o : v;
    const float yaw = yawDeg * (3.14159265f / 180.f);
    const float c = za::cos(yaw), s = za::sin(yaw);
    glm::vec3 out{c * d.x + s * d.y, -s * d.x + c * d.y, d.z};
    if(point)
    {
        out /= za::max(ENTSCALE_DECODE(e->scale), 0.01f);
    }
    return out;
}

using geom::segmentDistance;

// The bone nearest `h` (posed model space), of region `want` if any is (else of any); loose bones never.
[[nodiscard]] int nearestBone(const ragdoll::Rig& rig, const rigpose::Rigid* posed, const glm::vec3& h, int want)
{
    int best = -1;
    float bestD = 1e30f;
    for(int pass = 0; pass < 2 && best < 0; pass++)
    {
        for(int b = 0; b < rig.numBones; b++)
        {
            const ragdoll::Bone& bone = rig.bones[b];
            if(bone.joint == ragdoll::Joint::Loose || (pass == 0 && want >= 0 && static_cast<int>(regionOf(bone)) != want))
            {
                continue;
            }
            const float d = segmentDistance(h, rigpose::apply(posed[b], bone.pivot), rigpose::apply(posed[b], bone.end));
            if(d < bestD)
            {
                bestD = d;
                best = b;
            }
        }
    }
    return best;
}

[[nodiscard]] int findBone(const ragdoll::Rig& rig, Region region, int parent)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        if(regionOf(rig.bones[b]) == region && (parent < -1 || rig.bones[b].parent == parent) &&
            rig.bones[b].joint != ragdoll::Joint::Loose)
        {
            return b;
        }
    }
    return -1;
}

} // namespace

namespace hitreact_detail
{

[[nodiscard]] int chestOf(const ragdoll::Rig& rig)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        if(rig.bones[b].role == modelmeta::BoneRole::Chest)
        {
            return b;
        }
    }
    return findBone(rig, Region::Body, 0); // (a child of the root that is the body's)
}

} // namespace hitreact_detail

namespace
{

// The turn (axis times angle) the push `d` at `h` gives a bone jointed at `j`: about the push's torque, as much as the
// push is across the lever.
[[nodiscard]] glm::vec3 torqueTurn(const glm::vec3& h, const glm::vec3& j, const glm::vec3& d, float angle)
{
    const glm::vec3 r = h - j;
    const float len = za::sqrt(glm::dot(r, r));
    if(len < 0.5f)
    {
        return glm::vec3{0.f};
    }
    return glm::cross(r, d) * (angle / len);
}

// The turn the push `d` at `h` gives bone `b` (posed): bent the push's way about its joint, as much as the push is
// across the bone (a blow at the joint's own height bends it too: the bone's weight is along it, not at the joint), and
// twisted about its length by a blow off its middle (at most a third as much, 8 units off).
[[nodiscard]] glm::vec3 boneTurnOf(const ragdoll::Rig& rig, const rigpose::Rigid* posed, int b, const glm::vec3& h,
    const glm::vec3& d, float angle)
{
    const glm::vec3 j = rigpose::apply(posed[b], rig.bones[b].pivot);
    const glm::vec3 along = rigpose::apply(posed[b], rig.bones[b].end) - j;
    const float len = za::sqrt(glm::dot(along, along));
    if(len < 0.5f)
    {
        return torqueTurn(h, j, d, angle);
    }
    const glm::vec3 axis = along / len;
    const float twist = za::clamp(glm::dot(glm::cross(h - (j + 0.5f * along), d), axis) / 8.f, -1.f, 1.f);
    return (glm::cross(axis, d) + axis * (twist / 3.f)) * angle;
}

// The bones' turns of kick `k` on a rigged model in the pose `posed`.
void resolveRig(Kick& k, const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed)
{
    const glm::vec3 h = toLocal(e, k.at, true);
    glm::vec3 d = toLocal(e, k.dir, false);
    const float dl = za::sqrt(glm::dot(d, d));
    d = dl > 1e-4f ? d / dl : -forward;
    const int want = k.part == 1 ? static_cast<int>(Region::Head) : k.part == 2 ? static_cast<int>(Region::Arm)
                     : k.part == 3                                ? static_cast<int>(Region::Leg)
                     : k.part == 0                                ? static_cast<int>(Region::Body)
                                                                  : -1;
    glm::vec3 turn[ragdoll::maxBones];
    for(glm::vec3& t : turn)
    {
        t = glm::vec3{0.f};
    }
    const auto joint = [&](int b) { return rigpose::apply(posed[b], rig.bones[b].pivot); };
    int b = nearestBone(rig, posed, h, want);
    k.hitBone = b;
    const int chest = chestOf(rig);
    float f = 1.f;
    if(b >= 0 && rig.bones[b].parent < 0)
    {
        // The root (the pelvis) struck: driven back under the torso, which folds over it (the push's other way)
        b = chest;
        f = -0.6f;
    }
    // The bone struck and its parents up to the root, each about its joint, half as much each further up
    while(b >= 0 && rig.bones[b].parent >= 0 && rig.bones[b].joint != ragdoll::Joint::Loose)
    {
        turn[b] += boneTurnOf(rig, posed, b, h, d, boneTurn * k.size * f);
        f *= 0.5f;
        b = rig.bones[b].parent;
    }
    // A leg: the knee buckles (the shin bends back), the torso leans over it
    if(k.hitBone >= 0 && regionOf(rig.bones[k.hitBone]) == Region::Leg)
    {
        int shin = rig.bones[k.hitBone].joint == ragdoll::Joint::Hinge ? k.hitBone : -1;
        for(int c = 0; c < rig.numBones && shin < 0; c++)
        {
            if(rig.bones[c].parent == k.hitBone && rig.bones[c].joint == ragdoll::Joint::Hinge)
            {
                shin = c;
            }
        }
        if(shin >= 0)
        {
            const int thigh = rig.bones[shin].parent;
            glm::vec3 axis = posed[shin].rot * rig.bones[shin].hinge;
            const glm::vec3 knee = joint(shin);
            const glm::vec3 foot = rigpose::apply(posed[shin], rig.bones[shin].end);
            if(glm::dot(glm::cross(axis, foot - knee), forward) > 0.f)
            {
                axis = -axis; // (the foot goes back)
            }
            turn[shin] += axis * (kneeBend * k.size);
            if(thigh >= 0 && rig.bones[thigh].parent >= 0 && regionOf(rig.bones[thigh]) == Region::Leg)
            {
                if(chest >= 0)
                {
                    glm::vec3 side = joint(thigh) - joint(rig.bones[thigh].parent);
                    side.z = 0.f;
                    const float sl = za::sqrt(glm::dot(side, side));
                    side = (sl > 1e-3f ? side / sl : glm::vec3{0.f}) + 0.5f * forward;
                    turn[chest] += glm::cross(up, side) * (leanTurn * k.size);
                }
            }
        }
    }
    // The head lags the torso's turn (when it wasn't struck itself)
    const int head = rig.head >= 0 ? rig.head : findBone(rig, Region::Head, -2);
    if(head >= 0 && chest >= 0 && k.hitBone >= 0 && regionOf(rig.bones[k.hitBone]) != Region::Head &&
        rig.bones[head].parent == chest)
    {
        turn[head] -= whiplash * turn[chest];
    }
    k.bones = 0;
    for(int i = 0; i < rig.numBones; i++)
    {
        if(glm::dot(turn[i], turn[i]) > 1e-10f)
        {
            k.bones |= 1u << i;
            k.turn[i] = glm::conjugate(posed[i].rot) * turn[i];
        }
        else
        {
            k.turn[i] = glm::vec3{0.f};
        }
    }
}

// The turn (axis times angle) taking direction `a` to `b` the shortest way.
[[nodiscard]] glm::vec3 arcTurn(const glm::vec3& a, const glm::vec3& b)
{
    const glm::vec3 x = glm::cross(a, b);
    const float s = za::sqrt(glm::dot(x, x));
    const float angle = za::atan2(s, glm::dot(a, b));
    return s > 1e-6f ? x * (angle / s) : glm::vec3{0.f};
}

// A guard's turns on a rigged model in the pose `posed` (added to `total` times `w`): each arm (an arm bone whose parent
// isn't one, and its arm child) bent by two-bone IK so its hand is before the face, towards the blow (`k.at`) and to its
// own side, its elbow down and out.
void guardTurns(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, const Kick& k, float w,
    glm::vec3* total)
{
    const int head = rig.head >= 0 ? rig.head : findBone(rig, Region::Head, -2);
    if(head < 0)
    {
        return;
    }
    const glm::vec3 face = 0.5f * (rigpose::apply(posed[head], rig.bones[head].pivot) + rigpose::apply(posed[head], rig.bones[head].end));
    glm::vec3 toward = toLocal(e, k.at, true) - face;
    const float tl = za::sqrt(glm::dot(toward, toward));
    toward = tl > 1e-3f ? toward / tl : forward;
    glm::vec3 shield = forward + toward;
    const float sl = za::sqrt(glm::dot(shield, shield));
    shield = sl > 1e-3f ? shield / sl : forward;
    const int chest = chestOf(rig);
    const float mid = chest >= 0 ? rigpose::apply(posed[chest], rig.bones[chest].pivot).y : 0.f;
    for(int u = 0; u < rig.numBones; u++)
    {
        const ragdoll::Bone& bu = rig.bones[u];
        if(regionOf(bu) != Region::Arm || bu.joint == ragdoll::Joint::Loose || bu.parent < 0 ||
            regionOf(rig.bones[bu.parent]) == Region::Arm)
        {
            continue;
        }
        int lo = -1;
        for(int c = 0; c < rig.numBones && lo < 0; c++)
        {
            if(rig.bones[c].parent == u && regionOf(rig.bones[c]) == Region::Arm && rig.bones[c].joint != ragdoll::Joint::Loose)
            {
                lo = c;
            }
        }
        if(lo < 0)
        {
            continue;
        }
        const glm::vec3 S = rigpose::apply(posed[u], bu.pivot);
        const glm::vec3 E = rigpose::apply(posed[lo], rig.bones[lo].pivot);
        const glm::vec3 H = rigpose::apply(posed[lo], rig.bones[lo].end);
        const float a = glm::length(E - S), b = glm::length(H - E);
        if(a < 0.5f || b < 0.5f)
        {
            continue;
        }
        const float side = S.y >= mid ? 1.f : -1.f; // (a model's left: +y)
        const float L = a + b;
        const glm::vec3 G = face + shield * (0.35f * L) + glm::vec3{0.f, side * 0.1f * L, 0.08f * L};
        const glm::vec3 d = G - S;
        const float dl = za::max(glm::length(d), 1e-3f);
        const glm::vec3 dir = d / dl;
        const float dist = za::clamp(dl, za::abs(a - b) + 0.01f, 0.97f * (a + b));
        const float cosA = za::clamp((a * a + dist * dist - b * b) / (2.f * a * dist), -1.f, 1.f);
        const float sinA = za::sqrt(za::max(0.f, 1.f - cosA * cosA));
        glm::vec3 pole = glm::vec3{0.f, side * 0.6f, -1.f} - dir * glm::dot(glm::vec3{0.f, side * 0.6f, -1.f}, dir);
        const float pl = glm::length(pole);
        pole = pl > 1e-3f ? pole / pl : glm::vec3{0.f, 0.f, -1.f};
        const glm::vec3 E2 = S + a * (cosA * dir + sinA * pole);
        const glm::vec3 H2 = S + dir * dist;
        const glm::vec3 tu = arcTurn(glm::normalize(E - S), glm::normalize(E2 - S));
        const glm::vec3 back = glm::inverse(rigpose::fromVector(tu)) * (H2 - E2);
        const glm::vec3 tf = arcTurn(glm::normalize(H - E), glm::normalize(back));
        total[u] += w * tu;
        total[lo] += w * tf;
        if(vr_hit_react_debug.value >= 3.f)
        {
            Con_Printf("guardik %s S %.0f %.0f %.0f E %.0f %.0f %.0f H %.0f %.0f %.0f G %.0f %.0f %.0f face %.0f %.0f %.0f tu %.2f tf %.2f\n",
                bu.name, S.x, S.y, S.z, E.x, E.y, E.z, H.x, H.y, H.z, G.x, G.y, G.z, face.x, face.y, face.z, glm::length(tu),
                glm::length(tf));
        }
    }
}

// A guard's weight at age `t` (s), held `hold` s.
[[nodiscard]] float guardEnvelope(float t, float hold)
{
    const auto smooth = [](float x) {
        x = za::clamp(x, 0.f, 1.f);
        return x * x * (3.f - 2.f * x);
    };
    return smooth(t / guardIn) * (1.f - smooth((t - hold) / guardOut));
}

void resolveTilt(Kick& k, const entity_t* e)
{
    const glm::vec3 h = toLocal(e, k.at, true);
    glm::vec3 d = toLocal(e, k.dir, false);
    const float dl = za::sqrt(glm::dot(d, d));
    d = dl > 1e-4f ? d / dl : -forward;
    const glm::vec3 feet{0.f, 0.f, e->model ? e->model->mins[2] : -24.f};
    k.tilt = torqueTurn(h, feet, d, tiltTurn * k.size * (k.part == 1 ? 1.3f : 1.f));
}

// The entity's reaction now, its kicks of a model other than its own dropped; null if none.
[[nodiscard]] Reaction* reactionOf(const entity_t* e)
{
    if(reacts.entities.empty() || vr_hit_react.value == 0.f || !e || !e->model)
    {
        return nullptr;
    }
    const int num = client::entityNumber(e);
    if(num < 0)
    {
        return nullptr;
    }
    const auto it = reacts.entities.find(num);
    if(it == reacts.entities.end() || it->second.count == 0)
    {
        return nullptr;
    }
    if(ragdoll::sourceModel(e))
    {
        // A ragdoll now (a knockdown, a stagger, its get-up's start: vr_ragactive.cpp's): its body is the physics', and
        // what kicks were under way are dropped, not taken up again when it stands (the server sends none meanwhile:
        // QC's vr_knockdown)
        it->second.count = 0;
        return nullptr;
    }
    return &it->second;
}

} // namespace

namespace hitreact_detail
{

// a ∘ b: b first
rigpose::Rigid composeRigid(const rigpose::Rigid& a, const rigpose::Rigid& b)
{
    rigpose::Rigid r;
    r.rot = a.rot * b.rot;
    r.pos = a.rot * b.pos + a.pos;
    return r;
}

} // namespace hitreact_detail

namespace
{

void traceLine(const entity_t* e, Reaction& r, const ragdoll::Rig* rig, const glm::vec3* total, float tiltDeg)
{
    if(vr_hit_react_debug.value < 2.f || r.traced == cl.time)
    {
        return;
    }
    r.traced = cl.time;
    char line[512];
    int n = snprintf(line, sizeof(line), "hittrace %d t %.3f kicks %d", client::entityNumber(e), cl.time, r.count);
    if(rig)
    {
        float most = 0.f;
        int mostBone = -1;
        for(int b = 0; b < rig->numBones; b++)
        {
            const float a = za::sqrt(glm::dot(total[b], total[b])) * (180.f / 3.14159265f);
            if(a > most)
            {
                most = a;
                mostBone = b;
            }
        }
        n += snprintf(line + n, sizeof(line) - static_cast<size_t>(n), " most %.2f deg %s", most, mostBone >= 0 ? rig->bones[mostBone].name : "-");
        for(int b = 0; b < rig->numBones && n < static_cast<int>(sizeof(line)) - 24; b++)
        {
            const float a = za::sqrt(glm::dot(total[b], total[b])) * (180.f / 3.14159265f);
            if(a > 0.05f)
            {
                n += snprintf(line + n, sizeof(line) - static_cast<size_t>(n), " %s=%.2f", rig->bones[b].name, a);
            }
        }
    }
    else
    {
        n += snprintf(line + n, sizeof(line) - static_cast<size_t>(n), " tilt %.2f deg", tiltDeg);
    }
    Con_Printf("%s\n", line);
}

} // namespace

namespace hitreact_detail
{

RagErr ragErr;

} // namespace hitreact_detail

namespace
{

// A grip offset as a rigid motion (yaw about z, then pitch about y (down), then roll about x).
[[nodiscard]] rigpose::Rigid offsetRigid(const aggrip::Offset& o)
{
    constexpr float d2r = 3.14159265f / 180.f;
    rigpose::Rigid r;
    r.rot = glm::normalize(glm::angleAxis(o.angles.y * d2r, glm::vec3{0.f, 0.f, 1.f}) *
                           glm::angleAxis(o.angles.x * d2r, glm::vec3{0.f, 1.f, 0.f}) *
                           glm::angleAxis(o.angles.z * d2r, glm::vec3{1.f, 0.f, 0.f}));
    r.pos = o.pos;
    return r;
}

} // namespace

namespace hitreact_detail
{

// A hand's grip offset for a weapon (vr_agarms: its kind's global one, the weapon's own on top).
[[nodiscard]] rigpose::Rigid gripRigid(int kind, int slot, int hand)
{
    const rigpose::Rigid w = offsetRigid(aggrip::offset(kind, slot, hand));
    return composeRigid(offsetRigid(aggrip::global(kind, hand)), w);
}

// A pistol carried low (its barrel down past 27 degrees) in one hand: no cup (the other cupping it
// there reached across the body, its upper arm through the torso).
[[nodiscard]] bool lowCarry(const agarms::Targets& t)
{
    return (t.weapon[0].rot * glm::vec3{1.f, 0.f, 0.f}).z < -0.45f;
}

} // namespace hitreact_detail

namespace
{

void noteDrawnTurn(int num, const entity_t& p, const glm::vec3& at, const glm::vec3& fw);

} // namespace

namespace hitreact_detail
{

// A weapon drawn at `w` (model space of `e`; `kick`: its forearm's hit reaction on it, if any).
void placeProp(entity_t& p, const entity_t* e, const rigpose::Rigid& w, const rigpose::Rigid* kick)
{
    const rigpose::Rigid r = kick ? composeRigid(*kick, w) : w;
    float yaw = 0.f;
    (void)drawnOrigin(e, &yaw);
    const glm::vec3 at = toWorld(e, r.pos);
    const glm::vec3 fw = turnYaw(yaw, r.rot * glm::vec3{1.f, 0.f, 0.f});
    const glm::vec3 a = weaponAngles(fw, turnYaw(yaw, r.rot * glm::vec3{0.f, 0.f, 1.f}));
    const int num = static_cast<int>(&p - cl_entities);
    if(num > 0 && num < cl.num_entities)
    {
        noteDrawnTurn(num, p, at, fw);
    }
    for(int i = 0; i < 3; i++)
    {
        p.origin[i] = at[i];
        p.angles[i] = a[i];
    }
}

} // namespace hitreact_detail

namespace
{

// The held weapons as drawn lately (hitreact::drawnWeaponTurn): each one's turn and shift off the server's.
struct DrawnTurn
{
    int num{0};
    double time{0.0};
    glm::vec3 turn{0.f}, off{0.f};
};
za::Vector<DrawnTurn> drawnTurns;
constexpr za::SizeT maxDrawnTurns = 64;

[[nodiscard]] glm::vec3 forwardOfAngles(const float* angles)
{
    vec3_t a{-angles[0], angles[1], angles[2]}, f, r, u;
    AngleVectors(a, f, r, u);
    return {f[0], f[1], f[2]};
}

// placeProp's: the prop `p` (still at the server's place) is about to be drawn at `at`, its forward `fw`.
void noteDrawnTurn(int num, const entity_t& p, const glm::vec3& at, const glm::vec3& fw)
{
    const glm::vec3 f0 = glm::normalize(forwardOfAngles(p.angles));
    const glm::vec3 f1 = glm::normalize(fw);
    const glm::vec3 k = glm::cross(f0, f1);
    const float s = glm::length(k);
    const float deg = za::atan2(s, glm::dot(f0, f1)) * (180.f / 3.14159265f);
    DrawnTurn d;
    d.num = num;
    d.time = cl.time;
    d.turn = s > 1e-5f ? k * (deg / s) : glm::vec3{0.f};
    d.off = at - geom::vec(p.origin);
    for(DrawnTurn& o : drawnTurns)
    {
        if(o.num == num)
        {
            o = d;
            return;
        }
    }
    if(drawnTurns.size() >= maxDrawnTurns)
    {
        // (the stalest given up)
        za::SizeT old = 0;
        for(za::SizeT i = 1; i < drawnTurns.size(); i++)
        {
            old = drawnTurns[i].time < drawnTurns[old].time ? i : old;
        }
        drawnTurns[old] = d;
        return;
    }
    drawnTurns.pushBack(d);
}

} // namespace

namespace hitreact_detail
{

[[nodiscard]] bool heldProp(int num)
{
    return num > 0 && num < cl.num_entities && cl_entities[num].model && (cl_entities[num].effects & heldEffect) &&
           cl_entities[num].msgtime == cl.mtime[0];
}

[[nodiscard]] float viewDistance(const entity_t* e)
{
    const glm::vec3 o = drawnOrigin(e, nullptr);
    return glm::length(o - geom::vec(r_refdef.vieworg));
}

} // namespace hitreact_detail

namespace
{

// A ragdoll's drawn vertices and a hand's (rest, drawn) for their fit: drawnHandFit, ragWeaponMeasure.
struct RagWeaponScratch
{
    za::Vector<glm::vec3> drawn;
    za::Vector<glm::vec3> from, to;
};
RagWeaponScratch ragWeaponScratch;

// Ragdoll `num`'s hand `side` as drawn now: its vertices (agarms::handSkin's, the ones the posed arms hold a weapon by)
// fitted rest (times `scale`) -> drawn (world). False: none (no skin, not drawn). Leaves ragWeaponScratch.drawn the
// ragdoll's drawn vertices.
bool drawnHandFit(int num, const qmodel_t* src, int side, float scale, rigpose::Rigid& out)
{
    const za::Vector<int>* verts = nullptr;
    const za::Vector<glm::vec3>* rest = nullptr;
    rigpose::Rigid grip;
    za::Vector<glm::vec3>& drawn = ragWeaponScratch.drawn;
    if(!agarms::handSkin(src, side, verts, rest, grip) || !ragdoll::skinnedVertices(num, drawn, nullptr, true))
    {
        return false;
    }
    za::Vector<glm::vec3>& from = ragWeaponScratch.from;
    za::Vector<glm::vec3>& to = ragWeaponScratch.to;
    from.clear();
    to.clear();
    for(const int v : *verts)
    {
        if(v >= 0 && v < static_cast<int>(drawn.size()) && v < static_cast<int>(rest->size()))
        {
            from.pushBack((*rest)[static_cast<za::SizeT>(v)] * scale);
            to.pushBack(drawn[static_cast<za::SizeT>(v)]);
        }
    }
    if(from.size() < 3)
    {
        return false;
    }
    out = fitRigid(from.data(), to.data(), static_cast<int>(from.size()));
    return glm::dot(out.rot, out.rot) > 0.5f;
}

// A ragdoll's weapons (an active ragdoll's: a stagger, a shove's fall, a tug) on its hands as drawn now, by their grip
// offsets: never left floating while the server's catch up.
void ragdollProps(const ArmPose& s, const entity_t* e)
{
    const qmodel_t* src = ragdoll::sourceModel(e);
    const ArmedRig* a = armedRigOf(src);
    const ragdoll::Rig* rig = a ? ragdoll::rigFor(const_cast<qmodel_t*>(src)) : nullptr;
    if(!rig)
    {
        return;
    }
    // (the hands' grip frames its posed arms held its weapons by, so the weapon stays as it was held: without this its
    // weapons were left to the server's placing, riding the limb nearest them as the ragdoll began, off the hand)
    const int kind = static_cast<int>(a - armedRigs);
    for(int side = 0; side < 2; side++)
    {
        const int num = side == 0 ? s.prop : ((s.flags & apLeftWeapon) ? s.propL : 0);
        if(!heldProp(num))
        {
            continue;
        }
        rigpose::Rigid gripFrame;
        int handBone = -1;
        if(!agarms::ragdollGrip(src, side, gripFrame, handBone))
        {
            continue;
        }
        glm::quat rot;
        glm::vec3 pos;
        float scale = 1.f;
        if(!ragdoll::drawnPart(client::entityNumber(e), handBone, rot, pos, scale))
        {
            continue;
        }
        entity_t& p = cl_entities[num];
        rigpose::Rigid w = composeRigid(gripFrame,
            gripRigid(kind, weapons::slotForModel(p.model), side == 0 ? aggrip::Main : aggrip::Off));
        // vr_agarms: the hand as drawn (its vertices' fit, rest -> now), as the posed arms moved those vertices when it
        // held the weapon alive. Its bone alone is not that: the authored rig blends the hand's vertices over the hand and
        // its forearm and fingers (the grunt's weapon was drawn 0.6 units and 20 degrees off its hand by it).
        if(rigpose::Rigid H; drawnHandFit(client::entityNumber(e), src, side, scale, H))
        {
            rot = H.rot;
            pos = H.pos;
        }
        // (Made from its posed arms, its hand on its forearm's bone: the hand's turn on it then, the weapon where it was.)
        else if(rigpose::Rigid wrist; agarms::ragdollWrist(client::entityNumber(e), src, side, wrist))
        {
            w = composeRigid(wrist, w);
        }
        const glm::vec3 at = rot * (scale * w.pos) + pos;
        const glm::quat wr = rot * w.rot;
        if(!placeable(rigpose::Rigid{wr, at - drawnOrigin(e, nullptr)}))
        {
            continue; // (no place for it on the ragdoll's hand now: the server's)
        }
        const glm::vec3 an = weaponAngles(wr * glm::vec3{1.f, 0.f, 0.f}, wr * glm::vec3{0.f, 0.f, 1.f});
        {
            // (vr_agrunt_ik_err: how far the server's own placing, drawn before this, was off the drawn hand)
            vec3_t sa{-p.angles[0], p.angles[1], p.angles[2]}, f, r, u;
            AngleVectors(sa, f, r, u);
            const glm::vec3 fw = wr * glm::vec3{1.f, 0.f, 0.f};
            const float off = glm::length(geom::vec(p.origin) - at);
            const float turn = za::acos(za::clamp(glm::dot(fw, geom::vec(f)), -1.f, 1.f)) * (180.f / 3.14159265f);
            ragErr.n++;
            ragErr.offMax = za::max(ragErr.offMax, off);
            ragErr.offSum += off;
            ragErr.turnMax = za::max(ragErr.turnMax, turn);
            ragErr.propFar = za::max(ragErr.propFar, glm::length(at - drawnOrigin(e, nullptr)));
        }
        for(int i = 0; i < 3; i++)
        {
            p.origin[i] = at[i];
            p.angles[i] = an[i];
        }
    }
}

// vr_agrunt_ragweapon_check (vr_agrunt_test 88): an armed humanoid's held weapons against its hands as drawn now, per
// state (QC's tests name it: vr_agrunt_ragweapon_err <label>). The hand's grip frame fitted to its drawn vertices (the
// vertices the posed arms hold a weapon by: agarms::handSkin) against the weapon's own grip frame (its entity's frame
// less its grip offset); the hand's vertices and the weapon's line (its grip forward and back) inside the torso's
// proxies (vr_agbody) as drawn.
struct RagWeaponErr
{
    int n{0};
    float gripMax{0.f}, gripSum{0.f}, turnMax{0.f};
    float handIn{0.f}, weaponIn{0.f};
};
RagWeaponErr ragWeaponErr;

[[nodiscard]] glm::quat rotOfAngles(const float* angles)
{
    vec3_t a{-angles[0], angles[1], angles[2]}, f, r, u;
    AngleVectors(a, f, r, u);
    const glm::mat3 m{geom::vec(f), -geom::vec(r), geom::vec(u)};
    return glm::normalize(glm::quat_cast(m));
}

void ragWeaponMeasure(const ArmPose& s, const entity_t* e)
{
    const qmodel_t* src = ragdoll::sourceModel(e);
    const ArmedRig* a = armedRigOf(src);
    const ragdoll::Rig* rig = a ? ragdoll::rigFor(const_cast<qmodel_t*>(src)) : nullptr;
    const int num = client::entityNumber(e);
    za::Vector<glm::vec3>& drawn = ragWeaponScratch.drawn;
    if(!rig || !ragdoll::skinnedVertices(num, drawn, nullptr, true))
    {
        if(vr_agrunt_ragweapon_check.value >= 2.f)
        {
            Con_Printf("ragweapon %d: not measured (%s)\n", num, rig ? "no skinned vertices" : "no rig");
        }
        return;
    }
    const int chest = boneNamed(*rig, "chest");
    // The torso as drawn: each part's rest -> now (world units; a ragdoll's scale 1 here).
    rigpose::Rigid posed[ragdoll::maxBones];
    float scale = 1.f;
    for(int b = 0; b < rig->numBones; b++)
    {
        glm::quat r{1.f, 0.f, 0.f, 0.f};
        glm::vec3 t{0.f};
        if(ragdoll::drawnPart(num, b, r, t, scale))
        {
            posed[b] = rigpose::Rigid{r, t};
        }
    }
    const agbody::Proxies* px = chest >= 0 ? agbody::of(src, *rig, chest, a->torsoSides) : nullptr;
    const agbody::Posed q = px && za::abs(scale - 1.f) < 1e-3f ? agbody::pose(*px, posed, nullptr) : agbody::Posed{};
    const int kind = static_cast<int>(a - armedRigs);
    for(int side = 0; side < 2; side++)
    {
        const int pn = side == 0 ? s.prop : ((s.flags & apLeftWeapon) ? s.propL : 0);
        const za::Vector<int>* verts = nullptr;
        const za::Vector<glm::vec3>* rest = nullptr;
        rigpose::Rigid grip;
        if(!heldProp(pn) || !agarms::handSkin(src, side, verts, rest, grip) || verts->size() < 3)
        {
            if(vr_agrunt_ragweapon_check.value >= 2.f && pn > 0)
            {
                Con_Printf("ragweapon %d side %d: not measured (prop %d held %d)\n", num, side, pn, heldProp(pn) ? 1 : 0);
            }
            continue;
        }
        za::Vector<glm::vec3>& from = ragWeaponScratch.from;
        za::Vector<glm::vec3>& to = ragWeaponScratch.to;
        from.clear();
        to.clear();
        float handIn = 0.f;
        for(const int v : *verts)
        {
            if(v < 0 || v >= static_cast<int>(drawn.size()) || v >= static_cast<int>(rest->size()))
            {
                continue;
            }
            from.pushBack((*rest)[static_cast<za::SizeT>(v)] * scale);
            to.pushBack(drawn[static_cast<za::SizeT>(v)]);
            if(q.n > 0)
            {
                handIn = za::max(handIn, agbody::depth(q, drawn[static_cast<za::SizeT>(v)], 0.f));
            }
        }
        if(from.size() < 3)
        {
            continue;
        }
        const rigpose::Rigid H = fitRigid(from.data(), to.data(), static_cast<int>(from.size()));
        const glm::quat wantRot = glm::normalize(H.rot * grip.rot);
        const glm::vec3 wantPos = H.rot * (grip.pos * scale) + H.pos;
        const entity_t& p = cl_entities[pn];
        const rigpose::Rigid off = gripRigid(kind, weapons::slotForModel(p.model), side == 0 ? aggrip::Main : aggrip::Off);
        const glm::quat wr = rotOfAngles(p.angles);
        const glm::quat gr = glm::normalize(wr * glm::inverse(off.rot));
        const glm::vec3 gp = geom::vec(p.origin) - scale * (gr * off.pos);
        const float d = glm::length(gp - wantPos);
        const float turn = glm::degrees(2.f * za::acos(za::clamp(za::abs(glm::dot(gr, wantRot)), 0.f, 1.f)));
        float weaponIn = 0.f;
        if(q.n > 0 && p.model)
        {
            // (its line: from its grip back by its model's rear and on by its front, along its forward)
            const glm::vec3 fw = wr * glm::vec3{1.f, 0.f, 0.f};
            const float back = za::clamp(-p.model->mins[0], 0.f, 48.f), front = za::clamp(p.model->maxs[0], 0.f, 64.f);
            weaponIn = za::max(0.f, agbody::segmentDepth(q, gp - fw * back, gp + fw * front, 12, 0.f));
        }
        RagWeaponErr& r = ragWeaponErr;
        r.n++;
        r.gripMax = za::max(r.gripMax, d);
        r.gripSum += d;
        r.turnMax = za::max(r.turnMax, turn);
        r.handIn = za::max(r.handIn, handIn);
        r.weaponIn = za::max(r.weaponIn, weaponIn);
        if(vr_agrunt_ragweapon_check.value >= 2.f)
        {
            Con_Printf("ragweapon %d side %d: grip %.2f units %.1f deg off its hand; hand %.2f, weapon %.2f in its torso\n", num,
                side, d, turn, handIn, weaponIn);
        }
    }
}

} // namespace

namespace hitreact_detail
{

// A weapon's frame on a body (model space): finite, within reach of it.
[[nodiscard]] bool placeable(const rigpose::Rigid& w)
{
    const auto finite = [](float x) {
        uint32_t bits = 0;
        memcpy(&bits, &x, sizeof(bits));
        return (bits & 0x7f800000u) != 0x7f800000u && x > -1e6f && x < 1e6f; // (NaN, infinite: exponent all ones)
    };
    for(int i = 0; i < 3; i++)
    {
        if(!finite(w.pos[i]))
        {
            return false;
        }
    }
    if(!finite(w.rot.x) || !finite(w.rot.y) || !finite(w.rot.z) || !finite(w.rot.w) || glm::dot(w.rot, w.rot) < 0.5f)
    {
        return false;
    }
    return glm::length(w.pos) < 96.f;
}

} // namespace hitreact_detail

namespace
{

// --- The Pose Studio's Live mode (vr_poselive.hpp): its subject's frames recorded, or drawn from its tape -----------

static_assert(sizeof(ArmPose) <= poselive::poseBytes, "the Live tape's room for an arm pose");
static_assert(sizeof(entity_t) <= poselive::entBytes, "the Live tape's room for an entity");
static_assert(std::is_trivially_copyable_v<ArmPose>, "copied as bytes");

// The subject as the game had it when paused (put back on Play): its arm pose, its solve's state, its entity.
struct LiveHeld
{
    bool on{false};
    int ent{0};
    ArmPose pose;
    int armsLen{0}, legsLen{0}, bodyLen{0};
    unsigned char arms[agarms::stateBytes];
    unsigned char legs[aglegs::stateBytes];
    unsigned char body[agbody::stateBytes];
    entity_t place;
};
LiveHeld liveHeld;

// `to`'s place, frames and their lerps from `from`, its times moved by `shift` seconds (drawn at another clock as at
// its own).
void livePlace(entity_t& to, const entity_t& from, float shift)
{
    for(int i = 0; i < 3; i++)
    {
        to.origin[i] = from.origin[i];
        to.angles[i] = from.angles[i];
        to.previousorigin[i] = from.previousorigin[i];
        to.currentorigin[i] = from.currentorigin[i];
        to.previousangles[i] = from.previousangles[i];
        to.currentangles[i] = from.currentangles[i];
    }
    const auto moved = [shift](float t) { return t != 0.f ? t + shift : t; };
    to.frame = from.frame;
    to.lerpflags = from.lerpflags;
    to.lerpstart = moved(from.lerpstart);
    to.lerptime = from.lerptime;
    to.lerpfinish = moved(from.lerpfinish);
    to.animlerpfinish = moved(from.animlerpfinish);
    to.previouspose = from.previouspose;
    to.currentpose = from.currentpose;
    to.animfrom = from.animfrom;
    to.animpose0 = from.animpose0;
    to.animspan0 = from.animspan0;
    to.movelerpstart = moved(from.movelerpstart);
    to.movelerpfinish = moved(from.movelerpfinish);
}

// Playing: the subject's frame on the tape before its solve (its inputs, the solve's state, its entity as drawn).
void liveRecord(const ArmPose& s, const entity_t* e, int pose1, int pose2, float blend)
{
    poselive::Frame* f = poselive::record();
    f->pose1 = pose1;
    f->pose2 = pose2;
    f->blend = blend;
    f->frame = e->frame;
    memcpy(f->ent, e, sizeof(entity_t));
    memcpy(f->pose, &s, sizeof(ArmPose));
    f->armsLen = agarms::saveState(s.ent, f->arms);
    f->legsLen = aglegs::saveState(s.ent, f->legs);
    f->bodyLen = agbody::saveState(s.ent, f->body);
}

// The keys' edits since a frame was recorded laid on its arm pose as QC lays its keys on (vr_armedgrunt_pose.qc
// VR_AGP_Pose: the targets' places and turns in its body's space after the torso's turn, the elbows' poles, the torso
// turning the targets about its waist).
void liveEdit(ArmPose& s, const agpose::Key& d, float scale)
{
    const auto turn = [](const glm::vec3& r) {
        const float a = glm::length(r);
        return a < 1e-4f ? glm::quat{1.f, 0.f, 0.f, 0.f} : glm::angleAxis(a * (3.14159265f / 180.f), r / a);
    };
    if(s.flags & apBody)
    {
        if(glm::length(d.v[agpose::Torso]) > 1e-4f)
        {
            const glm::vec3 t1 = s.torso + d.v[agpose::Torso];
            if(!(s.flags & apFixed))
            {
                const glm::quat R = glm::normalize(posestudio::torsoRotation(t1) * glm::inverse(posestudio::torsoRotation(s.torso)));
                s.right = s.waist + R * (s.right - s.waist);
                s.left = s.waist + R * (s.left - s.waist);
                s.fw = R * s.fw;
                s.up = R * s.up;
                s.fwL = R * s.fwL;
                s.upL = R * s.upL;
            }
            s.torso = t1;
        }
        s.elbowR += d.v[agpose::ElbowR];
        s.elbowL += d.v[agpose::ElbowL];
        for(glm::vec3* p : {&s.elbowR, &s.elbowL})
        {
            *p = glm::length(*p) > 1.f ? glm::normalize(*p) : *p;
        }
    }
    s.right += d.v[agpose::RPos] / scale;
    const glm::quat qr = turn(d.v[agpose::RTurn]);
    s.fw = qr * s.fw;
    s.up = qr * s.up;
    if(s.flags & apLeft)
    {
        s.left += d.v[agpose::LPos] / scale;
    }
    if(s.flags & apLeftWeapon)
    {
        const glm::quat ql = turn(d.v[agpose::LTurn]);
        s.fwL = ql * s.fwL;
        s.upL = ql * s.upL;
    }
}

// Paused: the subject drawn from the tape. The window's frames solved in turn at their own clocks (the first from its
// recorded state, each from its recorded inputs with the edits laid on), the last drawn at the live clock as it was.
void liveReplay(ArmPose& s, entity_t* e, const aliashdr_t* hdr)
{
    int first = 0, count = 0;
    if(!poselive::window(first, count) || count <= 0)
    {
        return;
    }
    if(!liveHeld.on)
    {
        liveHeld.on = true;
        liveHeld.ent = s.ent;
        liveHeld.pose = s;
        liveHeld.armsLen = agarms::saveState(s.ent, liveHeld.arms);
        liveHeld.legsLen = aglegs::saveState(s.ent, liveHeld.legs);
        liveHeld.bodyLen = agbody::saveState(s.ent, liveHeld.body);
        liveHeld.place = *e;
    }
    const double timeWas = cl.time;
    const int frameWas = host_framecount;
    const int ent = s.ent;
    const float scale = za::max(ENTSCALE_DECODE(e->scale), 0.01f);
    entity_t rec;
    for(int k = 0; k < count; k++)
    {
        const poselive::Frame& f = poselive::at(first, k);
        if(k == 0)
        {
            agarms::loadState(ent, f.arms, f.armsLen);
            aglegs::loadState(ent, f.legs, f.legsLen);
            agbody::loadState(ent, f.body, f.bodyLen);
        }
        memcpy(&s, f.pose, sizeof(ArmPose));
        s.ent = ent;
        agpose::Key d;
        if(poselive::delta(f, d))
        {
            liveEdit(s, d, scale);
        }
        memcpy(&rec, f.ent, sizeof(entity_t));
        cl.time = f.time;
        host_framecount = frameWas - (count - 1 - k); // (each frame of the window a frame of its own to the solve)
        livePlace(*e, rec, 0.f);
        poselive::setWarming(k < count - 1);
        relinkSkinned(s, e, hdr, f.pose1, f.pose2, f.blend);
    }
    poselive::setWarming(false);
    cl.time = timeWas;
    host_framecount = frameWas;
    livePlace(*e, rec, static_cast<float>(timeWas - poselive::at(first, count - 1).time));
}

// Play again: the subject as the game had it when paused.
void liveRelease(ArmPose& s, entity_t* e)
{
    liveHeld.on = false;
    s = liveHeld.pose;
    agarms::loadState(s.ent, liveHeld.arms, liveHeld.armsLen);
    aglegs::loadState(s.ent, liveHeld.legs, liveHeld.legsLen);
    agbody::loadState(s.ent, liveHeld.body, liveHeld.bodyLen);
    livePlace(*e, liveHeld.place, 0.f);
}

} // namespace

namespace qvr::hitreact
{

bool drawnWeaponTurn(int prop, glm::vec3& turn, glm::vec3& off)
{
    for(const DrawnTurn& d : drawnTurns)
    {
        if(d.num == prop && cl.time - d.time <= 0.15 && cl.time >= d.time)
        {
            turn = d.turn;
            off = d.off;
            return true;
        }
    }
    return false;
}

void parseArmPose()
{
    ArmPoseMsg m;
    const int ent = MSG_ReadShort();
    m.prop = MSG_ReadShort();
    m.flags = MSG_ReadByte();
    m.wr = static_cast<float>(MSG_ReadByte()) / 255.f;
    const auto readVec = [](glm::vec3& v) {
        for(int i = 0; i < 3; i++)
        {
            v[i] = MSG_ReadCoord(cl.protocolflags);
        }
    };
    const auto readDir = [](glm::vec3& v) {
        for(int i = 0; i < 3; i++)
        {
            v[i] = static_cast<float>(MSG_ReadShort()) / 32000.f;
        }
        v = glm::length(v) > 1e-4f ? glm::normalize(v) : glm::vec3{1.f, 0.f, 0.f};
    };
    readVec(m.v);
    readDir(m.fwB);
    readDir(m.upB);
    readDir(m.fwG);
    readDir(m.upG);
    m.wo = static_cast<float>(MSG_ReadByte()) / 255.f;
    m.wl = static_cast<float>(MSG_ReadByte()) / 255.f;
    readVec(m.vl);
    if(m.flags & apLeftWeapon)
    {
        m.propL = MSG_ReadShort();
        readDir(m.fwL);
        readDir(m.upL);
    }
    if(m.flags & apBody)
    {
        for(int i = 0; i < 3; i++)
        {
            m.torso[i] = static_cast<float>(MSG_ReadShort()) / 100.f;
        }
        for(glm::vec3* v : {&m.elbowR, &m.elbowL})
        {
            for(int i = 0; i < 3; i++)
            {
                (*v)[i] = static_cast<float>(MSG_ReadShort()) / 32000.f;
            }
        }
        for(int i = 0; i < 3; i++)
        {
            m.waist[i] = MSG_ReadCoord(cl.protocolflags);
        }
        if(m.flags & apFixed)
        {
            const int k = MSG_ReadByte(); // (a take's weight x 127, its left hand's reach 128)
            m.take = static_cast<float>(k & 127) / 127.f;
            m.takeSide = (k & 128) ? 1 : 0;
        }
    }
    if((m.flags & (apLeftFree | apTwo)) == (apLeftFree | apTwo))
    {
        // The pose keys' off hand (QC VR_AGP_Pose's off-hand block: never both flags otherwise): its own flags, then the
        // grip's change on the weapon and the free hand's (places x 100, turns x 10)
        const int b = MSG_ReadByte();
        m.flags = (m.flags & ~(apLeftFree | apTwo)) | ((b & 1) ? apLeftFree : 0) | ((b & 2) ? apTwo : 0);
        m.offKeyed = (b & (4 | 8)) != 0; // (16 alone: the IK stretch only)
        m.keyGun = (b & 32) != 0;        // (its key's On the Gun: no flag of its own follows)
        const auto readShorts = [](glm::vec3& v, float per) {
            for(int i = 0; i < 3; i++)
            {
                v[i] = static_cast<float>(MSG_ReadShort()) / per;
            }
        };
        if(b & 4)
        {
            readShorts(m.gripPos, 100.f);
            readShorts(m.gripTurn, 10.f);
        }
        if(b & 8)
        {
            readShorts(m.freePos, 100.f);
            readShorts(m.freeTurn, 10.f);
        }
        if(b & 16)
        {
            m.stretch = 1.f + static_cast<float>(MSG_ReadByte()) / 500.f; // (its kind and class's IK stretch)
        }
    }
    m.t = cl.mtime[0];
    ArmPose* s = armPoseOf(ent, true);
    if(!s)
    {
        return;
    }
    if(m.t > s->cur.t)
    {
        s->prev = s->cur;
    }
    s->cur = m;
}

// The Rig Studio's subject drawn on the authored rig (Range of Motion; rig2::sourceModel its .mdl, `src`): no
// arms to solve (the rig's pose is the studio's), its held weapons on that rig's hands (rigstudio::authoredWeapon), each
// at its grip offset as relinkSkinned holds it (its kind's global one, the weapon's own on top).
void relinkAuthored(const ArmPose& s, const entity_t* e, const qmodel_t* src)
{
    const ArmedRig* a = armedRigOf(src);
    if(!a)
    {
        return;
    }
    const int kind = static_cast<int>(a - armedRigs);
    for(int side = 0; side < 2; side++)
    {
        const int num = side == 0 ? s.prop : ((s.flags & apLeftWeapon) ? s.propL : 0);
        if(!heldProp(num))
        {
            continue;
        }
        const rigpose::Rigid offset = gripRigid(kind, weapons::slotForModel(cl_entities[num].model), side == 0 ? aggrip::Main : aggrip::Off);
        rigpose::Rigid w;
        if(rigstudio::authoredWeapon(s.ent, side, num, offset, w) && placeable(w))
        {
            placeProp(cl_entities[num], e, w, nullptr);
        }
    }
}

void relinkArms()
{
    if(!vr_agrunt_ik.value)
    {
        return;
    }
    for(ArmPose& s : armPoses.slots)
    {
        if(s.ent <= 0 || s.ent >= cl.num_entities)
        {
            continue;
        }
        const entity_t* e = &cl_entities[s.ent];
        if(poselive::replaying(s.ent))
        {
            // (the Pose Studio's Live mode paused: its subject drawn from its tape)
            if(e->model && e->model->type == mod_alias && armedRigOf(e->model))
            {
                liveReplay(s, &cl_entities[s.ent], static_cast<const aliashdr_t*>(Mod_Extradata(e->model)));
            }
            continue;
        }
        if(liveHeld.on && liveHeld.ent == s.ent)
        {
            liveRelease(s, &cl_entities[s.ent]);
        }
        if(e->model && e->msgtime == cl.mtime[0] && ragdoll::sourceModel(e))
        {
            ragdollProps(s, e); // (its weapons on its ragdoll's hands; the server's own placing is for the game: QC VR_AG_RagProp)
            if(vr_agrunt_ragweapon_check.value >= 3.f)
            {
                Con_Printf("ragweapon %d: ragdoll branch\n", s.ent);
            }
            if(vr_agrunt_ragweapon_check.value != 0.f)
            {
                ragWeaponMeasure(s, e);
            }
            continue;
        }
        if(s.cur.t < cl.mtime[0] - armPoseStale)
        {
            continue;
        }
        // (the Rig Studio's subject posed on the authored rig: its "#rig2" swap is undone after each frame's drawing, so
        // here it has its .mdl again: studioPosedModel finds it by the studio's pose, not by the swap)
        if(const qmodel_t* src = e->model && e->msgtime == cl.mtime[0] ? rig2::studioPosedModel(e) : nullptr)
        {
            relinkAuthored(s, e, src); // (the Rig Studio's subject drawn on the authored rig)
            continue;
        }
        if(!e->model || e->model->type != mod_alias || e->msgtime != cl.mtime[0] ||
            !armedRigOf(e->model) || ragdoll::sourceModel(e))
        {
            continue; // (a ragdoll now, a stagger's, a tug's: the server keeps its weapon on its hand's limb, QC VR_AG_RagProp)
        }
        const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e->model));
        int pose1 = 0, pose2 = 0;
        float blend = 0.f;
        drawnPoses(e, hdr, pose1, pose2, blend);
        if(!placeArmPose(e, hdr, pose1, pose2, blend, s))
        {
            continue;
        }
        const bool rec = poselive::recording(s.ent); // (the Pose Studio's Live tape)
        if(rec)
        {
            liveRecord(s, e, pose1, pose2, blend);
        }
        relinkSkinned(s, e, hdr, pose1, pose2, blend);
        if(rec)
        {
            poselive::recorded();
        }
    }
    aggrip::frame();
}
} // namespace qvr::hitreact

extern "C" void VR_RelinkArmedGrunts(void)
{
    qvr::hitreact::relinkArms();
}

namespace qvr::hitreact
{

void parseEvent()
{
    const int num = MSG_ReadShort();
    glm::vec3 at, dir;
    for(int i = 0; i < 3; i++)
    {
        at[i] = MSG_ReadCoord(cl.protocolflags);
    }
    for(int i = 0; i < 3; i++)
    {
        dir[i] = static_cast<float>(MSG_ReadChar()) / 127.f;
    }
    const int part = MSG_ReadByte() - 1;
    const int amount = MSG_ReadByte();
    const float share = static_cast<float>(MSG_ReadByte()) / 255.f;
    if(vr_hit_react.value == 0.f || num <= 0 || num >= cl.num_entities)
    {
        return;
    }
    // The share of its health the blow took: a grunt's shotgun blast near 1, a nail a third, a shambler's rocket a sixth
    const float size = za::clamp(1.4f * za::sqrt(share), 0.15f, 1.2f) * za::clamp(vr_hit_react_strength.value, 0.f, 3.f);
    // (the entities whose kicks are all over forgotten)
    const float over = life * timeScale();
    for(auto& [n, r] : reacts.entities)
    {
        int kept = 0;
        for(int i = 0; i < r.count; i++)
        {
            const Kick& old = r.kicks[i];
            if(cl.time - old.start < (old.part == guardPart ? old.hold + guardOut : over))
            {
                r.kicks[kept++] = r.kicks[i];
            }
        }
        r.count = kept;
    }
    erase_if(reacts.entities, [](const auto& kv) { return kv.second.count == 0; });
    Reaction& r = reacts.entities[num];
    // A guard (its hold in hundredths of a second): one guarding already holds on longer, towards the new blow.
    if(part == guardPart)
    {
        for(int i = 0; i < r.count; i++)
        {
            if(r.kicks[i].part == guardPart)
            {
                Kick& g = r.kicks[i];
                g.hold = za::max(g.hold, static_cast<float>(cl.time - g.start) + static_cast<float>(amount) / 100.f);
                g.at = at;
                return;
            }
        }
    }
    int slot = r.count < maxKicks ? r.count++ : 0;
    if(slot == 0 && r.count == maxKicks)
    {
        for(int i = 1; i < maxKicks; i++)
        {
            slot = r.kicks[i].start < r.kicks[slot].start ? i : slot;
        }
    }
    Kick& k = r.kicks[slot];
    k = Kick{};
    k.start = cl.time;
    k.at = at;
    k.dir = dir;
    k.part = part;
    k.size = size;
    k.hold = part == guardPart ? static_cast<float>(amount) / 100.f : 0.f;
    if(vr_hit_react_debug.value >= 1.f)
    {
        Con_Printf("hitreact: ent %d part %d damage %d share %.2f size %.2f at (%.0f %.0f %.0f) dir (%.2f %.2f %.2f)\n", num,
            part, amount, share, size, at.x, at.y, at.z, dir.x, dir.y, dir.z);
    }
}

bool boneKicksNow(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, rigpose::Rigid* out)
{
    if(vr_hit_react.value != 1.f)
    {
        return false;
    }
    Reaction* r = reactionOf(e);
    if(!r)
    {
        return false;
    }
    glm::vec3 total[ragdoll::maxBones];
    for(glm::vec3& t : total)
    {
        t = glm::vec3{0.f};
    }
    bool any = false;
    for(int i = 0; i < r->count; i++)
    {
        Kick& k = r->kicks[i];
        if(k.part == guardPart)
        {
            // (A guard: posed afresh each frame, its arms to its face as it is drawn now.)
            const float w = guardEnvelope(static_cast<float>(cl.time - k.start), k.hold);
            if(w > 0.f)
            {
                guardTurns(e, rig, posed, k, w, total);
                any = true;
            }
            continue;
        }
        if(!k.resolved)
        {
            k.resolved = true;
            k.model = e->model;
            k.rigged = true;
            resolveRig(k, e, rig, posed);
            if(vr_hit_react_debug.value >= 1.f)
            {
                Con_Printf("hitreact: ent %d struck on %s (part %d), %d bones turned\n", client::entityNumber(e),
                    k.hitBone >= 0 ? rig.bones[k.hitBone].name : "-", k.part, __builtin_popcount(k.bones));
            }
        }
        if(k.model != e->model || !k.rigged)
        {
            continue;
        }
        const float env = envelope(static_cast<float>(cl.time - k.start));
        if(env == 0.f)
        {
            continue;
        }
        for(int b = 0; b < rig.numBones; b++)
        {
            if(k.bones & (1u << b))
            {
                total[b] += env * (posed[b].rot * k.turn[b]);
                any = true;
            }
        }
    }
    if(!any)
    {
        return false;
    }
    traceLine(e, *r, &rig, total, 0.f);
    // Each bone's turn about its joint as posed, carried by its parent's; a loose piece (the grunt's shotgun) goes with
    // the bone nearest it
    for(int b = 0; b < rig.numBones; b++)
    {
        const ragdoll::Bone& bone = rig.bones[b];
        out[b] = rigpose::Rigid{};
        if(bone.joint == ragdoll::Joint::Loose)
        {
            continue;
        }
        if(bone.parent >= 0 && bone.parent < b && rig.bones[bone.parent].joint != ragdoll::Joint::Loose)
        {
            out[b] = out[bone.parent];
        }
        if(glm::dot(total[b], total[b]) > 1e-12f)
        {
            const glm::vec3 j = rigpose::apply(posed[b], bone.pivot);
            rigpose::Rigid turn;
            turn.rot = rigpose::fromVector(total[b]);
            turn.pos = j - turn.rot * j;
            out[b] = composeRigid(out[b], turn);
        }
    }
    for(int b = 0; b < rig.numBones; b++)
    {
        if(rig.bones[b].joint != ragdoll::Joint::Loose)
        {
            continue;
        }
        const glm::vec3 p = rigpose::apply(posed[b], rig.bones[b].pivot);
        const int follow = nearestBone(rig, posed, p, -1);
        out[b] = follow >= 0 ? out[follow] : rigpose::Rigid{};
    }
    return true;
}

void tiltModelNow(const entity_t* e, float matrix[16])
{
    Reaction* r = reactionOf(e);
    if(!r || (vr_hit_react.value == 1.f && animsmooth::rigged(e)))
    {
        return;
    }
    glm::vec3 total{0.f};
    for(int i = 0; i < r->count; i++)
    {
        Kick& k = r->kicks[i];
        if(k.part == guardPart)
        {
            continue; // (a guard needs arms: rigged models only)
        }
        if(!k.resolved)
        {
            k.resolved = true;
            k.model = e->model;
            k.rigged = false;
            resolveTilt(k, e);
        }
        if(k.model != e->model || k.rigged)
        {
            continue;
        }
        total += envelope(static_cast<float>(cl.time - k.start)) * k.tilt;
    }
    const float angle = za::sqrt(glm::dot(total, total));
    if(angle < 1e-6f)
    {
        return;
    }
    traceLine(e, *r, nullptr, nullptr, angle * (180.f / 3.14159265f));
    const glm::vec3 feet{0.f, 0.f, e->model->mins[2]};
    const glm::mat4 turn = glm::mat4_cast(rigpose::fromVector(total));
    glm::mat4 m = glm::make_mat4(matrix);
    m = m * glm::translate(glm::mat4{1.f}, feet) * turn * glm::translate(glm::mat4{1.f}, -feet);
    memcpy(matrix, glm::value_ptr(m), sizeof(float) * 16);
}

bool boneKicks(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, rigpose::Rigid* out)
{
    const long long t0 = za::Clock::nowNanoseconds();
    const bool on = boneKicksNow(e, rig, posed, out);
    countCost(za::Clock::nowNanoseconds() - t0);
    return on;
}

bool reacting(const entity_t* e)
{
    return reactionOf(e) != nullptr;
}

bool armsWanted(const entity_t* e)
{
    const bool want = vr_agrunt_ik.value != 0.f && e && e->model && e->model->type == mod_alias && client::entityNumber(e) > 0 &&
                      armedRigOf(e->model) != nullptr;
    if(!want && vr_agrunt_ik_debug.value >= 2.f && e && e->model && strstr(e->model->name, "vrgrunt"))
    {
        Con_Printf("agik ent %d: not wanted (%s)\n", client::entityNumber(e), e->model->name);
    }
    return want;
}

bool bonesWanted(const entity_t* e)
{
    return (vr_hit_react.value == 1.f && reacting(e)) || armsWanted(e);
}

bool boneTurns(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, rigpose::Rigid* out)
{
    if(!armedRigOf(e->model))
    {
        return reacting(e) && boneKicks(e, rig, posed, out);
    }
    // vr_agarms: the arms are the skin's (its rows after these: kickInstance); the bones only kicked.
    const bool kicked = reacting(e) && boneKicks(e, rig, posed, out);
    if(!kicked)
    {
        for(int b = 0; b < rig.numBones; b++)
        {
            out[b] = rigpose::Rigid{};
        }
    }
    // (a take's kneel: the body carried by its pelvis, vr_aglegs; the upper body on it; a strike's body: bodyTurn; as
    // relinkSkinned solved the arms on them)
    rigpose::Rigid legs[ragdoll::maxBones], posedL[ragdoll::maxBones];
    const bool legged = aglegs::carry(e, rig, posed, legs);
    for(int b = 0; b < rig.numBones; b++)
    {
        posedL[b] = legged ? composeRigid(legs[b], posed[b]) : posed[b];
    }
    rigpose::Rigid leanTurn; // (the strafe lean on the legs': aglegs::lean, as relinkSkinned laid it)
    const bool leaned = aglegs::leanNow(e, leanTurn);
    for(int b = 0; b < rig.numBones && leaned; b++)
    {
        posedL[b] = composeRigid(leanTurn, posedL[b]);
    }
    rigpose::Rigid upperCorr;
    const bool uppered = agarms::active(e) && agbody::upperNow(client::entityNumber(e), upperCorr);
    rigpose::Rigid lay[ragdoll::maxBones];
    for(int b = 0; b < rig.numBones; b++)
    {
        lay[b] = rigpose::Rigid{};
    }
    if(uppered)
    {
        layOnChest(rig, chestOf(rig), upperCorr, lay);
        layOnChest(rig, chestOf(rig), upperCorr, posedL);
    }
    rigpose::Rigid body[ragdoll::maxBones];
    const ArmPose* ap = armPoseNow(e);
    const bool bodied = ap && agarms::active(e) && bodyTurn(rig, posedL, *ap, body);
    for(int b = 0; b < rig.numBones && bodied; b++)
    {
        out[b] = composeRigid(out[b], body[b]);
    }
    // (each bone's drawn turn: the kick's, the strike's, the upper body's, the legs' on the frames)
    for(int b = 0; b < rig.numBones && uppered; b++)
    {
        out[b] = composeRigid(out[b], lay[b]);
    }
    for(int b = 0; b < rig.numBones && leaned; b++)
    {
        out[b] = composeRigid(out[b], leanTurn);
    }
    for(int b = 0; b < rig.numBones && legged; b++)
    {
        out[b] = composeRigid(out[b], legs[b]);
    }
    return kicked || bodied || legged || leaned || uppered || agarms::active(e);
}

bool armSkinRow(const qmodel_t* model, const void* aliashdr, const ragdoll::Rig& rig, int v, glm::vec4& out)
{
    out = glm::vec4{0.f};
    const ArmedRig* a = armedRigOf(model);
    agarms::Spec spec;
    return a && specOf(*a, rig, static_cast<const aliashdr_t*>(aliashdr), spec) &&
           agarms::restRow(model, static_cast<const aliashdr_t*>(aliashdr), rig, spec, v, out);
}
void tiltModel(const entity_t* e, float matrix[16])
{
    const long long t0 = za::Clock::nowNanoseconds();
    tiltModelNow(e, matrix);
    countCost(za::Clock::nowNanoseconds() - t0);
}

namespace
{

// vr_hit_react_info: the entities reacting now and their kicks; the drawing's cost since the last time.
void info_f()
{
    Con_Printf("hit reactions: cost %.4f ms a frame over %d frames (%d calls, %.2f us each)\n",
        cost.frames ? static_cast<double>(cost.ns) * 1e-6 / cost.frames : 0.0, cost.frames, cost.calls,
        cost.calls ? static_cast<double>(cost.ns) * 1e-3 / cost.calls : 0.0);
    cost = Cost{};
    Con_Printf("hit reactions: %d entities (vr_hit_react %g, strength %g, time %g)\n", static_cast<int>(reacts.entities.size()),
        vr_hit_react.value, vr_hit_react_strength.value, vr_hit_react_time.value);
    for(const auto& [num, r] : reacts.entities)
    {
        for(int i = 0; i < r.count; i++)
        {
            const Kick& k = r.kicks[i];
            Con_Printf("  ent %d: age %.2f s, part %d, size %.2f, %s, bones %x\n", num, cl.time - k.start, k.part, k.size,
                !k.resolved ? "not drawn yet" : k.rigged ? "bones" : "tilt", k.bones);
        }
    }
}

// vr_agholds_report (Debug > Armed Humanoids: Weapon Holds Report; ROUND22.md, "Armed humanoids' weapon holds"): each
// armed humanoid whose arms were solved in the last half second: `aghold <ent> <gru|enf> <weapon model> s<slot>: <hands:
// 1, 2; 0 dual> off <grip (held) | grip-unreached | cup | point | weapon | free> [ask1 | ask2: the Weapon Holds asked]
// at <its second grip's point, the weapon's frame>`, then `agholds: <n> armed humanoids, <one> one-handed, <two>
// two-handed, <dual> dual`.
void holdsReport_f()
{
    int n = 0, counts[3] = {0, 0, 0};
    for(const ArmPose& s : armPoses.slots)
    {
        if(s.ent <= 0 || s.ent >= cl.num_entities || s.seenFrame < 0 || host_framecount - s.seenFrame > 36)
        {
            continue;
        }
        const entity_t* p = s.prop > 0 && s.prop < cl.num_entities ? &cl_entities[s.prop] : nullptr;
        const bool two = (s.seenLeft == 1 && s.seenHeld2) || s.seenLeft == 2;
        const int c = s.seenLeft == 4 ? 2 : two ? 1 : 0;
        counts[c]++;
        n++;
        // (short: the console wraps its lines)
        const char* left = s.seenLeft == 1 ? (s.seenHeld2 ? "grip" : "grip-unreached") : s.seenLeft == 2 ? "cup"
                           : s.seenLeft == 3                       ? "point"
                           : s.seenLeft == 4                       ? "weapon"
                                                                   : "free";
        const char* name = p && p->model ? p->model->name : "fists";
        const char* base = strrchr(name, '/');
        Con_Printf("aghold %d %s %s s%d: %d off %s%s at %.1f %.1f %.1f\n", s.ent, s.seenKind == 1 ? "enf" : "gru",
            base ? base + 1 : name, s.seenSlot, c == 2 ? 0 : two ? 2 : 1, left,
            (s.flags & apLeftFree) ? " ask1" : (s.flags & apTwo) ? " ask2" : "", s.seenSecond.x, s.seenSecond.y,
            s.seenSecond.z);
        glm::vec3 muzzle, butt, second;
        int type = 0;
        if(vr_agrunt_ik_debug.value >= 1.f && p && view::weaponLocalGeometry(p->model, muzzle, butt, second, type))
        {
            entity_t at{};
            at.model = p->model;
            at.scale = ENTSCALE_DEFAULT;
            const glm::vec3 two = view::anchorPosition(at, false, 1.f, weapons::anchor(s.seenSlot, weapons::Key::TwoHHandAnchor),
                glm::vec3{0.f});
            Con_Printf("aghold %d: muzzle %.1f %.1f %.1f butt %.1f %.1f %.1f second %.1f %.1f %.1f (%d) 2h %.1f %.1f %.1f\n",
                s.ent, muzzle.x, muzzle.y, muzzle.z, butt.x, butt.y, butt.z, second.x, second.y, second.z, type, two.x, two.y,
                two.z);
        }
    }
    Con_Printf("agholds: %d armed humanoids, %d one-handed, %d two-handed, %d dual\n", n, counts[0], counts[1], counts[2]);
}

// vr_agrunt_ik_err: the arms' numbers since the last time (vr_agarms.cpp printErr: counted afresh after), how far the held
// weapons were drawn from their holders, and the ragdolls' weapons against their drawn hands.
void armErr_f()
{
    agarms::printErr();
    agarms::clearErr();
    Con_Printf("agprop: held weapons drawn at most %.1f units from their holders\n", ragErr.propFar);
    ragErr.propFar = 0.f;
    if(ragErr.n > 0)
    {
        Con_Printf("agrag: %d ragdoll frames: the server's weapon off its drawn hand's grip max %.2f mean %.2f units, turned "
                   "%.0f deg (drawn on the hand instead)\n",
            ragErr.n, ragErr.offMax, ragErr.offSum / static_cast<float>(ragErr.n), ragErr.turnMax);
        ragErr = RagErr{};
    }
}

} // namespace
// vr_agrunt_ragweapon_err (vr_agrunt_ragweapon_check's numbers): `clear` forgets them; `<label>` prints them since the
// last (a case of vr_agrunt_test 88) and counts it passed (grip within vr_agrunt_ragweapon_most units and 12 degrees,
// hand and weapon within it of the torso's surface, some frames measured) or failed; `done` prints the cases' verdict.
namespace
{
struct RagWeaponTally
{
    int passed{0}, failed{0};
};
RagWeaponTally ragWeaponTally;

void ragWeaponErr_f()
{
    const char* what = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!strcmp(what, "clear"))
    {
        ragWeaponErr = RagWeaponErr{};
        return;
    }
    if(!strcmp(what, "done"))
    {
        qvr::testkit::verdict("ragweapon", ragWeaponTally.passed, ragWeaponTally.failed);
        ragWeaponTally = RagWeaponTally{};
        return;
    }
    const RagWeaponErr& r = ragWeaponErr;
    const float most = Cmd_Argc() > 2 ? static_cast<float>(atof(Cmd_Argv(2))) : 1.5f;
    const bool ok = r.n > 0 && r.gripMax <= most && r.turnMax <= 12.f && r.handIn <= most && r.weaponIn <= most;
    Con_Printf("ragweapon %s: %d frames, weapon grip off its hand max %.2f mean %.2f units, turned %.1f deg; in the torso: "
               "hand %.2f, weapon %.2f units: %s\n",
        what, r.n, r.gripMax, r.n ? r.gripSum / static_cast<float>(r.n) : 0.f, r.turnMax, r.handIn, r.weaponIn,
        ok ? "PASS" : "FAIL");
    (ok ? ragWeaponTally.passed : ragWeaponTally.failed)++;
    ragWeaponErr = RagWeaponErr{};
}
} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_agrunt_ragweapon_err", ragWeaponErr_f);
    Cmd_AddCommand("vr_hit_react_info", info_f);
    Cmd_AddCommand("vr_agrunt_ik_err", armErr_f);
    Cmd_AddCommand("vr_agholds_report", holdsReport_f);
    aggrip::registerCommands();
    agarms::registerCommands();
    aglegs::registerCommands();
}

} // namespace qvr::hitreact
