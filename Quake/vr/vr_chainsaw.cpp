// vr_chainsaw.cpp -- see vr_chainsaw.hpp.

#include "vr_chainsaw.hpp"
#include "vr_coil.hpp"
#include "vr_cvars.hpp"
#include "vr_flashlight.hpp"
#include "vr_gfx.hpp"
#include "vr_grip.hpp"
#include "vr_held.hpp"
#include "vr_lines.hpp"
#include "vr_mem.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"
#include "vr_protocol.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <string.h>

namespace qvr::chainsaw
{
namespace
{

constexpr const char* modelName = "progs/v_chainsaw.mdl";
constexpr int pulledFrame = 9;           // the model's frame without the handle (QC VR_SAW_PULLED_FRAME)
constexpr float reach = 0.09f;           // metres from the handle's middle a fist takes it within
constexpr float cordLength = 0.4f;       // metres the cord goes out past the pull's distance: then the hand lets go
constexpr float rearm = 0.35f;           // the next pull once the hand is back within this share of the pull's distance
constexpr double pullLatch = 0.15;       // seconds the move says "pulled" (QC reads its edge once a server frame)
constexpr const char* weakPullSound = "vr/saw_pull_weak.wav"; // (QC precaches it too: loaded with the map)
constexpr double retractTime = 0.12;     // seconds the handle flies back into its seat
constexpr float handleHalf = 0.045f;     // metres, half the T-handle's length (in the fist)
constexpr float handleRadius = 0.012f;   // metres
constexpr int wpnFlagSawRunning = 4;     // QC QVR_WPNFLAG_SAW_RUNNING: the engine runs
constexpr int wpnFlagSawChain = 8;       // QC QVR_WPNFLAG_SAW_CHAIN: the chain runs
// The engine's exhaust (model space: the right side of the engine block, ahead of the cord's handle; QC VR_SAW_EXHAUST)
// and the way its smoke leaves it (out to the right, a little up).
constexpr glm::vec3 exhaustPoint{25.f, -9.f, 2.f};
constexpr glm::vec3 exhaustOut{0.f, -1.f, 0.4f};
constexpr float shakeDegPerMm = 0.2f;    // the hand's turn as it shakes, degrees a mm of vr_chainsaw_shake
constexpr float shakeChain = 1.6f;       // harder with the chain running
constexpr float weakPullShare = 0.5f;    // a weak pull's feedback (shake, smoke, sparks) against a good one's
constexpr za::U8 bitOffHolds = 1;  // QC VR_SAWCORD_OFF_HOLDS: the off hand holds the main hand's chainsaw's cord
constexpr za::U8 bitMainHolds = 2; // VR_SAWCORD_MAIN_HOLDS
constexpr za::U8 bitOffPulled = 4; // VR_SAWCORD_OFF_PULLED: that cord pulled
constexpr za::U8 bitMainPulled = 8;

// The handle, read from the model (model space).
struct Handle
{
    const qmodel_t* model{nullptr};
    const void* data{nullptr}; // its alias header (read again after vr_model_reload)
    bool valid{false};
    glm::vec3 seat{0.f}; // the handle's middle, seated
    glm::vec3 hole{0.f}; // the cord's hole
    float barStart{0.f}; // x from which the model's vertices are its bar
};
Handle handle;

struct State
{
    int holder{-1};           // the hand holding a cord (the chainsaw is in the other), -1 none
    bool armed{true};         // a pull may come (the hand came back since the last)
    double pulledUntil{-1.0}; // the move says "pulled" until then (realtime)
    int pulledBy{-1};         // which hand pulled
    float peakSpeed{0.f};     // m/s along the cord, the most in the last frames (a crossing between frames)
    double retractFrom{-1.0}; // the handle flying back since then (realtime; -1 not)
    int retractHand{-1};      // the chainsaw's hand it flies back to
    glm::vec3 retractStart{0.f};
    glm::vec3 handlePos{0.f}, handleAxis{0.f, 0.f, 1.f}; // drawn this frame (world)
    bool drawHandle{false};
    glm::vec3 seatWorld[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // each hand's chainsaw's handle seat, as drawn last frame
    bool sawDrawn[2]{false, false};
    bool gripTaken[2]{false, false}; // a grip press the cord took (its release is taken too)
    int lastFrame{-1};               // the frame setupView last ran in (once a frame)
    double smokeTime{-1.0};          // cl.time the exhaust's smoke was last made at (-1: not yet)
    float smokeDue[2]{0.f, 0.f};     // each hand's chainsaw's puffs due (a share of one carried on)
    double shakePrinted{-1.0};       // vr_debug_chainsaw's last shake line (realtime)
    double pullKickAt[2]{-1.0, -1.0}; // each hand's chainsaw's last pull's kick (realtime; -1 none): it shakes after it
    float pullKick[2]{0.f, 0.f};      // how hard (1 a good pull, weakPullShare a weak one)
};
State st;

coil::Cord cord;

struct CordDraw
{
    gfx::TubeBatch cord, handle;
    int sides{0};
    int builtFrame{-1};
};
CordDraw cordDraw;

struct Scratch
{
    za::Vector<gfx::TubeRing> rings;
    za::Vector<gfx::TubeRing> handleRings;
    auto members() { return qvr::mem::list(rings, handleRings); }
};
mem::Scratch<Scratch> scratch{"chainsaw"};

[[nodiscard]] bool isChainsaw(const qmodel_t* m)
{
    return m && !strcmp(m->name, modelName);
}

// The handle from the model: its vertices moving between frames 0 and 9.
const Handle& handleOf(const qmodel_t* model)
{
    const auto* hdr = model && model->type == mod_alias
                          ? static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)))
                          : nullptr;
    if(handle.model == model && handle.data == hdr)
    {
        return handle;
    }
    handle = Handle{};
    handle.model = model;
    handle.data = hdr;
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || hdr->numframes <= pulledFrame)
    {
        Con_DPrintf("chainsaw: %s has no pulled frame: no cord\n", model ? model->name : "(none)");
        return handle;
    }
    const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const trivertx_t* seated = base + static_cast<za::SizeT>(hdr->frames[0].firstpose) * hdr->numverts;
    const trivertx_t* pulled = base + static_cast<za::SizeT>(hdr->frames[pulledFrame].firstpose) * hdr->numverts;
    const auto at = [&](const trivertx_t& t) {
        return glm::vec3{t.v[0] * hdr->scale[0] + hdr->scale_origin[0], t.v[1] * hdr->scale[1] + hdr->scale_origin[1],
            t.v[2] * hdr->scale[2] + hdr->scale_origin[2]};
    };
    glm::vec3 seat{0.f}, hole{0.f};
    int n = 0;
    float lo = 1e9f, hi = -1e9f;
    for(int v = 0; v < hdr->numverts; v++)
    {
        const glm::vec3 a = at(seated[v]);
        lo = za::min(lo, a.x);
        hi = za::max(hi, a.x);
        if(memcmp(seated[v].v, pulled[v].v, 3) != 0)
        {
            seat += a;
            hole += at(pulled[v]);
            n++;
        }
    }
    if(n == 0)
    {
        return handle;
    }
    handle.valid = true;
    handle.seat = seat / static_cast<float>(n);
    handle.hole = hole / static_cast<float>(n);
    handle.barStart = lo + (hi - lo) * 0.58f; // (make_chainsaw.py: the bar out of the engine block)
    Con_DPrintf("chainsaw: %s: the cord's handle (%d vertices) at %.2f %.2f %.2f, its hole at %.2f %.2f %.2f\n", model->name, n,
        handle.seat.x, handle.seat.y, handle.seat.z, handle.hole.x, handle.hole.y, handle.hole.z);
    return handle;
}

// The chainsaw drawn in `hand` this frame, or null.
[[nodiscard]] const view::ViewEntity* sawIn(int hand)
{
    const view::ViewEntity* ve = view::heldWeapon(hand);
    return ve && isChainsaw(ve->ent.model) ? ve : nullptr;
}

// The server's weapon flags of `hand`'s weapon (QVR_WPNFLAG_*).
[[nodiscard]] int flagsOf(int hand)
{
    return cl.stats[hand == HAND_MAIN ? protocol::STAT_QVR_WEAPONFLAGS : protocol::STAT_QVR_WEAPONFLAGS2];
}

// The fuel in `hand`'s chainsaw (QC keeps it in the weapon's clip, a fraction of a percent too).
[[nodiscard]] float fuelOf(int hand)
{
    const int stat = hand == HAND_MAIN ? protocol::STAT_QVR_WEAPONCLIP : protocol::STAT_QVR_WEAPONCLIP2;
    return za::max(cl.statsf[stat], static_cast<float>(cl.stats[stat]));
}

// The exhaust of the chainsaw drawn as `ve` (world) and the way its smoke leaves it (a unit vector).
void exhaustOf(const view::ViewEntity& ve, glm::vec3& at, glm::vec3& dir)
{
    at = view::modelPoint(ve, exhaustPoint);
    const glm::vec3 out = view::modelPoint(ve, exhaustPoint + exhaustOut) - at;
    const float len = glm::length(out);
    dir = len > 1e-4f ? out / len : glm::vec3{0.f, 0.f, 1.f};
}

// An engine's buzz on one axis (-1..1): three fast sines, out of step on each axis and hand (a running engine's
// vibration, not a tired arm's slow tremor: vr_fatigue.cpp).
[[nodiscard]] float buzz(float t, int hand, int axis)
{
    constexpr float freq[3]{19.f, 29.f, 43.f};
    constexpr float share[3]{0.5f, 0.3f, 0.2f};
    float n = 0.f;
    for(int k = 0; k < 3; k++)
    {
        const float f = freq[k] * (1.f + 0.047f * static_cast<float>(axis) + 0.089f * static_cast<float>(hand));
        const float phase = 1.7f * static_cast<float>(hand) + 2.3f * static_cast<float>(axis) + 0.9f * static_cast<float>(k);
        n += share[k] * za::sin(glm::two_pi<float>() * f * t + phase);
    }
    return n;
}

// The running chainsaws' exhaust smoke in the hands (vr_chainsaw_smoke puffs a second), once a frame; the one lying
// running is the server's (QC VR_Saw_PropThink: Preset::ChainsawSmoke).
void smokeFrame()
{
    const double now = cl.time;
    const float dt = st.smokeTime < 0.0 ? 0.f : static_cast<float>(za::clamp(now - st.smokeTime, 0.0, 0.1));
    st.smokeTime = now;
    const float rate = za::max(0.f, vr_chainsaw_smoke.value);
    for(int h = 0; h < 2; h++)
    {
        const view::ViewEntity* ve = st.sawDrawn[h] && (flagsOf(h) & wpnFlagSawRunning) ? sawIn(h) : nullptr;
        if(!ve || rate <= 0.f)
        {
            st.smokeDue[h] = 0.f;
            continue;
        }
        st.smokeDue[h] += rate * dt;
        const int n = static_cast<int>(st.smokeDue[h]);
        if(n <= 0)
        {
            continue;
        }
        st.smokeDue[h] -= static_cast<float>(n);
        glm::vec3 at, dir;
        exhaustOf(*ve, at, dir);
        particles::chainsawSmoke(at, dir, n);
        if(vr_debug_chainsaw.value)
        {
            Con_Printf("chainsaw: %s hand's exhaust: %d puff(s) at %.1f %.1f %.1f\n", h == HAND_MAIN ? "main" : "off", n,
                at.x, at.y, at.z);
        }
    }
}

// A pull of the cord of `sawHand`'s chainsaw (`share` 1 a good pull, weakPullShare a weak one; NOTES.md
// vrfiringrange_2026-10-01_22-54-54): not running, with fuel, the engine turns over: the hand shakes a moment
// (vr_chainsaw_pull_shake, in shake), a puff of exhaust smoke (vr_chainsaw_pull_smoke) and a few sparks
// (vr_chainsaw_pull_sparks, a casing's: particles::shellEject). Running or with an empty tank, nothing (the sound only).
void pullFeedback(int sawHand, float share)
{
    const view::ViewEntity* ve = sawIn(sawHand);
    const float fuel = fuelOf(sawHand);
    if(!ve || (flagsOf(sawHand) & wpnFlagSawRunning) || fuel <= 0.f)
    {
        if(vr_debug_chainsaw.value)
        {
            Con_Printf("chainsaw: %s hand's pull: no feedback (%s)\n", sawHand == HAND_MAIN ? "main" : "off",
                !ve ? "not drawn" : fuel <= 0.f ? "no fuel" : "running");
        }
        return;
    }
    st.pullKickAt[sawHand] = realtime;
    st.pullKick[sawHand] = share;
    glm::vec3 at, dir;
    exhaustOf(*ve, at, dir);
    const int puffs = static_cast<int>(za::ceil(za::max(0.f, vr_chainsaw_pull_smoke.value) * share));
    const int sparks = static_cast<int>(za::ceil(za::max(0.f, vr_chainsaw_pull_sparks.value) * share));
    particles::chainsawSmoke(at, dir, puffs);
    if(sparks > 0)
    {
        particles::shellEject(at, dir, 0.f, sparks); // no smoke of its own: only its sparks
    }
    if(vr_debug_chainsaw.value)
    {
        Con_Printf("chainsaw: %s hand's pull feedback: %.2f, fuel %.1f, %d puff(s), %d spark(s)\n",
            sawHand == HAND_MAIN ? "main" : "off", share, fuel, puffs, sparks);
    }
}

// The shake left of `hand`'s chainsaw's last pull (mm, dying out over vr_chainsaw_pull_shake_time), 0 none.
[[nodiscard]] float pullShakeMm(int hand)
{
    const float time = vr_chainsaw_pull_shake_time.value;
    const float since = static_cast<float>(realtime - st.pullKickAt[hand]);
    if(st.pullKickAt[hand] < 0.0 || time <= 0.f || since < 0.f || since >= time)
    {
        return 0.f;
    }
    const float left = 1.f - since / time;
    return za::max(0.f, vr_chainsaw_pull_shake.value) * st.pullKick[hand] * left * left;
}

// Where a fist closes (its grip channel's middle, world) and the channel's direction.
void fistOf(const hands::State& s, int hand, glm::vec3& point, glm::vec3& dir)
{
    const grip::HandFrame f = grip::handFrame(hand, hand == HAND_OFF, true);
    const glm::mat3 axes = held::axesFromAngles(&s.rot[hand][0], true);
    point = s.pos[hand] + axes * f.channelPoint;
    dir = glm::normalize(axes * f.channelDir);
}

[[nodiscard]] float metres(float m)
{
    return m * units::metresToUnits();
}

void letGo(int sawHand, const glm::vec3& from)
{
    st.retractFrom = realtime;
    st.retractHand = sawHand;
    st.retractStart = from;
    st.holder = -1;
    st.armed = true;
    st.peakSpeed = 0.f;
}

void debugLog(const char* what, int hand, float ext, float speed)
{
    if(vr_debug_chainsaw.value)
    {
        Con_Printf("chainsaw cord (%s hand): %s, out %.1f cm, %.2f m/s along it\n", hand == HAND_MAIN ? "main" : "off", what,
            ext / units::metresToUnits() * 100.f, speed);
    }
}

// vr_chainsaw_fit: where the main hand's fist is in the chainsaw's model space (placing the model's grip there).
void fit_f()
{
    const hands::State& s = hands::current();
    view::WeaponFrame wf;
    if(!s.valid || !view::weaponFrame(s, HAND_MAIN, wf) || !isChainsaw(wf.model))
    {
        Con_Printf("vr_chainsaw_fit: hold the chainsaw in the main hand\n");
        return;
    }
    glm::vec3 p, d;
    fistOf(s, HAND_MAIN, p, d);
    const glm::mat4 inv = glm::inverse(wf.modelToWorld);
    const glm::vec3 pm{inv * glm::vec4{p, 1.f}};
    const glm::vec3 dm = glm::normalize(glm::vec3{inv * glm::vec4{d, 0.f}});
    const glm::vec3 fm{inv * glm::vec4{wf.fist, 1.f}};
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.rot[HAND_MAIN], fwd, right, up);
    const glm::vec3 am = glm::normalize(glm::vec3{inv * glm::vec4{fwd, 0.f}});
    Con_Printf("vr_chainsaw_fit: fist channel at %.2f %.2f %.2f along %.3f %.3f %.3f (model); drawn fist %.2f %.2f %.2f; aim "
               "%.3f %.3f %.3f\n",
        pm.x, pm.y, pm.z, dm.x, dm.y, dm.z, fm.x, fm.y, fm.z, am.x, am.y, am.z);
    // Where the off hand's fist must go to take the cord (the mock's vr_mock_hand: metres right, up, back of the view's yaw).
    if(st.sawDrawn[HAND_MAIN])
    {
        glm::vec3 op, od;
        fistOf(s, HAND_OFF, op, od);
        hands::angleVectors(glm::vec3{0.f, cl.viewangles[YAW], 0.f}, fwd, right, up);
        const glm::vec3 d = (st.seatWorld[HAND_MAIN] - op) / units::metresToUnits();
        Con_Printf("vr_chainsaw_fit: the cord's handle from the off hand's fist: right %.3f up %.3f back %.3f m\n",
            glm::dot(d, right), glm::dot(d, up), -glm::dot(d, fwd));
        // Its hotspots (two, on the front handle's loop).
        int count = 0;
        for(int i = 0; i < weapons::maxHotspots; i++)
        {
            if(const view::WeaponHotspot h = view::weaponHotspot(HAND_MAIN, i); h.type)
            {
                count++;
                const glm::vec3 e = (h.pos - op) / units::metresToUnits();
                Con_Printf("vr_chainsaw_fit: hotspot %d (type %d) from the off hand's fist: right %.3f up %.3f back %.3f m\n",
                    i + 1, h.type, glm::dot(e, right), glm::dot(e, up), -glm::dot(e, fwd));
            }
        }
        Con_Printf("vr_chainsaw_fit: %d hotspots\n", count);
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_chainsaw_fit", fit_f);
}

void reset()
{
    st = State{};
    handle = Handle{}; // (keyed by a model: a new map may load it elsewhere)
    cord.hide();
}

bool holds(int hand)
{
    return st.holder == hand;
}

za::U8 moveBits()
{
    za::U8 bits = 0;
    const int holder = st.holder >= 0 ? st.holder : st.retractFrom >= 0.0 ? 1 - st.retractHand : -1;
    if(holder == HAND_OFF)
    {
        bits |= bitOffHolds;
    }
    else if(holder == HAND_MAIN)
    {
        bits |= bitMainHolds;
    }
    if(realtime < st.pulledUntil)
    {
        bits |= st.pulledBy == HAND_OFF ? bitOffPulled : bitMainPulled;
    }
    return bits;
}

float sinkDepth(int hand, const qmodel_t* model)
{
    if(!isChainsaw(model) || hand < 0 || hand > 1)
    {
        return 0.f;
    }
    const int flags = cl.stats[hand == HAND_MAIN ? protocol::STAT_QVR_WEAPONFLAGS : protocol::STAT_QVR_WEAPONFLAGS2];
    return (flags & wpnFlagSawChain) ? za::fmax(0.f, vr_chainsaw_overlap.value) * 0.01f * units::metresToUnits() : 0.f;
}

float barStart(const qmodel_t* model)
{
    return isChainsaw(model) ? handleOf(model).barStart : 1e9f;
}

bool grip(int hand, bool pressed)
{
    if(hand < 0 || hand > 1)
    {
        return false;
    }
    if(!pressed)
    {
        const bool taken = st.gripTaken[hand];
        st.gripTaken[hand] = false;
        if(st.holder == hand)
        {
            letGo(1 - hand, st.handlePos);
            debugLog("let go", hand, 0.f, 0.f);
        }
        return taken;
    }
    // An empty hand at the handle of the chainsaw in the other.
    const int sawHand = 1 - hand;
    const hands::State& s = hands::current();
    if(!s.valid || st.holder >= 0 || !st.sawDrawn[sawHand] || !held::handEmpty(hand) || flashlight::holds(hand) ||
        held::heldEntity(hand) != 0 || cl.stats[STAT_HEALTH] <= 0)
    {
        return false;
    }
    glm::vec3 p, d;
    fistOf(s, hand, p, d);
    const float dist = glm::distance(p, st.seatWorld[sawHand]);
    if(vr_debug_chainsaw.value)
    {
        Con_Printf("chainsaw cord (%s hand): grip %.1f cm from the handle (reach %.0f)\n", hand == HAND_MAIN ? "main" : "off",
            dist / units::metresToUnits() * 100.f, reach * 100.f);
    }
    if(dist > metres(reach))
    {
        return false;
    }
    st.holder = hand;
    st.armed = true;
    st.peakSpeed = 0.f;
    st.retractFrom = -1.0;
    st.gripTaken[hand] = true;
    debugLog("taken", hand, 0.f, 0.f);
    return true;
}

void shake(int hand, glm::vec3& pos, glm::vec3& angles)
{
    pos = glm::vec3{0.f};
    angles = glm::vec3{0.f};
    if(hand < 0 || hand > 1 || !sawIn(hand) || cl.stats[STAT_HEALTH] <= 0)
    {
        return;
    }
    // Steadied by the other hand, less (not still).
    const float two = za::clamp(twohand::transition(hand), 0.f, 1.f);
    float mm = 0.f;
    if(flagsOf(hand) & wpnFlagSawRunning)
    {
        mm = za::max(0.f, vr_chainsaw_shake.value) * (1.f - two) + za::max(0.f, vr_chainsaw_shake_2h.value) * two;
        if(flagsOf(hand) & wpnFlagSawChain)
        {
            mm *= shakeChain;
        }
    }
    // A pull's kick (it may just have caught: the stronger).
    mm = za::max(mm, pullShakeMm(hand));
    if(mm <= 0.f)
    {
        return;
    }
    const float t = static_cast<float>(za::fmod(realtime, 1000.0));
    const float amount = mm * 0.001f * units::metresToUnits();
    pos = glm::vec3{buzz(t, hand, 0), buzz(t, hand, 1), buzz(t, hand, 2)} * amount;
    angles = glm::vec3{buzz(t, hand, 3), buzz(t, hand, 4), buzz(t, hand, 5)} * (mm * shakeDegPerMm);
    if(vr_debug_chainsaw.value && realtime - st.shakePrinted >= 0.5)
    {
        st.shakePrinted = realtime;
        Con_Printf("chainsaw: %s hand shakes %.2f mm (two-handed %.2f), now %.3f mm %.3f deg\n",
            hand == HAND_MAIN ? "main" : "off", mm, two, glm::length(pos) / units::metresToUnits() * 1000.f,
            glm::length(angles));
    }
}

void setupView(const hands::State& s)
{
    if(st.lastFrame == host_framecount)
    {
        return;
    }
    st.lastFrame = host_framecount;

    const bool alive = cl.stats[STAT_HEALTH] > 0 && !cl.intermission;
    for(int h = 0; h < 2; h++)
    {
        const view::ViewEntity* ve = alive && s.valid ? sawIn(h) : nullptr;
        st.sawDrawn[h] = ve && handleOf(ve->ent.model).valid;
        if(st.sawDrawn[h])
        {
            st.seatWorld[h] = view::modelPoint(*ve, handle.seat);
        }
    }
    smokeFrame();
    st.drawHandle = false;

    // The chainsaw left the other hand, or the holder took something: the handle goes (nothing to fly back to).
    if(st.holder >= 0 && (!st.sawDrawn[1 - st.holder] || !held::handEmpty(st.holder)))
    {
        st.holder = -1;
        st.armed = true;
        cord.hide();
    }
    if(st.retractFrom >= 0.0 && (!st.sawDrawn[st.retractHand] || realtime - st.retractFrom > retractTime))
    {
        st.retractFrom = -1.0;
        st.retractHand = -1;
    }

    const int sawHand = st.holder >= 0 ? 1 - st.holder : st.retractHand;
    if(sawHand < 0)
    {
        cord.hide();
        return;
    }
    const view::ViewEntity* ve = sawIn(sawHand);
    const glm::vec3 hole = view::modelPoint(*ve, handle.hole);
    const glm::vec3 sawUp = glm::normalize(view::modelPoint(*ve, handle.hole + glm::vec3{0.f, 0.f, 1.f}) - hole);

    if(st.holder >= 0)
    {
        const int h = st.holder;
        glm::vec3 p, axis;
        fistOf(s, h, p, axis);
        const glm::vec3 out = p - hole;
        const float ext = glm::length(out) - glm::distance(st.seatWorld[sawHand], hole);
        const glm::vec3 dir = glm::length(out) > 1e-3f ? out / glm::length(out) : sawUp;
        const float speed = glm::dot(s.vel[h], dir); // m/s away from the hole
        st.peakSpeed = za::max(st.peakSpeed * 0.5f, speed);
        const float need = metres(za::fmax(1.f, vr_chainsaw_pull_distance.value) * 0.01f);
        if(st.armed && ext >= need)
        {
            st.armed = false;
            if(st.peakSpeed >= vr_chainsaw_pull_speed.value)
            {
                st.pulledUntil = realtime + pullLatch;
                st.pulledBy = h;
                debugLog("pulled", h, ext, st.peakSpeed);
                pullFeedback(sawHand, 1.f);
            }
            else
            {
                debugLog("pulled too slowly (a weak pull)", h, ext, st.peakSpeed);
                // The server never hears of it: its sound here (the cord's zip, the engine turned over, not firing).
                S_StartSound(cl.viewentity, -1, S_PrecacheSound(weakPullSound), vec3_origin, 0.7f, 1.f);
                pullFeedback(sawHand, weakPullShare);
            }
        }
        else if(!st.armed && ext < need * rearm)
        {
            st.armed = true;
            debugLog("back: ready for the next pull", h, ext, speed);
        }
        if(ext > need + metres(cordLength))
        {
            debugLog("out past its length: let go", h, ext, speed);
            st.handlePos = p;
            letGo(sawHand, p);
        }
        else
        {
            st.handlePos = p;
            st.handleAxis = axis;
            st.drawHandle = true;
        }
    }
    if(st.holder < 0 && st.retractFrom >= 0.0)
    {
        // Flying back: along the cord into its seat.
        const float t = static_cast<float>(za::clamp((realtime - st.retractFrom) / retractTime, 0.0, 1.0));
        st.handlePos = glm::mix(st.retractStart, st.seatWorld[sawHand], t * t);
        st.drawHandle = t < 1.f;
    }
    if(!st.drawHandle)
    {
        cord.hide();
        return;
    }
    coil::Style style;
    style.turns = 0;
    style.wireRadius = 0.0022f;
    style.albedo = glm::vec3{0.2f, 0.18f, 0.14f};
    const glm::vec3 toHandle = st.handlePos - hole;
    const glm::vec3 inDir = glm::length(toHandle) > 1e-3f ? -toHandle / glm::length(toHandle) : -sawUp;
    cord.update(hole, sawUp, st.handlePos, inDir, style);

    if(vr_debug_chainsaw.value >= 2.f)
    {
        lines::line(st.seatWorld[sawHand], hole, 0.2f, glm::vec4{1.f, 1.f, 0.f, 1.f}, glm::vec4{1.f, 0.5f, 0.f, 1.f});
    }
}

void drawOpaque()
{
    if(!st.drawHandle || !cord.visible())
    {
        return;
    }
    CordDraw& d = cordDraw;
    if(d.builtFrame != host_framecount)
    {
        d.builtFrame = host_framecount;
        QVR_PROFILE("chainsaw cord");
        const hands::State& s = hands::current();
        d.cord = cord.build(0.5f * (s.eyeOrigin[0] + s.eyeOrigin[1]), scratch.rings, d.sides) ? gfx::uploadTube(scratch.rings)
                                                                                              : gfx::TubeBatch{};
        // The T-handle across the fist: a short thick tube, closed at its ends.
        za::Vector<gfx::TubeRing>& rings = scratch.handleRings;
        rings.clear();
        const glm::vec3 axis = glm::normalize(st.handleAxis);
        const glm::vec3 ref = za::fabs(axis.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f};
        const glm::vec3 across = glm::normalize(glm::cross(ref, axis));
        const glm::vec3 light = coil::lightAt(st.handlePos);
        const float half = metres(handleHalf), r = metres(handleRadius);
        const float at[4][2] = {{-1.f, 0.05f}, {-0.97f, 1.f}, {0.97f, 1.f}, {1.f, 0.05f}};
        for(const auto& a : at)
        {
            gfx::TubeRing ring{};
            ring.mid = glm::vec4{st.handlePos + axis * (half * a[0]), r * a[1]};
            ring.across = glm::vec4{across, 0.f};
            ring.along = glm::vec4{axis, 0.f};
            ring.ambient = glm::vec4{light, 0.f};
            ring.lamp = glm::vec4{0.f};
            ring.lampDir = glm::vec4{0.f, 0.f, 1.f, 0.f};
            rings.pushBack(ring);
        }
        d.handle = gfx::uploadTube(rings);
    }
    QVR_GPU_PROFILE("chainsaw cord draw");
    if(d.cord.count)
    {
        gfx::drawTube(d.cord, d.sides, cord.albedo(), glm::normalize(glm::vec3{0.3f, 0.2f, 1.f}));
    }
    gfx::drawTube(d.handle, 12, glm::vec3{0.55f, 0.36f, 0.08f}, glm::normalize(glm::vec3{0.3f, 0.2f, 1.f}));
}

} // namespace qvr::chainsaw
