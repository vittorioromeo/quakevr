# Installing Quake VR

The [README](../README.md#installation) has the short version. This page covers each step in more detail, the
optional extras, and what to do when something goes wrong.

- [Requirements](#requirements)
- [Your copy of Quake](#your-copy-of-quake)
- [Installing the package](#installing-the-package)
- [Starting the game](#starting-the-game)
- [OpenXR runtimes: SteamVR or Virtual Desktop](#openxr-runtimes-steamvr-or-virtual-desktop)
- [Mission packs](#mission-packs)
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
- **The Microsoft Visual C++ Redistributable for Visual Studio 2015-2022 (x64).** Many games install it already. If
  `ironwail.exe` complains about a missing `VCRUNTIME140.dll` or `MSVCP140.dll`, install it from
  [Microsoft](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist).

## Your copy of Quake

Quake VR needs the game data of the original Quake: the `id1` folder with `PAK0.PAK` and `PAK1.PAK`.

- **Steam:** the Quake folder (usually `C:\Program Files (x86)\Steam\steamapps\common\Quake`) has the original data
  in `id1`, the mission packs in `hipnotic` and `rogue`, and the 2021 re-release in `rerelease`. Install Quake VR
  into this folder. Quake VR uses the original data; the re-release folder isn't needed.
- **GOG and other copies:** any Quake folder with `id1\PAK0.PAK` and `id1\PAK1.PAK` works the same way.
- **Music:** if you hear no music, copy the soundtrack into `id1\music`. The Steam version has it in
  `rerelease\id1\music`.

### Only the 2021 re-release?

The re-release's data works too, with the campaign and both mission packs. Its folder is the one with `QuakeEX.kpf`
and an `id1\pak0.pak` of about 220 MB. On Steam, that's `Quake\rerelease`. Don't unzip Quake VR into this folder,
because the package's `SDL2.dll` would replace the re-release's own. Use one of these instead:

- Copy the re-release's `id1`, `hipnotic` and `rogue` folders into your Quake VR folder, then run `QuakeVR.bat`.
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
- **Dimension of the Past** (`dopa`) plays with Quake VR's gameplay. If you copied the folders, copy `dopa` too.
  Start `ironwail.exe -game dopa -game quakevr`, in that order (plus the `-basedir` options, if you use them), then
  type `map e5start` in the console.
- **Dimension of the Machine** (`mg1`) has its own QuakeC. `QuakeVR.bat -game mg1` runs it in
  [compatibility mode](#custom-maps-and-mods).

## Installing the package

**Download Quake VR from [vittorioromeo.com](https://vittorioromeo.com)**, where the download page will be listed.
The package is too large for a GitHub release. You can also [build it yourself](BUILDING.md).

The package (`QuakeVR.zip`) contains:

| File | What it is |
|---|---|
| `ironwail.exe`, `ironwail.pak` | the engine, with Quake VR built in |
| `openxr_loader.dll`, `SDL2.dll` and the audio codec DLLs | libraries the engine needs |
| `QuakeVR.bat` | the launcher: runs `ironwail.exe -game quakevr` |
| `README-QuakeVR.txt` | a quick-start note |
| `quakevr\` | the game folder: Quake VR's QuakeC (`progs.dat`), models, sounds, textures, maps (hub, tutorial, firing range) and configs |
| `quakevr\tools\` | the scripts that relight your own copy of Quake's maps (see [RELIGHTING.md](RELIGHTING.md)) |

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

If `hipnotic` (Scourge of Armagon) and `rogue` (Dissolution of Eternity) are in your Quake folder, Quake VR uses
them automatically. There's nothing to copy or rename. In the VR hub, press the button for the campaign you want
(Quake, SoA or DoE) and step into the portal. The Steam version of Quake includes both packs.

Don't start the mission packs with `-game hipnotic` or `-game rogue`: that would run their own QuakeC, without Quake
VR's gameplay. Quake VR's QuakeC already contains all three campaigns.

## Custom maps and mods

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

To install it:

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

It takes about a minute on a fast PC and writes about 210 MB to `quakevr\relit\`.

**In the game:** the relit maps are used from the next map you load. *Advanced VR Options > Graphics > Relit Maps*
switches between the relit and the original lighting. Water on the relit maps is see-through (*Advanced VR Options >
Transparency > Water Alpha*, 0.6 by default; other maps keep opaque water whatever it says). Lava stays opaque.

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
most useful thing to attach to a report.

| Problem | What to try |
|---|---|
| Nothing in the headset | Look for the `VR:` lines in the console: they name the OpenXR call that failed. Check that the runtime is running, then `vr_restart`. Try another runtime in VR Settings > Headset. |
| Guns point the wrong way, hands misplaced | Adjust *Gun Angle* and *Off Hand Angle* in VR Settings. *Weapon Offsets (Held Weapon)* moves a single weapon in the hand. |
| Double vision, wrong scale | Send the output of `vr_status`, and a screenshot of the desktop mirror with `vr_mirror 2` (both eyes). |
| Too tall, too short, floor in the wrong place | *Set Height Now* while standing straight, *World Scale*, *Floor Offset*. |
| Water isn't see-through | Needs the relit maps made with the VisPatch files, and *Transparency > Water Alpha* below 1 (0.6 by default). See [RELIGHTING.md](RELIGHTING.md#troubleshooting). |
| No sound | Check the Windows output device (your headset's audio) and Ironwail's volume options. |
| Slow or stuttering | See [Performance](#performance). |
| A crash | `qconsole.log` up to the crash, and what you were doing. |
| Settings in a mess | *Options > Reset to defaults*, or delete `quakevr\ironwail.cfg` for a completely fresh start. |

Report bugs and ideas on the [GitHub issues page](https://github.com/vittorioromeo/quakevr/issues), with the log,
your headset and runtime, and, for performance, a profile or memory log.

## Updating and uninstalling

- **Updating:** unzip the new package over the old one. Your settings (`quakevr\ironwail.cfg`), saves, relit maps
  and notes are kept. Settings whose default changed are updated once, unless you changed them yourself. Running
  the [relight](RELIGHTING.md) again after an update is quick: it only relights the maps whose light the update
  changes.
- **Uninstalling:** delete `quakevr`, `ironwail.exe`, `ironwail.pak`, `QuakeVR.bat`, `README-QuakeVR.txt` and the
  DLLs from the Quake folder. Check first that nothing else in that folder uses the DLLs.
