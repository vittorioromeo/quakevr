// vr_decals.cpp -- see vr_decals.hpp.

#include "vr_decals.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_profile.hpp"
#include "vr_trace.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_map>
#include <vector>

namespace qvr::decals
{
namespace
{

enum Kind : int
{
    Blood,
    BloodDrop,
    Scorch,
    Hole
};

// The atlas: 4 x 4 cells of 64 texels. Blood splats 0-5, drops 6-7, scorches 8-10, chips 11-13.
constexpr int cellSize = 64;
constexpr int cellsPerRow = 4;
constexpr int atlasSize = cellSize * cellsPerRow;
constexpr int firstCell[] = {0, 6, 8, 11};
constexpr int cellCount[] = {6, 2, 3, 3};

gfx::Texture atlas = 0;

struct Decal
{
    glm::vec3 centre, u, v; // u, v: half extents on the surface
    int cell;
    double born;
};

std::deque<Decal> decals;
std::vector<gfx::Vertex> vertices;
int builtFrame = -1; // the host frame `vertices` were built in; -1 when decals came or went since
int addedThisFrame = 0;
int addedFrame = -1;

// ---- The atlas ---------------------------------------------------------------------------------

unsigned rngState = 12345u;
[[nodiscard]] float frand()
{
    rngState = rngState * 1664525u + 1013904223u;
    return static_cast<float>(rngState >> 8) / 16777216.f;
}

[[nodiscard]] float hash(int i, int seed)
{
    unsigned h = static_cast<unsigned>(i) * 374761393u + static_cast<unsigned>(seed) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<float>(h & 0xFFFF) / 65535.f;
}

// Smooth periodic noise round a circle (`a` in turns), 0..1.
[[nodiscard]] float ring(float a, int lobes, int seed)
{
    const float x = a * lobes;
    const int i = static_cast<int>(std::floor(x));
    const float f = x - i;
    const float s = f * f * (3.f - 2.f * f);
    return hash(((i % lobes) + lobes) % lobes, seed) * (1.f - s) + hash((((i + 1) % lobes) + lobes) % lobes, seed) * s;
}

[[nodiscard]] float smoothstep(float e0, float e1, float x)
{
    const float t = std::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

struct Blob
{
    glm::vec2 c;
    float r;
    glm::vec2 stretch; // elongation direction times its amount
};

// Coverage of a cell texel at `p` (-1..1): what `kind` variant `seed` covers, and its colour (what
// the surface is multiplied by).
void cellTexel(Kind kind, int seed, const std::vector<Blob>& blobs, glm::vec2 p, float& alpha, glm::vec3& color)
{
    const float r = glm::length(p);
    const float a = std::atan2(p.y, p.x) / 6.2831853f + 0.5f;
    switch(kind)
    {
        case Blood:
        case BloodDrop:
        {
            float cover = 0.f;
            if(kind == Blood)
            {
                const float edge = 0.38f + 0.16f * ring(a, 7, seed) + 0.06f * ring(a, 19, seed + 5);
                cover = smoothstep(edge + 0.03f, edge - 0.03f, r);
            }
            for(const Blob& b : blobs)
            {
                glm::vec2 d = p - b.c;
                const glm::vec2 axis = glm::normalize(b.stretch + glm::vec2{1e-4f});
                const float stretch = glm::length(b.stretch);
                d -= axis * glm::dot(d, axis) * (stretch / (1.f + stretch));
                cover = std::max(cover, smoothstep(b.r + 0.02f, b.r - 0.02f, glm::length(d)));
            }
            alpha = cover * (0.82f + 0.18f * hash(static_cast<int>((p.x + 1.f) * 40.f) * 97 + static_cast<int>((p.y + 1.f) * 40.f), seed));
            // Darker, thicker in the middle.
            color = glm::mix(glm::vec3{0.18f, 0.01f, 0.01f}, glm::vec3{0.42f, 0.04f, 0.03f}, smoothstep(0.f, 0.7f, r));
            break;
        }
        case Scorch:
        {
            const float edge = 0.75f + 0.2f * ring(a, 9, seed);
            alpha = smoothstep(edge, 0.15f, r) * (0.65f + 0.35f * ring(a, 23, seed + 3)) * 0.95f;
            color = glm::vec3{0.1f, 0.085f, 0.07f};
            break;
        }
        case Hole:
        {
            if(r < 0.16f)
            {
                alpha = 1.f;
                color = glm::vec3{0.06f};
            }
            else
            {
                const float edge = 0.32f + 0.12f * ring(a, 6, seed);
                alpha = smoothstep(edge, 0.16f, r) * 0.7f;
                color = glm::vec3{0.45f, 0.42f, 0.4f};
            }
            break;
        }
    }
}

void makeAtlas()
{
    std::vector<unsigned char> rgba(atlasSize * atlasSize * 4, 0);
    for(int kind = 0; kind < 4; kind++)
    {
        for(int n = 0; n < cellCount[kind]; n++)
        {
            const int cell = firstCell[kind] + n;
            const int seed = cell * 131 + 7;
            rngState = static_cast<unsigned>(seed);

            // Blood's satellite droplets and streaks (a drop cell: a few drops only).
            std::vector<Blob> blobs;
            const int count = kind == Blood ? 9 + static_cast<int>(frand() * 6) : kind == BloodDrop ? 4 : 0;
            for(int i = 0; i < count; i++)
            {
                const float ang = frand() * 6.2831853f;
                const float dist = kind == Blood ? 0.45f + frand() * 0.45f : frand() * 0.55f;
                const glm::vec2 dir{std::cos(ang), std::sin(ang)};
                const float streak = kind == Blood && frand() < 0.4f ? 1.f + frand() * 3.f : 0.f;
                blobs.push_back({dir * dist, (kind == Blood ? 0.03f : 0.08f) + frand() * (kind == Blood ? 0.06f : 0.12f),
                    dir * streak});
            }

            const int ox = (cell % cellsPerRow) * cellSize, oy = (cell / cellsPerRow) * cellSize;
            for(int y = 0; y < cellSize; y++)
            {
                for(int x = 0; x < cellSize; x++)
                {
                    const glm::vec2 p{(x + 0.5f) / cellSize * 2.f - 1.f, (y + 0.5f) / cellSize * 2.f - 1.f};
                    float alpha = 0.f;
                    glm::vec3 color{1.f};
                    cellTexel(static_cast<Kind>(kind), seed, blobs, p, alpha, color);
                    // Clear the cell's border: bilinear filtering must not reach the next cell.
                    if(x < 2 || y < 2 || x >= cellSize - 2 || y >= cellSize - 2)
                    {
                        alpha = 0.f;
                    }
                    unsigned char* out = &rgba[((oy + y) * atlasSize + ox + x) * 4];
                    // Premultiplied: the modulating blend is dst * (colour * alpha + 1 - alpha).
                    out[0] = static_cast<unsigned char>(std::clamp(color.r * alpha, 0.f, 1.f) * 255.f + 0.5f);
                    out[1] = static_cast<unsigned char>(std::clamp(color.g * alpha, 0.f, 1.f) * 255.f + 0.5f);
                    out[2] = static_cast<unsigned char>(std::clamp(color.b * alpha, 0.f, 1.f) * 255.f + 0.5f);
                    out[3] = static_cast<unsigned char>(std::clamp(alpha, 0.f, 1.f) * 255.f + 0.5f);
                }
            }
        }
    }
    atlas = gfx::createTexture(atlasSize, atlasSize, rgba.data());
}

// ---- Placing ----------------------------------------------------------------------------------

// The static world along `from` -> `to`: where, and its normal.
[[nodiscard]] bool hitWorld(const glm::vec3& from, const glm::vec3& to, glm::vec3& where, glm::vec3& normal, float& fraction)
{
    const trace_t tr = worldtrace::world(from, to, false);
    if(tr.fraction >= 1.f || tr.startsolid || tr.allsolid)
    {
        return false;
    }
    where = worldtrace::endPos(tr);
    normal = worldtrace::normal(tr);
    fraction = tr.fraction;
    return true;
}

// The nearest surface within `reach` of `org` (along `prefer` first, then down, then round).
[[nodiscard]] bool nearest(const glm::vec3& org, float reach, const glm::vec3& prefer, glm::vec3& where, glm::vec3& normal)
{
    const glm::vec3 dirs[] = {prefer, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}};
    float best = 2.f;
    for(const glm::vec3& d : dirs)
    {
        if(glm::dot(d, d) < 0.5f)
        {
            continue;
        }
        glm::vec3 w, n;
        float f;
        if(hitWorld(org, org + glm::normalize(d) * reach, w, n, f) && f < best)
        {
            best = f;
            where = w;
            normal = n;
        }
    }
    return best <= 1.f;
}

// Whether a surface with normal `n` lies under `p` (the decal's corner does not hang in the air).
[[nodiscard]] bool supported(const glm::vec3& p, const glm::vec3& n)
{
    glm::vec3 w, hn;
    float f;
    return hitWorld(p + n * 2.f, p - n * 2.f, w, hn, f) && glm::dot(hn, n) > 0.9f;
}

void add(Kind kind, const glm::vec3& where, const glm::vec3& normal, float size)
{
    const int max = static_cast<int>(vr_decal_max.value);
    if(!vr_decals.value || max <= 0 || !cl.worldmodel)
    {
        return;
    }
    if(addedFrame != host_framecount)
    {
        addedFrame = host_framecount;
        addedThisFrame = 0;
    }
    if(++addedThisFrame > 24)
    {
        return; // a shotgun blast into a wall is enough at once
    }

    // Not the same mark twice in one place (Quake's effect and Quake VR's for one hit).
    for(auto it = decals.rbegin(); it != decals.rend() && it - decals.rbegin() < 16; ++it)
    {
        if(cl.time - it->born < 0.1 && glm::distance(it->centre, where) < 2.f)
        {
            return;
        }
    }

    const glm::vec3 n = glm::normalize(normal);
    const glm::vec3 t0 = glm::normalize(glm::cross(n, std::fabs(n.z) < 0.9f ? glm::vec3{0, 0, 1} : glm::vec3{1, 0, 0}));
    const glm::vec3 t1 = glm::cross(n, t0);
    const float ang = static_cast<float>(rand() % 6283) * 0.001f;
    glm::vec3 u = (t0 * std::cos(ang) + t1 * std::sin(ang));
    glm::vec3 v = glm::cross(n, u);
    const glm::vec3 centre = where + n * 0.2f;

    // Shrunk (once) or dropped where it would hang over an edge.
    for(int attempt = 0; attempt < 2; attempt++)
    {
        const float half = size * 0.5f;
        bool ok = true;
        for(const glm::vec2 c : {glm::vec2{-1, -1}, glm::vec2{1, -1}, glm::vec2{1, 1}, glm::vec2{-1, 1}})
        {
            if(!supported(where + (u * c.x + v * c.y) * (half * 0.8f), n))
            {
                ok = false;
                break;
            }
        }
        if(ok)
        {
            const int cell = firstCell[kind] + rand() % cellCount[kind];
            decals.push_back({centre, u * half, v * half, cell, cl.time});
            while(static_cast<int>(decals.size()) > max)
            {
                decals.pop_front();
            }
            builtFrame = -1;
            return;
        }
        size *= 0.55f;
        if(size < 2.f)
        {
            return;
        }
    }
}

[[nodiscard]] float random(float lo, float hi)
{
    return lo + (hi - lo) * static_cast<float>(rand() % 1000) * 0.001f;
}

// ---- Gibs' blood (VR_GibTrail) ----------------------------------------------------------------

// What a gib (or head) has done so far, by entity number.
struct Gib
{
    const qmodel_t* model = nullptr;
    double seen = 0.0;       // cl.time it was last drawn,
    glm::vec3 origin{0.f};   // and where
    double msgTime = 0.0;    // the server message `velocity` is from
    glm::vec3 velocity{0.f}; // between its last two positions from the server
    bool moving = false;     // `velocity` is known
    float sinceTrail = 0.f;  // units travelled since the last trail particles,
    float sinceDrop = 0.f;   // and since the last drop on the floor
    int dropsLeft = 0;       // a gib bleeds dry
    double splatTime = 0.0;  // its last splat
};

std::unordered_map<int, Gib> gibs;
int gibsPrunedFrame = 0;

constexpr int gibDrops = 64;           // drops a gib leaves in all
constexpr float gibTrailSpacing = 5.f; // units between trail particles (vr_gib_blood_trail 1)
constexpr float gibDropSpacing = 20.f; // and between drops
constexpr float gibSplatSpeed = 150.f; // units/s it must go at, and its velocity turn, to splat

// Calls `f` every `spacing` units along `from` -> `to` (at most `max` times), `since` carrying the
// distance travelled since the last one over from call to call.
template <typename F>
void along(const glm::vec3& from, const glm::vec3& to, float spacing, float& since, int max, F&& f)
{
    const glm::vec3 d = to - from;
    const float len = glm::length(d);
    float at = std::max(0.f, spacing - since); // how far along the next one is
    int n = 0;
    while(at <= len && n < max)
    {
        f(from + d * (len > 0.f ? at / len : 0.f));
        at += spacing;
        n++;
    }
    since = n == max ? 0.f : len - (at - spacing);
}

// A drop on the floor under a gib at `org` (its blood falls straight down).
void drip(const glm::vec3& org)
{
    const glm::vec3 o = org + glm::vec3{random(-2.f, 2.f), random(-2.f, 2.f), 0.f};
    glm::vec3 where, normal;
    float f;
    if(hitWorld(o, o - glm::vec3{0, 0, 128}, where, normal, f) && normal.z > 0.6f)
    {
        // Low over the floor (sliding, rolling, carried low) a smear; from higher, smaller drops. As
        // big as Quake's gibs call for (a gib is 10-30 units across).
        add(BloodDrop, where, normal, f * 128.f < 16.f ? random(12.f, 20.f) : random(8.f, 15.f));
    }
}

// A gib struck the world between `from` and `to` (its last two positions from the server), going
// at `oldVelocity`, turned by `change` (beyond gravity's): a splat where it hit, a spurt of blood.
void splat(const glm::vec3& from, const glm::vec3& to, const glm::vec3& oldVelocity, const glm::vec3& change)
{
    const float strength = glm::length(change);
    const float reach = glm::distance(from, to) + 16.f; // a gib's box is about this big
    glm::vec3 where, normal;
    float f;
    // The surface pushed it back along its normal, so it lies the other way (else ahead of it).
    if(!hitWorld(from, from - change / strength * reach, where, normal, f) &&
       !hitWorld(from, from + glm::normalize(oldVelocity) * reach, where, normal, f))
    {
        return;
    }
    add(Blood, where, normal, std::clamp(16.f + strength * 0.03f, 16.f, 40.f));

    // Quake VR's blood (Quake's own with vr_particles 0).
    vec3_t org, dir;
    for(int i = 0; i < 3; i++)
    {
        org[i] = where[i] + normal[i] * 2.f;
        dir[i] = normal[i] * 2.f;
    }
    R_RunParticleEffect(org, dir, 73, strength > 500.f ? 24 : 12);
}

} // namespace

void drop(const glm::vec3& org, float size)
{
    glm::vec3 where, normal;
    float f;
    if(vr_decals.value && cl.worldmodel &&
        hitWorld(org + glm::vec3{0, 0, 2}, org - glm::vec3{0, 0, 16}, where, normal, f) && normal.z > 0.6f)
    {
        add(BloodDrop, where, normal, size);
    }
}

void fromEffect(const glm::vec3& org, const glm::vec3& dir, particles::Preset preset, int count)
{
    using particles::Preset;
    if(!vr_decals.value || !cl.worldmodel)
    {
        return;
    }
    glm::vec3 where, normal;
    float f;
    const glm::vec3 back = glm::dot(dir, dir) > 1e-4f ? -glm::normalize(dir) : glm::vec3{0.f};
    switch(preset)
    {
        case Preset::Blood: // a pool on the floor below, spatter on a wall near by
        {
            const float size = std::clamp(8.f + count * 0.4f, 8.f, 28.f);
            if(hitWorld(org, org - glm::vec3{0, 0, 160}, where, normal, f) && normal.z > 0.6f)
            {
                add(Blood, where, normal, size * (0.8f + f * 0.6f)); // the farther it falls, the wider it spreads
            }
            const float a = random(0.f, 6.2831853f);
            const glm::vec3 side = glm::dot(dir, dir) > 1e-4f ? glm::normalize(dir) : glm::vec3{std::cos(a), std::sin(a), random(-0.3f, 0.2f)};
            if(hitWorld(org, org + glm::normalize(side) * 72.f, where, normal, f))
            {
                add(Blood, where, normal, size * 0.7f);
            }
            break;
        }
        case Preset::BulletPuff: // a chip where it hit
        case Preset::Sparks:     // a melee blow on a wall
            if(nearest(org, 6.f, back, where, normal))
            {
                add(Hole, where, normal, preset == Preset::BulletPuff ? random(3.f, 4.5f) : random(4.f, 6.f));
            }
            break;
        case Preset::Explosion:
            if(nearest(org, 64.f, glm::vec3{0, 0, -1}, where, normal))
            {
                add(Scorch, where, normal, random(44.f, 60.f));
            }
            break;
        case Preset::Lightning:
        case Preset::LavaSpike:
            if(nearest(org, 10.f, back, where, normal))
            {
                add(Scorch, where, normal, random(8.f, 14.f));
            }
            break;
        default: break;
    }
}

void draw()
{
    QVR_GPU_PROFILE("decals");
    if(decals.empty() || !vr_decals.value)
    {
        return;
    }
    if(!atlas)
    {
        makeAtlas();
    }

    // Built once a frame (and again when marks come or go), for both eyes.
    if(builtFrame != host_framecount)
    {
        builtFrame = host_framecount;

        // Faded out over their last five seconds.
        const double life = std::max(5.f, vr_decal_life.value);
        while(!decals.empty() && cl.time - decals.front().born > life)
        {
            decals.pop_front();
        }

        vertices.clear();
        constexpr float inset = 0.5f / atlasSize;
        for(const Decal& d : decals)
        {
            const float age = static_cast<float>(cl.time - d.born);
            const float a = std::clamp(static_cast<float>((life - age) / 5.0), 0.f, 1.f);
            const glm::vec4 c{a, a, a, a};
            const float u0 = static_cast<float>(d.cell % cellsPerRow) / cellsPerRow + inset;
            const float v0 = static_cast<float>(d.cell / cellsPerRow) / cellsPerRow + inset;
            const float u1 = u0 + 1.f / cellsPerRow - 2.f * inset, v1 = v0 + 1.f / cellsPerRow - 2.f * inset;
            const gfx::Vertex q[4] = {{d.centre - d.u - d.v, {u0, v0}, c}, {d.centre + d.u - d.v, {u1, v0}, c},
                {d.centre + d.u + d.v, {u1, v1}, c}, {d.centre - d.u + d.v, {u0, v1}, c}};
            for(int i : {0, 1, 2, 0, 2, 3})
            {
                vertices.push_back(q[i]);
            }
        }
    }
    gfx::draw(vertices, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::Texture, .blend = gfx::Blend::Modulate, .depthTest = true, .depthWrite = false}, atlas);
}

void count_f()
{
    int kinds[4]{};
    for(const Decal& d : decals)
    {
        const Kind kind = d.cell >= firstCell[Hole]        ? Hole
                          : d.cell >= firstCell[Scorch]    ? Scorch
                          : d.cell >= firstCell[BloodDrop] ? BloodDrop
                                                           : Blood;
        kinds[kind]++;
    }
    Con_Printf("%d decals: %d blood, %d drops, %d scorch, %d chips\n", static_cast<int>(decals.size()), kinds[0], kinds[1],
        kinds[2], kinds[3]);
}

void clear()
{
    decals.clear();
    builtFrame = -1;
    gibs.clear();
}

} // namespace qvr::decals

// cl_tent.c: Quake's own wall hits and explosions.
extern "C" void VR_DecalTempEntity(int scorch, const float* pos)
{
    using qvr::particles::Preset;
    qvr::decals::fromEffect(
        {pos[0], pos[1], pos[2]}, glm::vec3{0.f}, scorch ? Preset::Explosion : Preset::BulletPuff, 1);
}

// CL_RelinkEntities, every frame for each gib and head (flying, sliding, lying, carried): Quake VR's
// blood trail behind it while it moves, drops on the floor under it (a trail that stays), and a
// splat and a spurt of blood where it hits a wall or the floor. A hit is seen in the positions the
// server sends: the velocity between the last two turning more than gravity turns it. Nonzero if
// Quake VR's particles drew the trail (Quake's is not drawn then).
extern "C" int VR_GibTrail(int ent, int zombie)
{
    using namespace qvr;
    using namespace qvr::decals;
    if(!vr_gib_blood.value || ent <= 0 || ent >= cl.num_entities || !cl.worldmodel)
    {
        return 0;
    }
    const entity_t& e = cl_entities[ent];
    const glm::vec3 o{e.origin[0], e.origin[1], e.origin[2]};

    // Forgotten when not seen for a second.
    if(host_framecount - gibsPrunedFrame > 100 || host_framecount < gibsPrunedFrame)
    {
        gibsPrunedFrame = host_framecount;
        std::erase_if(gibs, [](const auto& kv) { return cl.time - kv.second.seen > 1.0 || cl.time < kv.second.seen; });
    }

    // A new one (or a new gib in the slot, or a teleport): from here.
    Gib& g = gibs[ent];
    if(g.model != e.model || cl.time - g.seen > 0.25 || cl.time < g.seen || glm::distance(g.origin, o) > 128.f)
    {
        g = Gib{};
        g.model = e.model;
        g.origin = o;
        g.msgTime = e.msgtime;
        g.dropsLeft = gibDrops;
    }

    // A new position from the server: its velocity since the one before, and whether the world
    // turned it (a bounce, a landing, a wall).
    const double dt = cl.mtime[0] - cl.mtime[1];
    if(e.msgtime != g.msgTime && dt > 0.001)
    {
        g.msgTime = e.msgtime;
        const glm::vec3 p0{e.msg_origins[0][0], e.msg_origins[0][1], e.msg_origins[0][2]};
        const glm::vec3 p1{e.msg_origins[1][0], e.msg_origins[1][1], e.msg_origins[1][2]};
        const glm::vec3 v = (p0 - p1) / static_cast<float>(dt);
        if(g.moving && glm::length(g.velocity) > gibSplatSpeed && cl.time - g.splatTime > 0.15)
        {
            const glm::vec3 change = v - (g.velocity - glm::vec3{0.f, 0.f, sv_gravity.value * static_cast<float>(dt)});
            if(glm::length(change) > gibSplatSpeed)
            {
                g.splatTime = cl.time;
                splat(p1, p0, g.velocity, change);
            }
        }
        g.velocity = v;
        g.moving = true;
    }

    // The trail, where it went since the last frame (a zombie's gibs bleed less).
    const float density = std::clamp(vr_gib_blood_trail.value, 0.f, 4.f) * (zombie ? 0.5f : 1.f);
    const bool ours = particles::enabled();
    const float travelled = glm::distance(g.origin, o);
    if(density > 0.f && travelled > 0.01f)
    {
        const glm::vec3 dir = (o - g.origin) / travelled;
        if(ours)
        {
            along(g.origin, o, gibTrailSpacing / density, g.sinceTrail, 24,
                [&](const glm::vec3& p) { particles::spawn(p, dir, particles::Preset::BloodTrail, 1); });
        }
        if(vr_decals.value && g.dropsLeft > 0)
        {
            along(g.origin, o, gibDropSpacing / density, g.sinceDrop, std::min(4, g.dropsLeft),
                [&](const glm::vec3& p) {
                    drip(p);
                    g.dropsLeft--;
                });
        }
    }
    g.origin = o;
    g.seen = cl.time;
    return ours;
}
