// vr_view.cpp -- see vr_view.hpp. Ported from the old engine's view.cpp (V_RenderView_*).

#include "vr_view.hpp"
#include "vr_anchor.hpp"
#include "vr_body.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_protocol.hpp"
#include "vr_render.hpp"
#include "vr_stereo.hpp"
#include "vr_text3d.hpp"
#include "vr_twohand.hpp"
#include "vr_main.hpp"
#include "vr_weapons.hpp"

#include <array>
#include <cmath>
#include <cstring>

using namespace qvr;
using namespace qvr::protocol;
using weapons::Key;

namespace
{

constexpr int OFF = 0;
constexpr int MAIN = 1;

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
            if(!vr_finger_blending.value)
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
    view::ViewEntity torso;
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
    f(entities.torso);
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
void queueWeaponText(const hands::State& s, int hand, const view::ViewEntity& ve, int slot)
{
    // The view is set up for each eye; queue once per frame.
    static int queuedFrame[2]{-1, -1};
    if(!vr_show_weapon_text.value || !ve.visible || weapons::value(slot, Key::WpnTextMode) == 0.f ||
        queuedFrame[hand] == host_framecount)
    {
        return;
    }
    queuedFrame[hand] = host_framecount;

    const bool mirrored = hand == OFF;
    const glm::vec3 pos = view::anchorPosition(ve, static_cast<int>(weapons::value(slot, Key::WpnTextAnchorVertex)),
        weapons::vec(slot, Key::WpnTextX, Key::WpnTextY, Key::WpnTextZ));

    glm::vec3 angles = weapons::vec(slot, Key::WpnTextPitch, Key::WpnTextYaw, Key::WpnTextRoll);
    if(mirrored)
    {
        angles.z = -angles.z;
    }
    // Read from behind the weapon, along its aim (the old engine flipped the pitch for its own
    // text orientation).
    angles += s.visualRot[hand];

    const bool main = hand == MAIN;
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

    text3d::queue(buf, pos, angles, text3d::Align::Centre, 0.1f * weapons::value(slot, Key::WpnTextScale));
}

void setupWeapon(hands::State& s, int hand, qmodel_t* model, int frame)
{
    view::ViewEntity& ve = entities.weapon[hand];
    const bool mirrored = hand == OFF;
    const int slot = weapons::slotForModel(model);

    glm::vec3 gunOffset = weapons::vec(slot, Key::GunOffsetX, Key::GunOffsetY, Key::GunOffsetZ);
    if(mirrored)
    {
        gunOffset.y = -gunOffset.y;
    }

    const glm::vec3 o = weaponAngleOffsets(slot, mirrored);
    const glm::vec3& rot = s.visualRot[hand];

    place(ve, model, s.pos[hand] + gunOffset, {-rot.x + o.x, rot.y + o.y, rot.z + o.z}, frame,
        mirrored);

    ve.zeroBlend = weapons::value(slot, Key::ZeroBlend);

    if(model && slot >= 0)
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

    s.grip2HValid[hand] = model && slot >= 0 && weapons::value(slot, Key::TwoHDisplayMode) == 1.f;
    if(s.grip2HValid[hand])
    {
        s.grip2H[hand] = view::anchorPosition(ve, static_cast<int>(weapons::value(slot, Key::TwoHHandAnchorVertex)),
            weapons::vec(slot, Key::TwoHFixedOffsetX, Key::TwoHFixedOffsetY, Key::TwoHFixedOffsetZ));
    }

    // The empty hand's "weapon" is a hand model; hands are drawn separately.
    if(isHandModel(model))
    {
        ve.visible = false;
    }
    else if(model && slot >= 0)
    {
        queueWeaponText(s, hand, ve, slot);
    }
}

// ----------------------------------------------------------------------------
// Hands

[[nodiscard]] glm::vec3 fingerOffset(int finger, int hand)
{
    glm::vec3 result{vr_fingers_and_base_x.value, vr_fingers_and_base_y.value,
        vr_fingers_and_base_z.value};

    if(hand == OFF)
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

void setupHand(const hands::State& s, int hand)
{
    const view::ViewEntity& weapon = entities.weapon[hand];
    const bool mirrored = hand == OFF;
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
    if(gripBlend > 0.f)
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
            handRot = s.rot[other] + offsets + weaponAngleOffsets(fist, mirrored);
        }
    }

    for(int finger = 0; finger < FingerCount; finger++)
    {
        view::ViewEntity& ve = entities.hand[hand][finger];

        glm::vec3 foff = fingerOffset(finger, hand);
        if(mirrored)
        {
            foff.y = -foff.y;
        }

        place(ve, Mod_ForName(fingerModels[finger], false), pos + hands::redirect(foff, handRot),
            {-handRot.x, handRot.y, handRot.z}, fingerFrame(hand, finger), mirrored);

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

    for(int h = 0; h < HolsterCount; h++)
    {
        const bool mirrored = h == LeftHip || h == LeftUpper;
        const glm::vec3 pos = body::holsterPosition(s, bodyHolster[h]);
        const bool hover = hovered(s, bodyHolster[h]);

        qmodel_t* model = precachedModel(cl.stats[STAT_QVR_HOLSTERWEAPONMODEL0 + 2 + h]);
        if(isHandModel(model))
        {
            model = nullptr;
        }

        place(entities.holster[h], model, pos, angles[h], 0, mirrored);
        highlight(entities.holster[h], hover);

        if(vr_leg_holster_model_enabled.value)
        {
            place(entities.holsterSlot[h], Mod_ForName("progs/legholster.mdl", false), pos,
                slotAngles[h], 0, mirrored);
            highlight(entities.holsterSlot[h], hover);
        }
        else
        {
            entities.holsterSlot[h].visible = false;
        }
    }
}

void setupTorso(const hands::State& s)
{
    if(vr_vrtorso_enabled.value != 1.f)
    {
        entities.torso.visible = false;
        return;
    }

    const float heightRatio = CLAMP(0.f, s.crouchRatio, 0.8f);

    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, s.bodyYaw, 0.f}, fwd, right, up);

    glm::vec3 origin = s.playerOrigin + fwd * vr_vrtorso_x_offset.value -
                       fwd * (heightRatio * 14.f) + right * vr_vrtorso_y_offset.value;
    origin.z += s.headHeight * vr_vrtorso_head_z_mult.value + vr_vrtorso_z_offset.value;

    place(entities.torso, Mod_ForName("progs/vrtorso.mdl", false), origin,
        {vr_vrtorso_pitch.value - heightRatio * 35.f, s.bodyYaw + vr_vrtorso_yaw.value,
            vr_vrtorso_roll.value},
        0, false);
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
            Cbuf_AddText(hand == OFF ? "impulse 42\n" : "impulse 43\n");
        }
        st.hover = hover;
    }
}

void setupButton(const hands::State& s, int hand)
{
    view::ViewEntity& ve = entities.button[hand];
    const view::ViewEntity& weapon = entities.weapon[hand];
    const int slot = weapons::slotForModel(weapon.ent.model);

    if(!weapon.ent.model || slot < 0 || weapons::value(slot, Key::WpnButtonMode) == 0.f)
    {
        ve.visible = false;
        return;
    }

    const bool mirrored = hand == OFF;
    const glm::vec3 pos = view::anchorPosition(weapon,
        static_cast<int>(weapons::value(slot, Key::WpnButtonAnchorVertex)),
        weapons::vec(slot, Key::WpnButtonX, Key::WpnButtonY, Key::WpnButtonZ));

    glm::vec3 angles = weapons::vec(slot, Key::WpnButtonPitch, Key::WpnButtonYaw, Key::WpnButtonRoll);
    if(mirrored)
    {
        angles.z = -angles.z;
    }
    angles += s.visualRot[hand];
    angles.x = -angles.x;

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

extern "C" void VR_SetupViewEntities()
{
    hands::State& s = hands::current();
    if(s.valid && stereo::isRenderingEye())
    {
        applyEyeView(s);
    }
    if(!s.valid || cl.intermission)
    {
        forEachEntity([](view::ViewEntity& ve) { ve.visible = false; });
        s.muzzleValid[OFF] = s.muzzleValid[MAIN] = false;
        return;
    }

    updateFingerFrames();
    setupWeapon(s, MAIN, precachedModel(cl.stats[STAT_WEAPON]), cl.stats[STAT_WEAPONFRAME]);
    setupWeapon(s, OFF, precachedModel(cl.stats[STAT_QVR_WEAPONMODEL2]),
        cl.stats[STAT_QVR_WEAPONFRAME2]);

    setupHand(s, MAIN);
    setupHand(s, OFF);
    setupHolsters(s);
    setupTorso(s);
    setupButton(s, MAIN);
    setupButton(s, OFF);
    if(vrActive())
    {
        pressWeaponButtons(s);
    }

    // Rendering may run several times per frame (one per eye); add the entities only once.
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
            Con_Printf("%s weapon foregrip (%.1f %.1f %.1f)\n", h == MAIN ? "main" : "off", s.grip2H[h].x,
                s.grip2H[h].y, s.grip2H[h].z);
        }
        const HandInput& in = tracking().input.hands[h];
        Con_Printf("%s hand: trigger %.2f grip %.2f thumb %d, curls %.1f %.1f %.1f %.1f %.1f\n",
            h == MAIN ? "main" : "off", in.triggerValue, in.gripValue, in.thumbTouch, fingerFrames[h][FingerThumb],
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
