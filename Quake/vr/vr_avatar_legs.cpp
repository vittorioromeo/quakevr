// vr_avatar_legs.cpp -- the body's legs (see vr_avatar.hpp): their clock, walking, wading and swimming, standing
//    steps, and the legs' IK. Moved out of vr_avatar.cpp; what they share is vr_avatar_internal.hpp.

#include "vr_modelmetadata.hpp"
#include "vr_trace.hpp"
#include "vr_progs.hpp"
#include "vr_avatar.hpp"
#include "vr_avatar_internal.hpp"
#include "vr_agarmor.hpp"
#include "vr_selfcollide.hpp"
#include "vr_engine.hpp"
#include "vr_units.hpp"
#include "vr_backend.hpp"
#include "vr_bodycal.hpp"
#include "vr_bodytrack.hpp"
#include "vr_box3d.hpp"
#include "vr_climb.hpp"
#include "vr_cvars.hpp"
#include "vr_lines.hpp"
#include "vr_motion.hpp"
#include "vr_profile.hpp"
#include "vr_ragdoll.hpp"
#include "vr_seated.hpp"
#include "vr_view.hpp"

#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Copysign.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"
#include "vr_rig2.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string.h>
#include "vr_geom.hpp"

namespace qvr::avatar
{

using namespace detail;

namespace
{

// The legs' clock: seconds since they were last posed (0 the first time, and for a second pose in
// the same frame).
double legsPosedAt = -1.0; // legsDeltaTime's last call (vr_gametime; -1: never)

} // namespace

namespace detail
{

[[nodiscard]] float legsDeltaTime()
{
    double& lastTime = legsPosedAt;
    const double now = vr_gametime;
    const float dt = lastTime >= 0.0 ? static_cast<float>(CLAMP(0.0, now - lastTime, 0.1)) : 0.f;
    lastTime = now;
    return dt;
}

} // namespace detail

namespace
{

// Walking: a gait cycle driven by the player's own movement (the stick, not the room). The feet
// take turns: one on the ground going back under the body, the other swinging forward, lifted,
// along the direction of travel. The cadence follows the speed over the stride (so the planted
// foot keeps up with the ground), but only up to a natural rate (vr_body_step_rate steps per
// second at full running speed, somewhat fewer walking; at speed a person's cadence there,
// vr_body_step_run): Quake moves far faster than anyone walks, and legs spinning to keep up
// with a stride as short as the legs' look silly. Strides lengthen and the feet lift higher with
// speed, less so sideways and backwards; the whole of it eases in and out as the player starts
// and stops, and the feet tuck up in the air. The legs' IK does the knees.

} // namespace

namespace detail
{

Gait gait;

} // namespace detail

namespace
{

// Each foot's touchdowns so far (side 0 the left): the walk's (its swing ending) and the standing steps' (Skeleton::steps,
// the bloody footprints' steps).
} // namespace

namespace detail
{

unsigned footSteps[2]{0u, 0u};

} // namespace detail

namespace
{

// Full running speed (Quake's 320 units per second), in metres per second of the body at
// vr_world_scale 1.
constexpr float RUN_SPEED = 320.f / UNITS;

// In water (found at the body on the client: what the player sees). Wading, on the bottom in water
// above the knees, the walk gets heavier (vr_body_wade): shorter strides, the knees lifted higher
// through the water, a slower cadence. Swimming, off the bottom in water above the waist or under
// it, the legs float: they trail behind where the stick moves the player and flutter kick, the legs
// in turn, wider (vr_body_swim_kick) and faster (vr_body_swim_kick_rate) the further the stick
// pushes, so that the stick seems to work the legs while the hands swim. With the stick left alone
// they tread water: slow small kicks, the knees bent. Each eases in and out.

} // namespace

namespace detail
{

Water water;

} // namespace detail

namespace
{

// How high the liquid stands above the body's floor, in metres of the body: sampled up the pelvis's
// column from the floor to a little above `top` (solid below the water is the bottom, the floor
// being where the head's height puts it).
[[nodiscard]] float waterSurface(const Body& b, float top)
{
    if(!cl.worldmodel)
    {
        return 0.f;
    }

    constexpr float STEP = 0.1f;
    const glm::vec3& p = b.bones[Pelvis].pos;
    float surface = 0.f;
    bool wet = false;
    for(float h = 0.05f; h < top + 0.3f; h += STEP)
    {
        vec3_t v{p.x, p.y, b.floorZ + h * b.m2w};
        const int c = Mod_PointInLeaf(v, cl.worldmodel)->contents;
        if(worldtrace::isLiquid(c))
        {
            wet = true;
            surface = h + STEP * 0.5f;
        }
        else if(wet && c != CONTENTS_SOLID)
        {
            break;
        }
    }
    return surface;
}

} // namespace

namespace detail
{

void updateWater(const Body& b, const hands::State& s, float dt)
{
    const float head = (s.head.z - b.floorZ) / b.m2w;
    const float surface = waterSurface(b, head);
    const bool under = surface >= head;
    const bool swimming = under || (!cl.onground && surface > 0.85f);
    const float deep = CLAMP(0.f, (surface - 0.3f) / 0.45f, 1.f);
    const float wading = cl.onground && !swimming ? deep * deep * (3.f - 2.f * deep) : 0.f;

    // The stick as the server steers it (the head's angles, VR_MoveAngles), swimming up and down
    // too; full at the walking speed.
    glm::vec3 f, r, u;
    hands::angleVectors(s.headAngles, f, r, u);
    const glm::vec3 wish = f * cl.cmd.forwardmove + r * cl.cmd.sidemove + UP * cl.cmd.upmove;
    const float len = glm::length(wish);
    const float stick = za::min(1.f, len / za::max(1.f, cl_forwardspeed.value));

    const float ease = 1.f - za::exp(-4.f * dt);
    water.swim += ((swimming ? 1.f : 0.f) - water.swim) * ease;
    water.wade += (wading - water.wade) * ease;
    water.stick += (stick - water.stick) * (1.f - za::exp(-6.f * dt));
    water.dir += ((len > 1.f ? wish / len * stick : glm::vec3{0.f}) - water.dir) * (1.f - za::exp(-5.f * dt));

    // Treading water, about a kick in 1.4 seconds; at full stick, vr_body_swim_kick_rate more a second.
    const float rate = 0.7f + CLAMP(0.f, vr_body_swim_kick_rate.value, 5.f) * water.stick;
    water.phase = za::fmod(water.phase + rate * dt * glm::two_pi<float>(), glm::two_pi<float>());
}

} // namespace detail

namespace
{

// The offset of a foot (side 0 left, 1 right) from where it stands, in world units. In the first
// half of its cycle the foot is on the ground, going back from half a stride ahead to half a
// stride behind at an even pace; in the second it swings forward again, lifted.
[[nodiscard]] glm::vec3 gaitOffset(const Body& b, int side)
{
    const float u = za::fmod(gait.phase / glm::two_pi<float>() + (side == 0 ? 0.f : 0.5f), 1.f);
    float along = 0.5f - 2.f * u;
    float lift = 0.f;
    if(u >= 0.5f)
    {
        const float s = (u - 0.5f) * 2.f;
        along = -0.5f + s * s * (3.f - 2.f * s);
        lift = za::sin(glm::pi<float>() * s);
    }
    return (gait.dir * (along * gait.stride * gait.amount) + UP * (lift * gait.lift * gait.amount + 0.18f * gait.air)) *
           b.m2w;
}

} // namespace

namespace detail
{

void updateGait(const Body& b, float dt)
{
    const glm::vec3 vel{cl.velocity[0], cl.velocity[1], 0.f};
    const float speed = glm::length(vel) / b.m2w; // metres per second, of the body
    const bool walking = cl.onground && speed > 0.3f && vr_body_walk.value;

    const float ease = 1.f - za::exp(-8.f * dt);
    gait.amount += ((walking ? 1.f : 0.f) - gait.amount) * ease;
    gait.air += ((cl.onground ? 0.f : 1.f) - gait.air) * ease;
    if(walking)
    {
        gait.dir = glm::normalize(vel);
        const float run = za::min(1.f, speed / RUN_SPEED);
        const float sideways = za::abs(glm::dot(gait.dir, b.left));
        const float backwards = za::max(0.f, -glm::dot(gait.dir, b.fwd));
        // Wading: shorter, higher steps, fewer of them.
        const float wade = water.wade * CLAMP(0.f, vr_body_wade.value, 2.f);
        gait.stride = CLAMP(0.45f, 0.45f + speed * 0.06f, 0.8f) * (1.f - 0.45f * sideways) * (1.f - 0.2f * backwards) *
                      za::max(0.3f, 1.f - 0.2f * wade);
        gait.lift = 0.07f + 0.07f * run + 0.1f * wade;

        // Steps per second: as many as it takes to cover the ground, up to the cap. At speed the cap rises to a person's
        // cadence there (the footprints' steps: a run's 5-6 a second), as far as vr_body_step_run says (0: the cap
        // alone, the legs stepping half as often as the prints at a run); the planted foot slides less, not more.
        const float cap =
            CLAMP(0.5f, vr_body_step_rate.value, 6.f) * (0.7f + 0.3f * run) * za::max(0.3f, 1.f - 0.3f * wade);
        const float person = speed / personStep(speed) * za::max(0.3f, 1.f - 0.3f * wade);
        const float rate = za::min(speed / gait.stride, za::max(cap, CLAMP(0.f, vr_body_step_run.value, 1.5f) * person));
        const float before = gait.phase;
        gait.phase = za::fmod(gait.phase + rate * dt * glm::pi<float>(), glm::two_pi<float>());
        // A foot's swing ends (its cycle wraps: the left's at the phase's wrap, the right's at half of it): a touchdown.
        if(gait.amount > 0.5f)
        {
            if(gait.phase < before)
            {
                footSteps[0]++;
            }
            if(before < glm::pi<float>() && (gait.phase >= glm::pi<float>() || gait.phase < before))
            {
                footSteps[1]++;
            }
        }
    }
}

} // namespace detail

namespace
{

// Standing, each foot stays planted where it is, turned its own way, while the body turns and
// sways above it. When the body has turned more than vr_body_turn_step degrees from the feet (a
// snap or smooth turn, or turning round in the room; the foot on the side of the turn leads) or
// moved too far from them, a foot steps back under the body, and the other follows to square up:
// one or two small steps, as a person turning on the spot takes. While walking or in the air, the
// feet stay under the body.

} // namespace

namespace detail
{

Stance stance;

} // namespace detail

namespace
{

constexpr float STEP_TIME = 0.3f;     // seconds a step takes
constexpr float STEP_LIFT = 0.06f;    // metres
constexpr float STEP_DISTANCE = 0.25f; // metres the body may move from the feet before they step

// Degrees from `from` to `to`, -180 .. 180.
using geom::yawDelta;

[[nodiscard]] glm::vec3 yawForward(float yaw)
{
    const float r = glm::radians(yaw);
    return {za::cos(r), za::sin(r), 0.f};
}

// Where the foot of `side` stands under the body: under where it stands, the head less its lean (the
// balance point: crouching pushes the hips back and the knees forward; vr_body_legs_back further back,
// to match a posture), as far apart as the hips.
[[nodiscard]] glm::vec2 homeOf(const Body& b, const glm::vec3& stand, int side)
{
    const glm::vec3 centre = stand - b.fwd * (vr_body_legs_back.value * b.m2w);
    const glm::vec3 h = centre + b.left * (bind().pos[side == 0 ? ThighL : ThighR].y * b.m2w);
    return {h.x, h.y};
}

} // namespace

namespace detail
{

void updateStance(const Body& b, const glm::vec3& stand, float dt)
{
    const float bodyYaw = glm::degrees(za::atan2(b.fwd.y, b.fwd.x));
    if(dt > 0.f)
    {
        const float change = za::abs(yawDelta(bodyYaw, stance.lastYaw));
        const float rate = change < 20.f ? change / dt : 0.f;
        stance.yawRate += (rate - stance.yawRate) * (1.f - za::exp(-8.f * dt));
    }
    stance.lastYaw = bodyYaw;

    za::Array<glm::vec2, 2> home;
    for(int side = 0; side < 2; side++)
    {
        home[side] = homeOf(b, stand, side);
    }

    // New, or far away (a teleport, a respawn): stand there.
    bool reset = !stance.valid;
    for(int side = 0; side < 2; side++)
    {
        reset = reset || glm::distance(stance.feet[side].pos, home[side]) > 1.2f * b.m2w;
    }
    if(reset)
    {
        for(int side = 0; side < 2; side++)
        {
            stance.feet[side] = Foot{home[side], bodyYaw};
        }
        stance.follow = -1;
        stance.moving = false;
        stance.valid = true;
        return;
    }

    // Walking or in the air: under the body, easing there from where the feet stood (they go with
    // the body, their distance from where they belong shrinking).
    if(gait.amount > 0.02f || gait.air > 0.02f)
    {
        const float keep = za::exp(-10.f * dt);
        for(int side = 0; side < 2; side++)
        {
            Foot& f = stance.feet[side];
            if(!stance.moving)
            {
                f.slack = f.pos - home[side];
            }
            f.slack *= keep;
            f.pos = home[side] + f.slack;
            f.yaw += yawDelta(bodyYaw, f.yaw) * (1.f - keep);
            f.step = -1.f;
            f.lift = 0.f;
        }
        stance.moving = true;
        stance.follow = -1;
        return;
    }
    stance.moving = false;

    // A planted foot left too far behind a turn pivots round after the body (the rest waits for
    // its step).
    const float turnLimit = CLAMP(10.f, vr_body_turn_step.value, 180.f);
    const float maxLag = za::min(turnLimit + 25.f, 180.f);
    for(Foot& f : stance.feet)
    {
        const float lag = yawDelta(bodyYaw, f.yaw);
        if(f.step < 0.f && za::abs(lag) > maxLag)
        {
            f.yaw = bodyYaw - za::copysign(maxLag, lag);
        }
    }

    // A step under way (one foot at a time) goes to where the foot belongs now.
    for(int side = 0; side < 2; side++)
    {
        Foot& f = stance.feet[side];
        if(f.step < 0.f)
        {
            continue;
        }
        // Quicker steps while turning quickly (a smooth turn), to keep up.
        const float quicken = CLAMP(1.f, 1.f + stance.yawRate / 200.f, 2.5f);
        f.step = za::min(1.f, f.step + dt * quicken / STEP_TIME);
        const float e = f.step * f.step * (3.f - 2.f * f.step);
        f.pos = glm::mix(f.from, home[side], e);
        f.yaw = f.fromYaw + yawDelta(bodyYaw, f.fromYaw) * e;
        f.lift = za::sin(glm::pi<float>() * f.step) * STEP_LIFT;
        if(f.step >= 1.f)
        {
            f.step = -1.f;
            f.lift = 0.f;
            footSteps[side]++;
            stance.follow = stance.follow == side ? -1 : 1 - side;
        }
        return;
    }

    za::Array<float, 2> turned, moved;
    for(int side = 0; side < 2; side++)
    {
        turned[side] = yawDelta(bodyYaw, stance.feet[side].yaw);
        moved[side] = glm::distance(stance.feet[side].pos, home[side]) / b.m2w;
    }

    int stepping = -1;
    if(stance.follow >= 0)
    {
        // Squaring up after the other foot's step, if there is anything to square.
        const int side = stance.follow;
        stance.follow = -1;
        if(za::abs(turned[side]) > 5.f || moved[side] > 0.04f)
        {
            stepping = side;
        }
    }
    else if(za::max(za::abs(turned[0]), za::abs(turned[1])) > turnLimit)
    {
        stepping = turned[0] + turned[1] > 0.f ? 0 : 1; // turned left: the left foot leads
    }
    else if(za::max(moved[0], moved[1]) > STEP_DISTANCE)
    {
        stepping = moved[0] > moved[1] ? 0 : 1;
    }

    if(stepping >= 0)
    {
        Foot& f = stance.feet[stepping];
        f.from = f.pos;
        f.fromYaw = f.yaw;
        f.step = 0.f;
    }
}

} // namespace detail

namespace
{

// Swimming: where the foot (the ankle) of `side` goes from the hip, the leg `reach` long (world
// units). The legs trail behind where the stick moves the player, as far as 42 degrees from hanging
// straight down (half that going backwards: the torso stays upright under the head), and kick in
// turn across that, in the plane of the body's forward and the way it goes; the knees bend through
// each kick, more treading water.
[[nodiscard]] glm::vec3 swimFoot(const Body& b, int side, const glm::vec3& hip, float reach)
{
    const glm::vec3 across{water.dir.x, water.dir.y, 0.f}; // the stick's share, sideways of the body's up
    const float len = glm::length(across);
    const float backwards = len > 1e-3f ? za::max(0.f, -glm::dot(across, b.fwd) / len) : 0.f;
    const glm::vec3 trail =
        safeNormalize(-UP * (1.f + 0.5f * za::min(0.f, water.dir.z)) - across * (0.9f * (1.f - 0.5f * backwards)), -UP);

    const glm::vec3 kickDir = b.fwd + across * 0.8f;
    const glm::vec3 kickAxis = safeNormalize(kickDir - trail * glm::dot(kickDir, trail), b.fwd);
    const float amp = glm::radians(6.f + 16.f * water.stick) * CLAMP(0.f, vr_body_swim_kick.value, 2.f);
    const float phase = water.phase + (side == 0 ? 0.f : glm::pi<float>());
    const float angle = amp * za::sin(phase);
    const glm::vec3 dir = trail * za::cos(angle) + kickAxis * za::sin(angle);
    const float bent = 0.97f - 0.08f * (0.5f + 0.5f * za::cos(phase)) - 0.08f * (1.f - water.stick);
    return hip + dir * (reach * bent);
}

} // namespace

namespace detail
{

// Legs from the hips to the feet where they stand (updateStance) and walk (the gait), the knees
// over the toes, floating behind the body swimming (swimFoot); collapsed when not shown. `still`
// gets the thigh's rotation with the legs standing still under the body instead (the feet where
// they belong, no step, walk or water), for what the thigh carries (ThighMotion).
void solveLeg(Body& b, const glm::vec3& stand, int side, bool shown, glm::mat3& still)
{
    const Bind& bd = bind();
    const int thigh = side == 0 ? ThighL : ThighR;
    const int calf = thigh + 1;
    const int foot = thigh + 2;

    Bone& t = b.bones[thigh];
    t = Bone{};
    t.pos = childPos(b, thigh);

    Bone& c = b.bones[calf];
    Bone& f = b.bones[foot];
    c = Bone{};
    f = Bone{};

    if(!shown)
    {
        for(Bone* bone : {&t, &c, &f})
        {
            bone->pos = t.pos;
            bone->size = COLLAPSED;
        }
        still = t.rot;
        return;
    }

    const Foot& ft = stance.feet[side];
    const glm::vec3 footFwd = yawForward(ft.yaw);
    const glm::vec3 footLeft{-footFwd.y, footFwd.x, 0.f};
    const glm::vec3 outward = side == 0 ? footLeft : -footLeft;
    const float a = boneLength(thigh, calf) * b.legM2w;
    const float l = boneLength(calf, foot) * b.legM2w;
    const float footZ = b.floorZ + bd.pos[foot].z * b.m2w;

    // Standing still: the knee over the foot, both straight ahead.
    {
        const glm::vec2 home = homeOf(b, stand, side);
        glm::vec3 bend;
        const glm::vec3 knee = twoBone(t.pos, {home.x, home.y, footZ}, a, l,
            b.fwd + (side == 0 ? b.left : -b.left) * 0.1f, b.fwd, bend);
        still = basis(knee - t.pos, bend);
    }

    // The knee points between the body's forward and the foot's (swimming, the body's).
    glm::vec3 target = glm::vec3{ft.pos.x, ft.pos.y, footZ + ft.lift * b.m2w} + gaitOffset(b, side);
    glm::vec3 kneeDir = safeNormalize(b.fwd + footFwd, b.fwd);
    const float swim = water.swim;
    if(swim > 1e-3f)
    {
        target = glm::mix(target, swimFoot(b, side, t.pos, a + l), swim);
        kneeDir = safeNormalize(glm::mix(kneeDir, b.fwd, swim), b.fwd);
    }
    // Climbing: to its foothold or hanging (updateClimbing), the knee towards the face and a little out (hanging, a
    // little forward).
    const float climbW = climbing.weight;
    const ClimbFoot& cf = climbing.feet[side];
    if(climbW > 1e-3f)
    {
        target = glm::mix(target, cf.now, climbW);
        const glm::vec3 out = climbOutward(side);
        const glm::vec3 pole = cf.held ? climbing.in + out * 0.35f + UP * 0.15f : climbing.in * 0.6f + b.fwd * 0.4f + out * 0.1f;
        kneeDir = safeNormalize(glm::mix(kneeDir, safeNormalize(pole, b.fwd), climbW), b.fwd);
    }
    // Body tracking's generated legs (vr_bodytrack_legs), less what climbing and swimming take, and (vr_bodytrack_legs_walk)
    // what the walk, the run and the air take: moving, the gait drives the legs; standing still, the tracked legs show.
    const float gaitOver = vr_bodytrack_legs_walk.value != 0.f ? za::max(gait.amount, gait.air) : 0.f;
    bodytrack::leg(side, t.pos, a + l, footZ, (1.f - climbW) * (1.f - swim) * (1.f - gaitOver), target, kneeDir);

    glm::vec3 bend;
    const glm::vec3 knee = twoBone(t.pos, target, a, l, kneeDir + outward * 0.1f, kneeDir, bend);
    t.rot = basis(knee - t.pos, bend);

    c.pos = knee;
    const glm::vec3 shinDir = safeNormalize(target - knee, -UP);
    c.rot = basis(shinDir, bend);

    // The foot keeps its bind direction, turned to its own yaw; swimming, the toes point along the
    // shin, the top of the foot towards the knee's front.
    const glm::vec3 toe = bd.toe[side] - bd.pos[foot];
    f.pos = knee + shinDir * l;
    f.rot = basis(footFwd * toe.x + footLeft * toe.y + UP * toe.z, UP);
    if(swim > 1e-3f)
    {
        const glm::mat3 pointed = basis(shinDir + bend * 0.35f, bend);
        f.rot = glm::mat3_cast(glm::slerp(glm::quat_cast(f.rot), glm::quat_cast(pointed), swim));
    }
    if(climbW > 1e-3f)
    {
        // On a hold: flat, the toes into the face; hanging: the toes half pointed along the shin.
        const glm::vec3 cFwd = cf.fwd;
        const glm::vec3 cLeft{-cFwd.y, cFwd.x, 0.f};
        const glm::mat3 held = basis(cFwd * toe.x + cLeft * toe.y + UP * toe.z, UP);
        const glm::mat3 hanging = basis(safeNormalize(climbing.in + shinDir, climbing.in) + bend * 0.2f, bend);
        const glm::mat3 climbRot = glm::mat3_cast(glm::slerp(glm::quat_cast(hanging), glm::quat_cast(held), cf.flat));
        f.rot = glm::mat3_cast(glm::slerp(glm::quat_cast(f.rot), glm::quat_cast(climbRot), climbW));
    }
}

} // namespace detail

} // namespace qvr::avatar
