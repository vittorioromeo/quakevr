// vr_fatigue.cpp -- see vr_fatigue.hpp.

#include "vr_fatigue.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_meleehud.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_units.hpp"
#include "vr_weight.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/Hypot.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"

#include <glm/gtc/constants.hpp>


namespace qvr::fatigue
{
namespace
{

using namespace progs;

constexpr float easeTime = 0.3f; // s the shake takes to come and go (a hold taken or let go, the stamina's whole percent)
constexpr float growth = 1.5f;   // the shake's curve from the threshold to none: gentle at first

float level = 0.f;
double levelAt = -1.0;
double printedAt = -1.0;
double speedPrintedAt = -1.0; // vr_debug_stamina_speed
float heldAt = -1.f; // vr_debug_stamina_hold: the share the game's stamina is kept at (-1: not yet taken)

[[nodiscard]] bool climbing()
{
    return (cl.protocolflags & PRFL_QUAKEVR) && cls.state == ca_connected && (cl.stats[protocol::STAT_QVR_CLIMB] & 3) != 0;
}

// Smooth noise, -1..1: three sines of a tremor's quickness (5 to 13 Hz), each axis and hand its own.
[[nodiscard]] float tremor(float t, int hand, int axis)
{
    constexpr float freq[3]{5.3f, 8.7f, 12.9f};
    constexpr float share[3]{0.55f, 0.3f, 0.15f};
    float n = 0.f;
    for(int k = 0; k < 3; k++)
    {
        const float f = freq[k] * (1.f + 0.061f * static_cast<float>(axis) + 0.113f * static_cast<float>(hand));
        const float phase = 2.1f * static_cast<float>(hand) + 1.37f * static_cast<float>(axis) + 0.71f * static_cast<float>(k);
        n += share[k] * za::sin(glm::two_pi<float>() * f * t + phase);
    }
    return n;
}

// The local server's player, its stamina's fields (the VR progs), or null.
[[nodiscard]] edict_t* player()
{
    if(!sv.active || svs.maxclients < 1 || !svs.clients[0].active || !svs.clients[0].edict || !bindings().isVrProgs)
    {
        return nullptr;
    }
    const FieldOffsets& f = fields();
    return f.vr_stamina_used >= 0 && f.vr_stamina_time >= 0 ? svs.clients[0].edict : nullptr;
}

[[nodiscard]] float staminaMax()
{
    return za::max(1.f, vr_parry_stamina_max.value);
}

// The game's stamina to `share` (0..1), none coming back for vr_parry_stamina_delay. On the server's VM.
void setStamina(edict_t* ent, float share)
{
    const FieldOffsets& f = fields();
    fieldFloat(ent, f.vr_stamina_used) = staminaMax() * (1.f - za::clamp(share, 0.f, 1.f));
    fieldFloat(ent, f.vr_stamina_time) = static_cast<float>(qcvm->time);
}

// vr_stamina_set <0..1>: the game's stamina at that share (0: none, 1: rested); with vr_debug_stamina_hold, kept there.
void set_f()
{
    qcvm_t* oldvm = nullptr;
    if(!sv.active)
    {
        Con_Printf("vr_stamina_set: no local game\n");
        return;
    }
    PR_PushQCVM(&sv.qcvm, &oldvm);
    if(edict_t* ent = player())
    {
        const float before = za::clamp(1.f - fieldFloat(ent, fields().vr_stamina_used) / staminaMax(), 0.f, 1.f);
        if(Cmd_Argc() < 2)
        {
            Con_Printf("vr_stamina_set <0..1>: the stamina now %.2f\n", before);
        }
        else
        {
            const float share = za::clamp(Q_atof(Cmd_Argv(1)), 0.f, 1.f);
            setStamina(ent, share);
            heldAt = share;
            Con_Printf("stamina: %.2f (was %.2f)%s\n", share, before, vr_debug_stamina_hold.value ? ", held" : "");
        }
    }
    else
    {
        Con_Printf("vr_stamina_set: no Quake VR progs\n");
    }
    PR_PopQCVM(oldvm);
}

} // namespace

float staminaLeft()
{
    if(vr_debug_weight_stamina.value >= 0.f)
    {
        return za::min(vr_debug_weight_stamina.value, 1.f);
    }
    const meleehud::State st = meleehud::state();
    return st.stamina ? st.left : 1.f;
}

float shakeLevel()
{
    if(levelAt == realtime)
    {
        return level;
    }
    float target = 0.f;
    const bool on = vr_fatigue_shake.value > 0.f || vr_fatigue_shake_angle.value > 0.f;
    if(on && (vr_fatigue_shake_always.value || climbing()))
    {
        const float from = za::clamp(vr_fatigue_shake_from.value, 0.01f, 1.f);
        const float left = staminaLeft();
        if(left < from)
        {
            target = za::pow(za::clamp((from - left) / from, 0.f, 1.f), growth);
        }
    }
    const float dt = levelAt >= 0.0 ? static_cast<float>(za::clamp(realtime - levelAt, 0.0, 0.25)) : 1.f;
    levelAt = realtime;
    level = target + (level - target) * za::exp(-dt / easeTime);
    if(za::fabs(level - target) < 1e-4f)
    {
        level = target;
    }
    return level;
}

void shake(int hand, glm::vec3& pos, glm::vec3& angles)
{
    pos = glm::vec3{0.f};
    angles = glm::vec3{0.f};
    const float lv = shakeLevel();
    if(lv <= 0.f || (hand != 0 && hand != 1))
    {
        return;
    }
    const float t = static_cast<float>(za::fmod(realtime, 1000.0)) * za::max(vr_fatigue_shake_speed.value, 0.f);
    // Now stronger, now weaker (a tired arm's tremor comes in bouts).
    const float bout = lv * (0.75f + 0.25f * za::sin(glm::two_pi<float>() * 0.37f * t + 1.9f * static_cast<float>(hand)));
    const float cm = za::max(vr_fatigue_shake.value, 0.f) * 0.01f * units::metresToUnits() * bout;
    const float deg = za::max(vr_fatigue_shake_angle.value, 0.f) * bout;
    pos = glm::vec3{tremor(t, hand, 0), tremor(t, hand, 1), tremor(t, hand, 2)} * cm;
    angles = glm::vec3{tremor(t, hand, 3), tremor(t, hand, 4), tremor(t, hand, 5)} * deg;
    if(vr_debug_fatigue.value && hand == 1 && realtime - printedAt >= 0.25)
    {
        printedAt = realtime;
        Con_Printf("fatigue: stamina %.2f shake %.3f (bout %.3f) main off %.3f cm %.3f deg%s\n", staminaLeft(), lv, bout,
            glm::length(pos) / units::metresToUnits() * 100.f, glm::length(angles), climbing() ? " climbing" : "");
    }
}

void serverFrame()
{
    if(!vr_debug_stamina_hold.value)
    {
        heldAt = -1.f;
        return;
    }
    if(!sv.active)
    {
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    if(edict_t* ent = player())
    {
        if(heldAt < 0.f)
        {
            heldAt = za::clamp(1.f - fieldFloat(ent, fields().vr_stamina_used) / staminaMax(), 0.f, 1.f);
        }
        setStamina(ent, heldAt);
    }
    PR_PopQCVM(oldvm);
}

void registerCommands()
{
    Cmd_AddCommand("vr_stamina_set", set_f);
}

float speedScaleFor(float left)
{
    if(!vr_stamina_speed.value)
    {
        return 1.f;
    }
    const float least = za::clamp(vr_stamina_speed_min.value, 0.05f, 1.f);
    return 1.f - (1.f - least) * weight::tiredShare(left);
}

} // namespace qvr::fatigue

extern "C" cvar_t sv_maxspeed; // sv_user.c

// SV_AirMove (walking, and in the air): tired, the most speed the stick moves you at (sv_maxspeed) times this. This
// player's own stamina, as the progs sent it last frame (.vr_melee_hud: the pool on; .vr_stamina_used: what is spent).
// 1 without the VR progs, with the pool off, or rested above vr_weight_stamina_from.
extern "C" float VR_StaminaSpeedScale(edict_t* ent)
{
    using namespace qvr;
    using namespace qvr::progs;
    if(!ent || !bindings().isVrProgs)
    {
        return 1.f;
    }
    const FieldOffsets& f = fields();
    const bool on = (static_cast<int>(fieldFloatOr(ent, f.vr_melee_hud, 0.f)) & 128) != 0;
    const float most = za::max(1.f, vr_parry_stamina_max.value);
    const float left = on ? za::clamp(1.f - fieldFloatOr(ent, f.vr_stamina_used, 0.f) / most, 0.f, 1.f) : 1.f;
    const float scale = fatigue::speedScaleFor(left);
    if(vr_debug_stamina_speed.value && (realtime - fatigue::speedPrintedAt >= 0.5 || realtime < fatigue::speedPrintedAt))
    {
        fatigue::speedPrintedAt = realtime;
        Con_Printf("stamina speed: stamina %.2f cap %.0f (x%.3f) ground speed %.1f\n", left, sv_maxspeed.value * scale, scale,
            za::hypot(ent->v.velocity[0], ent->v.velocity[1]));
    }
    return scale;
}
