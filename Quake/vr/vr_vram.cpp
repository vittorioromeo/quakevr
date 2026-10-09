// vr_vram.cpp -- see vr_vram.hpp. The scan: every name GL hands out, from 1 until 4096 in a row are not objects (as
// vr_memstats counts them), described with GL 4.5's direct-state queries (no binding: nothing of the frame's state
// touched); its label (glObjectLabel: the engine's textures' names, the VR module's targets') says what it is.

#include "vr_vram.hpp"
#include "vr_engine.hpp"
#include "vr_gpustats.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

namespace qvr::vram
{
namespace
{

// ---- GL's descriptions (4.5, looked up at the first report) ----------------------------------------

using GetTextureParameterivFn = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GetTextureLevelParameterivFn = void(APIENTRY*)(GLuint, GLint, GLenum, GLint*);
using GetNamedRenderbufferParameterivFn = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GetNamedBufferParameteri64vFn = void(APIENTRY*)(GLuint, GLenum, GLint64*);
using GetObjectLabelFn = void(APIENTRY*)(GLenum, GLuint, GLsizei, GLsizei*, GLchar*);
using IsFn = GLboolean(APIENTRY*)(GLuint);

struct GlFns
{
    bool looked{false};
    GetTextureParameterivFn textureParameter{nullptr};
    GetTextureLevelParameterivFn textureLevelParameter{nullptr};
    GetNamedRenderbufferParameterivFn renderbufferParameter{nullptr};
    GetNamedBufferParameteri64vFn bufferParameter{nullptr};
    GetObjectLabelFn objectLabel{nullptr};
    IsFn isRenderbuffer{nullptr};
    IsFn isBuffer{nullptr};
};
GlFns gl;

void lookUp()
{
    if(gl.looked)
    {
        return;
    }
    gl.looked = true;
    const auto get = [](const char* name) { return SDL_GL_GetProcAddress(name); };
    gl.textureParameter = reinterpret_cast<GetTextureParameterivFn>(get("glGetTextureParameteriv"));
    gl.textureLevelParameter = reinterpret_cast<GetTextureLevelParameterivFn>(get("glGetTextureLevelParameteriv"));
    gl.renderbufferParameter = reinterpret_cast<GetNamedRenderbufferParameterivFn>(get("glGetNamedRenderbufferParameteriv"));
    gl.bufferParameter = reinterpret_cast<GetNamedBufferParameteri64vFn>(get("glGetNamedBufferParameteri64v"));
    gl.objectLabel = reinterpret_cast<GetObjectLabelFn>(get("glGetObjectLabel"));
    gl.isRenderbuffer = reinterpret_cast<IsFn>(get("glIsRenderbuffer"));
    gl.isBuffer = reinterpret_cast<IsFn>(get("glIsBuffer"));
}

constexpr GLenum labelTexture = 0x1702;      // GL_TEXTURE
constexpr GLenum labelRenderbuffer = 0x8D41; // GL_RENDERBUFFER
constexpr GLenum labelBuffer = 0x82E0;       // GL_BUFFER

enum class Kind : unsigned char
{
    Texture,
    Renderbuffer,
    Buffer,
};
constexpr const char* kindNames[] = {"texture", "renderbuffer", "buffer"};

// One object, as GL describes it.
struct Object
{
    Kind kind{Kind::Texture};
    GLuint name{0};
    za::String label;
    int width{0}, height{0}, depth{0}; // level 0's (depth: layers, a cube's 6)
    int levels{0}, samples{0};
    GLint format{0};
    double bytes{0.0};
};

[[nodiscard]] za::String labelOf(GLenum identifier, GLuint name)
{
    char buf[160] = "";
    GLsizei length = 0;
    if(gl.objectLabel)
    {
        gl.objectLabel(identifier, name, sizeof(buf), &length, buf);
    }
    buf[za::clamp(static_cast<int>(length), 0, static_cast<int>(sizeof(buf)) - 1)] = '\0';
    return za::String{buf};
}

// The bytes a texel takes as the GPU stores it (its channels' bits; 24 and 48-bit formats padded as drivers do).
[[nodiscard]] int texelBytes(int bits)
{
    if(bits == 24)
    {
        return 4;
    }
    if(bits == 48)
    {
        return 8;
    }
    if(bits == 96)
    {
        return 16;
    }
    return (bits + 7) / 8;
}

void clearErrors()
{
    while(glGetError() != GL_NO_ERROR)
    {
    }
}

[[nodiscard]] bool describeTexture(GLuint name, Object& o)
{
    if(!gl.textureParameter || !gl.textureLevelParameter)
    {
        return false;
    }
    o.kind = Kind::Texture;
    o.name = name;
    clearErrors();
    GLint target = 0, immutable = 0, immutableLevels = 0;
    gl.textureParameter(name, GL_TEXTURE_TARGET, &target);
    if(glGetError() != GL_NO_ERROR || target == 0)
    {
        return false; // a name never bound: no storage
    }
    if(target == GL_TEXTURE_BUFFER)
    {
        return false; // its storage is a buffer's, counted with the buffers
    }
    gl.textureParameter(name, GL_TEXTURE_IMMUTABLE_FORMAT, &immutable);
    if(immutable)
    {
        gl.textureParameter(name, GL_TEXTURE_IMMUTABLE_LEVELS, &immutableLevels);
    }
    const int faces = target == GL_TEXTURE_CUBE_MAP ? 6 : 1;
    const int maxLevels = immutable ? za::max(immutableLevels, 1) : 16;
    for(int level = 0; level < maxLevels; level++)
    {
        GLint w = 0, h = 0, d = 0, samples = 0, compressed = 0, format = 0;
        gl.textureLevelParameter(name, level, GL_TEXTURE_WIDTH, &w);
        if(w <= 0)
        {
            break;
        }
        gl.textureLevelParameter(name, level, GL_TEXTURE_HEIGHT, &h);
        gl.textureLevelParameter(name, level, GL_TEXTURE_DEPTH, &d);
        gl.textureLevelParameter(name, level, GL_TEXTURE_SAMPLES, &samples);
        gl.textureLevelParameter(name, level, GL_TEXTURE_COMPRESSED, &compressed);
        gl.textureLevelParameter(name, level, GL_TEXTURE_INTERNAL_FORMAT, &format);
        double bytes = 0.0;
        if(compressed)
        {
            GLint size = 0;
            gl.textureLevelParameter(name, level, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &size);
            bytes = static_cast<double>(size);
        }
        else
        {
            int bits = 0;
            constexpr GLenum sizes[] = {GL_TEXTURE_RED_SIZE, GL_TEXTURE_GREEN_SIZE, GL_TEXTURE_BLUE_SIZE, GL_TEXTURE_ALPHA_SIZE,
                GL_TEXTURE_DEPTH_SIZE, GL_TEXTURE_STENCIL_SIZE, 0x8C3F /* GL_TEXTURE_SHARED_SIZE */};
            for(const GLenum p : sizes)
            {
                GLint b = 0;
                gl.textureLevelParameter(name, level, p, &b);
                bits += b;
            }
            bytes = static_cast<double>(w) * za::max(h, 1) * za::max(d, 1) * texelBytes(bits) * za::max(samples, 1);
        }
        o.bytes += bytes * faces;
        if(level == 0)
        {
            o.width = w;
            o.height = h;
            o.depth = target == GL_TEXTURE_CUBE_MAP ? 6 : d;
            o.samples = samples;
            o.format = format;
        }
        o.levels = level + 1;
    }
    clearErrors();
    o.label = labelOf(labelTexture, name);
    const za::SizeT n = o.label.size();
    if(n >= 5 && strcmp(o.label.cStr() + n - 5, " view") == 0)
    {
        return false; // a view of another texture's storage (envmap's faces): none of its own
    }
    return o.levels > 0;
}

[[nodiscard]] bool describeRenderbuffer(GLuint name, Object& o)
{
    if(!gl.renderbufferParameter)
    {
        return false;
    }
    o.kind = Kind::Renderbuffer;
    o.name = name;
    GLint w = 0, h = 0, samples = 0, format = 0;
    gl.renderbufferParameter(name, 0x8D42, &w); // GL_RENDERBUFFER_WIDTH
    gl.renderbufferParameter(name, 0x8D43, &h); // _HEIGHT
    gl.renderbufferParameter(name, 0x8CAB, &samples);
    gl.renderbufferParameter(name, 0x8D44, &format);
    int bits = 0;
    for(GLenum p = 0x8D50; p <= 0x8D55; p++) // _RED_SIZE .. _STENCIL_SIZE
    {
        GLint b = 0;
        gl.renderbufferParameter(name, p, &b);
        bits += b;
    }
    clearErrors();
    o.width = w;
    o.height = h;
    o.depth = 1;
    o.levels = 1;
    o.samples = samples;
    o.format = format;
    o.bytes = static_cast<double>(w) * h * texelBytes(bits) * za::max(samples, 1);
    o.label = labelOf(labelRenderbuffer, name);
    return w > 0;
}

[[nodiscard]] bool describeBuffer(GLuint name, Object& o)
{
    if(!gl.bufferParameter)
    {
        return false;
    }
    o.kind = Kind::Buffer;
    o.name = name;
    GLint64 size = 0;
    gl.bufferParameter(name, GL_BUFFER_SIZE, &size);
    clearErrors();
    o.bytes = static_cast<double>(size);
    o.label = labelOf(labelBuffer, name);
    return size > 0;
}

// ---- What each object is ----------------------------------------------------------------------------

// The group: its label, digits as '#' ("lightmap0003" and "lightmap0004" one group), a model's textures by model
// ("progs/ogre.mdl:frame0" -> "progs/ogre.mdl"); no label: its kind, size and format.
[[nodiscard]] za::String groupOf(const Object& o)
{
    if(o.label.empty())
    {
        char buf[96];
        snprintf(buf, sizeof(buf), "(no label) %s %dx%dx%d fmt 0x%X%s", kindNames[static_cast<int>(o.kind)], o.width, o.height,
            o.depth, static_cast<unsigned>(o.format), o.samples > 1 ? " msaa" : "");
        return za::String{buf};
    }
    za::String g;
    const char* s = o.label.cStr();
    const char* colon = strchr(s, ':');
    const za::SizeT n = colon && (strstr(s, ".mdl") || strstr(s, ".bsp") || strstr(s, ".spr")) ? static_cast<za::SizeT>(colon - s)
                                                                                                : o.label.size();
    for(za::SizeT i = 0; i < n; i++)
    {
        const char c = s[i];
        if(c >= '0' && c <= '9')
        {
            if(g.empty() || g[g.size() - 1] != '#')
            {
                g += '#';
            }
        }
        else
        {
            g += c;
        }
    }
    return g;
}

// The categories, the first whose words the label holds (lower case); the order matters.
struct Rule
{
    const char* category;
    const char* words[12];
};
constexpr Rule rules[] = {
    {"xr swapchain images (runtime's)", {"xr swapchain", nullptr}},
    {"eye scene targets (colour, depth, msaa, oit)", {"scene", "oit ", "composite", "resolve", "stereo", nullptr}},
    {"shadow atlases", {"shadow", nullptr}},
    {"post targets: bloom, tonemap, upscale, foveation", {"bloom", "tonemap", "upscale", "fovea", "sharpen", "resample", nullptr}},
    {"effect targets: haze, water, particles, trails", {"haze", "water", "particle under", "half-res", "trail", "liquid", nullptr}},
    {"envmaps, light clusters", {"envmap", "cube", "light cluster", "lighting", nullptr}},
    {"gates/portals", {"portal", "gate", nullptr}},
    {"text3d screens, panels, menu, gadgets", {"text", "screen", "canvas", "menu", "hud", "board", "gadget", "spectator", nullptr}},
    {"wounds, blood, gore", {"wound", "blood", "gore", "wash", "spatter", nullptr}},
    {"decals, particle atlases, detail", {"decal", "particle atlas", "detail", nullptr}},
    {"generated normal maps (vrnorm)", {"_norm", "vrnorm", nullptr}},
    {"lightmaps", {"lightmap", "luxmap", nullptr}},
    {"world textures (bsp)", {".bsp", nullptr}},
    {"model skins (mdl, HD replacements, limbs)", {".mdl", "progs/", nullptr}},
    {"sprites", {".spr", nullptr}},
    {"sky", {"sky", nullptr}},
    {"2D: console, font, pics, palette", {"conchars", "gfx/", "wad", "conback", "char", "scrap", "palette", nullptr}},
    {"buffers: vertices, uniforms, uploads", {"buffer", "vbo", "ubo", "ssbo", "upload", "frame", "vert", "indices", nullptr}},
};

// Render targets: sized by the eyes' resolution (the settings'), not by what the map loads.
[[nodiscard]] bool isTargetCategory(const char* category)
{
    return strstr(category, "target") || strstr(category, "swapchain") || strstr(category, "post:") || strstr(category, "shadows")
        || strstr(category, "gates");
}

[[nodiscard]] const char* categoryOf(const Object& o)
{
    if(o.label.empty())
    {
        return o.kind == Kind::Buffer ? "unlabelled buffers" : "unlabelled textures/targets";
    }
    char lower[160];
    za::SizeT n = 0;
    for(; n < o.label.size() && n + 1 < sizeof(lower); n++)
    {
        const char c = o.label[n];
        lower[n] = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    }
    lower[n] = '\0';
    for(const Rule& r : rules)
    {
        for(const char* const* w = r.words; *w; w++)
        {
            if(strstr(lower, *w))
            {
                return r.category;
            }
        }
    }
    return o.kind == Kind::Buffer ? "other buffers" : "other textures";
}

// ---- The scan, and what the last one found -------------------------------------------------------------

struct Last
{
    bool scanned{false};
    ankerl::unordered_dense::map<za::U64, za::String> groups; // (kind << 32 | name) -> group, with its bytes below
    ankerl::unordered_dense::map<za::U64, double> bytes;
    Totals totals;
};
Last last;

[[nodiscard]] za::U64 keyOf(const Object& o)
{
    return static_cast<za::U64>(o.kind) << 32 | o.name;
}

template <class Describe>
void scanKind(IsFn isObject, Describe describe, za::Vector<Object>& out)
{
    if(!isObject)
    {
        return;
    }
    for(GLuint name = 1, misses = 0; misses < 4096 && name < (1u << 22); name++)
    {
        if(!isObject(name))
        {
            misses++;
            continue;
        }
        misses = 0;
        Object o;
        if(describe(name, o))
        {
            out.pushBack(ZA_MOVE(o));
        }
    }
}

[[nodiscard]] double mb(double bytes)
{
    return bytes / 1048576.0;
}

struct Sum
{
    double bytes{0.0};
    int count{0};
};

void writeCsv(const za::Vector<Object>& objects)
{
    const time_t now = time(nullptr);
    char stamp[64];
    strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", localtime(&now));
    const za::String dir = za::String{com_gamedir} + "/profile";
    Sys_mkdir(dir.cStr());
    const za::String path = dir + "/vram_" + stamp + ".csv";
    FILE* f = fopen(path.cStr(), "w");
    if(!f)
    {
        Con_Printf("vr_vram_report: cannot write %s\n", path.cStr());
        return;
    }
    fprintf(f, "kind,name,category,group,label,width,height,depth,levels,samples,format,kb\n");
    for(const Object& o : objects)
    {
        za::String label = o.label;
        for(char& c : label)
        {
            if(c == ',' || c == '"')
            {
                c = ';';
            }
        }
        za::String group = groupOf(o);
        for(char& c : group)
        {
            if(c == ',')
            {
                c = ';';
            }
        }
        fprintf(f, "%s,%u,%s,%s,%s,%d,%d,%d,%d,%d,0x%X,%.1f\n", kindNames[static_cast<int>(o.kind)], o.name, categoryOf(o),
            group.cStr(), label.cStr(), o.width, o.height, o.depth, o.levels, o.samples, static_cast<unsigned>(o.format),
            o.bytes / 1024.0);
    }
    fclose(f);
    Con_Printf("vr_vram_report: %d objects written to %s\n", static_cast<int>(objects.size()), path.cStr());
}

} // namespace

Totals lastTotals()
{
    return last.totals;
}

void report_f()
{
    bool diff = false, csv = false, all = false;
    for(int i = 1; i < Cmd_Argc(); i++)
    {
        const char* a = Cmd_Argv(i);
        diff |= !q_strcasecmp(a, "diff");
        csv |= !q_strcasecmp(a, "csv");
        all |= !q_strcasecmp(a, "all");
    }
    lookUp();
    if(!gl.textureLevelParameter || !gl.textureParameter)
    {
        Con_Printf("vr_vram_report: needs OpenGL 4.5's direct-state queries (glGetTextureLevelParameteriv)\n");
        return;
    }

    const double start = Sys_DoubleTime();
    za::Vector<Object> objects;
    scanKind(glIsTexture, describeTexture, objects);
    scanKind(gl.isRenderbuffer, describeRenderbuffer, objects);
    scanKind(gl.isBuffer, describeBuffer, objects);
    const double scanMs = (Sys_DoubleTime() - start) * 1000.0;

    // Categories, groups.
    ankerl::unordered_dense::map<za::String, Sum> byCategory, byGroup;
    Sum textures, renderbuffers, buffers, total, targets;
    for(const Object& o : objects)
    {
        const char* cat = categoryOf(o);
        Sum& c = byCategory[za::String{cat}];
        c.bytes += o.bytes;
        c.count++;
        Sum& g = byGroup[groupOf(o)];
        g.bytes += o.bytes;
        g.count++;
        Sum& k = o.kind == Kind::Texture ? textures : o.kind == Kind::Renderbuffer ? renderbuffers : buffers;
        k.bytes += o.bytes;
        k.count++;
        total.bytes += o.bytes;
        total.count++;
        if(o.kind == Kind::Renderbuffer || isTargetCategory(cat))
        {
            targets.bytes += o.bytes;
            targets.count++;
        }
    }

    // Windows' count, ours and the others'.
    const za::Vector<gpustats::ProgramVram> programs = gpustats::programVram();
    double selfMb = -1.0, selfShared = 0.0, othersMb = 0.0;
    for(const gpustats::ProgramVram& p : programs)
    {
        if(p.self)
        {
            selfMb = p.dedicatedMb;
            selfShared = p.sharedMb;
        }
        else
        {
            othersMb += p.dedicatedMb;
        }
    }
    const gpustats::Vram device = gpustats::latestVram();

    Con_Printf("vr_vram_report, map \"%s\" (scan %.0f ms)\n", cl.worldmodel ? cl.worldmodel->name : "", scanMs);
    if(selfMb >= 0.0)
    {
        Con_Printf("  this process (Windows): %.0f MB dedicated, %.0f MB shared; other programs %.0f MB", selfMb, selfShared, othersMb);
        if(device.totalMb > 0)
        {
            Con_Printf("; the GPU: %d of %d MB used", device.totalMb - device.freeMb, device.totalMb);
        }
        Con_Printf("\n");
    }
    else
    {
        Con_Printf("  this process: not counted here (Windows' GPU Process Memory counters)\n");
    }
    Con_Printf("  render targets (sized by the eyes, not the map): %.0f MB in %d\n", mb(targets.bytes), targets.count);
    Con_Printf("  GL objects: %.0f MB = textures %.0f MB (%d), renderbuffers %.0f MB (%d), buffers %.0f MB (%d)\n", mb(total.bytes),
        mb(textures.bytes), textures.count, mb(renderbuffers.bytes), renderbuffers.count, mb(buffers.bytes), buffers.count);
    if(selfMb >= 0.0)
    {
        Con_Printf("  not in GL's objects (driver, shaders, the runtime's own): %.0f MB\n", selfMb - mb(total.bytes));
    }

    const auto sorted = [](const ankerl::unordered_dense::map<za::String, Sum>& m) {
        za::Vector<const ankerl::unordered_dense::map<za::String, Sum>::value_type*> v;
        for(const auto& e : m)
        {
            v.pushBack(&e);
        }
        za::quickSort(v.begin(), v.end(), [](const auto* a, const auto* b) { return a->second.bytes > b->second.bytes; });
        return v;
    };
    Con_Printf("  by category:\n");
    for(const auto* e : sorted(byCategory))
    {
        Con_Printf("    %8.1f MB  %4d  %s\n", mb(e->second.bytes), e->second.count, e->first.cStr());
    }
    const int groupLines = all ? 100000 : 24;
    Con_Printf("  by group (the largest %s):\n", all ? "first" : "24; 'all' for every one");
    int shown = 0;
    for(const auto* e : sorted(byGroup))
    {
        if(shown++ >= groupLines || (!all && e->second.bytes < 1024.0 * 64))
        {
            break;
        }
        Con_Printf("    %8.1f MB  %4d  %s\n", mb(e->second.bytes), e->second.count, e->first.cStr());
    }
    Con_Printf("  the largest objects:\n");
    za::Vector<const Object*> big;
    for(const Object& o : objects)
    {
        big.pushBack(&o);
    }
    za::quickSort(big.begin(), big.end(), [](const Object* a, const Object* b) { return a->bytes > b->bytes; });
    for(za::SizeT i = 0; i < big.size() && i < 12; i++)
    {
        const Object& o = *big[i];
        Con_Printf("    %8.1f MB  %s %u %dx%dx%d %d lv %d smp fmt 0x%X \"%s\"\n", mb(o.bytes), kindNames[static_cast<int>(o.kind)], o.name,
            o.width, o.height, o.depth, o.levels, o.samples, static_cast<unsigned>(o.format), o.label.cStr());
    }
    if(!programs.empty())
    {
        Con_Printf("  programs on the GPU (dedicated MB):");
        int listed = 0;
        for(const gpustats::ProgramVram& p : programs)
        {
            if(listed++ >= 10 || p.dedicatedMb < 20.0)
            {
                break;
            }
            Con_Printf(" %s%s %.0f", p.name.cStr(), p.self ? " (us)" : "", p.dedicatedMb);
        }
        Con_Printf("\n");
    }

    // What came and went since the last report.
    if(diff && last.scanned)
    {
        ankerl::unordered_dense::map<za::String, Sum> made, freed;
        ankerl::unordered_dense::map<za::U64, bool> seen;
        for(const Object& o : objects)
        {
            const za::U64 key = keyOf(o);
            seen[key] = true;
            const auto it = last.groups.find(key);
            const za::String group = groupOf(o);
            if(it == last.groups.end() || it->second != group || last.bytes[key] != o.bytes)
            {
                Sum& s = made[group];
                s.bytes += o.bytes;
                s.count++;
                if(it != last.groups.end())
                {
                    Sum& f = freed[it->second];
                    f.bytes += last.bytes[key];
                    f.count++;
                }
            }
        }
        for(const auto& [key, group] : last.groups)
        {
            if(!seen.contains(key))
            {
                Sum& f = freed[group];
                f.bytes += last.bytes[key];
                f.count++;
            }
        }
        Sum madeAll, freedAll;
        for(const auto& [g, s] : made)
        {
            madeAll.bytes += s.bytes;
            madeAll.count += s.count;
        }
        for(const auto& [g, s] : freed)
        {
            freedAll.bytes += s.bytes;
            freedAll.count += s.count;
        }
        Con_Printf("  since the last report: %.1f MB made (%d), %.1f MB freed (%d); by group, the largest net first:\n",
            mb(madeAll.bytes), madeAll.count, mb(freedAll.bytes), freedAll.count);
        ankerl::unordered_dense::map<za::String, Sum> net;
        for(const auto& [g, s] : made)
        {
            net[g].bytes += s.bytes;
            net[g].count += s.count;
        }
        for(const auto& [g, s] : freed)
        {
            net[g].bytes -= s.bytes;
            net[g].count -= s.count;
        }
        za::Vector<const decltype(net)::value_type*> v;
        for(const auto& e : net)
        {
            v.pushBack(&e);
        }
        za::quickSort(v.begin(), v.end(), [](const auto* a, const auto* b) { return fabs(a->second.bytes) > fabs(b->second.bytes); });
        for(za::SizeT i = 0; i < v.size() && i < 16; i++)
        {
            const auto& [g, s] = *v[i];
            const Sum m = made.contains(g) ? made[g] : Sum{};
            const Sum f = freed.contains(g) ? freed[g] : Sum{};
            Con_Printf("    %+8.1f MB (%+d): made %d (%.1f MB), freed %d (%.1f MB)  %s\n", mb(s.bytes), s.count, m.count, mb(m.bytes),
                f.count, mb(f.bytes), g.cStr());
        }
    }

    last.groups.clear();
    last.bytes.clear();
    for(const Object& o : objects)
    {
        last.groups[keyOf(o)] = groupOf(o);
        last.bytes[keyOf(o)] = o.bytes;
    }
    last.scanned = true;
    last.totals.glMb = mb(total.bytes);
    last.totals.texturesMb = mb(textures.bytes);
    last.totals.targetsMb = mb(targets.bytes);
    last.totals.processMb = selfMb;
    last.totals.objects = total.count;
    if(csv)
    {
        writeCsv(objects);
    }
}

} // namespace qvr::vram
