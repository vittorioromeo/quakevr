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
        row(opt::vr_body_walk),
        row(opt::vr_body_step_rate),
        row(opt::vr_body_step_run),
        row(opt::vr_body_turn_step),
        row(opt::vr_body_wade),
        row(opt::vr_body_swim_kick),
        row(opt::vr_body_swim_kick_rate),
        row(opt::vr_body_climb_feet),
        row(opt::vr_body_climb_feet_reach),
        row(opt::vr_body_climb_feet_speed),
        row(opt::vr_body_climb_feet_props).extend()
            .help("The feet also find footholds on physics props (a crate you climb: its top and its sides). Off: on the "
                  "level only."),
        row(opt::vr_body_state),
        open("Wounds Drip Blood: Gore", pageIndex(pageGore)).help("The wounds' dripping blood is on Gore (Arm Drip Rate)."),
        row(opt::vr_body_powerups),
        row(opt::vr_quad_look),
        row(opt::vr_body_anchors),
        header("Body Collisions"),
        row(opt::vr_body_collide),
        row(opt::vr_body_collide_pass),
        row(opt::vr_body_collide_elbows),
        // Body tracking (vr_bodytrack.cpp): Quest 3 through Virtual Desktop (XR_FB_body_tracking).
        header("Body Tracking"),
        infoLine(bodytrackAvailability, 0),
        row(opt::vr_bodytrack),
        row(opt::vr_bodytrack_arms),
        row(opt::vr_bodytrack_arm_fit),
        row(opt::vr_bodytrack_legs_walk),
        header("Placement"),
        row(opt::vr_body_torso_back),
        row(opt::vr_body_legs_back),
        row(opt::vr_body_shoulders_back),
        row(opt::vr_body_eye_forward),
        row(opt::vr_body_eye_up),
        row(opt::vr_body_crouch_tilt),
        header("Crouched Pose"),
        open("Crouched Pose Offsets", pageIndex(pageBodyCrouch))
            .help("Where the torso, pelvis, shoulders, elbows and neck sit crouched, and the holsters and the ammo pouch: "
                  "offsets at a full crouch, blended in as you crouch."),
        row(opt::vr_body_crouch_pose_strength),
        row(opt::vr_body_crouch_pose_curve)
            .advanced(),
        row(opt::vr_body_crouch_preview)
            .advanced(),
        header("Death View"),
        row(opt::vr_death_view),
        // (The author's note vrfiringrange_2026-10-23_01-18-00, a player's request.)
        row(opt::vr_death_view_vomit),
        row(opt::vr_death_view_turn),
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
        row(opt::vr_body_crouch_elbow_out),
        row(opt::vr_body_crouch_elbow_back),
        header("Neck"),
        crouchMetres("Eyes Forward", vr_body_crouch_eye_forward, "Crouched, the eyes further in front of the top of the "
            "neck (the neck further back; negative: forward): added to Eyes Forward."),
        crouchMetres("Eyes Up", vr_body_crouch_eye_up, "Crouched, the eyes further over the top of the neck (the neck "
            "lower; negative: higher): added to Eyes Up."),
        header("Hip Holsters (crouched)"),
        crouchUnits("Hips Forward", vr_hip_holster_crouch_x, "Crouched, both hip holsters further forward (negative: "
            "back), units, in the way the body faces."),
        crouchUnits("Hips Out", vr_hip_holster_crouch_y, "Crouched, each hip holster further out (negative: in), units."),
        crouchDegrees("Hips Roll", vr_hip_holster_crouch_roll, "Crouched, their top tips further outwards."),
        header("Ammo Pouch (crouched)"),
        crouchUnits("Pouch Forward", vr_ammo_pouch_crouch_x, "Crouched, the ammo pouch further forward (negative: "
            "back), units."),
        crouchUnits("Pouch Up", vr_ammo_pouch_crouch_z, "Crouched, the ammo pouch higher (negative: lower), units."),
        crouchDegrees("Pouch Pitch", vr_ammo_pouch_crouch_pitch, "Crouched, its top tips further from the body."),
    };
}

// VR Settings > Height > Seated Play (vr_seated.hpp; docs/vr-port/SEATED.md).
[[nodiscard]] za::Vector<Item> pageSeatedHolsters();

[[nodiscard]] za::Vector<Item> pageSeated()
{
    return {
        row(opt::vr_bodycal_seated),
        row(opt::vr_seated_height),
        command("Set Seated Height Now", "vr_seated_height_now")
            .help("Sit up straight where you play, look ahead and press: your seated height from the headset."),
        row(opt::vr_seated_stand),
        row(opt::vr_seated_reach),
        row(opt::vr_seated_reach_start),
        row(opt::vr_seated_reach_floor),
        row(opt::vr_seated_crouch_depth),
        command("Crouch Toggle", "vr_crouch_toggle")
            .help("vr_crouch_toggle: crouch or stand up, seated (to try it here; bind it to a button to play)."),
        row(opt::vr_seated_crouch_button),
        info([]() -> const char* {
            return seated::crouchButtonConflict() ? "Warning: this stick press also starts Bullet Time (Bullet Time > Activation)." : "";
        }),
        open("Seated Holsters", pageIndex(pageSeatedHolsters))
            .help("Where the holsters and the pouches go seated, on top of where they are standing."),
        command("Print Seated Status", "vr_seated_status")
            .help("vr_seated_status: the lift, your head and the legs over the floor, the holsters on the body."),
    };
}

// VR Settings > Height > Seated Play > Seated Holsters: the seated layout (vr_*_seated_*), on top of the standing one.
[[nodiscard]] za::Vector<Item> pageSeatedHolsters()
{
    return {
        header("Hip Holsters (seated)"),
        crouchUnits("Hips Forward", vr_hip_holster_seated_x, "Seated, both hip holsters further forward, onto the "
            "thighs (negative: back), units, in the way the body faces."),
        crouchUnits("Hips Out", vr_hip_holster_seated_y, "Seated, each hip holster further out (negative: in, off the "
            "chair's sides), units."),
        crouchUnits("Hips Up", vr_hip_holster_seated_z, "Seated, the hip holsters higher (negative: lower), units."),
        crouchDegrees("Hips Pitch", vr_hip_holster_seated_pitch, "Seated, the hip holsters' top tips further from the "
            "body (as the Hotspots' Hip Pitch)."),
        crouchDegrees("Hips Yaw", vr_hip_holster_seated_yaw, "Seated, their face turns further outwards."),
        crouchDegrees("Hips Roll", vr_hip_holster_seated_roll, "Seated, their top tips further outwards."),
        header("Chest Holsters (seated)"),
        crouchUnits("Chest Forward", vr_upper_holster_seated_x, "Seated, both chest holsters further forward "
            "(negative: back), units."),
        crouchUnits("Chest Out", vr_upper_holster_seated_y, "Seated, each chest holster further out (negative: in), "
            "units."),
        crouchUnits("Chest Up", vr_upper_holster_seated_z, "Seated, the chest holsters higher (negative: lower), units."),
        crouchDegrees("Chest Pitch", vr_upper_holster_seated_pitch, "Seated, their top tips further from the body."),
        crouchDegrees("Chest Yaw", vr_upper_holster_seated_yaw, "Seated, their face turns further outwards."),
        crouchDegrees("Chest Roll", vr_upper_holster_seated_roll, "Seated, their top tips further outwards."),
        header("Shoulder Holsters (seated)"),
        crouchUnits("Back Forward", vr_shoulder_holster_seated_x, "Seated, both shoulder (back) holsters further "
            "forward, off the chair's back (negative: back), units."),
        crouchUnits("Back Out", vr_shoulder_holster_seated_y, "Seated, each shoulder holster further out (negative: "
            "in), units."),
        crouchUnits("Back Up", vr_shoulder_holster_seated_z, "Seated, the shoulder holsters higher (negative: lower), "
            "units."),
        crouchDegrees("Back Pitch", vr_shoulder_holster_seated_pitch, "Seated, the guns on the back tip further."),
        crouchDegrees("Back Yaw", vr_shoulder_holster_seated_yaw, "Seated, they turn further outwards."),
        crouchDegrees("Back Roll", vr_shoulder_holster_seated_roll, "Seated, their top tips further outwards."),
        header("Ammo Pouch (seated)"),
        crouchUnits("Pouch Forward", vr_ammo_pouch_seated_x, "Seated, the ammo pouch further forward (negative: back), "
            "units."),
        crouchUnits("Pouch Right", vr_ammo_pouch_seated_y, "Seated, the ammo pouch further right (negative: left), "
            "units."),
        crouchUnits("Pouch Up", vr_ammo_pouch_seated_z, "Seated, the ammo pouch higher, off the lap (negative: lower), "
            "units."),
        crouchDegrees("Pouch Pitch", vr_ammo_pouch_seated_pitch, "Seated, its top tips further from the body."),
        crouchDegrees("Pouch Yaw", vr_ammo_pouch_seated_yaw, "Seated, it turns further."),
        crouchDegrees("Pouch Roll", vr_ammo_pouch_seated_roll, "Seated, its top tips further sideways."),
        header("Grenade Pouch (seated)"),
        crouchUnits("Grenades Forward", vr_grenade_pouch_seated_x, "Seated, the grenade pouch further round the hips "
            "from the small of the back (against the chair) towards the front, units (as its own X)."),
        crouchUnits("Grenades Right", vr_grenade_pouch_seated_y, "Seated, the grenade pouch further right (negative: "
            "left), units."),
        crouchUnits("Grenades Up", vr_grenade_pouch_seated_z, "Seated, the grenade pouch higher (negative: lower), "
            "units."),
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
        row(opt::vr_body_tweak_upper_arm),
        row(opt::vr_body_tweak_forearm)
    );
    if(armsPageCalibrated)
    {
        list.pushBack(info(armLengthUnused));
    }
    else
    {
        list.pushBack(row(opt::vr_body_arm_length));
    }
    list.pushBackMultiple(
        row(opt::vr_body_arm_stretch),
        row(opt::vr_body_shoulder_reach),
        header("Shoulders"),
        row(opt::vr_body_tweak_shoulders_back),
        row(opt::vr_body_tweak_shoulders_up),
        row(opt::vr_body_tweak_shoulders_out),
        row(opt::vr_body_tweak_shoulder_rise),
        row(opt::vr_body_tweak_shoulder_swing),
        action("Reset Tweaks", armsResetTweaks)
            .help("Upper Arm, Forearm and the shoulders' five tweaks to 0: your arms and shoulders as measured "
                  "(uncalibrated: the default body's). Arm Length and the rest stay."),
        header("Elbows and Wrists (not measured)"),
        row(opt::vr_body_forearm_twist),
        row(opt::vr_body_wrist_limits),
        row(opt::vr_body_elbow_lift),
        row(opt::vr_body_elbow_spread),
        row(opt::vr_body_elbow_out),
        row(opt::vr_body_elbow_back),
        row(opt::vr_body_elbow_hand),
        row(opt::vr_body_elbow_tuck),
        row(opt::vr_body_elbow_tuck_back),
        row(opt::vr_body_elbow_tuck_near),
        row(opt::vr_body_elbow_tuck_far),
        header("Pauldrons"),
        open("Pauldrons, Style, Size: Player Appearance", pageIndex(pageVrsAppearance))
            .help("The pauldrons on or off, their style and size are on VR Settings > Player Appearance; their fit here."),
        row(opt::vr_body_pauldron_follow),
        row(opt::vr_body_pauldron_forward),
        row(opt::vr_body_pauldron_up),
        row(opt::vr_body_pauldron_out)
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
        row(opt::vr_bodycal_track),
        row(opt::vr_bodycal_track_weight)};
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
            row(opt::vr_bodycal_preview)
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
        row(opt::vr_flashlight_beam_quality),
        row(opt::vr_flashlight_shadows),
        row(opt::vr_flashlight_cord),
        hueSlider("Beam Hue", vr_flashlight_hue).help("The beam's colour, with Beam Saturation (at 0 it is white): its light, the beam in the air and the lens. 40 warm, 200 cold blue; Player's: the Player Effects Hue."),
        header("Turning It Over"),
        row(opt::vr_flashlight_flick),
        row(opt::vr_flashlight_flick_speed),
        header("Taking and Clipping On"),
        row(opt::vr_flashlight_grab_range),
        row(opt::vr_flashlight_head_range),
        row(opt::vr_flashlight_gun_range),
        row(opt::vr_flashlight_auto_head),
        row(opt::vr_flashlight_auto_gun),
        row(opt::vr_flashlight_clip_time),
        header("On the Belt"),
        row(opt::vr_flashlight_tilt),
        row(opt::vr_flashlight_forward),
        row(opt::vr_flashlight_up),
        row(opt::vr_flashlight_out),
    };
    return list;
}

// Split from Flashlight: the low grip (the one you take it in) and its fingers.
[[nodiscard]] za::Vector<Item> pageFlashlightLowGrip()
{
    za::Vector<Item> list;
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
    za::Vector<Item> list;
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
        row(opt::vr_show_flashlight_zones),
        header("On the Head"),
        row(opt::vr_flashlight_head_zone_forward),
        row(opt::vr_flashlight_head_zone_up),
        row(opt::vr_flashlight_head_zone_out),
        row(opt::vr_flashlight_head_zone_radius),
    };
}
