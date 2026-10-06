// vr_mapinstall.cpp -- a package from the map index (vr_mapindex.cpp) put into the game: its zip downloaded to
// <base>/cache/maps/<sha256>.zip, its files unpacked with miniz into its own folder, <base>/qvr_addons/<sha16>/, and
// what was written recorded in <base>/cache/maps_installed.txt so that it can be listed and removed again (<base>:
// com_basedirs' last, the user's base dir). Installing never starts a map: play() does (vr_mapinstall.hpp).
//
// The download and the unpacking run on a thread of their own (as the index's fetch does): the game never waits for
// them, and nothing is printed from that thread. poll() takes the finished job on the main thread: its console line,
// and the registry updated (or what it unpacked removed again, when it did not finish; finish() does the same for a
// job the quit stopped).
//
// Where the files go inside the package's folder: a BSP to maps/ (host_cmd.c's Cmd_Map_f builds "maps/%s.bsp"); the
// rest where the index's install.extract puts the zip's root, its game folder dropped (extractLayout). The folder is
// on the search path only while the package is played (mountActive, from vr_gamedir.cpp).

#include "vr_mapinstall.hpp"
#include "vr_cvars.hpp" // vr_maps_cache_mb: the download cache's cap
#include "vr_engine.hpp"
#include "vr_api.h" // VR_FileCacheForget: the engine's directory listings, told that files appeared
#include "vr_files.hpp"
#include "vr_mem.hpp"
#include "vr_sha256.hpp"
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::mapinstall
{
namespace
{

constexpr char registryMagic[] = "#quakevr-maps-installed-2"; // paths in the package's own folder
constexpr char registryMagicMerged[] = "#quakevr-maps-installed-1"; // paths in the game dir (moved at load, once)
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
// The mapindex::generation() the packages' titles were read from: the registry is read at start-up, before any index
// has arrived, and its titles (the sha's start until then) are read again when one does (poll()).
za::U32 titlesGeneration = 0;

// The paths, set at start() (com_basedirs' last: the user's game dir, as vr_mapindex.cpp's cache path).
za::String gameDirName; // com_gamedir when first asked: where the old layout (registryMagicMerged) put the files
za::String cacheDirName;
za::String registryPath;
// The same as a reference (addonsRoot), built on first use once the base dirs are known: file scope, not hidden in
// the function (docs/vr-port/CODE_STYLE.md, 'Scratch buffers and caches').
za::String addonsRootName;

// The map packages' own folders: <user base dir>/qvr_addons/<the sha256's first 16>/, each laid out as a game dir
// (maps/, gfx/, sound/, ...). Never on the search path but the active one's (mountActive): one package's files never
// meet another's, nor the stock game's when it is played, and Quake VR's own always come first.
[[nodiscard]] const za::String& addonsRoot()
{
    if(addonsRootName.empty() && com_numbasedirs > 0)
    {
        addonsRootName = za::String{com_basedirs[com_numbasedirs - 1]} + "/qvr_addons";
    }
    return addonsRootName;
}

[[nodiscard]] za::String addonDir(const za::String& sha)
{
    return addonsRoot() + "/" + za::String{za::StringView{sha}.substrByPosLen(0, za::min(sha.size(), za::SizeT{16}))};
}

// The package whose folder is mounted (main thread; "" : none, the stock game's folders alone), and the one Play just
// asked for (its `map` command keeps it whatever the stock game has of the same name).
za::String activeSha;
za::String playPending;
// Why the last Play could not start its package (the page shows it under the package; "" : it started).
za::String playProblemSha;
za::String playProblemText;
const za::String noPlayProblem;
bool baseOnly = false; // VR_SkipSearchPath: the packages' folders skipped (the stock game asked alone)

// The handoff (vr_mapindex.cpp's shape): the thread fills `pending`, poll() takes it.
za::AtomicMutex handoff;
za::UniquePtr<Job> pending;
za::Atomic<bool> pendingReady{false};
za::Thread worker;
za::Atomic<bool> jobRunning{false};
SDL_atomic_t cancelJob{}; // (the Download API's abort flag; not named `cancel`: this module has a cancel() too)
SDL_atomic_t progress{}; // 0..100, read while the job runs
// What the running job is doing, published by its thread for poll() and the page (the Job itself is the thread's own
// until it is handed off): its phase, the zip's bytes so far, the mirror tried (1-based) of how many, and why it was
// cancelled (a Cancel reason; 0: it was not).
SDL_atomic_t livePhase{};
SDL_atomic_t liveBytes{};
SDL_atomic_t liveMirror{};
SDL_atomic_t liveMirrors{};
SDL_atomic_t cancelReason{};

enum CancelReason
{
    CancelNone,
    CancelUser,    // the page's Cancel, maps_cancel
    CancelStalled, // nothing arrived for stallSeconds (the watchdog in poll())
    CancelQuit,    // the game quitting
};

// The watchdog: a download that received nothing for this long is cancelled (curl's own low-speed limit, in
// host_cmd.c's Download, catches a stalled transfer too; this also covers a mirror that never answers at all).
constexpr double stallSeconds = 45.0;
// Main thread: when the job began, and when its bytes last moved (the watchdog's clock).
double jobStarted = 0.0;
double lastMoved = 0.0;
int lastBytes = -1;
int lastMirror = 0;

void requestCancel(CancelReason reason)
{
    SDL_AtomicCAS(&cancelReason, CancelNone, reason);
    SDL_AtomicSet(&cancelJob, 1);
}

[[nodiscard]] const char* cancelText()
{
    switch(SDL_AtomicGet(&cancelReason))
    {
        case CancelUser: return "cancelled";
        case CancelStalled: return "timed out (nothing arrived for 45 s)";
        case CancelQuit: return "cancelled (the game quit)";
        default: return "cancelled";
    }
}

// The job asked for, copied out of the index before the thread starts: a fetch that finishes meanwhile replaces the
// index and every Entry in it.
struct Request
{
    za::String sha, title, urls; // the mirrors, mapindex::partSep separated
    za::String extract;          // install.extract: where the zip's root goes ("{base}/id1/maps/", "{base}/", ...)
    za::String root;             // the package's own folder the files go to (addonDir)
    za::U64 zipBytes{0};
    bool install{true};
    bool wasInstalled{false}; // the registry names it already (otherwise its folder holds nothing of ours yet)
};
Request request;

[[nodiscard]] za::String zipPath(const za::String& sha)
{
    return cacheDirName + "/" + sha + ".zip";
}

// ---------------------------------------------------------------- the installed list

bool writeRegistry();
void rebuildPackages();
void migrateMerged();
bool safePath(const char* name, za::String& out);

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
    bool merged = false; // the old layout: every package's files in the game dir itself
    bool known = false;
    files::forLines(text, [&](za::StringView l)
    {
        if(line++ == 0)
        {
            merged = l == za::StringView{registryMagicMerged};
            known = merged || l == za::StringView{registryMagic};
            return;
        }
        if(!known || l.empty())
        {
            return;
        }
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
        // (a line edited by hand, or damaged, is dropped: the sha names a folder uninstall removes, the path a file)
        za::String checked;
        if(mapindex::validSha(f.sha) && safePath(f.path.cStr(), checked) && checked == f.path)
        {
            registry.files.pushBack(ZA_MOVE(f));
        }
    });
    if(merged)
    {
        migrateMerged();
    }
    rebuildPackages();
}

// The old layout (every package's files written into the game dir itself, where they could meet each other's and
// override the stock game's for every map) moved to the packages' own folders, once: each file the list names is
// moved to the same path in its package's folder; one no longer there is dropped from the list.
void migrateMerged()
{
    int moved = 0, missing = 0;
    za::Vector<InstalledFile> kept;
    for(InstalledFile& f : registry.files)
    {
        const za::String from = gameDirName + "/" + f.path;
        const za::String to = addonDir(f.sha) + "/" + f.path;
        if(!files::isFile(from.cStr()))
        {
            missing++;
            continue;
        }
        files::createDirectories(za::String{files::parentPath(to)}.cStr());
        if(files::rename(from.cStr(), to.cStr()))
        {
            moved++;
            kept.pushBack(ZA_MOVE(f));
        }
        else
        {
            Con_Printf("maps: could not move %s to %s\n", from.cStr(), to.cStr());
        }
    }
    registry.files = ZA_MOVE(kept);
    writeRegistry();
    VR_FileCacheForget();
    Con_Printf("maps: the installed maps moved to their own folders in %s (%d file(s); %d were gone already)\n",
        addonsRoot().cStr(), moved, missing);
}

void rebuildPackages()
{
    titlesGeneration = mapindex::generation();
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

// A path segment every file system writes as named: no separator, drive colon or character Windows refuses
// (<>:"|?*, a control character), no trailing dot or space (Windows drops them: "maps/.. /x" made a folder ".. " that
// Explorer cannot remove, "x. " was written as "x" and recorded as "x. "), and no device name (CON, NUL, AUX.txt, COM1:
// a file Windows does not write, or one that cannot be removed again).
[[nodiscard]] bool portableSegment(const za::String& part)
{
    for(const char c : part)
    {
        if(static_cast<unsigned char>(c) < ' ' || strchr("<>:\"|?*\\", c))
        {
            return false;
        }
    }
    const char last = part.back();
    if(last == '.' || last == ' ')
    {
        return false;
    }
    const za::SizeT dot = part.findFirstOf('.');
    const za::String stem = dot == za::StringView::nPos ? part : za::String{part.substrByPosLen(0, dot)};
    static constexpr const char* devices[] = {"con", "prn", "aux", "nul", "conin$", "conout$"};
    for(const char* d : devices)
    {
        if(!q_strcasecmp(stem.cStr(), d))
        {
            return false;
        }
    }
    const bool numbered = stem.size() == 4 && (!q_strncasecmp(stem.cStr(), "com", 3) || !q_strncasecmp(stem.cStr(), "lpt", 3)) &&
                          stem[3] >= '0' && stem[3] <= '9';
    return !numbered;
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
        if(part.empty() || part == "." || part == ".." || !portableSegment(part))
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

// A folder a game dir holds its assets in: a zip's path starting with one is laid out from a game dir already.
[[nodiscard]] bool assetFolder(const za::String& first)
{
    static constexpr const char* names[] = {"maps", "gfx", "sound", "progs", "textures", "music", "env", "lits",
        "locs", "particles", "scripts", "shaders", "sprites", "skins", "models"};
    for(const char* n : names)
    {
        if(!q_strcasecmp(first.cStr(), n))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] za::String firstSegment(const za::String& p)
{
    const za::SizeT slash = p.findFirstOf('/');
    return slash == za::StringView::nPos ? za::String{} : za::String{p.substrByPosLen(0, slash)};
}

// Where the zip's root goes, from the index's install.extract ("{base}/id1/maps/", "{base}/ad/", "{base}/", ""): its
// game folder (id1, ad, copper, quoth, ...: every one is quakevr here, the VR progs being what plays them) dropped, the
// rest kept as the prefix inside our game dir ("maps" for "{base}/id1/maps/", "" for "{base}/ad/"). `perEntryGame`:
// "{base}/" (or none given): each entry starts with its own game folder ("vanisch01/maps/...": a mod's folder, made
// to be run with -game), dropped entry by entry.
void extractLayout(const za::String& extract, za::String& prefix, bool& perEntryGame)
{
    za::String rest = extract;
    if(rest.size() >= 6 && !q_strncasecmp(rest.cStr(), "{base}", 6))
    {
        rest.erase(0, 6);
    }
    za::Vector<za::String> parts;
    za::SizeT i = 0;
    while(i <= rest.size())
    {
        za::SizeT j = i;
        while(j < rest.size() && rest.cStr()[j] != '/' && rest.cStr()[j] != '\\')
        {
            j++;
        }
        if(j > i)
        {
            parts.pushBack(za::String{rest.substrByPosLen(i, j - i)});
        }
        i = j + 1;
    }
    prefix = za::String{};
    perEntryGame = parts.empty();
    for(za::SizeT p = 1; p < parts.size(); p++)
    {
        if(prefix.size())
        {
            prefix += "/";
        }
        prefix += parts[p];
    }
}

// The one folder every entry is under (a zip built around a folder: the index's zipbasedir tag). "" : none, or one of
// a game dir's own asset folders (a zip of maps/ alone is laid out from the game dir, not wrapped).
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
    return assetFolder(top) ? za::String{} : top;
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

// The download passed maxZipBytes (the job thread's own: writeChunk runs on it, inside Download).
bool downloadTooBig = false;

size_t writeChunk(void* buffer, size_t size, size_t nmemb, void* stream)
{
    if(SDL_AtomicGet(&cancelJob))
    {
        return 0; // (the transfer stops: as vr_mapindex.cpp's and host_cmd.c's do)
    }
    za::Vector<char>& body = *static_cast<za::Vector<char>*>(stream);
    const za::SizeT n = size * nmemb;
    if(static_cast<za::U64>(body.size()) + n > maxZipBytes)
    {
        downloadTooBig = true; // (the index's size is checked before; a server that sends more is stopped here)
        return 0;
    }
    body.reserveMore(n);
    body.unsafeEmplaceBackRange(static_cast<const char*>(buffer), n);
    SDL_AtomicSet(&liveBytes, static_cast<int>(za::min(static_cast<za::U64>(body.size()), static_cast<za::U64>(0x7fffffff))));
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
    SDL_AtomicSet(&liveMirrors, static_cast<int>(urls.size()));
    za::String tried; // each mirror's failure, for the message when all of them failed
    int mirror = 0;
    for(const za::String& url : urls)
    {
        mirror++;
        if(SDL_AtomicGet(&cancelJob))
        {
            why = cancelText();
            return false;
        }
        SDL_AtomicSet(&liveMirror, mirror);
        SDL_AtomicSet(&liveBytes, 0);
        body.clear();
        download_t dl{};
        dl.write_fn = writeChunk;
        dl.write_data = &body;
        dl.abort = &cancelJob;
        downloadTooBig = false;
        const bool ok = Download(url.cStr(), &dl);
        if(SDL_AtomicGet(&cancelJob))
        {
            why = cancelText();
            return false;
        }
        if(downloadTooBig)
        {
            body.clear();
            why = za::String{"the download passed "} + za::toString(maxZipBytes / 1024 / 1024) + " MB (the index says " +
                  formatBytes(request.zipBytes) + "): stopped";
            return false;
        }
        za::String failed;
        if(!ok)
        {
            failed = dl.error ? za::String{dl.error}
                              : za::String{"HTTP "} + za::toString(dl.response ? dl.response : 0);
        }
        else if(body.empty())
        {
            failed = "it came back empty";
        }
        else
        {
            // The zip against the index's sha256 (the hash of the zip's bytes, the package's identifier): a corrupted
            // download, or a file changed on the server, is neither kept nor unpacked; the next mirror is tried.
            const sha256::Digest got = sha256::of(body.data(), body.size());
            if(sha256::matches(got, request.sha.cStr()))
            {
                return true;
            }
            char hex[65];
            sha256::toHex(got, hex);
            body.clear();
            failed = za::String{"its sha256 is "} + za::String{hex, 16} + "..., not the index's " +
                     za::String{request.sha.cStr(), za::min(request.sha.size(), za::SizeT{16})} +
                     "... (a corrupted download or a changed file): not unpacked";
        }
        if(tried.size())
        {
            tried += "; ";
        }
        tried += "mirror " + za::toString(mirror) + ": " + failed;
    }
    why = urls.size() > 1 ? za::String{"every mirror failed ("} + tried + ")" : tried;
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
// The game code a mod carries (a zip's or a pak's file): a package holding one is refused.
[[nodiscard]] bool gameCode(const za::String& path)
{
    const za::String leaf = baseName(path);
    return !q_strcasecmp(leaf.cStr(), "progs.dat") || !q_strcasecmp(leaf.cStr(), "qwprogs.dat") ||
           !q_strcasecmp(leaf.cStr(), "csprogs.dat") || !q_strcasecmp(leaf.cStr(), "progs.lno");
}

// The startup scripts and settings a mod's folder carries: never installed (they are not run from a package's folder,
// and are not the game's to take).
[[nodiscard]] bool startupConfig(const za::String& path)
{
    const za::String leaf = baseName(path);
    return !q_strcasecmp(leaf.cStr(), "quake.rc") || !q_strcasecmp(leaf.cStr(), "autoexec.cfg") ||
           !q_strcasecmp(leaf.cStr(), "default.cfg") || !q_strcasecmp(leaf.cStr(), "config.cfg");
}

// "pak3.pak" at a package's root: 3 (the order the engine reads them in, a later one over an earlier); -1 otherwise.
[[nodiscard]] int rootPakNumber(const za::String& placed)
{
    if(placed.findFirstOf('/') != za::StringView::nPos || placed.size() < 8 || q_strncasecmp(placed.cStr(), "pak", 3) ||
       !endsFolded(placed, ".pak"))
    {
        return -1;
    }
    int n = 0;
    for(za::SizeT i = 3; i + 4 < placed.size(); i++)
    {
        const char c = placed.cStr()[i];
        if(c < '0' || c > '9')
        {
            return -1;
        }
        n = n * 10 + (c - '0');
    }
    return n;
}

// A map's own files (its BSP and the files named after it beside it) under a lower-case name: `map` is typed, and
// the index names startmaps, in lower case, and Linux's file names are not folded.
[[nodiscard]] za::String mapFileName(const za::String& placed)
{
    if(za::StringView{placed}.substrByPosLen(0, za::min(placed.size(), za::SizeT{5})) != za::StringView{"maps/"} ||
       za::String{placed.substrByPosLen(5, placed.size() - 5)}.findFirstOf('/') != za::StringView::nPos)
    {
        return placed;
    }
    if(!endsFolded(placed, ".bsp") && !endsFolded(placed, ".lit") && !endsFolded(placed, ".ent") &&
       !endsFolded(placed, ".vis") && !endsFolded(placed, ".lux"))
    {
        return placed;
    }
    za::String out = placed;
    for(za::SizeT i = 5; i < out.size(); i++)
    {
        out[i] = static_cast<char>(tolower(static_cast<unsigned char>(out[i])));
    }
    return out;
}

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
    int configs = 0;
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
        if(!it.dir && gameCode(it.path))
        {
            // A mod, not a map package: its own game code would take the place of Quake VR's.
            why = za::String{"it carries its own game code ("} + it.path + "): a mod Quake VR cannot play";
            mz_zip_reader_end(&z);
            return false;
        }
        if(!it.dir && startupConfig(it.path))
        {
            configs++; // (never run: a package's settings are not the game's)
            continue;
        }
        unpacked += it.bytes;
        if(!it.dir)
        {
            paths.pushBack(it.path); // (a folder's own entry, "vanisch01/" read as "vanisch01", is no file at the root)
        }
        items.pushBack(ZA_MOVE(it));
    }
    if(unpacked > maxUnpackedBytes)
    {
        why = za::String{"its files would unpack to "} + za::toString(unpacked / 1024 / 1024) + " MB (up to " +
              za::toString(maxUnpackedBytes / 1024 / 1024) + " MB)";
        mz_zip_reader_end(&z);
        return false;
    }

    za::String prefix;
    bool perEntryGame = false;
    extractLayout(request.extract, prefix, perEntryGame);
    // A single folder around everything (the zipbasedir tag), stripped where the entries do not each carry their game
    // folder anyway.
    const za::String top = perEntryGame ? za::String{} : commonTopDir(paths);

    int written = 0;
    int skipped = 0;
    int done = 0;
    za::String notes;
    const za::String& root = request.root; // the package's own folder (every path below is in it)

    // One file into the package's folder: `overwrite` for a pak's (the engine reads a pak over the folder's loose
    // files, and a later pak over an earlier one); otherwise one already there (the same path twice in a zip) is kept.
    const auto place = [&](const za::String& placed, const void* data, za::SizeT size, bool overwrite) -> bool
    {
        const za::String target = root + "/" + placed;
        if(!overwrite && files::exists(target.cStr()))
        {
            skipped++;
            return false;
        }
        if(endsFolded(placed, ".bsp"))
        {
            za::String version;
            if(!bspVersionOk(static_cast<const char*>(data), size, version))
            {
                notes += placed; // (a map this engine cannot load: said, and left out)
                notes += " is a BSP of ";
                notes += version;
                notes += "; ";
                return false;
            }
        }
        if(!files::createDirectories(za::String{files::parentPath(target)}.cStr()))
        {
            notes += "could not make the folder for "; // (e.g. a file of that name in the way)
            notes += placed;
            notes += "; ";
            return false;
        }
        if(!files::writeBytes(target.cStr(), data, size))
        {
            notes += "could not write ";
            notes += placed;
            notes += "; ";
            return false;
        }
        for(InstalledFile& w : job.wrote)
        {
            if(w.path == placed)
            {
                w.bytes = size; // (a pak's file over a loose one: the same path, recorded once)
                return true;
            }
        }
        InstalledFile f;
        f.sha = job.sha;
        f.bytes = size;
        f.path = placed;
        job.wrote.pushBack(ZA_MOVE(f));
        written++;
        return true;
    };

    // The packs at the package's root (pak0.pak, ...): their files unpacked after the loose ones, in order.
    struct Pak
    {
        int number{0};
        void* data{nullptr};
        size_t size{0};
    };
    za::Vector<Pak> paks;
    const auto freePaks = [&]
    {
        for(Pak& k : paks)
        {
            free(k.data);
        }
        paks.clear();
    };

    for(const Item& it : items)
    {
        if(SDL_AtomicGet(&cancelJob))
        {
            why = cancelText();
            freePaks();
            mz_zip_reader_end(&z);
            return false;
        }
        za::String rel = it.path;
        if(!top.empty() && rel.size() > top.size() + 1)
        {
            rel.erase(0, top.size() + 1);
        }
        else if(perEntryGame)
        {
            // "vanisch01/gfx/env/x.tga": its game folder dropped, unless it is laid out from a game dir already
            // ("maps/x.bsp"); a file at the root (a readme) stays there.
            const za::String first = firstSegment(rel);
            if(first.size() && !assetFolder(first))
            {
                rel.erase(0, first.size() + 1);
            }
        }
        if(rel.empty())
        {
            refused++;
            continue;
        }
        done++;
        SDL_AtomicSet(&progress, downloadShare + (100 - downloadShare) * done / static_cast<int>(items.size()));
        if(it.dir)
        {
            continue; // (a folder's own entry, "gfx/": its files' folders are made as they are written)
        }
        // A BSP to maps/, under its own name only: that is where the engine's `map` command looks. Everything else
        // where the index says the zip's root goes (maps/ for most single maps: a .lit beside its BSP), keeping its
        // path (textures and sounds are named from the game dir).
        const za::String placed = mapFileName(endsFolded(rel, ".bsp")
                ? za::String{"maps/"} + baseName(rel)
                : prefix.size() && !(firstSegment(rel).size() && assetFolder(firstSegment(rel))) ? prefix + "/" + rel
                                                                                                    : rel);
        const int pakNumber = rootPakNumber(placed);
        if(pakNumber < 0 && endsFolded(placed, ".pak"))
        {
            notes += placed; // (a pak the engine would never read from where it is)
            notes += " left out (not at the package's root); ";
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
        if(pakNumber >= 0)
        {
            paks.pushBack(Pak{pakNumber, data, got}); // (unpacked below, after every loose file)
            continue;
        }
        place(placed, data, static_cast<za::SizeT>(got), false);
        free(data);
    }
    mz_zip_reader_end(&z);

    // The packs, unpacked into loose files of the package's folder (a pak in a package's folder would not be read:
    // the engine reads paks only from the game dirs it adds itself). Refused whole if one carries game code.
    za::heapSort(paks.begin(), paks.end(), [](const Pak& x, const Pak& y) { return x.number < y.number; });
    for(const Pak& k : paks)
    {
        const char* d = static_cast<const char*>(k.data);
        za::I32 dirOfs = 0, dirLen = 0;
        if(k.size < 12 || memcmp(d, "PACK", 4))
        {
            notes += "pak" + za::toString(k.number) + ".pak is not a pak; ";
            continue;
        }
        memcpy(&dirOfs, d + 4, 4);
        memcpy(&dirLen, d + 8, 4);
        if(dirOfs < 12 || dirLen < 0 || dirLen % 64 || static_cast<za::U64>(dirOfs) + static_cast<za::U64>(dirLen) > k.size)
        {
            notes += "pak" + za::toString(k.number) + ".pak is damaged; ";
            continue;
        }
        for(za::I32 e = 0; e < dirLen / 64; e++)
        {
            char raw[57] = {};
            memcpy(raw, d + dirOfs + e * 64, 56);
            if(gameCode(za::String{raw}))
            {
                why = za::String{"its pak"} + za::toString(k.number) + ".pak carries its own game code (" + raw +
                      "): a mod Quake VR cannot play";
                freePaks();
                return false;
            }
        }
        for(za::I32 e = 0; e < dirLen / 64; e++)
        {
            if(SDL_AtomicGet(&cancelJob))
            {
                why = cancelText();
                freePaks();
                return false;
            }
            char raw[57] = {};
            za::I32 pos = 0, len = 0;
            memcpy(raw, d + dirOfs + e * 64, 56);
            memcpy(&pos, d + dirOfs + e * 64 + 56, 4);
            memcpy(&len, d + dirOfs + e * 64 + 60, 4);
            za::String name;
            if(pos < 0 || len < 0 || static_cast<za::U64>(pos) + static_cast<za::U64>(len) > k.size ||
               !safePath(raw, name) || skippedFile(name))
            {
                refused++;
                continue;
            }
            if(startupConfig(name))
            {
                configs++;
                continue;
            }
            place(mapFileName(name), d + pos, static_cast<za::SizeT>(len), true);
        }
    }
    freePaks();
    if(configs)
    {
        notes += za::toString(configs) + " config file(s) left out (quake.rc, *.cfg); ";
    }

    if(written)
    {
        VR_FileCacheForget(); // (the engine's file search caches a directory's listing while a map loads: a BSP written
        // here is invisible to it until the listings are forgotten - vr_fscache.cpp)
    }

    if(written == 0 && skipped == 0) // (every file there already - a package installed again - is an install that worked)
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

// ---------------------------------------------------------------- the download cache

// <base>/cache/maps/<sha256>.zip: each package's zip as it was downloaded, kept up to vr_maps_cache_mb, the oldest (by
// its last write) removed first: before a download (room made for it), when a job is taken, on the first frame (the
// config read by then: a cache over the cap at start-up is trimmed) and when the cap changes. Only the names this
// module gives its zips (64 hex digits and ".zip") are ever counted or removed: anything else in the folder is left
// alone. The running job's zip is never removed (its thread writes it once the download is done).

struct CachedZip
{
    za::String sha;
    za::U64 bytes{0};
    za::I64 stamp{0}; // its last write (files::lastWriteTime)
};

// What a trim did (its console line).
struct CacheTrim
{
    int removed{0};
    za::U64 freed{0};
    za::U64 left{0}; // the zips' bytes after it
};

// The vr_maps_cache_mb the cache was last trimmed to (poll(): the first frame, then a change trims at once).
float trimmedCapMb = -1.f;

// A name this module gives a zip, its sha out: 64 hex digits and ".zip" (zipPath), nothing else.
[[nodiscard]] bool cachedZipName(const char* name, za::String& sha)
{
    constexpr za::SizeT shaLength = 64;
    if(strlen(name) != shaLength + 4 || strcmp(name + shaLength, ".zip") != 0)
    {
        return false;
    }
    for(za::SizeT i = 0; i < shaLength; i++)
    {
        if(!isxdigit(static_cast<unsigned char>(name[i])))
        {
            return false;
        }
    }
    sha = za::String{name, shaLength};
    return true;
}

// The cache's zips, oldest first; `others`: the entries in the folder that are not (left alone).
void listCache(za::Vector<CachedZip>& out, int* others)
{
    out.clear();
    if(cacheDirName.empty() || !files::isDirectory(cacheDirName.cStr()))
    {
        return;
    }
    files::forEachEntry(cacheDirName.cStr(),
        [&](const char* name, bool isDirectory)
        {
            za::String sha;
            if(isDirectory || !cachedZipName(name, sha))
            {
                if(others)
                {
                    (*others)++;
                }
                return;
            }
            const za::String path = zipPath(sha);
            out.pushBack(CachedZip{sha, files::fileSize(path.cStr()), files::lastWriteTime(path.cStr())});
        });
    za::quickSort(out.begin(), out.end(),
        [](const CachedZip& a, const CachedZip& b)
        { return a.stamp != b.stamp ? a.stamp < b.stamp : strcmp(a.sha.cStr(), b.sha.cStr()) < 0; });
}

[[nodiscard]] za::U64 cacheCapBytes()
{
    const double mb = static_cast<double>(vr_maps_cache_mb.value);
    return mb > 0.0 ? static_cast<za::U64>(mb * 1024.0 * 1024.0) : 0;
}

// The oldest zips removed until the rest fit in `budget` bytes. Never `keep` (a sha, or "") nor the running job's.
CacheTrim trimCache(za::U64 budget, const za::String& keep)
{
    CacheTrim t;
    za::Vector<CachedZip> zips;
    listCache(zips, nullptr);
    za::U64 total = 0;
    for(const CachedZip& z : zips)
    {
        total += z.bytes;
    }
    const za::String running = jobRunning.loadSeqCst() ? request.sha : za::String{};
    for(const CachedZip& z : zips)
    {
        if(total <= budget)
        {
            break;
        }
        if(z.sha == keep || z.sha == running)
        {
            continue;
        }
        if(files::remove(zipPath(z.sha).cStr()))
        {
            total -= z.bytes;
            t.removed++;
            t.freed += z.bytes;
        }
    }
    t.left = total;
    return t;
}

void reportTrim(const CacheTrim& t, const char* when)
{
    if(t.removed)
    {
        Con_SafePrintf("maps: download cache (%s): %d old zip(s) removed, %s freed, %s kept (vr_maps_cache_mb %g)\n",
            when, t.removed, formatBytes(t.freed).cStr(), formatBytes(t.left).cStr(), vr_maps_cache_mb.value);
    }
}

// poll(): the first frame (the config read by then) and a changed cap trim the cache to it.
void trimCacheIfCapChanged()
{
    if(cacheDirName.empty() || vr_maps_cache_mb.value == trimmedCapMb)
    {
        return;
    }
    const bool first = trimmedCapMb < 0.f;
    trimmedCapMb = vr_maps_cache_mb.value;
    reportTrim(trimCache(cacheCapBytes(), za::String{}), first ? "start-up" : "its size changed");
}

// begin(): room made for the zip about to be downloaded (its own sha's left: the same file, written over).
void trimCacheForDownload(const za::String& sha, za::U64 zipBytes)
{
    const za::U64 cap = cacheCapBytes();
    reportTrim(trimCache(cap > zipBytes ? cap - zipBytes : 0, sha), "room for the download");
}

// takeFinished(): a job that failed keeps no zip (a broken or partly written download, or one whose unpacking failed);
// one that finished leaves the cache to the cap (installed with a cap of 0: its zip goes too). maps_get's zip is that
// job's whole result: kept until the next trim.
void settleCacheAfterJob(const Job& j)
{
    trimmedCapMb = vr_maps_cache_mb.value;
    if(j.phase != Phase::Done)
    {
        const za::String zp = zipPath(j.sha);
        if(files::isFile(zp.cStr()) && files::remove(zp.cStr()))
        {
            Con_SafePrintf("maps: %s - its zip was not kept (the job did not finish)\n", j.title.cStr());
        }
        return;
    }
    reportTrim(trimCache(cacheCapBytes(), j.install ? za::String{} : j.sha), "after the job");
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
        // (said in the job's message, not printed: this is the job's thread, and the console is the main thread's)
        const bool zipWritten = files::writeBytes(zp.cStr(), body.data(), body.size());
        if(request.install)
        {
            job->phase = Phase::Extract;
            SDL_AtomicSet(&livePhase, static_cast<int>(Phase::Extract));
            SDL_AtomicSet(&progress, downloadShare);
            const za::U32 e0 = SDL_GetTicks();
            if(!request.wasInstalled)
            {
                // Whatever an unpacking that never finished left (the game killed in the middle of one): the folder is
                // this package's own, and the registry names nothing in it, so it starts empty.
                files::removeAll(request.root.cStr());
            }
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
                if(!zipWritten)
                {
                    job->message += "; its zip could not be kept in " + zp;
                }
            }
        }
        else if(zipWritten)
        {
            job->message = za::String{"the zip is in "} + zp;
        }
        else
        {
            ok = false; // (maps_get: the zip was the whole job)
            why = za::String{"could not write "} + zp;
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
    jobRunning.storeSeqCst(false);
    return 0;
}

void ensureStarted()
{
    if(gameDirName.size() || com_numbasedirs <= 0)
    {
        return; // (already set, or the game dirs are not known yet)
    }
    // The cache and the installed list stay with the map index's cache, in the user's base dir (as qvr_addons/ does),
    // which does not change when the game dir does. The game dir is only where the old layout's files are moved from.
    gameDirName = com_gamedir;
    cacheDirName = za::String{com_basedirs[com_numbasedirs - 1]} + "/cache/maps";
    registryPath = za::String{com_basedirs[com_numbasedirs - 1]} + "/cache/maps_installed.txt";
    loadRegistry();
}

} // namespace

// ---------------------------------------------------------------- public

void start()
{
    ensureStarted();
}

namespace
{
void takeFinished();
} // namespace

void finish()
{
    // Quitting never waits on the network: the job is cancelled (a download stops within a second, an unpacking before
    // its next file) and given 3 s; one still running then (a blocking lookup inside curl) is let go, not joined, so
    // the process ends anyway. Host_Shutdown calls this (VR_StopDownloads) before NET_Shutdown's curl_global_cleanup,
    // and VR_Shutdown again.
    if(!worker.joinable())
    {
        return; // (no job started, or it was joined or let go already)
    }
    requestCancel(CancelQuit);
    const za::U32 t0 = SDL_GetTicks();
    while(jobRunning.loadSeqCst() && SDL_GetTicks() - t0 < 3000)
    {
        SDL_Delay(10);
    }
    if(worker.joinable())
    {
        if(jobRunning.loadSeqCst())
        {
            Sys_Printf("maps: the download did not stop in 3 s; quitting without it\n");
            Download_KeepGlobalState(); // (its transfer still reads libcurl's global state: not freed under it)
            worker.detach();
        }
        else
        {
            worker.join();
            if(pendingReady.loadSeqCst())
            {
                takeFinished(); // (a job that stopped as the game quit: its unpacked files removed, or recorded)
            }
        }
    }
    jobRunning.storeSeqCst(false);
}

void poll()
{
    trimCacheIfCapChanged(); // (the download cache: the first frame, and a changed cap)
    if(registryLoaded && titlesGeneration != mapindex::generation())
    {
        rebuildPackages(); // (the index arrived, or changed: the installed packages' titles from it)
    }
    if(!pendingReady.loadSeqCst())
    {
        // The running job, as its thread last said (the page shows `current` live), and the watchdog.
        if(jobRunning.loadSeqCst())
        {
            current.phase = static_cast<Phase>(SDL_AtomicGet(&livePhase));
            const int got = SDL_AtomicGet(&liveBytes);
            const int mirror = SDL_AtomicGet(&liveMirror);
            current.gotBytes = static_cast<za::U64>(got);
            if(got != lastBytes || mirror != lastMirror || current.phase != Phase::Download)
            {
                lastBytes = got;
                lastMirror = mirror;
                lastMoved = realtime;
            }
            else if(realtime - lastMoved > stallSeconds && !SDL_AtomicGet(&cancelJob))
            {
                Con_SafePrintf("maps: %s - nothing arrived for %.0f s: cancelled\n", current.title.cStr(), stallSeconds);
                requestCancel(CancelStalled);
            }
        }
        return;
    }
    takeFinished();
}

namespace
{

// The job its thread handed off: its console line, and what it wrote recorded (Done) or removed again (anything else).
// poll(), and finish(): a job that stopped because the game quit is rolled back too (poll() never runs again), so no
// unpacking leaves files the registry does not name.
void takeFinished()
{
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
    Con_SafePrintf("maps: %s - %s\n", j.title.cStr(), j.message.cStr());
    settleCacheAfterJob(j); // (the download cache: a failed job's zip dropped, the rest kept to the cap)
    // A job that did not finish is rolled back: the files its unpacking wrote are removed, so no half-installed package
    // is left offering Play. Not one the registry names (the package installed before: a pak's file is written over
    // its own copy, and stays its installed one).
    if(j.phase != Phase::Done)
    {
        int removed = 0;
        for(const InstalledFile& f : j.wrote)
        {
            bool recorded = false;
            for(const InstalledFile& r : registry.files)
            {
                recorded = recorded || (r.sha == f.sha && r.path == f.path);
            }
            if(!recorded)
            {
                removed += files::remove((addonDir(j.sha) + "/" + f.path).cStr()) ? 1 : 0;
            }
        }
        if(!installed(j.sha))
        {
            files::removeAll(addonDir(j.sha).cStr()); // (its folder, and the folders made for it)
        }
        if(removed)
        {
            VR_FileCacheForget();
            Con_SafePrintf("maps: %s - the %d file(s) it had unpacked were removed\n", j.title.cStr(), removed);
        }
        current.wrote.clear();
        return;
    }
    if(j.wrote.size())
    {
        for(const InstalledFile& f : j.wrote)
        {
            bool listed = false; // (a package installed again: its files listed once)
            for(InstalledFile& r : registry.files)
            {
                if(r.sha == f.sha && r.path == f.path)
                {
                    r.bytes = f.bytes;
                    listed = true;
                    break;
                }
            }
            if(!listed)
            {
                registry.files.pushBack(f);
            }
        }
        rebuildPackages();
        if(!writeRegistry())
        {
            Con_DPrintf("map install: could not write %s\n", registryPath.cStr());
        }
        // Installing writes files and reports them; it never starts a map. Playing is its own action (play(),
        // maps_play, the page's Play button), so a package can be got ready without leaving the current map.
    }
}

} // namespace

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
    return jobRunning.loadSeqCst() && request.sha == sha;
}

bool begin(const mapindex::Entry* entry, bool install, za::String* why)
{
    ensureStarted();
    const auto refuse = [&](za::String reason)
    {
        if(why)
        {
            *why = ZA_MOVE(reason);
        }
        return false;
    };
    if(!entry)
    {
        return refuse("no package");
    }
    if(jobRunning.loadSeqCst())
    {
        return refuse(statusLine() + " - one job at a time (maps_cancel, or Cancel on the page, stops it)");
    }
    if(!gameDirName.size())
    {
        return refuse("the game dir is not known yet");
    }
    if(worker.joinable())
    {
        worker.join(); // (a job that finished; its handle was never joined)
    }
    if(pendingReady.loadSeqCst())
    {
        // The last job finished after this frame's poll(): taken now, before this one replaces `current` (and before
        // its own handoff could replace it, its files never recorded).
        takeFinished();
    }
    request.sha = za::String{mapindex::index().field(entry->sha256)};
    request.title = za::String{mapindex::index().field(entry->title)};
    request.urls = za::String{mapindex::index().field(entry->urls)};
    request.extract = za::String{mapindex::index().field(entry->extract)};
    request.root = addonDir(request.sha);
    request.zipBytes = entry->bytes;
    request.install = install;
    request.wasInstalled = installed(request.sha);
    trimCacheForDownload(request.sha, request.zipBytes); // (the download cache: room made under the cap)
    SDL_AtomicSet(&cancelJob, 0);
    SDL_AtomicSet(&cancelReason, CancelNone);
    SDL_AtomicSet(&progress, 0);
    SDL_AtomicSet(&livePhase, static_cast<int>(Phase::Download));
    SDL_AtomicSet(&liveBytes, 0);
    SDL_AtomicSet(&liveMirror, 0);
    SDL_AtomicSet(&liveMirrors, 0);
    // The page's job is this one from now on (its thread's own Job replaces it when it is done).
    static_cast<Job&>(current) = Job{};
    current.sha = request.sha;
    current.title = request.title;
    current.install = install;
    current.zipBytes = request.zipBytes;
    current.phase = Phase::Download;
    jobStarted = lastMoved = realtime;
    lastBytes = 0;
    lastMirror = 0;
    jobRunning.storeSeqCst(true);
    worker = za::Thread(run);
    return true;
}

bool cancel()
{
    if(!jobRunning.loadSeqCst())
    {
        return false;
    }
    requestCancel(CancelUser);
    return true;
}

bool running()
{
    return jobRunning.loadSeqCst();
}

bool cancelling()
{
    return jobRunning.loadSeqCst() && SDL_AtomicGet(&cancelJob) != 0;
}

za::String statusLine()
{
    const Job& j = current;
    char line[320];
    if(jobRunning.loadSeqCst())
    {
        const int secs = static_cast<int>(realtime - jobStarted);
        if(SDL_AtomicGet(&cancelJob))
        {
            q_snprintf(line, sizeof(line), "Stopping %s (%s)...", j.title.cStr(), cancelText());
        }
        else if(j.phase == Phase::Extract)
        {
            q_snprintf(line, sizeof(line), "Installing %s: unpacking, %d%% (%d s)", j.title.cStr(), percent(), secs);
        }
        else
        {
            const int mirror = SDL_AtomicGet(&liveMirror), mirrors = SDL_AtomicGet(&liveMirrors);
            char where[48] = "";
            if(mirrors > 1 && mirror > 0)
            {
                q_snprintf(where, sizeof(where), ", mirror %d of %d", mirror, mirrors);
            }
            const double still = realtime - lastMoved;
            char stalled[48] = "";
            if(still >= 5.0)
            {
                q_snprintf(stalled, sizeof(stalled), ", nothing for %.0f s", still);
            }
            q_snprintf(line, sizeof(line), "Downloading %s: %s of %s%s (%d s%s)", j.title.cStr(),
                formatBytes(j.gotBytes).cStr(), formatBytes(j.zipBytes).cStr(), where, secs, stalled);
        }
    }
    else if(j.phase == Phase::Done)
    {
        q_snprintf(line, sizeof(line), "Idle. Last: %s - %s", j.title.cStr(), j.message.cStr());
    }
    else if(activeSha.size())
    {
        const mapindex::Entry* e = mapindex::find(activeSha);
        q_snprintf(line, sizeof(line), "Idle. Playing from %s's folder", e ? mapindex::index().field(e->title) : activeSha.cStr());
    }
    else if(j.phase == Phase::Failed)
    {
        q_snprintf(line, sizeof(line), "Idle. Failed: %s - %s", j.title.cStr(), j.message.cStr());
    }
    else
    {
        q_snprintf(line, sizeof(line), "Idle: nothing being downloaded");
    }
    return za::String{line};
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
    if(busy(sha))
    {
        // (its thread is writing into the folder this would remove, and would record files that are gone)
        Con_Printf("maps: %s is being installed again; cancel that first (maps_cancel).\n", sha.cStr());
        return false;
    }
    if(activeSha == sha)
    {
        activate(za::String{}); // (its folder off the search path before it goes)
    }
    int removed = 0;
    za::Vector<InstalledFile> keep;
    for(const InstalledFile& f : registry.files)
    {
        if(f.sha == sha)
        {
            removed++;
        }
        else
        {
            keep.pushBack(f);
        }
    }
    files::removeAll(addonDir(sha).cStr()); // (the package's own folder: nothing in it is another's)
    registry.files = ZA_MOVE(keep);
    rebuildPackages();
    writeRegistry();
    VR_FileCacheForget(); // (a map removed: the listings forget it too)
    Con_SafePrintf("maps: removed %d file(s) (%s).\n", removed, addonDir(sha).cStr());
    return true;
}

// The package's startmap, started now: the page's Play action, and maps_play. Never part of installing.
namespace
{

[[nodiscard]] za::String lowered(za::String s)
{
    for(za::SizeT i = 0; i < s.size(); i++)
    {
        s[i] = static_cast<char>(tolower(static_cast<unsigned char>(s[i])));
    }
    return s;
}

[[nodiscard]] bool packageHas(const za::String& sha, const char* map);

// Play's failure, said in the console and kept for the page.
bool playFailed(const za::String& sha, const za::String& text)
{
    Con_Printf("maps: %s\n", text.cStr());
    playProblemSha = sha;
    playProblemText = text;
    return false;
}

} // namespace

void packageMaps(const za::String& sha, za::Vector<za::String>& out)
{
    out.clear();
    if(!registryLoaded)
    {
        loadRegistry();
    }
    for(const InstalledFile& f : registry.files)
    {
        // "maps/<name>.bsp" (a subfolder's too: `map sub/name` loads it), as installed (lower case).
        if(f.sha == sha && f.path.size() > 9 && !q_strncasecmp(f.path.cStr(), "maps/", 5) && endsFolded(f.path, ".bsp"))
        {
            out.pushBack(lowered(za::String{f.path.substrByPosLen(5, f.path.size() - 9)}));
        }
    }
    za::quickSort(out.begin(), out.end(), [](const za::String& a, const za::String& b) { return strcmp(a.cStr(), b.cStr()) < 0; });
}

za::String startMap(const za::String& sha, za::String* why, int* mapCount)
{
    ensureStarted();
    za::Vector<za::String> maps;
    packageMaps(sha, maps);
    if(mapCount)
    {
        *mapCount = static_cast<int>(maps.size());
    }
    // The index's startmap first: a list for many packages ("start e1m1 e1m2 ..." for an episode, every map of a
    // speedmap pack), "start" preferred, else its first map the package holds.
    za::String pick;
    if(const mapindex::Entry* e = mapindex::find(sha))
    {
        mapindex::forParts(mapindex::index().field(e->startmap), [&](const za::String& part) {
            const za::String name = lowered(part);
            if((pick.empty() || name == "start") && pick != "start" && packageHas(sha, name.cStr()))
            {
                pick = name;
            }
        });
    }
    // None given (about half of the index's packages), or none of them installed: its own maps, "start" if one is,
    // else the first by name.
    for(za::SizeT i = 0; pick.empty() && i < maps.size(); i++)
    {
        pick = maps[i] == "start" ? maps[i] : za::String{};
    }
    if(pick.empty() && !maps.empty())
    {
        pick = maps[0];
    }
    if(pick.empty() && why)
    {
        *why = "its files hold no map (no BSP under maps/): it may be a mod or a texture pack, not a map";
    }
    return pick;
}

const za::String& playProblem(const za::String& sha)
{
    return sha == playProblemSha ? playProblemText : noPlayProblem;
}

za::String madeFor(const mapindex::Entry& e)
{
    // install.extract's game folder ("{base}/ad/"), else a mod's tag (a package laid out from the base dir).
    const char* extract = mapindex::index().field(e.extract);
    if(!q_strncasecmp(extract, "{base}", 6))
    {
        extract += 6;
    }
    while(*extract == '/' || *extract == '\\')
    {
        extract++;
    }
    za::String folder;
    while(*extract && *extract != '/' && *extract != '\\')
    {
        folder += static_cast<char>(tolower(static_cast<unsigned char>(*extract++)));
    }
    if(!*extract)
    {
        folder = za::String{}; // ("{base}/maps": no game folder named)
    }
    bool adTag = false, quothTag = false;
    mapindex::forParts(mapindex::index().field(e.themes), [&](const za::String& tag) {
        adTag = adTag || tag == "arcane_dimensions";
        quothTag = quothTag || tag == "quoth";
    });
    // The stock game's and the mission packs' (Quake VR runs those), and no folder at all, need nothing else.
    if(folder.empty() || assetFolder(folder) || folder == "id1" || folder == "hipnotic" || folder == "rogue" || folder == "quakevr")
    {
        folder = adTag ? za::String{"ad"} : quothTag ? za::String{"quoth"} : za::String{};
    }
    if(folder == "ad")
    {
        return za::String{"Arcane Dimensions"};
    }
    if(folder == "quoth")
    {
        return za::String{"Quoth"};
    }
    if(folder == "copper")
    {
        return za::String{"Copper"};
    }
    return folder;
}

bool play(const za::String& sha, const char* map)
{
    ensureStarted();
    const mapindex::Entry* e = mapindex::find(sha);
    if(!e)
    {
        return playFailed(sha, za::String{"the index no longer has that package."});
    }
    const za::String title{mapindex::index().field(e->title)};
    if(!installed(sha))
    {
        return playFailed(sha, title + " is not installed.");
    }
    za::String name;
    if(map && *map)
    {
        name = lowered(za::String{map});
        if(!packageHas(sha, name.cStr()))
        {
            return playFailed(sha, title + " has no map " + name + ".");
        }
    }
    else
    {
        za::String why;
        int count = 0;
        name = startMap(sha, &why, &count);
        if(name.empty())
        {
            return playFailed(sha, title + ": " + why + ".");
        }
        if(count > 1)
        {
            Con_Printf("maps: %s holds %d maps; starting %s (maps_play %.8s <map> for another)\n", title.cStr(), count,
                name.cStr(), sha.cStr());
        }
    }
    // Its folder on the search path (under Quake VR's own), then its map: the `map` command keeps this package for
    // it, whatever the stock game has of the same name (an episode's start).
    if(!activate(sha))
    {
        return playFailed(sha, za::String{"the package's folder could not be mounted (see the console)."});
    }
    playProblemSha = za::String{};
    playProblemText = za::String{};
    playPending = sha;
    // Cbuf_InsertText, not AddText: this runs next. AddText would put it after the commands already queued (a test
    // script's `screenshot; quit` would be run first, and the map never started).
    Cbuf_InsertText((za::String{"map "} + name + "\n").cStr());
    return true;
}

// ---------------------------------------------------------------- the active package

const za::String& active()
{
    return activeSha;
}

bool activate(const za::String& sha)
{
    if(sha == activeSha)
    {
        return true;
    }
    if(!VR_QuakeVRMounted())
    {
        Con_Printf("maps: the map packages are played in Quake VR's own game (quakevr) only.\n");
        return false;
    }
    if(sha.size() && !files::isDirectory(addonDir(sha).cStr()))
    {
        Con_Printf("maps: %s is missing; install the package again.\n", addonDir(sha).cStr());
        return false;
    }
    activeSha = sha;
    // The game folders rebuilt with it (mountActive, from vr_gamedir.cpp's hook), and every cache of models, sounds,
    // textures and file listings emptied: one package's files never outlive it into another's maps or the stock game.
    VR_ReloadVRGameKeepCampaign();
    VR_FileCacheForget();
    if(sha.size())
    {
        const mapindex::Entry* e = mapindex::find(sha);
        Con_Printf("maps: playing from %s's folder (%s)\n", e ? mapindex::index().field(e->title) : sha.cStr(),
            addonDir(sha).cStr());
    }
    else
    {
        Con_Printf("maps: no map package mounted (the stock game's folders)\n");
    }
    return true;
}

void mountActive()
{
    if(activeSha.empty())
    {
        return;
    }
    const za::String dir = addonDir(activeSha);
    if(!files::isDirectory(dir.cStr()))
    {
        Con_Printf("maps: %s is missing: no map package mounted\n", dir.cStr());
        activeSha = za::String{};
        return;
    }
    COM_AddAddonPath(dir.cStr());
}

bool skipSearchPath(const char* path)
{
    const za::String& root = addonsRoot();
    return baseOnly && root.size() && !q_strncasecmp(path, root.cStr(), root.size());
}

namespace
{

// A package's folder has this map (its BSP under the lower-case name it was installed with).
[[nodiscard]] bool packageHas(const za::String& sha, const char* map)
{
    za::String lower{map};
    for(za::SizeT i = 0; i < lower.size(); i++)
    {
        lower[i] = static_cast<char>(tolower(static_cast<unsigned char>(lower[i])));
    }
    return files::isFile((addonDir(sha) + "/maps/" + lower + ".bsp").cStr());
}

// Which package a map is played from: none when the stock game (Quake VR, id1, the packs) has it; else the active
// package if it has it, else the installed one that has it (the last installed first).
[[nodiscard]] za::String packageForMap(const char* map)
{
    baseOnly = true;
    const bool stock = COM_FileExists(va("maps/%s.bsp", map), nullptr);
    baseOnly = false;
    if(stock)
    {
        return za::String{};
    }
    if(activeSha.size() && packageHas(activeSha, map))
    {
        return activeSha;
    }
    const za::Vector<Installed>& list = installedList();
    for(za::SizeT i = list.size(); i-- > 0;)
    {
        if(packageHas(list[i].sha, map))
        {
            return list[i].sha;
        }
    }
    return za::String{};
}

} // namespace

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

void cancel_f()
{
    if(cancel())
    {
        Con_Printf("maps: cancelling %s...\n", current.title.cStr());
    }
    else
    {
        Con_Printf("maps: nothing to cancel (%s)\n", statusLine().cStr());
    }
}

void status_f()
{
    Con_Printf("maps: %s\n", statusLine().cStr());
    const mapindex::Entry* e = activeSha.size() ? mapindex::find(activeSha) : nullptr;
    Con_Printf("maps: mounted: %s\n", activeSha.empty() ? "none (the stock game)"
                                      : e ? mapindex::index().field(e->title) : activeSha.cStr());
}

void get_f()
{
    const mapindex::Entry* e = argEntry("maps_get", Cmd_Argv(1));
    za::String why;
    if(e && !begin(e, false, &why))
    {
        Con_Printf("maps_get: not started: %s\n", why.cStr());
    }
    else if(e)
    {
        Con_SafePrintf("maps: downloading %s (%s)...\n", mapindex::index().field(e->title),
                       formatBytes(e->bytes).cStr());
    }
}

void install_f()
{
    const mapindex::Entry* e = argEntry("maps_install", Cmd_Argv(1));
    za::String why;
    if(e && !begin(e, true, &why))
    {
        Con_Printf("maps_install: not started: %s\n", why.cStr());
    }
    else if(e)
    {
        Con_SafePrintf("maps: downloading and installing %s (%s)...\n", mapindex::index().field(e->title),
                       formatBytes(e->bytes).cStr());
    }
}

// maps_cache [trim]: the download cache (cache/maps/): its zips oldest first (the first removed when it is over
// vr_maps_cache_mb), what they hold, and `trim`: trimmed to the cap now.
void cache_f()
{
    ensureStarted();
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "trim"))
    {
        trimmedCapMb = vr_maps_cache_mb.value;
        const CacheTrim t = trimCache(cacheCapBytes(), za::String{});
        Con_Printf("maps: download cache trimmed to %g MB: %d zip(s) removed, %s freed\n", vr_maps_cache_mb.value,
            t.removed, formatBytes(t.freed).cStr());
    }
    za::Vector<CachedZip> zips;
    int others = 0;
    listCache(zips, &others);
    za::U64 total = 0;
    for(const CachedZip& z : zips)
    {
        total += z.bytes;
    }
    Con_Printf("maps: download cache %s: %d zip(s), %s of %g MB (vr_maps_cache_mb; 0: none kept once installed)\n",
        cacheDirName.cStr(), static_cast<int>(zips.size()), formatBytes(total).cStr(), vr_maps_cache_mb.value);
    if(others)
    {
        Con_Printf("maps: %d other entr%s in it, never counted nor removed\n", others, others == 1 ? "y" : "ies");
    }
    const za::String running = jobRunning.loadSeqCst() ? request.sha : za::String{};
    constexpr int maxShown = 40;
    int shown = 0;
    for(const CachedZip& z : zips)
    {
        if(shown == maxShown)
        {
            Con_Printf("  ... and %d more\n", static_cast<int>(zips.size()) - maxShown);
            break;
        }
        const mapindex::Entry* e = mapindex::find(z.sha);
        Con_Printf("  %2d. %.16s %9s  %s%s%s\n", ++shown, z.sha.cStr(), formatBytes(z.bytes).cStr(),
            e ? mapindex::index().field(e->title) : "(not in the index)", installed(z.sha) ? "  [installed]" : "",
            z.sha == running ? "  [its job is running: kept]" : "");
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
    Con_SafePrintf("maps: %d package(s) installed, each in its own folder in %s\n", static_cast<int>(list.size()),
        addonsRoot().cStr());
    for(const Installed& p : list)
    {
        Con_SafePrintf("  %s  %8s  %d file(s)  %s\n", p.title.cStr(), formatBytes(p.bytes).cStr(),
                       p.files, p.sha.cStr());
        // What Play starts (and its other maps), and the mod it was made for: as the Map Library's detail says.
        za::String why;
        int count = 0;
        const za::String start = startMap(p.sha, &why, &count);
        const mapindex::Entry* e = mapindex::find(p.sha);
        const za::String mod = e ? madeFor(*e) : za::String{};
        Con_SafePrintf("    %s%s%s%s%s\n", start.size() ? "starts " : "cannot be played: ",
            start.size() ? start.cStr() : why.cStr(), count > 1 ? (za::String{" (of "} + za::toString(count) + " maps)").cStr() : "",
            mod.size() ? "; made for " : "", mod.cStr());
    }
}

void uninstall_f()
{
    // An installed package by its sha's start, whether the index has it or not (it may have dropped it, or not have
    // arrived yet); otherwise the index's (argEntry says what is wrong).
    const char* arg = Cmd_Argv(1);
    za::String sha;
    int matches = 0;
    for(const Installed& p : installedList())
    {
        if(arg[0] && !q_strncasecmp(p.sha.cStr(), arg, strlen(arg)))
        {
            sha = p.sha;
            matches++;
        }
    }
    if(matches == 1)
    {
        uninstall(sha);
        return;
    }
    if(const mapindex::Entry* e = argEntry("maps_uninstall", arg))
    {
        if(!uninstall(za::String{mapindex::index().field(e->sha256)}))
        {
            Con_Printf("maps_uninstall: %s is not installed.\n", mapindex::index().field(e->title));
        }
    }
}

void play_f()
{
    const mapindex::Entry* e = argEntry("maps_play", Cmd_Argv(1));
    if(e)
    {
        (void)play(za::String{mapindex::index().field(e->sha256)}, Cmd_Argc() >= 3 ? Cmd_Argv(2) : nullptr);
    }
}

void registerCommands()
{
    Cmd_AddCommand("maps_get", get_f);
    Cmd_AddCommand("maps_install", install_f);
    Cmd_AddCommand("maps_cancel", cancel_f);
    Cmd_AddCommand("maps_status", status_f);
    Cmd_AddCommand("maps_play", play_f);
    Cmd_AddCommand("maps_installed", installed_f);
    Cmd_AddCommand("maps_uninstall", uninstall_f);
    Cmd_AddCommand("maps_cache", cache_f);
    Cmd_AddCommand("vr_sha256_test", sha256::selfTest_f);
}

za::String packageFor(const char* map)
{
    ensureStarted();
    return packageForMap(map);
}

bool playPendingFor(const char* map)
{
    // Play's `map`: the package it mounted is kept for it (the next `map` is decided by the stock game again).
    const bool keep = playPending.size() && playPending == activeSha && packageHas(activeSha, map);
    playPending = za::String{};
    return keep;
}

} // namespace qvr::mapinstall

// ---------------------------------------------------------------- the engine's hooks

using namespace qvr;

// `map X` (the console, the menus, Play): the map's package mounted, or none when the stock game has the map (New
// Game's `map start` is Quake's own, whatever package was played last). Play's own `map` keeps its package.
extern "C" int VR_AddonForMapCommand(const char* map)
{
    if(!map || !*map || !VR_QuakeVRMounted())
    {
        return 1;
    }
    if(mapinstall::playPendingFor(map))
    {
        return 1;
    }
    return mapinstall::activate(mapinstall::packageFor(map)) ? 1 : 0;
}

// `load`: the package the save was made in (its <save>.addon), else the one its map is in, mounted before the load.
extern "C" void VR_AddonForSave(const char* savepath, const char* map)
{
    if(!VR_QuakeVRMounted())
    {
        return;
    }
    za::String sha;
    if(files::readText((za::String{savepath} + ".addon").cStr(), sha))
    {
        while(sha.size() && (sha.back() == '\n' || sha.back() == '\r' || sha.back() == ' '))
        {
            sha.popBack();
        }
        if(sha.size() && !mapinstall::installed(sha))
        {
            Con_Printf("maps: the save was made in a map package no longer installed (%s)\n", sha.cStr());
            sha = za::String{};
        }
    }
    if(sha.empty())
    {
        sha = mapinstall::packageFor(map);
    }
    mapinstall::activate(sha);
}

// A map spawned: the crash report's context (vr_crash.cpp), so that a crash in a map package's map names it.
extern "C" void VR_NoteMapSpawn(const char* map)
{
    const za::String& sha = mapinstall::active();
    za::String line = za::String{"map "} + (map ? map : "?");
    if(sha.size())
    {
        const mapindex::Entry* e = mapindex::find(sha);
        line += za::String{", map package "} + (e ? mapindex::index().field(e->title) : "?") + " (" +
                za::String{za::StringView{sha}.substrByPosLen(0, za::min(sha.size(), za::SizeT{16}))} + ")";
    }
    else
    {
        line += ", no map package";
    }
    VR_SetCrashContext(line.cStr());
}

// `save`: the package mounted now, noted beside the save (removed when there is none).
extern "C" void VR_AddonOnSave(const char* savepath)
{
    const za::String note = za::String{savepath} + ".addon";
    const za::String& sha = mapinstall::active();
    if(sha.size())
    {
        (void)files::writeText(note.cStr(), za::StringView{sha});
    }
    else if(files::exists(note.cStr()))
    {
        files::remove(note.cStr());
    }
}
