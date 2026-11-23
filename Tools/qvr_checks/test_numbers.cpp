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
#include "maps.hpp"

#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Vocabulary/Optional.hpp"
#include "Zancle/Vocabulary/Pair.hpp"

#include <regex>

namespace qvrc
{
namespace
{

constexpr const char* registryPath = "QC/test_numbers.txt";
constexpr long long inf = 1LL << 30;

using Range = za::Pair<long long, long long>;

za::String fmtRange(long long lo, long long hi)
{
    if(hi >= inf)
    {
        return fmt("{}+", lo);
    }
    return lo == hi ? fmt("{}", lo) : fmt("{}-{}", lo, hi);
}

// "48", "20-39", "100+"
za::Optional<Range> parseNumbers(za::StringView spec)
{
    za::SizeT k = 0;
    while(k < spec.size() && isDigit(spec[k]))
    {
        ++k;
    }
    if(k == 0)
    {
        return za::nullOpt;
    }
    const long long lo = toInt(spec.substrByPosLen(0, k));
    long long hi = lo;
    const auto rest = spec.substrByPosLen(k);
    if(rest == "+")
    {
        hi = inf;
    }
    else if(!rest.empty())
    {
        if(rest[0] != '-' || !allDigits(rest.substrByPosLen(1)))
        {
            return za::nullOpt;
        }
        hi = toInt(rest.substrByPosLen(1));
    }
    if(hi < lo)
    {
        return za::nullOpt;
    }
    return za::Optional<Range>{Range{lo, hi}};
}

struct Dispatcher
{
    za::String cvar, file, func, var;
    za::Optional<Range> within;
    int line;
    bool loose;
};

struct Entry
{
    long long lo, hi;
    int line;
    bool fallthrough;
};

za::String stripComments(za::StringView text)
{
    za::String out;
    out.reserve(text.size());
    za::SizeT i = 0;
    while(i < text.size())
    {
        if(text[i] == '/' && i + 1 < text.size() && text[i + 1] == '/')
        {
            const za::SizeT j = text.find('\n', i);
            i = j == npos ? text.size() : j;
        }
        else if(text[i] == '/' && i + 1 < text.size() && text[i + 1] == '*')
        {
            const za::SizeT j = text.find("*/", i + 2);
            const za::SizeT k = j == npos ? text.size() : j + 2;
            out += blankKeepNewlines(text.substrByPosLen(i, k - i));
            i = k;
        }
        else if(text[i] == '"')
        {
            za::SizeT j = i + 1;
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
    za::Vector<Range> nums;
    za::SizeT start, end;
    int lineOffset;
};

za::StringView stripParens(za::StringView s)
{
    s = trim(s);
    while(!s.empty() && (s.front() == '(' || s.front() == ')'))
    {
        s.removePrefix(1);
    }
    while(!s.empty() && (s.back() == '(' || s.back() == ')'))
    {
        s.removeSuffix(1);
    }
    return trim(s);
}

za::Vector<za::StringView> splitOn(za::StringView s, za::StringView sep)
{
    za::Vector<za::StringView> out;
    za::SizeT a = 0;
    while(true)
    {
        const za::SizeT e = s.find(sep, a);
        if(e == npos)
        {
            out.pushBack(s.substrByPosLen(a));
            return out;
        }
        out.pushBack(s.substrByPosLen(a, e - a));
        a = e + sep.size();
    }
}

// The numbers a condition takes if it tests only `var` against numbers (an || of ==, >=, <=, ranges); nullOpt if it is
// anything else (another variable in it: a refinement, not a dispatch).
za::Optional<za::Vector<Range>> conditionNumbers(za::StringView cond, za::StringView var)
{
    // (re.sub(r"\bvar\b", "", cond), then no letter or underscore may be left)
    for(za::SizeT i = 0; i < cond.size();)
    {
        if(cond.substrByPosLen(i, var.size()) == var && wordStart(cond, i) && wordEnd(cond, i + var.size()))
        {
            i += var.size();
            continue;
        }
        const char c = cond[i];
        if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
        {
            return za::nullOpt;
        }
        ++i;
    }
    za::Vector<Range> out;
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
            if(term.substrByPosLen(0, var.size()) != var)
            {
                return za::nullOpt;
            }
            za::SizeT p = skipSpace(term, var.size());
            za::StringView op;
            for(za::StringView o : {"==", ">=", "<=", ">", "<"})
            {
                if(term.substrByPosLen(p, o.size()) == o)
                {
                    op = o;
                    break;
                }
            }
            if(op.empty())
            {
                return za::nullOpt;
            }
            p = skipSpace(term, p + op.size());
            const auto digits = term.substrByPosLen(p);
            if(!allDigits(digits))
            {
                return za::nullOpt;
            }
            const long long k = toInt(digits);
            ok = true;
            if(op == "==")
            {
                lo = za::max(lo, k);
                hi = za::min(hi, k);
            }
            else if(op == ">=")
            {
                lo = za::max(lo, k);
            }
            else if(op == ">")
            {
                lo = za::max(lo, k + 1);
            }
            else if(op == "<=")
            {
                hi = za::min(hi, k);
            }
            else
            {
                hi = za::min(hi, k - 1);
            }
        }
        if(!ok || lo > hi)
        {
            return za::nullOpt;
        }
        out.pushBack(Range{za::max(lo, 0LL), hi});
    }
    return za::Optional<za::Vector<Range>>{ZA_MOVE(out)};
}

// Each `if(...)` testing only `var`: (ranges, start, end of its block, line offset).
za::Vector<Branch> branches(za::StringView body, za::StringView var)
{
    za::Vector<Branch> found;
    for(za::SizeT i = 0; (i = body.find("if", i)) != npos; i += 2)
    {
        if(!wordStart(body, i))
        {
            continue;
        }
        const za::SizeT c0 = skipSpace(body, i + 2);
        if(c0 >= body.size() || body[c0] != '(')
        {
            continue;
        }
        const za::SizeT c1 = matchClose(body, c0, '(', ')');
        if(c1 == npos)
        {
            continue;
        }
        auto nums = conditionNumbers(body.substrByPosLen(c0 + 1, c1 - c0 - 1), var);
        if(!nums)
        {
            continue;
        }
        const za::SizeT j = skipSpace(body, c1 + 1);
        za::SizeT end;
        if(j < body.size() && body[j] == '{')
        {
            end = matchClose(body, j, '{', '}');
        }
        else
        {
            end = body.find(';', j);
        }
        const long long endPos = end == npos ? -1 : static_cast<long long>(end);
        found.pushBack({ZA_MOVE(*nums), i, static_cast<za::SizeT>(endPos), lineAt(body, i) - 1});
    }
    // (a branch inside another's block refines it: not a dispatch of its own)
    za::Vector<Branch> top;
    for(za::SizeT b = 0; b < found.size(); ++b)
    {
        bool inner = false;
        for(za::SizeT o = 0; o < found.size() && !inner; ++o)
        {
            inner = o != b && found[o].start < found[b].start &&
                    static_cast<long long>(found[b].end) <= static_cast<long long>(found[o].end);
        }
        if(!inner)
        {
            top.pushBack(found[b]);
        }
    }
    return top;
}

// Every identifier followed by `(` in body[start..end].
Set<za::String> callsIn(za::StringView body, za::SizeT start, za::SizeT end)
{
    Set<za::String> out;
    const auto s = body.substrByPosLen(start, end == static_cast<za::SizeT>(-1) ? 0 : end + 1 - start);
    for(za::SizeT i = 0; i < s.size();)
    {
        const za::SizeT n = identAt(s, i);
        if(n == 0 || !wordStart(s, i))
        {
            ++i;
            continue;
        }
        const za::SizeT p = skipSpace(s, i + n);
        if(p < s.size() && s[p] == '(')
        {
            out.emplace(s.substrByPosLen(i, n));
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
    za::String where;
    za::String looseFunc; // empty: not loose
};

// A C string literal's text at s[i] (the opening quote), escapes kept as written; i past its closing quote. nullOpt if
// s[i] is not a quote or the literal doesn't close on the line.
za::Optional<za::String> quoted(za::StringView s, za::SizeT& i)
{
    if(i >= s.size() || s[i] != '"')
    {
        return za::nullOpt;
    }
    za::String text;
    for(za::SizeT k = i + 1; k < s.size(); ++k)
    {
        if(s[k] == '\\' && k + 1 < s.size())
        {
            text += s[k];
            text += s[++k];
        }
        else if(s[k] == '"')
        {
            i = k + 1;
            return za::Optional<za::String>{ZA_MOVE(text)};
        }
        else
        {
            text += s[k];
        }
    }
    return za::nullOpt;
}

// The commands of a line's `command("<label>", "<command>"` menu rows (the second literal of each). By hand, not by
// a regex: an alternation repeated over a long literal (a 700-character row) runs VS 2022's STL out of stack
// (regex_error error_stack, make_release's build), where VS 2026's copes.
za::Vector<za::String> commandRowCommands(za::StringView line)
{
    za::Vector<za::String> cmds;
    auto skipWs = [&](za::SizeT i) {
        while(i < line.size() && (line[i] == ' ' || line[i] == '\t'))
        {
            ++i;
        }
        return i;
    };
    for(za::SizeT at = line.find("command("); at != npos; at = line.find("command(", at + 1))
    {
        // (isalnum in the "C" locale: ASCII letters and digits)
        const char before = at == 0 ? ' ' : line[at - 1];
        const bool wordStart = !((isAlnum(before) && static_cast<unsigned char>(before) < 0x80) || before == '_');
        za::SizeT i = skipWs(at + 8);
        if(!wordStart || !quoted(line, i))
        {
            continue;
        }
        i = skipWs(i);
        if(i >= line.size() || line[i] != ',')
        {
            continue;
        }
        i = skipWs(i + 1);
        if(auto cmd = quoted(line, i))
        {
            cmds.pushBack(ZA_MOVE(*cmd));
        }
    }
    return cmds;
}

} // namespace

int checkTestNumbers(const Ctx& ctx, const Args& args, Out& out)
{
    za::Vector<za::String> errors;
    za::Vector<Dispatcher> dispatchers;
    Map<za::String, za::Vector<Entry>> entries;
    bool ok = false;
    const za::String reg = readText(ctx.tree / registryPath, &ok);
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
        const auto line = trim(cut == npos ? rawLine : rawLine.substrByPosLen(0, cut));
        if(line.empty())
        {
            continue;
        }
        auto words = splitWords(line);
        if(words[0] == "dispatch")
        {
            // the options after the 5 words, the first `loose` taken out
            za::Vector<za::StringView> opts;
            bool loose = false;
            for(za::SizeT k = za::min<za::SizeT>(5, words.size()); k < words.size(); ++k)
            {
                if(!loose && words[k] == "loose")
                {
                    loose = true;
                    continue;
                }
                opts.pushBack(words[k]);
            }
            if(words.size() < 5 || (opts.size() != 0 && opts.size() != 2) || (!opts.empty() && opts[0] != "within"))
            {
                errors.pushBack(
                    fmt("{}:{}: dispatch <cvar> <file> <function> <variable> [within A-B] [loose]", registryPath, n));
                continue;
            }
            Dispatcher d{za::String{words[1]}, za::String{words[2]}, za::String{words[3]}, za::String{words[4]},
                         za::nullOpt, n, loose};
            if(!opts.empty())
            {
                d.within = parseNumbers(opts[1]);
            }
            dispatchers.pushBack(ZA_MOVE(d));
            continue;
        }
        if(words.size() < 3)
        {
            errors.pushBack(fmt("{}:{}: <cvar> <number|A-B|A+> <file> <what it tests>", registryPath, n));
            continue;
        }
        // (N*: the dispatcher's fall-through, e.g. `if(req) { ... }` past its numbered branches: no branch of its own)
        const bool fallthrough = endsWith(words[1], "*");
        auto spec = words[1];
        while(!spec.empty() && spec.back() == '*')
        {
            spec.removeSuffix(1);
        }
        const auto r = parseNumbers(spec);
        if(!r)
        {
            errors.pushBack(fmt("{}:{}: bad number '{}'", registryPath, n, words[1]));
            continue;
        }
        entries[za::String{words[0]}].pushBack({r->first, r->second, n, fallthrough});
    }

    Map<za::String, Set<za::String>> subsOf;
    for(const auto& d : dispatchers)
    {
        subsOf[d.cvar].insert(d.func);
    }
    Map<za::String, za::Vector<Leaf>> leaves;
    za::Vector<za::String> leafOrder; // (Python dicts keep insertion order)
    for(const auto& d : dispatchers)
    {
        bool fileOk = false;
        const za::String raw = readText(ctx.tree / toPath(d.file), &fileOk);
        if(!fileOk)
        {
            errors.pushBack(fmt("{}: dispatcher file {} not found", registryPath, d.file));
            continue;
        }
        const za::String text = stripComments(raw);
        // \bNAME\s*=\s*\{
        za::SizeT start = npos;
        for(za::SizeT i = 0; (i = text.find(d.func, i)) != npos; ++i)
        {
            if(!wordStart(text, i))
            {
                continue;
            }
            za::SizeT p = skipSpace(text, i + d.func.size());
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
        if(start == npos)
        {
            errors.pushBack(fmt("{}: dispatcher {} not found in {}", registryPath, d.func, d.file));
            continue;
        }
        const za::SizeT end = matchClose(text, start, '{', '}');
        // (Python's text[start:end + 1] with end = -1 is empty)
        const za::StringView body = end == npos ? za::StringView() : text.substrByPosLen(start, end + 1 - start);
        const int line0 = lineAt(text, start);
        Set<za::String> others = subsOf[d.cvar];
        others.erase(d.func);
        for(const auto& b : branches(body, d.var))
        {
            const za::String where = fmt("{}:{} ({})", d.file, line0 + b.lineOffset, d.func);
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
                    errors.pushBack(fmt("{}: {} {} outside its dispatcher's range {}", where, d.cvar, fmtRange(lo, hi),
                                        fmtRange(d.within->first, d.within->second)));
                }
                if(!leaves.contains(d.cvar))
                {
                    leafOrder.pushBack(d.cvar);
                }
                leaves[d.cvar].pushBack({lo, hi, where, d.loose ? d.func : za::String()});
            }
        }
    }
    if(has(args, "--list"))
    {
        for(const auto* e : qza::sortedByKey(leaves))
        {
            const auto& [cvar, ls] = *e;
            // (the specs ordered by their first number, then their text; each printed once)
            za::Vector<za::Pair<long long, za::String>> specs;
            for(const auto& l : ls)
            {
                specs.pushBack({l.lo, fmtRange(l.lo, l.hi)});
            }
            za::quickSort(specs.begin(), specs.end(), [](const auto& a, const auto& b) { return a < b; });
            za::String line = cvar;
            line += ':';
            Set<za::String> seenSpec;
            for(const auto& [lo, s] : specs)
            {
                if(seenSpec.insert(s).second)
                {
                    line += ' ';
                    line += s;
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
        for(za::SizeT i = 0; i < ls.size(); ++i)
        {
            for(za::SizeT j = i + 1; j < ls.size(); ++j)
            {
                if(!ls[i].looseFunc.empty() && ls[i].looseFunc == ls[j].looseFunc)
                {
                    continue;
                }
                if(overlap(ls[i].lo, ls[i].hi, ls[j].lo, ls[j].hi))
                {
                    errors.pushBack(fmt("{} test number clash: {} takes {} and {} takes {}", cvar, ls[i].where,
                                        fmtRange(ls[i].lo, ls[i].hi), ls[j].where, fmtRange(ls[j].lo, ls[j].hi)));
                }
            }
        }
    }
    // Registry clashes, coverage both ways. (Python's dict order: the cvars in their first registry line's order.)
    za::Vector<za::String> entryOrder;
    {
        za::Vector<za::Pair<int, za::String>> firsts;
        for(const auto& [cvar, es] : entries)
        {
            firsts.pushBack({es.front().line, cvar});
        }
        za::quickSort(firsts.begin(), firsts.end(), [](const auto& a, const auto& b) { return a < b; });
        for(auto& f : firsts)
        {
            entryOrder.pushBack(f.second);
        }
    }
    for(const auto& cvar : entryOrder)
    {
        const auto& es = entries[cvar];
        for(za::SizeT i = 0; i < es.size(); ++i)
        {
            for(za::SizeT j = i + 1; j < es.size(); ++j)
            {
                if(overlap(es[i].lo, es[i].hi, es[j].lo, es[j].hi))
                {
                    errors.pushBack(fmt("{}:{} and :{} both claim {} {}", registryPath, es[i].line, es[j].line, cvar,
                                        fmtRange(za::max(es[i].lo, es[j].lo), za::min(es[i].hi, es[j].hi))));
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
                errors.pushBack(fmt("{}:{}: {} {} is registered but no dispatcher runs it", registryPath, e.line, cvar,
                                    fmtRange(e.lo, e.hi)));
            }
        }
    }
    // (lo..hi within the union of the cvar's registry lines)
    const auto covered = [&](const za::String& cvar, long long lo, long long hi) {
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
                errors.pushBack(
                    fmt("{}: {} {} is not in {} (claim it there)", l.where, cvar, fmtRange(l.lo, l.hi), registryPath));
            }
        }
    }
    // The menus' commands.
    Set<za::String> cvars;
    for(const auto& [c, _] : entries)
    {
        cvars.insert(c);
    }
    for(const auto& [c, _] : leaves)
    {
        cvars.insert(c);
    }
    const std::regex testRe(R"(\b(vr_\w+_test)\s+(\d+))");
    for(const char* ext : {".inc", ".cpp", ".hpp"})
    {
        za::Vector<za::String> paths;
        for(const auto& p : listFiles(ctx.tree / "Quake/vr", {ext}, false))
        {
            paths.pushBack("Quake/vr\\" + pathStr(p.filename())); // (sorted as Python's glob paths)
        }
        sortStrings(paths);
        for(auto& path : paths)
        {
            replaceChar(path, '\\', '/');
            const za::String text = readText(ctx.tree / toPath(path));
            if(text.find("command(") == npos)
            {
                continue;
            }
            int ln = 0;
            for(auto line : splitLines(text))
            {
                ++ln;
                if(line.find("command(") == npos)
                {
                    continue;
                }
                for(const za::String& cmd : commandRowCommands(line))
                {
                    for(std::cregex_iterator t(cmd.begin(), cmd.end(), testRe), tend; t != tend; ++t)
                    {
                        const za::String cvar{za::StringView((*t)[1].first, static_cast<za::SizeT>((*t)[1].length()))};
                        const long long k =
                            toInt(za::StringView((*t)[2].first, static_cast<za::SizeT>((*t)[2].length())));
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
                            errors.pushBack(
                                fmt("{}:{}: the menu runs {} {}: not in {}", path, ln, cvar, k, registryPath));
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
    za::SizeT total = 0;
    for(const auto& [_, es] : entries)
    {
        total += es.size();
    }
    za::SizeT branchCount = 0;
    for(const auto& [_, ls] : leaves)
    {
        branchCount += ls.size();
    }
    out.line("test numbers: {} registered, {} dispatch branches, {} problems", total, branchCount, errors.size());
    return errors.empty() ? 0 : 1;
}

} // namespace qvrc
