// vr_relight.cpp -- see vr_relight.hpp.
//
// Relighting a map in the game (vr_relight; VR Settings > Advanced VR Options > Graphics > Relighting): the map's
// .bsp (the copy relight_maps.py made in quakevr/relit, with its see-through water, if there is one; else the map
// itself, as the game finds it) is given ericw-tools' `light` with:
//
// - the lights relight_maps.py gives the glowing textures (glowLights: a port of its glow_lights, which see): the
//   lamps and light panels (fixtures), glowing buttons and panels, lava and slime, by the rules of
//   quakevr/relight_textures.cfg, their glow found where the engine finds it (Glows: fullbright pixels, a replacement
//   texture's _glow or _luma, the material maps'), each kind as bright as the page's sliders say on top of the rules'
//   strengths (vr_relight_strength, _lamps, _glows, _liquids);
// - the map's own lights, as bright as Map Lights says (vr_relight_maplights: "light" and the linear falloff's "wait"
//   scaled together: as bright, as far), and its sunlight as Sunlight says (vr_relight_sunlight);
// - the look's options: ambient occlusion (vr_relight_ao: -dirt -dirtscale), bounced light (vr_relight_bounce),
//   a minimum light (vr_relight_minlight), smooth or fast shadows (vr_relight_quality: -extra4 or -extra), the light
//   grid and the light directions (-lightgrid -lux) and coloured light (-lit), as relight_maps.py gives them.
//
// light runs as a process of its own (vr_relight_process.cpp), below normal priority, on the copy in the work folder
// (<game folder>/relit_custom/_work); its output is read for the progress the page shows. When it ends, the map keeps
// its own entities again (the lights were for light only) and the result goes to <game folder>/relit_custom/<game>/
// maps/<map>.bsp, .lit and .lux, which the engine loads over relit/ (VR_ModelFile; vr_relight_use 0: not), with a
// <map>.relight saying how it was made. id's paks and the game folders' maps are only read. Then the map is reloaded
// where you are (vr_relight_reload: a quick save and load; where the game cannot save, the map restarted).
//
// relight_maps.py is the reference: with the page's settings at their defaults the lights given to light are the
// script's (vr_relight_lights writes them, to compare). What it does that this does not: the water-vis patch
// (VisPatch: done by the script, kept from its copy) and the BSP2 maps' glowing textures (neither lights those).

#include "vr_relight.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
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

extern "C" {
void VR_HeapFree(void* pointer);                                       // vr_alloccount.cpp
void VR_FileCacheEnable(int on);                                       // vr_fscache.cpp
int VR_MapGameFolder(const char* name, char* out, size_t size);        // vr_gamedir.cpp
}

namespace qvr::relight
{

namespace process // vr_relight_process.cpp
{
bool start(const za::String& exe, const za::Vector<za::String>& args, const za::String& cwd, const za::String& log,
    za::String& error);
bool running();
int lastExitCode();
void stop();
za::String onPath(const char* name);
} // namespace process

namespace
{

// ---------------------------------------------------------------------------------------------------------------------
// relight_maps.py's numbers (which see).

constexpr double surflight = 128.0;
constexpr double glowObject = 64.0;
constexpr double glowRoom = 256.0;
constexpr double fixtureLight = 250.0;
constexpr double fixtureLit = 0.5;
constexpr double fixtureNear = 128.0;
constexpr double fixtureOffset = 4.0;
constexpr double fixtureStep = 128.0;
constexpr double recessMax = 48.0;
constexpr double glowMinShare = 0.03;
constexpr double fixtureMinShare = 0.005;
constexpr double liquidStep = 96.0;
constexpr double liquidHeight = 16.0;
constexpr double liquidLight = 120.0;
constexpr double liquidRoom = 192.0;
constexpr double liquidLone = 5.0;
constexpr double liquidCrowd = 0.4;
constexpr double glowBudget = 300.0;
constexpr int contentsEmpty = -1;
constexpr int contentsSolid = -2;
constexpr const char* lightOptions = "-lit -lux -lightgrid"; // relight_maps.py's -lit and OUTPUT_ARGS
// The author's ericw-tools (relight_maps.py's DEFAULT_LIGHT), the last place looked in.
constexpr const char* authorsLight = "C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64/light.exe";

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

[[nodiscard]] double sq(double x)
{
    return za::pow(x, 2.0); // (Python's x ** 2)
}

[[nodiscard]] bool isNear(const V3& c, const V3& d, double r)
{
    PySum s;
    for(int j = 0; j < 3; j++)
    {
        s.add(sq(c[j] - d[j]));
    }
    return s.value() < r * r;
}

[[nodiscard]] double dot(const V3& a, const V3& b)
{
    PySum s;
    for(int j = 0; j < 3; j++)
    {
        s.add(a[j] * b[j]);
    }
    return s.value();
}

[[nodiscard]] double selfDot(const V3& a) // sum(x * x for x in a)
{
    return dot(a, a);
}

[[nodiscard]] V3 along(const V3& p, const V3& n, double d) // tuple(p[j] + n[j] * d for j in range(3))
{
    return V3{{p[0] + n[0] * d, p[1] + n[1] * d, p[2] + n[2] * d}};
}

// ---------------------------------------------------------------------------------------------------------------------
// The .bsp.

struct Bsp
{
    const unsigned char* data{nullptr};
    za::SizeT size{0};

    [[nodiscard]] int i32(za::SizeT at) const
    {
        int v = 0;
        if(at + 4 <= size)
        {
            memcpy(&v, data + at, 4);
        }
        return LittleLong(v);
    }
    [[nodiscard]] short i16(za::SizeT at) const
    {
        short v = 0;
        if(at + 2 <= size)
        {
            memcpy(&v, data + at, 2);
        }
        return LittleShort(v);
    }
    [[nodiscard]] unsigned short u16(za::SizeT at) const
    {
        return static_cast<unsigned short>(i16(at));
    }
    [[nodiscard]] double f32(za::SizeT at) const
    {
        float v = 0.f;
        if(at + 4 <= size)
        {
            memcpy(&v, data + at, 4);
        }
        return static_cast<double>(LittleFloat(v));
    }
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

// The entity lump's text (up to its first NUL).
[[nodiscard]] za::String entitiesText(const Bsp& b)
{
    const int o = b.lumpOffset(0), l = b.lumpLength(0);
    za::SizeT n = 0;
    while(n < static_cast<za::SizeT>(l) && b.data[o + n])
    {
        n++;
    }
    return za::String{reinterpret_cast<const char*>(b.data + o), n};
}

// The .bsp's BSPX lumps (vis_maps.bspx_lumps): engines look for them right after the last of the 15 lumps.
struct BspxLump
{
    char name[24]{};
    int offset{0};
    int length{0};
};

[[nodiscard]] za::Vector<BspxLump> bspxLumps(const Bsp& b)
{
    za::Vector<BspxLump> out;
    za::SizeT end = 0;
    for(int i = 0; i < 15; i++)
    {
        end = za::max(end, static_cast<za::SizeT>(b.lumpOffset(i)) + static_cast<za::SizeT>(b.lumpLength(i)));
    }
    end = (end + 3) & ~static_cast<za::SizeT>(3);
    if(end + 8 > b.size || memcmp(b.data + end, "BSPX", 4))
    {
        return out;
    }
    const int count = b.i32(end + 4);
    for(int i = 0; i < count; i++)
    {
        const za::SizeT at = end + 8 + static_cast<za::SizeT>(i) * 32;
        if(at + 32 > b.size)
        {
            break;
        }
        BspxLump l;
        memcpy(l.name, b.data + at, 24);
        l.offset = b.i32(at + 24);
        l.length = b.i32(at + 28);
        if(l.offset < 0 || l.length < 0 || static_cast<za::SizeT>(l.offset) + static_cast<za::SizeT>(l.length) > b.size)
        {
            continue;
        }
        out.pushBack(l);
    }
    return out;
}

// The .bsp with its entity lump `entities` (and a NUL), its lumps one after another and then its BSPX lumps
// (vis_maps.packed): where engines look for them.
[[nodiscard]] za::Vector<unsigned char> withEntities(const Bsp& b, za::StringView entities)
{
    za::Vector<unsigned char> out;
    const auto pad = [&] {
        while(out.size() % 4)
        {
            out.pushBack(0);
        }
    };
    const auto put32 = [&](za::SizeT at, int v) {
        v = LittleLong(v);
        memcpy(out.data() + at, &v, 4);
    };
    out.resize(4 + 15 * 8, 0);
    memcpy(out.data(), b.data, 4);
    for(int i = 0; i < 15; i++)
    {
        pad();
        const za::SizeT at = out.size();
        if(i == 0)
        {
            out.resize(at + entities.size() + 1, 0);
            memcpy(out.data() + at, entities.data(), entities.size());
        }
        else
        {
            const int o = b.lumpOffset(i), l = b.lumpLength(i);
            out.resize(at + static_cast<za::SizeT>(l), 0);
            if(l)
            {
                memcpy(out.data() + at, b.data + o, static_cast<za::SizeT>(l));
            }
        }
        put32(4 + static_cast<za::SizeT>(i) * 8, static_cast<int>(at));
        put32(8 + static_cast<za::SizeT>(i) * 8, static_cast<int>(out.size() - at));
    }
    pad();
    const za::Vector<BspxLump> bspx = bspxLumps(b);
    if(!bspx.empty())
    {
        const za::SizeT table = out.size() + 8;
        out.resize(table + 32 * bspx.size(), 0);
        memcpy(out.data() + table - 8, "BSPX", 4);
        put32(table - 4, static_cast<int>(bspx.size()));
        for(za::SizeT i = 0; i < bspx.size(); i++)
        {
            pad();
            const za::SizeT at = out.size();
            out.resize(at + static_cast<za::SizeT>(bspx[i].length), 0);
            memcpy(out.data() + at, b.data + bspx[i].offset, static_cast<za::SizeT>(bspx[i].length));
            memcpy(out.data() + table + i * 32, bspx[i].name, 24);
            put32(table + i * 32 + 24, static_cast<int>(at));
            put32(table + i * 32 + 28, bspx[i].length);
        }
        pad();
    }
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Entities: their text's blocks and keys, read as relight_maps.py's regular expressions read them.

struct Key
{
    za::String name;
    za::String value;
    za::SizeT start{0}, end{0}; // the pair's text, [start, end)
};

// The keys of a piece of entity text: re.findall(r'"([^"]*)"\s+"([^"]*)"').
[[nodiscard]] za::Vector<Key> keysOf(za::StringView text)
{
    za::Vector<Key> out;
    za::SizeT p = 0;
    while(true)
    {
        p = text.find('"', p);
        if(p == za::StringView::nPos)
        {
            break;
        }
        const za::SizeT q1 = text.find('"', p + 1);
        if(q1 == za::StringView::nPos)
        {
            break;
        }
        za::SizeT w = q1 + 1;
        while(w < text.size() && (text[w] == ' ' || text[w] == '\t' || text[w] == '\n' || text[w] == '\r' ||
                                     text[w] == '\f' || text[w] == '\v'))
        {
            w++;
        }
        const za::SizeT q2 = w < text.size() && w > q1 + 1 && text[w] == '"' ? text.find('"', w + 1) : za::StringView::nPos;
        if(q2 == za::StringView::nPos)
        {
            p++;
            continue;
        }
        out.pushBack(Key{za::String{text.substrByPosLen(p + 1, q1 - p - 1)}, za::String{text.substrByPosLen(w + 1, q2 - w - 1)},
            p, q2 + 1});
        p = q2 + 1;
    }
    return out;
}

// dict(keys).get(name): the last of that name.
[[nodiscard]] const za::String* keyValue(const za::Vector<Key>& keys, const char* name)
{
    const za::String* found = nullptr;
    for(const Key& k : keys)
    {
        if(k.name == za::StringView{name})
        {
            found = &k.value;
        }
    }
    return found;
}

// The {...} blocks with no brace inside (re r"\{[^{}]*\}"): [start, end) of each.
struct Span
{
    za::SizeT start, end;
};

[[nodiscard]] za::Vector<Span> blocksOf(za::StringView text)
{
    za::Vector<Span> out;
    za::SizeT i = 0;
    while(i < text.size())
    {
        if(text[i] != '{')
        {
            i++;
            continue;
        }
        za::SizeT j = i + 1;
        while(j < text.size() && text[j] != '{' && text[j] != '}')
        {
            j++;
        }
        if(j < text.size() && text[j] == '}')
        {
            out.pushBack(Span{i, j + 1});
            i = j + 1;
        }
        else
        {
            i = j;
        }
    }
    return out;
}

// Python's float() of a word (False: not a number).
[[nodiscard]] bool parseFloat(za::StringView word, double& out)
{
    char buf[64];
    za::SizeT a = 0, b = word.size();
    while(a < b && (word[a] == ' ' || word[a] == '\t' || word[a] == '\n' || word[a] == '\r'))
    {
        a++;
    }
    while(b > a && (word[b - 1] == ' ' || word[b - 1] == '\t' || word[b - 1] == '\n' || word[b - 1] == '\r'))
    {
        b--;
    }
    if(a == b || b - a >= sizeof(buf))
    {
        return false;
    }
    memcpy(buf, word.data() + a, b - a);
    buf[b - a] = 0;
    char* end = nullptr;
    out = strtod(buf, &end);
    return end && *end == 0;
}

// The words of a value (str.split()).
[[nodiscard]] za::Vector<za::StringView> wordsOf(za::StringView s)
{
    za::Vector<za::StringView> out;
    za::SizeT i = 0;
    while(i < s.size())
    {
        while(i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
        {
            i++;
        }
        za::SizeT j = i;
        while(j < s.size() && !(s[j] == ' ' || s[j] == '\t' || s[j] == '\n' || s[j] == '\r'))
        {
            j++;
        }
        if(j > i)
        {
            out.pushBack(s.substrByPosLen(i, j - i));
        }
        i = j;
    }
    return out;
}

[[nodiscard]] bool startsWith(za::StringView s, const char* prefix)
{
    return s.startsWith(za::StringView{prefix});
}

// The map's own lights (map_lights): origin, whether it starts off (a "start off" light with a target name), its light.
struct MapLight
{
    V3 origin;
    bool off{false};
    double light{0.0};
};

[[nodiscard]] za::Vector<MapLight> mapLights(za::StringView text)
{
    za::Vector<MapLight> out;
    za::SizeT start = 0;
    while(start <= text.size())
    {
        za::SizeT end = text.find('}', start);
        if(end == za::StringView::nPos)
        {
            end = text.size();
        }
        const za::Vector<Key> keys = keysOf(text.substrByPosLen(start, end - start));
        start = end + 1;
        const za::String* cls = keyValue(keys, "classname");
        const za::String* origin = keyValue(keys, "origin");
        if(!cls || !startsWith(*cls, "light") || !origin)
        {
            continue;
        }
        const za::Vector<za::StringView> o = wordsOf(*origin);
        const za::String* flagsText = keyValue(keys, "spawnflags");
        const za::String* light = keyValue(keys, "light");
        if(!light)
        {
            light = keyValue(keys, "_light");
        }
        const za::Vector<za::StringView> lw = wordsOf(light ? za::StringView{*light} : za::StringView{"300"});
        MapLight m;
        double flags = 0.0;
        bool ok = o.size() >= 3 && !lw.empty() && parseFloat(flagsText ? za::StringView{*flagsText} : za::StringView{"0"}, flags) &&
                  parseFloat(lw.back(), m.light);
        for(int j = 0; ok && j < 3; j++)
        {
            ok = parseFloat(o[static_cast<za::SizeT>(j)], m.origin[j]);
        }
        if(!ok || !isfinite(flags))
        {
            continue;
        }
        m.light = za::abs(m.light);
        m.off = keyValue(keys, "targetname") && !keyValue(keys, "targetname")->empty() && (static_cast<long long>(flags) & 1);
        out.pushBack(m);
    }
    return out;
}

// id_light_values: a light of "light" "0" gets id's default, 300 (ericw-tools 2 takes it as 0).
[[nodiscard]] za::String idLightValues(za::StringView text)
{
    za::String out;
    za::SizeT last = 0;
    for(const Span& s : blocksOf(text))
    {
        out += text.substrByPosLen(last, s.start - last);
        last = s.end;
        const za::StringView block = text.substrByPosLen(s.start, s.end - s.start);
        const za::Vector<Key> keys = keysOf(block);
        const za::String* cls = keyValue(keys, "classname");
        const za::String* light = keyValue(keys, "light");
        double v = 300.0;
        bool zero = false;
        if(cls && startsWith(*cls, "light"))
        {
            const za::Vector<za::StringView> w = wordsOf(light ? za::StringView{*light} : za::StringView{"300"});
            zero = !w.empty() && parseFloat(w.back(), v) && v == 0.0;
        }
        if(!zero)
        {
            out += block;
            continue;
        }
        // Each key and its value in turn: a "light" that is a value (the classname's) is not the key.
        za::SizeT p = 0;
        for(const Key& k : keys)
        {
            if(k.name == za::StringView{"light"})
            {
                out += block.substrByPosLen(p, k.start - p);
                out += "\"light\" \"300\"";
                p = k.end;
            }
        }
        out += block.substrByPosLen(p, block.size() - p);
    }
    out += text.substrByPosLen(last, text.size() - last);
    return out;
}

// without_map_light_settings: the worldspawn's "_" keys left out (the re-release's maps carry ericw-tools settings of
// their own, which would take over the look's); for id's games' maps only (another map's mapper set them to be used).
[[nodiscard]] za::String withoutMapLightSettings(za::StringView text)
{
    const za::Vector<Span> blocks = blocksOf(text);
    if(blocks.empty())
    {
        return za::String{text};
    }
    const za::StringView first = text.substrByPosLen(blocks[0].start, blocks[0].end - blocks[0].start);
    if(first.find(za::StringView{"\"worldspawn\""}) == za::StringView::nPos)
    {
        return za::String{text};
    }
    // re.sub(r'\s*"_[^"]*"\s+"[^"]*"', "", block)
    za::String block;
    za::SizeT p = 0;
    while(p < first.size())
    {
        za::SizeT ws = p;
        while(ws < first.size() && (first[ws] == ' ' || first[ws] == '\t' || first[ws] == '\n' || first[ws] == '\r'))
        {
            ws++;
        }
        bool matched = false;
        if(ws + 1 < first.size() && first[ws] == '"' && first[ws + 1] == '_')
        {
            const za::SizeT q1 = first.find('"', ws + 1);
            if(q1 != za::StringView::nPos)
            {
                za::SizeT w = q1 + 1;
                while(w < first.size() && (first[w] == ' ' || first[w] == '\t' || first[w] == '\n' || first[w] == '\r'))
                {
                    w++;
                }
                if(w > q1 + 1 && w < first.size() && first[w] == '"')
                {
                    const za::SizeT q2 = first.find('"', w + 1);
                    if(q2 != za::StringView::nPos)
                    {
                        p = q2 + 1;
                        matched = true;
                    }
                }
            }
        }
        if(!matched)
        {
            block.pushBack(first[p]);
            p++;
        }
    }
    za::String out{text.substrByPosLen(0, blocks[0].start)};
    out += block;
    out += text.substrByPosLen(blocks[0].end, text.size() - blocks[0].end);
    return out;
}

// A number's text for a key's value (as a mapper would write it).
void appendNumber(za::String& out, double v)
{
    char buf[32];
    q_snprintf(buf, sizeof(buf), "%g", v);
    out += buf;
}

// The map's own lights `mapScale` times as bright (and their linear falloff as long: "wait" with it), its sunlight
// `sunScale` times: the page's Map Lights and Sunlight. Each key set again at the block's end.
[[nodiscard]] za::String scaledLights(za::StringView text, double mapScale, double sunScale)
{
    if(mapScale == 1.0 && sunScale == 1.0)
    {
        return za::String{text};
    }
    za::String out;
    za::SizeT last = 0;
    for(const Span& s : blocksOf(text))
    {
        out += text.substrByPosLen(last, s.start - last);
        last = s.end;
        const za::StringView block = text.substrByPosLen(s.start, s.end - s.start);
        const za::Vector<Key> keys = keysOf(block);
        const za::String* cls = keyValue(keys, "classname");
        za::Vector<Key> set;
        if(cls && startsWith(*cls, "light") && mapScale != 1.0)
        {
            const za::String* light = keyValue(keys, "light");
            const za::Vector<za::StringView> w = wordsOf(light ? za::StringView{*light} : za::StringView{"300"});
            double v = 300.0;
            if(!w.empty() && parseFloat(w.back(), v))
            {
                za::String value;
                for(za::SizeT i = 0; i + 1 < w.size(); i++) // (a colour before it: kept)
                {
                    value += w[i];
                    value += " ";
                }
                appendNumber(value, v * mapScale);
                set.pushBack(Key{za::String{"light"}, ZA_MOVE(value)});
                const za::String* delay = keyValue(keys, "delay");
                double d = 0.0;
                if(!delay || (parseFloat(*delay, d) && d == 0.0)) // linear falloff: "wait" with it, the reach kept
                {
                    const za::String* wait = keyValue(keys, "wait");
                    double wv = 1.0;
                    if(!wait || !parseFloat(*wait, wv))
                    {
                        wv = 1.0;
                    }
                    za::String waitValue;
                    appendNumber(waitValue, wv * mapScale);
                    set.pushBack(Key{za::String{"wait"}, ZA_MOVE(waitValue)});
                }
            }
        }
        if(cls && *cls == za::StringView{"worldspawn"} && sunScale != 1.0)
        {
            for(const char* name : {"_sunlight", "_sun_light", "_sunlight2", "_sunlight3", "_sun2"})
            {
                const za::String* v = keyValue(keys, name);
                double x = 0.0;
                if(v && parseFloat(*v, x))
                {
                    za::String value;
                    appendNumber(value, x * sunScale);
                    set.pushBack(Key{za::String{name}, ZA_MOVE(value)});
                }
            }
        }
        if(set.empty())
        {
            out += block;
            continue;
        }
        out += "{";
        for(const Key& k : keys)
        {
            bool replaced = false;
            for(const Key& n : set)
            {
                replaced = replaced || k.name == n.name;
            }
            if(!replaced)
            {
                out += "\n\"";
                out += k.name;
                out += "\" \"";
                out += k.value;
                out += "\"";
            }
        }
        for(const Key& n : set)
        {
            out += "\n\"";
            out += n.name;
            out += "\" \"";
            out += n.value;
            out += "\"";
        }
        out += "\n}";
    }
    out += text.substrByPosLen(last, text.size() - last);
    return out;
}

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

constexpr const char* kindNames[] = {"fixture", "glow", "liquid", "off"};

// fnmatch.fnmatchcase: *, ?, [seq], [!seq].
[[nodiscard]] bool fnmatch(const char* p, const char* s)
{
    for(; *p; p++)
    {
        if(*p == '*')
        {
            while(p[1] == '*')
            {
                p++;
            }
            for(const char* t = s;; t++)
            {
                if(fnmatch(p + 1, t))
                {
                    return true;
                }
                if(!*t)
                {
                    return false;
                }
            }
        }
        if(!*s)
        {
            return false;
        }
        if(*p == '?')
        {
            s++;
            continue;
        }
        if(*p == '[')
        {
            const char* q = p + 1;
            const bool negate = *q == '!';
            if(negate)
            {
                q++;
            }
            if(*q == ']')
            {
                q++; // (a ] first is in the set)
            }
            while(*q && *q != ']')
            {
                q++;
            }
            if(*q == ']')
            {
                bool in = false;
                const char* c = p + 1 + (negate ? 1 : 0);
                for(bool first = true; first || *c != ']'; first = false)
                {
                    if(c[1] == '-' && c[2] && c[2] != ']')
                    {
                        in = in || (*s >= c[0] && *s <= c[2]);
                        c += 3;
                    }
                    else
                    {
                        in = in || *s == *c;
                        c++;
                    }
                }
                if(in == negate)
                {
                    return false;
                }
                s++;
                p = q;
                continue;
            }
            // (no closing ]: a literal [)
        }
        if(*p != *s)
        {
            return false;
        }
        s++;
    }
    return !*s;
}

[[nodiscard]] za::String lower(za::StringView s)
{
    za::String out{s};
    for(char& c : out)
    {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

// The rules of relight_textures.cfg (load_rules); a line it cannot read is said and skipped.
[[nodiscard]] za::Vector<Rule> loadRules(const char* text, const char* file)
{
    za::Vector<Rule> rules;
    int number = 0;
    files::forLines(za::StringView{text}, [&](za::StringView line) {
        number++;
        const za::SizeT hash = line.find('#');
        if(hash != za::StringView::nPos)
        {
            line = line.substrByPosLen(0, hash);
        }
        const za::Vector<za::StringView> words = wordsOf(line);
        if(words.empty())
        {
            return;
        }
        Rule r;
        const za::String first = lower(words[0]);
        bool ok = true;
        if(first == za::StringView{"strength"})
        {
            for(za::SizeT i = 1; i < words.size() && ok; i++)
            {
                const za::SizeT eq = words[i].find('=');
                ok = false;
                for(int k = 0; k < 3 && eq != za::StringView::nPos; k++)
                {
                    if(words[i].substrByPosLen(0, eq) == za::StringView{kindNames[k]})
                    {
                        ok = parseFloat(words[i].substrByPosLen(eq + 1, words[i].size() - eq - 1), r.strength[k]);
                        r.hasStrength[k] = ok;
                    }
                }
            }
        }
        else
        {
            int slashes = 0;
            for(char c : first)
            {
                slashes += c == '/';
            }
            for(int k = slashes; k < 2; k++)
            {
                r.pattern += "*/";
            }
            r.pattern += first;
            for(za::SizeT i = 1; i < words.size() && ok; i++)
            {
                const za::SizeT eq = words[i].find('=');
                ok = eq != za::StringView::nPos && eq + 1 < words[i].size();
                if(!ok)
                {
                    break;
                }
                const za::StringView key = words[i].substrByPosLen(0, eq);
                const za::StringView value = words[i].substrByPosLen(eq + 1, words[i].size() - eq - 1);
                if(key == za::StringView{"kind"})
                {
                    ok = false;
                    for(const char* k : kindNames)
                    {
                        if(value == za::StringView{k})
                        {
                            q_strlcpy(r.kind, k, sizeof(r.kind));
                            ok = r.hasKind = true;
                        }
                    }
                }
                else if(key == za::StringView{"scale"})
                {
                    ok = r.hasScale = parseFloat(value, r.scale);
                }
                else if(key == za::StringView{"light"})
                {
                    ok = r.hasLight = parseFloat(value, r.light);
                }
                else if(key == za::StringView{"reach"})
                {
                    ok = r.hasReach = parseFloat(value, r.reach);
                }
                else if(key == za::StringView{"color"})
                {
                    int n = 0;
                    za::SizeT a = 0;
                    while(ok && a <= value.size())
                    {
                        za::SizeT c = value.find(',', a);
                        if(c == za::StringView::nPos)
                        {
                            c = value.size();
                        }
                        ok = n < 3 && parseFloat(value.substrByPosLen(a, c - a), r.color[n]);
                        n++;
                        a = c + 1;
                    }
                    ok = ok && n == 3;
                    r.hasColor = ok;
                }
                else
                {
                    ok = false;
                }
            }
        }
        if(!ok)
        {
            Con_Printf("%s:%d: not understood, left out\n", file, number);
            return;
        }
        rules.pushBack(ZA_MOVE(r));
    });
    return rules;
}

// texture_rule: every rule matching game/map/texture, later ones' settings over earlier ones'.
[[nodiscard]] Rule textureRule(const za::Vector<Rule>& rules, const char* game, const char* map, const char* name)
{
    char full[256];
    q_snprintf(full, sizeof(full), "%s/%s/%s", game, map, lower(za::StringView{name}).cStr());
    Rule merged;
    for(const Rule& r : rules)
    {
        if(r.pattern.empty() || !fnmatch(r.pattern.cStr(), full))
        {
            continue;
        }
        if(r.hasKind)
        {
            q_strlcpy(merged.kind, r.kind, sizeof(merged.kind));
            merged.hasKind = true;
        }
        if(r.hasScale)
        {
            merged.scale = r.scale;
            merged.hasScale = true;
        }
        if(r.hasLight)
        {
            merged.light = r.light;
            merged.hasLight = true;
        }
        if(r.hasReach)
        {
            merged.reach = r.reach;
            merged.hasReach = true;
        }
        if(r.hasColor)
        {
            merged.color = r.color;
            merged.hasColor = true;
        }
    }
    return merged;
}

// ---------------------------------------------------------------------------------------------------------------------
// The world's faces and contents (texture_faces, contents_at).

struct Face
{
    V3 centre;
    double area{0.0};
    V3 normal;
    za::Vector<V3> pts;
};

// Each texture's faces, by its index in the texture lump (BSP29).
[[nodiscard]] za::Vector<za::Vector<Face>> textureFaces(const Bsp& b, int textures)
{
    za::Vector<za::Vector<Face>> out;
    out.resize(static_cast<za::SizeT>(za::max(textures, 0)));
    const int pofs = b.lumpOffset(1), plen = b.lumpLength(1) / 20;
    const int vofs = b.lumpOffset(3), vlen = b.lumpLength(3) / 12;
    const int tofs = b.lumpOffset(6), tlen = b.lumpLength(6) / 40;
    const int fofs = b.lumpOffset(7), flen = b.lumpLength(7) / 20;
    const int eofs = b.lumpOffset(12), elen = b.lumpLength(12) / 4;
    const int sofs = b.lumpOffset(13), slen = b.lumpLength(13) / 4;
    const auto vert = [&](int i) {
        return V3{{b.f32(vofs + i * 12), b.f32(vofs + i * 12 + 4), b.f32(vofs + i * 12 + 8)}};
    };
    for(int i = 0; i < flen; i++)
    {
        const za::SizeT at = static_cast<za::SizeT>(fofs) + static_cast<za::SizeT>(i) * 20;
        const int planenum = b.i16(at), side = b.i16(at + 2), first = b.i32(at + 4), count = b.i16(at + 8), texinfo = b.i16(at + 10);
        Face f;
        bool ok = true;
        for(int k = 0; k < count && ok; k++)
        {
            ok = first + k >= 0 && first + k < slen;
            if(!ok)
            {
                break;
            }
            const int e = b.i32(sofs + (first + k) * 4);
            const int ei = e >= 0 ? e : -e;
            ok = ei < elen;
            if(!ok)
            {
                break;
            }
            const int v = b.u16(eofs + ei * 4 + (e >= 0 ? 0 : 2));
            ok = v < vlen;
            if(ok)
            {
                f.pts.pushBack(vert(v));
            }
        }
        if(!ok || f.pts.empty())
        {
            continue;
        }
        double area = 0.0;
        for(za::SizeT k = 1; k + 1 < f.pts.size(); k++)
        {
            V3 a, c, d;
            for(int j = 0; j < 3; j++)
            {
                a[j] = f.pts[k][j] - f.pts[0][j];
                c[j] = f.pts[k + 1][j] - f.pts[0][j];
            }
            d = V3{{a[1] * c[2] - a[2] * c[1], a[2] * c[0] - a[0] * c[2], a[0] * c[1] - a[1] * c[0]}};
            area += 0.5 * za::pow(sq(d[0]) + sq(d[1]) + sq(d[2]), 0.5);
        }
        f.area = area;
        for(int j = 0; j < 3; j++)
        {
            PySum s;
            for(const V3& p : f.pts)
            {
                s.add(p[j]);
            }
            f.centre[j] = s.value() / static_cast<double>(f.pts.size());
        }
        f.normal = planenum >= 0 && planenum < plen ?
            V3{{b.f32(pofs + planenum * 20), b.f32(pofs + planenum * 20 + 4), b.f32(pofs + planenum * 20 + 8)}} :
            V3{{0.0, 0.0, 1.0}};
        if(side)
        {
            for(int j = 0; j < 3; j++)
            {
                f.normal[j] = -f.normal[j];
            }
        }
        const int m = texinfo >= 0 && texinfo < tlen ? b.i32(tofs + texinfo * 40 + 32) : -1;
        if(m >= 0 && m < textures)
        {
            out[static_cast<za::SizeT>(m)].pushBack(ZA_MOVE(f));
        }
    }
    return out;
}

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

[[nodiscard]] za::Vector<Thing> thingsOf(const za::Vector<Face>& faces)
{
    za::Vector<za::Vector<const Face*>> groups;
    for(const Face& f : faces)
    {
        za::Vector<const Face*> merged{&f};
        za::Vector<za::Vector<const Face*>> kept;
        for(za::Vector<const Face*>& g : groups)
        {
            bool joins = false;
            for(const Face* h : g)
            {
                if(isNear(f.centre, h->centre, glowObject))
                {
                    joins = true;
                    break;
                }
            }
            if(joins)
            {
                for(const Face* h : g)
                {
                    merged.pushBack(h);
                }
            }
            else
            {
                kept.pushBack(ZA_MOVE(g));
            }
        }
        kept.pushBack(ZA_MOVE(merged));
        groups = ZA_MOVE(kept);
    }
    za::Vector<Thing> out;
    for(za::Vector<const Face*>& g : groups)
    {
        PySum area;
        for(const Face* h : g)
        {
            area.add(h->area);
        }
        Thing t;
        t.n = za::max(za::pow(static_cast<double>(g.size()), 0.5), area.value() / sq(surflight));
        t.faces = ZA_MOVE(g);
        out.pushBack(ZA_MOVE(t));
    }
    return out;
}

[[nodiscard]] double crowd(const Thing& thing, const za::Vector<const Thing*>& everyone)
{
    const V3& c = thing.faces[0]->centre;
    PySum s;
    for(const Thing* g : everyone)
    {
        if(isNear(c, g->faces[0]->centre, glowRoom))
        {
            s.add(g->n);
        }
    }
    return za::max(1.0, s.value());
}

[[nodiscard]] za::Vector<V3> faceSamples(const Face& face, double step)
{
    const za::Vector<V3>& pts = face.pts;
    if(face.area <= sq(step) || pts.size() < 3)
    {
        return {face.centre};
    }
    const V3& n = face.normal;
    const V3 helper = za::abs(n[2]) < 0.9 ? V3{{0.0, 0.0, 1.0}} : V3{{1.0, 0.0, 0.0}};
    V3 u{{n[1] * helper[2] - n[2] * helper[1], n[2] * helper[0] - n[0] * helper[2], n[0] * helper[1] - n[1] * helper[0]}};
    const double ul = za::pow(selfDot(u), 0.5);
    for(int j = 0; j < 3; j++)
    {
        u[j] = u[j] / ul;
    }
    const V3 v{{n[1] * u[2] - n[2] * u[1], n[2] * u[0] - n[0] * u[2], n[0] * u[1] - n[1] * u[0]}};
    za::Vector<double> fx, fy;
    for(const V3& p : pts)
    {
        fx.pushBack(dot(p, u));
        fy.pushBack(dot(p, v));
    }
    const double base = dot(pts[0], n);
    const auto inside = [&](double x, double y) {
        double sign = 0.0;
        for(za::SizeT k = 0; k < fx.size(); k++)
        {
            const za::SizeT k1 = (k + 1) % fx.size();
            const double c = (fx[k1] - fx[k]) * (y - fy[k]) - (fy[k1] - fy[k]) * (x - fx[k]);
            if(za::abs(c) > 1e-3)
            {
                if(sign != 0.0 && (c > 0) != (sign > 0))
                {
                    return false;
                }
                sign = c;
            }
        }
        return true;
    };
    double lo[2] = {fx[0], fy[0]}, hi[2] = {fx[0], fy[0]};
    for(za::SizeT k = 0; k < fx.size(); k++)
    {
        lo[0] = za::min(lo[0], fx[k]);
        hi[0] = za::max(hi[0], fx[k]);
        lo[1] = za::min(lo[1], fy[k]);
        hi[1] = za::max(hi[1], fy[k]);
    }
    int counts[2];
    for(int k = 0; k < 2; k++)
    {
        counts[k] = za::max(1, static_cast<int>((hi[k] - lo[k]) / step + 0.5));
    }
    za::Vector<V3> out;
    for(int i = 0; i < counts[0]; i++)
    {
        for(int j = 0; j < counts[1]; j++)
        {
            const double x = lo[0] + (hi[0] - lo[0]) * (i + 0.5) / counts[0];
            const double y = lo[1] + (hi[1] - lo[1]) * (j + 0.5) / counts[1];
            if(inside(x, y))
            {
                out.pushBack(V3{{x * u[0] + y * v[0] + base * n[0], x * u[1] + y * v[1] + base * n[1],
                    x * u[2] + y * v[2] + base * n[2]}});
            }
        }
    }
    if(out.empty())
    {
        out.pushBack(face.centre);
    }
    return out;
}

[[nodiscard]] double recessDepth(const Face& face, const World& world)
{
    const za::Vector<V3>& pts = face.pts;
    if(pts.size() < 3)
    {
        return fixtureOffset;
    }
    struct Outside
    {
        V3 o, out;
    };
    za::Vector<Outside> outside;
    for(za::SizeT k = 0; k < pts.size(); k++)
    {
        V3 mid, out;
        for(int j = 0; j < 3; j++)
        {
            mid[j] = (pts[k][j] + pts[(k + 1) % pts.size()][j]) / 2;
            out[j] = mid[j] - face.centre[j];
        }
        const double a = dot(out, face.normal);
        for(int j = 0; j < 3; j++)
        {
            out[j] = out[j] - a * face.normal[j];
        }
        const double length = za::pow(selfDot(out), 0.5);
        if(length > 1e-3)
        {
            for(int j = 0; j < 3; j++)
            {
                out[j] = out[j] / length;
            }
            outside.pushBack(Outside{V3{{mid[0] + out[0] * 4, mid[1] + out[1] * 4, mid[2] + out[2] * 4}}, out});
        }
    }
    double last = fixtureOffset;
    double d = fixtureOffset;
    za::Vector<const V3*> walls;
    while(d <= recessMax)
    {
        if(world.solid(along(face.centre, face.normal, d)))
        {
            return last;
        }
        walls.clear();
        for(const Outside& o : outside)
        {
            if(world.solid(along(o.o, face.normal, d)))
            {
                walls.pushBack(&o.out);
            }
        }
        bool opposite = false;
        for(const V3* a : walls)
        {
            for(const V3* c : walls)
            {
                opposite = opposite || dot(*a, *c) < -0.5;
            }
        }
        if(!opposite)
        {
            return d == fixtureOffset ? d : za::min(recessMax, d + fixtureOffset);
        }
        last = d;
        d += 2.0;
    }
    return last;
}

[[nodiscard]] za::Vector<V3> fixtureSpots(const za::Vector<const Face*>& faces, const World& world)
{
    PySum totalSum;
    for(const Face* f : faces)
    {
        totalSum.add(f->area);
    }
    const double total = totalSum.value() != 0.0 ? totalSum.value() : 1.0;
    V3 centre;
    for(int j = 0; j < 3; j++)
    {
        PySum s;
        for(const Face* f : faces)
        {
            s.add(f->centre[j] * f->area);
        }
        centre[j] = s.value() / total;
    }
    double spread = 0.0;
    bool small = true;
    for(const Face* f : faces)
    {
        PySum s;
        for(int j = 0; j < 3; j++)
        {
            s.add(sq(f->centre[j] - centre[j]));
        }
        spread = za::max(spread, za::pow(s.value(), 0.5));
        small = small && f->area <= sq(fixtureStep);
    }
    if(spread <= fixtureStep / 2 && small)
    {
        V3 facing;
        for(int j = 0; j < 3; j++)
        {
            PySum s;
            for(const Face* f : faces)
            {
                s.add(f->normal[j] * f->area);
            }
            facing[j] = s.value() / total;
        }
        const double length = za::pow(selfDot(facing), 0.5);
        za::Vector<V3> candidates;
        if(length > 0.3)
        {
            for(int j = 0; j < 3; j++)
            {
                facing[j] = facing[j] / length;
            }
            bool any = false;
            double depth = 0.0;
            for(const Face* f : faces)
            {
                if(dot(f->normal, facing) > 0.8)
                {
                    const double r = recessDepth(*f, world);
                    depth = any ? za::max(depth, r) : r;
                    any = true;
                }
            }
            if(!any)
            {
                depth = fixtureOffset;
            }
            for(const double o : {depth, fixtureOffset, 2.0})
            {
                candidates.pushBack(along(centre, facing, o));
            }
        }
        double top = centre[2];
        if(!faces[0]->pts.empty())
        {
            bool first = true;
            for(const Face* f : faces)
            {
                for(const V3& p : f->pts)
                {
                    top = first ? p[2] : za::max(top, p[2]);
                    first = false;
                }
            }
        }
        candidates.pushBack(centre);
        candidates.pushBack(V3{{centre[0], centre[1], top + fixtureOffset}});
        za::Vector<const Face*> byArea = faces;
        za::stableSort(byArea.begin(), byArea.end(), [](const Face* a, const Face* c) { return -a->area < -c->area; });
        for(const Face* f : byArea)
        {
            candidates.pushBack(along(f->centre, f->normal, fixtureOffset));
        }
        for(const V3& p : candidates)
        {
            if(!world.solid(p))
            {
                return {p};
            }
        }
        return {};
    }
    za::Vector<V3> spots;
    for(const Face* f : faces)
    {
        const double depth = recessDepth(*f, world);
        for(const V3& p : faceSamples(*f, fixtureStep))
        {
            for(const double offset : {depth, fixtureOffset, 2.0})
            {
                const V3 q = along(p, f->normal, offset);
                if(!world.solid(q))
                {
                    bool close = false;
                    for(const V3& r : spots)
                    {
                        close = close || isNear(q, r, fixtureStep / 2);
                    }
                    if(!close)
                    {
                        spots.pushBack(q);
                    }
                    break;
                }
            }
        }
    }
    return spots;
}

[[nodiscard]] za::Vector<V3> liquidSpots(const za::Vector<Face>& faces, const World& world)
{
    za::Vector<V3> spots;
    for(const Face& f : faces)
    {
        for(const V3& p : faceSamples(f, liquidStep))
        {
            for(const double offset : {liquidHeight, 8.0, 2.0})
            {
                const V3 q = along(p, f.normal, offset);
                if(world.contents(q) == contentsEmpty)
                {
                    bool close = false;
                    for(const V3& r : spots)
                    {
                        close = close || isNear(q, r, liquidStep / 2);
                    }
                    if(!close)
                    {
                        spots.pushBack(q);
                    }
                    break;
                }
            }
        }
    }
    return spots;
}

// ---------------------------------------------------------------------------------------------------------------------
// Glow images, found as the engine finds them (relight_maps.py's Glows, which see).

struct Measured
{
    bool found{false};
    double share{0.0};
    V3 colour;
    char where[MAX_OSPATH]{};
};

// The first of <stem>.png, .tga, .jpg in the search path (Image_LoadImage's order): its file (where) or false.
[[nodiscard]] bool imageFile(const char* stem, char* where, size_t size)
{
    for(const char* ext : {"png", "tga", "jpg"})
    {
        char name[MAX_QPATH * 2];
        q_snprintf(name, sizeof(name), "%s.%s", stem, ext);
        if(COM_FileExists(name, nullptr))
        {
            q_snprintf(where, size, "%s/%s", com_filesource, name);
            return true;
        }
    }
    return false;
}

// The material maps' image <stem> (vr_extmaps_dir: in the game folders, or a full path): its file, or false.
[[nodiscard]] bool extmapsFile(const char* stem, char* where, size_t size)
{
    const char* dir = vr_extmaps_dir.string;
    const bool absolute = dir[0] == '/' || dir[0] == '\\' || (dir[0] && dir[1] == ':');
    if(!absolute)
    {
        char rel[MAX_QPATH * 2];
        q_snprintf(rel, sizeof(rel), "%s/%s", dir, stem);
        return imageFile(rel, where, size);
    }
    for(const char* ext : {"png", "tga", "jpg"})
    {
        q_snprintf(where, size, "%s/%s.%s", dir, stem, ext);
        if(Sys_FileType(where) & FS_ENT_FILE)
        {
            return true;
        }
    }
    return false;
}

// glow_measure of an image Image_LoadImage reads (`load`: its name for it, without extension).
[[nodiscard]] Measured measure(const char* load, const char* where)
{
    Measured m;
    q_strlcpy(m.where, where, sizeof(m.where));
    const int mark = Hunk_LowMark();
    int w = 0, h = 0;
    enum srcformat fmt = SRC_RGBA;
    const byte* data = Image_LoadImage(load, &w, &h, &fmt);
    if(data && fmt == SRC_RGBA && w > 0 && h > 0)
    {
        const za::SizeT n = static_cast<za::SizeT>(w) * static_cast<za::SizeT>(h);
        int peak = 0;
        for(za::SizeT i = 0; i < n; i++)
        {
            peak = za::max(peak, static_cast<int>(za::max(data[i * 4], data[i * 4 + 1], data[i * 4 + 2])));
        }
        if(peak >= 32)
        {
            long long count = 0, sum[3] = {0, 0, 0};
            for(za::SizeT i = 0; i < n; i++)
            {
                const byte* p = data + i * 4;
                if(za::max(p[0], p[1], p[2]) * 2 >= peak)
                {
                    count++;
                    for(int j = 0; j < 3; j++)
                    {
                        sum[j] += p[j];
                    }
                }
            }
            m.found = true;
            m.share = static_cast<double>(count) / static_cast<double>(n);
            for(int j = 0; j < 3; j++)
            {
                m.colour[j] = static_cast<double>(sum[j]) / static_cast<double>(count);
            }
        }
    }
    Hunk_FreeToLowMark(mark);
    return m;
}

// Glows.glow: (share, colour) of the texture `name`'s glow image in map `map`, or not found.
[[nodiscard]] Measured glowImage(const char* map, const char* name)
{
    char file[64];
    q_strlcpy(file, lower(za::StringView{name}).cStr(), sizeof(file));
    for(char& c : file)
    {
        if(c == '*')
        {
            c = '#';
        }
    }
    char where[MAX_OSPATH];
    char stems[2][MAX_QPATH * 2];
    q_snprintf(stems[0], sizeof(stems[0]), "textures/%s/%s", map, file);
    q_snprintf(stems[1], sizeof(stems[1]), "textures/%s", file);
    for(const char* stem : stems)
    {
        if(!imageFile(stem, where, sizeof(where)))
        {
            continue;
        }
        for(const char* suffix : {"_glow", "_luma"})
        {
            char glow[MAX_QPATH * 2];
            q_snprintf(glow, sizeof(glow), "%s%s", stem, suffix);
            if(imageFile(glow, where, sizeof(where)))
            {
                return measure(glow, where);
            }
        }
        break;
    }
    // The material maps' (vr_extmaps.cpp's packName: an animation's frame after the name).
    if(!vr_extmaps.value || !vr_extmaps_dir.string[0])
    {
        return {};
    }
    const char* s = file[0] == '#' ? file + 1 : file;
    char base[64];
    if(s[0] == '+' && s[1] && s[2])
    {
        q_snprintf(base, sizeof(base), "%s+%c", s + 2, s[1]);
    }
    else
    {
        q_strlcpy(base, s, sizeof(base));
    }
    if(!extmapsFile(base, where, sizeof(where)))
    {
        char* plus = strchr(base, '+');
        if(!plus)
        {
            return {};
        }
        *plus = 0;
        if(!extmapsFile(base, where, sizeof(where)))
        {
            return {};
        }
    }
    for(const char* suffix : {"_luma", "_glow"})
    {
        char stem[96];
        q_snprintf(stem, sizeof(stem), "%s%s", base, suffix);
        if(extmapsFile(stem, where, sizeof(where)))
        {
            char load[MAX_QPATH * 2];
            q_snprintf(load, sizeof(load), "vrext/%s", stem); // (Image_LoadImage reads vr_extmaps_dir's files so)
            return measure(load, where);
        }
    }
    return {};
}

// The glow images measured, by "<extmaps>|<extmaps dir>|<map>/<texture>" (main thread): a map relit again (its
// settings tried one after another) does not look for them and decode them again. The files are the game folders':
// emptied when they change.
struct GlowCache
{
    ankerl::unordered_dense::map<za::String, Measured> measured;
    auto members()
    {
        return mem::list(measured);
    }
};
mem::Cache<GlowCache> glowCache{"relight glow images", mem::GameDirChange};

[[nodiscard]] Measured cachedGlowImage(const char* map, const char* name)
{
    za::String key{va("%g|%s|%s/%s", vr_extmaps.value, vr_extmaps_dir.string, map, name)};
    const auto found = glowCache.measured.find(key);
    if(found != glowCache.measured.end())
    {
        return found->second;
    }
    const Measured m = glowImage(map, name);
    glowCache.measured.emplace(ZA_MOVE(key), m);
    return m;
}

// ---------------------------------------------------------------------------------------------------------------------
// glow_lights.

struct Strengths
{
    double scale{1.0};                  // all of them (Light Textures)
    double kinds[3]{1.0, 1.0, 1.0};     // fixtures, glows, liquids: the page's on top of the rules' strength lines
};

void lightEntity(za::String& out, const V3& origin, double value, double wait, const char* colour, const char* extra)
{
    char buf[512];
    q_snprintf(buf, sizeof(buf), "{\n\"classname\" \"light\"\n\"origin\" \"%g %g %g\"\n\"light\" \"%d\"\n\"wait\" \"%.2f\"\n\"_color\" \"%s\"\n%s}\n",
        origin[0], origin[1], origin[2], static_cast<int>(value), wait, colour, extra);
    out += buf;
}

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

[[nodiscard]] za::String glowLights(const Bsp& b, const byte* palette, const za::Vector<Rule>& rules, const char* game,
    const char* map, const Strengths& strength, Report& report)
{
    za::String out;
    if(!b.bsp29())
    {
        return out;
    }
    const int offset = b.lumpOffset(2), length = b.lumpLength(2);
    if(length < 4)
    {
        return out;
    }
    const int count = b.i32(offset);
    if(count <= 0 || 4 + count * 4 > length)
    {
        return out;
    }
    double t0 = Sys_DoubleTime();
    const za::Vector<za::Vector<Face>> faces = textureFaces(b, count);
    report.facesMs = (Sys_DoubleTime() - t0) * 1000.0;
    const World world{&b};
    double kinds[3];
    for(int k = 0; k < 3; k++)
    {
        kinds[k] = 1.0;
        for(const Rule& r : rules)
        {
            if(r.pattern.empty() && r.hasStrength[k])
            {
                kinds[k] = r.strength[k];
            }
        }
        kinds[k] *= strength.kinds[k];
    }

    za::Vector<Glow> glows;
    for(int i = 0; i < count; i++)
    {
        const int mip = b.i32(offset + 4 + i * 4);
        if(mip < 0 || faces[static_cast<za::SizeT>(i)].empty())
        {
            continue;
        }
        const za::SizeT base = static_cast<za::SizeT>(offset) + static_cast<za::SizeT>(mip);
        if(base + 40 > b.size)
        {
            continue;
        }
        char name[17] = {};
        memcpy(name, b.data + base, 16);
        const int width = b.i32(base + 16), height = b.i32(base + 20), pix = b.i32(base + 24);
        const za::String low = lower(za::StringView{name});
        const za::SizeT npix = width > 0 && height > 0 ? static_cast<za::SizeT>(width) * static_cast<za::SizeT>(height) : 0;
        const bool pixelsOk = pix > 0 && npix && base + static_cast<za::SizeT>(pix) + npix <= b.size;
        const byte* pixels = pixelsOk ? b.data + base + pix : nullptr;
        if(low.size() && low[0] == '*' && pix > 0 && width * height)
        {
            const Rule rule = textureRule(rules, game, map, name);
            if(rule.hasKind && !strcmp(rule.kind, "liquid") && pixels)
            {
                Glow g;
                g.name = za::String{name};
                g.kind = 2;
                g.faces = &faces[static_cast<za::SizeT>(i)];
                g.rule = rule;
                if(rule.hasColor)
                {
                    g.colour = rule.color;
                }
                else
                {
                    for(int j = 0; j < 3; j++)
                    {
                        long long s = 0;
                        for(za::SizeT k = 0; k < npix; k++)
                        {
                            s += palette[pixels[k] * 3 + j];
                        }
                        g.colour[j] = static_cast<double>(s) / static_cast<double>(npix);
                    }
                }
                q_strlcpy(g.source, "liquid", sizeof(g.source));
                glows.pushBack(ZA_MOVE(g));
            }
            continue;
        }
        if(!name[0] || pix <= 0 || startsWith(low, "sky") || width * height == 0 || !pixels)
        {
            continue;
        }
        const Rule rule = textureRule(rules, game, map, name);
        if(rule.hasKind && !strcmp(rule.kind, "liquid"))
        {
            continue;
        }
        int kind = rule.hasKind ? (!strcmp(rule.kind, "fixture") ? 0 : !strcmp(rule.kind, "glow") ? 1 : 3) :
                                  (low.toStringView().find(za::StringView{"light"}) != za::StringView::nPos ||
                                      low.toStringView().find(za::StringView{"lamp"}) != za::StringView::nPos) ? 0 : 1;
        if(kind == 3)
        {
            continue;
        }
        const double least = kind == 0 ? fixtureMinShare : glowMinShare;
        long long glowing = 0, sum[3] = {0, 0, 0};
        for(za::SizeT k = 0; k < npix; k++)
        {
            if(pixels[k] >= 224 && pixels[k] <= 254)
            {
                glowing++;
                for(int j = 0; j < 3; j++)
                {
                    sum[j] += palette[pixels[k] * 3 + j];
                }
            }
        }
        Glow g;
        g.share = static_cast<double>(glowing) / static_cast<double>(npix);
        q_strlcpy(g.source, "fullbright", sizeof(g.source));
        if(g.share >= least)
        {
            for(int j = 0; j < 3; j++)
            {
                g.colour[j] = static_cast<double>(sum[j]) / static_cast<double>(glowing);
            }
        }
        else
        {
            const double ti = Sys_DoubleTime();
            const Measured luma = cachedGlowImage(map, name);
            report.imagesMs += (Sys_DoubleTime() - ti) * 1000.0;
            if(luma.found && luma.share >= least)
            {
                g.share = luma.share;
                g.colour = luma.colour;
                q_strlcpy(g.source, "luma", sizeof(g.source));
                q_strlcpy(g.where, luma.where, sizeof(g.where));
            }
            else if(kind == 0 && rule.hasKind)
            {
                // Named a fixture but nothing glows: the colour of its brightest tenth (a stable sort, as Python's).
                za::Vector<int> order;
                order.resize(npix);
                for(za::SizeT k = 0; k < npix; k++)
                {
                    order[k] = static_cast<int>(k);
                }
                const auto bright = [&](int k) {
                    const int c = pixels[k];
                    return palette[c * 3] + palette[c * 3 + 1] + palette[c * 3 + 2];
                };
                za::stableSort(order.begin(), order.end(), [&](int a, int c) { return -bright(a) < -bright(c); });
                const za::SizeT top = za::max(static_cast<za::SizeT>(1), npix / 10);
                for(int j = 0; j < 3; j++)
                {
                    long long s = 0;
                    for(za::SizeT k = 0; k < top; k++)
                    {
                        s += palette[pixels[order[k]] * 3 + j];
                    }
                    g.colour[j] = static_cast<double>(s) / static_cast<double>(top);
                }
                g.share = 0.1;
                q_strlcpy(g.source, "brightest", sizeof(g.source));
            }
            else
            {
                continue;
            }
        }
        if(rule.hasColor)
        {
            g.colour = rule.color;
        }
        g.name = za::String{name};
        g.kind = kind;
        g.faces = &faces[static_cast<za::SizeT>(i)];
        g.things = thingsOf(faces[static_cast<za::SizeT>(i)]);
        g.rule = rule;
        glows.pushBack(ZA_MOVE(g));
    }
    t0 = Sys_DoubleTime();
    za::Vector<const Thing*> everyone, fixtures;
    for(const Glow& g : glows)
    {
        for(const Thing& t : g.things)
        {
            everyone.pushBack(&t);
            if(g.kind == 0)
            {
                fixtures.pushBack(&t);
            }
        }
    }
    za::Vector<MapLight> lamps;
    if(!fixtures.empty())
    {
        lamps = mapLights(entitiesText(b));
    }

    for(const Glow& g : glows)
    {
        double r = g.colour[0], gg = g.colour[1], bb = g.colour[2];
        const Rule& rule = g.rule;
        za::Vector<double> made;
        char colour[64];
        if(g.kind == 2)
        {
            const double top = za::max(r, gg, bb, 1.0);
            q_snprintf(colour, sizeof(colour), "%d %d %d", static_cast<int>(r * 255 / top), static_cast<int>(gg * 255 / top),
                static_cast<int>(bb * 255 / top));
            const double value = (rule.hasLight ? rule.light : liquidLight) * rule.scale * strength.scale * kinds[2];
            const za::Vector<V3> spots = value >= 12 ? liquidSpots(*g.faces, world) : za::Vector<V3>{};
            for(const V3& p : spots)
            {
                int crowded = 0;
                for(const V3& q : spots)
                {
                    crowded += isNear(p, q, liquidRoom) ? 1 : 0;
                }
                const double v = value * za::pow(za::min(1.0, liquidLone / crowded), liquidCrowd);
                made.pushBack(v);
                lightEntity(out, p, v, 1 / rule.reach, colour, "\"_dirt\" \"-1\"\n");
            }
        }
        else
        {
            const double top = za::max(r, gg, bb, 1.0);
            const double sat = (top - za::min(r, gg, bb)) / top;
            const double white = 0.25 - 0.15 * sat;
            r = r * (1 - white) + top * white;
            gg = gg * (1 - white) + top * white;
            bb = bb * (1 - white) + top * white;
            q_snprintf(colour, sizeof(colour), "%d %d %d", static_cast<int>(r * 255 / top), static_cast<int>(gg * 255 / top),
                static_cast<int>(bb * 255 / top));
            const double wait = 1 / (1 + sat) / rule.reach;
            const double own = rule.scale * strength.scale * kinds[g.kind];
            if(g.kind == 0)
            {
                const double each = (rule.hasLight ? rule.light : fixtureLight) * own * 1.0 * (0.6 + 0.4 * za::min(1.0, g.share * 5));
                for(const Thing& thing : g.things)
                {
                    double value = each;
                    const V3& c = thing.faces[0]->centre;
                    bool by = false;
                    double strongest = 0.0;
                    for(const MapLight& l : lamps)
                    {
                        if(l.off)
                        {
                            continue;
                        }
                        for(const Face* f : thing.faces)
                        {
                            if(isNear(l.origin, f->centre, fixtureNear))
                            {
                                strongest = by ? za::max(strongest, l.light) : l.light;
                                by = true;
                                break;
                            }
                        }
                    }
                    if(by)
                    {
                        value *= 1 - (1 - fixtureLit) * za::min(1.0, strongest / 300.0);
                    }
                    int room = 0;
                    for(const Thing* t : fixtures)
                    {
                        room += isNear(c, t->faces[0]->centre, glowRoom) ? 1 : 0;
                    }
                    value /= za::pow(static_cast<double>(room), 0.5);
                    PySum area;
                    for(const Face* f : thing.faces)
                    {
                        area.add(f->area);
                    }
                    value *= za::min(1.0, za::max(0.5, za::pow(area.value() / 1024.0, 0.5)));
                    const za::Vector<V3> spots = fixtureSpots(thing.faces, world);
                    for(const V3& p : spots)
                    {
                        const double v = za::min(300 * own * 1.0, value * za::max(0.4, za::pow(static_cast<double>(spots.size()), -0.5)));
                        if(v >= 12)
                        {
                            made.pushBack(v);
                            lightEntity(out, p, v, wait, colour, "\"_dirt\" \"-1\"\n");
                        }
                    }
                }
            }
            else
            {
                const double budget = glowBudget * za::min(1.0, 0.4 + g.share * 2) * own;
                const double cap = 130 * own * (1 + 0.3 * sat);
                bool smallFaces = true;
                for(const Face& f : *g.faces)
                {
                    smallFaces = smallFaces && f.area <= 2 * sq(surflight);
                }
                if(smallFaces)
                {
                    for(const Thing& thing : g.things)
                    {
                        const double value = za::min(cap, budget * (1 + sat) / crowd(thing, everyone));
                        za::Vector<V3> spots;
                        for(const Face* f : thing.faces)
                        {
                            const V3 p = along(f->centre, f->normal, 2);
                            if(!world.solid(p))
                            {
                                spots.pushBack(p);
                            }
                        }
                        for(const V3& p : spots)
                        {
                            const double v = value / za::pow(static_cast<double>(spots.size()), 0.5);
                            if(v >= 12)
                            {
                                made.pushBack(v);
                                lightEntity(out, p, v, wait, colour, "");
                            }
                        }
                    }
                }
                else
                {
                    double most = 0.0;
                    bool first = true;
                    for(const Thing& t : g.things)
                    {
                        const double c = crowd(t, everyone);
                        most = first ? c : za::max(most, c);
                        first = false;
                    }
                    const double value = za::min(cap, budget * (1 + sat) / most);
                    if(value >= 12)
                    {
                        made.pushBack(value);
                        char buf[512];
                        q_snprintf(buf, sizeof(buf),
                            "{\n\"classname\" \"light\"\n\"_surface\" \"%s\"\n\"light\" \"%d\"\n\"wait\" \"%.2f\"\n\"_color\" \"%s\"\n\"_surface_offset\" \"2\"\n}\n",
                            g.name.cStr(), static_cast<int>(value), wait, colour);
                        out += buf;
                    }
                }
            }
        }
        report.textures++;
        report.lights += static_cast<int>(made.size());
        double lo = 0.0, hi = 0.0;
        for(za::SizeT k = 0; k < made.size(); k++)
        {
            lo = k ? za::min(lo, made[k]) : made[k];
            hi = k ? za::max(hi, made[k]) : made[k];
        }
        char line[MAX_OSPATH + 160];
        q_snprintf(line, sizeof(line), "  %-16s %-7s %-10s glows %5.1f%%  lights %4d  %s%s%s\n", g.name.cStr(),
            kindNames[g.kind], g.source, g.share * 100, static_cast<int>(made.size()),
            made.empty() ? "(none)" : va("light %d..%d", static_cast<int>(lo), static_cast<int>(hi)), g.where[0] ? "  " : "",
            g.where);
        report.lines += line;
    }
    report.spotsMs = (Sys_DoubleTime() - t0) * 1000.0;
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// The job: one map relit at a time (main thread).

enum class Phase
{
    Idle,
    Running,   // light running
    Saving,    // the result in place; the game saved, its save being written, before the load
    Done,
    Failed,
};

struct Job
{
    Phase phase{Phase::Idle};
    char map[MAX_QPATH]{};     // e1m1
    char game[MAX_QPATH]{};    // id1: relit_custom/<game>/maps
    za::String work;           // <gamedir>/relit_custom/_work
    za::String original;       // the map's own entities, put back after
    za::String settings;       // how it is lit (the .relight file, the page)
    double started{0.0};       // (Sys_DoubleTime)
    double nextPoll{0.0};
    za::I64 inputWritten{0};   // the input .bsp's write time: light's results are newer
    za::SizeT logRead{0};
    char stage[64]{};
    int percent{-1};
    int lights{0};
    bool reload{false};
    za::Vector<char> logTail; // the log's last bytes (a failure's lines)
};
Job job;

// The page's and the commands' last word on it (statusLine).
char status[384] = "";

void say(const char* text)
{
    q_strlcpy(status, text, sizeof(status));
    Con_Printf("Relight: %s\n", text);
}

// The game folder relit_custom/ is written in (where the saves go: com_gamedir).
[[nodiscard]] za::String customRoot()
{
    return files::join(za::StringView{com_gamedir}, za::StringView{"relit_custom"});
}

[[nodiscard]] Strengths strengths()
{
    Strengths s;
    s.scale = za::max(0.0, static_cast<double>(vr_relight_strength.value));
    s.kinds[0] = za::max(0.0, static_cast<double>(vr_relight_lamps.value));
    s.kinds[1] = za::max(0.0, static_cast<double>(vr_relight_glows.value));
    s.kinds[2] = za::max(0.0, static_cast<double>(vr_relight_liquids.value));
    return s;
}

// light's options for the look (relight_maps.py's DEFAULT_LIGHT_ARGS at the defaults).
void lookOptions(za::Vector<za::String>& args)
{
    const auto add = [&](const char* a) { args.pushBack(za::String{a}); };
    // All the cores but one: the game keeps one for itself (light also runs below normal priority).
    add("-threads");
    const unsigned cores = za::Thread::hardwareConcurrency();
    add(va("%u", cores > 1u ? cores - 1u : 1u));
    add(vr_relight_quality.value >= 1.f ? "-extra4" : "-extra");
    if(vr_relight_ao.value > 0.f)
    {
        add("-dirt");
        add("-dirtscale");
        add(va("%g", vr_relight_ao.value));
        add("-dirtdepth");
        add("96");
    }
    if(vr_relight_bounce.value > 0.f)
    {
        add("-bounce");
        add("-bouncescale");
        add(va("%g", vr_relight_bounce.value));
    }
    if(vr_relight_minlight.value > 0.f)
    {
        add("-minlight");
        add(va("%g", vr_relight_minlight.value));
    }
    for(const za::StringView w : wordsOf(za::StringView{lightOptions}))
    {
        args.pushBack(za::String{w});
    }
}

[[nodiscard]] za::String settingsText()
{
    return za::String{va("Light Textures %.2fx (lamps %.2fx, glows %.2fx, lava %.2fx), Map Lights %.2fx, Sunlight %.2fx, "
                         "Bounce %.2f, Ambient Occlusion %.2f, Minimum Light %.0f, %s",
        vr_relight_strength.value, vr_relight_lamps.value, vr_relight_glows.value, vr_relight_liquids.value,
        vr_relight_maplights.value, vr_relight_sunlight.value, vr_relight_bounce.value, vr_relight_ao.value,
        vr_relight_minlight.value, vr_relight_quality.value >= 1.f ? "smooth" : "fast")};
}

// ericw-tools' light: vr_relight_tool, else the one Quake VR ships (tools/ericw-tools/ in a game folder), ERICW_LIGHT,
// PATH, the author's. Empty: none.
[[nodiscard]] za::String findTool()
{
#ifdef _WIN32
    constexpr const char* exe = "light.exe";
#else
    constexpr const char* exe = "light";
#endif
    if(vr_relight_tool.string[0])
    {
        return Sys_FileType(vr_relight_tool.string) & FS_ENT_FILE ? za::String{vr_relight_tool.string} : za::String{};
    }
    for(const searchpath_t* s = com_searchpaths; s; s = s->next)
    {
        if(s->pack)
        {
            continue;
        }
        const char* path = va("%s/tools/ericw-tools/%s", s->filename, exe);
        if(Sys_FileType(path) & FS_ENT_FILE)
        {
            return za::String{path};
        }
    }
    if(const char* env = getenv("ERICW_LIGHT"); env && (Sys_FileType(env) & FS_ENT_FILE))
    {
        return za::String{env};
    }
    za::String onPath = process::onPath(exe);
    if(!onPath.empty())
    {
        return onPath;
    }
#ifdef _WIN32
    if(Sys_FileType(authorsLight) & FS_ENT_FILE)
    {
        return za::String{authorsLight};
    }
#endif
    return {};
}

// The map's file as the game finds it, and the folder of its game (id1, hipnotic, quakevr, a package's): false if
// there is no such map.
[[nodiscard]] bool mapSource(const char* map, za::Vector<unsigned char>& data, char* game, size_t size, bool& fromRelit)
{
    char name[MAX_QPATH];
    q_snprintf(name, sizeof(name), "maps/%s.bsp", map);
    if(!VR_MapGameFolder(name, game, size))
    {
        return false;
    }
    // relight_maps.py's copy (its water-vis patch kept), else the map itself.
    char relit[MAX_QPATH * 2];
    q_snprintf(relit, sizeof(relit), "relit/%s/%s", game, name);
    fromRelit = COM_FileExists(relit, nullptr);
    byte* bytes = COM_LoadMallocFile(fromRelit ? relit : name, nullptr);
    if(!bytes)
    {
        return false;
    }
    data.resize(static_cast<za::SizeT>(com_filesize));
    memcpy(data.data(), bytes, data.size());
    VR_HeapFree(bytes);
    return true;
}

[[nodiscard]] za::Vector<Rule> rulesNow()
{
    byte* text = COM_LoadMallocFile("relight_textures.cfg", nullptr);
    if(!text)
    {
        Con_Printf("Relight: relight_textures.cfg not found: textures named *light* or *lamp* are lamps, other glowing "
                   "ones glow, liquids light nothing\n");
        return {};
    }
    za::String copy{reinterpret_cast<const char*>(text), static_cast<za::SizeT>(com_filesize)};
    VR_HeapFree(text);
    return loadRules(copy.cStr(), "relight_textures.cfg");
}

[[nodiscard]] const byte* paletteNow(za::Vector<unsigned char>& keep)
{
    byte* pal = COM_LoadMallocFile("gfx/palette.lmp", nullptr);
    if(!pal || com_filesize < 768)
    {
        if(pal)
        {
            VR_HeapFree(pal);
        }
        return nullptr;
    }
    keep.resize(768);
    memcpy(keep.data(), pal, 768);
    VR_HeapFree(pal);
    return keep.data();
}

// The map now in play ("e1m1"), or false.
[[nodiscard]] bool currentMap(char* out, size_t size)
{
    if(!cl.worldmodel || strncmp(cl.worldmodel->name, "maps/", 5))
    {
        return false;
    }
    COM_StripExtension(cl.worldmodel->name + 5, out, size);
    return out[0] != 0;
}

// The lights relight_maps.py would give light for this map, with the page's settings: (text, report).
[[nodiscard]] za::String lightsFor(const Bsp& b, const char* game, const char* map, Report& report)
{
    za::Vector<unsigned char> pal;
    const byte* palette = paletteNow(pal);
    if(!palette)
    {
        Con_Printf("Relight: gfx/palette.lmp not found: glowing textures will not light\n");
        return {};
    }
    const za::Vector<Rule> rules = rulesNow();
    VR_FileCacheEnable(1); // (each folder listed once while the glow images are looked for, as in a map's load)
    za::String out = glowLights(b, palette, rules, game, map, strengths(), report);
    VR_FileCacheEnable(0);
    return out;
}

// Whether the game can be saved now (Host_Savegame_f's conditions).
[[nodiscard]] bool canSave()
{
    if(!sv.active || sv.nomonsters || cl.intermission || svs.maxclients != 1)
    {
        return false;
    }
    for(int i = 0; i < svs.maxclients; i++)
    {
        if(svs.clients[i].active && svs.clients[i].edict->v.health <= 0)
        {
            return false;
        }
    }
    return true;
}

void start(const char* requested)
{
    if(job.phase == Phase::Running || job.phase == Phase::Saving)
    {
        say(va("already relighting %s", job.map));
        return;
    }
    char map[MAX_QPATH], current[MAX_QPATH];
    const bool inPlay = currentMap(current, sizeof(current));
    q_strlcpy(map, current, sizeof(map));
    if(requested && requested[0])
    {
        COM_StripExtension(requested, map, sizeof(map));
    }
    else if(!inPlay)
    {
        say("no map loaded: start a map first, or name one (vr_relight <map>)");
        job.phase = Phase::Failed;
        return;
    }
    const za::String tool = findTool();
    if(tool.empty())
    {
        say("ericw-tools' light not found: set vr_relight_tool to its light.exe (docs/RELIGHTING.md)");
        job.phase = Phase::Failed;
        return;
    }
    za::Vector<unsigned char> data;
    char game[MAX_QPATH];
    bool fromRelit = false;
    if(!mapSource(map, data, game, sizeof(game), fromRelit))
    {
        say(va("maps/%s.bsp not found", map));
        job.phase = Phase::Failed;
        return;
    }
    const Bsp b{data.data(), data.size()};
    if(!b.valid())
    {
        say(va("maps/%s.bsp is not a Quake map light can relight", map));
        job.phase = Phase::Failed;
        return;
    }

    const double t0 = Sys_DoubleTime();
    Report report;
    const za::String lights = lightsFor(b, game, map, report);
    const za::String original = entitiesText(b);
    // id's games' maps lose the re-release's worldspawn light settings (as relight_maps.py does); other maps keep
    // their mappers'.
    const bool idGame = !q_strcasecmp(game, "id1") || !q_strcasecmp(game, "hipnotic") || !q_strcasecmp(game, "rogue");
    za::String entities = idLightValues(idGame ? withoutMapLightSettings(original) : original);
    entities = scaledLights(entities, za::max(0.0, static_cast<double>(vr_relight_maplights.value)),
        za::max(0.0, static_cast<double>(vr_relight_sunlight.value)));
    entities += lights;

    job = Job{};
    q_strlcpy(job.map, map, sizeof(job.map));
    q_strlcpy(job.game, game, sizeof(job.game));
    job.work = files::join(customRoot(), za::StringView{"_work"});
    job.original = original;
    job.settings = settingsText();
    job.lights = report.lights;
    job.reload = inPlay && !strcmp(current, map) && vr_relight_reload.value != 0.f;
    files::createDirectories(job.work.cStr());
    const za::String bsp = files::join(job.work, za::StringView{va("%s.bsp", map)});
    const za::String log = files::join(job.work, za::StringView{va("%s.txt", map)});
    const za::Vector<unsigned char> input = withEntities(b, entities);
    if(!files::writeBytes(bsp.cStr(), input.data(), input.size()))
    {
        say(va("can't write %s", bsp.cStr()));
        job.phase = Phase::Failed;
        return;
    }
    job.inputWritten = files::lastWriteTime(bsp.cStr());
    za::Vector<za::String> args;
    lookOptions(args);
    args.pushBack(bsp);
    za::String error;
    job.started = Sys_DoubleTime();
    if(!process::start(tool, args, job.work, log, error))
    {
        say(va("%s: %s", tool.cStr(), error.cStr()));
        job.phase = Phase::Failed;
        return;
    }
    job.phase = Phase::Running;
    say(va("relighting %s (%s%s; %d lights from its textures, made in %.0f ms) with %s", map, game,
        fromRelit ? ", relight_maps.py's copy" : "", report.lights, (job.started - t0) * 1000.0, tool.cStr()));
    q_strlcpy(job.stage, "starting", sizeof(job.stage));
}

// The light log's new lines: its stage ("---- LightWorld ----") and percentage ("[ 45%]").
void readProgress()
{
    const za::String log = files::join(job.work, za::StringView{va("%s.txt", job.map)});
    FILE* f = Sys_fopen(log.cStr(), "rb");
    if(!f)
    {
        return;
    }
    fseek(f, static_cast<long>(job.logRead), SEEK_SET);
    char buf[4096];
    size_t n;
    while((n = fread(buf, 1, sizeof(buf), f)) > 0)
    {
        job.logRead += n;
        for(size_t i = 0; i < n; i++)
        {
            job.logTail.pushBack(buf[i]);
        }
    }
    fclose(f);
    if(job.logTail.size() > 8192)
    {
        za::Vector<char> keep;
        keep.resize(4096);
        memcpy(keep.data(), job.logTail.data() + job.logTail.size() - 4096, 4096);
        job.logTail = ZA_MOVE(keep);
    }
    // The last stage and percentage in what is kept.
    const za::StringView text{job.logTail.data(), job.logTail.size()};
    const za::SizeT stage = text.rfind(za::StringView{"---- "});
    if(stage != za::StringView::nPos)
    {
        const za::SizeT end = text.find(za::StringView{" ----"}, stage + 5);
        if(end != za::StringView::nPos && end - stage - 5 < sizeof(job.stage))
        {
            const za::StringView s = text.substrByPosLen(stage + 5, end - stage - 5);
            if(strncmp(job.stage, s.data(), s.size()) || job.stage[s.size()])
            {
                memcpy(job.stage, s.data(), s.size());
                job.stage[s.size()] = 0;
                job.percent = -1;
            }
        }
    }
    const za::SizeT pct = text.rfind('%');
    if(pct != za::StringView::nPos && pct >= 3 && text[pct - 4] == '[' && (stage == za::StringView::nPos || pct > stage))
    {
        job.percent = atoi(va("%.*s", 3, text.data() + pct - 3));
    }
}

// The log's last lines (a failure's reason).
[[nodiscard]] za::String logEnd(int lines)
{
    za::SizeT i = job.logTail.size();
    int seen = 0;
    while(i > 0 && seen <= lines)
    {
        i--;
        seen += job.logTail[i] == '\n' ? 1 : 0;
    }
    za::String out{job.logTail.data() + i, job.logTail.size() - i};
    for(char& c : out)
    {
        c = c == '\r' ? '\n' : c;
    }
    return out;
}

[[nodiscard]] bool copyFile(const za::String& from, const za::String& to)
{
    za::Vector<unsigned char> data;
    return files::readBytes(from.cStr(), data) && files::writeBytes(to.cStr(), data.data(), data.size());
}

// light ended well: the map's own entities back, the result where the game loads it.
void finish()
{
    const za::String bspIn = files::join(job.work, za::StringView{va("%s.bsp", job.map)});
    const za::String lit = files::join(job.work, za::StringView{va("%s.lit", job.map)});
    const za::String lux = files::join(job.work, za::StringView{va("%s.lux", job.map)});
    // (light's results, not an earlier run's: written since its input was)
    const za::I64 since = job.inputWritten;
    za::Vector<unsigned char> data;
    if(!files::readBytes(bspIn.cStr(), data) || !files::isFile(lit.cStr()) || files::lastWriteTime(lit.cStr()) < since)
    {
        say(va("light made no .lit for %s:\n%s", job.map, logEnd(6).cStr()));
        job.phase = Phase::Failed;
        return;
    }
    const Bsp b{data.data(), data.size()};
    if(!b.valid())
    {
        say(va("light's %s.bsp can't be read", job.map));
        job.phase = Phase::Failed;
        return;
    }
    const za::Vector<unsigned char> out = withEntities(b, job.original);
    const za::String dir = files::join(customRoot(), za::StringView{va("%s/maps", job.game)});
    files::createDirectories(dir.cStr());
    const za::String outBsp = files::join(dir, za::StringView{va("%s.bsp", job.map)});
    const bool hasLux = files::isFile(lux.cStr()) && files::lastWriteTime(lux.cStr()) >= since;
    const bool ok = files::writeBytes(outBsp.cStr(), out.data(), out.size()) &&
                    copyFile(lit, files::join(dir, za::StringView{va("%s.lit", job.map)})) &&
                    (!hasLux || copyFile(lux, files::join(dir, za::StringView{va("%s.lux", job.map)})));
    const double seconds = Sys_DoubleTime() - job.started;
    za::String note{va("relit in the game (vr_relight) in %.1f s, %d lights from its textures\n", seconds, job.lights)};
    note += job.settings;
    note += "\n";
    if(!ok || !files::writeText(files::join(dir, za::StringView{va("%s.relight", job.map)}).cStr(), note))
    {
        say(va("can't write the relit %s into %s", job.map, dir.cStr()));
        job.phase = Phase::Failed;
        return;
    }
    VR_FileCacheForget();
    char now[MAX_QPATH];
    const bool same = currentMap(now, sizeof(now)) && !strcmp(now, job.map);
    if(!job.reload || !same)
    {
        say(va("%s relit in %.1f s: loaded from its next start (%s)", job.map, seconds, dir.cStr()));
        job.phase = Phase::Done;
        return;
    }
    if(canSave())
    {
        // Saved now (its file written on the save thread), loaded once written (poll): ahead of whatever waits in the
        // command buffer (a script's waits).
        sv.lastsave[0] = 0;
        Cmd_ExecuteString("save \"autosave/relight\" 0", src_command);
        if(!strcmp(sv.lastsave, "autosave/relight.sav"))
        {
            job.phase = Phase::Saving;
            say(va("%s relit in %.1f s: reloading where you are", job.map, seconds));
            return;
        }
    }
    Cbuf_InsertText("restart\n");
    job.phase = Phase::Done;
    say(va("%s relit in %.1f s: the map restarted (the game can't be saved here)", job.map, seconds));
}

// ---------------------------------------------------------------------------------------------------------------------
// Commands.

void relightCommand()
{
    start(Cmd_Argc() > 1 ? Cmd_Argv(1) : nullptr);
}

void cancelCommand()
{
    if(job.phase != Phase::Running)
    {
        say("nothing is being relit");
        return;
    }
    process::stop();
    job.phase = Phase::Failed;
    say(va("relighting %s cancelled", job.map));
}

// The in-game relight of the map in play (or the one named) removed: it plays with relight_maps.py's light, or its own.
void revertCommand()
{
    char map[MAX_QPATH];
    if(Cmd_Argc() > 1)
    {
        COM_StripExtension(Cmd_Argv(1), map, sizeof(map));
    }
    else if(!currentMap(map, sizeof(map)))
    {
        say("no map loaded");
        return;
    }
    char game[MAX_QPATH];
    if(!VR_MapGameFolder(va("maps/%s.bsp", map), game, sizeof(game)))
    {
        say(va("maps/%s.bsp not found", map));
        return;
    }
    const za::String dir = files::join(customRoot(), za::StringView{va("%s/maps", game)});
    int removed = 0;
    for(const char* ext : {"bsp", "lit", "lux", "relight"})
    {
        removed += files::remove(files::join(dir, za::StringView{va("%s.%s", map, ext)}).cStr()) ? 1 : 0;
    }
    VR_FileCacheForget();
    say(removed ? va("%s's in-game relight removed: its other light from its next start", map) :
                  va("%s has no in-game relight", map));
}

void statusCommand()
{
    Con_Printf("%s\n%s\n%s\n", statusLine(), mapLine(), toolLine());
}

void defaultsCommand()
{
    for(cvar_t* c : {&vr_relight_strength, &vr_relight_lamps, &vr_relight_glows, &vr_relight_liquids,
            &vr_relight_maplights, &vr_relight_sunlight, &vr_relight_bounce, &vr_relight_ao, &vr_relight_minlight,
            &vr_relight_quality})
    {
        Cvar_SetQuick(c, c->default_string);
    }
    say("settings back to their defaults (relight_maps.py's look)");
}

// vr_relight_lights [file]: the lights the map in play's textures get with the page's settings, into the file
// (relight_lights.txt in the game folder), and a line each texture: to compare with relight_maps.py --list-glows.
void lightsCommand()
{
    char map[MAX_QPATH];
    if(!currentMap(map, sizeof(map)))
    {
        say("no map loaded");
        return;
    }
    za::Vector<unsigned char> data;
    char game[MAX_QPATH];
    bool fromRelit = false;
    if(!mapSource(map, data, game, sizeof(game), fromRelit))
    {
        say(va("maps/%s.bsp not found", map));
        return;
    }
    const Bsp b{data.data(), data.size()};
    if(!b.valid())
    {
        say("not a Quake map");
        return;
    }
    Report report;
    const double t0 = Sys_DoubleTime();
    const za::String lights = lightsFor(b, game, map, report);
    const double ms = (Sys_DoubleTime() - t0) * 1000.0;
    const za::String file = Cmd_Argc() > 1 ? za::String{Cmd_Argv(1)} : files::join(za::StringView{com_gamedir}, za::StringView{"relight_lights.txt"});
    const bool written = files::writeBytes(file.cStr(), lights.data(), lights.size());
    Con_Printf("%s/%s:\n", game, map);
    files::forLines(report.lines, [](za::StringView line) { Con_Printf("%.*s\n", static_cast<int>(line.size()), line.data()); });
    Con_Printf("Relight: %d lights from %d glowing textures (%.0f ms: faces %.0f, glow images %.0f, lights %.0f)%s%s\n",
        report.lights, report.textures, ms, report.facesMs, report.imagesMs, report.spotsMs,
        written ? ", written to " : "", written ? file.cStr() : "");
}

// Formats for the page (file scope: valid until the function's next call).
char statusText[512];
char mapText[512];
char toolText[MAX_OSPATH + 64];
double toolCheckedAt{0.0}; // when toolText was made (Sys_DoubleTime)

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_relight", relightCommand);
    Cmd_AddCommand("vr_relight_cancel", cancelCommand);
    Cmd_AddCommand("vr_relight_revert", revertCommand);
    Cmd_AddCommand("vr_relight_status", statusCommand);
    Cmd_AddCommand("vr_relight_defaults", defaultsCommand);
    Cmd_AddCommand("vr_relight_lights", lightsCommand);
}

void poll()
{
    if(job.phase == Phase::Saving)
    {
        if(Host_IsSaving())
        {
            return;
        }
        Cbuf_InsertText("load \"autosave/relight\"\n");
        job.phase = Phase::Done;
        return;
    }
    const double now = Sys_DoubleTime();
    if(job.phase != Phase::Running || now < job.nextPoll)
    {
        return;
    }
    job.nextPoll = now + 0.1;
    readProgress();
    if(process::running())
    {
        return;
    }
    readProgress();
    const int code = process::lastExitCode();
    if(code != 0)
    {
        say(va("light failed on %s (exit %d):\n%s", job.map, code, logEnd(6).cStr()));
        job.phase = Phase::Failed;
        return;
    }
    finish();
}

void shutdown()
{
    if(job.phase == Phase::Running)
    {
        process::stop();
        job.phase = Phase::Failed;
    }
    process::stop(); // (its job object closed)
}

const char* statusLine()
{
    if(job.phase == Phase::Running)
    {
        q_snprintf(statusText, sizeof(statusText), "Relighting %s: %s%s (%.0f s)", job.map, job.stage,
            job.percent >= 0 ? va(" %d%%", job.percent) : "", Sys_DoubleTime() - job.started);
        return statusText;
    }
    q_strlcpy(statusText, status[0] ? status : "Not relit in this session.", sizeof(statusText));
    for(char* c = statusText; *c; c++)
    {
        *c = *c == '\n' ? ' ' : *c; // (a failure's log lines, on one line)
    }
    return statusText;
}

const char* mapLine()
{
    char map[MAX_QPATH];
    if(!currentMap(map, sizeof(map)))
    {
        return "No map loaded.";
    }
    char game[MAX_QPATH];
    if(!VR_MapGameFolder(va("maps/%s.bsp", map), game, sizeof(game)))
    {
        q_snprintf(mapText, sizeof(mapText), "%s: its light as it is.", map);
        return mapText;
    }
    const char* custom = va("relit_custom/%s/maps/%s.bsp", game, map);
    const bool hasCustom = COM_FileExists(custom, nullptr);
    const bool hasRelit = COM_FileExists(va("relit/%s/maps/%s.bsp", game, map), nullptr);
    const char* loaded = !vr_relit_maps.value ? "Relit Maps off: its own light" :
                         hasCustom && vr_relight_use.value ? "relit in the game" :
                         hasRelit ? "relit by relight_maps.py" : "its own light";
    q_snprintf(mapText, sizeof(mapText), "%s (%s): %s%s.", map, game, loaded,
        hasCustom && !vr_relight_use.value ? " (an in-game relight is there, not used)" : "");
    return mapText;
}

const char* toolLine()
{
    // (looked for again every two seconds while the page shows it: a file check a search path folder)
    const double now = Sys_DoubleTime();
    if(toolText[0] && now < toolCheckedAt + 2.0)
    {
        return toolText;
    }
    toolCheckedAt = now;
    const za::String tool = findTool();
    q_snprintf(toolText, sizeof(toolText), "%s", tool.empty() ? "light.exe: not found (vr_relight_tool)" : va("light.exe: %s", tool.cStr()));
    return toolText;
}

} // namespace qvr::relight
