// vr_menu_rows.inc -- the settings' menu rows, each written once (design review 9's settings schema): its label, range,
// step, format, how far past the bar left and right go, help and choices. A page lists only `row(opt::<cvar>)` (and its
// headers); where it shows (its menu path) is the page it is on: `vr_menu_dump_paths` prints every row's, and
// Misc/quakevr/menu_paths.txt holds them for the scripts (the board maps). Made by Misc/quakevr/settings_schema.py from
// the pages' rows; edit it by hand now. A setting shown twice with different texts keeps its other row on its page.
//   QVR_TOGGLE(cvar, label, help)                                       Off / On
//   QVR_SLIDER(cvar, label, min, max, step, format, extent, help)       extent: noExt, ext() (four bars' widths past each
//                                                                       end; none below a bar from 0 up), ext(lo, hi)
//   QVR_CYCLE(cvar, label, help, {value, "label"}...)
//   ..._BYNAME: the cvar found by its name when the page is built (Cvar_FindVar: the row left out without it; Ironwail's
//   settings), not by its C++ variable. help: nullptr for none. The order is the pages' (the file each came from).

// vr_menu_ad_guns.inc: pageAdGuns
QVR_TOGGLE_BYNAME(vr_adgun_drops, "Soldiers Drop Their Guns",
    "Arcane Dimensions' armed soldiers drop their own gun as they die: single use, its rounds (or fuel) its "
    "own, never refilled; empty, it is still a club. Where Quake VR has a gun of its kind, its shots are "
    "ours (the grenade launcher's grenades, the rocket launcher's rockets, our tracers and impacts); its "
    "look and rate of fire stay its own. The plain soldier and enforcer drop the grunt's burst rifle and "
    "the enforcer's laser rifle. Off: they drop nothing.")
QVR_SLIDER_BYNAME(vr_adgun_fumigator_poison, "Poison Damage", 0.f, 30.f, 0.5f, "%.1f / s", ext(0.f, 200.f),
    "The poison a puff leaves on what it touches: its damage a second. 0: none.")
QVR_SLIDER_BYNAME(vr_adgun_fumigator_poisontime, "Poisoned For", 0.f, 15.f, 0.5f, "%.1f s", ext(0.f, 60.f), nullptr)
QVR_TOGGLE_BYNAME(vr_adgun_crossbow_anim, "Bolt Laid and String Animated",
    "A bolt lies in the groove while the crossbow is ready (morphing in after a shot) and is the one that "
    "flies; the string snaps forward and the limbs flex, then it is drawn back for the next.")
QVR_SLIDER_BYNAME(vr_adgun_crossbow_bolt_sound, "Next Bolt's Sound", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The soft shimmer as the next bolt morphs into the groove: its volume (0: none).")
QVR_TOGGLE_BYNAME(vr_admelee_drops, "Melee Monsters Drop Their Weapons",
    "The death fury drops its two swords, the death knight, zombie knight and death guard their swords, "
    "the hunter ogres their hammer and mace, the Dark Prince, the justice knight and the judicator their "
    "swords, the death sergeant its flail; Rogue's Phantom Swordsman falls as its sword: yours to swing (Weapon Offsets slots 41-46, "
    "47-51).")
QVR_SLIDER_BYNAME(vr_adgun_ad_copies, "AD's Flames Frequency", 0.f, 3.f, 1.f, "%.0f", noExt,
    "AD's flame and poison sprites drawn this many more times, between each and where it came from: a "
    "fuller stream (drawn only: the flames that burn stay AD's). 0: AD's alone.")
QVR_SLIDER_BYNAME(vr_adgun_fx_scorch, "Scorch Marks", 0.f, 3.f, 0.25f, "%.2fx", noExt,
    "Scorch marks where the flamethrowers' fire licks a wall or the floor: one where every flame is spent "
    "(under 1: that share of them; over 1: a little bigger). 0: none. Cheap: with Permanent Decals on, "
    "each is baked straight into them.")
QVR_SLIDER_BYNAME(vr_adgun_plasma_fx, "Plasma Bolt Arcs", 0.f, 3.f, 0.25f, "%.2fx", noExt,
    "Small lightning arcs crackling round the plasma bolts (the Plasma Gun's, the plasma soldiers' and "
    "centurions'). 0: none. Their blue light: Projectile Lights.")
QVR_TOGGLE_BYNAME(vr_adgun_hose, "Fuel Tank and Hose",
    "Holding the pyro's flamethrower or the fumigator's poison sprayer: a small fuel tank on your back and "
    "a hose from it to the gun, swinging as you move (seen in the body's mirror image too). Off: none.")
QVR_CYCLE(vr_adgun_hose_route, "Hose Route",
    "The hose's way from the tank to the gun, on the gun's hand's side: through a clip at the hip, or "
    "over the shoulder.",
    {0.f, "Hip"}, {1.f, "Shoulder"})
QVR_TOGGLE_BYNAME(vr_adgun_hose_holstered, "Holstered Guns' Hoses",
    "A holstered flamethrower or poison sprayer has its hose to the tank on your back too. Several carried, "
    "in the hands or the holsters: one tank, the last one held's colour, every gun's hose to it. Off: only "
    "the guns in your hands have a tank and hose.")
QVR_TOGGLE_BYNAME(vr_adgun_tank_world, "Tank Lying About",
    "A flamethrower or poison sprayer lying about (dropped, thrown, a monster's) has its tank beside it, a "
    "hose between them and a rope holding them together: grab and drag either and the other follows. A "
    "hand carries the tank (it can't be force grabbed). Taking the gun puts the tank back on your back; "
    "dropping it drops the tank too. Off: no tank lying about.")
QVR_SLIDER_BYNAME(vr_adgun_tank_rope, "Tank Rope", 0.4f, 1.6f, 0.05f, "%.2f m", noExt,
    "How far a tank lying about can be from its gun (the gun's grip to the tank's middle); its hose is "
    "that long and a little more.")
QVR_TOGGLE_BYNAME(vr_adgun_hosecut, "Hose Cut",
    "Slash a blade (a sword, an axe, a knife, the chainsaw) across the hose between a gun and its tank "
    "lying about to cut it: the gun is spent (it never fires again), the tank a prop of its own you can "
    "force grab, carry and throw, and shoot to burst with the fuel it kept (a thrown bomb, a trap). Both "
    "keep a short stub of the hose. Fists and blunt weapons (clubs, the hammer, the mace) don't cut it. "
    "Off: hoses can't be cut.")
QVR_TOGGLE_BYNAME(vr_adgun_tank_worn, "Armed Monsters' Tanks",
    "Armed monsters holding a flamethrower or sprayer wear its tank on their back, hosed to the gun. Shoot "
    "it enough (Tank Health) and it bursts on them: hurt, set on fire or poisoned, the field round them, and "
    "their gun spent: they drop it.")
QVR_TOGGLE_BYNAME(vr_adgun_bolt_fx, "Electric Bolts",
    "The plasma rifles' bolts (the soldiers', eliminators' and centurions', theirs and yours): a faint "
    "smoke and electric arcs and sparks along them, at the muzzle and where they strike. Off: AD's smoke.")

// vr_menu_ad_guns.inc: pageFlail
QVR_SLIDER_BYNAME(vr_flail_chain, "Flail Chain Length", 0.2f, 1.f, 0.05f, "%.2f m", ext(0.1f, 2.f),
    "The flail's chain, from the handle's end to its spiked head, which hangs and swings on it as a body of "
    "its own (it falls, bounces off walls, wraps a little round what it meets).")
QVR_TOGGLE_BYNAME(vr_flail_world, "Flail Head Lying About",
    "A flail lying about (dropped, thrown, a death sergeant's) keeps its head on its chain: a body of its "
    "own you can carry, dragged by the handle and dragging it. Off: the handle alone.")

// vr_menu_body.inc: pageBody
QVR_TOGGLE(vr_body_walk, "Walking Legs", "The legs (full body) walk as you move with the stick.")
QVR_SLIDER(vr_body_step_rate, "Step Rate", 1.f, 5.f, 0.1f, "%.1f /s", ext(),
    "How fast the legs step at most, in steps a second at full running speed (walking, somewhat fewer).")
QVR_SLIDER(vr_body_step_run, "Running Cadence", 0.f, 1.5f, 0.05f, "%.2fx", noExt,
    "At speed the legs step as often as a person's stride there (the bloody footprints' steps): a run looks "
    "like a run. 1: a person's cadence; 0: only the Step Rate above (a run's legs step half as often as its "
    "prints).")
QVR_SLIDER(vr_body_turn_step, "Turn Before Stepping", 15.f, 90.f, 5.f, "%.0f deg", ext(),
    "How far you turn over your planted feet before they step round to follow.")
QVR_SLIDER(vr_body_wade, "Wading Heaviness", 0.f, 2.f, 0.1f, "%.1fx", ext(),
    "Wading, the legs walk heavier: shorter, higher, slower steps (0: as on land).")
QVR_SLIDER(vr_body_swim_kick, "Swimming Kicks", 0.f, 2.f, 0.1f, "%.1fx", ext(),
    "Swimming, the legs trail behind and kick where the stick moves you: how wide (0: no kicks).")
QVR_SLIDER(vr_body_swim_kick_rate, "Swimming Kick Rate", 0.f, 4.f, 0.1f, "+%.1f /s", ext(),
    "How many more kicks a second at full stick (treading water, about 0.7).")
QVR_TOGGLE(vr_body_climb_feet, "Climbing Footholds",
    "Climbing, the feet find footholds below you (ledges, rungs, the wall) and step from one to the next as "
    "you climb; with none, they hang. Off: they hang as in the air.")
QVR_SLIDER(vr_body_climb_feet_reach, "Foothold Reach", 0.5f, 1.2f, 0.05f, "%.2fx", ext(),
    "How far below the hips the feet look for a foothold (1: down to nearly a straight leg).")
QVR_SLIDER(vr_body_climb_feet_speed, "Foothold Step Speed", 0.3f, 3.f, 0.1f, "%.1fx", ext(),
    "How fast a foot steps to a new foothold (1: about a third of a second).")
QVR_TOGGLE(vr_body_climb_feet_props, "Footholds on Props", nullptr)
QVR_TOGGLE(vr_body_state, "Show Armour and Wounds",
    "The armour you wear plates your torso; your arms and hands get bloodier as you are hurt (with Dynamic Wounds, on the Gore page: where you are hit).")
QVR_TOGGLE(vr_body_powerups, "Show Powerups",
    "Quad damage glows blue or sparks around your hands, the pentagram glows red, the biosuit green, the ring fades you.")
QVR_CYCLE(vr_quad_look, "Quad Damage Look",
    "Quad damage on you: a soft pulsing blue glow round your hands, arms and body, lighting the world round you "
    "blue; or Quake's electric arcs crackling over your hands and forearms (as when lightning strikes you); or both.",
    {1.f, "Blue Glow"}, {0.f, "Lightning Arcs"}, {2.f, "Both"})
QVR_TOGGLE(vr_body_anchors, "Anchors Follow Body",
    "Holsters, the virtual stock and hand collisions follow the body's lean and crouch.")
QVR_TOGGLE(vr_body_collide, "Body Collisions",
    "Your hands and the weapons in them stop at your other hand, your other arm, the wrist gadget and your "
    "body instead of passing through them. Push on and they pass through (Pass Through At). Drawn only: "
    "hits, shots, aim and grabs are still where your hands are.")
QVR_SLIDER(vr_body_collide_pass, "Pass Through At", 0.3f, 1.f, 0.05f, "%.2f", ext(0.05f, 1.f),
    "How far through what stops it your hand or weapon must be pushed before it lets go and passes "
    "through: 0.7, seven tenths of the way (and held out at most 10.5 cm); 1, only once all the way through. It "
    "stops again once clear.")
QVR_TOGGLE(vr_body_collide_elbows, "Elbows Out of the Torso",
    "With Body Collisions: an elbow that would go into your torso (a hand across your chest) swings out "
    "round the line from the shoulder to the wrist.")
QVR_TOGGLE(vr_bodytrack, "Body Tracking",
    "Where the headset tracks your body (Quest 3 through Virtual Desktop: allow Movement Tracking), your "
    "tracked torso, elbows and (if you like) legs are blended into the body; where it is lost or unsure, "
    "the guessed body comes back smoothly. Your hands always stay on the controllers.")
QVR_SLIDER(vr_bodytrack_arms, "Elbows", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much the elbows swing round to where your tracked elbows are.")
QVR_SLIDER(vr_bodytrack_arm_fit, "Elbow Fit", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How far the elbows go onto your tracked elbows themselves, not only round to their side: the shoulders "
    "follow your tracked shoulders and the arm's bones lengthen or shorten a little to put the elbow there. "
    "0: the swing only (the arm keeps its own shape).")
QVR_TOGGLE(vr_bodytrack_legs_walk, "Walk Over Tracked Legs",
    "With tracked legs: while you move (walking, running, in the air) the walk and run animation drives the "
    "legs over the tracked ones; standing still, the tracked legs show. Off: the tracked legs always.")
QVR_SLIDER(vr_body_torso_back, "Torso Offset", -0.2f, 0.4f, 0.01f, "%.2f m", ext(-1.f, 1.f),
    "How far the torso sits behind your neck (negative: in front).")
QVR_SLIDER(vr_body_legs_back, "Legs Offset", -0.2f, 0.4f, 0.01f, "%.2f m", ext(-1.f, 1.f),
    "How far the feet stand behind your head (negative: in front).")
QVR_SLIDER(vr_body_shoulders_back, "Shoulders Offset", -0.15f, 0.2f, 0.01f, "%.2f m", ext(-0.5f, 0.5f),
    "How far the shoulders sit behind your neck (negative: in front).")
QVR_SLIDER(vr_body_eye_forward, "Eyes Forward", 0.f, 0.25f, 0.01f, "%.2f m", ext(-0.1f, 0.5f), "From the top of the neck to the eyes, forward.")
QVR_SLIDER(vr_body_eye_up, "Eyes Up", 0.f, 0.25f, 0.01f, "%.2f m", ext(-0.1f, 0.5f), "From the top of the neck to the eyes, up.")
QVR_SLIDER(vr_body_crouch_tilt, "Crouch Tilt", 0.f, 80.f, 5.f, "%.0f deg", ext(),
    "How far the back tilts forward in a full crouch (the hips stay under you).")
QVR_SLIDER(vr_body_crouch_pose_strength, "Crouched Pose Strength", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 3.f),
    "Times the crouched pose's offsets (0: none, the body crouches as before; 1: as set).")
QVR_SLIDER(vr_body_crouch_pose_curve, "Crouched Pose Curve", 0.25f, 3.f, 0.05f, "%.2f", ext(0.1f, 10.f),
    "How the offsets come in as you crouch: 1 evenly (half a crouch, half of them); over 1 little until you "
    "are deep; under 1 most of them early.")
QVR_CYCLE(vr_body_crouch_preview, "Preview Crouch",
    "The crouched pose blended as if you crouched this deep, standing or not (the crouch itself as you are): "
    "to tune it with the body in front of you (Debug: Show Body Skeleton) or looking down. Off: as deep as "
    "you crouch. Not saved.",
    {-1.f, "Off"}, {0.f, "Standing"}, {0.25f, "Quarter"}, {0.5f, "Half"}, {0.75f, "Three Quarters"}, {1.f, "Full"})
QVR_CYCLE(vr_death_view, "Death View",
    "When you die (not gibbed) your body falls as a ragdoll (also on VR Settings, Comfort). Off: no body. "
    "Third Person: your view stays where your eyes were. Immersive: your view goes into your body's head "
    "(Third Person while a menu is open).",
    {0.f, "Off"}, {1.f, "Third Person"}, {2.f, "Immersive"})
QVR_TOGGLE(vr_death_view_vomit, "Vomit Mode",
    "Warning: very likely to make you sick. When you die, you see straight out of your ragdoll's eyes, "
    "turned exactly as its head is as it tumbles and rolls: no smoothing, no comfort, and your own head's "
    "turns do nothing. Overrides the Immersive rows below; needs Death View on (Third Person or "
    "Immersive). Too much: open the menu (it takes the view out) or close your eyes.")
QVR_TOGGLE(vr_death_view_turn, "Immersive: Turn With the Body",
    "Immersive Death View: your view turns sideways as your body's head does, slowly (Turn Speed). Off: "
    "only its place follows the head; you turn yourself.")

// vr_menu_body.inc: pageBodyCrouch
QVR_SLIDER(vr_body_crouch_elbow_out, "Elbows Out", -1.f, 1.f, 0.05f, "%+.2f", ext(-3.f, 3.f),
    "Crouched, the elbows point further out (negative: in): added to Arms and Pauldrons' Elbow Out.")
QVR_SLIDER(vr_body_crouch_elbow_back, "Elbows Back", -1.f, 1.f, 0.05f, "%+.2f", ext(-3.f, 3.f),
    "Crouched, the elbows point further back (negative: forward): added to Elbow Back.")

// vr_menu_body.inc: pageSeated
QVR_CYCLE(vr_bodycal_seated, "Position",
    "Seated: the game stands you up (your eyes at your standing Height), the body stands with its legs on "
    "the floor, the holsters and pouches move where a seated player reaches them, and a hand reaching low "
    "goes on down to the floor. VR Calibration and Body Calibration take you seated too.",
    {0.f, "Standing"}, {1.f, "Seated"})
QVR_SLIDER(vr_seated_height, "Seated Height", 0.8f, 1.5f, 0.01f, "%.2f m", ext(0.f, 2.f),
    "Your eyes' height sitting (VR Calibration measures it seated; 0: not measured, 0.74 of your standing "
    "Height). The game raises you by your standing Height less this.")
QVR_TOGGLE(vr_seated_stand, "Stand Up in the Game",
    "Seated, your eyes at your standing height in the game. Off: as low as they really are (crouched).")
QVR_TOGGLE(vr_seated_reach, "Reach the Floor",
    "Seated, a hand reaching down beside the chair goes on down faster, to the game's floor: pick things up "
    "without leaving the chair.")
QVR_SLIDER(vr_seated_reach_start, "Reach: Starts At", 0.3f, 0.9f, 0.01f, "%.2f m", ext(0.1f, 1.5f),
    "How high over your real floor a hand starts going down faster (keep it under your thighs and the hip "
    "holsters).")
QVR_SLIDER(vr_seated_reach_floor, "Reach: Floor At", 0.f, 0.4f, 0.01f, "%.2f m", ext(0.f, 1.f),
    "How high over your real floor a hand is at the game's floor.")
QVR_SLIDER(vr_seated_crouch_depth, "Crouch Depth", 0.f, 1.f, 0.05f, "%.2f m", ext(0.f, 1.5f),
    "Seated, how far vr_crouch_toggle (bind it to a button) crouches you. Ducking in the chair crouches "
    "too, as far as your head goes down.")
QVR_CYCLE(vr_seated_crouch_button, "Crouch Button",
    "Seated, a thumbstick press that crouches and stands you up (vr_crouch_toggle). It does only this in "
    "the game, never its bound key.",
    {0.f, "Off"}, {1.f, "Left Thumbstick Press"}, {2.f, "Right Thumbstick Press"})

// vr_menu_body.inc: pageBodyArms
QVR_SLIDER(vr_body_tweak_upper_arm, "Upper Arm", -10.f, 10.f, 0.5f, "%+.1f cm", ext(-30.f, 30.f),
    "Longer (negative: shorter) than measured, real cm: the shoulder joint to the elbow. 0: as Body "
    "Calibration measured it (uncalibrated: the default body's arm, times Arm Length).")
QVR_SLIDER(vr_body_tweak_forearm, "Forearm", -10.f, 10.f, 0.5f, "%+.1f cm", ext(-30.f, 30.f),
    "Longer (negative: shorter) than measured, real cm: the elbow to the drawn hand's wrist. 0: as Body "
    "Calibration measured it (uncalibrated: the default body's forearm, times Arm Length).")
QVR_SLIDER(vr_body_arm_length, "Arm Length", 0.7f, 1.4f, 0.01f, "%.2fx", ext(0.5f, 2.f),
    "Uncalibrated: the default body's arms (for your height), times this. Once Body Calibration "
    "has measured yours, it isn't used.")
QVR_SLIDER(vr_body_arm_stretch, "Arm Stretch", 1.f, 1.5f, 0.05f, "%.2fx", ext(1.f, 3.f),
    "How far the drawn arms may stretch to reach a hand beyond them (1: not at all). Calibrated, only past "
    "your measured reach and the Shoulder Reach (tracking glitches, a lunge); uncalibrated, before the "
    "shoulders reach.")
QVR_SLIDER(vr_body_shoulder_reach, "Shoulder Reach", 0.f, 0.25f, 0.01f, "%.2f m", ext(0.f, 0.6f),
    "How far the shoulders may move out towards a hand the arm can't reach, metres (not measured).")
QVR_SLIDER(vr_body_tweak_shoulders_back, "Shoulders Back", -0.1f, 0.1f, 0.005f, "%+.3f m", ext(-0.5f, 0.5f),
    "The shoulder joints further back (negative: forward) than measured, metres. 0: as Body Calibration "
    "measured them (uncalibrated: the default body's).")
QVR_SLIDER(vr_body_tweak_shoulders_up, "Shoulders Higher", -0.1f, 0.1f, 0.005f, "%+.3f m", ext(-0.5f, 0.5f),
    "The shoulder joints higher (negative: lower) than measured, metres. 0: as measured (uncalibrated: "
    "the default body's).")
QVR_SLIDER(vr_body_tweak_shoulders_out, "Shoulders Wider", -0.1f, 0.1f, 0.005f, "%+.3f m", ext(-0.3f, 0.3f),
    "Each shoulder joint further out (negative: in) than measured, metres. 0: as measured "
    "(uncalibrated: the default body's).")
QVR_SLIDER(vr_body_tweak_shoulder_rise, "Shoulder Rise", -30.f, 30.f, 1.f, "%+.0f deg", ext(-90.f, 90.f),
    "Degrees more (negative: fewer) the shoulders rise as you reach up than measured. 0: as measured "
    "(uncalibrated: the default body's 25).")
QVR_SLIDER(vr_body_tweak_shoulder_swing, "Shoulder Swing", -30.f, 30.f, 1.f, "%+.0f deg", ext(-90.f, 90.f),
    "Degrees more (negative: fewer) the shoulders swing forward as you reach far forward than measured. "
    "0: as measured (uncalibrated: the default body's 20).")
QVR_SLIDER(vr_body_forearm_twist, "Forearm Twist", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "Share of the wrist's roll the middle of the forearm follows (none at the elbow, all at the wrist).")
QVR_SLIDER(vr_body_wrist_limits, "Wrist Limits", 0.f, 2.f, 0.05f, "%.2f", noExt,
    "The elbow swings round to keep the wrist within a real one's bend and roll, times this (0: never).")
QVR_SLIDER(vr_body_elbow_lift, "Elbow Stays Clear", 0.f, 8.f, 0.5f, "%.1f", noExt,
    "How much that swing keeps the elbow out of the torso and from crossing the chest by the neck (a hand "
    "near the face or the chest), times this (0: free, as before).")
QVR_SLIDER(vr_body_elbow_spread, "Elbows Spread", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "With the hand at the face or the chest and the palm down, how far the elbow turns out to the side "
    "(wings). Palm up or on its side, it stays tucked in (0: as before).")
QVR_SLIDER(vr_body_elbow_out, "Elbow Out", 0.f, 1.f, 0.05f, "%.2f", noExt, "Where the elbow points: down, plus this much outwards.")
QVR_SLIDER(vr_body_elbow_back, "Elbow Back", 0.f, 1.f, 0.05f, "%.2f", noExt, "Where the elbow points: down, plus this much backwards.")
QVR_SLIDER(vr_body_elbow_hand, "Elbow From Hand", 0.f, 1.f, 0.05f, "%.2f", noExt, "How much the elbow points away from the back of the hand.")
QVR_SLIDER(vr_body_elbow_tuck, "Elbow Tuck", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "With the hand at the face or the chest (aiming down the sights, an axe raised by the face), the "
    "elbow stays down and back by the ribs instead of swinging out or across (0: as before).")
QVR_SLIDER(vr_body_elbow_tuck_back, "Tucked Elbow Back", 0.f, 2.f, 0.05f, "%.2f", noExt,
    "Where a tucked elbow points: down, plus this much backwards (more lets it go behind the chest).")
QVR_SLIDER(vr_body_elbow_tuck_near, "Tuck Within", 0.3f, 1.f, 0.05f, "%.2f arm", noExt,
    "Fully tucked with the wrist this close to the shoulder (times the arm's length).")
QVR_SLIDER(vr_body_elbow_tuck_far, "Tuck Fades By", 0.4f, 1.2f, 0.05f, "%.2f arm", noExt,
    "Not tucked at all with the wrist this far from the shoulder (times the arm's length).")
QVR_SLIDER(vr_body_pauldron_follow, "Pauldron Follows Arm", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much the shoulder cap turns with the upper arm (the lower plates follow the arm fully).")
QVR_SLIDER(vr_body_pauldron_forward, "Pauldron Forward", -0.08f, 0.08f, 0.005f, "%.3f m", ext(-0.3f, 0.3f), nullptr)
QVR_SLIDER(vr_body_pauldron_up, "Pauldron Up", -0.08f, 0.08f, 0.005f, "%.3f m", ext(-0.3f, 0.3f), nullptr)
QVR_SLIDER(vr_body_pauldron_out, "Pauldron Out", -0.08f, 0.08f, 0.005f, "%.3f m", ext(-0.3f, 0.3f), nullptr)

// vr_menu_body.inc: pageBodyCalibration
QVR_TOGGLE(vr_bodycal_track, "Use Body Tracking",
    "Where body tracking data comes in during the poses (Quest 3 through Virtual Desktop), your tracked "
    "shoulders' width and arm bones are blended into the measurements, so that the tracked elbows fit the "
    "arm without stretching it. A tracked length far off ours is left out. Taken at the next calibration, "
    "or a refit.")
QVR_SLIDER(vr_bodycal_track_weight, "Body Tracking Weight", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much of the tracked lengths goes into the measurements (0: ours alone, 1: the tracking's). The "
    "headset may scale its skeleton to your height, so ours (from the controllers) keep a share.")
QVR_CYCLE(vr_bodycal_preview, "Body in Front",
    "Your body in front of you while this page shows a result, moving as you do.",
    {0.f, "Off"}, {1.f, "Facing You"}, {2.f, "From the Side"})

// vr_menu_body.inc: pageFlashlight
QVR_CYCLE(vr_flashlight_beam_quality, "Beam Quality",
    "How closely the visible beam fades where walls cut it. Higher looks for them more often, costing more time each frame.",
    {0.f, "Low"}, {1.f, "Medium"}, {2.f, "High"})
QVR_TOGGLE(vr_flashlight_shadows, "Casts Shadows",
    "Its light casts shadows (takes one of the shadowed dynamic lights).")
QVR_CYCLE(vr_flashlight_cord, "Cord",
    "The retracting cord from the clip on your belt to the torch while it is off the belt. Chain: a rusty low-poly iron chain (chunky links of square bar, flat-shaded: Quake's look). Coiled: a springy coiled cord like an old telephone's, its turns opening out as you pull it. Both sag and swing as your hand moves. None: no cord drawn.",
    {0.f, "None"}, {1.f, "Chain"}, {2.f, "Coiled"})
QVR_TOGGLE(vr_flashlight_flick, "Flick to Turn Over",
    "A sharp flick of your wrist up or down turns the torch in your hand over, as B or Y does (the beam out "
    "past your thumb or out past your little finger). Not by a gun or your head (B or Y clips it on there).")
QVR_SLIDER(vr_flashlight_flick_speed, "Flick Strength", 300.f, 1500.f, 50.f, "%.0f deg/s", ext(100.f, 3000.f),
    "How fast your wrist must turn up or down to count. Higher: only sharper flicks (fewer by accident).")
QVR_SLIDER(vr_flashlight_grab_range, "Grab Range", 0.5f, 3.f, 0.1f, "%.1fx", ext(0.25f, 4.f),
    "How far from the torch your hand reaches it (it lights up, a grip takes it): times 9 cm from its middle line. Higher is more lenient.")
QVR_SLIDER(vr_flashlight_head_range, "Head Clip Range", 0.5f, 3.f, 0.1f, "%.1fx", ext(0.25f, 4.f),
    "How far from your temple the held torch clips on your head, and a hand takes it off there: times Head Zone Radius (On a Gun or Head).")
QVR_SLIDER(vr_flashlight_gun_range, "Gun Clip Range", 0.5f, 3.f, 0.1f, "%.1fx", ext(0.25f, 4.f),
    "How far from its place on the gun in your other hand the held torch clips on it (B or Y, or letting go with Clip on Gun When Let Go): times Gun Zone Radius (On a Gun or Head).")
QVR_TOGGLE(vr_flashlight_auto_head, "Clip on Head When Let Go",
    "Let go of the torch where it lights up by your head and it clips on there, no B or Y needed.")
QVR_TOGGLE(vr_flashlight_auto_gun, "Clip on Gun When Let Go",
    "Let go of the torch where it lights up by the gun in your other hand and it clips on the gun, no B or Y needed.")
QVR_SLIDER(vr_flashlight_clip_time, "Clip-On Transition", 0.f, 0.5f, 0.01f, "%.2f s", ext(0.f, 1.f),
    "How long the torch takes to move and turn from where it was onto the gun or your head as it clips on. It is on at once (it follows the gun, lights where it aims); only how it is drawn eases in. 0: it jumps there.")
QVR_SLIDER(vr_flashlight_tilt, "Lean Out", -90.f, 90.f, 1.f, "%.0f deg", ext(),
    "How far the stored torch, hanging on your belt lens down, leans its lens out from your body.")
QVR_SLIDER(vr_flashlight_forward, "Forward", -0.3f, 0.3f, 0.005f, "%.3f m", ext(), nullptr)
QVR_SLIDER(vr_flashlight_up, "Up", -0.4f, 0.4f, 0.01f, "%.2f m", ext(), nullptr)
QVR_SLIDER(vr_flashlight_out, "Out", -0.3f, 0.3f, 0.01f, "%.2f m", ext(), "Away from your middle, towards its side.")

// vr_menu_body.inc: pageFlashlightMounts
QVR_TOGGLE(vr_show_flashlight_zones, "Show Flashlight Zones",
    "Draws where holding the torch clips it on (yellow balls at your temples and forehead, an orange ball on "
    "each gun, where the torch sits on it) and the held torch's middle (white), which they measure. Green: in reach, B or Y clips it on (on "
    "your head: a hand there takes it off). The torch not in a hand: a capsule round it, green while a "
    "hand's grip there takes it, and a dot where the game reads each hand near it (the hand may be drawn "
    "elsewhere).")
QVR_SLIDER(vr_flashlight_head_zone_forward, "Head Zone Forward", -0.2f, 0.2f, 0.005f, "%.3f m", ext(),
    "Where the torch held at your head lets B or Y clip it on, and where a hand takes it off (B or Y, or the grip): "
    "balls at your temples and forehead, moved forward, up and out. Apart from where it then sits (On Head).")
QVR_SLIDER(vr_flashlight_head_zone_up, "Head Zone Up", -0.2f, 0.2f, 0.005f, "%.3f m", ext(), "The default is 0.150 (15 cm over your eyes' height).")
QVR_SLIDER(vr_flashlight_head_zone_out, "Head Zone Out", -0.2f, 0.2f, 0.005f, "%.3f m", ext(),
    "Away from your head: to the side at the temples, ahead at the forehead.")
QVR_SLIDER(vr_flashlight_head_zone_radius, "Head Zone Radius", 0.02f, 0.3f, 0.005f, "%.3f m", ext(),
    "How far from those places the torch's middle (or the fist taking it off) may be.")

// vr_menu_gameplay.inc: pageGameplay
QVR_TOGGLE(vr_enemy_liquid_damage, "Enemies Hurt by Liquids",
    "Monsters burn in slime/lava and drown after 12 seconds with their heads underwater, including knocked-down ragdolls. Fish, bosses and lava dwellers are immune.")
QVR_TOGGLE(vr_monster_lerp_continue, "Smooth Monster Steps",
    "A monster's next step is drawn on from where it is drawn, not from where its last step ends: no jump "
    "when it moves again before the last step is drawn out (a dog's quick turns looked like teleports). "
    "Off: Quake's drawing.")
QVR_TOGGLE(vr_ogre_aim_height, "Ogres Aim Grenades Up and Down",
    "Ogres (and zombies throwing flesh) lob at your height, on a ledge above them or a floor below, on an arc at "
    "their throw's own speed. Off: Quake's lob, which always flies as if you stood level with them.")
QVR_CYCLE_BYNAME(vr_enemy_drops, "Enemy Weapon Drops",
    "Controls random enemy weapon drops. 'Eligible' means that the player has obtained a weapon before through a level weapon pickup.",
    {0.f, "When Eligible"}, {1.f, "Always"}, {2.f, "Disabled"})
QVR_CYCLE_BYNAME(vr_ammobox_drops, "Ammo Box Weapon Drops",
    "Controls random ammo box weapon drops. 'Eligible' means that the player has obtained a weapon before through a level weapon pickup.",
    {0.f, "When Eligible"}, {1.f, "Always"}, {2.f, "Disabled"})
QVR_TOGGLE(vr_explosion_rumble, "Explosion Rumble", "Explosions near you rumble in your hands.")
QVR_TOGGLE(vr_heartbeat, "Low Health Heartbeat", "A heartbeat in your hands when your health is low.")

// vr_menu_gameplay.inc: pageEnemyShoves
QVR_TOGGLE(vr_enemy_shove, "Enemy Shoves",
    "Grunts and enforcers shove you away, pushing their gun out with both hands, when you stand too close "
    "for a moment; vores swing their arms at you. Never while they fire, are staggered or knocked away.")
QVR_SLIDER(vr_enemy_shove_range, "Too Close", 36.f, 96.f, 2.f, "%.0f units", ext(32.f, 160.f),
    "How close you must be (between your middles, flat: touching is 32 units) for them to shove you.")
QVR_SLIDER(vr_enemy_shove_delay, "Delay", 0.f, 3.f, 0.1f, "%.1f s", ext(0.f, 10.f),
    "How long you must stay that close before they shove (the shove's wind-up then takes 0.3 s).")
QVR_SLIDER(vr_enemy_shove_cooldown, "Cooldown", 0.f, 10.f, 0.5f, "%.1f s", ext(0.f, 30.f), "From one shove to the same enemy's next, parried or not.")
QVR_SLIDER(vr_enemy_shove_damage, "Damage", 0.f, 20.f, 1.f, "%.0f", ext(0.f, 50.f),
    "A shove's damage, cut by a parry as any blow's (Parry and Bash). 0: none, and it can't be parried.")
QVR_SLIDER(vr_enemy_shove_distance, "Push Distance", 0.f, 160.f, 4.f, "%.0f units", ext(0.f, 400.f),
    "How far a shove pushes you (a quick slide; 32 units is about a metre). Not scaled by Knockback.")
QVR_CYCLE_BYNAME(vr_agrunt_shove, "Armed Humanoids' Shove",
    "How armed grunts and enforcers shove you: posed (the Shove pose's keys, per weapon class: the off "
    "hand pushing beside a one-handed gun, a long gun pushed across the body in both hands, the palms "
    "out holding two weapons or the fists), with guns only (the default; melee and fist humanoids just "
    "strike) or with every weapon and their fists; or as before (their model's shove frames, the arms held as they aim, guns only).",
    {0.f, "Model Frames, Guns"}, {1.f, "Posed, Guns"}, {2.f, "Posed, Every Weapon"})
QVR_TOGGLE(vr_enemy_shove_vore, "Vores Shove",
    "Vores shove you too, swinging their arms down at you: the hardest shove of all. They reach a little "
    "farther (they are wider).")

// vr_menu_gameplay.inc: pageEnemyMelee
QVR_TOGGLE(vr_enemy_melee_arc, "Melee Arc",
    "A monster's melee blow (a sword, a bite, a claw, a chainsaw) lands only if you are inside the arc ahead "
    "of it when it strikes. Facing away, it turns to you before it attacks; while swinging it turns after "
    "you only slowly (Swing Turn), so stepping aside or behind it dodges the swing. Leaps still hit by "
    "touch. Off: Quake's blows, which check only reach (they hit you behind or beside it).")
QVR_TOGGLE(vr_enemy_melee_arc_show, "Show Melee Arcs",
    "Debug: each melee monster after you (within 600 units) draws on the floor its arc and reach as they "
    "are now (a fan; its sweeping blows' wider fan once it has swung one), red with you inside it (its blow "
    "would land), orange inside only a sweeping blow's, green outside. Its kind, arc and reach above it.")
QVR_TOGGLE(vr_enemy_aim_show, "Show Aim Cones",
    "Debug: each ranged monster after you (within 1000 units) draws its aim cone ahead of it, red with you "
    "inside (its shot comes straight at you), green outside, and its fire cone past it, orange with you "
    "there (it fires at once, bent toward you only as far as its aim cone). Outside both it waits, turning. "
    "Its kind and cones above it.")
QVR_TOGGLE(vr_enemy_melee_kind, "Kind Multipliers",
    "Some kinds' own arcs on top of their size: a knight's quick sword thrust is narrower (0.85: 55 deg), so "
    "it is easier to sidestep than an ogre's chainsaw. Off: size alone.")
QVR_SLIDER(vr_enemy_melee_space, "Melee Space", 0.f, 48.f, 2.f, "%.0f units", noExt,
    "A monster attacking you in melee (a knight's swing and its run past you, an ogre's chainsaw, a "
    "fiend's claws) is kept this far from your head, pushed back smoothly, so it never fills your view and "
    "you can parry. Its blows still reach you. 0: off (it charges in until your bodies meet).")
QVR_TOGGLE(vr_enemy_aim_cone, "Ranged Aim Cone",
    "A monster's ranged shot (a grunt's, an enforcer's laser, an ogre's grenade, a scrag's spit) leaves along "
    "its facing, bent toward you at most its aim cone, so circling or sidestepping a monster still turning "
    "makes it miss. With you far outside its fire cone, it waits a moment, turning, before it fires. Up "
    "and down it still aims at you. Homing shots and bosses are left alone. Off: Quake's shots, straight "
    "at you whatever way it faces.")
QVR_SLIDER(vr_enemy_aim_narrow, "Hitscan and Fast", 0.25f, 4.f, 0.05f, "x%.2f", noExt,
    "Both cones' multiplier for hitscan and fast shots (grunts, enforcers, gremlins, scourges, the "
    "expansions' gunners).")
QVR_SLIDER(vr_enemy_aim_wide, "Lobbed and Slow", 0.25f, 4.f, 0.05f, "x%.2f", noExt,
    "Both cones' multiplier for lobbed and slow shots (an ogre's grenade, a zombie's flesh, a scrag's spit, "
    "a death knight's magic): they are easier to see coming, so they may bend further.")

// vr_menu_gameplay.inc: pageHoldingStruggle
QVR_SLIDER(vr_struggle_strength, "Strength", 0.f, 2.f, 0.05f, "%.2f", noExt, "Times every struggling muscle.")
QVR_SLIDER(vr_struggle_speed, "Speed", 0.3f, 2.f, 0.05f, "%.2f", noExt, "Times the rate of its swings, tugs and kicks.")
QVR_SLIDER(vr_struggle_swat, "Swat at You", 0.f, 2.f, 0.05f, "%.2f", noExt,
    "Held by the body or a shoulder: an arm winds up and swings at your head or the hand holding it, the "
    "other reaching. 0: never.")
QVR_SLIDER(vr_struggle_flail, "Flail", 0.f, 2.f, 0.05f, "%.2f", noExt,
    "Its free legs' kicks and its body's and head's thrashing, under the moves above (more off the floor).")
QVR_SLIDER(vr_struggle_frantic, "Slap at Your Hands", 0.f, 2.f, 0.05f, "%.2f", noExt,
    "Held by the head, the legs or an arm: its free hands slap frantically at the hands holding it, drawn "
    "back and struck in, erratic. 0: they only tug and grasp.")
QVR_SLIDER(vr_struggle_squirm, "Squirm", 0.f, 2.f, 0.05f, "%.2f", noExt,
    "Held by the head or the legs: its body squirms, more thrashing over Flail.")
QVR_SLIDER(vr_struggle_look, "Look at You", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "Its head turns to look at you, as far as its neck goes: straight at you with its belly towards you or "
    "held up before you, to the side you are on when turned away. Not while you hold its head. 0: never.")
QVR_SLIDER(vr_struggle_look_strength, "Look Strength", 0.f, 0.9f, 0.05f, "%.2f", noExt,
    "Its neck's strength while it looks at you (times its effort: weaker as it tires or is hurt).")
QVR_SLIDER(vr_struggle_upright, "Stay Upright", 0.f, 2.f, 0.05f, "%.2f", noExt,
    "Dragged by an arm: it fights to stay up, its legs under it and its chest righted, rather than flopping "
    "down. 0: it flops.")
QVR_SLIDER(vr_struggle_arm_reach, "Arm Reach", 0.f, 175.f, 5.f, "%.0f deg", noExt,
    "Its shoulders' range while it struggles, for its hands to come over its head (at your hand on its "
    "head). 0: its body's own (no higher than its shoulders).")
QVR_SLIDER(vr_struggle_variation, "Variation", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much each enemy's rhythm, favourite arm and aim differ from another's. 0: all alike.")
QVR_SLIDER(vr_struggle_bursts, "Bursts", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "Struggling in fits: bursts of effort, quicker and harder, then a lull. 0: steady.")
QVR_SLIDER(vr_struggle_fatigue, "Tires After", 0.f, 60.f, 1.f, "%.0f s", noExt,
    "Seconds of struggling until it is worn out. 0: it never tires.")
QVR_SLIDER(vr_struggle_tired, "Effort When Tired", 0.f, 1.f, 0.05f, "%.2f", noExt, "Its effort left once worn out.")
QVR_SLIDER(vr_struggle_blend, "Blend Time", 0.05f, 1.f, 0.05f, "%.2f s", noExt,
    "How quickly it changes from one struggle to the next as your hold changes (lifted, put down, dragged).")
QVR_SLIDER(vr_struggle_held_share, "Held Limb Keeps", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The joints above the part you hold in its limb (the hip of a shin you hold) keep this share of their "
    "strength: it bends at the hip against you. 0: the held limb hangs slack.")
QVR_TOGGLE(vr_struggle_debug, "Show Struggle",
    "A panel over each struggling enemy: its behaviours' weights, its effort and fatigue, what you hold, "
    "how much lies on the floor, its drag, which way up it is, the hold's lag. Also Debug > Tests > "
    "Holding Enemies.")

// vr_menu_gameplay.inc: pageHoldingEnemies
QVR_TOGGLE(vr_foegrab, "Holding Enemies",
    "Experimental. Grip with an empty hand touching a living enemy to hold on to that spot of it: the hand "
    "stays on it as it moves, and the enemy is slowed and can be pulled a little. Small ones are held well, "
    "big ones (fiends, ogres) a little, huge ones (shamblers, vores) hardly. Let go of the grip, or pull too "
    "far, and the hand lets go. Grabbing a sleeping or idle enemy wakes it. Needs Precise Hit Detection.")
QVR_SLIDER(vr_foegrab_leniency, "Touch Leniency", 0.f, 5.f, 0.25f, "%.2f cm", ext(0.f, 20.f),
    "How far off the enemy's model your fist may be and still take hold.")
QVR_SLIDER(vr_foegrab_break, "Let Go Beyond", 10.f, 80.f, 1.f, "%.0f cm", ext(5.f, 200.f),
    "Your hand this far from the spot it holds (the enemy ran, or you moved away) lets go: after a quarter "
    "of a second there, or at once at twice this.")
QVR_CYCLE_BYNAME(vr_foegrab_by_mass, "Hold By",
    "Size Class: the three holds below, by kind of enemy. Mass: from the enemy's mass, full at Light and "
    "none at Heavy.",
    {0.f, "Size Class"}, {1.f, "Mass"})
QVR_SLIDER(vr_foegrab_slow, "Slow Down", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "Share of a fully held enemy's own movement (and turning) taken off, times its hold.")
QVR_SLIDER(vr_foegrab_follow, "Follow the Hand", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "Share of your holding hand's own movement the enemy follows, times its hold: pull your hand back, or "
    "walk off holding it, and a grunt comes along, an ogre a little, a shambler hardly.")
QVR_SLIDER(vr_foegrab_drag, "Pull", 0.f, 20.f, 0.5f, "%.1f /s", noExt,
    "How quickly your hand pulls the spot it holds towards it, times the hold (0: not at all): takes up "
    "what Follow the Hand leaves.")
QVR_CYCLE_BYNAME(vr_foegrab_drag_onehand, "One Hand Drags By",
    "Where one hand alone (the other off it) drags a standing enemy along by Follow the Hand and Pull. "
    "Torso, Shoulders: a hand on its legs only pulls its legs out (Leg Grabs, One-Hand Pull), on its head "
    "only holds it, on a forearm or hand neither. Anywhere: as before.",
    {1.f, "Torso, Shoulders"}, {2.f, "Torso, Shoulders, Head"}, {0.f, "Anywhere"})
QVR_CYCLE_BYNAME(vr_foegrab_shove, "Held Enemy's Shove",
    "Resisted: a grunt or enforcer you hold barely pushes you (by Shove Resistance and its hold), and comes "
    "along with you. Breaks Free: its shove pushes you fully, as ever, and tears it from your hands.",
    {0.f, "Resisted"}, {1.f, "Breaks Free"})
QVR_SLIDER(vr_body_monster_inset, "Body Inset", 0.f, 12.f, 1.f, "%.0f units", noExt,
    "Your body and a monster's meet this much further in on each side (both bodies: twice this closer face "
    "to face), so your hands reach its neck and head. Only between you and monsters: walls, doors, "
    "monsters against each other, shots and melee keep their boxes. 0: as before.")
QVR_TOGGLE(vr_foegrab_step_in, "Step In",
    "While a hand holds an enemy, your body and its body don't meet: step in and bring the other hand to "
    "its head. Let go while overlapping and the two are pushed gently apart (never stuck).")
QVR_SLIDER(vr_foegrab_step_in_depth, "Step In Depth", 0.f, 32.f, 1.f, "%.0f units", noExt,
    "With Step In, how far your body goes into the held enemy past where you would meet; there it stops "
    "you as a solid body (you slide along it), so you can't walk through it. Stepping back or aside is "
    "always free. 0: no limit (you can walk right through it).")
QVR_TOGGLE(vr_foegrab_throw, "Two-Hand Throw",
    "Hold one enemy with both hands and turn it over hard, a judo throw: one shoulder down and the other up, "
    "or its top pulled towards you or to a side. It is knocked down that way (it falls, lies, gets up), for "
    "sure: grunts, enforcers, zombies, knights, rottweilers, mummies and the infected always; death knights, "
    "ogres, fiends, spawns, scorpions and ranged knights only when hurt; never shamblers, vores, bosses, "
    "flyers or swimmers. One hand never throws. The training dummy shows how it would go.")
QVR_TOGGLE(vr_foegrab_throw_debug, "Show Diagnostic",
    "Over the enemy you hold (else the nearest): your hands' turn of it (an arrow from its top: the way it "
    "would fall) and their speed against Turn to Throw and Hands' Speed, what stops a throw now (one hand, "
    "both at its head: the neck snap's; pulling up: the lift's; away from you; its kind or health) and the "
    "last try's result. Each try is printed to the console.")
QVR_TOGGLE(vr_foegrab_throw_away, "Throw Away From You",
    "Off: a turn that would throw the enemy straight away from you does nothing (that's a shove's job). On: "
    "it throws too.")
QVR_CYCLE_BYNAME(vr_foegrab_throw_by_mass, "Who Can Be Thrown",
    "By Group: each monster group's Two-Hand Throw (Combat > Monster Groups: always, only when hurt, or "
    "never). By Mass: the two masses below.",
    {0.f, "By Group"}, {1.f, "By Mass"})
QVR_TOGGLE(vr_foegrab_throw_hold, "Hold On After Throw",
    "On: the thrown enemy stays in your hands as a ragdoll, held softly: it still topples and falls the way "
    "you threw it, and you can steer it, drag it along, pull it about; let go of the grips to drop it or "
    "throw it. Off: both hands let go as it is thrown.")
QVR_SLIDER(vr_foegrab_throw_grace, "Passes Through You", 0.f, 2.f, 0.05f, "%.2f s", ext(0.f, 5.f),
    "The thrown enemy passes through your hands, and you, this long as it falls: your hands, still in it and "
    "moving on, or you standing where you pull it, no longer knock it back over the other way. 0: they do.")
QVR_TOGGLE(vr_foegrab_lift, "Lift",
    "Walk up close to a humanoid (a grunt, an enforcer, a zombie, a knight, a death knight, Arcane Dimensions' "
    "soldiers and knights), hold it with both hands and pull up instead of to a side: it comes off its feet, a "
    "ragdoll flailing in your hands. Carry it, throw it down (let go of the grips as you swing). After a while "
    "it breaks free and shoves you back. Never ogres, fiends, shamblers or vores (each monster group's "
    "Lift: Combat > Monster Groups).")
QVR_TOGGLE(vr_foegrab_lift_debug, "Show Diagnostic",
    "Over the humanoid you hold (else the nearest): your hands' pull (an arrow: green once fast enough and "
    "within 45 degrees of straight up) against Lift Speed, the Lift Reach circle round it (green with you "
    "inside), what stops a lift now (one hand, both at its head: the neck snap's; not a humanoid; too far) "
    "and the last try's result. Each try is printed to the console.")
QVR_SLIDER(vr_foegrab_lift_speed, "Lift Speed", 0.3f, 3.f, 0.1f, "%.1f m/s", ext(0.1f, 10.f),
    "How fast your hands must pull up together (within 45 degrees of straight up) to lift it.")
QVR_SLIDER(vr_foegrab_lift_reach, "Lift Reach", 24.f, 96.f, 2.f, "%.0f units", ext(0.f, 200.f),
    "And how close you must stand to it (your middle to its; held at arm's length a grunt is about 36 units "
    "off; 32 units are about a metre).")
QVR_SLIDER(vr_foegrab_lift_time, "Breaks Free After", 0.5f, 10.f, 0.25f, "%.2f s", ext(0.1f, 60.f),
    "Held up this long, it breaks free: your hands lose it, it shoves you back and gets up.")
QVR_TOGGLE(vr_foegrab_free_sound, "Break-Free Sound", "The shove sound plays as it breaks free.")
QVR_SLIDER(vr_foegrab_manhandle_stagger, "Grab While Staggered", 0.f, 5.f, 0.25f, "%.2f s", ext(0.f, 30.f),
    "A humanoid staggered this recently (a shove, a hit or throw that unbalanced it, a stagger it just "
    "recovered from) goes down when one hand grabs it: a ragdoll in your hand, as a lifted one (it flails and "
    "breaks free). One staggering as a ragdoll goes down when a hand takes it. 0: never.")
QVR_SLIDER(vr_foegrab_manhandle_health, "Grab When Weak", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "A humanoid at this share of its full health or less (0.25: a quarter) goes down when one hand grabs it, "
    "as above. 0: never.")
QVR_TOGGLE(vr_foegrab_leggrab, "Leg Grabs",
    "Grab a humanoid's legs with both hands below the knees (one leg with both hands, or one leg each). Pull "
    "them towards you: its feet slide to you and it topples over away from you. Pull them up: it is lifted "
    "by the legs, head down, flailing, as a lift (carry it, throw it, it breaks free after Breaks Free "
    "After). Heavier humanoids need a harder pull.")
QVR_TOGGLE(vr_foegrab_leggrab_debug, "Show Diagnostic",
    "Over the humanoid you hold (else the nearest): where each hand holds it (green below the knee, orange a "
    "thigh, red off its legs), your pull towards you and up against Pull Speed and Leg Lift Speed (arrows "
    "and bars), the pull's stroke against Pull Distance, its weight's factor, what stops it now and the "
    "last try. Each try is printed to the console.")
QVR_SLIDER(vr_foegrab_leggrab_thigh, "Thigh Grip", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much a hand on a thigh (above the knee) counts: 0 it isn't a leg grab; 0.5 it is, but you must "
    "pull twice as fast.")
QVR_SLIDER(vr_foegrab_legpull_onehand, "One-Hand Pull", 0.f, 3.f, 0.05f, "%.2fx", ext(0.f, 10.f),
    "One hand alone on its leg (the other free, hanging on a ledge or holding something) pulls it out too, "
    "at Pull Speed times this: a harder pull than with both hands. 0: both hands only.")
QVR_TOGGLE(vr_foegrab_necksnap, "Neck Snap",
    "Hold a humanoid's head or neck with both hands and twist it round: push one hand forward and pull "
    "the other back, or turn both wrists the same way, or both. Its neck snaps with a crack and it dies on "
    "the spot, limp in your hands until you let go of the grips. Standing, staggering, knocked down or "
    "lifted, as long as it lives. With both hands on its head the neck snap is the only move: no judo "
    "throw, no lift. The same humanoids as the lift: never ogres, fiends, shamblers or vores.")
QVR_TOGGLE(vr_foegrab_necksnap_debug, "Show Diagnostic",
    "Over the humanoid you hold (else the nearest): its head zone (green once both hands are in it), where "
    "each hand holds (green at its head or neck, orange at a shoulder: never counts), the line between "
    "your hands, the twist's arc against Twist Angle and a panel of the hand line's numbers against it; "
    "the last try's result. Each try's largest turn is printed to the console.")
QVR_SLIDER(vr_foegrab_necksnap_angle, "Twist Angle", 10.f, 120.f, 5.f, "%.0f deg", ext(1.f, 360.f),
    "How far round your hands must turn its head, one way, within Twist Window: the line between your "
    "hands, across its neck and relative to its head, against where it pointed then (hands 30 cm apart, one "
    "15 cm forward and the other 15 cm back: 45 deg), or both wrists rolling the same way. Slow shuffles "
    "never add up; tilting doesn't count.")
QVR_SLIDER(vr_foegrab_necksnap_window, "Twist Window", 0.1f, 1.f, 0.05f, "%.2f s", noExt,
    "The head must turn Twist Angle within this long (0.15 s and 75 deg: 500 deg/s on average).")
QVR_SLIDER(vr_foegrab_necksnap_margin, "Head Grip Margin", 0.f, 12.f, 1.f, "%.0f units", ext(-8.f, 32.f),
    "Your hands count as on its head this far outside its head zone (the headshots'; 32 units are about a "
    "metre).")
QVR_SLIDER(vr_foegrab_necksnap_neck, "Neck Length", 0.f, 12.f, 0.5f, "%.1f units", ext(0.f, 32.f),
    "How far below its head your hands still hold its neck (as good as its head: only the neck snap). "
    "Lower, or out at its shoulders, both hands throw it (judo) or lift it: a shoulder never counts.")
QVR_TOGGLE(vr_foegrab_necksnap_loose, "Broken Neck Flops",
    "A snapped neck's joint goes loose: its head flops and rolls as if the vertebrae broke. Off: it stays "
    "as stiff as any body's.")

// vr_menu_gameplay.inc: pageKdTrigLedge
QVR_TOGGLE(vr_kdtrig_ledge_on, "Ledges",
    "Struck with a drop of Least Drop or more right behind it (along the hit, room for its body to drop), it is pushed off. Needs Ragdoll Triggers and Knockdowns on.")

// vr_menu_gameplay.inc: pageKdTrigStairs
QVR_TOGGLE(vr_kdtrig_stairs_on, "Stairs",
    "Struck at the top of steps going down behind it (two or more, the first right behind its feet), it falls down them. Needs Ragdoll Triggers and Knockdowns on.")

// vr_menu_gameplay.inc: pageKdTrigSlope
QVR_TOGGLE(vr_kdtrig_slope_on, "Slopes",
    "Standing on a steep slope, hit down it, it topples down the slope. Needs Ragdoll Triggers and Knockdowns on.")

// vr_menu_gameplay.inc: pageKdTrigShotgun
QVR_TOGGLE(vr_kdtrig_shotgun_on, "Shotgun Range",
    "A shotgun or super shotgun blast of yours from close by knocks it over. Needs Ragdoll Triggers and Knockdowns on.")

// vr_menu_gameplay.inc: pageKdTrigWounded
QVR_TOGGLE(vr_kdtrig_wounded_on, "Wounded",
    "Badly hurt, any hit may knock it down. Needs Ragdoll Triggers and Knockdowns on.")

// vr_menu_gameplay.inc: pageKdTrigBlow
QVR_TOGGLE(vr_kdtrig_blow_on, "Strong Blows",
    "A strong melee blow or a prop you fling knocks it over (up to twice the chance at twice Least Damage: a hard punch is about 30). Needs Ragdoll Triggers and Knockdowns on.")

// vr_menu_gameplay.inc: pageKdTrigBlast
QVR_TOGGLE(vr_kdtrig_blast_on, "Explosions",
    "A nearby explosion (a rocket, a grenade, a barrel, any blast that leaves it alive) throws it down away from the blast's centre (Least Damage: the damage it took, already less with distance). Needs Ragdoll Triggers and Knockdowns on.")

// vr_menu_gameplay.inc: pageKdTrigThrown
QVR_TOGGLE(vr_kdtrig_thrown_on, "Thrown Bodies",
    "A body you throw (a corpse or a knocked-down enemy: let go of, or a two-hand throw) or swing in your "
    "hand into a living monster knocks it over the way the body goes, by their weights and the body's "
    "momentum. Needs Ragdoll Triggers and Knockdowns on.")

// vr_menu_gameplay.inc: pageKdTrigTackle
QVR_TOGGLE(vr_kdtrig_tackle_on, "Shoulder Charges",
    "Running into a monster with a shoulder leading, your head tucked down, both hands low and close to "
    "your body (the leading arm tucked) knocks it back: a light one flies back and falls, an ogre staggers "
    "back, shamblers and bosses stand. You bounce back off it. Needs Ragdoll Triggers and Knockdowns on. "
    "(Tuned on recorded takes: Motion Recorder > Shoulder Charge.)")
QVR_SLIDER(vr_kdtrig_tackle, "Chance", 0.f, 1.f, 0.05f, "%.2f", noExt, "The chance a light one falls.")
QVR_SLIDER(vr_kdtrig_tackle_runup, "Run-Up", 0.f, 4.f, 0.1f, "%.1f s", noExt,
    "How long you must have been running at that speed before it counts (a sprint begun next to the "
    "monster doesn't; 0: none).")
QVR_TOGGLE(vr_kdtrig_tackle_feel, "Impact Feel",
    "A thud on the body, both controllers rumble and the monster cries out as it goes.")

// vr_menu_gameplay.inc: pageKdTriggers
QVR_TOGGLE(vr_kdtrig, "Ragdoll Triggers",
    "Situations in a fight that knock a grunt, an enforcer, a knight or another humanoid down as a "
    "ragdoll: by stairs and ledges, on slopes, a close shotgun blast, a strong blow, an explosion, "
    "badly hurt. It gets up as after a shove. Bigger monsters resist (Monster Resistance). Needs "
    "Knockdowns.")

// vr_menu_gameplay.inc: pageKnockdowns
QVR_TOGGLE(vr_knockdown, "Knockdowns",
    "A shove can knock a monster down as a ragdoll, alive: hit it, grab it, drag it, throw it. After a few "
    "seconds it gets up where it lies and fights on. Only monsters with ragdolls (Ragdolls on).")
QVR_SLIDER(vr_knockdown_chance, "Chance", 0.f, 3.f, 0.05f, "%.2fx", ext(0.f, 100.f),
    "Every monster's chance, times this. 0: never; 100: every shove.")
QVR_SLIDER(vr_knockdown_onehand, "One-Handed Shove", 0.f, 3.f, 0.05f, "%.2fx", ext(0.f, 10.f), "The chance of a shove with one hand, times this.")
QVR_SLIDER(vr_knockdown_twohand, "Two-Handed Shove", 0.f, 3.f, 0.05f, "%.2fx", ext(0.f, 10.f), "The chance of a shove with both hands, times this.")
QVR_TOGGLE(vr_knockdown_surprise, "Surprise Shoves Knock Down",
    "A shove of a monster that isn't fighting you (asleep, idle, busy with another) or from behind always "
    "knocks it down, if its group can be knocked down by a shove at all.")
QVR_TOGGLE(vr_knockdown_ledge, "Over a Ledge, Always",
    "A shove that would carry it over the edge of a high drop (Ledge Height or more) always knocks it down, "
    "so it tumbles off as a ragdoll. Not through a wall or railing; stairs are no ledge. Off: the chance as "
    "anywhere.")
QVR_TOGGLE(vr_hit_stagger, "Strong Hits Unbalance",
    "A strong throw (a heavy prop or a weapon, fast) or melee blow may make a monster lose its balance as a "
    "shove does: it staggers as a ragdoll and recovers, or by its knockdown chance goes over. Monsters "
    "without one are pushed and flinch. The stronger the hit and the more of its health it took, the likelier.")
QVR_TOGGLE(vr_ragimpact, "Impacts Hurt",
    "A monster you hold or threw as a ragdoll, alive or dead, is hurt where it strikes a wall or the floor "
    "hard: its speed times the mass behind it. Its head or limb struck wears down too, and pops past its own "
    "health (a head worn through kills). Slam a held head into a wall again and again; throw a lifted one into "
    "a wall.")
QVR_SLIDER(vr_ragimpact_crush_speed, "Crush Least Speed", 100.f, 1000.f, 25.f, "%.0f u/s", ext(0.f, 3000.f),
    "Landing slower than this crushes nothing (a jump from the same level lands at about 270 on the floor, "
    "200 on top of a body: 180 crushes both; a stair step: 160; a 100-unit drop: 400).")
QVR_SLIDER(vr_ragimpact_crush_push, "Crush Push", 0.f, 1.5f, 0.05f, "%.2fx", ext(0.f, 5.f),
    "A landing on a body lying down (alive or dead) pushes the part under your feet down and out at this "
    "times your landing speed, the body jolting with it. 0: it lies still.")
QVR_TOGGLE(vr_ragimpact_crush_standing, "Crush Standing Ones",
    "Landing on a standing monster's head or shoulders crushes too. Off: only one down or staggering as a "
    "ragdoll.")
QVR_TOGGLE(vr_ragimpact_crush_cushion, "Crush Breaks Your Fall",
    "A landing that crushed a monster spares you the fall's damage. Off: Quake's fall damage as ever.")
QVR_TOGGLE(vr_ragimpact_stomp, "Stomp Knocks Down",
    "Landing on a standing monster's top fast enough knocks it down as a shove does (it gets up again), "
    "if the crush did not kill it; you bounce off it.")
QVR_TOGGLE(vr_legsweep, "Leg Sweep",
    "Sweep a humanoid's lower legs with something long and blunt, level and fast, and it goes down as in a "
    "judo foot sweep: its feet slide the way you swept, the rest of it topples over the other way (forward "
    "or backward, whichever side you sweep it from). A long gun, the crowbar, a sword's hilt (not its "
    "blade), a long plank: yes. The axe, the super shotgun, the lightning gun, a rock, a brick, a crate: "
    "no. Only humanoids (no ogres, fiends, shamblers).")
QVR_SLIDER(vr_knockdown_time_min, "Time Down, Least", 0.f, 10.f, 0.25f, "%.2f s", ext(0.f, 60.f),
    "How long it stays down, at least (at random up to Time Down, Most).")
QVR_SLIDER(vr_knockdown_time_max, "Time Down, Most", 0.f, 10.f, 0.25f, "%.2f s", ext(0.f, 60.f), nullptr)
QVR_SLIDER(vr_knockdown_hit_time, "Hit Keeps It Down", 0.f, 3.f, 0.1f, "%.1f s", ext(0.f, 10.f),
    "Each hit while it lies there (a punch, a shot, a stomp) keeps it down at least this long after the hit, "
    "past its own time (never past Kept Down, Most in all).")
QVR_SLIDER(vr_knockdown_keep_time, "Handling Keeps It Down", 0.f, 3.f, 0.1f, "%.1f s", ext(0.f, 10.f),
    "Holding, dragging or lifting it keeps it down while you do and this long after you let go, past its "
    "own time (never past Kept Down, Most in all).")
QVR_SLIDER(vr_knockdown_keep_max, "Kept Down, Most", 0.f, 15.f, 0.25f, "%.2f s", ext(0.f, 60.f),
    "The longest it stays down in all while you keep at it (hits, stomps, holding it). Left alone it gets "
    "up on its own time, or soon after you stop if that has passed. 0: nothing keeps it down longer.")
QVR_SLIDER(vr_knockdown_push, "Launch", 0.f, 3.f, 0.05f, "%.2fx", ext(0.f, 10.f),
    "How hard the shove throws the body, times the shove's own push: the fastest its travel starts (Travel "
    "sets how far). 0: it drops where it stood.")
QVR_SLIDER(vr_knockdown_shove_travel, "Travel", 0.f, 1.5f, 0.05f, "%.2fx", ext(0.f, 3.f),
    "How far a shoved-down enemy goes, times how far the same shove pushes one that stays standing (where "
    "its hips lie once it is down; one- or two-handed alike). 0: it falls where it stood.")
QVR_SLIDER(vr_knockdown_shove_topple_angle, "Topple Angle", 0.f, 150.f, 5.f, "%.0f deg", ext(0.f, 180.f),
    "How far it turns over about its feet along the shove, head and chest first: 90 from standing to flat "
    "on its back or front. 0: no turn, it falls as it will.")
QVR_SLIDER(vr_knockdown_shove_topple_time, "Topple Time", 0.2f, 1.5f, 0.05f, "%.2f s", ext(0.05f, 3.f),
    "How long it takes to turn that far: slow to start, fastest past halfway, landing. Less: knocked flat "
    "hard; more: it keels over.")
QVR_SLIDER(vr_knockdown_shove_feet_lag, "Feet Lag", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How far its feet lag behind as it goes over (its head and chest as far ahead), a share of the "
    "travel's speed. 0: the whole body goes alike.")
QVR_SLIDER(vr_knockdown_shove_max_spin, "Max Spin", 60.f, 720.f, 10.f, "%.0f deg/s", ext(10.f, 2000.f),
    "The fastest it ever turns while the shove carries it (over a ledge too, as it tips over the edge and "
    "falls): no cartwheels, however hard the shove.")
QVR_SLIDER(vr_knockdown_shove_ledge_topple, "Topple Over a Ledge", 0.f, 600.f, 10.f, "%.0f deg/s", ext(0.f, 2000.f),
    "Shoved over a ledge, it is pushed whole so it goes over the edge, and turns over its feet as it falls "
    "this fast (a one-handed shove less fast) until it has turned Topple Angle. Off: it goes over upright.")
QVR_SLIDER(vr_knockdown_shove_ledge_push, "Ledge Push", 0.f, 1.5f, 0.05f, "%.2fx", ext(0.f, 3.f),
    "How hard a shove over a ledge throws it off, times the shove's own push (it is always kept going "
    "until it is past the edge). Less: it drops closer to the ledge's foot.")
QVR_SLIDER(vr_knockdown_shove_ledge_over, "Ledge Carry Past the Edge", 0.f, 1.f, 0.05f, "%.2fx", noExt,
    "Once a shove has taken it past the edge, how much of its speed it keeps as it falls: low, it just tips "
    "over the lip and drops near the ledge's foot; 1: it flies on as it goes (as before).")
QVR_TOGGLE(vr_knockdown_weld, "Weapon Stays in Hand",
    "Its weapon stays in its hand while it is down or staggering, lifted or struggling (killed there, it "
    "drops it as usual): held by the hand that holds it in its rest pose, following that hand. Off: it "
    "flops loose as a dead one's.")
QVR_SLIDER(vr_knockdown_wiggle, "Struggling Strength", 0.f, 3.f, 0.1f, "%.1fx", noExt,
    "Living knockdowns gently curl their chest and nod their head about their resting pose. Arms and legs follow physically. 0: still. Dead bodies never struggle.")
QVR_SLIDER(vr_knockdown_wiggle_frequency, "Struggling Frequency", 0.1f, 5.f, 0.1f, "%.1f Hz", noExt, nullptr)
QVR_SLIDER(vr_knockdown_retry, "Retry", 0.1f, 3.f, 0.1f, "%.1f s", ext(0.1f, 10.f), "How often it tries again to get up when there is no room.")
QVR_SLIDER(vr_knockdown_getup_slope, "Steepest Slope to Stand", 0.f, 45.f, 1.f, "%.0f deg", ext(0.f, 60.f),
    "Lying on a slope up to this steep, it gets up as on level ground. Steeper: it slides on until there "
    "is room, or is forced up (below).")
QVR_SLIDER(vr_knockdown_getup_force, "Forced Get-Up", 0.f, 10.f, 0.5f, "%.1f s", ext(0.f, 60.f),
    "Still down this long after it went down, with no room to stand (a steep slope, a cramped spot): it "
    "gets up where it lies. 0: it waits for room.")
QVR_SLIDER(vr_knockdown_blend, "Blend", 0.f, 1.f, 0.05f, "%.2f s", ext(0.f, 3.f),
    "How long the body takes to blend from how it lies into its get-up animation.")
QVR_SLIDER(vr_knockdown_getup_speed, "Get-Up Speed", 0.25f, 3.f, 0.05f, "%.2fx", ext(0.1f, 10.f), "How fast its get-up animation plays.")
QVR_CYCLE(vr_knockdown_debug, "Print Rolls",
    "Prints each shove's chance and roll, and each get-up (developer 1). And Get-Ups' Motion: a line per get-up "
    "of how it is drawn (how fast it moves, its biggest jump, the frames that go back on the one before: a "
    "jitter; the switch from its ragdoll to its animation). And Each Frame's: a line a frame as well. Knock "
    "one down with vr_knockdown_test 0 and get it up with vr_knockdown_test 1.",
    {0.f, "Off"}, {1.f, "On"}, {2.f, "And Get-Ups' Motion"}, {3.f, "And Each Frame's"})

// vr_menu_gameplay.inc: pageChainsawEngine
QVR_TOGGLE(vr_chainsaw_model_handle, "Model Handle in the Hand",
    "The cord's handle in your fist is the chainsaw model's own, the one seated on it, easing from its seat "
    "into the fist's turn as you take it. Off: a plain round handle.")

// vr_menu_gameplay.inc: pageArmedGrips
QVR_TOGGLE(vr_agrunt_freeze, "Freeze Them",
    "Armed grunts and enforcers hold still in their idle pose, weapons ready, while you tune (spawn one with "
    "the toolgun's Armed Humanoid page, or Debug > Tests > Test Scenarios: Armed Grip Editor). "
    "(vr_agrunt_freeze)")
QVR_CYCLE(vr_agrunt_grip_hand, "Hand",
    "Main Hand: the weapon in its right hand. Off Hand (Dual): its left hand's own weapon. Second Grip: its "
    "left hand on a two-handed weapon's second grip (where its hand goes, along the weapon's axes).",
    {0.f, "Main Hand"}, {1.f, "Off Hand (Dual)"}, {2.f, "Off Hand on the Second Grip"})
QVR_CYCLE(vr_agrunt_grip_scope, "Edits",
    "Every Weapon: the hand's grip for every weapon this kind of monster holds (grunts, enforcers: each "
    "its own), as you hold them; tune this first. This Weapon Only: one weapon's own nudge on top of it. "
    "(vr_agrunt_grip_scope)",
    {0.f, "Every Weapon (This Kind)"}, {1.f, "This Weapon Only"})

// vr_menu_gameplay.inc: pageArmedArmour
QVR_CYCLE_BYNAME(vr_agarmor_rule, "Absorb",
    "Player's Rule: a plate saves its type's share of a hit (green 30%, yellow 60%, red 80%) from its own "
    "pool, as your armour does. Plate Blocks All: a plate stops the whole hit until its pool is gone. "
    "A plate at 0 breaks off and falls.",
    {0.f, "Player's Rule"}, {1.f, "Plate Blocks All"})
QVR_TOGGLE(vr_agarmor_helmet_head, "Helmets Stop Headshots",
    "On: a helmet also halves a headshot's extra damage and no head pops off while it holds. Off: it only "
    "absorbs, as any plate.")
QVR_TOGGLE(vr_agarmor_block_cuts, "Plates Stop Cuts",
    "No limb is cut or popped off under an unbroken plate: break it first.")
QVR_SLIDER(vr_agarmor_debris_time, "Broken Plates Last", 1.f, 60.f, 1.f, "%.0f s", noExt,
    "A broken plate falls as debris (you can't force-grab it) and fades out after this long.")

// vr_menu_gameplay.inc: pageArmedPacks
QVR_TOGGLE(vr_worn_backpacks, "Worn Backpacks",
    "Experimental. Armed grunts wear the backpack they would drop on their back, as ragdolls too (it weighs "
    "and collides with the body, and you can drag the body by it). It tears off with a rip on a hard pull, "
    "a hard landing or enough damage on the pack, or grab a living one's pack and pull; torn off a living one, "
    "its spare ammo goes with it. A dead one's pack is taken as an item. Grunts spawned after it is "
    "turned on.")
QVR_CYCLE_BYNAME(vr_worn_backpack_pull, "Pulling the Pack",
    "Experimental. What a hand's hard pull on a worn pack does. Tears It Off: the pack rips off into the hand. "
    "Lifts Its Wearer: a pull never tears it (shots, blows, the force grab and damage still do); pulled hard "
    "enough, a living grunt is lifted by its pack as by its torso: it goes down flailing, held by the pack "
    "on its back, and you can carry, swing and throw it until it breaks free. A gentler pull drags it.",
    {0.f, "Tears It Off"}, {1.f, "Lifts Its Wearer"})
QVR_SLIDER(vr_worn_backpack_lift_force, "Pull That Lifts", 500.f, 4000.f, 100.f, "%.0f N", noExt,
    "Lifts Its Wearer: a pull on the pack this hard lifts a living grunt (standing, the hands' pull off the "
    "pack counts at 150 N a unit; held down by its pack, its hold's force).")
QVR_SLIDER(vr_worn_backpack_rip_force, "Pull That Tears It", 500.f, 4000.f, 100.f, "%.0f N", noExt,
    "A hand holding a ragdoll by its pack and yanking this hard (kept up a moment) tears the pack off into "
    "the hand. A slow drag pulls about 1500 N; a hold's spring gives 4000 N at most (Ragdoll Grab Force).")
QVR_SLIDER(vr_worn_backpack_pool, "Damage That Tears It", 0.f, 200.f, 5.f, "%.0f", noExt,
    "A living one's pack tears off once it has taken this much damage: blows, throws, shots and projectiles "
    "on the pack, and explosions and fire on its wearer (their shares below). Poison never. 0: never by "
    "damage.")
QVR_SLIDER(vr_worn_backpack_pool_blast, "Explosions' Share", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The share of an explosion's damage to its wearer that counts on the pack.")
QVR_SLIDER(vr_worn_backpack_pool_fire, "Fire's Share", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The share of fire's damage (the flamethrower, burning) that counts on the pack.")
QVR_SLIDER(vr_worn_backpack_hit_roll, "Extra Hit Chance", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "Besides its damage, a hit on the pack tears it off at once with this chance at the Hit Damage below "
    "(more damage: likelier). 0: off.")
QVR_SLIDER(vr_worn_backpack_hit_ref, "Hit Damage", 1.f, 100.f, 1.f, "%.0f", noExt, "The damage of a hit whose chance is the Extra Hit Chance.")
QVR_TOGGLE(vr_worn_backpack_tug, "Grab the Pack",
    "A fist on a living grunt's pack takes hold of the pack (it spots you): pull and it tears off into "
    "your hand. Off: the hand holds its back, as any enemy.")
QVR_SLIDER(vr_worn_backpack_tug_stretch, "Pull That Counts", 2.f, 40.f, 1.f, "%.0f cm", noExt,
    "How far your hand must pull the pack off its place for the pull to count.")
QVR_SLIDER(vr_worn_backpack_tug_time, "Pull Time", 0.05f, 3.f, 0.05f, "%.2f s", noExt,
    "How long that pull must be kept up to tear the pack off (twice as far: half as long).")
QVR_SLIDER(vr_worn_backpack_two_hand_mult, "Two-Hand Pull", 1.f, 4.f, 0.1f, "%.1fx", noExt,
    "Both hands on the pack pull this many times harder together (a living one's and a ragdoll's).")
QVR_TOGGLE(vr_worn_backpack_topple, "Tearing Topples It",
    "A ragdoll trigger: a pack torn off a living grunt by your pull knocks it down, falling the way you "
    "tore.")
QVR_SLIDER(vr_worn_backpack_topple_push, "Topple Push", 0.1f, 2.f, 0.1f, "%.1f", noExt, "That knockdown's push (a two-hand shove's: 1).")
QVR_SLIDER(vr_worn_backpack_impact, "Landing That Tears It", 2.f, 30.f, 0.5f, "%.1f m/s", noExt,
    "A ragdoll striking the level at least this fast loses its pack.")
QVR_SLIDER(vr_worn_backpack_reserve, "Spare Ammo in It", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The share of a grunt's spare rounds it carries in its pack: lost with the pack while it lives (it "
    "fights on with what its gun holds).")
QVR_SLIDER(vr_worn_backpack_volume, "Rip Volume", 0.f, 1.f, 0.05f, "%.2f", noExt, "The fabric rip's loudness.")
QVR_SLIDER(vr_worn_backpack_scale, "Size", 0.3f, 1.2f, 0.05f, "%.2f", noExt, "The pack's size on the back.")
QVR_SLIDER(vr_worn_backpack_x, "Forward", -8.f, 8.f, 0.25f, "%.2f", noExt, "Moves the pack into the back (out: negative).")
QVR_SLIDER(vr_worn_backpack_y, "Left", -8.f, 8.f, 0.25f, "%.2f", noExt, nullptr)
QVR_SLIDER(vr_worn_backpack_z, "Up", -12.f, 12.f, 0.25f, "%.2f", noExt, nullptr)
QVR_SLIDER(vr_worn_backpack_pitch, "Pitch", -45.f, 45.f, 1.f, "%.0f", noExt, nullptr)
QVR_SLIDER(vr_worn_backpack_yaw, "Yaw", -180.f, 180.f, 5.f, "%.0f", noExt, nullptr)
QVR_SLIDER(vr_worn_backpack_roll, "Roll", -45.f, 45.f, 1.f, "%.0f", noExt, nullptr)
QVR_SLIDER(vr_worn_backpack_mass, "Weight", 0.5f, 20.f, 0.5f, "%.1f kg", noExt, "Its weight on a ragdoll's chest.")

// vr_menu_gameplay.inc: pageArmedRepl
QVR_TOGGLE(vr_agrunt_repl, "Replace All Humanoids",
    "Every grunt and enforcer a map spawns (its own, its spawners', its waiting ones) is its armed version "
    "holding its own gun: the same damage, rate of fire, aim, health and loot as the one it replaces, but "
    "you can knock or pull the gun out of its hands, win it in a tug, parry it, shoot it out, and its "
    "ragdoll keeps hold of it. Takes effect at the next map load (vr_agrunt_repl).")
QVR_TOGGLE(vr_agknight, "Armed Knights",
    "Every knight a map spawns is an armed humanoid holding a sword: swung with the armed humanoids' "
    "melee, parried, disarmed, tugged and grabbed like theirs, dropped when it dies; its ragdoll keeps "
    "hold of it. A work in progress (ROUND23.md, \"Knights as armed humanoids\"). Takes effect at the "
    "next map load (vr_agknight).")
QVR_TOGGLE(vr_agdknight, "Armed Death Knights",
    "Every death knight (hell knight) a map spawns is an armed humanoid holding his sword: swung, parried, "
    "disarmed, tugged and grabbed like the armed humanoids', dropped when he dies; at range he still casts "
    "his fireball volley. A work in progress (ROUND23.md, \"Death knights as armed humanoids\"). Takes "
    "effect at the next map load (vr_agdknight).")
QVR_TOGGLE(vr_agknight_ranged_pickup, "Armed Knights: Pick Up Ranged Weapons",
    "Off: armed knights and death knights take up only melee weapons lying about (a gun they drop stays "
    "on the floor). On: they take up guns too, as grunts do (for experiments). One spawned holding a gun "
    "(the toolgun, the Rig Studio, a map's \"weapon\") holds it either way (vr_agknight_ranged_pickup).")
QVR_SLIDER(vr_agrunt_repl_rare_map, "Rare From Map", 0.f, 8.f, 1.f, "%.0f", noExt,
    "Rare weapons only from this map of an episode on (e1m4, hip1m4, r2m4, ...); maps with no number in "
    "their name never. 0: any map (vr_agrunt_repl_rare_map).")
QVR_SLIDER(vr_agrunt_repl_seed, "Shuffle", 0.f, 100.f, 1.f, "%.0f", noExt,
    "Each humanoid's loadout is the same every time you play a map (its spawn point and the map pick it); "
    "another number deals every map's out anew (vr_agrunt_repl_seed).")
QVR_TOGGLE(vr_agrunt_repl_id, "Quake's Campaigns",
    "Quake, Dimension of the Past, Dimension and Dawn of the Machine, and custom maps "
    "(vr_agrunt_repl_id).")
QVR_TOGGLE(vr_agrunt_repl_mp, "Mission Packs",
    "Scourge of Armagon and Dissolution of Eternity (vr_agrunt_repl_mp).")
QVR_TOGGLE(vr_agrunt_repl_ad, "Arcane Dimensions",
    "Arcane Dimensions' maps too: their encounters are designed around their own monsters, so off by "
    "default (vr_agrunt_repl_ad).")

// vr_menu_gameplay.inc: pageArmedAI
QVR_SLIDER(vr_agrunt_ai_skill, "Difficulty Impact", 0.f, 3.f, 0.1f, "%.1fx", ext(0.f, 10.f),
    "How much the skill level moves the knobs below, Normal always as set. At 1: Easy aggressiveness "
    "0.85x, accuracy 0.8x, reaction 1.3x, fire rate 0.9x, retreat 0.85x; Hard 1.2x, 1.25x, 0.7x, 1.15x, "
    "1.15x; Nightmare 1.4x, 1.5x, no reaction delay, 1.15x, 1.3x (and their parry's chance and speed "
    "by skill). 0: the skill changes none of them (Quake's own rules for every monster stay: "
    "Nightmare's attacks without pauses) (vr_agrunt_ai_skill).")
QVR_SLIDER(vr_agrunt_aim_error, "Aim Error", 0.f, 15.f, 0.5f, "%.1f deg", ext(0.f, 45.f),
    "How far off its aim is at most, each shot, besides the weapon's own spread; divided by Accuracy "
    "(vr_agrunt_aim_error).")
QVR_SLIDER(vr_agrunt_ai_reaction, "Reaction Time", 0.f, 3.f, 0.05f, "%.2f s", ext(0.f, 10.f),
    "From first seeing you to their first attack, at Normal (Quake's 1 s; by Difficulty Impact "
    "Nightmare none) (vr_agrunt_ai_reaction).")
QVR_TOGGLE(vr_agrunt_range, "Keep Their Distance",
    "With a gun it fights at its weapon's distance: a super shotgun close but out of your reach, a "
    "shotgun near, nail guns and the enforcer's rifle further; nearer than that it backs off facing you "
    "(further while you hold a melee weapon or charge it), about it it strafes round you, further it comes "
    "on. Off: it closes in as before. Melee weapons always close in (vr_agrunt_range).")
QVR_TOGGLE(vr_agrunt_aim_converge, "Aim From the Muzzle",
    "Its shots go from its gun's muzzle to your chest, its aim's error kept. Off: aimed from its grip and "
    "fired beside that line, a miss to one side at any distance (vr_agrunt_aim_converge).")
QVR_TOGGLE(vr_agrunt_aim_exposed, "Aim at What They Can Hit",
    "Partly behind cover, they aim at a part of you their gun's line reaches (your chest first, then your "
    "head, then whatever shows: shoulders, hips, legs; a rocket the floor at your feet), and hold fire while "
    "nothing shows. Off: always your chest, firing whenever they see your head (vr_agrunt_aim_exposed).")
QVR_SLIDER(vr_agrunt_friendly_care, "Careful of Friends", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How careful it is not to shoot or strike its friends: with another monster in its line of fire (a "
    "few lines for a shotgun's spread), the chance it holds fire and sidesteps for a clear shot. 1: "
    "always, 0: never, it fires through them as before (vr_agrunt_friendly_care).")
QVR_TOGGLE(vr_agrunt_blast_safe, "Launchers Fired Safely",
    "With a grenade or rocket launcher or a proximity gun, it fires only when the round's predicted burst "
    "(its arc, its bounces, a wall just ahead) is far enough from itself; otherwise it backs off or "
    "steps aside first, and keeps away from its mines (vr_agrunt_blast_safe).")
QVR_TOGGLE(vr_agrunt_rearm, "Picks Weapons Up",
    "Disarmed, it fights with its fists and goes for a weapon lying near (its own or another) when you are "
    "not nearer to it than it is (vr_agrunt_rearm).")
QVR_SLIDER(vr_agrunt_rearm_range, "Weapon Search Distance", 64.f, 1024.f, 32.f, "%.0f units", ext(0.f, 4096.f),
    "How far an unarmed one goes for a weapon lying on the floor (its own or another) rather than fight "
    "with its fists, when you are not nearer to it than it is (vr_agrunt_rearm_range).")
QVR_SLIDER(vr_agrunt_rearm_punch, "Punches Instead Within", 0.f, 512.f, 16.f, "%.0f units", ext(0.f, 4096.f),
    "Unarmed with you this near, it fights with its fists rather than going for a weapon further than "
    "64 units from it (one at its feet it still takes up). 0: off, as before (vr_agrunt_rearm_punch).")
QVR_TOGGLE(vr_agrunt_rearm_idle, "Re-Arms Out of a Fight",
    "Unarmed and not fighting, it also walks over to a weapon lying near and takes it up "
    "(vr_agrunt_rearm_idle; with Picks Weapons Up).")
QVR_TOGGLE(vr_agrunt_dual_pickup, "Pick Up a Second",
    "One holding a single weapon that pairs goes for a second lying about, out of a fight, and holds it in "
    "its left hand (vr_agrunt_dual_pickup).")
QVR_SLIDER(vr_agrunt_dual_chance, "Dual Wielding", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The chance one from the pool holding a weapon that pairs gets a second in its left hand: one-handed "
    "blades and clubs (a sword, the crowbar, the axe) swung in turn or both at once; guns one in each "
    "hand (not the rocket launcher, the Laser Cannon, the chainsaw, the hammers). Each can be knocked or "
    "pulled out of its hand, and it goes for another (vr_agrunt_dual_chance).")
QVR_SLIDER(vr_agrunt_flaregun, "Flare Gun First", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The chance one from the pool (or a replacement) holding one weapon carries a flare gun too: it fires "
    "its two flares first (setting you on fire), throws it away and takes its own weapon "
    "(vr_agrunt_flaregun).")
QVR_TOGGLE(vr_agrunt_parry, "They Parry",
    "One holding a weapon defends against a melee blow as you do: seeing it coming it raises and angles its "
    "weapon into the blow's way, and a blow that meets its weapon before its body is parried (the ring, "
    "sparks, your hand knocked back). A blow to its head, legs or a side its weapon doesn't cover lands in "
    "full. Never from behind, unaware of you, staggered or in pain. Monsters' blows on it too "
    "(vr_agrunt_parry).")
QVR_TOGGLE(vr_agrunt_parry_strikes_only, "Parry: Real Strikes Only",
    "It raises its parry only for a real strike: a held weapon swung at it, or a closed fist punching "
    "faster than a melee hit's least (vr_melee_speed). Your hands near it, an open hand or a grab reaching "
    "for its weapon never (0: any hand within 30 units of its chest, as before; "
    "vr_agrunt_parry_strikes_only).")
QVR_SLIDER(vr_agrunt_parry_react, "Parry: Reaction Time", 0.f, 1.f, 0.01f, "%.2f s", ext(0.f, 5.f),
    "From seeing a blow coming to its weapon raised into its way, at Normal skill (Easy 1.4x, Hard 0.85x, "
    "Nightmare 0.7x): a quick swing gets through (vr_agrunt_parry_react).")
QVR_SLIDER(vr_agrunt_parry_chance, "Parry: Chance", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The chance it sees a blow coming in time, at Normal skill (Easy 0.6x, Hard 1.15x, Nightmare 1.3x); "
    "caught out, its weapon stays where it was: a blow still meeting it there is parried "
    "(vr_agrunt_parry_chance).")
QVR_TOGGLE(vr_agrunt_react_parry, "Reactive Parry",
    "It raises its parry for a blow it sees coming (Parry: Reaction Time and Chance; by what you hold: "
    "Debug > Tunables, Armed Grunts - Parry by What You Hold). Off: only its guard below raises it "
    "(vr_agrunt_react_parry).")
QVR_TOGGLE(vr_agrunt_preparry, "Preventive Parry",
    "You up close and its shove cooling down, or a melee weapon in its hand: now and then it raises its "
    "guard for a while, then drops it, unable to parry for a moment: wait it out or bait it, then strike. "
    "Its chance, delays, guard and recovery times: Debug > Tunables, Armed Grunts - Preventive Parry "
    "(vr_agrunt_preparry).")
QVR_TOGGLE(vr_agrunt_windup_parry, "Wind-up Parry",
    "You raising a weapon, a gun, a prop or a fist overhead, out to your side or drawn back near it: it "
    "may raise its guard after a moment, hold it, then drop it: raise to bait it, wait it out, strike. "
    "Its thresholds, chance and times: Debug > Tunables, Armed Grunts - Wind-up Parry "
    "(vr_agrunt_windup_parry).")
QVR_TOGGLE(vr_agrunt_taunt, "Taunt on First Sight",
    "An armed grunt seeing you for the first time plays its taunt before it comes for you "
    "(vr_agrunt_taunt).")
QVR_SLIDER(vr_agrunt_strafe_lean, "Strafe Lean", 0.f, 15.f, 0.5f, "%.1f deg", ext(0.f, 30.f),
    "Moving sideways (strafing round you, side-stepping), its pelvis and legs tip toward the way it goes, "
    "this far at a strafe's speed (less slower), eased in and out; its upper body stays aimed. 0: "
    "none (vr_agrunt_strafe_lean).")
QVR_SLIDER(vr_agrunt_strafe_turn, "Strafe Turn", 0.f, 45.f, 1.f, "%.0f deg", ext(0.f, 60.f),
    "Moving sideways, its pelvis and legs turn toward the way it goes (hips and feet partly along the "
    "step), this far at a strafe's speed (less slower), eased in and out; the spine turns back so its "
    "chest and gun stay aimed at you. Backing off to a side turns the same way. 0: none "
    "(vr_agrunt_strafe_turn).")

// vr_menu_gameplay.inc: pageArena
QVR_SLIDER(vr_arena_skill_impact, "Difficulty Impact", 0.f, 3.f, 0.1f, "%.1fx", ext(0.f, 10.f),
    "How much the skill level (the Skill below) moves the arena's waves, Normal always as set. At 1: Easy "
    "0.75x, Hard 1.25x, Nightmare 1.5x of a wave's size, of how many foes are up at once, of a boss's bulk, "
    "of a kill's and a wave clear's score and of how many perks a boss clear offers, and the other way of "
    "the waits (6 s between waves is 8 at Easy, 4.8 at Hard, 4 at Nightmare) and of a wave clear's supplies "
    "(2 medkits is 3 at Easy, 1 at Nightmare; an armour every 4 waves is one every 3 at Easy, 6 at "
    "Nightmare). The ladder's gates move in whole waves: the second rung at wave 13 / 10 / 8 / 6 (Easy / "
    "Normal / Hard / Nightmare), a knight 7 / 4 / 2 / 2, a dog 9 / 6 / 4 / 2, an ogre 11 / 8 / 6 / 4, fiends "
    "15 / 12 / 10 / 8, death knights 19 / 16 / 14 / 12, and the first waves' unarmed humanoids 0.30 / 0.20 / "
    "0.15 / 0.10 of them; an Easy boss wave comes a wave less often, a Nightmare one carries two bosses. "
    "0: the skill changes nothing about the arena; 2: those steps doubled (0.5x at Easy, 2x at Nightmare); "
    "a count never below 1 and a score never below 1 (vr_arena_skill_impact).")
QVR_CYCLE_BYNAME(skill, "Skill",
    "skill: the game's difficulty, which the arena's waves follow through the Difficulty Impact above (its "
    "own knob: the arena's scaling is nothing to do with vr_agrunt_ai_skill). The opening line names it "
    "(ARENA - Nightmare: unarmed, wave 1 coming). From the next map, or Restart the Map (Debug > "
    "Cheats).",
    {0.f, "Easy"}, {1.f, "Normal"}, {2.f, "Hard"}, {3.f, "Nightmare"})
QVR_TOGGLE(vr_arena, "Arena In Any Map",
    "1: the waves run in this map too, off its info_arena_spawn and info_arena_item spots; the map vrarena "
    "runs them without it (vr_arena).")
QVR_SLIDER(vr_arena_wave_rest, "Rest Between Waves", 1.f, 60.f, 0.5f, "%.1f s", noExt,
    "From a wave cleared to the next wave's countdown: this many seconds at Normal (by Difficulty Impact "
    "5 is 6.7 at Easy, 4 at Hard, 3.3 at Nightmare) (vr_arena_wave_rest).")
QVR_SLIDER(vr_arena_countdown, "Wave Countdown", 0.f, 10.f, 1.f, "%.0f s", noExt,
    "After the rest, the next wave counts down in the middle of the view (Wave 7 in 5... 4... 3...), "
    "a boss wave naming its boss; the same at every difficulty. 0: no countdown (vr_arena_countdown).")
QVR_SLIDER(vr_arena_countdown_sound, "Countdown Sound", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The countdown's beeps: a tick each second, higher for the last three, and a chime as the wave "
    "comes in (Quake's own menu sounds at other pitches). Their volume; 0: silent "
    "(vr_arena_countdown_sound).")
QVR_SLIDER(vr_arena_wave_size, "Wave Size", 0.25f, 4.f, 0.25f, "%.2fx", noExt,
    "A wave's crowds (its grunts, enforcers and the like) times this; its one signature foe and its boss "
    "stay one. 1 is the ladder's own counts; at 1.75 (the default) wave 10 brings 10 foes instead of 6 "
    "and wave 20 brings 13 instead of 7. "
    "The difficulty then takes its share as it does of every count (vr_arena_wave_size).")
QVR_SLIDER(vr_arena_alive_max, "Alive Pool", 1.f, 16.f, 1.f, "%.0f", noExt,
    "How many of a wave's foes are up at once, at its start and at Normal: the rest come in one by one as "
    "these fall. The difficulty takes its share (6 is 4 at Easy, 7 at Hard, 9 at Nightmare), and it "
    "never passes Foes Up At Once (vr_arena_alive_max).")
QVR_SLIDER(vr_arena_alive_grow, "Pool Grows Every", 0.f, 20.f, 1.f, "%.0f waves", noExt,
    "The alive pool takes one more foe every this many waves (4: 6 at wave 1, 7 at 5, 8 at 9 ...), up to "
    "Foes Up At Once; 0: it never grows (vr_arena_alive_grow).")
QVR_SLIDER(vr_arena_burst, "Opening Burst", 1.f, 16.f, 1.f, "%.0f", noExt,
    "A wave opens with this many foes at once (never more than its pool); the rest trickle in "
    "(vr_arena_burst).")
QVR_SLIDER(vr_arena_spawn_gap, "Spawn Gap", 0.5f, 10.f, 0.25f, "%.2f s", noExt,
    "Once a foe falls, the next one comes in this long after, up to twice as long at random (1.5: 1.5 to 3 "
    "s), at Normal: the harder the difficulty, the shorter (2 / 1.5 / 1.2 / 1). A wave's boss comes in "
    "halfway through it, 3 s after its warning, instead (vr_arena_spawn_gap).")
QVR_SLIDER(vr_arena_max_alive, "Foes Up At Once", 1.f, 16.f, 1.f, "%.0f", noExt,
    "The ceiling on a wave's alive pool, at Normal -- the difficulty takes its share of this (8 is 6 at "
    "Easy, 10 at Hard, 12 at Nightmare; 16 at most). New foes come in out of your sight and not right "
    "next to you when the map has such a place (vr_arena_max_alive).")
QVR_SLIDER(vr_arena_boss_every, "Boss Every", 0.f, 20.f, 1.f, "%.0f", noExt,
    "A wave with a marked big one in it every this many waves (0: none) -- an enforcer boss early, an ogre "
    "from wave 10, a death knight from wave 16 at Normal (earlier the harder the skill). At Easy the gap is "
    "one longer (5 is 6); Normal, Hard and Nightmare keep it, and a Nightmare boss wave carries two, the "
    "second the same kind at half its bulk (vr_arena_boss_every).")
QVR_SLIDER(vr_arena_boss_health, "Boss Bulk", 1.f, 8.f, 0.05f, "%.2fx", noExt,
    "A boss's health times this and the difficulty's factor: an enforcer boss at 1.25 stands at 76 health "
    "at Easy, 101 at Normal, 126 at Hard, 151 at Nightmare (a Nightmare boss wave's second at half that) "
    "(vr_arena_boss_health).")
QVR_TOGGLE(vr_arena_foe_armor, "Armoured Foes",
    "1: the waves' grunts and enforcers may wear armour plates (Combat > Armed Grunts > Armour's rules), "
    "more of them, in more plates and heavier kinds as the waves go and the harder the skill: green "
    "cuirass and helmet first, yellow from wave 12 at Normal, red from wave 22. A grunt or enforcer boss "
    "always wears a kind above its wave's. In the arena this replaces the skill's own armour roll; 0: no "
    "wave foe wears any (vr_arena_foe_armor).")
QVR_SLIDER(vr_arena_foe_armor_scale, "Armour Chance", 0.f, 4.f, 0.05f, "%.2fx", noExt,
    "The armour's chance times this: at 1 a foe at the armour's first wave has 10% at Normal, 3% more each "
    "wave after (34% at wave 12, 58% at wave 20), times the difficulty's factor, never past 85%. A boss "
    "wave's foes have 15% more. 0: none (vr_arena_foe_armor_scale).")
QVR_SLIDER(vr_arena_foe_armor_from, "Armour From Wave", 2.f, 30.f, 1.f, "%.0f", noExt,
    "The first wave a foe may wear armour, at Normal; the difficulty moves it as it moves every gate "
    "(4 is wave 7 at Easy, 4 at Normal, 2 at Hard and Nightmare). Its plates grow from there: the "
    "cuirass and the helmet, one limb plate more every 3 waves (vr_arena_foe_armor_from).")
QVR_SLIDER(vr_arena_medkits_after, "Medkits", 0.f, 4.f, 1.f, "%.0f", noExt,
    "Medkits every wave clear drops (0: none), the difficulty dividing them: 2 is 3 at Easy, 2 at Normal "
    "and Hard, 1 at Nightmare (vr_arena_medkits_after).")
QVR_SLIDER(vr_arena_armor_every, "An Armour Every", 0.f, 12.f, 1.f, "%.0f waves", noExt,
    "A wave clear adds an armour every this many waves (0: none); the harder the skill the longer its gap: "
    "4 is one every 3 waves at Easy, 4 at Normal, 5 at Hard, 6 at Nightmare. From wave 12 of the run they "
    "alternate with the heavier kind -- what you have earned is yours, whatever the difficulty "
    "(vr_arena_armor_every).")
QVR_SLIDER(vr_arena_shards, "Armour Shards", 0.f, 4.f, 1.f, "%.0f", noExt,
    "On top of the medkits and armour, every wave clear drops 1 to this many armour shards at random "
    "(+5 armour each, up to 200). 0: none (vr_arena_shards).")
QVR_TOGGLE(vr_arena_scatter, "Supplies Anywhere",
    "1: a wave clear's medkits, armour and shards land at random places all over the arena -- the floor, "
    "the tower tops, the wall walks, anywhere you can walk to -- never right next to you, so you go "
    "looking for them; the middle of your view says what came. 0: they land on the map's own item "
    "points (vr_arena_scatter).")
QVR_SLIDER(vr_arena_score_kill, "Score For A Kill", 1.f, 10000.f, 5.f, "%.0f", noExt,
    "A kill's score, times the rung its holder was on (a boss four of that) and the difficulty: at 100 a "
    "first rung kill is worth 75 / 100 / 125 / 150 (Easy / Normal / Hard / Nightmare), a third rung boss "
    "900 / 1200 / 1500 / 1800; an armoured foe a quarter more per kind (green 1.25x, yellow 1.5x, red "
    "1.75x). A wave clear adds 250 times its wave, by the difficulty too (wave 3: 563 / 750 / 938 / 1125) "
    "(vr_arena_score_kill).")
QVR_TOGGLE(vr_arena_perks, "Boss Perks",
    "1: clearing a boss wave offers your choice of perks -- cards floating in front of you, the arena "
    "waiting (no countdown, no next wave) until you point at one and pull the trigger (or press its "
    "number); that perk is yours for the rest of the run. 0: it offers nothing (vr_arena_perks).")
QVR_SLIDER(vr_arena_perk_count, "Perks Offered", 1.f, 8.f, 1.f, "%.0f", noExt,
    "How many of the pool's eight a boss clear offers, at Normal -- the difficulty takes its share (3 is 2 "
    "at Easy, 4 at Hard, 5 at Nightmare); never the same perk twice (vr_arena_perk_count).")
QVR_SLIDER(vr_arena_corpse_fade, "Bodies Fade After", 0.f, 60.f, 1.f, "%.0f s", noExt,
    "A fallen foe's body (its head, a gibbed one's) lies this long, give or take 3 s, then fades out and "
    "is gone, as the Horde's do. 0: never by time. One in your hand, or still dying, waits for you: let "
    "go of, it lies 3 s more (vr_arena_corpse_fade).")
QVR_SLIDER(vr_arena_corpse_max, "Bodies Kept", 0.f, 64.f, 1.f, "%.0f", noExt,
    "At most this many fallen bodies lie at once: past it the oldest fades first (one in your hand "
    "stays: the next oldest goes). 0: no limit (vr_arena_corpse_max).")
QVR_SLIDER(vr_arena_item_waves, "Supplies Last", 0.f, 10.f, 1.f, "%.0f waves", noExt,
    "A wave clear's medkits and armour still lying there this many wave clears later fade out and are "
    "gone (one in your hand stays). 0: they stay for the run (vr_arena_item_waves).")
QVR_SLIDER(vr_arena_gun_max, "Dropped Guns Kept", 0.f, 96.f, 1.f, "%.0f", noExt,
    "At most this many of the guns the foes dropped lie about: past it the oldest fades away. Never a "
    "gun in your hands or holsters, one you dropped or threw, one carried, pulled by a force grab or in "
    "flight. The Spoils perk keeps twice as many. 0: no limit (vr_arena_gun_max).")
QVR_SLIDER(vr_arena_seed, "Wave Seed", 0.f, 1000.f, 1.f, "%.0f", noExt,
    "0: a different ladder of guns every run; a number: that seed's waves, so one difficulty's run can be "
    "compared against another's (vr_arena_seed). Not saved: set it every map.")

// vr_menu_gameplay.inc: pageArmedGrunts
QVR_SLIDER(vr_agrunt_damage_scale, "Damage to You", 0.f, 2.f, 0.05f, "%.2fx", ext(0.f, 10.f),
    "Their weapon's damage to you, times this (its damage to monsters is your weapon's own): a rocket "
    "isn't a one-shot (vr_agrunt_damage_scale).")
QVR_SLIDER(vr_agrunt_reserve, "Spare Clips", 0.f, 10.f, 1.f, "%.0f", ext(0.f, 100.f),
    "The clips each carries beyond the full one in its gun; each reload takes one. With none left it throws "
    "the gun away and fights with its fists. The guns whose clip is a shot or two carry more at the same "
    "setting (the double-barrelled shotgun 4x, the flare gun 2x, the super nailgun 1.5x). Its backpack "
    "holds what is left of them (vr_agrunt_reserve).")
QVR_SLIDER(vr_agrunt_dual_penalty, "Dual Guns' Penalty", 0.f, 2.f, 0.05f, "%.2fx", ext(0.f, 5.f),
    "Two guns held one in each hand aim as yours do in one hand: no two-hand spread reduction (spread "
    "and aim error x2 at 1) and heavy guns kick their aim up a shot by their heft. This times those "
    "penalties; 0: as accurate as with one gun in both hands (vr_agrunt_dual_penalty).")
QVR_SLIDER(vr_agrunt_strike_torso, "Torso Twist", 0.f, 2.f, 0.05f, "%.2fx", noExt,
    "How far its torso twists and leans in its melee strikes and punches: wound back, driven through the "
    "blow, followed through. 0: only its arms move (vr_agrunt_strike_torso).")
QVR_SLIDER(vr_agrunt_strike_shoulder, "Shoulder Drive", 0.f, 2.f, 0.05f, "%.2fx", noExt,
    "How far the striking shoulder lifts and bends back, its hand drawn back by its head and its elbow "
    "raised in the wind-up, then driven down through the blow (vr_agrunt_strike_shoulder).")
QVR_TOGGLE(vr_agrunt_disarm, "Hits Disarm",
    "A hard blow on its gun hand (a shot, a melee blow, a thrown thing) or a knockdown knocks the "
    "weapon out of its hand, its rounds with it; your shots by Shooting It Out's rule below while that is "
    "on (vr_agrunt_disarm).")
QVR_TOGGLE(vr_agrunt_disarm_yank, "Grab Its Weapon",
    "Your empty hand closing on its weapon takes it out of its hand: it is yours (vr_agrunt_disarm_yank).")
QVR_TOGGLE(vr_agrunt_disarm_deliberate, "Deliberate Blows Only",
    "Only a deliberate blow knocks its weapon out: a closed fist or something held, swung over more than "
    "a frame. An open hand reaching for it, or your idle hands jolted as you are pushed or walk into it, "
    "never (vr_agrunt_disarm_deliberate).")
QVR_TOGGLE(vr_agrunt_strike_first, "Strikes on Its Weapon Disarm",
    "A deliberate strike (a fist, a held gun or prop, in slow motion too) that meets its weapon or gun "
    "hand knocks the weapon out unless its parry was raised for it; into a raised parry it may still "
    "(Strike Beats a Raised Parry, in the tunables). Off: such a blow is parried whenever it can be "
    "(vr_agrunt_strike_first).")
QVR_TOGGLE(vr_agrunt_tug, "Tug-of-War",
    "Grabbing its weapon starts a struggle: it holds on and pulls back, and may shove you, headbutt you or "
    "fire. Pull your hand back far enough for long enough to take it. Staggered, knocked down, stunned "
    "by a parry or badly hurt, it lets go at once; hurt or in pain, sooner. Off: the grab takes it at once "
    "(vr_agrunt_tug).")
QVR_TOGGLE(vr_agrunt_tug_ragdoll, "Tug: Physical",
    "The tug as physics: it stands its ground as an active ragdoll, your hand holding its weapon in its "
    "hand, its arms pulling back; pull hard enough, or long enough, and its grip breaks: the weapon is "
    "yours, it staggers a moment and fights on unarmed. Let go first and it keeps it. Off: the old tug "
    "(the distance and time below; it fights back) (vr_agrunt_tug_ragdoll).")
QVR_SLIDER(vr_agrunt_tug_break_force, "Tug: Break Force", 50.f, 2000.f, 25.f, "%.0f N", ext(0.f, 20000.f),
    "A pull this hard on its weapon tears it out of its hand at once (the physical tug; less as it is "
    "hurt) (vr_agrunt_tug_break_force).")
QVR_SLIDER(vr_agrunt_tug_break_impulse, "Tug: Break Impulse", 10.f, 1000.f, 10.f, "%.0f N s", ext(0.f, 10000.f),
    "Pulling over time: the pull's force times its time (counted above Tug: Least Pull) that tears it "
    "out of its hand (the physical tug; less as it is hurt) (vr_agrunt_tug_break_impulse).")
QVR_SLIDER(vr_agrunt_tug_let_go, "Tug: Let Go Past", 20.f, 100.f, 5.f, "%.0f cm", ext(0.f, 300.f),
    "Physical tug: your hand lets go of its weapon (you lose the tug) once your real hand is this far from "
    "where it holds it, pulled away or stepped back before its grip gives; till then your hand is drawn on "
    "the weapon. A hard yank's pull reaches about 40 cm before it tears the weapon out "
    "(vr_agrunt_tug_let_go; 0: 2.4 m, as before).")
QVR_SLIDER(vr_agrunt_tug_downed_scale, "Tug: Staggering or Down", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 1.f),
    "A staggering or knocked-down one's weapon still needs a tug, this share of a standing one's (its "
    "pull time, its thresholds): it comes free sooner, not at once. 0: the grab takes it at once "
    "(vr_agrunt_tug_downed_scale).")
QVR_CYCLE(vr_agrunt_tug_second_grab, "Tug: Your Other Hand's Grab",
    "Your other hand closing on its weapon during a tug: it joins (both pulls count and it gives sooner), "
    "or it tears the weapon free into your first hand at once (vr_agrunt_tug_second_grab).",
    {0.f, "Joins the Tug"}, {1.f, "Tears It Free"})
QVR_TOGGLE(vr_agrunt_tug_strike, "Tug: Other Fist Strikes It Free",
    "In a tug, your free fist striking its weapon (at 60% of the disarm swing) tears it free into your "
    "holding hand; so does any blow that would knock it out of its hand (vr_agrunt_tug_strike).")
QVR_SLIDER(vr_agrunt_grab_speed, "Grab: Fastest Hand", 0.f, 6.f, 0.1f, "%.1f m/s", ext(0.f, 50.f),
    "A hand moving faster than this (a punch, a swipe) doesn't grab its weapon: its speed against you or "
    "against the weapon (going with a swing to catch it), the lesser, in real time (bullet time too). 0: "
    "any speed (vr_agrunt_grab_speed).")
QVR_SLIDER(vr_agrunt_tug_strength, "Tug: Its Pull", 0.f, 3.f, 0.1f, "%.1f", ext(0.f, 3.f),
    "How hard its arms pull its weapon back in the physical tug (vr_agrunt_tug_strength).")
QVR_SLIDER(vr_agrunt_tug_time, "Tug: Pull Time", 0.f, 5.f, 0.1f, "%.1f s", ext(0.f, 30.f),
    "How long you must keep it pulled that far against a calm grunt at full health; less as it is hurt, "
    "half while it flinches from a hit. 0: any pull wins (vr_agrunt_tug_time).")
QVR_SLIDER(vr_agrunt_tug_fight, "Tug: It Fights Back Every", 0.f, 5.f, 0.1f, "%.1f s", ext(0.f, 30.f),
    "About how often it fights back during the struggle: a shove or a headbutt breaks your grip, a gun "
    "fires at you. 0: never (vr_agrunt_tug_fight).")
QVR_SLIDER(vr_agrunt_parry_disarm, "Parry Disarm Chance", 0.f, 1.f, 0.05f, "%.2f", ext(0.f, 1.f),
    "The chance your weapon's parry of its melee blow knocks its weapon out of its hand; a well-timed "
    "parry's 1.5 times (vr_agrunt_parry_disarm).")
QVR_TOGGLE(vr_agrunt_fall_drop, "Drops It When Floored",
    "Knocked down onto the floor (not just staggered by a shove) it drops its weapon beside it, and "
    "takes it back as it gets up unless you took it (vr_agrunt_fall_drop; with Hits Disarm).")
QVR_TOGGLE(vr_agrunt_fall_keep, "Keeps It in a Gentle Fall",
    "A gentle fall (a shove's backpedal to the floor) keeps the weapon in its hand, but while it lies "
    "there a grab takes it with a short tug (Tug: Staggering or Down) and a blow knocks it away. A violent one (below) drops it "
    "(vr_agrunt_fall_keep).")
QVR_SLIDER(vr_agrunt_fall_keep_impact, "Fall: Drops It on Impact", 0.f, 20.f, 0.5f, "%.1f m/s", ext(0.f, 100.f),
    "Lifted, judo-thrown, shoulder-charged or shoved over, it holds on to its weapon until its body meets "
    "the ground this hard (the speed it loses as it lands); a gentle landing keeps it, a slam drops it. "
    "Never while your hands hold it. 0: the older rules below instead (vr_agrunt_fall_keep_impact).")
QVR_SLIDER(vr_agrunt_fall_keep_window, "Fall: Flung Within", 0.f, 5.f, 0.1f, "%.1f s", ext(0.f, 60.f),
    "Flung counts only this long after it goes down: lying there, your body or hand nudging it as you "
    "come to take its weapon doesn't make it let go. 0: any time while down (vr_agrunt_fall_keep_window).")
QVR_TOGGLE(vr_agrunt_saw_drop_running, "Drops Its Chainsaw Running",
    "One that was fighting you with a chainsaw drops it with the engine running (killed, disarmed, a tug "
    "lost): it runs on as yours does when let go (Runs On When Let Go), so taken up meanwhile it needs no "
    "cord pull. One that never saw you drops it off (vr_agrunt_saw_drop_running).")
QVR_TOGGLE(vr_agrunt_shotdisarm, "Shoot Weapons Out",
    "Your shots on the weapon an armed grunt holds, or the hand holding it, add up; each time they pass "
    "the threshold within the time window, a roll may knock the weapon out of its hand, along your shots. "
    "A close shotgun blast on the gun likely does, a quick nail burst may, a chip shot from afar seldom. "
    "Each hand's weapon on its own. While on, your shots disarm only this way (vr_agrunt_shotdisarm).")
QVR_CYCLE(vr_agrunt_shotdisarm_parts, "What Counts",
    "Which hits add up: on the weapon itself, on the hand and forearm holding it, or either "
    "(vr_agrunt_shotdisarm_parts).",
    {0.f, "The Weapon"}, {1.f, "The Hand"}, {2.f, "Weapon or Hand"})
QVR_SLIDER(vr_agrunt_shotdisarm_size, "Hit Volume Size", 0.25f, 3.f, 0.05f, "%.2fx", ext(0.f, 10.f),
    "The weapon's and the hand's hit volumes times this: under 1 only precise shots count, over 1 near ones "
    "too (vr_agrunt_shotdisarm_size).")
QVR_SLIDER(vr_agrunt_shotdisarm_chance, "Chance a Roll", 0.f, 1.f, 0.05f, "%.2f", ext(0.f, 1.f),
    "The roll's chance right at the threshold (vr_agrunt_shotdisarm_chance).")
QVR_SLIDER(vr_agrunt_shotdisarm_chance_scale, "Chance Past the Threshold", 0.f, 2.f, 0.05f, "+%.2f", ext(0.f, 10.f),
    "Added to the chance for each threshold's worth past it (half past: half of it); at most 0.95. A "
    "shotgun blast all on the gun, 24 on 16: 0.25 + 0.4 (vr_agrunt_shotdisarm_chance_scale).")
QVR_TOGGLE(vr_agrunt_shotdisarm_block, "Weapon Stops the Shot",
    "A pellet or a nail whose first hit is the held weapon (not its hand) stops there: sparks and a tink, "
    "no damage to the one holding it (each pellet on its own). Explosions and lightning hurt it as ever. "
    "Off: it goes on into the body behind (vr_agrunt_shotdisarm_block).")
QVR_TOGGLE(vr_agrunt_shotdisarm_proj, "Nails and Rockets Roll Each",
    "Each nail (the nailguns', the nail pistol's, lava nails) and rocket on or near its weapon or hand rolls "
    "once to knock it out: the chance below for that damage, more for more damage, less the further off. "
    "Off: their damage adds up to a roll as the shotguns' does (vr_agrunt_shotdisarm_proj).")
QVR_SLIDER(vr_agrunt_shotdisarm_proj_chance, "Projectile: Chance", 0.f, 1.f, 0.05f, "%.2f", ext(0.f, 1.f),
    "A projectile's chance on the weapon or hand at the reference damage below: a nail 0.2, a pistol nail "
    "0.27, a super nail 0.4 (vr_agrunt_shotdisarm_proj_chance).")
QVR_SLIDER(vr_agrunt_shotdisarm_proj_ref, "Projectile: Reference Damage", 1.f, 50.f, 1.f, "%.0f", ext(0.1f, 500.f),
    "The damage that chance is for; the chance grows with the damage (vr_agrunt_shotdisarm_proj_ref).")
QVR_SLIDER(vr_agrunt_shotdisarm_proj_near, "Projectile: Near Miss", 0.f, 12.f, 0.5f, "%.1f units", ext(0.f, 64.f),
    "A miss this close to the weapon or hand still rolls, its chance falling to 0 at this distance "
    "(vr_agrunt_shotdisarm_proj_near).")
QVR_SLIDER(vr_agrunt_shotdisarm_proj_max, "Projectile: Most Chance", 0.f, 1.f, 0.05f, "%.2f", ext(0.f, 1.f),
    "One projectile's chance at most: a rocket's direct hit (vr_agrunt_shotdisarm_proj_max).")
QVR_TOGGLE(vr_agrunt_ik, "Hands on the Grips",
    "Its arms are posed so its hand holds the weapon's grip and its other hand the second grip of a "
    "two-handed weapon, or hangs free beside a one-handed one; melee swings carry the arm "
    "(vr_agrunt_ik).")

// vr_menu_gameplay.inc: pageEnemyWeapons
QVR_TOGGLE(vr_enemy_weapon_depleted_loot, "Spent Guns Stand Out",
    "Off: an empty burst rifle, laser rifle or Arcane Dimensions soldier's gun, or an ogre's chainsaw out "
    "of fuel, lying about (nothing ever refills them) has no loot pillar and the force grab skips it; a hand still picks it up. On: as any "
    "weapon (vr_enemy_weapon_depleted_loot).")
QVR_TOGGLE(vr_grunt_burst, "Burst Rifles",
    "On: grunts fire 3-round bursts of single bullets, tighter than a shotgun's pellets, and the gun a "
    "grunt drops fires the same bursts. Off: id's shotgun, the grunts' and the dropped guns' (six "
    "pellets as yours, a shell a shot: a third of Rounds).")
QVR_SLIDER(vr_enemygun_spent_smoke, "Spent Smoke", 0.f, 15.f, 0.5f, "%.1f s", ext(0.f, 60.f),
    "A burst rifle or a laser rifle whose last round you fire smokes this long, in the hand or dropped: it "
    "is done, drop it (0: never).")
QVR_SLIDER(vr_enemygun_spent_crackle, "Spent Crackle", 0.f, 10.f, 0.5f, "%.1f s", ext(0.f, 30.f),
    "And crackles all over with lightning's arcs this long, as a corpse the lightning struck (0: never).")
QVR_SLIDER(vr_enemygun_spent_shake, "Spent Shake", 0.f, 1.f, 0.05f, "%.2f units", ext(0.f, 3.f),
    "While it crackles in your hand the gun shakes this much, fading with its arcs (0: still).")
QVR_SLIDER(vr_enemygun_spent_haptics, "Spent Haptics", 0.f, 1.f, 0.05f, "%.2f", noExt, "And buzzes in your hand this hard (0: not at all).")

// vr_menu_gameplay.inc: pageWeaponDamage
QVR_SLIDER(vr_dmg_shotgun, "Shotgun", 0.5f, 20.f, 0.5f, "%.1f a pellet", ext(),
    "Each of its 6 pellets (id's 4; a head more). A grunt's dropped shotgun (Burst Rifles off) too.")
QVR_SLIDER(vr_dmg_nail, "Nailgun", 1.f, 60.f, 1.f, "%.0f a nail", ext(), "id's 9 (a head more).")
QVR_SLIDER(vr_dmg_super_nail, "Super Nailgun", 1.f, 100.f, 1.f, "%.0f a nail", ext(), "id's 18 (a head more).")
QVR_SLIDER(vr_nail_speed, "Nail Speed", 500.f, 6000.f, 5.f, "%.0f u/s", ext(100.f, 20000.f),
    "The nailgun's and the super nailgun's nails, yours and the monsters' (Quake's 1000; 1750 until round "
    "23), plus the hand's.")
QVR_SLIDER(vr_nailpistol_speed, "Nail Pistol Speed", 500.f, 6000.f, 10.f, "%.0f u/s", ext(100.f, 20000.f),
    "Its nails' speed (the nailgun's Nail Speed), plus the hand's.")
QVR_SLIDER(vr_nailpistol_cooldown, "Nail Pistol Cooldown", 0.02f, 1.f, 0.01f, "%.2f s", ext(0.02f, 3.f),
    "Semi-automatic: a shot each pull of the trigger, at least this long apart.")
QVR_SLIDER(vr_nailpistol_spread, "Nail Pistol Spread", 0.f, 5.f, 0.05f, "%.2f deg", ext(0.f, 30.f), "How far its nails stray from its aim at most.")
QVR_SLIDER(vr_nailpistol_slide_travel, "Nail Pistol Slide Travel", 0.f, 3.f, 0.1f, "%.1f units", noExt,
    "How far its slide goes back each shot, and stays back while it is empty (the gun's own units: it is "
    "about 13 long). A magazine with rounds put in racks it closed.")
QVR_TOGGLE(vr_nailpistol_screen_ride, "Nail Pistol Screen Rides Slide",
    "Its ammo screen, mounted on the slide's left side, goes back and home with the slide (each shot, "
    "locked back while empty). Off: it stays put on the gun.")
QVR_TOGGLE(vr_flare_burn, "Flares Set Things on Fire",
    "A flare sets what it hits on fire (monsters, corpses, crates; Combat > Burning) and sets off "
    "explosive boxes. Off: its hit's damage only.")
QVR_SLIDER(vr_flare_stick_time, "Stuck Flare Burns", 1.f, 120.f, 1.f, "%.0f s", ext(0.5f, 3600.f),
    "How long a flare stuck in a wall or lying on the floor burns, flickering red with sparks and smoke.")
QVR_SLIDER(vr_flare_air_time, "Air Flare Burns", 1.f, 60.f, 1.f, "%.0f s", ext(0.5f, 3600.f),
    "How long a flare fired up into the air burns, drifting down from the top of its climb and lighting "
    "a wide area.")
QVR_SLIDER(vr_flare_air_fall, "Air Flare Sinks", 2.f, 100.f, 1.f, "%.0f u/s", ext(1.f, 400.f), "How fast a drifting flare sinks.")
QVR_SLIDER(vr_revolver_heavy_mult, "Heavy Revolver Big Bodies", 1.f, 4.f, 0.05f, "%.2fx", ext(0.f, 10.f),
    "A slug into a big monster (Big Body Health or more: an ogre, a fiend, a shambler) deals this many times "
    "as much: an ogre takes 4-5 in the body.")
QVR_SLIDER(vr_revolver_cooldown, "Heavy Revolver Cooldown", 0.05f, 1.f, 0.01f, "%.2f s", ext(0.05f, 3.f),
    "Double action: a shot each pull of the trigger, at least this long apart.")
QVR_SLIDER(vr_dmg_grenade, "Grenade", 10.f, 400.f, 5.f, "%.0f", ext(),
    "A grenade's blast at its middle, less further out (id's 120): the launcher's, a hand grenade's and an "
    "ogre's grenade caught and thrown back.")
QVR_SLIDER(vr_dmg_rocket, "Rocket", 10.f, 400.f, 5.f, "%.0f", ext(),
    "A direct hit, and up to a fifth more at random (id's 100); its blast on what's around 1.2 times it.")
QVR_SLIDER(vr_dmg_laser, "Laser Cannon", 1.f, 100.f, 1.f, "%.0f a bolt", ext(),
    "Each bolt (18; one in ten a third more). A bolt bouncing back into what it hit does half.")
QVR_SLIDER(vr_dmg_lava_nail, "Lava Nails", 1.f, 60.f, 1.f, "%.0f a nail", ext(), "A nail to a monster (15; to a player 3/5 of it).")
QVR_SLIDER(vr_crowbar_damage, "Crowbar", 1.f, 100.f, 1.f, "%.0f", ext(),
    "A blow (20, the axe's: 25 before), before its speed and weight; its hook hits 20% harder, its chisel "
    "end 40% softer. Crowbars lie in the firing range's prop area.")
QVR_SLIDER(vr_sword_damage_mult, "Knight's Sword", 0.5f, 3.f, 0.05f, "%.2fx 20", ext(),
    "Knights and hell knights always drop their sword: a swing's damage, times 20 (1.5: 30), before its "
    "speed and weight; the hell knight's sword 25% more.")
QVR_SLIDER(vr_gruntgun_damage, "Grunt's Burst Rifle", 1.f, 30.f, 1.f, "%.0f a round", ext(),
    "Grunts always drop their gun: a trigger pull fires a 3-round burst, each round this much damage (a "
    "head more; a grunt has 30 health). Burst Rifles off: the Shotgun's pellets.")

// vr_menu_gameplay.inc: pageSound
QVR_CYCLE(vr_snd_spatial, "Spatial Audio",
    "Steam Audio over Quake's mixer: the features below. Off: Quake's left/right panning as it always was. "
    "Always: on the desktop too (for headphones).",
    {0.f, "Off"}, {1.f, "In VR"}, {2.f, "Always (headphones)"})
QVR_TOGGLE(vr_snd_hrtf, "Sounds Around Your Head (HRTF)",
    "Binaural: each sound shaped as your ears would hear it from where it is, by your head's position and "
    "turn: in front, behind, above and below, not just left or right. Off: Quake's panning.")
QVR_CYCLE(vr_snd_hrtf_interp, "HRTF Smoothing",
    "Between the measured directions: blended (moving sounds glide), or the nearest one.",
    {1.f, "Smooth (Bilinear)"}, {0.f, "Nearest (Cheaper)"})
QVR_CYCLE(vr_snd_fullband, "Full-Band Sound",
    "Quake's 11 kHz low-pass (sndspeed 11025, the default) cuts everything above 5.5 kHz, where the "
    "binaural sounds' cues for ahead, behind, above and below are. Binaural Sounds: those skip it, each "
    "resampled cleanly (no hiss from Quake's old 11 kHz sounds; the 22 and 44 kHz ones keep their highs). "
    "All Sounds: the panned ones too. Off: everything through it, as Quake.",
    {0.f, "Off (Quake's 11 kHz)"}, {1.f, "Binaural Sounds"}, {2.f, "All Sounds"})
QVR_SLIDER(vr_snd_hrtf_gain, "HRTF Volume", 0.5f, 2.f, 0.05f, "%.2fx", ext(0.f, 4.f),
    "The binaural sounds' volume against the others (1.25: as loud as Quake's panning, all round; 1.5 by default: a little louder).")
QVR_SLIDER(vr_snd_voices, "Spatial Voices", 4.f, 64.f, 4.f, "%.0f", noExt,
    "How many of the loudest sounds are rendered this way at once; the rest are panned as ever.")
QVR_SLIDER(vr_snd_occlusion, "Occlusion", 0.f, 1.f, 0.05f, "%.2f", ext(0.f, 2.f),
    "A sound behind a wall or a closed door is quieter and duller (doors and lifts move in it as they move). "
    "0 off, 1 full; 0.8 keeps monsters behind walls audible.")
QVR_SLIDER(vr_snd_occlusion_samples, "Occlusion Rays", 1.f, 32.f, 1.f, "%.0f", noExt,
    "Rays from each sound (1: hidden or not; more: partly hidden, round corners). On the thread pool.")
QVR_SLIDER(vr_snd_occlusion_radius, "Sound Source Size", 0.1f, 2.f, 0.1f, "%.1f m", noExt, "The sphere a sound comes from, for partial occlusion.")
QVR_TOGGLE(vr_snd_air, "Air Absorption", "Far sounds lose their highs in the air.")
QVR_SLIDER(vr_snd_falloff, "Distance Falloff", 0.f, 2.f, 0.05f, "%.2fx", noExt,
    "How fast sounds get quieter with distance: 1 as Quake (a sound gone at about 30 m), 0.75 (the default) "
    "a bit further, 0.5 twice as far, 0 never quieter. Every sound, spatial audio on or off.")
QVR_SLIDER(vr_snd_reverb, "Room Reverb", 0.f, 1.f, 0.05f, "%.2f", ext(0.f, 2.f),
    "The reverb of the space around your head, simulated from the map (a small room rings short, a big hall "
    "long): how loud. 0 off.")
QVR_CYCLE(vr_snd_reverb_quality, "Reverb Quality",
    "Low: a reverb driven by the simulated decay times (cheapest, no echoes). Medium: the simulated response "
    "itself (2 s). High: 3 s from twice the rays.",
    {0.f, "Low (Parametric)"}, {1.f, "Medium (Convolution)"}, {2.f, "High (Convolution, Long)"})
QVR_SLIDER(vr_snd_reverb_interval, "Reverb Update", 0.1f, 1.f, 0.05f, "%.2f s", ext(0.05f, 5.f),
    "Seconds between the room's simulations (on the thread pool).")
QVR_SLIDER_BYNAME(snd_waterfx, "Underwater Muffle", 0.f, 2.f, 0.1f, "%.1f", noExt,
    "Sounds muffled while your head is in water, slime or lava (your head's place, not your body's). 0 off.")
QVR_TOGGLE(vr_snd_hands, "Weapons From Your Hands",
    "Your guns' shots, your blows and what your hands do (a parry, a reload, drawing and holstering, a dry "
    "click, the grenade pouch, a chainsaw's cord) sound from the hand doing it, not from the middle of your "
    "head.")
QVR_TOGGLE(vr_snd_follow, "Sounds Follow Things",
    "A sound stays with what made it as it moves (a monster, a door, a hook in flight).")
QVR_SLIDER(vr_snd_doppler, "Doppler", 0.f, 2.f, 0.1f, "%.1fx", ext(0.f, 4.f),
    "Things coming at you sound higher, going away lower (1: as physics has it; 0 off).")
QVR_SLIDER(vr_snd_pitch_jitter, "Pitch Variation", 0.f, 10.f, 1.f, "%.0f%%", ext(0.f, 25.f),
    "The most frequent sounds (punches and blows, swings, squishes, clicks, knocks, casings, footsteps) "
    "each a little higher or lower at random, up to this much, so that one recording isn't heard the same "
    "every time (0 off).")
QVR_SLIDER(vr_snd_hit_layer, "Blow Material Layer", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "A punch, a gun's butt, a headbutt or a thrown thing landing also sounds of what it hit: a slap of "
    "flesh on a body, a clink of armour on a knight or an enforcer, a knock of wood on a crate, your "
    "knuckles' knock on a wall. How loud, under the blow's thud (0 none).")
QVR_SLIDER(vr_snd_nearfield, "Near Field", 0.f, 2.f, 0.1f, "%.1f", noExt,
    "A sound within a metre of your head: the nearer ear louder, the farther one quieter and duller, by how "
    "near and how much to the side. 0 off.")
QVR_TOGGLE(vr_gunsfx, "Punchier Gunfire",
    "The extras below for every gun, yours and the enemies'. Off: every shot, click and buzz as before.")
QVR_SLIDER(vr_gunsfx_layers, "Shot Layers", 0.f, 2.f, 0.1f, "%.1f", noExt,
    "Under each shot a close mechanical layer (a hammer, a feed's tick, a launcher's clunk) and a low thump: "
    "how loud (0 none).")
QVR_SLIDER(vr_gunsfx_tail, "Room Tail", 0.f, 2.f, 0.1f, "%.1f", noExt,
    "Each shot's tail sized to where it is fired: short and tight in a small room, long in a big hall, long "
    "with a distant echo under the open sky. How loud (0 none).")
QVR_SLIDER(vr_gunsfx_vary, "Shot Variation", 0.f, 10.f, 1.f, "%.0f%%", noExt,
    "Each shot (and its layers) a little higher or lower, at random up to this much, and half as much louder "
    "or quieter, so a stream of shots isn't one recording again and again (0 off).")
QVR_TOGGLE(vr_gunsfx_casings, "Casings by Surface",
    "Spent casings ring on metal and knock on wood; stone as before, water's plips as before. Off: the one "
    "tink everywhere.")
QVR_SLIDER(vr_gunsfx_impacts, "Surface Impacts", 0.f, 2.f, 0.1f, "%.1f", noExt,
    "A bullet or a nail hitting a wall sounds of what it hit, from where it hit: a dull thud on stone, "
    "duller on dirt, a ricochet's ping on metal, a knock on wood (a nail's lighter, under its own tink). "
    "A shotgun's pellets or a nailgun's stream make one or two at a time, not a dozen. How loud (0 none).")
QVR_SLIDER(vr_gunsfx_dryfire, "Dry Fire Click", 0.f, 2.f, 0.1f, "%.1f", noExt,
    "An empty gun's click: how loud, with a small tick in that hand (0 silent).")
QVR_TOGGLE(vr_gunsfx_action, "Actions From the Action",
    "A gun's mechanical sounds (breaking open and closing, a magazine in or out, a shell in) heard from that "
    "part of the gun, not its muzzle.")
QVR_SLIDER(vr_gunsfx_punch, "Haptic Punch", 0.f, 2.f, 0.1f, "%.1fx", noExt,
    "Each shot punches the controller: a sharp spike, then a short rumble as long as the gun's recoil takes to "
    "come back, as hard as its recoil (each weapon's own: Weapon Offsets > Effects > Haptic Punch). 0: the "
    "old flat buzz.")
QVR_TOGGLE(vr_snd_limiter, "Mix Limiter",
    "Loud sounds piling up (explosive boxes blowing up together) are brought down smoothly just under full "
    "volume instead of clipping, which crackles; quieter sounds untouched. Spatial audio on or off. Off: "
    "Quake's hard clip, 6 dB lower.")
QVR_SLIDER(vr_snd_limiter_ceiling, "Limiter Ceiling", -6.f, 0.f, 0.5f, "%.1f dB", ext(-12.f, 0.f),
    "The loudest the mix gets, under full volume (-1 by default; -6: Quake's clip, quieter explosions).")
QVR_CYCLE(vr_snd_frame, "Frame Size",
    "Samples a voice renders at a time: larger is cheaper, a moving sound's direction coarser.",
    {256.f, "256 (5.8 ms)"}, {512.f, "512"}, {1024.f, "1024 (Cheapest)"})

// vr_menu_gameplay.inc: pageBloodFeedback
QVR_TOGGLE(vr_blood_hit, "Hit Blood Vignette",
    "Blood-red edges after a hit, including armor-only damage. The centre stays clear. Independent of Screen Tints and low-health pulses.")
QVR_SLIDER(vr_blood_hit_duration, "Hit Duration", 0.f, 2.f, 0.05f, "%.2f s", ext(0.f, 5.f),
    "Seconds until a hit fades away. 0: no hit effect. Pausing freezes it.")
QVR_SLIDER(vr_blood_hit_intensity, "Hit Intensity", 0.f, 0.8f, 0.02f, "%.2f", noExt,
    "Strongest opacity of a hit. Hits taking less than 20 health plus armor are lighter. Overlapping hits remain bounded.")
QVR_SLIDER(vr_blood_hit_size, "Hit Coverage", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How far the hit's red edges reach into your view. Larger covers more area without changing intensity or direction. 0: none; 1: widest. A clear centre always remains. Independent of Health Coverage.")
QVR_SLIDER(vr_blood_hit_bias, "Directional Bias", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "0: equal on every edge. 1: strongest towards the source, weakest opposite. Front: upper edge; rear: lower; above/below: upper/lower. Turns follow the world direction. Unknown, near-player and known self/environment hits are even.")
QVR_TOGGLE(vr_blood_health, "Low Health Blood Vignette",
    "Blood-red edges pulse below the health threshold. Losing health makes them stronger and more frequent; healing eases them. Independent of hits.")
QVR_SLIDER(vr_blood_health_threshold, "Health Threshold", 1.f, 100.f, 1.f, "%.0f", ext(1.f, 200.f),
    "Pulses below this health, from the local player's actual health stat. None at or above it, or while dead.")
QVR_SLIDER(vr_blood_health_intensity, "Health Intensity", 0.f, 0.8f, 0.02f, "%.2f", noExt,
    "Peak opacity as health approaches zero. The centre stays clear. Both blood effects together never exceed 0.8 opacity.")
QVR_SLIDER(vr_blood_health_size, "Health Coverage", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How far low-health pulses reach into your view. Larger covers more area without changing intensity or pulse timing. 0: none; 1: widest. A clear centre always remains. Independent of Hit Coverage.")
QVR_SLIDER(vr_blood_health_duration, "Pulse Duration", 0.05f, 2.f, 0.05f, "%.2f s", noExt,
    "Width of each beat. Independent of rate: wide beats overlap smoothly when pulses become frequent.")
QVR_SLIDER(vr_blood_health_rate, "Pulse Rate", 0.1f, 3.f, 0.05f, "%.2f Hz", noExt,
    "Beats per second near the threshold. Low-health Rate Scale increases this as health falls, up to 6 Hz.")
QVR_SLIDER(vr_blood_health_rate_scale, "Low-health Rate Scale", 0.f, 4.f, 0.1f, "%.1f", noExt,
    "Rate is Pulse Rate times (1 + severity times this). 0: constant rate; 2: up to three times as frequent.")
QVR_SLIDER(vr_blood_health_curve, "Health Scaling Curve", 0.25f, 4.f, 0.05f, "%.2f", noExt,
    "Severity is the missing share below the threshold, raised to this power. 1: linear; above 1: gentler near the threshold; below 1: stronger sooner. Both opacity and rate use severity.")

// vr_menu_gameplay.inc: pageDamage
QVR_TOGGLE(vr_hit_precise, "Precise Hit Detection",
    "Shots, projectiles, the grappling hook, melee blows and thrown things hit a monster's model as you see "
    "it, not the big box round it: aim at the body. Off: Quake's boxes.")
QVR_TOGGLE(vr_hit_drawn_pose, "Armed Bodies as Drawn",
    "An armed enemy's body is hit where you see it: its raised guard, its swinging arm, its aimed gun "
    "arm. Off: where its plain animation frame has it, as before.")
QVR_SLIDER(vr_hit_tolerance_melee, "Melee Tolerance", 0.f, 12.f, 0.5f, "%.1f units", ext(),
    "How much bigger than the model a monster is to your melee blows and shoves.")
QVR_TOGGLE(vr_hit_head_priority, "Head Priority",
    "Where a hit point, or a whole melee sweep's way through a body, lies in more than one zone, the "
    "head's zone decides it. Off: the limb and leg zones claim it first.")
QVR_SLIDER(vr_damage_to_enemies, "Damage to Enemies", 0.25f, 4.f, 0.05f, "%.2fx", ext(), "Damage you deal to monsters.")
QVR_SLIDER(vr_damage_to_player, "Damage to You", 0.f, 4.f, 0.05f, "%.2fx", ext(), "Damage monsters, traps and falls deal to you.")
QVR_SLIDER(vr_damage_self, "Self Damage", 0.f, 2.f, 0.05f, "%.2fx", ext(), "Damage your own rockets and grenades deal to you.")
QVR_TOGGLE(vr_positional_damage, "Positional Damage", "Headshots, arm and leg shots on humanoid monsters.")
QVR_TOGGLE(vr_melee_positional, "Melee Positional Damage",
    "Your melee blows take the same head / limb / leg multipliers as your shots, from the point on the "
    "model they met: a head is a head for a blade and for a shot. Off: flat melee damage, as before. The "
    "looser melee tolerance stays what connects only.")
QVR_SLIDER(vr_headshot_mult, "Headshot Damage", 1.f, 5.f, 0.1f, "%.1fx", ext(), nullptr)
QVR_SLIDER(vr_limbshot_mult, "Arm Shot Damage", 0.1f, 1.f, 0.05f, "%.2fx", ext(), nullptr)
QVR_SLIDER(vr_legshot_mult, "Leg Shot Damage", 0.1f, 1.f, 0.05f, "%.2fx", ext(), nullptr)
QVR_SLIDER(vr_headshot_sound, "Headshot Sound", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "Volume of the crack you hear when you land a headshot (0 off).")
QVR_SLIDER(vr_push, "Knockback", 0.f, 2.f, 0.05f, "%.2fx", ext(), "All knockback; the settings below scale this.")
QVR_SLIDER(vr_melee_push, "Your Melee Hits", 0.f, 3.f, 0.05f, "%.2fx", ext(),
    "How far your melee blows (and headbutts) push what they hit. A ragdoll or a corpse moves by mass instead (below).")
QVR_SLIDER(vr_melee_ragdoll_bounce, "Blows on Ragdolls: Bounce", 0.f, 1.f, 0.05f, "%.2f", ext(),
    "A blow or a weapon meeting a body on the floor, knocked down or staggering moves it as two masses meeting: the weapon's (and the arm's behind it) against the body's at the part struck. How lively it comes off: 0 dead, 1 elastic.")
QVR_SLIDER(vr_melee_ragdoll_most, "Blows on Ragdolls: Most", 0.f, 15.f, 0.5f, "%.1f m/s", ext(),
    "The fastest a blow sends the part of a ragdoll it strikes. 0: no cap.")
QVR_SLIDER(vr_hit_push, "Weapon Hits", 0.f, 3.f, 0.05f, "%.2fx", ext(), "How far heavy weapon hits shove monsters.")
QVR_SLIDER(vr_kill_push, "Killing Blows", 0.f, 3.f, 0.05f, "%.2fx", ext(), "How far killing blows and explosions throw the bodies.")
QVR_SLIDER(vr_parry_push_enemy, "Parry Pushes Enemy", 0.f, 3.f, 0.05f, "%.2fx", ext(),
    "How far a parry pushes the attacker back (Parry's Parry Pushback, the same setting).")
QVR_SLIDER(vr_parry_push_player, "Parry Pushes You", 0.f, 3.f, 0.05f, "%.2fx", ext(), nullptr)
QVR_SLIDER(vr_melee_push_player, "Monsters' Blows Push You", 0.f, 3.f, 0.05f, "%.2fx", ext(), nullptr)
QVR_TOGGLE(vr_pain_knock, "Hits Knock Your Hands",
    "Being hit knocks your hands, arms and what they hold away from where the hit came from, more the "
    "hand on that side, and they ease back. Looks only: your aim and your shots don't move.")
QVR_SLIDER(vr_pain_knock_strength, "Knock per Damage", 0.f, 1.f, 0.05f, "%.2f cm", ext(0.f, 5.f),
    "How far the hands are knocked for each point of damage, for small hits (a grunt's volley: about 16); "
    "harder ones curve towards Largest Knock.")
QVR_SLIDER(vr_pain_knock_max, "Largest Knock", 0.f, 15.f, 0.5f, "%.1f cm", ext(0.f, 50.f),
    "How far the hardest hits knock the hands (a rocket nearly that far; a harder hit always a little "
    "further; hits in quick succession add up to no more). Away from the hit and up.")
QVR_SLIDER(vr_pain_knock_tip, "Knock Tip", 0.f, 5.f, 0.25f, "%.2f deg/cm", ext(0.f, 20.f),
    "How much the hands and what they hold tip up for each cm they are knocked (a gun's barrel kicks up), "
    "about the wrist.")
QVR_TOGGLE(vr_pain_knock_seen, "Knock Across Your View",
    "On: the knock's size is what you see, across your view (a hit from ahead throws the hands up and "
    "towards you, from the side sideways). Off: straight away from the hit and up, which for hands held "
    "ahead of and below your eyes is mostly towards your eyes, where it barely shows.")
QVR_SLIDER(vr_pain_knock_time, "Knock Time", 0.1f, 1.5f, 0.05f, "%.2f s", ext(0.05f, 3.f),
    "How long the knock takes: out quickly, held a moment, then eased back.")

// vr_menu_gameplay.inc: pageParryBash
QVR_TOGGLE_BYNAME(vr_parry, "Parry",
    "Hold a weapon (sword, axe or gun, one hand or two) level across in front of you to block monsters' melee blows.")
QVR_SLIDER(vr_parry_stagger, "Parry Stagger", 0.1f, 1.5f, 0.05f, "%.2f s", ext(0.1f, 3.f),
    "With Parry Balance off: how long the monster pauses after a successful parry before it can move and "
    "attack again.")
QVR_TOGGLE(vr_parry_ragdoll, "Parry Stagger as Ragdoll", nullptr)
QVR_SLIDER_BYNAME(vr_parry_reduction, "Parry Damage Reduction", 0.f, 1.f, 0.05f, "%.2f", noExt, "Share of a parried blow's damage taken away.")
QVR_SLIDER_BYNAME(vr_parry_drop_chance, "Parry Drop Chance", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "Chance a one-handed parry knocks the weapon out of your hand (two hands: never). Not used with Parry Stamina on (Parry, Bash and Headbutt), which replaces it.")
QVR_SLIDER_BYNAME(vr_parry_wobble, "Parry Arm Knock", 0.f, 2.f, 0.1f, "%.1f", ext(), "How much a parried blow knocks your hand and arm.")
QVR_TOGGLE(vr_parry_unarmed, "Unarmed Parry",
    "Cross your arms in an X in front of you to block a blow with your forearms.")
QVR_TOGGLE(vr_dirparry, "Directional Parry (Experimental)",
    "An armed humanoid's sword, axe or club swung at you is parried only by your weapon held across its "
    "blade's way where they meet: an overhead strike by a weapon held level across, a side swing by one "
    "held upright. Parried: no damage, sparks where the weapons met, its swing stopped. Tunables: Combat "
    "> Directional Parry. Off: the level-across parry for every blow.")
QVR_TOGGLE(vr_parry_cooldown_attack, "Parry Cooldown: Whole Attack", nullptr)
QVR_TOGGLE(vr_parry_balance, "Parry Balance",
    "A parry's strength (its tool, times its grip, times its timing) over the monster's Parry Poise (Monster "
    "Groups) decides what it does: under Blocked Below the blow is only blocked (its damage cut, its attack "
    "goes on), under 1 it flinches, up to 2 it staggers back as a ragdoll, more a longer stagger that may "
    "take it down stairs or a slope. Off: every parry staggers it for Parry Stagger.")
QVR_SLIDER(vr_parry_stagger_min, "Stagger at 1", 0.1f, 2.f, 0.05f, "%.2f s", ext(0.1f, 3.f),
    "Strength over poise 1: it staggers back as a ragdoll this long (to Stagger at 2 at 2).")
QVR_SLIDER(vr_parry_stagger_max, "Stagger at 2", 0.1f, 2.f, 0.05f, "%.2f s", ext(0.1f, 3.f),
    "Strength over poise 2: its ragdoll stagger. Up to 2 it never goes down.")
QVR_SLIDER(vr_parry_stagger_heavy, "Heavy Stagger", 0.1f, 3.f, 0.05f, "%.2f s", ext(0.1f, 3.f),
    "Strength over poise 3 and over: its stagger (from Stagger at 2 over 2); over 2 it may go down stairs, "
    "a ledge or a slope behind it.")
QVR_SLIDER(vr_poise_chances, "Poise Against Melee", 0.f, 2.f, 0.1f, "%.1f", ext(0.f, 5.f),
    "A heavy group (Parry Poise over 1; Off as 3): your shoves', blows' and flung props' chances to knock "
    "it down or unbalance it divided by its poise to this power, so weapons and explosives stay the "
    "answer to ogres and the like (0: not).")
QVR_TOGGLE(vr_bash, "Bash",
    "The parry stance (a weapon level across in front, one hand or two) pushed straight forward bashes: knocks monsters back and staggers them. Open palms facing a monster pushed at it shove it. Two hands (a weapon held two-handed, a palm pushing on the blade, both palms) push harder and further.")
QVR_SLIDER(vr_bash_speed, "Bash Speed", 0.3f, 3.f, 0.1f, "%.1f m/s", ext(),
    "How fast the stance (a weapon level across, held half a second) must be pushed forward, both its ends going ahead. A swing passing through the stance doesn't bash.")
QVR_SLIDER(vr_shove_push_onehand, "One-Hand Push", 0.f, 2.f, 0.05f, "%.2fx", ext(0.f, 5.f),
    "A one-handed shove's or bash's push, times Shove and Bash Push.")
QVR_SLIDER(vr_shove_push_twohand, "Two-Hand Push", 0.f, 2.f, 0.05f, "%.2fx", ext(0.f, 5.f),
    "A two-handed shove's or bash's push (both palms, or a weapon held in two hands), times Shove and "
    "Bash Push. Its distance goes by the push's square (1.12: 1.25 times as far as 1).")
QVR_SLIDER(vr_bash_sound, "Bash and Parry Sounds", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "Volume of the sounds that tell a shove, a weapon bash, a counter bash (a bash right after a parry) and a parry apart from your blows (0: silent).")
QVR_TOGGLE(vr_counter, "Counter-Attacks",
    "After a parry (a weapon's or crossed arms), your next melee attack in the window is a counter and hits harder: a blow with either hand, a bash or a shove. One a parry.")
QVR_SLIDER(vr_counter_sound, "Counter Sounds", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "Volume of a blade's shing as the window opens and of the heavy strike as a counter lands (0 off).")
QVR_TOGGLE(vr_counter_glow, "Counter Window Glow",
    "While the counter's window is open, what your hands hold (a bare fist too) glows gold round its edges and sheds golden embers, fading as the window closes. Off as shipped.")

// vr_menu_gameplay.inc: pageStamina
QVR_TOGGLE(vr_parry_stamina, "Parry Stamina",
    "Parrying tires you: each parry with a weapon costs stamina (less with the weapon in two hands), and the parry that leaves you none knocks the weapon out of your hand. It comes back once you stop parrying for a moment. On, it replaces the Parry Drop Chance (Melee Settings). Crossed arms cost nothing.")
QVR_TOGGLE(vr_shove_stamina, "Shove Stamina",
    "Shoves and bashes that land cost stamina from the same pool as parries. Without enough left, they throw back less and hurt less (Exhausted Knockback, Exhausted Damage).")
QVR_TOGGLE(vr_strike_stamina, "Strike Stamina",
    "Blows that land on something that takes damage cost stamina from the same pool, once a swing, however many things it goes through. Without enough left, they hurt less (Exhausted Damage). Blows on walls cost nothing.")
QVR_TOGGLE(vr_climb_stamina, "Climbing Stamina",
    "Hanging from a ledge or a rung (Climbing) spends stamina from the same pool, and none comes back while you hang. With none left your hands let go. More on the Climbing page.")
QVR_SLIDER(vr_fatigue_shake, "Shake Distance", 0.f, 2.f, 0.1f, "%.1f cm", ext(0.f, 10.f),
    "How far your drawn hands, arms and what they hold shake at most, with no stamina left. 0: no moving shake.")
QVR_SLIDER(vr_fatigue_shake_angle, "Shake Turn", 0.f, 5.f, 0.25f, "%.2f deg", ext(0.f, 20.f),
    "How far they turn as they shake at most, with no stamina left. 0: no turning shake.")
QVR_TOGGLE(vr_fatigue_shake_always, "Shake Whenever Tired",
    "Off: your arms shake only while you hang from a ledge or a rung. On: whenever your stamina is low (after parries, shoves and blows too).")
QVR_TOGGLE(vr_stamina_speed, "Slower When Tired",
    "As your stamina runs low, you can't run as fast: full speed from half your stamina (From Stamina, on the Aiming page's Tired Arms) down to Slowest Run with none left, on the same curve as the weight.")

// vr_menu_gameplay.inc: pageBatting
QVR_TOGGLE(vr_deflect, "Bat Back Projectiles",
    "Swing a weapon (a gun, a melee weapon, or a club: a wall torch, a brick) through a monster's spike, laser, spit or grenade to bat it away as a bat hits a ball: off the weapon's face and the way it swings, faster the harder you swing. A bash sends it the way you push. Empty hands never bat: they catch grenades.")
QVR_SLIDER(vr_deflect_speed, "Batting Swing Speed", 0.2f, 1.5f, 0.05f, "%.2fx", ext(),
    "How fast a batting swing must be, times Swing Speed (a hit needs 1x, and more for a swung weapon).")
QVR_SLIDER(vr_deflect_window, "Batting Timing", 0.f, 0.5f, 0.05f, "%.2f s", ext(),
    "How early you may swing: the weapon's path keeps batting this long after it passed.")
QVR_TOGGLE(vr_deflect_swing, "Batting Needs a Swing", nullptr)
QVR_SLIDER(vr_bash_deflect_window, "Bash Batting Timing", 0.f, 1.f, 0.05f, "%.2f s", ext(), "How long a bash goes on batting after the push.")
QVR_CYCLE(vr_grenade_catch, "Catch Grenades",
    "Grenades bounce and roll as physics objects, and you can catch them (by hand or force grab) and throw them "
    "back; thrown, they burst on a monster as an ogre's does on you. \"And yours\" (the default): your grenade "
    "launcher's too.",
    {0.f, "Off"}, {1.f, "Ogres'"}, {2.f, "Ogres' and yours"})
QVR_TOGGLE(vr_grenade_fuse_regrab, "Fuse Resets Every Catch",
    "Off: the fuse is set once, as you catch the monster's throw; dropped and caught again, it keeps running. On: "
    "every catch sets it again.")
QVR_TOGGLE(vr_grenade_return_full, "Returned Grenades Hit Like Yours",
    "A grenade you throw or bat back (an ogre's or your own) goes off as your grenade launcher's: its damage and "
    "radius, credited to you, with your Quad. Off: an ogre's keeps its own, weaker blast.")
QVR_TOGGLE(vr_grenade_shoot, "Shoot Grenades",
    "A shot, a nail, a rocket, the lightning, a thrown thing or another blast sets a grenade off where it is: "
    "an ogre's in flight, yours on the ground, a hand grenade's dud left as a trap. Whoever set it off gets "
    "the kills. Not one in your hand.")
QVR_TOGGLE(vr_grenade_shoot_melee, "Blows Set Grenades Off",
    "A very strong melee blow (the speeds and damage below) or a running chainsaw sets an enemy's grenade "
    "off. Any other blow bats it away: one lying or rolling flies off the way the blow went; one in flight "
    "is batted by a weapon's swing and caught by an empty hand. Your own grenades your blows pass through. "
    "Off: blows pass through grenades.")
QVR_TOGGLE(vr_handgrenade, "Hand Grenades",
    "Reach for the pouch on your lower back with an empty hand and grip: a grenade from it, while you have "
    "rockets (the grenade launcher's ammo; one leaves your ammo with each grenade). Throw it as anything you "
    "carry: it goes off as the launcher's grenade. Where the pouch is: below.")
QVR_CYCLE(vr_handgrenade_arm, "Arm Hand Grenades",
    "Trigger: press it while holding the grenade to pull the pin (it fizzes; the fuse runs); let go of unarmed, "
    "it is a dud you can pick up again. When let go of: the lever flies off as it leaves your hand. Either "
    "way, let go of unarmed at the pouch, it goes back in.",
    {0.f, "Trigger pulls the pin"}, {1.f, "When let go of"})

// vr_menu_gore.inc: pageSmallGibs
QVR_TOGGLE(vr_smallgibs, "Small Gibs",
    "Hits tear small chunks of meat out of monsters and corpses: the gibs' models, a rock's size or half of it. They bleed, can be picked up and thrown, and fade away after a while.")
QVR_TOGGLE(vr_smallgibs_player, "From You Too", "Hits on you tear them out of you as well.")
QVR_SLIDER(vr_smallgibs_min_damage, "Least Damage", 0.f, 100.f, 1.f, "%.0f", ext(0.f, 500.f),
    "A hit with less damage tears none out (a shotgun blast's pellets count as one hit). Above it the chance rises.")
QVR_SLIDER(vr_smallgibs_full_damage, "Damage for a Sure One", 1.f, 200.f, 5.f, "%.0f", ext(1.f, 1000.f),
    "From this damage a hit always tears some out (times the weapon's chance below). Quad Damage, the chainsaw, gibbing and bursting a large gib always do.")
QVR_SLIDER(vr_smallgibs_curve, "Chance Curve", 0.5f, 3.f, 0.1f, "%.1f", noExt,
    "How the chance rises between the two: 1 straight, more makes light hits rarer (a plain shotgun blast) while hard ones stay sure.")
QVR_SLIDER(vr_smallgibs_damage_per_gib, "One More Each", 5.f, 200.f, 5.f, "%.0f damage", ext(1.f, 1000.f),
    "Harder hits tear more out: one more for about each this much damage.")
QVR_SLIDER(vr_smallgibs_per_hit, "Most From a Hit", 1.f, 10.f, 1.f, "%.0f", noExt, nullptr)
QVR_SLIDER(vr_smallgibs_shots, "Shotguns", 0.f, 3.f, 0.1f, "%.1fx", noExt,
    "The shotgun's blast rarely tears one out, the double-barrelled one's often (their pellets' damage summed). A grunt's gun too.")
QVR_SLIDER(vr_smallgibs_nails, "Nails", 0.f, 3.f, 0.1f, "%.1fx", noExt, "Nails and lasers (0: never, but under Quad Damage).")
QVR_SLIDER(vr_smallgibs_blades, "Blades", 0.f, 3.f, 0.1f, "%.1fx", noExt,
    "The axe's and a sword's edge or point, swung or thrown fast.")
QVR_SLIDER(vr_smallgibs_blunt, "Blunt Blows", 0.f, 3.f, 0.1f, "%.1fx", noExt,
    "Punches, a pommel or hilt, a gun or the crowbar swung, Mjolnir, bashes and headbutts: only strong ones, rarely.")
QVR_SLIDER(vr_smallgibs_props, "Props", 0.f, 3.f, 0.1f, "%.1fx", noExt,
    "A rock, a brick or another thing thrown hard or swung in the fist.")
QVR_SLIDER(vr_smallgibs_explosions, "Explosions", 0.f, 3.f, 0.1f, "%.1fx", noExt, nullptr)
QVR_SLIDER(vr_smallgibs_other, "Everything Else", 0.f, 3.f, 0.1f, "%.1fx", noExt, "Lightning, monsters' blows and the rest.")
QVR_SLIDER(vr_smallgibs_saw_interval, "Chainsaw: One Each", 0.05f, 1.f, 0.05f, "%.2f s", noExt,
    "The running chain tears one out of a monster or a corpse this often.")
QVR_SLIDER(vr_smallgibs_gibbing, "With a Gibbing", 0.f, 20.f, 1.f, "%.0f", noExt, "How many fly with a body's gibs when it is gibbed.")
QVR_SLIDER(vr_smallgibs_burst, "A Large Gib Bursts Into", 0.f, 10.f, 1.f, "%.0f", noExt,
    "How many a gib breaks into when it is destroyed (a head half as many again).")
QVR_TOGGLE(vr_smallgibs_brains, "Brain Chunks",
    "A head that bursts (a head gib destroyed, or a head popped by a headshot) throws torn lumps of brain too: small gibs in all else (picked up, thrown, bleeding, fading), lighter.")
QVR_SLIDER(vr_smallgibs_brains_burst, "From a Head Gib", 0.f, 20.f, 1.f, "%.0f", noExt,
    "How many when a head gib is destroyed (with its small gibs of meat).")
QVR_SLIDER(vr_smallgibs_brains_pop, "From a Head Pop", 0.f, 20.f, 1.f, "%.0f", noExt,
    "How many when a headshot pops a head (Decapitation > Head Shots).")
QVR_SLIDER(vr_smallgibs_brains_size, "Brain Chunk Size", 0.2f, 2.f, 0.05f, "%.2fx", noExt, "Times the small gibs' size (Smallest, Largest).")
QVR_SLIDER(vr_smallgibs_brains_mass, "Brain Chunk Mass", 0.05f, 2.f, 0.05f, "%.2f kg", noExt, nullptr)
QVR_SLIDER(vr_smallgibs_brains_gloss, "Brain Chunk Gloss", 0.f, 2.f, 0.1f, "%.1fx", noExt,
    "Their wet sheen, always on: the highlights on the folds and a warm, reddish rim; 0 dry and matt "
    "(vr_smallgibs_brains_gloss).")
QVR_SLIDER(vr_smallgibs_brains_jiggle, "Brain Chunk Jiggle", 0.f, 4.f, 0.25f, "%.2fx", noExt,
    "They wobble when knocked (landing, bouncing, shaken in a hand): squashed and bulging, settling in "
    "half a second. Drawn only: their physics stay rigid. 0 off (vr_smallgibs_brains_jiggle).")
QVR_SLIDER(vr_smallgibs_brains_speed, "Brain Chunk Speed", 0.f, 10.f, 0.1f, "%.1f m/s", ext(0.f, 30.f),
    "How fast brain chunks fly out of a head burst: their own, not Speed below (times A Large Gib Bursts: "
    "Speed). The default is Speed's, which is what they used then (vr_smallgibs_brains_speed).")
QVR_SLIDER(vr_smallgibs_brains_up, "Brain Chunk Up", 0.f, 10.f, 0.1f, "%.1f m/s", ext(0.f, 30.f),
    "And upwards: their own, not Up below (times A Large Gib Bursts: Up) (vr_smallgibs_brains_up).")
QVR_SLIDER(vr_smallgibs_speed, "Speed", 0.f, 10.f, 0.1f, "%.1f m/s", ext(0.f, 30.f),
    "How fast they fly out from where the hit landed (times Flight by Situation below).")
QVR_SLIDER(vr_smallgibs_up, "Up", 0.f, 10.f, 0.1f, "%.1f m/s", ext(0.f, 30.f), "And upwards (times Flight by Situation below).")
QVR_SLIDER(vr_smallgibs_grace, "Pass Through the Body", 0.f, 0.5f, 0.05f, "%.2f s", noExt,
    "For this long they don't collide with the body they came from.")
QVR_TOGGLE(vr_smallgibs_pass_inside, "Not Pushed Out of Bodies",
    "Still inside a monster, you, a hand or a blade after that (melee, the chainsaw), they pass through it until clear, not pushed out at 2 to 3 m/s.")
QVR_SLIDER(vr_smallgibs_blow_grace, "Not Batted Away For", 0.f, 1.f, 0.05f, "%.2f s", noExt,
    "For this long after it is torn out, your hands, weapons and what they hold pass through a small gib: the blade that tore it out doesn't bat it away. After it you can bat it.")
QVR_TOGGLE(vr_smallgibs_blow_burst, "Burst by Blows Meanwhile",
    "In that time a blow still bursts one (the blade that tore it out too, going on through it), with Can Be Destroyed on; it is never batted or pushed. Off: blows pass through it.")
QVR_SLIDER(vr_smallgibs_size_min, "Smallest", 0.1f, 2.f, 0.05f, "%.2fx rock", noExt, nullptr)
QVR_SLIDER(vr_smallgibs_size_max, "Largest", 0.1f, 2.f, 0.05f, "%.2fx rock", noExt, nullptr)
QVR_SLIDER(vr_smallgibs_mass, "Mass", 0.05f, 5.f, 0.05f, "%.2f kg", noExt, "Light: thrown, they hurt little.")
QVR_SLIDER(vr_smallgibs_speed_melee, "Melee: Speed", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f),
    "Speed times this. A hand's blow: a blade, a punch, a pommel or hilt, a rock or brick in the fist; bashes and headbutts.")
QVR_SLIDER(vr_smallgibs_up_melee, "Melee: Up", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f), "Up times this.")
QVR_SLIDER(vr_smallgibs_speed_saw, "Chainsaw: Speed", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f),
    "Speed times this. The chainsaw's running chain or a swing of it.")
QVR_SLIDER(vr_smallgibs_up_saw, "Chainsaw: Up", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f), "Up times this.")
QVR_SLIDER(vr_smallgibs_speed_guns, "Guns: Speed", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f),
    "Speed times this. Shotguns, nails, lasers, lightning, a grunt's gun.")
QVR_SLIDER(vr_smallgibs_up_guns, "Guns: Up", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f), "Up times this.")
QVR_SLIDER(vr_smallgibs_speed_explosions, "Explosions: Speed", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f), "Speed times this. Rockets, grenades, every blast.")
QVR_SLIDER(vr_smallgibs_up_explosions, "Explosions: Up", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f), "Up times this.")
QVR_SLIDER(vr_smallgibs_speed_thrown, "Thrown: Speed", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f),
    "Speed times this. A thrown axe, sword, rock, brick or gib; a projectile batted back.")
QVR_SLIDER(vr_smallgibs_up_thrown, "Thrown: Up", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f), "Up times this.")
QVR_SLIDER(vr_smallgibs_speed_other, "Everything Else: Speed", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f), "Speed times this. Monsters' blows and the rest.")
QVR_SLIDER(vr_smallgibs_up_other, "Everything Else: Up", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f), "Up times this.")
QVR_SLIDER(vr_smallgibs_speed_gibbing, "Gibbing a Live One: Speed", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f),
    "Speed times this. A living monster gibbed: they fly out of its whole body, with its gibs' push.")
QVR_SLIDER(vr_smallgibs_up_gibbing, "Gibbing a Live One: Up", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f), "Up times this.")
QVR_SLIDER(vr_smallgibs_speed_corpse, "Gibbing a Corpse: Speed", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f),
    "Speed times this. A corpse gibbed where it lies (struck, shot, blown up).")
QVR_SLIDER(vr_smallgibs_up_corpse, "Gibbing a Corpse: Up", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f), "Up times this.")
QVR_SLIDER(vr_smallgibs_speed_burst, "A Large Gib Bursts: Speed", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f),
    "Speed times this. A gib or head destroyed breaks into them.")
QVR_SLIDER(vr_smallgibs_up_burst, "A Large Gib Bursts: Up", 0.f, 3.f, 0.01f, "%.2fx", ext(0.f, 10.f), "Up times this.")
QVR_SLIDER(vr_smallgibs_max, "Most Lying About", 1.f, 128.f, 1.f, "%.0f", ext(1.f, 256.f), "Past it, the oldest go first (not one in your hand).")
QVR_SLIDER(vr_smallgibs_time, "Last", 0.f, 120.f, 5.f, "%.0f s", noExt,
    "Then they fade away (0: never). Held, and in the air after, they wait.")
QVR_TOGGLE(vr_smallgibs_destroy, "Can Be Destroyed",
    "Shots and blows burst them into a puff of blood. Off: they pass through them.")
QVR_TOGGLE(vr_smallgibs_stomp, "Pop Underfoot",
    "Landing on them (a jump, a drop, a stomp) pops the meat and brain chunks under your feet.")
QVR_SLIDER(vr_smallgibs_stomp_speed, "Pop Landing Speed", 50.f, 600.f, 10.f, "%.0f u/s", noExt,
    "How fast you must land on them to pop them (a jump from the same level lands at about 270).")

// vr_menu_gore.inc: pageSmallGibsEnemies
QVR_SLIDER(vr_smallgibs_mult_grunt, "Grunt", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f),
    "Its small gibs, times this: what a hit tears out, what flies with its gibs, what its gibs and head burst into (vr_smallgibs_mult_grunt).")
QVR_SLIDER(vr_smallgibs_mult_enforcer, "Enforcer", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "An enforcer's (vr_smallgibs_mult_enforcer).")
QVR_SLIDER(vr_smallgibs_mult_dog, "Rottweiler", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A rottweiler's (vr_smallgibs_mult_dog).")
QVR_SLIDER(vr_smallgibs_mult_fiend, "Fiend", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A fiend's (vr_smallgibs_mult_fiend).")
QVR_SLIDER(vr_smallgibs_mult_ogre, "Ogre", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "An ogre's (vr_smallgibs_mult_ogre).")
QVR_SLIDER(vr_smallgibs_mult_knight, "Knight", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A knight's (vr_smallgibs_mult_knight).")
QVR_SLIDER(vr_smallgibs_mult_hellknight, "Hell Knight", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A hell knight's (vr_smallgibs_mult_hellknight).")
QVR_SLIDER(vr_smallgibs_mult_vore, "Vore", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A vore's (vr_smallgibs_mult_vore).")
QVR_SLIDER(vr_smallgibs_mult_shambler, "Shambler", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A shambler's (vr_smallgibs_mult_shambler).")
QVR_SLIDER(vr_smallgibs_mult_scrag, "Scrag", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A scrag's (vr_smallgibs_mult_scrag).")
QVR_SLIDER(vr_smallgibs_mult_fish, "Rotfish", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A rotfish's (vr_smallgibs_mult_fish).")
QVR_SLIDER(vr_smallgibs_mult_gremlin, "Gremlin (Hipnotic)", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A gremlin's (vr_smallgibs_mult_gremlin).")
QVR_SLIDER(vr_smallgibs_mult_scourge, "Centroid (Rogue)", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A centroid's (vr_smallgibs_mult_scourge).")
QVR_SLIDER(vr_smallgibs_mult_eel, "Eel (Rogue)", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "An eel's (vr_smallgibs_mult_eel).")
QVR_SLIDER(vr_smallgibs_mult_zombie, "Zombie (Beheaded)", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A zombie's, beheaded or not (vr_smallgibs_mult_zombie).")
QVR_SLIDER(vr_smallgibs_mult_mummy, "Mummy (Rogue, Beheaded)", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A mummy's (vr_smallgibs_mult_mummy).")
QVR_SLIDER(vr_smallgibs_brains_mult_grunt, "Grunt", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f),
    "Its brain chunks, times this: from its head gib destroyed or its head popped by a headshot. A head it "
    "lost carries them with it (vr_smallgibs_brains_mult_grunt).")
QVR_SLIDER(vr_smallgibs_brains_mult_enforcer, "Enforcer", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "An enforcer's (vr_smallgibs_brains_mult_enforcer).")
QVR_SLIDER(vr_smallgibs_brains_mult_dog, "Rottweiler", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A rottweiler's (vr_smallgibs_brains_mult_dog).")
QVR_SLIDER(vr_smallgibs_brains_mult_fiend, "Fiend", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A fiend's (vr_smallgibs_brains_mult_fiend).")
QVR_SLIDER(vr_smallgibs_brains_mult_ogre, "Ogre", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "An ogre's (vr_smallgibs_brains_mult_ogre).")
QVR_SLIDER(vr_smallgibs_brains_mult_knight, "Knight", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A knight's (vr_smallgibs_brains_mult_knight).")
QVR_SLIDER(vr_smallgibs_brains_mult_hellknight, "Hell Knight", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A hell knight's (vr_smallgibs_brains_mult_hellknight).")
QVR_SLIDER(vr_smallgibs_brains_mult_vore, "Vore", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A vore's (vr_smallgibs_brains_mult_vore).")
QVR_SLIDER(vr_smallgibs_brains_mult_shambler, "Shambler", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A shambler's (vr_smallgibs_brains_mult_shambler).")
QVR_SLIDER(vr_smallgibs_brains_mult_scrag, "Scrag", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A scrag's (vr_smallgibs_brains_mult_scrag).")
QVR_SLIDER(vr_smallgibs_brains_mult_fish, "Rotfish", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A rotfish's (vr_smallgibs_brains_mult_fish).")
QVR_SLIDER(vr_smallgibs_brains_mult_gremlin, "Gremlin (Hipnotic)", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A gremlin's (vr_smallgibs_brains_mult_gremlin).")
QVR_SLIDER(vr_smallgibs_brains_mult_scourge, "Centroid (Rogue)", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A centroid's (vr_smallgibs_brains_mult_scourge).")
QVR_SLIDER(vr_smallgibs_brains_mult_eel, "Eel (Rogue)", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "An eel's (vr_smallgibs_brains_mult_eel).")
QVR_SLIDER(vr_smallgibs_brains_mult_zombie, "Zombie (Beheaded)", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A zombie's (vr_smallgibs_brains_mult_zombie).")
QVR_SLIDER(vr_smallgibs_brains_mult_mummy, "Mummy (Rogue, Beheaded)", 0.f, 4.f, 0.25f, "%.2fx", ext(0.f, 20.f), "A mummy's (vr_smallgibs_brains_mult_mummy).")

// vr_menu_gore.inc: pageDecapitation
QVR_TOGGLE(vr_decap, "Decapitation",
    "A killing slash at the head of a grunt, knight, ogre, enforcer, death knight, rottweiler, scrag, fiend, shambler, gremlin or mummy cuts it "
    "off: a sword's or an axe's blade swung across (not a stab, not the pommel or the hilt). The body falls at "
    "once as a headless ragdoll, the head flies off, the neck spurts blood. Needs Ragdolls on (vr_decap).")
QVR_SLIDER(vr_decap_speed, "Least Swing Speed", 0.f, 15.f, 0.5f, "%.1f m/s", ext(0.f, 30.f),
    "How fast the blade must move where it strikes (and a thrown axe fly) to cut the head off (vr_decap_speed).")
QVR_SLIDER(vr_decap_slash_angle, "Least Slash Angle", 0.f, 90.f, 5.f, "%.0f deg", noExt,
    "How far off the blade's line the struck point must move to cut (nearer its line it is a stab, which "
    "doesn't behead); 0: any motion (vr_decap_slash_angle).")
QVR_SLIDER(vr_decap_head_size, "Head Zone Size", 0.5f, 2.f, 0.05f, "%.2fx", ext(0.1f, 4.f),
    "Melee only (a slash, the chainsaw, a corpse's head): how big the head zone a blade must strike to behead is, "
    "times its size for headshots. Debug > Show Hit Zones draws it (vr_decap_head_size).")
QVR_SLIDER(vr_decap_neck, "Neck", 0.f, 20.f, 1.f, "%.0f units", ext(0.f, 40.f),
    "Melee only: the head zone reaches this far down below the head's middle, over the neck (a ragdoll's neck "
    "at least this far round it) (vr_decap_neck).")
QVR_SLIDER(vr_decap_head_speed, "Head Speed", 0.f, 1.5f, 0.05f, "%.2fx", ext(0.f, 5.f),
    "How much of the blade's speed the head flies off with; it spins as the swing turns it "
    "(vr_decap_head_speed).")
QVR_SLIDER(vr_decap_head_lift, "Head Thrown Up", 0.f, 400.f, 10.f, "%.0f u/s", ext(0.f, 1000.f),
    "And how fast it is thrown upwards besides (vr_decap_head_lift).")
QVR_SLIDER(vr_decap_fountain, "Fountain", 0.f, 8.f, 0.25f, "%.2f s", ext(0.f, 30.f),
    "How long the neck spurts blood, in beats, dying away (0: none) (vr_decap_fountain).")
QVR_SLIDER(vr_limbs_end_fountain, "Flying Head's Fountain", 0.f, 4.f, 0.25f, "%.2f s", ext(0.f, 20.f),
    "How long the neck of a head cut off spurts a smaller fountain as it flies (and the cut end of a limb: "
    "Limb Gore's Cut End Fountain, the same setting) (0: none) (vr_limbs_end_fountain).")
QVR_TOGGLE(vr_decap_corpses, "Corpses",
    "A slash at a ragdoll's (or a dying body's) head cuts it off instead of hurting the corpse: it never "
    "bursts into gibs from it (vr_decap_corpses).")
QVR_TOGGLE(vr_decap_zombies, "Zombies",
    "A slash at a zombie's head cuts it off whatever the damage: it falls as a headless ragdoll and never "
    "gets up again. Besides gibbing it, the only way to kill one (vr_decap_zombies).")
QVR_TOGGLE(vr_decap_thrown, "Thrown Axes",
    "An axe thrown into a head edge first (as it sticks) cuts it off when it kills (vr_decap_thrown).")
QVR_TOGGLE(vr_decap_chainsaw, "Chainsaw",
    "The chainsaw's running bar at the neck (or a swing of it) cuts the head off when it kills; a corpse's "
    "at once (vr_decap_chainsaw).")
QVR_TOGGLE(vr_decap_shotgun, "Shotgun",
    "A shotgun blast that kills with a headshot pops the head: it bursts in blood and small gibs, and the body "
    "falls at once as a headless ragdoll (never gibbed, nor its death animation) (vr_decap_shotgun).")
QVR_TOGGLE(vr_decap_super_shotgun, "Super Shotgun",
    "The same for a super shotgun blast (vr_decap_super_shotgun).")
QVR_TOGGLE(vr_decap_revolver, "Heavy Revolver", "The same for a heavy revolver's slug (vr_decap_revolver).")
QVR_TOGGLE(vr_decap_lightning, "Lightning Gun",
    "The same for a lightning gun bolt at the head that kills (vr_decap_lightning).")
QVR_SLIDER(vr_decap_head_share, "Blast's Head Share", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much of a blast's damage on him must come from pellets at the head (the headshot multiplier in) for "
    "it to count as a headshot; 0: any pellet at the head (vr_decap_head_share).")
QVR_SLIDER(vr_decap_pop_body_speed, "Body's Speed After a Pop", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "A popped head's body: how much of the shot's knock its headless ragdoll keeps (its own motion, as it ran, "
    "in full: Keeps Its Own Motion). The shot's force went into the head, so it slumps where it stood; 1: flung "
    "as before (vr_decap_pop_body_speed).")
QVR_SLIDER(vr_decap_chance_scale, "Head Chance", 0.f, 2.f, 0.05f, "%.2fx", ext(0.f, 10.f),
    "Every beheading or head pop's chance times this (a blade's sure cut too: below 1 it may fail); Quad Damage "
    "always pops. Limbs: Gore > Limb Gore (vr_decap_chance_scale).")
QVR_SLIDER(vr_decap_pop_always_range, "Always Within", 0.f, 10.f, 0.5f, "%.1f lengths", ext(0.f, 50.f),
    "A shotgun's or super shotgun's headshot kill this near (in player lengths, 56 units: about 1.75 m) "
    "always pops the head (vr_decap_pop_always_range).")
QVR_SLIDER(vr_decap_pop_never_range, "Never Beyond", 1.f, 40.f, 1.f, "%.0f lengths", ext(0.f, 200.f),
    "This far or farther it never does; between the two, the chance falls off (vr_decap_pop_never_range).")
QVR_SLIDER(vr_decap_pop_ssg_scale, "Super Shotgun Chance", 0.f, 3.f, 0.05f, "%.2fx", ext(0.f, 10.f),
    "The super shotgun's chance between the ranges, times this (vr_decap_pop_ssg_scale).")
QVR_SLIDER(vr_decap_pop_ssg_falloff, "Super Shotgun Falloff", 0.f, 5.f, 0.1f, "%.1f", ext(0.f, 20.f),
    "How fast its chance dies away with range: 1 straight down to the far range, higher sooner, 0 not at all "
    "(vr_decap_pop_ssg_falloff).")
QVR_SLIDER(vr_decap_pop_revolver_scale, "Heavy Revolver Chance", 0.f, 3.f, 0.05f, "%.2fx", ext(0.f, 10.f),
    "The heavy revolver's chance between the ranges, times this: a slug, all of it at the head, pops it "
    "often (vr_decap_pop_revolver_scale).")
QVR_SLIDER(vr_decap_pop_revolver_falloff, "Heavy Revolver Falloff", 0.f, 5.f, 0.1f, "%.1f", ext(0.f, 20.f),
    "How fast its chance dies away with range (gentler than the shotguns') (vr_decap_pop_revolver_falloff).")
QVR_SLIDER(vr_decap_pop_sg_scale, "Shotgun Chance", 0.f, 3.f, 0.05f, "%.2fx", ext(0.f, 10.f),
    "The shotgun's chance between the ranges, times this (vr_decap_pop_sg_scale).")
QVR_SLIDER(vr_decap_pop_sg_falloff, "Shotgun Falloff", 0.f, 5.f, 0.1f, "%.1f", ext(0.f, 20.f),
    "How fast the shotgun's chance dies away with range (stronger than the super shotgun's) "
    "(vr_decap_pop_sg_falloff).")
QVR_SLIDER(vr_decap_pop_pellet_weight, "Pellets at the Head", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much the share of the blast's pellets that struck the head weighs on the chance: 1 in proportion, "
    "0 not at all (vr_decap_pop_pellet_weight).")
QVR_TOGGLE(vr_decap_pop_lightning_always, "Lightning Always Pops",
    "A lightning bolt's headshot kill always pops the head, at any range; off: the ranges, with the super "
    "shotgun's falloff (vr_decap_pop_lightning_always).")
QVR_SLIDER(vr_decap_pop_laser, "Enforcer Laser", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "An enforcer's laser bolt that kills a monster with a head or limb hit (an enemy's bolt into another "
    "monster, or your own enforcer's rifle) pops it at this chance, at any range (0: never) (vr_decap_pop_laser).")
QVR_TOGGLE(vr_decap_pop_thrown, "Thrown Things",
    "A blunt weapon or prop thrown (or flung) into a head that kills pops it, when heavy enough (not a sword, "
    "an axe or the chainsaw: an axe's edge cuts the head off) (vr_decap_pop_thrown).")
QVR_SLIDER(vr_decap_pop_thrown_mass, "Thrown: Always From", 0.f, 20.f, 0.5f, "%.1f kg", ext(0.f, 200.f),
    "This heavy or heavier, a thrown thing's headshot kill always pops the head (vr_decap_pop_thrown_mass).")
QVR_SLIDER(vr_decap_pop_thrown_light_chance, "Thrown: Lighter's Chance", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "A lighter one's chance (0: never) (vr_decap_pop_thrown_light_chance).")
QVR_SLIDER(vr_decap_pop_fist_scale, "Fist", 0.f, 0.25f, 0.005f, "%.3fx", ext(0.f, 5.f),
    "A punch that kills with a head hit pops the head at this chance at the hardest hit, times the hit's "
    "hardness to the Hardness Curve (the rows below): very rarely by default (vr_decap_pop_fist_scale).")
QVR_SLIDER(vr_decap_pop_gun_scale, "Gun Butt", 0.f, 0.25f, 0.005f, "%.3fx", ext(0.f, 5.f),
    "The same for a gun swung, its butt or a pistol-whip (vr_decap_pop_gun_scale).")
QVR_SLIDER(vr_decap_pop_crowbar_scale, "Crowbar", 0.f, 0.25f, 0.005f, "%.3fx", ext(0.f, 5.f), "The same for the crowbar (vr_decap_pop_crowbar_scale).")
QVR_SLIDER(vr_decap_pop_pommel_scale, "Pommel", 0.f, 0.25f, 0.005f, "%.3fx", ext(0.f, 5.f),
    "The same for a sword's, axe's, chainsaw's or Mjolnir's pommel or handle end (vr_decap_pop_pommel_scale).")
QVR_SLIDER(vr_decap_pop_club_scale, "Club", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 5.f),
    "The same for a carried prop swung as a club, a wall torch (vr_decap_pop_club_scale).")
QVR_SLIDER(vr_decap_pop_mjolnir_scale, "Mjolnir", 0.f, 3.f, 0.05f, "%.2fx", ext(0.f, 10.f),
    "The same for Mjolnir's head: above 1 it is sure well before the hardest hit (1.75: a solid blow nearly "
    "always pops it) (vr_decap_pop_mjolnir_scale).")
QVR_SLIDER(vr_decap_pop_melee_soft_speed, "Soft Hit Speed", 0.f, 10.f, 0.5f, "%.1f m/s", ext(0.f, 30.f),
    "The striking part this slow or slower: no hardness from its speed (vr_decap_pop_melee_soft_speed).")
QVR_SLIDER(vr_decap_pop_melee_hard_speed, "Hard Hit Speed", 1.f, 20.f, 0.5f, "%.1f m/s", ext(0.f, 40.f),
    "This fast or faster: all of it (vr_decap_pop_melee_hard_speed).")
QVR_SLIDER(vr_decap_pop_melee_soft_damage, "Soft Hit Damage", 0.f, 50.f, 1.f, "%.0f", ext(0.f, 500.f),
    "The blow's damage (the headshot multiplier in) this or less: no hardness from it "
    "(vr_decap_pop_melee_soft_damage).")
QVR_SLIDER(vr_decap_pop_melee_hard_damage, "Hard Hit Damage", 1.f, 200.f, 1.f, "%.0f", ext(0.f, 1000.f), "This or more: all of it (vr_decap_pop_melee_hard_damage).")
QVR_SLIDER(vr_decap_pop_melee_damage_weight, "Damage's Weight", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much the damage weighs in the hit's hardness; the rest is its speed (vr_decap_pop_melee_damage_weight).")
QVR_SLIDER(vr_decap_pop_melee_curve, "Hardness Curve", 0.f, 5.f, 0.1f, "%.1f", ext(0.f, 20.f),
    "The hardness to this power: higher, soft blows nearer never; 1 straight (vr_decap_pop_melee_curve).")
QVR_TOGGLE(vr_decap_pop_quad, "Quad Damage: Always Pop",
    "With Quad Damage, every headshot kill pops the head: any gun (nails, rockets and grenades at the head too), "
    "any blow (a blade's stab), any throw or prop, at any range, by no chance (a slash still cuts it off) "
    "(vr_decap_pop_quad).")

// vr_menu_gore.inc: pageLimbGore
QVR_TOGGLE(vr_limbs, "Limb Gore",
    "A monster's arms, legs, tails and the rest come off as its head does (the monsters with ragdolls): a blade's "
    "slash cuts the limb off at the joint nearest the hit (the elbow the forearm, the shoulder the whole arm), "
    "a shot, a blunt blow or a bolt pops it, by the head's chances (Decapitation's rows). A living one only by "
    "the blow that kills it: it falls at once as a ragdoll; the limb flies off (pick it up, throw it), the "
    "stump spurts blood. Needs Ragdolls on (vr_limbs).")
QVR_SLIDER(vr_limbs_chance_scale, "Limb Chance", 0.f, 2.f, 0.05f, "%.2fx", ext(0.f, 10.f),
    "Every limb cut or pop's chance times this (a blade's sure cut too: below 1 it may fail); Quad Damage "
    "always pops (vr_limbs_chance_scale).")
QVR_TOGGLE(vr_limbs_corpses, "Corpses",
    "Corpses and ragdolls lose limbs too, each on its own, down to the torso: a slash cuts, a blunt blow or a "
    "shot pops one (vr_limbs_corpses).")
QVR_TOGGLE(vr_limbs_zombies, "Zombies and Mummies",
    "A zombie or a mummy losing a limb dies for good (a zombie: whatever the damage, as beheaded). Off: their "
    "limbs stay on (their heads: Decapitation > Zombies) (vr_limbs_zombies).")
QVR_SLIDER(vr_limbs_body_speed, "Body's Speed After", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "A living monster's limb cut or popped: how much of the blow's knock its ragdoll keeps (it slumps where it "
    "stood; 1: flung as a kill) (vr_limbs_body_speed).")
QVR_SLIDER(vr_limbs_max, "Most Limbs Lying About", 1.f, 64.f, 1.f, "%.0f", ext(1.f, 256.f),
    "Past it, the oldest go first (not one in your hand) (vr_limbs_max).")
QVR_CYCLE_BYNAME(vr_limbs_prebuild, "Make Limbs as the Map Loads",
    "The Map's: the limbs of every kind of monster the map has (those waiting to appear too) are made as it "
    "loads (about 5 ms each: a tenth of a second or so more), not at their first cut (a dropped frame). And the "
    "Range's: also every kind the firing range's dispensers and dummies can make (half a second more there, "
    "once a session). The next map load (vr_limbs_prebuild).",
    {0.f, "Off"}, {1.f, "The Map's"}, {2.f, "And the Range's"})
QVR_SLIDER(vr_limbs_mass_scale, "Limb Weight", 0.05f, 3.f, 0.05f, "%.2fx", ext(0.05f, 10.f),
    "A limb (or head) cut off weighs its share of its monster's ragdoll Mass by the kind of limb: a whole arm "
    "6.3%, a forearm and hand 2.8%, a hand 0.8%, a whole leg 15.5%, a shin and foot 6%, the head 7% (four "
    "legs share two legs' weight, six arms two arms'; a tail its share of the body's volume), times this; "
    "held and thrown as that (vr_limbs_mass_scale).")
QVR_SLIDER(vr_limbs_blast_radius, "Explosion Reach", 16.f, 256.f, 8.f, "%.0f units", ext(1.f, 1000.f),
    "An explosion pops the limbs near it by chance: a monster it kills falls as a ragdoll without them instead "
    "of bursting into gibs (if none popped: gibbed as before); corpses lose them too. Limbs this near the "
    "blast may pop (vr_limbs_blast_radius).")
QVR_SLIDER(vr_limbs_blast_chance, "Explosion Chance", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "A limb's chance at the blast itself, falling to none at the reach (times Limb Chance) "
    "(vr_limbs_blast_chance).")
QVR_CYCLE(vr_gib_limbs, "Gibbed Bodies Throw Limbs",
    "A body bursting into gibs throws its own arms, legs and tail as well (With the Gibs), or instead of "
    "Quake's meat chunks (Instead of Gibs) (vr_gib_limbs).",
    {0.f, "No"}, {1.f, "With the Gibs"}, {2.f, "Instead of Gibs"})

// vr_menu_gore.inc: pageGore
QVR_CYCLE(vr_gore, "Gore",
    "Over the top: hits spray blood onto the walls, floor and ceiling behind, gibbing paints the room, pools spread under corpses, gibs stick to ceilings and drip.",
    {0.f, "Quake VR"}, {1.f, "More"}, {2.f, "Over the top"})
QVR_SLIDER(vr_gore_sound, "Gore Sounds", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "How loud the gore's own sounds are (0 none): stumps spurting with each pulse, streams splattering on the "
    "floor, limbs tearing off, a hand squelching into a gib or a body, a splash stepping into a pool of blood "
    "and wet steps after it, gibs slapping walls; a choke if on (below). Each kind's own volume below, times "
    "this (vr_gore_sound).")
QVR_TOGGLE(vr_gore_sound_choke, "Gore Sounds: Chokes",
    "A beheaded or face-smashed enemy gargles and chokes instead of crying out (its pain or death cry dropped "
    "for a moment). Off: never, every cry plays (vr_gore_sound_choke).")
QVR_SLIDER(vr_gore_sound_vol_spurt, "Gore Sounds: Spurts", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "A stump's spurt with each pulse of its fountain: its volume, times Gore Sounds (vr_gore_sound_vol_spurt).")
QVR_SLIDER(vr_gore_sound_vol_pour, "Gore Sounds: Streams", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "A blood stream splattering where it lands (vr_gore_sound_vol_pour).")
QVR_SLIDER(vr_gore_sound_vol_tear, "Gore Sounds: Tears", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "The wet rip and bone crack of a limb or a head torn off (vr_gore_sound_vol_tear).")
QVR_SLIDER(vr_gore_sound_vol_squelch, "Gore Sounds: Squelches", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "A hand closing on a gib or a dead body (vr_gore_sound_vol_squelch).")
QVR_SLIDER(vr_gore_sound_vol_splash, "Gore Sounds: Pool Splash", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "Your foot stepping into a pool of blood (vr_gore_sound_vol_splash).")
QVR_SLIDER(vr_gore_sound_vol_step, "Gore Sounds: Wet Steps", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "Your steps for a few seconds after a pool of blood (vr_gore_sound_vol_step).")
QVR_SLIDER(vr_gore_sound_vol_slap, "Gore Sounds: Slaps", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "A gib slapping into a wall or a ceiling (vr_gore_sound_vol_slap).")
QVR_SLIDER(vr_gore_sound_vol_choke, "Gore Sounds: Chokes Volume", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f), "The chokes, when on (vr_gore_sound_vol_choke).")
QVR_SLIDER(vr_gore_sound_vol_drip, "Gore Sounds: Drips", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "A quiet drip as a drop of blood hits the floor: yours dripping from your wounds, from ceilings and from "
    "gibs stuck there (vr_gore_sound_vol_drip).")
QVR_SLIDER(vr_gore_sound_drips, "Gore Sounds: Drips Heard", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The share of the drops you hear (1: every drop; 0.5: every other) (vr_gore_sound_drips).")
QVR_TOGGLE(vr_footprints, "Bloody Footprints",
    "Stepping in a pool of blood or on a bloody body bloodies your feet: your next steps leave bloody boot "
    "prints where your feet come down, fading (vr_footprints).")
QVR_SLIDER(vr_footprint_count, "Footprints", 1.f, 16.f, 1.f, "%.0f", ext(1.f, 32.f), "How many prints after the blood (vr_footprint_count).")
QVR_SLIDER(vr_footprint_fade, "Footprint Fade", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much they fade from the first to the last (0: all as strong; 1: the last faint) (vr_footprint_fade).")
QVR_SLIDER(vr_footprint_size, "Footprint Size", 0.5f, 2.f, 0.05f, "%.2fx", noExt, "Times your foot's own size (vr_footprint_size).")
QVR_SLIDER(vr_footprint_rate, "Footprint Frequency", 0.25f, 2.f, 0.05f, "%.2fx", ext(0.25f, 4.f),
    "A print each step: a step as long as a person's at your speed (0.7 m walking, longer running, up to "
    "2.2 m at a full run), times as many as this (vr_footprint_rate).")
QVR_TOGGLE(vr_handprints, "Bloody Handprints",
    "A hand soaked in blood (a gib held, a bloody blow or cut) pressed on a wall, the floor or a ledge leaves a "
    "bloody handprint there, one a touch, using its blood up (water washes it off) (vr_handprints).")
QVR_SLIDER(vr_handprint_count, "Handprints", 1.f, 16.f, 1.f, "%.0f", ext(1.f, 32.f),
    "How many prints a hand soaked full leaves, fainter as its blood runs out (vr_handprint_count).")
QVR_SLIDER(vr_handprint_size, "Handprint Size", 0.5f, 2.f, 0.05f, "%.2fx", noExt, "Times your hand's own size (vr_handprint_size).")
QVR_TOGGLE(vr_handprint_props, "Handprints on Props",
    "A bloody hand pressed on a crate, a barrel, a box or anything lying about leaves its blood on it, "
    "moving with it (vr_handprint_props).")
QVR_TOGGLE(vr_handprint_gear, "Bloody Grips",
    "A bloody hand leaves its blood where it grips what it holds: your weapon (it stays on it, dropped, "
    "holstered or taken again) or a prop you carry, building up as you hold it, using the hand's blood up "
    "(vr_handprint_gear).")
QVR_SLIDER(vr_gore_spray, "Blood Sprays", 0.f, 3.f, 0.25f, "%.2fx", ext(),
    "How many splats a hit or a gibbing throws onto the walls, floor and ceiling round it (0 none).")
QVR_SLIDER(vr_gore_size, "Splat Size", 0.5f, 2.f, 0.1f, "%.1fx", ext(), "How big the gore's splats, pools and runs are.")
QVR_SLIDER(vr_gore_pools, "Blood Pools", 0.f, 2.f, 0.1f, "%.1fx", ext(),
    "Pools of blood spreading under corpses and gibs: their size (0 none).")
QVR_SLIDER(vr_gore_drips, "Dripping", 0.f, 3.f, 0.25f, "%.2fx", ext(),
    "Blood dripping from splats on the ceiling and from gibs stuck there: how long and how much (0 none).")
QVR_SLIDER(vr_gore_stick, "Gibs Stick", 0.f, 30.f, 1.f, "%.0f s", ext(),
    "Gibs flung into a ceiling or a wall may stick there about this long, dripping, then fall (0 never).")
QVR_SLIDER(vr_gore_stick_thrown, "Thrown Gibs Stick", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The chance a gib or head (or a small gib) you throw into a wall or a ceiling sticks there (faster than Gib Splat Speed it bursts instead).")
QVR_SLIDER(vr_gore_stick_speed, "Speed to Stick", 50.f, 500.f, 10.f, "%.0f u/s", ext(20.f, 1000.f),
    "How fast a gib must strike a wall or a ceiling to stick (flung from a body or thrown).")
QVR_TOGGLE(vr_head_flies, "Flies on Heads",
    "Flies buzzing round some severed heads (Scourge of Armagon's: about one head in ten). Off "
    "by default.")
QVR_SLIDER(vr_gib_speed_melee, "Gib Speed: Melee", 0.05f, 1.5f, 0.05f, "%.2fx", ext(0.05f, 3.f),
    "How fast the gibs fly when a melee blow gibs a monster or a corpse (a swing, a bash, a shove, a headbutt): "
    "times Quake's speed.")
QVR_SLIDER(vr_gib_speed_light, "Gib Speed: Light Weapons", 0.05f, 1.5f, 0.05f, "%.2fx", ext(0.05f, 3.f),
    "And when a thrown thing, the shotguns, nails or an enforcer's laser gib it.")
QVR_SLIDER(vr_gib_speed_heavy, "Gib Speed: Explosives", 0.05f, 1.5f, 0.05f, "%.2fx", ext(0.05f, 3.f),
    "And everything else: explosions, rockets, grenades, lightning, plasma, monsters' blows (1: Quake's).")
QVR_TOGGLE(vr_gore_streams, "Blood Streams",
    "Shots and nails open bullet holes that pour a thin stream of blood, pulsing and weakening over a few seconds, "
    "following the body as it moves and falls, painting a pool where it lands (vr_gore_streams).")
QVR_SLIDER(vr_gore_streams_chance, "Stream Chance", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The chance each shot or pellet opens one (vr_gore_streams_chance).")
QVR_SLIDER(vr_gore_streams_melee_blade, "Melee Blade Chance", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The chance each blow of a blade in your hand (the axe, the sword, the chainsaw) opens one, on a monster "
    "or a corpse (vr_gore_streams_melee_blade).")
QVR_SLIDER(vr_gore_streams_melee_blunt, "Melee Blunt Chance", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The chance each blunt blow of yours (the crowbar, a punch, a pommel, a gun, a thing in the fist) opens "
    "one (vr_gore_streams_melee_blunt).")
QVR_SLIDER(vr_gore_streams_thrown_blade, "Thrown Blade Chance", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The chance an axe or a sword you threw opens one where it strikes (vr_gore_streams_thrown_blade).")
QVR_SLIDER(vr_gore_streams_per_body, "Streams per Body", 1.f, 8.f, 1.f, "%.0f", noExt,
    "Most on one body: a new one takes the weakest's place (vr_gore_streams_per_body).")
QVR_SLIDER(vr_gore_streams_max, "Streams in All", 0.f, 64.f, 2.f, "%.0f", noExt, "Most at once everywhere (vr_gore_streams_max).")
QVR_SLIDER(vr_gore_streams_time, "Stream Duration", 1.f, 20.f, 0.5f, "%.1f s", ext(1.f, 30.f),
    "About how long one pours, weakening all along (vr_gore_streams_time).")
QVR_SLIDER(vr_gore_streams_speed, "Stream Force", 0.f, 200.f, 5.f, "%.0f u/s", ext(0.f, 400.f),
    "How fast it leaves the wound at its strongest: how far it arcs out (vr_gore_streams_speed).")
QVR_SLIDER(vr_gore_streams_width, "Stream Width", 0.1f, 2.f, 0.05f, "%.2f units", ext(0.05f, 4.f),
    "How thick, at its strongest (1 unit: 3.8 cm) (vr_gore_streams_width).")
QVR_SLIDER(vr_gore_streams_pool, "Stream Pools", 0.f, 3.f, 0.1f, "%.1fx", ext(0.f, 4.f),
    "The pools a stream spreads where it lands: their size (0: no marks) (vr_gore_streams_pool).")
QVR_SLIDER(vr_gore_streams_spread, "Stream Spread", 0.f, 90.f, 5.f, "%.0f deg", noExt,
    "Each stream's way out turned at random up to this far from the wound's, so one shotgun blast's "
    "streams go their own ways (0: all alike) (vr_gore_streams_spread).")
QVR_SLIDER(vr_gore_streams_speed_var, "Stream Force Variation", 0.f, 0.9f, 0.05f, "+-%.2f", noExt,
    "Each stream's force times a random 1 plus or minus this (vr_gore_streams_speed_var).")
QVR_SLIDER(vr_gore_streams_wobble, "Stream Wobble", 0.f, 45.f, 1.f, "%.0f deg", noExt,
    "How far each stream's way out wanders as it pours: slow noise (0: steady) (vr_gore_streams_wobble).")
QVR_SLIDER(vr_gore_streams_wobble_rate, "Stream Wobble Rate", 0.f, 3.f, 0.05f, "%.2f Hz", ext(0.f, 10.f),
    "How fast it wanders: about this many swings a second (vr_gore_streams_wobble_rate).")
QVR_SLIDER(vr_gore_streams_thickness_var, "Stream Thickness Variation", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much its thickness swells and thins as it pours, and so along it as it falls: an uneven flow, not a "
    "steady trickle (0: even; 1: from a thread to twice as thick) (vr_gore_streams_thickness_var).")
QVR_SLIDER(vr_gore_streams_thickness_rate, "Stream Thickness Rate", 0.f, 4.f, 0.1f, "%.1f Hz", ext(0.f, 10.f),
    "How fast it swells and thins: about this many swells a second, unevenly (vr_gore_streams_thickness_rate).")
QVR_SLIDER(vr_gore_streams_smooth, "Stream Smoothing", 0.f, 1.f, 0.05f, "%.2f s", ext(0.f, 2.f),
    "The body's motion at the wound, and the stream's way out, smoothed over about this long, so the "
    "monster's animation and turns don't fling it about in ribbons (0: none) (vr_gore_streams_smooth).")
QVR_SLIDER(vr_gore_streams_inherit, "Stream Body Motion", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 1.5f),
    "How much of the body's (smoothed) motion the stream and its spurts take: 1 as a real one, less to keep "
    "it steady as the monster moves (vr_gore_streams_inherit).")
QVR_SLIDER(vr_gore_streams_break, "Stream Break Stretch", 0.f, 10.f, 0.5f, "%.1fx", ext(0.f, 100.f),
    "A piece of the stream stretched this many times its length as poured breaks off, fading from half that; "
    "and it is cut where the wound jumps (0: never) (vr_gore_streams_break).")
QVR_TOGGLE(vr_gore_streams_spurts, "Stream Spurts",
    "Small drops spurting from the wound in a cone as it pours, most fading fast, a few flying further "
    "(vr_gore_streams_spurts).")
QVR_SLIDER(vr_gore_streams_spurt_rate, "Spurt Rate", 0.f, 150.f, 5.f, "%.0f /s", ext(0.f, 1000.f),
    "Drops a second at its strongest; fewer as it weakens (vr_gore_streams_spurt_rate).")
QVR_SLIDER(vr_gore_streams_spurt_cone, "Spurt Cone", 0.f, 90.f, 5.f, "%.0f deg", noExt,
    "How far round its way out they spurt (vr_gore_streams_spurt_cone).")
QVR_SLIDER(vr_gore_streams_spurt_speed, "Spurt Speed", 0.f, 2.f, 0.05f, "%.2fx", ext(0.f, 4.f),
    "Their speed, times the stream's (vr_gore_streams_spurt_speed).")
QVR_SLIDER(vr_gore_streams_spurt_life, "Spurt Lifetime", 0.05f, 1.f, 0.05f, "%.2f s", ext(0.02f, 5.f),
    "About how long most live, fading all along (vr_gore_streams_spurt_life).")
QVR_SLIDER(vr_gore_streams_spurt_outliers, "Spurt Outliers", 0.f, 1.f, 0.02f, "%.2f", noExt,
    "The share of them that fly further and live longer (vr_gore_streams_spurt_outliers).")
QVR_SLIDER(vr_gore_streams_spurt_outlier_life, "Spurt Outlier Lifetime", 0.1f, 3.f, 0.05f, "%.2f s", ext(0.02f, 10.f),
    "About how long those live (vr_gore_streams_spurt_outlier_life).")
QVR_SLIDER(vr_gore_streams_spurt_size, "Spurt Size", 0.05f, 1.f, 0.01f, "%.2f units", ext(0.02f, 3.f),
    "About how big the drops are (1 unit: 3.8 cm) (vr_gore_streams_spurt_size).")
QVR_SLIDER(vr_gore_streams_splash, "Stream Splash", 0.f, 3.f, 0.1f, "%.1fx", ext(0.f, 10.f),
    "Drops splashing away where it lands: on the floor, a wall, a prop or a monster: how many (0 none) "
    "(vr_gore_streams_splash).")
QVR_SLIDER(vr_gore_streams_splash_size, "Splash Size", 0.f, 3.f, 0.1f, "%.1fx", ext(0.f, 4.f),
    "Their size, times the spurts' (vr_gore_streams_splash_size).")
QVR_SLIDER(vr_gore_backsplatter, "Back-splatter", 0.f, 3.f, 0.25f, "%.2fx", ext(0.f, 4.f),
    "Thin long streaks thrown along the shot's line onto what is behind a monster hit: how many (0 none) "
    "(vr_gore_backsplatter).")
QVR_SLIDER(vr_gore_backsplatter_reach, "Back-splatter Reach", 0.25f, 3.f, 0.25f, "%.2fx", ext(0.f, 4.f),
    "How far they fly, times the hit's own reach: a shotgun's close, big hits across the room "
    "(vr_gore_backsplatter_reach).")
QVR_TOGGLE(vr_teeth, "Knocked-out Teeth",
    "A hard punch to a humanoid's face (grunts, enforcers, knights, zombies, AD's humanoids) knocks teeth out: "
    "tiny gibs that bounce, can be picked up, and fade (vr_teeth).")
QVR_SLIDER(vr_teeth_per_hit, "Teeth per Punch", 1.f, 10.f, 1.f, "%.0f", noExt,
    "Most from one blow, the hardest (fewer for softer ones) (vr_teeth_per_hit).")
QVR_SLIDER(vr_teeth_per_enemy, "Teeth per Enemy", 1.f, 32.f, 1.f, "%.0f", noExt, "Most one enemy loses (vr_teeth_per_enemy).")
QVR_SLIDER(vr_teeth_hardness, "Punch Hardness for Teeth", 0.f, 0.95f, 0.05f, "%.2f", noExt,
    "How hard a blow must be to knock any out (0..1: the hand's speed and the damage, as Decapitation's "
    "blunt pops) (vr_teeth_hardness).")
QVR_TOGGLE(vr_teeth_blunt, "Teeth from Blunt Weapons",
    "A gun's butt, a crowbar, a club or a pommel knocks them out too, not only fists (vr_teeth_blunt).")
QVR_SLIDER(vr_teeth_size, "Tooth Size", 1.f, 8.f, 0.5f, "%.1f cm", noExt, "How long a tooth is (vr_teeth_size).")
QVR_TOGGLE(vr_wounds, "Dynamic Wounds",
    "Blood painted on monsters, corpses and you where the hits land, in the skins' own pixels. Your body and hands show your wounds this way instead of the wound skins, and healing washes them off.")
QVR_TOGGLE(vr_wounds_burns, "Burns",
    "Explosions, fire, lightning, lava and slime char what they hit; fresh burns glow in their cracks for a moment.")
QVR_TOGGLE(vr_wounds_wet, "Wet from Liquids",
    "Monsters and you get wet up to where water or slime came, drip, and dry (Wet Duration).")
QVR_SLIDER(vr_wounds_wet_coat, "Wet Sheen", 0.f, 2.f, 0.1f, "%.1fx", noExt,
    "How strongly wet skin, gloves and weapons look: a little darker and glossy, all over where the liquid came, fading as they dry (0: no wet look, the drips stay).")
QVR_SLIDER(vr_wounds_wet_time, "Wet Duration", 5.f, 120.f, 1.f, "%.0f s", ext(2.f, 600.f),
    "How long it takes to dry after leaving the liquid: the wet look stays full for the first quarter of it, then fades out, and the drips stop.")
QVR_CYCLE(vr_wounds_pool, "Models Kept",
    "How many models keep their wounds at once: past it, the ones seen longest ago give theirs up.",
    {32.f, "32 (8 MB)"}, {64.f, "64 (16 MB)"}, {128.f, "128 (32 MB)"})
QVR_CYCLE(vr_wounds_own_res, "Your Wounds' Detail",
    "Your own body's and hands' wounds, burns and wetness: finer than their skin and smooth-edged, with a relief "
    "(Chunky: in the skin's own pixels, as the monsters').",
    {0.f, "Chunky (as monsters)"}, {512.f, "Fine (5 MB)"}, {1024.f, "Finer (20 MB)"}, {2048.f, "Finest (80 MB)"})
QVR_CYCLE(vr_wounds_body_res, "Body Wounds' Detail",
    "A minimum for your body's blood, burns and wetness. The long arm texture needs more rows than the hands: "
    "1024 gives arms about the same blood detail per metre as Chunky hands. Shares the finer mask array "
    "(1024: 20 MB, 2048: 80 MB); the hands keep Your Wounds' Detail. As Hands uses that setting alone.",
    {0.f, "As Hands"}, {512.f, "Fine"}, {1024.f, "Match Hands"}, {2048.f, "Finest"})
QVR_SLIDER(vr_wounds_bump_burns, "Your Burns' Relief", 0.f, 3.f, 0.25f, "%.2fx", noExt,
    "How crusted and cracked the char on your arms looks in the light (Finer detail only; 0 flat).")
QVR_SLIDER(vr_wounds_bump_blood, "Your Wounds' Depth", 0.f, 3.f, 0.25f, "%.2fx", noExt,
    "How deep your bleeding wounds look sunk into the skin (Finer detail only; 0 flat).")
QVR_SLIDER(vr_wounds_blood_alpha, "Blood Opacity", 0.2f, 1.f, 0.05f, "%.2f", noExt,
    "How opaque the painted blood is over the skins: a little of the skin shows through below 1.")
QVR_TOGGLE(vr_gore_dents, "Flesh Dents",
    "Bullets, pellets, nails, blades and blasts push monsters' and corpses' surfaces in where they strike: a crater "
    "of raw flesh with a bloody rim, kept through every animation and on ragdolls (with Dynamic Wounds or Burns on).")
QVR_SLIDER(vr_gore_dents_depth, "Dent Depth", 0.f, 3.f, 0.25f, "%.2fx", noExt,
    "How deep the dents push in (0: flat, the raw flesh only). Never through a thin part: an arm, a tail.")
QVR_SLIDER(vr_gore_dents_size, "Dent Size", 0.25f, 3.f, 0.25f, "%.2fx", noExt, "How wide the dents are (and as much deeper).")
QVR_SLIDER(vr_gore_dents_max, "Dents per Body", 1.f, 16.f, 1.f, "%.0f", noExt,
    "How many dents a body keeps: past it, the oldest merges into the one nearest it.")
QVR_TOGGLE(vr_shock_death, "Lightning Shock",
    "A monster the lightning strikes, alive or dead, keeps Quad Damage's arcs crawling over it a while, on through its "
    "death; its ragdoll convulses as long as they do and the killing bolt chars it (off: only each hit's own arcs and burn).")
QVR_TOGGLE(vr_shock_living, "Arcs on the Living",
    "The lasting arcs on living monsters too, from the first bolt and on through their death. Off: only on the bodies "
    "the lightning kills or strikes dead; a living monster gets just each hit's own short flicker of arcs.")
QVR_SLIDER(vr_shock_death_time, "Lightning Shock Duration", 0.5f, 30.f, 0.5f, "%.1f s", ext(0.1f, 60.f),
    "How long the arcs (and a body's convulsions) last after the last bolt.")
QVR_SLIDER(vr_shock_living_time, "On the Living", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "A living monster's arcs last this much of the Duration after the last bolt (0.5: half). Its death carries "
    "them on to the whole Duration from that bolt, as a body's (vr_shock_living_time).")
QVR_SLIDER(vr_shock_seizure, "Convulsions", 0.f, 3.f, 0.1f, "%.1fx", noExt,
    "How hard a shocked ragdoll's limbs convulse, easing off with the arcs (0 still). Only the dead convulse.")
QVR_SLIDER(vr_shock_sound, "Crackle Volume", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "The electric crackle a body makes as its lasting arcs start, and again while a bolt stays on it (0 none).")
QVR_SLIDER(vr_shock_arcs, "Arcs on Bodies", 0.f, 3.f, 0.1f, "%.1fx", noExt,
    "How many arcs crawl over a shocked monster or body (0 none).")
QVR_SLIDER(vr_shock_burns, "Burn Marks", 0.f, 16.f, 1.f, "%.0f", noExt,
    "The burn marks a lightning kill leaves over the body (each hit also chars where it strikes: Burns).")
QVR_SLIDER(vr_smoulder_time, "Smoke After Lightning", 0.f, 15.f, 0.5f, "%.1f s", ext(0.f, 60.f),
    "How long a monster or a body the lightning strikes smokes from its burns after the last bolt, thinning out (0 none).")
QVR_SLIDER(vr_smoulder, "Smouldering Smoke", 0.f, 3.f, 0.1f, "%.1fx", ext(0.f, 10.f),
    "How much smoke rises off bodies the lightning struck or fire burnt (0 none; how long after fire: Combat > Burning > Smoke After Flames).")
QVR_SLIDER(vr_smoulder_alpha, "Smoke Opacity", 0.1f, 1.f, 0.05f, "%.2f", noExt,
    "How opaque each wisp of that smoke is as it leaves the skin.")
QVR_SLIDER(vr_shock_self_time, "Arcs on You", 0.f, 3.f, 0.1f, "%.1f s", ext(0.f, 5.f),
    "Struck by lightning (a shambler's bolt, another's lightning gun, a trap, the water's shock), Quad's arcs crawl over "
    "your hands, arms and body this long after a shambler's bolt, longer for harder ones (0 none). None right in front "
    "of your eyes.")
QVR_SLIDER(vr_shock_self_arcs, "Arcs on You: Number", 0.f, 3.f, 0.1f, "%.1fx", noExt, "How many arcs crawl over you then.")
QVR_SLIDER(vr_shock_self_light, "Arcs on You: Light", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "A soft flicker of blue light round you while they crackle (0 none). There is no flash over the view.")
QVR_SLIDER(vr_body_blood, "Arm Drip Rate", 0.f, 4.f, 0.25f, "%.2fx", ext(),
    "How often blood drips from your wounded arms and hands (the body's wounds: Show Armour and Wounds; 0 none).")
QVR_SLIDER(vr_body_blood_amount, "Drop Size", 0.5f, 3.f, 0.25f, "%.2fx", ext(), "How big the drops are and how much they splash.")
QVR_SLIDER(vr_body_blood_floor, "Drips Round Feet", 0.f, 4.f, 0.25f, "%.2fx", ext(),
    "While wounded, blood drips from your body round your feet, faster when badly hurt or just hit (0 none).")
QVR_SLIDER(vr_body_blood_marks, "Drops Mark Floor", 0.f, 1.f, 0.05f, "%.2f", noExt, "The chance a drop leaves a mark on the floor.")
QVR_SLIDER(vr_body_blood_mark_size, "Floor Mark Size", 0.5f, 4.f, 0.25f, "%.2fx", ext(), nullptr)
QVR_SLIDER(vr_gore_hands, "Gib Blood on Hands", 0.f, 3.f, 0.25f, "%.2fx", ext(),
    "Taking a gib or a head bloodies the hand holding it, more the longer you hold it (0 none).")
QVR_TOGGLE(vr_gore_wash, "Water Washes Blood",
    "Water washes the blood off your body and hands, where they are under it.")
QVR_SLIDER(vr_gore_wash_time, "Wash Time", 0.25f, 10.f, 0.25f, "%.2f s", ext(0.05f, 60.f), "How long under water washes the blood off fully.")
QVR_TOGGLE(vr_gore_reopen, "Wounds Re-open",
    "Hurt, your wounds re-open after a wash and bleed onto your arms and hands again (healed ones don't, nor a gib's blood).")
QVR_SLIDER(vr_gore_reopen_delay, "Re-open Delay", 0.f, 30.f, 0.5f, "%.1f s", ext(0.f, 120.f),
    "How long you stay clean out of the water before they re-open.")
QVR_SLIDER(vr_gore_reopen_time, "Re-open Spread", 0.f, 10.f, 0.25f, "%.2f s", ext(),
    "Over how long they re-open, one after another (0: all at once).")
QVR_SLIDER(vr_gore_reopen_fade, "Re-open Bleed", 0.f, 30.f, 0.5f, "%.1f s", ext(0.f, 120.f),
    "How long each re-opened wound takes to bleed back to its full blood, from a trace; the blood runs onto your arms and hands along the way (0: at once).")
QVR_SLIDER(vr_gore_spatter, "Blood Spatter", 0.f, 3.f, 0.25f, "%.2fx", ext(),
    "Blood thrown from hits near you onto your hands, arms, body and what you hold, as drops: how much (0 none).")
QVR_SLIDER(vr_gore_spatter_melee, "From Your Blows", 0.f, 3.f, 0.25f, "%.2fx", ext(),
    "A punch, a blade, a prop or a gun swung into a monster bloodies what struck and the hand a little.")
QVR_SLIDER(vr_gore_spatter_saw, "From the Chainsaw", 0.f, 6.f, 0.25f, "%.2fx", ext(),
    "A chainsaw cutting a monster sprays your hands and arms: a lot.")
QVR_SLIDER(vr_gore_spatter_shots, "From Close Shots", 0.f, 3.f, 0.25f, "%.2fx", ext(),
    "A shot hitting close to you throws a few drops onto your weapon, hands and arms.")
QVR_SLIDER(vr_gore_spatter_range, "Close Shot Range", 16.f, 160.f, 8.f, "%.0f u", ext(0.f, 512.f),
    "How close to your hands or eyes a shot must hit for its blood to reach you.")
QVR_SLIDER(vr_gore_spatter_gibs, "Gibs Striking You", 0.f, 3.f, 0.25f, "%.2fx", ext(),
    "Gibs flying into you bloody you where they strike: how much (0 none).")
QVR_TOGGLE(vr_gore_gear, "Blood on Weapons and Props",
    "What you hold takes blood and keeps it: dropped, thrown and taken again, holstered and drawn, until water washes it.")
QVR_TOGGLE(vr_gore_clean_skins, "Clean Weapon Skins",
    "Weapons with blood painted into their skins (the axe, the knights' swords, the chainsaw, the grunt's shotgun) start clean: their blood is all yours, and water washes them clean.")
QVR_TOGGLE(vr_gore_gear_holstered, "Holstered Weapons Too",
    "Your holstered weapons take blood as your body and hands do, when it lands near enough to them (a holstered weapon keeps its blood either way).")
QVR_SLIDER(vr_gore_gear_nearby, "Things Lying Near", 0.f, 3.f, 0.25f, "%.2fx", ext(),
    "Weapons, boxes and props lying about take blood from bleeding hits, gibbings and bursting gibs near them: how much (0 none).")
QVR_SLIDER(vr_gore_gear_drops, "Gibbed Monsters' Drops", 0.f, 3.f, 0.25f, "%.2fx", ext(),
    "What lies right where a monster is gibbed (its gun, its backpack) comes out soaked: how much (0 only the drops near it).")
QVR_SLIDER(vr_gore_spread, "Blood over Your Arms", 0.f, 3.f, 0.25f, "%.2fx", ext(),
    "Hurt, holding a gib, or as your wounds re-open, blood runs over your arms too (a little your legs): how much (0 none).")
QVR_SLIDER(vr_gore_mist, "Mist Amount", 0.f, 3.f, 0.25f, "%.2fx", ext(0.f, 10.f),
    "Large, faint clouds of blood hanging in the air wherever something bleeds: how many (0 none).")
QVR_SLIDER(vr_gore_mist_size, "Mist Size", 0.25f, 3.f, 0.05f, "%.2fx", ext(0.05f, 6.f), "How big the clouds are.")
QVR_SLIDER(vr_gore_mist_alpha, "Mist Opacity", 0.f, 0.3f, 0.01f, "%.2f", ext(0.f, 1.f), "How opaque they start (very transparent: 0.06).")
QVR_SLIDER(vr_gore_mist_life, "Mist Lifetime", 0.25f, 8.f, 0.25f, "%.2f s", ext(0.2f, 30.f), "How long they take to fade.")
QVR_SLIDER(vr_gore_mist_grow, "Mist Spreading", 0.f, 3.f, 0.1f, "%.1fx", ext(), "How fast they spread as they fade.")
QVR_SLIDER(vr_gore_mist_speed, "Mist Drift", 0.f, 3.f, 0.1f, "%.1fx", ext(), "How fast they drift along the blow and apart.")
QVR_SLIDER(vr_gore_mist_rise, "Mist Rise", -20.f, 20.f, 1.f, "%.0f u/s", ext(-100.f, 100.f), "How fast they rise (negative: sink).")
QVR_SLIDER(vr_gore_mist_dark, "Mist Darkness", 0.f, 1.f, 0.05f, "%.2f", noExt, "How dark their red is (0 bright, 1 nearly black).")
QVR_SLIDER(vr_gore_mist_near, "Mist Clear of Eyes", 0.f, 128.f, 4.f, "%.0f u", ext(0.f, 512.f),
    "No mist closer to your eyes than this: your own bleeding is not in your face.")
QVR_SLIDER(vr_water_blood, "Blood in Water", 0.f, 2.f, 0.1f, "%.1fx", noExt,
    "Blood spreads on water and slime as dark red clouds where something bleeds into it, a hit, a gib, a corpse, a wounded body or you wading (0 none). Under water, sprays and streams of blood slow into slowly sinking clouds.")
QVR_SLIDER(vr_water_blood_time, "Blood in Water Lifetime", 2.f, 120.f, 2.f, "%.0f s", noExt, "How long a cloud of blood on the water takes to fade.")
QVR_TOGGLE(vr_corpse_dying, "Hit While Dying",
    "A monster dying takes damage as a corpse while it falls (once it drops what it drops): a chainsaw, a blow or a shot can gib it. Never killed twice. Needs Gib Corpses (Carrying and Throwing > Gibs and Corpses).")
QVR_TOGGLE(vr_dummy_gore, "Dummy Bleeds",
    "The firing range's training dummy bleeds as its enemy: blood sprays and mist, wounds on its model, small gibs, blood on you and what you hold. Off: it stays clean.")
QVR_TOGGLE(vr_dummy_dents, "Dummy Dents",
    "A clean training dummy (Dummy Bleeds off) is dented where it is hit all the same, with nothing else painted: for trying the dents. One that bleeds is dented as its enemy anyway (vr_dummy_dents).")
QVR_TOGGLE(vr_dummy_streams, "Dummy Blood Streams",
    "A clean training dummy pours blood streams from its bullet holes all the same (at Stream Chance): for trying the streams. One that bleeds pours them as its enemy anyway (vr_dummy_streams).")
QVR_TOGGLE(vr_dummy_gib, "Dummy Dies",
    "The hit that takes the last of its health (Dummy Health) kills the training dummy as its enemy: beheaded, its head popped, dying, lying as a ragdoll or gibbed exactly as that monster would be. Then it stands again. No loot. Off: it stays at 0 health (\"would kill\"). On as shipped.")
QVR_SLIDER(vr_dummy_gib_respawn, "Dummy Stands Again", 0.5f, 10.f, 0.5f, "%.1f s", ext(0.1f, 60.f),
    "How long a killed training dummy takes to stand again (its body stays, as its enemy's).")
QVR_TOGGLE(vr_dummy_anchored, "Dummy Stays Put",
    "The training dummy is never shoved away, staggered or knocked down; it still flinches and takes the hit. For recording shoves and bashes (vr_dummy_anchored).")
QVR_SLIDER(vr_dummy_regen, "Dummy Health Refills", 0.f, 15.f, 0.5f, "%.1f s", ext(0.f, 120.f),
    "How long after its last hit the training dummy's health fills up again (0: never; it stays as hit until it dies and stands again).")
QVR_TOGGLE(vr_dummy_healthbar, "Dummy Health Bar",
    "The training dummy's health over its head, as a bar and a number, turned to face you. Off: its sign only.")
QVR_TOGGLE(vr_decals, "Decals",
    "Blood, scorch marks and bullet chips on walls and floors (the gore needs them).")
QVR_TOGGLE(vr_permdecals, "Permanent Decals",
    "Marks stay for good, baked into the walls, floors, doors and lifts they land on: as many as you like at no extra cost a frame. Off: only the newest Max Decals show, each for its Decal Lifetime, as without it; the others are kept and come back when it is on again.")
QVR_TOGGLE(vr_decal_entities, "Marks on Moving Brushes",
    "Shots, blows and blood mark the brush things with models of their own (the firing range's targets: its spawn buttons and panels), and doors and lifts with Permanent Decals off: moving with them, the newest 128, flat (vr_decal_entities).")
QVR_SLIDER(vr_decal_max, "Max Decals", 64.f, 4096.f, 64.f, "%.0f", ext(),
    "The oldest go first. The gore makes many; more cost GPU time where they pile up (512 shipped).")
QVR_SLIDER(vr_decal_bucket_max, "Decals Piled in One Spot", 8.f, 255.f, 8.f, "%.0f", noExt,
    "How many marks one spot of the floor or wall keeps where they pile up (a corpse cut apart in one place). Past it the oldest there go whole, never leaving holes in the others. Each pixel there walks all of them: more cost GPU time when you look at the pile (128 shipped, 255 the most).")
QVR_SLIDER(vr_decal_life, "Decal Lifetime", 10.f, 600.f, 10.f, "%.0f s", ext(), nullptr)
QVR_TOGGLE(vr_decal_carve, "Carved Holes and Craters",
    "Bullet, nail and blast marks carved into walls and floors: a pit, hole or crater with a raised rim and a dark middle, its texture seen deeper in and lit by the world's light. Pellets leave small pits, nails narrow deeper holes (the super nailgun's and lava nails' larger), rockets and grenades wide shallow craters, the lightning gun a scorch only. Look only: nothing to bump into.")
QVR_SLIDER(vr_decal_carve_depth, "Carved Depth", 0.25f, 4.f, 0.25f, "%.2fx", ext(0.f, 4.f), "How deep the holes and craters look (1 shipped: shallow).")
QVR_SLIDER(vr_decal_carve_size, "Carved Size", 0.5f, 2.f, 0.1f, "%.1fx", noExt,
    "How wide the holes and craters made from now on are (1 shipped).")
QVR_SLIDER(vr_decal_carve_rim, "Carved Rim", 0.f, 3.f, 0.25f, "%.2fx", ext(0.f, 4.f),
    "How high the broken rim round each hole and crater stands (0 none).")
QVR_SLIDER(vr_decal_carve_steps, "Carving Quality", 4.f, 64.f, 4.f, "%.0f steps", noExt,
    "The most steps a pixel's look into a hole takes (fewer where it is small on the screen): more are smoother at grazing angles and cost more GPU time where many are in view (16 shipped).")
QVR_SLIDER(vr_decal_carve_fade, "Carving Distance", 128.f, 4096.f, 128.f, "%.0f", noExt,
    "How far off holes and craters stop looking carved (they flatten from half as far; 1024 shipped).")
QVR_SLIDER(vr_decal_carve_max, "Carved Marks Kept", 32.f, 1024.f, 32.f, "%.0f", noExt,
    "How many of the newest holes and craters stay carved; with Permanent Decals the older ones stay, flat (256 shipped).")
QVR_CYCLE(vr_decal_carve_style, "Carve Style",
    "How holes, craters, dents and grooves are carved. Smooth: round and smooth. Retro: pixelated like the walls, block by block on the wall's own texels, their depth in steps. Faceted: low-poly, flat sides (8 round, 3 rings out). Auto, as shipped: Retro on walls with Retro Textures' blocks, else Smooth.",
    {-1.f, "Auto"}, {0.f, "Smooth"}, {1.f, "Retro"}, {2.f, "Faceted"})
QVR_TOGGLE(vr_decal_melee, "Melee Marks",
    "Melee blows on walls and floors carve their own marks (needs Carved Holes and Craters): blunt ones (the crowbar, a gun's butt or barrel, a pommel, Mjolnir, a wall torch, a fist holding a prop, a prop thrown hard) leave shallow round dents, blades (a sword, an axe, the chainsaw's bar) grooves along the swing, deepest in the middle; harder blows bigger and deeper. A bare fist leaves none. Off: the old shallow pits.")
QVR_SLIDER(vr_decal_melee_blunt_depth, "Blunt Dent Depth", 0.25f, 4.f, 0.25f, "%.2fx", ext(0.f, 4.f),
    "How deep blunt blows' dents look (1 shipped: shallower than a pellet's pit; 0 a scuff only).")
QVR_SLIDER(vr_decal_melee_blunt_size, "Blunt Dent Size", 0.5f, 2.f, 0.1f, "%.1fx", noExt, "How wide the dents made from now on are (1 shipped).")
QVR_SLIDER(vr_decal_melee_slash_depth, "Slash Depth", 0.25f, 4.f, 0.25f, "%.2fx", ext(0.f, 4.f), "How deep blades' grooves look (1 shipped; 0 a scuff only).")
QVR_SLIDER(vr_decal_melee_slash_length, "Slash Length", 0.5f, 2.f, 0.1f, "%.1fx", noExt, "How long the grooves made from now on are (1 shipped).")
QVR_SLIDER(vr_decal_melee_slash_width, "Slash Width", 0.5f, 2.f, 0.1f, "%.1fx", noExt, "How wide the grooves made from now on are (1 shipped).")
QVR_TOGGLE(vr_gib_blood, "Gib Blood",
    "Gibs and heads leave a trail of blood drops and splat where they hit walls and floors. Off: Quake's trail.")
QVR_SLIDER(vr_gib_blood_trail, "Gib Blood Trail", 0.f, 3.f, 0.25f, "%.2fx", ext(), "How dense their trail of blood and drops is (0 none).")

// vr_menu_mgroups.inc: pageMonsterParam
QVR_SLIDER(vr_kdtrig_resist_size, "By Size", 0.f, 1.5f, 0.05f, "%.2f", noExt,
    "A group whose resistance is 0: its body's volume over a grunt's to this power (an "
    "ogre's, 5.5 times: 2.3 at 0.5). 0: all alike.")

// vr_menu_mgroups.inc: pageMonsterHealth
QVR_TOGGLE(vr_monster_health, "Cut Big Monsters' Health",
    "Monsters with a big health pool (ogres, shamblers, giants, most bosses) appear with less health, by "
    "the curve below: a reload by hand costs time. Small ones (grunts, knights, enforcers, zombies) keep "
    "theirs. Monsters already about keep theirs: the next map, or a monster appearing, shows a change. "
    "Not the scripted bosses (Chthon, Shub, Arcane Dimensions' wave bosses).")

// vr_menu_pages.inc: pageMenuSettings
QVR_SLIDER_BYNAME(vr_menu_scale, "Menu Scale", 0.05f, 0.6f, 0.01f, "%.2f", ext(0.02f, 1.5f),
    "Scale multiplier for the menu: the size of one of its pixels, in world units.")
QVR_SLIDER_BYNAME(vr_menu_distance, "Menu Distance", 24.f, 256.f, 1.f, "%.0f", ext(8.f, 600.f), "Distance of the menu from the anchor point.")
QVR_CYCLE_BYNAME(vr_menu_grab, "Move the Panel",
    "Point at the menu panel's frame (it lights up just outside the panel) and hold a grip: the panel follows your hand, moved and turned, until you let go; it stays there until the menus close. Frame or Face: a grip held on the panel itself takes hold too once your hand moves a few cm (a grip held still, or with a stick pushed, stays the sliders' fine step).",
    {0.f, "Never"}, {1.f, "Grip on Its Frame"}, {2.f, "Grip on Frame or Face"})
QVR_SLIDER_BYNAME(vr_menu_resolution, "Menu Resolution", 0.f, 3.f, 0.25f, "%.2fx", noExt,
    "The menus (and the text in front of you in game) are drawn at this many of their pixels to each of the headset's, then shrunk smoothly: sharp letters with even strokes at any distance and scale. 1.5 as shipped; 0: at the desktop window's size, as before (blurry when the headset's pixels are finer than the window's).")
QVR_SLIDER_BYNAME(vr_menu_sharpen, "Menu Sharpening", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "With Menu Resolution above 0: the menus shrunk less softly. Higher: crisper letters, the smallest ones shimmering a little as your head moves. 1 as shipped (0.5 before 2026-10-09).")
QVR_TOGGLE_BYNAME(vr_menu_status, "Status Box",
    "A small box in the menus' top right corner: VR, mock VR or flat screen, the runtime (SteamVR, VDXR), the resolution rendered (the runtime's times Eye Image Size times Render Scale; against the headset panel's, with a warning when far over it), the target rate, fps, the CPU's and GPU's milliseconds a frame (and their share of the frame's budget), RAM and VRAM.")
QVR_TOGGLE_BYNAME(vr_menu_banner_beside, "Banner Beside the Buttons",
    "With the VR Menu Style: the vertical Quake VR banner stands left of the corner's shortcut buttons, its top level with theirs, so the buttons, the banner and the menu don't crowd each other. Off: under the buttons, in their column, as before (also where the panel leaves too little room left of them).")
QVR_TOGGLE_BYNAME(vr_menu_version, "Version Label",
    "The game's name, version and author, small, in the menus' bottom right corner (where the page leaves room for it). A build not made for a release says \"dev\" after the version.")
QVR_TOGGLE_BYNAME(vr_update_check, "Check for Updates",
    "At start-up (at most once an hour) the game asks the release page whether a newer Quake VR is out, and says so above the version label: point at it and press to open its page in your desktop's browser. Only the release list is read; nothing about you or your game is sent. Off: never asked, no notice.")
QVR_TOGGLE_BYNAME(vr_menu_flat_shortcuts, "Corner Buttons on a Flat Screen",
    "With VR off: the buttons at the menus' top left (Back to Game, Main Menu, Search, Console, Advanced VR, Levels, Map Library) on the desktop menus too, for the mouse.")
QVR_TOGGLE_BYNAME(vr_menu_main_mods, "Mods on the Main Menu",
    "The main menu's Mods row (Ironwail's mods list). Off: only in Options > Mods.")
QVR_SLIDER_BYNAME(vr_menu_fine_step, "Fine Step", 0.01f, 0.5f, 0.01f, "%.2fx", noExt,
    "A slider's step while you hold a grip (Shift on a flat screen), over its own: 0.1, a tenth, for placing things precisely.")
QVR_SLIDER_BYNAME(vr_menu_section_gap, "Section Gap", 0.f, 2.f, 0.25f, "%.2f rows", noExt,
    "Space above each section's header on the settings pages (and under the page's title), so the sections stand apart. 0: none.")
QVR_TOGGLE_BYNAME(vr_menu_vr_style, "VR Menu Style",
    "The menus fill the panel with their rows spaced out, with modern sliders and switches, and a laser from your hand points at them: trigger to click and drag.")
QVR_TOGGLE_BYNAME(vr_menu_bigfont, "Main Menu Lettering",
    "The main menu's rows as text in Quake's own lettering (its letters cut from the menu pictures), VR Calibration among them. Off: Quake's picture of the rows, with VR Calibration above it in the console's letters.")
QVR_TOGGLE_BYNAME(vr_menu_recolor, "Blood Red Menus",
    "The menus' browns and tans (the letters, Quake's pictures, sliders, switches and panels) turned blood red, in Quake VR's colours. Off: Quake's own browns.")
QVR_SLIDER_BYNAME(vr_menu_recolor_strength, "Red Strength", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "With Blood Red Menus: how far the browns turn towards the hue (1 all the way, 0.5 halfway: a rusty red).")
QVR_SLIDER_BYNAME(vr_menu_recolor_saturation, "Red Saturation", 0.5f, 3.f, 0.1f, "%.1fx", noExt,
    "With Blood Red Menus: how colourful the red is, over Quake's browns (3 as shipped, the most; as far as the hue's colours go). Their lightness always stays: only the hue changes.")
QVR_SLIDER_BYNAME(vr_menu_spacing, "Row Spacing", 1.f, 2.f, 0.05f, "%.2fx", ext(),
    "With the VR menu style: how far apart the menus' rows are, over Quake's.")
QVR_SLIDER_BYNAME(vr_menu_height, "Menu Height", 1.f, 2.f, 0.05f, "%.2fx", ext(),
    "With the VR menu style: how tall the menus are, over Quake's: more rows at once. 1.35 reaches about 35 degrees up and down at the shipped distance, scale and spacing.")
QVR_SLIDER_BYNAME(scr_menubgalpha, "Menu Background Opacity", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How dark the panel behind the menus is (0.7 as shipped; the desktop menus' too): lower it to see the game while you tune the graphics. Below about 0.5 the text gets a dark outline, to stay readable.")
QVR_TOGGLE_BYNAME(ui_live_preview, "Live Preview",
    "Settings pages show their changes in the game as you make them, and in single player the game keeps running under them (paused while a monster is after you).")
QVR_TOGGLE_BYNAME(vr_menu_remember, "Reopen Where Left",
    "After Back to Game (the button at the menu's top left, or the menu button), the menu opens again on the page you left.")
QVR_SLIDER_BYNAME(vr_menu_help_wpm, "Help Reading Speed", 100.f, 400.f, 10.f, "%.0f wpm", ext(60.f, 600.f),
    "The help under a page's rows: one longer than its box shows a part at a time, the bar at its right saying which. Each part stays as long as reading it takes at this many words a minute (200: unhurried).")
QVR_CYCLE_BYNAME(vr_menu_dropdown, "Drop-Down Lists",
    "A setting with this many choices or more opens a list of them when clicked (or Enter / A): point at one and click. Left and right still step through them. Never: a click steps to the next.",
    {0.f, "Never"}, {3.f, "3+ Choices"}, {4.f, "4+ Choices"}, {5.f, "5+ Choices"}, {6.f, "6+ Choices"},
    {8.f, "8+ Choices"})

// vr_menu_pages.inc: pageCrosshairSettings
QVR_SLIDER_BYNAME(vr_crosshair_depth, "Crosshair Depth", 0.f, 4096.f, 16.f, "%.0f", ext(),
    "When zero, the crosshair will be projected to the closest wall. Otherwise, the crosshair will be projected at a fixed depth.")
QVR_SLIDER_BYNAME(vr_crosshair_size, "Crosshair Size", 0.f, 32.f, 0.5f, "%.1f", ext(), "Size of the crosshair.")
QVR_SLIDER_BYNAME(vr_crosshair_alpha, "Crosshair Alpha", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "Transparency of the crosshair. '1' means fully visible, '0' means fully transparent.")
QVR_SLIDER_BYNAME(vr_crosshairy, "Crosshair Z Offset", -10.f, 10.f, 0.05f, "%.2f", ext(-50.f, 50.f),
    "Z offset applied to the crosshair. Does not affect aiming. Leave to zero unless you changed other weapon options and your aim is off.")

// vr_menu_pages.inc: pageFireParticles
QVR_TOGGLE_BYNAME(vr_fire_particles, "Fire Model Particles",
    "Rising flame tongues from wall torches, map fires, candles, lanterns, burning bodies and taken torches. Keeps the low-poly flame models. Needs Quake VR Particles.")
QVR_SLIDER_BYNAME(vr_fire_particles_count, "Particles per Emission", 0.f, 16.f, 1.f, "%.0f", noExt,
    "Exact count per source per emission, independent of Particle Multiplier.")
QVR_SLIDER_BYNAME(vr_fire_particles_frequency, "Emission Frequency", 0.f, 30.f, 1.f, "%.0f /s", ext(0.f, 60.f), nullptr)
QVR_SLIDER_BYNAME(vr_fire_particles_origin, "Emission Height", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "0: base of the flame; 0.5: centre; 1: tip. Also applies to held torches.")
QVR_SLIDER_BYNAME(vr_fire_particles_speed_min, "Minimum Outward Speed", 0.f, 3.f, 0.05f, "%.2f m/s", ext(0.f, 10.f), nullptr)
QVR_SLIDER_BYNAME(vr_fire_particles_speed_max, "Maximum Outward Speed", 0.f, 3.f, 0.05f, "%.2f m/s", ext(0.f, 10.f),
    "Horizontal speed, away from the flame. Endpoints may be swapped.")
QVR_SLIDER_BYNAME(vr_fire_particles_up_min, "Minimum Upward Speed", 0.f, 3.f, 0.05f, "%.2f m/s", ext(0.f, 10.f), nullptr)
QVR_SLIDER_BYNAME(vr_fire_particles_up_max, "Maximum Upward Speed", 0.f, 3.f, 0.05f, "%.2f m/s", ext(0.f, 10.f), "Up in the world, even when a torch is inverted.")
QVR_SLIDER_BYNAME(vr_fire_particles_life_min, "Minimum Particle Life", 0.05f, 2.f, 0.05f, "%.2f s", ext(0.05f, 5.f), nullptr)
QVR_SLIDER_BYNAME(vr_fire_particles_life_max, "Maximum Particle Life", 0.05f, 2.f, 0.05f, "%.2f s", ext(0.05f, 5.f), nullptr)
QVR_SLIDER_BYNAME(vr_fire_particles_size, "Fire Particle Size", 0.1f, 4.f, 0.1f, "%.1fx", noExt, nullptr)
QVR_SLIDER_BYNAME(vr_fire_particles_alpha, "Fire Particle Opacity", 0.f, 1.f, 0.05f, "%.2f", noExt, nullptr)
QVR_SLIDER_BYNAME(vr_fire_particles_range, "Emission Distance", 0.f, 2048.f, 64.f, "%.0f units", ext(0.f, 4096.f),
    "Sources beyond this distance stop emitting; limits the cost on maps with many fires.")
QVR_TOGGLE_BYNAME(vr_flame_wobble, "Hands Stir Flames",
    "A hand passing quickly through or near a flame (torches, braziers, map fires, burning bodies, the pyro's pilot light) makes it lean the way the hand went, as if moving the air, swinging back a little as it settles; the fire and smoke particles near the hand are carried along.")
QVR_SLIDER_BYNAME(vr_flame_wobble_strength, "Stir Strength", 0.f, 4.f, 0.05f, "%.2fx", noExt,
    "How far the flames lean for a given hand speed and nearness (0 none).")
QVR_SLIDER_BYNAME(vr_flame_wobble_speed, "Stir Speed", 0.2f, 3.f, 0.1f, "%.1f m/s", ext(0.f, 6.f),
    "How fast a hand must move to stir a flame; slower hands do nothing.")

// vr_menu_pages.inc: explosionDebrisMultiplayerSlider
QVR_SLIDER_BYNAME(vr_explosion_debris_mp_max, "Most in Multiplayer", -1.f, 96.f, 1.f, "%.0f", ext(-1.f, 256.f),
    "The most at once in a multiplayer game (yours as the host). They are the server's: each one in sight "
    "costs every player's network packets (about 26 bytes a frame while it lives). Single Player's: as "
    "many as Maximum Active Chunks. 0: none.")

// vr_menu_pages.inc: pageExplosionDebris
QVR_TOGGLE_BYNAME(vr_explosion_debris, "Physical Explosion Debris",
    "Small incandescent chunks from every ordinary, coloured or tarbaby explosion: physics objects of the game (the host's) that bounce off the level, doors, lifts, monsters and props, ride lifts, are thrown by later blasts, pass through your hands, and never cause damage.")
QVR_SLIDER_BYNAME(vr_explosion_debris_count, "Chunks per Explosion", 0.f, 32.f, 1.f, "%.0f", ext(0.f, 64.f), nullptr)
QVR_SLIDER_BYNAME(vr_explosion_debris_speed_min, "Minimum Speed", 0.f, 20.f, 0.25f, "%.2f m/s", ext(0.f, 40.f), nullptr)
QVR_SLIDER_BYNAME(vr_explosion_debris_speed_max, "Maximum Speed", 0.f, 20.f, 0.25f, "%.2f m/s", ext(0.f, 40.f),
    "Speed chosen between Minimum and Maximum; swapped endpoints are accepted.")
QVR_SLIDER_BYNAME(vr_explosion_debris_up, "Upward Direction Bias", 0.f, 2.f, 0.05f, "%.2f", noExt,
    "0: burst equally in every direction. 1: launches into the upper hemisphere. 2: a narrower upward burst. Speed stays between your chosen limits.")
QVR_SLIDER_BYNAME(vr_explosion_debris_size_min, "Smallest Chunk", 0.8f, 8.f, 0.2f, "%.1f units", noExt, nullptr)
QVR_SLIDER_BYNAME(vr_explosion_debris_size_max, "Largest Chunk", 0.8f, 8.f, 0.2f, "%.1f units", noExt, nullptr)
QVR_SLIDER_BYNAME(vr_explosion_debris_life_min, "Minimum Lifetime", 0.1f, 10.f, 0.1f, "%.1f s", ext(0.1f, 30.f), nullptr)
QVR_SLIDER_BYNAME(vr_explosion_debris_life_max, "Maximum Lifetime", 0.1f, 10.f, 0.1f, "%.1f s", ext(0.1f, 30.f),
    "Lifetime chosen independently for each chunk; fades out at the end, even when resting.")
QVR_SLIDER_BYNAME(vr_explosion_debris_bounce, "Bounciness", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "Share of speed retained away from the surface. 0: no rebound, 1: full rebound. Surface friction also slows sliding.")
QVR_SLIDER_BYNAME(vr_explosion_debris_max, "Maximum Active Chunks", 0.f, 128.f, 1.f, "%.0f", ext(0.f, 256.f),
    "Oldest chunks go first when a blast needs room. Reducing this or disabling the effect removes the excess immediately.")
QVR_SLIDER_BYNAME(vr_explosion_debris_trail, "Fire and Smoke Trail", 0.f, 3.f, 0.1f, "%.1fx", noExt,
    "Trail density (0: off). Needs Quake VR Particles. Physical chunks and their lights work with particles off.")
QVR_SLIDER_BYNAME(vr_explosion_debris_lights, "Maximum Glowing Lights", 0.f, 16.f, 1.f, "%.0f", noExt,
    "Only the nearest chunks emit dynamic light, to keep many blasts inexpensive. Their model skins remain incandescent. These lights do not cast shadows.")
QVR_SLIDER_BYNAME(vr_explosion_debris_light_radius, "Glow Light Radius", 0.f, 120.f, 4.f, "%.0f units", ext(0.f, 200.f), nullptr)

// vr_menu_pages.inc: pageParticleSettings
QVR_SLIDER_BYNAME(vr_explosion_particles, "Large Fireballs per Explosion", 0.f, 16.f, 1.f, "%.0f", noExt,
    "Exact number of large textured fireballs per explosion, independent of Particle Multiplier. Each gets its own size, rotation, position and velocity; 0 keeps the sparks and smoke.")
QVR_TOGGLE_BYNAME(r_particles, "Particle Effects", "Enable or disable all particle effects.")
QVR_TOGGLE_BYNAME(vr_particles, "Quake VR Particles",
    "Textured smoke, sparks, blood and explosions for Quake VR's effects. Off: Quake's own dots.")
QVR_SLIDER_BYNAME(vr_particle_mult, "Particle Multiplier", 0.25f, 4.f, 0.25f, "%.2f", ext(), "How many particles Quake VR's effects make.")

// vr_menu_pages.inc: pageLocomotionSettings
QVR_CYCLE(vr_comfort_preset, "Comfort",
    "Sets Turning, Teleport and Default Speed together. Comfortable: snap turns of 45 degrees, teleport, walk. "
    "Moderate: snap turns of 30, teleport, run. Full Freedom: smooth turning, run, no teleport. Change them after "
    "to taste (Custom).",
    {0.f, "Custom"}, {1.f, "Comfortable"}, {2.f, "Moderate"}, {3.f, "Full Freedom"})
QVR_CYCLE(vr_movement_mode, "Move Towards",
    "Where the moving stick takes you: where you look (Head), or where a hand points. Moving stick's hand: the "
    "left one, or the right with Swap Stick Functions.",
    {1.f, "Head"}, {0.f, "Moving stick's hand"}, {2.f, "Left hand"}, {3.f, "Right hand"})
QVR_SLIDER(vr_deadzone, "Stick Deadzone", 0.f, 50.f, 5.f, "%.0f%%", noExt,
    "How far a stick must be pushed before it does anything. Raise it if you drift or turn without "
    "touching the stick: worn sticks often need 20-25%.")
QVR_SLIDER(vr_deadzone_outer, "Stick Outer Deadzone", 0.f, 30.f, 1.f, "%.0f%%", noExt,
    "The last part of the moving stick's travel that already counts as a full push, so you reach full "
    "speed without pressing the stick hard against its edge (vr_deadzone_outer).")
QVR_SLIDER(vr_move_curve, "Stick Response Curve", 1.f, 3.f, 0.1f, "%.1f", noExt,
    "How the moving stick's push becomes speed. 1: straight (half the push, half the speed). Higher: "
    "more of the travel for slow speeds, for careful walking, the top speed still at a full push. 1.6: "
    "about half speed at two thirds of a push (vr_move_curve).")
QVR_TOGGLE(vr_move_autorun, "Walk Into Run",
    "Walking (Default Speed: Walk), pushing the moving stick all the way eases you into a run; less than "
    "Run From Push walks. The speed button still runs (vr_move_autorun).")
QVR_SLIDER(vr_move_autorun_threshold, "Run From Push", 0.5f, 1.f, 0.05f, "%.2f", noExt,
    "Walk Into Run: how far the moving stick must be pushed (past its deadzones; 1 a full push) before "
    "you ease into running; under it you walk, at full walking speed there (vr_move_autorun_threshold).")
QVR_SLIDER_BYNAME(cl_forwardspeed, "Movement Speed", 100.f, 400.f, 25.f, "%.0f", ext(),
    "Walking speed (running is this times the running multiplier).")
QVR_CYCLE_BYNAME(cl_alwaysrun, "Default Speed", "The speed button switches to the other.",
    {1.f, "Run"}, {0.f, "Walk"})
QVR_SLIDER_BYNAME(cl_movespeedkey, "Running Mult.", 1.f, 3.f, 0.05f, "%.2f", ext(), "How much faster running is than walking.")
QVR_TOGGLE(vr_crouch_speed, "Slower When Crouched",
    "Crouched for real (your eyes a little under standing), the stick moves you at most Crouched Speed times "
    "your top speed, eased in and out as you crouch and stand, and always slow enough that monsters that "
    "haven't noticed you don't hear your steps. Jumps, falls and knockback are untouched (vr_crouch_speed).")
QVR_SLIDER(vr_crouch_speed_scale, "Crouched Speed", 0.2f, 1.f, 0.05f, "%.2fx", noExt,
    "Crouched, the most speed the stick moves you at, times your top speed. Whatever it is, crouched you "
    "stay under the speed whose steps monsters hear (Stealth AI > Running From) (vr_crouch_speed_scale).")
QVR_TOGGLE(vr_slope_walk, "Walk Up Slopes Slowly",
    "Standing on a slope you can walk on, gravity only presses you into it, none of it pulls you downhill: "
    "with the stick pushed only a little you still walk up slopes and stairs with a smooth ramp over them "
    "from a standstill. Off: Quake's (under about a quarter stick you never got going up one; going down "
    "you ran a little faster). Flat floors are the same either way (vr_slope_walk).")
QVR_CYCLE(vr_snap_turn, "Turning",
    "Smooth: the turning stick turns you steadily (Turn Speed). Snap: each push turns you at once by the angle.",
    {0.f, "Smooth"}, {30.f, "Snap 30"}, {45.f, "Snap 45"}, {90.f, "Snap 90"})
QVR_SLIDER(vr_turn_speed, "Turn Speed", 1.f, 8.f, 0.25f, "%.2f", ext(), "Smooth turning's speed.")
QVR_TOGGLE_BYNAME(vr_enable_joystick_turn, "Enable Joystick Turn",
    "Allows turning using the thumbstick. Off: the stick never turns you, in any state (menus, climbing, swimming), so you turn in real life; the stick's other uses stay. The same as VR Settings' Turning Mode: Off.")
QVR_TOGGLE(vr_teleport_enabled, "Teleport", "Jump to the spot you point at instead of walking there.")
QVR_SLIDER(vr_teleport_range, "Teleport Range", 100.f, 800.f, 50.f, "%.0f", ext(100.f, 3000.f),
    "How far a teleport reaches, in Quake's units (one is about 3 cm).")
QVR_SLIDER_BYNAME(vr_lean_radius, "Lean", 0.f, 14.f, 1.f, "%.0f units", ext(),
    "How far your head may lean off the middle of your body's box before the body follows: get your face near walls and over railings. 0: the body always under the head.")
QVR_SLIDER_BYNAME(vr_lean_recenter, "Lean Recentre", 0.f, 2.f, 0.1f, "%.1f m/s", ext(),
    "How fast the body slides back under your head when it can (not into walls, not over ledges), at least; with Lean Detection, faster the further off it is, and not while you lean. 0: never.")
QVR_SLIDER_BYNAME(vr_lean_detect, "Lean Detection", 0.f, 2.f, 0.1f, "%.1fx", noExt,
    "Tells a lean from walking in the room. Leaning (your head lower and tilted the way you lean, your hands left by your hips), your feet and hips stay where you stand and your back tilts; walking, the body follows your head. 0: off, the body always slides back under your head; higher: more readily a lean.")
QVR_CYCLE(vr_lean_turn, "Lean on Turns",
    "Turning with the stick while you lean keeps the lean on your body: the game's body stays where your real one stands. Round the Head: smooth turns keep your view where it is and swing the body round under it (where it fits), snap turns swing your head round the body. Round the Body: every turn swings your head round the body. Off: the old way (the body ended up leaning the wrong way, or off to a side, until it slid back).",
    {0.f, "Off"}, {1.f, "Round the Body"}, {2.f, "Round the Head"})
QVR_SLIDER(vr_roomscale_move_mult, "Room Scale", 0.5f, 2.f, 0.1f, "%.1fx", ext(0.2f, 5.f),
    "How far walking in your room moves you in the game (1: as far).")
QVR_TOGGLE_BYNAME(vr_roomscale_jump, "Roomscale Jump", "Jumping in the room jumps in the game.")
QVR_SLIDER_BYNAME(vr_roomscale_jump_threshold, "Jump Threshold", 0.3f, 2.f, 0.05f, "%.2f m/s", ext(), "How fast the head must rise above the standing height.")

// vr_menu_pages.inc: pageClimbing
QVR_TOGGLE_BYNAME(vr_climb, "Climbing",
    "Grip a ledge or a rung with either empty hand, or both, to hang from it; pull to climb hand over hand, shimmy along a ledge, pull up onto it. Edges only, not flat walls.")
QVR_SLIDER_BYNAME(vr_climb_min_height, "Lowest Ledge", 0.f, 64.f, 2.f, "%.0f units", ext(), "How far above your feet a ledge must be to grab it.")
QVR_SLIDER_BYNAME(vr_climb_fling, "Ledge Fling", 0.f, 2.f, 0.1f, "%.1f", ext(),
    "How much letting go of a ledge flings you with your hands' motion.")
QVR_SLIDER_BYNAME(vr_climb_blast_letgo, "Blasts Knock You Off", 0.f, 100.f, 5.f, "%.0f damage", ext(0.f, 300.f),
    "Hanging from a ledge or a rung, an explosion this strong or more (its damage to you; a rocket's is up to 120) makes you let go, and throws you as it would on the ground. 0: never. A grunt's or an enforcer's shove that lands always does.")
QVR_SLIDER_BYNAME(vr_climb_leniency, "Grab Leniency", 0.f, 30.f, 1.f, "%.0f cm", ext(),
    "How far from a ledge or a rung a grip still takes it: the nearest one within this distance of your hand (not through walls, behind you or below your reach). 0: only a hand touching it (Touch Distance).")
QVR_SLIDER_BYNAME(vr_climb_leniency_air, "Mid-Air Leniency", 0.f, 15.f, 0.5f, "+%.1f cm", ext(),
    "Extra Grab Leniency while you are in the air, neither on the ground nor holding on: after a jump or while falling, so a ledge you leap at is easier to catch. 0: the same as on the ground.")
QVR_SLIDER_BYNAME(vr_climb_air_grab_time, "Mid-Air Grab Window", 0.f, 1.f, 0.05f, "%.2f s", ext(),
    "In the air (after a jump, falling), a grip pressed away from any ledge keeps reaching for one this long while you hold it: press at the top of a jump and your hand takes the ledge as it comes down onto it. 0: only at the moment you press.")
QVR_SLIDER_BYNAME(vr_climb_touch, "Touch Distance", 0.f, 15.f, 0.5f, "%.1f cm", ext(),
    "How close your hand must be to a ledge's top, or to the face under its lip, to count as touching it: the whole reach at Grab Leniency 0 (the leniency adds its distance round this).")
QVR_SLIDER_BYNAME(vr_climb_over_top, "Reach Over the Top", 0.f, 40.f, 1.f, "%.0f units", ext(),
    "Over the top of a ledge, how far in from the drop a hand touching the top still holds it (up to how deep the top is). Less: only a hand near the edge grabs.")
QVR_TOGGLE_BYNAME(vr_climb_slide, "Hands Slide Along Ledges", nullptr)
QVR_SLIDER_BYNAME(vr_physsound_grab, "Grab Sound", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "A small slap as your hand takes a hold, of what the hold is made of: wood, muted metal or stone (0 off; also Carrying: Physics Sounds).")
QVR_SLIDER_BYNAME(vr_climb_mantle_height, "Mantle Height", 0.f, 40.f, 1.f, "%.0f units", noExt,
    "Holding a ledge, how far your head must rise above its top before you pull up onto it. Higher: you "
    "can lean up and down over a ledge without mantling (the old point was about 15 at a chest-high ledge; "
    "32 units are about a metre).")
QVR_TOGGLE_BYNAME(vr_climb_mantle_lenient, "Mantle onto Slopes",
    "Pulling yourself up onto a ledge whose top isn't level (rising or falling away from the edge, or "
    "sloping along it) stands you on the slope, up to Steepest Slope. Off: only level tops, as before.")
QVR_SLIDER_BYNAME(vr_climb_mantle_slope, "Steepest Slope", 10.f, 60.f, 5.f, "%.0f deg", noExt,
    "The steepest top a mantle stands you on with Mantle onto Slopes (Quake walks up to about 45).")
QVR_SLIDER_BYNAME(vr_climb_mantle_grunt, "Mantle Grunt", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "A small grunt as you pull yourself up onto a ledge (0 off).")
QVR_TOGGLE_BYNAME(vr_climb_props, "Climb Props",
    "The edges of crates, barrels, explosive boxes and their stacks are holds too, as they lie (on their side, tipped, on a lift), and you can pull up onto them. A heavy one holds like a wall; a light one tips or slides towards you and your hand lets go. Their lips are orange in Debug: Show Ledges.")
QVR_SLIDER_BYNAME(vr_climb_prop_min_height, "Lowest Prop Ledge", 0.f, 96.f, 2.f, "%.0f units", ext(),
    "Standing on the ground, how far above your feet a prop's edge must be to climb it: lower, a grip there picks the prop up instead (a small crate is 32, a large one 48; one lying on its side 40). Hanging or in the air: Lowest Ledge, as any ledge.")
QVR_SLIDER_BYNAME(vr_climb_stamina_slip, "Exhausted: Slip Time", 0.f, 5.f, 0.25f, "%.2f s", ext(),
    "With no stamina left: 0, your hands let go at once. More: they can't pull you up any more, you sink until your arms are straight, and they slip off after this long.")
QVR_SLIDER_BYNAME(vr_climb_hand_out, "Hand on Hold: Towards You", -10.f, 10.f, 0.5f, "%.1f cm", ext(),
    "Where your hand is drawn on a ledge or a rung: back from the lip, towards you (negative: deeper). Looks only; the climbing is the same.")
QVR_SLIDER_BYNAME(vr_climb_hand_up, "Hand on Hold: Up", -10.f, 10.f, 0.5f, "%.1f cm", ext(),
    "Where your hand is drawn on a hold: up (negative: down). Looks only.")
QVR_SLIDER_BYNAME(vr_climb_hand_side, "Hand on Hold: Sideways", -10.f, 10.f, 0.5f, "%.1f cm", ext(),
    "Where your hand is drawn on a hold: along the edge, outwards (negative: towards the other hand). Looks only.")
QVR_SLIDER_BYNAME(vr_climb_hand_turn_blend, "Hold Rotation Blend", 0.f, 1.f, 0.05f, "%.2f", ext(),
    "How your hand is turned on a hold: 0 facing the ledge (Hand on Hold: Pitch, Yaw, Roll), 1 as your controller is. Looks only.")
QVR_SLIDER_BYNAME(vr_climb_hand_pitch, "Hand on Hold: Pitch", -30.f, 120.f, 5.f, "%.0f deg", ext(),
    "The hand facing a hold, its knuckles tilted up: 0 the palm down on the top, 90 the palm against the face under the lip. Looks only.")
QVR_SLIDER_BYNAME(vr_climb_hand_yaw, "Hand on Hold: Yaw", -60.f, 60.f, 5.f, "%.0f deg", ext(),
    "The hand facing a hold, its knuckles turned towards the other hand (negative: away). Looks only.")
QVR_SLIDER_BYNAME(vr_climb_hand_roll, "Hand on Hold: Roll", -90.f, 90.f, 5.f, "%.0f deg", ext(),
    "The hand facing a hold, rolled about its knuckles, the thumb's side up (negative: down). Looks only.")

// vr_menu_pages.inc: pageSwimSettings
QVR_TOGGLE_BYNAME(vr_swim, "Swimming",
    "In water the stick slows and strokes of your hands move you.")
QVR_SLIDER_BYNAME(vr_swim_shallow_speed, "Stick in Shallow Water", 0.f, 1.f, 0.05f, "%.2f", noExt, "The stick's speed with your feet in water.")
QVR_SLIDER_BYNAME(vr_swim_wade_speed, "Stick Wading", 0.f, 1.f, 0.05f, "%.2f", noExt, "Waist deep, standing on the bottom.")
QVR_SLIDER_BYNAME(vr_swim_stick_speed, "Stick Swimming", 0.f, 1.f, 0.05f, "%.2f", noExt, "Under water or off the bottom: swim with your hands.")
QVR_SLIDER_BYNAME(vr_air_supply, "Air Supply", 1.f, 4.f, 0.25f, "%.2fx", noExt,
    "How long you can stay under water before you start drowning: times Quake's 12 seconds (1.5x: 18 s). Once drowning, the damage comes as ever. The Biosuit's air is unchanged.")
QVR_TOGGLE_BYNAME(vr_water_jump, "Jump Out of Water",
    "On (Quake's): swimming against an edge with room above it lifts you out. Off: you climb out with your hands, grabbing the edge (needs Climbing on; with Climbing off, Quake's jump out stays).")
QVR_SLIDER_BYNAME(vr_swim_stroke, "Stroke Strength", 0.f, 30.f, 1.f, "%.0f", ext(), "How far a stroke of your hand moves you.")
QVR_SLIDER_BYNAME(vr_swim_speed_exp, "Speed Curve", 1.f, 3.f, 0.25f, "%.2f", ext(),
    "How the push grows with the hand's speed. 1: in proportion. 2: as water's drag, so a brisk stroke outweighs a slower return by far. 3: even more.")
QVR_SLIDER_BYNAME(vr_swim_palm, "Palm Matters", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "How much more a hand meeting the water flat pushes than one meeting it at an angle. Either side of the hand: which side leads is Stroke Against Palm.")
QVR_SLIDER_BYNAME(vr_swim_against_palm, "Stroke Against Palm", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much a stroke counts when the back of your hand leads (a backhand, or turning a hand round to reposition it) instead of the palm pushing the water. 0: ignored. 1: as a palm stroke. Blends smoothly through the hand edge-on.")
QVR_SLIDER_BYNAME(vr_swim_flat_exp, "Palm Sharpness", 0.5f, 4.f, 0.25f, "%.2f", ext(),
    "How flat the palm must be to push fully. 1: pushing falls off evenly as the hand turns; 2-3: only a really flat palm pushes well.")
QVR_SLIDER_BYNAME(vr_swim_recovery, "Recovery Push", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much a hand going edge first (bringing it back for the next stroke) pushes.")
QVR_SLIDER_BYNAME(vr_swim_palm_dir, "Push Along Palm", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "0: you go against the hand's motion. 1: away from where the palm faces, so sweeping the hands in with palms facing each other pushes sideways (the two cancel), not back.")
QVR_SLIDER_BYNAME(vr_swim_stroke_min, "Slowest Push", 0.f, 1.f, 0.05f, "%.2f m/s", noExt,
    "A hand slower than this pushes nothing at all (tracking jitter, drifting hands).")
QVR_SLIDER_BYNAME(vr_swim_power_threshold, "Intent Threshold", 0.f, 3.f, 0.1f, "%.1f m/s", ext(),
    "A stroke moves you only if the hand's top speed during it passes this: a brisk stroke swims, a relaxed return of the arms does nothing. 0: every motion pushes.")
QVR_SLIDER_BYNAME(vr_swim_power_knee, "Intent Fade-In", 0.f, 2.f, 0.1f, "%.1f m/s", ext(),
    "Above the threshold, strokes fade in to full power over this much more speed. 0: all or nothing.")
QVR_TOGGLE_BYNAME(vr_swim_power_whole, "Whole Stroke Counts",
    "On: once a stroke passes the threshold, all of it pushes, its slow start too. Off: only from the moment it passes.")
QVR_SLIDER_BYNAME(vr_swim_intent_memory, "Stroke Memory", 0.f, 4.f, 0.25f, "%.2f s", ext(),
    "How long each hand remembers the way of its last full stroke, to tell the next return from a stroke back the other way. 0 off.")
QVR_SLIDER_BYNAME(vr_swim_reverse_damp, "Return Damping", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much a stroke against the remembered one is damped when it is slower than the Reverse Stroke Speed below (0.3 slower or more: fully). A deliberate stroke back, as brisk, is not damped.")
QVR_SLIDER_BYNAME(vr_swim_reverse_speed, "Reverse Stroke Speed", 0.5f, 2.f, 0.05f, "%.2fx", ext(),
    "How fast, over the stroke it reverses, a stroke the other way must be to count fully. 1: as fast. 1.2: 20% faster (returns never swim you back). 0.8: a bit slower is enough.")
QVR_SLIDER_BYNAME(vr_swim_look, "Swim Where You Look", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "You swim where you look (look up to rise): this much of a push sideways of it is lost.")
QVR_SLIDER_BYNAME(vr_swim_stroke_assist, "Stroke Steering", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "Strokes push this much more towards where the stick points, and as much less against it.")
QVR_SLIDER_BYNAME(vr_swim_stroke_pitch, "Stroke Pitch Offset", -30.f, 30.f, 1.f, "%+.0f deg", ext(-90.f, 90.f),
    "Tilts every stroke's push up (positive) or down (negative), as the last adjustment: if strokes carry you upward a little too much, a few degrees down. 0: as they are.")
QVR_SLIDER_BYNAME(vr_swim_glide, "Glide", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "How far you glide between strokes (less of the water's drag).")
QVR_SLIDER_BYNAME(vr_swim_max_speed, "Swim Top Speed", 100.f, 800.f, 25.f, "%.0f", ext(), nullptr)

// vr_menu_pages.inc: pageHandGunCalibration
QVR_TOGGLE_BYNAME(vr_show_controller, "Show Controller",
    "Draws each real controller, translucent, where the game tracks it, with its axes (red along the handle, "
    "green left, blue up) and its point (white: the middle of the handle); a yellow line to the hand's "
    "point when the calibration moves it. If the preview itself is off your real controller, line it up "
    "first (Weapon Offsets > Controller Preview). Not saved.")
QVR_SLIDER_BYNAME(vr_handcal_x, "Main Hand X (forward)", -5.f, 5.f, 0.1f, "%+.1f cm", ext(), nullptr)
QVR_SLIDER_BYNAME(vr_handcal_y, "Main Hand Y (left)", -5.f, 5.f, 0.1f, "%+.1f cm", ext(), nullptr)
QVR_SLIDER_BYNAME(vr_handcal_z, "Main Hand Z (up)", -5.f, 5.f, 0.1f, "%+.1f cm", ext(), nullptr)
QVR_SLIDER(vr_gunangle, "Main Hand Pitch (Gun Angle)", -30.f, 90.f, 2.5f, "%.1f", ext(-180.f, 180.f),
    "Gun Angle (vr_gunangle): how the main hand and the guns in it are tilted on the controller: set it so "
    "aiming feels natural. Changes where it aims.")
QVR_SLIDER(vr_offhandpitch, "Off Hand Pitch (Off Hand Angle)", -30.f, 90.f, 2.5f, "%.1f", ext(-180.f, 180.f),
    "Off Hand Angle (vr_offhandpitch): the same for the off hand.")
QVR_SLIDER_BYNAME(vr_gunyaw, "Main Hand Yaw (left)", -30.f, 30.f, 0.25f, "%+.2f", ext(-180.f, 180.f),
    "Gun Yaw (vr_gunyaw): turns the whole hand, and what it holds, left about the controller's tracked "
    "point. Changes where it aims.")
QVR_SLIDER_BYNAME(vr_handcal_roll, "Main Hand Roll", -30.f, 30.f, 0.25f, "%+.2f", ext(-180.f, 180.f), nullptr)
QVR_CYCLE_BYNAME(vr_handcal_off_mirror, "Off Hand",
    "Mirrors: the off hand takes the main hand's values mirrored (Y, Yaw and Roll the other way). Its Own "
    "Values: its own sliders, below.",
    {1.f, "Mirrors the Main Hand"}, {0.f, "Its Own Values"})
QVR_SLIDER_BYNAME(vr_handcal_off_x, "Off Hand X (forward)", -5.f, 5.f, 0.1f, "%+.1f cm", ext(), nullptr)
QVR_SLIDER_BYNAME(vr_handcal_off_y, "Off Hand Y (left)", -5.f, 5.f, 0.1f, "%+.1f cm", ext(), nullptr)
QVR_SLIDER_BYNAME(vr_handcal_off_z, "Off Hand Z (up)", -5.f, 5.f, 0.1f, "%+.1f cm", ext(), nullptr)
QVR_SLIDER_BYNAME(vr_offhandyaw, "Off Hand Yaw (left)", -30.f, 30.f, 0.25f, "%+.2f", ext(-180.f, 180.f),
    "Off-Hand Yaw (vr_offhandyaw): turns the whole hand, and what it holds, left about the "
    "controller's tracked point. Changes where it aims.")
QVR_SLIDER_BYNAME(vr_handcal_off_roll, "Off Hand Roll", -30.f, 30.f, 0.25f, "%+.2f", ext(-180.f, 180.f), nullptr)
QVR_SLIDER_BYNAME(vr_gunmodelscale, "Gun Model Scale", 0.1f, 2.f, 0.05f, "%.2f", ext(0.02f, 5.f),
    "Scale multiplier for gun models. Does NOT affect aiming, but does affect muzzle position (where bullets and projectiles come from).")
QVR_SLIDER_BYNAME(vr_gunmodely, "Gun Model Z Offset", -10.f, 10.f, 0.1f, "%.1f", ext(-50.f, 50.f), "Z offset for gun models. Does NOT affect aiming.")
QVR_SLIDER_BYNAME(vr_gun_z_offset, "Gun Z Offset", -30.f, 30.f, 0.25f, "%.1f", ext(-150.f, 150.f), "Z offset for guns/hands. Affects aiming.")
QVR_SLIDER_BYNAME(vr_finger_grip_bias, "Finger Grip Bias", 0.f, 1.f, 0.05f, "%.2f", noExt, nullptr)
QVR_TOGGLE_BYNAME(vr_finger_auto_close_thumb, "Auto close thumb", nullptr)
QVR_SLIDER_BYNAME(vr_weapon_button_cone, "Ammo Button Cone", 10.f, 180.f, 5.f, "%.0f deg", noExt,
    "The ammo button on a gun is pressed only by the other fingertip coming at it from its front, within "
    "this many degrees of the way it faces; never from behind (180: from anywhere).")
QVR_SLIDER_BYNAME(vr_weapon_button_depth, "Ammo Button Depth", 0.f, 3.f, 0.1f, "%.1f units", ext(0.f, 10.f),
    "How far behind the button's middle the cone above starts: a fingertip coming in flat, just over the "
    "button's face, still presses it (0: the cone from its middle).")
QVR_TOGGLE_BYNAME(vr_ammo_switch_empty_hand, "Ammo Switch Needs Empty Hand",
    "The ammo type switch button on a gun works only while your other hand holds nothing at all: not another "
    "gun (akimbo), a prop, a torch or a grabbed enemy. A refused press gives a short click and a buzz (off: "
    "always works).")

// vr_menu_pages.inc: pageFingersCollisions
QVR_TOGGLE_BYNAME(vr_hand_rig, "Jointed Hand",
    "The hand as one model with three joints a finger and an opposable thumb: the fingers curl smoothly and "
    "wrap what they hold. Off: the palm and five finger models as before (their six curl frames).")
QVR_TOGGLE_BYNAME(vr_hand_fit, "Fit Fingers to What You Hold",
    "With the jointed hand: the fingers close on what the hand holds (weapons, grabbed things) and stop on its "
    "surface, never through it and never floating off it. Off: they curl as the controller says.")
QVR_SLIDER_BYNAME(vr_hand_fit_thumb_outside, "Thumb Round In-Palm Props", 0.f, 3.f, 0.25f, "%.2f cm", ext(),
    "A rock, a brick or a grenade held In the Palm has the thumb round its outside, never tucked between your "
    "palm and it: its base may sink into it as far as this to lie along it. 0: as any held thing.")
QVR_TOGGLE_BYNAME(vr_hand_fit_thumb_wide, "Thumb Opens Wide Round Them",
    "With Thumb Round In-Palm Props: when the thumb would sink into the thing at its usual turns (a hand "
    "grenade's side), it opens wider and wraps round its outside instead. Off: it lies along it, sunk in.")
QVR_CYCLE_BYNAME(vr_model_collide, "Weapons Stop at Models",
    "A weapon held into a monster, a corpse or another player (and, with Objects, a box, gib or weapon on the "
    "ground) is drawn stopped at its surface: the model as drawn, not its much bigger box. Drawn only: hits "
    "and shots still come from your hand.",
    {0.f, "Off"}, {1.f, "Monsters"}, {2.f, "Monsters and Objects"})

// vr_menu_pages.inc: pageMeleeSettings
QVR_SLIDER_BYNAME(vr_melee_speed, "Swing Speed", 1.f, 8.f, 0.25f, "%.2f m/s", ext(),
    "How fast the striking hand must move to hit: a punch, a pommel strike; a weapon's swing a quarter faster, a stab three quarters of it. Any part of the blade then hits. At this speed a hit is weak; twice it, a full hit; faster, harder still (up to Most Blow Damage). A sword's cuts and stabs are measured at its tip, which swings about three times as fast as the hand.")
QVR_SLIDER_BYNAME(vr_melee_pommel_wait, "Pommel Strike Wait", 0.f, 0.2f, 0.01f, "%.2f s", ext(),
    "How long the touch of a pommel, a handle's end or a gun's butt waits for the blade or head to reach the same thing, so a cut from close by counts as a cut. Only pommel and butt strikes wait: punches and blades land at once. Lower: pommel strikes land sooner, but close cuts count as pommel strikes more often.")
QVR_SLIDER_BYNAME(vr_melee_butt_run, "Gun Butt Travel", 0.2f, 0.8f, 0.05f, "%.2f m", ext(0.f, 1.5f),
    "How far a gun must be carried at something within 0.3 s for its butt to strike: a gun wiggled in place flings its butt fast without going anywhere. 0.2: as any blow.")
QVR_SLIDER_BYNAME(vr_melee_punch_mult, "Punch Damage Mult.", 1.f, 3.f, 0.05f, "%.2f", ext(),
    "Damage of every punch: straight, hook, uppercut or overhead alike.")
QVR_TOGGLE_BYNAME(vr_melee_slap, "Slaps",
    "Swing an open hand across into an enemy to slap it: weaker than a punch, it never pops a head, cuts a limb or draws blood. Pushing an open palm out ahead is still a shove (one hand or both). Off: an open hand never strikes.")
QVR_SLIDER_BYNAME(vr_melee_slap_mult, "Slap Damage Mult.", 0.f, 1.f, 0.05f, "x%.2f", ext(0.f, 3.f), "A slap's damage, times a punch's.")
QVR_SLIDER_BYNAME(vr_melee_dmg_multiplier, "Damage Multiplier", 0.25f, 15.f, 0.25f, "%.1f", ext(), "Damage multiplier for melee attacks.")
QVR_SLIDER_BYNAME(vr_melee_dmg_min, "Least Blow Damage", 0.f, 1.f, 0.05f, "x%.2f", ext(0.f, 3.f),
    "The weakest blow still deals this much of its weapon's base damage, however slow and wherever it lands (a pommel strike its own share of it). Before the Damage Multiplier and the punch multiplier.")
QVR_SLIDER_BYNAME(vr_melee_dmg_max, "Most Blow Damage", 1.f, 4.f, 0.05f, "x%.2f", ext(1.f, 10.f),
    "The hardest blow deals at most this much of its weapon's base damage: the swing's speed times the zone's multiplier (a head hit) is capped here, so a fast head hit counts near its full speed and no more. Before the Damage Multiplier and the punch multiplier.")
QVR_SLIDER_BYNAME(vr_melee_range_multiplier, "Range Multiplier", 0.25f, 15.f, 0.25f, "%.1f", ext(), "Range multiplier for melee attacks.")
QVR_SLIDER_BYNAME(vr_melee_wall_hold, "Wall Hit Window", 0.f, 0.5f, 0.05f, "%.2f s", noExt,
    "A weapon swung into a wall or the floor is held off it, and slows down there: for this long after it met the surface, while it stays against it, the blow is still judged, at the speed it came in at, so a deliberate swing at a wall or the floor registers (sound, sparks, mark). A light brush along a wall still doesn't. 0: only in the frame the weapon crosses the surface, and never chopping down at the floor (as before).")
QVR_CYCLE_BYNAME(vr_melee_bloodlust, "Melee Bloodlust",
    "When enabled, restores the player's health when performing melee attacks on enemies.",
    {0.f, "On"}, {1.f, "Off"})
QVR_SLIDER_BYNAME(vr_melee_bloodlust_mult, "Melee Bloodlust Mult.", 0.05f, 5.f, 0.05f, "%.2f", ext(), "Multiplier for melee bloodlust health restoration.")
QVR_TOGGLE_BYNAME(vr_melee_swish, "Whoosh by Speed",
    "Every swing of a fist, an open hand or a melee weapon that moves air whooshes, louder the faster: softly short of a blow, a blow at full volume, the fastest blows louder still and a little higher. Off: only a blow whooshes, always at full volume. A gun's swing whooshes only as a blow.")
QVR_SLIDER_BYNAME(vr_melee_swish_min, "Heard From", 0.2f, 0.9f, 0.05f, "%.2fx", ext(0.05f, 0.95f),
    "The slowest swing heard, as a share of the speed its blow would land at (slower: silent).")
QVR_SLIDER_BYNAME(vr_melee_swish_sub, "Short of a Blow", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "Volume of the loudest swing that is not a blow, just under the speed it would land at (a blow is 1). Lower: the step to a blow stands out more.")
QVR_SLIDER_BYNAME(vr_melee_swish_full, "Loudest Blow At", 1.2f, 3.f, 0.05f, "%.2fx", ext(1.05f, 5.f),
    "The speed, times the blow's landing speed, at which its whoosh is loudest (Loudest Blow).")
QVR_SLIDER_BYNAME(vr_melee_swish_gain, "Loudest Blow", 1.f, 2.5f, 0.05f, "%.2fx", ext(1.f, 4.f),
    "Volume of the fastest blows' whoosh, times full volume (1: every blow at full volume).")
QVR_TOGGLE_BYNAME(vr_melee_phase, "Swing Through Enemies",
    "While your hand swings fast, your fist and the weapon in it pass through enemies instead of being drawn stopped at their bodies (Hands Stop at Models). Walls and things lying about still stop them. The blow is tested where your real hand goes either way: this shows the swing where it is tested. After the swing the hand is eased back out of the enemy. Off: stopped at enemies however fast (as before).")
QVR_SLIDER_BYNAME(vr_melee_phase_speed, "Swing Through Speed", 1.f, 6.f, 0.25f, "%.2f m/s", ext(0.25f, 10.f),
    "How fast the hand must move to pass through. 4 (the default): a hard blow's speed; 2.25: a stab's least speed at Swing Speed 3 (a punch 3, a weapon's swing 3.75). Lower: slower pushes pass through too; higher: only hard blows.")
QVR_SLIDER_BYNAME(vr_melee_phase_time, "Swing Through Follow-Through", 0.f, 0.5f, 0.01f, "%.2f s", ext(0.f, 2.f),
    "How long the hand still passes through after it slowed down, so a blow's end isn't stopped inside the enemy.")
QVR_TOGGLE_BYNAME(vr_melee_phase_hold, "Swing Through: Guns and Props",
    "Also a gun or a prop held in one hand, swung as a club. Off: only fists and melee weapons (the axe, Mjolnir, the swords, the crowbar, the chainsaw).")
QVR_TOGGLE_BYNAME(vr_melee_world_pass, "Strike Past Walls and Floor",
    "While your hand swings fast at an enemy, a corpse or a ragdoll near the wall or the floor, most of your weapon is let into the level instead of being held out of it, so a cut at a body lying on the floor reaches it instead of the blade bouncing up off the floor first. Swung at the floor or a wall with no body near, it meets the level as before (sparks, marks, the wall blow). Off: always held out of the level whole.")
QVR_SLIDER_BYNAME(vr_melee_world_pass_length, "Strike Past: Length Held Out", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "During such a strike, the share of the weapon (from your hand) still held out of the walls and the floor: lower lets more of the blade in. 0: only your hand; 1: all of it (as off).")
QVR_SLIDER_BYNAME(vr_melee_world_pass_speed, "Strike Past: Speed", 1.f, 6.f, 0.25f, "%.2f m/s", ext(0.25f, 10.f),
    "How fast the hand must move for a strike to be let past the level.")
QVR_SLIDER_BYNAME(vr_melee_world_pass_time, "Strike Past: Follow-Through", 0.f, 0.5f, 0.05f, "%.2f s", ext(0.f, 2.f),
    "How long the weapon stays let in after the hand slowed (the end of the cut), before it is eased back out.")
QVR_SLIDER_BYNAME(vr_melee_world_pass_reach, "Strike Past: Target Reach", 0.f, 32.f, 1.f, "%.0f units", ext(0.f, 64.f),
    "How near the weapon's far end (and where it is going) the body must be for the strike to be let past the level.")
QVR_TOGGLE_BYNAME(vr_melee_multi, "Hit Everything the Edge Passes",
    "A swing of a weapon strikes every enemy, corpse, ragdoll and prop its striking part passes through, each once and each by its own speed and spot, and the wall or the floor as well (sparks, marks): a blade that meets the floor first still cuts the body lying on it. Bare fists strike one thing a punch. Off: as before (the floor or a wall struck first ends the swing).")
QVR_SLIDER_BYNAME(vr_melee_multi_max, "Most Things a Swing", 1.f, 6.f, 1.f, "%.0f", noExt,
    "How many things (not counting the wall or the floor) one swing strikes at most.")
QVR_TOGGLE(vr_headbutt, "Headbutt", "Lunge your head at something to headbutt it.")
QVR_SLIDER(vr_headbutt_speed, "Headbutt Speed", 0.4f, 3.f, 0.05f, "%.2f m/s", ext(), "How fast the head must lunge (towards where you look).")

// vr_menu_pages.inc: pageAimingSettings
QVR_CYCLE(vr_2h_mode, "Two-Handed",
    "Hold a gun with both hands to steady it. Virtual stock: a gun brought near your shoulder also aims from it.",
    {0.f, "Off"}, {1.f, "Basic"}, {2.f, "Virtual stock"})
QVR_CYCLE(vr_dominant_eye, "Dominant Eye",
    "The eye that looks along the sights: pointing at something with both eyes open, the one that stays on "
    "it when you close the other.",
    {0.f, "Right"}, {1.f, "Left"})
QVR_TOGGLE(vr_2h_handoff, "Two-Handed Hand-Off",
    "Letting go with the hand holding a two-handed weapon leaves it in the other hand: a sword changes hands; a gun hangs from its foregrip until a hand takes its handle.")
QVR_SLIDER(vr_2h_grip_sound, "Grip Click", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "Your other hand taking hold of a weapon's foregrip clicks softly from that hand (and pulses in it); "
    "letting go, a quieter click. How loud (0: no sound, the pulse stays).")
QVR_SLIDER_BYNAME(vr_2h_angle_threshold, "2H Aiming Threshold", -1.f, 1.f, 0.05f, "%.2f", noExt,
    "How far out of line the hands may be before two-handed aiming stops. Higher is stricter; -1 never stops.")
QVR_SLIDER_BYNAME(vr_2h_virtual_stock_factor, "2H Stock Factor", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much the virtual stock matters in the final angle calculation for two-handed aiming. Decrease if you want your hands to matter more.")
QVR_SLIDER(vr_2h_sticky, "Stickiness", 0.5f, 4.f, 0.05f, "%.2fx", ext(0.1f, 20.f),
    "Once your other hand holds a weapon's grip, how far it may go off it (20 units), how far out of line "
    "(2H Aiming Threshold) and past the muzzle, before it lets go: times this, always (and times the "
    "hotspot's own Stickiness: Weapon Offsets).")
QVR_SLIDER(vr_2h_sticky_fast, "While Swinging", 0.f, 5.f, 0.1f, "+%.1fx", ext(0.f, 20.f),
    "And this many times more while your hands swing (a two-handed melee blow): back to the above as they "
    "slow, so a weapon held still lets go as before. 0: off.")
QVR_SLIDER(vr_2h_sticky_fast_from, "Swing From", 0.f, 5.f, 0.1f, "%.1f m/s", ext(0.f, 20.f), "The faster hand's speed it starts growing from.")
QVR_SLIDER(vr_2h_sticky_fast_full, "Swing Full At", 0.5f, 10.f, 0.1f, "%.1f m/s", ext(0.1f, 30.f), "The faster hand's speed at which all of it applies.")
QVR_SLIDER(vr_2h_sticky_fast_hold, "Swing Fades Over", 0.f, 2.f, 0.05f, "%.2f s", ext(0.f, 10.f),
    "How long it takes to fade once the hands slow (a swing's end and the way back).")
QVR_TOGGLE(vr_weight_stamina, "Heavier When Tired",
    "As your stamina (Parry Stamina) runs low, what you hold gets heavier.")

// vr_menu_pages.inc: pageImmersionSettings
QVR_TOGGLE_BYNAME(vr_body_interactions, "Body Picks Up Items",
    "Pick up items and level weapons by walking over them. Thrown and dropped weapons still need your hands.")
QVR_CYCLE(vr_disablehaptics, "Haptics", "The controllers' vibrations, all of them.",
    {0.f, "On"}, {1.f, "Off"})
QVR_SLIDER(vr_haptics_strength, "Vibration Strength", 0.f, 1.f, 0.1f, "%.1f", ext(0.f, 2.f), "How strongly the controllers vibrate (0: never).")
QVR_CYCLE_BYNAME(vr_holster_mode, "Weapon Mode",
    "Immersive: a holster holds one weapon, and taking it out empties it. Quick Slots: holsters never empty (with weapon cycling).",
    {0.f, "Immersive"}, {1.f, "Quick Slots"})
QVR_CYCLE_BYNAME(vr_reload_mode, "Weapon Reloading Mode",
    "Weapons have magazines. All Holsters, Hip Holsters: a gun held at a holster reloads. Immersive: the shotgun is loaded by hand, a shell at a time from the ammo pouch on your belt; the other guns reload at the hip holsters (Weapons > Reloading). Reloading only works with 'immersive' weapon mode.",
    {0.f, "None"}, {1.f, "All Holsters"}, {2.f, "Hip Holsters"}, {3.f, "Immersive"})
QVR_CYCLE_BYNAME(vr_holster_haptics, "Holster Haptics", "Haptic feedback when hovering a usable holster slot.",
    {0.f, "Off"}, {1.f, "Continuous"}, {2.f, "Once"})
QVR_TOGGLE_BYNAME(vr_weapon_cycle_mode, "Weapon Cycle Mode",
    "Allows or disallows cycling weapons using controller buttons.")
QVR_TOGGLE_BYNAME(vr_weapondrop_particles, "Dropped Wpn Particles",
    "Enable particle effects for weapons dropped/thrown on the ground.")
QVR_TOGGLE(vr_magholster, "Magnetic Holsters",
    "As in RE4VR: with a hand at a holster that has a weapon in it, press the bind button to bind the weapon to that "
    "holster (press it again to unbind). Thrown or dropped, it flies back into the holster on its own. Weapon "
    "Mode Immersive.")
QVR_CYCLE(vr_magholster_button, "Bind Button",
    "Pressed by the hand at the holster. The press is taken only there (its usual action elsewhere). Lower Face "
    "Button: A or X.",
    {0.f, "Either Stick Click"}, {1.f, "Off-Hand Stick Click"}, {2.f, "Main-Hand Stick Click"},
    {3.f, "Lower Face Button"})
QVR_SLIDER(vr_magholster_return_time, "Return After", 0.f, 15.f, 0.5f, "%.1f s", ext(0.f, 120.f),
    "Out of your hands this long, it comes back (0: never by time).")
QVR_SLIDER(vr_magholster_return_distance, "Return Beyond", 0.f, 20.f, 0.5f, "%.1f m", ext(0.f, 200.f),
    "This far from its holster, it comes back (0: never by distance). Either one brings it back.")
QVR_SLIDER(vr_magholster_land_time, "Land First", 0.f, 3.f, 0.25f, "%.2f s", ext(0.f, 10.f),
    "A thrown or dropped weapon must lie still on the ground (or stuck in a wall) this long before either return "
    "brings it back, however far it is: a throw is let finish, never called back in the air (0: off).")
QVR_SLIDER(vr_magholster_flight_time, "Flight Time", 0.1f, 1.5f, 0.05f, "%.2f s", ext(0.05f, 5.f),
    "How long it takes to fly back, through walls, before it snaps in with a click. Grab it on the way to keep it.")
QVR_CYCLE(vr_magholster_style, "Chain",
    "Lanyard: a short chain hangs off a bound weapon, a stub of it at the empty holster. Tether: a chain from the "
    "holster to the weapon whenever it is out of it (in your hand too), slack close by, taut far off. Both.",
    {0.f, "Lanyard"}, {1.f, "Tether"}, {2.f, "Both"}, {3.f, "None"})
QVR_SLIDER(vr_magholster_chain_size, "Chain Size", 0.2f, 2.f, 0.05f, "%.2fx", ext(0.2f, 3.f),
    "Its links' size, times the flashlight chain's (1.6 cm links).")
QVR_SLIDER(vr_magholster_chain_alpha, "Chain Opacity", 0.f, 1.f, 0.05f, "%.2f", noExt, "Below 1 the chain is see-through (0: not drawn).")
QVR_SLIDER(vr_magholster_chain_lod, "Full Chain Up To", 0.f, 15.f, 0.5f, "%.1f m", ext(0.f, 100.f),
    "The tether is drawn as links this far along it from the holster, then as a plain thin line on to the "
    "weapon along the same curve (cheaper, unnoticeable far off); it never disappears, however far the "
    "weapon goes (0: all a line).")
QVR_SLIDER(vr_magholster_zip_volume, "Zip Volume", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "A bound weapon flying home: the reel's zip as it winds the chain back in (0: none).")
QVR_SLIDER(vr_magholster_draw_volume, "Draw Sound Volume", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "A bound weapon drawn from its holster: the chain paid out, heard over the holster's own sound (0: none).")
QVR_TOGGLE(vr_magholster_chain_collide, "Chain Collision",
    "The tether is a chain of points that hangs, lies on the floor and wraps round walls (the grappling "
    "hook rope's physics, coarser): its points and pieces kept out of the world (off: cheaper, through it).")
QVR_SLIDER(vr_magholster_chain_spacing, "Chain Point Spacing", 0.05f, 1.f, 0.05f, "%.2f m", ext(0.02f, 4.f),
    "Metres between the chain's simulated points (finer: truer on edges and the floor, more work). A long "
    "tether's are farther apart (Chain Most Points).")
QVR_SLIDER(vr_magholster_chain_points, "Chain Most Points", 4.f, 128.f, 4.f, "%.0f", ext(4.f, 256.f), "The most pieces the chain has, however long.")
QVR_SLIDER(vr_magholster_chain_iterations, "Chain Iterations", 1.f, 32.f, 1.f, "%.0f", ext(1.f, 64.f),
    "Passes a frame holding the pieces to their length (more: less stretch, more work).")
QVR_SLIDER(vr_magholster_chain_stiffness, "Chain Stiffness", 0.05f, 1.f, 0.05f, "%.2f", noExt,
    "The share of each piece's stretch a pass takes out (lower: stretchier, softer).")
QVR_SLIDER(vr_magholster_chain_radius, "Chain Thickness", 0.05f, 4.f, 0.05f, "%.2f", ext(0.05f, 8.f), "Units the chain's points keep off what they lie on.")
QVR_SLIDER(vr_magholster_chain_drag, "Chain Air Drag", 0.f, 10.f, 0.25f, "%.2f", ext(0.f, 20.f),
    "Its speed lost to the air a second (higher: it settles from a swing sooner).")
QVR_SLIDER(vr_magholster_chain_slide, "Chain Floor Slide", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The share of its speed along a surface a point keeps where it touches it (lower: a rougher floor).")
QVR_SLIDER(vr_magholster_chain_carry, "Chain Carry", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much your own movement carries the chain along near the holster (none of it at the weapon; 0: it is "
    "only pulled).")
QVR_SLIDER(vr_magholster_chain_sway, "Locomotion Sway", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much the chain swings as you walk and turn with the sticks: the share of that movement it lags "
    "behind (0: carried along whole; 1: all of it, as before). Your real body's moves swing it as ever; "
    "what lies on the floor is dragged as before.")
QVR_SLIDER(vr_magholster_chain_x, "All Forward", -8.f, 8.f, 0.25f, "%.2f", ext(-30.f, 30.f),
    "Where the chain leaves every holster: units forward (the body's frame). Added to each kind's below.")
QVR_SLIDER(vr_magholster_chain_y, "All Outwards", -8.f, 8.f, 0.25f, "%.2f", ext(-30.f, 30.f),
    "Units outwards (away from the body's middle: the left holsters' to the left).")
QVR_SLIDER(vr_magholster_chain_z, "All Up", -8.f, 8.f, 0.25f, "%.2f", ext(-30.f, 30.f), "Units up.")
QVR_SLIDER(vr_magholster_chain_hip_x, "Hip Forward", -8.f, 8.f, 0.25f, "%.2f", ext(-30.f, 30.f), "The hip holsters' chain origin: units forward.")
QVR_SLIDER(vr_magholster_chain_hip_y, "Hip Outwards", -8.f, 8.f, 0.25f, "%.2f", ext(-30.f, 30.f), "The hip holsters' chain origin: units outwards.")
QVR_SLIDER(vr_magholster_chain_hip_z, "Hip Up", -8.f, 8.f, 0.25f, "%.2f", ext(-30.f, 30.f), "The hip holsters' chain origin: units up.")
QVR_SLIDER(vr_magholster_chain_upper_x, "Chest Forward", -8.f, 8.f, 0.25f, "%.2f", ext(-30.f, 30.f), "The upper (chest) holsters' chain origin: units forward.")
QVR_SLIDER(vr_magholster_chain_upper_y, "Chest Outwards", -8.f, 8.f, 0.25f, "%.2f", ext(-30.f, 30.f), "The upper (chest) holsters' chain origin: units outwards.")
QVR_SLIDER(vr_magholster_chain_upper_z, "Chest Up", -8.f, 8.f, 0.25f, "%.2f", ext(-30.f, 30.f), "The upper (chest) holsters' chain origin: units up.")
QVR_SLIDER(vr_magholster_chain_shoulder_x, "Shoulder Forward", -8.f, 8.f, 0.25f, "%.2f", ext(-30.f, 30.f), "The shoulder holsters' chain origin: units forward.")
QVR_SLIDER(vr_magholster_chain_shoulder_y, "Shoulder Outwards", -8.f, 8.f, 0.25f, "%.2f", ext(-30.f, 30.f), "The shoulder holsters' chain origin: units outwards.")
QVR_SLIDER(vr_magholster_chain_shoulder_z, "Shoulder Up", -8.f, 8.f, 0.25f, "%.2f", ext(-30.f, 30.f), "The shoulder holsters' chain origin: units up.")
QVR_TOGGLE_BYNAME(vr_shells, "Shell Casings",
    "The shotgun throws out a spent shell each shot, the super shotgun its two as you reload (a flick reload flings them). They bounce, roll and lie on the floor.")

// vr_menu_pages.inc: pageReloadShotgun
QVR_TOGGLE_BYNAME(vr_reload_shell_pairs, "Shell Pairs",
    "The pouch gives two shells taped together: both go into the shotgun at once. A single shell when only "
    "one is left, or the shotgun in your other hand has one place left (one goes in if only one fits).")
QVR_TOGGLE_BYNAME(vr_reload_pouch_shot, "Reload at the Pouch",
    "Bring the port under it down onto the shells in the ammo pouch, the gun upright (its port facing down "
    "into the pouch, within Reloading: Pouch Angle): a shell goes in, from your shells. One a touch: take "
    "it away and back for the next.")
QVR_TOGGLE_BYNAME(vr_reload_show_ports, "Show Load Points",
    "Draws each held gun's load point (yellow) and its radius (green), a held magazine's reference point and "
    "radius (orange), and an attached magazine's box (blue: where a hand holds it, within Pull Reach, and "
    "where a hit knocks it out), to tune the offsets live.")
QVR_SLIDER_BYNAME(vr_reload_port_shot_x, "Port X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the shotgun port's load point forward (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_shot_y, "Port Y", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the shotgun port's load point left (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_shot_z, "Port Z", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the shotgun port's load point up (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_shot_radius, "Port Radius", 0.f, 10.f, 0.25f, "%.2f units", ext(0.f, 40.f),
    "How near the shotgun port's load point a held round must come to go in (a magazine: its reference point, within this plus its own radius).")

// vr_menu_pages.inc: pageReloadSuperShotgun
QVR_TOGGLE_BYNAME(vr_reload_ssg_break, "Break Open",
    "With Immersive: its fired shells stay in its barrels. Break it open (the ways below) to throw them "
    "out, spent and live (live ones to take again); it stays open, can't fire, and its chambers take the "
    "pouch's taped pair at its breech (a single shell when one chamber is loaded). Off: the flick reloads it "
    "as before.")
QVR_SLIDER_BYNAME(vr_reload_ssg_open_angle, "Open Angle", 0.f, 90.f, 1.f, "%.0f deg", noExt,
    "How far its barrels drop about the hinge when it is open (looks; its load point goes with them).")
QVR_SLIDER_BYNAME(vr_ssg_fire_anim_speed, "Firing Animation Speed", 0.5f, 3.f, 0.1f, "%.1fx", ext(0.25f, 5.f),
    "How fast its firing animation plays (1: id's, 0.6 s). It breaks open only once the animation is over: "
    "faster, it opens sooner after a shot.")
QVR_TOGGLE_BYNAME(vr_reload_ssg_open_flick, "Open by Flick",
    "A flick of the wrist (the muzzle snapped up) breaks it open.")
QVR_SLIDER_BYNAME(vr_reload_ssg_flick_open_speed, "Flick Open Speed", 200.f, 1500.f, 25.f, "%.0f deg/s", noExt,
    "How fast the wrist must turn to flick it open (higher: only a hard flick).")
QVR_TOGGLE_BYNAME(vr_reload_ssg_pry, "Open by Pry",
    "Holding it with both hands (the two-handed grip on its fore-end), push the barrels down with the front "
    "hand against the back hand (or lift the stock): it breaks open.")
QVR_SLIDER_BYNAME(vr_reload_ssg_pry_angle, "Pry Angle", 10.f, 90.f, 1.f, "%.0f deg", noExt,
    "How far the front hand must go down from the back hand's aim (or the stock up) to pry it open.")
QVR_SLIDER_BYNAME(vr_reload_ssg_pry_speed, "Pry Speed", 20.f, 600.f, 10.f, "%.0f deg/s", noExt, "How quickly: a steady two-handed aim never pries it open.")
QVR_TOGGLE_BYNAME(vr_reload_ssg_open_hit, "Open by a Hit from Above",
    "A fist, a held thing or the other gun hitting the front of its barrels from above (the gun's own up, "
    "as you hold it) forces it open.")
QVR_TOGGLE_BYNAME(vr_reload_ssg_open_button, "Open by B/Y",
    "The B/Y button of the controller holding it breaks it open.")
QVR_TOGGLE_BYNAME(vr_reload_ssg_close_flick, "Close by Flick", "Open, a flick snaps it shut.")
QVR_SLIDER_BYNAME(vr_reload_ssg_flick_close_speed, "Flick Close Speed", 200.f, 1500.f, 25.f, "%.0f deg/s", noExt, "How fast the wrist must turn to flick it shut.")
QVR_SLIDER_BYNAME(vr_reload_ssg_close_flick_loaded, "Flick Close Leniency When Loaded", 1.f, 2.f, 0.05f, "%.2fx", noExt,
    "Both barrels loaded, the flick shut needs this much less speed (1: the same as empty).")
QVR_TOGGLE_BYNAME(vr_reload_ssg_close_pry, "Close by Lifting the Barrels",
    "Open and held with both hands, the front hand lifting the barrels back up snaps it shut.")
QVR_SLIDER_BYNAME(vr_reload_ssg_lift_angle, "Lift Angle", 10.f, 90.f, 1.f, "%.0f deg", noExt,
    "How far the front hand must come back up (from its lowest) to lift it shut.")
QVR_SLIDER_BYNAME(vr_reload_ssg_lift_hold, "Lift Hold", 0.f, 1.f, 0.05f, "%.2f s", noExt,
    "How long it must stay lifted that far before it shuts (a jolt doesn't).")
QVR_TOGGLE_BYNAME(vr_reload_ssg_close_hit, "Close by a Hit from Below",
    "Open, a fist, a held thing or the other gun hitting the front of its barrels from below snaps it shut.")
QVR_TOGGLE_BYNAME(vr_reload_ssg_close_auto, "Close When Loaded",
    "Open, it snaps shut by itself once both chambers are loaded.")
QVR_TOGGLE_BYNAME(vr_reload_ssg_close_button, "Close by B/Y",
    "Open, the B/Y button of the controller holding it snaps it shut (with Open by B/Y: opened and closed with "
    "the button alone, no gesture).")
QVR_SLIDER_BYNAME(vr_reload_ssg_hit_after_load, "Hit After Loading", 0.f, 1.f, 0.01f, "%.2f s", noExt,
    "How long after a shell goes in before a hit opens or shuts it (the loading hand drawing back is no hit).")
QVR_TOGGLE_BYNAME(vr_reload_pouch_sshot, "Reload at the Pouch",
    "Broken open, touch its open barrels to the shells in the ammo pouch: its empty chambers fill, from your "
    "shells. No free hand needed: two super shotguns, each reloaded at the pouch. Shut, nothing.")
QVR_SLIDER_BYNAME(vr_reload_port_sshot_x, "Breech X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the super shotgun's load point (its breech face, between the chambers) forward (the gun's model "
    "units, as closed; it turns with the barrels open).")
QVR_SLIDER_BYNAME(vr_reload_port_sshot_y, "Breech Y", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the super shotgun's load point left (the gun's model units).")
QVR_SLIDER_BYNAME(vr_reload_port_sshot_z, "Breech Z", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the super shotgun's load point up (the gun's model units).")
QVR_SLIDER_BYNAME(vr_reload_port_sshot_radius, "Breech Radius", 0.f, 10.f, 0.25f, "%.2f units", ext(0.f, 40.f),
    "How near the breech a held pair's middle must come to go in (Show Load Points draws it).")

// vr_menu_pages.inc: pageReloadNailgun
QVR_TOGGLE_BYNAME(vr_reload_pouch_nail, "Reload at the Pouch",
    "With its magazine out, bring its empty well down onto the magazines standing in the ammo pouch, along "
    "them (Reloading: Pouch Angle): a fresh magazine seats, from your nails. With one in, nothing.")
QVR_SLIDER_BYNAME(vr_reload_port_nail_x, "Well X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the nailgun well's load point forward (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_nail_y, "Well Y", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the nailgun well's load point left (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_nail_z, "Well Z", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the nailgun well's load point up (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_nail_radius, "Well Radius", 0.f, 10.f, 0.25f, "%.2f units", ext(0.f, 40.f),
    "How near the nailgun well's load point a held round must come to go in (a magazine: its reference point, within this plus its own radius).")
QVR_SLIDER_BYNAME(vr_reload_mag_nail_x, "Top X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the nailgun magazine's reference point (its top, its feed end) along its gun (its model units).")
QVR_SLIDER_BYNAME(vr_reload_mag_nail_y, "Top Y", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the nailgun magazine's reference point (its top, its feed end) to its left (its model units).")
QVR_SLIDER_BYNAME(vr_reload_mag_nail_z, "Top Z", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the nailgun magazine's reference point (its top, its feed end) up its length (its model units).")
QVR_SLIDER_BYNAME(vr_reload_mag_nail_radius, "Top Radius", 0.f, 6.f, 0.25f, "%.2f units", ext(0.f, 20.f),
    "The nailgun magazine's own radius round its reference point (added to the well's).")
QVR_SLIDER_BYNAME(vr_reload_well_nail_x, "Receiver X", -6.f, 6.f, 0.05f, "%.2f", ext(),
    "Moves the nailgun receiver (the collar round the magazine's top, drawn on the gun) forward, to sit flush (the gun's model units; looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_nail_y, "Receiver Y", -6.f, 6.f, 0.05f, "%.2f", ext(),
    "Moves the nailgun receiver (the collar round the magazine's top, drawn on the gun) left, to sit flush (the gun's model units; looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_nail_z, "Receiver Z", -6.f, 6.f, 0.05f, "%.2f", ext(),
    "Moves the nailgun receiver (the collar round the magazine's top, drawn on the gun) up, to sit flush (the gun's model units; looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_nail_pitch, "Receiver Pitch", -90.f, 90.f, 1.f, "%.0f", ext(-180.f, 180.f), "Turns the nailgun receiver about its seat (looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_nail_yaw, "Receiver Yaw", -90.f, 90.f, 1.f, "%.0f", ext(-180.f, 180.f), "Turns the nailgun receiver about its seat (looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_nail_roll, "Receiver Roll", -90.f, 90.f, 1.f, "%.0f", ext(-180.f, 180.f), "Turns the nailgun receiver about its seat (looks only).")
QVR_SLIDER_BYNAME(vr_reload_hold_nail_x, "Hand X", -6.f, 6.f, 0.1f, "%.1f", ext(),
    "The other hand holding the attached magazine is drawn moved forward (units, the gun's aim; mirrored for the main hand). Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_nail_y, "Hand Y", -6.f, 6.f, 0.1f, "%.1f", ext(),
    "The other hand holding the attached magazine is drawn moved left (units, the gun's aim; mirrored for the main hand). Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_nail_z, "Hand Z", -6.f, 6.f, 0.1f, "%.1f", ext(),
    "The other hand holding the attached magazine is drawn moved up (units, the gun's aim; mirrored for the main hand). Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_nail_pitch, "Hand Pitch", -180.f, 180.f, 5.f, "%.0f", noExt,
    "And turned (degrees), once its grip lies along the magazine where it holds it. Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_nail_yaw, "Hand Yaw", -180.f, 180.f, 5.f, "%.0f", noExt,
    "And turned (degrees), once its grip lies along the magazine where it holds it. Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_nail_roll, "Hand Roll", -180.f, 180.f, 5.f, "%.0f", noExt,
    "And turned (degrees), once its grip lies along the magazine where it holds it. Looks only.")
QVR_TOGGLE_BYNAME(vr_reload_hold_nail_fingers, "Fingers Set by Hand",
    "Off: the fingers close round the magazine as it is. On: each finger at its curl below.")
QVR_SLIDER_BYNAME(vr_reload_hold_nail_curl_thumb, "Thumb Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_nail_curl_index, "Index Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_nail_curl_middle, "Middle Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_nail_curl_ring, "Ring Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_nail_curl_pinky, "Little Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_pull_reach, "Pull Reach", 0.f, 12.f, 0.25f, "%.2f units", ext(0.f, 40.f),
    "How far off the magazine (its box, as drawn) a hand closing its grip may be and still hold it "
    "(0: on it). Nearer the gun's own two-handed grip, the hand takes that instead.")
QVR_SLIDER_BYNAME(vr_reload_bump_speed, "Knock Out Speed", 0.f, 15.f, 0.1f, "%.1f m/s", ext(0.f, 50.f),
    "A hit on a seated magazine this hard pops it out: a punch, a held prop, another magazine (the bump "
    "reload: the new one is then seated at the well), or the other gun.")
QVR_SLIDER_BYNAME(vr_reload_hit_reach, "Knock Out Reach", 0.f, 10.f, 0.25f, "%.2f units", ext(0.f, 40.f), "How near the magazine (anywhere on it) a hit must land.")

// vr_menu_pages.inc: pageReloadSuperNailgun
QVR_TOGGLE_BYNAME(vr_reload_pouch_snail, "Reload at the Pouch",
    "With its magazine out, bring its empty well onto the magazines standing in the ammo pouch, along them "
    "(the gun on its side: its well is on its left; Reloading: Pouch Angle): a fresh magazine seats, from "
    "your nails. With one in, nothing.")
QVR_SLIDER_BYNAME(vr_reload_port_snail_x, "Well X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the super nailgun well's load point forward (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_snail_y, "Well Y", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the super nailgun well's load point left (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_snail_z, "Well Z", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the super nailgun well's load point up (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_snail_radius, "Well Radius", 0.f, 10.f, 0.25f, "%.2f units", ext(0.f, 40.f),
    "How near the super nailgun well's load point a held round must come to go in (a magazine: its reference point, within this plus its own radius).")
QVR_SLIDER_BYNAME(vr_reload_mag_snail_x, "Top X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the super nailgun magazine's reference point (its top, its feed end) along its gun (its model units).")
QVR_SLIDER_BYNAME(vr_reload_mag_snail_y, "Top Y", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the super nailgun magazine's reference point (its top, its feed end) to its left (its model units).")
QVR_SLIDER_BYNAME(vr_reload_mag_snail_z, "Top Z", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the super nailgun magazine's reference point (its top, its feed end) up its length (its model units).")
QVR_SLIDER_BYNAME(vr_reload_mag_snail_radius, "Top Radius", 0.f, 6.f, 0.25f, "%.2f units", ext(0.f, 20.f),
    "The super nailgun magazine's own radius round its reference point (added to the well's).")
QVR_SLIDER_BYNAME(vr_reload_well_snail_x, "Receiver X", -6.f, 6.f, 0.05f, "%.2f", ext(),
    "Moves the super nailgun receiver (the collar round the magazine's top, drawn on the gun) forward, to sit flush (the gun's model units; looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_snail_y, "Receiver Y", -6.f, 6.f, 0.05f, "%.2f", ext(),
    "Moves the super nailgun receiver (the collar round the magazine's top, drawn on the gun) left, to sit flush (the gun's model units; looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_snail_z, "Receiver Z", -6.f, 6.f, 0.05f, "%.2f", ext(),
    "Moves the super nailgun receiver (the collar round the magazine's top, drawn on the gun) up, to sit flush (the gun's model units; looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_snail_pitch, "Receiver Pitch", -90.f, 90.f, 1.f, "%.0f", ext(-180.f, 180.f),
    "Turns the super nailgun receiver about its seat (looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_snail_yaw, "Receiver Yaw", -90.f, 90.f, 1.f, "%.0f", ext(-180.f, 180.f),
    "Turns the super nailgun receiver about its seat (looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_snail_roll, "Receiver Roll", -90.f, 90.f, 1.f, "%.0f", ext(-180.f, 180.f),
    "Turns the super nailgun receiver about its seat (looks only).")
QVR_SLIDER_BYNAME(vr_reload_hold_snail_x, "Hand X", -6.f, 6.f, 0.1f, "%.1f", ext(),
    "The other hand holding the attached magazine is drawn moved forward (units, the gun's aim; mirrored for the main hand). Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_snail_y, "Hand Y", -6.f, 6.f, 0.1f, "%.1f", ext(),
    "The other hand holding the attached magazine is drawn moved left (units, the gun's aim; mirrored for the main hand). Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_snail_z, "Hand Z", -6.f, 6.f, 0.1f, "%.1f", ext(),
    "The other hand holding the attached magazine is drawn moved up (units, the gun's aim; mirrored for the main hand). Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_snail_pitch, "Hand Pitch", -180.f, 180.f, 5.f, "%.0f", noExt,
    "And turned (degrees), once its grip lies along the magazine where it holds it. Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_snail_yaw, "Hand Yaw", -180.f, 180.f, 5.f, "%.0f", noExt,
    "And turned (degrees), once its grip lies along the magazine where it holds it. Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_snail_roll, "Hand Roll", -180.f, 180.f, 5.f, "%.0f", noExt,
    "And turned (degrees), once its grip lies along the magazine where it holds it. Looks only.")
QVR_TOGGLE_BYNAME(vr_reload_hold_snail_fingers, "Fingers Set by Hand",
    "Off: the fingers close round the magazine as it is. On: each finger at its curl below.")
QVR_SLIDER_BYNAME(vr_reload_hold_snail_curl_thumb, "Thumb Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_snail_curl_index, "Index Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_snail_curl_middle, "Middle Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_snail_curl_ring, "Ring Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_snail_curl_pinky, "Little Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")

// vr_menu_pages.inc: pageReloadThunderbolt
QVR_TOGGLE_BYNAME(vr_reload_pouch_light, "Reload at the Pouch",
    "With its cell out, bring its empty well down onto the cells standing in the ammo pouch, along them "
    "(Reloading: Pouch Angle): a fresh cell seats, from your cells. With one in, nothing.")
QVR_SLIDER_BYNAME(vr_reload_port_light_x, "Well X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the thunderbolt well's load point forward (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_light_y, "Well Y", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the thunderbolt well's load point left (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_light_z, "Well Z", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the thunderbolt well's load point up (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_light_radius, "Well Radius", 0.f, 10.f, 0.25f, "%.2f units", ext(0.f, 40.f),
    "How near the thunderbolt well's load point a held round must come to go in (a magazine: its reference point, within this plus its own radius).")
QVR_SLIDER_BYNAME(vr_reload_mag_light_x, "Top X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the thunderbolt cell's reference point (its top, its feed end) along its gun (its model units).")
QVR_SLIDER_BYNAME(vr_reload_mag_light_y, "Top Y", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the thunderbolt cell's reference point (its top, its feed end) to its left (its model units).")
QVR_SLIDER_BYNAME(vr_reload_mag_light_z, "Top Z", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the thunderbolt cell's reference point (its top, its feed end) up its length (its model units).")
QVR_SLIDER_BYNAME(vr_reload_mag_light_radius, "Top Radius", 0.f, 6.f, 0.25f, "%.2f units", ext(0.f, 20.f),
    "The thunderbolt cell's own radius round its reference point (added to the well's).")
QVR_SLIDER_BYNAME(vr_reload_battery_sparks, "Contact Sparks", 0.f, 40.f, 1.f, "%.0f", ext(0.f, 200.f),
    "Sparks a cell throws as it makes or breaks contact: seated in the gun, or taken out (0: none).")
QVR_SLIDER_BYNAME(vr_reload_battery_smoke_time, "Spent Cell Smoke", 0.f, 20.f, 0.5f, "%.1f s", ext(0.f, 120.f),
    "Seconds a spent cell (taken out empty) smokes from its top, in the hand or lying about; a spent "
    "magazine of lava nails too (0: never).")
QVR_SLIDER_BYNAME(vr_reload_well_light_x, "Receiver X", -6.f, 6.f, 0.05f, "%.2f", ext(),
    "Moves the thunderbolt receiver (the collar round the magazine's top, drawn on the gun) forward, to sit flush (the gun's model units; looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_light_y, "Receiver Y", -6.f, 6.f, 0.05f, "%.2f", ext(),
    "Moves the thunderbolt receiver (the collar round the magazine's top, drawn on the gun) left, to sit flush (the gun's model units; looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_light_z, "Receiver Z", -6.f, 6.f, 0.05f, "%.2f", ext(),
    "Moves the thunderbolt receiver (the collar round the magazine's top, drawn on the gun) up, to sit flush (the gun's model units; looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_light_pitch, "Receiver Pitch", -90.f, 90.f, 1.f, "%.0f", ext(-180.f, 180.f),
    "Turns the thunderbolt receiver about its seat (looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_light_yaw, "Receiver Yaw", -90.f, 90.f, 1.f, "%.0f", ext(-180.f, 180.f),
    "Turns the thunderbolt receiver about its seat (looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_light_roll, "Receiver Roll", -90.f, 90.f, 1.f, "%.0f", ext(-180.f, 180.f),
    "Turns the thunderbolt receiver about its seat (looks only).")
QVR_SLIDER_BYNAME(vr_reload_hold_light_x, "Hand X", -6.f, 6.f, 0.1f, "%.1f", ext(),
    "The other hand holding the attached cell is drawn moved forward (units, the gun's aim; mirrored for the main hand). Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_light_y, "Hand Y", -6.f, 6.f, 0.1f, "%.1f", ext(),
    "The other hand holding the attached cell is drawn moved left (units, the gun's aim; mirrored for the main hand). Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_light_z, "Hand Z", -6.f, 6.f, 0.1f, "%.1f", ext(),
    "The other hand holding the attached cell is drawn moved up (units, the gun's aim; mirrored for the main hand). Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_light_pitch, "Hand Pitch", -180.f, 180.f, 5.f, "%.0f", noExt,
    "And turned (degrees), once its grip lies along the cell where it holds it. Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_light_yaw, "Hand Yaw", -180.f, 180.f, 5.f, "%.0f", noExt,
    "And turned (degrees), once its grip lies along the cell where it holds it. Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_light_roll, "Hand Roll", -180.f, 180.f, 5.f, "%.0f", noExt,
    "And turned (degrees), once its grip lies along the cell where it holds it. Looks only.")
QVR_TOGGLE_BYNAME(vr_reload_hold_light_fingers, "Fingers Set by Hand",
    "Off: the fingers close round the cell as it is. On: each finger at its curl below.")
QVR_SLIDER_BYNAME(vr_reload_hold_light_curl_thumb, "Thumb Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_light_curl_index, "Index Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_light_curl_middle, "Middle Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_light_curl_ring, "Ring Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_light_curl_pinky, "Little Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")

// vr_menu_pages.inc: pageReloadRevolver
QVR_TOGGLE_BYNAME(vr_revolver_open_flick, "Flick to Open",
    "A flick of the wrist towards the cylinder's side (to the left in the right hand, to the right in the left) "
    "swings it out.")
QVR_SLIDER_BYNAME(vr_revolver_open_flick_speed, "Flick Open Speed", 50.f, 2000.f, 25.f, "%.0f deg/s", ext(0.f, 10000.f),
    "The wrist's roll speed needed to flick it open: how fast the wrist must roll that way, at its fastest.")
QVR_SLIDER_BYNAME(vr_revolver_open_flick_angle, "Flick Open Wrist Flick", 5.f, 120.f, 5.f, "%.0f deg", ext(0.f, 360.f), "And how far it must roll that way in one go.")
QVR_SLIDER_BYNAME(vr_revolver_open_flick_time, "Flick Open Time", 0.05f, 1.f, 0.05f, "%.2f s", ext(0.f, 5.f),
    "The wrist flick must be rolled within this long of the flick's start; a slower roll starts over (0: no limit).")
QVR_TOGGLE_BYNAME(vr_revolver_open_button, "Eject Button Opens",
    "B/Y swings it out; out, B/Y pushes the ejector (its shells drop).")
QVR_TOGGLE_BYNAME(vr_revolver_eject_rod, "Ejector Rod",
    "Out, the other hand pushing the ejector rod's head (in front of the cylinder) back drops them.")
QVR_SLIDER_BYNAME(vr_revolver_eject_rod_reach, "Ejector Rod Reach", 0.f, 8.f, 0.25f, "%.2f units", ext(0.f, 40.f), "How near its head the other hand must be.")
QVR_SLIDER_BYNAME(vr_revolver_eject_rod_speed, "Ejector Rod Push", 0.f, 3.f, 0.05f, "%.2f m/s", ext(0.f, 10.f), "How fast it must push back along the cylinder.")
QVR_TOGGLE_BYNAME(vr_revolver_spin, "Spin by Hand",
    "Out, the other hand swiping across the cylinder spins it: it slows and stops on a chamber, which changes "
    "which chamber comes under the hammer next.")
QVR_SLIDER_BYNAME(vr_revolver_spin_speed, "Swipe Speed", 0.f, 3.f, 0.05f, "%.2f m/s", ext(0.f, 10.f), "How fast a swipe must cross it.")
QVR_SLIDER_BYNAME(vr_revolver_spin_hold, "Wait After Loading", 0.f, 2.f, 0.05f, "%.2f s", ext(0.f, 10.f),
    "No spin while the other hand holds a shell (or anything), nor this long after it let go or the shell "
    "went in: loading a chamber doesn't spin it by accident.")
QVR_SLIDER_BYNAME(vr_revolver_close_flick_speed, "Flick Close Speed", 50.f, 2000.f, 25.f, "%.0f deg/s", ext(0.f, 10000.f),
    "Always on: a flick of the wrist the other way swings it home. The wrist's roll speed needed to flick it home, at its fastest.")
QVR_SLIDER_BYNAME(vr_revolver_close_flick_angle, "Flick Close Wrist Flick", 5.f, 120.f, 5.f, "%.0f deg", ext(0.f, 360.f), "And how far it must roll that way in one go.")
QVR_TOGGLE_BYNAME(vr_revolver_close_push, "Push Home",
    "The other hand pushing the swung-out cylinder up from below swings it home.")
QVR_TOGGLE_BYNAME(vr_revolver_close_button, "Button Closes",
    "Out, B/Y swings it home instead of dropping its shells (an accessibility option: the shells then drop "
    "by the muzzle up or the ejector rod alone).")
QVR_SLIDER_BYNAME(vr_reload_port_revolver_x, "Load Point X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves its load point (the swung-out cylinder's back face's middle) forward (the gun's model units).")
QVR_SLIDER_BYNAME(vr_reload_port_revolver_y, "Load Point Y", -10.f, 10.f, 0.1f, "%.1f", ext(), "Moves it left (the gun's model units).")
QVR_SLIDER_BYNAME(vr_reload_port_revolver_z, "Load Point Z", -10.f, 10.f, 0.1f, "%.1f", ext(), "Moves it up (the gun's model units).")
QVR_SLIDER_BYNAME(vr_reload_port_revolver_radius, "Load Point Radius", 0.f, 10.f, 0.25f, "%.2f units", ext(0.f, 40.f),
    "How near it a held shell must come to go in (into the empty chamber nearest it).")

// vr_menu_pages.inc: pageReloadNailPistol
QVR_TOGGLE_BYNAME(vr_reload_pouch_npistol, "Reload at the Pouch",
    "With its magazine out, bring its empty grip down onto the magazines standing in the ammo pouch, along them "
    "(Reloading: Pouch Angle): a fresh magazine seats, from your nails. With one in, nothing.")
QVR_SLIDER_BYNAME(vr_reload_port_npistol_x, "Well X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the nail pistol's well's load point forward (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_npistol_y, "Well Y", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the nail pistol's well's load point left (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_npistol_z, "Well Z", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the nail pistol's well's load point up (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_npistol_radius, "Well Radius", 0.f, 10.f, 0.25f, "%.2f units", ext(0.f, 40.f),
    "How near the nail pistol's well's load point a held round must come to go in (a magazine: its reference point, within this plus its own radius).")
QVR_SLIDER_BYNAME(vr_reload_mag_npistol_x, "Top X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the nail pistol's magazine's reference point (its top, its feed end) along its gun (its model units).")
QVR_SLIDER_BYNAME(vr_reload_mag_npistol_y, "Top Y", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the nail pistol's magazine's reference point (its top, its feed end) to its left (its model units).")
QVR_SLIDER_BYNAME(vr_reload_mag_npistol_z, "Top Z", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the nail pistol's magazine's reference point (its top, its feed end) up its length (its model units).")
QVR_SLIDER_BYNAME(vr_reload_mag_npistol_radius, "Top Radius", 0.f, 6.f, 0.25f, "%.2f units", ext(0.f, 20.f),
    "The nail pistol's magazine's own radius round its reference point (added to the well's).")
QVR_SLIDER_BYNAME(vr_reload_hold_npistol_x, "Hand X", -6.f, 6.f, 0.1f, "%.1f", ext(),
    "The other hand holding the attached magazine is drawn moved forward (units, the gun's aim; mirrored for the main hand). Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_npistol_y, "Hand Y", -6.f, 6.f, 0.1f, "%.1f", ext(),
    "The other hand holding the attached magazine is drawn moved left (units, the gun's aim; mirrored for the main hand). Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_npistol_z, "Hand Z", -6.f, 6.f, 0.1f, "%.1f", ext(),
    "The other hand holding the attached magazine is drawn moved up (units, the gun's aim; mirrored for the main hand). Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_npistol_pitch, "Hand Pitch", -180.f, 180.f, 5.f, "%.0f", noExt,
    "And turned (degrees), once its grip lies along the magazine where it holds it. Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_npistol_yaw, "Hand Yaw", -180.f, 180.f, 5.f, "%.0f", noExt,
    "And turned (degrees), once its grip lies along the magazine where it holds it. Looks only.")
QVR_SLIDER_BYNAME(vr_reload_hold_npistol_roll, "Hand Roll", -180.f, 180.f, 5.f, "%.0f", noExt,
    "And turned (degrees), once its grip lies along the magazine where it holds it. Looks only.")
QVR_TOGGLE_BYNAME(vr_reload_hold_npistol_fingers, "Fingers Set by Hand",
    "Off: the fingers close round the magazine as it is. On: each finger at its curl below.")
QVR_SLIDER_BYNAME(vr_reload_hold_npistol_curl_thumb, "Thumb Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_npistol_curl_index, "Index Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_npistol_curl_middle, "Middle Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_npistol_curl_ring, "Ring Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")
QVR_SLIDER_BYNAME(vr_reload_hold_npistol_curl_pinky, "Little Curl", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 open, 1 closed (Fingers Set by Hand).")

// vr_menu_pages.inc: pageReloadWidowmaker
QVR_SLIDER_BYNAME(vr_ad_widowmaker_motor, "Motor Volume", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The barrels' motor whine: it rises in pitch and volume as they spin up and dies as they coast down. "
    "0: silent.")
QVR_SLIDER_BYNAME(vr_reload_port_widow_x, "Port X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the Widowmaker port's load point forward (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_widow_y, "Port Y", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the Widowmaker port's load point left (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_widow_z, "Port Z", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the Widowmaker port's load point up (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_widow_radius, "Port Radius", 0.f, 10.f, 0.25f, "%.2f units", ext(0.f, 40.f),
    "How near the Widowmaker port's load point a held handful's middle must come to go in.")

// vr_menu_pages.inc: pageReloadPlasmaGun
QVR_SLIDER_BYNAME(vr_reload_port_adplasma_x, "Well X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the Plasma Gun well's load point forward (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_adplasma_y, "Well Y", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the Plasma Gun well's load point left (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_adplasma_z, "Well Z", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the Plasma Gun well's load point up (the gun's model units; negative: the other way).")
QVR_SLIDER_BYNAME(vr_reload_port_adplasma_radius, "Well Radius", 0.f, 10.f, 0.25f, "%.2f units", ext(0.f, 40.f),
    "How near the Plasma Gun well's load point a held cell's reference point must come to go in (within this plus the cell's own radius).")
QVR_SLIDER_BYNAME(vr_reload_well_adplasma_x, "Receiver X", -6.f, 6.f, 0.05f, "%.2f", ext(),
    "Moves the Plasma Gun's cell collar (drawn on the gun) forward (the gun's model units; looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_adplasma_y, "Receiver Y", -6.f, 6.f, 0.05f, "%.2f", ext(),
    "Moves the Plasma Gun's cell collar left (the gun's model units; looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_adplasma_z, "Receiver Z", -6.f, 6.f, 0.05f, "%.2f", ext(),
    "Moves the Plasma Gun's cell collar up (the gun's model units; looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_adplasma_pitch, "Receiver Pitch", -90.f, 90.f, 1.f, "%.0f", ext(-180.f, 180.f),
    "Turns the Plasma Gun's cell collar about its seat (looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_adplasma_yaw, "Receiver Yaw", -90.f, 90.f, 1.f, "%.0f", ext(-180.f, 180.f),
    "Turns the Plasma Gun's cell collar about its seat (looks only).")
QVR_SLIDER_BYNAME(vr_reload_well_adplasma_roll, "Receiver Roll", -90.f, 90.f, 1.f, "%.0f", ext(-180.f, 180.f),
    "Turns the Plasma Gun's cell collar about its seat (looks only).")

// vr_menu_pages.inc: pageReloadLaunchers
QVR_TOGGLE_BYNAME(vr_reload_front, "Load at the Muzzle",
    "Immersive: the grenade, proximity and rocket launchers take a round at a time from the ammo pouch (a "
    "grenade, a proximity grenade, a rocket; in their other ammo mode the multi-grenade or multi-rocket), "
    "put butt first into the muzzle, or thrown in, or the launcher brought to one lying about. Off: they "
    "reload at the hip holsters.")
QVR_SLIDER_BYNAME(vr_reload_front_angle, "Round Angle", 5.f, 80.f, 5.f, "%.0f deg", noExt,
    "How far off the barrel a rocket or grenade may lie and still go in, butt first (its nose out of the "
    "muzzle). The wrong way round never loads: a dull tap. A proximity grenade goes in any way round.")
QVR_SLIDER_BYNAME(vr_reload_front_hold_pitch, "Round In Hand Pitch", -180.f, 180.f, 5.f, "%.0f deg", noExt,
    "A rocket or grenade (any of them: multi, proximity) taken from the ammo pouch, the same way every time: its "
    "nose tipped up in your hand (negative: down), about its middle. 90: standing in the fist, nose up by the "
    "thumb, butt down by the little finger (thumb forward, it lies along the barrel, butt to the muzzle).")
QVR_SLIDER_BYNAME(vr_prox_scale, "Proximity Mine Size", 0.3f, 1.f, 0.01f, "%.2f", ext(0.2f, 2.f),
    "The proximity grenade's own size, the same everywhere (in the hand, the pouches, its launcher, in flight "
    "and stuck), its hit boxes with it. 1: Quake's size; the other grenades' 0.78.")
QVR_SLIDER_BYNAME(vr_nail_scale, "Nail Size in Flight", 0.4f, 1.f, 0.01f, "%.2f", ext(0.25f, 2.f),
    "Every nail in flight (the nailguns', the nail pistol's, the monsters' and the traps'), as drawn: they "
    "hit as a point either way. 1: Quake's size.")
QVR_SLIDER_BYNAME(vr_reload_port_gl_x, "Muzzle X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the grenade launcher's load point forward (the gun's model units; negative: back into the barrel). "
    "The multi-grenade launcher's too.")
QVR_SLIDER_BYNAME(vr_reload_port_gl_y, "Muzzle Y", -10.f, 10.f, 0.1f, "%.1f", ext(), "Left (negative: right).")
QVR_SLIDER_BYNAME(vr_reload_port_gl_z, "Muzzle Z", -10.f, 10.f, 0.1f, "%.1f", ext(), "Up (negative: down).")
QVR_SLIDER_BYNAME(vr_reload_port_gl_radius, "Muzzle Radius", 0.f, 10.f, 0.25f, "%.2f units", ext(0.f, 40.f),
    "How near the muzzle's load point a grenade's butt must come to go in.")
QVR_SLIDER_BYNAME(vr_reload_port_prox_x, "Muzzle X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the proximity launcher's load point forward (the gun's model units; negative: back).")
QVR_SLIDER_BYNAME(vr_reload_port_prox_y, "Muzzle Y", -10.f, 10.f, 0.1f, "%.1f", ext(), "Left (negative: right).")
QVR_SLIDER_BYNAME(vr_reload_port_prox_z, "Muzzle Z", -10.f, 10.f, 0.1f, "%.1f", ext(), "Up (negative: down).")
QVR_SLIDER_BYNAME(vr_reload_port_prox_radius, "Muzzle Radius", 0.f, 10.f, 0.25f, "%.2f units", ext(0.f, 40.f),
    "How near the muzzle's load point a proximity grenade's middle must come to go in.")
QVR_SLIDER_BYNAME(vr_reload_port_rl_x, "Muzzle X", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "Moves the rocket launcher's load point forward (the gun's model units; negative: back). The "
    "multi-rocket launcher's too.")
QVR_SLIDER_BYNAME(vr_reload_port_rl_y, "Muzzle Y", -10.f, 10.f, 0.1f, "%.1f", ext(), "Left (negative: right).")
QVR_SLIDER_BYNAME(vr_reload_port_rl_z, "Muzzle Z", -10.f, 10.f, 0.1f, "%.1f", ext(), "Up (negative: down).")
QVR_SLIDER_BYNAME(vr_reload_port_rl_radius, "Muzzle Radius", 0.f, 10.f, 0.25f, "%.2f units", ext(0.f, 40.f),
    "How near the muzzle's load point a rocket's butt must come to go in.")

// vr_menu_pages.inc: pageReloading
QVR_TOGGLE_BYNAME(vr_cellcord_laser, "Laser Cannon Cord",
    "The laser cannon held, a coiled cord (the flashlight's Coiled) from its bottom into a cell in the ammo "
    "pouch, as if it drew on the pouch's cells (the pouch gives and shows cells while it is held). No cells "
    "left: the cord comes off and hangs loose off the gun; pick up cells and it plugs back in. A laser "
    "cannon in each hand: two cords. Needs Reloading Immersive (the pouch).")
QVR_TOGGLE_BYNAME(vr_cellcord_hammer, "Mjolnir Cord",
    "The same cord on Mjolnir (Hipnotic's hammer), from the end of its handle: it draws cells too.")
QVR_TOGGLE_BYNAME(vr_cellcord_superaxe, "Super Axe Cord",
    "The same cord on the Super Axe (MG3), from the end of its handle: it draws cells too.")
QVR_SLIDER_BYNAME(vr_reload_reinsert_clearance, "Magazine Reinsertion Clearance", 0.f, 16.f, 0.25f, "%.2f units", ext(0.f, 64.f),
    "After pulling or knocking a magazine out, withdraw this far beyond its insertion reach before it can "
    "go back in. Also applies to the new magazine that knocked it out. Smaller: a shorter out-and-back "
    "motion; 0: just outside the reach. Loose magazines also clear the collision leniency and eject cooldown.")
QVR_SLIDER_BYNAME(vr_reload_reinsert_time, "Magazine Reinsertion Delay", 0.f, 1.f, 0.05f, "%.2f s", ext(0.f, 5.f),
    "Or this long after a held magazine was pulled out, or knocked the old one out, it can go back in "
    "even if it never left the well by the clearance: kept close, it seats after a moment, never at "
    "once. Larger: more time to move it away; 0: by the clearance only. Loose magazines keep the eject "
    "cooldown and the clearance.")
QVR_TOGGLE_BYNAME(vr_reload_mag_release, "Release Seated Magazine",
    "On: a magazine you push into its well leaves your hand as it seats, so moving the hand on to the "
    "foregrip while still gripping never pulls it back out; to pull it out again, let go and grip it "
    "afresh. Off: the hand still gripping holds the seated magazine, so pulling it straight back out "
    "works without letting go.")
QVR_SLIDER_BYNAME(vr_reload_insert_time, "Shell Slide In", 0.f, 0.5f, 0.01f, "%.2f s", noExt,
    "A shell (the super shotgun's pair) let go of at a gun's load point is drawn sliding into it for this "
    "long, following the gun (0: gone at once). It is loaded at once all the same.")
QVR_SLIDER_BYNAME(vr_reload_collide_leniency, "Collision Leniency", 0.f, 60.f, 1.f, "%.0f cm", ext(0.f, 200.f),
    "A shell or magazine in your hand is drawn kept off the gun in the other hand (Held Things Collide): it "
    "may go this far into the gun's box first, to reach the port under the shotgun or a magazine well.")
QVR_TOGGLE_BYNAME(vr_reload_mag_collide, "Solid Magazines",
    "A magazine seated in a gun is solid as the gun is: the other hand stops at it, empty or holding "
    "something, the other gun meets it, and it pushes things about (a gun lying about too). Grabbing it "
    "still holds it and pulls it out. Off: they pass through it.")
QVR_TOGGLE_BYNAME(vr_reload_contact, "Load Loose Rounds",
    "A shell or magazine lying about, dropped or thrown goes into the gun it touches at its load point, lying "
    "the right way: drop shells into the shotgun's port or the open super shotgun's barrels, a magazine "
    "feed end up into its well, or bring the gun down onto one standing on a table.")
QVR_SLIDER_BYNAME(vr_reload_contact_leniency, "Loose Leniency", 0.f, 20.f, 1.f, "%.0f cm", ext(0.f, 60.f),
    "A loose round goes in this much further from the load point than one in your hand (the gun's "
    "collision covers its port and well), and passes through the gun within this much more again.")
QVR_SLIDER_BYNAME(vr_reload_eject_cooldown, "Ejected Stays Out", 0.f, 2.f, 0.05f, "%.2f s", noExt,
    "A magazine just ejected (or the super shotgun's live shells thrown out) goes back in by contact only "
    "once it has left both guns' wells and this long has passed.")
QVR_TOGGLE_BYNAME(vr_reload_eject_button, "Eject Button",
    "The B/Y button of the controller holding a nailgun, super nailgun or thunderbolt drops its magazine.")
QVR_TOGGLE_BYNAME(vr_reload_pouch_mag_slots, "Magazines Where Drawn",
    "The nailguns' and thunderbolt's well against each magazine as drawn in the pouch (the nearest one): "
    "Magazine Contact and Magazine Angle. Off: anywhere along the row of rounds, by Pouch Contact and Pouch "
    "Angle.")
QVR_TOGGLE_BYNAME(vr_ammo_pouch_show, "Show", "Draw the pouch (off: it still works where it is).")
QVR_SLIDER_BYNAME(vr_ammo_pouch_x, "X", -10.f, 10.f, 0.25f, "%.2f", ext(),
    "Forward (negative: back, round the hips). With the body drawn, 0 is on the belly.")
QVR_SLIDER_BYNAME(vr_ammo_pouch_y, "Y", -10.f, 10.f, 0.25f, "%.2f", ext(), "To the right (negative: left).")
QVR_SLIDER_BYNAME(vr_ammo_pouch_z, "Z", -10.f, 10.f, 0.25f, "%.2f", ext(), "Up, from the hip holsters' height.")
QVR_SLIDER_BYNAME(vr_ammo_pouch_thresh, "Reach", 0.5f, 20.f, 0.25f, "%.2f", ext(0.f, 40.f),
    "How near a hand must come to reach into it. The hip holsters beside it win where a hand is nearer them.")
QVR_SLIDER_BYNAME(vr_ammo_pouch_pitch, "Pitch", -180.f, 180.f, 5.f, "%.0f", noExt, "Its top tipped away from the body.")
QVR_SLIDER_BYNAME(vr_ammo_pouch_yaw, "Yaw", -180.f, 180.f, 5.f, "%.0f", noExt, nullptr)
QVR_SLIDER_BYNAME(vr_ammo_pouch_roll, "Roll", -180.f, 180.f, 5.f, "%.0f", noExt, nullptr)
QVR_SLIDER_BYNAME(vr_ammo_pouch_scale, "Size", 0.2f, 3.f, 0.05f, "%.2fx", ext(0.05f, 10.f), "As drawn (its reach is Reach's).")
QVR_TOGGLE_BYNAME(vr_ammo_pouch_counter, "Counter",
    "A small screen on the pouch: how many of what it gives (shells, nails, cells) you have left.")
QVR_TOGGLE_BYNAME(vr_ammo_screen_text_white, "White Counter Text",
    "The counter's number near-white, as the wrist gadget's (the guns' ammo screens' too: the same setting).")
QVR_SLIDER_BYNAME(vr_ammo_pouch_leg_follow, "Follow Legs", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "With the full body, how much the pouch moves with your legs as you walk, as the belt does (the hip "
    "holsters' Follow Legs likewise). 0: fixed on the belly.")
QVR_TOGGLE_BYNAME(vr_ammo_pouch_wobble_each, "Wobble Each Round",
    "Each shell or magazine sways on its own (a little quicker or slower than the others; lying shells "
    "slide and tilt), never through each other. Off: they all sway together, as one block.")

// vr_menu_pages.inc: pageGraphics
QVR_CYCLE_BYNAME(vid_fsaa, "Anti-aliasing",
    "Multisampling: smoother edges; thin lines such as the gaps between planks stop crawling as your head moves. 4x (as shipped) does most of it; 8x takes nearly twice 4x's GPU time for little more.",
    {0.f, "Off"}, {2.f, "2x"}, {4.f, "4x"}, {8.f, "8x"})
QVR_CYCLE_BYNAME(vr_texture_smooth, "Smooth Textures",
    "Smooth filtering for high-resolution replacement textures, or for all textures.",
    {0.f, "As gl_texturemode"}, {1.f, "Replacements"}, {2.f, "All"})
QVR_CYCLE_BYNAME(gl_texture_anisotropy, "Anisotropic Filtering",
    "Keeps floors and walls seen at a glancing angle sharp instead of blurry (smooth-filtered textures). "
    "Little GPU time; 16x is the sharpest. Capped at what the graphics card supports.",
    {1.f, "Off"}, {2.f, "2x"}, {4.f, "4x"}, {8.f, "8x"}, {16.f, "16x"})
QVR_TOGGLE_BYNAME(vr_alpha_coverage, "Fence Coverage",
    "Fences and grates keep their bars at a distance instead of thinning out and crawling; with Anti-aliasing (MSAA) their edges are smoothed too.")
QVR_TOGGLE_BYNAME(vr_relit_maps, "Relit Maps",
    "Maps re-lit with shadows, ambient occlusion and colour, if made (Misc/quakevr/relight_maps.py, or Relighting below). Next map.")
QVR_SLIDER_BYNAME(vr_gamma, "Headset Gamma", 0.5f, 1.5f, 0.02f, "%.2f", ext(),
    "The headset's brightness: lower is brighter (1 neutral). The desktop's gamma is not used.")
QVR_SLIDER_BYNAME(vr_contrast, "Headset Contrast", 0.5f, 2.f, 0.02f, "%.2f", ext(), "The headset's own contrast (1 neutral).")
QVR_CYCLE_BYNAME(vr_profile, "Performance Profile",
    "Record CPU and GPU time per effect into quakevr/profile/ every 5 seconds (and show the profiler's panel over the wrist: Debug > Profiling has more). Not kept after a restart.",
    {0.f, "Off"}, {1.f, "Record"}, {2.f, "Record and show"})
QVR_CYCLE_BYNAME(vr_gadget_fps, "FPS Counter on the Gadget",
    "Under the wrist gadget: the frame rate and the CPU's and GPU's milliseconds a frame (our work, the eyes' drawing; not the runtime's waits). Basic: one line, averaged over half a second. Detailed: now, the last quarter second's average, the last 5 seconds' least and most, the frames that missed a refresh (Late), and graphs of the last 3 seconds against the frame budget (the dotted line); figures over the budget are highlighted.",
    {0.f, "Off"}, {1.f, "Basic"}, {2.f, "Detailed"})

// vr_menu_pages.inc: pageGraphicsLights
QVR_SLIDER_BYNAME(vr_light_contrast, "Light Contrast", 1.f, 2.5f, 0.02f, "%.2f", ext(),
    "Darker shade, lamps as bright: the level's own light sources carry the scene (1: Quake's).")
QVR_SLIDER_BYNAME(vr_ambient_light, "Ambient Light", 0.f, 0.4f, 0.01f, "%.2f", ext(),
    "The room's own fill light: how much light the places no lamp reaches have, so a map reads clearly lit without the flashlight. A companion to Light Contrast, which still shapes the shade at any level of it (0: none).")
QVR_TOGGLE_BYNAME(vr_colored_lights, "Coloured Lights",
    "Warm muzzle flashes, rockets and explosions; the lightning gun's blue.")
QVR_CYCLE_BYNAME(vr_dlight_falloff, "Dynamic Light Falloff",
    "DarkPlaces: strong lights, full near and smoothly gone at the edge. Quake: dimmer, fading linearly all the way.",
    {0.f, "Quake"}, {1.f, "DarkPlaces"})
QVR_TOGGLE_BYNAME(vr_dlight_uncapped, "Uncapped Dynamic Lights",
    "Dynamic lights add fully to bright walls (Quake caps them).")
QVR_TOGGLE_BYNAME(vr_dlight_models, "Dynamic Lights on Models",
    "Per pixel, by the angle, and shadowed (off: Quake's flat brightening).")
QVR_SLIDER_BYNAME(vr_dlight_angle, "Dynamic Light Angle", 0.f, 1.f, 0.25f, "%.2f", noExt,
    "How much surfaces facing away from a dynamic light get less of it (0: Quake's).")
QVR_SLIDER_BYNAME(vr_flash_scale, "Muzzle Flash Light", 0.f, 3.f, 0.05f, "%.2fx", ext(), "How far your shots light up the room (0 off).")
QVR_SLIDER_BYNAME(vr_explosion_light_scale, "Explosion Light", 0.f, 3.f, 0.05f, "%.2fx", ext(), nullptr)
QVR_SLIDER_BYNAME(vr_projectile_lights, "Projectile Light", 0.f, 2.f, 0.05f, "%.2fx", ext(),
    "Hell knight flames, scrag spit, vore balls and lasers light up the room, and where they hit (0 off).")
QVR_SLIDER_BYNAME(vr_lavanail_lights, "Lava Nail Lights", 0.f, 16.f, 1.f, "%.0f", ext(),
    "How many lava nails light up the room at once, the nearest; sized by Projectile Light (0 none).")
QVR_SLIDER_BYNAME(vr_lavagun_light, "Lava Gun Light", 0.f, 3.f, 0.1f, "%.1fx", ext(),
    "The nailguns loaded with lava nails glow from their barrels, lighting your hand and what is near (0 off).")
QVR_SLIDER_BYNAME(vr_lavagun_light_radius, "Lava Gun Light Reach", 16.f, 200.f, 4.f, "%.0f", ext(), "How far the lava guns' glow reaches.")
QVR_SLIDER_BYNAME(vr_lavagun_light_flicker, "Lava Gun Light Flicker", 0.f, 1.f, 0.05f, "%.2f", noExt, "How much the lava guns' glow flickers (0 steady).")
QVR_SLIDER_BYNAME(vr_lavagun_light_idle, "Lava Gun Light at Rest", 0.f, 1.f, 0.05f, "%.2fx", noExt,
    "Lava guns in your holsters or lying on the floor glow too, this much of a held one's (0 not).")
QVR_SLIDER_BYNAME(vr_beam_lights, "Lightning Beam Lights", 0.f, 12.f, 1.f, "%.0f", ext(),
    "Lightning beams light the room along their length with this many flickering lights (0 off).")
QVR_SLIDER_BYNAME(vr_flame_lights, "Flamethrower Lights", 0.f, 8.f, 1.f, "%.0f", ext(),
    "Flamethrowers' streams (your pyro gun, pyro monsters) light the room: their flames gathered into at most this many flickering lights, all streams together (0 off).")
QVR_SLIDER_BYNAME(vr_flame_light_scale, "Flamethrower Light Size", 0.f, 3.f, 0.05f, "%.2fx", ext(), "How far the flamethrowers' light reaches.")
QVR_SLIDER_BYNAME(vr_beam_arcs, "Lightning Arcs", 0.f, 3.f, 0.1f, "%.1fx", ext(),
    "Quad Damage's electric arcs crackle along the lightning's beams (the lightning gun, shamblers, Chthon): how many (1: one every 24 units or so; 0 off).")
QVR_SLIDER_BYNAME(vr_beam_arcs_spread, "Lightning Arc Spread", 2.f, 32.f, 1.f, "%.0f", ext(), "How far the arcs reach from the beam, units.")
QVR_SLIDER_BYNAME(vr_beam_arcs_width, "Lightning Arc Width", 0.5f, 4.f, 0.1f, "%.1fx", ext(), "How thick the arcs are, times Quad Damage's on the arms.")
QVR_SLIDER_BYNAME(vr_torch_lights, "Torch Lights", 0.f, 16.f, 1.f, "%.0f", ext(),
    "Torches and flames flicker a small warm light onto the room: how many at once, the nearest (0 off).")
QVR_SLIDER_BYNAME(vr_torch_light_scale, "Torch Light Brightness", 0.f, 16.f, 0.25f, "%.2fx", ext(),
    "How bright the torches' flickering light is, on top of the map's own light.")
QVR_SLIDER_BYNAME(vr_torch_light_shadows, "Torch Light Shadows", 0.f, 4.f, 1.f, "%.0f", ext(),
    "How many of the nearest torches cast shadows (each takes one of the dynamic lights' shadows; 0 none).")
QVR_SLIDER_BYNAME(vr_weapon_screen_light, "Ammo Screen Light", 0.f, 3.f, 0.1f, "%.1fx", ext(),
    "Your weapons' ammo screens cast a small light in their colour on what is in front of them (0 off).")

// vr_menu_pages.inc: pageGraphicsShadows
QVR_SLIDER_BYNAME(vr_shadow_dlights, "Shadowed Dynamic Lights", 0.f, 12.f, 1.f, "%.0f", ext(),
    "Explosions, rockets and other dynamic lights casting shadows, the most important first (0 off).")
QVR_CYCLE_BYNAME(vr_shadow_dlight_size, "Dynamic Shadow Detail",
    "Largest shadow face size, in texels; farther lights get smaller ones.",
    {128.f, "128"}, {256.f, "256"}, {512.f, "512"}, {1024.f, "1024"})
QVR_TOGGLE_BYNAME(vr_shadow_muzzleflash, "Muzzle Flash Shadows",
    "Your own muzzle flashes cast shadows too (brief; costs more).")
QVR_SLIDER_BYNAME(vr_shadow_maplights, "Map Light Shadows", 0.f, 8.f, 1.f, "%.0f", ext(),
    "Map lights near you casting the shadows of monsters and of you (0 off).")
QVR_CYCLE_BYNAME(vr_shadow_maplight_size, "Map Shadow Detail", "Their shadow face size, in texels.",
    {256.f, "256"}, {512.f, "512"}, {1024.f, "1024"})
QVR_SLIDER_BYNAME(vr_shadow_maplight_strength, "Map Shadow Strength", 0.f, 1.f, 0.05f, "%.2f", noExt, "How much of a map light those shadows take away.")
QVR_CYCLE_BYNAME(vr_shadow_self, "Your Shadow", "What of you casts map lights' shadows.",
    {0.f, "None"}, {1.f, "Body"}, {2.f, "Body and hands"})
QVR_CYCLE_BYNAME(vr_shadow_filter, "Shadow Softness", nullptr,
    {0.f, "Hard (1 tap)"}, {1.f, "Soft (4 taps)"}, {2.f, "Softer (9)"}, {3.f, "Softest (16)"})
QVR_SLIDER_BYNAME(vr_shadow_distance, "Shadow Distance", 256.f, 4096.f, 128.f, "%.0f", ext(), "Lights farther than this from you cast no shadows.")
QVR_CYCLE_BYNAME(vr_shadow_atlas, "Shadow Atlas",
    "Room for all shadows at once: 8192 holds eight lights' shadows at full size (256 MB); smaller fits fewer or halves them (64 MB at 4096).",
    {2048.f, "2048"}, {4096.f, "4096"}, {8192.f, "8192"})
QVR_TOGGLE_BYNAME(vr_shadow_atlas_grow, "Grow Shadow Atlas",
    "The shadow atlases start small (64 MB, plus a row of map light shadows) and grow as the map's lights need them, until the next map: less video memory where few lights are near. Off: their full size at once (352 MB at the defaults).")
QVR_SLIDER_BYNAME(vr_shadow_bias, "Shadow Bias", 0.f, 4.f, 0.25f, "%.2f", ext(),
    "Higher: fewer speckles on lit surfaces; lower: shadows start closer to what casts them.")
QVR_CYCLE_BYNAME(vr_shadow_stats, "Shadow Statistics",
    "Print what is shadowed and its GPU time, once a second, to the console. Each frame: also every shadowed light and its shadow's strength (+ chosen, - fading out), to check shadows fade rather than pop.",
    {0.f, "Off"}, {1.f, "Each Second"}, {2.f, "Each Frame, Per Light"})
QVR_SLIDER_BYNAME(vr_ao_dynamic, "Contact Shadows", 0.f, 2.f, 0.1f, "%.1f", ext(),
    "Monsters, items, gibs and your body darken the floor and walls close to them, and each other: soft shadows where things touch (0 off).")
QVR_SLIDER_BYNAME(vr_ao_dynamic_range, "Contact Shadow Reach", 1.25f, 5.f, 0.25f, "%.2f", ext(),
    "How far round a thing its darkening reaches, in its own sizes.")
QVR_SLIDER_BYNAME(vr_ao_brush, "Door and Lift Shadows", 0.f, 2.f, 0.1f, "%.1f", ext(),
    "Doors, lifts, platforms and the item boxes darken the walls and floors close to them (the map's light never saw them; 0 off).")
QVR_SLIDER_BYNAME(vr_ao_models, "Model Self-Shadowing", 0.f, 2.f, 0.1f, "%.1f", ext(),
    "Monsters, weapons and pickups are darker in their own creases: armpits, under guns, grooves (worked out as each model loads; 0 off).")
QVR_CYCLE_BYNAME(vr_blob_shadows, "Blob Shadows",
    "Dark discs under you and things, a cheap stand-in for shadows. Auto: only where the real shadows (above) do not fall.",
    {0.f, "Off"}, {1.f, "Always"}, {2.f, "Auto"})
QVR_CYCLE_BYNAME(vr_player_shadows, "Blob Shadows: You", nullptr,
    {0.f, "Off"}, {1.f, "Hands"}, {2.f, "Body"}, {3.f, "Both"})
QVR_TOGGLE_BYNAME(vr_entity_shadows, "Blob Shadows: Things",
    "Blob shadows under monsters and items, away from their light.")

// vr_menu_pages.inc: pageGraphicsSurfaces
QVR_SLIDER_BYNAME(vr_specular, "Light Sheen", 0.f, 0.5f, 0.025f, "%.3f", ext(), "Dynamic lights glint off walls and models (0 off).")
QVR_TOGGLE_BYNAME(vr_specular_aa, "Sheen Anti-Aliasing",
    "Distant bumps' sheen and the water's glints stay steady instead of sparkling as your head moves.")
QVR_TOGGLE_BYNAME(vr_normalmaps, "Bump Maps",
    "Bumps made from the textures, shown by the map's light and dynamic lights. Next map.")
QVR_SLIDER_BYNAME(vr_normalmap_strength, "Bump Depth", 0.f, 3.f, 0.05f, "%.2f", ext(), nullptr)
QVR_SLIDER_BYNAME(vr_normalmap_baked, "Bumps in Map Light", 0.f, 2.f, 0.1f, "%.1f", ext(),
    "How much the bumps shade the map's own light, lit from where it seems to come from (0: only dynamic lights show them).")
QVR_TOGGLE_BYNAME(vr_deluxemap, "Real Light Directions",
    "Relit maps know where their light comes from at every point (.lux): the bumps are lit from there and the lamps glint off them. Off: guessed from where the light gets brighter (always so on maps without it).")
QVR_SLIDER_BYNAME(vr_normalmap_models, "Bumps on Models", 0.f, 2.f, 0.1f, "%.1f", ext(),
    "How much the bumps shade models' own light (monsters, pickups, weapons; held ones half), lit from the map's lights (times Bumps in Map Light; 0: only dynamic lights show them).")
QVR_SLIDER_BYNAME(vr_normalmap_authored, "Authored Model Bumps", 0.f, 2.f, 0.1f, "%.1f", ext(),
    "How deep models' authored normal maps are (a skin's _norm or _bump file: a model pack's, or our own models'); 1 as made. Held ones too.")
QVR_TOGGLE_BYNAME(vr_parallax, "Parallax",
    "Walls and floors look carved: their dark parts sink in and shift as you move your head (needs Bump Maps).")
QVR_SLIDER_BYNAME(vr_parallax_depth, "Parallax Depth", 0.5f, 6.f, 0.25f, "%.2f", ext(),
    "How deep the darkest parts of the textures sink, in Quake units.")
QVR_SLIDER_BYNAME(vr_parallax_distance, "Parallax Distance", 128.f, 2048.f, 64.f, "%.0f", ext(), "How far away walls still look carved (further costs more).")
QVR_SLIDER_BYNAME(vr_parallax_refine, "Parallax Refinement", 0.f, 8.f, 1.f, "%.0f", ext(),
    "Steps finding exactly where your view meets the carving: smooth edges seen from the side instead of stairs and layers that slide as you move (0 none; 1 the old look; 4 the default).")
QVR_SLIDER_BYNAME(vr_parallax_grazing, "Parallax Side Fade", 60.f, 90.f, 1.f, "%.0f", ext(),
    "How far round to the side, in degrees from straight on, walls still look carved before it fades out (90, the default: never fades; 86 before; 83 the old look; lower if seen-from-the-side walls swim).")
QVR_TOGGLE_BYNAME(vr_parallax_depth_write, "Parallax Depth Write",
    "The carving has real depth: monsters, items, hands, particles and water meet the sunken parts where they look to be, instead of at the flat wall. Off by default, on in the Ultra preset: costs about a millisecond a frame in big maps.")
QVR_SLIDER_BYNAME(vr_parallax_items, "Parallax Items Depth", 0.f, 4.f, 0.25f, "%.2f", ext(),
    "How deep the ammo and health boxes look carved, in their own units (scaled with the box; 0 off).")
QVR_SLIDER_BYNAME(vr_parallax_models, "Parallax Models Depth", 0.f, 2.f, 0.125f, "%.3f", ext(),
    "How deep monsters, pickups and weapons without baked maps look carved, their heights guessed from the skin's paint, in their own units (0 off, the default: on Quake's blocky skins it bends the texels).")
QVR_SLIDER_BYNAME(vr_parallax_authored, "Parallax Depth: Authored Models", 0.f, 2.f, 0.25f, "%.2f", ext(),
    "How deep models with authored bump maps look carved (your hands and body, the gadget, the flashlight, the weapons): their real baked relief, 1 as deep as baked, 0 off.")
QVR_TOGGLE_BYNAME(vr_extmaps, "External Maps",
    "Quetoo's normal, specular and glow maps for Quake's textures (shipped: quakevr/textures_quetoo, CC BY-SA 4.0; or another pack in vr_extmaps_dir), where its pictures match the textures drawn. Next map.")
QVR_TOGGLE_BYNAME(vr_extmaps_normals, "External Bump Maps",
    "Its normal maps instead of the bumps made from the textures (where it has them).")
QVR_TOGGLE_BYNAME(vr_extmaps_spec, "External Specular Maps",
    "Its specular maps: how much and in what colour each texel glints (Light Sheen), and each material's hardness.")
QVR_SLIDER_BYNAME(vr_extmaps_spec_scale, "External Specular Brightness", 0.f, 8.f, 0.25f, "%.2f", ext(),
    "How bright its specular maps make the sheen (4: about as bright as without, on average).")
QVR_TOGGLE_BYNAME(vr_extmaps_luma, "External Glow Maps", "Its glow maps on textures that have none of their own.")
QVR_SLIDER_BYNAME(vr_extmaps_match, "External Maps: Match", 0.f, 1.f, 0.05f, "%.2f", ext(),
    "How closely the pack's own picture must match the texture drawn for its maps to be used (0: always; a repainted picture's bumps don't fit). Next map.")
QVR_TOGGLE_BYNAME(vr_detail, "Detail Textures",
    "A fine grain of stone, metal, wood or grime over walls and floors close by, where Quake's textures turn into big blurry texels.")
QVR_SLIDER_BYNAME(vr_detail_strength, "Detail Strength", 0.f, 2.f, 0.1f, "%.1f", ext(), "How strongly the detail textures show.")
QVR_SLIDER_BYNAME(vr_detail_distance, "Detail Distance", 64.f, 384.f, 16.f, "%.0f", ext(),
    "How far away, in Quake units, the detail textures still show (they fade in closer than this).")

// vr_menu_pages.inc: pageGraphicsRetro
QVR_TOGGLE(vr_retro, "Retro Textures",
    "High-resolution textures (QRP's, a pack's) drawn in Quake's look: on a coarse grid of blocks, each the average of "
    "the texels under it, with crisp edges that don't crawl as your head moves, optionally in Quake's 256 colours. "
    "Each kind of thing has its own settings below. Not a filter over the image: the textures themselves.")

// vr_menu_pages.inc: pageGraphicsLootBeams
QVR_TOGGLE_BYNAME(vr_lootbeams, "Loot Beams",
    "A soft pillar of light rising from things lying about to pick up, as in Diablo or Path of Exile, so loot stands out among corpses and gibs. Never while held, holstered or flying: it fades in once the thing comes to rest. Which things: below.")
QVR_TOGGLE_BYNAME(vr_lootbeam_weapons, "Weapons",
    "Weapons lying about: dropped, thrown, a map's, an enemy's dropped gun (the expansions' and Arcane Dimensions' too); an enemy's gun that is empty for good only with Spent Guns Stand Out (Enemy Weapons).")
QVR_TOGGLE_BYNAME(vr_lootbeam_mags, "Magazines",
    "Magazines you dropped or pulled out, unless empty or nearly (Magazine Fullness).")
QVR_SLIDER_BYNAME(vr_lootbeam_mag_min, "Magazine Fullness", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "A magazine has a beam only when fuller than this share of its rounds. 0: any magazine not empty.")
QVR_TOGGLE_BYNAME(vr_lootbeam_ammo, "Ammo Boxes", nullptr)
QVR_TOGGLE_BYNAME(vr_lootbeam_health, "Health Boxes", nullptr)
QVR_TOGGLE_BYNAME(vr_lootbeam_armor, "Armour", nullptr)
QVR_TOGGLE_BYNAME(vr_lootbeam_powerups, "Power-ups",
    "Quad damage, the pentagram, the ring, the suit, and the mission packs' power-ups.")
QVR_TOGGLE_BYNAME(vr_lootbeam_keys, "Keys and Runes", nullptr)
QVR_TOGGLE_BYNAME(vr_lootbeam_backpacks, "Backpacks", nullptr)
QVR_SLIDER_BYNAME(vr_lootbeam_opacity, "Opacity", 0.f, 1.f, 0.05f, "%.2f", noExt, "How bright the beams are.")
QVR_SLIDER_BYNAME(vr_lootbeam_length, "Length", 0.2f, 4.f, 0.1f, "%.1f m", noExt, "How high a beam rises from the thing's middle.")
QVR_SLIDER_BYNAME(vr_lootbeam_thickness, "Thickness", 0.01f, 0.3f, 0.01f, "%.2f m", noExt,
    "How wide its bright core is; its soft glow is three times as wide.")
QVR_SLIDER_BYNAME(vr_lootbeam_saturation, "Colourfulness", 0.f, 1.f, 0.05f, "%.2f", noExt, "0: all white.")
QVR_SLIDER_BYNAME(vr_lootbeam_fade_in, "Fade In", 0.f, 2.f, 0.1f, "%.1f s", noExt,
    "How long a beam takes to appear once the thing has come to rest.")
QVR_SLIDER_BYNAME(vr_lootbeam_settle, "Rest Before Showing", 0.f, 2.f, 0.1f, "%.1f s", noExt, "How long a thing must lie still before its beam fades in.")
QVR_SLIDER_BYNAME(vr_lootbeam_near, "Fade Up Close", 0.f, 2.f, 0.1f, "%.1f m", noExt,
    "A beam fades out as your head comes this close to it (across the floor), so it is not in your face as you pick the thing up. 0: never.")
QVR_SLIDER_BYNAME(vr_lootbeam_far, "Fade Far Away", 0.f, 60.f, 1.f, "%.0f m", noExt, "Beams farther than this fade out. 0: never.")
QVR_SLIDER_BYNAME(vr_lootbeam_hue_weapons, "Weapons Hue", 0.f, 355.f, 5.f, "%.0f", noExt,
    "Hue in degrees: 0 red, 40 gold, 120 green, 190 cyan, 240 blue, 285 violet.")
QVR_SLIDER_BYNAME(vr_lootbeam_hue_weapons_empty, "Empty Weapons Hue", -5.f, 355.f, 5.f, "%.0f", noExt,
    "A reloadable weapon whose magazine is empty (or out): 0 red, so you know it needs ammo before you grab it. Below 0: as the other weapons.")
QVR_SLIDER_BYNAME(vr_lootbeam_hue_mags, "Magazines Hue", 0.f, 355.f, 5.f, "%.0f", noExt, nullptr)
QVR_SLIDER_BYNAME(vr_lootbeam_hue_ammo, "Ammo Boxes Hue", 0.f, 355.f, 5.f, "%.0f", noExt, nullptr)
QVR_SLIDER_BYNAME(vr_lootbeam_hue_health, "Health Boxes Hue", 0.f, 355.f, 5.f, "%.0f", noExt, nullptr)
QVR_SLIDER_BYNAME(vr_lootbeam_hue_armor, "Armour Hue", 0.f, 355.f, 5.f, "%.0f", noExt, nullptr)
QVR_SLIDER_BYNAME(vr_lootbeam_hue_powerups, "Power-ups Hue", 0.f, 355.f, 5.f, "%.0f", noExt, nullptr)
QVR_SLIDER_BYNAME(vr_lootbeam_hue_keys, "Keys and Runes Hue", 0.f, 355.f, 5.f, "%.0f", noExt, nullptr)
QVR_SLIDER_BYNAME(vr_lootbeam_hue_backpacks, "Backpacks Hue", 0.f, 355.f, 5.f, "%.0f", noExt, nullptr)
QVR_SLIDER_BYNAME(vr_lootbeam_hue_toolgun, "Toolgun Hue", 0.f, 355.f, 5.f, "%.0f", noExt,
    "A dropped toolgun's own colour (it is a weapon, but easy to lose among many): 305 magenta.")
QVR_SLIDER_BYNAME(vr_lootbeam_toolgun_length, "Toolgun Beam Length", 1.f, 4.f, 0.25f, "x%.2f", noExt,
    "A dropped toolgun's beam this many times Length, so it stands above the weapons'.")

// vr_menu_pages.inc: pageGraphicsTeleporters
QVR_TOGGLE_BYNAME(vr_teleporters, "Quake VR Teleporters",
    "Teleporters show where they lead in VR, flat screen and the spectator camera. Movement, shots and dynamic light go through them. Off: Quake's plain teleporters and their starry surfaces. Takes effect at once, no map reload.")
QVR_SLIDER_BYNAME(vr_portals, "Teleporter Views", 0.f, 1.f, 0.1f, "%.1f", ext(),
    "Teleporters show where they lead: the place they take you to, live, through their shimmer (the scene is drawn again for the visible teleporters). 0: Quake's plain shimmer.")
QVR_TOGGLE_BYNAME(vr_portals_walk, "Seamless Teleporters",
    "With Teleporter Views on, you walk through teleporters into the place you saw, keeping your speed and the way you face (no flash, no pause), and rockets, grenades, nails, thrown props and gunfire go through them. Off: Quake's teleport.")
QVR_TOGGLE_BYNAME(vr_portals_monsters, "Monsters Walk Through",
    "With Seamless Teleporters on, monsters walk through a teleporter that comes out of another teleporter's face as you do, coming out of that face. Off (and at other teleporters, or for a monster too big for the opening): Quake's teleport, which puts them at the destination marker, a step out.")
QVR_TOGGLE_BYNAME(vr_portals_ai, "Enemies See and Shoot Through",
    "Ranged enemies spot players through an active aperture and aim through it. The teleporter and both rooms must be clear; closing it cancels delayed shots. One teleporter per sight line. Melee and navigation stay local.")
QVR_TOGGLE_BYNAME(vr_teleporter_pair_exits, "Exits On Paired Teleporters",
    "A teleporter whose destination stands in front of another teleporter of its size (a two-way pair) brings you, props and the view out of that teleporter's face, so you are halfway through both as you cross. Off: out at the destination marker, which can stand a step out from the teleporter.")
QVR_TOGGLE_BYNAME(vr_teleporter_self_head, "Your Head In Teleporters",
    "Seeing yourself through a teleporter (the loop room's teleporters show your back), your body has its head. Your own eyes never draw it. Off: headless there too.")
QVR_SLIDER_BYNAME(vr_portals_maxviews, "Visible Teleporters", 1.f, 8.f, 1.f, "%.0f", noExt,
    "Maximum destination views drawn per camera (default 4). Each visible teleporter renders another scene; lower this for VR performance.")
QVR_SLIDER_BYNAME(vr_portals_recursion, "Teleporters Within Teleporters", 0.f, 3.f, 1.f, "%.0f", noExt,
    "A teleporter seen through a teleporter shows where it leads too, this many teleporters deep (the loop room's teleporters: you, again and again). Each costs another scene, drawn smaller. 0: teleporters seen through a teleporter are left out.")
QVR_SLIDER_BYNAME(vr_portals_recursion_views, "Teleporters In A Teleporter's View", 1.f, 8.f, 1.f, "%.0f", noExt,
    "How many of the teleporters seen through a teleporter show where they lead (the nearest and largest). Deeper, only the best one does.")
QVR_SLIDER_BYNAME(vr_portals_recursion_max, "Most Teleporters Within Teleporters", 0.f, 16.f, 1.f, "%.0f", noExt,
    "At most this many teleporters seen through teleporters show where they lead, for each eye: the teleporter you look at most first. Each costs another scene.")
QVR_SLIDER_BYNAME(vr_portals_recursion_scale, "Deeper Teleporters' Resolution", 1.f, 4.f, 1.f, "1/%.0f", noExt,
    "Each teleporter deeper is drawn with this many times fewer pixels a side (2: a quarter of the pixels, then a sixteenth). 1: all at full resolution.")
QVR_SLIDER_BYNAME(vr_teleporter_surface_size, "Size", 1.f, 2.f, 0.02f, "%.2fx", noExt,
    "Enlarges the single star layer on the entrance plane. No extra star quads are drawn inside the destination view. The collision opening keeps its size.")
QVR_SLIDER_BYNAME(vr_teleporter_surface_opacity, "Opacity", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much entrance shimmer covers the view through the teleporter. 0: a clear view; 1: the original shimmer.")
QVR_SLIDER_BYNAME(vr_teleporter_surface_fade, "Fade Up Close", 0.f, 1.f, 0.05f, "%.2f m", noExt,
    "The shimmer over the destination view fades out as your eyes come this close to the teleporter, and is gone as you cross, so the room does not brighten in a step as you arrive. 0: it stays to the end.")

// vr_menu_pages.inc: pageGraphicsFoam
QVR_SLIDER_BYNAME(vr_water_foam, "Shoreline Foam", 0.f, 2.f, 0.1f, "%.1f", ext(),
    "Every liquid's shoreline foam: how much (0 off). Teleports have none.")

// vr_menu_pages.inc: pageGraphicsLiquids
QVR_SLIDER_BYNAME(vr_water_waves, "Waves", 0.f, 2.f, 0.1f, "%.1f", ext(),
    "Waves on water, slime and lava, in the lighting (the surfaces stay flat). 0: Quake's flat liquids.")
QVR_SLIDER_BYNAME(vr_water_fresnel, "Water Reflection", 0.f, 2.f, 0.1f, "%.1f", ext(),
    "Water is clearer looking straight down and reflects the room at grazing angles.")
QVR_SLIDER_BYNAME(vr_water_reflections, "Reflected Room", 0.f, 1.f, 0.1f, "%.1f", ext(),
    "Water and slime mirror the room around them (walls, lights, the sky) instead of a plain sheen: a picture of the world made at the water's surface nearest you. How strongly the water reflects is Water Reflection. 0: off.")
QVR_SLIDER_BYNAME(vr_water_refraction, "Water Refraction", 0.f, 2.f, 0.1f, "%.1f", ext(),
    "What is under the water is bent by the waves (needs r_oit 1).")
QVR_SLIDER_BYNAME(vr_water_glints, "Water Glints", 0.f, 2.f, 0.1f, "%.1f", ext(), "Light glinting off the waves.")
QVR_SLIDER_BYNAME(vr_water_lava_glow, "Lava Glow", 0.f, 2.f, 0.1f, "%.1f", ext(), "Lava glows, its hot parts brightest, pulsing slowly.")
QVR_TOGGLE_BYNAME(vr_water_geo_waves, "Real Waves",
    "The surfaces of water, slime and lava rise and fall in long swells (still at the walls). Off: flat, the waves only in the lighting.")
QVR_SLIDER_BYNAME(vr_water_geo_amplitude, "Real Wave Height", 0.5f, 24.f, 0.5f, "%.1f", ext(),
    "How high the swells rise, in units (lava's are bigger and slower, slime's smaller).")
QVR_SLIDER_BYNAME(vr_heat_haze, "Heat Haze", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "The air shimmers over lava, round explosions and over flames (0 off).")
QVR_CYCLE_BYNAME(vr_heat_haze_half, "Heat Haze Detail", nullptr,
    {1.f, "Half Size"}, {0.f, "Full Size"})
QVR_SLIDER_BYNAME(vr_water_caustics, "Caustics", 0.f, 1.5f, 0.05f, "%.2f", ext(), "Dappled light on floors and walls under water.")
QVR_SLIDER_BYNAME(vr_water_underwater, "Underwater View", 0.f, 1.5f, 0.1f, "%.1f", ext(),
    "With your head under: the liquid's fog and tint, a little blur. 0: Quake's colour shift.")
QVR_SLIDER_BYNAME(vr_water_wobble, "Underwater Wobble", 0.f, 2.f, 0.1f, "%.1f", ext(),
    "A slow wobble of the view under water. Turn it down or off if it makes you uneasy.")
QVR_SLIDER_BYNAME(vr_water_splash, "Splashes", 0.f, 2.f, 0.1f, "%.1f", ext(),
    "Drops, foam and ripples where shots, thrown things, you and your hands hit water, slime or lava, and as you wade: how many (0 off).")
QVR_SLIDER_BYNAME(vr_water_splash_size, "Splash Size", 0.5f, 2.5f, 0.1f, "%.1f", ext(), "How big the splashes' drops, spray, foam and rings are.")
QVR_SLIDER_BYNAME(vr_water_splash_ring_speed, "Splash Ring Speed", 0.3f, 2.f, 0.1f, "%.1f", ext(),
    "How fast the rings on the surface spread (1: riding the ripples' crest).")
QVR_SLIDER_BYNAME(vr_water_splash_ring_size, "Splash Ring Size", 0.f, 2.5f, 0.1f, "%.1f", ext(),
    "How big the rings start and how long they last, so how far they get (0: none).")
QVR_TOGGLE_BYNAME(vr_water_ripples, "Ripples",
    "Splashes, wading and strokes send rings of waves across the surface, in its lighting and, with Real Waves' grid, its shape.")
QVR_SLIDER_BYNAME(vr_water_ripple_amplitude, "Ripple Height (Geometry)", 0.f, 24.f, 0.5f, "%.1f", ext(),
    "How high a hand's slap's ripples rise in the surface itself, in units (a shot's about half, a body's two to three times).")
QVR_SLIDER_BYNAME(vr_water_ripple_normal, "Ripple Normal Strength", 0.f, 2.f, 0.05f, "%.2f", ext(),
    "How steep the ripples look in the lighting (glints, reflection, refraction), times the geometry's slope: 1 as shaped, 0 the shape only.")
QVR_SLIDER_BYNAME(vr_water_ripple_speed, "Ripple Speed", 10.f, 100.f, 2.f, "%.0f", ext(),
    "How fast the rings of ripples spread, in units a second (lava's a third, slime's slower).")
QVR_SLIDER_BYNAME(vr_water_ripple_decay, "Ripple Duration", 0.3f, 4.f, 0.1f, "%.1f", ext(), "Seconds for the ripples to die down to a third.")
QVR_SLIDER_BYNAME(vr_water_ripple_wavelength, "Ripple Wavelength", 16.f, 192.f, 4.f, "%.0f", ext(),
    "Units between the ripples' crests. In the geometry from two of its 16-unit cells up (vr_water_geo_cell), fully from three; below that in the lighting only.")
QVR_SLIDER_BYNAME(vr_water_ripple_max, "Ripples at Once", 1.f, 32.f, 1.f, "%.0f", ext(),
    "How many ripples can spread at once; a new one takes the weakest's place. Fewer costs less GPU time when much is splashing.")
QVR_SLIDER_BYNAME(vr_water_sounds, "Water Sounds", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "Volume of splashes, shots into water, wading and swimming strokes (0 off: Quake's own splash sounds only).")

// vr_menu_pages.inc: pageGraphicsPost
QVR_SLIDER_BYNAME(vr_bloom, "Bloom", 0.f, 1.5f, 0.02f, "%.2f", ext(),
    "Glow around lamps, glowing panels and buttons, flashes and explosions (0 off).")
QVR_SLIDER_BYNAME(vr_bloom_threshold, "Bloom Threshold", 0.3f, 1.f, 0.01f, "%.2f", ext(), "How bright a thing must be to glow.")
QVR_SLIDER_BYNAME(vr_bloom_radius, "Bloom Size", 0.25f, 3.f, 0.05f, "%.2f", ext(), nullptr)
QVR_SLIDER_BYNAME(vr_bloom_white, "Bloom: White Lights", 0.f, 2.f, 0.02f, "%.2fx", ext(),
    "Glow of white and pale lights (lamps, flashes), times Bloom.")
QVR_SLIDER_BYNAME(vr_bloom_color, "Bloom: Coloured Lights", 0.f, 3.f, 0.05f, "%.2fx", ext(),
    "Glow of coloured lights (red buttons, blue panels), times Bloom.")
QVR_SLIDER_BYNAME(vr_bloom_adapt, "Bloom: Adapt", 0.f, 10.f, 0.25f, "%.2f", ext(),
    "How much bloom weakens when much of the view glows (a brightly lit map); 0: never.")
QVR_TOGGLE_BYNAME(vr_bloom_fast, "Bloom: Fast",
    "On: a cheaper glow: three glow sizes instead of four (the widest folded into the next) and the glow added with one texture read a pixel instead of four. About 0.035 ms an eye cheaper at 2784 x 2784 (a quarter of the post-processing and bloom), halo edges and lava at most about 20/255 different. Off: as before, for comparison.")
QVR_TOGGLE_BYNAME(vr_tonemap, "Tone Mapping",
    "Bright lights, flashes, lava and explosions keep their colour and detail instead of clipping to yellow and white (a filmic shoulder). The rest is unchanged. Off: Quake's.")
QVR_SLIDER_BYNAME(vr_exposure, "Exposure", 0.5f, 2.f, 0.05f, "%.2f", ext(), "Brightness before tone mapping (1: as Quake).")
QVR_CYCLE_BYNAME(vr_grade, "Colour Grade",
    "A subtle colour mood. Quake VR: a touch of contrast, cool shadows, warm highlights. By Episode: cold, mossy, warm, violet for episodes 1 to 4.",
    {0.f, "Off"}, {1.f, "Quake VR"}, {2.f, "Cold"}, {3.f, "Warm"}, {4.f, "By Episode"})
QVR_SLIDER_BYNAME(vr_grade_strength, "Grade Strength", 0.f, 1.f, 0.05f, "%.2f", noExt, nullptr)
QVR_CYCLE_BYNAME(vr_dither, "Dither",
    "A faint noise of one step that hides banding in dark gradients (fog, dark rooms, under water).",
    {0.f, "Off"}, {1.f, "Per Eye"}, {2.f, "Per Eye, Moving"}, {3.f, "Same in Both Eyes"})

// vr_menu_pages.inc: pageGraphicsModels
QVR_TOGGLE_BYNAME(vr_model_lighting, "Model Lighting",
    "Monsters, items and weapons are shaded from the map's lights, not one fixed direction.")
QVR_TOGGLE_BYNAME(vr_model_light_parity, "Models Lit as the World",
    "Monsters and items as bright as the floor they stand on (off: Quake's brighter models).")
QVR_TOGGLE_BYNAME(vr_model_ambient_dir, "Directional Ambient",
    "Monsters, items, weapons and hands take the light around them: brighter on the side facing lit walls, darker facing shade.")
QVR_SLIDER_BYNAME(vr_model_ambient_contrast, "Ambient Contrast", 0.f, 1.5f, 0.05f, "%.2f", ext(),
    "How much brighter the lit side is than the shaded one (0: one brightness all round).")
QVR_CYCLE_BYNAME(r_lerpmodels, "Model Animation",
    "How monsters move between the frames of their animations. Stepped: frame to frame, as DOS Quake. Quake's Blend: each point slides straight to its next place (limbs shorten mid-swing, the speed jumps at each frame). Smooth: along curves through the frames before and after, limbs swung round their joints at full length. Every frame is still shown exactly when the game shows it: only what is drawn between frames changes.",
    {0.f, "Stepped"}, {1.f, "Quake's Blend"}, {3.f, "Smooth"})
QVR_SLIDER_BYNAME(vr_anim_smooth_tension, "Animation Ease", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "Smooth: 0 keeps the speed through each frame (the smoothest; a swing may swing a little past a frame between two); 1 eases to rest at each frame.")
QVR_TOGGLE_BYNAME(vr_mdl_dequant, "Steady Model Vertices",
    "Quake's models store each point rounded to a 255th of the model's size, frame by frame, so surfaces ripple as they animate. On: each point follows the smoothest path through its animation that stays within that rounding (no point moves further than the file's own error), round the loop where an animation loops (as the game plays it); the lighting's directions likewise, within their own rounding, so the shading does not flicker. Off: the points exactly as stored.")
QVR_TOGGLE_BYNAME(vr_anim_smooth_rig, "Swing Limbs",
    "Smooth: a monster with a ragdoll rig (grunts, knights, ogres, the shambler and more) swings its limbs round their joints between frames at full length (off: the curves alone).")
QVR_TOGGLE_BYNAME(vr_anim_smooth_rebuild, "Rebuild From Skeleton",
    "A monster with a ragdoll rig is drawn from its skeleton every frame, with any Model Animation: each point keeps one place on its bone and moves with it, so the wobble left by the file's rounding is gone; points the skeleton fits less well (round the joints) keep the frames' places. Off: the frames' points as stored (steadied by Steady Model Vertices).")
QVR_SLIDER_BYNAME(vr_rim_light, "Rim Light", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "A faint light round the edges of monsters and things, in the light around them, so they stand out from dark backgrounds (0 off).")
QVR_SLIDER_BYNAME(vr_viewmodel_minlight, "Hands' Least Light", 0.f, 32.f, 1.f, "%.0f", ext(),
    "How lit your hands and weapons stay in the dark (Quake: 24).")
QVR_TOGGLE_BYNAME(vr_weapon_reflections, "Weapon Reflections",
    "The metal of your weapons (barrels, blades) reflects the room around you, dimmer in the dark.")
QVR_SLIDER_BYNAME(vr_weapon_reflections_strength, "Reflection Strength", 0.f, 2.f, 0.1f, "%.1fx", ext(), "How strongly your weapons' metal reflects the room.")
QVR_SLIDER_BYNAME(vr_weapon_glow, "Weapon Sight Glow", 0.f, 3.f, 0.1f, "%.1fx", ext(),
    "Your weapons' glowing parts, as the shotgun's sights, shine brighter and glow (0: as Quake).")
QVR_TOGGLE_BYNAME(vr_particle_light, "Lit Particles",
    "Blood, smoke, dust, debris and water take the light where they are: dark in dark corners, lit by your flashlight, muzzle flashes and explosions. Fire, sparks and glows stay bright. Off: all as bright as their colours, as in Quake.")
QVR_TOGGLE_BYNAME(vr_soft_particles, "Soft Particles",
    "Smoke, fire, blood and explosions fade out where they meet walls and floors instead of cutting hard lines through them.")
QVR_SLIDER_BYNAME(vr_soft_particles_scale, "Soft Particle Distance", 0.25f, 3.f, 0.25f, "%.2fx", ext(),
    "How far in front of walls and floors they start fading (bigger: softer, but thin puffs against a wall fainter).")
QVR_SLIDER_BYNAME(vr_particle_near_fade, "Particle Fade Near Eyes", 0.f, 100.f, 5.f, "%.0f cm", noExt,
    "Smoke, blood and sparks fade out as they come this close to your eyes, gone at Particles Hidden Nearer: no screen-filling blot in your face (0: no fade).")
QVR_SLIDER_BYNAME(vr_particle_near_cull, "Particles Hidden Nearer", 0.f, 50.f, 1.f, "%.0f cm", ext(),
    "Particles nearer your eyes than this are not drawn at all (0: all drawn).")
QVR_SLIDER_BYNAME(vr_particle_max_screen, "Largest Particle", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "A particle is never drawn larger than this part of the view's height across: a bigger one is shrunk round its centre (cheaper on the GPU, no single puff filling the view). 0: no limit.")
QVR_TOGGLE_BYNAME(vr_particle_halfres, "Half-Res Heavy Particles",
    "When smoke, fire and blood fill much of the view, the particles are drawn at half resolution and blended in: a quarter of their cost on the GPU, a little softer.")
QVR_TOGGLE_BYNAME(vr_particle_trim, "Trim Particle Margins",
    "Trim empty margins of larger fire and smoke sprites. Off by default: the extra work can cost more than it saves for small particles. Keeps filtering and retro blocks; uncertain cases keep their full shape.")
QVR_TOGGLE_BYNAME(vr_particle_saturate, "Skip Hidden Particles",
    "Particles are composited in reverse order into a layer of their own, and where thick smoke or blood is already opaque the particles under it are skipped: the same image, less GPU work in dense effects. Off: each blended into the scene in turn, as before.")
QVR_TOGGLE_BYNAME(vr_particle_retro_fast, "Fast Retro Particles",
    "Optimize full-resolution retro particles while keeping palette, dither and edge filtering. Off compares the original full-resolution shader. Retro half resolution always uses the fast shader.")
QVR_TOGGLE_BYNAME(vr_particle_retro_halfres, "Half-Res Retro Particles",
    "Optional: render large retro particles at half resolution during heavy effects. Keeps their palette and world-space grid, with softer edges. Small particles stay at full resolution.")
QVR_SLIDER_BYNAME(vr_particle_retro_halfres_pixels, "Retro Half-Res Size", 0.f, 256.f, 8.f, "%.0f px", noExt,
    "Minimum size from particle centre to edge for half resolution. Zero draws all in their existing order; splitting sizes draws the small particles over the large ones.")
QVR_SLIDER_BYNAME(vr_weapon_morph_time, "Ammo Switch Morph", 0.f, 1.f, 0.05f, "%.2f s", ext(),
    "Switching a gun's ammo with its button morphs it into its other look (lava, multi-rockets, plasma) over this long, a glowing seam sweeping over it (0: at once).")

// vr_menu_pages.inc: pageHudConfiguration
QVR_SLIDER_BYNAME(vr_hud_scale, "HUD Scale", 0.01f, 0.1f, 0.005f, "%.3f", ext(0.005f, 0.3f), nullptr)
QVR_SLIDER_BYNAME(vr_sbar_offset_x, "Offset X", -200.f, 200.f, 1.f, "%.0f", ext(-1000.f, 1000.f), nullptr)
QVR_SLIDER_BYNAME(vr_sbar_offset_y, "Offset Y", -200.f, 200.f, 1.f, "%.0f", ext(-1000.f, 1000.f), nullptr)
QVR_SLIDER_BYNAME(vr_sbar_offset_z, "Offset Z", -200.f, 200.f, 1.f, "%.0f", ext(-1000.f, 1000.f), nullptr)
QVR_SLIDER_BYNAME(vr_sbar_offset_roll, "Roll", -3.14f, 3.14f, 0.05f, "%.2f rad", ext(), nullptr)
QVR_SLIDER_BYNAME(vr_sbar_offset_pitch, "Pitch", -3.14f, 3.14f, 0.05f, "%.2f rad", ext(), nullptr)
QVR_SLIDER_BYNAME(vr_sbar_offset_yaw, "Yaw", -3.14f, 3.14f, 0.05f, "%.2f rad", ext(), nullptr)

// vr_menu_pages.inc: pageHotspotSettings
QVR_TOGGLE_BYNAME(vr_show_virtual_stock, "Show Virtual Stock", nullptr)
QVR_TOGGLE_BYNAME(vr_show_shoulder_holsters, "Show Shoulder Holst.", nullptr)
QVR_SLIDER_BYNAME(vr_shoulder_holster_offset_x, "Shoulder X", -50.f, 50.f, 0.5f, "%.1f", ext(-200.f, 200.f), nullptr)
QVR_SLIDER_BYNAME(vr_shoulder_holster_offset_y, "Shoulder Y", -50.f, 50.f, 0.5f, "%.1f", ext(-200.f, 200.f), nullptr)
QVR_SLIDER_BYNAME(vr_shoulder_holster_offset_z, "Shoulder Z", -50.f, 50.f, 0.5f, "%.1f", ext(-200.f, 200.f), nullptr)
QVR_SLIDER_BYNAME(vr_shoulder_holster_thresh, "Shoulder Threshold", 0.f, 30.f, 0.1f, "%.1f", ext(0.f, 100.f), nullptr)
QVR_TOGGLE_BYNAME(vr_show_upper_holsters, "Show Upper Holsters", nullptr)
QVR_SLIDER_BYNAME(vr_upper_holster_offset_x, "Upper X", -50.f, 50.f, 0.5f, "%.1f", ext(-200.f, 200.f),
    "Forward (negative: back), units. With the body drawn, the default (-4.25) is on the chest; further back, the holsters go round your ribs to your back.")
QVR_SLIDER_BYNAME(vr_upper_holster_offset_y, "Upper Y", -50.f, 50.f, 0.5f, "%.1f", ext(-200.f, 200.f), nullptr)
QVR_SLIDER_BYNAME(vr_upper_holster_offset_z, "Upper Z", -50.f, 50.f, 0.5f, "%.1f", ext(-200.f, 200.f), nullptr)
QVR_SLIDER_BYNAME(vr_upper_holster_thresh, "Upper Threshold", 0.f, 30.f, 0.1f, "%.1f", ext(0.f, 100.f), nullptr)

// vr_menu_pages.inc: pageHolsterZones
QVR_CYCLE(vr_holster_zones, "Zones",
    "Boxes: a box at each holster and pouch where it is drawn; a gun goes in a holster when its grip or body is "
    "in the box, the one it is deepest in, and a pouch never takes a gun (it gives and takes rounds, "
    "magazines and grenades). Old Spheres: a ball round each, the hand alone (a gun let go by the ammo pouch "
    "fell there).",
    {1.f, "Boxes"}, {0.f, "Old Spheres"})
QVR_TOGGLE(vr_holster_zones_show, "Show Zones",
    "Every zone as a wireframe: grey idle, blue with a gun in it, yellow a candidate, green where a hand is "
    "(holding a gun: where it goes when let go). Also on Debug > Views. On the body's clone beside the "
    "menu too.")
QVR_TOGGLE(vr_holster_zones_show_items, "Show Item Boxes",
    "Each zone's box as for a hand carrying an ammo box, a health box or a backpack (its Item Size below), "
    "in magenta, on you and on the clone. Also while an Item Size is chosen.")
QVR_TOGGLE(vr_holster_zone_hand_point, "Hand Point",
    "A hand holding a gun is tested at its palm as drawn too, beside the gun's grip and body: the hand in a "
    "holster puts the gun in it even when the gun's points are not (the holsters only: the pouches take an "
    "empty hand alone).")
QVR_SLIDER(vr_holster_zone_handle_radius, "Handle Radius", 0.f, 10.f, 0.25f, "%.2f", ext(0.f, 30.f),
    "A hand reaching for a holstered gun: a ball this many units round its grip as drawn is that holster's "
    "zone too, so a handle standing out of the box (the shoulders') can be taken. 0: the box alone.")

// vr_menu_pages.inc: pageHipHolsters
QVR_TOGGLE_BYNAME(vr_show_hip_holsters, "Show Hip Holsters", nullptr)
QVR_SLIDER_BYNAME(vr_hip_offset_x, "Hip X", -50.f, 50.f, 0.5f, "%.1f", ext(-200.f, 200.f),
    "Forward (negative: back), units. With the body drawn, the default (-3.5) is on the belly; further back, the holsters go round your hips to your back.")
QVR_SLIDER_BYNAME(vr_hip_offset_y, "Hip Y", -50.f, 50.f, 0.5f, "%.1f", ext(-200.f, 200.f), nullptr)
QVR_SLIDER_BYNAME(vr_hip_offset_z, "Hip Z", -50.f, 50.f, 0.5f, "%.1f", ext(-200.f, 200.f), nullptr)
QVR_SLIDER_BYNAME(vr_hip_holster_thresh, "Hip Threshold", 0.f, 30.f, 0.1f, "%.1f", ext(0.f, 100.f), nullptr)
QVR_SLIDER_BYNAME(vr_leg_holster_model_scale, "Holster Slot Scale", 0.1f, 2.f, 0.05f, "%.2f", ext(0.02f, 5.f), nullptr)
QVR_SLIDER_BYNAME(vr_leg_holster_model_x_offset, "Holster Slot X", -100.f, 100.f, 0.5f, "%.1f", ext(-400.f, 400.f), nullptr)
QVR_SLIDER_BYNAME(vr_leg_holster_model_y_offset, "Holster Slot Y", -100.f, 100.f, 0.5f, "%.1f", ext(-400.f, 400.f), nullptr)
QVR_SLIDER_BYNAME(vr_leg_holster_model_z_offset, "Holster Slot Z", -100.f, 100.f, 0.5f, "%.1f", ext(-400.f, 400.f), nullptr)
QVR_TOGGLE_BYNAME(vr_show_grenade_pouch, "Show Grenade Pouch",
    "A marker at the pouch the size of its reach, green while a hand is there. Hand grenades, and these sliders too, are on Batting and Catching.")
QVR_SLIDER_BYNAME(vr_grenade_pouch_x, "Pouch X", -50.f, 50.f, 0.5f, "%.1f", ext(-200.f, 200.f),
    "Forward (negative: back), units. With the body drawn, the default (-7) is on the belt on your lower back; forward, it goes round your hips.")
QVR_SLIDER_BYNAME(vr_grenade_pouch_y, "Pouch Y", -50.f, 50.f, 0.5f, "%.1f", ext(-200.f, 200.f), "To your right (negative: left), units.")
QVR_SLIDER_BYNAME(vr_grenade_pouch_z, "Pouch Z", -50.f, 50.f, 0.5f, "%.1f", ext(-200.f, 200.f), "Up (negative: down), units: 0 is the hip holsters' height.")
QVR_SLIDER_BYNAME(vr_grenade_pouch_thresh, "Pouch Threshold", 0.f, 30.f, 0.1f, "%.1f", ext(0.f, 100.f),
    "How near the pouch a hand must be to take a grenade from it (or put one back).")
QVR_SLIDER_BYNAME(vr_grenade_pouch_hold_pitch, "Grenade In Hand Pitch", -180.f, 180.f, 5.f, "%.0f deg", noExt,
    "A grenade taken from the pouch: its front tipped up in your hand (negative: down), about its middle.")
QVR_SLIDER_BYNAME(vr_grenade_pouch_hold_yaw, "Grenade In Hand Yaw", -180.f, 180.f, 5.f, "%.0f deg", noExt,
    "A grenade taken from the pouch: its front turned left in your hand (negative: right), mirrored for the left hand.")
QVR_SLIDER_BYNAME(vr_grenade_pouch_hold_roll, "Grenade In Hand Roll", -180.f, 180.f, 5.f, "%.0f deg", noExt,
    "A grenade taken from the pouch: rolled right in your hand (negative: left), mirrored for the left hand.")

// vr_menu_pages.inc: pageTransparencyOptions
QVR_TOGGLE_BYNAME(r_novis, "(!) No Vis",
    "NOT RECOMMENDED: water transparency in maps without vis data, by drawing the whole map every frame. Vispatch maps instead.")
QVR_SLIDER_BYNAME(r_wateralpha, "Water Alpha", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "How see-through water is (1: opaque). Only in maps vised for it (the relit maps are).")
QVR_SLIDER_BYNAME(r_lavaalpha, "Lava Alpha", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "How see-through lava is (1: opaque; 0: as Water Alpha). Only in maps vised for it.")
QVR_SLIDER_BYNAME(r_telealpha, "Tele Alpha", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "How see-through teleporters are (1: opaque; 0: as Water Alpha). Only in maps vised for it.")
QVR_SLIDER_BYNAME(r_slimealpha, "Slime Alpha", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "How see-through slime is (1: opaque; 0: as Water Alpha). Only in maps vised for it.")
QVR_TOGGLE_BYNAME(vr_map_liquid_alpha, "Map's Own Alpha",
    "Maps that set their liquids' alpha themselves (worldspawn keys) use theirs instead of the sliders above.")

// vr_menu_pages.inc: pageGraphicsRelighting
QVR_TOGGLE_BYNAME(vr_relight_use, "Use In-game Relights",
    "Maps relit here are used over those relight_maps.py made (next map). Off: the relit maps of the script, or the maps' own light.")
QVR_TOGGLE_BYNAME(vr_relight_reload, "Reload Where You Are",
    "After relighting the map you are in, it is reloaded at once, where you are (a quick save and load; where the game can't be saved, the map restarts). Off: you see the new light from its next start.")
QVR_CYCLE_BYNAME(vr_relight_batch_set, "Maps",
    "What Relight These Maps relights, with the settings below: the map you are in, an episode (E1M1 to E1M8...), a whole game (Quake, a mission pack, a campaign), every map installed from the Map Library, or every map there is.",
    {0.f, "This Map"}, {1.f, "An Episode"}, {2.f, "A Game"}, {3.f, "Map Library's"}, {4.f, "Every Map"})
QVR_CYCLE_BYNAME(vr_relight_batch_episode, "Episode",
    "Maps: An Episode. The one the map you are in belongs to (E1M3: E1; a mission pack's HIP2M1: its second), or one of Quake's.",
    {0.f, "The One You're In"}, {1.f, "E1"}, {2.f, "E2"}, {3.f, "E3"}, {4.f, "E4"}, {5.f, "E5 (Past)"})
QVR_CYCLE_BYNAME(vr_relight_batch_game, "Game",
    "Maps: A Game. The one the map you are in is from, or Quake, Scourge of Armagon, Dissolution of Eternity, Dimension of the Past, Dimension of the Machine, Dawn of the Machine (when installed).",
    {0.f, "The One You're In"}, {1.f, "Quake"}, {2.f, "Armagon"}, {3.f, "Eternity"}, {4.f, "Past"}, {5.f, "Machine"},
    {6.f, "Dawn"})
QVR_TOGGLE_BYNAME(vr_relight_batch_force, "Relight Unchanged Maps Too",
    "Off: a map relit with the same settings before (and the same map file and texture rules) is skipped. On: every map is relit again.")
QVR_SLIDER_BYNAME(vr_relight_parallel, "Maps at Once", 0.f, 8.f, 1.f, "%.0f", noExt,
    "How many maps are lit side by side (light's threads shared between them). 0: two on a computer with 8 cores or more, else one. More finishes a batch of small maps a little sooner; maps with Bounced Light gain nothing, and four are slower.")
QVR_CYCLE_BYNAME(vr_relight_batch_reload, "Reload the Map You're In",
    "When the batch relights the map you are in (it goes first) and Reload Where You Are is on: reload it as soon as it is done, or only when the whole batch is.",
    {0.f, "When It's Done"}, {1.f, "At the End"})
QVR_TOGGLE_BYNAME(vr_relight_indicator, "Progress Outside the Menu",
    "While maps are being relit, a line on the wrist gadget's screen (on a flat screen, in the corner): maps done, percentage and the time left.")
QVR_SLIDER_BYNAME(vr_relight_strength, "Light Textures", 0.f, 4.f, 0.1f, "%.1fx", ext(),
    "How bright everything a texture lights is: lamps, light panels, glowing buttons and panels, lava (1: the relight script's).")
QVR_SLIDER_BYNAME(vr_relight_lamps, "Lamps and Light Panels", 0.f, 4.f, 0.1f, "%.1fx", ext(),
    "The light fixtures' light, times Light Textures: lamps, light panels and strips, the pyramid lanterns.")
QVR_SLIDER_BYNAME(vr_relight_glows, "Glowing Panels and Buttons", 0.f, 4.f, 0.1f, "%.1fx", ext(),
    "Buttons, computer panels, runes and teleporters' glow on what is round them, times Light Textures.")
QVR_SLIDER_BYNAME(vr_relight_liquids, "Lava and Slime", 0.f, 4.f, 0.1f, "%.1fx", ext(),
    "The red glow lava casts on the walls round it, and slime's faint green one, times Light Textures.")
QVR_SLIDER_BYNAME(vr_relight_maplights, "Map Lights", 0.f, 4.f, 0.1f, "%.1fx", ext(),
    "The map's own lights (the mapper's), as far, this much brighter.")
QVR_SLIDER_BYNAME(vr_relight_sunlight, "Sunlight", 0.f, 4.f, 0.1f, "%.1fx", ext(),
    "The sun and the sky's light, in maps that have them (outdoor maps; not id's).")
QVR_SLIDER_BYNAME(vr_relight_bounce, "Bounced Light", 0.f, 2.f, 0.1f, "%.1f", noExt,
    "Light bouncing off the walls fills the shade: brighter, flatter rooms (0 none, the relight script's moodier look). Takes longer.")
QVR_SLIDER_BYNAME(vr_relight_ao, "Ambient Occlusion", 0.f, 3.f, 0.1f, "%.1f", noExt,
    "Corners and crevices darker (1.5: the relight script's; 0 none).")
QVR_SLIDER_BYNAME(vr_relight_minlight, "Ambient Light Level", 0.f, 64.f, 4.f, "%.0f", noExt,
    "A basic ambient light added everywhere in the relit map, on top of the map's own minimum (mostly none: then no place is darker than this; in a map's light units: 32 a dim fill, 64 a clear one; 0: the map's own). Not darkened by Ambient Occlusion. An easy way out of pitch-black corners, at some cost to contrast.")
QVR_CYCLE_BYNAME(vr_relight_quality, "Shadow Quality",
    "Smooth: soft shadow edges (the relight script's). Fast: about four times quicker, harder edges.",
    {0.f, "Fast"}, {1.f, "Smooth"})
QVR_TOGGLE_BYNAME(vr_relight_seethrough, "See-Through Liquids",
    "id's maps (Quake and its two mission packs) relit with see-through water, slime and teleporters: VisPatch's visibility data put into them, so you see what is under and beyond (how clear: Transparency's alphas). Off: their liquids stay opaque. Maps made with see-through water keep it either way.")

// vr_menu_pages.inc: pageGraphicsRetroLight
QVR_TOGGLE_BYNAME(vr_retrolight, "Retro Lighting",
    "The light in Quake's coarse look: the baked light in a few levels (bands), the lightmap's blocky grid, "
    "dynamic lights in levels and on blocks, hard blocky shadows. Fixed to the surfaces, so nothing swims "
    "as your head moves. Goes with Retro Textures. Debug > Views > Retro Lighting A/B compares.")
QVR_SLIDER_BYNAME(vr_retrolight_edge_soft, "Block Edge Softness", 0.f, 3.f, 0.25f, "%.2f px", noExt,
    "The blocks' edges (the lightmap's, the dynamic lights', the shadows'): a ramp this many pixels wide, so they "
    "don't crawl as your head moves. 0: hard nearest (crawls). Blocks smaller than a pixel turn smooth.")
QVR_CYCLE_BYNAME(vr_retrolight_spacing, "Level Spacing",
    "How the light levels below are spread: evenly in light, as Quake's colormap (few levels crush the shade to "
    "black), or evenly in brightness (more levels in the shade, the bands look even).",
    {0.f, "Even in Light (Quake's)"}, {1.f, "Even in Brightness"})
QVR_TOGGLE_BYNAME(vr_retrolight_world, "World",
    "Retro lighting on the map and its brush models (doors, lifts, boxes).")
QVR_SLIDER_BYNAME(vr_retrolight_world_steps, "Light Levels", 0.f, 64.f, 1.f, "%.0f", noExt,
    "The baked light in this many levels a unit of the lightmap (bands). 0: smooth. 64: Quake's software "
    "renderer's own 64 levels (subtle); 8 to 16: clear bands. Each colour keeps its hue.")
QVR_SLIDER_BYNAME(vr_retrolight_world_soft, "Level Edges Softness", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The bands' edges blurred over this share of a level: 0 hard (anti-aliased over a pixel), 1 smooth steps.")
QVR_SLIDER_BYNAME(vr_retrolight_world_dither, "Dither", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "An ordered (Bayer) dither between the levels, fixed to the texels: the bands break into a pattern "
    "instead of a hard line. Fades out where its cells get smaller than a pixel.")
QVR_TOGGLE_BYNAME(vr_retrolight_world_lightmap, "Blocky Lightmap",
    "The lightmap read on its grid of blocks instead of smoothly: Quake's chunky light.")
QVR_CYCLE_BYNAME(vr_retrolight_world_luxel, "Lightmap Block",
    "The blocks, in Quake texels: 16 is a luxel (GLQuake with nearest filtering: big squares); 1 lights each "
    "texel on its own (Quake's software renderer: soft gradients in texel steps, aligned with the textures).",
    {1.f, "1 texel (software Quake)"}, {2.f, "2 texels"}, {4.f, "4 texels"}, {8.f, "8 texels"},
    {16.f, "16 texels (a luxel)"})
QVR_SLIDER_BYNAME(vr_retrolight_world_dyn_steps, "Dynamic Light Levels", 0.f, 64.f, 1.f, "%.0f", noExt,
    "The flashlight, muzzle flashes, explosions and the other dynamic lights in this many levels a unit "
    "(their pools in rings). 0: smooth.")
QVR_TOGGLE_BYNAME(vr_retrolight_models, "Models",
    "Retro lighting on models: monsters, items, weapons, your body.")
QVR_SLIDER_BYNAME(vr_retrolight_model_steps, "Model Light Levels", 0.f, 64.f, 1.f, "%.0f", noExt,
    "A model's own light (the light grid's, per vertex) in this many levels a unit (as the world's). 0: smooth.")
QVR_SLIDER_BYNAME(vr_retrolight_model_soft, "Model Level Edges Softness", 0.f, 1.f, 0.05f, "%.2f", noExt, "As the world's.")
QVR_SLIDER_BYNAME(vr_retrolight_model_dither, "Model Dither", 0.f, 1.f, 0.05f, "%.2f", noExt, "As the world's, fixed to the skin's texels.")
QVR_SLIDER_BYNAME(vr_retrolight_model_dyn_steps, "Model Dynamic Light Levels", 0.f, 64.f, 1.f, "%.0f", noExt, "As the world's.")
QVR_CYCLE_BYNAME(vr_retrolight_shadow_filter, "Shadow Edges",
    "Hard: one tap, no soft filtering. Blocky: the shadow map's own texels, as squares.",
    {0.f, "As Graphics > Shadows"}, {1.f, "Hard"}, {2.f, "Blocky"})
QVR_SLIDER_BYNAME(vr_retrolight_shadow_steps, "Shadow Levels", 0.f, 8.f, 1.f, "%.0f", noExt,
    "A shadow's darkness in levels: 1, lit or not (a hard edge along its middle); 2 or more, steps. 0: smooth.")
QVR_SLIDER_BYNAME(vr_retrolight_shadow_soft, "Shadow Level Softness", 0.f, 1.f, 0.05f, "%.2f", noExt, "Those steps' edges blurred over this share of a level.")

// vr_menu_physics.inc: pageThrowing
QVR_SLIDER(vr_2h_throw_velocity_mult, "Two-Hand Throw Speed", 0.5f, 3.f, 0.1f, "%.1fx", ext(), nullptr)
QVR_TOGGLE(vr_throw_release, "Analog Release",
    "A throw lets go as the grip starts to open, not only once it is released.")
QVR_SLIDER(vr_throw_slowmo_stroke_time, "Slow Motion: Full Stroke Tempo", 0.04f, 0.2f, 0.005f, "%.3f s", ext(),
    "In slow motion (bullet time), a quick toss, flick or nudge goes as far as you made it, and a full "
    "throw made slowly with the slowed world goes as far as at full speed: the slower your stroke against "
    "a real-speed one of its length, the more it is raised. This is a full arm throw's stroke at real "
    "speed (its path over its peak speed). Lower: slowed throws reach full strength sooner, real-speed "
    "throws in bullet time are raised a little more.")
QVR_TOGGLE(vr_throw_slowmo_real_strength, "Slow Motion: Throws As In Real Time",
    "In slow motion, every throw is as strong as the same motion at full speed: never raised. A throw "
    "you make slowly with the slowed world then goes as slowly as you made it. Off: raised by the "
    "stroke's tempo (Full Stroke Tempo).")
QVR_TOGGLE(vr_throw_spin_from_pose, "Spin From Controller Turn",
    "A throw's spin from how the controller turned, not the runtime's angular velocity (Virtual Desktop "
    "reports it in the controller's frame: a flick facing away from the play space's front spun throws "
    "sideways).")
QVR_CYCLE(vr_angvel_frame, "Controller Spin Frame",
    "Which frame the runtime's controller angular velocity is in (melee, flick reload, throws). Auto: "
    "Virtual Desktop's controller frame, else the tracking space, checked against how the controllers "
    "turn. Tracking Space: as before. From Turn: ignore it, use the controller's turn.",
    {-1.f, "Auto"}, {0.f, "Tracking Space"}, {1.f, "Controller's"}, {2.f, "From Turn"})
QVR_SLIDER(vr_throw_wrist_dist, "Wrist to Controller", 0.f, 0.15f, 0.005f, "%.3f m", noExt,
    "From the wrist to the controller's point: the turn of the hand about the wrist gives the controller "
    "this lever's speed, the part of a throw a heavy thing keeps less of.")
QVR_SLIDER(vr_throw_spin_drag, "Spin Drag", 0.f, 2.f, 0.05f, "%.2f", ext(), nullptr)
QVR_TOGGLE(vr_axestick, "Axes Stick",
    "A thrown axe that strikes blade first sticks in walls, doors, props and monsters (it moves with them). "
    "Grip it, or force grab it (it tugs, then comes free), to pull it out. Off: it bounces off.")
QVR_TOGGLE(vr_prop_impact_damage, "Flung Props Hurt",
    "A prop you didn't throw (swung on the grapple's rope, batted, knocked flying) hurts the monster it flies "
    "into fast enough. Off: only what you throw by hand.")
QVR_TOGGLE(vr_prop_impact_players, "Flung Props Hurt Players",
    "They hurt players too (never you as you let go of it, bat it or while your grapple holds it). Off: "
    "monsters only.")
QVR_SLIDER(vr_gib_spawn_harmless, "Fresh Gibs Harmless For", 0.f, 2.f, 0.1f, "%.1f s", noExt,
    "A gib, head or small gib just torn out can't hurt you or a monster as a flung prop for this long (gibbing a "
    "body you stand on). Thrown by you after it, it hurts as ever.")
QVR_SLIDER(vr_prop_drop_grace, "Monster Drops Harmless For", 0.f, 2.f, 0.1f, "%.1f s", noExt,
    "A weapon or a backpack a monster drops as it dies can't hurt players for this long (it may land on "
    "you); after it, see Monster Drops Hurt Only Falling.")
QVR_TOGGLE(vr_prop_drop_falls_only, "Monster Drops Hurt Only Falling",
    "After that, a monster's weapon or backpack hurts you only falling on you or once a hand has thrown it: "
    "never pushed or tossed into you from under you (a monster killed at your feet). Off: as any flung prop.")
QVR_TOGGLE(vr_prop_drop_pass_inside, "Monster Drops Pass Through Bodies",
    "A monster's drop made inside you or a monster (killed at your feet) passes through it until clear, "
    "instead of being pushed out hard.")
QVR_SLIDER(vr_prop_impact_min_speed, "Least Speed", 2.f, 20.f, 0.5f, "%.1f m/s", ext(0.f, 50.f),
    "How fast it must fly into what it hits to hurt it: less for things over 10 kg. The damage grows with "
    "the speed over it.")
QVR_SLIDER(vr_prop_impact_mult, "Damage", 0.f, 3.f, 0.05f, "%.2fx", ext(0.f, 10.f),
    "All their damage: the throw's (Thrown Box Damage, a gib's, a weapon's) at the least speed and the "
    "reference mass.")
QVR_SLIDER(vr_prop_impact_speed_max, "Most Speed Multiplier", 1.f, 6.f, 0.25f, "%.2fx", ext(0.f, 20.f),
    "Faster hits hurt more, up to this many times the damage at the least speed.")
QVR_SLIDER(vr_prop_impact_weight_curve, "Weight Curve", 0.f, 1.5f, 0.05f, "%.2f", ext(0.f, 3.f),
    "How much mass changes the damage: 0 not at all, 1 in proportion (twice the mass, twice the damage).")
QVR_SLIDER(vr_prop_impact_weight_ref, "Reference Mass", 0.5f, 20.f, 0.5f, "%.1f kg", ext(0.1f, 100.f),
    "A prop of this mass deals the damage as set; lighter ones less, heavier ones more.")
QVR_SLIDER(vr_prop_impact_weight_max, "Most Weight Multiplier", 1.f, 5.f, 0.1f, "%.1fx", ext(1.f, 20.f), "The most a heavy prop multiplies the damage.")
QVR_SLIDER(vr_prop_impact_min_mass, "Least Mass", 0.f, 10.f, 0.1f, "%.1f kg", ext(0.f, 100.f),
    "Lighter props never hurt when flung (thrown by hand, they still do).")
QVR_SLIDER(vr_shot_push, "Shot Push", 0.f, 3.f, 0.1f, "%.1fx", ext(0.f, 10.f),
    "Pellets, nails and the lightning beam push the boxes, gibs and weapons they hit or pass through (monsters' "
    "shots too): times every push below. 0: off. Explosions push things anyway.")
QVR_TOGGLE(vr_button_throw, "Thrown Things Press Buttons",
    "A weapon, rock, box or gib thrown at a wall button presses it. Buttons you must shoot are still shot.")

// vr_menu_physics.inc: pageCarrying
QVR_TOGGLE(vr_carry, "Carry Ammo and Health",
    "Grip a box or a backpack to carry it, push it with a hand or gun. Off: touching takes it.")
QVR_CYCLE(vr_carry_take, "Take a Box",
    "At a holster: let go of it at a hip or shoulder holster to put it in your pack.",
    {0.f, "At a holster"}, {1.f, "Trigger"}, {2.f, "Either"})
QVR_CYCLE(vr_carry_autocollect, "Auto-Collect",
    "An ammo or health box (and a backpack) is taken the moment a hand grips it or a force grab brings it "
    "in, seen shrinking into the hand. Full up of it: the full cue, and it stays in the hand.",
    {0.f, "Off"}, {1.f, "Ammo and Health"}, {2.f, "And Backpacks"})
QVR_TOGGLE(vr_carry_full_cue, "Full Cue",
    "A box put away (or taken at once) while you are full up of it: a dull knock and a tick in the hand, "
    "so it is not taken for a miss of the holster. It drops there.")
QVR_TOGGLE(vr_collect_fx, "Put-Away Transition",
    "What you put away (a box or backpack at a holster, a key, a rune or a power-up, an ammo box or a round "
    "at the ammo pouch, a grenade at its pouch) is seen shrinking into the holster or pouch, following you, "
    "instead of vanishing. Only how it looks: it is yours the moment you let go, as before.")
QVR_SLIDER(vr_collect_fx_time, "Put-Away Time", 0.05f, 0.6f, 0.05f, "%.2f s", ext(0.f, 2.f), "How long it takes to go in (slowed in bullet time).")
QVR_SLIDER(vr_collect_fx_size, "Put-Away End Size", 0.05f, 1.f, 0.05f, "%.2fx", noExt, "How small it is as it goes in: its size then, of its own.")
QVR_TOGGLE(vr_weapon_grab_drawn, "Weapons by the Fist",
    "A weapon lying on the floor is taken where the fist touches it: a crowbar by its bar, a sword by its "
    "blade. Off: only with the hand at its handle.")
QVR_SLIDER(vr_weapon_grab_slack, "Weapon Grab Slack", 0.f, 10.f, 0.5f, "%.1f cm", noExt,
    "With Weapons by the Fist: a weapon lying on the floor is taken with the fist this far off it too (a gun "
    "lying flat is thinner than the lowest your fist gets over it). Not one in the air: caught by the fist on it.")
QVR_SLIDER(vr_weapon_grab_handle_leniency, "Handle Grab Leniency", 0.f, 10.f, 0.5f, "%.1f cm", ext(0.f, 20.f),
    "A gun lying on a table or the floor is taken by its handle with your fist this much farther off it than "
    "the slack above (the hand at its handle; your hand nudging it doesn't stop it). Only a direct grab: a "
    "weapon caught in the air has Mid-Air Catch Leniency, and a force grab is unchanged. 0: as anywhere on it.")
QVR_SLIDER(vr_weapon_catch_handle_leniency, "Mid-Air Catch Leniency", 0.f, 15.f, 0.5f, "%.1f cm", noExt,
    "A weapon in the air (tossed up to re-grip it, flipped, spinning) is caught by its handle with your fist "
    "this much off it. Not a force grab's pull. 0: only by your fist on it.")
QVR_TOGGLE(vr_pickup_float_grace, "Floating Pickup Grace",
    "A weapon, key or armour hanging in the air (spinning) isn't knocked down by your empty hand reaching into "
    "it slowly for a moment: close your grip and it is taken where it hangs. Off: a touch knocks it loose.")
QVR_TOGGLE(vr_weapon_grab_hotspots, "Weapons by Their Hotspots",
    "Grip a weapon on the floor nearer one of its other grips than its handle (a chainsaw's front handle, "
    "a crowbar's bar, a sword's blade, a gun's pump) and you carry it by that grip, as when your other hand "
    "lets go of a weapon held in both: grip its handle with the other hand to use it. Not a pistol's "
    "two-handed cup grip. A force grab always takes the handle. Off: always by the handle.")
QVR_TOGGLE(vr_weapon_grab_anywhere, "Weapons Anywhere",
    "Grip a weapon anywhere away from its handle and its other grips and you hold it there, as a box or a gib: "
    "off the floor as it lies, with your other hand on one you hold (Other Hand Anywhere), with both hands "
    "on one you carry. Held off its handle it doesn't fire, but strikes and throws; grip its handle to use "
    "it. Holstered, it is drawn by its handle. Off: only by the handle and the other grips.")
QVR_SLIDER(vr_weapon_grab_anywhere_min, "Anywhere: Away From Grips", 0.f, 30.f, 1.f, "%.0f cm", noExt,
    "How far from a weapon's handle and its other grips (a foregrip, a pump, a blade) you must grip it to hold "
    "it anywhere: nearer, the handle or that grip takes it, so they stay easy to take.")
QVR_CYCLE(vr_weapon_anygrip_mode, "Other Hand Anywhere",
    "Your other hand gripping a weapon you hold by its handle, away from its grips. As a Foregrip: both hands "
    "aim it, from wherever it holds it. Support Only: it bears the weight, the aim stays your weapon hand's. "
    "Rigid: the weapon follows both hands as a box held in both. Each weapon may override it (Weapon "
    "Offsets: Other Hand Anywhere).",
    {0.f, "As a Foregrip"}, {1.f, "Support Only"}, {2.f, "Rigid"})
QVR_TOGGLE(vr_carry_local, "Drawn In the Hand",
    "What you carry is drawn in your hand as it is this frame: no lag or lead as you walk or turn. Off: where the server has it.")
QVR_TOGGLE(vr_carry_two_hands, "Two-Handed Carrying",
    "Grip what your other hand carries to hold it in both: it moves and turns with both hands, and letting go "
    "of both together throws it. Let go with one and the other keeps it (to pass it from hand to hand).")
QVR_TOGGLE(vr_carry_two_hands_solid, "Two-Handed Stops at Walls",
    "What you hold in both hands stops at walls, floors and doors however hard you push, and slides along "
    "them; it keeps its turn where a new one would put it in. Off: it goes where your hands put it.")
QVR_TOGGLE_BYNAME(vr_carry_body_collision, "Carried Things Stop at Your Body",
    "Things you carry stop at your torso. Your hands follow their grips as you press them against it.")
QVR_SLIDER_BYNAME(vr_carry_move_mass, "Carry Load: Mass", 0.f, 300.f, 10.f, "%.0f kg", noExt,
    "Carried mass that halves stick walking speed. Both hands are counted; 0 disables the mass penalty.")
QVR_SLIDER_BYNAME(vr_carry_move_size, "Carry Load: Size", 0.f, 3.f, 0.05f, "%.2f m", noExt,
    "Total carried length allowed without a bulk penalty; twice this halves stick walking speed. 0 disables the size penalty.")
QVR_SLIDER_BYNAME(vr_carry_move_min, "Carry Load: Minimum Speed", 0.1f, 1.f, 0.05f, "%.2fx", noExt,
    "Minimum fraction of stick walking speed under load; 1 disables the slowdown.")
QVR_TOGGLE(vr_held_surface_fit, "Fit to the Hand",
    "A box, backpack or gib you grip sits against your curled fingers, by its drawn shape. Off: it stays where you gripped it.")
QVR_TOGGLE(vr_grip_fit, "Keep Fingers Out of Props",
    "Once, as you grip a prop (or take it with your other hand too): if your drawn hand (palm, back, wrist, "
    "fingers as they will grip) would sink into it, it is moved out of your hand the least way that leaves the "
    "hand resting on its surface, and kept there while you hold it. Not for weapons, nor props with a Fixed or "
    "Along the Handle grip.")
QVR_TOGGLE(vr_held_collide, "Held Things Collide",
    "Things you hold one in each hand, or a thing in one hand and a weapon in the other, bump into each other "
    "instead of passing through: each hand is drawn moved back with what it holds, a short buzz as they meet. "
    "Your real hands are never held back.")
QVR_TOGGLE(vr_hand_collide_props_keep_out, "Empty Hand Stays Out of Held Props",
    "With Empty Hand Against Held Things: an empty hand pushed into a prop your other hand holds is drawn "
    "resting on its surface however deep you push, and gripping it there takes it in both hands from the "
    "outside. Hitting it and throwing it are your real hand's. Off: past the push the hand sinks in, as "
    "with a weapon.")
QVR_TOGGLE(vr_hand_collide_fingers, "Fingers Rest on Held Things",
    "With Empty Hand Against Held Things: the empty hand's fingers rest on what the other hand holds or bend "
    "out of it, instead of the whole hand being pushed by its fingertips.")
QVR_TOGGLE(vr_gun_wall_slide, "Weapons Slide Along Walls",
    "A weapon you hold into a wall, a floor or a table slides along it: lowered onto a table it rests on the top, "
    "pushed into a wall it is held off along the wall. Off: the old push-back, the hand moved back along the aim "
    "until the muzzle stops at the wall.")
QVR_TOGGLE(vr_held_collide_walls, "Held Things Stop at Walls",
    "A thing you hold in one hand stops at walls, floors and doors: it and your hand are drawn held out of them, "
    "a short buzz as it touches. Looks only: your real hand is never held back.")
QVR_TOGGLE(vr_model_collide_rest, "Things Rest on Hands",
    "A thing lying on your open palm, fist or gun (let go of there, or balanced) never pushes the drawn hand away: "
    "the hand stays under it. Off: it pushes the hand out of it as other things do.")
QVR_TOGGLE(vr_held_collide_monsters, "Held Things Stop at Monsters",
    "A thing you hold in one hand and swing as a club stops at the monster's body, as weapons do (held out "
    "up to vr_model_collide_max, 20 cm), instead of passing through it. It still hits, and is never dropped for "
    "it.")
QVR_TOGGLE(vr_box3d_hand_props, "Hands Push and Hold Things",
    "Your empty hands are solid to loose things: they push them, and what you let go of on an open palm turned "
    "up stays there. Grenades pass through (your palm catches them).")
QVR_TOGGLE(vr_box3d_hand_push_fist, "Push Boxes With the Fist",
    "Your hands push explosive boxes where the drawn fist meets them. Off: a small ball at the hand's point, "
    "which met a box before the fist did.")
QVR_TOGGLE(vr_box3d_weapon_push, "Weapons Push Things",
    "The weapons you hold are solid to loose things as drawn: shove things with a gun, balance them on it, bat a "
    "grenade away with the axe.")
QVR_CYCLE(vr_box3d_gun_pieces, "Guns' Shape",
    "The guns' solid shape, held and lying about: in convex pieces that follow the drawn gun (a shell drops "
    "into the shotgun's port, a magazine into its well, nothing rests on the air between its parts), or one "
    "hull round each (which fills them). For guns made from then on (picked up, dropped).",
    {1.f, "One Hull"}, {6.f, "6 Pieces"}, {12.f, "12 Pieces"}, {20.f, "20 Pieces"})
QVR_SLIDER(vr_box3d_throw_grace, "Throw Grace", 0.f, 0.5f, 0.05f, "%.2f s", ext(0.f, 2.f),
    "How long a thing you throw passes through your hands (and the weapon you hold) as it leaves them: it keeps "
    "the direction and spin you threw it with instead of being knocked by the hand still moving. 0: off.")
QVR_TOGGLE(vr_box3d_throw_grace_body, "Throw Grace for the Body",
    "During Throw Grace the thing also passes through your body (an overhead wrist snap throwing it down "
    "across you).")
QVR_TOGGLE(vr_box3d_press_back, "Pressing Props: Hand Gives",
    "A hand, a weapon or a held thing pressed into a crate that has nowhere to go (on the floor, against a wall) "
    "stops on its surface, drawn there too, instead of pushing the crate into the floor (where it was squeezed "
    "out and flew off or broke).")
QVR_SLIDER(vr_throw_inherit, "Throws Keep Your Motion", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much of your own velocity a thing or weapon you throw takes with it (1: all: a brick thrown running "
    "forward flies further, running back shorter). 0: none.")
QVR_SLIDER(vr_carry_melee_mult, "Box Punch Damage", 1.f, 3.f, 0.1f, "%.1fx", ext(), "Punching with a box in hand.")
QVR_SLIDER(vr_physsound, "Physics Sounds", 0.f, 2.f, 0.1f, "%.1f", noExt,
    "Volume of what things say as physics moves them: knocks as they land, bounce and hit each other, "
    "scrapes as they slide, a climbing hand taking a hold (0 off; over 1 louder than recorded, up to 2). By what they are made of: wood (crates, "
    "torches), metal (weapons, ammo, armour), stone, brick, flesh (gibs, heads), a backpack's thud.")
QVR_TOGGLE(vr_explobox_physics, "Physics Explosive Boxes",
    "The explosive boxes can be pushed, tipped over, stacked and carried by hand (heavy; never force "
    "grabbed). Off: fixed in place, as in id's Quake. Next map.")
QVR_SLIDER(vr_explobox_impact, "Blow Up On Impact", 0.f, 30.f, 1.f, "%.0f m/s", ext(0.f, 100.f),
    "A box hitting something this hard blows up: dropped from about 85 units or more (tipped over, its "
    "top lands at 11 m/s). 0: never.")
QVR_CYCLE(vr_armor_wear, "Armour",
    "Wear by hand: grip the armour to carry it and let go of it over your chest to put it on (only if it is better "
    "than yours). Walking over it no longer takes it. Next map.",
    {0.f, "Touch takes it"}, {1.f, "Wear by hand"})
QVR_CYCLE(vr_item_objects, "Weapons and Keys",
    "Objects: the map's weapons, keys, runes and suits hang spinning until you grab, knock or force-grab them, "
    "then they are physics objects. A weapon you grip is yours at once; keys, runes and suits you carry to a "
    "holster and let go of there. Powerups are as before. Next map.",
    {0.f, "Touch takes them"}, {1.f, "Objects"})
QVR_CYCLE(vr_pickup_prop_models, "Weapon Pickups Look",
    "As held and dropped: the map's weapons are drawn with the same models as the weapons in your hands and "
    "the ones you drop, at their size, spinning about their middle. Classic models: id's pickup models (g_*.mdl). A weapon "
    "without a model of its own keeps the classic one. Next map.",
    {0.f, "Classic models"}, {1.f, "As held and dropped"})

// vr_menu_physics.inc: pageWallTorches
QVR_TOGGLE(vr_walltorch, "Take Torches Off Walls",
    "Grip a wall torch to take it immediately (or force grab it): it is a burning club, lighting the "
    "room round you as the wall torch did. Held, it burns for ever; dropped, thrown or used up by blows, "
    "its fire dies. Grip and fingers: Held Object Offsets, holding it. Off: fixed, as in id's Quake. Next map.")
QVR_SLIDER(vr_walltorch_grab_time, "Grab Window", 0.f, 1.5f, 0.05f, "%.2f s", ext(0.f, 3.f),
    "A grip closed on the way to a torch on its wall, still held, takes it as your hand gets there, up to "
    "this long after you closed it (not a punch: a fast fist goes through). 0: only a grip closed there.")
QVR_TOGGLE(vr_walltorch_forcegrab, "Force Grab Torches",
    "Torches come off their walls, and from where they lie, to a force grab.")
QVR_SLIDER(vr_walltorch_die_time, "Dying Time", 1.f, 30.f, 0.5f, "%.1f s", ext(0.f, 120.f),
    "How long a dying torch's fire takes to go out: dropped, thrown or out of blows. Picked up again before "
    "it is out, a dropped torch burns up again; one out of blows goes on dying.")
QVR_TOGGLE(vr_walltorch_relight, "Light Again",
    "A dying or burnt-out torch held in another torch's flame (on a wall or in your other hand), in one of "
    "the map's flames (braziers, flame balls) or dipped in lava lights again, as new.")
QVR_TOGGLE(vr_walltorch_shot, "Shot Off Its Wall",
    "A wall torch hit by anything you shoot (a shotgun's pellets, a nail, a rocket, a grenade, a laser) or "
    "by a thing you throw into it hard comes off its wall: it falls lit, and its light on the wall is gone. "
    "Your hand and a punch take it as before. Off: shots go through it, as in id's Quake. Next map.")
QVR_TOGGLE(vr_walltorch_shadows, "Taken Torch Casts Shadows",
    "A taken torch's light casts shadows (your hands and body, what is round you), whatever Graphics' Torch "
    "Light Shadows says. Its brightness is the wall torch's: Graphics' Torch Light Brightness.")

// vr_menu_physics.inc: pageArcaneDimensions
QVR_CYCLE(vr_ad_projectiles, "Projectile Rules",
    "Whose rules shotguns and monster projectiles play by. AD Maps Only: Arcane Dimensions' in its maps "
    "(projectile pellets you see fly, from your shotguns and the grunts'; the hell knights', scrags', ogres' "
    "and zombies' missiles at its speeds for the skill), Quake's elsewhere. Off: Quake's everywhere. "
    "Everywhere: its rules wherever its data is loaded. Its own monsters always play by its rules.",
    {1.f, "Off"}, {0.f, "AD Maps Only"}, {2.f, "Everywhere"})
QVR_SLIDER(vr_ad_pellet_spread, "Pellet Spread", 0.f, 2.f, 0.05f, "x%.2f", ext(0.f, 4.f),
    "Arcane Dimensions' pellets: how wide they spread, times its own (the shotgun 0.04, the super shotgun "
    "0.14 by 0.08: wider than Quake VR's hitscan, aimed with the hand). 0: every pellet straight.")
QVR_SLIDER(vr_ad_pellet_speed, "Player Pellet Speed", 0.25f, 3.f, 0.05f, "x%.2f", ext(0.1f, 10.f),
    "How fast your pellets fly, times Arcane Dimensions' (the shotgun 2000, the super shotgun 1500 units a "
    "second): faster is nearer hitscan.")
QVR_SLIDER(vr_ad_enemy_pellet_speed, "Enemy Pellet Speed", 0.25f, 3.f, 0.05f, "x%.2f", ext(0.1f, 10.f),
    "Enemy shotgun pellets' speed multiplier, independent of yours. Applies to Quake grunts using AD projectile rules and AD enemies in any map.")
QVR_SLIDER(vr_ad_pellet_damage, "Pellet Damage", 0.f, 3.f, 0.05f, "x%.2f", ext(0.f, 10.f),
    "Your pellets' damage, times Weapon Damage's shotgun and super shotgun pellet (4: Arcane Dimensions').")
QVR_CYCLE(vr_ad_ammo_models, "Ammo Models",
    "AD's custom ammo boxes while its data is loaded. Off: original Quake boxes. Newly spawned ammo only; reload the map to change existing boxes. Ammo counts, held weapons and VR pickup physics stay the same.",
    {0.f, "Off"}, {1.f, "AD Maps Only"}, {2.f, "Everywhere"})
QVR_CYCLE(vr_ad_resistance, "Ammo Resistance",
    "AD ammo resistance and vulnerability on entities with resistance keys. Everywhere preserves AD monsters' resistance outside AD maps; ordinary enemies without keys stay unchanged. Takes effect immediately.",
    {0.f, "Off"}, {1.f, "AD Maps Only"}, {2.f, "Everywhere"})
QVR_CYCLE(vr_ad_upgrades, "Weapon Upgrades",
    "Shadow Axe, Widowmaker and Plasma Gun upgrade effects and new pickups. Owned upgrades are preserved while Off. "
    "With AD Content: in AD maps, and in any map while Additional Content is Everywhere (their pickups placed "
    "there or spawned with the toolgun). Everywhere: in any map regardless. No automatic replacement of ordinary guns.",
    {0.f, "Off"}, {1.f, "With AD Content"}, {2.f, "Everywhere"})
QVR_TOGGLE(vr_ad_evil, "Evil Skill (with Nightmare)",
    "Arcane Dimensions' fifth skill, in any campaign while its data is installed: chosen with Nightmare (the "
    "start map's skill hall, or Arcane Dimensions' own Evil choice, which sets this). 50 health at most "
    "(megahealth 100), the Shadow Axe's blows, Arcane Dimensions' monsters at their hardest. Kept in saves.")
QVR_SLIDER(vr_ad_tints, "Screen Tints", 0.f, 1.f, 0.05f, "x%.2f", noExt,
    "Arcane Dimensions' screen colours, times this: green while poisoned, dark amber while bleeding, "
    "orange while burning (with Burn Damage off), a book's dark background while you read it. Eased in and out. 0: none. Its "
    "cinematic cameras' fades to black always play (Graphics' colour shift settings apply to all).")
QVR_TOGGLE(vr_climb_ladders, "Hand-Climbed Ladders",
    "Arcane Dimensions' ladders climbed with your hands, as ledges (Hand Climbing on): grip an actual "
    "rung, pull yourself up hand over hand, mantle at its top; a ladder with no rung within reach (a flat "
    "ladder wall) is held anywhere on it. While hands climb ladders, keyboard ladder movement and jump "
    "climbing are off; with this or Hand Climbing off, ladders climb Arcane Dimensions' way (hold jump).")
QVR_CYCLE(vr_ad_everywhere, "Additional Content",
    "Arcane Dimensions' monsters, new artifacts, custom keys, health vials and gems, custom items, ladders and "
    "weapons (the Shadow Axe, Widowmaker and Plasma Gun with Weapon Upgrades on; its soldiers' guns) in any "
    "map that places them (custom maps, the toolgun), while its data is installed. AD Maps Only: only in its own "
    "maps. Its pickups that would replace Quake VR's (backpacks) are only ever in its maps. Disabling required monsters or keys in AD maps would prevent progress, so they remain available there.",
    {0.f, "AD Maps Only"}, {1.f, "Everywhere"})

// vr_menu_physics.inc: pageBurning
QVR_TOGGLE(vr_burn_corpses, "Corpses Burn",
    "Corpses catch fire too, and a monster that dies burning burns on as a corpse. Corpses are the ones "
    "Gibs and Corpses' Gib Corpses keeps.")
QVR_TOGGLE(vr_burn_lava_bodies, "Bodies Burn in Lava",
    "Ragdolls, knocked-down monsters and corpses in lava catch fire there, and burn on in it.")
QVR_SLIDER(vr_burn_lava_gib, "Burnt Through in Lava", 0.f, 15.f, 0.5f, "%.1f s", ext(0.f, 60.f),
    "A body this long in lava bursts into gibs (a knocked-down one is killed). 0: never.")
QVR_TOGGLE(vr_burn_touch, "Torch Touch",
    "A lit torch, held or thrown, sets a monster or a corpse on fire just by touching it: no blow needed. "
    "Mapper flames touching your body burn you in the same way as a dropped lit torch. "
    "Off: a lit torch's blow, or one thrown into it.")
QVR_TOGGLE(vr_burn_lava_nails, "Lava Nails",
    "Your lava nails (the nailgun's and super nailgun's lava ammo) set what they hit on fire.")
QVR_TOGGLE(vr_burn_nail_convert, "Nails Through a Flame",
    "A nail fired through a fire becomes a lava nail: more damage, and it sets what it hits on fire. A lit "
    "torch's flame (in your other hand, thrown, lying or on its wall), the map's flames, braziers and "
    "candles, the toolgun's flames, a lava ball.")
QVR_TOGGLE(vr_burn_nail_burning, "Nails Through Burning Things",
    "A burning monster's, corpse's or crate's flames turn nails into lava nails too (Nails Through a "
    "Flame on).")
QVR_SLIDER(vr_burn_nail_sound, "Nail Sizzle Volume", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "Volume of the short fizz a nail makes as it catches fire in a torch's flame, and of fire hissing out "
    "in water (0 off).")
QVR_TOGGLE(vr_burn_map_monsters, "Map Flames Burn Monsters",
    "Monsters awake, ragdolls and corpses touching the map's flames (flames, braziers, wall torches, the "
    "toolgun's) catch fire, as you do.")
QVR_TOGGLE(vr_burn_water_douse, "Water Puts Fire Out",
    "A flamethrower's flames going into water or slime go out in a puff of steam; a lava nail going in "
    "becomes a plain nail.")
QVR_TOGGLE(vr_burn_crates, "Crates Burn",
    "Wooden crates catch fire: a lit torch's blow or touch, a lava nail, a burning crate touching them.")
QVR_TOGGLE(vr_burn_lava, "Wood Burns in Lava",
    "Crates, barrels and their pieces dropped in lava catch fire there, and burn on in it.")
QVR_TOGGLE(vr_burn_crate_break, "Burnt Crates Break",
    "A crate that has burnt its time breaks into its pieces. Off: its fire goes out.")
QVR_TOGGLE(vr_burn_crate_char, "Burnt Crates' Pieces Charred",
    "A crate that burns through breaks into charred, blackened pieces that don't burn again. Off: plain pieces, "
    "which burn as any other.")
QVR_TOGGLE(vr_burn_pieces, "Pieces Burn",
    "The pieces of a crate you break catch fire as crates do: a lit torch's blow or touch, a lava nail, burning "
    "crates and pieces near them. A crate broken while burning scatters burning pieces.")
QVR_SLIDER(vr_walltorch_drips, "Upside Down: Burning Drips", 0.f, 20.f, 1.f, "%.0f / s", ext(0.f, 60.f),
    "Burning drips falling off the head of a torch held head down, a second (0: none).")
QVR_TOGGLE(vr_burn_self, "Its Flame Burns You",
    "The flame of the torch you hold, kept on your other hand, an arm, your body or your head, sets you on fire "
    "there after a moment (Catch Fire After), as it does a monster: flames on you, burns on your skin, Burn "
    "Damage. Till then the hand buzzes, harder as it comes. Mapper flames touching your hands or arms use "
    "the same warning and delay.")
QVR_SLIDER(vr_burn_self_time, "Catch Fire After", 0.f, 3.f, 0.1f, "%.1f s", ext(0.f, 10.f),
    "How long the flame must stay on you before you catch fire (off it, it eases back twice as fast).")
QVR_TOGGLE(vr_burn_drop, "Upside Down Burns Your Hand",
    "Held head down, a torch's flame climbs the stick to your hand: kept there a moment (Drop After), it burns "
    "your hand and you drop the torch.")

// vr_menu_physics.inc: pageRocksBricks
QVR_TOGGLE(vr_debris, "Rocks and Bricks",
    "Rocks lie on natural ground and at the foot of rock and stone walls, bricks at the foot of brick walls, "
    "by what the textures are: pick them up, force grab them, punch with them, throw them. The same "
    "places at every load. Single player. Next map.")
QVR_TOGGLE(vr_debris_rocks, "Rocks",
    "Rocks on grass, dirt and rock, and by rock and stone walls. Next map.")
QVR_TOGGLE(vr_debris_bricks, "Bricks", "Bricks at the foot of brick walls. Next map.")

// vr_menu_physics.inc: pageCrates
QVR_TOGGLE(vr_crates, "Crates",
    "Wooden crates, small and large, stand by the walls and in corners, some stacked two high: push them, "
    "carry them, stand on them, break them. The same places at every load. Single player. Next map.")
QVR_SLIDER(vr_crate_health, "Health", 0.f, 100.f, 5.f, "%.0f", ext(0.f, 1000.f),
    "A small crate's (a large one's 1.6 times): shots, blows, explosions and hard hits wear it down. A shotgun "
    "blast does up to 24, the axe about 20. 0: they never break.")
QVR_SLIDER(vr_prop_break_speed, "Break Speed", 2.f, 20.f, 0.25f, "%.2f m/s", ext(0.1f, 60.f),
    "A hand's or weapon's blow on a small crate, or one held and smashed into a wall, the floor or a monster, "
    "this fast breaks it at once (no real blow is: two strong ones do); from Blows Wear From of it it is "
    "damaged (more the faster), slower it is only pushed. Every prop that breaks follows it: explosive boxes "
    "(they blow up), barrels, AD's and the expansions' breakables; each by its health (the square root of it "
    "over a small crate's) and its material (below). A weapon's heft helps (the axe 1.4x).")
QVR_SLIDER(vr_prop_blow_wear, "Blows Wear From", 0.1f, 0.9f, 0.05f, "%.2fx", ext(0.05f, 0.95f),
    "How much of Break Speed a blow (or a prop held and smashed) needs to damage a prop, a little at first, "
    "more the faster. 0.25: a strong punch (12 m/s) takes two to break a crate, a medium one (9) three or "
    "four, a light one (5) barely marks it. Thrown and dropped props are damaged from half their own break "
    "speeds.")
QVR_SLIDER(vr_prop_break_wood, "Break Speed: Wood", 0.25f, 3.f, 0.05f, "%.2fx", ext(0.01f, 10.f),
    "Times Break Speed for crates, barrels and wooden breakables.")
QVR_SLIDER(vr_prop_break_glass, "Break Speed: Glass", 0.25f, 3.f, 0.05f, "%.2fx", ext(0.01f, 10.f),
    "Times Break Speed for glass and pottery (AD's windows and vases).")
QVR_SLIDER(vr_prop_break_stone, "Break Speed: Stone", 0.25f, 3.f, 0.05f, "%.2fx", ext(0.01f, 10.f), "Times Break Speed for rock and brick breakables.")
QVR_SLIDER(vr_prop_break_metal, "Break Speed: Metal", 0.25f, 3.f, 0.05f, "%.2fx", ext(0.01f, 10.f), "Times Break Speed for metal breakables.")
QVR_SLIDER(vr_prop_break_explosive, "Break Speed: Explosive", 0.25f, 3.f, 0.05f, "%.2fx", ext(0.01f, 10.f),
    "Times Break Speed for explosive boxes (20 health: 16.7 m/s by default). Broken, they blow up, in your "
    "hands too.")
QVR_SLIDER(vr_prop_break_piece, "Break Speed: Pieces", 1.f, 12.f, 0.25f, "%.2f m/s", ext(0.1f, 40.f),
    "A hand's hit on a crate's piece, or one held and smashed into something, this fast bursts it into dust; "
    "from half as fast it is worn (two such hits burst it).")
QVR_TOGGLE(vr_carry_slam, "Overhead Slam",
    "A crate or barrel held in both hands over your head and swung down onto a monster, a prop or a wall breaks "
    "at once (what it hits takes a heavy blow); thrown down from there, it takes heavy damage (two such "
    "throws break it).")
QVR_SLIDER(vr_crate_flaregun, "Flare Gun on Crates", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The chance a crate placed about the map (the top one of a stack, no crowbar on it) has a loaded "
    "flare gun lying on it; the same crates each load. Experimental: off (0) by default. Next map.")
QVR_SLIDER(vr_crate_flaregun_max, "Most Flare Guns", 0.f, 4.f, 1.f, "%.0f", ext(0.f, 64.f), "Most flare guns lying on crates in a map. Next map.")
QVR_TOGGLE(vr_crate_sight, "Crates Hide You",
    "Monsters can't see you through crates and explosive boxes lying about (crouch behind a small one). "
    "Carrying one, they see you through it.")
QVR_CYCLE(vr_crate_shield, "Held Crate Shields You",
    "A crate you hold up takes monsters' bullets meant for you (enough of them break it). Crates and "
    "Explosive Boxes: an explosive box too, which then blows up in your hands (vr_crate_shield).",
    {0.f, "Off"}, {1.f, "Crates"}, {2.f, "Crates and Explosive Boxes"})

// vr_menu_physics.inc: pageGibs
QVR_CYCLE(vr_grab_gibs, "Gibs and Heads",
    "Pick up and throw gibs and heads, by reaching for them (or force-grabbing them too).",
    {0.f, "Left alone"}, {1.f, "Grab by hand"}, {2.f, "Hand and force grab"})
QVR_TOGGLE(vr_gib_destroy, "Destroy Gibs",
    "Gibs and heads burst in a mist of blood when shot, blown up, struck, or thrown hard at a wall or a monster.")
QVR_TOGGLE(vr_corpse_gib, "Gib Corpses",
    "Corpses burst into gibs when shot, blown up or struck enough: shotguns, nails, lightning, rockets, fists, melee weapons.")
QVR_CYCLE(vr_corpse_nogib, "Never Gib Corpses",
    "For testing: corpses and ragdolls take hits (blood, pushes, blasts) but never burst into gibs, even blown "
    "up. And Dying Monsters: a monster killed by a rocket dies whole too, so it becomes a ragdoll "
    "(vr_corpse_nogib).",
    {0.f, "Off"}, {1.f, "Corpses and Ragdolls"}, {2.f, "And Dying Monsters"})
QVR_CYCLE(vr_corpse_collide, "Corpses",
    "Corpses in the physics: props, thrown things and held things meet them. Fixed: they never move. Pushable: "
    "heavy, they slide when pushed (hands, held weapons, props, walking into them). Box: a low box round the "
    "body; Pose: its shape as it lies (vr_corpse_collide).",
    {0.f, "Pass Through"}, {1.f, "Fixed Box"}, {2.f, "Pushable Box"}, {3.f, "Fixed Pose"}, {4.f, "Pushable Pose"})
QVR_TOGGLE(vr_corpse_collide_props, "Props Meet Corpses",
    "Boxes, weapons, gibs and heads lying or falling rest on corpses and stop at them (vr_corpse_collide_props).")
QVR_TOGGLE(vr_corpse_collide_thrown, "Thrown Things Meet Corpses",
    "Off: what you throw flies through corpses (its hit still hurts them) (vr_corpse_collide_thrown).")
QVR_TOGGLE(vr_corpse_collide_held, "Held Things Meet Corpses",
    "Your hands, held weapons and props push pushable corpses; a prop held in both hands stops at any "
    "(vr_corpse_collide_held).")
QVR_CYCLE(vr_corpse_collide_player, "You and Corpses",
    "Step Over: a corpse is a low step you walk onto. Solid: walk round it or jump onto it "
    "(vr_corpse_collide_player).",
    {0.f, "Walk Through"}, {1.f, "Step Over"}, {2.f, "Solid"})
QVR_TOGGLE(vr_box3d_walk_nudge, "Feet Nudge Ragdolls",
    "Walking through or over a ragdoll gently nudges the limbs your body overlaps along your pace, without blocking you (vr_box3d_walk_nudge).")
QVR_TOGGLE(vr_corpse_collide_monsters, "Monsters Step Over Corpses",
    "Monsters walk over corpses as a low step instead of through them (vr_corpse_collide_monsters).")
QVR_CYCLE(vr_ragdoll, "Ragdolls",
    "A dying grunt, knight, ogre, enforcer, death knight, rottweiler, scrag, fiend, shambler or gremlin goes limp: his body becomes jointed parts that fall, tumble, are pushed, grabbed "
    "and thrown (vr_ragdoll). Their settings: Ragdoll Settings.",
    {0.f, "Off"}, {1.f, "On (Experimental)"})

// vr_menu_physics.inc: pageRagdolls
QVR_SLIDER(vr_ragdoll_start, "Go Limp At", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "When in his death animation: 0 as soon as he stops being solid, 1 once he lies still (vr_ragdoll_start).")
QVR_SLIDER(vr_ragdoll_max, "Most Ragdolls", 0.f, 16.f, 1.f, "%.0f", ext(0.f, 64.f),
    "At most this many at once. New deaths replace the oldest dead ragdoll first, then the oldest living knockdown if necessary. 0: ragdolls off.")
QVR_TOGGLE(vr_ragdoll_collide_each, "Ragdolls Meet Each Other",
    "Ragdolls fall onto and pile on each other; off, they pass through each other (vr_ragdoll_collide_each).")
QVR_TOGGLE(vr_ragdoll_self_collide, "Limbs Meet Each Other",
    "A ragdoll's own limbs meet: an arm stays out of his chest, a leg out of the other, a body folded over "
    "lies on itself. Jointed neighbours (a forearm and its upper arm), parts deep in each other as he went "
    "limp (until they come apart) and an arm reaching for his wound, a stump or a wall pass through; his gun "
    "never meets him. Off: his limbs pass through each other, as before. Ragdolls made from now on "
    "(vr_ragdoll_self_collide).")
QVR_TOGGLE(vr_ragdoll_active, "Active Deaths",
    "A dying grunt, enforcer, knight, death knight, ogre or zombie (AD's soldiers, knights and zombies too) goes limp as soon as he stops being solid, "
    "his body playing his death animation as his strength fades (legs first): he stumbles if he dies on the "
    "move, may hold his wound and may crawl before going limp; a headshot or beheading collapses him. Off: "
    "limp at Go Limp At (vr_ragdoll_active).")
QVR_SLIDER(vr_ragdoll_active_strength, "Muscle Strength", 0.f, 2.f, 0.1f, "%.1fx", ext(0.f, 3.f),
    "How strongly the joints follow the death animation (vr_ragdoll_active_strength).")
QVR_SLIDER(vr_ragdoll_active_fade, "Strength Lasts", 0.25f, 3.f, 0.25f, "%.2fx", ext(0.1f, 5.f),
    "How long the strength lasts, times his death animation's length (vr_ragdoll_active_fade).")
QVR_SLIDER(vr_ragdoll_active_hertz, "Muscle Stiffness", 2.f, 20.f, 1.f, "%.0f Hz", noExt,
    "How quickly a joint at full strength springs towards its pose: more follows the animation tighter, "
    "less is looser and lags (vr_ragdoll_active_hertz; at most 20).")
QVR_SLIDER(vr_ragdoll_active_damping, "Muscle Damping", 0.2f, 2.f, 0.1f, "%.1f", noExt,
    "How the joints' springs settle: 1 without overshoot, less bouncy, more sluggish (vr_ragdoll_active_damping).")
QVR_SLIDER(vr_ragdoll_active_torque, "Muscle Torque", 0.5f, 5.f, 0.25f, "%.2f", ext(0.f, 10.f),
    "A joint's most force at full strength, times the weight of what hangs on it times its lever: a blow "
    "or a hand overpowers less of it (vr_ragdoll_active_torque).")
QVR_SLIDER(vr_ragdoll_active_pin, "Thrown by His Death", 0.f, 2.f, 0.1f, "%.1f", ext(0.f, 5.f),
    "Body weights pulling his pelvis along his death animation's at first (its throw back or forward "
    "kept); 0 none (vr_ragdoll_active_pin).")
QVR_SLIDER(vr_ragdoll_active_pin_time, "Thrown For", 0.f, 1.f, 0.05f, "%.2f s", ext(0.f, 3.f),
    "And that pull fading out over this (vr_ragdoll_active_pin_time).")
QVR_SLIDER(vr_ragdoll_active_stumble, "Stumble From", 0.f, 300.f, 10.f, "%.0f u/s", ext(0.f, 1000.f),
    "Dying on the move at least this fast, he stumbles on: carried a moment, a leg stepping ahead. 0: "
    "never (vr_ragdoll_active_stumble; a run is about 200-300, a walk 50-100).")
QVR_TOGGLE(vr_ragdoll_active_dismember, "React to Lost Limbs",
    "A dying monster losing a limb holds the stump with his free hand and writhes a few seconds once down; "
    "killed standing by a leg cut off, he tries to keep his balance on the other before he falls; with no "
    "legs he crawls on his arms. Off: he dies as from any blow (vr_ragdoll_active_dismember).")
QVR_SLIDER(vr_ragdoll_active_dismember_time, "Writhing", 0.f, 8.f, 0.5f, "%.1f s", ext(0.f, 20.f),
    "How long he writhes holding the stump once down (vr_ragdoll_active_dismember_time).")
QVR_SLIDER(vr_ragdoll_active_writhe, "Writhe Size", 0.f, 2.f, 0.1f, "%.1fx", ext(0.f, 3.f),
    "How far his torso and legs stir as he writhes, times the default; 0 still (vr_ragdoll_active_writhe).")
QVR_SLIDER(vr_ragdoll_active_balance_time, "Balance on One Leg", 0.f, 3.f, 0.1f, "%.1f s", ext(0.f, 5.f),
    "How long at most he tries to keep his balance on one leg (0: he falls at once) "
    "(vr_ragdoll_active_balance_time).")
QVR_SLIDER(vr_ragdoll_active_balance_lean, "Falls Past", 5.f, 45.f, 1.f, "%.0f deg", ext(1.f, 90.f),
    "And he falls once leaning this far from how he stood (vr_ragdoll_active_balance_lean).")
QVR_SLIDER(vr_ragdoll_active_wound, "Hold the Wound", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The chance he puts a free hand on the wound that killed him (vr_ragdoll_active_wound).")
QVR_TOGGLE(vr_ragdoll_active_env, "Use What Is Round Him",
    "A dying monster senses the walls, floors, ledges and crates round him and uses his hands on them (the "
    "three below). Off: his death ignores them (vr_ragdoll_active_env).")
QVR_SLIDER(vr_ragdoll_active_crawl, "Crawl Away", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The chance a humanoid lying after his death rolls onto his front and crawls a few seconds before going "
    "limp (vr_ragdoll_active_crawl).")
QVR_SLIDER(vr_ragdoll_active_crawl_time, "Crawl Time", 0.5f, 8.f, 0.5f, "%.1f s", ext(0.1f, 30.f), "How long he crawls (vr_ragdoll_active_crawl_time).")
QVR_TOGGLE(vr_ragdoll_active_crawl_back, "Flail on His Back",
    "One to crawl who lies on his back: on, he doesn't roll over onto his front; he flails there like a "
    "dying cockroach, his limbs pawing the air, then goes limp. Off: he rolls over and crawls "
    "(vr_ragdoll_active_crawl_back).")
QVR_SLIDER(vr_ragdoll_active_crawl_force, "Crawl Pull", 0.2f, 3.f, 0.1f, "%.1f", ext(0.f, 6.f),
    "Body weights his arms pull along the floor at most: more drags him further (vr_ragdoll_active_crawl_force).")
QVR_TOGGLE(vr_ragdoll_active_liquids, "Swim and Flail in Liquids",
    "Dying in water (drowned, or falling in just after his death) he paddles and kicks, his head held up, "
    "weakening as he drowns; in lava or slime he flails, short and violent; then limp. Off: his death in a "
    "liquid plays as on land (vr_ragdoll_active_liquids).")
QVR_SLIDER(vr_ragdoll_active_paddle, "Paddle Time", 1.f, 10.f, 0.5f, "%.1f s", ext(0.2f, 30.f),
    "About how long he paddles in water before going limp (vr_ragdoll_active_paddle).")
QVR_SLIDER(vr_ragdoll_active_flail, "Flail Time", 0.5f, 4.f, 0.25f, "%.2f s", ext(0.2f, 30.f),
    "About how long he flails in lava (in slime 1.4 times it) before going limp (vr_ragdoll_active_flail).")
QVR_CYCLE(vr_hit_react, "Hit Reactions",
    "A living monster you hit flinches where you hit it, then carries on fighting: its AI, its damage and "
    "its hitboxes are as before. Limbs: the bones struck turn round their joints (the head snaps back, an "
    "arm swings, a leg's knee buckles and the torso leans over it; a monster with no rig tilts whole). "
    "Whole Body: every monster tilts whole. Off: none, and no slow-down (vr_hit_react).",
    {0.f, "Off"}, {1.f, "Limbs"}, {2.f, "Whole Body"})
QVR_SLIDER(vr_hit_react_strength, "Reaction Strength", 0.f, 2.f, 0.1f, "%.1fx", ext(0.f, 3.f),
    "How far it flinches, times the default (by the share of its health the blow took; "
    "vr_hit_react_strength).")
QVR_SLIDER(vr_hit_react_time, "Reaction Length", 0.5f, 2.f, 0.1f, "%.1fx", ext(0.25f, 4.f),
    "How long a flinch lasts, times the default (its peak at about 0.07 s, gone by about 0.5 s; "
    "vr_hit_react_time).")
QVR_SLIDER(vr_hit_react_slow, "Hits Slow Them", 0.f, 2.f, 0.1f, "%.1fx", ext(0.f, 3.f),
    "A monster hit a moment ago takes shorter steps: a leg hit about half for half a second, with a hitch "
    "towards that leg; the body a third, 0.3 s. 0: none (vr_hit_react_slow).")
QVR_TOGGLE(vr_ragdoll_active_stagger, "Active Shoves and Throws",
    "With Knockdowns, a shove makes a grunt, enforcer, knight, death knight, ogre or zombie backpedal as a "
    "ragdoll held up by its muscles, about as far as a shove used to slide it, its arms wheeling: beaten by "
    "the knockdown's chance it gives way after 3 or 4 steps (knocked down), else it catches itself and "
    "fights on. Going down, its arms brace for the floor; a two-hand throw's body goes slack where you held "
    "it and curls as it goes over. Off: the knockdown's chance alone (vr_ragdoll_active_stagger).")
QVR_SLIDER(vr_ragdoll_active_stagger_lean, "Stagger Falls Past", 10.f, 60.f, 5.f, "%.0f deg", noExt,
    "A staggering monster whose middle leans this far past its feet for a third of a second, or twice "
    "this far at once, falls whatever its balance (vr_ragdoll_active_stagger_lean).")
QVR_SLIDER(vr_ragdoll_active_stagger_steps, "Backpedal Steps", 1.f, 10.f, 1.f, "%.0f", noExt,
    "How many steps back a shoved monster takes at most (a steady one up to 3 more to catch itself); one "
    "the shove beat gives way after 3 or 4 of them (vr_ragdoll_active_stagger_steps).")
QVR_SLIDER(vr_ragdoll_active_stagger_travel, "Backpedal Distance", 0.f, 2.f, 0.1f, "%.1fx", noExt,
    "How far a shoved monster backpedals, times how far the same shove slid a standing one before "
    "active shoves (at most 4 m) (vr_ragdoll_active_stagger_travel).")
QVR_SLIDER(vr_ragdoll_active_stagger_push, "Shove's Push", 0.f, 5.f, 0.25f, "%.2f m/s", ext(0.f, 10.f),
    "A fresh two-hand shove's push on a staggering monster's top (its pelvis carried by its balance, its "
    "feet not): more tips it further (vr_ragdoll_active_stagger_push).")
QVR_SLIDER(vr_ragdoll_active_stagger_pin, "Stagger Balance", 0.5f, 5.f, 0.25f, "%.2f", ext(0.f, 10.f),
    "Body weights holding its pelvis up while it staggers, at most (by its balance: the knockdown's chance "
    "against its roll): less, it falls more often (vr_ragdoll_active_stagger_pin).")
QVR_SLIDER(vr_ragdoll_active_stagger_blend, "Stagger Recovery Blend", 0.05f, 1.f, 0.05f, "%.2f s", noExt,
    "Recovered, its ragdoll blends into the animated monster over this (vr_ragdoll_active_stagger_blend).")
QVR_SLIDER(vr_ragdoll_active_stagger_settle, "Stagger Settles Below", 0.f, 3.f, 0.1f, "%.1f m/s", noExt,
    "Its backpedal nearly done, not mid-step, steady and slower than this: it has recovered and fights on "
    "(higher: handed back sooner) (vr_ragdoll_active_stagger_settle).")
QVR_SLIDER(vr_ragdoll_active_stagger_settle_time, "Stagger Settle at Most", 0.f, 1.2f, 0.05f, "%.2f s", noExt,
    "Still staggering this long after its backpedal: recovered however it moves "
    "(vr_ragdoll_active_stagger_settle_time).")
QVR_SLIDER(vr_ragdoll_active_stagger_drop, "Stagger Drop Falls", 0.f, 64.f, 4.f, "%.0f units", noExt,
    "Backpedalling off a ledge or down from the top of stairs: the floor this much lower under it than "
    "where it stood, it falls instead of stepping on air. 0: never (vr_ragdoll_active_stagger_drop).")
QVR_TOGGLE(vr_ragdoll_active_getup, "Active Get-Ups",
    "A knocked-down grunt, enforcer, knight, death knight, ogre or zombie gets up with its body: it rolls "
    "over if it must, pulls itself into its get-up and rises through it, then fights on. Off: its ragdoll "
    "blends into the get-up from the floor (vr_ragdoll_active_getup).")
QVR_SLIDER(vr_ragdoll_active_drown, "Drowning Struggle", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "With Knockdowns, the chance a grunt, enforcer, knight, death knight, ogre or zombie standing with its "
    "head under water, out of breath, struggles as a ragdoll: it paddles frantically for the surface, its "
    "head up, towards where it can stand, and gets up there. Needs Swim and Flail in Liquids. 0: it drowns "
    "standing (vr_ragdoll_active_drown).")
QVR_SLIDER(vr_ragdoll_active_pullup, "Pull Up From Ledges", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "A knocked-down grunt, enforcer, knight, death knight, ogre or zombie falling past a ledge's lip near a "
    "hand grabs it (Grab Ledges on) and hangs a moment; then, by this chance, it pulls itself up onto the "
    "top and lies there until it gets up, else it drops. Shooting a hand makes it let go. 0: it never grabs "
    "(vr_ragdoll_active_pullup).")
QVR_SLIDER(vr_ragdoll_active_grab, "Grab Others Falling", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The chance a grunt, enforcer, knight, death knight, ogre or zombie going down (knocked down or dying) "
    "grabs at another monster within reach, standing or lying, to catch itself: a hand on it a moment, "
    "slowing its fall; a body grabbed is dragged along. 0: never (vr_ragdoll_active_grab).")
QVR_SLIDER(vr_ragdoll_active_guard, "Guard the Head", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The chance a living monster guards its head when your hand swings at it in melee (a sword, an axe, a "
    "fist, a bash; never a shot): its arms up before its face a moment. Standing it is drawn as a hit "
    "reaction (Hit Reactions on Limbs); staggering, down or getting up, its body's arms. Looks alone: "
    "the blow lands where it lands. 0: never (vr_ragdoll_active_guard).")
QVR_TOGGLE(vr_corpse_placed, "Placed Corpses Are Bodies",
    "The corpses a map places (Dimension and Dawn of the Machine) are dead monsters' bodies: shot, gibbed and "
    "burnt, and ragdolls you take and cut once you come near. Off: scenery. The next map load (vr_corpse_placed).")
QVR_SLIDER(vr_ragdoll_active_getup_pin, "Get-Up Lift", 0.5f, 5.f, 0.25f, "%.2f", ext(0.f, 10.f),
    "Body weights pulling its pelvis through its get-up, at most: less, it struggles and may slump "
    "(vr_ragdoll_active_getup_pin).")
QVR_SLIDER(vr_ragdoll_active_getup_pace, "Get-Up Pace", 0.3f, 2.f, 0.1f, "%.1fx", noExt,
    "Its get-up's speed, times the get-up's own (vr_ragdoll_active_getup_pace).")
QVR_SLIDER(vr_ragdoll_active_getup_blend, "Get-Up Blend", 0.05f, 1.f, 0.05f, "%.2f s", noExt,
    "Risen, its ragdoll blends into the animated monster over this (vr_ragdoll_active_getup_blend).")
QVR_TOGGLE(vr_ragdoll_active_getup_hold, "Get Up in Place",
    "On: it rolls over and gathers itself where it lies, then rises; only the get-up's own motion moves "
    "it. Off: as before, its roll carried it along the floor and it was dragged back "
    "(vr_ragdoll_active_getup_hold).")
QVR_SLIDER(vr_ragdoll_mass, "Mass", 20.f, 200.f, 5.f, "%.0f kg", ext(),
    "A ragdoll's whole weight, its parts by their size, for a monster whose own Mass is Global: each has its "
    "own by default (grunt 80, knight 90, enforcer 100, death knight 130, ogre 200, rottweiler and scrag 40, fiend 140, shambler 280, gremlin 20; "
    "vr_ragdoll_mass).")
QVR_SLIDER(vr_ragdoll_friction, "Friction", 0.1f, 2.f, 0.1f, "%.1f", ext(),
    "How much it drags and catches on floors and steps (vr_ragdoll_friction).")
QVR_SLIDER(vr_ragdoll_joint_friction, "Joint Friction", 0.f, 10.f, 0.5f, "%.1f N m", ext(),
    "How hard the joints are to turn: 0 limp as a rag, more like a body freshly dead (vr_ragdoll_joint_friction).")
QVR_SLIDER(vr_ragdoll_joint_stiffness, "Joint Stiffness", 0.f, 5.f, 0.25f, "%.2f Hz", ext(),
    "A spring in each joint pulling it back towards how he stood: 0 none, 1 to 3 some tone left in the body "
    "(vr_ragdoll_joint_stiffness).")
QVR_SLIDER(vr_ragdoll_limits, "Joint Limits", 0.25f, 1.5f, 0.05f, "%.2fx", ext(0.05f, 3.f),
    "How far the joints bend and twist, times the body's own limits: less stiff, more loose "
    "(vr_ragdoll_limits).")
QVR_SLIDER(vr_ragdoll_damping, "Limb Damping", 0.f, 3.f, 0.1f, "%.1f", ext(), "Less flailing of the arms and legs (vr_ragdoll_damping).")
QVR_SLIDER(vr_ragdoll_blast, "Blast Throw", 0.f, 5.f, 0.25f, "%.2fx", ext(),
    "How far explosions throw a ragdoll, times what they give a prop of its weight (vr_ragdoll_blast).")
QVR_SLIDER(vr_ragdoll_inherit, "Death Motion Kept", 0.f, 2.f, 0.1f, "%.1fx", noExt,
    "How much of his death animation's motion the parts keep as he goes limp (vr_ragdoll_inherit).")
QVR_SLIDER(vr_ragdoll_grab_reach, "Grip Reach", 1.f, 16.f, 1.f, "%.0f units", ext(),
    "How near a limb your hand must be to take it (vr_ragdoll_grab_reach).")

// vr_menu_settings.inc: ?
QVR_TOGGLE_BYNAME(sv_autosave, "Autosave",
    "Saves the game by itself in quiet moments (and as each level starts).")
QVR_SLIDER_BYNAME(sv_autosave_interval, "Every", 10.f, 300.f, 10.f, "%.0f s", noExt,
    "About how often it autosaves: it waits for a quiet moment (not hurt, not firing, slowing down).")
QVR_TOGGLE_BYNAME(vr_autosave_level_start, "As Each Level Starts",
    "An autosave a moment after each level starts (a new map, not a loaded game).")
QVR_SLIDER_BYNAME(vr_autosave_slots, "Autosaves Kept", 1.f, 9.f, 1.f, "%.0f per map", noExt,
    "How many of each map's latest autosaves are kept: the oldest is replaced by a new one. 1: only the "
    "last one, overwritten.")
QVR_TOGGLE_BYNAME(vr_autosave_list_all, "List Every Game's",
    "Load Autosave and the death menu list the autosaves of every campaign and map package, not only "
    "the one you are playing (their first row switches it too).")
QVR_TOGGLE_BYNAME(vr_autosave_safe, "Wait Until Safe",
    "An autosave due waits until the checks below pass, then it is made: so it never traps you in a fight.")
QVR_TOGGLE_BYNAME(vr_autosave_safe_alerted, "Not While Hunted", "Not while a living monster is after you, wherever it is.")
QVR_SLIDER_BYNAME(vr_autosave_safe_radius, "Monsters in Sight Within", 0.f, 1500.f, 50.f, "%.0f units", noExt,
    "Not while a living monster you can see is this near (a unit is 3 cm; 0: off), awake or not.")
QVR_SLIDER_BYNAME(vr_autosave_safe_health, "Health at Least", 0.f, 100.f, 5.f, "%.0f%%", noExt,
    "Not while your health is below this share of its maximum (0: off). Healing ends this wait.")
QVR_SLIDER_BYNAME(vr_autosave_safe_maxdelay, "Longest Wait for Monsters", 0.f, 300.f, 10.f, "%.0f s", noExt,
    "After this long waiting for the monsters (one stuck hunting you), it autosaves anyway; your health "
    "must still be high enough. 0: waits as long as it takes.")
QVR_TOGGLE_BYNAME(vr_death_menu, "Death Menu",
    "A moment after you die: Respawn (your last save), Restart the Level, or any autosave kept, newest "
    "first. Back: you stay dead, fire respawns as ever.")
QVR_SLIDER_BYNAME(vr_death_menu_delay, "Death Menu After", 0.5f, 10.f, 0.5f, "%.1f s", noExt, nullptr)
QVR_CYCLE_BYNAME(sv_autoload, "Respawn",
    "What respawning does (fire when dead, the death menu's Respawn): load your last save (manual or "
    "auto), or start the level again.",
    {0.f, "Restarts the Level"}, {2.f, "Loads the Last Save"})

// vr_menu_settings.inc: pageVrsHeight
QVR_SLIDER(vr_height_calibration, "Height", 1.f, 2.2f, 0.01f, "%.2f m", ext(0.5f, 3.f),
    "Your real height: it puts your eyes at the right height in the game and fits the body to you. Set "
    "Height Now measures it.")
QVR_SLIDER(vr_world_scale, "World Scale", 0.5f, 2.f, 0.05f, "%.2f", ext(0.25f, 4.f),
    "How big the world feels around you: higher makes it bigger. 1.25: Quake's sizes as a person would "
    "see them.")
QVR_SLIDER(vr_floor_offset, "Floor Offset", -50.f, 30.f, 1.f, "%.0f", ext(-400.f, 400.f),
    "Moves the floor up or down: change it if you feel you are floating above it or sunk into it.")

// vr_menu_settings.inc: pageVrsHands
QVR_SLIDER(vr_menu_hands_x, "Hand Forward", -5.f, 5.f, 0.1f, "%+.1f cm", ext(-20.f, 20.f), nullptr)
QVR_SLIDER(vr_menu_hands_y, "Hand Inward", -5.f, 5.f, 0.1f, "%+.1f cm", ext(-20.f, 20.f), nullptr)
QVR_SLIDER(vr_menu_hands_z, "Hand Up", -5.f, 5.f, 0.1f, "%+.1f cm", ext(-20.f, 20.f), nullptr)
QVR_SLIDER(vr_menu_hands_pitch, "Hand Pitch", -30.f, 30.f, 0.5f, "%+.1f", ext(-90.f, 90.f),
    "Both hands: tilts the hands and the guns in them up or down on the controllers, until aiming feels "
    "natural. Changes where they aim.")
QVR_SLIDER(vr_menu_hands_yaw, "Hand Yaw", -30.f, 30.f, 0.5f, "%+.1f", ext(-90.f, 90.f),
    "Both hands, mirrored: turns the hands and the guns in them inward or outward on the controllers. "
    "Changes where they aim.")
QVR_SLIDER(vr_menu_hands_roll, "Hand Roll", -30.f, 30.f, 0.5f, "%+.1f", ext(-90.f, 90.f), nullptr)

// vr_menu_settings.inc: pageVrsHolsters
QVR_SLIDER(vr_menu_holster_hip_x, "Hip Holsters Forward", -10.f, 10.f, 0.5f, "%+.1f", ext(-40.f, 40.f), nullptr)
QVR_SLIDER(vr_menu_holster_hip_y, "Hip Holsters Inward", -10.f, 10.f, 0.5f, "%+.1f", ext(-40.f, 40.f), nullptr)
QVR_SLIDER(vr_menu_holster_hip_z, "Hip Holsters Up", -10.f, 10.f, 0.5f, "%+.1f", ext(-40.f, 40.f), nullptr)
QVR_SLIDER(vr_menu_holster_chest_x, "Chest Holsters Forward", -10.f, 10.f, 0.5f, "%+.1f", ext(-40.f, 40.f), nullptr)
QVR_SLIDER(vr_menu_holster_chest_y, "Chest Holsters Inward", -10.f, 10.f, 0.5f, "%+.1f", ext(-40.f, 40.f), nullptr)
QVR_SLIDER(vr_menu_holster_chest_z, "Chest Holsters Up", -10.f, 10.f, 0.5f, "%+.1f", ext(-40.f, 40.f), nullptr)
QVR_SLIDER(vr_menu_holster_back_x, "Back Holsters Forward", -10.f, 10.f, 0.5f, "%+.1f", ext(-40.f, 40.f), nullptr)
QVR_SLIDER(vr_menu_holster_back_y, "Back Holsters Inward", -10.f, 10.f, 0.5f, "%+.1f", ext(-40.f, 40.f), nullptr)
QVR_SLIDER(vr_menu_holster_back_z, "Back Holsters Up", -10.f, 10.f, 0.5f, "%+.1f", ext(-40.f, 40.f), nullptr)
QVR_TOGGLE(vr_holster_weapons_draw_shoulder, "Shoulder Holster Weapons",
    "Draws the guns in the two holsters on your back, reached over your shoulders (the highest pair). Off: "
    "out of your view; you still take them and put them away there. The other pairs: Advanced VR Options "
    "> Body and Display.")

// vr_menu_settings.inc: pageVrsItemCollection
QVR_CYCLE(vr_menu_pickup_mode, "Item Pickup Mode",
    "Immersive: grip ammo, health and backpacks to carry them, and let go of them at a holster to put them "
    "away. Auto-Collect: an ammo or health box is taken the moment a hand grips it or a force grab brings "
    "it in, seen shrinking into the hand. Vanilla: walking into it takes it, as in Quake. The details: "
    "Advanced VR Options > Carrying and Throwing > Carrying.",
    {0.f, "Immersive"}, {1.f, "Auto-Collect"}, {2.f, "Vanilla"})

// vr_menu_settings.inc: pageVrsAmmoPouch
QVR_SLIDER(vr_ammo_pouch_shells_max, "Shells Shown in the Pouch", 1.f, 20.f, 1.f, "%.0f", noExt,
    "The most shells piled in the ammo pouch: one a shell up to this many.")
QVR_SLIDER(vr_ammo_pouch_mags_max, "Magazines Shown in the Pouch", 1.f, 6.f, 1.f, "%.0f", noExt,
    "The most magazines (or cells) standing in the ammo pouch: up to three in a row, then two rows. Pouch "
    "reloading takes the one your gun's well meets.")

// vr_menu_settings.inc: pageVrsBodyEquipment
QVR_TOGGLE(vr_body_leg_fit, "Legs Fit Your Height",
    "The legs lengthen or shorten (up to a quarter) so that, standing at the height the headset sees you "
    "stand at, the feet are on the floor and the knees straight: changing Height no longer breaks the legs "
    "or needs Body Calibration again. Off: the legs as the body's proportions make them.")
QVR_TOGGLE(vr_body_gear_fit, "Gear Fits Your Body",
    "The holsters and the pouches are placed for your body's size (Height, World Scale) and the floor, so "
    "the belt sits on the belt for a tall player too. Off: placed as before (low on a tall body).")

// vr_menu_settings.inc: pageVrsLocomotion
QVR_CYCLE(vr_menu_move_towards, "Move Towards",
    "Where pushing the stick forward takes you: where you look (Head), or where that hand points, so you "
    "can walk one way and look another.",
    {1.f, "Head"}, {2.f, "Left Hand"}, {3.f, "Right Hand"})
QVR_TOGGLE(vr_stick_swap, "Swap Stick Functions",
    "Off: the left stick moves you and the right one turns. On: the right stick moves, the left turns.")

// vr_menu_settings.inc: pageVrsComfort
QVR_CYCLE(vr_comfort_vignette, "Vignette",
    "Darkens the edges of your view while the sticks move or turn you: it eases motion sickness for many. "
    "Your own steps in the room never do it.",
    {0.f, "Off"}, {1.f, "Moving and Turning"}, {2.f, "Moving Only"}, {3.f, "Turning Only"})
QVR_SLIDER(vr_comfort_vignette_strength, "Vignette Strength", 0.1f, 1.f, 0.1f, "%.1f", noExt,
    "How dark and how wide the vignette is: at 1 you see through a narrow tunnel.")
QVR_SLIDER(vr_comfort_teleport_fade, "Fade on Scripted Teleports", 0.f, 2.f, 0.1f, "%.1f s", noExt,
    "When the game moves you somewhere else at once (a boss sending you to another arena), your view goes "
    "black and comes back over this long. 0: no fade.")
QVR_SLIDER(vr_lightstyle_comfort, "Soften Flickering Lights", 0.f, 1.f, 0.25f, "%.2f", noExt,
    "Flickering and pulsing map lights (Quake's and Arcane Dimensions' lamps) swing less around their usual "
    "brightness, their steps blended: 0 as the maps made them, 0.5 half the swing, 1 steady. They keep "
    "their rhythm. Lights switched on and off are not changed.")

// vr_menu_settings.inc: pageVrsTurning
QVR_CYCLE(vr_menu_turning, "Turning Mode",
    "Smooth: the turning stick turns you steadily. Snap: each push turns you at once by Snap Angle, which "
    "many find more comfortable. Off: the stick never turns you, in any state (its other uses stay): "
    "you turn in real life.",
    {0.f, "Smooth"}, {1.f, "Snap"}, {2.f, "Off"})

// vr_menu_settings.inc: pageVrsFlashlight
QVR_CYCLE(vr_flashlight, "Flashlight",
    "Immersive: a torch on your belt: pull the trigger with an open hand at it to switch it on or off; press "
    "the grip to take it in your hand. Always on Head: worn at your temple, on from the start: pull the "
    "trigger with an open hand at it to switch it.",
    {0.f, "Off"}, {1.f, "Immersive"}, {2.f, "Always on Head"})
QVR_CYCLE(vr_flashlight_side, "Flashlight Side",
    "The side of your belt the torch hangs on (Always on Head: the temple it is worn at).",
    {0.f, "Left"}, {1.f, "Right"})

// vr_menu_settings.inc: pageVrsWeapons
QVR_CYCLE(vr_weapon_grip_mode, "Weapon Grip",
    "Hold: keep the grip pressed to hold a weapon. Sticky: a press takes it, another lets it go.",
    {0.f, "Hold"}, {1.f, "Sticky"})
QVR_SLIDER(vr_weapon_load_grace, "Grip Grace After Loading", 0.f, 2.f, 0.05f, "%.2f s", noExt,
    "After loading a save, keep the restored weapons in your hands this long even without holding the grip. "
    "Starts when play resumes; 0 disables it.")

// vr_menu_settings.inc: pageVrsBulletTime
QVR_CYCLE_BYNAME(vr_bullettime_meter_mode, "Meter Refill",
    "Earned by Damage: the TIME meter never fills by itself; hurting enemies fills it (six grunts' worth "
    "fills it from empty; less while slowed). Over Time: it fills by itself after each use. Either way it "
    "is kept from level to level and in saves, full on a new game.",
    {1.f, "Earned by Damage"}, {0.f, "Over Time"})
QVR_CYCLE_BYNAME(vr_bullettime_tap_gesture, "Screen Tap",
    "Activation by the Wrist Gadget: Double Tap, two quick taps on its screen (a single one does nothing, "
    "so a stray knock never starts it); Single Tap, one hard tap.",
    {0.f, "Single Tap"}, {1.f, "Double Tap"})
QVR_SLIDER_BYNAME(vr_bullettime_tap_blow_quiet, "Quiet After Blows", 0.f, 1.5f, 0.05f, "%.2f s", ext(0.f, 3.f),
    "Screen taps don't count for this long after a melee blow of yours lands, or you shove or bash, nor "
    "while your hands move fast (Calm Hands Speed, Advanced VR Options > Combat > Bullet Time): no slow "
    "motion by accident in a melee. 0: off.")

// vr_menu_settings.inc: pageVrsBody
QVR_CYCLE(vr_body_mode, "Body Type",
    "How much of your body you see: all of it, legs and all; the torso and arms; or only the hands.",
    {3.f, "Full Body"}, {2.f, "Torso and Arms"}, {0.f, "Only Hands"})
QVR_CYCLE(vr_gadget_arm, "Wrist Gadget Arm", "The arm the wrist gadget (health, armour and ammo) is on.",
    {0.f, "Left"}, {1.f, "Right"})

// vr_menu_settings.inc: pageVrsHud
QVR_CYCLE(vr_hud_mode, "HUD",
    "Wrist Gadget: health, armour and ammo on your wrist. Status Bar: Quake's, on a hand.",
    {1.f, "Wrist Gadget"}, {0.f, "Status Bar"})
QVR_CYCLE(vr_crosshair, "Crosshair",
    "A mark where your gun aims: a dot, or a laser beam from the muzzle.",
    {0.f, "Off"}, {1.f, "Dot"}, {2.f, "Laser"}, {3.f, "Soft laser"})

// vr_menu_settings.inc: pageVrsSound
QVR_SLIDER_BYNAME(volume, "Volume", 0.f, 1.f, 0.05f, "%.2f", noExt, "The game's sounds.")
QVR_SLIDER_BYNAME(bgmvolume, "Music Volume", 0.f, 1.f, 0.05f, "%.2f", noExt, "The music's volume.")

// vr_menu_settings.inc: pageVrsScaling
QVR_SLIDER(vr_render_scale, "Render Scale", 0.5f, 1.5f, 0.05f, "%.2f", ext(0.25f, 2.f),
    "Eye rendering resolution, times the headset's. Above 1: smoother edges, slower. Below 1: faster, "
    "blurrier.")
QVR_TOGGLE(vr_dynres, "Dynamic Resolution",
    "When the graphics card can't keep up with the headset, the eyes are drawn smaller for a while (down to "
    "Dynamic Lowest Scale) and upscaled, instead of missing frames; back up slowly once it can. With "
    "headroom it stays at Render Scale: nothing changes.")
QVR_SLIDER(vr_dynres_target, "Dynamic GPU Budget", 0.5f, 1.f, 0.05f, "%.2f", noExt,
    "The share of each headset frame the eyes' GPU time may take before the scale drops (0.85: 85 %; the "
    "rest is left for the headset's runtime and the window). Lower: drops sooner, steadier.")
QVR_SLIDER(vr_dynres_min, "Dynamic Lowest Scale", 0.4f, 1.f, 0.05f, "%.2f", ext(0.25f, 2.f),
    "The lowest scale Dynamic Resolution goes to under load (times the headset's resolution).")
QVR_CYCLE(vr_upscale, "Upscaling",
    "Below Render Scale 1: how the image is enlarged to the headset's size. FSR (AMD) and NIS (NVIDIA) "
    "keep it sharper than Bilinear.",
    {0.f, "Bilinear"}, {1.f, "FSR"}, {2.f, "NIS"})
QVR_SLIDER(vr_upscale_sharpness, "Sharpening", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much the upscaler sharpens the image (FSR, NIS). Too much makes edges shimmer.")
QVR_CYCLE(vr_foveated, "Foveated Rendering",
    "Draws the edges of the view, which the lenses blur anyway, in less detail: faster. NVIDIA graphics "
    "cards only.",
    {0.f, "Off"}, {1.f, "Conservative"}, {2.f, "Balanced"}, {3.f, "Aggressive"})

// vr_menu_settings.inc: pageVrsGraphics
QVR_TOGGLE_BYNAME(vr_taa, "Temporal Antialiasing (TAA)",
    "vr_taa (experimental): smooths edges and shimmer by blending each frame with the last ones, following "
    "how everything moved. Replaces Antialiasing (MSAA) while on. Can leave faint trails behind fast hands.")
QVR_SLIDER_BYNAME(vr_taa_sharpen, "TAA Sharpening", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "vr_taa_sharpen: sharpens the image after TAA, which softens textures a little (0: off).")
QVR_TOGGLE_BYNAME(vr_pbr, "Generated Companion Maps",
    "Adds normal and height maps generated for the exact wall or floor image. Keeps authored maps. From the next map.")
QVR_SLIDER_BYNAME(vr_pbr_strength, "Generated Relief Strength", 0.f, 4.f, 0.1f, "%.1fx", ext(0.f, 8.f),
    "Makes generated bumps and grooves stronger. 1: the generated shape; 0: flat. Authored normal and height components keep their own strength.")

// vr_menu_settings.inc: pageVrsRangerHead
QVR_TOGGLE(vr_body_ranger_head, "Use the Ranger's Head",
    "The head your body has in the mirror, the third-person views and shadows (never in your own eyes): "
    "the Quake ranger's, taken from your game's player model, or off for the plain placeholder head. "
    "Takes effect at the next start; the rows below apply at once.")
QVR_SLIDER(vr_body_head_size, "Size", 0.5f, 2.f, 0.05f, "%.2fx", noExt,
    "How big the head is (1 the first version's size, a little small).")
QVR_SLIDER(vr_body_head_sink, "Neck Sink", 0.f, 12.f, 0.5f, "%.1f cm", noExt,
    "Lowers the head over the neck, and the neck's top with it, so less of the neck shows.")
QVR_SLIDER(vr_body_head_forward, "Forward", -10.f, 10.f, 0.25f, "%.2f cm", noExt, "Moves the head forward (back below zero).")
QVR_SLIDER(vr_body_head_side, "Right", -10.f, 10.f, 0.25f, "%.2f cm", noExt, "Moves the head to its right (left below zero).")
QVR_SLIDER(vr_body_head_up, "Up", -10.f, 10.f, 0.25f, "%.2f cm", noExt,
    "Moves the head up (down below zero), the neck's top with it.")
QVR_SLIDER(vr_body_head_pitch, "Pitch", -45.f, 45.f, 1.f, "%.0f deg", noExt,
    "Tips the chin down (up below zero), about the bottom of the head.")
QVR_SLIDER(vr_body_head_yaw, "Yaw", -45.f, 45.f, 1.f, "%.0f deg", noExt, "Turns the head to its left (right below zero).")
QVR_SLIDER(vr_body_head_roll, "Roll", -45.f, 45.f, 1.f, "%.0f deg", noExt, "Tilts the head to its right (left below zero).")
QVR_SLIDER(vr_menu_body_clone_distance, "Preview Distance", 0.5f, 5.f, 0.1f, "%.1f m", noExt,
    "How far off the mirror image of your body is drawn while this page (or a page placing what you wear) is open.")
QVR_CYCLE(vr_menu_body_clone_side, "Preview Side",
    "Where the mirror image stands, beside the menu panel or straight ahead behind it.",
    {0.f, "Away From Main Hand"}, {1.f, "Left"}, {2.f, "Right"}, {3.f, "Ahead"})

// vr_menu_settings.inc: pageVrsAppearance
QVR_CYCLE(vr_body_build, "Body Shape",
    "The shape of the body you see: lean, athletic or brawny, or feminine (narrower shoulders and waist, "
    "wider hips, a modest chest, slimmer arms and legs). Your calibration still sets its size.",
    {0.f, "Lean"}, {1.f, "Athletic"}, {2.f, "Brawny"}, {3.f, "Feminine"})
QVR_SLIDER(vr_skin_tone, "Skin Tone", -1.f, 1.f, 0.05f, "%.2f", noExt,
    "Darken or lighten the skin on your hands and arms (and the fingertips out of fingerless gloves). Zero "
    "keeps your painted skin. Blood, nails, clothes, gloves and gadgets keep their own colours.")
QVR_TOGGLE(vr_body_bracers, "Bracers",
    "The leather bracers on your forearms. Off: bare forearms with the wrist gadget strapped round the skin, running "
    "on into the hands (with the jointed hand, vr_hand_rig 1: the default).")
QVR_CYCLE(vr_hand_shape, "Hand Shape",
    "The shape of your hands. Feminine: slimmer fingers and palm, finer knuckles, tapered fingertips with "
    "longer nails. The joints are the same, so grips, weapons and reloads fit as before; "
    "skin tone, gloves and nails work on both. Changes at once. On the jointed hands (vr_hand_rig 1: the "
    "default); the older hands (vr_hand_rig 0) have one shape only.",
    {0.f, "Standard"}, {1.f, "Feminine"})
QVR_CYCLE(vr_voice, "Voice",
    "Your character's voice: pain, death, the jump's grunt, landing hard, gasping for air, drowning, "
    "burning. Feminine: Quake's recordings pitched up with their formants raised as they load (no new "
    "recordings). Changes at once. Hear Voice plays one.",
    {0.f, "Masculine"}, {1.f, "Feminine"})
QVR_SLIDER(vr_voice_pitch, "Voice Pitch", 0.f, 8.f, 0.5f, "+%.1f semitones", noExt,
    "With the feminine voice: how much higher it is pitched (the sounds keep their length).")
QVR_SLIDER(vr_voice_formant, "Voice Formants", 0.f, 30.f, 1.f, "+%.0f%%", noExt,
    "With the feminine voice: how much its resonances are raised, a smaller throat's. Too little sounds "
    "like a man's voice sped up, too much a child's.")
QVR_CYCLE(vr_gloves, "Gloves",
    "Leather gloves on your hands: fingerless ones leave the fingertips bare (in your skin tone); full ones "
    "cover every finger. With bare forearms (no bracers) a cuff covers the wrist. On the jointed hands "
    "(vr_hand_rig 1: the default).",
    {0.f, "None"}, {1.f, "Fingerless"}, {2.f, "Full"})
QVR_SLIDER(vr_glove_hue, "Glove Hue", 0.f, 360.f, 5.f, "%.0f deg", noExt,
    "The leather's colour round the colour wheel: 0 red, 25 brown, 60 yellow, 120 green, 240 blue, 300 "
    "purple.")
QVR_SLIDER(vr_glove_saturation, "Glove Saturation", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How strong the colour is: 0 grey, black or white leather; 1 the purest.")
QVR_SLIDER(vr_glove_lightness, "Glove Lightness", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "0 black, 0.5 the colour at its fullest, 1 white. The hand's own creases and knuckles shade it.")
QVR_SLIDER(vr_glove_thickness, "Glove Thickness", 0.f, 4.f, 0.25f, "%.2f mm", noExt,
    "How thick the leather is: the glove is its own layer over the hand, bending with the fingers, its "
    "edge a step down to the skin. 0: painted on the skin only (no layer of its own).")
QVR_TOGGLE(vr_glove_paint, "Worn Leather",
    "The gloves' leather painted worn: scuffed knuckles, creases over the finger joints, stitched seams, "
    "scratches (progs/hand_rig_00_00_glove.png: editable, docs/vr-port/GLOVES.md). Off: plain new leather.")
QVR_SLIDER(vr_glove_relief, "Leather Relief", 0.f, 2.f, 0.1f, "%.1f", noExt,
    "How deep the worn leather's relief is in the light: raised stitches in their grooves, the padded "
    "panels between seams, the creases over the knuckles and joints, the grain. 1 matches the bracers "
    "and pauldrons; 0 flat.")
QVR_TOGGLE(vr_nails, "Painted Nails",
    "Polish on your fingernails, in the colour below. Full gloves cover them; fingerless gloves leave them "
    "showing. Your skin tone doesn't tint the polish. On the jointed hands (vr_hand_rig 1: the default).")
QVR_SLIDER(vr_nail_hue, "Nail Hue", 0.f, 360.f, 5.f, "%.0f deg", noExt,
    "The polish's colour round the colour wheel: 0 and 355 red, 20 coral, 330 pink, 270 violet, 210 blue, "
    "150 green.")
QVR_SLIDER(vr_nail_saturation, "Nail Saturation", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How strong the colour is: 0 grey, black or white polish; 1 the purest. Pastels: low saturation with "
    "high lightness.")
QVR_SLIDER(vr_nail_lightness, "Nail Lightness", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "0 black, 0.5 the colour at its fullest, 1 white. The nail's own shape and the light still shade it.")
QVR_SLIDER(vr_nail_gloss, "Nail Gloss", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How shiny the polish is: 0 matte, 1 a glassy top coat with a sharp highlight and the room's light "
    "mirrored in it.")
QVR_TOGGLE(vr_body_pauldrons, "Pauldrons",
    "Leather pads over the shoulders and the tops of the arms, as the Quake ranger wears.")
QVR_CYCLE(vr_body_pauldron_style, "Pauldron Style",
    "Armour colour: green, yellow or red as the armour you wear (leather without).",
    {0.f, "Ranger leather"}, {1.f, "Armour colour"}, {2.f, "Steel"})
QVR_SLIDER(vr_body_pauldron_size, "Pauldron Size", 0.5f, 1.5f, 0.05f, "%.2fx", ext(0.25f, 4.f),
    "Their size (scaled with the build too). Their fit: Advanced VR Options > Body > Arms and Pauldrons.")
QVR_CYCLE(vr_ghost_hands, "Ghost Hands",
    "A see-through copy of each hand where your real hand is, whenever the game draws your hand elsewhere: a "
    "heavy weapon lagging or drooping, a two-handed grip aiming it, a wall stopping it. It fades out as "
    "the drawn hand comes back to yours.",
    {0.f, "Off"}, {1.f, "Hands"}, {2.f, "Hands and Forearms"})
QVR_CYCLE(vr_ghost_hands_which, "Ghost Which Hands",
    "Both hands, or only the off hand (the one that steadies a weapon in two hands).",
    {0.f, "Both"}, {1.f, "Off Hand Only"})
QVR_SLIDER(vr_ghost_hands_alpha, "Ghost Opacity", 0.05f, 1.f, 0.05f, "%.2f", noExt, nullptr)
QVR_SLIDER(vr_ghost_hands_lightness, "Ghost Lightness", 0.25f, 3.f, 0.05f, "%.2fx", noExt,
    "Times the light on the ghost: brighter stands out from your drawn hand.")
QVR_SLIDER(vr_ghost_hands_tint, "Ghost Tint", 0.f, 1.f, 0.05f, "%.2f", noExt, "0: the hand's own colours; 1: a cold blue.")
QVR_SLIDER(vr_ghost_hands_fade_from, "Ghost Hidden Within", 0.f, 20.f, 0.5f, "%.1f cm", noExt,
    "No ghost while the drawn hand is this close to your real one (its grip, and a point 15 cm ahead of "
    "it: a turn counts too).")
QVR_SLIDER(vr_ghost_hands_fade_range, "Ghost Fade-In Range", 0.5f, 30.f, 0.5f, "%.1f cm", noExt, "And it fades in over this much further apart.")
QVR_TOGGLE(vr_ghost_hands_ignore_recoil, "No Ghost from Recoil",
    "A shot's recoil kicks the drawn hand back and up off your real one (Weapon Effects' Programmatic "
    "Recoil): on, that kick doesn't bring the ghost out (only what else holds the hand off does); off, a "
    "strong kick shows it for a moment.")

// vr_menu_settings.inc: pageBodyDisplay
QVR_CYCLE(vr_handedness, "Handedness",
    "Sets which stick moves you, the wrist gadget's arm and the flashlight's side together. Left-handed: the right "
    "stick moves, the gadget on the right arm, the torch on the right (Custom: set apart, below).",
    {0.f, "Custom"}, {1.f, "Right-handed"}, {2.f, "Left-handed"})
QVR_TOGGLE(vr_leg_holster_model_enabled, "Holster Models",
    "Draws the holsters on your body, with a gun in each one that holds one.")
QVR_TOGGLE(vr_holster_weapons_draw_hip, "Hip Holster Weapons",
    "Draws the guns in the two holsters on your hips. Off: not drawn; you still take them and put them "
    "away there.")
QVR_TOGGLE(vr_holster_weapons_draw_upper, "Chest Holster Weapons",
    "Draws the guns in the two holsters on your chest. Off: not drawn; you still take them and put them "
    "away there.")
QVR_CYCLE(vr_sbar_mode, "Status Bar", "The hand Quake's status bar is on (HUD: Status bar).",
    {1.f, "Off Hand"}, {0.f, "Main Hand"})
QVR_TOGGLE(vr_mirror, "Desktop Mirror",
    "The headset's view in the desktop window. Which eye, both, smoothed or a spectator camera: Recording "
    "(Window View).")

// vr_menu_settings.inc: pageHeadset
QVR_TOGGLE(vr_enabled, "VR",
    "Off: play on the desktop screen, flat, without the headset.")
QVR_CYCLE(vr_xr_runtime, "OpenXR Runtime",
    "Which OpenXR runtime runs the headset; VR restarts. Auto: the one whose app is running (Virtual Desktop: the runtime picked in its Streamer's OpenXR Runtime option; VDXR, which skips SteamVR, unless SteamVR is picked there), else the system's. Keep Virtual Desktop's 'Emulate Index controllers' off.",
    {4.f, "Auto"}, {0.f, "System default"}, {1.f, "Virtual Desktop (VDXR)"}, {2.f, "SteamVR"})
QVR_CYCLE(vr_xr_runtime_fallback, "Try Other Runtimes",
    "Auto: when the chosen runtime fails to start (no headset), try the other installed ones before playing flat. An idle SteamVR (not the system's runtime) only with 'SteamVR too': trying it starts SteamVR.",
    {0.f, "Off"}, {1.f, "On"}, {2.f, "On, SteamVR too"})
QVR_SLIDER(vr_xr_steamvr_wait, "SteamVR Wait", 0.f, 20.f, 1.f, "%.0f s", noExt,
    "How long SteamVR, just started, is given to find the headset (through Virtual Desktop or the Link) before it counts as failed and Auto tries the next runtime.")
QVR_SLIDER(vr_xr_eye_scale, "Eye Image Size", 0.5f, 1.f, 0.05f, "%.2f", noExt,
    "The headset images' size, times the runtime's recommended (each side), at the next VR start. SteamVR's recommended is 1.5 times the panel's pixels by default (its own Render Resolution): 0.82 matches the panel. Smaller images cost less to draw, to copy (SteamVR copies an OpenGL game's every image) and in video memory. The menus' status box shows the eyes' size, the headset panel's if known (vr_xr_panel WxH sets it) and warns over 1.3 times its pixels.")
QVR_CYCLE(vr_xr_unfocused, "Runtime Menu Open",
    "While the runtime's own menu (SteamVR's dashboard, Virtual Desktop's or Meta's menu) has the controllers: the game's last frame stays in view, not rendered again, which leaves the graphics card to the menu; or the game keeps rendering as usual.",
    {1.f, "Freeze the Game's View"}, {0.f, "Keep Rendering"})
QVR_CYCLE(vr_xr_unfocused_pause, "Runtime Menu Pauses",
    "While the runtime's own menu has the controllers, a single player game pauses as the game's own menu pauses it (the monsters wait, the game's time stands still) and resumes as you come back; or it keeps running. Multiplayer always runs on.",
    {1.f, "Pause the Game"}, {0.f, "Keep Running"})
QVR_SLIDER(vr_xr_unfocused_volume, "Runtime Menu Volume", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "The game's sounds while the runtime's menu pauses it: 0 silent, 1 unchanged. The music pauses.")
QVR_CYCLE(vr_xr_exit_quits, "Exit From Runtime Menu",
    "Exiting the game from the runtime's own menu (SteamVR's dashboard, Virtual Desktop's or Meta's): the game "
    "quits, your settings saved; or it keeps running flat on the desktop (Restart VR brings the headset back).",
    {1.f, "Quit the Game"}, {0.f, "Play Flat"})
QVR_TOGGLE(vr_xr_late_acquire, "Late Image Acquire",
    "Each eye's headset image is taken from the runtime only once its scene is drawn, so the runtime's wait for it (SteamVR copying the last one out) overlaps the drawing. Off: before the scene, the old way, to compare.")
QVR_TOGGLE(vr_xr_vendor_profiles, "Vendor Controller Profiles",
    "Pico, HTC Vive Cosmos and Focus 3, HP Reverb G2 and Samsung Odyssey controllers get their own buttons, "
    "menu button included, when the runtime offers them. Off: they go as the runtime emulates them (the "
    "1.0.0 way, if your hands' angle changed). From the next VR start.")
QVR_TOGGLE(vr_xr_log_timing, "OpenXR Timing Log",
    "A line a second in quakevr/qvr_openxr.txt while VR runs: frames rendered, frames missed, each OpenXR call's time and the session's state.")
QVR_TOGGLE(vr_visibility_mask, "Hide Lens Corners",
    "Skip the pixels the lenses never show (if the headset gives them): faster, looks the same. Black corners in the desktop mirror.")
QVR_SLIDER(vr_nearclip, "Near Clip", 0.02f, 1.f, 0.02f, "%.2f units", ext(0.02f, 4.f),
    "How near the eyes things are still drawn (a unit is 3 cm): lower draws a gun at your face whole. Not below 1 without Float Depth.")
QVR_CYCLE(vr_nearclip_held, "Held Items at the Eyes",
    "A weapon or hand right at your eyes: Never clipped draws its parts nearer than Near Clip too; Block the view also draws its inside when an eye is in it, instead of seeing through it.",
    {0.f, "Clipped"}, {1.f, "Never clipped"}, {2.f, "Block the view"})
QVR_TOGGLE(vr_depth_float, "Float Depth",
    "A 32-bit float depth buffer for the eyes: no flicker far away with a small Near Clip. Applies after Restart VR.")
QVR_TOGGLE(vr_weapon_recoil, "Programmatic Recoil",
    "Each shot kicks the drawn weapon back and tips it up, as far as the weapon's own Recoil Kick Back and "
    "Muzzle Rise (Weapon Offsets > Effects: every gun has its own; the kick in Quake's gun models is taken "
    "out). Looks only: the aim and the shots stay where your controller is.")
QVR_SLIDER(vr_recoil_amount, "Recoil Amount", 0.f, 2.f, 0.05f, "%.2fx", ext(0.f, 5.f),
    "Every weapon's recoil times this: its kick back and its muzzle rise (each weapon's own cm and degrees: "
    "Weapon Offsets > Effects). 1: as each weapon is set; 0: none.")
QVR_SLIDER(vr_recoil_2h_kick, "Two-Handed Kick Back", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "A shot fired with the weapon held in both hands: its kick back times this, times the weapon's own "
    "Two-Handed Kick Back (Weapon Offsets > Effects). 1: as held in one hand; lower: steadier with the "
    "second hand on it.")
QVR_SLIDER(vr_recoil_2h_rise, "Two-Handed Muzzle Rise", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "A shot fired with the weapon held in both hands: its muzzle rise (and the little sideways turn with it) "
    "times this, times the weapon's own Two-Handed Muzzle Rise. 1: as held in one hand.")
QVR_SLIDER(vr_recoil_2h_time, "Two-Handed Return Time", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "A shot fired with the weapon held in both hands: the time it takes back to rest times this, times the "
    "weapon's own Two-Handed Return Time. 1: as held in one hand; lower: back sooner.")
QVR_SLIDER(vr_recoil_2h_haptic, "Two-Handed Haptics", 0.f, 1.f, 0.05f, "%.2fx", ext(0.f, 2.f),
    "A shot fired with the weapon held in both hands: its haptic punch (how hard and how long) times this, "
    "times the weapon's own Two-Handed Haptics. 1: as held in one hand.")
QVR_SLIDER(vr_autopump_time, "Auto Pump Time", 0.15f, 0.45f, 0.01f, "%.2f s", ext(0.1f, 0.48f),
    "After each shot the shotgun cycles itself: its fore-end is driven back along the guide rods over the "
    "barrel and springs home, and the spent shell leaves the port as it reaches the back (looks only: the "
    "fire rate is the same). How long the stroke takes, back and home (the shotgun fires again after 0.5 s).")
QVR_SLIDER(vr_autopump_delay, "Auto Pump Delay", 0.f, 0.3f, 0.01f, "%.2f s", ext(0.f, 0.38f),
    "How long after the shot the stroke starts: the shot, a moment, then the cycle (0: at once). Cut short so "
    "that the stroke is home before the shotgun can fire again (0.5 s).")
QVR_SLIDER(vr_autopump_sound, "Auto Pump Sound", 0.f, 1.f, 0.1f, "%.1f", noExt, "Volume of its two clacks (0: off).")
QVR_SLIDER(vr_autopump_haptics, "Auto Pump Haptics", 0.f, 2.f, 0.1f, "%.1f", noExt,
    "Strength of the light ticks in the hand at the back of the stroke and home (0: off).")
QVR_TOGGLE(vr_muzzle_flash, "Programmatic Muzzle Flash",
    "The shotgun's flash at the muzzle of the weapons whose Muzzle Flash is on (Weapon Offsets > Effects: the "
    "grunts' burst rifle), following the gun.")
QVR_TOGGLE(vr_muzzle_flash_enemies, "Enemies' Muzzle Flashes",
    "The grunts' and enforcers' guns flash at their muzzles as they fire.")
QVR_TOGGLE(vr_muzzle_smoke_enemies, "Enemies' Muzzle Smoke",
    "Puffs of gun smoke out of an enforcer's rifle's and a grunt's gun's muzzle as they fire, as out of your guns'.")
QVR_TOGGLE(vr_gunfx, "Gunfire Particles",
    "Each shot's sparks and smoke, by the weapon: yours, the armed grunts' and the grunts' and enforcers' "
    "guns. Not the flamethrowers, the crossbow, melee weapons, the grapple or the toolgun.")
QVR_TOGGLE(vr_gunfx_sparks, "Muzzle Sparks",
    "A cone of sparks out of the muzzle (each barrel of the super shotgun; the energy guns throw electric arcs).")
QVR_SLIDER(vr_gunfx_spark_amount, "Spark Amount", 0.f, 3.f, 0.05f, "%.2fx", ext(0.f, 4.f),
    "How many sparks a shot throws, the energy guns' arcs too (each weapon's own: Weapon Offsets > Effects). "
    "0: none.")
QVR_SLIDER(vr_gunfx_spark_size, "Spark Size", 0.25f, 3.f, 0.05f, "%.2fx", ext(0.25f, 4.f), "How big the sparks are (the energy guns' arcs too).")
QVR_SLIDER(vr_gunfx_spark_speed, "Spark Speed", 0.25f, 3.f, 0.05f, "%.2fx", ext(0.25f, 4.f), "How fast the sparks fly out of the muzzle (so how far).")
QVR_TOGGLE(vr_gunfx_smoke, "Muzzle Smoke",
    "A cone of smoke billowing forward out of the muzzle with the flash, swirling as it spreads and fades, the "
    "flash's glow in it; the nail pistol's side vents smoke lightly. Never the energy guns'.")
QVR_SLIDER(vr_gunfx_smoke_amount, "Smoke Amount", 0.f, 3.f, 0.05f, "%.2fx", ext(0.f, 4.f),
    "How many puffs a shot's muzzle smoke has: how thick the cloud is (each weapon's own: Weapon Offsets > "
    "Effects); the thunderbolt's ozone haze too. 0: none.")
QVR_SLIDER(vr_gunfx_smoke_size, "Smoke Size", 0.25f, 3.f, 0.05f, "%.2fx", ext(0.25f, 4.f),
    "How big the muzzle smoke's puffs are and how much they grow (the thunderbolt's ozone haze too).")
QVR_SLIDER(vr_gunfx_smoke_speed, "Smoke Speed", 0.25f, 3.f, 0.05f, "%.2fx", ext(0.25f, 4.f),
    "How fast the muzzle smoke billows out (so how far ahead the cloud reaches).")
QVR_SLIDER(vr_gunfx_smoke_direction, "Smoke Directionality", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "1: the smoke billows forward in a cone out of the muzzle. 0: it bursts out every way. In between: more "
    "or less forward.")
QVR_SLIDER(vr_gunfx_smoke_alpha, "Smoke Opacity", 0.f, 3.f, 0.05f, "%.2fx", ext(0.f, 3.f),
    "How opaque the muzzle smoke is as it comes out (it fades from there).")
QVR_TOGGLE(vr_gunfx_port, "Ejection Port Smoke",
    "A small quick puff out of a shotgun's port as the casing leaves it (the super shotgun's open breech).")
QVR_TOGGLE(vr_gunfx_backblast, "Rocket Backblast",
    "A swirling cone of smoke out of the rocket launchers' exhaust, the exhaust smoking a moment after, and "
    "dust kicked up off the floor under it.")
QVR_TOGGLE(vr_gunfx_heat, "Barrel Heat",
    "Sustained fire (the nailguns) leaves the muzzle smoking in thin wisps and glowing faintly until it cools.")
QVR_TOGGLE(vr_gunfx_wisps, "Muzzle Wisps",
    "After you stop firing, a very thin, faint line of smoke trails off each barrel for a few seconds (the "
    "shotgun's one, the super shotgun's and the nailgun's two), following the muzzle as you move the gun.")
QVR_SLIDER(vr_gunfx_wisp_delay, "Wisp Delay", 0.f, 1.f, 0.05f, "%.2f s", ext(0.f, 3.f), "How long after the last shot the wisps start.")
QVR_SLIDER(vr_gunfx_wisp_time, "Wisp Persistence", 0.5f, 10.f, 0.25f, "%.2f s", ext(0.f, 30.f),
    "How long a barrel keeps smoking (after a burst; a single shot of the nailguns: less).")
QVR_SLIDER(vr_gunfx_wisp_fade, "Wisp Fade Time", 0.2f, 4.f, 0.1f, "%.1f s", noExt,
    "How long the smoke let go lasts, rising and fading: the trail's length.")
QVR_SLIDER(vr_gunfx_wisp_width, "Wisp Thickness", 0.1f, 3.f, 0.05f, "%.2f cm", ext(0.05f, 20.f), "How thick a wisp is at the muzzle.")
QVR_SLIDER(vr_gunfx_wisp_spread, "Wisp Spread", 1.f, 8.f, 0.25f, "%.2fx", ext(1.f, 10.f), "How much thicker (and fainter) a wisp gets as it fades.")
QVR_SLIDER(vr_gunfx_wisp_amount, "Wisp Amount", 0.f, 3.f, 0.05f, "%.2fx", ext(0.f, 4.f),
    "How dense the wisps are (their opacity; each weapon's own: Weapon Offsets > Effects).")
QVR_SLIDER(vr_gunfx_wisp_opacity, "Wisp Max Opacity", 0.02f, 1.f, 0.02f, "%.2f", noExt,
    "The most opaque a wisp gets, wherever it is (its start at the muzzle is its densest): a cap on Wisp "
    "Amount. 1: no cap.")
QVR_SLIDER(vr_gunfx_wisp_rise, "Wisp Rise", 0.f, 60.f, 1.f, "%.0f cm/s", ext(0.f, 200.f),
    "How fast the smoke rises off the barrel (Smoke Turbulence curls it).")
QVR_TOGGLE(vr_gunfx_impacts, "Impacts by Material",
    "Bullets and nails hitting walls throw up what the surface is made of: dust off stone and ground, sparks "
    "off metal, chips off wood (flesh has the gore's blood mist).")
QVR_TOGGLE(vr_gunfx_ozone, "Lightning Ozone Haze",
    "A faint bluish haze lingers where the thunderbolt's beam plays on a surface.")
QVR_TOGGLE(vr_gunfx_enemies, "Enemies' Gunfire Particles", "The monsters' guns make them too.")
QVR_TOGGLE(vr_tracers, "Bullet Tracers",
    "Hitscan shots (the shotguns, the burst rifle) draw streaks of light flying from the muzzle to what they "
    "hit. A weapon may differ (Weapon Offsets > Effects: Tracers, and its multipliers).")
QVR_TOGGLE(vr_tracers_enemies, "Enemies' Tracers", "The grunts' shots too.")
QVR_TOGGLE(vr_lg_water, "Lightning Gun in Water",
    "Fired with your body waist deep, or your hand or the gun's muzzle in water, the lightning gun shocks you and both hands drop what they hold; the shock spreads through the water to what is in it. Fired into water from outside, it electrifies the water round where the beam goes in. Off: Quake's discharge (every cell at once, all round you).")
QVR_TOGGLE(vr_hotblade, "Incandescent Blades",
    "Hold a melee weapon's blade in a flame (a torch on its wall or in your other hand, a brazier, the map's "
    "flames) or dip it in lava: it glows red-hot for a while, hits harder and sets what it strikes on fire. "
    "The axe (the Shadow Axe too), the knights' swords, the crowbar, the Super Axe and the chainsaw; not "
    "Mjolnir. The heat is the weapon's: it cools held, holstered or dropped alike.")
QVR_SLIDER(vr_hotblade_time, "Stays Hot", 5.f, 120.f, 5.f, "%.0f s", ext(1.f, 600.f),
    "How long it stays incandescent. A flame again while it glows makes it full again.")
QVR_SLIDER(vr_hotblade_fade, "Fades Over", 0.f, 15.f, 0.5f, "%.1f s", ext(0.f, 60.f),
    "Its glow is full until these last seconds, then fades out (0: full to the end).")
QVR_TOGGLE(vr_hotblade_ignite, "Sets Enemies on Fire",
    "Its blows set living enemies on fire (Combat > Burning: the damage and how long).")
QVR_TOGGLE(vr_hotblade_ignite_corpses, "Sets Corpses on Fire", "And corpses (with Combat > Burning > Corpses Burn on).")
QVR_TOGGLE(vr_hotblade_quench, "Water Quenches",
    "Dipped in water or slime, it cools at once, with a hiss and a puff of steam.")
QVR_SLIDER(vr_hotblade_glow, "Glow Brightness", 0.f, 2.f, 0.05f, "%.2f", noExt,
    "How brightly the heated part of the blade glows (not the handle), held, holstered or lying on the "
    "ground, and its embers (0: no glow, no light, no embers, no trail).")
QVR_SLIDER(vr_hotblade_light, "Light", 0.f, 2.f, 0.05f, "%.2f", noExt,
    "How bright the light the glowing blade casts round it is; it follows the glow, rising as it heats and "
    "dying as it fades (0: none).")
QVR_SLIDER(vr_hotblade_embers, "Embers per Puff", 0.f, 10.f, 1.f, "%.0f", ext(0.f, 32.f),
    "How many embers leave the heated part of the blade at a time, from all over it (0: none).")
QVR_SLIDER(vr_hotblade_trail, "Swing Trail", 0.f, 3.f, 0.1f, "%.1f", noExt,
    "How much fire and how many sparks a swung (or thrown) glowing blade leaves hanging in the air behind its "
    "heated part; a faster swing leaves more (0: none).")

// vr_menu_stealth.inc: pageStealth
QVR_TOGGLE(vr_ai_enhanced, "Enhanced AI",
    "Every rule on this page at once. Off: Quake's monster AI, exactly (they see you at once, hear nothing); "
    "switched mid-game, every monster goes back to it at once (an investigating monster stops, idle). For a "
    "map that doesn't play well with the stealth rules.")
QVR_TOGGLE(vr_stealth_meter, "Suspicion Meter",
    "A monster that sees you fills a meter, faster the more light is on you, the nearer, the more in front "
    "of it, the more you move (crouching slows it). Partly full it turns to look (Alert); full, it attacks. "
    "Off: Quake's sight (seen at once in its view, or near).")
QVR_TOGGLE(vr_stealth_contact, "Touching Gives You Away",
    "Your body, either hand or a prop you hold touching a monster: it spots you at once. Off: brushing past "
    "one is unnoticed; taking hold of it (grabbing) still gives you away.")
QVR_SLIDER(vr_stealth_sneak, "Sneak Attack Damage", 1.f, 3.f, 0.05f, "x%.2f", ext(0.f, 10.f),
    "Any damage you deal (a blow, a shot, an explosion) to a monster that hasn't noticed anything. 1: none.")
QVR_TOGGLE(vr_stealth_noise, "Monsters Hear Noises",
    "Noises draw monsters (Alert: they come to look): your running, hard landings, props knocking, blows, "
    "shots, explosions; louder ones farther. A noise you make in a monster's sight gives you away. Walls "
    "muffle them; a thrown prop draws them to where it lands, not to you.")
QVR_SLIDER(vr_stealth_run_speed, "Running From", 100.f, 400.f, 10.f, "%.0f u/s", noExt,
    "Moving faster than this is running, and heard (full stick: 320; walking and crouching are quiet). "
    "Climbing and shimmying are silent.")
QVR_SLIDER(vr_stealth_noise_guns, "Shots Heard", 0.f, 3.f, 0.05f, "x%.2f", noExt,
    "Times each weapon's reach (a shotgun 1100 units, the super shotgun 1300, nailguns 800-900).")
QVR_SLIDER(vr_stealth_noise_blasts, "Explosions Heard", 0.f, 3.f, 0.05f, "x%.2f", noExt,
    "Times an explosion's reach (8 units a point of its damage, at least 800: a rocket's or a grenade's 960).")
QVR_TOGGLE(vr_stealth_gates, "Through Teleporters",
    "Seamless teleporters that come out of another gate (Graphics > Teleporters): every monster sees you "
    "through them (not only ranged ones), noises carry through them as through an opening, and an Alert "
    "monster walks through to look (Monsters Walk Through on). Off: sight through them is ranged "
    "monsters' only, and noises stay in their room.")
QVR_TOGGLE(vr_stealth_seethrough, "See-Through Walls",
    "Grates, fences, webs and glass (see-through textures and walls): monsters see you through them, your "
    "flashlight's beam reaches them through them, and noises carry through as in the open. Off: they block "
    "sight and muffle sound as solid walls.")
QVR_TOGGLE(vr_stealth_ambush_deaf, "Deaf Ambushers",
    "Monsters a map placed in ambush don't hear noises or other monsters (Quake's meaning of the flag); "
    "they still see you.")
QVR_TOGGLE(vr_stealth_senses, "Senses by Kind",
    "Dogs hear far better, fiends and shamblers better, zombies are near deaf and half blind, ogres a bit "
    "dull.")
QVR_TOGGLE(vr_stealth_investigate, "Walk to It",
    "An Alert monster turns to what it noticed, then walks there and searches; off: it only turns and looks.")
QVR_TOGGLE(vr_stealth_lose, "Lose You",
    "A monster hunting you that lost sight of you hunts on for a while, then gives up: it searches where "
    "your trail ended and goes back (Alert). Off: it hunts you for ever (Quake's).")
QVR_SLIDER(vr_stealth_lose_time, "Gives Up After", 2.f, 120.f, 1.f, "%.0f s", ext(0.5f, 600.f), "How long without seeing you before it gives up.")
QVR_SLIDER(vr_stealth_lose_far, "Sooner Beyond", 0.f, 6000.f, 250.f, "%.0f units", noExt,
    "With you farther than this it gives up after a quarter of that time. 0: never sooner.")
QVR_TOGGLE(vr_stealth_chase_trail, "Follows Your Trail",
    "Out of sight, it goes to where it saw you last (round the corner you went round), then on the way you "
    "were going, then heads for you. Off: straight at you, as Quake's monsters (they know where you are).")
QVR_SLIDER(vr_stealth_chase_predict, "Guesses Ahead", 0.f, 5.f, 0.25f, "%.2f s", noExt,
    "How far along your way it guesses you went: this many seconds of your speed when it last saw you.")
QVR_TOGGLE(vr_ai_props, "Round Props",
    "A monster whose way is blocked by a crate, a barrel or another solid prop walks round it (to its side "
    "nearer where it is going). Off: Quake's way, which only steps at random and can stay stuck behind it.")
QVR_TOGGLE(vr_ai_prop_push, "Push Props",
    "And pushes the prop as it meets it, by its own weight against the prop's: a dog hardly moves a crate, "
    "an ogre shoves it.")
QVR_SLIDER(vr_stealth_share_near, "Near Monsters Join", 0.f, 1000.f, 16.f, "%.0f units", noExt,
    "A monster this near another that turns Alert or attacks (in line, not through a wall) does too. 0: off.")
QVR_SLIDER(vr_stealth_share_view, "Monsters in Sight Join", 0.f, 3000.f, 50.f, "%.0f units", noExt,
    "One that sees another attack within this does too (turning Alert: within half). 0: off.")
QVR_TOGGLE(vr_stealth_flashlight, "Flashlight Beams",
    "Your flashlight on a monster: it comes towards the light (and its glare shows you). Its beam seen "
    "landing nearby: it looks about for where it came from.")
QVR_TOGGLE(vr_stealth_gem, "Visibility Gem",
    "A gem on the wrist gadget: its glow how visible you are (the light on you, crouching), a ring round it "
    "how loud your last noise was.")
QVR_TOGGLE(vr_stealth_marks, "? and ! Marks",
    "A ? rises over a monster turning Alert, a ! over one that spots you.")

// vr_menu_stealth.inc: pageStealthTests
QVR_TOGGLE(vr_stealth_debug_meters, "Meters Over Monsters",
    "Each monster's state (idle, ALERT and its step, HOSTILE and how long it hasn't seen you) and its "
    "suspicion meter as a bar over it, through walls: to watch the meter rise and drain while tuning.")
QVR_SLIDER(vr_stealth_debug_meters_range, "Meters Within", 250.f, 6000.f, 250.f, "%.0f units", noExt, nullptr)
QVR_SLIDER(vr_stealth_debug, "Log", 0.f, 2.f, 1.f, "%.0f", noExt,
    "1: each monster's state changes (Alert, Hostile, Idle again); 2: also each meter step and noise heard.")
QVR_SLIDER(vr_stealth_light_force, "Force the Light on You", -1.f, 160.f, 1.f, "%.0f", noExt, "Tests: the light on you as this (-1: measured).")

// vr_menu.cpp: dummyHealthSlider
QVR_SLIDER(vr_dummy_health, "Dummy Health", 0.f, 1000.f, 5.f, "%.0f", ext(1.f, 100000.f),
    "The training dummy's full health: what its hits take away (shown over it) and, with Dummy Dies, "
    "what kills it. Its Own: its enemy's (a grunt's 30, an ogre's 200).")

// vr_menu.cpp: dynresMaxSlider
QVR_SLIDER(vr_dynres_max, "Dynamic Highest Scale", 0.45f, 1.5f, 0.05f, "%.2f", ext(0.45f, 2.f),
    "The highest scale Dynamic Resolution goes to, with headroom. Render Scale: yours (the default), so "
    "with headroom nothing changes; above it, sharper while the graphics card keeps up.")

// vr_menu.cpp: debrisMultiplayerSlider
QVR_SLIDER(vr_debris_mp_max, "Most in Multiplayer", -8.f, 160.f, 8.f, "%.0f", ext(-8.f, 400.f),
    "The most in a multiplayer map (yours as the host). They are the server's, to pick up and throw: "
    "each one in sight costs every player's network packets (about 18 bytes a frame). Single Player's: "
    "as many as Most in a Map. 0: none. Next map.")

// vr_menu.cpp: pageMotionRecorder
QVR_TOGGLE(vr_motion_armed, "Arm Recorder",
    "Armed: click the Record Button to start a take of the Category, click it again to end it and save "
    "it (quakevr/motions). A beep and a buzz each time; REC in view.")
QVR_CYCLE(vr_motion_button, "Record Button",
    "Pressed to start and to end a take, while armed. Its own action (stick clicks: run, reload; A: jump; "
    "B, Y: weapons; X: reload; grips: grab) rests until you disarm. Hold B or Y + trigger: the trigger "
    "fires as usual without it.",
    {0.f, "Off stick click"}, {1.f, "Main stick click"}, {2.f, "A (main lower)"}, {3.f, "B (main upper)"},
    {4.f, "X (off lower)"}, {5.f, "Y (off upper)"}, {6.f, "Off grip"}, {7.f, "Main grip"},
    {8.f, "Hold B + main trigger"}, {9.f, "Hold Y + off trigger"})
QVR_SLIDER(vr_motion_preroll, "Lead-in", 0.f, 2.f, 0.1f, "%.1f s", ext(),
    "Kept from before the take starts (the motion's start, for the melee's trackers).")
QVR_SLIDER(vr_motion_tail, "Tail", 0.f, 1.f, 0.05f, "%.2f s", ext(), "Recorded after the take ends (hits that land late).")

// vr_menu.cpp: pageReviewTakes
QVR_CYCLE(vr_motion_review_show, "Show",
    "To Review: failing or suspect, not yet kept. A take picked opens its page: play it as a ghost, "
    "keep, discard or relabel it.",
    {0.f, "To Review"}, {1.f, "Failing"}, {2.f, "Suspect"}, {3.f, "Not Evaluated"}, {4.f, "Reviewed"}, {5.f, "All"},
    {6.f, "Discarded"})

// vr_menu.cpp: pageReviewTake
QVR_CYCLE(vr_motion_review_speed, "Ghost Speed", nullptr,
    {1.f, "1x"}, {0.5f, "0.5x"}, {0.25f, "0.25x"}, {0.1f, "0.1x"})

// vr_menu.cpp: pageGadget
QVR_SLIDER(vr_gadget_scale, "Size", 0.5f, 2.f, 0.05f, "%.2fx", ext(0.25f, 3.f), nullptr)
QVR_SLIDER(vr_gadget_x, "Along the Arm", -15.f, 15.f, 0.5f, "%.1f cm", ext(-40.f, 40.f), nullptr)
QVR_SLIDER(vr_gadget_y, "Across the Arm", -8.f, 8.f, 0.25f, "%.2f cm", ext(-20.f, 20.f), nullptr)
QVR_SLIDER(vr_gadget_z, "Height", -5.f, 8.f, 0.25f, "%.2f cm", ext(-15.f, 25.f), "How far it stands out of the forearm.")
QVR_SLIDER(vr_gadget_pitch, "Pitch", -90.f, 90.f, 5.f, "%.0f deg", ext(-180.f, 180.f), nullptr)
QVR_SLIDER(vr_gadget_yaw, "Yaw", -90.f, 90.f, 5.f, "%.0f deg", ext(-180.f, 180.f), nullptr)
QVR_SLIDER(vr_gadget_roll, "Roll", -180.f, 180.f, 15.f, "%.0f deg", noExt,
    "Turns the screen: 90 reads along the arm, 180 turns the text the other way.")

// vr_menu.cpp: pageDoom
QVR_CYCLE(vr_doom_controls, "Controls",
    "Touch Only: everything on the screen with your fingertip; the sticks and buttons stay Quake's. "
    "Controller and Touch: also, while Doom has the focus (the gadget raised and looked at, or touched), "
    "the gadget hand's stick moves and turns, trigger fires, grip uses, X next weapon, Y Doom's menu, "
    "stick press the map; your other hand stays Quake's. Controller Only: no touch controls.",
    {0.f, "Touch Only"}, {1.f, "Controller and Touch"}, {2.f, "Controller Only"})
QVR_CYCLE(vr_doom_overlay, "Touch Controls Shown",
    "The buttons drawn over Doom's picture (with Touch Only: always, faint when your finger is away).",
    {0.f, "When Your Finger Is Near"}, {1.f, "Always"}, {2.f, "Never"})
QVR_TOGGLE(vr_doom_hard_fire, "Hard Press Fires",
    "Pressing through the screen (Hard Press Depth) while steering with the MOVE stick, or on the "
    "picture, fires: one finger can move and shoot.")
QVR_TOGGLE(vr_doom_always_run, "Always Run", "Doom's always run (RUN on the touch bar).")
QVR_SLIDER(vr_doom_focus_look, "Look to Focus", 0.f, 2.f, 0.1f, "%.1f s", noExt,
    "How long you look at the raised gadget before its stick and buttons go to Doom (0: only a touch).")
QVR_SLIDER(vr_doom_focus_grace, "Out of View to Unfocus", 0.1f, 3.f, 0.1f, "%.1f s", noExt,
    "How long the gadget may be out of view (lowered, looked away from) before they come back to Quake.")
QVR_SLIDER(vr_doom_light_presses, "Light Button Presses", 0.f, 10.f, 1.f, "%.0f", noExt,
    "Press the gadget's side button (the light on/off one) this many times in quick succession to open "
    "Doom on the gadget; one press of it then closes Doom again, without toggling the light (0: off). "
    "The light itself keeps toggling with each press and is put back as it was.")
QVR_SLIDER(vr_doom_light_gap, "Light Press Gap", 0.1f, 1.f, 0.05f, "%.2f s", noExt, "The longest time between two of those presses.")
QVR_TOGGLE(vr_doom_pause_unfocused, "Pause When Not Focused",
    "Off: Doom keeps running while you play Quake. On: it pauses until you look at it or touch it.")
QVR_TOGGLE(vr_doom_aspect, "4:3 Picture",
    "Doom's picture as it was shown (4:3, bars at the sides); off: stretched over the screen.")
QVR_SLIDER(vr_doom_crt, "CRT Look", 0.f, 2.f, 0.1f, "%.1fx", noExt, "Times the gadget's CRT look (0: a clean picture).")
QVR_SLIDER(vr_doom_volume, "Volume", 0.f, 1.f, 0.05f, "%.2f", noExt, "Doom's sounds, from the gadget, against the game's.")
QVR_TOGGLE(vr_doom_panel, "Big Floating Panel",
    "Doom on a big panel in front of you instead of the gadget (placed as you turn this on, it moves with "
    "you; touch it as the gadget).")
QVR_SLIDER(vr_doom_panel_size, "Panel Width", 0.2f, 1.5f, 0.05f, "%.2f m", noExt, nullptr)
QVR_SLIDER(vr_touch_press, "Touch Distance", 0.f, 2.f, 0.1f, "%.1f cm", noExt,
    "How near the screen your fingertip must come for a touch (it must come from in front).")
QVR_SLIDER(vr_touch_release, "Release Distance", 0.3f, 4.f, 0.1f, "%.1f cm", noExt, "How far off it a touch ends.")
QVR_SLIDER(vr_touch_hard, "Hard Press Depth", 0.5f, 4.f, 0.1f, "%.1f cm", noExt, "How far through the screen is a hard press.")
QVR_SLIDER(vr_touch_haptic, "Touch Haptics", 0.f, 1.f, 0.05f, "%.2f", noExt, "Your hand's buzz as a touch begins.")

// vr_menu.cpp: pageTips
QVR_CYCLE(vr_tips, "Tips",
    "Tips for new players, each shown once: the first time you come near something you can use (a wall "
    "torch). Floating Panel: a screen like the maps' text boards by it, with a cable to it. Wrist Gadget: in the gadget's hologram, waiting until "
    "you look at it (it chimes and buzzes); the panel if the HUD is not the gadget.",
    {0.f, "Off"}, {1.f, "Floating Panel"}, {2.f, "Wrist Gadget"})
QVR_SLIDER(vr_tips_distance, "Distance", 50.f, 400.f, 10.f, "%.0f", ext(16.f, 2000.f),
    "How near you must come to what a tip is about (in Quake units: about 40 a metre).")
QVR_TOGGLE(vr_tips_line_of_sight, "Line of Sight", "Only when no wall is between your eyes and it.")
QVR_SLIDER(vr_tips_view_angle, "View Angle", 0.f, 90.f, 5.f, "%.0f deg", ext(0.f, 180.f),
    "How far from where you look it may be (0: anywhere, even behind you).")
QVR_SLIDER(vr_tips_delay, "Delay", 0.f, 3.f, 0.25f, "%.2f s", ext(0.f, 10.f), "How long it must stay near and seen before the tip shows.")
QVR_SLIDER(vr_tips_time, "Time Shown", 3.f, 30.f, 1.f, "%.0f s", ext(1.f, 120.f), "How long a tip shows (on the gadget: once you look at it).")
QVR_CYCLE(vr_tips_facing, "Panel Facing",
    "Towards your eyes: the floating screen turns to face you and stays level. Square to your view: it is "
    "always flat in front of you, as if on your view, tilting as you tilt your head.",
    {0.f, "Towards Your Eyes"}, {1.f, "Square to Your View"})
QVR_SLIDER(vr_tips_size, "Panel Text Size", 0.5f, 2.f, 0.1f, "%.1fx", ext(0.25f, 4.f), "How big the writing on a tip's floating panel is.")

// vr_menu.cpp: pageScreens
QVR_TOGGLE(vr_gadget_show_level, "Level and Stats", nullptr)
QVR_TOGGLE(vr_dead_hide_gear, "Hide Gear When Dead",
    "While you are dead the holstered guns, the ammo pouch, the grenade pouch and the wrist gadget are hidden "
    "(the flashlight is, always) and the HUD is "
    "Quake's status bar on a hand; all back when you respawn or load a save.")
QVR_TOGGLE(vr_gadget_stamina, "Stamina and Counters",
    "The STAMINA row shows your stamina (parries, shoves and blows spend it) and COUNTER while a counter-attack's window is open.")
QVR_SLIDER(vr_gadget_light, "Screen Light", 0.f, 3.f, 0.1f, "%.1fx", ext(),
    "The screen casts a light in its colour the way it faces, and a faint one on your hand (0 off).")
QVR_TOGGLE(vr_gear_lights, "Gear Lights",
    "Off: your gear's lights dim, for sneaking about in the dark: the light the gadget's and your guns' "
    "screens cast (monsters see you by it) and their glow, and the screens themselves. The button on the "
    "gadget's lower edge (the inner one) toggles this with a click; bindable: vr_gear_lights_toggle. The "
    "flashlight keeps its own switch.")
QVR_SLIDER(vr_gear_lights_dim, "Lights When Dimmed", 0.f, 0.5f, 0.01f, "%.2fx", ext(0.f, 1.f),
    "While off: the screens' cast light and glow at this share (0: none at all).")
QVR_SLIDER(vr_gear_lights_screen_dim, "Screens When Dimmed", 0.05f, 1.f, 0.05f, "%.2fx", ext(0.f, 1.f),
    "While off: the screens' text, numbers and the hologram at this share of their brightness, readable "
    "up close.")
QVR_TOGGLE(vr_gadget_button, "Side Button",
    "The button on the gadget's lower edge, pressed with your other hand's fingertip, toggles the gear "
    "lights.")
QVR_SLIDER(vr_gadget_button_size, "Button Size", 1.f, 8.f, 0.25f, "%.2f cm", ext(0.3f, 20.f),
    "How near the button's middle your fingertip must come (from the button's side: never from over the "
    "screen). Show the Button shows it.")
QVR_SLIDER(vr_gadget_button_x, "Button Across", -6.f, 6.f, 0.25f, "%.2f cm", ext(-20.f, 20.f),
    "The button's hit spot moved along the screen's width (positive: to the screen's right).")
QVR_SLIDER(vr_gadget_button_y, "Button Up", -6.f, 6.f, 0.25f, "%.2f cm", ext(-20.f, 20.f),
    "Moved along the screen's height (negative: further out past its lower edge).")
QVR_SLIDER(vr_gadget_button_z, "Button Out", -6.f, 6.f, 0.25f, "%.2f cm", ext(-20.f, 20.f),
    "Moved out of the screen's face (negative: down towards your arm).")
QVR_SLIDER(vr_gadget_button_pitch, "Button Tilt Out", -90.f, 90.f, 5.f, "%.0f deg", noExt,
    "The side the button is pressed from (the faint disc: no press from behind it) tilted out of the "
    "screen's face (negative: towards your arm).")
QVR_SLIDER(vr_gadget_button_yaw, "Button Tilt Across", -90.f, 90.f, 5.f, "%.0f deg", noExt, "... and along the screen's width (positive: to its right).")
QVR_TOGGLE(vr_gadget_fingertip_drawn, "Drawn Fingertip",
    "On: the fingertip that presses the button is your drawn hand's index fingertip, as the hand is posed "
    "(point with it). Off: Fingertip Reach ahead of your hand's point.")
QVR_SLIDER(vr_gadget_button_reach, "Fingertip Reach", -5.f, 15.f, 0.5f, "%.1f cm", ext(-20.f, 30.f),
    "With Drawn Fingertip off (or no jointed hand): where your fingertip is taken to be, ahead of your "
    "hand's point.")
QVR_SLIDER(vr_gadget_fingertip_x, "Fingertip Forward", -6.f, 6.f, 0.25f, "%.2f cm", ext(-20.f, 20.f),
    "The fingertip moved along your hand's forward (negative: back). Show the Button shows it: the drawn "
    "fingertip white, joined to the one that presses.")
QVR_SLIDER(vr_gadget_fingertip_y, "Fingertip Outward", -6.f, 6.f, 0.25f, "%.2f cm", ext(-20.f, 20.f),
    "... away from your other hand (the right hand's right; negative: towards it).")
QVR_SLIDER(vr_gadget_fingertip_z, "Fingertip Up", -6.f, 6.f, 0.25f, "%.2f cm", ext(-20.f, 20.f), "... and up from the back of your hand (negative: down).")
QVR_SLIDER(vr_gadget_fingertip_pitch, "Fingertip Pitch", -45.f, 45.f, 1.f, "%.0f deg", ext(-180.f, 180.f),
    "The fingertip turned round your hand's point: down (negative: up), before the moves above.")
QVR_SLIDER(vr_gadget_fingertip_yaw, "Fingertip Yaw", -45.f, 45.f, 1.f, "%.0f deg", ext(-180.f, 180.f), "... inward, towards your other hand (negative: outward).")
QVR_SLIDER(vr_gadget_fingertip_roll, "Fingertip Roll", -45.f, 45.f, 1.f, "%.0f deg", ext(-180.f, 180.f),
    "... rolled about your hand's forward (positive: its top outward).")
QVR_SLIDER(vr_gadget_button_cooldown, "Button Cooldown", 0.f, 2.f, 0.1f, "%.1f s", ext(0.f, 5.f),
    "After a press counts, how long before the next one does (no double toggles from a bounce).")
QVR_CYCLE(vr_debug_gadget_button, "Show the Button",
    "Draws the button's hit volume (green ready, yellow pressed, red cooling down; the faint disc: no "
    "press from behind it, the short line the side it is pressed from) and your fingertip (the drawn "
    "index fingertip white, joined to the tuned one that presses); And the Screen Tap: also bullet time's "
    "tap zone over the screen and what strikes it (your hand's surface, the gun's butt). Presses are "
    "printed; And Print Sync also prints, each frame, how far the zone is from the drawn gadget.",
    {0.f, "Off"}, {1.f, "Its Hit Volume"}, {2.f, "And the Screen Tap"}, {3.f, "And Print Sync"})
QVR_SLIDER(vr_gadget_crt, "CRT Look", 0.f, 2.f, 0.1f, "%.1fx", ext(),
    "Scanlines, a slight flicker, faint static and now and then a glitch (0 off).")
QVR_SLIDER(vr_screen_glow, "Screen Glow", 0.f, 3.f, 0.1f, "%.1fx", ext(),
    "The gadget's and your weapons' screens glow softly round their edges (0 off).")
QVR_SLIDER(vr_screen_text_glow, "Text Glow", 0.f, 3.f, 0.1f, "%.1fx", ext(),
    "The text, numbers and icons on those screens glow: bright whitish cores, a soft halo (0 off).")
QVR_TOGGLE(vr_messages_hologram, "Game Messages as Hologram",
    "The game's messages (a key needed, a secret found, the map's text, pickups) float as a hologram the gadget projects over its screen, while you look at it. Else the key and map messages show in front of you.")
QVR_SLIDER(vr_messages_hologram_time, "Hologram Time", 2.f, 15.f, 0.5f, "%.1f s", ext(), "How long a message stays in the hologram.")
QVR_SLIDER(vr_messages_hologram_size, "Hologram Text Size", 0.5f, 2.f, 0.05f, "%.2fx", ext(0.25f, 4.f), nullptr)
QVR_SLIDER(vr_messages_hologram_height, "Hologram Height", 0.f, 15.f, 0.5f, "%.1f cm", ext(0.f, 50.f), "How high over the gadget it floats.")
QVR_SLIDER(vr_messages_hologram_effect, "Hologram Effect", 0.f, 2.f, 0.1f, "%.1fx", ext(),
    "The beam of light from the screen, scanlines, flicker, glitches and the projection as it appears (0: plain glowing text).")
QVR_TOGGLE(vr_messages_hologram_only, "Messages Only on the Gadget",
    "The game's messages never show in front of you: they wait in the hologram until you look at the gadget. A new one (a key needed, a secret, the map's text; not pickups) chimes from the gadget on your wrist and buzzes it.")
QVR_SLIDER(vr_view_text_smooth, "Messages in View: Follow Smoothing", 0.f, 0.5f, 0.02f, "%.2f s", ext(0.f, 2.f),
    "The messages floating in front of you (VR Calibration's and the maps' texts, Body Calibration's poses, "
    "the game's messages when not on the gadget) turn after your head this smoothly: about how long they "
    "take to catch up. 0: fixed to your view.")
QVR_TOGGLE(vr_hud_console_log, "Console Log on the HUD",
    "The console's log (the engine's own lines: warnings, settings changed, command output) shows with the game's messages, over the gadget or in view (and at the top of the flat screen). Off: only the game's messages (pickups, deaths, chat); the console (~) keeps everything.")
QVR_CYCLE(vr_notify_wrist, "Console Messages",
    "The console's other messages (the engine's: settings changed, cheats, errors) float in a small log over the gadget, or at the top of the view.",
    {1.f, "Over the gadget"}, {2.f, "Both"}, {0.f, "In view"})
QVR_SLIDER(vr_notify_wrist_time, "Console Message Time", 2.f, 30.f, 1.f, "%.0f s", ext(), "How long a message stays in the gadget's log.")
QVR_SLIDER(vr_notify_wrist_height, "Console Log Height", 0.f, 20.f, 0.5f, "%.1f cm", ext(0.f, 60.f),
    "How high over the gadget the log floats (always over the hologram).")
QVR_SLIDER(vr_notify_wrist_alpha, "Console Log Brightness", 0.2f, 1.f, 0.05f, "%.2f", ext(), nullptr)
QVR_TOGGLE_BYNAME(vr_show_weapon_text, "Weapon Text", "Show floating ammunition text attached to weapons")
QVR_TOGGLE_BYNAME(vr_weapon_screen, "Weapon Ammo Screen",
    "The ammunition text on a small screen on the weapon (colours from the wrist gadget's screen).")
QVR_SLIDER_BYNAME(vr_weapon_screen_padding, "Ammo Screen Margin", 0.f, 2.f, 0.1f, "%.1f", ext(), nullptr)
QVR_SLIDER_BYNAME(vr_weapon_screen_crt, "Ammo Screen CRT Look", 0.f, 2.f, 0.1f, "%.1fx", ext(),
    "Scanlines, a slight flicker, faint static and now and then a glitch, as on the wrist gadget's screen (0 off).")
QVR_TOGGLE(vr_ammo_color_warn, "Low Ammo Colours",
    "A held gun's ammo numbers turn orange when less than a quarter of its magazine is left (Low Ammo "
    "Fraction), and red when it is empty (nothing loaded). With reloading off, the ammo it fires from "
    "against a magazine's worth. Off: always the screen's colour.")
QVR_SLIDER(vr_ammo_color_low, "Low Ammo Hue", 0.f, 355.f, 5.f, "%.0f", noExt,
    "The numbers' colour when ammo is low: 30 orange, 55 yellow, 0 red, 200 blue (console: \"r g b\" too).")
QVR_SLIDER(vr_ammo_color_empty, "Empty Ammo Hue", 0.f, 355.f, 5.f, "%.0f", noExt,
    "The numbers' colour when the gun is empty: 0 red, 30 orange, 280 purple (console: \"r g b\" too).")
QVR_SLIDER(vr_ammo_low_frac, "Low Ammo Fraction", 0.05f, 0.75f, 0.05f, "%.2f", ext(0.f, 1.f),
    "Below this share of the magazine's capacity, the numbers turn the low colour (0.25: a quarter).")
QVR_TOGGLE_BYNAME(vr_weapon_screen_idle, "Screens on Weapons at Rest",
    "Weapons in your holsters and lying in the world show their ammo screen and button too, not only the ones in your hands.")
QVR_SLIDER_BYNAME(vr_weapon_world_attach_range, "Lying Weapons' Parts Range", 250.f, 5000.f, 250.f, "%.0f units", ext(0.f, 20000.f),
    "How far off a weapon lying in the world still shows its magazine, ammo screen and button (Quake units: 32 is about a metre).")
QVR_SLIDER_BYNAME(vr_weapon_world_attach_max, "Lying Weapons With Parts", 0.f, 48.f, 1.f, "%.0f", noExt,
    "How many of the weapons lying nearest show their magazine, ammo screen and button.")
QVR_TOGGLE_BYNAME(vr_worldtext_crt, "Map Boards as CRTs",
    "The text boards in maps (the tutorial's, the start map's) are CRT screens with glowing text, as the wrist gadget's. Off: plain text.")
QVR_SLIDER_BYNAME(vr_worldtext_hue, "Map Board Hue", 0.f, 355.f, 5.f, "%.0f", noExt, "Their colour: 40 amber, 128 green, 200 blue, 0 red.")

// vr_menu.cpp: pageColours
QVR_SLIDER(vr_player_hue, "Player Effects Hue", 0.f, 355.f, 5.f, "%.0f", noExt,
    "One colour for all your effects: the wrist gadget's screen, your weapons' screens and sights, the force grab, the teleport arc, the crosshair, the menu laser. 128 green, 40 amber, 200 blue, 0 red.")
QVR_SLIDER(vr_player_saturation, "Player Effects Saturation", 0.f, 2.f, 0.05f, "%.2f", noExt, "How colourful they are: 1 as made, 0 white.")
QVR_SLIDER(vr_gadget_screen_brightness, "Screen Brightness", 0.3f, 1.5f, 0.05f, "%.2f", ext(), nullptr)
QVR_SLIDER(vr_gadget_screen_background, "Screen Background", 0.f, 4.f, 0.1f, "%.1f", ext(),
    "How bright the screen's dark background is in its colour (the text reads best dark: 1).")
QVR_SLIDER(vr_gadget_screen_text_white, "Screen Text Whiteness", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How white the screen's numbers and values are: 1 white, 0 the screen's colour (as its labels). Warnings stay red.")
QVR_SLIDER(vr_gadget_tint, "Casing Tint", 0.f, 1.f, 0.05f, "%.2f", noExt, "0 keeps the casing's own olive drab.")
QVR_SLIDER(vr_gadget_tint_hue, "Casing Tint Hue", 0.f, 355.f, 5.f, "%.0f", noExt, nullptr)
QVR_SLIDER(vr_sight_saturation, "Weapon Sight Saturation", 0.f, 2.f, 0.05f, "%.2f", noExt, "1 their own, 0 white.")
QVR_SLIDER(vr_forcegrab_saturation, "Force Grab Saturation", 0.f, 2.f, 0.05f, "%.2f", noExt,
    "How colourful they are: 1 as made, 0 white, 0.7 like the wrist gadget's screen. Times the Player Effects Saturation.")

// vr_menu.cpp: pageForceGrab
QVR_TOGGLE(vr_forcegrab_mode, "Force Grab", "Pull pickups and weapons to your hand from afar.")
QVR_TOGGLE(vr_forcegrab_bodies_block, "Bodies Block",
    "An enemy (or a body) between your hand and an object hides it from the force grab, as a wall does. "
    "Off: only walls do.")
QVR_SLIDER(vr_forcegrab_catch_late, "Catch Late", 0.f, 0.5f, 0.05f, "%.2f s", ext(), nullptr)
QVR_TOGGLE(vr_forcegrab_eligible_particles, "Pointing Particles",
    "Sparkles on the object an empty hand points at, that it can pull.")
QVR_TOGGLE(vr_forcegrab_eligible_haptics, "Pointing Haptics",
    "A tick in the hand when it points at a new object it can pull.")
QVR_TOGGLE_BYNAME(vr_forcegrab_fx, "Effects",
    "A faint beam to what you point at, a crackling tendril when locked on, a trail behind what flies to you.")
QVR_SLIDER(vr_forcegrabbable_box_scale, "Ammo/Health Box Size", 0.1f, 1.f, 0.05f, "%.2f", ext(), "Takes effect on the next map.")

// vr_menu.cpp: pageGrapple
QVR_CYCLE(vr_grapple_rope, "Rope",
    "Holds: the hook bites and the rope holds you at its length (swing on it, walk closer), pulling nothing "
    "until you hold B or Y (either hand's, when the other has no hook of its own out); A or X in the air "
    "pays it out. The hook stays in when you drop the gun, holster it or pass it to the other hand. "
    "Pulls at once: the mission pack's grapple, pulling you in as soon as it bites.",
    {1.f, "Holds"}, {0.f, "Pulls at once"})
QVR_CYCLE(vr_grapple_trigger_release, "Trigger Released",
    "Stays in: letting go of the trigger does nothing; the back button on the gun (over the grip) detaches "
    "the hook, the front one (near the muzzle) reels it straight in. Comes back: letting go brings it "
    "straight into the gun. Comes loose: letting go takes it off, hanging on the rope until you reel it in "
    "(B or Y).",
    {0.f, "Hook Stays In"}, {1.f, "Hook Comes Back"}, {2.f, "Hook Comes Loose"})
QVR_SLIDER(vr_grapple_drop_grace, "Drop Grace", 0.f, 0.5f, 0.02f, "%.2f s", ext(0.f, 2.f),
    "Comes Back and Comes Loose: the trigger let go waits this long before the hook comes off: dropping the "
    "gun, holstering it or passing it to the other hand in that time (letting go of trigger and grip "
    "together) keeps the hook in.")
QVR_SLIDER(vr_grapple_max_length, "Longest Rope", 200.f, 3000.f, 50.f, "%.0f", ext(64.f, 10000.f),
    "The rope is never longer (units): the hook flies no farther, the unreel pays out no more.")
QVR_TOGGLE(vr_grapple_shootable, "Shoot the Hook Off",
    "A hook in a wall, a floor or a door comes off when you shoot it (any weapon): get it back when it is "
    "stuck or its gun is out of reach.")
QVR_SLIDER(vr_grapple_quick_time, "Reel-In Button Time", 0.f, 2.f, 0.05f, "%.2f s", ext(0.f, 10.f),
    "How long the front button takes to reel the hook in: it speeds up, then slows down into the gun (a "
    "short rope quicker). 0: it flies straight back at the Reel-In Button Speed.")
QVR_SLIDER(vr_grapple_quick_speed, "Reel-In Button Speed", 300.f, 4000.f, 100.f, "%.0f u/s", ext(100.f, 10000.f),
    "The fastest the hook comes back when you press the front button (a long rope takes longer than the "
    "Reel-In Button Time), and its speed with that time at 0.")
QVR_SLIDER(vr_grapple_hook_scale, "Hook Size", 0.3f, 1.f, 0.05f, "%.2f", ext(0.2f, 2.f),
    "The size of the hook the gun fires (1: the mission pack's, too big for the gun). From the next shot.")
QVR_TOGGLE(vr_grapple_front_button, "Front Button",
    "The gun's second button, near the muzzle: press it with the other hand's finger and the hook comes "
    "straight back into the gun (through anything). The back button, over the grip, only detaches it.")
QVR_SLIDER(vr_grapple_front_button_x, "Front Button Along", -20.f, 30.f, 0.1f, "%.1f", ext(),
    "Where the front button is: from the back button along the gun, towards the muzzle (model units).")
QVR_SLIDER(vr_grapple_front_button_y, "Front Button Side", -10.f, 10.f, 0.1f, "%.1f", ext(),
    "From the back button, towards the gun's left (model units).")
QVR_SLIDER(vr_grapple_front_button_z, "Front Button Up", -10.f, 10.f, 0.1f, "%.1f", ext(), "From the back button, up (model units).")
QVR_TOGGLE(vr_grapple_rope_sim, "Physical Rope",
    "The rope wraps round the world and the props (a pillar's edge, a box's top), never through them, and "
    "pulls along where it lies; slack, it is drawn hanging from the corners it wraps and lying on the "
    "world. Off: a straight line, drawn sagging.")
QVR_SLIDER(vr_grapple_rope_spacing, "Rope Point Spacing", 4.f, 48.f, 1.f, "%.0f", ext(2.f, 256.f),
    "Units between the points of the rope as drawn hanging (the physical rope): finer bends more smoothly "
    "and costs more (128 points at most).")
QVR_SLIDER(vr_grapple_rope_iterations, "Rope Precision", 1.f, 32.f, 1.f, "%.0f", ext(1.f, 64.f),
    "Passes a frame holding the drawn rope's pieces to their length: more is stiffer and truer, and costs "
    "more.")
QVR_SLIDER(vr_grapple_rope_radius, "Rope Thickness", 0.25f, 4.f, 0.25f, "%.2f", ext(0.1f, 8.f),
    "Half the rope's thickness (units): how far its corners keep from the edges it wraps round, and the "
    "drawn rope from what it lies on.")
QVR_SLIDER(vr_grapple_rope_tail, "Rope Straight Out of the Hook", 0.f, 12.f, 0.5f, "%.1f", ext(0.f, 16.f),
    "How far the drawn rope goes straight out of the hook's back before it bends (units, at the hook's full "
    "size): its last pieces point into the hook, not across it. 0: it bends right at the hook.")
QVR_SLIDER(vr_grapple_rope_depth, "Rope Depth in the Gun", 0.f, 8.f, 0.5f, "%.1f", ext(-1.f, 16.f),
    "How far inside the muzzle the rope is drawn from (units), in the hand and with the gun lying about. "
    "-1: the old places.")
QVR_SLIDER(vr_grapple_move_share, "Walking Shared", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much of your walking (the thumbstick) the hook or a hooked prop hanging on the rope takes: it "
    "trails a little behind you instead of lagging far back and snapping forward. Lying on the floor, it "
    "is left alone. 0: none.")
QVR_SLIDER(vr_grapple_load_drag, "Air Drag", 0.f, 5.f, 0.1f, "%.1f", ext(),
    "How fast what swings on the rope slows down (a share of its speed a second, relative to you): a gun "
    "left hanging stops swinging, a flail can't wind up without end.")
QVR_SLIDER(vr_grapple_hang_drag, "Hanging Air Drag", 0.f, 10.f, 0.25f, "%.2f", ext(),
    "The same for what hangs on a rope no one holds (a gun left hanging from a ceiling, what hangs from a "
    "dropped gun): it stops swinging in a couple of seconds.")
QVR_SLIDER(vr_grapple_load_max_speed, "Top Speed", 0.f, 2000.f, 50.f, "%.0f u/s", ext(0.f, 10000.f),
    "The fastest what hangs on the rope moves relative to you (no rope of death). 0: no limit.")
QVR_SLIDER(vr_grapple_loose_slack, "Slack When Detached", 0.f, 128.f, 4.f, "%.0f", ext(0.f, 1000.f),
    "Units of slack the rope gets when the hook comes off: a loose hook lying there is not dragged by "
    "your next step.")
QVR_SLIDER(vr_grapple_reel_speed, "Reel Speed", 100.f, 1000.f, 25.f, "%.0f u/s", ext(25.f, 2000.f),
    "How fast the reel pulls you in: to a wall or a ceiling, to a huge monster, to a prop too heavy to come.")
QVR_SLIDER(vr_grapple_min_length, "Shortest Rope", 16.f, 128.f, 4.f, "%.0f", ext(0.f, 400.f), "How short the reel takes the rope (units).")
QVR_SLIDER(vr_grapple_reel_home, "Loose Hook Goes In At", 0.5f, 16.f, 0.5f, "%.1f", ext(0.5f, 64.f),
    "How close to the muzzle (units) you reel a loose hook's back before it goes into the gun. Smaller: you "
    "reel it in further.")
QVR_SLIDER(vr_grapple_unreel_speed, "Unreel Speed", 50.f, 800.f, 25.f, "%.0f u/s", ext(0.f, 2000.f),
    "The hand's lower button (A on the right hand, X on the left) pays the rope out while held, this fast: "
    "you let yourself down from a ceiling, a monster or a prop can go farther. Let go and the rope keeps its "
    "length. 0: no unreel.")
QVR_SLIDER(vr_grapple_unreel_slack, "Unreel Slack", 4.f, 1000.f, 4.f, "%.0f", ext(4.f, 10000.f),
    "With nothing pulling on the rope (you standing, a hooked box on the floor), the unreel lets out this "
    "much slack (units) beyond the hook's distance: the rope comes off the drum and lies there. Hanging, "
    "what pulls takes the rope out as ever. 4: no slack.")
QVR_TOGGLE(vr_grapple_unreel_airborne, "Unreel Button Only When Airborne",
    "On: a button that jumps (A) unreels only in the air: on the ground it jumps as ever, and a press that "
    "jumped does not unreel until pressed again; X (the reload) unreels on the ground too. Off: either "
    "unreels anywhere, its key still pressed too.")
QVR_SLIDER(vr_grapple_prop_speed, "Prop Reel Speed", 100.f, 1500.f, 25.f, "%.0f u/s", ext(25.f, 3000.f),
    "How fast light props come in: weapons, pickups, keys, gibs. They hang at the gun, to take with the "
    "other hand.")
QVR_SLIDER(vr_grapple_prop_light, "Light Up To", 1.f, 40.f, 1.f, "%.0f kg", ext(0.1f, 200.f),
    "Props up to this mass come at the Prop Reel Speed; heavier ones slower, in proportion (twice as heavy: "
    "half as fast). Masses: Held Object Offsets.")
QVR_SLIDER(vr_grapple_prop_anchor, "Too Heavy From", 10.f, 500.f, 5.f, "%.0f kg", ext(1.f, 10000.f),
    "A prop this heavy does not come: the reel pulls you to it, as to a wall.")
QVR_SLIDER(vr_grapple_tow_light, "Dragging: Light Up To", 1.f, 100.f, 1.f, "%.0f kg", ext(0.1f, 1000.f),
    "Walking away from a hooked prop on a taut rope drags it along: up to this mass, at the Dragging Speed; "
    "heavier, its weight holds you back, in proportion (twice as heavy: half as fast; an explosive box is "
    "40 kg).")
QVR_SLIDER(vr_grapple_tow_speed, "Dragging Speed", 0.f, 600.f, 10.f, "%.0f u/s", ext(0.f, 2000.f),
    "The fastest you walk dragging a light prop on a taut rope. 0: no limit (it comes at your speed).")
QVR_SLIDER(vr_grapple_small_mass, "Small Up To", 20.f, 600.f, 10.f, "%.0f kg", ext(0.f, 10000.f),
    "Monsters up to this mass are small: pulled fast and staggered (dogs 35, grunts 80, knights 90, "
    "enforcers 100).")
QVR_SLIDER(vr_grapple_huge_mass, "Huge From", 100.f, 2000.f, 25.f, "%.0f kg", ext(0.f, 100000.f),
    "Monsters this heavy are huge: the reel pulls you to them (shamblers 600, bosses). In between, medium: "
    "pulled slowly, not staggered (hell knights 150, ogres and fiends 250).")
QVR_SLIDER(vr_grapple_small_speed, "Small Reel Speed", 50.f, 1000.f, 25.f, "%.0f u/s", ext(10.f, 2000.f), nullptr)
QVR_SLIDER(vr_grapple_medium_speed, "Medium Reel Speed", 25.f, 600.f, 25.f, "%.0f u/s", ext(10.f, 2000.f), nullptr)
QVR_TOGGLE(vr_grapple_stagger, "Stagger Small Monsters",
    "A small monster reeled in is staggered: its pain, cutting off its attack, and again every second.")
QVR_SLIDER(vr_grapple_stamina, "Stamina a Second", 0.f, 50.f, 1.f, "%.0f", ext(0.f, 200.f),
    "Reeling a monster in tires you (the parry, shove and strike stamina): this much a second. Short of it, "
    "it comes a quarter as fast. 0: free.")
QVR_SLIDER(vr_grapple_haptics, "Haptics", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "The hook biting, the reel's buzz (stronger with a heavier load), the rope snapping taut.")
QVR_TOGGLE(vr_grapple_sag, "Slack Rope Hangs", "A slack rope is drawn hanging (off: always straight).")
QVR_TOGGLE(vr_hull_edge_probe, "Ledge Test Fix",
    "Quake slows you more when the floor ahead of you drops away (an edge). On: that test looks down from "
    "your box's front edge (half your width ahead), and a point inside a step counts as floor. Off: "
    "Quake's, 16 units ahead whatever your width; stairs with a smooth ramp over them (MG1's start) then "
    "slowed you to a crawl, and a jump against them went nowhere (vr_hull_edge_probe).")
QVR_TOGGLE(vr_hull_brushmodels, "Doors, Lifts and Walls Too",
    "The width above against brush models as well (doors, lifts, trains, func_walls, the firing range's "
    "panels and tables). Off: only the world's walls; brush models meet Quake's box.")
QVR_TOGGLE(vr_hull_monsters, "Monsters", "That width between you and monsters.")
QVR_TOGGLE(vr_hull_players, "Other Players", "That width between players.")
QVR_TOGGLE(vr_hull_boxes, "Solid Boxes", "That width against explosive boxes and other solid boxes.")
QVR_TOGGLE(vr_hull_hit_head, "Height Shots Hit: Your Head's",
    "Shots and missiles meet you only up to the top of your head: crouched behind a crate, those flying "
    "over it miss you (Quake's box stands as tall), and monsters' bullets aim no higher than your body. "
    "Off: Quake's height, crouched or not (vr_hull_hit_head).")
QVR_TOGGLE(vr_crouch_hull, "Crouched Hitbox",
    "Crouch in real life and your box is lower: you walk under low ceilings and through small teleporters, "
    "and behind cover shots meet only what shows of you. Standing up somewhere too low, you keep the low box "
    "until there is room. Off: always Quake's standing height (vr_crouch_hull).")
QVR_SLIDER(vr_crouch_height, "Lowest Crouched Height", 24.f, 52.f, 4.f, "%.0f units", noExt,
    "Your box's height with your eyes under it (Quake's standing box: 56; the crouching room's tunnel in "
    "the teleporter test map: 40) (vr_crouch_height).")
QVR_TOGGLE(vr_crouch_sight, "Monsters See Your Eyes",
    "Crouched (with the crouched hitbox), monsters look for you at your real eyes, not at a standing "
    "player's: crouched behind cover lower than you stand, you are out of their sight, as Arcane "
    "Dimensions' height-aware monsters check (vr_crouch_sight). Standing: Quake's as ever.")
QVR_CYCLE(vr_crouch_step, "Heights Above It",
    "Taller crouched boxes up to 52, so half a crouch gets you under a ceiling that is a little low: your "
    "eyes under one, that box (36, 44, 52 every 8). None: the lowest only (vr_crouch_step).",
    {0.f, "None"}, {4.f, "Every 4 units"}, {8.f, "Every 8 units"}, {12.f, "Every 12 units"})
QVR_SLIDER(vr_box3d_player_radius, "Prop Push Radius", 5.f, 40.f, 1.f, "%.0f cm", noExt,
    "Loose props (rocks, bricks, weapons on the floor) are pushed by a capsule this wide round your body, "
    "not by your box (vr_box3d_player_radius).")
QVR_TOGGLE(vr_box3d_player_stand, "Stand on Boxes",
    "Explosive boxes and other solid props are ground: you stand still on them, walk and jump from them, "
    "and ride them as they move; your body doesn't shove the one under you (vr_box3d_player_stand).")
QVR_SLIDER(vr_box3d_player_slope, "Steepest Face to Stand On", 0.f, 46.f, 1.f, "%.0f deg", noExt,
    "A solid prop's face tilted more than this isn't ground: you slide off it, as off a slope too steep to "
    "walk, and your weight doesn't press it. 46: Quake's own limit (you stood on steeper faces, creeping "
    "down slowly) (vr_box3d_player_slope).")
QVR_SLIDER(vr_box3d_player_mass, "Your Weight on Them", 0.f, 150.f, 5.f, "%.0f kg", noExt,
    "What you press the prop you stand on down with (a floating box sinks lower: at most half its own "
    "weight there). 0: none.")
QVR_SLIDER(vr_box3d_player_jump_push, "Jump Push", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The share of your jump's push the prop you jump from takes (1: all of it, your weight times your "
    "speed). 0: none.")
QVR_SLIDER(vr_box3d_player_shove, "Walking Into Them", 0.f, 2.f, 0.1f, "%.1f", noExt,
    "How hard you shove a solid prop you walk into, as a share of your weight: a light box goes nearly at "
    "your pace (at most 2.5 m/s: vr_box3d_player_push_speed), a heavy one slowly, one too heavy not at all. 0: "
    "they stop you like walls (vr_box3d_player_shove).")
QVR_SLIDER(vr_box3d_monster_push_speed, "Monsters Shove Them At", 0.f, 10.f, 0.5f, "%.1f m/s", noExt,
    "The fastest a monster walking or running into a solid prop pushes it: its steps are jumps, and a crate "
    "kicked at their speed broke and hurt the monster. 0: no limit (vr_box3d_monster_push_speed).")
QVR_TOGGLE(vr_box3d_player_unstick, "Never Trapped by Them",
    "A solid prop that ends up in your body (toppled onto you, pushed into you) never holds you: you walk out "
    "of it, and it is pushed out of you (vr_box3d_player_unstick).")
QVR_TOGGLE(vr_box3d_player_hold, "Never Through Them",
    "Inside a solid prop, you only move out of it, never deeper; with your feet a little into its top (you "
    "landed on it, or it rocked up into you), you are put back on top. A box you threw or let go of is solid "
    "to you too (vr_box3d_player_hold).")
QVR_TOGGLE(vr_box3d_player_shape, "Their Real Shape",
    "You meet a solid prop's shape as drawn, however it is turned, and round: as close to a box's face turned "
    "any way as to a wall; a tilted box is no taller than it looks. Off: the upright box round it "
    "(vr_box3d_player_shape).")
QVR_TOGGLE(vr_box3d_shot_shape, "Shots Meet Their Shape",
    "Rockets, nails, grenades, lasers and every other missile, and the guns' shots, meet a solid prop's "
    "shape as drawn, however it is turned: a tilted box's empty corners let them by, and they hit its real "
    "faces. Nails, rockets and lasers meet a dropped backpack where it is drawn, not the wide pickup box "
    "round it. Off: the upright box round it (the grappling hook still meets its shape) (vr_box3d_shot_shape).")
QVR_TOGGLE(vr_mhull, "Narrower Monsters",
    "Quake moves monsters against walls with a 32-wide box (64 for the big ones), wider than most of their "
    "own boxes in Quake VR (a grunt's is 24, an ogre's 40). On: the widths below instead (a clipping hull "
    "compiled for each when the map loads, as yours is): they walk closer to walls and through narrower "
    "gaps (on by default). You and your shots meet their own boxes and models already.")
QVR_TOGGLE(vr_mhull_ents, "Against Bodies Too",
    "A width narrower than a monster's own box meets you, other monsters and solid boxes too, both ways. "
    "Off: their own boxes between bodies.")
QVR_TOGGLE(vr_mhull_ledges, "Ledges by Their Width",
    "A width narrower than a monster's own box also takes the ledge test (a monster doesn't step where "
    "its corners aren't over ground): it walks as close to a ledge as its width. Off: its box's corners.")

// vr_menu.cpp: menuLevel
QVR_CYCLE(vr_menu_level, "Menu Detail",
    "How much the VR pages show. Standard: what every player sets (comfort, height, the HUD, the headset, "
    "volume). Advanced: every gameplay, display and graphics setting. Developer: also the tuning pages "
    "(weapon and prop offsets and weights, ragdolls, hitboxes), recording, debug and tests.",
    {0.f, "Standard"}, {1.f, "Advanced"}, {2.f, "Developer"})

// vr_menu_ad_monsters.inc: pageAdMonsterTests
QVR_SLIDER_BYNAME(vr_adgun_crossbow_anim_at, "Crossbow Held After a Shot", -1.f, 2.5f, 0.01f, "%.2f s", noExt,
    "Holds the crossbows in your hands this long after a shot, loaded: the string's snap, the limbs' "
    "wobble, the draw and the next bolt's morph, frame by frame. -1: as they are.")
QVR_TOGGLE_BYNAME(vr_adgun_hose_arms, "Fuel Hose Avoids the Arms",
    "The hose kept out of your drawn arms (the upper arm and forearm as capsules), going round them. Off: "
    "it passes through them (to compare; Fuel Hose Info measures how deep).")
QVR_TOGGLE_BYNAME(vr_flail_frame, "Flail Head in Your Frame",
    "Debug: the held flail's head simulated in your moving frame (the stick's walk and turn carry it; Flail "
    "Body Inertia's share of your speeding up swings it). Off: in the world's, as before (the stick's walk "
    "flung it).")
QVR_TOGGLE_BYNAME(vr_flail_smooth, "Flail Head Drawn Smooth",
    "Debug: the held flail's head drawn between the server's steps, on the hand as drawn, at the headset's "
    "frame rate. Off: where each 72 Hz step left it (jerky under the hand).")
QVR_TOGGLE_BYNAME(vr_melee_curved_blades, "Curved Blades",
    "A curved sword (the death fury's) strikes along its curve; off: along the straight line from the hand "
    "to its tip, as before.")

// vr_menu_automation.inc: ?
QVR_TOGGLE(vr_agrunt_ik_debug, "Armed Grunts: Grip Distances",
    "Prints each armed grunt's hands' distances to its weapon's grips as drawn (agik lines; "
    "vr_agrunt_ik).")
QVR_TOGGLE(vr_agrunt_strike_demo, "Armed Humanoids: Strike Demo",
    "Armed humanoids with a melee weapon or their fists and no enemy strike the air ahead of them every "
    "couple of seconds, standing still: watch their wind-ups, blows and follow-throughs from afar (with "
    "notarget; scene agrunt_strikes; vr_agrunt_strike_demo).")
QVR_TOGGLE(vr_agrunt_debug, "Armed Grunts: Log",
    "Prints each armed grunt's weapon, shots, reloads, disarms and pickups (developer 1; vr_agrunt_debug).")
QVR_TOGGLE(vr_agrunt_test_kind, "Armed Grunts: Tests Spawn Enforcers",
    "The Armed Grunts tests above spawn armed enforcers (monster_vr_enforcer, the armed monsters' second "
    "row) instead of armed grunts (vr_agrunt_test_kind 1; 0 grunts).")
QVR_TOGGLE(vr_grapple_test_stuck, "Load Stuck",
    "What hangs on the rope (a loose hook, a hooked prop) stays where it is, as if snagged: walk away and the "
    "rope should hold you back.")
QVR_TOGGLE(vr_smallgibs_test_dummy, "On the Training Dummy",
    "vr_smallgibs_test_dummy: the tests below hit the nearest training dummy instead (the firing range's), and the shotgun, nail, blow and chainsaw ones print the gore each sent (gore hits, wound events, blood particles): to compare with a grunt's.")
QVR_TOGGLE(vr_smallgibs_trace, "Trace Small Gibs",
    "vr_smallgibs_trace: each small gib's first 2.5 s in the console (sgibtrace:): where made, its speed and every jump in it, what touches, nudges or strikes it, how far it lay.")
QVR_SLIDER(vr_gore_test_crowd, "Crowd Radius", 0.f, 1024.f, 64.f, "%.0f units", noExt,
    "Above 0, the limb and head tests act on every monster this near you at once, in one frame, quietly (one "
    "goretest: crowd line): Crowd: ... below, or any test above. 0: the nearest monster only (vr_gore_test_crowd).")
QVR_SLIDER(vr_decap_poptest_dist, "Test Range", 0.5f, 25.f, 0.5f, "%.1f lengths", noExt,
    "The tests below shoot (or throw) from this many player lengths (56 units) off the nearest live monster's head "
    "(you are moved there and back; round it till the way is clear) (vr_decap_poptest_dist).")
QVR_TOGGLE(vr_walltorch_debug, "Torch Flames to Console",
    "vr_walltorch_debug: each lit torch you hold or that lies about, twice a second (wtflame: how far upside "
    "down, its flame's foot, its speed and flattening, what of you its flame touches), and the torches' smoke "
    "(wtsmoke). 2: every frame.")
QVR_TOGGLE(vr_xr_test_drop_context, "Test: No GL Context at the Next VR Start",
    "vr_xr_test_drop_context 1: the next eye swapchain is requested with no GL context current (once): the "
    "game makes its context current again first and logs it in qvr_openxr.txt. Then vr_restart.")

// vr_menu_cheats.inc: pageDebugCheats
QVR_TOGGLE(vr_cheat_ammo, "Infinite Ammo",
    "vr_cheat_ammo: your ammunition kept full, and the magazine of the weapon in either hand (not an enemy's "
    "gun's, nor the chainsaw's fuel). Not saved.")
QVR_TOGGLE(vr_freeze_monsters, "Freeze Monsters",
    "vr_freeze_monsters: the living monsters stand frozen mid-step, mid-swing: no thinking, no moving (their "
    "missiles fly on). A blow still hurts them, and kills them. Off: each goes on where it stopped. Not saved.")
QVR_TOGGLE_BYNAME(sv_freezenonclients, "Pause the World",
    "sv_freezenonclients: everything but you stands still: monsters, missiles, gibs, physics, the game's "
    "clock. You move (noclip to place yourself), the world waits. Single player. Not saved.")
QVR_CYCLE(vr_shot_hide, "Hide",
    "vr_shot_hide: not drawn, for a clean shot: the wrist gadget (its screen and messages), your hands and "
    "what they hold, your body and holsters. They work on unseen. Not saved.",
    {0.f, "Nothing"}, {1.f, "Gadget"}, {2.f, "Hands"}, {3.f, "Gadget and Hands"}, {4.f, "Body"},
    {5.f, "Gadget and Body"}, {6.f, "Hands and Body"}, {7.f, "Gadget, Hands, Body"})

// vr_menu_checklist.inc: pageChecklist
QVR_TOGGLE(vr_checklist_hide_ticked, "Hide Ticked",
    "Leaves the passed items out of the list. Off: every item, the passed ones dimmed.")

// vr_menu_debug.inc: pageDebug
QVR_CYCLE(vr_menu_body_clone, "Body Clone While Tweaking",
    "While a page tweaks something worn on your body (the holsters, the pouches, the flashlight on your belt "
    "or head and its zones, the wrist gadget's mount, the virtual stock), a mirror image of your body beside "
    "the menu, facing you, with the zones drawn on it too. Every VR Page: on any page (a test). "
    "vr_menu_body_clone_info prints where it is.",
    {0.f, "Off"}, {1.f, "On"}, {2.f, "Every VR Page"})
QVR_TOGGLE(vr_anim_surface, "Animated Surfaces",
    "On: an animated texture's frames (the wall buttons' lit and dim frames) share one surface: the same "
    "bumps, parallax depth, sheen and detail, only their colours and glow change. Off: each frame its own "
    "(the relief and sheen pulsed with the frames). At once.")
QVR_TOGGLE(vr_debug_damage_numbers, "Show Damage Numbers",
    "Every hit on anything that takes damage (monsters, corpses, gibs, crates, props, shootable walls): its "
    "damage floating where it struck, and printed as the training dummy's (what struck, where, what's left).")
QVR_TOGGLE(vr_debug_rope, "Show Grapple Rope",
    "Each grappling hook rope: its drawn chain (points white, pieces green), the corners it wraps round "
    "(red) and its taut path (yellow), the ends (orange).")
QVR_TOGGLE(vr_debug_physics_shapes, "Show Physics Shapes",
    "Draws the physics bodies (Box3D) as wireframes: props awake green, asleep blue, held yellow; doors purple, "
    "monsters orange, you cyan, hanging pickups grey; red dots where they touch. And each hand's grab reach.")
QVR_CYCLE(vr_debug_hitzones, "Show Hit Zones",
    "Positional Damage colors the animated model surface: head red, body green, extremities yellow, legs blue. "
    "Uses the same standing-pose mapping and Head Priority as precise shots and positional melee. "
    "Precise hits off: box/ray reference zones. Decapitation colors, on the animated model too, where a "
    "slash beheads (magenta: the melee's zone, Head Size and Neck) and the head that shots, lasers and "
    "thrown axes pop (red); Both outlines the magenta over Positional Damage's colors. Every head and limb "
    "test maps the point struck to the standing pose, so these zones move with the model.",
    {0.f, "Off"}, {1.f, "Positional Damage"}, {2.f, "Decapitation"}, {3.f, "Both"})
QVR_CYCLE(vr_debug_disarm, "Show Disarm Volumes",
    "Armed enemies, standing or an active ragdoll: their weapon's blow/parry reach (orange; yellow: your "
    "grip would take it now; green: in a tug, its progress bar), shot volumes (magenta weapon, cyan hand), the "
    "grip spheres a body blow disarms within (red; faint: a limb hit's), the ragdoll limb it rides (white). Your "
    "fists' grab spheres (grey; yellow touching a weapon, the gap shown), your strike lines (white) and blow "
    "lines (blue). The last hit is marked a moment: parry cyan, disarm red, body grey, shot magenta, blocked "
    "yellow.",
    {0.f, "Off"}, {1.f, "On"}, {2.f, "And Print Hits"})
QVR_TOGGLE(vr_debug_hitzones_xray, "Hit Zones Through Walls",
    "Draws the animated positional regions through walls and the back of the model. Off: only visible surfaces.")
QVR_CYCLE(vr_debug_hits, "Show Hits",
    "Precise hit detection: each hit on a monster's model drawn for a few seconds (the model as it was then, "
    "the triangle hit in green, the point on the model in red, where the grown model was met in yellow) and "
    "printed. Hits and Misses: also the shots that went through a monster's box beside its model (orange).",
    {0.f, "Off"}, {1.f, "Hits"}, {2.f, "Hits and Misses"})
QVR_TOGGLE(vr_debug_melee_blades, "Show Melee Blades",
    "vr_debug_melee_blades: what each hand's blows strike with, live (swords, axes, fists, guns used as clubs, "
    "torches): the blow line from the hand to the tip (bold; the near end behind the hand thinner), white "
    "idle, yellow while the hand moves fast enough to strike, red just after a blow landed; the lines the "
    "sweep tested and the blade's swept surface over the last third of a second, fading; a hammer's or "
    "mace's head as its box. A red dot where a blow landed, its damage above it.")
QVR_TOGGLE(vr_agrunt_aim_exposed_show, "Show Armed Humanoids' Aim Points",
    "vr_agrunt_aim_exposed_show: where each armed humanoid aims on you, each time it looks (every 0.1 s): "
    "yellow sparks on the part of you its gun's line reaches, red where it last aimed when nothing of you "
    "shows (it holds fire). The setting: Armed Grunts > AI Behaviour > Aim at What They Can Hit.")
QVR_CYCLE(vr_agrunt_shotdisarm_debug, "Show Shot Disarms",
    "vr_agrunt_shotdisarm_debug: each shot of yours on an armed grunt printed (shotdisarm: the part it hit, "
    "weapon, hand or body; the damage adding up; each roll, its chance and result); Print "
    "and Volumes also shows each held weapon's line (blue-white sparks) and its hand and forearm (sparks). "
    "The settings: Armed Grunts > Disarming: Shooting It Out.",
    {0.f, "Off"}, {1.f, "Print"}, {2.f, "Print and Volumes"})
QVR_CYCLE(vr_agarmor, "Armed Humanoids' Armour (P0)",
    "vr_agarmor (debug, the armour spike: drawn only, no gameplay): armour shells made from the grunt's own mesh "
    "(a cuirass and a helmet) on every grunt and enforcer drawn on the authored rig (the Rig Studio's "
    "subject too), in the player's armour colours. One of Each: green, "
    "yellow, red by entity number. vr_agarmor_set <entity> <type> [pieces] for one.",
    {0.f, "None"}, {1.f, "Green"}, {2.f, "Yellow"}, {3.f, "Red"}, {4.f, "One of Each"})
QVR_CYCLE(vr_agarmor_pieces, "Armour Pieces",
    "vr_agarmor_pieces: which pieces the armoured grunts wear (each piece hidden by collapsing its bones).",
    {3.f, "Cuirass and Helmet"}, {1.f, "Cuirass"}, {2.f, "Helmet"})
QVR_TOGGLE(vr_debug_hand_bones, "Show Hand Bones",
    "Draws both hands' joints and bones, and what they grip: the finger spheres (green touching, yellow near, red "
    "sunk in) and the palm's fit (white: where the hand is, cyan: where the grip moved the palm).")
QVR_CYCLE(vr_debug_ledges, "Show Ledges",
    "The ledge map round you, what climbing holds: lips green (on moving brushes pink; on props orange, as they "
    "lie now: Climb Props), a tick out every 8 units (yellow where the drop starts further out). Built even with "
    "climbing off.",
    {0.f, "Off"}, {256.f, "Within 256 units"}, {1.f, "Within 512 units"}, {1024.f, "Within 1024 units"})
QVR_TOGGLE(vr_debug_hand_collide, "Log Empty Hand Against Held",
    "Prints, each frame an empty hand meets the weapon or the thing the other hand holds, how near the real "
    "and the drawn hand's palm and knuckles are to its surface, how far the drawn hand and the thing are "
    "held back (Carrying: Empty Hand Against Held Things).")
QVR_CYCLE(vr_debug_carry, "Show Grab Test",
    "For each hand near something to carry: the box it is drawn in and the fist tested (its spheres; the nearest "
    "bright). Logged: each grab and held prop's placing printed, carry_trace.txt written, and held props meeting each "
    "other, a weapon or a wall; also far fists: hands not near anything too.",
    {0.f, "Off"}, {1.f, "Drawn"}, {2.f, "Drawn and Logged"}, {3.f, "Also Far Fists"})
QVR_TOGGLE(vr_debug_grab, "Log Grab Query",
    "Prints each grip the grab query gives a take in the player's frame (grabquery: main hand, torch (the torch, "
    "3.2 u)): a torch on its wall, the grenade pouch, the ammo pouch (vr_grab.hpp: the kinds in their order).")
QVR_CYCLE(vr_debug_climb_feet, "Climbing Footholds Debug",
    "Climbing, prints each foothold a foot finds (climbfoot: where, what, how many traces); each frame's feet "
    "(climbfeet: planted or stepping, their slip, any in a wall); drawn: each foot's hold and the search.",
    {0.f, "Off"}, {1.f, "Log Holds"}, {2.f, "Log Each Frame"}, {3.f, "Logged and Drawn"})
QVR_CYCLE(vr_bodytrack_debug, "Show Tracked Skeleton",
    "Body tracking: the headset's tracked skeleton drawn (cyan: seen, magenta: guessed by the headset, as "
    "Quest 3's legs), on your body or beside it.",
    {0.f, "Off"}, {1.f, "On the Body"}, {2.f, "Beside the Body"})
QVR_CYCLE(vr_bodytrack_debug_bones, "Skeleton Bones",
    "Which bones Show Tracked Skeleton draws: the headset's tracked ones (cyan, magenta); our IK's, the body "
    "posed without the tracking (orange: while they are drawn, the body ignores the tracking); or both "
    "over each other, to compare. Body Over Tracked: the body's own bones as posed with the tracking "
    "(yellow) over the tracked ones: the yellow elbow should sit on the magenta one.",
    {0.f, "Tracked"}, {1.f, "Untracked (Our IK)"}, {2.f, "Both Overlapped"}, {3.f, "Body Over Tracked"})
QVR_CYCLE(vr_body_debug, "Show Body Skeleton",
    "Draws the body's skeleton; or shows the body in front of you, facing you or seen from its left (to check "
    "its pose and calibration without a mirror).",
    {0.f, "Off"}, {1.f, "Skeleton"}, {2.f, "Body Facing You"}, {3.f, "Body From Its Left"})
QVR_TOGGLE(vr_debug_body_error, "Log Body Drift",
    "Twice a second in the console: how far the game's body (its feet) has drifted in your room since it was turned "
    "on (or marked), and how far its facing has turned, with your lean. Stand still in the room: leaning and "
    "turning or moving with the stick, both should stay near 0 (vr_body_error).")
QVR_TOGGLE(vr_debug_body_heights, "Log Body Heights",
    "Twice a second in the console: metres above the body's floor of your eyes (as calibrated and as learnt "
    "standing), the pelvis, the knees and ankles (and how straight the legs are, 1 straight), the ammo pouch "
    "and the hip holsters (and how far from the pelvis), the grenade pouch and the chest holsters. Standing "
    "still, they should stay put; a leg at 1 with its ankle off the floor is too short.")
QVR_CYCLE(vr_debug_body_collide, "Show Body Collisions",
    "The drawn hands and weapons stopping at each other and the body: each contact printed (and "
    "body_collide_trace.txt); drawn: the capsules and the pushes.",
    {0.f, "Off"}, {1.f, "Logged"}, {2.f, "Logged and Drawn"})
QVR_CYCLE(vr_debug_hand_offset, "Log Hand Offsets",
    "Each hand drawn away from where it is tracked, and by what (walls, the weight's spring, models, the body, "
    "a held thing against a wall, the other hand's weapon or held thing), in the console.",
    {0.f, "Off"}, {1.f, "When Moved"}, {2.f, "Every Frame"})
QVR_TOGGLE(vr_magholster_chain_log, "Magnetic Holster Chain Log",
    "vr_magholster_chain_log: each frame, in the console, every magnetic holster chain's worst link stretch (a share), "
    "and its most movement and jerk off its anchor (mm); a jump that starts it afresh.")
QVR_CYCLE(vr_debug_model_collide, "Show Model Collisions",
    "Held weapons and props stopping at the models' triangles: each hand's push printed; drawn: the rays (grey as tracked, "
    "green or red as drawn) and the push (yellow).",
    {0.f, "Off"}, {1.f, "Logged"}, {2.f, "Logged and Drawn"})
QVR_TOGGLE(vr_foveated_debug, "Show Foveation",
    "The shading rates of Foveated Rendering in the eyes and the mirror (yellow 2x2, red 4x4) and the upscaler's "
    "sharp circle (cyan).")
QVR_CYCLE(vr_temporal_debug, "Show Motion Vectors",
    "vr_temporal_debug (TAA's inputs, vr_temporal.cpp): Vectors: red/green the motion across/up the eye, blue "
    "a model's own; Reactive Mask: red translucency and particles, green history thrown away, blue liquids; "
    "Reprojection Test: the last frame moved by the vectors (it should hold still as you move; red: thrown away).",
    {0.f, "Off"}, {1.f, "Vectors"}, {2.f, "Reactive Mask"}, {3.f, "Reprojection Test"})
QVR_TOGGLE_BYNAME(vr_parallax_debug, "Show Parallax Depth",
    "vr_parallax_debug: where the walls' parallax writes the depth of the carving it shows (Graphics > Surfaces > "
    "Parallax Depth Write): red, brighter the deeper below the surface; green, brighter the higher over it "
    "(Parallax Surface Level); dark blue: the surface's own depth.")
QVR_TOGGLE_BYNAME(r_showbboxes, "Show Entity Boxes",
    "Every entity's bounding box, as the game collides with it (monsters, items, missiles, triggers), through "
    "walls. Single player only.")
QVR_CYCLE_BYNAME(developer, "Developer Messages",
    "The game's developer messages: needed by Shots and Damage and the Grappling Hook's log below, and many "
    "others (melee events, grenades, deflections). Verbose: every frame's melee detail too.",
    {0.f, "Off"}, {1.f, "On"}, {2.f, "Verbose"})
QVR_TOGGLE(vr_gore_sound_debug, "Gore Sounds",
    "vr_gore_sound_debug: each gore sound played or dropped (capped, rate-limited, too far, off) and why, "
    "and a choking monster's cry dropped (Gore > Gore Sounds; vr_gore_sound_stats counts them).")
QVR_TOGGLE(vr_bloodprints_debug, "Bloody Prints",
    "vr_bloodprints_debug: each bloody footprint and handprint (where, its way, size, strength; the ankle and "
    "toe or the wrist it came from), each pool or body that bloodied your feet.")
QVR_CYCLE(vr_debug_bullettime, "Bullet Time",
    "Bullet time starting, stopping and refused (the meter, the cooldown), each tap and taps that never "
    "stopped on the screen; And the Screen Tap: every frame near the gadget's screen, how far over it your "
    "hand (and its gun's butt) is and how fast it comes into it.",
    {0.f, "Off"}, {1.f, "On, Off, Refused, Refills"}, {2.f, "And the Screen Tap"})
QVR_CYCLE(vr_flashlight_flick_debug, "Flashlight Flicks",
    "vr_flashlight_flick_debug: the wrist flick that turns the held torch over, taken or refused (and why); 2: the held hand's wrist speed every frame.",
    {0.f, "Off"}, {1.f, "Each Flick and Why"}, {2.f, "And the Wrist Every Frame"})
QVR_CYCLE(vr_debug_chainsaw, "Chainsaw",
    "The chainsaws' cords (taken, pulled, too slow, let go), their engines (started, stalled) and cuts; "
    "And the Bar: also each cut's test against what is near, how deep the drawn bar sinks into a monster, "
    "and the cord's hole drawn. The engine and cut lines need Developer Messages.",
    {0.f, "Off"}, {1.f, "Pulls and Cuts"}, {2.f, "And the Bar in Monsters"})
QVR_CYCLE(vr_debug_flame_wobble, "Flame Wobble",
    "Flames stirred by a fast hand (Graphics > Fire Model Particles > Flames in the Air): each push (the "
    "flame, the hand, how near and fast); And Every Frame's Lean: each swinging flame's lean every frame.",
    {0.f, "Off"}, {1.f, "Each Push"}, {2.f, "And Every Frame's Lean"})
QVR_CYCLE(vr_debug_wallbuttons, "Wall Buttons",
    "Each button pressed: what pressed it and how (a hand, a held weapon and its line, a thrown thing and "
    "its speed, stepped on), and where the hand was: how far from the button and from you (a far one is a "
    "stray press). And Weapon Lines: every frame, each held weapon's line that presses buttons "
    "(from its pommel or butt to its tip or muzzle). Needs Developer Messages.",
    {0.f, "Off"}, {1.f, "Each Press"}, {2.f, "And Weapon Lines"})
QVR_CYCLE(vr_debug_pose_check, "Monster Poses",
    "vr_debug_pose_check: a monster drawn squashed between two poses for over 0.2 s (\"posecheck:\"; a "
    "parried ogre was, for half a second), drawn with a pose out of its model's range or a broken blend; "
    "And a Screenshot: a picture at each; Every Frame: each monster's drawn poses and how flat (a lot).",
    {0.f, "Off"}, {1.f, "Squashed Bodies"}, {3.f, "And a Screenshot"}, {2.f, "Every Frame"})
QVR_TOGGLE(vr_debug_shots, "Shots and Damage",
    "Each hitscan shot (where it starts, its direction, what its pellets hit, headshots), each damage you deal "
    "(and when Quad's sound plays) and each prop a shot pushes. Needs Developer Messages.")
QVR_TOGGLE(vr_debug_missiles, "Missile Hits",
    "Each missile's hit (rockets, nails, grenades, lasers...): what it met, where and the surface's normal; "
    "a solid prop: its drawn box as it stands (vr_debug_missiles).")
QVR_CYCLE(vr_debug_throw, "Throws",
    "Each throw's speed estimate from the hand's motion (and the release's timing); a thrown weapon's spin "
    "and how far its axis is off end over end (0: a wrist flick's, tip going down; needs Developer Messages).",
    {0.f, "Off"}, {1.f, "Each Throw"}, {2.f, "And Its Timing"})
QVR_TOGGLE(vr_debug_angvel, "Controller Spin",
    "While a controller turns fast: which frame its angular velocity is read in (Controller Spin Frame), "
    "how well the runtime's and the fixed one follow the turn, the hand's speed before and after.")
QVR_TOGGLE(vr_debug_highlights, "Highlights",
    "Each moment the highlight log takes (Graphics > Recording > Log Highlights): its time from the sync "
    "mark, kind, score and what it was done to and with.")
QVR_TOGGLE(vr_debug_axestick, "Axe Sticks",
    "Each thrown axe's blade striking something: stuck (how fast, how deep, at what angles) or why it bounced; "
    "its bleeding, its fall, its pull. Needs Developer Messages for the last.")
QVR_CYCLE(vr_climb_debug, "Climbing",
    "Holds taken, released, mantles; every frame: the body, the hands, the pull, the holds' reach (a lot).",
    {0.f, "Off"}, {1.f, "Holds"}, {2.f, "Every Frame"}, {3.f, "And Shoulders"})
QVR_CYCLE(vr_debug_hands, "Hands",
    "Each hand's state: the grip, its weapon, what it carries, the force grab, the flashlight, the hotspot, and "
    "what climbing makes of a grip (holding, free, or why it takes nothing).",
    {0.f, "Off"}, {1.f, "When They Change"}, {2.f, "Every Frame"})
QVR_TOGGLE(vr_swim_debug, "Swim Strokes",
    "Each stroke: its peak speed, the power gate, the reverse damping and the push it gave.")
QVR_CYCLE(vr_grapple_debug, "Grappling Hook",
    "What the hook bites and each reel (mass, class, speeds); the rope 4 times a second, or every frame; and "
    "each rope drawn every frame (which beam, from where to where: a rope drawn where there is none). Needs "
    "Developer Messages.",
    {0.f, "Off"}, {1.f, "Bites and Reels"}, {2.f, "And the Rope"}, {3.f, "Every Frame"}, {4.f, "And Every Rope Drawn"})
QVR_TOGGLE(vr_debug_buttons, "Controller Buttons",
    "Each controller button pressed and let go: the hand, the button, its key and binding, and what took it "
    "(the posing mode, a voice note, the flashlight); and the controller profile the headset's runtime "
    "picked for each hand (Virtual Desktop can report Index controllers). For a button that does nothing.")
QVR_TOGGLE(vr_foreign_configs, "Other Ports' Startup Configs",
    "vr_foreign_configs: at the start, an autoexec.cfg or config.cfg found only outside Quake VR's own "
    "folder (in id1 next to a GOG or Steam Quake, a re-release folder: another source port's) runs too. Off "
    "(the default): only quakevr's own; a skipped one is named in the console. Exec by hand runs any.")
QVR_TOGGLE(vr_wounds_debug, "Wounds", "Each wound painted on a model.")
QVR_CYCLE(vr_debug_grasp, "Grasp",
    "Each grasp solve of the jointed hands (and each finger's stops).",
    {0.f, "Off"}, {1.f, "Each Solve"}, {2.f, "Each Finger"})
QVR_CYCLE(vr_debug_collect_fx, "Put-Away Transition",
    "Each thing put away at a holster or pouch (its model, from where, the holster) and when it has gone "
    "in; or also each frame's size and distance left.",
    {0.f, "Off"}, {1.f, "Each Thing"}, {2.f, "Each Frame"})
QVR_CYCLE(vr_debug_physsound, "Physics Sounds",
    "Each knock (the prop, its material and weight, the hit's speed, the volume), each scrape starting and "
    "stopping and each climbing grab (the hold's texture); or also the hits too soon after the last and each "
    "scrape grain.",
    {0.f, "Off"}, {1.f, "Each Sound"}, {2.f, "And Each Hit Skipped"})
QVR_CYCLE(vr_debug_snd, "Spatial Audio",
    "Each second: the voices in use, their mix's time and the simulations' (the walls, the room's reverb); or "
    "also each voice: its sound, distance, occlusion (low, middle, high) and Doppler.",
    {0.f, "Off"}, {1.f, "Each Second"}, {2.f, "And Each Voice"})
QVR_CYCLE(vr_debug_net, "Network Messages",
    "vr_debug_net: each frame the unreliable broadcast (particles, sounds, temp entities, wounds, tracers) "
    "was full: how much QuakeC's writes lost and how much a client was not sent (whole messages only); or "
    "also each second the most sent: the broadcast, the reliable messages, a client's own data and entities.",
    {0.f, "Off"}, {1.f, "When the Broadcast Is Full"}, {2.f, "And Each Second"})
QVR_TOGGLE(vr_debug_ragdoll, "Ragdolls",
    "vr_debug_ragdoll: each ragdoll made (when it went limp, its parts), each push on it and each blast's throw.")
QVR_CYCLE(vr_debug_ragdoll_look, "Ragdolls' Look Check",
    "vr_debug_ragdoll_look: each ragdoll drawn in its model's rest pose where its entity stands, as its "
    "skinned copy or as its own .mdl (the live monster's look): the two should look the same. "
    "vr_ragdoll_draw_info in the console: the renderer's data for each that differ.",
    {0.f, "Off"}, {1.f, "Skinned Copy at Rest"}, {2.f, "Its .mdl at Rest"}, {3.f, "Hidden"})
QVR_TOGGLE(vr_debug_ragdoll_gun, "Ragdolls' Weapons in Hand",
    "vr_debug_ragdoll_gun: every 0.25 s, each ragdoll's held weapon (an enforcer's rifle, a grunt's shotgun, "
    "a knight's sword, an ogre's chainsaw) as drawn: how far from the part holding it, beside the rest "
    "pose's (\"gun:\"; the same: in his hand).")
QVR_TOGGLE(vr_debug_deadanim, "Dead Monsters Animating",
    "vr_debug_deadanim: each dead monster that animates again printed (its think its AI's stand, walk, run or "
    "attack again, or its frame changing 3 s after its death: a corpse that gets up or runs in place). Needs "
    "Developer Messages.")
QVR_CYCLE(vr_hit_react_debug, "Hit Reaction Log",
    "vr_hit_react_debug: each blow a living monster flinches at printed (the bone struck, its size) and the "
    "server's slow-down of its steps; Every Frame: its bones' turns each frame too (hittrace lines).",
    {0.f, "Off"}, {1.f, "Blows"}, {2.f, "Every Frame"})
QVR_TOGGLE(vr_ragdoll_active_debug, "Active Ragdoll Targets",
    "vr_ragdoll_active_debug: each active ragdoll's joint targets drawn over it (green as strong as the joint "
    "is, grey limp), its pelvis pin (yellow) and its hands' and feet's goals (red: a wound held, a step, a "
    "crawl), what is round him: his probes (cyan, orange where they met something), his hands' goals and "
    "holds (magenta), the wall he leans on (blue). vr_ragdoll_active_list in the console: each one's "
    "strengths and what its death does.")
QVR_CYCLE(vr_debug_box3d, "Physics Bodies",
    "Box3D bodies made, woken and put to sleep; or every awake body every frame (a lot). Also each throw: how "
    "fast, whether it passes through your hands, and how much its velocity and spin changed by the grace's end "
    "(with Throw Grace 0: watched 0.2 s, to compare).",
    {0.f, "Off"}, {1.f, "Made and Slept"}, {2.f, "Every Awake Body"})
QVR_CYCLE(vr_debug_debris, "Rocks and Bricks Placement",
    "At the next map load: the pieces placed, the spots, the time; each piece; each spot rejected and why.",
    {0.f, "Off"}, {1.f, "A Line a Map"}, {2.f, "Each Piece"}, {3.f, "Each Spot Rejected"})
QVR_CYCLE(vr_debug_crates, "Crates Placement",
    "vr_debug_crates: where the crates went (and why not), each crate's clearance in front of it. Also Developer "
    "Messages print each crate's damage and breaking, the pieces and what it held.",
    {0.f, "Off"}, {1.f, "A Line a Map"}, {2.f, "Each Crate"}, {3.f, "Each Spot Rejected"})
QVR_CYCLE(vr_debug_arm, "Arm IK",
    "Each drawn arm's joints once (shoulder, elbow, wrist in the body's axes, the elbow's swing, the wrist's bend "
    "and twist, and the swing's cost every 15 degrees: armcost); or arm_trace.txt every frame.",
    {0.f, "Off"}, {1.f, "Print Once"}, {2.f, "Trace File"})
QVR_CYCLE(vr_debug_2h_grip, "Two-Handed Grip",
    "Each grip the helping hand takes (a sword's or the crowbar's: below the hand or along the blade) and why "
    "it let go of a weapon (the check, its number and limit), the stickiness then (Aiming: 2H Grip "
    "Stickiness), and weapons held anywhere (taken, let go, carried); And Where: also where the grip is "
    "from it, how far along a blade it holds it, and where an empty hand is on a weapon held, carried or "
    "lying about (its surface, handle and nearest hotspot: Weapons Anywhere).",
    {0.f, "Off"}, {1.f, "Taken and Let Go"}, {2.f, "And Where"})
QVR_TOGGLE(vr_verbosebots, "Bot Chatter", "The bots' thoughts, with bots in the game.")
QVR_TOGGLE(vr_debug_glstate, "Graphics State",
    "Each texture bind the engine skipped, believing the texture already bound, while the graphics card had "
    "another there (\"texcache: stale bind\"): a texture deleted without telling the engine's cache, its "
    "name given to a new one; shows as a blank or black image after a size change. And a clip rectangle "
    "left on at a frame's end (\"glstate:\"): the menus and console cut off in the window. And the "
    "framebuffers bound, against the engine's record of them (\"glstate: ... framebuffers\"), at a frame's "
    "end and at each eye's image acquire (vr_debug_glstate).")
QVR_CYCLE(vr_debug_grasp_trace, "Grasp Trace", "grasp_trace.txt: the drawn hand's joints every frame.",
    {0.f, "Off"}, {1.f, "Main Hand"}, {2.f, "Off Hand"}, {3.f, "Both Hands"})
QVR_CYCLE(vr_debug_weight, "Weight Trace",
    "weight_trace.txt: each hand's target and drawn pose under the weight, every frame.",
    {0.f, "Off"}, {1.f, "File"}, {2.f, "File and Console"})
QVR_TOGGLE(vr_debug_lean, "Lean Trace",
    "lean_trace.txt: the head, the body's box, the lean, the pelvis and the feet every frame, and the lean's cues.")
QVR_CYCLE(vr_profile_overlay, "Profiler Panel",
    "Where each frame's time goes: the game's systems (Box3D, QuakeC, the world's drawing, waiting for the "
    "headset...), the last second's average and worst in ms, with a bar against the frame's budget; the CPU's, "
    "then the GPU's. Showing it runs the profiler.",
    {0.f, "Off"}, {1.f, "Over the Wrist"}, {2.f, "In Front"})
QVR_CYCLE(vr_profile_csv, "CSV Capture",
    "While on, a row a second (each system's time, a column each) into quakevr/profile/systems_<date>_<time>.csv, "
    "for a spreadsheet: turn it on, play what feels slow, turn it off.",
    {0.f, "Off"}, {1.f, "Recording"})
QVR_CYCLE(vr_profile_hitch, "Hitch Log",
    "While the profiler runs (the panel, a capture), frames that take this long go to the console and "
    "quakevr/profile/hitches_<date>_<time>.csv, with what took their time.",
    {0.f, "Off"}, {1.5f, "Over 1.5 Frames"}, {2.f, "Over 2 Frames"}, {3.f, "Over 3 Frames"})
QVR_CYCLE(vr_renderdoc_delay, "Capture Delay",
    "vr_renderdoc_delay: seconds from the press to the capture, to close the menu or get into position.",
    {0.f, "None"}, {1.f, "1 s"}, {3.f, "3 s"})
QVR_CYCLE(vr_renderdoc_frames, "Capture Frames",
    "vr_renderdoc_frames: frames in the button's capture (one file).",
    {1.f, "1 Frame"}, {2.f, "2 Frames"}, {5.f, "5 Frames"})
QVR_TOGGLE(vr_renderdoc_hide_menu, "Hide Menu While Capturing",
    "vr_renderdoc_hide_menu: the menu is closed (and the toolgun panel hidden) for the captured frame, the menu "
    "reopened on the same page after.")
QVR_TOGGLE(vr_debug_runtime_menu, "Act as if the Runtime's Menu Were Open",
    "vr_debug_runtime_menu 1: the game acts as if SteamVR's dashboard (or Virtual Desktop's, Meta's menu) had "
    "the focus, on any backend, the mock's too: a single player game pauses and its sounds fade "
    "(Runtime Menu Pauses); the console says when it paused and resumed, and the game's time.")
QVR_TOGGLE(vr_debug_window_unfocused, "Act as if the Window Lost Focus",
    "vr_debug_window_unfocused 1: the game acts as if its desktop window had lost the focus to another "
    "program's: the menus' warning in the top right corner; in VR the mirror presented only "
    "vr_mirror_unfocused_hz times a second (the headset's frames never slowed); on a flat screen the "
    "background sleep.")
QVR_CYCLE_BYNAME(vr_debug_mirror_vsync, "Act as if the Driver Forced V-Sync",
    "vr_debug_mirror_vsync 60 (any Hz in the console): in VR the desktop window's presents wait as on a "
    "monitor of that refresh with the GPU driver forcing V-Sync (or G-SYNC on the focused window; GitHub "
    "issue #79). With Recording > Window Frame Rate at Every Headset Frame the headset then drops to the "
    "monitor's rate; at Up to the Monitor's (the default) it never slows. vr_debug_clock says the frame "
    "period, the presents and the swap interval the driver reports.",
    {0.f, "Off"}, {60.f, "60 Hz Monitor"}, {75.f, "75 Hz Monitor"})
QVR_TOGGLE(vr_debug_window_hidden, "Act as if the Window Were Minimized",
    "vr_debug_window_hidden 1: the game acts as if its desktop window were minimized: the Spectator "
    "Camera is skipped (Recording > Skip While the Window Is Minimized; the console says so with "
    "developer 1). vr_debug_window minimize, hide or show does it to the real window.")
QVR_CYCLE(vr_debug_frame_stall_ms, "Simulate Held Frames",
    "vr_debug_frame_stall_ms: each frame's end waits this long, as if the runtime held the frames (the "
    "100 ms frames of a VR drop): checks the held-frames lines in quakevr/qvr_openxr.txt (the foreground "
    "program, the session's state, the frame times), written once when it starts and once when it ends. "
    "Turn it off at once in the headset: the view stutters.",
    {0.f, "Off"}, {40.f, "40 ms a Frame"}, {100.f, "100 ms a Frame"})
QVR_CYCLE(vr_profile_detail, "Detail",
    "Every Trace and Builtin also times each collision trace and each QuakeC builtin call apart: dearer, to "
    "split QuakeC's time.",
    {1.f, "Systems"}, {2.f, "Every Trace and Builtin"})
QVR_CYCLE(vr_profile_gpu, "GPU Timing",
    "How often the profiler times the GPU: each timed frame stalls it a little. Off: CPU times only.",
    {0.f, "Off"}, {1.f, "Every Frame"}, {2.f, "One Frame in 2"}, {4.f, "One Frame in 4"}, {8.f, "One Frame in 8"})
QVR_SLIDER(vr_profile_interval, "Report Interval", 0.f, 30.f, 1.f, "%.0f s", noExt,
    "Performance Profile (Graphics): seconds between its reports in the console and its CSV. 0: only on Dump Profile.")
QVR_SLIDER(vr_dynres_test_load, "Artificial GPU Load", 0.f, 200.f, 5.f, "%.0f", noExt,
    "vr_dynres_test_load: extra GPU work in every pixel of each eye (it changes nothing seen), to watch "
    "Dynamic Resolution drop the scale and raise it back (the status box's Dynamic res line). 0: none.")
QVR_TOGGLE(vr_dynres_log, "Dynamic Resolution Log",
    "vr_dynres_log: a console line at each scale change, with the eyes' GPU time and the budget.")
QVR_CYCLE(vr_gl_markers, "GL Debug Groups",
    "vr_gl_markers: each GPU profile scope (eye L/R, depth prepass, shadow maps, post, 2D, xr submit...) "
    "named as a GL debug group, so RenderDoc and Nsight captures show the passes. With a Debugger: only "
    "when one (or RenderDoc, Nsight) is in the process. Their count with GL Debug Groups Report.",
    {0.f, "Off"}, {1.f, "On"}, {2.f, "With a Debugger"})
QVR_TOGGLE(vr_qcprofile, "Time QuakeC Functions",
    "vr_qcprofile: each server QuakeC function's time (its own, with the builtins it calls, and with its "
    "callees), its calls, and each builtin's, for QuakeC Time Report. Costs a little while on.")
QVR_CYCLE(vr_qcexec, "QuakeC Loop",
    "vr_qcexec: the QuakeC interpreter's loop. Fast (goto), the default: the same results, errors and "
    "instruction counts as the original, quicker (docs/vr-port/QC_VM.md). Original: the old loop, to "
    "compare or to rule it out.",
    {0.f, "Original"}, {1.f, "Fast (goto)"}, {2.f, "Fast (switch)"})
QVR_CYCLE(vr_gun_pieces_cache, "Gun Shapes on Disk",
    "The guns' convex pieces kept on disk (cache/gunpieces) and read back the first time a gun is dropped, "
    "picked up or holstered in a session, instead of cut again (10-85 ms each: a hitch as the first "
    "grunt's gun drops). Check: read, then cut anyway and compared (Gun Shapes Stats counts them). Off: "
    "cut once every session (vr_gun_pieces_cache).",
    {0.f, "Off"}, {1.f, "On"}, {2.f, "Check"})
QVR_CYCLE(vr_gun_pieces_ahead, "Gun Shapes at Load",
    "A map's load makes the convex pieces of every weapon it can give (lying about, its monsters' drops, "
    "backpacks, your hands and holsters) from their files, or cuts them (on the job pool), so the first "
    "pickup mid-game is not a hitch. Off: cut at the first pickup (vr_gun_pieces_ahead).",
    {0.f, "Off"}, {1.f, "On"})
QVR_CYCLE(vr_bench_profiler, "External Profiler Collects",
    "For a CPU profiler run on the game (VTune started paused: -start-paused): Benchmark Windows resumes its "
    "collection for each Benchmark Capture (vr_bench_begin) alone, Map Loads for each load alone (the map "
    "command to its first frame drawn), so gameplay and loading are profiled apart (vr_bench_profiler; "
    "vr_profiler_collect 0|1 by hand).",
    {0.f, "As Started"}, {1.f, "Benchmark Windows"}, {2.f, "Map Loads"})
QVR_TOGGLE(vr_shadow_layered, "Layered Shadow Casters",
    "vr_shadow_layered: each shadow caster drawn once per light into all the faces it reaches (the GPU picks "
    "the face), instead of once per face: fewer draw calls, the same shadows. Off: a face at a time (also when "
    "the GPU lacks GL_ARB_shader_viewport_layer_array).")
QVR_CYCLE(vr_alias_eye_cache, "Second Eye's Model Set-up",
    "vr_alias_eye_cache: each animated model's set-up that is the same for both eyes (its pose, place, light "
    "at its origin, instance data) made in the first eye and reused by the second (First Eye's), made again "
    "(Made Again), or made again and compared with the first eye's (Checked: see Model Set-up Stats).",
    {0.f, "Made Again"}, {1.f, "First Eye's"}, {2.f, "Checked"})
QVR_TOGGLE(vr_setup_screens_cache, "Button Screens' List",
    "vr_setup_screens_cache: the buttons with value screens over them (the calibration room's settings, the "
    "hub's campaigns and Continue) found by a walk of every entity every half second (on), or every frame "
    "(off, as before: 0.1-0.2 ms a frame in Arcane Dimensions' biggest maps). The same screens either way.")
QVR_TOGGLE(vr_place_entities_first, "Crates' Placing: Entities First",
    "vr_place_entities_first: a place for a crate, barrel, rock or brick (at a map's start) tested against "
    "the map's entities before its traces (on), or after them (off, as before). The same places either way; "
    "a big map starts faster (150 ms in Arcane Dimensions' Sepulcher).")
QVR_CYCLE(vr_wounds_budget, "Wound Painting Budget",
    "vr_wounds_budget: the wounds painted at most this long a frame, the rest of a burst (an explosion's, a "
    "dismembering's: hundreds at once) painted over the next frames where they struck their bodies, all "
    "within vr_wounds_budget_wait (0.04 s); Off: all in the frame they came, as before. Wounds (Debug > Info) "
    "shows the queue.",
    {0.f, "Off"}, {0.5f, "0.5 ms"}, {1.f, "1 ms"}, {2.f, "2 ms"})
QVR_TOGGLE(vr_place_keep, "Keep Crates' and Rocks' Layouts",
    "vr_place_keep: the crates', rocks' and bricks' last layout kept and taken again when the map, its "
    "entities and their settings are the same (on: a restart after a death loads faster, 200-250 ms in "
    "Arcane Dimensions' big maps), or planned again at every load (off, as before). The same layout either way; "
    "another seed or setting plans anew.")
QVR_TOGGLE(vr_load_prewarm, "Load Prewarm",
    "vr_load_prewarm: made in a map's load (on), or at their first use as before (off): each model's rig data "
    "for smooth animation (6-27 ms the first time a monster kind was seen), the reflections' world buffers, the "
    "guns' screen anchors (10 ms the first time a gun lay near you), your skin masks and glove paint.")
QVR_CYCLE(vr_alias_cull_posed, "Cull Posed Models",
    "vr_alias_cull_posed: your body, its mirror image, the jointed hands and the ragdolls left out of each "
    "eye's view and each shadow map face their bones' box does not reach (On), or drawn into all of them "
    "(Off, as before), or On with each one culled skinned again on the CPU and checked to lie wholly outside "
    "(Checked: see Model Cull Stats). The same images either way.",
    {0.f, "Off"}, {1.f, "On"}, {2.f, "Checked"})
QVR_CYCLE(vr_body_prepass, "Body Depth Pre-pass",
    "vr_body_prepass: your body and its mirror image drawn into the eye's depth first, then shaded once a "
    "pixel (On), or shaded as drawn, the arms over the torso shaded twice (Off, as before). The same image "
    "either way.",
    {0.f, "Off"}, {1.f, "On"})
QVR_CYCLE(vr_alias_prepass, "Model Depth Pre-pass",
    "vr_alias_prepass: the world's opaque models (monsters, props, gibs, items) drawn into the eye's depth "
    "first, each batch, then shaded once a pixel (On), or shaded as drawn (Off, as before): crowds, props "
    "and gore 4-23% less GPU on models at a headset's resolution. The same image either way.",
    {0.f, "Off"}, {1.f, "On"})
QVR_TOGGLE(vr_oit_lazy_clear, "Lazy OIT Clear",
    "vr_oit_lazy_clear: the translucent pass's two buffers cleared at a view's first translucent draw, and "
    "neither cleared nor resolved in a view with nothing translucent (on), or cleared and resolved in every "
    "view (off, as before). The same image either way.")
QVR_CYCLE(vr_shadow_setup_share, "Shadow Caster Set-up Shared",
    "vr_shadow_setup_share: each animated shadow caster set up once for all the frame's shadow lights (its "
    "whole instance copied into each light's draw, drawn without textures) and the first eye taking the "
    "shadow pass's pose and transform for the same models (On); only the pose and transform shared by the "
    "lights (Off); or On with the first eye's worked out again and compared (Checked: see Model Set-up "
    "Stats). The same images either way.",
    {0.f, "Off"}, {1.f, "On"}, {2.f, "Checked"})
QVR_TOGGLE(vr_shadow_caster_cache, "Keep Light Casters",
    "vr_shadow_caster_cache: each dynamic light's world shadow casters kept between frames while it stays "
    "where they were collected for (a moving light's collected a little wider); off: collected again every "
    "frame. The same images either way.")
QVR_TOGGLE(vr_stereo_single, "Single-Pass Stereo (Experimental)",
    "vr_stereo_single: the opaque world, brush models and the models away from the eyes drawn once for both "
    "eyes (GL_OVR_multiview2) instead of once per eye; the rest is still drawn per eye. Less CPU per frame, "
    "the same images (docs/vr-port/STEREO.md). Off with MSAA, or when the driver lacks multiview.")
QVR_TOGGLE(vr_cache_warmup, "Cache Warm-up",
    "On: the derived-data cache is filled in the background (every installed game's models' occlusion bakes) on "
    "one low-priority worker, paused while a map loads or a frame runs late; it carries on next session where "
    "it stopped (vr_cache_warmup).")
QVR_TOGGLE(vr_probe_kinds, "Ready What Can Appear",
    "On: the kinds of monster that can appear later on a map (the firing range's dispensers and dummies, monsters "
    "waiting for a trigger) are made ready as it loads: their models, sounds and compiled hull, so their first "
    "appearance drops no frame (15-25 ms otherwise). The next map load (vr_probe_kinds).")
QVR_CYCLE_BYNAME(vr_probe_test_spawn, "Ready the Debug Spawner's",
    "The debug spawner's monsters (vr_test_spawn, impulse 241) made ready as each map loads too: Its Kind the one "
    "vr_test_spawn names, Every Kind all 23 (a longer load). The next map load (vr_probe_test_spawn).",
    {0.f, "Off"}, {1.f, "Its Kind"}, {2.f, "Every Kind"})
QVR_SLIDER(vr_image_cache_mb, "Decoded Image Cache", 0.f, 2048.f, 64.f, "%.0f MB", noExt,
    "Image files decoded once are kept for later loads up to this size (a texture pack's world textures and "
    "material maps: QRP's E1M1 loads 0.5 s faster the second time); the least recently used go first. 0: none "
    "kept (vr_image_cache_mb).")
QVR_TOGGLE(vr_campaign_keep_models, "Campaign Switch Keeps Models",
    "On: a switch of campaign (id1 to a mission pack and back) keeps the models whose files are the same in "
    "its folders (each file their load looked for is found the same again), instead of loading every one "
    "again (vr_campaign_keep_models).")
QVR_TOGGLE(vr_texture_prefetch, "Decode Textures Ahead",
    "On: a map load decodes its textures' replacement images (their glows, the external pack's maps) on worker "
    "threads while it uploads them in order: the same pixels, QRP's E1M1 about 0.15 s faster the first time. "
    "Off: each decoded on the main thread as it is uploaded (vr_texture_prefetch).")
QVR_SLIDER(vr_particle_saturate_cover, "Skip From Coverage", 0.f, 50.f, 1.f, "%.0f views", noExt,
    "vr_particle_saturate_cover: from how many screens' worth of particles the frame uses it (it costs a "
    "clear, a few full-screen marks and a blend an eye; it pays from about 7). 0: always.")
QVR_SLIDER(vr_particle_saturate_batches, "Skip Batches", 1.f, 32.f, 1.f, "%.0f", noExt,
    "vr_particle_saturate_batches: the particles drawn in this many batches, the opaque pixels marked "
    "after each (more: earlier skipping, more marks). 1: nothing skipped.")
QVR_CYCLE(vr_particle_saturate_opacity, "Opaque At",
    "vr_particle_saturate_opacity: how opaque a pixel must be for the particles under it to be skipped.",
    {0.99f, "99%"}, {0.999f, "99.9%"}, {1.f, "Never (no skipping)"})
QVR_TOGGLE(vr_debug_particle_waste, "Count Wasted Particle Fill",
    "vr_debug_particle_waste: the particles' shader counts the fragments it shades and those that show "
    "nothing or next to nothing (slower while on). Print with Particle Fill Waste Report.")
QVR_TOGGLE(vr_particle_freeze, "Freeze Particles",
    "vr_particle_freeze: the particles stop where they are (still drawn), to compare settings on the same "
    "frame.")
QVR_TOGGLE(vr_particle_halfres_force, "Half-Res Particles Always",
    "vr_particle_halfres_force: the particles drawn at half resolution in every frame with some in view "
    "(when Half-Res Heavy Particles or Half-Res Retro Particles allow it), not only in dense effects: to "
    "compare with Freeze Particles.")
QVR_CYCLE(vr_particle_halfres_upsample, "Half-Res Blend",
    "vr_particle_halfres_upsample: how half-resolution particles are blended into the view. By Depth: each "
    "pixel takes the half-size texels at its own distance (no smoke bled onto a crate's edge in front of "
    "it). Edges Shown: the pixels that took texels by distance in green. Filtered: the four blended, as "
    "before.",
    {0.f, "Filtered (before)"}, {1.f, "By Depth"}, {2.f, "By Depth, Edges Shown"})
QVR_TOGGLE(host_fixedtick, "Fixed 72 Hz Server Tick",
    "host_fixedtick: the server (monsters, physics, your hands' blows) runs in steady 1/72 s ticks at any "
    "headset rate, the leftover time carried to the next frame. Off: the old way, a server frame once 1/72 s "
    "had built up: 36 Hz on a 72 Hz headset, 45 at 90, 60 at 120, 48 at 144 (to compare).")
QVR_TOGGLE(vr_jobs_parallel, "Split Work Between Threads",
    "The game's thread pool shares out the grasp solve, the liquids' volume, the decal atlas and the models' "
    "occlusion bakes. Off: the calling thread does all of it (the same results, slower: to compare).")
QVR_TOGGLE(snd_mixasync, "Sound Mix on the Pool",
    "snd_mixasync: the sound's mix-ahead painted on the thread pool while the screen updates (the spatial "
    "audio's voices too), joined before anything else touches the sound. Off: on the main thread in the "
    "render, as before (the same samples, to compare).")
QVR_TOGGLE(vr_zone_threadcheck, "Catch Memory Use off the Main Thread",
    "The game crashes at once, with the culprit's stack (the crash report), when a thread other than the main one "
    "uses the hunk, the model cache or the zone (none of them is thread-safe; vr_zone_threadcheck). For "
    "testing: a map load's crash in its data is often one of these.")
QVR_TOGGLE(vr_hitmodel_cachestress, "Evict Models at Map Start",
    "vr_hitmodel_cachestress: at the next map's start, each model's data is dropped from the model cache once "
    "the next is loaded (what a full cache does), before precise hit detection's meshes are made on the "
    "thread pool. They must come out the same (developer 1: the hash in the console). For testing.")
QVR_CYCLE(vr_jobs_threads, "Worker Threads",
    "The thread pool's workers besides the main thread (Auto: the CPU's threads less one). Changed, the pool is "
    "made again. -jobs <n> on the command line sets it from the start.",
    {0.f, "Auto"}, {1.f, "1"}, {2.f, "2"}, {3.f, "3"}, {4.f, "4"}, {8.f, "8"}, {16.f, "16"})
QVR_TOGGLE(vr_box3d_threads, "Physics on Threads",
    "Box3D's step (the props' physics) shared between the thread pool and the main thread. Off: the main "
    "thread steps it alone (the same results: Box3D is deterministic whatever its threads).")
QVR_CYCLE(vr_box3d_workers, "Physics Threads",
    "With Physics on Threads: at most this many threads step Box3D, the main thread one of them (All: every "
    "worker of the pool). Measured on a 24-core CPU: 4 is as fast as 6 or 8 up to ~500 awake bodies (and "
    "leaves cores to the render thread and the headset's runtime); 8 gains another 20% only from ~1000. "
    "All is slower than 8.",
    {0.f, "All"}, {2.f, "2"}, {3.f, "3"}, {4.f, "4"}, {6.f, "6"}, {8.f, "8"})
QVR_TOGGLE(vr_box3d_avx2, "Physics AVX2",
    "vr_box3d_avx2: Box3D's contact solver and hull contacts 8 at a time with AVX2, where the CPU has it "
    "(the console says which at the first map). Off (the default): 4 at a time with SSE2. The same results "
    "either way; measured in the game (an i9-13900K), AVX2 was no faster with 500 props and a little slower "
    "with 40 (Box3D alone, a box pile on a box floor, it is 20% faster). Takes effect at the next physics step.")
QVR_CYCLE(vr_box3d_threads_bodies, "Physics Threads From",
    "With Physics on Threads: shared out only while at least this many bodies are awake (a smaller step is "
    "faster on the main thread alone: handing it out costs more than it saves). Measured: even at ~100 awake, "
    "4 threads 15% faster at 125, 25% at 150, 40-50% at 300-500, 2-3x at 1000.",
    {0.f, "Always"}, {100.f, "100 Bodies"}, {150.f, "150 Bodies"}, {200.f, "200 Bodies"}, {300.f, "300 Bodies"},
    {500.f, "500 Bodies"}, {1000.f, "1000 Bodies"})
QVR_CYCLE_BYNAME(vr_memstats_log, "Memory Log",
    "Write memory use and the frame rate to quakevr/profile/memstats_<date>.csv, and after each map load.",
    {0.f, "Off"}, {30.f, "Every 30 s"}, {60.f, "Every minute"}, {300.f, "Every 5 minutes"})
QVR_CYCLE_BYNAME(vr_memstats_glscan, "Memory Log: GL Count",
    "vr_memstats_glscan: the log's count of GL objects at each map load. Skip free names (5 ms): the names "
    "the driver hands out as free are not asked about, the same count; every name (13 ms): each asked up "
    "to 4096 past the last object; both: compared at every load, printed.",
    {0.f, "Every name"}, {1.f, "Skip free names"}, {2.f, "Both, compared"})
QVR_TOGGLE(vr_memstats_log_gpu, "Memory Log: GPU",
    "Also sample the GPU for the Memory Log (its clocks, slowdowns, and each program's use of it: SteamVR, "
    "Virtual Desktop), on a thread of its own. Off: only while profiling (Performance Profile, the Profiler Panel or its CSV Capture).")
QVR_CYCLE_BYNAME(vr_heap_purge_delay, "Heap: Purge Delay",
    "vr_heap_purge_delay: how long the heap (mimalloc) keeps freed memory before giving it back to the system. "
    "At once: the smallest working set. 1 s, mimalloc's own default: map loads about 15% quicker, 0.3 to 1 GB "
    "more memory held after them.",
    {0.f, "At once"}, {10.f, "10 ms"}, {1000.f, "1 s (mimalloc's)"}, {-1.f, "Never"})
QVR_CYCLE_BYNAME(vr_heap_load_hold, "Heap: Hold During Loads",
    "vr_heap_load_hold: a map load keeps the memory it frees for its own later allocations, then gives it all "
    "back at its first frame drawn. Off: freed memory goes back as Purge Delay says (a big map's hitbox "
    "build much slower: its threads queue on the system's memory calls).",
    {0.f, "Off"}, {60000.f, "On"})
QVR_TOGGLE(vr_glsafe, "GL Safe Mode",
    "vr_glsafe, from the next start: the optional GL features off (persistent mapped buffers, bindless textures, "
    "multi-bind, clip control, layered shadow casters, debug output, MSAA, the 8192 shadow atlas), each one "
    "logged. Also -glsafe, and on by itself after a start that crashed before its first map.")
QVR_TOGGLE(vr_gl_workarounds, "AMD Workarounds",
    "vr_gl_workarounds, from the next start: on AMD/ATI GPUs bindless textures off, as -nobindless (1.0.0 crashed "
    "in AMD's driver at start-up with them). Off: every feature the driver offers.")
QVR_TOGGLE(vr_diagnostics, "Diagnostics Mode",
    "vr_diagnostics, from the next start (as -diagnostics or the Diagnostics shortcut): a debug GL context and "
    "its messages, the OpenXR debug messenger, what is said to a debugger, the console, the crash report and "
    "the runtimes' logs, all in quakevr/diagnostics/<date>_<time>. Slower: for a bug report only.")
QVR_TOGGLE(vr_diagnostics_xr_validation, "OpenXR Validation Layer",
    "vr_diagnostics_xr_validation, from the next VR start, in diagnostics mode only: Khronos' core validation "
    "layer checks every OpenXR call the game makes (openxr_validation.log in the diagnostics folder). Off: "
    "not loaded, if the runtime behaves differently with it.")
QVR_TOGGLE(vr_xr_guard, "Runtime Crash Guard",
    "vr_xr_guard: a crash inside the OpenXR runtime while the game makes its eye images or stops it is caught "
    "and logged (qvr_openxr.txt): that VR start fails, Auto tries the next runtime, else the game plays flat. "
    "Off: the game crashes with its report.")
QVR_TOGGLE(vr_xr_api_layers, "OpenXR API Layers",
    "vr_xr_api_layers, from the next VR start: off turns off every implicit OpenXR API layer installed "
    "(overlays, toolkits, capture tools between the game and the runtime) by its own switch. "
    "vr_xr_runtime_explain lists them, and the other programs' DLLs in the game.")
QVR_TOGGLE(vr_xr_skip_incompatible_layers, "Skip Incompatible API Layers",
    "vr_xr_skip_incompatible_layers, from the next VR start: the implicit OpenXR API layers known not to "
    "work with this OpenGL game (OBS's OpenXR mirror: Direct3D only) are turned off by their own switch, "
    "each said in the console and qvr_openxr.txt. Off: allowed.")
QVR_TOGGLE(vr_flatplay_debug, "Flatscreen Play Print",
    "Prints each E and V press, and where the flatscreen hands are, to the console.")
QVR_SLIDER(vr_flat_reach, "Flatscreen Hand Reach", 0.25f, 3.f, 0.05f, "%.2f", noExt,
    "How far the flatscreen hands sit from the shoulders: a held gun out in front of the chest, an empty "
    "hand at its side.")
QVR_SLIDER(vr_flat_sway, "Flatscreen Move Sway", 0.f, 3.f, 0.05f, "%.2f", noExt,
    "How much the flatscreen hands lag and settle as you start, stop, jump and land (0: none). Looks only: "
    "the shots go along the crosshair.")
QVR_SLIDER(vr_flat_bob, "Flatscreen Walk Bob", 0.f, 3.f, 0.05f, "%.2f", noExt,
    "How much the flatscreen hands bob as you walk (with your speed) and breathe at rest (0: none). Looks "
    "only.")
QVR_SLIDER(vr_flat_turnlag, "Flatscreen Turn Lag", 0.f, 3.f, 0.05f, "%.2f", noExt,
    "How much the flatscreen hands trail a turn before catching up, as in modern shooters (0: locked to "
    "the view). Looks only: the shots go along the crosshair.")
QVR_SLIDER(vr_flat_turnlag_speed, "Flatscreen Turn Lag Recovery", 0.1f, 5.f, 0.05f, "%.2fx", noExt,
    "How fast the flatscreen hands catch up with a turn: higher, quicker and less lag.")
QVR_SLIDER_BYNAME(vr_anim_smooth_rig_error, "Limb Fit Limit", 0.25f, 5.f, 0.25f, "%.2f units", noExt,
    "vr_anim_smooth_rig_error: a bone fitted to a frame worse than this (rms) isn't swung between it and its neighbours; a vertex further off its bone swings less (none at twice it).")
QVR_SLIDER_BYNAME(vr_anim_smooth_rebuild_error, "Rebuild Fit Limit", 0.1f, 2.f, 0.1f, "%.1f units", noExt,
    "vr_anim_smooth_rebuild_error: with Rebuild From Skeleton, a point further than this (rms over the frames) off its bone's fits is rebuilt less (not at twice it); a model with under half its points rebuilt is drawn as before (Smooth Animation Models: each model's counts).")
QVR_TOGGLE(vr_ao_cache, "Keep AO Bakes on Disk",
    "vr_ao_cache: the models' occlusion bakes kept on disk, so a later session's loads bake nothing (off: baked at every first load; 2 in the console: baked anyway and compared with the file).")
QVR_TOGGLE(vr_menu_link_dryrun, "Menu Links: Print, Do Not Open",
    "vr_menu_link_dryrun: the version box's Support on Ko-fi link (bottom right of the menus) prints its address "
    "in the console instead of opening the desktop's browser (tests).")
QVR_CYCLE(vr_obs_process_check, "OBS: Process Check",
    "vr_obs_process_check: On (the default): on this PC the game connects only while OBS's process "
    "(obs64.exe) runs, and says when its WebSocket server is off. Off: always tries (a mock server's "
    "tests). Act as if Running: the hint's test (nothing listening reads as OBS's server off).",
    {0.f, "Off (Always Connect)"}, {1.f, "On"}, {2.f, "Act as if Running"})
QVR_SLIDER(vr_ghost_hands_test_offset, "Ghost Hands Test Offset", 0.f, 30.f, 1.f, "%.0f cm", noExt,
    "vr_ghost_hands_test_offset: moves each ghost hand this far sideways, to see it apart from the drawn hand without a heavy weapon or a wall.")
QVR_TOGGLE(vr_maps_allow_progs, "Include Packages with progs.dat",
    "vr_maps_allow_progs: packages that ship their own progs.dat replace the game's code, so they are left out of the list by default. They are in the index either way (maps_info shows them, maps_stats counts them).")
QVR_SLIDER(vr_maps_cache_mb, "Download Cache Size", 0.f, 4096.f, 64.f, "%.0f MB", ext(0.f, 65536.f),
    "vr_maps_cache_mb: the downloaded packages' zips (cache/maps/) kept up to this size; past it the oldest are removed (before a download, at start-up, and when this is lowered). 0: none kept once a package is installed. Installed maps are not affected.")
QVR_SLIDER(vr_maps_max_download_mb, "Largest Download", 0.f, 4096.f, 64.f, "%.0f MB", ext(0.f, 65536.f),
    "vr_maps_max_download_mb: a package whose zip is over this size is refused (the index's size, and the bytes as they arrive). 0: no limit (the default). The free disk space is checked either way, before the download and before the unpacking.")
QVR_SLIDER(vr_maps_debug_free_mb, "Pretend Free Disk Space", 0.f, 4096.f, 64.f, "%.0f MB", ext(0.f, 65536.f),
    "vr_maps_debug_free_mb: test aid - the map installer is told the disk has this much free (a low disk simulated: its message, before the download or the unpacking). 0: the disk's real free space.")
QVR_TOGGLE(vr_permdecal_show, "Show Permanent Decal Pages",
    "vr_permdecal_show: the baked marks' pages drawn unlit and green, the marks on them (where the layer has texels; a mark on a door moves with it).")
QVR_CYCLE_BYNAME(vr_gadget_test_state, "Gadget Screen Readings",
    "vr_gadget_test_state: the wrist gadget's screen shows made-up readings, to see each state of its layout: low health, ammo and "
    "stamina; no stamina with a counter's window open; hanging with bullet time running; maps being relit; every key, powerup and sigil; "
    "Arcane Dimensions' artifacts, a timer, custom keys and debuffs (its icons need its data installed).",
    {0.f, "Real"}, {1.f, "Low"}, {2.f, "Exhausted, Counter"}, {3.f, "Hanging, Bullet Time"}, {4.f, "Relighting"},
    {5.f, "Every Item"}, {6.f, "Arcane Dimensions Row"})
QVR_CYCLE_BYNAME(vr_setup_debug, "Calibration Trace",
    "vr_setup_debug: each step as it starts and, at the summary, how many frames the stickman showed and "
    "was hidden since it first showed (a hidden frame printed as it happens). Every Frame: one line a frame.",
    {0.f, "Off"}, {1.f, "Steps"}, {2.f, "Every Frame"})
QVR_TOGGLE(vr_debug_spawn_toolgun, "Spawn With a Toolgun",
    "vr_debug_spawn_toolgun: each time you spawn (a new level, a respawn), a toolgun in the first empty holster "
    "(the hips, the chest, the shoulders; right first) unless you carry one. Weapon Mode Immersive.")
QVR_TOGGLE(vr_toolgun_armed_tiles, "Armed Tiles on the Spawn Grid",
    "vr_toolgun_armed_tiles: the Monsters tab also lists the Armed Grunt and Armed Enforcer tiles, one a weapon "
    "(the Armed Humanoid page makes them all).")
QVR_TOGGLE(vr_brush_autoload, "Load Brushes with the Map",
    "vr_brush_autoload: a map started (not a saved game) gets the brushes of its brush file (Save This Map's "
    "Brushes).")
QVR_TOGGLE(vr_brush_bake, "Bake the Brushes' Light",
    "vr_brush_bake: each brush's faces lit from the map's lights in the game (shadows, the sun, flickering lights "
    "in their own lightstyle), a few ms a frame; off: the flat light where each face is.")
QVR_TOGGLE(vr_brush_shade, "Shade the Boxes' Faces",
    "vr_brush_shade: a box's flat light shaded by each face's way (top full, sides 0.8, bottom 0.55); off, every "
    "face as the light where it is. For boxes made after the change.")

// vr_menu_debug_tests.inc: staminaReadout
QVR_SLIDER(vr_mg3_ghost_alpha, "Ghosts' Opacity", 0.05f, 1.f, 0.05f, "%.2f", noExt,
    "How solid Dawn of the Machine's ghosts look: 0.4 see-through (default), 1 solid.")
QVR_TOGGLE(vr_mg3_aggro_groups, "Aggro Groups",
    "A waking monster wakes what its aggro_target names (map3, map7, map8). Upstream ships this off; off by default.")
QVR_TOGGLE(vr_lootbeam_debug, "Loot Beams: Debug Lines", "vr_lootbeam_debug: those counts once a second.")
QVR_TOGGLE(vr_debug_stick, "Stick: Show Speed",
    "vr_debug_stick: twice a second on the console, the moving stick's push (raw, and past the "
    "deadzones), the speed it asks (and the walking and running tops) and the speed you move at.")
QVR_TOGGLE(vr_debug_touch, "Doom: Show Touches",
    "vr_debug_touch: each touch's start and end printed (2: every frame).")
QVR_TOGGLE(vr_debug_doom, "Doom: Debug",
    "vr_debug_doom: Doom's own text, the focus changing and menu taps printed.")
QVR_SLIDER_BYNAME(vr_ad_widowmaker_spin_at, "Arcane Dimensions: Widowmaker Barrels Held At", -1.f, 360.f, 5.f, "%.0f deg", noExt, nullptr)
QVR_SLIDER(vr_test_pile_count, "Pile Size", 50.f, 1000.f, 50.f, "%.0f props", ext(10.f, 2000.f),
    "How many props Pile of Rocks, Pile of Bricks and Mixed Pile put ahead of you. 500: about 350 bodies "
    "awake as they fall (Physics on Threads at work: about twice as fast), one frame of ~9 ms as they "
    "appear; 1000: a 20+ ms frame as they appear, then 1-2 ms steps on threads.")
QVR_SLIDER(vr_test_pile_crates, "Crates in the Wall", 8.f, 120.f, 8.f, "%.0f crates", ext(1.f, 400.f),
    "How many small crates Wall of Crates stacks ahead of you (walls 8 wide and 5 high, one behind another; "
    "80: two walls). Standing walls cost little (under 0.1 ms a step); knocked over, a wall wakes its 40.")
QVR_SLIDER(vr_test_spawn_dist, "Distance", 32.f, 256.f, 8.f, "%.0f units", ext(), "How far ahead.")
QVR_CYCLE(vr_test_spawn_facing, "Facing",
    "Which way monsters put ahead of you face: Put It There, impulse 244 (a grunt ahead) and vr_physics_spawn "
    "(Spawn a Crowd). As Each Spawner: the first two face you, vr_physics_spawn keeps the map's angle. Away: "
    "they look where you look (trailer shots from behind them).",
    {0.f, "As Each Spawner"}, {1.f, "Towards You"}, {2.f, "Away"}, {3.f, "Its Own Angle"},
    {4.f, "Its Left Side to You"}, {5.f, "Its Right Side to You"})
QVR_TOGGLE(vr_test_spawn_hold, "Into the Main Hand",
    "A box or a crate (Health Box .. Explosive Box, the crates) put into your empty main hand, as if gripped: "
    "to test held props (the blood on what you hold: vr_gore_spatter_test, vr_gore_hands_info).")
QVR_CYCLE(vr_test_spawn_dead, "As a Corpse",
    "A monster killed at once: a corpse, to test gibbing and carrying; Gibbed: killed hard enough to gib (its "
    "gibs and head to pick up); Ragdoll: a corpse with ragdolls on (vr_ragdoll 1: a grunt, knight, ogre, enforcer, death knight, rottweiler, scrag, fiend, shambler or gremlin goes limp as he "
    "falls).",
    {0.f, "Off"}, {1.f, "Corpse"}, {2.f, "Gibbed"}, {3.f, "Ragdoll"})
QVR_SLIDER(vr_test_spawn_yaw, "Box Turned", 0.f, 90.f, 1.f, "%.0f degrees", ext(),
    "A box (Health Box .. Explosive Box): let loose and turned this far about its upright, to test the "
    "grappling hook against its real shape (its turned box's empty corners let the hook by).")
QVR_SLIDER(vr_test_spawn_tilt, "Box Tilted", 0.f, 90.f, 1.f, "%.0f degrees", ext(),
    "A box: tipped this far about the way you face, on its lowest corner (it topples: sv_gravity 0 keeps it so).")
QVR_TOGGLE_BYNAME(vr_melee_swept, "Swept Blades",
    "vr_melee_swept: a weapon's blow is the surface its whole blade sweeps each frame (a hammer's or mace's "
    "head: its volume), landing where it crossed the thing most squarely; off: its points' lines alone, the "
    "outermost striking, as before.")
QVR_CYCLE_BYNAME(vr_kdtrig_debug, "Show Ragdoll Triggers",
    "vr_kdtrig_debug: a `kdtrig:` line for each hit that knocks a humanoid down by stairs, a ledge, a slope, "
    "a close shotgun blast or its wounds (which, its chance and roll), each thrown body and shoulder "
    "charge; Also Lost Rolls: each that held but missed, and, each half second while you run fast, why "
    "your pose isn't a shoulder charge (Combat > Ragdoll Triggers).",
    {0.f, "Off"}, {1.f, "Log"}, {2.f, "Also Lost Rolls"})
QVR_SLIDER(vr_kdtrig_test_speed, "Test Throw Speed", 50.f, 800.f, 10.f, "%.0f u/s", noExt, "vr_kdtrig_test_speed: Fling the Nearest Body's speed.")
QVR_TOGGLE(vr_struggle_force, "Every Downed One Struggles",
    "vr_struggle_force: every knocked-down humanoid struggles as if held by its chest where it lies (to see "
    "the moves, and their cost with many).")
QVR_TOGGLE(vr_enemy_melee_arc_debug, "Log Melee Arc",
    "Prints each monster blow at you: its angle off the monster's facing, its arc and reach (Combat > "
    "Enemy Melee) and whether it landed.")
QVR_TOGGLE(vr_enemy_aim_debug, "Log Aim Cone",
    "Prints each monster shot at you: its angle off the monster's facing, its aim and fire cones (Combat > "
    "Enemy Melee > Ranged Aim), how long it waited and how far it was bent.")
QVR_TOGGLE(vr_legsweep_debug, "Log Leg Sweeps",
    "vr_legsweep_debug: each time what a hand holds meets a humanoid's lower legs the console prints its "
    "measure (length, slenderness, the part that met them, the sweep's speed) and whether it swept them or "
    "why not; a sweep's fall is traced for a second (its feet along the sweep, which way it fell).")
QVR_CYCLE_BYNAME(vr_foegrab_debug, "Log Holds",
    "vr_foegrab_debug: the console logs each hold taken and let go and why (and a grip that found none), "
    "each two-hand throw and lift tried and how it went (why a lift wasn't: not a humanoid, too far), a "
    "ragdoll held on and let go of or broken free, and each two-hand hold's hardest turn; Every Frame: each "
    "held enemy's movement, each hand's stretch, both hands' turn.",
    {0.f, "Off"}, {1.f, "Taken and Let Go"}, {2.f, "Every Frame"})
QVR_CYCLE(vr_test_weaponinst_slot, "Holster", "The holster the two commands below use.",
    {0.f, "Right Hip"}, {5.f, "Left Hip"}, {4.f, "Right Shoulder"}, {3.f, "Left Shoulder"}, {9.f, "Right Upper"},
    {8.f, "Left Upper"})
QVR_CYCLE_BYNAME(vr_reload_debug, "Reload Prints",
    "Events: each take, load, refund, loss, magazine out and hold. Every Frame: a held round's distance to the "
    "port, a held magazine's pull, snap and apart, a hit's speed. And Magazine Grips: each empty hand's "
    "distance off the other gun's magazine (its box) and the grip it would take.",
    {0.f, "Off"}, {1.f, "Events"}, {2.f, "Every Frame"}, {3.f, "And Magazine Grips"})
QVR_TOGGLE_BYNAME(vr_ammo_pouch_wobble_test, "Shake the Ammo Pouch",
    "vr_ammo_pouch_wobble_test: the ammo pouch shaken as if walking, to see its rounds wobble standing still.")
QVR_CYCLE_BYNAME(vr_tutorial_goto, "Go to a Lesson",
    "In vrtutorial: puts you at that lesson's start (its checkpoint, taken: you come back there). Doors "
    "on the way stay as they are.",
    {0.f, "-"}, {1.f, "1 Moving"}, {2.f, "2 Buttons"}, {3.f, "3 Jumping"}, {4.f, "4 Swimming"}, {5.f, "5 Healing"},
    {6.f, "6 Melee"}, {7.f, "7 A Fight"}, {8.f, "8 Weapons"}, {9.f, "9 Darkness"}, {10.f, "10 Throwing"},
    {11.f, "11 Fire"}, {12.f, "12 The Arena"})
QVR_TOGGLE_BYNAME(vr_trailer_log, "Trailer Scene Log",
    "vr_trailer_log 1: the grunt's state (enemy, frame, think, stealth state, oblivious), the axe's place "
    "and yours, four times a second (the console). Off for takes: the lines would show.")
QVR_SLIDER_BYNAME(vr_climb_prop_mass, "Props Hold Like the Level From", 0.f, 200.f, 5.f, "%.0f kg", noExt,
    "vr_climb_prop_mass: a prop at least this heavy holds like a wall while you hang from it; lighter, your "
    "weight pulls it (Light Prop Pull) and it tips or slides towards you, and your hand lets go. Large crate "
    "65, explosive box 80, small crate 40, barrel 30. 0: every prop holds.")
QVR_SLIDER_BYNAME(vr_climb_prop_pull, "Light Prop Pull", 0.f, 100.f, 5.f, "%.0f kg", noExt,
    "vr_climb_prop_pull: the most weight a hand on a light prop pulls it with (your weight, Box3D's Player "
    "Mass, shared by the holding hands, towards your body).")
QVR_SLIDER_BYNAME(vr_climb_prop_tilt, "Prop Hold Tips Off At", 0.f, 45.f, 1.f, "%.0f deg", noExt,
    "vr_climb_prop_tilt: a prop's top turned this far from how it lay when you took hold lets your hand go.")
QVR_SLIDER_BYNAME(vr_climb_prop_slip, "Light Prop Slips Off At", 0.f, 32.f, 1.f, "%.0f units", noExt,
    "vr_climb_prop_slip: a light prop's hold moved this far from where you took it lets your hand go.")
QVR_SLIDER_BYNAME(vr_climb_prop_letgo_speed, "Prop Knocked Away At", 0.f, 600.f, 25.f, "%.0f u/s", noExt,
    "vr_climb_prop_letgo_speed: a held prop's hold whose speed changes this much in a frame (a blast, a hit) "
    "lets your hand go.")
QVR_TOGGLE(vr_nailslide_debug, "Slide: Print",
    "vr_nailslide_debug: the slide's place in each hand every frame it moves (0 home .. 1 back by Slide "
    "Travel) and its edges: a shot, the lock, the re-rack (\"nailslide: ...\").")
QVR_CYCLE_BYNAME(vr_nailpistol_model, "Nail Pistol Model (Archived)",
    "vr_nailpistol_model: its look. Full Slide (the default): the whole upper part is the slide, as a real "
    "pistol's. Classic (archived, kept to compare): a copper slide on top of the receiver. Changes at once.",
    {0.f, "Classic"}, {1.f, "Full Slide"})
QVR_TOGGLE(vr_debug_impacts, "Print Impact Effects",
    "vr_debug_impacts: every particle2 message and QC sound the server sends (\"impfx p2 <preset> <count> "
    "...\", \"impfx snd ...\"): the hit ladders' output (vr_impacts.cpp).")
QVR_TOGGLE(vr_debug_gunfx, "Print Gunfire Particles",
    "vr_debug_gunfx: each shot's particles (\"gunfx shot ...\", \"gunfx port ...\", \"gunfx impact ...\").")
QVR_TOGGLE(vr_debug_ribbon_overlap, "Wisps Overlap Again",
    "vr_debug_ribbon_overlap: the muzzle wisps blend twice where they fold over themselves (a sharp swing), "
    "as before the fix: the denser creases, to compare.")
QVR_TOGGLE(vr_debug_gunsfx, "Print Gunfire Layers and Punches",
    "vr_debug_gunsfx: a console line for each shot (its layers, tail, room), each punch and dry click.")
QVR_TOGGLE(vr_debug_weaponfx, "Print Weapon Effects",
    "vr_debug_weaponfx: each shot's recoil, flash and tracers (2: and the recoil each frame; 4: the main "
    "hand's gun as drawn each frame, its body's place and the recoil: \"recoiltrack\"); the shotgun's auto "
    "pump strokes (start, back, home) and when its shell leaves.")
QVR_SLIDER(vr_autopump_hold, "Hold the Auto Pump", -0.05f, 1.f, 0.05f, "%.2f", noExt,
    "vr_autopump_hold: every shotgun's fore-end held at that point of its auto pump's stroke, to look at it "
    "(0.35-0.45: at the back; below 0: off).")
QVR_TOGGLE(vr_weapon_kick_cancel, "Take Out the Models' Own Kick",
    "vr_weapon_kick_cancel: Quake's guns' firing frames kick the whole gun back and up in the model; that kick "
    "is taken out of their poses (the body exactly still; their pumps, barrels and drums still move; the "
    "grenade launchers wholly still) and the programmatic recoil kicks them instead. Off: the models' own "
    "kick again, to compare.")
QVR_SLIDER(vr_debug_weapon_frame, "Hold the Weapon Frame", -1.f, 12.f, 1.f, "%.0f", noExt,
    "vr_debug_weapon_frame: the held weapons drawn at this frame of their models (1-6: the firing frames of "
    "most), to look at a kick with and without the models' own (below 0: as the game steps them).")
QVR_SLIDER(vr_test_fling_speed, "Fling Speed", 1.f, 40.f, 1.f, "%.0f m/s", ext(), nullptr)
QVR_CYCLE(vr_test_fling_at, "Fling At", nullptr,
    {0.f, "Nearest Monster"}, {1.f, "You"})
QVR_TOGGLE(vr_test_fling_away, "Fling Away From It",
    "The prop starts inside the edge of its box and flies away from it (a prop batted away from you): it "
    "must never hurt.")
QVR_CYCLE(vr_test_held_pick, "Which Gib",
    "vr_test_held_pick: the gib or head Gib in the Off Hand gives (Each in Turn: the next each press). The "
    "torso and the big chunk are the large ones (Misc/quakevr/gib_2h_models_test.sh throws each). Rock, "
    "Shells Box, Brick: the nearest lying about; Hand Grenade: one from the pouch (Misc/quakevr/"
    "twohand_regrip_test.sh takes each in both hands and hands it over).",
    {-1.f, "Each in Turn"}, {0.f, "Small Gib (gib1)"}, {1.f, "Player's Head"}, {2.f, "Torso (gib2)"},
    {3.f, "Big Chunk (gib3)"}, {4.f, "Grunt's Head"}, {5.f, "Ogre's Head"}, {6.f, "Knight's Head"},
    {7.f, "Zombie's Head"}, {8.f, "Fiend's Head"}, {9.f, "Nearest Rock"}, {10.f, "Hand Grenade"},
    {11.f, "Nearest Shells Box"}, {12.f, "Nearest Brick"})
QVR_TOGGLE(vr_test_held_hand, "Into the Main Hand",
    "vr_test_held_hand: Gib in the Off Hand fills the main hand instead (hold its grip): a prop to hit the "
    "super shotgun's barrels open or shut with, the gun in the off hand (reload_test.sh section 11).")
QVR_TOGGLE(vr_test_held_destroy, "Real Gib",
    "vr_test_held_destroy: the off hand's test gib can be burst as a real one (shot, struck, thrown hard "
    "at a wall). Misc/quakevr/gib_2h_throw_test.sh throws it with both hands along several arcs.")
QVR_SLIDER(vr_test_throw_up_speed, "Throw Up Speed", 0.f, 25.f, 0.5f, "%.1f m/s", noExt,
    "0: let go of gently (dropped, not thrown): it must never hurt you.")
QVR_TOGGLE(vr_test_throw_up_body, "Throw Up From Your Body",
    "Throw the Nearest Prop Up lets it go at your chest, inside your body, as a long heavy thing let go "
    "of at the body (not from over your head).")
QVR_CYCLE(vr_test_axe, "Axe Throw",
    "How Throw an Axe throws it: blade first (the edge upright), the blade's side first, the handle's end "
    "first, blade first spinning end over end; or as a hand would, turned and spun a little differently "
    "each time: overhand (end over end), sidearm (the blade level, spun about the upright), sloppy (any "
    "way), upright (pushed, the handle upright, the blade ahead), knife-style (held by the head, the "
    "handle ahead, flicked end over end), spear-like (the head ahead), flat (the blade's side ahead) and "
    "handle first. The hand's throws aim up to meet what is ahead at eye level.",
    {0.f, "Blade First"}, {1.f, "Flat"}, {2.f, "Handle First"}, {3.f, "Spinning"}, {4.f, "Overhand"}, {5.f, "Sidearm"},
    {6.f, "Sloppy"}, {8.f, "Upright"}, {9.f, "Knife-Style"}, {10.f, "Spear-Like"}, {11.f, "Flat (Hand)"},
    {12.f, "Handle First (Hand)"})
QVR_SLIDER(vr_test_axe_speed, "Axe Speed", 2.f, 20.f, 1.f, "%.0f m/s", ext(), nullptr)
QVR_CYCLE(vr_test_axe_what, "Throw Instead",
    "Throws a gib or an explosive box (one that never blows up) the same way instead of the axe: to "
    "compare their spin in the air (Debug: Spin in the Air).",
    {0.f, "The Axe"}, {1.f, "A Gib"}, {2.f, "An Explosive Box"})
QVR_CYCLE(vr_test_axe_weapon, "Weapon Instead",
    "Throws that weapon instead of the axe, hurting as your hand's throws do (their damage by speed and "
    "weight; the training dummy shows it): Axe Speed is its speed.",
    {0.f, "The Axe"}, {2.f, "Axe (as a throw)"}, {4.f, "Shotgun"}, {7.f, "Super Nailgun"}, {12.f, "Laser Cannon"})
QVR_TOGGLE(vr_test_axe_damage, "Axe Hurts", "Off: its blow does no damage (to watch a monster bleed).")
QVR_CYCLE(vr_test_axe_at, "Axe At",
    "What Throw an Axe throws at: ahead of you, or the nearest live monster, door, loose prop or wall "
    "button (you are moved to face it, level with its middle; a button: square to its wall).",
    {0.f, "Ahead"}, {1.f, "Nearest Monster"}, {2.f, "Nearest Door"}, {3.f, "Nearest Prop"}, {4.f, "Nearest Button"})
QVR_SLIDER(vr_test_axe_dist, "Axe Range", 0.f, 400.f, 20.f, "%.0f units", noExt,
    "You are moved first to this far from what is ahead (0: where you are).")
QVR_SLIDER(vr_test_axe_up, "Axe Height", -30.f, 40.f, 1.f, "%+.0f units", noExt,
    "At a target: thrown this far above its middle (15: a grunt's head, to behead it: Gore > Decapitation).")
QVR_CYCLE(vr_test_projectile, "Projectile",
    "What Fire at Me fires at your face from 300 units: to bat back with a weapon, or catch (a grenade).",
    {0.f, "Knight's Spike"}, {1.f, "Enforcer's Laser"}, {2.f, "Scrag's Spit"}, {3.f, "Vore's Ball"},
    {4.f, "Ogre's Grenade"}, {5.f, "Zombie's Flesh"})
QVR_SLIDER(vr_test_projectile_side, "From the Left", -90.f, 90.f, 15.f, "%.0f deg", noExt,
    "Degrees to your left of ahead it comes from (negative: from the right).")
QVR_CYCLE(vr_test_grenade_shot, "Grenade Shot",
    "What Shoot the Nearest Grenade fires from your eyes (Shoot Grenades, on Batting and Catching).",
    {10.f, "Pellet"}, {1.f, "Nail"}, {0.f, "Rocket"}, {4.f, "Laser"})
QVR_SLIDER(vr_test_grenade_dist, "Dud Distance", 64.f, 1024.f, 32.f, "%.0f units", noExt, "How far ahead Drop a Dud Ahead puts the grenade.")
QVR_SLIDER(vr_test_grenade_height, "Dud Height", 0.f, 64.f, 2.f, "%.0f units", noExt,
    "How high over the floor Drop a Dud Ahead puts the grenade (it falls): at hand height to punch or swing at "
    "it (Blow Speed to Set Off, on Batting and Catching).")
QVR_TOGGLE(vr_test_grenade_yours, "Dud Is Yours",
    "On: the dud is yours (your blows pass through your own grenades). Off: nobody's, as an enemy's grenade "
    "lying about: your blows meet it (bat it, or set it off when very strong).")
QVR_TOGGLE(vr_debug_pain, "Print Hits",
    "Prints each hit (painhit: damage, side, knock, buzz) and the hands' knock each frame (painknock, cm along "
    "your body) (vr_debug_pain).")
QVR_TOGGLE(vr_debug_stamina_hold, "Hold Stamina",
    "Keeps your stamina where it is, or where the buttons above put it: nothing spends it and it doesn't come back (vr_debug_stamina_hold).")
QVR_CYCLE(vr_loading_notice, "Loading Notice",
    "\"Loading...\" in front of you before a level change (a map's exit, a map transition, a map started "
    "from the menus, a save loaded, a death's restart): the load waits until two headset frames showed "
    "it, as the runtime keeps showing the last frame while the game loads (vr_loading_notice). Mock Too: "
    "the mock headset's test runs too.",
    {0.f, "Off"}, {1.f, "Headset"}, {2.f, "Mock Too"})
QVR_CYCLE(vr_loading_hold, "Loading Notice Hold",
    "Keeps the notice up this long before each load starts, to look at it (vr_loading_hold; Off: two "
    "frames).",
    {0.f, "Off"}, {1.f, "1 s"}, {3.f, "3 s"})
QVR_TOGGLE(vr_unstick, "Unstick",
    "Found inside a wall, a door, a button or a lift, you're moved to the nearest free spot (vr_unstick). "
    "Off: only Quake's small nudge up.")
QVR_TOGGLE(vr_unstick_monsters, "Unstick Monsters",
    "A monster found inside a wall, the floor, a door or a lift, not moving, for the time below is moved to "
    "the nearest free spot (vr_unstick_monsters; Developer Messages prints each). Off: it stays stuck.")
QVR_SLIDER(vr_unstick_monsters_time, "Unstick Monsters After", 0.2f, 5.f, 0.2f, "%.1f s", noExt,
    "How long a monster stays stuck inside something before it is moved out (vr_unstick_monsters_time).")
QVR_TOGGLE(vr_debug_walkmove, "Print Walk Moves",
    "Prints each of your walk moves: the slide, the step up, the planes met, the ledge test "
    "(vr_debug_walkmove). For a report of stairs or slopes you can't walk up.")
QVR_CYCLE(vr_debug_sound_log, "Log Sounds",
    "Prints each sound the server starts: its entity, channel, sample and volume, and a dropped one's reason "
    "(not precached, a full datagram) (vr_debug_sound_log). Not Yours: monsters' and the world's only. For "
    "a report of a weapon or a monster that makes no sound.",
    {0.f, "Off"}, {1.f, "All"}, {2.f, "Not Yours"})
QVR_CYCLE(vr_hull_keep, "Keep Hitboxes for Reloads",
    "The map's brushes and compiled hulls kept in memory when it is left, for a load of the same map again "
    "(a death's reload, restart, a changelevel back): that load skips their build (vr_hull_keep). Off: "
    "built at every load.",
    {0.f, "Off"}, {1.f, "1 Map"}, {2.f, "2 Maps"}, {4.f, "4 Maps"})
QVR_CYCLE(vr_hull_cache, "Hitboxes on Disk",
    "A big map's compiled hulls kept on disk (cache/hulls) and read at its next load instead of compiled "
    "again (vrstart: 9 s to 0.1 s). Check: read, then compiled anyway and compared (Hitbox Stats counts "
    "them). Off: compiled at every load (vr_hull_cache).",
    {0.f, "Off"}, {1.f, "On"}, {2.f, "Check"})
QVR_CYCLE(vr_hull_build_mb, "Hitbox Build Memory",
    "How much memory a big map's hitbox build (its first load after an install or update) may hold at once "
    "(estimated): past it a tree's build waits for another to end. vrstart: about 330 MB a tree, three at "
    "once in 1 GB. No Limit: all at once (about 1 GB more, 5-10% quicker) (vr_hull_build_mb).",
    {0.f, "No Limit"}, {512.f, "512 MB"}, {1024.f, "1 GB"}, {2048.f, "2 GB"})
QVR_CYCLE(vr_hull_async, "New Monster Sizes in Play",
    "A monster whose box size the map's load did not compile (a delayed or triggered spawn, the test "
    "spawner): On the Pool compiles its hull in the background while it moves with Quake's own hull, "
    "no hitch; In the Frame compiles it at its first move, the game waiting (ad_tears: 1.3-1.9 s) "
    "(vr_hull_async; developer 1 prints the switch).",
    {0.f, "In the Frame"}, {1.f, "On the Pool"})
QVR_TOGGLE_BYNAME(vr_portals_debug_split, "Print Gate Cuts",
    "vr_portals_debug_split 1: each frame, every entity drawn cut by a teleporter (where, the plane, how far "
    "through), the force grab's beam end, and each thrown or rigid thing's middle and why a gate did not "
    "take it. -1 (console) also follows the main hand's held object, a number that entity, every frame.")
QVR_TOGGLE_BYNAME(vr_portals_unstick, "Get Out of a Teleporter's Wall",
    "vr_portals_unstick 1 (default): if you end up in the wall behind a teleporter, you are carried on "
    "through it (your torso past its plane and room at the far side) or put back in front of it at once. "
    "0: off, to see a softlock as it was.")

// vr_menu_ragtool.inc: pageRagtool
QVR_TOGGLE(vr_ragtool_loop, "Loop (L)", "Played again when it ends.")
QVR_SLIDER(vr_ragtool_length, "Watch For (s)", 1.f, 20.f, 0.5f, "%.1f", noExt,
    "How long a run lasts when its behaviour gives no length of its own.")
QVR_SLIDER(vr_ragtool_dir, "Push From (deg)", 0.f, 315.f, 45.f, "%.0f", noExt,
    "Where a shove, a blow, a knockdown or a parry comes from: 0 his front, 90 his left, 180 his back.")
QVR_CYCLE(vr_ragtool_hold, "Hold",
    "The virtual hand's hold for Struggle, Lift and Drag (H in the game). Move it: the arrows, Page Up/Down.",
    {0.f, "Chest"}, {1.f, "Head"}, {2.f, "Arm"}, {3.f, "Leg"}, {4.f, "Pelvis"})
QVR_SLIDER(vr_ragtool_speed, "Speed ([ ])", 0.05f, 2.f, 0.05f, "%.2fx", noExt, "The time scale while the tool runs.")
QVR_TOGGLE(vr_ragtool_show_bones, "Skeleton and Frames", nullptr)
QVR_TOGGLE(vr_ragtool_show_limits, "Joint Limits",
    "Cyan: a ball joint's cone; orange: a hinge's arc; magenta: the bone now.")
QVR_TOGGLE(vr_ragtool_show_targets, "Motor Targets",
    "Green: each joint's target, as bright as it is strong (grey limp); yellow: the pelvis pin; red: a hand's or foot's goal.")
QVR_TOGGLE(vr_ragtool_show_contacts, "Contacts", "Red: pressed into the world; pink: touching.")
QVR_TOGGLE(vr_ragtool_show_balance, "Centre of Mass and Support",
    "Yellow: the centre of mass; its point on the floor green over the feet's support (its outline), red off it.")
QVR_SLIDER(vr_ragtool_cam_dist, "Camera Distance (Wheel)", 24.f, 600.f, 8.f, "%.0f", noExt, nullptr)
QVR_SLIDER(vr_ragtool_cam_shift, "Subject Off Centre", -0.6f, 0.6f, 0.05f, "%.2f", noExt, nullptr)
QVR_CYCLE(vr_ragtool_log, "Log", nullptr,
    {0.f, "Off"}, {1.f, "Each Run"}, {2.f, "And Samples"})

// vr_menu_recording.inc: pageRecording
QVR_CYCLE_BYNAME(vr_window_view, "Window View",
    "What the desktop window shows, to record it (OBS: a Window Capture of it). Both Eyes: the two lenses' "
    "images side by side, as they are. Left or Right Eye (raw): that eye's image as it is, shaking with your "
    "head. Smoothed: the same image, turned to follow a steadied head and cropped to the window: free. "
    "Spectator Camera (Recording Mode): the game drawn a third time for the window, from your head, "
    "steadied, with its own field of view: sharpest and widest, and it costs about one more eye's "
    "rendering. The headset's view is the same in all of them.",
    {3.f, "Both Eyes (raw)"}, {0.f, "Left Eye (raw)"}, {1.f, "Left Eye (smoothed)"}, {4.f, "Right Eye (raw)"},
    {5.f, "Right Eye (smoothed)"}, {2.f, "Spectator Camera"})
QVR_TOGGLE_BYNAME(vr_window_hud_mirror, "Flat HUD on the Mirror",
    "The flat status bar over the window's raw and smoothed eye views (on by default).")
QVR_TOGGLE_BYNAME(vr_window_hud_spectator, "Flat HUD on the Spectator Camera",
    "The flat status bar over the Spectator Camera (off by default: the recording shows the game as the "
    "headset does, with the wrist gadget as its HUD).")
QVR_TOGGLE_BYNAME(vr_mirror_hide_hud_text, "Hide Head Text on the Mirror",
    "Hides the text that floats in front of your head (pickup messages, centre prints, notify lines) in the "
    "window's raw and smoothed eye views (off by default; in the raw views also the window's own copy of "
    "that text, at its top). The headset still shows it.")
QVR_TOGGLE_BYNAME(vr_spectator_hide_hud_text, "Hide Head Text on the Spectator Camera",
    "Hides the text that floats in front of your head (pickup messages, centre prints, notify lines) in the "
    "Spectator Camera (on by default: clean footage for trailers). The headset still shows it; menus "
    "and the wrist gadget stay.")
QVR_TOGGLE_BYNAME(vr_recording_clean, "Recording Mode (No Tips or Messages)",
    "No tips, no messages on the wrist gadget and no text in front of your head (pickups, centre prints, "
    "notify lines), in the headset too: clean takes. The trailer scene (vrtrailer) turns it on by "
    "itself while it is loaded; menus and the wrist gadget's own display stay.")
QVR_CYCLE_BYNAME(vr_mirror_hz, "Window Frame Rate",
    "How often the desktop window shows a new frame while it has the focus. Up to the Monitor's (the "
    "default): at most once each refresh of its monitor, so the window never holds the headset back (a "
    "60 Hz monitor with the headset at 90 Hz: 45 fps in the window; a monitor as fast as the headset: "
    "every frame). Every Headset Frame: smoother window on a slower monitor, for a 60 fps recording; if "
    "the headset then stutters while the window has the focus, the GPU driver makes the window wait for "
    "the monitor (its V-Sync or G-SYNC setting): go back. Unfocused: vr_mirror_unfocused_hz.",
    {-1.f, "Up to the Monitor's"}, {0.f, "Every Headset Frame"})
QVR_TOGGLE_BYNAME(vr_obs, "OBS Recording Control",
    "While a menu is open, the game looks for OBS Studio on this PC: when it finds it, a row above the "
    "Spectator Camera's switch (bottom left of the headset's menus) shows whether OBS is recording, and "
    "for how long; point at it and press to start or stop. In OBS (28 or later): Tools > WebSocket Server "
    "Settings > Enable WebSocket server (port 4455). With Enable Authentication on, its password (Show "
    "Connect Info) goes in the console: vr_obs_password \"...\" (kept in the config in plain text); or "
    "turn authentication off. OBS on another PC: vr_obs_host and vr_obs_port. Off: never looks.")
QVR_SLIDER_BYNAME(vr_window_smooth, "Smoothing", 0.f, 0.5f, 0.01f, "%.2f s", noExt,
    "How much the head's shake is smoothed out: the view's lag behind your head. 0.1-0.2 s takes out the "
    "shake and keeps turns lively; more glides like a camera operator's.")
QVR_SLIDER_BYNAME(vr_window_level, "Level Horizon", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "How much of your head's tilt to the side is taken out of the window's view: 1 keeps the horizon level.")
QVR_SLIDER_BYNAME(vr_window_zoom, "Zoom", 1.f, 2.f, 0.05f, "%.2fx", noExt,
    "How much of the eye's image the window shows: 1 the widest crop of the window's shape; higher crops "
    "closer and leaves a margin for the steadied view to turn in. Past the margin, the view follows your "
    "head: raise it if quick turns show the view catching up.")
QVR_TOGGLE_BYNAME(vr_spectator_preview, "Preview in the Menus",
    "While the Spectator Camera is on, a small copy of what the window shows, in the bottom left corner "
    "of the headset's menus, above its switch: what the recording looks like (on by default). Only in the "
    "headset: the recording itself shows the menu without it.")
QVR_TOGGLE_BYNAME(vr_spectator_bullettime_fx, "Bullet Time Effects",
    "Shows bullet time's drained colour, tint and darkened edges in the Spectator Camera (on by default). "
    "Off: the camera keeps its normal look. The headset is unchanged. Strength and colour: Combat > "
    "Bullet Time. The recording's Time Scale alone never applies this look.")
QVR_SLIDER_BYNAME(vr_spectator_fov, "Field of View", 60.f, 150.f, 1.f, "%.0f deg", ext(40.f, 160.f),
    "The window's horizontal field of view (90 as a flat game; the vertical one follows the window's shape).")
QVR_CYCLE_BYNAME(vr_spectator_rate, "Frame Rate",
    "How often the camera is drawn; the window shows its last image between. 60 fps (the default): as "
    "often as a 60 fps recording takes (OBS), a third of the cost saved at 90 Hz, half at 120 Hz. Every "
    "Frame: the headset's rate (smoother only if you record at more than 60 fps). Every 2nd or 3rd Frame: "
    "half or a third of the headset's rate (45 or 30 fps at 90 Hz), for a slower PC.",
    {1.f, "Every Frame"}, {2.f, "Every 2nd Frame"}, {3.f, "Every 3rd Frame"}, {60.f, "60 fps"})
QVR_CYCLE_BYNAME(vr_spectator_scale, "Resolution Scale",
    "Its resolution, times the window's (vr_spectator_scale; the cost goes with the pixels, the scale's "
    "square). 1x (the default): sharp at the window's size. Lower costs less: 0.75x draws 56% of the "
    "pixels, drawn up to the window smoothly (a recording's compression softens more than that); 0.5x a "
    "quarter. Above 1x supersamples: at a 1920x1080 window, 1.5x draws 2880x1620 (about 1.3 ms of the "
    "GPU's frame in a big fight on an RTX 4090, twice that on the frames it draws).",
    {0.5f, "0.5x (a quarter of the pixels)"}, {0.75f, "0.75x (56% of the pixels)"}, {1.f, "1x (the window's)"},
    {1.25f, "1.25x (1.6 times the pixels)"}, {1.5f, "1.5x (2.25 times the pixels)"}, {2.f, "2x (4 times the pixels)"})
QVR_TOGGLE_BYNAME(vr_spectator_skip_hidden, "Skip While the Window Is Minimized",
    "While the desktop window is minimized or hidden, the camera is not drawn at all (on by default): "
    "nothing to record, its whole cost saved; drawn again the moment the window shows. A window covered "
    "by others is still drawn (OBS's Window Capture records it). The menus' preview keeps it drawn while "
    "it shows.")
QVR_CYCLE_BYNAME(vr_spectator_aa, "Anti-Aliasing",
    "Window's MSAA (the default): the window's multisampling (Anti-Aliasing, vid_fsaa), as the eyes: "
    "smooth edges, which a recording shows (4x costs the camera about a tenth more; nothing without "
    "vid_fsaa). Off: none, the cheapest.",
    {0.f, "Off"}, {1.f, "Window's MSAA"})
QVR_SLIDER_BYNAME(vr_spectator_pos_smooth, "Position Smoothing", 0.f, 0.3f, 0.01f, "%.2f s", noExt,
    "How much your head's small movements are smoothed out (its lag). Walking, turning and teleporting are "
    "never smoothed.")
QVR_SLIDER_BYNAME(vr_timescale, "Time Scale", 0.1f, 1.f, 0.05f, "%.2fx", ext(0.05f, 4.f),
    "The game's speed, for footage to speed up in editing (0.25: record at a quarter speed, play it back 4x "
    "faster): monsters, missiles, physics, animations, effects and timers all slowed alike. Your view "
    "follows your head in real time; your hands move with the slowed world. Single player. Not saved. "
    "Bindable: bind a key to vr_slowmo (toggles to the scale below) or vr_slowmo 0.5.")
QVR_SLIDER_BYNAME(vr_slowmo_scale, "Toggle's Time Scale", 0.1f, 1.f, 0.05f, "%.2fx", ext(0.05f, 4.f),
    "The time scale the vr_slowmo command toggles to (and back to 1).")
QVR_SLIDER_BYNAME(vr_timescale_ramp, "Ease In and Out", 0.f, 2.f, 0.1f, "%.1f s", noExt,
    "Seconds the time scale takes to slide all the way from 1 to 0: a gradual slow-down and speed-up; 0 "
    "switches at once.")
QVR_TOGGLE_BYNAME(vr_timescale_sound, "Slow Sounds",
    "On: sounds play slowed and lower with the game, as a slowed recording sounds (sped up in editing, "
    "they're right again). Off: at their normal speed and pitch. Music is never slowed. For the cleanest "
    "sound sped up, use the Game-Time Sound File below.")
QVR_TOGGLE_BYNAME(vr_timescale_wav, "Game-Time Sound File",
    "For trailers: while Log Highlights runs, the game's sound is also mixed a second time as it sounds at "
    "normal speed, in the game's time (no music; what you hear doesn't change), to "
    "highlights/<log>_gametime.wav (24-bit), from the sync mark's beep (a new file at each Sync Mark Now). "
    "It lines up with a recording sped up by the time scale; use it instead of the slowed sound "
    "(docs/vr-port/TRAILER.md). About 16 MB a minute; a second spatial mix's CPU while it runs.")
QVR_SLIDER_BYNAME(vr_timescale_hand_speed, "Hand Speed Limit", 0.f, 20.f, 0.5f, "%.1f m/s", noExt,
    "In slow motion, your hands follow your controllers as fast as the slowed world allows: at most this "
    "fast in the game's time (a quarter of it in real time at 0.25x). A slow move is followed exactly; "
    "a real-speed swing lands as a hard blow, not a supersonic one. 0: no limit (the hands keep up).")
QVR_SLIDER_BYNAME(vr_timescale_hand_spin, "Hand Turn Limit", 0.f, 40.f, 1.f, "%.0f rad/s", noExt,
    "Likewise, the most your hands turn per second of the game's time in slow motion. 0: no limit.")
QVR_TOGGLE_BYNAME(vr_timescale_turn_realtime, "Turn in Real Time",
    "On: the stick's smooth turn keeps its real-time speed in slow motion (sped up in editing, it looks "
    "faster). Off: slowed with the world.")
QVR_TOGGLE_BYNAME(vr_timescale_move_realtime, "Move in Real Time",
    "On: your own movement (the stick, jumps, falls) and your weapons' timing keep their real-time speed "
    "in slow motion; your hands are still slowed. Off: slowed with the world.")
QVR_TOGGLE_BYNAME(vr_sandevistan, "Sandevistan: You at Full Speed",
    "On: in slow motion only the world slows: you move, turn, swing, shoot and reload at full speed, your "
    "hands follow your controllers as they are (Your Missiles at Full Speed Too: Combat > Bullet Time).")
QVR_TOGGLE_BYNAME(vr_highlights, "Log Highlights",
    "For trailers (docs/vr-port/TRAILER.md): logs the cool moments as they happen (gibs, multi-kills, parries, "
    "counters, bullet time, explosions and chains, shot grenades, stuck axes, hook swings...), each scored, "
    "in real time (right in slow motion). Turned on: a sync mark (a white flash over the window, never the "
    "headset, and a beep) to line the log up with the recording: start recording first. Turned off: the "
    "log's DaVinci Resolve markers are written. Files: quakevr/highlights/. Bindable: vr_highlights_start, "
    "vr_highlights_stop. Not saved.")
QVR_SLIDER_BYNAME(vr_highlights_flash, "Sync Flash", 0.f, 1.f, 0.05f, "%.2f s", noExt,
    "How long the sync mark's flash lasts over the window (0: none).")
QVR_SLIDER_BYNAME(vr_highlights_beep, "Sync Beep", 0.f, 1.f, 0.1f, "%.1f", noExt,
    "The sync mark's beep's volume (0: none): a 1 kHz tone the trailer script can find in the recording.")
QVR_CYCLE_BYNAME(vr_highlights_fps, "Markers' Frame Rate",
    "The marker file's (EDL) frame rate: the recording's and the Resolve timeline's.",
    {24.f, "24 fps"}, {25.f, "25 fps"}, {30.f, "30 fps"}, {50.f, "50 fps"}, {60.f, "60 fps"}, {120.f, "120 fps"})
QVR_SLIDER_BYNAME(vr_highlights_min_score, "Least Score for a Marker", 0.f, 10.f, 0.5f, "%.1f", noExt,
    "Moments scoring less are left out of the marker file (the CSV and JSON keep them all): 2 leaves out "
    "plain kills.")
QVR_SLIDER_BYNAME(vr_highlights_multikill, "Multi-Kill Window", 0.5f, 10.f, 0.5f, "%.1f s", noExt,
    "Kills at most this far apart (game seconds) make one multi-kill.")

// vr_menu_recording.inc: pageBulletTime
QVR_TOGGLE_BYNAME(vr_bullettime_enabled, "Bullet Time",
    "Tap your gadget's screen hard with your other hand (or the butt of the gun it holds): the world "
    "slows for as long as the meter on the gadget's screen (TIME) lasts; tap again to stop. Then a "
    "cooldown; the meter fills as you hurt enemies (or over time: Meter Refill). Bindable: vr_bullettime. "
    "Single player.")
QVR_CYCLE_BYNAME(vr_bullettime_trigger, "Trigger",
    "What starts and stops it. Gadget: a hard tap on its screen (its settings below). A stick press: that "
    "press does only this in the game (never its bound key: by default the left's is Run, the right's "
    "Reload), and the screen tap does nothing.",
    {0.f, "Gadget (Screen Tap)"}, {1.f, "Left Stick Press"}, {2.f, "Right Stick Press"})
QVR_SLIDER_BYNAME(vr_bullettime_scale, "Time Scale", 0.1f, 1.f, 0.05f, "%.2fx", ext(0.05f, 1.f),
    "How fast the world runs while it lasts (0.3: under a third of its speed).")
QVR_SLIDER_BYNAME(vr_bullettime_duration, "Duration", 1.f, 20.f, 0.5f, "%.1f s", ext(0.5f, 120.f), "Real seconds a full meter lasts.")
QVR_SLIDER_BYNAME(vr_bullettime_earn_damage, "Damage to Fill", 30.f, 600.f, 10.f, "%.0f", ext(1.f, 5000.f),
    "Earned by Damage: the damage dealt that fills the meter from empty on Normal (350: about twelve "
    "grunts, 30 health each); the other skills scale it (below). Less: it fills faster.")
QVR_SLIDER_BYNAME(vr_bullettime_earn_skill0, "Easy: Damage Needed", 0.25f, 2.f, 0.05f, "%.2fx", ext(0.05f, 5.f),
    "Earned by Damage, on Easy (skill 0): times Damage to Fill (0.75 shipped: it fills sooner).")
QVR_SLIDER_BYNAME(vr_bullettime_earn_skill1, "Normal: Damage Needed", 0.25f, 2.f, 0.05f, "%.2fx", ext(0.05f, 5.f),
    "Earned by Damage, on Normal (skill 1): times Damage to Fill (1 shipped).")
QVR_SLIDER_BYNAME(vr_bullettime_earn_skill2, "Hard: Damage Needed", 0.25f, 2.f, 0.05f, "%.2fx", ext(0.05f, 5.f),
    "Earned by Damage, on Hard (skill 2): times Damage to Fill (1.2 shipped: it fills later).")
QVR_SLIDER_BYNAME(vr_bullettime_earn_skill3, "Nightmare: Damage Needed", 0.25f, 2.f, 0.05f, "%.2fx", ext(0.05f, 5.f),
    "Earned by Damage, on Nightmare (skill 3): times Damage to Fill (1.5 shipped).")
QVR_SLIDER_BYNAME(vr_bullettime_earn_active, "Refill While Slowed", 0.f, 1.f, 0.05f, "%.2fx", noExt,
    "Earned by Damage: damage dealt while bullet time runs fills the meter at this share (low: no endless "
    "slow motion; 0: not at all).")
QVR_SLIDER_BYNAME(vr_bullettime_recharge, "Recharge Time", 2.f, 120.f, 1.f, "%.0f s", ext(0.5f, 600.f),
    "Over Time: real seconds the meter takes to fill from empty.")
QVR_SLIDER_BYNAME(vr_bullettime_cooldown, "Cooldown", 0.f, 10.f, 0.5f, "%.1f s", ext(0.f, 60.f),
    "Real seconds after it ends (or is stopped) before it can start again (over time, the meter waits "
    "too).")
QVR_TOGGLE_BYNAME(vr_bullettime_sandevistan, "Sandevistan: You at Full Speed",
    "On: only the world slows; you move, turn, swing, throw, shoot and reload at full speed. Off: you are "
    "slowed with the world but your view (your hands follow your controllers as the slowed world allows).")
QVR_TOGGLE_BYNAME(vr_sandevistan_missiles, "Your Missiles at Full Speed Too",
    "In Sandevistan: your rockets, grenades, nails, thrown weapons and the hook fly at full speed. Off: they "
    "join the slowed world as they leave you. Props you throw always join it.")
QVR_SLIDER_BYNAME(vr_bullettime_fx, "Look's Strength", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "While it runs, the view drained of colour, tinted and darkened at its edges: this much (0 off). Never "
    "with the recording's Time Scale (Graphics > Recording).")
QVR_TOGGLE_BYNAME(vr_bullettime_tap, "Tap the Screen",
    "Strike the gadget's screen hard and straight with your other hand, or with the butt of the gun it "
    "holds. It must come straight into the screen fast enough and stop on it: a swing across it, a "
    "soft touch or a hand brushing past does nothing.")
QVR_SLIDER_BYNAME(vr_bullettime_tap_calm_speed, "Calm Hands Speed", 1.f, 5.f, 0.25f, "%.2f m/s", ext(0.f, 10.f),
    "Taps don't count for 0.3 s after the gadget's hand moves faster than this, or the tapping hand "
    "sweeps across the screen this fast (blows and shoves take 3 m/s or more; a tap's own motion into the "
    "screen is never counted). Lower: calmer hands only. 0: off.")
QVR_TOGGLE_BYNAME(vr_bullettime_tap_both_held, "Not While Both Hands Hold",
    "Taps don't count while both your hands hold an enemy or a body (fighting, pushing, shoving one): "
    "the gadget knocked in the scuffle.")
QVR_TOGGLE_BYNAME(vr_bullettime_tap_butt, "The Gun's Butt Taps Too",
    "On: the butt of the gun in your other hand taps the screen as your hand does. Off: only your hand.")
QVR_TOGGLE_BYNAME(vr_bullettime_tap_holding, "Tap While Holding",
    "On: the tapping hand may hold a weapon or a prop. Off: only an empty hand taps.")
QVR_TOGGLE_BYNAME(vr_bullettime_tap_twohanded, "Tap During Two-Handed Holds",
    "Off: no taps while both hands hold one weapon (your gadget hand on its foregrip), against accidents. "
    "Moving together, the hands never tap anyway.")
QVR_SLIDER_BYNAME(vr_bullettime_trigger_cooldown, "Ignore Repeats For", 0.f, 2.f, 0.1f, "%.1f s", noExt,
    "After a tap counts, how long before the next one does (a bounce is one).")
QVR_SLIDER_BYNAME(vr_bullettime_tap_sound, "Tap Click Volume", 0.f, 1.f, 0.05f, "%.2f", noExt,
    "The gadget clicks from its screen as a tap registers: a short tick for a double tap's first, two "
    "rising blips for the tap that starts or stops bullet time (0: silent).")
QVR_CYCLE_BYNAME(vr_bullettime_trails, "Distortion Trails",
    "As in F.E.A.R.: a wake behind each bullet, nail, rocket, grenade and monster's shot bends the view "
    "behind it like a rippling glass rod, fading along its length. In Bullet Time: while it runs (and "
    "fading out after). Always: in normal time too. Off: no cost.",
    {0.f, "Off"}, {1.f, "In Bullet Time"}, {2.f, "Always"})
QVR_TOGGLE_BYNAME(vr_bullettime_trails_hitscan, "Behind Your Bullets",
    "The shotguns' pellets and the rifles' bullets (whether their tracer is drawn or not).")
QVR_TOGGLE_BYNAME(vr_bullettime_trails_nails, "Behind Nails", nullptr)
QVR_TOGGLE_BYNAME(vr_bullettime_trails_explosives, "Behind Rockets and Grenades", nullptr)
QVR_TOGGLE_BYNAME(vr_bullettime_trails_enemy, "Behind Monsters' Shots",
    "Enforcers' lasers, scrags' spit, knights' flames, vores' balls, zombies' flesh and grunts' bullets.")
