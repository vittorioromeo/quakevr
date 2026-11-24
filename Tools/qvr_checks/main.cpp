// qvr_checks: the build's source checks in one tool (Tools/qvr_checks/README.md).
//
//   qvr_checks [--tree DIR] [--fteqcc PATH] <check> [its arguments]
//   qvr_checks [--tree DIR] [--fteqcc PATH] all [--qchash HASH] [--times]
//
// `all` runs every check at once (a thread each) and prints each one's result as a block, in a fixed order, as the
// kit's build.sh printed them: a passing check prints nothing (the FGD check prints its one line), a failing one its
// last lines and a "... FAILED" line. Exit 1 when a check fails (the FGD check only warns, as before).
#include "common.hpp"

#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Concurrency/Thread.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace qvrc
{
namespace
{

using CheckFn = int (*)(const Ctx&, const Args&, Out&);

struct Check
{
    const char* name;
    CheckFn fn;
    const char* help;
};

constexpr Check checks[] = {
    {"fgd", fgdgen, "runs Misc/trenchbroom/fgdgen.py (Python: skipped without it) with these arguments"},
    {"statics", checkStatics, "no function-local static/thread_local in Quake/vr that is not constexpr [files...]"},
    {"seat_state", checkSeatState, "no new mutable file-scope state in client-side code (splitscreen) [--accept | --tighten]"},
    {"test_numbers", checkTestNumbers, "the numbered tests' dispatchers against QC/test_numbers.txt [--list]"},
    {"builtins", checkBuiltins, "the QC's `= #0` builtins against the engine's tables"},
    {"toolgun_weapons", checkToolgunWeapons, "every weapon id has a row in the toolgun's Weapons section"},
    {"cvars_qc", genCvarsQc, "write QC/vr_cvars_gen.qc from the QC's cvarh_ uses; --check"},
    {"canonical", checkCanonical, "no new copies of the canonical helpers [--list FAMILY | --tighten]"},
    {"release_qc", checkReleaseQc, "the release flavour's QC (-DQVR_NO_TESTS) compiles clean"},
    {"qc_precedence", checkQcPrecedence, "no QC expression that reads differently under C priorities"},
    {"progs_format", checkProgsFormat, "progs.dat is the 32-bit format [progs.dat]"},
    {"std_uses", checkStdUses, "Zancle, not std:: (docs/vr-port/ZANCLE.md): no std:: use or std header past the allowlist [--list]"},
};

// How `all` reports a failure: how many of the check's last lines, the line after them, and whether it fails the build.
struct AllEntry
{
    const char* name;
    int tail;           // 0: the whole output, always printed
    const char* failed; // the line after a failure
    bool fatal;
};

constexpr AllEntry allEntries[] = {
    {"fgd", 0, "FGD CHECK FAILED (see docs/vr-port/MAPPING.md, 'Adding an entity')", false},
    {"statics", 12, "STATICS CHECK FAILED (see docs/vr-port/CODE_STYLE.md, 'Scratch buffers and caches')", true},
    {"seat_state", 12, "SEAT STATE CHECK FAILED (see docs/vr-port/SPLITSCREEN.md, 'The seat_state check')", true},
    {"test_numbers", 12, "TEST NUMBERS CHECK FAILED (see docs/vr-port/TESTING.md, 'Test numbers')", true},
    {"builtins", 12, "BUILTINS CHECK FAILED (see docs/vr-port/CODE_STYLE.md, 'Canonical helpers')", true},
    {"cvars_qc", 8, "QC CVAR HANDLES STALE: run build/qvr_checks/qvr_checks.exe cvars_qc", true},
    {"canonical", 12, "CANONICAL HELPERS CHECK FAILED (see docs/vr-port/CODE_STYLE.md, 'Canonical helpers')", true},
    {"release_qc", 12, "RELEASE QC CHECK FAILED (see docs/vr-port/CODE_STYLE.md, QuakeC)", true},
    {"qc_precedence", 12, "QC PRECEDENCE CHECK FAILED (see docs/vr-port/CODE_STYLE.md, 'QuakeC')", true},
    {"progs_format", 12, "PROGS FORMAT CHECK FAILED (see Tools/fteqcc/README.md)", true},
    {"std_uses", 12, "STD USES CHECK FAILED (see docs/vr-port/ZANCLE.md)", true},
};

const Check* findCheck(za::StringView name)
{
    for(const auto& c : checks)
    {
        if(name == c.name)
        {
            return &c;
        }
    }
    return nullptr;
}

// A check run, an exception from it (a std::regex error, a std::filesystem one, an allocation failure, ...) reported as its failure
// instead of ending the tool with no output (exit 3: build.sh's "BUILD FAILED" with an empty log).
int runCheck(const Check& c, const Ctx& ctx, const Args& args, Out& out)
{
    try
    {
        return c.fn(ctx, args, out);
    }
    catch(const std::exception& e)
    {
        out.line("{}: the check crashed: {}", c.name, e.what());
        return 2;
    }
}

void print(za::StringView s)
{
    fwrite(s.data(), 1, s.size(), stdout);
}

za::String lastLines(za::StringView s, int n)
{
    // (as `echo "$(cmd)" | tail -n`: the trailing newlines dropped, then the last n lines)
    while(!s.empty() && s.back() == '\n')
    {
        s.removeSuffix(1);
    }
    const auto lines = splitLines(s);
    za::String o;
    for(za::SizeT k = lines.size() > static_cast<za::SizeT>(n) ? lines.size() - n : 0; k < lines.size(); ++k)
    {
        o += lines[k];
        o += '\n';
    }
    return o;
}

int runAll(const Ctx& ctx, const Args& args)
{
    za::String qchash;
    bool times = false;
    for(za::SizeT i = 0; i < args.size(); ++i)
    {
        if(args[i] == "--qchash" && i + 1 < args.size())
        {
            qchash = args[++i];
        }
        else if(args[i] == "--times")
        {
            times = true;
        }
    }
    const fs::path stamp = ctx.tree / "scratch" / ".qc_precedence_pass";
    const auto trimmed = [](za::StringView s) {
        while(!s.empty() && isSpace(s.back()))
        {
            s.removeSuffix(1);
        }
        return s;
    };
    // The precedence check compiles the QC twice: skipped when the QC hasn't changed since its last pass.
    const bool skipPrecedence = !qchash.empty() && trimmed(readRaw(stamp)) == qchash;

    constexpr za::SizeT count = za::getArraySize(allEntries);
    struct Result
    {
        Out out;
        int rc = 0;
        double seconds = 0;
        bool ran = false;
    };
    Result results[count];
    za::Vector<za::Thread> threads;
    for(za::SizeT i = 0; i < count; ++i)
    {
        if(skipPrecedence && za::StringView(allEntries[i].name) == "qc_precedence")
        {
            continue;
        }
        threads.emplaceBack([&, i] {
            const za::I64 t0 = za::Clock::nowNanoseconds();
            const Check* c = findCheck(allEntries[i].name);
            Args a;
            if(za::StringView(c->name) == "fgd" || za::StringView(c->name) == "cvars_qc")
            {
                a.emplaceBack("--check");
            }
            results[i].rc = runCheck(*c, ctx, a, results[i].out);
            results[i].ran = true;
            results[i].seconds = za::nanosecondsToSeconds(za::Clock::nowNanoseconds() - t0);
        });
    }
    for(auto& t : threads)
    {
        t.join();
    }
    int rc = 0;
    za::String timing;
    for(za::SizeT i = 0; i < count; ++i)
    {
        const auto& e = allEntries[i];
        const auto& r = results[i];
        if(!r.ran)
        {
            continue;
        }
        appendFmt(timing, " {} {:.2f}", e.name, r.seconds);
        if(e.tail == 0)
        {
            print(r.out.text);
        }
        if(r.rc != 0)
        {
            if(e.tail != 0)
            {
                print(lastLines(r.out.text, e.tail));
            }
            print(e.failed);
            print("\n");
            if(e.fatal)
            {
                rc = 1;
            }
        }
        else if(za::StringView(e.name) == "qc_precedence" && !qchash.empty())
        {
            std::error_code ec;
            fs::create_directories(stamp.parent_path(), ec);
            writeRaw(stamp, qchash + "\n");
        }
    }
    if(times)
    {
        fprintf(stderr, "qvr_checks: seconds:%s\n", timing.cStr());
    }
    fflush(stdout);
    return rc;
}

int usage()
{
    za::String s = "usage: qvr_checks [--tree DIR] [--fteqcc PATH] <check> [arguments]\n"
                   "  all [--qchash HASH] [--times]   every check at once (the kit's build.sh)\n";
    for(const auto& c : checks)
    {
        appendFmt(s, "  {:<14} {}\n", c.name, c.help);
    }
    print(s);
    return 2;
}

} // namespace
} // namespace qvrc

int main(int argc, char** argv)
{
    using namespace qvrc;
    // No error dialogs: a crash or a failed DLL load ends the process quietly (the builds run unattended).
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    Ctx ctx;
    ctx.tree = fs::current_path();
    za::String cmd;
    Args args;
    for(int i = 1; i < argc; ++i)
    {
        const za::StringView a = argv[i];
        if(a == "--tree" && i + 1 < argc)
        {
            ctx.tree = argv[++i];
        }
        else if(a == "--fteqcc" && i + 1 < argc)
        {
            ctx.fteqcc = argv[++i];
        }
        else if(cmd.empty() && !a.startsWith('-'))
        {
            cmd = a;
        }
        else
        {
            args.emplaceBack(a);
        }
    }
    std::error_code ec;
    ctx.tree = fs::absolute(ctx.tree, ec).lexically_normal();
    if(cmd.empty() || cmd == "help")
    {
        return usage();
    }
    if(cmd == "all")
    {
        return runAll(ctx, args);
    }
    const Check* c = findCheck(cmd);
    if(!c)
    {
        return usage();
    }
    Out out;
    const int rc = runCheck(*c, ctx, args, out);
    print(out.text);
    fflush(stdout);
    return rc;
}
