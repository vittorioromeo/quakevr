// vr_debris.cpp -- rocks and bricks lying about the maps; see vr_debris.hpp and docs/vr-port/ROUND21.md, "Rocks and
// bricks". The models are Misc/quakevr/make_debris.py's; QC vr_debris.qc spawns and handles them.

#include "vr_debris.hpp"

#include "vr_cvars.hpp"
#include "vr_held.hpp"
#include "vr_progs.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace qvr::debris
{
namespace
{

double spawnStart = 0.0; // (vr_debug_debris: the map's load time)
double planMs = 0.0;

using progs::fields;

// ---------------------------------------------------------------------------------------------------------------------
// Materials by texture name

struct Rule
{
    const char* pattern;
    Material material;
    bool prefix; // else anywhere in the name
};

// First match wins. Names are compared lowercased, without an animation's "+0"/"+a" or a fence's "{".
constexpr Rule rules[] = {
    {"sky", Material::None, true},
    {"trigger", Material::None, true},
    {"clip", Material::None, true},
    {"skip", Material::None, true},
    {"hint", Material::None, true},
    {"origin", Material::None, true},
    // Brick before anything else that a brick texture's name may hold (a "wbrick", a "bricka").
    {"brick", Material::Brick, false},
    {"city2_", Material::Brick, true}, // id's city2_1..8: brick walls, red, yellow, blue, sooty
    {"city1_4", Material::Brick, true},
    // Rough stone walls: rocks come off them.
    {"wall14", Material::Fieldstone, true},
    {"church1_2", Material::Fieldstone, true},
    {"city6_7", Material::Fieldstone, true},
    {"city6_8", Material::Fieldstone, true},
    {"cobble", Material::Fieldstone, false},
    {"rubble", Material::Fieldstone, false},
    // The ground.
    {"grass", Material::Natural, false},
    {"ground", Material::Natural, false},
    {"wgrnd", Material::Natural, true},
    {"dirt", Material::Natural, false},
    {"mud", Material::Natural, false},
    {"sand", Material::Natural, false},
    {"gravel", Material::Natural, false},
    {"moss", Material::Natural, false},
    {"cliff", Material::Natural, false},
    {"earth", Material::Natural, false},
    {"wswamp1", Material::Natural, true}, // roots in the earth
    // ("rock" followed by digits and "_" or the end: rock1_2, rock4_1; not rock0sid or rockettop, which are machines)
    // Dressed stone.
    {"wswamp2", Material::Masonry, true},
    {"wiz1_", Material::Masonry, true},
    {"stone", Material::Masonry, false},
    {"city4_", Material::Masonry, true},
    {"city3_", Material::Masonry, true},
    {"city5_", Material::Masonry, true},
    {"afloor", Material::Masonry, true},
    {"wall9", Material::Masonry, true},
    {"church", Material::Masonry, true},
    {"column", Material::Masonry, true},
    {"arch", Material::Masonry, true},
    {"altar", Material::Masonry, true},
    {"marble", Material::Masonry, false},
    // Nothing comes off these.
    {"wood", Material::Wood, false},
    {"crate", Material::Wood, true},
    {"metal", Material::Metal, false},
    {"met", Material::Metal, false},
    {"tech", Material::Metal, false},
    {"comp", Material::Metal, false},
    {"cop", Material::Metal, false},
    {"plat", Material::Metal, false},
    {"light", Material::Metal, false},
    {"door", Material::Metal, false},
    {"exit", Material::Metal, false},
    {"switch", Material::Metal, false},
    {"button", Material::Metal, false},
    {"twall", Material::Metal, true},
    {"uwall", Material::Metal, true},
    {"sfloor", Material::Metal, true},
    {"slip", Material::Metal, true},
    {"window", Material::Other, false},
};

[[nodiscard]] std::string cleanName(const char* texture)
{
    std::string s = texture ? texture : "";
    if(!s.empty() && (s[0] == '+' || s[0] == '-') && s.size() > 2)
    {
        s = s.substr(2); // an animation's frame: "+0lava"
    }
    if(!s.empty() && s[0] == '{')
    {
        s = s.substr(1);
    }
    for(char& c : s)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// ---------------------------------------------------------------------------------------------------------------------
// The models: their size and their skins' colours, from the files

struct ModelInfo
{
    const char* name;
    int kind;      // 1 rock, 2 brick
    float weight;  // how often it is picked among its kind
    bool loaded{false};
    glm::vec3 lo{0.f}, hi{0.f};       // frame 0's box, model units
    std::vector<glm::vec3> skinLab;   // each skin's average colour (OKLab)
};

ModelInfo models[] = {
    {"progs/vr_rock1.mdl", 1, 1.f},
    {"progs/vr_rock2.mdl", 1, 1.f},
    {"progs/vr_rock3.mdl", 1, 1.f},
    {"progs/vr_rock4.mdl", 1, 1.f},
    {"progs/vr_rock5.mdl", 1, 1.f},
    {"progs/vr_brick1.mdl", 2, 0.3f},  // whole
    {"progs/vr_brick2.mdl", 2, 0.25f}, // whole, chipped
    {"progs/vr_brick3.mdl", 2, 0.25f}, // a half
    {"progs/vr_brick4.mdl", 2, 0.2f},  // a broken piece
};
constexpr int numModels = static_cast<int>(std::size(models));

[[nodiscard]] glm::vec3 paletteRgb(int index)
{
    const unsigned c = d_8to24table[index & 255];
    return {static_cast<float>(c & 255u), static_cast<float>((c >> 8) & 255u), static_cast<float>((c >> 16) & 255u)};
}

// sRGB (0..255) to OKLab: distances there follow what the eye sees.
[[nodiscard]] glm::vec3 okLab(const glm::vec3& rgb)
{
    const auto lin = [](float c) {
        c /= 255.f;
        return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
    };
    const float r = lin(rgb.r), g = lin(rgb.g), b = lin(rgb.b);
    const float l = std::cbrt(0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b);
    const float m = std::cbrt(0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b);
    const float s = std::cbrt(0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b);
    return {0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s, 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s,
        0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s};
}

[[nodiscard]] int32_t readInt(const byte* p)
{
    int32_t v;
    memcpy(&v, p, 4);
    return v;
}

[[nodiscard]] float readFloat(const byte* p)
{
    float v;
    memcpy(&v, p, 4);
    return v;
}

// Reads a model's box and skin colours from its file (an .mdl: its header, skins and first frame).
void loadModel(ModelInfo& m)
{
    m.loaded = true;
    m.skinLab.clear();
    unsigned int pathId = 0;
    byte* data = COM_LoadMallocFile(m.name, &pathId);
    if(!data)
    {
        Con_DPrintf("debris: no %s\n", m.name);
        return;
    }
    const int size = com_filesize;
    const auto fail = [&](const char* why) {
        Con_Printf("debris: %s: %s\n", m.name, why);
        m.skinLab.clear();
        free(data);
    };
    if(size < 84 || memcmp(data, "IDPO", 4) != 0 || readInt(data + 4) != 6)
    {
        return fail("not an alias model");
    }
    const glm::vec3 scale{readFloat(data + 8), readFloat(data + 12), readFloat(data + 16)};
    const glm::vec3 origin{readFloat(data + 20), readFloat(data + 24), readFloat(data + 28)};
    const int numSkins = readInt(data + 48), w = readInt(data + 52), h = readInt(data + 56);
    const int numVerts = readInt(data + 60), numTris = readInt(data + 64);
    int off = 84;
    for(int k = 0; k < numSkins; k++)
    {
        if(off + 4 > size)
        {
            return fail("cut short");
        }
        int count = 1;
        const bool group = readInt(data + off) != 0;
        off += 4;
        if(group)
        {
            count = readInt(data + off);
            off += 4 + 4 * count;
        }
        if(off + w * h * count > size)
        {
            return fail("cut short");
        }
        glm::vec3 sum{0.f};
        for(int i = 0; i < w * h; i++)
        {
            sum += paletteRgb(data[off + i]);
        }
        m.skinLab.push_back(okLab(sum / static_cast<float>(std::max(w * h, 1))));
        off += w * h * count;
    }
    off += 12 * numVerts + 16 * numTris;
    if(off + 4 + 24 + 4 * numVerts > size || readInt(data + off) != 0)
    {
        return fail("no first frame");
    }
    const byte* v = data + off + 4 + 24;
    m.lo = glm::vec3{1e9f};
    m.hi = glm::vec3{-1e9f};
    for(int i = 0; i < numVerts; i++)
    {
        const glm::vec3 p = origin + scale * glm::vec3{v[4 * i], v[4 * i + 1], v[4 * i + 2]};
        m.lo = glm::min(m.lo, p);
        m.hi = glm::max(m.hi, p);
    }
    free(data);
}

// ---------------------------------------------------------------------------------------------------------------------
// Random numbers: a seed from the map's name, the same sequence at every load

struct Rng
{
    uint64_t s;
    uint64_t next()
    {
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    float uniform() { return static_cast<float>(next() >> 40) / static_cast<float>(1ull << 24); }
    float range(float a, float b) { return a + (b - a) * uniform(); }
};

[[nodiscard]] uint64_t hashString(const char* s, uint64_t h = 1469598103934665603ull)
{
    for(; *s; s++)
    {
        h = (h ^ static_cast<unsigned char>(*s)) * 1099511628211ull;
    }
    return h;
}

// ---------------------------------------------------------------------------------------------------------------------
// The map's faces

struct MapFace
{
    glm::vec3 normal;
    float dist;
    std::vector<glm::vec3> pts;
    Material material;
    const texture_t* texture;
    int surf; // its msurface_t in the world model
};

// The map's static entities (makestatic: wall torches, flames), which leave no entity to keep clear of (VR_OnMakeStatic).
struct StaticThing
{
    glm::vec3 lo, hi;
    std::string classname;
};
std::vector<StaticThing> statics;

// How lit the floor at `p` on surface `surf` is: its lightmap there (all its styles at their normal brightness), 0 to
// 255 (Quake's normal full light is about 128 in the models' terms, a bright spot 200 and more). 255 for a map without
// light (drawn fullbright). The map's own data: the same at every load, whatever the client has.
[[nodiscard]] float floorLight(const qmodel_t* map, int surf, const glm::vec3& p)
{
    if(!map->lightdata)
    {
        return 255.f;
    }
    const msurface_t& sf = map->surfaces[surf];
    if(!sf.samples || (sf.flags & SURF_DRAWTILED))
    {
        return 0.f;
    }
    const float* v0 = sf.texinfo->vecs[0];
    const float* v1 = sf.texinfo->vecs[1];
    const int ds = std::clamp(static_cast<int>(p.x * v0[0] + p.y * v0[1] + p.z * v0[2] + v0[3]) - sf.texturemins[0], 0,
        static_cast<int>(sf.extents[0]));
    const int dt = std::clamp(static_cast<int>(p.x * v1[0] + p.y * v1[1] + p.z * v1[2] + v1[3]) - sf.texturemins[1], 0,
        static_cast<int>(sf.extents[1]));
    const int smax = (sf.extents[0] >> 4) + 1, tmax = (sf.extents[1] >> 4) + 1;
    const byte* lm = sf.samples + ((dt >> 4) * smax + (ds >> 4)) * 3;
    float sum = 0.f;
    for(int k = 0; k < MAXLIGHTMAPS && sf.styles[k] != 255; k++, lm += smax * tmax * 3)
    {
        sum += (static_cast<float>(lm[0]) + lm[1] + lm[2]) / 3.f;
    }
    return std::min(sum, 255.f);
}

struct Placement
{
    int model;
    int skin;
    glm::vec3 floor;  // under its middle
    glm::vec3 normal; // the floor's
    float yaw;        // degrees
    bool onSide;      // a brick lying on its side
    glm::vec3 scale;  // per axis: its size times vr_world_scale
    glm::vec2 out;    // the way out of the wall it lies by (vr_debug_debris)
};

std::vector<Placement> placements;

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return {v[0], v[1], v[2]};
}

[[nodiscard]] trace_t traceLine(const glm::vec3& a, const glm::vec3& b)
{
    vec3_t start{a.x, a.y, a.z}, end{b.x, b.y, b.z};
    return SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NOMONSTERS, qcvm->edicts);
}

[[nodiscard]] int contents(const glm::vec3& p)
{
    vec3_t v{p.x, p.y, p.z};
    return SV_PointContents(v);
}

// A texture's average colour (OKLab), from its pixels (the BSP's own: QRP's replacements keep their hue).
[[nodiscard]] glm::vec3 textureLab(const texture_t* t)
{
    static std::unordered_map<const texture_t*, glm::vec3> cache;
    static const qmodel_t* cacheMap = nullptr;
    if(cacheMap != sv.worldmodel)
    {
        cache.clear();
        cacheMap = sv.worldmodel;
    }
    if(!t)
    {
        return glm::vec3{0.4f, 0.f, 0.f};
    }
    if(auto it = cache.find(t); it != cache.end())
    {
        return it->second;
    }
    glm::vec3 sum{0.f};
    const unsigned n = t->width * t->height;
    const byte* px = reinterpret_cast<const byte*>(t + 1);
    const unsigned step = std::max(1u, n / 4096u);
    unsigned count = 0;
    for(unsigned i = 0; i < n; i += step, count++)
    {
        sum += paletteRgb(px[i]);
    }
    const glm::vec3 lab = okLab(count ? sum / static_cast<float>(count) : glm::vec3{80.f});
    cache.emplace(t, lab);
    return lab;
}

// Up-facing faces in a grid of cells, to find the one under a point (its texture).
struct FloorGrid
{
    static constexpr float cell = 128.f;
    std::unordered_map<int64_t, std::vector<int>> cells;

    [[nodiscard]] static int64_t key(int x, int y) { return (static_cast<int64_t>(x) << 32) ^ static_cast<uint32_t>(y); }

    void add(int index, const MapFace& f)
    {
        glm::vec2 lo{1e9f}, hi{-1e9f};
        for(const glm::vec3& p : f.pts)
        {
            lo = glm::min(lo, glm::vec2{p});
            hi = glm::max(hi, glm::vec2{p});
        }
        for(int x = static_cast<int>(std::floor(lo.x / cell)); x <= static_cast<int>(std::floor(hi.x / cell)); x++)
        {
            for(int y = static_cast<int>(std::floor(lo.y / cell)); y <= static_cast<int>(std::floor(hi.y / cell)); y++)
            {
                cells[key(x, y)].push_back(index);
            }
        }
    }

    [[nodiscard]] int at(const std::vector<MapFace>& faces, const glm::vec3& p) const
    {
        auto it = cells.find(key(static_cast<int>(std::floor(p.x / cell)), static_cast<int>(std::floor(p.y / cell))));
        if(it == cells.end())
        {
            return -1;
        }
        for(const int i : it->second)
        {
            const MapFace& f = faces[static_cast<size_t>(i)];
            if(std::abs(glm::dot(f.normal, p) - f.dist) > 1.f)
            {
                continue;
            }
            bool inside = false; // crossings of a ray along +x, in the xy plane
            for(size_t a = 0, b = f.pts.size() - 1; a < f.pts.size(); b = a++)
            {
                const glm::vec3 &pa = f.pts[a], &pb = f.pts[b];
                if((pa.y > p.y) != (pb.y > p.y) && p.x < (pb.x - pa.x) * (p.y - pa.y) / (pb.y - pa.y) + pa.x)
                {
                    inside = !inside;
                }
            }
            if(inside)
            {
                return i;
            }
        }
        return -1;
    }
};

// What entities a piece keeps away from: their boxes, grown by a margin.
struct Obstacle
{
    glm::vec3 lo, hi;
    const char* why;
};

[[nodiscard]] bool startsWith(const char* s, const char* prefix)
{
    return !strncmp(s, prefix, strlen(prefix));
}

void gatherObstacles(std::vector<Obstacle>& out)
{
    for(const StaticThing& t : statics)
    {
        out.push_back({t.lo - glm::vec3{40.f}, t.hi + glm::vec3{40.f}, t.classname.c_str()});
    }
    for(int i = svs.maxclients + 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free)
        {
            continue;
        }
        const char* cls = PR_GetString(e->v.classname);
        glm::vec3 lo = vec(e->v.absmin), hi = vec(e->v.absmax);
        if(glm::any(glm::greaterThan(lo, hi)) || (lo == glm::vec3{0.f} && hi == glm::vec3{0.f}))
        {
            lo = hi = vec(e->v.origin);
        }
        const glm::vec3 size = hi - lo;
        float margin = 0.f;
        const char* why = cls;
        glm::vec3 extra{0.f};
        if(startsWith(cls, "func_"))
        {
            if(!strcmp(cls, "func_wall") || !strcmp(cls, "func_illusionary") || !strcmp(cls, "func_episodegate") ||
                !strcmp(cls, "func_bossgate") || startsWith(cls, "func_detail") || !strcmp(cls, "func_group"))
            {
                margin = 8.f; // still brushes: not under or in them
            }
            else
            {
                // Doors, lifts, trains, buttons, what turns or moves: clear of where they go, as far as their own size
                // (a door slides about its width, a lift goes down its height).
                margin = 48.f;
                extra = glm::min(size, glm::vec3{128.f});
                if(startsWith(cls, "func_plat") || startsWith(cls, "func_new_plat") || startsWith(cls, "func_elvtr"))
                {
                    extra.z = std::max(extra.z, 256.f);
                }
            }
        }
        else if(!strcmp(cls, "trigger_teleport") || !strcmp(cls, "info_teleport_destination") ||
                startsWith(cls, "info_player") || startsWith(cls, "trigger_changelevel"))
        {
            margin = 64.f;
        }
        else if(startsWith(cls, "trigger_"))
        {
            margin = 24.f;
        }
        else if(startsWith(cls, "item_") || startsWith(cls, "weapon_") || startsWith(cls, "ammo_") ||
                startsWith(cls, "misc_explobox") || startsWith(cls, "monster_") || startsWith(cls, "path_corner") ||
                startsWith(cls, "light_torch") || startsWith(cls, "light_flame") || startsWith(cls, "func_weapon"))
        {
            margin = 40.f;
        }
        else if(e->v.modelindex > 0.f || static_cast<int>(e->v.solid) != SOLID_NOT)
        {
            margin = 24.f; // anything else there is to see or touch
        }
        else
        {
            continue; // lights, info_null, ambient sounds: nothing to keep off
        }
        out.push_back({lo - glm::vec3{margin} - extra, hi + glm::vec3{margin} + extra, why});
    }
}

// The worldspawn's "_vr_debris" (1 if none): 0 none in this map, another number times the chance.
[[nodiscard]] float worldspawnSetting()
{
    if(!sv.worldmodel || !sv.worldmodel->entities)
    {
        return 1.f;
    }
    const char* data = COM_Parse(sv.worldmodel->entities);
    if(!data || com_token[0] != '{')
    {
        return 1.f;
    }
    while(true)
    {
        data = COM_Parse(data);
        if(!data || com_token[0] == '}')
        {
            return 1.f;
        }
        const std::string key = com_token;
        data = COM_Parse(data);
        if(!data)
        {
            return 1.f;
        }
        if(key == "_vr_debris")
        {
            return std::max(static_cast<float>(atof(com_token)), 0.f);
        }
    }
}

[[nodiscard]] bool excludedMap(const char* name)
{
    const char* list = vr_debris_exclude.string;
    const size_t len = strlen(name);
    for(const char* p = list; *p;)
    {
        while(*p == ' ' || *p == ',' || *p == ';')
        {
            p++;
        }
        const char* start = p;
        while(*p && *p != ' ' && *p != ',' && *p != ';')
        {
            p++;
        }
        if(static_cast<size_t>(p - start) == len && !q_strncasecmp(start, name, len))
        {
            return true;
        }
    }
    return false;
}

// Rejection reasons, counted for vr_debug_debris.
enum Reason
{
    RNoFloor,
    RSlope,
    RLow,
    RLiquid,
    RCramped,
    RUneven,
    REntity,
    RSpacing,
    RArea,
    RCount
};
constexpr const char* reasonNames[RCount] = {"no floor at the wall", "sloped", "under something or a low wall",
    "liquid", "no room", "uneven or an edge", "by an entity", "too near another", "the area is full"};

struct Planner
{
    Rng rng;
    std::vector<MapFace> faces;
    FloorGrid floors;
    std::vector<Obstacle> obstacles;
    int rejected[RCount]{};
    float worldScale{1.25f};

    // The horizontal half-extent of piece `m` (scaled) along the unit vector `d` (world, horizontal), turned by `yaw`.
    [[nodiscard]] static float halfAlong(const ModelInfo& m, const glm::vec3& scale, bool onSide, float yaw, const glm::vec2& d)
    {
        const glm::vec3 half = (m.hi - m.lo) * 0.5f * scale;
        const float hx = half.x, hy = onSide ? half.z : half.y;
        const float c = std::cos(glm::radians(yaw)), s = std::sin(glm::radians(yaw));
        const glm::vec2 ax{c, s}, ay{-s, c};
        return std::abs(glm::dot(d, ax)) * hx + std::abs(glm::dot(d, ay)) * hy;
    }

    // Checks the place of a piece whose middle is over `p` (on the floor at z): room round it, a flat floor of the
    // world under all of it, no liquid, clear of entities. Fills the floor's point and normal.
    [[nodiscard]] bool fits(const ModelInfo& m, const glm::vec3& scale, bool onSide, float yaw, glm::vec3 p,
        glm::vec3& floor, glm::vec3& normal, int& reason)
    {
        const float height = (onSide ? (m.hi.y - m.lo.y) * scale.y : (m.hi.z - m.lo.z) * scale.z);
        const trace_t down = traceLine(p + glm::vec3{0.f, 0.f, 6.f}, p - glm::vec3{0.f, 0.f, 6.f});
        if(down.fraction >= 1.f || down.startsolid || down.ent != qcvm->edicts)
        {
            reason = RUneven;
            return false;
        }
        floor = vec(down.endpos);
        normal = vec(down.plane.normal);
        if(normal.z < 0.9f)
        {
            reason = RSlope;
            return false;
        }
        const glm::vec3 mid = floor + glm::vec3{0.f, 0.f, std::max(height * 0.5f, 0.6f)};
        if(contents(floor + glm::vec3{0.f, 0.f, 0.5f}) != CONTENTS_EMPTY || contents(floor + glm::vec3{0.f, 0.f, height + 4.f}) != CONTENTS_EMPTY)
        {
            reason = RLiquid;
            return false;
        }
        // Room round it: along eight ways, its own extent that way and a little more.
        for(int k = 0; k < 8; k++)
        {
            const float a = static_cast<float>(k) * 0.785398f;
            const glm::vec2 d{std::cos(a), std::sin(a)};
            const float reach = halfAlong(m, scale, onSide, yaw, d) + 0.3f;
            if(traceLine(mid, mid + glm::vec3{d * reach, 0.f}).fraction < 1.f)
            {
                reason = RCramped;
                return false;
            }
        }
        // Flat under all of it: the floor at its footprint's corners (a little in) as high as under its middle.
        const float c = std::cos(glm::radians(yaw)), s = std::sin(glm::radians(yaw));
        const glm::vec3 half = (m.hi - m.lo) * 0.5f * scale;
        const float hx = half.x * 0.8f, hy = (onSide ? half.z : half.y) * 0.8f;
        for(const glm::vec2 corner : {glm::vec2{hx, hy}, glm::vec2{-hx, hy}, glm::vec2{hx, -hy}, glm::vec2{-hx, -hy}})
        {
            const glm::vec3 at = floor + glm::vec3{c * corner.x - s * corner.y, s * corner.x + c * corner.y, 0.f};
            const trace_t t = traceLine(at + glm::vec3{0.f, 0.f, 3.f}, at - glm::vec3{0.f, 0.f, 3.f});
            if(t.fraction >= 1.f || t.startsolid || t.ent != qcvm->edicts || std::abs(t.endpos[2] - floor.z) > 1.f ||
                t.plane.normal[2] < 0.9f)
            {
                reason = RUneven;
                return false;
            }
        }
        // Open above (not under a step or a low ledge).
        if(traceLine(floor + glm::vec3{0.f, 0.f, 0.5f}, floor + glm::vec3{0.f, 0.f, 40.f}).fraction < 1.f)
        {
            reason = RLow;
            return false;
        }
        const float r = std::max(half.x, std::max(half.y, half.z));
        for(const Obstacle& o : obstacles)
        {
            if(floor.x + r > o.lo.x && floor.x - r < o.hi.x && floor.y + r > o.lo.y && floor.y - r < o.hi.y &&
                floor.z + height > o.lo.z && floor.z < o.hi.z)
            {
                if(vr_debug_debris.value >= 3)
                {
                    Con_Printf("debris: (%.0f %.0f %.0f) by %s\n", floor.x, floor.y, floor.z, o.why);
                }
                reason = REntity;
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] int pickModel(int kind)
    {
        float total = 0.f;
        for(const ModelInfo& m : models)
        {
            total += m.kind == kind && !m.skinLab.empty() ? m.weight : 0.f;
        }
        float u = rng.uniform() * total;
        int last = -1;
        for(int i = 0; i < numModels; i++)
        {
            if(models[i].kind != kind || models[i].skinLab.empty())
            {
                continue;
            }
            last = i;
            u -= models[i].weight;
            if(u <= 0.f)
            {
                return i;
            }
        }
        return last;
    }

    // A skin near the colour of what it lies by: weighted by how near (bricks keenly: a red wall sheds red bricks;
    // rocks loosely: a grey stone on brown earth is no surprise).
    [[nodiscard]] int pickSkin(const ModelInfo& m, const glm::vec3& lab, bool keen)
    {
        const float sigma = keen ? 0.025f : 0.06f, floorWeight = keen ? 0.02f : 0.35f;
        std::vector<float> w(m.skinLab.size());
        float total = 0.f;
        for(size_t k = 0; k < w.size(); k++)
        {
            glm::vec3 d = m.skinLab[k] - lab;
            d.x *= 0.3f; // lightness counts less than hue: the world's lightmaps and the models' shading differ
            w[k] = floorWeight + std::exp(-glm::dot(d, d) / (sigma * sigma));
            total += w[k];
        }
        float u = rng.uniform() * total;
        for(size_t k = 0; k < w.size(); k++)
        {
            u -= w[k];
            if(u <= 0.f)
            {
                return static_cast<int>(k);
            }
        }
        return static_cast<int>(w.size()) - 1;
    }

    [[nodiscard]] glm::vec3 pickScale(int kind)
    {
        const float spread = std::clamp(vr_debris_size.value, 0.f, 0.6f) * (kind == 2 ? 0.4f : 1.f);
        const float k = 1.f + spread * (2.f * rng.uniform() - 1.f);
        const float axis = kind == 2 ? 0.02f : 0.07f; // a little more one way than another
        return glm::vec3{k * (1.f + rng.range(-axis, axis)), k * (1.f + rng.range(-axis, axis)), k * (1.f + rng.range(-axis, axis))} *
               worldScale;
    }
};

// A place at a wall's foot: a point of its bottom edge, the way out of the wall, along it, and what is there.
struct Spot
{
    glm::vec3 at;
    glm::vec2 out, along;
    int face;
    float rockWeight, brickWeight;
    glm::vec3 rockLab, brickLab;
    float light; // the floor's lightmap there (floorLight)
};

// "vr_debris_list [lit]": the pieces in the map now (the server's entities), each with its model, skin, place, whether
// it rests (asleep, on the ground) and the light there (the client's R_LightPoint: 128 is Quake's full light); "lit":
// the brightest first (to find ones to look at).
void list_f()
{
    if(!sv.active)
    {
        return;
    }
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    struct Row
    {
        int num, light;
        std::string text;
    };
    std::vector<Row> rows;
    lightcache_t cache{};
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        const char* cls = e->free ? "" : PR_GetString(e->v.classname);
        if(strcmp(cls, "vr_rock") != 0 && strcmp(cls, "vr_brick") != 0)
        {
            continue;
        }
        vec3_t at{e->v.origin[0], e->v.origin[1], e->v.origin[2] + 4.f};
        const int light = cl.worldmodel == sv.worldmodel ? R_LightPoint(at, 0.f, &cache) : -1;
        rows.push_back({i, light,
            va("%d %s %s skin %d at (%.1f %.1f %.1f) angles (%.0f %.0f %.0f)%s light %d", i, cls,
                PR_GetString(e->v.model) + 6, static_cast<int>(e->v.skin), e->v.origin[0], e->v.origin[1], e->v.origin[2],
                e->v.angles[0], e->v.angles[1], e->v.angles[2], (static_cast<int>(e->v.flags) & FL_ONGROUND) ? ", resting" : "",
                light)});
    }
    PR_PopQCVM(oldVm);
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "lit"))
    {
        std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.light > b.light; });
    }
    for(const Row& r : rows)
    {
        Con_Printf("debris: %s\n", r.text.c_str());
    }
    Con_Printf("debris: %d pieces\n", static_cast<int>(rows.size()));
}

} // namespace

Material materialOf(const char* texture)
{
    const std::string s = cleanName(texture);
    if(s.empty() || s[0] == '*' || s[0] == '!')
    {
        return Material::None; // liquids
    }
    if(!strncmp(s.c_str(), "rock", 4) && s.size() > 4 && std::isdigit(static_cast<unsigned char>(s[4])))
    {
        size_t i = 4;
        while(i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])))
        {
            i++;
        }
        if(i == s.size() || s[i] == '_')
        {
            return Material::Natural;
        }
    }
    for(const Rule& r : rules)
    {
        const bool hit = r.prefix ? !strncmp(s.c_str(), r.pattern, strlen(r.pattern)) : s.find(r.pattern) != std::string::npos;
        if(hit)
        {
            return r.material;
        }
    }
    return Material::Other;
}

const char* materialName(Material m)
{
    switch(m)
    {
    case Material::None: return "none";
    case Material::Natural: return "natural";
    case Material::Fieldstone: return "fieldstone";
    case Material::Masonry: return "masonry";
    case Material::Brick: return "brick";
    case Material::Metal: return "metal";
    case Material::Wood: return "wood";
    default: return "other";
    }
}

bool enabledHere()
{
    return vr_debris.value != 0.f && sv.active && sv.worldmodel && svs.maxclients == 1 && !excludedMap(sv.name) &&
           worldspawnSetting() > 0.f;
}

int plan()
{
    if(static bool registered = false; !registered) // (no init hook of its own)
    {
        registered = true;
        Cmd_AddCommand("vr_debris_list", list_f);
    }
    placements.clear();
    if(!enabledHere())
    {
        if(vr_debug_debris.value && sv.active)
        {
            const char* why = vr_debris.value == 0.f ? "vr_debris 0" : svs.maxclients != 1 ? "multiplayer" :
                              excludedMap(sv.name) ? "in vr_debris_exclude" : "its worldspawn's _vr_debris is 0";
            Con_Printf("debris: %s: none (%s)\n", sv.name, why);
        }
        return 0;
    }
    const double t0 = Sys_DoubleTime();
    for(ModelInfo& m : models)
    {
        loadModel(m); // (every map: a repainted skin counts at once)
    }

    Planner pl;
    pl.rng.s = hashString(sv.name) ^ (static_cast<uint64_t>(static_cast<int64_t>(vr_debris_seed.value)) * 0x9E3779B97F4A7C15ull);
    pl.worldScale = std::clamp(vr_world_scale.value, 0.5f, 3.f);
    const float chance = std::max(vr_debris_chance.value, 0.f) * worldspawnSetting();
    const bool rocks = vr_debris_rocks.value != 0.f, bricks = vr_debris_bricks.value != 0.f;

    // The world's faces, their materials; the floors in a grid.
    const qmodel_t* map = sv.worldmodel;
    pl.faces.reserve(static_cast<size_t>(map->nummodelsurfaces));
    for(int i = 0; i < map->nummodelsurfaces; i++)
    {
        const msurface_t& surf = map->surfaces[map->firstmodelsurface + i];
        if(surf.numedges < 3 || !surf.texinfo)
        {
            continue;
        }
        MapFace f;
        f.normal = vec(surf.plane->normal);
        f.dist = surf.plane->dist;
        if(surf.flags & SURF_PLANEBACK)
        {
            f.normal = -f.normal;
            f.dist = -f.dist;
        }
        const int texnum = surf.texinfo->texnum;
        f.texture = texnum >= 0 && texnum < map->numtextures ? map->textures[texnum] : nullptr;
        f.material = (surf.flags & (SURF_DRAWSKY | SURF_DRAWTURB)) ? Material::None : materialOf(f.texture ? f.texture->name : "");
        for(int k = 0; k < surf.numedges; k++)
        {
            const int e = map->surfedges[surf.firstedge + k];
            const int v = static_cast<int>(e >= 0 ? map->edges[e].v[0] : map->edges[-e].v[1]);
            f.pts.push_back(vec(map->vertexes[v].position));
        }
        f.surf = map->firstmodelsurface + i;
        pl.faces.push_back(std::move(f));
        if(pl.faces.back().normal.z >= 0.7f && pl.faces.back().material != Material::None)
        {
            pl.floors.add(static_cast<int>(pl.faces.size()) - 1, pl.faces.back());
        }
    }
    gatherObstacles(pl.obstacles);

    // The spots: along each wall's bottom edge, every 16 units, where a floor meets it.
    std::vector<Spot> spots;
    int edgesSeen = 0;
    for(size_t fi = 0; fi < pl.faces.size(); fi++)
    {
        const MapFace& f = pl.faces[fi];
        const glm::vec2 outFull{f.normal};
        if(f.normal.z < -0.3f || f.normal.z > 0.6f || glm::length(outFull) < 0.5f || f.material == Material::None)
        {
            continue;
        }
        float wallRock = 0.f, wallBrick = 0.f;
        switch(f.material)
        {
        case Material::Natural: wallRock = 1.f; break;
        case Material::Fieldstone: wallRock = 0.8f; break;
        case Material::Masonry: wallRock = 0.25f; break;
        case Material::Brick: wallBrick = 1.f; break;
        default: break;
        }
        float lo = 1e9f;
        for(const glm::vec3& p : f.pts)
        {
            lo = std::min(lo, p.z);
        }
        const glm::vec2 out = glm::normalize(outFull);
        for(size_t k = 0; k < f.pts.size(); k++)
        {
            const glm::vec3 a = f.pts[k], b = f.pts[(k + 1) % f.pts.size()];
            if(std::abs(a.z - lo) > 0.5f || std::abs(b.z - lo) > 0.5f || glm::length(b - a) < 8.f)
            {
                continue;
            }
            edgesSeen++;
            const glm::vec3 edge = b - a;
            const float len = glm::length(edge);
            const glm::vec2 along = glm::normalize(glm::vec2{edge});
            for(float t = 6.f; t < len - 2.f; t += 16.f)
            {
                const glm::vec3 at = a + edge * (t / len);
                // The floor that meets it (a point just out from the wall, at the edge's height).
                const glm::vec3 probe = at + glm::vec3{out * 2.f, 0.f};
                const trace_t down = traceLine(probe + glm::vec3{0.f, 0.f, 4.f}, probe - glm::vec3{0.f, 0.f, 4.f});
                if(down.fraction >= 1.f || down.startsolid || std::abs(down.endpos[2] - at.z) > 1.5f)
                {
                    pl.rejected[RNoFloor]++;
                    continue;
                }
                const int floorFace = pl.floors.at(pl.faces, vec(down.endpos));
                const Material floorMat = floorFace >= 0 ? pl.faces[static_cast<size_t>(floorFace)].material : Material::Other;
                const float floorRock = floorMat == Material::Natural ? 1.f : 0.f;
                const float floorBrick = floorMat == Material::Brick ? 0.25f : 0.f;
                Spot s;
                s.at = vec(down.endpos);
                s.out = out;
                s.along = along;
                s.face = static_cast<int>(fi);
                s.rockWeight = rocks ? std::max(wallRock, floorRock) : 0.f;
                s.brickWeight = bricks ? std::max(wallBrick, floorBrick) : 0.f;
                if(s.rockWeight <= 0.f && s.brickWeight <= 0.f)
                {
                    continue;
                }
                const texture_t* floorTex = floorFace >= 0 ? pl.faces[static_cast<size_t>(floorFace)].texture : nullptr;
                s.rockLab = textureLab(wallRock >= floorRock ? f.texture : floorTex);
                s.brickLab = textureLab(wallBrick >= floorBrick ? f.texture : floorTex);
                s.light = floorFace >= 0 ? floorLight(map, pl.faces[static_cast<size_t>(floorFace)].surf, s.at + glm::vec3{out * 4.f, 0.f}) : 255.f;
                spots.push_back(s);
            }
        }
    }

    // In a random order (the same each load), so that the limits leave no part of the map favoured.
    for(size_t i = spots.size(); i > 1; i--)
    {
        std::swap(spots[i - 1], spots[pl.rng.next() % i]);
    }

    const int freeEdicts = qcvm->max_edicts - qcvm->num_edicts - static_cast<int>(std::max(vr_debris_edicts_left.value, 0.f));
    const int most = std::min(static_cast<int>(std::max(vr_debris_max.value, 0.f)), std::max(freeEdicts, 0));
    const float areaSize = std::max(vr_debris_area_size.value, 32.f);
    const int areaMax = static_cast<int>(std::max(vr_debris_area_max.value, 1.f));
    const float spacing = std::max(vr_debris_spacing.value, 0.f);
    const int clusterMax = std::clamp(static_cast<int>(vr_debris_cluster.value), 1, 6);
    std::unordered_map<int64_t, int> areas;
    std::vector<glm::vec3> anchors;
    int corners = 0, rolled = 0;

    for(const Spot& s : spots)
    {
        if(static_cast<int>(placements.size()) >= most)
        {
            break;
        }
        const float weight = std::max(s.rockWeight, s.brickWeight);
        // A corner: a wall beside the spot, along its own wall (either way).
        const glm::vec3 side = s.at + glm::vec3{s.out * 6.f, 5.f};
        const bool corner = traceLine(side, side + glm::vec3{s.along * 20.f, 0.f}).fraction < 1.f ||
                            traceLine(side, side - glm::vec3{s.along * 20.f, 0.f}).fraction < 1.f;
        // Fewer where it is dark (where nobody sees them: the foot of a wall is often the darkest of a room).
        const float dark = std::clamp(vr_debris_dark.value, 0.f, 1.f);
        const float lit = dark + (1.f - dark) * std::clamp((s.light - 8.f) / 32.f, 0.f, 1.f);
        const float roll = pl.rng.uniform();
        if(roll >= chance * weight * lit * (corner ? std::max(vr_debris_corner.value, 0.f) : 1.f))
        {
            continue;
        }
        rolled++;
        const int kind = pl.rng.uniform() * (s.rockWeight + s.brickWeight) < s.brickWeight ? 2 : 1;
        const glm::vec3 lab = kind == 2 ? s.brickLab : s.rockLab;
        bool first = true;
        const int n = std::min(clusterMax, 1 + (pl.rng.uniform() < (corner ? 0.5f : 0.3f)) + (pl.rng.uniform() < (corner ? 0.25f : 0.1f)));
        glm::vec3 anchor{0.f};
        float anchorHalf = 0.f;
        std::vector<std::pair<glm::vec3, float>> cluster;
        for(int piece = 0; piece < n && static_cast<int>(placements.size()) < most; piece++)
        {
            Placement p;
            p.model = pl.pickModel(kind);
            if(p.model < 0)
            {
                break;
            }
            const ModelInfo& m = models[p.model];
            p.scale = pl.pickScale(kind);
            p.onSide = kind == 2 && pl.rng.uniform() < 0.2f;
            const float wallYaw = glm::degrees(std::atan2(s.along.y, s.along.x));
            p.yaw = kind == 2 && pl.rng.uniform() < 0.5f ? wallYaw + pl.rng.range(-20.f, 20.f) : pl.rng.range(0.f, 360.f);
            const float half = Planner::halfAlong(m, p.scale, p.onSide, p.yaw, s.out);
            const float halfAlongWall = Planner::halfAlong(m, p.scale, p.onSide, p.yaw, s.along);
            glm::vec3 at;
            if(first)
            {
                at = s.at + glm::vec3{s.out * (half + pl.rng.range(0.3f, 1.5f)), 0.f};
            }
            else
            {
                // Beside the first, along the wall, or just out from it.
                const float dir = pl.rng.uniform() < 0.5f ? -1.f : 1.f;
                const float gap = anchorHalf + halfAlongWall + pl.rng.range(0.3f, 2.5f);
                at = anchor + glm::vec3{s.along * (dir * gap), 0.f} + glm::vec3{s.out * pl.rng.range(-0.5f, 2.f), 0.f};
                at = s.at + glm::vec3{s.out * std::max(glm::dot(glm::vec2{at - s.at}, s.out), half + 0.3f), 0.f} +
                     glm::vec3{s.along * glm::dot(glm::vec2{at - s.at}, s.along), 0.f};
            }
            at.z = s.at.z;
            p.yaw = std::fmod(p.yaw + 360.f, 360.f);
            glm::vec3 floor, normal;
            int reason = RCount;
            const float r = std::max(half, halfAlongWall);
            bool ok = pl.fits(m, p.scale, p.onSide, p.yaw, at, floor, normal, reason);
            if(ok)
            {
                for(const auto& [c, cr] : cluster)
                {
                    if(glm::length(glm::vec2{c - floor}) < cr + r * 0.9f)
                    {
                        ok = false;
                        reason = RCramped;
                    }
                }
            }
            if(ok && first)
            {
                for(const glm::vec3& a : anchors)
                {
                    if(glm::length(a - floor) < spacing)
                    {
                        ok = false;
                        reason = RSpacing;
                        break;
                    }
                }
            }
            const int64_t area = (static_cast<int64_t>(std::floor(floor.x / areaSize)) << 42) ^
                                 (static_cast<int64_t>(std::floor(floor.y / areaSize)) << 21) ^
                                 static_cast<int64_t>(std::floor(floor.z / areaSize)) & 0x1fffff;
            if(ok && areas[area] >= areaMax)
            {
                ok = false;
                reason = RArea;
            }
            if(!ok)
            {
                pl.rejected[reason]++;
                if(first)
                {
                    break; // the spot itself won't do
                }
                continue;
            }
            p.floor = floor;
            p.normal = normal;
            p.out = s.out;
            p.skin = pl.pickSkin(m, lab, kind == 2);
            placements.push_back(p);
            areas[area]++;
            cluster.emplace_back(floor, r);
            if(first)
            {
                anchors.push_back(floor);
                anchor = floor;
                anchorHalf = halfAlongWall;
                first = false;
                corners += corner;
            }
        }
    }

    if(vr_debug_debris.value || developer.value)
    {
        int numRocks = 0;
        uint64_t layout = 1469598103934665603ull;
        for(const Placement& p : placements)
        {
            numRocks += models[p.model].kind == 1;
            const int q[5] = {p.model, p.skin, static_cast<int>(std::lround(p.floor.x * 8.f)), static_cast<int>(std::lround(p.floor.y * 8.f)),
                static_cast<int>(std::lround(p.yaw * 10.f))};
            for(const int v : q)
            {
                layout = (layout ^ static_cast<uint32_t>(v)) * 1099511628211ull;
            }
        }
        Con_Printf("debris: %s: %d pieces (%d rocks, %d bricks; %d spots in corners) from %d spots on %d wall edges, %d "
                   "rolled; limit %d (%d entities, %d free); %.1f ms; layout %08x\n",
            sv.name, static_cast<int>(placements.size()), numRocks, static_cast<int>(placements.size()) - numRocks, corners,
            static_cast<int>(spots.size()), edgesSeen, rolled, most, qcvm->num_edicts, qcvm->max_edicts - qcvm->num_edicts,
            (Sys_DoubleTime() - t0) * 1000.0, static_cast<unsigned>(layout ^ (layout >> 32)));
        planMs = (Sys_DoubleTime() - t0) * 1000.0;
        if(vr_debug_debris.value >= 1)
        {
            std::string why;
            for(int r = 0; r < RCount; r++)
            {
                if(pl.rejected[r])
                {
                    why += va("%s%d %s", why.empty() ? "" : ", ", pl.rejected[r], reasonNames[r]);
                }
            }
            Con_Printf("debris: rejected: %s\n", why.empty() ? "none" : why.c_str());
        }
        if(vr_debug_debris.value >= 2)
        {
            for(size_t i = 0; i < placements.size(); i++)
            {
                const Placement& p = placements[i];
                Con_Printf("debris: %d %s skin %d at (%.1f %.1f %.1f) yaw %.0f%s size %.2f, out (%.2f %.2f)\n", static_cast<int>(i),
                    models[p.model].name + 6, p.skin, p.floor.x, p.floor.y, p.floor.z, p.yaw, p.onSide ? " on its side" : "",
                    p.scale.x / pl.worldScale, p.out.x, p.out.y);
            }
        }
    }
    return static_cast<int>(placements.size());
}

const char* modelOf(int i)
{
    return i >= 0 && i < static_cast<int>(placements.size()) ? models[placements[static_cast<size_t>(i)].model].name : "";
}

int put(edict_t* e, int i)
{
    if(i < 0 || i >= static_cast<int>(placements.size()))
    {
        return 0;
    }
    const Placement& p = placements[static_cast<size_t>(i)];
    const progs::FieldOffsets& f = fields();
    e->v.skin = static_cast<float>(p.skin);
    e->v.frame = 0.f;
    progs::setFieldVec(e, f.model_scale, p.scale - glm::vec3{1.f});

    // Its turn: its base on the floor (up the floor's normal), turned by its yaw about it; a brick on its side has its
    // width up.
    const glm::vec3 n = glm::normalize(p.normal);
    const glm::vec3 f0{std::cos(glm::radians(p.yaw)), std::sin(glm::radians(p.yaw)), 0.f};
    const glm::vec3 fwd = glm::normalize(f0 - n * glm::dot(f0, n));
    const glm::mat3 axes = p.onSide ? glm::mat3{fwd, n, glm::cross(fwd, n)} : glm::mat3{fwd, glm::cross(n, fwd), n};
    held::anglesFromAxes(axes, e->v.angles, false);

    // Resting on its lowest corner along the floor's normal; its box round it as it lies.
    const glm::mat3 drawn = held::axesFromAngles(e->v.angles, false);
    thread_local std::vector<glm::vec3> verts;
    glm::vec3 lo{1e9f}, hi{-1e9f};
    float lowest = 0.f;
    if(held::drawnVertices(e, verts) && !verts.empty())
    {
        lowest = 1e9f;
        for(const glm::vec3& v : verts)
        {
            const glm::vec3 w = drawn * v;
            lowest = std::min(lowest, glm::dot(w, n));
            lo = glm::min(lo, w);
            hi = glm::max(hi, w);
        }
    }
    else
    {
        const ModelInfo& m = models[p.model];
        lo = m.lo * p.scale;
        hi = m.hi * p.scale;
        lowest = lo.z;
    }
    const glm::vec3 origin = p.floor - n * lowest + n * 0.02f;
    for(int k = 0; k < 3; k++)
    {
        e->v.origin[k] = origin[k];
        e->v.mins[k] = lo[k];
        e->v.maxs[k] = hi[k];
        e->v.size[k] = hi[k] - lo[k];
    }
    e->v.flags = static_cast<float>(static_cast<int>(e->v.flags) | FL_ONGROUND);
    e->v.groundentity = EDICT_TO_PROG(qcvm->edicts);
    e->v.velocity[0] = e->v.velocity[1] = e->v.velocity[2] = 0.f;
    SV_LinkEdict(e, false);
    return models[p.model].kind;
}

} // namespace qvr::debris

namespace qvr::debris
{

void reset()
{
    statics.clear();
    placements.clear();
    spawnStart = Sys_DoubleTime();
    planMs = 0.0;
}

void afterLoad()
{
    if(vr_debug_debris.value && spawnStart > 0.0)
    {
        Con_Printf("debris: %s: the server spawned in %.1f ms (%d entities; placing the pieces %.1f ms)\n", sv.name,
            (Sys_DoubleTime() - spawnStart) * 1000.0, qcvm->num_edicts, planMs);
    }
    spawnStart = 0.0;
}

} // namespace qvr::debris

extern "C" void VR_OnMakeStatic(edict_t* ent)
{
    using namespace qvr::debris;
    if(sv.state != ss_loading || !ent)
    {
        return;
    }
    const glm::vec3 o = vec(ent->v.origin);
    const glm::vec3 lo = glm::min(vec(ent->v.mins), glm::vec3{-16.f}), hi = glm::max(vec(ent->v.maxs), glm::vec3{16.f});
    statics.push_back({o + lo, o + hi, PR_GetString(ent->v.classname)});
}
