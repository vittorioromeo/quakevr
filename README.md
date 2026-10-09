<p align="center"><img src="docs/images/quakevr-unleashed-wide.webp" alt="Quake VR: UNLEASHED" width="760"></p>

# Quake VR: UNLEASHED

**id Software's Quake (1996), rebuilt for virtual reality.**

[![Quake VR: UNLEASHED trailer](https://img.youtube.com/vi/TRj7xRLuT64/maxresdefault.jpg)](https://www.youtube.com/watch?v=TRj7xRLuT64)

*Click the picture to watch the trailer on YouTube.*

Quake VR: UNLEASHED (**QVR:U**) is a VR mod of Quake by Vittorio Romeo. You play the whole game, both mission packs
and the re-release's newer campaigns included, with your own hands and body: a weapon in each hand, aimed with one
hand or two, holstered at your hips and shoulders, thrown across the room or swung as a club. You walk around your
room, crouch behind cover, punch, parry, headbutt, and swim with your arms.

QVR:U runs on the [Ironwail](https://github.com/andrei-drexler/ironwail) engine (0.8.2) and talks to your headset
through [OpenXR](https://www.khronos.org/openxr/). It is the successor of the
[original Quake VR](https://github.com/vittorioromeo/quakevr/tree/quakevr-old), which ran on QuakeSpasm-Spiked and
SteamVR's OpenVR (see [Compared with the original Quake VR](#compared-with-the-original-quake-vr)).

> **Status:** in active development. It is playtested mostly on a Meta Quest 3 through Virtual Desktop. Other
> headsets and controllers work through OpenXR but have had less testing. Expect rough edges, and please
> [report](https://github.com/vittorioromeo/quakevr/issues) what you find.

**Contents:** [Installation](#installation) · [First steps](#first-steps-and-basic-tweaking) ·
[Features](#features) · [Compared with the original](#compared-with-the-original-quake-vr) ·
[Documentation](#documentation) · [Building from source](#building-from-source) · [Support](#support) ·
[Credits and licence](#credits-and-licence)

## Installation

**You need:**

- A Windows 10 or 11 PC (x64) that can run PC VR. The graphics card needs OpenGL 4.3.
- A PC VR headset and an **OpenXR runtime**: SteamVR, Virtual Desktop (its VDXR runtime), Meta Quest Link, or
  another.
- **Quake**, the full game, from [Steam](https://store.steampowered.com/app/2310/QUAKE/), GOG or Epic: the original
  1996 data or the 2021 re-release. The mission packs and the re-release's newer campaigns are optional; the ones
  you own are found by themselves.

**Steps:**

1. **Download `QuakeVR-Setup.exe`** from the latest release on the
   [Releases page](https://github.com/vittorioromeo/quakevr/releases/latest).
2. **Run it.** It isn't code-signed, so Windows SmartScreen may say "Windows protected your PC": click **More info**,
   then **Run anyway**. It needs no administrator rights. It then:
   - shows a short **statement** from the author on how AI was used to make QVR:U, with four points to answer;
   - finds **your Quake** (Steam, GOG or Epic), the expansions you own, your OpenXR runtimes and the Visual C++
     runtime (**Your PC**), changing nothing; *Use another Quake folder…* if it is somewhere else;
   - lets you pick the **options**: the install folder (`%LOCALAPPDATA%\Programs\QuakeVR` by default), the **HD
     textures** (the Quake Revitalization Project's, about 0.6 GB, highly recommended), **relighting the maps** at the
     first start (with see-through water, highly recommended), and the shortcuts;
   - downloads the game, checks every file and **installs** it in its own folder. Your Quake folder is never changed.
     If the Visual C++ runtime is missing or too old, it installs Microsoft's (one Windows permission prompt).
3. Start your OpenXR runtime (SteamVR, or Virtual Desktop), put the headset on, and press **Play in VR** (or use the
   **Quake VR Unleashed** shortcut on the desktop or in the Start menu). The installer's last page also shows how to
   add the game to Steam, so it appears in SteamVR's and Virtual Desktop's game lists.

The first start runs **VR Calibration**, which measures your height and body, then leads to the **VR hub**: pick
Quake, a mission pack or another campaign there and step into the portal. The hub also leads to the tutorial and the
firing range.

**Updating:** when a new version is out, the game's menus say so in their bottom right corner. Download the new
`QuakeVR-Setup.exe` and run it: it finds your install and replaces only the program files that changed. Your
settings, saves, screenshots and relit maps are kept. **Uninstalling:** Windows' *Installed apps* (Quake VR:
Unleashed > Uninstall).

[docs/INSTALL.md](docs/INSTALL.md) has the details: each installer page, where everything goes, installing from the
`QuakeVR.zip` package by hand instead, the official campaigns, custom maps and mods, HD textures, relit maps,
performance and troubleshooting.

**Network and privacy.** The game sends nothing about you or your play. It goes online only for these:

- **Update check:** at start-up, at most once an hour, it reads the latest release's `latest.json` from GitHub. It
  never installs anything. To turn it off: *Advanced VR > HUD and Menus > Menu > Check for Updates* (Menu Detail:
  Advanced), or `vr_update_check 0`.
- **Map Library:** at start-up, at most once a day, it fetches Quaddicted's package index and ratings
  (`vr_maps_fetch 0` turns this off). It downloads maps only when you ask.
- **Ironwail's add-on list** (the re-release's add-ons), at most once a day; `-noaddons` turns it off.
- **Relighting:** it downloads ericw-tools only when you ask.

## First steps and basic tweaking

**Default controls** (main hand / off hand):

| Button | Main hand | Off hand |
|---|---|---|
| Trigger | Fire | Fire the off-hand weapon (or use the force grab) |
| Grip | Grab, hold, and let go to drop or throw | Same |
| A / X | Jump | Reload |
| B / Y | Next weapon | Previous weapon. Held at your mouth, it records a voice note. |
| Stick | Turn. Up and down swim up and down. | Move |
| Stick click | Reload | Run / walk |
| Menu button | Menu: the left controller's (Index controllers have none: their left B opens it) | |

Every button can be rebound in **Options > Key Setup** or with `bind` in the console. They are named like gamepad
keys (`RTRIGGER`, `LSHOULDER`, `ABUTTON`...). See [docs/SETTINGS.md](docs/SETTINGS.md#controls-and-bindings).

**VR Settings** (the main menu's *VR Settings*, menu > Options > VR Settings, or `menu_vr` in the console) has the
settings most people need, each with a line of help. Point at it with the laser and pull the trigger, or use the sticks:
A selects, B goes back. The main menu's first row, *VR Calibration*, runs the calibration again. Worth checking first,
top to bottom:

1. **Height and hands:** *Height* (stand straight and pick *Set Height Now*), *World Scale*, *Floor Offset*; then
   *Show Controller* and the *Hand Calibration* rows to line the drawn hands up with your real ones. If shots go
   above or below where the gun seems to point, change *Hand Pitch* first.
2. **Moving:** *Move Towards* (head or a hand), *Default Speed*, *Swap Stick Functions*, the comfort *Vignette*,
   *Teleport*, and *Turning Mode* (smooth, or snap 30/45/90 degrees) with its speed or angle.
3. **Weapons and body:** *Weapon Grip* set to *Sticky* keeps weapons in your hand without holding the grip;
   *Two-Handed*, *Body Type*, *Wrist Gadget Arm*, the *Flashlight* and its side.
4. **Sound and display:** *Volume*, *Music Volume*, *HUD* (wrist gadget or status bar), *Crosshair*, *Headset Gamma*,
   *Render Scale* with its upscaling and foveated rendering, and the main graphics switches.

Everything else is in **Advanced VR** (the main menu's *Advanced VR* row, or the corner's *Advanced VR* button):
*Handedness*, the *Headset* page with the *OpenXR Runtime*, *Changed Settings*, *Run VR Calibration Again*, the
*Tips*, and, with *Menu Detail: Advanced*, every gameplay page, the *Comfort* preset (Locomotion) and the *Graphics
Preset*.

**Performance:** the shipped settings are tuned for a fast PC. If frames drop, pick a lower *Preset* (Advanced VR >
Graphics, with Menu Detail: Advanced: Off, Low, Medium, High or Ultra) and lower *Render Scale* (VR Settings) below 1.

**Search:** the corner's *Search* button finds any setting by its name or what it does, as you type, and opens its
page on it (a cvar's name works too, e.g. `vr_snap_turn`).

**Tips:** new players get a tip the first time they come near something they can use (for now, a wall torch), shown
once, on a small floating screen beside it or on the wrist gadget (*Advanced VR > Tips*, which also has *Show Tips
Again*).

**Console:** the corner's *Console* button shows Quake's console with a keyboard under it, to type commands in the
headset: *Run* runs the line, *Tab* completes it, *Prev* and *Next* go through the history; the stick, the wheel or
Page Up and Down scroll the text.

**More:** every page ends with **Menu Detail**. *Standard* (the default) shows what every player sets; *Advanced* shows
every gameplay, display and graphics setting (about sixty pages: combat, movement, carrying and throwing, gore, the
body, weapons, the HUD, graphics...); *Developer* adds the tuning pages (weapon and prop offsets and weights, ragdolls,
hitboxes), recording, debug and tests. A setting you changed has a `*` by it; *Changed Settings* (Advanced VR) lists
them all, and each page's *Reset This Page* puts its settings back. [docs/SETTINGS.md](docs/SETTINGS.md) covers them.

**Starting over:** *Options > Reset All* restores the shipped settings and bindings. Your settings are saved in
`quakevr\ironwail.cfg` in the install folder: delete it for a completely fresh start (VR Calibration then runs again).

## Features

The headlines, grouped. [docs/FEATURES.md](docs/FEATURES.md) explains each one, how to use it and where its settings
are.

### Weapons

- **A weapon in each hand:** every weapon of Quake and both mission packs, with modelled grips, iron sights, a glowing
  ammo screen, recoil, muzzle flashes and tracers, and a button on top for a second ammo type.
- **Two-handed aiming** by the foregrip, an optional virtual stock, and weight: heavy guns lag, want two hands, and
  can be wrenched from your grip. Hands and barrels stop at walls.
- **Holsters** at your hips, chest and shoulders; reload at a holster or flick the super shotgun open. **Hand
  grenades** from a pouch at your back.
- **New weapons to take:** knights' swords, the ogres' chainsaws (pull the cord), the grunts' burst rifles, the
  enforcers' laser rifles, crowbars from crates, and the grappling hook with a physical rope.

### Combat

- **Melee that reads real swings:** punches, blades, clubs and pistol-whips that land on the monster's real shape;
  axes that stick where you throw them.
- **Parry** with a weapon or crossed forearms (it stops the attack), **counter-attack**, **bash**, **shove**,
  **headbutt**, and **bat** spikes, lasers and grenades back. Catch grenades, or shoot them.
- **Bullet time**, from your wrist or a stick, with an optional Sandevistan mode.
- **Ragdolls and knockdowns:** corpses go limp as physical bodies you can drag and throw; a hard shove knocks a
  monster down to struggle back up, and off a ledge it always falls.
- **Stamina** and tired arms (optional), enemy shoves, and a **training dummy** that can be any enemy and fights
  back.
- **Positional damage** (headshots, arms and legs) and knockback in both directions.

### Physics and gore

- **Everything is physical:** thrown weapons, boxes, crates, rocks, explosive boxes you can stack and stand on, wall
  torches you take off the wall, gibs and heads, all rigid bodies with true-scale gravity. Throws are read from your
  hand as in Half-Life: Alyx and feel the same at any frame rate.
- **Force grab:** point, pull the trigger, flick your wrist, and catch.
- **Gore you can tune:** blood on walls, you, your weapons and props (water washes it off), sticking and dripping gibs,
  small gibs and brain chunks, wounds, beheading, head pops and limb gore.
- **Fire and lightning:** flames spread over monsters, corpses and crates and smoulder out; lightning leaves arcs,
  convulsions and burns, on you too.

### Movement and body

- Smooth or teleport locomotion, smooth or snap turning, comfort presets, and room-scale play with crouching,
  real jumps and leaning.
- **Swimming** with arm strokes, **climbing** ledges and rungs hand over hand, and the **grappling hook**.
- **Full body** with arms and legs (IK), three builds, your armour, wounds and powerups on it; hands fitted to what
  they hold. **VR Calibration** measures you at the first start.
- **Left- or right-handed**, every side-dependent option at once or one by one. **Haptics** for shots, hits,
  holsters and catches.

### Interface

- **Wrist gadget HUD:** health, armour, ammo and stamina on a CRT screen on your forearm, with the game's messages as
  a hologram. The classic status bar on a hand is still there.
- **Flashlight** on your belt: a shadow-casting torch you switch, take in hand, flip with a flick, or clip on a gun
  or your head.
- **VR menus** with a laser, live previews, drop-down lists, **search** (with your recent results), a **console**
  with a keyboard, Menu Detail levels and per-page reset; **tips** for new players, also placed by mappers.

### Graphics and sound

- **Relit maps**, made on your PC **in the game** (one map or every map; the installer can queue them all for the
  first start) or by a script, with coloured light, ambient occlusion and lamps that light their rooms;
  **see-through water**.
- **HD textures:** the Quake Revitalization Project's, offered by the installer.
- A **darker, moodier look** like DarkPlaces: coloured dynamic lights, real-time shadows, bloom, lit particles,
  models lit by the map's lights, bump maps (Quetoo's material maps shipped), parallax and detail textures.
- **Teleporters that show where they lead**, live: you, your missiles and the monsters' sight pass through.
- **Water** with waves, reflections, refraction, caustics, splashes and foam; tone mapping and colour grades; an
  optional **retro look**; FSR/NIS upscaling and foveated rendering.
- **Spatial sound** with Steam Audio (HRTF, occlusion, reverb), physics sounds, and the soundtrack played from the
  Quake you own.

### Maps, campaigns and play

- **Quake and both mission packs** in one game, from a VR hub with a tutorial and a firing range. The re-release's
  **Dimension of the Past**, **Dimension of the Machine** (with its Horde mode) and **Dawn of the Machine** play
  natively in single player, when you own them.
- **Map Library:** browse [Quaddicted](https://www.quaddicted.com/)'s custom maps and download, install, uninstall and
  play them in the game. **Other mods** run in a compatibility mode.
- **Multiplayer and bots** (FrikBot), with a fixed 72 Hz server tick; flat-screen play (`vr_enabled 0`, or the
  *flat screen* shortcut).
- **For recording and testing:** a spectator camera, slow motion and highlight markers for trailers; voice notes, an
  in-game checklist, a motion recorder and debug pages for playtesters.

## Compared with the original Quake VR

The [original Quake VR](https://github.com/vittorioromeo/quakevr/tree/quakevr-old) (the `quakevr-old` branch, up to
v0.0.7 beta) was built on QuakeSpasm-Spiked and OpenVR. QVR:U keeps its gameplay and QuakeC and rebuilds the rest.

| | Original Quake VR (QSS + OpenVR) | QVR:U (Ironwail + OpenXR) |
|---|---|---|
| Engine | QuakeSpasm-Spiked (2020), converted to C++ | Ironwail 0.8.2. The VR code is a separate module. |
| Headset API | OpenVR (SteamVR only) | OpenXR: SteamVR, Virtual Desktop (VDXR), Meta Quest Link and other runtimes, picked automatically |
| Controls | SteamVR Input bindings | Controller buttons are ordinary Quake keys: rebind them in Key Setup or the console |
| Installation | A separate folder: copy Quake's pak files into it (renamed, for the mission packs) | An installer: its own folder, your Quake read in place and never changed; the mission packs and the re-release's campaigns found automatically; updates replace only what changed |
| Body | A floating torso | A full body with arms and legs (IK) |
| HUD | Status bar on a hand | Wrist gadget with a CRT screen, or the status bar |
| Lighting | Quake's lightmaps (optional external `.lit` files) | Relit maps, real-time shadows, per-pixel lights, bump maps, bloom, tone mapping |
| New in combat | | Parry, bash, headbutt, bullet time, ragdolls and knockdowns, enemy weapons, hand grenades, batting projectiles back, physics props, gore |
| New in movement | | Swimming with strokes, leaning, climbing, the grappling hook's rope, a flashlight on your belt |
| Campaigns | Quake and the two mission packs | Also Dimension of the Past, Dimension of the Machine and Dawn of the Machine |
| Other mods | Had to be ported | Run in a compatibility mode |
| Not (yet) carried over | | Index per-finger tracking (fingers follow the buttons instead) |

Saves and configs from the original Quake VR don't carry over.

## Documentation

For players:

- [docs/INSTALL.md](docs/INSTALL.md): installation in detail (the installer, or the zip by hand), OpenXR runtimes,
  mission packs and official campaigns, custom maps and mods, HD textures, relit maps, performance, troubleshooting.
- [docs/RELIGHTING.md](docs/RELIGHTING.md): relighting your own copy of Quake's maps, for the relit look and
  see-through water, step by step.
- [docs/FEATURES.md](docs/FEATURES.md): every feature, and how to use it.
- [docs/SETTINGS.md](docs/SETTINGS.md): advanced settings, all the menu pages, console variables, config files.

[docs/README.md](docs/README.md) lists every document, with a line on each.

For developers:

- [docs/BUILDING.md](docs/BUILDING.md): building the engine, the QuakeC and the release package, and the tool
  scripts.
- [Installer/README.md](Installer/README.md): the installer (C#, WPF): its build, command lines and tests.
- [docs/vr-port/RELEASING.md](docs/vr-port/RELEASING.md): making a release.
- [docs/vr-port/](docs/vr-port/): the port's design notes. [PLAYTEST.md](docs/vr-port/PLAYTEST.md) is the playtest
  guide, [TESTING.md](docs/vr-port/TESTING.md) the testing tools, [ROUND21.md](docs/vr-port/ROUND21.md) the log of
  the current feedback round (its older sections in `archive/`, the earlier rounds in git history), and there are
  deep dives into graphics, lighting, throwing, hitboxes and body IK.

## Building from source

You need Windows x64, Visual Studio 2022 with the C++ desktop workload and its **C++ Clang tools for Windows** (the
engine is built by clang-cl), and [FTEQCC](https://www.fteqcc.org/) for the QuakeC.

- **Engine and QuakeC:** build `Windows\VisualStudio\ironwail.sln` (Release | x64), in Visual Studio or with
  `msbuild Windows\VisualStudio\ironwail.sln -p:Configuration=Release -p:Platform=x64 -p:QvrQcCompiler=<path to fteqcc64.exe> -m`.
  The build compiles the QuakeC too (`quakevr\progs.dat`).
- **Package:** `powershell -ExecutionPolicy Bypass -File Windows\package-quakevr.ps1 -Fteqcc <path to fteqcc64.exe>`
  puts it in `dist\QuakeVR` and `dist\QuakeVR.zip`.
- **Installer:** `dotnet build Installer\QuakeVR.Installer.sln` (.NET 9 SDK): see
  [Installer/README.md](Installer/README.md).
- **Release:** one command, `Misc\release\make_release.ps1` (builds, tests, packages, tags and publishes on GitHub):
  see [docs/vr-port/RELEASING.md](docs/vr-port/RELEASING.md).
- **CMake** also works, for development (on Windows, configure it for clang-cl with `-T ClangCL`). VR needs Windows
  x64; the other platforms (CMake or the Makefiles) build with a mock headset only.

[docs/BUILDING.md](docs/BUILDING.md) has the details.

## Support

QVR:U is free. If you enjoy it, you can support its author on [Ko-fi](https://ko-fi.com/vittorioromeovee). Talk
about it on [Discord](https://discord.me/quakevr), and report bugs and ideas on the
[GitHub issues page](https://github.com/vittorioromeo/quakevr/issues).

## Credits and licence

Quake VR: UNLEASHED is by **Vittorio Romeo**. It builds on:

- **[Ironwail](https://github.com/andrei-drexler/ironwail)** by Andrei Drexler and contributors: a fast
  [QuakeSpasm](https://sourceforge.net/projects/quakespasm/) fork, which descends from FitzQuake and id Software's
  Quake. Ironwail's own features, such as playing the 2021 re-release content, the Mods menu and its HUD styles,
  are described in [its README](https://github.com/andrei-drexler/ironwail#readme).
- The **original Quake VR** and the ports it grew from: Fishbiter's and Zackin5's OpenVR ports, Dominic
  Szablewski's Oculus port, and Spike's QuakeSpasm-Spiked.
- Hand models by [CrazyHairGuy](https://www.crazyhairguy.com/). Weapon models are based on
  [Authentic Model Improvements](https://github.com/NightFright2k19/authmdl).
- The world's normal, specular and glow maps are from the [Quetoo](https://github.com/jdolan/quetoo) game data by
  Jay Dolan (jdolan) and contributors, made for Rygel's Texturepack Ultra (from the Quake Retexture Project and
  others; id Software's textures retextured), under
  [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/): see
  [quakevr/textures_quetoo/README.md](quakevr/textures_quetoo/README.md) for every author.
- The HD textures are the [Quake Revitalization Project](https://www.moddb.com/addons/quake-revitalization-project-archive)'s
  ([docs/vr-port/TEXTURES.md](docs/vr-port/TEXTURES.md): contents, credits, licence).
- Box3D (Erin Catto) for the physics, Steam Audio (Valve) for the spatial sound, Zancle (Vittorio Romeo), FrikBot
  (Ryan "FrikaC" Smith), the OpenXR SDK (Khronos), SDL2, GLM, FTEQCC, and ericw-tools. The Map Library's maps come
  from [Quaddicted](https://www.quaddicted.com/).

The full list, with the research and techniques used, is in [docs/vr-port/CREDITS.md](docs/vr-port/CREDITS.md).

QVR:U is free software under the **GNU General Public License v2** ([LICENSE.txt](LICENSE.txt)), like the Quake
engine it comes from. Quake's game data (maps, models, sounds, textures) belongs to id Software and isn't included:
you need your own copy of Quake.
