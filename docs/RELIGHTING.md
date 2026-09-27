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

A tool that does all of this automatically is planned. Until then, these are the steps.

## What you need

1. **Quake VR, installed** in your Quake folder (see [INSTALL.md](INSTALL.md)). The package has the relighting
   scripts in `quakevr\tools\`: `relight_maps.py`, `vis_maps.py`, `quakepak.py` and `relight_textures.cfg`.
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
   [INSTALL.md](INSTALL.md#hd-textures-qrp). The relight looks at the textures' glow images (`_luma`) to find the
   lamps and light panels that glow in QRP but not in Quake's own textures, and gives them a light of their own.
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
| `--list-glows` | List each map's glowing textures and the lights they get, without relighting anything. |

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
