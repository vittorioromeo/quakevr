// vr_bodyblood.cpp -- see vr_bodyblood.hpp.

#include "vr_bodyblood.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_decals.hpp"
#include "vr_lines.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace qvr::bodyblood
{
namespace
{

// Sizes in metres of the player's body (times units::bodyScale()), as make_vrbody.py's.
constexpr float forearmLength = 0.25f;
constexpr float handLength = 0.085f; // the wrist to the knuckles
constexpr float elbowRadius = 0.04f;
constexpr float wristRadius = 0.027f;
constexpr float handRadius = 0.022f;

constexpr std::size_t maxDrops = 64;

// Drops per second from each arm, by the damage skin (0..3); a hit adds more for a while.
constexpr float dripRate[4] = {0.f, 0.35f, 0.9f, 1.8f};

// A hand's limb, as a chain from the elbow through the wrist to the knuckles: the forearm (while
// the body is posed) and the hand (while it is drawn).
struct Limb
{
    bool forearm{false};
    bool hand{false};
    glm::vec3 elbow{0.f};
    glm::vec3 armWrist{0.f}; // the body's wrist, where its forearm ends
    glm::vec3 wrist{0.f};    // the drawn hand's
    glm::vec3 knuckles{0.f};
    float m2w{1.f};
};

enum Segment : std::uint8_t
{
    Forearm,
    Hand
};

enum class Phase : std::uint8_t
{
    Hanging, // gathering under the limb, carried by it
    Falling,
    Speck // a splash, thrown up off the floor
};

struct Drop
{
    Phase phase{Phase::Hanging};
    std::uint8_t hand{0};
    Segment segment{Forearm};
    float along{0.f}; // 0..1 along the segment (towards the knuckles)
    glm::vec3 pos{0.f};
    glm::vec3 vel{0.f};
    float size{1.f}; // across, in world units
    float age{0.f};
    float life{0.f};   // how long it hangs, or a speck flies
    float floorZ{0.f}; // specks: gone below it
    bool gone{false};
};

std::vector<Drop> drops;
std::vector<Drop> splashes; // thrown up this frame, added after the others move
Limb limbs[2];
glm::vec3 lastWrist[2]{glm::vec3{0.f}, glm::vec3{0.f}};
glm::vec3 handVel[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // world units per second
bool haveLastWrist[2]{false, false};
float pending[2]{0.f, 0.f}; // drops owed to each arm
float burst = 0.f;          // a recent hit: more drops, fading
int lastHealth = 0;
double lastTime = -1.0;
int lastFrame = -1;
const qmodel_t* lastWorld = nullptr;

std::mt19937 rng{std::random_device{}()};

[[nodiscard]] float rnd(float lo, float hi)
{
    return std::uniform_real_distribution<float>{lo, hi}(rng);
}

[[nodiscard]] glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback)
{
    const float len = glm::length(v);
    return len > 1e-4f ? v / len : fallback;
}

// A point on the limb's axis, the direction along it, and the limb's radius there.
bool axisPoint(const Limb& l, Segment segment, float along, glm::vec3& centre, glm::vec3& axis, float& radius)
{
    if(segment == Forearm ? !l.forearm : !l.hand)
    {
        return false;
    }
    const glm::vec3 a = segment == Forearm ? l.elbow : l.wrist;
    const glm::vec3 b = segment == Forearm ? l.armWrist : l.knuckles;
    centre = glm::mix(a, b, along);
    axis = safeNormalize(b - a, glm::vec3{0.f, 0.f, -1.f});
    radius = (segment == Forearm ? glm::mix(elbowRadius, wristRadius, along) : handRadius) * l.m2w;
    return true;
}

// The underside of the limb there: where blood gathers into a drop. On a level limb, below the
// axis; on an upright one, below its end.
bool underside(const Limb& l, Segment segment, float along, glm::vec3& out)
{
    glm::vec3 centre, axis;
    float radius;
    if(!axisPoint(l, segment, along, centre, axis, radius))
    {
        return false;
    }
    const glm::vec3 down{0.f, 0.f, -1.f};
    const glm::vec3 across = down - axis * glm::dot(down, axis); // length: how level the limb is
    out = centre + (across + down * (1.f - glm::length(across))) * radius;
    return true;
}

// Where a new drop gathers on `hand`'s limb: anywhere on it, or, the steeper it slopes, more
// likely at its lower end, where the blood runs to.
bool pickSpot(const Limb& l, Segment& segment, float& along)
{
    if(!l.forearm && !l.hand)
    {
        return false;
    }
    segment = !l.hand ? Forearm : !l.forearm ? Hand : rnd(0.f, 1.f) < 0.65f ? Forearm : Hand;
    along = rnd(0.1f, 0.95f);

    const glm::vec3 top = l.forearm ? l.elbow : l.wrist;
    const glm::vec3 bottom = l.hand ? l.knuckles : l.armWrist;
    const float slope = safeNormalize(bottom - top, glm::vec3{0.f}).z; // < 0: the fingers are lower
    const float steep = std::clamp((std::fabs(slope) - 0.35f) / 0.5f, 0.f, 1.f);
    if(rnd(0.f, 1.f) < steep)
    {
        if(slope < 0.f)
        {
            segment = l.hand ? Hand : Forearm;
            along = 1.f;
        }
        else
        {
            segment = l.forearm ? Forearm : Hand;
            along = 0.f;
        }
    }
    return true;
}

// Close under the eyes (a hand raised to the face): no drops there, to fall across the view.
[[nodiscard]] bool nearEyes(const hands::State& s, const glm::vec3& p, float m2w)
{
    const glm::vec2 flat{p.x - s.head.x, p.y - s.head.y};
    return p.z > s.head.z - 0.3f * m2w && glm::length(flat) < 0.5f * m2w;
}

void addDrop(const hands::State& s, int hand, bool flung)
{
    const Limb& l = limbs[hand];
    Drop d;
    if(drops.size() >= maxDrops || !pickSpot(l, d.segment, d.along) || !underside(l, d.segment, d.along, d.pos) ||
        nearEyes(s, d.pos, l.m2w))
    {
        return;
    }
    d.hand = static_cast<std::uint8_t>(hand);
    d.size = rnd(0.006f, 0.01f) * l.m2w * std::clamp(vr_body_blood_amount.value, 0.25f, 4.f);
    if(flung)
    {
        // Thrown off the swung arm at once, a little behind its speed.
        d.phase = Phase::Falling;
        d.vel = handVel[hand] * rnd(0.6f, 1.f);
    }
    else
    {
        // It hangs a moment, swelling; less just after a hit, when the blood runs faster.
        d.life = rnd(0.35f, 0.8f) / (1.f + burst);
    }
    drops.push_back(d);
}

// A drop reaches a surface: specks thrown up off a floor, and a mark on it (vr_body_blood_marks: the
// chance, vr_body_blood_mark_size; at most one every 0.08 s: a steady drip would otherwise fill the
// decal budget).
double lastMark = -1.0;

void land(const Drop& d, const glm::vec3& where, const glm::vec3& normal, float m2w)
{
    if(normal.z < 0.6f)
    {
        return; // a wall: it runs down out of sight
    }
    const float amount = std::clamp(vr_body_blood_amount.value, 0.25f, 4.f);
    if(rnd(0.f, 1.f) < std::clamp(vr_body_blood_marks.value, 0.f, 1.f) && (cl.time < lastMark || cl.time - lastMark > 0.08))
    {
        lastMark = cl.time;
        decals::drop(where, rnd(3.5f, 6.5f) * std::clamp(vr_body_blood_mark_size.value, 0.25f, 4.f) * std::sqrt(amount));
    }
    const int specks = static_cast<int>((2.f + rnd(0.f, 2.99f)) * std::sqrt(amount));
    for(int i = 0; i < specks && drops.size() + splashes.size() < maxDrops; i++)
    {
        Drop sp;
        sp.phase = Phase::Speck;
        sp.pos = where + normal * 0.1f;
        const float a = rnd(0.f, 6.2831853f);
        const float out = rnd(0.2f, 0.6f) * m2w;
        sp.vel = {std::cos(a) * out, std::sin(a) * out, rnd(0.4f, 0.9f) * m2w};
        sp.size = d.size * rnd(0.3f, 0.5f);
        sp.life = rnd(0.15f, 0.3f);
        sp.floorZ = where.z;
        splashes.push_back(sp);
    }
}

void simulate(float dt, float m2w)
{
    const float gravity = 9.81f * units::metresToUnits(); // real gravity, not Quake's
    const float terminal = 7.f * units::metresToUnits();
    const float fling = 2.2f * units::metresToUnits();

    for(Drop& d : drops)
    {
        d.age += dt;
        switch(d.phase)
        {
            case Phase::Hanging:
            {
                const bool swung = glm::length(handVel[d.hand]) > fling && d.age > 0.05f;
                glm::vec3 at;
                if(!underside(limbs[d.hand], d.segment, d.along, at))
                {
                    d.phase = Phase::Falling; // the limb is gone (hidden): let it go where it was
                    d.age = 0.f;
                    break;
                }
                const float grow = std::min(1.f, d.age / std::max(0.05f, d.life));
                d.pos = at + glm::vec3{0.f, 0.f, -0.5f * d.size * grow};
                if(d.age >= d.life || swung)
                {
                    d.phase = Phase::Falling;
                    d.vel = handVel[d.hand] * (swung ? rnd(0.6f, 1.f) : 0.8f);
                    d.age = 0.f;
                }
                break;
            }
            case Phase::Falling:
            {
                d.vel.z = std::max(d.vel.z - gravity * dt, -terminal);
                const glm::vec3 next = d.pos + d.vel * dt;
                const trace_t tr = worldtrace::world(d.pos, next);
                if(tr.startsolid || tr.allsolid)
                {
                    d.gone = true; // it started in a wall
                }
                else if(tr.fraction < 1.f)
                {
                    land(d, worldtrace::endPos(tr), worldtrace::normal(tr), m2w);
                    d.gone = true;
                }
                else
                {
                    d.pos = next;
                    d.gone = d.age > 3.f;
                }
                break;
            }
            case Phase::Speck:
                d.vel.z -= gravity * dt;
                d.pos += d.vel * dt;
                d.gone = d.age > d.life || d.pos.z < d.floorZ;
                break;
        }
    }
    std::erase_if(drops, [](const Drop& d) { return d.gone; });
    drops.insert(drops.end(), splashes.begin(), splashes.end());
    splashes.clear();
}

// How lit the player's surroundings are (the lightmap under the hands), for the drops' colour:
// the lines they are drawn with take no light of their own.
[[nodiscard]] float shade(const hands::State& s)
{
    static lightcache_t cache{};
    if(!cl.worldmodel)
    {
        return 1.f;
    }
    const glm::vec3 p = (s.pos[0] + s.pos[1]) * 0.5f;
    vec3_t v = {p.x, p.y, p.z};
    const float light = static_cast<float>(R_LightPoint(v, 0.f, &cache)); // 128: Quake's full light
    return std::clamp(std::max(light, 24.f) / 110.f, 0.2f, 1.4f);
}

void draw(const hands::State& s)
{
    if(drops.empty())
    {
        return;
    }
    const float k = shade(s);
    const glm::vec3 blood = glm::min(glm::vec3{0.42f, 0.02f, 0.015f} * k, glm::vec3{1.f});
    const glm::vec4 solid{blood, 0.95f};
    const glm::vec4 faded{blood, 0.f};

    for(const Drop& d : drops)
    {
        switch(d.phase)
        {
            case Phase::Hanging:
            {
                // A drop swelling under the skin, with a thin neck while it is big.
                const float grow = std::min(1.f, d.age / std::max(0.05f, d.life));
                glm::vec3 at;
                if(grow > 0.4f && underside(limbs[d.hand], d.segment, d.along, at))
                {
                    lines::line(at, d.pos, d.size * 0.4f * grow, solid, solid);
                }
                lines::point(d.pos, d.size * (0.35f + 0.65f * grow), solid);
                break;
            }
            case Phase::Falling:
            {
                // Streaked along its fall, the faster the longer.
                glm::vec3 trail = -d.vel * 0.03f;
                const float len = glm::length(trail);
                const float lo = d.size * 0.8f;
                const float hi = 0.1f * units::metresToUnits();
                trail = len > 1e-4f ? trail * (std::clamp(len, lo, hi) / len) : glm::vec3{0.f, 0.f, lo};
                lines::line(d.pos, d.pos + trail, d.size * 0.8f, solid, faded);
                lines::point(d.pos, d.size, solid);
                break;
            }
            case Phase::Speck: lines::point(d.pos, d.size, {blood, 0.85f * (1.f - d.age / d.life)}); break;
        }
    }
}

} // namespace

void update(const hands::State& s, const avatar::HandPose* const drawnHands[2], int damage)
{
    if(host_framecount == lastFrame)
    {
        return; // once per frame, however often the view is set up
    }
    lastFrame = host_framecount;

    if(!vr_body_blood.value || cl.worldmodel != lastWorld)
    {
        clear();
        lastWorld = cl.worldmodel;
        if(!vr_body_blood.value)
        {
            return;
        }
    }

    // Time: the game's (the drops stop with it); a long gap (a load, the view gone) starts over.
    double dtd = lastTime < 0.0 ? 0.0 : cl.time - lastTime;
    lastTime = cl.time;
    if(dtd < 0.0 || dtd > 0.25)
    {
        clear();
        lastTime = cl.time;
        dtd = 0.0;
    }
    const float dt = static_cast<float>(dtd);

    const float m2w = units::metresToUnits() * units::bodyScale();
    const int build = static_cast<int>(vr_body_build.value);
    const float girth = build <= 0 ? 0.88f : build >= 2 ? 1.2f : 1.f; // lean, athletic, brawny

    // The limbs, where they are this frame.
    for(int hand = 0; hand < 2; hand++)
    {
        Limb& l = limbs[hand];
        l = Limb{};
        l.m2w = m2w * girth;

        glm::vec3 wrist, direction;
        if(avatar::forearm(hand, wrist, direction))
        {
            l.forearm = true;
            l.armWrist = wrist;
            l.elbow = wrist - safeNormalize(direction, glm::vec3{0.f, 0.f, 1.f}) *
                                  (forearmLength * m2w * std::max(0.5f, vr_body_arm_length.value));
        }
        if(const avatar::HandPose* hp = drawnHands[hand])
        {
            l.hand = true;
            l.wrist = hp->wrist;
            l.knuckles = hp->wrist + hp->forward * (handLength * m2w);
        }

        // The arm's speed (the world's: walking counts), to fling drops off a swing.
        const bool any = l.forearm || l.hand;
        const glm::vec3 w = l.hand ? l.wrist : l.armWrist;
        if(any && haveLastWrist[hand] && dt > 0.f)
        {
            const glm::vec3 moved = w - lastWrist[hand];
            const glm::vec3 v = glm::length(moved) > 0.6f * m2w ? glm::vec3{0.f} : moved / dt; // a teleport
            handVel[hand] = glm::mix(handVel[hand], v, 0.5f);
        }
        else if(!any)
        {
            handVel[hand] = glm::vec3{0.f};
        }
        haveLastWrist[hand] = any;
        lastWrist[hand] = w;
    }

    // A hit: more drops at once, fading over a couple of seconds.
    const int health = cl.stats[STAT_HEALTH];
    if(lastHealth > 0 && health > 0 && health < lastHealth)
    {
        burst = std::min(2.5f, burst + static_cast<float>(lastHealth - health) / 15.f);
    }
    lastHealth = health;
    burst *= std::exp(-dt / 1.2f);

    // New drops, from the wounds the skins show.
    damage = std::clamp(damage, 0, 3);
    const float mult = std::clamp(vr_body_blood.value, 0.f, 4.f);
    for(int hand = 0; hand < 2; hand++)
    {
        if(damage == 0 || dt <= 0.f)
        {
            pending[hand] = 0.f;
            continue;
        }
        const float rate = (dripRate[damage] + burst * 2.f) * mult;
        pending[hand] += rate * dt * rnd(0.4f, 1.6f);
        while(pending[hand] >= 1.f)
        {
            pending[hand] -= 1.f;
            addDrop(s, hand, false);
        }

        // Swinging a hurt arm hard flings drops off it.
        const float speed = glm::length(handVel[hand]) / units::metresToUnits(); // m/s
        if(speed > 2.2f && rnd(0.f, 1.f) < (speed - 2.2f) * 1.5f * mult * damage / 3.f * dt)
        {
            addDrop(s, hand, true);
        }
    }

    if(dt > 0.f)
    {
        simulate(dt, m2w);
    }
    draw(s);
}

void clear()
{
    drops.clear();
    pending[0] = pending[1] = 0.f;
    haveLastWrist[0] = haveLastWrist[1] = false;
    handVel[0] = handVel[1] = glm::vec3{0.f};
    burst = 0.f;
    lastTime = -1.0;
}

} // namespace qvr::bodyblood
