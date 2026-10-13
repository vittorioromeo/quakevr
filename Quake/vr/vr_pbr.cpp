// Content-addressed companion maps. No diffuse replacement or game-specific texture-name rules.
#include "vr_pbr.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_extmaps.hpp"
#include "vr_sha256.hpp"
#include "vr_mem.hpp"
#include "vr_modelmetadata.h"
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
    unsigned generatedFlags{};
};
za::Vector<Recipe> recipes; // Small reload recipes only; decoded pixels stay on the hunk.
struct Use
{
    gltexture_t* base{};
    char hash[65]{};
    bool found{}, normal{}, height{};
    bool attached{};
    bool preserveMaterial{};
    unsigned generatedFlags{};
};
za::Vector<Use> uses;
struct Provenance
{
    char source[33]{}, output[65]{};
    unsigned flags{};
};
struct Strength
{
    char path[MAX_QPATH]{};
    unsigned flags{};
};
za::Vector<Strength> strengths; // the live textures' companions (VR_PBRForget empties it with them)
struct StockSource { char hash[33]{}; };
// A companion's state by its source's hash: missing, or its verified generated components (verifiedFlags).
struct Companion
{
    char source[33]{};
    bool missing{};
    unsigned flags{};
};

// The bundle's indexes and what its companions were verified as: content-addressed, so kept until the game
// folders change (not re-read and re-hashed at every map).
struct Session
{
    za::Vector<Provenance> provenance;
    za::Vector<StockSource> stockSources;
    za::Vector<Companion> companions;
    bool provenanceLoaded{};
    bool stockLoaded{};
    auto members() { return mem::list(provenance, stockSources, companions, provenanceLoaded, stockLoaded); }
};
mem::Cache<Session> session{"pbr companions", mem::GameDirChange};

// A text index's records after its header line: LF or CRLF (a checkout or an editor may have made either). Each
// record line goes to `record` (without its line end); false when the header differs.
template <class F>
bool parseLines(const char* data, const char* header, F&& record)
{
    const size_t headerLength = strlen(header);
    if(!data || strncmp(data, header, headerLength)) return false;
    const char* line = data + headerLength;
    if(*line == '\r') ++line;
    if(*line != '\n') return false;
    ++line;
    while(*line)
    {
        const char* end = strchr(line, '\n');
        size_t length = end ? static_cast<size_t>(end - line) : strlen(line);
        const char* next = end ? end + 1 : line + length;
        if(length && line[length - 1] == '\r') --length;
        if(length && !record(line, length)) break; // malformed: the rest is not trusted
        line = next;
    }
    return true;
}

int parseStock(const char* data, za::Vector<StockSource>& out)
{
    int records{};
    parseLines(data, "qvr-pbr-stock-v1", [&](const char* line, size_t length)
    {
        if(length != 32 || strspn(line, "0123456789abcdef") < 32) return false;
        StockSource source{};
        memcpy(source.hash, line, 32);
        out.emplaceBack(source);
        ++records;
        return true;
    });
    return records;
}

int parseProvenance(const char* data, za::Vector<Provenance>& out)
{
    int records{};
    parseLines(data, "qvr-pbr-provenance-v1", [&](const char* line, size_t length)
    {
        char text[160];
        if(length >= sizeof(text)) return false;
        memcpy(text, line, length);
        text[length] = 0;
        Provenance p{};
        int consumed{};
        if(sscanf(text, "%32s %64s %u%n", p.source, p.output, &p.flags, &consumed) != 3 ||
            strlen(p.source) != 32 || strlen(p.output) != 64 || p.flags > 3 || consumed == 0)
            return false; // Malformed or unknown records never acquire generated strength.
        out.emplaceBack(p);
        ++records;
        return true;
    });
    return records;
}

// A present index that gives no records is a broken file, not an empty bundle: say so.
template <class T>
void loadIndex(const char* file, int (*parse)(const char*, za::Vector<T>&), za::Vector<T>& out)
{
    const int mark = Hunk_LowMark();
    const char* data = reinterpret_cast<const char*>(COM_LoadHunkFile(file, nullptr));
    if(data && parse(data, out) == 0)
        Con_Warning("%s: no records read (malformed?)\n", file);
    Hunk_FreeToLowMark(mark);
}

bool isStockSource(const char* hash)
{
    if(!session.stockLoaded)
    {
        session.stockLoaded = true;
        loadIndex("textures/pbr/stock.txt", parseStock, session.stockSources);
    }
    for(const StockSource& source : session.stockSources)
        if(!strncmp(source.hash, hash, 32)) return true;
    return false;
}

void loadProvenance()
{
    if(session.provenanceLoaded) return;
    session.provenanceLoaded = true;
    loadIndex("textures/pbr/provenance.txt", parseProvenance, session.provenance);
}

Companion* companion(const char* hash)
{
    for(Companion& c : session.companions)
        if(!strncmp(c.source, hash, 32)) return &c;
    return nullptr;
}

void rememberStrength(const char* path, unsigned flags)
{
    for(Strength& s : strengths)
        if(!strcmp(s.path, path)) { s.flags = flags; return; }
    Strength s{};
    q_strlcpy(s.path, path, sizeof(s.path));
    s.flags = flags;
    strengths.emplaceBack(s);
}

void componentStrength(unsigned flags, float multiplier, float* normal, float* height)
{
    multiplier = CLAMP(0.f, multiplier, 8.f);
    *normal = flags & 1u ? multiplier : 1.f;
    *height = flags & 2u ? multiplier : 1.f;
}


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

unsigned verifiedFlags(const char* source, const byte* rgba, int w, int h)
{
    loadProvenance();
    char digest[65];
    hashSource(rgba, SRC_RGBA, w, h, digest);
    for(const Provenance& p : session.provenance)
        if(!strncmp(p.source, source, 32) && !strcmp(p.output, digest)) return p.flags;
    return 0; // Missing/stale metadata is conservatively treated as authored.
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
        if(!u.base || (u.base->owner != cl.worldmodel && (Cmd_Argc() < 2 || strcmp(Cmd_Argv(1), "models"))))
            continue;
        ++queried;
        found += u.found;
        normal += u.normal;
        height += u.height;
        if(Cmd_Argc() > 1)
        {
            Con_Printf("pbr: %s %.32s %s authored normal %d height %d\n", u.base->name, u.hash,
                u.found ? "found" : "missing", u.normal, u.height);
            gltexture_t* selected = TexMgr_NormalMap(u.base);
            float ns{}, hs{};
            VR_PBRStrength(selected, &ns, &hs);
            Con_Printf("pbr strength: %s source %s generated %u normal %.2f height %.2f\n", u.base->name, selected->source_file, u.generatedFlags, ns, hs);
            if(u.base->owner)
                for(int i = 0; i < u.base->owner->numtextures; ++i)
                {
                    const texture_t* tx = u.base->owner->textures[i];
                    if(!tx || tx->gltexture != u.base) continue;
                    gltexture_t* nm = TexMgr_NormalMap(tx->gltexture);
                    gltexture_t* spec{};
                    gltexture_t* fullbright = tx->fullbright;
                    float material[4];
                    VR_ExtMapsCall(tx, &nm, &spec, &fullbright, material);
                    VR_PBRStrength(nm, &ns, &hs);
                    Con_Printf("pbr strength active: %s normal %.2f height %.2f\n", tx->name, ns, hs);
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

// Machine-readable GPU census: includes all precached brush and alias models, animated frames,
// and the external authored normal actually selected for each world material. No image export.
void audit_f()
{
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    const bool worldOnly = Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "world");
    auto audit = [](qmodel_t* owner, const char* name, gltexture_t* base, gltexture_t* nm)
    {
        if(!nm) return;
        const size_t bytes = static_cast<size_t>(nm->width) * nm->height * 4;
        byte* pixels = static_cast<byte*>(VR_HeapMalloc(bytes));
        GL_Bind(GL_TEXTURE0, nm);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        char digest[65];
        hashSource(pixels, SRC_RGBA, nm->width, nm->height, digest);
        const size_t count = bytes / 4;
        byte* alpha = static_cast<byte*>(VR_HeapMalloc(count));
        for(size_t p = 0; p < count; ++p)
        {
            alpha[p] = pixels[p * 4 + 3];
            pixels[p * 4 + 3] = 255;
        }
        char rgbDigest[65], heightDigest[65];
        hashSource(pixels, SRC_RGBA, nm->width, nm->height, rgbDigest);
        sha256::Hasher heightHash;
        heightHash.update(alpha, count);
        sha256::toHex(heightHash.finish(), heightDigest);
        VR_HeapFree(alpha);
        VR_HeapFree(pixels);
        float ns{}, hs{};
        VR_PBRStrength(nm, &ns, &hs);
        bool authored = TexMgr_NormalMapAuthored(base);
        if(nm != TexMgr_NormalMap(base))
        {
            authored = true; // External authored material, unless it is another animation frame's procedural map.
            bool resolved = false;
            for(int model = 1; model < MAX_MODELS && !resolved; ++model)
            {
                const qmodel_t* materialOwner = cl.model_precache[model];
                if(!materialOwner || materialOwner->type != mod_brush) continue;
                for(int i = 0; i < materialOwner->numtextures; ++i)
                {
                    const texture_t* candidate = materialOwner->textures[i];
                    if(candidate && candidate->gltexture && TexMgr_NormalMap(candidate->gltexture) == nm)
                    {
                        authored = TexMgr_NormalMapAuthored(candidate->gltexture);
                        resolved = true;
                        break;
                    }
                }
            }
        }
        Con_Printf("pbr audit: %d %s %s %s %u %u %s %s %.2f %.2f %s %s %d\n", vr_pbr.value != 0.f,
            cl.worldmodel ? cl.worldmodel->name : "none", owner->name, name,
            nm->width, nm->height, digest, nm->source_file[0] ? nm->source_file : "flat", ns, hs, rgbDigest, heightDigest, authored);
    };
    for(int model = 1; model < MAX_MODELS; ++model)
    {
        qmodel_t* owner = cl.model_precache[model];
        if(!owner || owner->name[0] == '*') continue;
        if(owner->type == mod_alias)
        {
            if(worldOnly) continue;
            for(aliashdr_t* hdr = static_cast<aliashdr_t*>(Mod_Extradata(owner)); hdr; hdr = Mod_NextSurface(hdr))
                for(int skin = 0; skin < hdr->numskins; ++skin)
                    for(int frame = 0; frame < 4; ++frame)
                    {
                        if(!hdr->gltextures[skin][frame]) continue;
                        bool repeated = false;
                        for(int prior = 0; prior < frame; ++prior)
                            repeated |= hdr->gltextures[skin][prior] == hdr->gltextures[skin][frame];
                        if(repeated) continue;
                        char name[32];
                        q_snprintf(name, sizeof(name), "skin%d.%d", skin, frame);
                        audit(owner, name, hdr->gltextures[skin][frame], TexMgr_NormalMap(hdr->gltextures[skin][frame]));
                    }
            continue;
        }
        if(owner->type != mod_brush) continue;
        for(int i = 0; i < owner->numtextures; ++i)
        {
            texture_t* tx = owner->textures[i];
            if(!tx || !tx->gltexture) continue;
            gltexture_t* nm = TexMgr_NormalMap(tx->gltexture);
            gltexture_t* spec{};
            gltexture_t* fullbright = tx->fullbright;
            float material[4];
            VR_ExtMapsCall(tx, &nm, &spec, &fullbright, material);
            audit(owner, tx->name, tx->gltexture, nm);
        }
    }
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
    float normal{}, height{};
    for(unsigned flags = 0; flags < 4; ++flags)
    {
        componentStrength(flags, 3.f, &normal, &height);
        check(normal == (flags & 1u ? 3.f : 1.f) && height == (flags & 2u ? 3.f : 1.f),
            "generated/mixed/authored independent strength");
    }
    componentStrength(3, -1.f, &normal, &height);
    check(normal == 0.f && height == 0.f, "generated zero lower bound");
    componentStrength(3, 99.f, &normal, &height);
    check(normal == 8.f && height == 8.f, "generated upper bound");
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "stock"))
    {
        int checked{};
        for(const Use& u : uses)
        {
            if(!u.base || !isStockSource(u.hash)) continue; // (none has a companion: not shipped)
            ++checked;
            const gltexture_t* nm = TexMgr_NormalMap(u.base);
            float ns{}, hs{};
            VR_PBRStrength(nm, &ns, &hs);
            check(u.preserveMaterial && !u.attached && !u.generatedFlags && ns == 1.f && hs == 1.f,
                "stock source retains existing components and strength");
        }
        check(checked > 6, "stock coverage extends beyond explosive boxes");
        check(!isStockSource("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"),
            "unknown content remains eligible for generated relief");
    }
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
    loadProvenance();
    Provenance proof{};
    q_strlcpy(proof.source, "00000000000000000000000000000000", sizeof(proof.source));
    hashSource(pixels, SRC_RGBA, 2, 1, proof.output);
    proof.flags = 3;
    session.provenance.emplaceBack(proof);
    check(verifiedFlags(proof.source, pixels, 2, 1) == 3, "matching decoded companion provenance");
    check(verifiedFlags(proof.source, changed, 2, 1) == 0, "stale companion provenance stays authored");
    check(verifiedFlags(proof.source, pixels, 1, 2) == 0, "provenance binds output dimensions");
    session.provenance.popBack();
    {
        // The indexes parse alike with LF and CRLF line ends (a Windows checkout or an editor can make the latter).
        auto withCRLF = [](const char* text)
        {
            const size_t length = strlen(text);
            char* out = static_cast<char*>(Hunk_AllocNoFill(length * 2 + 1));
            char* o = out;
            for(const char* c = text; *c; ++c)
            {
                if(*c == '\n' && (c == text || c[-1] != '\r')) *o++ = '\r';
                *o++ = *c;
            }
            *o = 0;
            return out;
        };
        const int mark = Hunk_LowMark();
        const char* stock = reinterpret_cast<const char*>(COM_LoadHunkFile("textures/pbr/stock.txt", nullptr));
        za::Vector<StockSource> lf, crlf;
        const int records = parseStock(stock, lf);
        check(records > 1000 && parseStock(withCRLF(stock ? stock : ""), crlf) == records &&
                  !memcmp(lf.back().hash, crlf.back().hash, 33),
            "shipped stock.txt parses, the same with CRLF");
        const char provenanceText[] = "qvr-pbr-provenance-v1\n"
            "000498127d0fd3e511919fd2abe15e2d 16188e3b8952f9e0f48f677a14145fd03b9a083a7efee70c5694666b16c3b1c4 3\n"
            "0008d2d8e0c3c884494127187ebbc296 9716bbde367499f2b2e8c5f5bd00799d79869750d8c495b87b53d977e3f5298d 0\n";
        za::Vector<Provenance> plf, pcrlf;
        check(parseProvenance(provenanceText, plf) == 2 && parseProvenance(withCRLF(provenanceText), pcrlf) == 2 &&
                  pcrlf[0].flags == 3 && !strcmp(pcrlf[1].output, plf[1].output),
            "provenance parses the same with CRLF");
        za::Vector<StockSource> bad;
        check(parseStock("qvr-pbr-stock-v1\r\n0123456789abcdef0123456789abcdeX\n", bad) == 0 &&
                  parseStock("qvr-pbr-stock-v2\n0123456789abcdef0123456789abcdef\n", bad) == 0,
            "malformed stock lines and other headers give no records");
        Hunk_FreeToLowMark(mark);
    }
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
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "fixtures"))
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
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "boxes"))
    {
        int checked{};
        for(const Use& u : uses)
        {
            if(!u.base || !u.base->owner ||
                (!VR_ModelHasTrait(u.base->owner, VR_MODEL_TRAIT_ExplosiveBoxFile) &&
                 !VR_ModelHasTrait(u.base->owner, VR_MODEL_TRAIT_ExplosiveCrateFile))) continue;
            ++checked;
            gltexture_t* nm = TexMgr_NormalMap(u.base);
            float ns{}, hs{};
            VR_PBRStrength(nm, &ns, &hs);
            check(!u.attached && strncmp(nm->source_file, "textures/pbr/", 13) &&
                  strncmp(nm->source_file, "pbrmix/", 7) && ns == 1.f && hs == 1.f &&
                  TexMgr_NormalMapParallax(u.base) != 0, "box retains original material relief");
        }
        check(checked == 6, "both stock box models inspected");
    }
    Con_Printf("pbr test: %d/%d PASS\n", passed, total);
}
} // namespace

void init()
{
    Cmd_AddCommand("vr_pbr_stats", stats_f);
    Cmd_AddCommand("vr_pbr_test", test_f);
    Cmd_AddCommand("vr_pbr_audit", audit_f);
}
} // namespace qvr::pbr

extern "C" void VR_PBRExternal(gltexture_t* base, const char* normal, char* path, size_t size)
{
    using namespace qvr;
    q_strlcpy(path, normal, size);
    for(const pbr::Use& use : pbr::uses)
    {
        if(use.base != base || !use.found || use.preserveMaterial)
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
        r.generatedFlags = use.generatedFlags & 2u;
        sha256::Hasher identity;
        identity.update(r.generated, strlen(r.generated));
        identity.update(r.normal, strlen(r.normal) + 1);
        constexpr char tag[] = "external";
        identity.update(tag, sizeof(tag));
        char hex[65];
        sha256::toHex(identity.finish(), hex);
        q_snprintf(r.path, sizeof(r.path), "pbrmix/%.32s", hex);
        pbr::rememberStrength(r.path, r.generatedFlags);
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

// TexMgr_LoadNormalMap: a new normal map's verified generated components, by its source (once per upload).
extern "C" int VR_PBRGeneratedFlags(const char* source)
{
    if(source && source[0])
        for(const qvr::pbr::Strength& s : qvr::pbr::strengths)
            if(!strcmp(s.path, source)) return static_cast<int>(s.flags);
    return 0;
}

// Every draw batch (R_AddBModelCall): the flags resolved at load, no search.
extern "C" void VR_PBRStrength(const gltexture_t* normalmap, float* normal, float* height)
{
    qvr::pbr::componentStrength(static_cast<unsigned>(TexMgr_NormalMapGenerated(normalmap)), qvr::vr_pbr_strength.value,
        normal, height);
}

extern "C" int VR_PBREnabled()
{
    return qvr::vr_pbr.value != 0.f;
}

extern "C" void VR_PBRForget(gltexture_t* base)
{
    for(qvr::pbr::Use& u : qvr::pbr::uses)
        if(u.base == base)
            u.base = nullptr;
    for(const qvr::pbr::Use& u : qvr::pbr::uses)
        if(u.base) return;
    qvr::pbr::strengths.clear(); // (the indexes and verified companions stay: session, until the game folders change)
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
    // AD reuses many stock/mission-pack pixels, including installed diffuse replacements. They already
    // had procedural or authored relief before the learned corpus existed. Preserve their complete
    // established material path (including partial authored maps), also when AD reuses those pixels.
    const bool box = VR_ModelHasTrait(base->owner, VR_MODEL_TRAIT_ExplosiveBoxFile) ||
                     VR_ModelHasTrait(base->owner, VR_MODEL_TRAIT_ExplosiveCrateFile);
    use.preserveMaterial = box || pbr::isStockSource(use.hash);
    const int mark = Hunk_LowMark();
    int w{}, h{};
    enum srcformat f{};
    byte* generated = nullptr;
    if(!use.preserveMaterial) // (the bundle has no companions for stock textures: nothing to look up)
    {
        pbr::Companion* known = pbr::companion(use.hash);
        if(!known || !known->missing) // (a source without one: no lookups again this session)
        {
            generated = Image_LoadImage(r.generated, &w, &h, &f);
            use.found = generated && f == SRC_RGBA;
        }
        if(!known)
        {
            // Verified once per session: the bundle is content-addressed, so the same file decodes the same.
            pbr::Companion c{};
            memcpy(c.source, use.hash, 32);
            c.missing = !use.found;
            c.flags = use.found ? pbr::verifiedFlags(use.hash, generated, w, h) : 0u;
            pbr::session.companions.emplaceBack(c);
            known = &pbr::session.companions.back();
        }
        use.generatedFlags = use.found ? known->flags : 0u;
    }
    const bool completeExternal = !use.preserveMaterial && use.found && extmaps::completePending();
    if(use.found && !use.preserveMaterial)
    {
        r.generatedFlags = use.generatedFlags;
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
                pbr::rememberStrength(r.generated, r.generatedFlags);
                TexMgr_LoadNormalMap(base, r.generated, w, h, f, generated, r.generated, 0,
                    NORMALMAP_AUTHORED | NORMALMAP_FILE | NORMALMAP_HEIGHTS, worldwidth);
                use.attached = true;
            }
            else
            {
                r.generatedFlags &= ~(use.normal ? 1u : 0u);
                r.generatedFlags &= ~(use.height ? 2u : 0u);
                sha256::Hasher identity;
                identity.update(r.generated, strlen(r.generated));
                identity.update(r.normal, strlen(r.normal) + 1);
                identity.update(r.bump, strlen(r.bump) + 1);
                char hex[65];
                sha256::toHex(identity.finish(), hex);
                q_snprintf(r.path, sizeof(r.path), "pbrmix/%.32s", hex);
                pbr::rememberStrength(r.path, r.generatedFlags);
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
