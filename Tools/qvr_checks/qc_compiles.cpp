// release_qc and qc_precedence: the two checks that compile the QC (in work folders under build/).
#include "common.hpp"

#include <algorithm>
#include <map>
#include <regex>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

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

// A read-only view of a whole file.
struct Mapped
{
    HANDLE file = INVALID_HANDLE_VALUE;
    HANDLE map = nullptr;
    const char* data = nullptr;
    size_t size = 0;

    explicit Mapped(const fs::path& p)
    {
        file = CreateFileW(p.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if(file == INVALID_HANDLE_VALUE)
        {
            return;
        }
        LARGE_INTEGER sz{};
        GetFileSizeEx(file, &sz);
        size = static_cast<size_t>(sz.QuadPart);
        if(size == 0)
        {
            return;
        }
        map = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if(map)
        {
            data = static_cast<const char*>(MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0));
        }
    }
    ~Mapped()
    {
        if(data)
        {
            UnmapViewOfFile(data);
        }
        if(map)
        {
            CloseHandle(map);
        }
        if(file != INVALID_HANDLE_VALUE)
        {
            CloseHandle(file);
        }
    }
    Mapped(const Mapped&) = delete;
    Mapped& operator=(const Mapped&) = delete;
};

struct Instr
{
    int line;
    std::string text;
};
using Listing = std::map<std::string, std::vector<Instr>>;

// One instruction line: its trailing /*N*/ line number off, and every temp_N / locked_N renamed `tmp` (temporaries are
// numbered per function and may be allocated differently: they compare by role, not number).
Instr parseInstr(std::string_view l)
{
    Instr in{0, {}};
    // \s*/\*(\d+)\*/\s*$
    std::string_view s = l;
    while(!s.empty() && isSpace(s.back()))
    {
        s.remove_suffix(1);
    }
    if(endsWith(s, "*/"))
    {
        const size_t open = s.rfind("/*");
        if(open != std::string_view::npos && open + 2 < s.size() - 2)
        {
            const auto num = s.substr(open + 2, s.size() - 2 - (open + 2));
            if(std::all_of(num.begin(), num.end(), isDigit))
            {
                in.line = std::stoi(std::string(num));
                s = s.substr(0, open);
            }
        }
    }
    s = trim(s);
    std::string& o = in.text;
    o.reserve(s.size());
    for(size_t i = 0; i < s.size();)
    {
        // \b(?:temp|locked)_\d+\b
        bool replaced = false;
        if(wordStart(s, i))
        {
            for(std::string_view w : {"temp_", "locked_"})
            {
                if(s.substr(i, w.size()) == w)
                {
                    size_t e = i + w.size();
                    while(e < s.size() && isDigit(s[e]))
                    {
                        ++e;
                    }
                    if(e > i + w.size() && wordEnd(s, e))
                    {
                        o += "tmp";
                        i = e;
                        replaced = true;
                    }
                    break;
                }
            }
        }
        if(!replaced)
        {
            o += s[i++];
        }
    }
    return in;
}

// qc.asm -> {function: [(line, instruction)]}. The file is huge (about 1.7 GB: each function first lists every
// temporary of the progs, `local ...` lines, then its instructions, tab-indented): mapped, and each function's locals
// skipped by a search for its first instruction line, so only the instructions are decoded.
Listing parseAsm(const fs::path& asmPath)
{
    Listing funcs;
    Mapped mm(asmPath);
    if(!mm.data)
    {
        return funcs;
    }
    const std::string_view all(mm.data, mm.size);
    size_t pos = 0;
    while(true)
    {
        const size_t h = all.find(" = asm\n", pos);
        if(h == std::string_view::npos)
        {
            break;
        }
        const size_t nl = h == 0 ? std::string_view::npos : all.rfind('\n', h - 1);
        const size_t start = nl == std::string_view::npos ? 0 : nl + 1;
        std::string_view name = all.substr(start, h - start);
        while(!name.empty() && name.back() == '\r')
        {
            name.remove_suffix(1);
        }
        const size_t end = all.find("\n}", h);
        if(end == std::string_view::npos)
        {
            break;
        }
        std::vector<Instr> body;
        const size_t first = all.substr(0, end).find("\n\t", h);
        if(first != std::string_view::npos)
        {
            for(auto l : splitLines(all.substr(first + 1, end - (first + 1))))
            {
                if(!l.empty() && l[0] == '\t')
                {
                    body.push_back(parseInstr(l));
                }
            }
        }
        funcs[std::string(name)] = std::move(body);
        pos = end + 2;
    }
    return funcs;
}

struct CompileResult
{
    Listing funcs;
    std::string error; // empty: compiled
};

// Compiles QC/progs.src into work/sub with the asm listing; parses it, then empties it (it is about 1.7 GB).
CompileResult compileAsm(const Ctx& ctx, const fs::path& fteqcc, const fs::path& work, const std::string& sub,
                         const std::vector<std::string>& extra)
{
    CompileResult res;
    const fs::path out = work / sub;
    if(!writeWorkProgsSrc(ctx, out))
    {
        res.error = "cannot write " + (out / "progs.src").generic_string();
        return res;
    }
    const fs::path asmPath = out / "qc.asm";
    std::error_code ec;
    if(fs::is_regular_file(asmPath, ec))
    {
        writeRaw(asmPath, ""); // (a failed compile must not pass on the last run's listing)
    }
    std::vector<std::string> cmd = {fteqcc.string()};
    cmd.insert(cmd.end(), qcFlags.begin(), qcFlags.end());
    cmd.emplace_back("-Fwasm");
    cmd.insert(cmd.end(), extra.begin(), extra.end());
    cmd.insert(cmd.end(), {"-srcfile", "progs.src"});
    const ProcResult r = runProcess(cmd, out, false);
    if(r.rc != 0 || r.out.find("ERROR") != std::string::npos || !fs::is_regular_file(asmPath, ec))
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
    else
    {
        res.funcs = parseAsm(asmPath);
    }
    // The listing and the progs are only this check's: emptied, so the work folder stays small.
    writeRaw(asmPath, "");
    if(fs::is_regular_file(out / "progs.dat", ec))
    {
        writeRaw(out / "progs.dat", "");
    }
    return res;
}

// The QC file that defines `func` (its name as qc.asm prints it: `float(entity) Name`) and has line `line`.
std::string sourceFile(const Ctx& ctx, const std::vector<std::pair<fs::path, std::string>>& qcFiles,
                       const std::string& func, int line)
{
    std::string name = func.substr(func.rfind(')') == std::string::npos ? 0 : func.rfind(')') + 1);
    name = std::string(trim(name));
    for(const auto& [path, text] : qcFiles)
    {
        // ^[^/\n]*\)\s*NAME\s*=  (re.M)
        bool defines = false;
        for(size_t i = 0; !defines && (i = text.find(name, i)) != std::string::npos; ++i)
        {
            size_t p = skipSpace(text, i + name.size());
            if(p >= text.size() || text[p] != '=')
            {
                continue;
            }
            // back from the name: spaces, then `)`, with no '/' or newline before it on the line
            size_t b = i;
            while(b > 0 && isSpace(text[b - 1]))
            {
                --b;
            }
            if(b == 0 || text[b - 1] != ')')
            {
                continue;
            }
            // (`\s*` may cross newlines: the `)` line is the one checked for a '/')
            size_t ls = b - 1;
            bool slash = false;
            while(ls > 0 && text[ls - 1] != '\n')
            {
                slash |= text[ls - 1] == '/';
                --ls;
            }
            // ([^/\n]* is greedy: any `)` on that line with no '/' before it would do; this `)` is the last one)
            defines = !slash;
        }
        if(defines && static_cast<int>(std::count(text.begin(), text.end(), '\n')) >= line - 1)
        {
            return relPosix(path, ctx.tree);
        }
    }
    return "?";
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
// and compares each function's instructions. A function that differs holds an expression that depends on the
// priorities: parenthesise it (or use a temporary) so it reads the same both ways.
//
// The compiler: --fteqcc, else $FTEQCC, else C:/OHWorkspace/quakevr/QC/fteqcc64.exe, else fteqcc64/fteqcc on PATH.
// Work files go to build/qcprec/ (git-ignored); the listings are emptied after use. QC/ and quakevr/progs.dat are not
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
        std::thread c([&] { cPrio = compileAsm(ctx, fteqcc, work, "c", {"-Fcpriority"}); });
        qcPrio = compileAsm(ctx, fteqcc, work, "qc", {});
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
    std::vector<std::pair<std::string, int>> found;
    for(const auto& [name, body] : qcPrio.funcs)
    {
        const auto o = cPrio.funcs.find(name);
        if(o == cPrio.funcs.end())
        {
            continue;
        }
        const auto& other = o->second;
        bool same = body.size() == other.size();
        for(size_t k = 0; same && k < body.size(); ++k)
        {
            same = body[k].text == other[k].text;
        }
        if(same)
        {
            continue;
        }
        // The first differing instruction's source line.
        const size_t common = std::min(body.size(), other.size());
        long long k = static_cast<long long>(common) - 1;
        for(size_t j = 0; j < common; ++j)
        {
            if(body[j].text != other[j].text)
            {
                k = static_cast<long long>(j);
                break;
            }
        }
        int line = 0;
        if(!body.empty())
        {
            // (Python's body[-1] when k is -1)
            line = body[k < 0 ? body.size() - 1 : static_cast<size_t>(k)].line;
        }
        found.emplace_back(name, line);
    }
    if(!found.empty())
    {
        std::vector<fs::path> qcPaths = listFiles(ctx.tree / "QC", {".qc"}, true);
        sortPaths(qcPaths);
        std::vector<std::pair<fs::path, std::string>> qcFiles;
        for(const auto& p : qcPaths)
        {
            qcFiles.emplace_back(p, readText(p));
        }
        out.line("QC expressions that read differently under C priorities (parenthesise them; "
                 "docs/vr-port/CODE_STYLE.md, QuakeC):");
        std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for(const auto& [name, line] : found)
        {
            out.line("  {}: {}:{}", name, sourceFile(ctx, qcFiles, name, line), line);
        }
        return 1;
    }
    return 0;
}

} // namespace qvrc
