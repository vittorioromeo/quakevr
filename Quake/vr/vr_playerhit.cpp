// vr_playerhit.cpp -- shots meet the other players' bodies as they are drawn: see vr_playerhit.hpp.

#include "vr_playerhit.hpp"

#include "vr_avatar.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_hitmodel.hpp"
#include "vr_mpavatar.hpp"
#include "vr_region_shared.h"
#include "vr_progs.hpp"
#include "vr_server.hpp"
#include "vr_testkit.hpp"
#include "vr_tunables.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/geometric.hpp>

namespace qvr::playerhit
{

namespace
{

// The drawn body's proportions (vrbody.mdl at its model eye height, 1.646 m; measured on a passive bot's posed skeleton,
// vr_mp_avatars' mp_body, m2w 31.5: keep in step with the body), metres from his drawn eyes along his facing (f), his left
// (l) and up.
constexpr float headBack = 0.075f, headUp = 0.03f, headRadius = 0.13f; // the skull's middle; the helmet round it
constexpr float neckBack = 0.11f, neckDown = 0.225f;                   // the shoulders' line (mp_body's shoulders)
constexpr float shoulderSide = 0.19f;
constexpr float pelvisBack = 0.15f, pelvisDown = 0.70f;               // mp_body's pelvis
constexpr float torsoRadius = 0.14f;
// (the forearm on to the hand's controller, a held weapon's grip: 0.27 to the wrist and 0.19 past it, measured on the bot)
constexpr float upperArm = 0.29f, foreArm = 0.45f, armRadius = 0.055f;
constexpr float hipSide = 0.09f, footBack = 0.15f, ankleUp = 0.09f;    // mp_body's ankles: behind the pelvis, over the sole
constexpr float legLength = 0.88f, legRadius = 0.07f;                  // hip to ankle
constexpr float lowestPelvis = 0.3f;                                    // over the floor: a crouch's lowest

// A two-bone limb from `root` to `tip` (lengths `a`, `b`): its joint, bent towards `pole` (a direction) when the limb is
// shorter than its bones; straight (on the line) when it reaches.
glm::vec3 joint(const glm::vec3& root, const glm::vec3& tip, float a, float b, const glm::vec3& pole)
{
    const glm::vec3 d = tip - root;
    const float len = glm::length(d);
    if(len < 1e-3f)
    {
        return root + pole * a;
    }
    const glm::vec3 dir = d / len;
    if(len >= a + b)
    {
        return root + dir * (len * a / (a + b));
    }
    const float along = za::clamp((a * a - b * b + len * len) / (2.f * len), -a, a);
    const float out = za::sqrt(za::max(0.f, a * a - along * along));
    glm::vec3 side = pole - dir * glm::dot(pole, dir);
    side = glm::length(side) > 1e-3f ? glm::normalize(side) : glm::vec3{0.f, 0.f, -1.f};
    return root + dir * along + side * out;
}

// The first t (distance along the unit `rd` from `ro`) at which the ray meets the capsule pa..pb of radius ra; < 0 none.
// A start inside: 0. (Inigo Quilez's capsule intersection, the inside case and a ray along the axis added.)
float capsuleHit(const glm::vec3& ro, const glm::vec3& rd, const glm::vec3& pa, const glm::vec3& pb, float ra)
{
    const glm::vec3 ba = pb - pa, oa = ro - pa;
    const float baba = glm::dot(ba, ba);
    {
        const float s = baba > 1e-6f ? za::clamp(glm::dot(oa, ba) / baba, 0.f, 1.f) : 0.f;
        if(glm::length(oa - ba * s) <= ra)
        {
            return 0.f;
        }
    }
    const float bard = glm::dot(ba, rd), baoa = glm::dot(ba, oa), rdoa = glm::dot(rd, oa), oaoa = glm::dot(oa, oa);
    const float a = baba - bard * bard;
    if(a > 1e-6f)
    {
        const float b = baba * rdoa - baoa * bard, c = baba * oaoa - baoa * baoa - ra * ra * baba;
        const float h = b * b - a * c;
        if(h < 0.f)
        {
            return -1.f;
        }
        const float t = (-b - za::sqrt(h)) / a;
        const float y = baoa + t * bard;
        if(y > 0.f && y < baba)
        {
            return t;
        }
    }
    // The caps: the nearer end's sphere first, then the other's.
    float best = -1.f;
    for(const glm::vec3& centre : {pa, pb})
    {
        const glm::vec3 oc = ro - centre;
        const float b = glm::dot(rd, oc), c = glm::dot(oc, oc) - ra * ra;
        const float h = b * b - c;
        if(h > 0.f)
        {
            const float t = -b - za::sqrt(h);
            best = t >= 0.f && (best < 0.f || t < best) ? t : best;
        }
    }
    return best;
}

} // namespace

void build(const Pose& p, const glm::vec3& origin, Shape& out)
{
    const float k = p.m2w;
    const float yaw = p.yaw * (3.14159265f / 180.f);
    const glm::vec3 f{za::cos(yaw), za::sin(yaw), 0.f}, l{-f.y, f.x, 0.f}, up{0.f, 0.f, 1.f};
    const glm::vec3 eye = origin + p.head;
    const float floorZ = origin.z + p.floor;
    const glm::vec3 neck = eye - f * (neckBack * k) - up * (neckDown * k);
    glm::vec3 pelvis = eye - f * (pelvisBack * k) - up * (pelvisDown * k);
    pelvis.z = za::max(pelvis.z, za::min(neck.z, floorZ + lowestPelvis * k)); // (crouched: no lower than a crouch)
    out.n = 0;
    const auto add = [&](const glm::vec3& a, const glm::vec3& b, float r, int zone)
    { out.c[out.n++] = Capsule{a, b, r * k, zone}; };
    add(eye - f * (headBack * k) + up * (headUp * k), eye - f * (headBack * k) + up * (headUp * k), headRadius, VR_REGION_HEAD);
    add(neck, pelvis, torsoRadius, VR_REGION_BODY);
    for(int h = 0; h < 2; h++)
    {
        const float side = h == 0 ? 1.f : -1.f; // (the off hand the left)
        const glm::vec3 shoulder = neck + l * (side * shoulderSide * k);
        const glm::vec3 hand = p.hasHand[h] ? origin + p.hand[h] : shoulder - up * ((upperArm + foreArm) * k);
        const glm::vec3 elbow = joint(shoulder, hand, upperArm * k, foreArm * k, glm::normalize(l * (side * 0.3f) - up));
        add(shoulder, elbow, armRadius, VR_REGION_ARM);
        add(elbow, hand, armRadius, VR_REGION_ARM);
        const glm::vec3 hip = pelvis + l * (side * hipSide * k);
        glm::vec3 ankle = pelvis - f * (footBack * k) + l * (side * hipSide * k);
        ankle.z = floorZ + ankleUp * k;
        if(glm::length(ankle - hip) > legLength * k)
        {
            ankle = hip + glm::normalize(ankle - hip) * (legLength * k); // (off the ground: his legs hang)
        }
        const glm::vec3 knee = joint(hip, ankle, 0.5f * legLength * k, 0.5f * legLength * k, f);
        add(hip, knee, legRadius, VR_REGION_LEG);
        add(knee, ankle, legRadius, VR_REGION_LEG);
    }
}

bool serverShape(edict_t* ent, Shape& out)
{
    if(vr_hit_player_shape.value == 0.f || !sv.active || !ent || ent->free || ent->v.health <= 0.f ||
       static_cast<int>(ent->v.solid) == SOLID_NOT)
    {
        return false;
    }
    const progs::ServerVm vm; // (the view's Show Hit Zones too: its edicts numbered by the server's VM)
    const int slot = NUM_FOR_EDICT(ent) - 1;
    Pose p;
    if(slot < 0 || slot >= svs.maxclients || !server::avatarPose(slot, p))
    {
        return false;
    }
    build(p, glm::vec3{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]}, out);
    // (the leniency, vr_hit_player_fatten: every part a little thicker for what shoots at him)
    const float fatten = za::max(0.f, vr_hit_player_fatten.value);
    for(int i = 0; i < out.n; i++)
    {
        out.c[i].r += fatten;
    }
    return true;
}

bool segment(const Shape& s, const glm::vec3& a, const glm::vec3& b, float radius, float maxT, float& t, glm::vec3& point,
    glm::vec3& normal, int& zone)
{
    const float len = glm::length(b - a);
    if(len < 1e-4f)
    {
        return false;
    }
    const glm::vec3 rd = (b - a) / len;
    float best = -1.f, bestRank = 0.f;
    int bestC = -1;
    for(int i = 0; i < s.n; i++)
    {
        const Capsule& c = s.c[i];
        const float d = capsuleHit(a, rd, c.a, c.b, c.r + radius);
        // (head priority, vr_hit_head_priority: the head met within half a unit of another part is the head)
        const float rank = d - (c.zone == VR_REGION_HEAD && vr_hit_head_priority.value != 0.f ? 0.5f : 0.f);
        if(d >= 0.f && d <= len * maxT && (bestC < 0 || rank < bestRank))
        {
            best = d;
            bestRank = rank;
            bestC = i;
        }
    }
    if(bestC < 0)
    {
        return false;
    }
    const Capsule& c = s.c[bestC];
    t = best / len;
    point = a + rd * best;
    const glm::vec3 ba = c.b - c.a;
    const float baba = glm::dot(ba, ba);
    const float u = baba > 1e-6f ? za::clamp(glm::dot(point - c.a, ba) / baba, 0.f, 1.f) : 0.f;
    const glm::vec3 n = point - (c.a + ba * u);
    normal = glm::length(n) > 1e-4f ? glm::normalize(n) : -rd;
    zone = c.zone;
    return true;
}

bool clientShape(int num, Shape& out)
{
    const mpavatar::Avatar* a = mpavatar::of(num);
    avatar::Skeleton k;
    if(vr_hit_player_shape.value == 0.f || !a || !a->valid || !mpavatar::drawnSkeleton(num, k) || num <= 0 ||
       num >= cl.num_entities)
    {
        return false;
    }
    const float m2w = k.m2w;
    Pose p;
    p.head = a->head;
    p.yaw = a->body.valid ? a->body.yaw : a->headAngles[1];
    for(int h = 0; h < 2; h++)
    {
        p.hasHand[h] = a->hands[h].flags != 0;
        p.hand[h] = a->hands[h].handOrigin;
    }
    p.m2w = m2w;
    const entity_t& e = cl_entities[num];
    build(p, glm::vec3{e.origin[0], e.origin[1], e.origin[2]}, out);
    return true;
}

namespace
{

// vr_hitzones_aim [frames]: the test (mp_dm_hitzones, a listen host with a passive armed bot drawn): shots from round the
// bot at what is drawn of it, judged by the server as a shot is (SV_Move, a gun's precise trace; the zone QC would take).
// The head (its middle and the helmet's top) hits the head from every side; the main forearm hits an arm; a shot beside it
// (its radius, the tolerance and 3 units out) and one at the weapon's muzzle from the side miss the bot. Also how far the
// shape's joints are from the drawn skeleton's. Waits for the bot drawn (a frame a try, `frames` at most: "timed out").
// `hitzones_aim: N passed, M failed`.
testkit::Tally aimTally;

void aimCheck(bool ok, const char* what)
{
    aimTally.check(ok);
    Con_Printf("hitzones_aim: %s %s\n", ok ? "PASS" : "FAIL", what);
}

void aim_f()
{
    const int frames = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 600;
    if(!sv.active && frames > 0)
    {
        Cbuf_AddText(va("wait;vr_hitzones_aim %d\n", frames - 1)); // (the scene's map still loading)
        return;
    }
    const progs::ServerVm vm;
    int bot = 0;
    for(int e = 1; sv.active && e <= svs.maxclients && !bot; e++)
    {
        avatar::Skeleton k;
        glm::vec3 butt, muzzle, hand;
        bot = server::isBot(e - 1) && mpavatar::drawnSkeleton(e, k) && mpavatar::drawnWeapon(e, HAND_MAIN, butt, muzzle, hand)
                  ? e
                  : 0;
    }
    if(!bot)
    {
        if(frames <= 0)
        {
            aimTally.reset();
            aimCheck(false, "timed out (no armed bot drawn)");
            aimTally.verdict("hitzones_aim");
            return;
        }
        Cbuf_AddText(va("wait;vr_hitzones_aim %d\n", frames - 1));
        return;
    }
    aimTally.reset();
    edict_t* const ent = EDICT_NUM(bot);
    edict_t* const shooter = progs::localPlayer();
    avatar::Skeleton k;
    glm::vec3 butt, muzzle, hand;
    Shape s;
    if(!mpavatar::drawnSkeleton(bot, k) || !mpavatar::drawnWeapon(bot, HAND_MAIN, butt, muzzle, hand) || !serverShape(ent, s))
    {
        aimCheck(false, "the bot's shape");
        aimTally.verdict("hitzones_aim");
        return;
    }
    const float m = k.m2w;
    const glm::vec3 up{0.f, 0.f, 1.f};
    // The shape's joints against the drawn ones (units).
    {
        const auto dist = [](const glm::vec3& a, const glm::vec3& b) { return glm::length(a - b); };
        const float sh = za::max(dist(s.c[2].a, k.shoulder[HAND_OFF]), dist(s.c[6].a, k.shoulder[HAND_MAIN]));
        const float el = za::max(dist(s.c[2].b, k.elbow[HAND_OFF]), dist(s.c[6].b, k.elbow[HAND_MAIN]));
        const float wr = za::max(dist(s.c[3].b, k.wrist[HAND_OFF]), dist(s.c[7].b, k.wrist[HAND_MAIN]));
        const float an = za::max(dist(s.c[5].b, k.ankle[0]), dist(s.c[9].b, k.ankle[1]));
        const float pe = dist(s.c[1].b, k.pelvis);
        const glm::vec3 headMid = 0.5f * (k.headJoint + k.headEye) + up * (0.03f * m);
        const float hd = dist(s.c[0].a, headMid);
        Con_Printf("hitzones_aim: joints off the drawn: head %.1f shoulders %.1f elbows %.1f wrists %.1f pelvis %.1f ankles "
                   "%.1f (m2w %.1f)\n", hd, sh, el, wr, pe, an, m);
        // (the main arm in its frame from its drawn eyes: forward, left, up)
        const float y = ent->v.v_angle[YAW] * (3.14159265f / 180.f);
        const glm::vec3 fw{za::cos(y), za::sin(y), 0.f}, lf{-fw.y, fw.x, 0.f};
        const auto loc = [&](const glm::vec3& p)
        {
            const glm::vec3 d = p - k.headEye;
            return va("%.1f %.1f %.1f", glm::dot(d, fw), glm::dot(d, lf), d.z);
        };
        Con_Printf("hitzones_aim: main arm drawn: shoulder %s", loc(k.shoulder[HAND_MAIN]));
        Con_Printf(" elbow %s", loc(k.elbow[HAND_MAIN]));
        Con_Printf(" wrist %s; shape:", loc(k.wrist[HAND_MAIN]));
        Con_Printf(" elbow %s", loc(s.c[6].b));
        Con_Printf(" hand %s\n", loc(s.c[7].b));
        aimCheck(hd < 3.f && sh < 3.f && pe < 4.f && an < 5.f && el < 6.f && wr < 6.f, "the shape on the drawn skeleton");
    }
    const float yaw = ent->v.v_angle[YAW] * (3.14159265f / 180.f);
    const glm::vec3 f{za::cos(yaw), za::sin(yaw), 0.f}, l{-f.y, f.x, 0.f};
    const glm::vec3 centre{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    const glm::vec3 eye = centre + glm::vec3{0.f, 0.f, k.headEye.z - centre.z};
    const glm::vec3 headMid = 0.5f * (k.headJoint + k.headEye) + up * (0.03f * m);
    const glm::vec3 helmet = headMid + up * (0.08f * m);
    const glm::vec3 forearm = 0.5f * (k.elbow[HAND_MAIN] + k.wrist[HAND_MAIN]);
    const glm::vec3 nearMuzzle = butt + (muzzle - butt) * 0.9f;
    // How far out from the bot's eyes at `deg` round it (0 its front, 90 its left) the map lets a shooter stand: 80 units,
    // else 64, 48 or 32 (a wall nearer).
    const auto reach = [&](float deg)
    {
        const float a = deg * (3.14159265f / 180.f);
        for(const float d : {80.f, 64.f, 48.f, 32.f})
        {
            const glm::vec3 p = eye + (f * za::cos(a) + l * za::sin(a)) * (d + 8.f);
            vec3_t st{p.x, p.y, p.z}, en{eye.x, eye.y, eye.z}; // (as the shots go: from there, by the shooter)
            const trace_t tr = SV_Move(st, vec3_origin, vec3_origin, en, MOVE_NOMONSTERS, shooter);
            if(tr.fraction >= 1.f && !tr.startsolid)
            {
                return d;
            }
        }
        return 24.f;
    };
    // Shoot from `deg` round the bot, reach(deg) out at its eyes' height, at `target`: what it met. (`level`: from the
    // target's own height, square to the bot: a shot beside its arm passes on beside its legs.)
    const auto shoot = [&](float deg, const glm::vec3& target, int& zone, bool level = false)
    {
        const float a = deg * (3.14159265f / 180.f);
        const glm::vec3 from =
            eye + (f * za::cos(a) + l * za::sin(a)) * reach(deg) + glm::vec3{0.f, 0.f, level ? target.z - eye.z : 0.f};
        const glm::vec3 dir = glm::normalize(target - from);
        vec3_t st{from.x, from.y, from.z}, en{target.x + dir.x * 64.f, target.y + dir.y * 64.f, target.z + dir.z * 64.f};
        const trace_t tr = SV_Move(st, vec3_origin, vec3_origin, en, MOVE_NORMAL | MOVE_HITMODEL, shooter);
        // (not the bot: -1 nothing met, else -(10 + the edict met) -- 10 the world -- for the report)
        zone = tr.fraction >= 1.f ? -1 : -(10 + (tr.ent ? NUM_FOR_EDICT(tr.ent) : 0));
        if(tr.ent == ent)
        {
            float t = 1.f;
            glm::vec3 point, normal;
            const glm::vec3 at{tr.endpos[0], tr.endpos[1], tr.endpos[2]};
            zone = segment(s, at - dir * 16.f, at + dir * 48.f, hitmodel::tolerance(hitmodel::Class::Guns) + 0.5f, 1.f, t,
                       point, normal, zone)
                       ? zone
                       : -2;
        }
        else
        {
            // (what the shape alone says of the same line, and where the shot stopped)
            float t = 1.f;
            glm::vec3 point, normal;
            int z = -1;
            const bool own = segment(s, from, glm::vec3{en[0], en[1], en[2]}, hitmodel::tolerance(hitmodel::Class::Guns), 1.f,
                t, point, normal, z);
            Con_Printf("hitzones_aim: (from %.0f: the shape alone %d at %.2f zone %d; the shot stopped at %.2f)\n", deg,
                own ? 1 : 0, t, z, tr.fraction);
        }
        return tr.ent == ent;
    };
    int zone = 0;
    for(const float deg : {0.f, 45.f, 90.f, -90.f, -45.f, 150.f})
    {
        const bool hitMid = shoot(deg, headMid, zone);
        aimCheck(hitMid && zone == VR_REGION_HEAD, va("head's middle from %.0f: met %d zone %d", deg, hitMid ? 1 : 0, zone));
        const bool hitTop = shoot(deg, helmet, zone);
        aimCheck(hitTop && zone == VR_REGION_HEAD, va("helmet's top from %.0f: met %d zone %d", deg, hitTop ? 1 : 0, zone));
    }
    for(const float deg : {0.f, -45.f, -90.f})
    {
        const bool hit = shoot(deg, forearm, zone);
        aimCheck(hit && zone == VR_REGION_ARM, va("main forearm from %.0f: met %d zone %d", deg, hit ? 1 : 0, zone));
    }
    for(const float deg : {0.f, -45.f})
    {
        // (out from the forearm, away from the body, square to the shot and the forearm)
        const float a = deg * (3.14159265f / 180.f);
        const glm::vec3 from = eye + (f * za::cos(a) + l * za::sin(a)) * reach(deg);
        const glm::vec3 axis = glm::normalize(k.wrist[HAND_MAIN] - k.elbow[HAND_MAIN]);
        glm::vec3 out = glm::cross(glm::normalize(forearm - from), axis);
        out = glm::normalize(out) * (glm::dot(out, -l) >= 0.f ? 1.f : -1.f);
        const float gap = armRadius * m + hitmodel::tolerance(hitmodel::Class::Guns) + vr_hit_player_fatten.value + 3.f;
        const bool hit = shoot(deg, forearm + out * gap, zone, true);
        // (from its front nothing is behind it; from across its corner the shot goes on into the thigh drawn behind the
        // hanging hand: a leg hit, not the arm's)
        aimCheck(deg == 0.f ? !hit : zone != VR_REGION_ARM,
            va("%.1f units beside the main forearm from %.0f: met %d zone %d", gap, deg, hit ? 1 : 0, zone));
    }
    for(const float deg : {90.f, -90.f})
    {
        const bool hit = shoot(deg, nearMuzzle, zone);
        aimCheck(!hit, va("the weapon's muzzle from %.0f: met %d zone %d", deg, hit ? 1 : 0, zone));
    }
    aimTally.verdict("hitzones_aim");
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_hitzones_aim", aim_f);
}

} // namespace qvr::playerhit

using namespace qvr;

extern "C" int VR_PlayerShotClip(edict_t* touch, const float* start, const float* mins, const float* maxs, const float* end,
    int hittype, float hitmodel, int hitgibs, trace_t* trace)
{
    // (a shot's or a thrown thing's precise trace, a missile, what meets gibs: not melee's, nor the grappling hook's)
    const int hitClass = (hittype >> MOVE_HITMODEL_CLASS_SHIFT) & 3;
    const bool shot = hitmodel >= 0.f ? (hitClass == static_cast<int>(hitmodel::Class::Guns) || hitClass == static_cast<int>(hitmodel::Class::Thrown))
                                      : (hitgibs != 0 || (hittype & ~(MOVE_HITGIBS | MOVE_HITPASS)) == MOVE_MISSILE);
    if(!shot || !(static_cast<int>(touch->v.flags) & FL_CLIENT))
    {
        return -1;
    }
    playerhit::Shape s;
    if(!playerhit::serverShape(touch, s))
    {
        return -1;
    }
    // (the mover's own size: the largest sphere in its box; and the guns' tolerance, as a monster's model is grown)
    float radius = 0.f;
    for(int i = 0; i < 3; i++)
    {
        radius = i == 0 ? za::min(-mins[i], maxs[i]) : za::min(radius, za::min(-mins[i], maxs[i]));
    }
    radius = za::max(0.f, radius) + hitmodel::tolerance(hitmodel::Class::Guns);
    float t = 1.f;
    glm::vec3 point, normal;
    int zone = 0;
    if(!playerhit::segment(s, glm::vec3{start[0], start[1], start[2]}, glm::vec3{end[0], end[1], end[2]}, radius, 1.f, t,
           point, normal, zone))
    {
        return 0;
    }
    ZA_MEMSET(trace, 0, sizeof(*trace));
    trace->fraction = t;
    trace->allsolid = false;
    trace->startsolid = t <= 0.f;
    trace->inopen = true;
    for(int i = 0; i < 3; i++)
    {
        trace->endpos[i] = point[i];
        trace->plane.normal[i] = normal[i];
    }
    trace->plane.dist = glm::dot(normal, point);
    trace->ent = touch;
    return 1;
}

namespace
{

// playerbodypoint's last shape: points 0..6 make it, 16..22 read it again for the same player (QC VR_Lean_Scan's seven
// points with tracelines between them: one shape made, not seven; questopt2).
struct BodyPointKept
{
    const edict_t* ent{nullptr};
    bool ok{false};
    qvr::playerhit::Shape shape;
};
BodyPointKept bodyPointKept; // seat-shared: the server's hit memo (by entity)

} // namespace

void PF_playerbodypoint()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    int i = static_cast<int>(G_FLOAT(OFS_PARM1));
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = out[1] = out[2] = 0.f;
    const bool again = i >= 16 && bodyPointKept.ent == e;
    i = i >= 16 ? i - 16 : i;
    if(!again)
    {
        bodyPointKept.ent = e;
        bodyPointKept.ok = playerhit::serverShape(e, bodyPointKept.shape) && bodyPointKept.shape.n >= 8;
    }
    if(!bodyPointKept.ok)
    {
        return;
    }
    const playerhit::Shape& s = bodyPointKept.shape;
    // (build's order: 0 the head, 1 the torso neck..pelvis, then each side h its upper arm 2+4h, forearm 3+4h, legs)
    glm::vec3 p;
    switch(i)
    {
        case 0: p = s.c[0].a; break;
        case 1: p = s.c[1].a; break;
        case 2: p = (s.c[1].a + s.c[1].b) * 0.5f; break;
        case 3: p = s.c[2].a; break;
        case 4: p = s.c[6].a; break;
        case 5: p = s.c[3].b; break;
        case 6: p = s.c[7].b; break;
        default: return;
    }
    out[0] = p.x;
    out[1] = p.y;
    out[2] = p.z;
}

void PF_playerhitzone()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const float* p = G_VECTOR(OFS_PARM1);
    const float* d = G_VECTOR(OFS_PARM2);
    G_FLOAT(OFS_RETURN) = static_cast<float>(VR_REGION_NONE);
    playerhit::Shape s;
    const glm::vec3 dir{d[0], d[1], d[2]};
    if(glm::length(dir) < 1e-4f || !playerhit::serverShape(e, s))
    {
        return;
    }
    // (the shot's line through where it met him, from a little before it: the capsule it met first; none, his shape
    // missed -- a melee point on his box: his box decides)
    const glm::vec3 u = glm::normalize(dir), at{p[0], p[1], p[2]};
    const float back = 16.f;
    float t = 1.f;
    glm::vec3 point, normal;
    int zone = 0;
    if(playerhit::segment(s, at - u * back, at + u * 48.f, hitmodel::tolerance(hitmodel::Class::Guns) + 0.5f, 1.f, t, point,
           normal, zone))
    {
        G_FLOAT(OFS_RETURN) = static_cast<float>(zone);
    }
}
