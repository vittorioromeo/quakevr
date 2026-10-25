# Flare gun: art source

The disposable double-barrelled flare gun's model and skin, before it is a weapon (no gameplay yet): two short fat
over-and-under barrels with wide bores, a stamped sheet-iron body in rusty signal orange-red paint (rust patches, chipped
paint, bare metal at the worn edges), a break-open hinge block with a bolted strap on each side, a plain flat stamped-iron
top over the breeches ending in a low hump with the rear sight notch, a curved wooden grip with bolts, a small trigger.
After Vittorio's mockup (`qvr-kit/scratch/flaregun/mockup.webp`) with fixes: **the two barrels are exactly parallel**
(one shape about +X, its two copies' centres on the same vertical line), iron sights added, and the mockup's raised
breech housing with its two cartridges on top left out (Vittorio: a mockup artefact). Not reloadable: one model, no
moving parts.

| File | What |
| --- | --- |
| `flaregun.blend` | The source: objects `Body` (the gun) and `Flare` (the projectile), one material `FlareGunSkin` (the skin, nearest filtering), one UV atlas shared by both. Empties `_MuzzleTop`, `_MuzzleBottom`, `_SightRear`, `_SightFront` (children of Body, not exported) mark the points below. |
| `flaregun_skin.png` | Skin 0 (loaded), 256x256 RGB, already in Quake's palette: paint this one (in Blender or any image editor). |
| `flaregun_skin_fired1.png`, `flaregun_skin_fired2.png` | Overlays (RGBA): only the texels that differ from skin 0, the rest transparent. Skin 1 = skin 0 + fired1 (the top barrel fired: its muzzle sooted); skin 2 = skin 0 + fired2 (both muzzles sooted). A repaint of skin 0 carries over to them. |
| `flaregun_skin_pal.png` | Skin 0 as palette indices (8-bit, Quake's palette), written by every export. |
| `export_mdl.py` | Writes the MDLs from the .blend (below). |
| `build_flaregun.py` | Makes the .blend from nothing (shapes, UVs, a procedurally painted skin and its overlays) and exports. It **overwrites** the .blend and the skins: once they are edited by hand, don't run it. |
| `render_previews.py` | Renders the exported MDLs into `renders/` (below). |

Triangles: `v_flaregun.mdl` 860 (barrels 2 x 116, the 12 hex bolts 240, the rest the body, grip, sights, trigger),
`vr_flare.mdl` 116. Skin 256x256, 3 skins in `v_flaregun.mdl`; the flare uses skin 0 (the atlas).

## Axes, origins, scale

Quake's view weapon axes: **+X forward (the muzzles), +Y left, +Z up**; units are Quake model units, as `v_nail.mdl` and
`v_shot.mdl`. The gun is ~15.7 units long (muzzle to the grip's pommel), ~12.2 tall; at a view weapon's usual in-hand
scale (`vr_wofs_scale` ~0.45) that is ~28 cm, a little longer than the nail pistol, with a longer grip. Its barrels
are 8.1 units long (Vittorio, 2026-10-26: half as long again as the first art's 5.4; `MUZZLE_X` 10.5 in
build_flaregun.py, the front sight 0.25 behind the muzzle, the fired skins' soot at the new muzzles)
(`renders/scale_vs_nailpistol_shotgun.png`).

- `Body`: origin at the grip's hand point (two units down the grip's axis from where it meets the frame; the grip leans
  back 30 degrees). The MDL's origin.
- `Flare`: the projectile, exported in its own frame: origin its middle, its red cap along **+X**, 2.5 long, radius 0.42
  (0.47 at the brass rim).
- `Body`'s custom property `preview_scale` (0.45): the scale of the preview model.

**For the gameplay** (in `v_flaregun.mdl`'s frame, model units; export_mdl.py prints them from the empties):

| Point | Position | Axis |
| --- | --- | --- |
| Top barrel's muzzle (exit, bore centre) | 12.000 0 4.532 | +X |
| Bottom barrel's muzzle | 12.000 0 2.232 | +X |
| Rear sight (the notch's bottom, at its back edge) | 1.150 0 6.132 | |
| Front sight (the blade's top, on the top barrel) | 11.750 0 6.132 | |

The sight line runs from the rear notch to the front blade's top: y 0, z 6.132, **parallel to both bores** (+X), 1.6
above the top bore's axis. The bores are 1.6 wide (radius 0.8), 2.6 deep. Aim: the front blade's top (painted bright
brass) in the bottom of the rear notch. The rear notch (0.32 wide, 0.28 deep) is cut in a low hump at the rear end of
the frame's flat top, between its two cheeks; the hump ramps up from the flat top in front and ends square at the back,
and the frame's tang behind it slopes down, below the line. The hinge block's top (0.32 below the line) and the frame's
flat top stay under it.
(For `vr_sightalign`: rear `{1.15, 0, 6.132}`, front `{11.75, 0, 6.132}`.)

## Editing in Blender

Open `flaregun.blend` (saved by Blender 5.2: open it with 5.2 or later). Edit the meshes as usual (modifiers are applied
on export); keep the objects and their origins. Faces and UVs: an edited face keeps its UVs; a new one needs UVs in the
atlas (UV Editor, the image `flaregun_skin`). Paint in Texture Paint or in an image editor on `flaregun_skin.png` and save
it (Image > Save in Blender); any colour is fine, the export takes the nearest of Quake's 224 ordinary palette colours
(never the fullbright ones). Quake's palette has one red-orange ramp (indices 102-107) and the deep reds (64-79): the
signal paint lives there. The fired overlays are separate PNGs (open them in Blender or an image editor; transparent =
skin 0's texel).

The face attribute `fg_part` only tells `build_flaregun.py` what to paint where; nothing else reads it. An object whose
name starts with `_` is left out of the export (references, helpers). Moving an empty moves only what export_mdl.py
prints, not the model.

## Exporting

From the repository root:

    "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b Misc/quakevr/models_src/flaregun/flaregun.blend -P Misc/quakevr/models_src/flaregun/export_mdl.py

(add `-- <folder>` to write elsewhere). It writes, into `quakevr/progs/`:

- `v_flaregun.mdl`: every mesh but the Flare, in the Body's frame; one frame `rest`; skins 0 loaded, 1 top fired, 2 both.
- `vr_flare.mdl`: the Flare in its own frame (the projectile).
- `vr_flaregun_preview.mdl`: the gun scaled by `preview_scale`, skin 0: for looking at it in the game.

and `flaregun_skin_pal.png` here. Re-exporting the shipped .blend gives the shipped MDLs byte for byte. It uses
`Misc/quakevr/mdlgen.py` (the generated models' MDL writer) and the palette in
`Misc/quakevr/blender/addons/quakevr_models/qpal.py`. Art lint (needs numpy: Blender's own Python has it):
`"C:\Program Files\Blender Foundation\Blender 5.2\5.2\python\bin\python.exe" Misc/quakevr/check_mdl_art.py
quakevr/progs/v_flaregun.mdl quakevr/progs/vr_flare.mdl` (LOW only: bleed, and one overlap texel on the gun).

Renders: `blender -b --factory-startup -P Misc/quakevr/models_src/flaregun/render_previews.py [-- <view> ...]` imports
the exported MDLs (the quakevr_models add-on's importer) and writes `renders/`: `three_quarter`, `side` (the barrels
parallel), `front` (both bores), `back` (the rear notch), `sights` (the sight picture down the line), `skins` (skins 0, 1,
2), `flare`, `scale_vs_nailpistol_shotgun` (each at its in-hand scale, hand points on one line), `compare_mockup` (the
mockup's three-quarter beside ours).

## In the game

Not a weapon yet. To look at it, in VR, at the size it would be in your hand:

- Debug menu > Tools > Model Previews > **Flare Gun Art**: the preview model floats ahead at arm's reach (walk around
  it). Console: `vr_physics_spawn viewthing reach; viewmodel progs/vr_flaregun_preview.mdl`.
- The test scenario `vr_scenario flaregun_art` (vrtesthall): the same, with the nail pistol and the shotgun on the floor
  beyond it to compare (pick them up and hold them next to it); `flaregun_art` in the console shows it again.

(It is Quake's debugging `viewthing` given the model, so until the map changes `player.mdl` is drawn as the gun too.)
After a re-export, Debug > Tools > Reload Models (or a map change) reads the new file.
