# Widowmaker: art source

Arcane Dimensions' upgraded super shotgun as Quake VR's two-handed Gatling auto shotgun, made by hand after Vittorio's
mockup (`qvr-kit/scratch/widowmaker/mockup.png`, 2026-10-25): two long round barrels side by side, each with brass
collars and raised rings at the muzzle; a big chamfered octagonal receiver (its back a riveted plate of dark rusty iron
with red-brown stains, its sides a light steel panel with darker bars, its top an olive-brass cover with a raised
strip); vertical brick-brown grips on riveted iron feet, the rear one under the receiver's back, the front one under the
barrels. Nothing of AD's is in it (AD's own model is still what `vr_ad_widowmaker_art 0` makes the VR model from).

Its one moving part: **the barrels turn** (a Gatling), with their collars and muzzle rings, about the spin axis between
them; a still brass bearing ring round them carries the front grip and the front sight. A shot each half turn; at rest
the two side by side (`Quake/vr/vr_adweapons.cpp` spinFrame: "twin" frames, two barrels).

| File | What |
| --- | --- |
| `widowmaker.blend` | The source: objects `Body` (everything still) and `Barrels` (what turns), one material `WidowmakerSkin` (the skin, nearest filtering), one UV atlas. Empties `_Muzzle`, `_SightRear`, `_SightFront`, `_HandAnchor`, `_TwoHHandAnchor`, `_ShellPort`, `_EjectSlot` (children of Body, not exported) mark the points below. |
| `widowmaker_skin.png` | The skin, 256x256 RGB, already in Quake's palette: paint this one. |
| `widowmaker_skin_pal.png` | The skin as palette indices (8-bit), written by every export. |
| `export_mdl.py` | Writes the MDLs from the .blend (below). |
| `build_widowmaker.py` | Makes the .blend from nothing (shapes, UVs, a procedurally painted skin) and exports. It **overwrites** the .blend and the skin: once they are edited by hand, don't run it. |
| `render_previews.py` | Renders the exported MDL into `renders/` (below). |

Triangles: 1840 (`Body` 720: receiver, rear plate, top strip, sights, shell port and ejection slot frames, bearing
ring, grips and feet, trigger and guard, the muzzle mark; `Barrels` 1120: two 12-sided barrels with bores, two collars
and three muzzle rings each). The model made from AD's has 1230. Skin 256x256, one skin.

## Axes, origins, frame

Quake's view weapon axes, **+X forward, +Y left, +Z up**, in the frame of the Widowmaker made from AD's view model (the
super shotgun's: `vr_adweapons.cpp` recipes), so weapon slot 34's settings (Scale 0.48, Offsets, anchors, hotspots,
holsters) hold both models. The MDL is packed from the super shotgun's corner (-15.5775 -4.6938 -4.2402: the settings
scale about it; `export_mdl.py` CORNER, every vertex past it). `Body`'s origin is the model's origin.

| Point (model units) | Where | Used by |
| --- | --- | --- |
| 44 0 8.8 | the spin axis at the muzzles' plane, between the bores (a still speck marks it) | slot 34 MuzzleAnchor: the shots |
| 5.10 -0.05 0.71 | the rear grip's front (it leans back 10 degrees, as the super shotgun's grip) | slot 34 HandAnchor |
| 29.08 -0.04 -0.18 | the front grip's front | slot 34 TwoHHandAnchor (the other hand) |
| 6.5 3.6 5.6 | the shell port's middle (+y, the inner side; opening x 4.55..8.45, z 5.05..6.15) | `vr_view_internal.hpp` loadPorts |
| 7.25 -3.9 7.7 | the ejection slot (-y; x 5..9.5, z 7..8.4) | `vr_shells.cpp` |
| 12 0.85 11.4 | the receiver's top beside the strip | slot 34 WpnTextAnchor, WpnButtonAnchor (the ammo readout) |
| 1.7 0 12.45 / 28.8 0 12.45 | the rear sight's notch / the front post's top on the bearing ring: parallel to the bores, 3.65 over the axis | the sight line |

The spin axis: y 0, z 8.8, along x. The barrels' centres 1.72 either side of it (radius 1.55), their collars' corners
3.7 out at most, the bearing ring's flats 3.45 out (x 27.1..30.6).

## Editing in Blender

Open `widowmaker.blend` (Blender 5.2). Edit the meshes as usual; keep the objects, their origins and the barrels centred
on the spin axis (anything parented to `Barrels` turns with it). A new face needs UVs in the atlas. Paint on
`widowmaker_skin.png` (any colour: the export takes the nearest of Quake's 224 ordinary colours, never the fullbright
ones). The face attribute `wm_part` only tells `build_widowmaker.py` what to paint where. An object whose name starts
with `_` is left out of the export.

## Exporting

From the repository root:

    "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b Misc/quakevr/models_src/widowmaker/widowmaker.blend -P Misc/quakevr/models_src/widowmaker/export_mdl.py

It writes into `quakevr/progs/`: `v_widowmaker.mdl` (36 frames: `vr0`..`vr8` at rest, the weapon animations' frame
numbers; `twin0`..`twin26` the barrels turned a whole turn in 27 steps, clockwise seen from behind) and
`vr_widowmaker_preview.mdl` (at rest, scaled by Body's `preview_scale` 0.48: the size in the hand). It uses
`Misc/quakevr/mdlgen.py` and the flare gun's exporter's palette and triangle helpers.

Renders: `blender -b --factory-startup -P Misc/quakevr/models_src/widowmaker/render_previews.py [-- <view> ...]`
writes `renders/`: `three_quarter`, `first_person`, `side` (the inner side, the shell port), `side_out` (the ejection
slot), `front` (both bores), `back` (the rear plate and sights), `sights` (down the sight line), `spin` (frame twin7,
the barrels a quarter turn round), `compare_mockup` (the mockup's two halves above ours), `sheet` (side, side_out,
back, front, sights, spin on one page).

## In the game

`vr_ad_widowmaker_art` (1): the Widowmaker (`progs/v_shot3.mdl#vr`) is this model; 0 the one made from AD's view model
(three barrels), for A/B; a change reloads it. Debug > Tools > Model Previews: **Widowmaker Art** (the preview floats
ahead at arm's reach), **Widowmaker: Hand-Made Model** / **Model Made from AD's**. The scenario `widowmaker_art`
(vrtesthall) puts one in your hand and the preview ahead. After a re-export, Debug > Tools > Reload Models.
