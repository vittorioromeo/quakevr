// vr_humstudio.cpp -- the Texture Studio (Debug > Tools > Texture Studio; vr_humlook.hpp): the armed humanoids' recolours
// edited live. His words (2026-11-21): "An in-game tool to tweak the recoloring and masks would also be good". The page
// picks a row of the table (type, weapon category, part); its sliders (vr_humstudio_*) are laid into the table as they
// move and the row of humanoids lined up ahead (Line Up: QC VR_HUM_LineupKind, one per category) shows them at once. The
// masks: each palette row of the skin edited counted as clothes, pack or none (the kind's rows, or the skin's own), and
// texels painted by hand where you point at an armed humanoid (VR: either hand's ray, its trigger paints, its grip erases,
// B/Y undoes; flat: the view's centre, attack paints) -- the same strokes in both, only the ray differs. Undo, the masks
// shown (overlay), Save writes humlook.txt and humlook_masks.txt.

#include "vr_humlook.hpp"

#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_hands.hpp"
#include "vr_hitmodel_internal.hpp"
#include "vr_main.hpp"
#include "vr_modelcollide.hpp"
#include "vr_posing.hpp"
#include "vr_progs.hpp"
#include "vr_ragdoll.hpp"
#include "vr_region_shared.h"
#include "vr_rig2.hpp"
#include "vr_testkit.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/Swap.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/glm.hpp>

#include <cstdio>
#include <cstring>

namespace qvr::humlook
{
namespace
{
constexpr int undoMax = 24;
constexpr const char* rowClassNames[] = {"None", "Clothes", "Pack"};

// An undo step: the rows edited (a key's or a skin's own) and a skin's painted texels, as they were.
struct UndoStep
{
    int skin{-1}; // its index in allSkins() (-1: a key's rows only)
    int key{0};
    bool ownRows{false};
    unsigned char rows[16]{};
    za::Vector<unsigned char> paint;
};

// An armed humanoid's (or a pack's) model as drawn this frame: its triangles in the world, three corners each, in its
// .mdl's order (`hdr`: their skin coordinates), and its mask.
struct Drawn
{
    int num{0};
    Skin* skin{nullptr};
    const aliashdr_t* hdr{nullptr};
    const qmodel_t* model{nullptr}; // the entity's model as drawn (its rig: the triangles' body parts)
    za::Vector<glm::vec3> tris;
};

// What the ray met: the skin, the entity, the triangle (its index), the skin coordinates and texel there, the point.
struct Hit
{
    Skin* skin{nullptr};
    int num{0};
    int tri{-1};
    float s{0.f}, t{0.f};
    int texel{0};
    glm::vec3 point{0.f};
};

// vr_humstudio_picktest's state: the kinds lined up in turn, each tested once drawn.
struct PickTest
{
    bool on{false};
    int kind{0};      // the kind lined up now
    int lastKind{3};  // the last tested
    int frames{0};    // frames waited for the row to be drawn
    int settled{0};   // ... and since all of it was
    testkit::Tally tally;
    int skipped{0};
};

struct Studio
{
    int sel[3]{-1, -1, -1}; // the row whose values the sliders show (kind, category, part)
    float vals[6]{};        // the sliders as last laid into the table
    int skin{-1};           // the skin edited (its index in allSkins(); the last painted, else the first of its key)
    int stroke{0};          // 1 painting, -1 erasing, 0 none
    int strokeHand{1};
    bool strokeUndone{false};
    bool unsaved{false};
    za::Vector<UndoStep> undo;
    Drawn hit, cand;               // the ray's model (the brush's footprint reads it), and the one being tested
    za::Vector<int> brushTexels;   // the brush's texels now (the cursor's)
    za::Vector<unsigned char> mark; // (footprint's: a texel taken already)
    int cursorSkin{-1};            // the skin showing the cursor (its index in allSkins(); -1 none)
    float turn{0.f};               // vr_humstudio_turn as last laid on the lineup
    PickTest pick;
    gfx::Picture pictures[numKeys]; // each key's skin view
    za::Vector<unsigned char> rgba; // (the skin view's pixels)
};
// seat-shared: the Texture Studio, one editor (seat 1's mouse and headset)
Studio st;
// seat-shared: with st
char statusText[256];
// seat-shared: with st
char rowText[96];

cvar_t* const valueCvars[6] = {&vr_humstudio_r, &vr_humstudio_g, &vr_humstudio_b, &vr_humstudio_strength,
    &vr_humstudio_light, &vr_humstudio_tint};

[[nodiscard]] int selKind()
{
    return za::clamp(static_cast<int>(vr_humstudio_kind.value), 0, 3);
}
[[nodiscard]] int selCat()
{
    return za::clamp(static_cast<int>(vr_humstudio_cat.value), 1, maxCategories - 1);
}
[[nodiscard]] int selPart()
{
    return vr_humstudio_part.value > 0.5f ? 1 : 0;
}
[[nodiscard]] int selKey()
{
    return selPart() ? keyPack : selKind();
}

// The skin edited: the last painted if it is of the key picked, else the first read of that key (nullptr: none drawn yet).
[[nodiscard]] Skin* editedSkin()
{
    za::Vector<Skin>& skins = allSkins();
    if(st.skin >= 0 && st.skin < static_cast<int>(skins.size()) && skins[static_cast<za::SizeT>(st.skin)].key == selKey() &&
        !skins[static_cast<za::SizeT>(st.skin)].texels.empty())
    {
        return &skins[static_cast<za::SizeT>(st.skin)];
    }
    for(za::SizeT i = 0; i < skins.size(); i++)
    {
        if(skins[i].key == selKey() && !skins[i].texels.empty())
        {
            st.skin = static_cast<int>(i);
            return &skins[i];
        }
    }
    return nullptr;
}

void markKeyDirty(int key)
{
    for(Skin& s : allSkins())
    {
        s.dirty = s.dirty || s.key == key;
    }
}

void pushUndo(Skin* s, int key)
{
    UndoStep u;
    u.key = key;
    if(s)
    {
        u.skin = static_cast<int>(s - allSkins().data());
        u.ownRows = s->ownRows;
        memcpy(u.rows, s->ownRows ? s->rows : keyRows(key), sizeof(u.rows));
        u.paint = s->paint;
    }
    else
    {
        memcpy(u.rows, keyRows(key), sizeof(u.rows));
    }
    if(static_cast<int>(st.undo.size()) >= undoMax)
    {
        st.undo.erase(st.undo.begin());
    }
    st.undo.pushBack(static_cast<UndoStep&&>(u));
    st.unsaved = true;
}

void undo_f()
{
    if(st.undo.empty())
    {
        Con_Printf("humstudio: nothing to undo\n");
        return;
    }
    UndoStep u = static_cast<UndoStep&&>(st.undo.back());
    st.undo.popBack();
    za::Vector<Skin>& skins = allSkins();
    Skin* s = u.skin >= 0 && u.skin < static_cast<int>(skins.size()) ? &skins[static_cast<za::SizeT>(u.skin)] : nullptr;
    if(s)
    {
        s->ownRows = u.ownRows;
        memcpy(s->ownRows ? s->rows : keyRows(u.key), u.rows, sizeof(u.rows));
        s->paint = static_cast<za::Vector<unsigned char>&&>(u.paint);
        s->dirty = true;
    }
    else
    {
        memcpy(keyRows(u.key), u.rows, sizeof(u.rows));
    }
    markKeyDirty(u.key);
    Con_Printf("humstudio: undone (%d left)\n", static_cast<int>(st.undo.size()));
}

// The sliders against the table: a new row picked shows its values; a slider moved is laid into the row (set).
void syncSliders()
{
    const int sel[3] = {selKind(), selCat(), selPart()};
    Spec* s = spec(sel[0], sel[1], sel[2]);
    if(!s)
    {
        return;
    }
    if(memcmp(sel, st.sel, sizeof(sel)) != 0)
    {
        memcpy(st.sel, sel, sizeof(sel));
        const Spec shown = s->set ? *s : Spec{};
        const float v[6] = {shown.rgb[0], shown.rgb[1], shown.rgb[2], shown.strength, shown.light, shown.tint};
        for(int i = 0; i < 6; i++)
        {
            Cvar_SetValueQuick(valueCvars[i], v[i]);
            st.vals[i] = valueCvars[i]->value;
        }
        return;
    }
    bool moved = false;
    for(int i = 0; i < 6; i++)
    {
        moved = moved || valueCvars[i]->value != st.vals[i];
        st.vals[i] = valueCvars[i]->value;
    }
    if(moved)
    {
        s->set = true;
        for(int i = 0; i < 3; i++)
        {
            s->rgb[i] = za::clamp(st.vals[i], 0.f, 1.f);
        }
        s->strength = za::clamp(st.vals[3], 0.f, 1.f);
        s->light = za::clamp(st.vals[4], 0.f, 4.f);
        s->tint = za::clamp(st.vals[5], 0.f, 1.f);
        st.unsaved = true;
    }
}

// The ray painting: VR the hand's aim, flat the view's centre (the crosshair).
[[nodiscard]] bool aimRay(int hand, glm::vec3& from, glm::vec3& dir)
{
    if(vrActive() && hand < 2) // (2: the view's centre in the headset too: vr_humstudio_stroke's tests)
    {
        const hands::State& s = hands::current();
        if(!s.valid)
        {
            return false;
        }
        const int h = za::clamp(hand, 0, 1);
        from = s.pos[h];
        dir = hands::forward(s.aimRot[h]);
        return true;
    }
    vec3_t f, r, u;
    AngleVectors(r_refdef.viewangles, f, r, u);
    from = glm::vec3{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    dir = glm::vec3{f[0], f[1], f[2]};
    return true;
}

// What the ray paints on (vr_humstudio_target): 1 bodies, 2 packs, 3 either; 0 the part picked's (clothes: the bodies, the
// packs passed through: a pack never takes a stroke meant for its wearer's head or back).
[[nodiscard]] int targetMode()
{
    const int t = za::clamp(static_cast<int>(vr_humstudio_target.value), 0, 3);
    return t != 0 ? t : selPart() ? 2 : 1;
}

// `num`'s model as drawn this frame, if it is an armed humanoid (or its pack) the ray may paint on (`mode`: targetMode's):
// its triangles in the world (modelcollide::drawnTriangles: a live one's on its authored rig as its arms are solved, a
// ragdoll's as its parts carry it, else its frames' lerp), its .mdl's (their skin coordinates, in the same order) and its
// mask. False: none such.
bool drawnOf(int num, int mode, Drawn& d)
{
    entity_t* e = &cl_entities[num];
    const int key = keyOf(lookOf(num));
    if(key < 0 || !e->model || e->model->type != mod_alias || e->msgtime != cl.mtime[0] ||
        (mode != 3 && (mode == 2) != (key == keyPack)))
    {
        return false;
    }
    const qmodel_t* src = ragdoll::sourceModel(e); // (the .mdl a derived model is drawn from: its triangles' order)
    src = src ? src : rig2::sourceModel(e);
    src = src ? src : e->model;
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(src)));
    Skin* sk = skinFor(e->model, e->skinnum, key);
    if(!sk || sk->texels.empty() || !hdr || hdr->skinwidth != sk->w || hdr->skinheight != sk->h ||
        !modelcollide::drawnTriangles(*e, num, d.tris) || d.tris.size() != static_cast<za::SizeT>(hdr->numindexes / 3 * 3))
    {
        return false;
    }
    d.num = num;
    d.skin = sk;
    d.hdr = hdr;
    d.model = e->model;
    return true;
}

// Corner `idx` (an index of the .mdl's triangle list) of a triangle: its skin coordinates (a texel's middle at whole numbers).
[[nodiscard]] glm::vec2 stAt(const aliashdr_t* hdr, int idx)
{
    const auto* base = reinterpret_cast<const byte*>(hdr);
    const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
    const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
    return glm::vec2{static_cast<float>(mesh[indexes[idx]].st[0]), static_cast<float>(mesh[indexes[idx]].st[1])};
}

// The texel at skin coordinates `uv` (y * w + x).
[[nodiscard]] int texelAt(const Skin& s, glm::vec2 uv)
{
    const int x = za::clamp(static_cast<int>(uv.x + 0.5f), 0, s.w - 1);
    const int y = za::clamp(static_cast<int>(uv.y + 0.5f), 0, s.h - 1);
    return y * s.w + x;
}

// Where the ray from `o` along `d` crosses triangle (a, b, c): its distance along it and the place on it (b's share, c's).
[[nodiscard]] bool crossing(const glm::vec3& o, const glm::vec3& d, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
    float& t, float& bu, float& bv)
{
    const glm::vec3 e1 = b - a, e2 = c - a, p = glm::cross(d, e2);
    const float det = glm::dot(e1, p);
    if(det > -1e-9f && det < 1e-9f)
    {
        return false;
    }
    const glm::vec3 q = o - a;
    bu = glm::dot(q, p) / det;
    const glm::vec3 qq = glm::cross(q, e1);
    bv = glm::dot(d, qq) / det;
    t = glm::dot(e2, qq) / det;
    return bu >= 0.f && bv >= 0.f && bu + bv <= 1.f && t > 0.f;
}

// The nearest armed humanoid's (or pack's: `mode`) triangle on the ray, as drawn this frame, and its skin's texel there
// (from the place on the triangle: its corners' skin coordinates); its model kept in st.hit for the brush.
[[nodiscard]] Hit hitTest(const glm::vec3& from, const glm::vec3& dir, int mode)
{
    Hit best;
    st.hit.skin = nullptr;
    if(!cl_entities)
    {
        return best;
    }
    float bestT = 1e30f, bestU = 0.f, bestV = 0.f;
    for(int i = 1; i < cl.num_entities && i < cl_max_edicts; i++)
    {
        if(!drawnOf(i, mode, st.cand))
        {
            continue;
        }
        bool nearer = false;
        for(za::SizeT k = 0; k + 2 < st.cand.tris.size(); k += 3)
        {
            float t, bu, bv;
            if(crossing(from, dir, st.cand.tris[k], st.cand.tris[k + 1], st.cand.tris[k + 2], t, bu, bv) && t < bestT)
            {
                bestT = t;
                bestU = bu;
                bestV = bv;
                best.tri = static_cast<int>(k / 3);
                nearer = true;
            }
        }
        if(nearer)
        {
            za::genericSwap(st.hit, st.cand);
            best.num = i;
        }
    }
    if(best.tri < 0 || !st.hit.skin)
    {
        return best;
    }
    const int k = best.tri * 3;
    const glm::vec2 uv = stAt(st.hit.hdr, k) * (1.f - bestU - bestV) + stAt(st.hit.hdr, k + 1) * bestU +
                         stAt(st.hit.hdr, k + 2) * bestV;
    best.skin = st.hit.skin;
    best.s = uv.x;
    best.t = uv.y;
    best.texel = texelAt(*best.skin, uv);
    best.point = from + dir * bestT;
    return best;
}

// The brush's texels about hit `h` (st.hit's model): the texel hit, and every texel within vr_humstudio_brush (cm) of the
// point hit on the model's surface, on whichever triangles carry it (each triangle near the point walked over in its skin
// coordinates: a texel's middle inside it placed on the model). A texel some far triangle also carries (a skin's shared
// texels) is changed there too: the cursor shows it on the whole model.
void footprint(const Hit& h, za::Vector<int>& out)
{
    out.clear();
    if(!h.skin)
    {
        return;
    }
    const Skin& s = *h.skin;
    out.pushBack(h.texel);
    const float r = za::max(vr_humstudio_brush.value, 0.f) * 0.01f * units::metresToUnits();
    if(r <= 0.f)
    {
        return;
    }
    st.mark.resize(static_cast<za::SizeT>(s.w * s.h), 0);
    st.mark[static_cast<za::SizeT>(h.texel)] = 1;
    const za::Vector<glm::vec3>& tris = st.hit.tris;
    for(za::SizeT k = 0; k + 2 < tris.size(); k += 3)
    {
        const glm::vec3 &a = tris[k], &b = tris[k + 1], &c = tris[k + 2];
        const glm::vec3 lo = glm::min(a, glm::min(b, c)) - glm::vec3{r}, hi = glm::max(a, glm::max(b, c)) + glm::vec3{r};
        if(glm::any(glm::lessThan(h.point, lo)) || glm::any(glm::greaterThan(h.point, hi)))
        {
            continue;
        }
        const int idx = static_cast<int>(k);
        const glm::vec2 ta = stAt(st.hit.hdr, idx), tb = stAt(st.hit.hdr, idx + 1), tc = stAt(st.hit.hdr, idx + 2);
        const glm::vec2 v0 = tb - ta, v1 = tc - ta;
        const float den = v0.x * v1.y - v1.x * v0.y;
        if(den > -1e-6f && den < 1e-6f)
        {
            continue;
        }
        const glm::vec2 slo = glm::min(ta, glm::min(tb, tc)), shi = glm::max(ta, glm::max(tb, tc));
        for(int y = za::max(static_cast<int>(slo.y) - 1, 0); y <= za::min(static_cast<int>(shi.y) + 1, s.h - 1); y++)
        {
            for(int x = za::max(static_cast<int>(slo.x) - 1, 0); x <= za::min(static_cast<int>(shi.x) + 1, s.w - 1); x++)
            {
                const glm::vec2 v2 = glm::vec2{static_cast<float>(x), static_cast<float>(y)} - ta;
                const float u = (v2.x * v1.y - v1.x * v2.y) / den, v = (v0.x * v2.y - v2.x * v0.y) / den;
                if(u < -1e-4f || v < -1e-4f || u + v > 1.0001f)
                {
                    continue;
                }
                const glm::vec3 p = a + (b - a) * u + (c - a) * v;
                const int t = y * s.w + x;
                if(glm::dot(p - h.point, p - h.point) <= r * r && !st.mark[static_cast<za::SizeT>(t)])
                {
                    st.mark[static_cast<za::SizeT>(t)] = 1;
                    out.pushBack(t);
                }
            }
        }
    }
    for(const int t : out)
    {
        st.mark[static_cast<za::SizeT>(t)] = 0;
    }
}

// The brush's texels painted the class picked (or erased: their row's again). The texels changed.
int brush(Skin& s, const za::Vector<int>& texels, int value)
{
    int changed = 0;
    for(const int t : texels)
    {
        unsigned char& p = s.paint[static_cast<za::SizeT>(t)];
        if(p != value)
        {
            if(!st.strokeUndone)
            {
                pushUndo(&s, s.key);
                st.strokeUndone = true;
            }
            p = static_cast<unsigned char>(value);
            changed++;
        }
    }
    s.dirty = s.dirty || changed > 0;
    return changed;
}

// The cursor shown on skin `s` (nullptr: none): the brush's texels now.
void setCursor(Skin* s, const za::Vector<int>& texels)
{
    za::Vector<Skin>& skins = allSkins();
    const int idx = s ? static_cast<int>(s - skins.data()) : -1;
    if(st.cursorSkin >= 0 && st.cursorSkin < static_cast<int>(skins.size()) && st.cursorSkin != idx)
    {
        Skin& old = skins[static_cast<za::SizeT>(st.cursorSkin)];
        old.cursor.clear();
        old.dirty = true;
    }
    st.cursorSkin = idx;
    if(s && !(s->cursor.size() == texels.size() && memcmp(s->cursor.data(), texels.data(), texels.size() * sizeof(int)) == 0))
    {
        s->cursor = texels;
        s->dirty = true;
    }
}

// This frame's ray: the cursor where it meets an armed humanoid, and the stroke's paint; what it did (the tests' line).
const char* strokeFrame()
{
    const bool cursor = vr_humstudio_cursor.value != 0.f;
    if(vr_humstudio_paint.value == 0.f || key_dest != key_game || (st.stroke == 0 && !cursor))
    {
        setCursor(nullptr, st.brushTexels);
        return st.stroke == 0 || vr_humstudio_paint.value == 0.f ? "no stroke" : "not in the game (a menu or the console up)";
    }
    glm::vec3 from, dir;
    if(!aimRay(st.strokeHand, from, dir))
    {
        setCursor(nullptr, st.brushTexels);
        return "no ray";
    }
    const Hit h = hitTest(from, dir, targetMode());
    if(!h.skin)
    {
        setCursor(nullptr, st.brushTexels);
        return va("no armed humanoid on the ray from %.0f %.0f %.0f along %.2f %.2f %.2f", from.x, from.y, from.z, dir.x,
            dir.y, dir.z);
    }
    footprint(h, st.brushTexels);
    setCursor(cursor ? h.skin : nullptr, st.brushTexels);
    if(st.stroke == 0)
    {
        return "no stroke";
    }
    const int value = st.stroke > 0 ? za::clamp(static_cast<int>(vr_humstudio_class.value), 0, 3) : ClassNone;
    const int changed = brush(*h.skin, st.brushTexels, value);
    st.skin = static_cast<int>(h.skin - allSkins().data());
    return va("%s texel %d %d: %d changed", h.skin->model.cStr(), h.texel % h.skin->w, h.texel / h.skin->w, changed);
}

// The lineup's options (QC VR_HUM_LineupKind's bits): 1 no backpacks, 2 no armour, 4 no weapons drawn, 8 a pack on its
// stand, 16 turned (vr_humstudio_turn).
[[nodiscard]] int lineupOptions()
{
    return (vr_humstudio_nopack.value != 0.f ? 1 : 0) | (vr_humstudio_noarmour.value != 0.f ? 2 : 0) |
           (vr_humstudio_noweapon.value != 0.f ? 4 : 0) | (vr_humstudio_packstand.value != 0.f ? 8 : 0) | 16;
}

// QC `name`(player, kind, options, turn): the lineup made, or turned. False: no map with you in it, or no such function.
bool callLineup(const char* name, int kind)
{
    if(!sv.active)
    {
        Con_Printf("humstudio: a map with you in it is needed\n");
        return false;
    }
    const progs::ServerVm vm;
    const func_t fn = progs::findFunction(name);
    edict_t* player = progs::playerEdict();
    if(!fn || !player)
    {
        Con_Printf("humstudio: no lineup (QC %s)\n", name);
        return false;
    }
    st.turn = vr_humstudio_turn.value;
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(player);
    G_INT(OFS_PARM0) = EDICT_TO_PROG(player);
    G_FLOAT(OFS_PARM1) = static_cast<float>(kind);
    G_FLOAT(OFS_PARM2) = static_cast<float>(lineupOptions());
    G_FLOAT(OFS_PARM3) = st.turn;
    PR_ExecuteProgram(fn);
    return true;
}

void lineup_f()
{
    (void)callLineup("VR_HUM_LineupKind", Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : selKind());
}

// ---- vr_humstudio_picktest: the pick against known texels ----
//
// Each kind lined up in turn (the options as set: the test script keeps the packs on), and once its five are drawn, on
// each of them rays aimed at known places of the model as drawn: its hand (the arm's triangle farthest from its middle),
// its forearm (the arm's middle one), the back of its head (the head's rearmost: a ray from behind, level, as a hand
// behind it aims) and its back beside the pack (the body's rearmost below the head: a level ray from behind). Each must
// paint the texel under that triangle's middle (its corners' skin coordinates), whatever is in the way (a pack, a gun).
// The first hand also takes a stroke (the brush's footprint holds the texel; one undo step puts the paint back).
// "humstudio_picktest: N passed, M failed" ends it.

constexpr int pickWaitFrames = 400;  // (a safety net: a row drawn in a few frames)
constexpr int pickSettleFrames = 36; // frames its five are drawn before the cases (its spawn's turn and arms settled)

// The triangles of `d` of body part `region` (VR_REGION_*) with their middles at most `maxUp` above `origin`, the farthest
// along `axis` from it first (none: no such part, no rig fits).
void candidates(const Drawn& d, int region, const glm::vec3& origin, const glm::vec3& axis, float maxUp, za::Vector<int>& out)
{
    const auto* base = reinterpret_cast<const byte*>(d.hdr);
    const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + d.hdr->meshdesc);
    const auto* indexes = reinterpret_cast<const unsigned short*>(base + d.hdr->indexes);
    za::Vector<glm::vec2> found; // (along, triangle)
    for(za::SizeT k = 0; k + 2 < d.tris.size(); k += 3)
    {
        const int r = hitmodel::detail::cornersRegion(d.model, d.hdr->numverts, mesh[indexes[k]].vertindex,
            mesh[indexes[k + 1]].vertindex, mesh[indexes[k + 2]].vertindex);
        const glm::vec3 mid = (d.tris[k] + d.tris[k + 1] + d.tris[k + 2]) / 3.f;
        const glm::vec3 n = glm::cross(d.tris[k + 1] - d.tris[k], d.tris[k + 2] - d.tris[k]);
        if(r != region || mid.z - origin.z > maxUp || glm::dot(n, n) < 0.05f)
        {
            continue;
        }
        found.pushBack(glm::vec2{-glm::dot(mid - origin, axis), static_cast<float>(k / 3)});
    }
    for(za::SizeT i = 1; i < found.size(); i++) // (insertion sort: a few hundred at most)
    {
        for(za::SizeT j = i; j > 0 && found[j - 1].x > found[j].x; j--)
        {
            za::genericSwap(found[j - 1], found[j]);
        }
    }
    out.clear();
    for(const glm::vec2& f : found)
    {
        out.pushBack(static_cast<int>(f.y));
    }
}

// The first of `d`'s own triangles on the ray (-1 none): a case's triangle is one nothing of its own model hides.
[[nodiscard]] int firstOwn(const Drawn& d, const glm::vec3& from, const glm::vec3& dir)
{
    int best = -1;
    float bestT = 1e30f;
    for(za::SizeT k = 0; k + 2 < d.tris.size(); k += 3)
    {
        float t, bu, bv;
        if(crossing(from, dir, d.tris[k], d.tris[k + 1], d.tris[k + 2], t, bu, bv) && t < bestT)
        {
            bestT = t;
            best = static_cast<int>(k / 3);
        }
    }
    return best;
}

// One case: the ray from `from` along `dir` must paint the texel under triangle `tri`'s middle on `num`'s skin.
bool pickCase(const char* what, int num, int tri, const glm::vec3& from, const glm::vec3& dir, bool stroke)
{
    if(tri < 0)
    {
        st.pick.skipped++;
        Con_Printf("humstudio_picktest: entity %d %s: no case found (no such part, or no pack worn: skipped)\n", num, what);
        return true;
    }
    if(!drawnOf(num, 3, st.cand))
    {
        st.pick.tally.check(false);
        Con_Printf("humstudio_picktest: FAIL entity %d %s: not drawn\n", num, what);
        return false;
    }
    const glm::vec2 uv = (stAt(st.cand.hdr, tri * 3) + stAt(st.cand.hdr, tri * 3 + 1) + stAt(st.cand.hdr, tri * 3 + 2)) / 3.f;
    Skin* want = st.cand.skin;
    const int wantTexel = texelAt(*want, uv);
    const Hit any = hitTest(from, dir, 3); // (what is first on the ray, a pack too: whether one is in the way)
    const bool packFirst = any.skin && any.skin->key == keyPack;
    const Hit h = hitTest(from, dir, targetMode());
    const bool ok = h.skin == want && h.num == num && h.texel == wantTexel;
    st.pick.tally.check(ok);
    Con_Printf("humstudio_picktest: %s entity %d %s: entity %d texel %d %d (want %d %d)%s\n", ok ? "ok" : "FAIL", num, what,
        h.num, h.skin ? h.texel % h.skin->w : -1, h.skin ? h.texel / h.skin->w : -1, wantTexel % want->w,
        wantTexel / want->w, packFirst ? ", a pack first on the ray (passed through)" : "");
    if(ok && stroke)
    {
        const za::Vector<unsigned char> before = want->paint;
        footprint(h, st.brushTexels);
        bool holds = false;
        for(const int t : st.brushTexels)
        {
            holds = holds || t == wantTexel;
        }
        st.strokeUndone = false;
        const int undoBefore = static_cast<int>(st.undo.size());
        const int changed = brush(*want, st.brushTexels, ClassNever);
        const int steps = static_cast<int>(st.undo.size()) - undoBefore;
        undo_f();
        const bool same = want->paint.size() == before.size() &&
                          memcmp(want->paint.data(), before.data(), before.size()) == 0;
        const bool strokeOk = holds && changed > 0 && steps == 1 && same;
        st.pick.tally.check(strokeOk);
        Con_Printf("humstudio_picktest: %s entity %d stroke: %d texels in the brush, %d changed, %d undo step, undone %s\n",
            strokeOk ? "ok" : "FAIL", num, static_cast<int>(st.brushTexels.size()), changed, steps,
            same ? "to the paint before" : "NOT to the paint before");
    }
    return ok;
}

// The cases on one lined-up humanoid.
void pickEntity(int num)
{
    if(!drawnOf(num, 1, st.hit))
    {
        return;
    }
    const entity_t& e = cl_entities[num];
    vec3_t ang = {0.f, e.angles[1], 0.f}, f, r, u;
    AngleVectors(ang, f, r, u);
    const glm::vec3 fwd{f[0], f[1], f[2]}, origin{e.origin[0], e.origin[1], e.origin[2]};
    glm::vec3 mid{0.f};
    for(const glm::vec3& v : st.hit.tris)
    {
        mid += v;
    }
    mid /= static_cast<float>(za::max(st.hit.tris.size(), static_cast<za::SizeT>(1)));
    const Drawn d = st.hit; // (hitTest replaces st.hit)
    // A case: of the candidates (from `rank` of the way down their list on), the first whose ray (`ray`: from its middle,
    // along its outward normal, or level from behind) meets it before anything else of its own model.
    za::Vector<int> list;
    // `throughPack`: only one with a pack first on its ray (passed through: the stroke is the body's).
    auto run = [&](const char* what, float rank, bool behind, bool stroke, bool throughPack = false) {
        const za::SizeT first = static_cast<za::SizeT>((1.f - rank) * static_cast<float>(list.size()));
        for(za::SizeT i = first; i < list.size() && (throughPack || i < first + 40); i++)
        {
            const int tri = list[i];
            const glm::vec3 a = d.tris[static_cast<za::SizeT>(tri * 3)], b = d.tris[static_cast<za::SizeT>(tri * 3 + 1)],
                            c = d.tris[static_cast<za::SizeT>(tri * 3 + 2)];
            const glm::vec3 p = (a + b + c) / 3.f;
            glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
            n = glm::dot(n, p - mid) < 0.f ? -n : n; // (outwards: away from the body's middle)
            const glm::vec3 from = behind ? p - fwd * 40.f : p + n * 8.f, dir = behind ? fwd : -n;
            if(firstOwn(d, from, dir) == tri && (!throughPack || [&] {
                    const Hit any = hitTest(from, dir, 3);
                    return any.skin && any.skin->key == keyPack;
                }()))
            {
                (void)pickCase(what, num, tri, from, dir, stroke);
                return;
            }
        }
        (void)pickCase(what, num, -1, origin, fwd, false);
    };
    const glm::vec3 right = glm::normalize(glm::vec3{r[0], r[1], r[2]});
    // A hand: the arm's triangles farthest out to its side; the forearm: three quarters of the way out.
    candidates(d, VR_REGION_ARM, mid, right, 1e30f, list);
    run("right hand", 1.f, false, true);
    run("forearm", 0.75f, false, false);
    candidates(d, VR_REGION_ARM, mid, -right, 1e30f, list);
    run("left hand", 1.f, false, false);
    // The back of the head: its rearmost triangles, a level ray from behind (as a hand behind it aims).
    candidates(d, VR_REGION_HEAD, origin, -fwd, 1e30f, list);
    run("back of the head", 1.f, true, false);
    // Beside the pack: the body's rearmost triangles below the head's height (where a pack sits on the back).
    candidates(d, VR_REGION_BODY, origin, -fwd, 20.f, list);
    run("back beside the pack", 1.f, true, false);
    // Behind the pack: the body's triangles a pack hides from a level ray from behind (none: no pack worn).
    candidates(d, VR_REGION_BODY, origin, -fwd, 1e30f, list); // (a grunt's pack sits high: its shoulders too)
    run("back behind the pack", 1.f, true, false, true);
}

// studioFrame: the test under way.
void pickFrame()
{
    PickTest& t = st.pick;
    if(!t.on)
    {
        return;
    }
    const int key = t.kind;
    za::Vector<int> row;
    for(int i = 1; i < cl.num_entities && i < cl_max_edicts; i++)
    {
        if(keyOf(lookOf(i)) == key && drawnOf(i, 1, st.cand))
        {
            row.pushBack(i);
        }
    }
    if(static_cast<int>(row.size()) < 5)
    {
        t.settled = 0;
        if(++t.frames > pickWaitFrames)
        {
            t.tally.check(false);
            Con_Printf("humstudio_picktest: FAIL kind %d: timed out (%d of its five drawn)\n", t.kind, static_cast<int>(row.size()));
            t.frames = 0;
            t.kind = t.lastKind + 1; // (ends it)
        }
        else
        {
            return;
        }
    }
    else if(++t.settled < pickSettleFrames)
    {
        return;
    }
    else
    {
        Con_Printf("humstudio_picktest: kind %d drawn after %d frames\n", t.kind, t.frames);
        t.settled = 0;
        for(const int num : row)
        {
            pickEntity(num);
        }
        t.frames = 0;
        t.kind++;
    }
    if(t.kind > t.lastKind || !callLineup("VR_HUM_LineupKind", t.kind))
    {
        t.on = false;
        Con_Printf("humstudio_picktest: %d cases skipped (no such part on the rig)\n", t.skipped);
        t.tally.verdict("humstudio_picktest");
    }
}

// vr_humstudio_picktest [first [last]]: the kinds (0 grunt .. 3 death knight) lined up and picked in turn.
void pickTest_f()
{
    st.pick = PickTest{};
    st.pick.kind = Cmd_Argc() > 1 ? za::clamp(atoi(Cmd_Argv(1)), 0, 3) : 0;
    st.pick.lastKind = Cmd_Argc() > 2 ? za::clamp(atoi(Cmd_Argv(2)), st.pick.kind, 3) : 3;
    st.pick.on = callLineup("VR_HUM_LineupKind", st.pick.kind);
    if(!st.pick.on)
    {
        st.pick.tally.check(false, "humstudio_picktest", "no lineup");
        st.pick.tally.verdict("humstudio_picktest");
    }
}

void save_f()
{
    const bool a = saveTable();
    const bool b = saveMasks();
    st.unsaved = !(a && b);
    Con_Printf("humstudio: %s %s, %s %s\n", tableName(), a ? "saved" : "NOT saved", masksName(), b ? "saved" : "NOT saved");
}

void ownRows_f()
{
    Skin* s = editedSkin();
    if(!s)
    {
        Con_Printf("humstudio: no skin of that kind drawn yet (Line Up first)\n");
        return;
    }
    pushUndo(s, s->key);
    if(!s->ownRows)
    {
        memcpy(s->rows, keyRows(s->key), sizeof(s->rows));
    }
    s->ownRows = !s->ownRows;
    s->dirty = true;
    Con_Printf("humstudio: %s skin %d: %s\n", s->model.cStr(), s->skin, s->ownRows ? "its own rows" : "its kind's rows");
}

void clearPaint_f()
{
    Skin* s = editedSkin();
    if(!s)
    {
        return;
    }
    pushUndo(s, s->key);
    for(unsigned char& p : s->paint)
    {
        p = ClassNone;
    }
    s->dirty = true;
}

// vr_humstudio_stroke <1|-1|0> [hand]: a stroke begun (painting, erasing) or ended: a key bound to it, the tests (hand 0
// the off hand's ray, 1 the main hand's, 2 the view's centre; flat: always the view's).
void stroke_f()
{
    st.stroke = Cmd_Argc() > 1 ? za::clamp(atoi(Cmd_Argv(1)), -1, 1) : 0;
    st.strokeHand = Cmd_Argc() > 2 ? za::clamp(atoi(Cmd_Argv(2)), 0, 2) : 1;
    st.strokeUndone = false;
    const char* what = strokeFrame();
    Con_Printf("humstudio_stroke %d: %s, undo steps %d\n", st.stroke, what, static_cast<int>(st.undo.size()));
}

// vr_humstudio_check: the masks against the skins, a line: per mask its clothes and pack texels, and how many of them are on
// the flesh's and blood's rows (7, 4) by their rows (a texel painted by hand is meant: not counted): "humstudio_check: N
// masks, M on flesh" (the tests' verdict reads it).
void check_f()
{
    int masks = 0, flesh = 0, bare = 0;
    for(const Skin& s : allSkins())
    {
        if(s.texels.empty())
        {
            continue;
        }
        masks++;
        int dyed = 0;
        for(int t = 0; t < s.w * s.h; t++)
        {
            dyed += classAt(s, t) != 0 ? 1 : 0;
        }
        bare += dyed == 0 ? 1 : 0;
        int n[3] = {}, f = 0;
        for(int t = 0; t < s.w * s.h; t++)
        {
            const unsigned c = classAt(s, t);
            n[c]++;
            const int row = s.texels[static_cast<za::SizeT>(t)] >> 4;
            f += c != 0 && s.paint[static_cast<za::SizeT>(t)] == ClassNone && (row == 7 || row == 4) ? 1 : 0; // (painted: meant)
        }
        flesh += f;
        Con_Printf("humstudio_check: %s skin %d %s: clothes %d pack %d left %d, on flesh rows %d\n", s.model.cStr(), s.skin,
            s.key == keyPack ? "pack" : "body", n[1], n[2], n[0], f);
    }
    const int want = Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : 1;
    Con_Printf("humstudio_check: %d masks (want %d), %d on flesh, %d dyeing nothing: %s\n", masks, want, flesh, bare,
        masks >= want && flesh == 0 && bare == 0 ? "PASS" : "FAIL");
}

} // namespace

void studioInit()
{
    Cmd_AddCommand("vr_humstudio_lineup", lineup_f);
    Cmd_AddCommand("vr_humstudio_save", save_f);
    Cmd_AddCommand("vr_humstudio_undo", undo_f);
    Cmd_AddCommand("vr_humstudio_ownrows", ownRows_f);
    Cmd_AddCommand("vr_humstudio_clearpaint", clearPaint_f);
    Cmd_AddCommand("vr_humstudio_stroke", stroke_f);
    Cmd_AddCommand("vr_humstudio_check", check_f);
    Cmd_AddCommand("vr_humstudio_picktest", pickTest_f);
}

void studioFrame()
{
    if(cls.state != ca_connected)
    {
        st.stroke = 0;
        st.pick.on = false;
        return;
    }
    syncSliders();
    if(vr_humstudio_turn.value != st.turn && sv.active)
    {
        (void)callLineup("VR_HUM_LineupTurn", selKind()); // (the turntable: the lineup turned in place)
    }
    pickFrame();
    strokeFrame();
}

bool studioOverlay()
{
    return vr_humstudio_overlay.value != 0.f;
}

bool studioCursorOn()
{
    return st.cursorSkin >= 0;
}

// The skin view: the skin edited (the cursor's, else the last painted or its type's first) flat in the top right, its
// texels in their colours, the clothes' tinted green, the pack's blue, the brush's yellow: where the cursor's texels lie
// on the skin (a texel shared by triangles far apart shows once here, and on each of them on the model).
void studioDraw2D()
{
    if(vr_humstudio_paint.value == 0.f || vr_humstudio_cursor.value == 0.f || cls.state != ca_connected || key_dest != key_game)
    {
        return;
    }
    za::Vector<Skin>& skins = allSkins();
    Skin* s = st.cursorSkin >= 0 && st.cursorSkin < static_cast<int>(skins.size()) ? &skins[static_cast<za::SizeT>(st.cursorSkin)]
                                                                                    : editedSkin();
    if(!s || s->texels.empty() || s->key < 0 || s->key >= numKeys)
    {
        return;
    }
    static constexpr const char* names[numKeys] = {"humstudio_grunt", "humstudio_enforcer", "humstudio_knight",
        "humstudio_dknight", "humstudio_pack"};
    gfx::Picture& pic = st.pictures[s->key];
    gfx::ensurePicture(pic, s->w, s->h, names[s->key]);
    if(!pic.texture)
    {
        return;
    }
    st.mark.resize(static_cast<za::SizeT>(s->w * s->h), 0);
    for(const int t : s->cursor)
    {
        st.mark[static_cast<za::SizeT>(t)] = 1;
    }
    st.rgba.resize(static_cast<za::SizeT>(pic.width * pic.height * 4));
    for(int y = 0; y < pic.height; y++)
    {
        for(int x = 0; x < pic.width; x++)
        {
            const int t = (y * s->h / pic.height) * s->w + x * s->w / pic.width; // (another skin of its key's size: stretched)
            const unsigned c = d_8to24table[s->texels[static_cast<za::SizeT>(t)]];
            float rgb[3] = {static_cast<float>(c & 255u), static_cast<float>((c >> 8) & 255u), static_cast<float>((c >> 16) & 255u)};
            const unsigned klass = classAt(*s, t);
            const float tint[3][3] = {{0.f, 0.f, 0.f}, {30.f, 230.f, 60.f}, {50.f, 100.f, 255.f}};
            const float k = st.mark[static_cast<za::SizeT>(t)] ? 0.8f : klass != 0 ? 0.4f : 0.f;
            for(int i = 0; i < 3; i++)
            {
                const float to = st.mark[static_cast<za::SizeT>(t)] ? (i == 2 ? 30.f : 240.f) : tint[klass][i];
                rgb[i] = rgb[i] + (to - rgb[i]) * k;
            }
            unsigned char* px = &st.rgba[static_cast<za::SizeT>((y * pic.width + x) * 4)];
            px[0] = static_cast<unsigned char>(rgb[0]);
            px[1] = static_cast<unsigned char>(rgb[1]);
            px[2] = static_cast<unsigned char>(rgb[2]);
            px[3] = 255;
        }
    }
    for(const int t : s->cursor)
    {
        st.mark[static_cast<za::SizeT>(t)] = 0;
    }
    gfx::updatePicture(pic, st.rgba.data());
    GL_SetCanvas(CANVAS_TOPRIGHT);
    const float w = 150.f, h = w * static_cast<float>(s->h) / static_cast<float>(s->w), x = 320.f - 4.f - w, y = 16.f;
    const float frame[3] = {0.f, 0.f, 0.f};
    Draw_FillEx(x - 1.f, y - 1.f, w + 2.f, h + 2.f, frame, 0.8f);
    Draw_TextureRect(x, y, w, h, static_cast<gltexture_t*>(pic.texture), 1.f);
    Draw_StringEx(x, y + h + 2.f, 6.f, va("%s %d texels", s->model.cStr(), static_cast<int>(s->cursor.size())));
}

const char* studioStatus()
{
    const Skin* s = editedSkin();
    q_snprintf(statusText, sizeof(statusText), "%s%s; undo %d%s", s ? s->model.cStr() : "no skin of this kind drawn yet",
        s ? (s->ownRows ? " (its own rows)" : " (its kind's rows)") : "", static_cast<int>(st.undo.size()),
        st.unsaved ? "; not saved" : "");
    return statusText;
}

const char* studioRowText(int row)
{
    Skin* s = editedSkin();
    const unsigned char* rows = s && s->ownRows ? s->rows : keyRows(selKey());
    int count = 0;
    for(za::SizeT t = 0; s && t < s->texels.size(); t++)
    {
        count += (s->texels[t] >> 4) == row ? 1 : 0;
    }
    q_snprintf(rowText, sizeof(rowText), "Row %d (%d-%d): %s, %d texels", row, row * 16, row * 16 + 15,
        rows ? rowClassNames[za::min(static_cast<int>(rows[row]), 2)] : "?", count);
    return rowText;
}

void studioRowPick(int row)
{
    Skin* s = editedSkin();
    const int key = selKey();
    unsigned char* rows = s && s->ownRows ? s->rows : keyRows(key);
    if(!rows || row < 0 || row > 15)
    {
        return;
    }
    pushUndo(s, key);
    rows[row] = static_cast<unsigned char>((rows[row] + 1) % 3);
    markKeyDirty(key);
}

bool studioButton(int hand, int button, bool down)
{
    if(vr_humstudio_paint.value == 0.f || (down && key_dest != key_game))
    {
        return false;
    }
    const auto b = static_cast<posing::Button>(button);
    if(b == posing::Button::Trigger || b == posing::Button::Grip)
    {
        st.stroke = down ? (b == posing::Button::Trigger ? 1 : -1) : 0;
        st.strokeHand = hand;
        st.strokeUndone = false;
        return true;
    }
    if(b == posing::Button::Secondary)
    {
        if(down)
        {
            undo_f();
        }
        return true;
    }
    return false;
}

bool studioAttack(bool down)
{
    if(vr_humstudio_paint.value == 0.f || vrActive() || (down && key_dest != key_game))
    {
        return false;
    }
    st.stroke = down ? 1 : 0;
    st.strokeHand = 1;
    st.strokeUndone = false;
    return true;
}

} // namespace qvr::humlook

extern "C" void VR_HumStudioDraw2D(void)
{
    qvr::humlook::studioDraw2D();
}

extern "C" int VR_HumStudioAttack(int down)
{
    return qvr::humlook::studioAttack(down != 0) ? 1 : 0;
}
