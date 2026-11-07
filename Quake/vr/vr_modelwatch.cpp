// vr_modelwatch.cpp -- vr_model_watch: the files of the loaded models (alias models in progs/ that a folder has, not a
// pak: the .mdl, and beside it its normal map, full-colour skins, a body's .md5mesh and .md5anim) watched on disk, and
// a model whose file changed read again as vr_model_reload reads it, with a console line. It is what makes the Blender
// add-on's export on save show in the game by itself (docs/vr-port/MODELS_IN_BLENDER.md, "See it in the game").
//
// Cost, nothing changing: on Windows the folders that hold the files are watched by the system (a change
// notification), asked every half second (one wait of 0 ms each: no file looked at). A notification (any write in the
// folder) has every watched file's last write time looked at over the next half seconds, spread evenly over the frames,
// until a half second passes with nothing new. Elsewhere the files are looked at so all the time. The models loaded
// since the last look are listed for 0.2 ms a frame at most, their folders listed once (no lookup of a file that isn't
// there). vr_model_watch_status prints the counts and the time spent.
//
// A change is read when a whole half second has passed without another (an export writes several files: they are read
// at once), and only if the model file is whole (its header, and its size at least what its counts need: a file still
// being written is tried again at the next half second). Real time (the disk's clock), not the game's.

#include "vr_modelwatch.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_limbmodel.hpp"
#include "vr_strings.hpp"
#include "vr_view.hpp"

#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace qvr::modelwatch
{
namespace
{
constexpr double cycleSeconds = 0.5; // every watched file looked at once in this, while looking
constexpr double listSeconds = 0.0002; // a frame's listing of newly loaded models stops after this (a folder's listing,
                                       // once, can take longer: ~1 ms)
// The files a model's load reads besides itself, beside it: appended to its name (the normal map, the full-colour skin
// and what glows), or in place of its extension (the body's enhanced replacement).
constexpr const char* companions[] = {"_0_norm.png", "_0.png", "_0_glow.png", "_0_luma.png"};
constexpr const char* replacements[] = {".md5mesh", ".md5anim"};

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
using Names = ankerl::unordered_dense::set<za::String, NameHash, NameEqual>;   // a folder's files (lower case)
using Listings = ankerl::unordered_dense::map<za::String, Names, NameHash, NameEqual>; // by folder

struct File
{
    int model{0};     // the known model's index (Mod_KnownModel)
    za::String path;  // on disk
    za::I64 stamp{0}; // the last write time last seen
};

struct Folder
{
    za::String path;
#ifdef _WIN32
    HANDLE note{INVALID_HANDLE_VALUE}; // FindFirstChangeNotification's
#endif
};

struct Watch
{
    bool on{false};
    za::String gameDir;            // the files were listed for it (a game change reuses the models' slots)
    za::Vector<File> files;
    za::Vector<Folder> folders;    // the files' folders, watched by the system
    Listings listings;             // the folders listed while models are being listed (dropped after)
    za::Vector<za::String> listed; // by known model index: the name its files were listed for ("": not yet)
    za::Vector<char> changed;      // by known model index: a file of it changed, not read again yet
    int firstUnlisted{0};          // known models before it are listed (or not watched)
    za::SizeT next{0};             // the next file looked at
    double owed{0.0};              // files owed a look, carried from frame to frame
    double lastTime{0.0};
    double nextAsk{0.0};           // the folders' notifications asked for next
    int looking{0};                // cycles still to look at the files (a notification: until a quiet one)
    bool changedThisCycle{false};
    bool waiting{false};           // a changed model not whole yet (said once)
    // vr_model_watch_status's counts, since it was turned on
    za::I64 frames{0}, asks{0}, notes{0}, cycles{0}, looks{0}, changes{0}, reloads{0};
    double seconds{0.0}; // spent here
    double since{0.0};
};

Watch watch;

void closeFolders()
{
#ifdef _WIN32
    for(Folder& f : watch.folders)
    {
        if(f.note != INVALID_HANDLE_VALUE)
        {
            FindCloseChangeNotification(f.note);
        }
    }
#endif
    watch.folders.clear();
}

void forget()
{
    closeFolders();
    watch.gameDir = com_gamedir;
    watch.files.clear();
    watch.listings.clear();
    watch.listed.clear();
    watch.changed.clear();
    watch.firstUnlisted = 0;
    watch.next = 0;
    watch.owed = 0.0;
    watch.looking = 0;
    watch.changedThisCycle = false;
    watch.waiting = false;
}

using strings::lower;

const Names& listing(const za::String& folder)
{
    const auto it = watch.listings.find(za::StringView{folder});
    if(it != watch.listings.end())
    {
        return it->second;
    }
    Names names;
    files::forEachEntry(folder.cStr(), [&](const char* name, bool isDirectory) {
        if(!isDirectory)
        {
            names.insert(lower(za::StringView{name, strlen(name)}));
        }
    });
    return watch.listings.emplace(folder, static_cast<Names&&>(names)).first->second;
}

// `name`'s path on disk when the first search path that has it is a folder (false: a pak has it first, or none does).
[[nodiscard]] bool loosePath(const char* name, za::String& out)
{
    const char* slash = strrchr(name, '/');
    const za::StringView sub = slash ? za::StringView{name, static_cast<za::SizeT>(slash - name)} : za::StringView{};
    const za::String base = lower(za::StringView{slash ? slash + 1 : name});
    for(const searchpath_t* s = com_searchpaths; s; s = s->next)
    {
        if(s->pack)
        {
            if(COM_PackHasFile(s->pack, name))
            {
                return false;
            }
            continue;
        }
        za::String folder{s->filename};
        if(!sub.empty())
        {
            folder += "/";
            folder += sub;
        }
        const Names& names = listing(folder);
        if(names.find(za::StringView{base}) != names.end())
        {
            out = folder + "/" + za::String{slash ? slash + 1 : name};
            return true;
        }
    }
    return false;
}

void watchFolder(const za::String& folder)
{
    for(const Folder& f : watch.folders)
    {
        if(f.path == folder)
        {
            return;
        }
    }
    Folder f;
    f.path = folder;
#ifdef _WIN32
    wchar_t w[MAX_PATH];
    if(MultiByteToWideChar(CP_UTF8, 0, folder.cStr(), -1, w, MAX_PATH) > 0)
    {
        f.note = FindFirstChangeNotificationW(w, FALSE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE);
    }
#endif
    watch.folders.pushBack(static_cast<Folder&&>(f));
}

[[nodiscard]] bool systemWatches()
{
#ifdef _WIN32
    for(const Folder& f : watch.folders)
    {
        if(f.note == INVALID_HANDLE_VALUE)
        {
            return false;
        }
    }
    return true;
#else
    return false;
#endif
}

void addFile(int model, const za::String& path)
{
    watch.files.pushBack(File{model, path, files::lastWriteTime(path.cStr())});
    watchFolder(za::String{path.substrByPosLen(0, path.rfind('/'))});
}

void listModel(int i, const qmodel_t* m)
{
    watch.listed[static_cast<za::SizeT>(i)] = m->name;
    za::String path;
    // (a limb gore model is made from its monster's file, never read from one of its own: no folder listed for it, which
    // late in a map was a directory listing in a frame of play for each new limb model)
    if(strstr(m->name, limbmodel::suffix) || !loosePath(m->name, path))
    {
        return;
    }
    addFile(i, path);
    // (beside it: the export writes them there; a model's load looks for them elsewhere too, rarely edited)
    const za::SizeT slash = path.rfind('/');
    const za::String folder{path.substrByPosLen(0, slash)};
    const za::String file{path.substrByPosLen(slash + 1)};
    const Names& names = listing(folder);
    const auto beside = [&](const za::String& other) {
        if(names.find(za::StringView{lower(za::StringView{other})}) != names.end())
        {
            addFile(i, folder + "/" + other);
        }
    };
    for(const char* c : companions)
    {
        beside(file + c);
    }
    const za::SizeT dot = file.rfind('.');
    for(const char* r : replacements)
    {
        if(dot != za::StringView::nPos)
        {
            beside(file.substrByPosLen(0, dot) + r);
        }
    }
}

// The models loaded since the last call: their files listed, as they are now (a frame's: until listSeconds have gone,
// at least one model; all: every one).
void listNew(bool all)
{
    const double start = Sys_DoubleTime();
    if(strcmp(watch.gameDir.cStr(), com_gamedir) != 0)
    {
        forget();
    }
    const int known = Mod_KnownCount();
    while(static_cast<int>(watch.listed.size()) < known)
    {
        watch.listed.emplaceBack();
        watch.changed.pushBack(0);
    }
    int i = watch.firstUnlisted;
    bool listedOne = false;
    for(; i < known && (all || !listedOne || Sys_DoubleTime() - start < listSeconds); i++)
    {
        const qmodel_t* m = Mod_KnownModel(i);
        const bool watched = m && m->type == mod_alias && !q_strncasecmp(m->name, "progs/", 6);
        if(watched && m->needload)
        {
            break; // (not loaded yet: listed when it is)
        }
        if(watched)
        {
            listModel(i, m);
            listedOne = true;
        }
    }
    watch.firstUnlisted = i;
    if(i >= known)
    {
        watch.listings.clear(); // (all listed: a folder's listing is old by the next model)
    }
}

// The model file is whole: an alias model's header and at least the size its counts need (a file being written is
// shorter). Any other kind: as it is.
[[nodiscard]] bool whole(const char* name)
{
    za::String path;
    const bool loose = loosePath(name, path);
    watch.listings.clear();
    if(!loose)
    {
        return true; // (a pak's now: read as vr_model_reload reads it)
    }
    FILE* f = fopen(path.cStr(), "rb");
    if(!f)
    {
        return false;
    }
    unsigned char h[84];
    const bool headed = fread(h, 1, sizeof(h), f) == sizeof(h);
    fseek(f, 0, SEEK_END);
    const za::I64 size = ftell(f);
    fclose(f);
    if(!headed)
    {
        return false;
    }
    if(memcmp(h, "IDPO", 4) != 0)
    {
        return true;
    }
    const auto at = [&](int ofs) {
        int v;
        memcpy(&v, h + ofs, sizeof(v));
        return static_cast<za::I64>(LittleLong(v));
    };
    const za::I64 skins = at(48), w = at(52), hgt = at(56), verts = at(60), tris = at(64), frames = at(68);
    if(at(4) != ALIAS_VERSION || skins < 0 || w < 0 || hgt < 0 || verts < 0 || tris < 0 || frames < 0)
    {
        return false;
    }
    // (each skin at least a type and its texels, each frame at least a type, its box and name, and its vertices)
    return size >= 84 + skins * (4 + w * hgt) + verts * 12 + tris * 16 + frames * (28 + verts * 4);
}

void readChanged()
{
    za::Vector<za::String> names;
    za::String list;
    bool partial = false;
    for(za::SizeT i = 0; i < watch.changed.size(); i++)
    {
        if(!watch.changed[i])
        {
            continue;
        }
        const qmodel_t* m = Mod_KnownModel(static_cast<int>(i));
        if(!m || strcmp(m->name, watch.listed[i].cStr()) != 0)
        {
            watch.changed[i] = 0;
            continue;
        }
        if(!whole(m->name))
        {
            partial = true;
            continue;
        }
        watch.changed[i] = 0;
        names.emplaceBack(m->name);
        list += (list.empty() ? "" : ", ") + za::String{m->name};
    }
    if(partial && !watch.waiting)
    {
        Con_Printf("vr_model_watch: a changed model isn't whole yet (still being written?): read when it is\n");
    }
    watch.waiting = partial;
    if(names.empty())
    {
        return;
    }
    Con_Printf("vr_model_watch: changed on disk, read again: %s\n", list.cStr());
    view::reloadModels(names);
    watch.reloads++;
}

void endCycle()
{
    watch.cycles++;
    bool any = false;
    for(const char c : watch.changed)
    {
        any = any || c != 0;
    }
    // (read once a whole cycle saw no new change: the files an export writes one after another read together)
    if(any && !watch.changedThisCycle)
    {
        readChanged();
    }
    if(watch.changedThisCycle || watch.waiting)
    {
        watch.looking = 2; // (looking on until a quiet cycle has read it)
    }
    else if(watch.looking > 0)
    {
        watch.looking--;
    }
    watch.changedThisCycle = false;
}

// The folders' notifications: a write in one has the files looked at (until a quiet cycle).
void askFolders()
{
#ifdef _WIN32
    watch.asks++;
    for(Folder& f : watch.folders)
    {
        if(f.note != INVALID_HANDLE_VALUE && WaitForSingleObject(f.note, 0) == WAIT_OBJECT_0)
        {
            FindNextChangeNotification(f.note);
            watch.notes++;
            if(watch.looking == 0)
            {
                watch.next = 0; // (a whole cycle from the first file)
                watch.owed = 0.0;
            }
            watch.looking = 2;
        }
    }
#endif
}

void status_f()
{
    if(!watch.on)
    {
        Con_Printf("vr_model_watch: off\n");
        return;
    }
    listNew(true);
    int models = 0;
    int last = -1;
    for(const File& f : watch.files)
    {
        models += f.model != last ? 1 : 0; // (a model's files are listed together)
        last = f.model;
    }
    const double secs = Sys_DoubleTime() - watch.since;
    Con_Printf("vr_model_watch: %d files of %d models in %d folders watched (%s); in %.1f s, %lld frames: %lld asks, "
               "%lld notifications, %lld cycles, %lld looks, %lld changes, %lld reloads; %.3f ms spent (%.2f us a "
               "frame)\n",
        static_cast<int>(watch.files.size()), models, static_cast<int>(watch.folders.size()),
        systemWatches() ? "by the system" : "looked at all the time", secs, static_cast<long long>(watch.frames),
        static_cast<long long>(watch.asks), static_cast<long long>(watch.notes), static_cast<long long>(watch.cycles),
        static_cast<long long>(watch.looks), static_cast<long long>(watch.changes),
        static_cast<long long>(watch.reloads), watch.seconds * 1000.0,
        watch.frames ? watch.seconds * 1e6 / static_cast<double>(watch.frames) : 0.0);
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_model_watch_status", status_f);
}

void frame()
{
    if(vr_model_watch.value == 0.f)
    {
        if(watch.on)
        {
            forget();
            watch.on = false;
        }
        return;
    }
    const double now = Sys_DoubleTime();
    if(!watch.on)
    {
        forget();
        watch = Watch{};
        watch.gameDir = com_gamedir;
        watch.on = true;
        watch.lastTime = watch.since = watch.nextAsk = now;
    }
    const double dt = za::clamp(now - watch.lastTime, 0.0, cycleSeconds);
    watch.lastTime = now;
    watch.frames++;
    if(watch.firstUnlisted < Mod_KnownCount() || strcmp(watch.gameDir.cStr(), com_gamedir) != 0)
    {
        listNew(false);
    }
    if(watch.files.empty())
    {
        watch.seconds += Sys_DoubleTime() - now;
        return;
    }
    if(now >= watch.nextAsk)
    {
        watch.nextAsk = now + cycleSeconds;
        askFolders();
    }
    if(watch.looking > 0 || !systemWatches())
    {
        watch.owed += static_cast<double>(watch.files.size()) * dt / cycleSeconds;
        za::SizeT n = static_cast<za::SizeT>(watch.owed);
        watch.owed -= static_cast<double>(n);
        n = za::min(n, watch.files.size());
        for(za::SizeT k = 0; k < n; k++)
        {
            File& f = watch.files[watch.next];
            const za::I64 stamp = files::lastWriteTime(f.path.cStr());
            watch.looks++;
            if(stamp != f.stamp)
            {
                f.stamp = stamp;
                watch.changed[static_cast<za::SizeT>(f.model)] = 1;
                watch.changedThisCycle = true;
                watch.changes++;
            }
            if(++watch.next == watch.files.size())
            {
                watch.next = 0;
                endCycle(); // (may read models again: the list stays, the slots are the same)
            }
        }
    }
    watch.seconds += Sys_DoubleTime() - now;
}

} // namespace qvr::modelwatch
