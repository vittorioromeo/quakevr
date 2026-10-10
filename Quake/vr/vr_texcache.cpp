// vr_texcache.cpp -- work the start-up and the map loads would redo every time, kept on disk under
// `<gamedir>/cache/<kind>/<build>/<key>.<ext>`: <key> a hash of everything the work reads, <build> the working code's own
// build (its source file's compile time: changed code never reads an old file). Other builds' folders of a kind are
// removed at its first write in a session. Files are written beside their place and renamed: another copy of the game
// never reads half a file.
// - normalmaps: the normal maps made from skins' colours (vr_normalmaps.cpp: TexMgr_SkinToNormals, 3-4 ms a skin; 0.2 s of
//   the first map load). vr_normalmap_cache: 1 on, 0 off, 2 check (made anyway and compared with the file).
// - gunpieces: the guns' convex pieces (vr_box3d.cpp gunPieces, vr_convex.cpp: 10-85 ms a gun, the first time one is
//   dropped, picked up or holstered in a session). vr_gun_pieces_cache: 1 on, 0 off, 2 check. Through vr_diskcache.hpp.

#include "vr_diskcache.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_files.hpp"
#include "vr_jobs.hpp"

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

// The files being written on the game's pool (writeFile; the main thread's list): finished at the latest by
// finishWrites (the game's shutdown), the oldest waited for when too many are under way (each holds its bytes).
za::Vector<qvr::jobs::Future<void>> writes;
constexpr za::SizeT writesMost = 64;

// A file written (its folder made, other builds' pruned, here), then its bytes written and renamed into place on the
// pool: a file a map's first load makes per skin, 1-2 ms each on the main thread (open, write, close, rename) that no
// one waited for. Until it is renamed into place it is not there: the same key asked for meanwhile is made again (the
// same bytes), as before its first write.
void writeFile(const char* kind, const char* build, const za::String& path, const char (&magic)[5], unsigned a, unsigned b,
    const void* data, size_t length)
{
    prune(kind, build);
    qvr::files::createDirectories(dirFor(kind, build).cStr());
    const za::String tmp = path + va(".%u.tmp", static_cast<unsigned>(Sys_DoubleTime() * 1e6) & 0xffffffu);
    za::Vector<unsigned char> bytes;
    const unsigned header[3] = {version, a, b};
    bytes.resize(4 + sizeof(header) + length);
    memcpy(bytes.data(), magic, 4);
    memcpy(bytes.data() + 4, header, sizeof(header));
    if(length)
    {
        memcpy(bytes.data() + 4 + sizeof(header), data, length);
    }
    while(!writes.empty() && writes.front().ready())
    {
        writes.erase(writes.begin());
    }
    if(writes.size() >= writesMost)
    {
        writes.front().wait();
        writes.erase(writes.begin());
    }
    writes.pushBack(qvr::jobs::async([tmp, path, bytes = ZA_MOVE(bytes)] {
        FILE* out = Sys_fopen(tmp.cStr(), "wb");
        if(!out)
        {
            return;
        }
        bool ok = fwrite(bytes.data(), 1, bytes.size(), out) == bytes.size();
        ok = fclose(out) == 0 && ok;
        if(!ok || !qvr::files::rename(tmp.cStr(), path.cStr()))
        {
            qvr::files::remove(tmp.cStr());
        }
    }));
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

bool qvr::diskcache::read(const char* kind, const char* build, unsigned long long key, const char* ext,
    const char (&magic)[5], unsigned& a, unsigned& b, za::Vector<char>& data)
{
    return readFile(fileFor(kind, build, key, ext), magic, a, b, data);
}

void qvr::diskcache::write(const char* kind, const char* build, unsigned long long key, const char* ext,
    const char (&magic)[5], unsigned a, unsigned b, const void* data, size_t length)
{
    writeFile(kind, build, fileFor(kind, build, key, ext), magic, a, b, data, length);
}

// The files still being written finished (VR_Shutdown, before the pool goes).
extern "C" void VR_TexCacheFinishWrites(void)
{
    writes.clear(); // (each Future waits for its write)
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
