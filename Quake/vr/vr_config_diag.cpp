// vr_config_diag.cpp -- vr_config_test: the sparse config's round trip (configVersion 218; vr_cvars.cpp VR_ConfigSaves,
// docs/vr-port/CODE_STYLE.md, "Settings and the config"). Driven by a script, one step a command, so the configs are
// executed and written by the game's own exec and writeconfig (Misc/quakevr/config_sparse_test.sh):
//   exec <full config>; vr_migrate_config                           an old full config, as the game loads it
//   writeconfig; vr_config_test othercopy; writeconfig; vr_config_test checkmerge   another copy's edits taken
//   vr_config_test snap                                              the values to come back to
//   writeconfig <sparse>; vr_config_test checkfile <sparse>.cfg      only changed vr_ settings written, each as snapped
//   vr_config_test scramble; exec <sparse>.cfg; vr_config_test compare   back to the snapped values from any state
//   vr_config_test newdefault; exec <sparse>.cfg; vr_config_test checkdefault   a changed default reaches it
//   vr_config_test done                                              "config_test: N passed, M failed"
// "vr_config_test dumpfull <name>" writes every archived setting (the full config's lines) for comparing builds.
// The config's tracking (vr_cvars.cpp ConfigTrack; REVIEW_NEXT_2026-11-18.md, finding 4): "vr_config_test tracked"
// right after a writeconfig (nothing changed, the tracking as the file and the settings read afresh); "vr_config_test
// trackedit" its writes, a failed one and a name two settings share; "vr_config_test cost [n]" a menu close's no-change
// test timed, saveConfigNow's against the map of every setting it made until 2026-11-19.

#include "vr_alloccount.hpp"
#include "vr_api.h"
#include "vr_config.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/Vocabulary/Span.hpp"
#include "vr_zancle.hpp"

#include <cstdio>
#include <cstdlib>

namespace qvr
{

namespace
{

// A default moved by "newdefault": the setting, its new default, and the value it must have after the sparse config.
struct MovedDefault
{
    cvar_t* var = nullptr;
    za::String expect;
};

struct ConfigTest
{
    testkit::Tally tally;
    ankerl::unordered_dense::map<za::String, za::String> snap; // every archived setting's value at "snap"
    MovedDefault moved[3];                                     // newdefault: one at its default, a slot one, a changed one
    MovedDefault merged[2]; // othercopy: one another copy put back to its default (its line gone), one it changed
};
ConfigTest configTest;

constexpr const char* testName = "config_test";

[[nodiscard]] bool isSlot(const cvar_t* var)
{
    return !ZA_STRNCMP(var->name, "vr_wofs_", 8) || !ZA_STRNCMP(var->name, "vr_prop_", 8);
}

[[nodiscard]] bool isNumber(const char* s)
{
    char* end;
    strtod(s, &end);
    return end != s && !*end;
}

// A setting the test may set freely: a vr_ one, archived, with no callback, writable.
[[nodiscard]] bool plain(const cvar_t* var)
{
    return !ZA_STRNCMP(var->name, "vr_", 3) && !var->callback && !(var->flags & (CVAR_ROM | CVAR_LOCKED)) &&
           var->default_string;
}

// The archived settings, in their names' order.
[[nodiscard]] cvar_t* firstArchived()
{
    return Cvar_FindVarAfter("", CVAR_ARCHIVE);
}

// (After the cvars, the data tables' values, each table's in turn: no cvars, weapons.txt's and props.txt's stand-ins,
// written in the config as the vr_ settings.)
[[nodiscard]] cvar_t* nextArchived(const cvar_t* var)
{
    const auto tables = standInTables();
    for(za::SizeT t = 0; t < tables.size(); t++)
    {
        const za::Span<cvar_t> table = tables[t];
        if(var >= table.data() && var < table.data() + table.size())
        {
            if(var + 1 < table.data() + table.size())
            {
                return const_cast<cvar_t*>(var + 1);
            }
            return t + 1 < tables.size() ? tables[t + 1].data() : nullptr;
        }
    }
    cvar_t* next = Cvar_FindVarAfter(var->name, CVAR_ARCHIVE);
    return next ? next : tables[0].data();
}

void snap()
{
    configTest.snap.clear();
    for(cvar_t* var = firstArchived(); var; var = nextArchived(var))
    {
        configTest.snap[za::String{var->name}] = var->string;
    }
    Con_Printf("%s: snap: %d archived settings\n", testName, static_cast<int>(configTest.snap.size()));
}

// The config's `name "value"` lines, and whether a "vr_config_defaults" line came before the first vr_ one.
void checkFile(const char* name)
{
    za::String text;
    if(!configTest.tally.check(files::readText(va("%s/%s", com_gamedir, name), text), testName, "the config not read"))
    {
        return;
    }
    ankerl::unordered_dense::map<za::String, za::String> lines;
    bool resetFirst = false;
    bool sawVr = false;
    int unchangedVr = 0, wrongValue = 0, vrLines = 0, slotLines = 0;
    const za::StringView all{text.cStr()};
    for(size_t start = 0; start < all.size();)
    {
        size_t end = all.find('\n', start);
        end = end == za::StringView::nPos ? all.size() : end;
        za::StringView line = all.substrByPosLen(start, end - start);
        start = end + 1;
        if(line == za::StringView{"vr_config_defaults"})
        {
            resetFirst = !sawVr;
            continue;
        }
        const size_t space = line.find(' ');
        if(space == za::StringView::nPos || line.size() < space + 3 || line[space + 1] != '"' || line[line.size() - 1] != '"')
        {
            continue; // (a bind, vid_restart...)
        }
        const za::String cname{line.substrByPosLen(0, space)};
        const za::String value{line.substrByPosLen(space + 2, line.size() - space - 3)};
        cvar_t* var = Cvar_FindVar(cname.cStr());
        if(!var || !(var->flags & CVAR_ARCHIVE))
        {
            continue;
        }
        lines[cname] = value;
        if(!ZA_STRNCMP(var->name, "vr_", 3))
        {
            sawVr = true;
            vrLines++;
            slotLines += isSlot(var) ? 1 : 0;
            if(!VR_ConfigSaves(var))
            {
                if(unchangedVr++ < 5)
                {
                    Con_Printf("%s: FAIL %s \"%s\" written at its default\n", testName, var->name, value.cStr());
                }
            }
        }
        const auto was = configTest.snap.find(cname);
        if(was == configTest.snap.end() || was->second != value)
        {
            if(wrongValue++ < 5)
            {
                Con_Printf("%s: FAIL %s \"%s\" written, \"%s\" snapped\n", testName, var->name, value.cStr(),
                    was == configTest.snap.end() ? "(none)" : was->second.cStr());
            }
        }
    }
    int missing = 0, archived = 0, changedVr = 0, changedSlots = 0;
    for(cvar_t* var = firstArchived(); var; var = nextArchived(var))
    {
        archived++;
        const bool vr = !ZA_STRNCMP(var->name, "vr_", 3);
        const bool saves = VR_ConfigSaves(var) != 0;
        changedVr += vr && saves ? 1 : 0;
        changedSlots += vr && saves && isSlot(var) ? 1 : 0;
        if(saves && !lines.contains(za::String{var->name}) && missing++ < 5)
        {
            Con_Printf("%s: FAIL %s \"%s\" not written\n", testName, var->name, var->string);
        }
    }
    Con_Printf("%s: %s: %d lines of %d archived settings; vr_ %d (slots %d) of %d changed (slots %d)\n", testName, name,
        static_cast<int>(lines.size()), archived, vrLines, slotLines, changedVr, changedSlots);
    configTest.tally.check(resetFirst, testName, "no vr_config_defaults line before the vr_ settings");
    configTest.tally.check(unchangedVr == 0, testName, "vr_ settings at their defaults written");
    configTest.tally.check(wrongValue == 0, testName, "settings written with other values than snapped");
    configTest.tally.check(missing == 0, testName, "changed settings not written");
    configTest.tally.check(vrLines == changedVr, testName, "vr_ lines and changed vr_ settings differ");
}

// Every plain vr_ setting off its value (at its default or not): the sparse config must bring each back.
void scramble()
{
    int count = 0;
    for(cvar_t* var = firstArchived(); var; var = nextArchived(var))
    {
        if(plain(var) && isNumber(var->default_string))
        {
            Cvar_SetQuick(var, sameSettingValue(var->string, "7") ? "8" : "7");
            count++;
        }
    }
    Con_Printf("%s: scramble: %d settings\n", testName, count);
}

void compare()
{
    int differ = 0, sameNumber = 0;
    for(cvar_t* var = firstArchived(); var; var = nextArchived(var))
    {
        const auto was = configTest.snap.find(za::String{var->name});
        if(was == configTest.snap.end() || was->second == za::StringView{var->string})
        {
            continue;
        }
        if(sameSettingValue(was->second.cStr(), var->string))
        {
            sameNumber++; // (the default's text, the same number: "0.50" for "0.5")
            continue;
        }
        if(differ++ < 8)
        {
            Con_Printf("%s: FAIL %s \"%s\", snapped \"%s\"\n", testName, var->name, var->string, was->second.cStr());
        }
    }
    Con_Printf("%s: compare: %d differ, %d the same number in other text\n", testName, differ, sameNumber);
    configTest.tally.check(differ == 0, testName, "settings not as snapped after the sparse config");
}

// Defaults moved as a new build's would be (vr_default): one setting at its default (no line), a slot one at its
// default, one the config changes (its line wins).
void newDefault()
{
    MovedDefault* m = configTest.moved;
    for(cvar_t* var = firstArchived(); var; var = nextArchived(var))
    {
        if(!plain(var) || !isNumber(var->default_string) || !isNumber(var->string))
        {
            continue;
        }
        const bool atDefault = !VR_ConfigSaves(var);
        MovedDefault* slot = atDefault && !isSlot(var) && !m[0].var ? &m[0]
                             : atDefault && isSlot(var) && !m[1].var ? &m[1]
                             : !atDefault && !isSlot(var) && !m[2].var && !strstr(var->name, "_version") ? &m[2]
                                                                                                                 : nullptr;
        if(!slot)
        {
            continue;
        }
        const za::String was{var->string};
        const za::String moved{va("%g", atDefault ? Q_atof(var->default_string) + 1.5 : Q_atof(var->string) + 2.5)};
        Z_Free(const_cast<char*>(var->default_string));
        var->default_string = Z_Strdup(moved.cStr());
        slot->var = var;
        slot->expect = atDefault ? moved : was;
        Con_Printf("%s: newdefault: %s default %s (expect %s)\n", testName, var->name, moved.cStr(), slot->expect.cStr());
    }
    configTest.tally.check(m[0].var && m[1].var && m[2].var, testName, "newdefault: no settings to move");
}

void checkDefault()
{
    for(const MovedDefault& m : configTest.moved)
    {
        if(m.var)
        {
            configTest.tally.check(sameSettingValue(m.var->string, m.expect.cStr()), testName,
                va("%s \"%s\" after the sparse config, expected \"%s\"", m.var->name, m.var->string, m.expect.cStr()));
        }
    }
}

// Another copy of the game on the same (sparse) config, as VR_ConfigMergeOthers sees it: the game folder's config
// edited on disk, a changed setting's line removed (that copy put it back to its default) and a line added for one at its
// default (that copy changed it). The next "writeconfig" (the game folder's config: merged) takes both.
void otherCopy()
{
    const char* path = va("%s/%s", com_gamedir, CONFIG_NAME);
    za::String text;
    if(!configTest.tally.check(files::readText(path, text), testName, "othercopy: the config not read"))
    {
        return;
    }
    MovedDefault* m = configTest.merged;
    m[0] = m[1] = MovedDefault{};
    for(cvar_t* var = firstArchived(); var; var = nextArchived(var))
    {
        if(!plain(var) || !isNumber(var->default_string) || isSlot(var) || strstr(var->name, "_version"))
        {
            continue;
        }
        const bool atDefault = !VR_ConfigSaves(var);
        if(!atDefault && !m[0].var)
        {
            m[0] = {var, za::String{var->default_string}};
        }
        else if(atDefault && !m[1].var)
        {
            m[1] = {var, za::String{va("%g", Q_atof(var->default_string) + 1.5)}};
        }
    }
    if(!configTest.tally.check(m[0].var && m[1].var, testName, "othercopy: no settings to change"))
    {
        return;
    }
    const za::String gone{va("%s \"", m[0].var->name)};
    za::String edited;
    const za::StringView all{text.cStr()};
    for(size_t start = 0; start < all.size();)
    {
        size_t end = all.find('\n', start);
        end = end == za::StringView::nPos ? all.size() : end + 1;
        const za::StringView line = all.substrByPosLen(start, end - start);
        start = end;
        if(line.size() < gone.size() || line.substrByPosLen(0, gone.size()) != za::StringView{gone.cStr()})
        {
            edited += line;
        }
    }
    edited += va("%s \"%s\"\n", m[1].var->name, m[1].expect.cStr());
    configTest.tally.check(files::writeBytes(path, edited.cStr(), edited.size()), testName, "othercopy: not written");
    Con_Printf("%s: othercopy: %s's line removed (expect %s), %s \"%s\" added\n", testName, m[0].var->name,
        m[0].expect.cStr(), m[1].var->name, m[1].expect.cStr());
}

void checkMerge()
{
    for(const MovedDefault& m : configTest.merged)
    {
        if(m.var)
        {
            configTest.tally.check(sameSettingValue(m.var->string, m.expect.cStr()), testName,
                va("merge: %s \"%s\", expected \"%s\" from the other copy", m.var->name, m.var->string, m.expect.cStr()));
        }
    }
}

void dumpFull(const char* name)
{
    FILE* f = Sys_fopen(va("%s/%s", com_gamedir, name), "wb");
    if(!f)
    {
        Con_Printf("%s: can't write %s\n", testName, name);
        return;
    }
    int count = 0;
    for(cvar_t* var = firstArchived(); var; var = nextArchived(var))
    {
        fprintf(f, "%s \"%s\"\n", var->name, var->string);
        count++;
    }
    fclose(f);
    Con_Printf("%s: dumpfull: %d settings to %s\n", testName, count, name);
}

// hasline <file> <word>... <value>: the game folder's file, as on disk now, has the line `<word>... "<value>"`
// (savecfg_test.sh: the menu's Save Default wrote the config and tunables.cfg before any quit).
void hasLine()
{
    za::String text;
    const char* name = Cmd_Argv(2);
    if(!configTest.tally.check(files::readText(va("%s/%s", com_gamedir, name), text), testName, va("%s not read", name)))
    {
        return;
    }
    za::String want = "\n";
    for(int i = 3; i < Cmd_Argc() - 1; i++)
    {
        want += Cmd_Argv(i);
        want += " ";
    }
    want += va("\"%s\"", Cmd_Argv(Cmd_Argc() - 1)); // (then \n or \r\n: the closing quote ends the value)
    const za::String all = za::String{"\n"} + text;
    configTest.tally.check(strstr(all.cStr(), want.cStr()) != nullptr, testName, va("%s has no line %s", name, want.cStr() + 1));
}

// A check's line either way: "config_test: PASS|FAIL <what>".
void expect(bool ok, const char* what)
{
    if(configTest.tally.check(ok, testName, what))
    {
        Con_Printf("%s: PASS %s\n", testName, what);
    }
}

// The no-change test's count (saveConfigNow's) and the map's (as it was counted until 2026-11-19), both `want`.
void expectChanged(int want, const char* when)
{
    const int now = configChangedCount(), byMap = configChangedCountByMap();
    expect(now == want && byMap == want, va("%s: %d changed (the map's count %d), %d expected", when, now, byMap, want));
}

// The tracking as the config and the settings read afresh (what VR_ConfigWritten read back until 2026-11-19), but for
// `hereDiffers` settings changed since.
void expectTracked(const char* path, int hereDiffers, const char* when)
{
    const ConfigTrackCheck c = configTrackCheck(path);
    expect(c.on && c.fileDiffers == 0 && c.hereDiffers == hereDiffers,
        va("%s: the tracking as read afresh (file %d differ%s%s, here %d of %d expected%s%s)", when, c.fileDiffers,
            c.fileDiffers ? ", first " : "", c.firstFile.cStr(), c.hereDiffers, hereDiffers, c.hereDiffers ? ", first " : "",
            c.firstHere.cStr()));
}

void tracked()
{
    expectChanged(0, "tracked");
    expectTracked(va("%s/%s", com_gamedir, CONFIG_NAME), 0, "tracked");
}

// The tracking through writes: two settings changed (a cvar, a stand-in) and counted; a write that fails (its temporary
// file can't be made; the config not replaced after it was written) leaves them counted and the tracking as the file still is; a write
// that succeeds tracks them. Then a name two settings share (a stand-in renamed to the cvar's name for the test, the
// tracking read again): the stand-in is the setting the map kept (the last), in the count and through three writes
// (both changed, the cvar only, neither: the file's value is the last line's, else the first's default). Every value,
// name and the tracking put back after.
void trackEdit()
{
    const za::String path{va("%s/%s", com_gamedir, CONFIG_NAME)};
    if(!configTest.tally.check(configTrackCheck(path.cStr()).on, testName, "trackedit: the game folder's config not tracked"))
    {
        return;
    }
    cvar_t* var = nullptr;
    for(cvar_t* v = firstArchived(); v && !var; v = Cvar_FindVarAfter(v->name, CVAR_ARCHIVE))
    {
        var = plain(v) && !isSlot(v) && isNumber(v->default_string) && !strcmp(v->string, v->default_string)
                      && !strstr(v->name, "_version")
                  ? v
                  : nullptr;
    }
    cvar_t* standIn = nullptr;
    const auto tables = standInTables(); // (held: a range-for over standInTables()[0] would dangle)
    for(cvar_t& v : tables[0])
    {
        standIn = standIn ? standIn : isNumber(v.default_string) && !strcmp(v.string, v.default_string) ? &v : nullptr;
    }
    if(!configTest.tally.check(var && standIn, testName, "trackedit: no settings to change"))
    {
        return;
    }
    Con_Printf("%s: trackedit: %s and %s\n", testName, var->name, standIn->name);
    const auto write = [] { return Host_WriteConfigurationToFile(CONFIG_NAME) != 0; };
    const auto bump = [](cvar_t* v, double by) { Cvar_SetQuick(v, va("%g", Q_atof(v->default_string) + by)); };

    expect(write(), "trackedit: the config written");
    expectChanged(0, "after a write");
    expectTracked(path.cStr(), 0, "after a write");

    bump(var, 1.5);
    bump(standIn, 2.5);
    expectChanged(2, "a cvar and a stand-in changed");

    // (b) Writes that fail: VR_ConfigWritten not called, so the settings are still counted changed.
    const za::String tmp = path + ".tmp";
    files::createDirectories(tmp.cStr()); // (its temporary file: a directory there)
    expect(!write(), "the config's temporary file not made: the write fails");
    files::remove(tmp.cStr());
    expectChanged(2, "after a write that failed to begin");
    expectTracked(path.cStr(), 2, "after a write that failed to begin");
    // (A write that failed after its settings were written to the temporary file, as when it can't replace the config:
    // VR_ConfigNoteWrite called, VR_ConfigWritten not.)
    VR_ConfigNoteWrite();
    expectChanged(2, "after a write that failed to replace the config");
    expectTracked(path.cStr(), 2, "after a write that failed to replace the config");
    expect(write(), "written once it can be");
    expectChanged(0, "after it");
    expectTracked(path.cStr(), 0, "after it");

    // (a) A name two settings have: the stand-in's is the setting (the last of them), as in the map.
    Cvar_SetQuick(var, var->default_string);
    Cvar_SetQuick(standIn, standIn->default_string);
    const char* const standInName = standIn->name;
    standIn->name = var->name;
    configTrackAgain(path.cStr());
    expect(configTrackCheck(path.cStr()).sharedNames == 2, "a name two settings have: tracked as shared");
    bump(var, 1.5);
    expectChanged(0, "the cvar of a shared name changed (the stand-in is the setting)");
    bump(standIn, 2.5);
    expectChanged(1, "the stand-in of a shared name changed");
    expect(write(), "a shared name: both lines written");
    expectChanged(0, "a shared name, both changed, after a write");
    expectTracked(path.cStr(), 0, "a shared name, both changed, after a write");
    Cvar_SetQuick(standIn, standIn->default_string);
    expectChanged(1, "the stand-in of a shared name back to its default");
    expect(write(), "a shared name: the cvar's line only");
    expectTracked(path.cStr(), 0, "a shared name, the cvar's line only, after a write");
    Cvar_SetQuick(var, var->default_string);
    expectChanged(0, "the cvar of a shared name back to its default (the stand-in is the setting)");
    expect(write(), "a shared name: no line");
    expectChanged(0, "a shared name, no line, after a write");
    expectTracked(path.cStr(), 0, "a shared name, no line, after a write");

    standIn->name = standInName;
    configTrackAgain(path.cStr());
    expect(write(), "trackedit: the config written again as it was");
    expectChanged(0, "trackedit: put back");
    expectTracked(path.cStr(), 0, "trackedit: put back");
}

// A menu close's no-change test, `n` times each way: its time and heap allocations on this thread.
void cost(int n)
{
    const auto measure = [n](const char* what, auto&& count) {
        const alloccount::Stats before = alloccount::statsThisThread();
        const za::I64 t0 = za::Clock::nowNanoseconds();
        int changed = 0;
        for(int i = 0; i < n; i++)
        {
            changed += count();
        }
        const za::I64 t1 = za::Clock::nowNanoseconds();
        const alloccount::Stats after = alloccount::statsThisThread();
        za::U64 allocations = 0;
        for(const alloccount::Kind k : {alloccount::Kind::New, alloccount::Kind::Malloc, alloccount::Kind::Calloc,
                alloccount::Kind::Realloc})
        {
            allocations += after.calls[static_cast<int>(k)] - before.calls[static_cast<int>(k)];
        }
        Con_Printf("%s: cost: %s: %.3f ms and %.0f allocations (%.0f KiB asked) a call, %d changed\n", testName, what,
            za::nanosecondsToMilliseconds(t1 - t0) / n, static_cast<double>(allocations) / n,
            static_cast<double>(after.requestedBytes - before.requestedBytes) / 1024.0 / n, changed / n);
    };
    measure("saveConfigNow's walk", [] { return configChangedCount(); });
    measure("the map (until 2026-11-19)", [] { return configChangedCountByMap(); });
}

void test_f()
{
    const char* step = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    const char* arg = Cmd_Argc() > 2 ? Cmd_Argv(2) : "";
    if(!strcmp(step, "snap"))
    {
        snap();
    }
    else if(!strcmp(step, "hasline") && Cmd_Argc() >= 5)
    {
        hasLine();
    }
    else if(!strcmp(step, "checkfile") && *arg)
    {
        checkFile(arg);
    }
    else if(!strcmp(step, "scramble"))
    {
        scramble();
    }
    else if(!strcmp(step, "compare"))
    {
        compare();
    }
    else if(!strcmp(step, "newdefault"))
    {
        newDefault();
    }
    else if(!strcmp(step, "checkdefault"))
    {
        checkDefault();
    }
    else if(!strcmp(step, "othercopy"))
    {
        otherCopy();
    }
    else if(!strcmp(step, "checkmerge"))
    {
        checkMerge();
    }
    else if(!strcmp(step, "dumpfull") && *arg)
    {
        dumpFull(arg);
    }
    else if(!strcmp(step, "tracked"))
    {
        tracked();
    }
    else if(!strcmp(step, "trackedit"))
    {
        trackEdit();
    }
    else if(!strcmp(step, "cost"))
    {
        cost(*arg ? za::max(Q_atoi(arg), 1) : 20);
    }
    else if(!strcmp(step, "done"))
    {
        configTest.tally.verdict(testName);
        configTest.tally.reset();
    }
    else
    {
        Con_Printf("vr_config_test snap | checkfile <name.cfg> | scramble | compare | newdefault | checkdefault | "
                   "othercopy | checkmerge | dumpfull <name> | hasline <file> <word>... <value> | tracked | trackedit | "
                   "cost [n] | done\n");
    }
}

} // namespace

void registerConfigDiag()
{
    Cmd_AddCommand("vr_config_test", test_f);
}

} // namespace qvr
