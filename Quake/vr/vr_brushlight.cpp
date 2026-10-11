// vr_brushlight.cpp -- the runtime brushes' light (vr_brushlight.hpp): the faces' lightmaps laid out in their brush's
// rectangle, the flat light, and the bake from the map's lights (Quake's and ericw-tools' light formulas, traced).

#include "vr_brushlight.hpp"

#include "vr_brushes.hpp"
#include "vr_cvars.hpp"

#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::brushlight
{
namespace
{

constexpr double rangeScale = 0.5; // light.exe's (id's and ericw's -range): a light value of 256 is a lightmap byte of 128
constexpr double falloffUnit = 128.0; // ericw's LF_SCALE: the inverse falloffs' unit distance
constexpr double normalStyle = 264.0; // d_lightstylevalue of 'm' (a lightmap byte drawn at 264 / 256)
constexpr unsigned straightOn = 0xffff8080u; // a lux texel: the light along the normal
constexpr int worldOnly = -2; // (a brush id for the occlusion tests: the world alone, as the map's own light was made)

// ---------------------------------------------------------------------------------------------------------------------
// The map's lights (its entities), read once a world.

struct Light
{
    glm::dvec3 origin{0.0};
    double value{300.0};
    double atten{1.0}; // "wait"
    int delay{0};      // 0 linear, 1 1/x, 2 1/x^2, 3 none, 5 1/(x+128)^2 (4, a local minimum: not baked)
    glm::dvec3 colour{1.0};
    int style{0};
    double angleScale{0.5};
    bool spot{false};
    glm::dvec3 spotDir{0.0, 0.0, -1.0};
    double cosOuter{1.0}, cosInner{1.0};
    char target[64]{};
};

struct MapLights
{
    const qmodel_t* world{nullptr};
    za::Vector<Light> lights;
    bool sun{false};
    double sunValue{0.0};
    glm::dvec3 sunDir{0.0, 0.0, -1.0}; // the way it shines
    glm::dvec3 sunColour{1.0};
    double sunAngleScale{0.5};
    glm::dvec3 minlight{0.0}; // light units
};
MapLights mapLights;

[[nodiscard]] const qmodel_t* currentWorld()
{
    return sv.worldmodel ? sv.worldmodel : cl.worldmodel;
}

[[nodiscard]] glm::dvec3 parseVec(const char* s)
{
    double v[3]{0.0, 0.0, 0.0};
    (void)sscanf(s, "%lf %lf %lf", &v[0], &v[1], &v[2]);
    return glm::dvec3{v[0], v[1], v[2]};
}

// "_color": 0..1 or 0..255.
[[nodiscard]] glm::dvec3 parseColour(const char* s)
{
    glm::dvec3 c = parseVec(s);
    if(c.x > 1.0 || c.y > 1.0 || c.z > 1.0)
    {
        c /= 255.0;
    }
    return glm::clamp(c, glm::dvec3{0.0}, glm::dvec3{1.0});
}

// ericw's vec_from_mangle: yaw, then pitch up (90 straight up, -90 straight down).
[[nodiscard]] glm::dvec3 fromMangle(const glm::dvec3& m)
{
    const double y = glm::radians(m.x), p = glm::radians(m.y);
    return glm::dvec3{za::cos(p) * za::cos(y), za::cos(p) * za::sin(y), za::sin(p)};
}

const MapLights& lights()
{
    MapLights& m = mapLights;
    const qmodel_t* w = currentWorld();
    if(m.world == w)
    {
        return m;
    }
    m = MapLights{};
    m.world = w;
    if(!w || !w->entities)
    {
        return m;
    }
    struct Named
    {
        char name[64];
        glm::dvec3 origin;
    };
    za::Vector<Named> targets;
    const char* data = w->entities;
    bool first = true;
    while((data = COM_Parse(data)) && com_token[0] == '{')
    {
        char classname[64] = "", targetname[64] = "";
        Light l;
        bool hasOrigin = false, surface = false, sunEntity = false, haveAngle = false, haveMangle = false;
        double angle = 40.0, softAngle = 0.0;
        glm::dvec3 mangle{0.0};
        double sunlight = 0.0, minlight = -1.0, anglescale = -1.0;
        glm::dvec3 sunMangle{0.0, -90.0, 0.0}, sunColour{1.0}, minColour{1.0};
        while((data = COM_Parse(data)) && com_token[0] != '}')
        {
            char key[64];
            q_strlcpy(key, com_token, sizeof(key));
            if(!(data = COM_Parse(data)))
            {
                break;
            }
            const char* v = com_token;
            if(!strcmp(key, "classname")) { q_strlcpy(classname, v, sizeof(classname)); }
            else if(!strcmp(key, "origin")) { l.origin = parseVec(v); hasOrigin = true; }
            else if(!strcmp(key, "light") || !strcmp(key, "_light"))
            {
                // (a "_light" of four numbers: Half-Life's colour and brightness; its last)
                double n[4]{0.0, 0.0, 0.0, 0.0};
                const int got = sscanf(v, "%lf %lf %lf %lf", &n[0], &n[1], &n[2], &n[3]);
                l.value = got == 4 ? n[3] : n[0];
                minlight = l.value; // (worldspawn's "light": its minimum)
            }
            else if(!strcmp(key, "wait")) { l.atten = atof(v); }
            else if(!strcmp(key, "delay")) { l.delay = atoi(v); }
            else if(!strcmp(key, "_color") || !strcmp(key, "color")) { l.colour = parseColour(v); minColour = l.colour; }
            else if(!strcmp(key, "style")) { l.style = atoi(v); }
            else if(!strcmp(key, "angle")) { angle = atof(v); haveAngle = true; }
            else if(!strcmp(key, "_softangle")) { softAngle = atof(v); }
            else if(!strcmp(key, "mangle") || !strcmp(key, "_mangle")) { mangle = parseVec(v); haveMangle = true; }
            else if(!strcmp(key, "target")) { q_strlcpy(l.target, v, sizeof(l.target)); }
            else if(!strcmp(key, "targetname")) { q_strlcpy(targetname, v, sizeof(targetname)); }
            else if(!strcmp(key, "_surface")) { surface = v[0] != '\0'; }
            else if(!strcmp(key, "_sun")) { sunEntity = atoi(v) != 0; }
            else if(!strcmp(key, "_anglescale") || !strcmp(key, "_anglesense")) { anglescale = atof(v); }
            else if(!strcmp(key, "_sunlight") || !strcmp(key, "_sun_light")) { sunlight = atof(v); }
            else if(!strcmp(key, "_sun_mangle") || !strcmp(key, "_sunlight_mangle")) { sunMangle = parseVec(v); }
            else if(!strcmp(key, "_sunlight_color") || !strcmp(key, "_sun_color")) { sunColour = parseColour(v); }
            else if(!strcmp(key, "_minlight")) { minlight = atof(v); }
            else if(!strcmp(key, "_minlight_color")) { minColour = parseColour(v); }
        }
        if(!data)
        {
            break;
        }
        if(first)
        {
            // worldspawn: the sun, the minimum, the angle term for every light
            first = false;
            m.sunAngleScale = anglescale >= 0.0 ? anglescale : 0.5;
            if(sunlight > 0.0)
            {
                m.sun = true;
                m.sunValue = sunlight;
                m.sunDir = fromMangle(sunMangle);
                m.sunColour = sunColour;
            }
            if(minlight > 0.0)
            {
                m.minlight = minColour * minlight;
            }
            continue;
        }
        if(targetname[0] && hasOrigin)
        {
            Named n{};
            q_strlcpy(n.name, targetname, sizeof(n.name));
            n.origin = l.origin;
            targets.pushBack(n);
        }
        if(strncmp(classname, "light", 5) != 0 || surface || sunEntity || !hasOrigin || l.value <= 0.0 || l.delay == 4)
        {
            continue;
        }
        l.atten = l.atten > 0.0 ? l.atten : 1.0;
        l.angleScale = anglescale >= 0.0 ? anglescale : m.sunAngleScale;
        l.style = za::clamp(l.style, 0, 254);
        if(haveMangle)
        {
            l.spot = true;
            l.spotDir = fromMangle(mangle);
        }
        if(l.spot || l.target[0])
        {
            l.cosOuter = za::cos(glm::radians(za::clamp(haveAngle ? angle : 40.0, 1.0, 179.0) * 0.5));
            l.cosInner = softAngle > 0.0 ? za::cos(glm::radians(za::clamp(softAngle, 1.0, 179.0) * 0.5)) : l.cosOuter;
        }
        m.lights.pushBack(l);
    }
    // Spotlights by "target": their way to the target's origin.
    for(Light& l : m.lights)
    {
        if(l.spot || !l.target[0])
        {
            continue;
        }
        for(const Named& n : targets)
        {
            if(!strcmp(n.name, l.target) && glm::length(n.origin - l.origin) > 1e-3)
            {
                l.spot = true;
                l.spotDir = glm::normalize(n.origin - l.origin);
                break;
            }
        }
    }
    return m;
}

// A light's value at `dist` units (before the angle term), Quake's and ericw's falloffs.
[[nodiscard]] double lightValue(const Light& l, double dist)
{
    const double d = za::max(dist * l.atten, 1.0);
    switch(l.delay)
    {
    case 0: return l.value - d;
    case 1: return l.value / (d / falloffUnit);
    case 2: return l.value / ((d / falloffUnit) * (d / falloffUnit));
    case 3: return l.value;
    case 5:
    {
        const double e = (d + falloffUnit) / falloffUnit;
        return l.value / (e * e);
    }
    default: return 0.0;
    }
}

// The first contents along a..b through the world's hull 0 that light doesn't cross (solid or sky: Quake's trace
// crosses sky); CONTENTS_EMPTY if none (liquids let it through).
[[nodiscard]] int firstContents(const hull_t* hull, int num, const glm::dvec3& p1, const glm::dvec3& p2)
{
    while(num >= 0)
    {
        const mclipnode_t& node = hull->clipnodes[num];
        const mplane_t& plane = hull->planes[node.planenum];
        const glm::dvec3 n{plane.normal[0], plane.normal[1], plane.normal[2]};
        const double t1 = glm::dot(n, p1) - plane.dist, t2 = glm::dot(n, p2) - plane.dist;
        if(t1 >= 0.0 && t2 >= 0.0)
        {
            num = node.children[0];
            continue;
        }
        if(t1 < 0.0 && t2 < 0.0)
        {
            num = node.children[1];
            continue;
        }
        const int side = t1 < 0.0 ? 1 : 0;
        const glm::dvec3 mid = p1 + (p2 - p1) * (t1 / (t1 - t2));
        const int c = firstContents(hull, node.children[side], p1, mid);
        if(c != CONTENTS_EMPTY)
        {
            return c;
        }
        return firstContents(hull, node.children[side ^ 1], mid, p2);
    }
    return num == CONTENTS_SOLID || num == CONTENTS_SKY ? num : CONTENTS_EMPTY;
}

[[nodiscard]] int worldFirst(const glm::dvec3& a, const glm::dvec3& b)
{
    const qmodel_t* w = currentWorld();
    return w ? firstContents(&w->hulls[0], w->hulls[0].firstclipnode, a, b) : CONTENTS_EMPTY;
}

// True if nothing solid (the world's, its sky, then the other brushes) is in the way from `a` to `b`.
[[nodiscard]] bool clearLine(const glm::dvec3& a, const glm::dvec3& b, int brush)
{
    return worldFirst(a, b) == CONTENTS_EMPTY && (brush == worldOnly || !brushes::blocks(glm::vec3{a}, glm::vec3{b}, brush));
}

[[nodiscard]] int contentsAt(const glm::dvec3& p)
{
    const qmodel_t* w = currentWorld();
    if(!w)
    {
        return CONTENTS_EMPTY;
    }
    vec3_t q{static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)};
    return SV_HullPointContents(const_cast<hull_t*>(&w->hulls[0]), w->hulls[0].firstclipnode, q);
}

// The sun at `p` (off a face whose normal is `n`): seen, its light (light units) and its way; false if not.
[[nodiscard]] bool sunAt(const MapLights& m, const glm::dvec3& p, const glm::dvec3& n, int brush, glm::dvec3& light)
{
    if(!m.sun)
    {
        return false;
    }
    const glm::dvec3 toSun = -m.sunDir;
    const double c = glm::dot(n, toSun);
    if(c <= 0.0)
    {
        return false;
    }
    // (the sun is seen if the way to it meets the sky before anything solid)
    const glm::dvec3 farPoint = p + toSun * 16384.0;
    if(worldFirst(p, farPoint) != CONTENTS_SKY || (brush != worldOnly && brushes::blocks(glm::vec3{p}, glm::vec3{farPoint}, brush)))
    {
        return false;
    }
    light = m.sunColour * (m.sunValue * ((1.0 - m.sunAngleScale) + m.sunAngleScale * c));
    return true;
}

// Light `l`'s share at `p` (off a face whose normal is `n`), light units, unshadowed; 0: none.
[[nodiscard]] double share(const Light& l, const glm::dvec3& p, const glm::dvec3& n, glm::dvec3& way)
{
    const glm::dvec3 d = l.origin - p;
    const double dist = glm::length(d);
    if(dist < 1e-3)
    {
        return 0.0;
    }
    way = d / dist;
    double v = lightValue(l, dist);
    if(v <= 0.0)
    {
        return 0.0;
    }
    if(l.spot)
    {
        const double c = glm::dot(l.spotDir, -way);
        if(c < l.cosOuter)
        {
            return 0.0;
        }
        if(c < l.cosInner && l.cosInner > l.cosOuter)
        {
            v *= (c - l.cosOuter) / (l.cosInner - l.cosOuter);
        }
    }
    const double angle = (1.0 - l.angleScale) + l.angleScale * glm::dot(n, way);
    return angle > 0.0 ? v * angle : 0.0;
}

// The lights that may reach a face (in front of its plane, in reach of its polygon's sphere): their indices; and its
// styles (0 first, up to four: a light of a fifth is left out).
void candidates(const Face& f, const MapLights& m, za::Vector<int>& out, za::U8 (&styles)[4], int& numStyles)
{
    out.clear();
    styles[0] = 0;
    styles[1] = styles[2] = styles[3] = 255;
    numStyles = 1;
    glm::dvec3 c{0.0};
    for(const glm::dvec3& p : f.poly)
    {
        c += p / static_cast<double>(f.poly.size());
    }
    double r = 0.0;
    for(const glm::dvec3& p : f.poly)
    {
        r = za::max(r, glm::length(p - c));
    }
    for(za::SizeT i = 0; i < m.lights.size(); i++)
    {
        const Light& l = m.lights[i];
        if(glm::dot(f.normal, l.origin) - f.dist <= 0.0)
        {
            continue; // (behind the face: id's light skips it)
        }
        const double nearest = za::max(glm::length(l.origin - c) - r, 1.0);
        if(lightValue(l, nearest) * rangeScale < 0.5)
        {
            continue;
        }
        int s = 0;
        while(s < numStyles && styles[s] != l.style)
        {
            s++;
        }
        if(s == numStyles)
        {
            if(numStyles == 4)
            {
                continue;
            }
            styles[numStyles++] = static_cast<za::U8>(l.style);
        }
        out.pushBack(static_cast<int>(i));
    }
}

// The averages the lightstyles have (d_lightstylevalue's units: 'm' 264), and back.
struct StyleAverages
{
    int saved[MAX_LIGHTSTYLES]{};
};
StyleAverages styleAverages;

[[nodiscard]] double styleAverage(int style)
{
    if(style < 0 || style >= MAX_LIGHTSTYLES || !cl_lightstyle[style].length)
    {
        return 256.0;
    }
    return static_cast<double>((cl_lightstyle[style].average - 'a') * 22);
}

// The world's light at `p` (R_LightPoint: the lightmap under it), every style at its average; lightmap units as a
// style at 'm' draws them.
[[nodiscard]] glm::dvec3 worldLight(const glm::dvec3& p)
{
    if(!cl.worldmodel)
    {
        return glm::dvec3{0.0};
    }
    for(int j = 0; j < MAX_LIGHTSTYLES; j++)
    {
        styleAverages.saved[j] = d_lightstylevalue[j];
        d_lightstylevalue[j] = static_cast<int>(styleAverage(j));
    }
    vec3_t q{static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)};
    lightcache_t cache{};
    const int level = R_LightPoint(q, 0.f, &cache);
    for(int j = 0; j < MAX_LIGHTSTYLES; j++)
    {
        d_lightstylevalue[j] = styleAverages.saved[j];
    }
    if(level <= 0 && lightcolor[0] <= 0.f && lightcolor[1] <= 0.f && lightcolor[2] <= 0.f)
    {
        return glm::dvec3{-1.0}; // (over nothing lit: the void, the sky)
    }
    return glm::dvec3{lightcolor[0], lightcolor[1], lightcolor[2]} * (256.0 / normalStyle);
}

[[nodiscard]] unsigned pack(const glm::dvec3& c)
{
    const auto channel = [](double v) { return static_cast<unsigned>(za::clamp(v, 0.0, 255.0) + 0.5); };
    return channel(c.r) | (channel(c.g) << 8) | (channel(c.b) << 16) | 0xff000000u;
}

[[nodiscard]] glm::dvec3 centreOf(const Face& f)
{
    glm::dvec3 c{0.0};
    for(const glm::dvec3& p : f.poly)
    {
        c += p / static_cast<double>(f.poly.empty() ? 1 : f.poly.size());
    }
    return c;
}

// ---------------------------------------------------------------------------------------------------------------------
// The bakes.

struct Job
{
    int id{0};
    za::U32 serial{0};
    za::Vector<Face> faces;
    za::Vector<Map> maps;
    Texels out;
    int face{-1};  // the face being baked (-1: none begun)
    int luxel{0};  // its next luxel
    // The face being baked:
    za::Vector<int> lights;      // its lights (MapLights indices)
    glm::dmat3 toWorld{1.0};     // (s, t, plane distance) -> the point
    za::Vector<glm::dvec4> edges; // its polygon's edges' inward planes (in its plane)
    glm::dvec3 centre{0.0};
    glm::dvec3 residual{0.0};    // lightmap units added to its first style
    glm::dvec3 lux0{1.0, 0.0, 0.0}, lux1{0.0, 1.0, 0.0}; // the lux frame's first two axes
    double ms{0.0};
    int luxels{0}, traces{0};
};

struct Bakes
{
    za::Vector<za::UniquePtr<Job>> queue;
    int done{0};
    double ms{0.0};
    int luxels{0}, traces{0};
};
Bakes bakes;

// The face's light at the floor under a point off its middle that the lights (and sun, minimum) don't explain.
[[nodiscard]] glm::dvec3 residualFor(const Face& f, const MapLights& m, int brush)
{
    if(vr_brush_bake_fill.value <= 0.f)
    {
        return glm::dvec3{0.0};
    }
    const glm::dvec3 p = centreOf(f) + f.normal * 4.0;
    const glm::dvec3 have = worldLight(p);
    if(have.x < 0.0)
    {
        return glm::dvec3{0.0};
    }
    const qmodel_t* w = currentWorld();
    trace_t tr;
    memset(&tr, 0, sizeof(tr));
    tr.fraction = 1.f;
    tr.allsolid = true;
    vec3_t p1{static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)};
    vec3_t p2{p1[0], p1[1], p1[2] - 8192.f};
    SV_RecursiveHullCheck(const_cast<hull_t*>(&w->hulls[0]), w->hulls[0].firstclipnode, 0.f, 1.f, p1, p2, &tr);
    if(tr.startsolid || tr.fraction >= 1.f)
    {
        return glm::dvec3{0.0};
    }
    const glm::dvec3 n{tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2]};
    const glm::dvec3 at = glm::dvec3{tr.endpos[0], tr.endpos[1], tr.endpos[2]} + n * 1.0;
    glm::dvec3 sum{0.0}, way;
    for(const Light& l : m.lights)
    {
        if(glm::dot(n, l.origin - at) <= 0.0)
        {
            continue;
        }
        const double v = share(l, at, n, way);
        if(v * rangeScale >= 0.5 && clearLine(at, l.origin, worldOnly))
        {
            sum += l.colour * (v * styleAverage(l.style) / normalStyle);
        }
    }
    glm::dvec3 sun;
    if(sunAt(m, at, n, worldOnly, sun))
    {
        sum += sun;
    }
    sum = glm::max(sum, m.minlight) * rangeScale;
    (void)brush;
    return glm::max(have - sum, glm::dvec3{0.0}) * static_cast<double>(vr_brush_bake_fill.value);
}

void beginFace(Job& j)
{
    const Face& f = j.faces[static_cast<za::SizeT>(j.face)];
    const Map& map = j.maps[static_cast<za::SizeT>(j.face)];
    const MapLights& m = lights();
    za::U8 styles[4];
    int numStyles = 0;
    candidates(f, m, j.lights, styles, numStyles);
    // (the layout's styles are the ones baked: a light of another style left out)
    za::SizeT keep = 0;
    for(za::SizeT i = 0; i < j.lights.size(); i++)
    {
        const int style = m.lights[static_cast<za::SizeT>(j.lights[i])].style;
        bool in = false;
        for(int s = 0; s < map.numStyles; s++)
        {
            in = in || map.styles[s] == style;
        }
        if(in)
        {
            j.lights[keep++] = j.lights[i];
        }
    }
    j.lights.resize(keep);
    const glm::dmat3 rows{glm::dvec3{f.s}, glm::dvec3{f.t}, f.normal};
    j.toWorld = glm::inverse(glm::transpose(rows));
    j.centre = centreOf(f);
    j.edges.clear();
    for(za::SizeT i = 0; i < f.poly.size(); i++)
    {
        const glm::dvec3 a = f.poly[i], b = f.poly[(i + 1) % f.poly.size()];
        glm::dvec3 in = glm::cross(f.normal, b - a);
        if(glm::length(in) < 1e-9)
        {
            continue;
        }
        in = glm::normalize(in);
        if(glm::dot(in, j.centre - a) < 0.0)
        {
            in = -in;
        }
        j.edges.pushBack(glm::dvec4{in, glm::dot(in, a)});
    }
    j.residual = residualFor(f, m, j.id);
    // The lux frame (VR_FillSurfaceLux's): the texture's s axis on the face, the normal crossed with it, the normal.
    glm::dvec3 t = glm::dvec3{f.s} - f.normal * glm::dot(glm::dvec3{f.s}, f.normal);
    t = glm::length(t) > 1e-6 ? glm::normalize(t) : glm::dvec3{1.0, 0.0, 0.0};
    j.lux0 = t;
    j.lux1 = glm::cross(f.normal, t);
    j.luxel = 0;
}

// The point a luxel lights: its place on the face's plane, kept inside its polygon (a luxel past an edge lit as the
// edge is), a unit off the face; moved towards the face's middle while it is in the world's solid.
[[nodiscard]] glm::dvec3 luxelPoint(const Job& j, const Face& f, const Map& map, int i, int k)
{
    const glm::dvec3 rhs{map.s0 + i * map.step - f.s.w, map.t0 + k * map.step - f.t.w, f.dist};
    glm::dvec3 p = j.toWorld * rhs;
    for(int pass = 0; pass < 3; pass++)
    {
        for(const glm::dvec4& e : j.edges)
        {
            const double d = glm::dot(glm::dvec3{e}, p) - e.w;
            if(d < 0.5)
            {
                p += glm::dvec3{e} * (0.5 - d);
            }
        }
    }
    glm::dvec3 q = p + f.normal;
    for(int tries = 0; tries < 6 && contentsAt(q) == CONTENTS_SOLID; tries++)
    {
        p += (j.centre - p) * 0.35;
        q = p + f.normal;
    }
    return q;
}

// One luxel of the face being baked (all its styles), into the job's texels.
void bakeLuxel(Job& j)
{
    const Face& f = j.faces[static_cast<za::SizeT>(j.face)];
    const Map& map = j.maps[static_cast<za::SizeT>(j.face)];
    const MapLights& m = lights();
    const int i = j.luxel % map.w, k = j.luxel / map.w;
    const glm::dvec3 q = luxelPoint(j, f, map, i, k);
    glm::dvec3 acc[4]{glm::dvec3{0.0}, glm::dvec3{0.0}, glm::dvec3{0.0}, glm::dvec3{0.0}};
    glm::dvec3 dir{0.0}, way;
    const bool solid = contentsAt(q) == CONTENTS_SOLID;
    for(const int li : j.lights)
    {
        if(solid)
        {
            break;
        }
        const Light& l = m.lights[static_cast<za::SizeT>(li)];
        const double v = share(l, q, f.normal, way);
        if(v * rangeScale < 0.25)
        {
            continue;
        }
        j.traces++;
        if(!clearLine(q, l.origin, j.id))
        {
            continue;
        }
        int s = 0;
        while(s < map.numStyles && map.styles[s] != l.style)
        {
            s++;
        }
        if(s < map.numStyles)
        {
            acc[s] += l.colour * v;
            dir += way * (v * (l.colour.r + l.colour.g + l.colour.b));
        }
    }
    glm::dvec3 sun;
    if(!solid && sunAt(m, q, f.normal, j.id, sun))
    {
        acc[0] += sun;
        dir += -m.sunDir * (sun.r + sun.g + sun.b);
    }
    acc[0] = glm::max(acc[0], m.minlight);
    for(int s = 0; s < map.numStyles; s++)
    {
        glm::dvec3 bytes = acc[s] * rangeScale;
        if(s == 0)
        {
            bytes += j.residual;
        }
        const int x = map.x + s * map.w + i, y = map.y + k;
        if(x >= 0 && x < rectWidth && y >= 0 && y < rectHeight)
        {
            j.out.light[static_cast<za::SizeT>(y * rectWidth + x)] = pack(bytes);
        }
    }
    // (the light's way where the first style is: the shader reads one)
    const int x = map.x + i, y = map.y + k;
    if(x >= 0 && x < rectWidth && y >= 0 && y < rectHeight)
    {
        unsigned luxel = straightOn;
        if(glm::length(dir) > 1e-6)
        {
            const glm::dvec3 d = glm::normalize(dir);
            const auto enc = [](double v) { return static_cast<unsigned>(za::clamp(v * 127.5 + 127.5, 0.0, 255.0) + 0.5); };
            luxel = enc(glm::dot(d, j.lux0)) | (enc(glm::dot(d, j.lux1)) << 8) | (enc(glm::dot(d, f.normal)) << 16) | 0xff000000u;
        }
        j.out.lux[static_cast<za::SizeT>(y * rectWidth + x)] = luxel;
    }
    j.luxels++;
    j.luxel++;
}

} // namespace

void layout(const za::Vector<Face>& faces, za::Vector<Map>& out)
{
    const MapLights& m = lights();
    out.clear();
    out.resize(faces.size());
    za::Vector<int> scratch;
    // (each face's styles and s, t ranges once)
    za::Vector<glm::dvec4> ranges;
    for(za::SizeT k = 0; k < faces.size(); k++)
    {
        const Face& f = faces[k];
        candidates(f, m, scratch, out[k].styles, out[k].numStyles);
        glm::dvec4 r{1e30, -1e30, 1e30, -1e30};
        for(const glm::dvec3& p : f.poly)
        {
            const double s = glm::dot(p, glm::dvec3{f.s}) + f.s.w, t = glm::dot(p, glm::dvec3{f.t}) + f.t.w;
            r = glm::dvec4{za::min(r.x, s), za::max(r.y, s), za::min(r.z, t), za::max(r.w, t)};
        }
        ranges.pushBack(r);
    }
    for(double step = za::clamp(static_cast<double>(vr_brush_luxel.value), 2.0, 64.0); step <= 65536.0; step *= 2.0)
    {
        for(za::SizeT k = 0; k < faces.size(); k++)
        {
            Map& mp = out[k];
            const glm::dvec4& r = ranges[k];
            const double bs = za::floor(r.x / step), bt = za::floor(r.z / step);
            mp.step = step;
            mp.s0 = bs * step;
            mp.t0 = bt * step;
            mp.w = static_cast<int>(za::ceil(r.y / step) - bs) + 1;
            mp.h = static_cast<int>(za::ceil(r.w / step) - bt) + 1;
        }
        if(pack(out))
        {
            return;
        }
    }
}

bool pack(za::Vector<Map>& maps)
{
    // Shelves, the tallest first.
    za::Vector<int> order;
    for(za::SizeT k = 0; k < maps.size(); k++)
    {
        order.pushBack(static_cast<int>(k));
    }
    za::stableSort(order.begin(), order.end(), [&](int a, int b) { return maps[static_cast<za::SizeT>(a)].h > maps[static_cast<za::SizeT>(b)].h; });
    int x = 0, y = 0, shelf = 0;
    for(const int k : order)
    {
        Map& mp = maps[static_cast<za::SizeT>(k)];
        const int w = mp.w * mp.numStyles;
        if(w > rectWidth || mp.h > rectHeight)
        {
            return false;
        }
        if(x + w > rectWidth)
        {
            x = 0;
            y += shelf;
            shelf = 0;
        }
        if(y + mp.h > rectHeight)
        {
            return false;
        }
        mp.x = x;
        mp.y = y;
        x += w;
        shelf = za::max(shelf, mp.h);
    }
    return true;
}

glm::vec3 flatLight(const Face& f)
{
    glm::dvec3 c = worldLight(centreOf(f) + f.normal * 4.0);
    if(c.x < 0.0)
    {
        c = glm::dvec3{24.0};
    }
    const double n = f.normal.z;
    const double shade = n >= 0.0 ? 0.8 + 0.2 * n : 0.8 + 0.25 * n;
    c *= 1.0 + (shade - 1.0) * za::clamp(static_cast<double>(vr_brush_shade.value), 0.0, 1.0);
    return glm::vec3{c};
}

void fill(const za::Vector<Map>& maps, const za::Vector<glm::vec3>& colours, Texels& out)
{
    out.light.clear();
    out.light.resize(static_cast<za::SizeT>(rectWidth * rectHeight), 0xff000000u);
    out.lux.clear();
    out.lux.resize(static_cast<za::SizeT>(rectWidth * rectHeight), straightOn);
    for(za::SizeT k = 0; k < maps.size() && k < colours.size(); k++)
    {
        const Map& m = maps[k];
        const unsigned c = pack(glm::dvec3{colours[k]});
        for(int y = m.y; y < m.y + m.h && y < rectHeight; y++)
        {
            for(int x = m.x; x < m.x + m.w && x < rectWidth; x++)
            {
                out.light[static_cast<za::SizeT>(y * rectWidth + x)] = c;
            }
        }
    }
}

void queue(int id, za::U32 serial, const za::Vector<Face>& faces, const za::Vector<Map>& maps)
{
    cancel(id);
    za::UniquePtr<Job> j = za::makeUnique<Job>();
    j->id = id;
    j->serial = serial;
    j->faces = faces;
    j->maps = maps;
    j->out.light.resize(static_cast<za::SizeT>(rectWidth * rectHeight), 0xff000000u);
    j->out.lux.resize(static_cast<za::SizeT>(rectWidth * rectHeight), straightOn);
    bakes.queue.pushBack(ZA_MOVE(j));
}

void cancel(int id)
{
    for(za::SizeT i = 0; i < bakes.queue.size(); i++)
    {
        if(bakes.queue[i]->id == id)
        {
            bakes.queue.erase(bakes.queue.begin() + i);
            return;
        }
    }
}

void clear()
{
    bakes.queue.clear();
    mapLights = MapLights{};
}

void run(double ms, za::FunctionRef<void(int id, za::U32 serial, const Texels& texels)> done)
{
    if(bakes.queue.empty() || !currentWorld() || !cl.worldmodel)
    {
        return;
    }
    const double t0 = Sys_DoubleTime(), until = t0 + ms * 0.001;
    while(!bakes.queue.empty())
    {
        Job& j = *bakes.queue[0];
        const double jt0 = Sys_DoubleTime();
        bool finished = false;
        for(int n = 0;; n++)
        {
            if(j.face < 0 || j.luxel >= j.maps[static_cast<za::SizeT>(j.face)].w * j.maps[static_cast<za::SizeT>(j.face)].h)
            {
                j.face++;
                if(j.face >= static_cast<int>(j.faces.size()))
                {
                    finished = true;
                    break;
                }
                beginFace(j);
                continue;
            }
            bakeLuxel(j);
            if((n & 7) == 7 && Sys_DoubleTime() >= until)
            {
                break;
            }
        }
        j.ms += (Sys_DoubleTime() - jt0) * 1000.0;
        if(!finished)
        {
            return;
        }
        bakes.done++;
        bakes.ms += j.ms;
        bakes.luxels += j.luxels;
        bakes.traces += j.traces;
        if(developer.value || vr_brush_bake_report.value)
        {
            Con_Printf("brush light: %d baked, %d faces, %d luxels, %d traces in %.1f ms\n", j.id, static_cast<int>(j.faces.size()),
                j.luxels, j.traces, j.ms);
        }
        const za::UniquePtr<Job> job = ZA_MOVE(bakes.queue[0]);
        bakes.queue.erase(bakes.queue.begin());
        done(job->id, job->serial, job->out);
        if(Sys_DoubleTime() >= until)
        {
            return;
        }
    }
}

int pending()
{
    return static_cast<int>(bakes.queue.size());
}

void listLights(const glm::vec3* nearPoint, bool onlyStyled)
{
    const MapLights& m = lights();
    int styled = 0;
    for(const Light& l : m.lights)
    {
        styled += l.style != 0 ? 1 : 0;
    }
    Con_Printf("brush lights: %d (%d of a style), sun %s %.0f, minimum %.0f; bakes: %d done (%d luxels, %d traces, %.1f ms), %d queued\n",
        static_cast<int>(m.lights.size()), styled, m.sun ? "on" : "off", m.sunValue, m.minlight.x, bakes.done, bakes.luxels,
        bakes.traces, bakes.ms, pending());
    for(const Light& l : m.lights)
    {
        if(onlyStyled && l.style != 0)
        {
            Con_Printf("  light at %.0f %.0f %.0f: %.0f, style %d (%s)\n", l.origin.x, l.origin.y, l.origin.z, l.value, l.style,
                l.style < MAX_LIGHTSTYLES && cl_lightstyle[l.style].length ? cl_lightstyle[l.style].map : "-");
        }
    }
    if(!nearPoint)
    {
        return;
    }
    za::Vector<int> order;
    for(za::SizeT i = 0; i < m.lights.size(); i++)
    {
        order.pushBack(static_cast<int>(i));
    }
    const glm::dvec3 p{*nearPoint};
    if(m.sun && currentWorld())
    {
        // (the way to the sun from there: the first thing on it, the sky or a solid)
        glm::dvec3 sun{0.0};
        const bool seen = sunAt(m, p, -m.sunDir, -1, sun);
        Con_Printf("  sun from %.2f %.2f %.2f: the way meets %s; a face facing it there gets %.0f\n", -m.sunDir.x, -m.sunDir.y, -m.sunDir.z,
            worldFirst(p, p - m.sunDir * 16384.0) == CONTENTS_SKY ? "the sky" : "a solid", seen ? sun.x * rangeScale : 0.0);
    }
    za::stableSort(order.begin(), order.end(), [&](int a, int b) {
        return glm::length(m.lights[static_cast<za::SizeT>(a)].origin - p) < glm::length(m.lights[static_cast<za::SizeT>(b)].origin - p);
    });
    for(za::SizeT i = 0; i < order.size() && i < 5; i++)
    {
        const Light& l = m.lights[static_cast<za::SizeT>(order[i])];
        Con_Printf("  light at %.0f %.0f %.0f: %.0f, wait %.2f, delay %d, style %d%s, %.0f away\n", l.origin.x, l.origin.y, l.origin.z,
            l.value, l.atten, l.delay, l.style, l.spot ? ", spot" : "", glm::length(l.origin - p));
    }
}

} // namespace qvr::brushlight
