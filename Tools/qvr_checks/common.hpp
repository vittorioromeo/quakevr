// qvr_checks: the build's source checks (Tools/qvr_checks/README.md). Shared helpers.
#pragma once

#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace qvrc
{

namespace fs = std::filesystem;

// One check's run: the repository's root, the QC compiler, its printed lines and its exit code.
struct Ctx
{
    fs::path tree;   // the repository's root (absolute)
    fs::path fteqcc; // the QC compiler (--fteqcc), empty when not given
};

struct Out
{
    std::string text;

    template <typename... Args>
    void line(std::format_string<Args...> f, Args&&... args)
    {
        text += std::format(f, std::forward<Args>(args)...);
        text += '\n';
    }

    void raw(std::string_view s)
    {
        text += s;
        text += '\n';
    }
};

using Args = std::vector<std::string>;

// The checks (each in its own file). `args` are the subcommand's own arguments.
int checkStatics(const Ctx& ctx, const Args& args, Out& out);
int checkTestNumbers(const Ctx& ctx, const Args& args, Out& out);
int checkBuiltins(const Ctx& ctx, const Args& args, Out& out);
int genCvarsQc(const Ctx& ctx, const Args& args, Out& out);
int checkCanonical(const Ctx& ctx, const Args& args, Out& out);
int checkReleaseQc(const Ctx& ctx, const Args& args, Out& out);
int checkQcPrecedence(const Ctx& ctx, const Args& args, Out& out);
int checkProgsFormat(const Ctx& ctx, const Args& args, Out& out);
int fgdgen(const Ctx& ctx, const Args& args, Out& out);

// --- files

// A file's bytes as they are (`ok` false when it can't be read).
std::string readRaw(const fs::path& p, bool* ok = nullptr);
// A file's text as Python's text mode reads it: "\r\n" and "\r" become "\n".
std::string readText(const fs::path& p, bool* ok = nullptr);
bool writeRaw(const fs::path& p, std::string_view data);
// Every regular file under `dir` (recursive or not) whose extension is one of `exts` (".cpp"; empty: any).
std::vector<fs::path> listFiles(const fs::path& dir, const std::vector<std::string>& exts, bool recursive);
// `p` relative to `base`, with forward slashes.
std::string relPosix(const fs::path& p, const fs::path& base);
// Python's sorted() of pathlib paths on Windows: part by part, case-insensitive.
void sortPaths(std::vector<fs::path>& v);
// Python's sorted() of str paths.
void sortStrings(std::vector<std::string>& v);

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
std::string_view trim(std::string_view s);
std::vector<std::string_view> splitLines(std::string_view s); // str.splitlines() for \n text
std::vector<std::string_view> splitWords(std::string_view s); // str.split()
std::string blankKeepNewlines(std::string_view s);            // re.sub(r"[^\n]", " ", s)
int lineAt(std::string_view text, size_t pos);                 // 1-based line of a position
bool startsWith(std::string_view s, std::string_view p);
bool endsWith(std::string_view s, std::string_view p);
// \bword at pos (the char before is not a word char).
inline bool wordStart(std::string_view s, size_t pos)
{
    return pos == 0 || !isWord(s[pos - 1]);
}
inline bool wordEnd(std::string_view s, size_t pos)
{
    return pos >= s.size() || !isWord(s[pos]);
}
size_t skipSpace(std::string_view s, size_t pos);
// An identifier ([A-Za-z_]\w*) at pos: its length, 0 if none.
size_t identAt(std::string_view s, size_t pos);
// The `close` matching the `open` at s[i] (depth counted from i), npos when none.
size_t matchClose(std::string_view s, size_t i, char open, char close);

// Python's re.findall(r"^" + pattern, text, re.M) for a pattern with one group: tried at each line's start (its
// whitespace may run on over the next lines, as `\s*` does), `first` the character the match needs after the leading
// whitespace (a cheap test before the regex).
std::vector<std::string> lineStartGroups(const std::string& text, const std::string& pattern, char first);

// --- processes

struct ProcResult
{
    int rc = -1;
    std::string out; // stdout (and stderr when merged)
    std::string err;
    bool started = false;
};
// Runs a program and waits; inherits nothing but its own pipes (safe from several threads at once).
ProcResult runProcess(const std::vector<std::string>& args, const fs::path& cwd, bool mergeStderr);
// The program on PATH (as shutil.which), or empty.
fs::path which(const std::string& name);

// The fteqcc to run: --fteqcc, else $FTEQCC, else the given fallbacks, else fteqcc64/fteqcc on PATH.
fs::path findFteqcc(const Ctx& ctx, const std::vector<fs::path>& fallbacks);
// QC/progs.src's entries relative to `work`, written to work/progs.src (+ fteqcc.ini): returns false on failure.
bool writeWorkProgsSrc(const Ctx& ctx, const fs::path& work);
// QC/build.sh's flags.
extern const std::vector<std::string> qcFlags;

// --- progs.dat

struct ProgsHeader
{
    int32_t h[15]{};
};
// "id16", "fte16", "fte32" (+ " compressed"), another description; `ok` false when it isn't a progs.dat.
std::string progsFormat(std::string_view data, ProgsHeader& hdr, bool& ok);

} // namespace qvrc
