# Armour Studio

The armour's plates tweaked by hand, per wearer kind and per piece, laid over what the generator makes
(`Quake/vr/vr_agarmor.cpp` `buildShell`). The tweaks live in `quakevr/data/armour.txt`; with nothing in it the armour is the
generator's exactly (the same shells, byte for byte: every wearer's shell hash checked against `origin/next`'s).

## Where

Debug > Tools > Armour Studio (VR Settings menu: the headset's laser or the flat screen's mouse, the same page).

1. **Start: the Kind Ahead** (`vr_armourstudio [kind]`): the kind put 72 units ahead of you with its fists, held still
   (`vr_agrunt_freeze` on while the studio runs), unhurt, wearing the tier picked. The part picked is outlined in cyan.
2. **Kind**: grunt, enforcer, knight, death knight, or the player (your own body's plates: see them on the menu's body clone or in
   a mirror; no subject, no outline). **Tier**, **Turn** (the subject turned from facing you), **Body Ghost** (how opaque its
   body is drawn), **Wear the Part Alone**.
3. **Part** and **Side**: cuirass, helmet, pauldrons, bracers, thighs, shins, and the four sleeves over the elbows and knees (the
   upper part's and the lower part's). A limb's part is edited on both sides at once (the left's values; the right gets the mirror
   image) or on one side alone (its own values, over the both-sides ones, value by value).
4. The sliders (below), live: each change makes the kind's armour again (a grunt's in about 40 ms).
5. **Undo** (a slider's run of steps on one value is one step), **Reset Part**, **Reset Kind**, **Save** (writes armour.txt),
   **Reload the File**, **Print the Tweaks**, **Close** (the subject removed, the armed humanoids move again).
   Since 2026-11-10 (his note: "After closing the armour studio it seems that armed humanoids do not recover their
   AI/damageability") leaving the menu closes it too, once the menu has been open since it started, and so does closing
   its window (Close With the Menu or Window, `vr_armourstudio_autoclose` 1; 0: it runs until Close, as before). While it
   runs every armed humanoid is held still (`vr_agrunt_freeze` 1): the menu or the window closed used to leave it running,
   and them frozen. Test: `Misc/quakevr/armourstudio_close_test.sh <agent>`.

## The ImGui window (flat screen)

The same studio in a floating window over play, so it can be tweaked without opening and closing the Quake menu:
**`vr_armourstudio_window`** toggles it (`open`, `close`, `test` as arguments; `vr_armourstudio_window_close` too), and the
page's first row, Open the ImGui Window, opens it. It ships no key: `bind F7 vr_armourstudio_window` (any key) gives it one,
and that key closes it again while the window is open (a key bound to `vr_armourstudio_window` still reaches its binding
while the window holds the keys, unless a value box is being typed in). Escape or the window's X closes it too; closing it
closes the studio too (`vr_armourstudio_autoclose` 1; 0: the studio keeps running until Stop). It can be moved, resized and collapsed; it opens where it was left.

- Its rows are the page's: Start / Stop the subject, Undo, Reset Part, Reset Kind, Save, Reload the File, Print the Tweaks at
  the top; Kind, Tier, Turn, Body Ghost, Wear the Part Alone; Part and Side; every value as a slider (Ctrl+click types into
  it) and a typed box with - / + steps (Ctrl+click the steps for ten times as much).
- **The file's values**: for the part picked, the table's values (the file as read, with edits not saved yet: the status line
  says whether it is saved): the both-sides values and, for a limb, each side's own and what each side uses (`own` wins over
  `both`, the right's mirrored; `generator` where nothing is set).
- It drives the same `vr_armourstudio_*` cvars and runs the same commands as the menu page (no second copy of the studio): a
  change in one shows in the other at once.
- Input is the Weapon Studio's: while it is open the mouse and keys go to the window (the cursor shown, the game's mouse look
  and keys held back; the console key still opens the console); closing gives the mouse back.
- Flat screen only: in a headset it does not open (the menu page is the headset's). It does not open while the Rig Studio, the
  wave editor or the Weapon Studio is open, and closes when one of them opens.
- Test: `vr_armourstudio_window test` (Debug > Tests: Armour Studio: ImGui Window Test), with `-flatscreenmode` in a map:
  opens it, clicks Start, the grunt bracer's Forward `+`, Save, Reset Part, Reload the File with a mock mouse, writes a value
  the menu's way and sees it on the window, clicks Stop, presses Escape: `armourwin: 17 passed, 0 failed` (writes
  `quakevr/data/armour_wintest.txt`, never armour.txt). In a headset it checks the window refuses: `armourwin: 1 passed`.

### The handle (gizmo)

While the window is open and the studio runs, the part picked has a handle on it in the view (the Rig Studio's handle,
`vr_gizmo.cpp`, shared): at the part's middle, in the part's own frame (its bone as posed, turned by its rotate). The window's
**Handle** row (or the keys **Q W E R**, `vr_armourstudio_gizmo` 0-3) picks it: Off, **Move** (arrows along the part's forward /
left / up, plane squares, the centre dot in the view's plane: Forward / Left / Up), **Rotate** (rings about its axes: Pitch /
Yaw / Roll), **Scale** (arms with box ends: Size Fwd / Left / Up; the centre dot dragged across: Scale, 150 pixels doubling
it). A drag writes the rows' own cvars, so it is the same edit as a slider's: the menu and the rows follow, Save writes it,
and the whole drag is one Undo step. Ctrl snaps to the rows' steps (0.1 units, 5 degrees, 0.02x). Arrows and rings follow the mouse exactly under perspective (the
mouse's ray met with the arrow's line or the ring's plane). The values show by the
mouse while dragging. `vr_armourstudio_gizmo_size` (pixels, 80) its size. None on a sleeve or junction part, nor in a
headset (the menu page's sliders there: no hand grab of the part). A both-sides limb part's handle is on its left piece.
Test: `vr_armourstudio_gizmo_test` (Debug > Tests), `-flatscreenmode` in a map: each mode's drag by a mock mouse makes the
value it should (2 units along the arrow longest on the screen, 30 degrees round the ring facing the eye most, 1.25x
along an arm, 1.5x by the centre, and a rottweiler piece's move), each with one undo step, the view first turned to the
part (the row's **Look at the Part** button, `vr_armourstudio_gizmo_look`; `vr_armourstudio_gizmo_info` prints where the
handle is): `armourgizmo: 16 passed, 0 failed` (plus the turn step below).

**Turning the subject with the mouse.** In the flat window, a drag with the **right mouse button** on the view turns the subject:
across is its **Turn** (yaw, 0.5 deg a pixel), down is its **Pitch** (the Subject rows follow; session-only, never in armour.txt).
It does not begin over the window, nor while the handle drags; **Put Back** resets it; the handle and the part's frame follow the
turned subject. The window's help line says it (the middle button is free too; the right is the usual drag-to-orbit one). The
test's last step drags across 80 px and down 40 px: -40 deg Turn, +20 deg Pitch.

## Animations

The subject plays its model's animations with its plates on, as in play (Animation on the page, the window's Animation
header): **Play an Animation** (`vr_armourstudio_anim`; off, it stands still), **Animation** (`vr_armourstudio_anim_group`:
the model's frames grouped by name: stand, walk, run, pain, death, attacks...; the generated vr models' unnamed frames take
the names of the Quake model they were made from), **Play / Pause** (`vr_armourstudio_anim_play`), **Speed**
(`vr_armourstudio_anim_speed`, 1 = 10 frames a second, below 0 backwards), **Frame** (`vr_armourstudio_anim_frame`: scrubbing
pauses). The engine sets the subject's frame; the QC's freeze keeps its stand frame otherwise (vr_armedgrunt_pose.qc).
`vr_armourstudio_anim_info` prints the animations and the subject's frame.

## Your own body (the player's kind)

With Kind = Player, the menu page shows the body's mirror image (vr_bodyclone.cpp) beside the panel, and the ImGui window shows
it too, ahead of you and to the left (the window opens on the right): its plates are the ones you edit. While either shows
the player's kind, your body (and so its image) wears the studio's armour whatever you have: the Tier picked (green, yellow,
red; as the kind first shows, the tier is set to the armour you wear, if any), every piece, or the part picked alone with Wear
the Part Alone (`armourstudio::playerArmour`, read by the plates' `view::setupPlates`). Switching `vr_body_build` while it
shows keeps the image and its armour. Test: `vr_armourstudio_clone_test` (Debug > Tests: Armour Studio: Your Clone's Armour
Test), in a fresh game (no armour on you), flat screen with `-flatscreenmode` (the window) or in the headset (the menu page):
each tier, then the build switched 2 -> 1 -> 3 -> 2, then the kind another (nothing on you): `armourclone: 8 passed, 0 failed`.

## The values

| Slider | File key | What it does |
| --- | --- | --- |
| Forward, Left, Up | `offset "x y z"` | the piece moved, units along the model's forward, left and up (its rest pose) |
| Pitch, Yaw, Roll | `rotate "p y r"` | turned, degrees about the model's left, up and forward axes, round the piece's middle |
| Scale | `scale` | its size round its middle (1 as made; 0.2 .. 3) |
| Stand-off | `standoff` | units further off the skin: a plate piece's every row radius; a sleeve pushed out from its bone's line, the helmet from its middle |
| Start Along | `start` | units its first row of plates starts further along it (towards the waist, the hand, the foot; below 0 further up) |
| End Along | `end` | units its last row ends further along it (longer; below 0 shorter) |
| Rows | `rows` | its rows of plates (0 the generator's; the cuirass counts its collar row; a limb's two sides take the larger) |
| Taper at Start | `taper_start` | the plate rows' size round the part's line at its first row (1 as made; 0.2 .. 3), lerped row by row to ... |
| Taper at End | `taper_end` | ... this at its last row: a death knight's thighs wider at the pelvis, narrower at the knee (1.15, 0.9) |
| Leather Sleeve Shown | `leather` | a joint's leather (over an elbow or a knee) drawn, or `"0"` on either of its sleeve parts: that joint's whole leather not made |
| Leather Folds | `folds` | the joint's leather's bellows folds, 0-4 (the upper sleeve part's, else the lower's, else `vr_agarmor_folds`, 4) |
| Fold Depth | `fold_depth` | how far they stand on the inside of a bend, in units (flat on its outside; else `vr_agarmor_fold_depth`, 0.3) |
| Hero Plates | `hero` | `"0"`: the part's hero plates off, every plate its own (1 or unset: the cuirass's breastplate, the pauldrons' top row as two plates, the bracers' elbow cop, the shins' knee cop, each one plate over several columns; `vr_agarmor_hero 0` turns them off everywhere) |

### A sleeve's ends (ROUND26.md, "ogresleeves")

When a leather junction does not connect where you expect, these move its two ends and shape it. They are the sleeve parts'
alone (a humanoid's elbow and knee sleeves, a beast's leather junctions), every one off by default (the leather exactly as
before). "Own end" (`start_`) is the end on the sleeve's own piece, "joined end" (`end_`) the one on the piece it joins: a
beast's junction is its child piece's sleeve (the shin's for the ogre's knee); a humanoid's joint has two sleeves, and each end
takes its own piece's sleeve's `start_` value, else the other sleeve's `end_` value (the thigh's sleeve's start_offset moves the
thigh's end of the knee; its end_offset the shin's end, unless the shin's sleeve sets its own start_offset).

| Slider | File key | What it does |
| --- | --- | --- |
| Own End Fwd / Left / Up | `start_offset "x y z"` | that end's ring moved, units along the model's forward, left and up (rest pose); it still rides its piece |
| Joined End Fwd / Left / Up | `end_offset "x y z"` | the other end's |
| Own End Size, Joined End Size | `start_radius`, `end_radius` | that end's ring's size round its middle (1 as made) |
| Own / Joined End Far Edge | `start_edge`, `end_edge` | `"1"`: a beast's end on its piece's far edge, not the one facing the other piece (a humanoid's end is always its row at the joint) |
| Twist | `twist` | degrees: the joined end's ring turned round the line between the ends (the columns spiral); for a humanoid the joint's (the upper sleeve's, else the lower's), turning the lower end |
| Bulge Fwd / Left / Up | `bulge "x y z"` | the middle pushed this far this way (the model's axes; eased to nothing at the ends; a humanoid's turned with the bones) |
| Segments | `segments` | its rings less one, 1-8 (0: as made, from its folds) |
| Closed All Round | `fill` | `"1"`: a beast's junction closed all round: the columns where a piece has no edge (open strips where plates meet at the flanks) laid round its end from its neighbours' reach. The ogre's knees ship with it on |

How a beast's junction picks its ends (vr_beastarmour.cpp buildJunctions): 12 columns round the line between the two pieces'
middles; in each, the outermost vertex within 30% of the piece's end facing the other, tucked 0.35 in and 0.6 under; only
columns both pieces have are drawn. Where that goes wrong: plates that meet only at the front (few columns: an open strip,
`fill`), ends that overlap along the line (`vr_beastarmour_info` prints "along" below 0: the leather inside the plates; move
the ends), radii far apart (one end inside a plate: `start_radius` / `end_radius`). `vr_beastarmour_info` lists each
junction's ends, length along the line, radii, columns (and each end's edges before `fill`).

The bending leather (the only kind since 2026-11-11; ROUND24.md, "Leather bridge at elbows and knees"): a joint's
leather is one tube made each frame from the upper part's last row to the lower part's first: a sleeve part's tapers size
its half (eased to the rows at its ends), its stand-off and scale apply round the tube, its offset and rotate do not.

Start, End and Rows are the cuirass's and the limbs' plates' (not the helmet's or a sleeve's; the ImGui window hides those rows
there). The tapers are every part's but the helmet's: on a sleeve part they size its leather's rings the same way, in the
limb's order (Taper at Start on its end nearer the body: an upper part's sleeve under its plates, a lower part's at the joint;
Taper at End on the other), the ring under the plates kept under their feet so the seam holds; a knee's or an elbow's two
halves overlap at the joint, so tapering one shows a step there, not a gap. A taper of 1 is not written to the file. Leather is the sleeve parts' alone (pick e.g. Knee Sleeves (Thigh's), Left Only, to hide that one
strap). The pieces stay on the bones they
rode: a moved plate keeps its skin weights, so it follows the body as before. What hit tests use to find a limb's plates from
inside (the grid's tube) follows the stand-off, rows, start and end, not a move or a turn.

## The file

```
kind grunt
{
    bracer
    {
        offset "0 0.5 0"
        standoff "0.2"
    }
    thigh_sleeve_r
    {
        standoff "0.3"
    }
}
```

Kinds: `grunt` (vrgrunt and the plain soldier), `enforcer` (both), `knight`, `death_knight`, `player` (every vrbody build), or a
model's path (`kind "progs/vrbody_bare.mdl"`, over its kind's). Parts: `cuirass`, `helmet`, `pauldron`, `bracer`, `thigh`,
`shin`, `pauldron_sleeve`, `bracer_sleeve`, `thigh_sleeve`, `shin_sleeve`; a limb part with `_l` or `_r` is that side's own.
Value by value the most specific wins: the model's side, the model's both sides (mirrored on the right), the kind's side, the
kind's both sides. Read at start, again when it changes on disk (`vr_armour_watch` 1, looked at once a second) and on
`vr_armour_reload`; `vr_armour_print` prints the table as the file would have it. The file is `quakevr/<vr_armour_file>`
(`data/armour.txt`) in the last base folder that has a quakevr folder.

## Test

`vr_armour_test` (Debug > Tests: Armour Tweaks: Data File Test), in a map: a bracer override moves those plates alone (the
right's by the mirror image) and nothing else, a right thigh sleeve's stand-off pushes it out, rows change the shins alike,
saved and read back to the same shell, the file edited on disk read again by the watcher, kept over a map change, the shipped
file again the generator's shell. Ends on `armourtest: N passed, M failed` (it writes `quakevr/data/armour_test.txt`).
Script: `map vrtesthall;vr_armour_test`, `-QuitOn "armourtest: [0-9]+ passed"`.
