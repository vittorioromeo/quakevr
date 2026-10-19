// vr_weight.cpp -- see vr_weight.hpp.
//
// The spring, per hand, in the body's frame (metres, relative to the floor below the head, the play space's turn taken
// out), in fixed substeps of at most a millisecond (the same motion at any frame rate), the hand's target interpolated
// across the frame:
//   linear:  F = k (target - x) + c (target's velocity - v), |F| <= the arm's force;  a = F / m + sag
//   angular: tau = kA e + C (target's spin - w), |tau| <= the wrist's torque, e the turn to the target; plus the
//            centre of mass's weight and its trailing a moving grip: r x m (g sag - a_grip) (r from the grip to it);
//            alpha = tau / I, per axis of the hand (I about the grip: a long thing is slow to pitch and yaw, quicker to roll;
//            a weapon rolls about the forearm, below its body, by the forearm's twist: a quarter of the wrist's kA and
//            torque, gunLoad; its inertia about the forward axis times Roll Weight, vr_weight_spring_roll and w_roll).
// k = 4000 N/m for everything (times vr_weight_spring_stiffness): a thing of m kg follows at sqrt(k / m) rad/s (a 3 kg
// shotgun 37, an 8 kg rocket launcher 22, a 40 kg explosive box 10), capped at 120 (what weighs nothing follows at
// once); the wrist's kA is 400 N m/rad plus 3 per N m of the weight's pull about the grip. c = 2 zeta sqrt(k m)
// (critically damped at vr_weight_spring_damping 1); the damping is against the target's velocity, so a hand moving
// steadily is followed without lag: the lag is in the starts and stops (the error is the hand's acceleration over the
// frequency squared), and a stop overshoots a little and settles. The arm's 400 N and the wrist's 100 N m (times
// vr_weight_spring_strength), beyond holding it up, cap how fast a heavy thing is swung: a 40 kg box gains 10 m/s^2 at
// most. A two-handed grip multiplies both by vr_weight_spring_2h (4: twice as quick). The sag is
// 0.3 of gravity's pull (times vr_weight_spring_sag), 35% of it with the hand at the shoulder, all of it at arm's length.
// Each of these settings is also times the thing's own (Load::tune: Weapon Weights, Held Object Weights).

#include "vr_modelmetadata.hpp"
#include "vr_weight_internal.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_fatigue.hpp"
#include "vr_flashlight.hpp"
#include "vr_held.hpp"
#include "vr_main.hpp"
#include "vr_meleehud.hpp"
#include "vr_profile.hpp"
#include "vr_props.hpp"
#include "vr_protocol.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/quaternion.hpp>

#include <stdio.h>
#include <string.h>

namespace qvr::weight
{
using namespace detail;

namespace
{

constexpr float armStiffness = 4000.f;  // N/m
constexpr float wristStiffness = 400.f; // N m/rad
constexpr float gripStiffening = 3.f;   // N m/rad more per N m of the weight's pull about the grip
constexpr float armForce = 400.f;       // N, beyond holding it up
constexpr float wristTorque = 100.f;    // N m, beyond holding it level
// A weapon's turn about its barrel (a roll of the wrist, palm up and palm down; NOTES.md vrfiringrange_2026-10-02_01-14-49):
// the forearm's twist turns it, weaker than the wrist's bend and the arm (a share of their stiffness and torque), and about
// the forearm's axis, which runs through the hand below the weapon's body (its own girth, 6 cm, and its body 8 cm above
// that axis). A 7 kg laser cannon: 0.07 kg m^2 against 106 N m/rad, 39 rad/s (pitch and yaw: 27); 0.025 and 425 before,
// as quick as the spring goes (120), no weight at all.
constexpr float forearmTwist = 0.25f;
constexpr float gravity = 9.81f;        // m/s^2: the real world's, as the hands are
constexpr float fastest = 120.f;        // rad/s: the quickest a spring follows (and the substeps' stability: 0.12 a step)
constexpr float substep = 0.001f;       // s, at most
constexpr float jumpSpeed = 20.f;       // m/s: a hand's target moving faster jumped (put back in the hand at once)
constexpr const char* flashlightModel = "progs/vrflashlight.mdl";
constexpr const char* emptyHandModel = "empty hand";

Body bodies[2];

// Wrenched out (vr_weight_drop): each hand's controller turns over the last moments (the grip pose as tracked, the
// play space's turn taken out), on the spring's clock (the frames' dt: fixed frames in the tests).
struct Wrench
{
    static constexpr int size = 64;
    double t[size]{};
    glm::quat q[size]{};
    int count{0}, newest{-1};
    double clock{0.0};
    double until{-1.0};   // vr_gametime: the move says "wrenched out" until then (the server reads it once a frame)
};
Wrench wrenches[2];
constexpr double wrenchLatch = 0.15; // seconds (as the chainsaw's pull)
constexpr double jumpLatch = 0.05;   // seconds: put back in the hand (a move or two)
constexpr float wrenchJump = 90.f;   // degrees in one frame: tracking lost and found, not a hand

// A prop's mass as Box3D has it (a listen server), kept while the same thing of the same setting is held.
struct MassCache
{
    int ent{0};
    const qmodel_t* model{nullptr};
    float setting{-1.f};
    unsigned generation{0}; // props::settingsGeneration: a Size or Mass x changed while held weighs it again
    float mass{0.f};
};
MassCache massCache[2];

float easedShare = 0.f; // staminaShare's, eased
double easedAt = -1.0;

} // namespace
namespace detail
{
[[nodiscard]] glm::quat quatFromAngles(const glm::vec3& angles)
{
    glm::vec3 f, r, u;
    hands::angleVectors(angles, f, r, u);
    return glm::normalize(glm::quat_cast(glm::mat3{f, -r, u}));
}
} // namespace detail
namespace
{

} // namespace
namespace detail
{
[[nodiscard]] glm::vec3 anglesFromQuat(const glm::quat& q)
{
    const glm::mat3 m = glm::mat3_cast(q);
    return hands::anglesFromVectors(m[0], m[2]);
}
} // namespace detail
namespace
{

// The rotation vector (axis times angle, radians) of `q`, the short way round.
} // namespace
namespace detail
{
[[nodiscard]] glm::vec3 rotationVector(glm::quat q)
{
    if(q.w < 0.f)
    {
        q = -q;
    }
    const glm::vec3 v{q.x, q.y, q.z};
    const float s = glm::length(v);
    return s < 1e-7f ? v * 2.f : v * (2.f * za::atan2(s, q.w) / s);
}
} // namespace detail
namespace
{

// The prop's mass: its setting, else Box3D's (a listen server: the server's entity of the same number), else estimated.
[[nodiscard]] float propMass(int hand, int ent, const qmodel_t* model, const glm::vec3& boxSize)
{
    const float setting = props::valueFor(model, props::Key::Mass);
    if(setting > 0.f)
    {
        return setting;
    }
    MassCache& c = massCache[hand];
    const unsigned generation = props::settingsGeneration();
    if(c.ent == ent && c.model == model && c.setting == setting && c.generation == generation)
    {
        return c.mass;
    }
    c = {ent, model, setting, generation, 0.f};
    if(sv.active && ent < sv.qcvm.num_edicts)
    {
        qcvm_t* const old = qcvm;
        if(old != &sv.qcvm)
        {
            if(old)
            {
                PR_SwitchQCVM(nullptr);
            }
            PR_SwitchQCVM(&sv.qcvm);
        }
        edict_t* e = EDICT_NUM(ent);
        if(sv.models[static_cast<int>(e->v.modelindex)] == nullptr ||
            strcmp(sv.models[static_cast<int>(e->v.modelindex)]->name, model->name) == 0)
        {
            c.mass = box3d::propMass(e);
        }
        if(old != &sv.qcvm)
        {
            PR_SwitchQCVM(nullptr);
            if(old)
            {
                PR_SwitchQCVM(old);
            }
        }
    }
    if(c.mass <= 0.f)
    {
        c.mass = props::estimateMass(model, boxSize);
    }
    return c.mass;
}

// The spring's multipliers of a weapon's slot (Weapon Weights) or a prop's (Held Object Weights; slot -1: the defaults, 1).
} // namespace
namespace detail
{
[[nodiscard]] Tuning weaponTuning(int slot)
{
    using weapons::Key;
    return {weapons::value(slot, Key::SpringStiffness), weapons::value(slot, Key::SpringDamping),
        weapons::value(slot, Key::SpringStrength), weapons::value(slot, Key::SpringSag), weapons::value(slot, Key::SpringSwing),
        weapons::value(slot, Key::SpringTwoHanded), weapons::value(slot, Key::SpringSnap), weapons::value(slot, Key::SpringRoll)};
}
} // namespace detail
namespace
{

} // namespace
namespace detail
{
[[nodiscard]] Tuning propTuning(int slot)
{
    using props::Key;
    return {props::value(slot, Key::SpringStiffness), props::value(slot, Key::SpringDamping), props::value(slot, Key::SpringStrength),
        props::value(slot, Key::SpringSag), props::value(slot, Key::SpringSwing), props::value(slot, Key::SpringTwoHanded),
        props::value(slot, Key::SpringSnap)};
}
} // namespace detail
namespace
{

// The stamina's part of a hand's load (an empty hand's is in its mass).
void withStamina(Load& l)
{
    l.staminaMult = l.empty ? 1.f : staminaMultiplier();
    l.staminaAdd = l.empty ? 0.f : za::max(vr_weight_stamina_add.value, 0.f) * staminaShare();
}

// A rod along the hand's forward: `length` metres, its centre of mass `balance` ahead of the grip, `radius` round it.
} // namespace
namespace detail
{
void rodLoad(Load& l, float mass, float balance, float length, float radius)
{
    l.mass = mass;
    l.com = {balance, 0.f, 0.f};
    const float across = mass * (length * length / 12.f + balance * balance);
    l.inertia = {mass * radius * radius, across, across};
}
} // namespace detail
namespace
{

// A weapon: a rod turned about the forearm's axis by its twist (forearmTwist).
} // namespace
namespace detail
{
void gunLoad(Load& l, float mass, float balance, float length)
{
    rodLoad(l, mass, balance, length, 0.f);
    l.inertia.x = mass * gunRollRadius2;
    l.twist = forearmTwist;
}
} // namespace detail
namespace
{

// Tired, an empty hand weighs vr_weight_stamina_empty kg at none (on the stamina's curve): a fist. Not a hand holding a
// ledge or a rung (the climb pulls by the controller) nor one steadying the other's weapon (it stays on its grip).
[[nodiscard]] Load emptyHandLoad(int h)
{
    Load l;
    const float mass = za::max(vr_weight_stamina_empty.value, 0.f) * staminaShare();
    const bool climbing = (cl.protocolflags & PRFL_QUAKEVR) && (cl.stats[protocol::STAT_QVR_CLIMB] & (1 << h)) != 0;
    if(mass <= 0.01f || climbing || twohand::helping(h))
    {
        return l;
    }
    l.valid = true;
    l.empty = true;
    l.model = emptyHandModel;
    rodLoad(l, mass, 0.04f, 0.12f, 0.04f);
    return l;
}

// What hand `h` holds. With the hands (`s`), a two-handed hold's pivot: between the two grips (a box held by its sides,
// a gun by its handle and foregrip), where the thing turns about, so its weight's pull and its inertia are about there.
[[nodiscard]] Load computeLoad(int h, const hands::State* s)
{
    Load l;
    const float u2m = 1.f / units::metresToUnits();
    // A carried prop (Held Object Weights), the flashlight, else the weapon (Weapon Weights).
    glm::vec3 origin, other{0.f};
    glm::mat3 axes;
    bool both = false;
    if(const int ent = held::placeInHand(h, origin, axes, both, other))
    {
        const qmodel_t* model = cl_entities[ent].model;
        glm::vec3 lo, hi;
        if(held::drawnBox(ent, lo, hi))
        {
            const int slot = props::slotForModel(model);
            const glm::vec3 size = hi - lo;
            const float mass = propMass(h, ent, model, size);
            const glm::vec3 com = (lo + hi) * 0.5f +
                                  glm::vec3{props::scaledValue(slot, props::Key::ComX), props::scaledValue(slot, props::Key::ComY),
                                      props::scaledValue(slot, props::Key::ComZ)};
            const glm::vec3 pivot = both ? other * (0.5f * u2m) : glm::vec3{0.f};
            const glm::vec3 r = (origin + axes * com) * u2m - pivot;
            const glm::vec3 d = size * u2m;
            const float scale = za::max(props::value(slot, props::Key::Inertia), 0.f);
            const glm::mat3 own{glm::vec3{mass / 12.f * (d.y * d.y + d.z * d.z) * scale, 0.f, 0.f},
                glm::vec3{0.f, mass / 12.f * (d.x * d.x + d.z * d.z) * scale, 0.f},
                glm::vec3{0.f, 0.f, mass / 12.f * (d.x * d.x + d.y * d.y) * scale}};
            // About the grip, in the hand's axes (the parallel axes); the diagonal (a box held at a corner turns
            // about the hand's axes near enough).
            const glm::mat3 about = axes * own * glm::transpose(axes) +
                                    mass * (glm::dot(r, r) * glm::mat3{1.f} - glm::outerProduct(r, r));
            l.valid = mass > 0.01f;
            l.prop = true;
            l.entity = ent;
            l.model = model->name;
            l.mass = mass;
            l.com = r;
            l.inertia = {about[0][0], about[1][1], about[2][2]};
            l.twoHanded = both ? 1.f : 0.f;
            l.tune = propTuning(slot);
        }
        return l;
    }
    if(flashlight::holds(h))
    {
        const float mass = props::valueFor(flashlightModel, props::Key::Mass);
        if(mass > 0.f)
        {
            l.valid = true;
            l.model = flashlightModel;
            rodLoad(l, mass, 0.05f, 0.25f, 0.025f);
            l.tune = propTuning(props::slotForModel(flashlightModel));
        }
        return l;
    }
    const int slot = weapons::heldSlot(h);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return emptyHandLoad(h);
    }
    const float mass = weapons::value(slot, weapons::Key::Mass);
    if(mass <= 0.01f)
    {
        return l;
    }
    l.valid = true;
    l.model = weapons::cvar(slot, weapons::Key::ID)->string;
    gunLoad(l, mass, weapons::value(slot, weapons::Key::Balance) * 0.01f, za::max(weapons::value(slot, weapons::Key::Span), 1.f) * 0.01f);
    l.twoHanded = za::clamp(twohand::support(h), 0.f, 1.f); // (a hotspot's grip, or the other hand anywhere on it)
    l.tune = weaponTuning(slot);
    l.slot = slot;
    l.heft = heft(slot);
    if(!s)
    {
        return l;
    }
    // The other hand on it: at a hotspot (its point), or anywhere (its place on it: the same pivot, NOTES.md
    // vrfiringrange_2026-10-01_17-10-50).
    float onGrip = za::clamp(twohand::transition(h), 0.f, 1.f);
    glm::vec3 gripAt{0.f};
    bool gripped = onGrip > 0.f && s->grip2HValid[h];
    if(gripped)
    {
        gripAt = s->grip2H[h];
    }
    else if(float t = 0.f; twohand::freeGripPoint(*s, h, gripAt, t) && t > 0.f)
    {
        gripped = true;
        onGrip = za::clamp(t, 0.f, 1.f);
    }
    const glm::mat3 hand = held::axesFromAngles(&s->rot[h][0], true);
    glm::vec3 axis{1.f, 0.f, 0.f}; // the weapon's, in the hand's axes
    // Carried off its handle (the hand-off, a hotspot, anywhere): its centre of mass is where it is drawn, its Balance ahead
    // of its handle, not ahead of this hand.
    twohand::HeldAs as{glm::vec3{0.f}, glm::vec3{0.f}, false};
    const bool carried = twohand::carrying(h) && twohand::carriedWeapon(*s, h, as);
    if(carried)
    {
        glm::vec3 f, r, u;
        hands::angleVectors(as.rot, f, r, u);
        axis = glm::transpose(hand) * f;
        l.com = glm::transpose(hand) * (as.pos - s->pos[h]) * u2m + axis * (weapons::value(slot, weapons::Key::Balance) * 0.01f);
    }
    if(gripped)
    {
        // Held by its handle and a foregrip (or both hands on it anywhere): it turns about between them (as far as the
        // grip is taken).
        l.com -= glm::transpose(hand) * (gripAt - s->pos[h]) * (0.5f * u2m * onGrip);
    }
    if(gripped || carried)
    {
        // A rod along `axis` (its own inertia: about its middle; about its length as gunLoad's), about the pivot (the
        // parallel axes).
        const float length = za::max(weapons::value(slot, weapons::Key::Span), 1.f) * 0.01f;
        const glm::mat3 along = glm::outerProduct(axis, axis);
        const glm::vec3 r = l.com;
        const glm::mat3 about = l.mass * gunRollRadius2 * along + l.mass * length * length / 12.f * (glm::mat3{1.f} - along) +
                                l.mass * (glm::dot(r, r) * glm::mat3{1.f} - glm::outerProduct(r, r));
        l.inertia = {about[0][0], about[1][1], about[2][2]};
    }
    return l;
}

// How far the hand is from its shoulder, 0 (at it) .. 1 (at arm's length), in the body's frame (metres).
[[nodiscard]] float extension(const hands::State& s, int h, const glm::vec3& grip, const glm::vec3& base, float turnYaw)
{
    const float u2m = 1.f / units::metresToUnits();
    const glm::vec3 head = hands::rotateYaw(s.head - base, -turnYaw) * u2m;
    glm::vec3 f, r, u;
    hands::angleVectors({0.f, s.headAngles.y - turnYaw, 0.f}, f, r, u);
    const bool right = h == HAND_MAIN; // (the right controller)
    const glm::vec3 shoulder = head + glm::vec3{0.f, 0.f, -0.22f} + r * (right ? 0.18f : -0.18f);
    return za::clamp(glm::distance(grip, shoulder) / 0.62f, 0.f, 1.f);
}

// vr_debug_weight: a line a frame per hand holding something: where it is tracked (target, body frame metres) and
// drawn, how far apart (cm, degrees; the drawn turn's pitch, yaw and roll less the target's) and the drawn speed.
FILE* traceFile = nullptr; // vr_debug_weight's weight_trace file (trace)

void trace(int h, const Load& l, const glm::vec3& xt, const glm::quat& qt, const glm::vec3& x, const glm::quat& q, float speed, float droopDeg)
{
    FILE*& file = traceFile;
    if(!vr_debug_weight.value)
    {
        if(file)
        {
            fclose(file);
            file = nullptr;
        }
        return;
    }
    if(!file && !(file = fopen(va("%s/weight_trace.txt", com_gamedir), "w")))
    {
        return;
    }
    const glm::vec3 ta = anglesFromQuat(qt), da = anglesFromQuat(q);
    const auto wrap = [](float a) { return za::remainder(a, 360.f); };
    const float off = glm::distance(x, xt) * 100.f;
    const float ang = glm::degrees(glm::length(rotationVector(qt * glm::conjugate(q))));
    fprintf(file,
        "%.4f %s %s mass %.2f mult %.3f 2h %.2f target %.4f %.4f %.4f drawn %.4f %.4f %.4f off_cm %.2f ang_deg %.2f dpitch %.2f dyaw %.2f "
        "droll %.2f speed %.3f model %s\n",
        vr_gametime, h == HAND_MAIN ? "main" : "off", l.model, l.mass, l.staminaMult, l.twoHanded, xt.x, xt.y, xt.z, x.x, x.y, x.z, off, ang,
        wrap(da.x - ta.x), wrap(da.y - ta.y), wrap(da.z - ta.z), speed, "spring");
    if(vr_debug_weight.value >= 2)
    {
        // (off its target with the droop; and off the controller itself: the droop taken out of the target)
        const glm::quat hand = qt * glm::angleAxis(-glm::radians(droopDeg), glm::vec3{0.f, 1.f, 0.f});
        const float handAng = glm::degrees(glm::length(rotationVector(hand * glm::conjugate(q))));
        Con_Printf("weight: %s pitch %+.2f below the controller\n", h == HAND_MAIN ? "main" : "off",
            za::remainder(anglesFromQuat(q).x - anglesFromQuat(hand).x, 360.f));
        Con_Printf("weight: %s %s %.1f kg x%.2f off %.2f cm %.2f deg (the controller %.2f, droop %.1f), 2h %.2f, com %.3f %.3f %.3f m, inertia %.4f %.4f %.4f\n",
            h == HAND_MAIN ? "main" : "off", l.model, l.mass, l.staminaMult, off, ang, handAng, droopDeg, l.twoHanded, l.com.x, l.com.y, l.com.z,
            l.inertia.x, l.inertia.y, l.inertia.z);
    }
}

// Left too far behind the hand (vr_weight_spring_snap: cm, or 3 times as many degrees, checked each frame): put back
// in it at once.
} // namespace
namespace detail
{
[[nodiscard]] bool putBack(const Body& b, const glm::vec3& xt, const glm::quat& qt, float snapCm)
{
    return glm::distance(b.x, xt) * 100.f > snapCm || glm::degrees(glm::length(rotationVector(qt * glm::conjugate(b.q)))) > 3.f * snapCm;
}
} // namespace detail
namespace
{

// Turned further than this off the hand part way through a frame (each substep), it is put back in it whatever
// vr_weight_spring_snap says: the spring pulls the short way round, and past half a turn off the short way flips, so it
// would swing on round the other way, a whole turn (a heavy weapon after a fast wrist snap and back). Measured against
// the hand's turn at that moment (not the frame's end: a wiggled shotgun lags up to about 160 degrees on its own).
constexpr float snapTurnMost = 165.f; // degrees
constexpr float catchFrom = 1.0471976f; // radians (60 degrees): the wrist's catch-up (step) from this far off the hand
constexpr float catchGain = 30.f;               // up to 1 + this times its pull and torque near snapTurnMost
constexpr float leashRad = 2.6179939f;          // radians (150 degrees): the most it lags the hand's turn (step's leash)

// One frame of the spring. True: it was left too far behind the hand's turn part way through (a fast wrist snap and back:
// swinging on past half a turn off, the short way would flip); put it back in the hand.
} // namespace
namespace detail
{
[[nodiscard]] bool step(Body& b, const Load& l, float m, const glm::vec3& xt0, const glm::vec3& xt, const glm::quat& qt0,
    const glm::quat& qt, float dt, float sag)
{
    const float snapRad = glm::radians(snapTurnMost);
    const int n = za::max(1, static_cast<int>(za::ceil(dt / substep - 1e-4f)));
    const float h = dt / static_cast<float>(n);
    const glm::vec3 vt = (xt - xt0) / dt;
    const glm::vec3 wt = rotationVector(qt * glm::conjugate(qt0)) / dt;

    // The global settings (Aiming: Spring) times the thing's own (Weapon Weights, Held Object Weights).
    const Tuning& t = l.tune;
    const float zeta = za::max(vr_weight_spring_damping.value * t.damping, 0.f);
    const float grip2 = 1.f + (za::max(vr_weight_spring_2h.value * t.twoHanded, 1.f) - 1.f) * l.twoHanded; // two hands: stiffer, stronger
    const float stiff = za::max(vr_weight_spring_stiffness.value * t.stiffness, 0.01f) * grip2;
    float k = armStiffness * stiff;
    k = za::min(k, m * fastest * fastest);
    const float c = 2.f * zeta * za::sqrt(k * m);
    // A heavy thing is gripped harder: the wrist's stiffness grows with the weight's pull about the grip (its own mass,
    // not what tiredness adds: a tired arm droops more).
    // About the forward axis (a roll), the forearm's twist for a weapon (Load::twist).
    // Heavy handling: in one hand a heavy weapon is held less firmly and springier (the struggle), more so just after a
    // shot's kick (heavyWrist).
    float heavyStiff = 1.f, heavyDamp = 1.f;
    heavyWrist(l, heavyStiff, heavyDamp);
    const float kA = (wristStiffness + gripStiffening * l.mass * gravity * glm::length(l.com)) * stiff * heavyStiff;
    const float zetaA = zeta * heavyDamp;
    const glm::vec3 kAxis{kA * l.twist, kA, kA};
    const float strength = za::max(vr_weight_spring_strength.value * t.strength, 0.f);
    const float force = strength > 0.f ? armForce * strength * grip2 : 0.f;
    const float torque = strength > 0.f ? wristTorque * strength * grip2 : 0.f;
    const float inert = za::max(vr_weight_spring_inertia.value * t.swing, 0.f);

    // Roll Weight: times the inertia about the forward axis (Aiming: Spring, Weapon Weights).
    const glm::vec3 own{l.inertia.x * za::max(vr_weight_spring_roll.value * t.roll, 0.f), l.inertia.y, l.inertia.z};
    glm::vec3 inertia, damping;
    for(int i = 0; i < 3; i++)
    {
        inertia[i] = za::max(own[i], kAxis[i] / (fastest * fastest));
        damping[i] = 2.f * zetaA * za::sqrt(kAxis[i] * inertia[i]);
    }
    const glm::vec3 sagAccel{0.f, 0.f, -gravity * sag};
    const glm::vec3 holdForce = -m * sagAccel; // holding it up takes this much: the strength is what is left for moving it

    for(int i = 0; i < n; i++)
    {
        const float a = static_cast<float>(i + 1) / static_cast<float>(n);
        const glm::vec3 target = glm::mix(xt0, xt, a);
        const glm::quat targetTurn = glm::slerp(qt0, qt, a);

        // The grip.
        glm::vec3 f = k * (target - b.x) + c * (vt - b.v) - holdForce;
        if(const float len = glm::length(f); force > 0.f && len > force)
        {
            f *= force / len;
        }
        f += holdForce;
        const glm::vec3 before = b.v;
        b.v += (f / m + sagAccel) * h;
        b.x += b.v * h;
        const glm::vec3 gripAccel = (b.v - before) / h;

        // Its turn, in the hand's axes.
        const glm::mat3 rot = glm::mat3_cast(b.q);
        const glm::mat3 toHand = glm::transpose(rot);
        const glm::vec3 e = toHand * rotationVector(targetTurn * glm::conjugate(b.q));
        if(glm::length(e) > snapRad)
        {
            return true;
        }
        const glm::vec3 dw = toHand * (wt - b.w);
        const glm::vec3 weightTorque = glm::cross(l.com, toHand * (m * sagAccel));
        // Far behind the hand (a heavy weapon after a quick turn of the wrist), the wrist works harder to bring it back:
        // its pull and its most torque grow from catchFrom off on, so it lags gradually and stays short of snapTurnMost
        // (put back in the hand at once there: an abrupt jump; the hammer in one hand lagged a 425 deg/s yaw to 165).
        const float behind = za::clamp((glm::length(e) - catchFrom) / (snapRad - catchFrom), 0.f, 1.f);
        const float catchUp = 1.f + catchGain * behind * behind;
        glm::vec3 tau = kAxis * e * catchUp + damping * dw + weightTorque;
        const float most = torque * catchUp;
        if(torque > 0.f)
        {
            tau.x = za::clamp(tau.x, -most * l.twist, most * l.twist); // the forearm's twist is weaker
        }
        if(const float len = glm::length(tau); torque > 0.f && len > most)
        {
            tau *= most / len;
        }
        // The centre of mass: its weight (the sag; held up whatever the strength) and its trailing the grip's acceleration.
        tau += -weightTorque + glm::cross(l.com, toHand * (m * (sagAccel - inert * gripAccel)));
        b.w += rot * (tau / inertia) * h;
        const glm::quat spin{0.f, b.w.x, b.w.y, b.w.z};
        b.q = glm::normalize(b.q + (spin * b.q) * (0.5f * h));
        // On a leash: never further than leashTurn off the hand. Past it the hand drags it along (turned back onto the
        // leash, spinning no further away than the hand: smoothly, not put back in the hand at once as at snapTurnMost).
        const glm::vec3 off = rotationVector(targetTurn * glm::conjugate(b.q)); // (the body frame)
        if(const float apart = glm::length(off); apart > leashRad)
        {
            const glm::vec3 axis = off / apart;
            b.q = glm::normalize(glm::angleAxis(apart - leashRad, axis) * b.q);
            if(const float away = glm::dot(b.w - wt, axis); away < 0.f)
            {
                b.w -= axis * away;
            }
        }
    }
    return false;
}
} // namespace detail
namespace
{

// The most a weapon of `mass` kg may turn (degrees a second) before it is wrenched out, `twoHanded` (0..1) held so; 0: it
// never is (off, or lighter than vr_weight_drop_from).
[[nodiscard]] float wrenchLimit(float mass, float twoHanded)
{
    const float from = za::max(vr_weight_drop_from.value, 0.1f);
    if(!vr_weight_drop.value || !(mass >= from) || vr_weight_drop_speed.value <= 0.f)
    {
        return 0.f;
    }
    const float twoHands = 1.f + (za::max(vr_weight_drop_2h.value, 1.f) - 1.f) * za::clamp(twoHanded, 0.f, 1.f);
    return vr_weight_drop_speed.value * za::pow(from / mass, za::max(vr_weight_drop_curve.value, 0.f)) * twoHands;
}

// Wrenched out: hand `h`'s controller turned faster than its weapon's limit over the window (the grip pose as tracked,
// the play space's turn taken out: the stick's turning and walking don't count, nor does the spring's lag).
void wrenchFrame(int h, const hands::State& s, float turnYaw, const Load& l, float dt)
{
    Wrench& w = wrenches[h];
    w.clock += dt;
    const bool weapon = l.valid && !l.prop && !l.empty && modelmeta::identifyPath(l.model) != modelmeta::Id::Vrflashlight;
    const float limit = weapon && vrActive() ? wrenchLimit(l.mass, l.twoHanded) : 0.f;
    if(limit <= 0.f)
    {
        w.count = 0;
        return;
    }
    const glm::quat q = quatFromAngles(s.gripRot[h] - glm::vec3{0.f, turnYaw, 0.f});
    if(w.count > 0 && glm::degrees(glm::length(rotationVector(q * glm::conjugate(w.q[w.newest])))) > wrenchJump)
    {
        w.count = 0;
    }
    w.newest = (w.newest + 1) % Wrench::size;
    w.t[w.newest] = w.clock;
    w.q[w.newest] = q;
    w.count = za::min(w.count + 1, Wrench::size);

    // The newest sample at least the window old (else the oldest kept, once half the window is kept).
    const double window = za::clamp(static_cast<double>(vr_weight_drop_window.value), 0.005, 0.5);
    int from = -1;
    for(int i = 1; i < w.count; i++)
    {
        const int k = (w.newest - i + Wrench::size) % Wrench::size;
        from = k;
        if(w.clock - w.t[k] >= window)
        {
            break;
        }
    }
    const double span = from >= 0 ? w.clock - w.t[from] : 0.0;
    if(from < 0 || span < window * 0.5)
    {
        return;
    }
    const float speed = glm::degrees(glm::length(rotationVector(q * glm::conjugate(w.q[from])))) / static_cast<float>(span);
    if(speed > limit)
    {
        w.until = vr_gametime + wrenchLatch;
        w.count = 0;
        Con_DPrintf("weight drop: %s %s wrenched out: %.0f deg/s over %.3f s, limit %.0f (%.1f kg, two hands %.2f)\n",
            h == HAND_MAIN ? "main" : "off", l.model, speed, span, limit, l.mass, l.twoHanded);
    }
}

} // namespace

za::U8 dropBits()
{
    za::U8 bits = 0;
    if(vr_gametime < wrenches[HAND_OFF].until)
    {
        bits |= 1; // QC VR_HANDDROP_OFF
    }
    if(vr_gametime < wrenches[HAND_MAIN].until)
    {
        bits |= 2; // VR_HANDDROP_MAIN
    }
    if(vr_gametime < bodies[HAND_OFF].jumpUntil)
    {
        bits |= 4; // VR_HANDJUMP_OFF
    }
    if(vr_gametime < bodies[HAND_MAIN].jumpUntil)
    {
        bits |= 8; // VR_HANDJUMP_MAIN
    }
    return bits;
}

float dropLimit(float mass, float twoHanded)
{
    return wrenchLimit(mass, twoHanded);
}

void registerCommands()
{
    registerDiagCommands(); // vr_throw_table, vr_weight_test, vr_weight_table (vr_weight_diag.cpp)
}

float staminaShareFor(float left)
{
    return vr_weight_stamina.value ? tiredShare(left) : 0.f;
}

float tiredShare(float left)
{
    const float from = za::clamp(vr_weight_stamina_from.value, 0.01f, 1.f);
    if(left >= from)
    {
        return 0.f;
    }
    const float u = za::clamp((from - left) / from, 0.f, 1.f);
    return za::pow(u, za::max(vr_weight_stamina_curve.value, 0.1f));
}

float staminaCurve(float left)
{
    return 1.f + (za::max(vr_weight_stamina_max.value, 1.f) - 1.f) * staminaShareFor(left);
}

float staminaShare()
{
    if(easedAt == vr_gametime)
    {
        return easedShare;
    }
    const float target = staminaShareFor(fatigue::staminaLeft());
    const float dt = easedAt >= 0.0 ? static_cast<float>(za::clamp(vr_gametime - easedAt, 0.0, 0.25)) : 1.f;
    easedAt = vr_gametime;
    easedShare = target + (easedShare - target) * za::exp(-dt / 0.25f);
    if(za::fabs(easedShare - target) < 1e-4f)
    {
        easedShare = target; // (settled exactly: unchanged at 0)
    }
    return easedShare;
}

float staminaMultiplier()
{
    return 1.f + (za::max(vr_weight_stamina_max.value, 1.f) - 1.f) * staminaShare();
}

float effectiveMass(const Load& l)
{
    return l.empty ? l.mass : l.mass * l.staminaMult + l.staminaAdd;
}

Load load(int hand)
{
    Load l = hand == 0 || hand == 1 ? computeLoad(hand, nullptr) : Load{};
    withStamina(l);
    return l;
}

Offset offset(int hand)
{
    return hand == 0 || hand == 1 ? bodies[hand].off : Offset{};
}

// Heavy handling ------------------------------------------------------------------------------------------------------

float heft(int slot)
{
    if(!vr_heavy.value || slot < 0 || slot == weapons::fistSlot())
    {
        return 0.f;
    }
    const float own = weapons::value(slot, weapons::Key::Heft);
    if(own >= 0.f)
    {
        return za::clamp(own, 0.f, 1.f);
    }
    // From its mass (a weapon without its own class).
    const float from = vr_heavy_auto_from.value, full = za::max(vr_heavy_auto_full.value, from + 0.1f);
    return za::clamp((weapons::value(slot, weapons::Key::Mass) - from) / (full - from), 0.f, 1.f);
}

float droop(int slot, float twoHanded)
{
    if(!vr_heavy_droop.value)
    {
        return 0.f;
    }
    const float one = 1.f - za::clamp(twoHanded, 0.f, 1.f);
    return vr_heavy_droop_deg.value * heft(slot) * one * za::max(slot >= 0 ? weapons::value(slot, weapons::Key::Droop) : 0.f, 0.f);
}

float kickSpeed(int slot, float twoHanded)
{
    if(!vr_heavy_recoil.value || slot < 0)
    {
        return 0.f;
    }
    const float one = 1.f - za::clamp(twoHanded, 0.f, 1.f);
    return za::max(vr_heavy_recoil_kick.value, 0.f) * heft(slot) * one * za::max(weapons::value(slot, weapons::Key::HeavyRecoil), 0.f);
}

namespace
{

// A hand's load's heavy shares now: its one-handed struggle and its recovering kick.
void heavyShares(Load& l, const Body& b)
{
    const float one = 1.f - za::clamp(l.twoHanded, 0.f, 1.f);
    l.struggle = l.heft * one * za::clamp(vr_heavy_struggle.value, 0.f, 1.f);
    const double time = za::max(static_cast<double>(vr_heavy_recoil_time.value), 0.01);
    const double since = vr_gametime - b.kickAt;
    l.kick = b.kickAt >= 0.0 && since >= 0.0 && since < time ? b.kickShare * one * static_cast<float>(1.0 - since / time) : 0.f;
}

} // namespace

namespace detail
{
void heavyWrist(const Load& l, float& stiffness, float& damping)
{
    const float s = za::clamp(l.struggle, 0.f, 1.f), k = za::clamp(l.kick, 0.f, 1.f);
    stiffness = (1.f + (za::max(vr_heavy_struggle_stiffness.value, 0.01f) - 1.f) * s) *
                (1.f + (za::max(vr_heavy_recoil_recovery.value, 0.01f) - 1.f) * k);
    damping = 1.f + (za::max(vr_heavy_struggle_damping.value, 0.f) - 1.f) * s;
}
} // namespace detail

void heavyKick(int hand)
{
    if(hand != 0 && hand != 1)
    {
        return;
    }
    Body& b = bodies[hand];
    const Load l = computeLoad(hand, nullptr);
    const float speed = l.valid && b.active ? kickSpeed(l.slot, l.twoHanded) : 0.f;
    if(speed <= 0.f)
    {
        return;
    }
    // Up about the hand's left axis (a turn about it pitches the barrel down).
    b.w += glm::mat3_cast(b.q) * glm::vec3{0.f, -glm::radians(speed), 0.f};
    b.kickAt = vr_gametime;
    b.kickShare = l.heft * (1.f - za::clamp(l.twoHanded, 0.f, 1.f));
    if(vr_debug_weight.value)
    {
        Con_Printf("heavy kick: %s %s %.0f deg/s (heft %.2f, two hands %.2f)\n", hand == HAND_MAIN ? "main" : "off", l.model, speed,
            l.heft, l.twoHanded);
    }
}

void spring(hands::State& s, float turnYaw, float dt, bool newFrame)
{
    QVR_PROFILE("weight");
    const float m2u = units::metresToUnits();
    const glm::vec3 base = s.playerOrigin + s.lean;
    for(int h = 0; h < HAND_COUNT; h++)
    {
        Body& b = bodies[h];
        const glm::vec3 xt = hands::rotateYaw(s.pos[h] - base, -turnYaw) / m2u;
        glm::quat qt = quatFromAngles(s.rot[h] - glm::vec3{0.f, turnYaw, 0.f});
        Load l = computeLoad(h, &s);
        withStamina(l);
        if(newFrame && dt > 0.f)
        {
            wrenchFrame(h, s, turnYaw, l, dt);
        }
        // Heavy handling: in one hand, a heavy weapon's struggle and a shot's kick recovering, and its droop (the barrel
        // down about the grip: the target the spring pulls it to; the drawn hand and the aim with it).
        heavyShares(l, b);
        if(const float sag = droop(l.slot, l.twoHanded); sag != 0.f && l.valid)
        {
            qt = qt * glm::angleAxis(glm::radians(sag), glm::vec3{0.f, 1.f, 0.f});
        }
        if(!l.valid)
        {
            b.active = false;
            b.off = {};
            b.xt = xt;
            b.qt = qt;
            b.targetValid = true;
            continue;
        }
        if(newFrame && dt > 0.f)
        {
            const float snap = za::max(vr_weight_spring_snap.value * l.tune.snap, 1.f);
            // Put back: left too far behind, or the hand jumped (faster than a hand goes: a teleport, the play space
            // re-based, tracking lost and found; as the melee's VR_MELEE_JUMP).
            const bool jumped = b.targetValid && glm::distance(xt, b.xt) > jumpSpeed * dt;
            const bool snapped = b.active && (jumped || putBack(b, xt, qt, snap));
            bool flipped = false; // (left half a turn behind part way through the frame: step)
            const bool other = b.model != l.model || b.entity != l.entity; // (a weapon changed, a prop taken)
            if(b.active && b.targetValid && !snapped && !other)
            {
                const float m = za::max(effectiveMass(l), 0.01f);
                const float sag = sagShare * za::max(vr_weight_spring_sag.value * l.tune.sag, 0.f) *
                                  (0.35f + 0.65f * za::pow(extension(s, h, b.x, base, turnYaw), 2.f));
                flipped = step(b, l, m, b.xt, xt, b.qt, qt, dt, sag);
            }
            if(!b.active || !b.targetValid || snapped || flipped || other)
            {
                // Taken (or put back in the hand): where the hand is, at rest (the hand's pose may jump as a weapon
                // comes: its Hand and Weapon Together offset).
                b.active = true;
                b.model = l.model;
                b.entity = l.entity;
                b.x = xt;
                b.q = qt;
                b.v = glm::vec3{0.f};
                b.w = glm::vec3{0.f};
                if(flipped)
                {
                    // The melee takes it for a jump, not a swing (the other put-backs, as they were: a hand's own jump
                    // is one already).
                    b.jumpUntil = vr_gametime + jumpLatch;
                }
                if((snapped || flipped) && vr_debug_weight.value)
                {
                    Con_Printf("weight: %s put back in the hand\n", h == HAND_MAIN ? "main" : "off");
                }
            }
            b.xt = xt;
            b.qt = qt;
            b.targetValid = true;
            b.off.active = true;
            b.off.distance = glm::distance(b.x, xt) * 100.f;
            b.off.angle = glm::degrees(glm::length(rotationVector(qt * glm::conjugate(b.q))));
            trace(h, l, xt, qt, b.x, b.q, glm::length(b.v), droop(l.slot, l.twoHanded));
        }
        if(!b.active)
        {
            continue;
        }
        s.pos[h] = base + hands::rotateYaw(b.x * m2u, turnYaw);
        s.rot[h] = anglesFromQuat(b.q) + glm::vec3{0.f, turnYaw, 0.f};
    }
}

float damageMultiplier(float mass)
{
    const float k = za::max(vr_weight_damage_exp.value, 0.f);
    if(!(mass > 0.f) || k == 0.f)
    {
        return 1.f;
    }
    const float light = za::max(vr_weight_damage_light.value, 0.01f);
    const float heavy = za::max(vr_weight_damage_heavy.value, light);
    const float ref = mass > heavy ? heavy : mass < light ? light : mass;
    const float lo = za::min(vr_weight_damage_min.value, 1.f), hi = za::max(vr_weight_damage_max.value, 1.f);
    return za::clamp(za::pow(mass / ref, k), lo, hi);
}

namespace
{

// The gain of a real throw (docs/vr-port/THROWING.md 3.2): speeds up to vr_throw_gain_lo m/s leave the hand as they are
// (drops, passes); above, smoothly more, up to vr_throw_gain_max times at vr_throw_gain_hi. (QC's VRThrowGain before.)
[[nodiscard]] float throwGain(float speed)
{
    const float lo = vr_throw_gain_lo.value, hi = vr_throw_gain_hi.value, most = vr_throw_gain_max.value;
    if(hi <= lo)
    {
        return speed >= hi ? most : 1.f;
    }
    const float t = za::clamp((speed - lo) / (hi - lo), 0.f, 1.f);
    return 1.f + (most - 1.f) * t * t * (3.f - 2.f * t);
}

// As if it weighed this much (two hands: less).
[[nodiscard]] float throwMass(float mass, int hands)
{
    return hands >= 2 ? mass / za::max(vr_throw_2h_strength.value, 0.1f) : mass;
}

// The mass a throw's flick moves (throwMass) and the most a wrist flicks freely (vr_throw_flick_mass): a heavier thing
// keeps free / mass of the flick.
struct Flick
{
    float mass;
    float free;
};
[[nodiscard]] Flick flickOf(float mass, int hands)
{
    return {throwMass(za::max(mass, 0.f), hands), za::max(vr_throw_flick_mass.value, 0.01f)};
}

} // namespace

float throwLimit(float mass, int hands)
{
    const float m = throwMass(za::max(mass, 0.f), hands);
    const float light = za::max(vr_throw_mass_light.value, 0.01f);
    const float most = za::max(vr_throw_max_speed.value, 0.1f);
    return m > light ? most * za::pow(light / m, za::max(vr_throw_mass_exp.value, 0.f)) : most;
}

namespace
{

// `s` (> 0) eased towards `limit`: as it is up to vr_throw_mass_knee of it, then a tanh towards it (the same slope at the
// knee).
[[nodiscard]] float softLimit(float s, float limit)
{
    const float knee = za::clamp(vr_throw_mass_knee.value, 0.f, 1.f) * limit;
    if(s <= knee)
    {
        return s;
    }
    const float room = limit - knee;
    if(room <= 1e-4f)
    {
        return limit;
    }
    const float x = (s - knee) / room;
    return knee + room * (1.f - 2.f / (za::exp(2.f * x) + 1.f));
}

} // namespace

float spinLimit(float mass, int hands)
{
    const float k = vr_throw_spin_mass_exp.value;
    if(!(k > 0.f))
    {
        return 0.f;
    }
    const Flick fl = flickOf(mass, hands);
    return fl.mass > fl.free ? za::max(vr_throw_spin_max.value, 0.f) * za::pow(fl.free / fl.mass, k) : 0.f;
}

glm::vec3 throwSpin(const glm::vec3& angVel, float mass, int hands)
{
    const float limit = spinLimit(mass, hands);
    const float rate = glm::length(angVel);
    if(!(limit > 0.f) || rate < 1e-4f)
    {
        return angVel;
    }
    const glm::vec3 out = angVel * (softLimit(rate, limit) / rate);
    if(vr_debug_throw.value)
    {
        Con_Printf("throw spin: %.1f kg %dh %.1f rad/s, limit %.1f: %.1f rad/s\n", mass, hands, rate, limit,
            glm::length(out));
    }
    return out;
}

float throwScale(float mass)
{
    const float lenient = leniency(mass);
    const float top = vr_throw_hit_top.value;
    const float limit = throwLimit(mass, 1);
    if(!(top > 0.f) || !(limit > 0.f) || !(mass > 0.f))
    {
        return lenient;
    }
    const float least = za::clamp(vr_weight_lenient_min.value, 0.01f, 1.f);
    return za::clamp(za::min(lenient, limit / top), za::min(least, lenient), 1.f);
}

float throwHitSpeed(float mass, float minMult)
{
    return za::max(vr_throw_hit_min_speed.value, 0.f) * throwScale(mass) * za::max(minMult, 0.f);
}

float throwDamage(float speed, float fullSpeed, float mass, float minMult, float curveMult)
{
    const float hit = throwHitSpeed(mass, minMult);
    if(!(speed > 0.f) || speed < hit)
    {
        return 0.f;
    }
    const float scale = za::max(throwScale(mass), 0.01f);
    float f = speed * za::max(curveMult, 0.f) / (za::max(fullSpeed, 1e-3f) * scale);
    const float ramp = za::max(vr_throw_hit_ramp.value, 0.f) * hit;
    if(ramp > 1e-3f && speed < hit + ramp)
    {
        const float t = (speed - hit) / ramp;
        const float floor = za::clamp(vr_throw_hit_ramp_floor.value, 0.f, 1.f);
        f *= floor + (1.f - floor) * t * t * (3.f - 2.f * t);
    }
    return f;
}

ThrowOut throwVelocity(const glm::vec3& vel, const glm::vec3& flick, float mass, int hands)
{
    ThrowOut out;
    glm::vec3 v = vel;
    // A wrist too weak to flick it: of a heavy thing, only some of the flick's speed.
    const Flick fl = flickOf(mass, hands);
    out.kept = fl.mass > fl.free ? fl.free / fl.mass : 1.f;
    // No more of it than the throw has along it (a controller turned about its own point, not the wrist).
    if(const float f = glm::length(flick); f > 1e-4f)
    {
        const glm::vec3 along = flick / f;
        v -= along * (za::clamp(glm::dot(v, along), 0.f, f) * (1.f - out.kept));
    }
    out.gain = throwGain(glm::length(v));
    v *= out.gain;

    out.limit = throwLimit(mass, hands);
    const float speed = glm::length(v);
    if(out.limit > 0.f && speed > 1e-4f)
    {
        // Soft: as it was up to the knee, then easing towards the limit (tanh: the same slope at the knee).
        v *= softLimit(speed, out.limit) / speed;
    }
    out.vel = v;

    if(vr_debug_throw.value)
    {
        // And as it was without the model (the gain only), and how far each would go at 45 degrees on level ground.
        const float off = glm::length(vel) * throwGain(glm::length(vel)), now = glm::length(out.vel);
        const float g = vr_throw_gravity.value > 0.f ? vr_throw_gravity.value : 9.81f;
        Con_Printf("throw weight: %.1f kg %dh %.1f (flick %.1f, %.0f%%) x%.2f lim %.1f: %.1f m/s %.0f m; was %.1f %.0f m\n",
            mass, hands, glm::length(vel), glm::length(flick), out.kept * 100.f, out.gain, out.limit, now, now * now / g,
            off, off * off / g);
    }
    return out;
}

float leniency(float mass)
{
    const float j = za::max(vr_weight_lenient.value, 0.f);
    const float from = za::max(vr_weight_lenient_from.value, 0.01f);
    if(!(mass > from) || j == 0.f)
    {
        return 1.f;
    }
    return za::clamp(za::pow(from / mass, j), za::clamp(vr_weight_lenient_min.value, 0.01f, 1.f), 1.f);
}

void reset()
{
    for(Body& b : bodies)
    {
        b = Body{};
    }
    for(MassCache& c : massCache)
    {
        c = MassCache{};
    }
    easedShare = 0.f;
    easedAt = -1.0;
}

} // namespace qvr::weight
