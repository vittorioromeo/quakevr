// vr_view.hpp -- client-side VR view entities: weapons in both hands, hands and fingers,
// holstered weapons, holster slots, torso and weapon buttons. Built every frame from the
// tracked hands and the VR stats, drawn as ordinary alias entities.

#pragma once

#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_pouchitems.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Container/Vector.hpp"

namespace qvr::view
{

// QVR_SVC_HANDIMPACT: the drawn hand is knocked and wobbles back (a parried blow).
void parseHandImpact();

struct ViewEntity
{
    entity_t ent{};
    bool visible{false};
    bool mirrored{false};  // left-hand versions of right-hand models
    float zeroBlend{0.f};  // blend of the animation towards frame 0 (weapons)
    float morph{0.f};      // a gun morphing into its other ammo's model: + coming in, - going out (VR_AliasMorph)
    int poseFrame[2]{-1, -1}; // drawn as these two frames blended, poseBlend of the second (-1: its own frame's blend;
    float poseBlend{0.f};     // the AD crossbow's string and limbs: vr_adcrossbow.cpp, VR_AliasPoseOverride)
    bool lightMultiply{false};
    glm::vec3 lightMod{1.f};
    glm::vec3 lightShift{0.f}; // the light sampled this far from the origin (a climbing hand: vr_climb.cpp drawnHand)
    glm::vec3 scale{1.f};  // the model's scale per axis, exact (entity_t's is a byte, in sixteenths; vr_render.cpp)
    const qmodel_t* lastModel{nullptr};
    int netEntity{0};      // > 0: a part of that world entity (a lying gun's magazine): drawn with its networked scale and
                           // offset (a map's spinning weapon pickup is moved by its model_offset: vr_render.cpp)
};

// Null if `e` is not a VR view entity.
[[nodiscard]] const ViewEntity* find(const entity_t* e);

// The alias shader's skin tint of `e` (vraliasinstance_t's skintint, skintintrange) with bare forearms (vr_body_bracers
// 0): your body (and its copy in a mirror), its forearms' skin turning to the hand's colour towards the wrist; a jointed
// hand (w -1), its end inside the arm folded onto the wrist's line. False for any other entity (none).
[[nodiscard]] bool skinTint(const entity_t* e, float tint[4], float range[4]);
// Skin-only tone: kind 1 the jointed hand, 2 the body, zero other entities.
[[nodiscard]] int skinTone(const entity_t* e, float tone[4]);
// Gloves (vr_gloves, vr_glove_hue/saturation/lightness) on the same entities: `glove` the leather's colour (rgb) and the
// skin mask's channel it covers (w: 1 fingerless, 2 full, 0 none); on a bare-forearmed body, `cuff` the glove's cuff
// over the forearm's end in its skin (x below this u, from v y, its hem to v z, below v w; zero: none). The kind as
// skinTone's, zero when nothing of a glove is drawn on `e`.
[[nodiscard]] int gloveLook(const entity_t* e, float glove[4], float cuff[4]);
// That cuff's place in the bare-forearmed body's skin (as gloveLook's `cuff`; vr_handrig.cpp's cuff shell too).
void gloveCuffRange(float cuff[4]);
// The kind (as skinTone's) when `e` reads its skin mask (a skin tone or a glove on it), else zero.
[[nodiscard]] int skinMaskWanted(const entity_t* e);
// A power-up's glow on `e` (Quad's blue, the pentagram's red, the biosuit's green; vr_view.cpp powerupGlow): on your
// hands and body (and their copies in a mirror) the alias shader's code, -2 - 2 x its colour (r + 16g + 256b, 16 levels
// a channel) - its strength (0..1); else 0.
[[nodiscard]] float powerupGlow(const entity_t* e);

// With bare forearms (vr_body_bracers 0) the jointed hands are drawn in your body's light, so that the skin is lit alike
// on both sides of the wrist: bareLight (r_alias.c's light colour, R_SetupAliasLighting) and bareShade (the instance's
// light direction and the light round it, vraliasinstance_t) keep the body's and give it to the hands.
void bareLight(const entity_t* e, float lightcolor[3]);
void bareShade(const entity_t* e, float lightdir[4], float ambient[6][4]);
// Whether e is the body whose light the bare hands take (bareLitHand): drawn in each eye, never culled.
[[nodiscard]] bool keepsBareLight(const entity_t* e);

// Which of the view entities' parts `e` is (VR_DRAW_HANDS ... VR_DRAW_OTHER, vr_api_render.h); -1 not a view entity.
[[nodiscard]] int drawGroup(const entity_t* e);

// The view entity a copy in the body's mirror image (vr_bodyclone.cpp) is of; null if `e` is not one. (find() gives the
// copy's own ViewEntity: its place, turn and mirroring the image's.)
[[nodiscard]] const entity_t* cloneSource(const entity_t* e);

// World position of an anchor of `ve`'s model (its text: vr_anchor.hpp), with `extra` offsets applied in the
// entity's (mirrored) space before the model's own scaling.
[[nodiscard]] glm::vec3 anchorPosition(const ViewEntity& ve, const char* anchor, const glm::vec3& extra);

// The same for any alias entity (a weapon lying in the world), drawn mirrored or not (a view entity: as above).
[[nodiscard]] glm::vec3 anchorPosition(
    const entity_t& e, bool mirrored, float zeroBlend, const char* anchor, const glm::vec3& extra);

// World position of `ve`'s model's vertex `vertex` (its file's order) as drawn: not an attachment (the melee HUD's embers
// off a random vertex).
[[nodiscard]] glm::vec3 vertexPosition(const ViewEntity& ve, int vertex);

// Immersive reloading: the reference point of the round `hand` holds (a magazine's top, moved by its
// vr_reload_mag_<kind>_x/y/z, and its radius in `radius`; a shell's middle, radius -1), as drawn. False: it holds none.
[[nodiscard]] bool heldRoundRef(int hand, glm::vec3& out, float* radius = nullptr);

// Where `hand` goes for its fingertip to be `units` off the ammo button of the other hand's gun: `angle` degrees off the
// way its face looks (0 in front of it, 90 beside it, 180 behind it), turned `azimuth` degrees round it, as drawn last frame (the mock's "vr_mock_hand_to <hand> wbutton"). False: no
// button shown.
[[nodiscard]] bool weaponButtonHandTarget(int hand, float angle, float azimuth, float units, glm::vec3& out);

// Immersive reloading's super shotgun broken open (phase 2b): how far (degrees) the barrels of the gun in `hand` are
// drawn turned down about its hinge now (0: closed, or not a super shotgun), and a point (or a direction, `point`
// false) of its model (v_shot2.mdl's space) turned with them by `deg` (vr_shells.cpp: its casings out of the chambers).
[[nodiscard]] float ssgOpenAngle(int hand);
[[nodiscard]] glm::vec3 ssgTurned(const glm::vec3& p, float deg, bool point = true);

// A gun model's loading port in its model space (frame 0; +x forward, +y left, +z up), moved by its Load Point offsets:
// its point, the way a round goes in (a unit vector) and its opening's outward way (one), the super shotgun's turned down
// with its barrels `ssgOpen` degrees. False if the model has none. Static data and settings only: the held guns' ports
// (setupWeapon) and the guns lying about (QC loadportof, the server's).
[[nodiscard]] bool modelLoadPort(const qmodel_t* model, float ssgOpen, glm::vec3& at, glm::vec3& axis, glm::vec3& face);

// The gun drawn in `hand` this frame (its model's space: as its frames' vertices; none: false): a point of it in the world,
// a world point in it, and its turn (forward, left, up: its angles', not mirrored). Immersive reloading's shells sliding
// into it (vr_collectfx.cpp).
[[nodiscard]] bool gunToWorld(int hand, const glm::vec3& p, glm::vec3& out);
[[nodiscard]] bool gunFromWorld(int hand, const glm::vec3& w, glm::vec3& out);
[[nodiscard]] bool gunAxes(int hand, glm::mat3& out);
// The way a round goes into the gun drawn in `hand` (its model's space), past its load point: `port` (the load point, as
// the reload takes it: its offsets; the open super shotgun's turned with its barrels), then `deep` and `end`, inside it
// (the shotgun: up through its loading port's well, then forward into the tube; the super shotgun: into its chambers).
// False: no gun there with a load point.
[[nodiscard]] bool loadPath(int hand, glm::vec3& port, glm::vec3& deep, glm::vec3& end);
// The same for any gun's `model`, the super shotgun's barrels turned down `ssgOpen` degrees.
[[nodiscard]] bool modelLoadPath(const qmodel_t* model, float ssgOpen, glm::vec3& port, glm::vec3& deep, glm::vec3& end);
// The gun lying about as client entity `num` (a weapon prop: QC's thrown_weapon), as it is drawn this frame, in `out`
// (for modelPoint), and how far its barrels are drawn down (a super shotgun lying open: setupWorldSsgs); false if it is
// not there or not a gun with a load point. Shells loaded into it sliding in (vr_collectfx.cpp).
[[nodiscard]] bool propGun(int num, ViewEntity& out, float& ssgOpen);

// World position of a point given in `ve`'s model space (as its frames' vertices).
[[nodiscard]] glm::vec3 modelPoint(const ViewEntity& ve, const glm::vec3& point);

// modelPoint's transform made once, for several points of the same entity as it is now (the same results, without making
// its matrix again for each): ModelSpace m{ve}; m.point(p).
class ModelSpace
{
public:
    explicit ModelSpace(const ViewEntity& ve);
    [[nodiscard]] glm::vec3 point(const glm::vec3& p) const;

private:
    float m_[16]{};
    glm::vec3 origin_;           // (not an alias model: its origin for every point)
    glm::vec3 scaleOrigin_{0.f}; // the model's header's
    glm::vec3 scale_{1.f};
    bool alias_ = false;
};

// QVR flashlight on guns (round 20): the gun drawn in `hand` this frame, for what clips onto it: its
// model, the pose it is drawn from (the hand's; for a gun carried by its foregrip, the hand's that let
// it go) and its muzzle. False when the hand holds no gun (empty, or nothing drawn).
struct WeaponMount
{
    const qmodel_t* model{nullptr};
    glm::vec3 pos{0.f};
    glm::vec3 rot{0.f};
    glm::vec3 muzzle{0.f};
    bool mirrored{false};
    int slot{-1}; // its per-weapon settings' (weapons::slotForModel)
};
[[nodiscard]] bool weaponMount(int hand, WeaponMount& out);

// Body Calibration (vr_bodycal.cpp): the empty hand as drawn on the calibrated controller (Hand Calibration's place,
// with the fist's own offsets), whatever the hand holds: its wrist (world) and axes (towards the fingers, the thumb's
// side, the back of the hand). Without the jointed hand, the hand as drawn (jointed false).
struct EmptyHand
{
    glm::vec3 wrist{0.f};
    glm::vec3 forward{1.f, 0.f, 0.f};
    glm::vec3 up{0.f, 0.f, 1.f};
    glm::vec3 back{0.f, 1.f, 0.f};
    bool jointed{false};
};
[[nodiscard]] bool emptyHandPose(const hands::State& s, int hand, EmptyHand& out);

// Align Sights to My Aim (vr_sightalign.cpp): the weapon drawn in `hand` this frame: its model, the world transform of
// its model space (as its frames' vertices: view::modelPoint), and the middle of the fist round its grip (the drawn
// hand's grip channel; its palm without the jointed hand). False when the hand holds no weapon.
struct WeaponFrame
{
    const qmodel_t* model{nullptr};
    glm::mat4 modelToWorld{1.f};
    glm::vec3 fist{0.f};
    bool fistFromRig{false};
};
[[nodiscard]] bool weaponFrame(const hands::State& s, int hand, WeaponFrame& out);

// The counter glow (vr_meleehud.cpp): the weapon drawn in `hand` this frame (null: none, or the hand's own model), and
// which hand an entity belongs to (-1 none): its weapon (or the model a gun morphs out of; `weapon` true) or a part of
// the hand itself.
[[nodiscard]] const ViewEntity* heldWeapon(int hand);

// The cells the ammo pouch shows this frame (the lightning gun's, standing in it: 0 when it gives none, or none are left;
// up to vr_ammo_pouch_mags_max), and cell `index`'s copper contact on its top as drawn, wobbling (world), and its up (the
// cell cords, vr_cellcord.cpp). False: no pouch drawn.
[[nodiscard]] int ammoPouchCells();
// The leather bag's centre, using the pouch's rendered pose (turn, crouch and size included).
[[nodiscard]] bool ammoPouchCentre(glm::vec3& at);
[[nodiscard]] bool ammoPouchCell(int index, glm::vec3& at, glm::vec3& up);
// Where the rounds stand in the ammo pouch as placed last frame (drawn or not: vr_ammo_pouch_show 0 still places it), a
// box (its middle, then its half-axes: out of the body, to the left, up along the rounds; hands::State::magBox's form):
// from the pouch's rim up to the rounds' tops, across its width (pouch reloading: QC vr_reload.qc VR_Reload_PouchFrame).
// False: no pouch (not immersive reloading, dead).
[[nodiscard]] bool ammoPouchRounds(glm::vec3 (&box)[4]);
// The magazines the ammo pouch shows (a nailgun's, the super nailgun's, cells: STAT_QVR_POUCHKIND 2..4), the first on its
// right (vr_pouchitems.cpp magSlot: a row, then two): how many, and each one's top middle (world) and their up.
// ammoPouchMags: as drawn this frame (each its own model, wobbling); ammoPouchMagSlots: at rest, through the rounds' box
// (ammoPouchRounds) as QC
// vr_reload.qc VR_Reload_PouchMagTop finds them (pouch reloading's magazine guns; vr_reload_pouch_mag_*). 0: none shown.
[[nodiscard]] int ammoPouchMags(glm::vec3 (&top)[pouchitems::magsMost], glm::vec3& up);
[[nodiscard]] int ammoPouchMagSlots(glm::vec3 (&top)[pouchitems::magsMost], glm::vec3& up);
[[nodiscard]] int handOf(const entity_t* e, bool& weapon);
// The weapon drawn in holster `stat` (body::Holster: its STAT_QVR_HOLSTERWEAPON* slot) last frame, the one it holds
// (null: empty, or a stand-in: the Weapon Offsets preview, a posing session): its blood (vr_wounds.cpp).
[[nodiscard]] entity_t* holsteredWeapon(int stat);
// The gun drawn in holster `index` (0 .. body::HolsterCount - 1, in no particular order) as placed this frame (null:
// empty, a stand-in, not drawn): a fuelled gun's hose to the back tank (vr_adtank.cpp).
[[nodiscard]] const ViewEntity* holsterGun(int index);
// The weapon flags of the gun in holster `index` (as holsterGun's; QC's .holsterweaponflags*).
[[nodiscard]] int holsterFlags(int index);
// Where holster `stat` (body::Holster) itself was drawn last frame, empty or not: its model's origin, the loops a gun
// hangs in (on the body, not the hand's reach point body::holsterPosition, which stands out of it). False: not placed.
[[nodiscard]] bool holsterPoint(int stat, glm::vec3& at);
// A gun drawn as parts in its place: the super shotgun broken open (its frame and barrels, their skin the gun's with rows
// added under it for the breech plates), the shotgun while its auto pump strokes (the gun without its fore-end and the
// fore-end, the gun's very skin). The parts drawn this frame in place of the gun entity `gun` (a hand's, a holster's, a
// lying prop's; 0: none, it is drawn itself), and the gun a part stands for (null: not a part; the model a hand's gun
// morphs out of stands for it too). Its blood (vr_wounds.cpp) stays on it through them: one gun, one blood.
[[nodiscard]] int gunPartsOf(const entity_t* gun, entity_t* out[2]);
[[nodiscard]] const entity_t* gunPartSource(const entity_t* part);
// Whether models `a` and `b` are the same gun (one is the other's other ammo's: its button switched it).
[[nodiscard]] bool sameGun(const qmodel_t* a, const qmodel_t* b);

// How curled a drawn finger is (0 open .. 1 curled): `finger` 0 thumb, 1 index, 2 middle, 3 ring, 4 pinky.
[[nodiscard]] float fingerCurl(int hand, int finger);

// The weapons' hotspots (round 21): the hotspot point (the weapon's model space, weapons::Hotspot) of a world point
// `p` on the weapon in `hand`; false when the hand holds none.
[[nodiscard]] bool hotspotAt(int hand, const glm::vec3& p, glm::vec3& out);

// Hotspot `index` of the weapon in `hand` as drawn this frame: its type (weapons::HotspotType, 0 none), where (a grip's
// point, a blade grip's middle), its bias, and a blade's share of the way from the hand to the tip.
struct WeaponHotspot
{
    int type{0};
    glm::vec3 pos{0.f};
    float bias{0.f};
    float share{0.f};
};
[[nodiscard]] WeaponHotspot weaponHotspot(int hand, int index);

// Hotspot `index` of the weapon lying about as client entity `entity` (vr_weapon_grab_hotspots: taken by it): a grip's
// point, a blade's zone's middle. False for none or a cup (tests: vr_mock_hand_to <hand> spot).
[[nodiscard]] bool groundHotspotPoint(int entity, int index, glm::vec3& out);

// A weapon carried off its handle by `hand` (vr_twohand.cpp: the hand-off, a hotspot, anywhere), as drawn last: the pose
// it is drawn from (`pos`: its handle's place; `rot`: its entity's angles, the pitch negated, as QC gives a thrown weapon
// (MakeThrown negates it back); `mirrored`: as the off hand holds it) and its tip (the muzzle's anchor: the melee's line, QC's throw from where it is). False if it carries none.
[[nodiscard]] bool carriedWeaponPose(int hand, glm::vec3& pos, glm::vec3& rot, bool& mirrored, glm::vec3& tip);

// The same weapon's handle and tip in the frame of the hand that carries it as it was drawn (its position and `rot`:
// forward, right, up), for the server to place on the hand of the move it runs (QC's melee: VR_Melee_FreeLine). False if
// it carries none.
[[nodiscard]] bool carriedWeaponInHand(int hand, glm::vec3& handle, glm::vec3& tip);

// A point in the weapon `hand` holds or carries, as drawn last: `fraction` of the way from its handle to its tip (the
// middle of its drawn points within a unit and a half of the way there), `cm` over it (tests: vr_mock_hand_to <hand>
// held). False if it has none.
[[nodiscard]] bool heldWeaponPoint(int hand, float fraction, float cm, glm::vec3& out);

// The weapon `hand` holds, as drawn last: its handle's place and its muzzle's anchor (the holster zones: its grip and
// body). False if it holds none (or a fist).
[[nodiscard]] bool heldWeaponLine(int hand, glm::vec3& handle, glm::vec3& tip);

// The middle of the palm of `hand` as drawn last holding a weapon (the holster zones' hand point: the hand can be in
// a holster's box while its gun's grip and body are not). False if it holds none.
[[nodiscard]] bool heldHandPoint(int hand, glm::vec3& out);

// The butt of the weapon `hand` holds, as drawn last: the middle of its drawn points within a unit of its rearmost end
// along its line from the muzzle to the handle (a stock's heel, a pistol grip's base). The wrist gadget's screen tap
// (vr_bullettime_tap_butt). False if it holds none (or a fist).
[[nodiscard]] bool heldWeaponButt(int hand, glm::vec3& out);
// Its butt as a region: its drawn points within `depth` units of that rearmost end along the same line (world; a point
// may come more than once: the shape's triangles share them). The screen tap's striking volume. False as above.
[[nodiscard]] bool heldWeaponButtRegion(int hand, float depth, za::Vector<glm::vec3>& out);
// The prop `hand` holds (held::heldEntity: a box, a gib, a health pack) as drawn: its shape's points (world; a point may
// come more than once). The screen tap's striking volume. False if it holds none (or it has no shape).
[[nodiscard]] bool heldPropSurface(int hand, za::Vector<glm::vec3>& out);

// The parts of a drawn hand (drawnHandSurface): what struck the gadget's screen, in the tap's messages.
enum class HandPart : unsigned char
{
    Hand,       // the old models' hand (three spheres along it)
    Palm,
    Back,       // the back of the hand
    Knuckles,
    Fingers,
    Fingertips,
    Thumb
};
[[nodiscard]] const char* handPartName(HandPart part);

// The hand `hand` as drawn this frame, as its surface (world; w a radius): the jointed hand's mesh vertices, posed as
// drawn (w 0), or the old models' three spheres along the hand; with each point's part (`parts`, if given). The wrist
// gadget's screen tap (its striking volume). False when it isn't drawn.
[[nodiscard]] bool drawnHandSurface(int hand, za::Vector<glm::vec4>& out, za::Vector<HandPart>* parts);

// The weapon in `hand` as drawn last (the local player's): its model, whether mirrored (the off hand's), and its
// entity's place and turn relative to the hand's pose (hands::State pos and rot: held::axesFromAngles' forward, left,
// up) -- rigid; the model's vertices go in it as the view draws them (mirrored, then weapons::ModelTransform). For the
// held weapons' bodies in Box3D (vr_box3d.cpp: they push props). `when`: vr_gametime it was drawn; model nullptr: none.
// `mag`: the magazine drawn in it (immersive reloading, its magazine in; made in the gun's model space, vertices placed as
// the gun's: held::magazineVertices), nullptr none; `magBox` its box (hands::State::magBox's middle and half-axes) in the
// hand's frame, as `inHand` is. Installed magazines are solid (vr_reload_mag_collide): the gun's body in Box3D, the
// other hand and a prop in it (vr_view.cpp pushAgainst, vr_held.cpp meetFrame), the other weapon (vr_selfcollide.cpp).
struct DrawnWeapon
{
    const qmodel_t* model{nullptr};
    bool mirrored{false};
    glm::mat4 inHand{1.f};
    double when{-1.0};
    const qmodel_t* mag{nullptr};
    glm::vec3 magBox[4]{};
};
[[nodiscard]] const DrawnWeapon& drawnWeapon(int hand);

// The jointed hand's index fingertip as `hand` was drawn last (its pose then: pointing, curled, holding), from its
// tracked place (hands::State::pos) along its tracked turn's forward, right and up (units): the gadget's side button
// (vr_gearlights.cpp). False without the jointed hand drawn in the last two frames.
[[nodiscard]] bool drawnIndexTip(int hand, glm::vec3& local);
// The magazine model attached to the magazine gun `gun` (vr_mag_on_<gun>.mdl, made in its model space), nullptr for
// another gun (the server's guns lying about: their bodies in Box3D).
[[nodiscard]] qmodel_t* magazineModelOf(const qmodel_t* gun);

// The weapon in `hand` drawn pressed back by the other, free hand pushed into it (vr_hand_collide: each drawn moved back
// by half how deep they meet, as a prop in that hand and the weapon do), last frame's: the model collision moves the
// weapon hand by it (vr_modelcollide.cpp, with held::drawnPush).
[[nodiscard]] glm::vec3 handPress(int hand);

// The free `hand` drawn held out of the prop `ent` (an entity number) the other hand holds alone, last frame's
// (vr_hand_collide_props_keep_out): how far it is drawn moved off its tracked place, relative to the prop as drawn (the
// hand's push less the prop's press; world units). Zero when it is not against it. The second hand's grab of that prop
// (vr_carry2h.cpp) takes it there, at the surface, not where the real hand is in it.
[[nodiscard]] glm::vec3 heldOutOf(int hand, int ent);


// vr_hotspots_check: the migrated hotspots against the old two-handed grips, every slot, either hand.
void hotspotsCheck_f();
void anchorWhere_f(); // vr_anchor_where

// vr_weapon_hotspot_here <1..4> [<type>] [main|off]: hotspot n of the weapon in the main hand (or the named one) put
// where the other hand is now, as a grip (1) or the type given (3: a cup) -- the Weapon Offsets page's "Put It Where the
// Other Hand Is", for scripts; "vr_weapon_hotspot_here <n> 0" removes it.
void hotspotHere_f();

// vr_hotspot_fit: the weapon in the main hand (any: a sword, the crowbar, a gun): where the hand and the drawn fist are
// on its model (model units: its generator's frame), its tip's distance, and where the off hand must move to take each
// hotspot (a grip: its point; a blade: its zone's ends and middle), in the mock's vr_mock_hand metres (right, up, back of
// the view's yaw) from the off hand as it is. For tests without a headset (Debug > Tests > Hotspot Fit).
void hotspotFit_f();

// Wounds painted on models (vr_wounds.cpp): the player's own drawn this frame, that take its wounds: the body, and
// each jointed hand (null where not drawn).
void woundTargets(entity_t* out[3]);

// The jointed hand (vr_handrig.cpp): the skinning matrices of `e` if it is a drawn hand rig (their count, else 0).
[[nodiscard]] int handBonePoses(const entity_t* e, const float** matrices);

void dumpView_f();

// The view's debug and test commands (vr_view_diag.cpp: vr_dumpview, vr_grasp_*, vr_ragdoll_hand_probe,
// vr_model_reload, vr_hotspots_check, vr_anchor_where, vr_weapon_hotspot_here, vr_hotspot_fit), registered (VR_Init).
void registerDiagCommands();

// vr_pose_check (vr_posing.cpp): the hand holding the weapon now (`weaponTarget`: the weapon hand; else the other hand,
// holding it by a hotspot) against the pose confirmed: its rig and drawn palm, and the weapon's muzzle, where the pose put
// them relative to each other (`rigInWeapon`, `palmInWeapon`: in the weapon's model frame; no palm if it wasn't seen
// solved while posing).
void posingCheck(bool weaponTarget, int weaponHand, const glm::mat4& rigInWeapon, const glm::vec3* palmInWeapon,
    const glm::mat4& rigWorld);
// vr_pose_check after posing a weapon in a holster: each holster of that kind holding that weapon now, against the pose
// set (the weapon's place, forward and up in the holster's frame, from its point: posing::Candidate::inHolster*).
void holsterPoseCheck(weapons::HolsterKind kind, const qmodel_t* model, const glm::vec3& inPos, const glm::vec3& inFwd,
    const glm::vec3& inUp);

// A new map (VR_OnClientClearState): the per-hand states timed by the client's time or eased frame to frame start
// afresh (a parried blow's knock, the weapons' button hover and morph, the drawn hands' grasp and curls).
void resetClientState();
void graspDump_f();

// Mod_ForName(name, false) for a model asked for every frame, `name` a string constant (its address is the key): kept
// while it is loaded; a missing one remembered until the next map.
[[nodiscard]] qmodel_t* viewModel(const char* name);

// A map's load (VR_NewMap), in VR: the view's own models loaded (the hands' rig, the body and its pauldrons, the leg
// holsters, the wrist gadget), not in the first frame drawn (0.3 s the first time: the body's 0.2).
void prepareModels();

// Forgets every cache keyed by a model (the view models, clip sizes, the jointed hand's check, the grasp shapes): for
// a game directory change, which reuses the models' slots.
void resetCaches();

// vr_model_reload [model ...]: models edited in Blender (docs/vr-port/MODELS_IN_BLENDER.md) read again from their files,
// and what the engine worked out from them forgotten (the anchors' bindings, the grasp's shapes, the collision
// triangles, the body's bones). Without arguments: the weapons (progs/v_*.mdl), the body (progs/vrbody*.mdl: its
// .md5mesh and skins) and the wrist gadget (progs/vrgadget*.mdl) that are loaded.
void modelReload_f();

// The motion review's ghost (vr_motion_review.cpp): a recorded take's weapon (or empty hand) in `hand`, drawn
// translucent and tinted this frame where the game draws a weapon held at the hand pose `pos`, `rot` (hands::State's
// pos and rot, as a take records them): the weapon's own angle offsets and model transform, mirrored in the off hand.
// Asked for every frame it is shown (from VR_BeginFrame); `model` null or not asked for: not drawn.
void setGhost(int hand, qmodel_t* model, const glm::vec3& pos, const glm::vec3& rot, float alpha);

// vr_grasp_bench [n]: solves each hand's grasp of what it holds n times (1000), and prints the times (min, median,
// max, microseconds).
void graspBench_f();
// vr_ragdoll_hand_probe: each hand holding a ragdoll's limb: how far the limb is from the hand (the hold's lag), the hand
// drawn off its controller onto it, the palm and fingertips from the limb's mesh (cm) and how many fingers met it.
void ragdollHandProbe_f();
void graspSweep_f();

// Hand/Gun Calibration > Match Controller Preview: how far (world) the empty hand drawn on `hand`'s calibrated controller
// must move for the middle of its fist (grasp::gripChannel's point: the middle of the circles its fingers close round)
// to be on the Show Controller preview's point (the middle of the handle, where OpenXR puts the fist). False without
// the jointed hand, or before it has been drawn.
[[nodiscard]] bool previewGripMove(const hands::State& s, int hand, glm::vec3& worldMove);

} // namespace qvr::view
