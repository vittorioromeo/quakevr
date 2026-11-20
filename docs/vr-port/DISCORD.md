# Discord Rich Presence

The player's Discord profile shows what they play: the campaign, the map, the mode, VR or flat screen, and the time on
this map. Off by default. Code: `Quake/vr/vr_discord.cpp` (its head comment has the protocol and the threads).

## What is shared

Only with the Discord app running on the same PC (its local pipe; nothing goes to the internet from the game), and only
with **Menu Settings > Discord Status** on (`vr_discord 1`) and an application id set (`vr_discord_appid`):

| Discord's line | What it says |
|---|---|
| (the application's name) | the name given to the Discord application, e.g. "Quake VR" |
| details | the map's title (its worldspawn message, e.g. "the Slipgate Complex"), else its file name; "In the menus" |
| state | campaign · mode · VR or Flat screen: "Quake · Single player · VR", "Arcane Dimensions · Co-op · Flat screen", "Quake · Survival Arena wave 3 · VR", "Watching a demo", "Deathmatch" |
| elapsed | since this map began (kept when the same map reloads after a death) |
| large image | `vr_discord_image`'s asset, if set; its hover text: the map's file name |

The campaign is the campaign table's title (`VR_CampaignTitle`: Quake, Scourge of Armagon, Dissolution of Eternity,
Dimension of the Past, Arcane Dimensions, ...); a map from the Map Library shows "Quake". The wave is known only
to the host (a co-op client sees "Survival Arena"). Nothing about the player (names, scores, chat, addresses) is sent.

## Setting it up (the author, once)

1. https://discord.com/developers/applications > New Application, named as Discord should show the game ("Quake VR").
2. Its **Application ID** (General Information) goes in `vr_discord_appid` (it becomes the shipped default once given:
   an id is public, not a secret).
3. Optional: Rich Presence > Art Assets: upload a 1024x1024 logo, its key (e.g. `logo`) in `vr_discord_image`.
4. `vr_discord 1`, the Discord desktop app running (Settings > Activity Privacy > "Share your detected activities"
   on). `vr_discord_status` (Debug > Reports: "Discord: Status") says the connection, tries, the last error
   (an unknown id: Discord's "Invalid Client ID") and the activity last handed over.

## Behaviour

- The main thread reads a small key each frame (mode, wave, VR, campaign, map; no allocation) and hands the activity
  over only when it changed, no sooner than 15 s after the last (Discord allows 5 updates in 20 s; the changes in
  between are merged, the newest goes). Turned off: at once (cleared).
- A thread of its own talks to Discord: connects (pipes 0 to 9), sends, reads Discord's answers (PING answered,
  CLOSE and errors kept for the status), reconnects with a back-off of 2 s doubling to 60 s when Discord is not
  running or restarted (the activity sent again). Every read and write waits 1 s at most: the game never waits on it,
  and quit waits at most that long for the clear.
- Quit: the activity cleared, the thread joined (`VR_Shutdown`). The kit's test runs (`QVR_TEST_BACKGROUND`) never
  reach the real Discord.
- Linux and macOS: the same code over the `discord-ipc-N` unix socket (`XDG_RUNTIME_DIR`, `TMPDIR`, `TMP`, `TEMP` or
  `/tmp`, also Flatpak's `app/com.discordapp.Discord` and Snap's `snap.discord` under it). Not yet tried there.

## Protocol notes

Frames on the pipe: opcode (uint32 LE), length (uint32 LE), JSON. 0 HANDSHAKE `{"v":1,"client_id":"<id>"}`, answered
by a FRAME with `"evt":"READY"`; 1 FRAME: `{"cmd":"SET_ACTIVITY","args":{"pid":<pid>,"activity":{...}},"nonce":"<n>"}`
(no `activity`: cleared); 2 CLOSE (Discord refusing: its `message`); 3 PING, answered by 4 PONG with its payload.

## The join button (later)

Multiplayer room codes would add the activity's `party` (`id`, `size: [n, max]`) and `secrets.join` (the room code),
and a `SUBSCRIBE` to `ACTIVITY_JOIN`, whose dispatch carries the secret to join with. The places are marked `JOIN-HOOK`
in vr_discord.cpp.

## Test

`vr_discord_test` (Debug > Reports: "Discord: Self-Test"; Windows): a fake Discord on a pipe of the test's own
(`qvr-discord-test-<pid>-0`, never the real one) records the frames: no Discord (the map plays, nothing connects), the
handshake (version, application id), single player in VR (title, campaign, mode, start time, pid), nothing re-sent
while nothing changes for longer than the gap, flat screen (no sooner than the gap), co-op (e1m2), the Survival Arena
with its wave (vrarena), Discord restarted (a new handshake, the activity again), turned off (cleared, closed).
`run.sh <agent> -Script "vr_discord_test" -Filter vr_discord_test -QuitOn "vr_discord_test: [0-9]+ passed"`.
