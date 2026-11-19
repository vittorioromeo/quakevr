# Quake VR: UNLEASHED 1.1.1: patch notes

**Version 1.1.1**, compared with 1.1.0 (whose notes, PATCH_NOTES_1.1.md, are final). This file grows with every change
that ships in 1.1.1.

## Graphics

- **Relighting about twice as fast.** Quake VR now ships its own build of ericw-tools' `light` (2.0.0-alpha11-qvr1)
  for the relighting (Graphics > Relighting, the installer's relight of the official maps, Download ericw-tools). With
  Bounced Light on (the default) it traces the bounced light from fewer samples and shares it between neighbours:
  e1m1 relights in 4 s instead of 8, e3m5 in 8.5 s instead of 21, Arcane Dimensions' ad_mountain in 50 s instead of
  95 to 113, the installer's relight of Quake's and the mission packs' maps in 6 minutes instead of 9, and the look is the same (a change of under one step in 255 on average). Maps you relit before stay as
  they are and are not relit again. Its source, with the changes and how to build it, is offered beside every release
  (GPL-3). The setting `vr_relight_bounceextra` (2; 0 = as before) is for comparisons.
- **Bloom without black or coloured blocks.** One extremely bright pixel can no longer spread a block of black or
  colour through the bloom.
- **Safe mode only after a real failed start.** The automatic GL safe mode (optional graphics features off for a
  start) now turns on only when the last start crashed, or died within seconds of building its shaders. Closing the
  game while it compiles its shaders on the first start, or after a slow compile, no longer puts the next start in
  safe mode; the start-up log (`quakevr/crash/gl_startup.log`) and the console say why either way.

## Multiplayer beta

Quake VR's multiplayer, rebuilt for VR: play the campaigns together or against each other with your real hands,
weapons, bodies and voices. It is a **beta**: tested on every build, but with little play over the internet yet.
The guide: [MULTIPLAYER.md](MULTIPLAYER.md).

- **Coop and deathmatch, up to 16 players**, from Multiplayer > Host Game; everyone needs this version of Quake VR.
- **Join by room code**: the host's Host Game page shows a 6-letter room code (C copies it); the others type it at
  Join Game > Join by room code (or `connect @CODE`). Quake VR's room service only introduces the two games (UDP hole
  punching): no port forwarding through most home routers, and the game goes directly between the players. Behind a
  router that doesn't allow it (a symmetric NAT) the joiner is told so, and the host forwards UDP port 26000 as before.
  The service sees the players' IP addresses only while the room lives and logs none; `vr_mp_room 0` hosts without a
  code, `vr_mp_rendezvous` names another service.
- **One server socket**: the host talks to every player from his one port (26000), so a forwarded port works behind
  routers that change port numbers too (Quake gave each player a port of his own).
- **You see each other as you are**: heads, hands, fingers, held weapons and bodies sized by each player's own
  calibration, moving smoothly between the server's updates.
- **Everything you can do alone works together**: grabbing, throwing, melee and parries, climbing and mantling, the
  grapple, holsters and pouches, reloads, force grab; hand weapons and ammo to each other, knock weapons out of other
  players' hands in deathmatch.
- **Your own movement predicted**: walking, jumping, swimming, climbing and the grapple respond at once, with the server
  correcting them smoothly; hits are judged where you saw the others when you fired (lag compensation). The server
  runs your moves one a tick even when your packets arrive bunched or out of order (on a jittery link it holds them a
  little longer, as long as your link's jitter needs), so lifts and doors meet you where your game showed.
- **Deathmatch with bots** (FrikBot: `impulse 100`) that fight, swing and grab with their hands like players.
- **Bots with a flat-screen player's hands**: they dual wield, swing melee weapons overhead and parry blows by their
  skill; **co-op bots** (VR Settings > Play > Add Co-op Bot) fight the monsters beside you and follow you to the next level.
- **Proximity voice chat** (Opus, with RNNoise noise suppression): push-to-talk or voice activation, heard at the
  talker's head; per-player mute and volume (VR Settings > HUD, Haptics and Sound > Sound > Voice Chat).
- **Server Browser** (Multiplayer > Join Game): favourites, the servers you joined last and the LAN's, with their
  maps, players and pings.
- **Map Library maps installed on joining**: a server's map package you lack is installed before you join.
- **Coming and going**: friends can join a coop game in progress; a player who leaves or crashes and comes back by the
  same name gets his slot and his things back.
- **A vanished server**: after 3 seconds of silence your view says "server not responding"; after 30 seconds
  (`vr_mp_client_timeout`) you are back at the Server Browser with the reason, not frozen for 5 minutes.
- **Your link**: a net HUD (ping, loss, corrections; Debug > Reports > Network: Net HUD) and a rate cap
  (`vr_net_rate`) that keeps what is near and ahead of you up to date first on a slow connection.
- The host's gameplay rules govern the game; each player keeps his own comfort, hand and display settings.
- **Dedicated servers** (`ironwail.exe -dedicated 8`): `server.cfg` read at start, a console status line with the
  players' pings every minute (`vr_sv_status`), pings in `status`, nobody can pause them.
- **Hardened against broken packets**: a malformed message from a player drops only him; a malformed one from a
  server ends that game with the reason instead of closing Quake VR (also a damaged demo in single player).

**Single player only**: time scale and bullet time, saving at will (the host's level-start coop autosave instead),
the crates Quake VR places about maps, Dimension of the Past and Dimension of the Machine.

**Known issues**: little real internet play yet (latency, loss and routers tested through simulated links); no relay
for the routers hole punching can't get through (the host forwards his port, or a virtual LAN); custom maps that
aren't Map Library packages aren't sent to joining players.

## Movement

- **Sliding in VR: the desktop screen's slide, view and body.** While you slide in VR (Sliding (VR, Experimental)),
  your view now moves quickly but smoothly down to the desktop screen's sliding eye height and back up when the slide
  ends, on top of your own head's movement; your drawn body takes exactly the desktop screen's slide pose, its legs out
  on the floor. VR Settings > Locomotion: Slide View (`vr_slide_camera`, on; off keeps the view at your real head) and
  Slide View Time (`vr_slide_camera_time`, 0.15 s). The comfort vignette counts the view's move as movement.

## Menus

- **Search filter (development builds).** The Search page has a "Show: Both / Cvars / Tunables" row over its text
  box: the menus' settings alone, the Tunables pages' constants alone, or both (the default). The results update
  at once; the choice lasts until the game quits. Release builds have no tunables, so they show no row and search
  as before.
