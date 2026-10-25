// vr_bullettime.cpp -- see vr_bullettime.hpp.

#include "vr_bullettime.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_foegrab.hpp"
#include "vr_touch.hpp"
#include "vr_gadget.hpp"
#include "vr_hands.hpp"
#include "vr_held.hpp"
#include "vr_highlights.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_progs.hpp"
#include "vr_twohand.hpp"
#include "vr_timescale.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <cstdio>

namespace qvr::bullettime
{
namespace
{

struct State
{
    bool active = false;
    float level = 1.f;    // the meter, 0 .. 1
    float cooldown = 0.f; // real seconds left before the meter fills and it can start again
    float look = 0.f;     // the look's ease, 0 .. 1
    double triggeredAt = -1.0; // realtime of the last tap or stick press that counted (vr_bullettime_trigger_cooldown)
    float tapPeak = 0.f;       // the screen tap: the peak speed (m/s) into the screen, over it; 0: none
    double tapPeakAt = -1.0;   // ... and when
    int tapStriker = 0;        // ... and which striking point (its index in the striking volume: strikers)
    double tappedAt = -1.0;    // realtime of the last tap that counted (tapping())
    double firstTapAt = -1.0;  // vr_bullettime_tap_gesture 1: realtime of a double tap's first tap (-1: none waiting)
    bool tapLifted = true;     // the striking volume off the screen since the last tap counted (none counts before)
    // The frantic-melee guards (calm()): the player's last landed blow, shove or bash as the QC has it (.vr_blow_time,
    // server time) and the realtime it changed; the realtime a hand was last faster than vr_bullettime_tap_calm_speed,
    // which and how fast.
    float blowField = 0.f;
    double blowAt = -1e9;
    double fastAt = -1e9;
    int fastHand = 0;
    float fastSpeed = 0.f;
    bool bothHeld = false; // both hands on an enemy or a body (vr_bullettime_tap_both_held: taps off)
    bool stickTaken[HAND_COUNT] = {}; // the stick press taken at its press (vr_bullettime_trigger): its release is too
    glm::vec3 viewPos[HAND_COUNT]{};   // the hands' places (hands::State::pos) as the view tested the tap (movedSinceView)
    bool viewValid = false;
    glm::vec3 startOffset{0.f};        // vr_debug_gadget_button 3: the screen's middle from the tapping hand at the frame's
    bool startValid = false;           // start (frameStartOffset)
    float written = -1.f;              // the player's .vr_bullettime_spent as last written (another value: a load, a level)
    double earnedAt = -1e9;            // realtime the meter last grew by damage dealt (the gadget's frame lights)
    double emptyNoticeAt = -1e9;       // realtime the gadget last said the meter is empty (emptyNotice)
};
State state;

// Earned bullet time pressed with too little in the meter (the author's note vrtesthall_2026-10-23_01-47-31): the wrist
// gadget's hologram says how to fill it, at most every few seconds (another press while it shows keeps it on). No
// hologram (the HUD not the gadget, or vr_messages_hologram 0): a centre print.
constexpr const char* emptyNoticeText = "Bullet time empty:\nhurt enemies to refill";
constexpr double emptyNoticeEvery = 4.0; // real seconds

void emptyNotice()
{
    if(realtime - state.emptyNoticeAt < emptyNoticeEvery && state.emptyNoticeAt <= realtime)
    {
        return;
    }
    state.emptyNoticeAt = realtime;
    if(!gadget::notice(emptyNoticeText, 3.f))
    {
        SCR_CenterPrintFor(emptyNoticeText, 2.5f);
    }
    if(vr_debug_bullettime.value)
    {
        Con_Printf("bullet time: empty notice\n");
    }
}

// The "off" sound (vr_bullettime_sound_off: a placeholder 3 s long, longer than a short burst): played for at most
// vr_bullettime_sound_off_max real seconds, then faded out over offFade; cut (faded over offCut) as bullet time starts
// again, or a denied press sounds. Its channel found again each frame (by its sound, the player's entity and the local
// sounds' channel -1, as S_LocalSound starts it): a channel taken by another sound since is never touched.
struct OffSound
{
    sfx_t* sfx = nullptr;   // playing (nullptr: none, or done with)
    double fadeFrom = 0.0;  // realtime its fade starts
    double fadeLen = 0.0;   // ... and lasts
};
OffSound offSound;
constexpr double offFade = 0.25;
constexpr double offCut = 0.06;

[[nodiscard]] channel_t* offChannel()
{
    if(!offSound.sfx || cl.viewentity <= 0)
    {
        return nullptr;
    }
    S_MixJoin(); // (the pool's mix done first: S_MixKick)
    for(int i = 0; i < MAX_DYNAMIC_CHANNELS; i++)
    {
        channel_t* ch = &snd_channels[i];
        if(ch->sfx == offSound.sfx && ch->entnum == cl.viewentity && ch->entchannel == -1)
        {
            return ch;
        }
    }
    return nullptr;
}

// The "off" sound's fade, each frame (advance).
void fadeOffSound()
{
    channel_t* ch = offChannel();
    if(!ch)
    {
        offSound.sfx = nullptr;
        return;
    }
    const double t = realtime - offSound.fadeFrom;
    if(t <= 0.0)
    {
        return;
    }
    const double gain = offSound.fadeLen > 0.0 ? 1.0 - t / offSound.fadeLen : 0.0;
    if(gain <= 0.0)
    {
        ch->sfx = nullptr; // (as S_StopSound: this channel only)
        ch->end = 0;
        offSound.sfx = nullptr;
        if(vr_debug_bullettime.value)
        {
            Con_Printf("bullet time: the off sound cut %.2f s after its fade began\n", t);
        }
        return;
    }
    ch->master_vol = za::min(ch->master_vol, static_cast<int>(255.0 * gain));
}

// Its fade from now, over `seconds` (not later than one under way).
void cutOffSound(double seconds)
{
    if(offSound.sfx && realtime + seconds < offSound.fadeFrom + offSound.fadeLen)
    {
        offSound.fadeFrom = realtime;
        offSound.fadeLen = seconds;
    }
}

[[nodiscard]] bool earnedMode()
{
    return vr_bullettime_meter_mode.value != 0.f;
}

// Earned bullet time (vr_bullettime_meter_mode 1; ROUND22.md, "Earned bullet time"): the meter never fills over time,
// only by the damage the player deals living monsters (QC VR_BulletTime_Earn: at most the health each had, never a
// corpse's, a prop's or a monster's to another), vr_bullettime_earn_damage of it from empty to full; while it runs, at
// vr_bullettime_earn_active of that. The meter is kept on the player (.vr_bullettime_spent, 1 - the level: 0 is full, a
// new game's and an old save's), so a save keeps it and the next level gets it (parm74); a value there the engine didn't
// write (a load, a level's start) is taken as the meter. The damage arrives in .vr_bullettime_earned, taken each frame
// (in both modes: over time it is dropped).
[[nodiscard]] edict_t* meterPlayer()
{
    return sv.active && svs.maxclients == 1 && svs.clients[0].edict ? svs.clients[0].edict : nullptr;
}

void readMeter()
{
    edict_t* player = meterPlayer();
    const int ofs = progs::fields().vr_bullettime_spent;
    if(player && ofs >= 0)
    {
        const float spent = progs::fieldFloat(player, ofs);
        if(spent != state.written)
        {
            state.level = za::clamp(1.f - spent, 0.f, 1.f);
            state.written = spent;
        }
    }
}

void writeMeter()
{
    edict_t* player = meterPlayer();
    const int ofs = progs::fields().vr_bullettime_spent;
    if(player && ofs >= 0)
    {
        state.written = 1.f - state.level;
        progs::fieldFloat(player, ofs) = state.written;
    }
}

// The damage that fills the meter from empty: vr_bullettime_earn_damage (Normal's) times the game's skill's multiplier
// (vr_bullettime_earn_skill0..3: Easy fills sooner, Hard and Nightmare later).
[[nodiscard]] float earnDamage()
{
    const cvar_t* const perSkill[] = {&vr_bullettime_earn_skill0, &vr_bullettime_earn_skill1, &vr_bullettime_earn_skill2,
        &vr_bullettime_earn_skill3};
    const int level = za::clamp(static_cast<int>(skill.value + 0.5f), 0, 3);
    return za::max(1.f, vr_bullettime_earn_damage.value * za::max(0.01f, perSkill[level]->value));
}

void earn()
{
    edict_t* player = meterPlayer();
    const int ofs = progs::fields().vr_bullettime_earned;
    if(!player || ofs < 0)
    {
        return;
    }
    const float damage = progs::fieldFloat(player, ofs);
    progs::fieldFloat(player, ofs) = 0.f;
    if(damage <= 0.f || !earnedMode())
    {
        return;
    }
    const float share = state.active ? za::clamp(vr_bullettime_earn_active.value, 0.f, 1.f) : 1.f;
    const float gain = damage / earnDamage() * share;
    const float before = state.level;
    state.level = za::min(1.f, state.level + gain);
    if(state.level > before)
    {
        state.earnedAt = realtime;
    }
    if(vr_debug_bullettime.value)
    {
        Con_Printf("bullet time: earned %g damage%s, meter %.3f -> %.3f\n", damage, state.active ? " (running)" : "",
                   before, state.level);
    }
}

[[nodiscard]] bool inGame()
{
    return vr_bullettime_enabled.value != 0.f && sv.active && svs.maxclients == 1 && cls.state == ca_connected &&
           cls.signon == SIGNONS && !cl.intermission && cl.stats[STAT_HEALTH] > 0;
}

void playSound(const cvar_t& name)
{
    if(name.string && name.string[0])
    {
        S_LocalSound(name.string);
    }
}

void buzz(int hand, float seconds, float amplitude)
{
    if(Backend* const be = backend(); be && (hand == HAND_OFF || hand == HAND_MAIN))
    {
        be->haptic(hand, seconds, 160.f, amplitude);
    }
}

void stop(bool quiet)
{
    if(!state.active)
    {
        return;
    }
    state.active = false;
    highlights::bulletTime(false, 1.f);
    state.cooldown = za::max(0.f, vr_bullettime_cooldown.value);
    if(!quiet)
    {
        playSound(vr_bullettime_sound_off);
        offSound = OffSound{};
        const float most = vr_bullettime_sound_off_max.value;
        if(most > 0.f && vr_bullettime_sound_off.string && vr_bullettime_sound_off.string[0])
        {
            offSound.sfx = S_PrecacheSound(vr_bullettime_sound_off.string);
            offSound.fadeFrom = realtime + static_cast<double>(most);
            offSound.fadeLen = offFade;
        }
    }
    if(vr_debug_bullettime.value)
    {
        Con_Printf("bullet time: off, meter %.2f, cooldown %.1f s\n", state.level, state.cooldown);
    }
}

// A tap or stick press that counts: the tick in both hands and the toggle, unless one counted a moment ago
// (vr_bullettime_trigger_cooldown: a bounce is one).
void trigger(int hand, const char* what)
{
    if(state.triggeredAt >= 0.0 && realtime - state.triggeredAt < za::max(0.f, vr_bullettime_trigger_cooldown.value))
    {
        if(vr_debug_bullettime.value)
        {
            Con_Printf("bullet time: %s ignored (%.2f s after the last)\n", what, realtime - state.triggeredAt);
        }
        return;
    }
    state.triggeredAt = realtime;
    const float haptic = za::clamp(vr_bullettime_haptic.value, 0.f, 2.f);
    if(haptic > 0.f)
    {
        buzz(hand, 0.04f, za::min(1.f, 0.5f * haptic));
        buzz(hands::gadgetHand(), 0.04f, za::min(1.f, 0.3f * haptic));
    }
    if(vr_debug_bullettime.value)
    {
        Con_Printf("bullet time: %s\n", what);
    }
    toggle();
}

// Whether both hands hold one weapon (the off hand on a foregrip, a free grip, a blade grip, a cup).
[[nodiscard]] bool twoHanded()
{
    for(int h = 0; h < HAND_COUNT; ++h)
    {
        if(twohand::helping(h) || twohand::helpKind(h) > 0 || twohand::support(h) > 0.f)
        {
            return true;
        }
    }
    return false;
}

// The tapping hand's striking volume: the hand as drawn (its mesh's vertices: the palm, the back of the hand, the
// knuckles, the fingers and their tips alike; the old models' three spheres along it) and, holding a gun
// (vr_bullettime_tap_butt), its butt (the gun's drawn points within vr_bullettime_tap_butt_depth of its rearmost end),
// holding a prop (a box, a gib, a health pack), its shape; each point with its velocity (m/s, the hand's turn included),
// its radius (units: the old models' spheres; 0 a vertex) and its part (view::HandPart, buttPart or propPart).
struct Striker
{
    glm::vec3 at{0.f};
    glm::vec3 vel{0.f};
    float r{0.f};
    unsigned char part{0};
};
constexpr unsigned char buttPart = 255;
constexpr unsigned char propPart = 254;

[[nodiscard]] const char* partName(unsigned char part)
{
    return part == buttPart   ? "gun's butt"
           : part == propPart ? "held prop"
                              : view::handPartName(static_cast<view::HandPart>(part));
}

// The tap's buffers (the main thread: the view).
struct TapScratch
{
    za::Vector<Striker> strikers;     // (tapScreen)
    za::Vector<glm::vec4> surface;    // the drawn hand's points (strikers)
    za::Vector<view::HandPart> parts; // and their parts
    za::Vector<glm::vec3> butt;       // the held gun's butt's points
    za::Vector<glm::vec3> prop;       // the held prop's points
    auto members() { return mem::list(strikers, surface, parts, butt, prop); }
};
mem::Scratch<TapScratch> scratch{"bullet time"};

void strikers(const hands::State& s, int hand, za::Vector<Striker>& out)
{
    out.clear();
    const float m2u = units::metresToUnits();
    const auto add = [&](const glm::vec3& p, float r, unsigned char part) {
        out.pushBack(Striker{p, s.vel[hand] + glm::cross(s.angVel[hand], (p - s.pos[hand]) / m2u), r, part});
    };
    TapScratch& sc = scratch;
    if(view::drawnHandSurface(hand, sc.surface, &sc.parts))
    {
        for(size_t i = 0; i < sc.surface.size(); i++)
        {
            add(glm::vec3{sc.surface[i]}, sc.surface[i].w, static_cast<unsigned char>(sc.parts[i]));
        }
    }
    else
    {
        add(hands::palmPoint(s, hand), 0.02f * m2u, static_cast<unsigned char>(view::HandPart::Palm)); // (no hand drawn)
    }
    const float depth = za::max(0.f, vr_bullettime_tap_butt_depth.value) * 0.01f * m2u;
    if(vr_bullettime_tap_butt.value != 0.f && view::heldWeaponButtRegion(hand, depth, sc.butt))
    {
        for(const glm::vec3& p : sc.butt)
        {
            add(p, 0.f, buttPart);
        }
    }
    if(view::heldPropSurface(hand, sc.prop))
    {
        for(const glm::vec3& p : sc.prop)
        {
            add(p, 0.f, propPart);
        }
    }
}

// Where a striking point is against the screen: across it (u: its right, w: its up), over it (h: its surface's nearest
// out of it; units), and its velocity against the screen's point under it (the gadget's arm may move and turn too).
struct Against
{
    float u{0.f}, w{0.f}, h{0.f};
    glm::vec3 v{0.f}; // m/s
    float into{0.f};  // its speed into the screen (m/s)
};

[[nodiscard]] Against against(const hands::State& s, const Screen& z, const Striker& k)
{
    const int g = hands::gadgetHand();
    const glm::vec3 rel = k.at - z.centre;
    Against a;
    a.u = glm::dot(rel, z.right);
    a.w = glm::dot(rel, z.up);
    const float centre = glm::dot(rel, z.normal);
    a.h = centre - k.r;
    const glm::vec3 under = k.at - z.normal * centre;
    // (In the player's real time: in slow motion the hands' velocities are the game's, sped up as the world is slowed.)
    const glm::vec3 screenVel = s.vel[g] + glm::cross(s.angVel[g], (under - s.pos[g]) / units::metresToUnits());
    a.v = (k.vel - screenVel) * timescale::handScale();
    a.into = -glm::dot(a.v, z.normal);
    return a;
}

// The frantic-melee guards (his note 2026-10-15: double taps in a melee's thick turned slow motion on by accident). Each
// frame: whether the player's hands are calm enough for a screen tap to count, else why not (in `why`). Not within
// vr_bullettime_tap_blow_quiet real seconds of a blow of his landing, a shove or a bash (the QC's .vr_blow_time), nor
// within calmTime of a hand going faster than vr_bullettime_tap_calm_speed (real m/s, in the play space: walking
// aside): the gadget's hand at all, the tapping one across the screen (against the gadget's hand; its motion into or
// out of the screen is the tap's own). Blows, shoves and bashes need 3 m/s or more; a tap is a hand brought to the
// wrist held still. A double tap's first waiting is dropped meanwhile.
constexpr float calmTime = 0.3f; // s
[[nodiscard]] bool calm(const hands::State& s, int hand, const Screen& z, char* why, int whyLen)
{
    const double now = realtime;
    if(edict_t* player = sv.active && svs.maxclients >= 1 ? svs.clients[0].edict : nullptr)
    {
        const float blow = progs::fieldFloatOr(player, progs::fields().vr_blow_time, 0.f);
        if(blow != state.blowField)
        {
            state.blowField = blow;
            state.blowAt = blow > 0.f ? now : -1e9; // (0: none yet, or a new map's)
        }
        // (Both hands on an enemy or a ragdoll: fighting, pushing, shoving it, the gadget knocked in the scuffle.)
        // (The client's frame: the server's VM pushed for its ragdoll holds.)
        bool held[2] = {foegrab::holding(0), foegrab::holding(1)};
        if(vr_bullettime_tap_both_held.value != 0.f && (!held[0] || !held[1]) && sv.qcvm.edicts)
        {
            qcvm_t* oldVm = nullptr;
            PR_PushQCVM(&sv.qcvm, &oldVm);
            for(int h = 0; h < 2; h++)
            {
                held[h] = held[h] || box3d::ragdollHeld(player, h) > 0;
            }
            PR_PopQCVM(oldVm);
        }
        const bool both = vr_bullettime_tap_both_held.value != 0.f && held[0] && held[1];
        if(both != state.bothHeld && vr_debug_bullettime.value)
        {
            Con_Printf("bullet time: screen taps %s\n", both ? "off: both hands hold an enemy or a body" : "on again");
        }
        state.bothHeld = both;
        if(both)
        {
            q_snprintf(why, static_cast<size_t>(whyLen), "both hands hold an enemy or a body");
            return false;
        }
    }
    const float limit = vr_bullettime_tap_calm_speed.value;
    if(limit > 0.f)
    {
        const int g = hands::gadgetHand();
        const float scale = timescale::handScale(); // (in slow motion the hands' velocities are the game's)
        const glm::vec3 rel = s.vel[hand] - s.vel[g];
        const float speed[2] = {glm::length(s.vel[g]) * scale, glm::length(rel - z.normal * glm::dot(rel, z.normal)) * scale};
        const int who[2] = {g, hand};
        for(int i = 0; i < 2; i++)
        {
            if(speed[i] > limit)
            {
                state.fastAt = now;
                state.fastHand = who[i];
                state.fastSpeed = speed[i];
            }
        }
    }
    const float quiet = za::max(0.f, vr_bullettime_tap_blow_quiet.value);
    if(now - state.blowAt < static_cast<double>(quiet))
    {
        q_snprintf(why, static_cast<size_t>(whyLen), "%.2f s after a blow, shove or bash", now - state.blowAt);
        return false;
    }
    if(limit > 0.f && now - state.fastAt < static_cast<double>(calmTime))
    {
        q_snprintf(why, static_cast<size_t>(whyLen), "the %s hand at %.1f m/s %.2f s ago", state.fastHand == hands::gadgetHand() ? "gadget's" : "tapping",
            state.fastSpeed, now - state.fastAt);
        return false;
    }
    return true;
}

// The screen tap: the other hand's striking volume (strikers) over the gadget's screen (a point within
// vr_bullettime_tap_margin of its edges), having come straight into it (within vr_bullettime_tap_angle of its normal) at
// vr_bullettime_tap_speed or more (against the screen's own motion: both may move), then stopped on it (a point within
// vr_bullettime_tap_depth of its face, the fastest point's speed into it down to vr_bullettime_tap_stop of the peak
// within vr_bullettime_tap_window seconds: an impact; a hand passing by keeps its speed). A swing across the screen (the
// melee) comes from the side; resting, brushing or soft touches are too slow; hands moving together (a two-handed hold)
// have no speed towards each other, and are ignored anyway unless vr_bullettime_tap_twohanded. After a tap counts, the
// volume must lift off the face before the next can (a bounce, or a hand left resting, is never a second tap). With
// vr_bullettime_tap_gesture 1 (Double Tap) it takes two taps, each at vr_bullettime_tap_double_speed or more, the second
// within vr_bullettime_tap_double_window of the first but no sooner than vr_bullettime_tap_min_gap; one alone does nothing.
void tapScreen(const hands::State& s, int hand, const Screen& z)
{
    char why[96];
    const bool restless = !calm(s, hand, z, why, static_cast<int>(sizeof(why)));
    if(restless && state.firstTapAt >= 0.0)
    {
        if(vr_debug_bullettime.value)
        {
            Con_Printf("bullet time: a double tap's first dropped (%s)\n", why);
        }
        state.firstTapAt = -1.0;
    }
    const bool allowed = (vr_bullettime_tap_holding.value != 0.f || held::handEmpty(hand)) &&
                         (vr_bullettime_tap_twohanded.value != 0.f || !twoHanded());
    if(!allowed)
    {
        if(vr_debug_bullettime.value >= 2.f)
        {
            Con_Printf("bullet time: tap off (%s)\n", twoHanded() ? "two-handed" : "holding");
        }
        state.tapPeak = 0.f;
        return;
    }
    const float cm = 0.01f * units::metresToUnits();
    const float margin = za::max(0.f, vr_bullettime_tap_margin.value) * cm;
    const float depth = za::max(0.5f, vr_bullettime_tap_depth.value) * cm;
    const bool twice = vr_bullettime_tap_gesture.value >= 1.f;
    const float least = za::max(0.05f, twice ? vr_bullettime_tap_double_speed.value : vr_bullettime_tap_speed.value);
    if(state.firstTapAt >= 0.0 && (!twice || realtime - state.firstTapAt > za::max(0.05f, vr_bullettime_tap_double_window.value)))
    {
        if(twice && vr_debug_bullettime.value)
        {
            Con_Printf("bullet time: no second tap within %.2f s\n", vr_bullettime_tap_double_window.value);
        }
        state.firstTapAt = -1.0;
    }
    const float cosMost = za::cos(glm::radians(za::clamp(vr_bullettime_tap_angle.value, 1.f, 89.f)));
    const auto over = [&](const Against& a) {
        return za::fabs(a.u) <= z.halfSize.x + margin && za::fabs(a.w) <= z.halfSize.y + margin;
    };
    za::Vector<Striker>& strike = scratch.strikers;
    strikers(s, hand, strike);
    const int count = static_cast<int>(strike.size());
    if(state.tapPeak > 0.f && state.tapStriker >= count)
    {
        state.tapPeak = 0.f; // (the gun let go of)
    }
    int nearest = -1;  // the point over the screen nearest its face (or furthest through it)
    int touching = -1; // and the nearest on it: within the depth over it, or a little through it
    Against nearestAt, touchingAt;
    float buttH = 1e30f; // (vr_debug_bullettime 2: the gun's butt's nearest)
    for(int i = 0; i < count; ++i)
    {
        const Against a = against(s, z, strike[i]);
        if(!over(a))
        {
            continue;
        }
        if(nearest < 0 || a.h < nearestAt.h)
        {
            nearest = i;
            nearestAt = a;
        }
        if(strike[i].part == buttPart)
        {
            buttH = za::min(buttH, a.h);
        }
        if(a.h <= depth && a.h >= -2.f * depth && (touching < 0 || a.h < touchingAt.h))
        {
            touching = i;
            touchingAt = a;
        }
        // Coming at it: from a little above it to just through it, straight and fast enough.
        const float speed = glm::length(a.v);
        const bool coming = a.h <= depth + 12.f * cm && a.h >= -2.f * depth;
        if(coming && state.tapLifted && a.into >= least && a.into >= cosMost * speed && a.into >= state.tapPeak)
        {
            state.tapPeak = a.into;
            state.tapPeakAt = realtime;
            state.tapStriker = i;
        }
    }
    // Lifted: off the face, or drawn back from it (0.3 m/s or more out of it: a hand tapping twice quickly needn't clear
    // the whole depth; the fingers may hang past the screen's face while the palm taps).
    if(!state.tapLifted && (touching < 0 || touchingAt.into <= -0.3f))
    {
        state.tapLifted = true; // (the next tap may come)
    }
    if(vr_debug_bullettime.value >= 2.f && nearest >= 0 && nearestAt.h < depth + 20.f * cm && nearestAt.h > -2.f * depth)
    {
        char butt[48] = "";
        if(buttH < 1e29f)
        {
            q_snprintf(butt, sizeof(butt), "; the gun's butt %.1f cm", buttH / cm);
        }
        Con_Printf("bullet time: tap %s %.1f cm over the screen (%.1f, %.1f), into it %.2f m/s of %.2f, peak %.2f (%d "
                   "points%s)\n",
            partName(strike[nearest].part), nearestAt.h / cm, nearestAt.u / cm, nearestAt.w / cm, nearestAt.into,
            glm::length(nearestAt.v), state.tapPeak, count, butt);
    }
    if(state.tapPeak <= 0.f)
    {
        return;
    }
    if(realtime - state.tapPeakAt > za::max(0.f, vr_bullettime_tap_window.value))
    {
        if(vr_debug_bullettime.value)
        {
            Con_Printf("bullet time: a tap at %.2f m/s never stopped on the screen\n", state.tapPeak);
        }
        state.tapPeak = 0.f; // came at it, but never stopped there
        return;
    }
    // The impact: the volume on the face (a point within the depth over it, or a little through it), the fastest point
    // slowed.
    const Against a = against(s, z, strike[state.tapStriker]);
    if(touching >= 0 && a.into <= state.tapPeak * za::clamp(vr_bullettime_tap_stop.value, 0.f, 1.f) && restless)
    {
        // A melee's thick (calm()): no tap, no feedback; the volume must lift off before the next.
        if(vr_debug_bullettime.value)
        {
            Con_Printf("bullet time: a tap at %.2f m/s ignored (%s)\n", state.tapPeak, why);
        }
        state.tapPeak = 0.f;
        state.tapLifted = false;
        return;
    }
    if(touching >= 0 && a.into <= state.tapPeak * za::clamp(vr_bullettime_tap_stop.value, 0.f, 1.f))
    {
        // What struck: the part furthest in, and the others within a centimetre of it (a flat slap lands the palm and the
        // fingers together).
        unsigned char struck[3] = {strike[touching].part, 0, 0};
        int parts = 1;
        for(int i = 0; i < count && parts < 3; ++i)
        {
            const Against b = against(s, z, strike[i]);
            bool known = false;
            for(int j = 0; j < parts; j++)
            {
                known = known || struck[j] == strike[i].part;
            }
            if(!known && over(b) && b.h <= touchingAt.h + cm && b.h >= -2.f * depth)
            {
                struck[parts++] = strike[i].part;
            }
        }
        char what[160];
        // A second tap sooner than vr_bullettime_tap_min_gap after the first (an arm wiggled or flicked: two contacts
        // in a blink) is no double tap: it counts as a new first one.
        const bool tooSoon = twice && state.firstTapAt >= 0.0 &&
                             realtime - state.firstTapAt < za::max(0.f, vr_bullettime_tap_min_gap.value);
        const bool second = twice && state.firstTapAt >= 0.0 && !tooSoon;
        q_snprintf(what, sizeof(what), "%sscreen tapped by the %s%s%s%s%s%s at %.2f m/s", second ? "double tap: " : "",
            partName(struck[0]), parts > 1 ? " (and the " : "", parts > 1 ? partName(struck[1]) : "",
            parts > 2 ? ", the " : "", parts > 2 ? partName(struck[2]) : "", parts > 1 ? ")" : "", state.tapPeak);
        state.tapPeak = 0.f;
        state.tappedAt = realtime;
        state.tapLifted = false;
        if(twice && !second)
        {
            // A double tap's first: a faint tick in the tapping hand, then the second must come in time.
            state.firstTapAt = realtime;
            buzz(hand, 0.02f, za::min(1.f, 0.2f * za::clamp(vr_bullettime_haptic.value, 0.f, 2.f)));
            gadget::tapFeedback(false); // a click and a glitch of the screen
            if(vr_debug_bullettime.value)
            {
                Con_Printf("bullet time: first tap%s (%s); the second within %.2f s\n", tooSoon ? " (again: the last too soon)" : "",
                    what, vr_bullettime_tap_double_window.value);
            }
            return;
        }
        state.firstTapAt = -1.0;
        gadget::tapFeedback(true); // the activation's click and glitch (as the tap registers, whether it then starts)
        trigger(hand, what);
    }
}

// vr_bullettime_trigger's stick: HAND_OFF (the left controller) for 1, HAND_MAIN (the right) for 2; -1: the gadget.
[[nodiscard]] int stickHand()
{
    const int t = static_cast<int>(vr_bullettime_trigger.value);
    return t == 1 ? HAND_OFF : t == 2 ? HAND_MAIN : -1;
}

// vr_bullettime: as the screen tap.
void bullettime_f()
{
    if(!inGame())
    {
        Con_Printf("vr_bullettime: needs vr_bullettime_enabled 1 and a single-player game\n");
        return;
    }
    toggle();
}

// vr_bullettime_tap_feedback_test [1]: a screen tap's click and glitch, as a double tap's first (or with 1, the tap that
// starts or stops it), without tapping.
void tapFeedbackTest_f()
{
    gadget::tapFeedback(Cmd_Argc() > 1 && atoi(Cmd_Argv(1)) != 0);
}

// vr_bullettime_meter [level]: prints the meter (and how it fills), or sets it (0 .. 1; a test's empty meter).
void meter_f()
{
    if(Cmd_Argc() > 1)
    {
        state.level = za::clamp(static_cast<float>(atof(Cmd_Argv(1))), 0.f, 1.f);
        writeMeter();
        Con_Printf("bullet time meter set to %.3f\n", state.level);
        return;
    }
    Con_Printf("bullet time meter: %.3f (%s, %s, cooldown %.1f s; fills %s)\n", state.level,
               state.active ? "running" : "off", meterPlayer() && progs::fields().vr_bullettime_spent >= 0 ? "kept" : "not kept",
               state.cooldown,
               earnedMode() ? va("by %g damage dealt (skill %d)", earnDamage(), static_cast<int>(skill.value + 0.5f))
                            : va("over %g s", za::max(0.1f, vr_bullettime_recharge.value)));
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_bullettime", bullettime_f);
    Cmd_AddCommand("vr_bullettime_tap_feedback_test", tapFeedbackTest_f);
    Cmd_AddCommand("vr_bullettime_meter", meter_f);
}

bool ownsStick(int hand)
{
    return hand == stickHand() && vr_bullettime_enabled.value != 0.f;
}

bool stickPress(int hand, bool now)
{
    if(!now)
    {
        const bool taken = state.stickTaken[hand];
        state.stickTaken[hand] = false;
        return taken;
    }
    if(hand != stickHand() || key_dest != key_game || vr_bullettime_enabled.value == 0.f)
    {
        return false;
    }
    state.stickTaken[hand] = true;
    if(inGame())
    {
        trigger(hand, hand == HAND_OFF ? "left stick press" : "right stick press");
    }
    return true;
}

void toggle()
{
    if(!inGame())
    {
        return;
    }
    if(state.active)
    {
        stop(false);
        return;
    }
    if(state.cooldown > 0.f || state.level < za::clamp(vr_bullettime_min.value, 0.01f, 1.f))
    {
        cutOffSound(offCut);
        playSound(vr_bullettime_sound_denied);
        if(earnedMode() && state.level < za::clamp(vr_bullettime_min.value, 0.01f, 1.f))
        {
            emptyNotice(); // (not for a cooldown alone: the meter is full enough, it only waits)
        }
        if(vr_debug_bullettime.value)
        {
            Con_Printf("bullet time: not ready (meter %.2f, cooldown %.1f s)\n", state.level, state.cooldown);
        }
        return;
    }
    state.active = true;
    highlights::bulletTime(true, scale());
    cutOffSound(offCut);
    playSound(vr_bullettime_sound_on);
    if(vr_debug_bullettime.value)
    {
        Con_Printf("bullet time: on at %.2fx, meter %.2f (%.1f s)\n", scale(), state.level,
                   state.level * za::max(0.1f, vr_bullettime_duration.value));
    }
}

void advance(double dt)
{
    const float fadeTime = 0.25f;
    fadeOffSound();
    if(!sv.active)
    {
        state = State{};
        return;
    }
    if(!inGame())
    {
        stop(true);
    }
    readMeter();
    earn();
    const float step = static_cast<float>(dt);
    const bool running = key_dest == key_game && !runtimeMenuPaused(); // a menu open (the runtime's too): the meter waits
    if(running && state.active)
    {
        state.level -= step / za::max(0.1f, vr_bullettime_duration.value);
        if(state.level <= 0.f)
        {
            state.level = 0.f;
            stop(false);
        }
    }
    else if(running && state.cooldown > 0.f)
    {
        state.cooldown = za::max(0.f, state.cooldown - step);
    }
    else if(running && !earnedMode())
    {
        state.level = za::min(1.f, state.level + step / za::max(0.1f, vr_bullettime_recharge.value));
    }
    writeMeter();
    const float target = state.active ? 1.f : 0.f;
    const float most = step / fadeTime;
    state.look = state.look < target ? za::min(target, state.look + most) : za::max(target, state.look - most);
}

void frame()
{
    // vr_debug_gadget_button 3: where the tapping hand was from the gadget's screen at the frame's start, as the tap was
    // tested before it moved to the view (the gadget a frame old there): the view prints how far that was off.
    const hands::State& s = hands::current();
    Screen z;
    state.startValid = vr_debug_gadget_button.value >= 3.f && s.valid && screen(z);
    if(state.startValid)
    {
        state.startOffset = z.centre - s.pos[1 - hands::gadgetHand()];
    }
}

void viewFrame(const hands::State& s)
{
    for(int h = 0; h < HAND_COUNT; h++)
    {
        state.viewPos[h] = s.pos[h];
    }
    state.viewValid = s.valid;
    Screen z;
    // A stick press starts it instead (vr_bullettime_trigger): the screen tap does nothing.
    // (An app on the gadget's screen takes its touches: the tap is off meanwhile, vr_touch.hpp.)
    if(!inGame() || key_dest != key_game || !s.valid || stickHand() >= 0 || vr_bullettime_tap.value == 0.f || !tapZone(z) ||
        touch::takesGadgetScreen())
    {
        state.tapPeak = 0.f;
        return;
    }
    tapScreen(s, 1 - hands::gadgetHand(), z);
}

bool frameStartOffset(glm::vec3& out)
{
    out = state.startOffset;
    return state.startValid;
}

glm::vec3 movedSinceView(const hands::State& s, int hand)
{
    return state.viewValid && (hand == 0 || hand == 1) ? s.pos[hand] - state.viewPos[hand] : glm::vec3{0.f};
}

bool tapZone(Screen& out)
{
    if(!screen(out))
    {
        return false;
    }
    const float cm = 0.01f * units::metresToUnits();
    out.centre += (out.right * vr_bullettime_tap_x.value + out.up * vr_bullettime_tap_y.value +
                      out.normal * vr_bullettime_tap_z.value) * cm;
    out.halfSize *= glm::vec2{za::max(0.f, vr_bullettime_tap_width.value), za::max(0.f, vr_bullettime_tap_height.value)};
    return true;
}

void strikingVolume(const hands::State& s, int hand, za::Vector<glm::vec4>& out, za::Vector<unsigned char>* parts)
{
    za::Vector<Striker>& k = scratch.strikers;
    strikers(s, hand, k);
    out.clear();
    if(parts)
    {
        parts->clear();
    }
    for(const Striker& p : k)
    {
        out.pushBack(glm::vec4{p.at, p.r});
        if(parts)
        {
            parts->pushBack(p.part);
        }
    }
}

bool screen(Screen& out)
{
    const gadget::Pose& gp = gadget::pose();
    if(!gadget::active() || !gp.valid)
    {
        return false;
    }
    glm::vec3 corner;
    glm::vec2 size;
    gadget::screenRect(corner, size);
    const glm::vec3 mid{corner.x + size.x * 0.5f, corner.y + size.y * 0.5f, corner.z};
    out.centre = gp.origin + gp.axes * (mid * gp.scale);
    out.right = gp.axes[0];
    out.up = gp.axes[1];
    out.normal = gp.axes[2];
    out.halfSize = size * (0.5f * gp.scale);
    return true;
}

bool tapping()
{
    return state.tapPeak > 0.f || (state.tappedAt >= 0.0 && realtime - state.tappedAt < 0.5);
}

bool tapHandTarget(int hand, float cm, float sideCm, const char* part, glm::vec3& out)
{
    Screen z;
    const hands::State& s = hands::current();
    if(!s.valid || !tapZone(z) || (hand != 0 && hand != 1))
    {
        return false;
    }
    const float u = 0.01f * units::metresToUnits();
    const glm::vec3 to = z.centre + z.normal * (cm * u) + z.right * (sideCm * u);
    // The gadget and the striking point as the view drew them last (the hand moved on since with the player: the move
    // asked of it is from there).
    glm::vec3 from{0.f};
    if(!part || !q_strcasecmp(part, "palm"))
    {
        from = hands::palmPoint(s, hand) - movedSinceView(s, hand);
    }
    else if(!q_strcasecmp(part, "butt"))
    {
        if(!view::heldWeaponButt(hand, from))
        {
            return false;
        }
    }
    else
    {
        // The point of that part of the drawn hand furthest towards the screen (the one that strikes first).
        struct Key
        {
            const char* name;
            view::HandPart part;
        };
        constexpr Key keys[] = {{"palmskin", view::HandPart::Palm}, {"back", view::HandPart::Back},
            {"knuckles", view::HandPart::Knuckles}, {"fingers", view::HandPart::Fingers},
            {"tips", view::HandPart::Fingertips}, {"thumb", view::HandPart::Thumb}};
        const Key* key = nullptr;
        for(const Key& k : keys)
        {
            key = !q_strcasecmp(k.name, part) ? &k : key;
        }
        za::Vector<glm::vec4>& pts = scratch.surface;
        za::Vector<view::HandPart>& parts = scratch.parts;
        if(!key || !view::drawnHandSurface(hand, pts, &parts))
        {
            return false;
        }
        float best = 1e30f;
        for(size_t i = 0; i < pts.size(); i++)
        {
            const float h = glm::dot(glm::vec3{pts[i]}, z.normal) - pts[i].w;
            if(parts[i] == key->part && h < best)
            {
                best = h;
                from = glm::vec3{pts[i]} - z.normal * pts[i].w;
            }
        }
        if(best > 1e29f)
        {
            return false;
        }
    }
    out = s.pos[hand] + (to - from);
    return true;
}

float scale()
{
    return state.active ? za::clamp(vr_bullettime_scale.value, 0.05f, 1.f) : 1.f;
}

bool sandevistan()
{
    return state.active && vr_bullettime_sandevistan.value != 0.f;
}

Meter meter()
{
    Meter m;
    m.enabled = vr_bullettime_enabled.value != 0.f && sv.active && svs.maxclients == 1;
    m.active = state.active;
    m.cooling = !state.active && state.cooldown > 0.f;
    m.ready = !state.active && !m.cooling && state.level >= za::clamp(vr_bullettime_min.value, 0.01f, 1.f);
    m.level = state.level;
    m.earned = earnedMode();
    m.gained = realtime - state.earnedAt < 0.4;
    return m;
}

Look look()
{
    Look l;
    l.strength = state.look * za::clamp(vr_bullettime_fx.value, 0.f, 1.f);
    l.desaturate = za::clamp(vr_bullettime_fx_desat.value, 0.f, 1.f);
    l.vignette = za::clamp(vr_bullettime_fx_vignette.value, 0.f, 1.f);
    float r = 1.f, g = 1.f, b = 1.f;
    if(std::sscanf(vr_bullettime_fx_tint.string, "%f %f %f", &r, &g, &b) == 3)
    {
        l.tint = glm::clamp(glm::vec3{r, g, b}, glm::vec3{0.f}, glm::vec3{2.f});
    }
    return l;
}

} // namespace qvr::bullettime
