// vr_scenario.cpp -- the checklist's test scenes (vr_scenario.hpp): a scenario file read and checked, its settings set
// before its map loads, the rest placed once the map is in (you, your hands' weapons, what stands around you), its
// settings put back when the test ends; vr_scenario validate and runall check every file headless.

#include "vr_scenario.hpp"
#include "vr_box3d.hpp"
#include "vr_checklist.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_notectx.hpp"
#include "vr_progs.hpp"
#include "vr_setup.hpp"
#include "vr_toolgun.hpp"
#include "vr_zancle.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <glm/glm.hpp>

#include "vr_cvars.hpp"

#include <stdio.h>

namespace qvr::scenario
{

namespace
{

struct Spawn
{
    za::String what;
    int entry{-1}; // the toolgun's spawn list entry (-1: a classname)
    float fwd{0.f}, left{0.f}, up{0.f}, yaw{0.f};
    bool hasYaw{false};
    bool armed{false}; // `spawnarmed`: `what` a classname, made with its right hand's weapon code and its left's
    float code{0.f}, code2{0.f};
    int line{0};
};

struct Give
{
    za::String what;
    int entry{-1};
    int hand{1}; // QC cVR_MainHand (1) or cVR_OffHand (0)
    int line{0};
};

struct Setting
{
    za::String name;
    za::String value;
};

struct Scenario
{
    za::String id, title, map, hint;
    bool hasStart{false};
    float start[3]{};
    float startYaw{0.f};
    bool hasStartYaw{false};
    bool god{false}, notarget{false};
    float health{0.f};
    za::Vector<Give> gives;
    za::Vector<Spawn> spawns;
    za::Vector<Setting> cvars;
    za::Vector<za::String> cmds;
};

// A spawn line's entity as placed: runall checks it is still there (or, freed, that what it made stands at its spot:
// a func_weapon_grabbable makes its weapon as a new entity).
struct Placed
{
    int num{0};
    int line{0};
    int classname{0}; // its string_t then
    glm::vec3 at{0.f};
};

// The scenario running (or the last), the settings it changed (their values before), the errors of the last file read.
struct State
{
    za::Vector<Placed> placed;      // the current placing's spawn lines' entities
    Scenario run;
    za::String lastHint;            // Reset Test's hint (the checklist line's)
    za::Vector<Setting> saved;      // each setting changed: its value before (put back)
    za::Vector<za::String> errors;  // the last file read's errors ("<line>|<message>")
    za::Vector<za::String> notes;   // the last placing's problems (an entry not available, a spot in a wall)
    za::Vector<za::String> queue;   // vr_scenario runall's ids still to run
    za::String error;               // lastError()
    za::String hint;                // the hint shown
    za::Vector<const char*> live;   // while the config is written: each changed setting's own string (put back after)
    auto members() { return qvr::mem::list(placed, run, lastHint, saved, errors, notes, queue, error, hint, live); }
};
mem::Cache<State> state{"scenario", mem::Never};

enum Phase : int
{
    Idle,
    Loading, // its map asked for, not yet in (or not yet settled)
    Active   // placed
};
int phase = Idle;
bool reloaded = false;      // the world was reset since the map was asked for
double phaseSince = 0.0;    // realtime when the phase began
double hintUntil = 0.0;     // realtime the hint shows until
bool runAll = false;        // vr_scenario runall in progress
bool runAllQuit = false;    // and the game quit after it
int runAllFrames = 0;       // its frames since the current one was placed
int runAllCount = 0, runAllFailed = 0;
float givenWeapon[2] = {-1.f, -1.f}; // per QC hand (0 off, 1 main): the weapon its give left there (-1: none given)
double placedAt = 0.0;      // the server's time when the current one was placed
// runall: each scene runs this long in game time; a hand given a weapon must still hold it then (it stays until let go).
static constexpr double runAllHoldSeconds = 3.0;
bool swapped = false;       // the config's write: the saved values swapped in (VR_ScenarioConfigWrite)

[[nodiscard]] za::String dirPath()
{
    return za::String{com_gamedir} + "/scenarios";
}

[[nodiscard]] za::String filePath(const char* id)
{
    return dirPath() + "/" + id + ".txt";
}

[[nodiscard]] bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// A line's words: separated by blanks, a "quoted" one taken whole.
void words(za::StringView l, za::Vector<za::String>& out)
{
    out.clear();
    size_t i = 0;
    while(i < l.size())
    {
        while(i < l.size() && isSpace(l[i]))
        {
            i++;
        }
        if(i >= l.size())
        {
            break;
        }
        za::String w;
        if(l[i] == '"')
        {
            i++;
            while(i < l.size() && l[i] != '"')
            {
                w += l[i++];
            }
            i++; // (the closing quote)
        }
        else
        {
            while(i < l.size() && !isSpace(l[i]))
            {
                w += l[i++];
            }
        }
        out.pushBack(ZA_MOVE(w));
    }
}

// The rest of the line after its first word, trimmed (and its quotes taken off, if the whole of it is quoted).
[[nodiscard]] za::String rest(za::StringView l)
{
    size_t i = 0;
    while(i < l.size() && isSpace(l[i]))
    {
        i++;
    }
    while(i < l.size() && !isSpace(l[i]))
    {
        i++;
    }
    while(i < l.size() && isSpace(l[i]))
    {
        i++;
    }
    size_t e = l.size();
    while(e > i && isSpace(l[e - 1]))
    {
        e--;
    }
    if(e - i >= 2 && l[i] == '"' && l[e - 1] == '"')
    {
        i++;
        e--;
    }
    return za::String{l.substrByPosLen(i, e - i)};
}

[[nodiscard]] bool number(const za::String& w, float& out)
{
    char* end = nullptr;
    out = strtof(w.cStr(), &end);
    return end && end != w.cStr() && *end == '\0';
}

void error(int line, const char* text)
{
    state.errors.pushBack(za::String{va("%d|%s", line, text)});
}

// The toolgun's spawn list entry with that label (any case), or else the first one of that classname with no key set
// (the plain one); -1 none. By its plain name: spawnableLabel() is the panel's text, "> " before the toolgun's selected
// entry (his beta4 pass, 2026-10-26: the entry he had picked, "Nailgun", then "AD Widowmaker", failed every scene
// naming it; validate's selection check guards it).
[[nodiscard]] int findEntry(const char* what)
{
    for(int i = 0; i < toolgun::spawnableCount(); i++)
    {
        if(!q_strcasecmp(toolgun::spawnableName(i), what))
        {
            return i;
        }
    }
    for(int i = 0; i < toolgun::spawnableCount(); i++)
    {
        if(!q_strcasecmp(toolgun::spawnableClass(i), what))
        {
            return i;
        }
    }
    return -1;
}

// A spawn function's name in the server's progs; with no server running (validate from the menu or console, a scene
// started before any map: no progs loaded), anything shaped like a classname (an underscore). The server's VM made
// current for the lookup: the console runs with none (qcvm null).
[[nodiscard]] bool isClassname(const char* what)
{
    if(!sv.active || !sv.qcvm.progs)
    {
        return strchr(what, '_') != nullptr && strchr(what, ' ') == nullptr;
    }
    const progs::ServerVm vm;
    return progs::findFunction(what) != 0;
}

// A console command's first word: a command, an alias or a setting.
[[nodiscard]] bool knownCommand(const za::String& line)
{
    za::Vector<za::String> w;
    words(line, w);
    if(w.empty())
    {
        return false;
    }
    za::String first = w[0];
    const size_t semi = first.find(';');
    if(semi != za::StringView::nPos)
    {
        first = za::String{first.substrByPosLen(0, semi)};
    }
    return Cmd_Exists(first.cStr()) || Cmd_AliasExists(first.cStr()) || Cvar_FindVar(first.cStr());
}

// Reads and checks quakevr/scenarios/<id>.txt into `out`; the errors in state.errors. False: errors.
bool read(const char* id, Scenario& out)
{
    state.errors.clear();
    out = Scenario{};
    out.id = id;
    za::String text;
    if(!id[0] || !files::readText(filePath(id).cStr(), text))
    {
        error(0, va("no file scenarios/%s.txt", id));
        return false;
    }
    za::Vector<za::String> w;
    int lineNo = 0;
    files::forLines(text, [&](za::StringView raw) {
        lineNo++;
        za::String l = za::String{raw};
        while(!l.empty() && isSpace(l.back()))
        {
            l.popBack();
        }
        words(l, w);
        if(w.empty() || w[0][0] == '#' || (w[0].size() >= 2 && w[0][0] == '/' && w[0][1] == '/'))
        {
            return;
        }
        const char* d = w[0].cStr();
        float n[5]{};
        if(!q_strcasecmp(d, "title"))
        {
            out.title = rest(l);
        }
        else if(!q_strcasecmp(d, "hint"))
        {
            out.hint = rest(l);
        }
        else if(!q_strcasecmp(d, "map"))
        {
            if(w.size() != 2)
            {
                error(lineNo, "map <name>");
                return;
            }
            out.map = w[1];
            if(!COM_FileExists(va("maps/%s.bsp", w[1].cStr()), nullptr))
            {
                // (an AD map is found only while Arcane Dimensions is the running campaign: Play > Official Campaigns)
                error(lineNo, va("no map maps/%s.bsp%s", w[1].cStr(),
                                  !q_strncasecmp(w[1].cStr(), "ad_", 3) ? " (start Arcane Dimensions first)" : ""));
            }
        }
        else if(!q_strcasecmp(d, "start"))
        {
            if((w.size() != 4 && w.size() != 5) || !number(w[1], n[0]) || !number(w[2], n[1]) || !number(w[3], n[2]) ||
                (w.size() == 5 && !number(w[4], n[3])))
            {
                error(lineNo, "start <x> <y> <z> [yaw]");
                return;
            }
            out.hasStart = true;
            out.start[0] = n[0];
            out.start[1] = n[1];
            out.start[2] = n[2];
            out.hasStartYaw = w.size() == 5;
            out.startYaw = n[3];
        }
        else if(!q_strcasecmp(d, "god"))
        {
            out.god = true;
        }
        else if(!q_strcasecmp(d, "notarget"))
        {
            out.notarget = true;
        }
        else if(!q_strcasecmp(d, "health"))
        {
            if(w.size() != 2 || !number(w[1], out.health) || out.health <= 0.f)
            {
                error(lineNo, "health <n>");
            }
        }
        else if(!q_strcasecmp(d, "give"))
        {
            Give g;
            g.line = lineNo;
            if(w.size() < 2 || w.size() > 3 ||
                (w.size() == 3 && q_strcasecmp(w[2].cStr(), "main") && q_strcasecmp(w[2].cStr(), "off")))
            {
                error(lineNo, "give <weapon> [main|off]");
                return;
            }
            g.what = w[1];
            g.hand = w.size() == 3 && !q_strcasecmp(w[2].cStr(), "off") ? 0 : 1;
            g.entry = findEntry(g.what.cStr());
            if(g.entry < 0 || strcmp(toolgun::spawnableSection(g.entry), "Weapons") != 0)
            {
                error(lineNo, va("\"%s\" is not a toolgun Weapons entry", g.what.cStr()));
                return;
            }
            out.gives.pushBack(ZA_MOVE(g));
        }
        else if(!q_strcasecmp(d, "spawn"))
        {
            Spawn s;
            s.line = lineNo;
            const int nums = static_cast<int>(w.size()) - 2;
            bool ok = w.size() >= 3 && w.size() <= 6;
            for(int k = 0; ok && k < nums; k++)
            {
                ok = number(w[2 + k], n[k]);
            }
            if(!ok)
            {
                error(lineNo, "spawn <entry> <fwd> [left] [up] [yaw]");
                return;
            }
            s.what = w[1];
            s.fwd = n[0];
            s.left = n[1];
            s.up = n[2];
            s.yaw = n[3];
            s.hasYaw = nums >= 4;
            s.entry = findEntry(s.what.cStr());
            if(s.entry < 0 && !isClassname(s.what.cStr()))
            {
                error(lineNo, va("\"%s\" is no toolgun spawn entry nor classname", s.what.cStr()));
                return;
            }
            out.spawns.pushBack(ZA_MOVE(s));
        }
        else if(!q_strcasecmp(d, "spawnarmed"))
        {
            Spawn s;
            s.line = lineNo;
            const int nums = static_cast<int>(w.size()) - 4;
            bool ok = w.size() >= 5 && w.size() <= 8;
            float code[2]{};
            ok = ok && number(w[2], code[0]) && number(w[3], code[1]);
            for(int k = 0; ok && k < nums; k++)
            {
                ok = number(w[4 + k], n[k]);
            }
            if(!ok)
            {
                error(lineNo, "spawnarmed <classname> <weapon> <weapon2> <fwd> [left] [up] [yaw]");
                return;
            }
            s.what = w[1];
            s.armed = true;
            s.code = code[0];
            s.code2 = code[1];
            s.fwd = n[0];
            s.left = n[1];
            s.up = n[2];
            s.yaw = n[3];
            s.hasYaw = nums >= 4;
            if(!isClassname(s.what.cStr()))
            {
                error(lineNo, va("\"%s\" is not a spawn function", s.what.cStr()));
                return;
            }
            out.spawns.pushBack(ZA_MOVE(s));
        }
        else if(!q_strcasecmp(d, "cvar"))
        {
            if(w.size() != 3)
            {
                error(lineNo, "cvar <name> <value>");
                return;
            }
            if(!Cvar_FindVar(w[1].cStr()))
            {
                error(lineNo, va("no setting %s", w[1].cStr()));
                return;
            }
            out.cvars.pushBack(Setting{w[1], w[2]});
        }
        else if(!q_strcasecmp(d, "cmd"))
        {
            za::String c = rest(l);
            if(!knownCommand(c))
            {
                error(lineNo, va("unknown command in \"%s\"", c.cStr()));
                return;
            }
            out.cmds.pushBack(ZA_MOVE(c));
        }
        else
        {
            error(lineNo, va("unknown directive \"%s\"", d));
        }
    });
    if(out.map.empty())
    {
        error(0, "no map line");
    }
    return state.errors.empty();
}

void printErrors(const char* id)
{
    for(const za::String& e : state.errors)
    {
        Con_Printf("SCNERR|%s|%s\n", id, e.cStr());
    }
}

// The settings changed put back.
void restoreSettings()
{
    for(const Setting& s : state.saved)
    {
        if(cvar_t* var = Cvar_FindVar(s.name.cStr()))
        {
            Cvar_SetQuick(var, s.value.cStr());
        }
    }
    if(!state.saved.empty())
    {
        Con_Printf("vr_scenario: %d setting%s put back\n", static_cast<int>(state.saved.size()),
            state.saved.size() == 1 ? "" : "s");
    }
    state.saved.clear();
}

void end()
{
    restoreSettings();
    phase = Idle;
    hintUntil = 0.0;
}

// Takes the weapon a toolgun Weapons entry made (`spawner`: a func_weapon_grabbable makes the weapon itself as a new
// entity, or an AD upgrade's prop is one) into `hand` of `p`, as a grab does (QC VR_ADMelee_Take: either hand, any
// grabbable weapon). False: none found that a hand can take.
bool takeIntoHand(edict_t* p, edict_t* spawner, int hand, za::Vector<int>& taken)
{
    const func_t fn = progs::findFunction("VR_ADMelee_Take");
    const int handtouch = progs::fields().handtouch;
    if(!fn || handtouch < 0)
    {
        return false;
    }
    // The weapon made now (QC CreateThrownWeapon: its vr_drop_born this frame's time; any free slot, below the
    // spawner's too), a hand able to take it; else the spawner itself (an upgrade's prop). The nearest to it.
    const int born = progs::fields().vr_drop_born;
    edict_t* w = nullptr;
    float best = 1e9f;
    for(int n = 1; n < qcvm->num_edicts; n++)
    {
        edict_t* e = EDICT_NUM(n);
        if(e->free || e == p || za::find(taken.begin(), taken.end(), n) != taken.end() || progs::fieldInt(e, handtouch) == 0 ||
            (e != spawner && (born < 0 || progs::fieldFloat(e, born) != static_cast<float>(qcvm->time))))
        {
            continue;
        }
        const float d = glm::distance(glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]},
            glm::vec3{spawner->v.origin[0], spawner->v.origin[1], spawner->v.origin[2]});
        if(d < best)
        {
            best = d;
            w = e;
        }
    }
    if(!w)
    {
        return false;
    }
    const int oldSelf = pr_global_struct->self;
    const int oldOther = pr_global_struct->other;
    pr_global_struct->time = qcvm->time;
    G_INT(OFS_PARM0) = EDICT_TO_PROG(p);
    G_INT(OFS_PARM1) = EDICT_TO_PROG(w);
    G_FLOAT(OFS_PARM2) = static_cast<float>(hand);
    PR_ExecuteProgram(fn);
    if(!w->free)
    {
        taken.pushBack(NUM_FOR_EDICT(w)); // (still an entity: the next give mustn't take it again; a freed slot may be the next weapon's)
    }
    pr_global_struct->self = oldSelf;
    pr_global_struct->other = oldOther;
    return true;
}

void note(const char* text)
{
    state.notes.pushBack(za::String{text});
    Con_Printf("vr_scenario: %s\n", text);
}

// Everything placed: you, your hands, what stands around you, the commands; the hint shown.
void place()
{
    const progs::ServerVm vm;
    const Scenario& s = state.run;
    state.notes.clear();
    edict_t* p = progs::localPlayer();
    if(!p)
    {
        note("no player");
        return;
    }
    if(s.hasStart)
    {
        for(int k = 0; k < 3; k++)
        {
            p->v.origin[k] = s.start[k];
            p->v.oldorigin[k] = s.start[k];
            p->v.velocity[k] = 0.f;
        }
    }
    const float yaw = s.hasStartYaw ? s.startYaw : p->v.angles[YAW];
    p->v.angles[PITCH] = p->v.angles[ROLL] = 0.f;
    p->v.angles[YAW] = yaw;
    p->v.v_angle[PITCH] = p->v.v_angle[ROLL] = 0.f;
    p->v.v_angle[YAW] = yaw;
    p->v.fixangle = 1;
    SV_LinkEdict(p, true);
    int flags = static_cast<int>(p->v.flags);
    flags |= s.god ? FL_GODMODE : 0;
    flags |= s.notarget ? FL_NOTARGET : 0;
    p->v.flags = static_cast<float>(flags);
    if(s.health > 0.f)
    {
        p->v.health = s.health;
    }

    za::Vector<int> taken;
    bool gaveTo[2] = {false, false};
    for(const Give& g : s.gives)
    {
        edict_t* w = toolgun::spawnEntryAt(g.entry, glm::vec3{p->v.origin[0], p->v.origin[1], p->v.origin[2]}, yaw);
        if(!w)
        {
            note(va("line %d: \"%s\" not available here (its pack's data missing?)", g.line, g.what.cStr()));
            continue;
        }
        if(!takeIntoHand(p, w, g.hand, taken))
        {
            note(va("line %d: \"%s\" left at your feet (no weapon a hand can take found)", g.line, g.what.cStr()));
            continue;
        }
        gaveTo[g.hand == 0 ? 0 : 1] = true;
    }
    const int weapon2 = progs::fields().weapon2;
    // Held as if just grabbed: in the hand until its grip is let go there (QC VR_Scene_HoldGiven), whatever the grip mode.
    const func_t hold = progs::findFunction("VR_Scene_HoldGiven");
    for(const int h : {0, 1})
    {
        givenWeapon[h] = !gaveTo[h] ? -1.f : h == 1 ? p->v.weapon : weapon2 >= 0 ? progs::fieldFloat(p, weapon2) : -1.f;
        if(gaveTo[h] && hold)
        {
            pr_global_struct->time = qcvm->time;
            G_INT(OFS_PARM0) = EDICT_TO_PROG(p);
            G_FLOAT(OFS_PARM1) = static_cast<float>(h);
            PR_ExecuteProgram(hold);
        }
    }
    placedAt = qcvm->time;

    const glm::vec3 feet{p->v.origin[0], p->v.origin[1], p->v.origin[2] + p->v.mins[2]};
    state.placed.clear();
    const float a = glm::radians(yaw);
    const glm::vec3 fwd{glm::cos(a), glm::sin(a), 0.f};
    const glm::vec3 left{-fwd.y, fwd.x, 0.f};
    int made = 0;
    for(const Spawn& sp : s.spawns)
    {
        glm::vec3 at = feet + fwd * sp.fwd + left * sp.left;
        vec3_t zero{0.f, 0.f, 0.f};
        vec3_t from{at.x, at.y, feet.z + 64.f}, down{at.x, at.y, feet.z - 1024.f};
        const trace_t tr = SV_Move(from, zero, zero, down, MOVE_NOMONSTERS, p);
        if(tr.startsolid || tr.allsolid)
        {
            note(va("line %d: \"%s\" placed in a wall (fwd %.0f, left %.0f)", sp.line, sp.what.cStr(), sp.fwd, sp.left));
        }
        at.z = (tr.fraction < 1.f ? tr.endpos[2] : feet.z) + sp.up;
        const float faceYaw = sp.hasYaw ? yaw + sp.yaw : glm::degrees(glm::atan(feet.y - at.y, feet.x - at.x));
        edict_t* e = nullptr;
        if(sp.armed)
        {
            e = box3d::spawnClass(sp.what.cStr(), at + glm::vec3{0.f, 0.f, 25.f}, glm::vec3{0.f, faceYaw, 0.f}, nullptr,
                "weapon", sp.code, false, 0.f, "weapon2", sp.code2);
        }
        else if(sp.entry >= 0)
        {
            e = toolgun::spawnEntryAt(sp.entry, at, faceYaw);
        }
        else
        {
            const float lift = !strncmp(sp.what.cStr(), "monster_", 8) ? 25.f : 1.f;
            e = box3d::spawnClass(sp.what.cStr(), at + glm::vec3{0.f, 0.f, lift}, glm::vec3{0.f, faceYaw, 0.f});
        }
        if(!e)
        {
            note(va("line %d: \"%s\" not made (not available here?)", sp.line, sp.what.cStr()));
            continue;
        }
        made++;
        state.placed.pushBack(Placed{NUM_FOR_EDICT(e), sp.line, e->v.classname, glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]}});
        Con_DPrintf("vr_scenario: line %d: \"%s\" (edict %d) at %.0f %.0f %.0f\n", sp.line, sp.what.cStr(),
            NUM_FOR_EDICT(e), e->v.origin[0], e->v.origin[1], e->v.origin[2]);
    }

    for(const za::String& c : s.cmds)
    {
        Cbuf_AddText(c.cStr());
        Cbuf_AddText("\n");
    }

    state.hint = !s.hint.empty() ? s.hint : !state.lastHint.empty() ? state.lastHint : s.title;
    hintUntil = realtime + static_cast<double>(vr_scenario_hint_time.value);
    if(!vrActive() && !state.hint.empty())
    {
        SCR_CenterPrintFor(state.hint.cStr(), vr_scenario_hint_time.value);
    }
    Con_Printf("SCNPLACED|%s|%d of %d spawned|%d given|%d problems|hands: main %.0f, off %.0f\n", s.id.cStr(), made,
        static_cast<int>(s.spawns.size()), static_cast<int>(s.gives.size()), static_cast<int>(state.notes.size()),
        p->v.weapon, weapon2 >= 0 ? progs::fieldFloat(p, weapon2) : -1.f);
}

void runNext()
{
    if(state.queue.empty())
    {
        runAll = false;
        end();
        Con_Printf("SCNRUNSUM|%d run|%d failed\n", runAllCount, runAllFailed);
        if(runAllQuit)
        {
            Cbuf_AddText("toggleconsole\nquit\n");
        }
        return;
    }
    const za::String id = state.queue.front();
    state.queue.erase(state.queue.begin());
    runAllFrames = 0;
    runAllCount++;
    if(!start(id.cStr(), ""))
    {
        runAllFailed++;
        Con_Printf("SCNRUN|%s|FAIL|%s\n", id.cStr(), state.error.cStr());
        runNext();
    }
}

// Every scenario file's id, sorted.
void allIds(za::Vector<za::String>& out)
{
    out.clear();
    files::forEachEntry(dirPath().cStr(), [&](const char* name, bool isDir) {
        const size_t n = strlen(name);
        if(!isDir && n > 4 && !q_strcasecmp(name + n - 4, ".txt"))
        {
            out.pushBack(za::String{za::StringView{name, n - 4}});
        }
    });
    for(size_t i = 1; i < out.size(); i++) // (a few dozen: an insertion sort)
    {
        for(size_t j = i; j > 0 && strcmp(out[j - 1].cStr(), out[j].cStr()) > 0; j--)
        {
            za::genericSwap(out[j - 1], out[j]);
        }
    }
}

// vr_scenario validate: every file read and checked, and every checklist tag's file there.
void validate()
{
    za::Vector<za::String> ids;
    allIds(ids);
    int bad = 0;
    Scenario s;
    for(const za::String& id : ids)
    {
        if(read(id.cStr(), s))
        {
            Con_Printf("SCNOK|%s|%s|%d spawns|%d gives|%d cvars|%d cmds\n", id.cStr(), s.map.cStr(),
                static_cast<int>(s.spawns.size()), static_cast<int>(s.gives.size()), static_cast<int>(s.cvars.size()),
                static_cast<int>(s.cmds.size()));
        }
        else
        {
            bad++;
            printErrors(id.cStr());
        }
    }
    // Each available toolgun entry found by its name while it is the toolgun's selection (the panel's label then has
    // "> " before it: his beta4 pass's "not started"). The selection put back.
    const int wasSelected = toolgun::selectedSpawnable();
    int lookups = 0, lost = 0;
    for(int i = 0; i < toolgun::spawnableCount(); i++)
    {
        if(!toolgun::spawnableAvailable(i))
        {
            continue;
        }
        toolgun::selectSpawnable(i, false);
        const int found = findEntry(toolgun::spawnableName(i));
        lookups++;
        if(found < 0 || q_strcasecmp(toolgun::spawnableName(found), toolgun::spawnableName(i)) != 0)
        {
            lost++;
            Con_Printf("SCNERR|toolgun|0|\"%s\" not found while selected\n", toolgun::spawnableName(i));
        }
    }
    toolgun::selectSpawnable(wasSelected, false); // (a picked thing not in the list comes back as nothing)
    bad += lost ? 1 : 0;
    Con_Printf("SCNSEL|%d entries found while selected|%d not\n", lookups - lost, lost);
    checklist::refresh(true);
    int tags = 0, missing = 0;
    for(int i = 0; i < checklist::itemCount(); i++)
    {
        const char* id = checklist::scenario(i);
        if(!*id)
        {
            continue;
        }
        tags++;
        if(!files::isFile(filePath(id).cStr()))
        {
            missing++;
            Con_Printf("SCNERR|%s|0|checklist item %d's tag: no file scenarios/%s.txt\n", id, i, id);
        }
    }
    Con_Printf("SCNSUM|%d files|%d with errors|%d checklist tags|%d missing\n", static_cast<int>(ids.size()), bad, tags,
        missing);
}

// vr_scenario where: where you stand and face (a start line), how far the walls are, the nearest water.
void where()
{
    const progs::ServerVm vm;
    edict_t* p = progs::localPlayer();
    if(!p)
    {
        Con_Printf("vr_scenario where: no game running\n");
        return;
    }
    const float yaw = p->v.v_angle[YAW];
    Con_Printf("SCNWHERE|start %.0f %.0f %.0f %.0f|map %s\n", p->v.origin[0], p->v.origin[1], p->v.origin[2], yaw, sv.name);
    const char* names[4] = {"ahead", "left", "behind", "right"};
    for(int k = 0; k < 4; k++)
    {
        const float a = glm::radians(yaw + 90.f * static_cast<float>(k));
        vec3_t zero{0.f, 0.f, 0.f};
        vec3_t from{p->v.origin[0], p->v.origin[1], p->v.origin[2]};
        vec3_t to{from[0] + glm::cos(a) * 4096.f, from[1] + glm::sin(a) * 4096.f, from[2]};
        const trace_t tr = SV_Move(from, zero, zero, to, MOVE_NOMONSTERS, p);
        Con_Printf("SCNWHERE|%s %.0f\n", names[k], tr.fraction * 4096.f);
    }
    float best = 1e9f;
    glm::vec3 water{0.f};
    for(float dx = -1536.f; dx <= 1536.f; dx += 32.f)
    {
        for(float dy = -1536.f; dy <= 1536.f; dy += 32.f)
        {
            for(float dz = -128.f; dz <= 32.f; dz += 16.f)
            {
                vec3_t q{p->v.origin[0] + dx, p->v.origin[1] + dy, p->v.origin[2] + dz};
                const int c = SV_PointContents(q);
                const float d = dx * dx + dy * dy + dz * dz;
                if((c == CONTENTS_WATER || c == CONTENTS_SLIME || c == CONTENTS_LAVA) && d < best)
                {
                    best = d;
                    water = glm::vec3{q[0], q[1], q[2]};
                }
            }
        }
    }
    if(best < 1e9f)
    {
        // Its surface (up from there) and its extent at that height, along x and y (16-unit steps).
        vec3_t q{water.x, water.y, water.z};
        const int kind = SV_PointContents(q);
        while(q[2] < water.z + 512.f && SV_PointContents(q) == kind)
        {
            q[2] += 1.f;
        }
        const float surface = q[2] - 1.f;
        float ext[4]{};
        for(int k = 0; k < 4; k++)
        {
            vec3_t r{water.x, water.y, surface - 4.f};
            const int axis = k / 2;
            const float step = k % 2 ? 16.f : -16.f;
            while(glm::abs(r[axis] - (axis ? water.y : water.x)) < 4096.f && SV_PointContents(r) == kind)
            {
                r[axis] += step;
            }
            ext[k] = r[axis] - step;
        }
        // Its depth in the middle of that extent (the floor under it).
        vec3_t zero{0.f, 0.f, 0.f};
        vec3_t mid{(ext[0] + ext[1]) * 0.5f, (ext[2] + ext[3]) * 0.5f, surface - 1.f};
        vec3_t deep{mid[0], mid[1], surface - 1024.f};
        const trace_t tr = SV_Move(mid, zero, zero, deep, MOVE_NOMONSTERS, p);
        Con_Printf("SCNWHERE|liquid %d nearest at %.0f %.0f %.0f (%.0f away), surface z %.0f, x %.0f..%.0f, y %.0f..%.0f, "
                   "%.0f deep in its middle\n",
            kind, water.x, water.y, water.z, glm::sqrt(best), surface, ext[0], ext[1], ext[2], ext[3],
            surface - tr.endpos[2]);
    }
}

} // namespace

bool start(const char* id, const char* hint)
{
    Scenario s;
    if(!read(id, s))
    {
        printErrors(id);
        state.error = state.errors.empty() ? za::String{"errors"} : state.errors.front();
        Con_Printf("vr_scenario: %s not started: %s\n", id, state.error.cStr());
        return false;
    }
    end(); // (the one before: its settings back first)
    state.error.clear();
    state.run = ZA_MOVE(s);
    state.lastHint = hint ? hint : "";
    for(const Setting& c : state.run.cvars)
    {
        cvar_t* var = Cvar_FindVar(c.name.cStr());
        bool known = false;
        for(const Setting& o : state.saved)
        {
            known = known || !q_strcasecmp(o.name.cStr(), c.name.cStr());
        }
        if(!known)
        {
            state.saved.pushBack(Setting{c.name, za::String{var->string}});
        }
        Cvar_SetQuick(var, c.value.cStr());
    }
    phase = Loading;
    reloaded = false;
    phaseSince = realtime;
    Con_Printf("vr_scenario: %s: loading %s\n", id, state.run.map.cStr());
    Cbuf_InsertText(va("map %s\n", state.run.map.cStr())); // (next: before the rest of a script)
    if(!runAll)
    {
        notectx::testRun(state.run.id.cStr()); // (named in the voice notes that follow)
    }
    return true;
}

bool reset()
{
    if(state.run.id.empty())
    {
        Con_Printf("vr_scenario reset: no test run yet\n");
        return false;
    }
    const za::String id = state.run.id;
    const za::String hint = state.lastHint;
    return start(id.cStr(), hint.cStr());
}

void stop()
{
    if(phase != Idle)
    {
        Con_Printf("vr_scenario: %s ended\n", state.run.id.cStr());
    }
    end();
}

const char* current()
{
    return phase != Idle ? state.run.id.cStr() : "";
}

const char* lastError()
{
    return state.error.cStr();
}

void worldReset()
{
    if(phase == Loading)
    {
        reloaded = true; // (its own load)
        return;
    }
    if(phase == Active)
    {
        Con_Printf("vr_scenario: %s ended (another map or a load)\n", state.run.id.cStr());
        end();
    }
}

void frame()
{
    if(phase == Loading)
    {
        if(realtime - phaseSince > 60.0 || (reloaded && !sv.active && cls.state != ca_connected && realtime - phaseSince > 5.0))
        {
            state.error = "the map didn't load";
            Con_Printf("vr_scenario: %s: %s\n", state.run.id.cStr(), state.error.cStr());
            end();
            if(runAll)
            {
                runAllFailed++;
                Con_Printf("SCNRUN|%s|FAIL|%s\n", state.run.id.cStr(), state.error.cStr());
                runNext();
            }
            return;
        }
        if(reloaded && sv.active && cls.signon == SIGNONS && q_strcasecmp(sv.name, state.run.map.cStr()) != 0)
        {
            Con_Printf("vr_scenario: %s abandoned: %s loaded instead\n", state.run.id.cStr(), sv.name);
            end(); // (another map asked for meanwhile: the settings back)
            return;
        }
        if(reloaded && sv.active && cls.signon == SIGNONS && !q_strcasecmp(sv.name, state.run.map.cStr()) && sv.qcvm.time >= 1.5)
        {
            phase = Active;
            phaseSince = realtime;
            place();
        }
        return;
    }
    if(phase != Active)
    {
        return;
    }
    if(runAll && ++runAllFrames >= 30 && (!sv.active || sv.qcvm.time - placedAt >= runAllHoldSeconds))
    {
        // The hands given a weapon still hold it (nothing let go of it: the grips stay unpressed throughout).
        bool held = true;
        if(sv.active)
        {
            const progs::ServerVm vm;
            edict_t* p = progs::localPlayer();
            const int weapon2 = progs::fields().weapon2;
            const float now[2] = {!p ? -1.f : weapon2 >= 0 ? progs::fieldFloat(p, weapon2) : -1.f, !p ? -1.f : p->v.weapon};
            held = givenWeapon[0] == now[0] || givenWeapon[0] < 0.f;
            held = held && (givenWeapon[1] == now[1] || givenWeapon[1] < 0.f);
            Con_Printf("SCNHANDS|%s|%s|after %.1f s|main given %.0f now %.0f|off given %.0f now %.0f\n", state.run.id.cStr(),
                held ? "HELD" : "LOST", sv.qcvm.time - placedAt, givenWeapon[1], now[1], givenWeapon[0], now[0]);
            // What the spawn lines placed is still there: the same entity, or (freed: it made another) something else
            // standing within 64 units of its spot.
            int there = 0;
            for(const Placed& pl : state.placed)
            {
                const edict_t* e = pl.num < qcvm->num_edicts ? EDICT_NUM(pl.num) : nullptr;
                bool ok = e && !e->free && e->v.classname == pl.classname;
                for(int n = 1; !ok && n < qcvm->num_edicts; n++)
                {
                    const edict_t* o = EDICT_NUM(n);
                    ok = !o->free && o != p && o->v.modelindex != 0.f &&
                         glm::distance(glm::vec3{o->v.origin[0], o->v.origin[1], o->v.origin[2]}, pl.at) < 64.f;
                }
                there += ok ? 1 : 0;
                if(!ok)
                {
                    note(va("line %d: what it placed is gone after %.1f s", pl.line, sv.qcvm.time - placedAt));
                }
            }
            Con_Printf("SCNKEPT|%s|%d of %d placed still there\n", state.run.id.cStr(), there,
                static_cast<int>(state.placed.size()));
        }
        if(!held)
        {
            state.notes.pushBack(za::String{"a given weapon left its hand"});
        }
        const bool ok = state.notes.empty() && sv.active; // (a QC error ends the server)
        runAllFailed += ok ? 0 : 1;
        Con_Printf("SCNRUN|%s|%s|%d problems%s%s\n", state.run.id.cStr(), ok ? "OK" : "FAIL",
            static_cast<int>(state.notes.size()), ok ? "" : "|",
            ok ? "" : !sv.active ? "the server stopped (a QC error?)" : state.notes.front().cStr());
        runNext();
        return;
    }
    if(realtime < hintUntil && !state.hint.empty() && vrActive() && key_dest == key_game)
    {
        setup::drawViewText(state.hint, 0.35f); // below the eyes' line: the scene stays in view
    }
}

void command_f()
{
    const char* a = Cmd_Argc() >= 2 ? Cmd_Argv(1) : "";
    if(!q_strcasecmp(a, "reset"))
    {
        reset();
    }
    else if(!q_strcasecmp(a, "stop"))
    {
        stop();
    }
    else if(!q_strcasecmp(a, "hint"))
    {
        hintUntil = realtime + static_cast<double>(vr_scenario_hint_time.value);
        Con_Printf("vr_scenario: %s\n", state.hint.cStr());
    }
    else if(!q_strcasecmp(a, "list"))
    {
        za::Vector<za::String> ids;
        allIds(ids);
        Scenario s;
        for(const za::String& id : ids)
        {
            const bool ok = read(id.cStr(), s);
            Con_Printf("SCNLIST|%s|%s|%s\n", id.cStr(), ok ? "ok" : "errors", s.title.cStr());
        }
    }
    else if(!q_strcasecmp(a, "validate"))
    {
        validate();
    }
    else if(!q_strcasecmp(a, "runall"))
    {
        // vr_scenario runall [quit] [<id>...]: every file (or those), the game quit after the last (headless tests).
        runAllQuit = Cmd_Argc() >= 3 && !q_strcasecmp(Cmd_Argv(2), "quit");
        state.queue.clear();
        for(int k = runAllQuit ? 3 : 2; k < Cmd_Argc(); k++)
        {
            state.queue.pushBack(za::String{Cmd_Argv(k)});
        }
        if(state.queue.empty())
        {
            allIds(state.queue);
        }
        runAll = true;
        runAllCount = runAllFailed = 0;
        runNext();
    }
    else if(!q_strcasecmp(a, "where"))
    {
        where();
    }
    else if(a[0] && a[0] != '?')
    {
        start(a, Cmd_Argc() >= 3 ? Cmd_Argv(2) : "");
    }
    else
    {
        Con_Printf("vr_scenario <id> [hint] | reset | stop | hint | list | validate | runall | where: a test scene "
                   "(quakevr/scenarios/<id>.txt) run, the last run again, ended (its settings put back), its hint shown "
                   "again; the files listed, checked (SCNOK/SCNERR/SCNSUM), each run in turn (SCNRUN); where you stand "
                   "(a start line, the walls, the nearest liquid). Now: %s\n",
            phase == Idle ? "none" : state.run.id.cStr());
    }
}

} // namespace qvr::scenario

// Host_WriteConfigurationToFile, around Cvar_WriteVariables: a test's settings written as they were before it (each
// one's string swapped for the saved value while the file is written, no callback run).
extern "C" void VR_ScenarioConfigWrite(int begin)
{
    using namespace qvr::scenario;
    if((begin != 0) == swapped)
    {
        return;
    }
    if(begin)
    {
        state.live.clear();
        for(const Setting& s : state.saved)
        {
            cvar_t* var = Cvar_FindVar(s.name.cStr());
            state.live.pushBack(var ? var->string : nullptr);
            if(var)
            {
                var->string = s.value.cStr();
            }
        }
    }
    else
    {
        for(size_t i = 0; i < state.saved.size() && i < state.live.size(); i++)
        {
            if(cvar_t* var = Cvar_FindVar(state.saved[i].name.cStr()); var && state.live[i])
            {
                var->string = state.live[i];
            }
        }
        state.live.clear();
    }
    swapped = begin != 0;
}
