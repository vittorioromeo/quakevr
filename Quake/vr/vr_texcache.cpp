// vr_texcache.cpp -- work the start-up and the map loads would redo every time, kept on disk under
// `<gamedir>/cache/<kind>/<build>/<key>.<ext>`: <key> a hash of everything the work reads, <build> the working code's own
// build (its source file's compile time: changed code never reads an old file). Other builds' folders of a kind are
// removed at its first write in a session. Files are written beside their place and renamed: another copy of the game
// never reads half a file.
// - normalmaps: the normal maps made from skins' colours (gl_texmgr.c: TexMgr_SkinToNormals, 3-4 ms a skin; 0.2 s of
//   the first map load). vr_normalmap_cache: 1 on, 0 off, 2 check (made anyway and compared with the file).

#include "vr_engine.hpp"
#include "vr_cvars.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace
{

namespace fs = std::filesystem;

fs::path toPath(const std::string& utf8)
{
    return fs::path(std::u8string(utf8.begin(), utf8.end()));
}

constexpr unsigned version = 1;

std::string dirFor(const char* kind, const char* build)
{
    return std::string{com_gamedir} + "/cache/" + kind + "/" + build;
}

std::string fileFor(const char* kind, const char* build, unsigned long long key, const char* ext)
{
    char name[40];
    snprintf(name, sizeof(name), "%016llx.%s", key, ext);
    return dirFor(kind, build) + "/" + name;
}

// A kind's first write in a session: its other builds' folders removed (they are never read again).
void prune(const char* kind, const char* build)
{
    static std::vector<std::string> done;
    const std::string id = std::string{kind} + "/" + build;
    for(const std::string& d : done)
    {
        if(d == id)
        {
            return;
        }
    }
    done.push_back(id);
    std::error_code ec;
    const fs::path root = toPath(std::string{com_gamedir} + "/cache/" + kind);
    for(fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
    {
        if(it->is_directory(ec) && it->path().filename().string() != build)
        {
            std::error_code ec2;
            fs::remove_all(it->path(), ec2);
        }
    }
}

// A file: 4 bytes of magic, the version, two numbers of the caller's, then the data.
bool readFile(const std::string& path, const char (&magic)[5], unsigned& a, unsigned& b, std::vector<char>& data)
{
    std::ifstream in(toPath(path), std::ios::binary | std::ios::ate);
    if(!in)
    {
        return false;
    }
    const std::streamoff size = in.tellg();
    constexpr std::streamoff headerSize = 4 + 3 * sizeof(unsigned);
    if(size < headerSize)
    {
        return false;
    }
    in.seekg(0);
    char m[4];
    unsigned header[3];
    in.read(m, sizeof(m));
    in.read(reinterpret_cast<char*>(header), sizeof(header));
    if(!in || memcmp(m, magic, 4) != 0 || header[0] != version)
    {
        return false;
    }
    a = header[1];
    b = header[2];
    data.resize(static_cast<size_t>(size - headerSize));
    in.read(data.data(), static_cast<std::streamsize>(data.size()));
    return in.gcount() == static_cast<std::streamsize>(data.size());
}

void writeFile(const char* kind, const char* build, const std::string& path, const char (&magic)[5], unsigned a, unsigned b,
    const void* data, size_t length)
{
    prune(kind, build);
    std::error_code ec;
    fs::create_directories(toPath(dirFor(kind, build)), ec);
    const std::string tmp = path + va(".%u.tmp", static_cast<unsigned>(Sys_DoubleTime() * 1e6) & 0xffffffu);
    {
        std::ofstream out(toPath(tmp), std::ios::binary | std::ios::trunc);
        if(!out)
        {
            return;
        }
        const unsigned header[3] = {version, a, b};
        out.write(magic, 4);
        out.write(reinterpret_cast<const char*>(header), sizeof(header));
        out.write(static_cast<const char*>(data), static_cast<std::streamsize>(length));
        if(!out)
        {
            out.close();
            fs::remove(toPath(tmp), ec);
            return;
        }
    }
    fs::rename(toPath(tmp), toPath(path), ec);
    if(ec)
    {
        fs::remove(toPath(tmp), ec);
    }
}

constexpr char normalMagic[5] = "QVRN";

int checked = 0, differed = 0;

} // namespace

extern "C" int VR_NormalCacheMode(void)
{
    return static_cast<int>(qvr::vr_normalmap_cache.value);
}

extern "C" int VR_NormalCacheLoad(const char* build, unsigned long long key, unsigned char* rgba, int width, int height)
{
    unsigned w = 0, h = 0;
    std::vector<char> data;
    if(!readFile(fileFor("normalmaps", build, key, "nrm"), normalMagic, w, h, data) || w != static_cast<unsigned>(width) ||
       h != static_cast<unsigned>(height) || data.size() != static_cast<size_t>(width) * height * 4)
    {
        return 0;
    }
    memcpy(rgba, data.data(), data.size());
    return 1;
}

extern "C" void VR_NormalCacheStore(const char* build, unsigned long long key, const unsigned char* rgba, int width, int height)
{
    writeFile("normalmaps", build, fileFor("normalmaps", build, key, "nrm"), normalMagic, static_cast<unsigned>(width),
        static_cast<unsigned>(height), rgba, static_cast<size_t>(width) * height * 4);
}

extern "C" void VR_NormalCacheChecked(int same, const char* name)
{
    checked++;
    if(!same)
    {
        differed++;
        Con_Warning("vr_normalmap_cache 2: %s's normal map differs from its file (%d of %d so far)\n", name, differed, checked);
    }
    else if(developer.value)
    {
        Con_DPrintf("vr_normalmap_cache 2: %s's normal map is its file's (%d checked, %d differed)\n", name, checked, differed);
    }
}
