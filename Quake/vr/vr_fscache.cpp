// vr_fscache.cpp -- the loose files' presence while the game starts and while a map loads (COM_FindFile): each
// directory looked in is listed once, and a file that isn't there is answered from memory. A load looks for hundreds
// of optional files that mostly don't exist (a model's replacement skins in png, tga, jpg, pcx and lmp; its md5 and md3
// replacements; its authored normal maps), each in every game directory: 3500 misses, 0.4-0.7 s of a first map load
// in the file system. Outside those windows every lookup asks the file system, so a file made while the game runs (a
// map compiled in TrenchBroom, a config) is found as before. Inside them a file made, written or removed through the
// engine (Sys_fopen for writing, Sys_remove, Sys_ReplaceFile, Sys_rename: VR_FileCacheForgetFile, after the change)
// forgets its directory's listing, the directory known by its own identity (its volume and file index; its device and
// inode), not by its name: the same directory by another name (relative, through a junction such as the game folder's
// quakevr, in another case or with other separators) is the same; a listing of a directory that wasn't there goes
// with any. When that can't be told (the directory not opened, a directory moved or made: Sys_mkdir, the map
// installer's and the relighting's bulk work) every listing goes (VR_FileCacheForget). (Until 2026-10-12 any write
// forgot them all: the texture caches written in the background while a map loads had the main thread list the same
// directories again and again.)
// The main thread's only: another thread's lookups (the map menu's description parser, ExtraMaps_ParseDescriptions,
// which runs from start-up on while the first maps load) ask the file system, and its writes (a background save)
// leave the forgetting to the main thread's next lookup. It used to share the cache unlocked, and
// a lookup made while the main thread listed a directory or cleared the cache crashed the game now and then (a
// test run's start: an access violation or a fast fail).

#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_jobs.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace
{

za::Atomic<bool> enabled{false};
za::Atomic<bool> forgetAsked{false}; // VR_FileCacheForget from another thread
za::ThreadId owner; // the main thread: VR_FileCacheEnable's first caller (VR_TimeStart, before any other thread)
// By name; looked up by a view of a stack buffer (a transparent hash): no za::String made a lookup (48 K a big map's load).
struct NameHash
{
    using is_transparent = void;
    [[nodiscard]] za::U64 operator()(za::StringView s) const { return ankerl::unordered_dense::hash<za::StringView>{}(s); }
};
struct NameEqual
{
    using is_transparent = void;
    [[nodiscard]] bool operator()(za::StringView a, za::StringView b) const { return a == b; }
};
using Names = ankerl::unordered_dense::set<za::String, NameHash, NameEqual>;

// A directory's identity, whatever the name it is reached by (Windows: its volume's serial and file index; elsewhere
// its device and inode).
struct DirId
{
    za::U64 volume{0};
    za::U64 index{0};
    [[nodiscard]] bool operator==(const DirId&) const = default;
};

// A listed directory: its files (not subdirectories), and who it is (`known` false: it wasn't there, or wasn't opened).
struct Listing
{
    Names files;
    DirId id;
    bool known{false};
};
ankerl::unordered_dense::map<za::String, Listing, NameHash, NameEqual> dirs; // by its name as looked up (folded)

// The names COM_FindFile found in no search path while loading (as asked: a pak's names are case-sensitive), not
// looked for again until the window ends (PROFILE_20261016 B4: a warm AD load asks 4800 names 28000 times, most of
// them misses). Forgotten with any listing (a file written anywhere: a cache's too, on any thread), and when the search
// paths or the folders a lookup skips change (VR_FileCacheSearchChanged).
Names misses;
za::Atomic<bool> missesForgetAsked{false}; // VR_FileCacheSearchChanged from another thread
za::U64 missesEpoch = 0;  // bumped as they are forgotten: a miss looked up before is not noted after (VR_FileCacheMissNote)
za::U64 lookupEpoch = 0;  // ... the epoch the lookup being made started in (VR_FileCacheMissKnown)

void clearMisses()
{
    misses.clear();
    missesEpoch++;
}

// The directories other threads wrote in (VR_FileCacheForgetFile), for the main thread's next lookup to forget.
struct Pending
{
    za::AtomicMutex lock;
    za::Vector<DirId> ids;
};
Pending pending;

#ifdef _WIN32
constexpr bool caseInsensitive = true; // Windows' file names: the same file whatever the case (ASCII here)
#else
constexpr bool caseInsensitive = false;
#endif

void fold(char* s, za::SizeT length)
{
    if(caseInsensitive)
    {
        for(char* c = s; c != s + length; c++)
        {
            if(*c >= 'A' && *c <= 'Z')
            {
                *c = static_cast<char>(*c - 'A' + 'a');
            }
            else if(*c == '\\')
            {
                *c = '/';
            }
        }
    }
}

void fold(za::String& s)
{
    fold(s.data(), s.size());
}

// The identity of the directory `dir` (UTF-8; "": the current one): false if it isn't opened (not there).
[[nodiscard]] bool dirIdentity(const char* dir, DirId& out)
{
    if(!*dir)
    {
        dir = ".";
    }
#ifdef _WIN32
    wchar_t wide[MAX_PATH];
    if(!MultiByteToWideChar(CP_UTF8, 0, dir, -1, wide, MAX_PATH))
    {
        return false;
    }
    // (a directory's handle needs FILE_FLAG_BACKUP_SEMANTICS; a junction or a link is followed to what it names)
    const HANDLE h = CreateFileW(wide, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if(h == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    BY_HANDLE_FILE_INFORMATION info;
    const bool ok = GetFileInformationByHandle(h, &info) != 0 && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    CloseHandle(h);
    if(ok)
    {
        out.volume = info.dwVolumeSerialNumber;
        out.index = (static_cast<za::U64>(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
    }
    return ok;
#else
    struct stat st;
    if(stat(dir, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        return false;
    }
    out.volume = static_cast<za::U64>(st.st_dev);
    out.index = static_cast<za::U64>(st.st_ino);
    return true;
#endif
}

// The directory `path` (UTF-8) is in, into `out` ("" for a bare name: the current directory; "C:/x": "C:/", "/x":
// "/"): false if too long.
[[nodiscard]] bool parentOf(const char* path, char (&out)[MAX_OSPATH])
{
    const size_t length = strlen(path);
    if(length >= sizeof(out))
    {
        return false;
    }
    size_t name = length; // where the name after the last separator starts
    while(name > 0 && path[name - 1] != '/' && path[name - 1] != '\\')
    {
        name--;
    }
    size_t end = name > 0 ? name - 1 : 0; // the separator dropped...
    if(name > 0 && (end == 0 || path[end - 1] == ':'))
    {
        end = name; // ... but a root's kept
    }
    memcpy(out, path, end);
    out[end] = '\0';
    return true;
}

// Forgets the listings of the directory `id`, and those of directories that weren't there (it may be one of them now).
void forget(const DirId& id)
{
    erase_if(dirs, [&](const auto& entry) { return !entry.second.known || entry.second.id == id; });
    clearMisses();
}

// What other threads asked (the main thread's, before a lookup).
void takePending()
{
    if(missesForgetAsked.exchangeSeqCst(false))
    {
        clearMisses();
    }
    if(forgetAsked.exchangeSeqCst(false))
    {
        dirs.clear();
        clearMisses();
    }
    za::Vector<DirId> ids;
    {
        za::LockGuard guard{pending.lock};
        if(pending.ids.empty())
        {
            return;
        }
        ids.swap(pending.ids);
    }
    for(const DirId& id : ids)
    {
        forget(id);
    }
}

const Names& listing(za::StringView dir)
{
    const auto it = dirs.find(dir);
    if(it != dirs.end())
    {
        return it->second.files;
    }
    const za::String key{dir};
    Listing& l = dirs[key];
    // (who it is before what is in it: a file made in between is forgotten with it, not missed)
    l.known = dirIdentity(key.cStr(), l.id);
    for(findfile_t* f = Sys_FindFirst(key.cStr(), nullptr); f; f = Sys_FindNext(f))
    {
        if(!(f->attribs & FA_DIRECTORY))
        {
            za::String name = f->name;
            fold(name);
            l.files.insert(ZA_MOVE(name));
        }
    }
    return l.files;
}

} // namespace

extern "C" void VR_FileCacheEnable(int on)
{
    if(owner == za::ThreadId{})
    {
        owner = za::ThisThread::getId();
    }
    enabled.storeSeqCst(on != 0);
    takePending(); // (all listed afresh from now on: what was asked before is in the new listings)
    dirs.clear();
    clearMisses();
}

extern "C" int VR_FileCacheEnabled()
{
    return enabled.loadSeqCst() ? 1 : 0;
}

extern "C" void VR_FileCacheForget()
{
    if(za::ThisThread::getId() != owner)
    {
        forgetAsked.storeSeqCst(true); // a file written on another thread (a background save, an add-on's install): the main
        return;             // thread's next lookup forgets the listings
    }
    dirs.clear();
    clearMisses();
}

extern "C" void VR_FileCacheSearchChanged()
{
    if(za::ThisThread::getId() != owner)
    {
        missesForgetAsked.storeSeqCst(true);
        return;
    }
    clearMisses();
}

extern "C" int VR_FileCacheMissKnown(const char* name)
{
    if(!enabled.loadSeqCst() || za::ThisThread::getId() != owner)
    {
        return 0;
    }
    takePending();
    lookupEpoch = missesEpoch;
    return misses.find(za::StringView{name, strlen(name)}) != misses.end() ? 1 : 0;
}

extern "C" void VR_FileCacheMissNote(const char* name)
{
    if(!enabled.loadSeqCst() || za::ThisThread::getId() != owner)
    {
        return;
    }
    takePending(); // (a write meanwhile, on another thread: this miss may be older than it, not noted)
    if(lookupEpoch == missesEpoch)
    {
        misses.insert(za::String{name});
    }
}

extern "C" void VR_FileCacheForgetFile(const char* path)
{
    if(!enabled.loadSeqCst())
    {
        return; // (none listed: VR_FileCacheEnable starts afresh, after this file's change)
    }
    char dir[MAX_OSPATH];
    DirId id;
    if(!path || !parentOf(path, dir) || !dirIdentity(dir, id))
    {
        VR_FileCacheForget();
        return;
    }
    if(za::ThisThread::getId() != owner)
    {
        za::LockGuard guard{pending.lock};
        pending.ids.pushBack(id);
        return;
    }
    forget(id);
}

extern "C" void VR_FileCacheForgetMoved(const char* from, const char* to)
{
    if(!enabled.loadSeqCst())
    {
        return;
    }
    // A file's move: its old directory and its new one. A directory's (or not known): every listing (those of the
    // directories in it too).
    if(!from || !to || Sys_FileType(to) != FS_ENT_FILE)
    {
        VR_FileCacheForget();
        return;
    }
    VR_FileCacheForgetFile(from);
    VR_FileCacheForgetFile(to);
}

extern "C" int VR_FileCacheHas(const char* path)
{
    if(!enabled.loadSeqCst() || za::ThisThread::getId() != owner)
    {
        return -1;
    }
    takePending();
    // Only plain names: no directory walks, nothing past ASCII (whose case folding the file system does its own way).
    for(const char* p = path; *p; p++)
    {
        if(static_cast<unsigned char>(*p) >= 0x80 || (p[0] == '.' && p[1] == '.'))
        {
            return -1;
        }
    }
    // (folded in a buffer on the stack: a lookup makes no string; a longer path is the file system's to answer)
    char full[MAX_OSPATH];
    const size_t length = strlen(path);
    if(length >= sizeof(full))
    {
        return -1;
    }
    memcpy(full, path, length + 1);
    fold(full, length);
    const za::StringView view{full, length};
    const size_t slash = view.findLastOf("/\\");
    if(slash == za::StringView::nPos || slash + 1 == length)
    {
        return -1;
    }
    const Names& files = listing(view.substrByPosLen(0, slash));
    return files.find(view.substrByPosLen(slash + 1)) != files.end() ? 1 : 0;
}

namespace
{

// vr_fscache_test [the game directory by another name]: the listings forget what they must, in a folder of the game
// directory made for it (removed after): a file written by another spelling of its directory's path (".", backslashes,
// another case; the argument's name for the game directory, e.g. its junction's target), removed, moved to another
// directory, written in a directory that wasn't there, written on another thread, and a directory moved; and a
// listing of another directory kept when a file is written. Prints each failure and "fscache test: N passed, M failed".
void testCommand()
{
    int passed = 0, failed = 0;
    const auto check = [&](bool ok, const char* what) {
        ok ? passed++ : failed++;
        if(!ok)
        {
            Con_Printf("fscache test FAILED: %s\n", what);
        }
    };
    const bool wasEnabled = enabled.loadSeqCst();
    const za::String root = za::String{com_gamedir} + "/fscache_test";
    const za::String other = root + "/other";
    qvr::files::removeAll(root.cStr());
    qvr::files::createDirectories(other.cStr());
    VR_FileCacheEnable(1);

    const auto has = [](const za::String& p) { return VR_FileCacheHas(p.cStr()); };
    const auto write = [](const za::String& p) {
        FILE* f = Sys_fopen(p.cStr(), "wb");
        if(f)
        {
            fputc('x', f);
            fclose(f);
        }
        return f != nullptr;
    };
    const auto listed = [](const za::String& dir) {
        za::String key = dir;
        fold(key);
        return dirs.find(za::StringView{key}) != dirs.end();
    };
    // `p` with backslashes, upper case and a "." for its last directory.
    const auto respelt = [](const za::String& dir, const char* name) {
        za::String out = dir + "/./" + name;
        for(char& c : out)
        {
            c = c == '/' ? '\\' : (c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c);
        }
        return out;
    };

    check(has(root + "/a.tmp") == 0 && has(other + "/b.tmp") == 0, "files not there yet");
    check(listed(root) && listed(other), "both directories listed");
    check(write(respelt(root, "a.tmp")) && has(root + "/a.tmp") == 1, "a file written by another spelling found");
    check(listed(other), "another directory's listing kept");
    check(Sys_remove((root + "/a.tmp").cStr()) == 0 && has(root + "/a.tmp") == 0, "a removed file gone");

    check(write(root + "/c.tmp") && has(root + "/c.tmp") == 1 && has(other + "/c.tmp") == 0, "a file to move");
    check(Sys_ReplaceFile((root + "/c.tmp").cStr(), (other + "/c.tmp").cStr()) == 0, "moved");
    check(has(root + "/c.tmp") == 0 && has(other + "/c.tmp") == 1, "a moved file in its new directory only");

    check(has(root + "/sub/x.tmp") == 0, "a directory not there");
    check(write(root + "/sub/x.tmp") && has(root + "/sub/x.tmp") == 1, "a file in a directory made for it found");

    check(has(root + "/d.tmp") == 0, "a file not written yet (another thread)");
    za::Atomic<bool> done{false};
    za::Atomic<bool> offMain{false};
    {
        qvr::jobs::Future<void> job = qvr::jobs::async([&]() {
            offMain.storeSeqCst(VR_OnMainThread() == 0);
            write(root + "/d.tmp");
            done.storeSeqCst(true);
        });
        for(int i = 0; i < 2000 && !done.loadSeqCst(); i++)
        {
            Sys_Sleep(1);
        }
        job.get();
    }
    check(!offMain.loadSeqCst() || has(root + "/d.tmp") == 1, "a file written on another thread found");
    if(!offMain.loadSeqCst())
    {
        Con_Printf("fscache test: the job ran on the main thread (no worker took it): not a test of another thread's\n");
    }

    if(Cmd_Argc() > 1)
    {
        const za::String alias = za::String{Cmd_Argv(1)} + "/fscache_test";
        check(has(root + "/e.tmp") == 0, "a file not written yet (alias)");
        check(write(alias + "/e.tmp") && has(root + "/e.tmp") == 1, "a file written by the directory's other name found");
        check(listed(other), "another directory's listing kept (alias)");
    }

    check(has(root + "/sub2/x.tmp") == 0 && has(root + "/sub/x.tmp") == 1, "before a directory's move");
    check(Sys_rename((root + "/sub").cStr(), (root + "/sub2").cStr()) == 0, "a directory moved");
    check(has(root + "/sub2/x.tmp") == 1 && has(root + "/sub/x.tmp") == 0, "a moved directory's files where it went");

    qvr::files::removeAll(root.cStr());
    check(has(root + "/other/c.tmp") == 0, "all gone after");
    VR_FileCacheEnable(wasEnabled ? 1 : 0);
    Con_Printf("fscache test: %d passed, %d failed\n", passed, failed);
}

} // namespace

extern "C" void VR_FileCacheRegisterCommands()
{
    Cmd_AddCommand("vr_fscache_test", testCommand);
}
