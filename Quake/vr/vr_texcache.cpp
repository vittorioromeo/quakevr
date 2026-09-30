// vr_texcache.cpp -- work the start-up and the map loads would redo every time, kept on disk under
// `<gamedir>/cache/<kind>/<build>/<key>.<ext>`: <key> a hash of everything the work reads, <build> the working code's own
// build (its source file's compile time: changed code never reads an old file). Other builds' folders of a kind are
// removed at its first write in a session. Files are written beside their place and renamed: another copy of the game
// never reads half a file.
// - normalmaps: the normal maps made from skins' colours (vr_normalmaps.cpp: TexMgr_SkinToNormals, 3-4 ms a skin; 0.2 s of
//   the first map load). vr_normalmap_cache: 1 on, 0 off, 2 check (made anyway and compared with the file).

#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_files.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

namespace
{

constexpr unsigned version = 1;

za::String dirFor(const char* kind, const char* build)
{
    return za::String{com_gamedir} + "/cache/" + kind + "/" + build;
}

za::String fileFor(const char* kind, const char* build, unsigned long long key, const char* ext)
{
    char name[40];
    snprintf(name, sizeof(name), "%016llx.%s", key, ext);
    return dirFor(kind, build) + "/" + name;
}

// The folders pruned this session (a kind's in a game directory, at a build): each game's own, by its path.
za::Vector<za::String> pruned;

// A kind's first write in a session (in each game directory): its other builds' folders removed (they are never read
// again).
void prune(const char* kind, const char* build)
{
    const za::String id = dirFor(kind, build);
    if(za::find(pruned.begin(), pruned.end(), id) != pruned.end())
    {
        return;
    }
    pruned.pushBack(id);
    const za::String root = za::String{com_gamedir} + "/cache/" + kind;
    za::Vector<za::String> others;
    qvr::files::forEachEntry(root.cStr(), [&](const char* name, bool isDirectory) {
        if(isDirectory && strcmp(name, build) != 0)
        {
            others.pushBack(root + "/" + name);
        }
    });
    for(const za::String& other : others)
    {
        qvr::files::removeAll(other.cStr());
    }
}

// A file: 4 bytes of magic, the version, two numbers of the caller's, then the data.
bool readFile(const za::String& path, const char (&magic)[5], unsigned& a, unsigned& b, za::Vector<char>& data)
{
    FILE* in = Sys_fopen(path.cStr(), "rb");
    if(!in)
    {
        return false;
    }
    Sys_fseek(in, 0, SEEK_END);
    const qfileofs_t size = Sys_ftell(in);
    Sys_fseek(in, 0, SEEK_SET);
    constexpr qfileofs_t headerSize = 4 + 3 * sizeof(unsigned);
    char m[4];
    unsigned header[3];
    bool ok = size >= headerSize && fread(m, 1, sizeof(m), in) == sizeof(m) &&
              fread(header, 1, sizeof(header), in) == sizeof(header) && memcmp(m, magic, 4) == 0 && header[0] == version;
    if(ok)
    {
        a = header[1];
        b = header[2];
        data.resize(static_cast<size_t>(size - headerSize));
        ok = fread(data.data(), 1, data.size(), in) == data.size();
    }
    fclose(in);
    return ok;
}

void writeFile(const char* kind, const char* build, const za::String& path, const char (&magic)[5], unsigned a, unsigned b,
    const void* data, size_t length)
{
    prune(kind, build);
    qvr::files::createDirectories(dirFor(kind, build).cStr());
    const za::String tmp = path + va(".%u.tmp", static_cast<unsigned>(Sys_DoubleTime() * 1e6) & 0xffffffu);
    FILE* out = Sys_fopen(tmp.cStr(), "wb");
    if(!out)
    {
        return;
    }
    const unsigned header[3] = {version, a, b};
    bool ok = fwrite(magic, 1, 4, out) == 4 && fwrite(header, 1, sizeof(header), out) == sizeof(header) &&
              (length == 0 || fwrite(data, 1, length, out) == length);
    ok = fclose(out) == 0 && ok;
    if(!ok || !qvr::files::rename(tmp.cStr(), path.cStr()))
    {
        qvr::files::remove(tmp.cStr());
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
    za::Vector<char> data;
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
