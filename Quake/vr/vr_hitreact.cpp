// vr_hitreact.cpp -- living hit reactions (vr_hitreact.hpp; ROUND22.md, "Active ragdolls: living hit reactions").

#include "vr_hitreact.hpp"
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

// The number of a client entity (-1: not one of cl_entities: your gun, a temporary entity).
[[nodiscard]] int numberOf(const entity_t* e)
{
    if(!e || !cl_entities || e < cl_entities || e >= cl_entities + cl.num_entities)
    {
        return -1;
    }
    return static_cast<int>(e - cl_entities);
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

[[nodiscard]] float segmentDistance(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b)
{
    const glm::vec3 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    const float t = len2 > 1e-6f ? za::clamp(glm::dot(p - a, ab) / len2, 0.f, 1.f) : 0.f;
    const glm::vec3 d = p - (a + t * ab);
    return za::sqrt(glm::dot(d, d));
}

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
    const int num = numberOf(e);
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

namespace
{

// a ∘ b: b first
rigpose::Rigid composeRigid(const rigpose::Rigid& a, const rigpose::Rigid& b)
{
    rigpose::Rigid r;
    r.rot = a.rot * b.rot;
    r.pos = a.rot * b.pos + a.pos;
    return r;
}

void traceLine(const entity_t* e, Reaction& r, const ragdoll::Rig* rig, const glm::vec3* total, float tiltDeg)
{
    if(vr_hit_react_debug.value < 2.f || r.traced == cl.time)
    {
        return;
    }
    r.traced = cl.time;
    char line[512];
    int n = snprintf(line, sizeof(line), "hittrace %d t %.3f kicks %d", numberOf(e), cl.time, r.count);
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

// ---------------------------------------------------------------------------------------------------------------------
// Armed monsters' arms on their weapon (armsWanted; QC vr_armedgrunt.qc, vr_armedmon.qc)

constexpr int heldEffect = 128;                // QC VR_AG_EF_HELD: a weapon in a monster's hand
constexpr float propReach = 28.f;              // units: its weapon is the held one nearest its right hand within this

// The armed monsters' table (QC vr_armedmon.qc VR_AM_Row; docs/vr-port/ARMED_MONSTERS.md): the model it is drawn with
// (its gun's triangles dropped: Misc/quakevr/make_vrgrunt.py), its right hand's vertices (where the server puts its
// weapon's grip), its hidden gun's vertices (never a hand's) and their line (the carry's turn), its arms' bones.
struct VertRange
{
    int first;
    int last;
};
struct ArmedRig
{
    modelmeta::Id model;
    int numVerts;
    int rightHand[3];
    VertRange gun[4];
    int numGun;
    int gunRear[2];
    int gunMuzzle[2];
    int gunTop;
    int gunBottom;
    const char* upperR;
    const char* foreR;
    const char* upperL;
    const char* foreL;
    int anchor; // vr_agarms: its chest's vertex the poses are placed from (QC vr_am_anchor; vr_agarms_info finds it)
    float torsoSides; // vr_agarms: its torso proxy's sides and back fitted from this share of the chest from the waist
    float twistShare; // ... the hand's twist about the forearm the forearm takes
    float wristMore;  // ... degrees its wrist bends past vr_agarms_wrist_most
};
// (the enforcer's: its torso's sides from its lower 0.6, under its shoulder pads; its wrist 20 degrees more, its guns
// held turned further on its long forearms; its twist share as the grunt's, 0.75 left elbows stuck the wrong way:
// ROUND22.md, "The enforcer's rig fixed")
constexpr ArmedRig armedRigs[] = {
    {modelmeta::Id::VrGrunt, 555, {32, 26, 31}, {{463, 548}}, 1, {497, 545}, {498, 514}, 529, 496, "upperarm_r", "forearm_r",
        "upperarm_l", "forearm_l", 148, 1.f, 0.5f, 0.f},
    {modelmeta::Id::VrEnforcer, 479, {41, 364, 365}, {{22, 23}, {100, 100}, {400, 430}, {455, 478}}, 4, {425, 420},
        {407, 100}, 409, 412, "upperarm_r", "forearm_r", "upperarm_l", "forearm_l", 322, 0.6f, 0.5f, 20.f},
};
constexpr int numArmedRigs = static_cast<int>(sizeof(armedRigs) / sizeof(armedRigs[0]));

// `model`'s row (nullptr: not an armed monster's model).
[[nodiscard]] const ArmedRig* armedRigOf(const qmodel_t* model)
{
    if(!model || model->type != mod_alias)
    {
        return nullptr;
    }
    const modelmeta::ModelMetadata& meta = modelmeta::get(model);
    for(const ArmedRig& a : armedRigs)
    {
        if(meta.is(a.model))
        {
            return &a;
        }
    }
    return nullptr;
}

[[nodiscard]] bool isGunVertex(const ArmedRig& a, int v)
{
    for(int i = 0; i < a.numGun; i++)
    {
        if(v >= a.gun[i].first && v <= a.gun[i].last)
        {
            return true;
        }
    }
    return false;
}
constexpr int maxProps = 32;
constexpr int maxHandVerts = 16;
constexpr float hangReach = 0.95f;             // a free arm's hand this share of its reach below its shoulder
constexpr glm::vec3 hangDir{0.15f, 0.12f, -1.f}; // ... that way (model space: a little ahead and out, its left)
constexpr glm::vec3 hangElbow{-1.f, 0.3f, 0.f};  // ... its elbow back
constexpr glm::vec3 cupOffset{0.f, 1.2f, -2.5f}; // a Cup grip: the left hand under and round the right (model units)

struct LeftHand
{
    const qmodel_t* model{nullptr}; // the model its vertices were found on
    int verts[maxHandVerts]{};
    int num{0};
};

struct ArmIk
{
    int frame{-1};                  // host_framecount the held weapons were gathered on
    int numProps{0};
    int props[maxProps]{};          // their cl_entities numbers
    LeftHand left[numArmedRigs];    // each row's left hand
    LeftHand right[numArmedRigs];   // ... its right hand about its forearm's end (the drawn hand: hitreact::HandFit's)
    bool forcePrint{false};          // (vr_agrunt_ik_info: printed whatever vr_agrunt_ik_debug)
};
ArmIk armIk;

[[nodiscard]] glm::vec3 toWorld(const entity_t* e, const glm::vec3& local)
{
    float yawDeg = 0.f;
    const glm::vec3 o = drawnOrigin(e, &yawDeg);
    const float yaw = yawDeg * (3.14159265f / 180.f);
    const float c = za::cos(yaw), s = za::sin(yaw);
    const glm::vec3 d = local * za::max(ENTSCALE_DECODE(e->scale), 0.01f);
    return o + glm::vec3{c * d.x - s * d.y, s * d.x + c * d.y, d.z};
}

// Vertex `v` of the poses drawn (posed model space, units).
[[nodiscard]] glm::vec3 poseVertex(const aliashdr_t* hdr, int pose1, int pose2, float blend, int v)
{
    const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const trivertx_t& a = base[static_cast<za::SizeT>(pose1) * static_cast<za::SizeT>(hdr->numverts) + static_cast<za::SizeT>(v)];
    const trivertx_t& b = base[static_cast<za::SizeT>(pose2) * static_cast<za::SizeT>(hdr->numverts) + static_cast<za::SizeT>(v)];
    const glm::vec3 scale{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
    const glm::vec3 origin{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
    const glm::vec3 pa = glm::vec3{a.v[0], a.v[1], a.v[2]} * scale + origin;
    const glm::vec3 pb = glm::vec3{b.v[0], b.v[1], b.v[2]} * scale + origin;
    return pa + (pb - pa) * blend;
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

// The weapons held by monsters this frame (the held bit), gathered once a frame.
void gatherProps()
{
    if(armIk.frame == host_framecount)
    {
        return;
    }
    armIk.frame = host_framecount;
    armIk.numProps = 0;
    for(int i = 1; i < cl.num_entities && armIk.numProps < maxProps; i++)
    {
        const entity_t& p = cl_entities[i];
        if(p.model && p.model->type == mod_alias && (p.effects & heldEffect))
        {
            armIk.props[armIk.numProps++] = i;
        }
    }
}

// Its left hand: the vertices about the left forearm's far end (its seed's end: the hand), within 3 units of it in its
// first pose (none: the 4 nearest), found once a model (each row's kept).
// (`right`: the right forearm's, `fl` its bone: the drawn right hand, its row's three vertices being the old rifle grip's,
// fitted badly by its bone: they take a sixth of its turns.)
const LeftHand& findLeftHand(const ArmedRig& a, const qmodel_t* model, const ragdoll::Rig& rig, const aliashdr_t* hdr, int fl,
    bool right = false)
{
    LeftHand& lh = right ? armIk.right[&a - armedRigs] : armIk.left[&a - armedRigs];
    if(lh.model == model)
    {
        return lh;
    }
    lh.model = model;
    lh.num = 0;
    const glm::vec3 end = rigpose::apply(rigpose::key(rig, 0, fl), rig.bones[fl].end);
    for(float within = 3.f; within < 12.f && lh.num < 4; within += 1.f)
    {
        lh.num = 0;
        for(int v = 0; v < hdr->numverts; v++)
        {
            if(isGunVertex(a, v) || lh.num >= maxHandVerts)
            {
                continue;
            }
            if(glm::length(poseVertex(hdr, 0, 0, 0.f, v) - end) <= within)
            {
                lh.verts[lh.num++] = v;
            }
        }
    }
    return lh;
}

// Bends an arm (shoulder `S`, elbow `E`, hand point `H` on the forearm, posed) so H reaches `T` (reach allowing: within
// `most` of the arm's length, the elbow bent at most `bendMost` degrees), its elbow towards `poleHint` (across the
// shoulder-target line): the upper arm's turn and the forearm's (both absolute, model space). False: a segment too short
// (the turns then none).
bool solveArmPoints(const glm::vec3& S, const glm::vec3& E, const glm::vec3& H, const glm::vec3& T, const glm::vec3& poleHint,
    float most, float bendMost, rigpose::Rigid& turnU, rigpose::Rigid& turnFore)
{
    turnU = rigpose::Rigid{};
    turnFore = rigpose::Rigid{};
    const float a = glm::length(E - S), b = glm::length(H - E);
    if(a < 0.5f || b < 0.5f)
    {
        return false;
    }
    const glm::vec3 d = T - S;
    const float dl = za::max(glm::length(d), 1e-3f);
    const glm::vec3 dir = d / dl;
    const float bend = za::clamp(bendMost, 0.f, 179.f) * (3.14159265f / 180.f);
    const float shortest = za::sqrt(za::max(0.f, a * a + b * b - 2.f * a * b * za::cos(3.14159265f - bend)));
    const float dist = za::clamp(dl, za::max(za::abs(a - b) + 0.01f, shortest), za::clamp(most, 0.1f, 0.999f) * (a + b));
    const float cosA = za::clamp((a * a + dist * dist - b * b) / (2.f * a * dist), -1.f, 1.f);
    const float sinA = za::sqrt(za::max(0.f, 1.f - cosA * cosA));
    glm::vec3 pole = poleHint - dir * glm::dot(poleHint, dir);
    if(glm::length(pole) < 1e-3f)
    {
        pole = glm::vec3{0.f, 0.f, -1.f} - dir * glm::dot(glm::vec3{0.f, 0.f, -1.f}, dir);
    }
    pole = glm::length(pole) > 1e-4f ? glm::normalize(pole) : glm::vec3{0.f, 0.f, -1.f};
    const glm::vec3 E2 = S + a * (cosA * dir + sinA * pole);
    const glm::vec3 H2 = S + dir * dist;
    turnU.rot = rigpose::fromVector(arcTurn(glm::normalize(E - S), glm::normalize(E2 - S)));
    turnU.pos = S - turnU.rot * S;
    const glm::vec3 back = glm::inverse(turnU.rot) * (H2 - E2);
    rigpose::Rigid turnF;
    turnF.rot = rigpose::fromVector(arcTurn(glm::normalize(H - E), glm::normalize(back)));
    turnF.pos = E - turnF.rot * E;
    turnFore = composeRigid(turnU, turnF);
    return true;
}

// Bends the arm (upper arm `u`, forearm `lo`) so its hand point `H` (posed, on the forearm) reaches `T` (reach allowing),
// its elbow towards `poleHint` (across the shoulder-target line): out[u], out[lo]. The hand's miss after (units).
float solveArm(const ragdoll::Rig& rig, const rigpose::Rigid* posed, int u, int lo, const glm::vec3& H, const glm::vec3& T,
    const glm::vec3& poleHint, rigpose::Rigid* out)
{
    const glm::vec3 S = rigpose::apply(posed[u], rig.bones[u].pivot);
    const glm::vec3 E = rigpose::apply(posed[lo], rig.bones[lo].pivot);
    const float a = glm::length(E - S), b = glm::length(H - E);
    if(a < 0.5f || b < 0.5f)
    {
        return glm::length(H - T);
    }
    const glm::vec3 d = T - S;
    const float dl = za::max(glm::length(d), 1e-3f);
    const glm::vec3 dir = d / dl;
    const float dist = za::clamp(dl, za::abs(a - b) + 0.01f, 0.999f * (a + b));
    const float cosA = za::clamp((a * a + dist * dist - b * b) / (2.f * a * dist), -1.f, 1.f);
    const float sinA = za::sqrt(za::max(0.f, 1.f - cosA * cosA));
    glm::vec3 pole = poleHint - dir * glm::dot(poleHint, dir);
    if(glm::length(pole) < 1e-3f)
    {
        pole = glm::vec3{0.f, 0.f, -1.f} - dir * glm::dot(glm::vec3{0.f, 0.f, -1.f}, dir);
    }
    pole = glm::length(pole) > 1e-4f ? glm::normalize(pole) : glm::vec3{0.f, 0.f, -1.f};
    const glm::vec3 E2 = S + a * (cosA * dir + sinA * pole);
    const glm::vec3 H2 = S + dir * dist;
    rigpose::Rigid turnU;
    turnU.rot = rigpose::fromVector(arcTurn(glm::normalize(E - S), glm::normalize(E2 - S)));
    turnU.pos = S - turnU.rot * S;
    const glm::vec3 back = glm::inverse(turnU.rot) * (H2 - E2);
    rigpose::Rigid turnF;
    turnF.rot = rigpose::fromVector(arcTurn(glm::normalize(H - E), glm::normalize(back)));
    turnF.pos = E - turnF.rot * E;
    out[u] = turnU;
    out[lo] = composeRigid(turnU, turnF);
    return glm::length(rigpose::apply(out[lo], H) - T);
}

// ---------------------------------------------------------------------------------------------------------------------
// Armed grunts' arm poses (QVR_SVC_ARMPOSE, QC VR_AG_SendPose; ROUND22.md, "Armed grunts: presentation"): where the server
// wants its hands and its weapon, relative to its drawn body and its drawn right hand, every server frame. The client puts
// them on the body as drawn now (its step lerp, its poses' lerp), between the last two messages: the weapon drawn there
// (the held entity's origin and angles replaced after the relink; the server's own is for the game) and the hands on it.

constexpr int maxArmPoses = 64;
constexpr double armPoseStale = 0.3; // seconds without a message: back to finding its weapon by its hand
constexpr int apLeft = 2;            // the left hand to its own target (else on the weapon's second grip, or free)
constexpr int apLeftFree = 4;        // ... the left as the animation has it
constexpr int apLeftWeapon = 8;      // ... the left holding its own weapon at its target (dual wielding: propL, its turn)
constexpr int apAnchor = 64;         // vr_agarms: wr and wl weigh its chest's anchor vertex as drawn, not its hands (QC VR_AGP_ANCHOR)
constexpr int apBody = 32;           // its body's turns in a strike follow (torso, elbows: bodyTurn)
constexpr int apFixed = 1;           // ... its hands' targets not turned with them (a take's bend: QC VR_AGP_FIXED)
constexpr int apReach = 128;         // vr_agarms: the right hand reaching for a weapon lying (none sent): its grip to fw, up (QC VR_AGP_REACH)
constexpr int apTwo = 16;            // the Weapon Holds' two-handed hold: a weapon with no second grip cupped (QC VR_AGP_TWO); apLeftFree its one-handed (VR_AGP_FREE)
constexpr float handEase = 0.04f;    // seconds: the animation's hand followed so (its jumps smoothed)

struct ArmPoseMsg
{
    double t{-1.0}; // cl.mtime[0] it came with
    int prop{0};
    int flags{0};
    float wr{1.f};  // the right hand's target: wr * the drawn right hand + v (its body's space, units)
    glm::vec3 v{0.f};
    glm::vec3 fwB{1.f, 0.f, 0.f}, upB{0.f, 0.f, 1.f}; // the weapon's turn in its body's space,
    glm::vec3 fwG{1.f, 0.f, 0.f}, upG{0.f, 0.f, 1.f}; // ... in its drawn rifle line's,
    float wo{1.f};                                     // ... the second's share
    float wl{0.f};  // the left hand's target: wl * the drawn left hand + vl
    glm::vec3 vl{0.f};
    int propL{0};   // apLeftWeapon: the left hand's weapon, its turn in its body's space
    glm::vec3 fwL{1.f, 0.f, 0.f}, upL{0.f, 0.f, 1.f};
    glm::vec3 torso{0.f};                  // apBody: its torso's turn in a strike (yaw, pitch, roll; degrees)
    glm::vec3 elbowR{0.f}, elbowL{0.f};    // ... its elbows' poles (its body's space; their length the share)
    glm::vec3 waist{0.f};                  // ... the point QC turned its targets about (its body's space, units)
    float take{0.f};                       // apFixed: a take's weight (vr_aglegs: 0 standing .. 1 at its lowest)
    int takeSide{0};                       // ... its reaching hand (0 right, 1 left)
    // The pose keys' off hand (QC's off-hand block, told by apLeftFree and apTwo both set: agpose GripPos/GripTurn,
    // LPos/LTurn): its grip on the right weapon moved and turned (the weapon's frame), the free hand's (its body's space).
    bool offKeyed{false};
    bool keyGun{false}; // its key's Off Hand On the Gun (the block's 32): cupped even with a pistol carried low
    glm::vec3 gripPos{0.f}, gripTurn{0.f};
    glm::vec3 freePos{0.f}, freeTurn{0.f};
    float stretch{1.f}; // its kind and class's IK stretch (agpose::stretch; the block's 16)
};

struct ArmPose
{
    int ent{0};
    ArmPoseMsg prev, cur;
    int frame{-1};   // host_framecount its targets were placed on
    int prop{0};
    int flags{0};
    glm::vec3 hand{0.f};                         // the animation's right hand, eased (handEase)
    double handTime{0.0};
    glm::vec3 right{0.f}, left{0.f};             // the hands' targets (model space, units)
    glm::vec3 fw{1.f, 0.f, 0.f}, up{0.f, 0.f, 1.f}; // the weapon's turn (model space)
    int propL{0};                                // the left hand's weapon (apLeftWeapon), drawn at `left`
    glm::vec3 fwL{1.f, 0.f, 0.f}, upL{0.f, 0.f, 1.f};
    glm::vec3 torso{0.f};                        // its torso's turn now (degrees: bodyTurn), its elbows' poles
    glm::vec3 elbowR{0.f}, elbowL{0.f};
    glm::vec3 waist{0.f};                        // (model space: the message's over its scale)
    glm::vec3 anchorAt{0.f};                     // vr_agarms: its chest's anchor as the frames draw it (model space; apAnchor),
    float wr{0.f}, wl{0.f};                      // ... its weight in the right and left targets (the upper body moves them with it)
    float take{0.f};                             // a take's weight, its reaching hand (vr_aglegs)
    int takeSide{0};
    bool offKeyed{false};                        // the pose keys' off hand (ArmPoseMsg's; the free hand's in model space)
    bool keyGun{false};                          // its key's Off Hand On the Gun (ArmPoseMsg's; in offKeyed's padding)
    glm::vec3 gripPos{0.f}, gripTurn{0.f}, freePos{0.f}, freeTurn{0.f};
    float stretch{1.f};                          // its IK stretch (ArmPoseMsg's: the arms' Targets::stretch)
    // vr_agholds_report: as its arms were last solved (vr_agarms): its kind, its weapon's slot, its off hand's goal (0
    // free, 1 the second grip, 2 cupping the right hand, 3 its own point, 4 its own weapon), the grip held, its point (the
    // weapon's frame, units).
    int seenFrame{-1};
    int seenKind{-1};
    int seenSlot{-1};
    int seenLeft{0};
    bool seenHeld2{false};
    glm::vec3 seenSecond{0.f};
};

struct ArmPoses
{
    ArmPose slots[maxArmPoses];
};
ArmPoses armPoses;

[[nodiscard]] ArmPose* armPoseOf(int ent, bool make)
{
    ArmPose* free = nullptr;
    for(ArmPose& s : armPoses.slots)
    {
        if(s.ent == ent)
        {
            return &s;
        }
        if(!free && (s.ent == 0 || s.cur.t < cl.mtime[0] - armPoseStale || s.cur.t > cl.mtime[0] + 1.0))
        {
            free = &s;
        }
    }
    if(!make || !free)
    {
        return nullptr;
    }
    *free = ArmPose{};
    free->ent = ent;
    return free;
}

[[nodiscard]] glm::vec3 nlerp(const glm::vec3& a, const glm::vec3& b, float t)
{
    const glm::vec3 m = a + (b - a) * t;
    return glm::length(m) > 1e-4f ? glm::normalize(m) : b;
}

// The poses it is drawn between now: R_SetupAliasFrame's lerp read without changing it (a frame begun this frame: the
// pose it leaves, as it will be drawn).
void drawnPoses(const entity_t* e, const aliashdr_t* hdr, int& pose1, int& pose2, float& blend)
{
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

// The entity angles (a model's pitch) that turn a weapon model's forward to `fw` and its up to `up` (QC VR_AxeStick_Angles).
[[nodiscard]] glm::vec3 weaponAngles(const glm::vec3& fw, const glm::vec3& up)
{
    vec3_t a, f, r, u;
    a[YAW] = za::atan2(fw.y, fw.x) * (180.f / 3.14159265f);
    a[PITCH] = -za::atan2(fw.z, za::sqrt(fw.x * fw.x + fw.y * fw.y)) * (180.f / 3.14159265f);
    a[ROLL] = 0.f;
    AngleVectors(a, f, r, u);
    const glm::vec3 u0{u[0], u[1], u[2]};
    const float roll = za::atan2(glm::dot(glm::cross(u0, up), fw), glm::dot(u0, up)) * (180.f / 3.14159265f);
    a[ROLL] = roll;
    AngleVectors(a, f, r, u);
    if(glm::dot(glm::vec3{u[0], u[1], u[2]}, up) < 0.999f)
    {
        a[ROLL] = -roll;
    }
    return {-a[PITCH], a[YAW], a[ROLL]};
}

[[nodiscard]] glm::vec3 turnYaw(float yawDeg, const glm::vec3& d)
{
    const float yaw = yawDeg * (3.14159265f / 180.f);
    const float c = za::cos(yaw), s = za::sin(yaw);
    return {c * d.x - s * d.y, s * d.x + c * d.y, d.z};
}

// The turn `corr` laid on every bone on rig's `chest` (the chest too; its loose pieces not), on the left (rest -> now).
void layOnChest(const ragdoll::Rig& rig, int chest, const rigpose::Rigid& corr, rigpose::Rigid* bones)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        bool onChest = b == chest;
        for(int p = rig.bones[b].parent, n = 0; p >= 0 && n < ragdoll::maxBones && !onChest; p = rig.bones[p].parent, n++)
        {
            onChest = p == chest;
        }
        if(onChest && rig.bones[b].joint != ragdoll::Joint::Loose)
        {
            bones[b] = composeRigid(corr, bones[b]);
        }
    }
}

// Its arm pose's targets now (`s` fresh), on the poses drawn: s.right, s.left, s.fw, s.up. False: none now.
bool placeArmPose(const entity_t* e, const aliashdr_t* hdr, int pose1, int pose2, float blend, ArmPose& s)
{
    const ArmedRig* a = armedRigOf(e->model);
    if(!a || hdr->numverts != a->numVerts || s.cur.t < cl.mtime[0] - armPoseStale)
    {
        return false;
    }
    // Between the last two messages, as the entities' origins are (CL_RelinkEntities' lerp).
    ArmPoseMsg m = s.cur;
    if(s.prev.t > 0.0 && s.cur.t > s.prev.t && s.prev.prop == s.cur.prop && (s.prev.flags == s.cur.flags))
    {
        const float f = za::clamp(static_cast<float>((cl.time - s.prev.t) / (s.cur.t - s.prev.t)), 0.f, 1.f);
        const ArmPoseMsg& p = s.prev;
        m.wr = p.wr + (m.wr - p.wr) * f;
        m.v = p.v + (m.v - p.v) * f;
        m.fwB = nlerp(p.fwB, m.fwB, f);
        m.upB = nlerp(p.upB, m.upB, f);
        m.fwG = nlerp(p.fwG, m.fwG, f);
        m.upG = nlerp(p.upG, m.upG, f);
        m.wo = p.wo + (m.wo - p.wo) * f;
        m.wl = p.wl + (m.wl - p.wl) * f;
        m.vl = p.vl + (m.vl - p.vl) * f;
        if(p.propL == m.propL)
        {
            m.fwL = nlerp(p.fwL, m.fwL, f);
            m.upL = nlerp(p.upL, m.upL, f);
        }
        m.torso = p.torso + (m.torso - p.torso) * f;
        m.elbowR = p.elbowR + (m.elbowR - p.elbowR) * f;
        m.elbowL = p.elbowL + (m.elbowL - p.elbowL) * f;
        m.take = p.take + (m.take - p.take) * f;
        m.gripPos = p.gripPos + (m.gripPos - p.gripPos) * f; // (zero in a message with none)
        m.gripTurn = p.gripTurn + (m.gripTurn - p.gripTurn) * f;
        m.freePos = p.freePos + (m.freePos - p.freePos) * f;
        m.freeTurn = p.freeTurn + (m.freeTurn - p.freeTurn) * f;
        m.offKeyed = m.offKeyed || p.offKeyed;
        m.stretch = p.stretch + (m.stretch - p.stretch) * f;
    }
    const float scale = za::max(ENTSCALE_DECODE(e->scale), 0.01f);
    glm::vec3 hr{0.f};
    for(const int v : a->rightHand)
    {
        hr += poseVertex(hdr, pose1, pose2, blend, v) / static_cast<float>(za::getArraySize(a->rightHand));
    }
    // Its drawn rifle's line: the carry's turn.
    const glm::vec3 rear = (poseVertex(hdr, pose1, pose2, blend, a->gunRear[0]) + poseVertex(hdr, pose1, pose2, blend, a->gunRear[1])) * 0.5f;
    const glm::vec3 muzzle =
        (poseVertex(hdr, pose1, pose2, blend, a->gunMuzzle[0]) + poseVertex(hdr, pose1, pose2, blend, a->gunMuzzle[1])) * 0.5f;
    glm::vec3 gx = muzzle - rear;
    gx = glm::length(gx) > 1e-4f ? glm::normalize(gx) : glm::vec3{1.f, 0.f, 0.f};
    glm::vec3 gz = poseVertex(hdr, pose1, pose2, blend, a->gunTop) - poseVertex(hdr, pose1, pose2, blend, a->gunBottom);
    gz -= gx * glm::dot(gz, gx);
    gz = glm::length(gz) > 1e-4f ? glm::normalize(gz) : glm::vec3{0.f, 0.f, 1.f};
    const glm::vec3 gy = glm::cross(gz, gx);
    const glm::vec3 fwG = gx * m.fwG.x + gy * m.fwG.y + gz * m.fwG.z;
    const glm::vec3 upG = gx * m.upG.x + gy * m.upG.y + gz * m.upG.z;
    glm::vec3 fw = nlerp(m.fwB, fwG, m.wo);
    glm::vec3 up = nlerp(m.upB, upG, m.wo);
    up -= fw * glm::dot(up, fw);
    up = glm::length(up) > 1e-4f ? glm::normalize(up) : glm::vec3{0.f, 0.f, 1.f};
    // The animation's hand eased (a frame drawn unlerped, the shot's, moved it 7 units at once): the weapon and both
    // hands follow it without the jolt.
    const float dt = static_cast<float>(cl.time - s.handTime);
    s.hand = (s.handTime <= 0.0 || dt < 0.f || dt > 0.2f) ? hr : s.hand + (hr - s.hand) * (1.f - za::exp(-dt / handEase));
    s.handTime = cl.time;
    hr = s.hand;
    // vr_agarms: placed from its chest's anchor as drawn (the server placed them from it as it had it).
    const bool anchored = (m.flags & apAnchor) && a->anchor >= 0;
    const glm::vec3 anchorAt = anchored ? poseVertex(hdr, pose1, pose2, blend, a->anchor) : glm::vec3{0.f};
    if(anchored)
    {
        hr = anchorAt;
    }
    s.right = hr * m.wr + m.v / scale;
    s.fw = fw;
    s.up = up;
    s.prop = m.prop;
    s.flags = m.flags;
    s.propL = (m.flags & apLeftWeapon) ? m.propL : 0;
    s.fwL = m.fwL;
    s.upL = m.upL;
    s.left = m.vl / scale;
    s.torso = m.torso;
    s.elbowR = m.elbowR;
    s.elbowL = m.elbowL;
    s.waist = m.waist / scale;
    s.anchorAt = anchorAt;
    s.wr = anchored ? m.wr : 0.f;
    s.wl = anchored ? m.wl : 0.f;
    s.take = (m.flags & apFixed) ? m.take : 0.f;
    s.takeSide = m.takeSide;
    s.offKeyed = m.offKeyed;
    s.gripPos = m.gripPos;
    s.gripTurn = m.gripTurn;
    s.freePos = m.freePos / scale;
    s.freeTurn = m.freeTurn;
    s.stretch = m.stretch;
    s.keyGun = m.keyGun;
    if(anchored)
    {
        s.left += anchorAt * m.wl;
    }
    else if((m.flags & apLeft) && m.wl > 0.f)
    {
        const ragdoll::Rig* rig = ragdoll::rigFor(e->model);
        const int fl = rig ? boneNamed(*rig, a->foreL) : -1;
        if(fl >= 0)
        {
            const LeftHand& lh = findLeftHand(*a, e->model, *rig, hdr, fl);
            glm::vec3 hl{0.f};
            for(int i = 0; i < lh.num; i++)
            {
                hl += poseVertex(hdr, pose1, pose2, blend, lh.verts[i]) / static_cast<float>(za::max(lh.num, 1));
            }
            s.left += hl * m.wl;
        }
    }
    s.frame = host_framecount;
    return true;
}

// The arm pose of `e` placed this frame (relinkArms'), or none.
[[nodiscard]] const ArmPose* armPoseNow(const entity_t* e)
{
    const ArmPose* s = armPoseOf(numberOf(e), false);
    return (s && s->frame == host_framecount) ? s : nullptr;
}

// A strike's body (QC vr_armedgrunt_pose.qc VR_AGP_TorsoNow; ROUND22.md, "Armed monsters: the body in their strikes"):
// its chest turned about its waist (its joint with the pelvis) by s.torso (yaw about z, pitch about y, roll about x: as
// QC turned the hands' targets and the weapons), its pelvis by vr_agrunt_strike_hips of the yaw (the legs left planted),
// its head turned back by vr_agrunt_strike_head (its eyes kept on its target), every other bone on the chest (the arms)
// carried. Out: every bone's turn on `posed` (none: unturned). False: no turn now.
bool bodyTurn(const ragdoll::Rig& rig, const rigpose::Rigid* posed, const ArmPose& s, rigpose::Rigid* out)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        out[b] = rigpose::Rigid{};
    }
    const glm::vec3 t = s.torso * (3.14159265f / 180.f);
    if(glm::dot(t, t) < 1e-8f)
    {
        return false;
    }
    const int chest = chestOf(rig);
    // (the derived rigs' chest on the pelvis; the authored rig's on its spine on the pelvis: the turn at the
    // waist, the spine's joint, carrying the chest)
    int waist = chest;
    while(rig.authored && waist >= 0 && rig.bones[waist].parent >= 0 && rig.bones[rig.bones[waist].parent].parent >= 0)
    {
        waist = rig.bones[waist].parent;
    }
    const int pelvis = waist >= 0 ? rig.bones[waist].parent : -1;
    if(pelvis < 0 || rig.bones[pelvis].parent >= 0)
    {
        return false;
    }
    const glm::vec3 X{1.f, 0.f, 0.f}, Y{0.f, 1.f, 0.f}, Z{0.f, 0.f, 1.f};
    const auto about = [](const glm::quat& r, const glm::vec3& j) {
        rigpose::Rigid k;
        k.rot = r;
        k.pos = j - r * j;
        return k;
    };
    const glm::quat q = glm::angleAxis(t.x, Z) * glm::angleAxis(t.y, Y) * glm::angleAxis(t.z, X);
    const glm::quat qp = glm::angleAxis(za::clamp(vr_agrunt_strike_hips.value, 0.f, 1.f) * t.x, Z);
    out[pelvis] = about(qp, rigpose::apply(posed[pelvis], rig.bones[pelvis].pivot));
    out[waist] = composeRigid(out[pelvis], about(glm::inverse(qp) * q, rigpose::apply(posed[waist], rig.bones[waist].pivot)));
    out[chest] = out[waist];
    const int head = rig.head >= 0 ? rig.head : boneNamed(rig, "head");
    const float hs = za::clamp(vr_agrunt_strike_head.value, 0.f, 1.f);
    for(int b = 0; b < rig.numBones; b++)
    {
        if(b == chest || b == pelvis || b == waist || rig.bones[b].joint == ragdoll::Joint::Loose)
        {
            continue;
        }
        bool onChest = false;
        for(int p = rig.bones[b].parent, n = 0; p >= 0 && n < ragdoll::maxBones; p = rig.bones[p].parent, n++)
        {
            onChest = onChest || p == chest;
        }
        if(!onChest)
        {
            continue; // (the legs: planted)
        }
        out[b] = out[chest];
        if(b == head && hs > 0.f)
        {
            const glm::quat back = glm::angleAxis(-hs * t.x, Z) * glm::angleAxis(-hs * t.y, Y);
            out[b] = composeRigid(out[chest], about(back, rigpose::apply(posed[b], rig.bones[b].pivot)));
        }
    }
    return true;
}

// How far its rig's forearm, in each pose, misplaces each hand's vertices (units; [side][pose], side 0 right): the IK
// turns that forearm, so where it is far off (a grunt's painc and painb crouches) the hand can't be put anywhere by it.
struct Misfit
{
    const qmodel_t* model{nullptr};
    za::Vector<float> at[2];
};
Misfit misfits[numArmedRigs];

const Misfit& misfitOf(const ArmedRig& a, const qmodel_t* model, const ragdoll::Rig& rig, const aliashdr_t* hdr)
{
    Misfit& m = misfits[&a - armedRigs];
    if(m.model == model)
    {
        return m;
    }
    m.model = model;
    for(int side = 0; side < 2; side++)
    {
        const int fore = boneNamed(rig, side == 0 ? a.foreR : a.foreL);
        m.at[side].clear();
        m.at[side].resize(static_cast<za::SizeT>(hdr->numposes), 0.f);
        if(fore < 0)
        {
            continue;
        }
        const LeftHand& h = findLeftHand(a, model, rig, hdr, fore, side == 0);
        for(int p = 0; p < hdr->numposes && h.num > 0; p++)
        {
            const rigpose::Rigid k = rigpose::key(rig, p, fore);
            glm::vec3 d{0.f};
            for(int i = 0; i < h.num; i++)
            {
                d += poseVertex(hdr, p, p, 0.f, h.verts[i]) - rigpose::apply(k, poseVertex(hdr, 0, 0, 0.f, h.verts[i]));
            }
            m.at[side][static_cast<za::SizeT>(p)] = glm::length(d) / static_cast<float>(h.num);
        }
    }
    return m;
}

// Where the rig's forearm misplaces its hand (Misfit, over vr_agrunt_ik_misfit: wholly at twice it), its hand's target
// (and the weapon drawn there) is the animation's hand: the IK leaves that arm as it is drawn.
void misfitToHands(const ArmedRig& a, const qmodel_t* model, const ragdoll::Rig& rig, const aliashdr_t* hdr, int pose1, int pose2,
    float blend, ArmPose& s)
{
    const float from = vr_agrunt_ik_misfit.value;
    if(from <= 0.f)
    {
        return;
    }
    const Misfit& m = misfitOf(a, model, rig, hdr);
    for(int side = 0; side < 2; side++)
    {
        if(side == 1 && !(s.flags & apLeft))
        {
            continue;
        }
        const za::SizeT n = m.at[side].size();
        if(static_cast<za::SizeT>(pose1) >= n || static_cast<za::SizeT>(pose2) >= n)
        {
            continue;
        }
        const float off = m.at[side][static_cast<za::SizeT>(pose1)] * (1.f - blend) + m.at[side][static_cast<za::SizeT>(pose2)] * blend;
        const float u = za::clamp((off - from) / from, 0.f, 1.f);
        if(u <= 0.f)
        {
            continue;
        }
        const int fore = boneNamed(rig, side == 0 ? a.foreR : a.foreL);
        const LeftHand& h = findLeftHand(a, model, rig, hdr, fore, side == 0);
        glm::vec3 hand{0.f};
        for(int i = 0; i < h.num; i++)
        {
            hand += poseVertex(hdr, pose1, pose2, blend, h.verts[i]) / static_cast<float>(za::max(h.num, 1));
        }
        glm::vec3& t = side == 0 ? s.right : s.left;
        t += (hand - t) * (u * u * (3.f - 2.f * u));
    }
}

// Its hands' targets brought within vr_agrunt_ik_reach of their arms' length from their shoulders as posed (`posed`: the
// rig's bones as drawn): the weapons drawn there are on hands that reach them.
void reachIn(const ragdoll::Rig& rig, const ArmedRig& a, const rigpose::Rigid* posed, ArmPose& s)
{
    const float share = vr_agrunt_ik_reach.value;
    if(share <= 0.f)
    {
        return;
    }
    const auto in = [&](const char* upper, const char* fore, glm::vec3& t) {
        const int u = boneNamed(rig, upper), lo = boneNamed(rig, fore);
        if(u < 0 || lo < 0)
        {
            return;
        }
        const glm::vec3 S = rigpose::apply(posed[u], rig.bones[u].pivot);
        const glm::vec3 E = rigpose::apply(posed[lo], rig.bones[lo].pivot);
        const glm::vec3 H = rigpose::apply(posed[lo], rig.bones[lo].end);
        const float reach = share * (glm::length(E - S) + glm::length(H - E));
        const glm::vec3 d = t - S;
        const float l = glm::length(d);
        if(l > reach && l > 1e-3f)
        {
            t = S + d * (reach / l);
        }
    };
    in(a.upperR, a.foreR, s.right);
    if(s.flags & apLeft)
    {
        in(a.upperL, a.foreL, s.left);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Armed monsters' arms as rigid pieces (vr_agrunt_ik_rigid; ROUND22.md, "Armed humanoids' arms, hands and grips"): each
// arm's forearm and hand drawn as their rest shape moved whole, fitted to the animation's frames by one rigid motion a
// pose (the grunt's forearm and hand keep their shape in its frames to 1.2 units), the arm bent onto its weapon by the IK,
// the hand turned at its wrist towards the weapon's turn (within vr_agrunt_ik_wrist), and the weapon drawn on the hand by
// its grip offset (vr_aggrip.cpp): the hand never stretched, the weapon always in it.

constexpr int numPieces = 4;          // right forearm, right hand, left forearm, left hand
constexpr float pieceReach = 0.43f;   // an arm's vertices: within this share of its forearm's length of its bones' lines
constexpr float pieceReachMost = 5.f; // ... at most (units)
constexpr float wristBack = 0.34f;    // its wrist: this share of its forearm's length back from the forearm's end
constexpr float armMost = 0.97f;      // a target within this share of its arm's length (straighter: snapping)

struct ArmSide
{
    za::Vector<int> fore, hand;                  // the pieces' vertices
    za::Vector<int> all;                         // both: the forearm's, then the hand's
    za::Vector<glm::vec3> rest;                  // ... their rest places
    za::Vector<int> edges;                       // the hand's edges (pairs of indices into `all`: the stretch measured)
    int upper{-1}, foreBone{-1};                 // the rig's bones
    int handBone{-1};                            // the rig bone most of its hand's vertices are on (a ragdoll's hand)
    glm::vec3 elbow{0.f}, wrist{0.f}, grip{0.f}; // rest (model units): the forearm's pivot, the wrist, the hand's middle
    rigpose::Rigid gripFrame;                    // the hand's grip frame at rest: at its middle, turned as the rest pose's gun
    za::Vector<rigpose::Rigid> fit;              // each pose: rest -> that pose (the forearm and hand together)
};
struct ArmModel
{
    const qmodel_t* model{nullptr};
    bool ok{false};
    ArmSide side[2]; // right, left
    za::Vector<int8_t> piece; // each vertex: its piece (0..3) or -1
    int chest{-1}, pelvis{-1};
    float torso{0.f}; // the torso's radius about its chest's line (units)
};
ArmModel armModels[numArmedRigs];

// The rigid motion taking points `a` nearest to `b` (Horn's quaternion: the largest eigenvector, by Jacobi).
[[nodiscard]] rigpose::Rigid fitRigid(const glm::vec3* a, const glm::vec3* b, int n)
{
    rigpose::Rigid r;
    if(n <= 0)
    {
        return r;
    }
    glm::dvec3 ca{0.0}, cb{0.0};
    for(int i = 0; i < n; i++)
    {
        ca += glm::dvec3{a[i]};
        cb += glm::dvec3{b[i]};
    }
    ca /= static_cast<double>(n);
    cb /= static_cast<double>(n);
    double S[3][3] = {};
    for(int i = 0; i < n; i++)
    {
        const glm::dvec3 da = glm::dvec3{a[i]} - ca, db = glm::dvec3{b[i]} - cb;
        for(int x = 0; x < 3; x++)
        {
            for(int y = 0; y < 3; y++)
            {
                S[x][y] += da[x] * db[y];
            }
        }
    }
    double N[4][4] = {
        {S[0][0] + S[1][1] + S[2][2], S[1][2] - S[2][1], S[2][0] - S[0][2], S[0][1] - S[1][0]},
        {S[1][2] - S[2][1], S[0][0] - S[1][1] - S[2][2], S[0][1] + S[1][0], S[2][0] + S[0][2]},
        {S[2][0] - S[0][2], S[0][1] + S[1][0], -S[0][0] + S[1][1] - S[2][2], S[1][2] + S[2][1]},
        {S[0][1] - S[1][0], S[2][0] + S[0][2], S[1][2] + S[2][1], -S[0][0] - S[1][1] + S[2][2]}};
    double V[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    for(int sweep = 0; sweep < 30; sweep++)
    {
        double off = 0.0;
        for(int p = 0; p < 4; p++)
        {
            for(int q = p + 1; q < 4; q++)
            {
                off += N[p][q] * N[p][q];
            }
        }
        if(off < 1e-20)
        {
            break;
        }
        for(int p = 0; p < 4; p++)
        {
            for(int q = p + 1; q < 4; q++)
            {
                if(za::abs(N[p][q]) < 1e-18)
                {
                    continue;
                }
                const double th = (N[q][q] - N[p][p]) / (2.0 * N[p][q]);
                const double t = (th >= 0.0 ? 1.0 : -1.0) / (za::abs(th) + za::sqrt(th * th + 1.0));
                const double c = 1.0 / za::sqrt(t * t + 1.0), s = t * c;
                for(int k = 0; k < 4; k++)
                {
                    const double kp = N[k][p], kq = N[k][q];
                    N[k][p] = c * kp - s * kq;
                    N[k][q] = s * kp + c * kq;
                }
                for(int k = 0; k < 4; k++)
                {
                    const double pk = N[p][k], qk = N[q][k];
                    N[p][k] = c * pk - s * qk;
                    N[q][k] = s * pk + c * qk;
                }
                for(int k = 0; k < 4; k++)
                {
                    const double kp = V[k][p], kq = V[k][q];
                    V[k][p] = c * kp - s * kq;
                    V[k][q] = s * kp + c * kq;
                }
            }
        }
    }
    int best = 0;
    for(int i = 1; i < 4; i++)
    {
        best = N[i][i] > N[best][best] ? i : best;
    }
    const glm::quat q{static_cast<float>(V[0][best]), static_cast<float>(V[1][best]), static_cast<float>(V[2][best]),
        static_cast<float>(V[3][best])};
    r.rot = glm::normalize(q);
    r.pos = glm::vec3{cb} - r.rot * glm::vec3{ca};
    return r;
}

[[nodiscard]] rigpose::Rigid inverseRigid(const rigpose::Rigid& a)
{
    rigpose::Rigid r;
    r.rot = glm::inverse(a.rot);
    r.pos = -(r.rot * a.pos);
    return r;
}

[[nodiscard]] rigpose::Rigid lerpRigid(const rigpose::Rigid& a, const rigpose::Rigid& b, float t)
{
    rigpose::Rigid r;
    r.rot = glm::slerp(a.rot, b.rot, t);
    r.pos = glm::mix(a.pos, b.pos, t);
    return r;
}

// A frame (x forward, z up) as a rotation.
[[nodiscard]] glm::quat frameRot(const glm::vec3& fw, const glm::vec3& upHint)
{
    const glm::vec3 x = glm::length(fw) > 1e-5f ? glm::normalize(fw) : glm::vec3{1.f, 0.f, 0.f};
    glm::vec3 z = upHint - x * glm::dot(upHint, x);
    z = glm::length(z) > 1e-4f ? glm::normalize(z) : glm::normalize(glm::cross(x, glm::vec3{0.f, 1.f, 0.f}) + glm::vec3{0.f, 0.f, 1e-3f});
    const glm::vec3 y = glm::cross(z, x);
    return glm::normalize(glm::quat_cast(glm::mat3{x, y, z}));
}

[[nodiscard]] bool isGunOf(const void* row, int v)
{
    return isGunVertex(*static_cast<const ArmedRig*>(row), v);
}

// The skin's spec of an armed rig's model (vr_agarms): its arms' bones, its chest, its rest gun's turn (the hands' grips).
bool specOf(const ArmedRig& a, const ragdoll::Rig& rig, const aliashdr_t* hdr, agarms::Spec& s)
{
    if(hdr->numverts != a.numVerts || rig.numVerts != hdr->numverts)
    {
        return false;
    }
    s.kind = static_cast<int>(&a - armedRigs);
    s.upper[0] = boneNamed(rig, a.upperR);
    s.fore[0] = boneNamed(rig, a.foreR);
    s.upper[1] = boneNamed(rig, a.upperL);
    s.fore[1] = boneNamed(rig, a.foreL);
    s.chest = boneNamed(rig, "chest");
    s.chest = s.chest >= 0 ? s.chest : chestOf(rig);
    const glm::vec3 rear = (poseVertex(hdr, 0, 0, 0.f, a.gunRear[0]) + poseVertex(hdr, 0, 0, 0.f, a.gunRear[1])) * 0.5f;
    const glm::vec3 muzzle = (poseVertex(hdr, 0, 0, 0.f, a.gunMuzzle[0]) + poseVertex(hdr, 0, 0, 0.f, a.gunMuzzle[1])) * 0.5f;
    s.gunRot = frameRot(muzzle - rear, poseVertex(hdr, 0, 0, 0.f, a.gunTop) - poseVertex(hdr, 0, 0, 0.f, a.gunBottom));
    s.isGun = isGunOf;
    s.row = &a;
    s.torsoSides = a.torsoSides;
    s.twistShare = a.twistShare;
    s.wristMore = a.wristMore;
    return true;
}

// `model`'s arms as pieces, found once a model (null: not found; vr_agrunt_ik_rigid then has no effect on it).
const ArmModel* armModelOf(const ArmedRig& a, const qmodel_t* model, const ragdoll::Rig& rig, const aliashdr_t* hdr)
{
    ArmModel& m = armModels[&a - armedRigs];
    if(m.model == model)
    {
        return m.ok ? &m : nullptr;
    }
    m = ArmModel{};
    m.model = model;
    if(hdr->numverts != a.numVerts || rig.numVerts != hdr->numverts || rig.numPoses != hdr->numposes)
    {
        return nullptr;
    }
    const int nv = hdr->numverts;
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
    const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
    const auto corner = [&](int i) { return za::clamp(static_cast<int>(desc[idx[i]].vertindex), 0, nv - 1); };
    za::Vector<glm::vec3> rest(static_cast<za::SizeT>(nv));
    for(int v = 0; v < nv; v++)
    {
        rest[static_cast<za::SizeT>(v)] = poseVertex(hdr, 0, 0, 0.f, v);
    }
    m.piece.resize(static_cast<za::SizeT>(nv), static_cast<int8_t>(-1));
    // The rest pose's gun: the grip frames' turn (a weapon held as the grunt held its own).
    const glm::vec3 rear = (rest[static_cast<za::SizeT>(a.gunRear[0])] + rest[static_cast<za::SizeT>(a.gunRear[1])]) * 0.5f;
    const glm::vec3 muzzle = (rest[static_cast<za::SizeT>(a.gunMuzzle[0])] + rest[static_cast<za::SizeT>(a.gunMuzzle[1])]) * 0.5f;
    const glm::quat gunRot = frameRot(muzzle - rear, rest[static_cast<za::SizeT>(a.gunTop)] - rest[static_cast<za::SizeT>(a.gunBottom)]);
    za::Vector<uint8_t> in(static_cast<za::SizeT>(nv));
    for(int s = 0; s < 2; s++)
    {
        ArmSide& side = m.side[s];
        side.upper = boneNamed(rig, s == 0 ? a.upperR : a.upperL);
        side.foreBone = boneNamed(rig, s == 0 ? a.foreR : a.foreL);
        if(side.upper < 0 || side.foreBone < 0)
        {
            return nullptr;
        }
        const glm::vec3 S = rigpose::apply(rigpose::key(rig, 0, side.upper), rig.bones[side.upper].pivot);
        const glm::vec3 E = rigpose::apply(rigpose::key(rig, 0, side.foreBone), rig.bones[side.foreBone].pivot);
        const glm::vec3 H = rigpose::apply(rigpose::key(rig, 0, side.foreBone), rig.bones[side.foreBone].end);
        const float L = glm::length(H - E);
        if(L < 1.f)
        {
            return nullptr;
        }
        const glm::vec3 ax = (H - E) / L;
        const float R = za::min(pieceReach * L, pieceReachMost);
        const glm::vec3 farEnd = H + (H - E) * 0.3f;
        const auto nearArm = [&](int v) {
            const glm::vec3& p = rest[static_cast<za::SizeT>(v)];
            if(isGunVertex(a, v) || m.piece[static_cast<za::SizeT>(v)] >= 0)
            {
                return false;
            }
            const glm::vec3 su = E - S;
            const float tu = glm::dot(p - S, su) / za::max(glm::dot(su, su), 1e-4f);
            return segmentDistance(p, E, farEnd) <= R || (tu > 0.25f && segmentDistance(p, S, E) <= R);
        };
        // From the vertex nearest the forearm's end, along the triangles' edges.
        int start = -1;
        float best = 1e30f;
        for(int i = 0; i + 2 < hdr->numindexes; i++)
        {
            const int v = corner(i);
            const float d = glm::length(rest[static_cast<za::SizeT>(v)] - H);
            if(d < best && nearArm(v))
            {
                best = d;
                start = v;
            }
        }
        if(start < 0)
        {
            return nullptr;
        }
        for(uint8_t& f : in)
        {
            f = 0;
        }
        in[static_cast<za::SizeT>(start)] = 1;
        for(bool grew = true; grew;)
        {
            grew = false;
            for(int i = 0; i + 2 < hdr->numindexes; i += 3)
            {
                const int c[3] = {corner(i), corner(i + 1), corner(i + 2)};
                if(!in[static_cast<za::SizeT>(c[0])] && !in[static_cast<za::SizeT>(c[1])] && !in[static_cast<za::SizeT>(c[2])])
                {
                    continue;
                }
                for(const int v : c)
                {
                    if(!in[static_cast<za::SizeT>(v)] && nearArm(v))
                    {
                        in[static_cast<za::SizeT>(v)] = 1;
                        grew = true;
                    }
                }
            }
        }
        int boneCount[ragdoll::maxBones] = {};
        glm::vec3 mid{0.f};
        for(int v = 0; v < nv; v++)
        {
            if(!in[static_cast<za::SizeT>(v)])
            {
                continue;
            }
            const glm::vec3& p = rest[static_cast<za::SizeT>(v)];
            const float t = glm::dot(p - E, ax);
            if(t > L - wristBack * L)
            {
                side.hand.pushBack(v);
                m.piece[static_cast<za::SizeT>(v)] = static_cast<int8_t>(2 * s + 1);
                mid += p;
                boneCount[za::clamp(static_cast<int>(rig.vertBone[static_cast<za::SizeT>(v)]), 0, ragdoll::maxBones - 1)]++;
            }
            else if(segmentDistance(p, E, H) <= segmentDistance(p, S, E))
            {
                side.fore.pushBack(v);
                m.piece[static_cast<za::SizeT>(v)] = static_cast<int8_t>(2 * s);
            }
        }
        if(side.hand.size() < 3 || side.fore.size() < 3)
        {
            return nullptr;
        }
        side.handBone = 0;
        for(int b = 1; b < ragdoll::maxBones; b++)
        {
            side.handBone = boneCount[b] > boneCount[side.handBone] ? b : side.handBone;
        }
        side.elbow = E;
        side.wrist = E + ax * (L - wristBack * L);
        side.grip = mid / static_cast<float>(side.hand.size());
        side.gripFrame.rot = gunRot;
        side.gripFrame.pos = side.grip;
        // Both pieces' vertices in one list (the forearm's, then the hand's), and the hand's edges in it.
        za::Vector<int> at(static_cast<za::SizeT>(nv), -1);
        for(const int v : side.fore)
        {
            at[static_cast<za::SizeT>(v)] = static_cast<int>(side.all.size());
            side.all.pushBack(v);
        }
        for(const int v : side.hand)
        {
            at[static_cast<za::SizeT>(v)] = static_cast<int>(side.all.size());
            side.all.pushBack(v);
        }
        for(int i = 0; i + 2 < hdr->numindexes; i += 3)
        {
            for(int k = 0; k < 3; k++)
            {
                const int u = corner(i + k), w = corner(i + (k + 1) % 3);
                if(u < w && m.piece[static_cast<za::SizeT>(u)] == 2 * s + 1 && m.piece[static_cast<za::SizeT>(w)] == 2 * s + 1)
                {
                    side.edges.pushBack(at[static_cast<za::SizeT>(u)]);
                    side.edges.pushBack(at[static_cast<za::SizeT>(w)]);
                }
            }
        }
        for(const int v : side.all)
        {
            side.rest.pushBack(rest[static_cast<za::SizeT>(v)]);
        }
        // Each pose's fit.
        za::Vector<glm::vec3> from, to;
        for(const int v : side.fore)
        {
            from.pushBack(rest[static_cast<za::SizeT>(v)]);
        }
        for(const int v : side.hand)
        {
            from.pushBack(rest[static_cast<za::SizeT>(v)]);
        }
        side.fit.resize(static_cast<za::SizeT>(hdr->numposes));
        for(int p = 0; p < hdr->numposes; p++)
        {
            to.clear();
            for(const int v : side.fore)
            {
                to.pushBack(poseVertex(hdr, p, p, 0.f, v));
            }
            for(const int v : side.hand)
            {
                to.pushBack(poseVertex(hdr, p, p, 0.f, v));
            }
            side.fit[static_cast<za::SizeT>(p)] = fitRigid(from.data(), to.data(), static_cast<int>(from.size()));
        }
    }
    m.chest = boneNamed(rig, "chest");
    m.pelvis = boneNamed(rig, "pelvis");
    if(m.chest >= 0 && m.pelvis >= 0)
    {
        const glm::vec3 c0 = rigpose::apply(rigpose::key(rig, 0, m.chest), rig.bones[m.chest].pivot);
        const glm::vec3 c1 = rigpose::apply(rigpose::key(rig, 0, m.chest), rig.bones[m.chest].end);
        float r = 0.f;
        for(const ArmSide& side : m.side)
        {
            r += 0.5f * segmentDistance(rigpose::apply(rigpose::key(rig, 0, side.upper), rig.bones[side.upper].pivot), c0, c1);
        }
        m.torso = 0.75f * r;
    }
    m.ok = true;
    if(developer.value)
    {
        Con_Printf("agpieces %s: right fore %d hand %d (bone %d), left fore %d hand %d (bone %d), torso %.1f\n", model->name,
            static_cast<int>(m.side[0].fore.size()), static_cast<int>(m.side[0].hand.size()), m.side[0].handBone,
            static_cast<int>(m.side[1].fore.size()), static_cast<int>(m.side[1].hand.size()), m.side[1].handBone, m.torso);
    }
    return &m;
}

// The forearm and hand of `side` as the animation has them now (its fits between the poses drawn).
[[nodiscard]] rigpose::Rigid pieceNow(const ArmSide& side, int pose1, int pose2, float blend)
{
    const za::SizeT n = side.fit.size();
    if(static_cast<za::SizeT>(pose1) >= n || static_cast<za::SizeT>(pose2) >= n)
    {
        return rigpose::Rigid{};
    }
    return lerpRigid(side.fit[static_cast<za::SizeT>(pose1)], side.fit[static_cast<za::SizeT>(pose2)], pose1 == pose2 ? 0.f : blend);
}

// The turn `q` (rest space) with its twist about `axis` within `twistMost` and its swing within `swingMost` (degrees);
// `bend` its swing's angle before (degrees).
[[nodiscard]] glm::quat clampWrist(const glm::quat& q, const glm::vec3& axis, float swingMost, float twistMost, float& bend)
{
    const glm::vec3 v{q.x, q.y, q.z};
    const glm::vec3 p = axis * glm::dot(v, axis);
    glm::quat twist{q.w, p.x, p.y, p.z};
    const float tl = za::sqrt(twist.w * twist.w + glm::dot(p, p));
    twist = tl > 1e-6f ? glm::quat{twist.w / tl, twist.x / tl, twist.y / tl, twist.z / tl} : glm::quat{1.f, 0.f, 0.f, 0.f};
    const glm::quat swing = q * glm::inverse(twist);
    const auto limit = [](const glm::quat& r, float mostDeg, float& angleDeg) {
        glm::quat t = r.w < 0.f ? -r : r;
        const float s = za::sqrt(t.x * t.x + t.y * t.y + t.z * t.z);
        const float angle = 2.f * za::atan2(s, t.w);
        angleDeg = angle * (180.f / 3.14159265f);
        const float most = mostDeg * (3.14159265f / 180.f);
        if(angle <= most || s < 1e-6f)
        {
            return t;
        }
        const glm::vec3 ax{t.x / s, t.y / s, t.z / s};
        return glm::angleAxis(most, ax);
    };
    float twistDeg = 0.f;
    const glm::quat sw = limit(swing, swingMost, bend);
    const glm::quat tw = limit(twist, twistMost, twistDeg);
    return glm::normalize(sw * tw);
}

// What an arm reaches for: 0 nothing (the animation's forearm and hand), 1 the hand's grip frame to `want` (the arm to
// its wrist, the hand turned at it), 2 its hand's middle to `want.pos` (the hand unturned).
struct ArmGoal
{
    int mode{0};
    rigpose::Rigid want;
    glm::vec3 pole{0.f, 0.f, -1.f};
};
struct ArmDone
{
    rigpose::Rigid fore, hand; // the pieces (absolute: rest -> model space)
    float bend{0.f};           // the wrist's turn wanted (degrees, its swing)
    float miss{0.f};           // the hand's miss (units)
    bool reached{true};
};

// Solves one arm (`posed`: the rig as drawn, `now`: its forearm and hand piece now): out[upper], out[fore] and its pieces.
ArmDone solveArmPiece(const ArmModel& am, const ragdoll::Rig& rig, const rigpose::Rigid* posed, int s, const rigpose::Rigid& now,
    const ArmGoal& goal, rigpose::Rigid* out)
{
    const ArmSide& side = am.side[s];
    ArmDone done;
    done.fore = now;
    done.hand = now;
    if(goal.mode == 0)
    {
        return done;
    }
    const glm::vec3 S = rigpose::apply(posed[side.upper], rig.bones[side.upper].pivot);
    const glm::vec3 E = rigpose::apply(now, side.elbow);
    rigpose::Rigid wantHand;
    glm::vec3 H, T;
    if(goal.mode == 1)
    {
        wantHand = composeRigid(goal.want, inverseRigid(side.gripFrame));
        H = rigpose::apply(now, side.wrist);
        T = rigpose::apply(wantHand, side.wrist);
    }
    else
    {
        H = rigpose::apply(now, side.grip);
        T = goal.want.pos;
    }
    // Its hand out of the torso: a target inside it (a swing's arc across the chest, a crouch's) moved out to its skin.
    if(am.chest >= 0 && am.torso > 0.f)
    {
        const glm::vec3 c0 = rigpose::apply(posed[am.chest], rig.bones[am.chest].pivot);
        const glm::vec3 c1 = rigpose::apply(posed[am.chest], rig.bones[am.chest].end);
        const glm::vec3 cl = c1 - c0;
        const float t = za::clamp(glm::dot(T - c0, cl) / za::max(glm::dot(cl, cl), 1e-4f), 0.f, 1.f);
        const glm::vec3 c = c0 + cl * t;
        const glm::vec3 out = T - c;
        const float skin = am.torso + 1.5f; // (the hand's own thickness)
        if(glm::length(out) < skin)
        {
            const glm::vec3 away = glm::length(out) > 1e-3f ? glm::normalize(out) : glm::normalize(S - c);
            T = c + away * skin;
        }
    }
    // Its elbow and forearm out of the torso: tried as wanted, then (if they went in) away from the chest's line.
    rigpose::Rigid turnU, turnF;
    glm::vec3 pole = goal.pole;
    for(int attempt = 0; attempt < 2; attempt++)
    {
        if(!solveArmPoints(S, E, H, T, pole, armMost, vr_agrunt_ik_elbow.value, turnU, turnF))
        {
            return done;
        }
        if(am.chest < 0 || am.pelvis < 0 || am.torso <= 0.f || attempt == 1)
        {
            break;
        }
        const glm::vec3 c0 = rigpose::apply(posed[am.chest], rig.bones[am.chest].pivot);
        const glm::vec3 c1 = rigpose::apply(posed[am.chest], rig.bones[am.chest].end);
        const glm::vec3 E2 = rigpose::apply(turnU, E);
        const glm::vec3 M2 = (E2 + rigpose::apply(turnF, H)) * 0.5f;
        if(segmentDistance(E2, c0, c1) >= am.torso && segmentDistance(M2, c0, c1) >= am.torso)
        {
            break;
        }
        const glm::vec3 cl = c1 - c0;
        const glm::vec3 m = (S + T) * 0.5f;
        const float t = za::clamp(glm::dot(m - c0, cl) / za::max(glm::dot(cl, cl), 1e-4f), 0.f, 1.f);
        pole = m - (c0 + cl * t);
    }
    out[side.upper] = turnU;
    out[side.foreBone] = turnF;
    done.fore = composeRigid(turnF, now);
    done.hand = done.fore;
    if(goal.mode == 1)
    {
        const glm::vec3 axis = glm::normalize(side.wrist - side.elbow);
        const glm::quat rel = glm::inverse(done.fore.rot) * wantHand.rot;
        const glm::quat q = clampWrist(rel, axis, vr_agrunt_ik_wrist.value, vr_agrunt_ik_twist.value, done.bend);
        done.hand.rot = glm::normalize(done.fore.rot * q);
        done.hand.pos = rigpose::apply(done.fore, side.wrist) - done.hand.rot * side.wrist;
        done.miss = glm::length(rigpose::apply(done.hand, side.grip) - rigpose::apply(wantHand, side.grip));
    }
    else
    {
        done.miss = glm::length(rigpose::apply(done.fore, side.grip) - T);
    }
    done.reached = done.miss < 0.5f;
    return done;
}

// The arms solved this frame (relinkArms', before the drawing: its weapons placed on the hands; boneTurns reads them).
struct ArmSolve
{
    const entity_t* e{nullptr};
    int frame{-1};
    rigpose::Rigid out[ragdoll::maxBones];
    rigpose::Rigid pieces[numPieces];
    ArmDone done[2];
    int kindL{0};
    bool fitOk[2]{};
    glm::vec3 fitAngles[2]{}; // the grip offsets' angles that would leave its wrists unturned now (vr_agrunt_grip_fit)
};

// A turn as a grip offset's angles (offsetRigid's: yaw about z, then pitch about y, then roll about x; degrees).
[[nodiscard]] glm::vec3 offsetAngles(const glm::quat& q)
{
    const glm::mat3 m = glm::mat3_cast(q);
    constexpr float r2d = 180.f / 3.14159265f;
    const float pitch = za::asin(za::clamp(-m[0][2], -1.f, 1.f));
    return {pitch * r2d, za::atan2(m[0][1], m[0][0]) * r2d, za::atan2(m[1][2], m[2][2]) * r2d};
}
constexpr int maxArmSolves = 64;
struct ArmSolves
{
    ArmSolve at[maxArmSolves];
    int next{0};
};
ArmSolves armSolves;

[[nodiscard]] const ArmSolve* armSolveOf(const entity_t* e)
{
    for(const ArmSolve& s : armSolves.at)
    {
        if(s.e == e && s.frame == host_framecount)
        {
            return &s;
        }
    }
    return nullptr;
}

ArmSolve& armSolveFor(const entity_t* e)
{
    for(ArmSolve& s : armSolves.at)
    {
        if(s.e == e || s.frame != host_framecount)
        {
            s.e = e;
            return s;
        }
    }
    ArmSolve& s = armSolves.at[armSolves.next];
    armSolves.next = (armSolves.next + 1) % maxArmSolves;
    s.e = e;
    return s;
}

// The hands as kickInstance will draw them (setHandsDrawn), for the entity it draws now.
struct HandsDrawn
{
    const entity_t* e{nullptr};
    int frame{-1};
    hitreact::HandFit right, left;
};
HandsDrawn handsDrawn;

// vr_agrunt_ik_err: the drawn hands' distances to their weapons, per armed rig and vr_agrunt_ik_tag (QC's tests set it:
// an animation frame, an attack's phase).
constexpr int errTags = 1024;
struct HandErr
{
    int n{0}, nL{0};
    float maxR{0.f}, sumR{0.f}, maxL{0.f}, sumL{0.f};
};
struct HandErrs
{
    HandErr at[2][errTags];
};
HandErrs handErrs;

// vr_agrunt_ik_err's arm numbers (noteArmsDrawn), per armed rig, tag and side: the hand's middle to the weapon's grip
// (its grip offset's), the hand's stretch (its edges' lengths against rest: the root of the mean squares' ratio, and the
// longest's), the forearm and hand's shape (the worst vertex off their rest shape moved whole), the elbow's bend
// (degrees), the arm's points inside the torso (units).
struct ArmErr
{
    int n{0}, nGrip{0};
    float gripMax{0.f}, gripSum{0.f};
    float stretchMax{0.f}, stretchSum{0.f}, edgeMax{0.f};
    float shapeMax{0.f};
    float bendMax{0.f}, bendMin{999.f};
    float penMax{0.f};
    float wristMax{0.f}; // the wrist's turn wanted (degrees: its bend onto the weapon's turn; vr_agrunt_ik_wrist clamps it)
};
struct ArmErrs
{
    ArmErr at[2][errTags][2];
};
ArmErrs armErrs;

// vr_agrunt_ik_err's ragdoll numbers (ragdollProps): the server's placing of a ragdoll's weapons against its drawn hand.
struct RagErr
{
    int n{0};
    float offMax{0.f}, offSum{0.f}, turnMax{0.f};
    float propFar{0.f}; // vr_agarms: a held weapon drawn this far from its holder's origin, most (units)
};
RagErr ragErr;

// The arms' turns now (armsWanted), out: every bone's (none but the arms' turned). False: not now (dead, no weapon).
bool armsNow(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, const aliashdr_t* hdr, int pose1,
    int pose2, float blend, rigpose::Rigid* out)
{
    const ArmedRig* a = armedRigOf(e->model);
    if(!a || hdr->numverts != a->numVerts || rig.numVerts != hdr->numverts)
    {
        if(vr_agrunt_ik_debug.value >= 2.f)
        {
            Con_Printf("agik ent %d: %d vertices (its rig's %d)\n", numberOf(e), hdr->numverts, rig.numVerts);
        }
        return false;
    }
    for(int i = 0; i < rig.deaths && i < 4; i++)
    {
        if(pose1 >= rig.deathFirst[i] && pose1 <= rig.deathLast[i])
        {
            if(vr_agrunt_ik_debug.value >= 2.f)
            {
                Con_Printf("agik ent %d: pose %d a death's (frame %d, poses %d %d)\n", numberOf(e), pose1, e->frame,
                    e->previouspose, e->currentpose);
            }
            return false; // (dying: its weapon dropped)
        }
    }
    if(vr_agrunt_ik_rigid.value != 0.f)
    {
        if(const ArmSolve* sol = armSolveOf(e))
        {
            for(int b = 0; b < rig.numBones; b++)
            {
                out[b] = sol->out[b]; // (relinkRigid's: its weapons drawn on these hands)
            }
            return true;
        }
    }
    const int ur = boneNamed(rig, a->upperR), fr = boneNamed(rig, a->foreR);
    const int ul = boneNamed(rig, a->upperL), fl = boneNamed(rig, a->foreL);
    if(ur < 0 || fr < 0 || ul < 0 || fl < 0)
    {
        if(vr_agrunt_ik_debug.value >= 2.f)
        {
            Con_Printf("agik ent %d: no arm bones\n", numberOf(e));
        }
        return false;
    }
    glm::vec3 hr{0.f};
    for(const int v : a->rightHand)
    {
        hr += poseVertex(hdr, pose1, pose2, blend, v) / static_cast<float>(za::getArraySize(a->rightHand));
    }
    // Its arm pose from the server (QVR_SVC_ARMPOSE): the hands' targets and its weapon; else its weapon is the held one
    // nearest its right hand.
    const ArmPose* ap = armPoseNow(e);
    gatherProps();
    const glm::vec3 hrWorld = toWorld(e, hr);
    int prop = -1;
    float best = propReach;
    if(ap)
    {
        prop = (ap->prop > 0 && ap->prop < cl.num_entities && cl_entities[ap->prop].model &&
                   (cl_entities[ap->prop].effects & heldEffect))
                   ? ap->prop
                   : -1;
        best = -1.f; // (no search)
    }
    for(int i = 0; i < armIk.numProps && !ap; i++)
    {
        const entity_t& p = cl_entities[armIk.props[i]];
        const float d = glm::length(glm::vec3{p.origin[0], p.origin[1], p.origin[2]} - hrWorld);
        if(d < best)
        {
            best = d;
            prop = armIk.props[i];
        }
    }
    if(prop < 0 && !ap)
    {
        if(vr_agrunt_ik_debug.value >= 2.f)
        {
            Con_Printf("agik ent %d: pose %d, no weapon within %.0f of its hand (%d held weapons)\n", numberOf(e), pose1,
                propReach, armIk.numProps);
        }
        return false; // (its fists: the animation's arms)
    }
    const entity_t* pe = prop >= 0 ? &cl_entities[prop] : nullptr;
    const int slot = pe ? weapons::slotForModel(pe->model) : -1;
    for(int b = 0; b < rig.numBones; b++)
    {
        out[b] = rigpose::Rigid{};
    }

    // The right hand on the grip (the weapon's handle: its origin; its fist's target), the elbow where the animation has it.
    const glm::vec3 tr = ap ? ap->right : toLocal(e, glm::vec3{pe->origin[0], pe->origin[1], pe->origin[2]}, true);
    const glm::vec3 er = rigpose::apply(posed[fr], rig.bones[fr].pivot) - rigpose::apply(posed[ur], rig.bones[ur].pivot);
    const float mainBefore = glm::length(hr - tr);
    // Aimed by the hand as it is drawn (kickInstance's fit: Model Animation's spline, swings and rebuild move the drawn
    // hand off the frames' lerp, and a vertex fitted worse takes only its share of the turn): drawn D + W K(P) on target.
    const bool fitted = vr_agrunt_ik_drawn.value != 0.f && handsDrawn.e == e && handsDrawn.frame == host_framecount;
    const hitreact::HandFit& fitR = handsDrawn.right;
    if(vr_agrunt_ik_debug.value >= 3.f)
    {
        Con_Printf("agfit ent %d fitted %d ok %d W %.2f P %.1f %.1f %.1f D %.1f %.1f %.1f hr %.1f %.1f %.1f; left ok %d W %.2f\n",
            numberOf(e), fitted ? 1 : 0, fitR.ok ? 1 : 0, fitR.W, fitR.P.x, fitR.P.y, fitR.P.z, fitR.D.x, fitR.D.y, fitR.D.z, hr.x,
            hr.y, hr.z, handsDrawn.left.ok ? 1 : 0, handsDrawn.left.W);
    }
    const float mainAfter = (fitted && fitR.ok)
                                ? solveArm(rig, posed, ur, fr, fitR.P, (tr - fitR.D) / fitR.W, er, out) * fitR.W
                                : solveArm(rig, posed, ur, fr, hr, tr, er, out);

    // The left hand on its second grip, under the right one (a Cup), or hanging free.
    const LeftHand& lh = findLeftHand(*a, e->model, rig, hdr, fl);
    if(lh.num == 0)
    {
        if(vr_agrunt_ik_debug.value >= 2.f)
        {
            Con_Printf("agik ent %d: no left hand found (main %.2f -> %.2f)\n", numberOf(e), mainBefore, mainAfter);
        }
        return true;
    }
    glm::vec3 hl{0.f};
    for(int i = 0; i < lh.num; i++)
    {
        hl += poseVertex(hdr, pose1, pose2, blend, lh.verts[i]) / static_cast<float>(lh.num);
    }
    int kind = 0; // 0 free, 1 a grip, 2 a cup, 3 its own target (a punch, a guard), 4 the animation's
    glm::vec3 tl{0.f};
    if(ap && (ap->flags & apLeft))
    {
        kind = 3;
        tl = ap->left;
    }
    else if(ap && (ap->flags & apLeftFree))
    {
        kind = 4;
    }
    for(int i = 0; slot >= 0 && i < weapons::maxHotspots && kind == 0; i++)
    {
        const weapons::Hotspot h = weapons::hotspot(slot, i);
        glm::vec3 w;
        if(h.type == weapons::HotspotType::Grip && view::groundHotspotPoint(prop, i, w))
        {
            kind = 1;
            tl = toLocal(e, w, true);
        }
        else if(h.type == weapons::HotspotType::Cup)
        {
            kind = 2;
            tl = tr + cupOffset;
        }
    }
    const glm::vec3 sl = rigpose::apply(posed[ul], rig.bones[ul].pivot);
    const glm::vec3 el = rigpose::apply(posed[fl], rig.bones[fl].pivot);
    glm::vec3 pole = el - sl;
    if(kind == 0)
    {
        const float reach = glm::length(el - sl) + glm::length(hl - el);
        tl = sl + glm::normalize(hangDir) * (hangReach * reach);
        pole = hangElbow;
    }
    if(kind == 4)
    {
        tl = hl; // (the animation's: unturned)
    }
    if(kind == 3)
    {
        pole = glm::vec3{-0.3f, 0.6f, -1.f}; // (a fist's elbow down and out, its left)
    }
    const float secondBefore = glm::length(hl - tl);
    const hitreact::HandFit& fitL = handsDrawn.left;
    const float secondAfter = kind == 4                              ? secondBefore
                              : (fitted && fitL.ok && kind != 0) ? solveArm(rig, posed, ul, fl, fitL.P, (tl - fitL.D) / fitL.W, pole, out) * fitL.W
                                                                  : solveArm(rig, posed, ul, fl, hl, tl, pole, out);
    if(vr_agrunt_ik_debug.value >= 3.f)
    {
        const glm::vec3 S = rigpose::apply(posed[ul], rig.bones[ul].pivot);
        Con_Printf("agfitl ent %d kind %d P %.1f %.1f %.1f D %.1f %.1f %.1f tl %.1f %.1f %.1f S %.1f %.1f %.1f miss %.2f\n",
            numberOf(e), kind, fitL.P.x, fitL.P.y, fitL.P.z, fitL.D.x, fitL.D.y, fitL.D.z, tl.x, tl.y, tl.z, S.x, S.y, S.z,
            secondAfter);
    }
    if(vr_agrunt_ik_debug.value < 0.f)
    {
        // The jitter trace (scratch/agjit.py): the targets and the animation's hands in its model space, each frame.
        Con_Printf("agj %d %.4f %d %d %.3f tr %.3f %.3f %.3f tl %.3f %.3f %.3f hr %.3f %.3f %.3f kind %d\n", numberOf(e),
            cl.time, pose1, pose2, blend, tr.x, tr.y, tr.z, tl.x, tl.y, tl.z, hr.x, hr.y, hr.z, kind);
    }
    const int every = static_cast<int>(vr_agrunt_ik_debug.value);
    if((every > 0 && host_framecount % every == 0) || armIk.forcePrint)
    {
        const float sc = ENTSCALE_DECODE(e->scale);
        Con_Printf("agik ent %d hf %d frame %d slot %d %s kind %d main %.2f %.2f second %.2f %.2f\n", numberOf(e), host_framecount,
            pose1, slot, pe ? pe->model->name : "(fists)", kind, mainBefore * sc, mainAfter * sc, secondBefore * sc,
            secondAfter * sc);
    }
    return true;
}

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

// A hand's grip offset for a weapon (vr_agarms: its kind's global one, the weapon's own on top).
[[nodiscard]] rigpose::Rigid gripRigid(int kind, int slot, int hand)
{
    const rigpose::Rigid w = offsetRigid(aggrip::offset(kind, slot, hand));
    return agarms::enabled() ? composeRigid(offsetRigid(aggrip::global(kind, hand)), w) : w;
}

// vr_agarms_arm_clear: a pistol carried low (its barrel down past 27 degrees) in one hand: no cup (the other cupping it
// there reached across the body, its upper arm through the torso).
[[nodiscard]] bool lowCarry(const agarms::Targets& t)
{
    return vr_agarms_arm_clear.value != 0.f && (t.weapon[0].rot * glm::vec3{1.f, 0.f, 0.f}).z < -0.45f;
}

void noteDrawnTurn(int num, const entity_t& p, const glm::vec3& at, const glm::vec3& fw);

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
    d.off = at - glm::vec3{p.origin[0], p.origin[1], p.origin[2]};
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

[[nodiscard]] bool heldProp(int num)
{
    return num > 0 && num < cl.num_entities && cl_entities[num].model && (cl_entities[num].effects & heldEffect) &&
           cl_entities[num].msgtime == cl.mtime[0];
}

[[nodiscard]] float viewDistance(const entity_t* e)
{
    const glm::vec3 o = drawnOrigin(e, nullptr);
    return glm::length(o - glm::vec3{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]});
}

// The rigid arms' relink (vr_agrunt_ik_rigid): both arms solved onto the server's targets now (the hands' grip frames
// on the weapons' as their grip offsets put them), each weapon drawn on its hand as solved; the solve kept for the
// drawing (boneTurns). False: not an armed rig's model with arm pieces (the old way then).
bool relinkRigid(const ArmPose& s, const entity_t* e, const aliashdr_t* hdr, int pose1, int pose2, float blend,
    const rigpose::Rigid* posed, const rigpose::Rigid* kick, int foreR, int foreL)
{
    const ArmedRig* a = armedRigOf(e->model);
    const ragdoll::Rig* rig = ragdoll::rigFor(e->model);
    const ArmModel* am = a && rig ? armModelOf(*a, e->model, *rig, hdr) : nullptr;
    if(!am)
    {
        return false;
    }
    const int kind = static_cast<int>(a - armedRigs);
    ArmSolve& sol = armSolveFor(e);
    sol.frame = host_framecount;
    for(int b = 0; b < rig->numBones; b++)
    {
        sol.out[b] = rigpose::Rigid{};
    }
    entity_t& p = cl_entities[s.prop];
    const int slot = weapons::slotForModel(p.model);
    const float b = pose1 == pose2 ? 0.f : za::clamp(blend, 0.f, 1.f);

    // Its right hand: the weapon's grip frame (the server's place for the weapon less its grip offset).
    rigpose::Rigid T;
    T.rot = frameRot(s.fw, s.up);
    T.pos = s.right;
    const rigpose::Rigid O = offsetRigid(aggrip::offset(kind, slot, aggrip::Main));
    ArmGoal gr;
    gr.mode = 1;
    gr.want = composeRigid(T, inverseRigid(O));
    gr.pole = rigpose::apply(posed[am->side[0].foreBone], rig->bones[am->side[0].foreBone].pivot) -
              rigpose::apply(posed[am->side[0].upper], rig->bones[am->side[0].upper].pivot);
    const rigpose::Rigid nowR = pieceNow(am->side[0], pose1, pose2, b);
    sol.fitOk[0] = true;
    sol.fitAngles[0] = offsetAngles(glm::inverse(nowR.rot * am->side[0].gripFrame.rot) * T.rot);
    sol.fitOk[1] = false;
    sol.done[0] = solveArmPiece(*am, *rig, posed, 0, nowR, gr, sol.out);
    const rigpose::Rigid weaponR = composeRigid(composeRigid(sol.done[0].hand, am->side[0].gripFrame), O);
    placeProp(p, e, weaponR, foreR >= 0 ? &kick[foreR] : nullptr);

    // Its left hand: its own weapon (dual), its own target (a punch, a guard), the animation's, the weapon's second grip,
    // or hanging free.
    const ArmSide& sideL = am->side[1];
    const rigpose::Rigid nowL = pieceNow(sideL, pose1, pose2, b);
    const glm::vec3 SL = rigpose::apply(posed[sideL.upper], rig->bones[sideL.upper].pivot);
    const glm::vec3 EL = rigpose::apply(nowL, sideL.elbow);
    const float reachL = glm::length(EL - SL) + glm::length(rigpose::apply(nowL, sideL.wrist) - EL);
    ArmGoal gl;
    gl.pole = EL - SL;
    int slotL = -1;
    rigpose::Rigid OL;
    sol.kindL = 0;
    if((s.flags & apLeftWeapon) && heldProp(s.propL))
    {
        slotL = weapons::slotForModel(cl_entities[s.propL].model);
        OL = offsetRigid(aggrip::offset(kind, slotL, aggrip::Off));
        rigpose::Rigid TL;
        TL.rot = frameRot(s.fwL, s.upL);
        TL.pos = s.left;
        sol.fitOk[1] = true;
        sol.fitAngles[1] = offsetAngles(glm::inverse(nowL.rot * sideL.gripFrame.rot) * TL.rot);
        gl.mode = 1;
        gl.want = composeRigid(TL, inverseRigid(OL));
        sol.kindL = 5;
    }
    else if(s.flags & apLeft)
    {
        gl.mode = 2;
        gl.want.pos = s.left;
        gl.pole = glm::vec3{-0.3f, 0.6f, -1.f}; // (a fist's elbow down and out, its left)
        sol.kindL = 3;
    }
    else if(s.flags & apLeftFree)
    {
        sol.kindL = 4;
    }
    else
    {
        for(int i = 0; slot >= 0 && i < weapons::maxHotspots && sol.kindL == 0; i++)
        {
            const weapons::Hotspot h = weapons::hotspot(slot, i);
            glm::vec3 w;
            if(h.type == weapons::HotspotType::Grip && view::groundHotspotPoint(s.prop, i, w))
            {
                const glm::vec3 hp = rigpose::apply(foreR >= 0 ? inverseRigid(kick[foreR]) : rigpose::Rigid{}, toLocal(e, w, true));
                const aggrip::Offset o2 = aggrip::offset(kind, slot, aggrip::Second);
                const rigpose::Rigid O2 = offsetRigid(o2);
                gl.mode = 1;
                gl.want.rot = glm::normalize(weaponR.rot * O2.rot);
                gl.want.pos = hp + weaponR.rot * O2.pos;
                const glm::vec3 wristAt = rigpose::apply(composeRigid(gl.want, inverseRigid(sideL.gripFrame)), sideL.wrist);
                sol.kindL = glm::length(wristAt - SL) <= vr_agrunt_ik_second.value * reachL ? 1 : 0;
                gl.mode = sol.kindL == 1 ? 1 : 0;
            }
            else if(h.type == weapons::HotspotType::Cup)
            {
                gl.mode = 2;
                gl.want.pos = rigpose::apply(sol.done[0].hand, am->side[0].grip) + cupOffset;
                sol.kindL = 2;
            }
        }
    }
    if(sol.kindL == 0)
    {
        gl.mode = 2;
        gl.want.pos = SL + glm::normalize(hangDir) * (hangReach * reachL);
        gl.pole = hangElbow;
    }
    sol.done[1] = solveArmPiece(*am, *rig, posed, 1, nowL, gl, sol.out);
    if(sol.kindL == 1 && sol.done[1].bend > vr_agrunt_ik_wrist.value + 25.f)
    {
        // (its wrist can't turn onto the second grip: it lets go, hanging free)
        sol.out[sideL.upper] = rigpose::Rigid{};
        sol.out[sideL.foreBone] = rigpose::Rigid{};
        gl.mode = 2;
        gl.want.pos = SL + glm::normalize(hangDir) * (hangReach * reachL);
        gl.pole = hangElbow;
        sol.kindL = 0;
        sol.done[1] = solveArmPiece(*am, *rig, posed, 1, nowL, gl, sol.out);
    }
    if(sol.kindL == 5)
    {
        const rigpose::Rigid weaponL = composeRigid(composeRigid(sol.done[1].hand, sideL.gripFrame), OL);
        placeProp(cl_entities[s.propL], e, weaponL, foreL >= 0 ? &kick[foreL] : nullptr);
    }
    sol.pieces[0] = sol.done[0].fore;
    sol.pieces[1] = sol.done[0].hand;
    sol.pieces[2] = sol.done[1].fore;
    sol.pieces[3] = sol.done[1].hand;
    aggrip::noteSubject(kind, slot, slotL, viewDistance(e));
    return true;
}

[[nodiscard]] bool placeable(const rigpose::Rigid& w);

// A ragdoll's weapons (an active ragdoll's: a stagger, a shove's fall, a tug) on its hands' bones as drawn now, by their
// grip offsets: never left floating while the server's catch up.
void ragdollProps(const ArmPose& s, const entity_t* e)
{
    const qmodel_t* src = ragdoll::sourceModel(e);
    const ArmedRig* a = armedRigOf(src);
    const ragdoll::Rig* rig = a ? ragdoll::rigFor(const_cast<qmodel_t*>(src)) : nullptr;
    if(!rig)
    {
        return;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(src)));
    // (vr_agarms: the hands' grip frames its posed arms held its weapons by, so the weapon stays as it was held. Its
    // rigid arms' model (vr_agrunt_ik_rigid) is never made for it then: without this its weapons were left to the server's
    // placing, riding the limb nearest them as the ragdoll began, off the hand.)
    const bool skinned = agarms::enabled() && vr_agrunt_ragweapon_check.value < 5.f; // TEMP before/after
    const ArmModel* am = !skinned && hdr ? armModelOf(*a, src, *rig, hdr) : nullptr;
    if(!skinned && !am)
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
        rigpose::Rigid gripFrame;
        int handBone = -1;
        if(am)
        {
            gripFrame = am->side[side].gripFrame;
            handBone = am->side[side].handBone;
        }
        else if(!agarms::ragdollGrip(src, side, gripFrame, handBone))
        {
            continue;
        }
        glm::quat rot;
        glm::vec3 pos;
        float scale = 1.f;
        if(!ragdoll::drawnPart(numberOf(e), handBone, rot, pos, scale))
        {
            continue;
        }
        entity_t& p = cl_entities[num];
        rigpose::Rigid w = composeRigid(gripFrame,
            gripRigid(kind, weapons::slotForModel(p.model), side == 0 ? aggrip::Main : aggrip::Off));
        // (Made from its posed arms, its hand on its forearm's bone: the hand's turn on it then, the weapon where it was.)
        if(rigpose::Rigid wrist; agarms::ragdollWrist(numberOf(e), src, side, wrist))
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
            const float off = glm::length(glm::vec3{p.origin[0], p.origin[1], p.origin[2]} - at);
            const float turn = za::acos(za::clamp(glm::dot(fw, glm::vec3{f[0], f[1], f[2]}), -1.f, 1.f)) * (180.f / 3.14159265f);
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
struct RagWeaponScratch
{
    za::Vector<glm::vec3> drawn;
    za::Vector<glm::vec3> from, to;
};
RagWeaponScratch ragWeaponScratch;

[[nodiscard]] glm::quat rotOfAngles(const float* angles)
{
    vec3_t a{-angles[0], angles[1], angles[2]}, f, r, u;
    AngleVectors(a, f, r, u);
    const glm::mat3 m{glm::vec3{f[0], f[1], f[2]}, -glm::vec3{r[0], r[1], r[2]}, glm::vec3{u[0], u[1], u[2]}};
    return glm::normalize(glm::quat_cast(m));
}

void ragWeaponMeasure(const ArmPose& s, const entity_t* e)
{
    const qmodel_t* src = ragdoll::sourceModel(e);
    const ArmedRig* a = armedRigOf(src);
    const ragdoll::Rig* rig = a ? ragdoll::rigFor(const_cast<qmodel_t*>(src)) : nullptr;
    const int num = numberOf(e);
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
        const glm::vec3 gp = glm::vec3{p.origin[0], p.origin[1], p.origin[2]} - scale * (gr * off.pos);
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

// vr_agarms: `e`'s arms posed by its weapons (vr_agarms.cpp) and those drawn on its hands; `s` placed this frame.
void relinkSkinned(const ArmPose& s, const entity_t* e, const aliashdr_t* hdr, int pose1, int pose2, float blend)
{
    const ArmedRig* a = armedRigOf(e->model);
    const ragdoll::Rig* rig = ragdoll::rigFor(e->model);
    agarms::Spec spec;
    if(!a || !rig || rig->numPoses != hdr->numposes || !specOf(*a, *rig, hdr, spec))
    {
        return;
    }
    // Its bones as drawn (kickInstance's), their hit reactions (the chest's carries the shoulders, and the weapons).
    rigpose::Rigid posed[ragdoll::maxBones];
    const float b = pose1 == pose2 ? 0.f : za::clamp(blend, 0.f, 1.f);
    for(int i = 0; i < rig->numBones; i++)
    {
        const rigpose::Rigid k1 = rigpose::key(*rig, pose1, i), k2 = rigpose::key(*rig, pose2, i);
        posed[i].rot = glm::slerp(k1.rot, k2.rot, b);
        posed[i].pos = glm::mix(k1.pos, k2.pos, b);
    }
    if(rig->authored)
    {
        rig2::posedSmooth(e, hdr, *rig, pose1, pose2, blend, posed); // (drawn from these: r_lerpmodels 3's spline)
    }
    // The Rig Studio's animations (vr_rigseq.cpp; kickInstance draws them so): its bones from them, the layers below on top.
    bool layerArms = false;
    // (the keys' torso: the chest on the hips as keyed against as the frame has it; the upper body's hold keeps it)
    rigpose::Rigid layerTorso;
    bool layered = false;
    if(rigseq::wants(e))
    {
        rigpose::Rigid F[ragdoll::maxBones];
        if(rigseq::bones(e, *rig, hdr, pose1, pose2, blend, posed, F, &layerArms))
        {
            int hips = spec.chest;
            for(int n = 0; hips >= 0 && rig->bones[hips].parent >= 0 && n < ragdoll::maxBones; n++)
            {
                hips = rig->bones[hips].parent;
            }
            if(spec.chest >= 0 && hips >= 0 && hips != spec.chest)
            {
                const rigpose::Rigid was = composeRigid(inverseRigid(posed[hips]), posed[spec.chest]);
                const rigpose::Rigid now = composeRigid(inverseRigid(F[hips]), F[spec.chest]);
                layerTorso = composeRigid(inverseRigid(was), now);
                layered = true;
            }
            for(int i = 0; i < rig->numBones; i++)
            {
                posed[i] = F[i];
            }
        }
    }
    // (the strafe lean's measure, vr_agrunt_lean_info: its pelvis's roll by it, its chest's as solved)
    const auto rollOf = [](const rigpose::Rigid& r) {
        const glm::vec3 u = r.rot * glm::vec3{0.f, 0.f, 1.f};
        return za::atan2(-u.y, u.z) * (180.f / 3.14159265f);
    };
    const auto yawOf = [](const rigpose::Rigid& r) {
        const glm::vec3 f = r.rot * glm::vec3{1.f, 0.f, 0.f};
        return za::atan2(f.y, f.x) * (180.f / 3.14159265f);
    };
    float leanPelvisYaw = 0.f;
    int leanHips = spec.chest;
    for(int n = 0; leanHips >= 0 && rig->bones[leanHips].parent >= 0 && n < ragdoll::maxBones; n++)
    {
        leanHips = rig->bones[leanHips].parent;
    }
    float leanPelvis = 0.f;
    {
        // A take's kneel (vr_aglegs: its legs posed, the body above carried by its pelvis, the arms solved on it)
        float yawDeg = 0.f;
        const glm::vec3 origin = drawnOrigin(e, &yawDeg);
        rigpose::Rigid legs[ragdoll::maxBones];
        aglegs::Take take;
        take.w = s.take;
        take.side = s.takeSide;
        const int hs = s.takeSide == 1 ? 1 : 0;
        if(s.take > 0.002f && spec.upper[hs] >= 0 && spec.fore[hs] >= 0 && spec.chest >= 0)
        {
            // (its hand reaching for the weapon lying, then taking it up: the pelvis bends as far as the hand needs)
            const ragdoll::Bone& u = rig->bones[spec.upper[hs]];
            const ragdoll::Bone& f = rig->bones[spec.fore[hs]];
            take.reach = true;
            take.target = hs ? s.left : s.right;
            take.shoulder = u.pivot;
            take.chest = rig->bones[spec.chest].pivot;
            take.armLen = glm::length(f.pivot - u.pivot) + glm::length(f.end - f.pivot);
            take.torso = s.torso;
        }
        if(aglegs::solve(e, hdr, *rig, spec.kind, posed, take, origin, yawDeg, za::max(ENTSCALE_DECODE(e->scale), 0.01f),
               legs))
        {
            for(int i = 0; i < rig->numBones; i++)
            {
                posed[i] = composeRigid(legs[i], posed[i]);
            }
        }
        // The strafe lean (vr_agrunt_strafe_lean): moving sideways, its pelvis and legs tipped toward the way it goes,
        // about the floor under it, on every bone; the upper body's hold below keeps the chest aimed on the hips.
        rigpose::Rigid leanTurn;
        if(aglegs::lean(e, *rig, origin, yawDeg, s.take > 0.002f, leanTurn))
        {
            const float before = leanHips >= 0 ? rollOf(posed[leanHips]) : 0.f;
            const float beforeYaw = leanHips >= 0 ? yawOf(posed[leanHips]) : 0.f;
            for(int i = 0; i < rig->numBones; i++)
            {
                posed[i] = composeRigid(leanTurn, posed[i]);
            }
            leanPelvis = leanHips >= 0 ? rollOf(posed[leanHips]) - before : 0.f;
            leanPelvisYaw = leanHips >= 0 ? std::remainder(yawOf(posed[leanHips]) - beforeYaw, 360.f) : 0.f;
        }
    }
    // The procedural upper body (vr_agrunt_upper: the chest held on the hips, not the frames'; a pickup's crouch keeps
    // the frames'): laid under everything else, drawn too (boneTurns); the targets placed from the chest's anchor move
    // with it.
    rigpose::Rigid upperCorr;
    const bool pickup = (s.flags & apReach) || ((s.flags & apBody) && (s.flags & apFixed));
    const bool uppered = agbody::upper(s.ent, *rig, spec.chest, posed, pickup, upperCorr, aglegs::twistNow(e));
    if(uppered)
    {
        layOnChest(*rig, spec.chest, upperCorr, posed);
        if(layered)
        {
            // (the Rig Studio's torso keys on the held chest: its turn on the hips as keyed, not taken out with the
            // frames')
            const rigpose::Rigid keyed = composeRigid(posed[spec.chest], layerTorso);
            layOnChest(*rig, spec.chest, composeRigid(keyed, inverseRigid(posed[spec.chest])), posed);
        }
    }
    aglegs::leanMeasured(e, leanPelvis, spec.chest >= 0 ? rollOf(posed[spec.chest]) : 0.f, leanPelvisYaw,
        spec.chest >= 0 ? std::remainder(yawOf(posed[spec.chest]) - yawOf(rigpose::key(*rig, 0, spec.chest)), 360.f) : 0.f);
    const glm::vec3 anchorShift = uppered ? rigpose::apply(upperCorr, s.anchorAt) - s.anchorAt : glm::vec3{0.f};
    // A strike's body (bodyTurn: the torso, hips and head turned as QC turned the hands' targets) under the arms; the
    // same turns drawn on the bones (boneTurns).
    rigpose::Rigid body[ragdoll::maxBones];
    const bool bodied = bodyTurn(*rig, posed, s, body);
    for(int i = 0; i < rig->numBones && bodied; i++)
    {
        posed[i] = composeRigid(body[i], posed[i]);
    }
    rigpose::Rigid kick[ragdoll::maxBones];
    const bool kicked = vr_hit_react.value == 1.f && hitreact::reacting(e) && hitreact::boneKicks(e, *rig, posed, kick);
    const rigpose::Rigid chestKick = kicked ? kick[spec.chest] : rigpose::Rigid{};
    const int kind = spec.kind;
    agarms::Targets t;
    const bool hasR = heldProp(s.prop);
    const int slot = hasR ? weapons::slotForModel(cl_entities[s.prop].model) : -1;
    if(hasR)
    {
        t.goal[0] = agarms::Goal::Frame;
        t.weapon[0].rot = frameRot(s.fw, s.up);
        t.weapon[0].pos = s.right;
        t.offset[0] = gripRigid(kind, slot, aggrip::Main);
    }
    else if(s.flags & apReach)
    {
        // (reaching for a weapon lying: its grip frame onto the weapon's handle, nothing drawn on it yet)
        t.goal[0] = agarms::Goal::Frame;
        t.weapon[0].rot = frameRot(s.fw, s.up);
        t.weapon[0].pos = s.right;
        t.offset[0] = gripRigid(kind, -1, aggrip::Main);
        t.reaching = true;
    }
    else
    {
        t.goal[0] = agarms::Goal::Point; // (its fists: a guard, a punch)
        t.weapon[0].pos = s.right;
    }
    int slotL = -1;
    glm::vec3 secondPoint{0.f}; // (the second grip's point, the weapon's frame: vr_agholds_report)
    if((s.flags & apLeftWeapon) && heldProp(s.propL))
    {
        slotL = weapons::slotForModel(cl_entities[s.propL].model);
        t.goal[1] = agarms::Goal::Frame;
        t.weapon[1].rot = frameRot(s.fwL, s.upL);
        t.weapon[1].pos = s.left;
        t.offset[1] = gripRigid(kind, slotL, aggrip::Off);
    }
    else if(s.flags & apLeft)
    {
        t.goal[1] = agarms::Goal::Point;
        t.weapon[1].pos = s.left;
    }
    else if(!(s.flags & apLeftFree) && hasR)
    {
        // A two-handed weapon's second grip (view::secondGrip: its first Grip hotspot, or a front handle, the Widowmaker's;
        // in the weapon's frame), or a Cup under the right hand. QC's one-handed hold (the Weapon Holds) sends apLeftFree;
        // its two-handed one apTwo: a weapon with neither cupped.
        int type = 0;
        if(view::secondGrip(cl_entities[s.prop].model, secondPoint, type) && type == 1)
        {
            t.secondAt = composeRigid(rigpose::Rigid{glm::quat{1.f, 0.f, 0.f, 0.f}, secondPoint}, gripRigid(kind, slot, aggrip::Second));
            t.second = true;
        }
        else if(type == 2 && (s.keyGun || !lowCarry(t)))
        {
            // (vr_agarms_arm_clear: a pistol carried low, pointing down, in one hand: the other cupping it there reached
            // across the body, its upper arm through the torso; unless its pose key chose On the Gun)
            t.cup = true;
        }
        if(!t.second && !t.cup && (s.flags & apTwo) && (s.keyGun || !lowCarry(t)))
        {
            t.cup = true; // (two-handed and no second grip: the off hand under and round the right on its handle)
        }
        t.keyGun = s.keyGun; // (its second grip out of reach: cupping the right hand instead, vr_agarms)
    }
    if(s.offKeyed && hasR && !(s.flags & (apLeft | apLeftWeapon)))
    {
        // The pose keys' off hand: its grip on the weapon moved and turned (the weapon's frame), the free hand's
        const auto rotOf = [](const glm::vec3& r) {
            const float a = glm::length(r);
            return a < 1e-4f ? glm::quat{1.f, 0.f, 0.f, 0.f} : glm::angleAxis(glm::radians(a), r / a);
        };
        t.offKeyed = true;
        t.gripOff = rigpose::Rigid{rotOf(s.gripTurn), s.gripPos};
        t.freeOff = rigpose::Rigid{rotOf(s.freeTurn), s.freePos};
    }
    t.stretch = s.stretch;
    if(uppered)
    {
        t.weapon[0].pos += anchorShift * s.wr;
        t.weapon[1].pos += anchorShift * s.wl;
    }
    if(bodied && !(s.flags & apFixed))
    {
        // (QC turned the targets about its guess of the waist, `s.waist`; the chest turns about its own joint: the
        // targets moved by the difference, a shift, so the arms ride the chest exactly. apFixed: a take's bend, the
        // targets where the lying weapon is: not moved)
        const rigpose::Rigid& c = body[spec.chest];
        const glm::vec3 shift = c.pos + c.rot * s.waist - s.waist;
        for(int side = 0; side < 2; side++)
        {
            t.weapon[side].pos += shift;
        }
    }
    for(int side = 0; side < 2 && kicked; side++)
    {
        t.weapon[side] = composeRigid(chestKick, t.weapon[side]); // (the torso's flinch carries the weapons)
    }
    if(bodied || (s.flags & apBody))
    {
        // (its elbows' poles in a strike: raised and drawn back in the wind-up, on its turned chest; the Pose Studio's
        // keys may send them with no turn: vr_agpose.hpp)
        const glm::quat cr = bodied ? body[spec.chest].rot : glm::quat{1.f, 0.f, 0.f, 0.f};
        t.pole[0] = cr * s.elbowR;
        t.pole[1] = cr * s.elbowL;
    }
    // (a target that is no place on its body: none solved, its weapons left where the server has them, never drawn far
    // off, at NaN or inside it: the note of 2026-10-24 03:11:45's invisible sword)
    for(int side = 0; side < 2; side++)
    {
        if(t.goal[side] != agarms::Goal::None && !placeable(t.weapon[side]))
        {
            return;
        }
    }
    posestudio::adjust(s.ent, t); // (the Pose Studio's drag: a weapon held at the controller)
    if(layerArms)
    {
        rigseq::adjustTargets(*rig, spec, posed, kicked ? kick : nullptr, t); // (the Rig Studio's arms: the hands on their bones)
    }
    agarms::Placed placed;
    if(!agarms::solve(e, hdr, *rig, spec, posed, kicked ? kick : nullptr, t, placed))
    {
        return;
    }
    if(rig->authored)
    {
        // drawn on the authored rig, its arms laid on its arm bones within their limits, its weapons carried
        // by its hands as drawn (vr_rig2_game.cpp)
        rig2::liveArmed(e, *rig, posed, kicked ? kick : nullptr, placed, pose1, pose2, blend, layerArms);
    }
    if(ArmPose* seen = armPoseOf(s.ent, false))
    {
        seen->seenFrame = host_framecount;
        seen->seenKind = kind;
        seen->seenSlot = slot;
        seen->seenLeft = t.second ? 1 : t.cup ? 2 : t.goal[1] == agarms::Goal::Frame ? 4 : t.goal[1] == agarms::Goal::Point ? 3 : 0;
        seen->seenHeld2 = placed.held2;
        seen->seenSecond = secondPoint;
    }
    if(posestudio::active() || rigstudio::wantsNotes())
    {
        posestudio::Note n;
        n.ent = s.ent;
        n.kind = kind;
        n.slot = slot;
        n.slotL = slotL;
        n.prop = hasR ? s.prop : 0;
        n.propL = placed.on[1] ? s.propL : 0;
        n.pose = (s.flags & apReach)                          ? "pickup (reaching)"
                 : (s.flags & apBody) && (s.flags & apFixed) ? "pickup (the bend)"
                 : (s.flags & apBody)                        ? "a strike"
                 : !hasR                                     ? "fists"
                 : vr_agrunt_freeze.value != 0.f             ? "idle, weapon ready (frozen)"
                                                             : "live";
        n.space.origin = drawnOrigin(e, &n.space.yawDeg);
        n.space.scale = za::max(ENTSCALE_DECODE(e->scale), 0.01f);
        n.t = t;
        n.placed = placed;
        n.bodyFlag = (s.flags & apBody) != 0;
        n.torso = n.bodyFlag ? s.torso : glm::vec3{0.f};
        n.elbowPole[0] = n.bodyFlag ? s.elbowR : glm::vec3{0.f};
        n.elbowPole[1] = n.bodyFlag ? s.elbowL : glm::vec3{0.f};
        n.waist = s.waist;
        if(posestudio::active())
        {
            posestudio::note(n);
        }
        rigstudio::note(n); // (the Rig Studio's display: its subject's IK targets and grips)
    }
    if(placed.on[0] && hasR && placeable(placed.weapon[0]))
    {
        placeProp(cl_entities[s.prop], e, placed.weapon[0], nullptr);
    }
    if(placed.on[1] && placeable(placed.weapon[1]))
    {
        placeProp(cl_entities[s.propL], e, placed.weapon[1], nullptr);
    }
    if(vr_agrunt_ik_debug.value >= 2.f && hasR)
    {
        const entity_t& p = cl_entities[s.prop];
        Con_Printf("agarms ent %d prop %d: want %.1f %.1f %.1f, placed %d %.1f %.1f %.1f, drawn at %.0f %.0f %.0f (it %.0f %.0f %.0f), "
                   "model %s\n",
            s.ent, s.prop, t.weapon[0].pos.x, t.weapon[0].pos.y, t.weapon[0].pos.z, placed.on[0] ? 1 : 0, placed.weapon[0].pos.x,
            placed.weapon[0].pos.y, placed.weapon[0].pos.z, p.origin[0], p.origin[1], p.origin[2], e->origin[0], e->origin[1],
            e->origin[2], p.model ? p.model->name : "none");
    }
    // (vr_agrunt_ik_err: how far its held weapons are drawn from it, most)
    for(const int num : {hasR ? s.prop : 0, placed.on[1] ? s.propL : 0})
    {
        if(num > 0)
        {
            const entity_t& p = cl_entities[num];
            ragErr.propFar = za::max(ragErr.propFar, glm::length(glm::vec3{p.origin[0], p.origin[1], p.origin[2]} - drawnOrigin(e, nullptr)));
        }
    }
    aggrip::noteSubject(kind, slot, slotL, viewDistance(e));
}

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
    const bool rigid = vr_agrunt_ik_rigid.value != 0.f || agarms::enabled(); // (both: a ragdoll's weapons on its drawn hands)
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
            if(agarms::enabled() && e->model && e->model->type == mod_alias && armedRigOf(e->model))
            {
                liveReplay(s, &cl_entities[s.ent], static_cast<const aliashdr_t*>(Mod_Extradata(e->model)));
            }
            continue;
        }
        if(liveHeld.on && liveHeld.ent == s.ent)
        {
            liveRelease(s, &cl_entities[s.ent]);
        }
        if(rigid && e->model && e->msgtime == cl.mtime[0] && ragdoll::sourceModel(e))
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
        if(agarms::enabled())
        {
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
            continue;
        }
        if(s.prop <= 0 || s.prop >= cl.num_entities)
        {
            continue;
        }
        // Its weapon drawn on the drawn body (the hands' IK reaches the same place: armsNow).
        entity_t& p = cl_entities[s.prop];
        if(!p.model || !(p.effects & heldEffect) || p.msgtime != cl.mtime[0])
        {
            continue;
        }
        float yaw = 0.f;
        (void)drawnOrigin(e, &yaw);
        // A hit reaction's or a guard's turns on its arms (boneTurns: the kick on the arms' IK, as it is drawn): its
        // weapons carried by its forearms' turns, not left floating where its hands were.
        rigpose::Rigid kick[ragdoll::maxBones];
        int foreR = -1, foreL = -1;
        const ragdoll::Rig* rig = ragdoll::rigFor(e->model);
        const bool rigged = rig && rig->numPoses == hdr->numposes && rig->numVerts == hdr->numverts;
        rigpose::Rigid posed[ragdoll::maxBones];
        if(rigged)
        {
            // (as kickInstance poses them)
            const float b = pose1 == pose2 ? 0.f : za::clamp(blend, 0.f, 1.f);
            for(int i = 0; i < rig->numBones; i++)
            {
                const rigpose::Rigid k1 = rigpose::key(*rig, pose1, i), k2 = rigpose::key(*rig, pose2, i);
                posed[i].rot = glm::slerp(k1.rot, k2.rot, b);
                posed[i].pos = glm::mix(k1.pos, k2.pos, b);
            }
            // Its weapons where its arms reach (a reload's, a pain's bent body moves its shoulders off a pose made upright;
            // a swing's arc out past the arm): drawn there, the hands' IK on them (armsNow reads s). (The rigid arms
            // reach and fit in their own solve.)
            if(!rigid)
            {
                reachIn(*rig, *armedRigOf(e->model), posed, s);
                misfitToHands(*armedRigOf(e->model), e->model, *rig, hdr, pose1, pose2,
                    pose1 == pose2 ? 0.f : za::clamp(blend, 0.f, 1.f), s);
            }
        }
        if(vr_hit_react.value == 1.f && rigged && reacting(e))
        {
            if(boneKicks(e, *rig, posed, kick))
            {
                const ArmedRig* ar = armedRigOf(e->model);
                foreR = boneNamed(*rig, ar->foreR);
                foreL = boneNamed(*rig, ar->foreL);
            }
        }
        if(rigid && rigged && relinkRigid(s, e, hdr, pose1, pose2, blend, posed, kick, foreR, foreL))
        {
            continue;
        }
        glm::vec3 right = s.right, fw = s.fw, up = s.up;
        if(foreR >= 0)
        {
            right = rigpose::apply(kick[foreR], right);
            fw = kick[foreR].rot * fw;
            up = kick[foreR].rot * up;
        }
        const glm::vec3 at = toWorld(e, right);
        const glm::vec3 a = weaponAngles(turnYaw(yaw, fw), turnYaw(yaw, up));
        for(int i = 0; i < 3; i++)
        {
            p.origin[i] = at[i];
            p.angles[i] = a[i];
        }
        // Dual wielding: its left hand's weapon at the left's target (the left arm's IK reaches it: armsNow's own target).
        if(s.propL > 0 && s.propL < cl.num_entities)
        {
            entity_t& pl = cl_entities[s.propL];
            if(pl.model && (pl.effects & heldEffect) && pl.msgtime == cl.mtime[0])
            {
                glm::vec3 left = s.left, fwL = s.fwL, upL = s.upL;
                if(foreL >= 0)
                {
                    left = rigpose::apply(kick[foreL], left);
                    fwL = kick[foreL].rot * fwL;
                    upL = kick[foreL].rot * upL;
                }
                const glm::vec3 atL = toWorld(e, left);
                const glm::vec3 aL = weaponAngles(turnYaw(yaw, fwL), turnYaw(yaw, upL));
                for(int i = 0; i < 3; i++)
                {
                    pl.origin[i] = atL[i];
                    pl.angles[i] = aL[i];
                }
            }
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
                Con_Printf("hitreact: ent %d struck on %s (part %d), %d bones turned\n", numberOf(e),
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
    const bool want = vr_agrunt_ik.value != 0.f && e && e->model && e->model->type == mod_alias && numberOf(e) > 0 &&
                      armedRigOf(e->model) != nullptr;
    if(!want && vr_agrunt_ik_debug.value >= 2.f && e && e->model && strstr(e->model->name, "vrgrunt"))
    {
        Con_Printf("agik ent %d: not wanted (%s)\n", numberOf(e), e->model->name);
    }
    return want;
}

bool bonesWanted(const entity_t* e)
{
    return (vr_hit_react.value == 1.f && reacting(e)) || armsWanted(e);
}

bool boneTurns(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, const void* aliashdr, int pose1,
    int pose2, float blend, rigpose::Rigid* out, rigpose::Rigid* pieces, bool* piecesOn)
{
    rigpose::Rigid arms[ragdoll::maxBones];
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    if(agarms::enabled() && armedRigOf(e->model))
    {
        // vr_agarms: the arms are the skin's (its rows after these: kickInstance); the bones only kicked.
        const bool kicked = reacting(e) && boneKicks(e, rig, posed, out);
        if(pieces && piecesOn)
        {
            *piecesOn = false;
        }
        if(!kicked)
        {
            for(int b = 0; b < rig.numBones; b++)
            {
                out[b] = rigpose::Rigid{};
            }
        }
        // (a take's kneel: the body carried by its pelvis, vr_aglegs; the upper body on it, vr_agrunt_upper; a strike's
        // body: bodyTurn; as relinkSkinned solved the arms on them)
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
        const bool uppered = agarms::active(e) && agbody::upperNow(numberOf(e), upperCorr);
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
    const bool armed = armsWanted(e) && armsNow(e, rig, posed, hdr, pose1, pose2, blend, arms);
    const bool kicked = reacting(e) && boneKicks(e, rig, posed, out);
    if(pieces && piecesOn)
    {
        // The rigid arms' pieces (their vertices drawn as these move their rest places: vr_animsmooth.cpp's rest data).
        *piecesOn = false;
        const ArmedRig* a = armedRigOf(e->model); // (whatever vr_agrunt_ik_rigid is now: its rest data may have pieces)
        const ArmModel* am = a ? armModelOf(*a, e->model, rig, hdr) : nullptr;
        if(am && (armed || kicked))
        {
            const ArmSolve* sol = armed ? armSolveOf(e) : nullptr;
            for(int p = 0; p < numPieces; p++)
            {
                const ArmSide& side = am->side[p / 2];
                rigpose::Rigid base = sol ? sol->pieces[p] : pieceNow(side, pose1, pose2, blend);
                if(!sol && armed)
                {
                    base = composeRigid(arms[side.foreBone], base);
                }
                pieces[p] = kicked ? composeRigid(out[side.foreBone], base) : base;
            }
            *piecesOn = true;
        }
    }
    if(!armed)
    {
        return kicked;
    }
    for(int b = 0; b < rig.numBones; b++)
    {
        out[b] = kicked ? composeRigid(out[b], arms[b]) : arms[b]; // (the arms on the grips, then the kick on them)
    }
    return true;
}

bool armSkinRow(const qmodel_t* model, const void* aliashdr, const ragdoll::Rig& rig, int v, glm::vec4& out)
{
    out = glm::vec4{0.f};
    const ArmedRig* a = agarms::enabled() ? armedRigOf(model) : nullptr;
    agarms::Spec spec;
    return a && specOf(*a, rig, static_cast<const aliashdr_t*>(aliashdr), spec) &&
           agarms::restRow(model, static_cast<const aliashdr_t*>(aliashdr), rig, spec, v, out);
}

int armedPiece(const qmodel_t* model, const void* aliashdr, const ragdoll::Rig& rig, int v, int& bone)
{
    const ArmedRig* a = vr_agrunt_ik_rigid.value != 0.f && !agarms::enabled() ? armedRigOf(model) : nullptr;
    const ArmModel* am = a ? armModelOf(*a, model, rig, static_cast<const aliashdr_t*>(aliashdr)) : nullptr;
    if(!am || v < 0 || static_cast<za::SizeT>(v) >= am->piece.size() || am->piece[static_cast<za::SizeT>(v)] < 0)
    {
        return -1;
    }
    const int p = am->piece[static_cast<za::SizeT>(v)];
    bone = am->side[p / 2].foreBone;
    return p;
}

void armPieceVerts(const entity_t* e, const void* aliashdr, const ragdoll::Rig& rig, int side, const int*& verts, int& n)
{
    verts = nullptr;
    n = 0;
    const ArmedRig* a = agarms::enabled() ? nullptr : armedRigOf(e->model); // (vr_agarms: its own numbers)
    const ArmModel* am = a ? armModelOf(*a, e->model, rig, static_cast<const aliashdr_t*>(aliashdr)) : nullptr;
    if(am && side >= 0 && side < 2)
    {
        verts = am->side[side].all.data();
        n = static_cast<int>(am->side[side].all.size());
    }
}

void noteArmsDrawn(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, const rigpose::Rigid* kick,
    const glm::vec3* drawnR, const glm::vec3* drawnL)
{
    const int tag = static_cast<int>(vr_agrunt_ik_tag.value);
    const ArmedRig* a = armedRigOf(e->model);
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e->model));
    const ArmModel* am = a && hdr ? armModelOf(*a, e->model, rig, hdr) : nullptr;
    const ArmPose* ap = armPoseNow(e);
    if(tag < 0 || tag >= errTags || !am || !ap)
    {
        return;
    }
    const int kind = static_cast<int>(a - armedRigs);
    const auto turned = [&](int b, const glm::vec3& p) {
        const glm::vec3 q = rigpose::apply(posed[b], p);
        return kick ? rigpose::apply(kick[b], q) : q;
    };
    for(int s = 0; s < 2; s++)
    {
        const ArmSide& side = am->side[s];
        const glm::vec3* d = s == 0 ? drawnR : drawnL;
        const int n = static_cast<int>(side.all.size());
        if(!d || n < 3)
        {
            continue;
        }
        ArmErr& r = armErrs.at[kind][tag][s];
        r.n++;
        // The forearm and the hand each as a rigid piece: how far their worst vertex is off their rest shape; the
        // forearm's elbow and wrist.
        const int nf = static_cast<int>(side.fore.size());
        const rigpose::Rigid f = fitRigid(side.rest.data(), d, nf);
        const rigpose::Rigid h = fitRigid(side.rest.data() + nf, d + nf, n - nf);
        float shape = 0.f;
        for(int i = 0; i < n; i++)
        {
            shape = za::max(shape, glm::length(rigpose::apply(i < nf ? f : h, side.rest[static_cast<za::SizeT>(i)]) - d[i]));
        }
        r.shapeMax = za::max(r.shapeMax, shape);
        float sumD = 0.f, sumR = 0.f, most = 0.f;
        for(za::SizeT k = 0; k + 1 < side.edges.size(); k += 2)
        {
            const int i = side.edges[k], j = side.edges[k + 1];
            const float rl = glm::length(side.rest[static_cast<za::SizeT>(i)] - side.rest[static_cast<za::SizeT>(j)]);
            const float dl = glm::length(d[i] - d[j]);
            sumD += dl * dl;
            sumR += rl * rl;
            if(rl > 0.3f)
            {
                most = za::max(most, dl / rl);
            }
        }
        const float stretch = sumR > 0.f ? za::sqrt(sumD / sumR) : 1.f;
        r.stretchMax = za::max(r.stretchMax, stretch);
        r.stretchSum += stretch;
        r.edgeMax = za::max(r.edgeMax, most);
        const glm::vec3 S = turned(side.upper, rig.bones[side.upper].pivot);
        const glm::vec3 E = rigpose::apply(f, side.elbow);
        const glm::vec3 W = rigpose::apply(f, side.wrist);
        const glm::vec3 u = glm::normalize(S - E), w = glm::normalize(W - E);
        const float bend = 180.f - za::acos(za::clamp(glm::dot(u, w), -1.f, 1.f)) * (180.f / 3.14159265f);
        r.bendMax = za::max(r.bendMax, bend);
        r.bendMin = za::min(r.bendMin, bend);
        if(const ArmSolve* sol = armSolveOf(e))
        {
            r.wristMax = za::max(r.wristMax, sol->done[s].bend);
        }
        glm::vec3 grip{0.f};
        for(za::SizeT k = side.fore.size(); k < static_cast<za::SizeT>(n); k++)
        {
            grip += d[k];
        }
        grip /= static_cast<float>(side.hand.size());
        if(am->chest >= 0 && am->torso > 0.f)
        {
            const glm::vec3 c0 = turned(am->chest, rig.bones[am->chest].pivot);
            const glm::vec3 c1 = turned(am->chest, rig.bones[am->chest].end);
            for(const glm::vec3& p : {E, (E + W) * 0.5f, W, grip})
            {
                r.penMax = za::max(r.penMax, am->torso - segmentDistance(p, c0, c1));
            }
        }
        // Its weapon's grip (the weapon drawn, less its grip offset) against the hand's middle.
        const int prop = s == 0 ? ap->prop : ((ap->flags & apLeftWeapon) ? ap->propL : 0);
        if(prop > 0 && prop < cl.num_entities && (cl_entities[prop].effects & heldEffect))
        {
            const entity_t& p = cl_entities[prop];
            vec3_t ang{-p.angles[0], p.angles[1], p.angles[2]}, fw, rt, up;
            AngleVectors(ang, fw, rt, up);
            const glm::mat3 wr{glm::vec3{fw[0], fw[1], fw[2]}, -glm::vec3{rt[0], rt[1], rt[2]}, glm::vec3{up[0], up[1], up[2]}};
            const rigpose::Rigid O = offsetRigid(aggrip::offset(kind, weapons::slotForModel(p.model), s == 0 ? aggrip::Main : aggrip::Off));
            const glm::vec3 gripW = glm::vec3{p.origin[0], p.origin[1], p.origin[2]} +
                                    wr * (inverseRigid(O).pos * za::max(ENTSCALE_DECODE(e->scale), 0.01f));
            const float dg = glm::length(toWorld(e, grip) - gripW);
            r.nGrip++;
            r.gripMax = za::max(r.gripMax, dg);
            r.gripSum += dg;
        }
    }
}

bool armedHandVerts(const entity_t* e, const void* aliashdr, const ragdoll::Rig& rig, const int*& right, int& numRight,
    const int*& left, int& numLeft)
{
    const ArmedRig* a = armedRigOf(e->model);
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    if(!a || hdr->numverts != a->numVerts || rig.numVerts != hdr->numverts || agarms::enabled() ||
        (vr_agrunt_ik_rigid.value != 0.f && armModelOf(*a, e->model, rig, hdr)))
    {
        return false; // (the rigid arms' hands are drawn where solved: no fit, and the arm numbers are noteArmsDrawn's)
    }
    const int fr = boneNamed(rig, a->foreR);
    const int fl = boneNamed(rig, a->foreL);
    if(fl < 0 || fr < 0)
    {
        return false;
    }
    const LeftHand& rh = findLeftHand(*a, e->model, rig, hdr, fr, true);
    right = rh.verts;
    numRight = rh.num;
    const LeftHand& lh = findLeftHand(*a, e->model, rig, hdr, fl);
    left = lh.verts;
    numLeft = lh.num;
    return true;
}

int armedHandBone(const qmodel_t* model, const void* aliashdr, const ragdoll::Rig& rig, int v)
{
    const ArmedRig* a = armedRigOf(model);
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    if(!a || vr_agrunt_ik_hands.value <= 0.f || hdr->numverts != a->numVerts || rig.numVerts != hdr->numverts ||
        isGunVertex(*a, v) || agarms::enabled())
    {
        return -1;
    }
    const glm::vec3 p = poseVertex(hdr, 0, 0, 0.f, v);
    for(const char* name : {a->foreR, a->foreL})
    {
        const int fore = boneNamed(rig, name);
        if(fore >= 0 && glm::length(p - rigpose::apply(rigpose::key(rig, 0, fore), rig.bones[fore].end)) <= vr_agrunt_ik_hands.value)
        {
            return fore;
        }
    }
    return -1;
}

void setHandsDrawn(const entity_t* e, const HandFit& right, const HandFit& left)
{
    handsDrawn.e = e;
    handsDrawn.frame = host_framecount;
    handsDrawn.right = right;
    handsDrawn.left = left;
}

void noteHandsDrawn(const entity_t* e, const glm::vec3& right, const glm::vec3& left)
{
    handsDrawn.e = nullptr; // (used: the next entity sets its own)
    const int tag = static_cast<int>(vr_agrunt_ik_tag.value);
    const ArmedRig* a = armedRigOf(e->model);
    const ArmPose* ap = armPoseNow(e);
    if(tag < 0 || tag >= errTags || !a || !ap || (a - armedRigs) > 1)
    {
        return;
    }
    HandErr& h = handErrs.at[a - armedRigs][tag];
    const auto dist = [&](int prop, const glm::vec3& hand) {
        if(prop <= 0 || prop >= cl.num_entities || !(cl_entities[prop].effects & heldEffect))
        {
            return -1.f;
        }
        const entity_t& p = cl_entities[prop];
        return glm::length(toWorld(e, hand) - glm::vec3{p.origin[0], p.origin[1], p.origin[2]});
    };
    const float dr = dist(ap->prop, right);
    if(dr >= 0.f)
    {
        h.n++;
        h.sumR += dr;
        h.maxR = za::max(h.maxR, dr);
    }
    const float dl = dist(ap->propL, left);
    if(dl >= 0.f)
    {
        h.nL++;
        h.sumL += dl;
        h.maxL = za::max(h.maxL, dl);
    }
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

// vr_agrunt_ik_info: each armed grunt's arms solved now as drawn (its lerp's poses), an agik line each (the hands'
// distances to the grips before and after); drawing or not (the headless tests: QC vr_armedgrunt_test.qc asks it).
void armInfo_f()
{
    armIk.forcePrint = true;
    int grunts = 0;
    for(int i = 1; i < cl.num_entities; i++)
    {
        const entity_t* e = &cl_entities[i];
        if(!armsWanted(e))
        {
            continue;
        }
        grunts++;
        const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e->model));
        const ragdoll::Rig* rig = ragdoll::rigFor(e->model);
        const int p1 = e->previouspose, p2 = e->currentpose;
        if(!rig || rig->numPoses != hdr->numposes || p1 < 0 || p2 < 0 || p1 >= hdr->numposes || p2 >= hdr->numposes)
        {
            Con_Printf("agik ent %d: not posed (rig %d, poses %d %d of %d)\n", i, rig ? 1 : 0, p1, p2, hdr->numposes);
            continue;
        }
        const float blend = e->lerptime > 0.f ? za::clamp((static_cast<float>(cl.time) - e->lerpstart) / e->lerptime, 0.f, 1.f) : 1.f;
        rigpose::Rigid posed[ragdoll::maxBones];
        for(int b = 0; b < rig->numBones; b++)
        {
            const rigpose::Rigid k1 = rigpose::key(*rig, p1, b), k2 = rigpose::key(*rig, p2, b);
            posed[b].rot = glm::slerp(k1.rot, k2.rot, blend);
            posed[b].pos = glm::mix(k1.pos, k2.pos, blend);
        }
        rigpose::Rigid arms[ragdoll::maxBones];
        armsNow(e, *rig, posed, hdr, p1, p2, blend, arms);
    }
    armIk.forcePrint = false;
    if(vr_agrunt_ik_debug.value >= 2.f)
    {
        Con_Printf("agik: %d armed grunts of %d entities\n", grunts, cl.num_entities);
    }
}

// vr_agrunt_ik_err: each armed rig's (0 the grunt, 1 the enforcer) drawn hands' distances to their weapons per
// vr_agrunt_ik_tag since the last time (units): "agerr <rig> <tag> n <frames> right max <> mean <> left max <> mean <>".
// vr_agrunt_grip_fit: the Grip Editor's angles set so the nearest armed monster's wrist (vr_agrunt_grip_hand's) is
// unturned in its pose now (its weapon as the server turns it, its hand as the animation does): a starting point.
void gripFit_f()
{
    const ArmSolve* best = nullptr;
    float bestD = 1e30f;
    const int hand = static_cast<int>(vr_agrunt_grip_hand.value) == 1 ? 1 : 0;
    for(const ArmSolve& s : armSolves.at)
    {
        if(s.e && s.frame >= host_framecount - 1 && s.fitOk[hand] && viewDistance(s.e) < bestD)
        {
            bestD = viewDistance(s.e);
            best = &s;
        }
    }
    if(!best || static_cast<int>(vr_agrunt_grip_hand.value) == 2)
    {
        Con_Printf("vr_agrunt_grip_fit: no armed monster near holding a weapon in that hand (or the second grip: not fitted)\n");
        return;
    }
    const glm::vec3 a = best->fitAngles[hand];
    Cvar_SetValueQuick(&vr_agrunt_grip_pitch, za::floor(a.x * 2.f + 0.5f) * 0.5f);
    Cvar_SetValueQuick(&vr_agrunt_grip_yaw, za::floor(a.y * 2.f + 0.5f) * 0.5f);
    Cvar_SetValueQuick(&vr_agrunt_grip_roll, za::floor(a.z * 2.f + 0.5f) * 0.5f);
    Con_Printf("vr_agrunt_grip_fit: pitch %.1f yaw %.1f roll %.1f\n", a.x, a.y, a.z);
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

void armErr_f()
{
    if(agarms::enabled())
    {
        agarms::printErr(); // (vr_agarms: its numbers; counted afresh after)
        agarms::clearErr();
        Con_Printf("agprop: held weapons drawn at most %.1f units from their holders\n", ragErr.propFar);
        ragErr.propFar = 0.f;
    }
    float most[2] = {0.f, 0.f};
    for(int r = 0; r < 2; r++)
    {
        for(int t = 0; t < errTags; t++)
        {
            HandErr& h = handErrs.at[r][t];
            if(h.n == 0 && h.nL == 0)
            {
                continue;
            }
            Con_Printf("agerr %d %d n %d right max %.2f mean %.2f left n %d max %.2f mean %.2f\n", r, t, h.n, h.maxR,
                h.n ? h.sumR / static_cast<float>(h.n) : 0.f, h.nL, h.maxL, h.nL ? h.sumL / static_cast<float>(h.nL) : 0.f);
            most[0] = za::max(most[0], h.maxR);
            most[1] = za::max(most[1], h.maxL);
            h = HandErr{};
        }
    }
    Con_Printf("agerr: most right %.2f, left %.2f units\n", most[0], most[1]);
    if(ragErr.n > 0)
    {
        Con_Printf("agrag: %d ragdoll frames: the server's weapon off its drawn hand's grip max %.2f mean %.2f units, turned "
                   "%.0f deg (drawn on the hand instead)\n",
            ragErr.n, ragErr.offMax, ragErr.offSum / static_cast<float>(ragErr.n), ragErr.turnMax);
        ragErr = RagErr{};
    }
    // The arms' numbers (noteArmsDrawn): each tag's, then each rig's and side's worst.
    const bool each = Cmd_Argc() > 1 && atoi(Cmd_Argv(1)) > 0;
    for(int r = 0; r < 2; r++)
    {
        for(int s = 0; s < 2; s++)
        {
            ArmErr all;
            int tags = 0;
            for(int t = 0; t < errTags; t++)
            {
                ArmErr& h = armErrs.at[r][t][s];
                if(h.n == 0)
                {
                    continue;
                }
                tags++;
                if(each)
                {
                    Con_Printf("agarm %d %d %s n %d grip %.2f stretch %.2f edge %.2f shape %.2f bend %.0f..%.0f pen %.2f wrist %.0f\n",
                        r, t, s == 0 ? "R" : "L", h.n, h.gripMax, h.stretchMax, h.edgeMax, h.shapeMax, h.bendMin, h.bendMax,
                        h.penMax, h.wristMax);
                }
                all.n += h.n;
                all.nGrip += h.nGrip;
                all.gripMax = za::max(all.gripMax, h.gripMax);
                all.gripSum += h.gripSum;
                all.stretchMax = za::max(all.stretchMax, h.stretchMax);
                all.stretchSum += h.stretchSum;
                all.edgeMax = za::max(all.edgeMax, h.edgeMax);
                all.shapeMax = za::max(all.shapeMax, h.shapeMax);
                all.bendMax = za::max(all.bendMax, h.bendMax);
                all.bendMin = za::min(all.bendMin, h.bendMin);
                all.penMax = za::max(all.penMax, h.penMax);
                all.wristMax = za::max(all.wristMax, h.wristMax);
                h = ArmErr{};
            }
            if(all.n == 0)
            {
                continue;
            }
            Con_Printf("agarms %s %s: %d tags, %d frames: hand to grip max %.2f mean %.2f; stretch max %.2f mean %.3f, "
                       "longest edge %.2fx; shape off %.2f; elbow bend %.0f..%.0f deg; in the torso %.2f; wrist turn wanted %.0f deg\n",
                r == 0 ? "grunt" : "enforcer", s == 0 ? "right" : "left", tags, all.n, all.gripMax,
                all.nGrip ? all.gripSum / static_cast<float>(all.nGrip) : 0.f, all.stretchMax,
                all.stretchSum / static_cast<float>(all.n), all.edgeMax, all.shapeMax, all.bendMin, all.bendMax,
                za::max(all.penMax, 0.f), all.wristMax);
        }
    }
}

} // namespace

void resetModels()
{
    for(int i = 0; i < numArmedRigs; i++)
    {
        armModels[i] = ArmModel{};
        misfits[i] = Misfit{};
        armIk.left[i] = LeftHand{};
        armIk.right[i] = LeftHand{};
    }
}

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
        Con_Printf("ragweapon: %d passed, %d failed\n", ragWeaponTally.passed, ragWeaponTally.failed);
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
    Cmd_AddCommand("vr_agrunt_ik_info", armInfo_f);
    Cmd_AddCommand("vr_agrunt_ik_err", armErr_f);
    Cmd_AddCommand("vr_agholds_report", holdsReport_f);
    Cmd_AddCommand("vr_agrunt_grip_fit", gripFit_f);
    aggrip::registerCommands();
    agarms::registerCommands();
    aglegs::registerCommands();
}

} // namespace qvr::hitreact
