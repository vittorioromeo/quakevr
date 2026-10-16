// vr_menu_wofs.inc -- Weapon Offsets: the editor's actions and its pages. Part of vr_menu.cpp (included where it was:
// the same translation unit, so its anonymous namespace).

// Weapon Offsets: the settings of the weapon a hand holds (vr_wofs_*_NN of its slot), built anew
// each time the page is shown, as the weapon in hand changes.
int weaponOffsetsHand = 1; // 1 main hand, 0 off hand
bool weaponOffsetsStale = true;
int weaponOffsetsSlot = -1;

void showPage(int target);

void weaponOffsetsOtherHand()
{
    weaponOffsetsHand = 1 - weaponOffsetsHand;
    weaponOffsetsStale = true;
    showPage(qvr::menu::currentPage()); // (the same part of Weapon Offsets)
}

int weaponOffsetsHeldSlot = -1; // the weapon in the hand (weaponOffsetsSlot: the one edited: what it inherits from)
int weaponOffsetsInherit = -1;

void weaponOffsetsStopInheriting()
{
    weapons::stopInheriting(weaponOffsetsHeldSlot);
    weaponOffsetsStale = true;
}

void weaponOffsetsReset()
{
    weapons::resetSlotToDefaults(weaponOffsetsSlot, weapons::Part::Offsets);
    weaponOffsetsStale = true;
}

void weaponOffsetsPrint()
{
    weapons::printSlot(weaponOffsetsSlot, weapons::Part::Offsets);
}

// The kind of holster whose Holstered pose the page edits (vr_weapon_holster: 1 hips, 2 chest, 3 back), as the page was
// built: a change rebuilds it.
int weaponOffsetsHolster = -1;

[[nodiscard]] weapons::HolsterKind editedHolster()
{
    return static_cast<weapons::HolsterKind>(CLAMP(1, static_cast<int>(vr_weapon_holster.value), weapons::holsterKinds) - 1);
}

// Holstered: the edited kind's six back to 0 (their defaults).
void weaponOffsetsHolsteredReset()
{
    for(int f = 0; f < weapons::holsteredFields; f++)
    {
        if(cvar_t* var = weapons::cvar(weaponOffsetsSlot, weapons::holsteredKey(editedHolster(), f)))
        {
            Cvar_SetQuick(var, var->default_string);
        }
    }
}

// The hotspot being edited (vr_weapon_hotspot, 1..4) and its type, as the page was built: a change rebuilds it.
int weaponOffsetsHotspot = -1;
int weaponOffsetsHotspotType = -1;
// Whether the weapon's and the edited hotspot's fingers are set by hand, as the page was built (round 21, third pass).
int weaponOffsetsManual = -1;
int weaponOffsetsPreviewOwn = -1; // the page shows the off hand's controller preview offsets (vr_show_controller_off_own)
int weaponOffsetsHotspotManual = -1;

[[nodiscard]] int editedHotspot()
{
    return CLAMP(1, static_cast<int>(vr_weapon_hotspot.value), weapons::maxHotspots) - 1;
}

// A hotspot given to a weapon whose two-handed use is forbidden (the axe, Mjolnir, the grappling hook by
// default) would never be taken: giving one allows it (Two-Handed: Allowed).
void allowTwoHands(int slot)
{
    cvar_t* mode = weapons::cvar(slot, weapons::Key::TwoHMode);
    if(mode && static_cast<int>(mode->value) == 2) // WPN_2H_FORBIDDEN (vr_twohand.cpp)
    {
        Cvar_SetValueQuick(mode, 0.f);
        Con_Printf("Two-handed use allowed for this weapon (it has a hotspot now).\n");
    }
}

// Puts the edited hotspot (a grip) where the other hand is now, on the page's weapon.
void weaponOffsetsHotspotAtHand()
{
    const int slot = weaponOffsetsSlot;
    if(slot < 0)
    {
        return;
    }
    weapons::Hotspot h = weapons::hotspot(slot, editedHotspot());
    glm::vec3 p;
    // A cup is where the other hand's palm is (round 21, third pass); a grip, where its point is.
    const hands::State& hs = hands::current();
    const glm::vec3 at = h.type == weapons::HotspotType::Cup ? hands::palmPoint(hs, 1 - weaponOffsetsHand) : hs.pos[1 - weaponOffsetsHand];
    if(!view::hotspotAt(weaponOffsetsHand, at, p))
    {
        Con_Printf("Hold the weapon in the %s hand to place its hotspot with the other hand.\n",
            weaponOffsetsHand == 1 ? "main" : "off");
        return;
    }
    if(!weapons::isGripType(h.type))
    {
        h.type = weapons::HotspotType::Grip; // a grip or a cup stays what it is
    }
    h.pos = p;
    weapons::setHotspot(slot, editedHotspot(), h);
    allowTwoHands(slot);
    weaponOffsetsStale = true;
}

// The weapon posing mode (vr_posing.cpp) on the page's weapon: the weapon (hotspot -2), a hotspot (0..3), a new one (-1),
// the edited kind of holster (-3).
void weaponOffsetsPose(int hotspot)
{
    qmodel_t* model = weapons::heldModel(weaponOffsetsHand);
    if(weapons::slotForModel(model) != weaponOffsetsHeldSlot)
    {
        Con_Printf("Posing mode: the %s hand holds another weapon now: reopen the page.\n", weaponOffsetsHand == 1 ? "main" : "off");
        return;
    }
    const int weaponHand = static_cast<int>(vr_pose_weapon_hand.value) == 0 ? HAND_OFF : HAND_MAIN;
    const posing::Target target = hotspot == -3   ? posing::Target::Holster
                                  : hotspot == -2 ? posing::Target::Weapon
                                                  : posing::Target::Hotspot;
    if(posing::start(weaponOffsetsHeldSlot, model, weaponHand, target,
           target == posing::Target::Holster ? static_cast<int>(editedHolster()) : hotspot, qvr::menu::currentPage()))
    {
        weaponOffsetsStale = true;
    }
}

// Align Sights to My Aim (vr_sightalign.cpp) on the page's weapon.
int weaponOffsetsSightVersion = -1;
bool weaponOffsetsSightFocus = false; // the section changed: the cursor to Apply (a result) or Undo (applied)

void sightAlignStart()
{
    sightalign::start(weaponOffsetsHand, qvr::menu::currentPage());
}

void sightAlignApply()
{
    sightalign::apply();
}

void sightAlignCancel()
{
    sightalign::cancel();
}

void sightAlignUndo()
{
    sightalign::undo();
}

const char* sightAlignLine(int i)
{
    const char* line = sightalign::statusLine(i);
    return line ? line : "";
}

const char* sightAlignNone()
{
    return "No sights: a melee weapon";
}

void weaponOffsetsPoseWeapon()
{
    weaponOffsetsPose(-2);
}

void weaponOffsetsPoseHotspot()
{
    weaponOffsetsPose(editedHotspot());
}

void weaponOffsetsPoseNewHotspot()
{
    weaponOffsetsPose(-1);
}

// The weapon posing mode in the edited kind of holster: the holster floats in front, the weapon in it to be carried.
void weaponOffsetsPoseHolster()
{
    weaponOffsetsPose(-3);
}

void weaponOffsetsHotspotRemove()
{
    if(weaponOffsetsSlot >= 0)
    {
        weapons::setHotspot(weaponOffsetsSlot, editedHotspot(), weapons::Hotspot{});
        weaponOffsetsStale = true;
    }
}

// Effects: the page's weapon's effects as if it fired (vr_weaponfx_test).
void weaponOffsetsClipTorch()
{
    Cbuf_AddText(va("vr_flashlight 1; vr_flashlight_clip_gun %s\n", weaponOffsetsHand == 1 ? "right" : "left"));
}

void weaponOffsetsTorchReset()
{
    using weapons::Key;
    for(const Key key : {Key::TorchForward, Key::TorchUp, Key::TorchOut, Key::TorchPitch, Key::TorchYaw, Key::TorchRoll})
    {
        if(cvar_t* var = weapons::cvar(weaponOffsetsSlot, key))
        {
            Cvar_SetQuick(var, var->default_string);
        }
    }
}

void weaponOffsetsTestEffects()
{
    Cbuf_AddText(va("vr_weaponfx_test %d 3\n", weaponOffsetsHand == 1 ? 1 : 0));
}

// After the posing test: Shot Pitch and Yaw, under Posing Mode and under Muzzle (weapons::shotAngles).
constexpr const char* weaponOffsetsShotHelp = "Turns where the weapon's shots, projectiles and beams go (the red line, from the "
                                              "muzzle) without moving the weapon: line the red line up with the sights. "
                                              "Degrees, as you hold it (the off hand's yaw mirrored).";

// A slider of the page's weapon's setting (weaponOffsetsSlot's).
[[nodiscard]] Item weaponOffsetsSlider(const char* label, weapons::Key key, float min, float max, float step, const char* format)
{
    return slider(label, weapons::cvar(weaponOffsetsSlot, key), min, max, step, format);
}

// Every Weapon Offsets page begins so (the main page and its parts, WeaponOffsetsPart): the weapon the hand holds now (an
// empty hand: the hand model), its title and Edit the Other Hand's Weapon; the main page also Inherit From. Returns the
// slot edited (weaponOffsetsSlot: the one the weapon inherits from, if it does), -1 if none (the page says so). What the
// page shows that rebuilds it (the hotspot, the Fingers choices, the holster...) is set again by the part showing it.
[[nodiscard]] int weaponOffsetsBegin(za::Vector<Item>& list, int part)
{
    using weapons::Key;
    za::String& title = pageTexts.weaponOffsetsTitle[part];
    weaponOffsetsHotspot = weaponOffsetsHotspotType = weaponOffsetsManual = weaponOffsetsPreviewOwn =
        weaponOffsetsHotspotManual = weaponOffsetsHolster = -1;
    int slot = vrActive() || cls.state == ca_connected ? weapons::heldSlot(weaponOffsetsHand) : -1;
    if(slot < 0 && cls.state == ca_connected)
    {
        slot = weapons::fistSlot(); // an empty hand: the hand model's own settings
    }
    weaponOffsetsSlot = slot;
    weaponOffsetsSightVersion = sightalign::version();

    const char* hand = weaponOffsetsHand == 1 ? "Main hand" : "Off hand";
    if(slot < 0)
    {
        title = za::String(hand) + ": hold a weapon in a game to adjust it";
        list.pushBack(header(title.cStr()));
        list.pushBack(action("Edit the Other Hand's Weapon", weaponOffsetsOtherHand));
        return -1;
    }

    const char* model = weapons::cvar(slot, Key::ID)->string;
    title = za::String(hand) + ": " + model + " (_" + (slot + 1 < 10 ? "0" : "") + za::toString(slot + 1) + ")";

    // A weapon inheriting another's settings (InheritFrom: the other ammo's model): the page edits those.
    const int heldSlot = slot;
    weaponOffsetsHeldSlot = heldSlot;
    weaponOffsetsInherit = weapons::inheritsFrom(heldSlot);
    za::String& inheritTitle = pageTexts.weaponOffsetsInheritTitle[part];
    if(weaponOffsetsInherit >= 0)
    {
        slot = weaponOffsetsInherit;
        weaponOffsetsSlot = slot;
        inheritTitle = za::String("Settings of ") + weapons::cvar(slot, Key::ID)->string + " (inherited)";
    }
    list = {
        header(title.cStr()),
        action("Edit the Other Hand's Weapon", weaponOffsetsOtherHand)
            .help("The page shows the weapon the hand held when it was opened: reopen it after changing weapons."),
    };
    if(part != WofsMain || slot == weapons::fistSlot())
    {
        if(weaponOffsetsInherit >= 0)
        {
            list.pushBack(header(inheritTitle.cStr()));
        }
        return slot;
    }
    za::Vector<za::Pair<float, za::String>>& inheritNames = pageTexts.weaponOffsetsInheritNames;
    inheritNames.clear();
    inheritNames.pushBack({0.f, "None"});
    for(int other = 0; other < weapons::numSlots; other++)
    {
        const char* id = weapons::cvar(other, Key::ID)->string;
        if(other != heldSlot && other != weapons::fistSlot() && id[0] && strcmp(id, "-1") != 0)
        {
            const char* base = strrchr(id, '/');
            inheritNames.pushBack({static_cast<float>(other + 1), base ? base + 1 : id});
        }
    }
    za::Vector<Choice> choices;
    for(const auto& [v, name] : inheritNames)
    {
        choices.pushBack({v, name.cStr()});
    }
    list.pushBack(cycle("Inherit From", weapons::cvar(heldSlot, Key::InheritFrom), ZA_MOVE(choices))
                       .help("Use another weapon's settings (its placement, fingers, hotspots, muzzle, screen): the other "
                             "ammo's model, set once for both. These pages then edit that weapon's."));
    if(weaponOffsetsInherit >= 0)
    {
        list.pushBack(header(inheritTitle.cStr()));
        list.pushBack(action("Stop Inheriting (Copy Them Here)", weaponOffsetsStopInheriting)
                           .help("This weapon gets its own copy of the settings it inherits, to change apart."));
    }
    return slot;
}

// A part of Weapon Offsets with nothing for the empty hand (its own settings: Hand and Grip), or no weapon held.
[[nodiscard]] const char* weaponOffsetsFistNote()
{
    return "The empty hand: its placement is under Hand and Grip";
}

[[nodiscard]] za::Vector<Item> weaponOffsetsNoWeapon(za::Vector<Item>& list, int slot)
{
    if(slot >= 0)
    {
        list.pushBack(info(weaponOffsetsFistNote));
        list.pushBack(open("Hand and Grip", pageIndex(pageWofsHand)));
    }
    return ZA_MOVE(list);
}

// Weapon Offsets: the posing mode, the parts' pages (weaponOffsetsPartPages) and the weapon's own actions.
za::Vector<Item> pageWeaponOffsets()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsMain);
    if(slot < 0)
    {
        return list;
    }
    const auto s = weaponOffsetsSlider;
    const bool fist = slot == weapons::fistSlot(); // the empty hand's "weapon" is the hand model
    const char* shotHelp = weaponOffsetsShotHelp;
    if(!fist)
    {
        // The weapon posing mode (vr_posing.cpp).
        list.pushBackMultiple(
            header("Posing Mode"),
            action("Pose This Weapon", weaponOffsetsPoseWeapon)
                .help("The weapon floats still in front of you: put the weapon hand on it as you want to hold it and "
                      "press the other hand's A/X to set its place in the hand. B/Y undoes, the trigger goes on to the "
                      "hotspots, the stick turns the weapon, the menu button comes back here."),
            cycle("Weapon Hand", vr_pose_weapon_hand, {{1.f, "Main Hand"}, {0.f, "Off Hand (Mirrored)"}})
                .help("The hand that poses the weapon (the settings are shared: the off hand's are mirrored). The other "
                      "hand poses the hotspots and confirms."),
            cycle("Tuning Offsets on Confirm", vr_pose_reset_offsets, {{0.f, "Keep"}, {1.f, "Set to 0"}})
                .help("Keep: the pose is kept with Hand and Weapon Together, Hand Only and Held Hand as they are. "
                      "Set to 0: confirming sets them to 0 (the pose alone places the hand)."),
            s("Shot Pitch (up)", Key::ShotPitch, -10.f, 10.f, 0.1f, "%+.1f").extend(-45.f, 45.f).help(shotHelp),
            s("Shot Yaw (left)", Key::ShotYaw, -10.f, 10.f, 0.1f, "%+.1f").extend(-45.f, 45.f).help(shotHelp)
        );
    }
    list.pushBack(header("Settings"));
    for(const WeaponOffsetsPartPage& part : weaponOffsetsPartPages)
    {
        if(!fist || part.build == pageWofsHand)
        {
            list.pushBack(open(fist ? "The Hand" : part.link, pageIndex(part.build)).help(part.help));
        }
    }
    list.pushBackMultiple(
        open("Weight, Melee and Throwing", pageIndex(pageWeaponWeights))
            .help("Weapon Weights: its mass, balance and length, how it follows your hand, its melee and throw damage, "
                  "its spin thrown."),
        header("This Weapon"),
        action("Print Changes to Console", weaponOffsetsPrint)
            .help("Prints this weapon's offsets that differ from the defaults, ready to be made the shipped defaults."),
        action("Reset This Weapon", weaponOffsetsReset)
            .help("This weapon's offsets (every Weapon Offsets page's) back to their defaults. Its weight settings (Weapon "
                  "Weights) stay.")
    );
    return list;
}

za::Vector<Item> pageWofsHand()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsHand);
    if(slot < 0)
    {
        return list;
    }
    const auto s = weaponOffsetsSlider;
    const bool fist = slot == weapons::fistSlot(); // the empty hand's "weapon" is the hand model
    list.pushBackMultiple(
        header(fist ? "The Hand" : "Weapon in the Hand"),
        s("Offset X (forward)", Key::OffsetX, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f)
            .help(fist ? "Moves the drawn hand." :
                         "Where the weapon sits in the hand (the hand is where the controller is, and its fingers wrap "
                         "the weapon's grip). Doesn't change where it aims."),
        s("Offset Y (left)", Key::OffsetY, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
        s("Offset Z (up)", Key::OffsetZ, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
        s("Pitch", Key::Pitch, -180.f, 180.f, 0.5f, "%.1f").help("How the weapon is turned in the hand."),
        s("Yaw", Key::Yaw, -180.f, 180.f, 0.5f, "%.1f"),
        s("Roll", Key::Roll, -180.f, 180.f, 0.5f, "%.1f"),
        s("Scale", Key::Scale, 0.1f, 3.f, 0.01f, "%.2f").extend(0.02f, 10.f),
        cycle("Hide Hand", weapons::cvar(slot, Key::HideHand), {{0.f, "No"}, {1.f, "Yes"}})
    );
    if(!fist)
    {
        // Round 21, third pass: the tuning offsets, applied last, and the aids to see them by.
        const char* frameHelp = "In the controller's aim frame: X forward, Y left, Z up (as for the main hand; the off hand "
                                "mirrored).";
        list.pushBackMultiple(
            header("Tuning Aids"),
            toggle("Show Controller", vr_show_controller)
                .help("Draws each controller as tracked: a Quest 3 controller at its grip, translucent, with its axes (red "
                      "along the handle, green left, blue up), before any offset; the hand's point (yellow) where the "
                      "offsets below move it. Line it up with your real controller under Controller Preview."),
            toggle("Show Controller Laser", vr_show_controller_laser)
                .help("White: where the controller points (Gun Angle included). Red: where the weapon's shots go, from "
                      "its muzzle. Green: the weapon's barrel, as drawn. Turn the weapon (Pitch, Yaw) until green runs "
                      "along red, or the shots (Shot Pitch, Shot Yaw: Muzzle and Sights) until red meets the sights.")
        );
        // After the posing test: the preview's offsets, to match the real controllers (drawControllerPreview).
        const char* previewHelp = "Moves the Show Controller preview (not the hands or weapons) to match your real controller: "
                                  "centimetres along its axes (red: along the handle, green: left, blue: up), degrees.";
        list.pushBackMultiple(
            header("Controller Preview (Show Controller)"),
            slider("Preview X (red)", vr_show_controller_x, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
            slider("Preview Y (green)", vr_show_controller_y, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
            slider("Preview Z (blue)", vr_show_controller_z, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
            slider("Preview Pitch (up)", vr_show_controller_pitch, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
            slider("Preview Yaw (left)", vr_show_controller_yaw, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
            slider("Preview Roll", vr_show_controller_roll, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
            cycle("Off Hand Preview", vr_show_controller_off_own, {{0.f, "Mirrors the Main Hand's"}, {1.f, "Its Own"}})
                .help("The sliders above are the main hand's; the off hand's preview mirrors them (Y, Yaw and Roll the other "
                      "way), or takes its own.")
        );
        weaponOffsetsPreviewOwn = vr_show_controller_off_own.value != 0.f ? 1 : 0;
        if(weaponOffsetsPreviewOwn)
        {
            list.pushBackMultiple(
                slider("Off Hand X (red)", vr_show_controller_off_x, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
                slider("Off Hand Y (green)", vr_show_controller_off_y, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
                slider("Off Hand Z (blue)", vr_show_controller_off_z, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
                slider("Off Hand Pitch (up)", vr_show_controller_off_pitch, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
                slider("Off Hand Yaw (left)", vr_show_controller_off_yaw, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
                slider("Off Hand Roll", vr_show_controller_off_roll, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp)
            );
        }
        list.pushBackMultiple(
            header("Hand and Weapon Together"),
            s("Together X (forward)", Key::WholeX, -15.f, 15.f, 0.1f, "%+.1f").extend()
                .help("Moves the hand and the weapon together, last (after the fingers wrap it and the palm fits): the "
                      "muzzle, the aim and the melee move with them. Use it to put the drawn hand back on the controller. "
                      "Units."),
            s("Together Y (left)", Key::WholeY, -15.f, 15.f, 0.1f, "%+.1f").extend().help(frameHelp),
            s("Together Z (up)", Key::WholeZ, -15.f, 15.f, 0.1f, "%+.1f").extend().help(frameHelp),
            s("Together Pitch (up)", Key::WholePitch, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f)
                .help("Turns the hand and the weapon together about the controller's point: the aim turns too."),
            s("Together Yaw (left)", Key::WholeYaw, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f),
            s("Together Roll", Key::WholeRoll, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f),
            header("Hand Only"),
            s("Hand X (forward)", Key::HandOnlyX, -10.f, 10.f, 0.1f, "%+.1f").extend()
                .help("Moves the drawn hand alone on the weapon: the weapon, its muzzle and its aim stay; the fingers wrap "
                      "it again. Units."),
            s("Hand Y (left)", Key::HandOnlyY, -10.f, 10.f, 0.1f, "%+.1f").extend().help(frameHelp),
            s("Hand Z (up)", Key::HandOnlyZ, -10.f, 10.f, 0.1f, "%+.1f").extend().help(frameHelp),
            s("Hand Pitch (up)", Key::HandOnlyPitch, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f)
                .help("Turns the drawn hand alone about its palm (a bent wrist straightened): the weapon stays."),
            s("Hand Yaw (left)", Key::HandOnlyYaw, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f),
            s("Hand Roll", Key::HandOnlyRoll, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f)
        );
    }
    return list;
}

za::Vector<Item> pageWofsFingers()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsFingers);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    const auto s = weaponOffsetsSlider;
    const char* fingerHelp = "Closes (+) or opens (-) this finger on top of how it wraps the weapon on its own "
                             "(a share of a full curl).";
    list.pushBackMultiple(
        header("Fingers on the Weapon"),
        cycle("Fingers", weapons::cvar(slot, Key::FingerManual), {{0.f, "Automatic"}, {1.f, "Manual"}})
            .help("Automatic: the fingers wrap the weapon on their own. Manual: they take the curls set below (no "
                  "fitting); the index finger still pulls the trigger.")
    );
    weaponOffsetsManual = weapons::value(slot, Key::FingerManual) >= 0.5f ? 1 : 0;
    if(weaponOffsetsManual)
    {
        const char* curlHelp = "How far this finger is curled: 0 open, 1 a fist (the controller's grip still opens it).";
        list.pushBackMultiple(
            s("Thumb Curl", Key::FingerCurlThumb, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            s("Thumb Across", Key::FingerThumbAcross, 0.f, 1.f, 0.02f, "%.2f")
                .help("How far the thumb turns across the palm: 0 beside the hand, 1 across it."),
            s("Index Curl", Key::FingerCurlIndex, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            s("Middle Curl", Key::FingerCurlMiddle, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            s("Ring Curl", Key::FingerCurlRing, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            s("Little Curl", Key::FingerCurlPinky, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp)
        );
    }
    else
    {
        list.pushBack(s("Overlap", Key::GripOverlap, 0.f, 1.f, 0.05f, "%.2f")
                           .help("How far the fingers and palm may sink into the weapon: 0 they stop on its surface, 1 a "
                                 "centimetre in."));
    }
    list.pushBackMultiple(
        s("Thumb", Key::FingerThumbBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        s("Index Finger", Key::FingerIndexBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        s("Middle Finger", Key::FingerMiddleBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        s("Ring Finger", Key::FingerRingBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        s("Little Finger", Key::FingerPinkyBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        s("Thumb X (forward)", Key::FingerThumbX, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f).help("Moves the thumb on the hand."),
        s("Thumb Y (palm)", Key::FingerThumbY, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f),
        s("Thumb Z (up)", Key::FingerThumbZ, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f)
    );
    return list;
}

za::Vector<Item> pageWofsSights()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsSights);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    const auto s = weaponOffsetsSlider;
    const char* shotHelp = weaponOffsetsShotHelp;
    // Align Sights to My Aim (vr_sightalign.cpp).
    list.pushBack(header("Align Sights to My Aim"));
    if(sightalign::alignable(weaponOffsetsHand))
    {
        if(sightalign::phase() == sightalign::Phase::Result)
        {
            list.pushBack(action("Apply", sightAlignApply)
                               .help("Turns the hand and the gun together about your fist (Hand and Weapon Together) so "
                                     "that the sights line up in front of your dominant eye, and the shots onto the "
                                     "sight line (Shot Pitch and Yaw). Undo puts the values back."));
            list.pushBack(action("Cancel", sightAlignCancel).help("Nothing changes."));
        }
        else
        {
            list.pushBack(action("Align Sights to My Aim", sightAlignStart)
                               .help("Close your eyes and lower the gun. After three beeps and a high one, raise it as you "
                                     "raise your own gun, and hold still: a click takes it. Lower it and raise it again "
                                     "after each click, until the chime. Then open your eyes: Apply or Cancel. The menu "
                                     "button stops."));
            if(sightalign::canUndo())
            {
                list.pushBack(action("Undo", sightAlignUndo).help("Puts back the values from before Apply, exactly."));
            }
        }
        for(int i = 0; sightalign::statusLine(i); i++)
        {
            list.pushBack(infoLine(sightAlignLine, i));
        }
    }
    else
    {
        list.pushBack(info(sightAlignNone));
    }
    list.pushBackMultiple(
        cycle("Dominant Eye", vr_dominant_eye, {{0.f, "Right"}, {1.f, "Left"}})
            .help("The eye that looks along the sights: pointing at something with both eyes open, the one that stays on "
                  "it when you close the other."),
        cycle("Captures", vr_sight_align_captures, {{3.f, "3"}, {4.f, "4"}, {5.f, "5"}})
            .help("How many times the aim is taken: averaged, one that stands apart from the others dropped."),
        toggle("Show Sight Line", vr_show_sight_line)
            .help("Draws each held gun's sight line: its rear point (yellow), its front one (cyan), and the line through "
                  "them to the wall. Painted sights: the shotguns, the lightning gun; the others a line along the top.")
    );
    list.pushBackMultiple(
        header("Muzzle"),
        s("Muzzle X", Key::MuzzleOffsetX, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f).help("Where shots and the muzzle flash start, from the muzzle vertex."),
        s("Muzzle Y", Key::MuzzleOffsetY, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
        s("Muzzle Z", Key::MuzzleOffsetZ, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
        s("Shot Pitch (up)", Key::ShotPitch, -10.f, 10.f, 0.1f, "%+.1f").extend(-45.f, 45.f).help(shotHelp),
        s("Shot Yaw (left)", Key::ShotYaw, -10.f, 10.f, 0.1f, "%+.1f").extend(-45.f, 45.f).help(shotHelp)
    );
    return list;
}

za::Vector<Item> pageWofsTwoHanded()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsTwoHanded);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    const auto s = weaponOffsetsSlider;
    // The two-handed grips: the hotspot being edited.
    const int index = editedHotspot();
    const weapons::Hotspot h = weapons::hotspot(slot, index);
    weaponOffsetsHotspot = index;
    weaponOffsetsHotspotType = static_cast<int>(h.type);
    const auto hk = [&](int field) { return weapons::cvar(slot, weapons::hotspotKey(index, field)); };
    list.pushBackMultiple(
        header("Other Hand's Grips (Hotspots)"),
        cycle("Two-Handed", weapons::cvar(slot, Key::TwoHMode),
            {{0.f, "Allowed"}, {1.f, "Allowed, No Stock"}, {2.f, "Not Allowed"}, {3.f, "Sword"}})
            .help("Whether the other hand may hold this weapon. Not Allowed ignores its hotspots (giving it a "
                  "hotspot allows it). No Stock: never steadied at the shoulder. Sword: the other hand below "
                  "the holding hand or on the blade."),
        cycle("Other Hand Anywhere", weapons::cvar(slot, Key::AnyGripMode),
            {{-1.f, "As Carrying Setting"}, {0.f, "As a Foregrip"}, {1.f, "Support Only"}, {2.f, "Rigid"}})
            .help("The other hand gripping this weapon away from its handle and hotspots (Carrying: Weapons "
                  "Anywhere): both hands aim it from there, it only bears the weight, or the weapon follows both "
                  "hands rigidly. As Carrying Setting: Carrying's Other Hand Anywhere. Two-Handed Not Allowed: "
                  "support only."),
        cycle("Hotspot", vr_weapon_hotspot, {{1.f, "1"}, {2.f, "2"}, {3.f, "3"}, {4.f, "4"}})
            .help("Where the other hand may hold the weapon: it takes the one nearest it, less its bias. Pick one to edit."),
        cycle("Type", hk(0), {{0.f, "None"}, {1.f, "Grip"}, {2.f, "Blade"}, {3.f, "Cup"}})
            .help("Grip: a point (a foregrip, a pump, a magazine) the hand is drawn on; the two hands aim the weapon. "
                  "Blade: the half-sword grip along the blade. Cup: a two-handed pistol grip, the hand under and "
                  "round the holding hand (it doesn't aim)."),
        action("Pose This Hotspot", weaponOffsetsPoseHotspot)
            .help("Posing mode on this hotspot (a grip if it has no type): the weapon floats, held by the weapon "
                  "hand; put the other hand where it should hold it and press the weapon hand's A/X."),
        action("Pose a New Hotspot", weaponOffsetsPoseNewHotspot).help("The same on the first free hotspot.")
    );
    if(h.type == weapons::HotspotType::Blade)
    {
        list.pushBack(slider("Along the Blade", hk(1), 0.f, 1.f, 0.01f, "%.2f")
                           .help("Where on the blade the grip is centred: a share of the way from the hand to the tip."));
        list.pushBack(slider("Blade Grip Ends At", hk(2), 0.f, 1.05f, 0.01f, "%.2f")
                           .help("How far towards the tip the hand may hold it and slide along it: a share of the way "
                                 "from the hand to the tip (0: just past the tip). The crowbar's ends short of its hook."));
    }
    else
    {
        list.pushBackMultiple(
            slider("Hotspot X", hk(1), -40.f, 40.f, 0.1f, "%.2f")
                .extend(-200.f, 200.f)
                .help(h.type == weapons::HotspotType::Cup
                          ? "Where the helping hand's palm sits, in the weapon's model units: it is taken there (by the "
                            "palm) and drawn there."
                          : "The grip's point, in the weapon's model units."),
            slider("Hotspot Y", hk(2), -40.f, 40.f, 0.1f, "%.2f").extend(-200.f, 200.f),
            slider("Hotspot Z", hk(3), -40.f, 40.f, 0.1f, "%.2f").extend(-200.f, 200.f),
            action("Put It Where the Other Hand Is", weaponOffsetsHotspotAtHand)
                .help("Makes this hotspot a grip at the other hand, as it is now (a cup: at its palm).")
        );
    }
    list.pushBackMultiple(
        slider("Hand Pitch", hk(5), -90.f, 90.f, 1.f, "%.0f").extend(-180.f, 180.f).help("How the hand holding it is turned there."),
        slider("Hand Yaw", hk(6), -90.f, 90.f, 1.f, "%.0f").extend(-180.f, 180.f),
        slider("Hand Roll", hk(7), -180.f, 180.f, 1.f, "%.0f"),
        cycle("Thumb", hk(8), {{0.f, "Wraps round"}, {1.f, "Along the top"}})
            .help("Whether the thumb wraps round it with the fingers, or lies along its top."),
        cycle("Fingers There", hk(16), {{0.f, "Automatic"}, {1.f, "Manual"}})
            .help("Automatic: the hand holding it wraps it on its own. Manual: its fingers take the curls set here.")
    );
    weaponOffsetsHotspotManual = h.manual ? 1 : 0;
    if(h.manual)
    {
        const char* curlHelp = "How far this finger is curled: 0 open, 1 a fist.";
        list.pushBackMultiple(
            slider("Thumb Curl There", hk(17), 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Thumb Across There", hk(22), 0.f, 1.f, 0.02f, "%.2f")
                .help("How far the thumb turns across the palm: 0 beside the hand, 1 across it."),
            slider("Index Curl There", hk(18), 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Middle Curl There", hk(19), 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Ring Curl There", hk(20), 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Little Curl There", hk(21), 0.f, 1.f, 0.02f, "%.2f").help(curlHelp)
        );
    }
    else
    {
        list.pushBack(slider("Overlap There", hk(9), 0.f, 1.f, 0.05f, "%.2f")
                           .help("How far the hand holding it may sink into the weapon: 0 not at all, 1 a centimetre (into "
                                 "the other hand, on a cup: Hand/Gun Calibration's Fit Overlap: Hands)."));
    }
    list.pushBackMultiple(
        slider("Held Hand X (forward)", hk(10), -10.f, 10.f, 0.1f, "%+.1f").extend()
            .help("Moves the hand drawn on this hotspot once it holds it (visual only: where it is taken, and the "
                  "aim, don't change). In the holding hand's aim frame, units; as for the off hand helping."),
        slider("Held Hand Y (left)", hk(11), -10.f, 10.f, 0.1f, "%+.1f").extend(),
        slider("Held Hand Z (up)", hk(12), -10.f, 10.f, 0.1f, "%+.1f").extend(),
        slider("Held Hand Pitch (up)", hk(13), -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help("Turns it there, about its palm."),
        slider("Held Hand Yaw (left)", hk(14), -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f),
        slider("Held Hand Roll", hk(15), -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f)
    );
    list.pushBackMultiple(
        slider("Bias", hk(4), 0.f, 10.f, 0.1f, "%.1f").extend(0.f, 50.f).help("Units taken off its distance: larger, easier to take than the others."),
        slider("Stickiness", hk(23), 0.5f, 4.f, 0.05f, "%.2fx").extend(0.1f, 20.f)
            .help("Once your other hand holds it, times how far it may go off it (20 units), out of line (Aiming: 2H "
                  "Aiming Threshold) and past the muzzle before it lets go; times Aiming: 2H Grip Stickiness too, more "
                  "while swinging. For heavy weapons swung with two hands."),
        action("Remove This Hotspot", weaponOffsetsHotspotRemove),
        toggle("Show Hotspots", vr_show_weapon_hotspots).help("Marks the held weapons' hotspots (the edited one white).")
    );
    list.pushBackMultiple(
        header("Two-Handed Aim"),
        s("Aim Offset X", Key::TwoHOffsetX, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f)
            .help("Moves the point the aim is taken from, in the holding hand's frame (nothing drawn moves)."),
        s("Aim Offset Y", Key::TwoHOffsetY, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
        s("Aim Offset Z", Key::TwoHOffsetZ, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
        s("Aim Pitch", Key::TwoHPitch, -180.f, 180.f, 0.5f, "%.1f")
            .help("Turns the two-handed aim (a sword: its blade's direction in the model)."),
        s("Aim Yaw", Key::TwoHYaw, -180.f, 180.f, 0.5f, "%.1f"),
        s("Aim Roll", Key::TwoHRoll, -180.f, 180.f, 0.5f, "%.1f")
    );
    return list;
}

// Virtual Stock: how far the virtual stock steadies the page's hand's aim now (twohand::stock).
[[nodiscard]] const char* weaponOffsetsStockReadout(int /* unused */)
{
    za::String& text = readouts.weaponOffsetsStock;
    text = static_cast<int>(vr_2h_mode.value) != 2
               ? za::String("Now: off (2H Aiming below is not Virtual Stock)")
               : za::String(va("Now: two-handed %.0f%%, at the shoulder %.0f%%", 100.f * twohand::transition(weaponOffsetsHand),
                     100.f * twohand::stock(weaponOffsetsHand)));
    return text.cStr();
}

za::Vector<Item> pageWofsStock()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsStock);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    const auto s = weaponOffsetsSlider;
    const char* turnHelp = "Turns the aim (and the weapon) while it is steadied at your shoulder, on top of the two-handed aim, "
                           "as far as the stock is engaged: degrees, up, left and clockwise as you hold it (the off hand's "
                           "yaw and roll mirrored). For a weapon that points off where it should when shouldered.";
    list.pushBackMultiple(
        header("Virtual Stock: This Weapon"),
        infoLine(weaponOffsetsStockReadout, 0),
        cycle("Two-Handed", weapons::cvar(slot, Key::TwoHMode),
            {{0.f, "Allowed"}, {1.f, "Allowed, No Stock"}, {2.f, "Not Allowed"}, {3.f, "Sword"}})
            .help("Whether the other hand may hold this weapon; No Stock: never steadied at the shoulder (as on Two-Handed "
                  "and Hotspots)."),
        s("Stock Pitch (up)", Key::StockPitch, -30.f, 30.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(turnHelp),
        s("Stock Yaw (left)", Key::StockYaw, -30.f, 30.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(turnHelp),
        s("Stock Roll (right)", Key::StockRoll, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(turnHelp),
        header("Every Weapon"),
        cycle("2H Aiming", "vr_2h_mode", {{0.f, "Disabled"}, {1.f, "Basic"}, {2.f, "Virtual Stock"}})
            .help("Virtual Stock: a two-handed weapon whose holding hand comes near your shoulder aims from the shoulder "
                  "(as on Aiming)."),
        slider("Stock Factor", "vr_2h_virtual_stock_factor", 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the shoulder counts in the aim against the hands (as on Aiming)."),
        slider("Stock Distance", "vr_virtual_stock_thresh", 0.f, 30.f, 0.1f, "%.1f").extend(0.f, 100.f)
            .help("How near the shoulder the holding hand must be (units; Virtual Stock Thresh. on Hotspots)."),
        toggle("Show Virtual Stock", "vr_show_virtual_stock").help("Marks the shoulders the stock is taken at."),
        open("Hotspots (the Shoulders)", pageIndex(pageHotspotSettings))
    );
    return list;
}

za::Vector<Item> pageWofsScreen()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsScreen);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    const auto s = weaponOffsetsSlider;
    list.pushBackMultiple(
        header("Ammo Screen"),
        s("Screen X", Key::WpnTextX, -20.f, 20.f, 0.05f, "%.2f").extend(-100.f, 100.f),
        s("Screen Y", Key::WpnTextY, -20.f, 20.f, 0.05f, "%.2f").extend(-100.f, 100.f),
        s("Screen Z", Key::WpnTextZ, -20.f, 20.f, 0.05f, "%.2f").extend(-100.f, 100.f),
        s("Screen Pitch", Key::WpnTextPitch, -180.f, 180.f, 0.5f, "%.1f"),
        s("Screen Yaw", Key::WpnTextYaw, -180.f, 180.f, 0.5f, "%.1f"),
        s("Screen Roll", Key::WpnTextRoll, -180.f, 180.f, 0.5f, "%.1f"),
        s("Screen Scale", Key::WpnTextScale, 0.05f, 3.f, 0.05f, "%.2f").extend(0.01f, 10.f),
        cycle("Ammo Screen", weapons::cvar(slot, Key::WpnTextMode), {{1.f, "Shown"}, {0.f, "Hidden"}})
            .help("Whether this weapon shows its ammunition screen (every weapon's: Screens, Weapon Ammo Screen).")
    );
    return list;
}

za::Vector<Item> pageWofsHolstered()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsHolstered);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    // The weapon in a holster (weapons::holsteredPose), per kind of holster: the page's weapon drawn in both holsters
    // of that kind while one of these is chosen (view: setupHolsters, menu::holsterPreview).
    const weapons::HolsterKind kind = editedHolster();
    weaponOffsetsHolster = static_cast<int>(kind);
    const auto hs = [&](const char* label, int field, float min, float max, float step, const char* format) {
        return slider(label, weapons::cvar(slot, weapons::holsteredKey(kind, field)), min, max, step, format);
    };
    const char* moveHelp = "Moves this weapon in the holster (units): X off your body (negative: into it), Y outwards, "
                           "away from your middle, Z up. The left holster mirrors the right. Only how it is drawn: "
                           "where you reach for the holster doesn't change.";
    const char* turnHelp = "Turns this weapon in the holster about its grip, on top of the holster's own turn (Hotspots): "
                           "Pitch tips its top off your body, Yaw turns it outwards, Roll tips its top outwards (a "
                           "hanging gun's muzzle goes the other way). The left holster mirrors the right.";
    list.pushBackMultiple(
        header("Holstered"),
        cycle("Holster", vr_weapon_holster, {{1.f, "Hip"}, {2.f, "Upper (Chest)"}, {3.f, "Shoulder (Back)"}})
            .help("The holsters whose pose of this weapon the sliders below edit: a weapon lies differently on the hips, "
                  "the chest and the back, so each has its own."),
        action("Pose in This Holster", weaponOffsetsPoseHolster)
            .help("Posing mode in these holsters: the right one floats in front of you, this weapon in it as the "
                  "sliders below put it. Take the weapon with either grip, carry it, let go where it should sit (it "
                  "stays there); A/X sets it (B/Y undoes), the menu button sets it and comes back here. The trigger "
                  "goes on to the next kind of holster, the stick turns it all."),
        toggle("Preview in Holster", vr_weapon_holster_preview)
            .help("While a setting of this section is chosen, this weapon is drawn in both holsters of that kind (in place "
                  "of what they hold): look down at them, or at the body preview, as you tune it."),
        hs("Holstered X (off body)", 0, -10.f, 10.f, 0.1f, "%+.1f").extend().help(moveHelp),
        hs("Holstered Y (outwards)", 1, -10.f, 10.f, 0.1f, "%+.1f").extend().help(moveHelp),
        hs("Holstered Z (up)", 2, -10.f, 10.f, 0.1f, "%+.1f").extend().help(moveHelp),
        hs("Holstered Pitch", 3, -90.f, 90.f, 1.f, "%+.0f deg").extend(-180.f, 180.f).help(turnHelp),
        hs("Holstered Yaw", 4, -90.f, 90.f, 1.f, "%+.0f deg").extend(-180.f, 180.f).help(turnHelp),
        hs("Holstered Roll", 5, -90.f, 90.f, 1.f, "%+.0f deg").extend(-180.f, 180.f).help(turnHelp),
        action("Holstered Back to 0", weaponOffsetsHolsteredReset)
            .help("This weapon's pose in these holsters back to 0: as the holster alone places it.")
    );
    return list;
}

za::Vector<Item> pageWofsEffects()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsEffects);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    const auto s = weaponOffsetsSlider;
    list.pushBackMultiple(
        // The programmatic weapon effects (vr_weaponfx.cpp; their global settings: Weapon Effects).
        header("Effects"),
        cycle("Recoil", weapons::cvar(slot, Key::Recoil), {{0.f, "Off (the model's own)"}, {1.f, "On"}})
            .help("Each shot kicks the drawn weapon back and tips it up, then it comes back to rest: for a model without "
                  "a recoil animation of its own. Looks only: the aim and the shots don't move."),
        s("Recoil Strength", Key::RecoilStrength, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("Times Weapon Effects' Kick Back and Muzzle Rise."),
        s("Recoil Return Time", Key::RecoilTime, 0.03f, 0.6f, 0.01f, "%.2f s").extend(0.02f, 3.f)
            .help("How long a shot's kick takes to come back to rest."),
        cycle("Muzzle Flash", weapons::cvar(slot, Key::Flash), {{0.f, "Off (the model's own)"}, {1.f, "On"}})
            .help("The shotgun's flash at this weapon's muzzle (its Muzzle settings) for a moment at each shot, following "
                  "the gun; gone when it leaves your hand."),
        s("Flash Size", Key::FlashSize, 0.2f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f).help("Times the shotgun's flash."),
        s("Flash Time", Key::FlashTime, 0.02f, 0.2f, 0.01f, "%.2f s").extend(0.01f, 1.f).help("How long it shows."),
        cycle("Tracers", weapons::cvar(slot, Key::Tracers), {{0.f, "As Weapon Effects"}, {1.f, "Never"}, {2.f, "Always"}})
            .help("Bullet tracers of this weapon's hitscan shots (the shotguns, the burst rifle): as Weapon Effects' "
                  "Bullet Tracers, never, or always."),
        s("Tracer Speed", Key::TracerSpeed, 0.1f, 5.f, 0.05f, "%.2fx").extend(0.01f, 20.f).help("Times Weapon Effects'."),
        s("Tracer Length", Key::TracerLength, 0.1f, 5.f, 0.05f, "%.2fx").extend(0.01f, 20.f).help("Times Weapon Effects'."),
        s("Tracer Thickness", Key::TracerWidth, 0.1f, 5.f, 0.05f, "%.2fx").extend(0.01f, 20.f).help("Times Weapon Effects'."),
        s("Tracer Chance", Key::TracerChance, 0.f, 4.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("Times Weapon Effects' Chance a Pellet (at most every pellet)."),
        cycle("Tracer Colour", weapons::cvar(slot, Key::TracerOwnColour), {{0.f, "Weapon Effects'"}, {1.f, "Its Own"}})
            .help("Its Own: the Red, Green and Blue below."),
        s("Tracer Red", Key::TracerRed, 0.f, 1.f, 0.05f, "%.2f"),
        s("Tracer Green", Key::TracerGreen, 0.f, 1.f, 0.05f, "%.2f"),
        s("Tracer Blue", Key::TracerBlue, 0.f, 1.f, 0.05f, "%.2f"),
        action("Test the Effects", weaponOffsetsTestEffects)
            .help("This weapon kicks and flashes as if it fired, with 3 tracers (no shot: vr_weaponfx_test)."),
        open("Weapon Effects (All Weapons)", pageIndex(pageWeaponEffects))
    );
    return list;
}

za::Vector<Item> pageWofsFlashlight()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsFlashlight);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    const auto s = weaponOffsetsSlider;
    const char* moveHelp = "Moves the flashlight clipped on this weapon (metres): forward along the barrel, up, and out away "
                           "from your body; on top of every weapon's On Gun Forward, Up and Out and the place fitted to the "
                           "model. The zone that clips it on (B or Y, or letting go by the gun) moves with it.";
    const char* turnHelp = "Turns the flashlight clipped on this weapon about its middle (degrees): Pitch tips the beam up, Yaw "
                           "turns it out away from your body, Roll tips its top out (the off hand's mirrored).";
    list.pushBackMultiple(
        header("Flashlight on This Weapon"),
        cycle("Flashlight Can Clip On", weapons::cvar(slot, Key::TorchClip), {{0.f, "No"}, {1.f, "Yes"}})
            .help("Whether the flashlight clips on this weapon at all: its zone, B or Y by it, and letting go by it (Clip on Gun "
                  "When Let Go). Off by default for the melee weapons (the axe, Mjolnir, the swords, the chainsaw, the crowbar)."),
        action("Clip the Flashlight on It", weaponOffsetsClipTorch)
            .help("Puts the flashlight (switched on) on this weapon now, to tune it in place (vr_flashlight_clip_gun)."),
        toggle("Show the Flashlight's Place", vr_flashlight_mount_preview)
            .help("While this page is shown: the flashlight's outline and its beam's line on this weapon (cyan) and the zone "
                  "that clips it on (orange), wherever the flashlight is. The flashlight must be on (Flashlight)."),
        s("Flashlight Forward", Key::TorchForward, -0.2f, 0.2f, 0.005f, "%+.3f m").extend(-1.f, 1.f).help(moveHelp),
        s("Flashlight Up", Key::TorchUp, -0.2f, 0.2f, 0.005f, "%+.3f m").extend(-1.f, 1.f).help(moveHelp),
        s("Flashlight Out", Key::TorchOut, -0.2f, 0.2f, 0.005f, "%+.3f m").extend(-1.f, 1.f).help(moveHelp),
        s("Flashlight Pitch (up)", Key::TorchPitch, -45.f, 45.f, 0.5f, "%+.1f deg").extend(-180.f, 180.f).help(turnHelp),
        s("Flashlight Yaw (out)", Key::TorchYaw, -45.f, 45.f, 0.5f, "%+.1f deg").extend(-180.f, 180.f).help(turnHelp),
        s("Flashlight Roll (out)", Key::TorchRoll, -90.f, 90.f, 0.5f, "%+.1f deg").extend(-180.f, 180.f).help(turnHelp),
        action("Flashlight Back to 0", weaponOffsetsTorchReset).help("This weapon's own flashlight place back to 0: as every "
                                                                     "weapon's."),
        open("On a Gun or Head (Every Weapon)", pageIndex(pageFlashlightMounts))
            .help("Every weapon's place for the flashlight and the zone that clips it on, its size (Flashlight).")
    );
    return list;
}

// (The page shown, the Back stack: MenuInstance::page, nav.)
