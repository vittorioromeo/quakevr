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
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_posing.hpp"
#include "vr_progs.hpp"

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

struct Hit
{
    Skin* skin{nullptr};
    float s{0.f}, t{0.f}; // the skin's texel coordinates hit
};

// The nearest armed humanoid's skin texel on the ray (its pose this frame's first, as drawn but not interpolated).
[[nodiscard]] Hit hitTest(const glm::vec3& from, const glm::vec3& dir)
{
    Hit best;
    float bestT = 1e30f;
    if(!cl_entities)
    {
        return best;
    }
    for(int i = 1; i < cl.num_entities && i < cl_max_edicts; i++)
    {
        entity_t* e = &cl_entities[i];
        const int key = keyOf(lookOf(i));
        if(key < 0 || !e->model || e->model->type != mod_alias)
        {
            continue;
        }
        Skin* sk = skinFor(e->model, e->skinnum, key);
        const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e->model));
        if(!sk || !hdr || hdr->numframes <= 0 || hdr->skinwidth != sk->w || hdr->skinheight != sk->h)
        {
            continue;
        }
        vec3_t ang = {-e->angles[0], e->angles[1], e->angles[2]}, f, r, u;
        AngleVectors(ang, f, r, u);
        const glm::vec3 F{f[0], f[1], f[2]}, R{r[0], r[1], r[2]}, U{u[0], u[1], u[2]};
        const glm::vec3 rel = from - glm::vec3{e->origin[0], e->origin[1], e->origin[2]};
        const glm::vec3 o{glm::dot(rel, F), -glm::dot(rel, R), glm::dot(rel, U)};
        const glm::vec3 d{glm::dot(dir, F), -glm::dot(dir, R), glm::dot(dir, U)};
        const auto* base = reinterpret_cast<const byte*>(hdr);
        const int frame = za::clamp(e->frame, 0, hdr->numframes - 1);
        const int pose = za::clamp(hdr->frames[frame].firstpose, 0, hdr->numposes - 1);
        const auto* verts = reinterpret_cast<const trivertx_t*>(base + hdr->vertexes) + pose * hdr->numverts;
        const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
        const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
        auto at = [&](int k) {
            const trivertx_t& v = verts[za::min(static_cast<int>(mesh[indexes[k]].vertindex), hdr->numverts - 1)];
            return glm::vec3{v.v[0] * hdr->scale[0] + hdr->scale_origin[0], v.v[1] * hdr->scale[1] + hdr->scale_origin[1],
                v.v[2] * hdr->scale[2] + hdr->scale_origin[2]};
        };
        for(int k = 0; k + 2 < hdr->numindexes; k += 3)
        {
            const glm::vec3 a = at(k), b = at(k + 1), c = at(k + 2);
            const glm::vec3 e1 = b - a, e2 = c - a, p = glm::cross(d, e2);
            const float det = glm::dot(e1, p);
            if(det > -1e-9f && det < 1e-9f)
            {
                continue;
            }
            const glm::vec3 q = o - a;
            const float bu = glm::dot(q, p) / det;
            const glm::vec3 qq = glm::cross(q, e1);
            const float bv = glm::dot(d, qq) / det;
            const float t = glm::dot(e2, qq) / det;
            if(bu < 0.f || bv < 0.f || bu + bv > 1.f || t <= 0.f || t >= bestT)
            {
                continue;
            }
            bestT = t;
            const float bw = 1.f - bu - bv;
            best.skin = sk;
            best.s = bw * mesh[indexes[k]].st[0] + bu * mesh[indexes[k + 1]].st[0] + bv * mesh[indexes[k + 2]].st[0] + 0.5f;
            best.t = bw * mesh[indexes[k]].st[1] + bu * mesh[indexes[k + 1]].st[1] + bv * mesh[indexes[k + 2]].st[1] + 0.5f;
        }
    }
    return best;
}

// The brush at a skin's texel: painted the class picked (or erased: its row's again). The texels changed.
int brush(Skin& s, float cs, float ct, int value)
{
    const int r = za::clamp(static_cast<int>(vr_humstudio_brush.value + 0.5f), 0, 32);
    const int x0 = static_cast<int>(cs), y0 = static_cast<int>(ct);
    int changed = 0;
    for(int y = za::max(y0 - r, 0); y <= za::min(y0 + r, s.h - 1); y++)
    {
        for(int x0w = x0 - r; x0w <= x0 + r; x0w++)
        {
            const int x = (x0w % s.w + s.w) % s.w;
            if((x0w - x0) * (x0w - x0) + (y - y0) * (y - y0) > r * r)
            {
                continue;
            }
            unsigned char& p = s.paint[static_cast<za::SizeT>(y * s.w + x)];
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
    }
    s.dirty = s.dirty || changed > 0;
    return changed;
}

// This frame's stroke; what it did (the tests' line).
const char* strokeFrame()
{
    if(st.stroke == 0 || vr_humstudio_paint.value == 0.f)
    {
        return "no stroke";
    }
    if(key_dest != key_game)
    {
        return "not in the game (a menu or the console up)";
    }
    glm::vec3 from, dir;
    if(!aimRay(st.strokeHand, from, dir))
    {
        return "no ray";
    }
    const Hit h = hitTest(from, dir);
    if(!h.skin)
    {
        return va("no armed humanoid on the ray from %.0f %.0f %.0f along %.2f %.2f %.2f", from.x, from.y, from.z, dir.x,
            dir.y, dir.z);
    }
    const int value = st.stroke > 0 ? za::clamp(static_cast<int>(vr_humstudio_class.value), 0, 3) : ClassNone;
    const int changed = brush(*h.skin, h.s, h.t, value);
    st.skin = static_cast<int>(h.skin - allSkins().data());
    return va("%s texel %.0f %.0f: %d changed", h.skin->model.cStr(), h.s, h.t, changed);
}

void lineup_f()
{
    if(!sv.active)
    {
        Con_Printf("humstudio: a map with you in it is needed\n");
        return;
    }
    const progs::ServerVm vm;
    const func_t fn = progs::findFunction("VR_HUM_LineupKind");
    edict_t* player = progs::playerEdict();
    if(!fn || !player)
    {
        Con_Printf("humstudio: no lineup (QC VR_HUM_LineupKind)\n");
        return;
    }
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(player);
    G_INT(OFS_PARM0) = EDICT_TO_PROG(player);
    G_FLOAT(OFS_PARM1) = static_cast<float>(Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : selKind());
    PR_ExecuteProgram(fn);
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
// the flesh's and blood's rows (7, 4): "humstudio_check: N masks, M on flesh" (the tests' verdict reads it).
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
            f += c != 0 && (row == 7 || row == 4) ? 1 : 0;
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
}

void studioFrame()
{
    if(cls.state != ca_connected)
    {
        st.stroke = 0;
        return;
    }
    syncSliders();
    strokeFrame();
}

bool studioOverlay()
{
    return vr_humstudio_overlay.value != 0.f;
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

extern "C" int VR_HumStudioAttack(int down)
{
    return qvr::humlook::studioAttack(down != 0) ? 1 : 0;
}
