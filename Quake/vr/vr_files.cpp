// vr_files.cpp -- the VR code's file access (vr_files.hpp): the engine's Sys_* calls, and the system's own where the
// engine has none (a directory's removal, a file's write time to the system's resolution). Its own translation unit:
// <windows.h> stays out of the engine's headers.

#include "vr_files.hpp"

#include "vr_engine.hpp"

#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace qvr::files
{

namespace
{

#ifdef _WIN32
// UTF-8 to UTF-16 (the engine's UTF8ToWideString, for the calls it doesn't wrap): false when it doesn't fit.
[[nodiscard]] bool wide(const char* path, wchar_t (&out)[MAX_PATH])
{
    return MultiByteToWideChar(CP_UTF8, 0, path, -1, out, MAX_PATH) != 0;
}
#endif

bool removeDirectory(const char* path)
{
#ifdef _WIN32
    wchar_t w[MAX_PATH];
    return wide(path, w) && RemoveDirectoryW(w) != 0;
#else
    return rmdir(path) == 0;
#endif
}

template <typename Vector>
bool readWhole(const char* path, const char* mode, Vector& out)
{
    out.clear();
    FILE* f = Sys_fopen(path, mode);
    if(!f)
    {
        return false;
    }
    char chunk[16384];
    bool ok = true;
    for(;;)
    {
        const size_t n = fread(chunk, 1, sizeof(chunk), f);
        if(n > 0)
        {
            const za::SizeT at = out.size();
            out.resize(at + n);
            ZA_MEMCPY(out.data() + at, chunk, n);
        }
        if(n < sizeof(chunk))
        {
            ok = !ferror(f);
            break;
        }
    }
    fclose(f);
    return ok;
}

bool writeWhole(const char* path, const char* mode, const void* data, za::SizeT size)
{
    FILE* f = Sys_fopen(path, mode);
    if(!f)
    {
        return false;
    }
    const bool wrote = size == 0 || fwrite(data, 1, size, f) == size;
    const bool closed = fclose(f) == 0;
    return wrote && closed;
}

} // namespace

bool readBytes(const char* path, za::Vector<unsigned char>& out)
{
    return readWhole(path, "rb", out);
}

bool readBytes(const char* path, za::Vector<char>& out)
{
    return readWhole(path, "rb", out);
}

bool readText(const char* path, za::String& out)
{
    za::Vector<char> bytes;
    const bool ok = readWhole(path, "r", bytes); // (text mode: CR LF read as LF on Windows, as a stream's)
    out.clear();
    if(!bytes.empty())
    {
        out.append(bytes.data(), bytes.size());
    }
    return ok;
}

bool writeBytes(const char* path, const void* data, za::SizeT size)
{
    return writeWhole(path, "wb", data, size);
}

bool writeText(const char* path, za::StringView text)
{
    return writeWhole(path, "w", text.data(), text.size());
}

bool createDirectories(const char* path)
{
    if(isDirectory(path))
    {
        return true;
    }
    // Each level from the first, as std::filesystem::create_directories makes them (Sys_mkdir: the one level).
    za::String level;
    const za::StringView p{path};
    for(za::SizeT i = 0; i <= p.size(); i++)
    {
        if(i == p.size() || p[i] == '/' || p[i] == '\\')
        {
            level = za::String{p.substrByPosLen(0, i)};
            if(!level.empty() && !(level.size() == 2 && level[1] == ':') && !isDirectory(level.cStr()))
            {
                Sys_mkdir(level.cStr());
            }
        }
    }
    return isDirectory(path);
}

bool exists(const char* path)
{
    return Sys_FileType(path) != FS_ENT_NONE;
}

bool isDirectory(const char* path)
{
    return Sys_FileType(path) == FS_ENT_DIRECTORY;
}

bool isFile(const char* path)
{
    return Sys_FileType(path) == FS_ENT_FILE;
}

bool remove(const char* path)
{
    if(isDirectory(path))
    {
        return removeDirectory(path);
    }
    return Sys_remove(path) == 0;
}

bool removeAll(const char* path)
{
    if(isDirectory(path))
    {
        za::Vector<za::String> entries;
        forEachEntry(path, [&](const char* name, bool) { entries.pushBack(za::String{path} + "/" + name); });
        for(const za::String& e : entries)
        {
            removeAll(e.cStr());
        }
        return removeDirectory(path);
    }
    return Sys_remove(path) == 0;
}

bool rename(const char* from, const char* to)
{
    return Sys_ReplaceFile(from, to) == 0;
}

void forEachEntry(const char* dir, za::FunctionRef<void(const char* name, bool isDirectory)> f)
{
    for(findfile_t* find = Sys_FindFirst(dir, nullptr); find; find = Sys_FindNext(find))
    {
        if(!strcmp(find->name, ".") || !strcmp(find->name, ".."))
        {
            continue;
        }
        f(find->name, (find->attribs & FA_DIRECTORY) != 0);
    }
}

za::I64 lastWriteTime(const char* path)
{
#ifdef _WIN32
    wchar_t w[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA data;
    if(!wide(path, w) || !GetFileAttributesExW(w, GetFileExInfoStandard, &data))
    {
        return 0;
    }
    return static_cast<za::I64>((static_cast<za::U64>(data.ftLastWriteTime.dwHighDateTime) << 32) |
                                data.ftLastWriteTime.dwLowDateTime);
#else
    struct stat st;
    if(stat(path, &st) != 0)
    {
        return 0;
    }
#ifdef __APPLE__
    return static_cast<za::I64>(st.st_mtimespec.tv_sec) * 1000000000 + st.st_mtimespec.tv_nsec;
#else
    return static_cast<za::I64>(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec;
#endif
#endif
}

namespace
{

[[nodiscard]] bool isSpace(const char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

// The next word (the white space before it skipped), or false at the end (a stream's sentry failing).
[[nodiscard]] bool nextWord(za::StringView& text, za::StringView& word)
{
    za::SizeT i = 0;
    while(i < text.size() && isSpace(text[i]))
    {
        i++;
    }
    za::SizeT j = i;
    while(j < text.size() && !isSpace(text[j]))
    {
        j++;
    }
    word = text.substrByPosLen(i, j - i);
    text = text.substrByPosLen(j);
    return !word.empty();
}

// A number from the whole word (0, and false, when it doesn't start with one: a stream's failed extraction).
template <typename T, typename Parse>
bool nextNumber(za::StringView& text, bool& failed, T& value, Parse&& parse)
{
    za::StringView word;
    if(failed || !nextWord(text, word))
    {
        failed = true;
        return false;
    }
    char buf[128];
    const za::SizeT n = za::min<za::SizeT>(word.size(), sizeof(buf) - 1);
    ZA_MEMCPY(buf, word.data(), n);
    buf[n] = '\0';
    char* end = buf;
    const T v = parse(buf, &end);
    if(end == buf)
    {
        value = T{0};
        failed = true;
        return false;
    }
    value = v;
    return true;
}

} // namespace

bool Words::next(za::StringView& word)
{
    if(failed_ || !nextWord(text_, word))
    {
        failed_ = true;
        return false;
    }
    return true;
}

bool Words::next(za::String& word)
{
    za::StringView w;
    if(!next(w))
    {
        return false;
    }
    word = w;
    return true;
}

bool Words::next(float& value)
{
    return nextNumber(text_, failed_, value, [](const char* s, char** e) { return strtof(s, e); });
}

bool Words::next(double& value)
{
    return nextNumber(text_, failed_, value, [](const char* s, char** e) { return strtod(s, e); });
}

bool Words::next(int& value)
{
    return nextNumber(text_, failed_, value, [](const char* s, char** e) { return static_cast<int>(strtol(s, e, 10)); });
}

za::StringView fileName(za::StringView path)
{
    const za::SizeT slash = path.findLastOf("/\\");
    return slash == za::StringView::nPos ? path : path.substrByPosLen(slash + 1);
}

} // namespace qvr::files
