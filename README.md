# Quake VR

**id Software's Quake (1996), rebuilt for virtual reality.**

Quake VR is a VR mod of Quake by Vittorio Romeo. You play the whole game, both mission packs included, with your
own hands and body: a weapon in each hand, aimed with one hand or two, holstered at your hips and shoulders, thrown
across the room or swung as a club. You walk around your room, crouch behind cover, punch, parry, headbutt, and swim
with your arms.

This version runs on the [Ironwail](https://github.com/andrei-drexler/ironwail) engine (0.8.2) and talks to your
headset through [OpenXR](https://www.khronos.org/openxr/). It is a new version of the
[original Quake VR](https://github.com/vittorioromeo/quakevr/tree/master), which ran on QuakeSpasm-Spiked and
SteamVR's OpenVR (see [Compared with the original Quake VR](#compared-with-the-original-quake-vr)).

> **Status:** in active development. It is playtested mostly on a Meta Quest 3 through Virtual Desktop. Other
> headsets and controllers work through OpenXR but have had less testing. Expect rough edges, and please report
> what you find.

**Contents:** [Features](#features) · [Compared with the original](#compared-with-the-original-quake-vr) ·
[Installation](#installation) · [First steps and basic tweaking](#first-steps-and-basic-tweaking) ·
[Documentation](#documentation) · [Building from source](#building-from-source) ·
[Credits and licence](#credits-and-licence)

## Features

A short list. [docs/FEATURES.md](docs/FEATURES.md) explains each feature and how to use it.

### Weapons and combat

- **Weapons in your hands:** every weapon from Quake and both mission packs, one in each hand. The guns have
  modelled grips, iron sights, a glowing ammo screen and, where the gun has a second ammo type, a button on top that
  switches to it.
- **Two-handed aiming:** grip the foregrip with your other hand. An optional virtual stock steadies your aim when
  your hand is near your shoulder. Heavy weapons lag your hand a little, and hands and barrels stop at walls.
- **Holsters** at your hips, on your chest and behind your shoulders: let go of a weapon there to put it away and
  grip there to draw it. You reload by bringing the weapon to a holster, or by flicking the super shotgun open.
- **Throwing and physics:** thrown weapons, boxes, gibs and heads are rigid bodies with true-scale gravity. They
  spin, bounce, float in water and come to rest. The throw is read from your hand as in Half-Life: Alyx.
- **Force grab:** point an open hand at a weapon or item, pull the trigger, flick your wrist, and catch it as it
  flies to you.
- **Melee:** punches, axe and sword swings, and pistol-whipping with any gun. Knights drop their swords for you to
  take. You can parry with a weapon held level across your body or with your forearms crossed, bash with a guard,
  headbutt, and bat enemy projectiles back.
- **Positional damage** (headshots, arm and leg shots) and **knockback** in both directions.
- **Hand interaction:** pick up weapons, ammo, health, backpacks, gibs and heads by hand. Carry boxes, stash them at
  a holster, or throw them.
- **Gore** that you can turn up or down: blood sprays on walls, gibs that stick to ceilings and drip, pools under
  corpses, and wounds that drip blood.
- **Haptics:** hits felt on the side they come from, explosions rumble, and a heartbeat at low health.

### Movement and comfort

- Smooth locomotion towards your head or your off hand, smooth or snap turning, and optional teleport.
- Room-scale play: walking around your room moves you in the game, with collision. You can crouch, jump for real,
  and lean over railings.
- **Swimming** with arm strokes.
- **Ledge grab** (experimental): hang from a ledge and pull yourself up.
- Left-handed mode, height calibration and world scale.

### Body and immersion

- **Full body:** your arms reach your hands, and your legs walk and step round as you turn. You can pick one of
  three builds. Your body shows the armour you wear, your wounds and your powerups.
- **Wrist gadget HUD:** health, armour and ammo on a CRT screen strapped to your forearm. You read it like a
  watch, and messages float above it. The classic status bar on a hand is still available.
- **Chest flashlight:** a real shadow-casting spotlight. Switch it with the trigger, or take it in your hand.
- **Hands** whose fingers curl with the trigger, the grip and your thumb.
- **VR menus** that you point at with a laser. Settings pages preview their changes live.
- **Voice notes:** hold a button with your hand at your mouth to record feedback. Each note is saved with a
  screenshot of where you were.

### Graphics

- **Relit maps (optional, [made on your PC](docs/RELIGHTING.md)):** Quake's maps relit with ericw-tools, with
  ambient occlusion, coloured light, and lamps that light their rooms. **Water, slime and teleporters become
  see-through.**
- **A darker, moodier look, like DarkPlaces:** strong coloured dynamic lights, bloom, and a sheen on surfaces.
- **Real-time shadows** from explosions, rockets, your muzzle flash and the map lights near you. Monsters and your
  own body cast them. Five presets run from Off (Quake) to Ultra.
- **Model lighting:** models are shaded from the map's own lights, with directional ambient, a rim light, and
  reflections on your weapons.
- **Surface detail:** bump maps (made from the textures, or the texture pack's own), the baked light's real
  direction (deluxemaps), parallax, and detail textures up close.
- **Water and liquids:** waves that move the surface, reflection, refraction, caustics, splashes, ripples,
  shoreline foam, heat haze over lava, and fog and a wobble under water.
- **Effects:** textured particles (smoke, sparks, blood, explosions), soft particles, decals, flickering torch
  lights, glowing projectiles.
- Tone mapping, colour grades and dither. Anti-aliasing, a render-scale setting, and support for HD texture packs.

### Compatibility

- **Quake and both mission packs** (Scourge of Armagon, Dissolution of Eternity) in one game. They are found
  automatically and picked from the start hub.
- **Custom maps** run with Quake VR's gameplay. **Other mods** run in a compatibility mode: you aim with your hand,
  but you have no off-hand weapons or holsters.
- **Ironwail's strengths:** fast on huge modern maps, a Maps and Mods menu, and flat-screen play (`vr_enabled 0`).
- **Multiplayer and bots** (FrikBot), carried over from the original.

## Compared with the original Quake VR

The [original Quake VR](https://github.com/vittorioromeo/quakevr/tree/master) (the `master` branch, up to v0.0.7 beta)
was built on QuakeSpasm-Spiked and OpenVR. This version keeps its gameplay and QuakeC and rebuilds the rest.

| | Original (QSS + OpenVR) | This version (Ironwail + OpenXR) |
|---|---|---|
| Engine | QuakeSpasm-Spiked (2020), converted to C++ | Ironwail 0.8.2. The VR code is a separate module. |
| Headset API | OpenVR (SteamVR only) | OpenXR: SteamVR, Virtual Desktop (VDXR), Meta, and other runtimes |
| Controls | SteamVR Input bindings | Controller buttons are ordinary Quake keys: rebind them in Key Setup or the console |
| Installation | A separate folder: copy Quake's pak files into it (renamed, for the mission packs) | A `quakevr` folder in your Quake folder. The mission packs are found automatically. |
| Body | A floating torso | A full body with arms and legs (IK) |
| HUD | Status bar on a hand | Wrist gadget with a CRT screen, or the status bar |
| Lighting | Quake's lightmaps (optional external `.lit` files) | Relit maps, real-time shadows, per-pixel lights, bump maps, bloom, tone mapping |
| New in combat | | Parry, bash, headbutt, knights' swords, batting projectiles back, carrying boxes and gibs, gore |
| New in movement | | Swimming with strokes, leaning, ledge grab (experimental), chest flashlight |
| Other mods | Had to be ported | Run in a compatibility mode |
| Not (yet) carried over | | The virtual keyboard, Index per-finger tracking (fingers follow the buttons instead) |

Saves and configs from the original version don't carry over.

## Installation

The short version is below. [docs/INSTALL.md](docs/INSTALL.md) has the details, the optional extras and
troubleshooting.

**You need:**

- A Windows 10 or 11 PC (x64) that can run PC VR. The graphics card needs OpenGL 4.3.
- A PC VR headset and an **OpenXR runtime**: SteamVR, Virtual Desktop (its VDXR runtime), Meta Quest Link, or
  another.
- **Quake** ([Steam](https://store.steampowered.com/app/2310/QUAKE/) or GOG). Quake VR uses the original game data:
  the `id1` folder with `PAK0.PAK` and `PAK1.PAK`. The Steam version has it next to the 2021 re-release.
- The [Microsoft Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)
  (x64), if it isn't installed already.

**Steps:**

1. **Download Quake VR from [vittorioromeo.com](https://vittorioromeo.com)**, where the download page will be
   listed: the package, `QuakeVR.zip`, is too large for a GitHub release. Or [build it yourself](docs/BUILDING.md).
2. Unzip it **into your Quake folder** (the folder containing `id1`), for example
   `C:\Program Files (x86)\Steam\steamapps\common\Quake`. This adds `ironwail.exe`, its DLLs, `QuakeVR.bat` and the
   `quakevr` game folder. If `hipnotic` and `rogue` are there too, the mission packs are used automatically.
3. Start your OpenXR runtime (SteamVR, or Virtual Desktop), and put the headset on.
4. Run **`QuakeVR.bat`**. It runs `ironwail.exe -game quakevr`, so a shortcut with those arguments works just as
   well.

You start in the **VR hub**. Pick Quake or one of the mission packs there and step into the portal. The hub also
leads to the tutorial and the firing range.

**Optional extras** (all in [docs/INSTALL.md](docs/INSTALL.md)):

- **HD textures:** the Quake Revitalization Project (QRP) map textures, from the
  [QRP Archive on ModDB](https://www.moddb.com/addons/quake-revitalization-project-archive). Their `textures`
  folders go in `id1`, `hipnotic` and `rogue`.
- **Relit maps and see-through water:** made on your own PC from your copy of Quake, with the script in
  `quakevr\tools`, ericw-tools and the VisPatch data. id Software's maps can't be redistributed, so they aren't in
  the package. [docs/RELIGHTING.md](docs/RELIGHTING.md) has the steps. It takes about a minute.
- **Transcribing voice notes** with Whisper. This is for playtesters.

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
| Menu button | Menu (on Index controllers, and Quest controllers under SteamVR: the left B/Y button) | |

Every button can be rebound in **Options > Key Setup** or with `bind` in the console. They are named like gamepad
keys (`RTRIGGER`, `LSHOULDER`, `ABUTTON`...). See [docs/SETTINGS.md](docs/SETTINGS.md#controls-and-bindings).

**VR Settings** (menu > Options > VR Settings, or `menu_vr` in the console) has the settings most people need. Point
at it with the laser and pull the trigger, or use the sticks: A selects, B goes back. Worth doing first:

1. **Height:** stand straight and pick *Set Height Now* (Body section).
2. **Comfort:** *Turning* (smooth, or snap 30/45/90 degrees), *Turn Speed*, *Move Towards* (head or off hand), and
   *Teleport*.
3. **Weapons:** if guns don't point where your controller points, adjust *Gun Angle* and *Off Hand Angle*. *Weapon
   Grip* set to *Sticky* keeps weapons in your hand without holding the grip.
4. **Body:** *Body* (off, torso and arms, or full body), *Build*, and the *Torso*, *Legs* and *Shoulders* offsets
   if your body looks misplaced when you look down.
5. **Display:** *HUD* (wrist gadget or status bar), menu distance and scale, and the desktop mirror.
6. **Headset:** the *OpenXR Runtime* (system default, Virtual Desktop's VDXR, or SteamVR), *Render Scale*, and
   *Hide Lens Corners*.

**Performance:** the shipped settings are tuned for a fast PC. If frames drop, open *Advanced VR Options >
Graphics*, pick a lower *Preset* (Off, Low, Medium, High or Ultra), and lower *Render
Scale* below 1.

**More:** *Advanced VR Options* at the bottom of VR Settings opens about twenty more pages: gameplay, body, gore,
wrist gadget, throwing, force grab, swimming, locomotion, graphics, holsters and more. *Weapon Offsets (Held
Weapon)* adjusts how the weapon in your hand sits. [docs/SETTINGS.md](docs/SETTINGS.md) covers all of these.

**Starting over:** *Options > Reset to defaults* restores the shipped settings and bindings. Your settings are saved
in `quakevr\ironwail.cfg`: delete it for a completely fresh start.

## Documentation

For players:

- [docs/INSTALL.md](docs/INSTALL.md): installation in detail, OpenXR runtimes, HD textures, relit maps, mission
  packs and mods, performance, troubleshooting.
- [docs/RELIGHTING.md](docs/RELIGHTING.md): relighting your own copy of Quake's maps, for the relit look and
  see-through water, step by step.
- [docs/FEATURES.md](docs/FEATURES.md): every feature, and how to use it.
- [docs/SETTINGS.md](docs/SETTINGS.md): advanced settings, all the menu pages, console variables, config files.

For developers:

- [docs/BUILDING.md](docs/BUILDING.md): building the engine, the QuakeC and the release package, and the tool
  scripts.
- [docs/vr-port/](docs/vr-port/): the port's design notes. [PLAN.md](docs/vr-port/PLAN.md) covers the design and
  status, [TESTING.md](docs/vr-port/TESTING.md) is the playtest guide, and the `ROUND*.md` files hold the notes of
  each feedback round. There are also deep dives into graphics, lighting, throwing and body IK.

## Building from source

Build `Windows\VisualStudio\ironwail.sln` (Visual Studio 2022, Release | x64). Then run
`Windows\package-quakevr.ps1 -Fteqcc <path to fteqcc64.exe>`: it compiles the QuakeC and puts the package in
`dist\QuakeVR` and `dist\QuakeVR.zip`. [docs/BUILDING.md](docs/BUILDING.md) has the details. VR needs Windows x64.
The other platforms build with a mock headset only, for development.

## Credits and licence

Quake VR is by **Vittorio Romeo**. It builds on:

- **[Ironwail](https://github.com/andrei-drexler/ironwail)** by Andrei Drexler and contributors: a fast
  [QuakeSpasm](https://sourceforge.net/projects/quakespasm/) fork, which descends from FitzQuake and id Software's
  Quake. Ironwail's own features, such as playing the 2021 re-release content, the Mods menu and its HUD styles,
  are described in [its README](https://github.com/andrei-drexler/ironwail#readme).
- The **original Quake VR** and the ports it grew from: Fishbiter's and Zackin5's OpenVR ports, Dominic
  Szablewski's Oculus port, and Spike's QuakeSpasm-Spiked.
- Hand models by [CrazyHairGuy](https://www.crazyhairguy.com/). Weapon models are based on
  [Authentic Model Improvements](https://github.com/NightFright2k19/authmdl).
- The OpenXR SDK (Khronos), SDL2, GLM, FTEQCC, and ericw-tools.

The full list, with the research and techniques used, is in [docs/vr-port/CREDITS.md](docs/vr-port/CREDITS.md).

Quake VR is free software under the **GNU General Public License v2** ([LICENSE.txt](LICENSE.txt)), like the Quake
engine it comes from. Quake's game data (maps, models, sounds, textures) belongs to id Software and isn't included:
you need your own copy of Quake.
