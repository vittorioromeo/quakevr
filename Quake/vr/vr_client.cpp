// vr_client.cpp -- client side of the Quake VR protocol extensions (see vr_protocol.hpp):
// VR input commands, building the VR move from tracking, and parsing VR server data.

#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_input.hpp"
#include "vr_main.hpp"
#include "vr_move.hpp"
#include "vr_protocol.hpp"
#include "vr_throw.hpp"
#include "vr_worldtext.hpp"

#include <vector>

using namespace qvr;
using namespace qvr::protocol;

namespace
{

// QC/vr_defs.qc QVR_VRBITS0_*.
enum VrBits0 : int
{
    VRBITS0_TELEPORTING = 1 << 0,
    VRBITS0_OFFHAND_GRABBING = 1 << 1,
    VRBITS0_OFFHAND_PREVGRABBING = 1 << 2,
    VRBITS0_MAINHAND_GRABBING = 1 << 3,
    VRBITS0_MAINHAND_PREVGRABBING = 1 << 4,
    VRBITS0_2H_AIMING = 1 << 5,
    VRBITS0_OFFHAND_RELOADING = 1 << 6,
    VRBITS0_OFFHAND_PREVRELOADING = 1 << 7,
    VRBITS0_MAINHAND_RELOADING = 1 << 8,
    VRBITS0_MAINHAND_PREVRELOADING = 1 << 9,
    VRBITS0_OFFHAND_RELOADFLICKING = 1 << 10,
    VRBITS0_OFFHAND_PREVRELOADFLICKING = 1 << 11,
    VRBITS0_MAINHAND_RELOADFLICKING = 1 << 12,
    VRBITS0_MAINHAND_PREVRELOADFLICKING = 1 << 13,
};

// ----------------------------------------------------------------------------
// Input commands: +offhandattack and the per-hand +grab/+reload/+flickreload buttons.

struct HandButtons
{
    bool grab{false};
    bool reload{false};
    bool flickReload{false};
};

HandButtons leftButtons, rightButtons;
bool offhandAttack = false;
bool offhandAttackImpulse = false; // pressed and released between two moves

// Which physical hand is which depends on vr_lefthanded.
[[nodiscard]] const HandButtons& handButtons(int hand)
{
    const bool leftIsOffHand = !vr_lefthanded.value;
    return (hand == HAND_OFF) == leftIsOffHand ? leftButtons : rightButtons;
}

#define QVR_BUTTON_COMMANDS(name, expr)          \
    void name##Down_f()                          \
    {                                            \
        expr = true;                             \
    }                                            \
    void name##Up_f()                            \
    {                                            \
        expr = false;                            \
    }

QVR_BUTTON_COMMANDS(GrabLeft, leftButtons.grab)
QVR_BUTTON_COMMANDS(GrabRight, rightButtons.grab)
QVR_BUTTON_COMMANDS(ReloadLeft, leftButtons.reload)
QVR_BUTTON_COMMANDS(ReloadRight, rightButtons.reload)
QVR_BUTTON_COMMANDS(FlickReloadLeft, leftButtons.flickReload)
QVR_BUTTON_COMMANDS(FlickReloadRight, rightButtons.flickReload)

#undef QVR_BUTTON_COMMANDS

void OffhandAttackDown_f()
{
    offhandAttack = true;
    offhandAttackImpulse = true;
}

void OffhandAttackUp_f()
{
    offhandAttack = false;
}

// ----------------------------------------------------------------------------
// Move

bool wasGrabbing[2]{false, false};

[[nodiscard]] VrMove buildMove()
{
    VrMove move;

    const hands::State& hs = hands::current();
    if(!hs.valid)
    {
        return move;
    }

    move.headAngles = hs.headAngles;

    for(int h = 0; h < HAND_COUNT; h++)
    {
        VrHandMove& hand = move.hands[h];
        hand.pos = hs.pos[h];
        hand.rot = hs.rot[h];

        // Every move carries the throw estimate: the server throws with the one of the move
        // that lets go.
        const throwing::Estimate thrown = throwing::estimate(h);
        hand.vel = hs.vel[h];
        hand.velMag = glm::length(hs.vel[h]);
        hand.throwVel = thrown.vel;
        hand.angVel = thrown.angVel;

        const bool grabbing = handButtons(h).grab;
        if(wasGrabbing[h] && !grabbing && vr_debug_throw.value)
        {
            Con_Printf("throw %s: %.2f m/s (%.2f %.2f %.2f), spin %.1f rad/s, hand %.2f m/s\n",
                h == HAND_MAIN ? "main" : "off", glm::length(thrown.vel), thrown.vel.x, thrown.vel.y,
                thrown.vel.z, glm::length(thrown.angVel), hand.velMag);
        }
        wasGrabbing[h] = grabbing;

        // Muzzles come from the weapon models (vr_view.cpp), as of the last rendered frame.
        move.muzzlePos[h] =
            hs.muzzleValid[h] ? hs.muzzle[h] : hand.pos + hands::forward(hand.rot) * 8.f;
    }

    move.headVel = hs.headVel;

    // VR bits: current state only. The server fills in the "PREV" bits once per server
    // frame, so that press edges survive several moves arriving in one frame.
    int bits = 0;
    const auto set = [&](bool on, int bit) {
        if(on)
        {
            bits |= bit;
        }
    };
    set(handButtons(HAND_OFF).grab, VRBITS0_OFFHAND_GRABBING);
    set(handButtons(HAND_MAIN).grab, VRBITS0_MAINHAND_GRABBING);
    set(handButtons(HAND_OFF).reload, VRBITS0_OFFHAND_RELOADING);
    set(handButtons(HAND_MAIN).reload, VRBITS0_MAINHAND_RELOADING);
    set(handButtons(HAND_OFF).flickReload, VRBITS0_OFFHAND_RELOADFLICKING);
    set(handButtons(HAND_MAIN).flickReload, VRBITS0_MAINHAND_RELOADFLICKING);
    move.vrBits0 = static_cast<std::uint16_t>(bits);

    if(offhandAttack || offhandAttackImpulse)
    {
        move.buttons |= QVR_BUTTON_OFFHANDATTACK;
    }
    offhandAttackImpulse = false;

    if(vrActive())
    {
        move.buttons |= QVR_BUTTON_HANDSTRACKED;
    }

    return move;
}

// ----------------------------------------------------------------------------
// Per-entity VR data

std::vector<client::EntityVr> entityData;

[[nodiscard]] bool vrProtocol()
{
    return (cl.protocolflags & PRFL_QUAKEVR) != 0;
}

[[nodiscard]] glm::vec3 readFloats3()
{
    glm::vec3 v;
    for(int i = 0; i < 3; i++)
    {
        v[i] = MSG_ReadFloat();
    }
    return v;
}

[[nodiscard]] glm::vec3 readCoords3()
{
    glm::vec3 v;
    for(int i = 0; i < 3; i++)
    {
        v[i] = MSG_ReadCoord(cl.protocolflags);
    }
    return v;
}

void parseParticle2()
{
    vec3_t org, dir;
    for(int i = 0; i < 3; i++)
    {
        org[i] = MSG_ReadCoord(cl.protocolflags);
    }
    for(int i = 0; i < 3; i++)
    {
        dir[i] = MSG_ReadChar() * (1.f / 16.f);
    }
    const int preset = MSG_ReadByte();
    const int count = MSG_ReadShort();

    // TODO VR: (P7) particle presets; blood-coloured puffs until then.
    (void)preset;
    R_RunParticleEffect(org, dir, 73, q_min(count, 255));
}

void parsePrecacheModel()
{
    const int index = MSG_ReadShort();
    const char* name = MSG_ReadString();

    if(index <= 0 || index >= MAX_MODELS)
    {
        Host_Error("svc_quakevr: bad model precache index %d", index);
    }

    cl.model_precache[index] = Mod_ForName(name, false);
}

void parsePrecacheSound()
{
    const int index = MSG_ReadShort();
    const char* name = MSG_ReadString();

    if(index <= 0 || index >= MAX_SOUNDS)
    {
        Host_Error("svc_quakevr: bad sound precache index %d", index);
    }

    cl.sound_precache[index] = S_PrecacheSound(name);
}

} // namespace

namespace qvr::client
{

void init()
{
    Cmd_AddCommand("+offhandattack", OffhandAttackDown_f);
    Cmd_AddCommand("-offhandattack", OffhandAttackUp_f);
    Cmd_AddCommand("+grableft", GrabLeftDown_f);
    Cmd_AddCommand("-grableft", GrabLeftUp_f);
    Cmd_AddCommand("+grabright", GrabRightDown_f);
    Cmd_AddCommand("-grabright", GrabRightUp_f);
    Cmd_AddCommand("+reloadleft", ReloadLeftDown_f);
    Cmd_AddCommand("-reloadleft", ReloadLeftUp_f);
    Cmd_AddCommand("+reloadright", ReloadRightDown_f);
    Cmd_AddCommand("-reloadright", ReloadRightUp_f);
    Cmd_AddCommand("+flickreloadleft", FlickReloadLeftDown_f);
    Cmd_AddCommand("-flickreloadleft", FlickReloadLeftUp_f);
    Cmd_AddCommand("+flickreloadright", FlickReloadRightDown_f);
    Cmd_AddCommand("-flickreloadright", FlickReloadRightUp_f);
}

const EntityVr* entityVr(int num)
{
    if(!vrProtocol() || num < 0 || num >= static_cast<int>(entityData.size()))
    {
        return nullptr;
    }

    return &entityData[num];
}

} // namespace qvr::client

extern "C" void VR_OnSetAngle(float yaw)
{
    hands::setServerYaw(yaw);
}

extern "C" void VR_OnClientClearState()
{
    entityData.clear();
    worldtext::clientReset();
    throwing::reset();
}

extern "C" void VR_WriteMoveExtras(sizebuf_t* buf)
{
    if(vrProtocol())
    {
        writeVrMove(buf, buildMove());
    }
}

extern "C" void VR_ParseEntityUpdate(int num, int bits)
{
    if(!vrProtocol())
    {
        return;
    }

    if(num >= static_cast<int>(entityData.size()))
    {
        entityData.resize(num + 1);
    }

    // Absent bits mean "zero": the server never puts these in baselines.
    client::EntityVr& data = entityData[num];
    data.scale = (bits & U_QVR_SCALE) ? readFloats3() : glm::vec3{0.f};
    data.scaleOrigin = (bits & U_QVR_SCALEORIGIN) ? readCoords3() : glm::vec3{0.f};
    data.offset = (bits & U_QVR_OFFSET) ? readCoords3() : glm::vec3{0.f};
}

extern "C" int VR_ParseServerMessage(int cmd)
{
    if(cmd != svc_quakevr || !vrProtocol())
    {
        return 0;
    }

    const int subcmd = MSG_ReadByte();
    switch(subcmd)
    {
        case QVR_SVC_PARTICLE2: parseParticle2(); break;
        case QVR_SVC_PRECACHE_MODEL: parsePrecacheModel(); break;
        case QVR_SVC_PRECACHE_SOUND: parsePrecacheSound(); break;
        case QVR_SVC_HAPTIC: VR_ParseHaptic(); break;
        case QVR_SVC_WORLDTEXT_MAKE:
        case QVR_SVC_WORLDTEXT_TEXT:
        case QVR_SVC_WORLDTEXT_POS:
        case QVR_SVC_WORLDTEXT_ANGLES:
        case QVR_SVC_WORLDTEXT_HALIGN:
        case QVR_SVC_WORLDTEXT_SCALE: worldtext::clientParse(subcmd); break;
        default: Host_Error("svc_quakevr: unknown command %d", subcmd);
    }

    return 1;
}

extern "C" int VR_ParseBeamEntity(int ent)
{
    if(!vrProtocol())
    {
        return ent;
    }

    // The QC sends a beam id so that one entity (dual-wielded lightning guns, grapple and
    // weapon) can own several beams. Folding it into the key also stops Ironwail snapping
    // the beam start to the player origin: VR beams start at the weapon muzzle.
    const int beamId = MSG_ReadByte();
    return ent | ((beamId + 1) << 16);
}
