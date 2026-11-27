// vr_beastarmour.cpp -- see vr_beastarmour.hpp (the beasts' armour: rigid pieces on their derived rigs' bones, their tweaks
// from armour.txt and the leather junctions between them).

#include "vr_beastarmour.hpp"

#include "vr_agarmor.hpp"
#include "vr_agarmor_internal.hpp"
#include "vr_armourdata.hpp"
#include "vr_cvars.hpp"
#include "vr_geom.hpp"
#include "vr_held.hpp"
#include "vr_hitmodel.hpp"
#include "vr_lines.hpp"
#include "vr_mdldequant.hpp"
#include "vr_progs.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rigpose.hpp"
#include "vr_strings.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace qvr::beastarmour
{
namespace
{

using armourdata::Override;

constexpr int numSlots = agarmor::NumPieces;
constexpr int maxAnchors = 10;     // a piece's bone's vertices fitted (spread over the bone)
constexpr float shotBack = 64.f;   // units: a shot's line from this far before its point (vr_agarmor.cpp shotBack, QC VR_AR_SHOT_BACK)
constexpr float shotOn = 8.f;      // ... to this far past it (past the guns' tolerance) ...
constexpr float shotPastSkin = 1.f; // ... cut this far past where it first crosses the skin
constexpr float pointReach = 2.f;  // units: a point this near a piece's box is on it (pieceAt)
constexpr int maxEnts = 256;       // the pieces' entities drawn a frame at most
// The leather junctions (each piece's to the piece it joins: joinsOf):
constexpr int junctionCols = 12;     // its columns round the line from one piece to the other (a column where both pieces have an edge)
constexpr float rimBand = 0.3f;      // a piece's edge facing the other: its vertices this far (of its length along the line) from its end
constexpr float rimTuckIn = 0.35f;   // units: the leather's ends this far in from the plates' edges ...
constexpr float rimTuckBack = 0.6f;  // ... and this far back under them
constexpr float junctionEnvelope = 0.35f; // its width, thickness and folds at full this far in from each end (of its length)
constexpr float skinFitGap = 0.25f;  // units off the skin: a junction's end where its piece isn't worn (broken off)
constexpr float skinFitLeast = 0.4f; // ... its end ring shrunk to at least this share of its size for it
constexpr int maxJunctions = numSlots;
constexpr int maxLeatherBones = 2 * maxJunctions; // (a bone each end: the piece's place, or its skin fit)
constexpr int maxLeather = 64;       // the junctions' entities drawn a frame at most (one a beast)
constexpr const char* tweakSuffix = "#tweak";   // "<piece model>#tweak": the piece scaled, stood off, tapered (makeModel)
constexpr const char* leatherSuffix = "#bridge"; // "<beast model>#bridge": its junctions (drawn from both sides: VR_AliasTwoSided)
constexpr const char* leatherSkins = "progs/vragarmor.mdl"; // (the humanoids' atlas: its leather block, vr_agarmor_bridge.cpp)

struct Slot
{
    bool on{false};
    char bone[16]{};
    char model[MAX_QPATH]{};
    glm::vec3 centre{0.f};
    // Resolved on first use (the monster's model and rig loaded): its bone, the bone's vertices fitted (.mdl order) and their
    // rest places; the piece model's triangles (its own space: as made, and with armour.txt's shape tweaks) and box.
    int boneIndex{-1};
    int numAnchors{0};
    int anchor[maxAnchors]{};
    glm::vec3 rest[maxAnchors]{};
    za::Vector<glm::vec3> made; // (as the model has them)
    za::Vector<glm::vec3> tris; // (tweaked: scale, stand-off, taper)
    glm::vec3 lo{0.f}, hi{0.f};
    glm::vec3 along{1.f, 0.f, 0.f}; // its bone's way in the rest pose (the taper's)
    float alongLo{0.f}, alongHi{1.f};
    // armour.txt's (refresh): the piece's, its junction's; whether its shape is tweaked (drawn as "<model>#tweak")
    Override tw, jt;
    bool shaped{false};
    char drawn[MAX_QPATH]{};
    qmodel_t* drawnModel{nullptr}; // (the tweaked model as last drawn: given back when the tweaks change)
    int joins{-1}; // the piece its junction joins (-1 none)
};

// A junction's leather, as made in the rest pose (its piece's and the joined piece's tweaks on them).
struct Junction
{
    int child{-1}, parent{-1};
    int first{0}, numVerts{0};
    int firstIndex{0}, numIndexes{0};
    glm::vec3 endMid[2]{}; // its end rings' middles (the child's, the parent's)
    float skinFit[2]{1.f, 1.f}; // ... shrunk this much round them to sit on the skin
};

struct Kind
{
    char model[MAX_QPATH]{};
    int group{0};
    int mask{0};
    Slot slot[numSlots];
    bool resolved{false};
    bool ok{false};
    za::Vector<glm::vec3> body; // its .mdl's vertices in the rest pose (the skin a broken piece's junction end sits on)
    // armour.txt as applied (refresh)
    int gen{-1};
    uint32_t sig{0xffffffffu};
    // The junctions (made in refresh): their vertices (rest), normals, the share of the parent's end, which junction, the
    // atlas's place; the triangles.
    Junction junction[maxJunctions];
    int numJunctions{0};
    za::Vector<glm::vec3> lp, ln;
    za::Vector<float> lw, lu, lv;
    za::Vector<uint8_t> lj;
    za::Vector<unsigned short> lidx;
    qmodel_t* leather{nullptr}; // ("<model>#bridge" as last drawn)
    char leatherName[MAX_QPATH]{};
};

struct Placed
{
    glm::vec3 origin{0.f};
    glm::mat3 axes{1.f};
    float scale{1.f};
    // The rest pose's places (armour.txt's move and turn on them) carried to now: restRot * x + restT (the junctions' bones)
    glm::mat3 restRot{1.f};
    glm::vec3 restT{0.f};
};

za::Vector<Kind> kinds; // (reserved once, when read: the models' names handed to QC stay where they are)
bool loaded = false;
entity_t ents[maxEnts];
int numEnts = 0;
entity_t leatherEnts[maxLeather];
int leatherBoneCount[maxLeather]{};
float leatherBones[maxLeather][maxLeatherBones * 12]{};
int numLeather = 0;
// The Armour Studio's subject (studioView): its body drawn this see-through, the plates whole.
int studioEnt = 0;
float studioAlpha = 1.f;

[[nodiscard]] bool has(const Override& o, int field)
{
    return ((o.has >> static_cast<unsigned>(field)) & 1u) != 0u;
}

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
            q_strlcpy(s.drawn, piece, sizeof(s.drawn));
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

// armour.txt's shape tweaks on a vertex of piece `s` (its own space: its middle at 0): pushed out from the middle by its
// stand-off, its size round its bone's line tapered from the bone's start to its end, scaled round the middle.
[[nodiscard]] glm::vec3 shaped(const Slot& s, glm::vec3 v)
{
    const Override& o = s.tw;
    if(has(o, armourdata::Standoff))
    {
        v += geom::safeNormalize(v, glm::vec3{0.f}, 1e-6f) * o.v[armourdata::Standoff];
    }
    if(has(o, armourdata::TaperStart) || has(o, armourdata::TaperEnd))
    {
        const float a = glm::dot(v, s.along);
        const float t = za::clamp((a - s.alongLo) / za::max(s.alongHi - s.alongLo, 1e-3f), 0.f, 1.f);
        const float m = za::clamp(o.v[armourdata::TaperStart] + (o.v[armourdata::TaperEnd] - o.v[armourdata::TaperStart]) * t, 0.2f, 3.f);
        v = s.along * a + (v - s.along * a) * m;
    }
    if(has(o, armourdata::Scale))
    {
        v *= za::clamp(o.v[armourdata::Scale], 0.2f, 3.f);
    }
    return v;
}

// A piece model's triangles in its own space (as made) and how far it reaches along its bone.
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
    s.made.clear();
    s.alongLo = 1e9f;
    s.alongHi = -1e9f;
    for(int i = 0; i < hdr->numindexes; i++)
    {
        float xyz[3];
        signed char nrm[3];
        mdldequant::restVertex(hdr, za::clamp(static_cast<int>(idx[i]), 0, hdr->numverts_vbo - 1), xyz, nrm);
        const glm::vec3 v = geom::vec(xyz);
        s.made.pushBack(v);
        s.alongLo = za::min(s.alongLo, glm::dot(v, s.along));
        s.alongHi = za::max(s.alongHi, glm::dot(v, s.along));
    }
}

// Its triangles with armour.txt's shape tweaks, and their box.
void shapeTris(Slot& s)
{
    s.tris.clear();
    s.lo = glm::vec3{1e9f};
    s.hi = glm::vec3{-1e9f};
    for(const glm::vec3& m : s.made)
    {
        const glm::vec3 v = shaped(s, m);
        s.tris.pushBack(v);
        s.lo = glm::min(s.lo, v);
        s.hi = glm::max(s.hi, v);
    }
}

// The rest pose's place of a vertex of piece `s` (its own space, shaped), armour.txt's move and turn on it.
[[nodiscard]] glm::vec3 restOf(const Slot& s, const glm::vec3& v)
{
    const glm::vec3 move{s.tw.v[armourdata::OffsetX], s.tw.v[armourdata::OffsetY], s.tw.v[armourdata::OffsetZ]};
    return s.centre + move + agarmor::detail::tweakTurn(s.tw.v[armourdata::Pitch], s.tw.v[armourdata::Yaw], s.tw.v[armourdata::Roll]) * v;
}

// The closest two pieces come in the rest pose (as made; every few vertices).
[[nodiscard]] float apart(const Slot& a, const Slot& b)
{
    float best = 1e9f;
    const za::SizeT stepA = za::max<za::SizeT>(a.made.size() / 120u, 1u), stepB = za::max<za::SizeT>(b.made.size() / 120u, 1u);
    for(za::SizeT i = 0; i < a.made.size(); i += stepA)
    {
        for(za::SizeT j = 0; j < b.made.size(); j += stepB)
        {
            best = za::min(best, glm::distance(a.centre + a.made[i], b.centre + b.made[j]));
        }
    }
    return best;
}

// Each piece's junction: to the first piece of its own bone (a lower slot: the rottweiler's collar to his barding, the
// fiend's breastplate to his back plate), else to the nearest piece on the first bone up its rig that has one (the
// chanfron to the collar, a bracer to its pauldron, a pauldron to the cuirass). The first piece of the rig's root bone
// joins none.
void joinsOf(Kind& k, const ragdoll::Rig& rig)
{
    for(int i = 0; i < numSlots; i++)
    {
        Slot& s = k.slot[i];
        s.joins = -1;
        if(!s.on || s.boneIndex < 0)
        {
            continue;
        }
        for(int j = 0; j < i && s.joins < 0; j++)
        {
            s.joins = k.slot[j].on && k.slot[j].boneIndex == s.boneIndex ? j : -1;
        }
        for(int b = rig.bones[s.boneIndex].parent, guard = 0; s.joins < 0 && b >= 0 && guard < ragdoll::maxBones; b = rig.bones[b].parent, guard++)
        {
            float best = 1e9f;
            for(int j = 0; j < numSlots; j++)
            {
                const Slot& o = k.slot[j];
                if(o.on && o.boneIndex == b)
                {
                    const float d = apart(s, o);
                    s.joins = d < best ? j : s.joins;
                    best = za::min(best, d);
                }
            }
        }
    }
}

// A junction's edge on one piece: in each column round the line from `from` along `dir`, the piece's outermost vertex near
// its end facing the other piece (`last`: its furthest along `dir`, else its nearest), tucked in under its plates.
void rimOf(const Slot& s, const glm::vec3& from, const glm::vec3& dir, const glm::vec3& e1, const glm::vec3& e2, bool last,
    glm::vec3* rim, bool* found)
{
    float lo = 1e9f, hi = -1e9f;
    for(const glm::vec3& v : s.tris)
    {
        const float t = glm::dot(restOf(s, v) - from, dir);
        lo = za::min(lo, t);
        hi = za::max(hi, t);
    }
    const float band = za::max((hi - lo) * rimBand, 0.5f);
    float reach[junctionCols];
    for(int c = 0; c < junctionCols; c++)
    {
        found[c] = false;
        reach[c] = -1.f;
    }
    for(const glm::vec3& v : s.tris)
    {
        const glm::vec3 p = restOf(s, v);
        const float t = glm::dot(p - from, dir);
        if(last ? t < hi - band : t > lo + band)
        {
            continue;
        }
        const glm::vec3 r = p - from - dir * t;
        const float ang = za::atan2(glm::dot(r, e2), glm::dot(r, e1)) + geom::pi;
        const int c = za::clamp(static_cast<int>(ang / (2.f * geom::pi) * static_cast<float>(junctionCols)), 0, junctionCols - 1);
        const float len = glm::length(r);
        if(len > reach[c])
        {
            reach[c] = len;
            found[c] = true;
            const glm::vec3 in = len > 1e-4f ? r / len : glm::vec3{0.f};
            rim[c] = p - in * za::min(rimTuckIn, len * 0.5f) - dir * (last ? rimTuckBack : -rimTuckBack);
        }
    }
}

// The junctions in the rest pose (each piece worn's to the piece it joins, both tweaked), their leather's vertices.
void buildJunctions(Kind& k)
{
    k.numJunctions = 0;
    k.lp.clear();
    k.ln.clear();
    k.lw.clear();
    k.lu.clear();
    k.lv.clear();
    k.lj.clear();
    k.lidx.clear();
    for(int i = 0; i < numSlots; i++)
    {
        const Slot& s = k.slot[i];
        if(!s.on || s.joins < 0 || !k.slot[s.joins].on || s.tris.empty() || k.slot[s.joins].tris.empty() ||
            (has(s.jt, armourdata::Leather) && s.jt.v[armourdata::Leather] < 0.5f))
        {
            continue;
        }
        const Slot& q = k.slot[s.joins];
        const glm::vec3 a = restOf(s, glm::vec3{0.f}), b = restOf(q, glm::vec3{0.f});
        const glm::vec3 dir = geom::safeNormalize(b - a, glm::vec3{1.f, 0.f, 0.f}, 1e-4f);
        const glm::vec3 ref = za::abs(dir.z) > 0.9f ? glm::vec3{1.f, 0.f, 0.f} : glm::vec3{0.f, 0.f, 1.f};
        const glm::vec3 e1 = glm::normalize(ref - dir * glm::dot(ref, dir)), e2 = glm::cross(dir, e1);
        glm::vec3 rimA[junctionCols], rimB[junctionCols];
        bool inA[junctionCols], inB[junctionCols], col[junctionCols];
        rimOf(s, a, dir, e1, e2, true, rimA, inA);
        rimOf(q, a, dir, e1, e2, false, rimB, inB);
        int cols = 0;
        glm::vec3 midA{0.f}, midB{0.f};
        for(int c = 0; c < junctionCols; c++)
        {
            col[c] = inA[c] && inB[c];
            cols += col[c] ? 1 : 0;
            midA += col[c] ? rimA[c] : glm::vec3{0.f};
            midB += col[c] ? rimB[c] : glm::vec3{0.f};
        }
        if(cols < 2)
        {
            continue;
        }
        midA /= static_cast<float>(cols);
        midB /= static_cast<float>(cols);
        // (a column with no edge on one of them: none, its neighbours' cells not drawn)
        for(int c = 0; c < junctionCols; c++)
        {
            rimA[c] = col[c] ? rimA[c] : midA;
            rimB[c] = col[c] ? rimB[c] : midB;
        }
        const Override& o = s.jt;
        const int folds = za::clamp(static_cast<int>((has(o, armourdata::Folds) ? o.v[armourdata::Folds] : vr_agarmor_folds.value) + 0.5f), 0, 4);
        const float depth = has(o, armourdata::FoldDepth) ? o.v[armourdata::FoldDepth] : vr_agarmor_fold_depth.value;
        const float taperA = has(o, armourdata::TaperStart) ? za::clamp(o.v[armourdata::TaperStart], 0.2f, 3.f) : 1.f;
        const float taperB = has(o, armourdata::TaperEnd) ? za::clamp(o.v[armourdata::TaperEnd], 0.2f, 3.f) : 1.f;
        const float thick = has(o, armourdata::Standoff) ? o.v[armourdata::Standoff] : 0.f;
        const float sag = has(o, armourdata::Sag) ? o.v[armourdata::Sag] : 0.f;
        const int rings = za::clamp(2 * folds + 1, 5, 9);
        Junction& jn = k.junction[k.numJunctions];
        jn = Junction{};
        jn.child = i;
        jn.parent = s.joins;
        jn.first = static_cast<int>(k.lp.size());
        jn.numVerts = rings * (junctionCols + 1);
        jn.firstIndex = static_cast<int>(k.lidx.size());
        jn.endMid[0] = midA;
        jn.endMid[1] = midB;
        for(int r = 0; r < rings; r++)
        {
            const float t = static_cast<float>(r) / static_cast<float>(rings - 1);
            const float env = za::clamp(za::min(t, 1.f - t) / junctionEnvelope, 0.f, 1.f);
            const float width = 1.f + env * (taperA + (taperB - taperA) * t - 1.f);
            const glm::vec3 mid = midA + (midB - midA) * t;
            for(int c = 0; c <= junctionCols; c++)
            {
                const int cc = c % junctionCols;
                glm::vec3 p = rimA[cc] + (rimB[cc] - rimA[cc]) * t;
                const glm::vec3 r0 = p - mid;
                const glm::vec3 out = geom::safeNormalize(r0 - dir * glm::dot(r0, dir), e1, 1e-4f);
                p = mid + r0 * width + out * env * (thick + (folds > 0 && r % 2 == 1 ? depth : 0.f));
                p.z -= sag * 4.f * t * (1.f - t);
                k.lp.pushBack(p);
                k.ln.pushBack(out);
                k.lw.pushBack(t * t * (3.f - 2.f * t));
                k.lj.pushBack(static_cast<uint8_t>(k.numJunctions));
                k.lu.pushBack(static_cast<float>(c) / static_cast<float>(junctionCols));
                k.lv.pushBack(folds > 0 ? t * static_cast<float>(folds) * 0.25f : 1.f - za::abs(2.f * t - 1.f));
            }
        }
        for(int r = 0; r + 1 < rings; r++)
        {
            for(int c = 0; c < junctionCols; c++)
            {
                if(!col[c] || !col[(c + 1) % junctionCols])
                {
                    continue;
                }
                const int v0 = jn.first + r * (junctionCols + 1) + c, v1 = v0 + 1, v2 = v0 + junctionCols + 1, v3 = v2 + 1;
                for(const int x : {v0, v1, v3, v0, v3, v2})
                {
                    k.lidx.pushBack(static_cast<unsigned short>(x));
                }
            }
        }
        jn.numIndexes = static_cast<int>(k.lidx.size()) - jn.firstIndex;
        // Each end's fit to the skin (its piece broken off): its ring shrunk round its middle to the skin's distance from it.
        for(int e = 0; e < 2; e++)
        {
            const glm::vec3* rim = e == 0 ? rimA : rimB;
            float gap = 0.f, radius = 0.f;
            for(int c = 0; c < junctionCols; c++)
            {
                float nearest = 1e9f;
                for(za::SizeT v = 0; col[c] && v < k.body.size(); v++)
                {
                    nearest = za::min(nearest, glm::distance(k.body[v], rim[c]));
                }
                gap += col[c] ? nearest : 0.f;
                radius += col[c] ? glm::distance(rim[c], jn.endMid[e]) : 0.f;
            }
            gap /= static_cast<float>(cols);
            radius /= static_cast<float>(cols);
            jn.skinFit[e] = radius > 1e-3f ? za::clamp(1.f - za::max(gap - skinFitGap, 0.f) / radius, skinFitLeast, 1.f) : 1.f;
        }
        k.numJunctions++;
    }
}

// The models made from these tweaks given back (made again when next drawn).
void forget(Kind& k)
{
    for(Slot& s : k.slot)
    {
        if(s.drawnModel && strings::endsWith(s.drawnModel->name, tweakSuffix) && Cache_Check(&s.drawnModel->cache))
        {
            Cache_Free(&s.drawnModel->cache, false);
        }
        s.drawnModel = nullptr;
    }
    if(k.leather && !ZA_STRCMP(k.leather->name, k.leatherName) && Cache_Check(&k.leather->cache))
    {
        Cache_Free(&k.leather->cache, false);
    }
    k.leather = nullptr;
}

// armour.txt's tweaks for the kind taken again when they changed: each piece's, its shape (its triangles, its model), its
// junction's; the junctions made again.
void refresh(Kind& k)
{
    if(!k.ok || k.gen == armourdata::generation())
    {
        return;
    }
    k.gen = armourdata::generation();
    const uint32_t sig = armourdata::beastSignature(k.model);
    if(sig == k.sig)
    {
        return;
    }
    k.sig = sig;
    for(int i = 0; i < numSlots; i++)
    {
        Slot& s = k.slot[i];
        (void)armourdata::resolveBeast(k.model, i, false, s.tw);
        (void)armourdata::resolveBeast(k.model, i, true, s.jt);
        s.shaped = has(s.tw, armourdata::Scale) || has(s.tw, armourdata::Standoff) || has(s.tw, armourdata::TaperStart) ||
            has(s.tw, armourdata::TaperEnd);
        q_snprintf(s.drawn, sizeof(s.drawn), "%s%s", s.model, s.shaped ? tweakSuffix : "");
        shapeTris(s);
    }
    forget(k);
    buildJunctions(k);
}

// The monster's rig and rest pose: each piece's bone and the vertices it is fitted by, each piece model's triangles, the
// junctions.
bool resolve(Kind& k)
{
    if(k.resolved)
    {
        refresh(k);
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
    k.body = rest;
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
        s.along = geom::safeNormalize(rig->bones[s.boneIndex].end - rig->bones[s.boneIndex].pivot, glm::vec3{1.f, 0.f, 0.f}, 1e-4f);
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
        shapeTris(s);
    }
    joinsOf(k, *rig);
    k.ok = true;
    refresh(k);
    return true;
}

Kind* ready(edict_t* ent)
{
    Kind* k = kindOf(ent);
    return k && resolve(*k) ? k : nullptr;
}

// Where piece `s` of `ent` is now: its bone's vertices as drawn, the rigid motion (and the body's scale) from their rest
// places, armour.txt's move and turn laid on in the rest pose; not drawn (no model to pose), its rest place where the
// entity stands.
bool place(edict_t* ent, const Slot& s, Placed& out)
{
    if(!s.on || s.numAnchors < 3)
    {
        return false;
    }
    const glm::mat3 turn = agarmor::detail::tweakTurn(s.tw.v[armourdata::Pitch], s.tw.v[armourdata::Yaw], s.tw.v[armourdata::Roll]);
    const glm::vec3 mid = restOf(s, glm::vec3{0.f});
    glm::vec3 posed[maxAnchors];
    if(!hitmodel::drawnVertexPoints(ent, s.anchor, s.numAnchors, posed))
    {
        const glm::mat3 axes = held::axesFromAngles(ent->v.angles, false);
        out.axes = axes * turn;
        out.origin = geom::vec(ent->v.origin) + axes * mid;
        out.scale = 1.f;
        out.restRot = axes;
        out.restT = geom::vec(ent->v.origin);
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
    const glm::mat3 rot = glm::mat3_cast(r.rot);
    out.axes = rot * turn;
    out.origin = rigpose::apply(r, mid * k);
    out.scale = k;
    out.restRot = rot * k;
    out.restT = out.origin - out.restRot * mid;
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

// The junctions' bones now (2 a junction: its piece's end, the joined piece's), as 3x4 matrices from the rest pose to the
// world less `base`: each end carried as its piece is placed; a piece not worn (broken off), its end shrunk onto the skin
// there; neither worn, or the junctions off, collapsed (not drawn). Returns the junctions drawn.
int junctionBones(edict_t* ent, const Kind& k, int pieces, const glm::vec3& base, float* out)
{
    ZA_MEMSET(out, 0, sizeof(float) * 12u * static_cast<za::SizeT>(2 * k.numJunctions));
    int drawn = 0;
    for(int j = 0; j < k.numJunctions && vr_beastarmour_leather.value != 0.f; j++)
    {
        const Junction& jn = k.junction[j];
        const bool worn[2] = {((pieces >> jn.child) & 1) != 0, ((pieces >> jn.parent) & 1) != 0};
        Placed pl[2];
        if((!worn[0] && !worn[1]) || !place(ent, k.slot[jn.child], pl[0]) || !place(ent, k.slot[jn.parent], pl[1]))
        {
            continue;
        }
        drawn++;
        for(int e = 0; e < 2; e++)
        {
            // (worn: x -> restRot x + restT; broken off: x -> restRot (m + f (x - m)) + restT)
            const float f = worn[e] ? 1.f : jn.skinFit[e];
            const glm::mat3 R = pl[e].restRot * f;
            const glm::vec3 t = pl[e].restT + pl[e].restRot * (jn.endMid[e] * (1.f - f)) - base;
            float* m = out + (2 * j + e) * 12;
            for(int row = 0; row < 3; row++)
            {
                m[row * 4 + 0] = R[0][row];
                m[row * 4 + 1] = R[1][row];
                m[row * 4 + 2] = R[2][row];
                m[row * 4 + 3] = t[row];
            }
        }
    }
    return drawn;
}

// A junction vertex posed by `bones` (as the skinned path blends them).
[[nodiscard]] glm::vec3 posedLeather(const Kind& k, int v, const float* bones)
{
    const int j = k.lj[static_cast<za::SizeT>(v)];
    const float w = k.lw[static_cast<za::SizeT>(v)];
    const glm::vec3& p = k.lp[static_cast<za::SizeT>(v)];
    glm::vec3 out{0.f};
    for(int e = 0; e < 2; e++)
    {
        const float* m = bones + (2 * j + e) * 12;
        const float share = e == 0 ? 1.f - w : w;
        for(int row = 0; row < 3; row++)
        {
            out[row] += share * (m[row * 4 + 0] * p.x + m[row * 4 + 1] * p.y + m[row * 4 + 2] * p.z + m[row * 4 + 3]);
        }
    }
    return out;
}

// A made model's header, bones and vertices on the hunk (as vr_agarmor_bridge.cpp's makeBridgeModel), its skins `skins`'.
struct Made
{
    int start{0};
    aliashdr_t* hdr{nullptr};
    iqmvert_t* verts{nullptr};
    unsigned short* indexes{nullptr};
};
Made beginModel(const aliashdr_t* skins, int numVerts, int numIndexes, int numBones, const char* frameName)
{
    Made out;
    out.start = Hunk_LowMark();
    auto* hdr = static_cast<aliashdr_t*>(Hunk_Alloc(static_cast<int>(sizeof(aliashdr_t))));
    auto* bones = static_cast<boneinfo_t*>(Hunk_Alloc(static_cast<int>(sizeof(boneinfo_t)) * numBones));
    auto* bind = static_cast<bonepose_t*>(Hunk_Alloc(static_cast<int>(sizeof(bonepose_t)) * numBones));
    auto* poses = static_cast<bonepose_t*>(Hunk_Alloc(static_cast<int>(sizeof(bonepose_t)) * numBones));
    out.verts = static_cast<iqmvert_t*>(Hunk_Alloc(static_cast<int>(sizeof(iqmvert_t)) * numVerts));
    out.indexes = static_cast<unsigned short*>(Hunk_Alloc(static_cast<int>(sizeof(unsigned short)) * numIndexes));
    hdr->ident = skins->ident;
    hdr->version = skins->version;
    hdr->scale[0] = hdr->scale[1] = hdr->scale[2] = 1.f;
    hdr->boundingradius = 64.f;
    hdr->numskins = skins->numskins;
    hdr->skinwidth = skins->skinwidth;
    hdr->skinheight = skins->skinheight;
    hdr->numverts = numVerts;
    hdr->numverts_vbo = numVerts;
    hdr->numtris = numIndexes / 3;
    hdr->numframes = 1;
    hdr->synctype = ST_SYNC;
    hdr->numindexes = numIndexes;
    hdr->numposes = 1;
    hdr->numbones = numBones;
    hdr->poseverttype = aliashdr_t::PV_IQM;
    hdr->nextsurface = 0;
    hdr->frames[0].firstpose = 0;
    hdr->frames[0].numposes = 1;
    hdr->frames[0].interval = 0.1f;
    q_strlcpy(hdr->frames[0].name, frameName, sizeof(hdr->frames[0].name));
    ZA_MEMCPY(hdr->gltextures, skins->gltextures, sizeof(hdr->gltextures));
    ZA_MEMCPY(hdr->fbtextures, skins->fbtextures, sizeof(hdr->fbtextures));
    for(int i = 0; i < MAX_SKINS; i++) // the source's texels, as offsets from this header (they are other blocks)
        hdr->texels[i] = skins->texels[i] ? reinterpret_cast<const byte*>(skins) + skins->texels[i] - reinterpret_cast<const byte*>(hdr) : 0;
    for(int b = 0; b < numBones; b++)
    {
        q_snprintf(bones[b].name, sizeof(bones[b].name), "%s:%d", frameName, b);
        bones[b].parent = -1;
        for(int i = 0; i < 12; i++)
        {
            const float id = i % 5 == 0 ? 1.f : 0.f;
            bones[b].inverse.mat[i] = id;
            bind[b].mat[i] = id;
            poses[b].mat[i] = id;
        }
    }
    hdr->boneinfo = reinterpret_cast<byte*>(bones) - reinterpret_cast<byte*>(hdr);
    hdr->bindpose = reinterpret_cast<byte*>(bind) - reinterpret_cast<byte*>(hdr);
    hdr->boneposedata = reinterpret_cast<byte*>(poses) - reinterpret_cast<byte*>(hdr);
    hdr->vertexes = reinterpret_cast<byte*>(out.verts) - reinterpret_cast<byte*>(hdr);
    hdr->indexes = reinterpret_cast<byte*>(out.indexes) - reinterpret_cast<byte*>(hdr);
    out.hdr = hdr;
    return out;
}

bool endModel(qmodel_t* mod, Made& m, float reach)
{
    if(mod->meshvbo)
    {
        GLMesh_DeleteVertexBuffer(mod); // (made again: its cache was given back)
    }
    GLMesh_LoadVertexBuffer(mod, m.hdr);
    mod->type = mod_alias;
    mod->needload = false;
    mod->numframes = 1;
    mod->synctype = ST_SYNC;
    mod->flags = 0;
    for(int i = 0; i < 3; i++)
    {
        mod->mins[i] = mod->ymins[i] = mod->rmins[i] = -reach;
        mod->maxs[i] = mod->ymaxs[i] = mod->rmaxs[i] = reach;
    }
    Hunk_MoveToCache(m.start, &mod->cache, m.hdr, mod->name); // its hunk blocks, without a copy (ROUND25.md, "nohunk")
    Con_DPrintf("beastarmour: %s made (%d vertices, %d triangles)\n", mod->name, m.hdr->numverts, m.hdr->numindexes / 3);
    return true;
}

[[nodiscard]] int8_t normalByte(float n)
{
    return static_cast<int8_t>(za::clamp(static_cast<int>(n * 127.f + (n < 0.f ? -0.5f : 0.5f)), -127, 127));
}

// "<piece model>#tweak": the piece model shaped by armour.txt (its vertices as `shaped`, one still bone), its skins its own.
bool makeTweaked(qmodel_t* mod, const char* base)
{
    const Slot* s = nullptr;
    for(const Kind& k : kinds)
    {
        for(const Slot& x : k.slot)
        {
            s = x.on && !ZA_STRCMP(x.model, base) ? &x : s;
        }
    }
    qmodel_t* src = s ? Mod_ForName(base, false) : nullptr;
    const auto* sh = src && src->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(src)) : nullptr;
    if(!sh || sh->numverts_vbo <= 0 || sh->numindexes < 3 || sh->poseverttype == aliashdr_t::PV_IQM)
    {
        return false;
    }
    mdldequant::prepare(src);
    Made m = beginModel(sh, sh->numverts_vbo, sh->numindexes, 1, "tweak");
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(sh) + sh->meshdesc);
    const float padW = static_cast<float>(TexMgr_PadConditional(sh->skinwidth)), padH = static_cast<float>(TexMgr_PadConditional(sh->skinheight));
    for(int v = 0; v < sh->numverts_vbo; v++)
    {
        iqmvert_t& o = m.verts[v];
        float xyz[3];
        signed char n[3];
        mdldequant::restVertex(sh, v, xyz, n);
        const glm::vec3 p = shaped(*s, geom::vec(xyz));
        for(int i = 0; i < 3; i++)
        {
            o.xyz[i] = p[i];
            o.norm[i] = static_cast<int8_t>(n[i]);
        }
        o.norm[3] = 0;
        o.st[0] = (static_cast<float>(desc[v].st[0]) + 0.5f) / padW;
        o.st[1] = (static_cast<float>(desc[v].st[1]) + 0.5f) / padH;
        o.weight[0] = 255;
        o.weight[1] = o.weight[2] = o.weight[3] = 0;
        o.idx[0] = o.idx[1] = o.idx[2] = o.idx[3] = 0;
    }
    ZA_MEMCPY(m.indexes, reinterpret_cast<const byte*>(sh) + sh->indexes, sizeof(unsigned short) * static_cast<za::SizeT>(sh->numindexes));
    return endModel(mod, m, 64.f);
}

// "<beast model>#bridge": its junctions' leather, each vertex on its junction's two bones (its piece's end, the joined
// piece's: blended along it), mapped on the humanoids' leather block as their joints' (vr_agarmor_bridge.cpp).
bool makeLeather(qmodel_t* mod, Kind& k)
{
    qmodel_t* skins = Mod_ForName(leatherSkins, false);
    const auto* kh = skins && skins->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(skins)) : nullptr;
    const int numVerts = static_cast<int>(k.lp.size()), numIndexes = static_cast<int>(k.lidx.size());
    if(!kh || numVerts <= 0 || numIndexes <= 0 || numVerts > 65535)
    {
        return false;
    }
    aliashdr_t skinHdr;
    ZA_MEMCPY(&skinHdr, kh, sizeof(aliashdr_t)); // (as makeBridgeModel: the hunk growing below may evict the skins' model)
    Made m = beginModel(&skinHdr, numVerts, numIndexes, za::max(2 * k.numJunctions, 1), "bridge");
    constexpr float jx0 = 224.f, jy0 = 64.f, jx1 = 256.f, jy1 = 96.f; // (the atlas's joint block, make_agarmor.py)
    const float padW = static_cast<float>(TexMgr_PadConditional(skinHdr.skinwidth)), padH = static_cast<float>(TexMgr_PadConditional(skinHdr.skinheight));
    for(int v = 0; v < numVerts; v++)
    {
        const za::SizeT i = static_cast<za::SizeT>(v);
        iqmvert_t& o = m.verts[v];
        for(int a = 0; a < 3; a++)
        {
            o.xyz[a] = k.lp[i][a];
            o.norm[a] = normalByte(k.ln[i][a]);
        }
        o.norm[3] = 0;
        o.st[0] = (jx0 + 0.5f + za::clamp(k.lu[i], 0.f, 1.f) * (jx1 - jx0 - 1.f)) / padW;
        o.st[1] = (jy0 + 0.5f + za::clamp(k.lv[i], 0.f, 1.f) * (jy1 - jy0 - 1.f)) / padH;
        const int w = za::clamp(static_cast<int>(k.lw[i] * 255.f + 0.5f), 0, 255);
        o.weight[0] = static_cast<uint8_t>(255 - w);
        o.weight[1] = static_cast<uint8_t>(w);
        o.weight[2] = o.weight[3] = 0;
        o.idx[0] = static_cast<uint8_t>(2 * k.lj[i]);
        o.idx[1] = static_cast<uint8_t>(2 * k.lj[i] + 1);
        o.idx[2] = o.idx[3] = 0;
    }
    for(int i = 0; i < numIndexes; i++)
    {
        m.indexes[i] = k.lidx[static_cast<za::SizeT>(i)];
    }
    return endModel(mod, m, 128.f);
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
        Con_Printf("beastarmour: %s: %d pieces (slots %d, cuirass group %d), %d junctions%s\n", k.model, n, k.mask, k.group, k.numJunctions,
            k.ok ? "" : " NO RIG");
        for(int i = 0; i < numSlots; i++)
        {
            const Slot& s = k.slot[i];
            if(s.on)
            {
                Con_Printf("  slot %d: %s on %s (bone %d, %d anchors, %d triangles)%s, joins %d\n", i, s.drawn, s.bone, s.boneIndex,
                    s.numAnchors, static_cast<int>(s.tris.size() / 3), s.tw.has ? " tweaked" : "", s.joins);
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
    numLeather = 0;
}

void relinkEntity(entity_t* e, int num, int type, int pieces, int wear)
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
    const bool studio = studioEnt > 0 && num == studioEnt;
    for(int i = 0; i < numSlots && numEnts < maxEnts && cl_numvisedicts < MAX_VISEDICTS; i++)
    {
        Slot& s = k->slot[i];
        Placed pl;
        if(!(pieces & (1 << i)) || !place(ent, s, pl))
        {
            continue;
        }
        qmodel_t* m = Mod_ForName(s.drawn, false);
        if(!m)
        {
            continue;
        }
        s.drawnModel = s.shaped ? m : nullptr;
        // (a shaped piece's skins are its .mdl's: its own model's count)
        const qmodel_t* skinned = s.shaped ? Mod_ForName(s.model, false) : m;
        const int numSkins = skinned && skinned->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(skinned)))->numskins : 1;
        const int level = (wear >> numSlots) & (1 << i) ? 2 : (wear & (1 << i)) ? 1 : 0;
        entity_t& a = ents[numEnts++];
        a = *e;
        a.model = m;
        a.frame = 0;
        a.previouspose = a.currentpose = 0;
        a.lerpflags = LERP_RESETANIM | LERP_RESETMOVE;
        a.skinnum = numSkins >= 9 ? type - 1 + 3 * level : za::min(type - 1, numSkins - 1);
        a.scale = static_cast<byte>(za::clamp(static_cast<int>(pl.scale * ENTSCALE_DEFAULT + 0.5f), 1, 255));
        a.alpha = studio ? static_cast<byte>(ENTALPHA_DEFAULT) : a.alpha;
        for(int c = 0; c < 3; c++)
        {
            a.origin[c] = pl.origin[c];
        }
        held::anglesFromAxes(pl.axes, a.angles, false);
        cl_visedicts[cl_numvisedicts++] = &a;
    }
    // Its junctions' leather: an entity at the body's place, its bones the pieces' (junctionBones).
    if(k->numJunctions > 0 && !k->lp.empty() && vr_beastarmour_leather.value != 0.f && numLeather < maxLeather && cl_numvisedicts < MAX_VISEDICTS)
    {
        if(!k->leather || ZA_STRCMP(k->leather->name, k->leatherName) != 0)
        {
            q_snprintf(k->leatherName, sizeof(k->leatherName), "%s%s", k->model, leatherSuffix);
            k->leather = Mod_ForName(k->leatherName, false);
        }
        const int n = numLeather;
        if(k->leather && junctionBones(ent, *k, pieces, geom::vec(e->origin), leatherBones[n]) > 0)
        {
            numLeather++;
            leatherBoneCount[n] = 2 * k->numJunctions;
            entity_t& a = leatherEnts[n];
            a = *e;
            a.model = k->leather;
            a.frame = 0;
            a.previouspose = a.currentpose = 0;
            a.lerpflags = LERP_RESETANIM | LERP_RESETMOVE;
            a.skinnum = type - 1; // (the leather is the same under every wear's paint)
            a.scale = ENTSCALE_DEFAULT;
            a.alpha = studio ? static_cast<byte>(ENTALPHA_DEFAULT) : a.alpha;
            a.angles[0] = a.angles[1] = a.angles[2] = 0.f;
            cl_visedicts[cl_numvisedicts++] = &a;
        }
    }
    if(studio && studioAlpha < 0.999f)
    {
        e->alpha = ENTALPHA_ENCODE(za::clamp(studioAlpha, 0.05f, 1.f));
    }
}

int drawnCount()
{
    return numEnts;
}

int leatherDrawn()
{
    return numLeather;
}

int bonePoses(const entity_t* e, const float** matrices)
{
    if(numLeather <= 0 || e < &leatherEnts[0] || e >= &leatherEnts[numLeather])
    {
        return 0;
    }
    const int k = static_cast<int>(e - &leatherEnts[0]);
    if(matrices)
    {
        *matrices = leatherBones[k];
    }
    return leatherBoneCount[k];
}

bool makeModel(qmodel_t* mod)
{
    char base[MAX_QPATH];
    const za::SizeT len = ZA_STRLEN(mod->name);
    if(strings::endsWith(mod->name, tweakSuffix))
    {
        q_strlcpy(base, mod->name, za::min(sizeof(base), len - ZA_STRLEN(tweakSuffix) + 1));
        return makeTweaked(mod, base);
    }
    if(strings::endsWith(mod->name, leatherSuffix))
    {
        q_strlcpy(base, mod->name, za::min(sizeof(base), len - ZA_STRLEN(leatherSuffix) + 1));
        Kind* k = kindOf(base);
        return k && resolve(*k) && makeLeather(mod, *k);
    }
    return false;
}

void studioView(int ent, float bodyAlpha)
{
    studioEnt = ent;
    studioAlpha = bodyAlpha;
}

void outline(edict_t* ent, int pieces, bool sleeve)
{
    const Kind* k = ready(ent);
    if(!k)
    {
        return;
    }
    const glm::vec4 col{0.2f, 1.f, 1.f, 1.f};
    for(int i = 0; i < numSlots && !sleeve; i++)
    {
        Placed pl;
        if(!((pieces >> i) & 1) || !place(ent, k->slot[i], pl))
        {
            continue;
        }
        const Slot& s = k->slot[i];
        for(za::SizeT t = 0; t + 2 < s.tris.size(); t += 3)
        {
            glm::vec3 w[3];
            for(za::SizeT c = 0; c < 3; c++)
            {
                w[c] = pl.origin + pl.axes * (s.tris[t + c] * pl.scale);
            }
            lines::line(w[0], w[1], 0.06f, col, col);
            lines::line(w[1], w[2], 0.06f, col, col);
            lines::line(w[2], w[0], 0.06f, col, col);
        }
    }
    float bones[maxLeatherBones * 12];
    if(!sleeve || junctionBones(ent, *k, k->mask, glm::vec3{0.f}, bones) <= 0)
    {
        return;
    }
    for(int j = 0; j < k->numJunctions; j++)
    {
        const Junction& jn = k->junction[j];
        if(!((pieces >> jn.child) & 1))
        {
            continue;
        }
        for(int t = 0; t + 2 < jn.numIndexes; t += 3)
        {
            glm::vec3 w[3];
            for(int c = 0; c < 3; c++)
            {
                w[c] = posedLeather(*k, k->lidx[static_cast<za::SizeT>(jn.firstIndex + t + c)], bones);
            }
            lines::line(w[0], w[1], 0.06f, col, col);
            lines::line(w[1], w[2], 0.06f, col, col);
            lines::line(w[2], w[0], 0.06f, col, col);
        }
    }
}

int kindCount()
{
    if(!loaded)
    {
        load();
    }
    return static_cast<int>(kinds.size());
}

const char* kindModel(int i)
{
    return i >= 0 && i < kindCount() ? kinds[static_cast<za::SizeT>(i)].model : "";
}

int kindIndex(const char* model)
{
    const Kind* k = kindOf(model);
    return k ? static_cast<int>(k - kinds.data()) : -1;
}

int kindSlots(int i)
{
    if(i < 0 || i >= kindCount())
    {
        return 0;
    }
    Kind& k = kinds[static_cast<za::SizeT>(i)];
    return resolve(k) ? k.mask : 0;
}

int joins(int i, int slot)
{
    if(i < 0 || i >= kindCount() || slot < 0 || slot >= numSlots)
    {
        return -1;
    }
    Kind& k = kinds[static_cast<za::SizeT>(i)];
    return resolve(k) && k.slot[slot].on ? k.slot[slot].joins : -1;
}

bool junctionMade(int i, int slot)
{
    if(i < 0 || i >= kindCount())
    {
        return false;
    }
    Kind& k = kinds[static_cast<za::SizeT>(i)];
    for(int j = 0; resolve(k) && j < k.numJunctions; j++)
    {
        if(k.junction[j].child == slot)
        {
            return true;
        }
    }
    return false;
}

const char* boneOf(int i, int slot)
{
    return i >= 0 && i < kindCount() && slot >= 0 && slot < numSlots ? kinds[static_cast<za::SizeT>(i)].slot[slot].bone : "";
}

const char* drawnModel(int i, int slot)
{
    return i >= 0 && i < kindCount() && slot >= 0 && slot < numSlots ? kinds[static_cast<za::SizeT>(i)].slot[slot].drawn : "";
}

bool pieceExtent(int i, int slot, float* lo, float* hi)
{
    if(i < 0 || i >= kindCount() || slot < 0 || slot >= numSlots)
    {
        return false;
    }
    Kind& k = kinds[static_cast<za::SizeT>(i)];
    if(!resolve(k) || !k.slot[slot].on)
    {
        return false;
    }
    for(int c = 0; c < 3; c++)
    {
        lo[c] = k.slot[slot].lo[c];
        hi[c] = k.slot[slot].hi[c];
    }
    return true;
}

JunctionCheck checkJunction(edict_t* ent, int slot, int pieces)
{
    JunctionCheck out;
    Kind* k = ready(ent);
    float bones[maxLeatherBones * 12];
    if(!k || junctionBones(ent, *k, pieces, glm::vec3{0.f}, bones) <= 0)
    {
        return out;
    }
    for(int j = 0; j < k->numJunctions; j++)
    {
        const Junction& jn = k->junction[j];
        Placed pl[2];
        if(jn.child != slot || !place(ent, k->slot[jn.child], pl[0]) || !place(ent, k->slot[jn.parent], pl[1]))
        {
            continue;
        }
        out.found = true;
        const int ring = junctionCols + 1, rings = jn.numVerts / ring;
        glm::vec3 midSum{0.f};
        for(int v = 0; v < jn.numVerts; v++)
        {
            const int r = v / ring;
            const glm::vec3 p = posedLeather(*k, jn.first + v, bones);
            if(r == 0 || r == rings - 1)
            {
                // (an end ring: where its piece's place carries its rest place, when worn)
                const int e = r == 0 ? 0 : 1;
                const bool worn = ((pieces >> (e == 0 ? jn.child : jn.parent)) & 1) != 0;
                const glm::vec3 want = pl[e].restRot * k->lp[static_cast<za::SizeT>(jn.first + v)] + pl[e].restT;
                out.endError = worn ? za::max(out.endError, glm::distance(p, want)) : out.endError;
            }
            if(r == rings / 2)
            {
                midSum += p;
            }
            if(v % ring + 1 < ring && r + 1 < rings)
            {
                out.longestEdge = za::max(out.longestEdge, glm::distance(p, posedLeather(*k, jn.first + v + ring, bones)));
            }
        }
        const glm::vec3 mid = midSum / static_cast<float>(ring);
        for(int c = 0; c < 3; c++)
        {
            out.middle[c] = mid[c];
        }
        out.verts = jn.numVerts;
    }
    return out;
}

void init()
{
    Cmd_AddCommand("vr_beastarmour_info", info_f);
}

} // namespace qvr::beastarmour
