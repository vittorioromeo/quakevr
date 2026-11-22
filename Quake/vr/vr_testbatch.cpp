// vr_testbatch.cpp -- test runs that play many cases in one game, and waits that end on a condition.
//
// A headless test case used to be a game of its own (kit run.sh: the engine's start, the first map's load, the case,
// quit): seconds of start-up for every case. Misc/quakevr/testbatch.sh plays a suite's cases one after another in one
// game (and the cases split between parallel copies), with these between them (docs/vr-port/TESTING.md, "Batched test
// cases"):
//   vr_test_snapshot   every cvar's value now (the batch's start: the config, the autoexec's settings), and every
//                      data table's (weapons.txt's, props.txt's, ...: their stand-ins, set by name as cvars)
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
// A case's run.sh -QuitOn in a batch (testbatch.py writes these):
//   vr_test_quiton "<regex>"   from now on (until vr_test_reset) watches the console's lines for the pattern
//   vr_waitquiton <ticks>      the case's closing wait: the commands after it held aside (the game's own, QC's
//                              localcmd, run meanwhile, as after a lone run's script) until a line since
//                              vr_test_quiton matched, at most <ticks> server ticks (running out is silent: the run's
//                              own waitN ran out)

#include "vr_testbatch.hpp"
#include "vr_backend.hpp"
#include "vr_config.hpp"
#include "vr_cvars.hpp"
#include "vr_cvars_util.hpp"
#include "vr_engine.hpp"

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strstr.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/Span.hpp"
#include "vr_api.h"

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

[[nodiscard]] bool settable(const cvar_t* var)
{
    return !(var->flags & (CVAR_ROM | CVAR_LOCKED));
}

} // namespace

void CvarSnapshot::clear()
{
    saved_.clear();
    known_.clear();
    all_ = false;
}

void CvarSnapshot::take(cvar_t* var)
{
    if(var && known_.insert(var).second)
    {
        saved_.pushBack(Saved{var, za::String{var->string}});
    }
}

void CvarSnapshot::takeAll()
{
    clear();
    cvars::forEach([this](cvar_t* var) { take(var); });
    // The data tables' values too (no cvars, but set as ones: a case's "vr_reload_port_shot_radius 0.1" or
    // "vr_wofs_<key>_NN 2" sets a weapon table's stand-in, Cvar_SetQuick puts it back), or a case's setting stays for
    // the cases after it in the same game.
    for(const za::Span<cvar_t> table : standInTables())
    {
        for(cvar_t& var : table)
        {
            take(&var);
        }
    }
    all_ = true;
}

void CvarSnapshot::restore() const
{
    if(all_)
    {
        // The cvars made since the snapshot first (to their defaults: as a game that never made them), gathered before
        // any is set (a callback can make another and sort the list again).
        za::Vector<cvar_t*> made;
        cvars::forEach([this, &made](cvar_t* var) {
            if(!known_.contains(var))
            {
                made.pushBack(var);
            }
        });
        for(cvar_t* var : made)
        {
            if(settable(var) && var->default_string && ZA_STRCMP(var->string, var->default_string) != 0)
            {
                Cvar_SetQuick(var, var->default_string);
            }
        }
    }
    for(const Saved& s : saved_)
    {
        if(settable(s.var) && ZA_STRCMP(s.var->string, s.value.cStr()) != 0)
        {
            Cvar_SetQuick(s.var, s.value.cStr());
        }
    }
}

namespace
{

// vr_test_snapshot's cvars (vr_test_reset puts them back).
struct Snapshot
{
    bool taken{false};
    CvarSnapshot cvars;
};
Snapshot snapshot;

void snapshot_f()
{
    snapshot.cvars.takeAll();
    snapshot.taken = true;
}

void restoreCvars()
{
    snapshot.cvars.restore();
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

// --- A case's -QuitOn (run.sh's: the run ends at the first console line matching a regex) in a batch: vr_test_quiton
// <regex> arms it at the case's start, vr_waitquiton <frames> (the case's closing wait) ends as soon as a line since then
// matched. The patterns are run.ps1's (.NET's, case-insensitive); this matches the part the tests use: literals, . ^ $,
// [..] classes (ranges, ^, \d \s \w), \d \s \w \D \S \W, escaped literals, * + ? (a lazy ? after them is ignored: the
// same lines match), groups (?:) and | alternation. A pattern it can't read matches as plain text.
struct ReInst
{
    enum Op : unsigned char
    {
        Char,
        Any,
        Class,
        Split, // try x, then y
        Jmp,
        Bol,
        Eol,
        Match
    };
    Op op{Match};
    unsigned char c{0};
    int x{0}, y{0}; // jump targets; Class: x is the class's index
};

struct ReClass
{
    bool in[256]{};
};

struct ReNode
{
    enum Kind : unsigned char
    {
        Lit,
        Dot,
        Cls,
        Bol,
        Eol,
        Empty,
        Cat,
        Alt,
        Star,
        Plus,
        Quest
    };
    Kind kind{Empty};
    unsigned char c{0};
    int a{-1}, b{-1}; // children (Cat, Alt: both; repeats: a); Cls: a is the class's index
};

struct Regex
{
    za::Vector<ReInst> prog;
    za::Vector<ReClass> classes;
    za::Vector<ReNode> nodes;
    za::String text; // (the pattern; matched as plain text when it could not be read)
    bool plain{true};
};

[[nodiscard]] unsigned char reLower(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c - 'A' + 'a') : c;
}

struct ReParser
{
    Regex& re;
    const char* p;
    bool bad{false};

    int node(ReNode::Kind k, int a = -1, int b = -1, unsigned char c = 0)
    {
        ReNode n;
        n.kind = k;
        n.a = a;
        n.b = b;
        n.c = c;
        re.nodes.pushBack(n);
        return static_cast<int>(re.nodes.size()) - 1;
    }

    void classEscape(ReClass& cl, char e)
    {
        const bool neg = e == 'D' || e == 'S' || e == 'W';
        const char k = static_cast<char>(reLower(static_cast<unsigned char>(e)));
        for(int i = 0; i < 256; i++)
        {
            const bool d = i >= '0' && i <= '9';
            const bool s = i == ' ' || i == '\t' || i == '\r' || i == '\n' || i == '\f' || i == '\v';
            const bool w = d || (i >= 'a' && i <= 'z') || (i >= 'A' && i <= 'Z') || i == '_';
            const bool hit = k == 'd' ? d : k == 's' ? s : w;
            cl.in[i] = cl.in[i] || (hit != neg);
        }
    }

    int newClass()
    {
        re.classes.pushBack(ReClass{});
        return static_cast<int>(re.classes.size()) - 1;
    }

    int atom()
    {
        const char c = *p;
        if(c == '(')
        {
            p++;
            if(p[0] == '?' && p[1] == ':')
            {
                p += 2;
            }
            const int inner = alt();
            if(*p != ')')
            {
                bad = true;
                return inner;
            }
            p++;
            return inner;
        }
        p++;
        if(c == '.')
        {
            return node(ReNode::Dot);
        }
        if(c == '^')
        {
            return node(ReNode::Bol);
        }
        if(c == '$')
        {
            return node(ReNode::Eol);
        }
        if(c == '\\')
        {
            const char e = *p;
            if(!e)
            {
                bad = true;
                return node(ReNode::Empty);
            }
            p++;
            if(strchr("dDsSwW", e))
            {
                const int ci = newClass();
                classEscape(re.classes[ci], e);
                return node(ReNode::Cls, ci);
            }
            const char lit = e == 't' ? '\t' : e == 'n' ? '\n' : e == 'r' ? '\r' : e;
            return node(ReNode::Lit, -1, -1, reLower(static_cast<unsigned char>(lit)));
        }
        if(c == '[')
        {
            const int ci = newClass();
            bool neg = false;
            if(*p == '^')
            {
                neg = true;
                p++;
            }
            bool first = true;
            while(*p && (*p != ']' || first))
            {
                first = false;
                unsigned char lo = static_cast<unsigned char>(*p++);
                if(lo == '\\' && *p)
                {
                    const char e = *p++;
                    if(strchr("dDsSwW", e))
                    {
                        classEscape(re.classes[ci], e);
                        continue;
                    }
                    lo = static_cast<unsigned char>(e == 't' ? '\t' : e == 'n' ? '\n' : e);
                }
                unsigned char hi = lo;
                if(p[0] == '-' && p[1] && p[1] != ']')
                {
                    p++;
                    hi = static_cast<unsigned char>(*p++);
                    if(hi == '\\' && *p)
                    {
                        hi = static_cast<unsigned char>(*p++);
                    }
                }
                for(int i = lo; i <= hi; i++)
                {
                    re.classes[ci].in[i] = true;
                }
            }
            if(*p != ']')
            {
                bad = true;
                return node(ReNode::Empty);
            }
            p++;
            ReClass& cl = re.classes[ci];
            for(int i = 'a'; i <= 'z'; i++) // (case-insensitive: a letter in either case is both)
            {
                const bool either = cl.in[i] || cl.in[i - 'a' + 'A'];
                cl.in[i] = cl.in[i - 'a' + 'A'] = either;
            }
            if(neg)
            {
                for(bool& b : cl.in)
                {
                    b = !b;
                }
            }
            return node(ReNode::Cls, ci);
        }
        if(c == ')' || c == '*' || c == '+' || c == '?' || c == '|')
        {
            bad = true;
            return node(ReNode::Empty);
        }
        return node(ReNode::Lit, -1, -1, reLower(static_cast<unsigned char>(c)));
    }

    int piece()
    {
        int a = atom();
        while(*p == '*' || *p == '+' || *p == '?')
        {
            const char q = *p++;
            a = node(q == '*' ? ReNode::Star : q == '+' ? ReNode::Plus : ReNode::Quest, a);
            if(*p == '?' && q != '?')
            {
                p++; // (lazy: the same lines match)
            }
        }
        return a;
    }

    int seq()
    {
        int s = -1;
        while(*p && *p != '|' && *p != ')' && !bad)
        {
            const int x = piece();
            s = s < 0 ? x : node(ReNode::Cat, s, x);
        }
        return s < 0 ? node(ReNode::Empty) : s;
    }

    int alt()
    {
        int a = seq();
        while(*p == '|' && !bad)
        {
            p++;
            a = node(ReNode::Alt, a, seq());
        }
        return a;
    }
};

int reEmit(Regex& re, ReInst::Op op, int x = 0, int y = 0, unsigned char c = 0)
{
    ReInst i;
    i.op = op;
    i.x = x;
    i.y = y;
    i.c = c;
    re.prog.pushBack(i);
    return static_cast<int>(re.prog.size()) - 1;
}

void reCompile(Regex& re, int n)
{
    const ReNode nd = re.nodes[n]; // (a copy: emitting grows other vectors, not this one, but stay simple)
    const int here = static_cast<int>(re.prog.size());
    switch(nd.kind)
    {
        case ReNode::Lit: reEmit(re, ReInst::Char, 0, 0, nd.c); break;
        case ReNode::Dot: reEmit(re, ReInst::Any); break;
        case ReNode::Cls: reEmit(re, ReInst::Class, nd.a); break;
        case ReNode::Bol: reEmit(re, ReInst::Bol); break;
        case ReNode::Eol: reEmit(re, ReInst::Eol); break;
        case ReNode::Empty: break;
        case ReNode::Cat:
            reCompile(re, nd.a);
            reCompile(re, nd.b);
            break;
        case ReNode::Alt:
        {
            const int split = reEmit(re, ReInst::Split);
            re.prog[split].x = split + 1;
            reCompile(re, nd.a);
            const int jmp = reEmit(re, ReInst::Jmp);
            re.prog[split].y = static_cast<int>(re.prog.size());
            reCompile(re, nd.b);
            re.prog[jmp].x = static_cast<int>(re.prog.size());
            break;
        }
        case ReNode::Star:
        {
            const int split = reEmit(re, ReInst::Split);
            re.prog[split].x = split + 1;
            reCompile(re, nd.a);
            reEmit(re, ReInst::Jmp, split);
            re.prog[split].y = static_cast<int>(re.prog.size());
            break;
        }
        case ReNode::Plus:
        {
            reCompile(re, nd.a);
            const int split = reEmit(re, ReInst::Split, here);
            re.prog[split].y = split + 1;
            break;
        }
        case ReNode::Quest:
        {
            const int split = reEmit(re, ReInst::Split);
            re.prog[split].x = split + 1;
            reCompile(re, nd.a);
            re.prog[split].y = static_cast<int>(re.prog.size());
            break;
        }
    }
}

void reBuild(Regex& re, const char* pattern)
{
    re = Regex{};
    re.text = za::String{pattern};
    ReParser parser{re, pattern};
    const int root = parser.alt();
    if(parser.bad || *parser.p)
    {
        return; // (plain text)
    }
    reCompile(re, root);
    reEmit(re, ReInst::Match);
    re.nodes.clear();
    re.plain = false;
}

// Backtracking (the patterns and lines are short); `budget` stops a pathological pattern.
[[nodiscard]] bool reRun(const Regex& re, int pc, const unsigned char* s, int sp, int len, int& budget)
{
    while(budget-- > 0)
    {
        const ReInst& in = re.prog[pc];
        switch(in.op)
        {
            case ReInst::Char:
                if(sp >= len || reLower(s[sp]) != in.c)
                {
                    return false;
                }
                sp++;
                pc++;
                break;
            case ReInst::Any:
                if(sp >= len)
                {
                    return false;
                }
                sp++;
                pc++;
                break;
            case ReInst::Class:
                if(sp >= len || !re.classes[in.x].in[s[sp]])
                {
                    return false;
                }
                sp++;
                pc++;
                break;
            case ReInst::Split:
                if(reRun(re, in.x, s, sp, len, budget))
                {
                    return true;
                }
                pc = in.y;
                break;
            case ReInst::Jmp: pc = in.x; break;
            case ReInst::Bol:
                if(sp != 0)
                {
                    return false;
                }
                pc++;
                break;
            case ReInst::Eol:
                if(sp != len)
                {
                    return false;
                }
                pc++;
                break;
            case ReInst::Match: return true;
        }
    }
    return false;
}

[[nodiscard]] bool reSearch(const Regex& re, const char* line)
{
    if(re.plain)
    {
        return strstr(line, re.text.cStr()) != nullptr;
    }
    const auto* s = reinterpret_cast<const unsigned char*>(line);
    int len = static_cast<int>(strlen(line));
    while(len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n'))
    {
        len--;
    }
    int budget = 200000;
    for(int start = 0; start <= len; start++)
    {
        if(reRun(re, 0, s, start, len, budget))
        {
            return true;
        }
    }
    return false;
}

struct QuitOn
{
    bool armed{false};
    bool matched{false};
    Regex re;
    za::String line; // the console's line so far
    // vr_waitquiton
    bool holding{false};
    int left{0};     // ticks it may still hold
    za::String held; // the commands after it
};
QuitOn quitOn;

void quitOnDisarm()
{
    quitOn.armed = quitOn.matched = false;
    quitOn.line.clear();
}

// The rest of the command line after the first <skip> words, quotes around it dropped.
[[nodiscard]] za::String argsAfter(int skip)
{
    const char* rest = Cmd_Args();
    for(int i = 0; i < skip; i++)
    {
        while(*rest == ' ')
        {
            rest++;
        }
        while(*rest && *rest != ' ')
        {
            rest++;
        }
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
    return text;
}

void quitOn_f()
{
    quitOnDisarm();
    if(Cmd_Argc() < 2)
    {
        return; // (no pattern: off)
    }
    const za::String pattern = argsAfter(0);
    reBuild(quitOn.re, pattern.cStr());
    quitOn.armed = true;
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
    quitOnDisarm();
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
        return !ZA_STRCMP(op, "==") ? have == want : !ZA_STRCMP(op, "!=") ? have != want : !ZA_STRCMP(op, "<") ? have < want
             : !ZA_STRCMP(op, "<=") ? have <= want : !ZA_STRCMP(op, ">") ? have > want : have >= want;
    }
    if(!ZA_STRCMP(op, "=="))
    {
        return !ZA_STRCMP(var->string, waiting.value.cStr());
    }
    if(!ZA_STRCMP(op, "!="))
    {
        return ZA_STRCMP(var->string, waiting.value.cStr()) != 0;
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
            opOk = opOk || !ZA_STRCMP(Cmd_Argv(2), o);
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

// vr_waitquiton holds the rest of the batch's commands aside (not a "wait": the game's own commands, QC's localcmd,
// must still run meanwhile, as after the end of a script run alone) and puts them back at the server tick
// (VR_TestTick) after the pattern matched or <ticks> ticks.
void waitQuitOn_f()
{
    if(Cmd_Argc() != 2)
    {
        Con_Printf("vr_waitquiton <ticks>: holds the commands after it until a console line since vr_test_quiton matched "
                   "its pattern, at most <ticks> server ticks (as \"wait\" counts)\n");
        return;
    }
    if(!quitOn.armed || quitOn.matched || quitOn.holding)
    {
        return;
    }
    quitOn.held = za::String{Cbuf_PendingText(), static_cast<za::SizeT>(Cbuf_Pending())};
    Cbuf_DropPending();
    quitOn.holding = true;
    quitOn.left = Q_atoi(Cmd_Argv(1));
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
    Cmd_AddCommand("vr_test_quiton", quitOn_f);
    Cmd_AddCommand("vr_waitquiton", waitQuitOn_f);
}

} // namespace qvr::testbatch

extern "C" void VR_TestConsolePrint(const char* text)
{
    using qvr::testbatch::waiting;
    using qvr::testbatch::WaitKind;
    using qvr::testbatch::quitOn;
    if(quitOn.armed && !quitOn.matched)
    {
        for(const char* p = text; *p && !quitOn.matched; p++)
        {
            if(*p == '\n')
            {
                quitOn.matched = qvr::testbatch::reSearch(quitOn.re, quitOn.line.cStr());
                quitOn.line.clear();
                continue;
            }
            quitOn.line += *p;
        }
    }
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
        if(waiting.line.size() >= waiting.text.size() && ZA_STRSTR(waiting.line.cStr(), waiting.text.cStr()))
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

extern "C" void VR_TestTick(void)
{
    using qvr::testbatch::quitOn;
    if(!quitOn.holding || (!quitOn.matched && quitOn.left-- > 0))
    {
        return;
    }
    quitOn.holding = false;
    Cbuf_InsertText(quitOn.held.cStr()); // (ahead of what the game queued meanwhile: the case goes on to its end)
    quitOn.held.clear();
}

extern "C" int VR_LogFlushFrames(void)
{
    return qvr::vr_log_flush_frames.value != 0.f;
}
