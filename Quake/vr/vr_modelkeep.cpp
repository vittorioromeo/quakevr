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
#include "Zancle/String/StringView.hpp"

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

// A pak's path, looked up by a view of the caller's string (a transparent hash): no za::String made for a lookup.
struct PathHash
{
    using is_transparent = void;
    [[nodiscard]] za::U64 operator()(za::StringView s) const { return ankerl::unordered_dense::hash<za::StringView>{}(s); }
};
struct PathEqual
{
    using is_transparent = void;
    [[nodiscard]] bool operator()(za::StringView a, za::StringView b) const { return a == b; }
};

struct KeepState
{
    ankerl::unordered_dense::map<const qmodel_t*, za::Vector<Lookup>> sources; // an alias model's lookups (its load's)
    za::Vector<const qmodel_t*> recording; // the loads under way (a model's load may load another)
    // A pak's write time (read once: open, it is not rewritten).
    ankerl::unordered_dense::map<za::String, za::I64, PathHash, PathEqual> pakTimes;
    za::Vector<Lookup> before; // the palette's and colormap's, before the switch's new folders
    ankerl::unordered_dense::set<const qmodel_t*> kept; // the switch's (VR_ModelsKeepDecide to VR_ModelsKeepEnd)
    ankerl::unordered_dense::set<const cache_user_t*> keptCaches;
    za::Vector<qmodel_t*> boxes; // the item boxes kept across maps (VR_MapModelsLoad), in their hunk layer
    auto members() { return qvr::mem::list(sources, recording, pakTimes, before, kept, keptCaches, boxes); }
};
qvr::mem::Cache<KeepState> state{"campaign switch: models' files", qvr::mem::Never};

za::ThreadId owner; // the main thread (the loads' recorder: other threads' lookups are not theirs)
bool campaign = false; // the switch under way is a campaign's
// The last switch (vr_model_keep_info)
int lastKept = 0, lastLoaded = 0, lastChecks = 0;
double lastMs = 0.0;
const char* lastWhy = "no campaign switch yet";

constexpr const char* globalFiles[] = {"gfx/palette.lmp", "gfx/colormap.lmp"};

// The item boxes every map precaches (QC VR_Toolgun_Precache: the toolgun's health, ammo and explosive boxes; the
// expansions' where their pak is): small brush models, the same files every map, whose load was ~70 ms of each (their
// textures made and uploaded again: a brush model lives on the hunk, Mod_ClearAll frees its textures). Loaded once
// before the map's own models, under host_hunklevel (Host_KeepMapModels), they stay with their textures, as alias
// models do, until the game's files change: each map's Host_ClearMemory compares the search paths (every game folder
// and pak, in order) and the files the boxes, the palette and the colormap are found in with those of their load (a
// load looks for ~420 files a box, mostly replacement images that aren't there: not looked for again); any other, or
// vr_keep_item_boxes 0, and they are freed and loaded again (or not). A campaign switch (Mod_ResetAll) empties them.
constexpr const char* itemBoxes[] = {"maps/b_bh10.bsp", "maps/b_bh25.bsp", "maps/b_bh100.bsp", "maps/b_shell0.bsp",
    "maps/b_shell1.bsp", "maps/b_nail0.bsp", "maps/b_nail1.bsp", "maps/b_rock0.bsp", "maps/b_rock1.bsp",
    "maps/b_batt0.bsp", "maps/b_batt1.bsp", "maps/b_explob.bsp", "maps/b_exbox2.bsp", "maps/b_lnail0.bsp",
    "maps/b_lnail1.bsp", "maps/b_mrock0.bsp", "maps/b_mrock1.bsp", "maps/b_plas0.bsp", "maps/b_plas1.bsp"};
za::U64 boxesFiles = 0; // the boxes' load's filesNow()
// The last load and check (vr_model_keep_info)
int boxLoads = 0, boxKeeps = 0;
double boxCheckMs = 0.0, boxLoadMs = 0.0;
const char* boxWhy = "";

[[nodiscard]] za::I64 pakTime(const char* pak)
{
    const auto it = state.pakTimes.find(za::StringView{pak});
    if(it != state.pakTimes.end())
    {
        return it->second;
    }
    const za::I64 t = qvr::files::lastWriteTime(pak);
    state.pakTimes.emplace(za::String{pak}, t);
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

// The item boxes' files as the game finds them now (a hash): the search paths in order, and the file each box, the
// palette and the colormap is found in.
[[nodiscard]] za::U64 filesNow()
{
    za::U64 h = 14695981039346656037ull; // FNV-1a
    const auto mix = [&h](const void* p, za::SizeT n)
    {
        for(za::SizeT i = 0; i < n; ++i)
        {
            h = (h ^ static_cast<const unsigned char*>(p)[i]) * 1099511628211ull;
        }
    };
    const auto mixString = [&mix](const char* s) { mix(s, strlen(s) + 1); };
    for(const searchpath_t* s = com_searchpaths; s; s = s->next)
    {
        mixString(s->pack ? s->pack->filename : s->filename);
        mix(&s->path_id, sizeof(s->path_id));
    }
    const auto mixFile = [&](const char* name)
    {
        const FileId f = findNow(name);
        mixString(name);
        mix(&f.found, sizeof(f.found));
        mixString(f.source.cStr());
        mix(&f.offset, sizeof(f.offset));
        mix(&f.size, sizeof(f.size));
        mix(&f.writeTime, sizeof(f.writeTime));
    };
    for(const char* name : globalFiles)
    {
        mixFile(name);
    }
    for(const char* name : itemBoxes)
    {
        mixFile(name);
    }
    return h;
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
    Con_Printf("vr_model_keep: %d item boxes kept across maps (vr_keep_item_boxes %s); %d loads of them (the last %.1f ms), "
               "%d kept by the last check (%.2f ms)%s%s\n",
        static_cast<int>(state.boxes.size()), qvr::vr_keep_item_boxes.string, boxLoads, boxLoadMs, boxKeeps, boxCheckMs,
        boxWhy[0] ? ": " : "", boxWhy);
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

extern "C" int VR_MapModelKept(const qmodel_t* mod)
{
    return za::find(state.boxes.begin(), state.boxes.end(), mod) != state.boxes.end() ? 1 : 0;
}

extern "C" int VR_MapModelsCheck()
{
    if(state.boxes.empty())
    {
        return 0;
    }
    const double t0 = Sys_DoubleTime();
    const bool on = qvr::vr_keep_item_boxes.value != 0.f;
    bool same = on && filesNow() == boxesFiles;
    for(za::SizeT i = 0; same && i < state.boxes.size(); ++i)
    {
        same = !state.boxes[i]->needload && state.boxes[i]->type == mod_brush; // (Mod_ResetAll empties their slots)
    }
    boxCheckMs = (Sys_DoubleTime() - t0) * 1000.0;
    boxKeeps = same ? static_cast<int>(state.boxes.size()) : 0;
    if(!same)
    {
        boxWhy = on ? "a file of theirs is found elsewhere now: loaded again" : "vr_keep_item_boxes 0";
        Con_DPrintf("vr_model_keep: the item boxes freed (%s)\n", boxWhy);
        state.boxes.clear(); // (Mod_ClearAll frees them as any brush model)
        return 0;
    }
    boxWhy = "";
    return 1;
}

extern "C" int VR_MapModelsLoad()
{
    if(!state.boxes.empty() || qvr::vr_keep_item_boxes.value == 0.f)
    {
        return 0;
    }
    const double t0 = Sys_DoubleTime();
    boxesFiles = filesNow();
    for(const char* name : itemBoxes)
    {
        if(!COM_FileExists(name, nullptr))
        {
            continue;
        }
        qmodel_t* mod = Mod_ForName(name, false);
        if(mod && mod->type == mod_brush)
        {
            state.boxes.pushBack(mod);
        }
    }
    boxLoadMs = (Sys_DoubleTime() - t0) * 1000.0;
    ++boxLoads;
    return state.boxes.empty() ? 0 : 1;
}
