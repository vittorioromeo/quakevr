// vr_armourstudio.cpp -- see vr_armourstudio.hpp (the Armour Studio).

#include "vr_armourstudio.hpp"
#include "vr_agarmor.hpp"
#include "vr_agarmor_internal.hpp"
#include "vr_armourdata.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_progs.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/glm.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    &vr_armourstudio_start, &vr_armourstudio_end, &vr_armourstudio_rows};

struct State
{
    bool running{false};
    int subject{0};       // its entity (0 none)
    int subjectKind{-1};  // the kind it is
    int subjectTier{-1};
    int subjectWorn{-1};  // (the pieces it was given)
    char subjectMap[64]{}; // (the map it was put in: a new map, it is gone)
    float freezeWas{0.f};  // vr_agrunt_freeze before the studio
    int selKind{-1}, selPart{-1}, selSide{-1}, selGen{-1};
    float shown[NumFields]{};  // the cvars' values as last written or read
    int ownGen{-1};            // the table's generation after the studio's last change
    int savedGen{-1};          // ... as saved or read
    int undoField{-1}, undoSel{-1};
    double undoAt{-1e9};
    char statusText[256]{};
};
State st;

// ---- The test (vr_armour_test): its steps across frames ----
struct Test
{
    int step{0};
    double since{0.0}; // (realtime the step began)
    testkit::Tally tally;
    uint32_t hashEmpty{0}, hashTweaked{0}, hashHand{0};
    int genBefore{0}; // (the table's generation before the hand-edited file: the watcher's read bumps it)
    char fileWas[64]{};
    char mapWas[64]{};
};
Test test;

[[nodiscard]] int selectedSide()
{
    const int part = za::clamp(static_cast<int>(vr_armourstudio_part.value), 0, NumParts - 1);
    return part < PartPauldron ? SideBoth : za::clamp(static_cast<int>(vr_armourstudio_side.value), 0, 2);
}

[[nodiscard]] const char* kindName()
{
    return kindNames[za::clamp(static_cast<int>(vr_armourstudio_kind.value), 0, NumKinds - 1)];
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
    return field == Scale ? 1.f : 0.f;
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
    sleeve = part >= PartPauldronSleeve;
    if(part == PartCuirass)
    {
        return 1 << agarmor::Cuirass;
    }
    if(part == PartHelmet)
    {
        return 1 << agarmor::Helmet;
    }
    const int left = agarmor::PauldronL + 2 * ((sleeve ? part - PartPauldronSleeve : part - PartPauldron));
    return side == SideLeft ? 1 << left : side == SideRight ? 1 << (left + 1) : (1 << left) | (1 << (left + 1));
}

// The sliders against the table: a new selection (or the table changed elsewhere) shows its values; a moved slider sets its value.
void syncFields()
{
    const int kind = za::clamp(static_cast<int>(vr_armourstudio_kind.value), 0, NumKinds - 1);
    const int part = za::clamp(static_cast<int>(vr_armourstudio_part.value), 0, NumParts - 1);
    const int side = selectedSide();
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
    for(int f = 0; f < NumFields; f++)
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
    const int kind = za::clamp(static_cast<int>(vr_armourstudio_kind.value), 0, NumKinds - 1);
    freeSubject();
    st.subjectKind = kind;
    st.subjectTier = -1;
    st.subjectWorn = -1;
    if(kind >= subjectKinds || !sv.active)
    {
        return;
    }
    int num = 0;
    {
        const progs::ServerVm vm;
        const func_t fn = progs::findFunction("VR_ArmourStudio_Spawn");
        edict_t* player = progs::playerEdict();
        if(fn && player)
        {
            pr_global_struct->time = qcvm->time;
            pr_global_struct->self = EDICT_TO_PROG(player);
            G_FLOAT(OFS_PARM0) = static_cast<float>(kind);
            G_FLOAT(OFS_PARM1) = subjectAhead;
            PR_ExecuteProgram(fn);
            edict_t* made = PROG_TO_EDICT(G_INT(OFS_RETURN));
            num = made && made != sv.qcvm.edicts ? NUM_FOR_EDICT(made) : 0;
        }
    }
    st.subject = num;
    q_strlcpy(st.subjectMap, sv.name, sizeof(st.subjectMap));
    Con_Printf(num ? "armourstudio: a %s ahead (entity %d)\n" : "armourstudio: no %s made (a map with you in it needed)\n", kindNames[kind], num);
}

// Its tier, its turn (facing you, then turned), its look.
void keepSubject()
{
    if(!st.running)
    {
        return;
    }
    if(st.subject && (!sv.active || strcmp(st.subjectMap, sv.name) != 0))
    {
        st.running = false; // (a new map: the studio closed)
        st.subject = 0;
        agarmor::studioView(0, 1.f, 0, false);
        Cvar_SetValueQuick(&vr_agrunt_freeze, st.freezeWas);
        return;
    }
    // (a new kind put there once you are in the level: a script's `map x; vr_armourstudio` asks before)
    const bool ready = sv.active && cls.signon == SIGNONS && progs::localPlayer() != nullptr;
    if(ready && za::clamp(static_cast<int>(vr_armourstudio_kind.value), 0, NumKinds - 1) != st.subjectKind)
    {
        spawnSubject();
    }
    bool sleeve = false;
    const int pieces = piecesOf(st.selPart, st.selSide, sleeve);
    const int tier = za::clamp(static_cast<int>(vr_armourstudio_tier.value), 1, 3);
    const int worn = vr_armourstudio_alone.value ? piecesOf(st.selPart, SideBoth, sleeve) : agarmor::allPieces;
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
            const float dx = player->v.origin[0] - e->v.origin[0], dy = player->v.origin[1] - e->v.origin[1];
            e->v.angles[1] = za::atan2(dy, dx) * (180.f / 3.14159265f) + vr_armourstudio_turn.value;
            e->v.angles[0] = e->v.angles[2] = 0.f;
            e->v.takedamage = DAMAGE_NO; // (its start, a few frames after its spawn, makes it take damage)
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
    }
    st.running = true;
    Cvar_SetValueQuick(&vr_agrunt_freeze, 1.f);
    freeSubject(); // (put there by keepSubject once you are in the level)
}

void close_f()
{
    if(st.running)
    {
        Cvar_SetValueQuick(&vr_agrunt_freeze, st.freezeWas);
    }
    st.running = false;
    freeSubject();
    Con_Printf("armourstudio: closed\n");
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

// Whether the vertices of `piece` (its plates, or its sleeve) in `b` are those of `a` moved by `move` (within 1e-3); `same`: unmoved.
[[nodiscard]] bool movedBy(const za::Vector<agarmor::detail::Vert>& a, const za::Vector<agarmor::detail::Vert>& b, int piece, bool sleeve,
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
        const bool isSleeve = va.st.x >= 224.f && va.st.y >= 64.f && va.st.y <= 96.f && va.piece >= agarmor::PauldronL;
        if(va.piece != piece || isSleeve != sleeve)
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
    // An override on the bracers (both sides: the right mirrored) and on the right thigh's sleeve alone.
    Override* o = entry("grunt", PartBracer, SideBoth, true);
    o->v[OffsetX] = 0.f;
    o->v[OffsetY] = 1.5f;
    o->v[OffsetZ] = 0.5f;
    o->has |= (1u << OffsetX) | (1u << OffsetY) | (1u << OffsetZ);
    Override* sl = entry("grunt", PartThighSleeve, SideRight, true);
    sl->v[Standoff] = 0.5f;
    sl->has |= 1u << Standoff;
    changed();
    s = gruntShell();
    int n = 0;
    t.check(s && movedBy(testVerts, s->verts, agarmor::BracerL, false, glm::vec3{0.f, 1.5f, 0.5f}, n), testName, "the left bracer's plates not moved by (0 1.5 0.5)");
    t.check(s && movedBy(testVerts, s->verts, agarmor::BracerR, false, glm::vec3{0.f, -1.5f, 0.5f}, n), testName, "the right bracer's plates not moved by the mirror image (0 -1.5 0.5)");
    t.check(s && movedBy(testVerts, s->verts, agarmor::BracerL, true, glm::vec3{0.f}, n), testName, "the bracers' sleeves moved");
    for(const int p : {agarmor::Cuirass, agarmor::Helmet, agarmor::PauldronL, agarmor::PauldronR, agarmor::ThighL, agarmor::ThighR, agarmor::ShinL, agarmor::ShinR})
    {
        char what[96];
        q_snprintf(what, sizeof(what), "%s's plates moved (only the bracers were tweaked)", agarmor::detail::pieceNames[p]);
        t.check(s && movedBy(testVerts, s->verts, p, false, glm::vec3{0.f}, n), testName, what);
    }
    t.check(s && movedBy(testVerts, s->verts, agarmor::ThighL, true, glm::vec3{0.f}, n), testName, "the left thigh's sleeve moved (only the right's tweaked)");
    // (the right thigh's sleeve: further out from its middle, by the stand-off)
    float before = 0.f, after = 0.f;
    glm::vec3 mid{0.f};
    int m = 0;
    for(int i = 0; s && i < count(testVerts); i++)
    {
        const auto& v = at(testVerts, i);
        const bool isSleeve = v.st.x >= 224.f && v.st.y >= 64.f && v.st.y <= 96.f;
        mid += v.piece == agarmor::ThighR && isSleeve ? v.p : glm::vec3{0.f};
        m += v.piece == agarmor::ThighR && isSleeve ? 1 : 0;
    }
    mid /= static_cast<float>(za::max(m, 1));
    for(int i = 0; s && i < count(testVerts) && s->verts.size() == testVerts.size(); i++)
    {
        const auto& v = at(testVerts, i);
        const bool isSleeve = v.st.x >= 224.f && v.st.y >= 64.f && v.st.y <= 96.f;
        before += v.piece == agarmor::ThighR && isSleeve ? glm::length(glm::vec2{v.p.x - mid.x, v.p.y - mid.y}) : 0.f;
        after += v.piece == agarmor::ThighR && isSleeve ? glm::length(glm::vec2{at(s->verts, i).p.x - mid.x, at(s->verts, i).p.y - mid.y}) : 0.f;
    }
    const float out = (after - before) / static_cast<float>(za::max(m, 1));
    char what[96];
    q_snprintf(what, sizeof(what), "the right thigh's sleeve out by %.2f on average (0.3 .. 0.6 wanted)", static_cast<double>(out));
    t.check(m > 0 && out > 0.3f && out < 0.6f, testName, what);
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
    t.check(s && movedBy(testVerts, s->verts, agarmor::BracerR, false, glm::vec3{2.f, 0.f, 0.f}, n), testName, "the hand-edited file read: the right bracer not moved forward 2");
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
    // Back as it was: the shipped file, the generator's armour.
    Cvar_Set("vr_armour_file", test.fileWas);
    (void)load();
    s = gruntShell();
    t.check(s && s->hash == test.hashEmpty, testName, "the shipped file read again: not the generator's shell");
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

void testFrame()
{
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
    st.savedGen = st.ownGen = generation();
}

void frame()
{
    armourdata::frame();
    syncFields();
    keepSubject();
    testFrame();
}

const char* status()
{
    q_snprintf(st.statusText, sizeof(st.statusText), "%s: %s%s, %d part%s set%s%s", kindName(),
        partNames[za::clamp(st.selPart, 0, NumParts - 1)], st.selSide == SideLeft ? " (left)" : st.selSide == SideRight ? " (right)" : st.selPart >= PartPauldron ? " (both)" : "",
        countSet(), countSet() == 1 ? "" : "s", generation() != st.savedGen ? ", not saved" : ", saved",
        st.running && !st.subject && st.subjectKind != KindPlayer ? "; no subject: Start" : "");
    return st.statusText;
}

} // namespace qvr::armourstudio
