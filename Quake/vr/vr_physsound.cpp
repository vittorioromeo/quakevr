// vr_physsound.cpp -- the physics sounds; see vr_physsound.hpp and docs/vr-port/ROUND21.md, "Physics sounds".
//
// - Impacts. Box3D reports each step's hits faster than its hit threshold (1 m/s; vr_box3d.cpp soundHits: a prop against
//   the level, a door, another prop, a hand or a held weapon; not a monster's or a player's body, whose touches have their
//   own sounds in QC). Each prop's hardest hit of the frame plays if it is at least vr_physsound_min_speed, unless the
//   prop knocked less than vr_physsound_interval ago (a hit twice as loud still plays) or less than bounceWindow
//   intervals ago and this one is under bounceShare of that knock (the small hop after a landing): a resting stack's
//   jitter and a bouncing box's rattle stay quiet. One of the recordings of its material (materialOf) and weight
//   (light under lightMass kg, heavy from heavyMass), never the one played last; louder the faster (from a tenth at the
//   least speed to all of it at vr_physsound_full_speed) and the heavier (massGain), at most maxImpactsAFrame a frame
//   (the loudest). From the prop's entity at the contact, so that each client hears it where it hit.
// - Scrapes. A prop sliding on what it rests on or leans against (its contact points' relative speed along the contact,
//   rolling taken off, pressed on with at least a fifth of its weight; vr_box3d.cpp noteSlide) scrapes: grains of 0.5 s
//   of recorded sliding played back to back on two channels of the prop, each starting as the last fades out (their
//   fades cross), each at the loudness of the slide as it starts (vr_physsound_scrape_min to vr_physsound_scrape_full
//   m/s, by weight and how hard it is pressed). It starts once the prop has slid scrapeDelay s at scrapeStartVolume or
//   louder (a bounce's graze, a faint shuffle in a pile don't); a frame not sliding stops it at once (both channels).
// - Grabs. A climbing hand taking a hold (vr_climb.cpp): a palm's slap and a quiet tap of the hold's material, the
//   texture under the hold (debris::materialOf's table: wood, metal, the rest stone), vr_physsound_grab loud.
//
// Everything goes through the server's datagram as Quake's sounds do (by precache index: nothing looked up by name as
// they play); the recordings are precached at each map's start with Quake VR's progs (precache).

#include "vr_physsound.hpp"
#include "vr_cvars.hpp"
#include "vr_debris.hpp"
#include "vr_mem.hpp"
#include "vr_physics.hpp"
#include "vr_progs.hpp"
#include "vr_profile.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Log.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <stdlib.h>
#include <string.h>

using namespace qvr;

namespace
{

using physsound::Material;

constexpr int materialCount = static_cast<int>(Material::Count);
constexpr float lightMass = 1.5f;  // kg: lighter plays the light recordings
constexpr float heavyMass = 10.f;  // kg: this and heavier the heavy ones
constexpr int maxImpactsAFrame = 6;
constexpr double bounceWindow = 3.0; // intervals (vr_physsound_interval) after a knock: a hit this soon is a bounce's
constexpr float bounceShare = 0.35f; // tail, silent unless at least this share of the knock's volume
constexpr int maxGrainsAFrame = 8;
constexpr float scrapeStartVolume = 0.05f; // a scrape starts this loud at least (a faint shuffle in a pile is silent)
constexpr double scrapeDelay = 0.08; // s: a prop scrapes once it has slid this long (not a bounce's graze)
constexpr double grainLength = 0.5; // s (make_physics_sounds.py GRAIN)
constexpr double grainFade = 0.08;  // s: the next grain starts this long before one ends (FADE)
constexpr int scrapeChannels[2] = {5, 7}; // the prop's own channels for its scrape (QC's props use 0 to 4)

// The recordings: impacts by material and weight (light, medium, heavy), scrapes and grabs by material. The names are
// literals: they outlive the server's precache list that points at them.
struct Set
{
    const char* files[5];
    int count;
};

// clang-format off
constexpr Set impactSets[materialCount][3] = {
    {{{}, 0}, {{}, 0}, {{}, 0}}, // None
    {{{"vr/phys/wood_l1.wav", "vr/phys/wood_l2.wav", "vr/phys/wood_l3.wav", "vr/phys/wood_l4.wav"}, 4},
     {{"vr/phys/wood_m1.wav", "vr/phys/wood_m2.wav", "vr/phys/wood_m3.wav", "vr/phys/wood_m4.wav"}, 4},
     {{"vr/phys/wood_h1.wav", "vr/phys/wood_h2.wav", "vr/phys/wood_h3.wav", "vr/phys/wood_h4.wav"}, 4}},
    {{{"vr/phys/metal_l1.wav", "vr/phys/metal_l2.wav", "vr/phys/metal_l3.wav", "vr/phys/metal_l4.wav"}, 4},
     {{"vr/phys/metal_m1.wav", "vr/phys/metal_m2.wav", "vr/phys/metal_m3.wav", "vr/phys/metal_m4.wav"}, 4},
     {{"vr/phys/metal_h1.wav", "vr/phys/metal_h2.wav", "vr/phys/metal_h3.wav", "vr/phys/metal_h4.wav"}, 4}},
    {{{"vr/rock1.wav", "vr/rock2.wav", "vr/rock3.wav"}, 3}, // (make_sounds.py's: QC vr_debris.qc played them before)
     {{"vr/rock1.wav", "vr/rock2.wav", "vr/rock3.wav"}, 3},
     {{"vr/rock1.wav", "vr/rock2.wav", "vr/rock3.wav"}, 3}},
    {{{"vr/brick1.wav", "vr/brick2.wav", "vr/brick3.wav"}, 3},
     {{"vr/brick1.wav", "vr/brick2.wav", "vr/brick3.wav"}, 3},
     {{"vr/brick1.wav", "vr/brick2.wav", "vr/brick3.wav"}, 3}},
    {{{"vr/phys/soft_m1.wav", "vr/phys/soft_m2.wav", "vr/phys/soft_m3.wav", "vr/phys/soft_m4.wav"}, 4},
     {{"vr/phys/soft_m1.wav", "vr/phys/soft_m2.wav", "vr/phys/soft_m3.wav", "vr/phys/soft_m4.wav"}, 4},
     {{"vr/phys/soft_h1.wav", "vr/phys/soft_h2.wav", "vr/phys/soft_h3.wav", "vr/phys/soft_h4.wav"}, 4}},
    {{{"vr/phys/flesh_m1.wav", "vr/phys/flesh_m2.wav", "vr/phys/flesh_m3.wav", "vr/phys/flesh_m4.wav"}, 4},
     {{"vr/phys/flesh_m1.wav", "vr/phys/flesh_m2.wav", "vr/phys/flesh_m3.wav", "vr/phys/flesh_m4.wav", "zombie/z_miss.wav"}, 5},
     {{"vr/phys/flesh_h1.wav", "vr/phys/flesh_h2.wav", "vr/phys/flesh_h3.wav", "vr/phys/flesh_h4.wav", "zombie/z_miss.wav"}, 5}},
};
constexpr Set scrapeSets[materialCount] = {
    {{}, 0},
    {{"vr/phys/scrape_wood1.wav", "vr/phys/scrape_wood2.wav", "vr/phys/scrape_wood3.wav", "vr/phys/scrape_wood4.wav"}, 4},
    {{"vr/phys/scrape_metal1.wav", "vr/phys/scrape_metal2.wav", "vr/phys/scrape_metal3.wav", "vr/phys/scrape_metal4.wav"}, 4},
    {{"vr/phys/scrape_stone1.wav", "vr/phys/scrape_stone2.wav", "vr/phys/scrape_stone3.wav", "vr/phys/scrape_stone4.wav"}, 4},
    {{"vr/phys/scrape_stone1.wav", "vr/phys/scrape_stone2.wav", "vr/phys/scrape_stone3.wav", "vr/phys/scrape_stone4.wav"}, 4},
    {{"vr/phys/scrape_soft1.wav", "vr/phys/scrape_soft2.wav", "vr/phys/scrape_soft3.wav", "vr/phys/scrape_soft4.wav"}, 4},
    {{"vr/phys/scrape_soft1.wav", "vr/phys/scrape_soft2.wav", "vr/phys/scrape_soft3.wav", "vr/phys/scrape_soft4.wav"}, 4},
};
enum class GrabKind : int { Wood, Metal, Stone, Count };
constexpr Set grabSets[static_cast<int>(GrabKind::Count)] = {
    {{"vr/phys/grab_wood1.wav", "vr/phys/grab_wood2.wav", "vr/phys/grab_wood3.wav"}, 3},
    {{"vr/phys/grab_metal1.wav", "vr/phys/grab_metal2.wav", "vr/phys/grab_metal3.wav"}, 3},
    {{"vr/phys/grab_stone1.wav", "vr/phys/grab_stone2.wav", "vr/phys/grab_stone3.wav"}, 3},
};
// clang-format on

// How loud each material's recordings play (their loudness is matched by weight in make_physics_sounds.py).
constexpr float materialGain[materialCount] = {0.f, 1.f, 0.85f, 1.f, 1.f, 0.9f, 0.9f};

// A set's recordings' precache indices (this server's; 0: not precached) and the one played last.
struct Indices
{
    za::Array<int, 5> index{};
    int last{-1};
};

// A prop's sound state, by its edict number.
struct Body
{
    double lastHit{-1e9};  // the server's time of its last impact sound
    float lastVolume{0.f};
    uint32_t slideFrame{0}; // the last frame it slid (frameEnd's count)
    double slideSince{0.0}; // the server's time it began to slide (in a row)
    bool scraping{false};
    int channel{0};         // the next grain's (scrapeChannels)
    double nextGrain{0.0};
};

struct Hit
{
    int num;
    Material material;
    float mass, speed;
    glm::vec3 at;
    float volume;
};

struct Slide
{
    int num;
    Material material;
    float mass, slip, press;
    glm::vec3 at;
};

// The server's state (the main thread): the precache indices, the props' sound state, this frame's hits and slides.
struct State
{
    Indices impacts[materialCount][3];
    Indices scrapes[materialCount];
    Indices grabs[static_cast<int>(GrabKind::Count)];
    za::Vector<Body> bodies;   // by edict number
    za::Vector<int> scraping;  // the props scraping now
    za::Vector<Hit> hits;      // this frame's, the hardest per prop
    za::Vector<Slide> slides;  // this frame's
    uint32_t frames{1};
    int played{0}, skipped{0};  // the frame's impacts (vr_debug_physsound 2)
};
State state;

// The frame's buffers (the main thread).
struct PhysSoundScratch
{
    za::Vector<int> stillScraping; // (frameEnd)
    auto members() { return qvr::mem::list(stillScraping); }
};
mem::Scratch<PhysSoundScratch> scratch{"physsound"};

[[nodiscard]] bool debug(int level = 1)
{
    return vr_debug_physsound.value >= static_cast<float>(level);
}

[[nodiscard]] float master()
{
    return za::clamp(vr_physsound.value, 0.f, 1.f);
}

[[nodiscard]] Body& bodyOf(int num)
{
    if(num >= static_cast<int>(state.bodies.size()))
    {
        state.bodies.resize(static_cast<size_t>(num) + 64);
    }
    return state.bodies[num];
}

// As PF_precache_sound, while the server loads (the names are literals: they outlive the server).
int precacheName(const char* name)
{
    for(int i = 1; i < MAX_SOUNDS; i++)
    {
        if(!sv.sound_precache[i])
        {
            sv.sound_precache[i] = name;
        }
        if(!ZA_STRCMP(sv.sound_precache[i], name))
        {
            return i;
        }
    }
    return 0;
}

void precacheSet(const Set& set, Indices& out)
{
    out = Indices{};
    for(int k = 0; k < set.count; k++)
    {
        out.index[k] = precacheName(set.files[k]);
    }
}

// One of a set's recordings at random, never the one played last: its precache index (0: none), its name in `name`.
[[nodiscard]] int pick(const Set& set, Indices& ind, const char** name = nullptr)
{
    if(set.count <= 0)
    {
        return 0;
    }
    int k = set.count > 1 ? rand() % (ind.last >= 0 ? set.count - 1 : set.count) : 0;
    if(ind.last >= 0 && set.count > 1 && k >= ind.last)
    {
        k++;
    }
    ind.last = k;
    if(name)
    {
        *name = set.files[k];
    }
    return ind.index[k];
}

// A sound from entity `ent` on `channel` (0: any free one) at `at`, as SV_StartSound sends it (by precache index).
bool emit(int ent, int channel, int index, float volume, float attenuation, const glm::vec3& at)
{
    const int vol = static_cast<int>(za::lround(za::clamp(volume, 0.f, 1.f) * 255.f));
    if(index <= 0 || vol <= 0 || sv.datagram.cursize > MAX_DATAGRAM - 24)
    {
        return false;
    }
    int mask = 0;
    if(vol != DEFAULT_SOUND_PACKET_VOLUME)
    {
        mask |= SND_VOLUME;
    }
    if(attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
    {
        mask |= SND_ATTENUATION;
    }
    if(ent >= 8192)
    {
        if(sv.protocol == PROTOCOL_NETQUAKE)
        {
            return false;
        }
        mask |= SND_LARGEENTITY;
    }
    if(index >= 256)
    {
        if(sv.protocol == PROTOCOL_NETQUAKE)
        {
            return false;
        }
        mask |= SND_LARGESOUND;
    }
    MSG_WriteByte(&sv.datagram, svc_sound);
    MSG_WriteByte(&sv.datagram, mask);
    if(mask & SND_VOLUME)
    {
        MSG_WriteByte(&sv.datagram, vol);
    }
    if(mask & SND_ATTENUATION)
    {
        MSG_WriteByte(&sv.datagram, static_cast<int>(attenuation * 64.f));
    }
    if(mask & SND_LARGEENTITY)
    {
        MSG_WriteShort(&sv.datagram, ent);
        MSG_WriteByte(&sv.datagram, channel);
    }
    else
    {
        MSG_WriteShort(&sv.datagram, (ent << 3) | channel);
    }
    if(mask & SND_LARGESOUND)
    {
        MSG_WriteShort(&sv.datagram, index);
    }
    else
    {
        MSG_WriteByte(&sv.datagram, index);
    }
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(&sv.datagram, at[i], sv.protocolflags);
    }
    return true;
}

// What entity `ent` plays on `channel` stopped (svc_stopsound: an entity under 8192).
void stop(int ent, int channel)
{
    if(ent < 8192 && sv.datagram.cursize <= MAX_DATAGRAM - 8)
    {
        MSG_WriteByte(&sv.datagram, svc_stopsound);
        MSG_WriteShort(&sv.datagram, (ent << 3) | channel);
    }
}

[[nodiscard]] int weightOf(float mass)
{
    return mass < lightMass ? 0 : mass < heavyMass ? 1 : 2;
}

// Heavier is louder: 0.5 at 0.1 kg and less, 0.7 at 1 kg, all of it from 20 kg.
[[nodiscard]] float massGain(float mass)
{
    return za::clamp(0.7f + 0.1f * za::log(za::max(mass, 0.01f)), 0.5f, 1.f);
}

// How far `v` is from `lo` to `hi` (0 .. 1).
[[nodiscard]] float ramp(float v, float lo, float hi)
{
    return za::clamp((v - lo) / za::max(hi - lo, 0.01f), 0.f, 1.f);
}

[[nodiscard]] float impactVolume(Material m, float mass, float speed)
{
    const float lo = za::max(vr_physsound_min_speed.value, 0.f);
    if(speed < lo)
    {
        return 0.f;
    }
    const float t = ramp(speed, lo, vr_physsound_full_speed.value);
    return master() * za::max(vr_physsound_impact.value, 0.f) * materialGain[static_cast<int>(m)] * massGain(mass) *
           (0.1f + 0.9f * t);
}

[[nodiscard]] float scrapeVolume(Material m, float mass, float slip, float press)
{
    const float lo = za::max(vr_physsound_scrape_min.value, 0.f);
    if(slip < lo || press < 0.2f)
    {
        return 0.f;
    }
    const float t = ramp(slip, lo, vr_physsound_scrape_full.value);
    return master() * za::max(vr_physsound_scrape.value, 0.f) * materialGain[static_cast<int>(m)] * massGain(mass) *
           za::sqrt(za::min(press, 1.f)) * (0.15f + 0.85f * t);
}

void stopScrape(int num, Body& b, const char* why)
{
    b.scraping = false;
    for(const int ch : scrapeChannels)
    {
        stop(num, ch);
    }
    if(debug())
    {
        Con_Printf("physsound: %.2f %d scrape stops (%s)\n", qcvm->time, num, why);
    }
}

// ---- The texture under a point (the grab's hold)

// The first surface of brush model `m` the segment from `start` to `end` (the model's space) crosses where its texture's
// extents hold the crossing (RecursiveLightPoint's walk, gl_rlight.c, without the lightmaps).
const msurface_t* surfaceAlong(const qmodel_t* m, const mnode_t* node, const glm::vec3& start, const glm::vec3& end, int depth)
{
    while(node && node->contents >= 0 && depth < 256)
    {
        const mplane_t* p = node->plane;
        const glm::vec3 n{p->normal[0], p->normal[1], p->normal[2]};
        const float front = glm::dot(start, n) - p->dist;
        const float back = glm::dot(end, n) - p->dist;
        if((back < 0.f) == (front < 0.f))
        {
            node = node->children[front < 0.f];
            continue;
        }
        const glm::vec3 mid = start + (end - start) * (front / (front - back));
        if(const msurface_t* s = surfaceAlong(m, node->children[front < 0.f], start, mid, depth + 1))
        {
            return s;
        }
        const msurface_t* surf = m->surfaces + node->firstsurface;
        for(unsigned int i = 0; i < node->numsurfaces; i++, surf++)
        {
            if(!surf->texinfo || surf->texinfo->texnum < 0 || surf->texinfo->texnum >= m->numtextures || !m->textures[surf->texinfo->texnum])
            {
                continue;
            }
            const float* v0 = surf->texinfo->vecs[0];
            const float* v1 = surf->texinfo->vecs[1];
            const int ds = static_cast<int>(mid.x * v0[0] + mid.y * v0[1] + mid.z * v0[2] + v0[3]) - surf->texturemins[0];
            const int dt = static_cast<int>(mid.x * v1[0] + mid.y * v1[1] + mid.z * v1[2] + v1[3]) - surf->texturemins[1];
            if(ds >= 0 && dt >= 0 && ds <= surf->extents[0] && dt <= surf->extents[1])
            {
                return surf;
            }
        }
        node = node->children[front >= 0.f];
        return surfaceAlong(m, node, mid, end, depth + 1);
    }
    return nullptr;
}

// The texture's name under the hold at `at` (world) on `ent`'s brush model (nullptr: the world's), "" if none found.
[[nodiscard]] const char* textureUnder(const glm::vec3& at, edict_t* ent)
{
    const int index = ent ? static_cast<int>(ent->v.modelindex) : 1;
    const qmodel_t* m = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    if(!m || m->type != mod_brush || !m->nodes || !m->surfaces)
    {
        return "";
    }
    const glm::vec3 local = ent ? at - glm::vec3{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]} : at;
    const mnode_t* head = m->nodes + m->hulls[0].firstclipnode;
    // Down onto the top (the hold is on it, a little in from the lip); else in towards the face under the lip.
    const msurface_t* s = surfaceAlong(m, head, local + glm::vec3{0.f, 0.f, 4.f}, local - glm::vec3{0.f, 0.f, 12.f}, 0);
    return s ? m->textures[s->texinfo->texnum]->name : "";
}

[[nodiscard]] GrabKind grabKindOf(const char* texture)
{
    switch(debris::materialOf(texture))
    {
    case debris::Material::Wood: return GrabKind::Wood;
    case debris::Material::Metal: return GrabKind::Metal;
    default: return GrabKind::Stone;
    }
}

} // namespace

namespace qvr::physsound
{

Material materialOf(edict_t* ent, const qmodel_t* model)
{
    if(!model)
    {
        return Material::None;
    }
    const char* name = model->name;
    if(model->type == mod_brush)
    {
        // The ammo boxes (shells, nails, rockets, cells) and the health boxes (b_bh10, b_bh25, b_bh100) are metal;
        // the explosive boxes and any other box a map carries (a crate) wood.
        if(!strncmp(name, "maps/b_shell", 12) || !strncmp(name, "maps/b_nail", 11) || !strncmp(name, "maps/b_rock", 11) ||
            !strncmp(name, "maps/b_batt", 11) || !strncmp(name, "maps/b_bh", 9))
        {
            return Material::Metal;
        }
        return Material::Wood;
    }
    if(!strcmp(name, "progs/grenade.mdl") || !strcmp(name, "progs/mervup.mdl"))
    {
        return Material::None; // (QC vr_grenade.qc's bounce: weapons/bounce.wav)
    }
    if(!strncmp(name, "progs/vr_rock", 13))
    {
        return Material::Stone;
    }
    if(!strncmp(name, "progs/vr_brick", 14))
    {
        return Material::Brick;
    }
    if(!strcmp(name, "progs/vrtorch.mdl") || !strncmp(name, "progs/vr_crate", 14) || !strncmp(name, "progs/vr_plank", 14))
    {
        return Material::Wood; // (a wall torch; the crates and what they break into: make_crates.py)
    }
    if(strstr(name, "backpack"))
    {
        return Material::Soft;
    }
    if(!strncmp(name, "progs/gib", 9) || !strncmp(name, "progs/h_", 8) || !strncmp(name, "progs/zom_gib", 13))
    {
        return Material::Flesh;
    }
    const char* cls = ent ? PR_GetString(ent->v.classname) : "";
    const bool gib = ent && (static_cast<int>(ent->v.flags) & physics::FL_FORCEGRABBABLE) && !(static_cast<int>(ent->v.flags) & FL_ITEM);
    const bool weapon = !strcmp(cls, "thrown_weapon") || !strncmp(cls, "weapon_", 7) || !strncmp(cls, "item_key", 8) ||
                        !strncmp(name, "progs/g_", 8) || !strncmp(name, "progs/w_", 8) || !strncmp(name, "progs/v_", 8);
    if(weapon || !gib)
    {
        return Material::Metal; // guns and blades, keys, armour, the flashlight, the other pickups
    }
    return Material::Flesh; // gibs and heads
}

const char* materialName(Material m)
{
    switch(m)
    {
    case Material::Wood: return "wood";
    case Material::Metal: return "metal";
    case Material::Stone: return "stone";
    case Material::Brick: return "brick";
    case Material::Soft: return "soft";
    case Material::Flesh: return "flesh";
    default: return "none";
    }
}

void precache()
{
    state.bodies.clear();
    state.scraping.clear();
    state.hits.clear();
    state.slides.clear();
    for(int m = 0; m < materialCount; m++)
    {
        for(auto& w : state.impacts[m])
        {
            w = Indices{};
        }
        state.scrapes[m] = Indices{};
    }
    for(auto& g : state.grabs)
    {
        g = Indices{};
    }
    if(!progs::bindings().isVrProgs || sv.state != ss_loading)
    {
        return;
    }
    for(int m = 0; m < materialCount; m++)
    {
        for(int w = 0; w < 3; w++)
        {
            precacheSet(impactSets[m][w], state.impacts[m][w]);
        }
        precacheSet(scrapeSets[m], state.scrapes[m]);
    }
    for(int g = 0; g < static_cast<int>(GrabKind::Count); g++)
    {
        precacheSet(grabSets[g], state.grabs[g]);
    }
}

void hit(int num, Material material, float mass, float speed, const glm::vec3& at)
{
    if(material == Material::None || master() <= 0.f || speed < vr_physsound_min_speed.value)
    {
        return;
    }
    for(Hit& h : state.hits)
    {
        if(h.num == num)
        {
            if(speed > h.speed)
            {
                h.speed = speed;
                h.at = at;
            }
            return;
        }
    }
    state.hits.pushBack({num, material, mass, speed, at, 0.f});
}

bool scrapesWanted()
{
    return master() > 0.f && vr_physsound_scrape.value > 0.f;
}

void slide(int num, Material material, float mass, float slip, float press, const glm::vec3& at)
{
    if(material != Material::None)
    {
        state.slides.pushBack({num, material, mass, slip, press, at});
    }
}

void frameEnd()
{
    QVR_PROFILE("physics sounds");
    const double now = qcvm->time;
    const uint32_t frame = state.frames;

    // The hits, the loudest first.
    za::Vector<Hit>& hits = state.hits;
    for(Hit& h : hits)
    {
        h.volume = impactVolume(h.material, h.mass, h.speed);
    }
    za::quickSort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.volume > b.volume || (a.volume == b.volume && a.num < b.num); });
    int played = 0;
    for(const Hit& h : hits)
    {
        if(h.volume <= 0.005f || h.num >= qcvm->num_edicts || EDICT_NUM(h.num)->free)
        {
            continue;
        }
        Body& b = bodyOf(h.num);
        // Too soon after its last knock (unless twice as loud); or, a little later, a bounce's tail (a small hop after a
        // landing: far quieter than the knock).
        const double since = now - b.lastHit;
        const float interval = za::max(vr_physsound_interval.value, 0.f);
        const bool soon = since < interval && h.volume < 2.f * b.lastVolume;
        const bool bounce = since < bounceWindow * interval && h.volume < bounceShare * b.lastVolume;
        if(played >= maxImpactsAFrame || soon || bounce)
        {
            state.skipped++;
            if(debug(2))
            {
                Con_Printf("physsound: %.2f %d %s hit at %.1f m/s skipped (%s)\n", qcvm->time, h.num, materialName(h.material), h.speed,
                    played >= maxImpactsAFrame ? "a frame's most" : soon ? "too soon after its last" : "a bounce after its knock");
            }
            continue;
        }
        const int m = static_cast<int>(h.material);
        const int w = weightOf(h.mass);
        const char* name = "";
        const int index = pick(impactSets[m][w], state.impacts[m][w], &name);
        const float attenuation = h.volume > 0.35f ? 1.f : 1.5f;
        if(!emit(h.num, 0, index, h.volume, attenuation, h.at))
        {
            continue;
        }
        played++;
        state.played++;
        b.lastHit = now;
        b.lastVolume = h.volume;
        if(debug())
        {
            Con_Printf("physsound: %.2f %d %s impact %s (%.1f kg, %s) at %.2f m/s: volume %.2f\n", qcvm->time, h.num,
                PR_GetString(EDICT_NUM(h.num)->v.classname), materialName(h.material), h.mass,
                w == 0 ? "light" : w == 1 ? "medium" : "heavy", h.speed, h.volume);
        }
    }
    hits.clear();

    // The slides: scrapes started and carried on.
    int grains = 0;
    for(const Slide& s : state.slides)
    {
        Body& b = bodyOf(s.num);
        if(b.slideFrame + 1 != frame)
        {
            b.slideSince = now;
        }
        b.slideFrame = frame;
        const float volume = scrapeVolume(s.material, s.mass, s.slip, s.press);
        if(volume <= 0.01f)
        {
            b.slideFrame = 0; // (not sliding enough: as if it didn't)
            continue;
        }
        if(!b.scraping)
        {
            if(now - b.slideSince < scrapeDelay - 1e-4 || volume < scrapeStartVolume)
            {
                continue;
            }
            b.scraping = true;
            b.nextGrain = now;
            b.channel = 0;
            state.scraping.pushBack(s.num);
            if(debug())
            {
                Con_Printf("physsound: %.2f %d %s scrape starts, %s at %.2f m/s (pressed %.2f): volume %.2f\n", qcvm->time, s.num,
                    PR_GetString(EDICT_NUM(s.num)->v.classname), materialName(s.material), s.slip, s.press, volume);
            }
        }
        if(now + 1e-4 >= b.nextGrain && grains < maxGrainsAFrame)
        {
            const int m = static_cast<int>(s.material);
            if(emit(s.num, scrapeChannels[b.channel], pick(scrapeSets[m], state.scrapes[m]), volume, 1.f, s.at))
            {
                grains++;
                b.channel ^= 1;
                b.nextGrain = now + grainLength - grainFade;
                if(debug(2))
                {
                    Con_Printf("physsound: %.2f %d scrape grain at %.2f m/s: volume %.2f\n", qcvm->time, s.num, s.slip, volume);
                }
            }
        }
    }
    state.slides.clear();

    // The scrapes whose prop didn't slide this frame stop.
    za::Vector<int>& still = scratch.stillScraping;
    still.clear();
    for(const int num : state.scraping)
    {
        Body& b = bodyOf(num);
        if(!b.scraping)
        {
            continue;
        }
        if(b.slideFrame != frame || num >= qcvm->num_edicts || EDICT_NUM(num)->free || master() <= 0.f)
        {
            stopScrape(num, b, b.slideFrame != frame ? "not sliding" : "gone");
            continue;
        }
        still.pushBack(num);
    }
    state.scraping.swap(still);
    state.frames++;
}

void grab(edict_t* player, const glm::vec3& at, edict_t* holdEnt)
{
    const float volume = master() * za::max(vr_physsound_grab.value, 0.f);
    if(volume <= 0.f || !sv.active)
    {
        return;
    }
    const char* texture = textureUnder(at, holdEnt);
    const GrabKind kind = grabKindOf(texture);
    const int k = static_cast<int>(kind);
    // From the world, at the hold: heard there (the player's own entity is heard in both ears alike).
    emit(0, 0, pick(grabSets[k], state.grabs[k]), volume, 2.f, at);
    if(debug())
    {
        Con_Printf("physsound: %.2f %d grabs a hold on %s (%s): %s, volume %.2f\n", qcvm->time, player ? NUM_FOR_EDICT(player) : 0,
            texture[0] ? texture : "?", holdEnt ? PR_GetString(holdEnt->v.classname) : "the world",
            kind == GrabKind::Wood ? "wood" : kind == GrabKind::Metal ? "metal" : "stone", volume);
    }
}

} // namespace qvr::physsound
