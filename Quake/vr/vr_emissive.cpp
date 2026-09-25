// vr_emissive.cpp -- see vr_emissive.hpp.

#include "vr_emissive.hpp"
#include "vr_color.hpp"
#include "vr_cvars.hpp"
#include "vr_lighting.hpp"
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

// The ammo screens' lights' keys (no entity's: entities' are positive).
constexpr int screenLightKey = -0x5C00;

} // namespace

// Each frame a glowing projectile is relinked: its light where it is drawn, lasting until the next
// frame's (a hell knight's flame flickers a little).
extern "C" void VR_ProjectileLight(int ent)
{
    const float scale = vr_projectile_lights.value;
    const entity_t& e = cl_entities[ent];
    if(scale <= 0.f || ent == cl.viewentity || !e.model)
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

    // A little in front of the screen: it lights the hand holding the gun, the gun's back and what
    // is close by, faintly, in the screen's colour (its text's, less saturated).
    dlight_t* dl = CL_AllocDlight(screenLightKey - hand);
    const glm::vec3 p = pos + n * 3.f;
    dl->origin[0] = p.x;
    dl->origin[1] = p.y;
    dl->origin[2] = p.z;
    dl->die = static_cast<float>(cl.time + 0.05);
    dl->radius = 56.f;
    const bool darkplaces = vr_dlight_falloff.value != 0.f;
    const glm::vec3 c = hsv(vr_gadget_screen_hue.value, 0.45f, 1.f) * (bright * k * (darkplaces ? 0.5f : 0.8f));
    dl->color[0] = c.r;
    dl->color[1] = c.g;
    dl->color[2] = c.b;
    if(darkplaces)
    {
        lighting::dlightLook(dl, 0.f, 0.f);
    }
    lighting::dlightNoShadow(dl);
}

// The held weapons' dim fullbright texels (the shotgun's sights) shine brighter: the alias shader
// adds the fullbright colour (the fullbright texture's, or for Ironwail's ALPHABRIGHT skins the
// unlit share of the skin, alpha 0) times 1 + boost * (1 - its brightness), so that dim red sights
// (palette 226-228) reach full red and the bloom (vr_bloom_color) makes them glow, while bright ones
// (muzzle flashes) hardly change.
extern "C" float VR_EntityFullbrightBoost(const entity_t* e)
{
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
