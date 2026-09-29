# Testing in the headset

The first headset build. Everything below was checked on the desktop with the mock backend. OpenXR itself has
only been checked up to "no headset connected", so expect rough edges: tell me what you see and I will fix it.

## Build and install

The quickest way: `Windows\package-quakevr.ps1 -Build -Fteqcc <path to fteqcc64.exe>` builds everything into
`dist\QuakeVR` (and `dist\QuakeVR.zip`); copy its contents into your Quake folder and run `QuakeVR.bat`.
By hand:

1. Build `Windows/VisualStudio/ironwail.sln`, **Release | x64**. The output is
   `Windows/VisualStudio/Build-ironwail/bin/x64/Release/ironwail.exe`, with `openxr_loader.dll` copied next to it.
2. Build the progs: `QC/build.bat` (set `FTEQCC` to `fteqcc64.exe`). It writes `quakevr/progs.dat`.
3. Put the repository's `quakevr` folder in your Quake directory, next to `id1` (a directory junction works:
   `mklink /J <Quake>\quakevr C:\OHWorkspace\quakevr-iw\quakevr`). `hipnotic` and `rogue` are picked up
   automatically if they are installed.
4. Start the OpenXR runtime you want to use (Virtual Desktop, SteamVR or Oculus) and make it the active runtime.
5. Run:

   ```
   ironwail.exe -basedir <Quake> -game quakevr
   ```

`quakevr/quakevr.cfg` turns VR on (`vr_enabled 1`). The console reports `VR: started openxr backend` or the reason
it could not start; `vr_status` prints the tracking state and `vr_restart` restarts the session (after putting the
headset on, or switching runtimes). `vr_enabled 0` is flat-screen play.

## Controls

Controller buttons are Quake keys (issue #12), so everything can be rebound from the console or Ironwail's
bindings menu (press the controller button when asked for a key), aliases included. They reuse the gamepad key
names **by role**: the main hand is the gamepad's right half, the off hand its left half, so `vr_lefthanded 1`
needs no rebinding.

| Control | Main hand key | Off hand key | Default binding (main / off) |
|---|---|---|---|
| Trigger | `RTRIGGER` | `LTRIGGER` | `+attack` / `+offhandattack` |
| Grip | `RSHOULDER` | `LSHOULDER` | `+grabmain` / `+graboff` |
| A / X (primary) | `ABUTTON` | `XBUTTON` | `+jump` / `+reloadoff`; held in the air with the grappling hook in: its unreel (the key still pressed) |
| B / Y (secondary) | `BBUTTON` | `YBUTTON` | `impulse 10` / `impulse 12` (next weapon); held with the grappling hook out: its reel, whatever it is bound to |
| Stick click | `RTHUMB` | `LTHUMB` | `+reloadmain` / `+speed` |
| Stick | turn; up/down are `DPAD_UP`/`DPAD_DOWN` (`+moveup`/`+movedown`: swim) | move | |
| Menu button | Escape (not rebindable) | | |

Controllers without some of these: Index has no menu button, so its left B opens the menu. Vive wands use the
trackpads as sticks and their clicks as A/X, and the right menu button as B. WMR uses the trackpad clicks as A/X
and the right menu button as B. In menus both sticks navigate, A selects and B goes back.

The defaults live in `quakevr/vr_bindings.cfg`. They are applied once (`vr_bindings_version`) on top of whatever
config was saved before, including one inherited from `id1`; "Reset to defaults" applies them again.

Useful settings:

| Cvar | Default | |
|---|---|---|
| `vr_snap_turn` | 0 | degrees per snap; 0 = smooth turning at `vr_turn_speed` |
| `vr_controller_legacy_pose` | 1 | the hands follow the controller pose the old engine used (SteamVR's raw pose, rebuilt from OpenXR's grip pose for Touch/Quest and Index controllers), so the old tuned offsets line up; 0 uses the grip pose as is |
| `vr_gunangle`, `vr_offhandpitch` | 39.5, 40.25 | weapon pitch relative to the controller (the shipped values, tuned for that raw pose): Options > VR Settings > Gun Angle |
| `vr_world_scale` | 1.25 | |
| `vr_height_calibration`, `vr_floor_offset` | 1.646, -21 | |
| `vr_mirror` | 1 | desktop window: 0 off, 1 left eye, 2 both eyes |
| `vr_deadzone` | 25 | stick deadzone, percent |
| `vr_weapon_grip_mode` | 0 | 1 = weapons stay in the hand without holding the grip (issue #31) |

## Throwing: what changed

Reworked after the research in `docs/vr-port/THROWING.md`: how the throw is measured, how strong it comes out,
where the weapon starts, and how it flies. Most values are starting points: please tell me how they feel.

**Measuring the throw** (as Half-Life: Alyx):
- **Release point:** the release velocity is the controller's own velocity (from the runtime) where it was
  fastest, in a window around the moment you let go (`vr_throw_window` 0.12 s before, `vr_throw_lookahead`
  10 ms after), averaged over `vr_throw_peak_span` (17 ms) around that peak.
- **Frozen at release:** it is taken once, when you let go, on the headset's clock, so the network rate can no
  longer slide the window past the peak.
- **Wrist flicks:** a clear flick (spin above `vr_throw_ang_threshold`, 6 rad/s) adds 70% of the spin's
  velocity at the weapon's centre.

**Letting go** (`vr_throw_release 1`): the runtime's grip button lets go late. During a throw (hand faster than
`vr_throw_release_speed`, 1.5 m/s) the weapon now leaves the hand as soon as the grip eases 30% below its
firmest (`vr_throw_release_drop`), and it always does below 35% (`vr_throw_release_floor`). Grabbing needs 70%
(`vr_throw_grab_press`). **Tell me if weapons ever drop while you swing them without meaning to throw.**

**Strength:**
- **Gain:** slow movements (drops, passing a weapon between hands) are 1:1. Real throws get up to 1.5×
  (`vr_throw_gain_max`), rising smoothly from 1.5 to 6 m/s.
- **Weight:** heavy weapons are only slightly slower now (`vr_throw_weight_influence` 0.25; before, they were
  thrown at 40–60% of the hand's speed).
- **Two-handed:** two-handed throws are no longer 40% stronger (`vr_2h_throw_velocity_mult` 1).
- **Tuning:** if throws are still short, raise `vr_throw_gain_max` or lower `vr_throw_gravity`.

**Start position:** the weapon starts where it would be had it left the hand at the release point, not where the
hand followed through to.

**Flight** (the engine: Box3D, `vr_box3d.cpp`, since round 21 the only rigid-body physics):
- **Spin:** the hand's real spin, capped at `vr_throw_spin_max` (20 rad/s). It used to be applied as rates on
  each angle, which tumbled wildly.
- **Gravity:** the same true-scale gravity for the whole flight. It used to jump to Quake's 2.5 g after the first
  touch.
- **Bounces:** with `vr_throw_restitution` (0.25) and friction (`vr_throw_friction` 0.5).
- **Resting:** the weapon comes to rest on a side (its drawn shape) and stays still. No more wiggling.
- **Hit box:** a 6-unit hit box (`vr_throw_hitbox`) against monsters, so throws that look like hits are hits.

**Aim assist** (`vr_throw_assist 1`, off by default): bends a throw by up to 80% onto the best monster or
breakable within 12° of it, on the arc that reaches it. With `developer 1` it marks the target it picked.

**Debugging:** `vr_debug_throw 1` prints every throw's estimate; `2` also prints how long after the peak the release
came. `developer 1` prints the spawned velocity, gravity, spin and age.

## GitHub issues

| Issue | State in this port |
|---|---|
| #31 weapons should stick to hands | `vr_weapon_grip_mode 1`: grip, let go and the weapon stays; grip again and open the hand to throw it (or holster it) |
| #53 stuck on steps | not reproduced in a quick test (walking off and back up the steps behind the start.bsp spawn, flat and mock VR): please try the E1M1 spot from the issue |
| #37 force-grabbed BSP items become solid | the QC never makes items solid, and the new engine never blocks on `SOLID_NOT_BUT_TOUCHABLE`: please confirm in the headset |
| #34 weapon knockback too strong | the rendered hand follows the weapon's firing animation: please check how it feels |
| #67 / #40 hands shaking (frame cap, after a break) | OpenVR pose-timing problems; OpenXR predicts poses for the displayed frame: please confirm |
| #64, #70, #19, #49, #52 | old engine/renderer (SteamVR keyboard, lighting, animated textures, mission-pack launch, black screen): gone with Ironwail |
| #12 missing bindings | done: the controller buttons are Quake keys (see Controls) |
| #20, #14 status bar on the hands | done: on the off hand by default (see HUD below) |

## Voice notes while playing

Raise your off hand to your mouth, like a radio, and hold **Y** to talk; let go to save. "REC" and the
note's length show low in your view, and the hand buzzes as a note starts and ends. Away from your mouth
(more than about 30 cm) Y does what it always did; a note shorter than 0.8 s is taken for an accidental press
and dropped. Each note is saved in `quakevr/notes/` with a screenshot and where you were (map,
position, view, health, what each hand held). The microphone is Virtual Desktop's
(`vr_note_device "Virtual Desktop"`; `vr_note_devices` lists them); Gameplay > Voice Notes turns it off.
`+vr_note` records from a bound key too.

To turn the notes into text (Whisper, on your PC; the model is already downloaded):

    python Misc/quakevr/transcribe_notes.py

It transcribes the new notes and writes `quakevr/notes/NOTES.md`, every note with its transcript,
context and screenshot, ready to paste or to point me at.

## What to try

- **New in this round** (details in `docs/vr-port/ROUND21.md`; each section ends with an "In the headset" list):
  - **Punches land at once; the empty hammer is quiet** (ROUND21.md, same title): punch damage used to come 0.1-0.45 s
    after the hit (a bug: every punch waited as a pommel strike does); now in the frame of contact. Pommel and butt
    strikes wait 0.05 s for the blade (Melee Settings > Pommel Strike Wait; 0.1 before). Mjolnir with no cells no
    longer clicks when you squeeze the trigger.
  - **Precise hit detection (models, not boxes)** (ROUND21.md, same title): shots, nails, rockets, grenades, the
    grappling hook, melee blows and thrown things hit a monster (or a corpse, or the dummy) where its model is drawn,
    grown by a few units, not anywhere in Quake's big box round it: shots past a grunt's head or through a shambler's
    box corners fly on to the wall; the hook takes hold only on the body. Headshots and arm shots come from where the
    model was hit. Combat > Damage and Knockback > Hit Detection: Precise Hit Detection (off: the boxes, as before) and
    each class's tolerance (guns 4, hook 2, melee 6, thrown 2 units). Debug > Views > Show Hits draws each hit on the
    model (and the misses through a box: Hits and Misses).
  - **Hand grenades from the back pouch** (ROUND21.md, same title): with rockets, reach behind the small of your back
    with an empty hand (either) and grip: a grenade from the pouch on your belt there (a tap as the hand arrives; the
    pouch lights up), in your palm. Pull the trigger to pull its pin (a ping, the fizz, sparks, ticks faster and
    faster): 2.5 s, then it goes off as the grenade launcher's (120 damage over 160 units, yours, with Quad), or on a
    monster it hits. Throw it as anything you carry. Changed your mind? Let go of it at the pouch before pulling the
    pin: it goes back in, and the rocket with it (a rocket leaves your ammo while a grenade is in your hand). Let go
    of elsewhere unarmed, it is a dud at your feet: take it again to arm it or put it back. No rockets: a dull knock.
    Batting and Catching > **Hand Grenades** (on/off, **Arm Hand Grenades**: the trigger, or when let go of; **Hand
    Grenade Fuse**); the pouch's place under it (**Grenade Pouch**), and its turn on Hip Holsters > **Grenade Pouch**.
    Under it too, **Grenade In Hand Pitch/Yaw/Roll**: how a grenade from the pouch is turned in your palm (a held one
    turns as you drag them). A health box, ammo box or power-up you carry, let go of at the pouch, is taken as at a
    holster.
  - **Hands: both work; props through teleporters; climbing stamina** (ROUND21.md, same title): a hand that force
    grabbed something and put it down could no longer take a ledge (fixed); a main-hand grip on a thing the off hand
    touched did nothing, and a prop held in both hands lost a hand when you moved fast (both fixed). Bricks (whole,
    chipped, broken) can be held in both hands. What you carry comes through teleporters. Hanging from a hold tires
    you (Climbing page > Climbing Stamina; also on the Stamina page): 5 a second from one hand, 2 from both, nothing
    with your feet on something; at none your hands let go (or, with Exhausted: Slip Time, slip off after sinking);
    the gadget reads HANGING while it drains. `vr_debug_hands 1` (Debug > Logging > Hands) prints each hand's state when it changes.
  - **Performance fixes (review, 2026-09-28)** (ROUND21.md, same title): the spectator camera has a **Frame Rate**
    (60 fps by default: as often as a 60 fps recording takes), a **Resolution Scale** of 0.75 by default and an
    **Anti-Aliasing** choice (Recording page); climbing's mantle and lenient grab, the props' settings and two caches
    are cheaper or fixed with the same results; no GPU-sampling thread runs unless you profile (Debug > Profiling and
    Memory > Memory Log: GPU keeps it on for the Memory Log).
  - **Profiling: where the time goes** (ROUND21.md, same title): VR Settings > Advanced VR Options > Debug >
    Profiling and Memory. Profiler Panel (In Front) shows each system's milliseconds a frame with a bar against the budget; CSV
    Capture writes a row a second while on; the Hitch Log names what took a slow frame's time. See "Profiling" below.
    Also found with it: the foveated rendering's setup waited for the driver each eye (0.2-0.4 ms of the CPU a frame,
    fixed; the pictures are the same).
  - **Recording: smoothed mirror and spectator camera** (ROUND21.md, same title): VR Settings > Body and Display >
    Recording (Window View). **Smoothed Mirror** steadies the window's left-eye view (free); **Spectator Camera**
    draws the game a third time for the window, from your head, steadied, 90 degrees wide (about one more eye's cost at
    1080p). Record the window with OBS (Window Capture, "Windows 10 (1903 and up)", the window sized to the video,
    1920 x 1080); the OBSMirror layer can't capture this OpenGL game. Try Smoothing, Level Horizon, and the spectator's
    Resolution Scale while watching the headset's frame rate.
  - **Grappling hook: rope, reel on demand, props and monsters** (ROUND21.md, same title): the hook bites and the rope
    just holds you at its length (swing on it, walk closer; nothing pulls). Hold that hand's **B** (right) or **Y**
    (left) with the trigger to reel in; let go and the rope keeps its length. Walls and ceilings, heavy props (100 kg
    and more) and huge monsters (shamblers, bosses) pull you to them; props come to the gun (light ones fast, heavy
    ones slowly: the explosive box at a fifth) and hang there for the other hand to take; power-ups fly in and are
    yours; small monsters (dogs, grunts, knights, zombies, scrags) come fast and staggered, medium ones (ogres, hell
    knights, fiends, vores) slowly and not. A slack rope hangs. Letting go of the trigger drops it, in flight too.
    Advanced VR Options > Game > **Grappling Hook** (speeds, the classes' masses, stagger, stamina, haptics; Rope: Pulls
    at once for the mission pack's old pull). Test it: `impulse 9` gives it (`impulse 151`/`171` put it in the main /
    off hand).
  - **Grapple: unreel; rope drawn in one piece** (ROUND21.md, same title): in the air, hold the grapple hand's **A**
    (right) or **X** (left) to let the rope out (Unreel Speed, 300 u/s). Hanging, you are let down; a monster can walk
    away; a prop hanging at the gun is lowered. On the ground A just jumps (Unreel Button Only When Airborne); a press
    that jumped doesn't unreel until you press again. A short sagging rope is now one smooth chain, with no gaps
    between straight links.
  - **Menu: scroll memory and shortcuts** (ROUND21.md, same title): every VR Settings page reopens where you left it
    (the selected row and the scroll), after Back to Game, after going back and coming again, and after a restart.
    Weapon Offsets keeps the same row for another weapon. Under Back to Game, top left on every menu: **Advanced VR**
    (the Advanced VR Options) and **Levels** (the level list). Point and pull the trigger; or click a stick (or go up
    from a page's first setting), then up and down, A to press, B to go back to the page. B after a jump walks up
    the menus as always (Advanced VR Options, then VR Settings, then Options; Levels, then Single Player).
  - **Wall torches you can take** (ROUND21.md, same title): grip a wall torch and pull it out (or force grab it); it
    is a burning club that lights the room round you as the wall torch did (same colour and brightness, and it casts
    shadows). Its blows burn monsters; after 5 blows, or dropped, its fire dies in 6 s; held, it burns for ever; a dead
    one lights again in another torch's flame. Carrying and Gibs > **Wall Torches**; its grip and fingers on **Held
    Object Offsets** (hold it, open the page).
  - **Rocks and bricks** (ROUND21.md, same title): loose rocks on natural ground (grass, dirt, rock) and at the foot of
    rock and stone walls, bricks at the foot of brick walls, placed at map load where the textures say, more in
    corners, the same places every load (e1m2, e2m2, e4m2, e1m1's outdoor ground). Pick one up, punch with it (a rock
    or a brick hits harder the heavier it is: Held Object Weights' Melee Damage and the weight's curve), throw it (it hurts what it hits), force grab
    it (a whole brick is a club, held by its end); they knock and clack as they land. Carrying and Gibs > Rocks and Bricks: on/off, Rocks, Bricks, Chance, In
    Corners, In the Dark, Most Together, Most in a Map, Most in an Area, Spacing, Size Variation, Layout. Single player.
    Also fixed: a punch holding a box hit 2.25x, not Box Punch Damage's 1.5x.
  - **Deflection by blows and bashes; catching grenades; ogre aim** (ROUND21.md, same title): a swing or a blow bats
    a monster's spike, laser, spit, ball, grenade or flesh off the weapon's face as a bat hits a ball (across: off to
    the side; the face driven at the thrower: back at him, faster the harder); a bash or an armed shove sends it the
    way you push. Parry, Bash and Headbutt > Batting Projectiles: **Batting Bounce** (0.6), **Batting Aim Assist**
    (0.5). Ogres' grenades bounce and roll as physics; catch them (grip, or force grab even in flight), they fizz and
    tick (**Held Grenade Fuse** 2.5 s, **Fuse Resets Every Catch** off; Carrying page, Grenades; **Catch Grenades**
    also takes your launcher's with "Ogres' and yours"), throw them back at the ogre; held too long, they go off in
    your hand. Ogres (and zombies) now lob at your height (Gameplay > Monsters, **Ogres Aim Grenades Up and Down**).
  - **Swimming: air supply; strokes against the palm** (Swimming page; ROUND21.md, same title): a backhand (palm
    facing you, the hand pushed away) or a hand swept back-first to reposition now pushes a quarter as much as a real
    stroke (**Stroke Against Palm**, 0.25; 1 is the old swimming); palm strokes are unchanged. `vr_swim_debug 1` shows
    each stroke's `palm lead` (+1 palm first, -1 back first). **Air Supply** (1.5): 18 s under water before drowning
    starts instead of 12.
  - **Stamina on the gadget; the glow** (ROUND21.md, "Stamina on the gadget; the glow"): with Parry Stamina on, the
    bar over the weapon is gone; the wrist gadget's top row shows STAMINA and ten cells instead. In the firing range,
    turn on DUMMY ATTACKS and parry: each one-handed parry takes three cells; the cells blink when one more would
    knock the sword away; EXHAUSTED (and the screen's frame) blinks when none is left; a sweep runs along the empty
    cells while it comes back. Each parry turns the label into COUNTER over a bar that runs out with the window.
    Gameplay > Parry, Bash and Headbutt > Stamina on the Gadget turns it off (also HUD and Menus > Screens > Stamina and
    Counters). **Counter Window Glow** (Counter-Attacks; now off as shipped, and your saved "on" is turned off once):
    turn it on to try it: the weapon glows gold at its edges and sheds embers until the window closes.
  - **Dynamic wounds, burns and wetness** (Gore page > Wounds on Models; ROUND21.md "Dynamic wounds, burns and
    wetness"): shoot a grunt a few times, blow up an ogre, shove a monster into lava or slime, let a grunt shoot you,
    wade in water: blood where each blow landed, scorches, char with embers, wet and drying; a health pack washes
    your blood off. Your body and hands no longer use the wound skins (Dynamic Wounds off: as before).
  - **Enemies Hurt by Liquids** (Gameplay > Damage, on; ROUND21.md, "Enemies hurt by liquids; holster orientation"):
    shove a monster into slime or lava: it burns as you would (lava fast, with smoke; slime slowly). Fish, bosses and
    Hephaestus are immune; zombies burn up in lava.
  - **Holster orientation** (Hotspots: Shoulder / Hip / Upper Pitch, Yaw, Roll): turn each pair of holsters and the
    guns in them; the left mirrors the right. Draw and holster as before.
  - **Holster draw blend** (ROUND21.md, "Holster draw blend; holster defaults; body calibration kept"):
    - A gun drawn from a holster turns smoothly into your hand, the shortest way, over Draw Blend Time. A gun you
      holster settles into the holster over Holster Blend Time. Both are on Weapons > Immersion: 0.3 s each, 0 for at
      once. The gun can fire at once.
    - Your holstered poses are now everyone's defaults.
    - Your settings are saved when the menu closes and when you Apply a body calibration. Before, a game stopped from
      the debugger kept none of them.
  - **Per-weapon holstered pose** (Weapon Offsets > Holstered; ROUND21.md, "Per-weapon holstered pose"): hold a
    weapon, open its page, pick Hip, Upper (Chest) or Shoulder (Back) and move and turn it with the six sliders: while a
    Holstered setting is chosen, the weapon you hold is drawn in both holsters of that kind, so look down (or at the body
    preview) as you tune. Each kind of holster has its own pose; the left mirrors the right. Draw and holster as before.
  - **Items as physics pickups** (ROUND21.md, "Items as physics pickups; sinking; spinning shapes"): grip a hanging
    weapon (it is yours at once) or knock it with an open hand (it falls); grip or force-grab a key, the biosuit or a
    rune and let go of it at a hip or shoulder holster to take it (a key you have knocks and drops); powerups as
    before. Carrying and Gibs > Armour and Pickups > Weapons and Keys turns it off. In the firing range the weapons on
    the tables should lie on them, none cut by the table top; Show Physics Shapes: hanging items' outlines turn with them.
  - **Dummy attacks, for parry practice** (ROUND21.md, "Dummy attacks (firing range)"): in the firing range, press
    DUMMY ATTACKS (the panel south of the training dummy). Stand in front of it: every 2.5 s or so it winds up (a
    sound, a glow, the rifle raised) and strikes you as a knight would. Parry it: the parry, parry stamina and
    counters work as in a fight, and your counter's readout shows on the dummy. Off at every map load. Settings:
    Gameplay > Parry, Bash and Headbutt > Training Dummy Attacks (time between blows, randomness, wind-up, reach,
    damage). Your motion takes are unaffected: replays turn it off. A take recorded with it on says so, and its
    replays have the dummy strike at the same moments.
  - **After the posing test** (ROUND21.md, "After the posing test"): hold a gun into a monster's head and fire (a
    headshot now); while posing, the hand passes through the weapon (the solved grip shows for 1.5 s after A/X);
    Weapon Offsets > Tuning Aids: Show Controller is drawn in your palm as a Quest 3 controller, **Controller Preview**
    sliders line it up with your real one; **Shot Pitch / Shot Yaw** (under Muzzle and Posing Mode) turn where shots go
    without moving the gun: with Show Controller Laser, put the red line through the sights.
  - **Physics: Box3D only** (ROUND21.md, "Simplification: Box3D only, knights always drop swords"): the Physics
    Engine option is gone (Throwing and Physics > Physics starts at Bounciness), and an old config's
    `vr_physics_engine` line loads without a word. Thrown and dropped things collide with each other: stack boxes,
    build a pyramid, throw a box into a stack, sweep one off with a held box; throws at monsters, weapons landing on
    their sides, backpacks and armour on slopes, boxes on lifts, things floating, gibs.
  - **Knights always drop their sword** (the Knights Drop Swords slider is gone; Advanced VR Options > Gameplay > Knights' Swords keeps
    Sword Damage): every knight and hell knight you kill drops it, gibbed or not; a statue knight has none.
  - **Melee, redesigned:** swings in any direction (backswings too), stabs with the tip, pommel/butt strikes with
    the near end, parry bash = hold the stance ~0.5 s then push; palm shoves (both palms harder). Melee Speed was
    reset once to 4 m/s. Tell which of your motions still misread, and record more takes of them.
  - **Melee fixes** (ROUND21.md, "Melee fixes: flashlight, axe on walls, gibs"): punch with the torch in your fist
    (both grips: as hard as a gripped fist); shove with the free palm while the torch hand pushes along (a two-handed
    shove); the torch hand pushed alone, palm first, does nothing. Chop a wall with the axe, Mjolnir, a sword or a
    gun, sideways, diagonally and from overhead: the wall's thunk and buzz. Punch or chop a gib lying on the floor:
    it bursts.
  - **Stamina for shoves and strikes; thrown damage on gibs** (ROUND21.md, that section): in the firing range, shove
    and strike the dummy without pausing (DUMMY ATTACKS on to parry too) and watch the gadget's top row drain: a
    two-handed shove takes 20 of 100, a one-handed one 15, a weapon's blow 8 (two-handed 6), a punch 4, a parry 30
    as before. With nothing left, the dummy's readout says "exhausted" and the damage (and a shove's push) is half.
    Stop for 2 s: it comes back. Gameplay > Parry, Bash and Headbutt > Shove and Strike Stamina has the switches and
    costs. Throw the axe, a sword, a box or a gib at a gib lying on the floor: it bursts.
  - **Motion recorder:** Advanced VR Options > Motion Recorder; keep adding takes, especially of what misreads.
  - **Review Takes** (under Motion Recorder): the takes that fail the evaluation or are suspect (To Review). In the
    firing range, pick one: Play Ghost replays it in front of the dummy (translucent weapons, their lines, the tip's
    trail, the events), then Keep, Discard (into `motions/discarded/`) or Relabel it; Undo Last takes any of them
    back. Re-evaluate This Take / Re-evaluate Shown run the evaluation in a second copy of the game in the background
    (a small window appears; your headset view is untouched). Tell whether the ghost reads well in the headset, and
    whether a re-evaluation drops frames.
  - **Weapons stop at monsters and things:** a gun, a sword or a fist pushed into a monster, a corpse or a box on the
    ground stops at the model as drawn (not its box), the hand and arm with it; past 20 cm it gives way (Hand/Gun
    Calibration > Against Monsters and Things). Hits are unchanged: they come from your hand.
  - **Fitted hands:** fingers wrap guns, blades and objects; recoil moves the hand; two-handed grips steady (no
    jitter); the trigger finger pulls; hotspots (Weapon Offsets) incl. the Cup pistol grip; Inherit From for the
    alternate models. Check the thumb on pistol grips and objects held from far away (no more floating).
  - **Hand tuning (Weapon Offsets):** Tuning Aids (Show Controller, Show Controller Laser), Hand and Weapon Together
    (moves the aim too), Hand Only (the bent wrist), per hotspot Held Hand, Overlap per weapon and per hotspot,
    Fingers: Manual per weapon and per hotspot. A cup hotspot is now where your palm goes (yours was moved once).
    Take the super nailgun a few times: the same grip each time.
  - **Flashlight:** hanging from the belt (off-hand side; reach for it, it taps and brightens), B/Y away from a gun
    flips the grip (a quick spin now), B/Y at the head wears it; it should never be grabbed by a guard or a punch.
    After your test: the overhead grip sits in the fist (it went through the hand); each grip has its own sliders
    (Flashlight page, In the Hand: Low Grip / Overhead Grip); the beam is white by default (Beam Hue, Beam
    Saturation); wider offset sliders; Cord off hides the cord.
  - **Wrist gadget:** Screens > Messages (test button, messages only on the gadget), Graphics > Performance > FPS
    Counter on the Gadget (Off / Basic / Detailed: now, average, min/max, late frames, graphs).
  - **Gadget on the forearm:** bending the wrist no longer turns it (body off: at all; body on: only as far as the
    drawn forearm turns, which Body > Wrist Limits sets); rolling the hand turns it with the forearm.
  - **Torch from the belt:** always in the overhead grip; off the head or a gun, the grip nearer its beam.
  - **Gadget model:** olive straps all round the forearm, fitted to the bracer of your build; a seamed casing with
    screws, buttons, an antenna and the hologram's emitter.
  - **Casings** splash in water; **beam quality** (Flashlight section).
  - **Two-handed props:** grip what one hand carries with the other to hold it in both: it moves and turns with both
    hands, and letting go of both together throws it (tumbling as your hands turned it). Let go of one and the other
    keeps it: that's how to pass it between hands. Only a one-handed carry goes into the pack at a holster.
    Carrying and Gibs: Two-Handed Carrying, Two-Handed Hand Drift.

- **Previous round** (details in `docs/vr-port/ROUND20.md`, your seventh batch of notes; `ROUND19.md`, the
  performance review):
  - **Your settings are the defaults** (graphics too), and your weapon placements (applied once to slots 1, 2, 5,
    6, 7, 9, 10, 17 and the alternates 13 to 15: check they are where you left them).
  - **Menus:** the right stick only scrolls; no Quake plaque on the tall panel; Force Grab Saturation. (Round 20's
    Weapon Only X/Y/Z and per-weapon finger openness were replaced in round 21 by fitted hands: `ROUND21.md`.)
  - **Hologram messages:** "You need the gold keycard" and the like over the wrist gadget (e4m1's keys, secrets).
  - **Armour:** grip a pickup, let go over your torso to wear it; a worse one drops with a knock and a double buzz.
  - **Flashlight on a gun:** take it from the chest to the gun in the other hand, press B or Y; off on `map`, still on
    across level changes.
  - **Melee:** diagonal two-handed cuts, slow cuts and stabs should now be blade hits, not bashes or shoves; a shove
    needs open palms pushed hard; a bash or shove with a gun bats projectiles back (firing range grunts, ogres).
  - **Physics:** a fist moved onto a gib no longer grabs it; a backpack in a corner or at water's edge rests; a held
    health box sits against the hand.
  - **Firing range:** 13 more monster buttons on the other side of the range.
  - **Guns:** the nailgun's grip; no see-through faces (super nailgun, lava ones, launchers, rocket launcher); lava
    nailguns glow; the normal/lava morph; ammo screens only on the gun in hand; the flash stays big in two hands;
    lightning gun sights in your hue; holstered guns never invisible.
  - **Water:** bigger ripples in the waves (Graphics - Liquids sliders), recorded splash sounds.
  - **Lava** lights the walls once you rerun the relight; barrels explode with a light.
  - **Body:** hip holsters follow the thighs; kicking legs when wading and swimming.
  - **Graphics, Headset page:** *Upscaling* FSR or NIS (try Render Scale 0.8 with FSR), *Foveated Rendering*
    (Balanced: look for shimmer at the edges of the view). *Ambient Occlusion* (Graphics - Shadows): dark contact
    under monsters and round lifts.
  - **Performance** (round 19): the world pass 40–62% cheaper indoors, the CPU frame ~25% lower; the memory log has
    GPU columns (clocks, use, the encoder, each program's share) for the slowdown.

- **Previous round** (details in `docs/vr-port/ROUND17.md` and `ROUND18.md`):
  - **Graphics** (Graphics page, each with a switch): real light directions, detail textures up close, directional
    ambient and a rim light on models, sheen anti-aliasing, fence coverage, tone mapping and dither and colour grades,
    soft particles, shoreline foam and heat haze, reflections on your weapons, flickering torch lights.
  - **Gore** (Advanced > Gore): sprays on walls, gibs stuck to ceilings dripping, pools under corpses, your wounds
    marking the floor.
  - **Water:** big modern splashes and ripples that move the water.
  - **Weapons:** a sword blade grip (off hand near the tip), bash from a level weapon, new bash/parry/shove sounds;
    the carried gun's button; shotgun port, pump grooves and flames; dark super shotgun; the alternate models match.
  - **Colours:** Wrist Gadget > Colours > Player Effects Hue drives every effect of yours (force grab too).
  - **Performance:** your last log shows the slowdown in SteamVR's submit, not the game: try the VDXR runtime.

- **Previous round** (details in `docs/vr-port/ROUND16.md`, your sixth batch of notes):
  - **Hand-off:** let go of a two-handed sword and the other hand keeps it; let go of a gun's grip and it hangs from
    the foregrip hand (grip its handle again to take it back). VR Settings > Weapons > Two-Handed Hand-Off.
  - **Parry:** any weapon held level across in front of you (Gameplay > Parry and Bash: Parry Angle, Parry Reach).
  - **Weapon models:** every gun has a real grip now: a proper sawn-off; pistol grips on the shotgun, rocket,
    grenade, proximity and multi-grenade launchers and the lightning gun; a spade grip on the laser cannon; trigger
    guards; the super nailgun's grooves. Their hand placements reset once.
  - **Lights:** lava nails glow and light the room; the lightning beam lights its whole length.
  - **Iron sight hue** (Wrist Gadget > Colours, or Graphics); flick-reload shells fall in front; bigger waves.
  - **Corpses** tougher (big monsters more); the laser cannon stops when empty.
  - **Memory Log** now also times our CPU and GPU work and the runtime's waits: note the time when it feels slower.

- **Previous round** (details in `docs/vr-port/ROUND15.md`, your fifth batch of notes):
  - **Swords:** hits along the whole blade (the dummy names the point), two-handed grip below the main hand, both
    swords in vrfiringrange.
  - **Batting projectiles:** a brisk, slightly early swing; Gameplay > Feel has Batting Reach, Swing Speed, Timing.
  - **Corpses** gib from shots, nails and blows. **Shell casings** from the shotgun and the double shotgun's reload.
  - **Water:** splashes and sounds (shots, throws, jumps, hands, wading, strokes); real waves on liquids.
  - **Holsters** on the front of the body; a longer holster buzz; a buzz when catching a force-grabbed thing.
  - **Lights:** fixtures light their rooms (relit maps); map boards are CRT screens; no lines on bumpy walls up close.
  - **Memory Log** (Graphics > Performance): `quakevr/profile/memstats_<date>.csv`, for the slowdown.
  - **Your tuned settings** ship as defaults (`quakevr/vr_defaults.cfg`); `vr_savedefaults` rewrites it.

- **Previous round** (details in `docs/vr-port/ROUND14.md`, your fourth batch of notes):
  - **Melee from scratch:** strong swings and straight punches rewarded (the dummy shows speed, acceleration and
    strength); a bash needs a still, level guard first; waving does nothing.
  - **Render Scale fixed** (SteamVR's OpenGL path ignores swapchain resizes: the eyes are now rendered at the
    scale and resampled into fixed-size images).
  - **Menus:** Back to game (top-left, or hold the menu button), reopen where you left, right stick scrolls;
    live preview keeps the game running on settings pages.
  - **Water:** the HUD, menu and wrist log no longer wobble underwater; no halos round things over water.
  - **Swimming:** strokes judged whole, an intent threshold and stroke memory; a new Swimming page of knobs;
    `vr_swim_debug 1` shows each stroke.
  - **Models:** bumps under their own light; model parallax off by default. **Screens:** glowing text.

- **Previous round** (`docs/vr-port/ROUND13.md`, your third batch of notes):
  - **VR Settings > Headset:** OpenXR Runtime (try Virtual Desktop's VDXR), Render Scale, Hide Lens Corners.
  - **Menus:** bigger, spaced, modern widgets; point with the laser, trigger to click and drag.
  - **Flashlight:** a real spotlight: lights models, casts shadows, no flicker.
  - **Liquids:** waves, glints, refraction, caustics; fog, tint and a gentle wobble under water.
  - **Parallax:** items and models with their own depths; no black boxes.
  - **Ammo screens:** light only in front; CRT look.
  - **Melee:** no whips or backward pulls; hit a gib held in the other hand; shove monsters off ledges.
  - **Swimming:** the reverse stroke swims backwards; floating objects settle.
  - **Ledge grab (experimental):** Locomotion > Ledge Grab.
  - **Tutorial map** relit.

- **Previous round** (`docs/vr-port/ROUND12.md`, your second batch of notes):
  - **Melee:** real blows only (no flicks or wiggles), punches/slaps/overheads balanced, one-hand palm shove.
  - **Parallax:** walls with depth (Graphics: Parallax, Depth, Distance).
  - **Flashlight:** held right, shadows on, a soft beam of light.
  - **Effects:** Quake VR particles for lava balls, rockets, grenades and projectiles; dented bullet holes;
    gibs burst in a mist of blood when shot or thrown hard.
  - **Gadget:** a CRT screen in one colour, a directed light, a soft glow; ammo screens tighter and glowing.
  - **Firing range** relit; the shotgun's sights like the double shotgun's.
  - **Graphics > Performance Profile** to record a profile.

- **Previous round** (`docs/vr-port/ROUND11.md`, from your voice notes):
  - **Carrying:** held things stay in the hand when you move or turn; backpacks go to a holster; the force grab
    beam hits the middle of things; gibs stay on the floor.
  - **Chest flashlight:** trigger near your chest toggles it, grip takes it, let go and it springs back.
  - **Lights:** monster projectiles, ammo screens and the wrist gadget give light; the shotgun's sights glow;
    bumps show in the map's own light; glowing buttons tint their rooms.
  - **Melee and swimming:** a shove is one shove; swim where you look, recovery strokes don't pull you back.
  - **Body:** slower legs that step round when you turn; new sword hilts.
  - **Wrist log:** messages above the gadget; damage numbers readable against the sky.
  - **Profiling:** `vr_profile 1` (see Profiling below).

- **Previous round** (`docs/vr-port/ROUND10.md`):
  - **The DarkPlaces look:** darker shade, flat model lighting, strong coloured flashes and explosions, a sheen
    and bumps under dynamic lights, smooth QRP textures, bloom stronger on coloured lights and weaker on white
    (and on brightly lit maps). Every part has a switch on the Graphics page.
  - **See-through water** in the relit maps.
  - **Legs** (Body: Full body): they step at a natural rate now, not spinning at full speed (Advanced > Body >
    Step Rate, 2.2 steps a second running); turning on the spot (stick or for real), the feet stay planted until
    you have turned about 40 degrees (Turn Before Stepping), then take a step or two round. The legs and boots
    are a little sturdier.
  - **Leaning:** walk or lean up to a wall or railing: your head gets close and over it before the body follows
    (Locomotion: Lean, Lean Recentre).
  - **Training dummy** in vrfiringrange: every hit's damage, kind and body part in the console and as a floating
    number.
  - **Held boxes** keep up when you move; **knockback** has a base and a setting per source; **thrown boxes and
    gibs** hurt less (and only when fast); gibs by hand only; **heads** can be picked up; gibs bleed and splat;
    wounded arms drip; a meatier **headshot** sound.

- **Previous round** (`docs/vr-port/ROUND9.md`):
  - **Boxes:** a held box turns with your hand about where you hold it; it is grabbed only when your hand is
    on it; bounces turn the right way; it comes to rest flat on the floor.
  - **Swords:** the grip is centred on the blade, thicker and square in section.
  - **Bloom** is subtler by default, with fine slider steps; **headbutts** are easier to land.
  - **Shotgun held by the middle:** not reproduced; please describe when it happens.

- **Previous round** (`docs/vr-port/ROUND8.md`):
  - **Unarmed parry:** cross your arms in an X in front of you as a blow lands.
  - **Bash:** in a guard (a weapon held across in front, or both hands together, crossed or not), drive forward
    hard: little damage, the monster is thrown back and staggers. Also a two-handed shove with empty hands.
  - **Force grab:** what you point at glows softly; a faint beam when aiming, a crackling energy tendril when
    locked on and while it flies to you, with a sparkle trail. (Force Grab: Outline, Effects.)
  - **Decals:** blood pools and spatter, gib blood trails, scorch marks, bullet chips (Graphics: Decals).
  - **Gibs and heads:** pick them up, throw them (they hurt), force-grab them.
  - **Pickup sparkles** are faint and slow.

- **Previous round** (`docs/vr-port/ROUND7.md`):
  - **The look:** darker rooms lit by their lamps; your shots light the room up (coloured by the weapon);
    lamps, buttons and panels glow (bloom), and glowing textures light the walls round them (your relit maps
    were re-made). Graphics: Light Contrast, Bloom, Muzzle Flash Light, Explosion Light, Coloured Lights;
    "Off (Quake)" restores Quake's look.
  - **Gameplay page** (Advanced VR Options > Gameplay): damage multipliers (to enemies, to you, self),
    headshot/arm/leg multipliers, the headshot sound (now a clear crack), push-back, feel options, headbutt,
    knights' swords.
  - **Push-back:** your melee blows and headbutts push monsters (bodies too), their blows push you, parries
    push both apart; heavy shots shove monsters and killing blows throw the bodies.
  - **Headbutt:** lunge your head at a monster (towards where you look).
  - **Bat back projectiles:** swing a weapon or fist through a spike, laser, spit or grenade. Please try.
  - **Haptics:** hits felt on the side they come from, explosions rumble, a heartbeat at low health.
  - **Knights' swords:** a grip and pommel, the knight's own look, held like the axe.
  - **Boxes:** grabbed only when your hand touches them; put in your pack by letting go at a holster (hip or
    shoulder); the trigger is an option (Throwing and Physics > Take a Box). Their shadows are their size.
  - **Swimming:** the stick is 20% under water; strokes push more where the stick points.
  - **Blob shadows** are named so, and Auto (off where real shadows fall) by default.

- **Previous round** (`docs/vr-port/ROUND6.md`):
  - **Carrying boxes:** grip an ammo or health box to hold it, pull the trigger to take it, let go to throw it
    (thrown hard it hurts). A hand or gun touching a box without gripping nudges it. While a hand holds a box it
    cannot take a weapon; punching with a box in hand hits harder. Advanced > Throwing and Physics.
  - **Knights drop their swords** (knights and hell knights, gibbed or not): pick one up for a melee weapon with
    more reach and damage than the axe; dead knights no longer hold theirs. Advanced > Melee.
  - **Parrying:** hold a weapon sideways in front of you to block a monster's melee blow: less damage, a clang,
    sparks, your arm is knocked; one-handed, it may be knocked out of your hand. Advanced > Melee.
  - **Swimming:** wading is a little slower; under water or off the bottom the stick barely moves you, and
    swimming strokes (palm first) do. Advanced > Locomotion.
  - **Pauldrons** on your shoulders and the **ranger's clothes** on the body (olive vest, belt, camouflage
    trousers with thigh plates, tall boots). Pauldron style, size, position and how much they follow the arm:
    Advanced > Body.
  - **Headshots** measured properly whatever way the monster faces, with a quiet tick
    (Advanced > Gameplay, Headshot Sound). The tick never played before (its sound was not loaded).
  - **Ammo screen** behind each weapon's ammo counter (Advanced > HUD).
  - **Weapons, keys, armour and powerups** float at torso height.
  - **Swing sounds** are back, except when the hand moves down (reaching for a holster).
  - **Shoulder position** (Body: Shoulders Back/Up/Width); the **body is hidden while dead**; the **green armour**
    on the body matches the pickup's colour; **ammo and health boxes** have shadows; floating items **splash**
    only when they hit the water fast; a force-grabbed box missed near a wall no longer falls out of the level.

- **Previous round:**
  - **Real-time shadows and dynamic lights** (`docs/vr-port/LIGHTING.md`; Advanced > Graphical Settings > Lights
    and Shadows, with a Preset from Off to Ultra; defaults are Medium):
    - explosions and rockets cast shadows (and stop lighting through walls);
    - the map lights near you cast the shadows of monsters and of you (body and hands) onto the floor and walls;
    - dynamic lights light models per pixel, by angle, shadowed.
    - `vr_light_test` puts a light in front of you; `vr_shadow_stats 1` prints the cost each second. Please tell me
      the frame timing on your headset at Medium and Ultra, and any shadow speckles or light leaks you see.
  - **Graphics** (`docs/vr-port/GRAPHICS.md`, "Done"; Advanced > Graphical Settings):
    - **Re-lit maps:** softer shadows, ambient occlusion in corners, some bounced light, coloured light. Made on
      your machine by `Misc/quakevr/relight_maps.py` (already run for you); Relit Maps off compares.
    - **Model lighting:** monsters, items, weapons, hands and body are shaded from the map's lights.
    - **Shadows under monsters and items**, away from their light.
    - **The muzzle flash lights up your gun**, not a point in front of your chest.
    - **Anti-aliasing** setting (4x for new configs; yours is off, `vid_fsaa 4` to try).
  - **Crouching** keeps the hips under you and tilts the back forward (Advanced > Body: Crouch Tilt), instead of
    pulling the torso back.
  - **Bloody hands:** the hands and fingers get bloodier with the arms.
  - **Quad damage arcs** reshape every frame, with more of them and now and then a longer one.
  - **Grappling hook rope and lightning beam** start at the gun as drawn, every frame (they trailed behind it at
    the server's rate); the rope no longer twists randomly.
  - **Reloading:** a gun's swing makes no whoosh unless it hits something (the whoosh was what you heard when
    reaching down to reload), and a downward swing now ignores walls and floors from a shallower angle.
  - **Other mods in VR:** `-game quakevr -game <mod>` runs another mod's progs in VR (compatibility mode): you
    aim with your hand, its weapons fire from your gun, you move by your head, walk the room and teleport. No
    off-hand weapons, holsters or hand pickups there yet. See `docs/vr-port/MODS.md`; please try a mod you like.
  - **Particles:** your old textured particle system is back (smoke, sparks, blood, explosions, force grab and
    pickup sparkles), for Quake's own impacts and explosions too. Advanced > Particles: Quake VR Particles,
    Particle Multiplier. `vr_particle_test <0..11>` spawns one in front of you.
  - **Body state:** your torso shows the armour you wear (green, yellow, red plates) and your arms get bloodier as
    you are hurt; quad damage sparks around your hands and forearms, the pentagram makes you glow red, the ring
    fades you. Advanced > Body: Show Armour and Wounds, Show Powerups.
  - **Wrist gadget options:** Advanced > Wrist Gadget: arm, size, position and rotation, casing tint, screen colour.
  - **Default Speed: Run/Walk** in VR Settings (the speed button switches to the other). The stick now runs at the
    old speed (it walked at half of it) and moves as fast in every direction.
  - **Force grab:** one object at a time per hand (let go of the trigger before pulling another).
  - **Backpacks** no longer spin (their model's rotate flag).
  - **Reloading** (reaching down to a hip holster) no longer hits the floor or a wall as a melee swing.
  - **Old port features back:** VR actions in Options > Key Setup; the status bar shows the ammo count; grenade
    trails; roomscale jump (`vr_roomscale_jump`); a click in the controller for menu presses; Advanced > Play (hub,
    tutorial, firing range, bots); desktop keys for the VR actions after "Reset to defaults".
  - **Settings cleanup:** 15 settings that did nothing, the old floating torso (Body "Torso" is now the body with
    arms) and the old throw algorithms are gone. Fixed: the bloodlust toggle was inverted, drop chances were
    applied twice, the hub's Torso/HUD/Shadows buttons.

- **Previous round:**
  - **Force grab** (rewritten, like Half-Life: Alyx): point an empty, open hand at a weapon, backpack, ammo or
    health box (or, in single player, a weapon lying in the level). It sparkles. Pull the trigger to lock on,
    then flick your hand back or up. It flies to your hand in an arc and arrives in about half a second. Close
    your hand (grip) as it arrives to catch it; too early or too late and it drops at your feet. It flies through
    walls, so it cannot get stuck. Tuning: VR Settings > Advanced VR Options > Force Grab. `developer 1` prints
    each pull, catch and miss.
  - **Melee:** any swing faster than `vr_melee_speed` (3 m/s) hits once, whatever its direction; damage grows
    with speed, and punches (knuckles first) do 25% more (`vr_melee_punch_mult`). Tell me if weak swings still
    hit, or real punches don't.
  - **Advanced VR Options** (bottom of VR Settings): every settings page, in groups: Game (Play, Combat, Movement,
    Carrying and Throwing, World, Gore), Body and Weapons (Body, Flashlight, Weapons), Display (HUD and Menus,
    Graphics), Playtesting (Motion Recorder, Review Takes, Debug); a group of many pages is a page of links. At most
    three levels below VR Settings (ROUND21.md, "Menus reorganized"). `menu_vr <n>` opens a page directly (through
    the pages above it, so Back goes up the tree); `menu_vr list` prints the numbers and where each page is.
  - **Debug menu; quad sound; grenade catch default; no empty-hand deflection** (ROUND21.md, same title):
    - Advanced VR Options > **Debug** has Voice Notes and six pages: Views (Show Ledges, physics shapes, hand bones, grab
      test, skeleton, collisions, foveation, entity boxes), Logging (Developer Messages first: some logs need it),
      Profiling and Memory, Reports (buttons printing `vr_status`, the weights, the ledges ahead... to the console and
      the wrist log), Tools (rebuild the ledge map, reload models; debug images; test effects) and Tests (a monster or
      box ahead, a projectile at you, an ogre's throw; god mode, Quad, all weapons). Every debug setting of the console
      is there but the automated tests' (`vr_mock_*`, `vr_fixed_frames`, `vr_window_log`...).
    - Quad Damage's sound plays when you hurt something (a blow landing, a bash) or fire a gun, not when you squeeze
      the trigger holding a sword, a fist or a prop.
    - Catch Grenades is "Ogres' and yours" by default (your config's "Ogres'" moves to it once). Two copies of the game
      open at once (one from TrenchBroom) no longer undo each other's settings when they quit.
    - Only a weapon in your hand bats projectiles back (a gun, a melee weapon, or a club: a wall torch, a brick, a
      rock): swing or bash. Empty hands never do; a grenade meeting an empty hand is caught (grip closed or closing) or
      flies on.
  - **Posture:** VR Settings > Torso Offset and Legs Offset move the torso and the feet back (or forward)
    separately.
  - **Wrist gadget:** now over the back of the forearm, and it reads like a watch: raise your forearm across your
    chest, and the text runs towards your fingers.
  - **Ammo and health boxes** are small (`vr_forcegrabbable_box_scale` 0.25), their touch box is the box you see,
    and they can be force-grabbed.
  - **Backpacks** now come to rest instead of spinning on the ground. `impulse 243` (single player) drops a
    backpack of your ammo in front of you to try it.
  - **Hands:** the wrist end of the hand model is tapered, so it stays inside the bracer.

- **Previous round:**
  - **Wrist gadget:** the HUD is now a device strapped over the back of your off-hand forearm. Raise your forearm
    across your chest, like reading a watch. VR Settings > HUD switches back to the status bar.
  - **Body:** VR Settings > Build picks the body: Lean, Athletic or Brawny. Full body (VR Settings > Body) walks
    as you move (`vr_body_walk`).
  - **Pickups:** weapons, armour, powerups and keys are smaller and lie on the floor (`vr_pickup_scale` 0.6 in
    `quakevr.cfg`), so you crouch to take them.
  - **Physics:** thrown weapons and backpacks are real rigid bodies. `vr_physics_list` prints their state.
  - **Near clipping:** things close to your face are no longer cut away (`vr_nearclip`).

- **Body** (new): the old floating torso is replaced by a body whose arms reach your hands and which crouches and
  leans with your head (Options > VR Settings > Body: Off / Torso / Torso and arms / Full body). To see the whole
  pose, `vr_body_debug 2` (facing you) or `3` (from the side) shows a copy in front of you. Things to tell me:
  - where the elbows go when you aim, reload or reach behind you;
  - whether looking down at your chest feels right (`vr_body_torso_back`, metres the torso sits behind your neck);
  - whether crouching looks right (`vr_body_crouch_tilt`, degrees the back tilts forward in a full crouch).

  The tuning cvars are listed in `docs/vr-port/IK.md`. Holsters and the virtual stock now follow the body when you
  crouch or lean; `vr_body_anchors 0` restores the old placement for comparison.

- **Walking around the room** moves you in the game (with collision), as in the old engine
  (`vr_roomscale_move_mult`).
- **Holsters:** bring a hand to a hip, the chest or a shoulder. The holster lights up while hovered; let go of a
  weapon there to holster it, grip there to draw. Bringing both hands together passes a weapon between them.
- **Body collisions** (VR menu > Body > Body Collisions; ROUND21.md, "Body collisions"): push a hand into your other
  forearm, your chest, the wrist gadget or the other hand, and sweep a held gun through your other arm. It should stop
  at the surface and your arm follow it; push on (about 70% of the way through) and it slides through, and stays
  through until it is clear. Try the two-handed grips, a prop in both hands and the holsters: none should be blocked.
- **Two-handed aiming:** with a gun in one hand, grip its foregrip with the other (empty) hand: the hand snaps
  onto the gun. Weapons trail the hand a little depending on their weight (`vr_wpn_pos_weight`,
  `vr_wpn_dir_weight`); hands and barrels stop at walls. With a hand
  near the shoulder, the virtual stock steadies the aim (`vr_2h_mode`, `vr_virtual_stock_thresh`).
- **Flick reload:** with the super shotgun, flick the wrist to snap it open (`vr_spinreload_x_angular_threshold`).
- **Teleport:** `vr_teleport_enabled 1` and bind a button, e.g. `bind LTHUMB +teleport`. Aim with the off hand,
  release on a blue spot.
- **Fingers** curl with the trigger (index), the grip (middle to pinky) and the thumb resting on a button or stick.
- **Movement:** `vr_movement_mode 0` moves where the off hand points instead of the head; in both modes, pointing
  the off hand up or down while pushing forward swims up or down.

- **HUD:** the status bar is on the off hand (Options > VR Settings > Status Bar for the main hand); centre
  prints and messages float in front of you. Each weapon shows its ammo (and clip) on the weapon itself.
- **Crosshair:** Options > VR Settings > Crosshair: a dot, a laser or a soft laser from each muzzle.
- **VR Settings:** Options > VR Settings (or `menu_vr`) has the tuning pages (Weapon Offsets and Weights, Held
  Object Offsets and Weights, Hand/Gun and Body Calibration), the comfort and weapon settings, and Body and Display
  and Headset pages;
  the sticks move and change, A selects, B goes back. "Set Height Now" calibrates the height while standing.
  The buttons at the top left of every menu: Back to game, Advanced VR, Levels (the laser; or a stick's click).

`vr_status` shows tracking, hand angles, hotspots and grab and two-handed state; `vr_dumpview` shows the drawn
hands, weapons and finger curls.

## Profiling (CPU and GPU time per effect)

**Where the time goes, while you play** (ROUND21.md, "Profiling: where the time goes"): VR Settings > Advanced VR
Options > Debug > Profiling and Memory.

- **Profiler Panel**: *In Front* floats a table a metre ahead (it turns after you when you look 30 degrees away);
  *Over the Wrist* puts it above the wrist gadget's hand. Each line is one of the game's systems (Box3D, QuakeC, the
  world's drawing, the shadow maps, waiting for the headset...): its milliseconds a frame over the last second, its
  worst frame, and a bar against the frame's budget (the whole bar: one refresh, 11.1 ms at 90 Hz). Blue: the CPU's
  work; grey: waiting (not work); gold: the GPU's; red: one system over the whole budget by itself. The top lines:
  the frame rate, the CPU's work ("busy": without the waits), the GPU's, and each group's total.
- **CSV Capture**: turn it on, play what feels slow, turn it off. Each second is a row of
  `quakevr/profile/systems_<date>_<time>.csv` (a column per system, its average and worst frame, the GPU's, the
  counts), for a spreadsheet. Send me that file.
- **Hitch Log** (Over 1.5 Frames by default): while the panel or a capture is on, every frame that takes longer goes
  to the console and `quakevr/profile/hitches_<date>_<time>.csv`, with what took the time: the systems, and the
  three costliest scopes by name (e.g. `screen/3D/eye L/scene/vr opaque (text3d)/decals 423.3` for the first decal).
- **Print Report**: the last 5 seconds' table in the console (`vr_profile_report [seconds]`; with `-condebug` it is
  in `qconsole.log` too).
- **Detail**: *Every Trace and Builtin* also times each collision trace and each QuakeC builtin call apart (dearer).

In the console: `vr_profile_overlay 1` / `2`, `vr_profile_csv 1` / `0` (or `vr_profile_csv_toggle`, to bind to a key),
`vr_profile_hitch 1.5`, `vr_profile_detail 2`, `vr_profile_gpu 4` (the GPU's times on one frame in 4: each timer query
stalls the GPU a little; 1 every frame, 0 none). All are off again after a restart.

**The call tree:** `vr_profile 1` (in the console) times each part of every frame, on the CPU and on the GPU (OpenGL
timer queries, read a few frames later, on one frame in `vr_profile_gpu`), per eye. Every
`vr_profile_interval` seconds (5; 0: only on demand) and on `vr_profile_dump` it writes the averages and the
worst frame of each part to `quakevr/profile/profile_<map>_<date>_<time>.csv` (one file per map, one block of rows
per interval; the header lines give the map, the eye resolution, the graphics preset and the graphics settings)
and prints a one-line summary with the costliest parts. `vr_profile_dump` also prints the whole tree.
`vr_profile 2` also shows the profiler's panel over the wrist gadget. `vr_profile 0` (the default) stops it.

To send me a profile: play a while with `vr_profile 1` in the same spot and settings (the start of E1M1, a big
fight, ...), then `vr_profile_dump`, and send the `.csv` (and `qconsole.log` with `-condebug`). Comparing the
presets (`vr_graphics_preset 1` .. `4`, a profile each) shows what each effect costs.

Reading it: `frame` is the engine's frame on the CPU (`xr wait`, the headset's pacing, and `swap` are waiting, not
work: "CPU busy" leaves them out). Its GPU time spans the frame on the GPU's clock, including time the GPU sits idle
while the CPU waits in the runtime (`xr submit`: xrEndFrame paces the frame), so the eyes' own GPU time (the
summary's "eyes") is the real load; `frame period` is the time
between frames. Under `screen/3D`, `eye L` and `eye R` hold each eye's `scene` (`world+brush`, `alias` models,
`particles`, `sky`, `water`, `translucent`, Quake VR's `decals`, `blob shadows` and `vr particles`), `bloom`,
`postprocess`, the `hud panel` and the `mirror` to the window; the shadow maps (`dlight shadows`, `map light
shadows`) are drawn once, in the left eye's `setup view`. `self` columns leave out the parts inside a part.

**If it gets slower the longer you play:** `vr_memstats` (in the console) prints the GPU's memory (NVIDIA: used by
all programs, and how often the driver had to move things out of it: "evictions"), the game's RAM, its textures and
every live OpenGL object, and the average frame time since the last `vr_memstats`. Type it at the start, again after
each map load, and send the lines (with `-condebug`, they are in `qconsole.log`): if the game's counts stay the same
while the frame rate drops, the game is not leaking, and the slowdown is in SteamVR / Virtual Desktop (the profile's
`xr submit` and `xr acquire` growing while the eyes do not says the same). To tell for sure once it has slowed
down: quit and restart only Quake VR (same map): if the frame rate is back, it is the game; if it is not until
SteamVR (or Virtual Desktop) is restarted too, it is them. The Memory Log (`vr_memstats_log`, on by default: a row a
minute in `quakevr/profile/memstats_<date>.csv`) also times each frame whatever `vr_profile` is: our CPU work
(`busy_ms`) and the eyes' GPU time (`gpu_eyes_ms`) next to the runtime's waits (`xr_waitframe_ms`, `xr_submit_ms`,
`gpu_submit_ms`) and missed refreshes (`slow_frames`), with counts of what there is to draw (corpses, thrown weapons,
decals, lights, particles). Note the time when it feels slower and send that file (ROUND16.md, "Slowdown"). Its GPU
columns (clocks, slowdowns, each program's use of the GPU: `gpu_*`, `gpu3d_*`, `gpu_programs`) come from a sampling
thread that runs only while profiling (`vr_profile`, the Profiler Panel or its CSV Capture) or with
`vr_memstats_log_gpu 1` (Debug > Profiling and Memory > Memory Log: GPU); otherwise they are empty. With `developer 1` the console says when
that thread starts and stops (`gpustats: sampling thread started`).

## If something goes wrong

Add `-condebug` to the command line (or `QuakeVR.bat -condebug`): the console goes to `qconsole.log` in the Quake folder, which
is the most useful thing to send me along with a description. In particular:

- **Nothing in the headset:** look for the `VR:` lines. They say which OpenXR call failed, with its result code.
  `vr_restart` retries after the headset is on or the runtime is running.
- **The picture is wrong** (double vision, wrong scale, swimming): `vr_status` output while it happens, and a
  screenshot of the desktop mirror (`vr_mirror 2` shows both eyes).
- **Hands or weapons are in the wrong place or at the wrong angle:** `vr_status` and `vr_dumpview` while holding the
  pose. Gun Angle in VR Settings is the first thing to adjust.
- **Fingers wrong on something held** (through it, or stuck open): `vr_debug_grasp 1` prints each grasp solve;
  `vr_grasp_dump main hand.obj` writes the drawn hand and the held model as an .obj to send me. Hand/Gun
  Calibration > Fit Fingers to What You Hold off shows the controller's curls alone, Jointed Hand off the old
  hands.
- **A crash:** the log up to the crash, and what you were doing.

## Testing without a headset

`vr_backend mock; vr_enabled 1` runs everything with a pretend headset. `vr_mock_button <main|off> <trigger|grip|primary|secondary|stickclick|menu> <0|1>`,
`vr_mock_stick <main|off> <x> <y>` `vr_mock_hand <main|off|head> <x> <y> <z> [<pitch> <yaw> <roll>]` and `vr_mock_look <pitch> <yaw>` drive it; `vr_mock_swing <period>`
swings the main hand for throwing tests. `vr_mock_shake <degrees>` shakes the head (quick small turns at 5-13 Hz and
4 mm a degree of position wobble) and `vr_mock_shake_turn <degrees/s>` turns it slowly under that, timed from when the
shake starts (the same poses every run with `vr_fixed_frames 1`); `vr_window_log 1` prints the head's and the window
camera's angles each frame (Smoothed Mirror, Spectator Camera: ROUND21.md, "Recording"). The window's `screenshot` is
the window's view; `vr_eyeshot 1` the eyes'. `vr_particle_seed <n>` (not 0) makes the particles the same in every run (their random numbers restart from it at each map), for comparing images. `vr_mock_fingers <main|off> <trigger> <grip> [<thumb>]` sets the finger
sensors (0..1); a fifth argument sets the index finger's touch on the trigger.
Fast test runs (ROUND21.md, "Faster tests and startup"): `vr_mock_fast 1` runs the mock headset's frames as fast as
the machine makes them while the game's clock is fixed (`vr_fixed_frames 1`; `vr_motion_play` and `vr_motion_eval`
not in watch mode): no frame cap, no vsync, every frame 1/72 s of game time as before, so the same frames, poses and
numbers in less time (`wait600`: 6.7 s at `host_maxfps 90`, 1.7 s fast). Frames on the real clock (no
`vr_fixed_frames`) keep their pace, and so does a take played in watch mode. The kit's `run.sh` sets it for every run
(`-RealTime` leaves it off: a test that needs the wall clock with fixed frames, or measuring the frame cap). A `wait`
waits for a server frame: on the real clock that is 1/72 s whatever the frame rate, so `wait600` is 8.3 s there;
with `vr_fixed_frames 1` every frame is one. Sound (off in the kit) plays at the wall clock's pace, ahead of the game.
`vr_walltime [label]` prints the wall clock and the process's CPU time since the last call; `vr_startup_times` the
start-up's stages and the last map load's (with the work summed across them: model loads, image decoding, normal
maps, uploads); `vr_normalmap_cache 2` makes the cached skin normal maps anyway and warns of any that differ.
`quakevr/cache/` (the skin normal maps, `prefetch.txt`) is made again when removed: the first start after that is
about 0.4 s slower. A timing that compares builds must run each one twice (the second run's caches are warm).
Profiler (ROUND21.md, "Profiling: where the time goes"): `vr_profile_csv 1` collects (a row a second) without the
panel; `vr_profile_report [s]` prints the table; the hitch log prints `vr_profile: hitch` lines. The panel is UI, so
`vr_eyeshot` misses it: to see it in a screenshot, `vr_window_view 2; vr_spectator_fov 50; vr_spectator_scale 2;
vr_spectator_rate 1; vr_profile_overlay 2` (the spectator camera, narrow, frames the panel in the 960 x 540 window).
The spectator camera's own settings: `vr_spectator_rate` (1 every frame, 2 or 3 every 2nd or 3rd, more than 3 at most
that many images a second; the window pass shows the last image between: `window view` outside `spectator` in the
profile) and `vr_spectator_aa` (1 the window's MSAA, `vid_fsaa`; 0 none). Its timings need a real window size: the
kit's run.sh opens 960 x 540 (`vid_width` and `vid_restart` don't change it); the scratchpad's `spectator/runwh.sh
-W 1920 -H 1080` does. Timings need
`run.sh --exclusive`; the mock's frame cap (`host_maxfps` 250) sleeps in 15.6 ms steps in an exclusive run (Windows'
timer), which the report shows as "frame cap" (idle), not work.

Menus (ROUND21.md, "Menu: scroll memory and shortcuts"): `menu_vr pos` prints the menu shown and, on a VR page, its
selected row (with the header above it), its scroll and the page Back goes to. `menu_vr list`: every page's number
and place in the tree. `menu_vr dump` prints every page reached from VR Settings and its rows (MDPAGE/MDROW lines);
`python Misc/quakevr/menu_coverage.py before.log after.log` compares two dumps (every setting and action still on a
page, the tree, pages over 30 rows; ROUND21.md, "Menus reorganized"). Page numbers: 13 Grappling Hook, 23 Weapon
Offsets, 41 Held Object Offsets, 42 Weapon Weights, 43 Held Object Weights, 44 and on the pages added then, 65 Recording. `vr_mock_laser back|advanced|levels`
(or `<x> <y>` in menu coordinates; `off`) puts the main hand's laser on a corner button or a spot, whatever the hand's
pose; then `vr_mock_button main trigger 1` / `0` clicks. `vr_mock_button off stickclick 1` / `0` gives the corner
buttons the selection, `vr_mock_stick off 0 -1` (then `0 0`) moves down, `vr_mock_button main primary` presses;
`vr_mock_stick main 0 -1` scrolls a page. Back to Game: `vr_mock_button main menu 1; wait90; vr_mock_button main menu
0`, reopened by `togglemenu`. Across a restart: `writeconfig <file>` writes `vr_menu_positions`; exec that line at the
next start (the kit puts `ironwail.cfg` back after each run).
Climbing (ROUND21.md, "Climbing with both hands"): the mock's grip button does not press the grab. Script
`+graboff`/`-graboff` and `+grabmain`/`-grabmain` (in a `vr_mock_play` file: `<t> cmd +graboff`). Map `vrclimb` has a rung
wall (`setpos 71 0 24 0 0 0; noclip`, the second toggling setpos's noclip off) and a long ledge over a trench
(`setpos 78 176 24 0 0 0; noclip`). `python Misc/quakevr/climb/climb_plays.py ladder|ledge|mantle|e1m1` writes the
plays (hand over hand to the top, a two-hand hang and a shimmy with a fall, a mantle, and e1m1 from `setpos 250 2350
40`). With `vr_climb 1; vr_climb_debug 2`, `climb_trace.py qconsole.log` prints the body's move against the hands'
pull every frame and at every hand-off. `vr_climb_probe [yaw]` lists the holds ahead.
Stamina and tired arms (ROUND21.md, "Tired arms: shaking and heavy hands"): `vr_stamina_set <0..1>` puts the game's
stamina there, `vr_debug_stamina_hold 1` keeps it; `vr_debug_fatigue 1|2` prints the shake (2: each frame's aim,
muzzle and drawn weapon, `fatigueaim`); `vr_fatigue_shake_always 1` shakes off a hold too.
Swimming (ROUND21.md, "Swimming: air supply; strokes against the palm"): `python Misc/quakevr/swim/swim_plays.py`
writes `vr_mock_play` files: `strokes_main.txt` / `strokes_off.txt` (one stroke per case from rest: a palm-first pull,
a backhand, back-first sweeps square and at 45 degrees, an edge-first slice, a palm-first sweep; each announced by an
`echo STROKE`), `cycle_edge.txt` / `cycle_backhand.txt` (6 s of hands in turn, `viewpos` at the start and the end),
`intent.txt` (a pull after a backhand) and `air.txt` (under water at Air Supply 1, 1.5, 2, 3). The pool: `map
vrfiringrange; sv_gravity 0; setpos 612 474 -180 0 0 0; noclip` (370 for the cycles, which swim towards +x), and
`vr_swim_stick_speed 0.001; vr_mock_stick off 0 1` so that SV_WaterMove's idle sink (60 units a second, with no
stick) does not take you out of the pool's bottom: the player falls through it (below -360, anywhere in it).
`vr_swim_debug 1` prints each stroke. Mock hand poses: pitch -20 points the hand down (Gun Angle 70); yaw 90 (main)
or -90 (off) turns the palm back towards the body. For the air, nothing prints a drowning hit: add a temporary
`bprint` after `T_Damage` in `WaterMove` for the run (as for ROUND21.md's table).
Climbing, hand placement and grab leniency (ROUND21.md, "Climbing: hand placement and grab leniency"):
`vr_fixed_frames 1` makes every frame 1/72 s of game time with a server frame, so a scripted climb logs the same
numbers every run (compare two settings' `climbtrace` lines with `diff`). `vr_climb_try <x> <y> <z> [off|main] [<vx>
<vy> <vz>]` prints what a grip at that world point would take (exact or lenient, the hold, its distance, the holds turned
down and why, the search's time) and takes nothing; `python Misc/quakevr/climb/climb_leniency.py script` writes the
sweep (ledge, rungs, a wall, stairs, the thin wall with a ledge behind it at vrclimb's `setpos -270 -260 24 0 180 0`,
at leniencies 0 to 30 cm) and `climb_leniency.py table qconsole.log` tabulates it. `climb_plays.py pressL<d>|pressR<d>`
grips d units in front of the ledge or rung 56 and pulls (a real lenient grab; `vr_climb_debug 3` also prints the
search's time, its traces and the drawn hand's ease; `vr_climb_try` ends with the lenient search's trace count, and the
table's last lines give the searches' median and worst times and traces; `vr_climb_debug 4` prints each mantle search's
traces and time, `climbcost` lines, or "no room (searched ... s ago)" while a search that found no room holds), `ledgehang|runghang` hang still for screenshots (`vr_mock_camera` for the
side view, `vr_mock_fingers main 1 1 1` for a gripping hand).
Climbing, hand orientation, staying attached, small ledges (ROUND21.md, the section of that name): `climb_plays.py`
`ledgeodd|rungodd` hang with the controllers turned oddly (the hand should face the hold whatever they do; Hold Rotation
Blend 1 is the old look), `push` (the ledge, `setpos 78 176 24 0 0 0`: both hands drawn in and pushed out three times,
then out past an arm's reach, then the off hand alone), `overtop` (the mantle's motion: at the ledge, at vrclimb's
narrow wall `setpos -218 -280 24 0 0 0`, which it should mantle onto, and at the ledge with no room on top `setpos -138
-280 24 0 0 0`, which it should hang on to), `ladderlean` (the ladder with the head leant in, as a player's is: the old
plays reach about 1 m from the shoulder, past a default arm, so run them with `vr_body_arm_length 2` to compare the
climbing with older logs). These lean the mock head in 0.28 m first (a `head` keyframe). `vr_climb_debug 2` adds a
`climbreach` line a frame (each hold's distance from its shoulder / how far it may be, "passive" for a hand that
doesn't pull, the owed motion, "noroom"), and at a grab its distance and the reach; `vr_climb_debug 3` the server's
estimated shoulders (`climbshoulder`) and the drawn arm's (`climbarm`, with the hold's distance from it). The mock
side camera for these: `vr_mock_camera 1.1 1.5 -0.45 5 90`.
Climbing, sliding along the wall (ROUND21.md, "Climbing: sliding along the wall; throw angle after calibration"):
`climb_plays.py shimmy[e1m1]<close|far>[_<drift cm>[_<wobble cm>]]` hangs from the ledge (vrclimb `setpos 78 176 24 0 0
0`; e1m1 `setpos 250 2350 40 0 0 0` with `nomonsters 1`, as a grunt walks into the hanging body), with the body drawn in
against the face (`close`) or pushed 12 cm off it (`far`), and shimmies 4 strokes each way (e1m1: 2), the hands
drifting in towards the chest and wobbling by that much over each 33 cm stroke; an `echo STROKE` / `STROKEEND` brackets
each push. `python Misc/quakevr/climb/shimmy_stats.py <log>` prints each stroke's move along the ledge against the
pull (it should be 8.67 units, 100%), in/out, and the frames stuck.
Climbing, the ledge map (ROUND21.md, "Ledge map"): `vr_debug_ledges 1` draws the ledges within 512 units (`vr_debug_ledges
<n>`: n units): each lip a glowing line (green on the world, pink on a brush model: a plat, a train, a door), a tick out
every 8 units (yellow where the drop starts more than 2 units out: past a trim, a rung below), and the top's depth in
(grey; a short stub for a top deeper than 40 units). `vr_ledges` prints the map's ledges, samples, memory and build
time (`vr_ledges rebuild` makes them again: the build time without the load's other work); with `developer 1` or
`vr_climb_debug 1` the load prints the same line. `vr_climb_probe [yaw]` lists the ledges ahead (their ends, height
over the feet, way out, where the drop starts, the top's depth). `vr_climb_try` prints the hold as before; its counts
are now of ledges looked at ("points"), and it adds `covered` (no room over the hold, the drop gone, the top not
reached from the hand: another solid over it, a fence before it) and `hidden` (another ledge is taken there instead:
the near side of a thin wall, the rung above); its time is the whole query's. vrclimb's moving ledges (each started by
a trigger where the player stands, facing +x, 18 units from its face): the lift (func_train, `setpos -318 88 24 0 0 0`,
rising at 8 units a second as soon as you're there) and the plat (func_plat, `setpos -218 72 24 0 0 0`, lowered 40 at 24
units a second 3 s after); `climb_plays.py lift` (take the lift's lip as it rises, ride it, pull over its top at 4 s: a
mantle onto it moving), `liftride` (hang on; add `5.000 cmd vr_test_remove 10` to remove the lift, entity 10, under
the hands), `plat` (take the plat's lip, pull up a little, hang on while it is lowered: the body is carried into the
floor; with `vr_climb_mover_crush 1` the plat is blocked instead, hurts you 1 point and goes back up). With
`vr_climb_debug 2` the `climbtrace` lines show the body carried with the hold exactly (`want`/`moved` 0 while the
mover's push moves it).
Climbing, the hands in sync and floating platforms (ROUND21.md, "Climbing: the hands in sync past the reach; floating
platforms"): `climb_plays.py overreach<hands>_<metres>` (the long ledge, `setpos 78 176 24 0 0 0; noclip`: 1 the main
hand, 2 both, 3 both holding and the main alone sweeping; the hands swept along the lip and back each way, `echo REACH
<phase>` at each end); `vr_climb_debug 2`'s `climbreach` lines end with `sync` (each controller's drift from its drawn
hand since it took hold, and the vectors). `vr_climb_slide 0` for the old behaviour. `climb_plays.py
float_<dist>_<below>[_<sink cm>]`: vrclimb's floating platforms (`setpos -236|-56 <400 - dist> <72 - below> 0 90 0`
with no `noclip` after it: the play turns it off as it grabs), the lip `dist` ahead (negative: the body under the
platform), grab, sink (or pull up, negative), push out 0.8 m, push down; `echo FLOAT <phase>`. Keep the box out of the
platform at the start (under the thin slab: `below` at least 65). `vr_climb_overhang_stretch 0` for the old behaviour.
Throwing, the release angle (the same section): `python Misc/quakevr/throw_plays.py [--gunangle 70] [--out throws.txt]`
writes a `vr_mock_play` file of four main-hand throws (an overarm throw with a wrist flick, an underarm lob, a straight
push, an overarm throw with a still wrist), each announced by `echo THROW <name> <meant elevation>` and let go with
`-grabmain` at its release; play it with `vr_debug_throw 2; vr_mock_grip_velocity 1` (the grip's velocity, as a Touch
controller in the headset reports it) and read each `throw main:` line's direction (elevation `atan2(z, hypot(x, y))`).
Print the hand settings in the same script (`vr_gunangle; vr_gunyaw; vr_handcal_x; ...`): the throws must not change
with them. `python Misc/quakevr/throw_calibration.py <takes folder>` models the release estimate of the code before the
fix on recorded takes (the old and the new hand settings, each term alone) and prints the elevation and speed changes
per take and per kind.

Fitted hands (round 21): `impulse 252` puts a gib or a head (nine kinds in turn) in the empty off hand; `impulse 253`
prints the held weapons' hotspots through the QC query; `vr_show_weapon_hotspots 1` marks them; `vr_hotspots_check`
compares every slot's hotspots, muzzle and hand with round 20's placement; `vr_hotspots_legacy` prints the slots'
round-20 two-handed grips as hotspot defaults.

Fitted hands, second pass: `vr_grasp_bench [n]` times each hand's grasp solve on what it holds (the first solve, and
again n times: min, median, max in microseconds); `vr_debug_grasp_trace 3` writes both hands' joints, solves and drift
every frame to `grasp_trace.txt`; `vr_grasp_spheres` prints the hand's collision spheres; `vr_debug_carry 1` shows the
carry reach test (the thing's box, its nearest surface point, the reach); `vr_weapon_hotspot_here <n> [type]
[main|off]` puts hotspot n where the other hand is. `vr_profile` has scopes for each hand's update (`hand`, `rig hand`,
`grasp solve`, `hand walls`, `hand collide`).

Fitted hands, third pass: `vr_show_controller 1` draws each controller as tracked (before any offset), and
`vr_show_controller_laser 1` the controller's aim (white), the weapon's shots (red) and its barrel (green);
`vr_dumpview` prints each hand's place, angles, controller and drawn palm to four decimals; `vr_debug_grasp 1` says
where the held thing is in the hand and whether it was solved afresh at rest. The tuning keys are per weapon:
`vr_wofs_whole_*`, `vr_wofs_hand_only_*`, `vr_wofs_overlap`, `vr_wofs_fgr_manual`, `vr_wofs_fgr_curl_*`,
`vr_wofs_fgr_thumb_across`, and per hotspot `vr_wofs_hsN_overlap`, `_vx/vy/vz/vpitch/vyaw/vroll`, `_manual`,
`_curl_*`, `_thumb_across`.
Hands remodelled (ROUND21.md): `python Misc/quakevr/make_hand_rig.py` regenerates the jointed hand (its mesh, rig,
skins and `vr_handrig_data.inc`; rebuild the engine after it); `vr_grasp_spheres` lists the grasp solver's spheres and
`vr_debug_hand_bones 1` draws them with the joints; `vr_hand_rig 0` draws the six old models.
Hand editable in Blender (ROUND21.md, HANDS_IN_BLENDER.md): the rig is read from `progs/hand_rig.md5mesh`;
`vr_hand_reload` reads it again (a file it can't use is refused with the reason, the hand kept), `vr_hand_rig_info`
says where the rig came from and whether it is the compiled one bit for bit (it is, with the shipped files).
Two-handed props: `vr_rigid_place item_health main 0 3 0` with `+grabright; vr_mock_button main grip 1` puts a health
box in the main hand; move the off hand to its other side (`vr_mock_hand off -0.19 1.30 -0.45 0 0 0` with the main
at `0.10 1.30 -0.45`) and press its grip: `carry: both hands` (developer 1). `vr_debug_carry 2` writes the object and
both hands every frame to `carry_trace.txt` (and prints the second hand's reach test); `vr_debug_throw 1` prints
`throw both hands (...)`. `vr_mock_play` keyframes move both hands with their velocities (throws, turns).
Put the off hand out of the way first (`vr_mock_hand off -0.35 1.1 -0.2 0 0 0`): at its default pose it touches the
box before the main hand (no `carry: taken`, and the box drops). Carrying across a save (round 21):
`save c1; wait10; load c1; wait60; screenshot`; the box should still be in the hand(s).
Precise hit detection (ROUND21.md, "Precise hit detection (models, not boxes)"): `vr_hit_precise 0|1`,
`vr_hit_tolerance_guns|grapple|melee|thrown <units>`; `vr_debug_hits 1` draws and prints each hit (`hit model: ...`),
`2` also the moves through a box that missed its model. Tests (single player, at the monster spawned last with `impulse
241`): `impulse 238` prints `hittest` lines, boxes then models (rays through the box's
corner columns and its middle, the hook's, melee segments, thrown boxes; 24 rays at the head and at the chest with their
regions; `developer 2` adds each head ray's distance from the head sphere; removes the monster); `impulse 237` / `236` /
`234` fire one nail / rocket / hook-class nail through the next corner column (then the middle; the target takes no
damage), `impulse 235` says what it touched. `vr_hitmodel_bench [rays] [tolerance] [newest]` fires random rays at each
monster's box: the share that meets the model, each test's cost, how far the model reaches out of its box;
`vr_hitmodel_stats [reset]` the tests so far. The archived melee takes: the scratchpad's `hitbox/melee_replay.sh`.
Held weapons against models (round 21): `vr_debug_model_collide 1` prints each hand's push, `2` draws the rays;
`vr_model_collide_bench [n] [list]` times the test, `vr_model_collide_bench probe` lists the model triangles a ray along
the view goes in and out by. `vr_test_remove <n>` removes entity n as QC's `remove()` would (a slot to reuse; ROUND21.md, "Performance fixes
(review, 2026-09-28)"). `impulse 241` puts a monster (`vr_test_spawn`: the firing range dispenser's numbers) or a
box (100 health, 101 shells, 102 an explosive box, 103 a small one) `vr_test_spawn_dist` units ahead
(`vr_test_spawn_dead 1`: a corpse); `impulse 232` flings the loose prop nearest you at the nearest monster
(`vr_test_fling_speed` m/s; `vr_test_fling_at 1` at you, `vr_test_fling_away 1` away from inside its box: ROUND21.md,
"Flung props: settings, and never you");
`vr_mock_camera <x> <y> <z> <pitch> <yaw>` draws the mock eyes from elsewhere in the tracking space (a spectator's view of
your body; the hands stay with the head), `vr_mock_camera` alone puts them back.
Grappling hook (round 21): `impulse 151` (main hand), `vr_mock_hand main 0.2 1.3 -0.3 70 0 0` aims level (105:
up ahead, 160: straight up), `+attack` fires and holds, `vr_mock_button main secondary 1` / `0` reels; with
`developer 1; vr_grapple_debug 1` (2: the rope's state too) the log has what it bit, its mass and class, and each
reel's distance, rope and closing speed. The scratchpad's `grapple/run_all.sh` has the round's checks.
Unreel: `vr_mock_button main primary 1` / `0` (A). In a script, add `+jump` / `-jump` with it for A's jump: a mock
button's key binding runs only after the script's remaining commands. The log has `unreel on`, `unreels:` (rope,
distance, paying out u/s, on ground), `unreel off (braked)` and `unreel button on the ground`.
`vr_grapple_unreel_airborne 0` unreels standing. With `vr_grapple_debug 2` each rope drawn prints its chord, length,
sag, samples, links and build time twice a second. The profiler's **grapple rope** system is its cost. For rope
close-ups, bigger eye images: `vr_mock_eye_size 1440; vr_restart` before the map, `vr_eyeshot 1` before each
`screenshot`. The scratchpad's `grapple2/` has `unreel.sh`, `unreel_prop.sh`, `ropeshots.sh`, `slackshots.sh`,
`measure.sh` and `measure_long.sh`.
Grapple persistence (ROUND21.md, "Grappling hook: one persistent system"): `impulse 239` prints every hook (its state,
what it is in, where its gun is, the rope) and your hands' places; `impulse 233` takes what the hooks are in (a
pickup's take; a thrown weapon into an empty hand). The off trigger in a script is `+offhandattack` / `-offhandattack`
(a mock button's binding runs after the script). `impulse 171` puts a grapple in the off hand. With
`vr_weapon_grip_mode 0`, `+grabright; vr_mock_button main grip 1` before `impulse 151` keeps the gun, releasing the
grip drops it. `developer 2` prints the other fingertip's distance from a weapon's button (placing buttons; the button
sends its impulse at the front of the command buffer). The physical rope (ROUND21.md, "Grapple: a physical rope"):
`vr_grapple_rope_dump` prints each rope's points, its taut path's corners and how many points are inside the world or a
prop; `vr_grapple_rope_cast x y z x y z [radius]` sweeps its sphere between two points; `vr_grapple_debug 2` prints
each rope's chain and path twice a second, 3 each frame the rope holds you round a corner (`anchor:`); the profiler's
**grapple rope sim** is its cost. `vr_forcegrabbable_box_scale 1` before the map makes the health boxes full size (a
prop to lay the rope over). The kit's `scratch/hook/run_all.sh` runs the 18 scenarios (`fresh`: all again).
Leaning (round 21): `vr_mock_hand head <x> <y> <z> <pitch> <yaw> <roll>` and `vr_mock_play` head keyframes with angles
turn the head too (pitch up, roll as the hands'); `vr_debug_lean 1` writes `lean_trace.txt` (the game directory): the
head, the box, the lean, the pelvis, the feet and the lean's hold and cues, every frame.
Body calibration (ROUND21.md, "Body calibration"): `vr_bodycal standing` runs it in the mock too. A synthetic person
doing its poses is a take of raw tracking played alongside: `vr_motion_play <take> watch noplace`, then
`vr_bodycal standing` in the same frame (the scratchpad's `bodycal/gentake.py` makes them); `vr_bodycal_print`
prints the result. `vr_bodycal_refit <file>` fits a saved session (`quakevr/bodycal/`) again; `vr_bodycal_debug`
prints the empty hands' wrists.
Arms options and holster limits (ROUND21.md, "Arms options after body calibration; holster limits"): `cvarlist
vr_bodycal_` and `cvarlist vr_body_tweak` show the measurements and the tweaks (typing `vr_bodycal_undo` runs Undo;
list it instead). To test a config's migration, copy it over the worktree's `quakevr/ironwail.cfg` before the run (the
kit puts the baseline back after); `developer 1` can't show it (the config runs first), the cvars can. `vr_debug_arm 1`
in fixed `vr_mock_hand` poses compares arms between builds (the elbow's swing is eased over frames: 0.1 degrees between
runs). A holster behind you: `vr_hip_offset_x -12` puts the right hip holster at `vr_mock_hand main 0.20 0.95 0.26`
(-20: z 0.50); draw with `vr_weapon_grip_mode 0; +grabright; vr_mock_button main grip 1`, holster by going back there
with `-grabright; vr_mock_button main grip 0` (grip mode 1 keeps the gun until the grip is pressed again). `vr_dumpview` lists the holstered guns' models.
Body collisions (ROUND21.md, "Body collisions"): `vr_debug_body_collide 1` prints each contact's change (in, let
go, clear: `body collide: t ... main weapon/other forearm: block -> pass (through 0.54, push 10.5 cm)`) and writes
`body_collide_trace.txt` (the game directory): a line a frame while any is on, each hand's tracked place, its drawn
offset and target (cm) and each contact's state, weight, share through, push and way out; `2` also draws the proxies
(torso and head blue, arms green, gadget yellow, hands white, weapons red, stocks dark red) and each push (yellow).
`vr_body_collide_bench [n] [list]` times the solve as it is now (and lists the capsules). The scratchpad's
`handcoll/scen.py` runs a scenario with and without it and composes the shots (`s1.py` .. `s10.py`); `trace.py`
prints a trace in the mock's tracking-space metres (e1m1's start). Poses that meet: the main forearm raised across
(`vr_mock_hand main -0.10 1.45 -0.45 0 80 0`) with the off hand under it at `0.157 1.26 -0.426` going up; the chest
from `vr_mock_hand main 0.06 1.33 -0.14 0 90 90` going back (+z); the gadget (off forearm `0.10 1.45 -0.45 0 -80 0`)
from `vr_mock_hand main -0.15 1.66 -0.50` going down.
Arm IK (ROUND21.md, "Arm IK with calibrated hands"): `vr_debug_arm 1` prints each drawn arm once (shoulder, elbow,
wrist, the elbow's swing, the wrist's flexion, deviation, twist and strain against the solved forearm and the pole's;
`armT` lines in tracking-space metres from the head, for `vr_mock_hand`); `vr_debug_arm 2` writes it every frame to
`arm_trace.txt`, e.g. while `vr_motion_eval` replays takes.

After the posing test (ROUND21.md): `vr_debug_shots 1` with `developer 1` prints each hitscan shot (start, direction,
what its pellets hit, headshots) and each damage you deal; a monster at the muzzle: `impulse 150 + weapon id` (with
`vr_weapon_grip_mode 1`, `impulse 9` for ammo a frame before), `vr_mock_hand main 0.08 1.05 -1.2 40 0 0`, then
`vr_test_spawn 0; vr_test_spawn_dist 44; impulse 241` puts a grunt's head round the muzzle in vrfiringrange.
`vr_pose_solve 1` solves the posing hand live (as before).
Dummy attacks (round 21): `vr_dummy_attacks 1` in vrfiringrange (as the button); with `developer 1` each wind-up, blow
and miss is printed with its time; `vr_dummy_attack_jitter 0` makes the blows regular (the first 1.6 s after it's
turned on, then every `vr_dummy_attack_period`). Note: `setpos` turns noclip on, and in noclip a blow's knockback
(any push) is lost: `setpos ...; noclip` turns it off again. `vr_wofs_shot_pitch_NN` / `_shot_yaw_NN` turn a
weapon's shots; `vr_show_controller_x/y/z/pitch/yaw/roll` (and `_off_own`, `_off_*`) move the controller
preview; `vr_dumpview` prints each grip and its distance from the tracked pose (10.2 cm with
`vr_controller_legacy_pose 1`).
Hand calibration (ROUND21.md, "Hand calibration"): `vr_handcal_x/_y/_z` (cm along the controller's grip axes) and
`vr_handcal_roll` move and roll the main hand on its controller (`vr_handcal_off_*` the off hand's, or
`vr_handcal_off_mirror 1` the main hand's mirrored); `vr_handcal_match` is Match Controller Preview; `developer 3`
prints the melee's grip, far end and wrist as the server has them (`melee trace:`).
Weapon posing mode (ROUND21.md, "Weapon posing mode"): `vr_pose [weapon | 1..4 | new | stop] [main | off]` poses the
weapon in the main hand (or else the off hand's), held in the hand given (`vr_pose_weapon_hand`); `vr_pose_confirm`,
`vr_pose_undo`, `vr_pose_next`, `vr_pose_type` and `vr_pose_turn <yaw> <tilt>` (no arguments: back to the start) are
the buttons' actions. The buttons work in the mock too: the confirming hand's `primary` confirms, `secondary` undoes,
`trigger` goes on to the next thing to pose, `stickclick` changes a hotspot's type (on the weapon: its turn back to the start), `menu` leaves. `vr_pose_check`, after
leaving, holding the weapon (and, for a hotspot, the other hand holding it by the hotspot: `+graboff;
vr_mock_button off grip 1`), prints how far the hand is from the pose confirmed last, relative to the weapon (its rig,
the weapon's muzzle, the drawn palm), and, for the weapon, how far the hand is in the world from where it was drawn
while posing (the same controller pose: this one shows the angle offsets' Euler quirk). Recipe: `map vrfiringrange;
wait60; vr_weapon_grip_mode 1; impulse 155; wait60; vr_mock_look 30 0; vr_mock_hand main 0.0 1.33 -0.38 40 0 0;
vr_mock_fingers main 1 1 1; vr_pose; wait90; vr_mock_button off primary 1; wait3; vr_mock_button off primary 0; wait10;
vr_pose stop; wait120; vr_pose_check`. The floating weapon's grip is at (0, 1.35, -0.4) in the mock's tracking space
(40 cm ahead of the head, 35 cm below it); the super shotgun's foregrip is near (-0.06, 1.31, -0.71).
Rigid bodies (round 21, Box3D, the only solver): `vr_physics_stack`, `vr_physics_pyramid`,
`vr_physics_pile` put props (a number, a classname or `props`) in a column, a pyramid or toppling columns;
`vr_physics_loose` makes a hanging armour or a pickup a loose prop; `vr_physics_list` and `vr_physics_hash` print
them (the hash: determinism); `vr_debug_box3d 1|2|3` (1: bodies made, woken, asleep, the world mesh's build, frames
over 1 ms with Box3D's profile; 2: every awake body every frame; 3: frames over 0.2 ms; `developer 1` alone prints the
slow frames). `vr_physics_blast <x> <y> <z> [<damage>]` sets off an explosion there (QC's `T_RadiusDamage` from the
world, 120 by default, and its effect): monsters take it, props are thrown (Box3D). `vr_forcegrabbable_return 0` keeps
moved items from going back to their places during a long test.
Items as physics pickups (ROUND21.md): `vr_physics_spawn <classname> [<distance> [<left>]]` makes a map entity by its
spawn function on the floor ahead of you (`vr_physics_spawn item_key1 25 -1` puts a hanging key at the off hand's
`vr_mock_hand off -0.05 0.85 -0.9 0 0 0` in the firing range); the left hip holster is `vr_mock_hand off -0.20 0.95 0.0 0
0 0`; `developer 1` prints `pickup:` lines (an object at, taken as a weapon, taken at a holster, back at).
`vr_physics_sink [what]` measures how far props' drawn models are in the floor (and their shapes against the drawn);
with `vr_debug_box3d 1`, `vr_physics_list` prints movetype, solid, rigid and flags. The firing range has no deathmatch
starts (`SelectSpawnPoint` loops for ever there): use dm3 for deathmatch tests.
Debug views (Advanced VR Options > Debug > Views, also in the console): `vr_debug_physics_shapes 1` draws every Box3D body as a
wireframe (props awake green, fast white, asleep blue, held yellow; doors and plats purple; monsters orange; players
cyan, your own faint; hanging pickups grey), each prop's centre of mass, an awake prop's contact points (red pressed
in, pink apart), and the hands' grab probes (as `vr_debug_carry`). `vr_debug_hand_bones 1` draws both jointed hands'
bones and joints (thumb red, index orange, middle yellow, ring green, little blue; joints white), the grasp's spheres
against what the hand holds (green touching, yellow near, red sunk in, grey nothing near; a line to the nearest point
of each touching one), the palm's fit (white: its middle before the grasp's move, cyan: after; the cyan stroke is the
way the palm faces) and the grip channel (magenta). With a gun held: `vr_weapon_grip_mode 1; impulse 154` and
`+grabright; vr_mock_button main grip 1`; a box in the off hand: `vr_rigid_place item_health off 0 3 0; +graboff;
vr_mock_button off grip 1`; `vr_mock_camera -0.05 1.45 -1.0 25 180` looks back at both hands.

Grab reach from the fist (ROUND21.md): `vr_debug_carry 2` prints each grab test (the fist's gap to the surface, the allowed Grab Distance Bias `vr_carry_grab_bias`; 3: also fists not near); `vr_dumpview` prints the fist's and the open hand's extents from the hand's point. Two-handed detach: `vr_debug_carry 2` prints each hand's distance from its grip (`vr_carry_two_hands_detach`: cm past the drift). Brushing fingers (`vr_hand_collide_fingers`): `vr_debug_grasp 1` prints each brush solve (the fingers: on, out, in). Put a floating box in front of a hand: `sv_gravity 0; vr_rigid_place item_shells main <forward> 0 -2 0 <the hand's yaw> 0` (units; the box is 6 units, centred on its origin).

Tuning the body: `vr_show_hip_holsters 1`, `vr_show_upper_holsters 1`, `vr_show_shoulder_holsters 1` and
`vr_show_virtual_stock 1` mark where the holsters and the virtual stock's shoulders are (green while a hand is
there); move them with the `vr_*_offset_*` cvars.
Holsters with guns in them (mock; ROUND21.md, "Holster orientation"): `vr_weapon_grip_mode 1`, then per holster
`impulse 154; vr_mock_hand main 0.20 0.95 0.0 0 0 0; wait40; vr_mock_button main grip 1; wait10; vr_mock_button main
grip 0` (the right hip; the right shoulder is `0.1 1.75 0.12`). A new game starts with the shotgun and the axe on the
hips. To draw: `+grabright; vr_mock_button main grip 1` at the holster. Turn them with
`vr_{hip,upper,shoulder}_holster_{pitch,yaw,roll}`. `vr_body_debug 2` or `3` shows the hip and chest holsters on the
preview; `vr_mock_camera 0.7 1.9 0.8 20 40` shows the back.
Per-weapon holstered pose (ROUND21.md, "Per-weapon holstered pose"): `vr_wofs_hol_{hip,upper,shoulder}_{x,y,z,pitch,yaw,roll}_NN`
(the rocket launcher is `_07`, `progs/v_rock2.mdl`; the nailgun `_04`). A new game's hips are full (the shotgun and the
axe): empty one first by drawing its gun (`vr_weapon_grip_mode 0`, the hand at the hip, `+grabright; vr_mock_button main
grip 1`) and letting go of it in front. Holster a test gun with the grip held: `+grabright; vr_mock_button main grip 1;
impulse 160` in front, then at the holster `-grabright; vr_mock_button main grip 0`. The main hand doesn't reach the left
chest holster (`-0.12 1.3 -0.15` gives the two-handed grab): holster there with the off hand (`impulse 176`, `+grableft`,
`vr_mock_button off grip`); the right chest holster is `vr_mock_hand main 0.12 1.3 -0.15`. The preview: hold the gun,
`menu_vr 23 "Holstered X"` (menu_vr's row may now be a label's start), `vr_weapon_holster 1..3` picks the kind. The mock
draws the menu over the whole view: `scr_menubgalpha 0` and the camera off to a side (`vr_mock_camera 0.6 1.2 -0.4 4 0`
with `vr_body_debug 2`) leave the body preview visible on the left. `vr_dumpview` lists each holstered gun's place.
Runs aren't pixel-identical (particles, the arms' easing, lighting by ones): compare the holstered guns' lines of
`vr_dumpview`.
Draw and holster blend (ROUND21.md, "Holster draw blend; holster defaults; body calibration kept"):
- **The log:** `vr_debug_draw_blend 1` prints each frame of a blend: the turn and distance left, the start, and the turn
  without the sign flip.
- **Drawing:** draw the shotgun from the right hip with `vr_weapon_grip_mode 0; vr_mock_hand main 0.20 0.95 0.0 70 0
  <roll>; wait30; +grabright; vr_mock_button main grip 1`. A roll near 180 gives a start near 180 degrees.
- **Holstering:** put it back at the same place with `-grabright; vr_mock_button main grip 0`.
- **Pictures:** use a slow blend (`vr_weapon_draw_blend 3`) and `vr_mock_camera 0.75 1.35 -0.55 25 140`.
- **The config across a killed session** (scripts in the scratchpad's `notes4/`):
  1. Run `vr_bodycal_refit bodycal/<session>.txt; vr_bodycal_apply` with no `quit`. The run times out and is killed.
  2. While it runs, have a watcher copy `quakevr/ironwail.cfg`: the kit restores the baseline config after the run.
  3. Start a second run from that copy.
Wounds (ROUND21.md, "Dynamic wounds, burns and wetness"): `vr_wounds_test <entity|self|ahead|all> <kind> [amount]
[right] [up] [extra]` paints a wound as the server's event would (1 shot, 2 nail, 3 melee, 4 blast, 5 burn, 6 zap,
7 lava, 8 slime, 9 liquid; a liquid's `up` is its surface over the feet); `ahead` is the model nearest the view's
line (a grunt from `vr_test_spawn 0; vr_test_spawn_dist 70; impulse 241` in `vrfiringrange`), `all` every model
(the stress test). `vr_wounds_dump` writes each mask to `quakevr/wounds/mask_<layer>_<model>.png` (delete them
after), `vr_wounds_info` lists the pool, `vr_wounds_debug 1` prints each event and where it landed (2: also the
player's capsules). `give h 30` then `give h 100` checks the heal fade. `vr_physics_blast` from the console loses the
wound events (they go out in the next server frame's datagram, cleared first): test explosions with
`vr_wounds_test ahead 4 <damage>`. Screenshots comparable run to run: `host_framerate 0.0111; vr_particle_seed 7;
vr_body_blood 0; vr_body_blood_floor 0` (the wrist gadget's readout still changes).
Liquids (ROUND21.md, "Enemies hurt by liquids"): `vr_debug_shots 1; developer 1` logs each burn (`liquid: ... health`);
with `vr_enemy_liquid_damage 0` it logs a "not burnt" line each second instead. Run `god; notarget` first, then:
- a grunt in e1m1's slime: `setpos 200 2820 -60; wait5; vr_test_spawn 0; vr_test_spawn_dist 64; impulse 241`;
- a grunt in e1m7's lava: `setpos -50 48 20 0 0 0; vr_test_spawn_dist 200; impulse 241`.

Flashlight tuning (ROUND21.md, "Flashlight tuning"): `vr_show_flashlight_zones 1` draws the reach zones (the head's
balls, each gun's capsule; green in reach) and the held torch's middle; `vr_flashlight_head_zone_*` and
`vr_flashlight_gun_zone_*` move them, and `vr_flashlight_low_*` / `_high_*` `fingers`, `curl_*`, `thumb_across`,
`overlap`, `bias_*`, `thumb_x/y/z` set each grip's fingers. `vr_debug_grasp 2` prints the torch grasp's finger stops.
With `developer 1` a take logs the grip chosen ("flashlight: overhead grip (from the belt)"). In the mock (e1m1 start)
the belt torch is reached with `vr_mock_fingers off 0 0; vr_mock_hand off -0.066 1.05 -0.072 -80 0 0; wait40;
vr_mock_button off grip 1`.

Wrist gadget: `vr_gadget_info` prints its screen's centre and axes (right = along the forearm, up, out). To bend the
wrist in the mock without moving it, turn the controller about the wrist: the scratchpad's `gadget2/mkposes.py` makes
such `vr_mock_hand` poses (flexion, deviation, twist about the forearm's own axes, from a straight wrist found by
`straight.py`). `vr_gadget_fps 2` with `host_maxfps 45` or a `timerefresh` checks the Detailed counter's LATE and spikes
(the mock doesn't tell a refresh: 90 Hz is assumed, and it paces at about 64 Hz, so every frame is late there).
Melee fixes (ROUND21.md, "Melee fixes: flashlight, axe on walls, gibs"): the tests' motions come from
`Misc/quakevr/motion_synth.py` (`--settings-from <ironwail.cfg>` writes them for a config's hand calibration, `--mock`
also writes a `vr_mock_play` script, `--name` the file's name). A weapon into a wall: `chop_horizontal`,
`chop_diagonal`, `chop_overhead` with `--weapon axe|mjolnir|sword|shotgun`, played with `setpos 100 106 48 0 90 0;
wait20; vr_motion_play <take> noplace yaw 90` in the firing range (its north wall 22 units ahead of the player's
origin; `noplace` keeps the player there and `yaw 90` turns the take to face the wall: without it a synthetic take
faces yaw 0). The flashlight (a take can't carry it, and playback's first press would send it home): take it first
(`vr_mock_fingers off 0 0; vr_mock_hand off -0.066 1.05 -0.072 -80 0 0; wait40; vr_mock_button off grip 1; wait20`;
`vr_mock_button off secondary 1`, then 0, flips the grip), then `setpos 221.2 -656.7 41 0 180 0` (the dummy 0.95 m
ahead) and `vr_mock_play <punch_straight_off | palm_shove_2h_torch | palm_shove_torch_only>.mock`; for the gripped-fist
comparison `+graboff; vr_mock_fingers off 1 1; vr_mock_button off grip 1` instead. A gib on the floor: `setpos 316 -556
56 0 180 0`, `vr_mock_hand off -0.05 0.45 -0.45 0 0 0; +graboff; vr_mock_button off grip 1; impulse 245` (a destroyable
gib in the off hand; `impulse 252`'s test gibs don't take damage), `-graboff; vr_mock_button off grip 0; wait120` (it
lies 0.45 m ahead), then the main hand (`+grabright; vr_mock_button main grip 1`, or the axe: `vr_weapon_grip_mode 1;
impulse 9; impulse 152` first) and `vr_mock_play punch_down_gib.mock` or `chop_down_gib.mock` (`--weapon axe`).
`developer 1` prints `melee event: ... a wall` / `a gib`, `gib: struck by a blow`, `flashlight: taken`; `developer 2`
also `melee debug: going down at ... onto a surface facing ...: strikes|passes` (the wall rule for points going down).
Note that `setpos` turns noclip on (QuakeSpasm's), which these tests don't mind.
Deflection, grenades and ogre aim (ROUND21.md, "Deflection by blows and bashes; catching grenades; ogre aim"):
`impulse 246` fires `vr_test_projectile`'s kind at your face from 300 units (0 a knight's spike, 1 a laser, 2 a
scrag's spit, 3 a vore's ball, 4 an ogre's grenade, 5 a zombie's flesh; lobbed ones on the arc that reaches you) from
`vr_test_projectile_side` degrees to your left of ahead (`impulse 247`: a hell knight's spike at 300 u/s).
`impulse 240` makes the nearest ogre or zombie throw at you now (an ogre from the dispenser: `vr_test_spawn 1;
vr_test_spawn_dist 200; impulse 241`, with `notarget` it waits; `skill 0` so it isn't a multi-grenade ogre, or
`vr_test_projectile 6` for one). `vr_rigid_place ogre_grenade main` puts the grenade in the main hand (grip to catch
it). `developer 1` prints `deflect: <what> by a swing|bash, in <v>, out <v>`, the weapon's point, velocity and face
(swings) or the push (bashes), `deflect: aim assist towards ...`, `lob: <ogre> at player, <across>, <up>: the lower
arc|the higher arc|out of reach, <deg>, <s>; <u/s> <v> (Quake's <v>)`, `grenade: caught in hand <h>, the fuse <s> ->
<s>`, `grenade: let go of at ...`, `grenade: <what> hits <whom>`, `went off in player's hand`, `first bounces at`.
Drive batting and bashes with `vr_mock_play` in real time (`host_maxfps 90`, not `vr_fixed_frames 1`, which runs the
game's clock apart from the play's): the scratchpad's `projectiles/gen.py B|P|W|A` (swings, one-handed bashes,
two-handed parry bashes, the aim assist; round 20's poses, so it sets and prints the old hand settings),
`gren.py fly|place|force|hold|regrab|show`, and `ledge.sh above|below <aim 0|1>` (vrclimb's platform and trench).
Catching versus deflecting, returned grenades, bash direction (ROUND21.md, "Catching versus deflecting; returned
grenades; sword bash direction"): the scratchpad's `projfix/t.py C|W|D` prints the console script and writes the play.
- `C`: an empty main hand (`vr_weapon_grip_mode 1; impulse 150`) meets `impulse 246` grenades (`vr_test_projectile
  4`, aimed 16 units ahead of the face and 10 down; the hand at `0.02 1.33 -0.66`): held out still, reaching, open or
  closing, punching, shoving; the open palm is `160 -90 0` (Gun Angle 70).
- `W`: two- and one-handed bashes against a spike (old hand settings). Round 20's poses draw the blade a few degrees off
  the pose asked for, so compare the logged blade axis with the direction sent.
- `D`: throw back vs the launcher, `CASE=gl` for the launcher (`impulse 9; impulse 158`, the grip held, `+attack`),
  `EXTRA="vr_grenade_return_full 0"` or `EXTRA="impulse 255"` (Quad), with `vr_debug_shots 1` for `damage: ...`.
- `developer 1` prints:
  - `grenade: <what> caught in flight by hand <h> (the palm's reach, <d> units off | the palm's grip, <s> s after it met
    it | touched)`;
  - `grenade: <what> stopped by the open palm of hand <h> at <u/s> (facing it <cos>)`;
  - `drops from the open hand`;
  - `thrown back | batted back by a <how>: goes off as your launcher's`;
  - `deflect: off the blade <axis> (two hands: its middle | where it met), its point <t> along moving <v>: sent <dir>`.
Stamina for shoves and strikes (ROUND21.md): `setpos 221.2 -656.7 41 0 180 0` (the dummy 0.95 m ahead), the takes
from `motion_synth.py <palm_shove_2h | punch_straight | slash_horizontal_rtl> --distance 0.95 --mock` played with
`vr_mock_play` one after another (an empty main hand: `vr_weapon_grip_mode 1; impulse 150`; a fist: `+grabright;
vr_mock_fingers main 1 1; vr_mock_button main grip 1`); end each take with a slow return to its first pose, or the
jump back reads as a second blow. `developer 1` prints `stamina: 60 of 100 left (a shove, two hands, -20)`, `: short,
paid 4.0` and `stamina: the blow deals x0.75`, `bash hit: <monster>, <damage>, knocked back at <u/s>, staggered <s>`;
the dummy's readout adds `exhausted x0.50`. `vr_gadget_screen_dump <name>` saves the gadget screen. Dummy attacks
need `notarget` off. Thrown things onto a floor gib: the gib as in "Melee fixes" but 0.9 m ahead, `vr_mock_look 65 0`,
then the axe (`impulse 152`) or a sword (`impulse 163`) gripped at `vr_mock_hand main 0.1 1.7 -0.55 70 0 0` and a
`vr_mock_play` that moves the hand to `0.05 1.1 -0.75` in 0.12 s and lets go of the grip there (a box: `give s 100`,
`vr_test_spawn 101; impulse 241`, wait 2 s for it to become grabbable, `vr_rigid_place item_shells main` and grip;
a gib: `impulse 245` in the off hand); `developer 1` prints `gib: hit by thrown_weapon for 69.2, 0 left`,
`vr_debug_box3d 1` also `box3d: ... touch, at 0 and 293 u/s (11 and 146 after the step)` for two props meeting.
Weight (ROUND21.md, "Weight: spring model, stamina, held object offsets; explosive boxes"): `vr_weight_test [csv]`
runs the spring alone (shotgun, rocket launcher, a 40 kg box; one and two hands; stamina 100/50/25/0; 45/72/90/144 fps)
and prints lag, overshoot, settling, sag, jitter and the snap (`csv`: every frame into `weight_test.csv`).
`vr_debug_weight 1` writes each holding hand's target and drawn pose, a line a frame, to `weight_trace.txt` (2:
printed too); `vr_debug_weight_stamina <0..1>` sets the stamina the weight sees (-1: the game's). A swing:
`vr_fixed_frames 1`, `impulse 160` with `vr_weapon_grip_mode 1; impulse 9`, a `vr_mock_play` of the main hand (and the
off hand on the foregrip at `0.09 1.37 -0.83 70 0 0` with `+graboff; vr_mock_button off grip 1` for two hands); the
scratchpad's `weight/trace_stats.py` and `plot_swings.py` read the trace. The canary takes with the old hand settings:
`weight/oldeval.sh <label> "<cvars>"`. Explosive boxes in the firing range: `vr_physics_spawn misc_explobox`, then
`vr_rigid_place misc_explobox 240 -456 17.5` with the player at `setpos 300 -440 45 0 180 0; noclip` (clear of the
dummy); a push: the main hand from `0 1.7 -0.3 70 0 0` to `0 1.7 -1.9` in 1 s; shoot with `+attack` (the mock's
trigger button didn't fire here after `setpos`). `vr_physics_list` with `vr_debug_box3d 1` prints each prop's mass
(and whether Held Object Offsets set it) and throw share; `vr_physics_forcegrab <what>` whether the force grab may take
each; `developer 1` prints `explobox: hit at <m/s>` and `explobox: blows up at <where>`. The Held Object Offsets page is
`menu_vr 41`.
Spring only; Weapon Weights and Held Object Weights; weight and damage (ROUND21.md): the pages are `menu_vr 42` (Weapon
Weights) and `menu_vr 43` (Held Object Weights; `menu_vr <page> <row>` scrolls). `vr_weight_table` prints every weapon's
and prop's mass, its damage multipliers (the curve, times its own Melee and Throw Damage x) and its speed factor (heavy
leniency), then the level's other things with their mass as the game has it (Box3D's). `vr_weight_test` takes each
thing's own spring multipliers (`vr_wofs_w_stiff_07 2` changes the rocket launcher's rows, `vr_prop_damping_01 0.5` the
box's). A slow heavy club: `vr_weapon_grip_mode 1; impulse 9; impulse 152` (the axe), `vr_wofs_w_mass_01 20` **after**
the map has loaded (a test base's old `vr_wofs_version` resets the slots at the first lookup), a grunt at
`vr_test_spawn 0; vr_test_spawn_dist 24; impulse 241` from `setpos 300 -440 45 0 180 0; noclip`, and a `vr_mock_play` of
the main hand from `0.35 1.35 -0.30 70 35 0` to `-0.25 1.35 -0.45 70 -35 0` in 0.3 s (eased); `vr_weight_lenient 0`
for the old thresholds. A slow throw of the explosive box: `vr_physics_spawn misc_explobox 150 200; vr_rigid_place
misc_explobox main 0 3 0; +grabright; vr_mock_button main grip 1`, a grunt at `vr_test_spawn_dist 70`, and the hand from
`0.15 1.25 -0.25` to `0.15 1.45 -0.75` in 0.12 s, then `button main grip 0` and `-grabright`; `developer 1;
vr_debug_shots 1; vr_debug_box3d 1` print `box3d: ... misc_explobox hit ... monster_army at <m/s>`, `explobox: thrown
into monster_army at <u/s>: <damage>` and the damage. The scratchpad's `weights2/plays.py` writes the plays,
`mkswing.sh` / `mkbox.sh` the scripts, `go.sh` runs one; `oldeval.sh` replays the canary takes with the old hand settings.
Wall torches (ROUND21.md, "Wall torches you can take"): e1m2's torches are edicts 52 (1706 -206 316: pull it by hand
from `setpos 1714 -190 312 0 243 0; noclip` with the main hand at `0.0 0.8 -0.6 70 0 0`, `+grabright; vr_mock_button
main grip 1`, then the hand back 10 cm), 53 (2134 -34 316: force grab it from `setpos 2047 -84 312 0 30 0; noclip` with a
`vr_mock_play` that points the hand at it, `cmd +attack`, flicks it up 0.3 m in 0.1 s, then `cmd +grabright` and the
grip; the mock's trigger button doesn't lock on here, `+attack` does) and 156 (2134 -474 316). `vr_rigid_place 53 main 0
0 0` then the grip puts a lying torch back in the hand. `developer 1` prints `walltorch: ...` (gripped, pulled out,
blow n of 5, dying at t, out at t, taken again, lit again) and `wall torch: its wall's crackle ... silenced` (with
`-Sound`); `vr_debug_shots 1` the blows and `by vr_torch_burn`; `vr_debug_torch_lights 1` every torch light (radius,
colour, taken, shadowed). A monster to strike: `vr_test_spawn 0; vr_test_spawn_dist 34; impulse 241`; `god; notarget`
and `gl_cshiftpercent 0` keep the screenshots clear of its shots. The scratchpad's `torches/go.sh <script> <out.png>`
runs a multi-line script file. Two torches in hand: `vr_walltorch_pull 0` (the grip alone takes one), 52 by the main
hand as above, then `vr_rigid_place 53 off 0 0 0; +graboff; vr_mock_button off grip 1` (taken from its wall at the off
hand); let go of with `vr_walltorch_die_time 0.3` it goes out; taken again the same way and held at `0.2 1.3 -0.4 70 0 0`
(the main at `0.2 1.2 -0.4`), `developer 1` prints `walltorch: lit again from a burning torch` (the kit's
`scratch/heldphys/torch.sh`).
Hands and weapons as bodies (ROUND21.md): `vr_debug_box3d 1` prints each reach body made (`main hand's reach body:
weapon at ... (its box), the palm facing ...`) and each swing's strike (`... strikes 199 ogre_grenade at 235 u/s: 340
u/s after`); `2` each frame's move and each contact. A palm turned up: the main hand `vr_mock_hand main 0.1 1.2 -0.45 0
-153 -90`, the off hand `vr_mock_hand off -0.1 1.2 -0.45 0 -13 90` (Gun Angle 70). A floating grenade to bat:
`vr_test_projectile 4; impulse 246; wait3; sv_gravity 0; vr_rigid_place ogre_grenade 296 -545 84` in the firing range
with the axe (`vr_weapon_grip_mode 1; impulse 9; impulse 152`) at `vr_mock_hand main 0.1 1.3 -0.45 70 0 0`, `vr_deflect
0`, and a `vr_mock_play` moving the hand to `0.6 1.3 -0.45` (the kit's `scratch/heldphys/bat.sh`, `speeds.sh`). The
two-handed grip: `vr_dumpview` prints each hand's `two-handed <0..1>, helping <0|1>, empty <0|1>` (`grip.sh`).
Pushes by mass (ROUND21.md): `vr_debug_box3d 1` also prints each prop a hand's body pushed (`199 ogre_grenade (40.0 kg)
hit by 4.0 kg: 189 u/s gained, x0.09: 17 u/s`; `shoved` for the steps after, capped by `vr_box3d_push_force`), the swing's
strike's share (`(x0.77 by mass)`) and a QC poke's (`that push by 3.0 kg against its 40.0 kg`). The same grenade
made heavy: `vr_prop_mass_04 40` before `vr_test_projectile 4` (the kit's `scratch/heldphys2/flick.sh <kg>`); an
explosive box ahead: `vr_test_spawn 102; vr_test_spawn_dist 34; impulse 241` punched with `poke.txt` (`box.sh`).
Held props meeting: `vr_debug_carry 1` prints `held: 199 and 198 meet` / `apart` and, each frame they touch, how deep
and how far each is drawn moved (`meet.sh`: a gib put in the main hand by `vr_rigid_place new main 0 0 0` and the
grip, a head in the off hand by `impulse 252`); `vr_debug_carry 2`'s `carry_trace.txt` has each drawn hand off its
controller. A held club never dropped on a monster: `mon.sh` (a gib made a club by `vr_prop_tip_x_34 4`, swung and
stabbed through a shambler 20 times; `impulse 252` at the end prints nothing while the hand still holds it).
The firing range's prop area (ROUND21.md, "Bricks in the palm; the grenade pouch's turn; the firing range's prop area"):
every rock and brick on a table, three wall torches and both explosive boxes: `map vrfiringrange; setpos -476 -760 41 0 -90 0`
stands at the table (`vr_debris_list` lists the pieces on it).
Rocks and bricks (ROUND21.md): `vr_debug_debris 1` prints a line per map (pieces, spots, rejections by reason, the
time, the layout's hash, the server's spawn time), `2` each piece (model, skin, place, turn, size, the way out of its
wall); `vr_debris_list [lit]` lists the pieces in the map with the light where each lies. The scratchpad's
`debris/view.py <log> <map> <pieces> [dist] [pitch]` turns a `vr_debug_debris 2` log into `setpos` commands looking at
pieces (`shoot.sh`, `evidence.sh` take screenshots; `perf.sh` the exclusive timings). Hold one: `vr_rigid_place vr_rock
main 0 0 0; +grabright; vr_mock_button main grip 1` (bricks: `vr_brick`, in a map that has them), with the off hand
out of the way; punches and throws: `debris/motions/punch2.mock`, `throw.mock` (`vr_mock_play`, the old hand settings
set as `motions/hc.txt`), a grunt from `vr_test_spawn 0; vr_test_spawn_dist 36; impulse 241` on flat ground
(`setpos 340 1350 -200 0 180 0; noclip` in e1m1). A worldspawn's `_vr_debris` without editing a map: a
`maps/<map>.ent` override (`external_ents`).
Align Sights to My Aim (ROUND21.md): `vr_sight_align [start [main|off] | apply | cancel | undo]` runs the Weapon Offsets page's capture (the mock hand must be lowered, then raised and held 0.4 s, for each capture; `vr_sight_align_captures`), `vr_sight_check [main|off] [size]` prints the sight line against the dominant eye (`vr_dominant_eye`), where the sights and the laser land in that eye's image, and the laser against the line; `vr_sight_lines` lists every weapon's line; `vr_show_sight_line 1` draws them. Higher eye images: `vr_mock_eye_size 2048; vr_restart`, then `vr_eyeshot 1`.
Held props' grips (ROUND21.md, "Held props: grip modes, live offsets, palm grip, torch handle"): `vr_grip_frame` prints
each hand's grip frame (the palm, its normal, the grip channel); `developer 1` prints `grip: taken|placed again ...` (the
mode, the place taken and held, the prop's axes in the hand, the settings generation and frame), `props: <cvar> <value>
(generation, frame)` for each prop setting changed, and `held: <ent> placed again ...` from the client. In e1m2: rock3
is entity 274, rock5 245, the half brick 220, a whole brick 214, the wall torch 53 (taken off its wall first:
`torches/fg.play` from `setpos 2047 -84 312 0 30 0`). One in the palm: `vr_rigid_place 274 main -1.5 1.4 -2 <pitch>
<yaw> <roll>; +grabright; vr_mock_button main grip 1` (no wait in between: it falls). Close cameras on the main hand at
`vr_mock_hand main 0.15 1.2 -0.45 70 0 0`: `vr_mock_camera 0.35 1.15 -0.45 25 90` (outside), `-0.05 1.12 -0.45 25 -90`
(the palm's side), `0.15 1.18 -0.68 30 180` (the front); `r_fullbright 1` lights them.

Flung props (ROUND21.md, "Hand grenades: unarmed look; flung props hit as thrown ones"): from `setpos 300 -440 45 0 180 0`,
a grunt at `vr_test_spawn 0; vr_test_spawn_dist 200; impulse 241`, `vr_physics_spawn item_health 40 40`, then
`vr_rigid_place item_health 220 -440 50 0 0 0 -600 0 0` flings it at the grunt; `developer 1` prints `prop: flung ...`.
An unarmed hand grenade's trail: `developer 1` prints `grenade <n> (progs/grenade.mdl, skin <s>): smoke trail on|off`.
Hand grenades from the back pouch (ROUND21.md, same title): the scratchpad's `handgren/` has the scripts and logs.
`gen.py` writes the `vr_mock_play` files (from `throw_plays.py`'s `throws.txt`: `python Misc/quakevr/throw_plays.py
--gunangle 70 --out throws.txt` first): a hand to the pouch at `vr_mock_hand main|off 0 1.0 0.2 0 0 0` (Gun Angle 70;
`vr_dumpview` prints `grenade pouch at ..., main hand <d> units off (hotspot 11)`), `+grabmain`/`+graboff` there takes
one, `+attack`/`+offhandattack` pulls the pin, the throw lets go with `-grabmain`/`-graboff`. `run1.sh <play> <png>
["<console commands>"] ["<spawn>"]` runs one in vrfiringrange from `setpos 190 -560 41 0 90 0` with 10 rockets and a
grunt 300 units ahead (`vr_test_spawn 0; vr_test_spawn_dist 300; impulse 241`); `runall.sh` runs them all into
`logs/`. `developer 1` prints `grenade: hand grenade taken from the pouch by hand <h>, <n> rockets left`, `the pouch
is empty`, `hand grenade armed (the pin pulled | the lever flies off ...)`, `a hand grenade let go of unarmed: a dud`,
`hand grenade put back in the pouch`, `an unarmed hand grenade back in the pouch at the level's end`, and the grenades'
own lines (`hits`, `goes off`, `went off in player's hand`); `vr_debug_shots 1` the damage. A dud rolls into the grate
at the start's feet (`edict <n>` prints where): reach it at `vr_mock_hand main 0.03 -0.14 0` (the mock's floor is 7
units over the map's there). The pouch from behind: `vr_mock_camera 0.25 1.15 0.95 8 15`; `give r 0` shows it empty.
The grenade's turn from the pouch (`vr_grenade_pouch_hold_*`): `developer 1` prints `grip: from the pouch <n> ...
its x ..., its z ...` at the take and `grip: placed again` as a slider moves. Carried pickups at the pouch:
`vr_rigid_place item_health main 0 3 0; +grabright; vr_mock_button main grip 1`, the hand to the pouch as above, then
`vr_mock_button main grip 0; -grabright`: `carry: into the pack` and the pickup's message.

Debug menu; quad sound; grenade catch default; no empty-hand deflection (ROUND21.md, same title): the scratchpad's
`misc23/` has the scripts and logs.
- `t.py N|W|Q` prints the console script and writes the play (it uses `projfix/t.py`'s poses and `r20/gen.py`):
  `N` empty hands (punch, two-palm shove, still fist, reach) and the shotgun (thrust, an off-hand shove beside it)
  against `impulse 246` spikes and ogre grenades; `W` projfix's bashes with a sword (old hand settings, set and printed;
  `EXTRA="vr_fixed_frames 1"` makes the numbers repeat to float noise, for A/B against a base build); `Q` Quad
  (`impulse 255`): the sword held with `+attack` and no swing, a sword blow at a grunt, the fist with `+attack`, a
  shotgun shot. `vr_debug_shots 1` (with `developer 1`) prints `quad sound: a shot | a blow landed | a bash or shove
  hurt | the hook bit`.
- `m.sh`: the config test. It writes a config as the previous version saved it (`vr_cfg_version "34"`,
  `vr_grenade_catch "1"`), runs a script, and plays another copy of the game from the shell: on a marker in
  `qconsole.log` it edits `vr_grenade_catch`, `vr_deflect` and `vr_melee_speed` in the file, while the game has set
  `vr_melee_speed 2.9` itself; `writeconfig` then prints `<cvar> "<value>": from another copy of the game (this one left
  it at "<value>")` for the first two and keeps 2.9. The markers carry the run's time (a previous run's log is still
  there while the kit waits for a slot).
- Menu buttons in a script: `menu_vr <page> <label prefix>` selects the row, then `vr_mock_button main primary 1`, a
  few frames, `0`. A button's command goes ahead of the script's waits (`Cbuf_InsertText`), so its output follows
  the click. The Debug pages are 64 and 66-71.
Hands, teleporters and climbing stamina (ROUND21.md, "Hands: both work; props through teleporters; climbing stamina";
the scratchpad's `climbhands/`): `vr_debug_hands 1` (2: every frame) prints each hand's state as `hands <time> <hand>:
...` lines, ending with what climbing makes of a grip; `vr_climb_debug 1` says why a grip is refused. The hand-state
matrix is `hands/gen.py` (writes the plays: an action, then both hands on vrclimb's ledge, a health box in each hand,
then in both) and `hands/mrun.sh <suffix> <action...>` (actions `base fgoff fgmain wpnmain wpnoff wpnkeep save climb`;
`summ.py` prints the verdicts). A force grab in the mock: point the hand at the thing, `+offhandattack` (off) or
`+attack` (main), move the hand up 0.3 m in 0.1 s, then the grip (`hands/fg_off.txt`). Teleporters: `tele/tele.sh <tag>
main|off|two|flash [start|e1m1] [edict] [what]` holds a thing (`vr_rigid_place new`: a brick in start) and walks
through start's skill teleporter `*2` (`setpos 544 1376 24 0 0 0`, a 90-degree turn) or e1m1's `*20` (walked into from
`setpos 1312 1030 -408 0 90 0`); `tele/torch.sh` takes e1m2's wall torch 52 through its teleporter `*1`; `walk.sh`
walks with a brick in both hands. Two hands on a brick in start: the main hand at `0.20 1.30 -0.45 70 0 0` with
`vr_rigid_place new main 0 0 0`, the off hand at `0.14 1.30 -0.56 70 0 0`. Climbing stamina: `stamina/st.sh <tag>
<play> [cvars] [waits]` (plays `hang`, `stand`, `regrab`, `gadget`; `vr_climb_debug 1` prints `climbstamina` lines
at every 10 spent, 2 every frame); `vr_gadget_screen_dump <name>` writes the gadget's screen. The 13 climb scripts:
`climb/set.sh <suffix>` (`EXTRA="vr_climb_stamina 0"` adds cvars) and `climb/cmp.sh A B`. `edict <n>` with a number past
the live edicts ends the game (`PR_SwitchQCVM: A qcvm was already active`): use numbers you have seen.

Hardcoded limits (ROUND21.md, "Hardcoded limits audit"): `vr_limits` (Debug > Reports > Limits) prints each limit's
usage, peak and maximum, highlighted from 80%. Overflows that used to be silent (temp entities, dynamic lights, the
datagram) print `Limit reached: ...` once a session. Stress tests: `vr_limits stress <n>` makes n more cvars
(`qvr_stress_00000`..., not saved), `vr_limits time` prints the milliseconds since the last one (`vr_limits time; exec
big.cfg; vr_limits time`), `vr_limits cvarlen <name>` gives a value's length (a print stops at 4095 characters). There
is no `set` command: a config line sets only an existing cvar. `vr_physics_spawn item_shells <dist> <left>` 320 times
in e1m1 moves 320 props (the datagram peaked at 16.5 KB of 64 KB).
