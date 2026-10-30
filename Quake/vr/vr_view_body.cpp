// vr_view_body.cpp -- the view's body pieces (see vr_view.cpp): the skinned body, the wrist info, the blood dripping
// from the wounds, the pauldrons and the weapons' buttons.

#include "vr_view_internal.hpp"
#include "vr_geom.hpp"

namespace view_detail
{

// The skinned body (vr_body_mode 2 and 3; 1, the old floating torso, is 2), its arms reaching the
// drawn hands' wrists.
void setupBody(const hands::State& s)
{
    const int mode = static_cast<int>(vr_body_mode.value);
    entities.body.visible = false;
    avatar::hide();
    drawnBuild = 1;
    drawnBare = false;

    // Not while dead (the view lies on the floor) or at the end of a level.
    const bool alive = cl.stats[STAT_HEALTH] > 0 && !cl.intermission;
    if(mode >= 1 && alive)
    {
        // The build (vr_body_build): progs/vrbody_lean, vrbody (athletic), vrbody_brawny or vrbody_fem; their bare forearms'
        // (vr_body_bracers 0) if both hands are the jointed hand they are fitted round.
        const int build = avatar::bodyBuild();
        qmodel_t* model = nullptr;
        if(bareForearms() && rigHands[0].drawn && rigHands[1].drawn)
        {
            model = viewModel(bodyModelName(build, true));
            drawnBare = avatar::usable(model);
        }
        if(!drawnBare)
        {
            model = viewModel(bodyModelName(build, false));
        }
        if(!avatar::usable(model))
        {
            model = viewModel("progs/vrbody.mdl");
        }
        if(avatar::usable(model))
        {
            drawnBuild = model == viewModel("progs/vrbody.mdl") ? 1 : build;
            drawnBare = drawnBare && model == viewModel(bodyModelName(build, true));
            const avatar::HandPose handPoses[2] = {drawnHand(s, 0), drawnHand(s, 1)};

            view::ViewEntity& ve = entities.body;
            const glm::vec3 origin = avatar::pose(s, model, &ve.ent, handPoses, mode >= 3);
            place(ve, model, origin, glm::vec3{0.f}, 0, false);
            showPlayerState(ve, s);
        }
    }
}

// vr_body_wrist_info: per hand, how big the drawn hand is against the body (metres of the modelled body a hand unit, along
// the hand's x, y, z) and how far the body's posed wrist is from the hand's.
void wristInfo_f()
{
    const hands::State& s = hands::current();
    const float m2w = units::metresToUnits() * units::bodyScale();
    const gadget::Pose& mount = gadget::pose();
    Con_Printf("wrist mount: bracers %d, valid %d, origin %.5f %.5f %.5f, straps %d\n", drawnBare ? 0 : 1,
        mount.valid ? 1 : 0, mount.origin.x, mount.origin.y, mount.origin.z,
        static_cast<int>(entities.gadgetStrap[0].visible) + static_cast<int>(entities.gadgetStrap[1].visible));
    for(int hand = 0; hand < 2; hand++)
    {
        const view::ViewEntity& base = entities.hand[hand][FingerBase];
        if(!s.valid || !base.ent.model)
        {
            Con_Printf("wrist %d: no hand drawn\n", hand);
            continue;
        }
        const glm::vec3 w = rigHands[hand].drawn ? vec3Of(handrig::data::wrist) : glm::vec3{-6.86f, -1.08f, 1.42f};
        const glm::vec3 o = view::modelPoint(base, w);
        const glm::vec3 k{glm::distance(view::modelPoint(base, w + glm::vec3{1.f, 0.f, 0.f}), o) / m2w,
            glm::distance(view::modelPoint(base, w + glm::vec3{0.f, 1.f, 0.f}), o) / m2w,
            glm::distance(view::modelPoint(base, w + glm::vec3{0.f, 0.f, 1.f}), o) / m2w};
        avatar::ForearmFrame f;
        const float gap = entities.body.visible && avatar::forearmFrame(hand, 1.f, f) ? glm::distance(f.point, o) / m2w : -1.f;
        // The wrist from the controller in the play space's axes (as vr_mock_hand and vr_mock_camera take them: x right,
        // y up, -z forward), metres.
        const glm::vec3 d = (o - s.gripPos[hand]) / units::metresToUnits();
        const float yaw = glm::radians(s.turnYaw);
        const glm::vec3 fwd{za::cos(yaw), za::sin(yaw), 0.f}, right{za::sin(yaw), -za::cos(yaw), 0.f};
        // The forearm's direction there (play space) and the wrist's bend: the hand's forward against it, degrees.
        const glm::vec3 handFwd = glm::normalize(view::modelPoint(base, w + glm::vec3{1.f, 0.f, 0.f}) - o);
        const glm::vec3 fa = gap >= 0.f ? f.axes[0] : handFwd;
        Con_Printf("wrist %d: rig %d, body %s%s, m/unit %.5f %.5f %.5f, arm's wrist off the hand's %.2f mm; the wrist from "
                   "the controller %.3f %.3f %.3f m, the forearm along %.3f %.3f %.3f (play space), the wrist bent %.0f deg\n",
            hand, rigHands[hand].drawn ? 1 : 0,
            entities.body.visible && entities.body.ent.model ? entities.body.ent.model->name : "-", drawnBare ? " (bare)" : "",
            k.x, k.y, k.z, gap * 1000.f, glm::dot(d, right), d.z, -glm::dot(d, fwd), glm::dot(fa, right), fa.z,
            -glm::dot(fa, fwd), glm::degrees(za::acos(CLAMP(-1.f, glm::dot(fa, handFwd), 1.f))));
    }
    Con_Printf("wrist light: body %.3f %.3f %.3f, off hand %.3f %.3f %.3f, main hand %.3f %.3f %.3f (its own; drawn in the "
               "body's: %s)\n",
        bareLit.color.x, bareLit.color.y, bareLit.color.z, bareLit.hand[0].x, bareLit.hand[0].y,
        bareLit.hand[0].z, bareLit.hand[1].x, bareLit.hand[1].y, bareLit.hand[1].z,
        bareLitHand(&entities.hand[0][FingerBase].ent) >= 0 ? "yes" : "no");
}

// Blood dripping from the wounds (vr_body_blood, vr_bodyblood.cpp): from the hands where they are
// drawn, and the forearms while the body is posed.
void dripBlood(const hands::State& s)
{
    avatar::HandPose poses[2];
    const avatar::HandPose* drawn[2]{nullptr, nullptr};
    for(int hand = 0; hand < 2; hand++)
    {
        const view::ViewEntity& base = entities.hand[hand][FingerBase];
        if(base.visible && base.ent.model)
        {
            poses[hand] = drawnHand(s, hand);
            drawn[hand] = &poses[hand];
        }
    }
    const bool alive = cl.stats[STAT_HEALTH] > 0;
    bodyblood::update(s, drawn, alive ? damageLevel() : 0);
    bloodprints::frame(s, drawn); // bloody footprints and handprints (vr_bloodprints.cpp)
}

// The pauldrons (vr_body_pauldrons; make_pauldron.py), after the Quake ranger's: a cap over each
// shoulder, carried by the clavicle and turning partly with the upper arm, and lames round the top
// of the upper arm, which they follow. Both models are made in the bind pose's body space about the
// left shoulder joint; the right side draws them mirrored.
void setupPauldrons()
{
    for(int side = 0; side < 2; side++)
    {
        entities.pauldron[side].visible = false;
        entities.pauldronArm[side].visible = false;
    }
    const view::ViewEntity& body = entities.body;
    if(!vr_body_pauldrons.value || !body.visible)
    {
        return;
    }

    // Skins: 0 leather, 1-3 the armours' colours, 4 steel.
    const int style = static_cast<int>(vr_body_pauldron_style.value);
    const int skin = style == 1 ? armorWorn() : style >= 2 ? 4 : 0;

    // Made for the athletic build.
    const int build = avatar::bodyBuild();
    const float size = CLAMP(0.25f, vr_body_pauldron_size.value, 4.f) * avatar::buildShape(build).pauldron;
    const float k = avatar::modelScale(&body.ent); // world units per model unit
    const unsigned char scale = static_cast<unsigned char>(CLAMP(1.f, k * size * ENTSCALE_DEFAULT + 0.5f, 255.f));
    const float follow = CLAMP(0.f, vr_body_pauldron_follow.value, 1.f);

    for(int side = 0; side < 2; side++)
    {
        avatar::Shoulder sh;
        if(!avatar::shoulder(side, sh))
        {
            return;
        }

        const bool mirrored = side == 1;
        // Offsets in the body's bind space: forward, out (left for the left side), up.
        const glm::vec3 offset = glm::vec3{vr_body_pauldron_forward.value,
                                     vr_body_pauldron_out.value * (mirrored ? -1.f : 1.f), vr_body_pauldron_up.value} *
                                 sh.m2w;

        const glm::quat capRot = glm::slerp(sh.clavicle, sh.upperArm, follow);
        const auto part = [&](view::ViewEntity& ve, const char* model, const glm::quat& rot) {
            const glm::mat3 m = glm::mat3_cast(rot);
            const glm::vec3 a = hands::anglesFromVectors(m[0], m[2]);
            place(ve, viewModel(model), sh.joint + rot * offset, {-a.x, a.y, a.z}, 0, mirrored);
            ve.ent.skinnum = skin;
            ve.ent.scale = scale;
            ve.ent.alpha = body.ent.alpha;
            ve.lightMultiply = body.lightMultiply;
            ve.lightMod = body.lightMod;
        };
        part(entities.pauldron[side], "progs/vrpauldron.mdl", capRot);
        part(entities.pauldronArm[side], "progs/vrpauldron_arm.mdl", sh.upperArm);
    }
}

// ----------------------------------------------------------------------------
// Weapon buttons

// The fingertip of `hand` that presses the other gun's buttons.
[[nodiscard]] glm::vec3 buttonFingertip(const hands::State& s, int hand)
{
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.rot[hand], fwd, right, up);
    return s.pos[hand] + fwd * 2.f - up * 2.5f;
}

// The way a weapon button's face looks (the model's +z, as drawn).
[[nodiscard]] glm::vec3 buttonFace(const view::ViewEntity& button)
{
    const glm::vec3 d = view::modelPoint(button, {0.f, 0.f, 1.f}) - view::modelPoint(button, {0.f, 0.f, 0.f});
    return glm::length(d) > 1e-4f ? glm::normalize(d) : glm::vec3{0.f, 0.f, 1.f};
}

// The author: the ammo button pressed only from its front, never from behind it or its side. A fingertip coming within
// reach of it is a press only within vr_weapon_button_cone degrees of the button's face direction (180: from anywhere,
// as before), the cone's tip vr_weapon_button_depth units behind the button's middle (a fingertip coming in flat, just
// above the face, still counts). Its angle off the face in `angle`.
[[nodiscard]] bool buttonFromFront(const glm::vec3& fingertip, const glm::vec3& at, const glm::vec3& face, float& angle)
{
    const glm::vec3 d = fingertip - (at - face * za::max(vr_weapon_button_depth.value, 0.f));
    const float len = glm::length(d);
    angle = len > 0.05f ? glm::degrees(std::acos(CLAMP(-1.f, glm::dot(d / len, face), 1.f))) : 0.f;
    return vr_weapon_button_cone.value >= 180.f || angle <= vr_weapon_button_cone.value;
}

void pressWeaponButtons(const hands::State& s)
{
    for(int k = 0; k < 4; k++)
    {
        const int hand = k & 1;
        const bool front = k >= 2;
        ButtonState& st = front ? frontButtonStates[hand] : buttonStates[hand];
        const view::ViewEntity& button = front ? entities.frontButton[hand] : entities.button[hand];
        if(!button.visible)
        {
            st.hover = false;
            continue;
        }
        const int other = 1 - hand;
        const glm::vec3 fingertip = buttonFingertip(s, other);
        const glm::vec3 buttonPos{button.ent.origin[0], button.ent.origin[1], button.ent.origin[2]};
        float angle = 0.f;
        const bool fromFront = buttonFromFront(fingertip, buttonPos, buttonFace(button), angle);

        const float dist = glm::distance(fingertip, buttonPos);
        const bool hover = dist < buttonReach || (st.hover && dist < buttonReach + buttonLeave);
        if(developer.value >= 2 && glm::distance(fingertip, buttonPos) < 8.f) // (placing a button: how near the finger is)
        {
            Con_Printf("weapon button %d%s: the other fingertip %.1f from it (%.1f %.1f %.1f), %.0f deg off its face\n", hand,
                front ? " (front)" : "", static_cast<double>(glm::distance(fingertip, buttonPos)),
                static_cast<double>(fingertip.x), static_cast<double>(fingertip.y), static_cast<double>(fingertip.z),
                static_cast<double>(angle));
        }
        if(hover && !st.hover && !fromFront)
        {
            if(developer.value)
            {
                Con_Printf("weapon button %d%s: not pressed, the fingertip came %.0f deg off its face (cone %g)\n", hand,
                    front ? " (front)" : "", static_cast<double>(angle), static_cast<double>(vr_weapon_button_cone.value));
            }
        }
        else if(hover && !st.hover && cl.time - st.lastPress >= buttonRepeat)
        {
            st.lastPress = cl.time;
            if(developer.value)
            {
                Con_Printf("weapon button %d%s: pressed, the fingertip came %.0f deg off its face\n", hand,
                    front ? " (front)" : "", static_cast<double>(angle));
            }
            // (Inserted: it runs next, before the rest of a mock script; a real game's buffer is empty then.)
            Cbuf_InsertText(front ? (hand == HAND_OFF ? "impulse 44\n" : "impulse 45\n") : (hand == HAND_OFF ? "impulse 42\n" : "impulse 43\n"));
        }
        st.hover = hover;
    }
}

void setupButton(int hand)
{
    view::ViewEntity& ve = entities.button[hand];
    const view::ViewEntity& weapon = entities.weapon[hand];
    const int slot = weapons::slotForModel(weapon.ent.model);

    if(!weapon.ent.model || slot < 0 || weapons::value(slot, Key::WpnButtonMode) == 0.f)
    {
        ve.visible = false;
        return;
    }

    // On the weapon as it is drawn: mirrored as the weapon is, and turned with the pose it is drawn
    // from (a gun carried by its foregrip is drawn from the pose of the hand that let it go, not
    // from the carrying hand's: round 18).
    const twohand::HeldAs& held = drawnAs[hand];
    const bool mirrored = held.mirrored;
    const glm::vec3 pos = view::anchorPosition(weapon,
        weapons::anchor(slot, Key::WpnButtonAnchor),
        weapons::vec(slot, Key::WpnButtonX, Key::WpnButtonY, Key::WpnButtonZ));

    glm::vec3 angles = weapons::vec(slot, Key::WpnButtonPitch, Key::WpnButtonYaw, Key::WpnButtonRoll);
    if(mirrored)
    {
        angles.z = -angles.z;
    }
    angles = composeAngles(held.rot, angles);
    angles.x = -angles.x; // alias models' pitch is the other way

    place(ve, viewModel("progs/wpnbutton.mdl"), pos, angles, 0, mirrored);
}

// The grappling gun's front button (vr_grapple_front_button): the back button's place moved along the gun
// (vr_grapple_front_button_x/y/z, model units), turned as it is.
void setupFrontButton(int hand)
{
    view::ViewEntity& ve = entities.frontButton[hand];
    const view::ViewEntity& weapon = entities.weapon[hand];
    const int slot = weapon.ent.model ? weapons::slotForModel(weapon.ent.model) : -1;
    if(!vr_grapple_front_button.value || slot < 0 || !modelmeta::is(weapon.ent.model, modelmeta::Id::VGrpple) ||
        weapons::value(slot, Key::WpnButtonMode) == 0.f || !entities.button[hand].visible)
    {
        ve.visible = false;
        return;
    }
    const twohand::HeldAs& held = drawnAs[hand];
    const bool mirrored = held.mirrored;
    const glm::vec3 pos = view::anchorPosition(weapon, weapons::anchor(slot, Key::WpnButtonAnchor),
        weapons::vec(slot, Key::WpnButtonX, Key::WpnButtonY, Key::WpnButtonZ) +
            glm::vec3{vr_grapple_front_button_x.value, vr_grapple_front_button_y.value, vr_grapple_front_button_z.value});
    const view::ViewEntity& back = entities.button[hand];
    place(ve, viewModel("progs/wpnbutton.mdl"), pos, geom::vec(back.ent.angles), 0,
        mirrored);
    double& logAt = grappleDebug.frontLogAt;
    if(vr_grapple_debug.value >= 3 && developer.value && vr_gametime >= logAt)
    {
        logAt = vr_gametime + 0.5;
        Con_Printf("grapple: hand %d's gun: the back button at %.1f %.1f %.1f, the front one at %.1f %.1f %.1f\n", hand,
            static_cast<double>(back.ent.origin[0]), static_cast<double>(back.ent.origin[1]), static_cast<double>(back.ent.origin[2]),
            static_cast<double>(pos.x), static_cast<double>(pos.y), static_cast<double>(pos.z));
    }
}

} // namespace view_detail
