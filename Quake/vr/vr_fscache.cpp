// vr_fscache.cpp -- the loose files' presence while the game starts and while a map loads (COM_FindFile): each
// directory looked in is listed once, and a file that isn't there is answered from memory. A load looks for hundreds
// of optional files that mostly don't exist (a model's replacement skins in png, tga, jpg, pcx and lmp; its md5 and md3
// replacements; its authored normal maps), each in every game directory: 3500 misses, 0.4-0.7 s of a first map load
// in the file system. Outside those windows (and after any file is written) every lookup asks the file system, so a
// file made while the game runs (a map compiled in TrenchBroom, a config) is found as before.
// The main thread's only: another thread's lookups (the map menu's description parser, ExtraMaps_ParseDescriptions,
// which runs from start-up on while the first maps load) ask the file system, and its writes (a background save)
// leave the forgetting to the main thread's next lookup. It used to share the cache unlocked, and
// a lookup made while the main thread listed a directory or cleared the cache crashed the game now and then (a
// test run's start: an access violation or a fast fail).

#include "vr_engine.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <string.h>

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
ankerl::unordered_dense::map<za::String, Names, NameHash, NameEqual> dirs; // directory -> its files (not subdirectories)

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

const Names& listing(za::StringView dir)
{
    const auto it = dirs.find(dir);
    if(it != dirs.end())
    {
        return it->second;
    }
    const za::String key{dir};
    Names& files = dirs[key];
    for(findfile_t* f = Sys_FindFirst(key.cStr(), nullptr); f; f = Sys_FindNext(f))
    {
        if(!(f->attribs & FA_DIRECTORY))
        {
            za::String name = f->name;
            fold(name);
            files.insert(ZA_MOVE(name));
        }
    }
    return files;
}

} // namespace

extern "C" void VR_FileCacheEnable(int on)
{
    if(owner == za::ThreadId{})
    {
        owner = za::ThisThread::getId();
    }
    enabled.storeSeqCst(on != 0);
    dirs.clear();
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
}

extern "C" int VR_FileCacheHas(const char* path)
{
    if(!enabled.loadSeqCst() || za::ThisThread::getId() != owner)
    {
        return -1;
    }
    if(forgetAsked.exchangeSeqCst(false))
    {
        dirs.clear();
    }
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
