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
| A / X (primary) | `ABUTTON` | `XBUTTON` | `+jump` / `+reloadoff` |
| B / Y (secondary) | `BBUTTON` | `YBUTTON` | `impulse 10` / `impulse 12` (next weapon) |
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

**Flight** (the engine, `vr_rigid.cpp`):
- **Spin:** the hand's real spin, capped at `vr_throw_spin_max` (20 rad/s). It used to be applied as rates on
  each angle, which tumbled wildly.
- **Gravity:** the same true-scale gravity for the whole flight. It used to jump to Quake's 2.5 g after the first
  touch.
- **Bounces:** with `vr_throw_restitution` (0.25) and friction (`vr_throw_friction` 0.5).
- **Resting:** the weapon turns onto its nearest flat side (`vr_throw_settle_rate`) and stays still. No more
  wiggling.
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
  - **Physics engine: Box3D, to compare** (Throwing and Physics > Physics Engine, switches at once): Quake VR is the
    solver you know; Box3D makes thrown and dropped things collide with each other too: stack boxes, build a pyramid,
    throw a box into a stack, sweep one off with a held box. Then the usual: throws at monsters, weapons landing on
    their sides, backpacks and armour on slopes, boxes on lifts, things floating. The side-by-side list is at the end
    of ROUND21.md, "Box3D physics".
  - **Melee, redesigned:** swings in any direction (backswings too), stabs with the tip, pommel/butt strikes with
    the near end, parry bash = hold the stance ~0.5 s then push; palm shoves (both palms harder). Melee Speed was
    reset once to 4 m/s. Tell which of your motions still misread, and record more takes of them.
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
    Counter on the Gadget.
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
  - **Advanced VR Options** (bottom of VR Settings): your old Quake VR settings pages (everything that still
    exists, with the old ranges and help), plus Body, Throwing and Physics, and Force Grab pages.
    `menu_vr <n>` opens a page directly.
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
  - **Physics:** thrown weapons and backpacks are real rigid bodies. `vr_debug_throw 3` prints their state.
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
- **VR Settings:** Options > VR Settings (or `menu_vr`) has the comfort, body, weapon and display settings;
  the sticks move and change, A selects, B goes back. "Set Height Now" calibrates the height while standing.

`vr_status` shows tracking, hand angles, hotspots and grab and two-handed state; `vr_dumpview` shows the drawn
hands, weapons and finger curls.

## Profiling (CPU and GPU time per effect)

`vr_profile 1` (in the console) times each part of every frame, on the CPU and on the GPU (OpenGL timer queries,
read a few frames later, so measuring does not slow the frame down noticeably), per eye. Every
`vr_profile_interval` seconds (5; 0: only on demand) and on `vr_profile_dump` it writes the averages and the
worst frame of each part to `quakevr/profile/profile_<map>_<date>_<time>.csv` (one file per map, one block of rows
per interval; the header lines give the map, the eye resolution, the graphics preset and the graphics settings)
and prints a one-line summary with the costliest parts. `vr_profile_dump` also prints the whole tree.
`vr_profile 2` also shows the costliest parts over the wrist gadget. `vr_profile 0` (the default) stops it.

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
decals, lights, particles). Note the time when it feels slower and send that file (ROUND16.md, "Slowdown").

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
swings the main hand for throwing tests. `vr_mock_fingers <main|off> <trigger> <grip> [<thumb>]` sets the finger
sensors (0..1); a fifth argument sets the index finger's touch on the trigger.

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
Two-handed props: `vr_rigid_place item_health main 0 3 0` with `+grabright; vr_mock_button main grip 1` puts a health
box in the main hand; move the off hand to its other side (`vr_mock_hand off -0.19 1.30 -0.45 0 0 0` with the main
at `0.10 1.30 -0.45`) and press its grip: `carry: both hands` (developer 1). `vr_debug_carry 2` writes the object and
both hands every frame to `carry_trace.txt` (and prints the second hand's reach test); `vr_debug_throw 1` prints
`throw both hands (...)`. `vr_mock_play` keyframes move both hands with their velocities (throws, turns).
Put the off hand out of the way first (`vr_mock_hand off -0.35 1.1 -0.2 0 0 0`): at its default pose it touches the
box before the main hand (no `carry: taken`, and the box drops). Carrying across a save (round 21):
`save c1; wait10; load c1; wait60; screenshot`; the box should still be in the hand(s).
Held weapons against models (round 21): `vr_debug_model_collide 1` prints each hand's push, `2` draws the rays;
`vr_model_collide_bench [n] [list]` times the test, `vr_model_collide_bench probe` lists the model triangles a ray along
the view goes in and out by. `impulse 241` puts a monster (`vr_test_spawn`: the firing range dispenser's numbers) or a
box (100 health, 101 shells) `vr_test_spawn_dist` units ahead (`vr_test_spawn_dead 1`: a corpse);
`vr_mock_camera <x> <y> <z> <pitch> <yaw>` draws the mock eyes from elsewhere in the tracking space (a spectator's view of
your body; the hands stay with the head), `vr_mock_camera` alone puts them back.

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
Rigid bodies (round 21, Box3D): `vr_physics_engine 0|1` switches the solver; `vr_physics_stack`, `vr_physics_pyramid`,
`vr_physics_pile` put props (a number, a classname or `props`) in a column, a pyramid or toppling columns;
`vr_physics_loose` makes a hanging armour or a pickup a loose prop; `vr_physics_list` and `vr_physics_hash` print
them (the hash: determinism); `vr_debug_box3d 1|2`. `vr_forcegrabbable_return 0` keeps moved items from going back
to their places during a long test.

Tuning the body: `vr_show_hip_holsters 1`, `vr_show_upper_holsters 1`, `vr_show_shoulder_holsters 1` and
`vr_show_virtual_stock 1` mark where the holsters and the virtual stock's shoulders are (green while a hand is
there); move them with the `vr_*_offset_*` cvars.
