# Editing the body, the weapons and the wrist gadget in Blender

You can open these models in Blender, change their shape and their skins, save them, and see them in the game
without a restart:

| Model | Files in `quakevr/progs` |
|---|---|
| The body (torso, head, arms, legs), in three builds | `vrbody_lean`, `vrbody` (athletic), `vrbody_brawny`: each a `.md5mesh` and a `.md5anim`; 16 skins shared by the three, `vrbody_00_00.tga` .. `vrbody_15_00.tga` |
| The weapons | `v_*.mdl` (`v_shot.mdl`, `v_nail2.mdl`, `v_axe.mdl`...) |
| The wrist gadget | `vrgadget.mdl` (the casing) and `vrgadget_strap.mdl` (a strap) |

The hand has its own add-on and guide: [HANDS_IN_BLENDER.md](HANDS_IN_BLENDER.md).

The rules, in short:

- **The body:** edit the mesh, its weights, its UVs and its skins. **Don't move, rename or delete bones:** the game
  places them itself, from your tracked head and hands.
- **The weapons and the gadget:** edit anything. The export keeps what the game hangs off the model: its anchors
  (where your hand, the muzzle, the ammo screen sit), its frames and its size grid. It tells you if an edit moves an
  anchor, and refuses an edit that would silently break one.

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

It reads the weapons, the body and the gadget again from their files, and prints what it read. `vr_model_reload
v_shot` (or `vrbody`, `vrgadget`, or several names) reads only those. There's no restart and no map reload: a
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
