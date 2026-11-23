// vr_mpdownload.cpp -- joining a server whose map is a Map Library package (vr_mpdownload.hpp; MP_PLAN.md, B4c).
//
// The server's side is VR_MpWritePackage (SV_SendServerinfo, to every remote client before its svc_serverinfo); the
// client's, parsePackage (the message) and poll (the steps after it left). Mounting a package rebuilds the game folders
// and ends any connection (COM_ReloadVRGame), so the client leaves first and joins again at the address it connected to
// (VR_MpNoteConnect: CL_EstablishConnection's).
// (mpcampaign) The server's campaign comes first (QVR_SVC_CAMPAIGN, parseCampaign): taken in place among Quake and its
// mission packs, else the same way (leave, switch, join again); refused when this game lacks its data. The client's own
// campaign comes back once it is disconnected (poll; vr_gamedir.cpp VR_JoinCampaign, VR_LeaveJoinedCampaign).

#include "vr_mpdownload.hpp"

#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_mapindex.hpp"
#include "vr_mapinstall.hpp"
#include "vr_protocol.hpp"
#include "vr_serverbrowser.hpp"

#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

namespace qvr::mpdownload
{

namespace
{

// What the client does after it left a server for its package.
enum class Step
{
    Idle,    // nothing
    Mount,   // the package to mount, installed first if it is not (the next frame)
    Index,   // waiting for the map index the client fetched (it had none listing the package)
    Install, // the map installer's job for it is running
    Campaign, // the server's campaign to make the active one (its game folders rebuilt), then join again (mpcampaign)
};

struct Pending
{
    Step step{Step::Idle};
    int campaign{-1};      // Campaign: the server's
    za::String sha;        // the server's package ("" : the stock game's)
    za::String address;    // where to join again
    za::U32 generation{0}; // Index: the map index's generation() when the fetch was asked
    double since{0.0};     // Index: when (realtime)
    bool fetched{false};   // the index was fetched once for this package already
};
Pending pending;

// The last `connect`'s address (CL_EstablishConnection's host; never "local").
za::String lastAddress;
// The package the client last left a server for and mounted (one rejoin per package: a server that names another one
// at once again is not followed round in a loop).
za::String rejoinedFor;
// (mpfix A3) The automatic installs since the last `connect` the player started (at most one: a server that names
// package after package, each installed and joined again with no player action, filled the disk from the Map
// Library's thousands), and whether the next connect is joinAgain's (not the player's). A package already installed,
// or the stock game, is still mounted and joined again (rejoinedFor: not in a loop).
int autoInstalls = 0;
bool joiningAgain = false;
// (mpcampaign) The campaign the client last left a server for and switched to (rejoinedFor's: not followed round in a
// loop).
int campaignRejoinedFor = -1;

// Index: the fetch's answer waited for this long at most (a real network fetch: the wall clock).
constexpr double indexWaitSeconds = 120.0;

[[nodiscard]] za::String titleOf(const za::String& sha)
{
    if(sha.empty())
    {
        return za::String{"the stock game"};
    }
    const mapindex::Entry* e = mapindex::find(sha);
    return e ? za::String{mapindex::index().field(e->title)}
             : za::String{za::StringView{sha}.substrByPosLen(0, za::min(sha.size(), za::SizeT{16}))};
}

void stop()
{
    pending = Pending{};
}

// `tag` and `done`: the console line's ("mp_package", "<title> mounted").
void joinAgain(const char* tag, const char* done)
{
    if(pending.address.empty())
    {
        Con_Printf("%s: %s; connect to the server again.\n", tag, done);
    }
    else
    {
        Con_Printf("%s: %s; joining again (%s)\n", tag, done, pending.address.cStr());
        joiningAgain = true;
        Cbuf_AddText((za::String{"connect "} + pending.address + "\n").cStr());
    }
    stop();
}

void mount()
{
    if(!mapinstall::activate(pending.sha))
    {
        Con_Printf("mp_package: %s could not be mounted (see above): not joining.\n", titleOf(pending.sha).cStr());
        stop();
        return;
    }
    rejoinedFor = pending.sha;
    joinAgain("mp_package", (titleOf(pending.sha) + " mounted").cStr());
}

// Campaign: the server's campaign the active one (the selector's switch, the game folders rebuilt), then joined again.
void switchCampaign()
{
    const int c = pending.campaign;
    if(!VR_JoinCampaign(c, 1))
    {
        Con_Printf("mp_campaign: %s could not be made this game's campaign (see above): not joining.\n",
            VR_CampaignTitle(c));
        stop();
        return;
    }
    campaignRejoinedFor = c;
    joinAgain("mp_campaign", va("%s is this game's campaign now", VR_CampaignTitle(c)));
}

// Mount: installed (or the stock game's) -> mounted and joined again; else its install begun, or why not.
void mountOrInstall()
{
    if(pending.sha.empty() || mapinstall::installed(pending.sha))
    {
        mount();
        return;
    }
    const mapindex::Entry* e = mapindex::find(pending.sha);
    if(!e)
    {
        if(!pending.fetched)
        {
            // No index lists it (none yet, or an old one): Quaddicted's and Slipseer's fetched once, as maps_fetch does.
            Con_Printf("mp_package: the server's map package (%.16s) is not in this game's map index: fetching it...\n",
                pending.sha.cStr());
            pending.fetched = true;
            pending.generation = mapindex::generation();
            pending.since = realtime;
            pending.step = Step::Index;
            mapindex::start(true);
            return;
        }
        Con_Printf("mp_package: the server's map package (sha256 %s) is not in the Map Library's index: it cannot be "
                   "installed, so this game cannot join.\n",
            pending.sha.cStr());
        stop();
        return;
    }
    const za::String title{mapindex::index().field(e->title)};
    if(!vr_mp_download.value)
    {
        Con_Printf("mp_package: the server plays %s, a Map Library package this game lacks (vr_mp_download 0: not "
                   "installed). Install it from the Map Library, then join again.\n",
            title.cStr());
        stop();
        return;
    }
    if(autoInstalls > 0)
    {
        Con_Printf("mp_package: the server plays %s, another package after the one this game installed for it: not "
                   "installed (one automatic install a join). Install it from the Map Library, then join again.\n",
            title.cStr());
        stop();
        return;
    }
    za::String why;
    if(!mapinstall::begin(e, true, &why))
    {
        Con_Printf("mp_package: %s cannot be installed now: %s\n", title.cStr(), why.cStr());
        stop();
        return;
    }
    Con_Printf("mp_package: the server plays %s: installing it from the Map Library, then joining again.\n",
        title.cStr());
    autoInstalls++;
    pending.step = Step::Install;
}

} // namespace

void parsePackage()
{
    const za::String sha{MSG_ReadString()};
    if(sv.active)
    {
        return; // hosting: the server's own files (never sent to the host's own client)
    }
    if(sha.size() && !mapindex::validSha(za::StringView{sha}))
    {
        Con_Printf("mp_package: the server named no valid package (\"%.80s\"): ignored.\n", sha.cStr());
        return;
    }
    if(sha == mapinstall::active())
    {
        rejoinedFor = za::String{};
        return;
    }
    if(rejoinedFor.size() && rejoinedFor == sha)
    {
        Host_EndGame("mp_package: %s was mounted, and the server named it again as another", titleOf(sha).cStr());
    }
    const bool have = sha.empty() || mapinstall::installed(sha);
    Con_Printf("mp_package: the server plays %s (%s): %s\n", titleOf(sha).cStr(),
        sha.empty() ? "no map package" : "a map package",
        have ? (sha.empty() ? "unmounting this game's, then joining again" : "installed; mounting it, then joining again")
             : "not installed here");
    pending = Pending{};
    pending.step = Step::Mount;
    pending.sha = sha;
    pending.address = lastAddress;
    // Leave now (the serverinfo after this message would load the map from the wrong folders); poll() goes on.
    Host_EndGame("the server's map package differs from this game's mounted one");
}

void parseCampaign()
{
    const za::String folder{MSG_ReadString()};
    if(sv.active || folder.empty() || !VR_QuakeVRMounted())
    {
        return; // hosting (the server's own campaign), or a game outside Quake VR's campaigns
    }
    const int c = VR_CampaignIndexOf(folder.cStr());
    if(c >= 0 && c == VR_ActiveCampaign())
    {
        campaignRejoinedFor = -1;
        return;
    }
    const char* missing = VR_CampaignMissingForJoin(c);
    const char* title = c >= 0 ? VR_CampaignTitle(c) : folder.cStr();
    if(missing[0])
    {
        Con_Printf("mp_campaign: the server plays %s (%s): this game can't join: %s.\n", title, folder.cStr(), missing);
        SCR_CenterPrint(va("Can't join: the server plays\n%s,\nwhich isn't installed here\n(see the console)", title));
        Host_EndGame("the server's campaign is missing here");
    }
    if(VR_JoinCampaign(c, 0))
    {
        Con_Printf("mp_campaign: the server plays %s: this game's campaign while joined\n", title);
        return; // (in place: the svc_serverinfo after this message loads the map from its folders)
    }
    if(cls.demoplayback)
    {
        Con_Printf("mp_campaign: the demo was recorded in %s: not switched (choose it in Official Campaigns)\n", title);
        return;
    }
    if(campaignRejoinedFor == c)
    {
        Host_EndGame("mp_campaign: %s was made this game's campaign, and the server named it again as another", title);
    }
    Con_Printf("mp_campaign: the server plays %s: switching this game to it, then joining again\n", title);
    pending = Pending{};
    pending.step = Step::Campaign;
    pending.campaign = c;
    pending.address = lastAddress;
    // Leave now (the serverinfo after this message would load the map from the wrong folders); poll() goes on.
    Host_EndGame("the server's campaign differs from this game's");
}

void poll()
{
    // (mpcampaign) Left the server whose campaign this game took: its own back (a game started here meanwhile: kept).
    if(VR_JoinedCampaignHeld() && pending.step == Step::Idle && !joiningAgain)
    {
        if(sv.active)
        {
            VR_ForgetJoinedCampaign();
        }
        else if(cls.state == ca_disconnected)
        {
            const int own = VR_LeaveJoinedCampaign();
            if(own >= 0)
            {
                Con_Printf("mp_campaign: left the server: %s is this game's campaign again\n", VR_CampaignTitle(own));
            }
        }
    }
    switch(pending.step)
    {
        case Step::Idle: return;
        case Step::Campaign: switchCampaign(); return;
        case Step::Mount: mountOrInstall(); return;
        case Step::Index:
            if(mapindex::generation() != pending.generation)
            {
                mountOrInstall(); // (once: `fetched` is set)
            }
            else if(realtime - pending.since > indexWaitSeconds)
            {
                Con_Printf("mp_package: the map index did not arrive (%s): not joining.\n", mapindex::state().cStr());
                stop();
            }
            return;
        case Step::Install:
        {
            if(cls.state == ca_connected)
            {
                stop(); // joined somewhere else meanwhile: the install goes on, no joining again
                return;
            }
            const mapinstall::Job& j = mapinstall::job();
            if(mapinstall::running() || j.sha != pending.sha)
            {
                return;
            }
            if(j.phase == mapinstall::Phase::Done)
            {
                mount();
            }
            else
            {
                Con_Printf("mp_package: %s was not installed (%s): not joining.\n", titleOf(pending.sha).cStr(),
                    j.message.cStr());
                stop();
            }
            return;
        }
    }
}

} // namespace qvr::mpdownload

using namespace qvr;

extern "C" void VR_MpNoteConnect(const char* host)
{
    if(host && *host && q_strcasecmp(host, "local"))
    {
        if(!mpdownload::joiningAgain)
        {
            mpdownload::autoInstalls = 0; // (mpfix A3) a connect the player started
        }
        mpdownload::joiningAgain = false;
        mpdownload::lastAddress = za::String{host};
        browser::noteConnected(host); // (B13) first in the server browser's recent list
    }
}

extern "C" void VR_MpWritePackage(struct sizebuf_s* msg)
{
    if(!(sv.protocolflags & PRFL_QUAKEVR))
    {
        return;
    }
    // (mpcampaign) the campaign first: a client switching to it may mount the package after
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_CAMPAIGN);
    MSG_WriteString(msg, VR_QuakeVRMounted() ? VR_CampaignFolder(VR_ActiveCampaign()) : "");
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_PACKAGE);
    MSG_WriteString(msg, mapinstall::active().cStr());
}
