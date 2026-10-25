# Widowmaker: art source

Arcane Dimensions' upgraded super shotgun as Quake VR's two-handed Gatling auto shotgun, made by hand after Vittorio's
mockup (`qvr-kit/scratch/widowmaker/mockup.png`, 2026-10-25) and his notes on it: three long round barrels in an even
triangle (one on top, two below), each with brass collars and raised rings at the muzzle; a big chamfered octagonal
receiver (its back a riveted plate of dark rusty iron with red-brown stains and rust runs, its sides a steel panel with
darker bars, its top an olive-brass cover with a raised strip); vertical brick-brown grips on riveted iron feet, the
rear one under the receiver's back, the front one hung from an iron rail under the barrels. Nothing of AD's is in it
(AD's own model is still what `vr_ad_widowmaker_art 0` makes the VR model from).

Its one moving part: **the barrels turn** (a Gatling), with their collars and muzzle rings, about the spin axis at the
triangle's middle: a shot each third of a turn, at rest a barrel on top, as the model made from AD's
(`Quake/vr/vr_adweapons.cpp` spinFrame). Their whole sweep stays inside the receiver's outline (their back ends never
show) and above the rail. No ring round them (his notes): the front grip's still mount is the rail, carried forward
from the receiver's bottom under the sweep.

| File | What |
| --- | --- |
| `widowmaker.blend` | The source: objects `Body` (everything still) and `Barrels` (what turns), one material `WidowmakerSkin` (the skin, nearest filtering), one UV atlas. Empties `_Muzzle`, `_SightRear`, `_SightFront`, `_HandAnchor`, `_TwoHHandAnchor`, `_ShellPort`, `_EjectSlot` (children of Body, not exported) mark the points below. |
| `widowmaker_skin.png` | The skin, 512x512 RGB, already in Quake's palette: paint this one. The rear plate and the grips (and their feet) have 1.7 times the texel density of the rest. |
| `widowmaker_skin_pal.png` | The skin as palette indices (8-bit), written by every export. |
| `export_mdl.py` | Writes the MDLs from the .blend (below). |
| `build_widowmaker.py` | Makes the .blend from nothing (shapes, UVs, a procedurally painted skin) and exports. It **overwrites** the .blend and the skin: once they are edited by hand, don't run it. |
| `render_previews.py` | Renders the exported MDL into `renders/` (below). |

Triangles: 2592 (`Body` 732: receiver, rear plate, top strip, sights, shell port and ejection slot frames, rail and its
strap, grips and feet, trigger and guard, the muzzle mark; `Barrels` 1860: three 12-sided barrels with bores, three
collars and three muzzle rings each). The model made from AD's has 1230. Skin 512x512, one skin.

The relief (normal map with heights) is made by the engine from the skin as the model loads, as for every weapon made
under a `#vr` name (`Quake/vr/vr_skinrelief.cpp`: the dark mortar and seams grooved, the painted rivets raised; cached by
the skin's contents, so a repaint gets a new one). Nothing to bake: `bake_normals.py` bakes files loaded under their own
names, and this one is loaded as `progs/v_shot3.mdl#vr`.

## Axes, origins, frame

Quake's view weapon axes, **+X forward, +Y left, +Z up**, in the frame of the Widowmaker made from AD's view model (the
super shotgun's: `vr_adweapons.cpp` recipes), so weapon slot 34's settings (Scale 0.48, Offsets, anchors, hotspots,
holsters) hold both models. The MDL is packed from the super shotgun's corner (-15.5775 -4.6938 -4.2402: the settings
scale about it; `export_mdl.py` CORNER, every vertex past it). `Body`'s origin is the model's origin.

| Point (model units) | Where | Used by |
| --- | --- | --- |
| 44 0 8.8 | the top barrel's bore's middle at the muzzle (a still speck marks it) | slot 34 MuzzleAnchor: the shots |
| 5.10 -0.05 0.71 | the rear grip's front (it leans back 10 degrees, as the super shotgun's grip) | slot 34 HandAnchor |
| 29.08 -0.04 -0.18 | the front grip's front | slot 34 TwoHHandAnchor (the other hand) |
| 6.5 3.6 5.6 | the shell port's middle (+y, the inner side; opening x 4.55..8.45, z 5.05..6.15) | `vr_view_internal.hpp` loadPorts |
| 7.25 -3.9 7.7 | the ejection slot (-y; x 5..9.5, z 7..8.4) | `vr_shells.cpp` |
| 12 0.85 11.4 | the receiver's top beside the strip | slot 34 WpnTextAnchor, WpnButtonAnchor (the ammo readout) |
| 1.7 0 11.9 / 20.1 0 11.9 | the rear sight's notch / the front post's top, both on the receiver: parallel to the bores, over the barrels' sweep | the sight line |

The spin axis: y 0, z 6.98, along x. The barrels' centres 1.82 from it (radius 1.45; the top one at z 8.8), their
sweep 3.27 out (z 3.71..10.25), the collars' 3.54 (z 3.44); the rail under it (x 19..31.4, z 2.05..3.2).

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
numbers; `spin0`..`spin26` the barrels turned a whole turn in 27 steps, clockwise seen from behind, as the model made
from AD's) and `vr_widowmaker_preview.mdl` (at rest, scaled by Body's `preview_scale` 0.48: the size in the hand). It
uses `Misc/quakevr/mdlgen.py` and the flare gun's exporter's palette and triangle helpers.

Renders: `blender -b --factory-startup -P Misc/quakevr/models_src/widowmaker/render_previews.py [-- <view> ...]`
writes `renders/`: `three_quarter`, `first_person`, `side` (the inner side, the shell port), `side_out` (the ejection
slot), `front` (the three bores), `back` (the rear plate and sights), `sights` (down the sight line), `spin` (frame
spin4, the barrels 53 degrees round, from the front), `compare_mockup` (the mockup's two halves above ours), `sheet`
(side, side_out, back, front, sights, spin on one page).

## In the game

`vr_ad_widowmaker_art` (1): the Widowmaker (`progs/v_shot3.mdl#vr`) is this model; 0 the one made from AD's view model,
for A/B; a change reloads it. Debug > Tools > Model Previews: **Widowmaker Art** (the preview floats ahead at arm's
reach), **Widowmaker: Hand-Made Model** / **Model Made from AD's**. The scenario `widowmaker_art` (vrtesthall) puts one
in your hand and the preview ahead. After a re-export, Debug > Tools > Reload Models.
