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

#include "Zancle/Base/Macros.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

namespace
{

za::Atomic<bool> enabled{false};
za::Atomic<bool> forgetAsked{false}; // VR_FileCacheForget from another thread
za::ThreadId owner; // the main thread: VR_FileCacheEnable's first caller (VR_TimeStart, before any other thread)
ankerl::unordered_dense::map<za::String, ankerl::unordered_dense::set<za::String>> dirs; // directory -> its files (not subdirectories)

#ifdef _WIN32
constexpr bool caseInsensitive = true; // Windows' file names: the same file whatever the case (ASCII here)
#else
constexpr bool caseInsensitive = false;
#endif

void fold(za::String& s)
{
    if(caseInsensitive)
    {
        for(char& c : s)
        {
            if(c >= 'A' && c <= 'Z')
            {
                c = static_cast<char>(c - 'A' + 'a');
            }
            else if(c == '\\')
            {
                c = '/';
            }
        }
    }
}

const ankerl::unordered_dense::set<za::String>& listing(const za::String& dir)
{
    const auto it = dirs.find(dir);
    if(it != dirs.end())
    {
        return it->second;
    }
    ankerl::unordered_dense::set<za::String>& files = dirs[dir];
    for(findfile_t* f = Sys_FindFirst(dir.cStr(), nullptr); f; f = Sys_FindNext(f))
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
    za::String full = path;
    fold(full);
    const size_t slash = full.findLastOf("/\\");
    if(slash == za::StringView::nPos || slash + 1 == full.size())
    {
        return -1;
    }
    const ankerl::unordered_dense::set<za::String>& files = listing(za::String{full.substrByPosLen(0, slash)});
    return files.count(za::String{full.substrByPosLen(slash + 1)}) ? 1 : 0;
}
