# Heavy revolver: art source

The heavy revolver's models and skins, before it is a weapon (no gameplay yet): a six-shot revolver for 12-gauge
shells after Vittorio's mockup (`qvr-kit/scratch/revolver/mockup.webp`): a dark iron hexagonal barrel with carved
runes along both flat sides and a blade front sight, a big fluted steel cylinder, a riveted brass octagonal frame plate
round it, a hooked hammer spur, a brass trigger guard and a curved trigger, a red-brown wooden grip wrapped in worn
leather straps. The cylinder swings out to the left on a crane; shells in red and in the revolver's blue slug colour.
Same pipeline as the nail pistol (`../nailpistol/`).

| File | What |
| --- | --- |
| `revolver.blend` | The source: objects `Body`, `Crane` (child of Body), `Cylinder` (child of Crane), `EjectorRod` (child of Cylinder), `Hammer`, `Trigger` (children of Body), `Shell` (on its own, beside the gun); material `RevolverSkin` (the gun's skin, one UV atlas for all six parts), `RevolverShellSkin` (the shell's). |
| `revolver_skin.png` | The gun's skin, 384x384 RGB, already in Quake's palette: paint this one. `revolver_skin_pal.png`: the indexed copy every export writes (what the MDLs hold). |
| `revolver_shell.png`, `revolver_shell_slug.png`, `revolver_shell_spent.png`, `revolver_shell_slug_spent.png` | The shell's four skins (64x64, one UV layout): red, the blue slug, both spent (darker, the crimp open, the primer struck). The Shell's `skins` property lists them in skin order. |
| `export_mdl.py` | Writes the MDLs from the .blend (below). |
| `build_revolver.py` | Makes the .blend from nothing (shapes, UVs, a procedurally painted skin), checks the crane's swing and exports. It **overwrites** the .blend and the skins: once they are edited by hand, don't run it. |
| `render_revolver.py` | Renders the exported MDLs (`renders/`): `blender -b --factory-startup -P render_revolver.py -- <out folder> [size] [view ...]`. |
| `renders/` | `three` (three-quarter, as the mockup), `vs_mockup` (beside it), `side`, `right`, `cocked`, `open` and `openfront` (swung out, shells chambered), `sights` (from behind along the sight line), `fp` (about as held), `shells`, `scale` (with the nail pistol and the shotgun, each at its in-hand scale). |

Triangles: the gun 914 (Body 490 with the sights, Hammer 88, Trigger 28, Crane 52, Cylinder and EjectorRod 256);
`v_revolver_shell.mdl` 76. Skin 384x384 (the nail pistol's 256x256: the runes need the texels).

## Axes, origins, scale

Quake's view weapon axes: **+X forward (the muzzle), +Y left, +Z up**; Quake model units, as `v_nail.mdl`,
`v_shot.mdl` and the nail pistol. The revolver is ~17.1 units long (muzzle to hammer spur): at a view weapon's usual
in-hand scale (`vr_wofs_scale` ~0.45) that is ~29 cm, its grip a little bigger than the nail pistol's
(`renders/scale.png`). Numbers below are in the Body's frame (the MDL's), as the export prints them.

- `Body`: origin at the grip's hand point (the grip's middle). Bore axis: y 0, z 7.058, along +X; muzzle at x 16.964.
- `Crane` (yoke): origin at its pivot **8.214 0.700 2.558**, low and left in a slot through the frame's bottom. It swings
  the cylinder out to the left about its local **X** (parallel to the bore) by its property `swing` (-95 degrees; the
  build checks the whole swing is clear of the frame, the hammer and the trigger: "swing check: clear").
- `Cylinder`: origin on its axis at its rear face, **3.764 0 5.608** at rest (-4.450 -0.700 3.050 in the Crane's frame).
  It spins about its local **X**: 60 degrees a chamber (its shape repeats every 60 degrees). Chambers at radius `pitch`
  (1.45) round the axis, chamber i at angle 90 + 60 i degrees from +Y towards +Z: chamber 0 on top, on the bore's line.
  Cylinder 4.4 long, radius 2.25 (fat enough for 12-gauge shells: walls 0.65 from each chamber's middle).
- `EjectorRod`: origin 8.164 0 5.608 (the cylinder's front, on its axis); pushed back along its local -X by `travel`
  (1.0) to eject.
- `Hammer`: origin at its pivot 2.164 0 5.658; cocks about its local **Y** by `cock` (-38 degrees: the spur back and
  down). Its spur is two prongs either side of the sight line.
- `Trigger`: origin at its pivot 4.464 0 2.808; pulled about its local **Y** by `pull` (22 degrees).
- `Shell`: its own frame: origin the rim's back face's centre, the hull along +X (into a chamber). 4.1 long, rim radius
  0.66, in the gun's units: drawn at the gun's scale (0.45) it is 7 cm, the size of `vr_shell_live.mdl` at world scale.
  Chambered: at `(-0.1, 1.45 cos a, 1.45 sin a)` in the Cylinder's frame (`a` = 90 + 60 i degrees), its rim 0.1 proud
  of the cylinder's face (the Shell's property `proud`).
- `Body`'s properties: `preview_scale` (0.45), `front_sight`, `rear_notch`.

**Sight line** (parallel to the bore, 2.12 above it): front blade top **16.714 0 9.178** (the blade's top front
corner), rear notch top **3.764 0 9.178** (the notch's top edges, at its back face; notch 0.30 wide, bottom at 8.898).
Look along it from behind: the blade sits in the notch, between the hammer's prongs (`renders/sights.png`).

## Models (quakevr/progs)

- `v_revolver.mdl`: all of it, one skin. Frames: 0 rest, 1 the hammer cocked, 2 the trigger pulled (hammer down), 3
  the cylinder swung out, 4 swung out with the rod pushed. Enough for a first weapon with frame animation only (the
  cylinder cannot spin in it).
- For code that moves the parts (recommended: spinning the cylinder and swinging the crane are rotations, which MDL
  frame interpolation turns into shrinking straight-line moves): `v_revolver_frame.mdl` (Body, Hammer, Trigger;
  frames 0 rest, 1 cocked, 2 trigger pulled), `v_revolver_crane.mdl` (the Crane in its own frame, origin its pivot),
  `v_revolver_cyl.mdl` (Cylinder and EjectorRod in the Cylinder's frame; frames 0 rest, 1 the rod pushed). Drawn as
  attached models: crane = body * T(crane origin) * Rx(swing); cylinder = crane * T(-4.450 -0.700 3.050) * Rx(spin);
  a chambered shell = cylinder * T(chamber).
- `v_revolver_shell.mdl`: the shell, skins 0 red, 1 blue slug, 2 red spent, 3 blue spent.
- `vr_revolver_preview.mdl`: all of it with six shells chambered (red and blue in turn), at `preview_scale`: frames 0
  rest, 1 swung out, 2 cocked. Its skin is the gun's with the red and blue shell skins below it (384x448).

## Editing in Blender

Open `revolver.blend` (saved by Blender 5.2). Edit the meshes as usual (modifiers are applied on export); keep each
part's object, origin and parent. Faces and UVs: an edited face keeps its UVs; a new one needs UVs in its atlas.
Paint `revolver_skin.png` (the gun) or the shell PNGs in Texture Paint or an image editor and save; any colour is fine,
the export takes the nearest of Quake's 224 ordinary palette colours. Changing `swing`, `cock`, `pull` or `travel`
changes the frames. An object whose name starts with `_` is left out; any other new mesh is drawn with the Body. The
face attribute `rv_part` only tells `build_revolver.py` what to paint where.

## Exporting

From the repository root:

    "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b Misc/quakevr/models_src/revolver/revolver.blend -P Misc/quakevr/models_src/revolver/export_mdl.py

(add `-- <folder>` to write elsewhere). It writes the six MDLs above into `quakevr/progs/` and
`revolver_skin_pal.png` here, and prints the parts' origins and the sight line in the Body's frame. Art lint:
`python Misc/quakevr/check_mdl_art.py quakevr/progs/v_revolver*.mdl quakevr/progs/vr_revolver_preview.mdl` (needs
numpy: Blender's own Python has it); today only LOW findings.

## In the game

Not a weapon yet. To look at it in VR, at the size it would be in your hand:

- Debug menu > Tools > Model Previews > **Heavy Revolver Art** (rest) and **Heavy Revolver Art: Swung Out**. Console:
  `vr_physics_spawn viewthing reach; viewmodel progs/vr_revolver_preview.mdl` then `viewframe 0/1/2` (rest, swung out,
  cocked).
- The test scenario `vr_scenario revolver_art` (vrtesthall, the shotgun beside it); then `revolver_art`,
  `revolver_art_open`, `revolver_art_cocked` in the console.

(It is Quake's debugging `viewthing` given the model, so until the map changes `player.mdl` is drawn as it too.)
After a re-export, Debug > Tools > Reload Models (or a map change) reads the new file.
