// vr_bloodprints.cpp -- see vr_bloodprints.hpp.

#include "vr_bloodprints.hpp"
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
// A foot is lifted above this (metres over the lower foot) and down again under footDown: a step.
constexpr float footUp = 0.025f;
constexpr float footDown = 0.008f;
constexpr float bodyReach = 22.f;      // units: a bloody body's origin this near a foot (level), and
constexpr float bodyHeight = 32.f;     // ... this near up or down, is stepped on
constexpr float fallbackStride = 0.7f; // metres walked a step without the body's legs
constexpr float fallbackApart = 0.1f;  // metres each foot is from the middle without them
// Hands: the palm's line from this far behind its middle (the back's way) to this far in front of it, metres; the
// surface must face the palm this much; a print at most every printGap s a hand, a touch slid this far another.
constexpr float palmBehind = 0.08f;
constexpr float palmReach = 0.06f;
constexpr float palmFacing = 0.6f;
constexpr double printGap = 0.3;
constexpr float slideAgain = 0.12f;
constexpr float handLeast = 0.02f; // blood a hand needs left for a print (as a share of one print's)

struct Foot
{
    bool lifted = false;
    glm::vec3 last{0.f}; // where its last print (or step) was
};

struct Hand
{
    float blood = 0.f; // 0..1: soaked full
    bool touching = false;
    glm::vec3 at{0.f}; // where its last print was
    double printed = -1.0;
};

struct State
{
    Foot feet[2]{};
    int feetLeft = 0;      // prints still to leave
    int feetFrom = 0;      // of how many (the fade's)
    float walked = 0.f;    // without the legs: units walked since the last step
    int nextSide = 0;
    glm::vec3 lastOrigin{0.f};
    bool hasOrigin = false;
    glm::vec3 lastSeen{0.f}; // your origin last frame (a teleport: settle)
    bool hasLast = false;
    int settle = 0;          // frames left without steps
    Hand hands[2]{};
    int footprints = 0, handprints = 0, refills = 0; // since the map began (vr_bloodprints_info)
    int frames = 0;                                  // frames the feet and hands were looked at
    double seconds = 0.0, worst = 0.0;               // their time in all, the worst frame's (vr_bloodprints_info)
    double searchWorst = 0.0, placeWorst = 0.0;      // the worst bloody body search, footprint laid
};
State st;

[[nodiscard]] float metres()
{
    return units::metresToUnits() * units::bodyScale();
}

[[nodiscard]] bool liquidAt(const glm::vec3& p)
{
    if(!cl.worldmodel)
    {
        return false;
    }
    vec3_t v{p.x, p.y, p.z};
    const int c = Mod_PointInLeaf(v, cl.worldmodel)->contents;
    return c <= CONTENTS_WATER && c >= CONTENTS_LAVA;
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
        st.feet[0].lifted = st.feet[1].lifted = false;
        st.hasOrigin = false;
        return;
    }
    avatar::Skeleton k;
    if(avatar::skeleton(k) && k.legs)
    {
        st.hasOrigin = false;
        const float low = za::min(k.ankle[0].z, k.ankle[1].z);
        for(int side = 0; side < 2; side++)
        {
            const float h = k.ankle[side].z - low;
            Foot& ft = st.feet[side];
            if(h > footUp * m2w)
            {
                ft.lifted = true;
            }
            else if(ft.lifted && h < footDown * m2w)
            {
                ft.lifted = false;
                if(cl.onground)
                {
                    footDown_(side, k.ankle[side], k.toe[side], k.sole[side]);
                }
            }
        }
        return;
    }
    // Without the legs: a step every stride walked on the ground, along the way you go.
    if(!st.hasOrigin)
    {
        st.lastOrigin = origin;
        st.hasOrigin = true;
        return;
    }
    const glm::vec2 d{origin.x - st.lastOrigin.x, origin.y - st.lastOrigin.y};
    st.lastOrigin = origin;
    const float moved = glm::length(d);
    if(moved > 64.f || !cl.onground)
    {
        st.walked = moved > 64.f ? 0.f : st.walked;
        return;
    }
    st.walked += moved;
    if(st.walked < fallbackStride * m2w || moved < 1e-3f)
    {
        return;
    }
    st.walked = 0.f;
    const glm::vec3 fwd{d.x / moved, d.y / moved, 0.f};
    const glm::vec3 left{-fwd.y, fwd.x, 0.f};
    const int side = st.nextSide;
    st.nextSide ^= 1;
    const float sole = 0.07f * m2w;
    const glm::vec3 ankle = origin + glm::vec3{0.f, 0.f, -24.f + sole} + left * ((side == 0 ? 1.f : -1.f) * fallbackApart * m2w);
    footDown_(side, ankle, ankle + fwd * (0.19f * m2w), sole);
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
    glm::vec3 where, normal;
    float f;
    const bool touch = decals::trace(mid + hp.back * (palmBehind * m2w), mid - hp.back * (palmReach * m2w), where, normal, f) &&
                       glm::dot(normal, hp.back) > palmFacing;
    if(vr_bloodprints_debug.value >= 2.f && (st.frames % 30) == 0)
    {
        Con_Printf("bloodprints: hand %d palm %.1f %.1f %.1f, its back's way %.2f %.2f %.2f: %s\n", hand, mid.x, mid.y, mid.z,
            hp.back.x, hp.back.y, hp.back.z, touch ? "touching" : "free");
    }
    const bool fresh = touch && (!h.touching || glm::distance(where, h.at) > slideAgain * m2w);
    h.touching = touch;
    if(!fresh || (h.printed >= 0.0 && cl.time - h.printed < printGap && cl.time >= h.printed))
    {
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
    const float size = za::clamp(vr_handprint_size.value, 0.5f, 2.f);
    const float len = handLength * m2w * size, width = handWidth * m2w * size;
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
    else
    {
        Con_Printf("usage: vr_bloodprints_test feet [n] | hands [amount 0..1] | pool [size]\n");
    }
}

// vr_bloodprints_info: the feet's prints left, the hands' blood, the prints laid since the map began.
void info_f()
{
    Con_Printf("bloodprints: feet %d of %d left, hands' blood %.2f %.2f; %d footprints, %d handprints laid, feet bloodied "
               "%d times\n",
        st.feetLeft, st.feetFrom, st.hands[0].blood, st.hands[1].blood, st.footprints, st.handprints, st.refills);
    avatar::Skeleton k;
    const bool posed = avatar::skeleton(k);
    const entity_t& pl = cl_entities[cl.viewentity];
    Con_Printf("bloodprints: you at %.1f %.1f %.1f (%s), the body %s; ankles %.1f %.1f %.1f, %.1f %.1f %.1f; frames %d, "
               "%.2f us a frame (worst %.1f us; the worst body search %.1f us, footprint laid %.1f us)\n",
        pl.origin[0], pl.origin[1], pl.origin[2], cl.onground ? "on the ground" : "in the air",
        !posed ? "not posed" : k.legs ? "posed with its legs" : "posed without legs", k.ankle[0].x, k.ankle[0].y,
        k.ankle[0].z, k.ankle[1].x, k.ankle[1].y, k.ankle[1].z, st.frames,
        st.frames > 0 ? st.seconds / st.frames * 1e6 : 0.0, st.worst * 1e6, st.searchWorst * 1e6, st.placeWorst * 1e6);
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
        st.feet[0].lifted = st.feet[1].lifted = false;
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
