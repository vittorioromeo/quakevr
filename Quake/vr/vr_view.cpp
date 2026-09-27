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
#include "vr_handrig.hpp"
#include "vr_grasp.hpp"
#include "vr_held.hpp"
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
    if(finger == FingerBase || twohand::helping(hand))
    {
        return 0.f;
    }
    const int slot = weapons::heldSlot(hand);
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
    for(int hand = 0; hand < 2; hand++)
    {
        for(int finger = 0; finger < FingerCount; finger++)
        {
            const bool holdsOther = twohand::helping(hand) || held::heldEntity(hand) != 0 || flashlight::holds(hand);
            approach(fingerFrames[hand][finger], targetCurl(input.hands[hand], finger, holdsOther) * 5.f);
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
    f(entities.flashlight);
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
[[nodiscard]] glm::vec3 attachedTurn(const glm::vec3& rot, int slot, bool mirrored, const glm::vec3& handAt0)
{
    const glm::mat3 w = anglesBasis(weaponTurn(rot, slot, mirrored));
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
void showHotspots()
{
    if(!vr_show_weapon_hotspots.value)
    {
        return;
    }
    const int edited = CLAMP(1, static_cast<int>(vr_weapon_hotspot.value), weapons::maxHotspots) - 1;
    for(int hand = 0; hand < 2; hand++)
    {
        if(!entities.weapon[hand].visible)
        {
            continue;
        }
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


    const glm::vec3 o = weaponAngleOffsets(slot, mirrored);
    const glm::vec3& rot = held.rot;

    place(ve, model, held.pos, {-rot.x + o.x, rot.y + o.y, rot.z + o.z}, frame,
        mirrored);

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
        w = WorldHotspot{h.type, glm::vec3{0.f}, glm::vec3{0.f}, h.bias, 0.f, h.angles, h.style};
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
                const WorldHotspot& w = worldHotspots[hand][i];
                const float d = glm::distance(s.pos[1 - hand], w.pos) - w.bias;
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

    // Morphing from the other ammo's model (vr_weapon_morph_time): that one too, where it would be held, going out.
    view::ViewEntity& old = entities.weaponMorph[hand];
    const float t = updateMorph(hand, model);
    const Morph& m = morphs[hand];
    const int oldSlot = weapons::slotForModel(m.from);
    ve.morph = 0.f;
    old.morph = 0.f;
    if(m.from && oldSlot >= 0 && ve.visible)
    {
        const glm::vec3 oo = weaponAngleOffsets(oldSlot, mirrored);
        place(old, m.from, held.pos, {-rot.x + oo.x, rot.y + oo.y, rot.z + oo.z},
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

    switch(finger)
    {
        case FingerThumb:
            return result + glm::vec3{vr_finger_thumb_x.value, vr_finger_thumb_y.value,
                                vr_finger_thumb_z.value} +
                   (holding ? weapons::vec(slot, Key::FingerThumbX, Key::FingerThumbY, Key::FingerThumbZ) : glm::vec3{0.f});
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
};

// A hand's grasp: what it was solved against (the held thing's model, pose and place in the hand's rig space, the
// fingers' places), and the fingers' stops.
struct Grasp
{
    bool valid{false};
    const qmodel_t* model{nullptr};
    int frame{-1};
    glm::mat4 inRig{1.f};
    glm::vec3 shift[handrig::FingerCount]{};
    grasp::Settings settings;
    grasp::Solution solution;
    float other[handrig::FingerCount * handrig::jointsPerFinger]{}; // a cup's: the other hand's joints it was solved for
    glm::mat4 otherInRig{1.f};                                      // and where that hand was
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

void printGrasp(int hand, const Grasp& g, float rigUnit)
{
    Con_Printf("grasp (%.2f s): %s hand, %s: %d triangles within reach; the palm moved %.2f cm (%.1f %.1f %.1f), the thumb "
               "turned %.0f (choice %d), the palm %.0f; %.1f us\n",
        realtime, hand == HAND_MAIN ? "main" : "off", g.model ? g.model->name : "-", g.solution.triangles,
        glm::length(g.solution.palm) * rigUnit / units::metresToUnits() * 100.f, g.solution.palm.x, g.solution.palm.y,
        g.solution.palm.z, glm::degrees(glm::angle(g.solution.thumbTurn)), g.solution.thumbChoice,
        glm::degrees(glm::angle(g.solution.palmTurn)), g.solution.seconds * 1e6);
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
    s.overlap = std::fmax(vr_hand_fit_overlap.value, 0.f) * toRig;
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
    const glm::mat4 inRig = glm::inverse(rigMatrix) * grasp::shapeToWorld(*held.ent, held.mirrored);
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
    const grasp::Settings settings = graspSettings(held, rigUnit);
    const bool settingsChanged = settings.palmLimit != g.settings.palmLimit || settings.palmTurnLimit != g.settings.palmTurnLimit ||
                                 settings.overlap != g.settings.overlap || settings.thenar != g.settings.thenar ||
                                 settings.thumbTop != g.settings.thumbTop;
    if(!settingsChanged && !otherMoved && sameGrasp(g, held.ent->model, frame, inRig, rh.pose, rigUnit))
    {
        return;
    }

    QVR_PROFILE("grasp solve");
    const bool same = g.valid && g.model == held.ent->model && g.frame == frame;
    grasp::Solution solution;
    if(cup)
    {
        static grasp::Shape otherShape;
        static std::vector<grasp::Triangle> tris;
        tris.clear();
        const auto at = [&](const glm::vec3& p) { return glm::vec3{otherInRig * glm::vec4{drawnInRig(otherHand, p), 1.f}}; };
        for(int t = 0; t < handrig::data::numPalmTriangles; t++)
        {
            const auto* tri = handrig::data::palmTriangles[t];
            tris.push_back({{at(handrig::palmVertex(otherHand.posed, tri[0])), at(handrig::palmVertex(otherHand.posed, tri[1])),
                at(handrig::palmVertex(otherHand.posed, tri[2]))}});
        }
        for(int t = 0; t < handrig::data::numFingerTriangles; t++)
        {
            const auto* tri = handrig::data::fingerTriangles[t];
            tris.push_back({{at(otherHand.posed.vertex[tri[0]]), at(otherHand.posed.vertex[tri[1]]), at(otherHand.posed.vertex[tri[2]])}});
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
// fingers' closing reach, not beside them.
void alignChannel(int hand, bool mirrored, const glm::vec3& on, const glm::vec3& axis, glm::vec3& pos, glm::vec3& handRot)
{
    glm::vec3 cp, cd;
    float radius;
    if(!handrig::usable(viewModel(handrig::modelName)) || !grasp::gripChannel(rigHands[hand].pose, cp, cd, radius))
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

// `motion`: the held weapon's firing animation, moving the drawn hand after its grasp (solved without it).
bool setupRigHand(int hand, const glm::vec3& pos, const glm::vec3& handRot, bool mirrored, bool hide, const Held& held,
    const glm::mat4& motion)
{
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

    // Where the six models are drawn: each at the hand plus its offset (fingerOffset, at offsetScale), scaled
    // by the fist slot's Scale about its own scale origin (weapons::modelTransform). The rig is hand_base.mdl's
    // space: the entity goes where the palm's origin is drawn, each finger moves from its bind place (the
    // defaults) by what its current offset changes.
    const weapons::ModelTransform t = weapons::modelTransform(model);
    const float k = t.active ? t.k : 1.f;
    const glm::vec3 ts = t.active ? t.scale : glm::vec3{1.f};
    const glm::vec3 baseOrigin = vec3Of(handrig::data::baseScaleOrigin);
    const glm::vec3 mBase = offsetInModel(fingerOffset(FingerBase, hand));
    const glm::vec3 e = mBase + k * (glm::vec3{1.f} - ts) * baseOrigin;
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        const glm::vec3 m = offsetInModel(fingerOffset(rigFinger[f], hand));
        const glm::vec3 origin = vec3Of(handrig::data::fingerScaleOrigin[f]);
        rh.pose.shift[f] = ((m - mBase) / k + (glm::vec3{1.f} - ts) * (origin - baseOrigin)) / ts -
                           vec3Of(handrig::data::fingerBindShift[f]);
    }
    rh.pose.metacarpal = glm::quat{1.f, 0.f, 0.f, 0.f};

    view::ViewEntity& ve = entities.hand[hand][FingerBase];
    const glm::mat4 rigToWorld = rigPlacement(hand, pos, handRot, mirrored, &ve);

    // The fingers: the controller's curls (as the six models'), or wrapping what the hand holds.
    updateGrasp(hand, held, rigToWorld, k * ts.x);
    rh.held = held;
    rh.rigToWorld = rigToWorld;
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
        if(rh.grasp.valid)
        {
            grasp::curls(rh.grasp.solution.finger[f], curl, engagement(curl), target);
            if(f == handrig::Index && held.trigger)
            {
                // The trigger finger pulls as the trigger is pulled, past where it met the weapon (the trigger
                // gives): from its stop (or the controller's curl, if less) towards a trigger pull's hook.
                const float pull = CLAMP(0.f, curl / 5.f, 1.f);
                for(int j = 0; j < handrig::jointsPerFinger; j++)
                {
                    target[j] = std::fmax(target[j], std::fmin(rh.grasp.solution.finger[f].stop[j], curl) + pull * triggerPull[j]);
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
    const glm::quat thumbTarget = rh.grasp.valid ? glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, rh.grasp.solution.thumbTurn, w)
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
        line += va(" %.3f %.4f %d %.3f %.2f %d\n", turned, moved, twohand::helping(hand) ? 1 : 0,
            glm::distance(glm::vec3{motion * glm::vec4{at, 1.f}}, at), glm::degrees(glm::angle(glm::quat_cast(glm::mat3{motion}))),
            entities.weapon[hand].ent.frame);
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
    ve.ent.skinnum = damageLevel();
    ve.visible = !hide;
    for(int finger = FingerBase + 1; finger < FingerCount; finger++)
    {
        entities.hand[hand][finger].visible = false;
    }
    rh.drawn = ve.visible;
    return true;
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
        rigHands[hand].drawn = false;
        rigHands[hand].held = Held{};
        return;
    }

    // The hand turned from the controller by the fist's angle offsets, rigidly (round 21, second pass: added as Euler
    // angles, the hand slid round what it held as the wrist turned, and its grasp was solved again and again).
    glm::vec3 handRot = basisAngles(anglesBasis(s.rot[hand]) * anglesBasis(weaponAngleOffsets(fist, mirrored)));

    // The hand is where the controller is, holding a weapon or not (round 21: the weapon is placed in the hand, and the
    // fingers wrap it; it was drawn at an anchor vertex of the weapon, which the settings kept within 0.8 of a
    // centimetre of the controller).
    glm::vec3 pos = s.pos[hand];
    bool hide = false;
    glm::mat4 motion{1.f}; // the weapon's firing animation where the hand holds it (animationMotion)
    if(slot >= 0 && slot != fist)
    {
        hide = weapons::value(slot, Key::HideHand) != 0.f;
        handRot = attachedTurn(s.rot[hand], slot, mirrored, weaponAngleOffsets(fist, mirrored));
        motion = animationMotion(weapon, pos);
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

        bool cupped = false;
        if(twohand::helping(hand) && otherSlot >= 0)
        {
            // A grip: the weapon's fixed-hand angles; a cup: turned as the holding hand is. Then the hotspot's own.
            const int chosen = chosenGrip[other];
            const WorldHotspot* spot = chosen >= 0 ? &worldHotspots[other][chosen] : nullptr;
            const bool cup = spot && spot->type == weapons::HotspotType::Cup;
            // (The weapon's and the hotspot's are for the off hand helping: mirrored for the main hand.)
            glm::vec3 offsets = cup ? glm::vec3{0.f}
                                    : weapons::vec(otherSlot, Key::TwoHFixedHandPitch, Key::TwoHFixedHandYaw, Key::TwoHFixedHandRoll);
            if(spot)
            {
                offsets += spot->angles;
            }
            if(!mirrored)
            {
                offsets.y = -offsets.y;
                offsets.z = -offsets.z;
            }
            if(cup)
            {
                offsets += weaponAngleOffsets(fist, mirrored); // the hand's own, as the holding hand's
            }
            cupped = cup;
            // The weapon hand's angles and the grip's, without the fist's own angle offsets (old engine's
            // V_SetupFixedHelpingHandViewEnt), carried rigidly by the weapon (attachedTurn), turned onto it as the
            // hand takes the grip.
            glm::vec3 attached = attachedTurn(s.rot[other], otherSlot, other == HAND_OFF, offsets);
            // A cup: its channel round the holding hand's (the holding hand drawn this frame, or the last for the
            // off hand holding: it is set up after), where the hotspot puts it along it.
            if(cupped && rigHands[other].drawn)
            {
                glm::vec3 cp, cd;
                float radius;
                if(grasp::gripChannel(rigHands[other].pose, cp, cd, radius))
                {
                    const glm::mat4& om = rigHands[other].rigToWorld;
                    glm::vec3 at = s.grip2H[other];
                    alignChannel(hand, mirrored, glm::vec3{om * glm::vec4{cp, 1.f}}, glm::mat3{om} * cd, at, attached);
                    pos = glm::mix(s.pos[hand], at, gripBlend);
                }
            }
            const glm::quat from = glm::quat_cast(anglesBasis(handRot)), to = glm::quat_cast(anglesBasis(attached));
            handRot = basisAngles(glm::mat3_cast(glm::slerp(from, to, gripBlend)));
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

    // What the fingers wrap (vr_hand_fit): the gun in the hand, the other hand's gun it helps hold (its foregrip, a
    // sword's blade), a carried box or gib, the flashlight.
    Held held;
    if(gripBlend > 0.f && entities.weapon[other].ent.model)
    {
        held = {&entities.weapon[other].ent, other == HAND_OFF, 0, true};
        // Its hotspot's kind and style (the blade's, for the half-sword grip).
        const WorldHotspot* spot = nullptr;
        if(twohand::bladeGrip(other))
        {
            for(const WorldHotspot& w : worldHotspots[other])
            {
                spot = w.type == weapons::HotspotType::Blade ? &w : spot;
            }
        }
        else if(chosenGrip[other] >= 0)
        {
            spot = &worldHotspots[other][chosenGrip[other]];
        }
        if(spot)
        {
            held.cup = spot->type == weapons::HotspotType::Cup;
            held.thumbTop = spot->style == weapons::HotspotStyle::ThumbTop;
        }
    }
    else if(slot >= 0 && slot != fist)
    {
        held = {&weapon.ent, mirrored, 0, true, true};
    }
    else if(const int ent = held::heldEntity(hand))
    {
        held = {&cl_entities[ent], false, -1};
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
        }
    }
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
        if(body::HolsterPlate plate = plates[static_cast<std::size_t>(bodyHolster[h])]; !shoulder && plate.out != glm::vec3{0.f})
        {
            glm::vec3 at = pos;
            if(vr_body_debug.value >= 2.f)
            {
                // The body's preview (vr_body_debug 2 and 3: in front of the player, turned)
                // carries them too.
                const glm::vec3 root{s.head.x, s.head.y, 0.f};
                const glm::vec3 centre =
                    root + hands::forward({0.f, yaw, 0.f}) * (1.8f * units::metresToUnits() * units::bodyScale());
                const glm::mat3 turn = glm::mat3_cast(glm::angleAxis(
                    glm::radians(vr_body_debug.value >= 3.f ? -90.f : 180.f), glm::vec3{0.f, 0.f, 1.f}));
                at = centre + turn * (pos - root);
                plate.out = turn * plate.out;
                plate.up = turn * plate.up;
            }
            pose = holsterOnBody(at, plate, mirrored, h == LeftUpper || h == RightUpper, slotModel);
        }

        const int stat = static_cast<int>(bodyHolster[h]);
        qmodel_t* model = precachedModel(cl.stats[STAT_QVR_HOLSTERWEAPONMODEL0 + stat]);
        if(isHandModel(model))
        {
            model = nullptr;
        }

        view::ViewEntity& ve = entities.holster[h];
        place(ve, model, pose.weaponPos, pose.weaponAngles, 0, mirrored);
        highlight(ve, hover);

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
        idleAttachments(ve.ent, mirrored, slot, entities.holsterButton[h], cl.stats[STAT_QVR_HOLSTERWEAPONCLIP0 + stat],
            queueTexts && model != nullptr);
        highlight(entities.holsterButton[h], hover);
        if(model && slot >= 0 && emissive::isLavaGun(model))
        {
            emissive::lavaGunLight(2 + h, lavaGlowPosition(ve.ent, mirrored, 0.f, slot), vr_lavagun_light_idle.value);
        }
    }
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
    place(ve, viewModel("progs/vrgadget.mdl"), pose.origin, {-a.x, a.y, a.z}, 0, false);
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
        qmodel_t* model = viewModel(name);
        if(!avatar::usable(model))
        {
            model = viewModel("progs/vrbody.mdl");
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

    place(ve, viewModel("progs/wpnbutton.mdl"), pos, angles, 0, mirrored);
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
    showHotspots();
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
    int base = 1;
    fprintf(f, "o hand\n");
    for(int i = 0; i < handrig::data::numPalmVertices; i++)
    {
        const glm::vec3 p = drawnInRig(rh, handrig::palmVertex(rh.posed, i));
        fprintf(f, "v %f %f %f\n", p.x, p.y, p.z);
    }
    for(int v = 0; v < handrig::data::numVertices; v++)
    {
        const glm::vec3 p = drawnInRig(rh, rh.posed.vertex[v]);
        fprintf(f, "v %f %f %f\n", p.x, p.y, p.z);
    }
    for(int t = 0; t < handrig::data::numPalmTriangles; t++)
    {
        const auto* tri = handrig::data::palmTriangles[t];
        fprintf(f, "f %d %d %d\n", base + tri[0], base + tri[1], base + tri[2]);
    }
    const int fingers = base + handrig::data::numPalmVertices;
    for(int t = 0; t < handrig::data::numFingerTriangles; t++)
    {
        const auto* tri = handrig::data::fingerTriangles[t];
        fprintf(f, "f %d %d %d\n", fingers + tri[0], fingers + tri[1], fingers + tri[2]);
    }
    base = fingers + handrig::data::numVertices;

    // The other hand, when this one cups it.
    const RigHand& otherHand = rigHands[1 - hand];
    if(rh.held.cup && otherHand.drawn)
    {
        const glm::mat4 toThis = glm::inverse(rh.rigToWorld) * otherHand.rigToWorld;
        fprintf(f, "o other\n");
        for(int i = 0; i < handrig::data::numPalmVertices; i++)
        {
            const glm::vec3 p{toThis * glm::vec4{drawnInRig(otherHand, handrig::palmVertex(otherHand.posed, i)), 1.f}};
            fprintf(f, "v %f %f %f\n", p.x, p.y, p.z);
        }
        for(int v = 0; v < handrig::data::numVertices; v++)
        {
            const glm::vec3 p{toThis * glm::vec4{drawnInRig(otherHand, otherHand.posed.vertex[v]), 1.f}};
            fprintf(f, "v %f %f %f\n", p.x, p.y, p.z);
        }
        for(int t = 0; t < handrig::data::numPalmTriangles; t++)
        {
            const auto* tri = handrig::data::palmTriangles[t];
            fprintf(f, "f %d %d %d\n", base + tri[0], base + tri[1], base + tri[2]);
        }
        const int otherFingers = base + handrig::data::numPalmVertices;
        for(int t = 0; t < handrig::data::numFingerTriangles; t++)
        {
            const auto* tri = handrig::data::fingerTriangles[t];
            fprintf(f, "f %d %d %d\n", otherFingers + tri[0], otherFingers + tri[1], otherFingers + tri[2]);
        }
        base = otherFingers + handrig::data::numVertices;
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
    if(slot < 0 || !hotspotAt(hand, hands::current().pos[1 - hand], p))
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
void dumpView_f()
{
    const hands::State& s = hands::current();
    Con_Printf("hands valid %d  player (%.1f %.1f %.1f)  main (%.1f %.1f %.1f)  off (%.1f %.1f %.1f)%s\n", s.valid,
        s.playerOrigin.x, s.playerOrigin.y, s.playerOrigin.z, s.pos[1].x, s.pos[1].y, s.pos[1].z, s.pos[0].x, s.pos[0].y,
        s.pos[0].z, twohand::helping(HAND_OFF) ? ", the off hand helping" : "");

    for(int h = 0; h < 2; h++)
    {
        if(s.grip2HValid[h])
        {
            Con_Printf("%s weapon foregrip (%.1f %.1f %.1f)\n", h == HAND_MAIN ? "main" : "off", s.grip2H[h].x,
                s.grip2H[h].y, s.grip2H[h].z);
        }
        if(s.muzzleValid[h])
        {
            Con_Printf("%s weapon muzzle (%.3f %.3f %.3f), %.1f units from the hand%s\n", h == HAND_MAIN ? "main" : "off",
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

    int i = 0;
    forEachEntity([&](ViewEntity& ve) {
        const entity_t& e = ve.ent;
        Con_Printf("%2d %-24s vis %d mir %d frame %d org (%.3f %.3f %.3f) ang (%.0f %.0f %.0f)\n", i++,
            e.model ? e.model->name : "-", ve.visible, ve.mirrored, e.frame, e.origin[0], e.origin[1],
            e.origin[2], e.angles[0], e.angles[1], e.angles[2]);
    });
}

} // namespace qvr::view
