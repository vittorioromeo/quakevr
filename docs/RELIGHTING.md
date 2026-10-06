# Relighting Quake's maps: relit maps and see-through water

This is optional, but it changes the look of Quake VR more than anything else. Quake's 1996 lightmaps have no
ambient occlusion, no coloured light and no bounce, so the world looks flat in VR. The relight gives every map of
Quake and the mission packs soft shadows, dark corners, coloured light, lamps, light panels and glowing buttons
that light their rooms, and lava that casts a red glow on the walls round it (slime a faint green one). The same
step makes **water, slime and teleporters see-through**.

id Software's maps can't be redistributed, so the relit maps aren't in the Quake VR package: you make them once, on
your own PC, from your own copy of Quake. It takes about a minute on a fast PC (about a second a map, 73 maps), and
the result is the same as the author's.

- [What you need](#what-you-need)
- [Step by step](#step-by-step)
- [In the game](#in-the-game)
- [Checking the result](#checking-the-result)
- [Options](#options)
- [Troubleshooting](#troubleshooting)
- [Running from the repository](#running-from-the-repository)
- [Relighting in the game](#relighting-in-the-game)
- [ericw-tools' licence](#ericw-tools-licence)

A tool that does all of this automatically is planned. Until then, these are the steps. A single map can also be
relit from inside the game, with your own brightness settings: see
[Relighting in the game](#relighting-in-the-game).

## What you need

1. **Quake VR, installed** in your Quake folder (see [INSTALL.md](INSTALL.md)). The package has the relighting
   scripts in `quakevr\tools\`: `relight_maps.py`, `vis_maps.py`, `quakepak.py`, `quakeimage.py` and
   `relight_probe.py`. Which textures give off light, and how much, is in `quakevr\relight_textures.cfg`
   (see [Brighter or darker lamps](#brighter-or-darker-lamps)).
2. **Python 3.7 or newer** from [python.org](https://www.python.org/downloads/). In the installer, tick *Add
   python.exe to PATH*. The scripts use only Python's standard library, so there's nothing else to install with
   `pip`.
3. **ericw-tools 2.0.0-alpha11**, the tools that compute the light. Download
   `ericw-tools-2.0.0-alpha11-win64.zip` (27.5 MB) from
   [its release page](https://github.com/ericwa/ericw-tools/releases/tag/2.0.0-alpha11) and extract it to a folder of
   its own, for example `C:\tools\ericw-tools-2.0.0-alpha11-win64`. `light.exe` should be directly in that folder.
   Use this exact version to get the same light as the author. Other versions can light the maps differently.
4. **The VisPatch data files**, for see-through water. Download these three files from the VisPatch project on
   SourceForge, in [vispatch data/1.0](https://sourceforge.net/projects/vispatch/files/vispatch%20data/1.0/):

   | File | Size | For |
   |---|---|---|
   | `id1_vis.tgz` | 949 KB | Quake |
   | `hipnotic_vis.tgz` | 807 KB | Scourge of Armagon |
   | `rogue_vis.tgz` | 787 KB | Dissolution of Eternity |

   Extract all three into one folder, for example `C:\tools\vispatch`. Windows 10 and 11 have `tar` for this. In a
   Command Prompt, in the folder you downloaded them to:

   ```
   mkdir C:\tools\vispatch
   tar -xzf id1_vis.tgz -C C:\tools\vispatch
   tar -xzf hipnotic_vis.tgz -C C:\tools\vispatch
   tar -xzf rogue_vis.tgz -C C:\tools\vispatch
   ```

   The folder then holds `id1.vis`, `hipnotic.vis` and `rogue.vis` (and a `rogue.txt` you can ignore). If
   SourceForge is unavailable, the [Quake Info Pool](https://www.quake-info-pool.net/vispatch/files.htm) has the
   same data as the original 1997 packages. Get `id1.zip`, `hipnotic.zip` and `rogue.zip`, and put the `ID1.VIS`,
   `HIPNOTIC.VIS` and `rogue.vis` from inside them in the folder. You don't need the other files in those zips.
   You can skip the VisPatch files entirely: the maps are still relit, but their water stays opaque.
5. **Recommended: the QRP HD textures, installed first.** See
   [INSTALL.md](INSTALL.md#hd-textures-qrp). The relight looks at the textures' glow images (`<name>_glow` or
   `<name>_luma`, as `.png`, `.tga` or `.jpg`) to find the lamps and light panels that glow in QRP but not in
   Quake's own textures, and gives them a light of their own. It finds them where the game does: in `quakevr`,
   `rogue`, `hipnotic` and `id1` (loose files and `.pak` files), `textures\<map>\` first; and, for a texture with
   no glow image of its own, in Quake VR's own material maps (`quakevr\textures_quetoo`).
   This changes 61 of the 73 maps. Without QRP, the maps are still relit, but only textures that glow in Quake's own
   images get lights, so the result differs from the author's.

You also need the original Quake data: `id1` with `PAK0.PAK` and `PAK1.PAK`, and `hipnotic` and `rogue` if you have
the mission packs. The Steam and GOG versions have them. The 2021 re-release's maps (in `rerelease`) aren't used.

## Step by step

1. Open a **Command Prompt** (press Windows+R, type `cmd`, press Enter) and go to your Quake folder:

   ```
   cd /d "C:\Program Files (x86)\Steam\steamapps\common\Quake"
   ```

2. Run the relight. Adjust the three paths if your Quake folder, ericw-tools or VisPatch files are elsewhere.
   This is a single line:

   ```
   python quakevr\tools\relight_maps.py --quake "C:\Program Files (x86)\Steam\steamapps\common\Quake" --light "C:\tools\ericw-tools-2.0.0-alpha11-win64\light.exe" --vis-dir "C:\tools\vispatch"
   ```

   It prints one line per map and a total at the end:

   ```
   relighting with ericw-tools 2.0.0-alpha11 (C:\tools\ericw-tools-2.0.0-alpha11-win64\light.exe)
   id1/dm1 ... ok
   id1/dm2 ... ok
   ...
   rogue/start ... ok
   73 maps: 73 relit, 0 up to date, 0 failed
   ```

   That's 38 maps for Quake, 18 for Scourge of Armagon and 17 for Dissolution of Eternity. A mission pack you
   don't have is skipped (`rogue: not installed, skipped`).

3. That's it. The relit maps are in **`quakevr\relit\`**, in a folder per game: `quakevr\relit\id1\maps\`,
   `quakevr\relit\hipnotic\maps\` and `quakevr\relit\rogue\maps\`. Each map has a `.bsp` (the map, with the
   water visibility and a grid of the light for models), a `.lit` (the coloured light), a `.lux` (the light's
   direction, for bump maps) and a `.relit` file, which records how the map was made. The whole set is about
   210 MB.

Running the command again is quick: maps that are already relit are skipped (`up to date`). A map is relit again
by itself when something that changes its light has changed: another version of ericw-tools, different options,
HD textures installed or removed since, or an update of the scripts that changes the light. `--force` relights
everything anyway.

If you write the paths as `--quake .` from inside the Quake folder, that works too. The output goes to the
`quakevr\relit` folder next to the scripts' `tools` folder unless you give `--out` (see [Options](#options)).

## In the game

- **Relit maps** are used automatically from the next map you load. *VR Settings > Advanced VR Options >
  Graphics > Relit Maps* (`vr_relit_maps`) switches between the relit and the original lighting. A map that has
  no relit version plays with its own light.
- **See-through water:** set how transparent it is in *VR Settings > Advanced VR Options > Transparency > Water
  Alpha* (`r_wateralpha`, **0.6** by default in Quake VR; 1 is opaque). Maps that aren't water-vised keep opaque water whatever it says. Slime
  and teleporters use the same value, unless *Slime Alpha* or *Tele Alpha* is set. Lava is opaque by default
  (*Lava Alpha* 1). All four are saved in the config.

## Checking the result

- **The water visibility:** in the Quake folder, run

  ```
  python quakevr\tools\vis_maps.py --check quakevr\relit\id1\maps quakevr\relit\hipnotic\maps quakevr\relit\rogue\maps
  ```

  Every map should say `vised for` and the liquids it has, such as `e1m1.bsp: vised for water, tele, slime`.
  `vised for nothing; opaque: ...` means the map has no water visibility: the relight ran without `--vis-dir`, or
  the VisPatch files weren't found (see [Troubleshooting](#troubleshooting)).
- **In the game:** with *Water Alpha* below 1, start a map with water (`map e1m1` in the console) and look into
  it. You should see the walls and floor below the surface. From under the water, you should see the room above.
- **The light:** switch *Relit Maps* off, reload the map (`map e1m1`), and compare. The relit version has darker
  corners, softer shadows and coloured light around lamps and glowing panels, and red light round lava (`map start`:
  the lava pit by the Hard hall).

## Options

`python quakevr\tools\relight_maps.py --help` lists them all. The useful ones:

| Option | What it does |
|---|---|
| `--quake <folder>` | Your Quake folder, the one containing `id1`. Required. |
| `--light <light.exe>` | ericw-tools' `light.exe`. Without it, the script tries the `ERICW_LIGHT` environment variable, then `light.exe` on `PATH`. |
| `--vis-dir <folder>` | The folder with `id1.vis`, `hipnotic.vis` and `rogue.vis`, for see-through water. The `QUAKEVR_VISPATCH` environment variable works too. |
| `--out <folder>` | Where the relit maps go. The default is the `quakevr\relit` folder next to the scripts' `tools` folder, which is right for an installed package. |
| `--games id1 hipnotic rogue` | Which games' maps to relight (the default is all three). |
| `--only e1m1 e1m2` | Only these maps, for trying things out. |
| `--force` | Relight every map, even those that are up to date. |
| `--bright` | A brighter look: some bounced light and weaker ambient occlusion (Quake VR's look before its tenth playtest round). |
| `--no-luma` | Don't use the HD textures' glow images to find what glows. |
| `--light-texture-strength 1.5` | Every light from a texture (lamps, light panels, glowing buttons, lava) 1.5 times as bright. The default is 1.2, as the game's Light Textures. `--glow-scale` is the same. |
| `--basedir <folder>` | Another folder the game reads (its own `-basedir`, with `id1`, `quakevr`...) for textures. Several are allowed. |
| `--extmaps-dir <folder>` | The material maps' folder (the game's `vr_extmaps_dir`; `textures_quetoo` by default, `""` for none). |
| `--list-glows --list-textures` | Also print the folders searched for textures and every glow image used, with where it came from. |
| `--list-glows` | List each map's glowing textures and the lights they get, without relighting anything. |

### Brighter or darker lamps

`quakevr\relight_textures.cfg` says which textures give off light and how much. Its `strength` line sets each kind's
brightness: `fixture` (lamps, light panels and strips, the pyramid lanterns), `glow` (buttons, computer panels,
runes, slipgates) and `liquid` (lava, slime). Rules below it make single textures brighter (`scale=`), name a
texture a lamp (`kind=fixture`) or switch one off (`kind=off`), for every map, one game (`hipnotic/*/tlight02`) or one
map (`e1m1/tlight11`). The comments at its top list every setting. After a change, run the relight again: only the
maps whose lights changed are relit.

`relight_probe.py` measures the result: the lightmap's mean brightness round each lamp of the textures you name, for
one map or several side by side (0-255; 128 is full light):

```
python quakevr\tools\relight_probe.py quakevr\relit\hipnotic\maps\hip1m1.bsp --textures tlight01 tlight02
```

**Adding the water visibility later:** if you relit without `--vis-dir`, run the same command again with it: maps
already relit get the water visibility without being relit. `vis_maps.py --vis-dir <folder> --relit
quakevr\relit` does the same on its own.

**Starting over:** delete `quakevr\relit`. Nothing else is changed: your `id1`, `hipnotic` and `rogue` folders are
only read.

## Troubleshooting

| Problem | What to do |
|---|---|
| `'python' is not recognized` | Python isn't on `PATH`. Reinstall it with *Add python.exe to PATH* ticked, or use `py` in place of `python`. |
| `ericw-tools' light not found` | Give `--light` with the full path to `light.exe`, in quotes. |
| A map says `failed` | The lines after it are `light`'s own output. If it mentions a missing DLL such as `VCRUNTIME140.dll` or `MSVCP140.dll`, install the Microsoft Visual C++ Redistributable (x64), which Quake VR needs too. |
| `<game>: not installed, skipped` | That game's folder isn't in the `--quake` folder. That's fine for a mission pack you don't have. |
| `the water-vis patch is for another version of the map, left alone` | That map isn't the original 1996/1997 version (a modified map, or a mod's map with the same name), so the VisPatch data doesn't fit it. It is relit, but its water stays opaque. |
| The water is still solid | *Water Alpha* was set to 1 (see [In the game](#in-the-game)); or *Relit Maps* is off; or the map isn't water-vised (see [Checking the result](#checking-the-result)). |
| `gfx/palette.lmp not found: glowing textures will not light` | `--quake` doesn't point at the folder containing `id1`. |

## Running from the repository

The scripts are also in the repository, in `Misc\quakevr\`. Run from there, they write into the repository's own
`quakevr\relit` unless you give `--out`. This is what you want if your Quake folder's `quakevr` is a link to the
repository's (see [BUILDING.md](BUILDING.md#running-from-the-repository)). Otherwise, point `--out` at the installed
folder:

```
python Misc\quakevr\relight_maps.py --quake "C:\Program Files (x86)\Steam\steamapps\common\Quake" --light "C:\tools\ericw-tools-2.0.0-alpha11-win64\light.exe" --vis-dir "C:\tools\vispatch" --out "C:\Program Files (x86)\Steam\steamapps\common\Quake\quakevr\relit"
```

`relight_quakevr_maps.py` is different: it relights Quake VR's own maps (the hub, tutorial and firing range). Those
are already relit in the package.

## Relighting in the game

*VR Settings > Advanced VR Options > Graphics > Relighting* relights the map you are in, with the settings on that page,
and shows you the result where you stand. It needs no Python: the game runs ericw-tools' `light` itself, in the
background, while you keep playing. Quake VR's package has it in `quakevr\tools\ericw-tools\`; without it, the page
uses `vr_relight_tool` (the full path of a `light.exe`), the `ERICW_LIGHT` environment variable or a `light.exe` on
`PATH`. The page's Tool line says which one it found.

When it finds none, the page offers **Download ericw-tools (27.5 MB)** (the console: `vr_relight_get_tool`). The game
downloads ericw-tools 2.0.0-alpha11's Windows release zip from
[its GitHub release](https://github.com/ericwa/ericw-tools/releases/tag/2.0.0-alpha11), always that exact file:
it checks its size and SHA-256 (`4e5ea11b...0745f`) and unpacks nothing from a file that does not match. It keeps
only what `light` needs and the licence texts (`light.exe`, `embree4.dll`, `tbb12.dll`, `tbbmalloc.dll`,
`gpl_v3.txt`, `LICENSE-embree.txt`, `README.md`) plus a `NOTICE.txt`, in `quakevr\tools\ericw-tools\` of the folder
the game saves to (normally the Quake VR folder: where the package puts them). The bar shows how far it is; **Cancel Download** (or
`vr_relight_cancel`, or quitting) stops it and keeps nothing. The files are written to `ericw-tools.download\` first
and moved into place with `light.exe` last, so a half-finished download is never used. Windows only (on Linux and macOS,
build ericw-tools and set `vr_relight_tool`). `vr_relight_get_tool status` prints what was found, where the download
goes and the pinned file; `vr_relight_get_tool force` downloads it again even when a `light.exe` is found.

For testing, `vr_relight_tool_dir <folder>` makes that folder the only place looked in (after `vr_relight_tool`) and
the place the download goes, and `vr_relight_tool_url <url>` downloads from elsewhere (the file must still match the
pinned SHA-256). The author's own copy (`C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64`, as `relight_maps.py`'s
default) is looked in last, and only at Menu Detail: Developer (`vr_menu_level 2`).

1. Load the map, open the page, set the sliders.
2. Choose **Relight This Map**. The page shows what `light` is doing (`Direct Lighting 45%`). A map takes from a second
   (e1m1 with the default settings) to a minute (a big map with Bounced Light). **Cancel** stops it.
3. When it ends, the map is reloaded where you are (a quick save, `autosave/relight`, and load; where the game can't
   be saved, during an intermission or when you are dead, the map restarts). *Reload Where You Are* off: the new light
   shows from the map's next start.

The settings:

| Setting | Console | What it does |
|---|---|---|
| Light Textures | `vr_relight_strength` (1.2) | Everything a texture lights: lamps, light panels, glowing buttons and panels, lava. |
| Lamps and Light Panels | `vr_relight_lamps` (1) | The light fixtures, times Light Textures (with `relight_textures.cfg`'s strength for them). |
| Glowing Panels and Buttons | `vr_relight_glows` (1) | Buttons, computer panels, runes, slipgates, times Light Textures. |
| Lava and Slime | `vr_relight_liquids` (1) | Their glow on the walls round them, times Light Textures. |
| Map Lights | `vr_relight_maplights` (1) | The mapper's own lights: brighter or dimmer, reaching as far. |
| Sunlight | `vr_relight_sunlight` (1) | The sun and sky light of maps that have them (id's maps have none). |
| Bounced Light | `vr_relight_bounce` (0) | Light bouncing off walls: brighter, flatter rooms. Takes longer. |
| Ambient Occlusion | `vr_relight_ao` (1.5) | Darker corners (0: none). |
| Minimum Light | `vr_relight_minlight` (0) | No place darker than this. |
| Shadow Quality | `vr_relight_quality` (1) | Smooth (soft edges) or Fast (about four times quicker). |

At their defaults the result is the relight script's (the lights given to `light` are the same, to the last digit).
*Defaults* puts them back. The console has the same: `vr_relight` (the map in play, or `vr_relight e1m2` for another one,
which is not reloaded), `vr_relight_cancel`, `vr_relight_status`, `vr_relight_defaults`, `vr_relight_revert`.

### Many maps at once

The page's **Many Maps** part relights a whole set with the same settings, in the background, while you play:

1. Choose the **Maps**: *This Map*, *An Episode* (the one you are in, e.g. E1M1 to E1M8 from E1M3, or HIP2M1... in a
   mission pack; or pick E1 to E4 under **Episode**), *A Game* (the one you are in, or pick Quake, Scourge of Armagon,
   Dissolution of Eternity, Dimension of the Past, Dimension of the Machine, Dawn of the Machine under **Game**, when
   installed), *Map Library's* (every map of the packages installed from the Map Library) or *Every Map* (all of them).
   Only maps you can play are taken (a game folder's brush models in `maps/`, the ammo boxes and Quake VR's buttons, are
   not).
2. Choose **Relight These Maps**. The page shows a bar: how far the whole batch is (each map weighed by its size, the
   one being lit by its stage), the time left, the maps done of how many and each map being lit with its stage. Outside
   the menu the wrist gadget's screen shows a line in place of the kills and secrets (`RELIGHT 3/8 45% 0:27`, a thin bar
   under it); on a flat screen it is in the top right corner (*Progress Outside the Menu*, `vr_relight_indicator`).
3. **Cancel** stops it: the maps it finished keep their new light, the ones being lit keep the light they had (nothing
   half-made is left: each file is written beside its place and renamed into it at the end, and `light`'s copies in the
   work folder are removed). Quitting the game mid-batch does the same.

Maps relit with the same settings before are skipped (the `.relight` file keeps a hash of the settings, `light`'s
options, the map's file and `relight_textures.cfg`); *Relight Unchanged Maps Too* (`vr_relight_batch_force`) relights
them again (a new texture pack's glow images are not in the hash: use it then). The map you are in goes first; with
*Reload Where You Are* on it is reloaded as soon as it is done, or only at the batch's end (*Reload the Map You're In*,
`vr_relight_batch_reload`).

**Maps at Once** (`vr_relight_parallel`, 0: two from 8 cores, else one) lights maps side by side, `light`'s threads
shared between them. On a 32-core computer id's episode 1 took 7 s one at a time, 6 s two at once and 5 s three or
four; with Bounced Light 34, 33, 33 and 42 s. A map's texture lights are made on the game's thread when it starts (5 to
15 ms for id's maps), one map a frame at most.

| Console | What it does |
|---|---|
| `vr_relight_batch` | The page's choice (`vr_relight_batch_set` 0..4, `vr_relight_batch_episode`, `vr_relight_batch_game`). |
| `vr_relight_batch episode e2`, `game hipnotic`, `library`, `everything`, `map` | That set. |
| `vr_relight_batch e1m1 dm4 start` | These maps (as the game finds them). |
| `... -force`, `... -list` | Relight unchanged maps too; only list the maps (their files and sizes). |
| `vr_relight_status` | The batch: maps done, each `light` running (stage, process id), progress and time left. |

**Where the result goes:** `quakevr\relit_custom\<game>\maps\` (in the folder the game saves into), with a
`<map>.relight` file saying how it was made. It is used over the relight script's map from then on (*Use In-game
Relights*, `vr_relight_use`; off: the script's or the map's own). *Remove This Map's Relight* (`vr_relight_revert`)
deletes it. The game starts from the relight script's copy of the map when there is one (it keeps that copy's
see-through water), else from the map itself; id's `.pak` files and the game folders' maps are only read. The work
folder, `relit_custom\_work\<game>\`, holds `light`'s logs (`<map>.txt`, `<map>-light.log`); the map given to it and
what it made are removed once the result is in place (or the relighting stopped).

**Not done in the game:** the water-vis patch (see-through water: the relight script does it; a map relit in the game
keeps it if the script's copy had it) and lights for the glowing textures of BSP2 maps (neither does those).

## ericw-tools' licence

ericw-tools is free software under the GNU General Public License, version 3 (GPL-3). Quake VR ships its `light.exe`,
unchanged, with the DLLs it needs, in `quakevr\tools\ericw-tools\`, and runs it as a separate program: it gives it a
copy of a map on the command line and reads the files it writes. That is what the GPL calls an *aggregate*: two
separate programs side by side, communicating as programs normally do (command-line arguments and files), so the GPL-3
applies to ericw-tools, not to Quake VR, which keeps its own licence (GPL-2.0 or later, from Quake and Ironwail;
compatible with GPL-3 anyway). See the FSF's
[GPL FAQ on aggregates](https://www.gnu.org/licenses/gpl-faq.html#MereAggregation).

What shipping it takes (GPL-3 section 6, for the binaries; Apache-2.0 for the libraries):

- **The licence texts beside it:** `gpl_v3.txt` (ericw-tools), `LICENSE-embree.txt` (Embree, Apache-2.0; oneTBB's
  `tbb12.dll` and `tbbmalloc.dll` are Apache-2.0 too), and `NOTICE.txt` saying what they are and where their source is
  (`Misc/quakevr/ericw-tools-NOTICE.txt`). `Windows/package-quakevr.ps1` copies them with the program; the in-game
  download (Relighting in the game, above) unpacks the same texts and writes its own `NOTICE.txt`.
- **Its source, offered the same way as the download:** the release page that offers the Quake VR package must also
  offer ericw-tools 2.0.0-alpha11's source (`ericw-tools-2.0.0-alpha11-src.zip`: the
  [2.0.0-alpha11 tag](https://github.com/ericwa/ericw-tools/tree/2.0.0-alpha11) with its submodules). A link to
  ericw-tools' own GitHub alone is allowed by section 6(d) only while it stays up: a copy next to the package is the safe
  way.
- **Unchanged:** if it is ever patched, the patched source is what must be offered.
