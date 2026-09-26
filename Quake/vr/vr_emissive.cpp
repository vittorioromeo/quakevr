// vr_emissive.cpp -- see vr_emissive.hpp.

#include "vr_emissive.hpp"
#include "vr_color.hpp"
#include "vr_cvars.hpp"
#include "vr_lighting.hpp"
#include "vr_particles.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace qvr;

namespace
{

// A glow's colour at DarkPlaces' brightness (1.5 an EF_DIMLIGHT's, 3 a rocket's) and its radius.
struct Glow
{
    glm::vec3 color;
    float radius;
};

// The rocket's (3, 1.5, 0.5) and the vore ball's (1.2, 0.5, 1) are DarkPlaces' own trail lights;
// DarkPlaces gives the others none (the lasers a white EF_DIMLIGHT): these match their trails.
constexpr Glow scragSpit{{0.8f, 1.8f, 0.3f}, 150.f};    // green, as its spit
constexpr Glow knightFlame{{1.6f, 0.8f, 0.25f}, 150.f}; // orange fire (six at a time)
constexpr Glow voreBall{{1.2f, 0.5f, 1.f}, 200.f};      // purple
constexpr Glow enforcerLaser{{1.8f, 1.2f, 0.4f}, 200.f}; // yellow-orange, as its bolt
constexpr Glow cannonLaser{{1.8f, 0.45f, 0.3f}, 200.f}; // the laser cannon's red
constexpr Glow lavaNail{{1.9f, 0.62f, 0.16f}, 110.f};    // molten orange-red (many at a time: small)
constexpr Glow beamBolt{{0.85f, 1.1f, 2.1f}, 170.f};      // the lightning's blue-white, along the beam
constexpr Glow beamEnd{{1.1f, 1.35f, 2.3f}, 230.f};       // and where it strikes

constexpr const char* lavaNailModel = "progs/lspike.mdl"; // Rogue's lava nails (and the lava ogre's)

// The lava nails' lights: only the nearest vr_lavanail_lights, each frame (the super nailgun has a
// dozen in the air, two guns twice as many).
constexpr int maxNailLights = 16;
struct NailLight
{
    int ent;
    float dist2;
};
NailLight nailLights[maxNailLights];
int nailLightCount = 0;
int nailLightFrame = -1;

// Where each lava nail was last frame, for its streak.
struct NailTrail
{
    int ent = 0;
    int frame = -100;
    glm::vec3 pos{0.f};
};
NailTrail nailTrails[32];

// The lightning's lights: keys (below the ammo screens' and the gadget's) and spacing.
constexpr int beamLightKey = -0x5D00; // - beam index * 16 - light
constexpr int maxBeamLights = 12;
constexpr float beamLightSpacing = 80.f;

// A random number in 0..1 from a few integers (the beams' flicker: steady within a frame).
[[nodiscard]] float hash01(unsigned a, unsigned b, unsigned c)
{
    unsigned h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
}

void killDlight(int key)
{
    for(dlight_t& dl : cl_dlights)
    {
        if(dl.key == key && dl.die >= cl.time)
        {
            dl.die = static_cast<float>(cl.time) - 1.f;
        }
    }
}

// The light of an entity's projectile, or null if it has none (following CL_RelinkEntities'
// choice of trail: a model with a gib or rocket trail is not one of these).
[[nodiscard]] const Glow* projectileGlow(const entity_t& e)
{
    const int flags = e.model->flags;
    if(flags & (EF_GIB | EF_ZOMGIB))
    {
        return nullptr;
    }
    if(flags & EF_TRACER)
    {
        return &scragSpit;
    }
    if(flags & EF_TRACER2)
    {
        return &knightFlame;
    }
    if(flags & (EF_ROCKET | EF_GRENADE))
    {
        return nullptr;
    }
    if(flags & EF_TRACER3)
    {
        return &voreBall;
    }
    if(e.effects & EF_DIMLIGHT)
    {
        if(!strcmp(e.model->name, "progs/laser.mdl"))
        {
            return &enforcerLaser;
        }
        if(!strcmp(e.model->name, "progs/lasrspik.mdl"))
        {
            return &cannonLaser;
        }
    }
    return nullptr;
}

// Sets a glow's colour and size for the falloff in use, unshadowed; after its death time. With
// DarkPlaces' falloff the colour is DarkPlaces' (white as bright without vr_colored_lights); with
// Quake's, the colour at most 1 (white without). `fade`: seconds its colour fades out over before
// it dies (0 none).
void setGlow(dlight_t* dl, glm::vec3 color, float radius, float fade)
{
    const bool colored = vr_colored_lights.value != 0.f;
    dl->radius = radius;
    if(vr_dlight_falloff.value != 0.f)
    {
        if(!colored)
        {
            color = glm::vec3{(color.r + color.g + color.b) / 3.f};
        }
        lighting::dlightLook(dl, 0.f, fade);
    }
    else
    {
        color = colored ? color / std::max({color.r, color.g, color.b, 1e-3f}) : glm::vec3{1.f};
        dl->decay = fade > 0.f ? radius / fade : 0.f;
    }
    dl->color[0] = color.r;
    dl->color[1] = color.g;
    dl->color[2] = color.b;
    lighting::dlightNoShadow(dl);
}

// The ammo screens' lights: their keys (no entity's: entities' are positive), how far in front of
// the screen they are, how far they reach, and their cone's inner and outer half angles.
constexpr int screenLightKey = -0x5C00;
constexpr float screenLightOut = 1.f;
constexpr float screenLightRadius = 28.f;
constexpr float screenLightInner = 45.f;
constexpr float screenLightOuter = 85.f;

// A lava nail's streak of embers, from where it was last frame (vr_particles).
void lavaNailTrail(int ent, const entity_t& e)
{
    const glm::vec3 pos{e.origin[0], e.origin[1], e.origin[2]};
    NailTrail* slot = nullptr;
    NailTrail* oldest = &nailTrails[0];
    for(NailTrail& t : nailTrails)
    {
        if(t.ent == ent)
        {
            slot = &t;
            break;
        }
        if(t.frame < oldest->frame)
        {
            oldest = &t;
        }
    }
    if(slot && host_framecount - slot->frame <= 3 && glm::distance(slot->pos, pos) < 160.f)
    {
        particles::lavaNailTrail(slot->pos, pos);
    }
    if(!slot)
    {
        slot = oldest;
    }
    *slot = NailTrail{ent, host_framecount, pos};
}

// A lava nail's light, if it is among the nearest vr_lavanail_lights this frame: a farther one lit
// earlier in the frame gives up its light.
void lavaNailLight(int ent, const entity_t& e, float scale)
{
    const int cap = std::clamp(static_cast<int>(vr_lavanail_lights.value), 0, maxNailLights);
    if(nailLightFrame != host_framecount)
    {
        nailLightFrame = host_framecount;
        nailLightCount = 0;
    }
    const float dx = e.origin[0] - r_refdef.vieworg[0];
    const float dy = e.origin[1] - r_refdef.vieworg[1];
    const float dz = e.origin[2] - r_refdef.vieworg[2];
    const float d2 = dx * dx + dy * dy + dz * dz;

    NailLight* slot = nullptr;
    if(nailLightCount < cap)
    {
        slot = &nailLights[nailLightCount++];
    }
    else if(cap > 0)
    {
        NailLight* farthest = std::max_element(nailLights, nailLights + cap,
            [](const NailLight& a, const NailLight& b) { return a.dist2 < b.dist2; });
        if(d2 < farthest->dist2)
        {
            killDlight(farthest->ent);
            slot = farthest;
        }
    }
    if(!slot)
    {
        killDlight(ent); // lit last frame, farther now
        return;
    }
    *slot = NailLight{ent, d2};

    dlight_t* dl = CL_AllocDlight(ent);
    VectorCopy(e.origin, dl->origin);
    dl->die = static_cast<float>(cl.time + 0.01);
    const float flicker = 0.9f + 0.1f * std::sin(static_cast<float>(cl.time) * 31.f + static_cast<float>(ent) * 2.3f);
    setGlow(dl, lavaNail.color, lavaNail.radius * scale * flicker, 0.f);
}

} // namespace

// Each frame a glowing projectile is relinked: its light where it is drawn, lasting until the next
// frame's (a hell knight's flame flickers a little).
extern "C" void VR_ProjectileLight(int ent)
{
    const float scale = vr_projectile_lights.value;
    const entity_t& e = cl_entities[ent];
    if(ent == cl.viewentity || !e.model)
    {
        return;
    }
    if(!strcmp(e.model->name, lavaNailModel))
    {
        lavaNailTrail(ent, e);
        if(scale > 0.f)
        {
            lavaNailLight(ent, e, scale);
        }
        return;
    }
    if(scale <= 0.f)
    {
        return;
    }
    const Glow* glow = projectileGlow(e);
    if(!glow)
    {
        return;
    }

    dlight_t* dl = CL_AllocDlight(ent);
    VectorCopy(e.origin, dl->origin);
    dl->die = static_cast<float>(cl.time + 0.01);
    float radius = glow->radius * scale;
    if(glow == &knightFlame)
    {
        radius *= 0.92f + 0.08f * std::sin(static_cast<float>(cl.time) * 23.f + static_cast<float>(ent) * 1.7f);
    }
    setGlow(dl, glow->color, radius, 0.f);
}

// A scrag's or a hell knight's spike hitting a wall: a flash of its colour, fading out in a quarter
// of a second.
extern "C" void VR_ProjectileImpactLight(int kind, const float* pos)
{
    const float scale = vr_projectile_lights.value;
    if(scale <= 0.f)
    {
        return;
    }
    const Glow& glow = kind == 0 ? scragSpit : knightFlame;
    dlight_t* dl = CL_AllocDlight(0);
    VectorCopy(pos, dl->origin);
    dl->die = static_cast<float>(cl.time + 0.25);
    setGlow(dl, glow.color, 120.f * scale, 0.25f);
}

// Each frame a lightning beam is drawn: a stream of lights along it, one every beamLightSpacing
// units up to vr_beam_lights of them, the last where it strikes (bigger). They flicker and shift a
// little along the beam each frame, as its bolt segments do, and cast no shadows. The shambler's
// and Chthon's lightning too; not the grappling hook's rope.
extern "C" void VR_BeamLights(int index, qmodel_t* model, const float* start, const float* end)
{
    const int cap = std::clamp(static_cast<int>(vr_beam_lights.value), 0, maxBeamLights);
    if(cap <= 0 || !model || strncmp(model->name, "progs/bolt", 10))
    {
        return;
    }
    const glm::vec3 a{start[0], start[1], start[2]};
    const glm::vec3 b{end[0], end[1], end[2]};
    const float len = glm::distance(a, b);
    if(len < 1.f)
    {
        return;
    }
    const int n = std::clamp(static_cast<int>(std::ceil(len / beamLightSpacing)), 1, cap);
    const float step = len / static_cast<float>(n);
    const unsigned frame = static_cast<unsigned>(cl.time * 30.0); // the flicker's rate: 30 a second
    for(int k = 0; k < n; k++)
    {
        const bool last = k == n - 1;
        const float jitter = last ? 0.f : (hash01(frame, static_cast<unsigned>(index), static_cast<unsigned>(k) * 2u) - 0.5f) * 0.5f;
        const float t = (static_cast<float>(k + 1) + jitter) / static_cast<float>(n);
        const glm::vec3 p = glm::mix(a, b, t);
        const float flicker = 0.7f + 0.4f * hash01(frame, static_cast<unsigned>(index), static_cast<unsigned>(k) * 2u + 1u);
        const Glow& glow = last ? beamEnd : beamBolt;
        // Near lights fill the gaps between them; a short beam's few lights are no bigger.
        const float radius = last ? glow.radius : std::clamp(step * 2.f, 110.f, glow.radius);

        dlight_t* dl = CL_AllocDlight(beamLightKey - index * 16 - k);
        dl->origin[0] = p.x;
        dl->origin[1] = p.y;
        dl->origin[2] = p.z;
        dl->die = static_cast<float>(cl.time + 0.05);
        setGlow(dl, glow.color * flicker, radius * (0.9f + 0.1f * flicker), 0.f);
    }
    // Fewer lights than last frame (a shorter beam): the rest go out.
    for(int k = n; k < maxBeamLights; k++)
    {
        killDlight(beamLightKey - index * 16 - k);
    }
}

void emissive::weaponScreenLight(int hand, const glm::vec3& pos, const glm::vec3& angles)
{
    const float k = vr_weapon_screen_light.value;
    const float bright = std::clamp(vr_gadget_screen_brightness.value, 0.f, 2.f);
    if(k <= 0.f || bright <= 0.f)
    {
        return;
    }

    vec3_t a{angles.x, angles.y, angles.z}, f, r, u;
    AngleVectors(a, f, r, u);
    const glm::vec3 n = glm::normalize(glm::cross(glm::vec3{r[0], r[1], r[2]}, glm::vec3{u[0], u[1], u[2]}));

    // A small, bright spot light on the screen, the way it faces: what is in front of the screen
    // (the hand holding the gun, the arm, a wall close by) is lit in its text's colour, and
    // nothing beside or behind it (the gun's sides and back, outside its cone), nor the room (out of
    // its reach). DarkPlaces' falloff is full to about 40% of the radius.
    dlight_t* dl = CL_AllocDlight(screenLightKey - hand);
    const glm::vec3 p = pos + n * screenLightOut;
    dl->origin[0] = p.x;
    dl->origin[1] = p.y;
    dl->origin[2] = p.z;
    dl->die = static_cast<float>(cl.time + 0.05);
    dl->radius = screenLightRadius;
    const bool darkplaces = vr_dlight_falloff.value != 0.f;
    const glm::vec3 c = hsv(vr_gadget_screen_hue.value, 0.55f, 1.f) * (bright * k * (darkplaces ? 1.5f : 2.5f));
    dl->color[0] = c.r;
    dl->color[1] = c.g;
    dl->color[2] = c.b;
    if(darkplaces)
    {
        lighting::dlightLook(dl, 0.f, 0.f);
    }
    lighting::dlightSpot(dl, n, screenLightInner, screenLightOuter);
    lighting::dlightNoShadow(dl);
}

// The held weapons' dim fullbright texels (the shotgun's sights) shine brighter: the alias shader
// adds the fullbright colour (the fullbright texture's, or for Ironwail's ALPHABRIGHT skins the
// unlit share of the skin, alpha 0) times 1 + boost * (1 - its brightness), so that dim red sights
// (palette 226-228) reach full red and the bloom (vr_bloom_color) makes them glow, while bright ones
// (muzzle flashes) hardly change.
extern "C" float VR_EntityFullbrightBoost(const entity_t* e)
{
    // A lava nail's molten skin (fullbright 229-236, dim oranges) burns bright orange, for the
    // bloom, with the projectiles' lights.
    if(e->model && vr_projectile_lights.value > 0.f && !strcmp(e->model->name, lavaNailModel))
    {
        return 2.5f;
    }
    const float k = vr_weapon_glow.value;
    if(k <= 0.f || !e->model || e->model->type != mod_alias || !view::find(e))
    {
        return 0.f;
    }
    const char* n = e->model->name;
    if(!strncmp(n, "progs/hand", 10) || !strncmp(n, "progs/finger_", 13) || weapons::slotForModel(e->model) < 0)
    {
        return 0.f;
    }
    return 3.f * k;
}
