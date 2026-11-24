**Quake VR: UNLEASHED 1.2.0 beta 3**: the third preview of the next update. This is a **beta**: expect rough edges, and the standalone Quest version is still **experimental**. Beta 3 brings a weapon wheel, your own holster places in multiplayer, joining a server's campaign, flat-screen throwing, melee and swimming, a charged Crystal Lance, a faster Quest version and a fix for the Quest's crash entering the hub. **On the Quest, beta 1 and beta 2 players must uninstall the old app once before updating, and back up their saves first** (steps below). What came in betas 1 and 2 is further below. Please report bugs: Options > Create Bug Report makes a zip you can attach.

**The full patch notes** (every change since 1.1.0, by area): [PATCH_NOTES_1.1.1.md](https://github.com/vittorioromeo/quakevr/blob/next/docs/vr-port/PATCH_NOTES_1.1.1.md) (they become the 1.2 notes at the final release).

# New in beta 3

## Before you update on the Quest: uninstall once (back up your saves first)

- Beta 3's Quest app is signed with Quake VR's own release key. A Quest cannot update an app signed with a different key, so **if you installed beta 1 or beta 2 on your Quest, uninstall the old app once** before installing beta 3. Uninstalling deletes the app's folder on the headset, **your saves and settings there too**. Setup does not back them up for you (if you skip the uninstall, it stops and says the Quest has a differently signed build), so copy them yourself first:
  1. Connect the Quest to your PC with its USB cable and put the headset on: if it asks to allow access to its files, allow it.
  2. On the PC, open the Quest in File Explorer (This PC > your Quest > Internal shared storage) and go to `Android\data\com.vittorioromeo.quakevr\files`.
  3. Copy your saves (`*.sav`) and settings (`ironwail.cfg`) from the `quakevr` folder to your PC, and the saves in the folder of any mod you played there.
  4. In the headset: Library > Unknown Sources > Quake VR > Uninstall.
  5. Run Setup again with Meta Quest ticked: it installs beta 3 and copies the game to the headset again.
  6. Copy your files back into the same folders.
- From beta 3 on, updates install over the old app again, keeping your saves.

## Multiplayer: beta 2 players can't join beta 3

- **Network version 46**: everyone in a game needs beta 3 (a beta 2 player gets a "different version" message).
- **Your own holster places** are sent to the other players, so they see your holsters where you put them; bots wear the default ones. Holsters now show on bots and other players.
- A player joining a server now plays the **server's campaign** (Quake, an expansion or a mod), switching to it if they have it, with a clear message if they don't.
- Shots now meet players' bodies **as they are drawn** (head, arms, held weapon) instead of a plain box; Show Hit Zones shows them.

## Weapon wheel

- **Hold B or Y** to open a wheel around that hand: empty hand, your holsters, drop and unload. Time slows while it is open; point at a slot and let go to take it, or let go off the wheel to cancel. In VR the wheel is **off by default**: turn it on in VR Settings > Weapons > Immersion > Weapon Wheel. It stays where you opened it as you walk and turn, and a quick press (under about a fifth of a second) still does the button's own job.
- On the flat screen: hold 1 or 2 for the same wheel (the mouse picks, Esc cancels); a tap still cycles as before.
- Holding the button for the wheel no longer ejects your magazine (a tap still does), and weapons put away through the wheel keep their rounds. Opening the wheel makes no menu sound.

## Flat-screen play

- **Throwing** (J by default): tap for a toss, hold to wind up. Blades stick, the throw leaves your hand towards the crosshair, and the tutorial's throwing lessons can be passed.
- **Melee**: guard + fire bashes; the Crystal Lance thrusts like a spear (V); long blades and the Lance are held properly.
- **Swimming**: your hands make the strokes, with water sounds and bubbles.
- The tutorial's messages use flat-screen wording and show **your own keys**.
- Ropes and poles: W and S climb when you look up or down and swing when you look level; crouch slides down or lets go; jumping off a pole pushes you away from it.
- Smaller fixes: the gun no longer jitters at a wall when you look down; the hub's lecterns stand in one row, in Select Campaign's order; your hands sit on a ladder's rungs.
- **The Crystal Lance is no longer lost after a thrust**: a flat-screen stab never leaves a weapon stuck in the enemy (it used to leave you with an empty lightning gun). New Spear Hand Forward / Up / Out settings place the holding hand alone, and the default spear, throw and thrust positions are retuned.
- Crouching on a rope slides you all the way down to the floor instead of letting go early; throws made while running carry your speed; parrying with the other hand faces the right way.

## Climbing

- Poles stop players and objects (crates rest on them); two climbers can share a pole; chains stop shots, nails and blows (with sparks) and still swing.
- Hands: under the rope on a monkey traverse, thumbs wrapped round ropes and thin poles, elbows out; your holsters stay on your hips while you climb.
- A level pole keeps the grip you took it with (it no longer follows your aim); legs swing more smoothly; the thumb on thick poles is back to the normal grip; letting go behaves the same in VR, on the flat screen and in multiplayer.

## Weapons

- **Dwell's Crystal Lance** is now a charged shot: hold to charge (sparks, light, a hum, a shake and a buzz in your hand), and let go when it is full to fire. Its beam hits like the lightning gun (it gibs, knocks objects and bodies about) with sparks and smoke, and pushes crates harder; no empty click; only its tip sticks in what it stabs; thrown, it flies point first; held the other way round it fires from the crystal along the shaft.
- **Dwell's Crystal Lance and Rotary Shotgun stay yours**: once you have one, every way of getting that weapon back (weapon keys and the wheel, picking it up again, holsters, throws, saves and level changes, multiplayer) gives the Lance or the Rotary, never a plain lightning gun or super shotgun.
- The super nailgun's barrels spin up quickly and coast down, with a motor whirr; its muzzle flash comes only from the barrel that fires, and never from an empty gun (the flash and the band baked into the old model are gone). The dark specks in the super shotgun's and nailgun's barrels are gone.
- Thrown blades' kills push the body less; Quake VR's own weapon models' caps no longer look white; Dwell's unused Rifle is gone (an old save holding it gets a shotgun instead).
- Armed grunts on big maps (Arcane Dimensions' largest) no longer stand with stuck arms and floating guns.
- Other players' and bots' left holsters are no longer drawn turned round.

## Quest (standalone, experimental)

- **Faster in fights**: **Quest Foveation** (VR Settings > Graphics; High by default on the Quest 3) draws the edges of the view in less detail; each eye is drawn in one pass; the image is allowed to get smaller only during big fights; your own body and gore draw more cheaply; the Standalone preset turns retro (pixelated) lighting off, the most expensive way to draw the world there. Armed grunts' weapon poses are worked out by the engine now: less CPU in big fights.
- Changing Quest Foveation during a game no longer crashes.
- Taking the headset off pauses a single player game (Advanced > Headset > Headset Taken Off).
- **Fixed: a crash entering the hub** with the headset on (reported on a Quest 3S; beta 2 crashed the same way on the Quest 3). A start closed while the game prepares its shaders no longer turns safe mode on at the next start.
- **Mods and the rerelease's add-ons on the Quest**: Setup now makes every mod folder it copies readable by the game, each time it runs (a folder left unreadable by an older copy kept expansions and mods such as Arcane Dimensions and Quoth from starting), and it copies the rerelease's language data, so **Dimension of the Past** and **Dimension of the Machine** start on the Quest.
- Beta 3 is tested on the Quest 3; the Quest 3S and the Quest 2 are not tested by us yet.

## Other

- The Steam Frame's controllers are supported (thanks to Kasper Olesen).
- **Launch options renamed**: `-vr-hide-mirror` is now `-hidemirror`, `-vr-unattended` is `-unattended`, and `-vrmock` is `+vr_backend mock`. The old names still work until 1.3 and print a note.
- **Quoth needs Arcane Dimensions** installed: without it, Select Campaign marks Quoth "needs AD" and says why instead of stopping with an error when it starts.
- The tutorial always starts as a single player Quake game (also right after a multiplayer game); its spikes can be batted away; some surfaces that showed black are textured again.
- Centre messages are 20% smaller (HUD > Screens > Messages > Centre Message Size).

## Known issues (beta 3)

- **The Quest version is experimental**: close-up fights with many gibs and very large battles can dip below 72 frames a second; tested on the Quest 3 only (the Quest 3S is untested by us).
- Quest: the first start after installing or updating spends about 30 seconds on the loading panel while the game prepares its shaders (later starts take a few seconds).
- **Splitscreen is not included yet.**
- The rest of beta 1's known issues below still apply (multiplayer is a beta; single player only features; Dwell and Quoth experimental; no code signing).

# Beta 2 (2026-11-23)

(The second beta's notes, unchanged. Where they say Setup's Quest install keeps your saves: it does again from beta 3 on, after the one-time uninstall above. Where they say the Quest 3S should behave the same: it is untested.)

**Quake VR: UNLEASHED 1.2.0 beta 2**: the second preview of the next update, and the first with **Quake VR on the Meta Quest itself**, no PC needed to play. This is a **beta**: expect rough edges. Beta 2 also brings ropes, poles and monkey ropes to climb, three new tutorial lessons, Dwell's Crystal Lance and Rotary Shotgun finished, fixes for Quoth's kellbase, number keys for hands and holsters on the flat screen, and many fixes. What came in beta 1 (multiplayer, Dwell, Quoth and much more) is below.

## Quake VR on the Quest (standalone, experimental)

- **Play on the headset itself**, untethered. Made and tested on the Quest 3 (the Quest 3S should behave the same; the Quest 2 is not tested yet). This is the first public build: treat it as **experimental**.
- **Install it with Setup on your PC**: on Setup's first page, under **Install on**, tick **Meta Quest (standalone)** (with or without This PC). Setup installs the app and copies your Quake files, the expansions you own, the HD textures, the music and the mods it installed (Dwell, Quoth, Arcane Dimensions) to the headset. Running it again copies only what changed; it never deletes anything on the headset, so your saves and settings there stay.
- **Before you install**, turn on USB debugging (Setup shows these steps too, with a **Check again** button):
  1. In the Meta Horizon app on your phone: Devices > your Quest > Headset settings > **Developer mode**, on (the first time, Meta asks you to register as a developer, free). Restart the headset.
  2. Connect the Quest to your PC with a USB cable that carries data (the one that came with it does).
  3. Put the headset on: it asks "Allow USB debugging?". Tick "Always allow from this computer" and press **Allow**.
  4. Press Check again in Setup. Setup fetches Google's adb tool itself if your PC has none.
- **Start it** from the headset's Library > **Unknown Sources**.
- **Standalone graphics preset**: the Quest starts with it, made for a steady 72 frames a second (a smaller image with dynamic resolution, no MSAA, shadows or bloom, Quake's own dynamic lights, fewer particles, marks and gibs). On a PC it is the lightest preset with Quake VR's look (VR Settings > Graphics Presets). Thinner Blood Nearer (off by default) thins out blood right in front of your eyes.
- **Music on the Quest**: the Ogg soundtracks of the store versions now play there too.
- Known limits: big close-up fights with lots of gibs can dip below 72 frames a second; the Quest cannot relight maps itself (Setup copies the maps your PC relit).

## Climbing: ropes, chains, poles and monkey ropes

- **Ropes and chains** hang in maps and swing, and you can climb them in VR: grip with either hand and go hand over hand; your weight swings the rope, your legs wrap round it, and jumping off mid-swing keeps the speed. Pump the swing with the stick. On the flat screen, walk or jump into one to grab it, look up or down and hold forward to climb.
- **Poles** (wood or metal) climb the same way, and **ropes strung between two points** become a **monkey traverse** when they are near level: hang under it and go hand over hand (flat screen: the move keys).
- Spawn them with the toolgun (Spawn > Props > Ropes); try them all in the new test map `vrropes`. Everyone in multiplayer sees the same rope and the climber on it.

## Tutorial

- **Three new lessons** before the arena: **12 Blades** (cut, stab and pull out, reverse grip, throw an axe), **13 Bash and Bat** (shove or disarm an armed grunt on a bridge, bat a spike back, catch a grenade and throw it back) and **14 Ropes** (swing across a pit; and putting on armour). The arena is now lesson 15, and the game is saved at its door: dying there brings you back to the door with what you had.
- Tips on disarming enemies and on bullet time.

## Dwell, Quoth and Arcane Dimensions

- **Dwell's Crystal Lance** is a two-handed lance: hold the pole like a spear, slide your hands along it, swing or thrust its crystal head, fire the beam from the tip. A cord runs from its butt to a cell in your pouch. Its beam now lasts as long as you fire (it used to stop at once), with violet sparks and glow.
- **Dwell's Rotary Shotgun** is worn as a gauntlet, as the super nailgun is, with a remade model (four round barrels, a shell port on the side to fill with a handful of shells).
- **Quoth's kellbase**: its energy field is visible now (pale blue sparks), monsters no longer keep walking into it and hurting themselves, and its ladders climb like ours (VR and flat screen).
- **Arcane Dimensions' and Quoth's soldiers fight as armed humanoids** (rocket, grenade and plasma soldiers, defenders, eliminators, pyros, fumigators, crossbow knights), dressed in the colour of their weapon, and the backpacks they drop match.
- Shotgun shells flying as projectiles (AD's and Quoth's rules) fly 30% faster; a heavy revolver headshot pops heads there too.

## Flat-screen play

- **Number keys for hands and holsters** (with holsters on): 1 and 2 cycle each hand through its holsters, 3 swaps hands, 4 and 5 holster, 6 and 7 drop. 9 dims the gear lights.
- The flat-screen keys moved into Options > Controls, in a "Quake VR (flat screen)" group, rebound like any other key; a key used twice is marked with "!".
- Mods' ladders climb as the tutorial's ladder; a blade stuck in an enemy keeps its hand on it; noclip flies where you look.

## Other changes and fixes

- **No more slipping through grates, bars, pillar rows or beside lifts**: invisible blockers close the gaps Quake's own box never fit (Movement > Player Hitbox > Close Gaps Quake's Box Never Fit).
- **Centre messages in the headset** (keys needed, secrets, map texts) use the flat visor's message look, smaller than before; the wrist hologram is back.
- **Being disarmed is predictable** (still off by default): two jolts on one hand within three seconds, and the first one warns you.
- **Haste speeds every gun**; the nail pistol, heavy revolver and flare gun fire full auto while it lasts. The heavy revolver's cylinder stays shut in a melee swing but a wrist flick still opens it.
- **Your body draws lighter** (62% fewer triangles, about a quarter less GPU time), looking the same.
- Multiplayer: other players bleed as you do; bots fire the revolver and no longer show floating hands; corpses keep their armour; deathmatch drops vanish after 20 seconds; ragdolls and tipped props no longer jump or spin at joining players. **Network version 44**: everyone needs beta 2.
- Fixed: a crash when a map loaded in VR while a centre message was up; grunts frozen at low doorways (e1m1's); a shove making an earlier-hurt enemy bleed from every limb.

## Known issues (beta 2)

- **The Quest version is experimental**: close-up fights with many gibs can dip below 72 frames a second; tested on the Quest 3 only.
- **Splitscreen is not included yet.**
- The rest of beta 1's known issues below still apply (multiplayer is a beta; single player only features; Dwell and Quoth experimental; no code signing).

# Beta 1 (2026-11-22)

(The first beta's notes, unchanged. Where they say the standalone Quest version is not included: beta 2 includes it.)

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
