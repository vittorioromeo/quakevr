// vr_weight_diag.cpp -- vr_weight_test (the spring on its own, offline), vr_weight_table (every weapon's and prop's mass and
// what it makes of the damage and the speeds) and vr_throw_table (throws by weight).
// Split from vr_weight.cpp (a pure move); what they share is in vr_weight_internal.hpp.

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
#include "Zancle/Math/Sin.hpp"
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
    bool empty{false}; // an empty hand: its mass is the stamina's (vr_weight_stamina_empty)
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
    const float empty = za::max(vr_weight_stamina_empty.value, 0.f);
    const TestCase emptyCases[] = {
        {"empty 100%", empty * staminaShareFor(1.f), 0.04f, 0.12f, 0.f, 1.f, Tuning{}, true},
        {"empty 50%", empty * staminaShareFor(0.5f), 0.04f, 0.12f, 0.f, 0.5f, Tuning{}, true},
        {"empty 25%", empty * staminaShareFor(0.25f), 0.04f, 0.12f, 0.f, 0.25f, Tuning{}, true},
        {"empty 5%", empty * staminaShareFor(0.05f), 0.04f, 0.12f, 0.f, 0.05f, Tuning{}, true},
    };
    Con_Printf("vr_weight_test: stiffness %.2f damping %.2f strength %.2f sag %.2f swing %.2f 2h %.1f roll %.2f\n",
        vr_weight_spring_stiffness.value, vr_weight_spring_damping.value, vr_weight_spring_strength.value,
        vr_weight_spring_sag.value, vr_weight_spring_inertia.value, vr_weight_spring_2h.value, vr_weight_spring_roll.value);
    Con_Printf("%-14s %4s %5s | %6s %6s | %6s %6s | %6s | %6s %6s | %6s %5s\n", "case", "fps", "kg", "lag cm", "lagdeg", "overcm",
        "overdg", "settle", "sag cm", "sagdeg", "jitter", "snap");
    // A test case's load, and its mass as the spring has it (its stamina's).
    const auto caseLoad = [](const TestCase& tc, float& m) {
        Load l;
        l.valid = true;
        if(tc.length > 0.f)
        {
            if(tc.empty)
            {
                rodLoad(l, za::max(tc.mass, 0.01f), tc.balance, tc.length, 0.04f);
            }
            else
            {
                gunLoad(l, za::max(tc.mass, 0.01f), tc.balance, tc.length);
            }
            if(tc.twoHanded > 0.f)
            {
                // Turning about between the handle and a foregrip 35 cm ahead.
                const float d = tc.balance - 0.175f;
                const float across = tc.mass * tc.length * tc.length / 12.f;
                l.com = {d, 0.f, 0.f};
                l.inertia = {tc.mass * gunRollRadius2, across + tc.mass * d * d, across + tc.mass * d * d};
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
        m = za::max(tc.empty ? tc.mass
                              : tc.mass * staminaCurve(tc.stamina) +
                                    za::max(vr_weight_stamina_add.value, 0.f) * staminaShareFor(tc.stamina),
            0.01f);
        return l;
    };
    za::Vector<TestCase> all(cases, cases + za::getArraySize(cases));
    all.emplaceBackRange(emptyCases, za::getArraySize(emptyCases));
    for(const TestCase& tc : all)
    {
        for(const float fps : rates)
        {
            float m = 0.f;
            const Load l = caseLoad(tc, m);
            const float sag = sagShare * za::max(vr_weight_spring_sag.value * tc.tune.sag, 0.f); // at arm's length
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
                const float u = za::clamp((t - swingFrom) / swingTime, 0.f, 1.f);
                const float sm = u * u * (3.f - 2.f * u);
                glm::vec3 xt = start + glm::vec3{0.f, 0.6f * sm, 0.f};
                const glm::quat qt = quatFromAngles(glm::vec3{0.f, 70.f * sm, 0.f});
                if(t >= jumpAt)
                {
                    xt += glm::vec3{2.f, 0.f, 0.f};
                }
                if(const float snapCm = za::max(vr_weight_spring_snap.value * tc.tune.snap, 1.f);
                    putBack(b, xt, qt, snapCm) || step(b, l, m, prevX, xt, prevQ, qt, dt, sag))
                {
                    b.x = xt;
                    b.q = qt;
                    b.v = glm::vec3{0.f};
                    b.w = glm::vec3{0.f};
                    snapped = true;
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
                    lagCm = za::max(lagCm, off - sagCm);
                    lagDeg = za::max(lagDeg, ang - sagDeg);
                }
                else if(t < jumpAt)
                {
                    overCm = za::max(overCm, (b.x.y - xt.y) * 100.f); // past the target, the way it went
                    overDeg = za::max(overDeg, za::remainder(anglesFromQuat(b.q).y - 70.f, 360.f));
                    const bool out = za::fabs(off - sagCm) > 0.5f || za::fabs(ang - sagDeg) > 0.5f;
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
                            jitter = za::max(jitter, glm::distance(b.x, restX) * 1000.f);
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

    // A wrist snap with the laser cannon: the hand turns 170 degrees (pitch, yaw or roll) in 0.1 or 0.05 s and stays, or
    // turns straight back; then 1.5 s still. The drawn weapon must turn the short way (as the hand did) or be put back in
    // the hand, never spin round the other way. Per case and frame rate: the largest angle off the hand, the most it
    // turned beyond the hand's own turn ("extra": about 360 when it went the long way), the runs that went the long way
    // and the runs put back in the hand. vr_debug_weight: every run.
    Con_Printf("%-14s %4s | %4s %6s %6s %4s %4s\n", "wrist snap", "fps", "runs", "maxdeg", "extra", "long", "snap");
    const TestCase snapCases[] = {gun("laser 1H", 9, 0.f, 1.f), gun("laser 2H", 9, 1.f, 1.f), gun("laser 1H 0%", 9, 0.f, 0.f)};
    for(const TestCase& tc : snapCases)
    {
        for(const float fps : {72.f, 90.f, 144.f})
        {
            float m = 0.f;
            const Load l = caseLoad(tc, m);
            const float sag = sagShare * za::max(vr_weight_spring_sag.value * tc.tune.sag, 0.f);
            const float snapCm = za::max(vr_weight_spring_snap.value * tc.tune.snap, 1.f);
            const float dt = 1.f / fps;
            int runs = 0, longWay = 0, snaps = 0;
            float maxDeg = 0.f, maxExtra = 0.f;
            for(int axis = 0; axis < 3; axis++)
            {
                for(const float dur : {0.2f, 0.15f, 0.1f, 0.07f, 0.05f, 0.03f})
                {
                    for(const bool back : {false, true})
                    {
                        const glm::vec3 x0{0.55f, -0.25f, 1.25f};
                        const auto turnAt = [&](float t) {
                            const float u = za::clamp(t / dur, 0.f, 1.f), w = za::clamp(t / dur - 1.f, 0.f, 1.f);
                            float a = 170.f * u * u * (3.f - 2.f * u);
                            if(back)
                            {
                                a -= 170.f * w * w * (3.f - 2.f * w);
                            }
                            glm::vec3 angles{0.f};
                            angles[axis] = a;
                            struct Turn
                            {
                                glm::quat q;
                                float angle;
                            };
                            return Turn{quatFromAngles(angles), a};
                        };
                        const glm::vec3 dir = glm::normalize(rotationVector(turnAt(dur).q * glm::conjugate(turnAt(0.f).q)));
                        Body b;
                        b.x = x0;
                        b.q = turnAt(0.f).q;
                        glm::quat prevQ = b.q;
                        float turned = 0.f, runMax = 0.f, runExtra = 0.f;
                        bool snapped = false;
                        const int frames = static_cast<int>((2.f * dur + 1.5f) * fps);
                        for(int f = 1; f <= frames; f++)
                        {
                            const auto [qt, want] = turnAt(static_cast<float>(f) * dt);
                            const glm::quat before = b.q;
                            if(putBack(b, x0, qt, snapCm) || step(b, l, m, x0, x0, prevQ, qt, dt, sag))
                            {
                                // Put back in the hand: a jump, not a turn either way (it may be more than half a turn
                                // off the hand by the frame's end, so the short way would count it as the long way).
                                b.x = x0;
                                b.q = qt;
                                b.v = glm::vec3{0.f};
                                b.w = glm::vec3{0.f};
                                snapped = true;
                                turned = want;
                            }
                            else
                            {
                                turned += glm::degrees(glm::dot(rotationVector(b.q * glm::conjugate(before)), dir));
                            }
                            prevQ = qt;
                            runMax = za::max(runMax, glm::degrees(glm::length(rotationVector(qt * glm::conjugate(b.q)))));
                            runExtra = za::max(runExtra, za::fabs(turned - want));
                        }
                        runs++;
                        longWay += runExtra > 200.f;
                        snaps += snapped;
                        maxDeg = za::max(maxDeg, runMax);
                        maxExtra = za::max(maxExtra, runExtra);
                        if(vr_debug_weight.value)
                        {
                            Con_Printf("  %s %.0f fps: %s %.2f s%s: max %.1f deg, extra %.1f, %s\n", tc.name, fps,
                                axis == 0 ? "pitch" : axis == 1 ? "yaw" : "roll", dur, back ? " and back" : "", runMax, runExtra,
                                snapped ? "put back" : "sprung");
                        }
                    }
                }
            }
            Con_Printf("%-14s %4.0f | %4d %6.1f %6.1f %4d %4d\n", tc.name, fps, runs, maxDeg, maxExtra, longWay, snaps);
        }
    }

    // A step of the wrist (NOTES.md vrfiringrange_2026-10-02_01-14-49: a roll as quick as no weight): the hand turns 45
    // or 90 degrees about one of its axes from one frame to the next (90 fps) and stays. Per weapon, step and axis: the time
    // (ms) the held weapon takes to turn 90% of it, and how far it overshoots (% of the step).
    Con_Printf("%-14s %5s %3s | %6s %5s | %6s %5s | %6s %5s\n", "wrist step", "kg", "deg", "p90ms", "p%", "y90ms", "y%", "r90ms",
        "r%");
    const TestCase stepCases[] = {{"pistol 1.2kg", 1.2f, 0.05f, 0.25f, 0.f, 1.f, Tuning{}}, gun("grapple 1H", 17, 0.f, 1.f),
        gun("shotgun 1H", 1, 0.f, 1.f), gun("laser 1H", 9, 0.f, 1.f), gun("laser 2H", 9, 1.f, 1.f), gun("rocket 1H", 6, 0.f, 1.f)};
    for(const TestCase& tc : stepCases)
    {
        for(const float stepDeg : {45.f, 90.f})
        {
            float m = 0.f;
            const Load l = caseLoad(tc, m);
            constexpr float fps = 90.f;
            const float dt = 1.f / fps;
            float t90[3]{-1.f, -1.f, -1.f}, over[3]{0.f, 0.f, 0.f};
            for(int axis = 0; axis < 3; axis++)
            {
                // Hand axes: 0 pitch (about left), 1 yaw (about up), 2 roll (about forward). No sag: the turn alone.
                glm::vec3 about{0.f};
                about[axis == 0 ? 1 : axis == 1 ? 2 : 0] = 1.f;
                const glm::vec3 x0{0.55f, -0.25f, 1.25f};
                const glm::quat q0{1.f, 0.f, 0.f, 0.f};
                const glm::quat qt = glm::angleAxis(glm::radians(stepDeg), about);
                Body b;
                b.x = x0;
                b.q = q0;
                glm::quat prevQ = q0;
                float was = 0.f;
                for(int f = 1; f <= static_cast<int>(1.5f * fps); f++)
                {
                    if(step(b, l, m, x0, x0, prevQ, qt, dt, 0.f))
                    {
                        b.q = qt;
                        b.w = glm::vec3{0.f};
                    }
                    prevQ = qt;
                    const float turned = glm::degrees(glm::dot(rotationVector(b.q), about));
                    if(t90[axis] < 0.f && turned >= 0.9f * stepDeg)
                    {
                        // (between this frame and the last)
                        const float part = (turned - 0.9f * stepDeg) / za::max(turned - was, 1e-4f);
                        t90[axis] = (static_cast<float>(f) - za::clamp(part, 0.f, 1.f)) * dt * 1000.f;
                    }
                    was = turned;
                    over[axis] = za::max(over[axis], (turned - stepDeg) / stepDeg * 100.f);
                }
            }
            Con_Printf("%-14s %5.1f %3.0f | %6.0f %5.1f | %6.0f %5.1f | %6.0f %5.1f\n", tc.name, m, stepDeg, t90[0], over[0], t90[1],
                over[1], t90[2], over[2]);
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
    // (throw1: one hand's top throw by its mass, m/s: throwLimit.)
    Con_Printf("%-4s %-26s %6s | %6s %6s %6s | %7s %6s\n", "slot", "model", "kg", "weight", "melee", "thrown", "speeds",
        "throw1");
    const auto row = [](const char* slot, const char* model, float mass, float melee, float thrown) {
        const float curve = damageMultiplier(mass);
        Con_Printf("%-4s %-26s %6.2f | %6.3f %6.3f %6.3f | %7.3f %6.2f\n", slot, model, mass, curve, curve * melee,
            curve * thrown, leniency(mass), throwLimit(mass, 1));
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
        if(props::value(slot, props::Key::Mass) <= 0.f)
        {
            // Its mass estimated, thing by thing (the level's rows below).
            Con_Printf("%-4s %-26s %6s | %6s %6.3f %6.3f | %7s\n", va("p%d", slot + 1), id, "est", "-",
                props::value(slot, props::Key::MeleeDamage), props::value(slot, props::Key::ThrowDamage), "-");
            continue;
        }
        row(va("p%d", slot + 1), id, props::value(slot, props::Key::Mass), props::value(slot, props::Key::MeleeDamage),
            props::value(slot, props::Key::ThrowDamage));
    }
    // What each hand holds, as the weight has it (tests: a held prop's Size and Mass x weigh it again at once).
    for(int hand = 1; hand >= 0; hand--)
    {
        if(const Load l = load(hand); l.valid && !l.empty)
        {
            Con_Printf("hand %s: %s %.2f kg\n", hand == 1 ? "main" : "off", l.model ? l.model : "?", l.mass);
        }
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
    za::Vector<za::String> seen;
    for(int i = 1; i < sv.qcvm.num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        const int index = static_cast<int>(e->v.modelindex);
        const qmodel_t* model = !e->free && index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
        if(!model || qvr::modelmeta::has(model, qvr::modelmeta::Trait::Submodel) || za::find(seen.begin(), seen.end(), model->name) != seen.end())
        {
            continue;
        }
        const float mass = box3d::propMass(e);
        const int slot = props::slotForModel(model);
        if(mass > 0.f && weapons::slotForName(model->name) < 0 && (slot < 0 || props::value(slot, props::Key::Mass) <= 0.f))
        {
            seen.pushBack(model->name);
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

// vr_throw_table [hand speed m/s] [flick m/s]: throws by weight (throwVelocity), for the weapons and props that have a mass:
// a throw whose hand's estimate is that fast (8 m/s: a hard overarm throw), that much of it the wrist's flick (2), the
// flick along it. Each: the speed it leaves at and its reach (level ground, at 45 degrees, at vr_throw_gravity) with the
// one hand and two.
void throwTable_f()
{
    const float hand = Cmd_Argc() > 1 ? Q_atof(Cmd_Argv(1)) : 8.f;
    const float flick = Cmd_Argc() > 2 ? Q_atof(Cmd_Argv(2)) : 2.f;
    const float g = vr_throw_gravity.value > 0.f ? vr_throw_gravity.value : 9.81f;
    const glm::vec3 vel{hand, 0.f, 0.f}, fl{za::min(flick, hand), 0.f, 0.f};
    Con_Printf("vr_throw_table: hand %.1f m/s (flick %.1f); max %.0f m/s up to %.1f kg, ^%.2f, knee %.2f\n", hand, flick,
        vr_throw_max_speed.value, vr_throw_mass_light.value, vr_throw_mass_exp.value, vr_throw_mass_knee.value);
    Con_Printf("  whole flick up to %.1f kg; two hands as %.1fx lighter; reach at 45 degrees, %.2f m/s^2\n",
        vr_throw_flick_mass.value, vr_throw_2h_strength.value, g);
    Con_Printf("  hits from (m/s): %.0f u/s times its scale (top %.0f m/s); spin limits (rad/s): %.0f, ^%.2f\n",
        vr_throw_hit_min_speed.value, vr_throw_hit_top.value, vr_throw_spin_max.value, vr_throw_spin_mass_exp.value);
    Con_Printf("%-4s %-24s %5s | %11s %5s | %11s %5s | %4s %5s | %5s %5s\n", "slot", "model", "kg", "1h m/s m", "lim",
        "2h m/s m", "lim", "hit", "scale", "spin1", "spin2");
    const auto row = [&](const char* slot, const char* model, float mass) {
        const float debug = vr_debug_throw.value;
        vr_debug_throw.value = 0.f;
        const float one = glm::length(throwVelocity(vel, fl, mass, 1).vel);
        const float two = glm::length(throwVelocity(vel, fl, mass, 2).vel);
        const float lim1 = throwLimit(mass, 1), lim2 = throwLimit(mass, 2);
        const float spin1 = glm::length(throwSpin(glm::vec3{vr_throw_spin_max.value, 0.f, 0.f}, mass, 1));
        const float spin2 = glm::length(throwSpin(glm::vec3{vr_throw_spin_max.value, 0.f, 0.f}, mass, 2));
        vr_debug_throw.value = debug;
        Con_Printf("%-4s %-24s %5.1f | %5.1f %5.1f %5.1f | %5.1f %5.1f %5.1f | %4.1f %5.2f | %5.1f %5.1f\n",
            slot, model, mass, one, one * one / g, lim1, two, two * two / g, lim2,
            throwHitSpeed(mass, 1.f) / units::metresToUnits(), throwScale(mass), spin1, spin2);
    };
    for(int slot = 0; slot < weapons::numSlots; slot++)
    {
        const char* id = weapons::cvar(slot, weapons::Key::ID)->string;
        if(id[0] && strcmp(id, "-1") && weapons::value(slot, weapons::Key::Mass) > 0.f)
        {
            row(va("w%d", slot + 1), id, weapons::value(slot, weapons::Key::Mass));
        }
    }
    for(int slot = 0; slot < props::numSlots; slot++)
    {
        const char* id = props::cvar(slot, props::Key::ID)->string;
        if(id[0] && strcmp(id, "-1") && props::value(slot, props::Key::Mass) > 0.f)
        {
            row(va("p%d", slot + 1), id, props::value(slot, props::Key::Mass));
        }
    }
}

// vr_heavy_test [slot]: heavy handling (vr_heavy*) offline, per weapon (or one slot, 0-based), at 90 fps: its weight class;
// its droop in one hand and two (degrees); an abrupt wrist snap (yaw 60 degrees in 0.1 s, held) one-handed with the
// struggle and without it (vr_heavy_struggle 0) and two-handed: the most it lags (degrees), its overshoot (% of the turn)
// and the time to 90% (ms); the fastest 60 ms turn of that snap and of an extreme one (120 in 0.08 s) against its
// wrenched-out limit (vr_weight_drop*: "drop" if over); a one-handed shot's kick, one hand and two: its peak (degrees up)
// and the time back under a tenth of it (ms).
void heavyTest_f()
{
    using weapons::Key;
    const int only = Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : -1;
    constexpr float fps = 90.f, dt = 1.f / fps;
    const auto loadOf = [](int slot, float twoHanded, bool struggle) {
        Load l;
        l.valid = true;
        const float mass = weapons::value(slot, Key::Mass), balance = weapons::value(slot, Key::Balance) * 0.01f;
        const float length = za::max(weapons::value(slot, Key::Span), 1.f) * 0.01f;
        gunLoad(l, mass, balance, length);
        if(twoHanded > 0.f)
        {
            const float d = balance - 0.175f, across = mass * length * length / 12.f; // (as vr_weight_test's)
            l.com = {d, 0.f, 0.f};
            l.inertia = {mass * gunRollRadius2, across + mass * d * d, across + mass * d * d};
        }
        l.twoHanded = twoHanded;
        l.tune = weaponTuning(slot);
        l.slot = slot;
        l.heft = heft(slot);
        l.struggle = struggle ? l.heft * (1.f - twoHanded) * za::clamp(vr_heavy_struggle.value, 0.f, 1.f) : 0.f;
        return l;
    };
    // The snap: degrees of yaw at `t` (s) for `deg` over `dur`, smoothstepped and held.
    const auto snapAt = [](float t, float deg, float dur) {
        const float u = za::clamp(t / dur, 0.f, 1.f);
        return deg * u * u * (3.f - 2.f * u);
    };
    const auto fastest = [&](float deg, float dur) {
        float most = 0.f;
        for(float t = 0.06f; t <= dur + 0.06f; t += 0.002f)
        {
            most = za::max(most, (snapAt(t, deg, dur) - snapAt(t - 0.06f, deg, dur)) / 0.06f);
        }
        return most;
    };
    struct Snap
    {
        float lag, over, t90;
        int putBack;
    };
    // `deg` over `dur`, held (or straight back over as long: `back`).
    const auto snap = [&](const Load& l, float deg, float dur, bool back) {
        Body b;
        const glm::vec3 x0{0.55f, -0.25f, 1.25f};
        b.x = x0;
        glm::quat prev{1.f, 0.f, 0.f, 0.f};
        Snap out{0.f, 0.f, -1.f, 0};
        const float m = za::max(l.mass, 0.01f);
        for(int f = 1; f <= static_cast<int>(1.5f * fps); f++)
        {
            const float t = static_cast<float>(f) * dt;
            const float want = snapAt(t, deg, dur) - (back ? snapAt(t - dur, deg, dur) : 0.f);
            const glm::quat qt = glm::angleAxis(glm::radians(want), glm::vec3{0.f, 0.f, 1.f});
            if(step(b, l, m, x0, x0, prev, qt, dt, 0.f))
            {
                b.q = qt;
                b.w = glm::vec3{0.f};
                out.putBack++;
            }
            if(back)
            {
                out.lag = za::max(out.lag, glm::degrees(glm::length(rotationVector(qt * glm::conjugate(b.q)))));
                prev = qt;
                continue;
            }
            prev = qt;
            const float turned = glm::degrees(glm::dot(rotationVector(b.q), glm::vec3{0.f, 0.f, 1.f}));
            out.lag = za::max(out.lag, want - turned);
            out.over = za::max(out.over, (turned - deg) / deg * 100.f);
            if(out.t90 < 0.f && turned >= 0.9f * deg)
            {
                out.t90 = t * 1000.f;
            }
        }
        return out;
    };
    struct Kick
    {
        float peak, back;
    };
    const auto kick = [&](Load l, int slot) {
        Body b;
        const glm::vec3 x0{0.55f, -0.25f, 1.25f};
        b.x = x0;
        const glm::quat q0{1.f, 0.f, 0.f, 0.f};
        const float speed = kickSpeed(slot, l.twoHanded);
        b.w = glm::vec3{0.f, -glm::radians(speed), 0.f};
        const float share = l.heft * (1.f - l.twoHanded), time = za::max(vr_heavy_recoil_time.value, 0.01f);
        Kick out{0.f, -1.f};
        const float m = za::max(l.mass, 0.01f);
        for(int f = 1; f <= static_cast<int>(2.f * fps); f++)
        {
            const float t = static_cast<float>(f) * dt;
            l.kick = speed > 0.f ? share * za::max(1.f - t / time, 0.f) : 0.f;
            (void)step(b, l, m, x0, x0, q0, q0, dt, 0.f);
            const float up = -glm::degrees(glm::dot(rotationVector(b.q), glm::vec3{0.f, 1.f, 0.f}));
            if(up > out.peak)
            {
                out.peak = up;
                out.back = -1.f;
            }
            else if(out.back < 0.f && out.peak > 0.f && up < 0.1f * out.peak)
            {
                out.back = t * 1000.f;
            }
        }
        return out;
    };
    Con_Printf("vr_heavy_test: struggle %.2f (stiffness %.2fx, damping %.2fx at heft 1), droop %.1f deg, kick %.0f deg/s "
               "(recovery %.2fx over %.2f s), drop %s\n",
        vr_heavy_struggle.value, vr_heavy_struggle_stiffness.value, vr_heavy_struggle_damping.value, vr_heavy_droop_deg.value,
        vr_heavy_recoil_kick.value, vr_heavy_recoil_recovery.value, vr_heavy_recoil_time.value, vr_weight_drop.value ? "on" : "off");
    Con_Printf("%-4s %-30s %5s %4s | %4s %4s | %-15s | %-15s | %-15s | %5s %5s %5s | %-11s | %-11s | %s\n", "slot", "model",
        "kg", "heft", "drp1", "drp2", "snap1H lag/ov/t90", "no-struggle 1H", "snap2H", "abrpt", "extr", "lim1H", "kick1H pk/ms",
        "kick2H pk/ms", "yaw170+back 1H off/putback");
    const float abrupt = fastest(60.f, 0.1f), extreme = fastest(120.f, 0.08f);
    for(int slot = 0; slot < weapons::numSlots; slot++)
    {
        const char* id = weapons::cvar(slot, Key::ID)->string;
        if((only >= 0 && slot != only) || !id[0] || !strcmp(id, "-1") || slot == weapons::fistSlot() ||
            weapons::value(slot, Key::Mass) <= 0.01f)
        {
            continue;
        }
        const Load one = loadOf(slot, 0.f, true), plain = loadOf(slot, 0.f, false), two = loadOf(slot, 1.f, true);
        const Snap s1 = snap(one, 60.f, 0.1f, false), s0 = snap(plain, 60.f, 0.1f, false), s2 = snap(two, 60.f, 0.1f, false);
        const Snap y1 = snap(one, 170.f, 0.4f, true); // (heavy_wrist_snap.mock's yaw_slow: 425 deg/s on average)
        const Kick k1 = kick(one, slot), k2 = kick(two, slot);
        const float limit = dropLimit(one.mass, 0.f);
        Con_Printf("%-4d %-30.30s %5.1f %4.2f | %4.1f %4.1f | %4.0f %4.0f %5.0f | %4.0f %4.0f %5.0f | %4.0f %4.0f %5.0f | %5s %5s "
                   "%5.0f | %4.1f %6.0f | %4.1f %6.0f | %4.0f %d\n",
            slot, id, one.mass, one.heft, droop(slot, 0.f), droop(slot, 1.f), s1.lag, s1.over, s1.t90, s0.lag, s0.over, s0.t90,
            s2.lag, s2.over, s2.t90, limit > 0.f && abrupt > limit ? "drop" : "-", limit > 0.f && extreme > limit ? "drop" : "-",
            limit, k1.peak, k1.back, k2.peak, k2.back, y1.lag, y1.putBack);
    }
    Con_Printf("vr_heavy_test: the abrupt snap turns %.0f deg/s at most over 60 ms, the extreme one %.0f\n", abrupt, extreme);
}

// vr_heavy_wiggle [slot]: heavy handling's stability offline at 90 fps, per weapon with a Heft (or one slot, 0-based),
// one-handed (with the struggle and the droop) and two-handed: the wrist wiggled (yaw +-A degrees at F Hz for 1.5 s,
// then held), a flick (30 degrees in 60 ms and back; and 90) and a blow stopped dead (pitched down at 900 deg/s with the grip
// moving 3 m/s, stopped at once: the hit). Per motion: gain (the weapon's yaw swing over the wrist's, the last second),
// spd (its fastest turn over the wrist's), off (the most it turns off the wiggle's axis, degrees), ring (its swings
// back and forth over 1 degree once the hand is still), late (degrees still off 0.3 s after), pb (put back).
void heavyWiggle_f()
{
    using weapons::Key;
    const int only = Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : -1;
    // vr_heavy_wiggle <slot> <motion 0..5> <hands 1|2>: that one's turn every third frame too.
    const int traceMotion = Cmd_Argc() > 2 ? atoi(Cmd_Argv(2)) : -1, traceHands = Cmd_Argc() > 3 ? atoi(Cmd_Argv(3)) : 1;
    constexpr float fps = 90.f, dt = 1.f / fps;
    struct Motion
    {
        const char* name;
        int kind; // 0 wiggle, 1 flick, 2 blow
        float amp, hz;
        int about = 2; // the hand's axis: 0 its roll (the forearm's twist), 1 pitch, 2 yaw
    };
    constexpr Motion motions[] = {{"w30@3", 0, 30.f, 3.f}, {"w30@5", 0, 30.f, 5.f}, {"w60@2", 0, 60.f, 2.f},
        {"w45@4", 0, 45.f, 4.f}, {"flick", 1, 30.f, 0.f}, {"blow", 2, 0.f, 0.f, 1}, {"flick90", 1, 90.f, 0.f},
        {"roll45@3", 0, 45.f, 3.f, 0}, {"pitch45@3", 0, 45.f, 3.f, 1}, {"rflick", 1, 45.f, 0.f, 0}};
    struct Result
    {
        float gain, spd, off, late;
        int ring, putBack;
    };
    const auto run = [&](const Load& l, float droopDeg, const Motion& mo, bool trace) {
        const glm::vec3 x0{0.55f, -0.25f, 1.25f};
        const glm::quat droopQ = glm::angleAxis(glm::radians(droopDeg), glm::vec3{0.f, 1.f, 0.f});
        glm::vec3 axis{0.f};
        axis[mo.about] = 1.f;
        const float moving = mo.kind == 0 ? 1.5f : mo.kind == 1 ? 0.12f : 0.1f;
        const auto angleAt = [&](float t) {
            if(mo.kind == 0)
            {
                return t < moving ? mo.amp * za::sin(2.f * 3.14159265f * mo.hz * t) : 0.f;
            }
            if(mo.kind == 1)
            {
                const float u = za::clamp(t / 0.06f, 0.f, 2.f), s = u <= 1.f ? u : 2.f - u;
                return mo.amp * s * s * (3.f - 2.f * s);
            }
            return 900.f * za::min(t, moving); // (the blow: straight on, then stopped dead)
        };
        const auto posAt = [&](float t) {
            return mo.kind == 2 ? x0 + glm::vec3{0.f, 0.f, -3.f * za::min(t, moving)} : x0;
        };
        const auto turnAt = [&](float t) { return glm::angleAxis(glm::radians(angleAt(t)), axis) * droopQ; };
        Body b;
        b.x = x0;
        b.q = turnAt(0.f);
        Result r{0.f, 0.f, 0.f, 0.f, 0, 0};
        const float m = za::max(l.mass, 0.01f);
        float handFastest = 0.f, last = 0.f, lastAngle = angleAt(0.f);
        float most = 0.f;
        for(int f = 1; f <= static_cast<int>(2.5f * fps); f++)
        {
            const float t = static_cast<float>(f) * dt;
            const glm::quat qt = turnAt(t);
            const glm::vec3 xt = posAt(t), xt0 = posAt(t - dt);
            const glm::quat qt0 = turnAt(t - dt);
            const glm::quat before = b.q;
            if(step(b, l, m, xt0, xt, qt0, qt, dt, 0.f))
            {
                b.q = qt;
                b.w = glm::vec3{0.f};
                b.v = glm::vec3{0.f};
                r.putBack++;
            }
            const float want = angleAt(t);
            handFastest = za::max(handFastest, za::fabs(want - lastAngle) / dt);
            lastAngle = want;
            r.spd = za::max(r.spd, glm::degrees(glm::length(rotationVector(b.q * glm::conjugate(before)))) / dt);
            // Its turn off the hand's (droop's) rest, in the world: along the motion's axis and off it.
            const glm::vec3 turned = glm::degrees(rotationVector(b.q * glm::conjugate(droopQ)));
            const float along = glm::dot(turned, axis);
            r.off = za::max(r.off, glm::length(turned - axis * along));
            if(trace && f % 3 == 0)
            {
                Con_Printf("t %.3f hand %.1f weapon %.1f off %.1f\n", t, want, along, glm::length(turned - axis * along));
            }
            if(mo.kind == 0 && t > moving - 1.f && t < moving)
            {
                most = za::max(most, za::fabs(along));
            }
            if(t >= moving)
            {
                const float err = along - want;
                if(za::fabs(err) > 1.f && last != 0.f && (err > 0.f) != (last > 0.f))
                {
                    r.ring++;
                }
                if(za::fabs(err) > 1.f)
                {
                    last = err;
                }
                if(t >= moving + 0.3f)
                {
                    r.late = za::max(r.late, za::fabs(err));
                }
            }
        }
        r.gain = mo.kind == 0 ? most / mo.amp : 0.f;
        r.spd = handFastest > 0.f ? r.spd / handFastest : 0.f;
        return r;
    };
    Con_Printf("vr_heavy_wiggle: gain spd off ring late pb per motion (1H with struggle and droop; 2H)\n");
    for(int slot = 0; slot < weapons::numSlots; slot++)
    {
        const char* id = weapons::cvar(slot, Key::ID)->string;
        if((only >= 0 && slot != only) || !id[0] || !strcmp(id, "-1") || slot == weapons::fistSlot() ||
            weapons::value(slot, Key::Mass) <= 0.01f || (only < 0 && heft(slot) <= 0.f))
        {
            continue;
        }
        for(int hands = 1; hands <= 2; hands++)
        {
            Load l;
            l.valid = true;
            const float mass = weapons::value(slot, Key::Mass), balance = weapons::value(slot, Key::Balance) * 0.01f;
            const float length = za::max(weapons::value(slot, Key::Span), 1.f) * 0.01f;
            gunLoad(l, mass, balance, length);
            const float twoHanded = hands == 2 ? 1.f : 0.f;
            if(hands == 2)
            {
                const float d = balance - 0.175f, across = mass * length * length / 12.f; // (as vr_heavy_test's)
                l.com = {d, 0.f, 0.f};
                l.inertia = {mass * gunRollRadius2, across + mass * d * d, across + mass * d * d};
            }
            l.twoHanded = twoHanded;
            l.tune = weaponTuning(slot);
            l.slot = slot;
            l.heft = heft(slot);
            l.struggle = l.heft * (1.f - twoHanded) * za::clamp(vr_heavy_struggle.value, 0.f, 1.f);
            const float droopDeg = droop(slot, twoHanded);
            char line[512];
            size_t used = 0;
            line[0] = 0;
            for(const Motion& mo : motions)
            {
                const int index = static_cast<int>(&mo - motions);
                const Result r = run(l, droopDeg, mo, index == traceMotion && hands == traceHands);
                used += static_cast<size_t>(q_snprintf(line + used, sizeof(line) - used, " | %s %.2f %.1f %.0f %d %.1f %d", mo.name,
                    r.gain, r.spd, r.off, r.ring, r.late, r.putBack));
                used = za::min(used, sizeof(line) - 1);
            }
            Con_Printf("%d %.12s %dH%s\n", slot, id, hands, line);
        }
    }
}

} // namespace

void registerDiagCommands()
{
    Cmd_AddCommand("vr_heavy_test", heavyTest_f);
    Cmd_AddCommand("vr_heavy_wiggle", heavyWiggle_f);
    Cmd_AddCommand("vr_throw_table", throwTable_f);
    Cmd_AddCommand("vr_weight_test", test_f);
    Cmd_AddCommand("vr_weight_table", table_f);
}

} // namespace qvr::weight
