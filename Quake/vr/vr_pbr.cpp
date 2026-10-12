// Content-addressed companion maps. No diffuse replacement or game-specific texture-name rules.
#include "vr_pbr.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_extmaps.hpp"
#include "vr_sha256.hpp"
#include "Zancle/Container/Vector.hpp"
#include <cstring>
#include <initializer_list>

namespace qvr::pbr
{
namespace
{
struct Recipe
{
    char path[MAX_QPATH]{};
    char generated[MAX_QPATH]{};
    char normal[MAX_OSPATH]{};
    char bump[MAX_OSPATH]{};
    bool external{};
};
za::Vector<Recipe> recipes; // Small reload recipes only; decoded pixels stay on the hunk.
struct Use
{
    gltexture_t* base{};
    char hash[65]{};
    bool found{}, normal{}, height{};
    bool attached{};
};
za::Vector<Use> uses;

void hashSource(const byte* data, enum srcformat fmt, int w, int h, char (&hex)[65])
{
    sha256::Hasher hash;
    constexpr char tag[] = "qvr-pbr-v1";
    hash.update(tag, sizeof(tag));
    byte dims[8];
    for(int i = 0; i < 4; ++i)
    {
        dims[i] = static_cast<byte>(static_cast<unsigned>(w) >> (i * 8));
        dims[i + 4] = static_cast<byte>(static_cast<unsigned>(h) >> (i * 8));
    }
    hash.update(dims, sizeof(dims));
    if(fmt == SRC_RGBA)
    {
        hash.update(data, static_cast<size_t>(w) * h * 4);
    }
    else
    {
        byte row[4096];
        const size_t pixels = static_cast<size_t>(w) * h;
        for(size_t start = 0; start < pixels; start += 1024)
        {
            const size_t count = q_min(size_t{1024}, pixels - start);
            for(size_t i = 0; i < count; ++i)
            {
                const byte* rgb = reinterpret_cast<const byte*>(&d_8to24table[data[start + i]]);
                memcpy(row + i * 4, rgb, 3);
                row[i * 4 + 3] = 255; // Hash the source palette, including index 255, before fence rendering.
            }
            hash.update(row, count * 4);
        }
    }
    sha256::toHex(hash.finish(), hex);
}

bool hasHeights(const byte* rgba, int w, int h)
{
    for(size_t i = 0, n = static_cast<size_t>(w) * h; i < n; ++i)
        if(rgba[i * 4 + 3] != 255)
            return true;
    return false;
}

// Preserve the component at its authored resolution, and sample the missing one periodically.
const byte* sample(const byte* pixels, int w, int h, int x, int y, int outW, int outH)
{
    const int sx = static_cast<int>(static_cast<size_t>(x) * w / outW);
    const int sy = static_cast<int>(static_cast<size_t>(y) * h / outH);
    return pixels + (static_cast<size_t>(sy) * w + sx) * 4;
}

byte* compose(const Recipe& recipe, int* w, int* h, enum srcformat* fmt)
{
    int gw{}, gh{}, nw{}, nh{}, bw{}, bh{};
    enum srcformat gf{}, nf{}, bf{};
    byte* generated = Image_LoadImage(recipe.generated, &gw, &gh, &gf);
    if(!generated || gf != SRC_RGBA)
        return nullptr;
    byte* normal = recipe.normal[0] ? Image_LoadImage(recipe.normal, &nw, &nh, &nf) : nullptr;
    byte* bump = recipe.bump[0] ? Image_LoadImage(recipe.bump, &bw, &bh, &bf) : nullptr;
    if(nf != SRC_RGBA) normal = nullptr;
    if(bf != SRC_RGBA) bump = nullptr;
    if(normal && recipe.external)
        VR_ExtMapsGreen(normal, nw, nh, recipe.normal); // Preserve the existing pack's rendered orientation.
    const bool height = normal && hasHeights(normal, nw, nh);
    *w = normal ? nw : bump ? bw : gw;
    *h = normal ? nh : bump ? bh : gh;
    *fmt = SRC_RGBA;
    byte* out = static_cast<byte*>(Hunk_AllocNoFill(static_cast<size_t>(*w) * *h * 4));
    for(int y = 0; y < *h; ++y)
    for(int x = 0; x < *w; ++x)
    {
        byte* p = out + (static_cast<size_t>(y) * *w + x) * 4;
        const byte* g = sample(generated, gw, gh, x, y, *w, *h);
        const byte* n = normal ? sample(normal, nw, nh, x, y, *w, *h) : g;
        memcpy(p, n, 3);
        const byte* b = bump ? sample(bump, bw, bh, x, y, *w, *h) : nullptr;
        p[3] = height ? n[3] : b ? static_cast<byte>((b[0] + b[1] + b[2]) / 3) : g[3];
    }
    return out;
}

void stats_f()
{
    int queried{}, found{}, normal{}, height{};
    for(const Use& u : uses)
    {
        if(!u.base || u.base->owner != cl.worldmodel)
            continue;
        ++queried;
        found += u.found;
        normal += u.normal;
        height += u.height;
        if(Cmd_Argc() > 1)
        {
            Con_Printf("pbr: %s %.32s %s authored normal %d height %d\n", u.base->name, u.hash,
                u.found ? "found" : "missing", u.normal, u.height);
            if(u.found && cl.worldmodel)
                for(int i = 0; i < cl.worldmodel->numtextures; ++i)
                {
                    const texture_t* tx = cl.worldmodel->textures[i];
                    if(!tx || tx->gltexture != u.base) continue;
                    gltexture_t* nm = TexMgr_NormalMap(tx->gltexture);
                    gltexture_t* spec{};
                    gltexture_t* fullbright = tx->fullbright;
                    float material[4];
                    VR_ExtMapsCall(tx, &nm, &spec, &fullbright, material);
                    const size_t n = static_cast<size_t>(nm->width) * nm->height;
                    byte* rgba = static_cast<byte*>(VR_HeapMalloc(n * 4));
                    GL_Bind(GL_TEXTURE0, nm);
                    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
                    int lo = 255, hi = 0;
                    for(size_t p = 0; p < n; ++p)
                    {
                        lo = q_min(lo, static_cast<int>(rgba[p * 4 + 3]));
                        hi = q_max(hi, static_cast<int>(rgba[p * 4 + 3]));
                    }
                    VR_HeapFree(rgba);
                    Con_Printf("pbr active: %s source %s %ux%u alpha %d..%d\n", tx->name,
                        nm->source_file, nm->width, nm->height, lo, hi);
                    break;
                }
        }
    }
    Con_Printf("pbr: %d/%d source hashes found; authored normal %d height %d; recipes %zu\n",
        found, queried, normal, height, static_cast<size_t>(recipes.size()));
}

void test_f()
{
    int passed{}, total{};
    auto check = [&](bool ok, const char* label)
    {
        ++total;
        passed += ok;
        Con_Printf("pbr test: %s %s\n", label, ok ? "PASS" : "FAIL");
    };
    const byte pixels[] = {1, 2, 3, 255, 4, 5, 6, 255};
    char a[65], b[65];
    hashSource(pixels, SRC_RGBA, 2, 1, a);
    check(!strcmp(a, "041b6db783e4f7010517b9834bbb0f6052d12caf0fd823f8d95fd407b23a67c6"), "SHA256 wire contract");
    hashSource(pixels, SRC_RGBA, 1, 2, b);
    check(strcmp(a, b) != 0, "dimensions distinguish sources");
    byte changed[8];
    memcpy(changed, pixels, sizeof(changed));
    changed[0] ^= 1;
    hashSource(changed, SRC_RGBA, 2, 1, b);
    check(strcmp(a, b) != 0, "same-name different pixels distinguish sources");
    const byte indexed[] = {0, 1, 254, 255};
    byte expanded[16];
    for(int i = 0; i < 4; ++i)
    {
        memcpy(expanded + i * 4, &d_8to24table[indexed[i]], 3);
        expanded[i * 4 + 3] = 255;
    }
    hashSource(indexed, SRC_INDEXED, 2, 2, a);
    hashSource(expanded, SRC_RGBA, 2, 2, b);
    check(!strcmp(a, b), "palette-expanded RGBA including index255");
    if(Cmd_Argc() > 1)
    {
        const int mark = Hunk_LowMark();
        constexpr const char* normals[] = {"", "textures/pbrtest/normal", "textures/pbrtest/complete", ""};
        constexpr const char* bumps[] = {"", "", "", "textures/pbrtest/bump"};
        constexpr byte rgb[][3] = {{128, 160, 250}, {140, 150, 245}, {145, 155, 240}, {128, 160, 250}};
        constexpr byte alpha[] = {191, 191, 213, 221};
        for(int i = 0; i < 4; ++i)
        {
            Recipe r{};
            q_strlcpy(r.generated, "textures/pbrtest/generated", sizeof(r.generated));
            q_strlcpy(r.normal, normals[i], sizeof(r.normal));
            q_strlcpy(r.bump, bumps[i], sizeof(r.bump));
            int w{}, h{};
            enum srcformat fmt{};
            byte* image = compose(r, &w, &h, &fmt);
            check(image && w == 4 && h == 4 && !memcmp(image, rgb[i], 3) && image[3] == alpha[i],
                i == 0 ? "generated RGBA" : i == 1 ? "authored RGB plus generated height" :
                i == 2 ? "complete authored RGB and alpha" : "generated RGB plus authored bump height");
        }
        Hunk_FreeToLowMark(mark);
    }
    Con_Printf("pbr test: %d/%d PASS\n", passed, total);
}
} // namespace

void init()
{
    Cmd_AddCommand("vr_pbr_stats", stats_f);
    Cmd_AddCommand("vr_pbr_test", test_f);
}
} // namespace qvr::pbr

extern "C" void VR_PBRExternal(gltexture_t* base, const char* normal, char* path, size_t size)
{
    using namespace qvr;
    q_strlcpy(path, normal, size);
    for(const pbr::Use& use : pbr::uses)
    {
        if(use.base != base || !use.found)
            continue;
        const int mark = Hunk_LowMark();
        int w{}, h{};
        enum srcformat fmt{};
        byte* img = Image_LoadImage(normal, &w, &h, &fmt);
        const bool fill = img && fmt == SRC_RGBA && !pbr::hasHeights(img, w, h);
        Hunk_FreeToLowMark(mark);
        if(!fill)
            return; // Complete authored maps keep exactly their existing source and upload path.
        pbr::Recipe r{};
        q_snprintf(r.generated, sizeof(r.generated), "textures/pbr/%.32s_norm", use.hash);
        q_strlcpy(r.normal, normal, sizeof(r.normal));
        r.external = true;
        sha256::Hasher identity;
        identity.update(r.generated, strlen(r.generated));
        identity.update(r.normal, strlen(r.normal) + 1);
        constexpr char tag[] = "external";
        identity.update(tag, sizeof(tag));
        char hex[65];
        sha256::toHex(identity.finish(), hex);
        q_snprintf(r.path, sizeof(r.path), "pbrmix/%.32s", hex);
        for(const pbr::Recipe& existing : pbr::recipes)
            if(!strcmp(existing.path, r.path))
            {
                q_strlcpy(path, r.path, size);
                return;
            }
        pbr::recipes.emplaceBack(r);
        q_strlcpy(path, r.path, size);
        return;
    }
}

extern "C" byte* VR_PBRImage(const char* name, int* w, int* h, enum srcformat* fmt)
{
    for(const qvr::pbr::Recipe& r : qvr::pbr::recipes)
        if(!strcmp(r.path, name))
            return qvr::pbr::compose(r, w, h, fmt);
    return nullptr;
}

extern "C" void VR_PBRForget(gltexture_t* base)
{
    for(qvr::pbr::Use& u : qvr::pbr::uses)
        if(u.base == base)
            u.base = nullptr;
}

extern "C" int VR_PBRNormal(gltexture_t* base, const char* image, const char* shared,
    const byte* data, enum srcformat fmt, int worldwidth, int flags)
{
    using namespace qvr;
    if(vr_pbr.value == 0.f || (flags & NORMALMAP_SKIN) || (fmt != SRC_RGBA && fmt != SRC_INDEXED))
        return 0;
    const double t0 = Sys_DoubleTime();
    pbr::Use use{};
    use.base = base;
    pbr::hashSource(data, fmt, base->source_width, base->source_height, use.hash);
    pbr::Recipe r{};
    q_snprintf(r.generated, sizeof(r.generated), "textures/pbr/%.32s_norm", use.hash);
    const int mark = Hunk_LowMark();
    int w{}, h{};
    enum srcformat f{};
    byte* generated = Image_LoadImage(r.generated, &w, &h, &f);
    use.found = generated && f == SRC_RGBA;
    const bool completeExternal = use.found && extmaps::completePending();
    if(use.found)
    {
        // Exact frame and map first. A _norm's RGB and nonflat alpha outrank generated components independently.
        for(const char* source : {image, shared})
        {
            if(!source) continue;
            char file[MAX_OSPATH];
            int aw{}, ah{};
            enum srcformat af{};
            q_snprintf(file, sizeof(file), "%s_norm", source);
            byte* a = !r.normal[0] ? Image_LoadImage(file, &aw, &ah, &af) : nullptr;
            if(a && af == SRC_RGBA)
            {
                q_strlcpy(r.normal, file, sizeof(r.normal));
                use.normal = true;
                use.height = pbr::hasHeights(a, aw, ah);
            }
            q_snprintf(file, sizeof(file), "%s_bump", source);
            a = !use.height && !r.bump[0] ? Image_LoadImage(file, &aw, &ah, &af) : nullptr;
            if(a && af == SRC_RGBA)
            {
                q_strlcpy(r.bump, file, sizeof(r.bump));
                use.height = true;
            }
        }
        if(!completeExternal && !(use.normal && use.height && !r.bump[0]))
        {
            if(!r.normal[0] && !r.bump[0])
            {
                TexMgr_LoadNormalMap(base, r.generated, w, h, f, generated, r.generated, 0,
                    NORMALMAP_AUTHORED | NORMALMAP_FILE | NORMALMAP_HEIGHTS, worldwidth);
                use.attached = true;
            }
            else
            {
                sha256::Hasher identity;
                identity.update(r.generated, strlen(r.generated));
                identity.update(r.normal, strlen(r.normal) + 1);
                identity.update(r.bump, strlen(r.bump) + 1);
                char hex[65];
                sha256::toHex(identity.finish(), hex);
                q_snprintf(r.path, sizeof(r.path), "pbrmix/%.32s", hex);
                bool registered = false;
                for(pbr::Recipe& existing : pbr::recipes)
                    if(!strcmp(existing.path, r.path))
                    {
                        existing = r;
                        registered = true;
                        break;
                    }
                if(!registered) pbr::recipes.emplaceBack(r);
                byte* mixed = pbr::compose(r, &w, &h, &f);
                if(mixed)
                {
                    TexMgr_LoadNormalMap(base, r.path, w, h, f, mixed, r.path, 0,
                        NORMALMAP_AUTHORED | NORMALMAP_FILE | NORMALMAP_HEIGHTS, worldwidth);
                    use.attached = true;
                }
                else use.found = false;
            }
        }
    }
    Hunk_FreeToLowMark(mark);
    bool recorded = false;
    for(pbr::Use& existing : pbr::uses)
        if(!existing.base || existing.base == base)
        {
            existing = use;
            recorded = true;
            break;
        }
    if(!recorded) pbr::uses.emplaceBack(use);
    VR_TimeAdd("content companion maps (vr_pbr)", Sys_DoubleTime() - t0);
    return use.attached;
}
