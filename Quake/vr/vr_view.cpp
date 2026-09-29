// vr_view.cpp -- see vr_view.hpp. Ported from the old engine's view.cpp (V_RenderView_*).

#include "vr_view.hpp"
#include "vr_engine.hpp"
#include "vr_units.hpp"
#include "vr_color.hpp"
#include "vr_anchor.hpp"
#include "vr_climb.hpp"
#include "vr_avatar.hpp"
#include "vr_gadget.hpp"
#include "vr_flashlight.hpp"
#include "vr_body.hpp"
#include "vr_bodyblood.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_emissive.hpp"
#include "vr_hands.hpp"
#include "vr_handrig.hpp"
#include "vr_grasp.hpp"
#include "vr_grip.hpp"
#include "vr_held.hpp"
#include "vr_ledges.hpp"
#include "vr_lines.hpp"
#include "vr_protocol.hpp"
#include "vr_render.hpp"
#include "vr_shells.hpp"
#include "vr_stereo.hpp"
#include "vr_window.hpp"
#include "vr_text3d.hpp"
#include "vr_trace.hpp"
#include "vr_twohand.hpp"
#include "vr_main.hpp"
#include "vr_modelcollide.hpp"
#include "vr_selfcollide.hpp"
#include "vr_posing.hpp"
#include "vr_sightalign.hpp"
#include "vr_bodycal.hpp"
#include "vr_drawblend.hpp"
#include "vr_profile.hpp"
#include "vr_props.hpp"
#include "vr_menu.hpp"
#include "vr_weapons.hpp"
#include "vr_wounds.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>

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

// `indexWithGrip`: the index finger closes with the grip too (a hand holding something that is not its own gun: a
// foregrip, a blade, a box: its trigger does not fire, and the controller's has no sensor for a finger resting on it).
[[nodiscard]] float targetCurl(const HandInput& in, int finger, bool indexWithGrip)
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
        case FingerIndex:
        {
            // Resting on the trigger (its touch sensor): curled half way, onto the trigger (its grasp's stop).
            const float trigger = in.triggerTouch ? std::fmax(in.triggerValue, 0.5f) : in.triggerValue;
            return curl(indexWithGrip && vr_hand_fit.value ? std::fmax(trigger, in.gripValue) : trigger);
        }
        default: return curl(in.gripValue);
    }
}

// Round 21: the finger tweaks on a held weapon (its fgr_bias_* keys: a share of a full curl, negative more open), on
// top of what the fingers do (the controller's curl, the grasp's wrap). Blended like the curls, so a weapon's tweaks
// ease in as it is taken. 0 for an empty hand and a hand helping hold the other's weapon.
float fingerBias[2][FingerCount]{};

[[nodiscard]] float biasFor(int hand, int finger)
{
    // Posing (vr_posing.cpp): the weapon hand holds the posed weapon; the other hand, posing a hotspot, helps.
    const bool posed = posing::active() && hand == posing::session().weaponHand;
    const bool posingHelper = posing::active() && !posed && posing::session().target == posing::Target::Hotspot;
    // Posing the weapon unsolved (posing::showSolved): the controller's plain curls, no tweaks.
    const bool posingPlain = posed && posing::session().target == posing::Target::Weapon && !posing::showSolved();
    if(finger == FingerBase || posingHelper || posingPlain || (!posed && twohand::helping(hand)))
    {
        return 0.f;
    }
    // The held torch's (its grip's finger tweaks: vr_flashlight_low_bias_*, _high_bias_*).
    if(flashlight::Fingers torch; !posed && flashlight::fingers(hand, torch))
    {
        return CLAMP(-1.f, torch.bias[finger - FingerThumb], 1.f);
    }
    const int slot = posed ? posing::session().slot : weapons::heldSlot(hand);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return 0.f;
    }
    constexpr Key keys[FingerCount] = {Key::FingerThumbBias, Key::FingerThumbBias, Key::FingerIndexBias, Key::FingerMiddleBias,
        Key::FingerRingBias, Key::FingerPinkyBias};
    return CLAMP(-1.f, weapons::value(slot, keys[finger]), 1.f);
}

void updateFingerFrames()
{
    const float dt = fingerFramesTime >= 0.0 ? static_cast<float>(CLAMP(0.0, cl.time - fingerFramesTime, 0.1)) : 0.f;
    fingerFramesTime = cl.time;

    const InputState& input = tracking().input;
    const float step = dt * vr_finger_blending_speed.value;
    const auto approach = [&](float& value, float target) {
        if(vr_finger_blending_speed.value <= 0.f) // instant
        {
            value = target;
            return;
        }
        value = value < target ? std::fmin(value + step, target) : std::fmax(value - step, target);
    };
    // Posing a hotspot (vr_posing.cpp): the weapon hand, drawn holding the floating weapon, holds it closed whatever its
    // controller does (it confirms); the posing hand closes its index finger with its grip, as a helping hand does.
    const bool posingHotspot = posing::active() && posing::session().target == posing::Target::Hotspot;
    HandInput holding;
    holding.gripValue = 1.f;
    holding.thumbTouch = holding.triggerTouch = true;
    for(int hand = 0; hand < 2; hand++)
    {
        const bool holder = posingHotspot && hand == posing::session().weaponHand;
        const HandInput& in = holder ? holding : input.hands[hand];
        for(int finger = 0; finger < FingerCount; finger++)
        {
            const bool holdsOther = posingHotspot ? !holder
                                                  : twohand::helping(hand) || held::heldEntity(hand) != 0 || flashlight::holds(hand);
            approach(fingerFrames[hand][finger], targetCurl(in, finger, holdsOther) * 5.f);
            approach(fingerBias[hand][finger], biasFor(hand, finger));
        }
    }
}

// A finger's drawn curl (the six models): the frame, and the blend of it towards frame 0 (open: ViewEntity::zeroBlend)
// for a curl between frames that a weapon's finger tweak leaves (the curls alone are whole frames, as they were).
struct FingerPose
{
    int frame;
    float open;
};

[[nodiscard]] FingerPose fingerPose(int hand, int finger)
{
    const float curl = CLAMP(0.f, std::floor(fingerFrames[hand][finger] + 0.5f) + 5.f * fingerBias[hand][finger], 5.f);
    const int frame = static_cast<int>(std::ceil(curl - 0.01f));
    if(frame <= 0)
    {
        return {0, 0.f};
    }
    const float open = 1.f - curl / static_cast<float>(frame);
    return {frame, open < 0.5f / 255.f ? 0.f : open};
}

enum Holster : int
{
    LeftHip,
    RightHip,
    LeftUpper,
    RightUpper,
    LeftShoulder, // on the back, reached over the shoulder (no holster model: holsterSlot not drawn)
    RightShoulder,
    HolsterCount
};

// Weapons lying in the world near the player that show their ammo screen and button (the nearest).
constexpr int maxWorldWeapons = 6;

struct Entities
{
    view::ViewEntity weapon[2];
    view::ViewEntity weaponMorph[2]; // the model a gun is morphing out of (its other ammo's: vr_weapon_morph_time)
    view::ViewEntity hand[2][FingerCount];
    view::ViewEntity holster[HolsterCount];
    view::ViewEntity holsterSlot[HolsterCount];
    view::ViewEntity holsterButton[HolsterCount]; // the buttons of the holstered weapons,
    view::ViewEntity worldButton[maxWorldWeapons]; // and of the weapons lying round (vr_weapon_screen_idle)
    view::ViewEntity body;
    view::ViewEntity pauldron[2];    // per side of the body (0 left): the cap,
    view::ViewEntity pauldronArm[2]; // and the lames round the upper arm
    view::ViewEntity gadget;
    view::ViewEntity gadgetStrap[2]; // round the forearm under its lugs (the elbow's side, the wrist's)
    view::ViewEntity flashlight; // vr_flashlight.cpp
    view::ViewEntity pouch;      // the grenade pouch at the small of the back (vr_handgrenade)
    view::ViewEntity button[2];
    view::ViewEntity ghost[2]; // the motion review's ghost of a take's weapons (view::setGhost)
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
    for(view::ViewEntity& ve : entities.weaponMorph)
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
    f(entities.gadgetStrap[0]);
    f(entities.gadgetStrap[1]);
    f(entities.flashlight);
    f(entities.pouch);
    for(view::ViewEntity& ve : entities.button)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.holsterButton)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.worldButton)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.ghost)
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
           !strncmp(n, "progs/finger_", 13) || !strcmp(n, handrig::modelName);
}

[[nodiscard]] qmodel_t* precachedModel(int index)
{
    return index > 0 && index < MAX_MODELS ? cl.model_precache[index] : nullptr;
}

} // namespace

// Mod_ForName(name, false) for the view entities' own models, asked for every frame: the model found for the
// same name (string) last time, while it is still that one and loaded (Mod_LoadModel's own check), without
// Mod_FindName's walk through every model known. A model that isn't there is remembered too, until the next map (or
// game) is loaded: not looked for on disk every frame.
namespace
{
struct ViewModelEntry
{
    const char* name;
    qmodel_t* model;
    int generation;
};
ViewModelEntry viewModels[64]{};
} // namespace

qmodel_t* view::viewModel(const char* name)
{
    using Entry = ViewModelEntry;
    Entry* const entries = viewModels;
    Entry& e = entries[(reinterpret_cast<std::uintptr_t>(name) * 0x9E3779B97F4A7C15ull) >> 58];
    if(e.name == name)
    {
        qmodel_t* m = e.model;
        if(!m && e.generation == worldGeneration())
        {
            return nullptr;
        }
        if(m && !m->needload && (m->type != mod_alias || Cache_Check(&m->cache)) && !strcmp(m->name, name))
        {
            return m;
        }
    }
    qmodel_t* m = Mod_ForName(name, false);
    e = {name, m, worldGeneration()};
    return m;
}

void view::prepareModels()
{
    if(!vr_enabled.value) // (in VR, even before its session runs: the first map loads as it starts)
    {
        return;
    }
    // As the view setup asks for them (setupBody, setupPauldrons, setupHolsters, setupGadget, the hands, the buttons).
    std::vector<const char*> names{handrig::modelName, "progs/vrgadget.mdl", "progs/vrgadget_strap.mdl", "progs/wpnbutton.mdl"};
    if(vr_body_mode.value >= 1.f)
    {
        const int build = static_cast<int>(vr_body_build.value);
        names.push_back(build <= 0 ? "progs/vrbody_lean.mdl" : build >= 2 ? "progs/vrbody_brawny.mdl" : "progs/vrbody.mdl");
        names.push_back("progs/vrbody.mdl");
        if(vr_body_pauldrons.value)
        {
            names.push_back("progs/vrpauldron.mdl");
            names.push_back("progs/vrpauldron_arm.mdl");
        }
    }
    if(vr_leg_holster_model_enabled.value)
    {
        names.push_back("progs/legholster.mdl");
    }
    for(const char* name : names)
    {
        (void)Mod_ForName(name, false);
    }
}

namespace
{

using view::viewModel;

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
    ve.scale = glm::vec3{1.f};
    ve.visible = model != nullptr;
}

// ----------------------------------------------------------------------------
// Weapons

// Per hand: the pose its weapon is drawn from this frame (the hand's own, or, for a gun carried by
// its foregrip, the pose of the hand that let it go: twohand::carriedWeapon). Everything attached
// to the weapon (the ammo screen, the button) is placed from it, never from the hand.
bool weaponCarried[2]{}; // the weapon in the hand is a gun carried by its foregrip (twohand::carriedWeapon)
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

// Quake angles (not an alias model's) to axes: forward, left, up.
[[nodiscard]] glm::mat3 angleAxes(const glm::vec3& a)
{
    vec3_t in{a.x, a.y, a.z}, f, r, u;
    AngleVectors(in, f, r, u);
    return glm::mat3{glm::vec3{f[0], f[1], f[2]}, -glm::vec3{r[0], r[1], r[2]}, glm::vec3{u[0], u[1], u[2]}};
}

// The angles of an attachment (ammo screen, button: `offsets` as tuned, composed with the hand holding the gun) on a
// weapon drawn at `aliasAngles` without a hand (holstered, lying in the world). A held gun is turned from the hand by
// its angle offsets (weaponAngleOffsets); the hand it would be held by is found by undoing them.
[[nodiscard]] glm::vec3 idleAttachmentAngles(const glm::vec3& aliasAngles, int slot, bool mirrored, const glm::vec3& offsets)
{
    const glm::vec3 o = weaponAngleOffsets(slot, mirrored);
    const glm::mat3 model = angleAxes({-aliasAngles.x, aliasAngles.y, aliasAngles.z});
    const glm::mat3 hand = model * glm::transpose(angleAxes({-o.x, o.y, o.z}));
    const glm::mat3 m = hand * angleAxes(offsets);
    return hands::anglesFromVectors(glm::normalize(m[0]), glm::normalize(m[2]));
}

// Clip sizes seen on the guns in the hands (the stats only give the held guns'), for the holstered ones' screens.
std::unordered_map<const qmodel_t*, int> clipSizes;

// The player's ammo for a gun not in a hand, or -1 when the client is not told it (the lava nails, multi-rockets
// and plasma are not in the stats).
[[nodiscard]] int idleAmmo(const qmodel_t* model)
{
    static constexpr std::pair<const char*, int> ammo[] = {{"progs/v_shot.mdl", STAT_SHELLS},
        {"progs/v_shot2.mdl", STAT_SHELLS}, {"progs/v_nail.mdl", STAT_NAILS}, {"progs/v_nail2.mdl", STAT_NAILS},
        {"progs/v_rock.mdl", STAT_ROCKETS}, {"progs/v_rock2.mdl", STAT_ROCKETS}, {"progs/v_prox.mdl", STAT_ROCKETS},
        {"progs/v_light.mdl", STAT_CELLS}, {"progs/v_laserg.mdl", STAT_CELLS}, {"progs/v_hammer.mdl", STAT_CELLS}};
    for(const auto& [name, stat] : ammo)
    {
        if(model && !strcmp(model->name, name))
        {
            return cl.stats[stat];
        }
    }
    return -1;
}

// The ammo screen's text of a gun not in a hand: as a held one's (its clip over the ammo when reloading), with what
// is known: the clip of a holstered gun (`clip` >= 0) and its size once seen in a hand, the player's ammo for it
// ("--" unknown).
[[nodiscard]] std::string idleWeaponText(const qmodel_t* model, int clip)
{
    const int ammo = idleAmmo(model);
    const std::string ammoText = ammo >= 0 ? std::to_string(ammo) : std::string{"--"};
    const bool reloading = vr_reload_mode.value != 0.f && vr_holster_mode.value == 0.f;
    const auto size = clipSizes.find(model);
    if(reloading && clip >= 0 && size != clipSizes.end() && size->second != 0)
    {
        return std::to_string(clip) + "/" + std::to_string(size->second) + "\n" + ammoText;
    }
    return ammoText;
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
    if(clipSize != 0)
    {
        clipSizes[ve.ent.model] = clipSize;
    }

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

// Switching a gun's ammo (its button) swaps its model for the other ammo's (the same gun, another paint and a
// few parts: improve_weapons_alt.py); over vr_weapon_morph_time the old model dissolves where the new one appears,
// a glowing seam sweeping between (the alias shader: VR_AliasMorph). The pairs, and the seam's colour (kind).
[[nodiscard]] int morphKind(const qmodel_t* a, const qmodel_t* b)
{
    static constexpr const char* pairs[][2] = {{"progs/v_nail.mdl", "progs/v_lava.mdl"},
        {"progs/v_nail2.mdl", "progs/v_lava2.mdl"}, {"progs/v_rock.mdl", "progs/v_multi.mdl"},
        {"progs/v_rock2.mdl", "progs/v_multi2.mdl"}, {"progs/v_light.mdl", "progs/v_plasma.mdl"}};
    static constexpr int kinds[] = {0, 0, 1, 1, 2}; // lava, multi-rockets, plasma
    if(!a || !b)
    {
        return -1;
    }
    for(int i = 0; i < static_cast<int>(std::size(pairs)); i++)
    {
        const char* x = pairs[i][0];
        const char* y = pairs[i][1];
        if((!strcmp(a->name, x) && !strcmp(b->name, y)) || (!strcmp(a->name, y) && !strcmp(b->name, x)))
        {
            return kinds[i];
        }
    }
    return -1;
}

struct Morph
{
    const qmodel_t* last{nullptr}; // the hand's gun model last frame
    qmodel_t* from{nullptr};       // morphing from this one (null: not morphing)
    int kind{0};
    double start{0.0};
};
Morph morphs[2];

// The morph's progress (0..1; 1 when not morphing), after following the hand's gun model.
[[nodiscard]] float updateMorph(int hand, qmodel_t* model)
{
    Morph& m = morphs[hand];
    const float time = vr_weapon_morph_time.value;
    if(model != m.last)
    {
        const int kind = morphKind(m.last, model);
        m.from = kind >= 0 && time > 0.f ? const_cast<qmodel_t*>(m.last) : nullptr;
        m.kind = std::max(kind, 0);
        m.start = realtime;
        m.last = model;
    }
    if(!m.from)
    {
        return 1.f;
    }
    const float t = time > 0.f ? static_cast<float>((realtime - m.start) / time) : 1.f;
    if(t >= 1.f || t < 0.f)
    {
        m.from = nullptr;
        return 1.f;
    }
    return t;
}

// The morph's value for the shader (vr_render.cpp VR_AliasMorph): + the model coming in, - the one going out.
[[nodiscard]] float morphValue(float t, int kind, bool incoming)
{
    const float v = 2.f * static_cast<float>(kind) + std::clamp(t, 1e-3f, 1.f);
    return incoming ? v : -v;
}

// Where a lava gun's glow comes from: along its barrels (between the hand and the muzzle), a little over them (a
// light inside the gun would light only the inside of its faces).
[[nodiscard]] glm::vec3 lavaGlowPosition(const entity_t& e, bool mirrored, float zeroBlend, int slot)
{
    const glm::vec3 grip{e.origin[0], e.origin[1], e.origin[2]}; // where the hand holds it (the controller)
    const glm::vec3 muzzle = view::entityAnchorPosition(e, mirrored, zeroBlend,
        static_cast<int>(weapons::value(slot, Key::MuzzleAnchorVertex)),
        weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
    const glm::mat3 axes = angleAxes({-e.angles[0], e.angles[1], e.angles[2]});
    return glm::mix(grip, muzzle, 0.55f) + axes[2] * 2.f;
}

// ----------------------------------------------------------------------------
// The weapons' hotspots (round 21, vr_weapons.hpp): where the other hand may hold them.

[[nodiscard]] bool hasGripHotspot(int slot)
{
    for(int i = 0; i < weapons::maxHotspots; i++)
    {
        if(weapons::isGripType(weapons::hotspot(slot, i).type))
        {
            return true;
        }
    }
    return false;
}

// Where a hotspot's point is drawn: the weapon entity's frame, as its model's vertices before their own scaling.
[[nodiscard]] glm::mat4 hotspotFrame(const entity_t& e, bool mirrored)
{
    float m[16];
    vec3_t origin, angles;
    VectorCopy(e.origin, origin);
    VectorCopy(e.angles, angles);
    R_EntityMatrix(m, origin, angles, ENTSCALE_DEFAULT);
    if(mirrored)
    {
        ApplyScale(m, 1.f, -1.f, 1.f);
    }
    if(const weapons::ModelTransform t = weapons::modelTransform(e.model); t.active)
    {
        ApplyScale(m, t.k, t.k, t.k);
        ApplyTranslation(m, t.offset.x, t.offset.y, t.offset.z);
    }
    glm::mat4 r;
    for(int c = 0; c < 4; c++)
    {
        for(int row = 0; row < 4; row++)
        {
            r[c][row] = m[c * 4 + row];
        }
    }
    return r;
}

// A turn given as the hands' angles (hands::angleVectors: the columns forward, left, up), and back.
[[nodiscard]] glm::mat3 anglesBasis(const glm::vec3& a)
{
    glm::vec3 f, r, u;
    hands::angleVectors(a, f, r, u);
    return glm::mat3{f, -r, u};
}
[[nodiscard]] glm::vec3 basisAngles(const glm::mat3& b)
{
    return hands::anglesFromVectors(glm::normalize(b[0]), glm::normalize(b[2]));
}

// A held weapon's turn at the holding hand's angles `rot`, as the hands' angles (setupWeapon draws its entity at
// these, the pitch negated).
[[nodiscard]] glm::vec3 weaponTurn(const glm::vec3& rot, int slot, bool mirrored)
{
    const glm::vec3 o = weaponAngleOffsets(slot, mirrored);
    return {rot.x - o.x, rot.y + o.y, rot.z + o.z};
}

// A hand's turn carried by a weapon (round 21, second pass). The weapon's Pitch/Yaw/Roll are added to the hand's
// angles, which is no rigid turn: a hand turned by angle offsets of its own would slide round the weapon as the wrist
// turns (up to a few degrees), and its grasp would be solved again and again. The hand keeps instead the turn it has
// on the weapon at `rot` 0 (`handAt0`: its angles there), however the weapon is turned.
// Round 21, third pass: the turn of the weapon held by `holder` at its hand's angles `rot`: posed as weaponTurn from the
// angles the hand had before its Hand and Weapon Together turn (hands::State::wholeTurn), then turned rigidly by it (the
// weapon's own offsets are Euler angles added to the hand's: turned by them, it would turn a little otherwise).
glm::mat3 wholeTurn[2]{glm::mat3{1.f}, glm::mat3{1.f}};

[[nodiscard]] glm::vec3 heldWeaponTurn(const glm::vec3& rot, int holder, int slot, bool mirrored)
{
    const glm::mat3& r = wholeTurn[holder];
    if(r == glm::mat3{1.f})
    {
        return weaponTurn(rot, slot, mirrored);
    }
    const glm::vec3 before = basisAngles(glm::transpose(r) * anglesBasis(rot));
    return basisAngles(r * anglesBasis(weaponTurn(before, slot, mirrored)));
}

[[nodiscard]] glm::vec3 attachedTurn(const glm::vec3& rot, int slot, bool mirrored, const glm::vec3& handAt0, int holder = -1)
{
    const glm::mat3 w = anglesBasis(holder >= 0 ? heldWeaponTurn(rot, holder, slot, mirrored) : weaponTurn(rot, slot, mirrored));
    const glm::mat3 w0 = anglesBasis(weaponTurn(glm::vec3{0.f}, slot, mirrored));
    return basisAngles(w * glm::transpose(w0) * anglesBasis(handAt0));
}

// The firing animation's move of a drawn weapon at the world point `at`: the rigid motion (least squares: Kabsch, by
// a polar decomposition) of the model's vertices round `at`, weighted by their distance beyond the nearest one's
// (over 7 cm), from its rest pose (frame 0) to the pose drawn this frame, as a world transform. A hand holding the
// weapon there (its arm, a hand steadying it) moves with it: the shotgun's kick takes the arm back with it, as the
// hand drawn at a vertex of the weapon did before round 21. The identity where the model doesn't move.
[[nodiscard]] glm::mat4 animationMotion(const view::ViewEntity& ve, const glm::vec3& at)
{
    static std::vector<glm::vec3> rest, now;
    if(!ve.ent.model || ve.ent.model->type != mod_alias || !anchor::posedVertices(ve.ent, ve.zeroBlend, rest, now))
    {
        return glm::mat4{1.f};
    }
    bool moved = false;
    for(std::size_t i = 0; i < rest.size() && !moved; i++)
    {
        moved = rest[i] != now[i];
    }
    if(!moved)
    {
        return glm::mat4{1.f};
    }

    float m[16];
    render::anchorMatrix(ve, glm::vec3{0.f}, m);
    glm::mat4 a;
    for(int c = 0; c < 16; c++)
    {
        a[c / 4][c % 4] = m[c];
    }
    const float sigma = 0.07f * units::metresToUnits();
    float nearest = 1e30f;
    for(glm::vec3& p : rest)
    {
        p = glm::vec3{a * glm::vec4{p, 1.f}};
        nearest = std::fmin(nearest, glm::distance(p, at));
    }
    double total = 0.0;
    glm::dvec3 restMid{0.0}, nowMid{0.0};
    static std::vector<float> weight;
    weight.resize(rest.size());
    for(std::size_t i = 0; i < rest.size(); i++)
    {
        now[i] = glm::vec3{a * glm::vec4{now[i], 1.f}};
        const float d = (glm::distance(rest[i], at) - nearest) / sigma;
        weight[i] = d < 4.f ? std::exp(-d * d) : 0.f;
        total += weight[i];
        restMid += glm::dvec3{rest[i]} * static_cast<double>(weight[i]);
        nowMid += glm::dvec3{now[i]} * static_cast<double>(weight[i]);
    }
    restMid /= total;
    nowMid /= total;
    glm::dmat3 h{0.0};
    double spread = 0.0;
    for(std::size_t i = 0; i < rest.size(); i++)
    {
        if(weight[i] > 0.f)
        {
            const glm::dvec3 r = glm::dvec3{rest[i]} - restMid, n = glm::dvec3{now[i]} - nowMid;
            h += glm::outerProduct(n, r) * static_cast<double>(weight[i]);
            spread += glm::dot(r, r) * weight[i];
        }
    }
    // The turn nearest h: its polar factor, with a touch of the identity (points in a line or a plane: the least turn).
    glm::dmat3 x = h + glm::dmat3{1.0} * (1e-4 * spread + 1e-12);
    for(int k = 0; k < 30; k++)
    {
        x = 0.5 * (x + glm::transpose(glm::inverse(x)));
    }
    if(!(glm::determinant(x) > 0.5))
    {
        x = glm::dmat3{1.0};
    }
    glm::mat4 out{glm::mat3{x}};
    out[3] = glm::vec4{glm::vec3{nowMid - x * restMid}, 1.f};
    return out;
}

// A share `t` of a rigid motion, about `centre`: its turn there slerped, the centre's move scaled.
[[nodiscard]] glm::mat4 partMotion(const glm::mat4& m, float t, const glm::vec3& centre)
{
    if(t >= 1.f)
    {
        return m;
    }
    t = std::fmax(t, 0.f);
    const glm::mat3 r = glm::mat3_cast(glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, glm::quat_cast(glm::mat3{m}), t));
    const glm::vec3 move = glm::vec3{m * glm::vec4{centre, 1.f}} - centre;
    glm::mat4 out{r};
    out[3] = glm::vec4{centre - r * centre + move * t, 1.f};
    return out;
}

// A pose (a place, the hands' angles) moved by a rigid motion.
void movePose(const glm::mat4& m, glm::vec3& pos, glm::vec3& angles)
{
    pos = glm::vec3{m * glm::vec4{pos, 1.f}};
    angles = basisAngles(glm::mat3{m} * anglesBasis(angles));
}

// Each hand's weapon's hotspots as drawn this frame (for the helping hand, the QC's weaponhotspot, their display).
struct WorldHotspot
{
    weapons::HotspotType type{weapons::HotspotType::None};
    glm::vec3 pos{0.f};   // a grip's point; a blade's grip's middle
    glm::vec3 end{0.f};   // a blade's: its grip's range, pos .. end
    float bias{0.f};
    float share{0.f};     // a blade's: the share of the way from the hand to the tip
    glm::vec3 angles{0.f}; // the helping hand's turn there
    weapons::HotspotStyle style{weapons::HotspotStyle::Wrap};
    float overlap{weapons::defaultOverlap}; // round 21, third pass: the hand's overlap there (0..1), its drawn pose's offset
    glm::vec3 visualPos{0.f};
    glm::vec3 visualAngles{0.f};
    weapons::Hotspot def; // as set (its fingers set by hand, among the rest)
};
WorldHotspot worldHotspots[2][weapons::maxHotspots];
int chosenGrip[2]{-1, -1}; // per holding hand: the grip hotspot the other hand holds, or last took

// Round 21's migration (weapons::takeHotspotMigration, vr_hotspots_legacy): a slot's two-handed grip keys (the
// foregrip, the sword's blade grip) as hotspots, exactly where they were drawn. The old foregrip was placed as
// its anchor vertex and offset (TwoHHandAnchorVertex, TwoHFixedOffset) in the weapon's frame mirrored as the
// helping hand is (not as the weapon is drawn); it is worked out for the weapon drawn in the main hand, at the
// origin, unturned: the same point for either hand. With `fromDefaults` the keys' defaults are used (the
// weapon's offset and scale too), else their values. Needs the model and a Quake VR server (its transforms).
bool legacyHotspots(int slot, bool fromDefaults, weapons::Hotspot out[2], int& count)
{
    count = 0;
    const cvar_t* id = weapons::cvar(slot, Key::ID);
    qmodel_t* model = id && id->string[0] ? Mod_ForName(id->string, false) : nullptr;
    if(!model || model->type != mod_alias || !(cl.protocolflags & PRFL_QUAKEVR))
    {
        return false;
    }
    const auto value = [&](Key key) {
        const cvar_t* var = weapons::cvar(slot, key);
        return fromDefaults ? static_cast<float>(Q_atof(var->default_string)) : var->value;
    };

    // The weapon's offset and scale as the keys have them (the defaults' for a moment, if asked).
    constexpr Key shape[] = {Key::OffsetX, Key::OffsetY, Key::OffsetZ, Key::Scale};
    std::string kept[4];
    if(fromDefaults)
    {
        for(int i = 0; i < 4; i++)
        {
            cvar_t* var = weapons::cvar(slot, shape[i]);
            kept[i] = var->string;
            Cvar_SetQuick(var, var->default_string);
        }
    }

    if(value(Key::TwoHDisplayMode) == 1.f)
    {
        view::ViewEntity ve;
        ve.ent.model = model;
        ve.ent.scale = ENTSCALE_DEFAULT;
        ve.mirrored = true; // as the helping hand of a gun in the main hand
        const glm::vec3 w = view::anchorPosition(ve, static_cast<int>(value(Key::TwoHHandAnchorVertex)),
            glm::vec3{value(Key::TwoHFixedOffsetX), value(Key::TwoHFixedOffsetY), value(Key::TwoHFixedOffsetZ)});
        // anchorPosition zero-blends by the entity's: frame 0 here (the idle pose).
        const glm::vec3 h = glm::vec3{glm::inverse(hotspotFrame(ve.ent, false)) * glm::vec4{w, 1.f}};
        out[count++] = {weapons::HotspotType::Grip, h, 0.f};
    }
    if(value(Key::TwoHBladeGrip) > 0.f)
    {
        out[count++] = {weapons::HotspotType::Blade, glm::vec3{value(Key::TwoHBladeGrip), 0.f, 0.f}, 0.f};
    }

    if(fromDefaults)
    {
        for(int i = 0; i < 4; i++)
        {
            Cvar_SetQuick(weapons::cvar(slot, shape[i]), kept[i].c_str());
        }
    }
    return true;
}

// vr_show_weapon_hotspots: the held weapons' hotspots marked (grips green, blades' grips orange, the one the Weapon
// Offsets page edits white), each with a faint ball as big as its bias.
// The hotspots of the weapon in `hand` marked, `edited` (-1 none) white.
void drawHotspots(int hand, int edited)
{
    for(int i = 0; i < weapons::maxHotspots; i++)
    {
        const WorldHotspot& w = worldHotspots[hand][i];
        if(w.type == weapons::HotspotType::None)
        {
            continue;
        }
        const glm::vec4 colour = i == edited                               ? glm::vec4{1.f, 1.f, 1.f, 1.f}
                                 : w.type == weapons::HotspotType::Grip ? glm::vec4{0.2f, 1.f, 0.3f, 1.f}
                                 : w.type == weapons::HotspotType::Cup  ? glm::vec4{0.3f, 0.6f, 1.f, 1.f}
                                                                        : glm::vec4{1.f, 0.6f, 0.1f, 1.f};
        if(w.type == weapons::HotspotType::Blade)
        {
            lines::line(w.pos, w.end, 0.35f, colour, colour);
        }
        lines::point(w.pos, 1.2f, colour);
        if(w.bias > 0.f)
        {
            lines::point(w.pos, 2.f * w.bias, glm::vec4{colour.r, colour.g, colour.b, 0.15f});
        }
    }
}

void showHotspots()
{
    if(!vr_show_weapon_hotspots.value)
    {
        return;
    }
    const int edited = CLAMP(1, static_cast<int>(vr_weapon_hotspot.value), weapons::maxHotspots) - 1;
    for(int hand = 0; hand < 2; hand++)
    {
        if(entities.weapon[hand].visible)
        {
            drawHotspots(hand, edited);
        }
    }
}

// The motion review's ghost (view::setGhost): asked for this frame, per hand.
struct GhostRequest
{
    qmodel_t* model{nullptr};
    glm::vec3 pos{0.f};
    glm::vec3 rot{0.f};
    float alpha{0.5f};
    int frame{-1}; // host_framecount it was asked for in
};
GhostRequest ghostRequests[2];

void setupGhosts()
{
    for(int hand = 0; hand < 2; hand++)
    {
        view::ViewEntity& ve = entities.ghost[hand];
        const GhostRequest& g = ghostRequests[hand];
        if(g.frame != host_framecount || !g.model)
        {
            ve.visible = false;
            continue;
        }
        // Placed as setupWeapon places a held weapon (the hand's pose, the weapon's angle offsets; no carried gun,
        // no Hand and Weapon Together turn: a take doesn't record them), the empty hand as the fist's slot.
        const bool mirrored = hand == HAND_OFF;
        const int slot = isHandModel(g.model) ? weapons::fistSlot() : weapons::slotForModel(g.model);
        const glm::vec3 wt = slot >= 0 ? weaponTurn(g.rot, slot, mirrored) : g.rot;
        place(ve, g.model, g.pos, {-wt.x, wt.y, wt.z}, 0, mirrored);
        ve.ent.alpha = static_cast<unsigned char>(ENTALPHA_ENCODE(CLAMP(0.05f, g.alpha, 1.f)));
        ve.zeroBlend = 0.f;
        ve.morph = 0.f;
        ve.lightMultiply = true;
        ve.lightMod = glm::vec3{1.2f, 1.7f, 2.6f}; // a bright cold tint: not the player's own
    }
}

// `floating`: the posing mode's weapon (vr_posing.cpp), never a carried gun.
void setupWeapon(hands::State& s, int hand, qmodel_t* model, int frame, bool floating = false)
{
    view::ViewEntity& ve = entities.weapon[hand];
    const int slot = weapons::slotForModel(model);

    // A gun hanging from its foregrip (the hand-off, vr_twohand.cpp): drawn as the hand that let it go
    // held it, carried by this hand.
    twohand::HeldAs held{s.pos[hand], s.visualRot[hand], hand == HAND_OFF};
    const bool carried = !floating && twohand::carriedWeapon(s, hand, held);
    weaponCarried[hand] = carried;
    const bool mirrored = held.mirrored;
    drawnAs[hand] = held; // what the weapon's attachments (its button) follow


    const glm::vec3& rot = held.rot;
    // (A carried gun is drawn as the hand that let it go held it, without this hand's offset turn.)
    const glm::vec3 wt = carried ? weaponTurn(rot, slot, mirrored) : heldWeaponTurn(rot, hand, slot, mirrored);

    place(ve, model, held.pos, {-wt.x, wt.y, wt.z}, frame, mirrored);

    // A config's own two-handed grips, once, as hotspots (round 21).
    if(model && slot >= 0 && weapons::takeHotspotMigration(slot))
    {
        weapons::Hotspot migrated[2];
        int count = 0;
        if(legacyHotspots(slot, false, migrated, count))
        {
            for(int i = 0; i < weapons::maxHotspots; i++)
            {
                weapons::setHotspot(slot, i, i < count ? migrated[i] : weapons::Hotspot{});
            }
            Con_DPrintf("%s: its two-handed grips are now %d hotspots\n", model->name, count);
        }
    }

    // Steadied in the "fixed" two-handed display mode (a grip hotspot: the helping hand drawn on it): the weapon's
    // own blend towards frame 0 for that grip (old engine's V_SetupHandViewEnts).
    const bool fixed2H = model && slot >= 0 && hasGripHotspot(slot);
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

    // The hotspots, as drawn; a blade's grip between its share of the way from the hand to the tip, less and more.
    const glm::mat4 hsFrame = hotspotFrame(ve.ent, mirrored);
    for(int i = 0; i < weapons::maxHotspots; i++)
    {
        const weapons::Hotspot h = model && slot >= 0 ? weapons::hotspot(slot, i) : weapons::Hotspot{};
        WorldHotspot& w = worldHotspots[hand][i];
        w = WorldHotspot{h.type, glm::vec3{0.f}, glm::vec3{0.f}, h.bias, 0.f, h.angles, h.style, h.overlap, h.visualPos,
            h.visualAngles, h};
        if(weapons::isGripType(h.type))
        {
            w.pos = w.end = glm::vec3{hsFrame * glm::vec4{h.pos, 1.f}};
        }
        else if(h.type == weapons::HotspotType::Blade && s.muzzleValid[hand])
        {
            w.share = h.pos.x;
            const glm::vec3 toTip = s.muzzle[hand] - held.pos;
            w.pos = held.pos + toTip * std::max(0.3f, h.pos.x - 0.3f);
            w.end = held.pos + toTip * 1.05f;
        }
    }

    s.grip2HValid[hand] = fixed2H && !carried;
    if(carried && model && slot >= 0)
    {
        // Where the hand that takes it back closes: its handle.
        twohand::setCarriedHandle(hand, glm::vec3{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]});
    }
    if(s.grip2HValid[hand])
    {
        // The grip the other hand takes: the one nearest it, less its bias; kept while it holds it.
        int& chosen = chosenGrip[hand];
        const bool holdsOne = twohand::helping(1 - hand) && chosen >= 0 && weapons::isGripType(worldHotspots[hand][chosen].type);
        if(!holdsOne)
        {
            float best = 1e30f;
            for(int i = 0; i < weapons::maxHotspots; i++)
            {
                // A cup is where the helping hand's palm goes: taken by the palm (round 21, third pass).
                // (As tracked: the hands drawn out of each other and the body, vr_body_collide, choose as without it.)
                const WorldHotspot& w = worldHotspots[hand][i];
                const glm::vec3 from = w.type == weapons::HotspotType::Cup ? hands::palmPoint(s, 1 - hand) : s.pos[1 - hand];
                const float d = glm::distance(from - selfcollide::drawnOffset(1 - hand), w.pos - selfcollide::drawnOffset(hand)) - w.bias;
                if(weapons::isGripType(w.type) && d < best)
                {
                    best = d;
                    chosen = i;
                }
            }
        }
        s.grip2H[hand] = worldHotspots[hand][chosen].pos;
        s.grip2HBias[hand] = worldHotspots[hand][chosen].bias;
        // A cup: a cup hotspot, or a grip not ahead of the holding hand (beside it, under it: a grip the two hands can't
        // aim by; it is held as a cup).
        const WorldHotspot& spot = worldHotspots[hand][chosen];
        glm::vec3 fwd, right, up;
        hands::angleVectors(s.rot[hand], fwd, right, up);
        const glm::vec3 toGrip = spot.pos - held.pos;
        const float along = glm::dot(toGrip, fwd);
        s.grip2HCup[hand] = spot.type == weapons::HotspotType::Cup ||
                            along < 5.f || along < std::cos(glm::radians(35.f)) * glm::length(toGrip);
        s.grip2HPalm[hand] = spot.type == weapons::HotspotType::Cup;
    }

    // Just drawn from a holster: eased from its holstered pose into the hand (vr_drawblend.cpp; drawn only, the muzzle
    // and the grips above are its real place).
    drawblend::hand(s, hand, ve.ent, !floating && !carried && model && slot >= 0 && slot != weapons::fistSlot() && !isHandModel(model));

    // The empty hand's "weapon" is a hand model; hands are drawn separately.
    if(isHandModel(model))
    {
        ve.visible = false;
    }
    else if(model && slot >= 0)
    {
        queueWeaponText(held.rot, mirrored, hand, ve, slot);
    }

    // Morphing from the other ammo's model (vr_weapon_morph_time): that one too, where it would be held, going out.
    view::ViewEntity& old = entities.weaponMorph[hand];
    const float t = updateMorph(hand, model);
    const Morph& m = morphs[hand];
    const int oldSlot = weapons::slotForModel(m.from);
    ve.morph = 0.f;
    old.morph = 0.f;
    if(m.from && oldSlot >= 0 && ve.visible)
    {
        const glm::vec3 ot = carried ? weaponTurn(rot, oldSlot, mirrored) : heldWeaponTurn(rot, hand, oldSlot, mirrored);
        place(old, m.from, held.pos, {-ot.x, ot.y, ot.z},
            std::clamp(frame, 0, std::max(m.from->numframes - 1, 0)), mirrored);
        const bool oldFixed2H = hasGripHotspot(oldSlot);
        old.zeroBlend = weapons::value(oldSlot, oldFixed2H && twohand::helping(1 - hand) ? Key::TwoHZeroBlend : Key::ZeroBlend);
        ve.morph = morphValue(t, m.kind, true);
        old.morph = morphValue(t, m.kind, false);
    }
    else
    {
        old.visible = false;
    }

    // The lava nailguns glow (vr_lavagun_light), fading in and out with a morph.
    const bool lavaIn = ve.visible && slot >= 0 && emissive::isLavaGun(model);
    const bool lavaOut = old.visible && emissive::isLavaGun(m.from);
    if(lavaIn || lavaOut)
    {
        const view::ViewEntity& lit = lavaIn ? ve : old;
        emissive::lavaGunLight(hand, lavaGlowPosition(lit.ent, mirrored, lit.zeroBlend, lavaIn ? slot : oldSlot),
            (lavaIn ? t : 0.f) + (lavaOut ? 1.f - t : 0.f));
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

    // On a held weapon, its thumb placement (fgr_thumb_x/y/z; round 21: the fingers wrap the grip on their own).
    const int slot = weapons::heldSlot(hand);
    const bool holding = slot >= 0 && slot != weapons::fistSlot() && !twohand::helping(hand);
    // On the held torch, its grip's (vr_flashlight_low_thumb_*, _high_thumb_*).
    flashlight::Fingers torch;
    const bool torchHeld = !holding && flashlight::fingers(hand, torch);

    switch(finger)
    {
        case FingerThumb:
            return result + glm::vec3{vr_finger_thumb_x.value, vr_finger_thumb_y.value,
                                vr_finger_thumb_z.value} +
                   (holding     ? weapons::vec(slot, Key::FingerThumbX, Key::FingerThumbY, Key::FingerThumbZ)
                       : torchHeld ? torch.thumb
                                   : glm::vec3{0.f});
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

// ----------------------------------------------------------------------------
// The jointed hand (vr_hand_rig, vr_handrig.cpp): drawn instead of the palm and the five finger models, as
// entities.hand[hand][FingerBase] (the others hidden), posed here: each joint of a finger at its curl.

// What a hand holds, for its fingers to wrap (vr_hand_fit, vr_grasp.cpp): an entity drawn this frame, and the pose
// of its model to wrap (-1: its own).
struct Held
{
    const entity_t* ent{nullptr};
    bool mirrored{false};
    int frame{-1};
    bool weapon{false}; // a weapon (its own, or the other hand's it helps hold): the palm fit's own limit
    bool trigger{false}; // its own weapon: the index finger pulls the trigger with the controller's
    bool cup{false};     // the other hand's weapon, by a cup hotspot: the other hand is in the way too
    bool thumbTop{false}; // the hotspot's style: the thumb along the top
    float overlap{-1.f};  // cm the fingers and palm may sink into it (round 21, third pass: the weapon's or the hotspot's
                          // overlap slider); negative: vr_hand_fit_overlap's (things)
    // Round 21, third pass: the fingers set by hand (the weapon's or the hotspot's Fingers: Manual), no solve: each
    // finger's curl (0..1 of the fist; thumb first) and the thumb across the palm (0..1).
    bool manual{false};
    float manualCurl[handrig::FingerCount]{};
    float manualThumbAcross{0.f};
    // Round 21, third pass: where it is in the hand's rig, as the settings put it (a weapon in its hand, the other hand
    // on a hotspot: they move and turn together), worked out at the controller's origin, unturned: the grasp is solved
    // there, the same wherever the hands are (else from where it is drawn this frame, which differs from it by float
    // noise, a hundredth of a unit: enough to flip the thumb between two holds nearly as good).
    bool canonical{false};
    glm::mat4 canonicalInRig{1.f};
    // And, with a tuning offset on the hand (the weapon's Hand Only, a hotspot's Held Hand), the place without it: the
    // palm is fitted there, and the offset moves the fitted hand (its fingers wrapping again where it puts them), so
    // the offset moves the hand by what it says (a fit searched again would pull it back towards the best grip).
    bool offset{false};
    glm::mat4 fitInRig{1.f};
};

// The weapon's (Key::FingerManual, FingerCurl*) or a hotspot's fingers set by hand, into `held`.
void setManualFingers(Held& held, bool manual, const float curl[handrig::FingerCount], float thumbAcross)
{
    held.manual = manual;
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        held.manualCurl[f] = CLAMP(0.f, curl[f], 1.f);
    }
    held.manualThumbAcross = CLAMP(0.f, thumbAcross, 1.f);
}

// A hand's grasp: what it was solved against (the held thing's model, pose and place in the hand's rig space, the
// fingers' places), and the fingers' stops.
struct Grasp
{
    bool valid{false};
    // Round 21, third pass: solved afresh where what it holds rests in the hand (its place then), so that the grip is
    // the same however it came there.
    bool rested{false};
    glm::mat4 restInRig{1.f};
    const qmodel_t* model{nullptr};
    int frame{-1};
    glm::mat4 inRig{1.f};
    glm::vec3 shift[handrig::FingerCount]{};
    grasp::Settings settings;
    grasp::Solution solution;
    float other[handrig::FingerCount * handrig::jointsPerFinger]{}; // a cup's: the other hand's joints it was solved for
    glm::mat4 otherInRig{1.f};                                      // and where that hand was
    unsigned rig{0};                                                // handrig::generation() it was solved with
};

struct RigHand
{
    bool drawn{false};
    handrig::Pose pose;
    handrig::Posed posed;
    std::array<float, handrig::data::numJoints * 12> skin{};
    Grasp grasp;
    float joints[handrig::FingerCount][handrig::jointsPerFinger]{}; // drawn curls, eased towards the grasp's
    glm::vec3 palm{0.f}; // the drawn hand's move (rig space), eased towards the grasp's
    glm::quat turn{1.f, 0.f, 0.f, 0.f}; // and its turn
    glm::quat thumb{1.f, 0.f, 0.f, 0.f}; // the thumb's metacarpal turn drawn, eased towards the grasp's
    Held held;           // what it held last frame, and where the hand was (vr_grasp_dump)
    glm::mat4 rigToWorld{1.f};
    double jointsTime{-1.0};
    glm::mat4 inRig{1.f}; // where what it holds is in the hand this frame (vr_debug_grasp_trace)
    glm::mat4 solveRig{1.f}; // the rig's place the grasp is solved at, and its size (vr_grasp_bench)
    float rigUnit{0.f};
    glm::vec3 pushed{0.f};   // drawn out of the other hand's weapon (vr_hand_collide), eased
    double pushedTime{-1.0};
    // Brushing the other hand's weapon (vr_hand_collide, pushOut): the weapon, and the fingers solved against it
    // (setupRigHand: they rest on it or bend out of it); a finger in it at every curl ("stuck") pushes the hand instead.
    const entity_t* brushEnt{nullptr};
    bool brushMirrored{false};
    bool brushValid{false};
    grasp::Solution brush;
    Grasp brushSolve; // where it was solved (solved again when the hand has moved on it: sameGrasp)
    bool brushStuck[handrig::FingerCount]{};
    float brushHold[handrig::FingerCount][handrig::jointsPerFinger]{}; // each finger's last pose clear of it
    bool brushHeld[handrig::FingerCount]{};
    // The palm's fit without the tuning offset (Held::offset): where it was solved, its settings, the palm's move.
    bool fitValid{false};
    glm::mat4 fitInRig{1.f};
    grasp::Settings fitSettings;
    const qmodel_t* fitModel{nullptr};
    glm::vec3 fitPalm{0.f};
    glm::quat fitTurn{1.f, 0.f, 0.f, 0.f};
    glm::mat4 drawnToWorld{1.f}; // the rig as drawn (the palm's fit and the firing motion in): vr_debug_hand_bones
};
RigHand rigHands[2];

constexpr int rigFinger[handrig::FingerCount] = {FingerThumb, FingerIndex, FingerMiddle, FingerRing, FingerPinky};

[[nodiscard]] glm::vec3 vec3Of(const float* v)
{
    return {v[0], v[1], v[2]};
}

// A finger offset (fingerOffset's hand units, y to the hand's right) in model space, at the offsets' scale.
[[nodiscard]] glm::vec3 offsetInModel(const glm::vec3& v)
{
    return glm::vec3{v.x, -v.y, v.z} * weapons::offsetScale();
}

// A finger's curl as drawn without a grasp: the controller's (whole frames or between them: the joints turn smoothly),
// with a held weapon's finger tweak.
[[nodiscard]] float rigCurl(int hand, int finger)
{
    return CLAMP(0.f, fingerFrames[hand][finger] + 5.f * fingerBias[hand][finger], 5.f);
}

[[nodiscard]] glm::mat4 toMat4(const float m[16])
{
    glm::mat4 r;
    for(int c = 0; c < 4; c++)
    {
        for(int row = 0; row < 4; row++)
        {
            r[c][row] = m[c * 4 + row];
        }
    }
    return r;
}

// Whether `g` was solved for `held` where it is in the hand now: turned no more than `degrees`, moved no more than
// `units` (hand units) from it, the fingers where they were. By default, vr_hand_fit_resolve's (cm, and twice as many
// degrees; `rigUnit` world units a hand unit).
[[nodiscard]] bool sameGrasp(const Grasp& g, const qmodel_t* model, int frame, const glm::mat4& inRig, const handrig::Pose& pose,
    float rigUnit)
{
    const float cm = std::fmax(vr_hand_fit_resolve.value, 0.f);
    const float degrees = 2.f * cm;
    const float units = rigUnit > 0.f ? cm * 0.01f * units::metresToUnits() / rigUnit : 0.f;
    if(!g.valid || g.model != model || g.frame != frame)
    {
        return false;
    }
    if(glm::distance(glm::vec3{inRig[3]}, glm::vec3{g.inRig[3]}) > units)
    {
        return false;
    }
    const float cosine = std::cos(glm::radians(degrees));
    for(int c = 0; c < 3; c++)
    {
        const glm::vec3 a{inRig[c]}, b{g.inRig[c]};
        if(glm::dot(a, b) < cosine * glm::length(a) * glm::length(b))
        {
            return false;
        }
    }
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        if(glm::distance(pose.shift[f], g.shift[f]) > 0.01f)
        {
            return false;
        }
    }
    return true;
}

// A rig point as drawn, moved and turned by the palm's fit, in the rig's space before them.
[[nodiscard]] glm::vec3 drawnInRig(const RigHand& rh, const glm::vec3& p)
{
    static const glm::vec3 c = grasp::palmCentre();
    return c + rh.palm + glm::mat3_cast(rh.turn) * (p - c);
}

int graspSolves[2]{}; // solves, per hand (vr_debug_grasp_trace)
int brushSolves[2]{}; // brushing the other hand's weapon: solves, per hand

void printGrasp(int hand, const Grasp& g, float rigUnit)
{
    Con_Printf("grasp (%.2f s): %s hand, %s: %d triangles within reach; the palm moved %.2f cm (%.1f %.1f %.1f), the thumb "
               "turned %.0f (choice %d), the palm %.0f; %.1f us%s; held at (%.3f %.3f %.3f) in the hand\n",
        realtime, hand == HAND_MAIN ? "main" : "off", g.model ? g.model->name : "-", g.solution.triangles,
        glm::length(g.solution.palm) * rigUnit / units::metresToUnits() * 100.f, g.solution.palm.x, g.solution.palm.y,
        g.solution.palm.z, glm::degrees(glm::angle(g.solution.thumbTurn)), g.solution.thumbChoice,
        glm::degrees(glm::angle(g.solution.palmTurn)), g.solution.seconds * 1e6, g.rested ? ", afresh at rest" : "",
        g.inRig[3][0], g.inRig[3][1], g.inRig[3][2]);
    if(vr_debug_grasp.value >= 2.f)
    {
        constexpr const char* names[handrig::FingerCount] = {"thumb", "index", "middle", "ring", "pinky"};
        for(int f = 0; f < handrig::FingerCount; f++)
        {
            const grasp::FingerStop& st = g.solution.finger[f];
            Con_Printf("  %-6s %s stops %.3f %.3f %.3f\n", names[f],
                st.startsInside ? "inside" : st.fromClosed ? "met (from closed)" : st.met ? "met" : "free", st.stop[0], st.stop[1],
                st.stop[2]);
        }
    }
}

// The solve's settings for what `held` is: the palm's move (vr_hand_fit_palm or _weapon, cm) and turn, the overlap
// (cm), in hand units (`rigUnit` world units each).
[[nodiscard]] grasp::Settings graspSettings(const Held& held, float rigUnit)
{
    const float toRig = rigUnit > 0.f ? 0.01f * units::metresToUnits() / rigUnit : 0.f;
    grasp::Settings s;
    s.palmLimit = std::fmax(held.weapon ? vr_hand_fit_palm_weapon.value : vr_hand_fit_palm.value, 0.f) * toRig;
    s.palmTurnLimit = std::fmax(vr_hand_fit_palm_turn.value, 0.f);
    // Round 21, third pass: a hand on a weapon (its own, or a hotspot of the other hand's) has its palm's place searched
    // near where the weapon's placement or the hotspot puts it (within vr_hand_fit_palm_weapon: along the grip and the
    // fingers, flush on it) for where the fingers hold best, never turned. The turn (up to 20 degrees towards the
    // surface the palm's normal met first) took the super nailgun's frame or its handle by a hair's difference in the
    // weapon's place at the grab: two grips, by turns. (Solved afresh once the weapon rests in the hand, updateGrasp:
    // the same grip however it was taken.) A cup keeps the fit it had (flush on the other hand and the weapon, turned
    // towards them): its place in the hand is worked out from the settings now (Held::canonical), so it is the same
    // every time, and a config's cups moved to the new definition look as they did.
    if(held.weapon && !held.cup)
    {
        s.palmTurnLimit = 0.f;
        s.searchPlace = true;
    }
    s.overlap = std::fmax(held.overlap >= 0.f ? held.overlap : vr_hand_fit_overlap.value, 0.f) * toRig;
    s.thenar = !held.weapon;
    s.thumbTop = held.thumbTop;
    return s;
}

// The hand's grasp of `held` (vr_grasp.cpp): solved on the spot, in the frame it is needed, when what it holds, its
// place in the hand (beyond vr_hand_fit_resolve) or the settings changed; else the last one stands. Tens of
// microseconds: no thread, the same inputs the same pose.
void updateGrasp(int hand, const Held& held, const glm::mat4& rigMatrix, float rigUnit)
{
    RigHand& rh = rigHands[hand];
    Grasp& g = rh.grasp;
    if(g.rig != handrig::generation()) // the hand was edited (vr_hand_reload): solved afresh
    {
        g.valid = false;
        g.rig = handrig::generation();
    }
    if(!held.ent || !held.ent->model || !vr_hand_fit.value)
    {
        g.valid = false;
        return;
    }
    const int frame = held.frame >= 0 ? held.frame : held.ent->frame;
    const grasp::Shape* shape = grasp::shapeOf(*held.ent, frame);
    if(!shape)
    {
        g.valid = false;
        return;
    }
    const glm::mat4 inRig =
        held.canonical ? held.canonicalInRig : glm::inverse(rigMatrix) * grasp::shapeToWorld(*held.ent, held.mirrored);
    // Whether two places in the hand are the same, to a hundredth of a hand unit and a tenth of a degree; whether it
    // rests (where it was last frame).
    const auto samePlace = [](const glm::mat4& p, const glm::mat4& q) {
        if(glm::distance(glm::vec3{p[3]}, glm::vec3{q[3]}) >= 0.01f)
        {
            return false;
        }
        for(int c = 0; c < 3; c++)
        {
            const glm::vec3 a{p[c]}, b{q[c]};
            if(glm::dot(a, b) < std::cos(glm::radians(0.1f)) * glm::length(a) * glm::length(b))
            {
                return false;
            }
        }
        return true;
    };
    const bool steady = samePlace(inRig, rh.inRig);
    rh.inRig = inRig;
    rh.solveRig = rigMatrix;
    rh.rigUnit = rigUnit;

    // Cupping the other hand (a two-handed pistol grip): that hand is in the way too, as drawn (its fingers move with
    // its trigger and grip: solved again when they, or it, move).
    const RigHand& otherHand = rigHands[1 - hand];
    const bool cup = held.cup && otherHand.drawn;
    float other[handrig::FingerCount * handrig::jointsPerFinger]{};
    glm::mat4 otherInRig{1.f};
    bool otherMoved = false;
    if(cup)
    {
        otherInRig = glm::inverse(rigMatrix) * otherHand.rigToWorld;
        float change = 0.f;
        for(int f = 0; f < handrig::FingerCount; f++)
        {
            for(int j = 0; j < handrig::jointsPerFinger; j++)
            {
                other[f * handrig::jointsPerFinger + j] = otherHand.pose.curl[f][j];
                change += std::fabs(other[f * handrig::jointsPerFinger + j] - g.other[f * handrig::jointsPerFinger + j]);
            }
        }
        otherMoved = change > 0.1f || glm::distance(glm::vec3{otherInRig[3]}, glm::vec3{g.otherInRig[3]}) > 0.1f;
    }

    // The settings changed: solved again.
    grasp::Settings settings = graspSettings(held, rigUnit);
    if(held.canonical && held.offset)
    {
        // The palm fitted without the tuning offset (solved again when that place or the settings change), kept as the
        // offset moves the hand.
        const bool fitSame = rh.fitValid && rh.fitModel == held.ent->model && rh.fitInRig == held.fitInRig &&
                             rh.fitSettings.palmLimit == settings.palmLimit && rh.fitSettings.palmTurnLimit == settings.palmTurnLimit &&
                             rh.fitSettings.overlap == settings.overlap && rh.fitSettings.searchPlace == settings.searchPlace &&
                             rh.fitSettings.thenar == settings.thenar;
        if(!fitSame)
        {
            QVR_PROFILE("grasp solve");
            grasp::Solution fit;
            grasp::solve(rh.pose, *shape, held.fitInRig, settings, nullptr, fit);
            rh.fitValid = true;
            rh.fitModel = held.ent->model;
            rh.fitInRig = held.fitInRig;
            rh.fitSettings = settings;
            rh.fitPalm = fit.palm;
            rh.fitTurn = fit.palmTurn;
        }
        settings.fixedPalm = true;
        settings.palmMove = rh.fitPalm;
        settings.palmTurnMove = rh.fitTurn;
    }
    const bool settingsChanged = settings.palmLimit != g.settings.palmLimit || settings.palmTurnLimit != g.settings.palmTurnLimit ||
                                 settings.overlap != g.settings.overlap || settings.thenar != g.settings.thenar ||
                                 settings.thumbTop != g.settings.thumbTop || settings.searchPlace != g.settings.searchPlace ||
                                 settings.fixedPalm != g.settings.fixedPalm || settings.palmMove != g.settings.palmMove ||
                                 settings.palmTurnMove != g.settings.palmTurnMove;
    bool fresh = false;
    if(!settingsChanged && !otherMoved && sameGrasp(g, held.ent->model, frame, inRig, rh.pose, rigUnit))
    {
        // Resting in the hand somewhere it wasn't solved afresh at (it moved in the hand, as a hand takes a grip, and
        // the solves then started from the one before): solved once more, afresh (round 21, third pass). A warm solve
        // keeps what it can of the one before, so where it rests after depended on how it came there (a foregrip
        // taken ten times: the palm 1.3 units apart); afresh, it is the same grip however it was taken.
        if(!steady || (g.rested && samePlace(inRig, g.restInRig)))
        {
            return;
        }
        fresh = true;
    }

    QVR_PROFILE("grasp solve");
    const bool same = !fresh && g.valid && g.model == held.ent->model && g.frame == frame;
    grasp::Solution solution;
    if(cup)
    {
        static grasp::Shape otherShape;
        static std::vector<grasp::Triangle> tris;
        tris.clear();
        const auto at = [&](const glm::vec3& p) { return glm::vec3{otherInRig * glm::vec4{drawnInRig(otherHand, p), 1.f}}; };
        static std::vector<glm::vec3> posed;
        handrig::vertices(otherHand.posed, posed);
        for(const auto& tri : handrig::rig().triangles)
        {
            tris.push_back({{at(posed[tri[0]]), at(posed[tri[1]]), at(posed[tri[2]])}});
        }
        grasp::makeShape(tris, otherShape);
        const float toRig = rigUnit > 0.f ? 0.01f * units::metresToUnits() / rigUnit : 0.f;
        grasp::solve(rh.pose, *shape, inRig, settings, same ? &g.solution : nullptr, solution, &otherShape, glm::mat4{1.f},
            std::fmax(vr_hand_fit_overlap_hands.value, 0.f) * toRig);
    }
    else
    {
        grasp::solve(rh.pose, *shape, inRig, settings, same ? &g.solution : nullptr, solution);
    }
    std::copy(other, other + handrig::FingerCount * handrig::jointsPerFinger, g.other);
    g.otherInRig = otherInRig;
    g.valid = true;
    g.rested = !same && steady;
    g.restInRig = inRig;
    g.model = held.ent->model;
    g.frame = frame;
    g.inRig = inRig;
    g.settings = settings;
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        g.shift[f] = rh.pose.shift[f];
    }
    g.solution = solution;
    graspSolves[hand]++;
    if(vr_debug_grasp.value)
    {
        printGrasp(hand, g, rigUnit);
    }
}

// How far past its stop on the weapon the trigger finger curls at a full pull (curl frames per joint: the knuckle
// little, the two distal joints more, a hook round the trigger).
constexpr float triggerPull[handrig::jointsPerFinger] = {0.6f, 1.4f, 1.2f};

// How much a finger grips (0..1) at the controller's curl: from half closed to nearly the full press.
[[nodiscard]] float engagement(float curl)
{
    const float t = std::fmin(std::fmax((curl - 2.5f) / 2.f, 0.f), 1.f);
    return t * t * (3.f - 2.f * t);
}

// The jointed hand's rig to the world for the hand at `pos` turned `handRot` (setupRigHand's placement: the entity where
// the palm model's origin is drawn), placing `ve` there if given.
glm::mat4 rigPlacement(int hand, const glm::vec3& pos, const glm::vec3& handRot, bool mirrored, view::ViewEntity* ve)
{
    qmodel_t* const model = viewModel(handrig::modelName);
    const weapons::ModelTransform t = weapons::modelTransform(model);
    const float k = t.active ? t.k : 1.f;
    const glm::vec3 ts = t.active ? t.scale : glm::vec3{1.f};
    const glm::vec3 mBase = offsetInModel(fingerOffset(FingerBase, hand));
    const glm::vec3 e = mBase + k * (glm::vec3{1.f} - ts) * vec3Of(handrig::data::baseScaleOrigin);
    view::ViewEntity scratch;
    view::ViewEntity& v = ve ? *ve : scratch;
    place(v, model, pos + hands::redirect({e.x, mirrored ? e.y : -e.y, e.z}, handRot), {-handRot.x, handRot.y, handRot.z}, 0,
        mirrored);
    float m[16];
    render::entityMatrix(v.ent, mirrored, ENTSCALE_DEFAULT, glm::vec3{0.f}, m);
    return toMat4(m);
}

// The hand at (`pos`, `handRot`) moved and turned, the least, so that its grip channel (grasp::gripChannel: where a
// handle lies in the curled fingers) is on the line through `on` along `axis`: turned about the channel's middle to
// lie along it (either way), then moved onto it (to its nearest point). A blade, a cupped hand: they are then in the
// fingers' closing reach, not beside them. `legacy`: the hand before "Hands remodelled"'s channel (migrating settings).
void alignChannel(int hand, bool mirrored, const glm::vec3& on, const glm::vec3& axis, glm::vec3& pos, glm::vec3& handRot,
    bool legacy = false)
{
    glm::vec3 cp, cd;
    float radius;
    if(!handrig::usable(viewModel(handrig::modelName)))
    {
        return;
    }
    if(legacy)
    {
        grasp::legacyGripChannel(rigHands[hand].pose, cp, cd, radius);
    }
    else if(!grasp::gripChannel(rigHands[hand].pose, cp, cd, radius))
    {
        return;
    }
    const glm::mat4 m = rigPlacement(hand, pos, handRot, mirrored, nullptr);
    const glm::vec3 c{m * glm::vec4{cp, 1.f}};
    glm::vec3 d = glm::normalize(glm::mat3{m} * cd);
    const glm::vec3 a = glm::normalize(axis);
    if(glm::dot(d, a) < 0.f)
    {
        d = -d;
    }
    // The least turn taking d to a (they are within 90 degrees).
    const glm::vec3 x = glm::cross(d, a);
    const glm::mat3 r = glm::mat3_cast(glm::normalize(glm::quat{1.f + glm::dot(d, a), x.x, x.y, x.z}));
    handRot = basisAngles(r * anglesBasis(handRot));
    pos = c + r * (pos - c);
    const glm::vec3 target = on + a * glm::dot(c - on, a);
    pos += target - c;
}

// Whether a finger at `curls` is clear of the world: the lines from its knuckle through its joints to its tip.
[[nodiscard]] bool fingerClear(const RigHand& rh, int finger, const glm::mat4& rigToWorld, const float curls[handrig::jointsPerFinger])
{
    glm::vec3 p[4];
    grasp::fingerPoints(rh.pose, finger, curls, p);
    for(int i = 0; i < 3; i++)
    {
        const glm::vec3 a{rigToWorld * glm::vec4{drawnInRig(rh, p[i]), 1.f}};
        const glm::vec3 b{rigToWorld * glm::vec4{drawnInRig(rh, p[i + 1]), 1.f}};
        if(worldtrace::line(a, b) < 1.f)
        {
            return false;
        }
    }
    return true;
}

// A finger going into the world's geometry curled (its joints alike) as little as keeps it out (six halvings: a
// sixty-fourth of the way to the fist); left as it is if it is in it even closed (its knuckle is: the hand is).
void bendOutOfWalls(const RigHand& rh, int finger, const glm::mat4& rigToWorld, float target[handrig::jointsPerFinger])
{
    QVR_PROFILE("hand walls");
    if(fingerClear(rh, finger, rigToWorld, target))
    {
        return;
    }
    float lo = 0.f, hi = 4.f;
    float trial[handrig::jointsPerFinger];
    const auto bent = [&](float by) {
        for(int j = 0; j < handrig::jointsPerFinger; j++)
        {
            trial[j] = std::fmin(target[j] + by, 4.f);
        }
    };
    bent(hi);
    if(!fingerClear(rh, finger, rigToWorld, trial))
    {
        return;
    }
    for(int h = 0; h < 6; h++)
    {
        const float mid = 0.5f * (lo + hi);
        bent(mid);
        (fingerClear(rh, finger, rigToWorld, trial) ? hi : lo) = mid;
    }
    bent(hi);
    std::copy(trial, trial + handrig::jointsPerFinger, target);
    if(vr_debug_grasp.value >= 3.f)
    {
        Con_Printf("finger %d bent %.2f out of the world\n", finger, hi);
    }
}

// Hand units from the palm's middle within which the other hand's weapon makes the fingers react (a finger's reach).
constexpr float brushReach = 24.f;

// vr_hand_collide: the free `hand` (if `free`) drawn out of the weapon in the other hand: its palm's middle and its
// knuckles tested against the weapon's surface (and the fingertips of the fingers that can't bend out of it); the
// deepest in it sets the push out (along the surface's normal), all of it up to vr_hand_collide cm, then less and less,
// none at twice as deep (it lets go). Eased over 0.08 s. The fingers react to the weapon's shape (setupRigHand, the
// weapon noted here while the hand is near it): they rest on it, or bend out of it, rather than the whole hand moving.
void pushOut(const hands::State& s, int hand, bool free, glm::vec3& pos, const glm::vec3& handRot, bool mirrored)
{
    QVR_PROFILE("hand collide");
    RigHand& rh = rigHands[hand];
    const double now = cl.time;
    const float dt = rh.pushedTime >= 0.0 ? static_cast<float>(CLAMP(0.0, now - rh.pushedTime, 0.1)) : 0.f;
    rh.pushedTime = now;
    rh.brushEnt = nullptr;
    glm::vec3 target{0.f};
    const float most = std::fmax(vr_hand_collide.value, 0.f) * 0.01f * units::metresToUnits();
    const view::ViewEntity& other = entities.weapon[1 - hand];
    const int slot = weapons::slotForModel(other.ent.model);
    if(free && most > 0.f && other.visible && slot >= 0 && slot != weapons::fistSlot() && handrig::usable(viewModel(handrig::modelName)))
    {
        if(const grasp::Shape* shape = grasp::shapeOf(other.ent, other.ent.frame))
        {
            const glm::mat4 toWorld = grasp::shapeToWorld(other.ent, other.mirrored);
            const glm::mat4 rig = rigPlacement(hand, pos, handRot, mirrored, nullptr);
            const float unit = glm::length(glm::vec3{rig[0]});
            // The palm's middle, the knuckles, and the tips of the fingers stuck in it last frame (the hand as posed).
            glm::vec3 points[1 + 2 * handrig::FingerCount];
            int count = 0;
            const glm::vec3 palm{rig * glm::vec4{grasp::palmCentre(), 1.f}};
            points[count++] = palm;
            for(int f = 0; f < handrig::FingerCount; f++)
            {
                glm::vec3 p[4];
                grasp::fingerPoints(rh.pose, f, rh.pose.curl[f], p);
                points[count++] = glm::vec3{rig * glm::vec4{p[0], 1.f}};
                if(rh.brushStuck[f] || !vr_hand_collide_fingers.value)
                {
                    points[count++] = glm::vec3{rig * glm::vec4{p[3], 1.f}};
                }
            }
            float deepest = 0.f;
            glm::vec3 out{0.f};
            for(int i = 0; i < count; i++)
            {
                glm::vec3 move;
                if(grasp::inside(*shape, toWorld, points[i], 2.f * most, move) && glm::length(move) > deepest)
                {
                    deepest = glm::length(move);
                    out = move;
                }
            }
            float give = 1.f;
            if(deepest > 0.f)
            {
                give = deepest <= most ? 1.f : std::fmax(0.f, 2.f - deepest / most); // past it: less, then none
                target = out * give;
            }
            // Near it (within a hand's length of the palm's middle) and not passing through it: the fingers react.
            glm::vec3 at;
            bool in = false;
            if(vr_hand_collide_fingers.value && give > 0.f && grasp::surfaceDistance(*shape, toWorld, palm, brushReach * unit, at, in) >= 0.f)
            {
                rh.brushEnt = &other.ent;
                rh.brushMirrored = other.mirrored;
            }
        }
    }
    (void)s;
    rh.pushed += (target - rh.pushed) * std::fmin(1.f, dt / 0.08f);
    pos += rh.pushed;
}

// `motion`: the held weapon's firing animation, moving the drawn hand after its grasp (solved without it).
// Where the six models are drawn: each at the hand plus its offset (fingerOffset, at offsetScale), scaled by the fist
// slot's Scale about its own scale origin (weapons::modelTransform). The rig is hand_base.mdl's space: the entity goes
// where the palm's origin is drawn, each finger moves from its bind place (the defaults) by what its current offset
// changes: `pose`'s shifts.
void rigShifts(qmodel_t* model, int hand, handrig::Pose& pose)
{
    const weapons::ModelTransform t = weapons::modelTransform(model);
    const float k = t.active ? t.k : 1.f;
    const glm::vec3 ts = t.active ? t.scale : glm::vec3{1.f};
    const glm::vec3 baseOrigin = vec3Of(handrig::data::baseScaleOrigin);
    const glm::vec3 mBase = offsetInModel(fingerOffset(FingerBase, hand));
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        const glm::vec3 m = offsetInModel(fingerOffset(rigFinger[f], hand));
        const glm::vec3 origin = vec3Of(handrig::data::fingerScaleOrigin[f]);
        pose.shift[f] = ((m - mBase) / k + (glm::vec3{1.f} - ts) * (origin - baseOrigin)) / ts -
                        vec3Of(handrig::data::fingerBindShift[f]);
    }
}

// The empty hand at the controller's pose (setupHand's: turned by the fist's angle offsets), open (`fist` false) or closed
// as a full press of the grip, trigger and thumb draws it (every finger curled), as the grasp's spheres, in the hand's
// frame: its place and the axes of its angles as the move sends them (held::setFist). False without the jointed hand.
bool emptyHandSpheres(const hands::State& s, int hand, bool fist, std::vector<glm::vec4>& out)
{
    out.clear();
    qmodel_t* const model = viewModel(handrig::modelName);
    if(!handrig::usable(model))
    {
        return false;
    }
    const bool mirrored = hand == HAND_OFF;
    handrig::Pose pose;
    rigShifts(model, hand, pose);
    HandInput full;
    full.gripValue = full.triggerValue = 1.f;
    full.grip = full.trigger = full.thumbTouch = full.triggerTouch = true;
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        const float curl = fist ? grasp::pathCurl(CLAMP(0.f, targetCurl(full, rigFinger[f], false) * 5.f, 5.f)) : 0.f;
        for(float& j : pose.curl[f])
        {
            j = curl;
        }
    }
    const glm::vec3 handRot = basisAngles(anglesBasis(s.rot[hand]) * anglesBasis(weaponAngleOffsets(weapons::fistSlot(), mirrored)));
    const glm::mat4 rig = rigPlacement(hand, s.pos[hand], handRot, mirrored, nullptr);
    const float unit = glm::length(glm::vec3{rig[0]});
    const glm::mat3 toHand = glm::transpose(held::axesFromAngles(&s.rot[hand][0], true));
    static std::vector<glm::vec4> spheres;
    grasp::posedSpheres(pose, spheres);
    for(const glm::vec4& sp : spheres)
    {
        const glm::vec3 w{rig * glm::vec4{glm::vec3{sp}, 1.f}};
        out.push_back(glm::vec4{toHand * (w - s.pos[hand]), sp.w * unit});
    }
    return true;
}

// Grab reach from the fist (held::setFist): the closed empty hand, every frame; none without the jointed hand model (the
// server's old test).
void updateFist(const hands::State& s, int hand)
{
    static std::vector<glm::vec4> local;
    emptyHandSpheres(s, hand, true, local);
    held::setFist(hand, local);
}

// Held props' grips (grip::setHandFrame): where the empty hand's palm and grip channel are in its frame (the move's place
// and angles), every frame; not measured without the jointed hand (the default hand's are used).
void gripFrame_f();

void updateGripFrame(const hands::State& s, int hand)
{
    if(static bool registered = false; !registered) // a test command (no init hook here)
    {
        registered = true;
        Cmd_AddCommand("vr_grip_frame", gripFrame_f);
    }
    grip::HandFrame f;
    qmodel_t* const model = viewModel(handrig::modelName);
    glm::vec3 cp, cd;
    float radius = 0.f;
    handrig::Pose pose;
    if(s.valid && handrig::usable(model))
    {
        rigShifts(model, hand, pose);
        f.measured = grasp::gripChannel(pose, cp, cd, radius);
    }
    if(f.measured)
    {
        const bool mirrored = hand == HAND_OFF;
        const glm::vec3 handRot =
            basisAngles(anglesBasis(s.rot[hand]) * anglesBasis(weaponAngleOffsets(weapons::fistSlot(), mirrored)));
        const glm::mat4 rig = rigPlacement(hand, s.pos[hand], handRot, mirrored, nullptr);
        const glm::mat3 toHand = glm::transpose(held::axesFromAngles(&s.rot[hand][0], true));
        const auto point = [&](const glm::vec3& p) { return toHand * (glm::vec3{rig * glm::vec4{p, 1.f}} - s.pos[hand]); };
        const auto dir = [&](const glm::vec3& d) { return glm::normalize(toHand * glm::vec3{rig * glm::vec4{d, 0.f}}); };
        f.palm = point(grasp::palmCentre());
        f.palmNormal = dir({0.f, 1.f, 0.f}); // the rig's palm faces +y (grasp::solve)
        f.channelPoint = point(cp);
        f.channelDir = dir(cd);
        f.channelRadius = radius * glm::length(glm::vec3{rig[0]});
        // The palm's middle is inside the hand: out along its normal to its skin, where the open hand's spheres (the
        // grasp's) the normal passes through end, so that what rests on it rests on the skin, not in the palm.
        static std::vector<glm::vec4> open;
        if(emptyHandSpheres(s, hand, false, open))
        {
            float skin = 0.f;
            for(const glm::vec4& sp : open)
            {
                const glm::vec3 d = glm::vec3{sp} - f.palm;
                const float along = glm::dot(d, f.palmNormal);
                const float across2 = glm::dot(d, d) - along * along;
                if(across2 < sp.w * sp.w)
                {
                    skin = std::max(skin, along + std::sqrt(sp.w * sp.w - across2));
                }
            }
            f.palm += f.palmNormal * skin;
        }
    }
    grip::setHandFrame(hand, f);
}

// vr_grip_frame: the hands' grip frames (grip::HandFrame), for the default hand's (vr_grip.cpp defaultFrame).
void gripFrame_f()
{
    for(int hand = 1; hand >= 0; hand--)
    {
        const grip::HandFrame f = grip::handFrame(hand, hand == HAND_OFF, true);
        Con_Printf("grip frame %s%s: palm %.2f %.2f %.2f normal %.2f %.2f %.2f channel %.2f %.2f %.2f dir %.2f %.2f %.2f radius %.2f\n",
            hand == HAND_MAIN ? "main" : "off", f.measured ? "" : " (not measured: the default)", f.palm.x, f.palm.y, f.palm.z,
            f.palmNormal.x, f.palmNormal.y, f.palmNormal.z, f.channelPoint.x, f.channelPoint.y, f.channelPoint.z, f.channelDir.x,
            f.channelDir.y, f.channelDir.z, f.channelRadius);
    }
}

bool setupRigHand(int hand, const glm::vec3& pos, const glm::vec3& handRot, bool mirrored, bool hide, const Held& held,
    const glm::mat4& motion)
{
    QVR_PROFILE("rig hand");
    RigHand& rh = rigHands[hand];
    rh.drawn = false;
    if(!vr_hand_rig.value)
    {
        return false;
    }
    qmodel_t* const model = viewModel(handrig::modelName);
    if(!handrig::usable(model))
    {
        return false;
    }

    const weapons::ModelTransform t = weapons::modelTransform(model);
    const float k = t.active ? t.k : 1.f;
    const glm::vec3 ts = t.active ? t.scale : glm::vec3{1.f};
    rigShifts(model, hand, rh.pose);
    rh.pose.metacarpal = glm::quat{1.f, 0.f, 0.f, 0.f};

    view::ViewEntity& ve = entities.hand[hand][FingerBase];
    const glm::mat4 rigToWorld = rigPlacement(hand, pos, handRot, mirrored, &ve);

    // The fingers: the controller's curls (as the six models'), or wrapping what the hand holds, or set by hand (the
    // weapon's or the hotspot's Fingers: Manual: no solve).
    if(held.manual)
    {
        rh.grasp.valid = false;
        rh.inRig = held.ent ? glm::inverse(rigToWorld) * grasp::shapeToWorld(*held.ent, held.mirrored) : glm::mat4{1.f};
    }
    else
    {
        updateGrasp(hand, held, rigToWorld, k * ts.x);
    }
    rh.held = held;
    rh.rigToWorld = rigToWorld;

    // A free hand brushing the other hand's weapon (pushOut noted it): its fingers closed onto it from open (the grasp's
    // solve, the palm where it is), to rest on it or bend out of it (below).
    const bool brushing = rh.brushEnt && !held.ent && !held.manual;
    if(brushing)
    {
        QVR_PROFILE("hand brush");
        if(const grasp::Shape* shape = grasp::shapeOf(*rh.brushEnt, rh.brushEnt->frame))
        {
            // Solved again only once the hand has moved on it by Refit Threshold (vr_hand_fit_resolve), as a held thing's
            // grasp: steady, and cheap while the hand rests against it.
            const glm::mat4 inRig = glm::inverse(rigToWorld) * grasp::shapeToWorld(*rh.brushEnt, rh.brushMirrored);
            Grasp& last = rh.brushSolve;
            if(!rh.brushValid || !sameGrasp(last, rh.brushEnt->model, rh.brushEnt->frame, inRig, rh.pose, k * ts.x))
            {
                grasp::Settings settings;
                grasp::solve(rh.pose, *shape, inRig, settings, rh.brushValid ? &rh.brush : nullptr, last.solution);
                rh.brush = last.solution;
                rh.brushValid = true;
                last.valid = true;
                last.model = rh.brushEnt->model;
                last.frame = rh.brushEnt->frame;
                last.inRig = inRig;
                for(int f = 0; f < handrig::FingerCount; f++)
                {
                    last.shift[f] = rh.pose.shift[f];
                }
                brushSolves[hand]++;
            }
            const grasp::Solution& solution = rh.brush;
            if(vr_debug_grasp.value)
            {
                Con_Printf("brush (%.2f s): %s hand on %s: solve %d, %.1f us, %d triangles; fingers", realtime,
                    hand == HAND_MAIN ? "main" : "off", rh.brushEnt->model ? rh.brushEnt->model->name : "-", brushSolves[hand],
                    solution.seconds * 1e6, solution.triangles);
                for(const grasp::FingerStop& st : solution.finger)
                {
                    Con_Printf(" %s%.1f/%.1f/%.1f", st.startsInside ? "in " : st.fromClosed ? "out " : st.met ? "on " : "- ",
                        st.stop[0], st.stop[1], st.stop[2]);
                }
                Con_Printf("\n");
            }
        }
        else
        {
            rh.brushValid = false;
        }
    }
    else
    {
        rh.brushValid = false;
    }
    for(bool& stuck : rh.brushStuck)
    {
        stuck = false;
    }
    const double now = cl.time;
    const float dt = rh.jointsTime >= 0.0 ? static_cast<float>(CLAMP(0.0, now - rh.jointsTime, 0.1)) : 1.f;
    rh.jointsTime = now;

    // The palm flush on what it holds: the drawn hand eased there (and back) over about a tenth of a second.
    // The palm turned to face the surface it holds (round its middle) and moved flush on it: the rig drawn as
    //   rigToWorld * T(centre + move) * turn * T(-centre),
    // as an entity: its angles R' = R * turn (mirrored as the hand is), its origin to match.
    // Every change of the grasp (a new solve, the controller's curls) is eased in over vr_hand_fit_blend seconds
    // (95% of the way: an exponential approach, a third of it the time constant): no jumps between poses.
    const float blend = std::fmax(vr_hand_fit_blend.value, 0.f);
    const float follow = blend > 0.f ? 1.f - std::exp(-dt * 3.f / blend) : 1.f;
    const glm::vec3 palmTarget = rh.grasp.valid ? rh.grasp.solution.palm : glm::vec3{0.f};
    const glm::quat turnTarget = rh.grasp.valid ? rh.grasp.solution.palmTurn : glm::quat{1.f, 0.f, 0.f, 0.f};
    rh.palm += (palmTarget - rh.palm) * follow;
    rh.turn = glm::normalize(glm::slerp(rh.turn, turnTarget, follow));
    if(glm::length(rh.palm) > 1e-4f || glm::angle(rh.turn) > 1e-4f)
    {
        const glm::mat3 q = glm::mat3_cast(rh.turn);
        static const glm::vec3 c = grasp::palmCentre();
        const glm::mat3 l = glm::mat3{rigToWorld} / ts.x; // R * [mirror] * k
        const glm::vec3 offset = t.active ? t.offset : glm::vec3{0.f};
        const glm::vec3 o{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]};
        const glm::vec3 moved = o + l * offset + l * (ts.x * (c + rh.palm - q * c)) - l * (q * offset);
        glm::vec3 f, r, u;
        hands::angleVectors(handRot, f, r, u);
        const glm::mat3 mirror{1.f, 0.f, 0.f, 0.f, mirrored ? -1.f : 1.f, 0.f, 0.f, 0.f, 1.f};
        const glm::mat3 turned = glm::mat3{f, -r, u} * mirror * q * mirror;
        const glm::vec3 a = hands::anglesFromVectors(glm::normalize(turned[0]), glm::normalize(turned[2]));
        for(int i = 0; i < 3; i++)
        {
            ve.ent.origin[i] = moved[i];
        }
        ve.ent.angles[0] = -a.x;
        ve.ent.angles[1] = a.y;
        ve.ent.angles[2] = a.z;
    }
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        const float curl = fingerFrames[hand][rigFinger[f]];
        const float bias = fingerBias[hand][rigFinger[f]];
        float target[handrig::jointsPerFinger];
        // Set by hand: the finger stops at its curl, as a solved finger stops on what it holds.
        grasp::FingerStop manualStop;
        if(held.manual)
        {
            manualStop.met = true;
            for(float& j : manualStop.stop)
            {
                j = 4.f * held.manualCurl[f];
            }
        }
        const grasp::FingerStop* stop = held.manual ? &manualStop : rh.grasp.valid ? &rh.grasp.solution.finger[f] : nullptr;
        if(stop)
        {
            grasp::curls(*stop, curl, engagement(curl), target);
            if(f == handrig::Index && held.trigger)
            {
                // The trigger finger pulls as the trigger is pulled, past where it met the weapon (the trigger
                // gives): from its stop (or the controller's curl, if less) towards a trigger pull's hook.
                const float pull = CLAMP(0.f, curl / 5.f, 1.f);
                for(int j = 0; j < handrig::jointsPerFinger; j++)
                {
                    target[j] = std::fmax(target[j], std::fmin(stop->stop[j], curl) + pull * triggerPull[j]);
                }
            }
            for(float& t : target)
            {
                t = CLAMP(0.f, t + 4.f * bias, 4.f); // the weapon's finger tweak, on top of the wrap
            }
        }
        else
        {
            for(float& t : target)
            {
                t = rigCurl(hand, rigFinger[f]);
            }
            // Brushing the other hand's weapon: a finger closing onto it stops on its surface (it rests there, as far as
            // the controller closes it); one in it open bends out of it, closing to where it is clear; one in it at every
            // curl is left, and pushes the hand out instead (pushOut). Eased in and out as any change of the fingers.
            if(brushing && rh.brushValid)
            {
                const grasp::FingerStop& st = rh.brush.finger[f];
                rh.brushStuck[f] = st.startsInside;
                if(st.startsInside && rh.brushHeld[f])
                {
                    // In it at every curl (the hand gone deeper): kept as it was last (not back to the controller's
                    // curl, into the weapon), while the hand is pushed out by its tip.
                    for(int j = 0; j < handrig::jointsPerFinger; j++)
                    {
                        target[j] = rh.brushHold[f][j];
                    }
                }
                else if(!st.startsInside && (st.met || st.fromClosed))
                {
                    for(int j = 0; j < handrig::jointsPerFinger; j++)
                    {
                        target[j] = st.fromClosed ? st.stop[j] : std::fmin(grasp::pathCurl(target[j]), st.stop[j]);
                    }
                }
                if(!st.startsInside)
                {
                    std::copy(target, target + handrig::jointsPerFinger, rh.brushHold[f]);
                    rh.brushHeld[f] = true;
                }
            }
            else
            {
                rh.brushHeld[f] = false;
            }
        }
        // Into a wall (vr_hand_walls): bent (curled, all its joints alike) as little as keeps it out: the lines from
        // the knuckle through its joints to its tip clear of the world's geometry.
        if(vr_hand_walls.value)
        {
            bendOutOfWalls(rh, f, rigToWorld, target);
        }
        for(int j = 0; j < handrig::jointsPerFinger; j++)
        {
            float& c = rh.joints[f][j];
            c += (target[j] - c) * follow;
            rh.pose.curl[f][j] = c;
        }
    }
    // The thumb turns across the palm (opposition) as the grasp found, as far as it closes.
    const float w = CLAMP(0.f, fingerFrames[hand][FingerThumb] / 3.f, 1.f);
    // Set by hand: across the palm by its share of 45 degrees (the solver's widest opposition), about the same axis.
    const glm::quat manualThumb = glm::angleAxis(glm::radians(45.f * held.manualThumbAcross), glm::vec3{-1.f, 0.f, 0.f});
    const bool brushThumb = brushing && rh.brushValid && !rh.brush.finger[handrig::Thumb].startsInside &&
                            (rh.brush.finger[handrig::Thumb].met || rh.brush.finger[handrig::Thumb].fromClosed);
    const glm::quat thumbTarget = held.manual   ? glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, manualThumb, w)
                                  : rh.grasp.valid ? glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, rh.grasp.solution.thumbTurn, w)
                                  : brushThumb     ? rh.brush.thumbTurn
                                                   : glm::quat{1.f, 0.f, 0.f, 0.f};
    rh.thumb = glm::normalize(glm::slerp(rh.thumb, thumbTarget, follow));
    rh.pose.metacarpal = rh.thumb;
    handrig::pose(rh.pose, rh.posed);
    handrig::skin(rh.posed, rh.skin.data());

    // The weapon's firing animation moves the hand (and so the arm) with it.
    if(motion != glm::mat4{1.f})
    {
        glm::vec3 o{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]};
        glm::vec3 a{-ve.ent.angles[0], ve.ent.angles[1], ve.ent.angles[2]};
        movePose(motion, o, a);
        for(int i = 0; i < 3; i++)
        {
            ve.ent.origin[i] = o[i];
        }
        ve.ent.angles[0] = -a.x;
        ve.ent.angles[1] = a.y;
        ve.ent.angles[2] = a.z;
        rh.rigToWorld = motion * rh.rigToWorld;
    }
    if(vr_debug_hand_bones.value)
    {
        float m[16];
        render::entityMatrix(ve.ent, mirrored, ENTSCALE_DEFAULT, glm::vec3{0.f}, m);
        rh.drawnToWorld = toMat4(m);
    }
    static FILE* trace = nullptr;
    if(!vr_debug_grasp_trace.value && trace)
    {
        fclose(trace);
        trace = nullptr;
    }
    if(static_cast<int>(vr_debug_grasp_trace.value) & (hand == HAND_MAIN ? 1 : 2))
    {
        // One line a frame: the time, the solves so far, whether a grasp holds, the 15 joints, the thumb's
        // metacarpal turn (degrees), the palm's move (rig units) and turn (degrees), and where the hand is.
        std::string line = va("gt %d %.4f %d %d", hand, cl.time, graspSolves[hand], rh.grasp.valid ? 1 : 0);
        for(int f = 0; f < handrig::FingerCount; f++)
        {
            for(int j = 0; j < handrig::jointsPerFinger; j++)
            {
                line += va(" %.3f", rh.pose.curl[f][j]);
            }
        }
        line += va(" %.2f %.3f %.3f %.3f %.2f %.2f %.2f %.2f", glm::degrees(glm::angle(rh.pose.metacarpal)), rh.palm.x,
            rh.palm.y, rh.palm.z, glm::degrees(glm::angle(rh.turn)), ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]);
        // What it holds, relative to the grasp's: turned (degrees; the largest of its axes') and moved (hand units).
        float turned = 0.f, moved = 0.f;
        if(rh.grasp.valid)
        {
            moved = glm::distance(glm::vec3{rh.inRig[3]}, glm::vec3{rh.grasp.inRig[3]});
            for(int c = 0; c < 3; c++)
            {
                const glm::vec3 a = glm::normalize(glm::vec3{rh.inRig[c]}), b = glm::normalize(glm::vec3{rh.grasp.inRig[c]});
                turned = std::fmax(turned, glm::degrees(std::acos(CLAMP(-1.f, glm::dot(a, b), 1.f))));
            }
        }
        // The firing animation's motion at the hand: moved (world units), turned (degrees).
        const glm::vec3 at{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]};
        line += va(" %.3f %.4f %d %.3f %.2f %d %.3f\n", turned, moved, twohand::helping(hand) ? 1 : 0,
            glm::distance(glm::vec3{motion * glm::vec4{at, 1.f}}, at), glm::degrees(glm::angle(glm::quat_cast(glm::mat3{motion}))),
            entities.weapon[hand].ent.frame, glm::length(rh.pushed));
        if(!trace)
        {
            trace = fopen(va("%s/grasp_trace.txt", com_gamedir), "w");
        }
        if(trace)
        {
            fputs(line.c_str(), trace);
            fflush(trace);
        }
    }
    ve.zeroBlend = 0.f;
    ve.ent.skinnum = wounds::replacesSkins() ? 0 : damageLevel(); // wounds painted instead (vr_wounds.cpp)
    ve.visible = !hide;
    for(int finger = FingerBase + 1; finger < FingerCount; finger++)
    {
        entities.hand[hand][finger].visible = false;
    }
    rh.drawn = ve.visible;
    return true;
}

// The middle of the palm of the jointed hand drawn at (`pos`, `handRot`), before the palm's fit; `pos` if the jointed
// hand isn't there.
[[nodiscard]] glm::vec3 palmAt(int hand, const glm::vec3& pos, const glm::vec3& handRot, bool mirrored)
{
    if(!handrig::usable(viewModel(handrig::modelName)))
    {
        return pos;
    }
    return glm::vec3{rigPlacement(hand, pos, handRot, mirrored, nullptr) * glm::vec4{grasp::palmCentre(), 1.f}};
}

// Round 21, third pass: a drawn hand (at `pos`, `handRot`) moved by `p` and turned by `a` (pitch up, yaw left, roll;
// degrees) in the frame of the angles `frameRot` (x forward, y left, z up), turned about its palm's middle. The values
// are as for the main hand's frame; `mirror`: as the off hand's (y, yaw and roll the other way).
void moveDrawnHand(int hand, bool mirrored, const glm::vec3& frameRot, glm::vec3 p, glm::vec3 a, bool mirror, glm::vec3& pos,
    glm::vec3& handRot)
{
    if(p == glm::vec3{0.f} && a == glm::vec3{0.f})
    {
        return;
    }
    if(mirror)
    {
        p.y = -p.y;
        a.y = -a.y;
        a.z = -a.z;
    }
    const glm::mat3 frame = anglesBasis(frameRot);
    const glm::vec3 pivot = palmAt(hand, pos, handRot, mirrored);
    const glm::mat3 turn = frame * anglesBasis({-a.x, a.y, a.z}) * glm::transpose(frame);
    handRot = basisAngles(turn * anglesBasis(handRot));
    pos = pivot + turn * (pos - pivot) + frame * p;
}

// Round 21, third pass: where a weapon is in the rig of the hand holding it (`hand`: its own), or of the hand holding it
// by the hotspot `spot` (the other), as the settings put them, worked out with the holding hand at the origin,
// unturned (Held::canonical). The same steps as the drawn hands', at that pose.
[[nodiscard]] glm::mat4 canonicalOwnGrip(int hand, int slot, qmodel_t* model, bool offset = true);
[[nodiscard]] glm::mat4 canonicalHotspotGrip(int hand, int otherSlot, qmodel_t* model, const weapons::Hotspot& spot,
    bool offset = true);

// The turn of a helping hand on the weapon in the other hand (`other`), from a hotspot's angles (as for the off hand
// helping): the weapon's fixed-hand angles for a grip; a cup, the holding hand's own turn. Carried rigidly by the
// weapon (attachedTurn).
[[nodiscard]] glm::vec3 helpingTurn(const glm::vec3& holderRot, int holder, int hand, int otherSlot, bool cup,
    const glm::vec3& spotAngles)
{
    const int other = 1 - hand;
    const bool mirrored = hand == HAND_OFF;
    glm::vec3 offsets = cup ? glm::vec3{0.f}
                            : weapons::vec(otherSlot, Key::TwoHFixedHandPitch, Key::TwoHFixedHandYaw, Key::TwoHFixedHandRoll);
    offsets += spotAngles;
    if(!mirrored)
    {
        offsets.y = -offsets.y;
        offsets.z = -offsets.z;
    }
    if(cup)
    {
        offsets += weaponAngleOffsets(weapons::fistSlot(), mirrored); // the hand's own, as the holding hand's
    }
    return attachedTurn(holderRot, otherSlot, other == HAND_OFF, offsets, holder);
}

// The hotspot angles (as for the off hand helping) that give a cup's hand the turn `turn` on the weapon in the other
// hand: helpingTurn undone.
[[nodiscard]] glm::vec3 cupAnglesFor(const hands::State& s, int hand, int otherSlot, const glm::vec3& turn)
{
    const int other = 1 - hand;
    const bool mirrored = hand == HAND_OFF;
    const glm::mat3 w = anglesBasis(heldWeaponTurn(s.rot[other], other, otherSlot, other == HAND_OFF));
    const glm::mat3 w0 = anglesBasis(weaponTurn(glm::vec3{0.f}, otherSlot, other == HAND_OFF));
    glm::vec3 a = basisAngles(w0 * glm::transpose(w) * anglesBasis(turn)) - weaponAngleOffsets(weapons::fistSlot(), mirrored);
    for(int k = 0; k < 3; k++)
    {
        a[k] = std::remainder(a[k], 360.f);
    }
    if(!mirrored)
    {
        a.y = -a.y;
        a.z = -a.z;
    }
    return a;
}

glm::mat4 canonicalOwnGrip(int hand, int slot, qmodel_t* model, bool offset)
{
    const bool mirrored = hand == HAND_OFF;
    const glm::vec3 zero{0.f};
    view::ViewEntity weapon;
    const glm::vec3 wt = weaponTurn(zero, slot, mirrored);
    place(weapon, model, zero, {-wt.x, wt.y, wt.z}, 0, mirrored);
    glm::vec3 pos{0.f};
    glm::vec3 handRot = attachedTurn(zero, slot, mirrored, weaponAngleOffsets(weapons::fistSlot(), mirrored));
    if(offset)
    {
        moveDrawnHand(hand, mirrored, attachedTurn(zero, slot, mirrored, zero), weapons::vec(slot, Key::HandOnlyX, Key::HandOnlyY, Key::HandOnlyZ),
            weapons::vec(slot, Key::HandOnlyPitch, Key::HandOnlyYaw, Key::HandOnlyRoll), mirrored, pos, handRot);
    }
    return glm::inverse(rigPlacement(hand, pos, handRot, mirrored, nullptr)) * grasp::shapeToWorld(weapon.ent, mirrored);
}

glm::mat4 canonicalHotspotGrip(int hand, int otherSlot, qmodel_t* model, const weapons::Hotspot& spot, bool offset)
{
    const int other = 1 - hand;
    const bool mirrored = hand == HAND_OFF, otherMirrored = other == HAND_OFF;
    const glm::vec3 zero{0.f};
    view::ViewEntity weapon;
    const glm::vec3 wt = weaponTurn(zero, otherSlot, otherMirrored);
    place(weapon, model, zero, {-wt.x, wt.y, wt.z}, 0, otherMirrored);
    const bool cup = spot.type == weapons::HotspotType::Cup;
    glm::vec3 handRot = helpingTurn(zero, -1, hand, otherSlot, cup, spot.angles);
    const glm::vec3 point{hotspotFrame(weapon.ent, otherMirrored) * glm::vec4{spot.pos, 1.f}};
    glm::vec3 pos = cup ? point - palmAt(hand, zero, handRot, mirrored) : point;
    if(offset)
    {
        moveDrawnHand(hand, mirrored, attachedTurn(zero, otherSlot, otherMirrored, zero), spot.visualPos, spot.visualAngles,
            otherMirrored, pos, handRot);
    }
    return glm::inverse(rigPlacement(hand, pos, handRot, mirrored, nullptr)) * grasp::shapeToWorld(weapon.ent, otherMirrored);
}

// Round 21, third pass: a config's cup hotspots made before (weapons::cupMigrationPending) moved to where their hand was
// drawn: round 21's second pass put the helping hand's grip channel round the holding hand's, at the hotspot's point
// along it (often far from the point); a cup is now the palm's place and the hand's turn there. Done once, with the
// weapon in `other` and both jointed hands drawn, for every cup its slot (or the one it inherits them from) owns.
void migrateCups(const hands::State& s, int hand)
{
    const int other = 1 - hand;
    const bool mirrored = hand == HAND_OFF;
    const view::ViewEntity& weapon = entities.weapon[other];
    const int otherSlot = weapons::slotForModel(weapon.ent.model);
    if(otherSlot < 0 || otherSlot == weapons::fistSlot() || !rigHands[other].drawn || !rigHands[hand].drawn ||
        !handrig::usable(viewModel(handrig::modelName)))
    {
        return;
    }
    const glm::mat4 hsFrame = hotspotFrame(weapon.ent, other == HAND_OFF);
    const glm::mat4 toHotspot = glm::inverse(hsFrame);
    int owners[weapons::maxHotspots];
    for(int i = 0; i < weapons::maxHotspots; i++)
    {
        owners[i] = weapons::ownerSlot(otherSlot, weapons::hotspotKey(i, 0));
        const weapons::Hotspot h = weapons::hotspot(otherSlot, i);
        if(!weapons::cupMigrationPending(owners[i]) || h.type != weapons::HotspotType::Cup)
        {
            continue;
        }
        // Where round 21's second pass drew it (at full grip).
        glm::vec3 at{hsFrame * glm::vec4{h.pos, 1.f}};
        glm::vec3 turn = helpingTurn(s.rot[other], other, hand, otherSlot, true, h.angles);
        // As the hand of round 21's second pass drew it: its channels (Hands remodelled changed the fingers).
        glm::vec3 cp, cd;
        float radius;
        grasp::legacyGripChannel(rigHands[other].pose, cp, cd, radius);
        {
            const glm::mat4& om = rigHands[other].rigToWorld;
            alignChannel(hand, mirrored, glm::vec3{om * glm::vec4{cp, 1.f}}, glm::mat3{om} * cd, at, turn, true);
        }
        weapons::Hotspot moved = h;
        moved.pos = glm::vec3{toHotspot * glm::vec4{palmAt(hand, at, turn, mirrored), 1.f}};
        moved.angles = cupAnglesFor(s, hand, otherSlot, turn);
        weapons::setHotspot(owners[i], i, moved);
        Con_Printf("%s: cup hotspot %d moved to where its hand was drawn: (%.2f %.2f %.2f) turned (%.1f %.1f %.1f), was "
                   "(%.2f %.2f %.2f) (%.1f %.1f %.1f)\n",
            weapons::cvar(owners[i], Key::ID)->string, i + 1, moved.pos.x, moved.pos.y, moved.pos.z, moved.angles.x,
            moved.angles.y, moved.angles.z, h.pos.x, h.pos.y, h.pos.z, h.angles.x, h.angles.y, h.angles.z);
    }
    for(const int owner : owners)
    {
        weapons::cupMigrationDone(owner);
    }
}

void drawHand(int hand, glm::vec3 pos, glm::vec3 handRot, bool mirrored, bool hide, const Held& held, const glm::mat4& motion);

void setupHand(const hands::State& s, int hand)
{
    QVR_PROFILE("hand");
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
        rigHands[hand].drawn = false;
        rigHands[hand].held = Held{};
        return;
    }

    // A prop held in both hands (vr_held.cpp): the hand is drawn on its grip on it, as if its controller were there.
    glm::vec3 controllerPos = s.pos[hand], controllerRot = s.rot[hand];
    held::drawnHand(hand, controllerPos, controllerRot);
    glm::vec3 lightShift{0.f}; // a hand holding a ledge or a rung: drawn on it, facing it (vr_climb.cpp), lit as without the looks' offset
    climb::drawnHand(s, hand, anglesBasis(weaponAngleOffsets(fist, mirrored)), controllerPos, controllerRot, lightShift);
    for(view::ViewEntity& ve : entities.hand[hand])
    {
        ve.lightShift = lightShift;
    }

    // The hand turned from the controller by the fist's angle offsets, rigidly (round 21, second pass: added as Euler
    // angles, the hand slid round what it held as the wrist turned, and its grasp was solved again and again).
    glm::vec3 handRot = basisAngles(anglesBasis(controllerRot) * anglesBasis(weaponAngleOffsets(fist, mirrored)));

    // The hand is where the controller is, holding a weapon or not (round 21: the weapon is placed in the hand, and the
    // fingers wrap it; it was drawn at an anchor vertex of the weapon, which the settings kept within 0.8 of a
    // centimetre of the controller).
    glm::vec3 pos = controllerPos;
    bool hide = false;
    glm::mat4 motion{1.f}; // the weapon's firing animation where the hand holds it (animationMotion)
    if(slot >= 0 && slot != fist)
    {
        hide = weapons::value(slot, Key::HideHand) != 0.f;
        handRot = attachedTurn(s.rot[hand], slot, mirrored, weaponAngleOffsets(fist, mirrored), hand);
        // Round 21, third pass: the Hand Only offset (vr_wofs_hand_only_*): the drawn hand moved and turned on the
        // weapon, in the aim frame, about its palm; the weapon, its muzzle and its aim stay, the fingers wrap it again.
        moveDrawnHand(hand, mirrored, attachedTurn(s.rot[hand], slot, mirrored, glm::vec3{0.f}, hand),
            weapons::vec(slot, Key::HandOnlyX, Key::HandOnlyY, Key::HandOnlyZ),
            weapons::vec(slot, Key::HandOnlyPitch, Key::HandOnlyYaw, Key::HandOnlyRoll), mirrored, pos, handRot);
        motion = animationMotion(weapon, pos);
    }

    // Steadying the other hand's weapon in the "fixed" two-handed display mode: the hand moves
    // onto the weapon's foregrip (blending in and out with the grip), turned like the holding
    // hand plus the weapon's fixed-hand angles (old engine's V_SetupFixedHelpingHandViewEnt).
    const int other = 1 - hand;
    const float gripBlend = s.grip2HValid[other] ? twohand::transition(other) : 0.f;
    const int otherSlot = weapons::slotForModel(entities.weapon[other].ent.model);
    for(int i = 0; i < weapons::maxHotspots && otherSlot >= 0; i++)
    {
        if(weapons::cupMigrationPending(weapons::ownerSlot(otherSlot, weapons::hotspotKey(i, 0))))
        {
            migrateCups(s, hand);
            break;
        }
    }
    // The hotspot it holds the other hand's weapon by (the blade's, for the half-sword grip).
    const WorldHotspot* heldSpot = nullptr;
    if(gripBlend > 0.f && twohand::bladeGrip(other))
    {
        for(const WorldHotspot& w : worldHotspots[other])
        {
            heldSpot = w.type == weapons::HotspotType::Blade ? &w : heldSpot;
        }
    }
    else if(gripBlend > 0.f && chosenGrip[other] >= 0)
    {
        heldSpot = &worldHotspots[other][chosenGrip[other]];
    }
    glm::vec3 bladePos = pos, bladeRot = handRot;
    if(gripBlend > 0.f && twohand::bladeGrip(other) && otherSlot >= 0 &&
        twohand::bladeGripHand(s, hand,
            glm::vec3{entities.weapon[other].ent.origin[0], entities.weapon[other].ent.origin[1], entities.weapon[other].ent.origin[2]},
            s.rot[other] + weaponAngleOffsets(fist, other == HAND_OFF), bladePos, bladeRot))
    {
        // Holding the other hand's sword by its blade (round 18): on the blade where the hand is,
        // turned (the least turn) to close round it; its fingers' channel on the blade (round 21, second pass: the
        // blade lay beside the fingers, the author's "very far away from the blade").
        if(s.muzzleValid[other])
        {
            const glm::vec3 hilt{entities.weapon[other].ent.origin[0], entities.weapon[other].ent.origin[1],
                entities.weapon[other].ent.origin[2]};
            alignChannel(hand, mirrored, hilt, s.muzzle[other] - hilt, bladePos, bladeRot);
        }
        pos = glm::mix(pos, bladePos, gripBlend);
        handRot = bladeRot;
        hide = false;
        motion = partMotion(animationMotion(entities.weapon[other], bladePos), gripBlend, bladePos);
    }
    else if(gripBlend > 0.f)
    {
        pos = glm::mix(pos, s.grip2H[other], gripBlend);
        hide = false;
        motion = partMotion(animationMotion(entities.weapon[other], s.grip2H[other]), gripBlend, s.grip2H[other]);

        if(twohand::helping(hand) && otherSlot >= 0)
        {
            // A grip: the weapon's fixed-hand angles; a cup: turned as the holding hand is. Then the hotspot's own.
            // (The weapon's and the hotspot's are for the off hand helping: mirrored for the main hand.) Carried
            // rigidly by the weapon (attachedTurn), turned onto it as the hand takes the grip.
            const bool cup = heldSpot && heldSpot->type == weapons::HotspotType::Cup;
            const glm::vec3 attached =
                helpingTurn(s.rot[other], other, hand, otherSlot, cup, heldSpot ? heldSpot->angles : glm::vec3{0.f});
            // A cup is where the palm's middle goes (round 21, third pass: it was a point the hand's grip channel was
            // aligned round, often far from where the hand ended up): the hand placed so, as it is turned there.
            if(cup)
            {
                const glm::vec3 at = s.grip2H[other] - palmAt(hand, glm::vec3{0.f}, attached, mirrored);
                pos = glm::mix(s.pos[hand], at, gripBlend);
            }
            const glm::quat from = glm::quat_cast(anglesBasis(handRot)), to = glm::quat_cast(anglesBasis(attached));
            handRot = basisAngles(glm::mat3_cast(glm::slerp(from, to, gripBlend)));
        }
    }
    // Round 21, third pass: the held hotspot's hand offset (vr_wofs_hsN_v*): its drawn pose moved and turned once it
    // holds it, as it takes it (visual only: the grab and the aim are the tracked hand's), in the holding hand's aim
    // frame, about its palm; as for the off hand helping, mirrored for the main hand.
    if(heldSpot && twohand::helping(hand))
    {
        moveDrawnHand(hand, mirrored, attachedTurn(s.rot[other], otherSlot, other == HAND_OFF, glm::vec3{0.f}, other),
            heldSpot->visualPos * gripBlend, heldSpot->visualAngles * gripBlend, other == HAND_OFF, pos, handRot);
    }

    // A free hand pushed into the other hand's weapon is held out of it (drawn only), a few centimetres at most: past
    // that it gives way, and passes through (vr_hand_collide).
    pushOut(s, hand, gripBlend <= 0.f && (slot < 0 || slot == fist) && !held::heldEntity(hand) && !flashlight::holds(hand), pos,
        handRot, mirrored);

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

    // What the fingers wrap (vr_hand_fit): the gun in the hand, the other hand's gun it helps hold (its foregrip, a
    // sword's blade), a carried box or gib, the flashlight.
    Held held;
    if(gripBlend > 0.f && entities.weapon[other].ent.model)
    {
        held = {&entities.weapon[other].ent, other == HAND_OFF, 0, true};
        // Its hotspot's kind, style and overlap.
        if(heldSpot)
        {
            held.cup = heldSpot->type == weapons::HotspotType::Cup;
            held.thumbTop = heldSpot->style == weapons::HotspotStyle::ThumbTop;
            held.overlap = CLAMP(0.f, heldSpot->overlap, 1.f) * weapons::maxOverlapCm;
            setManualFingers(held, heldSpot->def.manual, heldSpot->def.curl, heldSpot->def.thumbAcross);
            // Held fully by a grip or a cup (not while taking it, nor a blade, where the hand slides): its settled place.
            if(gripBlend >= 1.f && weapons::isGripType(heldSpot->type) && twohand::helping(hand) && otherSlot >= 0 &&
                handrig::usable(viewModel(handrig::modelName)))
            {
                held.canonical = true;
                held.canonicalInRig = canonicalHotspotGrip(hand, otherSlot, entities.weapon[other].ent.model, heldSpot->def);
                held.offset = heldSpot->def.visualPos != glm::vec3{0.f} || heldSpot->def.visualAngles != glm::vec3{0.f};
                if(held.offset)
                {
                    held.fitInRig = canonicalHotspotGrip(hand, otherSlot, entities.weapon[other].ent.model, heldSpot->def, false);
                }
            }
        }
    }
    else if(slot >= 0 && slot != fist)
    {
        held = {&weapon.ent, mirrored, 0, true, true};
        held.overlap = CLAMP(0.f, weapons::value(slot, Key::GripOverlap), 1.f) * weapons::maxOverlapCm;
        const float curl[handrig::FingerCount] = {weapons::value(slot, Key::FingerCurlThumb), weapons::value(slot, Key::FingerCurlIndex),
            weapons::value(slot, Key::FingerCurlMiddle), weapons::value(slot, Key::FingerCurlRing), weapons::value(slot, Key::FingerCurlPinky)};
        setManualFingers(held, weapons::value(slot, Key::FingerManual) >= 0.5f, curl, weapons::value(slot, Key::FingerThumbAcross));
        if(!weaponCarried[hand] && handrig::usable(viewModel(handrig::modelName)))
        {
            held.canonical = true;
            held.canonicalInRig = canonicalOwnGrip(hand, slot, weapon.ent.model);
            held.offset = weapons::vec(slot, Key::HandOnlyX, Key::HandOnlyY, Key::HandOnlyZ) != glm::vec3{0.f} ||
                          weapons::vec(slot, Key::HandOnlyPitch, Key::HandOnlyYaw, Key::HandOnlyRoll) != glm::vec3{0.f};
            if(held.offset)
            {
                held.fitInRig = canonicalOwnGrip(hand, slot, weapon.ent.model, false);
            }
        }
    }
    else if(const int ent = held::heldEntity(hand))
    {
        held = {&cl_entities[ent], false, -1};
        // Its fingers and overlap (Held Object Offsets, vr_props.inc; the flashlight's are its own page's).
        if(const qmodel_t* model = cl_entities[ent].model)
        {
            const int slot = props::slotForModel(model);
            using props::Key;
            held.overlap = props::value(slot, Key::Overlap);
            const float curl[handrig::FingerCount]{props::value(slot, Key::FingerCurlThumb), props::value(slot, Key::FingerCurlIndex),
                props::value(slot, Key::FingerCurlMiddle), props::value(slot, Key::FingerCurlRing), props::value(slot, Key::FingerCurlPinky)};
            setManualFingers(held, props::value(slot, Key::FingerManual) >= 0.5f, curl, props::value(slot, Key::FingerThumbAcross));
        }
    }
    else if(flashlight::holds(hand) && entities.flashlight.ent.model)
    {
        // The torch as it is placed this frame (it is set up after the hands).
        static entity_t torch[2];
        torch[hand] = entities.flashlight.ent;
        glm::vec3 origin, angles;
        if(flashlight::heldPlace(s, hand, origin, angles))
        {
            for(int i = 0; i < 3; i++)
            {
                torch[hand].origin[i] = origin[i];
                torch[hand].angles[i] = angles[i];
            }
            held = {&torch[hand], false, 0};
            // Its grip's fingers (vr_flashlight_low_* or _high_*: Fingers, the curls, Thumb Across, Overlap).
            if(flashlight::Fingers f; flashlight::fingers(hand, f))
            {
                held.overlap = f.overlap * weapons::maxOverlapCm;
                setManualFingers(held, f.manual, f.curl, f.thumbAcross);
            }
        }
    }
    drawHand(hand, pos, handRot, mirrored, hide, held, motion);
}

// A hand drawn at (`pos`, `handRot`), holding `held` (the jointed hand; else the six models).
void drawHand(int hand, glm::vec3 pos, glm::vec3 handRot, bool mirrored, bool hide, const Held& held, const glm::mat4& motion)
{
    if(setupRigHand(hand, pos, handRot, mirrored, hide, held, motion))
    {
        return;
    }
    movePose(motion, pos, handRot);

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

        const FingerPose curl = fingerPose(hand, finger);
        place(ve, viewModel(fingerModels[finger]), pos + hands::redirect(foff, handRot),
            {-handRot.x, handRot.y, handRot.z}, curl.frame, mirrored);
        ve.zeroBlend = curl.open;
        ve.ent.skinnum = skin;

        if(hide)
        {
            ve.visible = false;
        }
    }
}

// ----------------------------------------------------------------------------
// Weapon posing mode (vr_posing.cpp): the weapon floating still in front of the player, drawn as the weapon hand holds
// it (by setupWeapon, from a hand put where its settings, whatever they are now, leave the weapon where it floats); the
// posing hand drawn at its controller, passing through it (unsolved; for a moment after a set, wrapping it as play will);
// and what confirming would write (posing::Candidate), worked out from the placement's own steps undone.

// The turn of an entity as drawn (R_EntityMatrix's, without its origin).
[[nodiscard]] glm::mat3 entityTurn(const entity_t& e)
{
    float m[16];
    vec3_t origin{0.f, 0.f, 0.f}, angles;
    VectorCopy(e.angles, angles);
    R_EntityMatrix(m, origin, angles, ENTSCALE_DEFAULT);
    return glm::mat3{toMat4(m)};
}

// Where the floating weapon goes at first: level, facing where the head does, its grip (the holding hand's point: the
// weapon's origin) 40 cm ahead of the head and 35 cm below it (the chest); the sticks turn it about the middle of its
// grip and muzzle.
void placeFloating(const hands::State& s, posing::Session& ps)
{
    const float m2u = units::metresToUnits();
    const glm::vec3 level{0.f, s.headAngles.y, 0.f};
    const glm::vec3 grip = s.head + hands::forward(level) * (0.4f * m2u) - glm::vec3{0.f, 0.f, 0.35f * m2u};
    const bool mirrored = ps.weaponHand == HAND_OFF;
    view::ViewEntity ve;
    place(ve, ps.model, grip, level, 0, mirrored);
    ps.turn = anglesBasis(level);
    ps.modelOrigin = glm::vec3{hotspotFrame(ve.ent, mirrored)[3]};
    const glm::vec3 muzzle = view::anchorPosition(ve, static_cast<int>(weapons::value(ps.slot, Key::MuzzleAnchorVertex)),
        weapons::vec(ps.slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
    ps.pivot = glm::mix(grip, muzzle, 0.5f);
    ps.tiltAxis = ps.turn[1]; // across the view (to the left)
    ps.startOrigin = ps.modelOrigin;
    ps.startTurn = ps.turn;
    ps.placed = true;
}

// The weapon hand put where its weapon, placed by its settings now, is drawn where it floats: the weapon's turn is the
// hand's plus its angle offsets (weaponTurn), its model's origin the hand's point plus its offset in its own frame.
void holdFloating(hands::State& s, const posing::Session& ps)
{
    const int wh = ps.weaponHand;
    const bool mirrored = wh == HAND_OFF;
    const glm::vec3 wt = basisAngles(ps.turn);
    const glm::vec3 o = weaponAngleOffsets(ps.slot, mirrored);
    view::ViewEntity ve;
    place(ve, ps.model, glm::vec3{0.f}, {-wt.x, wt.y, wt.z}, 0, mirrored);
    s.pos[wh] = ps.modelOrigin - glm::vec3{hotspotFrame(ve.ent, mirrored)[3]};
    s.rot[wh] = s.visualRot[wh] = {wt.x + o.x, wt.y - o.y, wt.z - o.z};
    wholeTurn[wh] = glm::mat3{1.f};
}

// The posing hand (`hand`, its controller at `pos`, `rot`: the hands' pose, less the held weapon's Hand and Weapon
// Together offset, plus the posed one's for the weapon hand) holding the floating weapon, and the settings that put the
// weapon there from it (the candidate): the weapon's place relative to the hand as tracked (a hotspot's point and turn:
// the hand as tracked there). It is drawn where it will be held: there, moved by its tuning offset (the weapon's Hand
// Only; a hotspot's Held Hand) if it has one; unsolved (its fingers the controller's), or, for a moment after a set,
// wrapping the weapon as the settings will.
void setupPosingHand(int hand, const glm::vec3& pos, const glm::vec3& rot)
{
    const posing::Session& ps = posing::session();
    posing::Candidate& c = posing::candidate();
    c.valid = false;
    const int wh = ps.weaponHand;
    const bool weaponTarget = ps.target == posing::Target::Weapon;
    const bool mirrored = hand == HAND_OFF, wMirrored = wh == HAND_OFF;
    const int slot = ps.slot;
    const bool reset = posing::resetOffsets();
    const view::ViewEntity& weapon = entities.weapon[wh];
    const glm::mat3 F = anglesBasis(weaponAngleOffsets(weapons::fistSlot(), mirrored)); // the hand's own turn
    const glm::vec3 handRot = basisAngles(anglesBasis(rot) * F);                          // as an empty hand is drawn
    const glm::mat3 H = anglesBasis(handRot);
    const glm::mat3 W = anglesBasis(glm::vec3{-weapon.ent.angles[0], weapon.ent.angles[1], weapon.ent.angles[2]});
    const glm::mat4 hsFrame = hotspotFrame(weapon.ent, wMirrored);

    // The hand as the settings put it (`baseRot`: the same turn as `handRot`, worked out as setupHand does), and as it
    // is drawn, moved by its offset.
    glm::vec3 baseRot = handRot;
    glm::vec3 drawPos = pos, drawRot = handRot;
    bool offset = false;
    Held held{&weapon.ent, wMirrored, 0, true};
    const auto normalized = [](glm::vec3 a) {
        for(int k = 0; k < 3; k++)
        {
            a[k] = std::remainder(a[k], 360.f);
        }
        return a;
    };
    if(weaponTarget)
    {
        // Held, the hand's turn is the weapon's times w0^T times F (attachedTurn: w0 the weapon's turn at the hand's
        // angles 0, from its angle offsets): H = W w0^T F, so w0 = F H^T W.
        const glm::vec3 wt0 = basisAngles(F * glm::transpose(H) * W);
        // weaponTurn(0) is (-o.x, o.y, o.z), o the angle offsets (vr_gunmodelpitch added to the pitch; yaw and roll the
        // other way for the off hand).
        glm::vec3 o{-wt0.x, wt0.y, wt0.z};
        if(mirrored)
        {
            o.y = -o.y;
            o.z = -o.z;
        }
        o.x -= vr_gunmodelpitch.value;
        c.angles = normalized(o);
        // The frame the hand is carried in with those (Hand Only's), and the hand in it.
        glm::vec3 oo = c.angles;
        oo.x += vr_gunmodelpitch.value;
        if(mirrored)
        {
            oo.y = -oo.y;
            oo.z = -oo.z;
        }
        const glm::mat3 frame = W * glm::transpose(anglesBasis({-oo.x, oo.y, oo.z}));
        baseRot = basisAngles(frame * F);
        // The offset: the model's origin from the hand's point, in the weapon's (mirrored) frame, before its scale.
        const weapons::ModelTransform t = weapons::modelTransform(weapon.ent.model);
        glm::vec3 local = glm::transpose(entityTurn(weapon.ent)) * (glm::vec3{hsFrame[3]} - pos) / (t.active ? t.k : 1.f);
        if(wMirrored)
        {
            local.y = -local.y;
        }
        c.offset = local - glm::vec3{0.f, 0.f, vr_gunmodely.value};
        // Hand Only.
        const glm::vec3 p = reset ? glm::vec3{0.f} : weapons::vec(slot, Key::HandOnlyX, Key::HandOnlyY, Key::HandOnlyZ);
        const glm::vec3 a = reset ? glm::vec3{0.f} : weapons::vec(slot, Key::HandOnlyPitch, Key::HandOnlyYaw, Key::HandOnlyRoll);
        offset = p != glm::vec3{0.f} || a != glm::vec3{0.f};
        drawRot = baseRot;
        moveDrawnHand(hand, mirrored, basisAngles(frame), p, a, mirrored, drawPos, drawRot);

        held.trigger = true;
        held.overlap = CLAMP(0.f, weapons::value(slot, Key::GripOverlap), 1.f) * weapons::maxOverlapCm;
        const float curl[handrig::FingerCount] = {weapons::value(slot, Key::FingerCurlThumb), weapons::value(slot, Key::FingerCurlIndex),
            weapons::value(slot, Key::FingerCurlMiddle), weapons::value(slot, Key::FingerCurlRing), weapons::value(slot, Key::FingerCurlPinky)};
        setManualFingers(held, weapons::value(slot, Key::FingerManual) >= 0.5f, curl, weapons::value(slot, Key::FingerThumbAcross));
    }
    else
    {
        weapons::Hotspot h = weapons::hotspot(slot, ps.hotspot);
        h.type = ps.type;
        const bool cup = h.type == weapons::HotspotType::Cup;
        if(h.type == weapons::HotspotType::Blade)
        {
            // The share of the way from the hand to the tip where the hand's point is.
            const glm::vec3 hilt{weapon.ent.origin[0], weapon.ent.origin[1], weapon.ent.origin[2]};
            const glm::vec3 tip = hands::current().muzzleValid[wh] ? hands::current().muzzle[wh] : hilt;
            const glm::vec3 along = tip - hilt;
            const float length2 = glm::dot(along, along);
            h.pos = {length2 > 0.f ? CLAMP(0.f, glm::dot(pos - hilt, along) / length2, 1.f) : 0.f, 0.f, 0.f};
        }
        else
        {
            // Held, the hand's turn is W w0^T times the hotspot's angles (helpingTurn): undone, they are the weapon's
            // fixed-hand angles (a grip) plus the hotspot's, mirrored for the main hand, plus the hand's own (a cup).
            const glm::mat3 w0 = anglesBasis(weaponTurn(glm::vec3{0.f}, slot, wMirrored));
            const glm::mat3 frame = W * glm::transpose(w0);
            glm::vec3 a = basisAngles(w0 * glm::transpose(W) * H);
            if(cup)
            {
                a -= weaponAngleOffsets(weapons::fistSlot(), mirrored);
            }
            a = normalized(a);
            if(!mirrored)
            {
                a.y = -a.y;
                a.z = -a.z;
            }
            if(!cup)
            {
                a -= weapons::vec(slot, Key::TwoHFixedHandPitch, Key::TwoHFixedHandYaw, Key::TwoHFixedHandRoll);
            }
            h.angles = normalized(a);
            baseRot = helpingTurn(hands::current().rot[wh], wh, hand, slot, cup, h.angles);
            // A grip is the hand's point; a cup, its palm's middle.
            const glm::vec3 point = cup ? palmAt(hand, pos, baseRot, mirrored) : pos;
            h.pos = glm::vec3{glm::inverse(hsFrame) * glm::vec4{point, 1.f}};
            // Held Hand.
            if(reset)
            {
                h.visualPos = h.visualAngles = glm::vec3{0.f};
            }
            offset = h.visualPos != glm::vec3{0.f} || h.visualAngles != glm::vec3{0.f};
            drawRot = baseRot;
            moveDrawnHand(hand, mirrored, basisAngles(frame), h.visualPos, h.visualAngles, wMirrored, drawPos, drawRot);
        }
        held.cup = cup;
        held.thumbTop = h.style == weapons::HotspotStyle::ThumbTop;
        held.overlap = CLAMP(0.f, h.overlap, 1.f) * weapons::maxOverlapCm;
        setManualFingers(held, h.manual, h.curl, h.thumbAcross);
        c.spot = h;
    }

    // Posing, the hand passes through the weapon: drawn at its controller (moved by its tuning offset), unsolved, its
    // fingers the controller's curls (drawHand with nothing held), so that it can be put where it should hold it. For a
    // moment after a set (posing::showSolved) it wraps the weapon as play will: the grasp solved at the hand's place on
    // the weapon as the candidate settings put it (as the weapon, held, is: Held::canonical; the palm fitted without the
    // offset), worked out round the hand (small numbers, as the canonical places are).
    const bool solved = posing::showSolved();
    const bool blade = !weaponTarget && ps.type == weapons::HotspotType::Blade;
    if(solved && !blade && handrig::usable(viewModel(handrig::modelName)))
    {
        const auto inRig = [&](const glm::vec3& at, const glm::vec3& turn) {
            entity_t e = weapon.ent;
            for(int i = 0; i < 3; i++)
            {
                e.origin[i] -= pos[i];
            }
            return glm::inverse(rigPlacement(hand, at - pos, turn, mirrored, nullptr)) * grasp::shapeToWorld(e, wMirrored);
        };
        held.canonical = true;
        held.canonicalInRig = inRig(drawPos, drawRot);
        held.offset = offset;
        if(offset)
        {
            held.fitInRig = inRig(pos, baseRot);
        }
    }
    drawHand(hand, drawPos, drawRot, mirrored, false, solved ? held : Held{}, glm::mat4{1.f});

    const RigHand& rh = rigHands[hand];
    const glm::mat4 toWeapon = glm::inverse(hsFrame);
    const glm::mat4 rig = rigPlacement(hand, drawPos, drawRot, mirrored, nullptr);
    c.rigInWeapon = toWeapon * rig;
    c.rigWorld = rig;
    c.palmInWeapon = glm::vec3{toWeapon * (rh.drawn ? rh.rigToWorld * glm::vec4{drawnInRig(rh, grasp::palmCentre()), 1.f}
                                                    : rig * glm::vec4{grasp::palmCentre(), 1.f})};
    c.palmFitted = solved && rh.drawn;
    c.valid = true;
    if(solved)
    {
        posing::solvedPalm(c); // the palm the set pose gives (vr_pose_check)
    }
}

// What the posing shows on the floating weapon: its hotspots; posing one, where it would go (white); posing the weapon,
// its muzzle, its barrel (green) and where the posing hand's shots would go from it (red): turn the hand until they
// run together.
void posingMarks(const hands::State& s, const glm::vec3& aimRot)
{
    const posing::Session& ps = posing::session();
    const posing::Candidate& c = posing::candidate();
    const int wh = ps.weaponHand;
    const view::ViewEntity& weapon = entities.weapon[wh];
    drawHotspots(wh, -1);
    const glm::mat4 hsFrame = hotspotFrame(weapon.ent, wh == HAND_OFF);
    const glm::vec4 white{1.f, 1.f, 1.f, 1.f};
    if(ps.target == posing::Target::Hotspot)
    {
        if(!c.valid)
        {
            return;
        }
        if(c.spot.type == weapons::HotspotType::Blade)
        {
            const glm::vec3 hilt{weapon.ent.origin[0], weapon.ent.origin[1], weapon.ent.origin[2]};
            if(s.muzzleValid[wh])
            {
                const glm::vec3 at = glm::mix(hilt, s.muzzle[wh], c.spot.pos.x);
                lines::line(hilt, s.muzzle[wh], 0.15f, glm::vec4{1.f, 0.6f, 0.1f, 0.5f}, glm::vec4{1.f, 0.6f, 0.1f, 0.5f});
                lines::point(at, 1.6f, white);
            }
            return;
        }
        const glm::vec3 at{hsFrame * glm::vec4{c.spot.pos, 1.f}};
        lines::point(at, 1.6f, white);
        lines::point(at, 3.f, glm::vec4{1.f, 1.f, 1.f, 0.2f});
        return;
    }
    if(!s.muzzleValid[wh])
    {
        return;
    }
    const float length = 2.f * units::metresToUnits();
    const glm::vec3 m = s.muzzle[wh];
    lines::point(m, 1.f, glm::vec4{1.f, 0.9f, 0.3f, 1.f});
    const glm::vec4 green{0.3f, 1.f, 0.35f, 0.8f}, red{1.f, 0.25f, 0.2f, 0.8f};
    lines::line(m, m + glm::normalize(glm::vec3{hsFrame[0]}) * length, 0.1f, green, glm::vec4{glm::vec3{green}, 0.f});
    const glm::vec3 shot = hands::forward(weapons::shotAngles(aimRot, ps.slot, wh == HAND_OFF)); // Shot Pitch and Yaw
    lines::line(m, m + shot * length, 0.12f, red, glm::vec4{glm::vec3{red}, 0.f});
}

// Posing: the floating weapon (in the weapon hand's view entity), the hands (the posing one at its controller; the
// other confirming, or, posing a hotspot, drawn holding the weapon), and the marks. `s` is changed for the view only
// (VR_SetupViewEntities puts it back).
void setupPosing(hands::State& s)
{
    posing::Session& ps = posing::session();
    const int wh = ps.weaponHand, oh = 1 - wh;
    const bool weaponTarget = ps.target == posing::Target::Weapon;
    const int poser = weaponTarget ? wh : oh;

    // The posing hand as an empty hand is (the held weapon's Hand and Weapon Together offset taken off), the weapon hand
    // with the posed weapon's (hand and weapon move together by it: the pose between them doesn't change).
    glm::vec3 pos = s.pos[poser], rot = s.rot[poser];
    hands::undoWholeOffset(s, poser, pos, rot);
    if(weaponTarget && !posing::resetOffsets())
    {
        glm::mat3 turn;
        (void)hands::wholeOffset(ps.slot, poser, pos, rot, turn);
    }

    if(!ps.placed)
    {
        placeFloating(s, ps);
    }
    holdFloating(s, ps);
    setupWeapon(s, wh, ps.model, 0, true);
    if(weaponTarget)
    {
        setupWeapon(s, oh, precachedModel(cl.stats[oh == HAND_MAIN ? STAT_WEAPON : STAT_QVR_WEAPONMODEL2]),
            cl.stats[oh == HAND_MAIN ? STAT_WEAPONFRAME : STAT_QVR_WEAPONFRAME2]);
    }
    else
    {
        setupWeapon(s, oh, viewModel("progs/hand.mdl"), 0); // the posing hand holds nothing of its own
    }
    // No two-handed grip on either (the posing hand isn't taking one).
    s.grip2HValid[HAND_OFF] = s.grip2HValid[HAND_MAIN] = false;

    if(weaponTarget)
    {
        s.pos[wh] = pos; // what follows the hands (the arm, the gadget) follows the posing hand
        s.rot[wh] = s.visualRot[wh] = rot;
        setupPosingHand(wh, pos, rot);
        setupHand(s, oh);
    }
    else
    {
        setupHand(s, wh); // first: a cup wraps it too
        s.pos[oh] = pos;
        s.rot[oh] = s.visualRot[oh] = rot;
        setupPosingHand(oh, pos, rot);
    }
    posingMarks(s, rot);
}

// ----------------------------------------------------------------------------
// Body: holsters, holster slots, torso

// The drawn holsters in vr_body's terms, which are also their stat slots (0 and 1 the shoulders, 2..5 the hips and
// the upper holsters).
constexpr body::Holster bodyHolster[HolsterCount] = {
    body::LeftHip, body::RightHip, body::LeftUpper, body::RightUpper, body::LeftShoulder, body::RightShoulder};

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

// Alias model angles (their pitch inverted) that turn a model's +x to `fwd` and +z to `up`.
[[nodiscard]] glm::vec3 aliasAngles(const glm::vec3& fwd, const glm::vec3& up)
{
    const glm::vec3 a = hands::anglesFromVectors(fwd, up);
    return {-a.x, a.y, a.z};
}

// A holster on the drawn body (body::HolsterPlate) lies against it: legholster.mdl's plate
// (make_holster.py: +x forward, +z up with the belt loop, +y towards the body, where the plate is
// concave; the left holsters are drawn mirrored) faces the way the body's surface does there, its
// +x along the body towards its middle, and its back rests on the surface (the position, a hand's
// reach target, stands a little out of it). The weapon hangs in the loops, muzzle down along the
// plate and tipped out a little (more on the chest, as a chest rig carries it), its top towards
// the body's middle, the grip out towards the hand.
struct HolsterPose
{
    glm::vec3 slotPos, slotAngles, weaponPos, weaponAngles;
};

[[nodiscard]] HolsterPose holsterOnBody(const glm::vec3& pos, const body::HolsterPlate& plate, bool mirrored,
    bool upper, qmodel_t* slotModel)
{
    const glm::vec3 up = plate.up;
    const glm::vec3 fwd = mirrored ? glm::cross(plate.out, up) : glm::cross(up, plate.out);
    const glm::vec3 towardsBody = -plate.out;

    // The plate's back (make_holster.py: PLATE_Y + PLATE_T / 2 from the weapon) as the holster
    // model is drawn (vr_leg_holster_model_*: scaled, then moved in its axes).
    constexpr float plateBack = 2.95f;
    const weapons::ModelTransform t = weapons::modelTransform(slotModel);
    const glm::vec3 offset = t.active ? t.k * t.offset : glm::vec3{0.f};
    const float back = t.active ? t.k * (t.offset.y + t.scale.y * plateBack) : plateBack;
    const glm::vec3 slotPos = pos + plate.out * CLAMP(-4.f, back - plate.clearance, 4.f);

    // The model's origin (the loops round it) in the world: the weapon's grip goes there.
    const glm::vec3 loops = slotPos + fwd * offset.x + towardsBody * offset.y + up * offset.z;

    const float tip = glm::radians(upper ? 25.f : 10.f);
    const glm::vec3 muzzle = -up * std::cos(tip) - fwd * std::sin(tip);
    const glm::vec3 top = fwd * std::cos(tip) - up * std::sin(tip);

    return {slotPos, aliasAngles(fwd, up), loops, aliasAngles(muzzle, top)};
}

// The shoulder holsters (reached over the shoulder, vr_shoulder_holster_*): the gun hangs down the back from behind
// the shoulder, its top away from the back, tipped out a little; its grip where the hand reaches for it.
[[nodiscard]] HolsterPose holsterOnBack(const glm::vec3& pos, float yaw, bool left)
{
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, yaw, 0.f}, fwd, right, up);
    const float tip = glm::radians(12.f);
    const glm::vec3 muzzle = -up * std::cos(tip) + (left ? -right : right) * std::sin(tip);
    const glm::vec3 at = pos - fwd * 3.f;
    return {at, glm::vec3{0.f}, at, aliasAngles(muzzle, -fwd)};
}

// Each pair's turn (the Hotspots menu, vr_*_holster_pitch/yaw/roll): degrees, {pitch, yaw, roll}.
[[nodiscard]] glm::vec3 holsterTurn(int h)
{
    switch(h)
    {
        case LeftHip:
        case RightHip: return {vr_hip_holster_pitch.value, vr_hip_holster_yaw.value, vr_hip_holster_roll.value};
        case LeftUpper:
        case RightUpper: return {vr_upper_holster_pitch.value, vr_upper_holster_yaw.value, vr_upper_holster_roll.value};
        default: return {vr_shoulder_holster_pitch.value, vr_shoulder_holster_yaw.value, vr_shoulder_holster_roll.value};
    }
}

// The body's frame at a holster (turnHolster), made orthonormal: `out` away from the body, `up`, and `outwards`.
struct HolsterFrame
{
    glm::vec3 out, up, side;
};

[[nodiscard]] HolsterFrame holsterFrame(const glm::vec3& out, const glm::vec3& up, const glm::vec3& outwards)
{
    const glm::vec3 o = glm::normalize(out);
    const glm::vec3 u = glm::normalize(up - o * glm::dot(up, o));
    return {o, u, glm::normalize(outwards - o * glm::dot(outwards, o) - u * glm::dot(outwards, u))};
}

// A turn in a holster's frame: pitch tips the top towards `out`, yaw turns `out` towards `side`, roll tips the top
// towards `side`: yaw then pitch then roll, as Quake's angles. A left holster's frame is the right one's mirror image,
// so yaw and roll mirror and pitch does not.
[[nodiscard]] glm::quat holsterRotation(const HolsterFrame& f, const glm::vec3& turn)
{
    return glm::angleAxis(glm::radians(turn.y), glm::cross(f.out, f.side)) *
           glm::angleAxis(glm::radians(turn.x), glm::cross(f.up, f.out)) *
           glm::angleAxis(glm::radians(turn.z), glm::cross(f.up, f.side));
}

// Alias model angles turned by `q` in the world.
[[nodiscard]] glm::vec3 turnAliasAngles(const glm::quat& q, const glm::vec3& drawn)
{
    glm::vec3 f, r, t;
    hands::angleVectors({-drawn.x, drawn.y, drawn.z}, f, r, t); // alias models' pitch is the other way
    return aliasAngles(q * f, q * t);
}

// Turns a holster and the gun in it by `turn` about `pivot` (the holster's position: where the hand reaches for it
// stays), in the body's frame there (holsterFrame: `outwards` the body's right for the right holsters, its left for the
// left ones, so that the pairs mirror as their offsets' Y does; holsterRotation). `frame` becomes the turned frame.
void turnHolster(HolsterPose& pose, const glm::vec3& pivot, HolsterFrame& frame, const glm::vec3& turn)
{
    if(turn == glm::vec3{0.f})
    {
        return; // exactly as before
    }

    const glm::quat q = holsterRotation(frame, turn);
    pose.slotPos = pivot + q * (pose.slotPos - pivot);
    pose.weaponPos = pivot + q * (pose.weaponPos - pivot);
    pose.slotAngles = turnAliasAngles(q, pose.slotAngles);
    pose.weaponAngles = turnAliasAngles(q, pose.weaponAngles);
    frame = {q * frame.out, q * frame.up, q * frame.side};
}

// The weapon alone in its holster, by its own Holstered pose (weapons::holsteredPose): moved in the holster's (turned)
// frame, x off the body, y outwards (mirrored with the frame), z up, and turned about its own place (its grip, the
// model's origin once in the hand's transform) as the holster is turned.
void poseHolstered(HolsterPose& pose, const HolsterFrame& frame, const weapons::HolsteredPose& p)
{
    if(p.offset == glm::vec3{0.f} && p.angles == glm::vec3{0.f})
    {
        return; // exactly as before
    }
    pose.weaponPos += frame.out * p.offset.x + frame.side * p.offset.y + frame.up * p.offset.z;
    if(p.angles != glm::vec3{0.f})
    {
        pose.weaponAngles = turnAliasAngles(holsterRotation(frame, p.angles), pose.weaponAngles);
    }
}

// A gun not in a hand (holstered, lying in the world) carries its button and ammo screen as a held one does
// (vr_weapon_screen_idle), so that they do not pop up as it is taken: `button` placed (or hidden), the screen queued
// when `queueText` (once a frame). `clip`: its clip if known (a holstered gun's), else -1.
void idleAttachments(const entity_t& e, bool mirrored, int slot, view::ViewEntity& button, int clip, bool queueText)
{
    const bool on = vr_weapon_screen_idle.value != 0.f && slot >= 0 && e.model && !isHandModel(e.model) &&
                    slot != weapons::fistSlot();
    const glm::vec3 drawn{e.angles[0], e.angles[1], e.angles[2]};
    if(on && weapons::value(slot, Key::WpnButtonMode) != 0.f)
    {
        const glm::vec3 pos = view::entityAnchorPosition(e, mirrored, 0.f,
            static_cast<int>(weapons::value(slot, Key::WpnButtonAnchorVertex)),
            weapons::vec(slot, Key::WpnButtonX, Key::WpnButtonY, Key::WpnButtonZ));
        glm::vec3 angles = weapons::vec(slot, Key::WpnButtonPitch, Key::WpnButtonYaw, Key::WpnButtonRoll);
        if(mirrored)
        {
            angles.z = -angles.z;
        }
        angles = idleAttachmentAngles(drawn, slot, mirrored, angles);
        angles.x = -angles.x; // alias models' pitch is the other way
        place(button, viewModel("progs/wpnbutton.mdl"), pos, angles, 0, mirrored);
        button.ent.alpha = e.alpha;
    }
    else
    {
        button.visible = false;
    }

    if(on && queueText && vr_show_weapon_text.value && weapons::value(slot, Key::WpnTextMode) != 0.f)
    {
        const glm::vec3 pos = view::entityAnchorPosition(e, mirrored, 0.f,
            static_cast<int>(weapons::value(slot, Key::WpnTextAnchorVertex)),
            weapons::vec(slot, Key::WpnTextX, Key::WpnTextY, Key::WpnTextZ));
        glm::vec3 angles = weapons::vec(slot, Key::WpnTextPitch, Key::WpnTextYaw, Key::WpnTextRoll);
        if(mirrored)
        {
            angles.z = -angles.z;
        }
        text3d::queue(idleWeaponText(e.model, clip), pos, idleAttachmentAngles(drawn, slot, mirrored, angles),
            text3d::Align::Centre, 0.1f * weapons::value(slot, Key::WpnTextScale), vr_weapon_screen.value != 0.f);
    }
}

// The body's preview (vr_body_debug 2 and 3): in front of the player, turned to face them or seen from its left. Its turn,
// and where a point of the body is drawn in it.
[[nodiscard]] glm::mat3 bodyPreviewTurn()
{
    return glm::mat3_cast(
        glm::angleAxis(glm::radians(vr_body_debug.value >= 3.f ? -90.f : 180.f), glm::vec3{0.f, 0.f, 1.f}));
}

[[nodiscard]] glm::vec3 bodyPreviewPoint(const hands::State& s, const glm::vec3& pos)
{
    const glm::vec3 root{s.head.x, s.head.y, 0.f};
    const glm::vec3 centre =
        root + hands::forward({0.f, s.bodyYaw, 0.f}) * (1.8f * units::metresToUnits() * units::bodyScale());
    return centre + bodyPreviewTurn() * (pos - root);
}

void setupHolsters(const hands::State& s, bool queueTexts)
{
    const float yaw = s.bodyYaw;

    // The hips and upper holsters without a body to lie on, turned with the body's yaw.
    const glm::vec3 angles[LeftShoulder] = {
        {-90.f, 0.f, -yaw + 10.f}, {-90.f, 0.f, -yaw - 10.f}, {-20.f, yaw + 180.f, 0.f},
        {-20.f, yaw + 180.f, 0.f}};

    const glm::vec3 slotAngles[LeftShoulder] = {
        {0.f, yaw - 10.f, 0.f}, {0.f, yaw + 10.f, 0.f}, {-30.f, yaw - 10.f, 0.f},
        {-30.f, yaw + 10.f, 0.f}};

    // Weapon Offsets > Holstered, a setting chosen: the page's weapon (the one its hand holds) previewed in the holsters.
    int previewHand = 0, previewKind = 0;
    qmodel_t* previewModel = nullptr;
    const bool preview = menu::holsterPreview(previewHand, previewKind) &&
                         (previewModel = weapons::heldModel(previewHand)) != nullptr;

    body::HolsterPlates plates;
    const body::HolsterPositions positions = body::holsterPositions(s, &plates); // one body solve for all
    qmodel_t* const slotModel = vr_leg_holster_model_enabled.value ? viewModel("progs/legholster.mdl") : nullptr;
    for(int h = 0; h < HolsterCount; h++)
    {
        const bool shoulder = h == LeftShoulder || h == RightShoulder;
        const bool mirrored = h == LeftHip || h == LeftUpper || h == LeftShoulder;
        const glm::vec3 pos = positions[static_cast<std::size_t>(bodyHolster[h])];
        const bool hover = hovered(s, bodyHolster[h]);

        // The shoulders' guns (round 20): drawn too, on the back; a gun let go there was nowhere to be seen.
        HolsterPose pose = shoulder ? holsterOnBack(pos, yaw, mirrored) : HolsterPose{pos, slotAngles[h], pos, angles[h]};
        // The body's frame at the holster, for its turn (turnHolster): off the body, the body's yaw (the shoulders'
        // out is behind, down the back).
        glm::vec3 bodyFwd, bodyRight, bodyUp;
        hands::angleVectors({0.f, yaw, 0.f}, bodyFwd, bodyRight, bodyUp);
        glm::vec3 pivot = pos;
        glm::vec3 out = shoulder ? -bodyFwd : bodyFwd;
        glm::vec3 up = bodyUp;
        glm::vec3 outwards = mirrored ? -bodyRight : bodyRight;
        if(body::HolsterPlate plate = plates[static_cast<std::size_t>(bodyHolster[h])]; !shoulder && plate.out != glm::vec3{0.f})
        {
            glm::vec3 at = pos;
            if(vr_body_debug.value >= 2.f)
            {
                // The body's preview carries them too.
                const glm::mat3 turn = bodyPreviewTurn();
                at = bodyPreviewPoint(s, pos);
                plate.out = turn * plate.out;
                plate.up = turn * plate.up;
                outwards = turn * outwards;
            }
            pose = holsterOnBody(at, plate, mirrored, h == LeftUpper || h == RightUpper, slotModel);
            pivot = at;
            out = plate.out;
            up = plate.up;
        }
        HolsterFrame frame = holsterFrame(out, up, outwards);
        turnHolster(pose, pivot, frame, holsterTurn(h));

        const int stat = static_cast<int>(bodyHolster[h]);
        qmodel_t* model = precachedModel(cl.stats[STAT_QVR_HOLSTERWEAPONMODEL0 + stat]);
        int clip = cl.stats[STAT_QVR_HOLSTERWEAPONCLIP0 + stat];
        const weapons::HolsterKind kind = shoulder ? weapons::HolsterKind::Shoulder
                                          : h == LeftUpper || h == RightUpper ? weapons::HolsterKind::Upper
                                                                               : weapons::HolsterKind::Hip;
        if(preview && kind == static_cast<weapons::HolsterKind>(previewKind))
        {
            model = previewModel; // Weapon Offsets > Holstered: the page's weapon, in both holsters of the kind edited
            clip = -1;
        }
        if(isHandModel(model))
        {
            model = nullptr;
        }
        poseHolstered(pose, frame, weapons::holsteredPose(weapons::slotForModel(model), kind));

        view::ViewEntity& ve = entities.holster[h];
        place(ve, model, pose.weaponPos, pose.weaponAngles, 0, mirrored);
        highlight(ve, hover);
        // Just holstered: eased from the hand into the holster (vr_drawblend.cpp).
        drawblend::holster(s, stat, model ? &ve.ent : nullptr, !(preview && kind == static_cast<weapons::HolsterKind>(previewKind)));

        if(slotModel && !shoulder)
        {
            place(entities.holsterSlot[h], slotModel, pose.slotPos,
                pose.slotAngles, 0, mirrored);
            highlight(entities.holsterSlot[h], hover);
        }
        else
        {
            entities.holsterSlot[h].visible = false;
        }

        // Its ammo screen and button, and a lava gun's glow, as in a hand.
        const int slot = weapons::slotForModel(model);
        idleAttachments(ve.ent, mirrored, slot, entities.holsterButton[h], clip, queueTexts && model != nullptr);
        highlight(entities.holsterButton[h], hover);
        if(model && slot >= 0 && emissive::isLavaGun(model))
        {
            emissive::lavaGunLight(2 + h, lavaGlowPosition(ve.ent, mirrored, 0.f, slot), vr_lavagun_light_idle.value);
        }
    }
}

// The grenade pouch (vr_handgrenade; ROUND21.md, "Hand grenades from the back pouch"): vrpouch.mdl (make_pouch.py: +x out
// of the body, +z up, its back against the body at the origin) on the belt at the small of the back, facing the body's
// surface there (straight back without the body), turned by vr_grenade_pouch_pitch/yaw/roll about where the hand
// reaches for it (the holsters' axes: out, up, and the body's right as "outwards"). Frame 0 full (you have rockets),
// 1 empty (flat); lit up while a hand is at it, as a holster is.
void setupPouch(const hands::State& s)
{
    view::ViewEntity& ve = entities.pouch;
    qmodel_t* const model = body::pouchEnabled() ? viewModel("progs/vrpouch.mdl") : nullptr;
    if(!model)
    {
        ve.visible = false;
        return;
    }
    body::HolsterPlate plate;
    const glm::vec3 pos = body::pouchPosition(s, &plate);
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, s.bodyYaw, 0.f}, fwd, right, up);
    glm::vec3 out = plate.out != glm::vec3{0.f} ? plate.out : -fwd;
    glm::vec3 surfaceUp = plate.out != glm::vec3{0.f} ? plate.up : up;
    glm::vec3 outwards = right;
    glm::vec3 at = pos;
    if(vr_body_debug.value >= 2.f)
    {
        const glm::mat3 turn = bodyPreviewTurn();
        at = bodyPreviewPoint(s, pos);
        out = turn * out;
        surfaceUp = turn * surfaceUp;
        outwards = turn * outwards;
    }
    HolsterFrame frame = holsterFrame(out, surfaceUp, outwards);
    const float clearance = plate.out != glm::vec3{0.f} ? CLAMP(0.f, plate.clearance, 4.f) : 0.f;
    HolsterPose pose{at - frame.out * clearance, aliasAngles(frame.out, frame.up), at, glm::vec3{0.f}};
    turnHolster(pose, at, frame,
        {vr_grenade_pouch_pitch.value, vr_grenade_pouch_yaw.value, vr_grenade_pouch_roll.value});
    place(ve, model, pose.slotPos, pose.slotAngles, cl.stats[STAT_ROCKETS] >= 1 ? 0 : 1, false);
    highlight(ve, s.hotspot[HAND_OFF] == body::HS_GRENADE_POUCH || s.hotspot[HAND_MAIN] == body::HS_GRENADE_POUCH);
}

// The weapons lying in the world (map pickups are the guns themselves in Quake VR: thrown weapons,
// func_weapon_grabbable) near the player carry their ammo screen and button too (vr_weapon_screen_idle): the nearest
// maxWorldWeapons within reach; a lava gun among them glows (dimmer, vr_lavagun_light_idle).
void setupWorldWeapons(const hands::State& s, bool queueTexts)
{
    constexpr float reach = 320.f;
    struct Near
    {
        const entity_t* e;
        float dist;
    };
    Near nearest[maxWorldWeapons]{};
    int count = 0;
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        const entity_t* e = cl_visedicts[i];
        if(!e || !e->model || e->model->type != mod_alias || view::find(e) || e == &cl_entities[cl.viewentity] ||
            strncmp(e->model->name, "progs/v_", 8) || weapons::slotForModel(e->model) < 0)
        {
            continue;
        }
        const float d = glm::distance(glm::vec3{e->origin[0], e->origin[1], e->origin[2]}, s.head);
        if(d > reach || (count == maxWorldWeapons && d >= nearest[maxWorldWeapons - 1].dist))
        {
            continue;
        }
        // Kept sorted, nearest first.
        int at = count < maxWorldWeapons ? count++ : maxWorldWeapons - 1;
        while(at > 0 && nearest[at - 1].dist > d)
        {
            nearest[at] = nearest[at - 1];
            at--;
        }
        nearest[at] = {e, d};
    }

    int lights = 0;
    for(int i = 0; i < maxWorldWeapons; i++)
    {
        view::ViewEntity& button = entities.worldButton[i];
        if(i >= count)
        {
            button.visible = false;
            continue;
        }
        const entity_t& e = *nearest[i].e;
        const int slot = weapons::slotForModel(e.model);
        idleAttachments(e, false, slot, button, -1, queueTexts);
        if(emissive::isLavaGun(e.model) && 2 + HolsterCount + lights < emissive::lavaGunLights)
        {
            emissive::lavaGunLight(2 + HolsterCount + lights++, lavaGlowPosition(e, false, 0.f, slot), vr_lavagun_light_idle.value);
        }
    }
}

// The drawn hands' wrists and orientations, for the body's arms and the wrist gadget.
[[nodiscard]] avatar::HandPose drawnHand(const hands::State& s, int hand)
{
    // The centre of the wrist in hand_base.mdl (frame 0), or the jointed hand's (its palm is shorter: make_hand_rig.py).
    const glm::vec3 handWrist = rigHands[hand].drawn ? vec3Of(handrig::data::wrist) : glm::vec3{-6.86f, -1.08f, 1.42f};

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

// Body Calibration (vr_bodycal.cpp): the empty hand on the calibrated controller (vr_view.hpp).
bool emptyHandImpl(const hands::State& s, int hand, view::EmptyHand& out)
{
    if(!s.valid || hand < 0 || hand > 1)
    {
        return false;
    }
    if(!handrig::usable(viewModel(handrig::modelName)))
    {
        const avatar::HandPose hp = drawnHand(s, hand); // (the hand as drawn)
        out = {hp.wrist, hp.forward, hp.up, hp.back, false};
        return true;
    }
    // As previewGripMove places it: the fist's own Hand and Weapon Together offset and angles, whatever the hand holds.
    const bool mirrored = hand == HAND_OFF;
    const int fist = weapons::fistSlot();
    glm::vec3 pos = s.calibratedPos[hand], rot = s.calibratedRot[hand];
    glm::mat3 turn{1.f};
    hands::wholeOffset(fist, hand, pos, rot, turn);
    const glm::vec3 handRot = basisAngles(anglesBasis(rot) * anglesBasis(weaponAngleOffsets(fist, mirrored)));
    const glm::mat4 m = rigPlacement(hand, pos, handRot, mirrored, nullptr);
    const glm::vec3 w = vec3Of(handrig::data::wrist);
    const auto at = [&](const glm::vec3& p) { return glm::vec3{m * glm::vec4{p, 1.f}}; };
    out.wrist = at(w);
    // (drawnHand's axes: +x towards the fingers, +z the index finger's side, +y the palm's.)
    out.forward = glm::normalize(at(w + glm::vec3{1.f, 0.f, 0.f}) - out.wrist);
    out.up = glm::normalize(at(w + glm::vec3{0.f, 0.f, 1.f}) - out.wrist);
    out.back = glm::normalize(at(w + glm::vec3{0.f, -1.f, 0.f}) - out.wrist);
    out.jointed = true;
    return true;
}

// The build of the body drawn this frame (0 lean, 1 athletic, 2 brawny; the wrist gadget's straps fit its bracer).
int drawnBuild = 1;

// The wrist gadget (vr_hud_mode 1): over the back of the off hand's forearm, just behind the
// wrist, its screen facing out of the back of the hand like a watch's. It reads like one: with
// the forearm raised across the chest, its right is towards the fingers (the left arm's; the
// right arm's, towards the elbow) and its up away from the player.
//
// It is strapped to the forearm, not the hand: along the forearm's axis (the elbow to the wrist), turned about it
// with the forearm's own twist where it sits (the body's twist joints: a share of the hand's roll growing from the
// elbow to the wrist, as the bracer under it turns). Bending the wrist (up, down or to the sides) doesn't move it;
// rolling the hand turns the forearm and the gadget with it. Without the body drawn, the arms are solved all the same
// (avatar::solveArms, the same IK) for their forearms.
//
// Two straps (vrgadget_strap.mdl) hold it round the forearm under its lugs: in the forearm's own frame there (the
// twist joints' turn), sized to the bracer's ring of the body's build (avatar::forearmGirth; the athletic one without
// a body) a millimetre and a half out, as a cone as the bracer narrows (frame 1 blended towards frame 0's cylinder).
// They follow the forearm, not the player's offsets and turns of the gadget, which rests on them.
void setupGadget(const hands::State& s)
{
    view::ViewEntity& ve = entities.gadget;
    entities.gadgetStrap[0].visible = entities.gadgetStrap[1].visible = false;
    if(!gadget::active())
    {
        ve.visible = false;
        gadget::setPose({});
        return;
    }

    const int hand = vr_gadget_hand.value != 0.f ? HAND_MAIN : HAND_OFF;
    const bool leftArm = (hand == HAND_OFF) == (vr_lefthanded.value == 0.f);
    const avatar::HandPose hp = drawnHand(s, hand);
    const float body = units::bodyScale();
    const float m2w = units::metresToUnits() * body;

    // Where along the forearm it sits: 8.5 cm behind the wrist, and the player's Along the Arm (towards the fingers
    // on the left arm).
    const float behind = 0.085f * m2w - vr_gadget_x.value * 0.01f * m2w * (leftArm ? 1.f : -1.f);
    avatar::ForearmFrame fa;
    bool onForearm = avatar::forearmFrame(hand, 0.f, fa);
    const int build = onForearm ? drawnBuild : 1; // (the body's, if it was posed)
    glm::vec3 elbow = fa.point;
    if(!onForearm)
    {
        const avatar::HandPose handPoses[2] = {drawnHand(s, 0), drawnHand(s, 1)};
        avatar::solveArms(s, handPoses);
        onForearm = avatar::forearmFrame(hand, 0.f, fa);
        elbow = fa.point;
    }
    glm::vec3 wrist = hp.wrist;
    glm::vec3 dir = hp.forward;
    glm::vec3 back = hp.back;
    if(onForearm && fa.length > 1e-3f && avatar::forearmFrame(hand, 1.f - behind / fa.length, fa))
    {
        dir = fa.axes[0];
        wrist = fa.point + dir * behind;
        back = fa.axes * (glm::transpose(fa.hand) * hp.back); // the back of the hand, carried by the forearm there
    }

    glm::vec3 out = back - dir * glm::dot(back, dir);
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
    place(ve, viewModel("progs/vrgadget.mdl"), pose.origin, {-a.x, a.y, a.z}, 0, false);
    ve.scale = glm::vec3{scale}; // exact: the screen's image is drawn at this scale over it

    // The casing's tint: its lighting times a colour.
    const float tint = CLAMP(0.f, vr_gadget_tint.value, 1.f);
    ve.lightMultiply = tint > 0.f;
    ve.lightMod = glm::mix(glm::vec3{1.f}, hsv(vr_gadget_tint_hue.value, 1.f, 1.f) * 1.4f, tint);

    // The straps, under the lugs (make_gadget.py: x -+1.25, the band 0.3 either side of it).
    if(!onForearm || fa.length <= 1e-3f)
    {
        return;
    }
    constexpr float strapTaper = 0.6f; // make_gadget.py's STRAP_TAPER
    constexpr float margin = 0.0015f;  // metres off the bracer
    qmodel_t* strapModel = viewModel("progs/vrgadget_strap.mdl");
    const float halfWidth = 0.3f * scale;
    for(int i = 0; i < 2; i++)
    {
        const glm::vec3 lug = pose.origin + pose.axes * (glm::vec3{i == 0 ? -1.25f : 1.25f, 0.f, -0.47f} * scale);
        const float along = glm::dot(lug - elbow, dir) / fa.length;
        avatar::ForearmFrame sf;
        if(!avatar::forearmFrame(hand, along, sf))
        {
            continue;
        }
        const float edge = halfWidth / fa.length;
        float wu, wv, eu, ev; // the bracer's semi-axes at the band's wrist edge and elbow edge
        avatar::forearmGirth(build, along + edge, wu, wv);
        avatar::forearmGirth(build, along - edge, eu, ev);
        wu += margin, wv += margin, eu += margin, ev += margin;
        const float taper = CLAMP(0.f, std::max(eu / wu, ev / wv) - 1.f, strapTaper);
        view::ViewEntity& strap = entities.gadgetStrap[i];
        const glm::vec3 sa = hands::anglesFromVectors(sf.axes[0], sf.axes[2]);
        place(strap, strapModel, sf.point, {-sa.x, sa.y, sa.z}, 1, false);
        strap.scale = {halfWidth, wv * m2w, wu * m2w}; // model y: the frame's y (the ring's other axis), z: its hint
        strap.zeroBlend = 1.f - taper / strapTaper;    // frame 1 (the cone) towards frame 0 (the cylinder)
    }
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
        ve.ent.skinnum = armorWorn() * 4 + (wounds::replacesSkins() ? 0 : damageLevel()); // wounds painted instead (vr_wounds.cpp)
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

    const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(realtime * 6.0));
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
    drawnBuild = 1;

    // Not while dead (the view lies on the floor) or at the end of a level.
    const bool alive = cl.stats[STAT_HEALTH] > 0 && !cl.intermission;
    if(mode >= 1 && alive)
    {
        // The build (vr_body_build): progs/vrbody_lean, vrbody (athletic) or vrbody_brawny.
        const int build = static_cast<int>(vr_body_build.value);
        const char* name = build <= 0 ? "progs/vrbody_lean.mdl" : build >= 2 ? "progs/vrbody_brawny.mdl" : "progs/vrbody.mdl";
        qmodel_t* model = viewModel(name);
        if(!avatar::usable(model))
        {
            model = viewModel("progs/vrbody.mdl");
        }
        if(avatar::usable(model))
        {
            drawnBuild = model == viewModel("progs/vrbody.mdl") ? 1 : CLAMP(0, build, 2);
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

// Pressing a weapon's button with the other hand's fingertip toggles its secondary ammo (old
// engine's VR_DoWpnButton, which sent keys bound to these impulses).
struct ButtonState
{
    bool hover{false};
    double lastCheck{0.0}; // cl.time
};
ButtonState buttonStates[2];

void pressWeaponButtons(const hands::State& s)
{
    ButtonState (&states)[2] = buttonStates;

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

    place(ve, viewModel("progs/wpnbutton.mdl"), pos, angles, 0, mirrored);
}

} // namespace

namespace qvr::view
{

bool emptyHandPose(const hands::State& s, int hand, EmptyHand& out)
{
    return emptyHandImpl(s, hand, out);
}

const ViewEntity* find(const entity_t* e)
{
    const auto* first = reinterpret_cast<const std::byte*>(&entities);
    const auto* p = reinterpret_cast<const std::byte*>(e);
    if(p < first || p >= first + sizeof(entities))
    {
        return nullptr;
    }

    // The view entities are one run of ViewEntity (Entities holds nothing else): the one it falls in.
    static_assert(sizeof(Entities) % sizeof(ViewEntity) == 0 && alignof(Entities) == alignof(ViewEntity));
    const auto* ve = reinterpret_cast<const ViewEntity*>(&entities) + (p - first) / sizeof(ViewEntity);
    return &ve->ent == e ? ve : nullptr;
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

glm::vec3 entityAnchorPosition(const entity_t& e, bool mirrored, float zeroBlend, int anchorIndex, const glm::vec3& extra)
{
    if(!e.model || e.model->type != mod_alias)
    {
        return {e.origin[0], e.origin[1], e.origin[2]};
    }
    if(const ViewEntity* ve = find(&e))
    {
        return anchorPosition(*ve, anchorIndex, extra);
    }

    float m[16];
    render::entityMatrix(e, mirrored, e.scale ? e.scale : ENTSCALE_DEFAULT, extra, m);

    const glm::vec3 v = anchor::posedVertex(e, anchorIndex, zeroBlend);
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12],
        m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
        m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
}

// QVR flashlight on guns (round 20).
bool weaponMount(int hand, WeaponMount& out)
{
    const ViewEntity& ve = entities.weapon[hand];
    const int slot = weapons::slotForModel(ve.ent.model);
    if(!ve.visible || !ve.ent.model || slot < 0 || isHandModel(ve.ent.model))
    {
        return false;
    }
    out.model = ve.ent.model;
    out.pos = drawnAs[hand].pos;
    out.rot = drawnAs[hand].rot;
    out.mirrored = drawnAs[hand].mirrored;
    out.muzzle = anchorPosition(ve, static_cast<int>(weapons::value(slot, Key::MuzzleAnchorVertex)),
        weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
    return true;
}

const ViewEntity* heldWeapon(int hand)
{
    if(hand < 0 || hand > 1)
    {
        return nullptr;
    }
    const ViewEntity& ve = entities.weapon[hand];
    const bool weapon = ve.visible && ve.ent.model && ve.ent.model->type == mod_alias &&
                        weapons::slotForModel(ve.ent.model) >= 0 && !isHandModel(ve.ent.model);
    return weapon ? &ve : nullptr;
}

int handOf(const entity_t* e, bool& weapon)
{
    for(int hand = 0; hand < 2; hand++)
    {
        if(e == &entities.weapon[hand].ent || e == &entities.weaponMorph[hand].ent)
        {
            weapon = true;
            return hand;
        }
        for(const ViewEntity& ve : entities.hand[hand])
        {
            if(e == &ve.ent)
            {
                weapon = false;
                return hand;
            }
        }
    }
    return -1;
}

bool weaponFrame(const hands::State& s, int hand, WeaponFrame& out)
{
    const ViewEntity& ve = entities.weapon[hand];
    const int slot = weapons::slotForModel(ve.ent.model);
    if(!ve.visible || !ve.ent.model || ve.ent.model->type != mod_alias || slot < 0 || isHandModel(ve.ent.model))
    {
        return false;
    }
    out.model = ve.ent.model;
    const glm::vec3 o = modelPoint(ve, glm::vec3{0.f});
    out.modelToWorld = glm::mat4{glm::vec4{modelPoint(ve, {1.f, 0.f, 0.f}) - o, 0.f}, glm::vec4{modelPoint(ve, {0.f, 1.f, 0.f}) - o, 0.f},
        glm::vec4{modelPoint(ve, {0.f, 0.f, 1.f}) - o, 0.f}, glm::vec4{o, 1.f}};
    glm::vec3 cp, cd;
    float radius;
    const RigHand& rh = rigHands[hand];
    const ViewEntity& he = entities.hand[hand][FingerBase];
    out.fistFromRig = rh.drawn && he.ent.model && grasp::gripChannel(rh.pose, cp, cd, radius);
    if(out.fistFromRig)
    {
        float m[16];
        render::entityMatrix(he.ent, he.mirrored, ENTSCALE_DEFAULT, glm::vec3{0.f}, m); // the rig as drawn
        out.fist = glm::vec3{toMat4(m) * glm::vec4{cp, 1.f}};
    }
    else
    {
        out.fist = hands::palmPoint(s, hand);
    }
    return true;
}

bool sameGun(const qmodel_t* a, const qmodel_t* b)
{
    return a == b || morphKind(a, b) >= 0;
}

} // namespace qvr::view

extern "C" int VR_HideViewModel()
{
    return hands::current().valid;
}

// Moves the view to the eye being rendered (see vr_stereo.cpp).
static void applyEyeView(const hands::State& s)
{
    if(stereo::isSpectator())
    {
        // The desktop window's spectator camera (vr_window.cpp): its projection is its own too (VR_OverrideProjection).
        const window::Camera& c = window::spectatorCamera();
        for(int i = 0; i < 3; i++)
        {
            r_refdef.vieworg[i] = c.origin[i];
            r_refdef.viewangles[i] = c.angles[i];
        }
        r_refdef.fov_x = glm::degrees(2.f * std::atan(c.tanX));
        r_refdef.fov_y = glm::degrees(2.f * std::atan(c.tanY));
        return;
    }
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

} // namespace

// A new map (VR_OnClientClearState). The client's time starts over with each map, so what is timed by it would
// act again when the new map's time reaches the old times (a parried blow's knock replaying its wobble on the
// drawn hand and its weapon, moving the weapon's far end and the melee's striking points); and what is eased
// from frame to frame (or solved from the last frame's solution, the grasp) starts from nothing, as on the first
// map: the same start on every map.
void view::resetClientState()
{
    drawblend::reset();
    for(HandImpact& h : handImpacts)
    {
        h = HandImpact{};
    }
    for(ButtonState& b : buttonStates)
    {
        b = ButtonState{};
    }
    for(Morph& m : morphs)
    {
        m = Morph{};
    }
    for(RigHand& rh : rigHands)
    {
        rh.grasp = Grasp{};
        rh.held = Held{}; // (a pointer into cl_entities, which the new map's client state replaces)
        for(auto& finger : rh.joints)
        {
            for(float& j : finger)
            {
                j = 0.f;
            }
        }
        rh.palm = glm::vec3{0.f};
        rh.turn = glm::quat{1.f, 0.f, 0.f, 0.f};
        rh.thumb = glm::quat{1.f, 0.f, 0.f, 0.f};
        rh.jointsTime = -1.0;
        rh.pushed = glm::vec3{0.f};
        rh.pushedTime = -1.0;
    }
    for(int h = 0; h < 2; h++)
    {
        for(int f = 0; f < FingerCount; f++)
        {
            fingerFrames[h][f] = 0.f;
            fingerBias[h][f] = 0.f;
        }
        chosenGrip[h] = -1;
    }
    fingerFramesTime = -1.0;
    forEachEntity([](view::ViewEntity& ve) {
        ve.lastModel = nullptr; // (its animation's lerp starts over: LERP_RESETANIM)
        ve.morph = 0.f;
    });
}

namespace
{

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

// Where each hand's palm is, as the empty jointed hand is drawn (hands::State::palmLocal: a cup hotspot is taken by the
// palm).
void updatePalmPoints(hands::State& s)
{
    for(int hand = 0; hand < 2; hand++)
    {
        s.palmValid[hand] = handrig::usable(viewModel(handrig::modelName));
        if(!s.palmValid[hand])
        {
            continue;
        }
        const bool mirrored = hand == HAND_OFF;
        const glm::vec3 freeRot =
            basisAngles(anglesBasis(s.rot[hand]) * anglesBasis(weaponAngleOffsets(weapons::fistSlot(), mirrored)));
        const glm::vec3 d = palmAt(hand, s.pos[hand], freeRot, mirrored) - s.pos[hand];
        glm::vec3 f, r, u;
        hands::angleVectors(s.rot[hand], f, r, u);
        s.palmLocal[hand] = {glm::dot(d, f), glm::dot(d, r), glm::dot(d, u)};
    }
}

// Show Controller's preview of one controller: a Quest 3 (Touch Plus) controller, drawn translucent at the runtime's
// grip pose (OpenXR's grip/pose: the middle of the handle, x along it towards the thumb, y left, z towards the back of
// the hand), moved and turned by the preview's offsets (vr_show_controller_x .. _roll; the off hand's mirrored, or its
// own) to match the real one; its axes (red x, green y, blue z) at its point. Its shape, in centimetres of that frame:
// the handle (8 x 3.2 x 3.6), the head over its top (an oval face, level when the controller points ahead, 6.6 x 5.2,
// 1.4 thick) with the thumbstick on the thumb's side, and the trigger in front under the head. And, where the hand's own
// point differs from the pose the game tracks (the held weapon's Hand and Weapon Together), a yellow point joined to it.
// The preview's place: its point `c` (the middle of the handle) and its axes `b` (world).
void controllerPreviewPose(const hands::State& s, int hand, glm::vec3& c, glm::mat3& b)
{
    const bool off = hand == HAND_OFF;
    const bool own = off && vr_show_controller_off_own.value != 0.f;
    const float mirror = off && !own ? -1.f : 1.f;
    const glm::vec3 move = own ? glm::vec3{vr_show_controller_off_x.value, vr_show_controller_off_y.value, vr_show_controller_off_z.value}
                               : glm::vec3{vr_show_controller_x.value, vr_show_controller_y.value * mirror, vr_show_controller_z.value};
    const glm::vec3 turn = own ? glm::vec3{vr_show_controller_off_pitch.value, vr_show_controller_off_yaw.value,
                                     vr_show_controller_off_roll.value}
                               : glm::vec3{vr_show_controller_pitch.value, vr_show_controller_yaw.value * mirror,
                                     vr_show_controller_roll.value * mirror};
    const float cm = 0.01f * units::metresToUnits();
    const glm::mat3 grip = anglesBasis(s.gripRot[hand]);
    c = s.gripPos[hand] + grip * move * cm;
    b = grip * anglesBasis({-turn.x, turn.y, turn.z}); // pitch up
}

void drawControllerPreview(const hands::State& s, int hand)
{
    const bool off = hand == HAND_OFF;
    const float cm = 0.01f * units::metresToUnits();
    glm::vec3 c;
    glm::mat3 b;
    controllerPreviewPose(s, hand, c, b);
    const auto at = [&](float x, float y, float z) { return c + b * glm::vec3{x, y, z} * cm; };
    const glm::vec4 shell{0.85f, 0.9f, 1.f, 0.35f};
    const auto line = [&](const glm::vec3& p, const glm::vec3& q) { lines::line(p, q, 0.12f, shell, shell); };

    // The handle: a box along x.
    glm::vec3 corner[8];
    for(int i = 0; i < 8; i++)
    {
        corner[i] = at((i & 1) ? 3.2f : -4.8f, (i & 2) ? 1.6f : -1.6f, (i & 4) ? 1.7f : -1.9f);
    }
    constexpr int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for(const auto& e : edges)
    {
        line(corner[e[0]], corner[e[1]]);
    }
    // The head: its face level when the controller points ahead (the handle leans 30 degrees forward then): the face's
    // normal n and its forward a, in the grip frame.
    const glm::vec3 n = glm::normalize(glm::vec3{0.87f, 0.f, 0.5f}), a = glm::normalize(glm::vec3{0.5f, 0.f, -0.87f});
    const glm::vec3 left{0.f, 1.f, 0.f};
    const glm::vec3 face = glm::vec3{3.2f, 0.f, 0.f} + n * 1.0f + a * 1.2f;
    constexpr int sides = 20;
    const auto ellipse = [&](const glm::vec3& centre, float ra, float rl) {
        for(int i = 0; i < sides; i++)
        {
            const float a0 = 6.2831853f * i / sides, a1 = 6.2831853f * (i + 1) / sides;
            const glm::vec3 p = centre + a * (ra * std::cos(a0)) + left * (rl * std::sin(a0));
            const glm::vec3 q = centre + a * (ra * std::cos(a1)) + left * (rl * std::sin(a1));
            line(at(p.x, p.y, p.z), at(q.x, q.y, q.z));
        }
    };
    ellipse(face, 3.3f, 2.6f);
    ellipse(face - n * 1.4f, 3.3f, 2.6f);
    for(int i = 0; i < 4; i++)
    {
        const float ang = 6.2831853f * i / 4;
        const glm::vec3 p = face + a * (3.3f * std::cos(ang)) + left * (2.6f * std::sin(ang));
        const glm::vec3 q = p - n * 1.4f;
        line(at(p.x, p.y, p.z), at(q.x, q.y, q.z));
    }
    // The thumbstick, on the thumb's side (the right hand's left); the trigger, in front under the head.
    const glm::vec3 stick = face + n * 0.3f - a * 0.6f + left * (off ? -0.9f : 0.9f);
    ellipse(stick, 0.8f, 0.8f);
    const glm::vec3 trigger[3] = {glm::vec3{3.0f, 0.f, -2.0f}, glm::vec3{2.2f, 0.f, -3.2f}, glm::vec3{1.0f, 0.f, -3.0f}};
    line(at(trigger[0].x, 0.f, trigger[0].z), at(trigger[1].x, 0.f, trigger[1].z));
    line(at(trigger[1].x, 0.f, trigger[1].z), at(trigger[2].x, 0.f, trigger[2].z));

    const float axis = 6.f * cm;
    lines::line(c, c + b[0] * axis, 0.18f, {1.f, 0.2f, 0.2f, 0.8f}, {1.f, 0.2f, 0.2f, 0.8f});
    lines::line(c, c + b[1] * axis, 0.18f, {0.2f, 1.f, 0.2f, 0.8f}, {0.2f, 1.f, 0.2f, 0.8f});
    lines::line(c, c + b[2] * axis, 0.18f, {0.3f, 0.5f, 1.f, 0.8f}, {0.3f, 0.5f, 1.f, 0.8f});
    lines::point(c, 0.6f, {1.f, 1.f, 1.f, 0.8f});
    // The hand's own point (moved by the weapon's Hand and Weapon Together offset), joined to the tracked one's.
    if(glm::distance(s.pos[hand], s.controllerPos[hand]) > 0.05f)
    {
        lines::line(s.controllerPos[hand], s.pos[hand], 0.08f, {1.f, 0.9f, 0.3f, 0.6f}, {1.f, 0.9f, 0.3f, 0.6f});
        lines::point(s.pos[hand], 0.5f, {1.f, 0.9f, 0.3f, 0.8f});
    }
}

// Round 21, third pass, the Weapon Offsets page's tuning aids. vr_show_controller: each controller as tracked
// (drawControllerPreview). vr_show_controller_laser: for a held weapon, the controller's aim (white,
// from the controller along its calibrated aim: Gun Angle and the rest, before the weapon's offsets), the weapon's aim
// (red, from the muzzle where its shots go: the controller's aim with the weapon's Hand and Weapon Together turn, the
// two-handed aim, and its Shot Pitch and Yaw) and its barrel (green, from the muzzle along the drawn model's forward
// axis): the gun is turned right when green runs along red (or red turned onto the sights by Shot Pitch and Yaw).
// `lasers`: Show Controller Laser's lines (not while posing, which draws its own).
// vr_debug_hand_bones: each jointed hand as drawn this frame -- its bones (the palm's middle to each knuckle, then
// each finger's joints to its tip; thumb red, index orange, middle yellow, ring green, little finger blue), its joints
// (white), the spheres the grasp tests it as (vr_grasp.cpp) against what it holds (green touching, within a quarter
// of a unit; yellow near; red sunk in; grey nothing near), each touching or sunk one's nearest point on the held
// thing, and the palm's fit: its middle where the hand is (white) and where the grasp moved it (cyan), the way the
// palm faces (cyan), the grip channel (magenta: where a handle lies in the curled fingers).
void drawHandBones()
{
    static std::vector<glm::vec4> spheres;
    constexpr glm::vec4 fingerColour[handrig::FingerCount] = {
        {1.f, 0.25f, 0.25f, 1.f}, {1.f, 0.6f, 0.15f, 1.f}, {1.f, 1.f, 0.2f, 1.f}, {0.3f, 1.f, 0.3f, 1.f}, {0.3f, 0.6f, 1.f, 1.f}};
    const glm::vec4 white{1.f, 1.f, 1.f, 1.f}, cyan{0.2f, 1.f, 1.f, 1.f};
    for(int hand = 0; hand < 2; hand++)
    {
        const RigHand& rh = rigHands[hand];
        if(!rh.drawn)
        {
            continue;
        }
        const glm::mat4& m = rh.drawnToWorld;
        const auto world = [&](const glm::vec3& p) { return glm::vec3{m * glm::vec4{p, 1.f}}; };
        const float unit = glm::length(glm::vec3{m[0]}); // world units a rig unit
        const float width = 0.1f;

        // The bones and joints.
        const glm::vec3 palm = grasp::palmCentre();
        for(int f = 0; f < handrig::FingerCount; f++)
        {
            glm::vec3 p[4];
            grasp::fingerPoints(rh.pose, f, rh.pose.curl[f], p);
            lines::line(world(palm), world(p[0]), width * 0.6f, glm::vec4{0.8f, 0.8f, 0.8f, 0.7f}, fingerColour[f]);
            for(int j = 0; j < 3; j++)
            {
                lines::line(world(p[j]), world(p[j + 1]), width, fingerColour[f], fingerColour[f]);
            }
            for(int j = 0; j < 4; j++)
            {
                lines::point(world(p[j]), j == 3 ? 0.35f : 0.3f, j == 3 ? fingerColour[f] : white);
            }
        }

        // The palm's fit: where it is without the grasp's move and turn, where the grasp put it, the way it faces.
        const glm::vec3 unfitted{rh.rigToWorld * glm::vec4{palm, 1.f}};
        lines::point(unfitted, 0.5f, white);
        lines::point(world(palm), 0.5f, cyan);
        lines::line(unfitted, world(palm), width, white, cyan);
        lines::line(world(palm), world(palm + glm::vec3{0.f, 3.f, 0.f}), width, cyan, glm::vec4{0.2f, 1.f, 1.f, 0.f});
        glm::vec3 cp, cd;
        float radius;
        if(grasp::gripChannel(rh.pose, cp, cd, radius))
        {
            const glm::vec4 magenta{1.f, 0.3f, 1.f, 0.8f};
            lines::line(world(cp - cd * 4.f), world(cp + cd * 4.f), width, magenta, magenta);
        }

        // The spheres against what it holds.
        const grasp::Shape* shape = rh.held.ent ? grasp::shapeOf(*rh.held.ent, rh.held.frame) : nullptr;
        const glm::mat4 shapeToWorld = shape ? grasp::shapeToWorld(*rh.held.ent, rh.held.mirrored) : glm::mat4{1.f};
        grasp::posedSpheres(rh.pose, spheres);
        for(const glm::vec4& s : spheres)
        {
            const glm::vec3 c = world(glm::vec3{s});
            const float r = s.w * unit;
            glm::vec4 colour{0.6f, 0.6f, 0.6f, 0.3f};
            if(shape)
            {
                glm::vec3 at;
                bool in = false;
                const float d = grasp::surfaceDistance(*shape, shapeToWorld, c, r + 1.f, at, in);
                if(d >= 0.f)
                {
                    const float gap = in ? -d - r : d - r; // from the sphere's surface to the held thing's
                    colour = gap < -0.25f ? glm::vec4{1.f, 0.15f, 0.15f, 0.8f} : gap <= 0.25f ? glm::vec4{0.2f, 1.f, 0.3f, 0.8f}
                                                                                              : glm::vec4{1.f, 0.9f, 0.2f, 0.6f};
                    if(gap <= 0.25f)
                    {
                        lines::line(c, at, width * 0.5f, colour, colour);
                        lines::point(at, 0.25f, colour);
                    }
                }
            }
            lines::point(c, 2.f * r, glm::vec4{glm::vec3{colour}, colour.a * 0.3f});
        }
    }
}

void drawTuningAids(const hands::State& s, bool lasers)
{
    if(vr_show_controller.value)
    {
        for(int hand = 0; hand < 2; hand++)
        {
            drawControllerPreview(s, hand);
        }
    }
    if(lasers && vr_show_controller_laser.value)
    {
        const auto laser = [](const glm::vec3& from, const glm::vec3& dir, const glm::vec4& colour, float width) {
            const glm::vec3 farEnd = from + dir * 4096.f;
            const trace_t tr = worldtrace::world(from, farEnd);
            const glm::vec3 end = tr.fraction < 1.f ? worldtrace::endPos(tr) : from + dir * 1024.f;
            lines::line(from, end, width, colour, glm::vec4{glm::vec3{colour}, colour.a * 0.5f});
            if(tr.fraction < 1.f)
            {
                lines::point(end, std::fmax(1.f, glm::distance(from, end) * 0.006f), colour);
            }
        };
        for(int hand = 0; hand < 2; hand++)
        {
            const int slot = weapons::heldSlot(hand);
            if(slot < 0 || slot == weapons::fistSlot())
            {
                continue;
            }
            laser(s.controllerPos[hand], hands::forward(s.aimRot[hand]), {1.f, 1.f, 1.f, 0.55f}, 0.12f);
            if(s.muzzleValid[hand])
            {
                laser(s.muzzle[hand], hands::forward(weapons::shotAngles(s.rot[hand], slot, hand == HAND_OFF)),
                    {1.f, 0.25f, 0.2f, 0.7f}, 0.12f);
                const entity_t& e = entities.weapon[hand].ent;
                const glm::mat4 m = hotspotFrame(e, hand == HAND_OFF);
                laser(s.muzzle[hand], glm::normalize(glm::vec3{m[0]}), {0.3f, 1.f, 0.35f, 0.7f}, 0.08f);
            }
        }
    }
}

// What vr_body_collide's proxies are made from (vr_selfcollide.hpp): each hand as drawn (the jointed hand's grasp
// spheres; the six old models' as three spheres along the hand), and the weapon in it.
static selfcollide::Drawn selfCollideDrawn(const hands::State& s)
{
    static std::vector<glm::vec4> spheres[2];
    static std::vector<glm::vec4> rig;
    selfcollide::Drawn d;
    for(int hand = 0; hand < 2; hand++)
    {
        spheres[hand].clear();
        const RigHand& rh = rigHands[hand];
        if(rh.drawn)
        {
            grasp::posedSpheres(rh.pose, rig);
            const view::ViewEntity& he = entities.hand[hand][FingerBase];
            float mm[16];
            render::entityMatrix(he.ent, he.mirrored, ENTSCALE_DEFAULT, glm::vec3{0.f}, mm); // the rig as drawn
            const glm::mat4 m = toMat4(mm);
            const float unit = glm::length(glm::vec3{m[0]});
            for(const glm::vec4& sp : rig)
            {
                spheres[hand].push_back(glm::vec4{glm::vec3{m * glm::vec4{glm::vec3{sp}, 1.f}}, sp.w * unit});
            }
            d.hand[hand] = &spheres[hand];
        }
        else if(entities.hand[hand][FingerBase].visible && entities.hand[hand][FingerBase].ent.model)
        {
            const avatar::HandPose hp = drawnHand(s, hand);
            const float k = units::metresToUnits() * units::bodyScale();
            for(const float along : {0.035f, 0.065f, 0.095f})
            {
                spheres[hand].push_back(glm::vec4{hp.wrist + hp.forward * (along * k), 0.032f * k});
            }
            d.hand[hand] = &spheres[hand];
        }
        const view::ViewEntity& we = entities.weapon[hand];
        const int slot = weapons::slotForModel(we.ent.model);
        if(we.visible && we.ent.model && slot >= 0 && slot != weapons::fistSlot() && !isHandModel(we.ent.model))
        {
            d.weapon[hand] = &we.ent;
            d.mirrored[hand] = we.mirrored;
        }
    }
    return d;
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
        for(RigHand& rh : rigHands)
        {
            rh.drawn = false; // and what they held (an entity of a map that may be gone) forgotten
            rh.held = Held{};
            rh.grasp.valid = false;
        }
        s.muzzleValid[HAND_OFF] = s.muzzleValid[HAND_MAIN] = false;
        bodyblood::clear();
        return;
    }

    updateFingerFrames();

    // The weapon posing mode (vr_posing.cpp) moves the hands for the view only: the game's put back after it.
    const bool posingNow = posing::active();
    static hands::State unposed;
    if(posingNow)
    {
        unposed = s;
    }

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

    // Held out of the models they are pushed into (vr_model_collide): the weapons, hands and arms drawn moved (not while
    // posing: the floating weapon stays where it floats).
    if(!posingNow)
    {
        modelcollide::beginView(s);
        // And out of each other, the arms, the gadget and the body (vr_body_collide): drawn only, as above.
        selfcollide::beginView(s);
    }

    updatePalmPoints(s);
    wholeTurn[HAND_OFF] = s.wholeTurn[HAND_OFF] * s.calTurn[HAND_OFF];
    wholeTurn[HAND_MAIN] = s.wholeTurn[HAND_MAIN] * s.calTurn[HAND_MAIN];
    if(posingNow)
    {
        setupPosing(s);
    }
    else
    {
        setupWeapon(s, HAND_MAIN, precachedModel(cl.stats[STAT_WEAPON]), cl.stats[STAT_WEAPONFRAME]);
        setupWeapon(s, HAND_OFF, precachedModel(cl.stats[STAT_QVR_WEAPONMODEL2]),
            cl.stats[STAT_QVR_WEAPONFRAME2]);
        setupGhosts();

        setupHand(s, HAND_MAIN);
        setupHand(s, HAND_OFF);
        showHotspots();
    }
    for(int hand = 0; hand < 2; hand++)
    {
        updateFist(s, hand);
        updateGripFrame(s, hand);
    }
    drawTuningAids(s, !posingNow);
    if(!posingNow)
    {
        sightalign::viewFrame(s); // Align Sights to My Aim: its samples, Show Sight Line
    }
    bodycal::viewFrame(s); // Body Calibration: its samples
    held::drawCarryProbes();
    if(vr_debug_physics_shapes.value)
    {
        box3d::debugDraw();
    }
    ledges::debugDraw(); // vr_debug_ledges
    if(vr_debug_hand_bones.value)
    {
        drawHandBones();
    }
    // The guns not in a hand (holstered, lying round) show their screens (queued once a frame).
    static int idleTextFrame = -1;
    const bool idleTexts = idleTextFrame != host_framecount;
    idleTextFrame = host_framecount;
    setupHolsters(s, idleTexts);
    setupWorldWeapons(s, idleTexts);
    setupBody(s);
    setupPauldrons();
    setupGadget(s);
    flashlight::setupView(s, entities.flashlight);
    setupPouch(s);
    dripBlood(s);
    setupButton(HAND_MAIN);
    setupButton(HAND_OFF);
    if(!posingNow)
    {
        selfcollide::endView(s, selfCollideDrawn(s));
        const entity_t* const drawnWeapons[2]{&entities.weapon[HAND_OFF].ent, &entities.weapon[HAND_MAIN].ent};
        const bool drawnMirrored[2]{entities.weapon[HAND_OFF].mirrored, entities.weapon[HAND_MAIN].mirrored};
        modelcollide::endView(s, drawnWeapons, drawnMirrored); // the game reads the tracked hands and muzzles
    }
    for(int hand = 0; hand < 2; hand++)
    {
        s.pos[hand] -= knockPos[hand];
        s.rot[hand] -= knockAngles[hand];
        s.visualRot[hand] -= knockAngles[hand];
    }
    if(posingNow)
    {
        // The game's hands as they were; no muzzle (nothing is aimed or fired while posing) and no two-handed grip.
        s = unposed;
        for(int hand = 0; hand < 2; hand++)
        {
            s.muzzleValid[hand] = s.grip2HValid[hand] = false;
        }
    }
    else if(vrActive())
    {
        pressWeaponButtons(s);
    }

    // The ring of shadows fades the hands and weapons too (as the old engine did); the gadget
    // stays readable.
    if(vr_body_powerups.value && (cl.items & IT_INVISIBILITY))
    {
        forEachEntity([](view::ViewEntity& ve) {
            if(&ve != &entities.gadget && &ve != &entities.gadgetStrap[0] && &ve != &entities.gadgetStrap[1])
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

    // Wounds painted on the models (vr_wounds.cpp), the body and the hands posed.
    wounds::frame();
}

namespace qvr::view
{

void setGhost(int hand, qmodel_t* model, const glm::vec3& pos, const glm::vec3& rot, float alpha)
{
    if(hand < 0 || hand > 1)
    {
        return;
    }
    ghostRequests[hand] = {model, pos, rot, alpha, host_framecount};
}

float fingerCurl(int hand, int finger)
{
    if(hand < 0 || hand > 1 || finger < 0 || finger > 4)
    {
        return 0.f;
    }
    // As drawn without a grasp: the curl (frames 0..5) with a held weapon's finger tweak.
    return rigCurl(hand, FingerThumb + finger) / 5.f;
}

void resetCaches()
{
    std::fill(std::begin(viewModels), std::end(viewModels), ViewModelEntry{});
    clipSizes.clear();
    for(RigHand& rh : rigHands)
    {
        rh.drawn = false;
        rh.held = Held{};
        rh.grasp = Grasp{};
    }
    handrig::reset();
    grasp::reset();
}

namespace
{

// A model name as the engine knows it: progs/<name>.mdl (a body's .md5mesh is its .mdl's enhanced replacement).
[[nodiscard]] std::string modelPath(const char* arg)
{
    std::string name = arg;
    std::replace(name.begin(), name.end(), '\\', '/');
    if(name.find('/') == std::string::npos)
    {
        name = "progs/" + name;
    }
    const std::size_t dot = name.rfind('.');
    if(dot == std::string::npos || dot < name.rfind('/'))
    {
        name += ".mdl";
    }
    else if(!q_strcasecmp(name.c_str() + dot, ".md5mesh") || !q_strcasecmp(name.c_str() + dot, ".md5anim"))
    {
        name = name.substr(0, dot) + ".mdl";
    }
    return name;
}

struct ReloadMatch
{
    std::vector<std::string> names; // empty: the editable ones
    std::vector<std::string> done;
};

qboolean reloadMatch(const char* name, void* ctx)
{
    auto& m = *static_cast<ReloadMatch*>(ctx);
    bool yes = false;
    if(m.names.empty())
    {
        const char* base = strrchr(name, '/');
        base = base ? base + 1 : name;
        // The models the Blender add-on edits (docs/vr-port/MODELS_IN_BLENDER.md).
        static constexpr const char* prefixes[] = {"v_", "vrbody", "vrgadget", "vrflashlight", "vrpauldron", "legholster",
            "vr_shell", "wpnbutton", "hand_base", "finger_"};
        yes = !q_strncasecmp(name, "progs/", 6) && std::any_of(std::begin(prefixes), std::end(prefixes), [&](const char* p) {
            return !q_strncasecmp(base, p, strlen(p));
        });
    }
    else
    {
        yes = std::any_of(m.names.begin(), m.names.end(), [&](const std::string& n) { return !q_strcasecmp(n.c_str(), name); });
    }
    if(yes)
    {
        m.done.emplace_back(name);
    }
    return yes;
}

} // namespace

void modelReload_f()
{
    ReloadMatch m;
    for(int i = 1; i < Cmd_Argc(); i++)
    {
        m.names.push_back(modelPath(Cmd_Argv(i)));
    }
    Mod_ReloadAliasModels(reloadMatch, &m);
    for(const std::string& n : m.names)
    {
        if(std::find(m.done.begin(), m.done.end(), n) == m.done.end())
        {
            Con_Printf("vr_model_reload: %s isn't loaded (it is read when it's first drawn)\n", n.c_str());
        }
    }
    if(m.done.empty())
    {
        return;
    }
    // What was worked out from the old files: forgotten, worked out again from the new ones as they are drawn.
    anchor::onGameDirChanged(); // the strip orders (vr_anchor.cpp): by model, and a reloaded model keeps its slot
    grasp::reset();
    modelcollide::reset();
    selfcollide::reset();
    weapons::resetCaches();
    avatar::reset();
    flashlight::onModelsReloaded(std::find(m.done.begin(), m.done.end(), "progs/vrflashlight.mdl") != m.done.end());
    // Each model named, or (all of them) one line, and the body's check.
    const bool each = !m.names.empty();
    bool body = false;
    std::string list;
    for(const std::string& n : m.done)
    {
        qmodel_t* model = Mod_ForName(n.c_str(), false);
        if(!model || model->type != mod_alias)
        {
            continue;
        }
        const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
        const bool isBody = strstr(n.c_str(), "vrbody") != nullptr;
        body = body || isBody;
        list += (list.empty() ? "" : ", ") + n.substr(n.rfind('/') + 1);
        if(hdr->poseverttype == aliashdr_t::PV_QUAKE1)
        {
            if(each)
            {
                Con_Printf("vr_model_reload: %s: %d vertices, %d triangles, %d frames\n", n.c_str(), hdr->numverts,
                    hdr->numtris, hdr->numframes);
            }
        }
        else
        {
            const bool usable = !isBody || avatar::usable(model);
            if(each || !usable)
            {
                Con_Printf("vr_model_reload: %s: its enhanced replacement (%d vertices, %d bones)%s\n", n.c_str(),
                    hdr->numverts_vbo, hdr->numbones, usable ? "" : ": NOT usable as the body (see above)");
            }
        }
    }
    if(!each)
    {
        Con_Printf("vr_model_reload: %d models read again: %s\n", static_cast<int>(m.done.size()), list.c_str());
    }
    if(body)
    {
        bodyblood::clear(); // the wounds painted on the old mesh
    }
}

void woundTargets(entity_t* out[3])
{
    out[0] = entities.body.visible && entities.body.ent.model ? &entities.body.ent : nullptr;
    for(int hand = 0; hand < 2; hand++)
    {
        out[1 + hand] = rigHands[hand].drawn && entities.hand[hand][FingerBase].ent.model ? &entities.hand[hand][FingerBase].ent : nullptr;
    }
}

int handBonePoses(const entity_t* e, const float** matrices)
{
    for(int hand = 0; hand < 2; hand++)
    {
        if(rigHands[hand].drawn && e == &entities.hand[hand][FingerBase].ent)
        {
            if(matrices)
            {
                *matrices = rigHands[hand].skin.data();
            }
            return handrig::data::numJoints;
        }
    }
    return 0;
}


// vr_grasp_dump <main|off> <file>: the hand as drawn (its triangles, in its model space: hand_base.mdl's) and what it
// holds, in the same space, as an .obj ("o hand", "o held") in the game folder, to look at from any side
// (Misc/quakevr tools, round 21's composites).
void graspBench_f()
{
    const int runs = Cmd_Argc() > 1 ? CLAMP(1, Q_atoi(Cmd_Argv(1)), 100000) : 1000;
    for(int hand = 0; hand < 2; hand++)
    {
        const RigHand& rh = rigHands[hand];
        if(!rh.drawn || !rh.held.ent || !rh.held.ent->model)
        {
            continue;
        }
        const int frame = rh.held.frame >= 0 ? rh.held.frame : rh.held.ent->frame;
        const grasp::Shape* shape = grasp::shapeOf(*rh.held.ent, frame);
        if(!shape)
        {
            continue;
        }
        const glm::mat4 inRig = glm::inverse(rh.solveRig) * grasp::shapeToWorld(*rh.held.ent, rh.held.mirrored);
        const grasp::Settings settings = graspSettings(rh.held, rh.rigUnit);
        std::vector<double> us;
        grasp::Solution s;
        for(int i = 0; i < runs; i++)
        {
            const auto t0 = std::chrono::steady_clock::now();
            grasp::solve(rh.pose, *shape, inRig, settings, rh.grasp.valid ? &rh.grasp.solution : nullptr, s);
            us.push_back(std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() * 1e6);
        }
        std::sort(us.begin(), us.end());
        // Solved afresh (as when first taken) and again with the solve before (as each frame it moves in the hand).
        grasp::Solution first;
        const auto t0 = std::chrono::steady_clock::now();
        grasp::solve(rh.pose, *shape, inRig, settings, nullptr, first);
        const double firstUs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() * 1e6;

        Con_Printf("vr_grasp_bench: %s hand, %s (%d triangles): afresh %.1f us (%d probes, %d places); again, %d times: min "
                   "%.1f us, median %.1f us, max %.1f us (%d probes)\n",
            hand == HAND_MAIN ? "main" : "off", rh.held.ent->model->name, s.triangles, firstUs, first.probes, first.places, runs,
            us.front(), us[us.size() / 2], us.back(), s.probes);
        // Where the fingers stopped (afresh | again), to compare solvers.
        std::string stops;
        for(const grasp::Solution* sol : {&first, &s})
        {
            for(int f = 0; f < handrig::FingerCount; f++)
            {
                char b[64];
                const grasp::FingerStop& st = sol->finger[f];
                q_snprintf(b, sizeof(b), " %.2f/%.2f/%.2f%s%s%s", st.stop[0], st.stop[1], st.stop[2], st.met ? "m" : "",
                    st.startsInside ? "i" : "", st.fromClosed ? "c" : "");
                stops += b;
                q_snprintf(b, sizeof(b), "(%d)", sol->fingerProbes[f]);
                stops += b;
            }
            stops += sol == &first ? " |" : "";
        }
        Con_Printf("vr_grasp_bench: stops%s\n", stops.c_str());
    }
}

void graspDump_f()
{
    const int hand = Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "off") ? HAND_OFF : HAND_MAIN;
    const RigHand& rh = rigHands[hand];
    if(!rh.drawn)
    {
        Con_Printf("vr_grasp_dump: the %s hand's jointed hand is not drawn\n", hand == HAND_MAIN ? "main" : "off");
        return;
    }
    char path[MAX_OSPATH];
    q_snprintf(path, sizeof(path), "%s/%s", com_gamedir, Cmd_Argc() >= 3 ? Cmd_Argv(2) : "grasp_dump.obj");
    FILE* f = fopen(path, "w");
    if(!f)
    {
        Con_Printf("vr_grasp_dump: can't write %s\n", path);
        return;
    }
    // The hand's mesh (handrig::rig(): the file's, split at the skin's seams), posed.
    int base = 1;
    std::vector<glm::vec3> posed;
    const auto mesh = [&](const RigHand& which, const glm::mat4& toThis, const char* name) {
        handrig::vertices(which.posed, posed);
        fprintf(f, "o %s\n", name);
        for(const glm::vec3& v : posed)
        {
            const glm::vec3 p{toThis * glm::vec4{drawnInRig(which, v), 1.f}};
            fprintf(f, "v %f %f %f\n", p.x, p.y, p.z);
        }
        for(const auto& tri : handrig::rig().triangles)
        {
            fprintf(f, "f %d %d %d\n", base + tri[0], base + tri[1], base + tri[2]);
        }
        base += static_cast<int>(posed.size());
    };
    mesh(rh, glm::mat4{1.f}, "hand");

    // The other hand, when this one cups it.
    const RigHand& otherHand = rigHands[1 - hand];
    if(rh.held.cup && otherHand.drawn)
    {
        mesh(otherHand, glm::inverse(rh.rigToWorld) * otherHand.rigToWorld, "other");
    }

    // The spheres the solver tests the hand as, each an octahedron.
    std::vector<glm::vec4> spheres;
    grasp::posedSpheres(rh.pose, spheres);
    fprintf(f, "o spheres\n");
    for(const glm::vec4& s : spheres)
    {
        const glm::vec3 c{s};
        for(const glm::vec3 d : {glm::vec3{1, 0, 0}, glm::vec3{-1, 0, 0}, glm::vec3{0, 1, 0}, glm::vec3{0, -1, 0}, glm::vec3{0, 0, 1}, glm::vec3{0, 0, -1}})
        {
            const glm::vec3 p = drawnInRig(rh, c + d * s.w);
            fprintf(f, "v %f %f %f\n", p.x, p.y, p.z);
        }
        constexpr int faces[8][3] = {{0, 2, 4}, {2, 1, 4}, {1, 3, 4}, {3, 0, 4}, {2, 0, 5}, {1, 2, 5}, {3, 1, 5}, {0, 3, 5}};
        for(const auto& t : faces)
        {
            fprintf(f, "f %d %d %d\n", base + t[0], base + t[1], base + t[2]);
        }
        base += 6;
    }
    std::vector<grasp::Triangle> tris;
    if(rh.held.ent && rh.held.ent->model && grasp::worldTriangles(*rh.held.ent, rh.held.mirrored, rh.held.frame, tris))
    {
        const glm::mat4 toRig = glm::inverse(rh.rigToWorld);
        fprintf(f, "o held\n");
        for(const grasp::Triangle& t : tris)
        {
            for(const glm::vec3& p : t.p)
            {
                const glm::vec3 q{toRig * glm::vec4{p, 1.f}};
                fprintf(f, "v %f %f %f\n", q.x, q.y, q.z);
            }
        }
        for(size_t i = 0; i < tris.size(); i++)
        {
            fprintf(f, "f %d %d %d\n", base + static_cast<int>(3 * i), base + static_cast<int>(3 * i + 1), base + static_cast<int>(3 * i + 2));
        }
    }
    fclose(f);
    Con_Printf("vr_grasp_dump: %s\n", path);
}

bool hotspotAt(int hand, const glm::vec3& p, glm::vec3& out)
{
    const ViewEntity& ve = entities.weapon[hand];
    const int slot = weapons::slotForModel(ve.ent.model);
    if(!ve.ent.model || slot < 0 || slot == weapons::fistSlot())
    {
        return false;
    }
    out = glm::vec3{glm::inverse(hotspotFrame(ve.ent, ve.mirrored)) * glm::vec4{p, 1.f}};
    return true;
}

WeaponHotspot weaponHotspot(int hand, int index)
{
    if(hand < 0 || hand > 1 || index < 0 || index >= weapons::maxHotspots || !entities.weapon[hand].visible)
    {
        return {};
    }
    const WorldHotspot& w = worldHotspots[hand][index];
    const glm::vec3 pos = w.type == weapons::HotspotType::Blade ? glm::mix(w.pos, w.end, 0.5f) : w.pos;
    return {static_cast<int>(w.type), pos, w.bias, w.share};
}

// vr_hotspots_check: for every slot, the weapon drawn in either hand at a few poses: where its old two-handed grip
// (the retired foregrip keys, mirrored as the old code did) and its grip hotspot are (they should match), and how far
// the drawn hand moved (it was at the weapon's hand anchor, round 20; now where the controller is: the weapon's
// origin).
void hotspotHere_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_weapon_hotspot_here <1..4> [<type 0..3>] [main|off]\n");
        return;
    }
    const int index = CLAMP(1, Q_atoi(Cmd_Argv(1)), weapons::maxHotspots) - 1;
    const int type = Cmd_Argc() >= 3 ? CLAMP(0, Q_atoi(Cmd_Argv(2)), 3) : 1;
    const int hand = Cmd_Argc() >= 4 && !q_strcasecmp(Cmd_Argv(3), "off") ? HAND_OFF : HAND_MAIN;
    const int slot = weapons::slotForModel(entities.weapon[hand].ent.model);
    glm::vec3 p;
    // A cup is where the other hand's palm is (round 21, third pass).
    const glm::vec3 at = type == static_cast<int>(weapons::HotspotType::Cup) ? hands::palmPoint(hands::current(), 1 - hand)
                                                                             : hands::current().pos[1 - hand];
    if(slot < 0 || !hotspotAt(hand, at, p))
    {
        Con_Printf("vr_weapon_hotspot_here: no weapon in the %s hand\n", hand == HAND_MAIN ? "main" : "off");
        return;
    }
    weapons::Hotspot h = weapons::hotspot(slot, index);
    h.type = static_cast<weapons::HotspotType>(type);
    h.pos = type ? p : glm::vec3{0.f};
    weapons::setHotspot(slot, index, h);
    Con_Printf("vr_weapon_hotspot_here: %s hotspot %d: type %d at %.2f %.2f %.2f\n", entities.weapon[hand].ent.model->name, index + 1,
        type, p.x, p.y, p.z);
}

void hotspotsCheck_f()
{
    float worstGrip = 0.f, worstHand = 0.f, worstMuzzle = 0.f;
    for(int slot = 0; slot < weapons::numSlots; slot++)
    {
        const cvar_t* id = weapons::cvar(slot, Key::ID);
        qmodel_t* model = id && id->string[0] ? Mod_ForName(id->string, false) : nullptr;
        if(!model || model->type != mod_alias || slot == weapons::fistSlot())
        {
            continue;
        }
        float grip = -1.f, hand = 0.f, muzzle = 0.f;
        for(int pose = 0; pose < 6; pose++)
        {
            const bool mirrored = pose & 1;
            ViewEntity ve;
            ve.ent.model = model;
            ve.ent.scale = ENTSCALE_DEFAULT;
            ve.mirrored = mirrored;
            const float a = static_cast<float>(pose);
            ve.ent.origin[0] = 100.f + 7.f * a;
            ve.ent.origin[1] = -40.f + 3.f * a;
            ve.ent.origin[2] = 20.f - 5.f * a;
            ve.ent.angles[0] = -30.f + 17.f * a;
            ve.ent.angles[1] = 40.f * a;
            ve.ent.angles[2] = 25.f - 11.f * a;
            const glm::vec3 origin{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]};
            // The old drawn hand: the hand anchor and offset.
            const glm::vec3 oldHand = anchorPosition(ve, static_cast<int>(weapons::value(slot, Key::HandAnchorVertex)),
                weapons::vec(slot, Key::HandOffsetX, Key::HandOffsetY, Key::HandOffsetZ));
            hand = std::max(hand, glm::distance(oldHand, origin));
            // The muzzle: round 20 moved the weapon by its GunOffset (world axes, y mirrored); retired, now not.
            const int muzzleVertex = static_cast<int>(weapons::value(slot, Key::MuzzleAnchorVertex));
            const glm::vec3 muzzleOffset = weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ);
            glm::vec3 gunOffset = weapons::vec(slot, Key::GunOffsetX, Key::GunOffsetY, Key::GunOffsetZ) * weapons::offsetScale();
            if(mirrored)
            {
                gunOffset.y = -gunOffset.y;
            }
            ViewEntity shifted = ve;
            for(int i = 0; i < 3; i++)
            {
                shifted.ent.origin[i] += gunOffset[i];
            }
            muzzle = std::max(muzzle,
                glm::distance(anchorPosition(shifted, muzzleVertex, muzzleOffset), anchorPosition(ve, muzzleVertex, muzzleOffset)));
            if(weapons::value(slot, Key::TwoHDisplayMode) != 1.f)
            {
                continue;
            }
            ViewEntity helped = ve;
            helped.mirrored = !mirrored;
            const glm::vec3 old = anchorPosition(helped, static_cast<int>(weapons::value(slot, Key::TwoHHandAnchorVertex)),
                weapons::vec(slot, Key::TwoHFixedOffsetX, Key::TwoHFixedOffsetY, Key::TwoHFixedOffsetZ));
            const weapons::Hotspot h = weapons::hotspot(slot, 0);
            const glm::vec3 now{hotspotFrame(ve.ent, mirrored) * glm::vec4{h.pos, 1.f}};
            grip = std::max(grip, glm::distance(old, now));
        }
        worstGrip = std::max(worstGrip, grip);
        worstHand = std::max(worstHand, hand);
        worstMuzzle = std::max(worstMuzzle, muzzle);
        Con_Printf("slot %2d %-22s foregrip: %s   muzzle %.4f   the hand moved %.2f units (%.2f cm)\n", slot, id->string,
            grip < 0.f ? "none" : va("old and hotspot %.4f units apart", grip), muzzle, hand,
            hand * 100.f / units::metresToUnits());
    }
    Con_Printf("vr_hotspots_check: the most a foregrip moved %.4f units, a muzzle %.4f, the hand %.2f units\n", worstGrip,
        worstMuzzle, worstHand);
}

void hotspotsLegacy_f()
{
    for(int slot = 0; slot < weapons::numSlots; slot++)
    {
        weapons::Hotspot h[2];
        int count = 0;
        if(!legacyHotspots(slot, true, h, count))
        {
            continue;
        }
        for(int i = 0; i < count; i++)
        {
            Con_Printf("QVR_WEAPON_DEFAULT(%d, Hotspot%dType, \"%d\")\n", slot, i + 1, static_cast<int>(h[i].type));
            Con_Printf("QVR_WEAPON_DEFAULT(%d, Hotspot%dX, \"%.4f\")\n", slot, i + 1, h[i].pos.x);
            Con_Printf("QVR_WEAPON_DEFAULT(%d, Hotspot%dY, \"%.4f\")\n", slot, i + 1, h[i].pos.y);
            Con_Printf("QVR_WEAPON_DEFAULT(%d, Hotspot%dZ, \"%.4f\")\n", slot, i + 1, h[i].pos.z);
        }
    }
}

// vr_dumpview: lists the VR view entities.
void posingCheck(bool weaponTarget, int weaponHand, const glm::mat4& rigInWeapon, const glm::vec3* palmInWeapon,
    const glm::mat4& rigWorld)
{
    const int hand = weaponTarget ? weaponHand : 1 - weaponHand;
    const ViewEntity& weapon = entities.weapon[weaponHand];
    const RigHand& rh = rigHands[hand];
    if(!weapon.visible || !rh.drawn || posing::active())
    {
        Con_Printf("vr_pose_check: hold the weapon in the %s hand (out of the posing mode)\n", weaponHand == HAND_MAIN ? "main" : "off");
        return;
    }
    const glm::mat4 hs = hotspotFrame(weapon.ent, weapon.mirrored);
    const glm::mat4 predicted = hs * rigInWeapon;
    const glm::mat4& actual = rh.rigToWorld;
    const auto turnOf = [](const glm::mat4& m) {
        return glm::mat3{glm::normalize(glm::vec3{m[0]}), glm::normalize(glm::vec3{m[1]}), glm::normalize(glm::vec3{m[2]})};
    };
    const glm::quat q = glm::quat_cast(glm::transpose(turnOf(predicted)) * turnOf(actual));
    const float angle = glm::degrees(2.f * std::atan2(glm::length(glm::vec3{q.x, q.y, q.z}), std::fabs(q.w)));
    const float moved = glm::distance(glm::vec3{predicted[3]}, glm::vec3{actual[3]});
    const glm::vec3 palm{actual * glm::vec4{drawnInRig(rh, grasp::palmCentre()), 1.f}};
    const float palmMoved = palmInWeapon ? glm::distance(glm::vec3{hs * glm::vec4{*palmInWeapon, 1.f}}, palm) : -1.f;
    // The weapon where the pose puts it from the hand: its muzzle (its far end) against where it is.
    const hands::State& s = hands::current();
    float muzzleMoved = -1.f;
    if(s.muzzleValid[weaponHand])
    {
        const glm::mat4 weaponFromHand = actual * glm::inverse(rigInWeapon);
        const glm::vec3 muzzleInWeapon{glm::inverse(hs) * glm::vec4{s.muzzle[weaponHand], 1.f}};
        muzzleMoved = glm::distance(glm::vec3{weaponFromHand * glm::vec4{muzzleInWeapon, 1.f}}, s.muzzle[weaponHand]);
    }
    Con_Printf("pose check (%s, %s hand%s): hand %.4f units %.4f deg from the pose, muzzle %.4f units, drawn palm %s\n",
        weaponTarget ? "the weapon" : "a hotspot", hand == HAND_MAIN ? "main" : "off",
        weaponTarget || twohand::helping(hand) ? "" : ", NOT holding a hotspot", moved, angle, muzzleMoved,
        palmInWeapon ? va("%.4f units", palmMoved) : "not compared (the hand wasn't seen solved after the set)");
    // Where the weapon's hand is drawn in the world against where it was while posing (the same only with the controller,
    // and the player, where they were: the weapon's angle offsets are Euler angles added to the hand's, so the hand and
    // weapon together sit a little differently on a tilted controller).
    if(!weaponTarget)
    {
        return;
    }
    const glm::quat w = glm::quat_cast(glm::transpose(turnOf(rigWorld)) * turnOf(actual));
    Con_Printf("  in the world: %.4f units %.4f deg from where it was drawn while posing\n",
        glm::distance(glm::vec3{rigWorld[3]}, glm::vec3{actual[3]}),
        glm::degrees(2.f * std::atan2(glm::length(glm::vec3{w.x, w.y, w.z}), std::fabs(w.w))));
}

void dumpView_f()
{
    const hands::State& s = hands::current();
    Con_Printf("hands valid %d  player (%.1f %.1f %.1f)  main (%.1f %.1f %.1f)  off (%.1f %.1f %.1f)%s\n", s.valid,
        s.playerOrigin.x, s.playerOrigin.y, s.playerOrigin.z, s.pos[1].x, s.pos[1].y, s.pos[1].z, s.pos[0].x, s.pos[0].y,
        s.pos[0].z, twohand::helping(HAND_OFF) ? ", the off hand helping" : "");

    for(int h = 0; h < 2; h++)
    {
        Con_Printf("%s hand at (%.4f %.4f %.4f) angles (%.4f %.4f %.4f), controller at (%.4f %.4f %.4f) aim (%.4f %.4f %.4f)\n",
            h == HAND_MAIN ? "main" : "off", s.pos[h].x, s.pos[h].y, s.pos[h].z, s.rot[h].x, s.rot[h].y, s.rot[h].z,
            s.controllerPos[h].x, s.controllerPos[h].y, s.controllerPos[h].z, s.aimRot[h].x, s.aimRot[h].y, s.aimRot[h].z);
        Con_Printf("%s grip (Show Controller) at (%.4f %.4f %.4f) angles (%.4f %.4f %.4f), %.2f cm from the controller\n",
            h == HAND_MAIN ? "main" : "off", s.gripPos[h].x, s.gripPos[h].y, s.gripPos[h].z, s.gripRot[h].x, s.gripRot[h].y,
            s.gripRot[h].z, glm::distance(s.gripPos[h], s.controllerPos[h]) / (0.01f * units::metresToUnits()));
        if(rigHands[h].drawn)
        {
            const glm::vec3 palm{rigHands[h].rigToWorld * glm::vec4{drawnInRig(rigHands[h], grasp::palmCentre()), 1.f}};
            Con_Printf("%s drawn palm at (%.4f %.4f %.4f)\n", h == HAND_MAIN ? "main" : "off", palm.x, palm.y, palm.z);
        }
        // The grab test's fist, and the open hand, in the hand's frame (cm from its point: forward, left, up): how far
        // each reaches along each axis.
        std::vector<glm::vec4> fist, open;
        if(emptyHandSpheres(s, h, true, fist) && emptyHandSpheres(s, h, false, open))
        {
            const float cm = 100.f / units::metresToUnits();
            for(const auto& [name, spheres] : {std::pair{"fist", &fist}, std::pair{"open hand", &open}})
            {
                glm::vec3 lo{1e9f}, hi{-1e9f};
                for(const glm::vec4& sp : *spheres)
                {
                    lo = glm::min(lo, glm::vec3{sp} - sp.w);
                    hi = glm::max(hi, glm::vec3{sp} + sp.w);
                }
                Con_Printf("%s %s: %d spheres, forward %.1f..%.1f cm, left %.1f..%.1f, up %.1f..%.1f of the hand's point\n",
                    h == HAND_MAIN ? "main" : "off", name, static_cast<int>(spheres->size()), lo.x * cm, hi.x * cm, lo.y * cm,
                    hi.y * cm, lo.z * cm, hi.z * cm);
            }
            const glm::vec3 palm{glm::transpose(held::axesFromAngles(&s.rot[h][0], true)) *
                                 (glm::vec3{rigPlacement(h, s.pos[h], basisAngles(anglesBasis(s.rot[h]) *
                                     anglesBasis(weaponAngleOffsets(weapons::fistSlot(), h == HAND_OFF))), h == HAND_OFF, nullptr) *
                                     glm::vec4{grasp::palmCentre(), 1.f}} - s.pos[h])};
            Con_Printf("%s palm's middle: forward %.1f cm, left %.1f, up %.1f of the hand's point\n", h == HAND_MAIN ? "main" : "off",
                palm.x * cm, palm.y * cm, palm.z * cm);
        }
        if(s.grip2HValid[h])
        {
            Con_Printf("%s weapon foregrip (%.1f %.1f %.1f)\n", h == HAND_MAIN ? "main" : "off", s.grip2H[h].x,
                s.grip2H[h].y, s.grip2H[h].z);
        }
        if(s.muzzleValid[h])
        {
            Con_Printf("%s weapon muzzle (%.4f %.4f %.4f), %.1f units from the hand%s\n", h == HAND_MAIN ? "main" : "off",
                s.muzzle[h].x, s.muzzle[h].y, s.muzzle[h].z, glm::distance(s.muzzle[h], s.pos[h]),
                twohand::bladeGrip(h) ? ", held two-handed by its blade" : "");
        }
        const HandInput& in = tracking().input.hands[h];
        Con_Printf("%s hand: trigger %.2f grip %.2f thumb %d, curls %.1f %.1f %.1f %.1f %.1f\n",
            h == HAND_MAIN ? "main" : "off", in.triggerValue, in.gripValue, in.thumbTouch, fingerFrames[h][FingerThumb],
            fingerFrames[h][FingerIndex], fingerFrames[h][FingerMiddle], fingerFrames[h][FingerRing],
            fingerFrames[h][FingerPinky]);
        Con_Printf("  finger tweaks %+.2f %+.2f %+.2f %+.2f %+.2f (frame/open:", fingerBias[h][FingerThumb],
            fingerBias[h][FingerIndex], fingerBias[h][FingerMiddle], fingerBias[h][FingerRing], fingerBias[h][FingerPinky]);
        for(int f = FingerThumb; f < FingerCount; f++)
        {
            const FingerPose p = fingerPose(h, f);
            Con_Printf(" %d/%.2f", p.frame, p.open);
        }
        Con_Printf(")\n");
    }

    if(body::pouchEnabled())
    {
        const glm::vec3 pouch = body::pouchPosition(s);
        Con_Printf("grenade pouch at (%.2f %.2f %.2f), reach %.1f: main hand %.1f units off (hotspot %d), off hand %.1f (%d)\n",
            pouch.x, pouch.y, pouch.z, body::pouchReach(), glm::distance(s.pos[HAND_MAIN], pouch), s.hotspot[HAND_MAIN],
            glm::distance(s.pos[HAND_OFF], pouch), s.hotspot[HAND_OFF]);
    }

    int i = 0;
    forEachEntity([&](ViewEntity& ve) {
        const entity_t& e = ve.ent;
        Con_Printf("%2d %-24s vis %d mir %d frame %d org (%.3f %.3f %.3f) ang (%.0f %.0f %.0f)\n", i++,
            e.model ? e.model->name : "-", ve.visible, ve.mirrored, e.frame, e.origin[0], e.origin[1],
            e.origin[2], e.angles[0], e.angles[1], e.angles[2]);
    });
}


bool previewGripMove(const hands::State& s, int hand, glm::vec3& worldMove)
{
    glm::vec3 cp, cd;
    float radius;
    if(!s.valid || !handrig::usable(viewModel(handrig::modelName)) || !rigHands[hand].drawn ||
        !grasp::gripChannel(rigHands[hand].pose, cp, cd, radius))
    {
        return false;
    }
    // The empty hand as it is drawn on the calibrated controller (with the fist's own Hand and Weapon Together offset),
    // whatever the hand holds now.
    const bool mirrored = hand == HAND_OFF;
    const int fist = weapons::fistSlot();
    glm::vec3 pos = s.calibratedPos[hand], rot = s.calibratedRot[hand];
    glm::mat3 turn{1.f};
    hands::wholeOffset(fist, hand, pos, rot, turn);
    const glm::vec3 handRot = basisAngles(anglesBasis(rot) * anglesBasis(weaponAngleOffsets(fist, mirrored)));
    const glm::vec3 channel{rigPlacement(hand, pos, handRot, mirrored, nullptr) * glm::vec4{cp, 1.f}};
    glm::vec3 c;
    glm::mat3 b;
    controllerPreviewPose(s, hand, c, b);
    worldMove = c - channel;
    return true;
}

} // namespace qvr::view
