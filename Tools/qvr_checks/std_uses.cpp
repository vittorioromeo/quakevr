// std_uses: our C++ in Quake/vr (.cpp, .hpp and the .inc pieces of a .cpp) and Tools/qvr_checks uses Zancle, not the standard library (docs/vr-port/ZANCLE.md):
// fails when a file has more `std::` uses of a name, or includes a heavy standard header, than
// Misc/quakevr/std_uses_allowlist.txt allows.
//
// Counted in the code only (comments, strings and character literals blanked): every `std::<name>` (by its first
// name: `std::filesystem::path` counts as `std::filesystem`), and every `#include` of a C++ standard header other than
// the C ones Zancle has nothing for (<cstdio>, <cstdarg>, <cstdlib>, <ctime>, <cctype>, <cwchar>, <cerrno>, <csignal>,
// <cfloat>, <climits>). Quake/vr/external (third-party, Zancle itself) is not checked.
//
// The allowlist's lines: `<file> <name> <max>` (`# why` after them; `<file>` relative to Quake/vr, or
// `Tools/qvr_checks/<file>` for this tool's own sources; `<name>` a `std::x`, a `<header>`, or `*` for every use in
// that file). A file under its cap passes; `--list` prints every current use (file, name, count) to write or tighten
// the list.
//
// Usage: qvr_checks std_uses [--list]
#include "common.hpp"
#include "maps.hpp"

#include <cstdlib>

namespace qvrc
{
namespace
{

constexpr const char* allowlistPath = "Misc/quakevr/std_uses_allowlist.txt";

bool cHeader(za::StringView h)
{
    for(const char* c : {"cstdio", "cstdarg", "cstdlib", "ctime", "cctype", "cwchar", "cerrno", "csignal", "cfloat", "climits"})
    {
        if(h == c)
        {
            return true;
        }
    }
    return false;
}

// name -> count in one file: `std::` in its stripped code, the includes in its raw text (stripCode blanks the
// preprocessor's lines; an include line starts with `#`, so a commented-out one doesn't count).
Map<za::String, int> uses(za::StringView raw, za::StringView code)
{
    Map<za::String, int> found;
    for(za::SizeT i = 0; (i = code.find("std::", i)) != npos; i += 5)
    {
        if(i > 0 && (isAlnum(code[i - 1]) || code[i - 1] == '_'))
        {
            continue; // `xstd::` (`::std::` counts)
        }
        const za::SizeT len = identAt(code, i + 5); // (a length)
        if(len > 0)
        {
            za::String name = "std::";
            name += code.substrByPosLen(i + 5, len);
            ++found[name];
        }
    }
    for(const auto line : splitLines(raw))
    {
        const auto t = trim(line);
        if(!startsWith(t, "#"))
        {
            continue;
        }
        za::SizeT p = skipSpace(t, 1);
        if(t.substrByPosLen(p, 7) != "include")
        {
            continue;
        }
        p = skipSpace(t, p + 7);
        if(p >= t.size() || t[p] != '<')
        {
            continue;
        }
        const za::SizeT close = t.find('>', p);
        const auto h = t.substrByPosLen(p + 1, close == npos ? 0 : close - p - 1);
        bool isStd = !h.empty();
        for(char c : h)
        {
            isStd = isStd && (c == '_' || (c >= 'a' && c <= 'z'));
        }
        if(isStd && !cHeader(h))
        {
            za::String name = "<";
            name += h;
            name += '>';
            ++found[name];
        }
    }
    return found;
}

} // namespace

int checkStdUses(const Ctx& ctx, const Args& args, Out& out)
{
    const bool list = !args.empty() && args[0] == "--list";
    Map<za::String, Map<za::String, int>> caps; // file -> name -> max
    bool ok = true;
    const za::String allow = readText(ctx.tree / allowlistPath, &ok);
    if(!ok)
    {
        out.line("std_uses: can't read {}", allowlistPath);
        return 1;
    }
    for(const auto line : splitLines(allow))
    {
        const auto words = splitWords(line.substrByPosLen(0, line.find('#')));
        if(words.size() == 3)
        {
            caps[za::String{words[0]}][za::String{words[1]}] = atoi(za::String{words[2]}.cStr());
        }
        else if(!words.empty())
        {
            out.line("std_uses: bad allowlist line: {}", line);
            return 1;
        }
    }
    // The files: Quake/vr's (named relative to it, external/ aside), then this tool's (named from the tree's root).
    struct Source
    {
        fs::path path;
        za::String key;   // the allowlist's name for it
        za::String shown; // the report's
    };
    za::Vector<Source> sources;
    const fs::path vr = ctx.tree / "Quake/vr";
    for(auto& p : listFiles(vr, {".cpp", ".hpp", ".inc"}, true)) // (.inc: the pieces of a .cpp)
    {
        za::String rel = relPosix(p, vr);
        if(!startsWith(rel, "external/"))
        {
            za::String shown = "Quake/vr/";
            shown += rel;
            sources.pushBack({ZA_MOVE(p), ZA_MOVE(rel), ZA_MOVE(shown)});
        }
    }
    for(auto& p : listFiles(ctx.tree / "Tools/qvr_checks", {".cpp", ".hpp"}, false))
    {
        za::String rel = relPosix(p, ctx.tree);
        za::String shown = rel;
        sources.pushBack({ZA_MOVE(p), ZA_MOVE(rel), ZA_MOVE(shown)});
    }
    int total = 0;
    int over = 0;
    for(const auto& src : sources)
    {
        const za::String raw = readText(src.path);
        const auto found = uses(raw, stripCode(raw));
        const auto fileCaps = caps.find(src.key);
        const bool any = fileCaps != caps.end() && fileCaps->second.contains("*");
        for(const auto* e : qza::sortedByKey(found))
        {
            const auto& [name, n] = *e;
            total += n;
            if(list)
            {
                out.line("{} {} {}", src.key, name, n);
                continue;
            }
            int cap = 0;
            if(fileCaps != caps.end())
            {
                const auto c = fileCaps->second.find(name);
                cap = any ? 1 << 30 : c == fileCaps->second.end() ? 0 : c->second;
            }
            if(n > cap)
            {
                ++over;
                out.line("{}: {} x{} (allowed {}): use Zancle (docs/vr-port/ZANCLE.md)", src.shown, name, n, cap);
            }
        }
    }
    if(list)
    {
        out.line("std_uses: {} uses", total);
    }
    else if(over > 0)
    {
        out.line("std_uses: {} over the allowlist ({}); {} std uses in all", over, allowlistPath, total);
    }
    return over > 0 ? 1 : 0;
}

} // namespace qvrc
