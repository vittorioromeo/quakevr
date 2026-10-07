#include "vr_alloccount.h"
// vr_modelkeep.cpp -- a campaign switch keeps the alias models whose files are the same in its game folders.
//
// A switch of campaign (id1 to Scourge of Armagon and back: COM_ReloadVRGame) changes the game folders, and Ironwail then
// forgets every model (Mod_ResetAll), so the next map loads them all again: hip1m1 and back, 600 ms of alias models
// each way, though most are id1's own files both times.
//
// An alias model's load records every file it looks for, found or not (COM_FindFile while it loads, from its .mdl to
// its replacement models, external skins and authored normal maps; VR_FileLookupNoted): the file found (a pak, where its
// entry starts and its length, or a file of its own, its size), and its write time. At a campaign switch, once the new
// folders are in (VR_ModelsKeepDecide), each model's lookups are made again: a model kept is one each of whose lookups
// finds the same file again, the ones found nowhere still found nowhere (a mod's own model, skin or normal map of the
// same name, now first in the search path, is found instead: reloaded). The palette's and colormap's files likewise, for
// every model (their skins are made with them): either changed, nothing is kept. A kept model stays in its slot
// (Mod_ResetAll), its cache entry (Cache_FlushExcept), its textures (TexMgr_NewGame) and buffers
// (GLMesh_DeleteVertexBuffers); the VR caches that hold models still empty themselves (VR_OnGameDirChanged) and are
// made again for it. Only a campaign switch keeps models: the `game` command runs the new game's configuration, whose
// settings may load them otherwise. vr_campaign_keep_models 0: none kept.

#include "vr_modelkeep.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_mem.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

extern "C" void VR_FileCacheEnable(int on); // vr_fscache.cpp
extern "C" int VR_FileCacheEnabled();

namespace
{

// A lookup's result: the file found (none: found false), as COM_FindFile left it.
struct FileId
{
    bool found{false};
    za::String source; // the pak, or the file's own path
    za::I64 offset{0}; // where its entry starts in the pak (a file of its own: 0)
    za::I64 size{-1};
    za::I64 writeTime{0}; // the pak's or the file's
    [[nodiscard]] bool operator==(const FileId&) const = default;
};

struct Lookup
{
    za::String name;
    FileId file;
};

[[nodiscard]] za::SizeT heldBytes(const Lookup& l)
{
    return qvr::mem::heldBytes(l.name) + qvr::mem::heldBytes(l.file.source);
}

struct KeepState
{
    ankerl::unordered_dense::map<const qmodel_t*, za::Vector<Lookup>> sources; // an alias model's lookups (its load's)
    za::Vector<const qmodel_t*> recording; // the loads under way (a model's load may load another)
    ankerl::unordered_dense::map<za::String, za::I64> pakTimes; // a pak's write time (read once: open, it is not rewritten)
    za::Vector<Lookup> before; // the palette's and colormap's, before the switch's new folders
    ankerl::unordered_dense::set<const qmodel_t*> kept; // the switch's (VR_ModelsKeepDecide to VR_ModelsKeepEnd)
    ankerl::unordered_dense::set<const cache_user_t*> keptCaches;
    auto members() { return qvr::mem::list(sources, recording, pakTimes, before, kept, keptCaches); }
};
qvr::mem::Cache<KeepState> state{"campaign switch: models' files", qvr::mem::Never};

za::ThreadId owner; // the main thread (the loads' recorder: other threads' lookups are not theirs)
bool campaign = false; // the switch under way is a campaign's
// The last switch (vr_model_keep_info)
int lastKept = 0, lastLoaded = 0, lastChecks = 0;
double lastMs = 0.0;
const char* lastWhy = "no campaign switch yet";

constexpr const char* globalFiles[] = {"gfx/palette.lmp", "gfx/colormap.lmp"};

[[nodiscard]] za::I64 pakTime(const char* pak)
{
    const auto it = state.pakTimes.find(za::String{pak});
    if(it != state.pakTimes.end())
    {
        return it->second;
    }
    const za::I64 t = qvr::files::lastWriteTime(pak);
    state.pakTimes[za::String{pak}] = t;
    return t;
}

// The file the lookup of `name` just made found (COM_FindFile's globals).
[[nodiscard]] FileId lastFound(const char* name, bool found)
{
    FileId f;
    f.found = found;
    if(!found)
    {
        return f;
    }
    if(file_from_pak)
    {
        f.source = com_filesource;
        f.offset = com_fileoffset;
        f.size = com_filesize;
        f.writeTime = pakTime(com_filesource);
    }
    else
    {
        f.source = za::String{com_filesource} + "/" + name;
        f.size = static_cast<za::I64>(qvr::files::fileSize(f.source.cStr()));
        f.writeTime = qvr::files::lastWriteTime(f.source.cStr());
    }
    return f;
}

// The file a lookup of `name` finds now.
[[nodiscard]] FileId findNow(const char* name)
{
    const int noted = com_lookups_noted;
    com_lookups_noted = 0; // (not a load's)
    const bool found = COM_FileExists(name, nullptr);
    com_lookups_noted = noted;
    return lastFound(name, found);
}

void info_f()
{
    if(Cmd_Argc() > 1) // a model's recorded lookups
    {
        for(const auto& [mod, lookups] : state.sources)
        {
            if(q_strcasecmp(mod->name, Cmd_Argv(1)) != 0)
            {
                continue;
            }
            for(const Lookup& l : lookups)
            {
                Con_Printf("  %s: %s\n", l.name.cStr(), l.file.found ? l.file.source.cStr() : "(none)");
            }
            Con_Printf("vr_model_keep_info: %s, %d files looked for\n", mod->name, static_cast<int>(lookups.size()));
        }
        return;
    }
    Con_Printf("vr_model_keep: the last campaign switch kept %d of %d alias models (%d files looked for again, %.1f ms)%s%s; "
               "%d models' files recorded (vr_campaign_keep_models %s)\n",
        lastKept, lastLoaded, lastChecks, lastMs, lastWhy[0] ? ": " : "", lastWhy,
        static_cast<int>(state.sources.size()), qvr::vr_campaign_keep_models.string);
}

} // namespace

namespace qvr::modelkeep
{

void init()
{
    Cmd_AddCommand("vr_model_keep_info", info_f);
}

} // namespace qvr::modelkeep

extern "C" void VR_ModelSourcesBegin(qmodel_t* mod, const char* file)
{
    if(owner == za::ThreadId{})
    {
        owner = za::ThisThread::getId();
    }
    if(za::ThisThread::getId() != owner)
    {
        return;
    }
    za::Vector<Lookup>& lookups = state.sources[mod];
    lookups.clear();
    lookups.pushBack(Lookup{za::String{file}, lastFound(file, true)}); // (its own file: the lookup just made)
    state.recording.pushBack(mod);
    com_lookups_noted = 1;
}

extern "C" void VR_ModelSourcesEnd(qmodel_t* mod)
{
    if(za::ThisThread::getId() != owner)
    {
        return;
    }
    za::Vector<const qmodel_t*>& r = state.recording;
    for(za::SizeT i = r.size(); i-- > 0;)
    {
        if(r[i] == mod)
        {
            r.erase(r.begin() + static_cast<za::PtrDiffT>(i));
            break;
        }
    }
    com_lookups_noted = r.empty() ? 0 : 1;
}

extern "C" void VR_ModelSourcesForget(const qmodel_t* mod)
{
    const auto it = state.sources.find(mod);
    if(it != state.sources.end())
    {
        state.sources.erase(it);
    }
    za::Vector<const qmodel_t*>& r = state.recording; // (a load that never ended: an error's longjmp)
    for(za::SizeT i = r.size(); i-- > 0;)
    {
        if(r[i] == mod)
        {
            r.erase(r.begin() + static_cast<za::PtrDiffT>(i));
        }
    }
    com_lookups_noted = r.empty() ? 0 : 1;
}

extern "C" void VR_FileLookupNoted(const char* name, int found)
{
    if(state.recording.empty() || za::ThisThread::getId() != owner)
    {
        return;
    }
    za::Vector<Lookup>& lookups = state.sources[state.recording.back()];
    for(const Lookup& l : lookups)
    {
        if(l.name == name)
        {
            return; // (looked for again: the same answer)
        }
    }
    lookups.pushBack(Lookup{za::String{name}, lastFound(name, found != 0)});
}

extern "C" void VR_ModelsKeepBefore(int campaignSwitch)
{
    state.kept.clear();
    state.keptCaches.clear();
    state.before.clear();
    campaign = campaignSwitch != 0;
    if(!campaign)
    {
        return;
    }
    for(const char* name : globalFiles)
    {
        state.before.pushBack(Lookup{za::String{name}, findNow(name)});
    }
}

extern "C" void VR_ModelsKeepDecide()
{
    const double t0 = Sys_DoubleTime();
    lastKept = lastLoaded = lastChecks = 0;
    state.pakTimes.clear(); // (the new folders' paks: their times read again, once)
    int loaded = 0;
    for(const auto& [mod, lookups] : state.sources)
    {
        if(mod->type == mod_alias && !mod->needload && mod->cache.data)
        {
            ++loaded;
        }
    }
    lastLoaded = loaded;
    if(!campaign)
    {
        lastWhy = "not a campaign switch (the game command: none kept)";
        return;
    }
    if(qvr::vr_campaign_keep_models.value == 0.f)
    {
        lastWhy = "vr_campaign_keep_models 0";
        return;
    }
    for(const Lookup& l : state.before)
    {
        ++lastChecks;
        if(!(findNow(l.name.cStr()) == l.file))
        {
            lastWhy = "the palette or the colormap is another file: every model made again";
            return;
        }
    }
    const int listed = VR_FileCacheEnabled();
    VR_FileCacheEnable(1); // (each folder listed once for the lookups below, as in a map's load)
    for(const auto& [mod, lookups] : state.sources)
    {
        if(mod->type != mod_alias || mod->needload || !mod->cache.data || lookups.empty())
        {
            continue;
        }
        bool same = true;
        for(const Lookup& l : lookups)
        {
            ++lastChecks;
            if(!(findNow(l.name.cStr()) == l.file))
            {
                same = false;
                break;
            }
        }
        if(same)
        {
            state.kept.insert(mod);
            state.keptCaches.insert(&mod->cache);
        }
    }
    VR_FileCacheEnable(listed);
    lastKept = static_cast<int>(state.kept.size());
    lastMs = (Sys_DoubleTime() - t0) * 1000.0;
    lastWhy = "";
    Con_DPrintf("vr_model_keep: %d of %d alias models kept (their files the same; %d looked for again, %.1f ms)\n", lastKept,
        lastLoaded, lastChecks, lastMs);
}

extern "C" int VR_ModelKept(const qmodel_t* mod)
{
    return !state.kept.empty() && state.kept.find(mod) != state.kept.end();
}

extern "C" qboolean VR_ModelCacheKept(cache_user_t* c)
{
    return !state.keptCaches.empty() && state.keptCaches.find(c) != state.keptCaches.end() ? true : false;
}

extern "C" void VR_ModelsKeepEnd()
{
    state.kept.clear();
    state.keptCaches.clear();
    state.before.clear();
    campaign = false;
}
