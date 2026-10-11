// vr_hitreact.cpp -- living hit reactions (vr_hitreact.hpp; ROUND22.md, "Active ragdolls: living hit reactions").

#include "vr_hitreact.hpp"
#include "vr_animsmooth.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_mem.hpp"

#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Math/Acos.hpp"
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
};

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

// The entity's space (its yaw and scale; a monster has no pitch or roll): world -> model units.
[[nodiscard]] glm::vec3 toLocal(const entity_t* e, const glm::vec3& v, bool point)
{
    glm::vec3 d = point ? v - glm::vec3{e->origin[0], e->origin[1], e->origin[2]} : v;
    const float yaw = e->angles[1] * (3.14159265f / 180.f);
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
    return it != reacts.entities.end() && it->second.count > 0 ? &it->second : nullptr;
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

} // namespace

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
            if(cl.time - r.kicks[i].start < over)
            {
                r.kicks[kept++] = r.kicks[i];
            }
        }
        r.count = kept;
    }
    erase_if(reacts.entities, [](const auto& kv) { return kv.second.count == 0; });
    Reaction& r = reacts.entities[num];
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
    if(vr_hit_react_debug.value >= 1.f)
    {
        Con_Printf("hitreact: ent %d part %d damage %d share %.2f size %.2f at (%.0f %.0f %.0f) dir (%.2f %.2f %.2f)\n", num,
            part, amount, share, size, at.x, at.y, at.z, dir.x, dir.y, dir.z);
    }
}

bool boneKicks(const entity_t* e, const ragdoll::Rig& rig, const rigpose::Rigid* posed, rigpose::Rigid* out)
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
    traceLine(e, *r, &rig, total, 0.f);
    if(!any)
    {
        return false;
    }
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

void tiltModel(const entity_t* e, float matrix[16])
{
    Reaction* r = reactionOf(e);
    if(!r || ragdoll::sourceModel(e) || (vr_hit_react.value == 1.f && animsmooth::rigged(e)))
    {
        return;
    }
    glm::vec3 total{0.f};
    for(int i = 0; i < r->count; i++)
    {
        Kick& k = r->kicks[i];
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
    traceLine(e, *r, nullptr, nullptr, angle * (180.f / 3.14159265f));
    if(angle < 1e-6f)
    {
        return;
    }
    const glm::vec3 feet{0.f, 0.f, e->model->mins[2]};
    const glm::mat4 turn = glm::mat4_cast(rigpose::fromVector(total));
    glm::mat4 m = glm::make_mat4(matrix);
    m = m * glm::translate(glm::mat4{1.f}, feet) * turn * glm::translate(glm::mat4{1.f}, -feet);
    memcpy(matrix, glm::value_ptr(m), sizeof(float) * 16);
}

namespace
{

// vr_hit_react_info: the entities reacting now and their kicks.
void info_f()
{
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

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_hit_react_info", info_f);
}

} // namespace qvr::hitreact
