// qvr_checks: shared helpers (files, text, processes, progs.dat).
#include "common.hpp"

#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Vocabulary/Pair.hpp"

#include <cstdio>
#include <cstdlib>
#include <regex>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace qvrc
{

za::String readRaw(const fs::path& p, bool* ok)
{
    FILE* f = _wfopen(p.c_str(), L"rb");
    if(!f)
    {
        if(ok)
        {
            *ok = false;
        }
        return {};
    }
    za::String s;
    _fseeki64(f, 0, SEEK_END);
    const long long size = _ftelli64(f);
    _fseeki64(f, 0, SEEK_SET);
    if(size > 0)
    {
        s.resize(static_cast<za::SizeT>(size));
        s.resize(fread(s.data(), 1, s.size(), f));
    }
    fclose(f);
    if(ok)
    {
        *ok = true;
    }
    return s;
}

za::String readText(const fs::path& p, bool* ok)
{
    za::String s = readRaw(p, ok);
    if(s.find('\r') == npos)
    {
        return s;
    }
    za::String o;
    o.reserve(s.size());
    for(za::SizeT i = 0; i < s.size(); ++i)
    {
        if(s[i] == '\r')
        {
            o += '\n';
            if(i + 1 < s.size() && s[i + 1] == '\n')
            {
                ++i;
            }
        }
        else
        {
            o += s[i];
        }
    }
    return o;
}

bool writeRaw(const fs::path& p, za::StringView data)
{
    FILE* f = _wfopen(p.c_str(), L"wb");
    if(!f)
    {
        return false;
    }
    const bool wrote = fwrite(data.data(), 1, data.size(), f) == data.size();
    return fclose(f) == 0 && wrote;
}

za::Vector<fs::path> listFiles(const fs::path& dir, const za::Vector<za::StringView>& exts, bool recursive)
{
    za::Vector<fs::path> out;
    std::error_code ec;
    auto want = [&](const fs::directory_entry& e) {
        if(!e.is_regular_file(ec))
        {
            return false;
        }
        if(exts.empty())
        {
            return true;
        }
        return has(exts, za::StringView{pathStr(e.path().extension())});
    };
    if(recursive)
    {
        for(fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        {
            if(want(*it))
            {
                out.pushBack(it->path());
            }
        }
    }
    else
    {
        for(fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        {
            if(want(*it))
            {
                out.pushBack(it->path());
            }
        }
    }
    return out;
}

za::String relPosix(const fs::path& p, const fs::path& base)
{
    return genericStr(p.lexically_normal().lexically_relative(base.lexically_normal()));
}

namespace
{

za::Vector<za::String> lowerParts(const fs::path& p)
{
    za::Vector<za::String> parts;
    for(const auto& part : p)
    {
        za::String s = pathStr(part);
        if(s == "\\" || s == "/")
        {
            continue;
        }
        for(char& c : s)
        {
            if(c >= 'A' && c <= 'Z')
            {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        parts.pushBack(ZA_MOVE(s));
    }
    return parts;
}

} // namespace

void sortPaths(za::Vector<fs::path>& v)
{
    za::Vector<za::Pair<za::Vector<za::String>, fs::path>> keyed;
    keyed.reserve(v.size());
    for(auto& p : v)
    {
        keyed.emplaceBack(lowerParts(p), ZA_MOVE(p));
    }
    za::stableSort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    v.clear();
    for(auto& k : keyed)
    {
        v.pushBack(ZA_MOVE(k.second));
    }
}

void sortStrings(za::Vector<za::String>& v)
{
    za::stableSort(v.begin(), v.end(), [](const za::String& a, const za::String& b) { return a < b; });
}

bool allDigits(za::StringView s)
{
    if(s.empty())
    {
        return false;
    }
    for(const char c : s)
    {
        if(!isDigit(c))
        {
            return false;
        }
    }
    return true;
}

long long toInt(za::StringView digits)
{
    long long v = 0;
    for(const char c : digits)
    {
        if(!isDigit(c))
        {
            break;
        }
        v = v * 10 + (c - '0');
    }
    return v;
}

za::StringView trim(za::StringView s)
{
    za::SizeT a = 0;
    za::SizeT b = s.size();
    while(a < b && isSpace(s[a]))
    {
        ++a;
    }
    while(b > a && isSpace(s[b - 1]))
    {
        --b;
    }
    return s.substrByPosLen(a, b - a);
}

za::Vector<za::StringView> splitLines(za::StringView s)
{
    za::Vector<za::StringView> out;
    za::SizeT a = 0;
    while(a < s.size())
    {
        const za::SizeT e = s.find('\n', a);
        if(e == npos)
        {
            out.pushBack(s.substrByPosLen(a));
            break;
        }
        out.pushBack(s.substrByPosLen(a, e - a));
        a = e + 1;
    }
    return out;
}

za::Vector<za::StringView> splitWords(za::StringView s)
{
    za::Vector<za::StringView> out;
    za::SizeT i = 0;
    while(i < s.size())
    {
        while(i < s.size() && isSpace(s[i]))
        {
            ++i;
        }
        const za::SizeT a = i;
        while(i < s.size() && !isSpace(s[i]))
        {
            ++i;
        }
        if(i > a)
        {
            out.pushBack(s.substrByPosLen(a, i - a));
        }
    }
    return out;
}

za::String blankKeepNewlines(za::StringView s)
{
    za::String o{s};
    for(char& c : o)
    {
        if(c != '\n')
        {
            c = ' ';
        }
    }
    return o;
}

int lineAt(za::StringView text, za::SizeT pos)
{
    int n = 1;
    for(za::SizeT i = 0; i < pos; ++i)
    {
        n += text[i] == '\n';
    }
    return n;
}

bool startsWith(za::StringView s, za::StringView p)
{
    return s.startsWith(p);
}

bool endsWith(za::StringView s, za::StringView p)
{
    return s.endsWith(p);
}

za::SizeT skipSpace(za::StringView s, za::SizeT pos)
{
    while(pos < s.size() && isSpace(s[pos]))
    {
        ++pos;
    }
    return pos;
}

za::SizeT identAt(za::StringView s, za::SizeT pos)
{
    if(pos >= s.size() || isDigit(s[pos]) || !isWord(s[pos]))
    {
        return 0;
    }
    za::SizeT e = pos + 1;
    while(e < s.size() && isWord(s[e]))
    {
        ++e;
    }
    return e - pos;
}

za::SizeT matchClose(za::StringView s, za::SizeT i, char open, char close)
{
    int depth = 0;
    for(za::SizeT j = i; j < s.size(); ++j)
    {
        if(s[j] == open)
        {
            ++depth;
        }
        else if(s[j] == close)
        {
            if(--depth == 0)
            {
                return j;
            }
        }
    }
    return npos;
}

za::String withoutCr(za::StringView s)
{
    za::String o;
    o.reserve(s.size());
    for(const char c : s)
    {
        if(c != '\r')
        {
            o += c;
        }
    }
    return o;
}

void replaceChar(za::String& s, char from, char to)
{
    for(char& c : s)
    {
        if(c == from)
        {
            c = to;
        }
    }
}

za::Vector<za::String> lineStartGroups(za::StringView text, za::StringView pattern, char first)
{
    za::Vector<za::String> out;
    const std::regex re(pattern.data(), pattern.size());
    za::SizeT next = 0; // (finditer: no match starts inside the last one)
    for(za::SizeT ls = 0; ls < text.size();)
    {
        if(ls >= next)
        {
            const za::SizeT p = skipSpace(text, ls);
            if(p < text.size() && text[p] == first)
            {
                std::cmatch m;
                if(std::regex_search(text.data() + ls, text.data() + text.size(), m, re,
                                     std::regex_constants::match_continuous))
                {
                    out.emplaceBack(m[1].first, static_cast<za::SizeT>(m[1].length()));
                    next = ls + za::max<za::SizeT>(1, static_cast<za::SizeT>(m.length(0)));
                }
            }
        }
        const za::SizeT nl = text.find('\n', ls);
        if(nl == npos)
        {
            break;
        }
        ls = nl + 1;
    }
    return out;
}

// --- processes

namespace
{

// UTF-8 to UTF-16 (no terminator).
za::Vector<wchar_t> widen(za::StringView s)
{
    za::Vector<wchar_t> w;
    if(s.empty())
    {
        return w;
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    w.resize(static_cast<za::SizeT>(n));
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

void appendRepeated(za::Vector<wchar_t>& cmd, za::SizeT count, wchar_t c)
{
    for(za::SizeT k = 0; k < count; ++k)
    {
        cmd.pushBack(c);
    }
}

// CommandLineToArgvW's quoting.
void appendQuoted(za::Vector<wchar_t>& cmd, const za::Vector<wchar_t>& arg)
{
    bool plain = !arg.empty();
    for(const wchar_t c : arg)
    {
        plain = plain && c != L' ' && c != L'\t' && c != L'\n' && c != L'\v' && c != L'"';
    }
    if(plain)
    {
        for(const wchar_t c : arg)
        {
            cmd.pushBack(c);
        }
        return;
    }
    cmd.pushBack(L'"');
    for(za::SizeT i = 0;; ++i)
    {
        za::SizeT backslashes = 0;
        while(i < arg.size() && arg[i] == L'\\')
        {
            ++i;
            ++backslashes;
        }
        if(i == arg.size())
        {
            appendRepeated(cmd, backslashes * 2, L'\\');
            break;
        }
        if(arg[i] == L'"')
        {
            appendRepeated(cmd, backslashes * 2 + 1, L'\\');
        }
        else
        {
            appendRepeated(cmd, backslashes, L'\\');
        }
        cmd.pushBack(arg[i]);
    }
    cmd.pushBack(L'"');
}

void drain(HANDLE h, za::String& into)
{
    char buf[1 << 16];
    DWORD got = 0;
    while(ReadFile(h, buf, sizeof(buf), &got, nullptr) && got > 0)
    {
        into.append(buf, got);
    }
}

} // namespace

ProcResult runProcess(const za::Vector<za::String>& args, const fs::path& cwd, bool mergeStderr)
{
    ProcResult r;
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE outRead = nullptr;
    HANDLE outWrite = nullptr;
    HANDLE errRead = nullptr;
    HANDLE errWrite = nullptr;
    if(!CreatePipe(&outRead, &outWrite, &sa, 0))
    {
        return r;
    }
    SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
    if(!mergeStderr)
    {
        CreatePipe(&errRead, &errWrite, &sa, 0);
        SetHandleInformation(errRead, HANDLE_FLAG_INHERIT, 0);
    }
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    // Only these handles go to the child: another thread's pipes must not (their reader would wait for this child).
    HANDLE inherit[3] = {nul, outWrite, mergeStderr ? outWrite : errWrite};
    const DWORD inheritCount = mergeStderr ? 2 : 3;
    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    za::Vector<char> attrBuf;
    attrBuf.resize(attrSize);
    auto* attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrBuf.data());
    InitializeProcThreadAttributeList(attrs, 1, 0, &attrSize);
    UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, inheritCount * sizeof(HANDLE),
                              nullptr, nullptr);

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = nul;
    si.StartupInfo.hStdOutput = outWrite;
    si.StartupInfo.hStdError = mergeStderr ? outWrite : errWrite;
    si.lpAttributeList = attrs;

    za::Vector<wchar_t> cmd;
    for(za::SizeT i = 0; i < args.size(); ++i)
    {
        if(i)
        {
            cmd.pushBack(L' ');
        }
        appendQuoted(cmd, widen(args[i]));
    }
    cmd.pushBack(L'\0');
    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                   EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, nullptr,
                                   cwd.empty() ? nullptr : cwd.c_str(), &si.StartupInfo, &pi);
    DeleteProcThreadAttributeList(attrs);
    CloseHandle(outWrite);
    if(errWrite)
    {
        CloseHandle(errWrite);
    }
    CloseHandle(nul);
    if(!ok)
    {
        CloseHandle(outRead);
        if(errRead)
        {
            CloseHandle(errRead);
        }
        return r;
    }
    r.started = true;
    za::Thread errThread;
    if(errRead)
    {
        errThread = za::Thread([&] { drain(errRead, r.err); });
    }
    drain(outRead, r.out);
    if(errThread.joinable())
    {
        errThread.join();
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    r.rc = static_cast<int>(code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(outRead);
    if(errRead)
    {
        CloseHandle(errRead);
    }
    return r;
}

fs::path which(za::StringView name)
{
    wchar_t buf[MAX_PATH * 4];
    za::Vector<wchar_t> w = widen(name);
    w.pushBack(L'\0');
    for(const wchar_t* ext : {L".exe", L".com", L".bat", L".cmd", L""})
    {
        if(SearchPathW(nullptr, w.data(), ext, static_cast<DWORD>(sizeof(buf) / sizeof(buf[0])), buf, nullptr))
        {
            return fs::path(buf);
        }
    }
    return {};
}

fs::path findFteqcc(const Ctx& ctx, const za::Vector<fs::path>& fallbacks)
{
    za::Vector<za::String> cands;
    if(!ctx.fteqcc.empty())
    {
        cands.pushBack(pathStr(ctx.fteqcc));
    }
    if(const char* e = getenv("FTEQCC"); e && *e)
    {
        cands.emplaceBack(e);
    }
    for(const auto& f : fallbacks)
    {
        cands.pushBack(pathStr(f));
    }
    cands.emplaceBack("fteqcc64");
    cands.emplaceBack("fteqcc");
    for(const auto& c : cands)
    {
        std::error_code ec;
        const fs::path cp = toPath(c);
        if(fs::is_regular_file(cp, ec))
        {
            return fs::absolute(cp, ec);
        }
        if(auto w = which(c); !w.empty())
        {
            return w;
        }
    }
    return {};
}

const za::Vector<za::String> qcFlags = {"-O3", "-Fautoproto", "-Olo", "-Fiffloat", "-Fifvector", "-Fvectorlogic",
                                        "-Flo", "-Fsubscope", "-Wall", "-Wextra", "-Wno-F209", "-Wno-F208"};

bool writeWorkProgsSrc(const Ctx& ctx, const fs::path& work)
{
    std::error_code ec;
    fs::create_directories(work, ec);
    const fs::path qc = ctx.tree / "QC";
    bool ok = false;
    const za::String src = readText(qc / "progs.src", &ok);
    if(!ok)
    {
        return false;
    }
    za::Vector<za::StringView> entries;
    for(auto l : splitLines(src))
    {
        const auto cut = l.find("//");
        const auto e = trim(cut == npos ? l : l.substrByPosLen(0, cut));
        if(!e.empty())
        {
            entries.pushBack(e);
        }
    }
    // Relative paths: fteqcc's progs.src reader stops at a drive's "C:". (The first entry is the output file.)
    za::String out = "progs.dat\n";
    for(za::SizeT i = 1; i < entries.size(); ++i)
    {
        out += relPosix(fs::absolute(qc / toPath(entries[i])), fs::absolute(work));
        out += '\n';
    }
    if(!writeRaw(work / "progs.src", out))
    {
        return false;
    }
    if(fs::is_regular_file(qc / "fteqcc.ini", ec))
    {
        fs::copy_file(qc / "fteqcc.ini", work / "fteqcc.ini", fs::copy_options::overwrite_existing, ec);
    }
    return true;
}

// --- progs.dat

namespace
{

constexpr za::U32 fourcc(const char* s)
{
    return static_cast<za::U32>(static_cast<unsigned char>(s[0])) |
           static_cast<za::U32>(static_cast<unsigned char>(s[1])) << 8 |
           static_cast<za::U32>(static_cast<unsigned char>(s[2])) << 16 |
           static_cast<za::U32>(static_cast<unsigned char>(s[3])) << 24;
}

constexpr za::U32 secondary16 = fourcc("1FTE") ^ fourcc("PROG");
constexpr za::U32 secondary32 = fourcc("1FTE") ^ fourcc("32B ");

} // namespace

za::String progsFormat(za::StringView data, ProgsHeader& hdr, bool& ok)
{
    ok = false;
    if(data.size() < 60)
    {
        return "not a progs.dat (too short)";
    }
    ZA_MEMCPY(hdr.h, data.data(), 60);
    ok = true;
    if(hdr.h[0] == 6)
    {
        return "id16";
    }
    if(hdr.h[0] == 7 && data.size() >= 92)
    {
        za::I32 ext[8];
        ZA_MEMCPY(ext, data.data() + 60, 32);
        const auto secondary = static_cast<za::U32>(ext[7]);
        za::String kind = secondary == secondary16   ? za::String{"fte16"}
                          : secondary == secondary32 ? za::String{"fte32"}
                                                     : fmt("version 7 kind {:08x}", secondary);
        if(ext[6])
        {
            kind += " compressed";
        }
        return kind;
    }
    return fmt("version {}", hdr.h[0]);
}

} // namespace qvrc
