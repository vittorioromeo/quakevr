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

#include <algorithm>
#include <array>

namespace qvrc
{
namespace
{

// The source with comments, strings and character literals blanked (newlines kept, so line numbers hold).
std::string strip(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    const size_t n = text.size();
    size_t i = 0;
    size_t lineStart = 0; // where the current line starts (for a preprocessor line's `#`)
    auto blank = [&](size_t a, size_t b) {
        for(size_t k = a; k < b; ++k)
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
            size_t j = text.find('\n', i);
            j = j == std::string_view::npos ? n : j;
            out.append(j - i, ' ');
            i = j;
        }
        else if(c == '/' && i + 1 < n && text[i + 1] == '*')
        {
            size_t j = text.find("*/", i + 2);
            j = j == std::string_view::npos ? n : j + 2;
            blank(i, j);
            for(size_t k = i; k < j; ++k)
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
            size_t d = i + 2;
            while(d < n && d - (i + 2) < 16 && std::string_view(" ()\\\t\n").find(text[d]) == std::string_view::npos)
            {
                ++d;
            }
            if(d >= n || text[d] != '(')
            {
                out += c;
                ++i;
                continue;
            }
            const std::string close = ")" + std::string(text.substr(i + 2, d - (i + 2))) + "\"";
            const size_t end = text.find(close, i);
            const size_t j = end == std::string_view::npos ? n : end + close.size();
            out += "\"\"";
            blank(i + 2, j);
            for(size_t k = i; k < j; ++k)
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
            size_t j = i + 1;
            while(j < n && text[j] != c && text[j] != '\n')
            {
                j += text[j] == '\\' ? 2 : 1;
            }
            j = std::min(j + 1, n);
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
            for(size_t k = i; k < j; ++k)
            {
                if(text[k] == '\n')
                {
                    lineStart = k + 1;
                }
            }
            i = j;
        }
        else if(c == '#' && trim(text.substr(lineStart, i - lineStart)).empty())
        {
            // a preprocessor line (and its continuations): not code for this purpose
            size_t j = i;
            size_t k;
            while(true)
            {
                k = text.find('\n', j);
                if(k == std::string_view::npos)
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
    std::string_view t;
    int line;
};

// [A-Za-z_]\w* | \d[\w.]* | :: | -> | && | \S
std::vector<Tok> tokenize(std::string_view code)
{
    std::vector<Tok> toks;
    int line = 1;
    size_t i = 0;
    const size_t n = code.size();
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
        size_t e = i + 1;
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
        toks.push_back({code.substr(i, e - i), line});
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

template <size_t N>
bool oneOf(std::string_view t, const std::array<std::string_view, N>& set)
{
    return std::find(set.begin(), set.end(), t) != set.end();
}

constexpr std::array<std::string_view, 8> fnTail = {"const", "noexcept", "override", "final",
                                                    "mutable", "volatile", "&", "&&"};
constexpr std::array<std::string_view, 4> classKeys = {"class", "struct", "union", "enum"};
constexpr std::array<std::string_view, 9> specifiers = {"static", "thread_local", "constexpr", "constinit", "const",
                                                        "inline", "volatile", "mutable", "extern"};

bool contains(const std::vector<std::string_view>& v, std::string_view t, size_t from = 0)
{
    return std::find(v.begin() + static_cast<std::ptrdiff_t>(std::min(from, v.size())), v.end(), t) != v.end();
}

// What a `{` opens, from the tokens of its statement before it: (kind, is it inside an expression).
std::pair<Kind, bool> classify(const std::vector<std::string_view>& head, Kind parent)
{
    if(head.empty())
    {
        return {parent == Kind::init ? Kind::init : parent, false}; // a block (in a function) or a nested initialiser
    }
    const std::string_view prev = head.back();
    if((head.size() == 3 && head[0] == "extern" && head[1] == "\"" && head[2] == "\"") ||
       (contains(head, "namespace") && !contains(head, "(")))
    {
        return {Kind::ns, false};
    }
    // a class definition: `struct X {`, `class X : public Y<T> {`, `enum class E : int {`, `static const struct {`
    int depth = 0;
    for(size_t k = 0; k < head.size(); ++k)
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
        else if(oneOf(x, classKeys) && depth == 0 && !contains(head, "=", k + 1) && !contains(head, "(", k + 1))
        {
            return {Kind::cls, false};
        }
    }
    if(prev == "=" || prev == "," || prev == "(" || prev == "return" || prev == "?" || prev == "[" || prev == "{")
    {
        return {Kind::init, false};
    }
    // a function's body: `) {`, `) const noexcept {`, `-> T {`, `] {` (a lambda without parameters)
    std::ptrdiff_t j = static_cast<std::ptrdiff_t>(head.size()) - 1;
    const auto arrow = std::find(head.begin(), head.end(), "->");
    if(arrow != head.end())
    {
        std::ptrdiff_t lastClose = -1;
        for(std::ptrdiff_t k = static_cast<std::ptrdiff_t>(head.size()) - 1; k >= 0; --k)
        {
            if(head[static_cast<size_t>(k)] == ")")
            {
                lastClose = k;
                break;
            }
        }
        if(arrow - head.begin() > lastClose)
        {
            j = (arrow - head.begin()) - 1;
        }
    }
    while(j >= 0 && oneOf(head[static_cast<size_t>(j)], fnTail))
    {
        --j;
    }
    if(j >= 0 && head[static_cast<size_t>(j)] == "]")
    {
        return {Kind::fn, true};
    }
    if(j >= 0 && head[static_cast<size_t>(j)] == ")")
    {
        int d = 0;
        std::ptrdiff_t k = j;
        while(k >= 0)
        {
            const auto x = head[static_cast<size_t>(k)];
            d += x == ")" ? 1 : x == "(" ? -1 : 0;
            if(d == 0)
            {
                break;
            }
            --k;
        }
        const bool lam = k > 0 && head[static_cast<size_t>(k - 1)] == "]";
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
    size_t start;
    int paren;
    bool expr;
};

bool inFunction(const std::vector<Frame>& stack)
{
    for(auto it = stack.rbegin(); it != stack.rend(); ++it)
    {
        if(it->kind != Kind::init)
        {
            return it->kind == Kind::fn;
        }
    }
    return false;
}

struct Found
{
    int line;
    std::string text;
};

std::vector<Found> scan(const fs::path& path)
{
    const std::string raw = readText(path);
    const auto lines = splitLines(raw);
    const std::string code = strip(raw);
    const auto toks = tokenize(code);
    // The scope stack: ns (namespace or file), cls, fn (a function's or lambda's body, and every block in one), init
    // (a braced initialiser: holds no declarations; a lambda in it is fn). Each entry keeps the statement and the
    // parenthesis depth it interrupted: an initialiser or a lambda is part of its enclosing statement.
    std::vector<Frame> stack{{Kind::ns, 0, 0, false}};
    std::vector<Found> found;
    size_t start = 0; // the first token of the current statement or declaration
    int paren = 0;
    std::vector<std::string_view> head;
    for(size_t i = 0; i < toks.size(); ++i)
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
            for(size_t k = start; k < i; ++k)
            {
                head.push_back(toks[k].t);
            }
            const auto [kind, expr] = classify(head, stack.back().kind);
            stack.push_back({kind, start, paren, expr});
            start = i + 1;
            paren = 0;
        }
        else if(t == "}")
        {
            if(stack.size() > 1)
            {
                const Frame f = stack.back();
                stack.pop_back();
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
            size_t j = i;
            while(j > start && oneOf(toks[j - 1].t, specifiers))
            {
                --j;
            }
            bool second = false;
            for(size_t k = j; k < i; ++k)
            {
                second |= toks[k].t == "static" || toks[k].t == "thread_local";
            }
            if(second)
            {
                continue; // the same declaration's second specifier
            }
            bool isConstexpr = false;
            bool isThreadLocal = false;
            while(j < toks.size() && oneOf(toks[j].t, specifiers))
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
            const std::string_view text = static_cast<size_t>(ln - 1) < lines.size() ? lines[ln - 1] : "";
            if(text.find("statics-ok") != std::string_view::npos)
            {
                continue;
            }
            found.push_back({ln, std::string(trim(text))});
        }
    }
    return found;
}

std::vector<fs::path> treeFiles(const fs::path& root)
{
    std::vector<fs::path> out;
    for(auto& p : listFiles(root, {".cpp", ".hpp", ".h", ".inc", ".c"}, true))
    {
        bool external = false;
        for(const auto& part : p.lexically_relative(root))
        {
            external |= part == "external";
        }
        if(!external)
        {
            out.push_back(std::move(p));
        }
    }
    sortPaths(out);
    return out;
}

} // namespace

int checkStatics(const Ctx& ctx, const Args& args, Out& out)
{
    const fs::path root = ctx.tree / "Quake" / "vr";
    std::vector<fs::path> files;
    if(args.empty())
    {
        files = treeFiles(root);
    }
    for(const auto& a : args)
    {
        std::error_code ec;
        const fs::path p = fs::absolute(a, ec).lexically_normal();
        if(fs::is_directory(p, ec))
        {
            for(auto& f : treeFiles(p))
            {
                files.push_back(std::move(f));
            }
        }
        else
        {
            files.push_back(p);
        }
    }
    std::vector<std::string> found;
    for(const auto& path : files)
    {
        const auto rel = path.lexically_relative(ctx.tree);
        const std::string name = rel.empty() || *rel.begin() == ".." ? path.generic_string() : rel.generic_string();
        for(const auto& f : scan(path))
        {
            found.push_back(std::format("{}:{}: {}", name, f.line, f.text));
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
