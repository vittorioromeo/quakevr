// vr_heldpose.cpp -- see vr_heldpose.hpp.

#include "vr_heldpose.hpp"
#include "vr_seat.hpp"
#include "vr_moveguard.hpp"

#include "vr_client.hpp"

#include "vr_api.h"
#include "vr_body.hpp"
#include "vr_server.hpp"
#include "vr_clientprefs.hpp"
#include "vr_cvars.hpp"
#include "vr_flatplay.hpp"
#include "vr_hands.hpp"
#include "vr_held.hpp"
#include "vr_magholster.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_progs.hpp"
#include "vr_tunables.hpp"
#include "vr_twohand.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"


namespace qvr::heldpose
{
namespace
{

// "vr_heldpose <version> <hand> <valueCount values>" (encode's order); another version's or count's is ignored.
constexpr int commandVersion = 3; // (2: flagMag, B6b; 3: the carried grip, the leaving flip and slot, B11c)
constexpr int valueCount = 66;
constexpr float step = 0.05f; // (units, and the directions' parts as much: a change smaller goes up with the next one)
constexpr float most = 256.f; // (a value farther than this: not a hand's, the command ignored)
constexpr double spotHold = 0.25; // s: a ground spot lost goes up this late (send; twohand::groundSpot's grace)

enum Flag : int
{
    flagDrawn = 1,
    flagMirrored = 2,
    flagCarrying = 4,
    flagAxes = 8,
    flagMayReverse = 16,
    flagFlippable = 32,
    flagMag = 64,
    flagFlipBlocked = 128
};

void put3(float* v, int at, const glm::vec3& p)
{
    v[at] = p.x;
    v[at + 1] = p.y;
    v[at + 2] = p.z;
}

[[nodiscard]] glm::vec3 get3(const float* v, int at)
{
    return {v[at], v[at + 1], v[at + 2]};
}

void encode(const Hand& h, float v[valueCount])
{
    v[0] = static_cast<float>((h.drawn ? flagDrawn : 0) | (h.mirrored ? flagMirrored : 0) | (h.carrying ? flagCarrying : 0) |
                              (h.axesKnown ? flagAxes : 0) | (h.mayReverse ? flagMayReverse : 0) | (h.flippable ? flagFlippable : 0) |
                              (h.mag ? flagMag : 0) | (h.flipBlocked ? flagFlipBlocked : 0));
    v[1] = static_cast<float>(h.slot);
    v[2] = static_cast<float>(h.helpKind);
    v[3] = h.flip;
    for(int c = 0; c < 4; c++)
    {
        put3(v, 4 + c * 3, glm::vec3{h.inHand[c]});
    }
    for(int i = 0; i < maxSpots; i++)
    {
        v[16 + i * 6] = static_cast<float>(h.spotType[i]);
        put3(v, 17 + i * 6, h.spotPos[i]);
        v[20 + i * 6] = h.spotBias[i];
        v[21 + i * 6] = h.spotShare[i];
    }
    put3(v, 40, h.carriedHandle);
    put3(v, 43, h.carriedTip);
    put3(v, 46, h.carriedAxes[0]);
    put3(v, 49, h.carriedAxes[2]);
    put3(v, 52, h.axesForward);
    put3(v, 55, h.axesUp);
    v[58] = static_cast<float>(h.outOfEnt);
    put3(v, 59, h.outOf);
    v[62] = static_cast<float>(h.carriedGrip);
    v[63] = h.carriedGripCos;
    v[64] = h.leaveFlip;
    v[65] = static_cast<float>(h.leaveSlot);
}

// An integer field of a record (|v| <= most: checked by the command), clamped to [lo, hi].
[[nodiscard]] int intIn(float v, int lo, int hi)
{
    return za::clamp(static_cast<int>(v), lo, hi);
}

[[nodiscard]] Hand decode(const float v[valueCount])
{
    Hand h;
    const int flags = static_cast<int>(v[0]);
    h.drawn = (flags & flagDrawn) != 0;
    h.mirrored = (flags & flagMirrored) != 0;
    h.carrying = (flags & flagCarrying) != 0;
    h.axesKnown = (flags & flagAxes) != 0;
    h.mayReverse = (flags & flagMayReverse) != 0;
    h.flippable = (flags & flagFlippable) != 0;
    h.mag = (flags & flagMag) != 0;
    h.flipBlocked = (flags & flagFlipBlocked) != 0;
    // (The integers clamped to their domains: a guest's record indexes the host's tables, e.g. leaveSlot the weapon
    // cvars through view::leavingTurn; mpfix C1.)
    h.slot = intIn(v[1], -1, weapons::numSlots - 1);
    h.helpKind = intIn(v[2], 0, 2);
    h.flip = v[3];
    for(int c = 0; c < 4; c++)
    {
        h.inHand[c] = glm::vec4{get3(v, 4 + c * 3), c == 3 ? 1.f : 0.f};
    }
    for(int i = 0; i < maxSpots; i++)
    {
        h.spotType[i] = intIn(v[16 + i * 6], 0, static_cast<int>(weapons::HotspotType::Cup));
        h.spotPos[i] = get3(v, 17 + i * 6);
        h.spotBias[i] = v[20 + i * 6];
        h.spotShare[i] = v[21 + i * 6];
    }
    h.carriedHandle = get3(v, 40);
    h.carriedTip = get3(v, 43);
    const glm::vec3 fw = get3(v, 46), up = get3(v, 49);
    if(glm::length(fw) > 0.5f && glm::length(up) > 0.5f)
    {
        const glm::vec3 f = glm::normalize(fw);
        const glm::vec3 l = glm::normalize(glm::cross(up, f));
        h.carriedAxes = glm::mat3{f, l, glm::cross(f, l)};
    }
    h.axesForward = get3(v, 52);
    h.axesUp = get3(v, 55);
    h.outOfEnt = static_cast<int>(v[58]);
    h.outOf = get3(v, 59);
    h.carriedGrip = intIn(v[62], -1, 1);
    h.carriedGripCos = v[63];
    h.leaveFlip = za::clamp(v[64], 0.f, 1.f);
    h.leaveSlot = intIn(v[65], -1, weapons::numSlots - 1);
    return h;
}

// One hand's ground spot (B3f): the weapon entity it is near a hotspot of (0 none) and that hotspot.
struct Spot
{
    int entity{0};
    int index{-1};
};

// A seat's client: what went up last, each hand's (phase 4b).
struct Sent
{
    float sent[2][valueCount]{};
    bool wasSent[2]{};
    double nextSend[2]{};
    Spot spotSent[2];   // (B3f)
    double spotSeen[2]{}; // when each hand last had a spot (realtime)
    int magPending{0};  // the holster presses taken from magholster::poll, not sent yet
};

// splitscreen (SPLITSCREEN.md, phase 4b): the current seat's (seat::Part::HeldPose)
[[nodiscard]] Sent& up()
{
    return seat::part<Sent>(seat::current(), seat::Part::HeldPose);
}

// The server's: each slot's.
struct Slots
{
    Hand slot[MAX_SCOREBOARD][2];
    bool slotHas[MAX_SCOREBOARD][2]{};
    Spot slotSpot[MAX_SCOREBOARD][2];
    int slotMag[MAX_SCOREBOARD]{};
    Holsters slotHolsters[MAX_SCOREBOARD]; // (this game's own: publishOwn's)
    bool slotBot[MAX_SCOREBOARD]{};        // (bots3) the slot's hands placed by placeBots (a bot's)
};
Slots slots; // seat-shared: the server's, each player slot's

[[nodiscard]] int numOf(const edict_t* e)
{
    if(!e || !sv.qcvm.edicts)
    {
        return -1;
    }
    return static_cast<int>(((const byte*)e - (const byte*)sv.qcvm.edicts) / sv.qcvm.edict_size);
}

[[nodiscard]] edict_t* edictOf(int num)
{
    return num > 0 && num < sv.qcvm.num_edicts ? (edict_t*)((byte*)sv.qcvm.edicts + num * sv.qcvm.edict_size) : nullptr;
}

// A weapon edict as the view's entity_t (its model, place, turn): what view::reverseGripChoice reads.
[[nodiscard]] entity_t weaponEntity(const edict_t* weapon)
{
    entity_t ent{};
    const int index = static_cast<int>(weapon->v.modelindex);
    ent.model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    VectorCopy(weapon->v.origin, ent.origin);
    VectorCopy(weapon->v.angles, ent.angles);
    return ent;
}

// The weapon lying about nearest `at` (vr_mp_drawn's): on the server the thrown_weapon edict, else 0.
[[nodiscard]] int nearestServerWeapon(const glm::vec3& at)
{
    int best = 0;
    float bestDist = 0.f;
    for(int i = svs.maxclients + 1; i < sv.qcvm.num_edicts; i++)
    {
        edict_t* e = edictOf(i);
        if(!e || e->free || !e->v.modelindex || ZA_STRCMP(PR_GetString(e->v.classname), "thrown_weapon") != 0)
        {
            continue;
        }
        const float d = glm::distance(at, glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]});
        if(!best || d < bestDist)
        {
            best = i;
            bestDist = d;
        }
    }
    return best;
}

// The same on this client: the weapon model drawn nearest `at` that is no view entity of its own (as the view's ground
// spots look for one: vr_view_hands.cpp updateGroundSpots).
[[nodiscard]] int nearestClientWeapon(const glm::vec3& at)
{
    int best = 0;
    float bestDist = 0.f;
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        const entity_t* e = cl_visedicts[i];
        if(!e || e < cl_entities || e >= cl_entities + cl_max_edicts || !e->model || e->model->type != mod_alias ||
            view::find(e) || e == &cl_entities[cl.viewentity] || !modelmeta::has(e->model, modelmeta::Trait::ViewWeapon))
        {
            continue;
        }
        const float d = glm::distance(at, glm::vec3{e->origin[0], e->origin[1], e->origin[2]});
        if(!best || d < bestDist)
        {
            best = static_cast<int>(e - cl_entities);
            bestDist = d;
        }
    }
    return best;
}

// The client's own `hand` as his view drew it last, in the hand's frame (vr_heldpose.hpp).
[[nodiscard]] Hand measure(int hand)
{
    Hand h;
    // The hand's pose the weapon was drawn on (its entity less its place in the hand): the hotspots and the carried
    // weapon were placed on it, so they go into its frame (the tracked pose of this frame may be a frame on).
    glm::mat4 handPoseDrawn{1.f};
    bool havePose = false;
    const view::DrawnWeapon& d = view::drawnWeapon(hand);
    glm::vec3 origin, angles;
    if(d.model && vr_gametime - d.when <= 0.5)
    {
        h.drawn = true;
        h.mirrored = d.mirrored;
        h.mag = d.mag != nullptr;
        h.slot = weapons::slotForModel(d.model);
        h.inHand = d.inHand;
    }
    if(h.drawn && view::drawnWeaponPlace(hand, d.model, origin, angles))
    {
        float m[16];
        vec3_t o, a;
        for(int i = 0; i < 3; i++)
        {
            o[i] = origin[i];
            a[i] = angles[i];
        }
        R_EntityMatrix(m, o, a, ENTSCALE_DEFAULT);
        glm::mat4 entity;
        for(int c = 0; c < 4; c++)
        {
            for(int row = 0; row < 4; row++)
            {
                entity[c][row] = m[c * 4 + row];
            }
        }
        handPoseDrawn = entity * glm::inverse(d.inHand);
        // (A weapon carried off its handle is placed again after its place in the hand was taken (the carry's pose): its
        // entity less that place is not the hand. Its hotspots go into the tracked hand's frame below, the frame the
        // carry places it in: B3f, the server's carried hotspots 1.4 units off before.)
        havePose = !twohand::carrying(hand);
    }
    const hands::State& s = hands::current();
    if(!havePose && s.valid)
    {
        handPoseDrawn = glm::mat4{held::axesFromAngles(&s.rot[hand][0], true)};
        handPoseDrawn[3] = glm::vec4{s.pos[hand], 1.f};
        havePose = true;
    }
    if(havePose)
    {
        const glm::mat4 toHand = glm::inverse(handPoseDrawn);
        for(int i = 0; i < maxSpots; i++)
        {
            const view::WeaponHotspot w = view::weaponHotspot(hand, i);
            if(w.type != 0)
            {
                h.spotType[i] = w.type;
                h.spotPos[i] = glm::vec3{toHand * glm::vec4{w.pos, 1.f}};
                h.spotBias[i] = w.bias;
                h.spotShare[i] = w.share;
            }
        }
        glm::vec3 pos, rot, tip;
        bool mirrored = false;
        if(twohand::carrying(hand) && view::carriedWeaponPose(hand, pos, rot, mirrored, tip) &&
            view::carriedWeaponInHand(hand, h.carriedHandle, h.carriedTip))
        {
            h.carrying = true;
            h.carriedAxes = glm::mat3{toHand} * held::axesFromAngles(&rot[0], true);
        }
    }
    // (what is not placed on the hand: whatever the pose)
    h.helpKind = twohand::helpKind(hand);
    h.axesKnown = view::heldWeaponAxes(hand, h.axesForward, h.axesUp, h.flip);
    h.mayReverse = view::heldMayReverse(hand);
    h.flippable = view::heldGripFlippable(hand);
    h.flipBlocked = h.flippable && view::gripFlipBlocked(hand);
    const int other = held::heldAlone(1 - hand);
    if(other > 0)
    {
        h.outOf = view::heldOutOf(hand, other);
        h.outOfEnt = h.outOf != glm::vec3{0.f} ? other : 0;
    }
    h.carriedGrip = view::carriedGripChoice(hand, h.carriedGripCos);
    h.leaveFlip = view::leavingFlip(hand);
    h.leaveSlot = weapons::heldSlot(hand);
    return h;
}

// "vr_heldpose <version> <hand> <values>": a client's drawn pose of that hand (send), kept for his slot.
void heldPose_f()
{
    if(cmd_source != src_client || !sv.active || !host_client || Cmd_Argc() != 3 + valueCount ||
        Q_atoi(Cmd_Argv(1)) != commandVersion)
    {
        return;
    }
    const int slot = static_cast<int>(host_client - svs.clients);
    const int hand = Q_atoi(Cmd_Argv(2));
    if(slot < 0 || slot >= MAX_SCOREBOARD || (hand != 0 && hand != 1))
    {
        return;
    }
    float v[valueCount];
    for(int i = 0; i < valueCount; i++)
    {
        v[i] = Q_atof(Cmd_Argv(3 + i));
        if(!(za::abs(v[i]) <= most))
        {
            return;
        }
    }
    slots.slot[slot][hand] = decode(v);
    slots.slotHas[slot][hand] = true;
}

// The client command's sender's slot, or -1 (not a client's, no server).
[[nodiscard]] int senderSlot()
{
    if(cmd_source != src_client || !sv.active || !host_client)
    {
        return -1;
    }
    const int slot = static_cast<int>(host_client - svs.clients);
    return slot >= 0 && slot < MAX_SCOREBOARD ? slot : -1;
}

// "vr_grabspot <hand> <entity> <index>": the weapon a client's empty hand is near a hotspot of (0 -1: none), kept for his
// slot until the next.
void grabSpot_f()
{
    const int slot = senderSlot();
    const int hand = Q_atoi(Cmd_Argv(1));
    const int entity = Q_atoi(Cmd_Argv(2));
    const int index = Q_atoi(Cmd_Argv(3));
    if(slot < 0 || Cmd_Argc() != 4 || (hand != 0 && hand != 1) || entity < 0 || entity >= sv.qcvm.max_edicts || index < -1 ||
        index > twohand::anywhereSpot)
    {
        return;
    }
    slots.slotSpot[slot][hand] = Spot{entity, entity > 0 ? index : -1};
}

// "vr_magpoll <bits>": a client's magnetic holster presses (magholster::poll's), kept until his QC asks.
void magPoll_f()
{
    const int slot = senderSlot();
    if(slot >= 0 && Cmd_Argc() == 2)
    {
        slots.slotMag[slot] |= Q_atoi(Cmd_Argv(1)) & 7;
    }
}

// vr_mp_drawn's B3f parts of a hand: its ground spot, the weapon lying nearest it and how it would take it (the reverse
// grip's choice and cosine), and the hand's place and angles.
void printGround(const Spot& spot, int nearest, int choice, float cosine, const glm::vec3& hand, const glm::vec3& rot)
{
    Con_Printf(" gs=%d,%d rev=%d,%d,%.2f at=%.2f,%.2f,%.2f rot=%.1f,%.1f,%.1f", spot.entity, spot.index, nearest, choice, cosine,
        hand.x, hand.y, hand.z, rot.x, rot.y, rot.z);
}

// vr_mp_drawn: the held weapons' hotspots as this client draws them ("mp_drawn_own:", world) and, hosting, as the server
// places each player's ("mp_drawn_sv:"): the multiplayer harness compares them.
void printHand(const char* prefix, int ent, int hand, const Hand& h, const glm::vec3* spots)
{
    Con_Printf("%s: ent=%d hand=%d drawn=%d slot=%d mirrored=%d help=%d carrying=%d axes=%d flip=%.2f outof=%d", prefix, ent,
        hand, h.drawn ? 1 : 0, h.slot, h.mirrored ? 1 : 0, h.helpKind, h.carrying ? 1 : 0, h.axesKnown ? 1 : 0, h.flip,
        h.outOfEnt);
    for(int i = 0; i < maxSpots; i++)
    {
        if(h.spotType[i] != 0)
        {
            Con_Printf(" hs%d=%d,%.2f,%.2f,%.2f", i, h.spotType[i], spots[i].x, spots[i].y, spots[i].z);
        }
    }
}

void mpDrawn_f()
{
    if(cls.state == ca_connected && cl.viewentity > 0)
    {
        for(int hand = 0; hand < 2; hand++)
        {
            const Hand h = measure(hand);
            glm::vec3 spots[maxSpots];
            for(int i = 0; i < maxSpots; i++)
            {
                spots[i] = view::weaponHotspot(hand, i).pos;
            }
            printHand("mp_drawn_own", cl.viewentity, hand, h, spots);
            Spot spot;
            twohand::groundSpotNow(hand, spot.entity, spot.index);
            const glm::vec3 at = hands::current().pos[hand];
            const int nearest = nearestClientWeapon(at);
            float cosine = 1.f;
            const int choice = nearest > 0 ? view::reverseGripChoice(cl_entities[nearest], hand, cosine) : -1;
            printGround(spot, nearest, choice, cosine, at, hands::current().visualRot[hand]);
            Con_Printf("\n");
        }
    }
    if(!sv.active)
    {
        return;
    }
    const progs::ServerVm vm;
    for(int slot = 0; slot < svs.maxclients && slot < MAX_SCOREBOARD; slot++)
    {
        if(!svs.clients[slot].active)
        {
            continue;
        }
        edict_t* player = edictOf(slot + 1);
        for(int hand = 0; hand < 2 && player; hand++)
        {
            const Hand* r = slotHand(player, hand);
            const Hand h = r ? *r : Hand{};
            glm::vec3 spots[maxSpots];
            for(int i = 0; i < maxSpots; i++)
            {
                spots[i] = h.placed ? h.spotWorld[i] : toWorld(player, hand, h.spotPos[i]);
            }
            printHand("mp_drawn_sv", slot + 1, hand, h, spots);
            glm::vec3 at;
            glm::mat3 axes;
            handPose(player, hand, at, axes);
            const int nearest = nearestServerWeapon(at);
            float cosine = 1.f;
            const int choice = nearest > 0 ? reverseGrip(player, hand, edictOf(nearest), cosine) : -1;
            const progs::HandFields& f = progs::handFields(hand);
            printGround(slots.slotSpot[slot][hand], nearest, choice, cosine, at,
                f.rot >= 0 ? progs::fieldVec(player, f.rot) : glm::vec3{0.f});
            // (the nearest weapon's speed: the harness waits for one let go to come to rest before reaching for it)
            const edict_t* n = nearest > 0 ? edictOf(nearest) : nullptr;
            // (and where it lies: B5's harness puts the other player beside a weapon one let go of)
            Con_Printf(" nspeed=%.1f norg=%.1f,%.1f,%.1f\n", n ? static_cast<double>(glm::length(glm::vec3{n->v.velocity[0],
                n->v.velocity[1], n->v.velocity[2]})) : -1.0, n ? static_cast<double>(n->v.origin[0]) : 0.0,
                n ? static_cast<double>(n->v.origin[1]) : 0.0, n ? static_cast<double>(n->v.origin[2]) : 0.0);
        }
    }
}

} // namespace

// `player`'s slot (a client's: under svs.maxclients), else -1.
[[nodiscard]] int playerSlot(const edict_t* player)
{
    return progs::clientSlot(player);
}

int grabSpot(edict_t* player, int hand, const edict_t* weapon)
{
    const int num = numOf(weapon);
    const int slot = playerSlot(player);
    if((hand != 0 && hand != 1) || slot < 0)
    {
        return 0;
    }
    const Spot& s = slots.slotSpot[slot][hand];
    if(vr_debug_carry.value >= 2.f)
    {
        Con_Printf("grabspot: player %d's %s hand, weapon %d: his client's spot %d %d\n", slot + 1, hand == 0 ? "off" : "main",
            num, s.entity, s.index);
    }
    if(num <= 0 || s.entity != num || s.index < 0)
    {
        return 0;
    }
    // His claim only for a weapon his hand is in reach of on the server (vr_moveguard.hpp: an arm's reach and a weapon's
    // length): another player's claim of a hotspot of something across the level, refused and counted.
    if(clientprefs::ownSlot() != slot && progs::handFields(hand).pos >= 0)
    {
        const glm::vec3 at = progs::fieldVec(player, progs::handFields(hand).pos);
        const glm::vec3 w{weapon->v.origin[0], weapon->v.origin[1], weapon->v.origin[2]};
        if(glm::length(w - at) > moveguard::reachMetres * moveguard::unitsPerMetre(moveguard::slotScale(slot)))
        {
            moveguard::countSpot(slot);
            return 0;
        }
    }
    return s.index + 1;
}

int reverseGrip(edict_t* player, int hand, const edict_t* weapon, float& cosine)
{
    cosine = 1.f;
    const int slot = player ? playerSlot(player) : clientprefs::ownSlot();
    if(!weapon || (hand != 0 && hand != 1) || slot < 0 || slot >= MAX_SCOREBOARD)
    {
        return -1;
    }
    const entity_t ent = weaponEntity(weapon);
    if(slots.slotHas[slot][hand] && slots.slot[slot][hand].placed)
    {
        // The hand's own turn and vr_revgrip as the view measuring it had them this frame (this game's own player).
        const Hand& h = slots.slot[slot][hand];
        return view::reverseGripChoiceAt(ent, hand, h.gripRot, h.gripMayReverse, cosine);
    }
    if(!player)
    {
        return -1;
    }
    // His move's hand (the angles his client drew it at), his own vr_revgrip, his flat-screen play.
    const progs::HandFields& f = progs::handFields(hand);
    const glm::vec3 rot = f.rot >= 0 ? progs::fieldVec(player, f.rot) : glm::vec3{player->v.angles[0], player->v.angles[1], 0.f};
    const bool flat = progs::fieldFloatOr(player, progs::fields().vr_flatscreen, 0.f) != 0.f;
    const cvar_t* var = tunableFind("vr_revgrip");
    const float revgrip = clientprefs::value(clientprefs::indexOf(var), var, player);
    return view::reverseGripChoiceAt(ent, hand, rot, revgrip != 0.f && !flat, cosine);
}

int magPoll(const edict_t* player)
{
    const int slot = playerSlot(player);
    if(slot < 0)
    {
        return 0;
    }
    const int bits = slots.slotMag[slot];
    slots.slotMag[slot] = 0;
    if(bits && developer.value)
    {
        Con_Printf("magholster: player %d asked %d\n", slot + 1, bits);
    }
    return bits;
}

const Hand* slotHand(const edict_t* player, int hand)
{
    const int slot = playerSlot(player);
    return slot >= 0 && (hand == 0 || hand == 1) && slots.slotHas[slot][hand] ? &slots.slot[slot][hand] : nullptr;
}

void publishOwn()
{
    const int own = clientprefs::ownSlot();
    if(own < 0 || own >= MAX_SCOREBOARD)
    {
        return;
    }
    for(int hand = 0; hand < 2; hand++)
    {
        slots.slotHas[own][hand] = cls.state == ca_connected;
        slots.slotSpot[own][hand] = Spot{};
        if(!slots.slotHas[own][hand])
        {
            slots.slot[own][hand] = Hand{};
            continue;
        }
        Hand h = measure(hand);
        h.placed = true;
        for(int i = 0; i < maxSpots; i++)
        {
            const view::WeaponHotspot w = view::weaponHotspot(hand, i);
            h.spotType[i] = w.type;
            h.spotWorld[i] = w.pos;
            h.spotBias[i] = w.bias;
            h.spotShare[i] = w.share;
        }
        bool mirrored = false;
        h.carriedKnown = twohand::carrying(hand) && view::carriedWeaponPose(hand, h.carriedPos, h.carriedRot, mirrored, h.carriedTipWorld);
        glm::vec3 handle{0.f}, tip{0.f};
        h.carrying = h.carriedKnown && view::carriedWeaponInHand(hand, handle, tip);
        h.carriedHandle = handle;
        h.carriedTip = tip;
        // (view::reverseGripChoice's: the hand's own turn, not its drawn one)
        h.gripRot = hands::current().rot[hand];
        h.gripMayReverse = vr_revgrip.value != 0.f && !flatplay::active();
        glm::vec3 pos{0.f}, rot{0.f}, right{0.f};
        h.handKnown = view::drawnHandPose(hand, pos, rot);
        if(h.handKnown)
        {
            hands::angleVectors(rot, h.handForward, right, h.handUp);
        }
        h.leaveAnglesKnown = view::leavingAngles(hand, h.leaveAngles);
        slots.slot[own][hand] = h;
        if(vr_weapon_grab_hotspots.value || vr_weapon_grab_anywhere.value)
        {
            twohand::groundSpotNow(hand, slots.slotSpot[own][hand].entity, slots.slotSpot[own][hand].index);
        }
    }
    if(cls.state == ca_connected)
    {
        slots.slotMag[own] |= magholster::poll();
    }
    // Its holsters where its body has them now (QC holsterpos).
    Holsters& hs = slots.slotHolsters[own];
    hs = Holsters{};
    hs.placed = cls.state == ca_connected;
    hs.valid = hs.placed && hands::current().valid;
    static_assert(body::HolsterCount == sizeof(hs.at) / sizeof(hs.at[0]));
    for(int i = 0; i < body::HolsterCount && hs.valid; i++)
    {
        hs.at[i] = body::holsterPosition(hands::current(), static_cast<body::Holster>(i));
    }
}

void placeBots()
{
    if(!sv.active)
    {
        return;
    }
    for(int slot = 0; slot < svs.maxclients && slot < MAX_SCOREBOARD; slot++)
    {
        edict_t* const bot = server::isBot(slot) ? edictOf(slot + 1) : nullptr;
        if(!bot)
        {
            if(slots.slotBot[slot])
            {
                // (the bot gone: its slot's hands forgotten, a client coming into it sends his own)
                slots.slotBot[slot] = false;
                slots.slotHas[slot][0] = slots.slotHas[slot][1] = false;
                slots.slot[slot][0] = slots.slot[slot][1] = Hand{};
            }
            continue;
        }
        slots.slotBot[slot] = true;
        for(int hand = 0; hand < 2; hand++)
        {
            // Its weapon as every client draws it (server::botHeld: view::placeHeld at its controller), in the hand's frame
            // (the readers place it on the hand now: toWorld, the lag compensation's hand too).
            Hand h;
            view::HeldPlace p;
            glm::vec3 pos;
            glm::mat3 axes;
            handPose(bot, hand, pos, axes);
            if(server::botHeld(bot, hand, p))
            {
                const glm::mat3 toHand = glm::transpose(axes);
                h.drawn = true;
                h.mirrored = p.mirrored;
                h.slot = p.slot;
                float m[16];
                vec3_t origin{p.origin.x, p.origin.y, p.origin.z}, angles{p.angles.x, p.angles.y, p.angles.z};
                R_EntityMatrix(m, origin, angles, ENTSCALE_DEFAULT); // (as view::DrawnWeapon::inHand's)
                glm::mat4 entity;
                for(int c = 0; c < 16; c++)
                {
                    entity[c / 4][c % 4] = m[c];
                }
                glm::mat4 handMat{axes};
                handMat[3] = glm::vec4{pos, 1.f};
                h.inHand = glm::inverse(handMat) * entity;
                for(int i = 0; i < maxSpots; i++)
                {
                    h.spotType[i] = p.spotType[i];
                    h.spotPos[i] = toHand * (p.spot[i] - pos);
                    h.spotBias[i] = p.spotBias[i];
                    h.spotShare[i] = p.spotShare[i];
                }
            }
            slots.slot[slot][hand] = h;
            slots.slotHas[slot][hand] = true;
        }
    }
}

bool holsterPoint(edict_t* player, int slot, glm::vec3& out)
{
    const int s = playerSlot(player);
    if(s >= 0 && s < MAX_SCOREBOARD && slots.slotHolsters[s].placed)
    {
        const Holsters& hs = slots.slotHolsters[s];
        if(!hs.valid || slot < 0 || slot >= body::HolsterCount)
        {
            return false;
        }
        out = hs.at[slot];
        return true;
    }
    return server::holsterPoint(player, slot, out);
}

void handPose(edict_t* player, int hand, glm::vec3& pos, glm::mat3& axes)
{
    const progs::HandFields& f = progs::handFields(hand);
    pos = f.pos >= 0 ? progs::fieldVec(player, f.pos) : glm::vec3{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
    const glm::vec3 angles = f.rot >= 0 ? progs::fieldVec(player, f.rot) : glm::vec3{player->v.angles[0], player->v.angles[1], 0.f};
    axes = held::axesFromAngles(&angles[0], true);
}

glm::vec3 toWorld(edict_t* player, int hand, const glm::vec3& local)
{
    glm::vec3 pos;
    glm::mat3 axes;
    handPose(player, hand, pos, axes);
    return pos + axes * local;
}

void send()
{
    if(clientprefs::ownSlot() >= 0)
    {
        return; // (hosting: publishOwn puts this game's own into its slot)
    }
    const double now = realtime;
    for(int hand = 0; hand < 2; hand++)
    {
        if(now < up().nextSend[hand])
        {
            continue;
        }
        float v[valueCount];
        encode(measure(hand), v);
        bool moved = !up().wasSent[hand];
        for(int i = 0; i < valueCount && !moved; i++)
        {
            moved = za::abs(v[i] - up().sent[hand][i]) > (i < 3 || i == 16 || i == 22 || i == 28 || i == 34 || i == 58 || i == 62 || i == 65 ? 0.f : step);
        }
        if(!moved)
        {
            continue;
        }
        char text[1024];
        int n = q_snprintf(text, sizeof(text), "vr_heldpose %d %d", commandVersion, hand);
        for(int i = 0; i < valueCount && n > 0 && n < static_cast<int>(sizeof(text)); i++)
        {
            n += q_snprintf(text + n, sizeof(text) - static_cast<za::SizeT>(n), za::abs(v[i]) < 5e-4f ? " 0" : " %.3f", v[i]);
        }
        if(n <= 0 || n >= static_cast<int>(sizeof(text)) || !qvr::client::roomForCommand(n))
        {
            return; // (no room: the next frame)
        }
        MSG_WriteByte(&cls.message, clc_stringcmd);
        MSG_WriteString(&cls.message, text);
        ZA_MEMCPY(up().sent[hand], v, sizeof(v));
        up().wasSent[hand] = true;
        up().nextSend[hand] = now + static_cast<double>(za::max(vr_net_avatarcal_interval.value, 0.f));
    }
    // B3f: the ground spot of each empty hand when it changed, the holster presses when there are some (only with room in
    // the reliable buffer: else the next frame).
    for(int hand = 0; hand < 2; hand++)
    {
        Spot now;
        twohand::groundSpotNow(hand, now.entity, now.index);
        if(now.entity > 0)
        {
            up().spotSeen[hand] = realtime;
        }
        // A spot lost is sent a moment late (spotHold): the hand closing on the weapon leaves the spot as its move goes up,
        // and the server's touch in that move must still find it (this game's own server reads the view's spot of the
        // frame before: twohand::groundSpot).
        if((now.entity == up().spotSent[hand].entity && now.index == up().spotSent[hand].index) ||
            (now.entity == 0 && realtime - up().spotSeen[hand] < spotHold))
        {
            continue;
        }
        char text[64];
        const int n = q_snprintf(text, sizeof(text), "vr_grabspot %d %d %d", hand, now.entity, now.index);
        if(n <= 0 || !qvr::client::roomForCommand(n))
        {
            return;
        }
        MSG_WriteByte(&cls.message, clc_stringcmd);
        MSG_WriteString(&cls.message, text);
        up().spotSent[hand] = now;
    }
    up().magPending |= magholster::poll();
    if(up().magPending)
    {
        char text[32];
        const int n = q_snprintf(text, sizeof(text), "vr_magpoll %d", up().magPending);
        if(n > 0 && qvr::client::roomForCommand(n))
        {
            MSG_WriteByte(&cls.message, clc_stringcmd);
            MSG_WriteString(&cls.message, text);
            up().magPending = 0;
        }
    }
}

void clientClear()
{
    up().wasSent[0] = up().wasSent[1] = false;
    up().nextSend[0] = up().nextSend[1] = 0.0;
    up().spotSent[0] = up().spotSent[1] = Spot{};
    up().magPending = 0;
}

void serverReset(int slot)
{
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        slots.slotHas[slot][0] = slots.slotHas[slot][1] = false;
        slots.slot[slot][0] = slots.slot[slot][1] = Hand{};
        slots.slotSpot[slot][0] = slots.slotSpot[slot][1] = Spot{};
        slots.slotMag[slot] = 0;
        slots.slotHolsters[slot] = Holsters{};
        if(slot == clientprefs::ownSlot())
        {
            publishOwn(); // (this game's own: at once, as clientprefs::publishOwn)
        }
    }
}

void init()
{
    Cmd_AddCommand("vr_mp_drawn", mpDrawn_f);
    Cmd_AddCommand_ClientCommand("vr_heldpose", heldPose_f);
    Cmd_AddCommand_ClientCommand("vr_grabspot", grabSpot_f);
    Cmd_AddCommand_ClientCommand("vr_magpoll", magPoll_f);
}

} // namespace qvr::heldpose
