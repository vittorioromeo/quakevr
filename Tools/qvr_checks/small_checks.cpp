// progs_format and builtins: the two small checks.
#include "common.hpp"
#include "maps.hpp"

#include <regex>

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
    const za::String shown = args.empty() ? za::String{"quakevr/progs.dat"} : args[0];
    fs::path path = toPath(shown);
    if(path.is_relative())
    {
        path = ctx.tree / path;
    }
    bool ok = false;
    const za::String data = readRaw(path, &ok);
    std::error_code ec;
    if(!ok || !fs::is_regular_file(path, ec))
    {
        out.line("check_progs_format: {}: cannot read it", genericStr(toPath(shown)));
        return 1;
    }
    ProgsHeader hdr;
    bool isProgs = false;
    const za::String format = progsFormat(data, hdr, isProgs);
    if(!isProgs)
    {
        out.line("check_progs_format: {}: {}", genericStr(toPath(shown)), format);
        return 1;
    }
    const auto* h = hdr.h;
    out.line("check_progs_format: {}: {}, {} globals, {} statements, {} global defs, {} field defs, {} functions",
             genericStr(toPath(shown)), format, h[13], h[3], h[5], h[7], h[9]);
    if(format != "fte32")
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
    za::Vector<za::String> cmd = {pathStr(python), pathStr(script)};
    for(const auto& a : args)
    {
        cmd.pushBack(a);
    }
    const ProcResult r = runProcess(cmd, ctx.tree, true);
    out.text += withoutCr(r.out);
    return r.started ? r.rc : 1;
}

namespace
{

// re.sub(r"/\*.*?\*/", " ", s, flags=re.S) then re.sub(r"//[^\n]*", "", s)
za::String stripCommentsLoose(za::StringView s)
{
    za::String a;
    a.reserve(s.size());
    za::SizeT i = 0;
    while(i < s.size())
    {
        if(s[i] == '/' && i + 1 < s.size() && s[i + 1] == '*')
        {
            const za::SizeT e = s.find("*/", i + 2);
            if(e != npos)
            {
                a += ' ';
                i = e + 2;
                continue;
            }
        }
        a += s[i++];
    }
    za::String b;
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

// A regex match's group as text.
za::StringView group(const std::cmatch& m, int g)
{
    return za::StringView(m[g].first, static_cast<za::SizeT>(m[g].length()));
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
    const Set<za::String> engineOnly;
    const Set<za::String> optionalFiles = {"QC/ad/ad_builtins.qc"};

    const za::String vrSrc = readText(ctx.tree / "Quake/vr/vr_builtins.cpp");
    // VrBuiltin vrBuiltins\[\]\s*=\s*\{(.*?)\n\};
    za::SizeT bodyStart = npos;
    for(za::SizeT i = 0; (i = vrSrc.find("VrBuiltin vrBuiltins[]", i)) != npos; ++i)
    {
        za::SizeT p = skipSpace(vrSrc, i + 22);
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
    const za::SizeT bodyEnd = bodyStart == npos ? bodyStart : vrSrc.find("\n};", bodyStart);
    if(bodyEnd == npos)
    {
        out.line("check_builtins: vrBuiltins[] not found in Quake/vr/vr_builtins.cpp");
        return 1;
    }
    const za::String body = stripCommentsLoose(vrSrc.substrByPosLen(bodyStart, bodyEnd - bodyStart));
    const std::regex entryRe(R"re(\{\s*"(\w+)"\s*,)re");
    za::Vector<za::String> vrNames;
    for(std::cregex_iterator it(body.begin(), body.end(), entryRe), end; it != end; ++it)
    {
        vrNames.emplaceBack(group(*it, 1));
    }
    Set<za::String> ironwail;
    {
        const za::String pr = readText(ctx.tree / "Quake/pr_cmds.c");
        for(auto& name : lineStartGroups(pr, R"re(\s*\{\s*"(\w+)"\s*,)re", '{'))
        {
            ironwail.insert(ZA_MOVE(name));
        }
    }

    za::Vector<za::String> problems;
    Set<za::String> seen;
    for(const auto& n : vrNames)
    {
        if(seen.contains(n))
        {
            problems.pushBack(fmt("vrBuiltins names \"{}\" twice", n));
        }
        seen.insert(n);
    }

    za::Vector<za::String> paths;
    for(const char* dir : {"QC", "QC/ad"})
    {
        for(const auto& p : listFiles(ctx.tree / dir, {".qc"}, false))
        {
            za::String s = dir;
            s += "\\";
            s += pathStr(p.filename());
            paths.pushBack(ZA_MOVE(s)); // (sorted as Python's glob paths)
        }
    }
    sortStrings(paths);
    Map<za::String, za::Vector<za::String>> protos;
    for(auto& path : paths)
    {
        replaceChar(path, '\\', '/');
        const za::String text = stripCommentsLoose(readText(ctx.tree / toPath(path)));
        // (\w+)\s*=\s*#0\s*;  found from each "#0": the whole word before the `=` is the name
        for(za::SizeT p = 0; (p = text.find("#0", p)) != npos; p += 2)
        {
            const za::SizeT semi = skipSpace(text, p + 2);
            if(semi >= text.size() || text[semi] != ';')
            {
                continue;
            }
            za::SizeT b = p;
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
            za::SizeT a = b;
            while(a > 0 && isWord(text[a - 1]))
            {
                --a;
            }
            if(a < b)
            {
                protos[za::String{text.substrByPosLen(a, b - a)}].pushBack(path);
            }
        }
    }
    for(const auto* e : qza::sortedByKey(protos))
    {
        const auto& [name, ps] = *e;
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
        problems.pushBack(fmt("{}: `{} = #0` names no engine builtin (vrBuiltins, pr_builtindefs)", ps[0], name));
    }
    for(const auto& n : vrNames)
    {
        if(!protos.contains(n) && !engineOnly.contains(n))
        {
            problems.pushBack(fmt("vrBuiltins \"{}\" has no `#0` prototype in the QC (dead, or add it to engineOnly)", n));
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

// toolgun_weapons: every weapon id (Quake/vr/vr_weapon_ids.h) has a row in the toolgun's Weapons section.
//
// The spawn lists are hand-kept (vr_toolgun.cpp, vr_toolgun_packs.inc, vr_ad_spawnguns.inc); a new weapon id (WID_*)
// with no row is a weapon the toolgun cannot spawn (the grappling gun was). A row spells its id as `"weapon", <n>.f`
// or as QVR_AD_GUN's third argument. WID_FIST (0) is the empty hand, WID_ADELIMINATOR (24) is gone.
int checkToolgunWeapons(const Ctx& ctx, const Args&, Out& out)
{
    const Set<long long> exempt = {0, 24};
    Set<long long> rows;
    const std::regex plainRe(R"re("weapon"\s*,\s*(\d+)\.f)re");
    const std::regex adRe(R"re(QVR_AD_GUN\([^,]*,[^,]*,\s*(\d+)\s*,)re");
    for(const char* f : {"Quake/vr/vr_toolgun.cpp", "Quake/vr/vr_toolgun_packs.inc", "Quake/vr/vr_ad_spawnguns.inc"})
    {
        const za::String text = readText(ctx.tree / f);
        for(const std::regex* re : {&plainRe, &adRe})
        {
            for(std::cregex_iterator it(text.begin(), text.end(), *re), end; it != end; ++it)
            {
                rows.insert(toInt(group(*it, 1)));
            }
        }
    }
    const za::String ids = readText(ctx.tree / "Quake/vr/vr_weapon_ids.h");
    const std::regex idRe(R"re(#define\s+(WID_\w+)\s+(\d+))re");
    int checked = 0, missing = 0;
    for(std::cregex_iterator it(ids.begin(), ids.end(), idRe), end; it != end; ++it)
    {
        const long long n = toInt(group(*it, 2));
        if(exempt.contains(n))
        {
            continue;
        }
        ++checked;
        if(!rows.contains(n))
        {
            out.line("check_toolgun_weapons: {} ({}) has no row in the toolgun's Weapons section", group(*it, 1), n);
            ++missing;
        }
    }
    if(missing)
    {
        return 1;
    }
    out.line("check_toolgun_weapons: {} weapon ids, all in the toolgun", checked);
    return 0;
}

} // namespace qvrc
