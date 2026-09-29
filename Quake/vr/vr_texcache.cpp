// vr_texcache.cpp -- the normal maps made from textures' and skins' colours (gl_texmgr.c: TexMgr_ShadingToNormals,
// TexMgr_SkinToNormals), kept on disk: the same texels again without the work (0.2 s of the first map load: 60 skins;
// 50 ms of a later map's new models). A file per map, `<gamedir>/cache/normalmaps/<build>/<key>.nrm`, <key> a hash of
// everything the making reads (the texels, their size, the kind, the heights' mask, the texels per unit), <build> the
// making code's own build (gl_texmgr.c's compile time: a changed maker never reads an old map). Other builds' folders
// are removed at the first use. vr_normalmap_cache: 1 on, 0 off, 2 check (make every map anyway and count the ones that
// differ from their file: none should).

#include "vr_engine.hpp"
#include "vr_cvars.hpp"

#include <cstdio>
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

constexpr char magic[4] = {'Q', 'V', 'R', 'N'};
constexpr unsigned version = 1;

std::string dirFor(const char* build)
{
    return std::string{com_gamedir} + "/cache/normalmaps/" + build;
}

std::string fileFor(const char* build, unsigned long long key)
{
    char name[32];
    snprintf(name, sizeof(name), "%016llx.nrm", key);
    return dirFor(build) + "/" + name;
}

// The first use in a session: other builds' folders removed (they are never read again).
void prune(const char* build)
{
    static std::string done;
    if(done == build)
    {
        return;
    }
    done = build;
    std::error_code ec;
    const fs::path root = toPath(std::string{com_gamedir} + "/cache/normalmaps");
    for(fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
    {
        if(it->is_directory(ec) && it->path().filename().string() != build)
        {
            std::error_code ec2;
            fs::remove_all(it->path(), ec2);
        }
    }
}

int checked = 0, differed = 0;

} // namespace

extern "C" int VR_NormalCacheMode(void)
{
    return static_cast<int>(qvr::vr_normalmap_cache.value);
}

extern "C" int VR_NormalCacheLoad(const char* build, unsigned long long key, unsigned char* rgba, int width, int height)
{
    std::ifstream in(toPath(fileFor(build, key)), std::ios::binary);
    if(!in)
    {
        return 0;
    }
    char m[4];
    unsigned header[3];
    in.read(m, sizeof(m));
    in.read(reinterpret_cast<char*>(header), sizeof(header));
    if(!in || memcmp(m, magic, sizeof(m)) != 0 || header[0] != version || header[1] != static_cast<unsigned>(width) ||
       header[2] != static_cast<unsigned>(height))
    {
        return 0;
    }
    const std::streamsize n = static_cast<std::streamsize>(width) * height * 4;
    in.read(reinterpret_cast<char*>(rgba), n);
    return in.gcount() == n ? 1 : 0;
}

extern "C" void VR_NormalCacheStore(const char* build, unsigned long long key, const unsigned char* rgba, int width, int height)
{
    prune(build);
    std::error_code ec;
    fs::create_directories(toPath(dirFor(build)), ec);
    const std::string path = fileFor(build, key);
    // Written beside it, then renamed: a reader (another copy of the game) never sees half a file.
    const std::string tmp = path + va(".%u.tmp", static_cast<unsigned>(Sys_DoubleTime() * 1e6) & 0xffffffu);
    {
        std::ofstream out(toPath(tmp), std::ios::binary | std::ios::trunc);
        if(!out)
        {
            return;
        }
        const unsigned header[3] = {version, static_cast<unsigned>(width), static_cast<unsigned>(height)};
        out.write(magic, sizeof(magic));
        out.write(reinterpret_cast<const char*>(header), sizeof(header));
        out.write(reinterpret_cast<const char*>(rgba), static_cast<std::streamsize>(width) * height * 4);
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
