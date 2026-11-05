// vr_flatsweep.cpp -- vr_flat_sweep <label> [open|wall|low|quick] [step]: see vr_flatsweep.hpp. Everything here measures the
// pose as last drawn; nothing it reads changes the game. Pitches are Quake's: positive looks down.
//
// open: every stance, standing and crouching, hipfire and ironsight, where he stands. quick: the long gun's hipfire standing and
// the pistol's crouching (for choosing a number). wall: the long gun put against the wall he faces. low: the long gun crouched under the lowest ceiling
// of the map a crouched box fits under (found by tracing the level).
//
// The row's camera numbers come from the drawn body itself, skinned here as the shader does (avatar::posedMesh), with a grid
// of rays through the screen: inner_pct is the share of the screen whose first surface of the torso, neck and head is seen from
// behind (its inside), hidden_pct the share first seeing a triangle of the neck or the head (collapsed for the eye's view, so never
// meant to be seen), body_pct all that sees those; arm_inner_pct and arm_pct the same of the arms alone, which the pose moves
// and the neck pivot doesn't. The rest use simple shapes: the torso as vr_selfcollide makes it, the head a sphere behind
// the eye and the neck under it, the gun a capsule along its model's length.

#include "vr_flatsweep.hpp"

#include "vr_api.h"
#include "vr_avatar.hpp"
#include "vr_backend.hpp" // HAND_OFF / HAND_MAIN / HAND_COUNT
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatsight.hpp"
#include "vr_geom.hpp"
#include "vr_hands.hpp"
#include "vr_progs.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Tan.hpp"

#include <cstdio>

namespace qvr::flatsweep
{
namespace
{

constexpr int widAxe = 2;      // QC's WID_AXE
constexpr int widShotgun = 4;  // WID_SHOTGUN
constexpr int widRevolver = 31; // WID_REVOLVER

struct Config
{
    int wid;
    const char* name;
    bool crouch;
    bool ads;
};

constexpr Config openList[] = {
    {widAxe, "melee", false, false},
    {widAxe, "melee", true, false},
    {widRevolver, "pistol", false, false},
    {widRevolver, "pistol", false, true},
    {widRevolver, "pistol", true, false},
    {widRevolver, "pistol", true, true},
    {widShotgun, "long", false, false},
    {widShotgun, "long", false, true},
    {widShotgun, "long", true, false},
    {widShotgun, "long", true, true},
};
constexpr Config wallList[] = {
    {widShotgun, "long", false, false},
    {widShotgun, "long", false, true},
    {widShotgun, "long", true, false},
    {widShotgun, "long", true, true},
};
constexpr Config lowList[] = {
    {widShotgun, "long", true, false},
    {widShotgun, "long", true, true},
};
constexpr Config quickList[] = {
    {widShotgun, "long", false, false},
    {widRevolver, "pistol", true, false},
};
constexpr Config adsList[] = { // (the long gun sighted, standing and crouched: the off arm by the eye looking up)
    {widShotgun, "long", false, true},
    {widShotgun, "long", true, true},
};

constexpr float pitchEnd = 85.f; // the mode's window at its defaults (vr_flat_pitch_up / _down): as far as he can look
constexpr int calmNeeded = 4;    // frames in a row with nothing moving
constexpr float calmMove = 0.004f; // units: moving less than this a frame is not moving
constexpr int minFrames = 3;     // a step's least frames: the pose, the view and the body each come a frame after the pitch
constexpr int armFrames = 8;     // ... and a weapon's: the impulse arrives through the server
constexpr int giveUpFrames = 500; // the safety net: the row is written unsettled, and the check fails it
constexpr int gridX = 32;        // rays through the screen
constexpr int gridY = 18;
constexpr float rayEdge = 1e-4f;  // a ray along a seam hits one of its triangles (one down the chest's middle missed both)
constexpr float meshReach = 40.f; // triangles further from the eye than this can't show their inside (and cost time)

enum class Stage
{
    Idle,
    Place,   // wall, low: the box moved
    Arm,     // the weapon, the crouch
    Aim,     // the ironsight's travel
    Walk,    // the pitch's steps
    Shot     // the frame after a screenshot's command
};

struct Tri
{
    glm::vec3 a, b, c;
    bool collapsed;
};

struct Sweep
{
    Stage stage{Stage::Idle};
    char label[32]{};
    char mode[8]{};
    FILE* out{nullptr};
    const Config* list{nullptr};
    int listCount{0};
    int cfg{0};
    float pitch{-pitchEnd};
    float step{5.f};
    int frames{0};
    int calm{0};
    float moved{0.f}; // the most anything the row reads moved in the last frame, and the most since the step's 100th frame:
    float movedMost{0.f}; // what a pose that won't settle swings by
    glm::vec3 last[4]{};
    float lastShares[3]{};
    int rows{0};
    int unsettled{0};
    int shots{0};
    za::Vector<glm::vec3> verts;
    za::Vector<unsigned> tris;
    za::Vector<unsigned char> parts;
    za::Vector<Tri> near;    // the torso, neck and head
    za::Vector<Tri> nearArm; // the arms
} sw;

[[nodiscard]] edict_t* hisEdict()
{
    return progs::playerEdict();
}

// A line through the level, him left out.
[[nodiscard]] trace_t lineTrace(const glm::vec3& a, const glm::vec3& b)
{
    const glm::vec3 zero{0.f};
    const auto tr = worldtrace::move(a, zero, zero, b, MOVE_NOMONSTERS);
    if(tr)
    {
        return *tr;
    }
    trace_t none{};
    none.fraction = 1.f;
    return none;
}

// The closest point on a triangle to p (Ericson, Real-Time Collision Detection, 5.1.5).
[[nodiscard]] float triDistance(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if(d1 <= 0.f && d2 <= 0.f)
    {
        return glm::length(ap);
    }
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if(d3 >= 0.f && d4 <= d3)
    {
        return glm::length(bp);
    }
    const float vc = d1 * d4 - d3 * d2;
    if(vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
    {
        return glm::distance(p, a + ab * (d1 / (d1 - d3)));
    }
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if(d6 >= 0.f && d5 <= d6)
    {
        return glm::length(cp);
    }
    const float vb = d5 * d2 - d1 * d6;
    if(vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
    {
        return glm::distance(p, a + ac * (d2 / (d2 - d6)));
    }
    const float va = d3 * d6 - d5 * d4;
    if(va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
    {
        return glm::distance(p, b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6))));
    }
    const float denom = 1.f / (va + vb + vc);
    return glm::distance(p, a + ab * (vb * denom) + ac * (vc * denom));
}

struct Hit
{
    bool any{false};
    float t{1e9f};
    float det{0.f};
    bool collapsed{false};
};

// The nearest triangle along a ray, either side (Moeller-Trumbore): det's sign says the side.
[[nodiscard]] Hit castRay(const glm::vec3& org, const glm::vec3& dir, const za::Vector<Tri>& tris)
{
    Hit hit;
    for(const Tri& tr : tris)
    {
        const glm::vec3 e1 = tr.b - tr.a, e2 = tr.c - tr.a;
        const glm::vec3 p = glm::cross(dir, e2);
        const float det = glm::dot(e1, p);
        if(det > -1e-7f && det < 1e-7f)
        {
            continue;
        }
        const float inv = 1.f / det;
        const glm::vec3 o = org - tr.a;
        const float u = glm::dot(o, p) * inv;
        if(u < -rayEdge || u > 1.f + rayEdge)
        {
            continue;
        }
        const glm::vec3 q = glm::cross(o, e1);
        const float v = glm::dot(dir, q) * inv;
        if(v < -rayEdge || u + v > 1.f + rayEdge)
        {
            continue;
        }
        const float t = glm::dot(e2, q) * inv;
        if(t > 0.f && t < hit.t)
        {
            hit.any = true;
            hit.t = t;
            hit.det = det;
            hit.collapsed = tr.collapsed;
        }
    }
    return hit;
}

// The model's triangles wind one way: measured from outside on all sides of the chest, a ray's det is negative.
[[nodiscard]] bool seenFromBehind(const Hit& h)
{
    return h.any && h.det > 0.f;
}

[[nodiscard]] float pointSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b)
{
    const glm::vec3 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    const float t = len2 > 1e-8f ? za::clamp(glm::dot(p - a, ab) / len2, 0.f, 1.f) : 0.f;
    return glm::distance(p, a + ab * t);
}

struct Capsule
{
    glm::vec3 a, b;
    float r;
};

// How deep a capsule is in another: sampled along it, as these are short and only the deepest part is read.
[[nodiscard]] float capsuleDepth(const Capsule& x, const Capsule& y)
{
    float deepest = -1e9f;
    constexpr int samples = 17;
    for(int i = 0; i < samples; i++)
    {
        const glm::vec3 p = glm::mix(x.a, x.b, static_cast<float>(i) / (samples - 1));
        deepest = za::max(deepest, x.r + y.r - pointSegment(p, y.a, y.b));
    }
    return deepest;
}

struct Shapes
{
    Capsule torso[4];
    Capsule head;
    Capsule neck;
};

[[nodiscard]] Shapes bodyShapes(const hands::State& s, const glm::vec3& eye, const glm::vec3& fwd)
{
    // The torso's as vr_selfcollide's sceneFor makes them.
    const avatar::BuildShape& build = avatar::buildShape(avatar::bodyBuild());
    const avatar::Torso t = avatar::torso(s);
    const float k = units::metresToUnits() * units::bodyScale();
    Shapes out;
    int n = 0;
    for(const float side : {-1.f, 1.f})
    {
        const glm::vec3 cs = t.chest.rot[1] * (side * 0.058f * build.torso * build.capSide * k) - t.chest.rot[2] * (0.01f * k);
        out.torso[n++] = {t.chest.pos + t.chest.rot[0] * (-0.06f * k) + cs, t.chest.pos + t.chest.rot[0] * (0.09f * k) + cs,
            0.125f * build.torso * k};
        const glm::vec3 ps = t.pelvis.rot[1] * (side * 0.055f * k) - t.pelvis.rot[2] * (0.01f * k);
        out.torso[n++] = {t.pelvis.pos + ps, t.pelvis.pos + t.pelvis.rot[0] * (0.12f * k) + ps, 0.11f * k};
    }
    const glm::vec3 centre = eye - fwd * (0.09f * k);
    out.head = {centre, centre, 0.095f * k};
    const glm::vec3 top = s.head + flatplay::headLean(s.headAngles) - glm::vec3{0.f, 0.f, vr_body_eye_up.value * k};
    out.neck = {top - glm::vec3{0.f, 0.f, 0.09f * k}, top, 0.05f * build.torso * k};
    return out;
}

[[nodiscard]] float torsoDepth(const Shapes& b, const Capsule& c)
{
    float deepest = -1e9f;
    for(const Capsule& t : b.torso)
    {
        deepest = za::max(deepest, capsuleDepth(c, t));
    }
    return deepest;
}

[[nodiscard]] float headDepth(const Shapes& b, const Capsule& c)
{
    return za::max(capsuleDepth(c, b.head), capsuleDepth(c, b.neck));
}

void writeHeader()
{
    fprintf(sw.out, "label,weapon,crouch,ads,pitch,settled,frames,eye_x,eye_y,eye_z,eye_fwd,eye_up,hull_m,solid,clear,inner_pct,hidden_pct,"
                    "body_pct,arm_inner_pct,arm_pct,eye_torso,eye_neck,eye_arm,inside,arm_inside,gun_torso,gun_head,fore0_torso,fore1_torso,hand0_torso,hand1_torso,hand0_head,"
                    "hand1_head,arm0,arm1,gap0,gap1,holding,has_spot,look_share,look_far,spot_off,arm_hip,arm_sight,shot_deg,shot_apart,"
                    "rear_off,front_off,relief,ads_t,gun_x,gun_y,gun_z,hand0_x,hand0_y,hand0_z,hand1_x,hand1_y,hand1_z,point_x,point_y,point_z,stop_x,stop_y,stop_z,swing,"
                    "yaw,sh0_x,sh0_y,sh0_z,sh1_x,sh1_y,sh1_z,an0_x,an0_y,an0_z,an1_x,an1_y,an1_z,spot_slide,"
                    "el0_x,el0_y,el0_z,el1_x,el1_y,el1_z,wr0_x,wr0_y,wr0_z,wr1_x,wr1_y,wr1_z,gun_out\n");
}

// The row: the pose as last drawn.
void writeRow(const Config& c, bool settled)
{
    const hands::State& s = hands::current();
    const glm::vec3 eye = geom::vec(r_refdef.vieworg);
    glm::vec3 vf, vr, vu;
    hands::angleVectors(geom::vec(r_refdef.viewangles), vf, vr, vu);
    glm::vec3 yf, yr, yu;
    hands::angleVectors({0.f, r_refdef.viewangles[1], 0.f}, yf, yr, yu);
    const glm::vec3 fromHead = eye - s.head;

    // His box, and the level: how far inside the box's sides the eye stands, and whether it is in anything.
    float hullMargin = 999.f;
    int solid = 0;
    int clear = 1;
    if(sv.active && hisEdict())
    {
        const glm::vec3 org = geom::vec(hisEdict()->v.origin);
        const glm::vec3 mins = geom::vec(hisEdict()->v.mins), maxs = geom::vec(hisEdict()->v.maxs);
        const glm::vec3 d = eye - org;
        hullMargin = za::min(za::min(za::min(d.x - mins.x, maxs.x - d.x), za::min(d.y - mins.y, maxs.y - d.y)),
            za::min(d.z - mins.z, maxs.z - d.z));
        vec3_t eyeV = {eye.x, eye.y, eye.z};
        solid = SV_PointContents(eyeV) == CONTENTS_SOLID ? 1 : 0;
        const trace_t tr = lineTrace(s.head, eye);
        clear = tr.fraction >= 1.f && !tr.startsolid ? 1 : 0;
    }

    // The body: the triangles near enough.
    const bool meshed = avatar::posedMesh(sw.verts, sw.tris, sw.parts);
    sw.near.clear();
    sw.nearArm.clear();
    float eyeTo[3]{999.f, 999.f, 999.f};
    if(meshed)
    {
        for(unsigned i = 0; i < sw.parts.size(); i++)
        {
            const Tri tr{sw.verts[sw.tris[i * 3]], sw.verts[sw.tris[i * 3 + 1]], sw.verts[sw.tris[i * 3 + 2]], sw.parts[i] == 1};
            const float d = triDistance(eye, tr.a, tr.b, tr.c);
            eyeTo[sw.parts[i]] = za::min(eyeTo[sw.parts[i]], d);
            if(d < meshReach)
            {
                (sw.parts[i] == 2 ? sw.nearArm : sw.near).pushBack(tr);
            }
        }
    }
    int innerRays = 0, hiddenRays = 0, bodyRays = 0, armInnerRays = 0, armRays = 0;
    const float tanX = za::tan(glm::radians(r_refdef.fov_x * 0.5f));
    const float tanY = za::tan(glm::radians(r_refdef.fov_y * 0.5f));
    for(int y = 0; y < gridY; y++)
    {
        for(int x = 0; x < gridX; x++)
        {
            const float sx = (2.f * (static_cast<float>(x) + 0.5f) / gridX - 1.f) * tanX;
            const float sy = (2.f * (static_cast<float>(y) + 0.5f) / gridY - 1.f) * tanY;
            const glm::vec3 ray = glm::normalize(vf + vr * sx + vu * sy);
            const Hit h = castRay(eye, ray, sw.near);
            if(h.any)
            {
                bodyRays++;
                hiddenRays += h.collapsed ? 1 : 0;
                innerRays += seenFromBehind(h) && !h.collapsed ? 1 : 0;
            }
            const Hit ha = castRay(eye, ray, sw.nearArm);
            if(ha.any)
            {
                armRays++;
                armInnerRays += seenFromBehind(ha) ? 1 : 0;
            }
        }
    }
    // Inside: most of the ways out of the eye first meet a surface from behind.
    int backs = 0, hits = 0, armBacks = 0, armHits = 0;
    const glm::vec3 ways[] = {vf, -vf, vr, -vr, vu, -vu};
    for(const glm::vec3& way : ways)
    {
        const Hit h = castRay(eye, way, sw.near);
        hits += h.any ? 1 : 0;
        backs += seenFromBehind(h) ? 1 : 0;
        const Hit ha = castRay(eye, way, sw.nearArm);
        armHits += ha.any ? 1 : 0;
        armBacks += seenFromBehind(ha) ? 1 : 0;
    }
    const int inside = hits >= 3 && backs * 2 > hits ? 1 : 0;
    const int armInside = armHits >= 3 && armBacks * 2 > armHits ? 1 : 0;

    // The simple shapes.
    const Shapes shapes = bodyShapes(s, eye, vf);
    float gunTorso = -99.f, gunHead = -99.f;
    glm::vec3 gun{0.f};
    view::WeaponFrame wf;
    const int gunHand = flatplay::heldIn(HAND_MAIN) >= 0 ? HAND_MAIN : HAND_OFF;
    if(view::weaponFrame(s, gunHand, wf) && wf.model)
    {
        const float cy = 0.5f * (wf.model->mins[1] + wf.model->maxs[1]), cz = 0.5f * (wf.model->mins[2] + wf.model->maxs[2]);
        const glm::vec3 rear{wf.modelToWorld * glm::vec4{wf.model->mins[0], cy, cz, 1.f}};
        gun = glm::vec3{wf.modelToWorld * glm::vec4{wf.model->maxs[0], cy, cz, 1.f}};
        const Capsule cap{rear, gun, 1.f};
        gunTorso = torsoDepth(shapes, cap);
        gunHead = headDepth(shapes, cap);
    }
    avatar::Skeleton k;
    const bool haveBody = avatar::skeleton(k);
    float foreTorso[2]{-99.f, -99.f}, handTorso[2]{-99.f, -99.f}, handHead[2]{-99.f, -99.f}, arm[2]{0.f, 0.f}, gap[2]{0.f, 0.f};
    glm::vec3 hand[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    for(int h = 0; h < HAND_COUNT; h++)
    {
        glm::vec3 pos, rot;
        const bool drawn = view::drawnHandPose(h, pos, rot);
        hand[h] = drawn ? pos : s.pos[h];
        if(haveBody)
        {
            foreTorso[h] = torsoDepth(shapes, {k.elbow[h], k.wrist[h], 1.3f});
            handTorso[h] = torsoDepth(shapes, {hand[h], hand[h], 1.8f});
            handHead[h] = headDepth(shapes, {hand[h], hand[h], 1.8f});
            arm[h] = glm::distance(k.shoulder[h], hand[h]) / za::max(flatplay::armReach(), 1.f);
            gap[h] = glm::distance(k.wrist[h], hand[h]);
        }
    }

    const flatsight::SweepNumbers n = flatsight::sweepNumbers(s);
    const glm::vec3 anchor[2]{flatplay::shoulderOf(s, 0), flatplay::shoulderOf(s, 1)}; // the shoulder the pose stands on (k.shoulder: the one the arm hangs from)
    fprintf(sw.out,
        "%s,%s,%d,%d,%.1f,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,"
        "%.3f,%.3f,%d,%d,%.3f,%d,%.3f,%.3f,%.3f,%.4f,%.3f,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.3f,"
        "%.2f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.3f\n",
        sw.label, c.name, c.crouch ? 1 : 0, c.ads ? 1 : 0, sw.pitch, settled ? 1 : 0, sw.frames, eye.x, eye.y, eye.z, glm::dot(fromHead, yf),
        fromHead.z, hullMargin, solid, clear, 100.f * innerRays / (gridX * gridY), 100.f * hiddenRays / (gridX * gridY),
        100.f * bodyRays / (gridX * gridY), 100.f * armInnerRays / (gridX * gridY), 100.f * armRays / (gridX * gridY), eyeTo[0], eyeTo[1],
        eyeTo[2], inside, armInside, gunTorso, gunHead, foreTorso[0], foreTorso[1], handTorso[0], handTorso[1],
        handHead[0], handHead[1], arm[0], arm[1], gap[0], gap[1], n.holding, n.hasSpot ? 1 : 0, n.lookShare, n.lookFar ? 1 : 0, n.spotOff,
        n.armHip, n.armSight, n.shotDeg, n.shotApart, n.rearOff, n.frontOff, n.relief, n.adsT, gun.x, gun.y, gun.z, hand[0].x, hand[0].y, hand[0].z,
        hand[1].x, hand[1].y, hand[1].z, n.shotPoint.x, n.shotPoint.y, n.shotPoint.z, n.shotStop.x, n.shotStop.y, n.shotStop.z, sw.movedMost,
        r_refdef.viewangles[1], k.shoulder[0].x, k.shoulder[0].y, k.shoulder[0].z, k.shoulder[1].x, k.shoulder[1].y, k.shoulder[1].z, anchor[0].x, anchor[0].y,
        anchor[0].z, anchor[1].x, anchor[1].y, anchor[1].z, n.spotSlide, k.elbow[0].x, k.elbow[0].y, k.elbow[0].z, k.elbow[1].x,
        k.elbow[1].y, k.elbow[1].z, k.wrist[0].x, k.wrist[0].y, k.wrist[0].z, k.wrist[1].x, k.wrist[1].y, k.wrist[1].z, flatplay::gunOut(sw.pitch));
    sw.rows++;
    sw.unsettled += settled ? 0 : 1;
}

// Whether everything the row reads has stopped moving.
[[nodiscard]] bool calmNow()
{
    const hands::State& s = hands::current();
    avatar::Skeleton k;
    const bool body = avatar::skeleton(k);
    const glm::vec3 now[4]{geom::vec(r_refdef.vieworg), body ? k.wrist[0] : s.pos[0], body ? k.wrist[1] : s.pos[1], s.muzzle[HAND_MAIN]};
    const float shares[3]{flatsight::adsShare(), flatsight::stanceYaw() * 0.1f, r_refdef.fov_x * 0.1f};
    float moved = 0.f;
    for(int i = 0; i < 4; i++)
    {
        moved = za::max(moved, glm::distance(now[i], sw.last[i]));
        sw.last[i] = now[i];
    }
    for(int i = 0; i < 3; i++)
    {
        const float d = shares[i] - sw.lastShares[i];
        moved = za::max(moved, d < 0.f ? -d : d);
        sw.lastShares[i] = shares[i];
    }
    sw.moved = moved;
    sw.movedMost = sw.frames > 100 ? za::max(sw.movedMost, moved) : 0.f;
    sw.calm = moved < calmMove ? sw.calm + 1 : 0;
    return sw.calm >= calmNeeded;
}

void startConfig()
{
    const Config& c = sw.list[sw.cfg];
    Cbuf_AddText(va("vr_flat_aim 0; %cflatcrouch; vr_test_give_wid %d; impulse 149\n", c.crouch ? '+' : '-', c.wid));
    sw.stage = Stage::Arm;
    sw.frames = 0;
    sw.calm = 0;
    sw.pitch = -pitchEnd;
}

void finish()
{
    Cbuf_AddText("vr_flat_aim 0; -flatcrouch\n");
    if(sw.out)
    {
        fclose(sw.out);
        sw.out = nullptr;
    }
    sw.stage = Stage::Idle;
    // The end of the test's run: -QuitOn reads this line.
    Con_Printf("flatsweep: %s done, %d rows, %d unsettled, %d shots\n", sw.label, sw.rows, sw.unsettled, sw.shots);
}

// The pitches the sheet shows.
[[nodiscard]] const char* sheetName(float pitch)
{
    return pitch <= -pitchEnd ? "up" : pitch >= pitchEnd ? "down" : pitch == 0.f ? "level" : nullptr;
}

void nextStep()
{
    if(sw.pitch >= pitchEnd)
    {
        if(++sw.cfg >= sw.listCount)
        {
            finish();
            return;
        }
        startConfig();
        return;
    }
    sw.pitch = za::min(sw.pitch + sw.step, pitchEnd);
    sw.stage = Stage::Walk;
    sw.frames = 0;
    sw.calm = 0;
}

// The lowest ceiling a crouched box fits under, near the level's own spaces: a grid of traces. 0 when none. Each spot's
// corners must be clear too, and its floor and ceiling near the middle's, so it is a passage and no edge of a step.
[[nodiscard]] bool findLow(glm::vec3& at, float& gap)
{
    const qmodel_t* world = sv.worldmodel;
    gap = 1e9f;
    const auto column = [&](float x, float y, float z, float& floorZ, float& ceilZ) {
        vec3_t mid = {x, y, z};
        if(SV_PointContents(mid) != CONTENTS_EMPTY)
        {
            return false;
        }
        const trace_t a = lineTrace({x, y, z}, {x, y, z - 512.f});
        const trace_t b = lineTrace({x, y, z}, {x, y, z + 512.f});
        floorZ = a.endpos[2];
        ceilZ = b.endpos[2];
        return a.fraction < 1.f && b.fraction < 1.f;
    };
    constexpr float cell = 16.f;
    for(float x = world->mins[0]; x < world->maxs[0]; x += cell)
    {
        for(float y = world->mins[1]; y < world->maxs[1]; y += cell)
        {
            for(float z = world->mins[2]; z < world->maxs[2]; z += 16.f)
            {
                float f = 0.f, c = 0.f;
                if(!column(x, y, z, f, c))
                {
                    continue;
                }
                const float height = c - f;
                if(height < 36.f || height > 64.f || height >= gap)
                {
                    continue;
                }
                bool fits = true;
                for(int i = 0; i < 4 && fits; i++)
                {
                    float fc = 0.f, cc = 0.f;
                    const float cx = x + ((i & 1) ? 18.f : -18.f), cy = y + ((i & 2) ? 18.f : -18.f);
                    fits = column(cx, cy, 0.5f * (f + c), fc, cc) && fc <= f + 4.f && fc >= f - 4.f && cc <= c + 4.f && cc >= c - 4.f;
                }
                if(fits)
                {
                    gap = height;
                    at = {x, y, f + 24.f};
                }
            }
        }
    }
    return gap < 1e8f;
}

// Moved to where this mode measures: against the wall he faces, or under a low ceiling (crouched first: the box is the
// crouch's by the time it arrives).
void place()
{
    if(!q_strcasecmp(sw.mode, "wall"))
    {
        const glm::vec3 start = geom::vec(hisEdict()->v.origin);
        const glm::vec3 fwd = hands::forward({0.f, cl.viewangles[1], 0.f});
        // (At his box's top: at its middle the line stops at a step he can look over.)
        const glm::vec3 top = start + glm::vec3{0.f, 0.f, 30.f};
        const trace_t tr = lineTrace(top, top + fwd * 4096.f);
        const glm::vec3 at = glm::vec3{tr.endpos[0], tr.endpos[1], start.z} - fwd * 16.5f;
        Con_Printf("flatsweep: at the wall, %.1f %.1f %.1f, %.1f from it\n", at.x, at.y, at.z, 16.5f);
        Cbuf_AddText(va("setpos %.1f %.1f %.1f 0 %.1f 0\n", at.x, at.y, at.z, cl.viewangles[1]));
    }
    else if(!q_strcasecmp(sw.mode, "low"))
    {
        glm::vec3 at{0.f};
        float gap = 0.f;
        if(!findLow(at, gap))
        {
            Con_Printf("flatsweep: %s FAIL no low ceiling in this map\n", sw.label);
            sw.listCount = 0;
            finish();
            return;
        }
        Con_Printf("flatsweep: under a ceiling %.1f over the floor, at %.1f %.1f %.1f\n", gap, at.x, at.y, at.z);
        Cbuf_AddText(va("+flatcrouch; setpos %.1f %.1f %.1f 0 %.1f 0\n", at.x, at.y, at.z, cl.viewangles[1]));
    }
    startConfig();
}

void sweep_f()
{
    if(!flatplay::active() || !sv.active)
    {
        Con_Printf("vr_flat_sweep: flatscreen play in a local game only\n");
        return;
    }
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_flat_sweep <label> [open|wall|low|quick] [step degrees]\n");
        return;
    }
    q_strlcpy(sw.label, Cmd_Argv(1), sizeof(sw.label));
    q_strlcpy(sw.mode, Cmd_Argc() > 2 ? Cmd_Argv(2) : "open", sizeof(sw.mode));
    sw.step = Cmd_Argc() > 3 ? za::clamp(static_cast<float>(Q_atof(Cmd_Argv(3))), 1.f, 85.f) : 5.f;
    if(!q_strcasecmp(sw.mode, "wall"))
    {
        sw.list = wallList;
        sw.listCount = sizeof(wallList) / sizeof(wallList[0]);
    }
    else if(!q_strcasecmp(sw.mode, "low"))
    {
        sw.list = lowList;
        sw.listCount = sizeof(lowList) / sizeof(lowList[0]);
    }
    else if(!q_strcasecmp(sw.mode, "quick"))
    {
        sw.list = quickList;
        sw.listCount = sizeof(quickList) / sizeof(quickList[0]);
    }
    else if(!q_strcasecmp(sw.mode, "ads"))
    {
        sw.list = adsList;
        sw.listCount = sizeof(adsList) / sizeof(adsList[0]);
    }
    else
    {
        sw.list = openList;
        sw.listCount = sizeof(openList) / sizeof(openList[0]);
    }
    const char* path = va("%s/screenshots/flatsweep_%s.csv", com_gamedir, sw.label);
    Sys_mkdir(va("%s/screenshots", com_gamedir));
    sw.out = fopen(path, "w");
    if(!sw.out)
    {
        Con_Printf("flatsweep: %s FAIL can't write its file\n", sw.label);
        return;
    }
    writeHeader();
    sw.cfg = 0;
    sw.rows = sw.unsettled = sw.shots = 0;
    sw.pitch = -pitchEnd;
    sw.stage = Stage::Place;
}

// vr_flat_walk_probe <label> <frames> [shot every N frames]: the drawn hands, arms and body while he walks (the script holds
// +forward; FLATPLAY.md, "Bob and sway"). In the camera's frame: each hand's wrist on its arm against the hand (how far it moves off
// where it stood at rest: the hand detached from the forearm; and its largest move in a frame), each hand's range on the screen
// (the bob and sway he sees) and each shoulder's. Rest first (restFrames, the arm's place), then the move's settle, then the
// frames asked for; one verdict line, `flatwalk: <label> done ...`.
constexpr int walkRestFrames = 6;
constexpr int walkSettleFrames = 36; // half a second: the bob's size eases in with the speed (flatplay's bobEase)

struct Walk
{
    bool on{false};
    char label[48]{};
    int frames{0};
    int want{0};
    int shotEvery{0};
    int shots{0};
    bool holdPitch{false}; // the look held at `pitch` each frame (the 4th argument)
    float pitch{0.f};
    glm::vec3 restRel[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 lastRel[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    bool haveLast[2]{false, false};
    float gapMost[2]{0.f, 0.f};
    float jumpMost[2]{0.f, 0.f};
    glm::vec3 handLo[2], handHi[2], shLo[2], shHi[2];
    float speedSum{0.f};
    int measured{0};
};

Walk wk;

void walk_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("vr_flat_walk_probe <label> <frames> [shot every N frames] [pitch held]\n");
        return;
    }
    wk = Walk{};
    q_strlcpy(wk.label, Cmd_Argv(1), sizeof(wk.label));
    wk.want = za::clamp(Q_atoi(Cmd_Argv(2)), 10, 2000);
    wk.shotEvery = Cmd_Argc() > 3 ? za::max(0, Q_atoi(Cmd_Argv(3))) : 0;
    wk.holdPitch = Cmd_Argc() > 4;
    wk.pitch = wk.holdPitch ? za::clamp(static_cast<float>(Q_atof(Cmd_Argv(4))), -85.f, 85.f) : 0.f;
    wk.on = true;
}

void walkFrame()
{
    if(!wk.on)
    {
        return;
    }
    avatar::Skeleton k;
    const glm::vec3 cam = geom::vec(r_refdef.vieworg);
    glm::vec3 cf, cr, cu;
    hands::angleVectors(geom::vec(r_refdef.viewangles), cf, cr, cu);
    const glm::mat3 toCam = glm::transpose(glm::mat3{cf, cr, cu});
    if(!flatplay::active() || !avatar::skeleton(k))
    {
        Con_Printf("flatwalk: %s FAIL no body drawn\n", wk.label);
        wk.on = false;
        return;
    }
    if(wk.holdPitch)
    {
        cl.viewangles[0] = wk.pitch;
    }
    const int f = wk.frames++;
    const bool measuring = f >= walkRestFrames + walkSettleFrames;
    for(int h = 0; h < HAND_COUNT; h++)
    {
        glm::vec3 pos, rot;
        if(!view::drawnHandPose(h, pos, rot))
        {
            continue;
        }
        const glm::vec3 rel = toCam * (k.wrist[h] - pos);
        if(f < walkRestFrames)
        {
            wk.restRel[h] = rel; // (standing: the last of these frames is the rest)
            continue;
        }
        if(!measuring)
        {
            wk.lastRel[h] = rel;
            wk.haveLast[h] = true;
            continue;
        }
        wk.gapMost[h] = za::max(wk.gapMost[h], glm::distance(rel, wk.restRel[h]));
        if(wk.haveLast[h])
        {
            wk.jumpMost[h] = za::max(wk.jumpMost[h], glm::distance(rel, wk.lastRel[h]));
        }
        wk.lastRel[h] = rel;
        wk.haveLast[h] = true;
        const glm::vec3 hc = toCam * (pos - cam), sc = toCam * (k.shoulder[h] - cam);
        if(wk.measured == 0)
        {
            wk.handLo[h] = wk.handHi[h] = hc;
            wk.shLo[h] = wk.shHi[h] = sc;
        }
        wk.handLo[h] = glm::min(wk.handLo[h], hc);
        wk.handHi[h] = glm::max(wk.handHi[h], hc);
        wk.shLo[h] = glm::min(wk.shLo[h], sc);
        wk.shHi[h] = glm::max(wk.shHi[h], sc);
    }
    if(f == walkRestFrames)
    {
        Cbuf_AddText("+forward\n");
    }
    if(measuring)
    {
        wk.speedSum += glm::length(glm::vec2{cl.velocity[0], cl.velocity[1]});
        wk.measured++;
        if(wk.shotEvery > 0 && wk.measured % wk.shotEvery == 1 && wk.shots < 8)
        {
            Cbuf_AddText(va("cl_screenshotname \"screenshots/fw_%s_%d\"; screenshot\n", wk.label, wk.shots++));
        }
    }
    if(wk.measured >= wk.want)
    {
        Cbuf_AddText("-forward\n");
        const auto span = [](const glm::vec3& lo, const glm::vec3& hi) { return glm::length(hi - lo); };
        // (Short: the console wraps a long line in the log. Off hand, main hand; units.)
        Con_Printf("flatwalk: %s done %d speed %.0f wrist %.3f %.3f jump %.3f %.3f hand %.2f %.2f shoulder %.2f %.2f\n",
            wk.label, wk.measured, wk.speedSum / static_cast<float>(wk.measured), wk.gapMost[0], wk.gapMost[1], wk.jumpMost[0],
            wk.jumpMost[1], span(wk.handLo[0], wk.handHi[0]), span(wk.handLo[1], wk.handHi[1]), span(wk.shLo[0], wk.shHi[0]),
            span(wk.shLo[1], wk.shHi[1]));
        wk.on = false;
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_flat_sweep", sweep_f);
    Cmd_AddCommand("vr_flat_walk_probe", walk_f);
}

void frame()
{
    walkFrame();
    if(sw.stage == Stage::Idle)
    {
        return;
    }
    if(!flatplay::active() || !sv.active)
    {
        sw.listCount = 0;
        finish();
        return;
    }
    cl.viewangles[0] = sw.pitch; // held each frame: nothing else moves the look in a test's run
    sw.frames++;
    switch(sw.stage)
    {
        case Stage::Idle: break;
        case Stage::Place: place(); break;
        case Stage::Arm:
            if((calmNow() && sw.frames >= armFrames) || sw.frames >= giveUpFrames)
            {
                const Config& c = sw.list[sw.cfg];
                if(c.ads)
                {
                    Cbuf_AddText("vr_flat_aim 1\n");
                }
                sw.stage = Stage::Aim;
                sw.frames = 0;
                sw.calm = 0;
            }
            break;
        case Stage::Aim:
            if((calmNow() && sw.frames >= armFrames) || sw.frames >= giveUpFrames)
            {
                sw.stage = Stage::Walk;
                sw.frames = 0;
                sw.calm = 0;
            }
            break;
        case Stage::Walk:
        {
            const bool calm = calmNow() && sw.frames >= minFrames;
            if(calm || sw.frames >= giveUpFrames)
            {
                const Config& c = sw.list[sw.cfg];
                writeRow(c, calm);
                if(const char* name = sheetName(sw.pitch))
                {
                    Cbuf_AddText(va("cl_screenshotname \"screenshots/fs_%s_%s_%s_%s_%s\"; screenshot\n", sw.label, c.name,
                        c.crouch ? "crouch" : "stand", c.ads ? "ads" : "hip", name));
                    sw.shots++;
                    sw.stage = Stage::Shot;
                    sw.frames = 0;
                }
                else
                {
                    nextStep();
                }
            }
            break;
        }
        case Stage::Shot:
            if(sw.frames >= 2) // the command runs in this frame's buffer: the picture is written by the next
            {
                nextStep();
            }
            break;
    }
}

} // namespace qvr::flatsweep
