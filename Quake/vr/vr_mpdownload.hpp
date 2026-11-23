// vr_mpdownload.hpp -- joining a server whose map is a Map Library package (docs/vr-port/MP_PLAN.md, B4c).
//
// The server names the package its map is played from (QVR_SVC_PACKAGE: the sha256 of the map index's entry, "" for
// the stock game's), before its svc_serverinfo, to every remote client. A client whose mounted package (mapinstall::
// active()) is another one leaves at once and, the next frames:
//   - has it installed: mounts it (mapinstall::activate) and joins again;
//   - lacks it, with vr_mp_download 1 (the default): installs it through the map installer exactly as the Map Library's
//     Install does (its own index's entry for that sha256, the index's mirrors, the size, disk, zip-bomb, path and
//     progs.dat guards, the sha256 check), then mounts it and joins again; a package its index does not list is not
//     installed (the index fetched once first when it has none);
//   - lacks it, with vr_mp_download 0: is told what to install, and stays out.
// The server never sends a URL or a file: nothing is fetched from it or from anywhere the client's own index does not
// name (MP_PLAN.md, Decisions: no chunked fallback).

#pragma once

namespace qvr::mpdownload
{

// QVR_SVC_PACKAGE read (vr_client.cpp): the server's package. A different one than the mounted one ends the connection
// (Host_EndGame: does not return).
void parsePackage();
// QVR_SVC_CAMPAIGN read (vr_client.cpp, mpcampaign): the server's campaign made this game's active one, in place (Quake
// and its mission packs) or, needing the game folders rebuilt, by leaving (Host_EndGame) and joining again; a campaign
// whose data this game lacks: refused, told why (Host_EndGame). The client's own campaign comes back after it left.
void parseCampaign();
// Every frame (vr_main.cpp, after mapinstall::poll): the mount, the install and the joining again.
void poll();

} // namespace qvr::mpdownload
