// vr_prepare.cpp -- the installer's preparation run (vr_prepare.hpp).

#include "vr_prepare.hpp"

#include "vr_engine.hpp"
#include "vr_relight.hpp"

#include "Zancle/Base/GetArraySize.hpp"

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
    int frames{0};         // frames since the relight's command
    double nextProgress{0.0};
    int failedMaps{0};
};
Run run;

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
                if(run.relight)
                {
                    Cbuf_AddText("disconnect\n"); // (no map in play: the batch reloads none)
                    run.step = Step::Relight;
                }
                else
                {
                    finish();
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

} // namespace qvr::prepare
