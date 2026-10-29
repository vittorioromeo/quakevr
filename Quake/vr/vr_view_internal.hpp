#pragma once

// vr_view_internal.hpp -- what vr_view.cpp (see vr_view.hpp) shares with its parts: vr_view_weapon.cpp (the
// weapon's set-up), vr_view_hands.cpp (the hands') and vr_view_diag.cpp (debug and test commands). Internal:
// included by those alone. What was the file's anonymous namespace and more than one part uses is namespace
// view_detail (not view::detail: view:: is written all over the file, and a second `view` would make each
// ambiguous): its types and variables here (each variable one object: inline), its functions declared here
// and defined in the file that owns them.

#include "vr_portals.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_hitmodel.hpp"
#include "vr_rope.hpp"
#include "vr_touch.hpp"
#include "vr_view.hpp"
#include "vr_engine.hpp"
#include "vr_backend.hpp"
#include "vr_units.hpp"
#include "vr_color.hpp"
#include "vr_anchor.hpp"
#include "vr_autopump.hpp"
#include "vr_adtank.hpp"
#include "vr_cellcord.hpp"
#include "vr_chainsaw.hpp"
#include "vr_magholster.hpp"
#include "vr_collectfx.hpp"
#include "vr_bodycontact.hpp"
#include "vr_climb.hpp"
#include "vr_foegrab.hpp"
#include "vr_avatar.hpp"
#include "vr_bullettime.hpp"
#include "vr_gadget.hpp"
#include "vr_gearlights.hpp"
#include "vr_panel.hpp"
#include "vr_flashlight.hpp"
#include "vr_body.hpp"
#include "vr_bodyclone.hpp"
#include "vr_bodyblood.hpp"
#include "vr_bloodprints.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_emissive.hpp"
#include "vr_fatigue.hpp"
#include "vr_hands.hpp"
#include "vr_jobs.hpp"
#include "vr_pouchitems.hpp"
#include "vr_handrig.hpp"
#include "vr_grasp.hpp"
#include "vr_grip.hpp"
#include "vr_gripfit.hpp"
#include "vr_held.hpp"
#include "vr_ledges.hpp"
#include "vr_lines.hpp"
#include "vr_protocol.hpp"
#include "vr_client.hpp"
#include "vr_render.hpp"
#include "vr_shells.hpp"
#include "vr_explosiondebris.hpp"
#include "vr_fireparticles.hpp"
#include "vr_flamewobble.hpp"
#include "vr_shock.hpp"
#include "vr_smoulder.hpp"
#include "vr_comfortfade.hpp"
#include "vr_deathview.hpp"
#include "vr_stereo.hpp"
#include "vr_window.hpp"
#include "vr_text3d.hpp"
#include "vr_trace.hpp"
#include "vr_twohand.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_modelcollide.hpp"
#include "vr_handpose.hpp"
#include "vr_selfcollide.hpp"
#include "vr_posing.hpp"
#include "vr_painknock.hpp"
#include "vr_weaponfx.hpp"
#include "vr_sightalign.hpp"
#include "vr_bodycal.hpp"
#include "vr_drawblend.hpp"
#include "vr_profile.hpp"
#include "vr_props.hpp"
#include "vr_ragdoll.hpp"
#include "vr_menu.hpp"
#include "vr_weapons.hpp"
#include "vr_wounds.hpp"
#include "vr_bloodstream.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Replace.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/UIntPtrT.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Atan.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmin.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/String/ToString.hpp"
#include "Zancle/Vocabulary/Pair.hpp"
#include "vr_zancle.hpp"

#include <string.h>

using namespace qvr;
using namespace qvr::protocol;
using weapons::Key;

namespace view_detail
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
inline float fingerFrames[2][FingerCount]{};
[[nodiscard]] float targetCurl(const HandInput& in, int finger, bool indexWithGrip);

// Round 21: the finger tweaks on a held weapon (its fgr_bias_* keys: a share of a full curl, negative more open), on
// top of what the fingers do (the controller's curl, the grasp's wrap). Blended like the curls, so a weapon's tweaks
// ease in as it is taken. 0 for an empty hand and a hand helping hold the other's weapon.
inline float fingerBias[2][FingerCount]{};

// A finger's drawn curl (the six models): the frame, and the blend of it towards frame 0 (open: ViewEntity::zeroBlend)
// for a curl between frames that a weapon's finger tweak leaves (the curls alone are whole frames, as they were).
struct FingerPose
{
    int frame;
    float open;
};
[[nodiscard]] FingerPose fingerPose(int hand, int finger);

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
constexpr int maxWorldWeapons = 48; // (vr_weapon_world_attach_max of them; their range vr_weapon_world_attach_range)
constexpr int maxWorldSsgs = 4; // super shotguns lying about broken open, drawn open (setupWorldSsgs)

// The guns' loading ports (immersive reloading; docs/vr-port/RELOAD.md): where a round held in the other hand goes
// in, in the model's space (+x forward, +y left, +z up, frame 0; mirrored with the model in the off hand): the shotgun's
// under its receiver (polish_weapons.py loading_port: the opening's middle, a little below its frame). Sent to the server
// with each move (VrMove::loadPort -> .loadportpos, .offloadportpos), which loads a round held within the gun's radius
// of it (QC vr_reload.qc).
// Each moved by its gun's Load Point offsets and sized by its radius (Weapons > Reloading: vr_reload_port_<gun>_x/y/z,
// model units: +x forward, +y left, +z up; _radius, world units), tuned with vr_reload_show_ports.
struct LoadPort
{
    modelmeta::Id model;
    glm::vec3 point;
    cvar_t* offset[3];
    cvar_t* radius;
    bool magazine;      // a magazine's well (the pull's reach too); else a shell port
    bool front = false; // a launcher's muzzle (front loaded: a round goes in butt first, back along the barrel)
    float depth = 0.f;  // and how far in a round slides (model units: view::loadPath)
    bool side = false;  // a shell port on the gun's inner side (+y; mirrored in the off hand): its opening faces out there
};
constexpr LoadPort loadPorts[] = {
    {modelmeta::Id::VShot, {13.6f, 0.f, 0.3f}, {&vr_reload_port_shot_x, &vr_reload_port_shot_y, &vr_reload_port_shot_z},
        &vr_reload_port_shot_radius, false},
    // The super shotgun's breech face, between its chambers (make_ssg_open.py's barrels: ssgBreech), turned down with the
    // barrels as it is drawn open (setupWeapon): a held taped pair's middle there loads both.
    {modelmeta::Id::VShot2, {12.7f, 0.f, 7.f}, {&vr_reload_port_sshot_x, &vr_reload_port_sshot_y, &vr_reload_port_sshot_z},
        &vr_reload_port_sshot_radius, false},
    // The magazine guns' wells: where the attached magazine's top sits when seated (make_mags.py's seats): a held
    // magazine's top (its reference point: magProps) brought there, within the two radii, seats it.
    {modelmeta::Id::VNail, {6.9f, 0.f, -1.45f}, {&vr_reload_port_nail_x, &vr_reload_port_nail_y, &vr_reload_port_nail_z},
        &vr_reload_port_nail_radius, true},
    {modelmeta::Id::VLava, {6.9f, 0.f, -1.45f}, {&vr_reload_port_nail_x, &vr_reload_port_nail_y, &vr_reload_port_nail_z},
        &vr_reload_port_nail_radius, true},
    {modelmeta::Id::VNail2, {7.2f, 5.17f, 1.76f},
        {&vr_reload_port_snail_x, &vr_reload_port_snail_y, &vr_reload_port_snail_z}, &vr_reload_port_snail_radius, true},
    {modelmeta::Id::VLava2, {7.2f, 5.17f, 1.76f},
        {&vr_reload_port_snail_x, &vr_reload_port_snail_y, &vr_reload_port_snail_z}, &vr_reload_port_snail_radius, true},
    {modelmeta::Id::VLight, {11.6f, 0.f, 3.f},
        {&vr_reload_port_light_x, &vr_reload_port_light_y, &vr_reload_port_light_z}, &vr_reload_port_light_radius, true},
    {modelmeta::Id::VPlasma, {11.6f, 0.f, 3.f},
        {&vr_reload_port_light_x, &vr_reload_port_light_y, &vr_reload_port_light_z}, &vr_reload_port_light_radius, true},
    // The nail pistol's: its grip's bottom, where its magazine's top goes in (make_nailpistol_mags.py: 5.25 down the grip
    // from the seated top), slapped up into the grip from there.
    // The heavy revolver's: its cylinder's back face's middle (on its axis; README: 3.764 0 5.608 home), on the crane swung
    // out (modelLoadPort): a held shell there goes into the empty chamber nearest it (QC vr_revolver.qc).
    {modelmeta::Id::VRevolver, {3.764f, 0.f, 5.608f},
        {&vr_reload_port_revolver_x, &vr_reload_port_revolver_y, &vr_reload_port_revolver_z}, &vr_reload_port_revolver_radius, false},
    {modelmeta::Id::VNailPistol, {-0.6237f, 0.f, -2.934f},
        {&vr_reload_port_npistol_x, &vr_reload_port_npistol_y, &vr_reload_port_npistol_z}, &vr_reload_port_npistol_radius, true},
    // AD support: AD's Plasma Gun: its cell's well on its inner side (vr_adweapons.cpp plasmaSeat; vr_view.cpp magMounts).
    {modelmeta::Id::AdVrPlasma, {11.6f, 2.1f, 4.5f},
        {&vr_reload_port_adplasma_x, &vr_reload_port_adplasma_y, &vr_reload_port_adplasma_z}, &vr_reload_port_adplasma_radius,
        true},
    // AD support: the Widowmaker's shell port (his VR notes 2026-10-18), on its receiver's inner side by the rear handle
    // (vr_adweapons.cpp widowmakerParts: the opening's middle, on the side's face): a handful held there goes into its tube.
    {modelmeta::Id::AdVrWidowmaker, {6.5f, 3.6f, 5.6f},
        {&vr_reload_port_widow_x, &vr_reload_port_widow_y, &vr_reload_port_widow_z}, &vr_reload_port_widow_radius, false,
        false, 0.f, true},
    // The launchers' muzzles (front loaded; QC vr_reload.qc, "Front-loaded launchers"): the middle of the barrel's mouth
    // (measured on the models: the grenade launcher's hexagonal barrel ends at x 31.4, its middle 5.9 up; the rocket
    // launcher's tube at x 56.1, 4.55 up), the way in back along the barrel (-x), the opening facing forward.
    {modelmeta::Id::VRock, {31.4f, 0.f, 5.9f}, {&vr_reload_port_gl_x, &vr_reload_port_gl_y, &vr_reload_port_gl_z},
        &vr_reload_port_gl_radius, false, true, 9.f},
    {modelmeta::Id::VMulti, {31.45f, 0.f, 6.f}, {&vr_reload_port_gl_x, &vr_reload_port_gl_y, &vr_reload_port_gl_z},
        &vr_reload_port_gl_radius, false, true, 9.f},
    {modelmeta::Id::VProx, {31.4f, 0.f, 5.95f}, {&vr_reload_port_prox_x, &vr_reload_port_prox_y, &vr_reload_port_prox_z},
        &vr_reload_port_prox_radius, false, true, 8.f},
    {modelmeta::Id::VRock2, {56.1f, 0.f, 4.55f}, {&vr_reload_port_rl_x, &vr_reload_port_rl_y, &vr_reload_port_rl_z},
        &vr_reload_port_rl_radius, false, true, 16.f},
    {modelmeta::Id::VMulti2, {56.15f, 0.f, 4.55f}, {&vr_reload_port_rl_x, &vr_reload_port_rl_y, &vr_reload_port_rl_z},
        &vr_reload_port_rl_radius, false, true, 16.f},
};

// The magazines drawn in the magazine guns (immersive reloading's phase 2; QC vr_reload.qc): each gun's attached
// magazine model (make_mags.py: vr_mag_on_<gun>.mdl, made in the gun's model space; its Scale and offsets the gun's:
// vr_weapons.cpp makeModelTransform), drawn with the gun's place, turn and mirroring while the server's mode is
// Immersive and the gun has its magazine in (not QVR_WPNFLAG_NOMAG). `anchor`: a point of the gun's body by the
// magazine (vr_anchor.hpp) whose move from frame 0 (the firing animation's kick) moves it too. `well`: its
// receiver (make_mags.py vr_magwell_on_<gun>.mdl: a collar round the magazine's top, flush with the gun at `seat`), drawn
// in Immersive whether a magazine is in or not, moved and turned about its seat by vr_reload_well_<gun>_* (visual only).
// `centre`: the magazine's middle, where the gun's two-handed grip is in Immersive (the author: gripping the magazine is
// the two-handed aim; it comes out only pulled off hard, snapped, or the hands far apart: QC vr_reload.qc).
struct MagMount
{
    modelmeta::Id gun;
    const char* model;
    const char* anchor;
    const char* well;
    glm::vec3 seat;
    glm::vec3 centre;
    cvar_t* wellOffset[6]; // x, y, z (model units), pitch, yaw, roll (degrees)
    int kind;              // its magazine (magHolds): 0 the nailgun's, 1 the super nailgun's, 2 the thunderbolt's cell, 3 the nail pistol's
};
#undef QVR_WELL_OFFSETS

// The other hand holding a gun's attached magazine, per magazine (MagMount::kind; Weapons > Reloading, each gun's page:
// vr_reload_hold_<kind>_*): its drawn pose's offset there (x y z units, pitch yaw roll degrees: visual only) and its
// fingers (fitted round the magazine, or each at its curl).
struct MagHold
{
    cvar_t* offset[6];
    cvar_t* fingers;
    cvar_t* curl[handrig::FingerCount];
};
#define QVR_MAG_HOLD(k)                                                                                                     {{&vr_reload_hold_##k##_x, &vr_reload_hold_##k##_y, &vr_reload_hold_##k##_z, &vr_reload_hold_##k##_pitch,                  &vr_reload_hold_##k##_yaw, &vr_reload_hold_##k##_roll},                                                               &vr_reload_hold_##k##_fingers,                                                                                         {&vr_reload_hold_##k##_curl_thumb, &vr_reload_hold_##k##_curl_index, &vr_reload_hold_##k##_curl_middle,                    &vr_reload_hold_##k##_curl_ring, &vr_reload_hold_##k##_curl_pinky}}
constexpr MagHold magHolds[] = {QVR_MAG_HOLD(nail), QVR_MAG_HOLD(snail), QVR_MAG_HOLD(light), QVR_MAG_HOLD(npistol)};
#undef QVR_MAG_HOLD

// An attached magazine's shape (magazineBox): the box round its model's vertices (frame 0) in the gun's model space, along
// its length (from its seat to its middle: MagMount) and across it. Kept per model (models' slots: a game change, a
// model reload).
struct MagShape
{
    const qmodel_t* model{nullptr};
    glm::vec3 centre{0.f};
    glm::vec3 axis[3]{glm::vec3{0.f}, glm::vec3{0.f}, glm::vec3{0.f}}; // unit: along it (towards the seat), across, through
    float half[3]{0.f, 0.f, 0.f};
};
struct MagShapeCache
{
    za::Vector<MagShape> shapes;
    auto members() { return qvr::mem::list(shapes); }
};
inline mem::Cache<MagShapeCache> magShapeCache{"magazine shapes", mem::GameDirChange | mem::ModelReload};
[[nodiscard]] float boxDistance(const glm::vec3 (&box)[4], const glm::vec3& p);
[[nodiscard]] const MagMount* magMountFor(const qmodel_t* gun);
constexpr int weaponFlagNoMag = 16; // QC's QVR_WPNFLAG_NOMAG (vr_defs.qc)

struct SsgDrawn
{
    float angle{0.f};             // degrees the barrels are drawn turned down
    double time{-1.0};            // vr_gametime of its last step
    const qmodel_t* gun{nullptr}; // the model it was for (another gun: no animation)
    bool parts{false};            // drawn in its parts this frame (the gun itself not drawn)
};
inline SsgDrawn ssgHands[2];
// The heavy revolver in a hand (QC vr_revolver.qc; STAT_QVR_REVOLVER*, STAT_QVR_REVOLVERCYL*), drawn in its parts
// (setupRevolverParts): its crane swung out `swing` degrees (0 home .. 95 out, about the bore, to its +y), its cylinder
// turned `cyl` degrees (its model's +y towards +z), both stepped smoothly towards the server's (revolverStep).
struct RevolverDrawn
{
    float swing{0.f};
    float cyl{0.f};
    double time{-1.0};            // vr_gametime of its last step
    const qmodel_t* gun{nullptr}; // the model it was for (another gun: no animation)
    bool parts{false};            // drawn in its parts this frame (the gun itself not drawn)
};
inline RevolverDrawn rvHands[2];
float revolverStep(RevolverDrawn& st, const qmodel_t* gun, int hand);
// A point of v_revolver.mdl (its Body's frame) on its crane, the crane swung out `swing` degrees.
[[nodiscard]] glm::vec3 revolverCraneTurn(const glm::vec3& p, float swing);
float ssgStep(SsgDrawn& st, const qmodel_t* gun, int flags);
[[nodiscard]] glm::vec3 ssgTurnWorld(const view::ViewEntity& gun, const glm::vec3& w, float deg);
[[nodiscard]] glm::mat3 ssgWorldTurn(const view::ViewEntity& gun, float deg);

struct Entities
{
    view::ViewEntity weapon[2];
    view::ViewEntity weaponMorph[2]; // the model a gun is morphing out of (its other ammo's: vr_weapon_morph_time)
    view::ViewEntity hand[2][FingerCount];
    view::ViewEntity holster[HolsterCount];
    view::ViewEntity holsterSlot[HolsterCount];
    view::ViewEntity holsterButton[HolsterCount]; // the buttons of the holstered weapons,
    view::ViewEntity worldButton[maxWorldWeapons]; // and of the weapons lying round (vr_weapon_screen_idle)
    view::ViewEntity holsterFrontButton[HolsterCount]; // and the grappling guns' front buttons there
    view::ViewEntity worldFrontButton[maxWorldWeapons];
    view::ViewEntity body;
    view::ViewEntity pauldron[2];    // per side of the body (0 left): the cap,
    view::ViewEntity pauldronArm[2]; // and the lames round the upper arm
    view::ViewEntity gadget;
    view::ViewEntity gadgetStrap[2]; // round the forearm under its lugs (the elbow's side, the wrist's)
    view::ViewEntity flashlight; // vr_flashlight.cpp
    view::ViewEntity pouch;      // the grenade pouch at the small of the back (vr_handgrenade)
    view::ViewEntity ammoPouch;  // the ammo pouch on the front of the belt (vr_reload_mode 3)
    view::ViewEntity ammoPouchItem[pouchitems::most]; // and the rounds in it (setupAmmoPouchItems; vr_pouchitems.cpp)
    view::ViewEntity mag[2];                       // the magazines in the guns in the hands (setupMagazines),
    view::ViewEntity holsterMag[HolsterCount];     // in the holstered ones,
    view::ViewEntity worldMag[maxWorldWeapons];    // and in the ones lying nearest (setupWorldWeapons' nearest)
    view::ViewEntity well[2];                      // the magazine guns' wells (receivers): in the hands,
    view::ViewEntity holsterWell[HolsterCount];    // holstered,
    view::ViewEntity worldWell[maxWorldWeapons];   // and lying nearest
    view::ViewEntity pumpBody[2]; // the shotgun in a hand while its auto pump strokes: the gun without its fore-end,
    view::ViewEntity pump[2];     // drawn instead of it, and the fore-end, slid back (setupPumps)
    view::ViewEntity ssgFrame[2];                  // the super shotgun open (setupSsgParts): its frame and barrels,
    view::ViewEntity ssgBarrels[2];                // in the hands,
    view::ViewEntity holsterSsgFrame[HolsterCount]; // and holstered
    view::ViewEntity holsterSsgBarrels[HolsterCount];
    view::ViewEntity worldSsgFrame[maxWorldSsgs];   // and lying about (setupWorldSsgs)
    view::ViewEntity worldSsgBarrels[maxWorldSsgs];
    view::ViewEntity rvFrame[2];   // the heavy revolver in the hands, in its parts (setupRevolverParts): the frame with its
    view::ViewEntity rvCrane[2];   // hammer and trigger, the crane, the cylinder and the shells in its chambers
    view::ViewEntity rvCyl[2];
    view::ViewEntity rvShell[2][6];
    view::ViewEntity sawHandle;  // the chainsaw's cord's handle out of its seat (vr_chainsaw.cpp handleEntity)
    view::ViewEntity button[2];
    view::ViewEntity frontButton[2]; // the grappling gun's second button, near the muzzle (the reel-in)
    view::ViewEntity ghost[2]; // the motion review's ghost of a take's weapons (view::setGhost)
    view::ViewEntity ghostHand[2]; // Ghost Hands: each hand at its real controller (setupGhostHands)
    view::ViewEntity muzzleFlash[2]; // the weapons' programmatic muzzle flashes (vr_weaponfx.cpp)
    view::ViewEntity enemyFlash[weaponfx::maxEnemyFlashes]; // and the grunts' guns'
    view::ViewEntity adBolt[2]; // AD support: the bolts in the hands' crossbows' grooves (vr_adcrossbow.cpp)
};

inline Entities entities;

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
    f(entities.ammoPouch);
    for(view::ViewEntity& ve : entities.ammoPouchItem)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.mag)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.holsterMag)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.worldMag)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.well)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.holsterWell)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.worldWell)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.pumpBody)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.pump)
    {
        f(ve);
    }
    for(int i = 0; i < 2; i++)
    {
        f(entities.ssgFrame[i]);
        f(entities.ssgBarrels[i]);
    }
    for(int h = 0; h < HolsterCount; h++)
    {
        f(entities.holsterSsgFrame[h]);
        f(entities.holsterSsgBarrels[h]);
    }
    for(int i = 0; i < maxWorldSsgs; i++)
    {
        f(entities.worldSsgFrame[i]);
        f(entities.worldSsgBarrels[i]);
    }
    for(int i = 0; i < 2; i++)
    {
        f(entities.rvFrame[i]);
        f(entities.rvCrane[i]);
        f(entities.rvCyl[i]);
        for(view::ViewEntity& ve : entities.rvShell[i])
        {
            f(ve);
        }
    }
    f(entities.sawHandle);
    for(view::ViewEntity& ve : entities.button)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.frontButton)
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
    for(view::ViewEntity& ve : entities.holsterFrontButton)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.worldFrontButton)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.ghost)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.ghostHand)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.muzzleFlash)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.enemyFlash)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : entities.adBolt)
    {
        f(ve);
    }
}
[[nodiscard]] bool isHandModel(const qmodel_t* model);
[[nodiscard]] qmodel_t* precachedModel(int index);

// Mod_ForName(name, false) for the view entities' own models, asked for every frame: the model found for the
// same name (string) last time, while it is still that one and loaded (Mod_LoadModel's own check), without
// Mod_FindName's walk through every model known. A model that isn't there is remembered too, until the next map (or
// game) is loaded: not looked for on disk every frame.
struct ViewModelEntry
{
    const char* name;
    qmodel_t* model;
    int generation;
};
// 256 entries in pairs (two ways): a name goes in its hashed slot or the slot beside it, so two names that hash alike
// don't push each other out every frame.
inline ViewModelEntry viewModels[256]{};

// The bare forearms' skin at the wrist turned to the hand's (view::skinTint): make_vrbody.py's "wrist tint", the jointed
// hand's clean skin round its wrist over the arms' skin (progs/vrbody_00_00.tga) there, channel by channel.
// Generated by make_vrbody.py (vr_bodybare.inc).
namespace bodybare
{
#include "vr_bodybare.inc"
} // namespace bodybare
inline constexpr const float(&bareTint)[3] = bodybare::tint;

using view::viewModel;

// The drawn hands' buffers, each frame (the main thread: the grasp solver runs there).
struct ViewScratch
{
    za::Vector<glm::vec3> restVerts, nowVerts;     // a held model's vertices at rest and as drawn (animationMotion)
    za::Vector<float> weight;                      // each vertex's weight by its distance (animationMotion)
    grasp::Shape otherHand;                         // the other hand as a shape, for a cupped grasp
    za::Vector<grasp::Triangle> otherHandTris;     // its triangles
    za::Vector<glm::vec3> otherHandVerts;          // its vertices, posed
    za::Vector<glm::vec4> handSpheres;             // a hand's grasp spheres, posed (emptyHandSpheres)
    za::Vector<glm::vec4> fistSpheres;             // the closed empty hand's, in the hand (updateFist)
    za::Vector<glm::vec4> boneSpheres;             // vr_debug_hand_bones's
    za::Vector<glm::vec4> collideSpheres[2];       // vr_body_collide's hands (selfCollideDrawn: Drawn views them)
    za::Vector<glm::vec4> collideRig;              // and the rig's
    za::Vector<glm::vec3> limbPoints;              // a held ragdoll limb's triangles, as the rig gives them (limbHold)
    za::Vector<grasp::Triangle> limbTris;          // and as its shape is made of
    za::Vector<glm::vec3> magRest;                 // an attached magazine's vertices (magazineShape)
    za::Vector<glm::vec3> magNow;
    za::Vector<glm::vec4> anywhereFist;            // an empty hand's fist in the world (fistSurfaceGap)
    za::Vector<glm::vec3> buttMiddle;              // a held gun's butt's points (heldWeaponButt)
    za::Vector<glm::vec3> surfaceVerts;            // a drawn hand's vertices, posed (drawnHandSurface)
    za::Vector<glm::vec4> openSpheres;             // the open empty hand's spheres, in the hand (updateGripFrame: the palm's skin)
    auto members()
    {
        return qvr::mem::list(restVerts, nowVerts, weight, otherHand, otherHandTris, otherHandVerts, handSpheres, fistSpheres,
            boneSpheres, collideSpheres, collideRig, limbPoints, limbTris, magRest, magNow, anywhereFist,
            buttMiddle, surfaceVerts, openSpheres);
    }
};
inline mem::Scratch<ViewScratch> scratch{"view hands"};
void place(view::ViewEntity& ve, qmodel_t* model, const glm::vec3& origin, const glm::vec3& angles,
    int frame, bool mirrored);

// ----------------------------------------------------------------------------
// Weapons

// Per hand: the pose its weapon is drawn from this frame (the hand's own, or, for a gun carried by
// its foregrip, the pose of the hand that let it go: twohand::carriedWeapon). Everything attached
// to the weapon (the ammo screen, the button) is placed from it, never from the hand.
inline bool weaponCarried[2]{}; // the weapon in the hand is a gun carried by its foregrip (twohand::carriedWeapon)
inline twohand::HeldAs drawnAs[2]{{glm::vec3{0.f}, glm::vec3{0.f}, true}, {glm::vec3{0.f}, glm::vec3{0.f}, false}};
[[nodiscard]] glm::vec3 composeAngles(const glm::vec3& rot, const glm::vec3& offsets);
[[nodiscard]] glm::vec3 weaponAngleOffsets(int slot, bool mirrored);
[[nodiscard]] glm::vec3 idleAttachmentAngles(const glm::vec3& aliasAngles, int slot, bool mirrored, const glm::vec3& offsets);

// Clip sizes seen on the guns in the hands (the stats only give the held guns'), for the holstered ones' screens.
[[nodiscard]] za::String idleWeaponText(const qmodel_t* model, int clip, int clipSize, bool total);
[[nodiscard]] bool idleAmmoWarning(const qmodel_t* model, int clip, int clipSize, glm::vec3& tint, const char* seenCvar);

// Floating ammo counter on a weapon (old engine's V_SetupWpnTextViewEnt and the weapon text
// in R_DrawViewModels): clip/clip size over the ammo left when reloading is on, else the ammo.
// What is done once a frame however often the view is set up: the frame each last ran in (host_framecount).
struct OncePerFrame
{
    int weaponText[2]{-1, -1}; // queueWeaponText, per hand
    int quadArcs = -1;         // quadArcs
    int idleTexts = -1;        // VR_SetupViewEntities: the guns not in a hand show their screens
    int gadgetTouches = -1;    // gadgetTouches
};
inline OncePerFrame oncePerFrame;
inline glm::vec3 carriedTip[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // a carried weapon's tip as drawn (setupWeapon)
inline bool carriedTipValid[2]{false, false};
inline glm::vec3 carriedAngles[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // and its entity's angles, the pitch negated (QC's thrown weapon's)
inline glm::vec3 carriedInHand[2][2]{}; // and its handle and tip in the carrying hand's frame (view::carriedWeaponInHand)

// vr_grapple_debug's memory: what was printed last, and when the next print may come.
struct GrappleDebug
{
    int drawnFrame[2]{-1, -1};  // each hand's grappling gun's frame as drawn (setupWeapon)
    bool holsteredOut = false;  // the holstered gun drawn empty (holsteredGrappleOut)
    double idleLogAt = 0.0;     // idleAttachments (vr_grapple_debug 3)
    double frontLogAt = 0.0;    // setupFrontButton (likewise)
};
inline GrappleDebug grappleDebug;
[[nodiscard]] int morphKind(const qmodel_t* a, const qmodel_t* b);

struct Morph
{
    const qmodel_t* last{nullptr}; // the hand's gun model last frame
    qmodel_t* from{nullptr};       // morphing from this one (null: not morphing)
    int kind{0};
    double start{0.0};
};
inline Morph morphs[2];
[[nodiscard]] glm::vec3 lavaGlowPosition(const entity_t& e, bool mirrored, float zeroBlend, int slot);
[[nodiscard]] glm::mat4 hotspotFrame(const entity_t& e, bool mirrored);

// The held weapons as drawn this frame, for their bodies in Box3D (view::drawnWeapon): the entity's rigid place and turn
// (R_EntityMatrix's, as hotspotFrame) relative to the pose of the hand it was placed from (setupWeapon's: the drawn
// hand, moved out of what it was pushed into, knocked back, lagging with the weapon's weight), so that the body follows
// the tracked hand with the weapon where the hand holds it -- not held out of a prop it is swung into (vr_model_collide).
// None for the fist or a non-alias model.
inline view::DrawnWeapon drawnWeapons[2];
[[nodiscard]] glm::mat3 anglesBasis(const glm::vec3& a);
[[nodiscard]] glm::vec3 basisAngles(const glm::mat3& b);
[[nodiscard]] glm::vec3 weaponTurn(const glm::vec3& rot, int slot, bool mirrored);

// A hand's turn carried by a weapon (round 21, second pass). The weapon's Pitch/Yaw/Roll are added to the hand's
// angles, which is no rigid turn: a hand turned by angle offsets of its own would slide round the weapon as the wrist
// turns (up to a few degrees), and its grasp would be solved again and again. The hand keeps instead the turn it has
// on the weapon at `rot` 0 (`handAt0`: its angles there), however the weapon is turned.
// Round 21, third pass: the turn of the weapon held by `holder` at its hand's angles `rot`: posed as weaponTurn from the
// angles the hand had before its Hand and Weapon Together turn (hands::State::wholeTurn), then turned rigidly by it (the
// weapon's own offsets are Euler angles added to the hand's: turned by them, it would turn a little otherwise).
inline glm::mat3 wholeTurn[2]{glm::mat3{1.f}, glm::mat3{1.f}};
[[nodiscard]] glm::vec3 attachedTurn(const glm::vec3& rot, int slot, bool mirrored, const glm::vec3& handAt0, int holder = -1);
[[nodiscard]] glm::mat4 animationMotion(const view::ViewEntity& ve, const glm::vec3& at);
[[nodiscard]] glm::mat4 partMotion(const glm::mat4& m, float t, const glm::vec3& centre);
void movePose(const glm::mat4& m, glm::vec3& pos, glm::vec3& angles);

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
    bool ssgTurned{false}; // on the open super shotgun's barrels: turned down with them (the hand on it too)
};
inline WorldHotspot worldHotspots[2][weapons::maxHotspots];
inline int chosenGrip[2]{-1, -1}; // per holding hand: the grip hotspot the other hand holds, or last took
inline int chosenGripSlot[2]{-1, -1}; // the weapon (slot) it is of: another weapon chooses afresh (its indices are another's)
inline int magSpot[2]{-1, -1}; // per holding hand: the hotspot index its gun's attached magazine takes (setupWeapon), or -1
constexpr float magHandMargin = 1.5f; // units the helping hand's grip channel stays in from a held magazine's ends
void drawHotspots(int hand, int edited);
void showHotspots();

// The motion review's ghost (view::setGhost): asked for this frame, per hand.
struct GhostRequest
{
    qmodel_t* model{nullptr};
    glm::vec3 pos{0.f};
    glm::vec3 rot{0.f};
    float alpha{0.5f};
    int frame{-1}; // host_framecount it was asked for in
};
inline GhostRequest ghostRequests[2];
void setupGhosts();
// Ghost Hands (vr_ghost_hands): each jointed hand also drawn translucent where its controller really is (before the
// walls, the two-handed grip and the weight's spring move it: hands::State::unresolvedPos/Rot), fading out as the drawn
// hand comes within reach of it; with forearms, a translucent forearm from the drawn elbow to it. drawHand notes the
// pose each hand is drawn at (ghostDrawn); setupGhostHands places the ghosts after both hands.
struct GhostDrawn
{
    glm::vec3 pos{0.f}, rot{0.f};
    bool mirrored{false};
    int frame{-1}; // host_framecount drawn in
};
inline GhostDrawn ghostDrawn[2];
void setupGhostHands(const hands::State& s);
void setupWeapon(hands::State& s, int hand, qmodel_t* model, int frame, bool floating = false);
[[nodiscard]] int damageLevel();

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
    bool inPalm{false};   // a prop held In the Palm (its Grip): the thumb round the outside of it
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
    // A shape of its own instead of `ent`'s model's (a ragdoll's limb: limbHold; `frame` its part), drawn at `toWorld`.
    const grasp::Shape* shape{nullptr};
    glm::mat4 toWorld{1.f};
};
[[nodiscard]] const grasp::Shape* heldShape(const Held& held, int frame);
[[nodiscard]] glm::mat4 heldToWorld(const Held& held);
void setManualFingers(Held& held, bool manual, const float curl[handrig::FingerCount], float thumbAcross);
void weaponFingers(Held& held, int slot);

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
    za::Array<float, handrig::data::numJoints * 12> skin{};
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
    glm::vec3 pushed{0.f};   // drawn held off the other hand's weapon or prop (vr_hand_collide, pushAgainst)
    double pushedTime{-1.0};
    static constexpr int propPoints = 1 + 2 * handrig::FingerCount; // the palm's middle, the knuckles, the fingertips
    const entity_t* propAgainst{nullptr};
    glm::vec3 propDrawn[propPoints]{}; // where each point was drawn last frame, in the prop's frame (its way in)
    bool propDrawnValid[propPoints]{};
    bool propTouching[propPoints]{}; // held out last frame: a fingertip stays tested while it touches (not only stuck)
    // Against the other hand's weapon or prop (pushAgainst): each point's plane last frame (in the weapon's frame), kept
    // while the point is in it deeper than the hand is drawn held out (no way in found from where it was drawn, in it);
    // and whether they touched (a buzz as they meet).
    glm::vec3 propPlaneN[propPoints]{};
    glm::vec3 propPlaneAt[propPoints]{};
    bool propPlaneValid[propPoints]{};
    bool weaponTouching{false};
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
inline RigHand rigHands[2];
[[nodiscard]] glm::vec3 vec3Of(const float* v);
[[nodiscard]] float rigCurl(int hand, int finger);
[[nodiscard]] glm::mat4 toMat4(const float m[16]);
[[nodiscard]] glm::vec3 drawnInRig(const RigHand& rh, const glm::vec3& p);
// Each hand's weapon, or the prop it holds alone, drawn pressed back by the other, free hand against it (pushAgainst;
// NOTES.md vrfiringrange_2026-10-01_12-12-28, 17-15-34): the model collision adds it to that hand next frame
// (view::handPress; the prop moved with it: held::viewPush), as it does a prop's in the other hand (held::drawnPush).
inline glm::vec3 weaponPressed[2]{glm::vec3{0.f}, glm::vec3{0.f}};
[[nodiscard]] grasp::Settings graspSettings(const Held& held, float rigUnit);
glm::mat4 rigPlacement(int hand, const glm::vec3& pos, const glm::vec3& handRot, bool mirrored, view::ViewEntity* ve);
bool emptyHandSpheres(const hands::State& s, int hand, bool fist, za::Vector<glm::vec4>& out);
void updateFist(const hands::State& s, int hand);
void wristInfo_f();
void powerupInfo_f();
void updateGripFrame(const hands::State& s, int hand);

[[nodiscard]] int holsterStat(int h, int first); // (below, with the drawn holsters' order)

// Each drawn hand's index fingertip (view::drawnIndexTip: the gadget's side button), from its tracked place along its
// tracked turn's forward, right and up (units), and the frame it was drawn (host_framecount; -1 never).
struct IndexTip
{
    glm::vec3 local{0.f};
    int frame{-1};
};
inline IndexTip indexTips[2];
[[nodiscard]] glm::vec3 palmAt(int hand, const glm::vec3& pos, const glm::vec3& handRot, bool mirrored);
void moveDrawnHand(int hand, bool mirrored, const glm::vec3& frameRot, glm::vec3 p, glm::vec3 a, bool mirror, glm::vec3& pos,
    glm::vec3& handRot);
[[nodiscard]] glm::vec3 helpingTurn(const glm::vec3& holderRot, int holder, int hand, int otherSlot, bool cup,
    const glm::vec3& spotAngles);
[[nodiscard]] bool groundHotspotZone(const entity_t& e, int slot, const weapons::Hotspot& h, glm::vec3& a, glm::vec3& b);
[[nodiscard]] float surfaceDistance(const entity_t& e, bool mirrored, const glm::vec3& p, float reach);
void updateGroundSpots(const hands::State& s);
void updateFreeSpots(const hands::State& s);

void drawHand(int hand, glm::vec3 pos, glm::vec3 handRot, bool mirrored, bool hide, const Held& held, const glm::mat4& motion);

// ---- A hand holding a ragdoll's limb (vr_ragdoll_grab; ROUND21.md, "Hands on held ragdolls") ----
// The limb is held by a spring at the hand (vr_box3d.cpp): it lags the hand as it moves and sags under the body's
// weight, and the hand, drawn at its controller with its fingers in their fist, came off the limb. Drawn on the limb
// instead, where the hold has it (within vr_ragdoll_hand_stick of the controller), its fingers closed round the limb's
// mesh as round a carried prop's (the grasp: vr_grasp.cpp).

struct LimbHold
{
    int num{0};
    int part{-1};
    glm::vec3 pos{0.f};                 // the hand's place on the limb, as drawn (world)
    glm::quat turn{1.f, 0.f, 0.f, 0.f}; // and its turn (the axes of the controller's angles there)
    const grasp::Shape* shape{nullptr}; // the limb's (its rest pose's triangles)
    glm::mat4 toWorld{1.f};             // drawn there
    bool tug{false};                    // a tug-of-war's hold: the hand on the weapon in the limb's hand (box3d::RagdollHold)
};
[[nodiscard]] bool limbHold(int hand, LimbHold& out);

// A tug-of-war's hold (LimbHold::tug): where the hand took hold of the armed monster's weapon, on the weapon as drawn
// (the hold's point on the server, in the server's weapon's frame, put in the drawn weapon's: the client draws it on the
// ragdoll's drawn hand, vr_hitreact.cpp ragdollProps).
struct TugGrab
{
    int prop{0};                  // the weapon's entity
    glm::vec3 at{0.f};            // the grab, as drawn (world)
    glm::mat3 weaponTurn{1.f};    // the drawn weapon's axes
    float drawnOff{0.f};          // units the drawn weapon is from the server's
};
[[nodiscard]] bool tugGrab(int hand, const LimbHold& limb, TugGrab& out);

// The hand drawn on its limb, eased on as it takes it and off as it lets go (limbEaseTime): the last offset kept.
struct LimbEase
{
    double time{-1.0};
    float blend{0.f};
    glm::vec3 offset{0.f};
    glm::quat turn{1.f, 0.f, 0.f, 0.f};
    float unclamped{0.f}; // degrees the limb has the hand turned (vr_ragdoll_hand_probe)
    // The palm rested on the limb's mesh (vr_ragdoll_hand_rest): the slide (world units, eased), and the palm's gap to
    // the mesh before it and after (units; negative: in it; 999 none near) for the probe.
    glm::vec3 rest{0.f};
    float gapBefore{999.f}, gapAfter{999.f};
    glm::vec3 drawnPos{0.f}; // where the hand was drawn last (its controller's place moved; vr_ragdoll_hand_probe)
};
inline LimbEase limbEase[2];
void setupHand(const hands::State& s, int hand);
void drawHand(int hand, glm::vec3 pos, glm::vec3 handRot, bool mirrored, bool hide, const Held& held, const glm::mat4& motion);
[[nodiscard]] glm::mat3 entityTurn(const entity_t& e);
void placeFloating(const hands::State& s, posing::Session& ps);
void holdFloating(hands::State& s, const posing::Session& ps);
[[nodiscard]] int holsterStat(int h, int first);
[[nodiscard]] glm::vec3 aliasAngles(const glm::vec3& fwd, const glm::vec3& up);

// The body's frame at a holster (turnHolster), made orthonormal: `out` away from the body, `up`, and `outwards`.
struct HolsterFrame
{
    glm::vec3 out, up, side;
};
[[nodiscard]] glm::vec3 inHolsterFrame(const HolsterFrame& f, const glm::vec3& v);

// What each holster drew last (vr_pose_check after posing in a holster): its frame and point, and the weapon in it.
struct HolsterSeen
{
    bool valid{false};
    bool floating{false};
    weapons::HolsterKind kind{weapons::HolsterKind::Hip};
    int slot{-1};
    glm::vec3 pivot{0.f};
    HolsterFrame frame{};
    glm::vec3 weaponPos{0.f};
    glm::mat3 weaponTurn{1.f};
    glm::vec3 loops{0.f}; // the holster itself as drawn (its model's origin, the loops a gun hangs in), empty or not
    int placed{-1};       // host_framecount when placed (-1: never)
};
inline HolsterSeen holsterSeen[HolsterCount];

[[nodiscard]] avatar::HandPose drawnHand(const hands::State& s, int hand);
void gadgetTouches(const hands::State& s, bool posingNow);
void quadArcs(const hands::State& s);
void powerupInfo_f();
void wristInfo_f();
} // namespace view_detail

using namespace ::view_detail;

// Functions of qvr::view that one part defines and another calls.
namespace qvr::view
{
bool ammoPouchCentre(glm::vec3& at);
[[nodiscard]] int ammoPouchItemCount(); // the rounds placed in the ammo pouch last frame (setupAmmoPouchItems)
glm::vec3 anchorPosition(const ViewEntity& ve, const char* anchor, const glm::vec3& extra);
glm::vec3 anchorPosition(const entity_t& e, bool mirrored, float zeroBlend, const char* anchor, const glm::vec3& extra);
const ViewEntity* find(const entity_t* e);
bool modelLoadPort(const qmodel_t* model, float ssgOpen, glm::vec3& at, glm::vec3& axis, glm::vec3& face);
glm::vec3 modelPoint(const ViewEntity& ve, const glm::vec3& point);
bool weaponFrame(const hands::State& s, int hand, WeaponFrame& out);
} // namespace qvr::view

// The engine hooks one part defines and another calls.
void controllerPreviewPose(const hands::State& s, int hand, glm::vec3& c, glm::mat3& b);
