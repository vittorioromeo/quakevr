# Editing the hand in Blender

The jointed hand is one skinned model: `quakevr/progs/hand_rig.md5mesh` (the mesh, its weights and its joints),
`hand_rig.md5anim` (the joints again, for the engine's loader) and four skins, `hand_rig_00_00.lmp` (clean) to
`hand_rig_03_00.lmp` (the bloodiest). The engine reads the rig from these files. You can open the hand in Blender,
change its shape, its proportions or its skin, save it, and see the result in the game straight away.

The one rule: **don't rename or delete bones.** Everything else is allowed: moving vertices, adding or removing
geometry, moving the joints and repainting the skin.

## 1. Install the add-on (once)

The add-on is in the repository, in `Misc/quakevr/blender/addons/quakevr_hand`. It is written for Blender 5.2.

1. In Blender, open **Edit > Preferences > File Paths > Script Directories** and click **Add**.
2. Choose the repository's `Misc/quakevr/blender` folder. Name it `quakevr`.
3. Save the preferences (the menu at the bottom left of Preferences > **Save Preferences**, unless Auto-Save
   Preferences is on), then **close and reopen Blender**: Blender only reads a new script directory at startup.
4. In **Edit > Preferences > Add-ons**, search for **Quake VR Hand** and tick it.

"Add-on not loaded: quakevr_hand, cause: No module named 'quakevr_hand'" means step 3 was skipped or the
preferences weren't saved: the add-on was ticked before Blender had the folder on its path.

Alternatively, install it as a zip (no script directory needed, but reinstall it after the add-on changes): zip the
`Misc/quakevr/blender/addons/quakevr_hand` folder (the zip must contain the `quakevr_hand` folder), then in
**Preferences > Add-ons**, the menu at the top right > **Install from Disk...**, pick the zip, and tick it.

Blender now loads the add-on straight from the repository. When the add-on changes in a pull, restart Blender to pick
it up.

## 2. Import the hand

1. Start from an empty scene: File > New > General, then delete the cube, the camera and the light.
2. Choose **File > Import > Quake VR Hand (.md5mesh)**.
3. Pick `quakevr/progs/hand_rig.md5mesh`.

The add-on's buttons are also in the 3D view's sidebar: press N and open the **Quake VR** tab.

The import gives you:

| Object | What it is |
|---|---|
| `hand_rig` (armature) | The 16 joints you edit: `palm`, and `thumb_1..3`, `index_1..3`, `middle_1..3`, `ring_1..3`, `pinky_1..3`. |
| | Each bone's head is the pivot its segment turns about: `index_1` at the knuckle, `index_2` at the middle joint, `index_3` at the last joint. Each finger is a chain, so moving one joint drags the next bone's end along. |
| | The 17 helper bones are in the hidden bone collection `helpers`: `*_half` at each joint, and `thumb_1_quarter` and `thumb_1_three_quarters` at the ball of the thumb. The rings of vertices at the joints and the ball of the thumb ride them. You never need to move them. On export, each one is put back at its joint's pivot. |
| `hand` (mesh) | The hand. Its 33 vertex groups are the weights, one per bone. |
| | The vertices that the file splits along the skin's seams are joined, so edits don't tear the seams. |
| | The material shows the clean skin as the image `hand_rig_skin`. |

Space: +X points to the fingers, +Y to the palm's side and +Z to the thumb's side. 1 unit is about 1.2 cm.

## 3. Edit

### Shape (vertices)

- Work in Edit Mode or Sculpt Mode on `hand`. You can move, scale and slide vertices, add loop cuts, subdivide,
  extrude and dissolve.
- New vertices get their weights from their neighbours: Blender interpolates them.
- Check any new vertices in Weight Paint mode. Each vertex needs a weight on at least one bone.

### Proportions (joints)

To make a finger longer, shorter, thicker or pointed another way, use the bones:

1. Select `hand_rig` and enter **Pose Mode**.
2. Scale, move or rotate bones. For example, scaling `index_1` by 1.1 makes the whole index finger 10% bigger about
   its knuckle.
3. In the sidebar (Quake VR tab), click **Apply Pose as Rest (Mesh Too)**. The deformed mesh and the moved joints
   become the new rest shape.

You can also move joints directly: select `hand_rig`, enter Edit Mode and move the bone heads. The mesh doesn't
follow when you do this, so move the finger's vertices to match.

A joint is the bone's **head**. Only the heads are exported. Tails and roll are only for display.

### Do

- **Keep each joint inside its knuckle.** The finger bends about it.
- **Keep one mesh object.** If you split a part off to edit it, join it back (Ctrl+J).
- **Apply other modifiers first** (mirror, subdivision...). Only the armature is exported.
- **Keep the skin 512 x 512.** The UVs are laid out on it. You can edit the UVs.

### Don't

- **Don't rename or delete bones or vertex groups.** The export refuses, and so does the engine.
- **Don't move the `palm` bone.** The palm is the hand's frame and doesn't move. To change the palm, move its
  vertices.
- **Don't expect weapons to move with the palm.** Weapons, their grips and cups are placed from fixed points in
  the hand's space, which is where they sit on your controller. If you move the palm in Blender, the weapons stay
  where they were on the controller, and the hand changes shape around them.
- **Don't expect the arm to move with the wrist.** The arm meets the hand at a fixed point, `handrig::data::wrist`.
  If you move the wrist's vertices far from it, the arm won't follow.

### What the game works out from your hand

The game works these out from the file you export (`Quake/vr/vr_handrig.cpp`):

- **The hinges.** Each finger joint turns about its own axis. If you point a finger another way (move the next joint
  sideways or up), its hinge turns with it. The fist's curl angles stay the same.
- **The grasp solver's collision spheres.** These are the shipped hand's spheres, moved and resized to fit your mesh.
  Each finger sphere follows its finger's section (how wide the finger is, where its palm side is, and how long each
  segment is). The palm's spheres follow the skin over them.
- **The grip channel.** This comes from the spheres.

The fingers therefore wrap grips as your mesh is shaped. A bigger hand closes on things sooner, so some grips change,
as they would for a real bigger hand.

## 4. Export

1. Choose **File > Export > Quake VR Hand (.md5mesh)**, or click **Export Hand** in the sidebar.
2. The path defaults to the file you imported. Leave it: `quakevr/progs/hand_rig.md5mesh`.
3. Leave **Skin** ticked to write the skins too.

The export writes `hand_rig.md5mesh` and `hand_rig.md5anim`, and with **Skin** ticked, the four `.lmp` skins.

- If a bone is missing or renamed, or a vertex has no weight, the export stops and names the problem. Vertices
  without a weight are selected: click **Select Unweighted Vertices** to find them again.
- A vertex with more than 4 weights keeps its 4 largest.
- Weights that don't add up to 1 are normalized. The export reports both.
- If nothing was edited, the files come out byte for byte the same, apart from the `commandline` line.

**Keep your .blend.** It is the source of your edits. See section 7.

## 5. See it in the game

Open the console and type:

```
vr_hand_reload
```

The hand is read again, drawn with your edits, and grasps with them. There's no restart or map reload.

On success, it prints a report like this:

```
vr_hand_reload: progs/hand_rig.md5mesh: 455 vertices, 656 triangles; the pivots moved 0.59 units at most, 0 hinges
turned; the grasp's spheres moved 0.74 at most, sized x1.00 .. x1.10 (read in 2.1 ms)
```

If the file can't be used, the game says why and keeps the hand you had:

| Message | What to do |
|---|---|
| `the bones must be the hand's 33, unrenamed. Missing: ... Not the hand's: ...` | Rename the bone back, or import again. |
| `vertex N has no weights: it is weighted to no bone` | Give it a weight. |
| `vertex N's weights add up to X, not 1` | Weights > Normalize All, or export with the add-on. |
| `vertex N has K weights; the hand takes 4 at most` | Weights > Limit Total, 4. |
| `vertex N's weights ... run past the file's` | The weights are missing. Export again. |
| `..., line N: ...` | The file is damaged or cut short. Export again. |
| `progs/hand_rig.md5anim ... is not the mesh's` | Export both files together. |

If the game starts with a file it can't use, it prints the reason and draws the old six-model hand. Fix the file,
then run `vr_hand_reload`.

`vr_hand_rig_info` tells you where the rig in use came from. It also tells you whether that rig is the same as the
shipped one: it is, bit for bit, with the shipped files. `vr_grasp_spheres` lists the spheres.
`vr_debug_hand_bones 1` draws the spheres and the joints on the hand.

## 6. The skin

There are two ways to paint the skin:

- **In Blender:** use Texture Paint mode on `hand`. It paints the image `hand_rig_skin`.
- **In another editor:**
  1. Click **Save Skin PNG** in the sidebar and pick a file, for example next to your .blend.
  2. Paint the PNG and save it.
  3. Click **Reload Skin PNG**.

Then export, with **Skin** ticked. The image becomes `hand_rig_00_00.lmp` in Quake's palette:

- A texel you didn't change keeps its palette index.
- Any other colour becomes the nearest palette colour. Quake's glowing colours (224-255) are never used.
- The damage skins `hand_rig_01..03_00.lmp` get your changes too, under their blood. Their blood stays painted where
  it was on the texture, so if you move the UVs a lot, the blood stays with the texture and not the hand.

Don't put a `hand_rig_00_00.png` (or .tga) in `progs`: the engine would draw it instead of the `.lmp`. The export
warns you if it finds one.

## 7. The generator

`Misc/quakevr/make_hand_rig.py` still makes the shipped hand. It writes the MD5 files through the add-on's own
writer, and writes `Quake/vr/vr_handrig_data.inc`, the tables compiled into the engine:

- The engine uses those tables until it reads the file.
- The game measures your edits against them.

**The generator doesn't overwrite your edited hand.** It stops and names the edited files. `--keep-edited` writes the
rest (the tables) and leaves your hand as it is; `--force` overwrites it with the generator's hand (see
[MODELS_IN_BLENDER.md](MODELS_IN_BLENDER.md#6-the-generators)). Keep your .blend: you can export from it again at any
time.

If you change the generator itself, it gives you a new shipped hand. Rebuild the engine, because the tables change.
Your .blend then still holds the old hand: import the new one to edit it.

## 8. The normal map

`quakevr/progs/hand_rig_00_00_norm.png` is the hand's baked normal map (all four skins): the knuckles, the tendons and
a few veins on the back of the hand, the nails with their folds, the pads of the palm and the fingers, and the creases
and wrinkles painted into the skin, raised or grooved where they are painted. It is baked from the hand's own mesh:
the knuckles sit over your joints (the rings of vertices at them), the nails and pads follow each finger's bones, so
an edit of the shape or the proportions carries them along. `Misc/quakevr/bake_normals.py hand` bakes it
(ROUND21.md, "Baked normal maps").

- **Import** shows it on the hand (a Normal Map node into the material's Normal; Blender's tangent frame is a little
  different from the game's: judge it in the game).
- **After an edit, it's one step:** Export bakes it again from the hand you just wrote (Bake Normal Map, ticked in the
  export's options), or press **Bake Normal Map** in the Quake VR Hand panel (from the hand as last exported). Then
  `vr_hand_reload` in the game. Baking needs the Quake VR Models add-on enabled too (the baker lives there).
- **Repaint the skin's creases** and bake again: the grooves follow the dark lines you paint (a line needs to be clearly
  darker than the skin around it and a few texels long).
- **Paint the map yourself**, or **bake it from a high poly** (select the high poly, then the hand, and Bake Normal
  Map): see [MODELS_IN_BLENDER.md](MODELS_IN_BLENDER.md#8-normal-maps). The bake doesn't overwrite a map painted or
  baked from a high poly unless you tick Overwrite Edited Map.
- It never changes your mesh or your skin.
