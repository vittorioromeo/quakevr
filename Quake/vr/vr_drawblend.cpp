// vr_drawblend.cpp -- a gun drawn from a holster eases from its holstered pose into the hand, and one holstered eases
// from the hand into the holster (ROUND21.md, "Holster draw blend; holster defaults; body calibration kept").
//
// The server swaps the models at once (the hand's STAT_WEAPON / STAT_QVR_WEAPONMODEL2, the holster's
// STAT_QVR_HOLSTERWEAPONMODEL*): the view sees a gun appear in a hand and, a frame either side, go from a holster (or
// the other way), and eases the drawn gun between the two places. Visual only: the aim, the muzzle, the shots, the
// two-handed grips and the melee use the gun's real place in the hand from the first frame (it can be fired at once).
//
// The start pose is kept in the frame of where the gun is going (the hand's gun, or the holster's), so the gun follows
// the hand (or the body) as it comes: drawn = target * blend(offset, k), k from 1 to 0 on an ease-out. The turn is a
// quaternion slerp from the identity to the offset's turn taken the short way (its sign flipped when its w < 0): the
// gun turns by at most 180 degrees, straight to where it is going, and the angle left shrinks monotonically.

#include "vr_drawblend.hpp"
#include "vr_backend.hpp"
#include "vr_body.hpp"
#include "vr_cvars.hpp"
#include "vr_view.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace qvr::drawblend
{
namespace
{

// A model change and the other end of it (the holster it came from or went to, the hand at it) are at most this far
// apart (seconds): the stats of the hand and the holster come in the same update, but a hand may leave a holster's
// reach the frame it takes the gun.
constexpr double pairing = 0.25;

struct Pose
{
    glm::vec3 pos{0.f};
    glm::quat rot{1.f, 0.f, 0.f, 0.f};
};

// An alias entity's pose (its angles' pitch is the other way).
[[nodiscard]] Pose poseOf(const entity_t& e)
{
    glm::vec3 f, r, u;
    hands::angleVectors({-e.angles[0], e.angles[1], e.angles[2]}, f, r, u);
    return {{e.origin[0], e.origin[1], e.origin[2]}, glm::normalize(glm::quat_cast(glm::mat3{f, -r, u}))};
}

void setPose(entity_t& e, const Pose& p)
{
    const glm::mat3 m = glm::mat3_cast(p.rot);
    const glm::vec3 a = hands::anglesFromVectors(glm::normalize(m[0]), glm::normalize(m[2]));
    const glm::vec3 angles{-a.x, a.y, a.z};
    for(int i = 0; i < 3; i++)
    {
        e.origin[i] = p.pos[i];
        e.angles[i] = angles[i];
    }
}

// The angle (degrees) between two turns, the short way (from the relative turn's axis part: precise near 0, where an
// acos of the dot product is not).
[[nodiscard]] float angleBetween(const glm::quat& a, const glm::quat& b)
{
    const glm::quat d = glm::conjugate(a) * b;
    return glm::degrees(2.f * std::atan2(glm::length(glm::vec3{d.x, d.y, d.z}), std::fabs(d.w)));
}

struct Blend
{
    bool on{false};
    double start{0.0};
    float time{0.f};
    Pose offset;          // where it started, in the frame of where it is going
    float startAngle{0.f}; // degrees, the short way (for the log)
    float rawAngle{0.f};   // the turn a sign-blind slerp would have taken (over 180: the short way was chosen)
    int from{-1};          // the holster (a draw) or the hand (holstering), for the log
};

// Starts `b`: from `from` (a world pose) to `to` (this frame's), over `time` seconds.
void start(Blend& b, const Pose& from, const Pose& to, float time, int source)
{
    const glm::quat inv = glm::inverse(to.rot);
    b.offset.pos = inv * (from.pos - to.pos);
    b.offset.rot = glm::normalize(inv * from.rot);
    const float raw = glm::degrees(2.f * std::acos(std::clamp(b.offset.rot.w, -1.f, 1.f)));
    if(b.offset.rot.w < 0.f)
    {
        b.offset.rot = -b.offset.rot; // the same turn, the short way round
    }
    b.startAngle = glm::degrees(2.f * std::acos(std::min(b.offset.rot.w, 1.f)));
    b.rawAngle = raw;
    b.on = true;
    b.start = realtime;
    b.time = time;
    b.from = source;
}

// The share of the offset left (1 at the start, 0 at the end: an ease-out cubic, quick at first, settling in); turns
// `b` off once done.
[[nodiscard]] float remaining(Blend& b)
{
    if(!b.on)
    {
        return 0.f;
    }
    const double u = b.time > 0.f ? (realtime - b.start) / b.time : 1.0;
    if(u >= 1.0 || u < 0.0)
    {
        b.on = false;
        return 0.f;
    }
    const float left = 1.f - static_cast<float>(u);
    return left * left * left;
}

// `to` moved and turned by the share `k` of the offset (the turn from the identity, the short way).
[[nodiscard]] Pose blended(const Pose& to, const Blend& b, float k)
{
    const glm::quat identity{1.f, 0.f, 0.f, 0.f};
    return {to.pos + to.rot * (b.offset.pos * k), glm::normalize(to.rot * glm::slerp(identity, b.offset.rot, k))};
}

// vr_debug_draw_blend: each frame of a blend.
void logFrame(const char* what, const qmodel_t* model, const Blend& b, float k, const Pose& drawn, const Pose& to)
{
    if(vr_debug_draw_blend.value)
    {
        Con_Printf("draw blend: %s, %s: %.3f s, left %.3f; turn to go %.1f deg (from %.1f; unflipped %.1f), "
                   "%.2f units to go (from %.2f)\n",
            what, model ? model->name : "-", realtime - b.start, k, angleBetween(drawn.rot, to.rot), b.startAngle,
            b.rawAngle, glm::distance(drawn.pos, to.pos), glm::length(b.offset.pos));
    }
}

struct HandState
{
    const qmodel_t* model{nullptr}; // the gun it held last frame (null: none)
    Pose drawn;                     // where it was drawn
    int hovered{-1};                // the holster it was last at, and when
    double hoveredAt{-1e9};
    const qmodel_t* lost{nullptr};  // the gun it last let go of: where, when, at which holster
    Pose lostPose;
    double lostAt{-1e9};
    int lostHolster{-1};
    Blend blend;
};

struct HolsterState
{
    const qmodel_t* model{nullptr}; // the gun it showed last frame
    Pose drawn;                     // where it was drawn, and when last with a gun
    double shownAt{-1e9};
    double changedAt{-1e9};         // when its gun last changed (put in, taken out)
    Blend blend;
};

HandState handStates[2];
HolsterState holsterStates[body::HolsterCount];

[[nodiscard]] bool sameGun(const qmodel_t* a, const qmodel_t* b)
{
    return a && b && view::sameGun(a, b);
}

// The holster a hand is at this frame, -1 if none.
[[nodiscard]] int holsterAt(const hands::State& s, int hand)
{
    for(int h = 0; h < body::HolsterCount; h++)
    {
        if(s.hotspot[hand] == body::holsterHotspot(static_cast<body::Holster>(h)))
        {
            return h;
        }
    }
    return -1;
}

} // namespace

void hand(const hands::State& s, int hand, entity_t& e, bool gun)
{
    HandState& st = handStates[hand];
    const qmodel_t* const model = gun ? e.model : nullptr;
    if(const int h = holsterAt(s, hand); h >= 0)
    {
        st.hovered = h;
        st.hoveredAt = realtime;
    }
    const int recent = realtime - st.hoveredAt <= pairing ? st.hovered : -1;

    if(model != st.model && !sameGun(model, st.model)) // (a morph to the other ammo's model is the same gun)
    {
        st.blend.on = false;
        if(st.model)
        {
            // Let go of (holstered, thrown, passed): a holster that takes it at once eases it in from here.
            st.lost = st.model;
            st.lostPose = st.drawn;
            st.lostAt = realtime;
            st.lostHolster = recent;
        }
        const float time = vr_weapon_draw_blend.value;
        if(model && recent >= 0 && time > 0.f)
        {
            // Taken from the holster the hand is at, which showed this gun a moment ago.
            const HolsterState& from = holsterStates[recent];
            if(sameGun(from.model, model) && realtime - from.shownAt <= pairing)
            {
                start(st.blend, from.drawn, poseOf(e), time, recent);
            }
        }
    }
    st.model = model;

    if(const float k = remaining(st.blend); k > 0.f && model)
    {
        const Pose to = poseOf(e);
        const Pose drawn = blended(to, st.blend, k);
        setPose(e, drawn);
        char what[64];
        q_snprintf(what, sizeof(what), "%s hand from holster %d", hand == HAND_MAIN ? "main" : "off", st.blend.from);
        logFrame(what, model, st.blend, k, drawn, to);
    }
    if(model)
    {
        st.drawn = poseOf(e);
    }
}

void holster(const hands::State& s, int holster, entity_t* e, bool live)
{
    (void)s;
    HolsterState& st = holsterStates[holster];
    const qmodel_t* const model = live && e ? e->model : nullptr;
    if(model != st.model)
    {
        st.changedAt = realtime;
        st.blend.on = false;
    }

    // A gun let go of at this holster a moment ago, and in it now (as it changed: in Quick Slots the holster shows its gun
    // throughout): eased in from where the hand held it.
    const float time = vr_weapon_holster_blend.value;
    const bool quickSlots = vr_holster_mode.value != 0.f;
    for(int hand = 0; hand < 2 && model && time > 0.f && !st.blend.on; hand++)
    {
        HandState& h = handStates[hand];
        if(h.lostHolster == holster && sameGun(h.lost, model) && realtime - h.lostAt <= pairing &&
            (quickSlots || realtime - st.changedAt <= pairing))
        {
            start(st.blend, h.lostPose, poseOf(*e), time, hand);
            h.lost = nullptr; // once
        }
    }
    st.model = model;

    if(const float k = remaining(st.blend); k > 0.f && model)
    {
        const Pose to = poseOf(*e);
        const Pose drawn = blended(to, st.blend, k);
        setPose(*e, drawn);
        char what[64];
        q_snprintf(what, sizeof(what), "holster %d from %s hand", holster, st.blend.from == HAND_MAIN ? "main" : "off");
        logFrame(what, model, st.blend, k, drawn, to);
    }
    if(model)
    {
        st.drawn = poseOf(*e);
        st.shownAt = realtime;
    }
}

void reset()
{
    for(HandState& h : handStates)
    {
        h = HandState{};
    }
    for(HolsterState& h : holsterStates)
    {
        h = HolsterState{};
    }
}

} // namespace qvr::drawblend
