// vr_humlook.cpp -- see vr_humlook.hpp.

#include "vr_humlook.hpp"

#include "vr_api.h"
#include "vr_api_render.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace qvr::humlook
{
namespace
{
constexpr const char* fileName = "humlook.txt";
// The table's names: the rows (QC VR_AM_* order) and the categories (QC VR_HUM_CAT_*; 0 none).
constexpr const char* kindNames[] = {"grunt", "enforcer", "knight", "dknight"};
constexpr const char* categoryNames[] = {
    "none", "explosive", "energy", "flame", "poison", "shells", "nails", "melee", "own",
};
constexpr const char* partNames[] = {"clothes", "pack"};

// A row of the table, as the shader takes it (vr_glsl_alias.h QVR_ALIAS_FS_RECOLOR): a (the range's hue, half width,
// feather, least saturation), b (least and most lightness, the hue's turn, the saturation's factor), c (the tint mixed
// in by its amount, the lightness's factor).
struct Spec
{
    bool set{false};
    float a[4]{};
    float b[4]{};
    float c[4]{};
};

struct Table
{
    Spec specs[maxKinds][maxCategories][2];
    bool loaded{false};
    int rows{0};
};
Table table;

za::Vector<unsigned short> sent;  // the server's: per entity, what the clients were last told
za::Vector<unsigned short> looks; // the client's: per entity, its look from the server
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

void load()
{
    table = Table{};
    table.loaded = true;
    char* data = reinterpret_cast<char*>(COM_LoadMallocFile(fileName, nullptr));
    if(!data)
    {
        Con_DPrintf("humlook: no %s (no recolours)\n", fileName);
        return;
    }
    char* line = data;
    int lineNo = 0;
    while(line && *line)
    {
        char* next = strchr(line, '\n');
        if(next)
        {
            *next++ = '\0';
        }
        lineNo++;
        char* hash = strchr(line, '#');
        if(hash)
        {
            *hash = '\0';
        }
        char kind[32], cat[32], part[32];
        float v[13];
        const int got = sscanf(line, "%31s %31s %31s %f %f %f %f %f %f %f %f %f %f %f %f %f", kind, cat, part, &v[0],
            &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8], &v[9], &v[10], &v[11], &v[12]);
        if(got <= 0)
        {
            line = next;
            continue;
        }
        const int k = nameIndex(kind, kindNames, static_cast<int>(countof(kindNames)));
        const int c = nameIndex(cat, categoryNames, static_cast<int>(countof(categoryNames)));
        const int p = nameIndex(part, partNames, static_cast<int>(countof(partNames)));
        if(got != 16 || k < 0 || k >= maxKinds || c <= 0 || c >= maxCategories || p < 0 || p > 1)
        {
            Con_Warning("%s:%d: not a row (<type> <category> <part> and 13 numbers)\n", fileName, lineNo);
            line = next;
            continue;
        }
        Spec& s = table.specs[k][c][p];
        s.set = true;
        const float amount = za::clamp(v[12], 0.f, 1.f);
        const float a[4] = {v[0], za::clamp(v[1], 0.001f, 180.f), za::max(v[2], 0.001f), v[3]};
        const float b[4] = {v[4], v[5], v[6], za::max(v[7], 0.f)};
        const float cc[4] = {1.f + (v[9] - 1.f) * amount, 1.f + (v[10] - 1.f) * amount, 1.f + (v[11] - 1.f) * amount,
            za::max(v[8], 0.f)};
        memcpy(s.a, a, sizeof(a));
        memcpy(s.b, b, sizeof(b));
        memcpy(s.c, cc, sizeof(cc));
        table.rows++;
        line = next;
    }
    free(data);
    Con_DPrintf("humlook: %d rows from %s\n", table.rows, fileName);
}

[[nodiscard]] const Spec* specOf(int look)
{
    if(look <= 0)
    {
        return nullptr;
    }
    if(!table.loaded)
    {
        load();
    }
    const int part = (look & partPack) ? 1 : 0;
    const int body = look & (partPack - 1);
    const int kind = body / maxCategories;
    const int cat = body % maxCategories;
    if(kind < 0 || kind >= maxKinds || cat <= 0)
    {
        return nullptr;
    }
    const Spec& s = table.specs[kind][cat][part];
    return s.set ? &s : nullptr;
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

[[nodiscard]] int lookOf(int num)
{
    return num >= 0 && num < static_cast<int>(looks.size()) ? looks[static_cast<za::SizeT>(num)] : 0;
}

void describe(int look)
{
    const int part = (look & partPack) ? 1 : 0;
    const int body = look & (partPack - 1);
    const int kind = body / maxCategories;
    const int cat = body % maxCategories;
    Con_Printf("  look %d: %s %s %s", look, kind < static_cast<int>(countof(kindNames)) ? kindNames[kind] : "?",
        cat < static_cast<int>(countof(categoryNames)) ? categoryNames[cat] : "?", partNames[part]);
    const Spec* s = specOf(look);
    if(!s)
    {
        Con_Printf(": no row (as it is)\n");
        return;
    }
    Con_Printf(": hue %g +-%g (soft %g) sat >= %g light %g..%g -> turn %g sat x%g light x%g tint %g %g %g\n", s->a[0],
        s->a[1], s->a[2], s->a[3], s->b[0], s->b[1], s->b[2], s->b[3], s->c[3], s->c[0], s->c[1], s->c[2]);
}

// vr_humlook_info [entity]: the table, the client's looks, and the server's armed humanoid nearest you (or that one).
void info_f()
{
    if(!table.loaded)
    {
        load();
    }
    Con_Printf("humlook: %d rows from %s, %d looks known by the client, vr_humlook %g\n", table.rows, fileName, numLooks,
        vr_humlook.value);
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
} // namespace

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

void registerCommands()
{
    Cmd_AddCommand("vr_humlook_reload", [] {
        load();
        Con_Printf("humlook: %d rows from %s\n", table.rows, fileName);
    });
    Cmd_AddCommand("vr_humlook_info", info_f);
}

void instanceRows(const entity_t* e, float rows[3][4])
{
    memset(rows, 0, sizeof(float) * 12);
    if(vr_humlook.value == 0.f || numLooks <= 0 || !cl_entities || e < cl_entities || e >= cl_entities + cl_max_edicts)
    {
        return;
    }
    const Spec* s = specOf(lookOf(static_cast<int>(e - cl_entities)));
    if(!s)
    {
        return;
    }
    memcpy(rows[0], s->a, sizeof(s->a));
    memcpy(rows[1], s->b, sizeof(s->b));
    memcpy(rows[2], s->c, sizeof(s->c));
}

} // namespace qvr::humlook
