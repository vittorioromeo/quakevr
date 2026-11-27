// vr_beastarmour.cpp -- see vr_beastarmour.hpp (the beasts' armour: rigid pieces on their derived rigs' bones).

#include "vr_beastarmour.hpp"

#include "vr_agarmor.hpp"
#include "vr_geom.hpp"
#include "vr_held.hpp"
#include "vr_hitmodel.hpp"
#include "vr_mdldequant.hpp"
#include "vr_progs.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rigpose.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace qvr::beastarmour
{
namespace
{

constexpr int numSlots = agarmor::NumPieces;
constexpr int maxAnchors = 10;     // a piece's bone's vertices fitted (spread over the bone)
constexpr float shotBack = 64.f;   // units: a shot's line from this far before its point (vr_agarmor.cpp shotBack, QC VR_AR_SHOT_BACK)
constexpr float shotOn = 8.f;      // ... to this far past it (past the guns' tolerance) ...
constexpr float shotPastSkin = 1.f; // ... cut this far past where it first crosses the skin
constexpr float pointReach = 2.f;  // units: a point this near a piece's box is on it (pieceAt)
constexpr int maxEnts = 256;       // the pieces' entities drawn a frame at most

struct Slot
{
    bool on{false};
    char bone[16]{};
    char model[MAX_QPATH]{};
    glm::vec3 centre{0.f};
    // Resolved on first use (the monster's model and rig loaded): its bone, the bone's vertices fitted (.mdl order) and their
    // rest places; the piece model's triangles (its own space) and box.
    int boneIndex{-1};
    int numAnchors{0};
    int anchor[maxAnchors]{};
    glm::vec3 rest[maxAnchors]{};
    za::Vector<glm::vec3> tris;
    glm::vec3 lo{0.f}, hi{0.f};
};

struct Kind
{
    char model[MAX_QPATH]{};
    int group{0};
    int mask{0};
    Slot slot[numSlots];
    bool resolved{false};
    bool ok{false};
};

struct Placed
{
    glm::vec3 origin{0.f};
    glm::mat3 axes{1.f};
    float scale{1.f};
};

za::Vector<Kind> kinds; // (reserved once, when read: the models' names handed to QC stay where they are)
bool loaded = false;
entity_t ents[maxEnts];
int numEnts = 0;

void load()
{
    loaded = true;
    kinds.clear();
    char* text = reinterpret_cast<char*>(COM_LoadMallocFile("data/beastarmour.txt", nullptr));
    if(!text)
    {
        Con_DPrintf("beastarmour: no data/beastarmour.txt\n");
        return;
    }
    za::Vector<Kind> fresh;
    const char* p = text;
    while(p && *p)
    {
        char line[512];
        int n = 0;
        while(*p && *p != '\n' && n < static_cast<int>(sizeof(line)) - 1)
        {
            line[n++] = *p++;
        }
        line[n] = 0;
        p += *p == '\n' ? 1 : 0;
        char word[16], model[MAX_QPATH], bone[16], piece[MAX_QPATH];
        int a = 0;
        float x = 0.f, y = 0.f, z = 0.f;
        if(sscanf(line, "%15s", word) != 1 || word[0] == '/')
        {
            continue;
        }
        if(!strcmp(word, "kind") && sscanf(line, "%*s %63s %d", model, &a) == 2)
        {
            Kind k;
            q_strlcpy(k.model, model, sizeof(k.model));
            k.group = a;
            fresh.pushBack(k);
        }
        else if(!strcmp(word, "piece") && sscanf(line, "%*s %63s %d %15s %63s %f %f %f", model, &a, bone, piece, &x, &y, &z) == 7 &&
                !fresh.empty() && !strcmp(fresh.back().model, model) && a >= 0 && a < numSlots)
        {
            Slot& s = fresh.back().slot[a];
            s.on = true;
            q_strlcpy(s.bone, bone, sizeof(s.bone));
            q_strlcpy(s.model, piece, sizeof(s.model));
            s.centre = glm::vec3{x, y, z};
            fresh.back().mask |= 1 << a;
        }
    }
    free(text);
    kinds.reserve(fresh.size());
    for(const Kind& k : fresh)
    {
        kinds.pushBack(k);
    }
}

// The kind of model `name` (a suffix "#..." of a rig's copy ignored); nullptr none.
Kind* kindOf(const char* name)
{
    if(!loaded)
    {
        load();
    }
    if(!name || !name[0])
    {
        return nullptr;
    }
    const char* hash = strchr(name, '#');
    const za::SizeT len = hash ? static_cast<za::SizeT>(hash - name) : ZA_STRLEN(name);
    for(Kind& k : kinds)
    {
        if(ZA_STRLEN(k.model) == len && !strncmp(k.model, name, len))
        {
            return &k;
        }
    }
    return nullptr;
}

Kind* kindOf(edict_t* ent)
{
    return ent && !ent->free && ent->v.model ? kindOf(PR_GetString(ent->v.model)) : nullptr;
}

// A piece model's triangles in its own space and its box.
void readPiece(Slot& s)
{
    qmodel_t* m = Mod_ForName(s.model, false);
    if(!m || m->type != mod_alias)
    {
        return;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(m));
    if(!hdr || hdr->numverts_vbo <= 0 || hdr->numindexes < 3)
    {
        return;
    }
    mdldequant::prepare(m);
    const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
    s.tris.clear();
    s.lo = glm::vec3{1e9f};
    s.hi = glm::vec3{-1e9f};
    for(int i = 0; i < hdr->numindexes; i++)
    {
        float xyz[3];
        signed char nrm[3];
        mdldequant::restVertex(hdr, za::clamp(static_cast<int>(idx[i]), 0, hdr->numverts_vbo - 1), xyz, nrm);
        const glm::vec3 v = geom::vec(xyz);
        s.tris.pushBack(v);
        s.lo = glm::min(s.lo, v);
        s.hi = glm::max(s.hi, v);
    }
}

// The monster's rig and rest pose: each piece's bone and the vertices it is fitted by, each piece model's triangles.
bool resolve(Kind& k)
{
    if(k.resolved)
    {
        return k.ok;
    }
    k.resolved = true;
    qmodel_t* m = Mod_ForName(k.model, false);
    const ragdoll::Rig* rig = m && m->type == mod_alias ? ragdoll::derivedRigFor(m) : nullptr;
    const auto* hdr = rig ? static_cast<const aliashdr_t*>(Mod_Extradata(m)) : nullptr;
    if(!hdr || hdr->numverts <= 0 || rig->numVerts != hdr->numverts)
    {
        Con_DPrintf("beastarmour: %s: no rig\n", k.model);
        return false;
    }
    mdldequant::prepare(m);
    const int nm = hdr->numverts;
    za::Vector<glm::vec3> rest(static_cast<za::SizeT>(nm), glm::vec3{0.f});
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
    for(int v = 0; v < hdr->numverts_vbo; v++)
    {
        float xyz[3];
        signed char nrm[3];
        mdldequant::restVertex(hdr, v, xyz, nrm);
        rest[static_cast<za::SizeT>(za::clamp(static_cast<int>(desc[v].vertindex), 0, nm - 1))] = geom::vec(xyz);
    }
    for(Slot& s : k.slot)
    {
        if(!s.on)
        {
            continue;
        }
        for(int b = 0; b < rig->numBones; b++)
        {
            s.boneIndex = !strcmp(rig->bones[b].name, s.bone) ? b : s.boneIndex;
        }
        if(s.boneIndex < 0)
        {
            Con_Printf("beastarmour: %s: no bone \"%s\"\n", k.model, s.bone);
            s.on = false;
            k.mask &= ~(1 << static_cast<int>(&s - k.slot));
            continue;
        }
        // The bone's vertices spread over it: the one nearest the piece's middle first, then each the farthest from those.
        za::Vector<int> mine;
        for(int v = 0; v < nm; v++)
        {
            if(rig->vertBone[static_cast<za::SizeT>(v)] == s.boneIndex)
            {
                mine.pushBack(v);
            }
        }
        if(mine.size() < 3)
        {
            s.on = false;
            continue;
        }
        int first = mine[0];
        float best = 1e9f;
        for(const int v : mine)
        {
            const float d = glm::distance(rest[static_cast<za::SizeT>(v)], s.centre);
            first = d < best ? v : first;
            best = d < best ? d : best;
        }
        s.numAnchors = 0;
        s.anchor[s.numAnchors++] = first;
        while(s.numAnchors < maxAnchors && s.numAnchors < static_cast<int>(mine.size()))
        {
            int far = -1;
            float farD = -1.f;
            for(const int v : mine)
            {
                float d = 1e9f;
                for(int i = 0; i < s.numAnchors; i++)
                {
                    d = za::min(d, glm::distance(rest[static_cast<za::SizeT>(v)], rest[static_cast<za::SizeT>(s.anchor[i])]));
                }
                far = d > farD ? v : far;
                farD = d > farD ? d : farD;
            }
            if(far < 0 || farD < 0.25f)
            {
                break;
            }
            s.anchor[s.numAnchors++] = far;
        }
        for(int i = 0; i < s.numAnchors; i++)
        {
            s.rest[i] = rest[static_cast<za::SizeT>(s.anchor[i])];
        }
        readPiece(s);
    }
    k.ok = true;
    return true;
}

Kind* ready(edict_t* ent)
{
    Kind* k = kindOf(ent);
    return k && resolve(*k) ? k : nullptr;
}

// Where piece `s` of `ent` is now: its bone's vertices as drawn, the rigid motion (and the body's scale) from their rest
// places; not drawn (no model to pose), its rest place where the entity stands.
bool place(edict_t* ent, const Slot& s, Placed& out)
{
    if(!s.on || s.numAnchors < 3)
    {
        return false;
    }
    glm::vec3 posed[maxAnchors];
    if(!hitmodel::drawnVertexPoints(ent, s.anchor, s.numAnchors, posed))
    {
        const glm::mat3 axes = held::axesFromAngles(ent->v.angles, false);
        out.axes = axes;
        out.origin = geom::vec(ent->v.origin) + axes * s.centre;
        out.scale = 1.f;
        return true;
    }
    glm::vec3 restMid{0.f}, posedMid{0.f};
    for(int i = 0; i < s.numAnchors; i++)
    {
        restMid += s.rest[i];
        posedMid += posed[i];
    }
    restMid /= static_cast<float>(s.numAnchors);
    posedMid /= static_cast<float>(s.numAnchors);
    float spreadRest = 0.f, spreadPosed = 0.f;
    for(int i = 0; i < s.numAnchors; i++)
    {
        spreadRest += glm::dot(s.rest[i] - restMid, s.rest[i] - restMid);
        spreadPosed += glm::dot(posed[i] - posedMid, posed[i] - posedMid);
    }
    const float k = spreadRest > 1e-4f ? za::clamp(za::sqrt(spreadPosed / spreadRest), 0.2f, 5.f) : 1.f;
    glm::vec3 scaled[maxAnchors];
    for(int i = 0; i < s.numAnchors; i++)
    {
        scaled[i] = s.rest[i] * k;
    }
    const rigpose::Rigid r = rigpose::fitRigid(scaled, posed, s.numAnchors);
    out.axes = glm::mat3_cast(r.rot);
    out.origin = rigpose::apply(r, s.centre * k);
    out.scale = k;
    return true;
}

// The first of segment a..b's crossings of piece `s` placed `pl` (0..1), or a value over 1 for none.
float crossing(const Slot& s, const Placed& pl, const glm::vec3& a, const glm::vec3& b)
{
    const glm::mat3 inv = glm::transpose(pl.axes);
    const glm::vec3 la = inv * (a - pl.origin) / pl.scale;
    const glm::vec3 lb = inv * (b - pl.origin) / pl.scale;
    const glm::vec3 d = lb - la;
    // (the box first: most shots miss most pieces)
    glm::vec3 t0 = (s.lo - glm::vec3{0.5f} - la) / d;
    glm::vec3 t1 = (s.hi + glm::vec3{0.5f} - la) / d;
    const glm::vec3 tmin = glm::min(t0, t1), tmax = glm::max(t0, t1);
    const float enter = za::max(za::max(tmin.x, tmin.y), za::max(tmin.z, 0.f));
    const float leave = za::min(za::min(tmax.x, tmax.y), za::min(tmax.z, 1.f));
    if(!(enter <= leave))
    {
        return 2.f;
    }
    float best = 2.f;
    for(za::SizeT i = 0; i + 2 < s.tris.size(); i += 3)
    {
        // (Moller-Trumbore, both sides)
        const glm::vec3 e1 = s.tris[i + 1] - s.tris[i], e2 = s.tris[i + 2] - s.tris[i];
        const glm::vec3 pv = glm::cross(d, e2);
        const float det = glm::dot(e1, pv);
        if(det > -1e-8f && det < 1e-8f)
        {
            continue;
        }
        const float inv_det = 1.f / det;
        const glm::vec3 tv = la - s.tris[i];
        const float u = glm::dot(tv, pv) * inv_det;
        if(u < 0.f || u > 1.f)
        {
            continue;
        }
        const glm::vec3 qv = glm::cross(tv, e1);
        const float v = glm::dot(d, qv) * inv_det;
        if(v < 0.f || u + v > 1.f)
        {
            continue;
        }
        const float t = glm::dot(e2, qv) * inv_det;
        best = t >= 0.f && t <= 1.f && t < best ? t : best;
    }
    return best;
}

void info_f()
{
    if(!loaded)
    {
        load();
    }
    for(Kind& k : kinds)
    {
        resolve(k);
        int n = 0;
        for(const Slot& s : k.slot)
        {
            n += s.on ? 1 : 0;
        }
        Con_Printf("beastarmour: %s: %d pieces (slots %d, cuirass group %d)%s\n", k.model, n, k.mask, k.group, k.ok ? "" : " NO RIG");
        for(int i = 0; i < numSlots; i++)
        {
            const Slot& s = k.slot[i];
            if(s.on)
            {
                Con_Printf("  slot %d: %s on %s (bone %d, %d anchors, %d triangles)\n", i, s.model, s.bone, s.boneIndex,
                    s.numAnchors, static_cast<int>(s.tris.size() / 3));
            }
        }
    }
}

} // namespace

bool wears(edict_t* ent)
{
    return kindOf(ent) != nullptr;
}

bool kindNamed(const char* model)
{
    return kindOf(model) != nullptr;
}

int slots(edict_t* ent)
{
    const Kind* k = ready(ent);
    return k ? k->mask : 0;
}

int cuirassGroup(edict_t* ent)
{
    const Kind* k = ready(ent);
    return k ? k->group & k->mask : 0;
}

const char* pieceModel(edict_t* ent, int slot)
{
    const Kind* k = kindOf(ent);
    return k && slot >= 0 && slot < numSlots && k->slot[slot].on ? k->slot[slot].model : "";
}

int shotPiece(edict_t* ent, const float* at, const float* dir, int mask)
{
    const Kind* k = ready(ent);
    const glm::vec3 d = geom::safeNormalize(geom::vec(dir), glm::vec3{0.f}, 1e-6f);
    if(!k)
    {
        return -2;
    }
    if(!(mask & k->mask) || glm::dot(d, d) < 0.5f)
    {
        return mask ? pieceAt(ent, at) : -1;
    }
    // The shot's line from well before the body to where it first crosses the skin: a piece on that stretch is what it
    // met first (one beyond is on the far side).
    const glm::vec3 p = geom::vec(at);
    const glm::vec3 a = p - d * shotBack;
    const glm::vec3 b = p + d * (hitmodel::tolerance(hitmodel::Class::Guns) + shotOn);
    const float skin = hitmodel::skinCrossing(ent, a, b);
    const glm::vec3 end = (skin >= 0.f ? a + (b - a) * skin : b) + d * shotPastSkin;
    int found = -1;
    float best = 1.5f;
    for(int i = 0; i < numSlots; i++)
    {
        Placed pl;
        if(!(mask & (1 << i)) || !place(ent, k->slot[i], pl))
        {
            continue;
        }
        const float t = crossing(k->slot[i], pl, a, end);
        found = t <= 1.f && t < best ? i : found;
        best = t <= 1.f && t < best ? t : best;
    }
    return found;
}

int pieceAt(edict_t* ent, const float* at)
{
    const Kind* k = ready(ent);
    if(!k)
    {
        return -2;
    }
    const glm::vec3 p = geom::vec(at);
    int found = -1;
    float best = 1e9f;
    for(int i = 0; i < numSlots; i++)
    {
        Placed pl;
        const Slot& s = k->slot[i];
        if(!place(ent, s, pl))
        {
            continue;
        }
        // (the point's distance from the piece's box, in its own space)
        const glm::vec3 lp = glm::transpose(pl.axes) * (p - pl.origin) / pl.scale;
        const glm::vec3 out = glm::max(glm::max(s.lo - lp, lp - s.hi), glm::vec3{0.f});
        const float dist = glm::length(out) * pl.scale;
        found = dist <= pointReach && dist < best ? i : found;
        best = dist <= pointReach && dist < best ? dist : best;
    }
    return found;
}

uint32_t pieceBones(edict_t* ent, int slot)
{
    const Kind* k = ready(ent);
    return k && slot >= 0 && slot < numSlots && k->slot[slot].on && k->slot[slot].boneIndex < 32 ?
        1u << static_cast<unsigned>(k->slot[slot].boneIndex) : 0u;
}

void plateWhere(edict_t* ent, int slot, int what, float* out)
{
    out[0] = out[1] = out[2] = 0.f;
    const Kind* k = ready(ent);
    Placed pl;
    if(!k || slot < 0 || slot >= numSlots || !place(ent, k->slot[slot], pl))
    {
        return;
    }
    if(what == 1)
    {
        held::anglesFromAxes(pl.axes, out, false);
        return;
    }
    glm::vec3 v = pl.origin;
    if(what == 5)
    {
        // (its outward way: from the body's middle through the piece)
        const glm::vec3 mid = (geom::vec(ent->v.absmin) + geom::vec(ent->v.absmax)) * 0.5f;
        v = geom::safeNormalize(pl.origin - mid, glm::vec3{0.f, 0.f, 1.f}, 1e-6f);
    }
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

void beginRelink()
{
    numEnts = 0;
}

void relinkEntity(const entity_t* e, int num, int type, int pieces, int wear)
{
    // (a listen server's view: the server's body as drawn, the same entity number)
    if(!sv.active || num <= 0 || num >= sv.qcvm.num_edicts || type <= 0 || !pieces)
    {
        return;
    }
    const progs::ServerVm vm;
    edict_t* ent = EDICT_NUM(num);
    Kind* k = ready(ent);
    if(!k || !e->model || kindOf(e->model->name) != k)
    {
        return;
    }
    for(int i = 0; i < numSlots && numEnts < maxEnts && cl_numvisedicts < MAX_VISEDICTS; i++)
    {
        const Slot& s = k->slot[i];
        Placed pl;
        if(!(pieces & (1 << i)) || !place(ent, s, pl))
        {
            continue;
        }
        qmodel_t* m = Mod_ForName(s.model, false);
        if(!m)
        {
            continue;
        }
        const int level = (wear >> numSlots) & (1 << i) ? 2 : (wear & (1 << i)) ? 1 : 0;
        const int numSkins = m->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(m))->numskins : 1;
        entity_t& a = ents[numEnts++];
        a = *e;
        a.model = m;
        a.frame = 0;
        a.previouspose = a.currentpose = 0;
        a.lerpflags = LERP_RESETANIM | LERP_RESETMOVE;
        a.skinnum = numSkins >= 9 ? type - 1 + 3 * level : za::min(type - 1, numSkins - 1);
        a.scale = static_cast<byte>(za::clamp(static_cast<int>(pl.scale * ENTSCALE_DEFAULT + 0.5f), 1, 255));
        for(int c = 0; c < 3; c++)
        {
            a.origin[c] = pl.origin[c];
        }
        held::anglesFromAxes(pl.axes, a.angles, false);
        cl_visedicts[cl_numvisedicts++] = &a;
    }
}

int drawnCount()
{
    return numEnts;
}

void init()
{
    Cmd_AddCommand("vr_beastarmour_info", info_f);
}

} // namespace qvr::beastarmour
