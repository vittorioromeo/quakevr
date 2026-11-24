// qvr_checks: the build's source checks (Tools/qvr_checks/README.md). Shared helpers.
//
// Zancle instead of the standard library, as the engine (docs/vr-port/ZANCLE.md): its strings, vectors, dense maps
// and algorithms. Kept from std (Zancle has none): <filesystem> (paths, directory walking), <format> (Zancle's
// formatter isn't vendored: `fmt` and `Out::line` wrap it, za::String and za::StringView arguments included), <regex>
// in the checks that match patterns, and std::exception (main.cpp: a check's throw reported as its failure).
#pragma once

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <filesystem>
#include <format>

// std::format's arguments: Zancle's strings as string views.
template <>
struct std::formatter<za::StringView, char> : std::formatter<std::string_view, char>
{
    template <typename FormatContext>
    auto format(const za::StringView s, FormatContext& ctx) const
    {
        return std::formatter<std::string_view, char>::format(std::string_view(s.data(), s.size()), ctx);
    }
};

template <>
struct std::formatter<za::String, char> : std::formatter<std::string_view, char>
{
    template <typename FormatContext>
    auto format(const za::String& s, FormatContext& ctx) const
    {
        return std::formatter<std::string_view, char>::format(std::string_view(s.data(), s.size()), ctx);
    }
};

namespace qvrc
{

namespace fs = std::filesystem;

// "not found" (a find's result) / "to the end" (a length).
inline constexpr za::SizeT npos = za::StringView::nPos;

// An output iterator appending to a za::String (std::format_to's sink).
struct StringAppender
{
    using difference_type = za::PtrDiffT;
    za::String* s;
    StringAppender& operator=(char c)
    {
        s->pushBack(c);
        return *this;
    }
    StringAppender& operator*()
    {
        return *this;
    }
    StringAppender& operator++()
    {
        return *this;
    }
    StringAppender operator++(int)
    {
        return *this;
    }
};

template <typename... Args>
void appendFmt(za::String& s, std::format_string<Args...> f, Args&&... args)
{
    std::format_to(StringAppender{&s}, f, ZA_FORWARD(args)...);
}

template <typename... Args>
[[nodiscard]] za::String fmt(std::format_string<Args...> f, Args&&... args)
{
    za::String s;
    appendFmt(s, f, ZA_FORWARD(args)...);
    return s;
}

// One check's run: the repository's root, the QC compiler, its printed lines and its exit code.
struct Ctx
{
    fs::path tree;   // the repository's root (absolute)
    fs::path fteqcc; // the QC compiler (--fteqcc), empty when not given
};

struct Out
{
    za::String text;

    template <typename... Args>
    void line(std::format_string<Args...> f, Args&&... args)
    {
        appendFmt(text, f, ZA_FORWARD(args)...);
        text += '\n';
    }

    void raw(za::StringView s)
    {
        text += s;
        text += '\n';
    }
};

using Args = za::Vector<za::String>;

// The checks (each in its own file). `args` are the subcommand's own arguments.
int checkStatics(const Ctx& ctx, const Args& args, Out& out);
int checkSeatState(const Ctx& ctx, const Args& args, Out& out);
int checkTestNumbers(const Ctx& ctx, const Args& args, Out& out);
int checkBuiltins(const Ctx& ctx, const Args& args, Out& out);
int checkToolgunWeapons(const Ctx& ctx, const Args& args, Out& out);
int genCvarsQc(const Ctx& ctx, const Args& args, Out& out);
int checkCanonical(const Ctx& ctx, const Args& args, Out& out);
int checkReleaseQc(const Ctx& ctx, const Args& args, Out& out);
int checkQcPrecedence(const Ctx& ctx, const Args& args, Out& out);
int checkProgsFormat(const Ctx& ctx, const Args& args, Out& out);
int fgdgen(const Ctx& ctx, const Args& args, Out& out);
int checkStdUses(const Ctx& ctx, const Args& args, Out& out);

// The source with comments, strings and character literals blanked (newlines kept): statics.cpp's.
za::String stripCode(za::StringView text);

// --- files and paths

// A file's bytes as they are (`ok` false when it can't be read).
za::String readRaw(const fs::path& p, bool* ok = nullptr);
// A file's text as Python's text mode reads it: "\r\n" and "\r" become "\n".
za::String readText(const fs::path& p, bool* ok = nullptr);
bool writeRaw(const fs::path& p, za::StringView data);
// Every regular file under `dir` (recursive or not) whose extension is one of `exts` (".cpp"; empty: any).
za::Vector<fs::path> listFiles(const fs::path& dir, const za::Vector<za::StringView>& exts, bool recursive);
// `p` relative to `base`, with forward slashes.
za::String relPosix(const fs::path& p, const fs::path& base);
// A path from text, and a path's text (as path::string() / generic_string()).
inline fs::path toPath(za::StringView s)
{
    return fs::path(s.begin(), s.end());
}
inline za::String pathStr(const fs::path& p)
{
    return za::String{p.string()};
}
inline za::String genericStr(const fs::path& p)
{
    return za::String{p.generic_string()};
}
// Python's sorted() of pathlib paths on Windows: part by part, case-insensitive.
void sortPaths(za::Vector<fs::path>& v);
// Python's sorted() of str paths.
void sortStrings(za::Vector<za::String>& v);

// --- text

inline bool isWord(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
           static_cast<unsigned char>(c) >= 0x80;
}
inline bool isAlnum(char c)
{
    return isWord(c) && c != '_';
}
inline bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}
inline bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
bool allDigits(za::StringView s); // non-empty, every character a digit
// The value of a run of decimal digits (the callers check them first: std::stoll's result for those).
long long toInt(za::StringView digits);
za::StringView trim(za::StringView s);
za::Vector<za::StringView> splitLines(za::StringView s); // str.splitlines() for \n text
za::Vector<za::StringView> splitWords(za::StringView s); // str.split()
za::String blankKeepNewlines(za::StringView s);          // re.sub(r"[^\n]", " ", s)
int lineAt(za::StringView text, za::SizeT pos);          // 1-based line of a position
bool startsWith(za::StringView s, za::StringView p);
bool endsWith(za::StringView s, za::StringView p);
// \bword at pos (the char before is not a word char).
inline bool wordStart(za::StringView s, za::SizeT pos)
{
    return pos == 0 || !isWord(s[pos - 1]);
}
inline bool wordEnd(za::StringView s, za::SizeT pos)
{
    return pos >= s.size() || !isWord(s[pos]);
}
za::SizeT skipSpace(za::StringView s, za::SizeT pos);
// An identifier ([A-Za-z_]\w*) at pos: its length, 0 if none.
za::SizeT identAt(za::StringView s, za::SizeT pos);
// The `close` matching the `open` at s[i] (depth counted from i), nPos when none.
za::SizeT matchClose(za::StringView s, za::SizeT i, char open, char close);
// `s` with every '\r' left out.
za::String withoutCr(za::StringView s);
// `s` with every `from` replaced by `to`.
void replaceChar(za::String& s, char from, char to);
// Does `v` hold `x`?
template <typename V, typename T>
[[nodiscard]] bool has(const V& v, const T& x)
{
    for(const auto& e : v)
    {
        if(e == x)
        {
            return true;
        }
    }
    return false;
}

// Python's re.findall(r"^" + pattern, text, re.M) for a pattern with one group: tried at each line's start (its
// whitespace may run on over the next lines, as `\s*` does), `first` the character the match needs after the leading
// whitespace (a cheap test before the regex).
za::Vector<za::String> lineStartGroups(za::StringView text, za::StringView pattern, char first);

// --- processes

struct ProcResult
{
    int rc = -1;
    za::String out; // stdout (and stderr when merged)
    za::String err;
    bool started = false;
};
// Runs a program and waits; inherits nothing but its own pipes (safe from several threads at once).
ProcResult runProcess(const za::Vector<za::String>& args, const fs::path& cwd, bool mergeStderr);
// The program on PATH (as shutil.which), or empty.
fs::path which(za::StringView name);

// The fteqcc to run: --fteqcc, else $FTEQCC, else the given fallbacks, else fteqcc64/fteqcc on PATH.
fs::path findFteqcc(const Ctx& ctx, const za::Vector<fs::path>& fallbacks);
// QC/progs.src's entries relative to `work`, written to work/progs.src (+ fteqcc.ini): returns false on failure.
bool writeWorkProgsSrc(const Ctx& ctx, const fs::path& work);
// QC/build.sh's flags.
extern const za::Vector<za::String> qcFlags;

// --- progs.dat

struct ProgsHeader
{
    za::I32 h[15]{};
};
// "id16", "fte16", "fte32" (+ " compressed"), another description; `ok` false when it isn't a progs.dat.
za::String progsFormat(za::StringView data, ProgsHeader& hdr, bool& ok);

} // namespace qvrc
