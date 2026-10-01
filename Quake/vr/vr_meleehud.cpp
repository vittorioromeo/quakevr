// vr_meleehud.cpp -- see vr_meleehud.hpp.

#include "vr_meleehud.hpp"
#include "vr_cvars.hpp"
#include "vr_particles.hpp"
#include "vr_protocol.hpp"
#include "vr_twohand.hpp"
#include "vr_view.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <random>

namespace qvr::meleehud
{
namespace
{

constexpr float emberRate = 42.f;   // embers a second off each hand's weapon as the window opens (fewer as it closes)
constexpr float glowFadeOut = 0.2f; // s the glow takes to go when the window closes (a counter landed: at once)

float glow = 0.f;          // the glow now, 0..1 (following the window, fading out after it)
double openedAt = -1.0;    // realtime the window last opened (its flare)
float lastCounter = 0.f;   // the window's share left last frame
float emberDue[2] = {0.f}; // each hand's embers owed (a fraction of one carried over)
double lastQueue = -1.0;

std::minstd_rand rng{0x5C0FFEEu}; // ZANCLE-TODO: no random engines or distributions (these sequences kept)

[[nodiscard]] float rnd(float lo, float hi)
{
    return std::uniform_real_distribution<float>{lo, hi}(rng);
}

// A hand with nothing in it to glow for: empty, not steadying the other's weapon, carrying or climbing.
[[nodiscard]] bool bareFist(int hand)
{
    using namespace protocol;
    if(view::heldWeapon(hand) || twohand::helping(hand))
    {
        return false;
    }
    if(cl.stats[hand ? STAT_QVR_CARRYMAIN : STAT_QVR_CARRYOFF] != 0 || (cl.stats[STAT_QVR_CLIMB] & (1 << hand)))
    {
        return false;
    }
    return true;
}

// One ember off what `hand` holds: a random vertex of the weapon as drawn, leaving its surface (away from its line,
// from the grip to the muzzle or tip) and rising a little; off a bare fist, its knuckles.
void ember(const hands::State& s, int hand, float bright)
{
    if(const view::ViewEntity* ve = view::heldWeapon(hand))
    {
        const aliashdr_t* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(ve->ent.model));
        if(!hdr || hdr->numverts <= 0)
        {
            return;
        }
        const int vertex = std::uniform_int_distribution<int>{0, hdr->numverts - 1}(rng);
        const glm::vec3 p = view::anchorPosition(*ve, vertex, glm::vec3{0.f});
        glm::vec3 out{0.f};
        view::WeaponMount mount;
        if(view::weaponMount(hand, mount))
        {
            const glm::vec3 along = mount.muzzle - mount.pos;
            const float len2 = glm::dot(along, along);
            const glm::vec3 onLine = len2 > 1e-4f ? mount.pos + along * (glm::dot(p - mount.pos, along) / len2) : mount.pos;
            out = p - onLine;
            const float l = glm::length(out);
            out = l > 1e-3f ? out / l : glm::vec3{0.f};
        }
        particles::counterEmber(p + out * 0.25f, out * rnd(1.5f, 5.f) + glm::vec3{rnd(-1.f, 1.f), rnd(-1.f, 1.f), rnd(2.f, 7.f)},
            bright);
        return;
    }
    view::EmptyHand h;
    if(!view::emptyHandPose(s, hand, h))
    {
        return;
    }
    // The knuckles: about 9 cm from the wrist towards the fingers, 2 cm over the back of the hand, 8 cm across.
    const float cm = za::max(0.1f, vr_world_scale.value) / 3.81f; // units a centimetre (a unit is 1.5 inches)
    const glm::vec3 knuckles = h.wrist + h.forward * (9.f * cm) + h.back * (2.f * cm) + h.up * (rnd(-4.f, 4.f) * cm);
    particles::counterEmber(knuckles, h.back * rnd(1.f, 4.f) + glm::vec3{rnd(-1.f, 1.f), rnd(-1.f, 1.f), rnd(2.f, 7.f)},
        bright);
}

} // namespace

State state()
{
    State out;
    if(!(cl.protocolflags & PRFL_QUAKEVR) || cls.state != ca_connected)
    {
        return out;
    }
    const int bits = cl.stats[protocol::STAT_QVR_MELEE];
    out.stamina = (bits & 128) != 0;
    out.left = out.stamina ? static_cast<float>(za::clamp(bits & 127, 0, 100)) / 100.f : 1.f;
    out.low = (bits & 256) != 0;
    out.recovering = (bits & 512) != 0;
    out.counter = static_cast<float>(za::clamp((bits >> 10) & 63, 0, 63)) / 63.f;
    out.draining = out.stamina && (bits & 65536) != 0;
    return out;
}

void queue(const hands::State& s)
{
    const float dt = lastQueue < 0.0 ? 0.f : static_cast<float>(za::clamp(realtime - lastQueue, 0.0, 0.1));
    lastQueue = realtime;

    const float counter = state().counter;
    if(counter > lastCounter + 0.25f)
    {
        openedAt = realtime; // (re)opened: a parry
    }
    lastCounter = counter;

    // The glow follows the window down (a quarter of it left at the end), flares as it opens, and fades out after it.
    float target = 0.f;
    if(counter > 0.f)
    {
        const float flare = openedAt >= 0.0 ? za::max(0.f, 1.f - static_cast<float>(realtime - openedAt) / 0.25f) : 0.f;
        target = za::min(1.f, 0.25f + 0.5f * counter + 0.4f * flare);
    }
    glow = target >= glow ? target : za::max(target, glow - dt / glowFadeOut);

    if(!vr_counter_glow.value || counter <= 0.f)
    {
        emberDue[0] = emberDue[1] = 0.f;
        return;
    }
    for(int hand = 0; hand < 2; hand++)
    {
        if(!view::heldWeapon(hand) && !bareFist(hand))
        {
            emberDue[hand] = 0.f;
            continue;
        }
        emberDue[hand] += dt * emberRate * (0.3f + 0.7f * counter);
        for(; emberDue[hand] >= 1.f; emberDue[hand] -= 1.f)
        {
            ember(s, hand, 0.55f + 0.45f * counter);
        }
    }
}

float entityGlow(const entity_t* e)
{
    if(glow <= 0.f || !vr_counter_glow.value)
    {
        return 0.f;
    }
    bool weapon = false;
    const int hand = view::handOf(e, weapon);
    if(hand < 0 || (!weapon && !bareFist(hand)))
    {
        return 0.f;
    }
    // A weapon's whole length; a bare fist less (the whole hand would glow).
    const float breathe = 0.85f + 0.15f * static_cast<float>(za::sin(realtime * 11.0));
    return za::clamp(glow * breathe * (weapon ? 1.f : 0.6f), 0.f, 1.f);
}

} // namespace qvr::meleehud
