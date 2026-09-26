// vr_view.cpp -- see vr_view.hpp. Ported from the old engine's view.cpp (V_RenderView_*).

#include "vr_view.hpp"
#include "vr_engine.hpp"
#include "vr_units.hpp"
#include "vr_color.hpp"
#include "vr_anchor.hpp"
#include "vr_avatar.hpp"
#include "vr_gadget.hpp"
#include "vr_flashlight.hpp"
#include "vr_body.hpp"
#include "vr_bodyblood.hpp"
#include "vr_cvars.hpp"
#include "vr_emissive.hpp"
#include "vr_hands.hpp"
#include "vr_lines.hpp"
#include "vr_protocol.hpp"
#include "vr_render.hpp"
#include "vr_shells.hpp"
#include "vr_stereo.hpp"
#include "vr_text3d.hpp"
#include "vr_twohand.hpp"
#include "vr_main.hpp"
#include "vr_profile.hpp"
#include "vr_weapons.hpp"

#include <cmath>
#include <cstring>

using namespace qvr;
using namespace qvr::protocol;
using weapons::Key;

namespace
{

enum Finger : int
{
    FingerBase,
    FingerThumb,
    FingerIndex,
    FingerMiddle,
    FingerRing,
    FingerPinky,
    FingerCount
};

constexpr const char* fingerModels[FingerCount] = {"progs/hand_base.mdl",
    "progs/finger_thumb.mdl", "progs/finger_index.mdl", "progs/finger_middle.mdl",
    "progs/finger_ring.mdl", "progs/finger_pinky.mdl"};

// Finger curls, as finger model frames (0 open .. 5 curled), blended towards the controller's
// (vr_finger_blending_speed frames per second). From the old engine's finger tracking, driven
// by the trigger, the grip and the thumb's touch sensors instead of SteamVR's skeleton.
float fingerFrames[2][FingerCount]{};
double fingerFramesTime = -1.0;

[[nodiscard]] float targetCurl(const HandInput& in, int finger)
{
    const auto curl = [](float v) { return CLAMP(0.f, v + vr_finger_grip_bias.value, 1.f); };

    switch(finger)
    {
        case FingerBase: return 0.f;
        case FingerThumb:
        {
            if(!vrActive())
            {
                return 1.f; // flat screen: a closed thumb, as the old engine
            }
            const bool othersCurled = (curl(in.triggerValue) + 3.f * curl(in.gripValue)) / 4.f > 0.5f;
            return in.thumbTouch || (vr_finger_auto_close_thumb.value && othersCurled) ? 1.f : 0.f;
        }
        case FingerIndex: return curl(in.triggerValue);
        default: return curl(in.gripValue);
    }
}

void updateFingerFrames()
{
    const float dt = fingerFramesTime >= 0.0 ? static_cast<float>(CLAMP(0.0, cl.time - fingerFramesTime, 0.1)) : 0.f;
    fingerFramesTime = cl.time;

    const InputState& input = tracking().input;
    for(int hand = 0; hand < 2; hand++)
    {
        for(int finger = 0; finger < FingerCount; finger++)
        {
            float& frame = fingerFrames[hand][finger];
            const float target = targetCurl(input.hands[hand], finger) * 5.f;
            if(vr_finger_blending_speed.value <= 0.f) // instant
            {
                frame = target;
                continue;
            }

            const float step = dt * vr_finger_blending_speed.value;
            frame = frame < target ? std::fmin(frame + step, target) : std::fmax(frame - step, target);
        }
    }
}

[[nodiscard]] int fingerFrame(int hand, int finger)
{
    return static_cast<int>(fingerFrames[hand][finger] + 0.5f);
}

enum Holster : int
{
    LeftHip,
    RightHip,
    LeftUpper,
    RightUpper,
    HolsterCount
};

struct Entities
{
    view::ViewEntity weapon[2];
    view::ViewEntity hand[2][FingerCount];
    view::ViewEntity holster[HolsterCount];
    view::ViewEntity holsterSlot[HolsterCount];
    view::ViewEntity body;
    view::ViewEntity pauldron[2];    // per side of the body (0 left): the cap,
    view::ViewEntity pauldronArm[2]; // and the lames round the upper arm
    view::ViewEntity gadget;
    view::ViewEntity flashlight; // vr_flashlight.cpp
    view::ViewEntity button[2];
};

Entities entities;
int lastAddedFrame = -1;

template <typename F>
void forEachEntity(F&& f)
{
    for(view::ViewEntity& ve : entities.weapon)
    {
        f(ve);
    }
    for(auto& hand : entities.hand)
    {
        for(view::ViewEntity& ve : hand)
        {
            f(ve);
        }
    }
    for(view::ViewEntity& ve : entities.holster)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.holsterSlot)
    {
        f(ve);
    }
    f(entities.body);
    for(int side = 0; side < 2; side++)
    {
        f(entities.pauldron[side]);
        f(entities.pauldronArm[side]);
    }
    f(entities.gadget);
    f(entities.flashlight);
    for(view::ViewEntity& ve : entities.button)
    {
        f(ve);
    }
}

[[nodiscard]] bool isHandModel(const qmodel_t* model)
{
    if(!model)
    {
        return false;
    }

    const char* n = model->name;
    return !strcmp(n, "progs/hand.mdl") || !strcmp(n, "progs/hand_base.mdl") ||
           !strncmp(n, "progs/finger_", 13);
}

[[nodiscard]] qmodel_t* precachedModel(int index)
{
    return index > 0 && index < MAX_MODELS ? cl.model_precache[index] : nullptr;
}

void place(view::ViewEntity& ve, qmodel_t* model, const glm::vec3& origin, const glm::vec3& angles,
    int frame, bool mirrored)
{
    entity_t& e = ve.ent;

    if(model != ve.lastModel)
    {
        e.lerpflags |= LERP_RESETANIM;
        ve.lastModel = model;
    }

    e.model = model;
    e.frame = frame;
    e.colormap = vid.colormap;
    e.scale = ENTSCALE_DEFAULT;
    e.alpha = ENTALPHA_DEFAULT;
    for(int i = 0; i < 3; i++)
    {
        e.origin[i] = origin[i];
        e.angles[i] = angles[i];
    }

    ve.mirrored = mirrored;
    ve.visible = model != nullptr;
}

// ----------------------------------------------------------------------------
// Weapons

// Per hand: the pose its weapon is drawn from this frame (the hand's own, or, for a gun carried by
// its foregrip, the pose of the hand that let it go: twohand::carriedWeapon). Everything attached
// to the weapon (the ammo screen, the button) is placed from it, never from the hand.
twohand::HeldAs drawnAs[2]{{glm::vec3{0.f}, glm::vec3{0.f}, true}, {glm::vec3{0.f}, glm::vec3{0.f}, false}};

// Angles `offsets` (in the weapon's frame, as the per-weapon attachment angles are tuned) turned by
// `rot` (a hand pose): composed, not added (added Euler angles only agree while the hand is level).
[[nodiscard]] glm::vec3 composeAngles(const glm::vec3& rot, const glm::vec3& offsets)
{
    const auto axes = [](const glm::vec3& a) {
        vec3_t in{a.x, a.y, a.z}, f, r, u;
        AngleVectors(in, f, r, u);
        return glm::mat3{glm::vec3{f[0], f[1], f[2]}, -glm::vec3{r[0], r[1], r[2]}, glm::vec3{u[0], u[1], u[2]}};
    };
    const glm::mat3 m = axes(rot) * axes(offsets);
    return hands::anglesFromVectors(glm::normalize(m[0]), glm::normalize(m[2]));
}

[[nodiscard]] glm::vec3 weaponAngleOffsets(int slot, bool mirrored)
{
    glm::vec3 o = weapons::vec(slot, Key::Pitch, Key::Yaw, Key::Roll);
    o.x += vr_gunmodelpitch.value;
    if(mirrored)
    {
        o.y = -o.y;
        o.z = -o.z;
    }
    return o;
}

// Floating ammo counter on a weapon (old engine's V_SetupWpnTextViewEnt and the weapon text
// in R_DrawViewModels): clip/clip size over the ammo left when reloading is on, else the ammo.
void queueWeaponText(const glm::vec3& handRot, bool mirrored, int hand, const view::ViewEntity& ve, int slot)
{
    // The view may be set up more than once per frame; queue once.
    static int queuedFrame[2]{-1, -1};
    if(!vr_show_weapon_text.value || !ve.visible || weapons::value(slot, Key::WpnTextMode) == 0.f ||
        queuedFrame[hand] == host_framecount)
    {
        return;
    }
    queuedFrame[hand] = host_framecount;

    const glm::vec3 pos = view::anchorPosition(ve, static_cast<int>(weapons::value(slot, Key::WpnTextAnchorVertex)),
        weapons::vec(slot, Key::WpnTextX, Key::WpnTextY, Key::WpnTextZ));

    glm::vec3 angles = weapons::vec(slot, Key::WpnTextPitch, Key::WpnTextYaw, Key::WpnTextRoll);
    if(mirrored)
    {
        angles.z = -angles.z;
    }
    // Read from behind the weapon, along its aim: the weapon's offset turned by the hand (composed,
    // not added: added angles only agree while the hand is level, and a gun pointing up tilted the
    // screen the wrong way).
    angles = composeAngles(handRot, angles);

    const bool main = hand == HAND_MAIN;
    const int clip = cl.stats[main ? STAT_QVR_WEAPONCLIP : STAT_QVR_WEAPONCLIP2];
    const int clipSize = cl.stats[main ? STAT_QVR_WEAPONCLIPSIZE : STAT_QVR_WEAPONCLIPSIZE2];
    const int ammo = cl.stats[main ? STAT_QVR_AMMOCOUNTER : STAT_QVR_AMMOCOUNTER2];
    const bool reloading = vr_reload_mode.value != 0.f && vr_holster_mode.value == 0.f;

    char buf[64];
    if(reloading && clipSize != 0)
    {
        q_snprintf(buf, sizeof(buf), "%d/%d\n%d", clip, clipSize, ammo);
    }
    else
    {
        q_snprintf(buf, sizeof(buf), "%d", ammo);
    }

    text3d::queue(buf, pos, angles, text3d::Align::Centre, 0.1f * weapons::value(slot, Key::WpnTextScale),
        vr_weapon_screen.value != 0.f);
    if(vr_weapon_screen.value != 0.f)
    {
        emissive::weaponScreenLight(hand, pos, angles); // its faint glow (vr_weapon_screen_light)
    }
}

void setupWeapon(hands::State& s, int hand, qmodel_t* model, int frame)
{
    view::ViewEntity& ve = entities.weapon[hand];
    const int slot = weapons::slotForModel(model);

    // A gun hanging from its foregrip (the hand-off, vr_twohand.cpp): drawn as the hand that let it go
    // held it, carried by this hand.
    twohand::HeldAs held{s.pos[hand], s.visualRot[hand], hand == HAND_OFF};
    const bool carried = twohand::carriedWeapon(s, hand, held);
    const bool mirrored = held.mirrored;
    drawnAs[hand] = held; // what the weapon's attachments (its button) follow

    glm::vec3 gunOffset = weapons::vec(slot, Key::GunOffsetX, Key::GunOffsetY, Key::GunOffsetZ) * weapons::offsetScale();
    if(mirrored)
    {
        gunOffset.y = -gunOffset.y;
    }

    const glm::vec3 o = weaponAngleOffsets(slot, mirrored);
    const glm::vec3& rot = held.rot;

    place(ve, model, held.pos + gunOffset, {-rot.x + o.x, rot.y + o.y, rot.z + o.z}, frame,
        mirrored);

    // Steadied in the "fixed" two-handed display mode: the weapon's own blend towards frame 0
    // for that grip (old engine's V_SetupHandViewEnts).
    const bool fixed2H = model && slot >= 0 && weapons::value(slot, Key::TwoHDisplayMode) == 1.f;
    ve.zeroBlend = weapons::value(slot, fixed2H && twohand::helping(1 - hand) ? Key::TwoHZeroBlend : Key::ZeroBlend);

    if(model && slot >= 0 && !carried) // a carried gun has no aim
    {
        s.muzzle[hand] = view::anchorPosition(ve,
            static_cast<int>(weapons::value(slot, Key::MuzzleAnchorVertex)),
            weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
        s.muzzleValid[hand] = true;
    }
    else
    {
        s.muzzleValid[hand] = false;
    }

    s.grip2HValid[hand] = fixed2H && !carried;
    if(carried && model && slot >= 0)
    {
        // Where the hand that takes it back closes: its handle.
        twohand::setCarriedHandle(hand, view::anchorPosition(ve, static_cast<int>(weapons::value(slot, Key::HandAnchorVertex)),
            weapons::vec(slot, Key::HandOffsetX, Key::HandOffsetY, Key::HandOffsetZ)));
    }
    if(s.grip2HValid[hand])
    {
        // As the old engine's VR_GetWpnFixed2HFinalPosition, which the offsets were tuned with:
        // the vertex and the offsets are mirrored as the helping (other) hand is, not as the
        // weapon is drawn.
        view::ViewEntity helped = ve;
        helped.mirrored = !mirrored;
        s.grip2H[hand] = view::anchorPosition(helped, static_cast<int>(weapons::value(slot, Key::TwoHHandAnchorVertex)),
            weapons::vec(slot, Key::TwoHFixedOffsetX, Key::TwoHFixedOffsetY, Key::TwoHFixedOffsetZ));
    }

    // The empty hand's "weapon" is a hand model; hands are drawn separately.
    if(isHandModel(model))
    {
        ve.visible = false;
    }
    else if(model && slot >= 0)
    {
        queueWeaponText(held.rot, mirrored, hand, ve, slot);
    }
}

// ----------------------------------------------------------------------------
// Hands

[[nodiscard]] glm::vec3 fingerOffset(int finger, int hand)
{
    glm::vec3 result{vr_fingers_and_base_x.value, vr_fingers_and_base_y.value,
        vr_fingers_and_base_z.value};

    if(hand == HAND_OFF)
    {
        result += glm::vec3{vr_fingers_and_base_offhand_x.value,
            vr_fingers_and_base_offhand_y.value, vr_fingers_and_base_offhand_z.value};
    }

    if(finger == FingerBase)
    {
        return result + glm::vec3{vr_finger_base_x.value, vr_finger_base_y.value,
                            vr_finger_base_z.value};
    }

    result += glm::vec3{vr_fingers_x.value, vr_fingers_y.value, vr_fingers_z.value};

    switch(finger)
    {
        case FingerThumb:
            return result + glm::vec3{vr_finger_thumb_x.value, vr_finger_thumb_y.value,
                                vr_finger_thumb_z.value};
        case FingerIndex:
            return result + glm::vec3{vr_finger_index_x.value, vr_finger_index_y.value,
                                vr_finger_index_z.value};
        case FingerMiddle:
            return result + glm::vec3{vr_finger_middle_x.value, vr_finger_middle_y.value,
                                vr_finger_middle_z.value};
        case FingerRing:
            return result + glm::vec3{vr_finger_ring_x.value, vr_finger_ring_y.value,
                                vr_finger_ring_z.value};
        default:
            return result + glm::vec3{vr_finger_pinky_x.value, vr_finger_pinky_y.value,
                                vr_finger_pinky_z.value};
    }
}

// How hurt the player is, 0..3 (vr_body_state): the body's and the hands' damage skins
// (make_vrbody.py, make_bloody_hands.py).
[[nodiscard]] int damageLevel()
{
    if(!vr_body_state.value)
    {
        return 0;
    }
    const int health = cl.stats[STAT_HEALTH];
    return health > 75 ? 0 : health > 50 ? 1 : health > 25 ? 2 : 3;
}

void setupHand(const hands::State& s, int hand)
{
    const view::ViewEntity& weapon = entities.weapon[hand];
    const bool mirrored = hand == HAND_OFF;
    const int fist = weapons::fistSlot();
    const int slot = weapons::slotForModel(weapon.ent.model);

    if(!weapon.ent.model)
    {
        for(view::ViewEntity& ve : entities.hand[hand])
        {
            ve.visible = false;
        }
        return;
    }

    glm::vec3 handRot = s.rot[hand] + weaponAngleOffsets(fist, mirrored);

    // Hands hold weapons at an anchor vertex of the weapon model; empty hands follow the
    // tracked hand directly.
    glm::vec3 pos = s.pos[hand];
    bool hide = false;
    if(slot >= 0 && slot != fist)
    {
        pos = view::anchorPosition(weapon, static_cast<int>(weapons::value(slot, Key::HandAnchorVertex)),
            weapons::vec(slot, Key::HandOffsetX, Key::HandOffsetY, Key::HandOffsetZ));
        hide = weapons::value(slot, Key::HideHand) != 0.f;
    }

    // Steadying the other hand's weapon in the "fixed" two-handed display mode: the hand moves
    // onto the weapon's foregrip (blending in and out with the grip), turned like the holding
    // hand plus the weapon's fixed-hand angles (old engine's V_SetupFixedHelpingHandViewEnt).
    const int other = 1 - hand;
    const float gripBlend = s.grip2HValid[other] ? twohand::transition(other) : 0.f;
    const int otherSlot = weapons::slotForModel(entities.weapon[other].ent.model);
    glm::vec3 bladePos = pos, bladeRot = handRot;
    if(gripBlend > 0.f && twohand::bladeGrip(other) && otherSlot >= 0 &&
        twohand::bladeGripHand(s, hand,
            view::anchorPosition(entities.weapon[other], static_cast<int>(weapons::value(otherSlot, Key::HandAnchorVertex)),
                weapons::vec(otherSlot, Key::HandOffsetX, Key::HandOffsetY, Key::HandOffsetZ)),
            s.rot[other] + weaponAngleOffsets(fist, other == HAND_OFF), bladePos, bladeRot))
    {
        // Holding the other hand's sword by its blade (round 18): on the blade where the hand is,
        // turned (the least turn) to close round it.
        pos = glm::mix(pos, bladePos, gripBlend);
        handRot = bladeRot;
        hide = false;
    }
    else if(gripBlend > 0.f)
    {
        pos = glm::mix(pos, s.grip2H[other], gripBlend);
        hide = false;

        if(twohand::helping(hand))
        {
            const int otherSlot = weapons::heldSlot(other);
            glm::vec3 offsets = weapons::vec(otherSlot, Key::TwoHFixedHandPitch, Key::TwoHFixedHandYaw,
                Key::TwoHFixedHandRoll);
            if(!mirrored)
            {
                offsets.y = -offsets.y;
                offsets.z = -offsets.z;
            }
            // The weapon hand's angles and the grip's, without the fist's own angle offsets
            // (old engine's V_SetupFixedHelpingHandViewEnt).
            handRot = s.rot[other] + offsets;
        }
    }

    // The hand-off (vr_twohand.cpp): a helping hand's drawn pose is what it carries a gun by, and a
    // hand carrying one is drawn so.
    if(twohand::helping(hand))
    {
        twohand::recordHelp(s, hand, pos, handRot);
    }
    else if(twohand::carryingHand(s, hand, pos, handRot))
    {
        hide = false;
    }

    const float offsetScale = weapons::offsetScale();
    const int skin = damageLevel();
    for(int finger = 0; finger < FingerCount; finger++)
    {
        view::ViewEntity& ve = entities.hand[hand][finger];

        glm::vec3 foff = fingerOffset(finger, hand) * offsetScale;
        if(mirrored)
        {
            foff.y = -foff.y;
        }

        place(ve, Mod_ForName(fingerModels[finger], false), pos + hands::redirect(foff, handRot),
            {-handRot.x, handRot.y, handRot.z}, fingerFrame(hand, finger), mirrored);
        ve.ent.skinnum = skin;

        if(hide)
        {
            ve.visible = false;
        }
    }
}

// ----------------------------------------------------------------------------
// Body: holsters, holster slots, torso

// The drawn holsters (stat slots 2..5) in vr_body's terms.
constexpr body::Holster bodyHolster[HolsterCount] = {
    body::LeftHip, body::RightHip, body::LeftUpper, body::RightUpper};

[[nodiscard]] bool hovered(const hands::State& s, body::Holster holster)
{
    const int hotspot = body::holsterHotspot(holster);
    return s.hotspot[HAND_OFF] == hotspot || s.hotspot[HAND_MAIN] == hotspot;
}

void highlight(view::ViewEntity& ve, bool on)
{
    ve.lightMultiply = on;
    ve.lightMod = glm::vec3{on ? 6.f : 1.f};
}

void setupHolsters(const hands::State& s)
{
    const float yaw = s.bodyYaw;

    // Holster stat slots 2..5 are the hips and upper holsters (0 and 1 are the shoulders,
    // which are not drawn).
    const glm::vec3 angles[HolsterCount] = {
        {-90.f, 0.f, -yaw + 10.f}, {-90.f, 0.f, -yaw - 10.f}, {-20.f, yaw + 180.f, 0.f},
        {-20.f, yaw + 180.f, 0.f}};

    const glm::vec3 slotAngles[HolsterCount] = {
        {0.f, yaw - 10.f, 0.f}, {0.f, yaw + 10.f, 0.f}, {-30.f, yaw - 10.f, 0.f},
        {-30.f, yaw + 10.f, 0.f}};

    const body::HolsterPositions positions = body::holsterPositions(s); // one body solve for all
    qmodel_t* const slotModel = vr_leg_holster_model_enabled.value ? Mod_ForName("progs/legholster.mdl", false) : nullptr;
    for(int h = 0; h < HolsterCount; h++)
    {
        const bool mirrored = h == LeftHip || h == LeftUpper;
        const glm::vec3 pos = positions[static_cast<std::size_t>(bodyHolster[h])];
        const bool hover = hovered(s, bodyHolster[h]);

        qmodel_t* model = precachedModel(cl.stats[STAT_QVR_HOLSTERWEAPONMODEL0 + 2 + h]);
        if(isHandModel(model))
        {
            model = nullptr;
        }

        place(entities.holster[h], model, pos, angles[h], 0, mirrored);
        highlight(entities.holster[h], hover);

        if(slotModel)
        {
            place(entities.holsterSlot[h], slotModel, pos,
                slotAngles[h], 0, mirrored);
            highlight(entities.holsterSlot[h], hover);
        }
        else
        {
            entities.holsterSlot[h].visible = false;
        }
    }
}

// The drawn hands' wrists and orientations, for the body's arms and the wrist gadget.
[[nodiscard]] avatar::HandPose drawnHand(const hands::State& s, int hand)
{
    // The centre of the wrist in hand_base.mdl (frame 0).
    constexpr glm::vec3 handWrist{-6.86f, -1.08f, 1.42f};

    avatar::HandPose hp;
    const view::ViewEntity& base = entities.hand[hand][FingerBase];
    if(entities.weapon[hand].ent.model && base.ent.model)
    {
        hp.wrist = view::modelPoint(base, handWrist);
        // hand_base.mdl: +x towards the fingers, +z the index finger's side, +y the palm's.
        hp.up = glm::normalize(view::modelPoint(base, handWrist + glm::vec3{0.f, 0.f, 1.f}) - hp.wrist);
        hp.back = glm::normalize(view::modelPoint(base, handWrist + glm::vec3{0.f, -1.f, 0.f}) - hp.wrist);
        hp.forward = glm::normalize(view::modelPoint(base, handWrist + glm::vec3{1.f, 0.f, 0.f}) - hp.wrist);
    }
    else
    {
        glm::vec3 fwd, right, up;
        hands::angleVectors(s.rot[hand], fwd, right, up);
        hp.wrist = s.pos[hand] - fwd * 4.f;
        hp.up = up;
        hp.back = right * (hand == HAND_OFF ? -1.f : 1.f); // palms facing in
        hp.forward = fwd;
    }
    return hp;
}

// The wrist gadget (vr_hud_mode 1): over the back of the off hand's forearm, just behind the
// wrist, its screen facing out of the back of the hand like a watch's. It reads like one: with
// the forearm raised across the chest, its right is towards the fingers (the left arm's; the
// right arm's, towards the elbow) and its up away from the player.
void setupGadget(const hands::State& s)
{
    view::ViewEntity& ve = entities.gadget;
    if(!gadget::active())
    {
        ve.visible = false;
        gadget::setPose({});
        return;
    }

    const int hand = vr_gadget_hand.value != 0.f ? HAND_MAIN : HAND_OFF;
    const avatar::HandPose hp = drawnHand(s, hand);
    glm::vec3 wrist = hp.wrist;
    glm::vec3 dir = hp.forward;
    if(glm::vec3 w, d; avatar::forearm(hand, w, d))
    {
        wrist = w;
        dir = glm::normalize(d);
    }

    const bool leftArm = (hand == HAND_OFF) == (vr_lefthanded.value == 0.f);
    glm::vec3 out = hp.back - dir * glm::dot(hp.back, dir);
    if(glm::length(out) < 1e-4f)
    {
        ve.visible = false;
        gadget::setPose({});
        return;
    }
    out = glm::normalize(out);
    const glm::vec3 right = dir * (leftArm ? 1.f : -1.f);
    const glm::vec3 screenUp = glm::cross(out, right);

    // Sized with the body (make_gadget.py's units are at vr_world_scale 1.25, eyes at 1.646 m).
    const float body = units::bodyScale();
    const float m2w = units::metresToUnits() * body;
    const float scale = vr_world_scale.value / 1.25f * body * CLAMP(0.25f, vr_gadget_scale.value, 3.f);

    // The player's own placement: offsets in cm and turns in degrees, in the device's axes.
    const glm::mat3 axes{right, screenUp, out};
    const glm::vec3 offset{vr_gadget_x.value, vr_gadget_y.value, vr_gadget_z.value};
    const glm::mat3 turn = glm::mat3_cast(glm::quat{glm::radians(
        glm::vec3{vr_gadget_pitch.value, vr_gadget_yaw.value, vr_gadget_roll.value})});

    gadget::Pose pose;
    pose.valid = true;
    pose.origin = wrist - dir * (0.085f * m2w) + out * (0.045f * m2w + 0.35f * scale) + axes * offset * (0.01f * m2w);
    pose.axes = axes * turn;
    pose.scale = scale;
    gadget::setPose(pose);

    if(vr_body_debug.value)
    {
        lines::line(pose.origin, pose.origin + right * 4.f, 0.2f, {1.f, 0.2f, 0.2f, 1.f}, {1.f, 0.2f, 0.2f, 1.f});
        lines::line(pose.origin, pose.origin + screenUp * 4.f, 0.2f, {0.2f, 1.f, 0.2f, 1.f}, {0.2f, 1.f, 0.2f, 1.f});
        lines::line(pose.origin, pose.origin + out * 4.f, 0.2f, {0.2f, 0.4f, 1.f, 1.f}, {0.2f, 0.4f, 1.f, 1.f});
    }

    const glm::vec3 a = hands::anglesFromVectors(pose.axes[0], pose.axes[2]);
    place(ve, Mod_ForName("progs/vrgadget.mdl", false), pose.origin, {-a.x, a.y, a.z}, 0, false);
    ve.ent.scale = static_cast<unsigned char>(CLAMP(1.f, scale * ENTSCALE_DEFAULT, 255.f));

    // The casing's tint: its lighting times a colour.
    const float tint = CLAMP(0.f, vr_gadget_tint.value, 1.f);
    ve.lightMultiply = tint > 0.f;
    ve.lightMod = glm::mix(glm::vec3{1.f}, hsv(vr_gadget_tint_hue.value, 1.f, 1.f) * 1.4f, tint);
}

// Quad damage: electric arcs crawling over the hands and forearms, reshaped every frame (the same
// in both eyes); now and then a longer one jumps between the fingers and the elbow.
void quadArcs(const hands::State& s)
{
    static int lastFrame = -1;
    if(host_framecount == lastFrame)
    {
        return; // once per frame, however often the view is set up
    }
    lastFrame = host_framecount;

    unsigned seed = static_cast<unsigned>(host_framecount) * 2654435761u;
    const auto rnd = [&seed] { // 0..1
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed >> 8) / static_cast<float>(1u << 24);
    };
    const auto rndDir = [&] { return glm::normalize(glm::vec3{rnd() - 0.5f, rnd() - 0.5f, rnd() - 0.5f} + 1e-3f); };

    const float m2w = units::metresToUnits() * units::bodyScale();
    const auto arc = [&](glm::vec3 a, const glm::vec3& target, int segments, float jitter) {
        const float bright = 0.6f + 0.4f * rnd();
        const glm::vec4 core{0.75f, 0.85f, 1.f, 0.95f * bright};
        const glm::vec4 glow{0.3f, 0.45f, 1.f, 0.35f * bright};
        for(int seg = 1; seg <= segments; seg++)
        {
            glm::vec3 b = glm::mix(a, target, static_cast<float>(seg) / static_cast<float>(segments));
            if(seg < segments)
            {
                b += rndDir() * (jitter * m2w);
            }
            lines::line(a, b, 0.6f, glow, glow);
            lines::line(a, b, 0.15f, core, core);
            a = b;
        }
    };

    for(int hand = 0; hand < 2; hand++)
    {
        glm::vec3 wrist = s.pos[hand];
        glm::vec3 dir = hands::forward(s.rot[hand]);
        if(glm::vec3 w, d; avatar::forearm(hand, w, d))
        {
            wrist = w;
            dir = glm::normalize(d);
        }
        const glm::vec3 elbow = wrist - dir * (0.26f * m2w);
        const glm::vec3 fingers = s.pos[hand] + hands::forward(s.rot[hand]) * (0.05f * m2w);

        for(int bolt = 0; bolt < 5; bolt++)
        {
            if(rnd() < 0.3f)
            {
                continue; // flicker
            }
            const float along = rnd();
            glm::vec3 a = along < 0.25f ? fingers : glm::mix(wrist, elbow, (along - 0.25f) / 0.75f);
            a += rndDir() * (0.03f * m2w);
            arc(a, a + rndDir() * ((0.04f + 0.06f * rnd()) * m2w), 5, 0.012f);
        }
        if(rnd() < 0.2f)
        {
            arc(fingers + rndDir() * (0.02f * m2w), glm::mix(wrist, elbow, 0.5f + 0.5f * rnd()) + rndDir() * (0.03f * m2w), 8,
                0.02f);
        }
    }
}

// The player's state on the body (vr_body_state, vr_body_powerups): the armour worn and the
// damage taken are skins (make_vrbody.py: armour * 4 + damage); powerups tint, fade or spark.
// The armour worn: 0 none, 1 green, 2 yellow, 3 red.
[[nodiscard]] int armorWorn()
{
    return cl.stats[STAT_ARMOR] <= 0 ? 0
           : (cl.items & IT_ARMOR3)  ? 3
           : (cl.items & IT_ARMOR2)  ? 2
           : (cl.items & IT_ARMOR1)  ? 1
                                     : 0;
}

void showPlayerState(view::ViewEntity& ve, const hands::State& s)
{
    if(vr_body_state.value)
    {
        ve.ent.skinnum = armorWorn() * 4 + damageLevel();
    }
    else
    {
        ve.ent.skinnum = 0;
    }

    ve.lightMultiply = false;
    ve.lightMod = glm::vec3{1.f};
    if(!vr_body_powerups.value)
    {
        return;
    }

    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(realtime) * 6.f);
    if(cl.items & IT_INVULNERABILITY)
    {
        ve.lightMultiply = true;
        ve.lightMod = glm::vec3{1.6f + 0.6f * pulse, 0.7f, 0.7f};
    }
    else if(cl.items & IT_SUIT)
    {
        ve.lightMultiply = true;
        ve.lightMod = glm::vec3{0.8f, 1.25f, 0.8f};
    }
    if(cl.items & IT_INVISIBILITY)
    {
        ve.ent.alpha = ENTALPHA_ENCODE(0.3f);
    }
    if(cl.items & IT_QUAD)
    {
        quadArcs(s);
    }
}

// The skinned body (vr_body_mode 2 and 3; 1, the old floating torso, is 2), its arms reaching the
// drawn hands' wrists.
void setupBody(const hands::State& s)
{
    const int mode = static_cast<int>(vr_body_mode.value);
    entities.body.visible = false;
    avatar::hide();

    // Not while dead (the view lies on the floor) or at the end of a level.
    const bool alive = cl.stats[STAT_HEALTH] > 0 && !cl.intermission;
    if(mode >= 1 && alive)
    {
        // The build (vr_body_build): progs/vrbody_lean, vrbody (athletic) or vrbody_brawny.
        const int build = static_cast<int>(vr_body_build.value);
        const char* name = build <= 0 ? "progs/vrbody_lean.mdl" : build >= 2 ? "progs/vrbody_brawny.mdl" : "progs/vrbody.mdl";
        qmodel_t* model = Mod_ForName(name, false);
        if(!avatar::usable(model))
        {
            model = Mod_ForName("progs/vrbody.mdl", false);
        }
        if(avatar::usable(model))
        {
            const avatar::HandPose handPoses[2] = {drawnHand(s, 0), drawnHand(s, 1)};

            view::ViewEntity& ve = entities.body;
            const glm::vec3 origin = avatar::pose(s, model, &ve.ent, handPoses, mode >= 3);
            place(ve, model, origin, glm::vec3{0.f}, 0, false);
            showPlayerState(ve, s);
        }
    }
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
    const int build = static_cast<int>(vr_body_build.value);
    const float size = CLAMP(0.25f, vr_body_pauldron_size.value, 4.f) * (build <= 0 ? 0.88f : build >= 2 ? 1.18f : 1.f);
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
            place(ve, Mod_ForName(model, false), sh.joint + rot * offset, {-a.x, a.y, a.z}, 0, mirrored);
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

// Pressing a weapon's button with the other hand's fingertip toggles its secondary ammo (old
// engine's VR_DoWpnButton, which sent keys bound to these impulses).
void pressWeaponButtons(const hands::State& s)
{
    struct ButtonState
    {
        bool hover{false};
        double lastCheck{0.0};
    };
    static ButtonState states[2];

    for(int hand = 0; hand < 2; hand++)
    {
        ButtonState& st = states[hand];
        const view::ViewEntity& button = entities.button[hand];
        if(!button.visible)
        {
            st.hover = false;
            continue;
        }
        if(cl.time - st.lastCheck <= 0.2)
        {
            continue;
        }
        st.lastCheck = cl.time;

        const int other = 1 - hand;
        glm::vec3 fwd, right, up;
        hands::angleVectors(s.rot[other], fwd, right, up);
        const glm::vec3 fingertip = s.pos[other] + fwd * 2.f - up * 2.5f;
        const glm::vec3 buttonPos{button.ent.origin[0], button.ent.origin[1], button.ent.origin[2]};

        const bool hover = glm::distance(fingertip, buttonPos) < 2.7f;
        if(hover && !st.hover)
        {
            Cbuf_AddText(hand == HAND_OFF ? "impulse 42\n" : "impulse 43\n");
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
        static_cast<int>(weapons::value(slot, Key::WpnButtonAnchorVertex)),
        weapons::vec(slot, Key::WpnButtonX, Key::WpnButtonY, Key::WpnButtonZ));

    glm::vec3 angles = weapons::vec(slot, Key::WpnButtonPitch, Key::WpnButtonYaw, Key::WpnButtonRoll);
    if(mirrored)
    {
        angles.z = -angles.z;
    }
    angles = composeAngles(held.rot, angles);
    angles.x = -angles.x; // alias models' pitch is the other way

    place(ve, Mod_ForName("progs/wpnbutton.mdl", false), pos, angles, 0, mirrored);
}

} // namespace

namespace qvr::view
{

const ViewEntity* find(const entity_t* e)
{
    const auto* first = reinterpret_cast<const std::byte*>(&entities);
    const auto* p = reinterpret_cast<const std::byte*>(e);
    if(p < first || p >= first + sizeof(entities))
    {
        return nullptr;
    }

    const ViewEntity* found = nullptr;
    forEachEntity([&](ViewEntity& ve) {
        if(&ve.ent == e)
        {
            found = &ve;
        }
    });
    return found;
}

glm::vec3 modelPoint(const ViewEntity& ve, const glm::vec3& point)
{
    const entity_t& e = ve.ent;
    if(!e.model || e.model->type != mod_alias)
    {
        return {e.origin[0], e.origin[1], e.origin[2]};
    }

    float m[16];
    render::anchorMatrix(ve, glm::vec3{0.f}, m);

    // anchorMatrix ends with the model's own scale and origin: undo them for a point in model space.
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e.model));
    const glm::vec3 v{(point.x - hdr->scale_origin[0]) / hdr->scale[0], (point.y - hdr->scale_origin[1]) / hdr->scale[1],
        (point.z - hdr->scale_origin[2]) / hdr->scale[2]};
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12], m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
        m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
}

glm::vec3 anchorPosition(const ViewEntity& ve, int anchorIndex, const glm::vec3& extra)
{
    const entity_t& e = ve.ent;
    if(!e.model || e.model->type != mod_alias)
    {
        return {e.origin[0], e.origin[1], e.origin[2]};
    }

    float m[16];
    render::anchorMatrix(ve, extra, m);

    const glm::vec3 v = anchor::posedVertex(e, anchorIndex, ve.zeroBlend);
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12],
        m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
        m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
}

} // namespace qvr::view

extern "C" int VR_HideViewModel()
{
    return hands::current().valid;
}

// Moves the view to the eye being rendered (see vr_stereo.cpp).
static void applyEyeView(const hands::State& s)
{
    const int eye = stereo::eye();
    for(int i = 0; i < 3; i++)
    {
        r_refdef.vieworg[i] = s.eyeOrigin[eye][i];
        r_refdef.viewangles[i] = s.eyeAngles[eye][i];
    }

    // Only used for the near plane distance; the projection comes from the eye's FOV.
    const Fov& fov = frameState().eyes[eye].fov;
    r_refdef.fov_x = glm::degrees(fov.right - fov.left);
    r_refdef.fov_y = glm::degrees(fov.up - fov.down);
}

// Quake VR's grenade and proximity bomb models lack the smoke trail flag the old engine gave
// them when they loaded. Some of its heads (the dog's, the fiend's, the shambler's, the zombie's)
// lack the gib flag, so they left no blood trail: a head with no trail bleeds like a gib (the
// zombie's like a zombie's gibs).
static void patchModelFlags()
{
    static const qmodel_t* world = nullptr;
    if(cl.worldmodel == world)
    {
        return;
    }
    world = cl.worldmodel;
    for(int i = 1; i < MAX_MODELS && cl.model_precache[i]; i++)
    {
        qmodel_t* m = cl.model_precache[i];
        if(!strcmp(m->name, "progs/grenade.mdl") || !strcmp(m->name, "progs/proxbomb.mdl"))
        {
            m->flags |= EF_GRENADE;
        }
        constexpr int trails = EF_ROCKET | EF_GRENADE | EF_GIB | EF_TRACER | EF_ZOMGIB | EF_TRACER2 | EF_TRACER3;
        if(m->type == mod_alias && !strncmp(m->name, "progs/h_", 8) && !(m->flags & trails))
        {
            m->flags |= !strcmp(m->name, "progs/h_zombie.mdl") ? EF_ZOMGIB : EF_GIB;
        }
    }
}

namespace
{

// Knocks on the drawn hands (parried blows): the hand, and with it the weapon and the arm, is
// pushed along the blow and springs back, shaking (vr_parry_wobble scales it).
struct HandImpact
{
    double time = -1.0;
    float strength = 0.f;
    glm::vec3 dir{0.f};
};
HandImpact handImpacts[2];

// The knock now: a position offset and an angle offset (degrees).
void impactOffset(int hand, glm::vec3& pos, glm::vec3& angles)
{
    pos = glm::vec3{0.f};
    angles = glm::vec3{0.f};
    const HandImpact& h = handImpacts[hand];
    const float t = static_cast<float>(cl.time - h.time);
    if(h.time < 0.0 || t < 0.f || t > 0.6f)
    {
        return;
    }
    const float k = h.strength * std::max(0.f, vr_parry_wobble.value);
    const float decay = std::exp(-t * 9.f);
    pos = h.dir * (k * decay * std::sin(t * 38.f + 0.9f));
    angles = glm::vec3{std::sin(t * 41.f), std::cos(t * 33.f), std::sin(t * 29.f)} * (k * 2.5f * decay);
}

} // namespace

void view::parseHandImpact()
{
    const int hand = MSG_ReadByte();
    const float strength = MSG_ReadFloat();
    glm::vec3 dir;
    dir.x = MSG_ReadFloat();
    dir.y = MSG_ReadFloat();
    dir.z = MSG_ReadFloat();
    if(hand == HAND_OFF || hand == HAND_MAIN)
    {
        handImpacts[hand] = {cl.time, strength, glm::length(dir) > 0.f ? glm::normalize(dir) : glm::vec3{0.f}};
    }
}

extern "C" void VR_SetupViewEntities()
{
    QVR_PROFILE("view entities");
    hands::State& s = hands::current();
    if(stereo::isRenderingEye())
    {
        if(s.valid)
        {
            applyEyeView(s);
        }
        // Both eyes see the same entities: the second eye keeps what the first one set up.
        if(!stereo::isFirstEye())
        {
            return;
        }
    }
    if(!s.valid || cl.intermission)
    {
        forEachEntity([](view::ViewEntity& ve) { ve.visible = false; });
        s.muzzleValid[HAND_OFF] = s.muzzleValid[HAND_MAIN] = false;
        bodyblood::clear();
        return;
    }

    updateFingerFrames();

    // Parried blows knock the drawn hands (not the tracked ones the game uses): offset for the
    // view's setup, restored after it.
    glm::vec3 knockPos[2], knockAngles[2];
    for(int hand = 0; hand < 2; hand++)
    {
        impactOffset(hand, knockPos[hand], knockAngles[hand]);
        s.pos[hand] += knockPos[hand];
        s.rot[hand] += knockAngles[hand];
        s.visualRot[hand] += knockAngles[hand];
    }

    setupWeapon(s, HAND_MAIN, precachedModel(cl.stats[STAT_WEAPON]), cl.stats[STAT_WEAPONFRAME]);
    setupWeapon(s, HAND_OFF, precachedModel(cl.stats[STAT_QVR_WEAPONMODEL2]),
        cl.stats[STAT_QVR_WEAPONFRAME2]);

    setupHand(s, HAND_MAIN);
    setupHand(s, HAND_OFF);
    setupHolsters(s);
    setupBody(s);
    setupPauldrons();
    setupGadget(s);
    flashlight::setupView(s, entities.flashlight);
    dripBlood(s);
    setupButton(HAND_MAIN);
    setupButton(HAND_OFF);
    for(int hand = 0; hand < 2; hand++)
    {
        s.pos[hand] -= knockPos[hand];
        s.rot[hand] -= knockAngles[hand];
        s.visualRot[hand] -= knockAngles[hand];
    }
    if(vrActive())
    {
        pressWeaponButtons(s);
    }

    // The ring of shadows fades the hands and weapons too (as the old engine did); the gadget
    // stays readable.
    if(vr_body_powerups.value && (cl.items & IT_INVISIBILITY))
    {
        forEachEntity([](view::ViewEntity& ve) {
            if(&ve != &entities.gadget)
            {
                ve.ent.alpha = ENTALPHA_ENCODE(0.3f);
            }
        });
    }

    patchModelFlags();

    // The screen may be redrawn more than once per frame (a modal dialog); add the entities only once.
    if(lastAddedFrame == host_framecount)
    {
        return;
    }
    lastAddedFrame = host_framecount;

    forEachEntity([](view::ViewEntity& ve) {
        if(ve.visible && ve.ent.model && cl_numvisedicts < MAX_VISEDICTS)
        {
            cl_visedicts[cl_numvisedicts++] = &ve.ent;
        }
    });

    // Spent casings thrown out of the weapons (vr_shells.cpp).
    shells::frame(entities.weapon);
}

namespace qvr::view
{

// vr_dumpview: lists the VR view entities.
void dumpView_f()
{
    const hands::State& s = hands::current();
    Con_Printf("hands valid %d  player (%.1f %.1f %.1f)  main (%.1f %.1f %.1f)\n", s.valid,
        s.playerOrigin.x, s.playerOrigin.y, s.playerOrigin.z, s.pos[1].x, s.pos[1].y, s.pos[1].z);

    for(int h = 0; h < 2; h++)
    {
        if(s.grip2HValid[h])
        {
            Con_Printf("%s weapon foregrip (%.1f %.1f %.1f)\n", h == HAND_MAIN ? "main" : "off", s.grip2H[h].x,
                s.grip2H[h].y, s.grip2H[h].z);
        }
        if(s.muzzleValid[h])
        {
            Con_Printf("%s weapon muzzle (%.1f %.1f %.1f), %.1f units from the hand%s\n", h == HAND_MAIN ? "main" : "off",
                s.muzzle[h].x, s.muzzle[h].y, s.muzzle[h].z, glm::distance(s.muzzle[h], s.pos[h]),
                twohand::bladeGrip(h) ? ", held two-handed by its blade" : "");
        }
        const HandInput& in = tracking().input.hands[h];
        Con_Printf("%s hand: trigger %.2f grip %.2f thumb %d, curls %.1f %.1f %.1f %.1f %.1f\n",
            h == HAND_MAIN ? "main" : "off", in.triggerValue, in.gripValue, in.thumbTouch, fingerFrames[h][FingerThumb],
            fingerFrames[h][FingerIndex], fingerFrames[h][FingerMiddle], fingerFrames[h][FingerRing],
            fingerFrames[h][FingerPinky]);
    }

    int i = 0;
    forEachEntity([&](ViewEntity& ve) {
        const entity_t& e = ve.ent;
        Con_Printf("%2d %-24s vis %d mir %d frame %d org (%.1f %.1f %.1f) ang (%.0f %.0f %.0f)\n", i++,
            e.model ? e.model->name : "-", ve.visible, ve.mirrored, e.frame, e.origin[0], e.origin[1],
            e.origin[2], e.angles[0], e.angles[1], e.angles[2]);
    });
}

} // namespace qvr::view
