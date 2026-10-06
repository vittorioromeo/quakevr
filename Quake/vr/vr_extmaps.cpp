// vr_extmaps.cpp -- see vr_extmaps.hpp.

#include "vr_modelmetadata.hpp"
#include "vr_extmaps.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "vr_zancle.hpp"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace qvr::extmaps
{
namespace
{

constexpr const char* prefix = "vrext/"; // the folder Image_LoadImage reads vr_extmaps_dir's files by
constexpr int prefixLen = 6;
constexpr int sigSize = 64;    // the detail signatures compared (vr_extmaps_match), a side
constexpr int sigSamples = 4;  // samples a cell, a side (box-filtered from up to 256 texels a side)
constexpr unsigned cfSpecMap = 64u; // the world shader's CF_SPECMAP (gl_shaders.h)

// A .mat's top-level parameters (Quetoo's material file; its stages, for liquids and animations, are skipped).
struct Material
{
    char diffusemap[MAX_QPATH]{};
    char normalmap[MAX_QPATH]{};
    char specularmap[MAX_QPATH]{};
    float specularity{1.f};
    float hardness{1.f};
};

// The texture Mod_LoadTextures prepared last (VR_ExtMapsPrepare), for VR_ExtMapsAttach after its upload.
struct Pending
{
    char texname[16]{};
    char base[MAX_QPATH]{}; // the pack's name for it
    Material mat;
    float match{-2.f}; // -2: not compared (vr_extmaps_match 0)
    bool ok{false};
    char normal[MAX_QPATH]{}; // the normal map's file ("vrext/basebtn+0_norm"): its own, or another frame's (below)
    char normalFrame{0};      // ... another frame's of its animation ('0' for basebtn+1), or 0: its own
    float normalMatch{-2.f};  // ... how well that frame's picture matches this texture (-2: not compared)
    bool normalOnly{false};   // only that normal map (the pack has no picture of this frame: butn+a)
};
Pending pending;

// What each texture of the last map got (vr_extmaps_stats).
struct Record
{
    char texname[16]{};
    float match{-2.f};
    bool missing{false}, skipped{false}, normal{false}, spec{false}, luma{false}, mat{false};
    bool item{false}; // an item box's (maps/b_*.bsp)
    char normalFrame{0};     // its normal map another frame's of its animation (Pending::normalFrame)
    float normalMatch{-2.f}; // ... that frame's picture's match
};
za::Vector<Record> records;
char recordsWorld[MAX_QPATH] = {};

// The pack's name for a Quake texture: lower case, a liquid's '*' dropped, an animation's +<frame> moved to the end.
void packName(const char* tex, char* out, size_t size)
{
    char t[17];
    int i;
    for(i = 0; i < 16 && tex[i]; i++)
    {
        t[i] = static_cast<char>(tolower(static_cast<unsigned char>(tex[i])));
    }
    t[i] = 0;
    const char* s = t[0] == '*' || t[0] == '#' ? t + 1 : t;
    if(s[0] == '+' && s[1] && s[2])
    {
        q_snprintf(out, size, "%s+%c", s + 2, s[1]);
    }
    else
    {
        q_strlcpy(out, s, size);
    }
}

// A .mat's reference to an image ("quake/dem4_4_norm"): its file name in the folder (dem4_4_norm).
const char* fileOf(const char* ref)
{
    const char* slash = strrchr(ref, '/');
    return slash ? slash + 1 : ref;
}

bool isAbsolute(const char* dir)
{
    return dir[0] == '/' || dir[0] == '\\' || (dir[0] && dir[1] == ':');
}

// <name>.mat read into `mat` (defaults if there is none).
void readMat(const char* base, Material& mat)
{
    char path[MAX_QPATH];
    q_snprintf(path, sizeof(path), "%s%s.mat", prefix, base);
    FILE* f = VR_ExtMapsOpen(path);
    if(!f)
    {
        return;
    }
    char text[4096];
    const size_t n = fread(text, 1, za::clamp(static_cast<size_t>(com_filesize), size_t{0}, sizeof(text) - 1), f);
    fclose(f);
    text[n] = 0;

    int depth = 0;
    for(char* line = text; line && *line;)
    {
        char* next = strchr(line, '\n');
        if(next)
        {
            *next++ = 0;
        }
        char key[32] = {}, value[MAX_QPATH] = {};
        for(const char* c = line; *c; c++)
        {
            depth += *c == '{' ? 1 : *c == '}' ? -1 : 0;
        }
        if(depth == 1 && sscanf(line, " %31s %63s", key, value) == 2)
        {
            if(!strcmp(key, "diffusemap"))
            {
                q_strlcpy(mat.diffusemap, value, sizeof(mat.diffusemap));
            }
            else if(!strcmp(key, "normalmap"))
            {
                q_strlcpy(mat.normalmap, value, sizeof(mat.normalmap));
            }
            else if(!strcmp(key, "specularmap"))
            {
                q_strlcpy(mat.specularmap, value, sizeof(mat.specularmap));
            }
            else if(!strcmp(key, "specularity"))
            {
                mat.specularity = static_cast<float>(atof(value));
            }
            else if(!strcmp(key, "hardness"))
            {
                mat.hardness = static_cast<float>(atof(value));
            }
        }
        line = next;
    }
}

// Whether the pack has an image `name` ("vrext/<file>", no extension: Image_LoadImage's formats).
bool imageExists(const char* name)
{
    static constexpr const char* exts[] = {"png", "tga", "jpg"};
    for(const char* ext : exts)
    {
        char path[MAX_QPATH];
        q_snprintf(path, sizeof(path), "%s.%s", name, ext);
        if(FILE* f = VR_ExtMapsOpen(path))
        {
            fclose(f);
            return true;
        }
    }
    return false;
}

// The pack's normal map for `base` (its .mat's, else <base>_norm) into `file` ("vrext/..."): whether it has it.
bool normalFile(const char* base, const Material& mat, char* file, size_t size)
{
    if(mat.normalmap[0])
    {
        q_snprintf(file, size, "%s%s", prefix, fileOf(mat.normalmap));
    }
    else
    {
        q_snprintf(file, size, "%s%s_norm", prefix, base);
    }
    return imageExists(file);
}

// A picture's detail: its luminance at sigSize x sigSize (box-filtered), less each cell's 5 x 5 neighbourhood's
// (wrapping round: textures tile), into `out`; `tmp` as large.
void signature(const byte* data, enum srcformat fmt, int w, int h, float* out, float* tmp)
{
    constexpr int side = sigSize * sigSamples;
    for(int cy = 0; cy < sigSize; cy++)
    {
        for(int cx = 0; cx < sigSize; cx++)
        {
            float sum = 0.f;
            for(int sy = 0; sy < sigSamples; sy++)
            {
                const int y = (cy * sigSamples + sy) * h / side;
                for(int sx = 0; sx < sigSamples; sx++)
                {
                    const int x = (cx * sigSamples + sx) * w / side;
                    const byte* p;
                    if(fmt == SRC_INDEXED)
                    {
                        p = reinterpret_cast<const byte*>(&d_8to24table[data[y * w + x]]);
                    }
                    else
                    {
                        p = data + (static_cast<size_t>(y) * w + x) * 4;
                    }
                    sum += 0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2];
                }
            }
            tmp[cy * sigSize + cx] = sum / (sigSamples * sigSamples);
        }
    }
    for(int y = 0; y < sigSize; y++)
    {
        for(int x = 0; x < sigSize; x++)
        {
            float blur = 0.f;
            for(int dy = -2; dy <= 2; dy++)
            {
                for(int dx = -2; dx <= 2; dx++)
                {
                    blur += tmp[((y + dy) & (sigSize - 1)) * sigSize + ((x + dx) & (sigSize - 1))];
                }
            }
            out[y * sigSize + x] = tmp[y * sigSize + x] - blur / 25.f;
        }
    }
}

float correlation(const float* a, const float* b)
{
    constexpr int n = sigSize * sigSize;
    double ma = 0, mb = 0;
    for(int i = 0; i < n; i++)
    {
        ma += a[i];
        mb += b[i];
    }
    ma /= n;
    mb /= n;
    double ab = 0, aa = 0, bb = 0;
    for(int i = 0; i < n; i++)
    {
        ab += (a[i] - ma) * (b[i] - mb);
        aa += (a[i] - ma) * (a[i] - ma);
        bb += (b[i] - mb) * (b[i] - mb);
    }
    return aa > 0 && bb > 0 ? static_cast<float>(ab / sqrt(aa * bb)) : 0.f;
}

Record& record(const qmodel_t* mod, const char* texname)
{
    // A new map (not one of its item boxes, maps/b_*.bsp): the list starts again.
    const char* model = mod ? mod->name : "";
    const bool item = qvr::modelmeta::has(mod, qvr::modelmeta::Trait::AmmoBox);
    if(!item && strcmp(model, recordsWorld) != 0)
    {
        records.clear();
        q_strlcpy(recordsWorld, model, sizeof(recordsWorld));
    }
    for(Record& r : records)
    {
        if(r.item == item && !strcmp(r.texname, texname))
        {
            return r;
        }
    }
    records.emplaceBack();
    q_strlcpy(records.back().texname, texname, sizeof(records.back().texname));
    records.back().item = item;
    return records.back();
}

// vr_extmaps_stats [all]: what the last map's textures got (its item boxes' apart).
void stats_f()
{
    int n = 0, missing = 0, skipped = 0, normal = 0, spec = 0, luma = 0, mat = 0, items = 0, itemsUsed = 0;
    const bool all = Cmd_Argc() > 1;
    for(const Record& r : records)
    {
        if(r.item)
        {
            items++;
            itemsUsed += !r.missing && !r.skipped;
            continue;
        }
        n++;
        missing += r.missing;
        skipped += r.skipped;
        normal += r.normal;
        spec += r.spec;
        luma += r.luma;
        mat += r.mat;
        if(all)
        {
            char from[24] = "";
            if(r.normal && r.normalFrame)
            {
                q_snprintf(from, sizeof(from), " (+%c's, %.2f)", r.normalFrame, r.normalMatch);
            }
            Con_Printf("%-16s %s match %5.2f%s%s%s%s%s\n", r.texname, r.missing ? "none   " : r.skipped ? "differs" : "used   ", r.match,
                r.normal ? " norm" : "", from, r.spec ? " spec" : "", r.luma ? " luma" : "", r.mat ? " mat" : "");
        }
    }
    Con_Printf("extmaps %s: %d textures: %d used (%d normal maps, %d specular, %d glow, %d .mat), %d differ, %d not in the pack; "
               "item boxes %d of %d (%s)\n",
        recordsWorld, n, n - missing - skipped, normal, spec, luma, mat, skipped, missing, itemsUsed, items,
        vr_extmaps.value != 0.f ? vr_extmaps_dir.string : "vr_extmaps 0");
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_extmaps_stats", stats_f);
}

} // namespace qvr::extmaps

using namespace qvr;

// Image_LoadImage: whether `path` is one of vr_extmaps_dir's files ("vrext/<file>").
extern "C" int VR_ExtMapsIsPath(const char* path)
{
    return strncmp(path, extmaps::prefix, extmaps::prefixLen) == 0;
}

// ... that file opened (com_filesize its size), or NULL.
extern "C" FILE* VR_ExtMapsOpen(const char* path)
{
    const char* dir = vr_extmaps_dir.string;
    if(!VR_ExtMapsIsPath(path) || !dir[0])
    {
        return nullptr;
    }
    char full[MAX_OSPATH];
    q_snprintf(full, sizeof(full), "%s/%s", dir, path + extmaps::prefixLen);
    FILE* f = nullptr;
    if(!extmaps::isAbsolute(dir))
    {
        COM_FOpenFile(full, &f, nullptr);
        return f;
    }
    f = Sys_fopen(full, "rb");
    if(f)
    {
        fseek(f, 0, SEEK_END);
        com_filesize = ftell(f);
        fseek(f, 0, SEEK_SET);
    }
    return f;
}

// Mod_LoadTextures, a regular texture, before its upload (which mipmaps `data` in place): whether the pack's maps are
// to be used on it (VR_ExtMapsAttach next): its picture there, compared with `data` (RGBA, or Quake's 8-bit), matches
// (vr_extmaps_match). Its .mat is read here. A frame of an animation the pack has no normal map for (basebtn+1, the
// lit frame: only basebtn+0 has one) or no picture at all (+abasebtn) takes another frame's (siblingNormal): else it
// had the one made from its own shading, with other bumps and heights, and the relief jumped as the frames changed.
namespace
{

// The detail signatures prepare compares (vr_extmaps_match), on the hunk: the texture's (made once), a pack
// picture's, and scratch.
struct Signatures
{
    const byte* data;
    enum srcformat fmt;
    int width, height;
    float* sig;
    bool made{false};

    // How well a pack picture matches the texture (correlation of their detail; -1: another shape).
    float match(const byte* img, enum srcformat efmt, int ew, int eh)
    {
        constexpr int n = extmaps::sigSize * extmaps::sigSize;
        if(efmt != SRC_RGBA || static_cast<long long>(ew) * height != static_cast<long long>(eh) * width)
        {
            return -1.f;
        }
        if(!made)
        {
            extmaps::signature(data, fmt, width, height, sig, sig + 2 * n);
            made = true;
        }
        extmaps::signature(img, efmt, ew, eh, sig + n, sig + 2 * n);
        return extmaps::correlation(sig, sig + n);
    }
};

// Frames in the order tried: an animation's own (+0..+9) from its first, then its alternates (+a..+j); an alternate's
// the other way round.
constexpr const char* framesMain = "0123456789abcdefghij";
constexpr const char* framesAlt = "abcdefghij0123456789";

// `anim` ("basebtn+1"): another frame's normal map whose picture matches the texture (as vr_extmaps_match asks: any at
// 0) into p.normal, p.normalFrame, p.normalMatch: whether there is one.
bool siblingNormal(extmaps::Pending& p, const char* anim, Signatures& sigs, float need)
{
    const char* plus = strrchr(anim, '+');
    if(!plus || !plus[1] || plus[2])
    {
        return false;
    }
    const char own = plus[1];
    const size_t stemLen = static_cast<size_t>(plus - anim);
    for(const char* c = own >= 'a' ? framesAlt : framesMain; *c; c++)
    {
        if(*c == own)
        {
            continue;
        }
        char base[MAX_QPATH], file[MAX_QPATH];
        q_snprintf(base, sizeof(base), "%.*s+%c", static_cast<int>(stemLen), anim, *c);
        extmaps::Material mat;
        extmaps::readMat(base, mat);
        if(!extmaps::normalFile(base, mat, file, sizeof(file)))
        {
            continue;
        }
        float m = -2.f;
        if(need > 0.f)
        {
            char pic[MAX_QPATH];
            q_snprintf(pic, sizeof(pic), "%s%s", extmaps::prefix, mat.diffusemap[0] ? extmaps::fileOf(mat.diffusemap) : base);
            const int mark = Hunk_LowMark();
            int ew = 0, eh = 0;
            enum srcformat efmt = SRC_RGBA;
            const byte* img = Image_LoadImage(pic, &ew, &eh, &efmt);
            m = img ? sigs.match(img, efmt, ew, eh) : -1.f;
            Hunk_FreeToLowMark(mark);
            if(m < need)
            {
                Con_DPrintf("extmaps: %s: %s's normal map not taken (match %.2f)\n", p.texname, base, m);
                continue;
            }
        }
        q_strlcpy(p.normal, file, sizeof(p.normal));
        p.normalFrame = *c;
        p.normalMatch = m;
        return true;
    }
    return false;
}

int prepare(const qmodel_t* mod, const char* texname, const byte* data, enum srcformat fmt, int width, int height)
{
    extmaps::pending = extmaps::Pending{};
    if(vr_extmaps.value == 0.f || isDedicated || !data || width <= 0 || height <= 0 || (fmt != SRC_RGBA && fmt != SRC_INDEXED))
    {
        return 0;
    }
    extmaps::Pending& p = extmaps::pending;
    q_strlcpy(p.texname, texname, sizeof(p.texname));
    extmaps::packName(texname, p.base, sizeof(p.base));
    char anim[MAX_QPATH]; // the pack's name for it with its frame (p.base may lose it below)
    q_strlcpy(anim, p.base, sizeof(anim));
    extmaps::readMat(p.base, p.mat);
    extmaps::Record& r = extmaps::record(mod, texname);

    const float need = za::clamp(vr_extmaps_match.value, 0.f, 1.f);
    const int mark = Hunk_LowMark();
    constexpr int sigFloats = extmaps::sigSize * extmaps::sigSize * 3;
    Signatures sigs{data, fmt, width, height, need > 0.f ? static_cast<float*>(Hunk_AllocNoFill(sizeof(float) * sigFloats)) : nullptr};
    const int pictures = Hunk_LowMark(); // the pack's pictures, after the signatures
    char file[MAX_QPATH];
    q_snprintf(file, sizeof(file), "%s%s", extmaps::prefix, p.mat.diffusemap[0] ? extmaps::fileOf(p.mat.diffusemap) : p.base);
    int ew = 0, eh = 0;
    enum srcformat efmt = SRC_RGBA;
    byte* img = Image_LoadImage(file, &ew, &eh, &efmt);
    char* frame = strchr(p.base, '+');
    if(!img && frame && !p.mat.diffusemap[0]) // an animation the pack has one picture of (floorsw for +0floorsw...)
    {
        *frame = 0;
        extmaps::readMat(p.base, p.mat);
        q_snprintf(file, sizeof(file), "%s%s", extmaps::prefix, p.mat.diffusemap[0] ? extmaps::fileOf(p.mat.diffusemap) : p.base);
        img = Image_LoadImage(file, &ew, &eh, &efmt);
    }
    if(!img)
    {
        r.missing = true;
        // a frame it has no picture of (butn+a): only another frame's normal map, if one fits
        p.normalOnly = VR_NormalMaps() && siblingNormal(p, anim, sigs, need);
        Hunk_FreeToLowMark(mark);
        return p.normalOnly;
    }
    if(need > 0.f)
    {
        p.match = sigs.match(img, efmt, ew, eh); // -1, another shape: no match
    }
    Hunk_FreeToLowMark(pictures);
    r.match = p.match;
    p.ok = need <= 0.f || p.match >= need;
    r.skipped = !p.ok;
    if(!p.ok)
    {
        Con_DPrintf("extmaps: %s differs from the pack's %s (match %.2f)\n", texname, p.base, p.match);
    }
    else if(VR_NormalMaps() && !extmaps::normalFile(p.base, p.mat, p.normal, sizeof(p.normal)))
    {
        siblingNormal(p, anim, sigs, need); // a frame it has no normal map of (basebtn+1)
    }
    Hunk_FreeToLowMark(mark);
    return p.ok;
}

} // namespace

extern "C" int VR_ExtMapsPrepare(const qmodel_t* mod, const char* texname, const byte* data, enum srcformat fmt, int width, int height)
{
    if(vr_extmaps.value == 0.f)
    {
        extmaps::pending = extmaps::Pending{};
        return 0;
    }
    const double t0 = Sys_DoubleTime();
    const int ok = prepare(mod, texname, data, fmt, width, height);
    VR_TimeAdd("external material maps (vr_extmaps)", Sys_DoubleTime() - t0);
    return ok;
}

// Mod_LoadTextures, after the upload and the texture's own glow (`glows`: a _luma or _glow file, or Quake's fullbright
// colours): the pack's normal map (beside the made one; another frame's for a frame it has none of), specular map and
// glow (only if it has none) loaded onto tx; only that normal map for a frame it has no picture of.
extern "C" void VR_ExtMapsAttach(texture_t* tx, qmodel_t* mod, int glows)
{
    extmaps::Pending& p = extmaps::pending;
    if((!p.ok && !p.normalOnly) || !tx || !tx->gltexture || strcmp(p.texname, tx->name) != 0)
    {
        p = extmaps::Pending{};
        return;
    }
    const double t0 = Sys_DoubleTime();
    extmaps::Record& r = extmaps::record(mod, tx->name);
    char file[MAX_QPATH];
    int w, h;
    enum srcformat fmt;
    const int mark = Hunk_LowMark();

    if(VR_NormalMaps() && p.normal[0])
    {
        q_strlcpy(file, p.normal, sizeof(file)); // (prepare's: its own, or another frame's)
        tx->extnormal = TexMgr_LoadExtNormalMap(tx->gltexture, file, 0, 0, nullptr, tx->width); // another texture's
        byte* img = tx->extnormal ? nullptr : Image_LoadImage(file, &w, &h, &fmt);
        if(img && fmt == SRC_RGBA)
        {
            tx->extnormal = TexMgr_LoadExtNormalMap(tx->gltexture, file, w, h, img, tx->width);
        }
        Hunk_FreeToLowMark(mark);
        r.normal = tx->extnormal != nullptr;
        r.normalFrame = p.normalFrame;
        r.normalMatch = p.normalMatch;
        if(p.normalFrame && tx->extnormal)
        {
            Con_DPrintf("extmaps: %s takes +%c's normal map %s (match %.2f)\n", tx->name, p.normalFrame, file, p.normalMatch);
        }
    }
    if(p.normalOnly)
    {
        p = extmaps::Pending{};
        VR_TimeAdd("external material maps (vr_extmaps)", Sys_DoubleTime() - t0);
        return;
    }

    if(p.mat.specularmap[0])
    {
        q_snprintf(file, sizeof(file), "%s%s", extmaps::prefix, extmaps::fileOf(p.mat.specularmap));
    }
    else
    {
        q_snprintf(file, sizeof(file), "%s%s_spec", extmaps::prefix, p.base);
    }
    tx->extspec = TexMgr_FindTexture(mod, file);
    byte* img = tx->extspec ? nullptr : Image_LoadImage(file, &w, &h, &fmt);
    if(img)
    {
        tx->extspec = TexMgr_LoadImage(mod, file, w, h, fmt, img, file, 0, TEXPREF_MIPMAP | TEXPREF_BINDLESS);
    }
    Hunk_FreeToLowMark(mark);
    // Quetoo's numbers: specularity times the specular map, hardness times Blinn's exponent (0: as default)
    tx->extmat[0] = 1.f;
    tx->extmat[1] = za::clamp(p.mat.specularity, 0.f, 4.f);
    tx->extmat[2] = p.mat.hardness > 0.f ? za::clamp(p.mat.hardness, 0.25f, 8.f) : 1.f;
    tx->extmat[3] = 0.f;

    if(!glows && !tx->fullbright)
    {
        static constexpr const char* suffixes[] = {"_luma", "_glow"};
        for(const char* suffix : suffixes)
        {
            q_snprintf(file, sizeof(file), "%s%s%s", extmaps::prefix, p.base, suffix);
            img = Image_LoadImage(file, &w, &h, &fmt);
            if(img)
            {
                tx->fullbright = TexMgr_LoadImage(mod, file, w, h, fmt, img, file, 0, TEXPREF_MIPMAP | TEXPREF_BINDLESS);
                tx->extluma = tx->fullbright != nullptr;
                Hunk_FreeToLowMark(mark);
                break;
            }
        }
    }
    Hunk_FreeToLowMark(mark);

    r.spec = tx->extspec != nullptr;
    r.luma = tx->extluma != 0;
    r.mat = p.mat.specularity != 1.f || p.mat.hardness != 1.f || p.mat.normalmap[0] || p.mat.specularmap[0];
    p = extmaps::Pending{};
    VR_TimeAdd("external material maps (vr_extmaps)", Sys_DoubleTime() - t0);
}

// R_AddBModelCall: a texture's maps as drawn: the pack's normal map instead of the made one (vr_extmaps_normals), its
// specular map (vr_extmaps_spec: `spec`, `extmat` y its brightness, z its hardness, CF_SPECMAP returned), its glow
// hidden if off (vr_extmaps_luma); all of them as without the pack while vr_extmaps_ab is on.
extern "C" unsigned VR_ExtMapsCall(const texture_t* t, gltexture_t** normalmap, gltexture_t** spec, gltexture_t** fullbright,
    float extmat[4])
{
    extmat[0] = extmat[1] = extmat[2] = extmat[3] = 1.f;
    *spec = nullptr;
    if(!t)
    {
        return 0u;
    }
    const bool show = vr_extmaps_ab.value == 0.f;
    if(t->extluma && (!show || vr_extmaps_luma.value == 0.f) && *fullbright == t->fullbright)
    {
        *fullbright = nullptr;
    }
    if(!show)
    {
        return 0u;
    }
    if(t->extnormal && vr_extmaps_normals.value != 0.f)
    {
        *normalmap = t->extnormal;
    }
    if(t->extspec && vr_extmaps_spec.value != 0.f)
    {
        *spec = t->extspec;
        extmat[1] = t->extmat[1] * za::clamp(vr_extmaps_spec_scale.value, 0.f, 16.f);
        extmat[2] = t->extmat[2];
        return extmaps::cfSpecMap;
    }
    return 0u;
}

// VR_MakeNormalMap, an external pack's normal map (NORMALMAP_EXT), RGBA: its green made ours (y up the rows: a slope up
// them tilts it towards +y) where it runs the other way (vr_extmaps_green: 0 judged from its heights, the alpha: green
// against their slope down the rows; 1 as it is; 2 turned). Quetoo's runs down the rows (correlation -0.9 over its maps).
extern "C" void VR_ExtMapsGreen(byte* data, int width, int height, const char* name)
{
    const int mode = static_cast<int>(vr_extmaps_green.value);
    bool flip = mode == 2;
    if(mode == 0 && width > 2 && height > 2)
    {
        double gs = 0, ss = 0, gg = 0;
        for(int y = 0; y < height; y++)
        {
            const byte* up = data + static_cast<size_t>((y + height - 1) % height) * width * 4;
            const byte* down = data + static_cast<size_t>((y + 1) % height) * width * 4;
            const byte* row = data + static_cast<size_t>(y) * width * 4;
            for(int x = 0; x < width; x++)
            {
                const double slope = down[x * 4 + 3] - up[x * 4 + 3]; // the height's rise down the rows
                const double g = row[x * 4 + 1] - 127.5;
                gs += g * slope;
                ss += slope * slope;
                gg += g * g;
            }
        }
        // ours: green with the rise down the rows (its normal leans up the rows, away from it... as TexMgr_ShadingToNormals)
        const double c = ss > 0 && gg > 0 ? gs / sqrt(ss * gg) : 0;
        flip = c < -0.2;
        Con_DPrintf("extmaps: %s green %s (%.2f)\n", name, flip ? "turned" : "kept", c);
    }
    if(!flip)
    {
        return;
    }
    const size_t n = static_cast<size_t>(width) * height;
    for(size_t i = 0; i < n; i++)
    {
        data[i * 4 + 1] = static_cast<byte>(255 - data[i * 4 + 1]);
    }
}
