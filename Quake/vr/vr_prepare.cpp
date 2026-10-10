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

void relightResult()
{
    const relight::BatchResult r = relight::lastBatch();
    line(va("result relight ended=%d maps=%d relit=%d skipped=%d failed=%d cancelled=%d own=%d | %s", r.ended ? 1 : 0,
        r.maps, r.relit, r.skipped, r.failed, r.cancelled, r.alreadyLit, relight::statusLine()));
}

} // namespace

bool active()
{
    return COM_CheckParm("-prepare") != 0;
}

void start()
{
    if(run.step != Step::Off)
    {
        return;
    }
    const int arg = COM_CheckParm("-prepare");
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
            line("step models");
            Cbuf_AddText("vr_prepare_models\n");
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
            line(va("result models %d %d %.1f", precached.models, precached.limbs, now - run.since));
            afterMaps();
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
    // Every progs/*.mdl of the search paths (no subfolders), once each, sorted (the same order every time).
    za::Vector<za::String> names;
    const auto add = [&names](const char* file) {
        const size_t n = strlen(file);
        if(n <= 10 || q_strncasecmp(file, "progs/", 6) || strchr(file + 6, '/') || q_strcasecmp(file + n - 4, ".mdl"))
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
        if(search->pack)
        {
            for(int i = 0; i < search->pack->numfiles; i++)
            {
                add(search->pack->files[i].name);
            }
        }
        else if(search->filename[0])
        {
            files::forEachEntry(va("%s/progs", search->filename), [&add](const char* name, bool isDirectory) {
                if(!isDirectory)
                {
                    add(va("progs/%s", name));
                }
            });
        }
    }
    za::quickSort(names.begin(), names.end(), [](const za::String& a, const za::String& b) { return strcmp(a.cStr(), b.cStr()) < 0; });
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
            if(model && model->type == mod_alias && !strchr(sv.model_precache[i], '#') && ragdoll::eligible(model))
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
    Con_Printf("vr_prepare_models: %d models and %d limb models precached (%d files found), %.1f ms\n", p.models, p.limbs,
        static_cast<int>(names.size()), (Sys_DoubleTime() - t0) * 1000.0);
}

} // namespace qvr::prepare
