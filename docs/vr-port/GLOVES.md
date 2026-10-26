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

## Painted nails

`vr_nails 1` (VR Settings > Player Appearance > Nails > Painted Nails; off by default) paints the jointed hand's
fingernails (`vr_hand_rig 1` only) in the colour you pick: `vr_nail_hue` (355), `_saturation` (0.85), `_lightness`
(0.32: a classic deep red) and `vr_nail_gloss` (0.6; 0 matte, 1 a glassy top coat). Full gloves cover them; fingerless
gloves leave them showing. `vr_skin_tone` tints the skin round the nail, never the polish.

The mask is `quakevr/progs/hand_rig_0N_00_nails.png` (one per hand skin, 1024 x 1024 like the glove paint, greyscale:
255 polish, 0 none, a ~0.3 mm soft edge). It covers each nail plate as `make_hand_rig.py` paints it: from the cuticle's
curve (the middle nearest the knuckle) out to the free edge, its sides just inside the nail folds; the bloodied skins'
static blood is cut out of theirs so the blood stays over the polish. Paint over it in any editor to change the shape
(a missing skin's file falls back to skin 0's); **Reload Glove Paint** (`vr_gloves_reload`) or a map change reads it
again. Regenerate with `py -3.13 Misc/quakevr/make_nail_mask.py [--preview scratch/nails_preview.png]` (guarded like
the glove paint: it won't overwrite a hand-edited mask without `--force`).

The shader (`vr_glsl.h` QVR_ALIAS_FS_LIGHT, `NailGloss`): inside the mask the nail takes the polish's colour times a
little of its own painted shading (its lighter free edge), its bumps smoothed (a coat), and the gloss adds a highlight
in the hand's light (sharper with more gloss) and the room's light mirrored at grazing angles. With `vr_nails 0` the
mask is never loaded and the shader's nail code is skipped (the instance's Nails.w is 0).

## Randomize

VR Settings > Player Appearance > Randomize (console: `vr_appearance_random [seed]`) sets a random skin tone, gloves
(none 50 percent, fingerless 25, full 25) with leather-like colours, and painted nails (on half the time) with
polish-like colours and a gloss of 0.3-1. It leaves the gloves' thickness, worn leather and relief, and every other
setting, alone. Ranges: ROUND22.md, "Player Appearance: Randomize".

## Hand shape

VR Settings > Player Appearance > Hand Shape (`vr_hand_shape`: 0 standard, the default; 1 feminine, slimmer;
HANDS_IN_BLENDER.md, "The feminine hand"). The feminine hand shares the standard hand's texture layout, so the
gloves (their masks, painted leather, relief and shells), the nails' masks, the skins and skin tone all work on it
unchanged: nothing here needs a second copy. Randomize picks it 35 percent of the time. It changes at once (the
first switch loads the model); with the older hands (`vr_hand_rig 0`) it does nothing.
