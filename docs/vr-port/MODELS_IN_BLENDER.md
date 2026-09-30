# Editing the body, the weapons, the gadget, the flashlight and the other models in Blender

You can open these models in Blender, change their shape and their skins, save them, and see them in the game
without a restart:

| Model | Files in `quakevr/progs` |
|---|---|
| The body (torso, head, arms, legs), in three builds | `vrbody_lean`, `vrbody` (athletic), `vrbody_brawny`: each a `.md5mesh` and a `.md5anim`; 16 skins shared by the three, `vrbody_00_00.tga` .. `vrbody_15_00.tga` |
| The weapons | `v_*.mdl` (`v_shot.mdl`, `v_nail2.mdl`, `v_axe.mdl`...) |
| The wrist gadget | `vrgadget.mdl` (the casing) and `vrgadget_strap.mdl` (a strap) |
| The flashlight, the holster, the pauldrons, the spent shell, the ammo button... | every other `.mdl`: see [the table](#the-flashlight-and-the-other-models) |

The hand has its own add-on and guide: [HANDS_IN_BLENDER.md](HANDS_IN_BLENDER.md).

The rules, in short:

- **The body:** edit the mesh, its weights, its UVs and its skins. **Don't move, rename or delete bones:** the game
  places them itself, from your tracked head and hands.
- **The weapons and the gadget:** edit anything. The export keeps what the game hangs off the model: its anchors
  (where your hand, the muzzle, the ammo screen sit), its frames and its size grid. It tells you if an edit moves an
  anchor, and refuses an edit that would silently break one.
- **The flashlight:** edit anything. The game reads its lens, its tail and its outline from the model as it loads it:
  the beam and the light follow a lens you move or enlarge.

## 1. Install the add-on (once)

The add-on is `Misc/quakevr/blender/addons/quakevr_models`, next to the hand's. It is written for Blender 5.2.

If you installed the hand's add-on with a script directory, you already have this one: in **Edit > Preferences >
Add-ons**, search for **Quake VR Models** and tick it. Restart Blender first if it isn't listed (Blender reads a
script directory's contents at startup).

Otherwise:

1. **Edit > Preferences > File Paths > Script Directories > Add**: choose the repository's `Misc/quakevr/blender`
   folder, name it `quakevr`.
2. Save the preferences (unless Auto-Save Preferences is on), then **close and reopen Blender**.
3. **Edit > Preferences > Add-ons**: search for **Quake VR Models** and tick it.

Or install it as a zip: zip the `quakevr_models` folder (the zip must contain the folder), then **Preferences >
Add-ons**, the menu at the top right > **Install from Disk...**. Reinstall it when the add-on changes.

The buttons are in the 3D view's sidebar (press N) in the **Quake VR** tab, under **Quake VR Models**. The imports and
exports are also in **File > Import** and **File > Export**.

## 2. The weapons and the wrist gadget

### Import

Start from an empty scene (File > New > General, delete the cube, camera and light), then **File > Import > Quake VR
Model: Weapon, Gadget (.mdl)** and pick the model in `quakevr/progs`. You can import several models into one scene.

You get one mesh object, named after the model (`v_shot`):

| What | |
|---|---|
| The mesh | The model's vertices that are at the same place in every frame are one vertex, so edits don't tear the model apart (the file splits vertices along the skin's seams and wherever the shading is flat). One unit is one Quake unit, as the model file has it: +X is the gun's forward, +Z up. |
| Frames | Frame 0 is the mesh (the Basis shape key). Every other frame (the firing animation) is a shape key with the frame's name (`shot2`, `shot3`...). Scene frames 1, 2, 3... show the model's frames 1, 2, 3...: play or scrub the timeline to see the animation. |
| Skin | The skin is the image `v_shot_skin0`, on the object's material, drawn without filtering. |
| Shading | Faces are smooth where the model's shading is smooth and split by sharp edges where it is flat. UV seams are marked where the skin is split. |
| `qvr_*` attributes | They map Blender's vertices, corners and faces back to the file's. Leave them. |

### Edit

**Shape.** Work on frame 0 in Edit Mode or Sculpt Mode: move, scale, rotate, extrude, subdivide, add or delete.

- **Your edits of frame 0 are carried into every frame.** Each vertex you move follows the motion of the part it is
  on: the shotgun's recoil, the pump, the nailguns' spinning barrels. New vertices follow the part they are joined
  to. Blender's own preview of the other frames only moves them by the same amount; click **Carry Frame 0 Edits into
  Frames** in the sidebar to see exactly what the export will write.
- **You can edit a frame on its own.** Select its shape key and edit it. The export keeps a frame you edited as you left
  it, and says which.
- **Keep the frames.** Don't add or delete shape keys, and keep every key relative to the first.

**Skin.** Paint `v_shot_skin0` in Texture Paint mode, or in another editor (section 5).

- The export puts every changed texel on Quake's palette.
- **Glowing colours** (the sights, the ammo screens) stay only where they already are. A texel that glowed can be
  repainted in another glowing colour, or made ordinary. An ordinary texel never starts glowing.
- **Keep the skin's size.** The UVs are in its texels.
- **Leave the skin's top-left corner alone.** The engine takes the flat area at texel (0, 0) as background and paints
  over it with its neighbours' colours. Paint that area in the corner's colour, or change the corner's colour, and your
  paint there won't show. The export warns you.

**UVs.** You can move UVs. A UV edit that every face at a vertex agrees on is written in place. Faces that split away
get their own vertices.

### Do

- **Keep anchor vertices where they are, unless you mean to move them** (see [Anchors](#anchors)). The export report
  names every anchor and where it is.
- **Stay inside the model's box** (see [The box](#the-box)).
- **Keep one object per model.** Join parts back with Ctrl+J. Apply other modifiers first: only the mesh is exported.

### Don't

- **Don't delete or rebuild the model's old faces around its anchors.** Deleting old triangles can make an anchor
  index name another vertex. The export refuses that (see [Anchors](#anchors)). Moving, scaling and extruding are fine.
- **Don't sort the mesh's elements** (Mesh > Sort Elements). The export tells old vertices from new ones partly by
  their order.
- **Don't rename or delete the skin image.** The export writes the images named `<model>_skin<n>`.

### Export

Click **Export Model** in the sidebar, or **File > Export > Quake VR Model**. The path defaults to the file you
imported: leave it.

The export writes the `.mdl` and keeps:

- the header's scale and origin (the weapon's Scale turns about the origin);
- the frame count, names and groups; the skin count and size;
- **every old vertex's index**: a deleted vertex stays in the file, unused. New vertices and triangles go after the old
  ones, and new triangles never share a vertex with old ones, so the old triangles' strip order (the anchors'
  numbering) doesn't change;
- every byte you didn't change (vertex, normal, texel, frame bounds). **An unedited model comes back byte for byte.**

Faces with more than 3 corners are split into triangles, in your mesh too: the model is triangles.

After writing, the export checks the model and reports:

- **The anchors,** before and after (below).
- **Holes:** `check_mdl_holes.py` on the old and the new model. New see-through gaps, wide cracks or flipped faces
  are named with where they are.

The report is printed in Blender's console and kept in the Text Editor, as the text **Quake VR export report**. If an
anchor moved, or a hole appeared, a warning pops up.

After a successful export, the object stands for the file written: you can keep editing and exporting. The mesh is
snapped to the model's grid, which is what the game draws.

### Anchors

The game places things on a weapon by its vertices (vr_weapons.inc's `*_av` settings and your `vr_wofs_*_av`):

- the hand's anchor (`hand_av`), and the off hand's for two-handed guns (`2h_hand_av`);
- the muzzle (`muzzle_av`): shots, flashes, the laser sight;
- the ammo screen and its button (`wpntxt_av`, `wpnbtn_av`);
- the shell ejection port (vr_shells.cpp).

Your weapon poses hang off them. The report lists each anchor of each slot using the model:

```
  strip order: unchanged (every anchor index names the same vertex as before)
  ok     slot 1 the hand's (vr_wofs_hand_av_02): index 165 = vertex 120 at (2.06, 0.02, 1.28)
  MOVED  slot 1 the muzzle's (vr_wofs_muzzle_av_02): index 1 = vertex 1 at (33.07, -0.66, 3.01) -> (33.07, -0.66, 3.35): moved 0.333 units (0.85 cm at the default size) at most over the frames
```

- **ok:** the anchor is where it was.
- **MOVED:** you moved that vertex. The export writes the model, and warns. Whatever hangs off the anchor moves with
  it: check the weapon's poses in the game.
- **RENAMED:** deleted or changed old triangles changed the strip order, and the index now names another vertex. The
  export **refuses** and writes nothing. Undo the deletion, or tick **Remap Anchors**: the export then writes the model
  and prints the new indices to set in the console, for example:

  ```
    vr_wofs_hand_av_02 172        (was 165)
  ```

  Paste them into the console (the settings are saved). To ship them, change the same numbers in
  `Quake/vr/vr_weapons.inc` (and `vr_shells.cpp` for a shell port).

The report also says from which index on the strip order changed. Anchors below it are safe, and so are your own
`vr_wofs_*_av` settings if they are below it.

### The box

An `.mdl` stores each vertex as a byte per axis: its place is `origin + byte × scale`. The header's origin and scale
make a box, and every vertex must be inside it, in every frame. The export keeps the box, so an edit that reaches
past it is **refused**: the vertices outside are selected, and the message names the frames.

Tick **Grow the Box** to let the export make the box bigger:

- The grid gets coarser along the axes that need it. Every vertex goes onto the new grid, moving up to half a step
  (the report says how much). The anchors move by as little.
- Past the low side, the origin moves too. The weapon's Scale turns about the origin, so the drawn weapon would move
  by `(origin moved) × (1 - Scale)`. The export prints the offsets that keep it in place, for each slot using the
  model:

  ```
    inc vr_wofs_y_02 0.0430        (slot 1 at Scale 0.44; with another Scale: 0.0768 x (1 - Scale))
  ```

  Paste them into the console. The gadget has no Scale setting, so its origin can move freely.

### The wrist gadget

The same steps. The game doesn't place the gadget's parts by vertices but by points in its model space
(`vr_gadget.cpp`, `vr_view.cpp`):

- the screen: x -1.5 .. 1.5, y -0.93 .. 0.93, drawn at z 0.43, just over the screen's face;
- the hologram and the log, which rise from the screen's centre;
- the casing's outline, 3.8 × 2.6 × 0.7, which the hologram floats over;
- the lugs at x ±1.25, z -0.47, where the straps are drawn.

The report lists the vertices round each point, before and after. It also warns if anything now stands over the
screen, where it would cover the HUD. If you move the screen, the HUD stays where the code puts it: change those
numbers in the code too.

The strap (`vrgadget_strap.mdl`) is a band round the forearm: radius 1 is the bracer's surface, x -1 .. 1 the band's
width. The game scales it to your build's bracer. Frame 1 is the band as a cone, blended in where the bracer narrows.
The report gives its inner radius and width in each frame.

### The flashlight and the other models

Every `.mdl` in `quakevr/progs` imports and exports with the same steps (File > Import > Quake VR Model), byte for
byte when you change nothing: id's monsters and items too. Some are placed by points written in the game's code; the
export's report checks those, before and after, and a line starting with `CHECK` says exactly what to change.

| Model | What the game takes from it | If you change it |
|---|---|---|
| `vrflashlight.mdl` the flashlight | **Read from the model** as it loads (`vr_flashlight.cpp`): the lens, its centre and radius (the beam, the lens's glow and the light start there, that wide), the tail (the belt clip holds it there, the cord goes in there), the switch (its clicks), its outline (how close it sits under or beside a gun, how far in front of the belt clip). Fixed: +x is the beam, +z the switch's side (towards the knuckles in the fist, away from the body elsewhere), the origin is the middle of the grip, where the fist holds it | Anything. The lens is **the part painted fullbright in skin 1** (the "on" skin; the game draws skin 0 and its own glow): keep the lens's faces mapped there, or paint where they are. Keep it facing +x, the switch on +z, the origin inside the grip. Thicker or thinner grip: the fingers wrap it by themselves (Fingers set to Manual on the Flashlight page: retune them) |
| `legholster.mdl` the hip holsters | The weapon hangs at the origin (the loops go round it); the plate's back rests on the body, 2.95 from the origin along +y (`vr_view.cpp` `plateBack`); +x forward, +z up | The report gives the back's place: if it moves, it prints the new `plateBack` |
| `vrpouch.mdl` the grenade pouch | Its back against the body at x 0, the origin the middle of its back (where the body's surface is under the pouch); +x out of the body, +z up; frame 0 full, frame 1 empty (the engine picks it from your rockets: `vr_view.cpp` `setupPouch`) | Keep the origin on its back and both frames (the same vertices) |
| `vrpauldron.mdl`, `vrpauldron_arm.mdl` the pauldrons | The origin on the left shoulder joint (the right one is drawn mirrored); 5 skins: 0 leather, 1-3 the armour's colour, 4 steel (Pauldron Style) | Keep the origin and the 5 skins |
| `vr_shell.mdl` the spent shotgun shell | Its axis along +x, the origin in its middle (it tumbles about it); the rim's radius 1.12 cm, how high a lying shell's middle is (`vr_shells.cpp` `shellRadius`) | The report prints the new `shellRadius` if the rim changed |
| `wpnbutton.mdl` the ammo button on the guns | Drawn at the gun's button anchor; a fingertip within 2.7 units of its origin presses it | Keep it centred on the origin |
| `hand_base.mdl`, `finger_*.mdl` the unrigged hand | Drawn only with `vr_hand_rig 0`; the rigged hand (`hand_rig`, the hand add-on) is built from them | After an edit run `make_hand_rig.py` (and `make_bloody_hands.py` for the blood skins 1-3 after a skin 0 edit): the report says so |
| `knight.mdl`, `hknight.mdl` | The sword hidden as a knight dies, by its vertex numbers and the model's counts (`vr_monstermods.cpp` `knownSwords`); the dropped sword (`v_ksword.mdl`, `v_hksword.mdl`) is cut from them by `make_swords.py` | Keep the sword's vertices, or update `knownSwords` (the report says); moved the sword: run `make_swords.py` |
| `ogre.mdl` | Its chainsaw hidden as an ogre dies, by its vertices 416..496 and the model's counts (`vr_monstermods.cpp` `knownSwords`); the dropped chainsaw (`v_chainsaw.mdl`) is cut from it by `make_chainsaw.py` | Keep the chainsaw's vertices, or update `knownSwords` (the report says); moved the chainsaw: run `make_chainsaw.py` |
| `v_chainsaw.mdl` the chainsaw | Its cord's T-handle **read from the model** (`vr_chainsaw.cpp`): the vertices that differ between frames 0 and 9 are the handle (their middle in frame 0: where the hand takes it; where they meet in frame 9: the cord's hole); its bar: the vertices past 58% of its length along +x (they may sink into monsters). Round 21's close-up pass (`make_chainsaw.py` `polish`: the starter's housing under the handle, the bar's nuts, edge wear) only appends | Keep frame 9 as frame 0 with the handle collapsed into its hole, +x along the bar, the origin in the rear handle's grip (or retune slot 20's Offset) |
| `soldier.mdl`, `enforcer.mdl` | Their guns hidden as they die, by their vertices (the soldier's 463..548; the enforcer's 22, 23, 100, 400..430, 455..478) and the models' counts (`vr_monstermods.cpp` `knownSwords`; the soldier's death frames by number, 8..28); the dropped guns (`v_gruntgun.mdl`, `v_enfrifle.mdl`) are cut from them by `make_enemyguns.py` | Keep the guns' vertices, or update `knownSwords` (the report says); moved a gun: run `make_enemyguns.py` |
| `v_crowbar.mdl` the crowbar | Slot 23 (`vr_weapons.inc`): the muzzle by anchor index (200: the hook's back where the bar's line meets it, also the Blade hotspot's axis), the hotspots in model space; its Offset depends on the model's bounds (the weapon scales about their corner: `vr_hotspot_fit` shows where the hand is on it) | Laid where the swords are (along the axe's handle, `make_crowbar.py`); keep the vertices' order (anchors) |
| `v_gruntgun.mdl`, `v_enfrifle.mdl` the enemy guns | Slots 21 and 22 (`vr_weapons.inc`): the muzzle and the counter by anchor index (80 and 23; 11 and 37), the foregrip hotspot in model space | +x along the barrel, the origin in the pistol grip's middle (or retune the slot's Offset); keep the old vertices' order (anchors) |
| The weapons, the gadget | Section 2 | Section 2 |
| id's other models (monsters, items, gibs...) | Their frames and skins only | Anything |

Not editable here:

- **The hand** (`hand_rig.*`): its own add-on, [HANDS_IN_BLENDER.md](HANDS_IN_BLENDER.md).
- **The firing range's monster buttons** (`maps/vr_spawnpanel.bsp`, `vr_spawnbutton.bsp`): brush models, compiled by
  `make_spawn_buttons.py` from brushes written in it (TrenchBroom's world, not Blender's).
- **`s_bullet.spr`**: a sprite, not a model.
- **`hand.mdl`** is never drawn (it names the fist's weapon slot), and **`openhand.mdl`** and **`vrtorso.mdl`** aren't
  used at all: they import and export, but nothing shows the change.

After exporting the flashlight, `vr_model_reload vrflashlight` prints what the game read:

```
vr_model_reload: progs/vrflashlight.mdl: lens at (2.475 0.000 0.301), radius 0.582; tail at x -1.365, switch at
(0.842 0.000 0.400); 14.8 cm long, 4.8 cm thick at most
```

A model whose lens it can't find (nothing fullbright in skin 1) says `(NOT from the model: the default)` there, and
the console warns: the beam then starts where the shipped torch's lens is.

## 3. The body

### Import

**File > Import > Quake VR Body (.md5mesh)**, and pick `quakevr/progs/vrbody.md5mesh` (athletic), `vrbody_lean` or
`vrbody_brawny`. Before clicking Import, choose the **Skin** to paint in the side panel. The default is the plain skin,
`vrbody_00_00`.

You get:

| Object | What it is |
|---|---|
| `vrbody_rig` (armature) | The 29 joints: `pelvis`, `spine`, `chest`, `neck`, `head`, the `clavicle`, `upperarm`, `forearm` and `hand` of each side (`_l`, `_r`), `thigh`, `calf` and `foot`. The forearms' twist helpers (`foretwist1..4`, `wrist`) are in the bone collection `forearm twist`. Each bone points along the joint's +X (to its child), with its Z the joint's hint (forward for the spine and legs, back for the arms). |
| `vrbody` (mesh) | The body. A vertex group per joint holds its weights. Vertices that the file splits along the skin's seams are joined. |
| Skin | The image `vrbody_skin`, on the material. |

One unit is one Quake unit (a metre is 26.25 units). The feet are at z = 0, the body faces +X, and +Y is its left.

### Edit

- **Shape:** move, scale and sculpt vertices, and add or remove geometry. New vertices get their weights from their
  neighbours. Check them in Weight Paint mode.
- **Weights:** paint them, as for any skinned mesh. A vertex takes 4 weights at most (the export keeps the 4 largest).
  Weights are normalized.
- **Proportions with the bones:** in Pose Mode, scale or turn bones (a thicker upper arm: scale `upperarm_l`), then
  click **Apply Pose to Mesh**. The mesh takes the pose's shape and the bones go back to rest.
- **UVs:** you can edit them. The skins are 256 × 256; keep that size.

### Don't

- **Don't move, rename or delete bones.** The game poses the body from your tracked head and hands with its own
  skeleton, and uses the file's joints only as the rest frame of each bone. A moved joint would shift the mesh round
  it. The export refuses moved or renamed bones and names them.
- **Don't expect the limbs to get longer.** The arms and legs are as long as you are (your calibration). The mesh
  between two joints stretches to fit.
- **Don't expect the holsters, the belt clip or the gadget's straps to follow the torso, the belt or the bracers.** The
  game places them from the body's measurements in code (`vr_body.cpp`'s torso shape, the flashlight's belt clip, the
  straps' bracer rings), not from the mesh. Reshape the chest or the belt a lot, and the holsters stay where they were.
  Change those numbers in the code if you do.
- **Keep one mesh object** and apply other modifiers first.

### The three builds

The builds (Body > Build in the game, `vr_body_build`) are three meshes with the same vertices, triangles and UVs,
more or less muscular. To edit all three:

- **Edit one, and tick The Other Builds Too on export.** Each vertex you moved is moved by as much in the other two,
  and weights you changed are copied. This needs the same vertices, triangles and UVs: after adding, deleting or
  re-UVing geometry, the other builds are left alone, and the export says so.
- **Or edit each build on its own:** import it, edit it and export it.

### The skins

The 16 skins are one per armour and damage state: skin `armour × 4 + damage`. Armour 0 is none, then green, yellow and
red; damage 0 is clean, and 1..3 add scratches and blood. The three builds share them.

- **Paint the plain skin (00).** The export writes it and carries your changes into the other 15: each keeps its
  armour plates and wounds where they are, and takes your paint everywhere else. The option is **Into the Other
  Skins**.
- **To change an armour or a wound itself,** import with that skin chosen (for example `vrbody_06_00: yellow armour,
  wounded`) and paint it. The export writes that skin alone.
- **Quake's palette:** the body's skins are painted in Quake's palette colours, like the rest of the game. The export
  puts every changed texel on the nearest palette colour (texels you didn't change keep their exact colour), and never
  on a glowing one. Untick **Quake's Palette** to keep full colour: the game draws a `.tga` in full colour, but it
  won't look like Quake.

### Export

Click **Export Body**, or **File > Export > Quake VR Body**. The path defaults to the file you imported. It writes:

- the `.md5mesh` and the `.md5anim`;
- with **Skin** ticked, the skin (and, from the plain skin, the other 15);
- with **The Other Builds Too**, the other two builds' `.md5mesh` and `.md5anim`.

The export keeps the joints exactly, and every vertex you didn't change with its own numbers: **an unedited body comes
back byte for byte.** If a vertex has no weight, the export stops and selects it (**Select Unweighted Vertices** finds
them again).

## 4. See it in the game

Open the console and type:

```
vr_model_reload
```

It reads the weapons, the body, the gadget, the flashlight, the pauldrons, the holster, the shell, the ammo button
and the unrigged hand again from their files, and prints what it read. `vr_model_reload v_shot` (or `vrbody`,
`vrgadget`, `vrflashlight`, any model's name, or several names) reads only those. There's no restart and no map reload: a
map change doesn't read a model again.

For the body, it also checks the skeleton. If the file isn't usable as the body (a bone missing), it says so, and the
game draws the athletic body instead (or no body, if that's the one).

Useful checks after a weapon edit:

- `vr_hotspots_check` compares every slot's hotspots, muzzle and hand with the shipped placement;
- `vr_dumpview` prints each hand's placement while holding the weapon;
- `vr_anchor_info progs/v_shot.mdl <index>` says where an anchor is;
- `vr_anchor_nearest progs/v_shot.mdl <x> <y> <z>` finds the anchor index nearest a point.

## 5. Skins in another editor

Click **Save Skin PNG** in the sidebar (with the model selected) and pick a file, for example next to your .blend.
Paint the PNG and save it, then click **Reload Skin PNG**. Then export: the PNG becomes the skin, on Quake's palette.

## 6. The generators

The generators in `Misc/quakevr` make the shipped models:

| Generator | Writes |
|---|---|
| `make_vrbody.py` | the three builds and the 16 skins |
| `polish_weapons.py` (from `src_models/r21`), and the earlier `improve_weapons*.py`, `make_swords.py`, `recolor_shotgun_sight.py` | the weapons |
| `make_gadget.py` | `vrgadget.mdl`, `vrgadget_strap.mdl` |
| `make_hand_rig.py` | the hand |
| `make_flashlight.py` | `vrflashlight.mdl` and its sounds |
| `make_holster.py`, `make_pauldron.py`, `make_shell.py`, `make_pouch.py` | `legholster.mdl`; `vrpauldron.mdl`, `vrpauldron_arm.mdl`; `vr_shell.mdl`; `vrpouch.mdl` |
| `make_bloody_hands.py` | the blood skins (1-3) of `hand_base.mdl` and `finger_*.mdl` |
| `make_spawn_buttons.py` | `maps/vr_spawnpanel.bsp`, `maps/vr_spawnbutton.bsp` |
| `make_detail.py`, `make_grades.py` | the detail textures (`textures/vr/detail_*.png`), the colour grades (`gfx/vr/grade_*.png`) |

**They don't overwrite your edits.** `Misc/quakevr/generated.json` records what each generator last wrote. A
generator whose output file has changed since then (you exported it from Blender) stops before writing anything and
names the file:

```
make_vrbody.py: these files were edited since a generator wrote them (in Blender?), and this would overwrite them:
  quakevr/progs/vrbody.md5mesh (last written by make_vrbody.py)
Nothing was written. Run it again with --keep-edited to write the rest and leave them, or with --force to overwrite
them (git has the last committed ones).
```

- `--keep-edited` writes everything else and leaves your edited files as they are.
- `--force` overwrites them. Your edits are lost from the files, but not from your .blend: export again.

`make_bloody_hands.py` only rewrites the blood skins: it stops only if you painted those (skins 1-3). Reshape the
hand or repaint its skin 0, then run it: it paints the blood over your skin.

A file checked out from git that differs from what the generator last wrote counts as edited too. `--force`, or the
generator's own run, records it again.

**Keep your .blend.** It is the source of your edits. If a generator's own model changes (a new round), import the new
file and redo your edits, or keep yours and pass `--keep-edited`.

## 7. When the export says no

| Message | What to do |
|---|---|
| `N vertices (selected) are outside the model's box in frames ...` | Pull them back inside, or tick Grow the Box (section 2, [The box](#the-box)). |
| `N anchors would name other vertices ...` | You deleted or rebuilt old triangles. Undo it, or tick Remap Anchors and set the indices it prints. |
| `the model has N frames and the object M shape keys` | Put the shape keys back (import again, or undo). |
| `the skin image ... is WxH; the model's skin is ...` | Scale the image back to the skin's size. |
| `the flat area at the skin's top-left corner grew or changed colour` (a warning) | Repaint texel (0, 0) and the area round it as they were. |
| `bones moved: ...` (body) | Put the bones back (undo, or import again). Reshape with vertices, or with Apply Pose to Mesh. |
| `the bones must be the body's 29, unrenamed` | Rename them back. |
| `N vertices are weighted to no bone (selected now...)` | Give them weights. |
| `the other builds were left as they are` (a warning) | You changed the topology or the UVs: edit the other builds on their own. |

## 8. Normal maps

Every model here has a baked normal map beside it in `quakevr/progs`: the relief the game lights, which the mesh is
too coarse to carry. `Misc/quakevr/bake_normals.py` bakes them from the model files (ROUND21.md, "Baked normal maps").

| Model | Its map |
|---|---|
| A weapon, the gadget, the flashlight... | `<model>.mdl_0_norm.png` (all its skins) |
| The body, all three builds | `vrbody_00_00_norm.png` (the clothes: skins 00-03), `vrbody_04_00_norm.png` (the armoured torso: 04-15) |

**What they carry:** the edges rounded, faceted tubes made round, the seams, rivets and stitches painted into the skins
raised or grooved, wood grained; on the generated models what their generators paint on each material (the
flashlight's knurling, fins and ribs, the strap's webbing and stitching, the gadget's parting line); on the body its
clothes (quilting, belt, laces, straps, buckles, plates, folds) and its muscles.

**You see them in Blender:** Import shows the map on the model (a Normal Map node into the material's Normal). Blender
draws it in its own tangent frame, a little different from the game's, so judge the look in the game.

**After you edit a model, it's one step:** Export bakes its map again (Bake Normal Map, ticked in the export's
options), from the model you just wrote: the bevels and seams follow your new shape. Or press **Bake Normal Map** in
the Quake VR panel (it bakes from the model file as last exported). Then `vr_model_reload` in the game (the maps load
with the model).

**Your own detail:**

- **Paint over a map:** open the PNG in Blender's image editor (or any editor), paint, save. The bake then leaves it
  alone: Export and Bake Normal Map say it was edited and don't overwrite it (tick **Overwrite Edited Map** to bake it
  again), and so does `bake_normals.py` (`--force` overwrites it, `--keep-edited` bakes the others).
- **Bake from a high poly:** model or sculpt it over the imported model (in the same place), select it, then the model
  (active), and press Bake Normal Map. Cycles bakes the high poly's shape; the model's own relief (seams, stitches...)
  is laid on it unless you untick **Add Details**. The map is written in the game's own tangent frame (not Blender's,
  which differs by a few degrees on a hard-edged low poly). A map baked from a high poly counts as edited: the script
  leaves it alone.
- **Paint it yourself from scratch:** a tangent-space map, green up the image (OpenGL / Blender), linear (not sRGB),
  PNG or TGA, the skin's aspect ratio at 2-4 times its size. Name it as in the table.

**Where the skin's texels are shared** (every face of a generated model's material maps to that material's tile; a
weapon's left and right often share texels; the body's arms and legs do), the map carries only what suits every face
drawing those texels: a tile's relief shows on each face made from it.
