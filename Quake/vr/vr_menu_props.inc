
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/String/ToString.hpp"

// vr_menu_props.inc -- the weight's readouts and pages (included by vr_menu.cpp): Weapon Weights (the weight of the
// weapon a hand holds: vr_weapons.inc's weight keys), and Held Object Offsets and Held Object Weights (the prop a hand
// holds: vr_props.inc; its grip, fingers and melee points on one, its weight on the other), as Weapon Offsets is for a
// weapon's placement. See docs/vr-port/ROUND21.md, "Weight: spring model, stamina, held object offsets; explosive boxes"
// and "Spring only; Weapon Weights and Held Object Weights; weight and damage".

[[nodiscard]] za::Vector<Item> pageHeldObjectOffsets();
[[nodiscard]] za::Vector<Item> pageHeldObjectWeights();
[[nodiscard]] za::Vector<Item> pageWeaponWeights();
void showPage(int target);

// "Main: 8.0 kg x1.00, 1.2 cm 3.4 deg": what a hand holds as the weight sees it (the spring's offset from the hand).
[[nodiscard]] const char* weightReadout(int hand)
{
    za::String (&text)[2] = readouts.weight;
    const weight::Load l = weight::load(hand);
    const char* name = hand == 1 ? "Main" : "Off";
    if(!l.valid)
    {
        text[hand] = za::String(name) + ": nothing heavy (stamina x" + za::StringView{va("%f", l.staminaMult)}.substrByPosLen(0, 4) + ")"; // (std::to_string's %f, cut to 4)
        return text[hand].cStr();
    }
    const weight::Offset o = weight::offset(hand);
    if(l.empty)
    {
        text[hand] = va("%s: empty, tired: %.1f kg", name, l.mass);
    }
    else
    {
        text[hand] = va("%s: %.1f kg x%.2f%s%s", name, l.mass, l.staminaMult, l.staminaAdd > 0.05f ? va(" +%.1f kg", l.staminaAdd) : "",
            l.twoHanded > 0.5f ? " 2H" : "");
    }
    if(o.active)
    {
        text[hand] += va(", %.1f cm %.1f deg", o.distance, o.angle);
    }
    return text[hand].cStr();
}

// What a thing of `mass` kg with its own Melee and Throw Damage x deals, by the weight's curve (vr_weight_damage_*), and
// its strikes' and throws' speed thresholds (heavy leniency, vr_weight_lenient*), on two lines: `line` 0 "Damage: melee
// x1.90, thrown x1.90", 1 "Weight: damage x1.90, speeds x0.35".
[[nodiscard]] za::String damageReadout(float mass, float ownMelee, float ownThrow, int line)
{
    const float curve = weight::damageMultiplier(mass);
    return line == 0 ? va("Damage: melee x%.2f, thrown x%.2f", curve * ownMelee, curve * ownThrow)
                     : va("Weight: damage x%.2f, speeds x%.2f", curve, weight::leniency(mass));
}

// A spring multiplier's slider (Weapon Weights, Held Object Weights): times the global setting on the Aiming page.
[[nodiscard]] Item springSlider(const char* label, cvar_t* var, float max, const char* help)
{
    return slider(label, var, 0.f, max, 0.05f, "%.2fx").extend(0.f, 20.f).help(help);
}

// ---------------------------------------------------------------------------------------------------------------------
// Weapon Weights: the weight of the weapon a hand holds (vr_wofs_<weight key>_NN of its slot, or of the weapon it
// inherits from), built anew each time the page is shown, as the weapon in hand changes.

int weaponWeightsHand = 1; // 1 main hand, 0 off hand
bool weaponWeightsStale = true;
bool weaponWeightsKeepHand = false; // Edit the Other Hand's Weapon: that hand, even empty
int weaponWeightsSlot = -1;         // the slot edited (what the weapon in hand inherits from, or its own)

void weaponWeightsOtherHand()
{
    weaponWeightsHand = 1 - weaponWeightsHand;
    weaponWeightsKeepHand = true;
    weaponWeightsStale = true;
    showPage(pageIndex(pageWeaponWeights));
}

void weaponWeightsReset()
{
    weapons::resetSlotToDefaults(weaponWeightsSlot, weapons::Part::Weights);
}

void weaponWeightsPrint()
{
    weapons::printSlot(weaponWeightsSlot, weapons::Part::Weights);
}

[[nodiscard]] const char* weaponWeightsHandReadout(int /* unused */)
{
    return weightReadout(weaponWeightsHand);
}

[[nodiscard]] const char* weaponWeightsDamageReadout(int line)
{
    za::String (&text)[2] = readouts.weaponWeightsDamage;
    using weapons::Key;
    const int slot = weaponWeightsSlot;
    text[line] = damageReadout(weapons::value(slot, Key::Mass), weapons::value(slot, Key::MeleeDamage),
        weapons::value(slot, Key::ThrowDamage), line);
    return text[line].cStr();
}

// Wrenched out: the edited weapon's limits (vr_weight_drop*), one hand and two.
[[nodiscard]] const char* weaponWeightsDropReadout(int /* unused */)
{
    za::String& text = readouts.weaponWeightsDrop;
    const float mass = weapons::value(weaponWeightsSlot, weapons::Key::Mass);
    const float one = weight::dropLimit(mass, 0.f), two = weight::dropLimit(mass, 1.f);
    text = !vr_weight_drop.value ? "This weapon: never falls (off)"
           : one <= 0.f          ? va("This weapon: never falls (%.1f kg, lighter than %.1f)", mass, vr_weight_drop_from.value)
                                 : va("This weapon: falls turned faster than %.0f deg/s (%.0f with two hands)", one, two);
    return text.cStr();
}

// A hand's weapon slot (the empty hand's: the hand model's; -1: not in a game).
[[nodiscard]] int weaponWeightsSlotOf(int hand)
{
    if(!vrActive() && cls.state != ca_connected)
    {
        return -1;
    }
    const int slot = weapons::heldSlot(hand);
    return slot < 0 && cls.state == ca_connected ? weapons::fistSlot() : slot;
}

za::Vector<Item> pageWeaponWeights()
{
    using weapons::Key;
    za::String& title = pageTexts.weaponWeightsTitle;
    za::String& inheritTitle = pageTexts.weaponWeightsInheritTitle;
    // Opened with the chosen hand empty and a weapon in the other: that one.
    if(!weaponWeightsKeepHand)
    {
        const int here = weaponWeightsSlotOf(weaponWeightsHand), there = weaponWeightsSlotOf(1 - weaponWeightsHand);
        if((here < 0 || here == weapons::fistSlot()) && there >= 0 && there != weapons::fistSlot())
        {
            weaponWeightsHand = 1 - weaponWeightsHand;
        }
    }
    weaponWeightsKeepHand = false;
    const int held = weaponWeightsSlotOf(weaponWeightsHand);
    const char* hand = weaponWeightsHand == 1 ? "Main hand" : "Off hand";
    za::Vector<Item> list;
    weaponWeightsSlot = held;
    if(held < 0)
    {
        title = za::String(hand) + ": hold a weapon in a game to adjust it";
        list.pushBack(header(title.cStr()));
        list.pushBack(action("Edit the Other Hand's Weapon", weaponWeightsOtherHand));
        return list;
    }
    title = za::String(hand) + ": " + weapons::cvar(held, Key::ID)->string + " (_" + (held + 1 < 10 ? "0" : "") +
            za::toString(held + 1) + ")";
    list = {
        header(title.cStr()),
        action("Edit the Other Hand's Weapon", weaponWeightsOtherHand)
            .help("The page shows the weapon the hand held when it was opened: reopen it after changing weapons."),
    };
    // A weapon inheriting another's settings (Weapon Offsets: Inherit From): the page edits those.
    if(const int from = weapons::inheritsFrom(held); from >= 0)
    {
        weaponWeightsSlot = from;
        inheritTitle = za::String("Settings of ") + weapons::cvar(from, Key::ID)->string + " (inherited)";
        list.pushBack(header(inheritTitle.cStr()));
        list.pushBack(info([]() -> const char* { return "Weapon Offsets: Inherit From None to give it its own."; }));
    }
    const int slot = weaponWeightsSlot;
    const auto s = [&](const char* label, Key key, float min, float max, float step, const char* format) {
        return slider(label, weapons::cvar(slot, key), min, max, step, format);
    };
    list.pushBackMultiple(
        infoLine(weaponWeightsHandReadout, 0),
        header("Weight"),
        s("Mass", Key::Mass, 0.f, 20.f, 0.1f, "%.1f kg").extend(0.f, 200.f)
            .help("How heavy it is: it lags your hand, overshoots and settles (Aiming: Spring), and it strikes and is "
                  "thrown harder or softer (Aiming: Weight and Damage). 0: none, as the empty hand."),
        s("Balance", Key::Balance, -20.f, 60.f, 1.f, "%.0f cm").extend(-200.f, 200.f)
            .help("Its centre of mass ahead of the grip, along the aim (an axe's head far out): it droops and swings about "
                  "your hand towards it."),
        s("Length", Key::Span, 10.f, 200.f, 5.f, "%.0f cm").extend(1.f, 500.f)
            .help("Its length, for how sluggishly it turns (a long one slower to pitch and yaw)."),
        header("Spring (times the Aiming page's)"),
        springSlider("Stiffness", weapons::cvar(slot, Key::SpringStiffness), 4.f,
            "Times Aiming: Stiffness, for this weapon: higher follows your hand more firmly."),
        springSlider("Damping", weapons::cvar(slot, Key::SpringDamping), 3.f,
            "Times Aiming: Damping, for this weapon: lower overshoots and wobbles, higher creeps in."),
        springSlider("Arm Strength", weapons::cvar(slot, Key::SpringStrength), 4.f,
            "Times Aiming: Arm Strength, for this weapon: the most force and torque your arm gives it."),
        springSlider("Sag", weapons::cvar(slot, Key::SpringSag), 4.f,
            "Times Aiming: Sag, for this weapon: how much it droops under its weight."),
        springSlider("Swing", weapons::cvar(slot, Key::SpringSwing), 4.f,
            "Times Aiming: Swing, for this weapon: how much a moving hand swings it about the grip."),
        springSlider("Two-Handed Help", weapons::cvar(slot, Key::SpringTwoHanded), 4.f,
            "Times Aiming: Two-Handed Help, for this weapon: how much steadier both hands hold it."),
        springSlider("Snap Back Beyond", weapons::cvar(slot, Key::SpringSnap), 4.f,
            "Times Aiming: Snap Back Beyond, for this weapon: how far behind it may fall before it is put back in your "
            "hand."),
        header("Damage"),
        infoLine(weaponWeightsDamageReadout, 0),
        infoLine(weaponWeightsDamageReadout, 1),
        s("Melee Damage", Key::MeleeDamage, 0.f, 4.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("Times this weapon's melee damage (its blows with any part of it), on top of the weight's (the readout "
                  "above; Aiming: Weight and Damage)."),
        s("Throw Damage", Key::ThrowDamage, 0.f, 4.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("Times this weapon's damage thrown, on top of the weight's."),
        header("Thrown"),
        s("Spin in the Air", Key::SpinAlign, 0.f, 4.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("Times Throwing and Physics: Spin Alignment for this weapon thrown: how fast its tumble settles into a "
                  "clean spin end over end (an axe's, in its blade's plane). 0: it tumbles as it left your hand."),
        header("Wrenched Out (experimental, every weapon)"),
        infoLine(weaponWeightsDropReadout, 0),
        toggle("Heavy Weapons Wrenched Out", vr_weight_drop)
            .help("A very heavy weapon falls out of your hand when you snap or twist your wrist faster than it could "
                  "follow. Turning with the stick and walking don't count; only how fast the controller itself turns."),
        slider("Heavier Than", vr_weight_drop_from, 1.f, 40.f, 0.5f, "%.1f kg").extend(0.1f, 500.f)
            .help("Only weapons at least this heavy can fall (the laser cannon and the chainsaw weigh 15 kg; every other "
                  "weapon 8 or less)."),
        slider("Fastest Turn", vr_weight_drop_speed, 300.f, 5000.f, 50.f, "%.0f deg/s").extend(10.f, 20000.f)
            .help("How fast a weapon of the weight above may turn in one hand before it falls (over the time below). Your "
                  "recorded sword slashes turned the controller at up to 2100 degrees a second, slow waving 1000, "
                  "reloads 950, aiming 400."),
        slider("Weight Curve", vr_weight_drop_curve, 0.f, 3.f, 0.05f, "%.2f").extend(0.f, 10.f)
            .help("Heavier weapons fall at slower turns: the speed above times (Heavier Than / mass)^this. 1: in "
                  "proportion (15 kg: two thirds of it); 0: every heavy weapon the same."),
        slider("Two Hands", vr_weight_drop_2h, 1.f, 5.f, 0.1f, "%.1fx").extend(1.f, 50.f)
            .help("Times the speed with the other hand on it too."),
        slider("Measured Over", vr_weight_drop_window, 0.02f, 0.2f, 0.01f, "%.2f s").extend(0.005f, 0.5f)
            .help("The time the turn is measured over: shorter catches shorter snaps (and more of a tracking jitter)."),
        header("This Weapon"),
        action("Print Changes to Console", weaponWeightsPrint)
            .help("Prints this weapon's weight settings that differ from the defaults, ready to be made the shipped "
                  "defaults (vr_weapons.inc)."),
        action("Reset This Weapon", weaponWeightsReset)
            .help("This weapon's weight settings (this page's) back to their defaults. Its offsets stay.")
    );
    return list;
}

// ---------------------------------------------------------------------------------------------------------------------
// Held Object Offsets and Held Object Weights: the prop a hand holds (vr_prop_<key>_NN of its model's slot).

int heldObjectHand = 1; // 1 main hand, 0 off hand
int heldObjectSlot = -1;
bool heldObjectStale = true;
bool heldObjectKeepHand = false; // Edit the Other Hand's Prop: that hand, even empty
za::String heldObjectModel;

// The prop `hand` holds: a carried thing's model, or the flashlight's ("": none).
[[nodiscard]] za::String heldPropModel(int hand)
{
    if(cls.state != ca_connected)
    {
        return {};
    }
    if(const int ent = held::heldEntity(hand); ent && cl_entities[ent].model)
    {
        return cl_entities[ent].model->name;
    }
    return flashlight::holds(hand) ? "progs/vrflashlight.mdl" : "";
}

// The page's prop: the chosen hand's (opened with it empty and a prop in the other hand: that one), its slot claimed.
// Pushes the page's title; false: no prop to edit (the page is its title and the other hand's action).
bool heldObjectBegin(za::Vector<Item>& list, za::String& title, void (*otherHand)())
{
    if(!heldObjectKeepHand && heldPropModel(heldObjectHand).empty() && !heldPropModel(1 - heldObjectHand).empty())
    {
        heldObjectHand = 1 - heldObjectHand;
    }
    heldObjectKeepHand = false;
    heldObjectModel = heldPropModel(heldObjectHand);
    const char* hand = heldObjectHand == 1 ? "Main hand" : "Off hand";
    heldObjectSlot = heldObjectModel.empty() ? -1 : props::claimSlot(heldObjectModel.cStr());
    if(heldObjectSlot < 0)
    {
        title = za::String(hand) + (heldObjectModel.empty() ? ": hold a prop in a game to adjust it" : ": no free prop slot");
        list.pushBack(header(title.cStr()));
        list.pushBack(action("Edit the Other Hand's Prop", otherHand));
        return false;
    }
    const int slot = heldObjectSlot;
    title = za::String(hand) + ": " + heldObjectModel + " (_" + (slot + 1 < 10 ? "0" : "") + za::toString(slot + 1) + ")";
    list.pushBack(header(title.cStr()));
    list.pushBack(action("Edit the Other Hand's Prop", otherHand)
                       .help("The page shows what the hand held when it was opened: reopen it after taking something else."));
    return true;
}

void heldObjectOtherHand()
{
    heldObjectHand = 1 - heldObjectHand;
    heldObjectKeepHand = true;
    heldObjectStale = true;
    showPage(pageIndex(pageHeldObjectOffsets));
}

void heldObjectWeightsOtherHand()
{
    heldObjectHand = 1 - heldObjectHand;
    heldObjectKeepHand = true;
    heldObjectStale = true;
    showPage(pageIndex(pageHeldObjectWeights));
}

void heldObjectReset()
{
    props::resetSlotToDefaults(heldObjectSlot, props::Part::Offsets);
}

void heldObjectPrint()
{
    props::printSlot(heldObjectSlot, props::Part::Offsets);
}

void heldObjectWeightsReset()
{
    props::resetSlotToDefaults(heldObjectSlot, props::Part::Weights);
}

void heldObjectWeightsPrint()
{
    props::printSlot(heldObjectSlot, props::Part::Weights);
}

[[nodiscard]] const char* heldObjectMassReadout(int /* unused */)
{
    za::String& text = readouts.heldObjectMass;
    const weight::Load l = weight::load(heldObjectHand);
    if(!l.valid || strcmp(l.model, heldObjectModel.cStr()) != 0)
    {
        return "Mass now: (let go of)";
    }
    const bool set = props::valueFor(heldObjectModel.cStr(), props::Key::Mass) > 0.f;
    text = va("Mass now: %.1f kg (%s)", l.mass, set ? "set" : "estimated");
    return text.cStr();
}

[[nodiscard]] const char* heldObjectDamageReadout(int line)
{
    za::String (&text)[2] = readouts.heldObjectDamage;
    const weight::Load l = weight::load(heldObjectHand);
    if(!l.valid || strcmp(l.model, heldObjectModel.cStr()) != 0)
    {
        return line == 0 ? "Damage: (let go of)" : "";
    }
    const int slot = heldObjectSlot;
    text[line] = damageReadout(l.mass, props::value(slot, props::Key::MeleeDamage), props::value(slot, props::Key::ThrowDamage), line);
    return text[line].cStr();
}

za::Vector<Item> pageHeldObjectOffsets()
{
    using props::Key;
    za::String& title = pageTexts.heldObjectOffsetsTitle;
    za::Vector<Item> list;
    if(!heldObjectBegin(list, title, heldObjectOtherHand))
    {
        return list;
    }
    const int slot = heldObjectSlot;
    const auto s = [&](const char* label, Key key, float min, float max, float step, const char* format) {
        return slider(label, props::cvar(slot, key), min, max, step, format);
    };
    if(heldObjectModel == "progs/vrflashlight.mdl")
    {
        list.pushBack(info([]() -> const char* { return "Its grip and fingers: the Flashlight page; its weight: Held Object Weights."; }));
        return list;
    }
    list.pushBackMultiple(
        open("Held Object Weights", pageIndex(pageHeldObjectWeights)).help("Its mass, how it follows your hand and its damage."),
        s("Size", Key::Size, 0.25f, 3.f, 0.05f, "%.2fx").extend(0.05f, 10.f)
            .help("Every one of these props, drawn this much bigger or smaller, about its origin: its physics body and "
                  "box too, and its lengths below (Tip, Butt, Handle, Centre of Mass, a Fixed grip's place). Its Mass "
                  "setting stays as set (an estimated mass follows the size). Live."),
        header("Grip"),
        cycle("Grip", props::cvar(slot, Key::GripMode),
            {{0.f, "Where taken"}, {1.f, "Fixed"}, {2.f, "In the palm"}, {3.f, "Along the handle"}})
            .help("Where taken: as you took it. Fixed: always the same, as below. In the palm: its middle in your palm, "
                  "the fingers round it (a rock). Along the handle: its handle through your fist where you took it (a "
                  "torch). Changes the prop in your hand at once."),
        s("X (forward)", Key::GripX, -40.f, 40.f, 0.5f, "%+.1f").extend(-200.f, 200.f)
            .help("Fixed: its model's origin in your right hand, units (x forward, y left, z up; the left hand's "
                  "mirrored). The other grips: moves it that far in your hand, the hand staying where it is. Live."),
        s("Y (left)", Key::GripY, -40.f, 40.f, 0.5f, "%+.1f").extend(-200.f, 200.f),
        s("Z (up)", Key::GripZ, -40.f, 40.f, 0.5f, "%+.1f").extend(-200.f, 200.f),
        s("Pitch (up)", Key::GripPitch, -180.f, 180.f, 5.f, "%+.0f")
            .help("Fixed: its model's angles off your hand's. The other grips: turns it in your hand about where you "
                  "hold it (your palm; in the palm, its middle; a handle, your fist). Live."),
        s("Yaw (left)", Key::GripYaw, -180.f, 180.f, 5.f, "%+.0f"),
        s("Roll (right)", Key::GripRoll, -180.f, 180.f, 5.f, "%+.0f"),
        s("Handle From", Key::HandleFrom, -40.f, 40.f, 0.2f, "%+.1f").extend(-200.f, 200.f)
            .help("Along the handle: the part of it your fist may hold, units along its long axis from its model's "
                  "origin. Both 0: its butt's half."),
        s("Handle To", Key::HandleTo, -40.f, 40.f, 0.2f, "%+.1f").extend(-200.f, 200.f),
        s("Handle Tilt", Key::HandleTilt, 0.f, 90.f, 5.f, "%.0f deg")
            .help("Along the handle: how far it may lean off the line through your fist, as you took it (0: always "
                  "straight through)."),
        header("Fingers"),
        cycle("Fingers", props::cvar(slot, Key::FingerManual), {{0.f, "Fitted"}, {1.f, "Manual"}})
            .help("Fitted: they wrap it. Manual: each at its curl below."),
        s("Thumb Curl", Key::FingerCurlThumb, 0.f, 1.f, 0.05f, "%.2f"),
        s("Index Curl", Key::FingerCurlIndex, 0.f, 1.f, 0.05f, "%.2f"),
        s("Middle Curl", Key::FingerCurlMiddle, 0.f, 1.f, 0.05f, "%.2f"),
        s("Ring Curl", Key::FingerCurlRing, 0.f, 1.f, 0.05f, "%.2f"),
        s("Little Curl", Key::FingerCurlPinky, 0.f, 1.f, 0.05f, "%.2f"),
        s("Thumb Across", Key::FingerThumbAcross, 0.f, 1.f, 0.05f, "%.2f"),
        [&] {
            Item i = s("Overlap", Key::Overlap, -1.f, 1.f, 0.05f, "%.2f cm");
            i.negativeLabel = "Global";
            return i.help("Fitted: how far the fingers and palm may sink into it. Global: Hand/Gun Calibration's Fit "
                          "Overlap: Things.");
        }(),
        header("Carrying"),
        toggle("Two Hands", *props::cvar(slot, Key::TwoHands))
            .help("The other hand may hold it too (Carrying and Gibs: Two-Handed Carrying)."),
        cycle("Force Grab", props::cvar(slot, Key::ForceGrab), {{-1.f, "As the game has it"}, {0.f, "Never"}}),
        header("Melee"),
        s("Tip X", Key::TipX, -40.f, 40.f, 0.5f, "%+.1f").extend(-200.f, 200.f)
            .help("Its striking end (a torch's head), units along its model's axes. All three 0: it strikes as the "
                  "fist holding it (a box in hand). Set: it is a club, striking along a line from its butt through "
                  "your hand to here (a wall torch: its head, which also burns)."),
        s("Tip Y", Key::TipY, -40.f, 40.f, 0.5f, "%+.1f").extend(-200.f, 200.f),
        s("Tip Z", Key::TipZ, -40.f, 40.f, 0.5f, "%+.1f").extend(-200.f, 200.f),
        s("Butt X", Key::ButtX, -40.f, 40.f, 0.5f, "%+.1f").extend(-200.f, 200.f)
            .help("Its other end (a pommel strike's), units along its model's axes. All three 0: a quarter of the "
                  "way from the hand to the tip, behind the hand."),
        s("Butt Y", Key::ButtY, -40.f, 40.f, 0.5f, "%+.1f").extend(-200.f, 200.f),
        s("Butt Z", Key::ButtZ, -40.f, 40.f, 0.5f, "%+.1f").extend(-200.f, 200.f),
        header("This Prop"),
        action("Print Changes to Console", heldObjectPrint)
            .help("Prints this prop's offsets that differ from the defaults, ready to be made the shipped defaults "
                  "(vr_props.inc)."),
        action("Reset This Prop", heldObjectReset)
            .help("This prop's offsets (this page's) back to their defaults. Its weight settings stay.")
    );
    return list;
}

za::Vector<Item> pageHeldObjectWeights()
{
    using props::Key;
    za::String& title = pageTexts.heldObjectWeightsTitle;
    za::Vector<Item> list;
    if(!heldObjectBegin(list, title, heldObjectWeightsOtherHand))
    {
        return list;
    }
    const int slot = heldObjectSlot;
    const auto s = [&](const char* label, Key key, float min, float max, float step, const char* format) {
        return slider(label, props::cvar(slot, key), min, max, step, format);
    };
    const bool flashlightProp = heldObjectModel == "progs/vrflashlight.mdl";
    list.pushBackMultiple(
        infoLine(heldObjectMassReadout, 0),
        header("Weight"),
        s("Mass", Key::Mass, 0.f, 100.f, 0.5f, "%.1f kg").extend(0.f, 2000.f)
            .help("0: estimated from its size and what it is made of (the readout above). Also its mass in the physics "
                  "world (what knocks it how far), how it follows your hand, and how hard it strikes and is thrown "
                  "(Aiming: Weight and Damage)."),
        s("Inertia", Key::Inertia, 0.1f, 4.f, 0.1f, "%.1fx").extend(0.f, 20.f)
            .help("How sluggishly it turns in the hand, times its shape's."),
        s("Centre of Mass X", Key::ComX, -30.f, 30.f, 0.5f, "%+.1f").extend(-200.f, 200.f)
            .help("Units off the middle of its model, along its own axes: where its weight is (a torch's head). With "
                  "Sag, it droops about the hand towards it."),
        s("Centre of Mass Y", Key::ComY, -30.f, 30.f, 0.5f, "%+.1f").extend(-200.f, 200.f),
        s("Centre of Mass Z", Key::ComZ, -30.f, 30.f, 0.5f, "%+.1f").extend(-200.f, 200.f)
    );
    if(!flashlightProp)
    {
        list.pushBack(s("Throw", Key::Throw, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 10.f)
                           .help("How much of your hand's throw it keeps. 0: from its mass (all of it up to 12 kg, less "
                                 "beyond: vr_weight_throw_mass)."));
    }
    list.pushBackMultiple(
        header("Spring (times the Aiming page's)"),
        springSlider("Stiffness", props::cvar(slot, Key::SpringStiffness), 4.f,
            "Times Aiming: Stiffness, for this prop: higher follows your hand more firmly."),
        springSlider("Damping", props::cvar(slot, Key::SpringDamping), 3.f,
            "Times Aiming: Damping, for this prop: lower overshoots and wobbles, higher creeps in."),
        springSlider("Arm Strength", props::cvar(slot, Key::SpringStrength), 4.f,
            "Times Aiming: Arm Strength, for this prop: the most force and torque your arm gives it."),
        springSlider("Sag", props::cvar(slot, Key::SpringSag), 4.f,
            "Times Aiming: Sag, for this prop: how much it droops under its weight."),
        springSlider("Swing", props::cvar(slot, Key::SpringSwing), 4.f,
            "Times Aiming: Swing, for this prop: how much a moving hand swings it about the grip."),
        springSlider("Two-Handed Help", props::cvar(slot, Key::SpringTwoHanded), 4.f,
            "Times Aiming: Two-Handed Help, for this prop: how much steadier both hands hold it."),
        springSlider("Snap Back Beyond", props::cvar(slot, Key::SpringSnap), 4.f,
            "Times Aiming: Snap Back Beyond, for this prop: how far behind it may fall before it is put back in your "
            "hand.")
    );
    if(!flashlightProp)
    {
        list.pushBackMultiple(
            header("Damage"),
            infoLine(heldObjectDamageReadout, 0),
            infoLine(heldObjectDamageReadout, 1),
            s("Melee Damage", Key::MeleeDamage, 0.f, 4.f, 0.05f, "%.2fx").extend(0.f, 20.f)
                .help("Times its melee damage (a blow with it in hand), on top of the weight's (the readout above; "
                      "Aiming: Weight and Damage)."),
            s("Throw Damage", Key::ThrowDamage, 0.f, 4.f, 0.05f, "%.2fx").extend(0.f, 20.f)
                .help("Times its damage thrown, on top of the weight's.")
        );
    }
    list.pushBackMultiple(
        header("Thrown"),
        s("Spin in the Air", Key::SpinAlign, 0.f, 4.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("Times Throwing and Physics: Spin Alignment for this prop thrown: how fast its tumble settles into a "
                  "clean spin end over end about its steadiest axis (a long flat thing the most, a long round one a "
                  "tenth as much, a box not at all). 0: it tumbles as it left your hand.")
    );
    list.pushBackMultiple(
        header("This Prop"),
        open("Held Object Offsets", pageIndex(pageHeldObjectOffsets)).help("Its grip, fingers and melee points."),
        action("Print Changes to Console", heldObjectWeightsPrint)
            .help("Prints this prop's weight settings that differ from the defaults, ready to be made the shipped "
                  "defaults (vr_props.inc)."),
        action("Reset This Prop", heldObjectWeightsReset)
            .help("This prop's weight settings (this page's) back to their defaults. Its offsets stay.")
    );
    return list;
}

// The weight pages' rebuilds (items(), vr_menu.cpp): a page showing what a hand held when it was opened is built anew
// when it is shown again, or its hand changed.
[[nodiscard]] bool weightPageStale(PageBuilder build)
{
    if((build == pageHeldObjectOffsets || build == pageHeldObjectWeights) && heldObjectStale)
    {
        heldObjectStale = false;
        return true;
    }
    if(build == pageWeaponWeights && weaponWeightsStale)
    {
        weaponWeightsStale = false;
        return true;
    }
    return false;
}

// showPage (vr_menu.cpp): shown afresh, a weight page takes what is in the hand now.
void weightPageShown(PageBuilder build)
{
    if(build == pageHeldObjectOffsets || build == pageHeldObjectWeights)
    {
        heldObjectStale = true;
    }
    if(build == pageWeaponWeights)
    {
        weaponWeightsStale = true;
    }
}
