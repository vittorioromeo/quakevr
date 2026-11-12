// vr_view_body.cpp -- the view's body pieces (see vr_view.cpp): the skinned body, the wrist info, the blood dripping
// from the wounds, the pauldrons and the weapons' buttons.

#include "vr_view_internal.hpp"
#include "vr_agarmor.hpp"
#include "vr_armourstudio.hpp"
#include "vr_flatplay.hpp"
#include "vr_geom.hpp"
#include "vr_testkit.hpp"
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "vr_api.h"
#include "vr_api_render.h"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

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
            // Flatscreen play: the body rides the drawn view's own move off the pose's eye (the walk's bob, a step's easing) as the
            // drawn hands do (flatplay::drawnOffset), so the arms move with the hands. The whole body: its head height stays (taking
            // the bob in the legs changed the crouch's depth, and crouched the shoulders moved 2.5 on the screen against 0.1), so the
            // feet ride the bob too, a few units off the floor at most.
            hands::State moved = s;
            if(const glm::vec3 ride = flatplay::bodyRide(s); ride != glm::vec3{0.f})
            {
                moved.head += ride;
            }
            const glm::vec3 origin = avatar::pose(moved, model, &ve.ent, handPoses, mode >= 3);
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
        const view::ViewEntity& base = entities.hand[hand];
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
        bareLitHand(&entities.hand[0].ent) >= 0 ? "yes" : "no");
}

// Blood dripping from the wounds (vr_body_blood, vr_bodyblood.cpp): from the hands where they are
// drawn, and the forearms while the body is posed.
void dripBlood(const hands::State& s)
{
    avatar::HandPose poses[2];
    const avatar::HandPose* drawn[2]{nullptr, nullptr};
    for(int hand = 0; hand < 2; hand++)
    {
        const view::ViewEntity& base = entities.hand[hand];
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

// ----------------------------------------------------------------------------
// The armour's plates on your body (vr_player_plates; ROUND24.md, "The player's armour plates"). The server's word on them
// (QC vr_playerplates.qc, by QVR_SVC_ARMOR) says which pieces and what colour; here is how they come and go: a piece lost is
// gone at once (its debris flies from QC), a piece gained comes in over vr_player_plates_morph_time as a gun's other ammo's
// model does (the alias shader's dissolve, each entity its own morph: plate[0] the settled pieces, the others one change
// each), another colour dissolves out as the new one appears. The bones are the body's, a hidden piece's collapsed;
// the helmet is only ever drawn in a view that draws the head too (never from the eyes: plateBonePoses).
namespace
{

struct PlateMorph
{
    int pieces{0}, type{0}; // (0 pieces: a free one)
    double start{0.0};
    bool in{true};
};

struct PlayerPlates
{
    int type{0}, pieces{0}; // the server's word as of the last frame
    PlateMorph morphs[plateEntities - 1];
    int entityPieces[plateEntities]{}; // each of entities.plate's pieces this frame
    bool resync{true};                 // a new level or a load: the next word taken as it comes
    double settleUntil{0.0};           // (and its first words, a frame or two apart)
    int begun{0};                      // morphs begun so far (the tests)
    int drawnFrame{-1};
    int fromEyes{0}, withHead{0}, fromEyesLast{0}, withHeadLast{0}; // the pieces the views drew this frame, the last (the tests)
    int clone{0}, cloneLast{0}; // the pieces the body's mirror image drew (PlatePass::Clone) this frame, the last (the tests)
    int bridgePieces{0};        // the pieces the joints' leather is over this frame (entities.plate[plateBridge])
};
PlayerPlates playerPlates;
float platesBones[plateEntities][128 * 12]{}; // (agarmor's maxSlots)

void beginPlateMorph(int pieces, int type, bool in, double now)
{
    if(!pieces)
    {
        return;
    }
    PlateMorph* slot = &playerPlates.morphs[0];
    for(PlateMorph& m : playerPlates.morphs)
    {
        slot = !slot->pieces ? slot : !m.pieces || m.start < slot->start ? &m : slot; // (a free one, else the oldest ends now)
    }
    *slot = PlateMorph{pieces, type, now, in};
    playerPlates.begun++;
}

} // namespace

void setupPlates()
{
    PlayerPlates& p = playerPlates;
    for(view::ViewEntity& ve : entities.plate)
    {
        ve.visible = false;
        ve.morph = 0.f;
    }
    for(int& x : p.entityPieces)
    {
        x = 0;
    }
    const bool on = vr_player_plates.value != 0.f;
    int type = on ? za::max(agarmor::clientState(cl.viewentity, 0), 0) : 0;
    int pieces = on && type > 0 ? za::max(agarmor::clientState(cl.viewentity, 1), 0) : 0;
    // (the Armour Studio on the player's kind: the armour it edits, whatever you wear; ARMOUR_STUDIO.md, "Your own body")
    int studioType = 0, studioPieces = 0;
    if(on && armourstudio::playerArmour(studioType, studioPieces))
    {
        type = studioType;
        pieces = studioPieces;
    }
    const double now = vr_gametime;
    const float time = vr_player_plates_morph_time.value;

    if(p.resync)
    {
        p.resync = false;
        p.settleUntil = now + 1.0;
    }
    if(now < p.settleUntil || time <= 0.f || !on)
    {
        for(PlateMorph& m : p.morphs)
        {
            m.pieces = 0;
        }
    }
    else if(type != p.type && p.type > 0 && type > 0)
    {
        // Another armour: the old colour's pieces out where the new one's come in (their noise the same: nothing shows twice).
        for(PlateMorph& m : p.morphs)
        {
            m.pieces = 0;
        }
        beginPlateMorph(p.pieces, p.type, false, now);
        beginPlateMorph(pieces, type, true, now);
    }
    else
    {
        for(PlateMorph& m : p.morphs)
        {
            m.pieces &= m.in ? pieces : m.pieces; // (one lost while it came in: gone)
        }
        beginPlateMorph(pieces & ~p.pieces, type, true, now);
    }
    p.type = type;
    p.pieces = pieces;

    int coming = 0;
    for(PlateMorph& m : p.morphs)
    {
        const double t = time > 0.f ? (now - m.start) / static_cast<double>(time) : 1.0;
        m.pieces = t >= 1.0 || t < 0.0 ? 0 : m.pieces;
        coming |= m.in ? m.pieces : 0;
    }

    // The shell made as the body first shows (a level's load, not a first pickup's hitch), armour or not.
    const view::ViewEntity& body = entities.body;
    qmodel_t* armor = on && body.visible ? agarmor::playerArmorModel(body.ent.model) : nullptr;
    if(!armor)
    {
        return;
    }
    const auto show = [&](int i, int shown, int kind, float morph) {
        view::ViewEntity& ve = entities.plate[i];
        if(shown)
        {
            place(ve, armor, {body.ent.origin[0], body.ent.origin[1], body.ent.origin[2]}, glm::vec3{0.f}, 0, false);
            ve.ent.skinnum = kind - 1;
            ve.ent.alpha = body.ent.alpha;
            ve.lightMultiply = body.lightMultiply;
            ve.lightMod = body.lightMod;
            ve.morph = morph;
            p.entityPieces[i] = shown;
        }
    };
    show(0, pieces & ~coming, type, 0.f);
    // (the joints' leather over the pieces worn, settled or coming: an entity of its own, its bones made once a frame)
    if(qmodel_t* bridge = pieces ? agarmor::playerBridgeModel(body.ent.model) : nullptr)
    {
        view::ViewEntity& ve = entities.plate[plateBridge];
        place(ve, bridge, {body.ent.origin[0], body.ent.origin[1], body.ent.origin[2]}, glm::vec3{0.f}, 0, false);
        ve.ent.skinnum = type - 1;
        ve.ent.alpha = body.ent.alpha;
        ve.lightMultiply = body.lightMultiply;
        ve.lightMod = body.lightMod;
        p.bridgePieces = pieces;
    }
    for(int i = 0; i + 1 < plateEntities; i++)
    {
        const PlateMorph& m = p.morphs[i];
        const float t = time > 0.f ? za::clamp(static_cast<float>((now - m.start) / static_cast<double>(time)), 1e-3f, 1.f) : 1.f;
        show(i + 1, m.pieces, m.type, (m.in ? 1.f : -1.f) * (2.f + t)); // (the shader's kind 1, the seam amber: morphValue)
    }
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
    // (the armour's plates on: its colour is theirs, the caps the ranger's own leather, and none under a plate: below)
    const bool plates = vr_player_plates.value != 0.f;
    const int skin = style == 1 ? (plates ? 0 : armorWorn()) : style >= 2 ? 4 : 0;

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

        // A plate on this shoulder takes the cap's and the lames' place (they'd show through it).
        if(plates && (playerPlates.pieces >> (side == 0 ? agarmor::PauldronL : agarmor::PauldronR)) & 1)
        {
            continue;
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

namespace qvr::view
{

int plateBonePoses(const entity_t* e, const float** matrices, PlatePass pass)
{
    if(e == &entities.plate[plateBridge].ent)
    {
        // The joints' leather: from the body's bones as the views draw it (the limbs are the same in every pass)
        const entity_t& body = entities.body.ent;
        if(!matrices)
        {
            return agarmor::playerBridgePoses(body.model, playerPlates.bridgePieces, nullptr, 0, nullptr);
        }
        const float* src = nullptr;
        const int have = VR_AliasBonePoses(&body, &src);
        return agarmor::playerBridgePoses(body.model, playerPlates.bridgePieces, src, have, matrices);
    }
    int index = -1;
    for(int i = 0; i < plateEntities; i++)
    {
        index = e == &entities.plate[i].ent ? i : index;
    }
    if(index < 0)
    {
        return 0;
    }
    const entity_t& body = entities.body.ent;
    int numSlots = 0;
    const int* slotBone = nullptr;
    const int* slotPiece = nullptr;
    if(!agarmor::playerSlots(body.model, numSlots, slotBone, slotPiece))
    {
        return 0;
    }
    if(!matrices)
    {
        return numSlots;
    }
    const float* withHead = avatar::skinWithHead();
    const float* src = nullptr;
    const int have = VR_AliasBonePoses(&body, nullptr);
    if(pass == PlatePass::Clone)
    {
        src = withHead;
    }
    else if(pass == PlatePass::Shadow)
    {
        (void)VR_AliasShadowBonePoses(&body, &src);
    }
    else
    {
        (void)VR_AliasBonePoses(&body, &src);
    }
    // The helmet where the head is drawn: never from the eyes, VR or flat-screen (the body's own rule: VR_AliasBonePoses).
    const bool head = src && src == withHead;
    PlayerPlates& p = playerPlates;
    const int pieces = p.entityPieces[index] & (head ? agarmor::allPieces : ~(1 << agarmor::Helmet));
    if(pass == PlatePass::View || pass == PlatePass::Clone)
    {
        // (rolled over by whichever pass draws first in a frame: the renderer may draw the image before its source)
        if(p.drawnFrame != host_framecount)
        {
            p.drawnFrame = host_framecount;
            p.fromEyesLast = p.fromEyes;
            p.withHeadLast = p.withHead;
            p.cloneLast = p.clone;
            p.fromEyes = p.withHead = p.clone = 0;
        }
        (pass == PlatePass::Clone ? p.clone : head ? p.withHead : p.fromEyes) |= pieces;
    }
    float* out = platesBones[index];
    for(int slot = 0; slot < numSlots; slot++)
    {
        const int b = slotBone[slot];
        const bool shown = src && b < have && ((pieces >> slotPiece[slot]) & 1);
        for(int i = 0; i < 12; i++)
        {
            out[slot * 12 + i] = shown ? src[b * 12 + i] : 0.f; // (a hidden piece collapsed: its triangles gone)
        }
    }
    *matrices = out;
    return numSlots;
}

void platesResync()
{
    playerPlates.resync = true;
}

bool playerArmourShown()
{
    return vr_player_plates.value != 0.f && playerPlates.type > 0 && playerPlates.pieces != 0 && entities.body.visible;
}

void platesInfo_f()
{
    const PlayerPlates& p = playerPlates;
    const entity_t& body = entities.body.ent;
    Con_Printf("plates: on %d, colour %d, pieces %d, morphs begun %d, body %s (visible %d, scale %.3f)\n", vr_player_plates.value != 0.f ? 1 : 0,
        p.type, p.pieces, p.begun, body.model ? body.model->name : "-", entities.body.visible ? 1 : 0, static_cast<double>(avatar::modelScale(&body)));
    for(int i = 0; i < plateEntities; i++)
    {
        const view::ViewEntity& ve = entities.plate[i];
        Con_Printf("plates: entity %d: visible %d, pieces %d, skin %d, morph %.3f, model %s, origin %.1f %.1f %.1f, alpha %d, scale %d\n", i, ve.visible ? 1 : 0,
            p.entityPieces[i], ve.ent.skinnum, static_cast<double>(ve.morph), ve.ent.model ? ve.ent.model->name : "-", static_cast<double>(ve.ent.origin[0]),
            static_cast<double>(ve.ent.origin[1]), static_cast<double>(ve.ent.origin[2]), static_cast<int>(ve.ent.alpha), static_cast<int>(ve.ent.scale));
    }
    if(qmodel_t* mod = entities.plate[0].ent.model; mod && mod->type == mod_alias)
    {
        const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(mod));
        Con_Printf("plates: %s: %d verts, %d indexes, %d bones, vbo %u/%u, skins %d (%d x %d), texture %p, flags %d, mins %.0f\n", mod->name, hdr->numverts_vbo, hdr->numindexes,
            hdr->numbones, mod->meshvbo, mod->meshindexesvbo, hdr->numskins, hdr->skinwidth, hdr->skinheight,
            static_cast<const void*>(hdr->gltextures[za::clamp(entities.plate[0].ent.skinnum, 0, 3)][0]), mod->flags, static_cast<double>(mod->mins[0]));
    }
    const float* m = nullptr;
    const int n = plateBonePoses(&entities.plate[0].ent, &m, PlatePass::View);
    const float* b = nullptr;
    const int nb = VR_AliasBonePoses(&body, &b);
    Con_Printf("plates: drawn from the eyes %d, with the head %d, in the mirror image %d; %d slots, slot 0 row 0 %.3f %.3f %.3f %.3f; %d body bones, bone 1 row 0 %.3f %.3f %.3f %.3f\n",
        p.fromEyesLast, p.withHeadLast, p.cloneLast, n, n ? static_cast<double>(m[0]) : 0.0, n ? static_cast<double>(m[1]) : 0.0, n ? static_cast<double>(m[2]) : 0.0,
        n ? static_cast<double>(m[3]) : 0.0, nb, nb ? static_cast<double>(b[12]) : 0.0, nb ? static_cast<double>(b[13]) : 0.0, nb ? static_cast<double>(b[14]) : 0.0,
        nb ? static_cast<double>(b[15]) : 0.0);
}

// vr_player_plates_model_check's tally (its `end` prints and clears it).
testkit::Tally platesModelTally;

void platesModelCheck_f()
{
    testkit::Tally& c = platesModelTally;
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "end"))
    {
        c.verdict("ppmodel");
        c.reset();
        return;
    }
    // The settled plates drawn, as the armour made for the body drawn ("<body>#armor"), the body the build's own.
    const view::ViewEntity& body = entities.body;
    const view::ViewEntity& plate = entities.plate[0];
    const char* bodyName = body.visible && body.ent.model ? body.ent.model->name : "-";
    const char* plateName = plate.visible && plate.ent.model ? plate.ent.model->name : "-";
    const int build = avatar::bodyBuild();
    char want[MAX_QPATH];
    q_snprintf(want, sizeof(want), "%s#armor", bodyName);
    const bool bodyOk = !strcmp(bodyName, bodyModelName(build, drawnBare));
    const bool plateOk = plate.ent.model && plate.ent.model->type == mod_alias && !strcmp(plateName, want);
    const bool ok = c.check(bodyOk && plateOk && playerPlates.entityPieces[0] != 0);
    Con_Printf("ppmodel: %s build %d, body %s, plates %s (pieces %d)\n", ok ? "ok" : "FAIL", build, bodyName, plateName, playerPlates.entityPieces[0]);
}

int platesState(int what)
{
    const PlayerPlates& p = playerPlates;
    int coming = 0, going = 0;
    const PlateMorph* newest = nullptr;
    for(const PlateMorph& m : p.morphs)
    {
        coming |= m.in ? m.pieces : 0;
        going |= m.in ? 0 : m.pieces;
        newest = m.in && m.pieces && (!newest || m.start > newest->start) ? &m : newest;
    }
    int slots = 0;
    const int* bone = nullptr;
    const int* piece = nullptr;
    switch(what)
    {
        case 10: return p.entityPieces[0];
        case 11: return coming;
        case 12: return going;
        case 13:
        {
            const float time = vr_player_plates_morph_time.value;
            return newest && time > 0.f ? static_cast<int>((vr_gametime - newest->start) / static_cast<double>(time) * 1000.0) : 0;
        }
        case 14: return p.begun;
        case 15: return p.fromEyesLast;
        case 16: return p.withHeadLast;
        case 17: return agarmor::playerSlots(entities.body.ent.model, slots, bone, piece) ? slots : 0;
        case 18: return p.type;
        case 19: return p.cloneLast;
        default: return -1;
    }
}

} // namespace qvr::view
