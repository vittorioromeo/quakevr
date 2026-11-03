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

- a held weapon sits out in front of the chest, on the centre line; an empty hand hangs at its side; the free hand
  rests near the other hand's gun;
- the hands are turned so the weapon in them shoots along the crosshair, whatever that weapon's Shot Pitch and Yaw
  (`aimRotAtCrosshair` inverts `weapons::shotAngles`); the drawn gun and its muzzle follow the hand, so the shot and the
  viewmodel agree;
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
the steady hand), so the shots and the aim stay on the crosshair whatever they are set to:

- **Move Sway** (`vr_flat_sway`, 1): each change of the player's velocity kicks the hands the other way and a spring
  settles them, a little overshoot: they lag as he starts and stops (~1.3 units), dip on a jump and on a landing. At a
  steady run they are where they rest;
- **Walk Bob** (`vr_flat_bob`, 1): a figure of eight on the ground, its size and rate with the speed (0.7 units across,
  0.6 down, a little roll at a run), and breathing at rest;
- **Turn Lag** (`vr_flat_turnlag`, 1) and **Turn Lag Recovery** (`vr_flat_turnlag_speed`, 1): "weapon inertia": the hands
  keep their direction as the view turns and a critically damped spring brings them back (18/s times the recovery).
  ~2 degrees behind at 180 degrees a second, at most 4 (softly), back within a quarter second. A view set in one frame
  (a teleporter, a respawn) leaves no lag.

These are plain (not saved) cvars, and they are in the Debug menu: **Debug > Reports > Flatscreen Play** â€” Status, Print
(`vr_flatplay_debug`: prints each E and V press, each hand's punch and where the hands are; 2: each frame the drawn
weapon in the camera's frame, `flatview:`, each move's shot against the view, `flatmove:`, and each drawn hand against
the arm the body poses for it, `flatarm:` â€” how far its wrist is off the body's wrist, how far the hand is past what the
arm can hold it at, its speed and its wrist's bend), Hand Reach, Move Sway, Walk Bob, Turn Lag, Turn Lag Recovery, Arm
Hold, Hand Pitch, Punch (`vr_flat_punch`). Test: `bash Misc/quakevr/flatplay_test.sh <agent>`.

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
| `V` | a punch with the right hand: a real motion of the hand (wound up, thrown, drawn back), so the game's own melee rules decide whether it lands. `vr_flat_punch [main\|off]` drives either hand's punch from the console. A weapon that no swing strikes with never punches from its fire button â€” the chainsaw's trigger runs its chain, a gun's fires (the mode's own weapon set: `punchesWith`, `vr_flatplay.cpp`); `V` is the hand's punch whatever it holds, as it was. A hand that already started a punch this very frame is joined by the other only when both hands hold a swinging weapon |
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
who crouches, and `crouchSpeedMax` slows him as it slows him there. Nothing is added to the move wire, and nothing new
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
game started without the flag, the mode never ran and never took the key. `vr_flat_crouch_fire 1` (not saved) skips the
whole exchange: Ctrl keeps `+attack`, so holding it crouches and fires as it does in the plain game.
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
the held hand (fwd -3.2, right 0.4, up -3.8 of it)".

The mouse wheel's button (`K_MOUSE3`), or `vr_flat_aim` from the console, turns the ironsight on: the held hand is turned
so that the muzzle's own point â€” the one the shot starts from (`muzzleNow`) â€” sits on the crosshair's ray
(`centreOnCrosshair`), and the field of view narrows by `vr_flat_aim_fov`: `gl_screen.c` asks `VR_FlatAdsFov` where it
chooses `r_refdef.basefov`, and the mouse look's own measure comes from the same `basefov` (`in_sdl.c`'s `IN_FovScale`),
so the look and the crosshair keep their feel. It refuses while dual wielding, and on a weapon with no sight line.
`vr_flatplay_status` prints both hands and the ironsight, and `vr_flatplay_debug 2` adds a `flatsight:` line each frame
with where the muzzle stands off the crosshair before and after the turn.

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

The pose's places used to be named constants at the top of `vr_flatplay.cpp`. They are cvars now, read in the view's own
frame and clamped at the read (`poseNum`: -30..30, `nearNum`: 8..60 units, `camOffMax`: 20), and all seventeen are rows on
**Debug > Reports > Flatscreen Play** so they can be driven from the menu while the game runs. `vr_flatplay_status` prints
the pose and the eye's place, and `vr_flatplay_debug 2` prints them every frame (`flatsight:`, `flatcam:`, `flatmove:`).

**The held weapon** â€” **Flatscreen Gun Forward** (`vr_flat_gun_fwd`, 17), **Flatscreen Gun Down** (`vr_flat_gun_down`, 9), **Flatscreen Gun Out**
(`vr_flat_gun_out`, 2.5): how far out in front of the chest the gun sits, how far below the chest, and how far to one side.
Forward is along the look; *down* is written as a positive number of units *below*, and *out* as a positive number of units
*away from the body's centre line* â€” the main hand's right, the off hand's left, which is the side `twohand` measures its
grips to. Each is scaled by `vr_flat_reach`, as the pose always was. **Flatscreen Gun With The Crosshair** (`vr_flat_gun_near`, 24, 8..60) is how
far the drawn view may stand from his head and still carry the gun with it; below that the gun is left on his body while the
view comes down, which is what a small `vr_flat_crouch_drop` with a small keep distance looks like.

**The empty hand** (`vr_flat_empty_fwd` 5, `vr_flat_empty_down` 13, `vr_flat_empty_out` 7) hangs at its side, and **the
free hand's rest** near the other hand's gun (`vr_flat_support_fwd` 12, `vr_flat_support_down` 11, `vr_flat_support_out` 5)
is where it waits before a grip takes it. Neither is a look bolted on: the empty hand still travels onto the held weapon's
two-hand hotspot (`pinToGrip`, `helpKind 1`) and `twohand::apply` still runs over the pose, so the two-handed grip, its
`helpKind` and its transition are the game's own whatever these are set to.

Measured on `Misc/quakevr/flatpose_test.sh`, at `setpos 480 -352 88 0 0 0` with the shotgun in hand: the muzzle stands
**6.38** units off the crosshair's ray with the ironsight off, and **10.48** with `vr_flat_gun_fwd 26 vr_flat_gun_down 4
vr_flat_gun_out 8 vr_flat_gun_near 14`. Either way **every shot still leaves along the crosshair's direction**: the
`flatmove:` lines say the shot is `0.0000` degrees off the view, over 71 records in that run (the harness gates it at 0.1
degrees, which is what the whole suite has never exceeded: 0.0000 over 230 records in the run that nudges everything).
`vr_flatplay_status` prints "a held weapon 26 4 8 â€¦ within 14 of his head" so the number he set is the number he reads.

**The ironsight's own set** â€” **Flatscreen Ironsight Forward** (`vr_flat_ads_fwd`, 0), **Flatscreen Ironsight Up** (`vr_flat_ads_up`, 0), **Flatscreen Ironsight Out**
(`vr_flat_ads_out`, 0) and **Flatscreen Ironsight With The Crosshair** (`vr_flat_ads_near`, 24): the same four numbers for the sight picture, so the
ironsight pose and the hip-fire pose are independent. They are written in the same view's frame and the same side
convention as the gun's, except that this set is called *up* and counts *up* as positive. They are put on the pose the mode
just made â€” before the muzzle is measured and the weapon brought onto the crosshair's ray â€” over `vr_flat_aim_time`'s
seconds, and with them at 0 the defaults reproduce the muzzle-on-the-ray numbers exactly. `vr_flat_ads_near` is the keep
distance the sight uses, mixed out of `vr_flat_gun_near` by how far the sight's travel has gone.

They go on *before* the centring, and that ordering is the whole story of these four. `centreOnCrosshair` measures the
muzzle and translates the held hand â€” and the grip the free hand holds and that hand with it â€” across the crosshair's ray
until the muzzle stands on it, and `aimRotAtCrosshair` has already made the shot leave along that ray whatever the hand
sits at: the two together are what puts a shot on the crosshair. Offsets applied *after* it would carry an already-centred
weapon aside, and the shot would leave parallel to the crosshair's line from a muzzle standing beside it. Measured that way
first, `vr_flat_ads_fwd 6 vr_flat_ads_up 3 vr_flat_ads_out -4` left the muzzle **4.81** units to one side of the ray (and
`vr_flat_ads_out 20` alone, **20.05**) and a knight standing 24 units down the crosshair took **none** of the shot, while
the same sight with the offsets at zero hit it for 36. Applied before it, the centring answers what the offsets did to the
muzzle: the same `6 3 -4` leaves the muzzle 1.40 units off the ray before the bringing and **0.27** after it, `vr_flat_ads_out
20` leaves it 25.26 off before and **0.27** after, and each of those shots hit the knight on the crosshair for 36. So at the
settled sight the parts of `vr_flat_ads_up` / `vr_flat_ads_out` that lie across the ray are answered by the centring â€” they
move the weapon on the way and leave where it ends up to the part that lies along it, `vr_flat_ads_fwd`, how far out from
him the sight picture sits. That is stated here rather than tuned away: with this mode's rule that the shot stays on the
crosshair, a settled sight cannot both stand several units to the side of the crosshair's ray and hit what the crosshair
points at.

**His eye on the screen** â€” **Flatscreen View Forward** (`vr_flat_cam_fwd`, 0), **Flatscreen View Up** (`vr_flat_cam_up`, 0), **Flatscreen View Sideways**
(`vr_flat_cam_side`, 0), -20..20 units: where the flatscreen camera stands in the view's own frame of reference â€”
`+vr_flat_cam_fwd` along the look, the way he is looking, `+vr_flat_cam_up` higher, `+vr_flat_cam_side` to **the view's own
right** (a look due east with no pitch is the case where the view's frame is the world's: there the three read as `+x`,
`+y` and `+z` in order, and the run's `flatcam: nudge 12.00 4.00 6.00` line â€” east, north, up â€” is those three in world
order, 12 forward, 4 to the view's right, 6 up). The nudges go
where the mode already moves the view (`VR_FlatViewDrop`, at the end of `V_CalcRefdef` after Quake's bob, roll, kick and
punchangle, added on top of the crouch drop), and they move the camera only: his collision box and his server origin stay
where they are, measured with the game's own readings â€” origin 88.0 standing, 88.0 with Left Ctrl held, 88.0 again when it
is let go, his eye 47.0 units above the bottom of his box standing and 29.0 held, while the drawn eye stood **28.0** above
his origin with `vr_flat_cam_up 6` and 10.0 with the crouch held on top of it.

Every aiming measure reads the same camera the screen is drawn from, which is the point of putting the nudges there:
`flatplay::crosshairRay` hands `centreOnCrosshair` â€” and so `muzzleNow` and the sight's turn â€” the drawn view's own origin
and direction (`r_refdef.vieworg`, `r_refdef.viewangles`) whenever the camera is his own. Measured with the three at 12 / 6
/ 4: the muzzle stands **10.09** units off the nudged camera's crosshair ray before the bringing and **0.27** after it, so
the sight closes onto the ray the crosshair is drawn on and not onto the head the camera was moved off of. What a shot does
to a test knight is a separate measurement, and the honest version of it is this: the knight 24 units down the line the
game aims along takes **36** units of damage from the shot with the nudges off, and none with them on â€” tried at
`vr_test_spawn_dist` 24, 40 and 64, and he was unhurt in all three. The spawner is the reason, not the aim: that knight is
stood on **the player's** axis, from the server origin along the yaw the game sees, while the nudges move the crosshair's
ray 12 forward, 6 up and 4 to the view's right off that same axis â€” at 24 units `vr_flat_cam_fwd 12` also carries the
muzzle inside the monster's own box (a knight's box is 32 units across, so it runs from about 8 to 40 units ahead of him),
and 6 up and 4 to the side move the line away from him. The suites therefore gate on the shot fired **before** the nudges
and print the one fired after them rather than pretend it hit; whether the drawn eye should also move where the server's
own shot starts is a question this increment does not answer.

`Misc/quakevr/flatpose_test.sh` is the measurement suite for all seventeen (`bash Misc/quakevr/flatpose_test.sh <agent>`:
five runs â€” the defaults, a gun pose, the sight's offsets at a knight on the crosshair, the eye's nudges at a knight on the
crosshair, and the nudges held with the crouch keys); `flatplay_test.sh` sections 10 to 13 gate the same ground more
briefly. Over those runs' 230 `flatmove:` records, the worst angle between the shot leaving the muzzle and the crosshair's
line is **0.0000** of a degree; the harnesses gate it at **0.1** of a degree, a tolerance the measurement never came near.


`vr_flat_pitch_up` (85) and `vr_flat_pitch_down` (85; console only, no menu row): the flatscreen view's own window on the
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

## Tuning

`vr_flat_reach` (1), `vr_flat_sway` (1), `vr_flat_bob` (1), `vr_flat_turnlag` (1), `vr_flat_turnlag_speed` (1),
`vr_flat_arm_hold` (1), `vr_flat_pose_pitch` (65), `vr_flat_aim_fov` (12) and `vr_flat_aim_time` (0.25) â€” all not saved,
all in the Debug menu, and `vr_flat_pitch_up` (85) with `vr_flat_pitch_down` (85) not saved too, at the console only.
`vr_flat_arm_hold` is how
far a drawn hand may sit from its arm's shoulder, times what that arm can hold a hand at (`armSpan`: the model's bones
with `vr_body_arm_length` and `vr_body_arm_stretch`, the shoulder's own `vr_body_shoulder_reach`; 0: no limit, the hands
where they are placed). `vr_flat_pose_pitch` is how far the hands follow the view up and down, in degrees (90: all the
way, as before). `vr_flat_crouch_time` (0.25), `vr_flat_crouch_drop` (18) and `vr_flat_crouch_fire` (0), also not saved,
are at the console, and `vr_flatplay_status` prints them with what the keys did â€” including which bind Left Ctrl holds. The
seventeen numbers of the pose and of the eye â€” `vr_flat_gun_fwd`, `vr_flat_gun_down`, `vr_flat_gun_out` and
`vr_flat_gun_near`; `vr_flat_empty_fwd`, `vr_flat_empty_down`, `vr_flat_empty_out`; `vr_flat_support_fwd`,
`vr_flat_support_down`, `vr_flat_support_out`; `vr_flat_ads_fwd`, `vr_flat_ads_up`, `vr_flat_ads_out` and
`vr_flat_ads_near`; `vr_flat_cam_fwd`, `vr_flat_cam_up`, `vr_flat_cam_side` â€” are plain (not saved) too, and rows on
**Debug > Reports > Flatscreen Play**, and `vr_flatplay_status` prints what they add up to. What is left of the pose's own
numbers (how far a punch throws the fist, how long it takes, how the arm's hold is measured) are named constants at the top
of `vr_flatplay.cpp`, and the sight's (`vr_flatsight.cpp`).
