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

## The ImGui window (flat screen)

The same studio in a floating window over play, so it can be tweaked without opening and closing the Quake menu:
**`vr_armourstudio_window`** toggles it (`open`, `close`, `test` as arguments; `vr_armourstudio_window_close` too), and the
page's first row, Open the ImGui Window, opens it. It ships no key: `bind F7 vr_armourstudio_window` (any key) gives it one,
and that key closes it again while the window is open (a key bound to `vr_armourstudio_window` still reaches its binding
while the window holds the keys, unless a value box is being typed in). Escape or the window's X closes it too; closing it
leaves the studio running (Stop removes the subject). It can be moved, resized and collapsed; it opens where it was left.

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

Start, End and Rows are the cuirass's and the limbs' plates' (not the helmet's or a sleeve's). The pieces stay on the bones they
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
