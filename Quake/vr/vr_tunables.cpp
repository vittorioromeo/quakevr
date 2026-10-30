// vr_tunables.cpp -- the tuning constants (vr_tunables.inc; vr_cvars.hpp, "Tuning constants"): their stand-in cvars
// for the QC, and live tuning in a development build.
//
// Live tuning (every build but a release, QVR_RELEASE): each constant is an entry of tunables[], read where it is used
// as name.value / name.string (one load). It is set by:
// - the console: `tune <name> <value>` (`tune <name>`: its value, default, range and help; `tune <part>`: the names
//   holding it; `tune`: the ones changed), `tune_reset [name]` (all without one); tab completes the names;
// - the Debug > Tunables page (vr_menu_tunables.inc): its rows (vr_tunables_rows.inc) set the stand-in cvars, and a set
//   of an unregistered cvar comes here (Cvar_SetQuick: VR_TunableSetVar);
// - tunables.cfg in the game folder: the ones off their defaults (`tune <name> "<value>"` lines), read once after the
//   config (vr_migrate_config), written with the config (Host_WriteConfiguration: at quit, the menu's Save Default; not by a copy that never
//   writes its config). A console notice says how many it set.
// The values become the shipped defaults by Misc/quakevr/tune_promote.py (a tunables.cfg's values into vr_tunables.inc,
// on a worker's tree); the game never writes a source file.
//
// A release build reads each constant's default, folded in at compile time; none of this is in it (the stand-ins stay,
// constant: the QC reads them by name the same way in both builds).

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_testkit.hpp"

namespace qvr
{

#ifdef QVR_RELEASE

namespace
{

const cvar_t tunableStandIns[] = {
#define QVR_TUNABLE(name, def) {#name, def, CVAR_NONE, tunableValue(def), def},
#include "vr_tunables.inc"
#undef QVR_TUNABLE
    {nullptr, nullptr, CVAR_NONE, 0.f, nullptr},
};

} // namespace

const cvar_t* tunableFind(const char* name)
{
    for(const cvar_t& var : tunableStandIns)
    {
        if(var.name && !q_strcasecmp(var.name, name))
        {
            return &var;
        }
    }
    return nullptr;
}

void tunablesInit()
{
}

void tunablesLoad()
{
}

} // namespace qvr

extern "C" int VR_TunableSetVar(cvar_t* var, const char* value)
{
    (void)var;
    (void)value;
    return 0;
}

extern "C" int VR_TunablesWrite(void)
{
    return 1; // (no tunables.cfg in a release)
}

#else // development build: live tuning

Tunable tunables[tunableCount] = {
#define QVR_TUNABLE(name, def) {#name, def, tunableValue(def)},
#include "vr_tunables.inc"
#undef QVR_TUNABLE
};

namespace
{

constexpr const char* tunableDefaults[tunableCount] = {
#define QVR_TUNABLE(name, def) def,
#include "vr_tunables.inc"
#undef QVR_TUNABLE
};

// The QC's (and the Tunables page's) view of each: its string and value kept the constant's (tunableSet).
cvar_t tunableStandIns[tunableCount] = {
#define QVR_TUNABLE(name, def) {#name, def, CVAR_NONE, tunableValue(def), def},
#include "vr_tunables.inc"
#undef QVR_TUNABLE
};

// The rows' cycles' choices, then the rows (vr_tunables_rows.inc).
#define QVR_TUNABLE_GROUP(label)
#define QVR_TUNABLE_SLIDER(name, label, min, max, step, format, hardMin, hardMax, help)
#define QVR_TUNABLE_TEXT(name, label, help)
#define QVR_TUNABLE_CYCLE(name, label, help, ...) constexpr TunableChoice tunableChoices_##name[] = {__VA_ARGS__};
#include "vr_tunables_rows.inc"
#undef QVR_TUNABLE_GROUP
#undef QVR_TUNABLE_SLIDER
#undef QVR_TUNABLE_TEXT
#undef QVR_TUNABLE_CYCLE

constexpr TunableRow tunableRowTable[] = {
#define QVR_TUNABLE_GROUP(label) {TunableRow::Group, -1, label, 0.f, 0.f, 0.f, nullptr, 0.f, 0.f, nullptr, nullptr, 0},
#define QVR_TUNABLE_SLIDER(name, label, min, max, step, format, hardMin, hardMax, help)                                 \
    {TunableRow::Slider, tunableId_##name, label, min, max, step, format, hardMin, hardMax, help, nullptr, 0},
#define QVR_TUNABLE_TEXT(name, label, help)                                                                             \
    {TunableRow::Text, tunableId_##name, label, 0.f, 0.f, 0.f, nullptr, 0.f, 0.f, help, nullptr, 0},
#define QVR_TUNABLE_CYCLE(name, label, help, ...)                                                                       \
    {TunableRow::Cycle, tunableId_##name, label, 0.f, 0.f, 0.f, nullptr, 0.f, 0.f, help, tunableChoices_##name,      \
        static_cast<int>(sizeof(tunableChoices_##name) / sizeof(tunableChoices_##name[0]))},
#include "vr_tunables_rows.inc"
#undef QVR_TUNABLE_GROUP
#undef QVR_TUNABLE_SLIDER
#undef QVR_TUNABLE_TEXT
#undef QVR_TUNABLE_CYCLE
};
constexpr int tunableRowCount = static_cast<int>(sizeof(tunableRowTable) / sizeof(tunableRowTable[0]));

// One row a constant, in vr_tunables.inc's order (a constant added there needs its row).
[[nodiscard]] constexpr bool rowsMatchTunables()
{
    int next = 0;
    for(const TunableRow& r : tunableRowTable)
    {
        if(r.kind != TunableRow::Group)
        {
            if(r.id != next)
            {
                return false;
            }
            ++next;
        }
    }
    return next == tunableCount;
}
static_assert(rowsMatchTunables(), "vr_tunables_rows.inc: one row for each line of vr_tunables.inc, in its order");

// Each constant's row and the group (page) above it, by TunableId.
struct RowIndex
{
    int row[tunableCount];
    int group[tunableCount];
};
[[nodiscard]] constexpr RowIndex rowIndexOf()
{
    RowIndex index{};
    int group = -1;
    for(int i = 0; i < tunableRowCount; i++)
    {
        const TunableRow& r = tunableRowTable[i];
        if(r.kind == TunableRow::Group)
        {
            group = i;
            continue;
        }
        index.row[r.id] = i;
        index.group[r.id] = group;
    }
    return index;
}
constexpr RowIndex rowIndex = rowIndexOf();

// A constant's value set off its default (Z_Strdup'd; nullptr: its default).
char* overrides[tunableCount]{};
int overrideCount = 0;
int loadedCount = -1; // how many tunables.cfg set at start (-1: not read)
bool fileLoaded = false; // tunables.cfg read (until then it is not written: it would lose what is in it)
bool fileDirty = false;  // a constant set since it was read or written

cvar_t vr_tunables_show_changed = {"vr_tunables_show_changed", "0", CVAR_NONE}; // the Tunables page: changed ones only
cvar_t vr_tunables_page = {"vr_tunables_page", "0", CVAR_NONE}; // the Tunables page: one page's (its group, from 1)

[[nodiscard]] bool numeric(const char* s)
{
    char* end = nullptr;
    strtod(s, &end);
    return end != s && *end == '\0';
}

[[nodiscard]] bool sameAsDefault(int id, const char* value)
{
    const char* def = tunableDefaults[id];
    return !strcmp(value, def) || (numeric(value) && numeric(def) && Q_atof(value) == Q_atof(def));
}

[[nodiscard]] za::String filePath()
{
    return za::String{va("%s/tunables.cfg", com_gamedir)};
}

void describe(int id)
{
    const TunableRow& r = tunableRow(id);
    Con_Printf("%s = \"%s\"%s (default \"%s\")\n", tunables[id].name, tunables[id].string,
        tunableChanged(id) ? " [changed]" : "", tunableDefaults[id]);
    if(r.kind == TunableRow::Slider)
    {
        Con_Printf("  %s: %g to %g, step %g (as far as %g to %g)\n", r.label, r.min, r.max, r.step, r.hardMin, r.hardMax);
    }
    else if(r.kind == TunableRow::Cycle)
    {
        Con_Printf("  %s:", r.label);
        for(int c = 0; c < r.choiceCount; c++)
        {
            Con_Printf(" %g %s%s", r.choices[c].value, r.choices[c].label, c + 1 < r.choiceCount ? "," : "");
        }
        Con_Printf("\n");
    }
    else
    {
        Con_Printf("  %s (a text: tune %s \"...\")\n", r.label, tunables[id].name);
    }
    Con_Printf("  %s%s%s\n", tunableGroup(id), r.help && r.help[0] ? ": " : "", r.help ? r.help : "");
}

void listChanged()
{
    for(int id = 0; id < tunableCount; id++)
    {
        if(overrides[id])
        {
            Con_Printf("  %s = \"%s\" (default \"%s\")\n", tunables[id].name, tunables[id].string, tunableDefaults[id]);
        }
    }
}

void tune_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("tune <name> [value]: a tuning constant's value, range and help, or set it (live; kept in "
                   "tunables.cfg). tune <part of a name>: the names holding it. tune_reset [name]: back to the default "
                   "(all without a name). Debug > Tunables: the same as sliders.\n");
        Con_Printf("%d of %d tuning constants set off their defaults%s\n", overrideCount, static_cast<int>(tunableCount),
            overrideCount ? ":" : "");
        listChanged();
        return;
    }
    const int id = tunableIndex(Cmd_Argv(1));
    if(id < 0)
    {
        int found = 0;
        for(int i = 0; i < tunableCount; i++)
        {
            if(q_strcasestr(tunables[i].name, Cmd_Argv(1)))
            {
                if(found < 60)
                {
                    Con_Printf("  %s = \"%s\"%s\n", tunables[i].name, tunables[i].string, overrides[i] ? " [changed]" : "");
                }
                found++;
            }
        }
        if(!found)
        {
            Con_Printf("tune: no tuning constant \"%s\" (tune with a part of a name lists the names holding it)\n",
                Cmd_Argv(1));
        }
        else if(found > 60)
        {
            Con_Printf("  ... %d more\n", found - 60);
        }
        return;
    }
    if(Cmd_Argc() == 2)
    {
        describe(id);
        return;
    }
    za::String value{Cmd_Argv(2)};
    for(int a = 3; a < Cmd_Argc(); a++)
    {
        value += ' ';
        value += Cmd_Argv(a);
    }
    tunableSet(id, value.cStr());
    Con_Printf("%s = \"%s\"%s\n", tunables[id].name, tunables[id].string, overrides[id] ? "" : " (its default)");
}

void tuneReset_f()
{
    if(Cmd_Argc() < 2)
    {
        const int was = overrideCount;
        tunablesResetAll();
        Con_Printf("tune_reset: %d tuning constant%s back to %s default\n", was, was == 1 ? "" : "s",
            was == 1 ? "its" : "their");
        return;
    }
    const int id = tunableIndex(Cmd_Argv(1));
    if(id < 0)
    {
        Con_Printf("tune_reset: no tuning constant \"%s\"\n", Cmd_Argv(1));
        return;
    }
    tunableReset(id);
    Con_Printf("%s = \"%s\" (its default)\n", tunables[id].name, tunables[id].string);
}

// vr_tunables_test <step> (Misc/quakevr/tunables_test.sh): live tuning's round trip, every reader checked. Steps:
// set (a number by tune, one by a Tunables page row's Cvar_SetQuick, a text), write (tunables.cfg as at quit), checkfile
// (its lines), reload (the values back to their defaults in memory, the file read again), checkloaded (as set: a new
// start), reset (tune_reset: all back), checkreset, done (the verdict line).
testkit::Tally testTally;

void testCheck(bool ok, const char* what)
{
    testTally.check(ok, "tunables_test", what);
}

// Every reader of a number: C++ (the name), the QC's cvar() (Cvar_VariableValue/String) and cvar_hmake/hget (the
// stand-in tunableFind gives), the Tunables page's row (the stand-in).
void testReaders(const char* name, float cxx, float want)
{
    const cvar_t* standIn = tunableFind(name);
    testCheck(cxx == want, va("%s: C++ reads %g (want %g)", name, cxx, want));
    testCheck(Cvar_VariableValue(name) == want, va("%s: QC cvar() reads %g", name, Cvar_VariableValue(name)));
    testCheck(standIn && standIn->value == want, va("%s: QC cvar_hget reads %g", name, standIn ? standIn->value : -1.f));
}

void testExpectSet(bool set)
{
    testReaders("vr_throw_gain_lo", vr_throw_gain_lo.value, set ? 2.25f : 1.5f);
    testReaders("vr_throw_gain_hi", vr_throw_gain_hi.value, set ? 7.5f : 6.f);
    const char* tint = set ? "1 0.5 0.25" : "0.85 0.95 1.15";
    testCheck(!strcmp(vr_bullettime_fx_tint.string, tint), va("vr_bullettime_fx_tint: C++ reads \"%s\"", vr_bullettime_fx_tint.string));
    testCheck(!strcmp(Cvar_VariableString("vr_bullettime_fx_tint"), tint),
        va("vr_bullettime_fx_tint: QC cvar_string() reads \"%s\"", Cvar_VariableString("vr_bullettime_fx_tint")));
    testCheck(overrideCount == (set ? 3 : 0), va("%d changed", overrideCount));
}

void tunablesTest_f()
{
    const char* step = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!strcmp(step, "set"))
    {
        tunablesResetAll();
        testExpectSet(false);
        Cmd_ExecuteString("tune vr_throw_gain_lo 2.25", src_command);
        Cvar_SetValueQuick(tunableStandIn(tunableId_vr_throw_gain_hi), 7.5f); // as a Tunables page slider sets it
        Cmd_ExecuteString("tune vr_bullettime_fx_tint 1 0.5 0.25", src_command);
        testExpectSet(true);
        Cmd_ExecuteString("tune vr_throw_gain_lo 1.50", src_command); // its default, written another way: not changed
        testCheck(!tunableChanged(tunableId_vr_throw_gain_lo) && overrideCount == 2, "set to its default: not changed");
        Cmd_ExecuteString("tune vr_throw_gain_lo 2.25", src_command);
    }
    else if(!strcmp(step, "write"))
    {
        fileDirty = true;
        VR_TunablesWrite();
    }
    else if(!strcmp(step, "checkfile"))
    {
        long len = 0;
        char* text = reinterpret_cast<char*>(COM_LoadMallocFile_TextMode_OSPath(filePath().cStr(), &len));
        testCheck(text && strstr(text, "tune vr_throw_gain_lo \"2.25\"\n") && strstr(text, "tune vr_throw_gain_hi \"7.5\"\n") &&
                strstr(text, "tune vr_bullettime_fx_tint \"1 0.5 0.25\"\n") && !strstr(text, "vr_throw_peak_span"),
            "tunables.cfg holds the three changed, nothing else");
        free(text);
    }
    else if(!strcmp(step, "reload"))
    {
        const bool was = fileDirty;
        tunablesResetAll();
        testExpectSet(false);
        fileLoaded = false;
        tunablesLoad();
        testExpectSet(true);
        fileDirty = was;
    }
    else if(!strcmp(step, "checkloaded"))
    {
        testCheck(loadedCount == 3, va("tunables.cfg set %d at start (and said so)", loadedCount));
        testExpectSet(true);
    }
    else if(!strcmp(step, "checkset"))
    {
        testExpectSet(true);
    }
    else if(!strcmp(step, "reset"))
    {
        Cmd_ExecuteString("tune_reset vr_throw_gain_lo", src_command);
        testCheck(!tunableChanged(tunableId_vr_throw_gain_lo) && vr_throw_gain_lo.value == 1.5f, "tune_reset <name>");
        Cmd_ExecuteString("tune_reset", src_command);
        testExpectSet(false);
    }
    else if(!strcmp(step, "done"))
    {
        testTally.verdict("tunables_test");
    }
    else
    {
        Con_Printf("vr_tunables_test set|write|checkfile|reload|checkloaded|checkset|reset|done\n");
    }
}

void tuneComplete(const char* partial)
{
    for(int id = 0; id < tunableCount; id++)
    {
        Con_AddToTabList(tunables[id].name, partial, overrides[id] ? "changed" : "tunable");
    }
}

} // namespace

const cvar_t* tunableFind(const char* name)
{
    const int id = tunableIndex(name);
    return id >= 0 ? &tunableStandIns[id] : nullptr;
}

int tunableIndex(const char* name)
{
    for(int id = 0; id < tunableCount; id++)
    {
        if(!q_strcasecmp(tunables[id].name, name))
        {
            return id;
        }
    }
    return -1;
}

const char* tunableDefault(int id)
{
    return tunableDefaults[id];
}

bool tunableChanged(int id)
{
    return overrides[id] != nullptr;
}

int tunablesChanged()
{
    return overrideCount;
}

const TunableRow& tunableRow(int id)
{
    return tunableRowTable[rowIndex.row[id]];
}

const char* tunableGroup(int id)
{
    return rowIndex.group[id] >= 0 ? tunableRowTable[rowIndex.group[id]].label : "Other";
}

const TunableRow* tunableRows(int* count)
{
    *count = tunableRowCount;
    return tunableRowTable;
}

cvar_t* tunableStandIn(int id)
{
    return &tunableStandIns[id];
}

void tunableSet(int id, const char* value)
{
    if(sameAsDefault(id, value))
    {
        if(!overrides[id])
        {
            return;
        }
        Z_Free(overrides[id]);
        overrides[id] = nullptr;
        overrideCount--;
        tunables[id].string = tunableDefaults[id];
        tunables[id].value = tunableValue(tunableDefaults[id]);
    }
    else
    {
        if(overrides[id] && !strcmp(overrides[id], value))
        {
            return;
        }
        char* now = Z_Strdup(value);
        if(overrides[id])
        {
            Z_Free(overrides[id]);
        }
        else
        {
            overrideCount++;
        }
        overrides[id] = now;
        tunables[id].string = now;
        tunables[id].value = Q_atof(now);
    }
    tunableStandIns[id].string = tunables[id].string;
    tunableStandIns[id].value = tunables[id].value;
    fileDirty = true;
}

void tunableReset(int id)
{
    tunableSet(id, tunableDefaults[id]);
}

void tunablesResetAll()
{
    for(int id = 0; id < tunableCount; id++)
    {
        tunableReset(id);
    }
}

void tunablesInit()
{
    // (a constant's compile-time value is Q_atof of its default, as its cvar's was; a difference is a bug here)
    for(int id = 0; id < tunableCount; id++)
    {
        if(tunables[id].value != Q_atof(tunableDefaults[id]))
        {
            Con_Printf("VR: tuning constant %s: %.9g, not %.9g\n", tunables[id].name, tunables[id].value,
                Q_atof(tunableDefaults[id]));
        }
    }
    if(cmd_function_t* cmd = Cmd_AddCommand("tune", tune_f))
    {
        cmd->completion = tuneComplete;
    }
    if(cmd_function_t* cmd = Cmd_AddCommand("tune_reset", tuneReset_f))
    {
        cmd->completion = tuneComplete;
    }
    Cmd_AddCommand("vr_tunables_test", tunablesTest_f);
    Cvar_RegisterVariable(&vr_tunables_show_changed);
    Cvar_RegisterVariable(&vr_tunables_page);
}

cvar_t& tunablesPageCvar()
{
    return vr_tunables_page;
}

cvar_t& tunablesShowChangedCvar()
{
    return vr_tunables_show_changed;
}

// tunables.cfg, read once after the config (vr_migrate_config): its constants set as it says. A name no longer a
// constant (renamed, made a setting again, removed) is reported and left out.
void tunablesLoad()
{
    if(fileLoaded)
    {
        return;
    }
    fileLoaded = true;
    const za::String path = filePath();
    long len = 0;
    char* text = reinterpret_cast<char*>(COM_LoadMallocFile_TextMode_OSPath(path.cStr(), &len));
    if(!text)
    {
        return;
    }
    const char* data = text;
    int set = 0;
    while((data = COM_Parse(data)) != nullptr)
    {
        if(strcmp(com_token, "tune"))
        {
            continue;
        }
        if(!(data = COM_Parse(data)))
        {
            break;
        }
        char name[128];
        q_strlcpy(name, com_token, sizeof(name));
        if(!(data = COM_Parse(data)))
        {
            break;
        }
        const int id = tunableIndex(name);
        if(id < 0)
        {
            Con_Printf("\x02tunables.cfg: %s is no longer a tuning constant (left out)\n", name);
            continue;
        }
        tunableSet(id, com_token);
        set++;
    }
    free(text);
    fileDirty = false;
    if(overrideCount)
    {
        Con_Printf("\x02%d tuning constant%s set off %s default by tunables.cfg (tune lists them; tune_reset puts them "
                   "back)\n", overrideCount, overrideCount == 1 ? "" : "s", overrideCount == 1 ? "its" : "their");
    }
    loadedCount = set;
}

} // namespace qvr

// With the config (Host_WriteConfiguration: at quit, and the menu's Save Default): the constants off their defaults, if
// any was set since; none: the file goes. 0 when the write failed (the file kept as it was), else 1.
extern "C" int VR_TunablesWrite(void)
{
    using namespace qvr;
    if(!fileLoaded || !fileDirty)
    {
        return 1;
    }
    const za::String path = filePath();
    if(!overrideCount)
    {
        if(Sys_FileType(path.cStr()) == FS_ENT_FILE)
        {
            Sys_remove(path.cStr());
            Con_Printf("Removed tunables.cfg (no tuning constant off its default)\n");
        }
        fileDirty = false;
        return 1;
    }
    const za::String tmp = path + ".tmp";
    FILE* f = Sys_fopen(tmp.cStr(), "w");
    if(!f)
    {
        Con_Printf("\x02" "Couldn't write tunables.cfg\n");
        return 0;
    }
    fprintf(f, "// tunables.cfg -- Quake VR's tuning constants set off their defaults (tune; Debug > Tunables), written at "
               "quit\n// and read at start after the config. Misc/quakevr/tune_promote.py makes them the shipped "
               "defaults.\n");
    for(int id = 0; id < tunableCount; id++)
    {
        if(overrides[id])
        {
            fprintf(f, "tune %s \"%s\"\n", tunables[id].name, overrides[id]);
        }
    }
    if(ferror(f) | fclose(f) || Sys_ReplaceFile(tmp.cStr(), path.cStr()) != 0)
    {
        Con_Printf("\x02" "Couldn't write tunables.cfg (the disk full?): the file was kept as it was\n");
        Sys_remove(tmp.cStr());
        return 0;
    }
    fileDirty = false;
    Con_Printf("Wrote tunables.cfg (%d tuning constant%s)\n", overrideCount, overrideCount == 1 ? "" : "s");
    return 1;
}

// Cvar_SetQuick of a cvar never registered: a tuning constant's stand-in (a Tunables page row) sets the constant.
extern "C" int VR_TunableSetVar(cvar_t* var, const char* value)
{
    using namespace qvr;
    if(var < tunableStandIns || var >= tunableStandIns + tunableCount)
    {
        return 0;
    }
    tunableSet(static_cast<int>(var - tunableStandIns), value);
    return 1;
}

#endif
