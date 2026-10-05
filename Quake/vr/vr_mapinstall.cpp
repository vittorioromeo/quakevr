// vr_mapinstall.cpp -- a package from the map index (vr_mapindex.cpp) put into the game: its zip downloaded to
// quakevr/cache/maps/<sha256>.zip, its files unpacked with miniz into the user's game dir, its startmap started, and
// what was written recorded in quakevr/cache/maps_installed.txt so that it can be listed and removed again.
//
// The download and the unpacking run on a thread of their own (as the index's fetch does): the game never waits for
// them, and nothing is printed from that thread. poll() takes the finished job on the main thread: its console line,
// the registry updated, the map queued.
//
// Where the files go: a BSP to <game dir>/maps/, because that is where the engine's `map` command looks
// (host_cmd.c's Cmd_Map_f builds "maps/%s.bsp"); everything else at the game dir's root, where the engine's search
// paths reach it. The index's install.extract is read for its maps/ part only: the engine does not add a package's own
// subdirectory to the search paths, so a file placed in one would not be found.

#include "vr_mapinstall.hpp"
#include "vr_engine.hpp"
#include "vr_api.h" // VR_FileCacheForget: the engine's directory listings, told that files appeared
#include "vr_files.hpp"
#include "vr_mem.hpp"
#include "vr_zancle.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/ToString.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include "miniz.h" // (the engine's C header, which guards itself for C++)

#include <stdlib.h>
#include <string.h>

namespace qvr::mapinstall
{
namespace
{

constexpr char registryMagic[] = "#quakevr-maps-installed-1";
constexpr char fieldSep = '\t';
constexpr int downloadShare = 90; // the download's part of the job's percent; the unpacking's is the rest

// The live job (what the last one reported, and the one running now). Main thread only.
mem::Cache<Job> current{"map install job", mem::Never};

// What is installed, as maps_installed.txt holds it. Main thread only.
struct Registry
{
    za::Vector<InstalledFile> files;
    za::Vector<Installed> packages;
    auto members()
    {
        return mem::list(files, packages);
    }
};
mem::Cache<Registry> registry{"map install registry", mem::Never};
bool registryLoaded = false;

// The paths, set at start() (com_basedirs' last: the user's game dir, as vr_mapindex.cpp's cache path).
za::String gameDirName;
za::String mapsDirName;
za::String cacheDirName;
za::String registryPath;

// The handoff (vr_mapindex.cpp's shape): the thread fills `pending`, poll() takes it.
za::AtomicMutex handoff;
za::UniquePtr<Job> pending;
za::Atomic<bool> pendingReady{false};
za::Thread worker;
za::Atomic<bool> running{false};
SDL_atomic_t cancelJob{}; // (the Download API's abort flag; not named `cancel`: this module has a cancel() too)
SDL_atomic_t progress{}; // 0..100, read while the job runs

// The job asked for, copied out of the index before the thread starts: a fetch that finishes meanwhile replaces the
// index and every Entry in it.
struct Request
{
    za::String sha, title, urls; // the mirrors, mapindex::partSep separated
    za::U64 zipBytes{0};
    bool install{true};
};
Request request;

[[nodiscard]] za::String zipPath(const za::String& sha)
{
    return cacheDirName + "/" + sha + ".zip";
}

// ---------------------------------------------------------------- the installed list

bool writeRegistry();
void rebuildPackages();

void loadRegistry()
{
    registryLoaded = true;
    registry.files.clear();
    registry.packages.clear();
    za::String text;
    if(!files::readText(registryPath.cStr(), text))
    {
        return;
    }
    int line = 0;
    files::forLines(text, [&](za::StringView l)
    {
        if(line == 0)
        {
            if(l != za::StringView{registryMagic})
            {
                registry.files.clear();
                return;
            }
        }
        else if(!l.empty())
        {
            // sha \t bytes \t path (the path last: it may hold anything but a tab)
            const za::SizeT a = l.findFirstOf(fieldSep);
            if(a == za::StringView::nPos)
            {
                return;
            }
            const za::SizeT b = l.findFirstOf(fieldSep, a + 1);
            if(b == za::StringView::nPos)
            {
                return;
            }
            InstalledFile f;
            f.sha = za::String{l.substrByPosLen(0, a)};
            f.bytes = static_cast<za::U64>(strtoull(za::String{l.substrByPosLen(a + 1, b - a - 1)}.cStr(), nullptr, 10));
            f.path = za::String{l.substrByPosLen(b + 1, l.size() - b - 1)};
            if(f.sha.size() && f.path.size())
            {
                registry.files.pushBack(ZA_MOVE(f));
            }
        }
        line++;
    });
    rebuildPackages();
}

void rebuildPackages()
{
    registry.packages.clear();
    for(const InstalledFile& f : registry.files)
    {
        Installed* p = nullptr;
        for(Installed& c : registry.packages)
        {
            if(c.sha == f.sha)
            {
                p = &c;
                break;
            }
        }
        if(!p)
        {
            registry.packages.emplaceBack();
            p = &registry.packages.back();
            p->sha = f.sha;
            if(const mapindex::Entry* e = mapindex::find(f.sha))
            {
                p->title = za::String{mapindex::index().field(e->title)};
            }
            else
            {
                p->title = za::String{za::StringView{f.sha}.substrByPosLen(0, 12)}; // (a package the index no longer has)
            }
        }
        p->bytes += f.bytes;
        p->files++;
    }
}

bool writeRegistry()
{
    za::String out;
    out += registryMagic;
    out += "\n";
    for(const InstalledFile& f : registry.files)
    {
        out += f.sha;
        out += fieldSep;
        out += za::toString(static_cast<unsigned long long>(f.bytes));
        out += fieldSep;
        out += f.path;
        out += "\n";
    }
    return files::writeText(registryPath.cStr(), out);
}

// ---------------------------------------------------------------- the paths a zip entry may be written to

// A path's last segment.
za::String baseName(const za::String& path)
{
    const za::SizeT slash = path.findLastOf('/');
    return slash == za::StringView::nPos ? path : za::String{path.substrByPosLen(slash + 1, path.size() - slash - 1)};
}

// ".bsp" / ".BSP".
bool endsFolded(const za::String& s, const char* suffix)
{
    const za::SizeT n = strlen(suffix);
    return s.size() >= n && !q_strcasecmp(s.cStr() + (s.size() - n), suffix);
}

// A zip entry's name as a path inside the game dir (false: it does not belong here). `..`, an absolute path, a drive
// letter, a Windows separator and an empty segment are all refused, so that nothing written here can leave the game dir.
bool safePath(const char* name, za::String& out)
{
    out.clear();
    if(!name || !*name || name[0] == '/' || name[0] == '\\')
    {
        return false;
    }
    const za::SizeT n = static_cast<za::SizeT>(strlen(name));
    if(n > 512)
    {
        return false;
    }
    za::Vector<za::String> segments;
    za::SizeT i = 0;
    while(i < n)
    {
        za::SizeT j = i;
        while(j < n && name[j] != '/')
        {
            j++;
        }
        const za::String part{name + i, j - i};
        if(part.empty() || part == "." || part == ".." || part.findFirstOf(":\\") != za::StringView::nPos)
        {
            return false;
        }
        segments.pushBack(part);
        i = j + 1;
    }
    if(segments.empty())
    {
        return false;
    }
    for(za::SizeT s = 0; s < segments.size(); s++)
    {
        if(s)
        {
            out += "/";
        }
        out += segments[s];
    }
    return true;
}

// A file that is not part of the package (a macOS archive's resource fork, its folder metadata).
[[nodiscard]] bool skippedFile(const za::String& path)
{
    za::String lower = path;
    for(char& c : lower)
    {
        c = static_cast<char>(tolower(c));
    }
    return lower.startsWith("__macosx/") || lower.contains("/__macosx/") || lower.endsWith(".ds_store") ||
           lower.endsWith("/thumbs.db");
}

// The one folder every entry is under (a zip built around a folder: the index's zipbasedir tag). "" : none.
[[nodiscard]] za::String commonTopDir(const za::Vector<za::String>& paths)
{
    za::String top;
    for(const za::String& p : paths)
    {
        const za::SizeT slash = p.findFirstOf('/');
        if(slash == za::StringView::nPos)
        {
            return za::String{}; // (a file at the root: there is no folder around everything)
        }
        const za::String first{za::String{p.substrByPosLen(0, slash)}};
        if(top.empty())
        {
            top = first;
        }
        else if(top != first)
        {
            return za::String{};
        }
    }
    return top;
}

// A BSP's version: 29 (Quake's), or a BSP2 variant's (its version field spells 2PSB, BSP2 or Q64). Anything else is a
// map this engine cannot load, and is left out rather than written and failed on. (dheader_t's ident is its first int.)
bool bspVersionOk(const char* data, za::SizeT n, za::String& what)
{
    if(n < 4)
    {
        what = "too short to be a BSP";
        return false;
    }
    const za::U32 version = static_cast<za::U32>(static_cast<unsigned char>(data[0])) |
                            (static_cast<za::U32>(static_cast<unsigned char>(data[1])) << 8) |
                            (static_cast<za::U32>(static_cast<unsigned char>(data[2])) << 16) |
                            (static_cast<za::U32>(static_cast<unsigned char>(data[3])) << 24);
    char tag[5]{};
    memcpy(tag, data, 4);
    if(version == 29)
    {
        what = "29";
        return true;
    }
    if(!memcmp(tag, "2PSB", 4) || !memcmp(tag, "BSP2", 4) || !memcmp(tag, "Q64", 3))
    {
        what = za::String{tag, 4};
        return true;
    }
    what = za::String{"version "} + za::toString(version);
    return false;
}

// ---------------------------------------------------------------- the download

size_t writeChunk(void* buffer, size_t size, size_t nmemb, void* stream)
{
    if(SDL_AtomicGet(&cancelJob))
    {
        return 0; // (the transfer stops: as vr_mapindex.cpp's and host_cmd.c's do)
    }
    za::Vector<char>& body = *static_cast<za::Vector<char>*>(stream);
    const za::SizeT n = size * nmemb;
    body.reserveMore(n);
    body.unsafeEmplaceBackRange(static_cast<const char*>(buffer), n);
    if(request.zipBytes)
    {
        const za::U64 got = static_cast<za::U64>(body.size());
        SDL_AtomicSet(&progress, static_cast<int>(downloadShare * (got < request.zipBytes ? got : request.zipBytes) / request.zipBytes));
    }
    return nmemb;
}

// The mirrors, in the index's order.
void splitUrls(const za::String& urls, za::Vector<za::String>& out)
{
    out.clear();
    za::SizeT i = 0;
    const za::SizeT n = urls.size();
    while(i < n)
    {
        za::SizeT j = i;
        while(j < n && urls.cStr()[j] != mapindex::partSep)
        {
            j++;
        }
        if(j > i)
        {
            out.pushBack(za::String{urls.cStr() + i, j - i});
        }
        i = j + 1;
    }
}

bool downloadZip(za::Vector<char>& body, za::String& why)
{
    za::Vector<za::String> urls;
    splitUrls(request.urls, urls);
    if(urls.empty())
    {
        why = "the index gives no download URL";
        return false;
    }
    for(const za::String& url : urls)
    {
        if(SDL_AtomicGet(&cancelJob))
        {
            why = "cancelled";
            return false;
        }
        body.clear();
        download_t dl{};
        dl.write_fn = writeChunk;
        dl.write_data = &body;
        dl.abort = &cancelJob;
        const za::U32 t0 = SDL_GetTicks();
        const bool ok = Download(url.cStr(), &dl);
        if(!ok)
        {
            why = za::String{"HTTP "} +
                  za::toString(dl.response ? dl.response : 0) + " from " +
                  (dl.error ? dl.error : "no response");
            continue;
        }
        if(body.empty())
        {
            why = "the download came back empty";
            continue;
        }
        current.downloadMs = static_cast<int>(SDL_GetTicks() - t0); // (not read by the thread: set before the handoff)
        return true;
    }
    return false;
}

size_t readFromMemory(void* opaque, mz_uint64 ofs, void* buf, size_t n)
{
    const za::Vector<char>& body = *static_cast<const za::Vector<char>*>(opaque);
    if(ofs + n > body.size())
    {
        return 0;
    }
    memcpy(buf, body.data() + ofs, n);
    return n;
}

// ---------------------------------------------------------------- the unpacking

// `body` (a zip) into the game dir. Appends what it wrote to `job.wrote`; `why` says what stopped it.
bool extractZip(const za::Vector<char>& body, Job& job, za::String& why)
{
    mz_zip_archive z{};
    z.m_pRead = readFromMemory;
    z.m_pIO_opaque = const_cast<za::Vector<char>*>(&body);
    if(!mz_zip_reader_init(&z, static_cast<mz_uint64>(body.size()), 0))
    {
        why = "not a zip this engine can read"; // (miniz.c's error string is compiled out of the engine's build)
        return false;
    }

    const int count = static_cast<int>(z.m_total_files); // (mz_zip_reader_get_num_files is compiled out too)
    if(count <= 0 || count > maxFiles)
    {
        why = za::String{"the zip holds "} + za::toString(count) + " files (up to " + za::toString(maxFiles) + ")";
        mz_zip_reader_end(&z);
        return false;
    }

    // Its entries, as game-dir paths first (so that the one folder around everything can be recognised and stripped).
    struct Item
    {
        za::String path;
        za::U64 bytes{0};
        int index{0};
        bool dir{false};
    };
    za::Vector<Item> items;
    za::Vector<za::String> paths;
    za::U64 unpacked = 0;
    int refused = 0;
    for(int i = 0; i < count; i++)
    {
        mz_zip_archive_file_stat st{};
        if(!mz_zip_reader_file_stat(&z, static_cast<mz_uint>(i), &st))
        {
            continue;
        }
        Item it;
        it.index = i;
        it.bytes = static_cast<za::U64>(st.m_uncomp_size);
        it.dir = mz_zip_reader_is_file_a_directory(&z, static_cast<mz_uint>(i)) != MZ_FALSE;
        if(!safePath(st.m_filename, it.path) || skippedFile(it.path))
        {
            refused++;
            continue;
        }
        unpacked += it.bytes;
        paths.pushBack(it.path);
        items.pushBack(ZA_MOVE(it));
    }
    if(unpacked > maxUnpackedBytes)
    {
        why = za::String{"its files would unpack to "} + za::toString(unpacked / 1024 / 1024) + " MB (up to " +
              za::toString(maxUnpackedBytes / 1024 / 1024) + " MB)";
        mz_zip_reader_end(&z);
        return false;
    }

    const za::String top = commonTopDir(paths); // (the zipbasedir tag: a single top-level folder, stripped)

    int written = 0;
    int skipped = 0;
    int done = 0;
    za::String notes;
    for(const Item& it : items)
    {
        if(SDL_AtomicGet(&cancelJob))
        {
            why = "cancelled";
            mz_zip_reader_end(&z);
            return false;
        }
        za::String rel = it.path;
        if(!top.empty() && rel.size() > top.size() + 1)
        {
            rel.erase(0, top.size() + 1);
        }
        if(rel.empty())
        {
            refused++;
            continue;
        }
        // A BSP to maps/, under its own name only: that is where the engine's `map` command looks. Everything else to
        // the game dir's root, keeping its path (its textures and sounds are named from there).
        const bool bsp = endsFolded(rel, ".bsp");
        const za::String target = bsp ? mapsDirName + "/" + baseName(rel) : gameDirName + "/" + rel;
        done++;        SDL_AtomicSet(&progress, downloadShare + (100 - downloadShare) * done / static_cast<int>(items.size()));

        if(files::exists(target.cStr()))
        {
            skipped++; // (never overwritten silently)
            continue;
        }
        size_t got = 0;
        void* data = mz_zip_reader_extract_to_heap(&z, static_cast<mz_uint>(it.index), &got, 0);
        if(!data)
        {
            notes += "could not unpack ";
            notes += rel;
            notes += "; ";
            continue;
        }
        bool ok = true;
        if(bsp)
        {
            za::String version;
            if(!bspVersionOk(static_cast<const char*>(data), static_cast<za::SizeT>(got), version))
            {
                notes += rel; // (a map this engine cannot load: said, and left out)
                notes += " is a BSP of ";
                notes += version;
                notes += "; ";
                ok = false;
            }
        }
        if(ok)
        {
            files::createDirectories(za::String{files::parentPath(target)}.cStr());
            if(!files::writeBytes(target.cStr(), data, got))
            {
                notes += "could not write ";
                notes += rel;
                notes += "; ";
                ok = false;
            }
        }
        free(data);
        if(!ok)
        {
            continue;
        }
        InstalledFile f;
        f.sha = job.sha;
        f.bytes = it.bytes;
        f.path = za::String{target.substrByPosLen(gameDirName.size() + 1, target.size() - gameDirName.size() - 1)};
        job.wrote.pushBack(ZA_MOVE(f));
        written++;
    }
    mz_zip_reader_end(&z);

    if(written)
    {
        VR_FileCacheForget(); // (the engine's file search caches a directory's listing while a map loads: a BSP written
        // here is invisible to it until the listings are forgotten - vr_fscache.cpp)
    }

    if(written == 0)
    {
        why = notes.size() ? notes : za::String{"the zip holds no file this engine can use"};
        return false;
    }
    job.written = written;
    job.skipped = skipped;
    if(refused)
    {
        notes += za::toString(refused) + " file(s) left out; ";
    }
    if(notes.size() >= 2)
    {
        notes.erase(notes.size() - 2, 2); // (the last "; ")
    }
    why = notes;
    return true;
}

// ---------------------------------------------------------------- the job

// The thread: download, cache, unpack. It touches nothing but `request` (its own copy), its own Job, and the atomics;
// the console line, the registry and the map come from poll().
int run()
{
    za::UniquePtr<Job> job = za::makeUnique<Job>();
    job->sha = request.sha;
    job->title = request.title;
    job->install = request.install;
    job->zipBytes = request.zipBytes;
    job->phase = Phase::Download;

    za::Vector<char> body;
    za::String why;
    bool ok = false;
    if(job->zipBytes > maxZipBytes)
    {
        why = za::String{"the package is "} + za::toString(job->zipBytes / 1024 / 1024) + " MB (up to " +
              za::toString(maxZipBytes / 1024 / 1024) + " MB)";
    }
    else
    {
        const za::U32 d0 = SDL_GetTicks();
        ok = downloadZip(body, why);
        job->downloadMs = static_cast<int>(SDL_GetTicks() - d0);
    }

    if(ok)
    {
        files::createDirectories(cacheDirName.cStr());
        const za::String zp = zipPath(job->sha);
        if(!files::writeBytes(zp.cStr(), body.data(), body.size()))
        {
            Con_DPrintf("map install: could not write %s\n", zp.cStr());
        }
        if(request.install)
        {
            job->phase = Phase::Extract;
            SDL_AtomicSet(&progress, downloadShare);
            const za::U32 e0 = SDL_GetTicks();
            za::String note;
            const bool extracted = extractZip(body, *job, note);
            job->extractMs = static_cast<int>(SDL_GetTicks() - e0);
            if(!extracted)
            {
                ok = false;
                why = note;
            }
            else
            {
                job->message = za::toString(job->written) + " file(s) written";
                if(job->skipped)
                {
                    job->message += ", " + za::toString(job->skipped) + " already there";
                }
                job->message += " (download " + za::toString(job->downloadMs) + " ms, unpack " +
                                za::toString(job->extractMs) + " ms)";
                if(note.size())
                {
                    job->message += "; " + note;
                }
            }
        }
        else
        {
            job->message = za::String{"the zip is in "} + zipPath(job->sha);
        }
    }

    if(!ok)
    {
        job->phase = Phase::Failed;
        job->message = why;
    }
    else
    {
        job->phase = Phase::Done;
        SDL_AtomicSet(&progress, 100);
    }

    {
        za::LockGuard lock{handoff};
        pending = ZA_MOVE(job);
        pendingReady.storeSeqCst(true);
    }
    running.storeSeqCst(false);
    return 0;
}

void ensureStarted()
{
    if(gameDirName.size() || com_numbasedirs <= 0)
    {
        return; // (already set, or the game dirs are not known yet)
    }
    // The game dir the engine searches (COM_AddGameDirectory makes <basedir>/quakevr one): its maps/ is where the
    // engine's `map` command finds a BSP. The cache and the installed list stay with the map index's cache, in the
    // base dir, which does not change when the game dir does.
    gameDirName = com_gamedir;
    mapsDirName = gameDirName + "/maps";
    cacheDirName = za::String{com_basedirs[com_numbasedirs - 1]} + "/cache/maps";
    registryPath = za::String{com_basedirs[com_numbasedirs - 1]} + "/cache/maps_installed.txt";
    loadRegistry();
}

} // namespace

// ---------------------------------------------------------------- public

const za::String& gameDir()
{
    ensureStarted();
    return gameDirName;
}

za::String mapsDir()
{
    ensureStarted();
    return mapsDirName;
}

void start()
{
    ensureStarted();
}

void finish()
{
    SDL_AtomicSet(&cancelJob, 1);
    if(worker.joinable())
    {
        worker.join();
    }
    running.storeSeqCst(false);
}

void poll()
{
    if(!pendingReady.loadSeqCst())
    {
        return;
    }
    za::UniquePtr<Job> taken;
    {
        za::LockGuard lock{handoff};
        taken = ZA_MOVE(pending);
        pendingReady.storeSeqCst(false);
    }
    if(!taken)
    {
        return;
    }
    static_cast<Job&>(current) = ZA_MOVE(*taken); // (the registered job holds the live one: vr_memstats)
    const Job& j = current;
    if(j.phase == Phase::Done)
    {
        Con_SafePrintf("maps: %s - %s\n", j.title.cStr(), j.message.cStr());
        if(j.wrote.size())
        {
            for(const InstalledFile& f : j.wrote)
            {
                registry.files.pushBack(f);
            }
            rebuildPackages();
            if(!writeRegistry())
            {
                Con_DPrintf("map install: could not write %s\n", registryPath.cStr());
            }
            if(j.install)
            {
                play(j.sha); // (the flow the page is for: downloaded, installed, played)
            }
        }
    }
    else
    {
        Con_SafePrintf("maps: %s - %s\n", j.title.cStr(), j.message.cStr());
    }
}

const Job& job()
{
    return current;
}

int percent()
{
    return SDL_AtomicGet(&progress);
}

bool busy(const za::String& sha)
{
    return running.loadSeqCst() && request.sha == sha;
}

bool begin(const mapindex::Entry* entry, bool install)
{
    ensureStarted();
    if(!entry || running.loadSeqCst() || !gameDirName.size())
    {
        return false;
    }
    if(worker.joinable())
    {
        worker.join(); // (a job that finished; its handle was never joined)
    }
    request.sha = za::String{mapindex::index().field(entry->sha256)};
    request.title = za::String{mapindex::index().field(entry->title)};
    request.urls = za::String{mapindex::index().field(entry->urls)};
    request.zipBytes = entry->bytes;
    request.install = install;
    SDL_AtomicSet(&cancelJob, 0);
    SDL_AtomicSet(&progress, 0);
    running.storeSeqCst(true);
    worker = za::Thread(run);
    return true;
}

void cancel()
{
    SDL_AtomicSet(&cancelJob, 1);
}

bool cached(const za::String& sha, za::String& out)
{
    out = zipPath(sha);
    return files::exists(out.cStr());
}

bool installed(const za::String& sha)
{
    ensureStarted();
    if(!registryLoaded)
    {
        loadRegistry();
    }
    for(const InstalledFile& f : registry.files)
    {
        if(f.sha == sha)
        {
            return true;
        }
    }
    return false;
}

const za::Vector<Installed>& installedList()
{
    ensureStarted();
    if(!registryLoaded)
    {
        loadRegistry();
    }
    return registry.packages;
}

za::U64 installedBytes(const za::String& sha)
{
    for(const Installed& p : installedList())
    {
        if(p.sha == sha)
        {
            return p.bytes;
        }
    }
    return 0;
}

bool uninstall(const za::String& sha)
{
    if(!installed(sha))
    {
        return false;
    }
    int removed = 0;
    za::Vector<InstalledFile> keep;
    for(const InstalledFile& f : registry.files)
    {
        if(f.sha == sha)
        {
            if(files::remove((gameDirName + "/" + f.path).cStr()))
            {
                removed++;
            }
        }
        else
        {
            keep.pushBack(f);
        }
    }
    registry.files = ZA_MOVE(keep);
    rebuildPackages();
    writeRegistry();
    VR_FileCacheForget(); // (a map removed from maps/: the listings forget it too)
    Con_SafePrintf("maps: removed %d file(s).\n", removed);
    return true;
}

// The package's startmap, started now (the page's Play action, and what a finished install does).
bool play(const za::String& sha)
{
    ensureStarted();
    const mapindex::Entry* e = mapindex::find(sha);
    if(!e)
    {
        Con_Printf("maps: the index no longer has that package.\n");
        return false;
    }
    const za::String name = mapindex::index().field(e->startmap);
    if(!name.size())
    {
        Con_Printf("maps: %s does not say which map to start.\n", mapindex::index().field(e->title));
        return false;
    }
    // Cbuf_InsertText, not AddText: this runs next. AddText would put it after the commands already queued (a test
    // script's `screenshot; quit` would be run first, and the map never started).
    Cbuf_InsertText((za::String{"map "} + name + "\n").cStr());
    return true;
}

// ---------------------------------------------------------------- the console

// A byte count as the console and the page show it ("5.1 MB"), as vr_mapindex.cpp's own helper does.
za::String formatBytes(za::U64 bytes)
{
    char out[32];
    if(bytes >= 1024ull * 1024)
    {
        q_snprintf(out, sizeof(out), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    }
    else if(bytes >= 1024)
    {
        q_snprintf(out, sizeof(out), "%.0f KB", static_cast<double>(bytes) / 1024.0);
    }
    else
    {
        q_snprintf(out, sizeof(out), "%llu B", static_cast<unsigned long long>(bytes));
    }
    return za::String{out};
}

// A sha prefix, as maps_info takes it.
const mapindex::Entry* argEntry(const char* context, const char* argv0)
{
    const char* arg = argv0 ? argv0 : "";
    if(!*arg)
    {
        Con_Printf("%s <sha256> - see maps_list.\n", context);
        return nullptr;
    }
    const mapindex::Entry* e = mapindex::find(za::String{arg});
    if(!e)
    {
        Con_Printf("%s: no package starts with %s.\n", context, arg);
    }
    return e;
}

void get_f()
{
    const mapindex::Entry* e = argEntry("maps_get", Cmd_Argv(1));
    if(e && begin(e, false))
    {
        Con_SafePrintf("maps: downloading %s (%s)...\n", mapindex::index().field(e->title),
                       formatBytes(e->bytes).cStr());
    }
}

void install_f()
{
    const mapindex::Entry* e = argEntry("maps_install", Cmd_Argv(1));
    if(e && begin(e, true))
    {
        Con_SafePrintf("maps: downloading and installing %s (%s)...\n", mapindex::index().field(e->title),
                       formatBytes(e->bytes).cStr());
    }
}

void installed_f()
{
    const za::Vector<Installed>& list = installedList();
    if(list.empty())
    {
        Con_Printf("maps: nothing installed. (maps_install <sha256>)\n");
        return;
    }
    Con_SafePrintf("maps: %d package(s) installed in %s\n", static_cast<int>(list.size()), gameDir().cStr());
    for(const Installed& p : list)
    {
        Con_SafePrintf("  %s  %8s  %d file(s)  %s\n", p.title.cStr(), formatBytes(p.bytes).cStr(),
                       p.files, p.sha.cStr());
    }
}

void uninstall_f()
{
    const mapindex::Entry* e = argEntry("maps_uninstall", Cmd_Argv(1));
    if(e)
    {
        uninstall(za::String{mapindex::index().field(e->sha256)});
    }
}

void play_f()
{
    const mapindex::Entry* e = argEntry("maps_play", Cmd_Argv(1));
    if(e)
    {
        play(za::String{mapindex::index().field(e->sha256)});
    }
}

void registerCommands()
{
    Cmd_AddCommand("maps_get", get_f);
    Cmd_AddCommand("maps_install", install_f);
    Cmd_AddCommand("maps_play", play_f);
    Cmd_AddCommand("maps_installed", installed_f);
    Cmd_AddCommand("maps_uninstall", uninstall_f);
}

} // namespace qvr::mapinstall
