// vr_emissive.cpp -- see vr_emissive.hpp.

#include "vr_emissive.hpp"
#include "vr_color.hpp"
#include "vr_cvars.hpp"
#include "vr_hue.hpp"
#include "vr_lighting.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"
#include "vr_trace.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

extern "C" int* cl_efrags; // gl_refrag.c: each static entity's leaves (a count, then the leaves)

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
// it dies (0 none). `shadow`: it may take one of vr_shadow_dlights' shadows (the nearest torches).
void setGlow(dlight_t* dl, glm::vec3 color, float radius, float fade, bool shadow = false)
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
    if(!shadow)
    {
        lighting::dlightNoShadow(dl);
    }
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

// ---- Torches and flames (round 17) ----------------------------------------------------------

// A flame model: where its fire is (model units, before its yaw and scale), how far its light
// reaches and its colour (DarkPlaces' brightness: a tenth of a muzzle flash's white 4 in the same
// radius: the baked light already lights the room, this only moves it a little).
struct TorchKind
{
    const char* model;
    int frame; // -1: any
    glm::vec3 fire;
    float radius;
    glm::vec3 color;
};

constexpr TorchKind torchKinds[] = {
    {"progs/flame.mdl", -1, {0.f, 0.f, 20.f}, 150.f, {0.45f, 0.28f, 0.135f}}, // light_torch_small_walltorch
    {"progs/flame2.mdl", 1, {0.f, 0.f, 14.f}, 170.f, {0.5f, 0.31f, 0.15f}},   // light_flame_large_yellow
    {"progs/flame2.mdl", -1, {0.f, 0.f, 3.f}, 130.f, {0.42f, 0.26f, 0.125f}}, // light_flame_small_*
    {"progs/candle.mdl", -1, {0.f, 0.f, 10.f}, 80.f, {0.3f, 0.19f, 0.09f}},    // Rogue's light_candle
    {"progs/lantern.mdl", -1, {5.f, 0.f, 4.f}, 120.f, {0.38f, 0.24f, 0.115f}}, // Rogue's light_lantern
};

[[nodiscard]] const TorchKind* torchKind(const entity_t& e)
{
    if(!e.model || e.model->type != mod_alias || strncmp(e.model->name, "progs/", 6))
    {
        return nullptr;
    }
    for(const TorchKind& k : torchKinds)
    {
        if((k.frame < 0 || k.frame == e.frame) && !strcmp(e.model->name, k.model))
        {
            return &k;
        }
    }
    return nullptr;
}

// Keys: -0x6000 - a static entity's index (up to 4095), -0x7000 - an entity's number.
constexpr int torchLightKey = -0x6000;
constexpr int torchDynamicId = 0x1000;
constexpr int maxTorchLights = 16;
constexpr float torchLightDistance = 1200.f; // none farther (fading out over the last quarter)
constexpr float torchFadeTime = 0.3f;        // seconds a torch's light fades in or out over
constexpr float torchStandoff = 18.f;        // the light kept this far off walls near the flame

// A torch seen: its light's place (off the wall behind it), how lit it is (fading), its seed.
struct TorchState
{
    glm::vec3 pos{0.f};
    const TorchKind* kind = nullptr;
    float scale = 1.f;
    float weight = 0.f;
    int rank = -1; // among the chosen this frame, nearest first
    bool chosen = false;
    bool lit = false;
    unsigned seed = 0;
};
std::unordered_map<int, TorchState> torches;
const qmodel_t* torchWorld = nullptr;
double torchLastTime = 0.0;

// Smooth value noise in -1..1 (a random value at each whole x, eased between).
[[nodiscard]] float valueNoise(double x, unsigned seed)
{
    const double f = std::floor(x);
    const auto i = static_cast<unsigned>(static_cast<long long>(f));
    const float t = static_cast<float>(x - f);
    const float s = t * t * (3.f - 2.f * t);
    const float a = hash01(i, seed, 17u);
    const float b = hash01(i + 1u, seed, 17u);
    return (a + (b - a) * s) * 2.f - 1.f;
}

// A flame's flicker, -1..1 about 0: two noises at about 5-9 and 9-14 Hz and a slower sway, the
// same whatever the frame rate (from the client's time).
[[nodiscard]] float torchFlicker(double t, unsigned seed)
{
    return 0.5f * valueNoise(t * 9.0, seed) + 0.3f * valueNoise(t * 14.0 + 31.7, seed ^ 0x55u)
           + 0.2f * valueNoise(t * 2.3 + 7.1, seed ^ 0xAAu);
}

// Where a torch's light goes: at its fire, moved out to torchStandoff from walls close by (a wall
// torch's light otherwise sits a few units from the wall and shines through it into the next
// room). Eight short traces, once per torch.
[[nodiscard]] glm::vec3 placeTorchLight(const glm::vec3& fire)
{
    glm::vec3 push{0.f};
    for(int k = 0; k < 8; k++)
    {
        const float a = static_cast<float>(k) * (glm::pi<float>() / 4.f);
        const glm::vec3 dir{std::cos(a), std::sin(a), 0.f};
        const trace_t tr = worldtrace::world(fire, fire + dir * torchStandoff, false);
        if(tr.startsolid || tr.allsolid)
        {
            return fire;
        }
        if(tr.fraction < 1.f)
        {
            push += worldtrace::normal(tr) * (torchStandoff * (1.f - tr.fraction));
        }
    }
    push.z = 0.f;
    const float len = glm::length(push);
    if(len < 0.5f)
    {
        return fire;
    }
    push *= std::min(len, torchStandoff) / len;
    const trace_t tr = worldtrace::world(fire, fire + push, false);
    return glm::mix(fire, fire + push, std::max(0.f, tr.fraction - 0.05f));
}

[[nodiscard]] bool leafVisible(int leaf, const byte* vis)
{
    return !vis || (vis[leaf >> 3] & (1 << (leaf & 7))) != 0;
}

void killTorchLight(int id, TorchState& st)
{
    if(st.lit)
    {
        killDlight(torchLightKey - id);
        st.lit = false;
    }
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

// Each client frame, after the entities: torches and flames (Quake's wall torches and flame balls,
// Rogue's candles and lanterns; static entities, or entities with those models) flicker a small
// warm light onto the room. The nearest vr_torch_lights of those in the viewer's PVS within
// torchLightDistance, each fading in and out as it is chosen or dropped (the nearest
// vr_torch_light_shadows of them may cast shadows); brightness vr_torch_light_scale.
extern "C" void VR_TorchLights(void)
{
    QVR_PROFILE("torch lights");
    const int cap = std::clamp(static_cast<int>(vr_torch_lights.value), 0, maxTorchLights);
    if(cls.state != ca_connected || !cl.worldmodel)
    {
        return;
    }
    if(cl.worldmodel != torchWorld || cl.time < torchLastTime)
    {
        torches.clear(); // a new map (its lights were cleared with the client's state)
        torchWorld = cl.worldmodel;
        torchLastTime = cl.time;
    }
    const float dt = static_cast<float>(std::clamp(cl.time - torchLastTime, 0.0, 0.1));
    torchLastTime = cl.time;
    if(cap == 0 && torches.empty())
    {
        return;
    }

    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    vec3_t eyev{eye.x, eye.y, eye.z};
    const mleaf_t* eyeLeaf = Mod_PointInLeaf(eyev, cl.worldmodel);
    const byte* vis = !eyeLeaf || eyeLeaf->contents == CONTENTS_SOLID
                          ? nullptr
                          : Mod_LeafPVS(const_cast<mleaf_t*>(eyeLeaf), cl.worldmodel);

    // The candidates: torches in the PVS, near enough.
    struct Candidate
    {
        int id;
        float score;
    };
    static std::vector<Candidate> candidates;
    candidates.clear();
    const float maxDist2 = torchLightDistance * torchLightDistance;
    const auto consider = [&](int id, const entity_t& e, const TorchKind* kind) {
        const glm::vec3 org{e.origin[0], e.origin[1], e.origin[2]};
        const float d2 = glm::dot(org - eye, org - eye);
        if(d2 > maxDist2)
        {
            return;
        }
        TorchState& st = torches[id];
        if(st.kind != kind)
        {
            // First seen (or another flame): place its light, seed its flicker by where it is.
            const float s = e.scale ? ENTSCALE_DECODE(e.scale) : 1.f;
            const float yaw = glm::radians(e.angles[1]);
            const glm::vec3 off{kind->fire.x * std::cos(yaw) - kind->fire.y * std::sin(yaw),
                kind->fire.x * std::sin(yaw) + kind->fire.y * std::cos(yaw), kind->fire.z};
            st.kind = kind;
            st.scale = s;
            st.pos = placeTorchLight(org + off * s);
            st.seed = static_cast<unsigned>(static_cast<int>(org.x) * 73856093 ^ static_cast<int>(org.y) * 19349663
                                            ^ static_cast<int>(org.z) * 83492791);
        }
        candidates.push_back({id, std::sqrt(d2) * (st.chosen ? 0.8f : 1.f)}); // hysteresis
    };
    if(cap > 0)
    {
        const int* efrags = cl_efrags;
        for(int i = 0; i < cl.num_statics && efrags; i++)
        {
            const entity_t& e = cl_static_entities[i];
            if(!e.model)
            {
                continue;
            }
            const int count = *efrags++;
            const TorchKind* kind = torchKind(e);
            if(kind)
            {
                bool seen = false;
                for(int j = 0; j < count && !seen; j++)
                {
                    seen = leafVisible(efrags[j], vis);
                }
                if(seen)
                {
                    consider(i, e, kind);
                }
            }
            efrags += count;
        }
        for(int i = 1; i < cl.num_entities; i++)
        {
            const entity_t& e = cl_entities[i];
            const TorchKind* kind = e.model && i != cl.viewentity ? torchKind(e) : nullptr;
            if(kind && e.msgtime == cl.mtime[0])
            {
                vec3_t o{e.origin[0], e.origin[1], e.origin[2]};
                const std::ptrdiff_t leaf = Mod_PointInLeaf(o, cl.worldmodel) - cl.worldmodel->leafs - 1;
                if(leaf < 0 || leafVisible(static_cast<int>(leaf), vis))
                {
                    consider(torchDynamicId + i, e, kind);
                }
            }
        }
    }
    const size_t chosenCount = std::min(candidates.size(), static_cast<size_t>(cap));
    std::partial_sort(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(chosenCount),
        candidates.end(), [](const Candidate& a, const Candidate& b) { return a.score < b.score; });

    for(auto& [id, st] : torches)
    {
        st.chosen = false;
        st.rank = -1;
    }
    for(size_t c = 0; c < chosenCount; c++)
    {
        TorchState& st = torches[candidates[c].id];
        st.chosen = true;
        st.rank = static_cast<int>(c);
    }

    // Each fades towards its target: lit if chosen (less towards torchLightDistance), out if not.
    // Of those fading out, the brightest few keep their light until they are out, the rest go out
    // now.
    const float step = dt / torchFadeTime;
    static std::vector<std::pair<float, int>> fading;
    fading.clear();
    for(auto& [id, st] : torches)
    {
        const float d = glm::distance(st.pos, eye);
        const float target = st.chosen ? std::clamp((torchLightDistance - d) / (0.25f * torchLightDistance), 0.f, 1.f) : 0.f;
        st.weight = st.weight < target ? std::min(target, st.weight + step) : std::max(target, st.weight - step);
        if(!st.chosen && st.weight > 0.f)
        {
            fading.emplace_back(st.weight, id);
        }
    }
    if(fading.size() > 4)
    {
        std::sort(fading.begin(), fading.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        for(size_t f = 4; f < fading.size(); f++)
        {
            torches[fading[f].second].weight = 0.f;
        }
    }

    // Light them.
    const float scale = std::max(0.f, vr_torch_light_scale.value);
    const int shadowed = std::clamp(static_cast<int>(vr_torch_light_shadows.value), 0, cap);
    const bool darkplaces = vr_dlight_falloff.value != 0.f;
    const double t = cl.time;
    for(auto& [id, st] : torches)
    {
        if(st.weight <= 0.f || scale <= 0.f || !st.kind)
        {
            killTorchLight(id, st);
            continue;
        }
        const float n = torchFlicker(t, st.seed);
        const float k = 1.f + 0.3f * n; // +-30% brightness (at most; mostly less)
        const glm::vec3 jitter{0.8f * valueNoise(t * 6.0 + 3.3, st.seed ^ 0x1234u),
            0.8f * valueNoise(t * 6.0 + 9.1, st.seed ^ 0x4321u), 1.2f * valueNoise(t * 7.0 + 5.7, st.seed ^ 0x2468u)};
        const glm::vec3 p = st.pos + jitter * st.scale;

        dlight_t* dl = CL_AllocDlight(torchLightKey - id);
        dl->origin[0] = p.x;
        dl->origin[1] = p.y;
        dl->origin[2] = p.z;
        dl->die = static_cast<float>(cl.time + 0.1);
        float radius = st.kind->radius * st.scale * (1.f + 0.05f * n) * (0.75f + 0.25f * st.weight);
        glm::vec3 color = st.kind->color * (scale * k * st.weight);
        if(!darkplaces)
        {
            // Quake's falloff: the colour is at most 1 (setGlow), so the flicker and the scale
            // change the reach.
            radius *= std::sqrt(std::clamp(scale * k * st.weight, 0.f, 2.f));
        }
        setGlow(dl, color, radius, 0.f, st.chosen && st.rank < shadowed);
        st.lit = true;
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
    const glm::vec3 c = hue::color(vr_gadget_screen_hue, 0.55f, 1.f) * (bright * k * (darkplaces ? 1.5f : 2.5f));
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
