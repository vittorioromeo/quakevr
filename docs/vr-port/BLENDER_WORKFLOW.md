# Models in Blender: the workflow

Every model of the game is edited in Blender, from a `.blend` that is its source, with one add-on: **Quake VR**
(`Misc/quakevr/blender/addons/quakevr_models`). The loop is three keys long: open, edit, **Ctrl+S**; the game shows the
change by itself within a second. The details per model are in [MODELS_IN_BLENDER.md](MODELS_IN_BLENDER.md) (weapons,
props, gear, monsters, the body) and [HANDS_IN_BLENDER.md](HANDS_IN_BLENDER.md) (the jointed hand).

## Once: the add-on

**Edit > Preferences > File Paths > Script Directories > Add**: the repository's `Misc/quakevr/blender`; save the
preferences, restart Blender; **Preferences > Add-ons**: tick **Quake VR**. That's all: the hand, the body and every
`.mdl` are in it.

(Until 2026-11 there were two, Quake VR Models and Quake VR Hand. An existing setup keeps working: the models' add-on
is the one add-on now, under the same package name, so it stays ticked with its preferences, and the old hand entry
is taken out of the preferences by itself the first time Blender starts with it; save the preferences, or let
Auto-Save do it, and Blender stops looking for it.)

## The loop

1. **Open the model's .blend.** In the game: point at it or hold it and use **Debug > Tests > Edit in Blender**
   (`vr_edit_in_blender`: the weapon in your main hand, else the model you look at; `vr_edit_in_blender look`, or
   `vr_edit_in_blender <model>`, e.g. `v_shot`, `vr_crate1`, `soldier`, `vrgrunt`). It starts Blender on the right
   file: `Misc/quakevr/src_models/blend/<model>/<model>.blend`, a hand-made gun's `models_src/<gun>/<gun>.blend`, or
   for a part or a derived model the .blend that writes it (generated.json's `source`: `vr_pump_on_v_shot` opens
   `v_shot.blend`, `vrgrunt` opens `soldier.blend`). Blender's path: `vr_blender_path`, empty finds the newest in
   `Program Files\Blender Foundation`. Or just open the .blend in Blender yourself.
2. **Edit.** The sidebar's (N) **Quake VR** tab says what the file is ("Source of v_shot.mdl", "The jointed hand:
   hand_rig", "The body: vrbody") and shows only that kind's tools.
3. **Ctrl+S.** Saving exports it in a background Blender (about 5 s; the panel's top line says whether this file
   exports). The game watches its loaded models' files and reads the new ones (`vr_model_watch`). A failed check
   writes nothing and pops up why.
4. **Look**, or press **Show in Game** in the panel: the game draws the model in front of you, turning slowly, read
   again from its file (`vr_model_show <model>`; Debug > Tests > Hide the Model from Blender takes it away). Handy
   for a prop or a monster that isn't near you.

## The jointed hand and the body

Their sources are `Misc/quakevr/src_models/blend/hand_rig/hand_rig.blend` and `.../vrbody/vrbody.blend`, and
**Edit in Blender** opens them like any other source. Ctrl+S exports them like any other model: the md5 files, the
skins with their blood or armour carried over, the normal maps when the mesh or skins changed; the hand also makes the
feminine hand again from itself, and the body writes all its builds, which are the shape keys of its one mesh (edit
the basis and every build follows). Saved unedited, they write the shipped files byte for byte. The generators
(make_hand_rig.py, make_hand_feminine.py, make_vrbody.py's builds and skins) no longer write these files.
Details: [MODELS_IN_BLENDER.md](MODELS_IN_BLENDER.md) section 3, [HANDS_IN_BLENDER.md](HANDS_IN_BLENDER.md).

## How the two sides talk

- Blender to game: the files (the watch) and, for Show in Game, one line in `quakevr/blender_request.txt`
  (`show <model>`). The game asks that file's time every half second (`vr_blender_link 1`, Debug > Tests) and runs
  only `show <model>` and `reload <model>...`: anything else is printed and ignored. A request left from before the
  game started isn't run.
- Game to Blender: `vr_edit_in_blender` starts `blender.exe <file.blend>` (or `--python-expr` the add-on's import for
  the hand and the body). `vr_edit_in_blender <model> -n` prints what it would run.

Test: `bash Misc/quakevr/blend_addon_test.sh <agent>` (the game's side, the hand's and the body's export on save);
`blend_autoexport_test.sh <agent>` (a weapon; `BA_MODEL=vr_crate1` for a prop); `blend_md5_test.sh <agent>` (the
hand's and the body's source .blends: byte for byte unedited, an edit of each read again by the game).
