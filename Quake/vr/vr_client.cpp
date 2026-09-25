// vr_client.cpp -- client side of the Quake VR protocol extensions (see vr_protocol.hpp):
// VR input commands, building the VR move from tracking, and parsing VR server data.

#include "vr_client.hpp"
#include "vr_decals.hpp"
#include "vr_engine.hpp"
#include "vr_particles.hpp"
#include "vr_cvars.hpp"
#include "vr_flick.hpp"
#include "vr_handpose.hpp"
#include "vr_hands.hpp"
#include "vr_input.hpp"
#include "vr_lighting.hpp"
#include "vr_main.hpp"
#include "vr_move.hpp"
#include "vr_protocol.hpp"
#include "vr_teleport.hpp"
#include "vr_throw.hpp"
#include "vr_twohand.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"
#include "vr_worldtext.hpp"

#include <algorithm>
#include <cstring>
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
[[nodiscard]] HandButtons& handButtons(int hand)
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

// By role rather than side: what the controller keys are bound to (quakevr/default.cfg).
QVR_BUTTON_COMMANDS(GrabMain, handButtons(HAND_MAIN).grab)
QVR_BUTTON_COMMANDS(GrabOff, handButtons(HAND_OFF).grab)
QVR_BUTTON_COMMANDS(ReloadMain, handButtons(HAND_MAIN).reload)
QVR_BUTTON_COMMANDS(ReloadOff, handButtons(HAND_OFF).reload)

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

// The throw estimate taken when each hand let go.
bool thrownValid[2]{false, false};
throwing::Estimate thrown[2];

[[nodiscard]] VrMove buildMove()
{
    VrMove move;

    const hands::State& hs = hands::current();
    if(!hs.valid)
    {
        return move;
    }

    move.headAngles = hs.headAngles;
    move.vrYaw = hands::playSpaceYaw();
    move.origin = hs.playerOrigin;

    // The server walks the player by this over its frame (units per second).
    if(host_frametime > 0.0)
    {
        move.roomscaleMove = hands::takeRoomscaleMove() / static_cast<float>(host_frametime);
    }

    for(int h = 0; h < HAND_COUNT; h++)
    {
        VrHandMove& hand = move.hands[h];
        hand.pos = hs.pos[h];
        hand.rot = hs.rot[h];

        // Every move carries a throw estimate: while grabbing, as if let go now (the helping
        // hand of a two-handed throw); once let go, the one taken at the release, until the next
        // grab (the server throws with the move that carries the release, or a later one).
        const bool grabbing = handButtons(h).grab;
        const double latest = throwing::latestTime(h);
        if(wasGrabbing[h] && !grabbing)
        {
            // The grip's own release time, if it is this release (not a key's, nor a stale one).
            const double released = throwing::releaseTime(h);
            const double at =
                released >= 0.0 && latest - released >= -0.05 && latest - released < 0.25 ? released : latest;
            thrown[h] = throwing::estimateAt(h, at);
            thrownValid[h] = true;

            if(vr_debug_throw.value)
            {
                const throwing::Estimate& e = thrown[h];
                Con_Printf("throw %s: %.2f m/s (%.2f %.2f %.2f), spin %.1f rad/s, hand now %.2f m/s\n",
                    h == HAND_MAIN ? "main" : "off", glm::length(e.vel), e.vel.x, e.vel.y, e.vel.z,
                    glm::length(e.angVel), glm::length(hs.vel[h]));
                if(vr_debug_throw.value >= 2.f)
                {
                    Con_Printf("  released %.0f ms after the peak, sent %.0f ms after the release\n",
                        (at - e.time) * 1000.0, (latest - at) * 1000.0);
                }
            }
        }
        if(grabbing)
        {
            thrownValid[h] = false;
        }
        wasGrabbing[h] = grabbing;

        const throwing::Estimate e = thrownValid[h] ? thrown[h] : throwing::estimate(h);
        hand.vel = hs.vel[h];
        hand.velMag = glm::length(hs.vel[h]);
        hand.throwVel = e.vel;
        hand.angVel = e.angVel;
        hand.throwPos = e.pos;
        hand.throwAge = static_cast<float>(std::max(0.0, latest - e.time));

        move.hotspots[h] = static_cast<std::uint8_t>(hs.hotspot[h]);

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
    set(handButtons(HAND_OFF).flickReload || flick::flicking(HAND_OFF), VRBITS0_OFFHAND_RELOADFLICKING);
    set(handButtons(HAND_MAIN).flickReload || flick::flicking(HAND_MAIN), VRBITS0_MAINHAND_RELOADFLICKING);
    set(twohand::aiming(), VRBITS0_2H_AIMING);
    set(teleport::update(hs, move.teleportTarget), VRBITS0_TELEPORTING);
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
    using particles::Preset;
    const auto preset = static_cast<Preset>(MSG_ReadByte());
    const int count = MSG_ReadShort();

    const glm::vec3 o{org[0], org[1], org[2]};
    const glm::vec3 d{dir[0], dir[1], dir[2]};
    decals::fromEffect(o, d, preset, count);
    if(particles::spawn(o, d, preset, count))
    {
        return;
    }

    // Without Quake VR's particles (vr_particles 0): the old engine's particle presets,
    // approximated with Quake's own effects and palette colours.
    const auto puff = [&](int color, int n) { R_RunParticleEffect(org, dir, color, q_min(n, 255)); };
    switch(preset)
    {
        case Preset::BulletPuff: puff(0, count); break;
        case Preset::Blood: puff(73, count); break;
        case Preset::Explosion: R_ParticleExplosion(org); break;
        case Preset::Lightning: puff(225, count); break;
        case Preset::Smoke: puff(6, count); break;
        case Preset::Sparks: puff(111, count); break;
        case Preset::GunSmoke: puff(4, q_max(count / 2, 1)); break;
        case Preset::Teleport: R_TeleportSplash(org); break;
        case Preset::GunPickup: puff(254, count); break;
        case Preset::GunForceGrab: puff(208, count); break;
        case Preset::LavaSpike: puff(235, count); break;
        case Preset::BigSmoke: puff(6, count * 2); break;
        default: puff(73, count); break;
    }
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

// vr_particle_test <preset> [count]: a particle2 preset 64 units in front of the view (tuning).
void particleTest_f()
{
    if(Cmd_Argc() < 2 || cls.state != ca_connected)
    {
        Con_Printf("usage: vr_particle_test <preset 0..11> [count]\n");
        return;
    }
    vec3_t fwd, right, up;
    AngleVectors(r_refdef.viewangles, fwd, right, up);
    const glm::vec3 org = glm::vec3{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]} +
                          glm::vec3{fwd[0], fwd[1], fwd[2]} * 64.f;
    const int count = Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) : 8;
    if(!particles::spawn(org, glm::vec3{0.f}, static_cast<particles::Preset>(Q_atoi(Cmd_Argv(1))), count))
    {
        Con_Printf("vr_particle_test: Quake VR particles are off (vr_particles) or unavailable\n");
    }
}

} // namespace

namespace qvr::client
{

void init()
{
    teleport::init();
    Cmd_AddCommand("vr_particle_test", particleTest_f);
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
    Cmd_AddCommand("+grabmain", GrabMainDown_f);
    Cmd_AddCommand("-grabmain", GrabMainUp_f);
    Cmd_AddCommand("+graboff", GrabOffDown_f);
    Cmd_AddCommand("-graboff", GrabOffUp_f);
    Cmd_AddCommand("+reloadmain", ReloadMainDown_f);
    Cmd_AddCommand("-reloadmain", ReloadMainUp_f);
    Cmd_AddCommand("+reloadoff", ReloadOffDown_f);
    Cmd_AddCommand("-reloadoff", ReloadOffUp_f);
}

bool grabbing(int hand)
{
    return handButtons(hand).grab;
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

// The old engine drew the lightning gun's beam at half size and the grappling hook's at a
// quarter, which suits VR's scale better.
extern "C" float VR_BeamScale(qmodel_t* model)
{
    if(!vrProtocol() || !model)
    {
        return 1.f;
    }
    if(!strcmp(model->name, "progs/bolt2.mdl"))
    {
        return 0.5f;
    }
    if(!strcmp(model->name, "progs/beam.mdl"))
    {
        return 0.25f;
    }
    return 1.f;
}

// CL_Record_f, recording mid-game: the demo needs the VR state received so far.
extern "C" void VR_WriteDemoState(sizebuf_t* msg)
{
    if(vrProtocol())
    {
        worldtext::clientWriteAll(msg);
    }
}

extern "C" void VR_OnClientClearState()
{
    entityData.clear();
    particles::clear();
    decals::clear();
    worldtext::clientReset();
    throwing::reset();
    thrownValid[0] = thrownValid[1] = false;
    twohand::reset();
    flick::reset();
    handpose::reset();
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
    data.noRotate = (bits & U_QVR_NOROTATE) != 0;
}

extern "C" int VR_SuppressModelRotate(int num)
{
    return vrProtocol() && num >= 0 && num < static_cast<int>(entityData.size()) && entityData[num].noRotate;
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
        case QVR_SVC_HAPTIC: input::parseHaptic(); break;
        case QVR_SVC_HANDIMPACT: view::parseHandImpact(); break;
        case QVR_SVC_WORLDTEXT_MAKE:
        case QVR_SVC_WORLDTEXT_TEXT:
        case QVR_SVC_WORLDTEXT_POS:
        case QVR_SVC_WORLDTEXT_ANGLES:
        case QVR_SVC_WORLDTEXT_HALIGN:
        case QVR_SVC_WORLDTEXT_SCALE: worldtext::clientParse(subcmd); break;
        case QVR_SVC_FLOATTEXT: worldtext::clientParseFloatText(); break;
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
    if(!(cl.protocolflags & PRFL_QUAKEVR_PROGS))
    {
        // Another mod's progs (compatibility mode) send no beam id; still keep Ironwail from
        // snapping the player's beam to the view: it starts at the gun (vr_compat_muzzle), the
        // main hand's (beam id 1).
        return ent | (2 << 16);
    }

    // The QC sends a beam id so that one entity (dual-wielded lightning guns, grapple and
    // weapon) can own several beams. Folding it into the key also stops Ironwail snapping
    // the beam start to the player origin: VR beams start at the weapon muzzle.
    const int beamId = MSG_ReadByte();
    return ent | ((beamId + 1) << 16);
}

// Dynamic lights that make shooting light up a room (vr_flash_scale, vr_explosion_light_scale,
// vr_colored_lights): muzzle flashes bigger, fading out rather than blinking off, coloured by the
// weapon (the lightning gun's blue, the nailguns' orange, ...); rockets and explosions warm. The
// local player's muzzle flash lights up the gun (the main hand's, or the off hand's when the main
// hand has none), not a point in front of the player's origin.
// With DarkPlaces' falloff (vr_dlight_falloff) they are DarkPlaces' lights: brighter than 1 (the
// shaders' falloff keeps them full out to about 40% of the radius), smaller; an explosion lights
// what faces away from it a little, and shrinks at 700 units a second; a muzzle flash is white and
// fades out in a twentieth of a second (vr_colored_lights keeps the colours, as bright).
extern "C" void VR_TuneDlight(int kind, int ent, void* dlight)
{
    dlight_t* dl = static_cast<dlight_t*>(dlight);
    const bool colored = vr_colored_lights.value != 0.f;
    const bool darkplaces = vr_dlight_falloff.value != 0.f;
    float brightness = 1.f; // a muzzle flash is dimmer than an explosion: it reaches far, not glaring
    const auto color = [&](float r, float g, float b) {
        if(colored)
        {
            dl->color[0] = r * brightness;
            dl->color[1] = g * brightness;
            dl->color[2] = b * brightness;
        }
    };
    // DarkPlaces' colour; without colours, as bright in white.
    const auto dpColor = [&](float r, float g, float b) {
        const float grey = (r + g + b) / 3.f;
        dl->color[0] = colored ? r : grey;
        dl->color[1] = colored ? g : grey;
        dl->color[2] = colored ? b : grey;
    };

    if(kind == QVR_DLIGHT_ROCKET)
    {
        if(darkplaces)
        {
            dl->radius = 200.f * std::max(0.f, vr_explosion_light_scale.value);
            dpColor(3.f, 1.5f, 0.5f);
            lighting::dlightLook(dl, 0.f, 0.f);
            return;
        }
        dl->radius *= std::max(0.f, vr_explosion_light_scale.value) * 0.85f;
        color(1.f, 0.65f, 0.35f);
        return;
    }
    if(kind == QVR_DLIGHT_EXPLOSION)
    {
        const float k = std::max(0.f, vr_explosion_light_scale.value);
        if(darkplaces)
        {
            dl->radius = 350.f * k;
            dl->decay = 700.f * k;
            dl->die = static_cast<float>(cl.time + 0.5);
            dpColor(4.f, 2.f, 0.5f);
            lighting::dlightLook(dl, 0.25f, 0.f);
            return;
        }
        dl->radius *= k;
        dl->decay *= k;
        color(1.f, 0.72f, 0.42f);
        return;
    }

    if(darkplaces)
    {
        // DarkPlaces' muzzle flash: white, four times full light, gone in a twentieth of a second.
        dl->radius = 150.f * std::max(0.f, vr_flash_scale.value);
        dl->die = static_cast<float>(cl.time + 0.05);
        dl->decay = 0.f;
        brightness = 4.f;
        dl->color[0] = dl->color[1] = dl->color[2] = brightness;
        lighting::dlightLook(dl, 0.f, 0.05f);
    }
    else
    {
        // A muzzle flash: bright at once, gone in about a tenth of a second.
        dl->radius *= std::max(0.f, vr_flash_scale.value);
        dl->die = cl.time + 0.12;
        dl->decay = dl->radius * 6.f;
        brightness = 0.75f;
    }
    color(1.f, 0.85f, 0.6f);

    if(!vrProtocol() || ent != cl.viewentity)
    {
        return;
    }
    const hands::State& s = hands::current();
    const int hand = s.muzzleValid[1] ? 1 : s.muzzleValid[0] ? 0 : -1;
    if(hand < 0)
    {
        return;
    }
    // A little back from the muzzle, so that it is not inside the wall the gun touches.
    const glm::vec3 p = s.muzzle[hand] - hands::forward(s.rot[hand]) * 4.f;
    dl->origin[0] = p.x;
    dl->origin[1] = p.y;
    dl->origin[2] = p.z;

    // The weapon's colour.
    const qmodel_t* model = weapons::heldModel(hand);
    const char* n = model ? model->name : "";
    if(strstr(n, "v_light"))
    {
        color(0.55f, 0.7f, 1.f);
    }
    else if(strstr(n, "v_plasma"))
    {
        color(0.6f, 0.8f, 1.f);
    }
    else if(strstr(n, "v_laserg"))
    {
        color(1.f, 0.35f, 0.3f);
    }
    else if(strstr(n, "v_nail") || strstr(n, "v_lava"))
    {
        color(1.f, 0.65f, 0.35f);
    }
    else if(strstr(n, "v_rock") || strstr(n, "v_prox") || strstr(n, "v_multi"))
    {
        color(1.f, 0.55f, 0.3f);
    }
}

// The player's own beams follow the gun as drawn, every frame, rather than where the server last
// saw it (a few frames late, and stepping at the server's rate). Beam ids 0 and 1 (the off and
// main hands' lightning) start at that hand's muzzle and aim along the hand, keeping their length;
// 2 and 3 (the grappling hook's rope) start there and end at the hook, where it is drawn.
extern "C" int VR_UpdateBeam(int ent, float* start, float* end)
{
    const int id = (ent >> 16) - 1;
    if(!vrProtocol() || id < 0 || id > 3 || (ent & 0xFFFF) != cl.viewentity)
    {
        return 0;
    }

    const int hand = id & 1;
    const hands::State& s = hands::current();
    if(s.muzzleValid[hand])
    {
        const glm::vec3 muzzle = s.muzzle[hand];
        if(id < 2)
        {
            const float len = glm::distance(glm::vec3{start[0], start[1], start[2]}, glm::vec3{end[0], end[1], end[2]});
            const glm::vec3 e = muzzle + hands::forward(s.rot[hand]) * len;
            end[0] = e.x;
            end[1] = e.y;
            end[2] = e.z;
        }
        start[0] = muzzle.x;
        start[1] = muzzle.y;
        start[2] = muzzle.z;
    }
    if(id < 2)
    {
        return 0;
    }

    // The rope's end: the hook as drawn (interpolated) rather than as last sent, the nearest hook
    // to where the server put the end.
    const entity_t* hook = nullptr;
    float best = 48.f;
    for(int i = 1; i < cl.num_entities; i++)
    {
        const entity_t& e = cl_entities[i];
        if(!e.model || e.msgtime != cl.mtime[0] || strcmp(e.model->name, "progs/hook.mdl"))
        {
            continue;
        }
        const float d = glm::distance(glm::vec3{e.msg_origins[0][0], e.msg_origins[0][1], e.msg_origins[0][2]},
            glm::vec3{end[0], end[1], end[2]});
        if(d < best)
        {
            best = d;
            hook = &e;
        }
    }
    if(hook)
    {
        for(int k = 0; k < 3; k++)
        {
            end[k] += hook->origin[k] - hook->msg_origins[0][k];
        }
    }
    return 1;
}
