// vr_playergore.cpp -- see vr_playergore.hpp.

#include "vr_playergore.hpp"

#include "vr_bloodstream.hpp"
#include "vr_cvars.hpp"
#include "vr_decals.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_geom.hpp"
#include "vr_gore.hpp"
#include "vr_particles.hpp"
#include "vr_progs.hpp"
#include "vr_random.hpp"
#include "vr_selfshot.hpp"
#include "vr_testkit.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"
#include "vr_zancle.hpp"

namespace qvr::playergore
{
namespace
{

// Your capsules as drawn this frame (selfshot::build: the wounds' own), built once a frame.
struct Pose
{
    int frame{-1};
    selfshot::Capsules caps;
};
Pose pose;

// Your last four wounds' spots (the drips').
constexpr int maxWounds = 4;
Spot wounds[maxWounds];
int woundCount = 0, woundNext = 0;
float pending = 0.f; // drops owed

struct Counters
{
    int wounds{0};      // bound to a part
    int streams{0};     // opened
    int splashDrops{0}; // thrown
    int drips{0};       // fallen
    int eyeSkips{0};    // splashes and drips not made, too close to the eyes
};
Counters counters;

za::FastNonCryptoRng rng{static_cast<za::U64>(za::Clock::nowNanoseconds()) ^ 0x9e3779b97f4a7c15ull};
double lastTime = -1.0;
const qmodel_t* lastWorld = nullptr;
float testChance = 0.f; // the test's stream chance (0: the tunable's)

[[nodiscard]] float mToU()
{
    return units::metresToUnits();
}

[[nodiscard]] const selfshot::Capsules& capsules()
{
    if(pose.frame != host_framecount)
    {
        pose.frame = host_framecount;
        entity_t* own[3]{};
        view::woundTargets(own);
        pose.caps = selfshot::build(own);
    }
    return pose.caps;
}

[[nodiscard]] const selfshot::Capsule* capsuleOf(int part)
{
    for(const selfshot::Capsule& c : capsules())
    {
        if(c.part == part)
        {
            return &c;
        }
    }
    return nullptr;
}

// A capsule's frame: along its axis, and two across it (one level with your view's yaw).
void frameOf(const selfshot::Capsule& c, glm::vec3& axis, glm::vec3& side, glm::vec3& across)
{
    axis = geom::safeNormalize(c.b - c.a, glm::vec3{0.f, 0.f, 1.f}, 1e-4f);
    const float yaw = glm::radians(r_refdef.viewangles[YAW]);
    const glm::vec3 look{za::cos(yaw), za::sin(yaw), 0.f};
    side = glm::cross(axis, look);
    if(glm::length(side) < 0.2f)
    {
        side = glm::cross(axis, glm::vec3{-look.y, look.x, 0.f});
    }
    side = geom::safeNormalize(side, glm::vec3{1.f, 0.f, 0.f}, 1e-4f);
    across = glm::cross(side, axis);
}

[[nodiscard]] int stochastic(float n)
{
    const int whole = static_cast<int>(n);
    return whole + (vrand::range(rng, 0.f, 1.f) < n - static_cast<float>(whole) ? 1 : 0);
}

// A splash of drops out of the wound at `at` (vr_player_gore_splash); none close to the eyes.
void splash(const glm::vec3& at, const glm::vec3& n, const glm::vec3& way, int amount)
{
    if(nearEyes(at))
    {
        counters.eyeSkips++;
        return;
    }
    const float k = za::clamp(vr_player_gore_splash_amount.value, 0.f, 10.f) * za::clamp(static_cast<float>(amount) / 10.f, 0.5f, 2.f);
    const int count = za::min(32, stochastic(6.f * k));
    if(count <= 0)
    {
        return;
    }
    glm::vec3 floor, fn;
    float f = 1.f;
    const float floorZ = decals::trace(at, at - glm::vec3{0.f, 0.f, 256.f}, floor, fn, f) ? floor.z + 0.2f : at.z - 256.f;
    const glm::vec3 color = gore::bloodColorAt(at);
    const glm::vec3 back = glm::length(way) > 0.01f ? -glm::normalize(way) : n;
    for(int i = 0; i < count; i++)
    {
        const glm::vec3 jitter{vrand::range(rng, -0.5f, 0.5f), vrand::range(rng, -0.5f, 0.5f), vrand::range(rng, -0.2f, 0.5f)};
        const glm::vec3 d = geom::safeNormalize(n * 0.8f + back * 0.4f + jitter, n, 1e-4f);
        particles::bloodDroplet(at + n * 0.5f, d * vrand::range(rng, 50.f, 140.f), vrand::range(rng, 0.35f, 0.7f),
            vrand::range(rng, 0.5f, 1.1f), color, floorZ);
    }
    counters.splashDrops += count;
}

// Your wounds' drips (vr_player_gore_drips), as a wounded enemy's: the rate by the health lost.
void drip(float dt)
{
    const int health = cl.stats[STAT_HEALTH];
    if(health <= 0 || health >= 100 || dt <= 0.f)
    {
        if(health <= 0 || health >= 100)
        {
            woundCount = woundNext = 0; // (healed, or dead: a new life starts clean)
        }
        pending = 0.f;
        return;
    }
    if(woundCount == 0 || !on(Effect::Drips))
    {
        pending = 0.f;
        return;
    }
    const float lost = za::clamp(1.f - static_cast<float>(health) / 100.f, 0.f, 1.f);
    const float least = za::clamp(vr_drips_blood_min.value, 0.f, 1.f);
    const float rate = za::clamp(vr_drips_blood_rate.value, 0.f, 20.f) * za::clamp(vr_player_gore_drip_rate.value, 0.f, 10.f) *
                       (least + (1.f - least) * lost);
    pending = za::min(pending + rate * dt * vrand::range(rng, 0.5f, 1.5f), 3.f);
    while(pending >= 1.f)
    {
        pending -= 1.f;
        const int i = za::min(woundCount - 1, static_cast<int>(vrand::range(rng, 0.f, static_cast<float>(woundCount))));
        glm::vec3 at, n;
        if(!where(wounds[i], at, n))
        {
            continue;
        }
        if(nearEyes(at))
        {
            counters.eyeSkips++;
            continue;
        }
        at += glm::vec3{vrand::range(rng, -0.6f, 0.6f), vrand::range(rng, -0.6f, 0.6f), -1.f};
        const float size = vrand::range(rng, 0.5f, 0.75f) * za::clamp(vr_drips_blood_size.value, 0.1f, 5.f);
        if(gore::dropFrom(0, at, size, glm::vec4{gore::bloodColorAt(at), 1.f}, za::clamp(vr_drips_blood_marks.value, 0.f, 1.f)))
        {
            counters.drips++;
        }
    }
}

// ---- The test (vr_player_gore_test) -------------------------------------------------------------------------

struct Test
{
    int phase{0}; // 0 off
    double t0{0.0}, deadline{0.0};
    testkit::Tally tally;
    Counters at0;
    int streamsAt{0};
    bool shot{false};
};
Test test;
constexpr const char* testName = "player_gore_test";

void check(bool ok, const char* what)
{
    if(ok)
    {
        Con_Printf("%s: PASS %s\n", testName, what);
    }
    test.tally.check(ok, testName, what);
}

void testEnd()
{
    testChance = 0.f;
    test.tally.verdict(testName);
    test.phase = 0;
}

void testStep()
{
    char line[192];
    const double now = cl.time;
    if(test.deadline > 1e8 && cl.worldmodel && cls.signon == SIGNONS)
    {
        test.deadline = now + 90.0;
    }
    if(now > test.deadline)
    {
        check(false, "timed out");
        testEnd();
        return;
    }
    switch(test.phase)
    {
        case 1: // in a map, alive: shoot yourself (vr_selfshot_test 1: a pellet at the palm, the forearm, the thigh; nails)
        {
            if(cls.signon != SIGNONS || !cl.worldmodel || cl.viewentity <= 0 || cl.stats[STAT_HEALTH] <= 0)
            {
                return;
            }
            test.at0 = counters;
            testChance = 1.f;
            if(!flatplay::active())
            {
                Cbuf_AddText("vr_selfshot_test 1\n");
            }
            else
            {
                // Flat-screen play shoots nothing into you: a nail's wound on each hand, forearm and the body drawn, from
                // ahead (as the wounds would paint them: their capsules' front).
                int n = 0;
                for(const selfshot::Capsule& c : capsules())
                {
                    glm::vec3 axis, side, across;
                    frameOf(c, axis, side, across);
                    const float yaw = glm::radians(r_refdef.viewangles[YAW]);
                    const glm::vec3 look{za::cos(yaw), za::sin(yaw), 0.f};
                    const glm::vec3 face = geom::safeNormalize(-look - axis * glm::dot(-look, axis), across, 1e-3f);
                    onWound(0.5f * (c.a + c.b) + face * c.r, face, -face, c.part, 2, 9);
                    n++;
                    const glm::vec3 mid = 0.5f * (c.a + c.b);
                    Con_Printf("%s: capsule part %d: middle %.0f units under the eyes, %.0f out, radius %.1f\n", testName, c.part,
                        static_cast<double>(r_refdef.vieworg[2] - mid.z),
                        static_cast<double>(glm::length(glm::vec2{mid.x - r_refdef.vieworg[0], mid.y - r_refdef.vieworg[1]})),
                        static_cast<double>(c.r));
                }
                Con_Printf("%s: flat-screen play: %d wounds made on your capsules\n", testName, n);
            }
            test.phase = 2;
            return;
        }
        case 2: // three wounds of yours bound to their parts
        {
            if(counters.wounds - test.at0.wounds < 3)
            {
                return;
            }
            test.streamsAt = bloodstream::ownStreams();
            test.t0 = now;
            test.phase = 3;
            return;
        }
        case 3: // a picture after 1 s (the streams pouring); the self-shot test over (it heals you before each shot)
        {
            if(!test.shot && now - test.t0 >= 1.0)
            {
                test.shot = true;
                Cbuf_AddText("screenshot\n");
            }
            if(!test.shot || vr_selfshot_test.value != 0.f)
            {
                return;
            }
            // Hurt to 40 (the server's health): your wounds drip, as a wounded enemy's.
            if(sv.active)
            {
                const progs::ServerVm vm;
                if(edict_t* me = progs::playerEdict())
                {
                    me->v.health = 40.f;
                }
            }
            test.phase = 5;
            return;
        }
        case 5: // the client hurt too (its stats a frame later)
        {
            if(cl.stats[STAT_HEALTH] >= 100)
            {
                return;
            }
            // (its heals before each shot washed those wounds off: one more on the forearm, as a nail's)
            for(const int part : {selfshot::PartLegs, selfshot::PartTorso, selfshot::PartMainForearm})
            {
                if(const selfshot::Capsule* c = capsuleOf(part))
                {
                    glm::vec3 axis, side, across;
                    frameOf(*c, axis, side, across);
                    onWound(0.5f * (c->a + c->b) - across * c->r, -across, across, part, 2, 9);
                    break;
                }
            }
            test.at0.drips = counters.drips;
            test.t0 = now;
            test.phase = 4;
            return;
        }
        case 4: // 3 s of drips; then the rest
        {
            if(now - test.t0 < 3.0)
            {
                return;
            }
            q_snprintf(line, sizeof(line), "your wounds bound to your capsules as drawn: %d", counters.wounds - test.at0.wounds);
            check(counters.wounds - test.at0.wounds >= 3, line);
            q_snprintf(line, sizeof(line), "blood streams from them (chance 1): %d opened, %d pouring after the third",
                counters.streams - test.at0.streams, test.streamsAt);
            check(counters.streams - test.at0.streams >= 2 && test.streamsAt >= 1, line);
            q_snprintf(line, sizeof(line), "splashes: %d drops", counters.splashDrops - test.at0.splashDrops);
            check(counters.splashDrops - test.at0.splashDrops >= 6, line);
            q_snprintf(line, sizeof(line), "drips while hurt (health %d): %d drops in 3 s (%d spots, %d skipped by the eyes)",
                cl.stats[STAT_HEALTH], counters.drips - test.at0.drips, woundCount, counters.eyeSkips);
            check(cl.stats[STAT_HEALTH] < 100 && counters.drips - test.at0.drips >= 2, line);
            // The eyes kept clear: a splash at the eyes makes nothing; a hand low in front does.
            const glm::vec3 eyes{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
            const float yaw = glm::radians(r_refdef.viewangles[YAW]);
            const glm::vec3 look{za::cos(yaw), za::sin(yaw), 0.f};
            const int drops0 = counters.splashDrops;
            splash(eyes + look * (0.15f * mToU()), -look, look, 20);
            splash(eyes + look * (0.3f * mToU()) + glm::vec3{0.f, 0.f, 0.2f * mToU()}, -look, look, 20);
            q_snprintf(line, sizeof(line), "none at the eyes or over them (%d drops)", counters.splashDrops - drops0);
            check(counters.splashDrops == drops0 && nearEyes(eyes) && !nearEyes(eyes + look * (0.3f * mToU()) - glm::vec3{0.f, 0.f, 0.6f * mToU()}), line);
            // Off: nothing more.
            char saved[32];
            q_strlcpy(saved, vr_player_gore.string, sizeof(saved));
            Cvar_Set("vr_player_gore", "0");
            const Counters c0 = counters;
            if(woundCount > 0)
            {
                glm::vec3 at, n;
                if(where(wounds[0], at, n))
                {
                    onWound(at, n, -n, wounds[0].part, 1, 20);
                }
            }
            check(counters.wounds == c0.wounds && counters.streams == c0.streams && counters.splashDrops == c0.splashDrops,
                "with vr_player_gore 0: no stream, no splash, nothing bound");
            Cvar_Set("vr_player_gore", saved);
            testEnd();
            return;
        }
        default: return;
    }
}

void test_f()
{
    test = Test{};
    test.phase = 1;
    test.deadline = 1e9; // (set once the map is in)
    Con_Printf("%s: started\n", testName);
}

void info_f()
{
    Con_Printf("player gore (vr_player_gore %d: streams %d, drips %d, splash %d): %d wounds bound, %d streams opened (%d "
               "pouring), %d splash drops, %d drips, %d near the eyes skipped; %d spots kept\n",
        static_cast<int>(vr_player_gore.value), on(Effect::Streams), on(Effect::Drips), on(Effect::Splash), counters.wounds,
        counters.streams, bloodstream::ownStreams(), counters.splashDrops, counters.drips, counters.eyeSkips, woundCount);
    for(int i = 0; i < woundCount; i++)
    {
        glm::vec3 at, n;
        const bool drawn = where(wounds[i], at, n);
        Con_Printf("  spot %d: part %d, along %.2f, offset %.2f %.2f%s\n", i, wounds[i].part, static_cast<double>(wounds[i].along),
            static_cast<double>(wounds[i].offset.x), static_cast<double>(wounds[i].offset.y), drawn ? "" : " (not drawn)");
    }
}

} // namespace

bool on(Effect effect)
{
    if(vr_player_gore.value == 0.f || gore::level() <= 0)
    {
        return false;
    }
    switch(effect)
    {
        case Effect::Streams: return vr_player_gore_streams.value != 0.f && vr_gore_streams.value != 0.f;
        case Effect::Drips: return vr_player_gore_drips.value != 0.f;
        case Effect::Splash: return vr_player_gore_splash.value != 0.f;
    }
    return false;
}

bool bind(const glm::vec3& at, int part, Spot& out)
{
    const selfshot::Capsule* c = capsuleOf(part);
    if(!c || c->r <= 0.f)
    {
        return false;
    }
    glm::vec3 axis, side, across;
    frameOf(*c, axis, side, across);
    const float len = glm::length(c->b - c->a);
    const float along = len > 1e-3f ? za::clamp(glm::dot(at - c->a, axis) / len, 0.f, 1.f) : 0.f;
    const glm::vec3 d = at - (c->a + (c->b - c->a) * along);
    glm::vec2 o{glm::dot(d, side), glm::dot(d, across)};
    const float l = glm::length(o);
    o = l > 1e-3f ? o / za::max(l, c->r) : glm::vec2{0.f, 1.f}; // (on the surface at most: the capsules are round, the body flatter)
    out.part = part;
    out.along = along;
    out.offset = glm::vec3{o.x, o.y, 0.f};
    return true;
}

bool where(const Spot& spot, glm::vec3& at, glm::vec3& normal)
{
    const selfshot::Capsule* c = capsuleOf(spot.part);
    if(!c)
    {
        return false;
    }
    glm::vec3 axis, side, across;
    frameOf(*c, axis, side, across);
    const glm::vec3 d = side * spot.offset.x + across * spot.offset.y;
    at = c->a + (c->b - c->a) * spot.along + d * c->r;
    normal = geom::safeNormalize(d, across, 1e-4f);
    return true;
}

bool nearEyes(const glm::vec3& p)
{
    const glm::vec3 eyes{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const float clear = za::clamp(vr_player_gore_eye_clear.value, 0.f, 3.f) * mToU();
    const float flat = glm::length(glm::vec2{p.x - eyes.x, p.y - eyes.y});
    // Close in front (a hand at the face), or over the eyes within arm's reach (a hand raised: its blood would rain past them).
    return (flat < clear && p.z > eyes.z - clear) || (flat < 0.6f * mToU() && p.z > eyes.z);
}

void onWound(const glm::vec3& at, const glm::vec3& n, const glm::vec3& way, int part, int kind, int amount)
{
    const bool streams = on(Effect::Streams), drips = on(Effect::Drips), splashes = on(Effect::Splash);
    if(!streams && !drips && !splashes)
    {
        return;
    }
    Spot spot;
    if(!bind(at, part, spot))
    {
        return;
    }
    counters.wounds++;
    // Blood: a shot, a nail, a blow, a blast (not a burn, a zap, a liquid: QVR_WOUND_*).
    const bool bleeds = kind >= 1 && kind <= 4;
    if(drips && bleeds)
    {
        wounds[woundNext] = spot;
        woundNext = (woundNext + 1) % maxWounds;
        woundCount = za::min(woundCount + 1, maxWounds);
    }
    if(splashes && bleeds)
    {
        splash(at, n, way, amount);
    }
    const float chance = testChance > 0.f ? testChance : za::clamp(vr_player_gore_stream_chance.value, 0.f, 1.f);
    if(streams && kind >= 1 && kind <= 3 && vrand::range(rng, 0.f, 1.f) < chance && bloodstream::startOwn(spot, n, way, amount))
    {
        counters.streams++;
    }
}

void frame()
{
    if(cl.worldmodel != lastWorld)
    {
        clear();
        lastWorld = cl.worldmodel;
    }
    if(test.phase > 0)
    {
        testStep();
    }
    if(!cl.worldmodel || cls.signon != SIGNONS || cl.viewentity <= 0)
    {
        return;
    }
    double dtd = lastTime < 0.0 ? 0.0 : cl.time - lastTime;
    lastTime = cl.time;
    if(dtd < 0.0 || dtd > 0.25)
    {
        dtd = 0.0;
    }
    drip(static_cast<float>(dtd));
}

void clear()
{
    woundCount = woundNext = 0;
    pending = 0.f;
    lastTime = -1.0;
    pose.frame = -1;
}

void registerCommands()
{
    Cmd_AddCommand("vr_player_gore_info", info_f);
    Cmd_AddCommand("vr_player_gore_test", test_f);
}

} // namespace qvr::playergore
