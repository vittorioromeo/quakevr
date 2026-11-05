# Flatscreen play (`-flatscreenmode`)

Quake VR's gameplay played on the desktop screen, with no headset. Everything here is behind the command-line flag
`-flatscreenmode` and only while there is no headset session (`VR_IsActive() == 0`) â€” and the flag implies no session
(see below). Start the game without the flag and none of it happens.

Engine side: `Quake/vr/vr_flatplay.cpp`. Game-code side: `QC/vr_util.qc` (`VR_FlatPlay`), `QC/weapons.qc`,
`QC/vr_wpnforcegrab.qc`.

## Turning it on

    ironwail.exe -flatscreenmode

**`-flatscreenmode` implies no VR backend.** The mode runs only while no headset session is up, so the flag is what asks
for no backend at all: no OpenXR session, no mock one either â€” what `vr_backend none; vr_restart` used to be typed for.
The game says once, in the console, that the flag implied it (`startBackend`, `Quake/vr/vr_main.cpp`). Start the game
without the flag to play in a headset.

`vr_flatplay_status` (console, or Debug > Reports > Flatscreen Play Status) says whether the flag was given, whether a
headset session is up, and whether the mode is on.

## What the mode does

**The hands are tracked by the game, not by a headset.** The client marks the player's hands as tracked
(`QVR_BUTTON_HANDSTRACKED`, `Quake/vr/vr_client.cpp`), which is what lets the game run its VR gameplay for a flatscreen
player at all: held weapons, the force grab, melee, the holsters, body touches.

**The arms are posed by the game** (`flatplay::poseHands`, called from `Quake/vr/vr_hands.cpp` in place of the old
straight-out-in-front pose). The pose is built from the view angles, the weapon in each hand and the player's own motion:

- a held weapon sits out in front of the chest, on its own side, the elbow bent; an empty hand hangs at its side whatever the look (fingers down, level with the body: `hangPitch`, until it punches, reaches or goes to a grip); the free hand
  rests near the other hand's gun;
- the hands are turned so the weapon in them shoots along the crosshair, whatever that weapon's Shot Pitch and Yaw
  (`aimRotAtCrosshair` inverts `weapons::shotAngles`), and a gun points from its muzzle at what the crosshair is on
  (see The aim, below); the drawn gun and its muzzle follow the hand, so the shot and the viewmodel agree;
- the whole pose is measured from the shoulders (`hands::bodyAnchor` with `vr_shoulder_offset`), so it follows the body
  whatever the height calibration; `vr_flat_reach` scales how far the hands sit from them;
- the drawn hands are held to what the arm can hold them at (`vr_flat_arm_hold`, in `drawnOffset`): the flat anchor does
  not tip with the head (`hands::bodyAnchor`) while the body the arms hang from does (`avatar::solveTorso`), so a look
  straight up, a knock or a punch can ask a hand for more than the arm has, and past that the drawn hand hangs off the
  wrist the arm ends at. Each drawn hand is kept within `armSpan()` â€” the model's bones with `vr_body_arm_length` and
  `vr_body_arm_stretch`, plus the shoulder's own `vr_body_shoulder_reach` â€” of the shoulder the body poses that arm on,
  and the hands follow the view's up and down only as far as `vr_flat_pose_pitch`. A punch is a *thrust along the aim* (`punchSwing`: chambered back along the arm's own line, thrown to what
  the arm can hold the hand at, drawn back again â€” and not eased, because the game's melee rules ask the striking hand
  for vr_melee_speed, and thrown no further than the arm can hold it (0.13 s chambered, 0.12 s out, 0.22 s back: a punch peaks near 7 m/s against the 3.2 m/s the game asks a striking hand for), not the straight shove off the shoulder it used to be: that one asked the off hand's arm for 14 units more than it
  had, and its hand ended 401 mm off its wrist.

**The hands are locked to the view.** The game's pose is rigid in the view's frame (only a change of pose â€” a weapon
taken, a punch, E's reach â€” is eased, in that frame). The hands are made at the frame's start, before the mouse turns the
view and before the client moves the player, so in this mode they are made again twice (`hands::relatch`, the frame's
velocities kept): for the move (`buildMove`: the shots go where the crosshair is now) and for the drawn view
(`VR_SetupViewEntities`: the origin just moved to). The drawn hands are then carried onto the camera as drawn (Quake's
view bob, strafe roll, kick and step easing). With the three looks below at 0 the drawn weapon does not move in the view
at all (0.014 units strafing and turning; it was 23.7 units strafing and 7.5 degrees at 540 degrees a second turning).

**The looks** are the drawn hands' only (`flatplay::drawnOffset`, added like the fatigue shake; the muzzle is put back on
the steady hand), so the shots and the aim stay on the crosshair whatever they are set to (the drawn gun also eases onto
the aim's turn, 18/s, where the game's hand has it at once: the crosshair crossing a near edge moves its point by metres):

- **Move Sway** (`vr_flat_sway`, 1): each change of the player's velocity kicks the hands the other way and a spring
  settles them, a little overshoot: they lag as he starts and stops (~1.3 units), dip on a jump and on a landing. At a
  steady run they are where they rest;
- **Walk Bob** (`vr_flat_bob`, 1): a figure of eight on the ground, its size and rate with the speed (0.7 units across,
  0.6 down, a little roll at a run), and breathing at rest;
- **Turn Lag** (`vr_flat_turnlag`, 1) and **Turn Lag Recovery** (`vr_flat_turnlag_speed`, 1): "weapon inertia": the hands
  keep their direction as the view turns and a critically damped spring brings them back (18/s times the recovery).
  ~2 degrees behind at 180 degrees a second, at most 4 (softly), back within a quarter second. A view set in one frame
  (a teleporter, a respawn) leaves no lag.

These are tuning constants (`tune <name> <value>`; **Debug > Tunables > Flatscreen > Looks**, saved in `tunables.cfg`). Debug
> Reports > Flatscreen Play has Status, Print (`vr_flatplay_debug`: prints each E and V press, each hand's punch and where
the hands are; 2: each frame the drawn weapon in the camera's frame, `flatview:`, each move's shot against the view,
`flatmove:`, each gun's aim, `flataim:`, and each drawn hand against the arm the body poses for it, `flatarm:` -- how far its
wrist is off the body's wrist, how far the hand is past what the arm can hold it at, its speed and its wrist's bend), and
Punch (`vr_flat_punch`). Test: `bash Misc/quakevr/flatplay_test.sh <agent>`.

**The keys.** Read from the keyboard state, not from bindings â€” nothing is rebound and no config changes (Left Ctrl is the
one exception: the mode holds it while it runs and hands it back before the game writes its config, below); they only work
while the game has the keys (`key_dest == key_game`). A punch is not a key: it reads the hand's own fire button
(`client::attacking`, what LMB and RMB drive in VR), so `+attack`, `+offhandattack` and `vr_flat_punch` reach it from the
console.

| Key | |
|---|---|
| mouse LMB | fires the right hand (as in VR); with a weapon a swing strikes with in that hand â€” an empty hand (the fist), the axe, the maul, the sword, the crowbar, the super axe â€” it is also the punch `V` makes |
| mouse RMB | fires the left hand (as in VR); the same punch with a swinging weapon in the off hand |
| `E` | force-grabs what the view is pointed at, with the free hand (`impulse 46` Ã¢â€ â€™ `VR_Forcegrab_FlatGrab`; the search, the flight and the catch are the game's own force grab). With both hands holding weapons there is no hand to collect with and E does nothing. |
| `1` | cycles the **left** hand's weapon through the weapons collected |
| `2` | cycles the **right** hand's weapon |
| `V` | a punch with the right hand: a real motion of the hand (wound up, thrown, drawn back), so the game's own melee rules decide whether it lands. `vr_flat_punch [main\|off]` drives either hand's punch from the console. A weapon that no swing strikes with never punches from its fire button â€” the chainsaw's trigger runs its chain, a gun's fires (the mode's own weapon set: `punchesWith`, `vr_flatplay.cpp`); `V` is the hand's punch whatever it holds, as it was. A hand that already started a punch this very frame is joined by the other only when both hands hold a swinging weapon. A press while that hand's punch runs is no new punch. An empty hand on the held weapon's grip lets go of it for its punch and takes it again after |
| Left Ctrl | crouches while it is held, stands up as it is let go; `+flatcrouch` and `-flatcrouch` hold and release the same from the console (see Crouching below). Right Ctrl is the same key to the engine (`K_CTRL`: `Quake/in_sdl.c`). While the mode runs it is the crouch key **only** â€” the plain game's `+attack` is taken off it, so holding Ctrl crouches and fires nothing; `vr_flat_crouch_fire 1` gives Ctrl its own bind back and then it fires as it crouches (see Ctrl's firing bind below) |
| `F` | switches the flashlight: the engine's own `vr_flashlight_toggle`; put out, it stays out (a level change and a saved game included) |
| `G` | lets go of what a hand holds, the main hand first and the off hand after (`impulse 47` â†’ `VR_Forcegrab_FlatDrop`, `QC/vr_wpnforcegrab.qc`): a weapon out of the hand the way the game drops one, a box or a prop carried in it let go at his feet (physics again, free to take again); carried in both hands, one press frees both |

`1` and `2` go through the existing weapon cycling (`CycleWeaponCommand`, the controller's B and Y); `getWeaponCycleMode`
In this mode the cycle (1, 2 and the mouse wheel) also stops at the empty hand (the fist), both directions; VR cycling is unchanged.
returns ALLOWED while the mode is on, whatever `vr_weapon_cycle_mode` says. The other number keys keep Quake's own
weapon select.

**The hands are closed.** There is no grip to hold on the desktop, so a flatscreen hand reports its grip as pressed:
`gripping()` in `Quake/vr/vr_client.cpp` answers for the `QVR_VRBITS0_*HAND_GRABBING` bits and for `client::grabbing`
while the mode runs. The game's hold checks ask for that bit (`VRWeaponLetGo` in `QC/weapons.qc`, the carried thing's
upkeep in `QC/vr_carry.qc`), and with an open hand everything a flatscreen player is given leaves the hand on the frame
it arrives in. With it, what the mode puts in a hand stays there: `E` takes, `1` and `2` move weapons between the hands,
and a hand lets go of nothing by itself (`G` is the key for letting go).

**No reloading.** `VRIsWeaponReloadingEnabled` (`QC/vr_util.qc`) is off: a magazine is refilled from the reserve as it
empties, with no handling step.

**A box grabbed with `E` is taken at once.** `VR_Carry_AutoCollect` (`QC/vr_carry.qc`) follows `vr_carry_autocollect`, the
menu's Item Pickup Mode; for a player in the mode it counts as at least 1 â€” mode 1's collectable set, ammo and health
(backpacks are mode 2's) â€” whatever he has saved, since there is no hand-to-hand step on the screen to confirm a pickup. The
cvar itself is never written, so a headset player on the same save keeps what he set.

**Crouching (Left Ctrl).** `pollKeysFlat` (`Quake/vr/vr_flatplay.cpp`) reads Left Ctrl with `E`, `F` and `G` and eases one
number from 0 to 1 over `vr_flat_crouch_time`; `+flatcrouch` and `-flatcrouch` hold and release it from the console. That
number stands for `vr_flat_crouch_drop` units: `VR_FlatViewDrop` takes them off `r_refdef.vieworg` (`Quake/view.c`), and
`placeFlat` (`Quake/vr/vr_hands.cpp`) puts the head it poses, and `headHeight` with it, down the same, so the drawn body
follows the view. The pose's head goes over the move wire as any other head does, and the game's own crouching does the
rest: `hull::updateCrouch` (`Quake/vr/vr_hull.cpp`) chooses a crouched box for the eyes' new height (36 at the default 18,
out of `vr_crouch_height` and `vr_crouch_step`), the CROUCHED bit (23 of `vrBits0`) arrives as it does for a headset player
who crouches, and `crouchSpeedMax` slows him as it slows him there. What the server asks is the move's own word, per client
(`protocol::moveFlatCrouchBit`, bit 14 of `vrBits0`, set while his eased crouch is above 0), never the host's keys, and it
needs no protocol step: nothing else ever set that bit of the wire, a client that does not set it reads as not crouching by
it, and a demo records the server's messages, not moves. Nothing is added to the move wire, and nothing new
collides: a crouch is not a jump. `.vr_stand` â€” how upright the game thinks he is, read by `QC/vr_kdtrig.qc` â€” is worked out
from the same numbers in `Quake/vr/vr_server.cpp`: 1 while he stands, less as he comes down (a headset player's value is
computed as it was). Outside the mode nothing moves: `crouchDrop()` answers 0 and `VR_FlatViewDrop` returns at once.

**Left Ctrl's firing bind, taken and given back.** In the plain game Left Ctrl is the fire key: `bind CTRL +attack` is in
`Quake/default_cfg.h` and, saved, in `ironwail.cfg` (`bind "CTRL" "+attack"`). The mode needs that same key for the
crouch, and one key that does both is a crouch that shoots â€” so while flatscreen play runs the engine's binding table
holds Ctrl for `+flatcrouch` (`Key_SetBinding`, `Quake/keys.c`): the key read by `pollKeysFlat` and the bind lead to the
one eased crouch, and pressing it fires nothing. The plain game's bind is remembered, not thrown away, and it is
remembered afresh on any frame the mode finds Ctrl holding something else, because whatever that is is what the player
has: a config's own bind line, a `bind CTRL â€¦` said on the console, an `unbindall`. The mode's frames begin inside
`VR_BeginFrame` (`Quake/host.c`), ahead of the same frame's `Cbuf_Execute` where the start-up configs run â€” the configs
put `+attack` back over the mode's bind a frame later â€” which is why the mode asks for its key again every frame instead
of once. Before the engine writes its config out â€” at a quit, at the shutdown, with the Debug menu closed on a setting
changed, with `writeconfig` â€” `VR_FlatKeysForConfigWrite` (`Quake/host.c`, `Host_WriteConfigurationToFile`, just before
`Key_WriteBindings`) hands the key back, so the file on disk keeps the plain game's Ctrl and never keeps a `+flatcrouch`;
at a quit there is no next frame for the mode to take the key up again, which is the point. In a headset session, and in a
game started without the flag, the mode never ran and never took the key. whole exchange: Ctrl keeps `+attack`: Ctrl keeps `+attack`, so holding it crouches and fires as it does in the plain game.
`vr_flatplay_status` prints which bind Ctrl holds now, and `vr_flatplay_debug 1` prints the frame it was taken.

**The flashlight is worn on the head, and `F` switches it.** `vr_flashlight`'s "Always on Head" behaviour (`vr_flashlight 2`)
is what the mode stands in for: `enabled()` and `headMode()` in `Quake/vr/vr_flashlight.cpp` answer as they do for it. There
is no belt to reach for on the screen: the torch is worn at the temple and lit at a fresh start (`reset`). `F` sends the
engine's own `vr_flashlight_toggle`, and in the mode the state the engine saves each frame is left as it is when a game is
restored (`restoreState`): a torch put out with `F` stays out, over a level change. A headset game is as it was â€” its
restored state still forces the head-worn torch lit.

**The body presses the buttons it walks into.** `Quake/vr/vr_physics.cpp` lets a flatscreen player's body do the hand
touch it does for an untracked player, whatever `vr_body_interactions` says.

**Co-op: the mode is each player's own.** A client in the mode says so in its move (bit 15 of the VR block's
`vrBits0`, `protocol::moveFlatplayBit`); the server takes it out of the QC's bits and sets that player's
`.vr_flatscreen` (`Quake/vr/vr_server.cpp`). Every game-side gate asks for the player it is about (`VR_FlatPlay(player)`
in `QC/vr_util.qc`: reloading, `E`'s grab, `1` and `2`, the grip-less catch; the body's hand touch in
`vr_physics.cpp`). So a flatscreen host leaves reloading on for the VR players who join it, and a flatscreen client on a
VR host gets `E`, `1` and `2` and no reloading. The punches need no gate of their own: they are a motion of the hand, and
`VR_Melee_Deal` / `VR_Melee_Strike` (`QC/weapons.qc`) deal the damage of whatever that hand holds, as they do for a hand
in VR. The engine's `vr_flatplay` cvar is the local client's mode only (its
hands, its flashlight, its keys).

## The ironsight, and the free hand's grip

`vr_flatsight.cpp` works on the pose `poseHands` just made, and only while the mode runs. With one hand holding and the
other empty, the empty hand is carried onto the held weapon's two-hand grip (`pinToGrip`) and `twohand::apply` runs over the
pose, so the hold is the game's own â€” its helping flag, its `helpKind`, its transition, the weight and recoil that follow
them â€” and not a look, and only for a grip the pose can bring a hand to. In the flat view the hotspot stands 12.5 and 12.8
units *ahead* of the hand holding the weapon for the rocket launcher and the proximity gun, whose grips `vr_twohand` then
takes (`helpKind 1`, the transition in full); it stands 5 units off that hand, 3 to 5 units to its *rear*, for the shotgun,
the super shotgun and the nail gun, where a second hand has nowhere to go and carrying one there would only fold the two
hands onto each other. A weapon whose model has no such hotspot, a grip a second hand cannot reach, and dual wielding keep
the pose above exactly as it is, and `vr_flatplay_status` prints the measure the choice rests on: "hotspot: 5.0 units from
the held hand (fwd -3.2, right 0.4, up -3.8 of it)". That is the hotspot *nearest the hanging hand* (`chooseGrip`, as for a tracked
hand), which on those guns is the rear Cup: each of them also has a Grip hotspot well ahead of the hand (shotgun fwd 11.6, super
shotgun 10.1, nail gun 9.2, super nail gun 6.6).

## The stance

A long gun (a sight line, its muzzle 9.5 units or more from the hand: `longGunMuzzle`) with a two-handed hotspot held with the other
hand free takes a stance (`stanceAndLook`, `vr_flat_stance` 1; Debug > Tunables > Flatscreen > Free Hands):

- **The torso turns**, the free hand's shoulder forward (`vr_flat_stance_turn`, 25 degrees of `s.bodyYaw`, eased over about 0.4 s).
  The view doesn't turn. With the main hand holding, that is a turn to his right: the gun then lies across his chest. It is the
  drawn body's: the game's hands are posed from the anchor facing the view, so a punch lands where it would without the stance.
- **The gun's hand comes up towards the shoulder pocket** (`vr_flat_stance_back` 10, `_in` 2, `_up` 4, taken off Gun Forward, Gun Out
  and Gun Down). That is all that moves the gun, and it is the pose's: the shots are solved from wherever the muzzle stands, as ever.
- **The free hand is drawn on the weapon's own hotspot**: place, turn, fingers and offsets all come from Weapon Offsets through the
  same code that draws a helping hand in a headset (`steadyOther`, `heldBy`: `2h_fxd_*`, `hsN_pitch/yaw/roll`, `hsN_v*`, the style,
  overlap and curls), so editing the hotspot moves the flat hand. Nothing in this mode decides a place or a turn for it. Which
  hotspot: `view::twoHandSpot`, a free hand having no place of its own to choose by: the grip furthest along the gun (a Cup with
  Two-Handed Aim counts as a grip), else a cup.
- **It is a look, no grip.** The game's hand stays in the hanging pose: `twohand::helping`, `transition`, the aim, weight, recoil and
  punches don't know of it, and `chooseGrip` still chooses from the hanging hand. What draws a hand reads `twohand::drawnTransition`
  and `drawnHelping` (`flatsight::lookShare` on top of the real ones). A gun whose nearest grip the arm can take for real (the
  grenade launcher, proximity gun, rocket launcher, lightning gun, laser cannon, Enforcer's rifle) keeps its real grip as before,
  with the stance on top.
- **An arm that can't reach eases back to hanging** with no gun moved (`lookFar`: past 0.92 of `armReach` from the shoulder the body
  poses, `avatar::skeleton`; hand back on under 0.89). A punch or E takes the hand off as fast as a grip's (`gripEase`). With the gun
  gone it eases off its last place (`lookLast`).

Why the earlier barrel rest looked wrong (`vr_flat_rest`, retired with its numbers; old lines are dropped by `VR_RetiredCvar`): it made
up its own point on the barrel (a share of the way to the muzzle) and its own turn (palm to the gun's up), and never read the
hotspot's data, so its hand angle could not have been VR's; and it pulled the gun in 7 to 9.6 units so the arm could reach.

Arm used, off shoulder to hotspot, as a share of `armReach` (21.9; 0.92 is the most it keeps), read from `vr_flatplay_status` by
`Misc/quakevr/flatstance_test.sh`: on the shotgun, 1.31 with the torso turned and the gun's hand at Gun Forward, Out and Down, and
0.83 in the stance (super shotgun 0.77, nail gun 0.76, super nail gun 0.70, grunt gun 0.69). In the ironsight at the default eye
relief 14 those guns are out of reach (shotgun 1.27, super shotgun 1.15, nail gun 1.30, super nail gun 1.21, grunt gun 1.32), so the
hand hangs while sighted; the solve is exact as ever, and the relief is not given up for a look. A weapon whose data has no grip or
cup hotspot at all has no stance, and its hand hangs: the toolgun and the glowing hammer's model.

`vr_flatplay_status` prints four `stance` lines: its state, the arm's share, the hotspot in the held hand's frame, and the drawn hand
there with how far off the hotspot it stands.

The mouse wheel's button (`K_MOUSE3`), or `vr_flat_aim` from the console, turns the ironsight on: the held weapon is brought
onto its sight line (see The ironsight's solve, below), and the field of view narrows by `vr_flat_aim_fov`: `gl_screen.c` asks
`VR_FlatAdsFov` where it chooses `r_refdef.basefov`, and the mouse look's own measure comes from the same `basefov` (`in_sdl.c`'s
`IN_FovScale`), so the look and the crosshair keep their feel. It refuses while dual wielding, and on a weapon with no sight
line (the melee weapons; the fallback is no ironsight at all, the hipfire pose stays and the view does not narrow).
`vr_flatplay_status` prints both hands and the ironsight, and `vr_flatplay_debug 2` adds a `flatsight:` line each frame
with where the muzzle stands off the crosshair before and after the move, the eye relief and how far a wall has it back.

The narrowing is no step at the press. `vr_flat_aim_time` (0.25, 0.05..1.00 seconds, 0: at once) is how long the
ironsight's whole travel takes â€” onto the crosshair, and back off it â€” and the field of view is narrowed by
`vr_flat_aim_fov`'s degrees times how far that travel has gone, so it closes over those seconds and opens back off them
in step with the weapon: both the hand's move and the view go by the same fraction (`st.adsT`, `fovNarrowNow`), and the
depth the view settles at is the one it always settled at. Measured on the `flatsight:` lines, which print the field of
view being drawn every frame: at the default 0.25 it steps from 106.3 down to 94.4 degrees (fov_x) over 18 frames,
0.236 s, and back up over the same 18; at 0.05 it is done in 4 frames (0.042 s); at 0 it is one frame; at 1.00 it was
still closing 60 frames after the press with 9.84 of the 11.87 degrees taken, and 5 in the console is read as 1.00.
`Misc/quakevr/flatkeys_test.sh` section 7.

A flatscreen hand is always gripping (`gripping`, in `vr_client.cpp`), so the frame the pose brings the empty hand onto a
grip is stood in for the grip button's press by `twohand::armFlatGrip` â€” the one thing this adds to `vr_twohand`,
next to `setFreeCandidate`, which is the same shape: a public setter for one internal flag. It arms on the frame the pose
brings the empty hand onto a grip, and on each frame it still holds the hand on one nothing has taken: a flatscreen hand
is not lifted off a grip it was placed on; it stops the frame `vr_twohand` takes the hold, or the frame the weapon changes.
Its own distances, its angle test, the foregrip's stickiness and the weapon's Two-Handed setting decide the hold, unchanged.

## Where the weapon sits, and where his eye is on the screen

The pose's numbers are tuning constants (`tune <name> <value>`; **Debug > Tunables > Flatscreen**: Held Weapon, Free Hands,
Ironsight, Eye, Crouch, Looks; a value tuned stays, in `tunables.cfg`, and `Misc/quakevr/tune_promote.py` makes it the
default), read in the view's own frame and clamped at the read (`poseNum`: -30..30, `nearNum`: 8..60 units, `camOffMax`: 20).
`vr_flatplay_status` prints the pose and the eye's place, and `vr_flatplay_debug 2` prints them every frame (`flatsight:`,
`flatcam:`, `flatmove:`).

**The held weapon** â€” **Flatscreen Gun Forward** (`vr_flat_gun_ahead`, 14), **Flatscreen Gun Down** (`vr_flat_gun_down`, 12), **Flatscreen Gun Out**
(`vr_flat_gun_out`, 4): how far out in front of the chest the gun sits, how far below the chest, and how far to one side.
Forward is along the look; *down* is written as a positive number of units *below*, and *out* as a positive number of units
*away from the body's centre line* â€” the main hand's right, the off hand's left, which is the side `twohand` measures its
grips to. Each is scaled by `vr_flat_reach`, as the pose always was. **Flatscreen Gun With The Crosshair** (`vr_flat_gun_near`, 24, 8..60) is how
far the drawn view may stand from his head and still carry the gun with it; below that the gun is left on his body while the
view comes down, which is what a small `vr_flat_crouch_drop` with a small keep distance looks like.

**The empty hand** (`vr_flat_empty_fwd` 5, `vr_flat_empty_down` 13, `vr_flat_empty_out` 7) hangs at its side, and **the
free hand's rest** beside him while the other hand has a gun (`vr_flat_support_fwd` 6, `vr_flat_support_down` 15, `vr_flat_support_out` 6)
is where it waits before a grip takes it. Neither is a look bolted on: the empty hand still travels onto the held weapon's
two-hand hotspot (`pinToGrip`, `helpKind 1`) and `twohand::apply` still runs over the pose, so the two-handed grip, its
`helpKind` and its transition are the game's own whatever these are set to.

**The aim.** A gun (a weapon with a sight line, in either hand, ironsight or not) points from its muzzle at what the ray
through the middle of the screen hits, recomputed every pose (`aimAtPoint`, `vr_flatsight.cpp`): the line is traced as a shot's
is (the world, monsters' models, gibs and heads, him left out), far along the ray (4096) if nothing is there. QC fires along
the shot's angles on the line through the muzzle (`VR_ShotBackPoint`, from behind it), so the shot goes through the crosshair's
point wherever the muzzle stands: any range, any pose, any offset, the eye nudged. The hand's turn moves the muzzle the aim is
measured from, so the pose settles the two (8 passes) and hands the shot the muzzle of the hand's final turn (the view's
placed muzzle, kept in the hand's own frame: `notePlaced`, `keepMuzzle`; `hands::handMuzzle` only carries the move).
`vr_flat_aim_point 0` puts the old aim back, along the view. Three limits: a point nearer than 12 units past the muzzle (a wall
in his face, the gun inside a monster's box) is not pointed at, so no gun turns back on itself: the shot goes along the ray from
where the muzzle stands; a shot from inside a knight at 24 units is a shot from wherever the muzzle stands in him, and may
pass him; and a client not hosting has no world to trace (`vr_trace.hpp`), so its guns point far along the ray and a co-op client
keeps the muzzle's offset at near targets. Hand and Weapon Together (`whole_*`) corrects a controller's hold and does not apply
here (it moved and turned the hand after the shot was solved: 1.2 degrees off, on the shotgun).

**The ironsight's solve.** Each gun has a sight line (`sightalign::sightLine`: a rear and a front point on its model, found on
the painted sights or read from `vr_sightalign_table.inc`, the same ones Align Sights to My Aim and `vr_sight_check` read). The hand
is solved from it, with no number per weapon: `noteDrawn` takes the two points as the hand holds them, off the weapon as drawn,
and `sighted` turns that line by the least turn onto the crosshair's ray and slides it along until the rear sight stands
**Ironsight Eye Relief** (`vr_flat_ads_relief`) from the eye. So both sights lie on the ray at any field of view, any eye nudge and
any eye relief. `flatplay::drawnOffset` does it last, from the camera the frame is drawn with (`drawnSight`, its bob, kick and roll
in it): the looks are spent out over the travel, and the arm's hold does not apply to it, because the relief already keeps the
hand in the arm's reach (`reliefFor`: nearer than asked on a look straight up, further than asked where a gun's sights stand far
ahead of its hand). The game's hand stands at the same place, its muzzle on the drawn gun's, turned to shoot at what the crosshair
is on: the bore lies under the sights, so that turn differs from the drawn one a little, as much as the two lines differ at the
crosshair's point; the shot goes through the point whatever the sights do (`flatpose_test.sh` section f).

**Moving the picture.** **Ironsight Picture Right** and **Up** (`vr_flat_ads_screen_x`, `vr_flat_ads_screen_y`, percent of the
screen's height, 0 0: the centre) move the whole sight picture on the screen. Sights lined up through the eye can only stand on a
ray through the eye, so it is that ray that moves: the crosshair goes with it (`VR_FlatCrosshairShift`, `SCR_DrawCrosshair`), and so does
what the shots aim at (`crosshairRay`, through `flatsight::rayDir`). The look stays the view's. The sights stay lined up, on the ray
through the crosshair, and the shots through what it is on; the rows only change where on the screen all three stand.

A weapon with no sight line has no ironsight. The per-weapon `flat_ads_*` keys and the globals `vr_flat_ads_fwd`, `_up`, `_out` are
gone (their config lines are dropped quietly, `removedKeyLine`; a `tunables.cfg` line says it is left out). **Flatscreen Ironsight
With The Crosshair** (`vr_flat_ads_near`, 24) is the keep distance the sight uses, mixed out of `vr_flat_gun_near` by how far the
sight's travel has gone. `vr_flat_sight_check` (Debug > Reports) measures the weapon as drawn against the view it is drawn with;
`Misc/quakevr/flatsight_test.sh` reads it for every gun.

**Against the walls** (**Gun Against Walls**, `vr_flat_gun_collide`, on). A held weapon goes back along the way into the body until
it is out of the level (`pushBack`; `handpose::gunBack`, the headset's stop's points, traces and margin): towards its shoulder in
hipfire, along the ray in the ironsight, so the sights stay on it. It stops with the arm folded, or the rear sight 3 units from
the eye: past that, the gun's end enters the wall and the depth buffer hides it there, as it does past `vr_gun_wall_max` in a
headset. A wall met pushes it back at once, nearly; cleared, it comes out over a moment. It is the game's pose that moves, so the
gun aims from where it is pushed to and the shots leave from there. A blow is left alone: a hand throwing a punch is not stopped.
Models stop the drawn gun as in a headset (`vr_model_collide`, drawn only: the view runs it in this mode too).

**His eye on the screen** -- **View Forward** (`vr_flat_cam_fwd`, 0), **View Up** (`vr_flat_cam_up`, 0), **View Sideways**
(`vr_flat_cam_side`, 0), -20..20 units: where the flatscreen camera stands in the view's own frame of reference --
`+vr_flat_cam_fwd` along the look, `+vr_flat_cam_up` higher, `+vr_flat_cam_side` to **the view's own right**. The nudges go where
the mode already moves the view (`VR_FlatViewDrop`, at the end of `V_CalcRefdef` after Quake's bob, roll, kick and punchangle,
added on top of the crouch drop), and they move the camera, and the hands with it: his collision box and his server origin
stay where they are (origin 88.0 standing, 88.0 with Left Ctrl held; the drawn eye 28.0 above it with `vr_flat_cam_up 6`), while
the pose's hands carry the nudge, so the muzzle the shots go from is the one he sees, and it stands where the crosshair's ray
is read (`flatplay::crosshairRay`: the pose's head, its crouch in it, plus the nudge, along the aim; not `r_refdef`, a frame
behind the mouse at the move's pose). A nudged eye used to leave the shot parallel to a crosshair standing beside it: 36 units of
damage to a knight without the nudges, none with. Now the shot goes through the point: `flatpose_test.sh` section f.

`Misc/quakevr/flatpose_test.sh` is the measurement suite: the defaults, a gun pose, the sight's places at a knight on the
crosshair, the eye's nudges, the nudges held with the crouch keys, and section f: the server's own shot (`vr_debug_shots`' line)
against the crosshair's point (`flataim:`), for a moved sight, a nudged eye and hipfire, at a knight 64 and 300 away and the
far wall of `vrtesthall` (at most 0.09 units off the line, and every pellet count above 0 at the knights), and the control:
the moved sight with `vr_flat_aim_point 0` passes 26.4 units off the point and misses him.

**The off hand's axe in the suite's order.** `flatplay_test`'s off-hand axe case read 0 damage when it stood third in a run, and
was put down to two corpses at one spot. Measured on its own run on `origin/next` and on this: 0 in every order, alone included,
with the blow landing (`melee swish: blow, hand 0, 1.74`) in all of them. The difference is the read before the blow: with
`vr_test_hurt 1` three frames before it, 0 (75 -> 73, then 73 -> 72); with no read before it, the same blow lands
(`melee event: slash, hand 0 ... monster_knight`, 40.5 dealt). That read's own 1 unit of damage makes him react, and the blow
passes where he stood (that it is his flinch is read from that, not measured frame by frame). So the case has its own run, its
knight left alone until the blow, and one read after it against the 75 he starts with.

`vr_flat_pitch_up` (85) and `vr_flat_pitch_down` (85; saved settings, Movement > Locomotion > Desktop Screen Play): the flatscreen view's own window on the
pitch, in degrees from straight ahead, and both written as a plain positive number of degrees (0..90 â€” 0: he cannot look
that way at all, 90: as far as the plain game let him). Quake's pitch counts *down* as positive, so the view he is held
to is between `-vr_flat_pitch_up` and `+vr_flat_pitch_down`: at the defaults that is -85 to +85, and the last five degrees
at each end are the ones taken off. The clamp is the mode's own (`flatplay::pitchClamp`, `vr_flatplay.cpp`, through
`VR_FlatPitchClamp` at the point where the flatscreen view's angles are finished for the frame, `CL_AccumulateCmd` in
`Quake/cl_main.c`), so the keyboard's look keys, the mouse wheel, the mouse and a stick all come to the same edge, and it
comes after the engine's own clamp to `cl_maxpitch` / `cl_minpitch` â€” the window can only narrow what those already
allowed, never widen it. It asks for the mode first: outside flatscreen play it hands the angle back untouched, so the
plain game and a headset session are bounded by `cl_maxpitch` / `cl_minpitch` exactly as they were â€” measured on a plain
run, no `-flatscreenmode` and no backend either (`vr_backend none`), which pitches to -90 and +90 with these cvars at
their 85 defaults. `vr_flatplay_status` prints the window it is holding, in the same positive reading. Measured with
`r_pos` (which prints the view's angles every drawn frame, `Quake/gl_rmain.c`): holding `+lookdown` and then `+lookup`
the view stops at -85 and +85; with the cvars set to 0 and 20 on the console the same holds stop at 0 and +20; with both
at 90 they stop at -90 and +90. `Misc/quakevr/flatkeys_test.sh` section 6.

## The near plane

The plain desktop's near plane is up to 4 units (12 cm; `R_ViewMatrices`, `gl_rmain.c`), and the mode holds the VR body's hands
and weapons right at the camera: it cut them open and showed what stands behind through them. In the mode the scene is drawn as
a headset's eye is: at `vr_nearclip` (0.1), on the eyes' float depth with reversed Z, with `vr_nearclip_held` for what is nearer
still (`VR_FlatNearPlane`, `vr_stereo.cpp`; `VR_AliasNearEye`, `vr_render.cpp`). It is the one projection for everything,
the world too, so a model's shadows, outlines and overlays line up, and a gun against a wall reads the true depth as it always did
(no view model's depth range: it is hidden where it enters a wall, as in a headset). The world's plane moving loses it no precision:
a relative 2^-24 at any near plane and distance against the 24-bit depth's z/(4 * 2^24) at 4 units. The window's own framebuffer has
no float depth, so the mode always draws through the post-process's target (`GL_NeedsPostprocess`), one full-screen pass. No clip
control or `vr_depth_float 0` (read when those targets are next made: a new window size or `vid_fsaa`): the desktop's plane stays and only `vr_nearclip_held` applies.
`vr_flatplay_status` prints which. `flatpose_test.sh` section g.

## Tuning

Tuning constants (Debug > Tunables > Flatscreen, `tune`, `tunables.cfg`): `vr_flat_reach` (1), `vr_flat_pose_pitch` (65),
`vr_flat_arm_hold` (1), `vr_flat_gun_*`, `vr_flat_empty_*`, `vr_flat_support_*`; `vr_flat_ads_*`, `vr_flat_aim_fov` (12),
`vr_flat_aim_time` (0.25), `vr_flat_aim_point` (1); `vr_flat_cam_*`; `vr_flat_crouch_time` (0.25), `vr_flat_crouch_drop` (18);
`vr_flat_sway`, `vr_flat_bob`, `vr_flat_turnlag`, `vr_flat_turnlag_speed` (1). A release build has them as constants. Saved
settings (Movement > Locomotion > Desktop Screen Play): `vr_flat_crouch_fire` (0), `vr_flat_pitch_up`, `vr_flat_pitch_down` (85).
`vr_flat_arm_hold` is how far a drawn hand may sit from its arm's shoulder, times what that arm can hold it at (`armSpan`: the
model's bones with `vr_body_arm_length` and `vr_body_arm_stretch`, the shoulder's own `vr_body_shoulder_reach`; 0: no limit).
`vr_flat_pose_pitch` is how far the hands follow the view up and down, in degrees (90: all the way, as before).
What is left of the pose's own
numbers (how far a punch throws the fist, how long it takes, how the arm's hold is measured) are named constants at the top
of `vr_flatplay.cpp`, and the sight's (`vr_flatsight.cpp`).
