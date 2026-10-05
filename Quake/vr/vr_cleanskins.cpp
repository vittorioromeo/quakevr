#include "vr_alloccount.h"
// vr_cleanskins.cpp -- clean weapon skins: a skin's patch applied as it is uploaded (see vr_cleanskins.hpp).

#include "vr_cleanskins.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_mem.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_zancle.hpp"

using namespace qvr;

namespace
{

// The patch file: "QVRCLEAN", then little-endian u32s: version, width, height, bytes per pixel (1: palette indices, 4:
// RGBA), the hash of the pixels it was made for (pairsHash), the pair count; then the pairs (texel, texel copied into
// it; y * width + x). make_clean_skins.py's write_patch.
constexpr char magic[8] = {'Q', 'V', 'R', 'C', 'L', 'E', 'A', 'N'};
constexpr za::U32 version = 1;
constexpr za::SizeT headerBytes = 8 + 6 * 4;

// A patch looked for: `base` its name without ".clean" (progs/v_axe.mdl_0, progs/v_axe.mdl_0_hq), `prefix` the
// textures it serves (progs/v_axe.mdl:frame0, with its glow; progs/v_axe.mdl_0), its pairs at `first` in the pairs.
struct Entry
{
    char base[MAX_QPATH]{};
    char prefix[MAX_QPATH]{};
    bool found{false};
    int width{0};
    int height{0};
    int bpp{0};
    za::U32 hash{0};
    za::SizeT first{0};
    za::SizeT count{0};
    int applied{0};    // uploads it cleaned
    int mismatched{0}; // uploads of another file (its size or its pixels' hash): left as they were
};

// The patches looked for, by name (files: a game dir change, a model reload). The main thread (textures load there).
struct CleanCache
{
    za::Vector<Entry> entries;
    za::Vector<za::U32> pairs; // every found patch's (texel, source) pairs, two values each
    auto members() { return mem::list(entries, pairs); }
};
mem::Cache<CleanCache> cache{"clean skins", mem::GameDirChange | mem::ModelReload};

[[nodiscard]] za::U32 readU32(const byte* p)
{
    return static_cast<za::U32>(p[0]) | (static_cast<za::U32>(p[1]) << 8) | (static_cast<za::U32>(p[2]) << 16) |
           (static_cast<za::U32>(p[3]) << 24);
}

// FNV-1a (32 bits) of the texels the patch reads and writes, pair by pair (the one written, then the one read; each
// its `bpp` bytes): make_clean_skins.py's pairs_hash. Only those: the first upload of an 8-bit skin has its background
// filled (Mod_FloodFillSkin), a later one (TexMgr_ReloadImage) not, and the patches touch no texel the fill may change.
[[nodiscard]] za::U32 pairsHash(const byte* data, const za::U32* pairs, za::SizeT count, za::SizeT bpp)
{
    za::U32 h = 0x811C9DC5u;
    for(za::SizeT i = 0; i < count * 2; i++)
    {
        const byte* texel = data + pairs[i] * bpp;
        for(za::SizeT b = 0; b < bpp; b++)
        {
            h = (h ^ texel[b]) * 0x01000193u;
        }
    }
    return h;
}

// Reads `e.base`.clean into the cache: found, or not (none, or a broken one: said once).
void load(Entry& e)
{
    char path[MAX_QPATH];
    q_snprintf(path, sizeof(path), "%s.clean", e.base);
    if(!COM_FileExists(path, nullptr))
    {
        return;
    }
    byte* data = COM_LoadMallocFile(path, nullptr);
    if(!data)
    {
        return;
    }
    const za::SizeT size = static_cast<za::SizeT>(com_filesize);
    const char* why = nullptr;
    za::U32 w = 0;
    za::U32 h = 0;
    za::U32 bpp = 0;
    za::U32 count = 0;
    if(size < headerBytes || memcmp(data, magic, sizeof(magic)) != 0 || readU32(data + 8) != version)
    {
        why = "not a patch of this version";
    }
    else
    {
        w = readU32(data + 12);
        h = readU32(data + 16);
        bpp = readU32(data + 20);
        count = readU32(data + 28);
        if(w < 1 || h < 1 || w > 16384 || h > 16384 || (bpp != 1 && bpp != 4) ||
           size != headerBytes + static_cast<za::SizeT>(count) * 8)
        {
            why = "a bad header";
        }
    }
    za::Vector<za::U32>& pairs = cache.pairs;
    const za::SizeT first = pairs.size();
    for(za::U32 i = 0; !why && i < count; i++)
    {
        const za::U32 dst = readU32(data + headerBytes + static_cast<za::SizeT>(i) * 8);
        const za::U32 src = readU32(data + headerBytes + static_cast<za::SizeT>(i) * 8 + 4);
        if(dst >= w * h || src >= w * h)
        {
            why = "a texel outside the skin";
            break;
        }
        pairs.pushBack(dst);
        pairs.pushBack(src);
    }
    if(why)
    {
        pairs.resize(first);
        Con_Printf("clean skins: %s: %s\n", path, why);
    }
    else
    {
        e.found = true;
        e.width = static_cast<int>(w);
        e.height = static_cast<int>(h);
        e.bpp = static_cast<int>(bpp);
        e.hash = readU32(data + 24);
        e.first = first;
        e.count = count;
    }
    VR_HeapFree(data);
}

// The patch for `base` (looked for once).
[[nodiscard]] Entry& entryFor(const char* base, const char* prefix)
{
    for(Entry& e : cache.entries)
    {
        if(!strcmp(e.base, base))
        {
            return e;
        }
    }
    Entry& e = cache.entries.emplaceBack();
    q_strlcpy(e.base, base, sizeof(e.base));
    q_strlcpy(e.prefix, prefix, sizeof(e.prefix));
    load(e);
    return e;
}

[[nodiscard]] bool endsWith(const char* s, const char* end)
{
    const za::SizeT n = strlen(s);
    const za::SizeT m = strlen(end);
    return n >= m && !strcmp(s + n - m, end);
}

// The patch's name and the textures it serves, for texture `name` (bpp 1: an alias model's 8-bit skin,
// "progs/v_axe.mdl:frame0", a group's "progs/v_axe.mdl:frame0_1", their fullbrights "..._glow"; bpp 4: a full-colour
// replacement, "progs/v_axe.mdl_0"). False for any other texture.
[[nodiscard]] bool patchName(const char* name, int bpp, char (&base)[MAX_QPATH], char (&prefix)[MAX_QPATH])
{
    if(bpp == 1)
    {
        const char* frame = strstr(name, ".mdl:frame");
        if(!frame)
        {
            return false;
        }
        const char* p = frame + strlen(".mdl:frame");
        const char* digits = p;
        while(*p >= '0' && *p <= '9')
        {
            p++;
        }
        if(p == digits)
        {
            return false;
        }
        const za::SizeT modelLen = static_cast<za::SizeT>(frame - name) + 4; // up to ".mdl"
        const za::SizeT skinLen = static_cast<za::SizeT>(p - digits);
        int group = -1;
        if(p[0] == '_' && p[1] >= '0' && p[1] <= '9')
        {
            group = atoi(p + 1);
        }
        char model[MAX_QPATH];
        q_strlcpy(model, name, za::min(sizeof(model), modelLen + 1));
        char skin[16];
        q_strlcpy(skin, digits, za::min(sizeof(skin), skinLen + 1));
        if(group < 0)
        {
            q_snprintf(base, sizeof(base), "%s_%s", model, skin);
        }
        else
        {
            q_snprintf(base, sizeof(base), "%s_%s_%d", model, skin, group);
        }
        q_strlcpy(prefix, name, za::min(sizeof(prefix), static_cast<za::SizeT>(p - name) + 1));
        return true;
    }
    if(!strstr(name, ".mdl_") || endsWith(name, "_glow") || endsWith(name, "_luma") || endsWith(name, "_norm") ||
       endsWith(name, "_bump"))
    {
        return false;
    }
    q_snprintf(base, sizeof(base), "%s_hq", name);
    q_strlcpy(prefix, name, sizeof(prefix));
    return true;
}

void onCleanChanged(cvar_t*)
{
    // The skins with a patch, uploaded again (VR_CleanSkin applies it or not as they load).
    for(const Entry& e : cache.entries)
    {
        if(e.found)
        {
            TexMgr_ReloadImagesNamed(e.prefix);
        }
    }
}

} // namespace

extern "C" byte* VR_CleanSkin(const char* name, byte* data, int width, int height, int bpp)
{
    char base[MAX_QPATH];
    char prefix[MAX_QPATH];
    if(!name || !data || isDedicated || !patchName(name, bpp, base, prefix))
    {
        return data;
    }
    // Looked for even while off, so that turning it on uploads again only the skins it changes.
    Entry& e = entryFor(base, prefix);
    if(!e.found || !vr_gore_clean_skins.value)
    {
        return data;
    }
    const za::SizeT size = static_cast<za::SizeT>(width) * static_cast<za::SizeT>(height) * static_cast<za::SizeT>(bpp);
    const za::SizeT stride = static_cast<za::SizeT>(bpp);
    const za::U32* pairs = cache.pairs.data() + e.first; // `first` counts values (two a pair)
    if(e.width != width || e.height != height || e.bpp != bpp || pairsHash(data, pairs, e.count, stride) != e.hash)
    {
        if(!e.mismatched++)
        {
            Con_DPrintf("clean skins: %s.clean was made for another %s (left as it is)\n", e.base, name);
        }
        return data;
    }
    byte* out = static_cast<byte*>(Hunk_AllocNoFill(static_cast<int>(size))); // freed with the upload's (TexMgr's mark)
    memcpy(out, data, size);
    const za::U32* pair = pairs;
    for(za::SizeT i = 0; i < e.count; i++, pair += 2)
    {
        memcpy(out + pair[0] * stride, data + pair[1] * stride, stride);
    }
    e.applied++;
    return out;
}

namespace qvr::cleanskins
{

void init()
{
    Cvar_SetCallback(&vr_gore_clean_skins, onCleanChanged);
}

void list_f()
{
    int found = 0;
    for(const Entry& e : cache.entries)
    {
        if(!e.found)
        {
            continue;
        }
        found++;
        Con_Printf("%-28s %4d x %-4d %s %6d texels  applied %d  other file %d\n", e.base, e.width, e.height,
            e.bpp == 1 ? "8-bit" : "RGBA ", static_cast<int>(e.count), e.applied, e.mismatched);
    }
    Con_Printf("clean skins: %s; %d patches found of %d skins looked at\n",
        vr_gore_clean_skins.value ? "on" : "off", found, static_cast<int>(cache.entries.size()));
}

} // namespace qvr::cleanskins
