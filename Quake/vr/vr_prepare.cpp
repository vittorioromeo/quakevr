// vr_prepare.cpp -- the installer's preparation run (vr_prepare.hpp).

#include "vr_prepare.hpp"

#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_limbmodel.hpp"
#include "vr_ragdoll.hpp"
#include "vr_relight.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

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
    Done,
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
    const size_t n = strlen(file);
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
    za::quickSort(names.begin(), names.end(), [](const za::String& a, const za::String& b) { return strcmp(a.cStr(), b.cStr()) < 0; });
    return names;
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
        finish();
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
        if(!run.out)
        {
            Con_Printf("Prepare: can't write %s\n", com_argv[arg + 1]);
        }
    }
    run.relight = COM_CheckParm("-preparerelight") != 0;
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
            if(run.map >= za::getArraySize(firstMaps))
            {
                line(va("result maps ok=%d failed=%d", static_cast<int>(run.map) - run.failedMaps, run.failedMaps));
                if(inPlay(firstMaps[za::getArraySize(firstMaps) - 1].name))
                {
                    run.step = Step::Guns;
                }
                else
                {
                    afterMaps(); // (the hub did not load: the guns are cut in play, as before)
                }
                return;
            }
            const FirstMap& map = firstMaps[run.map];
            line(va("step maps %d %d %s", static_cast<int>(run.map) + 1, static_cast<int>(za::getArraySize(firstMaps)),
                map.name));
            Cbuf_AddText(map.command);
            run.since = now;
            run.signedOn = -1.0;
            run.step = Step::MapWait;
            return;
        }
        case Step::MapWait:
        {
            const char* map = firstMaps[run.map].name;
            if(inPlay(map))
            {
                if(run.signedOn < 0.0)
                {
                    run.signedOn = now;
                }
                if(now - run.signedOn >= settleSeconds)
                {
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
            Cbuf_AddText("vr_relight_batch everything\n");
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
            finish();
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
            if(!strcmp(sv.model_precache[i], name))
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
