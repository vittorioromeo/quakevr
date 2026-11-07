# Nail pistol: art source

**Archive (2026-11).** This folder is the nail pistol's first art, kept for its skin and renders. It no longer
makes the shipped gun: the full slide was built from `v_nailpistol_classic.mdl` and edited since, so the shipped
`v_nailpistol.mdl` and `v_nailpistol_mag.mdl` come from their own sources,
`Misc/quakevr/src_models/blend/v_nailpistol/v_nailpistol.blend` and `.../v_nailpistol_mag/v_nailpistol_mag.blend`
([MODELS_IN_BLENDER.md](../../../../docs/vr-port/MODELS_IN_BLENDER.md), section 9): edit those and press Ctrl+S
(export on save), or run `blend_export.py` on them. The scripts that were here (`build_nailpistol.py`,
`build_nailpistol_slide.py`, `export_mdl.py`) are removed (they are in git history); the text below describes
the archive as it was.

The nail pistol's model and skin, before it is a weapon (no gameplay yet): a compact, semi-automatic, Quake 1 style
pistol of rusty riveted iron, a hexagonal muzzle, three vent slots in each side, a copper feed block on top (the slide,
which snaps back per shot), iron sights, an angled grip, a thick trigger guard, and a separate magazine with a copper
baseplate and nails showing at its top. After Vittorio's mockup (`qvr-kit/scratch/nailpistol/mockup.webp`) with the
agreed changes (separate magazine, iron sights, separate slide, thicker guard and grip, Quake-contrast skin).

| File | What |
| --- | --- |
| `nailpistol.blend` | The source: objects `Body`, `Slide`, `Trigger`, `Magazine` (children of `Body`), one material `NailPistolSkin` (the skin, nearest filtering), one UV atlas shared by all four. |
| `nailpistol_skin.png` | The skin, 256x256 RGB, already in Quake's palette: paint this one (in Blender or any image editor). |
| `nailpistol_skin_pal.png` | The same skin as palette indices (8-bit, Quake's palette), written by every export: what the MDLs hold. |
| `renders/` | Blender renders (front-left, side, the magazine), the scale against the nailgun and shotgun, an in-game screenshot, the trigger guard beside the mockup's (`guard_vs_mockup.png`). |

Triangles: `v_nailpistol.mdl` 489 (Body, Slide, Trigger), `v_nailpistol_mag.mdl` 104. Skin 256x256 (v_nail.mdl's is
512x128), one skin for all three models.

## Axes, origins, scale

Quake's view weapon axes: **+X forward (the muzzle), +Y left, +Z up**; units are Quake model units, as `v_nail.mdl`
and `v_shot.mdl`. The pistol is ~13.2 units long: at a view weapon's usual in-hand scale (`vr_wofs_scale` ~0.45) that is
~23 cm, its grip about as big as the nailgun's (`renders/scale_vs_nailgun_shotgun.png`).

- `Body`: origin at the grip's hand point (the grip's middle, two units down its axis). The MDL's origin.
- `Slide`: origin at its rest point (its bottom middle). It snaps back along its local **-X** by its custom property
  `travel` (3.0): the export writes that as frame 1. The game draws it back by vr_nailpistol_slide_travel (0..3,
  default 1.5) as frames 0 and 1 blended (Quake/vr/vr_nailslide.cpp measures frame 1's travel from the model).
  The shipped MDL's frame 1 was moved from 1.0 to 3.0 back without Blender (its slide's vertices' offset scaled,
  re-quantised as the export's writer does: ROUND23.md, "Nail pistol: moving slide"); the .blend keeps 1.0 until
  the full slide was built.
- `Trigger`: origin at its pivot (its top), so it can be rotated about Y for a pull later.
- `Magazine`: origin at its top; it comes out along its local **-Z** (down the grip, which leans back 12 degrees). The
  export prints where it sits in the Body's frame: origin 0.468 0 2.201, rotated 12 degrees about Y.
- `Body`'s custom property `preview_scale` (0.45): the scale of the preview model below.

## Editing in Blender

Open `nailpistol.blend` (saved by Blender 5.2: open it with 5.2 or later). Edit the meshes as usual (modifiers are applied on export); keep each
part's object and origin. Faces and UVs: an edited face keeps its UVs; a new one needs UVs in the atlas (UV Editor,
the image `nailpistol_skin`). Paint in Texture Paint or in an image editor on `nailpistol_skin.png` and save it (Image >
Save in Blender); any colour is fine, the export takes the nearest of Quake's 224 ordinary palette colours (never the
fullbright ones). Quake's look: a few flat colours, darker crevices, brighter worn edges, 1-texel detail.

The face attribute `np_part` only told the removed `build_nailpistol.py` what to paint where; nothing reads it. An object
whose name starts with `_` is left out of the export (references, helpers).


## In the game

Not a weapon yet. To look at it, in VR, at the size it would be in your hand:

- Debug menu > Tools > Model Previews > **Nail Pistol Art**: the preview model floats ahead at arm's reach (walk around
  it). Console: `vr_physics_spawn viewthing reach; viewmodel progs/vr_nailpistol_preview.mdl`.
- The test scenario `vr_scenario nailpistol_art` (vrtesthall): the same, with the nailgun and the shotgun on the floor
  beyond it to compare (pick them up and hold them next to it); `nailpistol_art` in the console shows it again.

(It is Quake's debugging `viewthing` given the model, so until the map changes `player.mdl` is drawn as the pistol too.)
After a re-export, Debug > Tools > Reload Models (or a map change) reads the new file.
