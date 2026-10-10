# Installing Quake VR: UNLEASHED

The [README](../README.md#installation) has the short version. This page covers each step in more detail, the
optional extras, and what to do when something goes wrong. Quake VR: UNLEASHED is shortened to **QVR:U** below.

- [Requirements](#requirements)
- [Your copy of Quake](#your-copy-of-quake)
- [Installing with the installer](#installing-with-the-installer)
- [Starting the game](#starting-the-game)
- [Updating and uninstalling](#updating-and-uninstalling)
- [Installing from the zip instead](#installing-from-the-zip-instead)
- [OpenXR runtimes: SteamVR or Virtual Desktop](#openxr-runtimes-steamvr-or-virtual-desktop)
- [Mission packs](#mission-packs)
- [Official campaigns](#official-campaigns)
- [Custom maps and mods](#custom-maps-and-mods)
- [HD textures (QRP)](#hd-textures-qrp)
- [Relit maps and see-through water](#relit-maps-and-see-through-water)
- [Voice notes](#voice-notes)
- [Performance](#performance)
- [Troubleshooting and bug reports](#troubleshooting-and-bug-reports)

## Requirements

- **Windows 10 or 11, 64-bit.** VR works only in the Windows x64 build. The Linux and macOS builds have no OpenXR
  support yet.
- **A graphics card with OpenGL 4.3**, as Ironwail needs. For VR, get the fastest card you can. The shipped
  settings are tuned on an RTX 4090. The [graphics presets](#performance) scale down a long way.
- **A PC VR headset and an OpenXR runtime.** QVR:U is tested mostly on a Meta Quest 3 through Virtual Desktop.
  Controller bindings are included for Oculus/Meta Touch (and Touch Plus), Valve Index, HTC Vive wands, Windows
  Mixed Reality controllers, and OpenXR's generic "simple controller".
- **The Microsoft Visual C++ Redistributable for Visual Studio 2015-2022 (x64), version 14.44 or later.** The
  installer checks it and, when it is missing or older, installs Microsoft's current one for you (Windows asks for
  permission once). You need to care about it only when you [install from the zip](#installing-from-the-zip-instead):
  - QVR:U is built with Visual Studio 2022 17.14 (MSVC toolset 14.44.35207) and needs a runtime at least that new.
    Many games install the redistributable, but often an older one.
  - **Older than 14.40 (Visual Studio 2022 17.10), the game crashes as it starts,** with no message or with an
    access violation in `MSVCP140.dll`: the 17.10 standard library changed `std::mutex`, and an older
    `MSVCP140.dll` doesn't know it. A missing `VCRUNTIME140.dll` or `MSVCP140.dll` means the same.
  - **To check:** Settings > Apps > Installed apps, "Microsoft Visual C++ 2015-2022 Redistributable (x64)": its
    version is the last part of the name (e.g. 14.44.35211).
  - **To install or update it:** the latest x64 installer is always at
    [aka.ms/vs/17/release/vc_redist.x64.exe](https://aka.ms/vs/17/release/vc_redist.x64.exe) (Microsoft's
    permanent link; [the page listing it](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)).
    Installing it over an older one updates it.
- **Disk space:** about 0.3 GB for the game, 0.6 GB more for the HD textures, and about 0.2 GB for the relit maps.

## Your copy of Quake

QVR:U needs the full game (not the shareware): Steam, GOG or Epic, the original 1996 data or the 2021 re-release.
The installer finds it by itself; QVR:U reads it where it is and never changes it.

- **Steam:** the Quake folder (usually `C:\Program Files (x86)\Steam\steamapps\common\Quake`) has the original data
  in `id1` (`PAK0.PAK` and `PAK1.PAK`), the mission packs in `hipnotic` and `rogue`, and the 2021 re-release in
  `rerelease`. QVR:U plays the original data; the `rerelease` folder also supplies the newer official campaigns and
  their language tables (see [Official campaigns](#official-campaigns)).
- **GOG, Epic and other copies:** any Quake folder with `id1\PAK0.PAK` and `id1\PAK1.PAK` works the same way.
- **Music:** QVR:U plays the soundtrack from where your Quake keeps it (the Steam re-release's
  `rerelease\<game>\music`, or a GOG install's `music`), for each campaign; nothing to copy. A mod's own music still
  wins, and so does a soundtrack already copied into the game folders.

### Only the 2021 re-release?

The re-release's data works too, with the campaign and both mission packs (its folder is the one with `QuakeEX.kpf`
and an `id1\pak0.pak` of about 220 MB; on Steam, `Quake\rerelease`). The installer uses it by itself when there is no
original data. What changes:

- **Maps:** the re-release has its own versions of Quake's and Scourge of Armagon's maps. They're relit with
  coloured light, bounced light and ambient occlusion, and their water is already see-through.
- **Relit maps:** relighting in the game uses the maps the game plays, so the re-release's here. The relit maps get
  QVR:U's look, its bump shading and the light grid, as the original maps do: the re-release's own light settings
  (its bounced light) are left out. `relight_maps.py` with `--quake "<re-release folder>"` relights the original
  maps instead, when they are there (see [RELIGHTING.md](RELIGHTING.md#what-you-need)). `--vis-dir` still gives
  Dissolution of Eternity's maps see-through water. For the other maps the script reports that the patch "is for
  another version of the map" and leaves them alone, which is fine: their water is see-through already. Relit maps
  replace a map whichever version they were made from, so make them from the data you play with.
- **Additional official campaigns:** the re-release includes Dimension of the Past (`dopa`), Dimension of the
  Machine (`mg1`) and Dawn of the Machine (`mg3`). See [Official campaigns](#official-campaigns).

## Installing with the installer

1. **Download `QuakeVR-Setup.exe`** from the latest release on the
   [Releases page](https://github.com/vittorioromeo/quakevr/releases/latest). It is a single file; it downloads the
   game and the optional HD textures itself, and checks every file's SHA-256.
2. **Run it.** It isn't code-signed, so Windows SmartScreen may say "Windows protected your PC": click **More info**,
   then **Run anyway**. The release page lists each file's SHA-256, if you want to check it first
   (`Get-FileHash QuakeVR-Setup.exe` in PowerShell). It needs no administrator rights.
3. **Statement:** a short statement from the author on how AI was used to make QVR:U, with three points to answer YES
   or NO. *Continue* unlocks with YES to all three. The answers aren't saved or sent anywhere.
4. **Your PC:** what it found, before anything is changed: your Quake (and whether it is the original or the
   re-release), the expansions you own and whether each is ready to play, your OpenXR runtimes (SteamVR, Virtual
   Desktop, Meta Quest Link, Windows Mixed Reality), and the Visual C++ runtime. If Quake is somewhere it didn't
   look, pick it with **Use another Quake folder…**.
5. **Options:**
   - **Install folder:** `%LOCALAPPDATA%\Programs\QuakeVR` by default. It must not be inside your Quake folder or in
     Program Files: the game writes its settings, saves and relit maps there.
   - **HD textures** (highly recommended, about 0.6 GB): the Quake Revitalization Project's map textures, for Quake and
     the mission packs you own. See [HD textures](#hd-textures-qrp).
   - **Relight the maps at the first start** (highly recommended): the game relights your maps in the background the
     first time it starts, with light from lamps, lava and glowing panels. VisPatch's data (about 2.5 MB, from
     SourceForge) comes with it, for see-through water. See [Relit maps](#relit-maps-and-see-through-water).
   - **Shortcuts** on the desktop and in the Start menu.
   - **Package:** downloaded from the latest release by default. A `QuakeVR.zip` (or a `QuakeVR` folder) next to
     `QuakeVR-Setup.exe` is used instead, with no download at all; *Use a local package…* picks one.
6. **Install:** the files are downloaded, checked and put in place; a failed or cancelled install changes nothing.
   If the Visual C++ runtime is missing or too old, Microsoft's is installed afterwards (one Windows permission prompt).
7. **Play:** *Play in VR*, *Play on the monitor*, or open the folder. It also shows how to add QVR:U to Steam (so it
   appears in SteamVR's and Virtual Desktop's game lists): Steam > Games > Add a Non-Steam Game to My Library, pick the
   `ironwail.exe` it names, and paste the launch options it gives you into the shortcut's Properties.

The install folder holds everything QVR:U writes:

```
%LOCALAPPDATA%\Programs\QuakeVR\
    ironwail.exe, ironwail.pak, the DLLs
    quakevr\        QVR:U's game folder: its progs, models, sounds, maps and configs;
                    your settings (ironwail.cfg), saves, screenshots, notes and relit maps
    quakevr\tools\  the relight scripts, ericw-tools' light.exe and VisPatch's data
    id1\, hipnotic\, rogue\   only the HD textures, if you chose them
    qvr_addons\, cache\       the Map Library's maps and downloads
    setup\          a copy of the installer, for updates and Apps & Features
    install.json    what was installed, for updates and the uninstall
```

Your Quake folder is only read. The game is started as
`ironwail.exe -basedir "<Quake>" -basedir "<install folder>" -game quakevr`, in the install folder.

## Starting the game

1. Start your OpenXR runtime (SteamVR, or Virtual Desktop connected to the PC) and put the headset on.
2. Start **Quake VR Unleashed** from the desktop or the Start menu. The Start menu folder also has
   *Quake VR Unleashed (flat screen)* (no headset), *Quake VR Unleashed (log for bug reports)* (writes `qconsole.log`)
   and *Quake VR Unleashed files* (the `quakevr` folder: screenshots, notes, saves and settings).

The first time you start with the headset on, **VR Calibration** runs: it measures your height and body, and leads to
the **VR hub**. If the installer's relight was ticked, the maps are relit in the background meanwhile. The desktop
window shows a mirror of the left eye. If no headset is found, the game plays on the monitor instead. The console
(the `~` key on the desktop keyboard) then says why VR didn't start:

- `vr_restart` retries, for example after putting the headset on or starting the runtime.
- `vr_status` prints the runtime, the tracking state and the eye resolution.
- `vr_enabled 0` plays on the monitor on purpose, and `vr_enabled 1` goes back to VR.

## Updating and uninstalling

- **Update notice:** at start-up the game checks GitHub for a newer release (at most once an hour), and the menus say
  so in their bottom right corner, with a link to the release. It never installs anything by itself.
  *Advanced VR > HUD and Menus > Menu > Check for Updates* (`vr_update_check 0`) turns the check off.
- **Updating:** download the new `QuakeVR-Setup.exe` and run it (or run the copy in the install's `setup` folder). It
  finds your install and opens on its **Update** screen: only the program files that changed are replaced. Your
  settings, saves, screenshots, notes, relit maps and Map Library maps are never touched; a program file you changed
  yourself is backed up first, in `backups\`. Settings whose default changed are updated once, on the next start,
  unless you changed them yourself. The relight runs again only if the update changes what it uses, and then only
  for the maps whose light changes.
- **Repair or start over:** the same screen offers *Repair* (for the same version) and *Install again from scratch*,
  which can also reset your settings or remove your saves and installed maps; whatever it removes is moved into
  `backups\`, never deleted.
- **Uninstalling:** Windows' *Settings > Apps > Installed apps*, **Quake VR: Unleashed** > Uninstall, or *Remove…*
  on the installer's first page. It removes the files it installed and the shortcuts; it asks before removing the HD
  textures, and leaves your own files (settings, saves, screenshots, notes, relit maps, backups) in the folder.

## Installing from the zip instead

Each release also has the package itself, `QuakeVR.zip`, for people who'd rather not use the installer. Then:

- Install the [Visual C++ runtime](#requirements) yourself if needed.
- Unzip it **into your Quake folder**, so that `quakevr` sits next to `id1`, and run **`QuakeVR.bat`** (it runs
  `ironwail.exe -game quakevr`; anything you add to its command line goes to the engine). Your settings, saves,
  screenshots and notes go in `quakevr`; the Map Library writes `cache` and `qvr_addons` in the Quake folder.
  Don't unzip it into the re-release's own folder (`Quake\rerelease`): the package's `SDL2.dll` would replace the
  re-release's.
- Or keep it in a folder of its own and name both folders on the command line:
  `ironwail.exe -basedir "<Quake folder>" -basedir "<QVR:U folder>" -game quakevr`, started in the QVR:U folder.
  Don't end either path with a `\`.
- **Updating:** unzip the new package over the old one. Your settings, saves, relit maps and notes are kept.
- **Uninstalling:** delete `quakevr`, `ironwail.exe`, `ironwail.pak`, `ironwail.pdb`, `QuakeVR.bat`,
  `README-QuakeVR.txt`, `manifest.json` and the DLLs from the folder you unzipped it into. Check first that nothing
  else in that folder uses the DLLs.

The package contains:

| File | What it is |
|---|---|
| `ironwail.exe`, `ironwail.pak` | the engine, with QVR:U built in |
| `ironwail.pdb` | the engine's debug symbols: a crash report names the functions on the stack only with it there |
| `openxr_loader.dll`, `SDL2.dll` and the audio codec DLLs | libraries the engine needs |
| `QuakeVR.bat` | the launcher: runs `ironwail.exe -game quakevr` |
| `README-QuakeVR.txt` | a quick-start note |
| `manifest.json` | every file's size and checksum, for the installer |
| `quakevr\` | the game folder: QVR:U's QuakeC (`progs.dat`), models, sounds, textures, maps (hub, tutorial, firing range) and configs |
| `quakevr\tools\` | the scripts that relight your own copy of Quake's maps, and ericw-tools' `light.exe` (in `ericw-tools\`, GPL-3) for the in-game relighting (see [RELIGHTING.md](RELIGHTING.md)) |

## OpenXR runtimes: SteamVR or Virtual Desktop

QVR:U uses whichever OpenXR runtime you pick in **Advanced VR > Headset > OpenXR Runtime**. Switching restarts VR
without restarting the game. The line under it says which runtime is in use and why.

- **Auto** (the default): the runtime whose app is running. Virtual Desktop's Streamer means Virtual Desktop's own
  runtime (VDXR), or SteamVR's when SteamVR is the OpenXR runtime chosen in the Streamer's options; SteamVR means
  SteamVR's, the Meta app's server means Meta's. With none running, it's the system's active runtime. If that runtime
  can't start (no headset connected through it, for example), the game tries the other installed ones before playing
  flat (*Try Other Runtimes*, an advanced row: an idle SteamVR is tried only with *On, SteamVR too*, as trying it
  starts SteamVR). The console command `vr_xr_runtime_explain` prints what Auto sees and the order it tries runtimes
  in. After starting or closing a VR app, *Restart VR* chooses again.
- **System default:** the runtime that Windows has marked as active. This is usually the one set in SteamVR's or
  the Meta app's settings. An `XR_RUNTIME_JSON` environment variable the game was started with always wins.
- **Virtual Desktop (VDXR):** Virtual Desktop's own OpenXR runtime. It skips SteamVR entirely, which can give
  smoother frame pacing on a Quest. In Virtual Desktop's settings, keep *Emulate Index controllers* **off**. The
  first time you use it, check that your guns point where your controllers point (see below).
- **SteamVR:** SteamVR's runtime, for any headset SteamVR drives, including a Quest over Virtual Desktop or Link.
- A specific runtime's manifest: set `vr_xr_runtime 3` and `vr_xr_runtime_json "<path to the runtime's .json>"`
  in the console.

**Guns at the wrong angle?** Different runtimes report the controllers' pose slightly differently. Adjust *Hand
Pitch* (and *Hand Yaw*) under *Hand Calibration* in VR Settings.

## Mission packs

**No expansion is required for Quake's campaign, the VR hub, tutorial or firing range.** Scourge of Armagon
(`hipnotic`) and Dissolution of Eternity (`rogue`) are independent optional packs. Each enables its own campaign,
weapons, monsters and items; either works without the other. Pack weapons/items and their random drops are disabled
when their data is unavailable. Rogue is also needed for lava nails made by shooting through a torch's flame.

The installer finds the packs you own next to your Quake. For a zip install, put the complete owned pack data in its
folder alongside `id1`. QVR:U validates the pack's own required maps, models and sounds before mounting it; an empty
directory or a VR replacement view model does not establish that the pack is installed. Missing, incomplete or
corrupt packs are reported at startup. Their hub buttons are marked *unavailable*, and selecting their campaign is
blocked with a message explaining which data to restore. Run `vr_pack_status`, or choose *Debug > Reports > Mission
Pack Status*, to see each pack's status. Restore the pack from your owned copy when validation reports missing files
or a damaged archive.

In the hub, press an available campaign's button (Quake, SoA or DoE) and step into the portal. Keep the same pack
installation when restoring a save: changing the set of packs changes the saved model indices, so an incompatible
save is rejected before loading. Saves from the earlier merged VR progs require both packs.

Use the hub or Official Campaigns selector to start an owned campaign with QVR:U's gameplay. The merged VR
QuakeC contains Quake, Scourge of Armagon and Dissolution of Eternity; selecting one keeps VR's progs active.

## Official campaigns

Open the main menu's **Select Campaign**, **Advanced VR > Play > Official Campaigns**, or the hub's campaign board link.
The selector shows Quake, the two mission packs, Dimension of the Past (`dopa`), Dimension of the Machine (`mg1`)
and Dawn of the Machine (`mg3`). Each entry reports missing data, incomplete/corrupt data, or installed data with
its native gameplay readiness. **Dimension of the Past is ready for native single-player VR**, including authored
normal/secret routes, deferred monsters, fog/exploding geometry, VR inventory carry/save/reset and readable
completion text/menu. **Dimension of the Machine is ready for native single-player VR** too: its hub and five
episodes through the rune gates to the final gate, mgend and the credits, the electrode puzzle, the seven Horde arenas,
and VR inventory carry/save/reset. **Dawn of the Machine is ready for native single-player VR** as well: its hub,
chapters and secret levels through the four runes to Chthon and the credits, the health and ammunition upgrades, the
Super Axe, axe buttons, bloody shotguns and new monsters, and the hidden Bloody Nightmare difficulty (once found, also
offered in Official Campaigns) with its own ending. Coop context/join/respawn behavior of these three is not accepted
yet (MG1's Horde coop is tested between two local processes only; MG3's co-op and `dm1` come later); ordinary launch
requires `coop 0`, `deathmatch 0`, and `maxplayers 1`. Installed maps alone do not establish support: each campaign
was made ready on its own.

For an owned Steam installation, QVR:U checks the original Quake folder and its `rerelease` folders. It also
checks explicit `-basedir` roots and the existing Steam/GOG discovery paths. No expansion download or separate
installer is needed when complete data is found. For other copies, keep each complete owned pack folder alongside
`id1`, or use the two `-basedir` options shown [above](#installing-from-the-zip-instead). Detection does not copy
assets or write into the borrowed installation. The last explicit base has priority; a damaged copy there is reported
rather than silently replaced by another installation.

The newer campaigns also need current language tables from the owned rerelease `id1` data; their expansion
PAKs do not contain them. QVR:U borrows only these tables from the configured roots or enabled Steam/GOG
discovery, without adding the borrowed `id1` maps/models to your campaign paths. `-nosteam`, `-nogog`, and
`-noepic` disable the corresponding store lookup; explicit `-basedir` roots and their rerelease subfolders still
work. Keep the writable QVR:U folder as the last `-basedir`.

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

Official campaign arguments such as `-game mg1` now keep VR's merged progs active; they are not a way
to opt into the pack's original gameplay code. Developer-only `vr_campaign_native <folder>` bypasses the readiness
gate for testing and warns that gameplay/progression is incomplete. Use the normal selector for supported play.
Current port coverage and outstanding mechanics are recorded in the [expansion audit](vr-port/EXPANSIONS.md).

## Custom maps and mods

Command-line options below (`-game <folder>`) go after the shortcut's target (its Properties), or after
`QuakeVR.bat` for a zip install. Folders such as `quakevr` and `qvr_addons` are in the QVR:U folder (the Quake
folder for a zip install).

- **The Map Library** (the main menu's *Download Maps*, or the menu corner's *Map Library* button) lists
  [Quaddicted](https://www.quaddicted.com/)'s archive of custom maps (its index is fetched when the game starts and
  cached in `cache\maps_index.txt`). Type to search; the bar under the list sorts (rating, newest, oldest, size,
  title) and filters them (type, size, rating, installed only). Pick one to read its description, then **Install**:
  it is downloaded and unpacked in the background, into its own folder `qvr_addons\<id>\`, and
  **Play** starts it. An installed package also has **Uninstall** and **Reinstall** (press twice to confirm).
  Packages that bring their own `progs.dat` are left out, since QVR:U runs its own gameplay
  (`vr_maps_allow_progs 1` lists them). The downloads are kept in `cache\maps\`, at most `vr_maps_cache_mb` (512) MB;
  `vr_maps_fetch 0` (or `-nomapindex`) stops the start-up fetch. The console has the same: `maps_list`, `maps_info`,
  `maps_install`, `maps_play`, `maps_installed`, `maps_uninstall`, `maps_status`, `maps_cancel`, `maps_fetch`.
- **A map without its own `progs.dat`:** put its `.bsp` in `quakevr\maps`, then load it with
  `map <name>` in the console or from the main menu's *Play Custom Map*. For a map pack in its own folder, start the
  game with `-game <folder>`: QVR:U's gameplay stays, and the folder's maps, textures and sounds are added on top.
- **A mod with its own `progs.dat`** (Arcane Dimensions, Copper, Quoth, ...): `-game <mod>` runs it in a
  **compatibility mode**. You get the headset, tracked hands and body, the wrist gadget, movement, room-scale,
  teleport and QVR:U's particles. The mod's weapons fire from the gun in your main hand, and your hand aims them.
  QVR:U's own gameplay (off-hand weapons, holsters, throwing, force grab, melee, hand pickups) isn't available,
  and items are picked up by walking over them. The details are in
  [vr-port/MODS.md](vr-port/MODS.md).

## HD textures (QRP)

QVR:U looks best with high-resolution replacement textures. It uses the same texture packs as other Quake engines
based on QuakeSpasm: images in a `textures` folder, looked up in `textures\<map name>\` first, then `textures\`, in
the game folders (`quakevr`, the mission packs, then `id1`). They get smooth filtering, and QVR:U makes bump maps
from them. A pack's own normal maps (`<name>_norm`) are used as they are. Glow images (`<name>_luma`) glow, and the
relight lights rooms with them.

The author plays with the **Quake Revitalization Project (QRP)** map textures, for Quake and both mission packs.

**The easy way:** tick *HD textures* in the installer. It installs the author's pack, converted losslessly to PNG,
into the QVR:U folder's `id1`, and `hipnotic` and `rogue` for the packs you own (never into your Quake folder). The
same pack, `quakevr-hq-textures-png-2026-10-03.zip`, is on the
[support files release](https://github.com/vittorioromeo/quakevr/releases/tag/assets-2026-10-08): to install it by
hand, extract it into the QVR:U folder (the Quake folder for a zip install).
[vr-port/TEXTURES.md](vr-port/TEXTURES.md) has its contents, credits and licence. Then see step 3 below.

To install it from the QRP archive instead: QRP was completed in 2016. Its own site,
[qrp.quakeone.com](http://qrp.quakeone.com/), still lists the packs, but its download links no longer deliver the
files. Get them from the
**[Quake Revitalization Project Archive](https://www.moddb.com/addons/quake-revitalization-project-archive)** on
ModDB instead: `QuakeRevitalizationProject.7z` (1.26 GB), a complete compilation of every QRP release, mission packs
included.

1. Extract the archive with [7-Zip](https://www.7-zip.org/). You need the **map textures** packages:
   `QRP_map_textures_v.1.00.pk3` (Quake), `QRP_SoA_map_textures_add-on_v.1.00.pk3` (Scourge of Armagon) and the
   Dissolution of Eternity map textures. The normal-map add-ons are optional: QVR:U makes bump maps from the
   textures by itself, and the author plays without them. The item textures are made for other engines and aren't
   needed.
2. Ironwail doesn't read `.pk3` files, so extract them too. A `.pk3` is a zip file: 7-Zip opens it, or rename it to
   `.zip`. Each has a `textures` folder inside. Extract the Quake one into `id1` (in the QVR:U folder, or the Quake
   folder for a zip install), so that you get `id1\textures\` with the `.tga` files and the per-map subfolders
   (`e1m1`, `dm3`, ...). Scourge of Armagon's goes into `hipnotic` (`hipnotic\textures\`), and Dissolution of
   Eternity's into `rogue` (`rogue\textures\`).
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
files add water-aware visibility. QVR:U loads the relit maps in place of the originals. id Software's maps can't be
redistributed, so they are made on your PC, once, into `quakevr\relit\` (about 210 MB).

**With the installer:** tick *Relight the maps at the first start*. It also downloads VisPatch's data into
`quakevr\tools\vispatch`, and the game relights every map in the background the first time it starts.

**In the game:** *Advanced VR > Graphics > Relighting* (Menu Detail: Advanced) relights the map you are in, an
episode, a game or every map, in the background while you play, with brightness sliders. With the VisPatch files in
`quakevr\tools\vispatch` it makes the water see-through too (*See-Through Liquids*, on by default; dimmed without
the files). See [RELIGHTING.md](RELIGHTING.md#relighting-in-the-game).

**With the script** in `quakevr\tools\`: [RELIGHTING.md](RELIGHTING.md) has the step-by-step guide. In short, you
need Python 3, [ericw-tools 2.0.0-alpha11](https://github.com/ericwa/ericw-tools/releases/tag/2.0.0-alpha11) (the
package's own `quakevr\tools\ericw-tools\light.exe` works) and the
[VisPatch data files](https://sourceforge.net/projects/vispatch/files/vispatch%20data/1.0/), and, to get the same
result as the author, the QRP textures installed first. Then, from a Command Prompt in the QVR:U folder:

```
python quakevr\tools\relight_maps.py --quake "C:\Program Files (x86)\Steam\steamapps\common\Quake" --light "quakevr\tools\ericw-tools\light.exe" --vis-dir "C:\tools\vispatch"
```

It takes about a minute on a fast PC.

**Playing with them:** the relit maps are used from the next map you load. *Advanced VR > Graphics > Relit Maps*
switches between the relit and the original lighting. Water on the relit maps is see-through (*Advanced VR >
Graphics > Transparency > Water Alpha*, 0.3 by default; other maps keep opaque water whatever it says). Lava is
not water-vised by VisPatch, so it looks opaque.

QVR:U's own maps (the hub, tutorial and firing range) are already relit in the package.

## Voice notes

Voice notes (see [FEATURES.md](FEATURES.md#playtesting-tools)) are saved as `.wav` files in `quakevr\notes\`. To turn
them into text, the repository has `Misc\quakevr\transcribe_notes.py`. It runs Whisper locally, through
[faster-whisper](https://github.com/SYSTRAN/faster-whisper), and writes `quakevr\notes\NOTES.md`: every note with
its transcript, context and screenshot.

```
python -m pip install --user faster-whisper
python Misc\quakevr\transcribe_notes.py --notes "<QVR:U folder>\quakevr\notes" [--device cpu|cuda]
```

The Whisper model (`large-v3-turbo` by default) downloads once, the first time. Only new notes are transcribed.

## Performance

In VR, a missed frame is felt. What helps most:

1. **Graphics preset:** *Advanced VR > Graphics > Preset* (Menu Detail: Advanced). The presets are *Off (Quake)*,
   *Low*, *Medium*, *High* and *Ultra*. It sets the shadows, lights and model lighting. You can adjust single
   settings afterwards.
2. **Render Scale** (VR Settings, or Advanced VR > Headset): below 1 renders fewer pixels and upscales them smoothly.
   Above 1 gives smoother edges if the GPU has headroom.
3. **The runtime's resolution:** QVR:U renders at the size the runtime asks for. SteamVR's per-application
   resolution should be 100%. With Virtual Desktop, its quality preset multiplies SteamVR's, so a high preset plus
   SteamVR supersampling can mean over twice the headset's pixels. A lower refresh rate (90 instead of 120 Hz) also
   helps.
4. **Try Virtual Desktop's VDXR runtime** on a Quest: it skips SteamVR's compositor.
5. **Hide Lens Corners** (Advanced VR > Headset) skips pixels you can't see. **Anti-aliasing** (Graphics; 4x by
   default) costs GPU time: 2x or off is cheaper.

**Finding out what's slow:** *Graphics > Performance Profile* (`vr_profile 1`) times each part of the frame, on the
CPU and the GPU, and writes `quakevr\profile\profile_<map>_<date>.csv`. `vr_profile 2` also shows the costliest
parts on the wrist gadget.

**If it gets slower the longer you play:** the *Memory Log* (`vr_memstats_log`, on by default) writes a row a minute
to `quakevr\profile\memstats_<date>.csv`, with the game's memory, what it draws, and its frame times next to the
runtime's. `vr_memstats` prints the same in the console. To tell whether the game or the runtime is at fault:
once it has slowed down, restart only QVR:U on the same map. If the frame rate is back, it's the game. If it
only comes back after restarting SteamVR or Virtual Desktop too, it's them.

## Troubleshooting and bug reports

Start the game with the Start menu's **Quake VR Unleashed (log for bug reports)** (or add `-condebug` to its command
line; `QuakeVR.bat -condebug` for a zip install). The console goes to `qconsole.log` in the folder the game started
in (the QVR:U folder), which is the most useful thing to attach to a report. If the game crashes, it writes
`qvr_crash.txt` and `qvr_crash.dmp` in the same folder: attach both.

Paths under *Advanced VR* need *Menu Detail: Advanced* (the last row of every page) for most pages, and *Debug* pages
need *Developer*.

| Problem | What to try |
|---|---|
| The game doesn't start at all | The Visual C++ runtime is missing or too old: see [Requirements](#requirements). Running the installer again (*Repair*) also checks it. |
| Nothing in the headset | Look for the `VR:` lines in the console: they name the OpenXR call that failed. Check that the runtime is running, then `vr_restart`. Try another runtime in Advanced VR > Headset. |
| Guns point the wrong way, hands misplaced | Adjust *Hand Pitch* and the other *Hand Calibration* rows in VR Settings. *Weapon Offsets (Held Weapon)* moves a single weapon in the hand. |
| Double vision, wrong scale | Send the output of `vr_status`, and a screenshot of the desktop mirror with `vr_window_view 3` (both eyes; Graphics > Recording > Window View > Both Eyes (raw)). |
| Too tall, too short, floor in the wrong place | *Set Height Now* while standing straight, *World Scale*, *Floor Offset*. |
| Water isn't see-through | Needs maps relit with the VisPatch files (by the installer, the script, or in the game with *See-Through Liquids*), and *Transparency > Water Alpha* below 1 (0.3 by default). See [RELIGHTING.md](RELIGHTING.md#troubleshooting). |
| No sound | Check the Windows output device (your headset's audio) and Ironwail's volume options. |
| Slow or stuttering | See [Performance](#performance). |
| A crash | `qconsole.log` up to the crash, `qvr_crash.txt` and `qvr_crash.dmp`, and what you were doing. |
| Settings in a mess | *Options > Reset All*, or delete `quakevr\ironwail.cfg` for a completely fresh start (the installer's *Install again from scratch* with *Reset settings* does the same, keeping a backup). |

Report bugs and ideas on the [GitHub issues page](https://github.com/vittorioromeo/quakevr/issues), with the log,
your headset and runtime, and, for performance, a profile or memory log.
