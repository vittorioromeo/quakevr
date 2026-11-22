# Multiplayer in Quake VR (beta)

Play Quake's campaigns together in VR (cooperative) or against each other (deathmatch), with your real hands, held
weapons, bodies and voices. Multiplayer is a **beta**: it works and is tested on every build, but it has had little
play over the internet yet. Please report what you find (Options > Create Bug Report).

How it is built (what runs on the server and on each client) is in [MP_DESIGN.md](MP_DESIGN.md); the work log is
[MP_PLAN.md](MP_PLAN.md).

## The short version

- **Everyone needs Quake VR, the same version.** A game of another Quake VR version is refused with the reason;
  plain Quake clients can't join.
- **One player hosts** (Multiplayer > Host Game) and plays in his own game; the others **join** (Multiplayer > Join
  Game: the Server Browser, or the host's address).
- **Over the internet: a room code.** The host's Host Game page shows a 6-letter room code; the others type it at
  Join Game. Most home routers need nothing more; when they do, the host forwards UDP port 26000. On a LAN nothing is
  needed.
- Coop or deathmatch, up to 16 players; bots for deathmatch; proximity voice chat.

## Hosting a game

1. **Multiplayer > Host Game**. Set:
   - **Max players**: how many can play, you included (2-16).
   - **Game Type**: Cooperative (the campaign together, against its monsters) or Deathmatch (each other).
   - **Skill** (coop), **Frag Limit** and **Time Limit** (deathmatch), **Teamplay**.
   - The episode and level to start on, then **begin game**.
2. **Let your friends reach you.** On a LAN they find you in their Server Browser. Over the internet:
   - **The room code** (the easy way): once your game has started, Multiplayer > Host Game shows **Room Code ABCDEF**
     (press **C** there to copy it; the console's `vr_mp_roomcode` prints it too). Send it to your friends: they type
     it at Join Game > **Join by room code**. Quake VR's room service (`vittorioromeo.com`, UDP 26100) only introduces
     your games to each other -- each learns the other's public address and both open their routers to each other at
     the same moment ("hole punching") -- and the game itself then goes directly between you. It works through most
     home routers without any setup. Allow Quake VR through the Windows firewall when it asks.
   - **When a friend is told "no direct link"**: one of your routers gives every destination a different port (a
     "symmetric NAT": some mobile, carrier-grade and business connections), which hole punching can't get through.
     Then forward **UDP port 26000** on your router to your PC (your router's "port forwarding" page; the port is
     `-port` / `net_hostport` if you changed it) and give them your public address (search "what is my IP" in a
     browser) or a dynamic-DNS name; or use a virtual LAN between you.
   - **Is my port reachable?** While you host with the room service on, Host Game shows **Port 26000: reachable**
     (anyone can join, whatever their router), **not reachable from the internet** (forward UDP 26000 for the players
     behind strict routers; room codes still work for most), or **unknown** (the room service is off, too old, or not
     answering). The net HUD (`vr_net_hud 1`) shows it while you host (`port 26000 reachable`) and the console says it
     once (`vr_mp_roomcode` prints it again). How: the room service sends one unasked-for datagram to your public
     address at your game port from another of its ports; your router lets it in only through a forward (or if it
     lets anything in). A router that filters by address only (not by port) can read reachable for the service while
     still filtering strangers: rare.
   - Privacy: while you host, the room service sees your public IP address and your room code, and the joining
     player's; it keeps them only while your game runs (two minutes after it ends they are gone) and logs no
     addresses. `vr_mp_room 0` hosts without a room code (nothing is sent to the service); `vr_mp_rendezvous` names
     another service (`Misc/rendezvous/` in the source has the service and how to run your own).
3. **Your rules are the game's rules.** The gameplay settings that change the game for everyone (damage, disarming,
   physics, ammo and reload rules and the like) are the host's. Joining players keep their own comfort, hand,
   holster and display settings, and get their own back when they leave.

### A dedicated server

A **dedicated server** is a game with no player of its own, no window and no headset: a console that runs the world
for the others. Any PC with Quake VR installed can run one (the same port rule applies):

    ironwail.exe -dedicated 8

- `-dedicated 8`: up to 8 players (2-16). Without a map (from `server.cfg` or `+map e1m1` on the command line) it
  starts Quake's `start` map in deathmatch, as Quake did.
- **`server.cfg`** in the `quakevr` folder is read by itself when there is one (after `autoexec.cfg`; the command
  line's `+commands` come last and win). An example:

      // server.cfg: a Quake VR dedicated server
      hostname "Friday coop"     // its name in the Server Browser
      coop 1                     // cooperative (for deathmatch: coop 0; deathmatch 1; fraglimit 20; timelimit 15)
      deathmatch 0
      skill 1                    // 0 easy .. 3 nightmare
      teamplay 0
      vr_mp_rejoin_keep 1        // a player who comes back by the same name gets what he had (the default)
      vr_sv_status 60            // the console's status line every 60 s (0: off; the default)
      map e1m1                   // the level to start on (last: the settings above apply to it)

- **The console** says every minute (and whenever a player comes or goes) what the server runs and who is in:

      status: e1m1 (coop, skill 1) | 2 of 8 players: A 42 ms, B 85 ms | map 0:12:03, up 1:02:03

  Type `status` for the players' list (frags, time in, ping, address), `kick <name>`, `changelevel <map>`,
  `map <map>` (a new game), `quit`. `vr_sv_status_now` prints the status line at once.
- Nobody can pause a dedicated server (`pausable 0` is its default; a `pausable 1` in `server.cfg` allows it).
- The gameplay settings (damage, disarming, ammo rules...) are the server's own: set them in `server.cfg` as in a
  player's console, or copy the ones from your own game's `ironwail.cfg`.

## Joining a game

- **Multiplayer > Join Game > Server Browser** lists:
  - your **favourites** (press **Y** on a row to star or unstar it; `vr_mp_favourites`),
  - the servers you **joined last** (the newest 8),
  - the games found on your **LAN**,

  each with its map, players and ping. **A** joins, **X** searches again.
- Or type the host's address in **Join Game** (`host` or `host:port`), or `connect <address>` in the console.
- Or his **room code** in Join Game's **Join by room code** (or `connect @ABCDEF` in the console). If the code is
  wrong or his game ended you are told "No room with that code"; if the room service can't be reached, "Room service
  not answering" (join by his address instead).
- **A map you don't have**: if the server's map is a Map Library package, it is installed for you before you join
  (through the Map Library's own downloads and checks; `vr_mp_download`, on by default). Other custom maps must be
  copied by hand.
- **Coming back**: in a coop game, if you leave or your game crashes and you join again with the **same name**, you
  get your old slot back with the weapons, ammo, health and armour you had (keys and powerups are dropped, as a level
  change drops them).

## Coop

- The campaign as in single player, monsters scaled by the host's skill; every player respawns at the level's start
  with his level-start things. Level changes take everyone along.
- The host's game autosaves at each level's start (a coop save: the host can load it and the others join again).
- Players see each other's real heads, hands, fingers, held weapons and bodies (sized by each one's own calibration),
  can hand weapons and ammo to each other, and pick up what the others dropped.
- A hit player's wounds and bleeding show on him for everyone (blood pouring, drops, drips: you hear the pour land),
  and his gun's small things as yours: the auto pump's stroke and clacks, the ammo screen's light, the barrel's heat glow.

## Deathmatch and bots

- Deathmatch's item rules (weapons and items come back after their wait; dropped weapons go after 10 s) and its maps
  (dm1-dm6, or any deathmatch map). A map with no deathmatch starts (a campaign's, Dwell's start from Select Campaign)
  spawns players at coop's starts, the next clear one (it used to stop the game with "runaway loop error").
- **A dead player or bot drops every weapon he had** (deathmatch and coop: both hands and every holster), each as it
  was: a dagger lies as a dagger (not a knight's sword), a gun keeps its magazine, a Dwell or Arcane Dimensions variant
  its kind, a blade its blood.
- **Players can knock each other's weapons out of their hands** (shots, blows, grabs: Armed Grunts > "Players Can
  Disarm Each Other", on by default) and parry each other's swings.
- **Bots** (FrikBot): the host adds one with `impulse 100` in the console (each time one more), or **VR Settings > Play >
  Add Co-op Bot** / **Add Bot (Team 0/1)**. Bots run, fight, swing and grab with their hands like players do, and count
  on the scoreboard. Their hands are a flat-screen player's (the same code places them): they **dual wield** a second gun
  that pairs with their main one (the players' rule) and fire both, swing melee weapons overhead (the blow is the same
  melee sweep a player's is, and you can parry it), and **raise their guard** against a blow they see coming, more often
  and sooner the higher their skill (`skill` when added; `vr_bot_parry_chance` forces a chance, `vr_bot_dual_wield 0`
  turns the second gun off).
- **Bots by skill** (the `skill` they were added at, 0-3): the lower, the more their aim strays and lags behind a moving
  target (`vr_bot_aim_error`, 1: by skill, 0: dead on), the longer they wait before firing at someone they've just seen
  and pause between bursts (`vr_bot_reaction`), the slower they turn and the less far they notice players; their melee
  swing is held raised before it comes down (`vr_bot_swing_windup`, 0.3 s at skill 0, less the higher). They draw any
  gun they carry that has ammo (the revolver, the nail pistol, the mission packs' guns), else their melee weapon. They
  are drawn at a default player's size (1.646 m eyes at world scale 1.2); their hands are posed as a default-size
  flat-screen player's (from their box's eyes, a default player's arms), so their blows reach what a player's do.
- **Co-op bots**: in a co-op game a bot fights the monsters beside you, never the players, follows the players loosely,
  and comes along to the next level with what it had (as a player who left and came back does).
- **A bot's weapon is where you see it**: the server places it in the bot's hand (with the weapon's own offsets) and
  every player draws it there; a grab on it (or a blow, a parry, a shot) is measured on that weapon.
- **How bots find their way**: by FrikBot's waypoints (dropped where players walk, and by roaming), going for items,
  powerups, weapons and health. **`vr_bot_navmesh 1`** (off by default, the host's setting) adds the map's navmesh
  (NAVMESH.md 23): a bot walks it to a player it has seen in the last `vr_bot_navmesh_memory` (5) s within
  `vr_bot_navmesh_range` (1200) units once he is out of its sight (through doors it never used; in sight it fights as
  before), unless it is after a powerup or, under 60 health, health or armour; and to a goal its waypoints have no route
  to (or one much longer than the navmesh's path). In water, and on a waypoint's special ways, it keeps its waypoints.
  Developer 1 prints `botnav:` lines when a bot switches.
- **Debug views on players and bots**: Debug > Show Disarm Volumes outlines each other player's (and bot's) held
  weapon as a grab (orange: the weapon's line and its hotspots, Players Can Disarm Each Other's reach round them) and a
  shot (magenta on the weapon, cyan on the hand) test it; Debug > Show Hit Zones outlines their positional damage zones
  (head red, body green, limbs yellow, legs blue) on their box. The host draws the server's own volumes and zones; a
  player who joined rebuilds them from what he draws (the host's rules, the weapon as drawn, a standing box: a
  crouched player's zones are drawn standing there, and the hotspots are left out). Your own are not drawn.

## The scoreboard on the wrist

- In a multiplayer game (coop or deathmatch, bots included) the wrist gadget has an extra page: the **scoreboard**.
  Press the gadget's **outer button** (the one that turns to Arcane Dimensions' page) to go through the pages: the HUD,
  AD's page (with AD's data loaded), the scoreboard, and back to the HUD. Single player has no scoreboard page.
- It shows the game's own scoreboard's rows in its order (the most frags first): each player's colours, frags and name,
  your row on a lit strip, and `((o))` beside whoever is talking on voice chat (you too while you send). In coop the
  level's kills and secrets are above the players (the game's coop scoreboard). It updates live; up to 11 players at the
  gadget's text size, all 16 in smaller text.
- Ping isn't shown: a NetQuake client doesn't know the other players' pings (the host's `status` lists them).
- Flat-screen play has no wrist gadget: its scoreboard is the game's own (**TAB**, and at death).
- Debug > Reports: **Wrist Gadget Next Page** (`vr_gadget_page 1`) and **Wrist Gadget Scoreboard** (`vr_gadget_scores`,
  the page's rows in the console). Test: `mp_test.py wristscore`.

## Voice chat

- **On by default**, heard at the talking player's head and fading with distance (or everywhere: Radio).
- **Push-to-talk** (the default): the off hand's upper button held at your mouth (with Voice Notes off), or the
  **Push-to-Talk** key (CAPSLOCK). Or **Voice Activation**: sent while it hears your voice.
- Settings: **VR Settings > HUD, Haptics and Sound > Sound > Voice Chat** (your microphone, its gain and noise
  suppression, volume, fade with distance, the talking indicator) and a row per player: **mute**, quieter, louder.
- Your voice goes only to the server you joined and its players; nothing is recorded.

## Single player only

These stay off in multiplayer (the world can't slow down or stop for one player):

- Time scale and bullet time; the fly camera's pause.
- Saving and loading at will (the host's level-start coop autosave is the multiplayer save).
- The wooden crates Quake VR places about a map (crates a mapper placed work).
- Dimension of the Past and Dimension of the Machine (their campaigns run in single player only).

## When something goes wrong

- **The net HUD**: Debug > Reports > Network: Net HUD shows your ping, the packets lost and your movement's
  corrections at the top right (And Bytes: the bytes each way too).
- **"SERVER NOT RESPONDING"**: when the host's game stops answering (it crashed, its link dropped), after 3 seconds the
  top right and your view say so; if it comes back you play on. After 30 seconds you are taken back to the Server
  Browser with the reason (Debug > Reports > Network: Server Timeout, `vr_mp_client_timeout`; 0 waits 5 minutes as Quake did).
- **A slow link**: `vr_net_rate` (default 80000 bytes a second) caps what the server sends you; lower it on a slow or
  metered connection: the nearest and the things ahead of you are kept up to date first.
- **A jittery link** (Wi-Fi, a busy connection): the server holds each player's moves as long as his link's jitter
  needs (up to about 140 ms) and runs them one a tick, so his movement stays even for everyone and lifts meet him where
  his game showed; on a steady link it is about 28 ms at most. The others see a jittery player that much later. (Debug
  > Network: Move Buffer and Move Buffer Most, `vr_net_movebuffer` and `vr_net_movebuffer_max`, on the host.)
- **Can't connect over the internet**: by a room code, the message says why ("No direct link to the host": one of
  your routers doesn't allow hole punching; the host forwards UDP 26000). By address: almost always the host's port
  forwarding (UDP 26000, to the right PC) or a firewall. Try from the same LAN first.
- **Testing with two games on one PC** (`mp_local.ps1`): `vr_nav_follow <name | slot | next | 0>` in one player's console
  (Debug > Network: Follow the Next Player) walks him after the other along the navmesh by his own moves, so one keyboard
  plays both; your own move, look or jump takes over, it walks on a second later; `0` stops it (coop and single player
  servers only; NAVMESH.md 22).

## Known issues (beta)

- Little real internet play yet: lag over 150 ms, packet loss and NATs are tested on one PC through a simulated link,
  not across the world. Feedback welcome.
- Room codes are new: tested through simulated routers on one PC, not yet across real ones. No relay: behind a
  symmetric NAT the host still needs a forwarded port (or a virtual LAN between the players).
- Custom maps that aren't Map Library packages aren't sent to joining players.
- The Multiplayer menus, the Server Browser and voice chat have not yet been checked in a headset by the author.
