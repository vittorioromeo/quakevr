**Quake VR: UNLEASHED 1.2.0 beta 1**: a preview of the next update. This is a **beta**: expect rough edges. Multiplayer is back, rebuilt for VR: coop and deathmatch for up to 16 players with your real hands, weapons and bodies, room codes instead of port forwarding, proximity voice chat and bots that fight with their hands. Dwell and Quoth join the campaigns, and there is a long list of comfort, movement and performance improvements. Please report bugs: Options > Create Bug Report makes a zip you can attach.

**The full patch notes** (every change since 1.1.0, by area): [PATCH_NOTES_1.1.1.md](https://github.com/vittorioromeo/quakevr/blob/next/docs/vr-port/PATCH_NOTES_1.1.1.md) (they become the 1.2 notes at the final release).

## Pre-release: please test

This is a **pre-release** for testers, not the final 1.2.0: the installer and the game's update notice keep offering 1.1.0 until 1.2 is out. Please try multiplayer first (host a game with a friend by room code, or add a co-op bot), then Dwell and Quoth, and the new comfort options. Found a bug? **Options > Create Bug Report** makes a zip of your logs, crash reports and settings: attach it to a GitHub issue with a line on what happened.

## Highlights

- **Multiplayer beta**: coop and deathmatch for up to 16 players, from Multiplayer > Host Game. Everything you do alone works together: grabbing, throwing, melee and parries, climbing, holsters, reloads, the force grab.
- **Join by room code**: the host reads out a 6-letter code, the others type it at Join Game. No port forwarding behind most home routers.
- **Proximity voice chat**: push-to-talk or voice activation, heard from the talker's head, with noise suppression and per-player mute and volume.
- **Bots with hands**: deathmatch bots and co-op bots (VR Settings > Play > Add Co-op Bot) that dual wield, swing, parry and follow you to the next level.
- **Dwell (experimental)**: Dwell's two episodes play by their own rules, with its own monsters and bosses and its weapons made for VR. The installer offers it for free.
- **Quoth (experimental)**: Quoth 2.2's maps and custom Quoth maps play under Arcane Dimensions' rules. The installer offers it when Arcane Dimensions is chosen.
- **Comfort**: sliding on by default, sprint by clicking the stick, double-tap for the other speed, and choose when you pull up onto a ledge.
- **Faster relighting and lighter crowds**: map relighting about twice as fast, and crowded maps with many armed grunts run much lighter.

## Multiplayer and bots

- Coop and deathmatch for up to 16 players; everyone needs this version. The guide: [MULTIPLAYER.md](https://github.com/vittorioromeo/quakevr/blob/next/docs/vr-port/MULTIPLAYER.md).
- Room codes (Join Game > Join by room code, or `connect @CODE`); behind a strict router you are told so, and the host forwards UDP port 26000.
- Host Game says whether your port can be reached from the internet.
- You see each other as you are: heads, hands, fingers, held weapons and bodies sized by each player's calibration.
- Other players' guns, torches, power-ups, worn gear, shots, casings, wounds and slides look and sound as your own.
- Ragdolls, staggering and climbing monsters, and dead players' bodies fall the same way for everyone; a dead player's body is his own.
- Your movement is predicted and hits are judged where you saw the others (lag compensation).
- The scoreboard on your wrist gadget, with who is talking and, in coop, the level's kills and secrets.
- Server Browser with favourites, recent servers and LAN games; Map Library maps you lack are installed on joining.
- Friends can join a coop game in progress; a player who drops and comes back gets his slot and his things back.
- A vanished server is reported after 3 seconds and you are back at the Server Browser after 30.
- Dedicated servers (`-dedicated`), with `server.cfg` and players' pings in the console.
- Bots toned down at the lower skills: slower to aim and notice you, with swings you can see coming and parry.
- Bots draw every gun they carry, are a player's size, and you grab a bot's weapon where you see it.
- Debug > Show Disarm Volumes and Show Hit Zones work on bots and other players.

## Quoth and Dwell

- **Dwell**: Select Campaign > Dwell, and Copper maps from the Map Library (such as Underdark Overbright).
- Dwell's Kamikaze, Brute, Construct and both bosses fight in its maps, and from the toolgun in any map.
- Dwell's Rotary Shotgun and Crystal Lance are VR weapons made from Dwell's own models, with ammo screens like ours.
- The Crystal Lance is two-handed: slide your other hand along its pole to aim, swing its crystal head as a blade.
- The Rotary Shotgun is worn as a gauntlet; load it by holding shells from the pouch at its shell port.
- Haste, Berserk and the Wetsuit show their time left on the wrist and tint the screen in Dwell's colours.
- Armed grunts that pick up Dwell's weapons fire them as Dwell does.
- **Quoth**: Select Campaign > Quoth; needs Arcane Dimensions. The installer downloads Quoth 2.2 from Quaddicted.
- **Surface relief for Dwell and Quoth**: the installer can give their bricks, panels and rock the depth AD's textures have, made on your PC from your own copies (a few minutes on a graphics card). Already have them installed? Setup's **Make reliefs only** does just that.

## Arcane Dimensions

- Quoth's maps run on Arcane Dimensions' rules, monsters and weapons.
- In multiplayer, the wrist's Arcane Dimensions page follows the server's game, not your last local map.
- The holster menu names AD's Widowmaker, Shadow Axe and Plasma Gun.

## Weapons and armed monsters

- Crowded maps run lighter: armed grunts you can't see and that haven't noticed you cost far less; Dwell's biggest map about half.
- Melee in a crowd of armed enemies is cheaper; what a blow strikes is unchanged.
- The alarm spreads further: a monster you hurt or kill unseen cries out, and those nearby come to look (Combat > Stealth AI > Cries Heard).
- The heavy revolver's cylinder stays shut during a melee swing; a deliberate flick still opens it.
- A gun-and-axe grunt no longer swings an axe it has just put away.

## VR body and comfort

- **Sliding on by default**: while running, pull the turning stick down. Your view eases down to a slide and your legs point the way you slide.
- **Sprint by clicking the stick** (VR Settings > Locomotion and Controls > Speed Button: Toggle).
- **Double-tap for the other speed**: push the stick twice to run (or walk) until you let go (off by default).
- A stick click that bounces no longer counts twice.
- **Climbing: Mantle When**: pull up at once (as before), when you let go, on a jump, or either.
- Riding a lift no longer sinks your view or the monsters into it.
- Ammo Pouch Forward, Right and Up sliders in Holster Calibration.
- With the body hidden (Body Type Only Hands), the pouches sit on your belt again, within reach.
- **Eye-tracked foveation by default** with an eye-tracking headset (a Quest Pro through Virtual Desktop, Varjo...).

## Flat-screen play

- A flat-screen player aiming down the sights looks the part to others: head behind the sight, cheek on the stock.
- Ladders: only the back key lowers you onto a ladder from its top.
- The third-person camera and its fade behave with the fly camera and weapons lying around.
- `-window` makes a normal, resizable window; `-windowpos <x> <y>` places it.
- No attract-mode demos: flat goes straight to the main menu.

## Quest

- The standalone Quest version is not part of this pre-release: its renderer is still in progress, and it is tested in separate builds.
- Quest headsets through Link, Air Link, Steam Link or Virtual Desktop work as before; a Quest Pro's eye tracking now drives foveated rendering.

## Menus and other

- **Discord status**: with the Discord app running, your profile shows the campaign, map and mode (Menu Settings > Discord Status to turn it off).
- Multiplayer menu: Join and Host go straight to their page (the old IPX / TCP/IP choice is gone).
- Relighting (Graphics > Relighting and the installer) is about twice as fast with the same look.
- Safe mode turns on only after a real failed start, not after closing the game while it compiles its shaders.

## Fixes

- Bloom no longer spreads black or coloured blocks from one extremely bright pixel.
- A damaged network message or demo ends that game with the reason instead of closing Quake VR.
- Dwell: the Crystal Lance's first shot no longer ends the game; its map pickups no longer vanish; the start map's runes open the episodes; the Kamikaze chases before it bursts; the Sculptor no longer traps you inside it.
- Dwell's music no longer plays in Quake's maps; Dwell's maps play their own tracks.
- A dead player's holstered dagger no longer drops as a knight's sword.
- Starting a coop-only campaign in deathmatch no longer stops the game.
- Telefrags strike only the living, not corpses and gibs.
- Linux: SteamVR's logs are reachable from the Flatpak, and a failed VR start says why more clearly.

## Known issues / pre-release

- **Splitscreen is not included yet.**
- **Quest standalone**: renderer work in progress, not in this build.
- **Multiplayer is a beta**: little real internet play yet; no relay for routers that room codes can't get through (forward UDP 26000 or use a virtual LAN); custom maps that aren't Map Library packages aren't sent to joining players.
- Single player only: time scale and bullet time, saving at will, Quake VR's crates, Dimension of the Past and Dimension of the Machine.
- Dwell and Quoth are experimental: expect rough edges in some maps.
- The installer and the game are not code-signed, so Windows SmartScreen may warn you (More info > Run anyway).

## Notes

- Your settings carry over from 1.1: settings left at an old default move to the new one, values you changed stay yours.
- Every change since 1.1.0: [PATCH_NOTES_1.1.1.md](https://github.com/vittorioromeo/quakevr/blob/next/docs/vr-port/PATCH_NOTES_1.1.1.md).
