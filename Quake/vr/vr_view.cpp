// vr_view.cpp -- see vr_view.hpp. Ported from the old engine's view.cpp (V_RenderView_*).

#include "vr_view_internal.hpp"
#include "vr_holsterzones.hpp"
#include "vr_adcrossbow.hpp"

namespace view_detail
{
namespace
{
double fingerFramesTime = -1.0;
} // namespace

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
            const float trigger = in.triggerTouch ? za::fmax(in.triggerValue, 0.5f) : in.triggerValue;
            return curl(indexWithGrip && vr_hand_fit.value ? za::fmax(trigger, in.gripValue) : trigger);
        }
        default: return curl(in.gripValue);
    }
}

namespace
{
[[nodiscard]] float biasFor(int hand, int finger)
{
    // Posing (vr_posing.cpp): the weapon hand holds the posed weapon; the other hand, posing a hotspot, helps.
    // In a holster (Target::Holster) both hands are drawn empty.
    const bool posed = posing::active() && posing::session().target != posing::Target::Holster &&
                       hand == posing::session().weaponHand;
    if(posing::active() && posing::session().target == posing::Target::Holster)
    {
        return 0.f;
    }
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
        value = value < target ? za::fmin(value + step, target) : za::fmax(value - step, target);
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
} // namespace

[[nodiscard]] FingerPose fingerPose(int hand, int finger)
{
    const float curl = CLAMP(0.f, za::floor(fingerFrames[hand][finger] + 0.5f) + 5.f * fingerBias[hand][finger], 5.f);
    const int frame = static_cast<int>(za::ceil(curl - 0.01f));
    if(frame <= 0)
    {
        return {0, 0.f};
    }
    const float open = 1.f - curl / static_cast<float>(frame);
    return {frame, open < 0.5f / 255.f ? 0.f : open};
}

namespace
{
// The magazines' reference points (QC vr_reload.qc VR_Reload_MagRef): each prop's top, its feed end (make_mags.py: up its
// +z from its middle), moved by vr_reload_mag_<kind>_x/y/z (its model units), with its own radius: it seats when it comes
// within the gun's radius plus its own of the gun's point.
struct MagProp
{
    const char* model;
    float top;
    cvar_t* offset[3];
    cvar_t* radius;
};
constexpr MagProp magProps[] = {
    {"progs/vr_mag_nail.mdl", 3.3f, {&vr_reload_mag_nail_x, &vr_reload_mag_nail_y, &vr_reload_mag_nail_z},
        &vr_reload_mag_nail_radius},
    {"progs/vr_mag_snail.mdl", 4.7f, {&vr_reload_mag_snail_x, &vr_reload_mag_snail_y, &vr_reload_mag_snail_z},
        &vr_reload_mag_snail_radius},
    {"progs/vr_mag_light.mdl", 3.6f, {&vr_reload_mag_light_x, &vr_reload_mag_light_y, &vr_reload_mag_light_z},
        &vr_reload_mag_light_radius},
};
#define QVR_WELL_OFFSETS(k)                                                                                             \
    {&vr_reload_well_##k##_x, &vr_reload_well_##k##_y, &vr_reload_well_##k##_z, &vr_reload_well_##k##_pitch,         \
        &vr_reload_well_##k##_yaw, &vr_reload_well_##k##_roll}
constexpr MagMount magMounts[] = {
    {modelmeta::Id::VNail, "progs/vr_mag_on_v_nail.mdl", "8.8071 1.7302 -0.3806", "progs/vr_magwell_on_v_nail.mdl", {6.9f, 0.f, -1.45f},
        {7.36f, 0.f, -4.72f}, QVR_WELL_OFFSETS(nail), 0},
    {modelmeta::Id::VLava, "progs/vr_mag_on_v_lava.mdl", "8.8071 1.7302 -0.3806", "progs/vr_magwell_on_v_lava.mdl", {6.9f, 0.f, -1.45f},
        {7.36f, 0.f, -4.72f}, QVR_WELL_OFFSETS(nail), 0},
    {modelmeta::Id::VNail2, "progs/vr_mag_on_v_nail2.mdl", "4.7417 4.8402 0.2747", "progs/vr_magwell_on_v_nail2.mdl", {7.2f, 5.17f, 1.76f},
        {7.2f, 9.56f, 3.45f}, QVR_WELL_OFFSETS(snail), 1},
    {modelmeta::Id::VLava2, "progs/vr_mag_on_v_lava2.mdl", "4.7401 4.8594 0.2797", "progs/vr_magwell_on_v_lava2.mdl", {7.2f, 5.17f, 1.76f},
        {7.2f, 9.56f, 3.45f}, QVR_WELL_OFFSETS(snail), 1},
    {modelmeta::Id::VLight, "progs/vr_mag_on_v_light.mdl", "7.0097 0.5622 3.9392", "progs/vr_magwell_on_v_light.mdl", {11.6f, 0.f, 3.f},
        {11.6f, 0.f, -0.6f}, QVR_WELL_OFFSETS(light), 2},
    {modelmeta::Id::VPlasma, "progs/vr_mag_on_v_plasma.mdl", "7.0097 0.5622 3.9392", "progs/vr_magwell_on_v_plasma.mdl", {11.6f, 0.f, 3.f},
        {11.6f, 0.f, -0.6f}, QVR_WELL_OFFSETS(light), 2},
    // AD support: AD's Plasma Gun (vr_adweapons.cpp): the lightning gun's cell and well turned out of its inner side (+y),
    // into the receiver made there; it pops out sideways (QC VR_Reload_MagOutWay: the way the well faces). Seated 0.6 further
    // out than the lightning gun's would be (his VR notes 2026-10-18: it shows a little more; vr_adweapons.cpp plasmaSeat).
    {modelmeta::Id::AdVrPlasma, "progs/vr_mag_on_v_plasma.mdl#vr", "11.6 1.5 4.5", "progs/vr_magwell_on_v_plasma.mdl#vr",
        {11.6f, 2.1f, 4.5f}, {11.6f, 5.7f, 4.5f}, QVR_WELL_OFFSETS(adplasma), 2},
};
#undef QVR_WELL_OFFSETS
} // namespace

// The distance from `p` to the box `box` (its middle, its three half-axes: hands::State::magBox); 0 inside it.
[[nodiscard]] float boxDistance(const glm::vec3 (&box)[4], const glm::vec3& p)
{
    const glm::vec3 d = p - box[0];
    glm::vec3 nearest = box[0];
    for(int i = 1; i < 4; i++)
    {
        const float len2 = glm::dot(box[i], box[i]);
        if(len2 > 0.f)
        {
            nearest += box[i] * za::clamp(glm::dot(d, box[i]) / len2, -1.f, 1.f);
        }
    }
    return glm::distance(p, nearest);
}
[[nodiscard]] const MagMount* magMountFor(const qmodel_t* gun)
{
    if(!gun || gun->type != mod_alias)
    {
        return nullptr;
    }
    const auto& info = modelmeta::get(gun);
    for(const MagMount& m : magMounts)
    {
        if(info.is(m.gun))
        {
            return &m;
        }
    }
    return nullptr;
}

namespace
{
// The shotgun's auto pump (vr_autopump.cpp; polish_weapons.py auto_pump, split_auto_pump): its moving fore-end and the gun
// without it, made in v_shot.mdl's model space with its header (scale, origin) and frames (setupPumps).
constexpr const char* pumpModelName = "progs/vr_pump_on_v_shot.mdl";
constexpr const char* pumpBodyModelName = "progs/vr_pumpbody_on_v_shot.mdl";

// The super shotgun broken open (immersive reloading's phase 2b; QC vr_reload.qc, QVR_WPNFLAG_SSG_OPEN): drawn as
// make_ssg_open.py's two parts of v_shot2.mdl (made in its model space: +x forward, +y left, +z up), the frame where the
// gun is and the barrels turned down about the hinge by vr_reload_ssg_open_angle, dropping open and snapping shut over a
// few frames (ssgStep: smooth at any frame rate); the barrels' skin shows its loaded chambers (0, 1, 2). Closed, the gun
// is drawn as itself. Its load point turns with the barrels (setupWeapon), as its casings do (vr_shells.cpp).
constexpr const char* ssgFrameModel = "progs/vr_ssg_frame_on_v_shot2.mdl";
constexpr const char* ssgBarrelsModel = "progs/vr_ssg_barrels_on_v_shot2.mdl";
constexpr glm::vec3 ssgHinge{12.6f, 0.f, 3.2f}; // the hinge pin, under the barrels at the receiver's front
constexpr int weaponFlagSsgOpen = 32;           // QC's QVR_WPNFLAG_SSG_OPEN (vr_defs.qc)
constexpr float ssgOpenSpeed = 450.f;           // degrees a second the barrels drop open,
constexpr float ssgCloseSpeed = 900.f;          // and snap shut
SsgDrawn ssgHolsters[HolsterCount];
double worldSsgPrinted = -1.0; // (setupWorldSsgs' debug print: once a second)
const entity_t* worldSsgSource[maxWorldSsgs]{}; // the lying gun each of entities.worldSsg* stands for (its blood's key)
double worldMagPrinted = -1.0; // (setupMagazines' debug print of a lying gun's magazine: once a second)
double worldScreenPrinted = -1.0; // (setupWorldWeapons' debug print of a lying gun's ammo screen: once a second)

[[nodiscard]] bool ssgBreaks()
{
    return cl.stats[protocol::STAT_QVR_RELOADMODE] == 3 && vr_reload_ssg_break.value != 0.f;
}

// A point (or a direction) of the super shotgun's model turned down `deg` degrees about its hinge, as its barrels are.
[[nodiscard]] glm::vec3 ssgTurn(const glm::vec3& p, float deg, bool point = true)
{
    const float a = glm::radians(deg);
    const float c = std::cos(a);
    const float s = std::sin(a);
    const glm::vec3 d = point ? p - ssgHinge : p;
    const glm::vec3 r{d.x * c + d.z * s, d.y, -d.x * s + d.z * c};
    return point ? ssgHinge + r : r;
}
} // namespace

// The angle the super shotgun `gun` (flags `flags`) is drawn open at now, stepped towards open or shut.
float ssgStep(SsgDrawn& st, const qmodel_t* gun, int flags)
{
    const bool ssg = gun && modelmeta::is(gun, modelmeta::Id::VShot2);
    const float target = ssg && ssgBreaks() && (flags & weaponFlagSsgOpen) ? CLAMP(0.f, vr_reload_ssg_open_angle.value, 80.f) : 0.f;
    const float dt = st.time >= 0.0 ? static_cast<float>(CLAMP(0.0, vr_gametime - st.time, 0.1)) : 0.f;
    st.time = vr_gametime;
    if(gun != st.gun)
    {
        st.gun = gun;
        st.angle = target; // (a gun come to the hand, or holstered: as it is)
    }
    else if(st.angle < target)
    {
        st.angle = za::min(target, st.angle + ssgOpenSpeed * dt);
    }
    else if(st.angle > target)
    {
        st.angle = za::max(target, st.angle - ssgCloseSpeed * dt);
    }
    return st.angle;
}

// A world point on the super shotgun `gun` (as drawn closed) turned with its barrels `deg` degrees down about the
// hinge if it is on them (in front of the hinge): through the gun's model space (its drawn transform, mirrored or not,
// inverted from four of its points).
[[nodiscard]] glm::vec3 ssgTurnWorld(const view::ViewEntity& gun, const glm::vec3& w, float deg)
{
    const glm::vec3 o = view::modelPoint(gun, glm::vec3{0.f});
    const glm::mat3 m{view::modelPoint(gun, {1.f, 0.f, 0.f}) - o, view::modelPoint(gun, {0.f, 1.f, 0.f}) - o,
        view::modelPoint(gun, {0.f, 0.f, 1.f}) - o};
    if(std::abs(glm::determinant(m)) < 1e-9f)
    {
        return w;
    }
    const glm::vec3 local = glm::inverse(m) * (w - o);
    return local.x > ssgHinge.x ? view::modelPoint(gun, ssgTurn(local, deg)) : w; // (behind the hinge: the frame's)
}

// The barrels' turn `deg` degrees down about the hinge as a turn in the world (the gun as drawn, mirrored or not): what
// turns a hand holding them (setupHand).
[[nodiscard]] glm::mat3 ssgWorldTurn(const view::ViewEntity& gun, float deg)
{
    const glm::vec3 o = view::modelPoint(gun, glm::vec3{0.f});
    const glm::mat3 m{view::modelPoint(gun, {1.f, 0.f, 0.f}) - o, view::modelPoint(gun, {0.f, 1.f, 0.f}) - o,
        view::modelPoint(gun, {0.f, 0.f, 1.f}) - o};
    if(std::abs(glm::determinant(m)) < 1e-9f)
    {
        return glm::mat3{1.f};
    }
    const glm::mat3 r{ssgTurn({1.f, 0.f, 0.f}, deg, false), ssgTurn({0.f, 1.f, 0.f}, deg, false),
        ssgTurn({0.f, 0.f, 1.f}, deg, false)};
    return m * r * glm::inverse(m);
}

namespace
{
// The body's mirror image while a menu page tweaks it (vr_bodyclone.cpp, addClones): each view entity worn or held drawn
// again reflected, by the one at its own index in Entities (one run of ViewEntity: find()).
constexpr int viewEntityCount = static_cast<int>(sizeof(Entities) / sizeof(view::ViewEntity));
view::ViewEntity clones[viewEntityCount];
int lastAddedFrame = -1;
const entity_t* worldWeaponsNear[maxWorldWeapons]{}; // setupWorldWeapons' nearest, for their magazines (setupMagazines)
int pouchCounterFrame = -1; // the ammo pouch's counter queued in this frame (setupAmmoPouch: the view may be set up twice)
bool ammoPouchPlaced = false; // the ammo pouch placed last frame, drawn or not (setupAmmoPouch; ammoPouchRounds)

// Hidden for a clean shot (vr_shot_hide; Debug > Cheats and Recording): 1 the wrist gadget (setupGadget hides it whole,
// its screen and messages with it), 2 the hands and what they hold, 4 the body and the holsters. Drawn only: they
// work on as ever.
[[nodiscard]] bool hiddenForShot(const view::ViewEntity& ve)
{
    const int hide = static_cast<int>(vr_shot_hide.value);
    if(hide == 0)
    {
        return false;
    }
    const auto among = [&ve](const auto& list) {
        for(const view::ViewEntity& e : list)
        {
            if(&e == &ve)
            {
                return true;
            }
        }
        return false;
    };
    if((hide & 2) && (among(entities.weapon) || among(entities.weaponMorph) || among(entities.hand[0]) ||
                         among(entities.hand[1]) || among(entities.button) || among(entities.frontButton) ||
                         among(entities.muzzleFlash) || &ve == &entities.flashlight || &ve == &entities.sawHandle ||
                         among(entities.mag) || among(entities.well) || among(entities.pumpBody) ||
                         among(entities.pump) || among(entities.ssgFrame) || among(entities.ssgBarrels)))
    {
        return true;
    }
    return (hide & 4) && (&ve == &entities.body || among(entities.pauldron) || among(entities.pauldronArm) ||
                             among(entities.holster) || among(entities.holsterSlot) || among(entities.holsterButton) ||
                             &ve == &entities.pouch || &ve == &entities.ammoPouch || among(entities.ammoPouchGrenade) ||
                             among(entities.holsterMag) || among(entities.holsterWell) ||
                             among(entities.holsterSsgFrame) || among(entities.holsterSsgBarrels));
}

// Drawn in the body's mirror image (addClones): what is worn or held, not the guns lying about (their buttons, magazines
// and wells, the open super shotguns), the motion review's ghosts or the enemies' muzzle flashes.
[[nodiscard]] bool worn(const view::ViewEntity& ve)
{
    const auto among = [&ve](const auto& list) {
        return &ve >= &list[0] && &ve < &list[0] + sizeof(list) / sizeof(list[0]);
    };
    return !among(entities.worldButton) && !among(entities.worldFrontButton) && !among(entities.worldMag) &&
           !among(entities.worldWell) && !among(entities.worldSsgFrame) && !among(entities.worldSsgBarrels) &&
           !among(entities.ghost) && !among(entities.enemyFlash);
}

// The copy of `ve` in the body's mirror image (vr_bodyclone.cpp), `mirror` the world to it: its place reflected and its
// turn the reflected one (a reflection is a turn and a flip: the model flipped across its Y, as the off hand's, so
// mirrored the other way). The renderer's hooks give it its source's bones (VR_AliasBonePoses: the body's with its head)
// and scale (avatar::modelScale).
view::ViewEntity& addClone(const view::ViewEntity& ve, const glm::mat4& mirror)
{
    view::ViewEntity& c = clones[&ve - reinterpret_cast<const view::ViewEntity*>(&entities)];
    c = ve;
    const glm::mat3 h{mirror};
    const glm::vec3 o = glm::vec3{mirror * glm::vec4{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2], 1.f}};
    float r[16];
    vec3_t zero{0.f, 0.f, 0.f}, angles{ve.ent.angles[0], ve.ent.angles[1], ve.ent.angles[2]};
    R_EntityMatrix(r, zero, angles, ENTSCALE_DEFAULT);
    // R_EntityMatrix's columns: forward (cy cp, sy cp, sp), left (.., .., cp sr), up (.., .., cr cp).
    const glm::vec3 f = h * glm::vec3{r[0], r[1], r[2]};
    const glm::vec3 l = -(h * glm::vec3{r[4], r[5], r[6]});
    const glm::vec3 u = h * glm::vec3{r[8], r[9], r[10]};
    const glm::vec3 a{glm::degrees(za::asin(za::clamp(f.z, -1.f, 1.f))), glm::degrees(za::atan2(f.y, f.x)),
        glm::degrees(za::atan2(l.z, u.z))};
    for(int i = 0; i < 3; i++)
    {
        c.ent.origin[i] = o[i];
        c.ent.angles[i] = a[i];
    }
    c.mirrored = !ve.mirrored;
    c.lightShift = h * ve.lightShift;
    return c;
}
} // namespace

[[nodiscard]] bool isHandModel(const qmodel_t* model)
{
    if(!model)
    {
        return false;
    }

    const auto& info = modelmeta::get(model);
    return info.is(modelmeta::Id::Hand) || info.is(modelmeta::Id::HandBase) ||
           info.has(modelmeta::Trait::Finger) || info.is(modelmeta::Id::HandRig);
}
[[nodiscard]] qmodel_t* precachedModel(int index)
{
    return index > 0 && index < MAX_MODELS ? cl.model_precache[index] : nullptr;
}

} // namespace view_detail

qmodel_t* view::viewModel(const char* name)
{
    using Entry = ViewModelEntry;
    Entry* const entries = viewModels;
    const za::UIntPtrT slot = (reinterpret_cast<za::UIntPtrT>(name) * 0x9E3779B97F4A7C15ull) >> 56;
    Entry* pick = &entries[slot];
    if(pick->name != name && (entries[slot ^ 1].name == name || (pick->name && !entries[slot ^ 1].name)))
    {
        pick = &entries[slot ^ 1]; // (found in the other way, or that way still free)
    }
    Entry& e = *pick;
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

qmodel_t* view::magazineModelOf(const qmodel_t* gun)
{
    const MagMount* mount = magMountFor(gun);
    return mount ? viewModel(mount->model) : nullptr;
}

namespace view_detail
{
namespace
{
// The body's model for a build (0 lean, 1 athletic, 2 brawny), with the bracers or bare forearms (vr_body_bracers 0:
// progs/vrbody*_bare, make_vrbody.py; fitted round the jointed hand, so only with it: vr_hand_rig).
[[nodiscard]] const char* bodyModelName(int build, bool bare)
{
    if(bare)
    {
        return build <= 0 ? "progs/vrbody_lean_bare.mdl" : build >= 2 ? "progs/vrbody_brawny_bare.mdl" : "progs/vrbody_bare.mdl";
    }
    return build <= 0 ? "progs/vrbody_lean.mdl" : build >= 2 ? "progs/vrbody_brawny.mdl" : "progs/vrbody.mdl";
}

[[nodiscard]] bool bareForearms()
{
    return !vr_body_bracers.value && vr_hand_rig.value;
}

} // namespace

} // namespace view_detail

void view::prepareModels()
{
    if(!vr_enabled.value) // (in VR, even before its session runs: the first map loads as it starts)
    {
        return;
    }
    // As the view setup asks for them (setupBody, setupPauldrons, setupHolsters, setupGadget, the hands, the buttons).
    za::Vector<const char*> names{handrig::modelName, "progs/vrgadget.mdl", "progs/vrgadget_strap.mdl", "progs/wpnbutton.mdl"};
    if(vr_body_mode.value >= 1.f)
    {
        const int build = static_cast<int>(vr_body_build.value);
        if(bareForearms())
        {
            names.pushBack(bodyModelName(build, true));
        }
        names.pushBack(bodyModelName(build, false));
        names.pushBack("progs/vrbody.mdl");
        if(vr_body_pauldrons.value)
        {
            names.pushBack("progs/vrpauldron.mdl");
            names.pushBack("progs/vrpauldron_arm.mdl");
        }
    }
    if(vr_leg_holster_model_enabled.value)
    {
        names.pushBack("progs/legholster.mdl");
    }
    if(body::pouchEnabled())
    {
        names.pushBack("progs/vrpouch.mdl"); // (setupPouch)
    }
    // (Whatever the mode: the server's, STAT_QVR_RELOADMODE, comes after the precache.)
    names.pushBack("progs/vrpouch_ammo.mdl"); // (setupAmmoPouch)
    for(const MagMount& m : magMounts)
    {
        names.pushBack(m.model); // (setupMagazines)
        names.pushBack(m.well);
    }
    names.pushBack(pumpModelName); // (setupPumps)
    names.pushBack(pumpBodyModelName);
    // (setupSsgParts: else the super shotgun's first draw loads them, a 15 ms hitch with their normal maps.)
    names.pushBack(ssgFrameModel);
    names.pushBack(ssgBarrelsModel);
    for(const char* name : names)
    {
        (void)Mod_ForName(name, false);
    }
}

bool view::carriedWeaponPose(int hand, glm::vec3& pos, glm::vec3& rot, bool& mirrored, glm::vec3& tip)
{
    if(hand < 0 || hand > 1 || !weaponCarried[hand] || !carriedTipValid[hand])
    {
        return false;
    }
    pos = drawnAs[hand].pos;
    rot = carriedAngles[hand];
    mirrored = drawnAs[hand].mirrored;
    tip = carriedTip[hand];
    return true;
}

bool view::carriedWeaponInHand(int hand, glm::vec3& handle, glm::vec3& tip)
{
    if(hand < 0 || hand > 1 || !weaponCarried[hand] || !carriedTipValid[hand])
    {
        return false;
    }
    handle = carriedInHand[hand][0];
    tip = carriedInHand[hand][1];
    return true;
}

bool view::heldWeaponLine(int hand, glm::vec3& handle, glm::vec3& tip)
{
    if(hand < 0 || hand > 1)
    {
        return false;
    }
    const view::ViewEntity& w = entities.weapon[hand];
    const int slot = weapons::slotForModel(w.ent.model);
    if(!w.visible || !w.ent.model || w.ent.model->type != mod_alias || slot < 0 || slot == weapons::fistSlot())
    {
        return false;
    }
    handle = drawnAs[hand].pos;
    tip = view::anchorPosition(w, weapons::anchor(slot, Key::MuzzleAnchor),
        weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
    return true;
}

bool view::heldWeaponPoint(int hand, float fraction, float cm, glm::vec3& out)
{
    const view::ViewEntity& w = entities.weapon[hand];
    const int slot = weapons::slotForModel(w.ent.model);
    if(hand < 0 || hand > 1 || !w.ent.model || w.ent.model->type != mod_alias || slot < 0 || slot == weapons::fistSlot())
    {
        return false;
    }
    const glm::vec3 handle = drawnAs[hand].pos;
    const glm::vec3 tip = view::anchorPosition(w, weapons::anchor(slot, Key::MuzzleAnchor),
        weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
    const glm::vec3 at = glm::mix(handle, tip, fraction);
    const glm::vec3 axis = glm::normalize(tip - handle + glm::vec3{0.f, 0.f, 1e-6f});
    // Its drawn triangles' corners within a unit and a half of that point along it: their middle (where a hand holding it
    // there is: inside it, as a handle is in the hand).
    const grasp::Shape* shape = grasp::shapeOf(w.ent, -1);
    if(!shape)
    {
        return false;
    }
    const glm::mat4 m = grasp::shapeToWorld(w.ent, w.mirrored);
    glm::vec3 mid{0.f};
    int n = 0;
    for(const grasp::Triangle& t : shape->tris)
    {
        for(const glm::vec3& p : t.p)
        {
            const glm::vec3 v{m * glm::vec4{p, 1.f}};
            if(za::fabs(glm::dot(v - at, axis)) <= 1.5f)
            {
                mid += v;
                n++;
            }
        }
    }
    out = n ? mid / static_cast<float>(n) : at;
    out.z += cm * 0.01f * units::metresToUnits();
    return true;
}

bool view::heldWeaponButtRegion(int hand, float depth, za::Vector<glm::vec3>& out)
{
    out.clear();
    if(hand < 0 || hand > 1)
    {
        return false;
    }
    const view::ViewEntity& w = entities.weapon[hand];
    const int slot = weapons::slotForModel(w.ent.model);
    if(!w.ent.model || w.ent.model->type != mod_alias || slot < 0 || slot == weapons::fistSlot())
    {
        return false;
    }
    const glm::vec3 handle = drawnAs[hand].pos;
    const glm::vec3 tip = view::anchorPosition(w, weapons::anchor(slot, Key::MuzzleAnchor),
        weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
    const glm::vec3 axis = glm::normalize(tip - handle + glm::vec3{0.f, 0.f, 1e-6f});
    const grasp::Shape* shape = grasp::shapeOf(w.ent, -1);
    if(!shape)
    {
        return false;
    }
    // Its rearmost drawn point along the line, then those within `depth` of it.
    const glm::mat4 m = grasp::shapeToWorld(w.ent, w.mirrored);
    float rear = 1e30f;
    for(const grasp::Triangle& t : shape->tris)
    {
        for(const glm::vec3& p : t.p)
        {
            rear = za::min(rear, glm::dot(glm::vec3{m * glm::vec4{p, 1.f}} - handle, axis));
        }
    }
    for(const grasp::Triangle& t : shape->tris)
    {
        for(const glm::vec3& p : t.p)
        {
            const glm::vec3 v{m * glm::vec4{p, 1.f}};
            if(glm::dot(v - handle, axis) <= rear + depth)
            {
                out.pushBack(v);
            }
        }
    }
    return !out.empty();
}

bool view::heldPropSurface(int hand, za::Vector<glm::vec3>& out)
{
    out.clear();
    const int num = hand == 0 || hand == 1 ? held::heldEntity(hand) : 0;
    if(num <= 0 || num >= cl_max_edicts)
    {
        return false;
    }
    const entity_t& e = cl_entities[num];
    const grasp::Shape* shape = grasp::shapeOf(e, -1);
    if(!shape)
    {
        return false;
    }
    const glm::mat4 m = grasp::shapeToWorld(e, false);
    for(const grasp::Triangle& t : shape->tris)
    {
        for(const glm::vec3& p : t.p)
        {
            out.pushBack(glm::vec3{m * glm::vec4{p, 1.f}});
        }
    }
    return !out.empty();
}

bool view::heldWeaponButt(int hand, glm::vec3& out)
{
    // The middle of its points within a unit of its rearmost end.
    za::Vector<glm::vec3>& region = scratch.buttMiddle;
    if(!heldWeaponButtRegion(hand, 1.f, region))
    {
        return false;
    }
    glm::vec3 mid{0.f};
    for(const glm::vec3& v : region)
    {
        mid += v;
    }
    out = mid / static_cast<float>(region.size());
    return true;
}

bool view::groundHotspotPoint(int entity, int index, glm::vec3& out)
{
    if(entity <= 0 || entity >= cl_max_edicts || index < 0 || index >= weapons::maxHotspots)
    {
        return false;
    }
    const entity_t& e = cl_entities[entity];
    const int slot = e.model && e.model->type == mod_alias ? weapons::slotForModel(e.model) : -1;
    glm::vec3 a, b;
    if(slot < 0 || !groundHotspotZone(e, slot, weapons::hotspot(slot, index), a, b))
    {
        return false;
    }
    out = (a + b) * 0.5f;
    return true;
}

namespace view_detail
{
namespace
{
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
            a[k] = za::remainder(a[k], 360.f);
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
        weaponFingers(held, slot);
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
            // (Y, where its grip ends, kept.)
            h.pos = {length2 > 0.f ? CLAMP(0.f, glm::dot(pos - hilt, along) / length2, 1.f) : 0.f, h.pos.y, 0.f};
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

// Posing the weapon in a holster (posing::HolsterSession): both hands drawn empty at their controllers; the hand whose
// grip took the weapon carries it (its place in that hand taken on the first frame after the grip). The floating holster
// and the weapon in it are drawn by setupHolsters (floatingHolster), which places them on their first frame.
void setupHolsterPosing(hands::State& s)
{
    posing::HolsterSession& h = posing::session().holster;
    setupWeapon(s, HAND_MAIN, viewModel("progs/hand.mdl"), 0);
    setupWeapon(s, HAND_OFF, viewModel("progs/hand.mdl"), 0);
    s.grip2HValid[HAND_OFF] = s.grip2HValid[HAND_MAIN] = false;
    setupHand(s, HAND_MAIN);
    setupHand(s, HAND_OFF);
    if(h.grabHand < 0 || !h.placed)
    {
        return;
    }
    const glm::vec3 p = s.pos[h.grabHand];
    const glm::mat3 r = anglesBasis(s.rot[h.grabHand]);
    if(!h.grabTaken)
    {
        h.grabPos = glm::transpose(r) * (h.weaponPos - p);
        h.grabTurn = glm::transpose(r) * h.weaponTurn;
        h.grabTaken = true;
        return;
    }
    const glm::vec3 pos = p + r * h.grabPos;
    const glm::mat3 turn = r * h.grabTurn;
    h.moved = h.moved || pos != h.weaponPos || turn != h.weaponTurn;
    h.weaponPos = pos;
    h.weaponTurn = turn;
}

// Posing: the floating weapon (in the weapon hand's view entity), the hands (the posing one at its controller; the
// other confirming, or, posing a hotspot, drawn holding the weapon), and the marks. `s` is changed for the view only
// (VR_SetupViewEntities puts it back).
void setupPosing(hands::State& s)
{
    posing::Session& ps = posing::session();
    if(ps.target == posing::Target::Holster)
    {
        setupHolsterPosing(s);
        return;
    }
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
        setupWeapon(s, oh, precachedModel(cl.stats[oh == HAND_MAIN ? int{STAT_WEAPON} : int{STAT_QVR_WEAPONMODEL2}]),
            cl.stats[oh == HAND_MAIN ? int{STAT_WEAPONFRAME} : int{STAT_QVR_WEAPONFRAME2}]);
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
} // namespace

// A drawn holster's stat (`first`: its STAT_QVR_HOLSTERWEAPON*0), at its slot in vr_body's order (bodyHolster): the
// drawn ones' order is another (the open super shotguns' and the magazines' read the shoulders' and hips' flags and
// clips crossed, before round 22).
[[nodiscard]] int holsterStat(int h, int first)
{
    return cl.stats[first + static_cast<int>(bodyHolster[h])];
}

namespace
{
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
} // namespace

// Alias model angles (their pitch inverted) that turn a model's +x to `fwd` and +z to `up`.
[[nodiscard]] glm::vec3 aliasAngles(const glm::vec3& fwd, const glm::vec3& up)
{
    const glm::vec3 a = hands::anglesFromVectors(fwd, up);
    return {-a.x, a.y, a.z};
}

namespace
{
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
    const glm::vec3 muzzle = -up * za::cos(tip) - fwd * za::sin(tip);
    const glm::vec3 top = fwd * za::cos(tip) - up * za::sin(tip);

    return {slotPos, aliasAngles(fwd, up), loops, aliasAngles(muzzle, top)};
}

// The shoulder holsters (reached over the shoulder, vr_shoulder_holster_*): the gun hangs down the back from behind
// the shoulder, its top away from the back, tipped out a little; its grip where the hand reaches for it.
[[nodiscard]] HolsterPose holsterOnBack(const glm::vec3& pos, float yaw, bool left)
{
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, yaw, 0.f}, fwd, right, up);
    const float tip = glm::radians(12.f);
    const glm::vec3 muzzle = -up * za::cos(tip) + (left ? -right : right) * za::sin(tip);
    const glm::vec3 at = pos - fwd * 3.f;
    return {at, glm::vec3{0.f}, at, aliasAngles(muzzle, -fwd)};
}

// Each pair's turn (the Hotspots menu, vr_*_holster_pitch/yaw/roll): degrees, {pitch, yaw, roll}; and crouched, the
// share `crouched` of its crouched turn on top (vr_*_holster_crouch_pitch/yaw/roll; avatar::crouchPoseWeight).
[[nodiscard]] glm::vec3 holsterTurn(int h, float crouched)
{
    switch(h)
    {
        case LeftHip:
        case RightHip:
            return glm::vec3{vr_hip_holster_pitch.value, vr_hip_holster_yaw.value, vr_hip_holster_roll.value} +
                   glm::vec3{vr_hip_holster_crouch_pitch.value, vr_hip_holster_crouch_yaw.value,
                       vr_hip_holster_crouch_roll.value} *
                       crouched;
        case LeftUpper:
        case RightUpper:
            return glm::vec3{vr_upper_holster_pitch.value, vr_upper_holster_yaw.value, vr_upper_holster_roll.value} +
                   glm::vec3{vr_upper_holster_crouch_pitch.value, vr_upper_holster_crouch_yaw.value,
                       vr_upper_holster_crouch_roll.value} *
                       crouched;
        default:
            return glm::vec3{vr_shoulder_holster_pitch.value, vr_shoulder_holster_yaw.value, vr_shoulder_holster_roll.value} +
                   glm::vec3{vr_shoulder_holster_crouch_pitch.value, vr_shoulder_holster_crouch_yaw.value,
                       vr_shoulder_holster_crouch_roll.value} *
                       crouched;
    }
}

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

// Alias model angles as a turn (columns: forward, left, up), and a vector in a holster's frame (out, side, up).
[[nodiscard]] glm::mat3 aliasBasis(const glm::vec3& drawn)
{
    return anglesBasis({-drawn.x, drawn.y, drawn.z}); // alias models' pitch is the other way
}
} // namespace

[[nodiscard]] glm::vec3 inHolsterFrame(const HolsterFrame& f, const glm::vec3& v)
{
    return {glm::dot(v, f.out), glm::dot(v, f.side), glm::dot(v, f.up)};
}

namespace
{
// poseHolstered undone: the Holstered settings that move the weapon from `basePos`, `baseTurn` (as the holster alone
// places it) to `pos`, `turn`. In the frame's axes (out, side, up), holsterRotation(f, {pitch, yaw, roll}) is
// Rz(yaw) Ry(pitch) Rx(-roll) whichever way the frame is handed (a mirrored frame turns both its axes and the sense of
// its turns), so the angles are that product's.
[[nodiscard]] weapons::HolsteredPose holsteredFrom(const HolsterFrame& f, const glm::vec3& basePos, const glm::mat3& baseTurn,
    const glm::vec3& pos, const glm::mat3& turn)
{
    weapons::HolsteredPose p;
    p.offset = inHolsterFrame(f, pos - basePos);
    const glm::mat3 axes{f.out, f.side, f.up};
    const glm::mat3 q = glm::transpose(axes) * turn * glm::transpose(baseTurn) * axes;
    const float pitch = za::asin(CLAMP(-1.f, -q[0][2], 1.f));
    const float yaw = za::atan2(q[0][1], q[0][0]);
    const float roll = -za::atan2(q[1][2], q[2][2]);
    p.angles = glm::degrees(glm::vec3{pitch, yaw, roll});
    return p;
}

// The holster being posed (posing::HolsterSession). On its first frame it is placed in front of the player from where
// the body has it (`pose`, `frame`, `pivot`: the right holster of its kind, the weapon as the holster alone places it),
// turned about the vertical so that its outside faces the player, its point 45 cm ahead of the head and 35 cm below it,
// and the weapon put where its settings place it. Then `pose`, `frame` and `pivot` become the floating ones, the weapon
// where it was left; the candidate is the Holstered settings that put it there. Marks: the holster's point (white) and
// its axes (red off the body, green outwards, blue up).
void floatingHolster(const hands::State& s, HolsterPose& pose, HolsterFrame& frame, glm::vec3& pivot, bool slotDrawn)
{
    posing::Session& ps = posing::session();
    posing::HolsterSession& h = ps.holster;
    const float m2u = units::metresToUnits();
    if(!h.placed)
    {
        const glm::vec3 fwd = hands::forward(glm::vec3{0.f, s.headAngles.y, 0.f});
        glm::vec3 outFlat{frame.out.x, frame.out.y, 0.f};
        float yaw = 0.f;
        if(glm::length(outFlat) > 0.1f)
        {
            outFlat = glm::normalize(outFlat);
            yaw = za::atan2(glm::cross(outFlat, -fwd).z, glm::dot(outFlat, -fwd));
        }
        const glm::mat3 m = glm::mat3_cast(glm::angleAxis(yaw, glm::vec3{0.f, 0.f, 1.f}));
        const glm::vec3 at = s.head + fwd * (0.45f * m2u) - glm::vec3{0.f, 0.f, 0.35f * m2u};
        h.out = m * frame.out;
        h.up = m * frame.up;
        h.side = m * frame.side;
        h.pivot = at;
        h.basePos = at + m * (pose.weaponPos - pivot);
        h.baseTurn = m * aliasBasis(pose.weaponAngles);
        h.slotPos = at + m * (pose.slotPos - pivot);
        h.slotTurn = m * aliasBasis(pose.slotAngles);
        h.slotDrawn = slotDrawn;
        h.tiltAxis = glm::vec3{-fwd.y, fwd.x, 0.f}; // across the view, to the left
        h.placed = true;
        h.fromSettings = true;
        h.moved = false;
    }
    frame = {h.out, h.up, h.side};
    if(h.fromSettings)
    {
        const weapons::HolsteredPose p = weapons::holsteredPose(ps.slot, h.kind);
        h.weaponPos = h.basePos + frame.out * p.offset.x + frame.side * p.offset.y + frame.up * p.offset.z;
        h.weaponTurn = glm::mat3_cast(holsterRotation(frame, p.angles)) * h.baseTurn;
        h.fromSettings = false;
        h.grabTaken = false; // carried: taken again from where it is now
    }
    pivot = h.pivot;
    pose = {h.slotPos, aliasAngles(h.slotTurn[0], h.slotTurn[2]), h.weaponPos, aliasAngles(h.weaponTurn[0], h.weaponTurn[2])};

    posing::Candidate& c = posing::candidate();
    c.holstered = holsteredFrom(frame, h.basePos, h.baseTurn, h.weaponPos, h.weaponTurn);
    c.inHolsterPos = inHolsterFrame(frame, h.weaponPos - h.pivot);
    c.inHolsterFwd = inHolsterFrame(frame, h.weaponTurn[0]);
    c.inHolsterUp = inHolsterFrame(frame, h.weaponTurn[2]);
    c.valid = true;

    const float axis = 0.12f * m2u;
    lines::point(h.pivot, 1.2f, glm::vec4{1.f});
    lines::line(h.pivot, h.pivot + frame.out * axis, 0.12f, glm::vec4{1.f, 0.25f, 0.2f, 0.9f}, glm::vec4{1.f, 0.25f, 0.2f, 0.f});
    lines::line(h.pivot, h.pivot + frame.side * axis, 0.12f, glm::vec4{0.3f, 1.f, 0.35f, 0.9f}, glm::vec4{0.3f, 1.f, 0.35f, 0.f});
    lines::line(h.pivot, h.pivot + frame.up * axis, 0.12f, glm::vec4{0.3f, 0.5f, 1.f, 0.9f}, glm::vec4{0.3f, 0.5f, 1.f, 0.f});
    // Where the body would be (behind the holster): a faint line into it.
    lines::line(h.pivot, h.pivot - frame.out * (2.f * axis), 0.25f, glm::vec4{1.f, 1.f, 1.f, 0.25f}, glm::vec4{1.f, 1.f, 1.f, 0.f});
}
// Each holster drew the weapon it holds last frame (not empty, not a stand-in: the Weapon Offsets preview, a posing
// session): its blood is drawn on it (view::holsteredWeapon, vr_wounds.cpp).
bool holsterLive[HolsterCount]{};

// A holstered grappling gun (`model`, drawn at `at`) whose hook is out: a rope starts at it (the hook's own beam, from
// the server's holster place). Drawn empty then (frame 2; 0 is the hook in it).
[[nodiscard]] bool holsteredGrappleOut(const qmodel_t* model, const glm::vec3& at)
{
    if(!model || !modelmeta::is(model, modelmeta::Id::VGrpple))
    {
        return false;
    }
    bool out = false;
    for(const beam_t& b : cl_beams)
    {
        out = out || (b.model && b.starttime <= cl.time && b.endtime >= cl.time && (b.entity & 0xFFFF) != cl.viewentity &&
                         glm::distance(glm::vec3{b.start[0], b.start[1], b.start[2]}, at) < 24.f);
    }
    bool& was = grappleDebug.holsteredOut;
    if(out != was && vr_grapple_debug.value && developer.value)
    {
        Con_Printf("grapple: the holstered gun drawn %s\n", out ? "empty (its hook out)" : "with its hook");
    }
    was = out;
    return out;
}

// A gun not in a hand (holstered, lying in the world) carries its button and ammo screen as a held one does
// (vr_weapon_screen_idle), so that they do not pop up as it is taken: `button` placed (or hidden), and a grappling gun's
// front button, `front` (setupFrontButton); the screen queued when `queueText` (once a frame). `clip`: its clip if known
// (a holstered gun's), else -1.
void idleAttachments(const entity_t& e, bool mirrored, int slot, view::ViewEntity& button, view::ViewEntity& front, int clip,
    bool queueText, bool total = true)
{
    const bool on = vr_weapon_screen_idle.value != 0.f && slot >= 0 && e.model && !isHandModel(e.model) &&
                    slot != weapons::fistSlot();
    const glm::vec3 drawn{e.angles[0], e.angles[1], e.angles[2]};
    if(on && weapons::value(slot, Key::WpnButtonMode) != 0.f)
    {
        const glm::vec3 pos = view::anchorPosition(e, mirrored, 0.f,
            weapons::anchor(slot, Key::WpnButtonAnchor),
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
    if(button.visible && vr_grapple_front_button.value && modelmeta::is(e.model, modelmeta::Id::VGrpple))
    {
        const glm::vec3 pos = view::anchorPosition(e, mirrored, 0.f,
            weapons::anchor(slot, Key::WpnButtonAnchor),
            weapons::vec(slot, Key::WpnButtonX, Key::WpnButtonY, Key::WpnButtonZ) +
                glm::vec3{vr_grapple_front_button_x.value, vr_grapple_front_button_y.value, vr_grapple_front_button_z.value});
        place(front, viewModel("progs/wpnbutton.mdl"), pos,
            glm::vec3{button.ent.angles[0], button.ent.angles[1], button.ent.angles[2]}, 0, mirrored);
        front.ent.alpha = e.alpha;
        double& logAt = grappleDebug.idleLogAt;
        if(vr_grapple_debug.value >= 3 && developer.value && vr_gametime >= logAt)
        {
            logAt = vr_gametime + 0.5;
            Con_Printf("grapple: a gun not in a hand: its back button at %.1f %.1f %.1f, the front one at %.1f %.1f %.1f\n",
                static_cast<double>(button.ent.origin[0]), static_cast<double>(button.ent.origin[1]),
                static_cast<double>(button.ent.origin[2]), static_cast<double>(pos.x), static_cast<double>(pos.y),
                static_cast<double>(pos.z));
        }
    }
    else
    {
        front.visible = false;
    }

    if(on && queueText && vr_show_weapon_text.value && weapons::value(slot, Key::WpnTextMode) != 0.f)
    {
        const glm::vec3 pos = view::anchorPosition(e, mirrored, 0.f,
            weapons::anchor(slot, Key::WpnTextAnchor),
            weapons::vec(slot, Key::WpnTextX, Key::WpnTextY, Key::WpnTextZ));
        glm::vec3 angles = weapons::vec(slot, Key::WpnTextPitch, Key::WpnTextYaw, Key::WpnTextRoll);
        if(mirrored)
        {
            angles.z = -angles.z;
        }
        text3d::queue(idleWeaponText(e.model, clip, total), pos, idleAttachmentAngles(drawn, slot, mirrored, angles),
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

    // Posing the weapon in a holster (vr_posing.cpp): the right holster of that kind floats in front of the player, the
    // posed weapon in it (floatingHolster).
    const bool holsterPosing = posing::active() && posing::session().target == posing::Target::Holster;
    const weapons::HolsterKind posedKind = posing::session().holster.kind;
    const int posedHolster = !holsterPosing                                     ? -1
                             : posedKind == weapons::HolsterKind::Shoulder ? int{RightShoulder}
                             : posedKind == weapons::HolsterKind::Upper    ? int{RightUpper}
                                                                           : int{RightHip};

    // Weapon Offsets > Holstered, a setting chosen: the page's weapon (the one its hand holds) previewed in the holsters.
    int previewHand = 0, previewKind = 0;
    qmodel_t* previewModel = nullptr;
    const bool preview = menu::holsterPreview(previewHand, previewKind) &&
                         (previewModel = weapons::heldModel(previewHand)) != nullptr;

    body::HolsterPlates plates;
    const body::HolsterPositions positions = body::holsterPositions(s, &plates); // one body solve for all
    const float crouched = avatar::crouchPoseShare(s);                            // (their crouched turns)
    // Dead: none drawn, the guns nor the sleeves (body::gearHiddenForDeath).
    const bool deadHidden = body::gearHiddenForDeath();
    qmodel_t* const slotModel =
        vr_leg_holster_model_enabled.value && !deadHidden ? viewModel("progs/legholster.mdl") : nullptr;
    for(int h = 0; h < HolsterCount; h++)
    {
        const bool shoulder = h == LeftShoulder || h == RightShoulder;
        const bool mirrored = h == LeftHip || h == LeftUpper || h == LeftShoulder;
        const glm::vec3 pos = positions[static_cast<za::SizeT>(bodyHolster[h])];
        const bool posedHere = h == posedHolster;
        const bool hover = hovered(s, bodyHolster[h]) && !posedHere;

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
        if(body::HolsterPlate plate = plates[static_cast<za::SizeT>(bodyHolster[h])]; !shoulder && plate.out != glm::vec3{0.f})
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
        turnHolster(pose, pivot, frame, holsterTurn(h, crouched));
        if(posedHere)
        {
            floatingHolster(s, pose, frame, pivot, slotModel && !shoulder);
        }

        const int stat = static_cast<int>(bodyHolster[h]);
        qmodel_t* model = precachedModel(cl.stats[STAT_QVR_HOLSTERWEAPONMODEL0 + stat]);
        const bool occupied = model != nullptr && !isHandModel(model);
        int clip = cl.stats[STAT_QVR_HOLSTERWEAPONCLIP0 + stat];
        const weapons::HolsterKind kind = shoulder ? weapons::HolsterKind::Shoulder
                                          : h == LeftUpper || h == RightUpper ? weapons::HolsterKind::Upper
                                                                               : weapons::HolsterKind::Hip;
        const bool previewHere = preview && kind == static_cast<weapons::HolsterKind>(previewKind);
        if(previewHere)
        {
            model = previewModel; // Weapon Offsets > Holstered: the page's weapon, in both holsters of the kind edited
            clip = -1;
        }
        if(posedHere)
        {
            model = posing::session().model; // where it was left (floatingHolster)
            clip = -1;
        }
        // The pair's guns not drawn by choice (vr_holster_weapons_draw_*): as dead, none of it, though they are taken
        // and put away there as ever. A gun drawn from it appears in the hand (no draw blend from an unseen pose).
        const cvar_t& drawPair = kind == weapons::HolsterKind::Shoulder ? vr_holster_weapons_draw_shoulder
                                 : kind == weapons::HolsterKind::Upper  ? vr_holster_weapons_draw_upper
                                                                        : vr_holster_weapons_draw_hip;
        const bool pairHidden = drawPair.value == 0.f;
        if(isHandModel(model) || ((deadHidden || pairHidden) && !previewHere && !posedHere))
        {
            model = nullptr;
        }
        const glm::vec3 loops = pose.weaponPos; // (the holster itself, before its gun's own offset)
        if(!posedHere && vr_body_debug.value < 2.f)
        {
            // Its zone (vr_holsterzones.hpp) where it is drawn: the turned frame, the loops.
            zones::publish(stat, pos, frame.out, frame.side, frame.up, loops, occupied);
        }
        if(!posedHere)
        {
            poseHolstered(pose, frame, weapons::holsteredPose(weapons::slotForModel(model), kind));
        }

        view::ViewEntity& ve = entities.holster[h];
        place(ve, model, pose.weaponPos, pose.weaponAngles, holsteredGrappleOut(model, pose.weaponPos) ? 2 : 0, mirrored);
        highlight(ve, hover);
        // Just holstered: eased from the hand into the holster (vr_drawblend.cpp).
        drawblend::holster(s, stat, model ? &ve.ent : nullptr, !previewHere && !posedHere);
        holsterLive[h] = model != nullptr && !previewHere && !posedHere;
        holsterSeen[h] = {model != nullptr, posedHere, kind, weapons::slotForModel(model), pivot, frame, pose.weaponPos,
            aliasBasis(pose.weaponAngles), loops, host_framecount};

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
        idleAttachments(ve.ent, mirrored, slot, entities.holsterButton[h], entities.holsterFrontButton[h], clip,
            queueTexts && model != nullptr);
        if(model && slot >= 0 && slot != weapons::fistSlot() && occupied && !posedHere && !previewHere &&
            vr_body_debug.value < 2.f)
        {
            // Its grip as drawn (where the hand holding it is drawn on it: Hand Anchor and Offset) is the zone's too
            // (its handle sphere, vr_holster_zone_handle_radius).
            zones::publishGrip(stat, view::anchorPosition(ve, weapons::anchor(slot, Key::HandAnchor),
                                         weapons::vec(slot, Key::HandOffsetX, Key::HandOffsetY, Key::HandOffsetZ)));
        }
        highlight(entities.holsterButton[h], hover);
        highlight(entities.holsterFrontButton[h], hover);
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
    // Dead: not drawn, as the holsters and the ammo pouch (body::gearHiddenForDeath).
    qmodel_t* const model =
        body::pouchEnabled() && !body::gearHiddenForDeath() ? viewModel("progs/vrpouch.mdl") : nullptr;
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
    if(vr_body_debug.value < 2.f)
    {
        zones::publish(zones::ZoneGrenadePouch, pos, frame.out, frame.side, frame.up, pose.slotPos, false);
    }
    place(ve, model, pose.slotPos, pose.slotAngles, cl.stats[STAT_ROCKETS] >= 1 ? 0 : 1, false);
    highlight(ve, s.hotspot[HAND_OFF] == body::HS_GRENADE_POUCH || s.hotspot[HAND_MAIN] == body::HS_GRENADE_POUCH);
}

// The magazine in the gun `gun` (`show`: the server's mode Immersive and the gun's magazine in): its attached model, the
// gun's place, turn, mirroring, scale and light, moved with the gun's body by the firing animation (magMounts).
void setMagazine(view::ViewEntity& mag, const view::ViewEntity& gun, bool show, bool well = false)
{
    const MagMount* mount = show && gun.visible ? magMountFor(gun.ent.model) : nullptr;
    qmodel_t* const model = mount ? viewModel(well ? mount->well : mount->model) : nullptr;
    if(!model)
    {
        mag.visible = false;
        return;
    }
    const qmodel_t* last = mag.lastModel;
    mag = gun;
    mag.ent.model = model;
    mag.ent.frame = 0;
    mag.ent.lerpflags |= LERP_RESETANIM; // (one frame: never blended with the gun's)
    mag.zeroBlend = 0.f;
    mag.morph = 0.f;
    mag.lastModel = last;
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(gun.ent.model));
    const glm::vec3 k{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
    const glm::vec3 o{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
    const glm::vec3 now = anchor::posedAnchor(gun.ent, mount->anchor, gun.zeroBlend) * k + o;
    const glm::vec3 rest = gun.zeroBlend == 1.f ? now : anchor::posedAnchor(gun.ent, mount->anchor, 1.f) * k + o;
    const view::ModelSpace gunSpace{gun}; // (the gun's points below: its matrix made once)
    const glm::vec3 kick = gunSpace.point(now) - gunSpace.point(rest);
    // The weapon's Scale applies about each model's own bounds' corner (its header's origin: vr_render.cpp applyPost),
    // which the magazine's model doesn't share with the gun's: the point that is the gun's model-space origin drawn where
    // the gun draws it (the rest follows: the two differ by a move only).
    const glm::vec3 corner = gunSpace.point(glm::vec3{0.f}) - view::modelPoint(mag, glm::vec3{0.f});
    for(int i = 0; i < 3; i++)
    {
        mag.ent.origin[i] += kick[i] + corner[i];
    }
    if(!well)
    {
        return;
    }
    // The well's own offsets (vr_reload_well_<gun>_*, visual only): turned in the gun's frame about its seat, then its seat
    // moved (model units).
    const glm::vec3 move{mount->wellOffset[0]->value, mount->wellOffset[1]->value, mount->wellOffset[2]->value};
    glm::vec3 turn{mount->wellOffset[3]->value, mount->wellOffset[4]->value, mount->wellOffset[5]->value};
    if(turn != glm::vec3{0.f})
    {
        if(gun.mirrored)
        {
            turn.y = -turn.y;
            turn.z = -turn.z;
        }
        const glm::vec3 view{-mag.ent.angles[0], mag.ent.angles[1], mag.ent.angles[2]};
        const glm::vec3 turned = composeAngles(view, turn);
        mag.ent.angles[0] = -turned.x;
        mag.ent.angles[1] = turned.y;
        mag.ent.angles[2] = turned.z;
    }
    const glm::vec3 to = gunSpace.point(mount->seat + move) + kick - view::modelPoint(mag, mount->seat);
    for(int i = 0; i < 3; i++)
    {
        mag.ent.origin[i] += to[i];
    }
    if(vr_reload_show_ports.value)
    {
        lines::point(view::modelPoint(mag, mount->seat), 0.4f, glm::vec4{0.2f, 0.9f, 1.f, 0.9f}); // (cyan: its seat)
    }
}

// One of the open super shotgun's parts (`model`) where the gun `gun` is drawn (its frame, its lerp, its kick: a copy),
// turned down `deg` degrees about the hinge (the barrels; 0 the frame), with the skin `skin`.
void setSsgPart(view::ViewEntity& part, const view::ViewEntity& gun, const char* model, float deg, int skin)
{
    qmodel_t* const m = viewModel(model);
    if(!m)
    {
        part.visible = false;
        return;
    }
    const qmodel_t* last = part.lastModel;
    part = gun;
    part.ent.model = m;
    part.ent.skinnum = skin;
    // At rest (open, it can't fire): never the gun's firing frames, whose blending, frozen while the gun itself isn't
    // drawn, would hold its muzzle flash (the author's note, vrfiringrange_2026-10-07_22-07-20).
    part.ent.frame = 0;
    part.ent.lerpflags |= LERP_RESETANIM;
    part.zeroBlend = 0.f;
    part.lastModel = last;
    // (The parts share the gun's model header, but its Scale applies about each model's own corner: as setMagazine.)
    const glm::vec3 corner = view::modelPoint(gun, glm::vec3{0.f}) - view::modelPoint(part, glm::vec3{0.f});
    for(int i = 0; i < 3; i++)
    {
        part.ent.origin[i] += corner[i];
    }
    if(deg == 0.f)
    {
        return;
    }
    // Turned in the gun's frame about its +y (pitch: the muzzle down, unchanged by the mirroring), then moved so the
    // hinge stays where the gun has it.
    const glm::vec3 view{-part.ent.angles[0], part.ent.angles[1], part.ent.angles[2]};
    const glm::vec3 turned = composeAngles(view, glm::vec3{deg, 0.f, 0.f});
    part.ent.angles[0] = -turned.x;
    part.ent.angles[1] = turned.y;
    part.ent.angles[2] = turned.z;
    const glm::vec3 to = view::modelPoint(gun, ssgHinge) - view::modelPoint(part, ssgHinge);
    for(int i = 0; i < 3; i++)
    {
        part.ent.origin[i] += to[i];
    }
}

// Every frame, after the guns are placed: the super shotguns broken open (immersive reloading, phase 2b), in the hands
// (their drawn angle stepped by setupWeapon) and the holsters (as they are), in their two parts; the gun itself is then
// not drawn (drawnInParts). The barrels' skin: how many chambers are loaded.
void setupSsgParts()
{
    for(int hand = 0; hand < 2; hand++)
    {
        const view::ViewEntity& gun = entities.weapon[hand];
        SsgDrawn& st = ssgHands[hand];
        st.parts = gun.visible && st.gun == gun.ent.model && st.angle > 0.f;
        if(!st.parts)
        {
            entities.ssgFrame[hand].visible = entities.ssgBarrels[hand].visible = false;
            continue;
        }
        const int clip = cl.stats[hand == HAND_MAIN ? protocol::STAT_QVR_WEAPONCLIP : protocol::STAT_QVR_WEAPONCLIP2];
        setSsgPart(entities.ssgFrame[hand], gun, ssgFrameModel, 0.f, 0);
        setSsgPart(entities.ssgBarrels[hand], gun, ssgBarrelsModel, st.angle, CLAMP(0, clip, 2));
        if(vr_reload_debug.value >= 2 && developer.value)
        {
            // (The barrels as drawn against the gun's turned model space: the breech's middle, both ways.)
            const glm::vec3 breech{12.7f, 0.f, 7.f};
            const glm::vec3 a = view::modelPoint(entities.ssgBarrels[hand], breech);
            const glm::vec3 b = view::modelPoint(gun, ssgTurn(breech, st.angle));
            Con_Printf("ssg: hand %d open %.1f deg, its breech %.2f %.2f %.2f (turned %.2f off)\n", hand,
                static_cast<double>(st.angle), static_cast<double>(a.x), static_cast<double>(a.y),
                static_cast<double>(a.z), static_cast<double>(glm::distance(a, b)));
        }
    }
    for(int h = 0; h < HolsterCount; h++)
    {
        const view::ViewEntity& gun = entities.holster[h];
        SsgDrawn& st = ssgHolsters[h];
        const float deg =
            ssgStep(st, gun.visible ? gun.ent.model : nullptr, holsterStat(h, protocol::STAT_QVR_HOLSTERWEAPONFLAGS0));
        st.parts = gun.visible && deg > 0.f;
        if(!st.parts)
        {
            entities.holsterSsgFrame[h].visible = entities.holsterSsgBarrels[h].visible = false;
            continue;
        }
        setSsgPart(entities.holsterSsgFrame[h], gun, ssgFrameModel, 0.f, 0);
        setSsgPart(entities.holsterSsgBarrels[h], gun, ssgBarrelsModel, deg,
            CLAMP(0, holsterStat(h, protocol::STAT_QVR_HOLSTERWEAPONCLIP0), 2));
    }
}

// The super shotguns lying about broken open (a prop's U_QVR_SSGOPEN; the author's note vrfiringrange_2026-10-07_22-08-37:
// a dropped gun shows its state): drawn open as in the hands, at the open angle, the barrels' skin its loaded chambers,
// in place of their own entities (taken out of this frame's list; the first maxWorldSsgs of them).
void setupWorldSsgs()
{
    int n = 0;
    const bool on = ssgBreaks();
    int kept = 0;
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        entity_t* const e = cl_visedicts[i];
        const bool numbered = on && e && e >= cl_entities && e < cl_entities + cl_max_edicts;
        const client::EntityVr* net = numbered ? client::entityVr(static_cast<int>(e - cl_entities)) : nullptr;
        if(n < maxWorldSsgs && net && net->ssgOpen && e->model && e->model->type == mod_alias &&
           modelmeta::is(e->model, modelmeta::Id::VShot2) && viewModel(ssgFrameModel) && viewModel(ssgBarrelsModel))
        {
            view::ViewEntity gun;
            gun.ent = *e;
            gun.ent.frame = 0;
            gun.visible = true;
            gun.netEntity = static_cast<int>(e - cl_entities); // (drawn with its networked scale and offset, as it is)
            setSsgPart(entities.worldSsgFrame[n], gun, ssgFrameModel, 0.f, 0);
            setSsgPart(entities.worldSsgBarrels[n], gun, ssgBarrelsModel, CLAMP(0.f, vr_reload_ssg_open_angle.value, 80.f),
                CLAMP(0, net->ssgLoaded, 2));
            worldSsgSource[n] = e;
            if(vr_reload_debug.value >= 1 && developer.value && cl.time >= worldSsgPrinted + 1.0)
            {
                worldSsgPrinted = cl.time;
                Con_Printf("ssg: a super shotgun lying open (entity %d), %d loaded, drawn in its parts\n",
                    static_cast<int>(e - cl_entities), net->ssgLoaded);
            }
            n++;
            continue; // (not drawn itself)
        }
        cl_visedicts[kept++] = e;
    }
    cl_numvisedicts = kept;
    for(int i = n; i < maxWorldSsgs; i++)
    {
        entities.worldSsgFrame[i].visible = entities.worldSsgBarrels[i].visible = false;
        worldSsgSource[i] = nullptr;
    }
}

// Whether `ve` is a super shotgun drawn in its parts this frame (setupSsgParts): not drawn itself.
[[nodiscard]] bool drawnInParts(const view::ViewEntity& ve)
{
    for(int hand = 0; hand < 2; hand++)
    {
        if(&ve == &entities.weapon[hand])
        {
            return ssgHands[hand].parts;
        }
    }
    for(int h = 0; h < HolsterCount; h++)
    {
        if(&ve == &entities.holster[h])
        {
            return ssgHolsters[h].parts;
        }
    }
    return false;
}

// The magazines in the guns in the hands (as setupMagazines): placed right after the guns (a hand holding one wraps it),
// and again with the rest.
void setupHeldMagazines()
{
    const bool on = cl.stats[protocol::STAT_QVR_RELOADMODE] == 3;
    for(int hand = 0; hand < 2; hand++)
    {
        const int flags = cl.stats[hand == HAND_MAIN ? protocol::STAT_QVR_WEAPONFLAGS : protocol::STAT_QVR_WEAPONFLAGS2];
        setMagazine(entities.mag[hand], entities.weapon[hand], on && !(flags & weaponFlagNoMag));
        setMagazine(entities.well[hand], entities.weapon[hand], on, true);
    }
}

// Every frame, after the guns are placed: their magazines (immersive reloading: the server's mode, the guns' flags; a
// lying gun's U_QVR_NOMAG); with Show Load Points, a held magazine's reference point and radius (orange).
void setupMagazines()
{
    const bool on = cl.stats[protocol::STAT_QVR_RELOADMODE] == 3;
    if(on && vr_reload_show_ports.value)
    {
        for(int hand = 0; hand < 2; hand++)
        {
            glm::vec3 ref;
            float radius = 0.f;
            if(view::heldRoundRef(hand, ref, &radius) && radius >= 0.f)
            {
                lines::point(ref, 0.5f, glm::vec4{1.f, 0.6f, 0.1f, 0.9f});
                lines::point(ref, 2.f * radius, glm::vec4{1.f, 0.5f, 0.1f, 0.3f});
            }
        }
    }
    setupHeldMagazines();
    for(int h = 0; h < HolsterCount; h++)
    {
        const int flags = holsterStat(h, protocol::STAT_QVR_HOLSTERWEAPONFLAGS0);
        setMagazine(entities.holsterMag[h], entities.holster[h], on && !(flags & weaponFlagNoMag));
        setMagazine(entities.holsterWell[h], entities.holster[h], on, true);
    }
    const bool printMags = vr_reload_debug.value >= 1 && developer.value && cl.time >= worldMagPrinted + 1.0;
    if(printMags)
    {
        worldMagPrinted = cl.time;
    }
    for(int i = 0; i < maxWorldWeapons; i++)
    {
        const entity_t* e = worldWeaponsNear[i];
        const bool numbered = e && e >= cl_entities && e < cl_entities + cl_max_edicts;
        const client::EntityVr* net = numbered ? client::entityVr(static_cast<int>(e - cl_entities)) : nullptr;
        if(!e || !on)
        {
            entities.worldMag[i].visible = entities.worldWell[i].visible = false;
            continue;
        }
        view::ViewEntity gun;
        gun.ent = *e;
        gun.visible = true;
        gun.netEntity = static_cast<int>(e - cl_entities); // (its model_offset: a map's spinning pickup; the magazine too)
        setMagazine(entities.worldMag[i], gun, !(net && net->noMag));
        setMagazine(entities.worldWell[i], gun, true, true);
        const MagMount* mount = magMountFor(e->model);
        if(mount && entities.worldMag[i].visible && printMags)
        {
            // Its magazine's seat as the renderer draws it (the magazine's entity) against the gun's own (the world entity:
            // its networked offset; the author's note e1m1_2026-10-07_22-39-10, a spinning pickup's came off).
            const glm::vec3 zero{0.f};
            const auto drawn = [&](const entity_t& ent) {
                float m[16];
                render::entityMatrix(ent, false, ENTSCALE_DEFAULT, zero, m);
                const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(ent.model));
                const glm::vec3 v{(mount->seat.x - hdr->scale_origin[0]) / hdr->scale[0],
                    (mount->seat.y - hdr->scale_origin[1]) / hdr->scale[1], (mount->seat.z - hdr->scale_origin[2]) / hdr->scale[2]};
                return glm::vec3{m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12], m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
                    m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
            };
            Con_Printf("reload: a lying %s's magazine (entity %d%s): its seat drawn %.2f units off the gun's\n", e->model->name,
                gun.netEntity, net && net->spin ? ", spinning" : "", glm::distance(drawn(entities.worldMag[i].ent), drawn(*e)));
        }
    }
}

// The renderer's frame blending of an alias entity (the firing animation's), from one entity to another.
void copyLerp(entity_t& to, const entity_t& from)
{
    to.lerpflags = from.lerpflags;
    to.lerpstart = from.lerpstart;
    to.lerptime = from.lerptime;
    to.lerpfinish = from.lerpfinish;
    to.animlerpfinish = from.animlerpfinish;
    to.previouspose = from.previouspose;
    to.currentpose = from.currentpose;
    to.movelerpstart = from.movelerpstart;
    to.movelerpfinish = from.movelerpfinish;
    for(int i = 0; i < 3; i++)
    {
        to.previousorigin[i] = from.previousorigin[i];
        to.currentorigin[i] = from.currentorigin[i];
        to.previousangles[i] = from.previousangles[i];
        to.currentangles[i] = from.currentangles[i];
    }
}

// A part of the gun `gun` drawn as the gun (made in its model space): a copy of its entity (place, turn, mirroring,
// frame, blending, light, Scale and offsets: vr_weapons.cpp makeModelTransform) with `model`, moved `back` model units
// along the gun's -x. lastModel: the gun's model, whose copy it is (setupPumps).
void setGunPart(view::ViewEntity& part, const view::ViewEntity& gun, qmodel_t* model, float back)
{
    part = gun;
    part.ent.model = model;
    part.lastModel = gun.ent.model;
    // (The Scale applies about each model's own header origin: the same here, but as setMagazine does.)
    const view::ModelSpace gunSpace{gun};
    const glm::vec3 gunOrigin = gunSpace.point(glm::vec3{0.f});
    const glm::vec3 corner = gunOrigin - view::modelPoint(part, glm::vec3{0.f});
    const glm::vec3 slide = back != 0.f ? gunSpace.point(glm::vec3{-back, 0.f, 0.f}) - gunOrigin : glm::vec3{0.f};
    for(int i = 0; i < 3; i++)
    {
        part.ent.origin[i] += corner[i] + slide[i];
    }
}

// Every frame, after the guns are placed: the shotgun's auto pump (vr_autopump.cpp). While a hand's shotgun's fore-end
// is off its rest (a stroke after a shot; vr_autopump_hold), the gun is drawn as the gun without it
// (progs/vr_pumpbody_on_v_shot.mdl, in its place) and the fore-end (progs/vr_pump_on_v_shot.mdl) slid back; the gun's
// own entity is not drawn then (drawnAsPump). The renderer moves an entity's frame blending on as it draws it: the
// body's, copied from the gun's, is copied back to the gun's each frame (the anchors read it: the hands, the muzzle).
// Not for a shotgun model other than Quake VR's (a mod's: other frames or header).
void setupPumps()
{
    bool isShotgun[2]{};
    float where[2][3]{};
    for(int hand = 0; hand < 2; hand++)
    {
        view::ViewEntity& gun = entities.weapon[hand];
        view::ViewEntity& body = entities.pumpBody[hand];
        view::ViewEntity& pump = entities.pump[hand];
        const bool shotgun = gun.visible && gun.ent.model && gun.ent.model->type == mod_alias &&
                             modelmeta::get(gun.ent.model).is(modelmeta::Id::VShot);
        isShotgun[hand] = shotgun;
        if(shotgun)
        {
            const glm::vec3 p = view::modelPoint(gun, glm::vec3{24.f, 0.f, 3.f}); // (the fore-end's middle)
            where[hand][0] = p.x;
            where[hand][1] = p.y;
            where[hand][2] = p.z;
        }
        if(body.visible && body.lastModel == gun.ent.model)
        {
            copyLerp(gun.ent, body.ent);
        }
        const float back = shotgun ? autopump::travel(hand) : 0.f;
        qmodel_t* const bodyModel = back > 0.f ? viewModel(pumpBodyModelName) : nullptr;
        qmodel_t* const pumpModel = bodyModel ? viewModel(pumpModelName) : nullptr;
        bool fits = bodyModel && pumpModel && bodyModel->type == mod_alias && pumpModel->type == mod_alias;
        if(fits)
        {
            const auto* g = static_cast<const aliashdr_t*>(Mod_Extradata(gun.ent.model));
            for(qmodel_t* m : {bodyModel, pumpModel})
            {
                const auto* h = static_cast<const aliashdr_t*>(Mod_Extradata(m));
                fits = fits && h->numframes == g->numframes && h->numposes == g->numposes &&
                       VectorCompare(h->scale, g->scale) && VectorCompare(h->scale_origin, g->scale_origin);
            }
        }
        if(!fits)
        {
            body.visible = pump.visible = false;
            continue;
        }
        setGunPart(body, gun, bodyModel, 0.f);
        setGunPart(pump, gun, pumpModel, back);
        if(vr_debug_weaponfx.value >= 2.f)
        {
            Con_Printf("autopump hand %d t %.3f travel %.3f\n", hand, cl.time, back);
        }
    }
    autopump::frame(isShotgun, where);
}

// The gun entity of a hand whose shotgun is drawn as its auto pump's two parts (setupPumps).
[[nodiscard]] bool drawnAsPump(const view::ViewEntity& ve)
{
    for(int hand = 0; hand < 2; hand++)
    {
        if(&ve == &entities.weapon[hand] && entities.pumpBody[hand].visible)
        {
            return true;
        }
    }
    return false;
}

// The ammo pouch (immersive reloading, vr_reload_mode 3; docs/vr-port/RELOAD.md): vrpouch_ammo.mdl (make_ammo_pouch.py:
// as vrpouch.mdl, +x out of the body, its back at the origin) on the front of the belt between the hip holsters, facing
// the belly's surface there (straight forward without the body), turned by vr_ammo_pouch_pitch/yaw/roll about where the
// hand reaches for it, scaled by vr_ammo_pouch_scale about its back. Its frame shows what it gives and how much is
// left (ammoPouchFrame); lit up while a hand is at it.
// The ammo pouch's frame (make_ammo_pouch.py frame_spec): what it gives (STAT_QVR_POUCHKIND) and how much of it is left
// (STAT_QVR_POUCHCOUNT): a shell in sight each up to 5, a magazine (part-filled counting) up to 3 (the super nailgun's
// 2); 0 empty.
[[nodiscard]] int ammoPouchFrame()
{
    struct Kind
    {
        int first, most, each; // its frames' first, the most it shows, the rounds one shows
    };
    // (Shells, nailgun and super nailgun magazines, cells; the launchers' rounds, one a round: rockets, grenades,
    // proximity grenades. make_ammo_pouch.py's KINDS.)
    constexpr Kind kinds[] = {{1, 5, 1}, {6, 3, 24}, {9, 2, 36}, {11, 3, 36}, {14, 3, 1}, {17, 4, 1}, {21, 4, 1}};
    // (8: the lava nails' or plasma's, the multi-rockets' or multi-grenades': its skin 1)
    const int kind = cl.stats[protocol::STAT_QVR_POUCHKIND] & 7;
    const int left = cl.stats[protocol::STAT_QVR_POUCHCOUNT];
    if(kind < 1 || kind > 7 || left <= 0)
    {
        return 0;
    }
    const Kind& k = kinds[kind - 1];
    return k.first + za::min(k.most, (left + k.each - 1) / k.each) - 1;
}
constexpr float pouchGrenadeX = 1.35f; // their middles out of the body: the pouch's middle (make_pouch.py BACK_X + full DEPTH / 2)

void setupAmmoPouchGrenades(const view::ViewEntity& pouch)
{
    const int kind = cl.stats[protocol::STAT_QVR_POUCHKIND];
    const int left = cl.stats[protocol::STAT_QVR_POUCHCOUNT];
    const PouchGrenades* g = !pouch.visible || left <= 0 ? nullptr
                             : (kind & 7) == 6           ? &pouchGrenades[(kind & 8) ? 1 : 0]
                             : (kind & 7) == 7           ? &pouchGrenades[2]
                                                         : nullptr;
    qmodel_t* const model = g ? viewModel(g->model) : nullptr;
    const int n = model ? za::min(left, 3) : 0;
    for(int j = 0; j < 3; j++)
    {
        view::ViewEntity& ve = entities.ammoPouchGrenade[j];
        if(j >= n)
        {
            ve.visible = false;
            continue;
        }
        // The pouch's axes and scale where it stands (its model space to the world).
        const glm::vec3 at{pouchGrenadeX, (static_cast<float>(j) - static_cast<float>(n - 1) * 0.5f) * g->gap, g->top};
        const glm::vec3 o = modelPoint(pouch, at);
        const glm::vec3 ax = modelPoint(pouch, at + glm::vec3{1.f, 0.f, 0.f}) - o;
        const glm::vec3 az = modelPoint(pouch, at + glm::vec3{0.f, 0.f, 1.f}) - o;
        const float k = glm::length(az); // world units a pouch unit
        const glm::vec3 out = glm::normalize(ax), up = glm::normalize(az);
        const bool alongZ = modelmeta::is(model, modelmeta::Id::Mervup);
        // Nose up: the grenade's +x up (its +z out), the multi-grenade's (and the ball's) +z up (its +x out).
        const glm::vec3 angles = alongZ || modelmeta::is(model, modelmeta::Id::Proxbomb) ? aliasAngles(out, up) : aliasAngles(up, out);
        const float size = k * g->size * props::drawnSize(model); // world units a model unit
        const float nose = alongZ || modelmeta::is(model, modelmeta::Id::Proxbomb) ? model->maxs[2] : model->maxs[0];
        place(ve, model, o - up * (nose * size), angles, 0, false);
        ve.ent.skinnum = 1; // (muted: unarmed; make_grenade_skins.py)
        ve.scale = glm::vec3{k * g->size};
        ve.lightMultiply = pouch.lightMultiply;
        ve.lightMod = pouch.lightMod;
    }
}

void setupAmmoPouch(const hands::State& s)
{
    view::ViewEntity& ve = entities.ammoPouch;
    // (Placed even when not drawn, vr_ammo_pouch_show 0: it still works where it is, pouch reloading too.)
    qmodel_t* const model = body::ammoPouchEnabled() && !body::gearHiddenForDeath() ? viewModel("progs/vrpouch_ammo.mdl")
                                                                                   : nullptr;
    ammoPouchPlaced = false;
    if(!model)
    {
        ve.visible = false;
        setupAmmoPouchGrenades(ve);
        return;
    }
    body::HolsterPlate plate;
    const glm::vec3 pos = body::ammoPouchPosition(s, &plate);
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, s.bodyYaw, 0.f}, fwd, right, up);
    glm::vec3 out = plate.out != glm::vec3{0.f} ? plate.out : fwd;
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
        glm::vec3{vr_ammo_pouch_pitch.value, vr_ammo_pouch_yaw.value, vr_ammo_pouch_roll.value} +
            glm::vec3{vr_ammo_pouch_crouch_pitch.value, vr_ammo_pouch_crouch_yaw.value, vr_ammo_pouch_crouch_roll.value} *
                avatar::crouchPoseShare(s)); // (crouched: its crouched turn on top)
    if(vr_body_debug.value < 2.f)
    {
        zones::publish(zones::ZoneAmmoPouch, pos, frame.out, frame.side, frame.up, pose.slotPos, false);
    }
    place(ve, model, pose.slotPos, pose.slotAngles, ammoPouchFrame(), false);
    ve.ent.skinnum = (cl.stats[protocol::STAT_QVR_POUCHKIND] & 8) ? 1 : 0;
    ve.scale = glm::vec3{CLAMP(0.25f, vr_ammo_pouch_scale.value, 4.f)};
    ammoPouchPlaced = true;
    if(vr_ammo_pouch_show.value == 0.f)
    {
        ve.visible = false;
        setupAmmoPouchGrenades(ve);
        return;
    }

    // Its counter (vr_ammo_pouch_counter): how many of what it gives are left, on a small screen as the guns' ammo
    // counters, at vr_ammo_pouch_counter_x/y/z off the pouch (out of the body, to the right, up) facing the eyes as they
    // look down at it, turned by vr_ammo_pouch_counter_pitch/yaw/roll. Queued once a frame.
    if(vr_ammo_pouch_counter.value && cl.stats[protocol::STAT_QVR_POUCHKIND] > 0 && pouchCounterFrame != host_framecount)
    {
        pouchCounterFrame = host_framecount;
        const float k = CLAMP(0.25f, vr_ammo_pouch_scale.value, 4.f);
        const glm::vec3 side = glm::normalize(glm::cross(frame.up, frame.out)); // (the body's right, looking out)
        const glm::vec3 pos = at + (frame.out * vr_ammo_pouch_counter_x.value + side * vr_ammo_pouch_counter_y.value +
                                       frame.up * vr_ammo_pouch_counter_z.value) * k;
        // Turned with the pouch alone (the author: it turned with the view): the way the eyes look down at it and its
        // top towards the pouch's front, both in the pouch's own frame, then the offsets in that frame.
        const glm::vec3 look = glm::normalize(frame.out * 0.35f - frame.up);
        const glm::vec3 top = glm::normalize(frame.out - look * glm::dot(frame.out, look));
        const glm::vec3 angles = composeAngles(hands::anglesFromVectors(look, top),
            {vr_ammo_pouch_counter_pitch.value, vr_ammo_pouch_counter_yaw.value, vr_ammo_pouch_counter_roll.value});
        char buf[16];
        q_snprintf(buf, sizeof(buf), "%d", cl.stats[protocol::STAT_QVR_POUCHCOUNT]);
        text3d::queue(buf, pos, angles, text3d::Align::Centre, 0.1f * vr_ammo_pouch_counter_scale.value * k,
            vr_weapon_screen.value != 0.f);
    }
    highlight(ve, s.hotspot[HAND_OFF] == body::HS_AMMO_POUCH || s.hotspot[HAND_MAIN] == body::HS_AMMO_POUCH);
    setupAmmoPouchGrenades(ve);
}

// The cells standing in the ammo pouch (make_ammo_pouch.py MAGS[4]: the lightning gun's, feed end up along its width,
// their copper contacts on top at MAG_TOP; in its model units, +x out of the body, +y left, +z up): as many as its frame
// shows (ammoPouchFrame: one a 36 cells, up to 3), the first on its right.
constexpr float pouchCellX = 1.8f;                            // make_pouch.py BACK_X + full DEPTH / 2
constexpr float pouchCellTop = 2.9f;                          // MAG_TOP
constexpr float pouchCellY[3] = {-2.6f, 0.f, 2.6f};           // MAGS[4]'s

// The chainsaw's cord's handle in a fist (or flying back to its seat): the chainsaw's model, its frame of the handle
// alone, placed by vr_chainsaw.cpp, mirrored, scaled and lit as the chainsaw.
void setupSawHandle()
{
    view::ViewEntity& ve = entities.sawHandle;
    const view::ViewEntity* saw = nullptr;
    glm::vec3 origin, angles;
    int frame = 0;
    if(!chainsaw::handleEntity(saw, origin, angles, frame))
    {
        ve.visible = false;
        return;
    }
    place(ve, saw->ent.model, origin, angles, frame, saw->mirrored);
    ve.ent.lerpflags |= LERP_RESETANIM; // (never blended with the whole saw's frames)
    ve.scale = saw->scale;
    ve.lightMultiply = saw->lightMultiply;
    ve.lightMod = saw->lightMod;
    // Lit as the chainsaw (sampled where its light is): no change of shade as it leaves its seat.
    ve.lightShift = glm::vec3{saw->ent.origin[0], saw->ent.origin[1], saw->ent.origin[2]} + saw->lightShift - origin;
}

// The weapons lying in the world (map pickups are the guns themselves in Quake VR: thrown weapons,
// func_weapon_grabbable) near the player carry their ammo screen and button too (vr_weapon_screen_idle): the nearest
// vr_weapon_world_attach_max (up to maxWorldWeapons) within vr_weapon_world_attach_range (the author's note
// vrfiringrange_2026-10-09_12-35-45: the magazines and screens popped in and out a few metres off, at 320 units and 6 guns);
// a lava gun among them glows (dimmer, vr_lavagun_light_idle).
void setupWorldWeapons(const hands::State& s, bool queueTexts)
{
    const float reach = za::max(vr_weapon_world_attach_range.value, 0.f);
    const int most = CLAMP(0, static_cast<int>(vr_weapon_world_attach_max.value), maxWorldWeapons);
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
            !modelmeta::has(e->model, modelmeta::Trait::ViewWeapon) || weapons::slotForModel(e->model) < 0)
        {
            continue;
        }
        const float d = glm::distance(glm::vec3{e->origin[0], e->origin[1], e->origin[2]}, s.head);
        if(most == 0 || d > reach || (count == most && d >= nearest[most - 1].dist))
        {
            continue;
        }
        // Kept sorted, nearest first.
        int at = count < most ? count++ : most - 1;
        while(at > 0 && nearest[at - 1].dist > d)
        {
            nearest[at] = nearest[at - 1];
            at--;
        }
        nearest[at] = {e, d};
    }

    int lights = 0;
    const bool printScreens = queueTexts && vr_reload_debug.value >= 1 && developer.value && cl.time >= worldScreenPrinted + 1.0;
    if(printScreens)
    {
        worldScreenPrinted = cl.time;
    }
    for(int i = 0; i < maxWorldWeapons; i++)
    {
        view::ViewEntity& button = entities.worldButton[i];
        worldWeaponsNear[i] = nullptr;
        if(i >= count)
        {
            button.visible = false;
            entities.worldFrontButton[i].visible = false;
            continue;
        }
        const entity_t& e = *nearest[i].e;
        worldWeaponsNear[i] = &e;
        const int slot = weapons::slotForModel(e.model);
        // Its clip (a weapon prop's, networked: U_QVR_WEAPONUID), as a held gun shows it; its reserve only once in a hand.
        const bool numbered = &e >= cl_entities && &e < cl_entities + cl_max_edicts;
        const client::EntityVr* net = numbered ? client::entityVr(static_cast<int>(&e - cl_entities)) : nullptr;
        const int clip = net && net->weaponUid != 0 ? net->clip : -1;
        idleAttachments(e, false, slot, button, entities.worldFrontButton[i], clip, queueTexts, clip < 0);
        if(printScreens)
        {
            za::String text = idleWeaponText(e.model, clip, clip < 0);
            for(char& c : text)
            {
                c = c == '\n' ? '|' : c;
            }
            Con_Printf("world gun %d (%s): its screen \"%s\", %.0f units off\n", static_cast<int>(&e - cl_entities),
                e.model->name, text.data(), static_cast<double>(nearest[i].dist));
        }
        if(emissive::isLavaGun(e.model) && 2 + HolsterCount + lights < emissive::lavaGunLights)
        {
            emissive::lavaGunLight(2 + HolsterCount + lights++, lavaGlowPosition(e, false, 0.f, slot), vr_lavagun_light_idle.value);
        }
    }
}
} // namespace

// The drawn hands' wrists and orientations, for the body's arms and the wrist gadget.
[[nodiscard]] avatar::HandPose drawnHand(const hands::State& s, int hand)
{
    // The centre of the wrist in hand_base.mdl (frame 0), or the jointed hand's (its palm is shorter: make_hand_rig.py).
    const glm::vec3 handWrist = rigHands[hand].drawn ? vec3Of(handrig::data::wrist) : glm::vec3{-6.86f, -1.08f, 1.42f};

    avatar::HandPose hp;
    const view::ViewEntity& base = entities.hand[hand][FingerBase];
    if(entities.weapon[hand].ent.model && base.ent.model)
    {
        const view::ModelSpace space{base}; // (its matrix made once for the points)
        hp.wrist = space.point(handWrist);
        // hand_base.mdl: +x towards the fingers, +z the index finger's side, +y the palm's.
        const glm::vec3 alongX = space.point(handWrist + glm::vec3{1.f, 0.f, 0.f});
        const glm::vec3 alongZ = space.point(handWrist + glm::vec3{0.f, 0.f, 1.f});
        hp.up = glm::normalize(alongZ - hp.wrist);
        hp.back = glm::normalize(space.point(handWrist + glm::vec3{0.f, -1.f, 0.f}) - hp.wrist);
        hp.forward = glm::normalize(alongX - hp.wrist);
        if(rigHands[hand].drawn) // (its size: the bare forearms' ends fit round it, avatar.cpp)
        {
            hp.unit = {glm::distance(alongX, hp.wrist), glm::distance(space.point(handWrist + glm::vec3{0.f, 1.f, 0.f}), hp.wrist),
                glm::distance(alongZ, hp.wrist)};
        }
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

namespace
{
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

// The build of the body drawn this frame (0 lean, 1 athletic, 2 brawny; the wrist gadget's straps fit its bracer), and
// whether its forearms are bare (vr_body_bracers 0: straps and gadget fitted to the skin).
int drawnBuild = 1;
bool drawnBare = false;

// The light your body was last drawn in (view::bareLight, view::bareShade): with bare forearms the hands are drawn in
// it too (with bracers each hand samples its own light: the bracer's lip covers the wrist), so that the skin is lit alike on both sides of the wrist (each model's light, its direction and the light
// round it are otherwise sampled at its own place: the body's round the torso, a hand's round the hand).
struct BareLight
{
    int frame{-1}; // host_framecount when the body's was kept (-1: never)
    glm::vec3 color{0.f};
    float dir[4]{0.f, 0.f, 0.f, 0.f};
    float ambient[6][4]{};
    glm::vec3 hand[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // each hand's own (vr_body_wrist_info)
};
BareLight bareLit;

// A hand drawn in the body's light (bare forearms, the body's light kept this frame or the last): its hand, else -1.
[[nodiscard]] int bareLitHand(const entity_t* e)
{
    if(!drawnBare || bareLit.frame < host_framecount - 1)
    {
        return -1;
    }
    for(int hand = 0; hand < 2; hand++)
    {
        if(e == &entities.hand[hand][FingerBase].ent && rigHands[hand].drawn)
        {
            return hand;
        }
    }
    return -1;
}

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
    if(!gadget::active() || (static_cast<int>(vr_shot_hide.value) & 1)) // (hidden for a clean shot: hiddenForShot)
    {
        ve.visible = false;
        gadget::setPose({});
        return;
    }

    const int hand = hands::gadgetHand();
    const bool leftArm = hand == HAND_OFF; // (the left controller)
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

    // The casing stays at the same mount when bracers are toggled. The bands below it fit the forearm separately.
    constexpr float rest = 0.045f;

    gadget::Pose pose;
    pose.valid = true;
    pose.origin = wrist - dir * (0.085f * m2w) + out * (rest * m2w + 0.35f * scale) + axes * offset * (0.01f * m2w);
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

    // The straps, under the lugs (make_gadget.py: x -+1.25, the band 0.3 either side of it), round the skin or bracer.
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
        const float along = CLAMP(0.f, glm::dot(lug - elbow, dir) / fa.length, 1.f);
        avatar::ForearmFrame sf;
        if(!avatar::forearmFrame(hand, along, sf))
        {
            continue;
        }
        const float edge = halfWidth / fa.length;
        float wu, wv, eu, ev; // the bracer's semi-axes at the band's wrist edge and elbow edge
        avatar::forearmGirth(build, CLAMP(0.f, along + edge, 1.f), wu, wv, drawnBare);
        avatar::forearmGirth(build, CLAMP(0.f, along - edge, 1.f), eu, ev, drawnBare);
        wu += margin, wv += margin, eu += margin, ev += margin;
        const float taper = CLAMP(0.f, za::max(eu / wu, ev / wv) - 1.f, strapTaper);
        view::ViewEntity& strap = entities.gadgetStrap[i];
        const glm::vec3 sa = hands::anglesFromVectors(sf.axes[0], sf.axes[2]);
        place(strap, strapModel, sf.point, {-sa.x, sa.y, sa.z}, 1, false);
        strap.scale = {halfWidth, wv * m2w, wu * m2w}; // model y: the frame's y (the ring's other axis), z: its hint
        strap.zeroBlend = 1.f - taper / strapTaper;    // frame 1 (the cone) towards frame 0 (the cylinder)
    }
}
} // namespace

// The wrist gadget's screen tap (bullet time) and side button (the gear lights), once a frame, tested right after the
// gadget is placed: against the gadget and the hands as drawn this frame (the player's move and turn applied, on lifts
// alike), never a frame behind or ahead of what is seen (NOTES.md vrfiringrange_2026-10-09_11-01-21). Not while posing
// (the hands are moved for the view only). Then the debug drawing (vr_debug_gadget_button), as tested; with 3, each
// frame, how far the zone is from the drawn gadget's screen and how far off the tap's old test at the frame's start was.
void gadgetTouches(const hands::State& s, bool posingNow)
{
    if(oncePerFrame.gadgetTouches != host_framecount && !posingNow)
    {
        oncePerFrame.gadgetTouches = host_framecount;
        touch::viewFrame(s); // the touchscreen (an app on the gadget: Doom), before the screen tap it overrides
        bullettime::viewFrame(s);
        gearlights::viewFrame(s);
        bullettime::Screen sc;
        const view::ViewEntity& ve = entities.gadget;
        if(vr_debug_gadget_button.value >= 3.f && s.valid && ve.visible && bullettime::screen(sc))
        {
            // The screen's middle on the drawn model (its entity: origin, angles, scale), against the zone's.
            glm::vec3 fwd, right, up, corner;
            glm::vec2 size;
            hands::angleVectors(glm::vec3{-ve.ent.angles[0], ve.ent.angles[1], ve.ent.angles[2]}, fwd, right, up);
            gadget::screenRect(corner, size);
            const glm::vec3 mid = glm::vec3{corner.x + size.x * 0.5f, corner.y + size.y * 0.5f, corner.z} * ve.scale;
            const glm::vec3 drawn = glm::vec3{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]} + fwd * mid.x -
                                    right * mid.y + up * mid.z;
            const int tapper = 1 - hands::gadgetHand();
            const float cm = 0.01f * units::metresToUnits();
            glm::vec3 start;
            const bool hadStart = bullettime::frameStartOffset(start);
            Con_Printf("gadget sync: %d yaw %.1f zone %.4f cm, start %.2f cm; at "
                       "%.0f %.0f %.0f\n",
                host_framecount, cl.viewangles[YAW], glm::distance(drawn, sc.centre) / cm,
                hadStart ? glm::distance(start, sc.centre - s.pos[tapper]) / cm : -1.f, s.playerOrigin.x,
                s.playerOrigin.y, s.playerOrigin.z);
        }
    }
    gearlights::debugDraw(s);
}

// Quad damage: electric arcs crawling over the hands and forearms, reshaped every frame (the same
// in both eyes); now and then a longer one jumps between the fingers and the elbow (vr_shock.cpp).
void quadArcs(const hands::State& s)
{
    int& lastFrame = oncePerFrame.quadArcs;
    if(host_framecount == lastFrame)
    {
        return; // once per frame, however often the view is set up
    }
    lastFrame = host_framecount;
    shock::armArcs(s, static_cast<unsigned>(host_framecount) * 2654435761u, 5, 0.2f);
}

namespace
{
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
        // wounds painted instead (vr_wounds.cpp); the wound skins washed (wounds::skinLevel)
        ve.ent.skinnum = armorWorn() * 4 + (wounds::replacesSkins() ? 0 : wounds::skinLevel(2, damageLevel(), ve.ent.origin));
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

    // The pentagram's and the biosuit's old tints, only where their glows are off (powerupGlow)
    const float pulse = 0.5f + 0.5f * static_cast<float>(za::sin(vr_gametime * 6.0));
    if((cl.items & IT_INVULNERABILITY) && vr_pent_glow.value <= 0.f)
    {
        ve.lightMultiply = true;
        ve.lightMod = glm::vec3{1.6f + 0.6f * pulse, 0.7f, 0.7f};
    }
    else if((cl.items & IT_SUIT) && vr_suit_glow.value <= 0.f)
    {
        ve.lightMultiply = true;
        ve.lightMod = glm::vec3{0.8f, 1.25f, 0.8f};
    }
    if(cl.items & IT_INVISIBILITY)
    {
        ve.ent.alpha = ENTALPHA_ENCODE(0.3f);
    }
    if((cl.items & IT_QUAD) && static_cast<int>(vr_quad_look.value) != 1) // 1: the blue glow only (powerupGlow)
    {
        quadArcs(s);
    }
}

// A power-up's glow on you, so that each is known at a glance: Quad Damage's blue (vr_quad_look 1, 2; vr_quad_glow,
// vr_quad_glow_light), the pentagram's red (vr_pent_glow, vr_pent_glow_light), the biosuit's green (vr_suit_glow,
// vr_suit_glow_light). The alias shader's rim glow on the hands and less on the body (view::powerupGlow, VR_EntityGlow)
// and light from the hands on the world round you, gently pulsing. With more than one, they take turns, crossfading.
struct PowerupGlowLook
{
    int item;
    float shade[3]; // on the hands and body
    float light[3]; // the light they cast
    float gain;     // the glow's strength at 1x
    const char* name;
};
const PowerupGlowLook powerupGlowLooks[] = {
    {IT_QUAD, {0.12f, 0.35f, 1.f}, {0.3f, 0.5f, 1.f}, 1.f, "quad"}, // Quake's quad blue, a little towards cyan
    // The pentagram's red, deep and stronger: Quake's yellow pentagram flash over the view would wash a lighter one out
    {IT_INVULNERABILITY, {1.f, 0.05f, 0.08f}, {1.f, 0.25f, 0.2f}, 1.5f, "pent"},
    {IT_SUIT, {0.25f, 1.f, 0.18f}, {0.4f, 1.f, 0.3f}, 1.f, "suit"}, // the biosuit's green (its view blend)
};
constexpr int powerupGlowKinds = 3;
constexpr double powerupGlowTurn = 1.6; // seconds each glow shows when more than one take turns

struct PowerupGlowState
{
    float level = 0.f; // the glow's strength this frame (0 none), pulsing
    float code = 0.f;  // its colour coded for the shader (r + 16g + 256b, 16 levels a channel)
    int shown = 0;     // how many power-ups glow (taking turns)
    int kind = -1;     // the one glowing now (or fading out), powerupGlowLooks
};
PowerupGlowState powerupGlowState;

float powerupGlowStrength(int kind)
{
    switch(kind)
    {
        case 0: return static_cast<int>(vr_quad_look.value) >= 1 ? vr_quad_glow.value : 0.f;
        case 1: return vr_pent_glow.value;
        default: return vr_suit_glow.value;
    }
}

float powerupGlowLight(int kind)
{
    switch(kind)
    {
        case 0: return vr_quad_glow_light.value;
        case 1: return vr_pent_glow_light.value;
        default: return vr_suit_glow_light.value;
    }
}

void powerupGlow(const hands::State& s)
{
    PowerupGlowState& g = powerupGlowState;
    g = PowerupGlowState{};
    const bool can = vr_body_powerups.value != 0.f && cl.stats[STAT_HEALTH] > 0 && !cl.intermission && s.valid && !deathview::immersive();
    int shown[powerupGlowKinds];
    for(int k = 0; can && k < powerupGlowKinds; k++)
    {
        if((cl.items & powerupGlowLooks[k].item) && (powerupGlowStrength(k) > 0.f || powerupGlowLight(k) > 0.f))
        {
            shown[g.shown++] = k;
        }
    }
    if(g.shown == 0)
    {
        for(int hand = 0; hand < 2; hand++)
        {
            emissive::powerupLight(hand, s.pos[hand], 0.f, 0.f, glm::vec3{0.f});
        }
        return;
    }
    // Taking turns: each shows for powerupGlowTurn seconds, crossfading into the next over its last third.
    const double t = cl.time;
    const double phase = za::fmod(t / powerupGlowTurn, static_cast<double>(g.shown));
    const int turn = za::min(static_cast<int>(phase), g.shown - 1);
    const int a = shown[turn];
    const int b = shown[(turn + 1) % g.shown];
    const float f = static_cast<float>(phase - turn);
    const float w = g.shown > 1 ? glm::smoothstep(0.67f, 1.f, f) : 0.f;
    g.kind = w < 0.5f ? a : b;
    // A slow breath (about 0.8 Hz) and a slower swell under it: 0.64 .. 1.
    const float pulse = 0.82f + 0.12f * static_cast<float>(za::sin(t * 5.0)) + 0.06f * static_cast<float>(za::sin(t * 1.9 + 1.3));
    const auto mixed = [&](const float(&ca)[3], const float(&cb)[3]) {
        return glm::mix(glm::vec3{ca[0], ca[1], ca[2]}, glm::vec3{cb[0], cb[1], cb[2]}, w);
    };
    const glm::vec3 shade = mixed(powerupGlowLooks[a].shade, powerupGlowLooks[b].shade);
    const glm::vec3 lightColor = mixed(powerupGlowLooks[a].light, powerupGlowLooks[b].light);
    const float strengthA = za::clamp(powerupGlowStrength(a), 0.f, 2.f) * powerupGlowLooks[a].gain;
    const float strengthB = za::clamp(powerupGlowStrength(b), 0.f, 2.f) * powerupGlowLooks[b].gain;
    g.level = glm::mix(strengthA, strengthB, w) * pulse;
    const auto level16 = [](float c) { return static_cast<float>(za::clamp(static_cast<int>(c * 15.f + 0.5f), 0, 15)); };
    g.code = level16(shade.r) + 16.f * level16(shade.g) + 256.f * level16(shade.b);
    const float light = za::clamp(glm::mix(powerupGlowLight(a), powerupGlowLight(b), w), 0.f, 2.f) * pulse;
    for(int hand = 0; hand < 2; hand++) // (the body's own: Quake's blue quad and red pentagram lights round you, cl_main.c)
    {
        emissive::powerupLight(hand, s.pos[hand], light * 0.8f, 110.f, lightColor); // (inside the hand: lights the world, not it)
    }
}
} // namespace

// vr_powerup_info: the power-ups' look on you as drawn last frame: the quad's look and arcs, the glow (which, how many
// take turns), its strength on the hands and the body, and its lights.
void powerupInfo_f()
{
    const PowerupGlowState& g = powerupGlowState;
    Con_Printf("powerups: quad %d pent %d suit %d look %d arcsframe %d\n", (cl.items & IT_QUAD) ? 1 : 0,
        (cl.items & IT_INVULNERABILITY) ? 1 : 0, (cl.items & IT_SUIT) ? 1 : 0, static_cast<int>(vr_quad_look.value), oncePerFrame.quadArcs);
    const float hand = view::powerupGlow(&entities.hand[0][FingerBase].ent);
    const float body = view::powerupGlow(&entities.body.ent);
    Con_Printf("glow: %s shown %d level %.2f code %.0f hand %.3f body %.3f\n", g.kind >= 0 ? powerupGlowLooks[g.kind].name : "none",
        g.shown, g.level, g.code, hand, body);
    for(int i = 0; i < emissive::powerupLights; i++)
    {
        for(const dlight_t& dl : cl_dlights)
        {
            if(dl.key == emissive::powerupLightKey - i && dl.die > cl.time)
            {
                Con_Printf("glow light %d: radius %.0f colour %.2f %.2f %.2f\n", i, dl.radius, dl.color[0], dl.color[1], dl.color[2]);
            }
        }
    }
}

namespace
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
        // The build (vr_body_build): progs/vrbody_lean, vrbody (athletic) or vrbody_brawny; their bare forearms'
        // (vr_body_bracers 0) if both hands are the jointed hand they are fitted round.
        const int build = static_cast<int>(vr_body_build.value);
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
            drawnBuild = model == viewModel("progs/vrbody.mdl") ? 1 : CLAMP(0, build, 2);
            drawnBare = drawnBare && model == viewModel(bodyModelName(build, true));
            const avatar::HandPose handPoses[2] = {drawnHand(s, 0), drawnHand(s, 1)};

            view::ViewEntity& ve = entities.body;
            const glm::vec3 origin = avatar::pose(s, model, &ve.ent, handPoses, mode >= 3);
            place(ve, model, origin, glm::vec3{0.f}, 0, false);
            showPlayerState(ve, s);
        }
    }
}
} // namespace

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

namespace
{
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
// engine's VR_DoWpnButton, which sent keys bound to these impulses): impulse 42 (the off hand's weapon), 43 (the main
// hand's); the grappling gun's is the detach. Its front button (near the muzzle): 44, 45, the reel-in.
// Checked every frame (it was every 0.2 s: a fingertip coming at the button was first seen well inside its reach, often
// past its face, and refused as from behind: the author pressed it about once in a hundred).
struct ButtonState
{
    bool hover{false};     // the other fingertip within reach of it (until it is buttonLeave units further out)
    double lastPress{-1.0}; // cl.time
};
constexpr float buttonReach = 2.7f;   // units from the button's middle a fingertip presses it at
constexpr float buttonLeave = 0.5f;   // and further out than that again it leaves it (no flicker at the edge)
constexpr double buttonRepeat = 0.2;  // seconds between two presses of a button at least
ButtonState buttonStates[2];
ButtonState frontButtonStates[2];

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
    place(ve, viewModel("progs/wpnbutton.mdl"), pos, glm::vec3{back.ent.angles[0], back.ent.angles[1], back.ent.angles[2]}, 0,
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

} // namespace

} // namespace view_detail

namespace qvr::view
{
bool gunToWorld(int hand, const glm::vec3& p, glm::vec3& out)
{
    if(hand < 0 || hand > 1 || !entities.weapon[hand].visible || !entities.weapon[hand].ent.model)
    {
        return false;
    }
    out = modelPoint(entities.weapon[hand], p);
    return true;
}

bool gunFromWorld(int hand, const glm::vec3& w, glm::vec3& out)
{
    glm::vec3 o, x, y, z;
    if(!gunToWorld(hand, glm::vec3{0.f}, o) || !gunToWorld(hand, {1.f, 0.f, 0.f}, x) || !gunToWorld(hand, {0.f, 1.f, 0.f}, y) ||
        !gunToWorld(hand, {0.f, 0.f, 1.f}, z))
    {
        return false;
    }
    const glm::mat3 m{x - o, y - o, z - o};
    if(std::abs(glm::determinant(m)) < 1e-9f)
    {
        return false;
    }
    out = glm::inverse(m) * (w - o);
    return true;
}

bool gunAxes(int hand, glm::mat3& out)
{
    if(hand < 0 || hand > 1 || !entities.weapon[hand].visible || !entities.weapon[hand].ent.model)
    {
        return false;
    }
    out = held::axesFromAngles(entities.weapon[hand].ent.angles, false);
    return true;
}

bool loadPath(int hand, glm::vec3& port, glm::vec3& deep, glm::vec3& end)
{
    if(hand < 0 || hand > 1 || !entities.weapon[hand].visible || !entities.weapon[hand].ent.model)
    {
        return false;
    }
    return modelLoadPath(entities.weapon[hand].ent.model, ssgOpenAngle(hand), port, deep, end);
}

bool propGun(int num, ViewEntity& out, float& ssgOpen)
{
    if(num <= 0 || num >= cl.num_entities || num == cl.viewentity)
    {
        return false;
    }
    const entity_t& e = cl_entities[num];
    glm::vec3 port, deep, end;
    if(!e.model || e.model->type != mod_alias || !modelLoadPath(e.model, 0.f, port, deep, end))
    {
        return false;
    }
    const client::EntityVr* net = client::entityVr(num);
    out = ViewEntity{};
    out.ent = e;
    out.visible = true;
    out.netEntity = num; // (drawn with its networked scale and offset: setupMagazines' guns lying about)
    ssgOpen = net && net->ssgOpen && ssgBreaks() ? CLAMP(0.f, vr_reload_ssg_open_angle.value, 80.f) : 0.f;
    return true;
}

bool modelLoadPath(const qmodel_t* model, float ssgOpen, glm::vec3& port, glm::vec3& deep, glm::vec3& end)
{
    if(!model)
    {
        return false;
    }
    const auto& info = modelmeta::get(model);
    for(const LoadPort& lp : loadPorts)
    {
        if(!info.is(lp.model) || lp.magazine)
        {
            continue;
        }
        const glm::vec3 at = lp.point + glm::vec3{lp.offset[0]->value, lp.offset[1]->value, lp.offset[2]->value};
        if(lp.model == modelmeta::Id::VShot2)
        {
            // Into the chambers, along the barrels as they are drawn open.
            const float open = ssgOpen;
            const glm::vec3 along = ssgTurn({1.f, 0.f, 0.f}, open, false);
            port = ssgTurn(at, open);
            deep = port + along * 1.f;
            end = port + along * 2.2f;
            return true;
        }
        if(lp.side)
        {
            // A side port (AD's Widowmaker): in through its gate, then forward into the tube.
            port = at;
            deep = at - glm::vec3{0.f, 1.4f, 0.f};
            end = deep + glm::vec3{2.5f, 0.f, 0.f};
            return true;
        }
        if(lp.front)
        {
            // A launcher: back into the barrel from its muzzle, the round's middle the round's half length and more in.
            port = at;
            deep = at - glm::vec3{lp.depth * 0.5f, 0.f, 0.f};
            end = at - glm::vec3{lp.depth, 0.f, 0.f};
            return true;
        }
        // The shotgun: up through the port's well (polish_weapons.py loading_port: its ceiling 1 unit up), then forward
        // into the magazine tube.
        port = at;
        deep = at + glm::vec3{0.f, 0.f, 1.6f};
        end = deep + glm::vec3{2.5f, 0.f, 0.4f};
        return true;
    }
    return false;
}

float ssgOpenAngle(int hand)
{
    return hand >= 0 && hand < 2 && entities.weapon[hand].visible && ssgHands[hand].gun == entities.weapon[hand].ent.model
               ? ssgHands[hand].angle
               : 0.f;
}

glm::vec3 ssgTurned(const glm::vec3& p, float deg, bool point)
{
    return ssgTurn(p, deg, point);
}

bool modelLoadPort(const qmodel_t* model, float ssgOpen, glm::vec3& at, glm::vec3& axis, glm::vec3& face)
{
    if(!model || model->type != mod_alias)
    {
        return false;
    }
    const auto& info = modelmeta::get(model);
    for(const LoadPort& port : loadPorts)
    {
        if(!info.is(port.model))
        {
            continue;
        }
        const bool ssg = port.model == modelmeta::Id::VShot2;
        const glm::vec3 point = port.point + glm::vec3{port.offset[0]->value, port.offset[1]->value, port.offset[2]->value};
        // The way a round lies going in: the tube's or the barrels' forward (+x), a well's seated magazine's feed end
        // (its middle to its seat: magazineShape's first axis, whether one is in or not). Its opening's outward way: a
        // well's and the breech's back along the axis, the shotgun's port down, a launcher's muzzle forward (a round
        // goes in butt first: its nose along the axis).
        glm::vec3 way{1.f, 0.f, 0.f};
        if(const MagMount* mount = port.magazine ? magMountFor(model) : nullptr)
        {
            way = glm::normalize(mount->seat - mount->centre);
        }
        const glm::vec3 out = port.side ? glm::vec3{0.f, 1.f, 0.f} : port.front ? way : port.magazine || ssg ? -way :
            glm::vec3{0.f, 0.f, -1.f};
        at = ssg ? ssgTurn(point, ssgOpen) : point;
        axis = ssg ? ssgTurn(way, ssgOpen, false) : way;
        face = ssg ? ssgTurn(out, ssgOpen, false) : out;
        return true;
    }
    return false;
}

bool weaponButtonHandTarget(int hand, float angle, float azimuth, float units, glm::vec3& out)
{
    const hands::State& s = hands::current();
    const view::ViewEntity& button = entities.button[1 - hand];
    if(!s.valid || !button.visible)
    {
        return false;
    }
    const glm::vec3 at{button.ent.origin[0], button.ent.origin[1], button.ent.origin[2]};
    const glm::vec3 face = buttonFace(button);
    glm::vec3 across = glm::cross(face, glm::vec3{0.f, 0.f, 1.f});
    across = glm::length(across) > 1e-3f ? glm::normalize(across) : glm::vec3{1.f, 0.f, 0.f};
    const float a = glm::radians(angle), z = glm::radians(azimuth);
    const glm::vec3 dir = face * za::cos(a) + (across * za::cos(z) + glm::cross(face, across) * za::sin(z)) * za::sin(a);
    out = at + dir * units - (buttonFingertip(s, hand) - s.pos[hand]);
    return true;
}

bool emptyHandPose(const hands::State& s, int hand, EmptyHand& out)
{
    return emptyHandImpl(s, hand, out);
}

const ViewEntity* find(const entity_t* e)
{
    const auto* first = reinterpret_cast<const unsigned char*>(&entities);
    const auto* p = reinterpret_cast<const unsigned char*>(e);
    if(p < first || p >= first + sizeof(entities))
    {
        // The body's mirror image (addClones): its own copy, mirrored the other way.
        const auto* clone = reinterpret_cast<const unsigned char*>(clones);
        if(p >= clone && p < clone + sizeof(clones))
        {
            const ViewEntity* c = clones + (p - clone) / sizeof(ViewEntity);
            return &c->ent == e ? c : nullptr;
        }
        return nullptr;
    }

    // The view entities are one run of ViewEntity (Entities holds nothing else): the one it falls in.
    static_assert(sizeof(Entities) % sizeof(ViewEntity) == 0 && alignof(Entities) == alignof(ViewEntity));
    const auto* ve = reinterpret_cast<const ViewEntity*>(&entities) + (p - first) / sizeof(ViewEntity);
    return &ve->ent == e ? ve : nullptr;
}

int drawGroup(const entity_t* e)
{
    const auto* first = reinterpret_cast<const unsigned char*>(&entities);
    const auto* p = reinterpret_cast<const unsigned char*>(e);
    if(p < first || p >= first + sizeof(entities))
    {
        return find(e) ? VR_DRAW_MIRROR : -1; // the body's mirror image, or not a view entity
    }
    struct Range
    {
        size_t first, count;
        int group;
    };
#define QVR_DRAW_RANGE(member, group) \
    Range{offsetof(Entities, member) / sizeof(ViewEntity), sizeof(Entities::member) / sizeof(ViewEntity), group}
    static constexpr Range ranges[] = {QVR_DRAW_RANGE(weapon, VR_DRAW_HELD), QVR_DRAW_RANGE(weaponMorph, VR_DRAW_HELD),
        QVR_DRAW_RANGE(hand, VR_DRAW_HANDS), QVR_DRAW_RANGE(holster, VR_DRAW_HOLSTERED), QVR_DRAW_RANGE(holsterSlot, VR_DRAW_HOLSTERED),
        QVR_DRAW_RANGE(holsterButton, VR_DRAW_HOLSTERED), QVR_DRAW_RANGE(worldButton, VR_DRAW_LYING),
        QVR_DRAW_RANGE(holsterFrontButton, VR_DRAW_HOLSTERED), QVR_DRAW_RANGE(worldFrontButton, VR_DRAW_LYING),
        QVR_DRAW_RANGE(body, VR_DRAW_BODY), QVR_DRAW_RANGE(pauldron, VR_DRAW_BODY), QVR_DRAW_RANGE(pauldronArm, VR_DRAW_BODY),
        QVR_DRAW_RANGE(gadget, VR_DRAW_WORN), QVR_DRAW_RANGE(gadgetStrap, VR_DRAW_WORN), QVR_DRAW_RANGE(flashlight, VR_DRAW_WORN),
        QVR_DRAW_RANGE(pouch, VR_DRAW_WORN), QVR_DRAW_RANGE(ammoPouch, VR_DRAW_WORN), QVR_DRAW_RANGE(ammoPouchGrenade, VR_DRAW_WORN),
        QVR_DRAW_RANGE(mag, VR_DRAW_HELD), QVR_DRAW_RANGE(holsterMag, VR_DRAW_HOLSTERED), QVR_DRAW_RANGE(worldMag, VR_DRAW_LYING),
        QVR_DRAW_RANGE(well, VR_DRAW_HELD), QVR_DRAW_RANGE(holsterWell, VR_DRAW_HOLSTERED), QVR_DRAW_RANGE(worldWell, VR_DRAW_LYING),
        QVR_DRAW_RANGE(pumpBody, VR_DRAW_HELD), QVR_DRAW_RANGE(pump, VR_DRAW_HELD), QVR_DRAW_RANGE(ssgFrame, VR_DRAW_HELD),
        QVR_DRAW_RANGE(ssgBarrels, VR_DRAW_HELD), QVR_DRAW_RANGE(holsterSsgFrame, VR_DRAW_HOLSTERED),
        QVR_DRAW_RANGE(holsterSsgBarrels, VR_DRAW_HOLSTERED), QVR_DRAW_RANGE(worldSsgFrame, VR_DRAW_LYING),
        QVR_DRAW_RANGE(worldSsgBarrels, VR_DRAW_LYING), QVR_DRAW_RANGE(sawHandle, VR_DRAW_HELD), QVR_DRAW_RANGE(button, VR_DRAW_HELD),
        QVR_DRAW_RANGE(frontButton, VR_DRAW_HELD), QVR_DRAW_RANGE(ghost, VR_DRAW_OTHER), QVR_DRAW_RANGE(muzzleFlash, VR_DRAW_HELD),
        QVR_DRAW_RANGE(enemyFlash, VR_DRAW_OTHER), QVR_DRAW_RANGE(adBolt, VR_DRAW_HELD)};
#undef QVR_DRAW_RANGE
    const size_t index = static_cast<size_t>(p - first) / sizeof(ViewEntity);
    for(const Range& r : ranges)
    {
        if(index >= r.first && index < r.first + r.count)
        {
            return r.group;
        }
    }
    return VR_DRAW_OTHER;
}

const entity_t* cloneSource(const entity_t* e)
{
    const auto* clone = reinterpret_cast<const unsigned char*>(clones);
    const auto* p = reinterpret_cast<const unsigned char*>(e);
    if(p < clone || p >= clone + sizeof(clones))
    {
        return nullptr;
    }
    const auto i = static_cast<int>((p - clone) / sizeof(ViewEntity));
    return &clones[i].ent == e ? &reinterpret_cast<const ViewEntity*>(&entities)[i].ent : nullptr;
}

float powerupGlow(const entity_t* e)
{
    const PowerupGlowState& g = powerupGlowState;
    if(g.level <= 0.f || !e)
    {
        return 0.f;
    }
    if(const entity_t* source = cloneSource(e))
    {
        e = source;
    }
    float strength = 0.f;
    bool weapon = false;
    if(e == &entities.body.ent)
    {
        strength = za::min(g.level * 0.3f, 1.f); // less: seen from inside, its edges are all round the view
    }
    else if(handOf(e, weapon) >= 0 && (!weapon || isHandModel(e->model)))
    {
        strength = za::min(g.level * 0.6f, 1.f);
    }
    return strength > 0.f ? -2.f - 2.f * g.code - strength : 0.f; // (vr_glsl.h QVR_ALIAS_FS_GLOW decodes it)
}

void bareLight(const entity_t* e, float lightcolor[3])
{
    if(e == &entities.body.ent)
    {
        bareLit.color = {lightcolor[0], lightcolor[1], lightcolor[2]};
        return;
    }
    for(int hand = 0; hand < 2; hand++)
    {
        if(e == &entities.hand[hand][FingerBase].ent)
        {
            bareLit.hand[hand] = {lightcolor[0], lightcolor[1], lightcolor[2]};
        }
    }
    if(bareLitHand(e) >= 0)
    {
        lightcolor[0] = bareLit.color.x;
        lightcolor[1] = bareLit.color.y;
        lightcolor[2] = bareLit.color.z;
    }
}

bool keepsBareLight(const entity_t* e)
{
    return drawnBare && e == &entities.body.ent;
}

void bareShade(const entity_t* e, float lightdir[4], float ambient[6][4])
{
    if(e == &entities.body.ent)
    {
        bareLit.frame = host_framecount;
        for(int k = 0; k < 4; k++)
        {
            bareLit.dir[k] = lightdir[k];
        }
        for(int f = 0; f < 6; f++)
        {
            for(int k = 0; k < 4; k++)
            {
                bareLit.ambient[f][k] = ambient[f][k];
            }
        }
        return;
    }
    if(bareLitHand(e) < 0)
    {
        return;
    }
    for(int k = 0; k < 4; k++)
    {
        lightdir[k] = bareLit.dir[k];
    }
    for(int f = 0; f < 6; f++)
    {
        for(int k = 0; k < 3; k++)
        {
            ambient[f][k] = bareLit.ambient[f][k];
        }
    }
    ambient[0][3] = bareLit.ambient[0][3]; // how much it applies, how much of the directional shading stays (the rest
    ambient[1][3] = bareLit.ambient[1][3]; // of the .w are a morph's: the hand's own)
}

bool skinTint(const entity_t* e, float tint[4], float range[4])
{
    if(!drawnBare)
    {
        return false;
    }
    for(int hand = 0; hand < 2; hand++)
    {
        const entity_t* he = &entities.hand[hand][FingerBase].ent;
        if((e == he || cloneSource(e) == he) && rigHands[hand].drawn)
        {
            // The jointed hand's end inside the arm (behind its wrist ring: make_hand_rig.py's first palm ring and the
            // thumb's base) folded onto the wrist's middle line (the alias shader's QVR_ALIAS_VS_HANDFOLD): the bare
            // forearm fits the hand's wrist closely, and that end would come out through its skin as the wrist bends.
            constexpr float foldBehind = 0.2f; // hand units behind the wrist
            tint[0] = tint[1] = tint[2] = 0.f;
            tint[3] = -1.f;
            range[0] = handrig::data::wrist[0] - foldBehind;
            range[1] = handrig::data::wrist[1];
            range[2] = handrig::data::wrist[2];
            range[3] = 0.f;
            return true;
        }
    }
    if(e != &entities.body.ent && cloneSource(e) != &entities.body.ent)
    {
        return false;
    }
    // make_vrbody.py (bare): the arms' skin block (u 0 .. 0.5, v 0 .. 0.4 less its 4-texel pad), the forearm's rings 1/9
    // of its height apart; the tint grows from the ring at 0.19 m (8/9) to the wrist's ring and is whole over the base
    // of the hand. Its colour: the jointed hand's clean skin round its wrist over the arm's there (make_vrbody.py
    // prints it as "wrist tint"), so that the arm meets the hand in the hand's colour.
    constexpr float pad = bodybare::skinPad, blockV = bodybare::skinBlockV - 2.f * pad;
    constexpr float lipVpos = bodybare::lipVpos; // HAND_LIP * HAND_UNIT past the wrist
    tint[0] = bareTint[0];
    tint[1] = bareTint[1];
    tint[2] = bareTint[2];
    tint[3] = 1.f;
    range[0] = bodybare::skinBlockU;
    range[1] = pad + blockV * (8.f / 9.f);
    range[2] = pad + blockV * lipVpos;
    range[3] = bodybare::skinBlockV;
    return true;
}

int skinTone(const entity_t* e, float tone[4])
{
    int kind = 0;
    if(e == &entities.body.ent || cloneSource(e) == &entities.body.ent)
    {
        kind = 2;
    }
    for(int hand = 0; hand < 2 && !kind; hand++)
    {
        const entity_t* he = &entities.hand[hand][FingerBase].ent;
        if((e == he || cloneSource(e) == he) && rigHands[hand].drawn)
        {
            kind = 1;
        }
    }
    const float amount = CLAMP(-1.f, vr_skin_tone.value, 1.f);
    const glm::vec3 colour = amount < 0.f ? glm::mix(glm::vec3{1.f}, glm::vec3{0.22f, 0.15f, 0.12f}, -amount) :
        glm::mix(glm::vec3{1.f}, glm::vec3{1.4f, 1.35f, 1.3f}, amount);
    tone[0] = colour.x;
    tone[1] = colour.y;
    tone[2] = colour.z;
    tone[3] = kind && amount != 0.f ? 1.f : 0.f;
    return kind;
}

namespace
{
// HSL (hue in degrees, saturation and lightness 0..1) to RGB.
[[nodiscard]] glm::vec3 hslToRgb(float hue, float sat, float light)
{
    const float h = (hue - 360.f * std::floor(hue / 360.f)) / 60.f;
    const float c = (1.f - std::fabs(2.f * light - 1.f)) * sat;
    const float x = c * (1.f - std::fabs(std::fmod(h, 2.f) - 1.f));
    const float m = light - 0.5f * c;
    const glm::vec3 rgb = h < 1.f ? glm::vec3{c, x, 0.f} : h < 2.f ? glm::vec3{x, c, 0.f} : h < 3.f ? glm::vec3{0.f, c, x} :
        h < 4.f ? glm::vec3{0.f, x, c} : h < 5.f ? glm::vec3{x, 0.f, c} : glm::vec3{c, 0.f, x};
    return rgb + glm::vec3{m};
}

} // namespace

int gloveLook(const entity_t* e, float glove[4], float cuff[4])
{
    float tone[4];
    const int kind = skinTone(e, tone);
    const int mode = static_cast<int>(CLAMP(0.f, vr_gloves.value, 2.f));
    glove[0] = glove[1] = glove[2] = glove[3] = 0.f;
    cuff[0] = cuff[1] = cuff[2] = cuff[3] = 0.f;
    if(!kind || !mode)
    {
        return 0;
    }
    const glm::vec3 colour = hslToRgb(vr_glove_hue.value, CLAMP(0.f, vr_glove_saturation.value, 1.f),
        CLAMP(0.f, vr_glove_lightness.value, 1.f));
    glove[0] = colour.x;
    glove[1] = colour.y;
    glove[2] = colour.z;
    glove[3] = static_cast<float>(mode); // the skin mask's channel: G fingerless, B full (make_skin_masks.py)
    if(kind == 2)
    {
        if(!drawnBare)
        {
            glove[3] = 0.f; // the bracers cover the wrist: nothing of the glove on the body
            return 0;
        }
        gloveCuffRange(cuff);
    }
    return kind;
}

void gloveCuffRange(float cuff[4])
{
    // The cuff over a bare forearm's end: the arms' skin block (make_vrbody.py: its rings 1/9 of the block's height
    // apart, the forearm's last 0.14 m over 2 of them, the wrist at lipVpos and the lip over the hand at 1) from
    // cuffBack metres behind the wrist to the lip, a hem cuffHem wide along its edge.
    constexpr float cuffBack = 0.045f, cuffHem = 0.006f;
    constexpr float pad = bodybare::skinPad, blockV = bodybare::skinBlockV - 2.f * pad;
    constexpr float perMetre = 2.f / 0.14f / 9.f; // the block's v (0..1) a metre along the forearm's end
    const float start = bodybare::lipVpos - cuffBack * perMetre;
    cuff[0] = bodybare::skinBlockU;
    cuff[1] = pad + blockV * start;
    cuff[2] = pad + blockV * (start + cuffHem * perMetre);
    cuff[3] = bodybare::skinBlockV;
}

int skinMaskWanted(const entity_t* e)
{
    float tone[4], glove[4], cuff[4];
    const int kind = skinTone(e, tone);
    return kind && (tone[3] != 0.f || gloveLook(e, glove, cuff)) ? kind : 0;
}

bool heldRoundRef(int hand, glm::vec3& out, float* radius)
{
    const int num = hand == 0 || hand == 1 ? held::heldEntity(hand) : 0;
    if(num <= 0 || num >= cl_max_edicts || !cl_entities[num].model)
    {
        return false;
    }
    const entity_t& e = cl_entities[num];
    out = glm::vec3{e.origin[0], e.origin[1], e.origin[2]};
    if(radius)
    {
        *radius = -1.f; // (a shell: its middle, no radius of its own)
    }
    for(const MagProp& m : magProps)
    {
        if(!strcmp(e.model->name, m.model))
        {
            ViewEntity tmp;
            tmp.ent = e;
            tmp.visible = true;
            out = modelPoint(tmp, glm::vec3{m.offset[0]->value, m.offset[1]->value, m.top + m.offset[2]->value});
            if(radius)
            {
                *radius = za::max(m.radius->value, 0.f);
            }
        }
    }
    if((modelmeta::has(e.model, modelmeta::Trait::LiveRound) || modelmeta::isQuakeGrenade(e.model)) &&
       !strstr(e.model->name, "prox"))
    {
        // A launcher's rocket or grenade (QC VR_Reload_RoundRef): its butt, its model's back end along its axis (its +x;
        // the multi-grenade's progs/mervup.mdl stands along its z: its bottom).
        ViewEntity tmp;
        tmp.ent = e;
        tmp.visible = true;
        out = modelPoint(tmp, modelmeta::is(e.model, modelmeta::Id::Mervup) ? glm::vec3{0.f, 0.f, e.model->mins[2]}
                                                                             : glm::vec3{e.model->mins[0], 0.f, 0.f});
    }
    return true;
}

ModelSpace::ModelSpace(const ViewEntity& ve) : origin_{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]}
{
    const entity_t& e = ve.ent;
    alias_ = e.model && e.model->type == mod_alias;
    if(!alias_)
    {
        return;
    }
    render::anchorMatrix(ve, glm::vec3{0.f}, m_);
    // anchorMatrix ends with the model's own scale and origin: undone for a point in model space (point).
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e.model));
    scaleOrigin_ = {hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
    scale_ = {hdr->scale[0], hdr->scale[1], hdr->scale[2]};
}

glm::vec3 ModelSpace::point(const glm::vec3& p) const
{
    if(!alias_)
    {
        return origin_;
    }
    const float* m = m_;
    const glm::vec3 v{(p.x - scaleOrigin_.x) / scale_.x, (p.y - scaleOrigin_.y) / scale_.y, (p.z - scaleOrigin_.z) / scale_.z};
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12], m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
        m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
}

glm::vec3 modelPoint(const ViewEntity& ve, const glm::vec3& point)
{
    return ModelSpace{ve}.point(point);
}

namespace
{
// A point of an entity's model's byte space (anchor::posedAnchor's) through its matrix (render::anchorMatrix's).
[[nodiscard]] glm::vec3 bytePoint(const float m[16], const glm::vec3& v)
{
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12], m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
        m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
}

} // namespace

glm::vec3 anchorPosition(const ViewEntity& ve, const char* anchor, const glm::vec3& extra)
{
    const entity_t& e = ve.ent;
    if(!e.model || e.model->type != mod_alias)
    {
        return {e.origin[0], e.origin[1], e.origin[2]};
    }
    float m[16];
    render::anchorMatrix(ve, extra, m);
    return bytePoint(m, anchor::posedAnchor(e, anchor, ve.zeroBlend));
}

glm::vec3 anchorPosition(const entity_t& e, bool mirrored, float zeroBlend, const char* anchor, const glm::vec3& extra)
{
    if(const ViewEntity* ve = find(&e))
    {
        return anchorPosition(*ve, anchor, extra);
    }
    if(!e.model || e.model->type != mod_alias)
    {
        return {e.origin[0], e.origin[1], e.origin[2]};
    }
    float m[16];
    render::entityMatrix(e, mirrored, e.scale ? e.scale : ENTSCALE_DEFAULT, extra, m);
    return bytePoint(m, anchor::posedAnchor(e, anchor, zeroBlend));
}

glm::vec3 vertexPosition(const ViewEntity& ve, int vertex)
{
    const entity_t& e = ve.ent;
    if(!e.model || e.model->type != mod_alias)
    {
        return {e.origin[0], e.origin[1], e.origin[2]};
    }
    float m[16];
    render::anchorMatrix(ve, glm::vec3{0.f}, m);
    return bytePoint(m, anchor::posedVertex(e, vertex, ve.zeroBlend));
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
    out.slot = slot;
    out.muzzle = anchorPosition(ve, weapons::anchor(slot, Key::MuzzleAnchor),
        weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
    return true;
}

int ammoPouchCells()
{
    const int left = cl.stats[protocol::STAT_QVR_POUCHCOUNT];
    if(!entities.ammoPouch.visible || (cl.stats[protocol::STAT_QVR_POUCHKIND] & 7) != 4 || left <= 0)
    {
        return 0;
    }
    return za::min(3, (left + 35) / 36);
}

bool ammoPouchCentre(glm::vec3& at)
{
    const ViewEntity& pouch = entities.ammoPouch;
    if(!pouch.visible || !pouch.ent.model)
    {
        return false;
    }
    // make_ammo_pouch.py/make_pouch.py: the leather's outline middle is BACK_X + depth * 0.45, z -2.3..1.5.
    // Empty frame 0 has depth 1.3; all stocked frames have depth 2.5. Bounds including the rounds vary by ammo kind.
    const float depth = pouch.ent.frame == 0 ? 1.3f : 2.5f;
    at = modelPoint(pouch, {0.1f + depth * 0.45f, 0.f, -0.4f});
    return true;
}

bool ammoPouchCell(int index, glm::vec3& at, glm::vec3& up)
{
    const ViewEntity& pouch = entities.ammoPouch;
    if(!pouch.visible || !pouch.ent.model || index < 0 || index > 2)
    {
        return false;
    }
    const glm::vec3 top{pouchCellX, pouchCellY[index], pouchCellTop};
    at = modelPoint(pouch, top);
    const glm::vec3 d = modelPoint(pouch, top + glm::vec3{0.f, 0.f, 1.f}) - at;
    up = glm::length(d) > 1e-6f ? glm::normalize(d) : glm::vec3{0.f, 0.f, 1.f};
    return true;
}

bool ammoPouchRounds(glm::vec3 (&box)[4])
{
    const ViewEntity& pouch = entities.ammoPouch;
    if(!ammoPouchPlaced || !pouch.ent.model)
    {
        return false;
    }
    // In its model units (make_ammo_pouch.py: +x out of the body, +y left, +z up): the full pouch's depth (BACK_X 0.1 to
    // 2.6), the rounds' row (the shells' and magazines' middles within 2.6 of its middle, their radii on top), from its rim
    // (Z_TOP 1.5) up to the magazines' tops (MAG_TOP 2.9; the shells' 2.5 to 2.7).
    constexpr glm::vec3 mid{1.35f, 0.f, 2.2f};
    constexpr glm::vec3 half{1.25f, 3.2f, 0.7f};
    box[0] = modelPoint(pouch, mid);
    box[1] = modelPoint(pouch, mid + glm::vec3{half.x, 0.f, 0.f}) - box[0];
    box[2] = modelPoint(pouch, mid + glm::vec3{0.f, half.y, 0.f}) - box[0];
    box[3] = modelPoint(pouch, mid + glm::vec3{0.f, 0.f, half.z}) - box[0];
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

bool holsterPoint(int stat, glm::vec3& at)
{
    for(int h = 0; h < HolsterCount; h++)
    {
        if(static_cast<int>(bodyHolster[h]) == stat && holsterSeen[h].placed >= host_framecount - 1)
        {
            at = holsterSeen[h].loops;
            return true;
        }
    }
    return false;
}

entity_t* holsteredWeapon(int stat)
{
    for(int h = 0; h < HolsterCount; h++)
    {
        ViewEntity& ve = entities.holster[h];
        if(static_cast<int>(bodyHolster[h]) == stat && holsterLive[h] && ve.visible && ve.ent.model && ve.ent.model->type == mod_alias)
        {
            return &ve.ent;
        }
    }
    return nullptr;
}

int gunPartsOf(const entity_t* gun, entity_t* out[2])
{
    if(!gun)
    {
        return 0;
    }
    const auto both = [&](ViewEntity& frame, ViewEntity& barrels) {
        int n = 0;
        if(frame.visible)
        {
            out[n++] = &frame.ent;
        }
        if(barrels.visible)
        {
            out[n++] = &barrels.ent;
        }
        return n;
    };
    for(int hand = 0; hand < 2; hand++)
    {
        if(gun == &entities.weapon[hand].ent)
        {
            if(entities.pumpBody[hand].visible) // (the shotgun's auto pump stroking: setupPumps)
            {
                return both(entities.pumpBody[hand], entities.pump[hand]);
            }
            return ssgHands[hand].parts ? both(entities.ssgFrame[hand], entities.ssgBarrels[hand]) : 0;
        }
    }
    for(int h = 0; h < HolsterCount; h++)
    {
        if(gun == &entities.holster[h].ent)
        {
            return ssgHolsters[h].parts ? both(entities.holsterSsgFrame[h], entities.holsterSsgBarrels[h]) : 0;
        }
    }
    for(int i = 0; i < maxWorldSsgs; i++)
    {
        if(gun == worldSsgSource[i])
        {
            return both(entities.worldSsgFrame[i], entities.worldSsgBarrels[i]);
        }
    }
    return 0;
}

const entity_t* gunPartSource(const entity_t* part)
{
    if(!part)
    {
        return nullptr;
    }
    for(int hand = 0; hand < 2; hand++)
    {
        if(part == &entities.ssgFrame[hand].ent || part == &entities.ssgBarrels[hand].ent ||
           part == &entities.pumpBody[hand].ent || part == &entities.pump[hand].ent || part == &entities.weaponMorph[hand].ent)
        {
            return &entities.weapon[hand].ent;
        }
    }
    for(int h = 0; h < HolsterCount; h++)
    {
        if(part == &entities.holsterSsgFrame[h].ent || part == &entities.holsterSsgBarrels[h].ent)
        {
            return &entities.holster[h].ent;
        }
    }
    for(int i = 0; i < maxWorldSsgs; i++)
    {
        if(part == &entities.worldSsgFrame[i].ent || part == &entities.worldSsgBarrels[i].ent)
        {
            return worldSsgSource[i];
        }
    }
    return nullptr;
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
        r_refdef.fov_x = glm::degrees(2.f * za::atan(c.tanX));
        r_refdef.fov_y = glm::degrees(2.f * za::atan(c.tanY));
        return;
    }
    const int eye = stereo::eye();
    // An eye already through a teleporter the body has not crossed yet (or not yet through one it has): seen from the room
    // it is in (vr_portals_walk), not from behind the gate's surface -- that frame or two showed the gate's hidden back.
    glm::vec3 origin = s.eyeOrigin[eye], angles = s.eyeAngles[eye];
    portals::eyeThrough(glm::vec3{s.playerOrigin.x, s.playerOrigin.y, origin.z}, origin, angles);
    deathview::eyeView(s, eye, origin, angles); // dead, Immersive: in the ragdoll's head (vr_death_view)
    for(int i = 0; i < 3; i++)
    {
        r_refdef.vieworg[i] = origin[i];
        r_refdef.viewangles[i] = angles[i];
    }

    // Only used for the near plane distance; the projection comes from the eye's FOV.
    const Fov& fov = frameState().eyes[eye].fov;
    r_refdef.fov_x = glm::degrees(fov.right - fov.left);
    r_refdef.fov_y = glm::degrees(fov.up - fov.down);
}

namespace view_detail
{
namespace
{
// Quake VR's grenade and proximity bomb models lack the smoke trail flag the old engine gave
// them when they loaded. Some of its heads (the dog's, the fiend's, the shambler's, the zombie's)
// lack the gib flag, so they left no blood trail: a head with no trail bleeds like a gib (the
// zombie's like a zombie's gibs).
// The world and the map load patchModelFlags last ran for: a new map can get the old one's qmodel_t (and its models
// reloaded, their flags reset), so the world pointer alone is not enough.
struct FlagsPatched
{
    const qmodel_t* world{nullptr};
    int generation{-1};
};
FlagsPatched flagsPatchedFor;
} // namespace

} // namespace view_detail

static void patchModelFlags()
{
    FlagsPatched& done = flagsPatchedFor;
    if(cl.worldmodel == done.world && worldGeneration() == done.generation)
    {
        return;
    }
    done = {cl.worldmodel, worldGeneration()};
    for(int i = 1; i < MAX_MODELS && cl.model_precache[i]; i++)
    {
        qmodel_t* m = cl.model_precache[i];
        if(modelmeta::is(m, modelmeta::Id::Grenade) || modelmeta::is(m, modelmeta::Id::Proxbomb))
        {
            m->flags |= EF_GRENADE;
        }
        constexpr int trails = EF_ROCKET | EF_GRENADE | EF_GIB | EF_TRACER | EF_ZOMGIB | EF_TRACER2 | EF_TRACER3;
        if(m->type == mod_alias && modelmeta::has(m, modelmeta::Trait::Head) && !(m->flags & trails))
        {
            m->flags |= modelmeta::is(m, modelmeta::Id::HZombie) ? EF_ZOMGIB : EF_GIB;
        }
    }
}

namespace view_detail
{
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

} // namespace view_detail

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
        chosenGripSlot[h] = -1;
    }
    fingerFramesTime = -1.0;
    forEachEntity([](view::ViewEntity& ve) {
        ve.lastModel = nullptr; // (its animation's lerp starts over: LERP_RESETANIM)
        ve.morph = 0.f;
    });
}

namespace view_detail
{
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
    const float k = h.strength * za::max(0.f, vr_parry_wobble.value);
    const float decay = za::exp(-t * 9.f);
    pos = h.dir * (k * decay * za::sin(t * 38.f + 0.9f));
    angles = glm::vec3{za::sin(t * 41.f), za::cos(t * 33.f), za::sin(t * 29.f)} * (k * 2.5f * decay);
}

} // namespace

} // namespace view_detail

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
            const glm::vec3 p = centre + a * (ra * za::cos(a0)) + left * (rl * za::sin(a0));
            const glm::vec3 q = centre + a * (ra * za::cos(a1)) + left * (rl * za::sin(a1));
            line(at(p.x, p.y, p.z), at(q.x, q.y, q.z));
        }
    };
    ellipse(face, 3.3f, 2.6f);
    ellipse(face - n * 1.4f, 3.3f, 2.6f);
    for(int i = 0; i < 4; i++)
    {
        const float ang = 6.2831853f * i / 4;
        const glm::vec3 p = face + a * (3.3f * za::cos(ang)) + left * (2.6f * za::sin(ang));
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
    za::Vector<glm::vec4>& spheres = scratch.boneSpheres;
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
        const grasp::Shape* shape = rh.held.ent ? heldShape(rh.held, rh.held.frame) : nullptr;
        const glm::mat4 shapeToWorld = shape ? heldToWorld(rh.held) : glm::mat4{1.f};
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
                lines::point(end, za::fmax(1.f, glm::distance(from, end) * 0.006f), colour);
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
    za::Vector<glm::vec4> (&spheres)[2] = scratch.collideSpheres;
    za::Vector<glm::vec4>& rig = scratch.collideRig;
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
                spheres[hand].pushBack(glm::vec4{glm::vec3{m * glm::vec4{glm::vec3{sp}, 1.f}}, sp.w * unit});
            }
            d.hand[hand] = &spheres[hand];
        }
        else if(entities.hand[hand][FingerBase].visible && entities.hand[hand][FingerBase].ent.model)
        {
            const avatar::HandPose hp = drawnHand(s, hand);
            const float k = units::metresToUnits() * units::bodyScale();
            for(const float along : {0.035f, 0.065f, 0.095f})
            {
                spheres[hand].pushBack(glm::vec4{hp.wrist + hp.forward * (along * k), 0.032f * k});
            }
            d.hand[hand] = &spheres[hand];
        }
        const view::ViewEntity& we = entities.weapon[hand];
        const int slot = weapons::slotForModel(we.ent.model);
        if(we.visible && we.ent.model && slot >= 0 && slot != weapons::fistSlot() && !isHandModel(we.ent.model))
        {
            d.weapon[hand] = &we.ent;
            d.mirrored[hand] = we.mirrored;
            const view::ViewEntity& mag = entities.mag[hand]; // (its seated magazine: vr_reload_mag_collide)
            if(vr_reload_mag_collide.value && mag.visible && mag.ent.model)
            {
                d.magazine[hand] = &mag.ent;
                d.magMirrored[hand] = mag.mirrored;
            }
        }
    }
    return d;
}

// vr_debug_hand_offset: each hand as drawn (`s`, after the view's pushes) against where it is tracked, and the parts: the
// walls (handpose: the hand and what it holds), the rest of the game's (the two-handed grip, the weight's spring), a knock
// or a tired shake, the models (and a prop in the other hand pressing its weapon), the body, a held prop's meet or wall
// push, and a free hand held out of the other hand's weapon or prop (vr_hand_collide, last frame's).
static void debugHandOffsets(const hands::State& s, const glm::vec3 knock[2], const glm::vec3 shake[2])
{
    for(int hand = 0; hand < 2; hand++)
    {
        glm::vec3 heldOff{0.f};
        const int prop = held::heldAlone(hand, &heldOff);
        const glm::vec3 other = rigHands[hand].pushed;
        const glm::vec3 total = s.pos[hand] + heldOff + other - s.unresolvedPos[hand];
        if(vr_debug_hand_offset.value < 2.f && glm::length(total) < 0.01f)
        {
            continue;
        }
        const glm::vec3 walls = handpose::wallPush(hand);
        const glm::vec3 models = modelcollide::drawnOffset(hand);
        const glm::vec3 body = selfcollide::drawnOffset(hand);
        const glm::vec3 look = knock[hand] + shake[hand];
        const glm::vec3 game = s.pos[hand] - s.unresolvedPos[hand] - models - body - look - walls;
        Con_Printf("handoffset %s: t %.3f total %.3f; walls %.3f, grip/weight %.3f, knock %.3f, models %.3f, body %.3f, "
                   "held %.3f, other hand's %.3f; holding %d (hand %.1f %.1f %.1f)\n",
            hand == HAND_MAIN ? "main" : "off", cl.time, glm::length(total), glm::length(walls), glm::length(game),
            glm::length(look), glm::length(models), glm::length(body), glm::length(heldOff), glm::length(other), prop ? prop : held::heldEntity(hand),
            s.pos[hand].x, s.pos[hand].y, s.pos[hand].z);
    }
}

namespace view_detail
{
namespace
{
hands::State unposedHands; // (VR_SetupViewEntities: the game's hands, put back after the posing mode's)
} // namespace

} // namespace view_detail

extern "C" void VR_SetupViewEntities()
{
    QVR_PROFILE("view entities");
    hands::State& s = hands::current();
    if(stereo::isRenderingEye())
    {
        if(s.valid)
        {
            QVR_PROFILE("view eye");
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
        s.loadPortValid[HAND_OFF] = s.loadPortValid[HAND_MAIN] = false;
        bodyblood::clear();
        return;
    }

    // (The steps' zones, "view ...": docs/vr-port/ROUND22.md, "View entities: zones and trims".)
    {
        QVR_PROFILE("view finger frames");
        updateFingerFrames();
    }

    // The hands as the game reads them, before the view moves the drawn ones (below): what lights the flashlight up.
    flashlight::noteGameHands(s);

    // The weapon posing mode (vr_posing.cpp) moves the hands for the view only: the game's put back after it.
    const bool posingNow = posing::active();
    hands::State& unposed = unposedHands;
    if(posingNow)
    {
        unposed = s;
    }

    // Parried blows knock the drawn hands (not the tracked ones the game uses): offset for the
    // view's setup, restored after it. Tired arms shake (vr_fatigue.cpp) and hits knock them (vr_painknock.cpp) the same
    // way, looks only (their muzzles put back below: the shots and the aim don't move).
    glm::vec3 knockPos[2], knockAngles[2], shakePos[2], shakeAngles[2];
    for(int hand = 0; hand < 2; hand++)
    {
        QVR_PROFILE("view shakes");
        impactOffset(hand, knockPos[hand], knockAngles[hand]);
        fatigue::shake(hand, shakePos[hand], shakeAngles[hand]);
        glm::vec3 painPos, painAngles;
        painknock::offset(hand, painPos, painAngles);
        shakePos[hand] += painPos;
        shakeAngles[hand] += painAngles;
        // A shot's programmatic recoil (vr_weaponfx.cpp), the same way.
        glm::vec3 recoilPos, recoilAngles;
        weaponfx::recoilOffset(hand, s.visualRot[hand], recoilPos, recoilAngles);
        shakePos[hand] += recoilPos;
        shakeAngles[hand] += recoilAngles;
        // A running chainsaw's engine (vr_chainsaw.cpp), the same way.
        glm::vec3 sawPos, sawAngles;
        chainsaw::shake(hand, sawPos, sawAngles);
        shakePos[hand] += sawPos;
        shakeAngles[hand] += sawAngles;
        s.pos[hand] += knockPos[hand] + shakePos[hand];
        s.rot[hand] += knockAngles[hand] + shakeAngles[hand];
        s.visualRot[hand] += knockAngles[hand] + shakeAngles[hand];
    }

    // Held out of the models they are pushed into (vr_model_collide): the weapons, hands and arms drawn moved (not while
    // posing: the floating weapon stays where it floats).
    if(!posingNow)
    {
        {
            QVR_PROFILE("view model collide");
            modelcollide::beginView(s);
        }
        // And out of each other, the arms, the gadget and the body (vr_body_collide): drawn only, as above.
        QVR_PROFILE("view self collide");
        selfcollide::beginView(s);
    }

    // vr_debug_hand_offset: each hand drawn away from where it is tracked, and by what (tests: a held prop never pushes
    // its own hand).
    if(vr_debug_hand_offset.value && s.valid && !posingNow)
    {
        debugHandOffsets(s, knockPos, shakePos);
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
        {
            QVR_PROFILE("view ground spots");
            updateGroundSpots(s); // (before the weapons: a carry taken by a hotspot now is drawn so this frame)
        }
        {
            QVR_PROFILE("view weapons");
            setupWeapon(s, HAND_MAIN, precachedModel(cl.stats[STAT_WEAPON]), cl.stats[STAT_WEAPONFRAME]);
            setupWeapon(s, HAND_OFF, precachedModel(cl.stats[STAT_QVR_WEAPONMODEL2]),
                cl.stats[STAT_QVR_WEAPONFRAME2]);
        }
        {
            QVR_PROFILE("view held mags");
            setupHeldMagazines(); // (before the hands: a hand holding one wraps it)
            setupGhosts();
        }
        {
            QVR_PROFILE("view free spots");
            updateFreeSpots(s); // (after the weapons: where an empty hand is on the other's, for the next frame's grip)
        }

        setupHand(s, HAND_MAIN);
        setupHand(s, HAND_OFF);
        QVR_PROFILE("view hotspots");
        showHotspots();
    }
    {
        QVR_PROFILE("view fists");
        for(int hand = 0; hand < 2; hand++)
        {
            updateFist(s, hand);
            updateGripFrame(s, hand);
        }
    }
    {
        QVR_PROFILE("view debug draws");
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
        else
        {
            box3d::activeRagdollDraw(); // vr_ragdoll_active_debug
        }
        ledges::debugDraw(); // vr_debug_ledges
        hitmodel::debugDraw(); // vr_debug_hits
        hitmodel::zonesDraw(); // vr_debug_hitzones
        rope::debugDraw();     // vr_debug_rope
        foegrab::neckDebugDraw(s.head); // vr_foegrab_necksnap_debug
        foegrab::grabDebugDraw(s.head); // vr_foegrab_lift_debug, vr_foegrab_throw_debug
        box3d::struggleDraw(s.head);    // vr_struggle_debug
        bodycontact::meleeArcDraw(s.head); // vr_enemy_melee_arc_show
        bodycontact::aimConeDraw(s.head);  // vr_enemy_aim_show
        if(vr_debug_hand_bones.value)
        {
            drawHandBones();
        }
    }
    // The guns not in a hand (holstered, lying round) show their screens (queued once a frame).
    int& idleTextFrame = oncePerFrame.idleTexts;
    const bool idleTexts = idleTextFrame != host_framecount;
    idleTextFrame = host_framecount;
    {
        QVR_PROFILE("view holsters");
        setupHolsters(s, idleTexts);
    }
    {
        QVR_PROFILE("view world weapons");
        setupWorldWeapons(s, idleTexts);
    }
    {
        QVR_PROFILE("view body");
        setupBody(s);
    }
    {
        QVR_PROFILE("view powerups pauldrons");
        powerupGlow(s); // the power-ups' glows and lights (Quad, pentagram, biosuit)
        setupPauldrons();
    }
    {
        QVR_PROFILE("view gadget");
        setupGadget(s);
    }
    {
        QVR_PROFILE("view skin masks");
        VR_PrecacheSkinMasks();
    }
    {
        QVR_PROFILE("view gadget touches");
        gadgetTouches(s, posingNow); // the screen tap and the side button, against the gadget and hands just drawn
    }
    {
        QVR_PROFILE("view gear");
        flashlight::setupView(s, entities.flashlight);
        chainsaw::setupView(s); // the chainsaw's starter cord (vr_chainsaw.cpp)
        setupSawHandle();
        setupPouch(s);
        setupAmmoPouch(s);
    }
    {
        QVR_PROFILE("view blood shock smoke");
        dripBlood(s);
        shock::frame(s); // the lightning gun in water's arcs and flash (vr_lg_water)
        smoulder::frame(); // smoke off the bodies the lightning struck or fire burnt (vr_smoulder)
        comfortfade::frame(); // the view back from black after a scripted teleport (vr_comfortfade)
    }
    {
        QVR_PROFILE("view buttons");
        setupButton(HAND_MAIN);
        setupButton(HAND_OFF);
        setupFrontButton(HAND_MAIN);
        setupFrontButton(HAND_OFF);
    }
    if(deathview::immersive())
    {
        // Dead, the view in the ragdoll's head (vr_death_view 2): the hands and the gear not drawn (they stay where you
        // stand, far from the body).
        forEachEntity([](view::ViewEntity& ve) { ve.visible = false; });
    }
    {
        QVR_PROFILE("view cords chains");
        cellcord::setupView(s); // the cell cords, from the laser cannon and the hammers to the pouch (vr_cellcord.cpp)
        adtank::setupView(s); // AD support: the flamethrowers' fuel tank and hose (vr_adtank.cpp)
        magholster::setupView(s); // the magnetic holsters' chains (vr_magholster.cpp)
    }
    if(!posingNow)
    {
        {
            QVR_PROFILE("view self collide end");
            selfcollide::endView(s, selfCollideDrawn(s));
        }
        QVR_PROFILE("view model collide end");
        const entity_t* const drawnWeapons[2]{&entities.weapon[HAND_OFF].ent, &entities.weapon[HAND_MAIN].ent};
        const bool drawnMirrored[2]{entities.weapon[HAND_OFF].mirrored, entities.weapon[HAND_MAIN].mirrored};
        modelcollide::endView(s, drawnWeapons, drawnMirrored); // the game reads the tracked hands and muzzles
    }
    for(int hand = 0; hand < 2; hand++)
    {
        const glm::vec3 shakenPos = s.pos[hand], shakenRot = s.visualRot[hand];
        s.pos[hand] -= knockPos[hand] + shakePos[hand];
        s.rot[hand] -= knockAngles[hand] + shakeAngles[hand];
        s.visualRot[hand] -= knockAngles[hand] + shakeAngles[hand];
        if(s.muzzleValid[hand] && (shakePos[hand] != glm::vec3{0.f} || shakeAngles[hand] != glm::vec3{0.f}))
        {
            // The muzzle, placed on the shaking (or hit) weapon, put back on the steady one (the shots and the aim don't
            // shake).
            const auto axes = [](const glm::vec3& angles) {
                glm::vec3 f, r, u;
                hands::angleVectors(angles, f, r, u);
                return glm::mat3{f, r, u};
            };
            const glm::mat3 shaken = axes(shakenRot), steady = axes(s.visualRot[hand] + knockAngles[hand]);
            s.muzzle[hand] = s.pos[hand] + knockPos[hand] + steady * (glm::transpose(shaken) * (s.muzzle[hand] - shakenPos));
        }
    }
    if(vr_debug_fatigue.value >= 2)
    {
        // Tests: the aim and the muzzle the game uses against the drawn weapon (the shake moves only the latter).
        const entity_t& we = entities.weapon[HAND_MAIN].ent;
        Con_Printf("fatigueaim %.4f aim %.5f %.5f %.5f muzzle %.4f %.4f %.4f drawn %.4f %.4f %.4f ang %.4f %.4f %.4f\n", vr_gametime,
            cl.viewangles[0], cl.viewangles[1], cl.viewangles[2], s.muzzle[HAND_MAIN].x, s.muzzle[HAND_MAIN].y, s.muzzle[HAND_MAIN].z,
            we.origin[0], we.origin[1], we.origin[2], we.angles[0], we.angles[1], we.angles[2]);
    }
    if(vr_debug_pain.value >= 2.f)
    {
        // Tests (Misc/quakevr/painknock): where the hand and the weapon in it are drawn this frame, against the tracked hand.
        for(int hand = 0; hand < 2; hand++)
        {
            const entity_t& we = entities.weapon[hand].ent;
            const entity_t& he = entities.hand[hand][FingerBase].ent;
            Con_Printf("painview %.3f %s weapon %d %.2f %.2f %.2f hand %d %.2f %.2f %.2f eye %.2f %.2f %.2f\n", cl.time,
                hand == HAND_MAIN ? "main" : "off", entities.weapon[hand].visible ? 1 : 0, we.origin[0], we.origin[1],
                we.origin[2], entities.hand[hand][FingerBase].visible ? 1 : 0, he.origin[0], he.origin[1], he.origin[2],
                s.eyeOrigin[0].x, s.eyeOrigin[0].y, s.eyeOrigin[0].z);
        }
    }
    if(posingNow)
    {
        drawnWeapons[0] = drawnWeapons[1] = view::DrawnWeapon{};
    }
    if(posingNow)
    {
        // The game's hands as they were; no muzzle (nothing is aimed or fired while posing) and no two-handed grip.
        s = unposed;
        for(int hand = 0; hand < 2; hand++)
        {
            s.muzzleValid[hand] = s.grip2HValid[hand] = s.loadPortValid[hand] = s.magBoxValid[hand] = false;
            s.onMagazine[hand] = false;
        }
    }
    else if(vrActive())
    {
        QVR_PROFILE("view weapon buttons");
        pressWeaponButtons(s);
    }
    for(int hand = 0; hand < 2; hand++)
    {
        s.placedFrom[hand] = s.pos[hand]; // the hands the muzzles and ports were placed on (hands::State::placedFrom)
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

    {
        QVR_PROFILE("view weapon parts");
        setupMagazines();
        setupPumps();
        adcrossbow::frame(entities.weapon, entities.adBolt); // AD support: the crossbow's string and bolt
        setupSsgParts();
        setupWorldSsgs();
        patchModelFlags();
    }

    // The screen may be redrawn more than once per frame (a modal dialog); add the entities only once.
    if(lastAddedFrame == host_framecount)
    {
        return;
    }
    lastAddedFrame = host_framecount;

    // The muzzle flashes on the weapons as drawn now (vr_weaponfx.cpp).
    {
        QVR_PROFILE("view muzzle flashes");
        weaponfx::frame(entities.weapon, entities.muzzleFlash, entities.enemyFlash);
    }

    {
        QVR_PROFILE("view add entities");
        glm::mat4 mirror;
        const bool cloned = bodyclone::mirror(mirror);
        int clonesDrawn = 0;
        forEachEntity([&](view::ViewEntity& ve) {
            if(ve.visible && ve.ent.model && !hiddenForShot(ve) && !drawnAsPump(ve) && !drawnInParts(ve) &&
               cl_numvisedicts < MAX_VISEDICTS)
            {
                cl_visedicts[cl_numvisedicts++] = &ve.ent;
                if(cloned && worn(ve) && cl_numvisedicts < MAX_VISEDICTS)
                {
                    cl_visedicts[cl_numvisedicts++] = &addClone(ve, mirror).ent;
                    clonesDrawn++;
                }
            }
        });
        if(cloned)
        {
            const view::ViewEntity& body = clones[&entities.body - reinterpret_cast<view::ViewEntity*>(&entities)];
            bodyclone::noteDrawn(clonesDrawn, entities.body.visible, {body.ent.origin[0], body.ent.origin[1], body.ent.origin[2]});
        }
    }

    {
        QVR_PROFILE("view effects");
        // Spent casings thrown out of the weapons (vr_shells.cpp).
        shells::frame(entities.weapon);
        collectfx::frame(s); // things put away, going into their holster or pouch (vr_collectfx.cpp)
        explosiondebris::frame();
        fireparticles::frame();
        bloodstream::frame(); // blood pouring from bullet holes (vr_bloodstream.cpp), the monsters and ragdolls posed
    }

    // Wounds painted on the models (vr_wounds.cpp), the body and the hands posed.
    QVR_PROFILE("view wounds");
    wounds::frame();
}
