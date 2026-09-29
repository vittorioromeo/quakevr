// vr_fscache.cpp -- the loose files' presence while the game starts and while a map loads (COM_FindFile): each
// directory looked in is listed once, and a file that isn't there is answered from memory. A load looks for hundreds
// of optional files that mostly don't exist (a model's replacement skins in png, tga, jpg, pcx and lmp; its md5 and md3
// replacements; its authored normal maps), each in every game directory: 3500 misses, 0.4-0.7 s of a first map load
// in the file system. Outside those windows (and after any file is written) every lookup asks the file system, so a
// file made while the game runs (a map compiled in TrenchBroom, a config) is found as before.

#include "vr_engine.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace
{

bool enabled = false;
std::unordered_map<std::string, std::unordered_set<std::string>> dirs; // directory -> its files (not subdirectories)

#ifdef _WIN32
constexpr bool caseInsensitive = true; // Windows' file names: the same file whatever the case (ASCII here)
#else
constexpr bool caseInsensitive = false;
#endif

void fold(std::string& s)
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

const std::unordered_set<std::string>& listing(const std::string& dir)
{
    const auto it = dirs.find(dir);
    if(it != dirs.end())
    {
        return it->second;
    }
    std::unordered_set<std::string>& files = dirs[dir];
    for(findfile_t* f = Sys_FindFirst(dir.c_str(), nullptr); f; f = Sys_FindNext(f))
    {
        if(!(f->attribs & FA_DIRECTORY))
        {
            std::string name = f->name;
            fold(name);
            files.insert(std::move(name));
        }
    }
    return files;
}

} // namespace

extern "C" void VR_FileCacheEnable(int on)
{
    enabled = on != 0;
    dirs.clear();
}

extern "C" void VR_FileCacheForget()
{
    dirs.clear();
}

extern "C" int VR_FileCacheHas(const char* path)
{
    if(!enabled)
    {
        return -1;
    }
    // Only plain names: no directory walks, nothing past ASCII (whose case folding the file system does its own way).
    for(const char* p = path; *p; p++)
    {
        if(static_cast<unsigned char>(*p) >= 0x80 || (p[0] == '.' && p[1] == '.'))
        {
            return -1;
        }
    }
    std::string full = path;
    fold(full);
    const size_t slash = full.find_last_of("/\\");
    if(slash == std::string::npos || slash + 1 == full.size())
    {
        return -1;
    }
    const std::unordered_set<std::string>& files = listing(full.substr(0, slash));
    return files.count(full.substr(slash + 1)) ? 1 : 0;
}
