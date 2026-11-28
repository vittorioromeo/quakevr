// vr_humlook.cpp -- see vr_humlook.hpp.

#include "vr_humlook.hpp"

#include "vr_agarmor.hpp"
#include "vr_api.h"
#include "vr_api_render.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_glutil.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_testkit.hpp"
#include "vr_tunables.hpp"

#include "Zancle/Base/Memcmp.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strstr.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/String/String.hpp"

#include <cstdio>
#include <cstdlib>

namespace qvr::humlook
{
namespace
{
// The table's names: the rows (QC VR_AM_* order) and the categories (QC VR_HUM_CAT_*; 0 none).
constexpr const char* kindNames[] = {"grunt", "enforcer", "knight", "dknight"};
constexpr const char* categoryNames[] = {
    "none", "explosive", "energy", "flame", "poison", "shells", "nails", "melee", "own", "9", "10", "11", "12", "13", "14",
    "elite", // (catElite: an elite's clothes, whatever it holds; vr_humlook.hpp eliteBit)
};
static_assert(countof(categoryNames) == maxCategories && catElite == maxCategories - 1);
constexpr const char* partNames[] = {"clothes", "pack"};
// The masks' keys: a kind's body, or a worn pack (any kind's).
constexpr const char* keyNames[] = {"grunt", "enforcer", "knight", "dknight", "pack"};
// A texel's class names (Paint values; a palette row's class: none, clothes or pack).
constexpr const char* classNames[] = {"none", "clothes", "pack", "never"};

constexpr int bufferWords = 1 << 17; // the masks' buffer (512 KB: an enforcer's 576x384 skin takes 13824 words)

struct Table
{
    Spec specs[maxKinds][maxCategories][2];
    bool loaded{false};
    int rows{0};
};
// seat-shared: the looks' table, loaded once
Table table;

struct MaskSet
{
    unsigned char rows[numKeys][16]{}; // each key's palette rows' classes (ClassNone, ClassClothes, ClassPack)
    // Flesh: its rows (never dyed by any class), and a texel of a dyed row left as it is where at least `nearShare` of
    // the texels within `nearRadius` are flesh (the highlights of arms and faces share the uniforms' tans: a grunt's).
    bool flesh[16]{};
    float nearShare{0.35f};
    int nearRadius{2};
    bool loaded{false};
};

// The flesh round each texel (Skin::nearFlesh): its texels within the radius on the flesh's rows, over their count.
void markNearFlesh(Skin& s, const MaskSet& m)
{
    const int w = s.w, h = s.h, r = za::clamp(m.nearRadius, 0, 8);
    s.nearFlesh.clear();
    s.nearFlesh.resize(static_cast<za::SizeT>(w * h), 0);
    if(m.nearShare <= 0.f || m.nearShare > 1.f)
    {
        return;
    }
    // (a summed-area table of the flesh texels: each window counted in four reads)
    za::Vector<int> sum;
    sum.resize(static_cast<za::SizeT>((w + 1) * (h + 1)), 0);
    for(int y = 0; y < h; y++)
    {
        for(int x = 0; x < w; x++)
        {
            const int f = m.flesh[s.texels[static_cast<za::SizeT>(y * w + x)] >> 4] ? 1 : 0;
            sum[static_cast<za::SizeT>((y + 1) * (w + 1) + x + 1)] = f + sum[static_cast<za::SizeT>(y * (w + 1) + x + 1)] +
                sum[static_cast<za::SizeT>((y + 1) * (w + 1) + x)] - sum[static_cast<za::SizeT>(y * (w + 1) + x)];
        }
    }
    for(int y = 0; y < h; y++)
    {
        for(int x = 0; x < w; x++)
        {
            const int x0 = za::max(x - r, 0), x1 = za::min(x + r + 1, w), y0 = za::max(y - r, 0), y1 = za::min(y + r + 1, h);
            const int n = sum[static_cast<za::SizeT>(y1 * (w + 1) + x1)] - sum[static_cast<za::SizeT>(y0 * (w + 1) + x1)] -
                sum[static_cast<za::SizeT>(y1 * (w + 1) + x0)] + sum[static_cast<za::SizeT>(y0 * (w + 1) + x0)];
            s.nearFlesh[static_cast<za::SizeT>(y * w + x)] =
                static_cast<float>(n) >= m.nearShare * static_cast<float>((x1 - x0) * (y1 - y0)) ? 1 : 0;
        }
    }
}
// seat-shared: the masks' file, loaded once
MaskSet maskSet;
// seat-shared: the masks known (assets)
za::Vector<Skin> skins; // the masks known: read from the masks' file, or made for a drawn skin

// seat-shared: the GPU's copy of the masks
GLuint buffer = 0;
// seat-shared: with buffer
int bufferUsed = 0;
// seat-shared: scratch text
char pathText[MAX_OSPATH];

// seat-shared: the server's, per entity
za::Vector<unsigned short> sent;  // the server's: per entity, what the clients were last told
// seat-shared: the looks drawn, by entity: the client world's effects (phase 7)
za::Vector<unsigned short> looks; // the client's: per entity, its look from the server
// seat-shared: with looks (phase 7)
int numLooks = 0;

[[nodiscard]] int nameIndex(const char* name, const char* const* names, int n)
{
    for(int i = 0; i < n; i++)
    {
        if(!q_strcasecmp(name, names[i]))
        {
            return i;
        }
    }
    char* end = nullptr;
    const long v = strtol(name, &end, 10); // (a number: a row or category the names don't have yet)
    return end && end != name && *end == '\0' ? static_cast<int>(v) : -1;
}

// The next line of `text` (cut there, its comment gone), or nullptr at the end.
[[nodiscard]] char* nextLine(char*& text)
{
    if(!text || !*text)
    {
        return nullptr;
    }
    char* line = text;
    char* next = strchr(line, '\n');
    if(next)
    {
        *next++ = '\0';
    }
    text = next;
    char* hash = strchr(line, '#');
    if(hash)
    {
        *hash = '\0';
    }
    return line;
}

void loadTable()
{
    table = Table{};
    table.loaded = true;
    char* data = reinterpret_cast<char*>(COM_LoadMallocFile(tableName(), nullptr));
    if(!data)
    {
        Con_DPrintf("humlook: no %s (no recolours)\n", tableName());
        return;
    }
    char* cursor = data;
    int lineNo = 0;
    while(char* line = nextLine(cursor))
    {
        lineNo++;
        char kind[32], cat[32], part[32];
        float v[6];
        const int got = sscanf(line, "%31s %31s %31s %f %f %f %f %f %f", kind, cat, part, &v[0], &v[1], &v[2], &v[3],
            &v[4], &v[5]);
        if(got <= 0)
        {
            continue;
        }
        const int k = nameIndex(kind, kindNames, static_cast<int>(countof(kindNames)));
        const int c = nameIndex(cat, categoryNames, static_cast<int>(countof(categoryNames)));
        const int p = nameIndex(part, partNames, static_cast<int>(countof(partNames)));
        if(got != 9 || k < 0 || k >= maxKinds || c <= 0 || c >= maxCategories || p < 0 || p > 1)
        {
            Con_Warning("%s:%d: not a row (<type> <category> <part> r g b strength light tint)\n", tableName(), lineNo);
            continue;
        }
        Spec& s = table.specs[k][c][p];
        s.set = true;
        for(int i = 0; i < 3; i++)
        {
            s.rgb[i] = za::clamp(v[i], 0.f, 1.f);
        }
        s.strength = za::clamp(v[3], 0.f, 1.f);
        s.light = za::clamp(v[4], 0.f, 4.f);
        s.tint = za::clamp(v[5], 0.f, 1.f);
        table.rows++;
    }
    free(data);
    Con_DPrintf("humlook: %d rows from %s\n", table.rows, tableName());
}

// "a-b" or "a": texel ranges into `paint` as `klass`.
void readRanges(const char* text, Skin& s, unsigned char klass)
{
    const int n = s.w * s.h;
    const char* p = text;
    while(*p)
    {
        char* end = nullptr;
        const long a = strtol(p, &end, 10);
        if(end == p)
        {
            p++;
            continue;
        }
        long b = a;
        p = end;
        if(*p == '-')
        {
            b = strtol(p + 1, &end, 10);
            p = end;
        }
        for(long t = za::max(a, 0L); t <= b && t < n; t++)
        {
            s.paint[static_cast<za::SizeT>(t)] = klass;
        }
    }
}

[[nodiscard]] Skin* findSkin(const char* model, int skin, int key)
{
    for(Skin& s : skins)
    {
        if(s.skin == skin && s.key == key && !q_strcasecmp(s.model.cStr(), model))
        {
            return &s;
        }
    }
    return nullptr;
}

[[nodiscard]] Skin& addSkin(const char* model, int skin, int key)
{
    Skin* s = findSkin(model, skin, key);
    if(s)
    {
        return *s;
    }
    skins.pushBack(Skin{});
    Skin& made = skins.back();
    made.model = za::String{model};
    made.skin = skin;
    made.key = key;
    return made;
}

void loadMasks()
{
    maskSet = MaskSet{};
    maskSet.loaded = true;
    maskSet.flesh[7] = true; // (Quake's peach row; a `flesh` line says otherwise)
    bool fleshRead = false;
    skins.clear();
    bufferUsed = 0;
    char* data = reinterpret_cast<char*>(COM_LoadMallocFile(masksName(), nullptr));
    if(!data)
    {
        Con_DPrintf("humlook: no %s (no masks: nothing recoloured)\n", masksName());
        return;
    }
    char* cursor = data;
    int lineNo = 0;
    while(char* line = nextLine(cursor))
    {
        lineNo++;
        char what[16], a[MAX_QPATH], b[32] = "", c[32], d[32];
        int used = 0;
        const int got = sscanf(line, "%15s %63s %31s%n", what, a, b, &used);
        if(got < 2 || (got < 3 && q_strcasecmp(what, "flesh")))
        {
            continue;
        }
        if(!q_strcasecmp(what, "rows")) // rows <key> <class> <row>...: a key's palette rows of that class
        {
            const int key = nameIndex(a, keyNames, numKeys);
            const int klass = nameIndex(b, classNames, 3);
            if(key < 0 || key >= numKeys || klass < 0)
            {
                Con_Warning("%s:%d: rows <grunt|enforcer|knight|dknight|pack> <none|clothes|pack> <row>...\n", masksName(), lineNo);
                continue;
            }
            for(const char* p = line + used; *p;)
            {
                char* end = nullptr;
                const long r = strtol(p, &end, 10);
                if(end == p)
                {
                    break;
                }
                p = end;
                if(r >= 0 && r < 16)
                {
                    maskSet.rows[key][r] = static_cast<unsigned char>(klass);
                }
            }
            continue;
        }
        // skinrows <model> <skin> <key> <class> <row>... / paint <model> <skin> <key> <w> <h> <class> <ranges>...
        int used2 = 0;
        if(!q_strcasecmp(what, "skinrows") && sscanf(line + used, "%31s %31s%n", c, d, &used2) == 2)
        {
            const int key = nameIndex(c, keyNames, numKeys);
            const int klass = nameIndex(d, classNames, 3);
            if(key < 0 || key >= numKeys || klass < 0)
            {
                Con_Warning("%s:%d: skinrows <model> <skin> <key> <class> <row>...\n", masksName(), lineNo);
                continue;
            }
            Skin& s = addSkin(a, atoi(b), key);
            if(!s.ownRows)
            {
                s.ownRows = true;
                ZA_MEMSET(s.rows, 0, sizeof(s.rows));
            }
            for(const char* p = line + used + used2; *p;)
            {
                char* end = nullptr;
                const long r = strtol(p, &end, 10);
                if(end == p)
                {
                    break;
                }
                p = end;
                if(r >= 0 && r < 16)
                {
                    s.rows[r] = static_cast<unsigned char>(klass);
                }
            }
            continue;
        }
        char klassName[32];
        int w = 0, h = 0;
        if(!q_strcasecmp(what, "paint") && sscanf(line + used, "%31s %d %d %31s%n", c, &w, &h, klassName, &used2) == 4)
        {
            const int key = nameIndex(c, keyNames, numKeys);
            const int klass = nameIndex(klassName, classNames, 4);
            if(key < 0 || key >= numKeys || klass <= 0 || w <= 0 || h <= 0 || w * h > bufferWords * 8)
            {
                Con_Warning("%s:%d: paint <model> <skin> <key> <w> <h> <clothes|pack|never> <texel ranges>...\n", masksName(), lineNo);
                continue;
            }
            Skin& s = addSkin(a, atoi(b), key);
            if(s.paint.empty() || s.w != w || s.h != h)
            {
                s.w = w;
                s.h = h;
                s.paint.clear();
                s.paint.resize(static_cast<za::SizeT>(w * h), 0);
            }
            readRanges(line + used + used2, s, static_cast<unsigned char>(klass));
            continue;
        }
        if(!q_strcasecmp(what, "flesh")) // flesh <row>...: the flesh's rows
        {
            if(!fleshRead)
            {
                ZA_MEMSET(maskSet.flesh, 0, sizeof(maskSet.flesh));
                fleshRead = true;
            }
            for(const char* p = line + ZA_STRLEN(what) + 1; *p;)
            {
                char* end = nullptr;
                const long r = strtol(p, &end, 10);
                if(end == p)
                {
                    break;
                }
                p = end;
                maskSet.flesh[za::clamp(static_cast<int>(r), 0, 15)] = true;
            }
            continue;
        }
        if(!q_strcasecmp(what, "fleshnear")) // fleshnear <share> <radius>
        {
            maskSet.nearShare = static_cast<float>(atof(a));
            maskSet.nearRadius = atoi(b);
            continue;
        }
        Con_Warning("%s:%d: not a line (rows, skinrows, paint, flesh, fleshnear)\n", masksName(), lineNo);
    }
    free(data);
    Con_DPrintf("humlook: masks from %s: %d skins of our own\n", masksName(), static_cast<int>(skins.size()));
}

void ensureLoaded()
{
    if(!table.loaded)
    {
        loadTable();
    }
    if(!maskSet.loaded)
    {
        loadMasks();
    }
}

// The skin's 8-bit texels from its model's file (an MDL: the skins' block; a group's first picture), or false.
[[nodiscard]] bool readTexels(Skin& s)
{
    s.read = true;
    int size = 0;
    byte* data = COM_LoadMallocFile(s.model.cStr(), nullptr);
    size = data ? com_filesize : 0;
    if(!data)
    {
        return false;
    }
    bool ok = false;
    if(size >= 84 && !ZA_MEMCMP(data, "IDPO", 4))
    {
        const int numskins = LittleLong(*reinterpret_cast<const int*>(data + 48));
        const int w = LittleLong(*reinterpret_cast<const int*>(data + 52));
        const int h = LittleLong(*reinterpret_cast<const int*>(data + 56));
        int p = 84;
        const int want = za::clamp(s.skin, 0, za::max(numskins - 1, 0));
        for(int i = 0; i < numskins && w > 0 && h > 0 && w * h <= bufferWords * 8; i++)
        {
            if(p + 4 > size)
            {
                break;
            }
            const int type = LittleLong(*reinterpret_cast<const int*>(data + p));
            p += 4;
            int pics = 1;
            if(type != 0)
            {
                pics = p + 4 <= size ? LittleLong(*reinterpret_cast<const int*>(data + p)) : 0;
                p += 4 + 4 * za::max(pics, 0);
            }
            if(i == want && pics > 0 && p + w * h <= size)
            {
                if(!s.paint.empty() && (s.w != w || s.h != h))
                {
                    Con_Warning("humlook: %s skin %d is %dx%d, its painted mask %dx%d (dropped)\n", s.model.cStr(), s.skin, w,
                        h, s.w, s.h);
                    s.paint.clear();
                }
                s.w = w;
                s.h = h;
                s.texels.clear();
                s.texels.resize(static_cast<za::SizeT>(w * h), 0);
                ZA_MEMCPY(s.texels.data(), data + p, static_cast<za::SizeT>(w * h));
                if(s.paint.empty())
                {
                    s.paint.resize(static_cast<za::SizeT>(w * h), 0);
                }
                markNearFlesh(s, maskSet);
                ok = true;
                break;
            }
            p += w * h * za::max(pics, 0);
        }
    }
    free(data);
    if(!ok)
    {
        Con_DPrintf("humlook: %s skin %d: no 8-bit skin (not recoloured)\n", s.model.cStr(), s.skin);
    }
    return ok;
}

// The mask's words: each texel's class (painted, else its palette row's), 2 bits each; the Texture Studio's brush cursor
// 3 (the shader shows it yellow).
void build(Skin& s, za::Vector<za::U32>& words)
{
    const int n = s.w * s.h;
    words.clear();
    words.resize(static_cast<za::SizeT>((n + 15) / 16), 0u);
    for(int t = 0; t < n; t++)
    {
        const unsigned klass = classAt(s, t);
        words[static_cast<za::SizeT>(t >> 4)] |= klass << ((t & 15) * 2);
    }
    for(const int t : s.cursor)
    {
        if(t >= 0 && t < n)
        {
            words[static_cast<za::SizeT>(t >> 4)] |= 3u << ((t & 15) * 2);
        }
    }
}

// Its mask in the buffer (written again when changed): its first word, or -1.
[[nodiscard]] int upload(Skin& s)
{
    if(!s.read)
    {
        (void)readTexels(s);
    }
    if(s.texels.empty())
    {
        return -1;
    }
    const int words = (s.w * s.h + 15) / 16;
    if(!buffer)
    {
        buffer = GL_CreateBuffer(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_DRAW, "vr humlook masks",
            sizeof(za::U32) * bufferWords, nullptr);
    }
    if(s.first < 0)
    {
        if(bufferUsed + words > bufferWords) // (full: from the start again, the others written again when next drawn)
        {
            bufferUsed = 0;
            for(Skin& other : skins)
            {
                other.first = -1;
            }
        }
        s.first = bufferUsed;
        bufferUsed += words;
        s.dirty = true;
    }
    if(s.dirty)
    {
        za::Vector<za::U32> bits;
        build(s, bits);
        GL_BindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
        GL_BufferSubDataFunc(GL_SHADER_STORAGE_BUFFER, static_cast<GLintptr>(s.first) * static_cast<GLintptr>(sizeof(za::U32)),
            static_cast<GLsizeiptr>(words) * static_cast<GLsizeiptr>(sizeof(za::U32)), bits.data());
        s.dirty = false;
    }
    return s.first;
}

[[nodiscard]] int ownLook(edict_t* e, int ofs)
{
    return e->free ? 0 : za::clamp(static_cast<int>(progs::fieldFloat(e, ofs)), 0, 65535);
}

// Its look: its own (.vr_hum_look), or a worn backpack's, its wearer's body's with partPack (whenever the pack was made:
// a refit, a deferred spawn). The server's VM current.
[[nodiscard]] unsigned short stateOf(edict_t* e)
{
    const progs::FieldOffsets& f = progs::fields();
    if(e->free || f.vr_hum_look < 0)
    {
        return 0;
    }
    int look = ownLook(e, f.vr_hum_look);
    if(look == 0 && f.vr_wp_wearer >= 0 && f.vr_wp_state >= 0 && progs::fieldFloat(e, f.vr_wp_state) > 0.f)
    {
        const int w = progs::fieldInt(e, f.vr_wp_wearer);
        const int body = w > 0 ? ownLook(PROG_TO_EDICT(w), f.vr_hum_look) : 0;
        look = body > 0 && !(body & partPack) ? body + partPack : 0;
    }
    return static_cast<unsigned short>(look);
}

void writeState(sizebuf_t* msg, int num, unsigned short look)
{
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_HUMLOOK);
    MSG_WriteShort(msg, num);
    MSG_WriteShort(msg, look);
}

void describe(int look)
{
    const int part = (look & partPack) ? 1 : 0;
    const int body = look & (partPack - 1);
    const int kind = body / maxCategories;
    const int cat = body % maxCategories;
    const bool elite = (look & eliteBit) != 0;
    Con_Printf("  look %d: %s %s %s%s", look, kind < static_cast<int>(countof(kindNames)) ? kindNames[kind] : "?",
        cat < static_cast<int>(countof(categoryNames)) ? categoryNames[cat] : "?", partNames[part], elite ? " elite" : "");
    if(elite && !part)
    {
        const Spec* e = kind < maxKinds ? spec(kind, catElite, 0) : nullptr;
        Con_Printf(e && e->set ? ": elite colour %g %g %g strength %g light %g tint %g\n" : ": no elite row (as it is)\n",
            e ? e->rgb[0] : 0.f, e ? e->rgb[1] : 0.f, e ? e->rgb[2] : 0.f, e ? e->strength : 0.f, e ? e->light : 0.f,
            e ? e->tint : 0.f);
        return;
    }
    const Spec* s = kind < maxKinds && cat > 0 ? spec(kind, cat, part) : nullptr;
    if(!s || !s->set)
    {
        Con_Printf(": no row (as it is)\n");
        return;
    }
    Con_Printf(": colour %g %g %g strength %g light %g tint %g\n", s->rgb[0], s->rgb[1], s->rgb[2], s->strength, s->light,
        s->tint);
}

// vr_humlook_info [entity]: the table, the client's looks, and the server's armed humanoid nearest you (or that one).
void info_f()
{
    ensureLoaded();
    int elites = 0; // (the client's elites' bodies: the MP test reads it)
    for(const unsigned short l : looks)
    {
        elites += (l & eliteBit) && !(l & partPack) ? 1 : 0;
    }
    Con_Printf("humlook: %d rows from %s, %d skins' masks, %d looks known by the client, %d elites, vr_humlook %g\n",
        table.rows, tableName(), static_cast<int>(skins.size()), numLooks, elites, vr_humlook.value);
    // The client's elites as they are drawn: each one's instance's clothes row (the elite dye, pulsing), a remote client's too.
    for(int i = 0; i < static_cast<int>(looks.size()) && cl_entities && i < cl_max_edicts; i++)
    {
        const int look = looks[static_cast<za::SizeT>(i)];
        if((look & eliteBit) && !(look & partPack) && cl_entities[i].model)
        {
            float rows[4][4];
            instanceRows(&cl_entities[i], rows);
            float glow[4];
            eliteGlow(&cl_entities[i], glow);
            Con_Printf("  client elite %d (%s, look %d): clothes %.2f %.2f %.2f strength %.2f light %.2f, mask %s, marker %.2f\n",
                i, cl_entities[i].model->name, look, rows[0][0], rows[0][1], rows[0][2], rows[0][3], rows[2][0],
                rows[3][0] > 0.f ? "yes" : "no", glow[0]);
        }
    }
    for(const Skin& s : skins)
    {
        int count[4] = {};
        for(int t = 0; t < s.w * s.h && !s.texels.empty(); t++)
        {
            count[classAt(s, t)]++;
        }
        Con_Printf("  mask %s skin %d (%s): %dx%d, %s, clothes %d pack %d texels\n", s.model.cStr(), s.skin, keyNames[s.key],
            s.w, s.h, s.texels.empty() ? "not read" : s.ownRows ? "its own rows" : "its kind's rows", count[1], count[2]);
    }
    if(!sv.active || progs::fields().vr_hum_look < 0)
    {
        return;
    }
    const progs::ServerVm vm;
    edict_t* player = progs::playerEdict();
    int best = Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : -1;
    if(best < 0 && player)
    {
        float bestD = 1e30f;
        for(int i = svs.maxclients + 1; i < sv.qcvm.num_edicts; i++)
        {
            edict_t* e = EDICT_NUM(i);
            if(stateOf(e) == 0 || (stateOf(e) & partPack))
            {
                continue;
            }
            vec3_t d;
            VectorSubtract(e->v.origin, player->v.origin, d);
            const float dist = VectorLength(d);
            if(dist < bestD)
            {
                bestD = dist;
                best = i;
            }
        }
    }
    edict_t* e = progs::edictAt(best);
    if(!e)
    {
        Con_Printf("  no armed humanoid with a look\n");
        return;
    }
    Con_Printf("  entity %d (%s), the client's look %d:\n", best, PR_GetString(e->v.classname), lookOf(best));
    describe(stateOf(e));
}

// The shader's HumDye (vr_glsl_alias.h) on the CPU: `c` dyed by the row `col` (rgb, strength) at light/tint `lt`.
void humDye(const float c[3], const float col[4], const float lt[2], float out[3])
{
    const auto lum = [](const float* v) { return v[0] * 0.299f + v[1] * 0.587f + v[2] * 0.114f; };
    const float l = lum(c) * lt[0];
    const float colLum = za::max(lum(col), 0.02f);
    const float cm = za::max(za::max(col[0], col[1]), za::max(col[2], 0.02f));
    for(int i = 0; i < 3; i++)
    {
        const float dyed = za::min(col[i] * (l / colLum), 1.f);
        const float tinted = za::min(c[i] * lt[0] * col[i] / cm, 1.f);
        const float d = dyed + (tinted - dyed) * lt[1];
        out[i] = c[i] + (d - c[i]) * col[3];
    }
}

// vr_humlook_dyetest [n]: each of the client's elites' clothes as drawn (its skin texture's masked texels, read back,
// through its instance's rows, the shader's dye on the CPU) against its stock skin: the mean colour moved, towards gold (red over blue). Ends on
// `humlook dyetest: P passed, F failed` (fewer than n elites: a failure each). ROUND26.md, "elitedye".
void dyeTest_f()
{
    ensureLoaded();
    const int want = Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : 0;
    testkit::Tally tally;
    for(int i = 0; i < static_cast<int>(looks.size()) && cl_entities && i < cl_max_edicts; i++)
    {
        const int look = looks[static_cast<za::SizeT>(i)];
        const entity_t* e = &cl_entities[i];
        if(!(look & eliteBit) || (look & partPack) || !e->model || keyOf(look) < 0)
        {
            continue; // (AD's death guard and crossbow knight: their own look, no dye; the marker alone)
        }
        float rows[4][4];
        instanceRows(e, rows);
        const int kind = (look & (partPack - 1)) / maxCategories;
        Skin* s = skinFor(e->model, e->skinnum, keyOf(look));
        // (the skin as drawn: its texture read back, an external or retro one too; the mask's texel at its place)
        const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e->model));
        const int sk = hdr && e->skinnum >= 0 && e->skinnum < hdr->numskins ? e->skinnum : 0;
        gltexture_t* tex = hdr ? hdr->gltextures[sk][0] : nullptr;
        GLint tw = 0, th = 0;
        za::Vector<unsigned char> px;
        if(tex)
        {
            GL_Bind(GL_TEXTURE0, tex);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tw);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &th);
            px.resize(static_cast<za::SizeT>(za::max(tw, 0) * za::max(th, 0) * 4), 0);
            if(px.empty() || !gfx::readTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data(), false))
            {
                px.clear();
            }
        }
        double before[3] = {}, after[3] = {};
        int n = 0;
        for(int t = 0; s && !px.empty() && rows[3][0] > 0.f && t < s->w * s->h; t++)
        {
            if(classAt(*s, t) != 1u)
            {
                continue;
            }
            const int x = (t % s->w) * tw / s->w, y = (t / s->w) * th / s->h;
            const unsigned char* p = &px[static_cast<za::SizeT>((y * tw + x) * 4)];
            const float c[3] = {p[0] / 255.f, p[1] / 255.f, p[2] / 255.f};
            float d[3];
            humDye(c, rows[0], rows[2], d);
            for(int k = 0; k < 3; k++)
            {
                before[k] += c[k];
                after[k] += d[k];
            }
            n++;
        }
        for(int k = 0; k < 3 && n > 0; k++)
        {
            before[k] /= n;
            after[k] /= n;
        }
        // (by hue, not lightness: the elite rows darken; each colour's channels' shares of its sum. Gold's blue share is
        // 0.1: the dyed clothes' blue share down by 0.05 or more, red over green)
        const double sb = za::max(before[0] + before[1] + before[2], 1e-6), sa = za::max(after[0] + after[1] + after[2], 1e-6);
        double moved = 0.0;
        for(int k = 0; k < 3; k++)
        {
            moved = za::max(moved, za::abs(after[k] / sa - before[k] / sb));
        }
        const double warmer = before[2] / sb - after[2] / sa;
        const bool ok = n > 0 && moved > 0.04 && warmer > 0.05 && after[0] > after[1];
        tally.check(ok);
        Con_Printf("humlook dyetest: %s entity %d (%s %s, look %d): %d clothes texels, mean %.3f %.3f %.3f -> %.3f %.3f %.3f "
                   "(moved %.3f, warmer %.3f)\n",
            ok ? "PASS" : "FAIL", i, kind < static_cast<int>(countof(kindNames)) ? kindNames[kind] : "?", e->model->name, look,
            n, before[0], before[1], before[2], after[0], after[1], after[2], moved, warmer);
    }
    const int drawn = tally.passed + tally.failed;
    tally.check(drawn >= want, "humlook dyetest", "fewer elites drawn than wanted");
    tally.verdict("humlook dyetest");
    Cvar_SetValueQuick(&vr_test_humlook_dye, static_cast<float>(tally.failed)); // (test 234's QC check reads it)
}

// vr_humlook_glowtest [elites] [plates]: the elite marker (vr_elite_glow) as the last frame drew it: each drawn armed
// humanoid's body and each armour plate or joints' leather entity on one (agarmor::sourceOf), its instance's glow
// (eliteGlow) lit on an elite's, dark on an ordinary one's; at least `elites` elite bodies and `plates` plate entities on
// elites drawn. Ends on `humlook glowtest: P passed, F failed` (vr_test_humlook_glow: its failures; test 234 reads it).
void glowTest_f()
{
    const int want = Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : 0;
    const int wantPlates = Cmd_Argc() > 2 ? atoi(Cmd_Argv(2)) : 0;
    testkit::Tally tally;
    int bodies[2] = {}, plates[2] = {}; // (ordinary, elite)
    for(int v = 0; v < cl_numvisedicts && cl_entities; v++)
    {
        const entity_t* e = cl_visedicts[v];
        const bool own = e && e >= cl_entities && e < cl_entities + cl_max_edicts;
        const entity_t* src = own ? e : (e ? agarmor::sourceOf(e) : nullptr);
        const int num = src ? static_cast<int>(src - cl_entities) : -1;
        const int look = num >= 0 ? lookOf(num) : 0;
        if(!e || !e->model || look <= 0 || (look & partPack))
        {
            continue; // (armed humanoids' bodies and what they wear; their packs and the rest left out)
        }
        float g[4];
        eliteGlow(e, g);
        const int elite = (look & eliteBit) ? 1 : 0;
        const bool lit = g[0] > 0.f;
        const bool wanted = elite != 0 && vr_elite_glow.value > 0.f;
        (own ? bodies : plates)[elite]++;
        // (a plate entity's skin: an elite's the gold-trimmed nine, 9..17, an ordinary one's 0..8; the leather's its own 0..2)
        const auto* ahdr = !own && e->model->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(e->model)) : nullptr;
        const bool trimmable = ahdr && ahdr->numskins >= 18 && !ZA_STRSTR(e->model->name, "#bridge");
        const bool trimOk = !trimmable || (e->skinnum >= 9) == (elite != 0 && vr_elite_trim.value != 0.f);
        tally.check(lit == wanted && trimOk);
        if(lit != wanted || !trimOk)
        {
            Con_Printf("humlook glowtest: FAIL entity %d %s (%s skin %d, look %d): glow %.2f %.2f %.2f fill %.2f\n", num,
                own ? "body" : "plates", e->model->name, e->skinnum, look, g[0], g[1], g[2], g[3]);
        }
    }
    Con_Printf("humlook glowtest: bodies %d elite (lit), %d ordinary (dark); plate entities %d on elites, %d on ordinary ones; "
               "vr_elite_glow %g\n", bodies[1], bodies[0], plates[1], plates[0], vr_elite_glow.value);
    tally.check(bodies[1] >= want, "humlook glowtest", "fewer elites drawn than wanted");
    tally.check(plates[1] >= wantPlates, "humlook glowtest", "fewer plates drawn on elites than wanted");
    tally.verdict("humlook glowtest");
    Cvar_SetValueQuick(&vr_test_humlook_glow, static_cast<float>(tally.failed));
}

// Where a file of quakevr/ is written: quakevr/ in the last base folder that has one (as vr_armourdata.cpp's).
[[nodiscard]] const char* writePath(const char* name)
{
    for(int i = com_numbasedirs - 1; i >= 0; i--)
    {
        char dir[MAX_OSPATH];
        q_snprintf(dir, sizeof(dir), "%s/quakevr", com_basedirs[i]);
        if(files::isDirectory(dir) || i == 0)
        {
            q_snprintf(pathText, sizeof(pathText), "%s/%s", dir, name);
            return pathText;
        }
    }
    q_snprintf(pathText, sizeof(pathText), "quakevr/%s", name);
    return pathText;
}

constexpr const char* tableHead =
    "# humlook.txt: the armed humanoids' recolours by the weapon they spawned with (docs/vr-port/ARMED_MONSTERS.md,\n"
    "# \"Recolours by weapon\"; the engine's vr_humlook.cpp, the shader's QVR_ALIAS_FS_RECOLOR). Read at load; vr_humlook_reload\n"
    "# reads it again, vr_humlook_info prints what the nearest armed humanoid wears. The Texture Studio (Debug > Tools) writes it.\n"
    "#\n"
    "# A row: <type> <category> <part> r g b strength light tint\n"
    "#   type      grunt, enforcer, knight, dknight (the armed monsters' rows, QC VR_AM_*)\n"
    "#   category  explosive (rockets, grenades), energy (lightning, plasma, lasers), flame, poison; the others (shells, nails,\n"
    "#             melee, the monster's own gun) have no row: as today; elite: an elite's clothes whatever it holds (its pack\n"
    "#             keeps its weapon's colour, else the elite pack row; QC vr_attrs.qc), its lightness pulsing (vr_elite_pulse)\n"
    "#   part      clothes (its body's masked texels), pack (its worn backpack, and a pack in its body's skin)\n"
    "# Which texels: humlook_masks.txt (their palette rows; a few painted by hand). Faces and flesh are never changed.\n"
    "#   r g b     the colour (0..1) the texels are dyed: each keeps its own lightness (its shading), in that colour\n"
    "#   strength  how much of the dye (0 none, 1 all)\n"
    "#   light     the texels' lightness times this\n"
    "#   tint      0: the dye (the colour at the texel's lightness); 1: the texel's own colour times the colour (keeps more\n"
    "#             of its detail, less of the colour); between: a mix\n"
    "#\n"
    "# type     category  part        r     g     b    strength light  tint\n";

constexpr const char* masksHead =
    "# humlook_masks.txt: which texels of the armed humanoids' skins the recolours dye (humlook.txt; vr_humlook.cpp).\n"
    "# Made from each skin's 8-bit palette rows as the model loads: no texture is changed or copied. The Texture Studio\n"
    "# (Debug > Tools) writes it.\n"
    "#   rows <key> <class> <row>...      a key's palette rows (0..15: index / 16) of that class; the others: none\n"
    "#        key: grunt, enforcer, knight, dknight (their bodies), pack (a worn backpack); class: clothes, pack, none\n"
    "#   skinrows <model> <skin> <key> <class> <row>...   one skin's own rows instead of its key's\n"
    "#   paint <model> <skin> <key> <w> <h> <class> <texels>...   texels painted by hand (y * w + x, ranges a-b): clothes,\n"
    "#        pack, or never (left as it is whatever its row); only on a skin of that size\n"
    "#   flesh <row>...                  the flesh's rows: never dyed (default 7)\n"
    "#   fleshnear <share> <radius>      a dyed row's texel left as it is where this share of the texels within the radius\n"
    "#        are flesh (an arm's or a face's highlights on the uniforms' tans; 0: off)\n"
    "# Their palette rows (scratch analysis, ROUND25 \"humrepl2\"): the uniforms and leather are rows 1 and 10 (browns, tans),\n"
    "# flesh row 7 (peach), blood row 4 (reds): rows keep faces and flesh out where a hue band could not.\n";

} // namespace

const char* tableName()
{
    return vr_humlook_file.string && vr_humlook_file.string[0] ? vr_humlook_file.string : "humlook.txt";
}

const char* masksName()
{
    return vr_humlook_masks_file.string && vr_humlook_masks_file.string[0] ? vr_humlook_masks_file.string
                                                                           : "humlook_masks.txt";
}

Spec* spec(int kind, int cat, int part)
{
    ensureLoaded();
    if(kind < 0 || kind >= maxKinds || cat <= 0 || cat >= maxCategories || part < 0 || part > 1)
    {
        return nullptr;
    }
    return &table.specs[kind][cat][part];
}

unsigned char* keyRows(int key)
{
    ensureLoaded();
    return key >= 0 && key < numKeys ? maskSet.rows[key] : nullptr;
}

unsigned classAt(const Skin& s, int t)
{
    const unsigned char p = t >= 0 && t < static_cast<int>(s.paint.size()) ? s.paint[static_cast<za::SizeT>(t)] : 0;
    if(p != ClassNone)
    {
        return p == ClassNever ? 0u : p;
    }
    if(t < 0 || t >= static_cast<int>(s.texels.size()))
    {
        return 0u;
    }
    const int row = s.texels[static_cast<za::SizeT>(t)] >> 4;
    if(maskSet.flesh[row] || (t < static_cast<int>(s.nearFlesh.size()) && s.nearFlesh[static_cast<za::SizeT>(t)]))
    {
        return 0u; // (flesh, and a dyed row's texel amid it)
    }
    return s.ownRows ? s.rows[row] : maskSet.rows[s.key][row];
}

Skin* skinFor(const qmodel_t* model, int skinnum, int key)
{
    ensureLoaded();
    if(!model || model->type != mod_alias || key < 0 || key >= numKeys)
    {
        return nullptr;
    }
    char name[MAX_QPATH]; // (its file: a derived model's name without its "#rig2" and the like)
    q_strlcpy(name, model->name, sizeof(name));
    char* hash = strchr(name, '#');
    if(hash)
    {
        *hash = '\0';
    }
    Skin& s = addSkin(name, za::max(skinnum, 0), key);
    if(!s.read)
    {
        (void)readTexels(s);
    }
    return s.texels.empty() ? nullptr : &s;
}

int keyOf(int look)
{
    if(look <= 0)
    {
        return -1;
    }
    if(look & partPack)
    {
        return keyPack;
    }
    const int kind = (look & (partPack - 1)) / maxCategories;
    return kind >= 0 && kind < keyPack ? kind : -1;
}

int lookOf(int num)
{
    return num >= 0 && num < static_cast<int>(looks.size()) ? looks[static_cast<za::SizeT>(num)] : 0;
}

za::Vector<Skin>& allSkins()
{
    ensureLoaded();
    return skins;
}

void reload()
{
    loadTable();
    loadMasks();
}

bool saveTable()
{
    ensureLoaded();
    za::String out{tableHead};
    char line[256];
    for(int k = 0; k < static_cast<int>(countof(kindNames)); k++)
    {
        for(int c = 1; c < static_cast<int>(countof(categoryNames)); c++)
        {
            for(int p = 0; p < 2; p++)
            {
                const Spec& s = table.specs[k][c][p];
                if(s.set)
                {
                    q_snprintf(line, sizeof(line), "%-10s %-9s %-8s %5.2f %5.2f %5.2f   %5.2f  %5.2f  %5.2f\n", kindNames[k],
                        categoryNames[c], partNames[p], s.rgb[0], s.rgb[1], s.rgb[2], s.strength, s.light, s.tint);
                    out += line;
                }
            }
        }
    }
    return files::writeText(writePath(tableName()), out.cStr());
}

bool saveMasks()
{
    ensureLoaded();
    za::String out{masksHead};
    char line[512];
    q_strlcpy(line, "flesh", sizeof(line));
    for(int r = 0; r < 16; r++)
    {
        if(maskSet.flesh[r])
        {
            q_strlcat(line, va(" %d", r), sizeof(line));
        }
    }
    out += line;
    out += va("\nfleshnear %g %d\n", maskSet.nearShare, maskSet.nearRadius);
    for(int key = 0; key < numKeys; key++)
    {
        for(int klass = ClassClothes; klass <= ClassPack; klass++)
        {
            q_snprintf(line, sizeof(line), "rows %-8s %-7s", keyNames[key], classNames[klass]);
            bool any = false;
            for(int r = 0; r < 16; r++)
            {
                if(maskSet.rows[key][r] == klass)
                {
                    q_strlcat(line, va(" %d", r), sizeof(line));
                    any = true;
                }
            }
            if(any)
            {
                out += line;
                out += "\n";
            }
        }
    }
    for(const Skin& s : skins)
    {
        if(s.ownRows)
        {
            q_snprintf(line, sizeof(line), "skinrows %s %d %s none\n", s.model.cStr(), s.skin, keyNames[s.key]);
            out += line;
            for(int klass = ClassClothes; klass <= ClassPack; klass++)
            {
                q_snprintf(line, sizeof(line), "skinrows %s %d %s %s", s.model.cStr(), s.skin, keyNames[s.key], classNames[klass]);
                for(int r = 0; r < 16; r++)
                {
                    if(s.rows[r] == klass)
                    {
                        q_strlcat(line, va(" %d", r), sizeof(line));
                    }
                }
                out += line;
                out += "\n";
            }
        }
        for(int klass = ClassClothes; klass <= ClassNever; klass++)
        {
            // (the texels in runs, a line every 24 runs)
            int runs = 0;
            const int n = static_cast<int>(s.paint.size());
            for(int t = 0; t < n;)
            {
                if(s.paint[static_cast<za::SizeT>(t)] != klass)
                {
                    t++;
                    continue;
                }
                int e = t;
                while(e + 1 < n && s.paint[static_cast<za::SizeT>(e + 1)] == klass)
                {
                    e++;
                }
                if(runs % 24 == 0)
                {
                    q_snprintf(line, sizeof(line), "%spaint %s %d %s %d %d %s", runs ? "\n" : "", s.model.cStr(), s.skin,
                        keyNames[s.key], s.w, s.h, classNames[klass]);
                    out += line;
                }
                out += e > t ? va(" %d-%d", t, e) : va(" %d", t);
                runs++;
                t = e + 1;
            }
            if(runs)
            {
                out += "\n";
            }
        }
    }
    return files::writeText(writePath(masksName()), out.cStr());
}

void serverFrame()
{
    if(!sv.active || progs::fields().vr_hum_look < 0)
    {
        return;
    }
    const progs::ServerVm vm; // (a pack's wearer: PROG_TO_EDICT)
    const int n = sv.qcvm.num_edicts;
    if(static_cast<int>(sent.size()) < n)
    {
        sent.resize(static_cast<za::SizeT>(n), 0);
    }
    for(int i = svs.maxclients + 1; i < static_cast<int>(sent.size()); i++)
    {
        const unsigned short now = i < n ? stateOf(EDICT_NUM(i)) : 0;
        unsigned short& was = sent[static_cast<za::SizeT>(i)];
        if(now != was && sv.reliable_datagram.cursize + 8 < sv.reliable_datagram.maxsize)
        {
            writeState(&sv.reliable_datagram, i, now);
            was = now;
        }
    }
}

void serverWriteAll(sizebuf_t* msg)
{
    sent.clear();
    if(!sv.active || progs::fields().vr_hum_look < 0)
    {
        return;
    }
    writeState(msg, -1, 0);
    const progs::ServerVm vm;
    sent.resize(static_cast<za::SizeT>(sv.qcvm.num_edicts), 0);
    for(int i = svs.maxclients + 1; i < sv.qcvm.num_edicts; i++)
    {
        const unsigned short now = stateOf(EDICT_NUM(i));
        sent[static_cast<za::SizeT>(i)] = now;
        if(now && msg->cursize + 8 < msg->maxsize)
        {
            writeState(msg, i, now);
        }
    }
}

void parseEvent()
{
    const int num = MSG_ReadShort();
    const unsigned short look = static_cast<unsigned short>(MSG_ReadShort() & 0xffff);
    if(num < 0)
    {
        looks.clear();
        numLooks = 0;
    }
    else
    {
        if(num >= static_cast<int>(looks.size()))
        {
            looks.resize(static_cast<za::SizeT>(num + 1), 0);
        }
        unsigned short& l = looks[static_cast<za::SizeT>(num)];
        numLooks += (look != 0 ? 1 : 0) - (l != 0 ? 1 : 0);
        l = look;
    }
    if(vr_humlook_known.value != static_cast<float>(numLooks))
    {
        Cvar_SetROM(vr_humlook_known.name, va("%d", numLooks)); // (read-only: the tests read it)
    }
}

// vr_attr_set <attribute> <0|1> [entity]: an enemy attribute (QC vr_attrs.qc's table: infected, elite, or its bit) given
// or taken on that entity, or on the monster nearest the first player (Debug > Tools > Enemy Attributes).
namespace
{
void attrSet_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("vr_attr_set <attribute> <0|1> [entity]: infected, elite (QC vr_attrs.qc), on the nearest monster or that one\n");
        return;
    }
    if(!sv.active)
    {
        Con_Printf("vr_attr_set: no game running here\n");
        return;
    }
    const progs::ServerVm vm;
    const func_t count = progs::findFunction("VR_Attr_UiCount"), name = progs::findFunction("VR_Attr_UiName"),
                 cmd = progs::findFunction("VR_Attr_Cmd");
    if(!count || !name || !cmd)
    {
        Con_Printf("vr_attr_set: not in this game's QC\n");
        return;
    }
    int bit = 0;
    const int n = static_cast<int>(progs::qcFloat(count));
    for(int i = 0; i < n && i < 16 && !bit; i++)
    {
        char text[32];
        progs::qcText(text, sizeof(text), name, static_cast<float>(i));
        bit = !q_strcasecmp(text, Cmd_Argv(1)) ? 1 << i : 0;
    }
    bit = bit ? bit : atoi(Cmd_Argv(1));
    if(bit <= 0)
    {
        Con_Printf("vr_attr_set: no attribute \"%s\"\n", Cmd_Argv(1));
        return;
    }
    edict_t* e = Cmd_Argc() > 3 ? progs::edictAt(atoi(Cmd_Argv(3))) : nullptr;
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(qcvm->edicts);
    G_FLOAT(OFS_PARM0) = static_cast<float>(bit);
    G_FLOAT(OFS_PARM1) = atof(Cmd_Argv(2)) != 0.0 ? 1.f : 0.f;
    G_INT(OFS_PARM2) = e ? EDICT_TO_PROG(e) : 0;
    PR_ExecuteProgram(cmd);
}
} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_attr_set", attrSet_f);
    Cmd_AddCommand("vr_humlook_reload", [] {
        reload();
        Con_Printf("humlook: %d rows from %s, masks from %s\n", table.rows, tableName(), masksName());
    });
    Cmd_AddCommand("vr_humlook_info", info_f);
    Cmd_AddCommand("vr_humlook_dyetest", dyeTest_f);
    Cmd_AddCommand("vr_humlook_glowtest", glowTest_f);
    studioInit();
}

void instanceRows(const entity_t* e, float rows[4][4])
{
    ZA_MEMSET(rows, 0, sizeof(float) * 16);
    if(vr_humlook.value == 0.f || numLooks <= 0 || !cl_entities || e < cl_entities || e >= cl_entities + cl_max_edicts)
    {
        return;
    }
    const int look = lookOf(static_cast<int>(e - cl_entities));
    const int key = keyOf(look);
    if(key < 0)
    {
        return;
    }
    const int part = key == keyPack ? 1 : 0;
    const int body = look & (partPack - 1);
    const bool elite = (look & eliteBit) != 0;
    // (an elite: its clothes the elite row's; its pack its weapon's colour, else the elite row's pack)
    const Spec* clothes = part ? nullptr : spec(body / maxCategories, elite ? catElite : body % maxCategories, 0);
    const Spec* pack = spec(body / maxCategories, body % maxCategories, 1);
    clothes = clothes && clothes->set ? clothes : nullptr;
    pack = pack && pack->set ? pack : nullptr;
    if(elite && !pack)
    {
        pack = spec(body / maxCategories, catElite, 1);
        pack = pack && pack->set ? pack : nullptr;
    }
    const bool overlay = studioOverlay();
    if(!clothes && !pack && !overlay && !studioCursorOn())
    {
        return;
    }
    Skin* s = skinFor(e->model, e->skinnum, key);
    const int first = s ? upload(*s) : -1;
    if(first < 0)
    {
        return;
    }
    if(clothes)
    {
        rows[0][0] = clothes->rgb[0];
        rows[0][1] = clothes->rgb[1];
        rows[0][2] = clothes->rgb[2];
        rows[0][3] = clothes->strength;
        rows[2][0] = clothes->light;
        rows[2][1] = clothes->tint;
        if(elite && vr_elite_pulse.value > 0.f)
        {
            // (the elite's pulse: its dark gold row glows towards bright gold and back, brighter as it does; each one on its
            // own phase by its number. The MP test reads the row at rest: rows[0] is the table's colour at t 0.)
            const float phase = static_cast<float>(cl.time) * vr_elite_pulse_hz.value * 6.2831853f +
                                static_cast<float>(e - cl_entities) * 1.7f;
            const float t = za::clamp(vr_elite_pulse.value, 0.f, 1.f) * (0.5f + 0.5f * za::sin(phase));
            constexpr float gold[3] = {1.f, 0.85f, 0.35f};
            for(int c = 0; c < 3; c++)
            {
                rows[0][c] = clothes->rgb[c] + (gold[c] - clothes->rgb[c]) * t;
            }
            rows[2][0] = clothes->light * (1.f + t);
        }
    }
    if(pack)
    {
        rows[1][0] = pack->rgb[0];
        rows[1][1] = pack->rgb[1];
        rows[1][2] = pack->rgb[2];
        rows[1][3] = pack->strength;
        rows[2][2] = pack->light;
        rows[2][3] = pack->tint;
    }
    rows[3][0] = static_cast<float>(first + 1);
    rows[3][1] = static_cast<float>(s->w);
    rows[3][2] = static_cast<float>(s->h);
    rows[3][3] = overlay ? 1.f : 0.f;
}

bool isElite(int num)
{
    return (lookOf(num) & eliteBit) != 0;
}

void eliteGlow(const entity_t* e, float out[4])
{
    ZA_MEMSET(out, 0, sizeof(float) * 4);
    if(vr_elite_glow.value <= 0.f || !cl_entities || !e)
    {
        return;
    }
    const bool worn = e < cl_entities || e >= cl_entities + cl_max_edicts;
    if(worn)
    {
        e = agarmor::sourceOf(e); // (an armour's plates or its joints' leather: its wearer's)
        if(!e || e < cl_entities || e >= cl_entities + cl_max_edicts)
        {
            return;
        }
    }
    const int num = static_cast<int>(e - cl_entities);
    if(!(lookOf(num) & eliteBit))
    {
        return;
    }
    float strength = za::clamp(vr_elite_glow.value, 0.f, 4.f);
    if(vr_elite_pulse.value > 0.f)
    {
        // (the dye's pulse, on the same phase: brightest as the dye is)
        const float phase = static_cast<float>(cl.time) * vr_elite_pulse_hz.value * 6.2831853f + static_cast<float>(num) * 1.7f;
        strength *= 1.f + za::clamp(vr_elite_pulse.value, 0.f, 1.f) * za::sin(phase);
    }
    constexpr float gold[3] = {1.f, 0.72f, 0.25f};
    for(int c = 0; c < 3; c++)
    {
        out[c] = gold[c] * strength;
    }
    // (the plates: their outline alone, no fill over their faces: the tier's colour stays clearly read under the gold trim)
    out[3] = worn ? 0.f : za::clamp(vr_elite_glow_fill.value, 0.f, 1.f);
}

} // namespace qvr::humlook

extern "C" void VR_HumlookBind(void)
{
    if(qvr::humlook::buffer)
    {
        GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 17, qvr::humlook::buffer, 0, sizeof(za::U32) * qvr::humlook::bufferWords);
    }
}
