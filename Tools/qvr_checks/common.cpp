// qvr_checks: shared helpers (files, text, processes, progs.dat).
#include "common.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <regex>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace qvrc
{

std::string readRaw(const fs::path& p, bool* ok)
{
    std::ifstream f(p, std::ios::binary);
    if(!f)
    {
        if(ok)
        {
            *ok = false;
        }
        return {};
    }
    std::string s;
    f.seekg(0, std::ios::end);
    const auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    s.resize(static_cast<size_t>(size));
    f.read(s.data(), size);
    if(ok)
    {
        *ok = true;
    }
    return s;
}

std::string readText(const fs::path& p, bool* ok)
{
    std::string s = readRaw(p, ok);
    if(s.find('\r') == std::string::npos)
    {
        return s;
    }
    std::string o;
    o.reserve(s.size());
    for(size_t i = 0; i < s.size(); ++i)
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

bool writeRaw(const fs::path& p, std::string_view data)
{
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if(!f)
    {
        return false;
    }
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(f);
}

std::vector<fs::path> listFiles(const fs::path& dir, const std::vector<std::string>& exts, bool recursive)
{
    std::vector<fs::path> out;
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
        const std::string ext = e.path().extension().string();
        return std::find(exts.begin(), exts.end(), ext) != exts.end();
    };
    if(recursive)
    {
        for(fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        {
            if(want(*it))
            {
                out.push_back(it->path());
            }
        }
    }
    else
    {
        for(fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        {
            if(want(*it))
            {
                out.push_back(it->path());
            }
        }
    }
    return out;
}

std::string relPosix(const fs::path& p, const fs::path& base)
{
    return p.lexically_normal().lexically_relative(base.lexically_normal()).generic_string();
}

namespace
{

std::vector<std::string> lowerParts(const fs::path& p)
{
    std::vector<std::string> parts;
    for(const auto& part : p)
    {
        std::string s = part.string();
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
        parts.push_back(std::move(s));
    }
    return parts;
}

} // namespace

void sortPaths(std::vector<fs::path>& v)
{
    std::vector<std::pair<std::vector<std::string>, fs::path>> keyed;
    keyed.reserve(v.size());
    for(auto& p : v)
    {
        keyed.emplace_back(lowerParts(p), std::move(p));
    }
    std::sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    v.clear();
    for(auto& k : keyed)
    {
        v.push_back(std::move(k.second));
    }
}

void sortStrings(std::vector<std::string>& v)
{
    std::sort(v.begin(), v.end());
}

std::string_view trim(std::string_view s)
{
    size_t a = 0;
    size_t b = s.size();
    while(a < b && isSpace(s[a]))
    {
        ++a;
    }
    while(b > a && isSpace(s[b - 1]))
    {
        --b;
    }
    return s.substr(a, b - a);
}

std::vector<std::string_view> splitLines(std::string_view s)
{
    std::vector<std::string_view> out;
    size_t a = 0;
    while(a < s.size())
    {
        const size_t e = s.find('\n', a);
        if(e == std::string_view::npos)
        {
            out.push_back(s.substr(a));
            break;
        }
        out.push_back(s.substr(a, e - a));
        a = e + 1;
    }
    return out;
}

std::vector<std::string_view> splitWords(std::string_view s)
{
    std::vector<std::string_view> out;
    size_t i = 0;
    while(i < s.size())
    {
        while(i < s.size() && isSpace(s[i]))
        {
            ++i;
        }
        const size_t a = i;
        while(i < s.size() && !isSpace(s[i]))
        {
            ++i;
        }
        if(i > a)
        {
            out.push_back(s.substr(a, i - a));
        }
    }
    return out;
}

std::string blankKeepNewlines(std::string_view s)
{
    std::string o(s);
    for(char& c : o)
    {
        if(c != '\n')
        {
            c = ' ';
        }
    }
    return o;
}

int lineAt(std::string_view text, size_t pos)
{
    return 1 + static_cast<int>(std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(pos), '\n'));
}

bool startsWith(std::string_view s, std::string_view p)
{
    return s.substr(0, p.size()) == p;
}

bool endsWith(std::string_view s, std::string_view p)
{
    return s.size() >= p.size() && s.substr(s.size() - p.size()) == p;
}

size_t skipSpace(std::string_view s, size_t pos)
{
    while(pos < s.size() && isSpace(s[pos]))
    {
        ++pos;
    }
    return pos;
}

size_t identAt(std::string_view s, size_t pos)
{
    if(pos >= s.size() || isDigit(s[pos]) || !isWord(s[pos]))
    {
        return 0;
    }
    size_t e = pos + 1;
    while(e < s.size() && isWord(s[e]))
    {
        ++e;
    }
    return e - pos;
}

size_t matchClose(std::string_view s, size_t i, char open, char close)
{
    int depth = 0;
    for(size_t j = i; j < s.size(); ++j)
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
    return std::string_view::npos;
}

std::vector<std::string> lineStartGroups(const std::string& text, const std::string& pattern, char first)
{
    std::vector<std::string> out;
    const std::regex re(pattern);
    size_t next = 0; // (finditer: no match starts inside the last one)
    for(size_t ls = 0; ls < text.size();)
    {
        if(ls >= next)
        {
            const size_t p = skipSpace(text, ls);
            if(p < text.size() && text[p] == first)
            {
                std::smatch m;
                if(std::regex_search(text.begin() + static_cast<std::ptrdiff_t>(ls), text.end(), m, re,
                                     std::regex_constants::match_continuous))
                {
                    out.push_back(m[1].str());
                    next = ls + std::max<size_t>(1, static_cast<size_t>(m.length(0)));
                }
            }
        }
        const size_t nl = text.find('\n', ls);
        if(nl == std::string::npos)
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

std::wstring widen(const std::string& s)
{
    if(s.empty())
    {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

// CommandLineToArgvW's quoting.
void appendQuoted(std::wstring& cmd, const std::wstring& arg)
{
    if(!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos)
    {
        cmd += arg;
        return;
    }
    cmd += L'"';
    for(size_t i = 0;; ++i)
    {
        size_t backslashes = 0;
        while(i < arg.size() && arg[i] == L'\\')
        {
            ++i;
            ++backslashes;
        }
        if(i == arg.size())
        {
            cmd.append(backslashes * 2, L'\\');
            break;
        }
        if(arg[i] == L'"')
        {
            cmd.append(backslashes * 2 + 1, L'\\');
        }
        else
        {
            cmd.append(backslashes, L'\\');
        }
        cmd += arg[i];
    }
    cmd += L'"';
}

void drain(HANDLE h, std::string& into)
{
    char buf[1 << 16];
    DWORD got = 0;
    while(ReadFile(h, buf, sizeof(buf), &got, nullptr) && got > 0)
    {
        into.append(buf, got);
    }
}

} // namespace

ProcResult runProcess(const std::vector<std::string>& args, const fs::path& cwd, bool mergeStderr)
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
    std::vector<char> attrBuf(attrSize);
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

    std::wstring cmd;
    for(size_t i = 0; i < args.size(); ++i)
    {
        if(i)
        {
            cmd += L' ';
        }
        appendQuoted(cmd, widen(args[i]));
    }
    PROCESS_INFORMATION pi{};
    const std::wstring dir = cwd.wstring();
    const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                   EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, nullptr,
                                   dir.empty() ? nullptr : dir.c_str(), &si.StartupInfo, &pi);
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
    std::thread errThread;
    if(errRead)
    {
        errThread = std::thread([&] { drain(errRead, r.err); });
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

fs::path which(const std::string& name)
{
    wchar_t buf[MAX_PATH * 4];
    const std::wstring w = widen(name);
    for(const wchar_t* ext : {L".exe", L".com", L".bat", L".cmd", L""})
    {
        if(SearchPathW(nullptr, w.c_str(), ext, static_cast<DWORD>(std::size(buf)), buf, nullptr))
        {
            return fs::path(buf);
        }
    }
    return {};
}

fs::path findFteqcc(const Ctx& ctx, const std::vector<fs::path>& fallbacks)
{
    std::vector<std::string> cands;
    if(!ctx.fteqcc.empty())
    {
        cands.push_back(ctx.fteqcc.string());
    }
    if(const char* e = std::getenv("FTEQCC"); e && *e)
    {
        cands.emplace_back(e);
    }
    for(const auto& f : fallbacks)
    {
        cands.push_back(f.string());
    }
    cands.emplace_back("fteqcc64");
    cands.emplace_back("fteqcc");
    for(const auto& c : cands)
    {
        std::error_code ec;
        if(fs::is_regular_file(c, ec))
        {
            return fs::absolute(c, ec);
        }
        if(auto w = which(c); !w.empty())
        {
            return w;
        }
    }
    return {};
}

const std::vector<std::string> qcFlags = {"-O3", "-Fautoproto", "-Olo", "-Fiffloat", "-Fifvector", "-Fvectorlogic",
                                          "-Flo", "-Fsubscope", "-Wall", "-Wextra", "-Wno-F209", "-Wno-F208"};

bool writeWorkProgsSrc(const Ctx& ctx, const fs::path& work)
{
    std::error_code ec;
    fs::create_directories(work, ec);
    const fs::path qc = ctx.tree / "QC";
    bool ok = false;
    const std::string src = readText(qc / "progs.src", &ok);
    if(!ok)
    {
        return false;
    }
    std::vector<std::string> entries;
    for(auto l : splitLines(src))
    {
        const auto cut = l.find("//");
        const auto e = trim(cut == std::string_view::npos ? l : l.substr(0, cut));
        if(!e.empty())
        {
            entries.emplace_back(e);
        }
    }
    // Relative paths: fteqcc's progs.src reader stops at a drive's "C:". (The first entry is the output file.)
    std::string out = "progs.dat\n";
    for(size_t i = 1; i < entries.size(); ++i)
    {
        out += relPosix(fs::absolute(qc / entries[i]), fs::absolute(work));
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

constexpr uint32_t fourcc(const char* s)
{
    return static_cast<uint32_t>(static_cast<unsigned char>(s[0])) |
           static_cast<uint32_t>(static_cast<unsigned char>(s[1])) << 8 |
           static_cast<uint32_t>(static_cast<unsigned char>(s[2])) << 16 |
           static_cast<uint32_t>(static_cast<unsigned char>(s[3])) << 24;
}

constexpr uint32_t secondary16 = fourcc("1FTE") ^ fourcc("PROG");
constexpr uint32_t secondary32 = fourcc("1FTE") ^ fourcc("32B ");

} // namespace

std::string progsFormat(std::string_view data, ProgsHeader& hdr, bool& ok)
{
    ok = false;
    if(data.size() < 60)
    {
        return "not a progs.dat (too short)";
    }
    std::memcpy(hdr.h, data.data(), 60);
    ok = true;
    if(hdr.h[0] == 6)
    {
        return "id16";
    }
    if(hdr.h[0] == 7 && data.size() >= 92)
    {
        int32_t ext[8];
        std::memcpy(ext, data.data() + 60, 32);
        const auto secondary = static_cast<uint32_t>(ext[7]);
        std::string kind = secondary == secondary16   ? "fte16"
                           : secondary == secondary32 ? "fte32"
                                                      : std::format("version 7 kind {:08x}", secondary);
        return kind + (ext[6] ? " compressed" : "");
    }
    return std::format("version {}", hdr.h[0]);
}

} // namespace qvrc
