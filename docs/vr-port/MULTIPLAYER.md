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
- **Over the internet the host forwards UDP port 26000** on his router to his PC. On a LAN nothing is needed.
- Coop or deathmatch, up to 16 players; bots for deathmatch; proximity voice chat.

## Hosting a game

1. **Multiplayer > Host Game**. Set:
   - **Max players**: how many can play, you included (2-16).
   - **Game Type**: Cooperative (the campaign together, against its monsters) or Deathmatch (each other).
   - **Skill** (coop), **Frag Limit** and **Time Limit** (deathmatch), **Teamplay**.
   - The episode and level to start on, then **begin game**.
2. **Let your friends reach you.** On a LAN they find you in their Server Browser. Over the internet:
   - Forward **UDP port 26000** on your router to your PC (your router's "port forwarding" page; the port is
     `-port` / `net_hostport` if you changed it), and allow Quake VR through the Windows firewall when it asks.
   - Give them your public address (search "what is my IP" in a browser), or a dynamic-DNS name.
   - Quake VR has no relay or NAT punch-through yet: without the forwarded port, players outside your network can't
     join you.
3. **Your rules are the game's rules.** The gameplay settings that change the game for everyone (damage, disarming,
   physics, ammo and reload rules and the like) are the host's. Joining players keep their own comfort, hand,
   holster and display settings, and get their own back when they leave.

A **dedicated server** (no player of its own) runs with the game started as `ironwail.exe -dedicated 8 +coop 1 +map e1m1` (or `+deathmatch 1
+map dm4`); the same port rule applies.

## Joining a game

- **Multiplayer > Join Game > Server Browser** lists:
  - your **favourites** (press **Y** on a row to star or unstar it; `vr_mp_favourites`),
  - the servers you **joined last** (the newest 8),
  - the games found on your **LAN**,

  each with its map, players and ping. **A** joins, **X** searches again.
- Or type the host's address in **Join Game** (`host` or `host:port`), or `connect <address>` in the console.
- **A map you don't have**: if the server's map is a Map Library package, it is installed for you before you join
  (through the Map Library's own downloads and checks; `vr_mp_download`, on by default). Other custom maps must be
  copied by hand.
- **Coming back**: in a coop game, if you leave or your game crashes and you join again with the **same name**, you
  get your old slot back with your weapons, ammo, health and armour (as a level change would carry them).

## Coop

- The campaign as in single player, monsters scaled by the host's skill; every player respawns at the level's start
  with his level-start things. Level changes take everyone along.
- The host's game autosaves at each level's start (a coop save: the host can load it and the others join again).
- Players see each other's real heads, hands, fingers, held weapons and bodies (sized by each one's own calibration),
  can hand weapons and ammo to each other, and pick up what the others dropped.

## Deathmatch and bots

- Deathmatch's item rules (weapons and items come back after their wait; dropped weapons go after 10 s) and its maps
  (dm1-dm6, or any deathmatch map).
- **Players can knock each other's weapons out of their hands** (shots, blows, grabs: Armed Grunts > "Players Can
  Disarm Each Other", on by default) and parry each other's swings.
- **Bots** (FrikBot): the host adds one with `impulse 100` in the console (each time one more). Bots run, fight, swing
  and grab with their hands like players do, and count on the scoreboard.

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
- **Can't connect over the internet**: almost always the host's port forwarding (UDP 26000, to the right PC) or a
  firewall. Try from the same LAN first.

## Known issues (beta)

- Little real internet play yet: lag over 150 ms, packet loss and NATs are tested on one PC through a simulated link,
  not across the world. Feedback welcome.
- No NAT punch-through or relay: the host needs a forwarded port (or a VPN such as a virtual LAN between the players).
- A player who comes back to a coop game may have up to one shotgun magazine of shells more than when he left.
- Custom maps that aren't Map Library packages aren't sent to joining players.
- The Multiplayer menus, the Server Browser and voice chat have not yet been checked in a headset by the author.
