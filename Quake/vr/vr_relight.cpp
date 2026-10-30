// vr_relight.cpp -- see vr_relight.hpp.
//
// Relighting a map in the game (vr_relight; VR Settings > Advanced VR Options > Graphics > Relighting): the map's
// .bsp (the copy relight_maps.py made in quakevr/relit, with its see-through water, if there is one and See-Through
// Liquids is on; else the map itself, as the game finds it) is given ericw-tools' `light` with:
//
// - the lights relight_maps.py gives the glowing textures (glowLights: a port of its glow_lights, which see): the
//   lamps and light panels (fixtures), glowing buttons and panels, lava and slime, by the rules of
//   quakevr/relight_textures.cfg, their glow found where the engine finds it (Glows: fullbright pixels, a replacement
//   texture's _glow or _luma, the material maps'), each kind as bright as the page's sliders say on top of the rules'
//   strengths (vr_relight_strength, _lamps, _glows, _liquids);
// - the map's own lights, as bright as Map Lights says (vr_relight_maplights: "light" and the linear falloff's "wait"
//   scaled together: as bright, as far), and its sunlight as Sunlight says (vr_relight_sunlight);
// - the look's options: ambient occlusion (vr_relight_ao: -dirt -dirtscale), bounced light (vr_relight_bounce),
//   an ambient light level (vr_relight_minlight: added to the worldspawn's minlight, undarkened), smooth or fast shadows (vr_relight_quality: -extra4 or -extra), the light
//   grid and the light directions (-lightgrid -lux) and coloured light (-lit), as relight_maps.py gives them.
//
// light runs as a process of its own (vr_relight_process.cpp), below normal priority, on the copy in the work folder
// (<game folder>/relit_custom/_work/<game>/); its output is read for the progress the page shows. When it ends, the map
// keeps its own entities again (the lights were for light only), id's maps get VisPatch's water-vised visibility (See-
// Through Liquids, vr_relight_seethrough: vr_relight_vis.cpp, as relight_maps.py does after light; when its data files
// are found and have the map) and the result goes to <game folder>/relit_custom/
// <game>/maps/<map>.bsp, .lit and .lux, which the engine loads over relit/ (VR_ModelFile; vr_relight_use 0: not), with
// a <map>.relight saying how it was made (and a hash of the settings, the map's file and relight_textures.cfg: a batch
// skips a map relit from the same; and the map's own file's size and hash: a copy made from another version of the map
// is moved aside at its load, customCurrent). Each written as .tmp and renamed into place; the work folder's copies removed.
// id's paks and the game folders' maps are only read. Then the map is reloaded where you are (vr_relight_reload: a
// quick save and load; where the game cannot save, the map restarted).
//
// A batch (vr_relight_batch; the page's Many Maps): an episode, a game, the Map Library's maps or every map
// (vr_relight_maps.cpp finds them), lit one after another, vr_relight_parallel side by side (light's threads shared
// out), with the settings as they were when it started. The map in play goes first; it is reloaded when it is done
// (or at the end: vr_relight_batch_reload). Cancel (and a quit) stops every light and removes its half-made files: the
// maps done are kept. The progress (the page's bar, the wrist gadget's line, the flat screen's corner) weighs each map
// by its file's size and its light run by its stage (stageFraction).
//
// relight_maps.py is the reference: with the page's settings at their defaults the lights given to light are the
// script's (vr_relight_lights writes them, to compare), and the water-vis patch is its vis_maps.py's (the same bytes:
// vr_relight_vispatch patches a file to compare). What neither does: the BSP2 maps' glowing textures.

#include "vr_bytes.hpp"
#include "vr_strings.hpp"
#include "vr_relight.hpp"
#include "vr_relight_maps.hpp"
#include "vr_relight_tool.hpp"
#include "vr_relight_vis.hpp"

#include "vr_brushbake.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_gadget.hpp"
#include "vr_mem.hpp"
#include "vr_relight_internal.hpp"

#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
void VR_HeapFree(void* pointer);                                       // vr_alloccount.cpp
void VR_FileCacheEnable(int on);                                       // vr_fscache.cpp
void VR_FileCacheForget();                                             // vr_fscache.cpp
int VR_MapGameFolder(const char* name, char* out, size_t size);        // vr_gamedir.cpp
}

namespace qvr::relight
{
using namespace detail;


namespace process // vr_relight_process.cpp: a light process a slot (0..maxSlots-1)
{
bool start(int slot, const za::String& exe, const za::Vector<za::String>& args, const za::String& cwd,
    const za::String& log, za::String& error);
bool running(int slot);
int lastExitCode(int slot);
int processId(int slot);
void stopAll(bool closeJob);
za::String onPath(const char* name);
} // namespace process

namespace detail
{



// The glow images measured, by "<extmaps>|<extmaps dir>|<map>/<texture>" (main thread): a map relit again (its
// settings tried one after another) does not look for them and decode them again. The files are the game folders':
// emptied when they change.
struct GlowCache
{
    ankerl::unordered_dense::map<za::String, Measured> measured;
    auto members()
    {
        return mem::list(measured);
    }
};
mem::Cache<GlowCache> glowCache{"relight glow images", mem::GameDirChange};

[[nodiscard]] Measured cachedGlowImage(const char* map, const char* name)
{
    za::String key{va("%g|%s|%s/%s", vr_extmaps.value, vr_extmaps_dir.string, map, name)};
    const auto found = glowCache.measured.find(key);
    if(found != glowCache.measured.end())
    {
        return found->second;
    }
    const Measured m = glowImage(map, name);
    glowCache.measured.emplace(ZA_MOVE(key), m);
    return m;
}


// ---------------------------------------------------------------------------------------------------------------------
// The batch: the maps relit one after another, a few side by side (vr_relight_parallel), each light in a slot of its
// own (main thread). Relight This Map (vr_relight) is a batch of one.

constexpr int maxSlots = 8; // vr_relight_parallel's most (vr_relight_process.cpp's slots)

// The page's settings, taken when a batch starts: its maps are all lit alike, whatever the sliders do meanwhile.
struct Look
{
    float strength{1.2f}, lamps{1.f}, glows{1.f}, liquids{1.f}, maplights{1.f}, sunlight{1.f};
    float bounce{0.f}, ao{1.5f}, minlight{0.f}, quality{1.f};
    bool seeThrough{true}; // vr_relight_seethrough (not in settingsText: each map's hash has its patch, or none)

    [[nodiscard]] static Look now()
    {
        Look l;
        l.strength = vr_relight_strength.value;
        l.lamps = vr_relight_lamps.value;
        l.glows = vr_relight_glows.value;
        l.liquids = vr_relight_liquids.value;
        l.maplights = vr_relight_maplights.value;
        l.sunlight = vr_relight_sunlight.value;
        l.bounce = vr_relight_bounce.value;
        l.ao = vr_relight_ao.value;
        l.minlight = vr_relight_minlight.value;
        l.quality = vr_relight_quality.value;
        l.seeThrough = vr_relight_seethrough.value != 0.f;
        return l;
    }
};

enum class State
{
    Queued,
    Running,
    Done,
    Skipped,   // relit with the same settings already (the .relight's hash)
    Failed,
    Cancelled,
};

// One map of the batch.
struct Item
{
    maps::Source source;
    State state{State::Queued};
    double weight{1.0}; // its share of the work: its file's size (light's time grows with the map)
    float fraction{0.f}; // how far its light is (0..1, while it runs)
    double seconds{0.0}; // its light's time, when relit (the batch's summary)
};

// A light running: its map, its files in the work folder, what its log said.
struct Slot
{
    int item{-1}; // -1: free
    za::String stem;     // <work>/<game>/<map>: light's .bsp, .lit, .lux and -light.log; our .txt log
    za::String original; // the map's own entities, put back after
    za::String brushNotes; // the full bake's (vr_brush_bake_map): its brushes' lines, kept with them after
    za::String hash;     // what it was lit from (the .relight's hash line)
    za::String source;   // the map's own file it was lit from (the .relight's source line: sourceId)
    za::I64 inputWritten{0}; // the input .bsp's write time: light's results are newer
    za::SizeT logRead{0};
    za::Vector<char> logTail; // the log's last bytes (a failure's lines)
    char stage[64]{};
    int percent{-1};
    int lights{0};
    double started{0.0}; // (Sys_DoubleTime)
    bool patched{false}; // See-Through Liquids: `patch` goes in after light
    vis::Patch patch;
};

struct Batch
{
    bool active{false};
    bool single{false}; // vr_relight: one map (its messages as before)
    bool force{false};  // maps relit with these settings already done again
    bool brushes{false}; // the map in play relit with its runtime brushes in it (vr_brush_bake_map: vr_brushbake.cpp)
    za::String bakeMap; // a separate playable map; never overrides the source map
    za::Vector<Item> items;
    Slot slots[maxSlots];
    int parallel{1};
    int threads{1};
    za::String tool;
    za::String root; // <gamedir>/relit_custom (where it started: a game switch meanwhile changes nothing)
    Look look;
    za::String lookText; // settingsText: the .relight's line, the page
    za::String lookKey;  // the settings, light's options and relight_textures.cfg: hashed with each map's file
    double started{0.0};
    double ended{0.0};
    double nextPoll{0.0};
    za::SizeT next{0}; // the next queued item
    int relit{0}, skipped{0}, failed{0}, cancelled{0};
    int alreadyLit{0}; // Quake VR's own maps passed over (maps::Source::prelit: lit as they ship), not in `items`
    // The map in play, when the batch relights it (vr_relight_reload): reloaded when it is done, or at the batch's end
    // (vr_relight_batch_reload).
    char reloadMap[MAX_QPATH]{};
    char reloadGame[MAX_QPATH]{};
    bool reloadAtEnd{false};
    bool reloadPending{false};
    char failedMaps[128]{}; // the first few failures, for the summary
};
Batch batch;
bool saving = false; // the relit map in play saved, its save being written, before the load (poll)
za::String whenDone;  // vr_relight_whendone's commands: run when the batch ends (scripted tests: the end's wait)

// vr_relight_whendone's commands run (the batch ended or was cancelled).
void runWhenDone()
{
    if(!whenDone.empty())
    {
        Cbuf_AddText(va("%s\n", whenDone.cStr()));
        whenDone.clear();
    }
}

// The page's and the commands' last word on it (statusLine).
char status[384] = "";

void say(const char* text)
{
    q_strlcpy(status, text, sizeof(status));
    Con_Printf("Relight: %s\n", text);
}

// The game folder relit_custom/ is written in (where the saves go: com_gamedir).
[[nodiscard]] za::String customRoot()
{
    return files::join(za::StringView{com_gamedir}, za::StringView{"relit_custom"});
}

[[nodiscard]] Strengths strengths(const Look& look)
{
    Strengths s;
    s.scale = za::max(0.0, static_cast<double>(look.strength));
    s.kinds[0] = za::max(0.0, static_cast<double>(look.lamps));
    s.kinds[1] = za::max(0.0, static_cast<double>(look.glows));
    s.kinds[2] = za::max(0.0, static_cast<double>(look.liquids));
    return s;
}

// light's options for the look (relight_maps.py's DEFAULT_LIGHT_ARGS at the defaults), without -threads.
void lookOptions(za::Vector<za::String>& args, const Look& look)
{
    const auto add = [&](const char* a) { args.pushBack(za::String{a}); };
    add(look.quality >= 1.f ? "-extra4" : "-extra");
    if(look.ao > 0.f)
    {
        add("-dirt");
        add("-dirtscale");
        add(va("%g", look.ao));
        add("-dirtdepth");
        add("96");
    }
    if(look.bounce > 0.f)
    {
        add("-bounce");
        add("-bouncescale");
        add(va("%g", look.bounce));
    }
    // (the Ambient Light Level, look.minlight, goes into the worldspawn instead: scaledLights)
    for(const za::StringView w : wordsOf(za::StringView{lightOptions}))
    {
        args.pushBack(za::String{w});
    }
}

[[nodiscard]] za::String settingsText(const Look& l)
{
    return za::String{va("Light Textures %.2fx (lamps %.2fx, glows %.2fx, lava %.2fx), Map Lights %.2fx, Sunlight %.2fx, "
                         "Bounce %.2f, Ambient Occlusion %.2f, Minimum Light %.0f, %s",
        l.strength, l.lamps, l.glows, l.liquids, l.maplights, l.sunlight, l.bounce, l.ao, l.minlight,
        l.quality >= 1.f ? "smooth" : "fast")};
}

// FNV-1a, 64 bits: the settings and the map's bytes, to tell a map relit with the same already.
struct Fnv
{
    za::U64 h{1469598103934665603ull};
    void add(const void* data, za::SizeT n)
    {
        const unsigned char* p = static_cast<const unsigned char*>(data);
        for(za::SizeT i = 0; i < n; i++)
        {
            h = (h ^ p[i]) * 1099511628211ull;
        }
    }
};

// A map file's identity: its size and its bytes' wyhash ("<size> <hash>"). A relit copy's .relight says the one it was
// made from ("source <game>/maps/<map>.bsp <size> <hash>"): the copy is used only while the game finds that same file
// (customCurrent), and a batch skips it only then (relitAlready).
[[nodiscard]] za::String sourceId(const unsigned char* data, za::SizeT size)
{
    namespace wy = ankerl::unordered_dense::detail::wyhash;
    return za::String{va("%llu %016llx", static_cast<unsigned long long>(size),
        static_cast<unsigned long long>(size ? wy::hash(data, size) : 0))};
}

// Whether a .relight's source line names the file `id` (sourceId): an old .relight (none) never does.
[[nodiscard]] bool noteHasSource(za::StringView note, const za::String& id)
{
    if(id.empty())
    {
        return false;
    }
    bool same = false;
    files::forLines(note, [&](za::StringView line) {
        while(!line.empty() && (line[line.size() - 1] == '\r' || line[line.size() - 1] == ' '))
        {
            line = line.substrByPosLen(0, line.size() - 1);
        }
        const za::SizeT n = id.size();
        same = same || (line.size() > 7 + n && !strncmp(line.data(), "source ", 7) && line[line.size() - n - 1] == ' ' &&
                           line.substrByPosLen(line.size() - n, n) == za::StringView{id});
    });
    return same;
}

// ericw-tools' light: vr_relight_tool, else the one Quake VR ships or downloaded (tools/ericw-tools/ in a game folder),
// ERICW_LIGHT, PATH, the author's (Menu Detail: Developer only). vr_relight_tool_dir (testing): that folder alone after
// vr_relight_tool. Empty: none.
[[nodiscard]] za::String findTool()
{
#ifdef _WIN32
    constexpr const char* exe = "light.exe";
#else
    constexpr const char* exe = "light";
#endif
    if(vr_relight_tool.string[0])
    {
        return Sys_FileType(vr_relight_tool.string) & FS_ENT_FILE ? za::String{vr_relight_tool.string} : za::String{};
    }
    if(tool::testDir())
    {
        const char* path = va("%s/%s", tool::installDir(), exe);
        return Sys_FileType(path) & FS_ENT_FILE ? za::String{path} : za::String{};
    }
    for(const searchpath_t* s = com_searchpaths; s; s = s->next)
    {
        if(s->pack)
        {
            continue;
        }
        const char* path = va("%s/tools/ericw-tools/%s", s->filename, exe);
        if(Sys_FileType(path) & FS_ENT_FILE)
        {
            return za::String{path};
        }
    }
    if(const char* env = getenv("ERICW_LIGHT"); env && (Sys_FileType(env) & FS_ENT_FILE))
    {
        return za::String{env};
    }
    za::String onPath = process::onPath(exe);
    if(!onPath.empty())
    {
        return onPath;
    }
#ifdef _WIN32
    if(vr_menu_level.value >= 2.f && (Sys_FileType(authorsLight) & FS_ENT_FILE))
    {
        return za::String{authorsLight};
    }
#endif
    return {};
}

// The map's file as the game finds it, and the folder of its game (id1, hipnotic, quakevr, a package's): false if
// there is no such map.
[[nodiscard]] bool mapSource(const char* map, za::Vector<unsigned char>& data, char* game, size_t size, bool& fromRelit)
{
    char name[MAX_QPATH];
    q_snprintf(name, sizeof(name), "maps/%s.bsp", map);
    if(!VR_MapGameFolder(name, game, size))
    {
        return false;
    }
    // relight_maps.py's copy (its water-vis patch kept), else the map itself.
    char relit[MAX_QPATH * 2];
    q_snprintf(relit, sizeof(relit), "relit/%s/%s", game, name);
    fromRelit = COM_FileExists(relit, nullptr);
    byte* bytes = COM_LoadMallocFile(fromRelit ? relit : name, nullptr);
    if(!bytes)
    {
        return false;
    }
    data.resize(static_cast<za::SizeT>(com_filesize));
    memcpy(data.data(), bytes, data.size());
    VR_HeapFree(bytes);
    return true;
}

// A batch's map's bytes: relight_maps.py's copy (relit/<game>/maps/, its water-vis patch kept), else its own file;
// `seeThrough` off (See-Through Liquids): its own file always (the copy's patch left out with the rest of it).
[[nodiscard]] bool batchSource(const maps::Source& s, za::Vector<unsigned char>& data, bool& fromRelit, bool seeThrough)
{
    const char* relit = va("relit/%s/maps/%s.bsp", s.game.cStr(), s.map.cStr());
    fromRelit = seeThrough && COM_FileExists(relit, nullptr);
    if(fromRelit)
    {
        if(byte* bytes = COM_LoadMallocFile(relit, nullptr))
        {
            data.resize(static_cast<za::SizeT>(com_filesize));
            memcpy(data.data(), bytes, data.size());
            VR_HeapFree(bytes);
            return true;
        }
        fromRelit = false;
    }
    return maps::read(s, data);
}

[[nodiscard]] za::String rulesText()
{
    byte* text = COM_LoadMallocFile("relight_textures.cfg", nullptr);
    if(!text)
    {
        return {};
    }
    za::String copy{reinterpret_cast<const char*>(text), static_cast<za::SizeT>(com_filesize)};
    VR_HeapFree(text);
    return copy;
}

[[nodiscard]] za::Vector<Rule> rulesNow()
{
    const za::String text = rulesText();
    if(text.empty())
    {
        Con_Printf("Relight: relight_textures.cfg not found: textures named *light* or *lamp* are lamps, other glowing "
                   "ones glow, liquids light nothing\n");
        return {};
    }
    return loadRules(text.cStr(), "relight_textures.cfg");
}

[[nodiscard]] const byte* paletteNow(za::Vector<unsigned char>& keep)
{
    byte* pal = COM_LoadMallocFile("gfx/palette.lmp", nullptr);
    if(!pal || com_filesize < 768)
    {
        if(pal)
        {
            VR_HeapFree(pal);
        }
        return nullptr;
    }
    keep.resize(768);
    memcpy(keep.data(), pal, 768);
    VR_HeapFree(pal);
    return keep.data();
}

// The map now in play ("e1m1"), or false.
[[nodiscard]] bool currentMap(char* out, size_t size)
{
    if(!cl.worldmodel || strncmp(cl.worldmodel->name, "maps/", 5))
    {
        return false;
    }
    COM_StripExtension(cl.worldmodel->name + 5, out, size);
    return out[0] != 0;
}

// The lights relight_maps.py would give light for this map, with these settings: (text, report).
[[nodiscard]] za::String lightsFor(const Bsp& b, const char* game, const char* map, const Look& look, Report& report)
{
    za::Vector<unsigned char> pal;
    const byte* palette = paletteNow(pal);
    if(!palette)
    {
        Con_Printf("Relight: gfx/palette.lmp not found: glowing textures will not light\n");
        return {};
    }
    const za::Vector<Rule> rules = rulesNow();
    VR_FileCacheEnable(1); // (each folder listed once while the glow images are looked for, as in a map's load)
    za::String out = glowLights(b, palette, rules, game, map, strengths(look), report);
    VR_FileCacheEnable(0);
    return out;
}

// Whether the game can be saved now (Host_Savegame_f's conditions).
[[nodiscard]] bool canSave()
{
    if(!sv.active || sv.nomonsters || cl.intermission || svs.maxclients != 1)
    {
        return false;
    }
    for(int i = 0; i < svs.maxclients; i++)
    {
        if(svs.clients[i].active && svs.clients[i].edict->v.health <= 0)
        {
            return false;
        }
    }
    return true;
}

// How many maps are lit side by side: vr_relight_parallel, or (0) two from 8 cores. light uses every core it is given
// (its bounced light most of all), but a small map's run is partly single-threaded, loading and writing: on 32 cores
// id's episode 1 took 7 s one at a time, 6 two at once, 5 three or four; with Bounced Light 34, 33, 33 and 42 s.
// Two gain most of it at two thirds of the memory of three (a big map's light takes hundreds of megabytes).
[[nodiscard]] int parallelCount(int maps)
{
    const unsigned cores = za::Thread::hardwareConcurrency();
    int n = static_cast<int>(vr_relight_parallel.value);
    if(n <= 0)
    {
        n = cores >= 8u ? 2 : 1;
    }
    return za::max(1, za::min(za::min(n, maxSlots), maps));
}

// "1:05", "1:02:03".
[[nodiscard]] const char* hms(double seconds)
{
    const int s = static_cast<int>(za::max(0.0, seconds) + 0.5);
    return s >= 3600 ? va("%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60) : va("%d:%02d", s / 60, s % 60);
}

// <relit_custom>/<game>/maps/<map>.<ext>
[[nodiscard]] za::String outPath(const maps::Source& s, const char* ext)
{
    if(batch.brushes)
    {
        return files::join(za::String{files::parentPath(za::StringView{batch.root})},
            za::StringView{va("maps/%s.%s", batch.bakeMap.cStr(), ext)});
    }
    return files::join(batch.root, za::StringView{va("%s/maps/%s.%s", s.game.cStr(), s.map.cStr(), ext)});
}

// Light's files for a map in the work folder: <root>/_work/<game>/<map, its subfolders' / as _>.
[[nodiscard]] za::String workStem(const maps::Source& s)
{
    za::String flat = s.map;
    for(char& c : flat)
    {
        c = c == '/' || c == '\\' ? '_' : c;
    }
    return files::join(batch.root, za::StringView{va("_work/%s/%s", s.game.cStr(), flat.cStr())});
}

// The work folder's copies of a map light was given and made (its logs kept: a failure's reason).
void removeWork(const za::String& stem)
{
    for(const char* ext : {".bsp", ".lit", ".lux"})
    {
        za::String path = stem;
        path += ext;
        files::remove(path.cStr());
    }
}

// The .relight of a map relit before says it was lit from exactly this (its hash line) and from the map's file as it
// is now (its source line: the hash covers relight_maps.py's copy instead when that is what light was given), and its
// files are there.
[[nodiscard]] bool relitAlready(const maps::Source& s, const za::String& hash, const za::String& source)
{
    za::String note;
    if(!files::readText(outPath(s, "relight").cStr(), note) || !files::isFile(outPath(s, "bsp").cStr()) ||
        !files::isFile(outPath(s, "lit").cStr()))
    {
        return false;
    }
    bool same = false;
    files::forLines(za::StringView{note}, [&](za::StringView line) {
        same = same || (line.size() > 5 && !strncmp(line.data(), "hash ", 5) &&
                           line.substrByPosLen(5, line.size() - 5) == za::StringView{hash});
    });
    return same && noteHasSource(za::StringView{note}, source);
}

enum class Started
{
    Running,
    Skipped,
    Failed,
};

// The batch's item `i` given to light in slot `slot` (or skipped: relit with these settings already).
[[nodiscard]] Started startItem(za::SizeT i, int slotIndex)
{
    Item& item = batch.items[i];
    const maps::Source& s = item.source;
    const char* map = s.map.cStr();
    const double t0 = Sys_DoubleTime();
    za::Vector<unsigned char> data;
    bool fromRelit = false;
    if(!batchSource(s, data, fromRelit, batch.look.seeThrough))
    {
        Con_Printf("Relight: %s/maps/%s.bsp can't be read\n", s.game.cStr(), map);
        return Started::Failed;
    }
    const Bsp b{data.data(), data.size()};
    if(!b.valid())
    {
        Con_Printf("Relight: %s/maps/%s.bsp is not a Quake map light can relight\n", s.game.cStr(), map);
        return Started::Failed;
    }
    // See-Through Liquids: VisPatch's lumps for the map, put in after light (when they are for this version of it).
    vis::Patch patch;
    bool patched = false;
    if(batch.look.seeThrough && vis::find(s.game.cStr(), map, patch))
    {
        patched = vis::fits(data.data(), data.size(), patch);
        if(!patched)
        {
            Con_Printf("Relight: %s's water-vis patch (%s) is for another version of the map: its liquids stay as they are\n",
                map, patch.file.cStr());
        }
    }
    Fnv fnv;
    fnv.add(batch.lookKey.data(), batch.lookKey.size());
    fnv.add(data.data(), data.size());
    if(patched) // (the patch is part of what the map was made from: toggling it, or new data files, relights)
    {
        fnv.add("vispatch", 8);
        fnv.add(patch.visibility.data(), patch.visibility.size());
        fnv.add(patch.leafs.data(), patch.leafs.size());
    }
    const za::String hash{va("%016llx", static_cast<unsigned long long>(fnv.h))};
    // The map's own file (not relight_maps.py's copy, when that was read): what the game loads when there is no copy.
    za::String source;
    if(fromRelit)
    {
        za::Vector<unsigned char> own;
        if(maps::read(s, own))
        {
            source = sourceId(own.data(), own.size());
        }
    }
    else
    {
        source = sourceId(data.data(), data.size());
    }
    if(!batch.force && relitAlready(s, hash, source))
    {
        return Started::Skipped;
    }

    Report report;
    const za::String lights = lightsFor(b, s.game.cStr(), map, batch.look, report);
    // Keep the entities the engine actually loaded: a CRC-qualified .ent stops matching after a bake.
    const za::String original = batch.brushes && sv.active && sv.worldmodel && sv.worldmodel->entities
                                    ? za::String{sv.worldmodel->entities} : entitiesText(b);
    // id's games' maps lose the re-release's worldspawn light settings (as relight_maps.py does); other maps keep
    // their mappers'.
    const char* game = s.game.cStr();
    const bool idGame = !q_strcasecmp(game, "id1") || !q_strcasecmp(game, "hipnotic") || !q_strcasecmp(game, "rogue");
    za::String entities = idLightValues(idGame ? withoutMapLightSettings(original) : original);
    entities = scaledLights(entities, za::max(0.0, static_cast<double>(batch.look.maplights)),
        za::max(0.0, static_cast<double>(batch.look.sunlight)), static_cast<double>(batch.look.minlight));
    entities += lights;

    Slot& slot = batch.slots[slotIndex];
    slot = Slot{};
    slot.stem = workStem(s);
    slot.original = original;
    slot.hash = hash;
    slot.source = source;
    slot.lights = report.lights;
    slot.patched = patched;
    if(patched)
    {
        slot.patch = ZA_MOVE(patch);
    }
    files::createDirectories(za::String{files::parentPath(za::StringView{slot.stem})}.cStr());
    const za::String bsp = slot.stem + ".bsp";
    const za::String log = slot.stem + ".txt";
    za::Vector<unsigned char> input;
    char inPlay[MAX_QPATH];
    if(batch.brushes && currentMap(inPlay, sizeof(inPlay)) && !q_strcasecmp(inPlay, map))
    {
        // The full bake: the map's brushes in it as submodels, with their entities for light (vr_brushbake.cpp).
        za::Vector<unsigned char> augmented;
        za::String brushEntities, error;
        brushbake::Options options;
        options.classname = "func_wall"; // permanent, solid brushes in the exported copy
        if(!brushbake::augment(data.data(), data.size(), augmented, brushEntities, slot.brushNotes, error, options))
        {
            Con_Printf("Relight: %s's brushes not baked in: %s\n", map, error.cStr());
            return Started::Failed; // no incomplete bake is published
        }
        else
        {
            slot.original += brushEntities; // retain their entities, not just the light-only notes
            const Bsp ab{augmented.data(), augmented.size()};
            za::String all = entities;
            all += brushEntities;
            input = withEntities(ab, all);
        }
    }
    else
    {
        input = withEntities(b, entities);
    }
    if(!files::writeBytes(bsp.cStr(), input.data(), input.size()))
    {
        Con_Printf("Relight: can't write %s\n", bsp.cStr());
        return Started::Failed;
    }
    slot.inputWritten = files::lastWriteTime(bsp.cStr());
    za::Vector<za::String> args;
    args.pushBack(za::String{"-threads"});
    args.pushBack(za::String{va("%d", batch.threads)});
    lookOptions(args, batch.look);
    args.pushBack(bsp);
    za::String error;
    slot.started = Sys_DoubleTime();
    if(!process::start(slotIndex, batch.tool, args, za::String{files::parentPath(za::StringView{slot.stem})}, log, error))
    {
        Con_Printf("Relight: %s: %s\n", batch.tool.cStr(), error.cStr());
        removeWork(slot.stem);
        return Started::Failed;
    }
    slot.item = static_cast<int>(i);
    q_strlcpy(slot.stage, "starting", sizeof(slot.stage));
    item.state = State::Running;
    item.fraction = 0.f;
    const char* line = va("relighting %s (%s%s%s; %d lights from its textures, made in %.0f ms) with %s", map, game,
        fromRelit ? ", relight_maps.py's copy" : "", slot.patched ? ", see-through liquids after" : "", report.lights,
        (slot.started - t0) * 1000.0, batch.tool.cStr());
    if(batch.single)
    {
        say(line);
    }
    else
    {
        Con_Printf("Relight: %s\n", line);
    }
    return Started::Running;
}

// How far a light run is (0..1) by its stage and the stage's percentage (light's stages, in their order, each given
// its usual share of the run's time: the direct light is most of it, the bounces next, the light grid last).
[[nodiscard]] float stageFraction(const char* stage, int percent, bool bounce)
{
    struct StageSpan
    {
        const char* stage;
        float from, to;
    };
    static constexpr StageSpan plain[] = {{"LoadEntities", 0.f, .02f}, {"MakeSurfaceLights", .02f, .03f},
        {"SetupDirt", .03f, .04f}, {"LightWorld", .04f, .04f}, {"CalculateVertexNormals", .04f, .06f},
        {"MakeFaceCache", .06f, .08f}, {"CreateLightmapSurfaces", .08f, .14f}, {"MakeRadiositySurfaceLights", .14f, .16f},
        {"Direct Lighting", .16f, .78f}, {"Post-Processing", .78f, .82f}, {"SaveLightmapSurfaces", .82f, .86f},
        {"LightGrid", .86f, .99f}};
    static constexpr StageSpan bounced[] = {{"Direct Lighting", .05f, .15f}, {"Indirect Lighting", .15f, .86f}};
    const float p = percent >= 0 ? za::min(percent, 100) / 100.f : 0.f;
    if(bounce)
    {
        for(const StageSpan& s : bounced)
        {
            if(!strncmp(stage, s.stage, strlen(s.stage)))
            {
                return s.from + (s.to - s.from) * p;
            }
        }
    }
    for(const StageSpan& s : plain)
    {
        if(!strncmp(stage, s.stage, strlen(s.stage)))
        {
            return s.from + (s.to - s.from) * p;
        }
    }
    return -1.f; // (a stage not listed: where it was)
}

// The light log's new lines: its stage ("---- LightWorld ----") and percentage ("[ 45%]").
void readProgress(Slot& slot)
{
    const za::String log = slot.stem + ".txt";
    FILE* f = Sys_fopen(log.cStr(), "rb");
    if(!f)
    {
        return;
    }
    fseek(f, static_cast<long>(slot.logRead), SEEK_SET);
    char buf[4096];
    size_t n;
    while((n = fread(buf, 1, sizeof(buf), f)) > 0)
    {
        slot.logRead += n;
        for(size_t i = 0; i < n; i++)
        {
            slot.logTail.pushBack(buf[i]);
        }
    }
    fclose(f);
    if(slot.logTail.size() > 8192)
    {
        za::Vector<char> keep;
        keep.resize(4096);
        memcpy(keep.data(), slot.logTail.data() + slot.logTail.size() - 4096, 4096);
        slot.logTail = ZA_MOVE(keep);
    }
    // The last stage and percentage in what is kept.
    const za::StringView text{slot.logTail.data(), slot.logTail.size()};
    const za::SizeT stage = text.rfind(za::StringView{"---- "});
    if(stage != za::StringView::nPos)
    {
        const za::SizeT end = text.find(za::StringView{" ----"}, stage + 5);
        if(end != za::StringView::nPos && end - stage - 5 < sizeof(slot.stage))
        {
            const za::StringView s = text.substrByPosLen(stage + 5, end - stage - 5);
            if(strncmp(slot.stage, s.data(), s.size()) || slot.stage[s.size()])
            {
                memcpy(slot.stage, s.data(), s.size());
                slot.stage[s.size()] = 0;
                slot.percent = -1;
                if(slot.item >= 0) // (developer 1: when each stage began, to weigh them: stageFraction)
                {
                    Con_DPrintf("Relight: %s: %s from %.2f s\n", batch.items[static_cast<za::SizeT>(slot.item)].source.map.cStr(),
                        slot.stage, Sys_DoubleTime() - slot.started);
                }
            }
        }
    }
    const za::SizeT pct = text.rfind('%');
    if(pct != za::StringView::nPos && pct >= 4 && text[pct - 4] == '[' && (stage == za::StringView::nPos || pct > stage))
    {
        slot.percent = atoi(va("%.*s", 3, text.data() + pct - 3));
    }
    if(slot.item >= 0)
    {
        Item& item = batch.items[static_cast<za::SizeT>(slot.item)];
        const float f = stageFraction(slot.stage, slot.percent, batch.look.bounce > 0.f);
        item.fraction = za::max(item.fraction, f);
    }
}

// The log's last lines (a failure's reason).
[[nodiscard]] za::String logEnd(const Slot& slot, int lines)
{
    za::SizeT i = slot.logTail.size();
    int seen = 0;
    while(i > 0 && seen <= lines)
    {
        i--;
        seen += slot.logTail[i] == '\n' ? 1 : 0;
    }
    za::String out{slot.logTail.data() + i, slot.logTail.size() - i};
    for(char& c : out)
    {
        c = c == '\r' ? '\n' : c;
    }
    return out;
}

[[nodiscard]] bool copyFile(const za::String& from, const za::String& to)
{
    za::Vector<unsigned char> data;
    return files::readBytes(from.cStr(), data) && files::writeBytes(to.cStr(), data.data(), data.size());
}

// The map in play reloaded where you are (a save and a load once its file is written: poll), or restarted where the
// game can't be saved. `seconds`, `map`: for the message.
void reloadNow(const char* map, double seconds)
{
    if(canSave())
    {
        // Saved now (its file written on the save thread), loaded once written (poll): ahead of whatever waits in the
        // command buffer (a script's waits).
        sv.lastsave[0] = 0;
        Cmd_ExecuteString("save \"autosave/relight\" 0", src_command);
        if(!strcmp(sv.lastsave, "autosave/relight.sav"))
        {
            saving = true;
            say(va("%s relit in %.1f s: reloading where you are", map, seconds));
            return;
        }
    }
    Cbuf_InsertText("restart\n");
    say(va("%s relit in %.1f s: the map restarted (the game can't be saved here)", map, seconds));
}

// light ended well: the map's own entities back, the result where the game loads it. Each file written beside its
// place first (.tmp) and then renamed over it, the .bsp after its .lit and .lux and the .relight last: a quit or a
// crash halfway leaves the map as it was or as it is now, never half of each.
[[nodiscard]] bool finishSlot(Slot& slot, double& seconds)
{
    Item& item = batch.items[static_cast<za::SizeT>(slot.item)];
    const maps::Source& s = item.source;
    const char* map = s.map.cStr();
    const za::String bspIn = slot.stem + ".bsp";
    const za::String lit = slot.stem + ".lit";
    const za::String lux = slot.stem + ".lux";
    // (light's results, not an earlier run's: written since its input was)
    const za::I64 since = slot.inputWritten;
    za::Vector<unsigned char> data;
    if(!files::readBytes(bspIn.cStr(), data) || !files::isFile(lit.cStr()) || files::lastWriteTime(lit.cStr()) < since)
    {
        Con_Printf("Relight: light made no .lit for %s:\n%s\n", map, logEnd(slot, 6).cStr());
        return false;
    }
    const Bsp b{data.data(), data.size()};
    if(!b.valid())
    {
        Con_Printf("Relight: light's %s.bsp can't be read\n", map);
        return false;
    }
    // See-Through Liquids: VisPatch's visibility and leaves in, as relight_maps.py's water_vise does after light (light
    // keeps the leaves: checked again on its output all the same).
    const bool patch = slot.patched && vis::fits(b.data, b.size, slot.patch);
    if(slot.patched && !patch)
    {
        Con_Printf("Relight: light's %s.bsp has other leaves than the water-vis patch's: its liquids stay as they are\n", map);
    }
    za::String kept = slot.original;
    kept += slot.brushNotes;
    const za::Vector<unsigned char> out = withEntities(b, kept, patch ? &slot.patch : nullptr);
    const za::String outBsp = outPath(s, "bsp");
    files::createDirectories(za::String{files::parentPath(za::StringView{outBsp})}.cStr());
    const bool hasLux = files::isFile(lux.cStr()) && files::lastWriteTime(lux.cStr()) >= since;
    seconds = Sys_DoubleTime() - slot.started;
    za::String note{va("relit in the game (vr_relight) in %.1f s, %d lights from its textures\n", seconds, slot.lights)};
    note += batch.lookText;
    note += "\nliquids: ";
    note += vis::describe(vis::liquids(out.data(), out.size()));
    if(patch)
    {
        note += " (VisPatch's water-vis, ";
        note += slot.patch.file;
        note += ")";
    }
    note += "\nhash ";
    note += slot.hash;
    if(!slot.source.empty())
    {
        note += va("\nsource %s/maps/%s.bsp ", s.game.cStr(), map);
        note += slot.source;
    }
    note += "\n";
    const za::String tmpBsp = outBsp + ".tmp", tmpLit = outPath(s, "lit.tmp"), tmpLux = outPath(s, "lux.tmp"),
                     tmpNote = outPath(s, "relight.tmp");
    const bool written = files::writeBytes(tmpBsp.cStr(), out.data(), out.size()) && copyFile(lit, tmpLit) &&
                         (!hasLux || copyFile(lux, tmpLux)) && files::writeText(tmpNote.cStr(), za::StringView{note});
    // (an earlier relight's .relight goes first: a crash before the end leaves no hash to skip a half-made map by)
    files::remove(outPath(s, "relight").cStr());
    const bool placed = written && files::rename(tmpLit.cStr(), outPath(s, "lit").cStr()) &&
                        (hasLux ? files::rename(tmpLux.cStr(), outPath(s, "lux").cStr())
                                : (files::remove(outPath(s, "lux").cStr()), true)) &&
                        files::rename(tmpBsp.cStr(), outBsp.cStr()) &&
                        files::rename(tmpNote.cStr(), outPath(s, "relight").cStr());
    if(!placed)
    {
        for(const za::String* t : {&tmpBsp, &tmpLit, &tmpLux, &tmpNote})
        {
            files::remove(t->cStr());
        }
        Con_Printf("Relight: can't write the relit %s into %s\n", map, za::String{files::parentPath(za::StringView{outBsp})}.cStr());
        return false;
    }
    removeWork(slot.stem);
    VR_FileCacheForget();
    return true;
}

// A map's light over this many seconds is flagged (the console): its map is one a batch is slow on.
constexpr double slowSeconds = 60.0;

// The batch's maps relit, each with its light's time, the slowest first (the console), and those over slowSeconds.
void batchTimes()
{
    za::Vector<const Item*> done;
    for(const Item& item : batch.items)
    {
        if(item.state == State::Done)
        {
            done.pushBack(&item);
        }
    }
    if(done.empty())
    {
        return;
    }
    za::stableSort(done.begin(), done.end(), [](const Item* a, const Item* b) { return a->seconds > b->seconds; });
    za::String line{"Relight: each map's light, the slowest first:"};
    za::String slow;
    for(const Item* item : done)
    {
        line += va(" %s %.1f s,", item->source.map.cStr(), item->seconds);
        if(item->seconds > slowSeconds)
        {
            slow += va("%s%s (%.0f s)", slow.empty() ? "" : ", ", item->source.map.cStr(), item->seconds);
        }
    }
    line[line.size() - 1] = '\n';
    Con_Printf("%s", line.cStr());
    if(!slow.empty())
    {
        Con_Printf("Relight: over %.0f s: %s\n", slowSeconds, slow.cStr());
    }
}

// The batch's end: what it did, and the map in play reloaded if it waited for the end.
void endBatch()
{
    batch.active = false;
    batch.ended = Sys_DoubleTime();
    runWhenDone();
    const double seconds = batch.ended - batch.started;
    if(batch.single)
    {
        return; // (its map's own message said it)
    }
    batchTimes();
    za::String line{va("%d map%s relit", batch.relit, batch.relit == 1 ? "" : "s")};
    if(batch.skipped)
    {
        line += va(", %d skipped (relit with these settings already)", batch.skipped);
    }
    if(batch.alreadyLit)
    {
        line += va(", %d of Quake VR's own skipped (already lit)", batch.alreadyLit);
    }
    if(batch.failed)
    {
        line += va(", %d failed (%s)", batch.failed, batch.failedMaps);
    }
    line += va(" in %s", hms(seconds));
    if(batch.reloadPending)
    {
        batch.reloadPending = false;
        char now[MAX_QPATH];
        if(currentMap(now, sizeof(now)) && !strcmp(now, batch.reloadMap))
        {
            say(line.cStr());
            reloadNow(batch.reloadMap, seconds);
            return;
        }
    }
    say(line.cStr());
}

void itemEnded(Item& item, State state)
{
    item.state = state;
    item.fraction = 1.f;
    switch(state)
    {
        case State::Done: batch.relit++; break;
        case State::Skipped: batch.skipped++; break;
        case State::Failed:
            batch.failed++;
            if(strlen(batch.failedMaps) + item.source.map.size() + 3 < sizeof(batch.failedMaps))
            {
                q_strlcat(batch.failedMaps, batch.failedMaps[0] ? ", " : "", sizeof(batch.failedMaps));
                q_strlcat(batch.failedMaps, item.source.map.cStr(), sizeof(batch.failedMaps));
            }
            break;
        case State::Cancelled: batch.cancelled++; break;
        default: break;
    }
}

// A slot's light ended: its map's result taken (or its failure said), the map in play reloaded when it is that one.
void slotEnded(int slotIndex)
{
    Slot& slot = batch.slots[slotIndex];
    Item& item = batch.items[static_cast<za::SizeT>(slot.item)];
    const char* map = item.source.map.cStr();
    readProgress(slot);
    const int code = process::lastExitCode(slotIndex);
    double seconds = 0.0;
    if(code != 0)
    {
        const za::String why{va("light failed on %s (exit %d):\n%s", map, code, logEnd(slot, 6).cStr())};
        if(batch.single)
        {
            say(why.cStr());
        }
        else
        {
            Con_Printf("Relight: %s\n", why.cStr());
        }
        removeWork(slot.stem);
        itemEnded(item, State::Failed);
    }
    else if(!finishSlot(slot, seconds))
    {
        if(batch.single)
        {
            say(va("relighting %s failed (the console says why)", map));
        }
        itemEnded(item, State::Failed);
    }
    else
    {
        item.seconds = seconds;
        itemEnded(item, State::Done);
        if(seconds > slowSeconds)
        {
            Con_Printf("Relight: %s took %.0f s, over %.0f s: a slow map to relight\n", map, seconds, slowSeconds);
        }
        char now[MAX_QPATH];
        const bool inPlay = batch.reloadMap[0] && !strcmp(batch.reloadMap, map) &&
                            !q_strcasecmp(batch.reloadGame, item.source.game.cStr()) && currentMap(now, sizeof(now)) &&
                            !strcmp(now, map);
        const int done = batch.relit + batch.skipped + batch.failed;
        if(!batch.single)
        {
            Con_Printf("Relight: %s relit in %.1f s (%d of %d)\n", map, seconds, done, static_cast<int>(batch.items.size()));
        }
        if(batch.brushes)
        {
            say(va("baked copy saved: maps/%s.bsp; load with map %s (source map unchanged)",
                batch.bakeMap.cStr(), batch.bakeMap.cStr()));
        }
        else if(inPlay && !batch.reloadAtEnd)
        {
            reloadNow(map, seconds);
        }
        else if(inPlay)
        {
            batch.reloadPending = true;
        }
        else if(batch.single)
        {
            say(va("%s relit in %.1f s: loaded from its next start (%s)", map, seconds,
                za::String{files::parentPath(za::StringView{outPath(item.source, "bsp")})}.cStr()));
        }
    }
    slot = Slot{};
}

// The free slots given the next maps: at most one started a poll (its texture lights are made on the main thread, a
// few tens of milliseconds), the ones relit already passed over within a few milliseconds.
void fillSlots()
{
    const double until = Sys_DoubleTime() + 0.008;
    for(int s = 0; s < batch.parallel && batch.next < batch.items.size(); s++)
    {
        if(batch.slots[s].item >= 0)
        {
            continue;
        }
        while(batch.next < batch.items.size())
        {
            const za::SizeT i = batch.next++;
            const Started r = startItem(i, s);
            if(r == Started::Running)
            {
                return;
            }
            itemEnded(batch.items[i], r == Started::Skipped ? State::Skipped : State::Failed);
            if(r == Started::Skipped)
            {
                Con_DPrintf("Relight: %s skipped: relit with these settings already\n", batch.items[i].source.map.cStr());
                if(batch.single)
                {
                    say(va("%s is relit with these settings already", batch.items[i].source.map.cStr()));
                }
            }
            else if(batch.single)
            {
                say(va("relighting %s failed (the console says why)", batch.items[i].source.map.cStr()));
            }
            if(Sys_DoubleTime() > until)
            {
                return;
            }
        }
    }
}

[[nodiscard]] bool anyRunning()
{
    for(const Slot& slot : batch.slots)
    {
        if(slot.item >= 0)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool busy()
{
    return batch.active || saving;
}

// A batch of `sources` started with the page's settings (single: vr_relight's one map). `force`: maps relit with the
// same settings already relit again (vr_relight's always are).
void startBatch(za::Vector<maps::Source> sources, bool single, bool force, int alreadyLit = 0, bool brushes = false, const char* bakeMap = nullptr)
{
    if(busy())
    {
        say(batch.active ? "already relighting: cancel it first" : "reloading the map: wait a moment");
        return;
    }
    const za::String tool = findTool();
    if(tool.empty())
    {
        say(tool::running() ? "ericw-tools is being downloaded: relight when it is done"
                            : "ericw-tools' light not found: Download ericw-tools on the page (vr_relight_get_tool), or set "
                              "vr_relight_tool to a light.exe (docs/RELIGHTING.md)");
        return;
    }
    batch = Batch{};
    batch.single = single;
    batch.force = single || force;
    batch.alreadyLit = alreadyLit;
    batch.brushes = brushes;
    batch.bakeMap = bakeMap ? bakeMap : "";
    batch.tool = tool;
    batch.root = customRoot();
    batch.look = Look::now();
    batch.lookText = settingsText(batch.look);
    za::Vector<za::String> args;
    lookOptions(args, batch.look);
    batch.lookKey = batch.lookText;
    for(const za::String& a : args)
    {
        batch.lookKey += " ";
        batch.lookKey += a;
    }
    batch.lookKey += "\n";
    batch.lookKey += rulesText();

    // The map in play first, when it is one of them: it is the one whose light you see.
    char current[MAX_QPATH];
    maps::Source here;
    const bool inPlay = currentMap(current, sizeof(current)) && maps::locate(current, here);
    for(za::SizeT i = 0; inPlay && i < sources.size(); i++)
    {
        if(sources[i].map == here.map && !q_strcasecmp(sources[i].game.cStr(), here.game.cStr()))
        {
            if(i > 0)
            {
                maps::Source first = sources[i];
                for(za::SizeT j = i; j > 0; j--)
                {
                    sources[j] = sources[j - 1];
                }
                sources[0] = first;
            }
            if(vr_relight_reload.value != 0.f)
            {
                q_strlcpy(batch.reloadMap, here.map.cStr(), sizeof(batch.reloadMap));
                q_strlcpy(batch.reloadGame, here.game.cStr(), sizeof(batch.reloadGame));
            }
            break;
        }
    }
    batch.reloadAtEnd = !single && vr_relight_batch_reload.value != 0.f;
    for(maps::Source& s : sources)
    {
        Item item;
        item.weight = static_cast<double>(za::max(s.length, za::I64{200000})); // (light's start and end, a small map's most)
        item.source = ZA_MOVE(s);
        batch.items.pushBack(ZA_MOVE(item));
    }
    batch.parallel = parallelCount(static_cast<int>(batch.items.size()));
    const unsigned cores = za::Thread::hardwareConcurrency();
    // All the cores but one (the game keeps one for itself; light also runs below normal priority), shared out.
    batch.threads = za::max(1, (static_cast<int>(cores > 1u ? cores - 1u : 1u)) / batch.parallel);
    batch.started = Sys_DoubleTime();
    batch.active = true;
    if(!single)
    {
        say(va("relighting %d map%s, %d at once (%d threads each)%s%s", static_cast<int>(batch.items.size()),
            batch.items.size() == 1 ? "" : "s", batch.parallel, batch.threads,
            batch.force ? "" : "; those relit with these settings already skipped",
            alreadyLit ? va("; %d of Quake VR's own skipped, already lit", alreadyLit) : ""));
    }
    fillSlots();
    if(batch.active && batch.next >= batch.items.size() && !anyRunning())
    {
        endBatch();
    }
}

// Every light stopped, its half-made files removed; the maps done are kept.
void cancelBatch(const char* why)
{
    process::stopAll(false);
    int stopped = 0;
    za::String names;
    for(Slot& slot : batch.slots)
    {
        if(slot.item < 0)
        {
            continue;
        }
        Item& item = batch.items[static_cast<za::SizeT>(slot.item)];
        removeWork(slot.stem);
        itemEnded(item, State::Cancelled);
        names += names.empty() ? "" : ", ";
        names += item.source.map;
        stopped++;
        slot = Slot{};
    }
    for(za::SizeT i = batch.next; i < batch.items.size(); i++)
    {
        itemEnded(batch.items[i], State::Cancelled);
    }
    batch.next = batch.items.size();
    batch.active = false;
    batch.ended = Sys_DoubleTime();
    batch.reloadPending = false;
    runWhenDone();
    if(why)
    {
        if(batch.single)
        {
            say(va("relighting %s cancelled", batch.items.empty() ? "" : batch.items[0].source.map.cStr()));
        }
        else
        {
            say(va("%s: %d map%s relit kept, %s stopped (no output), %d not started", why, batch.relit,
                batch.relit == 1 ? "" : "s", stopped ? names.cStr() : "none", batch.cancelled - stopped));
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Commands.

void relightWithBrushesCommand()
{
    char current[MAX_QPATH];
    maps::Source s;
    if(!currentMap(current, sizeof(current)) || !maps::locate(current, s))
    {
        say("no map loaded: start a map first");
        return;
    }
    char name[MAX_QPATH];
    if(Cmd_Argc() > 1)
    {
        q_strlcpy(name, Cmd_Argv(1), sizeof(name));
    }
    else
    {
        char stem[MAX_QPATH];
        q_strlcpy(stem, current, sizeof(stem));
        for(char* p = stem; *p; ++p) { if(*p == '/' || *p == '\\') { *p = '_'; } }
        int suffix = 1;
        do
        {
            q_snprintf(name, sizeof(name), "%.40s_baked%d", stem, suffix++);
        } while(COM_FileExists(va("maps/%s.bsp", name), nullptr));
    }
    for(const char* p = name; *p; ++p)
    {
        if(!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-'))
        {
            say("bake name must contain only letters, digits, underscore or hyphen");
            return;
        }
    }
    if(!name[0] || !q_strcasecmp(name, current) || COM_FileExists(va("maps/%s.bsp", name), nullptr))
    {
        say("bake needs a new map name; existing maps are never overwritten");
        return;
    }
    za::Vector<maps::Source> one;
    one.pushBack(ZA_MOVE(s));
    startBatch(ZA_MOVE(one), true, true, 0, true, name);
}

void relightCommand()
{
    char current[MAX_QPATH];
    const char* requested = Cmd_Argc() > 1 ? Cmd_Argv(1) : nullptr;
    if(!requested && !currentMap(current, sizeof(current)))
    {
        say("no map loaded: start a map first, or name one (vr_relight <map>)");
        return;
    }
    char map[MAX_QPATH];
    COM_StripExtension(requested ? requested : current, map, sizeof(map));
    maps::Source s;
    if(!maps::locate(map, s))
    {
        say(va("maps/%s.bsp not found", map));
        return;
    }
    za::Vector<maps::Source> one;
    one.pushBack(ZA_MOVE(s));
    startBatch(ZA_MOVE(one), true, true);
}

// The page's game choices (vr_relight_batch_game).
constexpr const char* batchGames[] = {"", "id1", "hipnotic", "rogue", "dopa", "mg1", "mg3"};

// vr_relight_batch [map|episode [eN]|game [folder]|library|everything | <map> <map>...] [-force] [-list] [-own]: the
// page's choice (vr_relight_batch_set ...) without a set named. An episode, a game, the Library or everything passes
// over Quake VR's own maps (maps::Source::prelit: they ship lit as they are meant to be, and vrstart's light takes
// half an hour) unless -own; the map in play, maps named and vr_relight (Relight This Map) relight them.
void batchCommand()
{
    bool list = false, force = false, own = false;
    za::Vector<const char*> words;
    for(int i = 1; i < Cmd_Argc(); i++)
    {
        const char* a = Cmd_Argv(i);
        if(!q_strcasecmp(a, "-list"))
        {
            list = true;
        }
        else if(!q_strcasecmp(a, "-force"))
        {
            force = true;
        }
        else if(!q_strcasecmp(a, "-own"))
        {
            own = true;
        }
        else
        {
            words.pushBack(a);
        }
    }
    struct Named
    {
        const char* word;
        maps::Set set;
    };
    static constexpr Named sets[] = {{"map", maps::Set::Map}, {"episode", maps::Set::Episode}, {"game", maps::Set::Game},
        {"library", maps::Set::Library}, {"everything", maps::Set::Everything}};
    char current[MAX_QPATH];
    if(!currentMap(current, sizeof(current)))
    {
        current[0] = 0;
    }
    za::Vector<maps::Source> sources;
    za::String why;
    const Named* named = nullptr;
    for(const Named& n : sets)
    {
        named = !words.empty() && !q_strcasecmp(words[0], n.word) ? &n : named;
    }
    bool found = false;
    if(!words.empty() && !named)
    {
        // Maps by name, as the game finds them.
        for(const char* w : words)
        {
            char map[MAX_QPATH];
            COM_StripExtension(w, map, sizeof(map));
            maps::Source s;
            if(maps::locate(map, s))
            {
                sources.pushBack(ZA_MOVE(s));
            }
            else
            {
                Con_Printf("Relight: maps/%s.bsp not found\n", map);
            }
        }
        found = !sources.empty();
        why = "none of those maps found";
    }
    else
    {
        const int pick = static_cast<int>(vr_relight_batch_set.value);
        const maps::Set set = named ? named->set : static_cast<maps::Set>(CLAMP(0, pick, 4));
        const char* arg = words.size() > 1 ? words[1] : "";
        char episode[16];
        if(!named && set == maps::Set::Episode && vr_relight_batch_episode.value >= 1.f)
        {
            q_snprintf(episode, sizeof(episode), "e%d", static_cast<int>(vr_relight_batch_episode.value));
            arg = episode;
        }
        if(!named && set == maps::Set::Game)
        {
            arg = batchGames[CLAMP(0, static_cast<int>(vr_relight_batch_game.value), 6)];
        }
        found = maps::collect(set, arg, current, sources, why);
    }
    if(!found)
    {
        say(why.cStr());
        return;
    }
    int alreadyLit = 0;
    if(!own)
    {
        za::Vector<maps::Source> kept;
        for(maps::Source& s : sources)
        {
            if(s.prelit)
            {
                Con_Printf("Relight: %s skipped: already lit (Quake VR's own map; vr_relight relights it)\n", s.map.cStr());
                alreadyLit++;
                continue;
            }
            kept.pushBack(ZA_MOVE(s));
        }
        sources = ZA_MOVE(kept);
        if(sources.empty())
        {
            say(va("%d of Quake VR's own maps, already lit: nothing to relight (-own relights them)", alreadyLit));
            return;
        }
    }
    if(list)
    {
        za::I64 bytes = 0;
        for(const maps::Source& s : sources)
        {
            Con_Printf("  %s/%s (%s%s, %lld bytes)\n", s.game.cStr(), s.map.cStr(), s.inPak ? "in " : "",
                za::String{files::fileName(za::StringView{s.file})}.cStr(), static_cast<long long>(s.length));
            bytes += s.length;
        }
        Con_Printf("Relight: %d maps, %.1f MB\n", static_cast<int>(sources.size()), static_cast<double>(bytes) / 1048576.0);
        return;
    }
    startBatch(ZA_MOVE(sources), false, force || vr_relight_batch_force.value != 0.f, alreadyLit);
}

void cancelCommand()
{
    if(tool::cancel())
    {
        say("cancelling the ericw-tools download...");
        return;
    }
    if(!batch.active)
    {
        say("nothing is being relit");
        return;
    }
    cancelBatch("cancelled");
}

// The in-game relight of the map in play (or the one named) removed: it plays with relight_maps.py's light, or its own.
void revertCommand()
{
    char map[MAX_QPATH];
    if(Cmd_Argc() > 1)
    {
        COM_StripExtension(Cmd_Argv(1), map, sizeof(map));
    }
    else if(!currentMap(map, sizeof(map)))
    {
        say("no map loaded");
        return;
    }
    char game[MAX_QPATH];
    if(!VR_MapGameFolder(va("maps/%s.bsp", map), game, sizeof(game)))
    {
        say(va("maps/%s.bsp not found", map));
        return;
    }
    const za::String dir = files::join(customRoot(), za::StringView{va("%s/maps", game)});
    int removed = 0;
    for(const char* ext : {"bsp", "lit", "lux", "relight"})
    {
        removed += files::remove(files::join(dir, za::StringView{va("%s.%s", map, ext)}).cStr()) ? 1 : 0;
    }
    VR_FileCacheForget();
    say(removed ? va("%s's in-game relight removed: its other light from its next start", map) :
                  va("%s has no in-game relight", map));
}

// vr_relight_whendone <commands>: the commands run when the batch (or vr_relight's map) ends, at once when none
// runs: a script's wait for a relight ("vr_relight_batch everything; vr_relight_whendone quit").
void whenDoneCommand()
{
    const char* args = Cmd_Argc() == 2 ? Cmd_Argv(1) : Cmd_Args(); // ("a;b" quoted: its quotes off)
    whenDone = za::String{args ? args : ""};
    if(!batch.active)
    {
        runWhenDone();
    }
}

void statusCommand()
{
    Con_Printf("%s\n", statusLine());
    if(batch.active || batch.ended > 0.0)
    {
        Con_Printf("%s\n%s (%.3f)\n", detailLine(), progressText(), progress());
        for(int s = 0; s < maxSlots; s++)
        {
            if(batch.slots[s].item >= 0)
            {
                Con_Printf("  slot %d: %s, light pid %d, %s %d%%\n", s,
                    batch.items[static_cast<za::SizeT>(batch.slots[s].item)].source.map.cStr(), process::processId(s),
                    batch.slots[s].stage, batch.slots[s].percent);
            }
        }
    }
    Con_Printf("%s\n%s\n", mapLine(), toolLine());
}

void defaultsCommand()
{
    for(cvar_t* c : {&vr_relight_strength, &vr_relight_lamps, &vr_relight_glows, &vr_relight_liquids,
            &vr_relight_maplights, &vr_relight_sunlight, &vr_relight_bounce, &vr_relight_ao, &vr_relight_minlight,
            &vr_relight_quality, &vr_relight_seethrough})
    {
        Cvar_SetQuick(c, c->default_string);
    }
    say("settings back to their defaults (relight_maps.py's look)");
}

// vr_relight_lights [file]: the lights the map in play's textures get with the page's settings, into the file
// (relight_lights.txt in the game folder), and a line each texture: to compare with relight_maps.py --list-glows.
void lightsCommand()
{
    char map[MAX_QPATH];
    if(!currentMap(map, sizeof(map)))
    {
        say("no map loaded");
        return;
    }
    za::Vector<unsigned char> data;
    char game[MAX_QPATH];
    bool fromRelit = false;
    if(!mapSource(map, data, game, sizeof(game), fromRelit))
    {
        say(va("maps/%s.bsp not found", map));
        return;
    }
    const Bsp b{data.data(), data.size()};
    if(!b.valid())
    {
        say("not a Quake map");
        return;
    }
    Report report;
    const double t0 = Sys_DoubleTime();
    const za::String lights = lightsFor(b, game, map, Look::now(), report);
    const double ms = (Sys_DoubleTime() - t0) * 1000.0;
    const za::String file = Cmd_Argc() > 1 ? za::String{Cmd_Argv(1)} : files::join(za::StringView{com_gamedir}, za::StringView{"relight_lights.txt"});
    const bool written = files::writeBytes(file.cStr(), lights.data(), lights.size());
    Con_Printf("%s/%s:\n", game, map);
    files::forLines(report.lines, [](za::StringView line) { Con_Printf("%.*s\n", static_cast<int>(line.size()), line.data()); });
    Con_Printf("Relight: %d lights from %d glowing textures (%.0f ms: faces %.0f, glow images %.0f, lights %.0f)%s%s\n",
        report.lights, report.textures, ms, report.facesMs, report.imagesMs, report.spotsMs,
        written ? ", written to " : "", written ? file.cStr() : "");
}

// vr_relight_vispatch: where the VisPatch data (See-Through Liquids) is looked for, each game's file found, the map in
// play's liquids (as loaded) and whether there is a patch for it. vr_relight_vispatch check <file.bsp>...: which liquids
// .bsp files are vised for (vis_maps.py --check). vr_relight_vispatch <game> <map> <in.bsp> <out.bsp> (testing): the
// patch put into a file as the relight puts it (repacked, its entities and BSPX lumps kept): to compare with vis_maps.py.
void vispatchCommand()
{
    if(Cmd_Argc() >= 3 && !q_strcasecmp(Cmd_Argv(1), "check"))
    {
        for(int i = 2; i < Cmd_Argc(); i++)
        {
            za::Vector<unsigned char> data;
            Con_Printf("%s: %s\n", Cmd_Argv(i), !files::readBytes(Cmd_Argv(i), data) ? "can't be read" :
                vis::describe(vis::liquids(data.data(), data.size())).cStr());
        }
        return;
    }
    if(Cmd_Argc() == 5)
    {
        const char *game = Cmd_Argv(1), *map = Cmd_Argv(2), *in = Cmd_Argv(3), *outFile = Cmd_Argv(4);
        za::Vector<unsigned char> data;
        vis::Patch patch;
        if(!files::readBytes(in, data))
        {
            Con_Printf("vr_relight_vispatch: %s can't be read\n", in);
            return;
        }
        const Bsp b{data.data(), data.size()};
        if(!b.valid() || !vis::find(game, map, patch) || !vis::fits(data.data(), data.size(), patch))
        {
            Con_Printf("vr_relight_vispatch: %s\n", !b.valid() ? "not a Quake map" :
                patch.file.empty() ? va("no patch for %s/%s", game, map) : "the patch is for another version of the map");
            return;
        }
        const za::Vector<unsigned char> out = withEntities(b, za::StringView{entitiesText(b)}, &patch);
        Con_Printf("vr_relight_vispatch: %s (%s)\n", files::writeBytes(outFile, out.data(), out.size()) ? outFile : "not written",
            vis::describe(vis::liquids(out.data(), out.size())).cStr());
        return;
    }
    if(Cmd_Argc() != 1)
    {
        Con_Printf("vr_relight_vispatch [check <file.bsp>... | <game> <map> <in.bsp> <out.bsp>]\n");
        return;
    }
    Con_Printf("See-Through Liquids (vr_relight_seethrough %s): VisPatch's data looked for in %s\n",
        vr_relight_seethrough.string, vis::placesText().cStr());
    for(const char* g : {"id1", "hipnotic", "rogue"})
    {
        const za::String file = vis::dataFile(g);
        Con_Printf("  %s: %s\n", g, file.empty() ? "no data file" : file.cStr());
    }
    char map[MAX_QPATH], game[MAX_QPATH];
    if(currentMap(map, sizeof(map)) && VR_MapGameFolder(va("maps/%s.bsp", map), game, sizeof(game)))
    {
        const int t = cl.worldmodel->contentstransparent;
        vis::Patch patch;
        Con_Printf("  %s (%s), as loaded: see-through%s%s%s%s%s; %s\n", map, game, t & SURF_DRAWWATER ? " water" : "",
            t & SURF_DRAWTELE ? " tele" : "", t & SURF_DRAWSLIME ? " slime" : "", t & SURF_DRAWLAVA ? " lava" : "",
            t ? "" : " none", vis::find(game, map, patch) ? va("a patch in %s", patch.file.cStr()) : "no patch");
    }
}

// Formats for the page (file scope: valid until the function's next call).
char statusText[512];
char detailText[512];
char progressTextBuf[64];
char indicatorText[64];
char mapText[512];
char toolText[MAX_OSPATH + 64];
double toolCheckedAt{0.0}; // when toolText was made (Sys_DoubleTime)
bool toolWasFound = false; // what toolText says
bool visWasFound = false;  // VisPatch's data found then (See-Through Liquids' row)
int toolGeneration = -1;   // the tool::generation() toolText was made at (a download finished: looked again at once)
za::String toolCvars;      // vr_relight_tool, vr_relight_tool_dir, vr_relight_vispatch_dir and Menu Detail: Developer then (a change: looked again at once)

// The maps the batch has done with (relit, skipped, failed).
[[nodiscard]] int doneCount()
{
    return batch.relit + batch.skipped + batch.failed;
}

// Seconds left by the work done so far ( < 0: too early to say).
[[nodiscard]] double secondsLeft()
{
    const double p = progress();
    const double elapsed = Sys_DoubleTime() - batch.started;
    if(p < 0.01 || elapsed < 2.0)
    {
        return -1.0;
    }
    return elapsed * (1.0 - p) / p;
}

} // namespace detail

void registerCommands()
{
    Cmd_AddCommand("vr_relight", relightCommand);
    Cmd_AddCommand("vr_brush_bake_map", relightWithBrushesCommand); // (the brushes' full bake: vr_brushbake.cpp)
    Cmd_AddCommand("vr_relight_batch", batchCommand);
    Cmd_AddCommand("vr_relight_cancel", cancelCommand);
    Cmd_AddCommand("vr_relight_revert", revertCommand);
    Cmd_AddCommand("vr_relight_status", statusCommand);
    Cmd_AddCommand("vr_relight_whendone", whenDoneCommand);
    Cmd_AddCommand("vr_relight_defaults", defaultsCommand);
    Cmd_AddCommand("vr_relight_lights", lightsCommand);
    Cmd_AddCommand("vr_relight_vispatch", vispatchCommand);
    Cmd_AddCommand("vr_relight_get_tool", tool::command); // (vr_relight_tool.cpp)
}

void poll()
{
    tool::poll(); // (a finished ericw-tools download's line)
    if(saving)
    {
        if(Host_IsSaving())
        {
            return;
        }
        Cbuf_InsertText("load \"autosave/relight\"\n");
        saving = false;
        return;
    }
    const double now = Sys_DoubleTime();
    if(!batch.active || now < batch.nextPoll)
    {
        return;
    }
    batch.nextPoll = now + 0.1;
    for(int s = 0; s < maxSlots; s++)
    {
        Slot& slot = batch.slots[s];
        if(slot.item < 0)
        {
            continue;
        }
        readProgress(slot);
        if(!process::running(s))
        {
            slotEnded(s);
            if(saving)
            {
                return; // (the next maps after the reload's save)
            }
        }
    }
    fillSlots();
    if(!anyRunning() && batch.next >= batch.items.size())
    {
        endBatch();
    }
}

bool firstStart()
{
    const za::String marker = files::join(za::StringView{com_gamedir}, za::StringView{firstStartMarker});
    if(!files::exists(marker.cStr()))
    {
        return false;
    }
    if(!files::remove(marker.cStr()))
    {
        Con_Printf("Relight: %s cannot be removed: the installer's first-start relight is not started (Graphics > "
                   "Relighting starts it)\n",
            marker.cStr());
        return false;
    }
    Con_Printf("Relight: a first start after the installer: every map relit, except those relit with these settings "
               "already and Quake VR's own, lit as they ship (vr_relight_batch everything)\n");
    Cbuf_InsertText("vr_relight_batch everything\n");
    return true;
}

namespace
{
// customCurrent's answers, by the copy's stem: kept while the original, the copy and its .relight are as they were.
struct CustomCheck
{
    za::String stamp; // the original's place, size and write time; the copy's and its .relight's write times
    bool current{false};
};
ankerl::unordered_dense::map<za::String, CustomCheck> customChecks;

// A stale copy's files moved into relit_custom/_stale/<game>/maps/ (an older stale copy there replaced): kept, not
// loaded (VR_ModelFile looks in relit_custom/<game>/ only).
void moveAside(const za::String& dir, const char* game, const char* map)
{
    const za::String from = files::join(dir, za::StringView{va("relit_custom/%s/maps/%s", game, map)});
    const za::String to = files::join(dir, za::StringView{va("relit_custom/_stale/%s/maps/%s", game, map)});
    files::createDirectories(za::String{files::parentPath(za::StringView{to})}.cStr());
    for(const char* ext : {".bsp", ".lit", ".lux", ".relight"})
    {
        const za::String f = from + ext;
        if(files::isFile(f.cStr()) && !files::rename(f.cStr(), (to + ext).cStr()))
        {
            Con_Printf("Relight: can't move %s aside\n", f.cStr());
        }
    }
    VR_FileCacheForget();
}
} // namespace

bool customCurrent(const char* game, const char* name)
{
    // The original: what the game loads without the copy.
    if(!COM_FileExists(name, nullptr) || !com_filesource[0])
    {
        return true; // (nothing to compare with)
    }
    // (com_filesize is a .pak's file's size; a loose file's is not looked up by COM_FileExists)
    const qfileofs_t inPak = com_filesize;
    const za::String origin{com_filesource};
    const bool loose = !files::isFile(origin.cStr()); // (a game folder; else a .pak)
    const za::String originFile = loose ? files::join(origin, za::StringView{name}) : origin;
    const za::I64 size = loose ? static_cast<za::I64>(files::fileSize(originFile.cStr())) : static_cast<za::I64>(inPak);
    const za::I64 originTime = files::lastWriteTime(originFile.cStr());
    // The copy: relit_custom/ in a game folder (never in a .pak: written by the game).
    const char* relit = va("relit_custom/%s/%s", game, name);
    if(!COM_FileExists(relit, nullptr) || !com_filesource[0] || files::isFile(com_filesource))
    {
        return true;
    }
    const za::String dir{com_filesource};
    char map[MAX_QPATH];
    COM_StripExtension(name + 5, map, sizeof(map)); // ("maps/")
    const za::String stem = files::join(dir, za::StringView{va("relit_custom/%s/maps/%s", game, map)});
    const za::String note = stem + ".relight";
    const za::String stamp{va("%s|%lld|%lld|%lld|%lld", origin.cStr(), static_cast<long long>(size),
        static_cast<long long>(originTime), static_cast<long long>(files::lastWriteTime((stem + ".bsp").cStr())),
        static_cast<long long>(files::lastWriteTime(note.cStr())))};
    const auto known = customChecks.find(stem);
    if(known != customChecks.end() && known->second.stamp == stamp)
    {
        return known->second.current;
    }
    za::String text;
    bool current = false;
    // (an old .relight, with no source line: no read)
    if(files::readText(note.cStr(), text) && za::StringView{text}.find(za::StringView{"\nsource "}) != za::StringView::nPos)
    {
        if(byte* bytes = COM_LoadMallocFile(name, nullptr))
        {
            current = noteHasSource(za::StringView{text},
                sourceId(bytes, static_cast<za::SizeT>(za::max<qfileofs_t>(com_filesize, 0))));
            VR_HeapFree(bytes);
        }
    }
    customChecks[stem] = CustomCheck{stamp, current};
    if(!current)
    {
        moveAside(dir, game, map);
        Con_Printf("Relight: the relit copy of %s is out of date (made from another version of the map, or before "
                   "relit copies recorded it): using the original; relight it again in Graphics > Relighting (the old "
                   "copy is in relit_custom/_stale/)\n",
            map);
    }
    return current;
}

void shutdown()
{
    tool::finish(); // (an ericw-tools download cancelled, its half-made folder removed)
    if(batch.active)
    {
        cancelBatch(nullptr); // (no half-made files left in the work folder)
    }
    process::stopAll(true); // (its job object closed)
}

float progress()
{
    if(batch.items.empty())
    {
        return -1.f;
    }
    double total = 0.0, done = 0.0;
    for(const Item& item : batch.items)
    {
        if(item.state == State::Skipped)
        {
            continue; // (no work)
        }
        total += item.weight;
        // (a cancelled map counts as not done: the bar stays where the batch got to)
        done += item.state == State::Running                                         ? item.weight * item.fraction :
                item.state == State::Queued || item.state == State::Cancelled ? 0.0 :
                                                                                       item.weight;
    }
    return total > 0.0 ? static_cast<float>(done / total) : 1.f;
}

bool running()
{
    return batch.active;
}

BatchResult lastBatch()
{
    BatchResult r;
    r.ended = !batch.active && batch.ended > 0.0 && !batch.single;
    r.maps = static_cast<int>(batch.items.size());
    r.relit = batch.relit;
    r.skipped = batch.skipped;
    r.failed = batch.failed;
    r.cancelled = batch.cancelled;
    r.alreadyLit = batch.alreadyLit;
    return r;
}

const char* statusLine()
{
    if(batch.active)
    {
        const double elapsed = Sys_DoubleTime() - batch.started;
        if(batch.single)
        {
            q_snprintf(statusText, sizeof(statusText), "Relighting %s (%s)", batch.items[0].source.map.cStr(), hms(elapsed));
        }
        else
        {
            q_snprintf(statusText, sizeof(statusText), "Relighting: %d of %d maps (%s)", doneCount(),
                static_cast<int>(batch.items.size()), hms(elapsed));
        }
        return statusText;
    }
    q_strlcpy(statusText, status[0] ? status : "Not relit in this session.", sizeof(statusText));
    for(char* c = statusText; *c; c++)
    {
        *c = *c == '\n' ? ' ' : *c; // (a failure's log lines, on one line)
    }
    return statusText;
}

const char* detailLine()
{
    detailText[0] = 0;
    if(!batch.active)
    {
        if(batch.ended > 0.0 && !batch.single)
        {
            q_snprintf(detailText, sizeof(detailText), "%d relit, %d skipped, %d failed", batch.relit, batch.skipped,
                batch.failed);
        }
        return detailText;
    }
    int running = 0;
    for(const Slot& s : batch.slots)
    {
        running += s.item >= 0 ? 1 : 0;
    }
    for(const Slot& s : batch.slots)
    {
        if(s.item < 0)
        {
            continue;
        }
        const char* map = batch.items[static_cast<za::SizeT>(s.item)].source.map.cStr();
        // One map: its stage and percentage; several: each one's percentage of its run.
        const char* part = running == 1 ? va("%s: %s%s", map, s.stage, s.percent >= 0 ? va(" %d%%", s.percent) : "")
                                        : va("%s%s %.0f%%", detailText[0] ? "  " : "", map,
                                              100.f * batch.items[static_cast<za::SizeT>(s.item)].fraction);
        q_strlcat(detailText, part, sizeof(detailText));
    }
    if(!running)
    {
        q_strlcpy(detailText, "checking the next maps", sizeof(detailText));
    }
    return detailText;
}

const char* progressText()
{
    const float p = progress();
    if(p < 0.f)
    {
        return "";
    }
    if(!batch.active)
    {
        q_snprintf(progressTextBuf, sizeof(progressTextBuf), "%.0f%% %s", 100.f * p, hms(batch.ended - batch.started));
        return progressTextBuf;
    }
    const double left = secondsLeft();
    q_snprintf(progressTextBuf, sizeof(progressTextBuf), "%.0f%% %s", 100.f * p, left < 0.0 ? "--:--" : hms(left));
    return progressTextBuf;
}

const char* indicator()
{
    if(!batch.active || vr_relight_indicator.value == 0.f)
    {
        return nullptr;
    }
    const double left = secondsLeft();
    if(batch.single)
    {
        q_snprintf(indicatorText, sizeof(indicatorText), "RELIGHT %s %.0f%%", batch.items[0].source.map.cStr(),
            100.f * progress());
    }
    else
    {
        q_snprintf(indicatorText, sizeof(indicatorText), "RELIGHT %d/%d %.0f%% %s", doneCount(),
            static_cast<int>(batch.items.size()), 100.f * progress(), left < 0.0 ? "" : hms(left));
    }
    return indicatorText;
}

const char* mapLine()
{
    char map[MAX_QPATH];
    if(!currentMap(map, sizeof(map)))
    {
        return "No map loaded.";
    }
    char game[MAX_QPATH];
    if(!VR_MapGameFolder(va("maps/%s.bsp", map), game, sizeof(game)))
    {
        q_snprintf(mapText, sizeof(mapText), "%s: its light as it is.", map);
        return mapText;
    }
    const char* custom = va("relit_custom/%s/maps/%s.bsp", game, map);
    const bool hasCustom = COM_FileExists(custom, nullptr);
    const bool hasRelit = COM_FileExists(va("relit/%s/maps/%s.bsp", game, map), nullptr);
    const char* loaded = !vr_relit_maps.value ? "Relit Maps off: its own light" :
                         hasCustom && vr_relight_use.value ? "relit in the game" :
                         hasRelit ? "relit by relight_maps.py" : "its own light";
    q_snprintf(mapText, sizeof(mapText), "%s (%s): %s%s.", map, game, loaded,
        hasCustom && !vr_relight_use.value ? " (an in-game relight is there, not used)" : "");
    return mapText;
}

const char* toolLine()
{
    // (looked for again every two seconds while the page shows it: a file check a search path folder)
    const double now = Sys_DoubleTime();
    const za::String cvars = za::String{vr_relight_tool.string} + "|" + vr_relight_tool_dir.string + "|" +
                             vr_relight_vispatch_dir.string + (vr_menu_level.value >= 2.f ? "|dev" : "");
    if(toolText[0] && now < toolCheckedAt + 2.0 && toolGeneration == tool::generation() && toolCvars == cvars)
    {
        return toolText;
    }
    toolCheckedAt = now;
    toolGeneration = tool::generation();
    toolCvars = cvars;
    const za::String found = findTool();
    toolWasFound = !found.empty();
    visWasFound = vis::available();
    q_snprintf(toolText, sizeof(toolText), "%s",
        found.empty() ? "light.exe: not found (Download ericw-tools below, or vr_relight_tool)" : va("light.exe: %s", found.cStr()));
    return toolText;
}

bool toolFound()
{
    (void)toolLine();
    return toolWasFound;
}

bool seeThroughAvailable()
{
    (void)toolLine();
    return visWasFound;
}

int toolPageState()
{
    // (what the page shows of the tool: found or not, a download running, its result; a change rebuilds the page)
    return (toolFound() ? 1 : 0) | (tool::running() ? 2 : 0) | (tool::statusLine()[0] ? 4 : 0) |
           (seeThroughAvailable() ? 8 : 0);
}

} // namespace qvr::relight

extern "C" const char* VR_RelightIndicator(float* fraction)
{
    // (the wrist gadget's screen shows it in the headset; the menu's page while it is open)
    if(qvr::gadget::active() || key_dest == key_menu)
    {
        return nullptr;
    }
    const char* text = qvr::relight::indicator();
    if(text && fraction)
    {
        *fraction = qvr::relight::progress();
    }
    return text;
}
