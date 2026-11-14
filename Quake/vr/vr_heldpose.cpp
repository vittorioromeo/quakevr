// vr_heldpose.cpp -- see vr_heldpose.hpp.

#include "vr_heldpose.hpp"

#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_held.hpp"
#include "vr_progs.hpp"
#include "vr_twohand.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <string.h>

namespace qvr::heldpose
{
namespace
{

// "vr_heldpose <version> <hand> <valueCount values>" (encode's order); another version's or count's is ignored.
constexpr int commandVersion = 1;
constexpr int valueCount = 62;
constexpr float step = 0.05f; // (units, and the directions' parts as much: a change smaller goes up with the next one)
constexpr float most = 256.f; // (a value farther than this: not a hand's, the command ignored)

enum Flag : int
{
    flagDrawn = 1,
    flagMirrored = 2,
    flagCarrying = 4,
    flagAxes = 8,
    flagMayReverse = 16,
    flagFlippable = 32
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
                              (h.axesKnown ? flagAxes : 0) | (h.mayReverse ? flagMayReverse : 0) | (h.flippable ? flagFlippable : 0));
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
    h.slot = static_cast<int>(v[1]);
    h.helpKind = static_cast<int>(v[2]);
    h.flip = v[3];
    for(int c = 0; c < 4; c++)
    {
        h.inHand[c] = glm::vec4{get3(v, 4 + c * 3), c == 3 ? 1.f : 0.f};
    }
    for(int i = 0; i < maxSpots; i++)
    {
        h.spotType[i] = static_cast<int>(v[16 + i * 6]);
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
    return h;
}

struct State
{
    // Client: what went up last, each hand's.
    float sent[2][valueCount]{};
    bool wasSent[2]{};
    double nextSend[2]{};
    // Server: each slot's.
    Hand slot[MAX_SCOREBOARD][2];
    bool slotHas[MAX_SCOREBOARD][2]{};
};
State state;

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
    if(d.model && vr_gametime - d.when <= 0.5 && view::drawnWeaponPlace(hand, d.model, origin, angles))
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
        havePose = true;
        h.drawn = true;
        h.mirrored = d.mirrored;
        h.slot = weapons::slotForModel(d.model);
        h.inHand = d.inHand;
    }
    const hands::State& s = hands::current();
    if(!havePose && s.valid)
    {
        handPoseDrawn = glm::mat4{held::axesFromAngles(&s.rot[hand][0], true)};
        handPoseDrawn[3] = glm::vec4{s.pos[hand], 1.f};
        havePose = true;
    }
    if(!havePose)
    {
        return h;
    }
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
    h.helpKind = twohand::helpKind(hand);
    glm::vec3 pos, rot, tip;
    bool mirrored = false;
    if(twohand::carrying(hand) && view::carriedWeaponPose(hand, pos, rot, mirrored, tip) &&
        view::carriedWeaponInHand(hand, h.carriedHandle, h.carriedTip))
    {
        h.carrying = true;
        h.carriedAxes = glm::mat3{toHand} * held::axesFromAngles(&rot[0], true);
    }
    h.axesKnown = view::heldWeaponAxes(hand, h.axesForward, h.axesUp, h.flip);
    h.mayReverse = view::heldMayReverse(hand);
    h.flippable = view::heldGripFlippable(hand);
    const int other = held::heldAlone(1 - hand);
    if(other > 0)
    {
        h.outOf = view::heldOutOf(hand, other);
        h.outOfEnt = h.outOf != glm::vec3{0.f} ? other : 0;
    }
    return h;
}

[[nodiscard]] int slotOf(const edict_t* player)
{
    if(!player || !sv.active || !sv.qcvm.edicts)
    {
        return -1;
    }
    const int num = static_cast<int>(((const byte*)player - (const byte*)sv.qcvm.edicts) / sv.qcvm.edict_size);
    return num - 1;
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
    state.slot[slot][hand] = decode(v);
    state.slotHas[slot][hand] = true;
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
    Con_Printf("\n");
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
        }
    }
    if(!sv.active)
    {
        return;
    }
    for(int slot = 0; slot < svs.maxclients && slot < MAX_SCOREBOARD; slot++)
    {
        if(!svs.clients[slot].active)
        {
            continue;
        }
        edict_t* player = (edict_t*)((byte*)sv.qcvm.edicts + (slot + 1) * sv.qcvm.edict_size);
        for(int hand = 0; hand < 2; hand++)
        {
            const Hand* r = remote(player, hand);
            if(!r)
            {
                continue;
            }
            glm::vec3 spots[maxSpots];
            for(int i = 0; i < maxSpots; i++)
            {
                spots[i] = toWorld(player, hand, r->spotPos[i]);
            }
            printHand("mp_drawn_sv", slot + 1, hand, *r, spots);
        }
    }
}

} // namespace

bool isLocal(const edict_t* player)
{
    return cls.state != ca_dedicated && slotOf(player) == 0;
}

const Hand* remote(const edict_t* player, int hand)
{
    const int slot = slotOf(player);
    if(svs.maxclients <= 1 || slot < 0 || slot >= MAX_SCOREBOARD || (hand != 0 && hand != 1) || isLocal(player) ||
        !state.slotHas[slot][hand])
    {
        return nullptr;
    }
    return &state.slot[slot][hand];
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
    const double now = realtime;
    for(int hand = 0; hand < 2; hand++)
    {
        if(now < state.nextSend[hand])
        {
            continue;
        }
        float v[valueCount];
        encode(measure(hand), v);
        bool moved = !state.wasSent[hand];
        for(int i = 0; i < valueCount && !moved; i++)
        {
            moved = za::abs(v[i] - state.sent[hand][i]) > (i < 3 || i == 16 || i == 22 || i == 28 || i == 34 || i == 58 ? 0.f : step);
        }
        if(!moved)
        {
            continue;
        }
        char text[1024];
        int n = q_snprintf(text, sizeof(text), "vr_heldpose %d %d", commandVersion, hand);
        for(int i = 0; i < valueCount && n > 0 && n < static_cast<int>(sizeof(text)); i++)
        {
            n += q_snprintf(text + n, sizeof(text) - static_cast<size_t>(n), za::abs(v[i]) < 5e-4f ? " 0" : " %.3f", v[i]);
        }
        if(n <= 0 || n >= static_cast<int>(sizeof(text)) || cls.message.cursize + n + 2 > cls.message.maxsize)
        {
            return; // (no room: the next frame)
        }
        MSG_WriteByte(&cls.message, clc_stringcmd);
        MSG_WriteString(&cls.message, text);
        memcpy(state.sent[hand], v, sizeof(v));
        state.wasSent[hand] = true;
        state.nextSend[hand] = now + static_cast<double>(za::max(vr_net_avatarcal_interval.value, 0.f));
    }
}

void clientClear()
{
    state.wasSent[0] = state.wasSent[1] = false;
    state.nextSend[0] = state.nextSend[1] = 0.0;
}

void serverReset(int slot)
{
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        state.slotHas[slot][0] = state.slotHas[slot][1] = false;
        state.slot[slot][0] = state.slot[slot][1] = Hand{};
    }
}

void init()
{
    Cmd_AddCommand("vr_mp_drawn", mpDrawn_f);
    Cmd_AddCommand_ClientCommand("vr_heldpose", heldPose_f);
}

} // namespace qvr::heldpose
