// vr_prepare.cpp -- the installer's preparation run (vr_prepare.hpp).

#include "vr_prepare.hpp"

#include "vr_ambient.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_diskcache.hpp"
#include "vr_files.hpp"
#include "vr_hull_internal.hpp"
#include "vr_limbmodel.hpp"
#include "vr_nav.hpp"
#include "vr_ragdoll.hpp"
#include "vr_relight.hpp"
#include "vr_relight_maps.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "vr_api.h"

#include <stdio.h>

namespace qvr::prepare
{

namespace
{

enum class Step
{
    Off,
    Map,         // the next map's command given
    MapWait,     // its load, then a few seconds for the caches written on the pool
    Guns,        // on the hub (the last map): every weapon a monster can drop, dropped (vr_pickup_test 4)
    GunsWait,    // their convex pieces cut (or read: made before) and their files written
    Models,      // on the hub: every model of the game's folders and the monsters' limbs (vr_prepare_models)
    ModelsWait,  // loaded (their skins' normal maps made and written)
    Campaign,     // the next installed campaign with a folder of its own: made the active one, its start map's command
    CampaignWait, // its start map in play: then the models' step again, for its own models
    Relight,     // the batch's command given
    RelightWait, // the batch running
    Hulls,       // the next common map's command given (-preparehulls)
    HullsWait,   // its load, then every tree it asked for (the deferred ones too) made or read
    Done,
};

// One map of the hulls' step: its name and the campaign it is played in (Quake's, Scourge of Armagon's or Dissolution of
// Eternity's: "start" is each one's own).
struct HullMap
{
    za::String map;
    int campaign{0};
};

// The hulls' step's counts and the files of its maps' collision (the list Setup's Quest copy reads).
struct HullCounts
{
    int ok{0};
    int failed{0};
    int fresh{0}; // files written by this run (not there before the map's load)
    za::U64 bytes{0};
    double since{0.0};
    za::Vector<za::String> files;  // "hulls/<build>/<key>..." (under the disk cache's root)
    za::Vector<za::String> before; // every hull file there when the step began (the same form)
};

struct Run
{
    Step step{Step::Off};
    FILE* out{nullptr};
    bool relight{false};
    za::SizeT map{0};
    double since{0.0};     // the step's start (Sys_DoubleTime)
    double signedOn{-1.0}; // when the map was in play
    int frames{0};         // frames since the relight's or the guns' command
    box3d::GunPiecesCount guns; // the guns' pieces before the guns' command
    double nextProgress{0.0};
    int failedMaps{0};
    // The campaigns' pass (after the hub's): the campaign the models' step is for (0: Quake's, the hub).
    int campaign{0};
    int campaignStep{0};  // campaigns begun (the progress line's i, from 1)
    int campaignCount{0}; // campaigns to visit (its n)
    double campaignSince{0.0};
    bool switched{false}; // a campaign other than Quake's made active: Quake's again at the end
    int campaignsOk{0};
    int campaignsNone{0};
    int campaignsFailed{0};
    // -preparemaps <map>[,<map>...]: only these maps (the shipped cache's generator, Misc/release/make_shipped_cache.py:
    // one map a run, its files then all its own), then the end: no guns, models, campaigns nor relight.
    za::Vector<za::String> onlyMaps;
    // -preparehulls [<map>[,<map>...]]: the common maps' collision (their hull trees and brushes) made once, after the
    // relight (the relit copy is the map the game loads: the trees are keyed by its world), so their first visits read
    // it; the list given: only those maps (Quake's campaign).
    bool hulls{false};
    za::String progressPath; // the progress file's (the hulls' list beside it)
    za::Vector<HullMap> hullMaps;
    za::SizeT hullMap{0};
    int hullCampaign{0}; // the campaign active for the hulls' step (0: Quake's)
    HullCounts hullCounts;
};
Run run;

// vr_prepare_models' last counts (the preparation's line).
struct Precached
{
    int models{0}; // models precached (not the map's already)
    int limbs{0};  // limb models precached
};
Precached precached;

// The maps a first start loads, in its order and as it loads them (the calibration room: vr_setup.cpp; the tutorial on
// Easy and the hub: vr_main.cpp, startGameCommands; the monsters a skill spawns choose the hulls' boxes): their disk
// caches made.
struct FirstMap
{
    const char* name;
    const char* command;
};
constexpr FirstMap firstMaps[] = {
    {"vrcalibration", "maxplayers 1; deathmatch 0; coop 0; map vrcalibration\n"},
    {"vrtutorial", "maxplayers 1; deathmatch 0; coop 0; skill 0; map vrtutorial\n"},
    {"vrstart", "maxplayers 1; deathmatch 0; coop 0; map vrstart\n"}, // (the tutorial's skill kept, as its exit's)
};
constexpr double settleSeconds = 3.0; // after the map is in play: the pool's jobs (ambient occlusion, hull files) end
constexpr double mapTimeout = 600.0;  // a map not in play by then is passed over (a slow PC's cold vrstart: about a minute)
// The monsters' guns (PERF_DECISIONS.md 13): each one a monster drops was cut at its first drop in a session (the
// grunt's 35 ms, the knights' swords 19, the ogre's chainsaw 84 on the main thread) unless a session before cut it;
// here they are all dropped once on the hub, the game's way (the pieces' key is the dropped gun's drawn triangles; a
// held one's are the same), so the first session's first deaths read them. The cuts are done in the frame the guns
// become bodies; the wait is for those frames.
constexpr double gunsSeconds = 1.0;
constexpr int gunsFrames = 20;
// The models (vr_prepare_models): the hub precaches the monsters for its firing range, not their limbs (made for the
// monsters a map spawns) nor the items it has none of (armour, keys, runes...): a first session's first visits made
// those skins' normal maps at their load (e1m1, e1m2, e2m2, e4m7, hip2m3 after the hub: 126 skins, 218 ms in all).
// The precaches load at once (the client's at the next frames): the wait is for those frames.
constexpr double modelsSeconds = 1.0;
constexpr int modelsFrames = 20;
// The official campaigns whose game folder is searched only while they are the active one (Dimension of the Past,
// Dimension of the Machine, Dawn of the Machine; the mission packs' folders are on the hub's search path, their models
// precached there): each installed one made active (as the selector does: vr_campaign_select's selectCampaign, the game
// folders rebuilt), its start map loaded (a precache needs a map in play), then its own folder's models and their limbs
// (vr_prepare_models <folder>) as on the hub; one with no model of its own (Dimension of the Past) is passed over
// without a map. Quake's campaign is made active again at the end (the relight's batch sees Quake's folders, as before).
// Not the guns again: dropped there (vr_pickup_test 4) on Dimension of the Machine's and Dawn of the Machine's start
// maps, none was cut (the dropped guns are the same models), for 1 s each.
constexpr double campaignSettleSeconds = 1.0; // (the models' wait follows: 1 s more)

// The files under `relative` of a loose game folder `dir`, a few folders deep.
void addLooseFiles(const za::String& dir, const za::String& relative, int depth, za::Vector<za::String>& found)
{
    files::forEachEntry((dir + "/" + relative).cStr(), [&](const char* name, bool isDirectory) {
        const za::String path = relative + "/" + name;
        if(!isDirectory)
        {
            found.pushBack(path);
        }
        else if(depth > 0 && name[0] != '.')
        {
            addLooseFiles(dir, path, depth - 1, found);
        }
    });
}

[[nodiscard]] bool isModel(const char* file, bool anyDepth)
{
    const za::SizeT n = ZA_STRLEN(file);
    return n > 10 && !q_strncasecmp(file, "progs/", 6) && (anyDepth || !strchr(file + 6, '/')) &&
           !q_strcasecmp(file + n - 4, ".mdl");
}

// The models vr_prepare_models precaches: every progs/*.mdl of the search paths (`folder` null), or every progs/**.mdl
// of one game folder's (its paks' and its loose files); once each, sorted (the same order every time).
za::Vector<za::String> modelNames(const char* folder)
{
    za::Vector<za::String> names;
    const auto add = [&names, folder](const char* file) {
        if(!isModel(file, folder != nullptr))
        {
            return;
        }
        for(const za::String& have : names)
        {
            if(!q_strcasecmp(have.cStr(), file))
            {
                return;
            }
        }
        names.emplaceBack(file);
    };
    for(const searchpath_t* search = com_searchpaths; search; search = search->next)
    {
        if(folder)
        {
            char game[MAX_OSPATH];
            VR_GameFolderName(search->pack ? search->pack->filename : search->filename, game, sizeof(game));
            if(q_strcasecmp(game, folder))
            {
                continue;
            }
        }
        if(search->pack)
        {
            for(int i = 0; i < search->pack->numfiles; i++)
            {
                add(search->pack->files[i].name);
            }
        }
        else if(search->filename[0])
        {
            za::Vector<za::String> loose;
            addLooseFiles(za::String{search->filename}, za::String{"progs"}, folder ? 3 : 0, loose);
            for(const za::String& file : loose)
            {
                add(file.cStr());
            }
        }
    }
    za::quickSort(names.begin(), names.end(), [](const za::String& a, const za::String& b) { return ZA_STRCMP(a.cStr(), b.cStr()) < 0; });
    return names;
}

// The maps' step's maps: -preparemaps', else the first start's.
[[nodiscard]] za::SizeT mapCount()
{
    return run.onlyMaps.empty() ? za::getArraySize(firstMaps) : run.onlyMaps.size();
}

[[nodiscard]] const char* mapName(za::SizeT i)
{
    return run.onlyMaps.empty() ? firstMaps[i].name : run.onlyMaps[i].cStr();
}

// Its command: the first start's for one of its maps (the tutorial's skill), else a single-player game's.
[[nodiscard]] za::String mapCommand(za::SizeT i)
{
    const char* name = mapName(i);
    for(const FirstMap& m : firstMaps)
    {
        if(!q_strcasecmp(m.name, name))
        {
            return za::String{m.command};
        }
    }
    return za::String{"maxplayers 1; deathmatch 0; coop 0; map "} + name + "\n";
}

void line(const char* text)
{
    Con_Printf("Prepare: %s\n", text);
    if(run.out)
    {
        fprintf(run.out, "%s\n", text);
        fflush(run.out);
    }
}

[[nodiscard]] bool inPlay(const char* map)
{
    return sv.active && cls.state == ca_connected && cls.signon == SIGNONS && !q_strcasecmp(sv.name, map);
}

void finish()
{
    run.step = Step::Done;
    line("done");
    if(run.out)
    {
        fclose(run.out);
        run.out = nullptr;
    }
    Cbuf_AddText("quit\n");
}

void afterRelight();

// After the maps (and the guns): the relight, or the end.
void afterMaps()
{
    if(run.relight)
    {
        Cbuf_AddText("disconnect\n"); // (no map in play: the batch reloads none)
        run.step = Step::Relight;
    }
    else
    {
        afterRelight();
    }
}

// The next campaign of the pass (after `after`) that is installed, playable and has a folder of its own; 0 when none.
[[nodiscard]] int nextCampaign(int after)
{
    for(int c = after + 1; c < VR_CampaignCount(); c++)
    {
        if(VR_CampaignFolderOnlyWhenActive(c) && !VR_CampaignUnavailable(c))
        {
            return c;
        }
    }
    return 0;
}

void relightResult()
{
    const relight::BatchResult r = relight::lastBatch();
    line(va("result relight ended=%d maps=%d relit=%d skipped=%d failed=%d cancelled=%d own=%d | %s", r.ended ? 1 : 0,
        r.maps, r.relit, r.skipped, r.failed, r.cancelled, r.alreadyLit, relight::statusLine()));
}

// The hulls' step's maps: the single-player maps of Quake's and the mission packs' folders that are installed (start,
// e1m1..e4m8, end; hipstart, hip1m1.., hipend; start, r1m1.., end), not the deathmatch ones (dm1, hipdm1, ctf1).
constexpr int hullCampaigns = 3; // campaigns[] 0..2: id1, hipnotic, rogue (vr_gamedir.cpp)

[[nodiscard]] bool commonMap(const char* map)
{
    if(strchr(map, '/'))
    {
        return false;
    }
    for(const char* named : {"start", "end", "hipstart", "hipend"})
    {
        if(!q_strcasecmp(map, named))
        {
            return true;
        }
    }
    return !relight::maps::episodePrefix(map).empty();
}

void collectHullMaps()
{
    if(!run.hullMaps.empty())
    {
        return; // (the list given)
    }
    for(int c = 0; c < hullCampaigns && c < VR_CampaignCount(); c++)
    {
        if(c && VR_CampaignUnavailable(c))
        {
            continue;
        }
        za::Vector<relight::maps::Source> found;
        za::String why;
        if(!relight::maps::collect(relight::maps::Set::Game, VR_CampaignFolder(c), "", found, why))
        {
            continue;
        }
        for(const relight::maps::Source& s : found)
        {
            if(commonMap(s.map.cStr()))
            {
                run.hullMaps.pushBack(HullMap{s.map, c});
            }
        }
    }
}

// The disk cache's hull files (trees and brushes: "<key>_<box>.hul", "<key>.hbr" under hulls/<build>/) whose name
// begins with `key` (a world's key in hex; "": all), as paths under the cache's root.
za::Vector<za::String> hullFiles(const char* key)
{
    za::Vector<za::String> out;
    const za::SizeT keyLength = ZA_STRLEN(key);
    const za::String root = diskcache::root();
    const char* kind = diskcache::kindOf(diskcache::Product::Hulls);
    files::forEachEntry((root + "/" + kind).cStr(), [&](const char* build, bool isDirectory) {
        if(!isDirectory || build[0] == '.')
        {
            return;
        }
        const za::String folder = za::String{kind} + "/" + build;
        files::forEachEntry((root + "/" + folder).cStr(), [&](const char* name, bool isFolder) {
            if(!isFolder && !q_strncasecmp(name, key, static_cast<int>(keyLength)))
            {
                out.pushBack(folder + "/" + name);
            }
        });
    });
    return out;
}

// The world in play's hull files.
za::Vector<za::String> worldHullFiles()
{
    if(!sv.active || !sv.worldmodel || sv.worldmodel->type != mod_brush)
    {
        return {};
    }
    char key[24];
    q_snprintf(key, sizeof(key), "%016llx", static_cast<unsigned long long>(hull::detail::keyOf(sv.worldmodel)));
    return hullFiles(key);
}

// The hulls' list beside the progress file (setup_hulls.txt): a line per file, its path under the disk cache's root
// and its whole path, a tab between (Setup's Quest copy: Installer/.../QuestFiles.cs, AddHulls).
void writeHullList()
{
    if(run.progressPath.empty())
    {
        return;
    }
    za::String path = run.progressPath;
    const char* p = path.cStr();
    const char* slash = strrchr(p, '/');
    const char* back = strrchr(p, '\\');
    const char* sep = back > slash ? back : slash;
    za::String listPath = sep ? za::String{p, static_cast<za::SizeT>(sep - p + 1)} : za::String{};
    listPath += "setup_hulls.txt";
    const za::String root = diskcache::root();
    za::String text;
    for(const za::String& f : run.hullCounts.files)
    {
        text += f + "\t" + root + "/" + f + "\n";
    }
    if(!files::writeText(listPath.cStr(), za::StringView{text.cStr(), text.size()}))
    {
        Con_Printf("Prepare: can't write %s\n", listPath.cStr());
    }
}

void startHulls()
{
    collectHullMaps();
    run.hullMap = 0;
    run.hullCampaign = 0;
    run.hullCounts = HullCounts{};
    run.hullCounts.since = Sys_DoubleTime();
    run.hullCounts.before = hullFiles("");
    hull::setWriteEveryTree(true); // (the PC's quick trees too: the Quest's copies)
    run.step = Step::Hulls;
}

// After the relight (or the maps when there is none): the hulls, or the end.
void afterRelight()
{
    if(run.hulls)
    {
        startHulls();
    }
    else
    {
        finish();
    }
}

} // namespace

bool active()
{
    // -preparerelight implies -prepare: the relight is a step of the preparation run, so asking for the relight alone
    // runs the preparation just as -prepare does (COM_PrepareRun: the autosave check and the hidden window ask too).
    return COM_PrepareRun();
}

void start()
{
    if(run.step != Step::Off)
    {
        return;
    }
    const int arg = COM_CheckParm("-prepare"); // the progress file follows the flag itself: an implied -prepare (-preparerelight alone) has no path to write to
    if(arg && arg + 1 < com_argc && com_argv[arg + 1][0] != '-' && com_argv[arg + 1][0] != '+')
    {
        run.out = fopen(com_argv[arg + 1], "w");
        run.progressPath = za::String{com_argv[arg + 1]};
        if(!run.out)
        {
            Con_Printf("Prepare: can't write %s\n", com_argv[arg + 1]);
        }
    }
    run.relight = COM_CheckParm("-preparerelight") != 0;
    const int only = COM_CheckParm("-preparemaps");
    if(only && only + 1 < com_argc)
    {
        const char* list = com_argv[only + 1];
        while(*list)
        {
            const char* end = strchr(list, ',');
            const za::SizeT n = end ? static_cast<za::SizeT>(end - list) : ZA_STRLEN(list);
            if(n > 0 && n < 64)
            {
                char name[64];
                memcpy(name, list, n);
                name[n] = 0;
                run.onlyMaps.pushBack(za::String{name});
            }
            list += end ? n + 1 : n;
        }
        run.relight = false;
        // The shipped cache's run: every product written however quick it was here (the trees' 25 ms bar and the light
        // grid's 0.1 s are this PC's: a faster exe left out files the Quest, 5-15x slower, needs), so the file list is
        // the same from any exe on any PC.
        hull::setWriteEveryTree(true);
        ambient::setWriteEveryGrid(true);
    }
    const int hulls = COM_CheckParm("-preparehulls");
    run.hulls = hulls != 0;
    if(hulls && hulls + 1 < com_argc && com_argv[hulls + 1][0] != '-' && com_argv[hulls + 1][0] != '+')
    {
        const char* list = com_argv[hulls + 1];
        while(*list)
        {
            const char* end = strchr(list, ',');
            const za::SizeT n = end ? static_cast<za::SizeT>(end - list) : ZA_STRLEN(list);
            if(n > 0 && n < 64)
            {
                run.hullMaps.pushBack(HullMap{za::String{list, n}, 0});
            }
            list += end ? n + 1 : n;
        }
    }
    // No tips: one shown to the mock headset would be marked seen in tips_seen.txt and never shown to the player (the
    // config, where this would stay, is not written: -noconfigwrite).
    Cvar_Set("vr_tips", "0");
    run.map = 0;
    run.step = Step::Map;
    line(va("start %s", VR_BuildVersion()));
}

void frame()
{
    const double now = Sys_DoubleTime();
    switch(run.step)
    {
        case Step::Off:
        case Step::Done: return;
        case Step::Map:
        {
            if(run.map >= mapCount())
            {
                line(va("result maps ok=%d failed=%d", static_cast<int>(run.map) - run.failedMaps, run.failedMaps));
                if(!run.onlyMaps.empty())
                {
                    afterRelight(); // (no relight with a list: the hulls when asked, else the end)
                }
                else if(inPlay(firstMaps[za::getArraySize(firstMaps) - 1].name))
                {
                    run.step = Step::Guns;
                }
                else
                {
                    afterMaps(); // (the hub did not load: the guns are cut in play, as before)
                }
                return;
            }
            const char* name = mapName(run.map);
            line(va("step maps %d %d %s", static_cast<int>(run.map) + 1, static_cast<int>(mapCount()), name));
            Cbuf_AddText(mapCommand(run.map).cStr());
            run.since = now;
            run.signedOn = -1.0;
            run.step = Step::MapWait;
            return;
        }
        case Step::MapWait:
        {
            const char* map = mapName(run.map);
            if(inPlay(map))
            {
                if(run.signedOn < 0.0)
                {
                    run.signedOn = now;
                }
                if(now - run.signedOn >= settleSeconds)
                {
                    // Every product still on the pool finished and written before the next map (or the end): the
                    // load's trees and the monsters' deferred ones (hull), the navmesh's size classes (joined otherwise
                    // by a monster's first path, which a quick map may never ask for).
                    hull::finishLoads();
                    nav::finishBuilds();
                    line(va("map %s ok %.1f", map, run.signedOn - run.since));
                    run.map++;
                    run.step = Step::Map;
                }
            }
            else if(now - run.since > mapTimeout)
            {
                line(va("map %s failed %.1f", map, now - run.since));
                run.failedMaps++;
                run.map++;
                run.step = Step::Map;
            }
            return;
        }
        case Step::Guns:
        {
            line("step guns");
            run.guns = box3d::gunPiecesCount();
            Cbuf_AddText("vr_pickup_test 4\n");
            run.since = now;
            run.frames = 0;
            run.step = Step::GunsWait;
            return;
        }
        case Step::GunsWait:
        {
            run.frames++;
            if(run.frames < gunsFrames || now - run.since < gunsSeconds)
            {
                return;
            }
            const box3d::GunPiecesCount c = box3d::gunPiecesCount();
            line(va("result guns read=%d cut=%d written=%d %.1f", c.read - run.guns.read, c.cut - run.guns.cut,
                c.written - run.guns.written, now - run.since));
            run.step = Step::Models;
            return;
        }
        case Step::Models:
        {
            if(!run.campaign)
            {
                line("step models");
            }
            Cbuf_AddText(run.campaign ? va("vr_prepare_models %s\n", VR_CampaignFolder(run.campaign)) : "vr_prepare_models\n");
            run.since = now;
            run.frames = 0;
            run.step = Step::ModelsWait;
            return;
        }
        case Step::ModelsWait:
        {
            run.frames++;
            if(run.frames < modelsFrames || now - run.since < modelsSeconds)
            {
                return;
            }
            if(!run.campaign)
            {
                line(va("result models %d %d %.1f", precached.models, precached.limbs, now - run.since));
                run.campaignCount = 0;
                for(int c = nextCampaign(0); c; c = nextCampaign(c))
                {
                    run.campaignCount++;
                }
            }
            else
            {
                line(va("campaign %s ok %.1f models=%d limbs=%d", VR_CampaignFolder(run.campaign),
                    now - run.campaignSince, precached.models, precached.limbs));
                run.campaignsOk++;
            }
            run.step = Step::Campaign;
            return;
        }
        case Step::Campaign:
        {
            const int c = nextCampaign(run.campaign);
            if(!c)
            {
                if(run.switched && !VR_SelectCampaignNoMap(0))
                {
                    line("campaign id1 failed 0.0"); // (never: Quake's is always there)
                }
                run.campaign = 0;
                if(run.campaignCount)
                {
                    line(va("result campaigns ok=%d none=%d failed=%d", run.campaignsOk, run.campaignsNone,
                        run.campaignsFailed));
                }
                afterMaps();
                return;
            }
            run.campaign = c;
            run.campaignStep++;
            run.campaignSince = now;
            const char* folder = VR_CampaignFolder(c);
            line(va("step campaigns %d %d %s", run.campaignStep, run.campaignCount, folder));
            if(!VR_SelectCampaignNoMap(c))
            {
                line(va("campaign %s failed 0.0", folder));
                run.campaignsFailed++;
                return;
            }
            run.switched = true;
            if(modelNames(folder).empty())
            {
                line(va("campaign %s none %.1f", folder, Sys_DoubleTime() - run.campaignSince));
                run.campaignsNone++;
                return;
            }
            Cbuf_AddText(va("maxplayers 1; deathmatch 0; coop 0; map %s\n", VR_CampaignStartMap(c)));
            run.since = now;
            run.signedOn = -1.0;
            run.step = Step::CampaignWait;
            return;
        }
        case Step::CampaignWait:
        {
            if(inPlay(VR_CampaignStartMap(run.campaign)))
            {
                if(run.signedOn < 0.0)
                {
                    run.signedOn = now;
                }
                if(now - run.signedOn >= campaignSettleSeconds)
                {
                    run.step = Step::Models;
                }
            }
            else if(now - run.since > mapTimeout)
            {
                line(va("campaign %s failed %.1f", VR_CampaignFolder(run.campaign), now - run.campaignSince));
                run.campaignsFailed++;
                run.step = Step::Campaign;
            }
            return;
        }
        case Step::Relight:
        {
            line("step relight");
            Cbuf_AddText("vr_relight_batch official\n"); // (not the Map Library's nor AD's maps: maps::Set::Official)
            run.frames = 0;
            run.nextProgress = 0.0;
            run.step = Step::RelightWait;
            return;
        }
        case Step::RelightWait:
        {
            run.frames++;
            if(relight::running())
            {
                if(now >= run.nextProgress)
                {
                    run.nextProgress = now + 0.5;
                    const float p = relight::progress();
                    line(va("relight %d %s", static_cast<int>(100.f * (p < 0.f ? 0.f : p)), relight::statusLine()));
                }
                return;
            }
            if(run.frames < 3)
            {
                return; // (the command runs at the next frame's start)
            }
            relightResult();
            afterRelight();
            return;
        }
        case Step::Hulls:
        {
            HullCounts& h = run.hullCounts;
            if(run.hullMap >= run.hullMaps.size())
            {
                if(run.hullCampaign != 0 && !VR_SelectCampaignNoMap(0))
                {
                    line("campaign id1 failed 0.0"); // (never: Quake's is always there)
                }
                run.hullCampaign = 0;
                writeHullList();
                line(va("result hulls ok=%d failed=%d files=%d written=%d mb=%.1f %.1f", h.ok, h.failed,
                    static_cast<int>(h.files.size()), h.fresh, static_cast<double>(h.bytes) / (1024.0 * 1024.0), now - h.since));
                finish();
                return;
            }
            const HullMap& m = run.hullMaps[run.hullMap];
            line(va("step hulls %d %d %s", static_cast<int>(run.hullMap) + 1, static_cast<int>(run.hullMaps.size()), m.map.cStr()));
            if(m.campaign != run.hullCampaign)
            {
                if(!VR_SelectCampaignNoMap(m.campaign))
                {
                    line(va("hulls %s failed 0.0", m.map.cStr()));
                    h.failed++;
                    run.hullMap++;
                    return;
                }
                run.hullCampaign = m.campaign;
            }
            // (Hard: the most monsters, so the most of their boxes' trees; the deathmatch flags off as the maps' step's)
            Cbuf_AddText(va("maxplayers 1; deathmatch 0; coop 0; skill 2; map %s\n", m.map.cStr()));
            run.since = now;
            run.frames = 0;
            run.step = Step::HullsWait;
            return;
        }
        case Step::HullsWait:
        {
            const HullMap& m = run.hullMaps[run.hullMap];
            HullCounts& h = run.hullCounts;
            // (the map's command runs at the next frame's start: a map of the same name in play before, "start" of
            // another campaign, is never taken for it)
            if(++run.frames > 2 && inPlay(m.map.cStr()))
            {
                hull::finishLoads(); // every tree the load asked for, the deferred ones too (vr_hull_defer), made or read
                const za::Vector<za::String> found = worldHullFiles();
                for(const za::String& f : found)
                {
                    const bool listed = za::findIf(h.files.begin(), h.files.end(), [&f](const za::String& g) {
                        return !q_strcasecmp(g.cStr(), f.cStr());
                    }) != h.files.end();
                    if(listed)
                    {
                        continue; // (a map whose world another's has: the same files)
                    }
                    h.fresh += za::findIf(h.before.begin(), h.before.end(), [&f](const za::String& g) {
                        return !q_strcasecmp(g.cStr(), f.cStr());
                    }) == h.before.end() ? 1 : 0;
                    h.bytes += files::fileSize((diskcache::root() + "/" + f).cStr());
                    h.files.pushBack(f);
                }
                line(va("hulls %s ok %.1f files=%d", m.map.cStr(), now - run.since, static_cast<int>(found.size())));
                h.ok++;
                run.hullMap++;
                run.step = Step::Hulls;
            }
            else if(now - run.since > mapTimeout)
            {
                line(va("hulls %s failed %.1f", m.map.cStr(), now - run.since));
                h.failed++;
                run.hullMap++;
                run.step = Step::Hulls;
            }
            return;
        }
    }
}

void precacheModels_f()
{
    if(!sv.active || svs.maxclients != 1 || !VR_AllowLatePrecache())
    {
        Con_Printf("vr_prepare_models: a single player map in play first\n");
        return;
    }
    const double t0 = Sys_DoubleTime();
    // With a game folder's name: that folder's models only, at any depth under progs/ ("campaign": the active one's).
    const char* folder = Cmd_Argc() > 1 ? Cmd_Argv(1) : nullptr;
    if(folder && !q_strcasecmp(folder, "campaign"))
    {
        folder = VR_ActiveCampaignFolder();
        if(!folder)
        {
            Con_Printf("vr_prepare_models: no campaign (quakevr is not on the search path)\n");
            return;
        }
    }
    const za::Vector<za::String> names = modelNames(folder);
    Precached& p = precached;
    p = {};
    const auto has = [](const char* name) {
        for(int i = 1; i < MAX_MODELS && sv.model_precache[i]; i++)
        {
            if(!ZA_STRCMP(sv.model_precache[i], name))
            {
                return true;
            }
        }
        return false;
    };
    for(const za::String& name : names)
    {
        if(!has(name.cStr()))
        {
            // (sv.model_precache keeps the pointer: the map's hunk holds the name, as a QC string would)
            (void)VR_LatePrecacheModel(Hunk_Strdup(name.cStr(), "vr_prepare_models"));
            p.models++;
        }
    }
    // The whole limbs of every monster's model (vr_limbs_prebuild's, for each kind a map spawns): each a ragdoll rig's
    // limb joint that is not the head's.
    if(vr_limbs.value)
    {
        za::Vector<qmodel_t*> rigged;
        for(int i = 1; i < MAX_MODELS && sv.model_precache[i]; i++)
        {
            qmodel_t* model = sv.models[i];
            // (a game folder's: its own models' limbs only)
            const bool named = !folder || za::findIf(names.begin(), names.end(), [i](const za::String& n) {
                return !q_strcasecmp(n.cStr(), sv.model_precache[i]);
            }) != names.end();
            if(model && named && model->type == mod_alias && !strchr(sv.model_precache[i], '#') && ragdoll::eligible(model))
            {
                rigged.pushBack(model);
            }
        }
        ragdoll::warmRigs(rigged.data(), static_cast<int>(rigged.size()));
        for(qmodel_t* model : rigged)
        {
            const ragdoll::Rig* rig = ragdoll::rigFor(model);
            if(!rig)
            {
                continue;
            }
            const uint32_t head = ragdoll::headBones(*rig);
            for(int bone = 0; bone < rig->numBones && bone < 32; bone++)
            {
                if(ragdoll::limbJoint(*rig, bone) && !(head & (1u << bone)) && limbmodel::available(model->name, bone))
                {
                    const char* limb = limbmodel::keptName(model->name, bone, 0);
                    if(!has(limb))
                    {
                        (void)VR_LatePrecacheModel(limb);
                        p.limbs++;
                    }
                }
            }
        }
    }
    Con_Printf("vr_prepare_models: %d models and %d limb models precached (%d files found%s%s), %.1f ms\n", p.models,
        p.limbs, static_cast<int>(names.size()), folder ? " in " : "", folder ? folder : "", (Sys_DoubleTime() - t0) * 1000.0);
}

} // namespace qvr::prepare
