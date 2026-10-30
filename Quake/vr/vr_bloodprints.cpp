// vr_bloodprints.cpp -- see vr_bloodprints.hpp.

#include "vr_bloodprints.hpp"
#include "vr_trace.hpp"
#include "vr_cvars.hpp"
#include "vr_decals.hpp"
#include "vr_engine.hpp"
#include "vr_goresound.hpp"
#include "vr_profile.hpp"
#include "vr_ragdoll.hpp"
#include "vr_units.hpp"
#include "vr_wounds.hpp"

#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_zancle.hpp"

#include <stdlib.h>
#include <string.h>

namespace qvr::bloodprints
{
namespace
{

// Sizes in metres of the body (times units::bodyScale()): a boot's sole as long as the foot's ankle-to-toe over
// ankleShare, this wide for its length; a hand print from the wrist to the fingertips, and across with the thumb out.
constexpr float ankleShare = 0.75f;    // the ankle to the toe: this much of the sole (the heel the rest)
constexpr float bootWidth = 0.38f;     // the sole's width over its length
constexpr float handLength = 0.19f;
constexpr float handWidth = 0.11f;
constexpr float bodyReach = 22.f;      // units: a bloody body's origin this near a foot (level), and
constexpr float bodyHeight = 32.f;     // ... this near up or down, is stepped on
// A step's length, metres of the body, at a speed (metres a second of the body): a person's (avatar::personStep), from a
// walk's 0.7 m at 1.4 m/s to a sprint's 2.2 m (Quake's run, 320 units/s, is a sprint's 8-12 m/s), over
// vr_footprint_rate. The walk's legs step at that cadence at speed only as far as vr_body_step_run says (and walking
// slowly, up to vr_body_step_rate), so the prints don't wait for their touchdowns while you move.
constexpr double airGrace = 0.25;      // s off the ground still walking (a step down, the ground flickering off)
constexpr float standSpeed = 0.3f;    // metres a second of the body: slower, the legs' own steps (turning on the spot)
constexpr float fallbackApart = 0.1f;  // metres each foot is from the middle without the legs
constexpr float apartLeast = 0.05f;    // ... with them: as posed, within these
constexpr float apartMost = 0.2f;
constexpr float fallbackFoot = 0.19f;  // metres from the ankle to the toe without them
// Hands: the palm's line from this far behind its middle (the back's way) to this far in front of it, metres; the
// surface must face the palm this much; a print at most every printGap s a hand, a touch slid this far another.
constexpr float palmBehind = 0.08f;
constexpr float palmReach = 0.06f;
constexpr float palmFacing = 0.6f;
constexpr double printGap = 0.3;
constexpr float slideAgain = 0.12f;
constexpr float handLeast = 0.02f; // blood a hand needs left for a print (as a share of one print's)
// What a bloody hand holds: its blood put where it grips it every gripGap s, a gripUse share of a print's blood each.
constexpr double gripGap = 0.75;
constexpr float gripUse = 0.25f;
// A handprint over an edge: the face beyond it found this far past (units), and facing at least this much along the way
// it goes; the part over it at least this share of the print's half-extent.
constexpr float edgeProbe = 1.5f;
constexpr float edgeFacing = 0.5f;
constexpr float edgeLeast = 0.15f;

struct Foot
{
    glm::vec3 last{0.f}; // where its last print (or step) was
};

struct Hand
{
    float blood = 0.f; // 0..1: soaked full
    bool touching = false;
    glm::vec3 at{0.f}; // where its last print was
    double printed = -1.0;
    double gripped = -1.0; // when its blood was last put where it grips what it holds
};

struct State
{
    Foot feet[2]{};
    int feetLeft = 0;      // prints still to leave
    int feetFrom = 0;      // of how many (the fade's)
    float walked = 0.f;    // units walked since the last step
    int nextSide = 0;
    double grounded = -1.0;       // when you were last on the ground
    unsigned legSteps[2]{0u, 0u}; // the posed legs' touchdowns at the last look (avatar::Skeleton::steps)
    bool hasLegSteps = false;
    // vr_bloodprints_info's step cadence (since the map began): steps taken moving, the legs' touchdowns meanwhile, the
    // units walked and the seconds it took.
    int steps = 0, legTouchdowns = 0;
    double walkedUnits = 0.0, walkedSeconds = 0.0;
    glm::vec3 lastOrigin{0.f};
    bool hasOrigin = false;
    glm::vec3 lastSeen{0.f}; // your origin last frame (a teleport: settle)
    bool hasLast = false;
    int settle = 0;          // frames left without steps
    Hand hands[2]{};
    int footprints = 0, handprints = 0, refills = 0; // since the map began (vr_bloodprints_info)
    int wraps = 0, propPrints = 0, grips = 0;        // ... handprints' parts round an edge, on props, grips painted
    int frames = 0;                                  // frames the feet and hands were looked at
    double seconds = 0.0, worst = 0.0;               // their time in all, the worst frame's (vr_bloodprints_info)
    double searchWorst = 0.0, placeWorst = 0.0;      // the worst bloody body search, footprint laid
};
State st;

[[nodiscard]] float metres()
{
    return units::metresToUnits() * units::bodyScale();
}

// A step's length in metres of the body at `speed` (metres a second of the body), before vr_footprint_rate.
[[nodiscard]] float stepLength(float speed)
{
    return avatar::personStep(speed);
}

[[nodiscard]] bool liquidAt(const glm::vec3& p)
{
    return worldtrace::isLiquid(worldtrace::clientContents(p));
}

[[nodiscard]] bool debug()
{
    return vr_bloodprints_debug.value != 0.f;
}

// A bloody body (a ragdoll or a corpse with blood painted on it) under `at`.
[[nodiscard]] int bloodyBodyAt(const glm::vec3& at)
{
    for(int i = 1; i < cl.num_entities; i++)
    {
        const entity_t& e = cl_entities[i];
        if(i == cl.viewentity || !e.model || e.model->type != mod_alias || e.msgtime < cl.mtime[1] - 0.2)
        {
            continue;
        }
        const glm::vec2 d{e.origin[0] - at.x, e.origin[1] - at.y};
        if(glm::dot(d, d) > bodyReach * bodyReach || za::fabs(e.origin[2] - at.z) > bodyHeight)
        {
            continue;
        }
        const float* m = nullptr;
        if(ragdoll::bonePoses(&e, &m) > 0 && wounds::bloody(&e)) // (a ragdoll: a body lying there)
        {
            return i;
        }
    }
    return 0;
}

// A foot came down at `ankle` (its toe at `toe`, the ankle `sole` over the sole), `side` 0 the left: blood taken up
// there, or a print left.
void footDown_(int side, const glm::vec3& ankle, const glm::vec3& toe, float sole)
{
    const float m2w = metres();
    glm::vec3 where, normal;
    float f;
    if(!decals::trace(ankle + glm::vec3{0.f, 0.f, 4.f}, ankle - glm::vec3{0.f, 0.f, sole + 16.f}, where, normal, f) ||
        normal.z < 0.7f)
    {
        return;
    }
    glm::vec3 fwd = toe - ankle;
    fwd -= normal * glm::dot(fwd, normal);
    const float reach = glm::length(fwd);
    if(reach < 0.02f * m2w)
    {
        return;
    }
    fwd /= reach;
    const float length = reach / ankleShare;
    const glm::vec3 centre = where + fwd * (length * (ankleShare - 0.5f)); // (the sole's middle: ahead of the ankle)
    Foot& ft = st.feet[side];
    ft.last = centre;

    // In a pool, or on a bloody body: soaked again (no print in the blood itself).
    const bool pool = goresound::poolUnder(centre, 2.f);
    const double t0 = Sys_DoubleTime();
    const int body = pool ? 0 : bloodyBodyAt(centre);
    st.searchWorst = za::max(st.searchWorst, Sys_DoubleTime() - t0);
    if(pool || body)
    {
        st.feetFrom = st.feetLeft = za::clamp(static_cast<int>(vr_footprint_count.value), 1, 32);
        st.refills++;
        if(debug())
        {
            Con_Printf("bloodprints: feet bloodied (%s %d) at %.1f %.1f %.1f\n", pool ? "pool" : "body", body, centre.x,
                centre.y, centre.z);
        }
        return;
    }
    if(st.feetLeft <= 0)
    {
        return;
    }
    const int n = st.feetFrom - st.feetLeft; // (0 the first)
    const float strength = 1.f - za::clamp(vr_footprint_fade.value, 0.f, 1.f) * static_cast<float>(n) /
                                     static_cast<float>(za::max(st.feetFrom - 1, 1));
    st.feetLeft--;
    const float size = za::clamp(vr_footprint_size.value, 0.5f, 2.f);
    const float len = length * size, width = len * bootWidth;
    decals::MarkOptions o;
    o.along = fwd;
    o.aspect = len / width;
    o.variant = strength > 0.7f ? 0 : strength > 0.4f ? 1 : 2;
    o.mirror = side == 0; // (the cell's a right boot)
    const double t1 = Sys_DoubleTime();
    const bool placed = decals::place(decals::Mark::Footprint, centre, normal, width, o);
    st.placeWorst = za::max(st.placeWorst, Sys_DoubleTime() - t1);
    st.footprints += placed ? 1 : 0;
    if(debug())
    {
        Con_Printf("bloodprints: footprint %s side %d at %.1f %.1f %.1f yaw %.1f, %.1f x %.1f, strength %.2f (cell %d), %d "
                   "left; ankle %.1f %.1f %.1f toe %.1f %.1f %.1f\n",
            placed ? "laid" : "NOT laid", side, centre.x, centre.y, centre.z,
            glm::degrees(za::atan2(fwd.y, fwd.x)), len, width, strength, o.variant, st.feetLeft, ankle.x, ankle.y, ankle.z,
            toe.x, toe.y, toe.z);
    }
}

// The feet: each comes down after it was lifted (the body's legs), or every stride without them.
void feetFrame()
{
    const entity_t& pl = cl_entities[cl.viewentity];
    const glm::vec3 origin{pl.origin[0], pl.origin[1], pl.origin[2]};
    if(liquidAt(origin - glm::vec3{0.f, 0.f, 20.f}))
    {
        st.feetLeft = 0; // (washed)
    }
    const float m2w = metres();
    // A teleport (a respawn, setpos): the feet put down there are no steps (a moment to settle).
    if(st.hasLast && glm::distance(origin, st.lastSeen) > 48.f)
    {
        st.settle = 10;
    }
    st.lastSeen = origin;
    st.hasLast = true;
    if(st.settle > 0)
    {
        st.settle--;
        st.hasLegSteps = false;
        st.hasOrigin = false;
        return;
    }
    const glm::vec2 vel{cl.velocity[0], cl.velocity[1]};
    const float speed = glm::length(vel) / m2w; // metres a second of the body
    const bool moving = speed >= standSpeed;
    avatar::Skeleton k;
    const bool legs = avatar::skeleton(k) && k.legs;
    // Standing (turning on the spot, squaring up): the legs' own steps, each where its foot came down.
    if(legs)
    {
        for(int side = 0; side < 2; side++)
        {
            if(st.hasLegSteps && k.steps[side] != st.legSteps[side])
            {
                if(moving)
                {
                    st.legTouchdowns++;
                }
                else if(cl.onground)
                {
                    footDown_(side, k.ankle[side], k.toe[side], k.sole[side]);
                }
            }
            st.legSteps[side] = k.steps[side];
        }
        st.hasLegSteps = true;
    }
    else
    {
        st.hasLegSteps = false;
    }
    // Moving: a step every step's length walked on the ground (stepLength), the feet in turn, each beside the way you go
    // as the legs stand apart, along its foot.
    if(!st.hasOrigin)
    {
        st.lastOrigin = origin;
        st.hasOrigin = true;
        return;
    }
    const glm::vec2 d{origin.x - st.lastOrigin.x, origin.y - st.lastOrigin.y};
    st.lastOrigin = origin;
    const float moved = glm::length(d);
    // On the ground, or off it only a moment (a step down, the ground flickering off over a seam): still walking.
    if(cl.onground || cl.time < st.grounded)
    {
        st.grounded = cl.time;
    }
    const bool walking = cl.time - st.grounded < airGrace;
    if(moved > 64.f || !walking || !moving)
    {
        st.walked = moved > 64.f || !moving ? 0.f : st.walked;
        return;
    }
    st.walked += moved;
    st.walkedUnits += moved;
    st.walkedSeconds += cl.time - cl.oldtime;
    const float rate = za::clamp(vr_footprint_rate.value, 0.25f, 4.f);
    if(st.walked < stepLength(speed) * m2w / rate || moved < 1e-3f)
    {
        return;
    }
    st.walked = 0.f;
    st.steps++;
    const int side = st.nextSide;
    st.nextSide ^= 1;
    const glm::vec3 way{d.x / moved, d.y / moved, 0.f};
    glm::vec3 left{-way.y, way.x, 0.f};
    glm::vec3 fwd = way;
    float apart = fallbackApart * m2w, foot = fallbackFoot * m2w, sole = 0.07f * m2w;
    if(legs)
    {
        const glm::vec3 hips{k.hip[0].x - k.hip[1].x, k.hip[0].y - k.hip[1].y, 0.f};
        if(glm::length(hips) > 1e-3f)
        {
            left = glm::normalize(hips); // (the body's left: across its hips, whichever way it goes)
            const glm::vec3 mid = (k.hip[0] + k.hip[1]) * 0.5f;
            apart = za::clamp(za::fabs(glm::dot(k.ankle[side] - mid, left)), apartLeast * m2w, apartMost * m2w);
        }
        const glm::vec3 toe = k.toe[side] - k.ankle[side];
        if(const glm::vec3 flat{toe.x, toe.y, 0.f}; glm::length(flat) > 0.02f * m2w)
        {
            fwd = glm::normalize(flat);
            foot = glm::length(toe);
        }
        sole = k.sole[side];
    }
    const glm::vec3 ankle = origin + glm::vec3{0.f, 0.f, -24.f + sole} + left * ((side == 0 ? 1.f : -1.f) * apart);
    footDown_(side, ankle, ankle + fwd * foot, sole);
}

// A handprint laid at `centre` on a face (`normal`), `along` its length, `halfLen` by `halfWidth` (`width` across, as
// place takes it, with `o`): its part over an edge (a ledge's top beyond the wall a climbing hand is on, the floor at a
// wall's foot) laid on the face beyond as well, the print unfolded onto it (turned about the edge, its two parts meeting
// there: each cut where its face ends). How many such parts were laid (0..2).
int wrapPrint(const glm::vec3& centre, const glm::vec3& normal, const glm::vec3& along, float halfLen, float halfWidth,
    float width, const decals::MarkOptions& o)
{
    const glm::vec3 across = glm::cross(normal, along);
    const glm::vec3 ways[4]{along, -along, across, -across};
    const float extents[4]{halfLen, halfLen, halfWidth, halfWidth};
    int laid = 0;
    for(int i = 0; i < 4 && laid < 2; i++)
    {
        const glm::vec3& d = ways[i];
        const float e = extents[i];
        glm::vec3 hit, beyond;
        float f;
        // A face rising across the way (a wall's foot, an inside corner): met along the print, just over it.
        const glm::vec3 a = centre + normal * edgeProbe;
        if(decals::trace(a, a + d * e, hit, beyond, f))
        {
            if(glm::dot(beyond, -d) < edgeFacing)
            {
                continue;
            }
        }
        else
        {
            // The face ending under the print (a ledge's edge): the face beyond it, met from over it, back the way.
            const glm::vec3 q = centre + d * e;
            glm::vec3 n;
            if(decals::trace(q + normal * edgeProbe, q - normal * (2.f * edgeProbe), hit, n, f) && glm::dot(n, normal) > 0.9f)
            {
                continue; // (the face goes on)
            }
            const glm::vec3 b = q - normal * edgeProbe;
            if(!decals::trace(b, b - d * e, hit, beyond, f) || glm::dot(beyond, d) < edgeFacing)
            {
                continue;
            }
        }
        if(vr_bloodprints_debug.value >= 2.f)
        {
            Con_Printf("bloodprints: edge probe %d: a face beyond at %.1f %.1f %.1f facing %.2f %.2f %.2f\n", i, hit.x, hit.y,
                hit.z, beyond.x, beyond.y, beyond.z);
        }
        const glm::vec3 edge = hit - normal * glm::dot(hit - centre, normal); // (on the print's face, at the edge)
        const float over = e - glm::dot(edge - centre, d);
        const glm::vec3 axis = glm::cross(normal, beyond);
        const float sine = glm::length(axis);
        if(over < edgeLeast * e || sine < 1e-3f)
        {
            continue;
        }
        // Turned about the edge, taking the face's normal onto the face beyond's (Rodrigues).
        const glm::vec3 k = axis / sine;
        const float cosine = glm::dot(normal, beyond);
        const auto turn = [&](const glm::vec3& v) {
            return v * cosine + glm::cross(k, v) * sine + k * (glm::dot(k, v) * (1.f - cosine));
        };
        decals::MarkOptions o2 = o;
        o2.along = turn(o.along);
        const glm::vec3 c2 = edge + turn(centre - edge);
        const bool placed = decals::place(decals::Mark::Handprint, c2, beyond, width, o2);
        laid += placed ? 1 : 0;
        if(debug())
        {
            Con_Printf("bloodprints: handprint %s round an edge %.1f units over, onto %.2f %.2f %.2f at %.1f %.1f %.1f\n",
                placed ? "laid" : "NOT laid", over, beyond.x, beyond.y, beyond.z, c2.x, c2.y, c2.z);
        }
    }
    return laid;
}

// A hand's palm on the world: a print at a new touch (or one slid along), its blood used up.
void handFrame(int hand, const avatar::HandPose& hp)
{
    Hand& h = st.hands[hand];
    const float m2w = metres();
    const glm::vec3 mid = hp.wrist + hp.forward * (0.045f * m2w); // (the palm's middle)
    if(liquidAt(mid))
    {
        h.blood = 0.f; // (washed)
    }
    const int count = za::clamp(static_cast<int>(vr_handprint_count.value), 1, 32);
    const float use = 1.f / static_cast<float>(count);
    if(h.blood < use * handLeast || !vr_handprints.value)
    {
        h.touching = false;
        return;
    }
    const float size = za::clamp(vr_handprint_size.value, 0.5f, 2.f);
    const float len = handLength * m2w * size, width = handWidth * m2w * size;
    // What it holds (its weapon, a prop it carries): blood where it grips it, now and then while it holds it.
    if(vr_handprint_gear.value && (h.gripped < 0.0 || cl.time - h.gripped >= gripGap || cl.time < h.gripped) &&
        wounds::gripPrint(hand, mid, width * 0.5f, za::min(h.blood, 1.f)))
    {
        h.gripped = cl.time;
        h.blood = za::max(0.f, h.blood - use * gripUse);
        st.grips++;
    }
    glm::vec3 where, normal;
    float f;
    const glm::vec3 from = mid + hp.back * (palmBehind * m2w);
    const float reach = (palmBehind + palmReach) * m2w;
    bool touch = decals::trace(from, mid - hp.back * (palmReach * m2w), where, normal, f) && glm::dot(normal, hp.back) > palmFacing;
    // A prop lying about, nearer than the world: its own blood painted on it.
    glm::vec3 propAt, propNormal;
    const int prop = wounds::palmTouch(from, -hp.back, touch ? f * reach : reach, propAt, propNormal);
    if(prop && glm::dot(propNormal, hp.back) > palmFacing)
    {
        touch = true;
        where = propAt;
        normal = propNormal;
    }
    else if(prop)
    {
        touch = false; // (a prop in the way, not facing the palm)
    }
    if(vr_bloodprints_debug.value >= 2.f && (st.frames % 30) == 0)
    {
        Con_Printf("bloodprints: hand %d palm %.1f %.1f %.1f, its back's way %.2f %.2f %.2f: %s%s\n", hand, mid.x, mid.y, mid.z,
            hp.back.x, hp.back.y, hp.back.z, touch ? "touching" : "free", prop ? " a prop" : "");
    }
    const bool fresh = touch && (!h.touching || glm::distance(where, h.at) > slideAgain * m2w);
    h.touching = touch;
    if(!fresh || (h.printed >= 0.0 && cl.time - h.printed < printGap && cl.time >= h.printed))
    {
        return;
    }
    if(prop)
    {
        const float strength = za::min(h.blood, 1.f);
        const bool painted = wounds::handprintOn(prop, where, len * 0.5f, strength);
        h.at = where;
        h.printed = cl.time;
        if(painted)
        {
            h.blood = za::max(0.f, h.blood - use);
            st.handprints++;
            st.propPrints++;
        }
        return;
    }
    // Along the hand on the surface; the thumb (its index finger's side: hp.up) on the cell's +y or mirrored.
    glm::vec3 along = hp.forward - normal * glm::dot(hp.forward, normal);
    if(glm::length(along) < 0.2f)
    {
        along = hp.up - normal * glm::dot(hp.up, normal); // (fingers into the wall: along the hand's side)
    }
    if(glm::length(along) < 1e-3f)
    {
        return;
    }
    along = glm::normalize(along);
    glm::vec3 centre = hp.wrist + along * (len * 0.5f);
    centre -= normal * glm::dot(centre - where, normal);
    const float strength = za::min(h.blood, 1.f);
    decals::MarkOptions o;
    o.along = along;
    o.aspect = len / width;
    o.variant = strength > 0.5f ? 0 : 1;
    o.mirror = glm::dot(hp.up, glm::cross(normal, along)) < 0.f;
    const bool placed = decals::place(decals::Mark::Handprint, centre, normal, width, o);
    h.at = where;
    h.printed = cl.time;
    if(placed)
    {
        h.blood = za::max(0.f, h.blood - use);
        st.handprints++;
        st.wraps += wrapPrint(centre, normal, along, len * 0.5f, width * 0.5f, width, o);
    }
    if(debug())
    {
        Con_Printf("bloodprints: handprint %s hand %d at %.1f %.1f %.1f normal %.2f %.2f %.2f along %.2f %.2f %.2f%s, %.1f x "
                   "%.1f, strength %.2f, blood left %.2f; wrist %.1f %.1f %.1f\n",
            placed ? "laid" : "NOT laid", hand, centre.x, centre.y, centre.z, normal.x, normal.y, normal.z, along.x, along.y,
            along.z, o.mirror ? " mirrored" : "", len, width, strength, h.blood, hp.wrist.x, hp.wrist.y, hp.wrist.z);
    }
}

// vr_bloodprints_test feet [n] | hands [amount] | pool [size]: your feet bloodied for `n` prints (vr_footprint_count),
// both hands soaked (1: full), a pool of blood laid under you (a size in units: 48).
void test_f()
{
    const char* what = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!strcmp(what, "feet"))
    {
        st.feetFrom = st.feetLeft =
            za::clamp(Cmd_Argc() > 2 ? atoi(Cmd_Argv(2)) : static_cast<int>(vr_footprint_count.value), 1, 32);
        Con_Printf("bloodprints: feet bloodied for %d prints\n", st.feetLeft);
    }
    else if(!strcmp(what, "hands"))
    {
        const float a = Cmd_Argc() > 2 ? static_cast<float>(atof(Cmd_Argv(2))) : 1.f;
        st.hands[0].blood = st.hands[1].blood = za::clamp(a, 0.f, 1.f);
        Con_Printf("bloodprints: both hands' blood %.2f\n", st.hands[0].blood);
    }
    else if(!strcmp(what, "pool"))
    {
        const float size = Cmd_Argc() > 2 ? za::clamp(static_cast<float>(atof(Cmd_Argv(2))), 8.f, 256.f) : 48.f;
        const entity_t& pl = cl_entities[cl.viewentity];
        const glm::vec3 o{pl.origin[0], pl.origin[1], pl.origin[2]};
        glm::vec3 where, normal;
        float f;
        const bool laid = decals::trace(o, o - glm::vec3{0.f, 0.f, 64.f}, where, normal, f) &&
                          decals::place(decals::Mark::Pool, where, normal, size);
        Con_Printf("bloodprints: pool %s at %.1f %.1f %.1f\n", laid ? "laid" : "NOT laid", where.x, where.y, where.z);
    }
    else if(!strcmp(what, "wall") || !strcmp(what, "prop") || !strcmp(what, "floor"))
    {
        // A full handprint pressed ahead of you (the view's yaw, level, from `height` units over your feet): on the wall
        // there, fingers up, wrapping round an edge as a palm's would (wall); or on the prop it meets (prop).
        const float height = Cmd_Argc() > 2 ? static_cast<float>(atof(Cmd_Argv(2))) : 24.f;
        const entity_t& pl = cl_entities[cl.viewentity];
        const float yaw = glm::radians(cl.viewangles[YAW]);
        const glm::vec3 fwd{za::cos(yaw), za::sin(yaw), 0.f};
        const glm::vec3 from = glm::vec3{pl.origin[0], pl.origin[1], pl.origin[2] - 24.f + height};
        const float m2w = metres();
        const float size = za::clamp(vr_handprint_size.value, 0.5f, 2.f);
        const float len = handLength * m2w * size, width = handWidth * m2w * size;
        glm::vec3 where, normal;
        float f;
        if(!strcmp(what, "prop"))
        {
            // Pressed at the middle of the nearest thing ahead (within 128 units), at its middle's height.
            glm::vec3 dir = fwd, at = from;
            float nearest = 128.f;
            for(int i = cl.maxclients + 1; i < cl.num_entities; i++)
            {
                const entity_t& e = cl_entities[i];
                if(!e.model || e.model == cl.worldmodel || (e.model->type != mod_alias && e.model->type != mod_brush) ||
                    e.msgtime < cl.mtime[0] - 0.001)
                {
                    continue;
                }
                const glm::vec3 mid = glm::vec3{e.origin[0], e.origin[1], e.origin[2]} +
                                      (e.model->type == mod_brush ? (glm::vec3{e.model->mins[0], e.model->mins[1], e.model->mins[2]} +
                                                                       glm::vec3{e.model->maxs[0], e.model->maxs[1], e.model->maxs[2]}) * 0.5f
                                                                  : glm::vec3{0.f});
                const glm::vec3 to{mid.x - from.x, mid.y - from.y, 0.f};
                const float d = glm::length(to);
                if(d > 1.f && d < nearest && glm::dot(to / d, fwd) > 0.7f)
                {
                    nearest = d;
                    dir = to / d;
                    at = glm::vec3{from.x, from.y, mid.z};
                }
            }
            const int prop = wounds::palmTouch(at, dir, 128.f, where, normal);
            const bool painted = prop && wounds::handprintOn(prop, where, len * 0.5f, 1.f);
            Con_Printf("bloodprints: prop %d %s at %.1f %.1f %.1f\n", prop, painted ? "printed" : "NOT printed", where.x,
                where.y, where.z);
            return;
        }
        // floor <distance>: on the floor that far ahead instead, fingers ahead (over a step's edge: onto its riser).
        const bool floor = !strcmp(what, "floor");
        const glm::vec3 down = from + fwd * height;
        if(floor ? !decals::trace(glm::vec3{down.x, down.y, pl.origin[2]}, down - glm::vec3{0.f, 0.f, 256.f}, where, normal, f)
                 : !decals::trace(from, from + fwd * 128.f, where, normal, f))
        {
            Con_Printf("bloodprints: no %s ahead\n", floor ? "floor" : "wall");
            return;
        }
        const glm::vec3 up = floor ? fwd : glm::vec3{0.f, 0.f, 1.f};
        const glm::vec3 along = glm::normalize(up - normal * glm::dot(up, normal));
        decals::MarkOptions o;
        o.along = along;
        o.aspect = len / width;
        o.variant = 0;
        const glm::vec3 centre = where;
        const bool placed = decals::place(decals::Mark::Handprint, centre, normal, width, o);
        const int wrapped = placed ? wrapPrint(centre, normal, along, len * 0.5f, width * 0.5f, width, o) : 0;
        Con_Printf("bloodprints: wall print %s at %.1f %.1f %.1f, %d parts round an edge\n", placed ? "laid" : "NOT laid",
            centre.x, centre.y, centre.z, wrapped);
    }
    else
    {
        Con_Printf("usage: vr_bloodprints_test feet [n] | hands [amount 0..1] | pool [size] | wall [height] | prop [height] | "
                   "floor [distance]\n");
    }
}

// vr_bloodprints_info: the feet's prints left, the hands' blood, the prints laid since the map began.
void info_f()
{
    Con_Printf("bloodprints: feet %d of %d left, hands' blood %.2f %.2f; %d footprints, %d handprints laid (%d parts round "
               "an edge, %d on props), %d grips painted, feet bloodied %d times\n",
        st.feetLeft, st.feetFrom, st.hands[0].blood, st.hands[1].blood, st.footprints, st.handprints, st.wraps,
        st.propPrints, st.grips, st.refills);
    avatar::Skeleton k;
    const bool posed = avatar::skeleton(k);
    const entity_t& pl = cl_entities[cl.viewentity];
    Con_Printf("bloodprints: you at %.1f %.1f %.1f (%s), the body %s; ankles %.1f %.1f %.1f, %.1f %.1f %.1f; frames %d, "
               "%.2f us a frame (worst %.1f us; the worst body search %.1f us, footprint laid %.1f us)\n",
        pl.origin[0], pl.origin[1], pl.origin[2], cl.onground ? "on the ground" : "in the air",
        !posed ? "not posed" : k.legs ? "posed with its legs" : "posed without legs", k.ankle[0].x, k.ankle[0].y,
        k.ankle[0].z, k.ankle[1].x, k.ankle[1].y, k.ankle[1].z, st.frames,
        st.frames > 0 ? st.seconds / st.frames * 1e6 : 0.0, st.worst * 1e6, st.searchWorst * 1e6, st.placeWorst * 1e6);
    // The step cadence: the prints' steps moving, against the legs' touchdowns meanwhile.
    const double perSecond = st.walkedSeconds > 0.0 ? 1.0 / st.walkedSeconds : 0.0;
    const float m2w = metres();
    const float speed = glm::length(glm::vec2{cl.velocity[0], cl.velocity[1]});
    Con_Printf("bloodprints: steps %d (%.2f a second, one every %.1f units) over %.0f units in %.2f s (%.0f u/s); the legs' "
               "touchdowns meanwhile %d (%.2f a second); now %.0f u/s: a step every %.1f units\n",
        st.steps, st.steps * perSecond, st.steps > 0 ? st.walkedUnits / st.steps : 0.0, st.walkedUnits, st.walkedSeconds,
        st.walkedUnits * perSecond, st.legTouchdowns, st.legTouchdowns * perSecond, speed,
        stepLength(speed / m2w) * m2w / za::clamp(vr_footprint_rate.value, 0.25f, 4.f));
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "reset"))
    {
        st.steps = st.legTouchdowns = 0;
        st.walkedUnits = st.walkedSeconds = 0.0;
    }
}

} // namespace

void bloodyHand(int hand, float amount)
{
    if(hand >= 0 && hand < 2)
    {
        st.hands[hand].blood = za::min(1.f, st.hands[hand].blood + amount * 0.25f);
    }
}

void frame(const hands::State& s, const avatar::HandPose* const drawnHands[2])
{
    (void)s;
    QVR_PROFILE("blood prints");
    if(cls.state != ca_connected || !cl.worldmodel || cl.viewentity <= 0 || cl.viewentity >= cl.num_entities ||
        cl.stats[STAT_HEALTH] <= 0 || !vr_decals.value)
    {
        st.hasLegSteps = false;
        st.hasOrigin = false;
        st.hasLast = false;
        if(cl.stats[STAT_HEALTH] <= 0)
        {
            st.feetLeft = 0;
            st.hands[0].blood = st.hands[1].blood = 0.f;
        }
        return;
    }
    const double t0 = Sys_DoubleTime();
    st.frames++;
    if(vr_footprints.value)
    {
        feetFrame();
    }
    else
    {
        st.feetLeft = 0;
    }
    for(int hand = 0; hand < 2; hand++)
    {
        if(drawnHands[hand])
        {
            handFrame(hand, *drawnHands[hand]);
        }
    }
    const double took = Sys_DoubleTime() - t0;
    st.seconds += took;
    st.worst = za::max(st.worst, took);
}

void clear()
{
    st = State{};
}

void registerCommands()
{
    Cmd_AddCommand("vr_bloodprints_test", test_f);
    Cmd_AddCommand("vr_bloodprints_info", info_f);
}

} // namespace qvr::bloodprints
