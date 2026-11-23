// vr_lancecharge.cpp -- Dwell's Crystal Lance's charged shot as drawn and heard (vr_lancecharge.hpp).

#include "vr_lancecharge.hpp"

#include "vr_cvars.hpp"
#include "vr_emissive.hpp"
#include "vr_engine.hpp"
#include "vr_geom.hpp"
#include "vr_gunfx.hpp"
#include "vr_mem.hpp"
#include "vr_mpavatar.hpp"
#include "vr_particles.hpp"
#include "vr_random.hpp"
#include "vr_tunables.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Base/Strstr.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/glm.hpp>

namespace qvr::lancecharge
{

namespace
{

constexpr const char* humName = "vr/lance_charge.wav";
constexpr int humChannel = 1010;    // + the hand: the hum on the player's entity (none of the game's channels)
constexpr float humVolume = 0.75f;  // at full charge
constexpr float humPitchLow = 0.6f; // its rate as the charge begins .. once full
constexpr float humPitchHigh = 1.35f;
constexpr float firedFade = 0.06f;  // s the charge's look goes out over as it fires (its bolt takes over)
constexpr float quakeBeamLife = 0.2f; // (cl_tent.c's)
constexpr int maxSparksFrame = 12;
const glm::vec3 violet{0.62f, 0.32f, 1.f};
const glm::vec3 paleViolet{0.88f, 0.75f, 1.f};

// One player's charge in one hand.
struct Charge
{
    bool charging{false};
    double start{-1.0}; // cl.time it began
    float seconds{1.f}; // to full (the server's vr_dw_lance_charge)
    float level{0.f};   // drawn: 0 .. 1
    float fade{0.25f};  // s from full to none once let go
    float owed{0.f};    // sparks owed (a fraction carried to the next frame)
    double last{-1.0};  // the frame last drawn
    channel_t* ch{nullptr};
};

struct State
{
    sfx_t* hum{nullptr};
    Charge of[MAX_SCOREBOARD + 1][2];
    za::U32 seed{0x1a2b3c4du};
};

State st;

[[nodiscard]] bool isLance(const qmodel_t* m)
{
    return m && (ZA_STRSTR(m->name, "dw_lance") || ZA_STRSTR(m->name, "dw_rail"));
}

// Player `p`'s Lance in `hand` as drawn this frame (yours: `own`; another player's copy's), else null.
[[nodiscard]] const view::ViewEntity* drawnLance(int p, int hand, const view::ViewEntity (&own)[2])
{
    const view::ViewEntity* w = p == cl.viewentity ? &own[hand] : mpavatar::drawnWeapon(p, hand);
    return w && w->ent.model && (w->visible || p != cl.viewentity) && isLance(w->ent.model) ? w : nullptr;
}

[[nodiscard]] channel_t* playing(const Charge& c, int ent, int hand)
{
    channel_t* ch = c.ch;
    return ch && st.hum && ch->sfx == st.hum && ch->entnum == ent && ch->entchannel == humChannel + hand ? ch : nullptr;
}

void stopHum(Charge& c, int ent, int hand)
{
    if(c.ch)
    {
        S_MixJoin(); // (a read of the channels on the main thread: the pool's mix done first, S_MixKick)
        if(channel_t* ch = playing(c, ent, hand))
        {
            ch->sfx = nullptr;
        }
    }
    c.ch = nullptr;
}

// The hum at `at`, following the charge drawn (`k` eased).
void hum(Charge& c, int ent, int hand, const glm::vec3& at, float k)
{
    const float volume = humVolume * za::sqrt(za::clamp(k, 0.f, 1.f));
    if(!st.hum || volume < 0.01f)
    {
        stopHum(c, ent, hand);
        return;
    }
    const float pitch = humPitchLow + (humPitchHigh - humPitchLow) * k;
    S_MixJoin(); // (as stopHum)
    channel_t* ch = playing(c, ent, hand);
    if(!ch)
    {
        vec3_t o{at.x, at.y, at.z};
        S_StartSoundPitch(ent, humChannel + hand, st.hum, o, volume, 1.f, pitch);
        c.ch = nullptr;
        for(int i = 0; i < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; i++)
        {
            channel_t& s = snd_channels[i];
            if(s.sfx == st.hum && s.entnum == ent && s.entchannel == humChannel + hand)
            {
                c.ch = &s;
                break;
            }
        }
        ch = c.ch;
        if(!ch)
        {
            return; // (no sound system, or not heard: tried again next frame)
        }
    }
    ch->master_vol = static_cast<int>(volume * 255.f);
    ch->pitch = pitch;
    ch->resampled = 1; // (its end reckoned again at the new rate: snd_mix.c)
    geom::store(at, ch->origin);
}

// Sparks gathering into the tip `at`, the light there (`k` the charge eased, `full` charged and held).
void look(Charge& c, int p, int hand, const glm::vec3& at, float k, bool full, float dt)
{
    const float pulse = full ? 0.82f + 0.18f * za::sin(static_cast<float>(cl.time) * 17.f + static_cast<float>(p * 2 + hand)) : 1.f;
    emissive::lanceChargeLight(p, hand, at, glm::mix(violet, paleViolet, 0.35f * k), (0.2f + 1.1f * k) * pulse, 20.f + 90.f * k);
    if(!particles::enabled())
    {
        return;
    }
    c.owed += dt * (12.f + 70.f * k);
    const int n = za::min(static_cast<int>(c.owed), maxSparksFrame);
    c.owed -= static_cast<float>(static_cast<int>(c.owed));
    for(int i = 0; i < n; i++)
    {
        // A point round the tip (closer as it charges), drawn in to it over its life.
        glm::vec3 d{vrand::lcgRange(st.seed, -1.f, 1.f), vrand::lcgRange(st.seed, -1.f, 1.f), vrand::lcgRange(st.seed, -1.f, 1.f)};
        const float len = glm::length(d);
        d = len > 1e-3f ? d / len : glm::vec3{0.f, 0.f, 1.f};
        const float r = vrand::lcgRange(st.seed, 7.f, 15.f) * (1.15f - 0.4f * k);
        particles::GunParticle g;
        g.kind = particles::GunParticle::Spark;
        g.life = vrand::lcgRange(st.seed, 0.16f, 0.26f);
        g.org = at + d * r;
        g.vel = -d * (r / g.life);
        g.color = glm::vec4{glm::mix(violet, paleViolet, vrand::lcg01(st.seed)), 0.6f + 0.4f * k};
        g.scale = vrand::lcgRange(st.seed, 0.35f, 0.6f) * (0.8f + 0.5f * k);
        g.streak = 0.03f;
        g.fadeIn = 0.05f;
        (void)particles::gunParticle(g);
    }
    // Its core: a soft glow on the tip, growing.
    particles::GunParticle g;
    g.kind = particles::GunParticle::Glow;
    g.org = at;
    g.color = glm::vec4{glm::mix(violet, paleViolet, 0.5f), 0.25f + 0.35f * k * pulse};
    g.scale = 1.5f + 3.5f * k;
    g.life = 0.04f;
    (void)particles::gunParticle(g);
}

void debugPrint(const char* what, int p, int hand, const Charge& c)
{
    if(vr_debug_gunfx.value)
    {
        Con_Printf("lancecharge: %s ent %d hand %d level %.2f (%.2f s)\n", what, p, hand, c.level, c.seconds);
    }
}

mem::Reset clientStateReset{"lancecharge", mem::ResetEvent::ClientState, mem::ResetPhase::State, &clear};

} // namespace

void parse()
{
    const int p = MSG_ReadShort();
    const int hand = MSG_ReadByte();
    const int state = MSG_ReadByte();
    const float seconds = static_cast<float>(MSG_ReadByte()) / 50.f;
    if(p < 1 || p > MAX_SCOREBOARD || hand < 0 || hand > 1)
    {
        return;
    }
    Charge& c = st.of[p][hand];
    c.seconds = za::max(seconds, 0.02f);
    if(state == 1)
    {
        c.charging = true;
        c.start = cl.time - static_cast<double>(c.level * c.seconds); // (from where it was drawn, a fade not over)
        debugPrint("charging", p, hand, c);
        return;
    }
    c.charging = false;
    c.fade = state == 2 ? firedFade : za::max(vr_dw_lance_charge_fade.value, 0.02f);
    debugPrint(state == 2 ? "fired" : "let go short", p, hand, c);
}

void frame(const view::ViewEntity (&own)[2])
{
    const int players = za::min(cl.maxclients, MAX_SCOREBOARD);
    for(int p = 1; p <= players; p++)
    {
        for(int hand = 0; hand < 2; hand++)
        {
            Charge& c = st.of[p][hand];
            if(!c.charging && c.level <= 0.f && !c.ch)
            {
                c.last = -1.0;
                continue;
            }
            const float dt = c.last >= 0.0 ? za::clamp(static_cast<float>(cl.time - c.last), 0.f, 0.1f) : 0.f;
            c.last = cl.time;
            const view::ViewEntity* w = cls.signon == SIGNONS ? drawnLance(p, hand, own) : nullptr;
            gunfx::Muzzle m;
            if(!w || !gunfx::drawnMuzzle(*w, weapons::slotForModel(w->ent.model), m))
            {
                // The Lance left the hand (thrown, dropped, holstered, his copy not drawn): nothing to draw it on.
                if(c.charging || c.level > 0.f)
                {
                    debugPrint("gone with the Lance", p, hand, c);
                }
                c.charging = false;
                c.level = 0.f;
                stopHum(c, p, hand);
                continue;
            }
            const float was = c.level;
            c.level = c.charging ? za::clamp(static_cast<float>(cl.time - c.start) / c.seconds, 0.f, 1.f)
                                 : za::max(0.f, c.level - dt / c.fade);
            if(c.charging && c.level >= 1.f && was < 1.f)
            {
                debugPrint("full", p, hand, c);
            }
            if(c.level <= 0.f)
            {
                stopHum(c, p, hand);
                continue;
            }
            const float k = c.level * c.level * (3.f - 2.f * c.level); // (eased in and out)
            look(c, p, hand, m.at, k, c.charging && c.level >= 1.f, dt);
            hum(c, p, hand, m.at, k);
        }
    }
}

float beamLife(int ent)
{
    const int e = ent & 0xFFFF;
    const int id = (ent >> 16) - 1;
    if(e <= 0 || e >= cl.num_entities || id < 0 || id > 1)
    {
        return quakeBeamLife;
    }
    const qmodel_t* m = nullptr;
    if(e > MAX_SCOREBOARD || e > cl.maxclients)
    {
        m = cl_entities[e].model; // (an armed grunt's Lance prop)
    }
    else if(e == cl.viewentity)
    {
        m = weapons::heldModel(id);
    }
    else if(const view::ViewEntity* w = mpavatar::drawnWeapon(e, id))
    {
        m = w->ent.model;
    }
    if(!isLance(m))
    {
        return quakeBeamLife;
    }
    const float life = za::max(vr_dw_lance_bolt.value, 0.05f);
    if(vr_debug_gunfx.value)
    {
        Con_Printf("lancecharge: bolt ent %d hand %d lingers %.2f s\n", e, id, life);
    }
    return life;
}

void prepare()
{
    st.hum = S_PrecacheSound(humName);
}

void clear()
{
    for(int p = 0; p <= MAX_SCOREBOARD; p++)
    {
        for(int hand = 0; hand < 2; hand++)
        {
            stopHum(st.of[p][hand], p, hand);
            st.of[p][hand] = Charge{};
        }
    }
}

} // namespace qvr::lancecharge
