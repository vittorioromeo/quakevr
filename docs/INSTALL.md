# Installing Quake VR

The [README](../README.md#installation) has the short version. This page covers each step in more detail, the
optional extras, and what to do when something goes wrong.

- [Requirements](#requirements)
- [Your copy of Quake](#your-copy-of-quake)
- [Installing the package](#installing-the-package)
- [Starting the game](#starting-the-game)
- [OpenXR runtimes: SteamVR or Virtual Desktop](#openxr-runtimes-steamvr-or-virtual-desktop)
- [Mission packs](#mission-packs)
- [Official campaigns](#official-campaigns)
- [Custom maps and mods](#custom-maps-and-mods)
- [HD textures (QRP)](#hd-textures-qrp)
- [Relit maps and see-through water](#relit-maps-and-see-through-water)
- [Voice notes](#voice-notes)
- [Performance](#performance)
- [Troubleshooting and bug reports](#troubleshooting-and-bug-reports)
- [Updating and uninstalling](#updating-and-uninstalling)

## Requirements

- **Windows 10 or 11, 64-bit.** VR works only in the Windows x64 build. The Linux and macOS builds have no OpenXR
  support yet.
- **A graphics card with OpenGL 4.3**, as Ironwail needs. For VR, get the fastest card you can. The shipped
  settings are tuned on an RTX 4090. The [graphics presets](#performance) scale down a long way.
- **A PC VR headset and an OpenXR runtime.** Quake VR is tested mostly on a Meta Quest 3 through Virtual Desktop.
  Controller bindings are included for Oculus/Meta Touch (and Touch Plus), Valve Index, HTC Vive wands, Windows
  Mixed Reality controllers, and OpenXR's generic "simple controller".
- **The Microsoft Visual C++ Redistributable for Visual Studio 2015-2022 (x64), version 14.44 or later.** Quake VR is
  built with Visual Studio 2022 17.14 (MSVC toolset 14.44.35207), and needs a runtime at least that new: the one
  that comes with it is 14.44.35112. Many games install the redistributable, but often an older one.
  - **Older than 14.40 (Visual Studio 2022 17.10), the game crashes as it starts,** with no message or with an
    access violation in `MSVCP140.dll`: the 17.10 standard library changed `std::mutex`, and an older
    `MSVCP140.dll` doesn't know it. A missing `VCRUNTIME140.dll` or `MSVCP140.dll` means the same.
  - **To check:** Settings > Apps > Installed apps, "Microsoft Visual C++ 2015-2022 Redistributable (x64)": its
    version is the last part of the name (e.g. 14.44.35211).
  - **To install or update it:** the latest x64 installer is always at
    [aka.ms/vs/17/release/vc_redist.x64.exe](https://aka.ms/vs/17/release/vc_redist.x64.exe) (Microsoft's
    permanent link; [the page listing it](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)).
    Installing it over an older one updates it.
  - A future Quake VR installer will check the version and offer the update itself.

## Your copy of Quake

Quake VR needs the game data of the original Quake: the `id1` folder with `PAK0.PAK` and `PAK1.PAK`.

- **Steam:** the Quake folder (usually `C:\Program Files (x86)\Steam\steamapps\common\Quake`) has the original data
  in `id1`, the mission packs in `hipnotic` and `rogue`, and the 2021 re-release in `rerelease`. Install Quake VR
  into this folder. Quake VR can use the original data. The rerelease folder also supplies owned newer campaigns and their current language tables; it is optional for the original campaign.
- **GOG and other copies:** any Quake folder with `id1\PAK0.PAK` and `id1\PAK1.PAK` works the same way.
- **Music:** if you hear no music, copy the soundtrack into `id1\music`. The Steam version has it in
  `rerelease\id1\music`.

### Only the 2021 re-release?

The re-release's data works too, with the campaign and both mission packs. Its folder is the one with `QuakeEX.kpf`
and an `id1\pak0.pak` of about 220 MB. On Steam, that's `Quake\rerelease`. Don't unzip Quake VR into this folder,
because the package's `SDL2.dll` would replace the re-release's own. Use one of these instead:

- Copy the re-release's `id1` folder into your Quake VR folder, plus `hipnotic` and/or `rogue` if you want those
  campaigns, then run `QuakeVR.bat`.
- Or leave the data where it is, and name both folders on the command line:
  `QuakeVR.bat -basedir "<re-release folder>" -basedir "<Quake VR folder>"`. Don't end either path with a `\`.

What changes with the re-release data:

- **Maps:** the re-release has its own versions of Quake's and Scourge of Armagon's maps. They're relit with
  coloured light, bounced light and ambient occlusion, and their water is already see-through.
- **Relit maps:** run `relight_maps.py` with `--quake "<re-release folder>"`. The relit maps still add Quake VR's
  bump shading and the light grid. They come out brighter than relit original maps, because the re-release's maps
  ask for bounced light. `--vis-dir` still gives Dissolution of Eternity's maps see-through water. For the other
  maps the script reports that the patch "is for another version of the map" and leaves them alone, which is fine:
  their water is see-through already. Relit maps replace a map whichever version they were made from, so make them
  from the data you play with.
- **Additional official campaigns:** the re-release includes Dimension of the Past (`dopa`), Dimension of the
  Machine (`mg1`) and Dawn of the Machine (`mg3`). Their installed data is detected separately from native gameplay
  readiness. See [Official campaigns](#official-campaigns) before starting one.

## Installing the package

**Download Quake VR from [vittorioromeo.com](https://vittorioromeo.com)**, where the download page will be listed.
The package is too large for a GitHub release. You can also [build it yourself](BUILDING.md).

The package (`QuakeVR.zip`) contains:

| File | What it is |
|---|---|
| `ironwail.exe`, `ironwail.pak` | the engine, with Quake VR built in |
| `ironwail.pdb` | the engine's debug symbols: a crash report names the functions on the stack only with it there |
| `openxr_loader.dll`, `SDL2.dll` and the audio codec DLLs | libraries the engine needs |
| `QuakeVR.bat` | the launcher: runs `ironwail.exe -game quakevr` |
| `README-QuakeVR.txt` | a quick-start note |
| `quakevr\` | the game folder: Quake VR's QuakeC (`progs.dat`), models, sounds, textures, maps (hub, tutorial, firing range) and configs |
| `quakevr\tools\` | the scripts that relight your own copy of Quake's maps, and ericw-tools' `light.exe` (in `ericw-tools\`, GPL-3) for the in-game relighting (see [RELIGHTING.md](RELIGHTING.md)) |

Unzip it **into your Quake folder**, so that `quakevr` sits next to `id1`:

```
Quake\
    id1\            PAK0.PAK, PAK1.PAK (yours), and textures\ if you install HD textures
    hipnotic\       Scourge of Armagon (if you have it)
    rogue\          Dissolution of Eternity (if you have it)
    quakevr\        Quake VR's game folder
    ironwail.exe
    QuakeVR.bat
    ...
```

Quake VR doesn't change anything in `id1`, `hipnotic` or `rogue`. Your settings, saves, screenshots and notes all
go in `quakevr`.

If you'd rather keep Quake VR somewhere else, put `ironwail.exe` and its files in any folder and point it at your
Quake folder: `ironwail.exe -basedir "C:\...\Quake" -game quakevr`.

## Starting the game

1. Start your OpenXR runtime (SteamVR, or Virtual Desktop connected to the PC) and put the headset on.
2. Run `QuakeVR.bat`, or a shortcut to `ironwail.exe -game quakevr`. Anything you add to `QuakeVR.bat`'s command
   line goes to the engine. For example, `QuakeVR.bat -condebug` writes the console to `qconsole.log` in the Quake
   folder.

`quakevr\quakevr.cfg` turns VR on. The desktop window shows a mirror of the left eye, and the game starts in the
**VR hub**. If no headset is found, the game plays on the monitor instead. The console (the `~` key on the desktop
keyboard) then says why VR didn't start:

- `vr_restart` retries, for example after putting the headset on or starting the runtime.
- `vr_status` prints the runtime, the tracking state and the eye resolution.
- `vr_enabled 0` plays on the monitor on purpose, and `vr_enabled 1` goes back to VR.

## OpenXR runtimes: SteamVR or Virtual Desktop

Quake VR uses whichever OpenXR runtime you pick in **VR Settings > Headset > OpenXR Runtime**. Switching restarts VR
without restarting the game.

- **System default:** the runtime that Windows has marked as active. This is usually the one set in SteamVR's or
  the Meta app's settings, or whatever the `XR_RUNTIME_JSON` environment variable points to.
- **Virtual Desktop (VDXR):** Virtual Desktop's own OpenXR runtime. It skips SteamVR entirely, which can give
  smoother frame pacing on a Quest. In Virtual Desktop's settings, keep *Emulate Index controllers* **off**. The
  first time you use it, check that your guns point where your controllers point (see *Gun Angle* below).
- **SteamVR:** SteamVR's runtime, for any headset SteamVR drives, including a Quest over Virtual Desktop or Link.
- A specific runtime's manifest: set `vr_xr_runtime 3` and `vr_xr_runtime_json "<path to the runtime's .json>"`
  in the console.

**Guns at the wrong angle?** Different runtimes report the controllers' pose slightly differently. Adjust *Gun
Angle* and *Off Hand Angle* in VR Settings (the *Hand/Gun Calibration* page has the yaw too).

## Mission packs

**No expansion is required for Quake's campaign, the VR hub, tutorial or firing range.** Scourge of Armagon
(`hipnotic`) and Dissolution of Eternity (`rogue`) are independent optional packs. Each enables its own campaign,
weapons, monsters and items; either works without the other. Pack weapons/items and their random drops are disabled
when their data is unavailable. Rogue is also needed for lava nails made by shooting through a torch's flame.

Put the complete owned pack data in its folder alongside `id1`. Quake VR validates the pack's own required maps,
models and sounds before mounting it; an empty directory or a VR replacement view model does not establish that the
pack is installed. Missing, incomplete or corrupt packs are reported at startup. Their hub buttons are marked
*unavailable*, and selecting their campaign is blocked with a message explaining which data to restore. Run
`vr_pack_status`, or choose *Debug > Reports > Mission Pack Status*, to see each pack's status. Restore the pack
from your owned copy when validation reports missing files or a damaged archive.

In the hub, press an available campaign's button (Quake, SoA or DoE) and step into the portal. Keep the same pack
installation when restoring a save: changing the set of packs changes the saved model indices, so an incompatible
save is rejected before loading. Saves from the earlier merged VR progs require both packs.

Use the hub or Official Campaigns selector to start an owned campaign with Quake VR's gameplay. The merged VR
QuakeC contains Quake, Scourge of Armagon and Dissolution of Eternity; selecting one keeps VR's progs active.

## Official campaigns

Open **Single Player > Official Campaigns**, **VR Settings > Official Campaigns**, or the hub's campaign board link.
The selector shows Quake, the two mission packs, Dimension of the Past (`dopa`), Dimension of the Machine (`mg1`)
and Dawn of the Machine (`mg3`). Each entry reports missing data, incomplete/corrupt data, or installed data with
its native gameplay readiness. **Dimension of the Past is ready for native single-player VR**, including authored
normal/secret routes, deferred monsters, fog/exploding geometry, VR inventory carry/save/reset and readable
completion text/menu. Dopa coop context/join/respawn behavior is not accepted; ordinary launch requires `coop 0`, `deathmatch 0`, and `maxplayers 1`.
**Dimension of the Machine and Dawn of the Machine remain in progress**; ordinary selection refuses them until
their gameplay and progression are ready. Installed maps alone do not establish support.

For an owned Steam installation, Quake VR checks the original Quake folder and its `rerelease` folders. It also
checks explicit `-basedir` roots and the existing Steam/GOG discovery paths. No expansion download or separate
installer is needed when complete data is found. For other copies, keep each complete owned pack folder alongside
`id1`, or use the two `-basedir` options shown above. Detection does not copy assets or write into the borrowed
installation. The last explicit base has priority; a damaged copy there is reported rather than silently replaced
by another installation.

The newer campaigns also need current language tables from the owned rerelease `id1` data; their expansion
PAKs do not contain them. Quake VR borrows only these tables from the configured roots or enabled Steam/GOG
discovery, without adding the borrowed `id1` maps/models to your campaign paths. `-nosteam`, `-nogog`, and
`-noepic` disable the corresponding store lookup; explicit `-basedir` roots and their rerelease subfolders still
work. Keep the writable Quake VR folder as the last `-basedir` in the command shown above.

Custom local translations have priority. Missing, empty, or untranslated entries are filled from owned tables
in that language, then owned English. An old/incomplete table alone blocks ordinary launch and reports which
identifiers are missing; point `-basedir` at updated owned rerelease data or enable store lookup. No commercial
language text is included in the VR package.

Run `vr_campaign_status`, or **Debug > Reports > Official Campaign Status**, to inspect resolved folders,
language coverage, and readiness. Language-load messages show actual table paths. `loc_probe $identifier
[arguments...]` previews resolved/formatted text and prints the specific table that supplied it; **Debug >
Reports > Dopa Finale Text** previews the ending. `vr_campaign_select <folder>` uses the same availability checks as the selector. `vr_campaign_hub`
returns to the VR hub and restores the base campaign paths. Saves restore their campaign context, but still require
the same optional mission-pack set described above. Newer expansion save schemas may change during development.

Official campaign arguments such as `QuakeVR.bat -game mg1` now keep VR's merged progs active; they are not a way
to opt into the pack's original gameplay code. Developer-only `vr_campaign_native <folder>` bypasses the readiness
gate for testing and warns that gameplay/progression is incomplete. Use the normal selector for supported play.
Current port coverage and outstanding mechanics are recorded in the [expansion audit](vr-port/EXPANSIONS.md).

## Custom maps and mods

- **The Map Library** (the main menu's *Map Library*, or the menu corner's *Maps* button) lists
  [Quaddicted](https://www.quaddicted.com/)'s archive of custom maps (its index is fetched when the game starts and
  cached in `cache\maps_index.txt`). Type to search; the bar under the list sorts (rating, newest, oldest, size,
  title) and filters them (type, size, rating, installed only). Pick one to read its description, then **Install**:
  it is downloaded and unpacked in the background, into its own folder `qvr_addons\<id>\` in the Quake folder, and
  **Play** starts it. An installed package also has **Uninstall** and **Reinstall** (press twice to confirm).
  Packages that bring their own `progs.dat` are left out, since Quake VR runs its own gameplay
  (`vr_maps_allow_progs 1` lists them). The downloads are kept in `cache\maps\`, at most `vr_maps_cache_mb` (512) MB;
  `vr_maps_fetch 0` (or `-nomapindex`) stops the start-up fetch. The console has the same: `maps_list`, `maps_info`,
  `maps_install`, `maps_play`, `maps_installed`, `maps_uninstall`, `maps_status`, `maps_cancel`, `maps_fetch`.
- **A map without its own `progs.dat`:** put its `.bsp` in `quakevr\maps` (or `id1\maps`), then load it with
  `map <name>` in the console or from Ironwail's Maps menu. For a map pack in its own folder, run
  `QuakeVR.bat -game <folder>`: Quake VR's gameplay stays, and the folder's maps, textures and sounds are added on
  top.
- **A mod with its own `progs.dat`** (Arcane Dimensions, Copper, Quoth, ...): `QuakeVR.bat -game <mod>` runs it in a
  **compatibility mode**. You get the headset, tracked hands and body, the wrist gadget, movement, room-scale,
  teleport and Quake VR's particles. The mod's weapons fire from the gun in your main hand, and your hand aims them.
  Quake VR's own gameplay (off-hand weapons, holsters, throwing, force grab, melee, hand pickups) isn't available,
  and items are picked up by walking over them. The details are in
  [vr-port/MODS.md](vr-port/MODS.md).

## HD textures (QRP)

Quake VR looks best with high-resolution replacement textures. It uses the same texture packs as other Quake engines
based on QuakeSpasm: images in a `textures` folder, looked up in `textures\<map name>\` first, then `textures\`, in
the game folders (`quakevr`, the mission packs, then `id1`). They get smooth filtering, and Quake VR makes bump maps
from them. A pack's own normal maps (`<name>_norm`) are used as they are. Glow images (`<name>_luma`) glow, and the
relight script lights rooms with them.

The author plays with the **Quake Revitalization Project (QRP)** map textures, for Quake and both mission packs.
QRP was completed in 2016. Its own site, [qrp.quakeone.com](http://qrp.quakeone.com/), still lists the packs, but
its download links no longer deliver the files. Get them from the
**[Quake Revitalization Project Archive](https://www.moddb.com/addons/quake-revitalization-project-archive)** on
ModDB instead: `QuakeRevitalizationProject.7z` (1.26 GB), a complete compilation of every QRP release, mission packs
included. A mirror will be available on [vittorioromeo.com](https://vittorioromeo.com).

**The easy way:** download `quakevr-hq-textures-png-2026-10-03.zip` from the
[HQ texture pack (PNG)](https://github.com/vittorioromeo/quakevr/releases/tag/textures-2026-10-03) release and extract
it into your Quake folder. It is the author's installed QRP pack, converted losslessly to PNG, for Quake and both mission
packs ([vr-port/TEXTURES.md](vr-port/TEXTURES.md): contents, credits, licence). Then see step 3 below.

To install it from the QRP archive instead:

1. Extract the archive with [7-Zip](https://www.7-zip.org/). You need the **map textures** packages:
   `QRP_map_textures_v.1.00.pk3` (Quake), `QRP_SoA_map_textures_add-on_v.1.00.pk3` (Scourge of Armagon) and the
   Dissolution of Eternity map textures. The normal-map add-ons are optional: Quake VR makes bump maps from the
   textures by itself, and the author plays without them. The item textures are made for other engines and aren't
   needed.
2. Ironwail doesn't read `.pk3` files, so extract them too. A `.pk3` is a zip file: 7-Zip opens it, or rename it to
   `.zip`. Each has a `textures` folder inside. Extract the Quake one into `id1`, so that you get
   `Quake\id1\textures\` with the `.tga` files and the per-map subfolders (`e1m1`, `dm3`, ...), next to `PAK0.PAK`.
   Scourge of Armagon's goes into `hipnotic` (`hipnotic\textures\`), and Dissolution of Eternity's into `rogue`
   (`rogue\textures\`).
3. If you use relit maps, install the textures first, or run the relight again afterwards: it relights the maps the
   textures change by itself. The relight gives textures that glow in QRP but not in Quake's own images (some lamps
   and panels) a light of their own. See [RELIGHTING.md](RELIGHTING.md).

To play without the pack, move its `textures` folders out of `id1`, `hipnotic` and `rogue`.

## Relit maps and see-through water

This is optional, but it changes the look more than anything else. Quake's 1996 lightmaps have no ambient
occlusion, no coloured light and no bounce, so the world looks flat in VR. The relight gives **your own copy** of
every map of Quake and the mission packs soft shadows, dark corners, coloured light, and lamps, light panels and
glowing buttons that light their rooms. The same step makes **water, slime and teleporters see-through**: id's maps
were compiled with liquids as solid walls for visibility, so engines keep their water opaque, and the VisPatch data
files add water-aware visibility. Quake VR loads the relit maps in place of the originals.

id Software's maps can't be redistributed, so you make them yourself, once, with the script in `quakevr\tools\`.
**[RELIGHTING.md](RELIGHTING.md) has the step-by-step guide.** In short, you need Python 3,
[ericw-tools 2.0.0-alpha11](https://github.com/ericwa/ericw-tools/releases/tag/2.0.0-alpha11) and the
[VisPatch data files](https://sourceforge.net/projects/vispatch/files/vispatch%20data/1.0/), and, to get the same
result as the author, the QRP textures installed first. Then, from a Command Prompt in the Quake folder:

```
python quakevr\tools\relight_maps.py --quake "C:\Program Files (x86)\Steam\steamapps\common\Quake" --light "C:\tools\ericw-tools-2.0.0-alpha11-win64\light.exe" --vis-dir "C:\tools\vispatch"
```

It takes about a minute on a fast PC and writes about 210 MB to `quakevr\relit\`. The package's own `light.exe`
(`quakevr\tools\ericw-tools\light.exe`) works for `--light` too.

**Or in the game, without Python:** *VR Settings > Advanced VR Options > Graphics > Relighting* (Menu Detail:
Advanced) relights the map you are in, an episode, a game or every map, in the background while you play, with
brightness sliders. It has no VisPatch step, so on its own it doesn't make water see-through (a map the script made
see-through keeps it). See [RELIGHTING.md](RELIGHTING.md#relighting-in-the-game).

**In the game:** the relit maps are used from the next map you load. *Advanced VR Options > Graphics > Relit Maps*
switches between the relit and the original lighting. Water on the relit maps is see-through (*Advanced VR Options >
Graphics > Transparency > Water Alpha*, 0.3 by default; other maps keep opaque water whatever it says). Lava is
not water-vised by VisPatch, so it looks opaque.

Quake VR's own maps (the hub, tutorial and firing range) are already relit in the package.

## Voice notes

Voice notes (see [FEATURES.md](FEATURES.md#voice-notes)) are saved as `.wav` files in `quakevr\notes\`. To turn
them into text, the repository has `Misc\quakevr\transcribe_notes.py`. It runs Whisper locally, through
[faster-whisper](https://github.com/SYSTRAN/faster-whisper), and writes `quakevr\notes\NOTES.md`: every note with
its transcript, context and screenshot.

```
python -m pip install --user faster-whisper
python Misc\quakevr\transcribe_notes.py --notes "<Quake>\quakevr\notes" [--device cpu|cuda]
```

The Whisper model (`large-v3-turbo` by default) downloads once, the first time. Only new notes are transcribed.

## Performance

In VR, a missed frame is felt. What helps most:

1. **Graphics preset:** *Advanced VR Options > Graphics > Preset*. The presets are *Off
   (Quake)*, *Low*, *Medium*, *High* and *Ultra*. It sets the shadows, lights and model lighting. You can adjust
   single settings afterwards.
2. **Render Scale** (VR Settings > Headset): below 1 renders fewer pixels and upscales them smoothly. Above 1 gives
   smoother edges if the GPU has headroom.
3. **The runtime's resolution:** Quake VR renders at the size the runtime asks for. SteamVR's per-application
   resolution should be 100%. With Virtual Desktop, its quality preset multiplies SteamVR's, so a high preset plus
   SteamVR supersampling can mean over twice the headset's pixels. A lower refresh rate (90 instead of 120 Hz) also
   helps.
4. **Try Virtual Desktop's VDXR runtime** on a Quest: it skips SteamVR's compositor.
5. **Hide Lens Corners** (VR Settings > Headset) skips pixels you can't see. **Anti-aliasing** (Graphics; 4x by default)
   costs GPU time: 2x or off is cheaper.

**Finding out what's slow:** *Graphics > Performance Profile* (`vr_profile 1`) times each part of the frame, on the
CPU and the GPU, and writes `quakevr\profile\profile_<map>_<date>.csv`. `vr_profile 2` also shows the costliest
parts on the wrist gadget.

**If it gets slower the longer you play:** the *Memory Log* (`vr_memstats_log`, on by default) writes a row a minute
to `quakevr\profile\memstats_<date>.csv`, with the game's memory, what it draws, and its frame times next to the
runtime's. `vr_memstats` prints the same in the console. To tell whether the game or the runtime is at fault:
once it has slowed down, restart only Quake VR on the same map. If the frame rate is back, it's the game. If it
only comes back after restarting SteamVR or Virtual Desktop too, it's them.

## Troubleshooting and bug reports

Start the game with `QuakeVR.bat -condebug`. The console goes to `qconsole.log` in the Quake folder, which is the
most useful thing to attach to a report. If the game crashes, it writes `qvr_crash.txt` and `qvr_crash.dmp` in the
folder it was started from: attach both.

Paths under *Advanced VR Options* need *Menu Detail: Advanced* (the last row of every page), and *Debug* pages need
*Developer*.

| Problem | What to try |
|---|---|
| Nothing in the headset | Look for the `VR:` lines in the console: they name the OpenXR call that failed. Check that the runtime is running, then `vr_restart`. Try another runtime in VR Settings > Headset. |
| Guns point the wrong way, hands misplaced | Adjust *Gun Angle* and *Off Hand Angle* in VR Settings. *Weapon Offsets (Held Weapon)* moves a single weapon in the hand. |
| Double vision, wrong scale | Send the output of `vr_status`, and a screenshot of the desktop mirror with `vr_mirror 2` (both eyes). |
| Too tall, too short, floor in the wrong place | *Set Height Now* while standing straight, *World Scale*, *Floor Offset*. |
| Water isn't see-through | Needs the relit maps made with the VisPatch files, and *Transparency > Water Alpha* below 1 (0.3 by default). See [RELIGHTING.md](RELIGHTING.md#troubleshooting). |
| No sound | Check the Windows output device (your headset's audio) and Ironwail's volume options. |
| Slow or stuttering | See [Performance](#performance). |
| A crash | `qconsole.log` up to the crash, and what you were doing. |
| Settings in a mess | *Options > Reset All*, or delete `quakevr\ironwail.cfg` for a completely fresh start. |

Report bugs and ideas on the [GitHub issues page](https://github.com/vittorioromeo/quakevr/issues), with the log,
your headset and runtime, and, for performance, a profile or memory log.

## Updating and uninstalling

- **Updating:** unzip the new package over the old one. Your settings (`quakevr\ironwail.cfg`), saves, relit maps
  and notes are kept. Settings whose default changed are updated once, unless you changed them yourself. Running
  the [relight](RELIGHTING.md) again after an update is quick: it only relights the maps whose light the update
  changes.
- **Uninstalling:** delete `quakevr`, `ironwail.exe`, `ironwail.pak`, `QuakeVR.bat`, `README-QuakeVR.txt` and the
  DLLs from the Quake folder. Check first that nothing else in that folder uses the DLLs.
