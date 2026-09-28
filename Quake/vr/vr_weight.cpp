// vr_weight.cpp -- see vr_weight.hpp.
//
// The spring, per hand, in the body's frame (metres, relative to the floor below the head, the play space's turn taken
// out), in fixed substeps of at most a millisecond (the same motion at any frame rate), the hand's target interpolated
// across the frame:
//   linear:  F = k (target - x) + c (target's velocity - v), |F| <= the arm's force;  a = F / m + sag
//   angular: tau = kA e + C (target's spin - w), |tau| <= the wrist's torque, e the turn to the target; plus the
//            centre of mass's weight and its trailing a moving grip: r x m (g sag - a_grip) (r from the grip to it);
//            alpha = tau / I, per axis of the hand (I about the grip: a long thing is slow to pitch and yaw, quick to roll).
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

#include "vr_weight.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flashlight.hpp"
#include "vr_held.hpp"
#include "vr_meleehud.hpp"
#include "vr_profile.hpp"
#include "vr_props.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"
#include "vr_weapons.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace qvr::weight
{
namespace
{

constexpr float armStiffness = 4000.f;  // N/m
constexpr float wristStiffness = 400.f; // N m/rad
constexpr float gripStiffening = 3.f;   // N m/rad more per N m of the weight's pull about the grip
constexpr float armForce = 400.f;       // N, beyond holding it up
constexpr float wristTorque = 100.f;    // N m, beyond holding it level
constexpr float sagShare = 0.3f;        // of gravity's pull at arm's length (vr_weight_spring_sag 1)
constexpr float gravity = 9.81f;        // m/s^2: the real world's, as the hands are
constexpr float fastest = 120.f;        // rad/s: the quickest a spring follows (and the substeps' stability: 0.12 a step)
constexpr float substep = 0.001f;       // s, at most
constexpr float jumpSpeed = 20.f;       // m/s: a hand's target moving faster jumped (put back in the hand at once)
constexpr const char* flashlightModel = "progs/vrflashlight.mdl";

struct Body
{
    bool active{false};
    glm::vec3 x{0.f}, v{0.f}; // the grip: metres, m/s (body frame)
    glm::quat q{1.f, 0.f, 0.f, 0.f};
    glm::vec3 w{0.f};         // rad/s (body frame)
    bool targetValid{false};
    glm::vec3 xt{0.f};        // last frame's target
    glm::quat qt{1.f, 0.f, 0.f, 0.f};
    Offset off;
    const char* model{nullptr}; // what it holds (another: taken afresh)
    int entity{0};
};
Body bodies[2];

// A prop's mass as Box3D has it (a listen server), kept while the same thing of the same setting is held.
struct MassCache
{
    int ent{0};
    const qmodel_t* model{nullptr};
    float setting{-1.f};
    float mass{0.f};
};
MassCache massCache[2];

float easedMult = 1.f;
double easedAt = -1.0;

[[nodiscard]] glm::quat quatFromAngles(const glm::vec3& angles)
{
    glm::vec3 f, r, u;
    hands::angleVectors(angles, f, r, u);
    return glm::normalize(glm::quat_cast(glm::mat3{f, -r, u}));
}

[[nodiscard]] glm::vec3 anglesFromQuat(const glm::quat& q)
{
    const glm::mat3 m = glm::mat3_cast(q);
    return hands::anglesFromVectors(m[0], m[2]);
}

// The rotation vector (axis times angle, radians) of `q`, the short way round.
[[nodiscard]] glm::vec3 rotationVector(glm::quat q)
{
    if(q.w < 0.f)
    {
        q = -q;
    }
    const glm::vec3 v{q.x, q.y, q.z};
    const float s = glm::length(v);
    return s < 1e-7f ? v * 2.f : v * (2.f * std::atan2(s, q.w) / s);
}

// The prop's mass: its setting, else Box3D's (a listen server: the server's entity of the same number), else estimated.
[[nodiscard]] float propMass(int hand, int ent, const qmodel_t* model, const glm::vec3& boxSize)
{
    const float setting = props::valueFor(model->name, props::Key::Mass);
    if(setting > 0.f)
    {
        return setting;
    }
    MassCache& c = massCache[hand];
    if(c.ent == ent && c.model == model && c.setting == setting)
    {
        return c.mass;
    }
    c = {ent, model, setting, 0.f};
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
[[nodiscard]] Tuning weaponTuning(int slot)
{
    using weapons::Key;
    return {weapons::value(slot, Key::SpringStiffness), weapons::value(slot, Key::SpringDamping),
        weapons::value(slot, Key::SpringStrength), weapons::value(slot, Key::SpringSag), weapons::value(slot, Key::SpringSwing),
        weapons::value(slot, Key::SpringTwoHanded), weapons::value(slot, Key::SpringSnap)};
}

[[nodiscard]] Tuning propTuning(int slot)
{
    using props::Key;
    return {props::value(slot, Key::SpringStiffness), props::value(slot, Key::SpringDamping), props::value(slot, Key::SpringStrength),
        props::value(slot, Key::SpringSag), props::value(slot, Key::SpringSwing), props::value(slot, Key::SpringTwoHanded),
        props::value(slot, Key::SpringSnap)};
}

// A rod along the hand's forward: `length` metres, its centre of mass `balance` ahead of the grip, `radius` round it.
void rodLoad(Load& l, float mass, float balance, float length, float radius)
{
    l.mass = mass;
    l.com = {balance, 0.f, 0.f};
    const float across = mass * (length * length / 12.f + balance * balance);
    l.inertia = {mass * radius * radius, across, across};
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
            const int slot = props::slotForModel(model->name);
            const glm::vec3 size = hi - lo;
            const float mass = propMass(h, ent, model, size);
            const glm::vec3 com = (lo + hi) * 0.5f +
                                  glm::vec3{props::value(slot, props::Key::ComX), props::value(slot, props::Key::ComY),
                                      props::value(slot, props::Key::ComZ)};
            const glm::vec3 pivot = both ? other * (0.5f * u2m) : glm::vec3{0.f};
            const glm::vec3 r = (origin + axes * com) * u2m - pivot;
            const glm::vec3 d = size * u2m;
            const float scale = std::max(props::value(slot, props::Key::Inertia), 0.f);
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
        return l;
    }
    const float mass = weapons::value(slot, weapons::Key::Mass);
    if(mass <= 0.01f)
    {
        return l;
    }
    l.valid = true;
    l.model = weapons::cvar(slot, weapons::Key::ID)->string;
    rodLoad(l, mass, weapons::value(slot, weapons::Key::Balance) * 0.01f, std::max(weapons::value(slot, weapons::Key::Span), 1.f) * 0.01f,
        0.06f);
    l.twoHanded = std::clamp(twohand::transition(h), 0.f, 1.f);
    l.tune = weaponTuning(slot);
    if(s && l.twoHanded > 0.f && s->grip2HValid[h])
    {
        // Held by its handle and a foregrip: it turns about between them (as far as the grip is taken).
        const glm::mat3 hand = held::axesFromAngles(&s->rot[h][0], true);
        const glm::vec3 pivot = glm::transpose(hand) * (s->grip2H[h] - s->pos[h]) * (0.5f * u2m * l.twoHanded);
        l.com -= pivot;
        const float across = l.mass * (glm::pow(weapons::value(slot, weapons::Key::Span) * 0.01f, 2.f) / 12.f);
        const glm::vec3 r = l.com;
        l.inertia = {l.mass * 0.06f * 0.06f + l.mass * (r.y * r.y + r.z * r.z), across + l.mass * (r.x * r.x + r.z * r.z),
            across + l.mass * (r.x * r.x + r.y * r.y)};
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
    const bool right = (h == HAND_MAIN) != (vr_lefthanded.value != 0.f);
    const glm::vec3 shoulder = head + glm::vec3{0.f, 0.f, -0.22f} + r * (right ? 0.18f : -0.18f);
    return std::clamp(glm::distance(grip, shoulder) / 0.62f, 0.f, 1.f);
}

// vr_debug_weight: a line a frame per hand holding something: where it is tracked (target, body frame metres) and
// drawn, how far apart (cm, degrees; the drawn turn's pitch, yaw and roll less the target's) and the drawn speed.
void trace(int h, const Load& l, const glm::vec3& xt, const glm::quat& qt, const glm::vec3& x, const glm::quat& q, float speed)
{
    static FILE* file = nullptr;
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
    const auto wrap = [](float a) { return std::remainder(a, 360.f); };
    const float off = glm::distance(x, xt) * 100.f;
    const float ang = glm::degrees(glm::length(rotationVector(qt * glm::conjugate(q))));
    fprintf(file,
        "%.4f %s %s mass %.2f mult %.3f 2h %.2f target %.4f %.4f %.4f drawn %.4f %.4f %.4f off_cm %.2f ang_deg %.2f dpitch %.2f dyaw %.2f "
        "droll %.2f speed %.3f model %s\n",
        realtime, h == HAND_MAIN ? "main" : "off", l.model, l.mass, l.staminaMult, l.twoHanded, xt.x, xt.y, xt.z, x.x, x.y, x.z, off, ang,
        wrap(da.x - ta.x), wrap(da.y - ta.y), wrap(da.z - ta.z), speed, "spring");
    if(vr_debug_weight.value >= 2)
    {
        Con_Printf("weight: %s %s %.1f kg x%.2f off %.2f cm %.2f deg\n", h == HAND_MAIN ? "main" : "off", l.model, l.mass,
            l.staminaMult, off, ang);
    }
}

void step(Body& b, const Load& l, float m, const glm::vec3& xt0, const glm::vec3& xt, const glm::quat& qt0, const glm::quat& qt,
    float dt, float sag)
{
    const int n = std::max(1, static_cast<int>(std::ceil(dt / substep - 1e-4f)));
    const float h = dt / static_cast<float>(n);
    const glm::vec3 vt = (xt - xt0) / dt;
    const glm::vec3 wt = rotationVector(qt * glm::conjugate(qt0)) / dt;

    // The global settings (Aiming: Spring) times the thing's own (Weapon Weights, Held Object Weights).
    const Tuning& t = l.tune;
    const float zeta = std::max(vr_weight_spring_damping.value * t.damping, 0.f);
    const float grip2 = 1.f + (std::max(vr_weight_spring_2h.value * t.twoHanded, 1.f) - 1.f) * l.twoHanded; // two hands: stiffer, stronger
    const float stiff = std::max(vr_weight_spring_stiffness.value * t.stiffness, 0.01f) * grip2;
    float k = armStiffness * stiff;
    k = std::min(k, m * fastest * fastest);
    const float c = 2.f * zeta * std::sqrt(k * m);
    // A heavy thing is gripped harder: the wrist's stiffness grows with the weight's pull about the grip (its own mass,
    // not what tiredness adds: a tired arm droops more).
    const float kA = (wristStiffness + gripStiffening * l.mass * gravity * glm::length(l.com)) * stiff;
    const float strength = std::max(vr_weight_spring_strength.value * t.strength, 0.f);
    const float force = strength > 0.f ? armForce * strength * grip2 : 0.f;
    const float torque = strength > 0.f ? wristTorque * strength * grip2 : 0.f;
    const float inert = std::max(vr_weight_spring_inertia.value * t.swing, 0.f);

    glm::vec3 inertia, damping;
    for(int i = 0; i < 3; i++)
    {
        inertia[i] = std::max(l.inertia[i], kA / (fastest * fastest));
        damping[i] = 2.f * zeta * std::sqrt(kA * inertia[i]);
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
        const glm::vec3 dw = toHand * (wt - b.w);
        const glm::vec3 weightTorque = glm::cross(l.com, toHand * (m * sagAccel));
        glm::vec3 tau = kA * e + damping * dw + weightTorque;
        if(const float len = glm::length(tau); torque > 0.f && len > torque)
        {
            tau *= torque / len;
        }
        // The centre of mass: its weight (the sag; held up whatever the strength) and its trailing the grip's acceleration.
        tau += -weightTorque + glm::cross(l.com, toHand * (m * (sagAccel - inert * gripAccel)));
        b.w += rot * (tau / inertia) * h;
        const glm::quat spin{0.f, b.w.x, b.w.y, b.w.z};
        b.q = glm::normalize(b.q + (spin * b.q) * (0.5f * h));
    }
}

// vr_weight_test [csv]: the spring on its own, offline, for a light and a heavy weapon and an explosive box, one- and
// two-handed, tired or not, at several frame rates: a fast swing (60 cm across in 0.2 s, turning 70 degrees) and a stop,
// then a jump of the hand (a teleport). Prints the lag, the overshoot, the settling time and the sag (at arm's length);
// with "csv", every frame of each run into weight_test.csv (the game folder) for plotting.
struct TestCase
{
    const char* name;
    float mass, balance, length; // a rod (kg, m, m); length 0: a box, its middle `balance` metres out from the grip
    float twoHanded;             // (a rod: its foregrip 35 cm ahead; a box: the other hand on its far side)
    float stamina; // left, 0..1
    Tuning tune;   // its own multipliers (Weapon Weights, Held Object Weights)
};

void test_f()
{
    const bool csv = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "csv");
    FILE* out = csv ? fopen(va("%s/weight_test.csv", com_gamedir), "w") : nullptr;
    if(out)
    {
        fprintf(out, "case,fps,t,target_x,target_y,drawn_x,drawn_y,drawn_z,off_cm,ang_deg\n");
    }
    using weapons::Key;
    const auto gun = [](const char* name, int slot, float twoHanded, float stamina) {
        return TestCase{name, weapons::value(slot, Key::Mass), weapons::value(slot, Key::Balance) * 0.01f,
            weapons::value(slot, Key::Span) * 0.01f, twoHanded, stamina, weaponTuning(slot)};
    };
    const float box = props::valueFor("maps/b_explob.bsp", props::Key::Mass);
    const Tuning boxTune = propTuning(props::slotForModel("maps/b_explob.bsp"));
    const TestCase cases[] = {
        gun("shotgun 1H", 1, 0.f, 1.f),
        gun("shotgun 2H", 1, 1.f, 1.f),
        gun("rocket 1H", 6, 0.f, 1.f),
        gun("rocket 2H", 6, 1.f, 1.f),
        gun("rocket 1H 50%", 6, 0.f, 0.5f),
        gun("rocket 1H 25%", 6, 0.f, 0.25f),
        gun("rocket 1H 0%", 6, 0.f, 0.f),
        {"box 1H", box, 0.6f, 0.f, 0.f, 1.f, boxTune},
        {"box 2H", box, 0.6f, 0.f, 1.f, 1.f, boxTune},
    };
    const float rates[] = {45.f, 72.f, 90.f, 144.f};
    Con_Printf("vr_weight_test: stiffness %.2f damping %.2f strength %.2f sag %.2f swing %.2f 2h %.1f\n",
        vr_weight_spring_stiffness.value, vr_weight_spring_damping.value, vr_weight_spring_strength.value,
        vr_weight_spring_sag.value, vr_weight_spring_inertia.value, vr_weight_spring_2h.value);
    Con_Printf("%-14s %4s %5s | %6s %6s | %6s %6s | %6s | %6s %6s | %6s %5s\n", "case", "fps", "kg", "lag cm", "lagdeg", "overcm",
        "overdg", "settle", "sag cm", "sagdeg", "jitter", "snap");
    for(const TestCase& tc : cases)
    {
        for(const float fps : rates)
        {
            Load l;
            l.valid = true;
            if(tc.length > 0.f)
            {
                rodLoad(l, tc.mass, tc.balance, tc.length, 0.06f);
                if(tc.twoHanded > 0.f)
                {
                    // Turning about between the handle and a foregrip 35 cm ahead.
                    const float d = tc.balance - 0.175f;
                    const float across = tc.mass * tc.length * tc.length / 12.f;
                    l.com = {d, 0.f, 0.f};
                    l.inertia = {tc.mass * 0.06f * 0.06f, across + tc.mass * d * d, across + tc.mass * d * d};
                }
            }
            else
            {
                // A box held by its side, its middle `balance` out (the big explosive box: 1.14 x 1.14 x 2.36 m).
                const float a = tc.balance * 2.f;
                const float own = tc.mass / 12.f * 2.f * a * a;
                const float d = tc.twoHanded > 0.f ? 0.f : tc.balance; // both hands: about its middle
                l.mass = tc.mass;
                l.com = {d, 0.f, 0.f};
                l.inertia = {own, own + tc.mass * d * d, own + tc.mass * d * d};
            }
            l.twoHanded = tc.twoHanded;
            l.tune = tc.tune;
            const float m = tc.mass * staminaCurve(tc.stamina);
            const float sag = sagShare * std::max(vr_weight_spring_sag.value * tc.tune.sag, 0.f); // at arm's length
            const float dt = 1.f / fps;
            const glm::vec3 start{0.55f, -0.25f, 1.25f};
            Body b;
            b.x = start;
            b.q = quatFromAngles(glm::vec3{0.f});
            glm::vec3 prevX = b.x;
            glm::quat prevQ = b.q;
            float lagCm = 0.f, lagDeg = 0.f, overCm = 0.f, overDeg = 0.f, settle = -1.f, sagCm = 0.f, sagDeg = 0.f, jitter = 0.f;
            glm::vec3 restX{0.f};
            bool snapped = false;
            constexpr float swingFrom = 1.f, swingTime = 0.2f, jumpAt = 3.f;
            const int frames = static_cast<int>(3.5f * fps);
            for(int f = 1; f <= frames; f++)
            {
                const float t = static_cast<float>(f) / fps;
                const float u = std::clamp((t - swingFrom) / swingTime, 0.f, 1.f);
                const float sm = u * u * (3.f - 2.f * u);
                glm::vec3 xt = start + glm::vec3{0.f, 0.6f * sm, 0.f};
                const glm::quat qt = quatFromAngles(glm::vec3{0.f, 70.f * sm, 0.f});
                if(t >= jumpAt)
                {
                    xt += glm::vec3{2.f, 0.f, 0.f};
                }
                if(glm::distance(b.x, xt) * 100.f > std::max(vr_weight_spring_snap.value * tc.tune.snap, 1.f))
                {
                    b.x = xt;
                    b.q = qt;
                    b.v = glm::vec3{0.f};
                    b.w = glm::vec3{0.f};
                    snapped = true;
                }
                else
                {
                    step(b, l, m, prevX, xt, prevQ, qt, dt, sag);
                }
                prevX = xt;
                prevQ = qt;
                const float off = glm::distance(b.x, xt) * 100.f;
                const float ang = glm::degrees(glm::length(rotationVector(qt * glm::conjugate(b.q))));
                if(t < swingFrom)
                {
                    sagCm = off; // at rest before the swing: the sag alone
                    sagDeg = ang;
                }
                else if(t <= swingFrom + swingTime)
                {
                    lagCm = std::max(lagCm, off - sagCm);
                    lagDeg = std::max(lagDeg, ang - sagDeg);
                }
                else if(t < jumpAt)
                {
                    overCm = std::max(overCm, (b.x.y - xt.y) * 100.f); // past the target, the way it went
                    overDeg = std::max(overDeg, std::remainder(anglesFromQuat(b.q).y - 70.f, 360.f));
                    const bool out = std::fabs(off - sagCm) > 0.5f || std::fabs(ang - sagDeg) > 0.5f;
                    if(out)
                    {
                        settle = -1.f;
                    }
                    else if(settle < 0.f)
                    {
                        settle = t - swingFrom - swingTime;
                    }
                    // At rest: how much it still moves between frames over the last half second (none: no jitter).
                    if(t > jumpAt - 0.5f)
                    {
                        if(restX != glm::vec3{0.f})
                        {
                            jitter = std::max(jitter, glm::distance(b.x, restX) * 1000.f);
                        }
                        restX = b.x;
                    }
                }
                if(out)
                {
                    fprintf(out, "%s,%.0f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.3f,%.3f\n", tc.name, fps, t, xt.x, xt.y, b.x.x, b.x.y, b.x.z,
                        off, ang);
                }
            }
            Con_Printf("%-14s %4.0f %5.1f | %6.2f %6.2f | %6.2f %6.2f | %6.3f | %6.2f %6.2f | %6.4f %5s\n", tc.name, fps, m, lagCm,
                lagDeg, overCm, overDeg, settle, sagCm, sagDeg, jitter, snapped ? "yes" : "no");
        }
    }
    if(out)
    {
        fclose(out);
        Con_Printf("vr_weight_test: wrote %s/weight_test.csv\n", com_gamedir);
    }
}

// vr_weight_table: every weapon's and prop's mass and what the weight makes of its damage and its speed thresholds (the
// damage curve, the thing's own Melee and Throw Damage x, heavy leniency), and the level's other things' (their mass as
// the game has it: Box3D's).
void table_f()
{
    using weapons::Key;
    Con_Printf("vr_weight_table: damage x (mass / %.1f kg)^%.2f above, (mass / %.1f kg)^%.2f below, %.2f..%.2f; lenient "
               "above %.1f kg: (from / mass)^%.2f, at least %.2f\n",
        vr_weight_damage_heavy.value, vr_weight_damage_exp.value, vr_weight_damage_light.value, vr_weight_damage_exp.value,
        vr_weight_damage_min.value, vr_weight_damage_max.value, vr_weight_lenient_from.value, vr_weight_lenient.value,
        vr_weight_lenient_min.value);
    Con_Printf("%-4s %-26s %6s | %6s %6s %6s | %7s\n", "slot", "model", "kg", "weight", "melee", "thrown", "speeds");
    const auto row = [](const char* slot, const char* model, float mass, float melee, float thrown) {
        const float curve = damageMultiplier(mass);
        Con_Printf("%-4s %-26s %6.2f | %6.3f %6.3f %6.3f | %7.3f\n", slot, model, mass, curve, curve * melee, curve * thrown,
            leniency(mass));
    };
    for(int slot = 0; slot < weapons::numSlots; slot++)
    {
        const char* id = weapons::cvar(slot, Key::ID)->string;
        if(!id[0] || !strcmp(id, "-1"))
        {
            continue;
        }
        row(va("w%d", slot + 1), id, weapons::value(slot, Key::Mass), weapons::value(slot, Key::MeleeDamage),
            weapons::value(slot, Key::ThrowDamage));
    }
    for(int slot = 0; slot < props::numSlots; slot++)
    {
        const char* id = props::cvar(slot, props::Key::ID)->string;
        if(!id[0] || !strcmp(id, "-1"))
        {
            continue;
        }
        row(va("p%d", slot + 1), id, props::value(slot, props::Key::Mass), props::value(slot, props::Key::MeleeDamage),
            props::value(slot, props::Key::ThrowDamage));
    }
    // The things in the level (a local server): each model's mass as the game has it (its setting, else Box3D's).
    if(!sv.active)
    {
        return;
    }
    qcvm_t* const old = qcvm;
    if(old != &sv.qcvm)
    {
        if(old)
        {
            PR_SwitchQCVM(nullptr);
        }
        PR_SwitchQCVM(&sv.qcvm);
    }
    std::vector<std::string> seen;
    for(int i = 1; i < sv.qcvm.num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        const int index = static_cast<int>(e->v.modelindex);
        const qmodel_t* model = !e->free && index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
        if(!model || model->name[0] == '*' || std::find(seen.begin(), seen.end(), model->name) != seen.end())
        {
            continue;
        }
        const float mass = box3d::propMass(e);
        const int slot = props::slotForModel(model->name);
        if(mass > 0.f && weapons::slotForName(model->name) < 0 && (slot < 0 || props::value(slot, props::Key::Mass) <= 0.f))
        {
            seen.push_back(model->name);
            row("lvl", model->name, mass, props::value(slot, props::Key::MeleeDamage), props::value(slot, props::Key::ThrowDamage));
        }
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

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_weight_test", test_f);
    Cmd_AddCommand("vr_weight_table", table_f);
}

float staminaCurve(float left)
{
    if(!vr_weight_stamina.value)
    {
        return 1.f;
    }
    const float from = std::clamp(vr_weight_stamina_from.value, 0.01f, 1.f);
    if(left >= from)
    {
        return 1.f;
    }
    const float u = std::clamp((from - left) / from, 0.f, 1.f);
    const float most = std::max(vr_weight_stamina_max.value, 1.f);
    return 1.f + (most - 1.f) * std::pow(u, std::max(vr_weight_stamina_curve.value, 0.1f));
}

float staminaMultiplier()
{
    if(easedAt == realtime)
    {
        return easedMult;
    }
    const meleehud::State st = meleehud::state();
    const float left = vr_debug_weight_stamina.value >= 0.f ? std::min(vr_debug_weight_stamina.value, 1.f) : st.stamina ? st.left : 1.f;
    const float target = staminaCurve(left);
    const float dt = easedAt >= 0.0 ? static_cast<float>(std::clamp(realtime - easedAt, 0.0, 0.25)) : 1.f;
    easedAt = realtime;
    easedMult = target + (easedMult - target) * std::exp(-dt / 0.25f);
    if(std::fabs(easedMult - target) < 1e-4f)
    {
        easedMult = target; // (settled exactly: unchanged at 1)
    }
    return easedMult;
}

Load load(int hand)
{
    Load l = hand == 0 || hand == 1 ? computeLoad(hand, nullptr) : Load{};
    l.staminaMult = staminaMultiplier();
    return l;
}

Offset offset(int hand)
{
    return hand == 0 || hand == 1 ? bodies[hand].off : Offset{};
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
        const glm::quat qt = quatFromAngles(s.rot[h] - glm::vec3{0.f, turnYaw, 0.f});
        Load l = computeLoad(h, &s);
        l.staminaMult = staminaMultiplier();
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
            const float snap = std::max(vr_weight_spring_snap.value * l.tune.snap, 1.f);
            // Put back: left too far behind, or the hand jumped (faster than a hand goes: a teleport, the play space
            // re-based, tracking lost and found; as the melee's VR_MELEE_JUMP).
            const bool jumped = b.targetValid && glm::distance(xt, b.xt) > jumpSpeed * dt;
            const bool snapped = b.active && (jumped || glm::distance(b.x, xt) * 100.f > snap ||
                                                 glm::degrees(glm::length(rotationVector(qt * glm::conjugate(b.q)))) > 3.f * snap);
            const bool other = b.model != l.model || b.entity != l.entity; // (a weapon changed, a prop taken)
            if(!b.active || !b.targetValid || snapped || other)
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
                if(snapped && vr_debug_weight.value)
                {
                    Con_Printf("weight: %s put back in the hand\n", h == HAND_MAIN ? "main" : "off");
                }
            }
            else
            {
                const float m = std::max(l.mass * l.staminaMult, 0.01f);
                const float sag = sagShare * std::max(vr_weight_spring_sag.value * l.tune.sag, 0.f) *
                                  (0.35f + 0.65f * std::pow(extension(s, h, b.x, base, turnYaw), 2.f));
                step(b, l, m, b.xt, xt, b.qt, qt, dt, sag);
            }
            b.xt = xt;
            b.qt = qt;
            b.targetValid = true;
            b.off.active = true;
            b.off.distance = glm::distance(b.x, xt) * 100.f;
            b.off.angle = glm::degrees(glm::length(rotationVector(qt * glm::conjugate(b.q))));
            trace(h, l, xt, qt, b.x, b.q, glm::length(b.v));
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
    const float k = std::max(vr_weight_damage_exp.value, 0.f);
    if(!(mass > 0.f) || k == 0.f)
    {
        return 1.f;
    }
    const float light = std::max(vr_weight_damage_light.value, 0.01f);
    const float heavy = std::max(vr_weight_damage_heavy.value, light);
    const float ref = mass > heavy ? heavy : mass < light ? light : mass;
    const float lo = std::min(vr_weight_damage_min.value, 1.f), hi = std::max(vr_weight_damage_max.value, 1.f);
    return std::clamp(std::pow(mass / ref, k), lo, hi);
}

float leniency(float mass)
{
    const float j = std::max(vr_weight_lenient.value, 0.f);
    const float from = std::max(vr_weight_lenient_from.value, 0.01f);
    if(!(mass > from) || j == 0.f)
    {
        return 1.f;
    }
    return std::clamp(std::pow(from / mass, j), std::clamp(vr_weight_lenient_min.value, 0.01f, 1.f), 1.f);
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
    easedMult = 1.f;
    easedAt = -1.0;
}

} // namespace qvr::weight
