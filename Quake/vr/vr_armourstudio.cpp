// vr_armourstudio.cpp -- see vr_armourstudio.hpp (the Armour Studio).

#include "vr_armourstudio.hpp"
#include "vr_agarmor.hpp"
#include "vr_agarmor_internal.hpp"
#include "vr_armourdata.hpp"
#include "vr_beastarmour.hpp"
#include "vr_bodyclone.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_menu.hpp"
#include "vr_progs.hpp"
#include "vr_ragdoll.hpp"
#include "vr_testkit.hpp"
#include "vr_view.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "vr_api.h"

#include <glm/glm.hpp>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" int VR_IsActive(void);

namespace qvr::armourstudio
{

namespace
{

using namespace armourdata;
using agarmor::detail::at;
using agarmor::detail::count;

constexpr float subjectAhead = 72.f;   // units in front of you
constexpr double undoMerge = 1.5;      // seconds: a slider's run of steps on one value is one undo step
constexpr int subjectKinds = 4;        // (QC's VR_AM_GRUNT .. VR_AM_DKNIGHT; the player's kind has none)

cvar_t* const fieldCvars[NumFields] = {&vr_armourstudio_offset_x, &vr_armourstudio_offset_y, &vr_armourstudio_offset_z,
    &vr_armourstudio_pitch, &vr_armourstudio_yaw, &vr_armourstudio_roll, &vr_armourstudio_scale, &vr_armourstudio_standoff,
    &vr_armourstudio_start, &vr_armourstudio_end, &vr_armourstudio_rows, &vr_armourstudio_taper_start, &vr_armourstudio_taper_end,
    &vr_armourstudio_leather, &vr_armourstudio_folds, &vr_armourstudio_fold_depth, &vr_armourstudio_hero, &vr_armourstudio_sag,
    &vr_armourstudio_scale_x, &vr_armourstudio_scale_y, &vr_armourstudio_scale_z};

// The subject's model's animations: its frames grouped by name ("stand1".."stand8": stand).
constexpr int maxAnims = 64;
struct Anim
{
    char name[16];
    int first, frames;
};
struct Anims
{
    const qmodel_t* model{nullptr}; // (the model they were read from)
    Anim list[maxAnims]{};
    int count{0};
    int group{-1};    // the one playing
    double pos{0.0};  // frames into it
};
Anims anims;

struct State
{
    bool running{false};
    int subject{0};       // its entity (0 none)
    int subjectKind{-1};  // the kind it is
    int subjectTier{-1};
    int subjectWorn{-1};  // (the pieces it was given)
    char subjectMap[64]{}; // (the map it was put in: a new map, it is gone)
    float placed[3]{};     // where it was put, and the way from you to it (flat): vr_armourstudio_subject_* move it from there
    float placedFwd[2]{1.f, 0.f};
    float freezeWas{0.f};  // vr_agrunt_freeze before the studio
    int selKind{-1}, selPart{-1}, selSide{-1}, selGen{-1};
    float shown[NumFields]{};  // the cvars' values as last written or read
    int ownGen{-1};            // the table's generation after the studio's last change
    int savedGen{-1};          // ... as saved or read
    int undoField{-1}, undoSel{-1};
    double undoAt{-1e9};
    char statusText[256]{};
    char animText[160]{};
    bool playerShown{false}; // the player's kind shown last frame (playerArmour; its tier taken from yours as it first shows)
    bool menuSeen{false};    // the VR menu has been open since it started (closing it then closes the studio: autoClose)
};
State st;

// ---- The test (vr_armour_test): its steps across frames ----
struct Test
{
    int step{0};
    double since{0.0}; // (realtime the step began)
    testkit::Tally tally;
    uint32_t hashEmpty{0}, hashTweaked{0}, hashHand{0}, hashShipped{0};
    int genBefore{0}; // (the table's generation before the hand-edited file: the watcher's read bumps it)
    char fileWas[64]{};
    char mapWas[64]{};
};
Test test;

// ---- The subjects (vr_armourstudio_kind): the humanoids' kinds (armourdata's 0-4: grunt .. player), AD's two knights (one
// set of tweaks while their rigs match: armourdata::adShared), then the beasts (vr_beastarmour.hpp, beastarmour.txt's order) ----
constexpr int subjectAdGuard = 5;
constexpr int subjectAdCrossbow = 6;
constexpr int subjectBeasts = 7;
constexpr const char* adModels[2] = {"progs/vrdguard.mdl", "progs/vrdcrossbow.mdl"};
constexpr const char* adLabels[2] = {"Death Guard", "Crossbow Knight"};
constexpr int adQcKinds[2] = {4, 5}; // (QC's VR_AM_DGUARD, VR_AM_DCROSSBOW)
constexpr const char* humanLabels[5] = {"Grunt", "Enforcer", "Knight", "Death Knight", "Player (Your Body)"};
constexpr const char* beastLabels[][2] = {{"progs/dog.mdl", "Rottweiler"}, {"progs/ogre.mdl", "Ogre"}, {"progs/wizard.mdl", "Scrag"},
    {"progs/demon.mdl", "Fiend"}, {"progs/shambler.mdl", "Shambler"}};

[[nodiscard]] int subjectNow()
{
    return za::clamp(static_cast<int>(vr_armourstudio_kind.value), 0, subjectCount() - 1);
}

// The beast a subject is (beastarmour's kind; -1 none).
[[nodiscard]] int beastOf(int subject)
{
    return subject >= subjectBeasts && subject - subjectBeasts < beastarmour::kindCount() ? subject - subjectBeasts : -1;
}

// The table's entry a subject edits: its kind's name, AD's shared "ad_knight" (else its model's own), a beast's model.
[[nodiscard]] const char* entryOf(int subject)
{
    if(subject == subjectAdGuard || subject == subjectAdCrossbow)
    {
        return adShared() ? kindNames[KindAdKnight] : adModels[subject - subjectAdGuard];
    }
    const int beast = beastOf(subject);
    return beast >= 0 ? beastarmour::kindModel(beast) : kindNames[za::clamp(subject, 0, static_cast<int>(KindPlayer))];
}

// The player's kind on show: the ImGui window open, or the menu's Armour Studio page, on Kind = Player (playerArmour).
[[nodiscard]] bool playerShown()
{
    return subjectNow() == KindPlayer && (windowOpen() || menu::armourStudioPage());
}

// As it first shows, the studio's tier is the armour you wear (none: the tier picked stays).
void notePlayerShown()
{
    const bool shown = playerShown();
    const int worn = cls.state == ca_connected && cl.viewentity > 0 ? agarmor::clientState(cl.viewentity, 0) : 0;
    if(shown && !st.playerShown && worn >= 1 && worn <= 3)
    {
        Cvar_SetValueQuick(&vr_armourstudio_tier, static_cast<float>(worn));
    }
    st.playerShown = shown;
}

[[nodiscard]] int selectedSide()
{
    const int part = za::clamp(static_cast<int>(vr_armourstudio_part.value), 0, NumParts - 1);
    return !limbPart(part) ? SideBoth : za::clamp(static_cast<int>(vr_armourstudio_side.value), 0, 2);
}

[[nodiscard]] const char* kindName()
{
    return entryOf(subjectNow());
}

// The value shown for a field of the kind's part and side: its own, else (a side) the both-sides value (mirrored on the right).
[[nodiscard]] float shownValue(int part, int side, int field)
{
    const uint32_t bit = 1u << static_cast<unsigned>(field);
    const Override* own = entry(kindName(), part, side, false);
    const Override* both = side != SideBoth ? entry(kindName(), part, SideBoth, false) : nullptr;
    if(own && (own->has & bit))
    {
        return own->v[field];
    }
    if(both && (both->has & bit))
    {
        const bool mirror = side == SideRight && (field == OffsetY || field == Yaw || field == Roll);
        return mirror ? -both->v[field] : both->v[field];
    }
    // (a joint's folds not set: the global settings')
    return field == Folds ? vr_agarmor_folds.value : field == FoldDepth ? vr_agarmor_fold_depth.value : fieldDefault(field);
}

void showFields()
{
    for(int f = 0; f < NumFields; f++)
    {
        Cvar_SetValueQuick(fieldCvars[f], shownValue(st.selPart, st.selSide, f));
        st.shown[f] = fieldCvars[f]->value;
    }
}

// The pieces (agarmor Piece bits) a part and side are drawn on, and whether a sleeve.
[[nodiscard]] int piecesOf(int part, int side, bool& sleeve)
{
    sleeve = sleevePart(part);
    if(part == PartCuirass || part == PartCuirassSleeve)
    {
        return 1 << agarmor::Cuirass;
    }
    if(part == PartHelmet || part == PartHelmetSleeve)
    {
        return 1 << agarmor::Helmet;
    }
    if(part >= PartPlate8)
    {
        // (a beast's plates 8 and 9, their junctions)
        return 1 << (part == PartPlate8 || part == PartPlate8Sleeve ? 8 : 9);
    }
    const int left = agarmor::PauldronL + 2 * ((sleeve ? part - PartPauldronSleeve : part - PartPauldron));
    return side == SideLeft ? 1 << left : side == SideRight ? 1 << (left + 1) : (1 << left) | (1 << (left + 1));
}

// The sliders against the table: a new selection (or the table changed elsewhere) shows its values; a moved slider sets its value.
void syncFields()
{
    const int kind = subjectNow();
    const int part = za::clamp(static_cast<int>(vr_armourstudio_part.value), 0, NumParts - 1);
    const int side = selectedSide();
    const bool beast = beastOf(kind) >= 0;
    if(generation() != st.ownGen && generation() != st.selGen)
    {
        st.savedGen = generation(); // (read from the file: what is on disk)
    }
    if(kind != st.selKind || part != st.selPart || side != st.selSide || generation() != st.selGen)
    {
        st.selKind = kind;
        st.selPart = part;
        st.selSide = side;
        st.selGen = generation();
        showFields();
        return;
    }
    bool any = false;
    for(int f = 0; f < NumFields && partValid(kind, part); f++)
    {
        if(fieldCvars[f]->value == st.shown[f])
        {
            continue;
        }
        const int sel = (kind * NumParts + part) * NumSides + side;
        if(!any && (f != st.undoField || sel != st.undoSel || realtime - st.undoAt > undoMerge))
        {
            pushUndo();
        }
        st.undoField = f;
        st.undoSel = sel;
        st.undoAt = realtime;
        any = true;
        Override* o = entry(kindName(), part, side, true);
        const int first = f <= OffsetZ ? OffsetX : f <= Roll ? Pitch : f;
        const int last = f <= Roll ? first + 2 : f;
        for(int g = first; g <= last; g++)
        {
            o->v[g] = fieldCvars[g]->value;
            o->has |= 1u << static_cast<unsigned>(g);
            st.shown[g] = fieldCvars[g]->value;
        }
        // (rows 0: the generator's)
        o->has &= f == Rows && o->v[Rows] < 0.5f ? ~(1u << static_cast<unsigned>(Rows)) : ~0u;
        // (a taper of 1, the plates' or a sleeve's: as made, not written to the file)
        const bool taper = f == TaperStart || f == TaperEnd;
        o->has &= taper && za::abs(o->v[f] - 1.f) < 1e-4f ? ~(1u << static_cast<unsigned>(f)) : ~0u;
        // (leather 1, or on a part with no sleeve: as made; 0 hides a sleeve's)
        o->v[Leather] = f == Leather ? (o->v[Leather] < 0.5f ? 0.f : 1.f) : o->v[Leather];
        o->has &= f == Leather && (o->v[Leather] > 0.5f || !sleevePart(part)) ? ~(1u << static_cast<unsigned>(Leather)) : ~0u;
        // (hero plates 1, or on a part with none (the helmet, the thighs, the sleeves): as made; 0 every plate its own)
        o->v[Hero] = f == Hero ? (o->v[Hero] < 0.5f ? 0.f : 1.f) : o->v[Hero];
        const bool heroPart = !beast && (part == PartCuirass || part == PartPauldron || part == PartBracer || part == PartShin);
        o->has &= f == Hero && (o->v[Hero] > 0.5f || !heroPart) ? ~(1u << static_cast<unsigned>(Hero)) : ~0u;
        // (the folds: whole, 0-4; as the global settings, or on a part with no leather: not written)
        o->v[Folds] = f == Folds ? za::clamp(static_cast<float>(static_cast<int>(o->v[Folds] + 0.5f)), 0.f, 4.f) : o->v[Folds];
        const float global = f == Folds ? vr_agarmor_folds.value : vr_agarmor_fold_depth.value;
        o->has &= (f == Folds || f == FoldDepth) && (za::abs(o->v[f] - global) < 1e-4f || !sleevePart(part)) ? ~(1u << static_cast<unsigned>(f)) : ~0u;
        // (a beast junction's sag 0, or on a part with no junction: not written)
        o->has &= f == Sag && (za::abs(o->v[Sag]) < 1e-4f || !sleevePart(part) || !beast) ? ~(1u << static_cast<unsigned>(Sag)) : ~0u;
    }
    if(any)
    {
        changed();
        st.ownGen = st.selGen = generation();
    }
}

void freeSubject()
{
    if(sv.active)
    {
        const progs::ServerVm vm;
        const func_t fn = progs::findFunction("VR_ArmourStudio_Spawn");
        edict_t* player = progs::playerEdict();
        if(fn && player)
        {
            pr_global_struct->time = qcvm->time;
            pr_global_struct->self = EDICT_TO_PROG(player);
            G_FLOAT(OFS_PARM0) = -1.f;
            G_FLOAT(OFS_PARM1) = 0.f;
            PR_ExecuteProgram(fn);
        }
    }
    st.subject = 0;
    st.subjectKind = -1;
    agarmor::studioView(0, 1.f, 0, false);
}

// The kind put ahead of you (its armour given), or none for the player's kind.
void spawnSubject()
{
    const int kind = subjectNow();
    freeSubject();
    st.subjectKind = kind;
    st.subjectTier = -1;
    st.subjectWorn = -1;
    const int beast = beastOf(kind);
    const int qcKind = kind < subjectKinds ? kind : kind == subjectAdGuard || kind == subjectAdCrossbow ? adQcKinds[kind - subjectAdGuard] : -1;
    if((qcKind < 0 && beast < 0) || !sv.active)
    {
        return;
    }
    int num = 0;
    {
        const progs::ServerVm vm;
        // (a beast: by its model, VR_ArmourStudio_SpawnBeast; a humanoid: its row)
        const func_t fn = progs::findFunction(beast >= 0 ? "VR_ArmourStudio_SpawnBeast" : "VR_ArmourStudio_Spawn");
        edict_t* player = progs::playerEdict();
        if(fn && player)
        {
            pr_global_struct->time = qcvm->time;
            pr_global_struct->self = EDICT_TO_PROG(player);
            if(beast >= 0)
            {
                G_INT(OFS_PARM0) = PR_SetEngineString(beastarmour::kindModel(beast)); // (kept for good: vr_beastarmour.cpp)
            }
            else
            {
                G_FLOAT(OFS_PARM0) = static_cast<float>(qcKind);
            }
            G_FLOAT(OFS_PARM1) = subjectAhead;
            PR_ExecuteProgram(fn);
            edict_t* made = PROG_TO_EDICT(G_INT(OFS_RETURN));
            num = made && made != sv.qcvm.edicts ? NUM_FOR_EDICT(made) : 0;
        }
    }
    st.subject = num;
    q_strlcpy(st.subjectMap, sv.name, sizeof(st.subjectMap));
    if(num)
    {
        const progs::ServerVm vm;
        const edict_t* e = progs::edictAt(num);
        const edict_t* player = progs::playerEdict();
        if(e && player)
        {
            float dx = e->v.origin[0] - player->v.origin[0], dy = e->v.origin[1] - player->v.origin[1];
            const float len = za::sqrt(dx * dx + dy * dy);
            dx = len > 1e-3f ? dx / len : 1.f;
            dy = len > 1e-3f ? dy / len : 0.f;
            st.placedFwd[0] = dx;
            st.placedFwd[1] = dy;
            for(int c = 0; c < 3; c++)
            {
                st.placed[c] = e->v.origin[c];
            }
        }
    }
    Con_Printf(num ? "armourstudio: a %s ahead (entity %d)\n" : "armourstudio: no %s made (a map with you in it needed)\n", subjectLabel(kind), num);
}

// The model's animations read again (a new model).
void readAnims(const qmodel_t* m)
{
    anims.model = m;
    anims.count = 0;
    anims.group = -1;
    const auto* hdr = m && m->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(m))) : nullptr;
    // (the generated vr models' frames have no names: the Quake model they were made from names them, frame for frame)
    constexpr const char* sources[][2] = {{"progs/vrgrunt.mdl", "progs/soldier.mdl"}, {"progs/vrenforcer.mdl", "progs/enforcer.mdl"},
        {"progs/vrknight.mdl", "progs/knight.mdl"}, {"progs/vrhknight.mdl", "progs/hknight.mdl"}};
    const aliashdr_t* named = hdr;
    for(const auto& s : sources)
    {
        const bool unnamed = hdr && hdr->numframes > 0 && (!hdr->frames[0].name[0] || !ZA_STRNCMP(hdr->frames[0].name, "frame", 5));
        qmodel_t* src = unnamed && !ZA_STRCMP(m->name, s[0]) ? Mod_ForName(s[1], false) : nullptr;
        named = src && src->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(src)) : named;
    }
    char lastKey[16]{};
    for(int i = 0; hdr && i < hdr->numframes; i++)
    {
        char name[16];
        q_strlcpy(name, named && i < named->numframes ? named->frames[i].name : hdr->frames[i].name, sizeof(name));
        za::SizeT n = ZA_STRLEN(name);
        while(n > 0 && ((name[n - 1] >= '0' && name[n - 1] <= '9') || name[n - 1] == '_'))
        {
            name[--n] = '\0';
        }
        Anim* last = anims.count > 0 ? &anims.list[anims.count - 1] : nullptr;
        if(last && !ZA_STRCMP(lastKey, name))
        {
            last->frames++;
            continue;
        }
        q_strlcpy(lastKey, name, sizeof(lastKey));
        if(anims.count >= maxAnims)
        {
            break;
        }
        Anim& a = anims.list[anims.count++];
        q_strlcpy(a.name, name[0] ? name : "frame", sizeof(a.name));
        a.first = i;
        a.frames = 1;
    }
}

// The subject's frame while vr_armourstudio_anim plays (the QC holds its stand frame otherwise: vr_armedgrunt_pose.qc):
// the animation picked, played at 10 frames a second times its speed, or paused on vr_armourstudio_anim_frame.
void playAnim(edict_t* e)
{
    const int mi = static_cast<int>(e->v.modelindex);
    const qmodel_t* m = mi > 0 && mi < MAX_MODELS ? sv.models[mi] : nullptr;
    if(m != anims.model)
    {
        readAnims(m);
    }
    if(!vr_armourstudio_anim.value || anims.count == 0)
    {
        anims.group = -1;
        return;
    }
    const int group = za::clamp(static_cast<int>(vr_armourstudio_anim_group.value), 0, anims.count - 1);
    const Anim& a = anims.list[group];
    if(group != anims.group)
    {
        anims.group = group;
        anims.pos = 0.0;
        Cvar_SetValueQuick(&vr_armourstudio_anim_frame, 0.f);
    }
    if(vr_armourstudio_anim_play.value)
    {
        anims.pos += host_frametime * 10.0 * static_cast<double>(vr_armourstudio_anim_speed.value);
        anims.pos -= static_cast<double>(a.frames) * floor(anims.pos / static_cast<double>(a.frames));
        const int shown = za::clamp(static_cast<int>(anims.pos), 0, a.frames - 1);
        if(static_cast<int>(vr_armourstudio_anim_frame.value) != shown)
        {
            Cvar_SetValueQuick(&vr_armourstudio_anim_frame, static_cast<float>(shown));
        }
    }
    else
    {
        anims.pos = za::clamp(static_cast<double>(static_cast<int>(vr_armourstudio_anim_frame.value)), 0.0, static_cast<double>(a.frames - 1));
    }
    e->v.frame = static_cast<float>(a.first + za::clamp(static_cast<int>(anims.pos), 0, a.frames - 1));
}

// Its tier, its turn (facing you, then turned), its look.
void keepSubject()
{
    if(!st.running)
    {
        return;
    }
    if(st.subject && (!sv.active || ZA_STRCMP(st.subjectMap, sv.name) != 0))
    {
        st.running = false; // (a new map: the studio closed)
        st.subject = 0;
        agarmor::studioView(0, 1.f, 0, false);
        // (only while still frozen: a scene that set it and put it back, or a map change that ended the scene, unfroze them
        // first: putting the studio's value back then froze the next scene's humanoids; his note of 2026-11-23 23:03:39)
        if(vr_agrunt_freeze.value != 0.f)
        {
            Cvar_SetValueQuick(&vr_agrunt_freeze, st.freezeWas);
        }
        return;
    }
    // (a new kind put there once you are in the level: a script's `map x; vr_armourstudio` asks before)
    const bool ready = sv.active && cls.signon == SIGNONS && progs::localPlayer() != nullptr;
    if(ready && subjectNow() != st.subjectKind)
    {
        spawnSubject();
    }
    bool sleeve = false;
    const int pieces = partValid(subjectNow(), st.selPart) ? piecesOf(st.selPart, st.selSide, sleeve) : 0; // (a part not on it: none outlined)
    const int tier = za::clamp(static_cast<int>(vr_armourstudio_tier.value), 1, 3);
    // (alone: that part's pieces exactly, a beast's cuirass without the rest of its cuirass group: his note 2026-11-27)
    const int worn = vr_armourstudio_alone.value ? piecesOf(st.selPart, SideBoth, sleeve) | agarmor::exactPieces : agarmor::allPieces;
    if(st.subject && (tier != st.subjectTier || worn != st.subjectWorn))
    {
        const bool given = agarmor::give(st.subject, tier, worn);
        st.subjectTier = given ? tier : st.subjectTier;
        st.subjectWorn = given ? worn : st.subjectWorn;
    }
    if(st.subject)
    {
        const progs::ServerVm vm;
        edict_t* e = progs::edictAt(st.subject);
        const edict_t* player = progs::playerEdict();
        if(e && player)
        {
            // Its place: where it was put, moved by the Subject's position rows (away from you, to your left, up; a better
            // look from another angle, his note 2026-11-27); never written to armour.txt.
            const float fx = st.placedFwd[0], fy = st.placedFwd[1];
            const float ax = vr_armourstudio_subject_x.value, ay = vr_armourstudio_subject_y.value, az = vr_armourstudio_subject_z.value;
            const float want[3] = {st.placed[0] + fx * ax - fy * ay, st.placed[1] + fy * ax + fx * ay, st.placed[2] + az};
            if(want[0] != e->v.origin[0] || want[1] != e->v.origin[1] || want[2] != e->v.origin[2])
            {
                for(int c = 0; c < 3; c++)
                {
                    e->v.origin[c] = want[c];
                }
                SV_LinkEdict(e, false);
            }
            const float dx = player->v.origin[0] - e->v.origin[0], dy = player->v.origin[1] - e->v.origin[1];
            e->v.angles[1] = za::atan2(dy, dx) * (180.f / 3.14159265f) + vr_armourstudio_turn.value;
            e->v.angles[0] = vr_armourstudio_subject_pitch.value; // (tipped and rolled: its tops and undersides in view)
            e->v.angles[2] = vr_armourstudio_subject_roll.value;
            e->v.takedamage = DAMAGE_NO; // (its start, a few frames after its spawn, makes it take damage)
            playAnim(e);
        }
    }
    agarmor::studioView(st.subject, za::clamp(vr_armourstudio_ghost.value, 0.f, 1.f), pieces, sleeve);
}

void start_f()
{
    if(Cmd_Argc() > 1)
    {
        Cvar_SetValueQuick(&vr_armourstudio_kind, static_cast<float>(atoi(Cmd_Argv(1))));
    }
    if(!st.running)
    {
        st.freezeWas = vr_agrunt_freeze.value;
        st.menuSeen = false;
    }
    st.running = true;
    Cvar_SetValueQuick(&vr_agrunt_freeze, 1.f);
    freeSubject(); // (put there by keepSubject once you are in the level)
}

void closeStudio(const char* why)
{
    if(st.running)
    {
        // (only while still frozen: a scene that set it and put it back, or a map change that ended the scene, unfroze them
        // first: putting the studio's value back then froze the next scene's humanoids; his note of 2026-11-23 23:03:39)
        if(vr_agrunt_freeze.value != 0.f)
        {
            Cvar_SetValueQuick(&vr_agrunt_freeze, st.freezeWas);
        }
    }
    st.running = false;
    st.menuSeen = false;
    freeSubject();
    Con_Printf("armourstudio: closed%s%s (the armed humanoids free again: vr_agrunt_freeze %g)\n", why[0] ? ": " : "", why,
        vr_agrunt_freeze.value);
}

void close_f()
{
    closeStudio("");
}

// His note 2026-11-10: "After closing the armour studio it seems that armed humanoids do not recover their AI/damageability."
// The studio holds every armed humanoid still (vr_agrunt_freeze) until its Close; its window's X or Escape, or the menu
// closed, left it running, and them frozen. With vr_armourstudio_autoclose those close it too: the menu once it has been
// open since the studio started (a script's studio, never in the menu, is left alone), the window by its own close
// (vr_armourstudio_window.cpp).
void autoClose()
{
    if(!st.running || vr_armourstudio_autoclose.value == 0.f)
    {
        st.menuSeen = st.menuSeen && st.running;
        return;
    }
    const bool menuOpen = key_dest == key_menu && m_state != m_none;
    if(menuOpen)
    {
        st.menuSeen = true;
    }
    else if(st.menuSeen && !windowOpen())
    {
        closeStudio("the menu closed (vr_armourstudio_autoclose)");
    }
}

void save_f()
{
    const bool ok = save();
    st.savedGen = generation();
    Con_Printf(ok ? "armourstudio: saved %s (%d parts set)\n" : "armourstudio: could not write %s\n", filePath(), countSet());
}

void undo_f()
{
    Con_Printf(popUndo() ? "armourstudio: undone (%d more)\n" : "armourstudio: nothing to undo\n", undoCount());
    st.ownGen = generation();
}

void resetPart_f()
{
    pushUndo();
    clear(kindName(), za::clamp(static_cast<int>(vr_armourstudio_part.value), 0, NumParts - 1), selectedSide());
    changed();
    st.ownGen = generation();
    Con_Printf("armourstudio: %s %s reset\n", kindName(), partNames[za::clamp(static_cast<int>(vr_armourstudio_part.value), 0, NumParts - 1)]);
}

void resetKind_f()
{
    pushUndo();
    clear(kindName(), -1, -1);
    changed();
    st.ownGen = generation();
    Con_Printf("armourstudio: %s reset (every part)\n", kindName());
}

// ---- vr_armour_test ----
constexpr const char* testName = "armourtest";
constexpr const char* testFile = "data/armour_test.txt";
constexpr const char* testModel = "progs/vrgrunt.mdl";
constexpr double testStepMost = 10.0; // seconds a step may wait (a safety net: FAIL "timed out")

[[nodiscard]] agarmor::detail::Shell* gruntShell()
{
    qmodel_t* src = Mod_ForName(testModel, false);
    return src ? agarmor::detail::shellFor(src) : nullptr;
}

// Whether the vertices of `piece` (its plates) in `b` are those of `a` moved by `move` (within 1e-3); `same`: unmoved.
[[nodiscard]] bool movedBy(const za::Vector<agarmor::detail::Vert>& a, const za::Vector<agarmor::detail::Vert>& b, int piece,
    const glm::vec3& move, int& n)
{
    n = 0;
    if(a.size() != b.size())
    {
        return false;
    }
    for(int i = 0; i < count(a); i++)
    {
        const auto& va = at(a, i);
        if(va.piece != piece)
        {
            continue;
        }
        n++;
        if(glm::length(at(b, i).p - (va.p + move)) > 1e-3f)
        {
            return false;
        }
    }
    return n > 0;
}

// The table as the hand-edited file has it (the watcher's step): the bracers moved forward 2.
constexpr const char* handText = "kind grunt\n{\n    bracer\n    {\n        offset \"2 0 0\"\n    }\n}\n";

za::Vector<agarmor::detail::Vert> testVerts; // (the grunt's shell, untweaked)

void testStep1()
{
    auto& t = test.tally;
    t.reset();
    q_strlcpy(test.fileWas, vr_armour_file.string, sizeof(test.fileWas));
    q_strlcpy(test.mapWas, sv.name, sizeof(test.mapWas));
    {
        const agarmor::detail::Shell* shipped = gruntShell();
        test.hashShipped = shipped ? shipped->hash : 0u;
    }
    // (the joints' leather, made each frame, has its own checks, vr_agarmor_bridge_check's, armour.txt's settings on it
    // included)
    Cvar_Set("vr_armour_file", testFile);
    (void)files::writeText(filePath(), ""); // (the test's own file, empty)
    (void)load();
    agarmor::detail::Shell* s = gruntShell();
    if(!t.check(s != nullptr, testName, "no grunt shell (a map with its model needed)"))
    {
        test.step = 0;
        t.verdict(testName);
        return;
    }
    testVerts = s->verts;
    test.hashEmpty = s->hash;
    // An override on the bracers (both sides: the right mirrored).
    Override* o = entry("grunt", PartBracer, SideBoth, true);
    o->v[OffsetX] = 0.f;
    o->v[OffsetY] = 1.5f;
    o->v[OffsetZ] = 0.5f;
    o->has |= (1u << OffsetX) | (1u << OffsetY) | (1u << OffsetZ);
    changed();
    s = gruntShell();
    int n = 0;
    t.check(s && movedBy(testVerts, s->verts, agarmor::BracerL, glm::vec3{0.f, 1.5f, 0.5f}, n), testName, "the left bracer's plates not moved by (0 1.5 0.5)");
    t.check(s && movedBy(testVerts, s->verts, agarmor::BracerR, glm::vec3{0.f, -1.5f, 0.5f}, n), testName, "the right bracer's plates not moved by the mirror image (0 -1.5 0.5)");
    for(const int p : {agarmor::Cuirass, agarmor::Helmet, agarmor::PauldronL, agarmor::PauldronR, agarmor::ThighL, agarmor::ThighR, agarmor::ShinL, agarmor::ShinR})
    {
        char what[96];
        q_snprintf(what, sizeof(what), "%s's plates moved (only the bracers were tweaked)", agarmor::detail::pieceNames[p]);
        t.check(s && movedBy(testVerts, s->verts, p, glm::vec3{0.f}, n), testName, what);
    }
    char what[96];
    test.hashTweaked = s ? s->hash : 0u;
    t.check(test.hashTweaked != test.hashEmpty, testName, "the tweaked shell's hash is the untweaked one's");
    // Rows: the shins' 4 rows, both sides alike.
    Override* rows = entry("grunt", PartShin, SideBoth, true);
    const int shinTris = s ? s->pieceTris[agarmor::ShinL] : 0;
    rows->v[Rows] = 4.f;
    rows->has |= 1u << Rows;
    changed();
    s = gruntShell();
    q_snprintf(what, sizeof(what), "the shins' triangles with 4 rows: %d, %d (were %d)", s ? s->pieceTris[agarmor::ShinL] : 0, s ? s->pieceTris[agarmor::ShinR] : 0, shinTris);
    t.check(s && s->pieceTris[agarmor::ShinL] != shinTris && s->pieceTris[agarmor::ShinL] == s->pieceTris[agarmor::ShinR], testName, what);
    rows->has &= ~(1u << Rows);
    changed();
    s = gruntShell();
    t.check(s && s->hash == test.hashTweaked, testName, "rows back to the generator's: not the same shell as before");
    // Taper: the thighs' first ring 1.3 times as wide round their line, their last 0.8 times (each ring's mean radius).
    const auto ringMean = [](const agarmor::detail::Shell* sh, int piece, bool last) {
        const auto& g = sh->tubes[piece].grid;
        if(g.rows <= 0 || g.cols <= 0 || count(g.radius) < (g.rows + 1) * g.cols)
        {
            return 0.f;
        }
        const int ring = last ? g.rows : 0;
        float sum = 0.f;
        for(int c = 0; c < g.cols; c++)
        {
            sum += at(g.radius, ring * g.cols + c);
        }
        return sum / static_cast<float>(g.cols);
    };
    const float topWas = s ? ringMean(s, agarmor::ThighL, false) : 0.f, lowWas = s ? ringMean(s, agarmor::ThighR, true) : 0.f;
    Override* taper = entry("grunt", PartThigh, SideBoth, true);
    taper->v[TaperStart] = 1.3f;
    taper->v[TaperEnd] = 0.8f;
    taper->has |= (1u << TaperStart) | (1u << TaperEnd);
    changed();
    s = gruntShell();
    const float topRatio = s && topWas > 0.f ? ringMean(s, agarmor::ThighL, false) / topWas : 0.f;
    const float lowRatio = s && lowWas > 0.f ? ringMean(s, agarmor::ThighR, true) / lowWas : 0.f;
    q_snprintf(what, sizeof(what), "the thighs' taper: their first ring %.3fx (1.3 wanted), their last %.3fx (0.8 wanted)",
        static_cast<double>(topRatio), static_cast<double>(lowRatio));
    t.check(za::abs(topRatio - 1.3f) < 0.01f && za::abs(lowRatio - 0.8f) < 0.01f, testName, what);
    taper->has &= ~((1u << TaperStart) | (1u << TaperEnd));
    changed();
    s = gruntShell();
    t.check(s && s->hash == test.hashTweaked, testName, "the taper cleared: not the same shell as before");
    // (the leather's settings, on the sleeve parts, go to the joints' leather made each frame: vr_agarmor_bridge_check
    // checks them; here only the file's round trip of one, below)
    Override* leather = entry("grunt", PartThighSleeve, SideLeft, true);
    leather->v[Leather] = 0.f;
    // (both again, for the file's round trip below)
    taper->has |= (1u << TaperStart) | (1u << TaperEnd);
    leather->has |= 1u << Leather;
    changed();
    s = gruntShell();
    test.hashTweaked = s ? s->hash : 0u;
    // Saved and read back: the same shell.
    t.check(save(), testName, "could not write the test's file");
    clear("grunt", -1, -1);
    changed();
    s = gruntShell();
    t.check(s && s->hash == test.hashEmpty, testName, "every tweak cleared: not the generator's shell");
    (void)load();
    s = gruntShell();
    t.check(s && s->hash == test.hashTweaked, testName, "saved and read back: not the tweaked shell");
    // The watcher: the file edited by hand on disk (the bracers moved forward 2): read again within a few seconds.
    t.check(files::writeText(filePath(), handText), testName, "could not write the hand-edited file");
    test.step = 2;
    test.since = realtime;
    test.genBefore = generation();
}

void testStep2()
{
    auto& t = test.tally;
    if(generation() == test.genBefore)
    {
        if(realtime - test.since > testStepMost)
        {
            t.check(false, testName, "timed out: the hand-edited file not read again");
            test.step = 0;
            t.verdict(testName);
        }
        return;
    }
    agarmor::detail::Shell* s = gruntShell();
    int n = 0;
    t.check(s && movedBy(testVerts, s->verts, agarmor::BracerR, glm::vec3{2.f, 0.f, 0.f}, n), testName, "the hand-edited file read: the right bracer not moved forward 2");
    test.hashHand = s ? s->hash : 0u;
    // A map change: the tweaks still there.
    Cbuf_AddText(va("map %s\n", test.mapWas));
    test.step = 3;
    test.since = realtime;
}

void testStep3()
{
    auto& t = test.tally;
    // (the map loaded again and the player in it)
    if(!sv.active || cls.signon != SIGNONS || !progs::localPlayer() || sv.qcvm.time < 1.0)
    {
        if(realtime - test.since > testStepMost * 3.0)
        {
            t.check(false, testName, "timed out: the map change");
            test.step = 0;
            t.verdict(testName);
        }
        return;
    }
    agarmor::detail::Shell* s = gruntShell();
    t.check(s && s->hash == test.hashHand, testName, "after a map change the tweaked shell is not the same");
    // Back as it was: the shipped file, its armour as before the test.
    Cmd_ExecuteString("vr_agarmor_rebuild", src_command);
    Cvar_Set("vr_armour_file", test.fileWas);
    (void)load();
    s = gruntShell();
    t.check(s && s->hash == test.hashShipped, testName, "the shipped file read again: not the shell it made before the test");
    test.step = 0;
    t.verdict(testName);
}

void test_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_armour_test: a map first\n");
        return;
    }
    test = Test{};
    testStep1();
}

// ---- The clone's test (vr_armourstudio_clone_test): your body and its mirror image wear the studio's armour on the player's
// kind with none on you: each tier, each build (switched while it is shown), and none again once the kind is another. Flat
// screen: the ImGui window; in the headset: the menu's page. Its steps one a frame, each on its own condition. ----
constexpr const char* cloneTestName = "armourclone";
constexpr int cloneStepFrames = 72 * 8; // frames a step may wait (a safety net: FAIL "timed out")
constexpr int cloneBuildFirst = 2;             // the build it starts on (the tiers'), then switched while shown to these:
constexpr int cloneBuilds[3] = {1, 3, 2};      // (1 -> 3: the switch that once showed no clone)

struct CloneTest
{
    int step{0}; // 0 off; 1 .. 3 the tiers; 4 .. 6 the builds; 7 the kind another; 8 done
    int frames{0};
    testkit::Tally tally;
    float kindWas{0.f}, tierWas{3.f}, buildWas{1.f}, aloneWas{0.f};
};
CloneTest cloneTest;

void cloneVerdict()
{
    CloneTest& c = cloneTest;
    Cvar_SetValueQuick(&vr_armourstudio_kind, c.kindWas);
    Cvar_SetValueQuick(&vr_armourstudio_tier, c.tierWas);
    Cvar_SetValueQuick(&vr_body_build, c.buildWas);
    Cvar_SetValueQuick(&vr_armourstudio_alone, c.aloneWas);
    Cbuf_AddText(VR_IsActive() ? "togglemenu\n" : "vr_armourstudio_window close\n");
    c.step = 0;
    c.tally.verdict(cloneTestName);
}

// The step's settle: the tier wanted on every piece, settled (no morph), the clone's image showing the body, and the pieces
// drawn in the clone (its own pass, with the head: PlatePass::Clone) all of them.
[[nodiscard]] bool cloneSettled(int tier)
{
    return playerShown() && view::platesState(18) == tier && view::platesState(10) == agarmor::allPieces && view::platesState(11) == 0 &&
           bodyclone::bodyShown() && view::platesState(19) == agarmor::allPieces;
}

void cloneTestFrame()
{
    CloneTest& c = cloneTest;
    if(c.step == 0)
    {
        return;
    }
    c.frames++;
    const int tier = c.step <= 3 ? c.step : 3;
    // (the kind another: nothing on you; the clone's count is its last drawing's, so not asked)
    const bool done = c.step <= 6 ? cloneSettled(tier) && c.frames > 2 : view::platesState(10) == 0 && view::platesState(18) == 0 && !playerShown();
    if(!done)
    {
        if(c.frames > cloneStepFrames)
        {
            c.tally.check(false, cloneTestName,
                va("timed out: step %d (shown %d, colour %d, pieces %d, coming %d, clone body %d, the clone's pieces %d, build %d)", c.step, playerShown() ? 1 : 0,
                    view::platesState(18), view::platesState(10), view::platesState(11), bodyclone::bodyShown() ? 1 : 0, view::platesState(19),
                    static_cast<int>(vr_body_build.value)));
            cloneVerdict();
        }
        return;
    }
    c.tally.check(true);
    Con_Printf("%s: step %d ok: tier %d, build %d, colour %d, pieces %d, the clone's pieces %d, your armour %d (frames %d)\n", cloneTestName, c.step, tier,
        static_cast<int>(vr_body_build.value), view::platesState(18), view::platesState(10), view::platesState(19), agarmor::clientState(cl.viewentity, 0),
        c.frames);
    Cbuf_AddText(c.step >= 3 && c.step <= 6 ? "vr_player_plates_info\n" : ""); // (the body's model: the build's)
    c.step++;
    c.frames = 0;
    if(c.step <= 3)
    {
        Cvar_SetValueQuick(&vr_armourstudio_tier, static_cast<float>(c.step));
    }
    else if(c.step <= 6)
    {
        Cvar_SetValueQuick(&vr_body_build, static_cast<float>(cloneBuilds[c.step - 4])); // (switched while shown)
    }
    else if(c.step == 7)
    {
        Cvar_SetValueQuick(&vr_armourstudio_kind, static_cast<float>(KindGrunt)); // (your own armour again: none)
    }
    else
    {
        cloneVerdict();
    }
}

void cloneTest_f()
{
    if(!sv.active || cls.state != ca_connected)
    {
        Con_Printf("vr_armourstudio_clone_test: a map first\n");
        return;
    }
    CloneTest& c = cloneTest;
    c = CloneTest{};
    c.kindWas = vr_armourstudio_kind.value;
    c.tierWas = vr_armourstudio_tier.value;
    c.buildWas = vr_body_build.value;
    c.aloneWas = vr_armourstudio_alone.value;
    // (a fresh game: no armour on you, so what the clone wears is the studio's)
    const int worn = agarmor::clientState(cl.viewentity, 0);
    c.tally.check(worn <= 0, cloneTestName, va("you wear armour (type %d): a fresh game wanted", worn));
    Cvar_SetValueQuick(&vr_armourstudio_alone, 0.f);
    Cvar_SetValueQuick(&vr_armourstudio_tier, 1.f);
    Cvar_SetValueQuick(&vr_body_build, static_cast<float>(cloneBuildFirst));
    Cvar_SetValueQuick(&vr_armourstudio_kind, static_cast<float>(KindPlayer));
    if(VR_IsActive())
    {
        menu::jumpToArmourStudio();
    }
    else
    {
        Cbuf_AddText("vr_armourstudio_window open\n");
    }
    c.step = 1;
}

// ---- vr_beaststudio_test: a beast in the studio (the rottweiler): picked, its pieces and junctions drawn, a piece moved
// and scaled, a junction turned off and sagged, saved and read back, its junctions following the bent rig through an
// animation and his ragdoll; AD's knights' shared tweaks. Its steps one a frame, each on its own condition. ----
constexpr const char* beastTestName = "beaststudio";
constexpr const char* beastTestFile = "data/armour_beasttest.txt";
constexpr const char* beastTestModel = "progs/dog.mdl";
constexpr int beastTestDog = subjectBeasts;    // (the rottweiler: beastarmour.txt's first kind)
constexpr int beastStepFrames = 72 * 6;        // frames a step may wait (a safety net: FAIL "timed out")
constexpr float beastNear = 0.1f;              // units: a junction's end ring on its piece's place (as posed)

struct BeastTest
{
    int step{0};
    int frames{0};
    testkit::Tally tally;
    char fileWas[64]{};
    float origin0[3]{};
    float lo[3]{}, hi[3]{}, loR[3]{}, hiR[3]{};
    float middle[3]{};
    int animFrame{0};
};
BeastTest bt;

[[nodiscard]] edict_t* beastSubject()
{
    return st.subject && sv.active ? progs::edictAt(st.subject) : nullptr;
}

void beastVerdict()
{
    bt.step = 0;
    Cvar_Set(vr_armour_file.name, bt.fileWas);
    (void)load();
    Cbuf_AddText("vr_armourstudio_close\n");
    bt.tally.verdict(beastTestName);
}

// The step's settle: true once its condition holds; a FAIL "timed out" after beastStepFrames.
[[nodiscard]] bool beastWait(bool ready, const char* what)
{
    if(ready)
    {
        bt.frames = 0;
        return true;
    }
    if(++bt.frames > beastStepFrames)
    {
        bt.tally.check(false, beastTestName, what);
        beastVerdict();
    }
    return false;
}

void beastTestFrame()
{
    if(bt.step <= 0)
    {
        return;
    }
    auto& t = bt.tally;
    const progs::ServerVm vm;
    edict_t* e = beastSubject();
    const int dog = beastTestDog - subjectBeasts;
    if(!e)
    {
        if(bt.step > 1)
        {
            t.check(false, beastTestName, "the subject gone");
            beastVerdict();
            return;
        }
        (void)beastWait(false, "timed out: no subject");
        return;
    }
    switch(bt.step)
    {
    case 1: // the rottweiler ahead, wearing all eight pieces, its junctions drawn
    {
        if(!beastWait(e && beastarmour::drawnCount() >= 8 && beastarmour::leatherDrawn() >= 1, "timed out: no rottweiler with its pieces and leather"))
        {
            return;
        }
        t.check(!ZA_STRCMP(subjectLabel(beastTestDog), "Rottweiler") && !ZA_STRCMP(subjectEntry(), beastTestModel), beastTestName,
            "the subject is not the rottweiler's entry");
        t.check(partValid(beastTestDog, PartCuirass) && partValid(beastTestDog, PartPlate8) && partValid(beastTestDog, PartPlate9) &&
                !partValid(beastTestDog, PartBracer) && !partValid(beastTestDog, PartShin) && partValid(beastTestDog, PartCuirassSleeve) &&
                !partValid(beastTestDog, PartPlate8Sleeve),
            beastTestName, "the rottweiler's parts (cuirass, plates 8 and 9, no bracers or shins, the croup the root)");
        t.check(beastarmour::joins(dog, agarmor::PauldronL) == beastarmour::joins(dog, agarmor::PauldronR) &&
                beastarmour::joins(dog, agarmor::ThighL) == beastarmour::joins(dog, agarmor::ThighR),
            beastTestName, "the left and right shoulders' (haunches') junctions join different pieces");
        const beastarmour::JunctionCheck j = beastarmour::checkJunction(e, agarmor::Cuirass, agarmor::allPieces);
        Con_Printf("%s: cuirass's junction: %d vertices, its ends %.4f from its pieces, longest edge %.2f\n", beastTestName, j.verts,
            static_cast<double>(j.endError), static_cast<double>(j.longestEdge));
        t.check(j.found && j.verts > 0 && j.endError < beastNear, beastTestName, "the cuirass's junction's ends not on its pieces");
        beastarmour::plateWhere(e, agarmor::Cuirass, 0, bt.origin0);
        for(int c = 0; c < 3; c++)
        {
            bt.middle[c] = j.middle[c];
        }
        Cvar_SetValueQuick(&vr_armourstudio_part, static_cast<float>(PartCuirass));
        bt.step = 2;
        return;
    }
    case 2: // the cuirass picked: moved forward 2
        if(!beastWait(st.selPart == PartCuirass, "timed out: the cuirass not picked"))
        {
            return;
        }
        Cvar_SetValueQuick(&vr_armourstudio_offset_x, 2.f);
        bt.step = 3;
        return;
    case 3:
    {
        const Override* o = entry(beastTestModel, PartCuirass, SideBoth, false);
        if(!beastWait(o && (o->has & (1u << OffsetX)) != 0u, "timed out: the cuirass's offset not in the table"))
        {
            return;
        }
        float now[3];
        beastarmour::plateWhere(e, agarmor::Cuirass, 0, now);
        const float moved = za::sqrt((now[0] - bt.origin0[0]) * (now[0] - bt.origin0[0]) + (now[1] - bt.origin0[1]) * (now[1] - bt.origin0[1]) +
            (now[2] - bt.origin0[2]) * (now[2] - bt.origin0[2]));
        Con_Printf("%s: the cuirass moved %.3f units\n", beastTestName, static_cast<double>(moved));
        t.check(za::abs(moved - 2.f) < 0.25f, beastTestName, "the cuirass did not move 2 units");
        const beastarmour::JunctionCheck j = beastarmour::checkJunction(e, agarmor::Cuirass, agarmor::allPieces);
        t.check(j.found && j.endError < beastNear, beastTestName, "the cuirass's junction did not follow it");
        (void)beastarmour::pieceExtent(dog, agarmor::PauldronL, bt.lo, bt.hi);
        (void)beastarmour::pieceExtent(dog, agarmor::PauldronR, bt.loR, bt.hiR);
        Cvar_SetValueQuick(&vr_armourstudio_part, static_cast<float>(PartPauldron));
        Cvar_SetValueQuick(&vr_armourstudio_side, static_cast<float>(SideBoth));
        bt.step = 4;
        return;
    }
    case 4: // the shoulders scaled 1.25 (both)
        if(!beastWait(st.selPart == PartPauldron, "timed out: the shoulders not picked"))
        {
            return;
        }
        Cvar_SetValueQuick(&vr_armourstudio_scale, 1.25f);
        bt.step = 5;
        return;
    case 5:
    {
        const Override* o = entry(beastTestModel, PartPauldron, SideBoth, false);
        if(!beastWait(o && (o->has & (1u << Scale)) != 0u && beastarmour::drawnCount() >= 8, "timed out: the shoulders' scale not in the table"))
        {
            return;
        }
        float lo[3], hi[3], lo3[3], hi3[3];
        (void)beastarmour::pieceExtent(dog, agarmor::PauldronL, lo, hi);
        (void)beastarmour::pieceExtent(dog, agarmor::PauldronR, lo3, hi3);
        const float grew = (hi[0] - lo[0]) / za::max(bt.hi[0] - bt.lo[0], 1e-3f);
        Con_Printf("%s: the left shoulder %.3fx, drawn as %s\n", beastTestName, static_cast<double>(grew), beastarmour::drawnModel(dog, agarmor::PauldronR));
        t.check(za::abs(grew - 1.25f) < 0.01f, beastTestName, "the left shoulder not 1.25x");
        const float grewR = (hi3[0] - lo3[0]) / za::max(bt.hiR[0] - bt.loR[0], 1e-3f);
        t.check(za::abs(grewR - 1.25f) < 0.01f, beastTestName, "the right shoulder not scaled as the left");
        t.check(strstr(beastarmour::drawnModel(dog, agarmor::PauldronR), "#tweak") != nullptr, beastTestName, "the shoulder not drawn shaped");
        Cvar_SetValueQuick(&vr_armourstudio_part, static_cast<float>(PartPauldronSleeve));
        Cvar_SetValueQuick(&vr_armourstudio_side, static_cast<float>(SideLeft));
        bt.step = 6;
        return;
    }
    case 6: // the left shoulder's junction off
        if(!beastWait(st.selPart == PartPauldronSleeve && st.selSide == SideLeft, "timed out: the left shoulder's junction not picked"))
        {
            return;
        }
        Cvar_SetValueQuick(&vr_armourstudio_leather, 0.f);
        bt.step = 7;
        return;
    case 7:
        if(!beastWait(!beastarmour::junctionMade(dog, agarmor::PauldronL), "timed out: the left shoulder's junction still made"))
        {
            return;
        }
        t.check(beastarmour::junctionMade(dog, agarmor::PauldronR), beastTestName, "the right shoulder's junction went too");
        Cvar_SetValueQuick(&vr_armourstudio_part, static_cast<float>(PartCuirassSleeve));
        bt.step = 8;
        return;
    case 8: // the cuirass's junction sagged 2
        if(!beastWait(st.selPart == PartCuirassSleeve, "timed out: the cuirass's junction not picked"))
        {
            return;
        }
        Cvar_SetValueQuick(&vr_armourstudio_sag, 2.f);
        bt.step = 9;
        return;
    case 9:
    {
        const Override* o = entry(beastTestModel, PartCuirassSleeve, SideBoth, false);
        if(!beastWait(o && (o->has & (1u << Sag)) != 0u, "timed out: the sag not in the table"))
        {
            return;
        }
        const beastarmour::JunctionCheck j = beastarmour::checkJunction(e, agarmor::Cuirass, agarmor::allPieces);
        // (its middle ring dropped 2 at rest, carried by the pieces; the cuirass moved 2 forward meanwhile: the height alone)
        const float drop = bt.middle[2] - j.middle[2];
        Con_Printf("%s: the cuirass's junction's middle dropped %.3f\n", beastTestName, static_cast<double>(drop));
        t.check(drop > 1.f && drop < 3.f, beastTestName, "the junction's middle did not drop about 2");
        // Saved, the table cleared, read back.
        t.check(save(), beastTestName, "not saved");
        clear(beastTestModel, -1, -1);
        changed();
        t.check(!entry(beastTestModel, PartCuirass, SideBoth, false) || !entry(beastTestModel, PartCuirass, SideBoth, false)->has, beastTestName,
            "not cleared");
        (void)load();
        const Override* a = entry(beastTestModel, PartCuirass, SideBoth, false);
        const Override* b = entry(beastTestModel, PartPauldron, SideBoth, false);
        const Override* c = entry(beastTestModel, PartPauldronSleeve, SideLeft, false);
        const Override* d = entry(beastTestModel, PartCuirassSleeve, SideBoth, false);
        t.check(a && za::abs(a->v[OffsetX] - 2.f) < 1e-4f && b && za::abs(b->v[Scale] - 1.25f) < 1e-4f && c && (c->has & (1u << Leather)) &&
                c->v[Leather] < 0.5f && d && za::abs(d->v[Sag] - 2.f) < 1e-4f,
            beastTestName, "the saved tweaks not read back (offset, scale, leather off, sag)");
        // The junctions through an animation: its run's first frame and a later one.
        Cvar_SetValueQuick(&vr_armourstudio_part, static_cast<float>(PartThigh));
        int run = 0;
        for(int i = 0; i < animCount(); i++)
        {
            run = !ZA_STRCMP(animName(i), "run") ? i : run;
        }
        Cvar_SetValueQuick(&vr_armourstudio_anim_group, static_cast<float>(run));
        Cvar_SetValueQuick(&vr_armourstudio_anim_play, 0.f);
        Cvar_SetValueQuick(&vr_armourstudio_anim_frame, 0.f);
        Cvar_SetValueQuick(&vr_armourstudio_anim, 1.f);
        bt.animFrame = 0;
        bt.step = 10;
        return;
    }
    case 10:
    case 11:
    {
        // (a frame drawn twice: the pose settled)
        const int want = bt.step == 10 ? 0 : 4;
        if(!beastWait(static_cast<int>(vr_armourstudio_anim_frame.value) == want && bt.frames >= 0 && ++bt.animFrame > 3, "timed out: the frame"))
        {
            return;
        }
        const beastarmour::JunctionCheck j = beastarmour::checkJunction(e, agarmor::ThighL, agarmor::allPieces);
        Con_Printf("%s: frame %d: the left haunch's junction's ends %.4f from its pieces, longest edge %.2f, middle %.2f %.2f %.2f\n", beastTestName,
            static_cast<int>(e->v.frame), static_cast<double>(j.endError), static_cast<double>(j.longestEdge), static_cast<double>(j.middle[0]),
            static_cast<double>(j.middle[1]), static_cast<double>(j.middle[2]));
        t.check(j.found && j.endError < beastNear && j.longestEdge < 12.f, beastTestName, "the haunch's junction off its pieces in the run");
        if(bt.step == 10)
        {
            for(int c = 0; c < 3; c++)
            {
                bt.middle[c] = j.middle[c];
            }
            Cvar_SetValueQuick(&vr_armourstudio_anim_frame, 4.f);
            bt.animFrame = 0;
            bt.step = 11;
            return;
        }
        const float moved = za::sqrt((j.middle[0] - bt.middle[0]) * (j.middle[0] - bt.middle[0]) + (j.middle[1] - bt.middle[1]) * (j.middle[1] - bt.middle[1]) +
            (j.middle[2] - bt.middle[2]) * (j.middle[2] - bt.middle[2]));
        t.check(moved > 0.2f, beastTestName, "the haunch's junction did not move with the run");
        // His ragdoll: killed where he stands.
        Cvar_SetValueQuick(&vr_armourstudio_anim, 0.f);
        const func_t fn = progs::findFunction("Killed");
        edict_t* player = progs::playerEdict();
        if(fn && player)
        {
            e->v.takedamage = DAMAGE_AIM;
            e->v.health = -10.f; // (not gibbed: below -35)
            pr_global_struct->time = qcvm->time;
            pr_global_struct->self = EDICT_TO_PROG(player);
            G_INT(OFS_PARM0) = EDICT_TO_PROG(e);
            G_INT(OFS_PARM1) = EDICT_TO_PROG(player);
            PR_ExecuteProgram(fn);
        }
        bt.animFrame = 0;
        bt.step = 12;
        return;
    }
    case 12:
    {
        if(!beastWait(ragdoll::published(st.subject) && ++bt.animFrame > 72, "timed out: no ragdoll"))
        {
            return;
        }
        const beastarmour::JunctionCheck j = beastarmour::checkJunction(e, agarmor::ThighL, agarmor::allPieces);
        const beastarmour::JunctionCheck c = beastarmour::checkJunction(e, agarmor::Cuirass, agarmor::allPieces);
        Con_Printf("%s: ragdoll: the left haunch's junction's ends %.4f, longest edge %.2f; the cuirass's %.4f, %.2f\n", beastTestName,
            static_cast<double>(j.endError), static_cast<double>(j.longestEdge), static_cast<double>(c.endError), static_cast<double>(c.longestEdge));
        t.check(j.found && c.found && j.endError < beastNear && c.endError < beastNear && j.longestEdge < 12.f && c.longestEdge < 12.f, beastTestName,
            "a junction off its pieces on the ragdoll");
        // AD's two knights: one set of tweaks when their rigs match.
        const bool shared = adShared();
        Con_Printf("%s: AD's knights' rigs %s\n", beastTestName, shared ? "match: one set of tweaks" : "differ (or not there): each its own");
        Override* o = entry(shared ? kindNames[KindAdKnight] : "progs/vrdguard.mdl", PartBracer, SideBoth, true);
        o->v[OffsetX] = 1.5f;
        o->has |= 1u << OffsetX;
        changed();
        Override guard, crossbow;
        (void)resolve("progs/vrdguard.mdl", agarmor::BracerL, false, guard);
        (void)resolve("progs/vrdcrossbow.mdl", agarmor::BracerL, false, crossbow);
        t.check(za::abs(guard.v[OffsetX] - 1.5f) < 1e-4f && (shared ? za::abs(crossbow.v[OffsetX] - 1.5f) < 1e-4f : crossbow.v[OffsetX] == 0.f),
            beastTestName, shared ? "the death guard's tweak not the crossbow knight's too" : "the death guard's tweak on the crossbow knight");
        t.check(!shared || (kindOf("progs/vrdcrossbow.mdl") == KindAdKnight && kindOf("progs/vrdguard.mdl") == KindAdKnight), beastTestName,
            "AD's knights not the ad_knight kind");
        beastVerdict();
        return;
    }
    default:
        beastVerdict();
        return;
    }
}

void beastTest_f()
{
    if(!sv.active || !progs::localPlayer())
    {
        Con_Printf("%s: FAIL a map with you in it needed\n%s: 0 passed, 1 failed\n", beastTestName, beastTestName);
        return;
    }
    bt = BeastTest{};
    q_strlcpy(bt.fileWas, vr_armour_file.string, sizeof(bt.fileWas));
    Cvar_Set(vr_armour_file.name, beastTestFile);
    (void)save(); // (empty: no file left from an earlier run)
    (void)load();
    Cvar_SetValueQuick(&vr_armourstudio_alone, 0.f);
    Cvar_SetValueQuick(&vr_armourstudio_anim, 0.f);
    Cvar_SetValueQuick(&vr_armourstudio_part, static_cast<float>(PartCuirass));
    Cvar_SetValueQuick(&vr_armourstudio_kind, static_cast<float>(beastTestDog));
    Cbuf_AddText("vr_armourstudio\n");
    bt.step = 1;
}

void testFrame()
{
    beastTestFrame();
    cloneTestFrame();
    if(test.step == 2)
    {
        testStep2();
    }
    else if(test.step == 3)
    {
        testStep3();
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_armourstudio", start_f);
    Cmd_AddCommand("vr_armourstudio_close", close_f);
    Cmd_AddCommand("vr_armourstudio_save", save_f);
    Cmd_AddCommand("vr_armourstudio_undo", undo_f);
    Cmd_AddCommand("vr_armourstudio_reset_part", resetPart_f);
    Cmd_AddCommand("vr_armourstudio_reset_kind", resetKind_f);
    Cmd_AddCommand("vr_armour_test", test_f);
    Cmd_AddCommand("vr_armourstudio_clone_test", cloneTest_f);
    Cmd_AddCommand("vr_beaststudio_test", beastTest_f);
    Cmd_AddCommand("vr_armourstudio_anim_info", [] {
        Con_Printf("armourstudio: %s\n", animStatus());
        for(int i = 0; i < animCount(); i++)
        {
            Con_Printf("  %d %s (%d frames from %d)\n", i, anims.list[i].name, anims.list[i].frames, anims.list[i].first);
        }
        if(st.subject && sv.active)
        {
            const progs::ServerVm vm;
            const edict_t* e = progs::edictAt(st.subject);
            Con_Printf("armourstudio: subject frame %d\n", e ? static_cast<int>(e->v.frame) : -1);
        }
    });
    st.savedGen = st.ownGen = generation();
    windowInit();
}

int subjectCount()
{
    return subjectBeasts + beastarmour::kindCount();
}

const char* subjectLabel(int subject)
{
    if(subject >= 0 && subject < subjectAdGuard)
    {
        return humanLabels[subject];
    }
    if(subject == subjectAdGuard || subject == subjectAdCrossbow)
    {
        return adLabels[subject - subjectAdGuard];
    }
    const char* model = beastarmour::kindModel(beastOf(subject));
    for(const auto& b : beastLabels)
    {
        model = !ZA_STRCMP(b[0], model) ? b[1] : model;
    }
    return model;
}

const char* subjectEntry()
{
    return entryOf(subjectNow());
}

bool partValid(int subject, int part)
{
    const int beast = beastOf(subject);
    if(part < 0 || part >= NumParts)
    {
        return false;
    }
    if(beast < 0)
    {
        return part < humanParts;
    }
    if(part == PartShin || part == PartShinSleeve)
    {
        return false;
    }
    bool sleeve = false;
    const int bits = piecesOf(part, SideBoth, sleeve) & beastarmour::kindSlots(beast);
    bool any = false;
    for(int slot = 0; slot < agarmor::NumPieces; slot++)
    {
        any = any || (((bits >> slot) & 1) && (!sleeve || beastarmour::joins(beast, slot) >= 0));
    }
    return any;
}

bool fieldShown(int subject, int part, int field)
{
    const bool beast = beastOf(subject) >= 0;
    const bool sleeve = sleevePart(part);
    if(beast)
    {
        // (a beast's piece: moved, turned, scaled, stood off, tapered; its junction: leather, its tapers (width at each end),
        // stand-off (thickness), folds, sag)
        return sleeve ? (field == Leather || field == TaperStart || field == TaperEnd || field == Standoff || field == Folds || field == FoldDepth ||
                            field == Sag)
                      : (field <= Standoff || field == TaperStart || field == TaperEnd || field >= ScaleX);
    }
    // (the plates' rows: not the helmet's or a sleeve's; the tapers: not the helmet's (a sleeve's leather tapers too); leather:
    // the sleeves' alone; hero plates: the parts that have them; sag: the beasts')
    const bool plates = part != PartHelmet && !sleeve;
    const bool heroPart = part == PartCuirass || part == PartPauldron || part == PartBracer || part == PartShin;
    return !(((field == Leather || field == Folds || field == FoldDepth) && !sleeve) || ((field == Start || field == End || field == Rows) && !plates) ||
        ((field == TaperStart || field == TaperEnd) && part == PartHelmet) || (field == Hero && !heroPart) || field == Sag ||
        (field >= ScaleX && sleeve));
}

bool running()
{
    return st.running;
}

void windowClosed()
{
    if(st.running && vr_armourstudio_autoclose.value != 0.f)
    {
        closeStudio("its window closed (vr_armourstudio_autoclose)");
    }
}

void frame()
{
    armourdata::frame();
    syncFields();
    keepSubject();
    autoClose();
    notePlayerShown();
    testFrame();
}

const char* status()
{
    const int subject = subjectNow();
    const int part = za::clamp(st.selPart, 0, NumParts - 1);
    char what[128];
    what[0] = 0;
    const int beast = beastOf(subject);
    if(!partValid(subject, part))
    {
        q_strlcpy(what, " (none on this kind: pick another part)", sizeof(what));
    }
    else if(beast >= 0)
    {
        // (a beast's piece: its bone; a junction: the piece it joins)
        bool sleeve = false;
        const int bits = piecesOf(part, st.selSide, sleeve) & beastarmour::kindSlots(beast);
        int slot = 0;
        while(slot < agarmor::NumPieces - 1 && !((bits >> slot) & 1))
        {
            slot++;
        }
        const int other = beastarmour::joins(beast, slot);
        if(sleeve)
        {
            q_snprintf(what, sizeof(what), " = the leather from its %s to its %s%s", beastarmour::boneOf(beast, slot),
                other >= 0 ? beastarmour::boneOf(beast, other) : "?", beastarmour::junctionMade(beast, slot) ? "" : " (off)");
        }
        else
        {
            q_snprintf(what, sizeof(what), " = slot %d on its %s", slot, beastarmour::boneOf(beast, slot));
        }
    }
    else if(subject == subjectAdGuard || subject == subjectAdCrossbow)
    {
        q_snprintf(what, sizeof(what), adShared() ? " (shared with the %s: the same rig)" : " (its own: the rigs' bones differ)",
            adLabels[subject == subjectAdGuard ? 1 : 0]);
    }
    q_snprintf(st.statusText, sizeof(st.statusText), "%s: %s%s%s, %d part%s set%s%s", subjectLabel(subject), partNames[part],
        st.selSide == SideLeft ? " (left)" : st.selSide == SideRight ? " (right)" : limbPart(part) ? " (both)" : "", what,
        countSet(), countSet() == 1 ? "" : "s", generation() != st.savedGen ? ", not saved" : ", saved",
        st.running && !st.subject && st.subjectKind != KindPlayer ? "; no subject: Start" : "");
    return st.statusText;
}

int animCount()
{
    return st.subject ? anims.count : 0;
}

const char* animName(int i)
{
    return i >= 0 && i < animCount() ? anims.list[i].name : "";
}

int animFrames(int i)
{
    return i >= 0 && i < animCount() ? anims.list[i].frames : 0;
}

const char* animStatus()
{
    const int n = animCount();
    const int g = za::clamp(static_cast<int>(vr_armourstudio_anim_group.value), 0, za::max(n - 1, 0));
    if(n == 0)
    {
        q_snprintf(st.animText, sizeof(st.animText), "Animation: no subject (Start)");
    }
    else
    {
        q_snprintf(st.animText, sizeof(st.animText), "Animation %d of %d: %s, frame %d of %d (model frame %d)%s", g + 1, n,
            anims.list[g].name, static_cast<int>(vr_armourstudio_anim_frame.value) + 1, anims.list[g].frames,
            anims.list[g].first + za::clamp(static_cast<int>(vr_armourstudio_anim_frame.value), 0, anims.list[g].frames - 1),
            vr_armourstudio_anim.value ? (vr_armourstudio_anim_play.value ? ", playing" : ", paused") : ", off (standing)");
    }
    return st.animText;
}

bool cloneWanted()
{
    return windowOpen() && subjectNow() == KindPlayer;
}

bool playerArmour(int& type, int& pieces)
{
    if(!playerShown())
    {
        return false;
    }
    bool sleeve = false;
    type = za::clamp(static_cast<int>(vr_armourstudio_tier.value), 1, 3);
    pieces = vr_armourstudio_alone.value ? piecesOf(za::clamp(static_cast<int>(vr_armourstudio_part.value), 0, NumParts - 1), SideBoth, sleeve)
                                         : agarmor::allPieces;
    return true;
}

} // namespace qvr::armourstudio
