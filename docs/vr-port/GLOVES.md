# Editing the gloves

The gloves (`vr_gloves` 1 fingerless, 2 full; VR Settings > Player Appearance > Gloves) are drawn on the jointed hand
(`vr_hand_rig 1`) by the model shader: where the skin mask says the glove covers, the skin is replaced by leather in
the colour you pick (`vr_glove_hue`, `_saturation`, `_lightness`). On top of that colour sits a **painted layer** you
can edit in any image editor (`vr_glove_paint 1`, the default; "Worn Leather" in the menu; 0 = plain new leather).

## The files

Both in `quakevr/progs/`, laid out on the hand's own skin (the same UV layout as `hand_rig_00_00.lmp` and the skin
masks, at twice their resolution: 1024 x 1024, about 3 texels a millimetre):

| File | Channels |
|---|---|
| `hand_rig_00_00_glove.png` | **RGB** the leather's detail, *multiplied* with the chosen colour: 128 grey leaves the colour as it is, darker darkens it (creases, grime), lighter lightens it (thread, scratches). A coloured texel shifts the hue (coloured thread). Paint in grey and every glove colour works. **A** the wear: 0 none, 255 worn through: the leather goes paler, greyer and matte there (a black glove's scuffs go grey, a brown one's tan). Knuckles, finger joints, fingertips. |
| `hand_rig_00_00_glove_height.png` | Greyscale relief: 128 flat, lighter raised, darker sunk (seam grooves, wrinkles, stitches, grain); 0..255 spans 2.5 mm. Only its slope matters: the light picks out the edges. The engine makes it a normal map as it reads it (steepened twice, as the bracers' and pauldrons' baked relief is), so edit the heights, never a normal map. Optional: without it the layer is flat. |

Every hand skin (the clean hand and the three bloodied ones, `hand_rig_00..03_00`) uses these. To give one of them
its own, save a copy as `hand_rig_0N_00_glove.png` (and `_glove_height.png`); it is used for that skin instead.
Without any `_glove` file the gloves draw the plain procedural leather as before.

Keep the alpha channel when you save (PNG RGBA, not premultiplied; most editors show it as transparency: a worn
texel looks *more opaque*). TGA and JPG load too (same name, other extension; PNG is found first).

How deep the relief shows in the light: `vr_glove_relief` (1; Player Appearance > Gloves > "Leather Relief"; 0 flat,
up to 2).

What the shader adds on its own regardless: the hem's rolled, rubbed edge round a fingerless glove's finger ends and a
bare forearm's cuff, a very fine grain for close up, and the sheen. Where a fingerless glove leaves the fingertips bare,
the painted layer is simply not used.

## Thickness

The glove is its own layer over the hand (`vr_glove_thickness`, 2 mm; Player Appearance > Glove Thickness; 0 paints
it on the skin only): the engine builds it as the hand loads from the hand's mesh and the skin mask's G/B (and, with
bare forearms, the cuff from its rows), so an edited hand or mask gets a matching layer at the next load
(`vr_hand_reload` or a map change). Nothing to edit for it; ROUND22.md, "Glove depth".

## Seeing an edit in the game

Save the file, then either change map (each map load reads the files again) or press **Debug > Rebuild and Reload >
Reload Glove Paint** (console: `vr_gloves_reload`). No restart needed.

## Regenerating

The shipped files come from `Misc/quakevr/make_glove_paint.py` (numpy and Pillow, deterministic):

    py -3.13 Misc/quakevr/make_glove_paint.py [--preview scratch/glove_preview.png]

It works from the hand rig's 3D layout (`make_hand_rig.py`): the knuckles and finger joints from the rig's joint
positions, the seams along the fingers from `make_skin_masks.py`'s seam lines (the same places as the mask's), the
palm's folds as the skin's creases. `--preview` writes a flat brown view of colour and wear.

It is guarded (`genguard.py`, `Misc/quakevr/generated.json`): once you edit either file by hand, the generator stops
and names it instead of overwriting your work. `--keep-edited` writes only the untouched files; `--force` overwrites
your edits (git keeps the last committed version).
