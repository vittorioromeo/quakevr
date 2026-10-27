# The Rig Studio (`vr_rigstudio`)

A flat-screen editor, in the game, for the rigged humanoids' skeletons, skin weights, poses and animations: explore
and change the armed grunt's (or the enforcer's, or any rigged monster's) bones, which vertices each bone carries, pose
it, fix its animations with keyed override tracks, and author new keyed sequences that the AI can play. Built with
Dear ImGui and driven with Blender's mouse.

Code: `Quake/vr/vr_rigstudio.cpp` (the mode, the scene, the edits, undo, saving, the commands),
`vr_rigstudio_view.cpp` (the viewport: camera, drawing, picks, handles, the weight brush), `vr_rigstudio_ui.cpp` (the
windows), `vr_rigdata.cpp` (the data and its files), `vr_rigseq.cpp` (the game's side: QC's sequences, the drawing
layer), `vr_imgui.cpp` (ImGui in the game); `Quake/thirdparty/imgui` (Dear ImGui v1.92.8, MIT); QC
`vr_rigseq.qc`, the taunt in `vr_armedgrunt.qc`, the test `vr_rigseq_test.qc`.

## Opening it

    ironwail.exe -flatscreenmode        then, in the console: vr_rigstudio

or **Debug > Tools > Rig Studio (Flat Screen)**. It is flat-screen only: with a headset session up it refuses
("the Rig Studio is flat-screen only"). It loads `vr_rigstudio_map` (vrtesthall), makes you a god-mode, notarget
onlooker and stands a frozen armed grunt with a shotgun in the middle of the free floor ahead. `vr_rigstudio here`
keeps the map you are in. `vr_rigstudio <spawn class> [weapon] [weapon2]` picks the subject (the armed list's codes:
`vr_toolgun_armed_list`; 4 the shotgun, the default), as does the **Subject** menu (every armed humanoid kind, its right
and left hand's weapon, dual included, and a few unarmed rigged monsters). There is only ever one subject: a change (its
kind, its weapon, its left weapon, a rigged monster), Respawn, opening the studio again and closing it remove the old one
with everything it owns (its held weapons and their records, a worn tank: QC `VR_RigStudio_Remove`); a map loaded while
open stages it again in the new map. Your edits are the model's, not the entity's: they stay. Subject > Count Entities
(`vr_rigstudio_count`) prints the entities in use, the monsters, the held weapons, their records and the worn tanks, and
what the subject owns. `vr_rigstudio_close` (or File > Close Studio) gives the game back.

## Where the subject aims: the aim target

An armed subject aims at the **aim target** (an orange cross in the view) as the game aims at an enemy standing there
(QC `VR_AGK_Hold`, the Pose Studio's Aim stance: its target at that point, its gun's aim the way from its gun to it):
in every animation and in Game display. Before it, the frozen subject had no enemy and carried its weapon along its
animation's rifle line: in the shoot animation, the gun pointed at the floor (48 degrees off level). Its default is
straight ahead of the subject, 200 units away, at its chest's height (its origin + 12: level). Click it in the view to
select it and drag the move handle's arrows, planes or centre (Ctrl: whole units); undoable; not saved (the studio's
session state), kept across subject changes. **Tools > Aim Target**: Aim (off: it carries its weapon, as before),
Show, At Camera (it follows the studio's camera: the subject aims at you), its place (world), Reset (ahead, level),
Select. The engine writes the subject's `.vr_rs_aim` and `.vr_rs_aimon` (QC `vr_rigseq.qc`); every other entity's are
0, so the game's grunts aim as before. Its body keeps facing the stage: a target more than 30 degrees to a side is
aimed at as far as its arms turn (`VR_AGP_ArmsGun`'s limit; in the game its body would turn).

### Aim lines and Align to Target (`vr_rigstudio_aimline.cpp`)

**Tools > Display > Aim Lines** (on; drawn while the aim target is on): for each weapon the subject holds (both of two
guns), read from the weapon **as drawn this frame** (the renderer's own model matrix, kept by `VR_AliasPostTransform`
through `rigstudio::drawnMatrix`: every layer, lerp, the Pose Keys tool's pose, a mirrored left gun and either rig
included): its **barrel line** (cyan: the drawn model's forward from its muzzle anchor, thin along the barrel itself,
thick on to the target's distance, a ring labelled "R barrel" / "L barrel" at its end), the **shot line** where it is
another (dashed magenta to a magenta diamond labelled "shot": QC `VR_AG_SetHand`'s: with `vr_agrunt_aim_converge` 1,
the default, from the muzzle to the target, so the diamond sits on the target; else the drawn gun's line with
`vr_agrunt_shot_drawn`, or the hidden gun's), and a thin orange line from the muzzle to the target. The aim target's own
orange cross and "aim target" label are drawn over all of them (it alone is picked and dragged). At the view's foot:
"aim error: yaw x, pitch y (total z)" per weapon (degrees; yaw + left, pitch + up), the shot line's source, the key's
note ("this key also aims: ..."), Align's status. (Before 2026-10-27's fix the barrel was the butt-to-muzzle line of
the note's placed frame: a stock's heel sits below the barrel, so it ran 5-10 degrees off the drawn gun; the aim test
now reads this line too.)

**Align to Target** (Tools > Aim Target, and the Pose Studio Data window under the key when it is the subject's;
`vr_rigstudio_align [move 0|1]`): the **current pose key** (the subject's Aim key for its kind and gun class; with the
Pose Keys tool, the window's key when it is the subject's) has its weapon turn changed so the drawn barrel points
exactly at the target: Right Turn, and Left Turn for the left of two guns. It solves through the game's chain: the key
is previewed (`agpose::setPreview`: QC reads it the next frame), the arms solve, the drawn barrel is read once it has
settled, and the barrel's rotation onto the target is laid on the key's turn in its body's space (QC turns the
procedural gun by the key in the body's yaw frame: key' = Y^-1 C Y key); a step that made it worse goes back to the best
key and halves the next steps. Under 0.05 degrees the key is kept (`agpose::set`, its turns rounded to 0.1 as every
key's): 2-5 steps, 0.02-0.07 degrees drawn. One undo step (Ctrl+Z puts the key back exactly). The place (Right/Left
Place) is left alone unless **Also Move**: then, once aligned, place steps try to put the muzzle back where it was (the
gun turned about its muzzle), each kept only if the muzzle came nearer after re-aiming. On a shouldered gun they do not
help (the grunt's shotgun: 3.5 units of place turn it 12.8 degrees about its stock and move its muzzle 0.45), so the
key stays the turn-only one; the muzzle is never left further than the turn alone leaves it.

The key is shared: every weapon of the kind's pose category (Weapon Holds' Pose Category) takes it; the note lists them.
A weapon's own grip is no tool for its aim (the weapon is drawn on the pose's line; its grip only sets the hand on it),
so there is no per-weapon align: to aim one weapon apart, give it its own Pose Category.

## Pose Keys: the Pose Studio's data by handles

The **Pose Keys** tool (O, or Tools) drags the key picked in the Pose Studio Data window on the subject, by handles, with
the VR Pose Studio's own mapping from a part's place to the key's values (`posestudio::deskBegin`, `deskDrag`,
`deskEnd`): a dot on each part the key moves (the right weapon or fist, the left weapon or free hand, each elbow (its
pole drawn as a line from it), the torso (at its waist); in Pickup the pelvis, both feet and both knees). Click a dot:
its handle, in the key's space's axes (the subject's facing: x forward, y left, z up): arrows, planes and centre to move
it, rings (outside the arrows) to turn it (the weapons, the torso, the pelvis). The subject is held in that key's pose (a
strike at the phase picked; the game's frame drawn, not the timeline's, and none of the studio's own pose), the game's
IK result drawn each frame, the arms staying on their weapons; the window's numbers follow the drag. Ctrl snaps (1 unit,
15 degrees), Esc cancels, Ctrl+Z undoes; Save and Print as the window's. With **Follow the Subject** on (the default)
the window's kind and class are the subject's and the pose, as the tool is picked, the one it is in (Aim with the aim
target on, else Carry).

**The off hand with a single gun** (any gun category: Rifle, Pistol, Launcher, Sprayer, Laser Cannon, Widowmaker;
ROUND22.md, "Off hand: grip offset and Free"):
- On the gun (its second grip held, or cupping the right hand), a **left hand: grip offset** dot: its arrows and rings are
  the gun's own axes (x forward along the barrel, y left, z up), and drag the key's **Left Grip Place / Left Grip Turn**
  (`agpose::GripPos`, `GripTurn`): the off hand's grip target on the gun moved and turned about its grip point, in the
  gun's frame, so it follows the gun. Zero by default (no change). A second grip moved past the arm's reach still slides
  back along the gun (`vr_agarms_second`).
- Off it, hanging free, a **left hand (free)** dot: moved and turned (model space), it drags the key's Left Place / Left
  Turn, laid on the hanging hand (zero: relaxed at its side).
- The Pose Studio Data window's **Off Hand** combo beside the key: **Auto (the Weapon Holds)** (the default: its second
  grip, a cup, or free for a one-handed hold), **On the Gun** (on it even when the hold is one-handed; held as a
  two-handed hold is; a pistol carried low is still never cupped), **Free** (off the gun). Which pose's setting counts:
  Pickup's while taking a weapon up (if set), else Aim's while aimed, else Run's while running (if set), else Carry's; so
  Run Free with Aim Auto: free running, on the gun aimed. Between the hand eases round the shoulder over a quarter second
  (`vr_agarms`' second-grip ease). `vr_rigstudio_posekey offhand <0|1|2>` sets it from the console.

Undo, Save, Print / `vr_agrunt_pose_dump` (a row carries 30 numbers when any off-hand part is set, else the 21 of
before), Copy to the other kind (the grip offset as it is: the gun's frame; the free hand's place scaled by the fit) all
cover them. Not in the VR Pose Studio yet (its hands take the left hand on a second grip as the Grip Editor's Second grip).

ImGui is made the first time the studio opens; until then nothing of it exists, and while the studio is closed none of
it runs (the input loop and the swap test one flag each). While it is open the keyboard and mouse are the studio's
(the game's own keys are held back) except the console's key; SDL's text input is on meanwhile (`VR_ImGuiTextMode`:
without it the fields get no typed characters); the windows' places are kept in `quakevr/imgui.ini`.

## Controls

| Input | What it does |
|---|---|
| Middle mouse | orbit round the focus |
| Shift + middle mouse | pan |
| Wheel | zoom |
| Alt + left mouse | orbit (laptops); Alt + Shift + left: pan |
| Right mouse drag | look about (the camera stays where it is) |
| F | frame the selection (bones or vertices; else the model) |
| 1 / 3 / 7 (numpad too) | front / right side / top; Ctrl: back / left / bottom |
| Left click | select a bone (Shift adds or toggles); on a handle: drag it |
| Q R G E T W P O | tools: Select, Rotate, Move, IK, Rest Pose, Weights, Paint, Pose Keys |
| Left click on the aim target | select it: its move handle (where the subject aims) |
| Pose Keys: left click on a dot | pick that part: its arrows and rings drag the key |
| Ctrl while dragging | snap: 15 degrees, 1 unit (Tools > Snap: always) |
| I | key the selected bones at the frame |
| Space, Left, Right | play / pause, a frame back, a frame on |
| H, Alt+H | hide the selected bones, show them all |
| Ctrl+Z, Ctrl+Y (Ctrl+Shift+Z) | undo, redo (every edit) |
| Ctrl+S | save |
| Esc while dragging | cancel the drag |
| F1 | the Help window (these controls) |

The handles are drawn with ImGui's lists (no ImGuizmo): the rotate tool's three rings in the bone's own axes and a view
ring, the move tool's arrows, plane squares and its centre (the view's plane); their size is Preferences > Handle Size.

## The windows

- **Tools**: the tool; Snap, Auto Key. *Aim Target*: Aim, Show, At Camera, its place, Reset (ahead, level), Select
  (above). *Display*: the mesh (Game: the game's own drawing, every procedural layer on;
  Shaded: the studio's skinned mesh, lit flat, its weights shown live; Wire: the game's drawing with the studio mesh's
  edges; X-Ray: the studio mesh see-through, every edge), colours (none, each vertex its bone's, the selected bone's
  weights as a heat map: blue 0, green 0.5, red 1), bones (octahedral, stick, off), joints, names on hover, the joints'
  limits (a ball joint's cone, a hinge's arc), the body proxies (vr_agbody's), the IK targets and the weapon grips as
  the game solved them. *Layers*: the game's procedural layers on the drawn model, each on or off to see what it adds
  (the arms on their weapons, vr_agrunt_ik; the legs, vr_aglegs; the upper body held, vr_agrunt_upper; smoothing,
  r_lerpmodels 3; hit reactions, vr_hit_react), put back on closing. *Weights* (the Weights and Paint tools): select by
  bone, grow, shrink, none; Assign to Bone, Set Weight, Normalize; the brush (add, subtract, smooth; radius, strength;
  mirror left/right). *Poses*: save the pose by name, apply one (all bones, or the selected only), delete, mirror,
  copy, paste.
- **Outliner**: the bones' tree; a click selects (Shift or Ctrl adds), the box hides or shows, S solos; the search
  box filters by name; a `*` marks a bone whose rest pose is overridden.
- **Properties** (the active bone): its joint and parent, how many vertices it owns; *Rest Pose*: its pivot and end
  (model units), length, the limits (a ball's cone and twist, a hinge's lower, upper and axis), Reset to Computed;
  *IK*: its two-bone chain if any and the elbow's pole; *Pose*: its turn and offset now (Rest: its local; an animation
  or a sequence: the change not keyed yet), Key, Clear.
- **Timeline**: the source (the rest pose; one of the model's own animations, by its frames' names: stand, run, shoot,
  load, painb...; a new sequence), play, pause, step, loop, speed (0.1x to 2x), the frame; the ruler scrubs (Shift: in
  between frames); the dope sheet: a row per bone with keys (and the selected bones), diamonds coloured by their
  interpolation (white smooth, green linear, blue step); click or Shift-click keys, drag them (whole frames), drag a
  box, right click for the interpolation, Delete or X deletes, Ctrl+C and Ctrl+V copy and paste at the frame; the
  sequence's events above the rows. Onion: the active bone's previous and next keys ghosted (all the bones, as
  sticks).
- **Sequence**: New Sequence (10 frames at 10 fps), its name, frames or seconds, fps, Loop, Arms, Hold, blend in and
  out, the base frame, its events (a name at a frame: Add at the frame, the frame dragged, deleted), Duplicate, Test on
  Subject (saved, then played on the subject as QC would: `vr_rigseq_play`).
- **Pose Studio Data**: the armed humanoids' pose keys (kind, weapon class, pose, phase: the seven parts the Pose
  Studio drags; a single gun's class is its pose category, Rifle / Pistol / Shoulder Launcher / Sprayer / Laser Cannon /
  Widowmaker, per weapon and kind in Combat > Armed Grunts > Weapon Holds; one not tuned yet starts as the Rifle's keys) and grips (a kind's every-weapon grip, or the subject's weapon's own, per hand), the Pose Studio's own
  data (vr_agrunt_poses, vr_agrunt_grips), drawn by the game at once; Print writes them as code (vr_agrunt_pose_dump,
  vr_agrunt_grip_dump). Undo covers them. Follow the Subject (the Pose Keys tool's key the subject's); in Pickup the
  legs' key too (the pelvis's place and turn, the feet, the knees' poles). The Pose Keys tool drags the same key.
- **Help**, **Preferences** (orbit, pan and zoom speeds, handle size, theme: dark, light, classic).

The File menu: Save, Revert to Files, Print as Defaults, Close Studio. Edit: undo, redo, copy, paste (all bones or the
selected), mirror the pose, clear the changes not keyed. View: the windows, frame, the six views.

## What it edits (the data model)

A Quake model has no skeleton: the rig is derived from the .mdl's frames when first needed (`vr_ragdoll.cpp`: its
vertices clustered by how they move, the clusters put on the seed table's bones; each bone's rigid fit to every frame).
Each vertex is on one bone (`Rig::vertBone`); the armed humanoids' arms are skinned on top by `vr_agarms.cpp` (a blend
between the upper arm, forearm and hand by the vertex's place along the arm), their legs by `vr_aglegs.cpp`. The QC
picks a frame; the client draws it with every procedural layer on top: the smoothing between frames (r_lerpmodels 3),
the hit reactions, the arms on their weapons (IK), the legs, the upper body held.

The studio's pose model: each bone's place is its parent's carried by a local motion in the pivot form,
`p' = R (p - c) + c + o` (c the bone's rest pivot, R a rotation, o an offset), so a bone's world motion is
`F_b = F_parent L_b`; every L the identity is the rest pose (the model's first frame). A frame of the model gives each
bone's rest-to-frame fit P_b, its locals `L_b = P_parent^-1 P_b`. What is drawn: each vertex moved by its bones'
`F_b P_b^-1` from where the frame has it, weighted.

- **Bone overrides** (the rest pose): a bone's pivot, end, its ball joint's cone and twist, its hinge's lower and upper
  limits and axis. In the game: the rig as derived, with them laid on.
- **Weights**: up to four bones a vertex. The studio draws them blended (linear). In the game: the bone with the most
  weight owns the vertex (`vertBone`; the bones whose vertices changed are fitted again to the frames, and their hulls
  and the triangles' bones made again), which every rigged system then uses: ragdolls, the smoothing, the hit
  reactions, the limb cuts, the arms' skin.
- **Poses**: named, each bone's local (absolute, from the rest pose).
- **Override tracks**: keyed deltas `D_b` laid after one of the model's own animations' frames' locals
  (`F_b = F_parent L_b D_b`), by its frames (from 0), for every monster drawn with that model.
- **Sequences**: new animations, each bone's keyed local (absolute; the bones without a track keep the frame drawn under
  it), with a length, an fps, a loop, blends in and out, events, a base frame (held while it plays: Hold), and Arms.

Keys are interpolated smooth (Catmull-Rom through them, eased at a track's ends; a loop wraps), linear or step.

### How it reaches the drawn model

`kickInstance` (`vr_animsmooth.cpp`) turns each rigged instance's bones by a rigid motion on top of its animation (the
hit reactions' path); the studio's layer (`rigseq::layer`) is laid there under the other turns:
`drawn = kick (F posed^-1) posed`, with F the sequence's or the override tracks' bones over the frame drawn. An armed
humanoid's arms are not its bones' but its arm skin's, solved each frame by the arms' IK from where its weapons are: a
sequence with **Arms** on (or an override track on an arm bone, or the studio posing the arms) gives the IK each hand's
place from its forearm bone (`agarms::Targets::fk`, the weapons drawn on the hands), the elbows' poles from the
bones'; otherwise the arms stay on the weapons as the game poses them, the shoulders carried by the chest as drawn.
With no files, no studio and no sequence started, nothing of this runs (the layer's check fails at once) and every
model is drawn as before. On the authored rig (`vr_rig2 1`) its live drawing lays the same layer on its authored bones:
"Animating the authored rig" below.

## The files

Both are text, one record a line, numbers written with `%.6g` and read back with `strtof`: a file read and written
again comes out byte for byte the same. Bones are named as the rig names them (`vr_rigdata_info <model>` lists them);
turns are rotation vectors in degrees (the axis times the angle, the model's axes: x forward, y left, z up), offsets
model units. `<model>` is the model's file name without its folder and extension (progs/vrgrunt.mdl: vrgrunt). The
folder is the game's (`quakevr/`), or `vr_rigdata_dir` under it (the tests use `rigstudio_test`).

`quakevr/rigs/<model>.rig.txt`:

    qvrrig 1
    model progs/vrgrunt.mdl
    bone head pivot 8.5 -0.3 32.5 end 10.4 -0.5 37.7 cone 50 twist 50 lower 0 upper 0 hinge 0 1 0
    weight 300 thigh_l 0.5 chest 0.5
    pose twist30
      key chest 0 0 30 0 0 0
    end
    override load
      track forearm_r
        key 3 smooth 0 -25 0 0 0 0
        key 7 linear 0 -10 0 0 0 0
      end
    end

`bone`: a rest override (pivot, end, cone, twist, lower, upper in degrees, hinge axis). `weight <vertex> <bone> <w>
...`: a vertex's weights. `pose <name>`: `key <bone> <turn xyz> <offset xyz>` lines. `override <animation>`: tracks of
`key <frame> <smooth|linear|step> <turn xyz> <offset xyz>` (the interpolation from that key to the next).

`quakevr/anims/<model>/<name>.anim.txt`:

    qvranim 1
    model progs/vrgrunt.mdl
    name taunt
    fps 10
    length 16
    loop 0
    blend 0.2 0.3
    arms 0
    hold 1
    base stand1
    event 6 shout
    track chest
      key 0 smooth 0 0 0 0 0 0
      key 6 smooth 0 20 -6 0 0 0
    end

They are read at a map load (each model's when it is first drawn or asked for); the studio's edits go to the game at
once (published as they are made), and to the files on Save. The rig's bone overrides and weights are laid on the rig
when it is derived (`rigdata::applyToRig`, from `ragdoll::rigFor` and `warmRigs`): once a session (the rigs are kept),
so a changed rig file takes effect at the next start of the game (the studio shows it at once). Unsaved changes are
marked (`*` in the menu bar, the outliner, the timeline's list); Revert reads the files again (undoable).

**Print as Defaults** (File menu, `vr_rigstudio_print`) writes the bone overrides as rows for the model's seed table
(`vr_ragdoll.cpp` / `vr_ad_rigs.inc`), the weights and the poses as commented tables, to the console and
`quakevr/proposed_defaults/rig_<model>_<date>.txt`, as the Pose Studio prints its grips.

## In the game: QC's API

No builtin (the builtin numbers are full): fields the engine reads and writes at each server frame's end, and a QC
function it calls (`QC/vr_rigseq.qc`):

    .string vr_rigseq;       // a request: the sequence's name ("-": stop); taken by the engine (cleared)
    .float vr_rigseq_t0;     // when it was asked for (VR_RigSeq_Play sets it: the start's time)
    .float vr_rigseq_len;    // the engine: its length in seconds once taken (-1: no such sequence for its model)
    .float vr_rigseq_frame;  // the engine: its frame now, each frame it plays (-1: none)
    .void(string seq, string ev, float frame) vr_rigseq_on; // the monster's own handler of its events

    void(entity m, string seq) VR_RigSeq_Play;
    void(entity m) VR_RigSeq_Stop;
    void(string seq, string ev, float frame) VR_RigSeq_Event; // called by the engine, self the monster

Each event reaches `VR_RigSeq_Event` (and so the monster's `vr_rigseq_on`) at the server frame its frame is reached
(within 1/72 s at the fixed frame rate), then `"end"` at its end (frame = its length) or `"stopped"` when cut short
(stopped, or with Hold on, the monster's own frame changed: a pain, a death). With Hold on the engine sets its base
frame when it starts. Every client is told (`QVR_SVC_RIGSEQ`, reliable) and draws it from that time, blended in and
out. `vr_rigseq_play <name> [entity|nearest]` and `vr_rigseq_stop` do the same from the console; `vr_rigseq_status`
lists what plays; `vr_rigseq_debug 1` prints each start, stop and event; `vr_rigseq 0` turns the layer off.

### The proof: the armed grunt's taunt (`vr_agrunt_taunt`, off by default)

An armed grunt seeing a player for the first time (its `th_run`, `vr_ag_hunt`) stands and plays its model's `taunt`
(`quakevr/anims/vrgrunt/taunt.anim.txt`: 1.6 s, the chest thrown back then forward into a shout, the head nodding, a
dip; its `shout` event its sight sound), facing him, its weapon still in its hands, then comes for him. A model with no
taunt (the enforcer's, for now) goes on at once; a pain cuts it short (its frame changes: `stopped`); 4 seconds at most.

## Tests

- `vr_agrunt_test 80` (`QC/vr_rigseq_test.qc`): a studio sequence played by QC on a frozen armed grunt: its length, its
  two events at their frames' times, its end, its frame field. Needs the sequence `rstest` (10 frames at 10 fps, events
  `fire` at 3, `footstep` at 7) where the files are read: the studio's commands make it (below).
- The studio's commands (each what a window or a handle does; for scripts and headless tests): `vr_rigstudio_select
  <bone> [add]`, `_rotate <x> <y> <z> [world]`, `_move <x> <y> <z>`, `_key`, `_frame <f>`, `_source rest|anim
  <name>|seq <name>`, `_seq_new <name> [frames] [fps] [loop]`, `_seq_event <frame> <name>`, `_seq_set <field> <value>`,
  `_save`, `_revert`, `_undo`, `_redo`, `_weight <vertex> <bone> <w>`, `_vertex <v | first <bone>>` (its weights,
  rest and posed places), `_rest <bone> pivot|end <xyz> | cone|twist|lower|upper <deg> | reset`, `_selverts`,
  `_assign <bone>`, `_pose save|load|mirror`, `_panel <window> 0|1`, `_view mesh|colors|bones|tool|snap|frame|onion|
  proxies ...` (`_view tool posekeys`), `_play [0|1]`, `_respawn`, `_print`, `_status`, `_count`, `_aim [on|show|camera
  0|1 | reset | at <xyz> | rel <forward left up> | select]` (no arguments: where it is and how far off it the weapon
  points), `_posekey [kind|class|pose|phase|follow|part <n>]...` (the window's key, printed); `vr_rigdata_info [model]`,
  `vr_rigdata_reload`.
- `vr_rigstudio_test_subjects` (open the studio first: `-flatscreenmode`; ends on `rigstudio subjects: N passed, M
  failed`): 14 steps: the subject changed through its weapon, two weapons, a flamethrower (its tank), the enforcer, a
  rigged monster, fists, the ogre, the map loaded again, the studio closed and opened here twice: each time exactly one
  subject, its own weapons, records and tank, and the other entities as many as at the first.
- `vr_rigstudio_test_aim [animation] [frame]` (default shoot 1; ends on `rigstudio aim: N passed, M failed`): the shotgun
  grunt aiming off, then at a target level ahead, high left and low right: its barrel's line against the way from its
  muzzle to the target (within 8 degrees), a screenshot of each.
- `vr_rigstudio_test_align [first case] [count]` (ends on `rigstudio align: N passed, M failed`): the grunt and the
  enforcer with the shotgun (Rifle), the nail pistol (Pistol) and the rocket launcher (Launcher) (two of them under
  the Pose Keys tool, one in Shaded display), the grunt with two nail pistols, and the shotgun again with Also Move:
  the barrel line checked on the drawn model (its forward and muzzle through `R_EntityMatrix` of the weapon entity:
  under 0.5 degrees and 0.5 units), the current key's turn(s) turned 10 degrees left and 5 up, Align to Target (drawn
  error under 0.2 degrees; the place untouched, or with Also Move the muzzle no further than turning alone leaves it),
  the line checked again, undone (the key exactly as misaligned), put back; then the line checked every frame for 72
  frames with the shoot animation playing (the grunt's shotgun, the enforcer's two nail pistols), and the aim target
  clicked (selected), dragged 20 up by its handle and undone. Screenshots misaligned and aligned per case, and close
  side and top views of the aligned gun for three. `vr_rigstudio_aimline [0|1]` prints the readout.
- `vr_rigstudio_test_posekeys` (ends on `rigstudio posekeys: N passed, M failed`): the studio's mock mouse drags the
  Pose Keys handles (the off hand's grip offset 2 forward and rolled 15 degrees round the gun's x, the right weapon 4
  forward and 20 degrees round z, the right elbow 4 down, the torso 15 degrees, in Pickup the pelvis 3 down): the key
  changed by that in its space, the drawn part with it (the off hand measured in the gun's frame); undone, the key
  exactly as it was. Screenshots before and after the grip offset's and the weapon's moves. (The torso step times out
  on origin/next too: a known one.)
- `vr_rigstudio_test_offhand [mode]` (open the studio with a gun; ends on `rigstudio offhand: N passed, M failed`): the
  subject's Run key's Off Hand set to `mode` (default 2 Free), held in Run, Aim, Run: free in Run (not held, no cup, 4
  units at least off its grip), on the gun in Aim; each change's off hand followed every frame in its chest's space (how
  long it moves, its largest step a frame: an ease, not a snap); the key put back exactly. Screenshots in each.
- `vr_rigstudio_test_typing` (ends on `rigstudio typing: N passed, M failed`): SDL key, text and mouse events pushed
  through the game's input loop into the fields: a sequence's FPS typed (Ctrl+click, "12.5r", Enter: 12.5, no tool
  change), Ctrl+Z, a typed value cancelled by Escape, the Outliner's search typed (Backspace; the game's keys untouched),
  the console's key opening and closing the console.

## Range of Motion: the authored rig (stage 1 of the grunt rig rebuild)

**View > Range of Motion (authored rig)**, `vr_rigstudio_rom on`, or **Debug > Tools > Rig Studio: Range of Motion
(Authored Rig)**. The new grunt rig (`vr_rig2.cpp`, `quakevr/rigs/<model>.rig2.txt`, made by
`Misc/quakevr/make_rig2.py`; ROUND22.md, "Grunt rig rebuild, stage 1"): 20 bones (pelvis, spine, chest, neck, head,
clavicles, upper arms, forearms, hands, thighs, shins, feet, the gun), each joint with its limits (a ball's elliptical
swing cone and twist, a hinge's bend from straight) and blended skin weights (up to 4 bones a vertex). With `vr_rig2 1`
the game draws the grunt on it (stage 2) and the Timeline animates it (below: "Animating the authored rig").

- **Mesh drawn on**: *Authored rig* (`vr_rig2 1`: its blended weights; Game display the engine's skinned model, Shaded
  the studio's) or *Old rig* (`vr_rig2 0`: the old derived rig moved the same way, each old bone as its namesake: the
  comparison). *Dual quaternions* (`vr_rig2_dq`) instead of the linear blend. The line under it: the pose's faults as
  `vr_rig2_check` measures them (see-through, flipped, collapsed, sharp folds, stretch), live.
- **The joints**: the list (indented by parent), or a click on a bone in the view; a second click on the picked bone
  drags its limb by its end (held within its limits). The picked joint's limits are drawn: where its bone's end can go
  (magenta: a ball's swing ellipse, a hinge's arc) and its twist's two ends (cyan ticks).
- **Sliders**: Swing X (toward the joint's forward), Swing Y (toward its side), Twist; a hinge's Bend. Each is held
  within the limits (a swing onto its ellipse). **Sweep** plays the joint through each axis and once round its swing's
  edge; **Reset Joint**, **Reset All**, **Random Pose** (every joint within its limits, as a ragdoll's).
- **Joint (rest pose, limits)**: its pivot, end, the limits (and a ball's neutral direction, a hinge's axis), edited
  live; undoable (Ctrl+Z, with every other edit); **Save** writes the rig file (`vr_rigdata_dir` respected), **Reload**
  reads it again.
- Heat colours (Display > colours: the selected bone's weights) show the picked joint's blended weights.
- On the authored rig the held weapons ride its hands: the right on its `weapon` bone, the left on `weapon_l` (or
  `hand_l`), each hand's grip frame as the old rig's (the rest pose's) at the weapon's grip offset (the Grip Editor's).
  On the old rig the arms' solve places them, as in the game.
- Commands: `vr_rigstudio_rom [on|off] | rig old|new | joint <j> | set <j> <x> [y] [z] | reset | random [seed] |
  sweep <j> | limit <j> swingx|swingy|twist|range <min> <max> | pivot <j> <x y z> | save | camera <j> [yaw] [pitch]
  [dist] | status | test [weapon]`. `vr_rigstudio_rom test` ends on `rigstudio rom test: N passed, M failed`;
  `test weapon` (the weapons on the authored rig's hands, six poses, a screenshot each; a dual-wielding subject tests
  the left hand too) on `rigstudio rom weapon: N passed, M failed`. Both wait for the subject: run them as it opens.
- `vr_rig2_check [model] [detail lines] [joint|random|envelope]` (ends on `rig2 check: N passed, M failed`): every
  joint swept, joint pairs, 1000 random poses, the old rig on the same poses; `envelope` prints how far each joint
  turns cleanly from rest, each way alone, beside its limit. `vr_rig2_info`, `vr_rig2_reload`.

## Animating the authored rig (`vr_rig2 1`)

With **Debug > Grunt on the Authored Rig (Next Map)** on (`vr_rig2 1`) and a map loaded with it, the grunt is drawn on
the authored rig, and so is the studio's subject: the Timeline is the authored rig's. Everything works as on the old
rig; what differs:

- **The bones**: the dope sheet and the outliner list the authored bones (pelvis, spine, chest, neck, head, clavicles,
  upper arms, forearms, hands, thighs, shins, feet, the weapon). Rotate, Move, the IK tool, named poses (load, mirror),
  Auto Key, interpolation, onion skin, copy and paste, Mirror Pose and undo all work on them.
- **Limits**: a key is held within its joint's limits (Range of Motion's: a ball's swing ellipse and twist, a hinge's
  bend), an override's laid on its frame's own turn there; the console says `<bone>: keyed within its joint's limits`
  when it held one. A jointed bone turns about its pivot only: Move keys only the pelvis (the root). Between keys the
  drawn turn is held too (the game does the same).
- **Its own files**, the old rig's left as they are: `quakevr/rigs/<model>.rig2anim.txt` (header `qvrrig2anim 1`: poses
  and override tracks, no bone overrides or weights: the authored rig's rest and weights are its `rig2.txt`, edited by
  Range of Motion) and `quakevr/anims/<model>/<name>.anim2.txt` (header `qvranim2 1`, otherwise the sequences' format).
  Each key's turn and offset are written **in its bone's own frame**, to 0.001: a ball joint's `x` is Swing X, `y`
  Swing Y, `z` Twist (degrees, as Range of Motion's sliders; exact for one axis alone, a rotation vector when mixed); a
  hinge's `x` is its bend (`y`, `z` across and along it); the pelvis's and the weapon's are the model's axes. Hand-edit
  them as the old files (`vr_rigdata_reload`, or the studio's Revert, reads them again).

      qvrrig2anim 1
      model progs/vrgrunt.mdl
      override load
        track chest
          key 3 smooth 15 0 0 0 0 0      # bent 15 degrees forward over load's frame 3
          key 7 smooth 0 -10 3 0 0 0
        end
      end

- **Carrying the old rig's animations over**: `vr_rig2anim_import [model] [force]` (Debug > Tools: Carry the Old Rig's
  Animations to the Authored Rig) reads the old files and writes the authored ones (kept if there, unless `force`): each
  track on its namesake, the old chest's turns shared half and half by the spine and the chest, the old head's by the
  neck and the head (their pivots where the old ones were), every key held within the limits. The shipped taunt was
  made so (`anims/vrgrunt/taunt.anim2.txt`).
- **In the game** the authored live drawing (`vr_rig2_game.cpp`) plays them on the frames' authored bone poses, under the
  hit reactions, as the old path does: a model's override tracks by its animation group, QC's sequences (`vr_rigseq`
  requests: the taunt, blends in and out, events, Hold, Arms). An armed one: the upper body's hold (`vr_agrunt_upper`)
  keeps the keys' turn of the chest on the hips (on both rigs); an arm keyed (a sequence's Arms, an override track on a
  clavicle, upper arm, forearm or hand) hands its arms to the keys (the arm bones as keyed, its hand targets from the hand
  bones, the weapons on its hands: the right on the weapon bone), else the arms' IK as before. A ragdoll starts from the
  pose drawn. The enforcer gets the same once its rig is on the authored latch (all of it is per model).

**How to animate the new rig, step by step**: Debug > Grunt on the Authored Rig on; load a map; Debug > Tools > Rig Studio
(flat screen); the Timeline: pick an animation (an override of the grunt's own: `load`, `shoot`...) or a sequence (New:
its length, fps, loop; events, Hold, Arms in its properties); pick a bone, Rotate (R) or Move (G) it: Auto Key keys it
at the frame (I by hand); scrub, play, onion skin; File > Save writes `rig2anim.txt` / `anim2.txt`. The
game draws the edits at once (no save needed to see them on grunts in the map). QC plays a sequence with
`VR_RigSeq_Play(monster, "name")` or `vr_rigseq_play <name> nearest`.

Tests: `vr_rigstudio_test_rig2anim` (open the studio first; ends on `rigstudio rig2anim: N passed, M failed`; on either
latch: the dope sheet's bones, `load`'s chest and right upper arm keyed at frames 3 and 7, a forearm bent past its
hinge held and undone, Mirror Pose and undo, saved and read back; it writes the subject's files: use `vr_rigdata_dir
rigstudio_test`), then `vr_agrunt_test 82` (`vrtesthall`, `vr_rig2 1`; those keys measured on a live armed grunt held at
load's frames 3 and 7: each keyed bone drawn against its key, `vr_rig2anim_check`; ends on `rig2anim check: N passed, M
failed`). `vr_agrunt_test 81` checks the taunt's shout event (0.6 s) and end (1.6 s) on both rigs.

## Limits and rough edges

- In the game a vertex is drawn by one bone (its heaviest), except an armed humanoid's arm skin, whose blend is its
  own: the studio's blended weights show only in the studio's mesh (Shaded, X-Ray).
- The rig's bone overrides and weights take effect when the rig is derived: the next start of the game.
- A bone's parent is shown, not edited (the rig's tree is the seed table's).
- The IK tool solves two-bone chains (a hinge on a ball joint: the arms, the legs) in the plane the elbow is in; the
  joints' limits are shown, not enforced while posing. The game's arm IK (vr_agarms) keeps its own shoulder limits
  (`vr_agarms_limits`): with an armed humanoid's upper arm selected and Limits on, its clavicle (yellow, from its pivot
  near the neck to the joint) and the cone its upper arm keeps within (magenta, `vr_agarms_shoulder_cone`; dimmed when
  off) are drawn besides the ragdoll's cone.
- The studio's mesh is drawn with ImGui's lists (sorted by depth, no depth buffer): a few crossing triangles may show
  through at the X-Ray's edges.
