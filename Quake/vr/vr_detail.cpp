// vr_detail.cpp -- see vr_detail.hpp.

#include "vr_detail.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" texture_t *r_notexture_mip, *r_notexture_mip2; // gl_model.c

namespace qvr::detail
{
namespace
{

constexpr const char* cfgPath = "textures/vr/detail.cfg";
constexpr GLenum unit = GL_TEXTURE12; // DetailTex in gl_shaders.h
constexpr std::size_t maxKinds = 16;
// The fine octave: this many times smaller, off the coarse one's grid (DetailFactor).
constexpr float fineScale = 3.7f;

struct Kind
{
    std::string name;
    float size{64.f};     // world units a tile of the image covers
    float strength{0.5f}; // 1 + strength x (2 x detail - 1)
    bool grain{false};    // a grain along the image's x, to be turned along a texture's own
};

// The kinds without a detail.cfg (or with no kind lines in it), in make_detail.py's order.
const Kind defaultKinds[] = {{"stone", 64.f, 0.8f, false}, {"metal", 48.f, 0.6f, true}, {"wood", 64.f, 0.8f, true},
    {"dirt", 48.f, 0.75f, false}, {"organic", 48.f, 0.75f, false}, {"plaster", 32.f, 0.6f, false}};

constexpr int kindUnset = -3, kindAuto = -2, kindNone = -1;
enum Grain
{
    GrainUnset = -1,
    GrainS,
    GrainT,
    GrainAuto
};

struct Rule
{
    std::string map;     // empty: every map
    std::string pattern; // the texture's name
    int kind{kindUnset};
    float scale{-1.f};
    float strength{-1.f};
    int grain{GrainUnset};
    int line{0};
};

std::vector<Kind> kinds;
std::vector<Rule> rules;
bool cfgLoaded = false;

GLuint array = 0;
bool arrayTried = false;
int arraySize = 0;

// A texture's detail as the shader takes it (VR_DetailCall), and how it was chosen (vr_detail_list).
struct Entry
{
    char name[16]{};
    float v[4]{};
    int kind{kindNone};
    bool byRule{false};
    bool swapped{false};
};
std::unordered_map<const texture_t*, Entry> cache;
const qmodel_t* cacheWorld = nullptr;
char cacheWorldName[MAX_QPATH] = {};

std::string lower(const char* s)
{
    std::string r(s);
    for(char& c : r)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return r;
}

// * any run of characters, ? one; both lower case.
bool match(const char* p, const char* s)
{
    for(; *p; p++, s++)
    {
        if(*p == '*')
        {
            for(const char* t = s;; t++)
            {
                if(match(p + 1, t))
                {
                    return true;
                }
                if(!*t)
                {
                    return false;
                }
            }
        }
        if(!*s || (*p != '?' && *p != *s))
        {
            return false;
        }
    }
    return !*s;
}

int findKind(const std::string& name)
{
    for(std::size_t i = 0; i < kinds.size(); i++)
    {
        if(kinds[i].name == name)
        {
            return static_cast<int>(i);
        }
    }
    return kindUnset;
}

// ---- detail.cfg ------------------------------------------------------------------------------------------------

void loadCfg()
{
    cfgLoaded = true;
    kinds.clear();
    rules.clear();
    byte* data = COM_LoadMallocFile(cfgPath, nullptr);
    if(!data)
    {
        Con_DPrintf("VR: no %s; detail textures by colour alone\n", cfgPath);
    }
    std::vector<std::vector<std::string>> lines;
    std::vector<int> lineNumbers;
    if(data)
    {
        const char* p = reinterpret_cast<const char*>(data);
        int number = 0;
        while(*p)
        {
            const char* end = std::strchr(p, '\n');
            std::string line(p, end ? end - p : std::strlen(p));
            p = end ? end + 1 : p + line.size();
            number++;
            line = line.substr(0, line.find('#'));
            std::vector<std::string> tokens;
            std::size_t i = 0;
            while(i < line.size())
            {
                while(i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
                {
                    i++;
                }
                std::size_t j = i;
                while(j < line.size() && !std::isspace(static_cast<unsigned char>(line[j])))
                {
                    j++;
                }
                if(j > i)
                {
                    tokens.push_back(lower(line.substr(i, j - i).c_str()));
                }
                i = j;
            }
            if(!tokens.empty())
            {
                lines.push_back(std::move(tokens));
                lineNumbers.push_back(number);
            }
        }
        std::free(data);
    }

    const auto value = [](const std::string& token, const char* key, std::string& out) {
        const std::size_t n = std::strlen(key);
        if(token.size() > n && token.compare(0, n, key) == 0 && token[n] == '=')
        {
            out = token.substr(n + 1);
            return true;
        }
        return false;
    };

    // The kinds first (the rules name them).
    for(std::size_t l = 0; l < lines.size(); l++)
    {
        const auto& t = lines[l];
        if(t[0] != "kind")
        {
            continue;
        }
        if(t.size() < 2 || kinds.size() >= maxKinds)
        {
            Con_Warning("%s:%d: a kind needs a name (at most %d kinds)\n", cfgPath, lineNumbers[l], static_cast<int>(maxKinds));
            continue;
        }
        Kind k;
        k.name = t[1];
        for(std::size_t i = 2; i < t.size(); i++)
        {
            std::string v;
            if(value(t[i], "size", v))
            {
                k.size = std::clamp(static_cast<float>(std::atof(v.c_str())), 1.f, 4096.f);
            }
            else if(value(t[i], "strength", v))
            {
                k.strength = std::clamp(static_cast<float>(std::atof(v.c_str())), 0.f, 4.f);
            }
            else if(value(t[i], "grain", v))
            {
                k.grain = std::atoi(v.c_str()) != 0;
            }
            else
            {
                Con_Warning("%s:%d: unknown setting %s\n", cfgPath, lineNumbers[l], t[i].c_str());
            }
        }
        const int existing = findKind(k.name);
        if(existing >= 0)
        {
            kinds[existing] = k;
        }
        else
        {
            kinds.push_back(k);
        }
    }
    if(kinds.empty())
    {
        kinds.assign(std::begin(defaultKinds), std::end(defaultKinds));
    }

    for(std::size_t l = 0; l < lines.size(); l++)
    {
        const auto& t = lines[l];
        if(t[0] == "kind")
        {
            continue;
        }
        Rule r;
        r.line = lineNumbers[l];
        const std::size_t slash = t[0].rfind('/');
        r.pattern = slash == std::string::npos ? t[0] : t[0].substr(slash + 1);
        r.map = slash == std::string::npos ? std::string() : t[0].substr(0, slash);
        for(std::size_t i = 1; i < t.size(); i++)
        {
            std::string v;
            if(value(t[i], "kind", v))
            {
                r.kind = v == "none" ? kindNone : v == "auto" ? kindAuto : findKind(v);
                if(r.kind == kindUnset)
                {
                    Con_Warning("%s:%d: unknown kind %s\n", cfgPath, r.line, v.c_str());
                }
            }
            else if(value(t[i], "scale", v))
            {
                r.scale = std::clamp(static_cast<float>(std::atof(v.c_str())), 0.01f, 64.f);
            }
            else if(value(t[i], "strength", v))
            {
                r.strength = std::clamp(static_cast<float>(std::atof(v.c_str())), 0.f, 8.f);
            }
            else if(value(t[i], "grain", v))
            {
                r.grain = v == "s" ? GrainS : v == "t" ? GrainT : GrainAuto;
            }
            else
            {
                Con_Warning("%s:%d: unknown setting %s\n", cfgPath, r.line, t[i].c_str());
            }
        }
        rules.push_back(std::move(r));
    }
}

// ---- the texture array -------------------------------------------------------------------------------------------

void deleteArray()
{
    if(array)
    {
        GL_DeleteNativeTexture(array);
        array = 0;
    }
    arrayTried = false;
    arraySize = 0;
}

// The kinds' images (their red), all the same power-of-two size, in one mipmapped array: a missing one is flat.
void buildArray()
{
    arrayTried = true;
    if(kinds.empty())
    {
        return;
    }
    std::vector<std::uint8_t> pixels;
    int size = 0, found = 0;
    for(std::size_t i = 0; i < kinds.size(); i++)
    {
        const int mark = Hunk_LowMark();
        int w = 0, h = 0;
        enum srcformat fmt;
        const byte* data = Image_LoadImage(va("textures/vr/detail_%s", kinds[i].name.c_str()), &w, &h, &fmt);
        if(data && fmt == SRC_RGBA && w == h && w >= 16 && w <= 2048 && (w & (w - 1)) == 0 && (size == 0 || w == size))
        {
            if(size == 0)
            {
                size = w;
                pixels.assign(static_cast<std::size_t>(size) * size * kinds.size(), 128);
            }
            std::uint8_t* layer = pixels.data() + static_cast<std::size_t>(size) * size * i;
            for(int p = 0; p < size * size; p++)
            {
                layer[p] = data[p * 4];
            }
            found++;
        }
        else
        {
            Con_Warning("VR: detail texture textures/vr/detail_%s %s\n", kinds[i].name.c_str(),
                data ? "is not square, a power of two, or as big as the first" : "is missing");
        }
        Hunk_FreeToLowMark(mark);
    }
    if(!found)
    {
        return;
    }
    if(pixels.size() < static_cast<std::size_t>(size) * size * kinds.size())
    {
        pixels.resize(static_cast<std::size_t>(size) * size * kinds.size(), 128); // kinds before the first image found
    }
    glGenTextures(1, &array);
    GL_BindNative(unit, GL_TEXTURE_2D_ARRAY, array);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GL_TexImage3DFunc(GL_TEXTURE_2D_ARRAY, 0, GL_R8, size, size, static_cast<GLsizei>(kinds.size()), 0, GL_RED,
        GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    GL_GenerateMipmapFunc(GL_TEXTURE_2D_ARRAY);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if(gl_max_anisotropy > 1.f)
    {
        glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(gl_max_anisotropy, 8.f));
    }
    arraySize = size;
    Con_DPrintf("VR: detail textures: %d kinds, %d x %d\n", static_cast<int>(kinds.size()), size, size);
}

// ---- which detail a texture gets -----------------------------------------------------------------------------------

struct Stats
{
    bool ok{false};
    float lum{0.f}, sat{0.f}, hue{0.f}, gx{0.f}, gy{0.f}, fullbright{0.f};
};

// Its 8-bit pixels (after the texture_t, as Mod_LoadTextures keeps them): mean brightness, saturation, the hue of the
// mean colour, how much the brightness changes along s and t, and the share of fullbright pixels.
Stats analyse(const texture_t* t)
{
    Stats s;
    if(t == r_notexture_mip || t == r_notexture_mip2 || t->shift != 0 || t->width < 2 || t->height < 2 ||
        t->width > 4096 || t->height > 4096)
    {
        return s;
    }
    const byte* px = reinterpret_cast<const byte*>(t + 1);
    const int w = static_cast<int>(t->width), h = static_cast<int>(t->height);
    std::vector<float> lum(static_cast<std::size_t>(w) * h);
    double r = 0, g = 0, b = 0, sat = 0, l = 0;
    int bright = 0;
    for(int i = 0; i < w * h; i++)
    {
        const unsigned c = d_8to24table[px[i]];
        const float cr = (c & 255) / 255.f, cg = ((c >> 8) & 255) / 255.f, cb = ((c >> 16) & 255) / 255.f;
        const float mx = std::max({cr, cg, cb}), mn = std::min({cr, cg, cb});
        lum[i] = 0.3f * cr + 0.59f * cg + 0.11f * cb;
        l += lum[i];
        sat += mx > 0.f ? (mx - mn) / mx : 0.f;
        r += cr;
        g += cg;
        b += cb;
        bright += px[i] >= 224 && px[i] != 255;
    }
    const double n = static_cast<double>(w) * h;
    s.lum = static_cast<float>(l / n);
    s.sat = static_cast<float>(sat / n);
    s.fullbright = static_cast<float>(bright / n);
    r /= n;
    g /= n;
    b /= n;
    const double mx = std::max({r, g, b}), mn = std::min({r, g, b});
    if(mx > mn)
    {
        double hue = mx == r ? (g - b) / (mx - mn) : mx == g ? 2.0 + (b - r) / (mx - mn) : 4.0 + (r - g) / (mx - mn);
        hue *= 60.0;
        s.hue = static_cast<float>(hue < 0 ? hue + 360.0 : hue);
    }
    double gx = 0, gy = 0;
    for(int y = 0; y < h; y++)
    {
        for(int x = 0; x < w; x++)
        {
            const float v = lum[static_cast<std::size_t>(y) * w + x];
            gx += std::abs(lum[static_cast<std::size_t>(y) * w + (x + 1) % w] - v);
            gy += std::abs(lum[static_cast<std::size_t>((y + 1) % h) * w + x] - v);
        }
    }
    s.gx = static_cast<float>(gx / n);
    s.gy = static_cast<float>(gy / n);
    s.ok = true;
    return s;
}

// A texture no rule names, by its colours (Quake's palette is mostly brown, so this is a rough guess; detail.cfg names
// id's, hipnotic's and rogue's textures): mostly glowing, none; grey: metal if bluish or smooth, stone if busy; green
// or strongly red: organic (slime, moss, flesh); a strong grain one way: wood; very dark: grime; else stone.
int classify(const Stats& s)
{
    const int stone = findKind("stone"), metal = findKind("metal"), wood = findKind("wood"), dirt = findKind("dirt"),
              organic = findKind("organic"), plaster = findKind("plaster");
    const auto pick = [&](int k) { return k >= 0 ? k : plaster >= 0 ? plaster : kinds.empty() ? kindNone : 0; };
    if(!s.ok)
    {
        return pick(plaster);
    }
    if(s.fullbright > 0.3f)
    {
        return kindNone;
    }
    const float busy = 0.5f * (s.gx + s.gy);
    const float aniso = std::max(s.gx, s.gy) / std::max(std::min(s.gx, s.gy), 1e-4f);
    if(s.sat < 0.25f)
    {
        return pick((s.hue >= 180.f && s.hue <= 290.f && s.sat > 0.1f) || busy < 0.025f ? metal : stone);
    }
    if((s.hue >= 60.f && s.hue <= 170.f && s.sat > 0.35f) || ((s.hue < 12.f || s.hue > 330.f) && s.sat > 0.5f))
    {
        return pick(organic);
    }
    if(aniso > 1.8f && s.hue >= 15.f && s.hue <= 50.f)
    {
        return pick(wood);
    }
    if(s.lum < 0.08f)
    {
        return pick(dirt);
    }
    return pick(stone);
}

std::string mapName()
{
    char base[MAX_QPATH] = {};
    if(cl.worldmodel)
    {
        COM_FileBase(cl.worldmodel->name, base, sizeof(base));
    }
    return lower(base);
}

Entry resolve(const texture_t* t)
{
    Entry e;
    q_strlcpy(e.name, t->name, sizeof(e.name));
    if(t->type == TEXTYPE_SKY || TEXTYPE_ISLIQUID(t->type) || kinds.empty())
    {
        return e;
    }
    const std::string name = lower(t->name);
    // an animated texture's +0 / +a prefix: matched with and without
    const std::string bare = name.size() > 2 && name[0] == '+' ? name.substr(2) : name;
    const std::string map = mapName();
    int kind = kindUnset, grain = GrainUnset;
    float scale = 1.f, strength = 1.f;
    for(const Rule& r : rules)
    {
        if((!r.map.empty() && !match(r.map.c_str(), map.c_str())) ||
            (!match(r.pattern.c_str(), name.c_str()) && !match(r.pattern.c_str(), bare.c_str())))
        {
            continue;
        }
        kind = r.kind != kindUnset ? r.kind : kind;
        scale = r.scale >= 0.f ? r.scale : scale;
        strength = r.strength >= 0.f ? r.strength : strength;
        grain = r.grain != GrainUnset ? r.grain : grain;
    }
    e.byRule = kind != kindUnset && kind != kindAuto;
    Stats stats;
    if(!e.byRule || grain == GrainAuto || grain == GrainUnset)
    {
        stats = analyse(t);
    }
    if(!e.byRule)
    {
        kind = classify(stats);
    }
    e.kind = kind;
    if(kind < 0 || kind >= static_cast<int>(kinds.size()))
    {
        return e;
    }
    const Kind& k = kinds[kind];
    // A grain (wood, brushed metal) runs along the image's x: turned to run along t where the texture's own brightness
    // changes much more across s than along t (id's planks are mostly upright).
    bool swap = grain == GrainT;
    if(k.grain && (grain == GrainUnset || grain == GrainAuto) && stats.ok)
    {
        swap = stats.gx > 1.15f * stats.gy;
    }
    const float size = std::max(k.size * scale, 1.f);
    e.swapped = swap;
    e.v[0] = (swap ? -1.f : 1.f) * static_cast<float>(t->width) / size;
    e.v[1] = static_cast<float>(t->height) / size;
    e.v[2] = k.strength * strength;
    e.v[3] = static_cast<float>(kind);
    if(e.v[2] <= 0.f)
    {
        e.v[0] = e.v[1] = e.v[2] = e.v[3] = 0.f;
    }
    return e;
}

// A new map: the cfg read again (it may have been edited) and every texture looked at anew.
void checkWorld()
{
    if(cacheWorld == cl.worldmodel && (!cl.worldmodel || !std::strcmp(cacheWorldName, cl.worldmodel->name)))
    {
        return;
    }
    cacheWorld = cl.worldmodel;
    q_strlcpy(cacheWorldName, cl.worldmodel ? cl.worldmodel->name : "", sizeof(cacheWorldName));
    cache.clear();
    const std::size_t before = kinds.size();
    std::vector<std::string> names;
    for(const Kind& k : kinds)
    {
        names.push_back(k.name);
    }
    loadCfg();
    bool same = kinds.size() == before;
    for(std::size_t i = 0; same && i < kinds.size(); i++)
    {
        same = kinds[i].name == names[i];
    }
    if(!same)
    {
        deleteArray(); // the layers changed
    }
}

const Entry& lookup(const texture_t* t)
{
    auto it = cache.find(t);
    if(it == cache.end() || std::strncmp(it->second.name, t->name, sizeof(it->second.name)) != 0)
    {
        it = cache.insert_or_assign(t, resolve(t)).first;
    }
    return it->second;
}

// vr_detail_reload: detail.cfg and the images read again.
void reload_f()
{
    deleteArray();
    cfgLoaded = false;
    loadCfg();
    cache.clear();
    Con_Printf("detail textures: %d kinds, %d rules\n", static_cast<int>(kinds.size()), static_cast<int>(rules.size()));
}

// vr_detail_list [pattern]: the map's textures, the detail each gets and why.
void list_f()
{
    if(!cl.worldmodel)
    {
        Con_Printf("no map\n");
        return;
    }
    checkWorld();
    const std::string pattern = Cmd_Argc() > 1 ? lower(Cmd_Argv(1)) : std::string("*");
    for(int i = 0; i < cl.worldmodel->numtextures; i++)
    {
        const texture_t* t = cl.worldmodel->textures[i];
        if(!t || t == r_notexture_mip || t == r_notexture_mip2 || !match(pattern.c_str(), lower(t->name).c_str()))
        {
            continue;
        }
        const Entry& e = lookup(t);
        if(e.kind < 0 || e.v[2] <= 0.f)
        {
            Con_Printf("%-16s none%s\n", t->name, e.byRule || e.kind == kindNone ? "" : " (auto)");
            continue;
        }
        const Stats s = analyse(t);
        Con_Printf("%-16s %-8s %s tile %.0f strength %.2f%s   (lum %.2f sat %.2f hue %.0f grain %.2f)\n", t->name,
            kinds[e.kind].name.c_str(), e.byRule ? "cfg " : "auto", t->width / std::abs(e.v[0]), e.v[2],
            e.swapped ? " along t" : "", s.lum, s.sat, s.hue, s.gy > 0.f ? s.gx / s.gy : 0.f);
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_detail_reload", reload_f);
    Cmd_AddCommand("vr_detail_list", list_f);
}

} // namespace qvr::detail

using namespace qvr;

// R_SetupView: this view's settings for the world shader (Detail in the frame data) and the array on unit 12.
extern "C" void VR_DetailView(void)
{
    if(!detail::cfgLoaded)
    {
        detail::loadCfg();
    }
    detail::checkWorld();
    const bool on = vr_detail.value != 0.f;
    if(on && !detail::arrayTried)
    {
        detail::buildArray();
    }
    const float end = std::clamp(vr_detail_distance.value, 16.f, 1024.f);
    r_framedata.detail[0] = on && detail::array ? std::clamp(vr_detail_strength.value, 0.f, 4.f) : 0.f;
    r_framedata.detail[1] = end / 3.f; // full within a third of it
    r_framedata.detail[2] = end;
    r_framedata.detail[3] = vr_detail_fine.value != 0.f ? detail::fineScale : 0.f;
    GL_BindNative(detail::unit, GL_TEXTURE_2D_ARRAY, detail::array);
}

// R_AddBModelCall: a texture's detail for the world shader (Call.detail): s and t scales (s < 0: the axes swapped),
// the strength, the layer; zero for none.
extern "C" void VR_DetailCall(const texture_t* t, float out[4])
{
    out[0] = out[1] = out[2] = out[3] = 0.f;
    if(!t || r_framedata.detail[0] <= 0.f)
    {
        return;
    }
    const detail::Entry& e = detail::lookup(t);
    std::memcpy(out, e.v, sizeof(e.v));
}
