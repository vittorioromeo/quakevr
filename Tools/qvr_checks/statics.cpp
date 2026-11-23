// statics: fails on any function-local `static` or `thread_local` variable in Quake/vr that is not `constexpr`
// (docs/vr-port/CODE_STYLE.md, "Scratch buffers and caches").
//
// The rule: inside a function body (a lambda's too) a `static` variable must be `static constexpr`. `constexpr` is the
// compiler's own proof that the variable is constant-initialised and read-only: no hidden state, no first-call
// initialisation and no thread-safe guard (MSVC's `_Init_thread_header`/`$TSS` guard is emitted for every local static
// that is dynamically initialised or has a destructor). A `static const` is rejected too, even when it happens to be
// constant-initialised (`static const int n = 3`): from the source alone `static const T x = f();` (guarded, built on
// the first call) looks the same, so the rule is spelled `constexpr`. Everything else is state and goes where it is
// visible: the file's `mem::Scratch`/`mem::Cache` set, a named struct at file scope, or a namespace-scope `const`.
//
// A function-local `thread_local` is rejected the same way (a file-scope one is a worker's state by design:
// `vr_decals.cpp`'s RNG). Class members (`static` member functions and data) and namespace-scope `static`s are not
// function-local and are not checked.
//
// A line that must stay ends with the comment `// statics-ok: <why>`.
//
// Usage: qvr_checks statics [files or folders...]   (default: Quake/vr; exit 1 and the sites when any is found)
#include "common.hpp"

#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Vocabulary/Pair.hpp"

namespace qvrc
{
namespace
{

// The source with comments, strings and character literals blanked (newlines kept, so line numbers hold).
za::String strip(za::StringView text)
{
    za::String out;
    out.reserve(text.size());
    const za::SizeT n = text.size();
    za::SizeT i = 0;
    za::SizeT lineStart = 0; // where the current line starts (for a preprocessor line's `#`)
    auto blank = [&](za::SizeT a, za::SizeT b) {
        for(za::SizeT k = a; k < b; ++k)
        {
            out += text[k] == '\n' ? '\n' : ' ';
        }
    };
    while(i < n)
    {
        const char c = text[i];
        if(c == '\n')
        {
            lineStart = i + 1;
        }
        if(c == '/' && i + 1 < n && text[i + 1] == '/')
        {
            za::SizeT j = text.find('\n', i);
            j = j == npos ? n : j;
            out.append(j - i, ' ');
            i = j;
        }
        else if(c == '/' && i + 1 < n && text[i + 1] == '*')
        {
            za::SizeT j = text.find("*/", i + 2);
            j = j == npos ? n : j + 2;
            blank(i, j);
            for(za::SizeT k = i; k < j; ++k)
            {
                if(text[k] == '\n')
                {
                    lineStart = k + 1;
                }
            }
            i = j;
        }
        else if(c == 'R' && i + 1 < n && text[i + 1] == '"' && (i == 0 || !(isAlnum(text[i - 1]) || text[i - 1] == '_')))
        {
            // R"delim( ... )delim": the delimiter is up to 16 characters, none of ` ()\` tab newline.
            za::SizeT d = i + 2;
            while(d < n && d - (i + 2) < 16 && za::StringView(" ()\\\t\n").find(text[d]) == npos)
            {
                ++d;
            }
            if(d >= n || text[d] != '(')
            {
                out += c;
                ++i;
                continue;
            }
            za::String close = ")";
            close += text.substrByPosLen(i + 2, d - (i + 2));
            close += '"';
            const za::SizeT end = text.find(close, i);
            const za::SizeT j = end == npos ? n : end + close.size();
            out += "\"\"";
            blank(i + 2, j);
            for(za::SizeT k = i; k < j; ++k)
            {
                if(text[k] == '\n')
                {
                    lineStart = k + 1;
                }
            }
            i = j;
        }
        else if(c == '"' || c == '\'')
        {
            if(c == '\'' && i > 0 && isAlnum(text[i - 1])) // digit separator: 1'000
            {
                out += ' ';
                ++i;
                continue;
            }
            za::SizeT j = i + 1;
            while(j < n && text[j] != c && text[j] != '\n')
            {
                j += text[j] == '\\' ? 2 : 1;
            }
            j = za::min(j + 1, n);
            if(j - i >= 2)
            {
                out += c;
                out.append(j - i - 2, ' ');
                out += c;
            }
            else
            {
                out += c;
            }
            for(za::SizeT k = i; k < j; ++k)
            {
                if(text[k] == '\n')
                {
                    lineStart = k + 1;
                }
            }
            i = j;
        }
        else if(c == '#' && trim(text.substrByPosLen(lineStart, i - lineStart)).empty())
        {
            // a preprocessor line (and its continuations): not code for this purpose
            za::SizeT j = i;
            za::SizeT k;
            while(true)
            {
                k = text.find('\n', j);
                if(k == npos)
                {
                    k = n;
                    break;
                }
                if(text[k - 1] != '\\')
                {
                    break;
                }
                j = k + 1;
            }
            blank(i, k);
            i = k;
        }
        else
        {
            out += c;
            ++i;
        }
    }
    return out;
}

struct Tok
{
    za::StringView t;
    int line;
};

// [A-Za-z_]\w* | \d[\w.]* | :: | -> | && | \S
za::Vector<Tok> tokenize(za::StringView code)
{
    za::Vector<Tok> toks;
    int line = 1;
    za::SizeT i = 0;
    const za::SizeT n = code.size();
    while(i < n)
    {
        const char c = code[i];
        if(c == '\n')
        {
            ++line;
            ++i;
            continue;
        }
        if(isSpace(c))
        {
            ++i;
            continue;
        }
        za::SizeT e = i + 1;
        if(isWord(c) && !isDigit(c))
        {
            while(e < n && isWord(code[e]))
            {
                ++e;
            }
        }
        else if(isDigit(c))
        {
            while(e < n && (isWord(code[e]) || code[e] == '.'))
            {
                ++e;
            }
        }
        else if(i + 1 < n && ((c == ':' && code[i + 1] == ':') || (c == '-' && code[i + 1] == '>') ||
                              (c == '&' && code[i + 1] == '&')))
        {
            e = i + 2;
        }
        toks.pushBack({code.substrByPosLen(i, e - i), line});
        i = e;
    }
    return toks;
}

enum class Kind
{
    ns,
    cls,
    fn,
    init
};

constexpr za::Array<za::StringView, 8> fnTail = {"const", "noexcept", "override", "final",
                                                 "mutable", "volatile", "&", "&&"};
constexpr za::Array<za::StringView, 4> classKeys = {"class", "struct", "union", "enum"};
constexpr za::Array<za::StringView, 9> specifiers = {"static", "thread_local", "constexpr", "constinit", "const",
                                                     "inline", "volatile", "mutable", "extern"};

bool contains(const za::Vector<za::StringView>& v, za::StringView t, za::SizeT from = 0)
{
    for(za::SizeT k = from; k < v.size(); ++k)
    {
        if(v[k] == t)
        {
            return true;
        }
    }
    return false;
}

// What a `{` opens, from the tokens of its statement before it: (kind, is it inside an expression).
za::Pair<Kind, bool> classify(const za::Vector<za::StringView>& head, Kind parent)
{
    if(head.empty())
    {
        return {parent == Kind::init ? Kind::init : parent, false}; // a block (in a function) or a nested initialiser
    }
    const za::StringView prev = head.back();
    if((head.size() == 3 && head[0] == "extern" && head[1] == "\"" && head[2] == "\"") ||
       (contains(head, "namespace") && !contains(head, "(")))
    {
        return {Kind::ns, false};
    }
    // a class definition: `struct X {`, `class X : public Y<T> {`, `enum class E : int {`, `static const struct {`
    int depth = 0;
    for(za::SizeT k = 0; k < head.size(); ++k)
    {
        const auto x = head[k];
        if(x == "(" || x == "[" || x == "<")
        {
            ++depth;
        }
        else if(x == ")" || x == "]" || x == ">")
        {
            --depth;
        }
        else if(has(classKeys, x) && depth == 0 && !contains(head, "=", k + 1) && !contains(head, "(", k + 1))
        {
            return {Kind::cls, false};
        }
    }
    if(prev == "=" || prev == "," || prev == "(" || prev == "return" || prev == "?" || prev == "[" || prev == "{")
    {
        return {Kind::init, false};
    }
    // a function's body: `) {`, `) const noexcept {`, `-> T {`, `] {` (a lambda without parameters)
    const auto size = static_cast<za::PtrDiffT>(head.size());
    za::PtrDiffT j = size - 1;
    za::PtrDiffT arrow = size;
    for(za::PtrDiffT k = 0; k < size; ++k)
    {
        if(head[static_cast<za::SizeT>(k)] == "->")
        {
            arrow = k;
            break;
        }
    }
    if(arrow != size)
    {
        za::PtrDiffT lastClose = -1;
        for(za::PtrDiffT k = size - 1; k >= 0; --k)
        {
            if(head[static_cast<za::SizeT>(k)] == ")")
            {
                lastClose = k;
                break;
            }
        }
        if(arrow > lastClose)
        {
            j = arrow - 1;
        }
    }
    while(j >= 0 && has(fnTail, head[static_cast<za::SizeT>(j)]))
    {
        --j;
    }
    if(j >= 0 && head[static_cast<za::SizeT>(j)] == "]")
    {
        return {Kind::fn, true};
    }
    if(j >= 0 && head[static_cast<za::SizeT>(j)] == ")")
    {
        int d = 0;
        za::PtrDiffT k = j;
        while(k >= 0)
        {
            const auto x = head[static_cast<za::SizeT>(k)];
            d += x == ")" ? 1 : x == "(" ? -1 : 0;
            if(d == 0)
            {
                break;
            }
            --k;
        }
        const bool lam = k > 0 && head[static_cast<za::SizeT>(k - 1)] == "]";
        return {Kind::fn, lam};
    }
    if(prev == "else" || prev == "do" || prev == "try" || prev == "}")
    {
        return {Kind::fn, false}; // `} {`: a constructor's body after a braced member initialiser
    }
    return {Kind::init, false}; // `T name{...}`, `return T{...}`: a braced initialiser
}

struct Frame
{
    Kind kind;
    za::SizeT start;
    int paren;
    bool expr;
};

bool inFunction(const za::Vector<Frame>& stack)
{
    for(za::SizeT k = stack.size(); k-- > 0;)
    {
        if(stack[k].kind != Kind::init)
        {
            return stack[k].kind == Kind::fn;
        }
    }
    return false;
}

struct Found
{
    int line;
    za::String text;
};

za::Vector<Found> scan(const fs::path& path)
{
    const za::String raw = readText(path);
    const auto lines = splitLines(raw);
    const za::String code = strip(raw);
    const auto toks = tokenize(code);
    // The scope stack: ns (namespace or file), cls, fn (a function's or lambda's body, and every block in one), init
    // (a braced initialiser: holds no declarations; a lambda in it is fn). Each entry keeps the statement and the
    // parenthesis depth it interrupted: an initialiser or a lambda is part of its enclosing statement.
    za::Vector<Frame> stack;
    stack.pushBack({Kind::ns, 0, 0, false});
    za::Vector<Found> found;
    za::SizeT start = 0; // the first token of the current statement or declaration
    int paren = 0;
    za::Vector<za::StringView> head;
    for(za::SizeT i = 0; i < toks.size(); ++i)
    {
        const auto t = toks[i].t;
        if(t == "(" || t == "[")
        {
            ++paren;
        }
        else if(t == ")" || t == "]")
        {
            --paren;
        }
        else if(t == "{")
        {
            head.clear();
            for(za::SizeT k = start; k < i; ++k)
            {
                head.pushBack(toks[k].t);
            }
            const auto [kind, expr] = classify(head, stack.back().kind);
            stack.pushBack({kind, start, paren, expr});
            start = i + 1;
            paren = 0;
        }
        else if(t == "}")
        {
            if(stack.size() > 1)
            {
                const Frame f = stack.back();
                stack.popBack();
                if(f.kind == Kind::init || f.expr)
                {
                    start = f.start; // the statement goes on: `T x{...};`, `f([] {...});`
                    paren = f.paren;
                }
                else
                {
                    start = i + 1;
                    paren = 0;
                }
            }
        }
        else if(t == ";" && paren == 0)
        {
            start = i + 1;
        }
        else if(t == ":" && i > 0 &&
                (toks[i - 1].t == "public" || toks[i - 1].t == "private" || toks[i - 1].t == "protected" ||
                 toks[i - 1].t == "default"))
        {
            start = i + 1;
        }
        else if((t == "static" || t == "thread_local") && inFunction(stack))
        {
            // Anywhere in a function's body, parentheses included: `if(static bool once = false; !once)`. The
            // declaration's specifiers are the run of them around this one (counted once, at its first).
            za::SizeT j = i;
            while(j > start && has(specifiers, toks[j - 1].t))
            {
                --j;
            }
            bool second = false;
            for(za::SizeT k = j; k < i; ++k)
            {
                second |= toks[k].t == "static" || toks[k].t == "thread_local";
            }
            if(second)
            {
                continue; // the same declaration's second specifier
            }
            bool isConstexpr = false;
            bool isThreadLocal = false;
            while(j < toks.size() && has(specifiers, toks[j].t))
            {
                isConstexpr |= toks[j].t == "constexpr";
                isThreadLocal |= toks[j].t == "thread_local";
                ++j;
            }
            if(isConstexpr && !isThreadLocal)
            {
                continue;
            }
            const int ln = toks[i].line;
            const za::StringView text = static_cast<za::SizeT>(ln - 1) < lines.size() ? lines[ln - 1] : "";
            if(text.find("statics-ok") != npos)
            {
                continue;
            }
            found.pushBack({ln, za::String{trim(text)}});
        }
    }
    return found;
}

za::Vector<fs::path> treeFiles(const fs::path& root)
{
    za::Vector<fs::path> out;
    for(auto& p : listFiles(root, {".cpp", ".hpp", ".h", ".inc", ".c"}, true))
    {
        bool external = false;
        for(const auto& part : p.lexically_relative(root))
        {
            external |= part == "external";
        }
        if(!external)
        {
            out.pushBack(ZA_MOVE(p));
        }
    }
    sortPaths(out);
    return out;
}

} // namespace

za::String stripCode(za::StringView text)
{
    return strip(text);
}

int checkStatics(const Ctx& ctx, const Args& args, Out& out)
{
    const fs::path root = ctx.tree / "Quake" / "vr";
    za::Vector<fs::path> files;
    if(args.empty())
    {
        files = treeFiles(root);
    }
    for(const auto& a : args)
    {
        std::error_code ec;
        const fs::path p = fs::absolute(toPath(a), ec).lexically_normal();
        if(fs::is_directory(p, ec))
        {
            for(auto& f : treeFiles(p))
            {
                files.pushBack(ZA_MOVE(f));
            }
        }
        else
        {
            files.pushBack(p);
        }
    }
    za::Vector<za::String> found;
    for(const auto& path : files)
    {
        const auto rel = path.lexically_relative(ctx.tree);
        const za::String name = rel.empty() || *rel.begin() == ".." ? genericStr(path) : genericStr(rel);
        for(const auto& f : scan(path))
        {
            found.pushBack(fmt("{}:{}: {}", name, f.line, f.text));
        }
    }
    for(const auto& f : found)
    {
        out.raw(f);
    }
    if(!found.empty())
    {
        out.line("{} function-local static/thread_local that is not constexpr (see docs/vr-port/CODE_STYLE.md, "
                 "'Scratch buffers and caches')",
                 found.size());
        return 1;
    }
    return 0;
}

} // namespace qvrc
