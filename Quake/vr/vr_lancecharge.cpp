// vr_lancecharge.cpp -- Dwell's Crystal Lance's charged shot as drawn and heard (vr_lancecharge.hpp).

#include "vr_lancecharge.hpp"

#include "vr_anchor.hpp"
#include "vr_axestick.hpp"
#include "vr_backend.hpp"
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
constexpr double lostGrace = 0.25;  // s a charge's Lance may be found nowhere (a frame between the hand and its stuck
                                    // prop, embedfire) before it is gone with it
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
    bool logFired{false}; // vr_debug_gunfx: where it fired from to print (its bolt's start: boltStart)
    double fired{-1.0};   // cl.time it last fired (yours: shake's)
    double buzzNext{-1.0}; // yours: the charge's next buzz
    double lost{-1.0};     // cl.time its Lance was first found nowhere (-1: drawn)
};

// A Lance model's middle crystal's tip (his note 2026-11-23: the charge's light and sparks came from a little off it):
// the vertex furthest along the line from its hand to its muzzle (weapons.txt hand_av, muzzle_av) within tipReach model
// units of that line, found once per model as drawn. The gun's muzzle is on that tip; the spear's (dw_lance) between its
// two outer prongs' tips, a point in the air, bound to the nearer prong (6.5 units off the line), its middle crystal 8
// units back on the line.
constexpr float tipReach = 1.f;
struct Tip
{
    const qmodel_t* model{nullptr};
    int vertex{-1};
};

struct State
{
    sfx_t* hum{nullptr};
    Charge of[MAX_SCOREBOARD + 1][2];
    za::U32 seed{0x1a2b3c4du};
    Tip tips[2]; // (the gun's and the spear's)
    za::Vector<glm::vec3> rest, now; // (tipVertex's)
    bool boltLog[MAX_SCOREBOARD + 1][2]{}; // a new bolt's start to print (vr_debug_gunfx: beamLife, then boltStart)
    float shakeMost{0.f}; // yours: the most shake given (kicks) since the last vr_debug_gunfx event print (lance3)
    int buzzes{0};        // yours: the charge's and the shot's buzzes started (printed with it)
    view::ViewEntity stuck[2]; // your hands' Lance stuck in a body as drawn (stuckLance's)
    bool propBoltLog{false};   // a prop's new bolt's start to print (vr_debug_gunfx: beamLife, then boltStart)
};

State st;

[[nodiscard]] bool isLance(const qmodel_t* m)
{
    return m && (ZA_STRSTR(m->name, "dw_lance") || ZA_STRSTR(m->name, "dw_rail"));
}

// The Lance stuck in a body that your `hand` holds (embedfire: it charges and fires there, QC vr_propfire.qc), as drawn:
// its client entity (axestick::heldEnt) in st.stuck[hand]; else null.
[[nodiscard]] const view::ViewEntity* stuckLance(int hand)
{
    const int e = axestick::heldEnt(hand);
    if(e <= 0 || e >= cl.num_entities || !isLance(cl_entities[e].model))
    {
        return nullptr;
    }
    view::ViewEntity& w = st.stuck[hand];
    w = view::ViewEntity{};
    w.ent = cl_entities[e];
    w.visible = true;
    w.netEntity = e; // (its networked scale and offset, as a lying gun's)
    return &w;
}

// Player `p`'s Lance in `hand` as drawn this frame (yours: `own`, or the Lance stuck in a body that hand holds; another
// player's copy's), else null.
[[nodiscard]] const view::ViewEntity* drawnLance(int p, int hand, const view::ViewEntity (&own)[2])
{
    const view::ViewEntity* w = p == cl.viewentity ? &own[hand] : mpavatar::drawnWeapon(p, hand);
    if(w && w->ent.model && (w->visible || p != cl.viewentity) && isLance(w->ent.model))
    {
        return w;
    }
    return p == cl.viewentity ? stuckLance(hand) : nullptr;
}

// An anchor's text ("x y z", weapons.txt *_av) as a point in model units; false: not three numbers.
[[nodiscard]] bool modelUnits(const char* text, glm::vec3& out)
{
    const char* p = text ? text : "";
    for(int k = 0; k < 3; k++)
    {
        char* end = nullptr;
        out[k] = strtof(p, &end);
        if(end == p)
        {
            return false;
        }
        p = end;
    }
    return true;
}

// The drawn Lance `w` (weapon slot `slot`)'s middle crystal's tip vertex (Tip), its muzzle drawn at `muzzle`; -1 none
// (no Quake vertices, no line).
[[nodiscard]] int tipVertex(const view::ViewEntity& w, int slot, glm::vec3 muzzle)
{
    const qmodel_t* m = w.ent.model;
    for(const Tip& t : st.tips)
    {
        if(t.model == m)
        {
            return t.vertex;
        }
    }
    int best = -1;
    // (The anchors' own points, in the model's units: as drawn, an anchor is bound to the surface nearest it.)
    glm::vec3 handAt{0.f}, muzzleAt{0.f};
    const bool points = modelUnits(weapons::anchor(slot, weapons::Key::HandAnchor), handAt) &&
                        modelUnits(weapons::anchor(slot, weapons::Key::MuzzleAnchor), muzzleAt);
    const glm::vec3 hand = points ? view::modelPoint(w, handAt)
                                  : view::anchorPosition(w, weapons::anchor(slot, weapons::Key::HandAnchor), glm::vec3{0.f});
    if(points)
    {
        muzzle = view::modelPoint(w, muzzleAt);
    }
    const float perUnit = glm::length(view::modelPoint(w, glm::vec3{1.f, 0.f, 0.f}) - view::modelPoint(w, glm::vec3{0.f}));
    const float len = glm::length(muzzle - hand);
    if(len > 1e-3f && anchor::posedVertices(w.ent, w.zeroBlend, st.rest, st.now))
    {
        const glm::vec3 dir = (muzzle - hand) / len;
        float bestAlong = -1e30f;
        for(int v = 0; v < static_cast<int>(st.now.size()); v++)
        {
            const glm::vec3 d = view::vertexPosition(w, v) - hand;
            const float along = glm::dot(d, dir);
            if(glm::length(d - dir * along) <= tipReach * perUnit && along > bestAlong)
            {
                bestAlong = along;
                best = v;
            }
        }
    }
    Tip& t = st.tips[st.tips[0].model ? 1 : 0];
    t.model = m;
    t.vertex = best;
    return best;
}

// The drawn Lance `w`'s middle crystal's tip (Tip) into `at` (its muzzle as drawn going in, kept without a tip): the tip's
// vertex, -1 none. The charge's origin (frame) and its bolt's start (boltStart).
[[nodiscard]] int tipAt(const view::ViewEntity& w, glm::vec3& at)
{
    const int tip = tipVertex(w, weapons::slotForModel(w.ent.model), at);
    if(tip >= 0)
    {
        at = view::vertexPosition(w, tip);
    }
    return tip;
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
        Con_Printf("lancecharge: %s ent %d hand %d level %.2f (%.2f s); shake most %.3f, buzzes %d\n", what, p, hand, c.level,
            c.seconds, st.shakeMost, st.buzzes);
        st.shakeMost = 0.f;
        st.buzzes = 0;
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
    c.logFired = state == 2 && vr_debug_gunfx.value;
    debugPrint(state == 2 ? "fired" : "let go short", p, hand, c);
    if(state == 2)
    {
        c.fired = cl.time;
        // Your hand's buzz as it fires (lance3): strong, dying away with its shake.
        if(p == cl.viewentity && vr_dw_lance_haptic_fire.value > 0.f)
        {
            haptics::pulse(hand, za::max(vr_dw_lance_shake_time.value, 0.05f), za::min(vr_dw_lance_haptic_fire.value, 1.f), 70.f);
            st.buzzes++;
        }
    }
}

void shake(int hand, float& kick, float& sideways)
{
    if(hand < 0 || hand > 1 || cl.viewentity < 1 || cl.viewentity > MAX_SCOREBOARD)
    {
        return;
    }
    const Charge& c = st.of[cl.viewentity][hand];
    const float t = static_cast<float>(cl.time);
    constexpr float tau = 6.2831853f;
    // (Three waves of the clock, as a stream's tremble: the same each run.)
    const float up = 0.55f * za::sin(tau * 13.1f * t) + 0.3f * za::sin(tau * 21.7f * t + 1.1f) + 0.15f * za::sin(tau * 31.3f * t + 2.3f);
    const float side = 0.6f * za::sin(tau * 9.3f * t + 0.7f) + 0.4f * za::sin(tau * 17.1f * t + 2.9f);
    float amount = 0.f;
    if(c.charging && c.level > 0.f)
    {
        amount += za::max(vr_dw_lance_shake_charge.value, 0.f) * c.level * c.level;
    }
    const float since = static_cast<float>(cl.time - c.fired);
    const float time = za::max(vr_dw_lance_shake_time.value, 0.01f);
    if(c.fired >= 0.0 && since >= 0.f && since < time)
    {
        const float x = 1.f - since / time;
        amount += za::max(vr_dw_lance_shake_fire.value, 0.f) * x * x;
    }
    if(amount <= 0.f)
    {
        return;
    }
    kick += amount * (0.6f + 0.4f * up); // (always back and up some, trembling)
    sideways += amount * 0.5f * side;
    st.shakeMost = za::max(st.shakeMost, amount);
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
                // Found nowhere a moment (the frame the hand's Lance became its stuck prop, or came back out: embedfire):
                // the charge kept, not drawn.
                if(c.charging && (c.lost < 0.0 || cl.time - c.lost < lostGrace))
                {
                    if(c.lost < 0.0)
                    {
                        c.lost = cl.time;
                    }
                    continue;
                }
                // The Lance left the hand (thrown, dropped, holstered, his copy not drawn): nothing to draw it on.
                c.lost = -1.0;
                if(c.charging || c.level > 0.f)
                {
                    debugPrint("gone with the Lance", p, hand, c);
                }
                c.charging = false;
                c.level = 0.f;
                stopHum(c, p, hand);
                continue;
            }
            c.lost = -1.0;
            const float was = c.level;
            c.level = c.charging ? za::clamp(static_cast<float>(cl.time - c.start) / c.seconds, 0.f, 1.f)
                                 : za::max(0.f, c.level - dt / c.fade);
            // Drawn from the middle crystal's tip as drawn (Tip).
            const glm::vec3 muzzle = m.at;
            const int tip = tipAt(*w, m.at);
            if(vr_debug_gunfx.value && c.charging && was <= 0.f && c.level > 0.f && tip >= 0 && tip < static_cast<int>(st.rest.size()))
            {
                const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(w->ent.model)));
                const glm::vec3 r = st.rest[tip] * glm::vec3{hdr->scale[0], hdr->scale[1], hdr->scale[2]} +
                                    glm::vec3{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
                Con_Printf("lancecharge: origin at the middle crystal's tip (%s vertex %d, at rest %.2f %.2f %.2f); the muzzle "
                           "anchor %.3f from it\n",
                    w->ent.model->name, tip, r.x, r.y, r.z, glm::distance(muzzle, m.at));
            }
            if(c.logFired)
            {
                c.logFired = false;
                Con_Printf("lancecharge: the charge fired ent %d hand %d at %.2f %.2f %.2f\n", p, hand, m.at.x, m.at.y, m.at.z);
            }
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
            // Your hand's buzz as it charges (lance3): a light one growing with the charge, renewed every 50 ms.
            if(p == cl.viewentity && c.charging && vr_dw_lance_haptic_charge.value > 0.f && cl.time >= c.buzzNext)
            {
                c.buzzNext = cl.time + 0.05;
                haptics::pulse(hand, 0.06f, za::min(vr_dw_lance_haptic_charge.value, 1.f) * (0.25f + 0.75f * k), 140.f);
                st.buzzes++;
            }
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
        if(e <= MAX_SCOREBOARD)
        {
            st.boltLog[e][id] = true;
        }
        else
        {
            st.propBoltLog = true;
        }
    }
    return life;
}

bool boltStart(int ent, int hand, const glm::vec3& muzzle, glm::vec3& out)
{
    if(ent > cl.maxclients && ent < cl.num_entities)
    {
        // A prop's bolt (embedfire): the Lance stuck in a body that your hand holds and fires (QC vr_propfire.qc), from its
        // tip as drawn. (Not an armed grunt's: its gun is drawn by its arms, away from its entity.)
        for(int h = 0; h < 2; h++)
        {
            if(axestick::heldEnt(h) != ent)
            {
                continue;
            }
            const view::ViewEntity* w = stuckLance(h);
            gunfx::Muzzle m;
            if(!w || !gunfx::drawnMuzzle(*w, weapons::slotForModel(w->ent.model), m) || tipAt(*w, m.at) < 0)
            {
                return false;
            }
            out = m.at;
            if(st.propBoltLog)
            {
                st.propBoltLog = false;
                Con_Printf("lancecharge: bolt from the stuck Lance (ent %d, your hand %d) at its tip %.2f %.2f %.2f, %.2f off "
                           "the server's muzzle\n", ent, h, out.x, out.y, out.z, glm::distance(out, muzzle));
            }
            return true;
        }
        return false;
    }
    if(ent < 1 || ent > MAX_SCOREBOARD || ent > cl.maxclients || hand < 0 || hand > 1)
    {
        return false;
    }
    const view::ViewEntity* w = ent == cl.viewentity ? view::heldWeapon(hand) : mpavatar::drawnWeapon(ent, hand);
    gunfx::Muzzle m;
    if(!w || !isLance(w->ent.model) || !gunfx::drawnMuzzle(*w, weapons::slotForModel(w->ent.model), m))
    {
        return false;
    }
    const int tip = tipAt(*w, m.at);
    if(tip < 0)
    {
        return false;
    }
    out = m.at;
    if(st.boltLog[ent][hand])
    {
        st.boltLog[ent][hand] = false;
        Con_Printf("lancecharge: bolt ent %d hand %d from the tip (%s vertex %d) %.2f %.2f %.2f, %.2f off the muzzle\n",
            ent, hand, w->ent.model->name, tip, out.x, out.y, out.z, glm::distance(out, muzzle));
    }
    return true;
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
            st.boltLog[p][hand] = false;
        }
    }
}

} // namespace qvr::lancecharge
