# Editing the body, the weapons, the gadget, the flashlight and the other models in Blender

You can open these models in Blender, change their shape and their skins, save them, and see them in the game
without a restart:

| Model | Files in `quakevr/progs` |
|---|---|
| The body (torso, head, arms, legs), in three builds (and the feminine one) | `vrbody_lean`, `vrbody` (athletic), `vrbody_brawny`, `vrbody_fem`: each a `.md5mesh` and a `.md5anim`; 16 skins shared by them, `vrbody_00_00.tga` .. `vrbody_15_00.tga`. Source: `Misc/quakevr/src_models/blend/vrbody/vrbody.blend` |
| The jointed hand | `hand_rig.md5mesh`, `.md5anim`, its 4 skins `hand_rig_0N_00.lmp`, and the feminine hand `hand_rig_f` made from it. Source: `Misc/quakevr/src_models/blend/hand_rig/hand_rig.blend` ([HANDS_IN_BLENDER.md](HANDS_IN_BLENDER.md)) |
| The weapons | `v_*.mdl` (`v_shot.mdl`, `v_nail2.mdl`, `v_axe.mdl`...) |
| The wrist gadget | `vrgadget.mdl` (the casing) and `vrgadget_strap.mdl` (a strap) |
| The flashlight, the holster, the pauldrons, the spent shell, the ammo button... | every other `.mdl`: see [the table](#the-flashlight-and-the-other-models) |

The hand's guide: [HANDS_IN_BLENDER.md](HANDS_IN_BLENDER.md) (the same add-on). The workflow in short: [BLENDER_WORKFLOW.md](BLENDER_WORKFLOW.md).

The rules, in short:

- **The body:** edit the mesh, its weights, its UVs and its skins. **Don't move, rename or delete bones:** the game
  places them itself, from your tracked head and hands.
- **The weapons and the gadget:** edit anything. The export keeps what the game hangs off the model: its anchors
  (where your hand, the muzzle, the ammo screen sit: points on its surface), its frames and its size grid. It tells
  you if an edit moves an anchor.
- **The flashlight:** edit anything. The game reads its lens, its tail and its outline from the model as it loads it:
  the beam and the light follow a lens you move or enlarge.

## 1. Install the add-on (once)

The add-on is **Quake VR**, `Misc/quakevr/blender/addons/quakevr_models` (Blender 5.2): one add-on for every model,
the hand's tools included (until 2026-11 the hand had its own). Install it and use it as
[BLENDER_WORKFLOW.md](BLENDER_WORKFLOW.md) says: a script directory on `Misc/quakevr/blender`, restart, tick
**Quake VR**. With the script directory, Blender loads it from the repository itself (after a pull: restart Blender, or
**F3 > Reload Scripts**). (Your Blender 5.2 is set up this way: the script directory
`C:\OHWorkspace\quakevr-iw\Misc\quakevr\blender\`; the old hand entry goes away by itself.)

Or install it as a zip: zip the `quakevr_models` folder (the zip must contain the folder), then **Preferences >
Add-ons**, the menu at the top right > **Install from Disk...**. Reinstall it when the add-on changes.

The buttons are in the 3D view's sidebar (press N) in the **Quake VR** tab: one panel that says what the open .blend is
and shows its tools, with the imports at its foot. The imports and exports are also in **File > Import** and
**File > Export**.

The panel's top box is **Export on Save** (on by default; also in the add-on's preferences): saving a model's source
.blend, or a .blend holding the jointed hand or the body, exports it to the game ([section 9](#9-a-weapons-source-blend-every-weapon)).
**Game Folder** (in the box and in the preferences): where it writes; empty, the repository's `quakevr/progs` (the
folder you play from). **Show in Game** asks the running game to draw the model in front of you.

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

- **Keep the surface where the anchors are, unless you mean to move them** (see [Anchors](#anchors)). The export
  report names every anchor and where it is in every frame.
- **Stay inside the model's box** (see [The box](#the-box)).
- **Keep one object per model.** Join parts back with Ctrl+J. Apply other modifiers first: only the mesh is exported.

### Don't

- **Don't take away the surface an anchor is on** without picking the anchor again: the anchor binds to what is
  nearest it then (see [Anchors](#anchors)). Rebuilding faces, deleting and adding vertices, renumbering: all fine.
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
  ones (so a diff of the file shows your edit; the anchors don't depend on the numbering);
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

The game places things on a weapon at anchors (vr_weapons.inc's `*_av` settings and your `vr_wofs_*_av`; the
magazine mounts in vr_view.cpp, the shell ejection ports in vr_shells.cpp, the monsters' guns in vr_weaponfx.cpp):

- the hand's anchor (`hand_av`), and the off hand's for two-handed guns (`2h_hand_av`);
- the muzzle (`muzzle_av`): shots, flashes, the laser sight;
- the ammo screen and its button (`wpntxt_av`, `wpnbtn_av`);
- the magazine's mount (its kick moves the magazine and its well) and the shell ejection port;
- on `soldier.mdl` and `enforcer.mdl`, the gun's muzzle and rear (their middles: the shot's flash and direction).

An anchor is a **point on the model's surface**, in its own coordinates as frame 0 has them (the numbers Blender's
N panel shows for a vertex of the basis shape, and `vr_anchor_info` prints): `"x y z"`. The game binds the point to
the nearest triangle of frame 0 and moves it with that triangle in every frame, so the anchor follows the animation
(the kick, the pump, the double barrel opening) whatever the vertices' numbering: **edit freely**, just keep the
surface where the anchors are. Where two parts touch at the point (triangles equally near that move apart in the
animation: a muzzle flash folded onto the barrel's tip), the first in the file's order is taken, which an edit may
change: put such an anchor a little off the seam, on the part it belongs to (`vr_anchor_info` and `vr_anchor_nearest`
say when a point is on one). (Before round 22 an anchor was a vertex index in QuakeSpasm's old strip order; a config's
were replaced by the defaults' points once, vr_wofs_version 41, and a lone number is no longer read.)

Your weapon poses hang off them. The report lists each anchor on the model, bound on the old model and on the new:

```
  ok     slot 1 the hand's (vr_wofs_hand_av_02): "2.0562 0.0247 1.2804" at (2.06, 0.02, 1.28), triangle 102, 0.0000 off the surface
  MOVED  slot 1 the muzzle's (vr_wofs_muzzle_av_02): "33.0657 -0.6615 3.013" at (33.07, -0.66, 3.01), triangle 15, 0.0001 off the surface -> (33.07, -0.66, 3.35): moved 0.333 units (0.85 cm at the default size) at most over the frames
```

- **ok:** the anchor is where it was in every frame (or moved less than 0.01 units: the report says how much).
- **MOVED:** the surface under it moved. The export writes the model, and warns. Whatever hangs off the anchor moves
  with it: check the weapon's poses in the game. "off the new surface" means the surface it was on is gone: pick the
  anchor again (below).

To pick an anchor in the game (the developer console):

- `vr_anchor_nearest progs/v_shot.mdl <x> <y> <z>` prints the anchor of the surface point nearest a point (paste it:
  `vr_wofs_muzzle_av_02 "33.0657 -0.6615 3.013"`; in `Quake/vr/vr_weapons.inc` to ship it);
- `vr_anchor_info progs/v_shot.mdl <anchor>` says where an anchor binds (its triangle, its weights, how far off the
  surface it is, and whether other triangles as near would take it elsewhere in some frame);
- `vr_anchor_where` (Debug > Reports > Weapon Anchors) prints where the held weapons' anchors are this frame.

`Misc/quakevr/anchors.py check <model.mdl> [old.mdl]` checks the anchors on a model outside Blender (each within 0.1
units of the surface in every frame, and of where it was on the old model); `anchors.py point <model.mdl> <vertex>`
prints the anchor at a vertex.

### The box

An `.mdl` stores each vertex as a byte per axis: its place is `origin + byte × scale`. The header's origin and scale
make a box, and every vertex must be inside it, in every frame. The export keeps the box, so an edit that reaches
past it is **refused**: the vertices outside are selected, and the message names the frames.

Tick **Grow the Box** to let the export make the box bigger. It is in two places:

- a weapon's source .blend ([section 9](#9-a-weapons-source-blend-every-weapon)): the panel's top box (3D view
  sidebar, **Quake VR** tab), under Export on Save. It is kept in the .blend (on the model object), so every Ctrl+S
  honours it; `blend_export.py --grow` does the same from a command line;
- an imported model: File > Export > Quake VR Model's dialog (it starts ticked if the model's object has it ticked).

What growing does:

- The grid gets coarser along the axes that need it, just enough for the vertices outside. Every vertex goes onto
  the new grid, moving up to half a step (the report says how much). The anchors move by as little (a source .blend's
  export doesn't count an anchor whose surface moved no more than that as moved).
- It happens once: later exports start from the grown box (a source .blend's from the model file they replace, an
  imported model's from the file it now stands for), so it grows again only if a vertex goes past the grown box, and
  saving an unchanged .blend again writes the same bytes. To go back to the old box, put the old model file back.
- Past the low side, the origin moves too. The weapon's Scale turns about the origin, so the drawn weapon would move
  by `(origin moved) × (1 - Scale)`. The export gives the offsets that keep it in place, for each slot using the
  model:

  ```
    inc vr_wofs_x_37 1.6202        (slot 36 at Scale 0.45; with another Scale: 2.9458 x (1 - Scale))
  ```

  A source .blend's export (Ctrl+S, or blend_export.py into `quakevr/progs`) writes them into weapons.txt's defaults
  itself (`  box:` lines in its report; Ctrl+S shows them in a popup). Your config keeps its own offsets over
  weapons.txt's, so type the `inc` lines into the game's console too (they're saved); a grown model that ships needs
  a `vr_wofs_version` step for other players' saved configs, as any changed default does. From File > Export, only
  the console lines. The gadget has no Scale setting, so its origin can move freely.

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
| `hand_base.mdl`, `finger_*.mdl` the unrigged hand | Not drawn (the six-model hand was removed 2026-11-02); the rigged hand (`hand_rig`, the hand add-on) is built from them | Edit its source .blend (`src_models/blend/hand_base/`: the export makes the blood skins 1-3 from skin 0), then run `make_hand_rig.py`: the report says so |
| `knight.mdl`, `hknight.mdl` | The sword hidden as a knight dies, by its vertex numbers and the model's counts (`vr_monstermods.cpp` `knownSwords`); the dropped sword (`v_ksword.mdl`, `v_hksword.mdl`) is cut from them by `make_swords.py` | Keep the sword's vertices, or update `knownSwords` (the report says); moved the sword: run `make_swords.py` |
| `ogre.mdl` | Its chainsaw hidden as an ogre dies, by its vertices 416..496 and the model's counts (`vr_monstermods.cpp` `knownSwords`); the dropped chainsaw (`v_chainsaw.mdl`) is cut from it by `make_chainsaw.py` | Keep the chainsaw's vertices, or update `knownSwords` (the report says); moved the chainsaw: run `make_chainsaw.py` |
| `v_chainsaw.mdl` the chainsaw | Its cord's T-handle **read from the model** (`vr_chainsaw.cpp`): the vertices that differ between frames 0 and 9 are the handle (their middle in frame 0: where the hand takes it; where they meet in frame 9: the cord's hole); its bar: the vertices past 58% of its length along +x (they may sink into monsters). Round 21's close-up pass (`make_chainsaw.py` `polish`: the starter's housing under the handle, the bar's nuts, edge wear) only appends | Keep frame 9 as frame 0 with the handle collapsed into its hole, +x along the bar, the origin in the rear handle's grip (or retune slot 20's Offset) |
| `soldier.mdl`, `enforcer.mdl` | Their guns hidden as they die, by their vertices (the soldier's 463..548; the enforcer's 22, 23, 100, 400..430, 455..478) and the models' counts (`vr_monstermods.cpp` `knownSwords`; the soldier's death frames by number, 8..28); the dropped guns (`v_gruntgun.mdl`, `v_enfrifle.mdl`) are cut from them by `make_enemyguns.py` | Keep the guns' vertices, or update `knownSwords` (the report says); moved a gun: run `make_enemyguns.py` |
| `v_crowbar.mdl` the crowbar | Slot 23 (`vr_weapons.inc`): the muzzle by its anchor (a point: the hook's back where the bar's line meets it, also the Blade hotspot's axis), the hotspots in model space; its Offset depends on the model's bounds (the weapon scales about their corner: `vr_hotspot_fit` shows where the hand is on it) | Laid where the swords are (along the axe's handle, `make_crowbar.py`); keep the surface at the anchors |
| `v_gruntgun.mdl`, `v_enfrifle.mdl` the enemy guns | Slots 21 and 22 (`vr_weapons.inc`): the muzzle and the counter by their anchors (points on the surface), the foregrip hotspot in model space | +x along the barrel, the origin in the pistol grip's middle (or retune the slot's Offset); keep the surface at the anchors |
| The weapons, the gadget | Section 2 | Section 2 |
| id's other models (monsters, items, gibs...) | Their frames and skins only | Anything |

Not editable here:

- **The hand** (`hand_rig.*`): its own add-on, [HANDS_IN_BLENDER.md](HANDS_IN_BLENDER.md).
- **The firing range's monster buttons** (`maps/vr_spawnpanel.bsp`, `vr_spawnbutton.bsp`): brush models, compiled by
  `make_spawn_buttons.py` from brushes written in it (TrenchBroom's world, not Blender's).
- **`s_bullet.spr`**: a sprite, not a model.
- **`hand.mdl`** is never drawn: it names the fist's weapon slot.

After exporting the flashlight, `vr_model_reload vrflashlight` prints what the game read:

```
vr_model_reload: progs/vrflashlight.mdl: lens at (2.475 0.000 0.301), radius 0.582; tail at x -1.365, switch at
(0.842 0.000 0.400); 14.8 cm long, 4.8 cm thick at most
```

A model whose lens it can't find (nothing fullbright in skin 1) says `(NOT from the model: the default)` there, and
the console warns: the beam then starts where the shipped torch's lens is.

## 3. The body

### Its source .blend

**Open `Misc/quakevr/src_models/blend/vrbody/vrbody.blend`** (or, in the game, Debug > Tests > Edit in Blender on the
body). It is the body's source, as a weapon's .blend is its: edit it and **Ctrl+S**; the save exports every build and
the skins to the game (section 9's export on save), and the game reads them again by itself (`vr_model_watch`).
Saved unedited, it writes the shipped files byte for byte. It holds the athletic build (`vrbody`) with the other
builds as its shape keys (below) and the plain skin, `vrbody_00_00`, packed.

The generator, make_vrbody.py, no longer writes what the .blend owns (`Misc/quakevr/generated.json`: the four builds'
`.md5mesh`, `.md5anim` and `.mdl` stubs, the 16 skins): run, it leaves them as they are and says so. It still makes
the bare-forearm meshes (`vrbody*_bare`, below) and `Quake/vr/vr_bodybare.inc`.

(Made once by `blender -b --factory-startup --python Misc/quakevr/blend_build.py -- vrbody`, which imports the shipped
body with its builds and records the files it owns. Tested by `Misc/quakevr/blend_md5_test.sh <agent>`.)

### Import (another skin, or without the source)

**File > Import > Quake VR Body (.md5mesh)**, and pick `quakevr/progs/vrbody.md5mesh` (athletic), `vrbody_lean` or
`vrbody_brawny`. Before clicking Import, choose the **Skin** to paint in the side panel. The default is the plain skin,
`vrbody_00_00`. **Every Build (Shape Keys)** (on by default) brings the other builds in as shape keys.

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

The builds (Body > Build in the game, `vr_body_build`) are meshes with the same vertices, triangles, UVs and weights,
more or less muscular: `vrbody_lean`, `vrbody` (athletic), `vrbody_brawny` and `vrbody_fem`. In the source .blend
they are **shape keys** of the one mesh: the basis is `vrbody`, and `vrbody_lean`, `vrbody_brawny`, `vrbody_fem` are
the others (Object Data > Shape Keys).

- **Edit the basis** (`vrbody` selected in the shape keys, Edit Mode): Blender moves each vertex by as much in the
  other builds. Weights, UVs and added or deleted geometry are shared by all of them. The export writes the four.
- **Edit one build alone:** select its shape key and edit it in Edit Mode (or sculpt it). Only that build changes.
- **Apply Pose to Mesh** puts the pose into the basis and moves the other builds' vertices by as much.
- Without the source (a body imported with Every Build off): tick **The Other Builds Too** on export to carry each
  moved vertex into the other builds' files by as much, as before.

**Bare forearms** (Body > Bracers off, `vr_body_bracers 0`) are three more files, `vrbody_bare`, `vrbody_lean_bare` and
`vrbody_brawny_bare` (`make_vrbody.py` writes them; it leaves the edited skins alone with `--keep-edited`). They differ
from the builds only in the forearms: no bracer, the skin running on to the wrist and over the base of the hand,
fitted round the jointed hand (`hand_rig.md5mesh`, sliced at its wrist). They use the same skins (the arm's block, its
rows down to the wrist). Edits to the hand's wrist need `make_vrbody.py --keep-edited` run again, so that the arm's
end fits it; it prints the wrist's colour and size the engine uses (`vr_view.cpp`'s `bareTint`, `vr_avatar.cpp`'s
`forearmGirth`): repainting the arm's skin round the wrist or the hand changes the colour, so copy the new numbers.

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

**By itself:** the game watches the files of the models it has loaded (`vr_model_watch 1`, the default; Debug > Tests >
Read Models Again When Saved). Within a second of an export it reads a changed model again, as `vr_model_reload` does,
and prints:

```
vr_model_watch: changed on disk, read again: progs/v_shot.mdl, progs/vr_pump_on_v_shot.mdl, progs/vr_pumpbody_on_v_shot.mdl
```

It watches the `.mdl`, and beside it its normal map, its full-colour skins and a body's `.md5mesh` and `.md5anim`. Nothing
changing, it costs nothing (Windows tells it when a file in the folder is written). `vr_model_watch_status` prints what
it watches. A model it hasn't loaded yet (one not drawn so far) is read when it is first drawn, as always.

**By hand:** open the console and type:

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
- `vr_anchor_info progs/v_shot.mdl <anchor>` says where an anchor binds;
- `vr_anchor_nearest progs/v_shot.mdl <x> <y> <z>` gives the anchor of the surface point nearest a point.

## 5. Texturing

The skin is part of the source: a source .blend reads its skins from the PNGs beside it (`<model>_skin<n>.png`), and
saving the .blend writes a painted skin there and exports, so the game reads the model again (as for a mesh edit). The
tools are in the panel's **Texturing** box (3D view sidebar, **Quake VR** tab). **Skin** picks which of a model's
skins they work on (the armour has three).

### Loop 1: paint in Blender

1. Open the model's source .blend (or `vr_edit_in_blender` in the game) and click **Paint in Blender**: Texture
   Paint on the skin, nearest texels, the viewport showing the texture.
2. Paint. The brush's swatches are Quake's 224 ordinary colours (**Swatches**); **Fullbright** switches to the 32
   glowing ones (sights, screens, lights). A texel glows only when painted exactly in a glowing colour.
3. **Palette Preview** shows the skin as the export will put it on the palette; **UV Wires** draws the UV layout over
   it. Both only change what the viewport shows: the saved file keeps the skin.
4. **Ctrl+S**. The skin's PNG is written, the .blend exports, and the game reads the model again within a second.

### Loop 2: paint in another program

1. Click **Edit Skin Externally** (or, in the game, `vr_edit_skin`: the weapon in your hand, `vr_edit_skin look` for
   what you look at, `vr_edit_skin <model> [skin]`; Debug > Tests > Edit Skin in Paint Program). The skin's PNG opens
   in your paint program, and the UV layout is written beside it as `<skin>_uv.png`, to add as a layer.
2. Paint and save the PNG there (keep its size). Blender reads it within a second, saves the .blend, which exports,
   and the game reads the model again. Keep painting and saving: every save comes through.
3. **Stop Watching** when you're done (opening another .blend stops it too).

The paint program is the add-on's **Skin Editor** preference. If it is empty, the add-on uses the first one it finds
installed: Krita, Aseprite, GIMP or Photoshop; with none, the program Windows opens PNGs with. Quake's palette for
them is in `Misc/quakevr/blender/addons/quakevr_models/palettes`: `quake.gpl` (GIMP, Krita, Aseprite; the glowing
colours are named FULLBRIGHT), `quake_ordinary.gpl` and `quake_fullbright.gpl` (the two halves), `quake.pal` (JASC,
for Aseprite and others) and `quake.hex`. Paint without anti-aliasing or blending if you can: any colour off the palette
is put on the nearest palette colour by the export.

If you paint the skin in Blender and don't save, and the PNG then changes outside, Blender doesn't replace your
strokes. It says so and shows **Load PNG Anyway**, which takes the PNG (Ctrl+Z brings the strokes back). Ctrl+S
keeps Blender's version instead. When Edit Skin Externally starts on a skin painted in Blender and not saved, it writes
Blender's version to the PNG first. The PNG it replaces is copied to `quakevr_skin_backups` in the system's temp folder.

### Helpers

- **Bake Starter Shading**: ambient occlusion, creases and worn outer edges, baked from the mesh with Cycles (a few
  seconds), darken or lighten each texel along its own palette ramp (**Overlay**, the default). **Fresh Base** starts
  from a grey instead. **Selected Faces Only** limits it to the faces selected in Edit Mode. Texels no face uses are
  left alone.
- **Fill Material**: select faces in Edit Mode. Fill Material paints their texels with **metal**, **wood**,
  **leather**, **rust** or **cloth**: a pattern on a palette ramp (**Ramp** picks another row of the palette; **Scale**,
  **Brightness**, **Seed**).
- **Write UV Layout PNG**: the UV layout beside the skin's PNG (lines on transparent), without opening anything.

Both the bake and the fill leave the glowing texels and the flat area at texel (0, 0) alone (the engine floods that
area: see [section 7](#7-when-the-export-says-no)), unless you untick **Keep Glowing Texels** or **Keep Texel (0, 0)'s
Area**. Each one is a single Ctrl+Z step, and nothing reaches the PNG or the game until you save.

(The old **Save Skin PNG** and **Reload Skin PNG** buttons are gone from the panel: Edit Skin Externally does both.
A model imported without a source .blend, such as the jointed hand or the body, gets its skin written beside the saved
.blend as `<blend>_skin<n>.png` the first time.)

## 6. The generators

The generators in `Misc/quakevr` make the shipped models:

| Generator | Writes |
|---|---|
| `make_vrbody.py` | the three builds and the 16 skins |
| `polish_weapons.py` (from `src_models/r21`), and the earlier `improve_weapons*.py`, `make_swords.py` | the weapons until 2026-11: **retired**, every weapon's source is a .blend ([section 9](#every-weapon-2026-11)) |
| `gunlooks.py` (after `polish_weapons.py`: `--capture`, then it, then `bake_normals.py`) | the shotgun's and nailguns' skins and rivets, `g_shot0.mdl`, `g_nail.mdl`: **retired** likewise |
| `make_gadget.py` | `vrgadget.mdl`, `vrgadget_strap.mdl` |
| `make_hand_rig.py` | the hand |
| `make_flashlight.py` | `vrflashlight.mdl`, its full-colour skin (`vrflashlight.mdl_0.png`) and its sounds; then `bake_normals.py vrflashlight.mdl` |
| `make_holster.py`, `make_pouch.py`, and the removed `make_pauldron.py`, `make_shell.py` | `legholster.mdl`; `vrpauldron.mdl`, `vrpauldron_arm.mdl`; `vr_shell.mdl`; `vrpouch.mdl` |
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

### Tweaking the ammo screen housings

These guns have their ammo screen's housing modelled on them, each by its own script in `Misc/quakevr`, all run the
same way on the gun's source .blend (`housing_common.py` holds what they share):

| Gun | Script | Its .blend | Table entry |
|---|---|---|---|
| shotgun | `make_screen_housing_shot.py` | `src_models/blend/v_shot/v_shot.blend` | `"shot"` |
| super shotgun | `make_screen_housing_ssg.py` | `src_models/blend/v_shot2/v_shot2.blend` | `"shot2"` |
| nail pistol | `make_screen_housing_nailpistol.py` | `src_models/blend/v_nailpistol/v_nailpistol.blend` | `"nailpistol"` |
| super nailgun | `make_screen_housing_nail2.py` | `src_models/blend/v_nail2/v_nail2.blend` | `"nail2"` |
| lightning gun | `make_screen_housing_light.py` | `src_models/blend/v_light/v_light.blend` | `"light"` |
| nailgun | `make_screen_housing_nail.py` | `src_models/blend/v_nail/v_nail.blend` | `"nail"` |
| rocket launcher | `make_screen_housing_rocket.py` | `src_models/blend/v_rock2/v_rock2.blend` | `"rock2"` |

From the repository root, in PowerShell (the separator `--` before the script's own options must be quoted, `'--'`:
PowerShell drops a bare one):

```
& "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup Misc\quakevr\src_models\blend\v_shot\v_shot.blend --python Misc\quakevr\make_screen_housing_shot.py
& "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup Misc\quakevr\src_models\blend\v_shot\v_shot.blend --python Misc\quakevr\make_screen_housing_shot.py '--' --dry
```

In Git Bash:

```
"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup Misc/quakevr/src_models/blend/v_shot/v_shot.blend --python Misc/quakevr/make_screen_housing_shot.py
"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup Misc/quakevr/src_models/blend/v_shot/v_shot.blend --python Misc/quakevr/make_screen_housing_shot.py -- --dry
```

The other guns: their .blend and script from the table.

The script builds the housing (or moves the one already there: the same shape keeps its vertices and triangles in the
model file), exports the model as `blend_export.py` does (the nail pistol's with `--grow`), saves the .blend and ends
with `housing_<entry>: ok, ...`, or `housing_<entry>: REFUSED: ...` with nothing written. After `--`: `--dry` builds
and checks it without exporting or saving; `--save other.blend` saves there instead of in place. Run on an unchanged
.blend it writes the same files (git shows no change): a .blend whose content the run didn't change is not saved
again (Blender's saves of the same content differ in their bytes). Every script refuses if the UVs of any of the
gun's own faces change (only the housing's may), before and after the export; the shotgun's script re-skins its end
face's own two triangles on purpose (one flat mapping onto the steel plate: id's skin paints them as two mismatched
islands, which the bezel showed up) and the check covers the rest.

Three ways to change a housing:

1. **The opening** (its size, `opening`; its corners, `radius`; the SSG's glass `offset`): edit the gun's entry in
   `Quake/vr/vr_screenstyle.cpp`, rebuild the engine (the glass is drawn from the same table) and run the script.
2. **The rest of its shape** (the bezel's width, depths, bolts, the textures' places): the constants at the top of the
   script; run it again. The shotgun's depth is `DEPTH_STEPS` in `make_screen_housing_shot.py`: how many grid steps
   (0.22 units) the bezel's front stands in front of the end face's bottom edge (1, the default; 2, twice as far, the
   first housing's). Its bevel and opening go one step back from the front; the bolts' heads stand a step proud, at
   most flush with the front. The glass has to sit between the bezel's front and the leaning end face (its top edge is
   nearest): after a depth change the script refuses and prints the `offset` out value for the "shot" entry that puts
   it there; set it, rebuild and run again (the opening's walls then slope to just inside the glass).
3. **By hand in Blender**: edit the .blend and export it (`blend_export.py`, or Export on Save). Don't run the script
   after that: it would put its own shape back.

The model's vertices are bytes on a grid (on the super shotgun a quarter of a unit along the gun): a small change can
land on the same grid places and change nothing, and the SSG's script snaps its opening to the grid and refuses
unless the table's `opening`, `radius` and `offset` match what the snapped walls make (it prints the values that do).
A change that splits a quad the other way re-makes those triangles (the old file vertices stay, unused). Reload the
map to see the new model in the game.

## 7. When the export says no

| Message | What to do |
|---|---|
| `N vertices (selected) are outside the model's box in frames ...` | Pull them back inside, or tick Grow the Box: in a source .blend the panel's top box, under Export on Save (kept in the .blend; `blend_export.py --grow`), else File > Export's dialog (section 2, [The box](#the-box)). |
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

## 9. A weapon's source .blend (every weapon)

For every weapon the `.blend` is the source and the shipped files are made from it, as a generator made them before
(the list: [Every weapon](#every-weapon-2026-11) below). The shotgun's, as the example, lives in
`Misc/quakevr/src_models/blend/v_shot/`:

| File | What |
|---|---|
| `v_shot.blend` (300 KB, compressed) | The model (as Import gives it: frame 0 the mesh, every other frame a shape key named as QuakeC's `$frame` lines, in their order), its anchors and hotspots as empties, the rivets (vertex group `rivets`), which faces are the auto pump's moving fore-end (face attribute `qvr_part`: 1 the pump, 0 the gun without it) |
| `v_shot_skin0.png` (85 KB) | The skin, in Quake's palette colours. Paint it here or in any editor. |
| `v_shot_relief.png` (2 KB) | The relief the normal map is baked from (grey: 128 flat, one level a height step; gunlooks.py's wood grain, rust pits and panel lines). |

**Edit** it as section 2 says (frames, skin, UVs, the box: the same rules), and also:

- **Anchors** are the empties `av hand_av`, `av muzzle_av`, `av 2h_hand_av`, `av wpnbtn_av`, `av wpntxt_av` (weapons.txt's
  points), **hotspots** `hs 1`, `hs 2` (where the other hand grips). Move one and the export writes its new value into
  `quakevr/data/weapons.txt`. Keep an anchor on the surface (snap to Face). `port shell` is vr_shells.cpp's ejection
  port: moved, the export prints the line to paste there. `sight rear` and `sight front` show the sight line the game
  works out from the painted sights (texels in palette 224-239, 252, 253): they can't be moved; repaint the sights.
- **The pump** is drawn apart from the gun by the engine (vr_autopump.cpp). A face you add to the fore-end: set its
  `qvr_part` to 1 (Mesh > Set Attribute). The export writes `vr_pump_on_v_shot.mdl` and `vr_pumpbody_on_v_shot.mdl`
  from your model, so they follow every edit.
- **New animations:** add an armature and an Armature modifier on the model (weight the vertices to its bones), make an
  action, and give the action a custom property `qvr_frames` with a name (`pumpalt`, at most 12 letters). The export
  bakes it to frames after the model's own, one a scene frame over the action's range (`pumpalt1`, `pumpalt2`...;
  the shotgun's are 9 on). The model's own frames stay shape keys: QuakeC plays them by number. To see new frames in
  the game: the Debug menu's Hold the Weapon Frame (`vr_debug_weapon_frame 11`); QuakeC plays them once its `$frame` lines and
  a `player_*` sequence name them.

**The loop: edit, Ctrl+S, look.** With the game running, open `v_shot.blend` in Blender, edit, and press **Ctrl+S**.
Saving exports it (Export on Save, the panel's top box: on by default): a background Blender runs the export below on
the file just saved, in about 5 seconds, while you go on editing. The status bar says
`Quake VR: v_shot.blend exported (ok, 6 files) to ...\quakevr\progs`; a refusal says `NOT exported, nothing written`
and pops up the reasons (the whole report: `%TEMP%\quakevr_export_on_save.log`). The game reads the new files by
itself within a second ([section 4](#4-see-it-in-the-game)): look at the gun. Save again while an export runs and it
exports again after it.

It writes into the repository's `quakevr/progs` (the .blend's own checkout) unless the add-on's **Game Folder** names
another (then weapons.txt is left alone, as with `--out`). Only a weapon's source .blend exports on save (a model with
its anchors' empties, inside a repository); an imported model's .blend doesn't.

**Export by hand** (headless; the .blend is only read): the same export, from a command line (Git Bash; in PowerShell
put `&` before the quoted path):

```
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b Misc/quakevr/src_models/blend/v_shot/v_shot.blend --python Misc/quakevr/blend_export.py
```

It writes `quakevr/progs/v_shot.mdl`, its two pump parts and their three normal maps, and weapons.txt's changes
(anchors, hotspots, the barrel mask `fx_gfx_mask` kept on its triangles). `-- --out <folder>` writes elsewhere and
leaves weapons.txt alone. Before writing anything it checks, and refuses with a message (`blend_export: REFUSED`):

- the frames: as many shape keys as frames, named and ordered as the model's;
- the box (section 2, [The box](#the-box)): a vertex past it refuses, unless **Grow the Box** is ticked (the panel's
  top box, under Export on Save: kept in the .blend) or `--grow` is given. Growing starts from the grid of the model
  file the export replaces (so a grown box stays grown and doesn't grow again), and when the origin moves, the
  offsets that keep the weapon in place go into weapons.txt (with `--out`: listed only) and a popup shows them with
  the console lines for your config (`  box:` lines in the report);
- the parts (every face in one);
- the anchors: each on the surface (0.1 units at most), and the surface under an unmoved one where it was in every
  frame (`--allow-moved` to write anyway);
- the sight line (worked out as the game does: `vr_sight_lines`): it may turn 0.25 degrees at most (`--allow-sights`
  to write anyway, then line them up in the game with `vr_weaponstudio_align main apply`). Sights painted where there
  were none (the nail pistol's, 2026-11) are not refused: the export reports the new line and warns to run the align.
  On a skin with no glowing texels yet, paint them in the exact Fullbright swatches (the add-on's Quake Fullbright
  palette, 224-239): a colour merely near one stays ordinary (qpal.py), and only the sights may glow on a sighted
  model (vr_sights.cpp `sightedModels`).

An untouched .blend exports the shipped files byte for byte. The game reads them again by itself (or by hand:
`vr_model_reload v_shot vr_pump_on_v_shot vr_pumpbody_on_v_shot`).

The test: `bash Misc/quakevr/blend_autoexport_test.sh <agent> [--idle]` saves a copy headless (a refused save writes
nothing; a good one exports, and the running game reads v_shot.mdl again; `--idle`: a quiet minute has no reload).
`BA_MODEL=<model>` before it tests another source the game loads on e1m1 (`soldier`, `vrgadget`).

`Misc/quakevr/blend_build.py v_shot.mdl` made the .blend from the shipped files once. Don't run it again: it starts
from the files, not from your edits.

**Skins in full colour:** the engine also takes a full-colour skin for a weapon (`progs/v_shot.mdl_0.png`, with
`_glow` or `_luma` for what glows: vr_modelload.cpp), but the sights, the barrel glow, the ammo screen and the
flood-filled corner read the 8-bit skin's palette indices, so the source keeps the palette skin.

### Every weapon (2026-11)

Every weapon model the game loads has a source .blend now, and one workflow: open it, edit, **Ctrl+S** (export on
save), look in the game; or export by hand (`blend_export.py`, above, works on any of them). An untouched export writes
the shipped files byte for byte (checked for all of them: 56 sources, 112 files).

| Source | Writes (into `quakevr/progs`) |
|---|---|
| `src_models/blend/<model>/<model>.blend`, one per model: `v_axe`, `v_chainsaw`, `v_crowbar`, `v_enfrifle`, `v_grpple`, `v_gruntgun`, `v_hammer`, `v_hksword`, `v_ksword`, `v_laserg`, `v_lava`, `v_lava2`, `v_light`, `v_multi`, `v_multi2`, `v_nail`, `v_nail2`, `v_nailpistol`, `v_nailpistol_mag`, `v_light_plasma`, `v_prox`, `v_rock`, `v_rock2`, `v_spike`, `v_toolgun`; the pickups `g_light`, `g_nail`, `g_nail2`, `g_prox`, `g_rock`, `g_rock2`, `g_shot`, `g_shot0`; the magazines `vr_mag_light`, `vr_mag_nail`, `vr_mag_snail`, `vr_mag_npistol`, `vr_mag_on_v_*` (7), the wells `vr_magwell_on_v_*` (6); `vr_nailpistol_preview` | `<model>.mdl`, its normal map when it ships one, its full-colour skins (`v_crowbar.mdl_0.png`: the PNG beside the .blend, copied) |
| `src_models/blend/v_shot/v_shot.blend` | the shotgun, its auto pump's two parts, their normal maps |
| `src_models/blend/v_shot2/v_shot2.blend` | the super shotgun and its two open halves (`vr_ssg_frame_on_v_shot2`, `vr_ssg_barrels_on_v_shot2`: cut from the exported gun by make_ssg_open.py's code, so they follow every edit), their normal maps |
| `models_src/flaregun/flaregun.blend` (hand-made; its `export_mdl.py` does the export) | `v_flaregun.mdl`, `vr_flare.mdl`, `vr_flaregun_preview.mdl` |
| `models_src/revolver/revolver.blend` (hand-made) | `v_revolver.mdl` and its `_crane`, `_cyl`, `_frame`, `_shell` parts, `vr_revolver_preview.mdl` |
| `models_src/widowmaker/widowmaker.blend` (hand-made) | `v_widowmaker.mdl`, `vr_widowmaker_preview.mdl` |

- **The hand-made ones** keep their own layout (objects, a full-colour skin, empties: their README): saving one, or
  `blend_export.py` on it, runs the `export_mdl.py` beside it. `models_src/nailpistol/` is an archive: its .blend no
  longer made the shipped gun (the full slide was built from `v_nailpistol_classic.mdl`, edited since), so the nail
  pistol's files have generic sources made from the shipped ones; its `export_mdl.py` refuses.
- **The normal maps** are kept as shipped while what they are baked from (the model file, the relief) is unchanged, and
  baked again when it changes (the export says which). The shipped maps were baked by earlier code: a fresh bake of an
  unedited model differs in a few bytes for some guns.
- **Anchors already off the surface** in weapons.txt (the nail pistol's muzzle, the revolver's 2h hand) aren't refused
  until moved.
- **No generator overwrites a source's files.** `generated.json` records each file with its `.blend` (`"source"`);
  `genguard.py` makes a generator leave them as they are (even with `--force`) and say so, and stops a generator all of
  whose files are owned: polish_weapons.py, gunlooks.py, make_swords.py, make_enemyguns.py, make_chainsaw.py,
  make_crowbar.py, make_ssg_open.py (by hand),
  improve_weapons*.py; bake_normals.py for
  their maps; the hand-made `build_*.py` refuse without `--from-scratch` (they would overwrite the .blend). Their code
  stays: the export calls make_ssg_open.py's cut and the normal maps' recipes; gunlooks.py's relief is the PNG beside
  `v_shot`, `v_nail`, `v_lava`.
  Generators that no other code imports and whose files all have a .blend were removed (2026-11; git history has them):
  make_mags, make_nailpistol_mags, make_toolgun, make_pauldron, make_shell, make_rounds, make_muzzleflash,
  make_walltorch, make_teeth, make_enemyshove, make_voreshove, recolor_shotgun_sight, fix_shot_stock_corner, taper_hand,
  and the from-scratch build scripts of the nail pistol, revolver and burst rifle. Generators other code imports
  (make_swords, make_chainsaw, make_enemyguns, make_pouch, polish_weapons...) stay as libraries; they refuse to write.
- **A new model** (any `.mdl` the add-on imports): `blend_build.py <model>.mdl` once: it builds the .blend from the
  shipped files and records its files as owned. Then the .blend is the source.
- **The super shotgun's bead** (2026-11): a raised, glowing front bead on the muzzle band, and the rib's back end lowered
  under the ring's middle so the ring is a real aperture: edited in `v_shot2.blend`. Its sight line is 1.47 deg up (was
  0.65); its shots follow it (`shot_pitch 1.47`, `shot_yaw 0.11` in weapons.txt). Replaced since (ROUND24.md, "The super
  shotgun's notch sight"): the ring, the old front patch and the bead are gone; a blocky notch sight on the rib just in
  front of the barrels' back (a square U notch between two square ears, a glowing insert on each ear's back face) and a
  slim ramp post at the muzzle (its insert on its back face), the shotgun's idea. Both are on the barrels part (x past
  make_ssg_open.py's cut), so they swing open with the barrels. The sight line kept the old one's angles within 0.03 deg
  (1.488 up, 0.140 left), 0.17 higher (else the barrels' backs hide the rear inserts); vr_sightalign reads it as a notch.
- **The shotgun's sights** (2026-11): raised metal housings instead of glowing blocks, edited in `v_shot.blend` (ROUND24.md,
  "The shotgun's sight housings"): a rear housing across the receiver (a U notch between two ears), a ramp post on the
  muzzle band; the glowing inserts are painted on the ears' and the post's back faces, their tops where the old posts'
  were (the sight line turned 0.001 deg). The front post can't stand higher: the kick's frame `shot3` lifts it to the
  top of the model's box.
- **Repository cost:** 11 MB of .blend files and PNGs for the 53 generic sources, plus a new copy of a .blend at each
  commit that changes it (git doesn't diff .blend files: commit them after an edit session, not after every export).

### Every other model (2026-11)

Every other `.mdl` the game ships in `quakevr/progs` has a source .blend too, made the same way (`blend_build.py`) and
edited the same way: open `Misc/quakevr/src_models/blend/<model>/<model>.blend`, edit, **Ctrl+S**, look in the game (the
game reads any loaded model again when its file changes). An untouched export writes the shipped files byte for byte
(checked for all 96 sources, 101 files and their normal maps and full-colour skins). What each model's export must keep
(the points the game's code holds, the frames QuakeC numbers) is the table in
[The flashlight and the other models](#the-flashlight-and-the-other-models).

| Group | Sources (`src_models/blend/<model>/`) | Was made by (now retired for these files) |
|---|---|---|
| Our gear | `vrgadget`, `vrgadget_strap`; `vrflashlight`; `vrpouch`, `vrpouch_ammo`; `vrpauldron`, `vrpauldron_arm`; `legholster` | make_gadget.py, make_flashlight.py (still makes its sounds), make_pouch.py, make_ammo_pouch.py, make_pauldron.py, make_holster.py |
| Our props and effects | `vr_crate1`, `vr_crate2`, `vr_plank1`..`4`, `vr_barrel`; `vr_brick1`..`4`, `vr_rock1`..`5`; `vr_shell`, `vr_shell_live`, `vr_shell_pair`; `vr_round_rocket`; `vr_muzzleflash`; `vrtorch`; `vr_tooth`; `gib_brain1`..`3` | make_crates.py, make_debris.py, make_shell.py (removed), make_rounds.py (removed), make_muzzleflash.py (removed), make_walltorch.py (removed), make_teeth.py (removed), make_brains.py |
| | `vr_rock1` also writes `vr_explosion_debris.mdl` (its molten skin made from the rock by make_explosion_debris.py's code) | make_explosion_debris.py |
| The unrigged hand | `hand_base`, `finger_thumb`, `finger_index`, `finger_middle`, `finger_ring`, `finger_pinky`: skin 0 is the .blend's; the blood skins 1-3 are made from it at each export (make_bloody_hands.py's code) | make_bloody_hands.py; taper_hand.py (removed) refuses to write an owned file |
| id's models we ship changed | `soldier`, `enforcer` (their shove frames), `shalrath` (its shove); `grenade`, `mervup`, `proxbomb` (skin 1, unarmed, made from skin 0 at each export: make_grenade_skins.py's code); `knight`, `hknight` | make_enemyshove.py (removed), make_voreshove.py (removed), make_grenade_skins.py |
| | `soldier`, `enforcer`, `knight`, `hknight` also write the armed models without their weapon, `vrgrunt.mdl`, `vrenforcer.mdl`, `vrknight.mdl`, `vrhknight.mdl` (make_vrgrunt.py's code: it drops the weapon's triangles by vertex number, so keep those vertices) | make_vrgrunt.py |
| Shipped as they are (id's, the mission packs', the game folder's since the start) | the monsters `boss`, `demon`, `dog`, `fish`, `ogre`, `player`, `shambler`, `wizard`, `zombie`; the gibs `gib1`..`3`, `zom_gib` and the heads `h_demon`, `h_dog`, `h_guard`, `h_hellkn`, `h_knight`, `h_mega`, `h_ogre`, `h_player`, `h_shal`, `h_shams`, `h_zombie`; the items `armor`, `backpack`, `invisibl`, `invulner`, `quaddama`, `w_g_key`, `w_s_key`, `end1`..`4`; the projectiles and effects `spike`, `s_spike`, `k_spike`, `w_spike`, `lasrspik`, `missile`, `beam`, `flame`, `flame2`, `hook`, `teleport`; `wpnbutton`, `hand` | nothing |

Not sources (made otherwise):

- `vragarmor.mdl`: a placeholder quad carrying the armour's skin atlas, painted by make_agarmor.py; the plates
  themselves are cut by the engine from each wearer's body as it loads (`vr_agarmor.cpp`). Its source stays the
  generator.
- The body's bare-forearm meshes (`vrbody*_bare`: make_vrbody.py, section 3). The body itself and the jointed hand
  have source .blends (`src_models/blend/vrbody/vrbody.blend`, section 3; `src_models/blend/hand_rig/hand_rig.blend`,
  [HANDS_IN_BLENDER.md](HANDS_IN_BLENDER.md)); their 244-byte `.mdl` stubs (`vrbody.mdl`, `hand_rig.mdl`...) only name
  them for the engine.

Notes:

- **Frames without names** (id's player, dog and soldier have unnamed frames; the shambler's are all "From ASE"): the
  shape keys are `frame<n>`, or the name with `.001`, `.002`... (Blender's names are unique); the file keeps its own.
- **Skins made from skin 0** (the hand's blood, the grenades' unarmed skin): the .blend holds them, but the export
  makes them again from skin 0, so they follow its edits; painting them in the .blend does nothing.
- **Derived models** (the armed monsters, the molten debris): edit the model they're made from; the export writes both.
  The code that makes them needs the vertex numbers it names: the export refuses (nothing written) if it fails.
- **Repository cost:** 42 MB more of .blend files and PNGs (the big ones are id's monsters: the hell knight's 2 MB,
  the shambler's, the ogre's 1 MB); the full-colour skins beside them are the shipped files' copies (git stores them
  once).
