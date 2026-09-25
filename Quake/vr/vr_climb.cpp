// vr_climb.cpp -- ledge grabbing and mantling (vr_climb, experimental; server side).
//
// Not a climbing system: an empty hand can only take hold of a LEDGE -- a walkable top surface
// (normal z > 0.7) just under the hand, with room above it for the hand, and a drop of at least
// `minDrop` units within `edgeReach` of the hand (so an edge, not open floor, a wall or a stair).
// The grip must be pressed at the ledge (a press elsewhere, dragged onto a ledge, does nothing),
// with no weapon, carried object or locked force grab in that hand, and not at a holster.
//
// While a hand holds a ledge the player hangs from it: no gravity, no stick, no room-scale walk.
// The hand stays where it took hold, so the body goes wherever keeps it there: body origin =
// anchor - (hand - body origin), from the hand's position relative to the body in the latest move
// (pulling the hand down lifts the body; with both hands on, the average). The body moves through
// the world by player-box traces (sliding along what blocks it), at most `maxHangSpeed` units a
// second, and never with its feet above the ledge.
//
// Mantle: once a hand has pulled down by `mantlePull` units since it took hold and the head is
// `mantleHead` units above the ledge, and the player's box fits on top (with floor under it) and
// the way there (straight up, then over) is clear, the body is carried there in `mantleTime`
// seconds, and stands.
//
// Letting go of everything: the player falls, flung by the hands' release (vr_climb_fling, capped).
// Limits: no grab for `regrabDelay` seconds after letting go or mantling; while hanging (not
// standing), the other hand can only take a ledge about as high as the one held (`shimmyRise`):
// hands shimmy along a ledge but cannot climb a ladder of ledges; ledges lower than
// vr_climb_min_height above the feet are ignored.
//
// The QC never sees the grip of a hand that holds a ledge (its grab bits are masked in .vrbits0 from
// the press until the release), so the press picks nothing up; instead .vrbits0 bits 15 (off hand)
// and 16 (main hand) tell it the hand holds a ledge (QVR_VRBITS0_*HAND_CLIMBING: no melee blows).

#include "vr_climb.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_move.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_units.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

using namespace qvr;
using namespace qvr::progs;

namespace
{

// QC/vr_defs.qc QVR_VRBITS0_*: [0] off hand, [1] main hand.
constexpr int grabBit[2] = {1 << 1, 1 << 3};
constexpr int prevGrabBit[2] = {1 << 2, 1 << 4};
constexpr int climbingBit[2] = {1 << 15, 1 << 16};

// QVR_HS_* (vr_body.hpp): the grip spots the client reports per hand.
constexpr int HS_LEFT_SHOULDER_HOLSTER = 3;
constexpr int HS_HAND_SWITCH = 7;

constexpr float surfaceAbove = 12.f;  // the ledge's top may be this far above the hand (the hand sunk into it),
constexpr float surfaceBelow = 10.f;  // or this far below it
constexpr float handRoom = 16.f;      // clear space above the top for the hand
constexpr float edgeReach = 16.f;     // the drop must be this close to the hand
constexpr float edgeSlack = 6.f;      // a hand this far short of the edge still takes hold
constexpr float minDrop = 32.f;       // and this deep (a stair step is not a ledge)
constexpr float shimmyRise = 16.f;    // while hanging, a new hold at most this higher than the one held
constexpr float regrabDelay = 0.4f;   // seconds without new holds after letting go or mantling
constexpr float maxHangSpeed = 500.f; // units / second the body follows the hands at
constexpr float maxHangReach = 48.f;  // the body stays this close (horizontally) to the hold
constexpr float mantlePull = 8.f;     // units a hand must have pulled down before a mantle
constexpr float mantleHead = 8.f;     // and the head must be this far above the ledge
constexpr float mantleTime = 0.3f;    // seconds the mantle takes
constexpr float flingMax = 200.f;     // units / second the release flings at most,
constexpr float flingMaxUp = 150.f;   // and upwards

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return {v[0], v[1], v[2]};
}

void setVec(float* out, const glm::vec3& v)
{
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

[[nodiscard]] trace_t traceBox(const glm::vec3& start, const glm::vec3& mins, const glm::vec3& maxs,
    const glm::vec3& end, int type, edict_t* pass)
{
    vec3_t s{start.x, start.y, start.z}, mi{mins.x, mins.y, mins.z}, ma{maxs.x, maxs.y, maxs.z},
        e{end.x, end.y, end.z};
    return SV_Move(s, mi, ma, e, type, pass);
}

// A line through the world and brush models (a hand's probe: Quake's point hull).
[[nodiscard]] trace_t traceLine(const glm::vec3& start, const glm::vec3& end, edict_t* pass)
{
    return traceBox(start, glm::vec3{0.f}, glm::vec3{0.f}, end, MOVE_NOMONSTERS, pass);
}

// The player's box swept from `start` to `end`.
[[nodiscard]] trace_t tracePlayer(edict_t* ent, const glm::vec3& start, const glm::vec3& end)
{
    return traceBox(start, vec(ent->v.mins), vec(ent->v.maxs), end, MOVE_NORMAL, ent);
}

[[nodiscard]] bool debug()
{
    return vr_climb_debug.value != 0.f;
}

struct Ledge
{
    float top{0.f};             // the surface's height
    glm::vec3 out{0.f};         // horizontal, from the ledge towards the drop
    edict_t* ent{nullptr};      // the brush model it belongs to (nullptr: the world)
};

// Whether the hand at `hand` is at a ledge (see the top of the file).
[[nodiscard]] std::optional<Ledge> findLedgeAt(edict_t* player, const glm::vec3& hand)
{
    const trace_t down = traceLine(hand + glm::vec3{0.f, 0.f, surfaceAbove}, hand - glm::vec3{0.f, 0.f, surfaceBelow}, player);
    if(down.startsolid || down.allsolid || down.fraction >= 1.f || down.plane.normal[2] < 0.7f)
    {
        return std::nullopt;
    }

    Ledge ledge;
    ledge.top = down.endpos[2];
    ledge.ent = down.ent && down.ent != qcvm->edicts ? down.ent : nullptr;

    const glm::vec3 onTop{hand.x, hand.y, ledge.top + 2.f};
    if(traceLine(onTop, onTop + glm::vec3{0.f, 0.f, handRoom}, player).fraction < 1.f)
    {
        return std::nullopt; // no room for the hand
    }

    // The drop: in eight directions, the first towards the body.
    const glm::vec3 toBody = vec(player->v.origin) - hand;
    const float bodyYaw = std::atan2(toBody.y, toBody.x);
    constexpr float turns[8] = {0.f, 1.f, -1.f, 2.f, -2.f, 3.f, -3.f, 4.f};
    for(const float turn : turns)
    {
        const float yaw = bodyYaw + turn * glm::radians(45.f);
        const glm::vec3 dir{std::cos(yaw), std::sin(yaw), 0.f};
        for(float r = 4.f; r <= edgeReach; r += 4.f)
        {
            const glm::vec3 p = onTop + dir * r;
            if(traceLine(onTop, p, player).fraction < 1.f)
            {
                break; // a wall
            }
            const trace_t fall = traceLine(p, p - glm::vec3{0.f, 0.f, minDrop + 2.f}, player);
            if(fall.fraction >= 1.f)
            {
                ledge.out = dir;
                return ledge;
            }
            if(fall.endpos[2] < ledge.top - 3.f)
            {
                break; // a step down, not a drop
            }
        }
    }

    return std::nullopt;
}

// A hand hovering just in front of the edge (there is nothing to stop it) counts: the ledge is
// also looked for `edgeSlack` units further from the body.
[[nodiscard]] std::optional<Ledge> findLedge(edict_t* player, const glm::vec3& hand)
{
    if(std::optional<Ledge> ledge = findLedgeAt(player, hand))
    {
        return ledge;
    }
    glm::vec3 away = hand - vec(player->v.origin);
    away.z = 0.f;
    if(glm::length(away) < 1.f)
    {
        return std::nullopt;
    }
    const glm::vec3 further = hand + glm::normalize(away) * edgeSlack;
    if(traceLine(hand, further, player).fraction < 1.f)
    {
        return std::nullopt;
    }
    return findLedgeAt(player, further);
}

struct Grip
{
    bool active{false};
    glm::vec3 anchor{0.f};     // where the hand took hold (world)
    glm::vec3 relAtGrab{0.f};  // the hand relative to the body then
    Ledge ledge;
    int entNum{0};             // the ledge's brush model (0: the world), which may move
    glm::vec3 entOrigin{0.f};  // and where it was
    bool owned{false};         // the grip's press belongs to the ledge (hidden from the QC) until let go
    bool ownedLastFrame{false};
};

struct Climber
{
    Grip grips[2];
    bool mantling{false};
    double mantleStart{0.0};
    glm::vec3 mantleFrom{0.f}, mantleMid{0.f}, mantleTo{0.f};
    double noGrabUntil{0.0};
    double lastTime{-1.0};
    bool pressedLastFrame[2]{false, false};
    glm::vec3 lastOrigin{0.f}; // where the climb put the body (moved otherwise: let go)
    bool hanging() const
    {
        return grips[0].active || grips[1].active;
    }
};

std::vector<Climber> climbers;

[[nodiscard]] Climber* climberOf(edict_t* ent)
{
    const int client = NUM_FOR_EDICT(ent) - 1;
    if(client < 0 || client >= svs.maxclients)
    {
        return nullptr;
    }
    if(client >= static_cast<int>(climbers.size()))
    {
        climbers.resize(client + 1);
    }
    return &climbers[client];
}

// The hand `h` relative to the body, from the latest move (already moved along with the body).
[[nodiscard]] glm::vec3 handRel(edict_t* ent, const VrMove& move, int h)
{
    return move.hands[h].pos - vec(ent->v.origin);
}

[[nodiscard]] edict_t* entityField(edict_t* ent, int ofs)
{
    const int v = ofs >= 0 ? fieldInt(ent, ofs) : 0;
    return v ? PROG_TO_EDICT(v) : nullptr;
}

// Nothing in the hand: no weapon, no carried object, no locked force grab.
[[nodiscard]] bool handEmpty(edict_t* ent, int h)
{
    const FieldOffsets& f = fields();
    if(!bindings().isVrProgs)
    {
        return h == 0; // another mod's progs: the main hand always holds its weapon
    }
    const float weapon = h == 1 ? ent->v.weapon : fieldFloatOr(ent, f.weapon2, 0.f);
    if(weapon != 0.f)
    {
        return false;
    }
    const edict_t* held = entityField(ent, h == 1 ? f.mainhand_held : f.offhand_held);
    if(held && !held->free)
    {
        return false;
    }
    if(fieldFloatOr(ent, h == 1 ? f.mainhand_fglocked : f.offhand_fglocked, 0.f) != 0.f)
    {
        return false;
    }
    const edict_t* pulled = entityField(ent, h == 1 ? f.mainhand_fgpulled : f.offhand_fgpulled);
    return !pulled || pulled->free;
}

// Where grip `g`'s hold is now: it moves with its brush model.
[[nodiscard]] glm::vec3 anchorNow(const Grip& g)
{
    if(g.entNum <= 0 || g.entNum >= qcvm->num_edicts)
    {
        return g.anchor;
    }
    edict_t* e = EDICT_NUM(g.entNum);
    return g.anchor + (vec(e->v.origin) - g.entOrigin);
}

void letGoAll(Climber& c)
{
    c.grips[0].active = c.grips[1].active = false;
    c.mantling = false;
}

[[nodiscard]] bool onGround(edict_t* ent)
{
    return (static_cast<int>(ent->v.flags) & FL_ONGROUND) != 0;
}

// The mantle: a spot on top of grip `g`'s ledge where the box fits, over floor, reachable straight
// up and then over.
[[nodiscard]] bool findMantle(edict_t* ent, const Grip& g, glm::vec3& mid, glm::vec3& to)
{
    const glm::vec3 origin = vec(ent->v.origin);
    const glm::vec3 anchor = anchorNow(g);
    const float z = g.ledge.top - ent->v.mins[2] + 1.f;
    mid = {origin.x, origin.y, z};
    if(z < origin.z || tracePlayer(ent, origin, mid).fraction < 1.f)
    {
        return false;
    }
    for(const float k : {20.f, 28.f, 36.f})
    {
        to = glm::vec3{anchor.x, anchor.y, z} - g.ledge.out * k;
        if(tracePlayer(ent, to, to).startsolid)
        {
            continue;
        }
        if(traceBox(to, vec(ent->v.mins), vec(ent->v.maxs), to - glm::vec3{0.f, 0.f, 8.f}, MOVE_NOMONSTERS, ent).fraction >= 1.f)
        {
            continue; // nothing to stand on
        }
        if(tracePlayer(ent, mid, to).fraction < 1.f)
        {
            continue;
        }
        return true;
    }
    return false;
}

// Moves the body towards `target` through the world: the whole way, else up/down then across, else
// across then up/down (sliding along a wall or a ledge's face).
void moveBody(edict_t* ent, const glm::vec3& target)
{
    const glm::vec3 origin = vec(ent->v.origin);
    trace_t tr = tracePlayer(ent, origin, target);
    if(tr.allsolid)
    {
        return;
    }
    glm::vec3 best = vec(tr.endpos);
    if(tr.fraction < 1.f)
    {
        const glm::vec3 vertical{origin.x, origin.y, target.z};
        const glm::vec3 horizontal{target.x, target.y, origin.z};
        for(const glm::vec3& corner : {vertical, horizontal})
        {
            const trace_t a = tracePlayer(ent, origin, corner);
            if(a.allsolid)
            {
                continue;
            }
            const trace_t b = tracePlayer(ent, vec(a.endpos), target);
            const glm::vec3 end = b.allsolid ? vec(a.endpos) : vec(b.endpos);
            if(glm::distance(end, target) < glm::distance(best, target))
            {
                best = end;
            }
        }
    }
    setVec(ent->v.origin, best);
}

} // namespace

// SV_Physics_Client, before PlayerPreThink: takes hold of ledges and lets go (per the grips of the
// latest move), and hides the holding hands' grips from the QC.
extern "C" void VR_ClimbPreThink(edict_t* ent)
{
    Climber* cp = climberOf(ent);
    if(!cp)
    {
        return;
    }
    Climber& c = *cp;
    const double time = qcvm->time;
    if(c.lastTime < 0.0 || std::abs(time - c.lastTime) > 1.0) // a new map, a loaded game
    {
        c = Climber{};
    }
    c.lastTime = time;

    server::rebaseHands(ent);
    const VrMove* move = server::clientMove(ent);
    const bool tracked = move && (move->buttons & protocol::QVR_BUTTON_HANDSTRACKED);
    const bool enabled = vr_climb.value != 0.f && tracked && static_cast<int>(ent->v.movetype) == MOVETYPE_WALK &&
                         ent->v.health > 0.f;

    // Moved by something else (a teleporter, a respawn, setorigin), disabled, dead, noclipping, the
    // hold's brush model gone: let go, and fall as whatever moved the body has it.
    bool forced = !enabled;
    if((c.hanging() || c.mantling) && glm::distance(vec(ent->v.origin), c.lastOrigin) > 2.f)
    {
        if(debug())
        {
            Con_Printf("climb: moved away, letting go\n");
        }
        forced = true;
    }
    for(const Grip& g : c.grips)
    {
        if(g.active && g.entNum > 0 && (g.entNum >= qcvm->num_edicts || EDICT_NUM(g.entNum)->free))
        {
            forced = true;
        }
    }
    if(forced && (c.hanging() || c.mantling))
    {
        letGoAll(c);
        c.noGrabUntil = time + regrabDelay;
    }

    const bool wasHanging = c.hanging();
    const bool held[2] = {c.grips[0].active, c.grips[1].active};
    for(int h = 0; h < 2; h++)
    {
        Grip& g = c.grips[h];
        const bool pressed = move && (move->vrBits0 & grabBit[h]);
        const bool pressEdge = pressed && !c.pressedLastFrame[h];
        c.pressedLastFrame[h] = pressed;
        g.ownedLastFrame = g.owned;
        if(!pressed)
        {
            g.owned = false;
        }

        if(g.active && (!pressed || !handEmpty(ent, h)))
        {
            g.active = false;
            if(debug())
            {
                Con_Printf("climb: %s hand lets go\n", h ? "main" : "off");
            }
        }

        if(!enabled || !pressEdge || g.active || c.mantling)
        {
            continue;
        }
        // At a holster the grip draws a weapon (the two-hand and hand-switch spots need something in
        // a hand, and an empty hand at a ledge is not reaching for it).
        const int hotspot = move->hotspots[h];
        const bool atHolster = hotspot >= HS_LEFT_SHOULDER_HOLSTER && hotspot != HS_HAND_SWITCH;
        if(time < c.noGrabUntil || atHolster || !handEmpty(ent, h))
        {
            if(debug())
            {
                Con_Printf("climb: %s hand grips: %s\n", h ? "main" : "off",
                    time < c.noGrabUntil ? "too soon" : atHolster ? "at a holster" : "not empty");
            }
            continue;
        }

        const glm::vec3 hand = move->hands[h].pos;
        const std::optional<Ledge> ledge = findLedge(ent, hand);
        if(!ledge)
        {
            if(debug())
            {
                Con_Printf("climb: %s hand at (%.1f %.1f %.1f): no ledge\n", h ? "main" : "off", hand.x, hand.y, hand.z);
            }
            continue;
        }

        const float feet = ent->v.origin[2] + ent->v.mins[2];
        if(ledge->top < feet + vr_climb_min_height.value)
        {
            if(debug())
            {
                Con_Printf("climb: ledge at %.1f too low (feet %.1f)\n", ledge->top, feet);
            }
            continue;
        }

        // Hanging, only a hold about as high as the one held: shimmy, not a ladder.
        const Grip& other = c.grips[1 - h];
        if(other.active && !onGround(ent) && ledge->top > other.ledge.top + shimmyRise)
        {
            if(debug())
            {
                Con_Printf("climb: ledge at %.1f too far above the one held (%.1f)\n", ledge->top, other.ledge.top);
            }
            continue;
        }

        g.active = true;
        g.owned = true;
        g.anchor = hand;
        g.relAtGrab = handRel(ent, *move, h);
        g.ledge = *ledge;
        g.entNum = ledge->ent ? NUM_FOR_EDICT(ledge->ent) : 0;
        g.entOrigin = ledge->ent ? vec(ledge->ent->v.origin) : glm::vec3{0.f};
        c.lastOrigin = vec(ent->v.origin);
        server::sendHaptic(ent, h, 0.f, 0.06f, 80.f, 0.6f);
        if(debug())
        {
            Con_Printf("climb: %s hand holds the ledge at %.1f (out %.2f %.2f) from (%.1f %.1f %.1f)\n",
                h ? "main" : "off", ledge->top, ledge->out.x, ledge->out.y, ent->v.origin[0], ent->v.origin[1],
                ent->v.origin[2]);
        }
    }

    // Let go of everything: fall, flung by the hands' release.
    if(wasHanging && !c.hanging() && !c.mantling)
    {
        c.noGrabUntil = time + regrabDelay;
        glm::vec3 fling{0.f};
        if(move)
        {
            const int hands = (held[0] ? 1 : 0) + (held[1] ? 1 : 0);
            for(int h = 0; h < 2; h++)
            {
                if(held[h])
                {
                    fling -= move->hands[h].throwVel / static_cast<float>(hands);
                }
            }
            fling *= units::metresToUnits() * std::max(0.f, vr_climb_fling.value);
            fling.z = std::min(fling.z, flingMaxUp);
            if(glm::length(fling) > flingMax)
            {
                fling *= flingMax / glm::length(fling);
            }
        }
        setVec(ent->v.velocity, fling);
        if(debug())
        {
            Con_Printf("climb: falls, flung at (%.0f %.0f %.0f)\n", fling.x, fling.y, fling.z);
        }
    }

    // The QC: the holding hands' grips hidden; which hands hold a ledge.
    const int vrbits = fields().vrbits0;
    if(vrbits >= 0)
    {
        int bits = static_cast<int>(fieldFloat(ent, vrbits));
        for(int h = 0; h < 2; h++)
        {
            const Grip& g = c.grips[h];
            if(g.owned)
            {
                bits &= ~grabBit[h];
            }
            if(g.owned || g.ownedLastFrame)
            {
                bits &= ~prevGrabBit[h];
            }
            bits = g.active ? bits | climbingBit[h] : bits & ~climbingBit[h];
        }
        fieldFloat(ent, vrbits) = static_cast<float>(bits);
    }
}

// SV_Physics_Client, in place of the move: the body hangs from the hands, or mantles.
// 0: not climbing (move as usual); 1: moved; -1: the entity was freed by its think.
extern "C" int VR_ClientClimb(edict_t* ent)
{
    Climber* cp = climberOf(ent);
    if(!cp || (!cp->hanging() && !cp->mantling))
    {
        return 0;
    }
    Climber& c = *cp;
    if(!SV_RunThink(ent))
    {
        return -1;
    }

    const double time = qcvm->time;
    if(c.mantling)
    {
        const float up = glm::distance(c.mantleFrom, c.mantleMid);
        const float over = glm::distance(c.mantleMid, c.mantleTo);
        const float t = CLAMP(0.f, static_cast<float>((time - c.mantleStart) / mantleTime), 1.f);
        const float along = t * (up + over);
        const glm::vec3 pos = along <= up && up > 0.f
                                  ? glm::mix(c.mantleFrom, c.mantleMid, along / up)
                                  : glm::mix(c.mantleMid, c.mantleTo, over > 0.f ? (along - up) / over : 1.f);
        setVec(ent->v.origin, pos);
        setVec(ent->v.velocity, glm::vec3{0.f});
        ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
        if(t >= 1.f)
        {
            c.mantling = false;
            c.noGrabUntil = time + regrabDelay;
            ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) | FL_ONGROUND);
            if(debug())
            {
                Con_Printf("climb: mantled onto (%.1f %.1f %.1f)\n", pos.x, pos.y, pos.z);
            }
        }
        c.lastOrigin = pos;
        SV_CheckWater(ent);
        return 1;
    }

    server::rebaseHands(ent);
    const VrMove* move = server::clientMove(ent);
    if(!move)
    {
        letGoAll(c);
        return 0;
    }

    // Where the hands would have the body: each hold minus the hand relative to the body.
    const glm::vec3 rel[2] = {handRel(ent, *move, 0), handRel(ent, *move, 1)}; // before the body moves
    glm::vec3 target{0.f};
    glm::vec3 anchors{0.f};
    float top = -1e9f;
    int count = 0;
    for(int h = 0; h < 2; h++)
    {
        const Grip& g = c.grips[h];
        if(g.active)
        {
            const glm::vec3 a = anchorNow(g);
            target += a - rel[h];
            anchors += a;
            top = std::max(top, g.ledge.top);
            count++;
        }
    }
    target /= static_cast<float>(count);
    anchors /= static_cast<float>(count);

    // Never above the ledge (the mantle does that), never far from the hold.
    target.z = std::min(target.z, top - ent->v.mins[2] + 2.f);
    glm::vec2 fromHold{target.x - anchors.x, target.y - anchors.y};
    if(glm::length(fromHold) > maxHangReach)
    {
        fromHold *= maxHangReach / glm::length(fromHold);
        target.x = anchors.x + fromHold.x;
        target.y = anchors.y + fromHold.y;
    }

    const glm::vec3 origin = vec(ent->v.origin);
    glm::vec3 step = target - origin;
    const float maxStep = maxHangSpeed * static_cast<float>(host_frametime);
    if(glm::length(step) > maxStep)
    {
        step *= maxStep / glm::length(step);
    }
    moveBody(ent, origin + step);
    setVec(ent->v.velocity, glm::vec3{0.f});
    ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
    c.lastOrigin = vec(ent->v.origin);
    SV_CheckWater(ent);

    if(debug() && vr_climb_debug.value >= 2.f)
    {
        Con_Printf("climb: hang (%.1f %.1f %.1f) target (%.1f %.1f %.1f)\n", ent->v.origin[0], ent->v.origin[1],
            ent->v.origin[2], target.x, target.y, target.z);
    }

    // The mantle: pulled down, the head over the ledge, and room on top.
    const float headZ = move->headPos.z;
    for(int h = 0; h < 2; h++)
    {
        const Grip& g = c.grips[h];
        if(!g.active)
        {
            continue;
        }
        const float pulled = g.relAtGrab.z - rel[h].z;
        const float headAbove = headZ + (ent->v.origin[2] - origin.z) - g.ledge.top;
        if(pulled < mantlePull || headAbove < mantleHead)
        {
            continue;
        }
        glm::vec3 mid, to;
        if(!findMantle(ent, g, mid, to))
        {
            if(debug() && vr_climb_debug.value >= 2.f)
            {
                Con_Printf("climb: no room to mantle\n");
            }
            continue;
        }
        c.mantling = true;
        c.mantleStart = time;
        c.mantleFrom = vec(ent->v.origin);
        c.mantleMid = mid;
        c.mantleTo = to;
        c.grips[0].active = c.grips[1].active = false;
        if(debug())
        {
            Con_Printf("climb: mantle from (%.1f %.1f %.1f) up to %.1f, onto (%.1f %.1f %.1f)\n", c.mantleFrom.x,
                c.mantleFrom.y, c.mantleFrom.z, mid.z, to.x, to.y, to.z);
        }
        break;
    }

    return 1;
}

namespace
{

// vr_climb_probe: the ledges in front of the player (debugging: where a hand could take hold).
void probe(edict_t* ent)
{
    vec3_t fwd, right, up;
    vec3_t yawOnly{0.f, ent->v.angles[1], 0.f};
    AngleVectors(yawOnly, fwd, right, up);
    const glm::vec3 origin = vec(ent->v.origin);
    const float feet = origin.z + ent->v.mins[2];
    int found = 0;
    for(float d = 16.f; d <= 64.f && found < 12; d += 4.f)
    {
        for(float z = feet + 16.f; z <= feet + 128.f && found < 12; z += 4.f)
        {
            const glm::vec3 p{origin.x + fwd[0] * d, origin.y + fwd[1] * d, z};
            if(const std::optional<Ledge> l = findLedge(ent, p))
            {
                Con_Printf("ledge: hand (%.0f %.0f %.0f) top %.1f (%.0f above the feet) out (%.2f %.2f)\n", p.x, p.y, p.z,
                    l->top, l->top - feet, l->out.x, l->out.y);
                found++;
                z = l->top + surfaceBelow; // the next ledge up
            }
        }
    }
    if(!found)
    {
        Con_Printf("no ledge within 64 units ahead\n");
    }
}

void probe_f()
{
    if(!sv.active || svs.maxclients < 1 || !svs.clients[0].active || !svs.clients[0].edict)
    {
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    probe(svs.clients[0].edict);
    PR_PopQCVM(oldvm);
}

} // namespace

void qvr::climb::init()
{
    Cmd_AddCommand("vr_climb_probe", probe_f);
}
