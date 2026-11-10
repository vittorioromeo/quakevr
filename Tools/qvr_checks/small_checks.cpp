// progs_format and builtins: the two small checks.
#include "common.hpp"

#include <map>
#include <regex>
#include <set>

namespace qvrc
{

// progs_format: fails unless our progs.dat is the 32-bit format, and prints its globals count.
//
// Our QC compiles to FTE's extended progs (version 7, secondary version FTE32: 32-bit statements and defs, so no
// 65,536-globals limit): `#pragma flag enable fte32` in QC/defs.qc, a flag of our FTEQCC (Tools/fteqcc/README.md). A
// stock FTEQCC ignores the pragma with a warning and writes id's 16-bit version 6, which still runs today but would
// silently stop compiling (or start writing FTE32 only on overflow) as the globals grow: this check makes that loud.
// The engine reads both (PR_LoadProgs).
//
// Usage: qvr_checks progs_format [progs.dat]   (default quakevr/progs.dat, from the repository's root)
// Prints one line: the format and the counts. Exit 1 when it is not FTE32 (or not a progs.dat).
int checkProgsFormat(const Ctx& ctx, const Args& args, Out& out)
{
    const std::string shown = args.empty() ? "quakevr/progs.dat" : args[0];
    fs::path path = shown;
    if(path.is_relative())
    {
        path = ctx.tree / path;
    }
    bool ok = false;
    const std::string data = readRaw(path, &ok);
    std::error_code ec;
    if(!ok || !fs::is_regular_file(path, ec))
    {
        out.line("check_progs_format: {}: cannot read it", fs::path(shown).generic_string());
        return 1;
    }
    ProgsHeader hdr;
    bool isProgs = false;
    const std::string fmt = progsFormat(data, hdr, isProgs);
    if(!isProgs)
    {
        out.line("check_progs_format: {}: {}", fs::path(shown).generic_string(), fmt);
        return 1;
    }
    const auto* h = hdr.h;
    out.line("check_progs_format: {}: {}, {} globals, {} statements, {} global defs, {} field defs, {} functions",
             fs::path(shown).generic_string(), fmt, h[13], h[3], h[5], h[7], h[9]);
    if(fmt != "fte32")
    {
        out.line("check_progs_format: FAILED: not the 32-bit format (fte32). Compile with our FTEQCC "
                 "(Tools/fteqcc/fteqcc64.exe, or one built with Tools/fteqcc/fte32.patch): QC/defs.qc's "
                 "`#pragma flag enable fte32` selects it");
        return 1;
    }
    return 0;
}

// fgd: TrenchBroom's entity definitions (Misc/trenchbroom/fgdgen.py, still Python: a generator with its own FGD parser
// and model() expression evaluator) run as a process, so `all` runs it beside the others. Skipped without Python or
// the script.
int fgdgen(const Ctx& ctx, const Args& args, Out& out)
{
    const fs::path script = ctx.tree / "Misc/trenchbroom/fgdgen.py";
    std::error_code ec;
    fs::path python = which("python");
    if(python.empty())
    {
        python = which("py");
    }
    if(!fs::is_regular_file(script, ec) || python.empty())
    {
        return 0;
    }
    std::vector<std::string> cmd = {python.string(), script.string()};
    cmd.insert(cmd.end(), args.begin(), args.end());
    const ProcResult r = runProcess(cmd, ctx.tree, true);
    for(char c : r.out)
    {
        if(c != '\r')
        {
            out.text += c;
        }
    }
    return r.started ? r.rc : 1;
}

namespace
{

// re.sub(r"/\*.*?\*/", " ", s, flags=re.S) then re.sub(r"//[^\n]*", "", s)
std::string stripCommentsLoose(std::string_view s)
{
    std::string a;
    a.reserve(s.size());
    size_t i = 0;
    while(i < s.size())
    {
        if(s[i] == '/' && i + 1 < s.size() && s[i + 1] == '*')
        {
            const size_t e = s.find("*/", i + 2);
            if(e != std::string_view::npos)
            {
                a += ' ';
                i = e + 2;
                continue;
            }
        }
        a += s[i++];
    }
    std::string b;
    b.reserve(a.size());
    i = 0;
    while(i < a.size())
    {
        if(a[i] == '/' && i + 1 < a.size() && a[i + 1] == '/')
        {
            while(i < a.size() && a[i] != '\n')
            {
                ++i;
            }
            continue;
        }
        b += a[i++];
    }
    return b;
}

} // namespace

// builtins: the QC's by-name builtins (`= #0`) against the engine's tables.
//
// A QC prototype `... name = #0;` is bound at load by its name: Ironwail's pr_builtindefs (Quake/pr_cmds.c) or Quake
// VR's vrBuiltins (Quake/vr/vr_builtins.cpp, bindBuiltins). A misspelt or removed name leaves the function unbound and
// the first call errors at run time. This check fails when:
//   - a `#0` prototype in the QC (QC/*.qc, QC/ad/*.qc) names no builtin of either table (Arcane Dimensions' optional
//     ones, QC/ad/ad_builtins.qc, are allowed: it asks for them "when the engine has them");
//   - a vrBuiltins entry has no `#0` prototype in the QC (a dead builtin), unless it is listed in engineOnly below;
//   - a vrBuiltins name appears twice.
int checkBuiltins(const Ctx& ctx, const Args&, Out& out)
{
    // vrBuiltins entries no QC prototype names (none today: add one here with the reason when it is meant).
    const std::set<std::string> engineOnly;
    const std::set<std::string> optionalFiles = {"QC/ad/ad_builtins.qc"};

    const std::string vrSrc = readText(ctx.tree / "Quake/vr/vr_builtins.cpp");
    // VrBuiltin vrBuiltins\[\]\s*=\s*\{(.*?)\n\};
    size_t bodyStart = std::string::npos;
    for(size_t i = 0; (i = vrSrc.find("VrBuiltin vrBuiltins[]", i)) != std::string::npos; ++i)
    {
        size_t p = skipSpace(vrSrc, i + 22);
        if(p < vrSrc.size() && vrSrc[p] == '=')
        {
            p = skipSpace(vrSrc, p + 1);
            if(p < vrSrc.size() && vrSrc[p] == '{')
            {
                bodyStart = p + 1;
                break;
            }
        }
    }
    const size_t bodyEnd = bodyStart == std::string::npos ? bodyStart : vrSrc.find("\n};", bodyStart);
    if(bodyEnd == std::string::npos)
    {
        out.line("check_builtins: vrBuiltins[] not found in Quake/vr/vr_builtins.cpp");
        return 1;
    }
    const std::string body = stripCommentsLoose(std::string_view(vrSrc).substr(bodyStart, bodyEnd - bodyStart));
    const std::regex entryRe(R"re(\{\s*"(\w+)"\s*,)re");
    std::vector<std::string> vrNames;
    for(std::sregex_iterator it(body.begin(), body.end(), entryRe), end; it != end; ++it)
    {
        vrNames.push_back((*it)[1].str());
    }
    std::set<std::string> ironwail;
    {
        const std::string pr = readText(ctx.tree / "Quake/pr_cmds.c");
        for(auto& name : lineStartGroups(pr, R"re(\s*\{\s*"(\w+)"\s*,)re", '{'))
        {
            ironwail.insert(std::move(name));
        }
    }

    std::vector<std::string> problems;
    std::set<std::string> seen;
    for(const auto& n : vrNames)
    {
        if(seen.contains(n))
        {
            problems.push_back(std::format("vrBuiltins names \"{}\" twice", n));
        }
        seen.insert(n);
    }

    std::vector<std::string> paths;
    for(const char* dir : {"QC", "QC/ad"})
    {
        for(const auto& p : listFiles(ctx.tree / dir, {".qc"}, false))
        {
            paths.push_back(std::string(dir) + "\\" + p.filename().string()); // (sorted as Python's glob paths)
        }
    }
    sortStrings(paths);
    std::map<std::string, std::vector<std::string>> protos;
    for(auto& path : paths)
    {
        std::replace(path.begin(), path.end(), '\\', '/');
        const std::string text = stripCommentsLoose(readText(ctx.tree / path));
        // (\w+)\s*=\s*#0\s*;  found from each "#0": the whole word before the `=` is the name
        for(size_t p = 0; (p = text.find("#0", p)) != std::string::npos; p += 2)
        {
            const size_t semi = skipSpace(text, p + 2);
            if(semi >= text.size() || text[semi] != ';')
            {
                continue;
            }
            size_t b = p;
            while(b > 0 && isSpace(text[b - 1]))
            {
                --b;
            }
            if(b == 0 || text[b - 1] != '=')
            {
                continue;
            }
            --b;
            while(b > 0 && isSpace(text[b - 1]))
            {
                --b;
            }
            size_t a = b;
            while(a > 0 && isWord(text[a - 1]))
            {
                --a;
            }
            if(a < b)
            {
                protos[text.substr(a, b - a)].push_back(path);
            }
        }
    }
    for(const auto& [name, ps] : protos)
    {
        if(seen.contains(name) || ironwail.contains(name))
        {
            continue;
        }
        bool allOptional = true;
        for(const auto& p : ps)
        {
            allOptional &= optionalFiles.contains(p);
        }
        if(allOptional)
        {
            continue;
        }
        problems.push_back(
            std::format("{}: `{} = #0` names no engine builtin (vrBuiltins, pr_builtindefs)", ps[0], name));
    }
    for(const auto& n : vrNames)
    {
        if(!protos.contains(n) && !engineOnly.contains(n))
        {
            problems.push_back(
                std::format("vrBuiltins \"{}\" has no `#0` prototype in the QC (dead, or add it to engineOnly)", n));
        }
    }
    if(!problems.empty())
    {
        for(const auto& p : problems)
        {
            out.line("check_builtins: {}", p);
        }
        out.line("check_builtins: {} problem(s)", problems.size());
        return 1;
    }
    out.line("check_builtins: {} QC #0 prototypes, {} VR builtins, all bound", protos.size(), vrNames.size());
    return 0;
}

} // namespace qvrc
