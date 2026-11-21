# Quake VR: UNLEASHED 1.1.1: patch notes

**Version 1.1.1**, compared with 1.1.0 (whose notes, PATCH_NOTES_1.1.md, are final). This file grows with every change
that ships in 1.1.1.

## Campaigns

- **Dwell (experimental).** Dwell's two episodes, and custom maps made for Copper (such as Underdark Overbright), now
  play, by Copper's rules: Select Campaign > Dwell, and the Map Library's Copper maps. The installer offers Dwell
  (free, by Fairweather and the Dwell team), ticked by default. Dwell's own monsters (the Kamikaze, the Brute, the
  Construct and both bosses) fight in its maps, and, with Dwell installed, in any map and from the toolgun; its
  weapons and powerups work as variants of ours. Dwell's Rotary Shotgun, Crystal Lance and Rifle look like themselves
  in your hands, holsters and on armed grunts (made from Dwell's own models, with a grip to hold), the Rotary's four
  barrels turning as you fire; each shows its ammo screen in a riveted bezel on its back, as our guns do; the holster menu (flat and on the wrist) names them, and AD's Widowmaker, Shadow Axe
  and Plasma Gun too. Dwell's weapons
  now stay what they are in your holsters, through saves, level changes and coop rejoins; the Rotary Shotgun reloads
  and spins up as the Widowmaker; Haste, Berserk and the Wetsuit show their seconds left on the wrist (and the flat
  visor), and Haste and Berserk tint the screen in Dwell's colours. An armed grunt that picks up one of Dwell's
  weapons fires it as Dwell's. Fixed: the Crystal Lance's first shot ended the game with a network error.
- **Quoth (experimental).** Quoth 2.2's own maps (Select Campaign > Quoth) and custom maps made for Quoth play by
  Arcane Dimensions' rules, so they need AD. The installer offers Quoth (free, by Kell, Necros and Preach), downloaded
  from Quaddicted and installed as released: ticked when Arcane Dimensions is chosen, greyed out without it.

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
  calibration, moving smoothly between the server's updates. A sliding player is seen sliding (legs out in front, leant
  back), not crouched; a dead player's body is his own body (his build, skin tone, gloves, hands), not Quake's ranger,
  for everyone including himself.
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
- **A flat-screen player aiming down the sights looks the part** to others (and in the third-person camera): his head
  at his eye behind the rear sight, the cheek on the stock, the chest following the gun up and down.

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
- **Climbing: choose when you pull up onto a ledge.** Advanced VR Options > Movement > Climbing > Mantle When
  (`vr_climb_mantle_trigger`): High Enough (the default, as before: at once), Let Go (hang and look over the top, pull
  up when your last hand lets go; let go lower and you drop), Jump, or Let Go or Jump. Flatscreen mantles and ladders
  are unchanged.
- **Sprint by clicking the stick.** VR Settings > Locomotion and Controls > Speed Button (`vr_sprint_toggle`): Toggle
  makes a click of the speed button (the left stick's click as shipped, or Shift) switch between walking and running
  instead of holding it. The switch ends by itself, back to your Default Speed, when you stop moving for a moment, die,
  change level or open the menu. Hold (the default) is as before.
- **Run by double-tapping the stick.** VR Settings > Locomotion and Controls > Double-Tap Stick to Run
  (`vr_move_doubletap`, off by default): push the moving stick twice quickly the same way (push, let go, push again) to
  run until you let the stick go back to the middle. It works beside the speed button: either one makes you run. How
  quick the taps must be: Advanced VR Options > Movement > Locomotion > Double-Tap Window (`vr_move_doubletap_window`,
  0.3 s).
- **Ammo pouch position sliders.** VR Settings > Holster Calibration now has Ammo Pouch Forward, Right and Up beside the
  holsters' rows (the standing pouch; they agree with Holster Setup's drag and drop), and Reset Holsters resets the pouch
  too.

## Menus

- **Search filter (development builds).** The Search page has a "Show: Both / Cvars / Tunables" row over its text
  box: the menus' settings alone, the Tunables pages' constants alone, or both (the default). The results update
  at once; the choice lasts until the game quits. Release builds have no tunables, so they show no row and search
  as before.
- **No attract-mode demos.** Quake's demos no longer play by themselves at start-up or from a mod's start-up
  script, on the PC or the Quest, in VR or flat: flat goes straight to the main menu (VR starts in the hub as
  before). Options > Interface's "Start Demos" row is gone. `playdemo` and `timedemo` still play a demo.
- **Discord status.** With the Discord app running, your Discord profile shows what you play: the campaign, the
  map, single player, co-op, deathmatch or the Survival Arena's wave, VR or flat screen, and the time on the map. On
  by default; turn it off at Menu Settings > Discord Status. It talks only to the Discord app on your PC, never holds
  up the game (without Discord it quietly tries again now and then), and is not on the Quest.
- **Multiplayer menu.** Join Game and Host Game go straight to their page: Quake's "IPX / TCP/IP" choice is gone (IPX
  has been dead for decades, and "TCP/IP" always meant UDP). The "multiplayer is untested" note is gone too.
- **`-window` makes a normal window** with its title bar, to drag and resize, even with Borderless Window on (the
  setting is kept for the next launch without it). **`-windowpos <x> <y>`** places it (its inside's top-left, in
  desktop pixels) instead of centring it.


## Performance

- **Crowded maps run lighter.** Armed grunts you can't see (behind walls, or far off and out of view) and that
  haven't noticed you cost far less to run; Dwell's biggest map (d2m8, 250 armed grunts) about half. They wake,
  aim and fight exactly as before the moment you see them, hit them or they notice you.
- **Lighter melee in a crowd of armed enemies.** Swinging at several armed grunts up close now checks whether each
  blow strikes their weapons far more cheaply (about a tenth less game-code work with six around you); what a blow
  strikes is exactly as before.
