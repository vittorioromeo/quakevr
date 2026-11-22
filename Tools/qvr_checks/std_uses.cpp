// std_uses: our C++ in Quake/vr uses Zancle, not the standard library (docs/vr-port/ZANCLE.md): fails when a file has
// more `std::` uses of a name, or includes a heavy standard header, than Misc/quakevr/std_uses_allowlist.txt allows.
//
// Counted in the code only (comments, strings and character literals blanked): every `std::<name>` (by its first
// name: `std::filesystem::path` counts as `std::filesystem`), and every `#include` of a C++ standard header other than
// the C ones Zancle has nothing for (<cstdio>, <cstdarg>, <cstdlib>, <ctime>, <cctype>, <cwchar>, <cerrno>, <csignal>,
// <cfloat>, <climits>). Quake/vr/external (third-party, Zancle itself) is not checked.
//
// The allowlist's lines: `<file> <name> <max>` (`# why` after them; `<file>` relative to Quake/vr, `<name>` a
// `std::x`, a `<header>`, or `*` for every use in that file). A file under its cap passes; `--list` prints every
// current use (file, name, count) to write or tighten the list.
//
// Usage: qvr_checks std_uses [--list]
#include "common.hpp"

#include <map>

namespace qvrc
{
namespace
{

constexpr const char* allowlistPath = "Misc/quakevr/std_uses_allowlist.txt";

bool cHeader(std::string_view h)
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
std::map<std::string, int> uses(const std::string& raw, const std::string& code)
{
    std::map<std::string, int> found;
    for(size_t i = 0; (i = code.find("std::", i)) != std::string::npos; i += 5)
    {
        if(i > 0 && (isAlnum(code[i - 1]) || code[i - 1] == '_'))
        {
            continue; // `xstd::` (`::std::` counts)
        }
        const size_t len = identAt(code, i + 5); // (a length)
        if(len > 0)
        {
            ++found["std::" + code.substr(i + 5, len)];
        }
    }
    for(const auto line : splitLines(raw))
    {
        const auto t = trim(line);
        if(!startsWith(t, "#"))
        {
            continue;
        }
        size_t p = skipSpace(t, 1);
        if(t.substr(p, 7) != "include")
        {
            continue;
        }
        p = skipSpace(t, p + 7);
        if(p >= t.size() || t[p] != '<')
        {
            continue;
        }
        const size_t close = t.find('>', p);
        const auto h = t.substr(p + 1, close == std::string_view::npos ? 0 : close - p - 1);
        bool isStd = !h.empty();
        for(char c : h)
        {
            isStd = isStd && (c == '_' || (c >= 'a' && c <= 'z'));
        }
        if(isStd && !cHeader(h))
        {
            ++found["<" + std::string(h) + ">"];
        }
    }
    return found;
}

} // namespace

int checkStdUses(const Ctx& ctx, const Args& args, Out& out)
{
    const bool list = !args.empty() && args[0] == "--list";
    const fs::path root = ctx.tree / "Quake/vr";
    std::map<std::string, std::map<std::string, int>> caps; // file -> name -> max
    bool ok = true;
    const std::string allow = readText(ctx.tree / allowlistPath, &ok);
    if(!ok)
    {
        out.line("std_uses: can't read {}", allowlistPath);
        return 1;
    }
    for(const auto line : splitLines(allow))
    {
        const auto words = splitWords(line.substr(0, line.find('#')));
        if(words.size() == 3)
        {
            caps[std::string(words[0])][std::string(words[1])] = std::atoi(std::string(words[2]).c_str());
        }
        else if(!words.empty())
        {
            out.line("std_uses: bad allowlist line: {}", line);
            return 1;
        }
    }
    int total = 0;
    int over = 0;
    for(const auto& p : listFiles(root, {".cpp", ".hpp"}, true))
    {
        const std::string rel = relPosix(p, root);
        if(startsWith(rel, "external/"))
        {
            continue;
        }
        const std::string raw = readText(p);
        const auto found = uses(raw, stripCode(raw));
        const auto fileCaps = caps.find(rel);
        const bool any = fileCaps != caps.end() && fileCaps->second.contains("*");
        for(const auto& [name, n] : found)
        {
            total += n;
            if(list)
            {
                out.line("{} {} {}", rel, name, n);
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
                out.line("Quake/vr/{}: {} x{} (allowed {}): use Zancle (docs/vr-port/ZANCLE.md)", rel, name, n, cap);
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
