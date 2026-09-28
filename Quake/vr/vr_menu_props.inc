// vr_menu_props.inc -- the weight's readouts and the Held Object Offsets page (included by vr_menu.cpp): the settings of
// the prop a hand holds (vr_props.inc: weight, grip, fingers, carrying), as Weapon Offsets is for a weapon. See
// docs/vr-port/ROUND21.md, "Weight: spring model, stamina, held object offsets; explosive boxes".

[[nodiscard]] std::vector<Item> pageHeldObjectOffsets();
void showPage(int target);

// "Main: 8.0 kg x1.00, 1.2 cm 3.4 deg": what a hand holds as the weight sees it (the Spring's offset from the hand).
[[nodiscard]] const char* weightReadout(int hand)
{
    static std::string text[2];
    const weight::Load l = weight::load(hand);
    const char* name = hand == 1 ? "Main" : "Off";
    if(!l.valid)
    {
        text[hand] = std::string(name) + ": nothing heavy (stamina x" + std::to_string(l.staminaMult).substr(0, 4) + ")";
        return text[hand].c_str();
    }
    const weight::Offset o = weight::offset(hand);
    text[hand] = va("%s: %.1f kg x%.2f%s", name, l.mass, l.staminaMult, l.twoHanded > 0.5f ? " 2H" : "");
    if(o.active)
    {
        text[hand] += va(", %.1f cm %.1f deg", o.distance, o.angle);
    }
    return text[hand].c_str();
}

int heldObjectHand = 1; // 1 main hand, 0 off hand
int heldObjectSlot = -1;
bool heldObjectStale = true;
std::string heldObjectModel;

// The prop `hand` holds: a carried thing's model, or the flashlight's ("": none).
[[nodiscard]] std::string heldPropModel(int hand)
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

void heldObjectOtherHand()
{
    heldObjectHand = 1 - heldObjectHand;
    heldObjectStale = true;
    showPage(pageIndex(pageHeldObjectOffsets));
}

void heldObjectReset()
{
    props::resetSlotToDefaults(heldObjectSlot);
}

void heldObjectPrint()
{
    props::printSlot(heldObjectSlot);
}

[[nodiscard]] const char* heldObjectMassReadout(int /* unused */)
{
    static std::string text;
    const weight::Load l = weight::load(heldObjectHand);
    if(!l.valid || strcmp(l.model, heldObjectModel.c_str()) != 0)
    {
        return "Mass now: (let go of)";
    }
    const bool set = props::valueFor(heldObjectModel.c_str(), props::Key::Mass) > 0.f;
    text = va("Mass now: %.1f kg (%s)", l.mass, set ? "set" : "estimated");
    return text.c_str();
}

std::vector<Item> pageHeldObjectOffsets()
{
    using props::Key;
    static std::string title;
    heldObjectModel = heldPropModel(heldObjectHand);
    const char* hand = heldObjectHand == 1 ? "Main hand" : "Off hand";
    std::vector<Item> list;
    heldObjectSlot = heldObjectModel.empty() ? -1 : props::claimSlot(heldObjectModel.c_str());
    if(heldObjectSlot < 0)
    {
        title = std::string(hand) + (heldObjectModel.empty() ? ": hold a prop in a game to adjust it" : ": no free prop slot");
        list.push_back(header(title.c_str()));
        list.push_back(action("Edit the Other Hand's Prop", heldObjectOtherHand));
        return list;
    }
    const int slot = heldObjectSlot;
    title = std::string(hand) + ": " + heldObjectModel + " (_" + (slot + 1 < 10 ? "0" : "") + std::to_string(slot + 1) + ")";
    const auto s = [&](const char* label, Key key, float min, float max, float step, const char* format) {
        return slider(label, props::cvar(slot, key), min, max, step, format);
    };
    const bool torch = heldObjectModel == "progs/vrflashlight.mdl";
    list = {
        header(title.c_str()),
        action("Edit the Other Hand's Prop", heldObjectOtherHand)
            .help("The page shows what the hand held when it was opened: reopen it after taking something else."),
        infoLine(heldObjectMassReadout, 0),
        header("Weight"),
        s("Mass", Key::Mass, 0.f, 100.f, 0.5f, "%.1f kg").extend(0.f, 2000.f)
            .help("0: estimated from its size and what it is made of (the readout above). Also its mass in the physics "
                  "world: what knocks it how far."),
        s("Inertia", Key::Inertia, 0.1f, 4.f, 0.1f, "%.1fx").extend(0.f, 20.f)
            .help("How sluggishly it turns in the hand (Spring), times its shape's."),
        s("Centre of Mass X", Key::ComX, -30.f, 30.f, 0.5f, "%+.1f").extend(-200.f, 200.f)
            .help("Units off the middle of its model, along its own axes: where its weight is (a torch's head). With "
                  "Sag, it droops about the hand towards it."),
        s("Centre of Mass Y", Key::ComY, -30.f, 30.f, 0.5f, "%+.1f").extend(-200.f, 200.f),
        s("Centre of Mass Z", Key::ComZ, -30.f, 30.f, 0.5f, "%+.1f").extend(-200.f, 200.f),
    };
    if(!torch)
    {
        list.insert(list.end(), {
            s("Throw", Key::Throw, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 10.f)
                .help("How much of your hand's throw it keeps. 0: from its mass (all of it up to 12 kg, less beyond: "
                      "vr_weight_throw_mass)."),
            s("Blunt", Key::Blunt, 0.f, 3.f, 0.05f, "%.2f").extend(0.f, 10.f)
                .help("How hard it hits, punched with or thrown: this times the square root of its mass (kg) times a "
                      "punch's damage (held) or Thrown Box Damage (thrown). A rock of 1 kg at 1.5: 1.5 times; a brick of "
                      "2 kg: 2.1 times. 0: as a box (Box Punch Damage, Thrown Box Damage)."),
            header("Grip"),
            cycle("Grip", props::cvar(slot, Key::GripMode), {{0.f, "Where taken"}, {1.f, "Always the same"}})
                .help("Where taken: it stays in the hand as you took it. Always the same: it is held as below, whichever "
                      "part you take (a torch by its handle). From the next grab."),
            s("Grip X (forward)", Key::GripX, -40.f, 40.f, 0.5f, "%+.1f").extend(-200.f, 200.f)
                .help("Always the same: its model's origin in your right hand, units (x forward, y left, z up; the left "
                      "hand's mirrored)."),
            s("Grip Y (left)", Key::GripY, -40.f, 40.f, 0.5f, "%+.1f").extend(-200.f, 200.f),
            s("Grip Z (up)", Key::GripZ, -40.f, 40.f, 0.5f, "%+.1f").extend(-200.f, 200.f),
            s("Grip Pitch", Key::GripPitch, -180.f, 180.f, 5.f, "%+.0f"),
            s("Grip Yaw", Key::GripYaw, -180.f, 180.f, 5.f, "%+.0f"),
            s("Grip Roll", Key::GripRoll, -180.f, 180.f, 5.f, "%+.0f"),
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
        });
    }
    else
    {
        list.push_back(info([]() -> const char* { return "Its grip and fingers: the Flashlight page."; }));
    }
    list.insert(list.end(), {
        header("This Prop"),
        action("Print Changes to Console", heldObjectPrint)
            .help("Prints this prop's settings that differ from the defaults, ready to be made the shipped defaults "
                  "(vr_props.inc)."),
        action("Reset This Prop", heldObjectReset).help("Every setting of this prop back to its default."),
    });
    return list;
}
