// vr_avatar_pitch.cpp -- the body's pitch (vr_avatar_internal.hpp, BodyPitch): one pose state that turns the trunk
// forward about the top of the neck, eased both ways; solveTorso (vr_avatar.cpp) applies it, the swimming legs
// (vr_avatar_legs.cpp, swimFoot) trail along it. Its first source is swimming: fully under water (Quake's
// waterlevel 3) and not standing on the bottom, the trunk tips forward vr_body_swim_tilt_angle degrees, the legs trailing
// behind and up (ROUND24.md, "Swimming tilt"). Close to a ledge you can climb out of the water at (the ledge map,
// vr_ledges.hpp: a lip with water in the drop below it and its top above the water), it straightens again, so that the
// legs are already under the body when the hands take the ledge (the climbing feet, vr_avatar_climb.cpp, step onto the
// wall from there). VR and flat-screen play pose the body through the same avatar::pose, so both tilt.
//
// Lying down (vr_prone; ROUND24.md, "Lying down and crawling") is the second source: while the server has the local
// player lying down (vr_hull.cpp: his eyes under vr_prone_eye, his box low) the target turns the trunk until the hips
// rest vr_prone_hip_floor over the floor (pronePitchTarget; at most vr_prone_body_angle), eased over vr_prone_body_time;
// the same pivot (the head stays where the headset or the flat-screen eye is). BodyPitch::lying, its share, straightens
// the crouched trunk (solveTorso) and lays the legs on the floor behind (solveLeg); the arms follow the controllers from
// wherever the turned chest puts the shoulders.

#include "vr_avatar.hpp"
#include "vr_avatar_internal.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatmove.hpp"
#include "vr_geom.hpp"
#include "vr_hull.hpp"
#include "vr_ledges.hpp"
#include "vr_progs.hpp"
#include "vr_testkit.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/gtc/quaternion.hpp>

namespace qvr::avatar
{

namespace detail
{

BodyPitch bodyPitch;
// A critically damped spring towards `target` (the exact step: no overshoot, any dt): `angle` and its `rate` both
// continuous, so a target that jumps (under water, out again, a ledge come close) starts and stops the turn smoothly.
// `easeTime`: seconds to cover 95% of a step.
void easePitch(float& angle, float& rate, float target, float easeTime, float dt)
{
    if(dt <= 0.f)
    {
        return;
    }
    const float omega = 4.74f / za::max(0.05f, easeTime); // 1 - (1 + x) e^-x = 0.95 at x = 4.74
    const float x = angle - target;
    const float temp = (rate + omega * x) * dt;
    const float decay = za::exp(-omega * dt);
    rate = (rate - omega * temp) * decay;
    angle = target + (x + temp) * decay;
}

float swimPitchTarget(const SwimPitchIn& in)
{
    if(!in.on || in.waterlevel < 3 || in.onGround)
    {
        return 0.f;
    }
    float share = 1.f;
    if(in.ledgeRange > 0.f && in.ledgeDist >= 0.f)
    {
        // Upright within a third of the range, all of the tilt from its end.
        share = geom::smoothstep(in.ledgeRange * 0.35f, in.ledgeRange, in.ledgeDist);
    }
    return CLAMP(0.f, in.angle, 80.f) * share * (1.f - CLAMP(0.f, in.climb, 1.f));
}

float pronePitchTarget(float most, float drop, float torso, float torsoBack)
{
    const float cap = CLAMP(0.f, most, 100.f);
    if(torso <= 1.f || cap <= 0.f)
    {
        return cap;
    }
    // The straight trunk hangs from the neck's top: the pelvis `torso` under it and `torsoBack` behind. Turned forward by
    // a about the body's left, the pelvis comes torsoBack sin a - torso cos a above the neck (Ry: z' = -x sin a + z cos a);
    // that is -drop at a = phi + asin(-drop / r), phi = atan2(torso, torsoBack). (Until 2026-11-08 the target went past
    // 90 as the eyes stood higher, which lifted the hips off the floor: the body floated, his note.)
    const float r = za::sqrt(torso * torso + torsoBack * torsoBack);
    const float phi = glm::atan(torso, torsoBack);
    const float a = glm::degrees(phi + za::asin(CLAMP(-1.f, -drop / r, 1.f)));
    return CLAMP(0.f, a, cap);
}

float slidePitchTarget(float most, float drop, float torso, float torsoBack)
{
    const float cap = CLAMP(0.f, most, 80.f);
    if(torso <= 1.f || cap <= 0.f)
    {
        return cap;
    }
    // Leant back by b (the pitch -b), the pelvis comes -(torsoBack sin b + torso cos b) = -r cos(b - psi) under the neck,
    // psi = atan2(torsoBack, torso): lowest at b = psi, rising past it; -drop at b = psi + acos(drop / r). (Until
    // 2026-11-08 the lean was vr_flat_slide_body_angle itself: at his 80 the hips rose to the neck's height, the legs
    // reaching down to the floor, the whole body floating: his note.)
    const float r = za::sqrt(torso * torso + torsoBack * torsoBack);
    const float psi = glm::atan(torsoBack, torso);
    const float b = glm::degrees(psi + glm::acos(CLAMP(-1.f, drop / r, 1.f)));
    return CLAMP(0.f, b, cap);
}

} // namespace detail

namespace
{

using namespace detail;

constexpr float LEDGE_WATER_BELOW = 40.f; // units under the lip the water may start (higher, the top is out of reach)

[[nodiscard]] bool liquidAt(const glm::vec3& p)
{
    if(!cl.worldmodel)
    {
        return false;
    }
    vec3_t v{p.x, p.y, p.z};
    return worldtrace::isLiquid(Mod_PointInLeaf(v, cl.worldmodel)->contents);
}

// The nearest ledge of the world's ledge map (units from `head`, within `range`) you can climb out of the water at: its
// top out of the water, water in the drop in front of it no more than LEDGE_WATER_BELOW under the lip. -1: none.
[[nodiscard]] float nearestExitLedge(const glm::vec3& head, float range, glm::vec3* at = nullptr, glm::vec3* out = nullptr)
{
    if(range <= 0.f || !progs::localPlayer() || !sv.worldmodel)
    {
        return -1.f;
    }
    const ledges::Map* m = ledges::of(sv.worldmodel);
    if(!m)
    {
        return -1.f;
    }
    za::Vector<int> near;
    m->nearby(head - glm::vec3{range}, head + glm::vec3{range}, near);
    float best = -1.f;
    for(const int i : near)
    {
        const ledges::Edge& e = m->edges[static_cast<size_t>(i)];
        const float t = CLAMP(0.f, glm::dot(head - e.a, e.dir), e.len);
        const glm::vec3 lip = e.point(t);
        const float d = glm::distance(head, lip);
        if(d > range || (best >= 0.f && d >= best))
        {
            continue;
        }
        const ledges::Sample& sm = m->sampleAt(e, t);
        if(liquidAt(lip + UP * 2.f - e.out * 2.f))
        {
            continue; // the top is under water: no way out there
        }
        const glm::vec3 drop = lip + e.out * (sm.dropOut + 4.f);
        bool water = false;
        for(float below = 8.f; below <= LEDGE_WATER_BELOW && !water; below += 16.f)
        {
            water = liquidAt(drop - UP * below);
        }
        if(water)
        {
            best = d;
            if(at)
            {
                *at = lip;
                if(out)
                {
                    *out = e.out;
                }
            }
        }
    }
    return best;
}

void bodyPitch_f()
{
    const BodyPitch& p = bodyPitch;
    Con_Printf("body_pitch: angle %.2f target %.2f rate %.2f source %s waterlevel %d ground %d ledge %.1f frames %u "
               "lying %.2f\n",
        p.angle, p.target, p.rate, p.source == PitchSource::Swim ? "swim" : p.source == PitchSource::Prone ? "prone" : p.source == PitchSource::Slide ? "slide" : "none",
        p.waterlevel, p.onGround ? 1 : 0, p.ledgeDist, p.updates, p.lying);
    // vr_body_pitch <range>: also the nearest ledge out of the water within that many units of the eyes (finding one).
    if(Cmd_Argc() >= 2)
    {
        const float range = CLAMP(0.f, static_cast<float>(Q_atof(Cmd_Argv(1))), 4096.f);
        const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
        glm::vec3 at{0.f};
        glm::vec3 out{0.f};
        const float d = nearestExitLedge(eye, range, &at, &out);
        Con_Printf("body_pitch: exit ledge within %.0f: %s %.0f %.0f %.0f (%.0f units), towards the water %.2f %.2f\n", range,
            d >= 0.f ? "at" : "none", at.x, at.y, at.z, d, out.x, out.y);
    }
}

// vr_body_pitch_test: the pose's angle states through the ease and the swimming source, at 72 frames a second (the
// headset's, the fixed frames'): out of the water, half in, under, on the bottom, by a ledge, holding on, turned off,
// out again; each start and stop smooth (no frame's turn larger than the fastest the spring makes, the first frame
// after a change hardly moving). Prints "body_pitch_test: N passed, M failed".
void bodyPitchTest_f()
{
    constexpr const char* NAME = "body_pitch_test";
    constexpr float DT = 1.f / 72.f;
    constexpr float ANGLE = 45.f, EASE = 0.8f, RANGE = 64.f;
    testkit::Tally t;
    char what[256];
    float angle = 0.f, rate = 0.f, maxStep = 0.f, firstStep = 0.f, lowest = 0.f;
    const auto run = [&](const SwimPitchIn& in, float seconds) {
        maxStep = 0.f;
        lowest = angle;
        const int frames = static_cast<int>(seconds * 72.f + 0.5f);
        for(int f = 0; f < frames; f++)
        {
            const float before = angle;
            easePitch(angle, rate, swimPitchTarget(in), EASE, DT);
            const float step = za::abs(angle - before);
            maxStep = za::max(maxStep, step);
            lowest = za::min(lowest, angle);
            if(f == 0)
            {
                firstStep = step;
            }
        }
    };
    const auto check = [&](bool ok, const char* fmt, auto... args) {
        snprintf(what, sizeof(what), fmt, args...);
        if(t.check(ok, NAME, what))
        {
            Con_Printf("%s: PASS %s\n", NAME, what);
        }
    };
    // The fastest a 45-degree step turns at 72 Hz: its peak rate is 45 omega / e (omega 4.74 / EASE), a frame of it.
    const float fastest = ANGLE * (4.74f / EASE) / 2.71828f * DT * 1.05f;
    SwimPitchIn in;
    in.on = true;
    in.angle = ANGLE;
    in.ledgeRange = RANGE;

    in.waterlevel = 0;
    run(in, 2.f);
    check(angle == 0.f, "dry: angle %.3f (0)", angle);
    in.waterlevel = 2;
    run(in, 2.f);
    check(angle == 0.f, "half under (waterlevel 2): angle %.3f (0)", angle);

    in.waterlevel = 3;
    run(in, EASE);
    check(angle >= 0.94f * ANGLE && angle <= ANGLE + 0.01f, "under: after the ease time %.3f (95%% of %.0f)", angle, ANGLE);
    check(firstStep < 0.25f, "under: the first frame turns %.4f (a smooth start, < 0.25)", firstStep);
    check(maxStep <= fastest, "under: largest frame step %.3f (<= %.3f)", maxStep, fastest);
    run(in, 2.f);
    check(za::abs(angle - ANGLE) < 0.05f, "swimming: angle %.3f (%.0f)", angle, ANGLE);

    in.ledgeDist = 0.2f * RANGE;
    run(in, 2.f * EASE);
    check(angle < 0.5f && lowest > -0.5f, "by a ledge (%.0f units): angle %.3f, lowest %.3f (upright, no overshoot)", in.ledgeDist,
        angle, lowest);
    in.ledgeDist = 0.675f * RANGE;
    run(in, 3.f * EASE);
    check(za::abs(angle - 0.5f * ANGLE) < 1.f, "half the ledge range (%.0f units): angle %.3f (about %.1f)", in.ledgeDist, angle,
        0.5f * ANGLE);
    in.ledgeDist = 2.f * RANGE;
    run(in, 3.f * EASE);
    check(za::abs(angle - ANGLE) < 0.1f, "past the ledge range: angle %.3f (%.0f)", angle, ANGLE);
    in.ledgeDist = -1.f;

    in.climb = 1.f;
    run(in, 3.f * EASE);
    check(angle < 0.1f, "holding on (climbing): angle %.3f (0)", angle);
    in.climb = 0.f;
    run(in, 3.f * EASE);
    in.onGround = true;
    run(in, 3.f * EASE);
    check(angle < 0.1f, "under, standing on the bottom: angle %.3f (0)", angle);
    in.onGround = false;
    run(in, 3.f * EASE);
    in.on = false;
    run(in, 3.f * EASE);
    check(angle < 0.1f, "turned off: angle %.3f (0)", angle);
    in.on = true;
    run(in, 3.f * EASE);

    in.waterlevel = 2;
    run(in, EASE);
    check(angle <= 0.06f * ANGLE && lowest > -0.5f, "out to waterlevel 2: after the ease time %.3f, lowest %.3f", angle, lowest);
    check(firstStep < 0.25f, "out: the first frame turns %.4f (a smooth start, < 0.25)", firstStep);
    check(maxStep <= fastest, "out: largest frame step %.3f (<= %.3f)", maxStep, fastest);
    // In and out every half second (bobbing at the surface): never past the angle, never below upright.
    float highest = angle;
    for(int i = 0; i < 6; i++)
    {
        in.waterlevel = i % 2 == 0 ? 3 : 2;
        run(in, 0.5f);
        highest = za::max(highest, angle);
        check(maxStep <= fastest && lowest > -0.5f, "bobbing %d: largest step %.3f, lowest %.3f", i, maxStep, lowest);
    }
    check(highest <= ANGLE + 0.01f, "bobbing: highest %.3f (<= %.0f)", highest, ANGLE);

    // Lying down (vr_prone): the neck's top as high as the hips should rest, flat 90; higher, propped up (the trunk
    // slopes down to the hips on the floor: less than 90); never past the most; off at 0; and the ease down to it and back
    // up as smooth as the swimming one's.
    const float flatT = pronePitchTarget(90.f, 0.f, 20.f, 0.f);
    const float proppedT = pronePitchTarget(90.f, 10.f, 20.f, 0.f);
    const float backT = pronePitchTarget(100.f, 0.f, 20.f, 2.f);
    check(za::abs(flatT - 90.f) < 0.01f, "lying flat: target %.2f (90)", flatT);
    check(za::abs(proppedT - 60.f) < 0.01f, "propped up (the neck 10 over the hips' place, a 20 trunk): target %.2f (60)", proppedT);
    check(za::abs(backT - (90.f - glm::degrees(za::asin(2.f / za::sqrt(404.f))))) < 0.01f,
        "the pelvis behind the neck: target %.2f (a little short of 90, level)", backT);
    const float cappedT = pronePitchTarget(45.f, 0.f, 20.f, 0.f);
    check(za::abs(cappedT - 45.f) < 0.01f, "capped by vr_prone_body_angle: target %.2f (45)", cappedT);
    const float offT = pronePitchTarget(0.f, 10.f, 20.f, 0.f);
    check(offT == 0.f, "lying down's angle 0: target %.2f (0)", offT);
    angle = rate = 0.f;
    const auto runTo = [&](float target, float seconds) {
        maxStep = 0.f;
        const int frames = static_cast<int>(seconds * 72.f + 0.5f);
        for(int f = 0; f < frames; f++)
        {
            const float before = angle;
            easePitch(angle, rate, target, 0.6f, DT);
            maxStep = za::max(maxStep, za::abs(angle - before));
            if(f == 0)
            {
                firstStep = za::abs(angle - before);
            }
        }
    };
    const float fastestProne = proppedT * (4.74f / 0.6f) / 2.71828f * DT * 1.05f;
    runTo(proppedT, 0.6f);
    check(angle >= 0.94f * proppedT && angle <= proppedT + 0.01f, "lying down: after 0.6 s %.2f (95%% of %.2f)", angle, proppedT);
    // (The first frame's turn from rest: about the target times (omega dt)^2 / 2, 0.64 here; a smooth start is under a
    // quarter of the fastest frame's.)
    check(firstStep < 0.25f * fastestProne && maxStep <= fastestProne,
        "lying down: first step %.3f (< %.3f, a smooth start), largest %.3f (<= %.3f)", firstStep, 0.25f * fastestProne,
        maxStep, fastestProne);
    runTo(0.f, 0.6f);
    check(angle <= 0.06f * proppedT && maxStep <= fastestProne, "getting up: after 0.6 s %.2f, largest step %.3f", angle, maxStep);
    t.verdict(NAME);
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_body_pitch", bodyPitch_f);
    Cmd_AddCommand("vr_body_pitch_test", bodyPitchTest_f);
    initLie();
}

namespace detail
{

glm::mat3 bodyPitchTurn(const Body& b, float share)
{
    const float angle = bodyPitch.angle * share;
    if(angle == 0.f)
    {
        return glm::mat3{1.f};
    }
    return glm::mat3_cast(glm::angleAxis(glm::radians(angle), b.left));
}

glm::mat3 bodyLeanRollTurn(const Body& b)
{
    // (+ right: about the forward, a positive turn takes the left side up and the right side down.)
    const float roll = flatmove::proneLeanRoll() * bodyPitch.lying;
    if(roll == 0.f)
    {
        return glm::mat3{1.f};
    }
    return glm::mat3_cast(glm::angleAxis(glm::radians(roll), b.fwd));
}

void updateBodyPitch(const hands::State& s, float dt)
{
    BodyPitch& p = bodyPitch;
    p.updates++;
    // Fully under water: Quake's own waterlevel (the eyes under) and ground, from the server's player; a client of
    // another server: the eyes in a liquid, off the ground as the client knows it.
    if(const edict_t* pl = progs::localPlayer())
    {
        p.waterlevel = static_cast<int>(pl->v.waterlevel);
        p.onGround = (static_cast<int>(pl->v.flags) & FL_ONGROUND) != 0;
    }
    else
    {
        p.waterlevel = liquidAt(s.head) ? 3 : 0;
        p.onGround = cl.onground;
    }

    SwimPitchIn in;
    in.on = vr_body_swim_tilt.value != 0.f;
    in.waterlevel = p.waterlevel;
    in.onGround = p.onGround;
    in.angle = vr_body_swim_tilt_angle.value;
    in.ledgeRange = za::max(0.f, vr_body_swim_tilt_ledge.value);
    in.climb = climbing.weight;
    const bool swimming = in.on && in.waterlevel >= 3 && !in.onGround;
    p.ledgeDist = swimming ? nearestExitLedge(s.head, in.ledgeRange) : -1.f;
    in.ledgeDist = p.ledgeDist;

    p.target = swimPitchTarget(in);
    p.source = swimming ? PitchSource::Swim : PitchSource::None;
    p.easeTime = CLAMP(0.1f, vr_body_swim_tilt_time.value, 3.f);
    p.legs = 1.f;
    // Lying down (vr_prone): the server's word for the local player (vr_hull.cpp: his eyes under vr_prone_eye, his box
    // low); a client of another server: his eyes that low over the floor. Swimming wins (lying down needs a floor).
    const int served = hull::localProne();
    // (Flat-screen: the eyes over the box's feet -- his standing eyes are Quake's 46 there, not the calibrated height his
    // head's headHeight is measured from.)
    const float eyeOverFloor = flatplay::active() ? s.head.z - (s.playerOrigin.z - 24.f) : s.headHeight * units::metresToUnits();
    const bool lying = vr_prone.value != 0.f && vr_crouch_hull.value != 0.f &&
                       (served >= 0 ? served == 1 : eyeOverFloor < vr_prone_eye.value);
    // The drawn trunk's own lengths (solveTorso: the pelvis to the head's bind, vr_body_torso_back behind the neck's top,
    // that vr_body_eye_up under the eye) and the neck's top over the floor: lying down and sliding turn the trunk about it
    // until the hips are where they rest.
    const float m2w = units::metresToUnits() * units::bodyScale();
    const float torso = glm::distance(bind().pos[Pelvis], bind().pos[Head]) * m2w;
    // (Over the box's own floor where the server has him: the headset's calibrated height put the hips 2 units up.)
    float eyeOverBox = eyeOverFloor;
    if(const edict_t* pl = progs::localPlayer())
    {
        eyeOverBox = s.head.z - (pl->v.origin[2] + pl->v.mins[2]);
    }
    const float neckOverFloor = eyeOverBox - vr_body_eye_up.value * m2w;
    if(lying && !swimming)
    {
        // The hips down to vr_prone_hip_floor over the floor (flat-screen, from the neck vr_flat_prone_neck_drop lower).
        const float drop = neckOverFloor - flatplay::proneNeckDrop() - CLAMP(0.f, vr_prone_hip_floor.value, 10.f);
        p.target = pronePitchTarget(vr_prone_body_angle.value, drop, torso, vr_body_torso_back.value * m2w);
        p.source = PitchSource::Prone;
        p.easeTime = CLAMP(0.1f, vr_prone_body_time.value, 3.f);
        p.lyingFrom = 1.f;
        p.lyingAngle = za::max(30.f, p.target);
    }
    else if(swimming)
    {
        p.lyingFrom = 0.f;
    }
    else if(p.lyingFrom > 0.f)
    {
        p.easeTime = CLAMP(0.1f, vr_prone_body_time.value, 3.f); // getting up: lying down's own time
    }
    // Sliding (flat-screen play, vr_flatmove.cpp): the trunk leant back about the neck until the hips sit
    // vr_flat_slide_hip_floor over the floor (at most vr_flat_slide_body_angle), so the hips come forward under the lowered
    // eyes, the buttocks and the legs on the floor in front (solveLeg), only the trunk held up.
    if(flatplay::active() && flatmove::slidePose() && !swimming && !lying)
    {
        const float drop = neckOverFloor - za::clamp(vr_flat_slide_hip_floor.value, 0.f, 12.f);
        p.target = -slidePitchTarget(vr_flat_slide_body_angle.value, drop, torso, vr_body_torso_back.value * m2w);
        p.source = PitchSource::Slide;
        p.easeTime = CLAMP(0.05f, vr_flat_slide_body_time.value, 1.f);
        p.slidingFrom = 1.f;
        p.slidingAngle = za::max(10.f, -p.target);
        p.lyingFrom = 0.f;
    }
    else if(p.slidingFrom > 0.f)
    {
        p.easeTime = CLAMP(0.05f, vr_flat_slide_body_time.value, 1.f); // up out of it: the slide's own time
        if(swimming || lying)
        {
            p.slidingFrom = 0.f;
            p.slidingShare = p.slidingRate = 0.f;
        }
    }
    easePitch(p.angle, p.rate, p.target, p.easeTime, dt);
    // (Sliding's share: its own spring, on the angle's clock -- BodyPitch::sliding.)
    easePitch(p.slidingShare, p.slidingRate, p.source == PitchSource::Slide ? 1.f : 0.f, p.easeTime, dt);
    if(p.target == 0.f && za::abs(p.angle) < 0.01f && za::abs(p.rate) < 0.01f)
    {
        p.angle = p.rate = 0.f; // settled upright: exactly as without it
        p.lyingFrom = 0.f;
        p.slidingFrom = 0.f;
        p.slidingShare = p.slidingRate = 0.f;
    }
    p.lying = p.lyingFrom * CLAMP(0.f, p.angle / p.lyingAngle, 1.f);
    p.sliding = p.slidingFrom * CLAMP(0.f, p.slidingShare, 1.f);
}

} // namespace detail

// (The slide's lean back is a negative angle; lying down's and swimming's are positive and stay.)
SlideOff::SlideOff(bool want)
{
    using detail::bodyPitch;
    on = want && bodyPitch.slidingFrom > 0.f;
    if(!on)
    {
        return;
    }
    sliding = bodyPitch.sliding;
    angle = bodyPitch.angle;
    bodyPitch.sliding = 0.f;
    bodyPitch.angle = za::max(bodyPitch.angle, 0.f);
}

void SlideOff::end()
{
    if(on)
    {
        detail::bodyPitch.sliding = sliding;
        detail::bodyPitch.angle = angle;
        on = false;
    }
}

bool slideIn()
{
    return detail::bodyPitch.slidingFrom > 0.f;
}

} // namespace qvr::avatar
