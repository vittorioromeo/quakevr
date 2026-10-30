#pragma once

// vr_relight_internal.hpp -- what vr_relight.cpp (see vr_relight.hpp) shares with its parts vr_relight_bsp.cpp (the
// BSP's lumps and entities, map lights, the texture rules) and vr_relight_glow.cpp (the faces, things and glowing
// textures that become lights). Internal: included by those alone. What was the file's anonymous namespace is
// relight::detail: its types, constants and small helpers here, its functions declared here and defined in the file
// that owns them.

#include "vr_bytes.hpp"
#include "vr_strings.hpp"
#include "vr_relight.hpp"
#include "vr_relight_maps.hpp"
#include "vr_relight_tool.hpp"
#include "vr_relight_vis.hpp"
#include "vr_brushbake.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_gadget.hpp"
#include "vr_mem.hpp"
#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::relight::detail
{

inline constexpr double glowObject = 64.0;

inline constexpr double glowRoom = 256.0;

inline constexpr double fixtureLight = 250.0;

inline constexpr double fixtureLit = 0.5;

inline constexpr double fixtureNear = 128.0;

inline constexpr double fixtureOffset = 4.0;

inline constexpr double fixtureStep = 128.0;

inline constexpr double recessMax = 48.0;

inline constexpr double glowMinShare = 0.03;

inline constexpr double fixtureMinShare = 0.005;

inline constexpr double liquidStep = 96.0;

inline constexpr double liquidHeight = 16.0;

inline constexpr double liquidLight = 120.0;

inline constexpr double liquidRoom = 192.0;

inline constexpr double liquidLone = 5.0;

inline constexpr double liquidCrowd = 0.4;

inline constexpr double glowBudget = 300.0;

inline constexpr int contentsEmpty = -1;

inline constexpr int contentsSolid = -2;

inline constexpr const char* lightOptions = "-lit -lux -lightgrid"; // relight_maps.py's -lit and OUTPUT_ARGS

// The author's ericw-tools (relight_maps.py's DEFAULT_LIGHT), the last place looked in, and only at Menu Detail:
// Developer (vr_menu_level 2): a player never relies on a path that exists on one machine.
inline constexpr const char* authorsLight = "C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64/light.exe";

// Python's sum() of floats (3.12 on: Neumaier's compensated sum), which relight_maps.py's numbers come from: the same
// sums here give the same lights to the last digit.
struct PySum
{
    double s{0.0};
    double c{0.0};

    void add(double x)
    {
        const double t = s + x;
        c += za::abs(s) >= za::abs(x) ? (s - t) + x : (x - t) + s;
        s = t;
    }

    [[nodiscard]] double value() const
    {
        return c != 0.0 && isfinite(c) ? s + c : s;
    }
};

struct V3
{
    double v[3]{};

    double operator[](int i) const
    {
        return v[i];
    }
    double& operator[](int i)
    {
        return v[i];
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// The .bsp.

struct Bsp : bytes::View
{
    [[nodiscard]] double f32(za::SizeT at) const { return static_cast<double>(View::f32(at)); }
    [[nodiscard]] int lumpOffset(int index) const
    {
        return i32(4 + static_cast<za::SizeT>(index) * 8);
    }
    [[nodiscard]] int lumpLength(int index) const
    {
        return i32(8 + static_cast<za::SizeT>(index) * 8);
    }
    // A Quake .bsp whose 15 lumps all lie in the file.
    [[nodiscard]] bool valid() const
    {
        if(size < 124 || !(i32(0) == 29 || !memcmp(data, "BSP2", 4) || !memcmp(data, "2PSB", 4)))
        {
            return false;
        }
        for(int i = 0; i < 15; i++)
        {
            const int o = lumpOffset(i), l = lumpLength(i);
            if(o < 0 || l < 0 || static_cast<za::SizeT>(o) + static_cast<za::SizeT>(l) > size)
            {
                return false;
            }
        }
        return true;
    }
    [[nodiscard]] bool bsp29() const
    {
        return i32(0) == 29;
    }
};

// The .bsp's BSPX lumps (vis_maps.bspx_lumps): engines look for them right after the last of the 15 lumps.
struct BspxLump
{
    char name[24]{};
    int offset{0};
    int length{0};
};

// ---------------------------------------------------------------------------------------------------------------------
// Entities: their text's blocks and keys, read as relight_maps.py's regular expressions read them.

struct Key
{
    za::String name;
    za::String value;
    za::SizeT start{0}, end{0}; // the pair's text, [start, end)
};

// The {...} blocks with no brace inside (re r"\{[^{}]*\}"): [start, end) of each.
struct Span
{
    za::SizeT start, end;
};

// The map's own lights (map_lights): origin, whether it starts off (a "start off" light with a target name), its light.
struct MapLight
{
    V3 origin;
    bool off{false};
    double light{0.0};
};

// ---------------------------------------------------------------------------------------------------------------------
// relight_textures.cfg.

struct Rule
{
    za::String pattern; // game/map/texture, lower case; empty: a strength line
    bool hasKind{false}, hasScale{false}, hasLight{false}, hasColor{false}, hasReach{false};
    char kind[8]{};
    double scale{1.0}, light{0.0}, reach{1.0};
    V3 color;
    double strength[3]{1.0, 1.0, 1.0}; // a strength line's: fixture, glow, liquid
    bool hasStrength[3]{false, false, false};
};

inline constexpr const char* kindNames[] = {"fixture", "glow", "liquid", "off"};

// ---------------------------------------------------------------------------------------------------------------------
// The world's faces and contents (texture_faces, contents_at).

struct Face
{
    V3 centre;
    double area{0.0};
    V3 normal;
    za::Vector<V3> pts;
};

// The world's contents at a point (hull 0): -1 empty, -2 solid, -3 water... (contents_at).
struct World
{
    const Bsp* b{nullptr};

    [[nodiscard]] int contents(const V3& p) const
    {
        const int pofs = b->lumpOffset(1), plen = b->lumpLength(1) / 20;
        const int nofs = b->lumpOffset(5), nlen = b->lumpLength(5) / 24;
        const int lofs = b->lumpOffset(10), llen = b->lumpLength(10) / 28;
        int node = 0;
        for(int i = 0; i < 4096; i++)
        {
            if(node < 0 || node >= nlen)
            {
                return contentsEmpty;
            }
            const za::SizeT at = static_cast<za::SizeT>(nofs) + static_cast<za::SizeT>(node) * 24;
            const int planenum = b->i32(at), front = b->i16(at + 4), back = b->i16(at + 6);
            if(planenum < 0 || planenum >= plen)
            {
                return contentsEmpty;
            }
            const za::SizeT pa = static_cast<za::SizeT>(pofs) + static_cast<za::SizeT>(planenum) * 20;
            const double nx = b->f32(pa), ny = b->f32(pa + 4), nz = b->f32(pa + 8), dist = b->f32(pa + 12);
            const int child = p[0] * nx + p[1] * ny + p[2] * nz - dist >= 0 ? front : back;
            if(child < 0)
            {
                const int leaf = -child - 1;
                return leaf < llen ? b->i32(static_cast<za::SizeT>(lofs) + static_cast<za::SizeT>(leaf) * 28) : contentsEmpty;
            }
            node = child;
        }
        return contentsEmpty;
    }

    [[nodiscard]] bool solid(const V3& p) const
    {
        return contents(p) == contentsSolid;
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Where lights go (things, crowd, face_samples, recess_depth, fixture_spots, liquid_spots).

struct Thing
{
    za::Vector<const Face*> faces;
    double n{0.0};
};

// ---------------------------------------------------------------------------------------------------------------------
// Glow images, found as the engine finds them (relight_maps.py's Glows, which see).

struct Measured
{
    bool found{false};
    double share{0.0};
    V3 colour;
    char where[MAX_OSPATH]{};
};

// ---------------------------------------------------------------------------------------------------------------------
// glow_lights.

struct Strengths
{
    double scale{1.0};                  // all of them (Light Textures)
    double kinds[3]{1.0, 1.0, 1.0};     // fixtures, glows, liquids: the page's on top of the rules' strength lines
};

struct Glow
{
    za::String name;
    int kind{0}; // 0 fixture, 1 glow, 2 liquid
    double share{1.0};
    V3 colour;
    const za::Vector<Face>* faces{nullptr};
    za::Vector<Thing> things;
    Rule rule;
    char source[16]{};
    char where[MAX_OSPATH]{};
};

struct Report
{
    int textures{0};
    int lights{0};
    za::String lines; // a line each glowing texture (vr_relight_lights)
    double facesMs{0.0}, imagesMs{0.0}, spotsMs{0.0}; // where the time went
};

// ---------------------------------------------------------------------------------------------------------------------
// relight_maps.py's numbers (which see).

inline constexpr double surflight = 128.0;


[[nodiscard]] inline double sq(double x)
{
    return za::pow(x, 2.0); // (Python's x ** 2)
}

[[nodiscard]] inline bool isNear(const V3& c, const V3& d, double r)
{
    PySum s;
    for(int j = 0; j < 3; j++)
    {
        s.add(sq(c[j] - d[j]));
    }
    return s.value() < r * r;
}

[[nodiscard]] inline double dot(const V3& a, const V3& b)
{
    PySum s;
    for(int j = 0; j < 3; j++)
    {
        s.add(a[j] * b[j]);
    }
    return s.value();
}

[[nodiscard]] inline double selfDot(const V3& a) // sum(x * x for x in a)
{
    return dot(a, a);
}

[[nodiscard]] inline V3 along(const V3& p, const V3& n, double d) // tuple(p[j] + n[j] * d for j in range(3))
{
    return V3{{p[0] + n[0] * d, p[1] + n[1] * d, p[2] + n[2] * d}};
}

} // namespace qvr::relight::detail

namespace qvr::relight::detail
{

// vr_relight.cpp
[[nodiscard]] Measured cachedGlowImage(const char* map, const char* name);

// vr_relight_bsp.cpp
[[nodiscard]] za::String entitiesText(const Bsp& b);
[[nodiscard]] za::Vector<BspxLump> bspxLumps(const Bsp& b);
[[nodiscard]] za::Vector<unsigned char> withEntities(const Bsp& b, za::StringView entities, const vis::Patch* patch = nullptr);
[[nodiscard]] za::Vector<Key> keysOf(za::StringView text);
[[nodiscard]] const za::String* keyValue(const za::Vector<Key>& keys, const char* name);
[[nodiscard]] za::Vector<Span> blocksOf(za::StringView text);
[[nodiscard]] bool parseFloat(za::StringView word, double& out);
[[nodiscard]] za::Vector<za::StringView> wordsOf(za::StringView s);
[[nodiscard]] bool startsWith(za::StringView s, const char* prefix);
[[nodiscard]] za::Vector<MapLight> mapLights(za::StringView text);
[[nodiscard]] za::String idLightValues(za::StringView text);
[[nodiscard]] za::String withoutMapLightSettings(za::StringView text);
void appendNumber(za::String& out, double v);
[[nodiscard]] za::String scaledLights(za::StringView text, double mapScale, double sunScale, double minLight);
[[nodiscard]] bool fnmatch(const char* p, const char* s);
[[nodiscard]] za::Vector<Rule> loadRules(const char* text, const char* file);
[[nodiscard]] Rule textureRule(const za::Vector<Rule>& rules, const char* game, const char* map, const char* name);

// vr_relight_glow.cpp
[[nodiscard]] za::Vector<za::Vector<Face>> textureFaces(const Bsp& b, int textures);
[[nodiscard]] za::Vector<Thing> thingsOf(const za::Vector<Face>& faces);
[[nodiscard]] double crowd(const Thing& thing, const za::Vector<const Thing*>& everyone);
[[nodiscard]] za::Vector<V3> faceSamples(const Face& face, double step);
[[nodiscard]] double recessDepth(const Face& face, const World& world);
[[nodiscard]] za::Vector<V3> fixtureSpots(const za::Vector<const Face*>& faces, const World& world);
[[nodiscard]] za::Vector<V3> liquidSpots(const za::Vector<Face>& faces, const World& world);
[[nodiscard]] bool imageFile(const char* stem, char* where, size_t size);
[[nodiscard]] bool extmapsFile(const char* stem, char* where, size_t size);
[[nodiscard]] Measured measure(const char* load, const char* where);
[[nodiscard]] Measured glowImage(const char* map, const char* name);
void lightEntity(za::String& out, const V3& origin, double value, double wait, const char* colour, const char* extra);
[[nodiscard]] za::String glowLights(const Bsp& b, const byte* palette, const za::Vector<Rule>& rules, const char* game, const char* map, const Strengths& strength, Report& report);

} // namespace qvr::relight::detail
