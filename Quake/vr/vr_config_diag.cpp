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

#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "vr_zancle.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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

[[nodiscard]] cvar_t* nextArchived(const cvar_t* var)
{
    return Cvar_FindVarAfter(var->name, CVAR_ARCHIVE);
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

void test_f()
{
    const char* step = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    const char* arg = Cmd_Argc() > 2 ? Cmd_Argv(2) : "";
    if(!strcmp(step, "snap"))
    {
        snap();
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
    else if(!strcmp(step, "done"))
    {
        configTest.tally.verdict(testName);
        configTest.tally.reset();
    }
    else
    {
        Con_Printf("vr_config_test snap | checkfile <name.cfg> | scramble | compare | newdefault | checkdefault | "
                   "othercopy | checkmerge | dumpfull <name> | done\n");
    }
}

} // namespace

void registerConfigDiag()
{
    Cmd_AddCommand("vr_config_test", test_f);
}

} // namespace qvr
