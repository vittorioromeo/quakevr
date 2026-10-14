// vr_testbatch.cpp -- test runs that play many cases in one game, and waits that end on a condition.
//
// A headless test case used to be a game of its own (kit run.sh: the engine's start, the first map's load, the case,
// quit): seconds of start-up for every case. Misc/quakevr/testbatch.sh plays a suite's cases one after another in one
// game (and the cases split between parallel copies), with these between them (docs/vr-port/TESTING.md, "Batched test
// cases"):
//   vr_test_snapshot   every cvar's value now (the batch's start: the config, the autoexec's settings)
//   vr_test_reset      back to that start for the next case: the cvars a case set put back (a cvar made since: its
//                      default), the +commands let go (+grabmain, +attack: their - command), the mock headset's poses,
//                      buttons, sticks and scripted moves as at start-up (resetMockState), the client's rand() seeded as
//                      at start-up. The case's own "map" (or disconnect) does the rest: the server, the entities, the
//                      server's random stream (sv_random_seed), the player's items.
// And waits that end as soon as what they wait for has happened, instead of a fixed count of frames long enough for
// the slowest run (each wait counts frames as "wait" does: a server tick each):
//   vr_waituntil <cvar> <op> <value> <frames>   until the cvar compares so (== != < <= > >=; numbers as numbers, else
//                                               as text: == and != only), at most <frames>
//   vr_waitprint <frames>[x<n>] <text...>       until the console prints a line holding the text (after the command;
//                                               x<n>: n such lines), at most <frames>
// A wait that runs out prints "vr_waituntil: timed out ..." / "vr_waitprint: timed out ..." (a test can fail on it) and
// goes on with the script. The rest of the command line after a wait runs once it ends, in that frame.

#include "vr_testbatch.hpp"
#include "vr_backend.hpp"
#include "vr_engine.hpp"

#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <stdlib.h>
#include <string.h>

extern "C"
{
extern qboolean cmd_wait;              // cmd.c: "wait" ran (the rest of the buffer waits for the next tick)
extern cmd_function_t* cmd_functions; // cmd.c: every command
}

namespace qvr::testbatch
{
namespace
{

struct SavedCvar
{
    cvar_t* var;
    za::String value;
};

// vr_test_snapshot's cvars (vr_test_reset puts them back), and the cvars' set for a lookup.
struct Snapshot
{
    bool taken{false};
    za::Vector<SavedCvar> cvars;
    ankerl::unordered_dense::set<const cvar_t*> known;
};
Snapshot snapshot;

void snapshot_f()
{
    snapshot.cvars.clear();
    snapshot.known.clear();
    for(cvar_t* var = Cvar_FindVarAfter("", 0); var; var = var->next)
    {
        snapshot.cvars.pushBack(SavedCvar{var, za::String{var->string}});
        snapshot.known.insert(var);
    }
    snapshot.taken = true;
}

[[nodiscard]] bool settable(const cvar_t* var)
{
    return !(var->flags & (CVAR_ROM | CVAR_LOCKED));
}

void restoreCvars()
{
    // The cvars made since the snapshot first (to their defaults: as a game that never made them), gathered before any
    // is set (a callback can make another and sort the list again).
    za::Vector<cvar_t*> made;
    for(cvar_t* var = Cvar_FindVarAfter("", 0); var; var = var->next)
    {
        if(!snapshot.known.contains(var))
        {
            made.pushBack(var);
        }
    }
    for(cvar_t* var : made)
    {
        if(settable(var) && var->default_string && strcmp(var->string, var->default_string) != 0)
        {
            Cvar_SetQuick(var, var->default_string);
        }
    }
    for(const SavedCvar& s : snapshot.cvars)
    {
        if(settable(s.var) && strcmp(s.var->string, s.value.cStr()) != 0)
        {
            Cvar_SetQuick(s.var, s.value.cStr());
        }
    }
}

void releaseButtons()
{
    // Every +command whose -command is the engine's (not a mod's or QC's: those come and go with the game).
    za::Vector<const char*> ups;
    for(const cmd_function_t* c = cmd_functions; c; c = c->next)
    {
        if(c->name[0] == '+' && !c->dynamic && c->srctype == src_command)
        {
            const cmd_function_t* up = Cmd_FindCommand(va("-%s", c->name + 1));
            if(up && !up->dynamic)
            {
                ups.pushBack(up->name);
            }
        }
    }
    for(const char* name : ups)
    {
        Cmd_ExecuteString(name, src_command);
    }
}

void reset_f()
{
    if(!snapshot.taken)
    {
        Con_Printf("vr_test_reset: no vr_test_snapshot taken: the cvars stay as they are\n");
    }
    else
    {
        restoreCvars();
    }
    releaseButtons();
    resetMockState();
    srand(1); // (the client's effects' rand(): as a new game's, which never seeds it)
}

// The wait in progress (one at a time: the commands after it wait with it).
enum class WaitKind
{
    None,
    Cvar,
    Print
};

struct Wait
{
    WaitKind kind{WaitKind::None};
    int left{0};   // frames it may still wait
    int frames{0}; // its budget
    // vr_waituntil
    za::String cvar, op, value;
    // vr_waitprint
    za::String text;
    za::String line; // the console's line so far (since the last newline)
    bool printed{false};
    bool lineCounted{false}; // (this line already counted)
    int seen{0};             // lines holding the text so far
    int need{1};             // lines wanted (<frames>x<count>)
};
Wait waiting;

[[nodiscard]] bool numeric(const char* s, double& out)
{
    char* end = nullptr;
    out = strtod(s, &end);
    return end != s && *end == 0;
}

// vr_waituntil's condition now; false with a message when it can't be read.
[[nodiscard]] bool cvarHolds(bool& ok)
{
    ok = true;
    const cvar_t* var = Cvar_FindVar(waiting.cvar.cStr());
    if(!var)
    {
        ok = false;
        return false;
    }
    const char* op = waiting.op.cStr();
    double want = 0.0, have = 0.0;
    if(numeric(waiting.value.cStr(), want) && numeric(var->string, have))
    {
        return !strcmp(op, "==") ? have == want : !strcmp(op, "!=") ? have != want : !strcmp(op, "<") ? have < want
             : !strcmp(op, "<=") ? have <= want : !strcmp(op, ">") ? have > want : have >= want;
    }
    if(!strcmp(op, "=="))
    {
        return !strcmp(var->string, waiting.value.cStr());
    }
    if(!strcmp(op, "!="))
    {
        return strcmp(var->string, waiting.value.cStr()) != 0;
    }
    return false; // (an order between texts: never)
}

[[nodiscard]] bool done()
{
    if(waiting.kind == WaitKind::Print)
    {
        return waiting.printed;
    }
    bool ok = true;
    const bool holds = cvarHolds(ok);
    if(!ok)
    {
        Con_Printf("vr_waituntil: no cvar \"%s\": not waiting\n", waiting.cvar.cStr());
        return true;
    }
    return holds;
}

// The wait's frame: over (the script goes on now), or another frame (this command again after the next tick).
void step()
{
    if(done())
    {
        waiting.kind = WaitKind::None;
        return;
    }
    if(waiting.left <= 0)
    {
        if(waiting.kind == WaitKind::Print)
        {
            Con_Printf("vr_waitprint: timed out after %d frames waiting for \"%s\" (%d of %d lines)\n", waiting.frames,
                waiting.text.cStr(), waiting.seen, waiting.need);
        }
        else
        {
            const cvar_t* var = Cvar_FindVar(waiting.cvar.cStr());
            Con_Printf("vr_waituntil: timed out after %d frames (%s %s %s; it is %s)\n", waiting.frames, waiting.cvar.cStr(),
                waiting.op.cStr(), waiting.value.cStr(), var ? var->string : "?");
        }
        waiting.kind = WaitKind::None;
        return;
    }
    waiting.left--;
    Cbuf_InsertText("vr_waitcontinue");
    cmd_wait = true;
}

void waitUntil_f()
{
    static constexpr const char* ops[] = {"==", "!=", "<", "<=", ">", ">="};
    bool opOk = false;
    if(Cmd_Argc() == 5)
    {
        for(const char* o : ops)
        {
            opOk = opOk || !strcmp(Cmd_Argv(2), o);
        }
    }
    if(!opOk)
    {
        Con_Printf("vr_waituntil <cvar> <== | != | < | <= | > | >=> <value> <frames>: waits (as \"wait\" does, a tick a "
                   "frame) until the cvar compares so, at most <frames>\n");
        return;
    }
    waiting = Wait{};
    waiting.kind = WaitKind::Cvar;
    waiting.cvar = za::String{Cmd_Argv(1)};
    waiting.op = za::String{Cmd_Argv(2)};
    waiting.value = za::String{Cmd_Argv(3)};
    waiting.frames = waiting.left = Q_atoi(Cmd_Argv(4));
    step();
}

void waitPrint_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("vr_waitprint <frames>[x<count>] <text...>: waits (as \"wait\" does, a tick a frame) until the console prints a "
                   "line holding the text (x<count>: that many lines), at most <frames>\n");
        return;
    }
    waiting = Wait{};
    waiting.kind = WaitKind::Print;
    waiting.frames = waiting.left = Q_atoi(Cmd_Argv(1));
    const char* times = strchr(Cmd_Argv(1), 'x'); // <frames>x<count>: that many lines holding it
    waiting.need = times ? za::max(1, Q_atoi(times + 1)) : 1;
    // The text: the rest of the line after the frames (its words as typed; quotes around it dropped).
    const char* rest = Cmd_Args();
    while(*rest == ' ')
    {
        rest++;
    }
    while(*rest && *rest != ' ')
    {
        rest++;
    }
    while(*rest == ' ')
    {
        rest++;
    }
    za::String text{rest};
    if(text.size() >= 2 && text[0] == '"' && text[text.size() - 1] == '"')
    {
        text = za::String{text.substrByPosLen(1, text.size() - 2)};
    }
    waiting.text = ZA_MOVE(text);
    step();
}

void waitContinue_f()
{
    if(waiting.kind != WaitKind::None)
    {
        step();
    }
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_test_snapshot", snapshot_f);
    Cmd_AddCommand("vr_test_reset", reset_f);
    Cmd_AddCommand("vr_waituntil", waitUntil_f);
    Cmd_AddCommand("vr_waitprint", waitPrint_f);
    Cmd_AddCommand("vr_waitcontinue", waitContinue_f); // (the waits' next frame)
}

} // namespace qvr::testbatch

extern "C" void VR_TestConsolePrint(const char* text)
{
    using qvr::testbatch::waiting;
    using qvr::testbatch::WaitKind;
    if(waiting.kind != WaitKind::Print || waiting.printed)
    {
        return;
    }
    for(const char* p = text; *p; p++)
    {
        if(*p == '\n')
        {
            waiting.line.clear();
            waiting.lineCounted = false;
            continue;
        }
        if(waiting.lineCounted)
        {
            continue; // (a line counts once)
        }
        waiting.line += *p;
        if(waiting.line.size() >= waiting.text.size() && strstr(waiting.line.cStr(), waiting.text.cStr()))
        {
            waiting.lineCounted = true;
            if(++waiting.seen >= waiting.need)
            {
                waiting.printed = true;
                return;
            }
        }
    }
}
