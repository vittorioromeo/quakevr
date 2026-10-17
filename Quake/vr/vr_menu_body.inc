// vr_menu_body.inc -- the body pages: Body, Crouch, Arms, Body Calibration, the Flashlight pages. Part of vr_menu.cpp
// (included where it was: the same translation unit, so its anonymous namespace).

// Whether body tracking data comes in now (Body > Body Tracking, Body Calibration).
const char* bodytrackAvailability(int)
{
    return bodytrack::availabilityLine();
}

[[nodiscard]] za::Vector<Item> pageBody()
{
    return {
        open("Arms and Pauldrons", pageIndex(pageBodyArms)),
        open("Body Calibration", pageIndex(pageBodyCalibration)),
        header("Body"),
        open("Body: Body and Display", pageIndex(pageBodyDisplay)).help("The body on or off is on Body and Display."),
        open("Build, Skin, Bracers, Gloves: Player Appearance", pageIndex(pageVrsAppearance))
            .help("How you look: the body's build, the skin tone, the bracers, gloves and their colour, the pauldrons (VR "
                  "Settings > Player Appearance)."),
        toggle("Walking Legs", vr_body_walk).help("The legs (full body) walk as you move with the stick."),
        slider("Step Rate", vr_body_step_rate, 1.f, 5.f, 0.1f, "%.1f /s").extend()
            .help("How fast the legs step at most, in steps a second at full running speed (walking, somewhat fewer)."),
        slider("Turn Before Stepping", vr_body_turn_step, 15.f, 90.f, 5.f, "%.0f deg").extend()
            .help("How far you turn over your planted feet before they step round to follow."),
        slider("Wading Heaviness", vr_body_wade, 0.f, 2.f, 0.1f, "%.1fx").extend()
            .help("Wading, the legs walk heavier: shorter, higher, slower steps (0: as on land)."),
        slider("Swimming Kicks", vr_body_swim_kick, 0.f, 2.f, 0.1f, "%.1fx").extend()
            .help("Swimming, the legs trail behind and kick where the stick moves you: how wide (0: no kicks)."),
        slider("Swimming Kick Rate", vr_body_swim_kick_rate, 0.f, 4.f, 0.1f, "+%.1f /s").extend()
            .help("How many more kicks a second at full stick (treading water, about 0.7)."),
        toggle("Climbing Footholds", vr_body_climb_feet)
            .help("Climbing, the feet find footholds below you (ledges, rungs, the wall) and step from one to the next as "
                  "you climb; with none, they hang. Off: they hang as in the air."),
        slider("Foothold Reach", vr_body_climb_feet_reach, 0.5f, 1.2f, 0.05f, "%.2fx").extend()
            .help("How far below the hips the feet look for a foothold (1: down to nearly a straight leg)."),
        slider("Foothold Step Speed", vr_body_climb_feet_speed, 0.3f, 3.f, 0.1f, "%.1fx").extend()
            .help("How fast a foot steps to a new foothold (1: about a third of a second)."),
        toggle("Show Armour and Wounds", vr_body_state)
            .help("The armour you wear plates your torso; your arms and hands get bloodier as you are hurt (with Dynamic Wounds, on the Gore page: where you are hit)."),
        open("Wounds Drip Blood: Gore", pageIndex(pageGore)).help("The wounds' dripping blood is on Gore (Arm Drip Rate)."),
        toggle("Show Powerups", vr_body_powerups)
            .help("Quad damage glows blue or sparks around your hands, the pentagram glows red, the biosuit green, the ring fades you."),
        cycle("Quad Damage Look", vr_quad_look, {{1.f, "Blue Glow"}, {0.f, "Lightning Arcs"}, {2.f, "Both"}})
            .help("Quad damage on you: a soft pulsing blue glow round your hands, arms and body, lighting the world round you "
                  "blue; or Quake's electric arcs crackling over your hands and forearms (as when lightning strikes you); or both."),
        slider("Quad Glow", vr_quad_glow, 0.f, 2.f, 0.1f, "%.1fx").help("How strongly the blue glow shows on your hands, arms and body."),
        slider("Quad Glow Light", vr_quad_glow_light, 0.f, 2.f, 0.1f, "%.1fx").help("How much blue light your hands and body cast on the world round you (0: none)."),
        slider("Pentagram Glow", vr_pent_glow, 0.f, 2.f, 0.1f, "%.1fx")
            .help("The pentagram of protection on you: a pulsing red glow on your hands, arms and body (0: Quake VR's old red tint instead). With more than one power-up, the glows take turns."),
        slider("Pentagram Glow Light", vr_pent_glow_light, 0.f, 2.f, 0.1f, "%.1fx").help("How much red light your hands cast on the world round you (0: none)."),
        slider("Biosuit Glow", vr_suit_glow, 0.f, 2.f, 0.1f, "%.1fx")
            .help("The biosuit on you: a pulsing green glow on your hands, arms and body (0: Quake VR's old green tint instead). With more than one power-up, the glows take turns."),
        slider("Biosuit Glow Light", vr_suit_glow_light, 0.f, 2.f, 0.1f, "%.1fx").help("How much green light your hands cast on the world round you (0: none)."),
        toggle("Anchors Follow Body", vr_body_anchors)
            .help("Holsters, the virtual stock and hand collisions follow the body's lean and crouch."),
        slider("Hip Holsters Follow Legs", vr_holster_leg_follow, 0.f, 1.f, 0.05f, "%.2f")
            .help("Full body: the hip holsters move with the walking and kicking legs (0: fixed on the body, 1: all the way)."),
        header("Body Collisions"),
        toggle("Body Collisions", vr_body_collide)
            .help("Your hands and the weapons in them stop at your other hand, your other arm, the wrist gadget and your "
                  "body instead of passing through them. Push on and they pass through (Pass Through At). Drawn only: "
                  "hits, shots, aim and grabs are still where your hands are."),
        slider("Pass Through At", vr_body_collide_pass, 0.3f, 1.f, 0.05f, "%.2f").extend(0.05f, 1.f)
            .help("How far through what stops it your hand or weapon must be pushed before it lets go and passes "
                  "through: 0.7, seven tenths of the way (and held out at most 10.5 cm); 1, only once all the way through. It "
                  "stops again once clear."),
        toggle("Elbows Out of the Torso", vr_body_collide_elbows)
            .help("With Body Collisions: an elbow that would go into your torso (a hand across your chest) swings out "
                  "round the line from the shoulder to the wrist."),
        // Body tracking (vr_bodytrack.cpp): Quest 3 through Virtual Desktop (XR_FB_body_tracking).
        header("Body Tracking"),
        infoLine(bodytrackAvailability, 0),
        toggle("Body Tracking", vr_bodytrack)
            .help("Where the headset tracks your body (Quest 3 through Virtual Desktop: allow Movement Tracking), your "
                  "tracked torso, elbows and (if you like) legs are blended into the body; where it is lost or unsure, "
                  "the guessed body comes back smoothly. Your hands always stay on the controllers."),
        slider("Torso", vr_bodytrack_torso, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much of your tracked torso: its lean, the chest's twist, the hips' facing, and telling a lean from "
                  "a step in the room."),
        slider("Elbows", vr_bodytrack_arms, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the elbows swing round to where your tracked elbows are."),
        slider("Elbow Fit", vr_bodytrack_arm_fit, 0.f, 1.f, 0.05f, "%.2f")
            .help("How far the elbows go onto your tracked elbows themselves, not only round to their side: the shoulders "
                  "follow your tracked shoulders and the arm's bones lengthen or shorten a little to put the elbow there. "
                  "0: the swing only (the arm keeps its own shape)."),
        slider("Legs", vr_bodytrack_legs, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much of the tracked legs (Quest 3 guesses them from your upper body: generated legs). Climbing "
                  "and swimming feet keep theirs."),
        toggle("Walk Over Tracked Legs", vr_bodytrack_legs_walk)
            .help("With tracked legs: while you move (walking, running, in the air) the walk and run animation drives the "
                  "legs over the tracked ones; standing still, the tracked legs show. Off: the tracked legs always."),
        slider("Min Confidence", vr_bodytrack_confidence, 0.f, 1.f, 0.05f, "%.2f")
            .help("Below this confidence the tracking eases out to the guessed body."),
        slider("Smoothing", vr_bodytrack_smooth, 0.f, 0.2f, 0.01f, "%.2f s").help("How long the tracked joints are smoothed over."),
        header("Torso Direction"),
        cycle("Torso Follows", vr_torso_mode, {{1.f, "Mostly the head"}, {0.f, "The hands (old)"}})
            .help("Nothing tracks your torso: it is guessed. Mostly the head: it faces where your head has been facing, "
                  "the hands pulling it a little, only those in front of you (not one behind your back or far off to a "
                  "side); old: four fifths of the way towards the hands, wherever they are."),
        slider("Head Weight", vr_torso_head, 0.f, 5.f, 0.1f, "%.1f")
            .help("The torso faces a weighted mean of directions. This one: where your head faces now."),
        slider("Head History Weight", vr_torso_head_history, 0.f, 5.f, 0.1f, "%.1f")
            .help("Where your head has faced lately: higher, a glance turns the torso less (and a turn follows later)."),
        slider("Head History Span", vr_torso_head_lag, 0.f, 2.f, 0.05f, "%.2f s").extend(0.f, 5.f)
            .help("How long \"lately\" is."),
        slider("Both Hands Weight", vr_torso_hands, 0.f, 10.f, 0.1f, "%.1f")
            .help("The middle of your hands, when both are in front of you (holding a gun out, reaching for something)."),
        slider("One Hand Weight", vr_torso_one_hand, 0.f, 5.f, 0.1f, "%.1f")
            .help("One hand alone in front, the other behind your back or hanging down."),
        slider("Hands Down Weight", vr_torso_hands_down, 0.f, 5.f, 0.1f, "%.1f")
            .help("Both hands hanging by your sides: square to the line between them (they turn with your body, not with "
                  "a glance)."),
        slider("Side Reach", vr_torso_side_angle, 10.f, 90.f, 5.f, "%.0f deg")
            .help("A hand further off to a side than this pulls less, and not at all 35 degrees further."),
        slider("Turn Deadzone", vr_torso_deadzone, 0.f, 30.f, 1.f, "%.0f deg")
            .help("How far off the guess must be before the torso turns (then it turns all the way)."),
        slider("Turn Speed", vr_torso_speed, 0.f, 30.f, 1.f, "%.0f").extend(0.f, 100.f)
            .help("How quickly it turns then (0: at once)."),
        slider("Neck Turn", vr_torso_neck_max, 30.f, 120.f, 5.f, "%.0f deg")
            .help("How far your head turns from the torso at most: past it the torso turns with the head."),
        header("Placement"),
        slider("Torso Offset", vr_body_torso_back, -0.2f, 0.4f, 0.01f, "%.2f m").extend(-1.f, 1.f)
            .help("How far the torso sits behind your neck (negative: in front)."),
        slider("Legs Offset", vr_body_legs_back, -0.2f, 0.4f, 0.01f, "%.2f m").extend(-1.f, 1.f)
            .help("How far the feet stand behind your head (negative: in front)."),
        slider("Shoulders Offset", vr_body_shoulders_back, -0.15f, 0.2f, 0.01f, "%.2f m").extend(-0.5f, 0.5f)
            .help("How far the shoulders sit behind your neck (negative: in front)."),
        slider("Eyes Forward", vr_body_eye_forward, 0.f, 0.25f, 0.01f, "%.2f m").extend(-0.1f, 0.5f)
            .help("From the top of the neck to the eyes, forward."),
        slider("Eyes Up", vr_body_eye_up, 0.f, 0.25f, 0.01f, "%.2f m").extend(-0.1f, 0.5f).help("From the top of the neck to the eyes, up."),
        slider("Crouch Tilt", vr_body_crouch_tilt, 0.f, 80.f, 5.f, "%.0f deg").extend()
            .help("How far the back tilts forward in a full crouch (the hips stay under you)."),
        header("Crouched Pose"),
        open("Crouched Pose Offsets", pageIndex(pageBodyCrouch))
            .help("Where the torso, pelvis, shoulders, elbows and neck sit crouched, and the holsters and the ammo pouch: "
                  "offsets at a full crouch, blended in as you crouch."),
        slider("Crouched Pose Strength", vr_body_crouch_pose_strength, 0.f, 1.f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("Times the crouched pose's offsets (0: none, the body crouches as before; 1: as set)."),
        slider("Crouched Pose Curve", vr_body_crouch_pose_curve, 0.25f, 3.f, 0.05f, "%.2f").extend(0.1f, 10.f)
            .help("How the offsets come in as you crouch: 1 evenly (half a crouch, half of them); over 1 little until you "
                  "are deep; under 1 most of them early.")
            .advanced(),
        cycle("Preview Crouch", vr_body_crouch_preview,
            {{-1.f, "Off"}, {0.f, "Standing"}, {0.25f, "Quarter"}, {0.5f, "Half"}, {0.75f, "Three Quarters"}, {1.f, "Full"}})
            .help("The crouched pose blended as if you crouched this deep, standing or not (the crouch itself as you are): "
                  "to tune it with the body in front of you (Debug: Show Body Skeleton) or looking down. Off: as deep as "
                  "you crouch. Not saved.")
            .advanced(),
        header("Death View"),
        cycle("Death View", vr_death_view, {{0.f, "Off"}, {1.f, "Third Person"}, {2.f, "Immersive"}})
            .help("When you die (not gibbed) your body falls as a ragdoll (also on VR Settings, Comfort). Off: no body. "
                  "Third Person: your view stays where your eyes were. Immersive: your view goes into your body's head "
                  "(Third Person while a menu is open)."),
        toggle("Immersive: Turn With the Body", vr_death_view_turn)
            .help("Immersive Death View: your view turns sideways as your body's head does, slowly (Turn Speed). Off: "
                  "only its place follows the head; you turn yourself."),
        slider("Immersive: Turn Speed", vr_death_view_turn_speed, 10.f, 180.f, 10.f, "%.0f deg/s")
            .help("The fastest the Immersive Death View turns with your body's head."),
        slider("Immersive: Smoothing", vr_death_view_smooth, 0.f, 0.5f, 0.05f, "%.2f s")
            .help("How long the Immersive Death View takes to follow your body's head: more is gentler, less is closer."),
        slider("Immersive: Fade", vr_death_view_fade, 0.f, 2.f, 0.1f, "%.1f s")
            .help("The view fades in from black as it goes into your body's head, and as you respawn. 0: no fade."),
        slider("Immersive: Out for Menus", vr_death_view_menu_time, 0.f, 1.f, 0.05f, "%.2f s")
            .help("Opening a menu (or the console) while dead in the Immersive Death View moves the view out to Third "
                  "Person's place over this long, and back into your body's head as it closes (0: at once). The setting "
                  "stays Immersive."),
        slider("Your Body: Killing Blow's Push", vr_death_ragdoll_push, 0.f, 3.f, 0.25f, "%.2fx")
            .help("Your ragdoll body: the blow that killed you throws it this much of what Quake throws the player (0: "
                  "only the motion you had), at most about 10 m/s."),
    };
}

[[nodiscard]] za::Vector<Item> pageBodyCalibration();

// Body > Crouched Pose Offsets (vr_body_crouch_*, vr_*_holster_crouch_*, vr_ammo_pouch_crouch_*; ROUND21.md, "Crouched
// pose"): everything at a full crouch, blended by the crouch's depth (Strength and Curve, also on the Body page).
[[nodiscard]] Item crouchMetres(const char* label, cvar_t& c, const char* help)
{
    return slider(label, c, -0.2f, 0.2f, 0.005f, "%+.3f m").extend(-0.6f, 0.6f).help(help);
}

[[nodiscard]] Item crouchDegrees(const char* label, cvar_t& c, const char* help)
{
    return slider(label, c, -45.f, 45.f, 1.f, "%+.0f deg").extend(-180.f, 180.f).help(help);
}

[[nodiscard]] Item crouchUnits(const char* label, cvar_t& c, const char* help)
{
    return slider(label, c, -10.f, 10.f, 0.25f, "%+.2f").extend(-30.f, 30.f).help(help);
}

[[nodiscard]] za::Vector<Item> pageBodyCrouch()
{
    return {
        cycle("Preview Crouch", vr_body_crouch_preview,
            {{-1.f, "Off"}, {0.f, "Standing"}, {0.25f, "Quarter"}, {0.5f, "Half"}, {0.75f, "Three Quarters"}, {1.f, "Full"}})
            .help("The offsets below blended as if you crouched this deep, standing or not (the crouch itself as you are), "
                  "to tune them looking down or with the body in front of you (Debug: Show Body Skeleton). Off: as deep "
                  "as you crouch. Not saved."),
        slider("Strength", vr_body_crouch_pose_strength, 0.f, 1.f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("Times all of these (0: none, the body crouches as before)."),
        slider("Curve", vr_body_crouch_pose_curve, 0.25f, 3.f, 0.05f, "%.2f").extend(0.1f, 10.f)
            .help("How they come in as you crouch: 1 evenly (half a crouch, half); over 1 little until deep; under 1 most "
                  "early."),
        header("Torso (pelvis, spine and chest)"),
        crouchMetres("Torso Back", vr_body_crouch_torso_back, "Crouched, the whole trunk further back (negative: "
            "forward); the holsters and the ammo pouch on it go with it."),
        crouchMetres("Torso Up", vr_body_crouch_torso_up, "Crouched, the whole trunk higher (negative: lower)."),
        crouchMetres("Torso Right", vr_body_crouch_torso_right, "Crouched, the whole trunk to the right (negative: left)."),
        crouchDegrees("Torso Pitch", vr_body_crouch_torso_pitch, "Crouched, the spine and chest lean further forward "
            "(negative: back), about the base of the spine (on top of Crouch Tilt)."),
        crouchDegrees("Torso Yaw", vr_body_crouch_torso_yaw, "Crouched, the spine and chest turn to the left (negative: "
            "right)."),
        crouchDegrees("Torso Roll", vr_body_crouch_torso_roll, "Crouched, the spine and chest tip their top to the right "
            "(negative: left)."),
        header("Pelvis"),
        crouchMetres("Pelvis Back", vr_body_crouch_pelvis_back, "Crouched, the hips further back (negative: forward), "
            "on top of the torso's; the hip holsters and the ammo pouch go with them."),
        crouchMetres("Pelvis Up", vr_body_crouch_pelvis_up, "Crouched, the hips higher (negative: lower)."),
        crouchDegrees("Pelvis Pitch", vr_body_crouch_pelvis_pitch, "Crouched, the hips tip forward (negative: back)."),
        header("Shoulders"),
        crouchMetres("Shoulders Back", vr_body_crouch_shoulders_back, "Crouched, the shoulder joints further back "
            "(negative: forward)."),
        crouchMetres("Shoulders Up", vr_body_crouch_shoulders_up, "Crouched, the shoulder joints higher (negative: "
            "lower)."),
        crouchMetres("Shoulders Out", vr_body_crouch_shoulders_out, "Crouched, each shoulder joint further out "
            "(negative: in)."),
        crouchDegrees("Shoulders Swing Back", vr_body_crouch_shoulders_swing, "Crouched, the collarbones swing back "
            "(negative: forward) about the base of the neck."),
        crouchDegrees("Shoulders Shrug", vr_body_crouch_shoulders_shrug, "Crouched, the collarbones swing up (negative: "
            "down) about the base of the neck."),
        header("Elbows"),
        slider("Elbows Out", vr_body_crouch_elbow_out, -1.f, 1.f, 0.05f, "%+.2f").extend(-3.f, 3.f)
            .help("Crouched, the elbows point further out (negative: in): added to Arms and Pauldrons' Elbow Out."),
        slider("Elbows Back", vr_body_crouch_elbow_back, -1.f, 1.f, 0.05f, "%+.2f").extend(-3.f, 3.f)
            .help("Crouched, the elbows point further back (negative: forward): added to Elbow Back."),
        header("Neck"),
        crouchMetres("Eyes Forward", vr_body_crouch_eye_forward, "Crouched, the eyes further in front of the top of the "
            "neck (the neck further back; negative: forward): added to Eyes Forward."),
        crouchMetres("Eyes Up", vr_body_crouch_eye_up, "Crouched, the eyes further over the top of the neck (the neck "
            "lower; negative: higher): added to Eyes Up."),
        header("Hip Holsters (crouched)"),
        crouchUnits("Hips Forward", vr_hip_holster_crouch_x, "Crouched, both hip holsters further forward (negative: "
            "back), units, in the way the body faces."),
        crouchUnits("Hips Out", vr_hip_holster_crouch_y, "Crouched, each hip holster further out (negative: in), units."),
        crouchUnits("Hips Up", vr_hip_holster_crouch_z, "Crouched, the hip holsters higher (negative: lower), units."),
        crouchDegrees("Hips Pitch", vr_hip_holster_crouch_pitch, "Crouched, the hip holsters' top tips further from the "
            "body (as the Hotspots' Hip Pitch)."),
        crouchDegrees("Hips Yaw", vr_hip_holster_crouch_yaw, "Crouched, their face turns further outwards."),
        crouchDegrees("Hips Roll", vr_hip_holster_crouch_roll, "Crouched, their top tips further outwards."),
        header("Chest Holsters (crouched)"),
        crouchUnits("Chest Forward", vr_upper_holster_crouch_x, "Crouched, both chest holsters further forward "
            "(negative: back), units."),
        crouchUnits("Chest Out", vr_upper_holster_crouch_y, "Crouched, each chest holster further out (negative: in), "
            "units."),
        crouchUnits("Chest Up", vr_upper_holster_crouch_z, "Crouched, the chest holsters higher (negative: lower), "
            "units."),
        crouchDegrees("Chest Pitch", vr_upper_holster_crouch_pitch, "Crouched, their top tips further from the body."),
        crouchDegrees("Chest Yaw", vr_upper_holster_crouch_yaw, "Crouched, their face turns further outwards."),
        crouchDegrees("Chest Roll", vr_upper_holster_crouch_roll, "Crouched, their top tips further outwards."),
        header("Shoulder Holsters (crouched)"),
        crouchUnits("Back Forward", vr_shoulder_holster_crouch_x, "Crouched, both shoulder (back) holsters further "
            "forward (negative: back), units."),
        crouchUnits("Back Out", vr_shoulder_holster_crouch_y, "Crouched, each shoulder holster further out (negative: "
            "in), units."),
        crouchUnits("Back Up", vr_shoulder_holster_crouch_z, "Crouched, the shoulder holsters higher (negative: lower), "
            "units."),
        crouchDegrees("Back Pitch", vr_shoulder_holster_crouch_pitch, "Crouched, the guns on the back tip further."),
        crouchDegrees("Back Yaw", vr_shoulder_holster_crouch_yaw, "Crouched, they turn further outwards."),
        crouchDegrees("Back Roll", vr_shoulder_holster_crouch_roll, "Crouched, their top tips further outwards."),
        header("Ammo Pouch (crouched)"),
        crouchUnits("Pouch Forward", vr_ammo_pouch_crouch_x, "Crouched, the ammo pouch further forward (negative: "
            "back), units."),
        crouchUnits("Pouch Right", vr_ammo_pouch_crouch_y, "Crouched, the ammo pouch further right (negative: left), "
            "units."),
        crouchUnits("Pouch Up", vr_ammo_pouch_crouch_z, "Crouched, the ammo pouch higher (negative: lower), units."),
        crouchDegrees("Pouch Pitch", vr_ammo_pouch_crouch_pitch, "Crouched, its top tips further from the body."),
        crouchDegrees("Pouch Yaw", vr_ammo_pouch_crouch_yaw, "Crouched, it turns further."),
        crouchDegrees("Pouch Roll", vr_ammo_pouch_crouch_roll, "Crouched, its top tips further sideways."),
    };
}

// Split from Body: the arms' reach and bend, the shoulders, and the pauldrons over them. Body Calibration measures the
// arms' lengths and the shoulders (vr_bodycal_*); the sliders here are tweaks on top (vr_body_tweak_*, 0: as measured;
// uncalibrated, on the default body), and the settings it doesn't measure. Built again when calibrated or not
// (armsPageCalibrated): Arm Length is only for the default body's arms.
int armsPageCalibrated = -1;

const char* armsMeasured(int i)
{
    const char* line = bodycal::measuredLine(i);
    return line ? line : "";
}

const char* armLengthUnused()
{
    return "Arm Length: not used (arms measured)";
}

void armsResetTweaks()
{
    bodycal::resetTweaks();
}

[[nodiscard]] za::Vector<Item> pageBodyArms()
{
    armsPageCalibrated = bodycal::calibrated() ? 1 : 0;
    za::Vector<Item> list{open("Body Calibration", pageIndex(pageBodyCalibration))};
    for(int i = 0; bodycal::measuredLine(i); i++)
    {
        list.pushBack(infoLine(armsMeasured, i));
    }
    list.pushBackMultiple(
        header("Arms"),
        slider("Upper Arm", vr_body_tweak_upper_arm, -10.f, 10.f, 0.5f, "%+.1f cm").extend(-30.f, 30.f)
            .help("Longer (negative: shorter) than measured, real cm: the shoulder joint to the elbow. 0: as Body "
                  "Calibration measured it (uncalibrated: the default body's arm, times Arm Length)."),
        slider("Forearm", vr_body_tweak_forearm, -10.f, 10.f, 0.5f, "%+.1f cm").extend(-30.f, 30.f)
            .help("Longer (negative: shorter) than measured, real cm: the elbow to the drawn hand's wrist. 0: as Body "
                  "Calibration measured it (uncalibrated: the default body's forearm, times Arm Length).")
    );
    if(armsPageCalibrated)
    {
        list.pushBack(info(armLengthUnused));
    }
    else
    {
        list.pushBack(slider("Arm Length", vr_body_arm_length, 0.7f, 1.4f, 0.01f, "%.2fx").extend(0.5f, 2.f)
                .help("Uncalibrated: the default body's arms (for your height), times this. Once Body Calibration "
                      "has measured yours, it isn't used."));
    }
    list.pushBackMultiple(
        slider("Arm Stretch", vr_body_arm_stretch, 1.f, 1.5f, 0.05f, "%.2fx").extend(1.f, 3.f)
            .help("How far the drawn arms may stretch to reach a hand beyond them (1: not at all). Calibrated, only past "
                  "your measured reach and the Shoulder Reach (tracking glitches, a lunge); uncalibrated, before the "
                  "shoulders reach."),
        slider("Shoulder Reach", vr_body_shoulder_reach, 0.f, 0.25f, 0.01f, "%.2f m").extend(0.f, 0.6f)
            .help("How far the shoulders may move out towards a hand the arm can't reach, metres (not measured)."),
        header("Shoulders"),
        slider("Shoulders Back", vr_body_tweak_shoulders_back, -0.1f, 0.1f, 0.005f, "%+.3f m").extend(-0.5f, 0.5f)
            .help("The shoulder joints further back (negative: forward) than measured, metres. 0: as Body Calibration "
                  "measured them (uncalibrated: the default body's)."),
        slider("Shoulders Higher", vr_body_tweak_shoulders_up, -0.1f, 0.1f, 0.005f, "%+.3f m").extend(-0.5f, 0.5f)
            .help("The shoulder joints higher (negative: lower) than measured, metres. 0: as measured (uncalibrated: "
                  "the default body's)."),
        slider("Shoulders Wider", vr_body_tweak_shoulders_out, -0.1f, 0.1f, 0.005f, "%+.3f m").extend(-0.3f, 0.3f)
            .help("Each shoulder joint further out (negative: in) than measured, metres. 0: as measured "
                  "(uncalibrated: the default body's)."),
        slider("Shoulder Rise", vr_body_tweak_shoulder_rise, -30.f, 30.f, 1.f, "%+.0f deg").extend(-90.f, 90.f)
            .help("Degrees more (negative: fewer) the shoulders rise as you reach up than measured. 0: as measured "
                  "(uncalibrated: the default body's 25)."),
        slider("Shoulder Swing", vr_body_tweak_shoulder_swing, -30.f, 30.f, 1.f, "%+.0f deg").extend(-90.f, 90.f)
            .help("Degrees more (negative: fewer) the shoulders swing forward as you reach far forward than measured. "
                  "0: as measured (uncalibrated: the default body's 20)."),
        action("Reset Tweaks", armsResetTweaks)
            .help("Upper Arm, Forearm and the shoulders' five tweaks to 0: your arms and shoulders as measured "
                  "(uncalibrated: the default body's). Arm Length and the rest stay."),
        header("Elbows and Wrists (not measured)"),
        slider("Forearm Twist", vr_body_forearm_twist, 0.f, 1.f, 0.05f, "%.2f")
            .help("Share of the wrist's roll the middle of the forearm follows (none at the elbow, all at the wrist)."),
        slider("Wrist Limits", vr_body_wrist_limits, 0.f, 2.f, 0.05f, "%.2f")
            .help("The elbow swings round to keep the wrist within a real one's bend and roll, times this (0: never)."),
        slider("Elbow Stays Clear", vr_body_elbow_lift, 0.f, 8.f, 0.5f, "%.1f")
            .help("How much that swing keeps the elbow out of the torso and from crossing the chest by the neck (a hand "
                  "near the face or the chest), times this (0: free, as before)."),
        slider("Elbows Spread", vr_body_elbow_spread, 0.f, 1.f, 0.05f, "%.2f")
            .help("With the hand at the face or the chest and the palm down, how far the elbow turns out to the side "
                  "(wings). Palm up or on its side, it stays tucked in (0: as before)."),
        slider("Elbow Out", vr_body_elbow_out, 0.f, 1.f, 0.05f, "%.2f")
            .help("Where the elbow points: down, plus this much outwards."),
        slider("Elbow Back", vr_body_elbow_back, 0.f, 1.f, 0.05f, "%.2f")
            .help("Where the elbow points: down, plus this much backwards."),
        slider("Elbow From Hand", vr_body_elbow_hand, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the elbow points away from the back of the hand."),
        slider("Elbow Tuck", vr_body_elbow_tuck, 0.f, 1.f, 0.05f, "%.2f")
            .help("With the hand at the face or the chest (aiming down the sights, an axe raised by the face), the "
                  "elbow stays down and back by the ribs instead of swinging out or across (0: as before)."),
        slider("Tucked Elbow Back", vr_body_elbow_tuck_back, 0.f, 2.f, 0.05f, "%.2f")
            .help("Where a tucked elbow points: down, plus this much backwards (more lets it go behind the chest)."),
        slider("Tuck Within", vr_body_elbow_tuck_near, 0.3f, 1.f, 0.05f, "%.2f arm")
            .help("Fully tucked with the wrist this close to the shoulder (times the arm's length)."),
        slider("Tuck Fades By", vr_body_elbow_tuck_far, 0.4f, 1.2f, 0.05f, "%.2f arm")
            .help("Not tucked at all with the wrist this far from the shoulder (times the arm's length)."),
        header("Pauldrons"),
        open("Pauldrons, Style, Size: Player Appearance", pageIndex(pageVrsAppearance))
            .help("The pauldrons on or off, their style and size are on VR Settings > Player Appearance; their fit here."),
        slider("Pauldron Follows Arm", vr_body_pauldron_follow, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the shoulder cap turns with the upper arm (the lower plates follow the arm fully)."),
        slider("Pauldron Forward", vr_body_pauldron_forward, -0.08f, 0.08f, 0.005f, "%.3f m").extend(-0.3f, 0.3f),
        slider("Pauldron Up", vr_body_pauldron_up, -0.08f, 0.08f, 0.005f, "%.3f m").extend(-0.3f, 0.3f),
        slider("Pauldron Out", vr_body_pauldron_out, -0.08f, 0.08f, 0.005f, "%.3f m").extend(-0.3f, 0.3f)
    );
    return list;
}

// Body Calibration (vr_bodycal.cpp): start or continue it, the result to Apply or Cancel, each pose to take again.
int bodycalVersion = -1;
int bodycalSeated = -1;

void bodycalStart()
{
    bodycal::start(qvr::menu::currentPage());
}

void bodycalRestart()
{
    bodycal::restart(qvr::menu::currentPage());
}

void bodycalApply()
{
    bodycal::apply();
}

void bodycalCancel()
{
    bodycal::cancel();
}

void bodycalUndo()
{
    bodycal::undo();
}

void bodycalSwitch()
{
    bodycal::switchShown();
}

const char* bodycalLine(int i)
{
    const char* line = bodycal::statusLine(i);
    return line ? line : "";
}

void bodycalRevert(int i)
{
    bodycal::revert(i);
}

void bodycalRedo(int step)
{
    bodycal::redo(step, qvr::menu::currentPage());
}

const char* bodycalIntro(int i)
{
    static constexpr const char* lines[] = {"Measures your shoulders and arms from", "a few poses and moves, so that the drawn",
        "arm bends and straightens with yours."};
    return lines[i];
}

[[nodiscard]] za::Vector<Item> pageBodyCalibration()
{
    bodycalVersion = bodycal::version();
    bodycalSeated = vr_bodycal_seated.value != 0.f ? 1 : 0;
    za::Vector<Item> list{infoLine(bodycalIntro, 0), infoLine(bodycalIntro, 1), infoLine(bodycalIntro, 2),
        cycle("Position", vr_bodycal_seated, {{0.f, "Standing"}, {1.f, "Seated"}})
            .help("Seated, the height stays as it is (set it standing: Set Height Now); the arms and shoulders are measured "
                  "the same."),
        infoLine(bodytrackAvailability, 0),
        toggle("Use Body Tracking", vr_bodycal_track)
            .help("Where body tracking data comes in during the poses (Quest 3 through Virtual Desktop), your tracked "
                  "shoulders' width and arm bones are blended into the measurements, so that the tracked elbows fit the "
                  "arm without stretching it. A tracked length far off ours is left out. Taken at the next calibration, "
                  "or a refit."),
        slider("Body Tracking Weight", vr_bodycal_track_weight, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much of the tracked lengths goes into the measurements (0: ours alone, 1: the tracking's). The "
                  "headset may scale its skeleton to your height, so ours (from the controllers) keep a share.")};
    if(bodycal::phase() == bodycal::Phase::Result)
    {
        if(bodycal::trusted())
        {
            list.pushBack(action("Apply", bodycalApply)
                .help("Sets the measurements: your arms' lengths, where your shoulders are and how they rise and swing, and "
                      "(standing) your height. Undo puts the settings back."));
        }
        list.pushBackMultiple(
            action("Cancel", bodycalCancel).help("Nothing changes."),
            action(bodycal::showingNew() ? "Showing: New Measurements" : "Showing: Current Settings", bodycalSwitch)
                .help("Your body (and the one in front of you) with the new measurements, or with the settings as they are: "
                      "bend and straighten your arms to compare."),
            cycle("Body in Front", vr_bodycal_preview, {{0.f, "Off"}, {1.f, "Facing You"}, {2.f, "From the Side"}})
                .help("Your body in front of you while this page shows a result, moving as you do.")
        );
    }
    else
    {
        list.pushBack(action(bodycal::partial() ? "Continue Calibration" : "Start Calibration", bodycalStart)
                           .help("Stand (or sit) with room to stretch your arms. Follow the text in front of you and the "
                                 "figure ahead: after three beeps and a high one, take the pose and hold still until the "
                                 "click; the moves record for a few seconds. About two minutes. The menu button stops "
                                 "(Continue takes the rest)."));
        if(bodycal::partial())
        {
            list.pushBack(action("Start Over", bodycalRestart).help("All the poses again."));
        }
        if(bodycal::canUndo())
        {
            list.pushBack(action("Undo", bodycalUndo).help("Puts back the settings from before the last Apply, exactly."));
        }
    }
    for(int i = 0; bodycal::statusLine(i); i++)
    {
        list.pushBack(infoLine(bodycalLine, i));
    }
    // Previous Calibrations (bodycal_history.txt): the settings each Apply, Undo or Revert replaced, newest first.
    if(bodycal::historyCount() > 0)
    {
        list.pushBack(header("Previous Calibrations"));
        for(int i = 0; i < bodycal::historyCount(); i++)
        {
            list.pushBack(row(bodycal::historyRow, bodycal::historyHelp, bodycalRevert, i, -1));
        }
    }
    if(bodycal::phase() == bodycal::Phase::Result || bodycal::partial())
    {
        list.pushBack(header("Poses"));
        for(int i = 0; i < bodycal::stepCount(); i++)
        {
            if(bodycal::stepUsed(i))
            {
                list.pushBack(row(bodycal::stepRow, bodycal::stepHelp, bodycalRedo, i, -1));
            }
        }
    }
    return list;
}

// Whether each grip's fingers are set by hand (vr_flashlight_low_fingers, _high_fingers), as the Flashlight page was built:
// a change rebuilds it (the curls or the overlap shown).
int flashlightPageManual[2]{-1, -1};

// A grip's fingers on the torch (round 21, the author's tuning notes), as a weapon's (Weapon Offsets > Fingers on the
// Weapon): Fingers, then with Manual the curls and Thumb Across, with Automatic the Overlap; then the finger tweaks and
// the thumb's place.
struct FlashlightFingerCvars
{
    cvar_t& fingers;
    cvar_t& overlap;
    cvar_t& curlThumb;
    cvar_t& thumbAcross;
    cvar_t& curlIndex;
    cvar_t& curlMiddle;
    cvar_t& curlRing;
    cvar_t& curlPinky;
    cvar_t& biasThumb;
    cvar_t& biasIndex;
    cvar_t& biasMiddle;
    cvar_t& biasRing;
    cvar_t& biasPinky;
    cvar_t& thumbX;
    cvar_t& thumbY;
    cvar_t& thumbZ;
};

void flashlightFingers(za::Vector<Item>& list, const FlashlightFingerCvars& c, int& manualAsBuilt)
{
    const char* curlHelp = "How far this finger is curled round the torch: 0 open, 1 a fist (the controller's grip still opens it).";
    const char* fingerHelp = "Closes (+) or opens (-) this finger on top of how it holds the torch (a share of a full curl).";
    list.pushBack(cycle("Fingers", c.fingers, {{0.f, "Automatic"}, {1.f, "Manual"}})
                       .help("Automatic: the fingers wrap the torch on their own. Manual: they take the curls set below "
                             "(no fitting: the hand is where the grip's sliders put it)."));
    manualAsBuilt = c.fingers.value >= 0.5f ? 1 : 0;
    if(manualAsBuilt)
    {
        list.pushBackMultiple(
            slider("Thumb Curl", c.curlThumb, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Thumb Across", c.thumbAcross, 0.f, 1.f, 0.02f, "%.2f")
                .help("How far the thumb turns across the palm: 0 beside the hand, 1 across it."),
            slider("Index Curl", c.curlIndex, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Middle Curl", c.curlMiddle, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Ring Curl", c.curlRing, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Little Curl", c.curlPinky, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp)
        );
    }
    else
    {
        list.pushBack(slider("Overlap", c.overlap, 0.f, 1.f, 0.05f, "%.2f")
                           .help("How far the fingers and palm may sink into the torch: 0 they stop on its surface, 1 a centimetre in."));
    }
    list.pushBackMultiple(
        slider("Thumb", c.biasThumb, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        slider("Index Finger", c.biasIndex, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        slider("Middle Finger", c.biasMiddle, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        slider("Ring Finger", c.biasRing, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        slider("Little Finger", c.biasPinky, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        slider("Thumb X (forward)", c.thumbX, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f).help("Moves the thumb on the hand."),
        slider("Thumb Y (palm)", c.thumbY, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f),
        slider("Thumb Z (up)", c.thumbZ, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f)
    );
}

// Split from Body: the torch on the chest.
[[nodiscard]] za::Vector<Item> pageFlashlight()
{
    za::Vector<Item> list = {
        open("Low Grip", pageIndex(pageFlashlightLowGrip)).help("The torch in your hand as you take it: its place, its turn and the fingers on it."),
        open("Overhead Grip", pageIndex(pageFlashlightOverheadGrip)).help("The torch turned over in your hand (B or Y): its place, its turn and the fingers on it."),
        open("On a Gun or Head", pageIndex(pageFlashlightMounts)).help("Where the torch clips on a gun or on your head, and the zones that clip it on."),
        header("Flashlight"),
        open("Chest Flashlight and Side: Body and Display", pageIndex(pageBodyDisplay)).help("The chest flashlight on or off and its side are on Body and Display."),
        slider("Brightness", vr_flashlight_brightness, 0.25f, 2.5f, 0.05f, "%.2fx").extend(),
        slider("Range", vr_flashlight_range, 300.f, 2000.f, 50.f, "%.0f").extend(100.f, 6000.f),
        slider("Visible Beam", vr_flashlight_beam, 0.f, 1.f, 0.05f, "%.2f").help("A soft cone of light in the air from the lamp (0: none)."),
        cycle("Beam Quality", vr_flashlight_beam_quality, {{0.f, "Low"}, {1.f, "Medium"}, {2.f, "High"}})
            .help("How closely the visible beam fades where walls cut it. Higher looks for them more often, costing more time each frame."),
        toggle("Casts Shadows", vr_flashlight_shadows).help("Its light casts shadows (takes one of the shadowed dynamic lights)."),
        cycle("Cord", vr_flashlight_cord, {{0.f, "None"}, {1.f, "Chain"}, {2.f, "Coiled"}}).help("The retracting cord from the clip on your belt to the torch while it is off the belt. Chain: a rusty low-poly iron chain (chunky links of square bar, flat-shaded: Quake's look). Coiled: a springy coiled cord like an old telephone's, its turns opening out as you pull it. Both sag and swing as your hand moves. None: no cord drawn."),
        hueSlider("Beam Hue", vr_flashlight_hue).help("The beam's colour, with Beam Saturation (at 0 it is white): its light, the beam in the air and the lens. 40 warm, 200 cold blue; Player's: the Player Effects Hue."),
        slider("Beam Saturation", vr_flashlight_saturation, 0.f, 1.f, 0.05f, "%.2f").help("0 white (the default), 1 the Beam Hue in full."),
        header("Turning It Over"),
        toggle("Flick to Turn Over", vr_flashlight_flick)
            .help("A sharp flick of your wrist up or down turns the torch in your hand over, as B or Y does (the beam out "
                  "past your thumb or out past your little finger). Not by a gun or your head (B or Y clips it on there)."),
        slider("Flick Strength", vr_flashlight_flick_speed, 300.f, 1500.f, 50.f, "%.0f deg/s").extend(100.f, 3000.f)
            .help("How fast your wrist must turn up or down to count. Higher: only sharper flicks (fewer by accident)."),
        slider("Flick: Hand Still Below", vr_flashlight_flick_max_move, 0.5f, 4.f, 0.1f, "%.1f m/s").extend(0.1f, 10.f)
            .help("Your hand moving faster than this (a swing, a punch) never flicks: the wrist alone."),
        slider("Flick Cooldown", vr_flashlight_flick_cooldown, 0.1f, 1.5f, 0.05f, "%.2f s")
            .help("After a flick, how long before the next one counts (your wrist coming back is not a second flick)."),
        header("Taking and Clipping On"),
        slider("Grab Range", vr_flashlight_grab_range, 0.5f, 3.f, 0.1f, "%.1fx").extend(0.25f, 4.f)
            .help("How far from the torch your hand reaches it (it lights up, a grip takes it): times 9 cm from its middle line. Higher is more lenient."),
        slider("Head Clip Range", vr_flashlight_head_range, 0.5f, 3.f, 0.1f, "%.1fx").extend(0.25f, 4.f)
            .help("How far from your temple the held torch clips on your head, and a hand takes it off there: times Head Zone Radius (On a Gun or Head)."),
        slider("Gun Clip Range", vr_flashlight_gun_range, 0.5f, 3.f, 0.1f, "%.1fx").extend(0.25f, 4.f)
            .help("How far from its place on the gun in your other hand the held torch clips on it (B or Y, or letting go with Clip on Gun When Let Go): times Gun Zone Radius (On a Gun or Head)."),
        toggle("Clip on Head When Let Go", vr_flashlight_auto_head)
            .help("Let go of the torch where it lights up by your head and it clips on there, no B or Y needed."),
        toggle("Clip on Gun When Let Go", vr_flashlight_auto_gun)
            .help("Let go of the torch where it lights up by the gun in your other hand and it clips on the gun, no B or Y needed."),
        slider("Clip-On Transition", vr_flashlight_clip_time, 0.f, 0.5f, 0.01f, "%.2f s").extend(0.f, 1.f)
            .help("How long the torch takes to move and turn from where it was onto the gun or your head as it clips on. It is on at once (it follows the gun, lights where it aims); only how it is drawn eases in. 0: it jumps there."),
        header("On the Belt"),
        slider("Lean Out", vr_flashlight_tilt, -90.f, 90.f, 1.f, "%.0f deg").extend().help("How far the stored torch, hanging on your belt lens down, leans its lens out from your body."),
        slider("Forward", vr_flashlight_forward, -0.3f, 0.3f, 0.005f, "%.3f m").extend(),
        slider("Up", vr_flashlight_up, -0.4f, 0.4f, 0.01f, "%.2f m").extend(),
        slider("Out", vr_flashlight_out, -0.3f, 0.3f, 0.01f, "%.2f m").extend().help("Away from your middle, towards its side."),
        header("In the Hand"),
        slider("In Hand Forward", vr_flashlight_hand_forward, -0.3f, 0.3f, 0.005f, "%.3f m").extend().help("Where the held lamp sits in your fist."),
        slider("In Hand Up", vr_flashlight_hand_up, -0.3f, 0.3f, 0.005f, "%.3f m").extend(),
    };
    return list;
}

// Split from Flashlight: the low grip (the one you take it in) and its fingers.
[[nodiscard]] za::Vector<Item> pageFlashlightLowGrip()
{
    za::Vector<Item> list = {
        header("In the Hand: Low Grip"),
        slider("Low Grip Forward", vr_flashlight_low_x, -30.f, 30.f, 0.5f, "%.1f cm").extend()
            .help("The torch's place in your fist when the beam comes out of the thumb's side (the grip you take it in), on top of In Hand Forward/Up. The other hand's is the mirror image."),
        slider("Low Grip Towards Palm", vr_flashlight_low_y, -30.f, 30.f, 0.5f, "%.1f cm").extend(),
        slider("Low Grip Up", vr_flashlight_low_z, -30.f, 30.f, 0.5f, "%.1f cm").extend(),
        slider("Low Grip Pitch", vr_flashlight_low_pitch, -180.f, 180.f, 5.f, "%.0f deg").help("Tilts the beam up (positive) or down in the hand."),
        slider("Low Grip Yaw", vr_flashlight_low_yaw, -180.f, 180.f, 5.f, "%.0f deg").help("Turns the beam towards your palm (positive) or away."),
        slider("Low Grip Roll", vr_flashlight_low_roll, -180.f, 180.f, 5.f, "%.0f deg"),
    };
    list.pushBack(header("Low Grip: Fingers on the Torch"));
    flashlightFingers(list,
        {vr_flashlight_low_fingers, vr_flashlight_low_overlap, vr_flashlight_low_curl_thumb, vr_flashlight_low_thumb_across,
            vr_flashlight_low_curl_index, vr_flashlight_low_curl_middle, vr_flashlight_low_curl_ring, vr_flashlight_low_curl_pinky,
            vr_flashlight_low_bias_thumb, vr_flashlight_low_bias_index, vr_flashlight_low_bias_middle, vr_flashlight_low_bias_ring,
            vr_flashlight_low_bias_pinky, vr_flashlight_low_thumb_x, vr_flashlight_low_thumb_y, vr_flashlight_low_thumb_z},
        flashlightPageManual[0]);
    return list;
}

// Split from Flashlight: the overhead grip (turned over with B or Y) and its fingers.
[[nodiscard]] za::Vector<Item> pageFlashlightOverheadGrip()
{
    za::Vector<Item> list = {
        header("In the Hand: Overhead Grip"),
        slider("Overhead Grip Forward", vr_flashlight_high_x, -30.f, 30.f, 0.5f, "%.1f cm").extend()
            .help("The same when B or Y has turned it over (the beam out of the little finger's side)."),
        slider("Overhead Grip Towards Palm", vr_flashlight_high_y, -30.f, 30.f, 0.5f, "%.1f cm").extend(),
        slider("Overhead Grip Up", vr_flashlight_high_z, -30.f, 30.f, 0.5f, "%.1f cm").extend(),
        slider("Overhead Grip Pitch", vr_flashlight_high_pitch, -180.f, 180.f, 5.f, "%.0f deg"),
        slider("Overhead Grip Yaw", vr_flashlight_high_yaw, -180.f, 180.f, 5.f, "%.0f deg"),
        slider("Overhead Grip Roll", vr_flashlight_high_roll, -180.f, 180.f, 5.f, "%.0f deg"),
    };
    list.pushBack(header("Overhead Grip: Fingers on the Torch"));
    flashlightFingers(list,
        {vr_flashlight_high_fingers, vr_flashlight_high_overlap, vr_flashlight_high_curl_thumb, vr_flashlight_high_thumb_across,
            vr_flashlight_high_curl_index, vr_flashlight_high_curl_middle, vr_flashlight_high_curl_ring, vr_flashlight_high_curl_pinky,
            vr_flashlight_high_bias_thumb, vr_flashlight_high_bias_index, vr_flashlight_high_bias_middle, vr_flashlight_high_bias_ring,
            vr_flashlight_high_bias_pinky, vr_flashlight_high_thumb_x, vr_flashlight_high_thumb_y, vr_flashlight_high_thumb_z},
        flashlightPageManual[1]);
    return list;
}

// Split from Flashlight: the torch clipped on a gun or on your head, and the zones that clip it on.
[[nodiscard]] za::Vector<Item> pageFlashlightMounts()
{
    return {
        header("Reach Zones"),
        toggle("Show Flashlight Zones", vr_show_flashlight_zones)
            .help("Draws where holding the torch clips it on (yellow balls at your temples and forehead, an orange ball on "
                  "each gun, where the torch sits on it) and the held torch's middle (white), which they measure. Green: in reach, B or Y clips it on (on "
                  "your head: a hand there takes it off). The torch not in a hand: a capsule round it, green while a "
                  "hand's grip there takes it, and a dot where the game reads each hand near it (the hand may be drawn "
                  "elsewhere)."),
        header("On a Gun"),
        slider("On Gun Forward", vr_flashlight_gun_forward, -0.4f, 0.3f, 0.005f, "%.3f m").extend()
            .help("Where the torch sits once clipped on a gun: along the barrel, under it (or beside a bulky gun). B or Y at it "
                  "takes it off."),
        slider("On Gun Up", vr_flashlight_gun_up, -0.3f, 0.3f, 0.005f, "%.3f m").extend(),
        slider("On Gun Out", vr_flashlight_gun_out, -0.3f, 0.3f, 0.005f, "%.3f m").extend().help("Away from your body."),
        slider("Gun Zone Along", vr_flashlight_gun_zone_forward, -0.2f, 0.2f, 0.005f, "%.3f m").extend()
            .help("Where the torch held by the gun in your other hand lets B or Y (or letting go) clip it on: a ball round the "
                  "middle of the torch where it would sit on that gun (On Gun, and the weapon's own place: Weapon Offsets > "
                  "Flashlight), moved along the gun (forward), up and out. Not elsewhere along the gun (its butt)."),
        slider("Gun Zone Up", vr_flashlight_gun_zone_up, -0.2f, 0.2f, 0.005f, "%.3f m").extend(),
        slider("Gun Zone Out", vr_flashlight_gun_zone_out, -0.2f, 0.2f, 0.005f, "%.3f m").extend().help("Away from your body."),
        slider("Gun Zone Radius", vr_flashlight_gun_zone_radius, 0.02f, 0.3f, 0.005f, "%.3f m").extend()
            .help("How far from that place the held torch's middle may be (times Gun Clip Range)."),
        header("On the Head"),
        slider("On Head Forward", vr_flashlight_head_forward, -0.3f, 0.3f, 0.005f, "%.3f m").extend()
            .help("Where the torch sits once clipped on your head, lighting where you look."),
        slider("On Head Up", vr_flashlight_head_up, -0.3f, 0.3f, 0.005f, "%.3f m").extend(),
        slider("On Head Out", vr_flashlight_head_out, -0.3f, 0.3f, 0.005f, "%.3f m").extend().help("Away from your head, to the side."),
        slider("Head Zone Forward", vr_flashlight_head_zone_forward, -0.2f, 0.2f, 0.005f, "%.3f m").extend()
            .help("Where the torch held at your head lets B or Y clip it on, and where a hand takes it off (B or Y, or the grip): "
                  "balls at your temples and forehead, moved forward, up and out. Apart from where it then sits (On Head)."),
        slider("Head Zone Up", vr_flashlight_head_zone_up, -0.2f, 0.2f, 0.005f, "%.3f m").extend()
            .help("The default is 0.150 (15 cm over your eyes' height)."),
        slider("Head Zone Out", vr_flashlight_head_zone_out, -0.2f, 0.2f, 0.005f, "%.3f m").extend()
            .help("Away from your head: to the side at the temples, ahead at the forehead."),
        slider("Head Zone Radius", vr_flashlight_head_zone_radius, 0.02f, 0.3f, 0.005f, "%.3f m").extend()
            .help("How far from those places the torch's middle (or the fist taking it off) may be."),
    };
}
