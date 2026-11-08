// vr_avatar_lie.cpp -- the lying body's probe (ROUND24.md, "Lying body: slide and prone, on the floor"): the local body
// as last posed (avatar::pose captures it while a report or the test asks), measured against the real floor under the
// player and the drawn eye. vr_body_lie_report prints it once; vr_body_lie_test lies down, gets up and slides (a
// flat-screen game you host, a long gun held, the script holding +forward) and checks the pose: the trunk on the floor
// lying down, the legs straight, the feet on the floor, the holsters and pouches carried with the body (never in front
// of the camera), sliding with no two-handed stance turn and no walk bob. Since "Lying body 2" (ROUND24.md): flat, the
// chest low, the knees and feet resting on the floor (not sunk), the shotgun sighted lying down with its elbows under
// the eye; the slide's buttocks on the floor, the hips' holsters by the legs, the ammo pouch at the belt, the hips
// eased round after a turn of the view past the legs' most twist. Prints "body_lie_test: N passed, M failed".

#include "vr_avatar.hpp"
#include "vr_avatar_internal.hpp"
#include "vr_body.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatmove.hpp"
#include "vr_flatplay.hpp"
#include "vr_hull.hpp"
#include "vr_progs.hpp"
#include "vr_slidesound.hpp"
#include "vr_testkit.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/glm.hpp>

#include <cstdarg>

namespace qvr::avatar::detail
{

namespace
{

// The worn items checked: the six holsters, the grenade pouch, the ammo pouch; each against the bone that carries it.
constexpr int ITEMS = body::HolsterCount + 2;
constexpr const char* itemNames[ITEMS] = {"lshoulder", "rshoulder", "lhip", "rhip", "lupper", "rupper", "grenpouch", "ammopouch"};

// (The hips' holsters ride the thighs, about the hip joints: in a slide they turn with the legs out in front.)
[[nodiscard]] int carrierOf(int item)
{
    return item == body::LeftHip ? ThighL : item == body::RightHip ? ThighR : item >= body::HolsterCount ? Pelvis : Chest;
}

struct Probe
{
    bool armed{false};
    unsigned frames{0};
    Body body;
    glm::vec3 eye{0.f}; // the posed head (the eye the body is posed under)
};

Probe probe;

struct Measure
{
    float floorZ{0.f};       // the real floor under the player (a trace down)
    float pelvis{0.f}, spine{0.f}, chest{0.f}; // joints over the floor, units
    float knee[2]{}, foot[2]{};                // calf joint (the knee) and foot joint over the floor
    float bend[2]{};         // knee bend, degrees (0 straight)
    float itemDist[ITEMS]{}; // each item from its carrying bone, units
    float itemCone[ITEMS]{}; // degrees off the view's forward
    float itemNear[ITEMS]{}; // units from the eye
    float itemZ[ITEMS]{};    // over the floor
    float elbowZ[2]{}, elbowCone[2]{}, elbowNear[2]{}; // the elbows (the forearms' joints): over the floor, off the view, from the eye
    float m2w{1.f};
    float yawOff{0.f};       // the body's yaw off the view's, degrees
    glm::vec3 ride{0.f};     // the posed head off the state's (flatplay::bodyRide: the drawn body's ride on the view)
    float eyeOverFloor{0.f}; // the drawn eye over the floor
    float followOff[2]{};    // the Follower's pelvis and chest (torso of the state) off the drawn ones, units
};

[[nodiscard]] float floorUnder(const glm::vec3& p)
{
    const trace_t tr = worldtrace::world(p, p - glm::vec3{0.f, 0.f, 256.f}, true);
    return tr.endpos[2];
}

[[nodiscard]] float angleDeg(const glm::vec3& a, const glm::vec3& b)
{
    const float d = glm::dot(geom::safeNormalize(a, UP), geom::safeNormalize(b, UP));
    return glm::degrees(glm::acos(glm::clamp(d, -1.f, 1.f)));
}

[[nodiscard]] Measure measure()
{
    Measure m;
    const Body& b = probe.body;
    m.m2w = b.m2w;
    const glm::vec3 org{cl_entities[cl.viewentity].origin[0], cl_entities[cl.viewentity].origin[1],
        cl_entities[cl.viewentity].origin[2]};
    m.floorZ = floorUnder(org);
    m.pelvis = b.bones[Pelvis].pos.z - m.floorZ;
    m.spine = b.bones[Spine].pos.z - m.floorZ;
    m.chest = b.bones[Chest].pos.z - m.floorZ;
    for(int side = 0; side < 2; side++)
    {
        const int thigh = side == 0 ? ThighL : ThighR;
        m.knee[side] = b.bones[thigh + 1].pos.z - m.floorZ;
        m.foot[side] = b.bones[thigh + 2].pos.z - m.floorZ;
        m.bend[side] = angleDeg(b.bones[thigh + 1].pos - b.bones[thigh].pos, b.bones[thigh + 2].pos - b.bones[thigh + 1].pos);
    }
    const hands::State& s = hands::current();
    const body::HolsterPositions h = body::holsterPositions(s);
    glm::vec3 items[ITEMS];
    for(int i = 0; i < body::HolsterCount; i++)
    {
        items[i] = h[i];
    }
    items[body::HolsterCount] = body::pouchPosition(s);
    items[body::HolsterCount + 1] = body::ammoPouchPosition(s);
    glm::vec3 fwd, right, up;
    hands::angleVectors({r_refdef.viewangles[0], r_refdef.viewangles[1], r_refdef.viewangles[2]}, fwd, right, up);
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    for(int i = 0; i < ITEMS; i++)
    {
        m.itemDist[i] = glm::distance(items[i], b.bones[carrierOf(i)].pos);
        m.itemCone[i] = angleDeg(items[i] - eye, fwd);
        m.itemNear[i] = glm::distance(items[i], eye);
        m.itemZ[i] = items[i].z - m.floorZ;
    }
    for(int side = 0; side < 2; side++)
    {
        const glm::vec3 e = b.bones[side == 0 ? ForearmL : ForearmR].pos;
        m.elbowZ[side] = e.z - m.floorZ;
        m.elbowCone[side] = angleDeg(e - eye, fwd);
        m.elbowNear[side] = glm::distance(e, eye);
    }
    m.yawOff = geom::yawDelta(geom::yawOf(b.fwd), cl.viewangles[YAW]);
    m.ride = probe.eye - s.head;
    const Torso tr = torso(s);
    m.followOff[0] = glm::distance(tr.pelvis.pos, b.bones[Pelvis].pos);
    m.followOff[1] = glm::distance(tr.chest.pos, b.bones[Chest].pos);
    m.eyeOverFloor = eye.z - m.floorZ;
    return m;
}

void printMeasure(const char* tag, const Measure& m)
{
    Con_Printf("%s: over the floor (units): pelvis %.1f spine %.1f chest %.1f knees %.1f %.1f feet %.1f %.1f; knee bend %.0f %.0f "
               "deg; body yaw off the view %.1f; m2w %.1f; eye %.1f; ride %.1f %.1f %.1f; follower off %.1f %.1f\n",
        tag, m.pelvis, m.spine, m.chest, m.knee[0], m.knee[1], m.foot[0], m.foot[1], m.bend[0], m.bend[1], m.yawOff, m.m2w,
        m.eyeOverFloor, m.ride.x, m.ride.y, m.ride.z, m.followOff[0], m.followOff[1]);
    Con_Printf("%s: elbows over the floor %.1f %.1f, off the view %.0f %.0f deg, from the eye %.1f %.1f\n", tag, m.elbowZ[0],
        m.elbowZ[1], m.elbowCone[0], m.elbowCone[1], m.elbowNear[0], m.elbowNear[1]);
    for(int i = 0; i < ITEMS; i++)
    {
        Con_Printf("%s:   %s: %.1f from its bone, %.1f from the eye, %.0f deg off the view, %.1f over the floor\n", tag, itemNames[i],
            m.itemDist[i], m.itemNear[i], m.itemCone[i], m.itemZ[i]);
    }
}

// An item in front of the camera: within this cone off the view's forward and this close to the eye.
constexpr float FRONT_CONE = 35.f;
constexpr float FRONT_NEAR = 30.f;

struct LieTest
{
    int phase{0};
    double phaseAt{0.0};
    int settle{0};
    bool vr{false}; // the headset (the mock's head lowered to the floor), lying down only
    testkit::Tally tally;
    Measure standing;
    // The slide's frames: the pelvis over the floor (its range: the bob), the eye over the box's origin.
    int slideFrames{0};
    float pelvisMin{1e9f}, pelvisMax{-1e9f};
    float eyeMin{1e9f}, eyeMax{-1e9f};
    float yawMax{0.f}, bendMax{0.f}, footMax{0.f};
    int bendAt{0};
    float bendPelvis{0.f};
    float stanceBefore{0.f};
    // The slide's sound: slides started before, the scrape's level at the first and last frames of the full pose.
    int startsBefore{0};
    float levelFirst{-1.f}, levelLast{0.f};
    // The second slide, the view turned off its way: the legs' yaw off the slide's, the chest's off the view's (degrees).
    int turnFrames{0};
    float legsOffMax{0.f}, chestOffMax{0.f};
    // Then the view turned 60 more, past the legs' most twist: the hips dragged round, eased (vr_flat_slide_turn_smooth):
    // their largest step in a frame, and how far they went.
    bool turned2{false};
    int turn2Frames{0};
    float hipYawLast{0.f}, hipYawFrom{0.f}, hipStepMax{0.f}, hipWent{0.f};
    bool frontSeen{false};
    char frontWhat[96]{};
};

LieTest test;

void check(bool ok, const char* fmt, ...)
{
    char line[320];
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    if(ok)
    {
        Con_Printf("body_lie_test: PASS %s\n", line);
    }
    test.tally.check(ok, "body_lie_test", line);
}

void next(int phase)
{
    test.phase = phase;
    test.phaseAt = vr_gametime;
    test.settle = 0;
}

[[nodiscard]] bool itemsOnBody(const Measure& m, const char* what)
{
    float worst = 0.f;
    int worstAt = 0;
    for(int i = 0; i < ITEMS; i++)
    {
        const float d = za::abs(m.itemDist[i] - test.standing.itemDist[i]);
        if(d > worst)
        {
            worst = d;
            worstAt = i;
        }
    }
    int front = -1;
    for(int i = 0; i < ITEMS; i++)
    {
        if(m.itemCone[i] < FRONT_CONE && m.itemNear[i] < FRONT_NEAR)
        {
            front = i;
        }
    }
    check(worst < 3.f, "%s: the holsters and pouches stay with their bones (worst %s, %.1f units off standing's)", what,
        itemNames[worstAt], worst);
    check(front < 0, "%s: nothing worn in front of the camera (%s)", what,
        front < 0 ? "none within 35 deg and 30 units" : itemNames[front]);
    return worst < 3.f && front < 0;
}

void testFrame()
{
    if(test.phase == 0)
    {
        return;
    }
    const edict_t* pl = progs::localPlayer();
    if(!pl || flatplay::active() == test.vr)
    {
        check(false, test.vr ? "a headset game you host is needed (vr: not -flatscreenmode)" : "a flat-screen game you host is needed");
        next(99);
    }
    const double in = vr_gametime - test.phaseAt;
    if(in > 12.0 && test.phase != 99)
    {
        check(false, "timed out in step %d (lying %.2f, sliding %.2f, angle %.1f)", test.phase, bodyPitch.lying,
            bodyPitch.sliding, bodyPitch.angle);
        Cbuf_InsertText("-flatcrouch\n-forward\n");
        next(99);
    }
    switch(test.phase)
    {
        case 1: // standing still: the reference
            if(++test.settle >= 20)
            {
                test.standing = measure();
                printMeasure("body_lie_test standing", test.standing);
                // (The headset: the mock's eyes 0.28 m over the floor, looking ahead: lying on the front, the head up.)
                Cbuf_InsertText(test.vr ? "vr_mock_hand head 0 0.28 0 0 0 0\n" : "vr_prone_toggle\n");
                next(2);
            }
            break;
        case 2: // lying down, settled
            if(bodyPitch.lying > 0.995f && za::abs(bodyPitch.rate) < 0.5f && ++test.settle >= 20)
            {
                const Measure m = measure();
                printMeasure("body_lie_test prone", m);
                const float k = m.m2w;
                const float hip = vr_prone_hip_floor.value;
                check(za::abs(m.pelvis - hip) < 1.f && m.chest > m.pelvis && m.chest < m.eyeOverFloor - 2.f,
                    "prone: the hips on the floor, the trunk sloping up to the neck (pelvis %.1f over it, %.1f wanted; chest %.1f, "
                    "under the eye's %.1f)",
                    m.pelvis, hip, m.chest, m.eyeOverFloor);
                check(m.bend[0] < 15.f && m.bend[1] < 15.f, "prone: the legs straight (knee bend %.0f %.0f deg, under 15)", m.bend[0], m.bend[1]);
                check(za::max(m.foot[0], m.foot[1]) < 0.16f * k && za::max(m.knee[0], m.knee[1]) < 0.16f * k,
                    "prone: the knees and feet on the floor (knees %.1f %.1f, feet %.1f %.1f)", m.knee[0], m.knee[1], m.foot[0],
                    m.foot[1]);
                // (The ankles vr_prone_foot_lift up: the shins and feet resting on the floor, not sunk in: his note 2026-11-08.)
                const float lowest = za::min(za::min(m.foot[0], m.foot[1]), za::min(m.knee[0], m.knee[1]));
                const float lift = vr_prone_foot_lift.value;
                check(lift <= 0.f || lowest > 0.4f * lift, "prone: the knees and feet not sunk into the floor (lowest joint %.1f over it, "
                    "the ankles asked %.1f)", lowest, lift);
                if(!test.vr)
                {
                    // (The flat eye at vr_flat_prone_eye: the chest almost on the floor: his note 2026-11-08, 8.7 before.)
                    check(m.chest < 0.22f * k, "prone: the chest low, almost on the floor (chest %.1f over it, under %.1f; eye %.1f)",
                        m.chest, 0.22f * k, m.eyeOverFloor);
                }
                (void)itemsOnBody(m, "prone");
                // (Flat: the shotgun in the main hand for the next step, his screenshots' gun.)
                Cbuf_InsertText(test.vr ? "vr_mock_hand head\n" : "impulse 154\n");
                next(test.vr ? 99 : 21);
            }
            break;
        case 21: // flat, lying down, the shotgun down the sights (the gun at the eye): the elbows out and down, not over the view
            if(++test.settle == 20)
            {
                Cbuf_InsertText("vr_flat_aim 1\n");
            }
            if(test.settle >= 70)
            {
                const Measure m = measure();
                printMeasure("body_lie_test prone sighted", m);
                const float eyeZ = m.eyeOverFloor;
                const float high = za::max(m.elbowZ[0], m.elbowZ[1]);
                check(high < eyeZ - 1.f, "prone, sighted: the elbows under the eye (%.1f %.1f over the floor; the eye %.1f)", m.elbowZ[0],
                    m.elbowZ[1], eyeZ);
                int front = -1;
                for(int side = 0; side < 2; side++)
                {
                    if(m.elbowCone[side] < FRONT_CONE && m.elbowNear[side] < FRONT_NEAR)
                    {
                        front = side;
                    }
                }
                check(front < 0, "prone, sighted: no elbow in front of the camera (off the view %.0f %.0f deg, %.1f %.1f from the eye)",
                    m.elbowCone[0], m.elbowCone[1], m.elbowNear[0], m.elbowNear[1]);
                Cbuf_InsertText("vr_flat_aim 0\nimpulse 3\nvr_prone_toggle\n"); // (the super shotgun back: the slide's stance turn)
                next(3);
            }
            break;
        case 3: // up again
            if(bodyPitch.lying < 0.01f && za::abs(bodyPitch.angle) < 0.5f && hull::localProne() != 1)
            {
                Cbuf_InsertText("+forward\n");
                next(4);
            }
            break;
        case 4: // running up to speed (the stance turn on a long gun)
        {
            const float speed = glm::length(glm::vec2{pl->v.velocity[0], pl->v.velocity[1]});
            if(speed > 300.f && (static_cast<int>(pl->v.flags) & FL_ONGROUND) && in > 0.5)
            {
                test.stanceBefore = measure().yawOff;
                test.startsBefore = slidesound::starts();
                Cbuf_InsertText("+flatcrouch\n");
                next(5);
            }
            break;
        }
        case 5: // sliding: every frame of the full pose
            if(!flatmove::sliding() && in > 0.3)
            {
                check(test.slideFrames >= 20, "slide: %d frames in the full pose", test.slideFrames);
                check(test.yawMax < 2.f, "slide: no two-handed stance turn (body yaw off the view at most %.1f deg; %.1f running)",
                    test.yawMax, test.stanceBefore);
                check(test.bendMax < 16.f, "slide: the legs straighter (knee bend at most %.0f deg, under 16; 35 before; frame %d of %d, pelvis %.1f)",
                    test.bendMax, test.bendAt, test.slideFrames, test.bendPelvis);
                const float k = probe.body.m2w;
                check(test.footMax < 0.16f * k, "slide: the feet on the floor (at most %.1f units over it)", test.footMax);
                check(test.pelvisMax - test.pelvisMin < 1.f, "slide: no bob (the pelvis over the floor %.2f .. %.2f)", test.pelvisMin,
                    test.pelvisMax);
                check(test.eyeMax - test.eyeMin < 1.f, "slide: no view bob (the eye over the origin %.2f .. %.2f)", test.eyeMin,
                    test.eyeMax);
                check(!test.frontSeen, "slide: nothing worn in front of the camera (%s)", test.frontSeen ? test.frontWhat : "none");
                check(slidesound::starts() == test.startsBefore + 1 && slidesound::peakLevel() > 0.5f,
                    "slide: its sound started once (%d) and scraped up to %.2f of full", slidesound::starts() - test.startsBefore,
                    slidesound::peakLevel());
                check(test.levelLast < test.levelFirst - 0.1f, "slide: the scrape faded as it slowed (%.2f at the start of the pose, %.2f at its end)",
                    test.levelFirst, test.levelLast);
                Cbuf_InsertText("-flatcrouch\n-forward\n");
                next(6);
                break;
            }
            if(bodyPitch.sliding > 0.995f && flatmove::sliding())
            {
                const Measure m = measure();
                if(test.slideFrames == 10)
                {
                    printMeasure("body_lie_test slide", m);
                    (void)itemsOnBody(m, "slide");
                    // (His note 2026-11-08: the whole body floated, the holsters and the pouch high in front of the view.)
                    const float hip = vr_flat_slide_hip_floor.value;
                    check(za::abs(m.pelvis - hip) < 1.f || bodyPitch.angle <= -vr_flat_slide_body_angle.value + 0.5f,
                        "slide: the buttocks on the floor (pelvis %.1f over it, %.1f wanted; lean back %.1f deg)", m.pelvis, hip,
                        -bodyPitch.angle);
                    const float holsters = za::max(m.itemZ[body::LeftHip], m.itemZ[body::RightHip]);
                    check(holsters < m.pelvis + 1.f, "slide: the hips' holsters down by the legs (%.1f %.1f over the floor, the pelvis %.1f)",
                        m.itemZ[body::LeftHip], m.itemZ[body::RightHip], m.pelvis);
                    const float pouch = m.itemZ[body::HolsterCount + 1];
                    check(pouch < m.spine + 1.f, "slide: the ammo pouch at the belt (%.1f over the floor; the pelvis %.1f, spine %.1f)", pouch,
                        m.pelvis, m.spine);
                }
                test.slideFrames++;
                test.pelvisMin = za::min(test.pelvisMin, m.pelvis);
                test.pelvisMax = za::max(test.pelvisMax, m.pelvis);
                const float eye = r_refdef.vieworg[2] - cl_entities[cl.viewentity].origin[2];
                test.eyeMin = za::min(test.eyeMin, eye);
                test.eyeMax = za::max(test.eyeMax, eye);
                test.yawMax = za::max(test.yawMax, za::abs(m.yawOff));
                if(test.slideFrames >= 5 && za::max(m.bend[0], m.bend[1]) > test.bendMax) // (past the ease in)
                {
                    test.bendMax = za::max(m.bend[0], m.bend[1]);
                    test.bendAt = test.slideFrames;
                    test.bendPelvis = m.pelvis;
                }
                test.footMax = za::max(test.footMax, za::max(m.foot[0], m.foot[1]));
                test.levelFirst = test.levelFirst < 0.f ? slidesound::level() : test.levelFirst;
                test.levelLast = slidesound::level();
                for(int i = 0; i < ITEMS; i++)
                {
                    if(m.itemCone[i] < FRONT_CONE && m.itemNear[i] < FRONT_NEAR && !test.frontSeen)
                    {
                        test.frontSeen = true;
                        q_snprintf(test.frontWhat, sizeof(test.frontWhat), "%s, %.0f deg, %.0f units", itemNames[i], m.itemCone[i],
                            m.itemNear[i]);
                    }
                }
            }
            break;
        case 6: // the scrape dies away once the slide is over
            if(slidesound::level() < 0.01f)
            {
                check(in < 0.5, "slide: the scrape stopped %.2f s after the slide ended", in);
                cl.viewangles[YAW] += 180.f; // (back the way he came: open floor)
                Cbuf_InsertText("+forward\n");
                next(7);
            }
            break;
        case 7: // a second slide: up to speed again (past the cooldown)
        {
            const float speed = glm::length(glm::vec2{pl->v.velocity[0], pl->v.velocity[1]});
            if(speed > 300.f && (static_cast<int>(pl->v.flags) & FL_ONGROUND) && in > 0.8)
            {
                Cbuf_InsertText("+flatcrouch\n");
                next(8);
            }
            break;
        }
        case 8: // sliding: the keys let go (it runs straight on) and the view turned 40 degrees left off its way
            if(flatmove::sliding() && in > 0.1)
            {
                Cbuf_InsertText("-forward\n");
                cl.viewangles[YAW] += 40.f;
                next(9);
            }
            break;
        case 9: // the hips and legs the slide's way, the chest the view's
            if(!flatmove::sliding())
            {
                check(test.turnFrames >= 10, "slide, the view turned: %d frames measured", test.turnFrames);
                check(test.legsOffMax < 5.f, "slide, the view turned 40: the legs point the slide's way (at most %.1f deg off it)",
                    test.legsOffMax);
                check(test.chestOffMax < 8.f, "slide, the view turned 40: the chest turns back to the view (at most %.1f deg off it)",
                    test.chestOffMax);
                // (His note 2026-11-08: turning, the body's rotation stepped, not smoothed.)
                const float smooth = vr_flat_slide_turn_smooth.value;
                check(test.turn2Frames >= 10 && test.hipWent > 20.f && (smooth <= 0.f || test.hipStepMax < 6.f),
                    "slide, the view turned 60 more: the hips follow, eased (they went %.1f deg in %d frames, at most %.1f in one; "
                    "under 6 eased over %.2f s)", test.hipWent, test.turn2Frames, test.hipStepMax, smooth);
                Cbuf_InsertText("-flatcrouch\n");
                next(99);
                break;
            }
            if(bodyPitch.sliding > 0.995f && in > 0.3)
            {
                const Body& b = probe.body;
                const float hipYaw = geom::yawOf(b.fwd);
                if(test.turned2)
                {
                    // The hips dragged round past the twist's most, a step a frame at most so big.
                    test.turn2Frames++;
                    test.hipStepMax = za::max(test.hipStepMax, za::abs(geom::yawDelta(hipYaw, test.hipYawLast)));
                    test.hipWent = za::abs(geom::yawDelta(hipYaw, test.hipYawFrom));
                    test.hipYawLast = hipYaw;
                    break;
                }
                const glm::vec2 vel{cl.velocity[0], cl.velocity[1]};
                const float legsOff = geom::yawDelta(hipYaw, geom::yawOf(glm::vec3{vel, 0.f}));
                const float chestOff = geom::yawDelta(geom::yawOf(b.bones[Chest].rot[2]), cl.viewangles[YAW]);
                if(test.turnFrames == 5)
                {
                    Con_Printf("body_lie_test: turned: legs %.1f off the slide, chest %.1f off the view\n", legsOff, chestOff);
                }
                test.turnFrames++;
                test.legsOffMax = za::max(test.legsOffMax, za::abs(legsOff));
                test.chestOffMax = za::max(test.chestOffMax, za::abs(chestOff));
                if(test.turnFrames == 15)
                {
                    test.turned2 = true;
                    test.hipYawFrom = test.hipYawLast = hipYaw;
                    cl.viewangles[YAW] += 60.f; // (100 off the slide's way: 30 past vr_flat_slide_legs_twist's 70)
                }
            }
            break;
        default:
            test.tally.verdict("body_lie_test");
            test = LieTest{};
            probe.armed = false;
            break;
    }
}

void report_f()
{
    if(probe.frames == 0)
    {
        probe.armed = true;
        Con_Printf("vr_body_lie_report: armed, run it again next frame\n");
        return;
    }
    printMeasure("body_lie", measure());
    Con_Printf("body_lie: lying %.2f sliding %.2f angle %.1f prone %d sliding %d\n", bodyPitch.lying, bodyPitch.sliding,
        bodyPitch.angle, hull::localProne(), flatmove::sliding() ? 1 : 0);
}

// vr_body_lie_test [vr]: flat-screen play's lying down and slides; "vr": the headset's lying down (the mock's head).
void test_f()
{
    test = LieTest{};
    test.vr = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "vr");
    probe.armed = true;
    probe.frames = 0;
    next(1);
}

} // namespace

void captureLieProbe(const Body& b, const hands::State& s)
{
    if(!probe.armed)
    {
        return;
    }
    probe.body = b;
    probe.eye = s.head;
    probe.frames++;
    testFrame();
}

void initLie()
{
    Cmd_AddCommand("vr_body_lie_report", report_f);
    Cmd_AddCommand("vr_body_lie_test", test_f);
}

} // namespace qvr::avatar::detail
