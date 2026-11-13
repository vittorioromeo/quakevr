// vr_gadgetlock.cpp -- the wrist gadget locked while both hands hold one thing (vr_gadgetlock.hpp).

#include "vr_gadgetlock.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_foegrab.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_twohand.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace qvr::gadgetlock
{
namespace
{

struct State
{
    int frame = -1;               // host_framecount it was last worked out on
    bool locked = false;          // ... and the answer
    double heldAt = -1e9;         // realtime both hands last held one thing
    const char* what = nullptr;    // what (bothHold) on that frame
    const char* printed = nullptr; // ... on the frame before (vr_debug_*: changes printed only)
    bool printedLocked = false;    // ... and whether it was locked then
};
State state;

[[nodiscard]] bool debugging()
{
    return vr_debug_bullettime.value != 0.f || vr_debug_gadget_button.value != 0.f;
}

} // namespace

const char* bothHold()
{
    using namespace protocol;
    for(int h = 0; h < 2; h++)
    {
        // (The other hand on a foregrip or another hotspot, a free grip, a blade grip, a cup: as it holds now, not eased
        // out as bullet time's twoHanded is; the grace is the lock's own.)
        if(twohand::helping(h))
        {
            return "one weapon two-handed";
        }
    }
    const int carried = cl.stats[STAT_QVR_CARRYMAIN];
    if(carried > 0 && carried == cl.stats[STAT_QVR_CARRYOFF])
    {
        return "the same prop";
    }
    const int bladeMain = cl.stats[STAT_QVR_EMBEDMAIN] / 4; // (the blade's entity * 4 + reversed * 2 + its handle flag: vr_axestick.cpp calcStats)
    if(bladeMain > 0 && bladeMain == cl.stats[STAT_QVR_EMBEDOFF] / 4)
    {
        return "the same embedded blade";
    }
    // An enemy or a ragdoll: which one, from the listen server.
    edict_t* player = progs::localPlayer();
    if(!player || !sv.qcvm.edicts)
    {
        return nullptr;
    }
    // (The server's VM before foegrab::heldBy too: its NUM_FOR_EDICT reads the current VM, none in the view frame;
    // judo_test crashed there holding a grunt with both hands, release 1.1 regression run.)
    progs::ServerVm serverVm;
    if(foegrab::holding(0) && foegrab::holding(1))
    {
        const int foe = foegrab::heldBy(player, 1);
        if(foe > 0 && foe == foegrab::heldBy(player, 0))
        {
            return "the same enemy";
        }
    }
    const int num = NUM_FOR_EDICT(player);
    box3d::RagdollHold held[2];
    if(box3d::ragdollHold(num, 0, held[0]) && box3d::ragdollHold(num, 1, held[1]) && held[0].num > 0 &&
        held[0].num == held[1].num)
    {
        return "the same body";
    }
    return nullptr;
}

bool locked()
{
    if(state.frame == host_framecount)
    {
        return state.locked;
    }
    state.frame = host_framecount;
    const bool on = cls.state == ca_connected && cls.signon == SIGNONS;
    state.what = on ? bothHold() : nullptr;
    if(state.what)
    {
        state.heldAt = realtime;
    }
    const double grace = static_cast<double>(za::max(0.f, vr_gadget_lock_grace.value));
    state.locked = on && (state.what || realtime - state.heldAt < grace);
    if(debugging())
    {
        if(state.what && state.what != state.printed)
        {
            Con_Printf("gadget lock: on (both hands hold %s)\n", state.what);
        }
        else if(!state.what && state.printed && state.locked)
        {
            Con_Printf("gadget lock: let go, %.2f s grace\n", grace);
        }
        else if(!state.locked && state.printedLocked)
        {
            Con_Printf("gadget lock: off\n");
        }
    }
    state.printed = state.what;
    state.printedLocked = state.locked;
    return state.locked;
}

} // namespace qvr::gadgetlock
