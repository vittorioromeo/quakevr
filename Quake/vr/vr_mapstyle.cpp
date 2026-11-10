// vr_mapstyle.cpp -- see vr_mapstyle.hpp.
#include "vr_mapstyle.hpp"

#include "vr_engine.hpp"
#include "vr_progs.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <cctype>
#include <cstring>

namespace qvr::mapstyle
{
namespace
{
constexpr int styles = static_cast<int>(Style::Count);

struct Cell
{
    float w[styles]{};
};

// The server's map binned (made again when the map changes).
struct Grid
{
    const qmodel_t* model{nullptr};
    int surfaces{-1};
    char mapName[64]{};
    glm::vec3 origin{0.f};
    glm::ivec3 dims{0};
    za::Vector<Cell> cells;
    float total[styles]{};
};
Grid grid;

constexpr const char* medievalWords[] = {"wizmet", "wbrick", "city", "wood", "wwall", "wceil", "wswamp", "wiz1"};
// (E2-E4's metal panels, metal1_x .. metal5_x, metalt2: their runic metal, not E1's tech base.)
constexpr const char* runicWords[] = {"rune", "wizard", "skill", "metal"};
constexpr const char* baseWords[] = {"tech", "comp", "plat", "base", "slip", "twall", "tlight"};

template <size_t N>
[[nodiscard]] bool hasAny(const char* s, const char* const (&words)[N])
{
    for(const char* w : words)
    {
        if(strstr(s, w))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] float areaOf(const msurface_t& surf)
{
    float e[3] = {surf.maxs[0] - surf.mins[0], surf.maxs[1] - surf.mins[1], surf.maxs[2] - surf.mins[2]};
    // (the two largest extents: a wall's or a floor's face)
    const float lo = za::min(e[0], za::min(e[1], e[2]));
    const float sum = e[0] + e[1] + e[2] - lo;
    const float hi = za::max(e[0], za::max(e[1], e[2]));
    return za::max(1.f, hi * (sum - hi));
}

[[nodiscard]] int cellIndex(const glm::ivec3& c)
{
    return (c.z * grid.dims.y + c.y) * grid.dims.x + c.x;
}

[[nodiscard]] glm::ivec3 cellOf(const glm::vec3& p)
{
    const glm::vec3 f = (p - grid.origin) / cellSize;
    return glm::clamp(glm::ivec3{static_cast<int>(f.x), static_cast<int>(f.y), static_cast<int>(f.z)}, glm::ivec3{0}, grid.dims - 1);
}

// The grid for the server's map, binned once (every world surface's area into its middle's cell).
[[nodiscard]] bool ready()
{
    const qmodel_t* m = sv.active ? sv.worldmodel : nullptr;
    if(!m)
    {
        return false;
    }
    if(grid.model == m && grid.surfaces == m->nummodelsurfaces && !strcmp(grid.mapName, m->name))
    {
        return true;
    }
    grid = Grid{};
    grid.model = m;
    grid.surfaces = m->nummodelsurfaces;
    q_strlcpy(grid.mapName, m->name, sizeof(grid.mapName));
    grid.origin = glm::vec3{m->mins[0], m->mins[1], m->mins[2]};
    const glm::vec3 size = glm::vec3{m->maxs[0], m->maxs[1], m->maxs[2]} - grid.origin;
    grid.dims = glm::max(glm::ivec3{1}, glm::ivec3{glm::ceil(size / cellSize)});
    grid.cells.resize(static_cast<za::SizeT>(grid.dims.x * grid.dims.y * grid.dims.z));
    for(int i = 0; i < m->nummodelsurfaces; i++)
    {
        const msurface_t& surf = m->surfaces[m->firstmodelsurface + i];
        if(!surf.texinfo || surf.texinfo->texnum < 0 || surf.texinfo->texnum >= m->numtextures ||
            !m->textures[surf.texinfo->texnum])
        {
            continue;
        }
        const int s = static_cast<int>(ofTexture(m->textures[surf.texinfo->texnum]->name));
        if(s == static_cast<int>(Style::Unknown))
        {
            continue;
        }
        const glm::vec3 mid{(surf.mins[0] + surf.maxs[0]) * 0.5f, (surf.mins[1] + surf.maxs[1]) * 0.5f, (surf.mins[2] + surf.maxs[2]) * 0.5f};
        const float a = areaOf(surf);
        grid.cells[static_cast<za::SizeT>(cellIndex(cellOf(mid)))].w[s] += a;
        grid.total[s] += a;
    }
    return true;
}

// The heaviest style of `w`: a liquid's when liquids are a quarter of it or more.
[[nodiscard]] Style pick(const float (&w)[styles])
{
    float all = 0.f, wet = 0.f;
    for(int s = 1; s < styles; s++)
    {
        all += w[s];
    }
    for(int s = static_cast<int>(Style::Water); s <= static_cast<int>(Style::Lava); s++)
    {
        wet += w[s];
    }
    if(all <= 0.f)
    {
        return Style::Unknown;
    }
    const int from = wet >= 0.25f * all ? static_cast<int>(Style::Water) : static_cast<int>(Style::Base);
    const int to = wet >= 0.25f * all ? static_cast<int>(Style::Lava) : static_cast<int>(Style::Runic);
    int best = from;
    for(int s = from; s <= to; s++)
    {
        best = w[s] > w[best] ? s : best;
    }
    return static_cast<Style>(best);
}

// The weights round `c`: its own, and half of each neighbour's (an area is wider than a cell).
void around(const glm::ivec3& c, float (&w)[styles])
{
    for(int dz = -1; dz <= 1; dz++)
    {
        for(int dy = -1; dy <= 1; dy++)
        {
            for(int dx = -1; dx <= 1; dx++)
            {
                const glm::ivec3 n = c + glm::ivec3{dx, dy, dz};
                if(glm::any(glm::lessThan(n, glm::ivec3{0})) || glm::any(glm::greaterThanEqual(n, grid.dims)))
                {
                    continue;
                }
                const float k = (dx == 0 && dy == 0 && dz == 0) ? 1.f : 0.5f;
                const Cell& cell = grid.cells[static_cast<za::SizeT>(cellIndex(n))];
                for(int s = 0; s < styles; s++)
                {
                    w[s] += k * cell.w[s];
                }
            }
        }
    }
}

void printWeights(const float (&w)[styles])
{
    float all = 0.f;
    for(int s = 1; s < styles; s++)
    {
        all += w[s];
    }
    for(int s = 1; s < styles; s++)
    {
        if(w[s] > 0.f)
        {
            Con_Printf(" %s %.0f%%", name(static_cast<Style>(s)), 100.f * w[s] / za::max(all, 1.f));
        }
    }
    Con_Printf("\n");
}

void here_f()
{
    if(!ready())
    {
        Con_Printf("vr_mapstyle: no map\n");
        return;
    }
    glm::vec3 p{0.f};
    if(Cmd_Argc() >= 4)
    {
        p = glm::vec3{Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3))};
    }
    else if(const edict_t* e = progs::playerEdict())
    {
        p = glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]};
    }
    float w[styles]{};
    around(cellOf(p), w);
    Con_Printf("mapstyle: %s at %.0f %.0f %.0f: %s:", grid.mapName, p.x, p.y, p.z, name(pick(w)));
    printWeights(w);
}

void dump_f()
{
    if(!ready())
    {
        Con_Printf("vr_mapstyle_dump: no map\n");
        return;
    }
    Con_Printf("mapstyle: %s: %s overall:", grid.mapName, name(pick(grid.total)));
    printWeights(grid.total);
    int count[styles]{};
    int used = 0;
    const bool list = Cmd_Argc() >= 2 && Q_atoi(Cmd_Argv(1)) != 0;
    for(int z = 0; z < grid.dims.z; z++)
    {
        for(int y = 0; y < grid.dims.y; y++)
        {
            for(int x = 0; x < grid.dims.x; x++)
            {
                const glm::ivec3 c{x, y, z};
                const Cell& cell = grid.cells[static_cast<za::SizeT>(cellIndex(c))];
                float own = 0.f;
                for(int s = 1; s < styles; s++)
                {
                    own += cell.w[s];
                }
                if(own <= 0.f)
                {
                    continue;
                }
                float w[styles]{};
                around(c, w);
                const Style st = pick(w);
                count[static_cast<int>(st)]++;
                used++;
                if(list)
                {
                    const glm::vec3 mid = grid.origin + (glm::vec3{c} + 0.5f) * cellSize;
                    Con_Printf("mapstyle: cell %.0f %.0f %.0f: %s:", mid.x, mid.y, mid.z, name(st));
                    printWeights(w);
                }
            }
        }
    }
    Con_Printf("mapstyle: %s: %d areas of %.0f units:", grid.mapName, used, cellSize);
    for(int s = 0; s < styles; s++)
    {
        if(count[s])
        {
            Con_Printf(" %s %d", name(static_cast<Style>(s)), count[s]);
        }
    }
    Con_Printf("\n");
}
} // namespace

Style ofTexture(const char* texture)
{
    char s[32];
    size_t n = 0;
    for(const char* c = texture; c && *c && n + 1 < sizeof(s); c++)
    {
        s[n++] = static_cast<char>(tolower(static_cast<unsigned char>(*c)));
    }
    s[n] = 0;
    const char* t = s;
    if(t[0] == '+' && t[1])
    {
        t += 2; // (an animated texture's frame: +0tech...)
    }
    if(t[0] == '*')
    {
        if(strstr(t, "lava"))
        {
            return Style::Lava;
        }
        if(strstr(t, "slime"))
        {
            return Style::Slime;
        }
        return strstr(t, "tele") ? Style::Unknown : Style::Water;
    }
    if(!strncmp(t, "sky", 3) || !strncmp(t, "clip", 4) || !strncmp(t, "trigger", 7) || !strncmp(t, "skip", 4) ||
        !strncmp(t, "hint", 4))
    {
        return Style::Unknown;
    }
    if(hasAny(t, medievalWords))
    {
        return Style::Medieval;
    }
    if(hasAny(t, runicWords))
    {
        return Style::Runic;
    }
    if(hasAny(t, baseWords))
    {
        return Style::Base;
    }
    return Style::Unknown;
}

const char* name(Style s)
{
    switch(s)
    {
    case Style::Base: return "base";
    case Style::Medieval: return "medieval";
    case Style::Runic: return "runic";
    case Style::Water: return "water";
    case Style::Slime: return "slime";
    case Style::Lava: return "lava";
    default: return "unknown";
    }
}

Style at(const glm::vec3& p)
{
    if(!ready())
    {
        return Style::Unknown;
    }
    float w[styles]{};
    around(cellOf(p), w);
    return pick(w);
}

void registerCommands()
{
    Cmd_AddCommand("vr_mapstyle", here_f);
    Cmd_AddCommand("vr_mapstyle_dump", dump_f);
}

void builtin()
{
    const float* p = G_VECTOR(OFS_PARM0);
    G_FLOAT(OFS_RETURN) = static_cast<float>(at(glm::vec3{p[0], p[1], p[2]}));
}
} // namespace qvr::mapstyle
