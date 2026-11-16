// vr_mpdownload.cpp -- joining a server whose map is a Map Library package (vr_mpdownload.hpp; MP_PLAN.md, B4c).
//
// The server's side is VR_MpWritePackage (SV_SendServerinfo, to every remote client before its svc_serverinfo); the
// client's, parsePackage (the message) and poll (the steps after it left). Mounting a package rebuilds the game folders
// and ends any connection (COM_ReloadVRGame), so the client leaves first and joins again at the address it connected to
// (VR_MpNoteConnect: CL_EstablishConnection's).

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
};

struct Pending
{
    Step step{Step::Idle};
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

void joinAgain()
{
    const za::String title = titleOf(pending.sha);
    if(pending.address.empty())
    {
        Con_Printf("mp_package: %s mounted; connect to the server again.\n", title.cStr());
    }
    else
    {
        Con_Printf("mp_package: %s mounted; joining again (%s)\n", title.cStr(), pending.address.cStr());
        Cbuf_AddText((za::String{"connect "} + pending.address + "\n").cStr());
    }
    rejoinedFor = pending.sha;
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
    joinAgain();
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
    za::String why;
    if(!mapinstall::begin(e, true, &why))
    {
        Con_Printf("mp_package: %s cannot be installed now: %s\n", title.cStr(), why.cStr());
        stop();
        return;
    }
    Con_Printf("mp_package: the server plays %s: installing it from the Map Library, then joining again.\n",
        title.cStr());
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

void poll()
{
    switch(pending.step)
    {
        case Step::Idle: return;
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
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_PACKAGE);
    MSG_WriteString(msg, mapinstall::active().cStr());
}
