// vr_hitreact.cpp -- living hit reactions (vr_hitreact.hpp; ROUND22.md, "Active ragdolls: living hit reactions").

#include "vr_hitreact.hpp"
#include "vr_animsmooth.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_mem.hpp"
#include "vr_view.hpp"
#include "vr_render.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Base/GetArraySize.hpp"

#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
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
// Armed grunts' arms on their weapon (armsWanted; QC vr_armedgrunt.qc)

constexpr int heldEffect = 128;                // QC VR_AG_EF_HELD: a weapon in a monster's hand
constexpr float propReach = 28.f;              // units: its weapon is the held one nearest its right hand within this
constexpr int rightHandVerts[] = {32, 26, 31}; // QC VR_AG_HAND_V*: where the server puts its weapon's grip
constexpr int gunFirst = 463, gunLast = 548;   // its hidden rifle's vertices (make_vrgrunt.py): never a hand's
constexpr int maxProps = 32;
constexpr int maxHandVerts = 16;
constexpr float hangReach = 0.95f;             // a free arm's hand this share of its reach below its shoulder
constexpr glm::vec3 hangDir{0.15f, 0.12f, -1.f}; // ... that way (model space: a little ahead and out, its left)
constexpr glm::vec3 hangElbow{-1.f, 0.3f, 0.f};  // ... its elbow back
constexpr glm::vec3 cupOffset{0.f, 1.2f, -2.5f}; // a Cup grip: the left hand under and round the right (model units)

struct ArmIk
{
    int frame{-1};                  // host_framecount the held weapons were gathered on
    int numProps{0};
    int props[maxProps]{};          // their cl_entities numbers
    const qmodel_t* model{nullptr}; // the model the left hand's vertices were found on
    int leftVerts[maxHandVerts]{};
    int numLeft{0};
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
// first pose (none: the 4 nearest), found once a model.
void findLeftHand(const qmodel_t* model, const ragdoll::Rig& rig, const aliashdr_t* hdr, int fl)
{
    if(armIk.model == model)
    {
        return;
    }
    armIk.model = model;
    armIk.numLeft = 0;
    const glm::vec3 end = rigpose::apply(rigpose::key(rig, 0, fl), rig.bones[fl].end);
    for(float within = 3.f; within < 12.f && armIk.numLeft < 4; within += 1.f)
    {
        armIk.numLeft = 0;
        for(int v = 0; v < hdr->numverts; v++)
        {
            if((v >= gunFirst && v <= gunLast) || armIk.numLeft >= maxHandVerts)
            {
                continue;
            }
            if(glm::length(poseVertex(hdr, 0, 0, 0.f, v) - end) <= within)
            {
                armIk.leftVerts[armIk.numLeft++] = v;
            }
        }
    }
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
constexpr int gunRear[] = {497, 545};   // QC VR_AG_GUN_*: its hidden rifle's line (the carry's turn)
constexpr int gunMuzzle[] = {498, 514};
constexpr int gunTop = 529, gunBottom = 496;
constexpr double armPoseStale = 0.3; // seconds without a message: back to finding its weapon by its hand
constexpr int apLeft = 2;            // the left hand to its own target (else on the weapon's second grip, or free)
constexpr int apLeftFree = 4;        // ... the left as the animation has it
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

// Its arm pose's targets now (`s` fresh), on the poses drawn: s.right, s.left, s.fw, s.up. False: none now.
bool placeArmPose(const entity_t* e, const aliashdr_t* hdr, int pose1, int pose2, float blend, ArmPose& s)
{
    if(hdr->numverts <= gunLast || s.cur.t < cl.mtime[0] - armPoseStale)
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
    }
    const float scale = za::max(ENTSCALE_DECODE(e->scale), 0.01f);
    glm::vec3 hr{0.f};
    for(const int v : rightHandVerts)
    {
        hr += poseVertex(hdr, pose1, pose2, blend, v) / static_cast<float>(za::getArraySize(rightHandVerts));
    }
    // Its drawn rifle's line: the carry's turn.
    const glm::vec3 rear = (poseVertex(hdr, pose1, pose2, blend, gunRear[0]) + poseVertex(hdr, pose1, pose2, blend, gunRear[1])) * 0.5f;
    const glm::vec3 muzzle =
        (poseVertex(hdr, pose1, pose2, blend, gunMuzzle[0]) + poseVertex(hdr, pose1, pose2, blend, gunMuzzle[1])) * 0.5f;
    glm::vec3 gx = muzzle - rear;
    gx = glm::length(gx) > 1e-4f ? glm::normalize(gx) : glm::vec3{1.f, 0.f, 0.f};
    glm::vec3 gz = poseVertex(hdr, pose1, pose2, blend, gunTop) - poseVertex(hdr, pose1, pose2, blend, gunBottom);
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
    s.right = hr * m.wr + m.v / scale;
    s.fw = fw;
    s.up = up;
    s.prop = m.prop;
    s.flags = m.flags;
    s.left = m.vl / scale;
    if((m.flags & apLeft) && m.wl > 0.f)
    {
        const ragdoll::Rig* rig = ragdoll::rigFor(e->model);
        const int fl = rig ? boneNamed(*rig, "forearm_l") : -1;
        if(fl >= 0)
        {
            findLeftHand(e->model, *rig, hdr, fl);
            glm::vec3 hl{0.f};
            for(int i = 0; i < armIk.numLeft; i++)
            {
                hl += poseVertex(hdr, pose1, pose2, blend, armIk.leftVerts[i]) / static_cast<float>(za::max(armIk.numLeft, 1));
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

// The arms' turns now (armsWanted), out: every bone's (none but the arms' turned). False: not now (dead, no weapon).
bool armsNow(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, const aliashdr_t* hdr, int pose1,
    int pose2, float blend, rigpose::Rigid* out)
{
    if(hdr->numverts <= gunLast || rig.numVerts != hdr->numverts)
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
                Con_Printf("agik ent %d: pose %d a death's\n", numberOf(e), pose1);
            }
            return false; // (dying: its weapon dropped)
        }
    }
    const int ur = boneNamed(rig, "upperarm_r"), fr = boneNamed(rig, "forearm_r");
    const int ul = boneNamed(rig, "upperarm_l"), fl = boneNamed(rig, "forearm_l");
    if(ur < 0 || fr < 0 || ul < 0 || fl < 0)
    {
        if(vr_agrunt_ik_debug.value >= 2.f)
        {
            Con_Printf("agik ent %d: no arm bones\n", numberOf(e));
        }
        return false;
    }
    glm::vec3 hr{0.f};
    for(const int v : rightHandVerts)
    {
        hr += poseVertex(hdr, pose1, pose2, blend, v) / static_cast<float>(za::getArraySize(rightHandVerts));
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
    const float mainAfter = solveArm(rig, posed, ur, fr, hr, tr, er, out);

    // The left hand on its second grip, under the right one (a Cup), or hanging free.
    findLeftHand(e->model, rig, hdr, fl);
    if(armIk.numLeft == 0)
    {
        if(vr_agrunt_ik_debug.value >= 2.f)
        {
            Con_Printf("agik ent %d: no left hand found (main %.2f -> %.2f)\n", numberOf(e), mainBefore, mainAfter);
        }
        return true;
    }
    glm::vec3 hl{0.f};
    for(int i = 0; i < armIk.numLeft; i++)
    {
        hl += poseVertex(hdr, pose1, pose2, blend, armIk.leftVerts[i]) / static_cast<float>(armIk.numLeft);
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
    const float secondAfter = kind == 4 ? secondBefore : solveArm(rig, posed, ul, fl, hl, tl, pole, out);
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

} // namespace

namespace qvr::hitreact
{

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

void relinkArms()
{
    if(!vr_agrunt_ik.value)
    {
        return;
    }
    for(ArmPose& s : armPoses.slots)
    {
        if(s.ent <= 0 || s.ent >= cl.num_entities || s.cur.t < cl.mtime[0] - armPoseStale)
        {
            continue;
        }
        const entity_t* e = &cl_entities[s.ent];
        if(!e->model || e->model->type != mod_alias || e->msgtime != cl.mtime[0] ||
            !modelmeta::get(e->model).is(modelmeta::Id::VrGrunt))
        {
            continue;
        }
        const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e->model));
        int pose1 = 0, pose2 = 0;
        float blend = 0.f;
        drawnPoses(e, hdr, pose1, pose2, blend);
        if(!placeArmPose(e, hdr, pose1, pose2, blend, s) || s.prop <= 0 || s.prop >= cl.num_entities)
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
        const glm::vec3 at = toWorld(e, s.right);
        const glm::vec3 a = weaponAngles(turnYaw(yaw, s.fw), turnYaw(yaw, s.up));
        for(int i = 0; i < 3; i++)
        {
            p.origin[i] = at[i];
            p.angles[i] = a[i];
        }
    }
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
                      modelmeta::get(e->model).is(modelmeta::Id::VrGrunt);
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
    int pose2, float blend, rigpose::Rigid* out)
{
    rigpose::Rigid arms[ragdoll::maxBones];
    const bool armed = armsWanted(e) &&
                       armsNow(e, rig, posed, static_cast<const aliashdr_t*>(aliashdr), pose1, pose2, blend, arms);
    const bool kicked = reacting(e) && boneKicks(e, rig, posed, out);
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

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_hit_react_info", info_f);
    Cmd_AddCommand("vr_agrunt_ik_info", armInfo_f);
}

} // namespace qvr::hitreact
