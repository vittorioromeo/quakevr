// test_numbers: the numbered test dispatchers against their registry (QC/test_numbers.txt).
//
// The numbered test cvars (`vr_agrunt_test N`, `vr_ad_test N`, ...) are dispatched by QC functions as chains of
// `if(t == N)` / `if(t >= A && t <= B)` branches. Parallel workers picking "the next free number" have clashed (the
// second branch on a number is dead code). This check fails when:
//   - two registry lines claim the same number of the same cvar;
//   - two dispatch branches (not nested in one another) take the same number;
//   - a dispatched number is not in the registry, or a registry line is dispatched by nothing;
//   - a Debug menu / scenario command (Quake/vr: `command("...", "... vr_x_test N ...")`) runs a number that is not
//     registered.
// The registry's format is in QC/test_numbers.txt's header. Exit 1 on a problem. `--list` prints each cvar's
// dispatched numbers (for a new registry line).
#include "common.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <regex>
#include <set>

namespace qvrc
{
namespace
{

constexpr const char* registryPath = "QC/test_numbers.txt";
constexpr long long inf = 1LL << 30;

std::string fmtRange(long long lo, long long hi)
{
    if(hi >= inf)
    {
        return std::format("{}+", lo);
    }
    return lo == hi ? std::format("{}", lo) : std::format("{}-{}", lo, hi);
}

bool allDigits(std::string_view s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), isDigit);
}

// "48", "20-39", "100+"
std::optional<std::pair<long long, long long>> parseNumbers(std::string_view spec)
{
    size_t k = 0;
    while(k < spec.size() && isDigit(spec[k]))
    {
        ++k;
    }
    if(k == 0)
    {
        return std::nullopt;
    }
    const long long lo = std::stoll(std::string(spec.substr(0, k)));
    long long hi = lo;
    const auto rest = spec.substr(k);
    if(rest == "+")
    {
        hi = inf;
    }
    else if(!rest.empty())
    {
        if(rest[0] != '-' || !allDigits(rest.substr(1)))
        {
            return std::nullopt;
        }
        hi = std::stoll(std::string(rest.substr(1)));
    }
    if(hi < lo)
    {
        return std::nullopt;
    }
    return std::pair{lo, hi};
}

struct Dispatcher
{
    std::string cvar, file, func, var;
    std::optional<std::pair<long long, long long>> within;
    int line;
    bool loose;
};

struct Entry
{
    long long lo, hi;
    int line;
    bool fallthrough;
};

std::string stripComments(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    size_t i = 0;
    while(i < text.size())
    {
        if(text[i] == '/' && i + 1 < text.size() && text[i + 1] == '/')
        {
            const size_t j = text.find('\n', i);
            i = j == std::string_view::npos ? text.size() : j;
        }
        else if(text[i] == '/' && i + 1 < text.size() && text[i + 1] == '*')
        {
            const size_t j = text.find("*/", i + 2);
            const size_t k = j == std::string_view::npos ? text.size() : j + 2;
            out += blankKeepNewlines(text.substr(i, k - i));
            i = k;
        }
        else if(text[i] == '"')
        {
            size_t j = i + 1;
            while(j < text.size() && text[j] != '"')
            {
                j += text[j] == '\\' ? 2 : 1;
            }
            out += '"';
            out.append(j - i - 1, ' ');
            out += '"';
            i = j + 1;
        }
        else
        {
            out += text[i++];
        }
    }
    return out;
}

struct Branch
{
    std::vector<std::pair<long long, long long>> nums;
    size_t start, end;
    int lineOffset;
};

std::string_view stripParens(std::string_view s)
{
    s = trim(s);
    while(!s.empty() && (s.front() == '(' || s.front() == ')'))
    {
        s.remove_prefix(1);
    }
    while(!s.empty() && (s.back() == '(' || s.back() == ')'))
    {
        s.remove_suffix(1);
    }
    return trim(s);
}

std::vector<std::string_view> splitOn(std::string_view s, std::string_view sep)
{
    std::vector<std::string_view> out;
    size_t a = 0;
    while(true)
    {
        const size_t e = s.find(sep, a);
        if(e == std::string_view::npos)
        {
            out.push_back(s.substr(a));
            return out;
        }
        out.push_back(s.substr(a, e - a));
        a = e + sep.size();
    }
}

// The numbers a condition takes if it tests only `var` against numbers (an || of ==, >=, <=, ranges); nullopt if it is
// anything else (another variable in it: a refinement, not a dispatch).
std::optional<std::vector<std::pair<long long, long long>>> conditionNumbers(std::string_view cond, const std::string& var)
{
    // (re.sub(r"\bvar\b", "", cond), then no letter or underscore may be left)
    for(size_t i = 0; i < cond.size();)
    {
        if(cond.compare(i, var.size(), var) == 0 && wordStart(cond, i) && wordEnd(cond, i + var.size()))
        {
            i += var.size();
            continue;
        }
        const char c = cond[i];
        if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
        {
            return std::nullopt;
        }
        ++i;
    }
    std::vector<std::pair<long long, long long>> out;
    for(auto alt : splitOn(cond, "||"))
    {
        alt = stripParens(alt);
        long long lo = -inf;
        long long hi = inf;
        bool ok = false;
        for(auto term : splitOn(alt, "&&"))
        {
            term = stripParens(term);
            // var \s* (==|>=|<=|>|<) \s* (\d+), the whole term
            if(term.substr(0, var.size()) != var)
            {
                return std::nullopt;
            }
            size_t p = skipSpace(term, var.size());
            std::string_view op;
            for(std::string_view o : {"==", ">=", "<=", ">", "<"})
            {
                if(term.substr(p, o.size()) == o)
                {
                    op = o;
                    break;
                }
            }
            if(op.empty())
            {
                return std::nullopt;
            }
            p = skipSpace(term, p + op.size());
            const auto digits = term.substr(p);
            if(!allDigits(digits))
            {
                return std::nullopt;
            }
            const long long k = std::stoll(std::string(digits));
            ok = true;
            if(op == "==")
            {
                lo = std::max(lo, k);
                hi = std::min(hi, k);
            }
            else if(op == ">=")
            {
                lo = std::max(lo, k);
            }
            else if(op == ">")
            {
                lo = std::max(lo, k + 1);
            }
            else if(op == "<=")
            {
                hi = std::min(hi, k);
            }
            else
            {
                hi = std::min(hi, k - 1);
            }
        }
        if(!ok || lo > hi)
        {
            return std::nullopt;
        }
        out.emplace_back(std::max(lo, 0LL), hi);
    }
    return out;
}

// Each `if(...)` testing only `var`: (ranges, start, end of its block, line offset).
std::vector<Branch> branches(std::string_view body, const std::string& var)
{
    std::vector<Branch> found;
    for(size_t i = 0; (i = body.find("if", i)) != std::string_view::npos; i += 2)
    {
        if(!wordStart(body, i))
        {
            continue;
        }
        const size_t c0 = skipSpace(body, i + 2);
        if(c0 >= body.size() || body[c0] != '(')
        {
            continue;
        }
        const size_t c1 = matchClose(body, c0, '(', ')');
        if(c1 == std::string_view::npos)
        {
            continue;
        }
        auto nums = conditionNumbers(body.substr(c0 + 1, c1 - c0 - 1), var);
        if(!nums)
        {
            continue;
        }
        const size_t j = skipSpace(body, c1 + 1);
        size_t end;
        if(j < body.size() && body[j] == '{')
        {
            end = matchClose(body, j, '{', '}');
        }
        else
        {
            end = body.find(';', j);
        }
        const long long endPos = end == std::string_view::npos ? -1 : static_cast<long long>(end);
        found.push_back({std::move(*nums), i, static_cast<size_t>(endPos), lineAt(body, i) - 1});
    }
    // (a branch inside another's block refines it: not a dispatch of its own)
    std::vector<Branch> top;
    for(size_t b = 0; b < found.size(); ++b)
    {
        bool inner = false;
        for(size_t o = 0; o < found.size() && !inner; ++o)
        {
            inner = o != b && found[o].start < found[b].start &&
                    static_cast<long long>(found[b].end) <= static_cast<long long>(found[o].end);
        }
        if(!inner)
        {
            top.push_back(found[b]);
        }
    }
    return top;
}

// Every identifier followed by `(` in body[start..end].
std::set<std::string> callsIn(std::string_view body, size_t start, size_t end)
{
    std::set<std::string> out;
    const auto s = body.substr(start, end == static_cast<size_t>(-1) ? 0 : end + 1 - start);
    for(size_t i = 0; i < s.size();)
    {
        const size_t n = identAt(s, i);
        if(n == 0 || !wordStart(s, i))
        {
            ++i;
            continue;
        }
        const size_t p = skipSpace(s, i + n);
        if(p < s.size() && s[p] == '(')
        {
            out.emplace(s.substr(i, n));
        }
        i += n;
    }
    return out;
}

bool overlap(long long alo, long long ahi, long long blo, long long bhi)
{
    return alo <= bhi && blo <= ahi;
}

struct Leaf
{
    long long lo, hi;
    std::string where;
    std::string looseFunc; // empty: not loose
};

} // namespace

int checkTestNumbers(const Ctx& ctx, const Args& args, Out& out)
{
    std::vector<std::string> errors;
    std::vector<Dispatcher> dispatchers;
    std::map<std::string, std::vector<Entry>> entries;
    bool ok = false;
    const std::string reg = readText(ctx.tree / registryPath, &ok);
    if(!ok)
    {
        out.line("test numbers: {} not found", registryPath);
        return 1;
    }
    int n = 0;
    for(auto rawLine : splitLines(reg))
    {
        ++n;
        const auto cut = rawLine.find('#');
        const auto line = trim(cut == std::string_view::npos ? rawLine : rawLine.substr(0, cut));
        if(line.empty())
        {
            continue;
        }
        auto words = splitWords(line);
        if(words[0] == "dispatch")
        {
            std::vector<std::string_view> opts(words.begin() + std::min<size_t>(5, words.size()), words.end());
            const auto li = std::find(opts.begin(), opts.end(), "loose");
            const bool loose = li != opts.end();
            if(loose)
            {
                opts.erase(li);
            }
            if(words.size() < 5 || (opts.size() != 0 && opts.size() != 2) || (!opts.empty() && opts[0] != "within"))
            {
                errors.push_back(std::format(
                    "{}:{}: dispatch <cvar> <file> <function> <variable> [within A-B] [loose]", registryPath, n));
                continue;
            }
            Dispatcher d{std::string(words[1]), std::string(words[2]), std::string(words[3]), std::string(words[4]),
                         std::nullopt, n, loose};
            if(!opts.empty())
            {
                d.within = parseNumbers(opts[1]);
            }
            dispatchers.push_back(std::move(d));
            continue;
        }
        if(words.size() < 3)
        {
            errors.push_back(std::format("{}:{}: <cvar> <number|A-B|A+> <file> <what it tests>", registryPath, n));
            continue;
        }
        // (N*: the dispatcher's fall-through, e.g. `if(req) { ... }` past its numbered branches: no branch of its own)
        const bool fallthrough = endsWith(words[1], "*");
        auto spec = words[1];
        while(!spec.empty() && spec.back() == '*')
        {
            spec.remove_suffix(1);
        }
        const auto r = parseNumbers(spec);
        if(!r)
        {
            errors.push_back(std::format("{}:{}: bad number '{}'", registryPath, n, words[1]));
            continue;
        }
        entries[std::string(words[0])].push_back({r->first, r->second, n, fallthrough});
    }

    std::map<std::string, std::set<std::string>> subsOf;
    for(const auto& d : dispatchers)
    {
        subsOf[d.cvar].insert(d.func);
    }
    std::map<std::string, std::vector<Leaf>> leaves;
    std::vector<std::string> leafOrder; // (Python dicts keep insertion order)
    for(const auto& d : dispatchers)
    {
        bool fileOk = false;
        const std::string raw = readText(ctx.tree / d.file, &fileOk);
        if(!fileOk)
        {
            errors.push_back(std::format("{}: dispatcher file {} not found", registryPath, d.file));
            continue;
        }
        const std::string text = stripComments(raw);
        // \bNAME\s*=\s*\{
        size_t start = std::string::npos;
        for(size_t i = 0; (i = text.find(d.func, i)) != std::string::npos; ++i)
        {
            if(!wordStart(text, i))
            {
                continue;
            }
            size_t p = skipSpace(text, i + d.func.size());
            if(p >= text.size() || text[p] != '=')
            {
                continue;
            }
            p = skipSpace(text, p + 1);
            if(p < text.size() && text[p] == '{')
            {
                start = p;
                break;
            }
        }
        if(start == std::string::npos)
        {
            errors.push_back(std::format("{}: dispatcher {} not found in {}", registryPath, d.func, d.file));
            continue;
        }
        const size_t end = matchClose(text, start, '{', '}');
        // (Python's text[start:end + 1] with end = -1 is empty)
        const std::string_view body =
            end == std::string::npos ? std::string_view() : std::string_view(text).substr(start, end + 1 - start);
        const int line0 = lineAt(text, start);
        std::set<std::string> others = subsOf[d.cvar];
        others.erase(d.func);
        for(const auto& b : branches(body, d.var))
        {
            const std::string where = std::format("{}:{} ({})", d.file, line0 + b.lineOffset, d.func);
            bool handsOn = false;
            for(const auto& c : callsIn(body, b.start, b.end))
            {
                handsOn |= others.contains(c);
            }
            if(handsOn)
            {
                continue; // (it hands these numbers to a sub-dispatcher of its own registry line)
            }
            for(const auto& [lo, hi] : b.nums)
            {
                if(lo == 0 && hi == 0)
                {
                    continue; // (0: no test)
                }
                if(d.within && !(d.within->first <= lo && hi <= d.within->second))
                {
                    errors.push_back(std::format("{}: {} {} outside its dispatcher's range {}", where, d.cvar,
                                                 fmtRange(lo, hi), fmtRange(d.within->first, d.within->second)));
                }
                if(!leaves.contains(d.cvar))
                {
                    leafOrder.push_back(d.cvar);
                }
                leaves[d.cvar].push_back({lo, hi, where, d.loose ? d.func : std::string()});
            }
        }
    }
    if(std::find(args.begin(), args.end(), "--list") != args.end())
    {
        for(const auto& [cvar, ls] : leaves)
        {
            std::set<std::pair<long long, std::string>> specs;
            for(const auto& l : ls)
            {
                specs.emplace(l.lo, fmtRange(l.lo, l.hi));
            }
            std::string line = cvar + ":";
            std::set<std::string> seenSpec;
            for(const auto& [lo, s] : specs)
            {
                if(seenSpec.insert(s).second)
                {
                    line += " " + s;
                }
            }
            out.raw(line);
        }
        return 0;
    }
    // Dispatch clashes (a `loose` dispatcher's own branches may share numbers: set-up branches, then the cases).
    for(const auto& cvar : leafOrder)
    {
        const auto& ls = leaves[cvar];
        for(size_t i = 0; i < ls.size(); ++i)
        {
            for(size_t j = i + 1; j < ls.size(); ++j)
            {
                if(!ls[i].looseFunc.empty() && ls[i].looseFunc == ls[j].looseFunc)
                {
                    continue;
                }
                if(overlap(ls[i].lo, ls[i].hi, ls[j].lo, ls[j].hi))
                {
                    errors.push_back(std::format("{} test number clash: {} takes {} and {} takes {}", cvar, ls[i].where,
                                                 fmtRange(ls[i].lo, ls[i].hi), ls[j].where,
                                                 fmtRange(ls[j].lo, ls[j].hi)));
                }
            }
        }
    }
    // Registry clashes, coverage both ways. (Python's dict order: the cvars in their first registry line's order.)
    std::vector<std::string> entryOrder;
    {
        std::vector<std::pair<int, std::string>> firsts;
        for(const auto& [cvar, es] : entries)
        {
            firsts.emplace_back(es.front().line, cvar);
        }
        std::sort(firsts.begin(), firsts.end());
        for(auto& f : firsts)
        {
            entryOrder.push_back(f.second);
        }
    }
    for(const auto& cvar : entryOrder)
    {
        const auto& es = entries[cvar];
        for(size_t i = 0; i < es.size(); ++i)
        {
            for(size_t j = i + 1; j < es.size(); ++j)
            {
                if(overlap(es[i].lo, es[i].hi, es[j].lo, es[j].hi))
                {
                    errors.push_back(std::format("{}:{} and :{} both claim {} {}", registryPath, es[i].line, es[j].line,
                                                 cvar, fmtRange(std::max(es[i].lo, es[j].lo),
                                                                std::min(es[i].hi, es[j].hi))));
                }
            }
        }
        for(const auto& e : es)
        {
            bool run = false;
            if(const auto it = leaves.find(cvar); it != leaves.end())
            {
                for(const auto& l : it->second)
                {
                    run |= overlap(e.lo, e.hi, l.lo, l.hi);
                }
            }
            if(!e.fallthrough && !run)
            {
                errors.push_back(std::format("{}:{}: {} {} is registered but no dispatcher runs it", registryPath,
                                             e.line, cvar, fmtRange(e.lo, e.hi)));
            }
        }
    }
    // (lo..hi within the union of the cvar's registry lines)
    const auto covered = [&](const std::string& cvar, long long lo, long long hi) {
        const auto it = entries.find(cvar);
        long long pos = lo;
        while(pos <= hi)
        {
            const Entry* e = nullptr;
            if(it != entries.end())
            {
                for(const auto& x : it->second)
                {
                    if(x.lo <= pos && pos <= x.hi)
                    {
                        e = &x;
                        break;
                    }
                }
            }
            if(!e)
            {
                return false;
            }
            if(e->hi >= inf)
            {
                return true;
            }
            pos = e->hi + 1;
        }
        return true;
    };
    for(const auto& cvar : leafOrder)
    {
        for(const auto& l : leaves[cvar])
        {
            if(!covered(cvar, l.lo, l.hi))
            {
                errors.push_back(std::format("{}: {} {} is not in {} (claim it there)", l.where, cvar,
                                             fmtRange(l.lo, l.hi), registryPath));
            }
        }
    }
    // The menus' commands.
    std::set<std::string> cvars;
    for(const auto& [c, _] : entries)
    {
        cvars.insert(c);
    }
    for(const auto& [c, _] : leaves)
    {
        cvars.insert(c);
    }
    const std::regex cmdRe(R"re(\bcommand\(\s*"(?:[^"\\]|\\.)*"\s*,\s*"((?:[^"\\]|\\.)*)")re");
    const std::regex testRe(R"(\b(vr_\w+_test)\s+(\d+))");
    for(const char* ext : {".inc", ".cpp", ".hpp"})
    {
        std::vector<std::string> paths;
        for(const auto& p : listFiles(ctx.tree / "Quake/vr", {ext}, false))
        {
            paths.push_back("Quake/vr\\" + p.filename().string()); // (sorted as Python's glob paths)
        }
        sortStrings(paths);
        for(auto& path : paths)
        {
            std::replace(path.begin(), path.end(), '\\', '/');
            const std::string text = readText(ctx.tree / path);
            if(text.find("command(") == std::string::npos)
            {
                continue;
            }
            int ln = 0;
            for(auto line : splitLines(text))
            {
                ++ln;
                if(line.find("command(") == std::string_view::npos)
                {
                    continue;
                }
                const std::string l(line);
                for(std::sregex_iterator it(l.begin(), l.end(), cmdRe), end; it != end; ++it)
                {
                    const std::string cmd = (*it)[1].str();
                    for(std::sregex_iterator t(cmd.begin(), cmd.end(), testRe), tend; t != tend; ++t)
                    {
                        const std::string cvar = (*t)[1].str();
                        const long long k = std::stoll((*t)[2].str());
                        bool reg2 = false;
                        if(const auto e = entries.find(cvar); e != entries.end())
                        {
                            for(const auto& x : e->second)
                            {
                                reg2 |= x.lo <= k && k <= x.hi;
                            }
                        }
                        if(cvars.contains(cvar) && k != 0 && !reg2)
                        {
                            errors.push_back(std::format("{}:{}: the menu runs {} {}: not in {}", path, ln, cvar, k,
                                                         registryPath));
                        }
                    }
                }
            }
        }
    }
    for(const auto& e : errors)
    {
        out.line("test numbers: {}", e);
    }
    size_t total = 0;
    for(const auto& [_, es] : entries)
    {
        total += es.size();
    }
    size_t branchCount = 0;
    for(const auto& [_, ls] : leaves)
    {
        branchCount += ls.size();
    }
    out.line("test numbers: {} registered, {} dispatch branches, {} problems", total, branchCount, errors.size());
    return errors.empty() ? 0 : 1;
}

} // namespace qvrc
