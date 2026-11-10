// release_qc and qc_precedence: the two checks that compile the QC (in work folders under build/).
#include "common.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <thread>

namespace qvrc
{
namespace
{

// A file's bytes and modified time, or nothing without it.
struct Digest
{
    bool exists = false;
    std::string bytes;
    fs::file_time_type mtime;
    bool operator==(const Digest&) const = default;
};

Digest digest(const fs::path& p)
{
    Digest d;
    std::error_code ec;
    if(!fs::is_regular_file(p, ec))
    {
        return d;
    }
    d.exists = true;
    d.bytes = readRaw(p);
    d.mtime = fs::last_write_time(p, ec);
    return d;
}

// fteqcc's diagnostics: `file:line: error: ...` / `warning F123: ...`, and a final `... 3 warnings`-style summary.
// (\b(?:error|warning)\b[ :], case-insensitive)
bool isProblemLine(std::string_view l)
{
    std::string low(l);
    for(char& c : low)
    {
        if(c >= 'A' && c <= 'Z')
        {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    for(std::string_view w : {"error", "warning"})
    {
        for(size_t i = 0; (i = low.find(w, i)) != std::string::npos; ++i)
        {
            const size_t e = i + w.size();
            if(wordStart(low, i) && e < low.size() && (low[e] == ' ' || low[e] == ':'))
            {
                return true;
            }
        }
    }
    return false;
}

} // namespace

// release_qc: fails unless the release flavour's QC compiles clean: no error, no warning, the 32-bit progs format.
//
// A release's progs.dat leaves the tests out (fteqcc -DQVR_NO_TESTS: Windows/package-quakevr.ps1, QC/vr_macros.qc), a
// flavour no development build compiles, so a test file without its guard or a game file calling into a test file
// unguarded only shows on release day. This compiles it on every build; and every QC/*_test.qc must be wholly inside
// the `#ifdef QVR_TESTS` guard (one with no test helper in it would compile either way, and ship).
//
// The compiler: --fteqcc, else $FTEQCC, else Tools/fteqcc/fteqcc64.exe, else fteqcc64/fteqcc on PATH (none: skipped).
// Prints one line: the globals, statements and functions of the release progs. Its work files go to build/qcrelease/
// (git-ignored), overwritten each run; QC/ and quakevr/progs.dat are not touched (checked).
int checkReleaseQc(const Ctx& ctx, const Args&, Out& out)
{
    const fs::path fteqcc = findFteqcc(ctx, {ctx.tree / "Tools/fteqcc/fteqcc64.exe"});
    if(fteqcc.empty())
    {
        out.line("check_release_qc: no fteqcc found; skipped");
        return 0;
    }
    const fs::path work = ctx.tree / "build" / "qcrelease";
    const fs::path devProgs = ctx.tree / "quakevr" / "progs.dat";
    const std::string guard = "#ifdef QVR_TESTS";
    bool fail = false;

    // Every QC/**/*_test.qc whose code, comments aside, isn't wholly inside the QVR_TESTS guard.
    std::vector<fs::path> tests;
    for(auto& p : listFiles(ctx.tree / "QC", {".qc"}, true))
    {
        if(endsWith(p.filename().string(), "_test.qc"))
        {
            tests.push_back(std::move(p));
        }
    }
    sortPaths(tests);
    std::vector<std::string> bad;
    for(const auto& path : tests)
    {
        std::vector<std::string_view> code;
        const std::string text = readText(path);
        for(auto l : splitLines(text))
        {
            l = trim(l);
            if(!l.empty() && !startsWith(l, "//"))
            {
                code.push_back(l);
            }
        }
        if(code.empty() || code.front() != guard || !startsWith(code.back(), "#endif"))
        {
            bad.push_back(relPosix(path, ctx.tree));
        }
    }
    if(!bad.empty())
    {
        fail = true;
        out.line("check_release_qc: test files not wholly inside the guard (comments aside, first line `{}`, last "
                 "`#endif`):",
                 guard);
        for(const auto& b : bad)
        {
            out.line("  {}", b);
        }
    }
    const Digest before = digest(devProgs);
    const fs::path outProgs = work / "progs.dat";
    std::error_code ec;
    if(fs::is_regular_file(outProgs, ec))
    {
        writeRaw(outProgs, ""); // (a failed compile must not pass on the last run's output)
    }
    ProcResult r;
    if(writeWorkProgsSrc(ctx, work))
    {
        std::vector<std::string> cmd = {fteqcc.string(), "-DQVR_NO_TESTS"};
        cmd.insert(cmd.end(), qcFlags.begin(), qcFlags.end());
        cmd.insert(cmd.end(), {"-srcfile", "progs.src"});
        r = runProcess(cmd, work, true);
    }
    std::string text;
    for(char c : r.out)
    {
        if(c != '\r')
        {
            text += c;
        }
    }
    std::vector<std::string_view> problems;
    for(auto l : splitLines(text))
    {
        if(isProblemLine(l))
        {
            problems.push_back(l);
        }
    }
    if(r.rc != 0 || !problems.empty())
    {
        fail = true;
        out.line("check_release_qc: the release QC (-DQVR_NO_TESTS) doesn't compile clean: exit {}, {} lines of "
                 "errors and warnings (a game file calling into a test file wraps the call in #ifdef QVR_TESTS: "
                 "QC/vr_macros.qc):",
                 r.rc, problems.size());
        for(size_t i = 0; i < problems.size() && i < 30; ++i)
        {
            out.line("  {}", problems[i]);
        }
        if(r.rc != 0 && problems.empty())
        {
            const auto ls = splitLines(text);
            std::string tail;
            for(size_t i = ls.size() > 10 ? ls.size() - 10 : 0; i < ls.size(); ++i)
            {
                tail += (tail.empty() ? "" : "\n") + std::string(ls[i]);
            }
            out.raw(tail);
        }
    }
    if(!(digest(devProgs) == before))
    {
        fail = true;
        out.line("check_release_qc: quakevr/progs.dat changed: this check must not touch it");
    }
    if(r.rc == 0)
    {
        const std::string data = readRaw(outProgs);
        ProgsHeader hdr;
        bool isProgs = false;
        const std::string fmt = progsFormat(data, hdr, isProgs);
        if(!isProgs || fmt != "fte32")
        {
            fail = true;
            out.line("check_release_qc: the release progs isn't the 32-bit format: {}", fmt);
        }
        else
        {
            out.line("check_release_qc: {}: {} globals, {} statements, {} functions, {} bytes", fail ? "FAILED" : "ok",
                     hdr.h[13], hdr.h[3], hdr.h[9], data.size());
        }
    }
    return fail ? 1 : 0;
}

namespace
{

// The id opcodes (all the `standard` target emits; FTEQCC's own opcodes are compared by number like the rest).
constexpr uint32_t opIf = 49;
constexpr uint32_t opIfNot = 50;
constexpr uint32_t opGoto = 61;
constexpr uint32_t reservedOfs = 28; // OFS_NULL, OFS_RETURN, OFS_PARM0..7: fixed

// A jump's operand (IF / IFNOT: b, GOTO: a) is a statement count, not a global.
bool jumpOperand(uint32_t op, int which)
{
    return ((op == opIf || op == opIfNot) && which == 1) || (op == opGoto && which == 0);
}

// A vector operand (3 globals): MUL_V, ADD_V, SUB_V, EQ_V, NE_V (a, b); MUL_VF, STORE_V, STOREP_V, NOT_V (a); MUL_FV (b).
bool vectorOperand(uint32_t op, int which)
{
    switch(op)
    {
        case 2:
        case 7:
        case 9:
        case 11:
        case 16: return which <= 1;
        case 4:
        case 32:
        case 38:
        case 45: return which == 0;
        case 3: return which == 1;
        default: return false;
    }
}

// A string operand: EQ_S, NE_S (a, b); STORE_S, STOREP_S, NOT_S (a).
bool stringOperand(uint32_t op, int which)
{
    switch(op)
    {
        case 12:
        case 17: return which <= 1;
        case 33:
        case 39:
        case 46: return which == 0;
        default: return false;
    }
}

// One function's statements, each as text that is the same in both compiles unless the code differs, and their source
// lines.
struct Func
{
    std::string file; // relative to the repository's root
    std::vector<std::string> text;
    std::vector<int> lines;
};
using Bodies = std::map<std::string, Func>;

uint32_t u32At(std::string_view d, size_t ofs)
{
    uint32_t v = 0;
    std::memcpy(&v, d.data() + ofs, 4);
    return v;
}

// dir/progs.dat (FTE's 32-bit format) and dir/progs.lno (fteqcc's statement lines) -> {function: statements}; an error
// message, or empty.
//
// The two compiles number globals, immediates and temporaries independently (a function that differs can shift those of
// every later one), so a statement's operands are written by what they are, not where: a fixed global (return, parms)
// by its offset, a local or local temporary by its offset in the function's locals, a named global by its name, any
// other global (an immediate; a shared temporary, always 0) by its value: a string's text, a vector's three words.
std::string readProgs(const fs::path& dir, const fs::path& tree, Bodies& funcs)
{
    const std::string d = readRaw(dir / "progs.dat");
    const std::string lno = readRaw(dir / "progs.lno");
    ProgsHeader hdr;
    bool isProgs = false;
    const std::string fmt = progsFormat(d, hdr, isProgs);
    if(!isProgs || fmt != "fte32")
    {
        return "progs.dat isn't the 32-bit format: " + fmt;
    }
    const auto u = [&](int i) { return static_cast<size_t>(static_cast<uint32_t>(hdr.h[i])); };
    const size_t ofsSt = u(2), numSt = u(3), ofsGd = u(4), numGd = u(5), ofsFn = u(8), numFn = u(9), ofsStr = u(10),
                 numStr = u(11), ofsGl = u(12), numGl = u(13);
    for(const auto [ofs, size] : {std::pair{ofsSt, numSt * 16}, {ofsGd, numGd * 12}, {ofsFn, numFn * 36},
                                  {ofsStr, numStr}, {ofsGl, numGl * 4}})
    {
        if(ofs > d.size() || size > d.size() - ofs)
        {
            return "progs.dat is truncated";
        }
    }
    // progs.lno: "LNOF", version 1, numglobaldefs, numglobals, numfielddefs, numstatements, then a line per statement.
    if(lno.size() < 24 + numSt * 4 || lno.compare(0, 4, "LNOF") != 0 || u32At(lno, 4) != 1 || u32At(lno, 20) != numSt)
    {
        return "progs.lno is missing or doesn't match progs.dat";
    }
    const auto str = [&](uint32_t o) -> std::string_view {
        if(o >= numStr)
        {
            return {};
        }
        const char* p = d.data() + ofsStr + o;
        return {p, strnlen(p, numStr - o)};
    };
    const auto global = [&](uint32_t v) { return u32At(d, ofsGl + size_t{v} * 4); };
    // A global's name (several defs can share one: `origin`, `origin_x`; the least).
    std::vector<std::string_view> names(numGl);
    for(size_t i = 0; i < numGd; ++i)
    {
        const uint32_t ofs = u32At(d, ofsGd + i * 12 + 4);
        const std::string_view n = str(u32At(d, ofsGd + i * 12 + 8));
        if(ofs < numGl && !n.empty() && (names[ofs].empty() || n < names[ofs]))
        {
            names[ofs] = n;
        }
    }
    // The functions with code, by their first statement: each runs to the next one's.
    std::vector<std::pair<uint32_t, size_t>> order;
    for(size_t i = 0; i < numFn; ++i)
    {
        const auto first = static_cast<int32_t>(u32At(d, ofsFn + i * 36));
        if(first > 0 && static_cast<size_t>(first) < numSt)
        {
            order.emplace_back(static_cast<uint32_t>(first), i);
        }
    }
    std::sort(order.begin(), order.end());
    for(size_t f = 0; f < order.size(); ++f)
    {
        const size_t fn = ofsFn + order[f].second * 36;
        const uint32_t first = order[f].first;
        const uint32_t end = f + 1 < order.size() ? order[f + 1].first : static_cast<uint32_t>(numSt);
        const uint32_t parmStart = u32At(d, fn + 4);
        const uint32_t locals = u32At(d, fn + 8);
        std::string name(str(u32At(d, fn + 16)));
        for(int k = 2; funcs.contains(name); ++k)
        {
            name = std::format("{}#{}", str(u32At(d, fn + 16)), k);
        }
        Func& out = funcs[name];
        const std::string_view file = str(u32At(d, fn + 20));
        out.file = file.empty() ? "?" : relPosix((dir / fs::path(file)).lexically_normal(), tree);
        out.text.reserve(end - first);
        out.lines.reserve(end - first);
        for(uint32_t s = first; s < end; ++s)
        {
            const size_t st = ofsSt + size_t{s} * 16;
            const uint32_t op = u32At(d, st);
            std::string t = std::to_string(op);
            for(int w = 0; w < 3; ++w)
            {
                const uint32_t v = u32At(d, st + 4 + static_cast<size_t>(w) * 4);
                t += ' ';
                if(jumpOperand(op, w))
                {
                    t += std::to_string(static_cast<int32_t>(v));
                }
                else if(v < reservedOfs)
                {
                    t += std::format("r{}", v);
                }
                else if(v >= parmStart && v - parmStart < locals)
                {
                    t += std::format("l{}", v - parmStart);
                }
                else if(v >= numGl)
                {
                    t += std::format("?{}", v);
                }
                else if(!names[v].empty())
                {
                    t += names[v];
                }
                else if(stringOperand(op, w))
                {
                    t += std::format("\"{}\"", str(global(v)));
                }
                else if(vectorOperand(op, w) && v + 2 < numGl)
                {
                    t += std::format("={:x},{:x},{:x}", global(v), global(v + 1), global(v + 2));
                }
                else
                {
                    t += std::format("={:x}", global(v));
                }
            }
            out.text.push_back(std::move(t));
            out.lines.push_back(static_cast<int>(u32At(lno, 24 + size_t{s} * 4)));
        }
    }
    return {};
}

struct CompileResult
{
    Bodies funcs;
    std::string error; // empty: compiled
};

// Compiles QC/progs.src into work/sub (progs.dat and its progs.lno) and reads them, then empties them.
CompileResult compileProgs(const Ctx& ctx, const fs::path& fteqcc, const fs::path& work, const std::string& sub,
                           const std::vector<std::string>& extra)
{
    CompileResult res;
    const fs::path out = work / sub;
    if(!writeWorkProgsSrc(ctx, out))
    {
        res.error = "cannot write " + (out / "progs.src").generic_string();
        return res;
    }
    const fs::path files[] = {out / "progs.dat", out / "progs.lno"};
    std::error_code ec;
    for(const auto& f : files)
    {
        if(fs::is_regular_file(f, ec))
        {
            writeRaw(f, ""); // (a failed compile must not pass on the last run's output)
        }
    }
    std::vector<std::string> cmd = {fteqcc.string()};
    cmd.insert(cmd.end(), qcFlags.begin(), qcFlags.end());
    // (after -O3, which strips the file names: with them fteqcc writes progs.lno, each statement's source line)
    cmd.emplace_back("-Ono-filenames");
    cmd.insert(cmd.end(), extra.begin(), extra.end());
    cmd.insert(cmd.end(), {"-srcfile", "progs.src"});
    const ProcResult r = runProcess(cmd, out, false);
    if(r.rc != 0 || r.out.find("ERROR") != std::string::npos)
    {
        const auto ls = splitLines(r.out);
        std::string tail;
        for(size_t i = ls.size() > 10 ? ls.size() - 10 : 0; i < ls.size(); ++i)
        {
            std::string l(ls[i]);
            std::erase(l, '\r');
            tail += (tail.empty() ? "" : "\n") + l;
        }
        res.error = std::format("check_qc_precedence: the compile failed ({}):\n{}", sub, tail);
    }
    else if(const std::string e = readProgs(out, ctx.tree, res.funcs); !e.empty())
    {
        res.error = std::format("check_qc_precedence: {}/{}", sub, e);
    }
    // The progs are only this check's: emptied, so the work folder stays small.
    for(const auto& f : files)
    {
        if(fs::is_regular_file(f, ec))
        {
            writeRaw(f, "");
        }
    }
    return res;
}

} // namespace

// qc_precedence: fails when a QC expression means something else to fteqcc than to a C reader
// (docs/vr-port/CODE_STYLE.md, QuakeC).
//
// fteqcc's default ("QC") operator priorities are not C's. With our flags (QC/build.sh), fteqcc 64:
//   - `x = a && b` is `(x = a) && b`: an assignment statement (`=`, `+=`, a field's `self.f = ...`) takes only the
//     first operand of `&&` / `||`; the rest is computed and thrown away. (A declaration's initializer,
//     `float x = a && b;`, a `return` and a call's argument are whole expressions: fine.)
//   - `a && b ? c : d` is `a && (b ? c : d)`: `?:` binds tighter than `&&` / `||`, everywhere.
//   - `a || b && c` is `(a || b) && c`: `&&` and `||` have the same priority, left to right.
//   - `!a == b` is `!(a == b)` (fteqcc warns, F304); `a & b == c` is `(a & b) == c`.
// Comparisons against arithmetic (`time - last < 0.2`) are as in C.
//
// The check compiles the progs twice at once, with QC priorities (the build's) and with C priorities (-Fcpriority),
// and compares each function's statements (readProgs: operands by what they are, so the two compiles' numbering of
// globals and immediates doesn't count). A function that differs holds an expression that depends on the priorities:
// parenthesise it (or use a temporary) so it reads the same both ways. Its first differing statement gives the line.
//
// The compiler: --fteqcc, else $FTEQCC, else C:/OHWorkspace/quakevr/QC/fteqcc64.exe, else fteqcc64/fteqcc on PATH.
// Work files go to build/qcprec/ (git-ignored); the progs are emptied after use. QC/ and quakevr/progs.dat are not
// touched.
int checkQcPrecedence(const Ctx& ctx, const Args&, Out& out)
{
    const fs::path fteqcc = findFteqcc(ctx, {"C:/OHWorkspace/quakevr/QC/fteqcc64.exe"});
    if(fteqcc.empty())
    {
        out.line("check_qc_precedence: no fteqcc found; skipped");
        return 0;
    }
    const fs::path work = ctx.tree / "build" / "qcprec";
    CompileResult qcPrio;
    CompileResult cPrio;
    {
        std::thread c([&] { cPrio = compileProgs(ctx, fteqcc, work, "c", {"-Fcpriority"}); });
        qcPrio = compileProgs(ctx, fteqcc, work, "qc", {});
        c.join();
    }
    for(const auto* r : {&qcPrio, &cPrio})
    {
        if(!r->error.empty())
        {
            out.raw(r->error);
            return 1;
        }
    }
    struct Found
    {
        const std::string* name;
        const std::string* file;
        int line;
    };
    std::vector<Found> found;
    for(const auto& [name, body] : qcPrio.funcs)
    {
        const auto o = cPrio.funcs.find(name);
        if(o == cPrio.funcs.end() || body.text == o->second.text)
        {
            continue;
        }
        // The first differing statement's source line (one body a prefix of the other: its last common one).
        const auto& other = o->second.text;
        const size_t common = std::min(body.text.size(), other.size());
        size_t k = common == 0 ? body.text.size() - 1 : common - 1;
        for(size_t j = 0; j < common; ++j)
        {
            if(body.text[j] != other[j])
            {
                k = j;
                break;
            }
        }
        found.push_back({&name, &body.file, body.text.empty() ? 0 : body.lines[k]});
    }
    if(!found.empty())
    {
        out.line("QC expressions that read differently under C priorities (parenthesise them; "
                 "docs/vr-port/CODE_STYLE.md, QuakeC):");
        for(const auto& f : found)
        {
            out.line("  {}: {}:{}", *f.name, *f.file, f.line);
        }
        return 1;
    }
    return 0;
}

} // namespace qvrc
