// vr_view.hpp -- client-side VR view entities: weapons in both hands, hands and fingers,
// holstered weapons, holster slots, torso and weapon buttons. Built every frame from the
// tracked hands and the VR stats, drawn as ordinary alias entities.

#pragma once

#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_move.hpp"
#include "vr_pouchitems.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

namespace qvr::avatar
{
struct HandPose;
struct Subject;
}

namespace qvr::handrig
{
struct Pose;
struct Posed;
} // namespace qvr::handrig

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

// The steps of a gun drawn in its parts (vr_view.cpp; your own hands' and holsters' guns, and another player's: his
// avatar's GunParts): a magazine sliding home or out, the super shotgun's barrels dropping open, the heavy revolver's
// crane and cylinder.
struct MagSlide
{
    enum class Mode
    {
        None,
        Seat,
        Out
    };
    Mode mode{Mode::None};
    double start{0.0};            // vr_gametime it began
    double time{-1.0};            // vr_gametime of the last step
    float dt{0.f};                // its game time
    int frame{-1};                // host_framecount of the last step
    int uid{0};                   // the gun's id, last step
    const qmodel_t* gun{nullptr}; // its model
    bool noMag{false};            // its magazine out, last step
    int rounds{2};                // what its magazine held as it last sat in the gun (drawn sliding out: its top nails)
};
struct SsgDrawn
{
    float angle{0.f};             // degrees the barrels are drawn turned down
    double time{-1.0};            // vr_gametime of its last step
    const qmodel_t* gun{nullptr}; // the model it was for (another gun: no animation)
    bool parts{false};            // drawn in its parts this frame (the gun itself not drawn)
};
struct RevolverDrawn
{
    float swing{0.f};
    float cyl{0.f};
    double time{-1.0};            // vr_gametime of its last step
    const qmodel_t* gun{nullptr}; // the model it was for (another gun: no animation)
    bool parts{false};            // drawn in its parts this frame (the gun itself not drawn)
};
// Another player's gun drawn in its parts as yours are (MP_PLAN.md, "Held weapons' state"; vr_mpavatar.cpp keeps one
// for each of his hands and holsters): the entities and their steps.
struct GunParts
{
    enum Part
    {
        SsgFrame,
        SsgBarrels,
        RvFrame,
        RvCrane,
        RvCyl,
        RvShell, // (6)
        MagIn = RvShell + 6,
        MagOut,
        Well,
        Button,
        Front,
        PumpBody, // (mpopen) a held shotgun's auto pump stroking: the gun without its fore-end, and the fore-end slid back
        Pump,
        Count
    };
    ViewEntity part[Count];
    SsgDrawn ssg;
    RevolverDrawn rv;
    MagSlide mag;
    bool inParts{false}; // drawn in its parts this frame: the gun itself hidden
};
// Your own gun's look in `hand` (HAND_OFF, HAND_MAIN) from the stats: what the server sends another player's as.
[[nodiscard]] VrGunLook ownGunLook(int hand);
// The gun `gun` (placed, its model and frame) as `look` has it, in its parts `g` (the same routines as yours):
// `held` in a hand (the revolver in its parts, a magazine sliding; else a holster's: broken open, its magazine);
// `screen`: its button and ammo screen too (its clip). The gun is hidden while drawn in its parts. `player` (1..16, with
// `side` 0 off, 1 main: another player's held gun, syncfix5): its screen casts your screen's light too; (mpopen) his
// shotgun's auto pump strokes as yours (autopump::otherTravel, posePump).
void drawGunParts(ViewEntity& gun, const VrGunLook& look, GunParts& g, bool held, bool screen, int player = 0, int side = 0);

// Null if `e` is not a VR view entity.
[[nodiscard]] const ViewEntity* find(const entity_t* e);

// Whether `ve`'s model is drawn this frame: visible, with a model, and not hidden for a clean shot (vr_shot_hide). What
// is drawn from a model besides it (a torch's lit lens: vr_flashlight.cpp) follows this, so it goes when the model does.
[[nodiscard]] bool drawn(const ViewEntity& ve);

// The alias shader's skin tint of `e` (vraliasinstance_t's skintint, skintintrange) with bare forearms (vr_body_bracers
// 0): your body (and its copy in a mirror), its forearms' skin turning to the hand's colour towards the wrist; a jointed
// hand (w -1), its end inside the arm folded onto the wrist's line. False for any other entity (none).
[[nodiscard]] bool skinTint(const entity_t* e, float tint[4], float range[4]);
// A player's look on his skin and hands (MP_PLAN.md, B6: cosmetics others see): your own from your settings (ownLook), another
// player's from his avatar calibration (mpavatar::lookOf), so his hands and body are drawn as he dressed them.
struct SkinLook
{
    float tone{0.f};      // vr_skin_tone: -1 dark .. 1 light
    int gloves{0};        // vr_gloves: 0 none, 1 fingerless, 2 full
    glm::vec3 glove{0.f}; // vr_glove_hue (degrees), _saturation, _lightness
    bool nails{false};    // vr_nails
    glm::vec3 nail{0.f};  // vr_nail_hue, _saturation, _lightness
    float nailGloss{0.f}; // vr_nail_gloss
    bool bare{false};     // bare forearms (no bracers): a glove's cuff over the body's forearm
};
[[nodiscard]] SkinLook ownLook();
// `e`'s look and kind: 1 a jointed hand, 2 a body (yours, and another player's), zero other entities.
[[nodiscard]] int lookOf(const entity_t* e, SkinLook& look);
// Skin-only tone: kind 1 the jointed hand, 2 the body, zero other entities.
[[nodiscard]] int skinTone(const entity_t* e, float tone[4]);
// Gloves (vr_gloves, vr_glove_hue/saturation/lightness) on the same entities: `glove` the leather's colour (rgb) and the
// skin mask's channel it covers (w: 1 fingerless, 2 full, 0 none); on a bare-forearmed body, `cuff` the glove's cuff
// over the forearm's end in its skin (x below this u, from v y, its hem to v z, below v w; zero: none). The kind as
// skinTone's, zero when nothing of a glove is drawn on `e`.
[[nodiscard]] int gloveLook(const entity_t* e, float glove[4], float cuff[4]);
// That cuff's place in the bare-forearmed body's skin (as gloveLook's `cuff`; vr_handrig.cpp's cuff shell too).
void gloveCuffRange(float cuff[4]);
// Painted nails (vr_nails, vr_nail_hue/saturation/lightness/gloss) on a jointed hand (skinTone's kind 1): `nails` the
// polish's colour (rgb) and 1 + its gloss (w; 0 none: no nails on `e`). True when drawn.
[[nodiscard]] bool nailLook(const entity_t* e, float nails[4]);
// vr_mp_avatars' "mp_look:" lines (the harness's mp_cosmetics): `e`'s kind, tone, glove and nails as drawn; your own
// hands' and body's as "mp_look_own:" (parts 0 off hand, 1 main hand, 2 body).
void printLook(const char* what, int ent, int part, const entity_t* e);
void printOwnLooks(int ent);
// The kind (as skinTone's) when `e` reads its skin mask (a skin tone, a glove or painted nails on it), else zero.
[[nodiscard]] int skinMaskWanted(const entity_t* e);
// A power-up's glow on `e` (Quad's blue, the pentagram's red, the biosuit's green; vr_view.cpp powerupGlow): on your
// hands and body (and their copies in a mirror) the alias shader's code, -2 - 2 x its colour (r + 16g + 256b, 16 levels
// a channel) - its strength (0..1); else 0.
[[nodiscard]] float powerupGlow(const entity_t* e);
// Your worn gear as drawn this frame, for the others (syncfix: the move's VrDrawnGear).
void netGear(VrDrawnGear& out);
// The rounds in an ammo pouch `pouch` (placed this frame or not) holding `kind` (STAT_QVR_POUCHKIND's), `count` left:
// yours (setupAmmoPouchItems) and another player's in his copy's pouch (syncfix2), one code -> how many placed in `out`.
int posePouchItems(const ViewEntity& pouch, bool placed, int kind, int count, ViewEntity (&out)[pouchitems::most]);
// A hand's model (the jointed hand's, the old hand's: also the fists' weapon model, an empty holster's).
[[nodiscard]] bool handModel(const qmodel_t* model);
// VrDrawnGear::model's model (1 the sheath, 2 the gadget, 3 its strap, 4 the grenade pouch, 5 the ammo pouch), or nullptr.
[[nodiscard]] qmodel_t* gearModel(int index);
// A holster's sheath and gun on another player's body (a bot's: no client drew them; syncfix), posed as yours are
// (holsterOnBody, holsterOnBack): holster `holster` (body::Holster) at `pos`, the body's surface facing `out` there with
// `surfaceUp` up along it, `pos` standing `clearance` out of it (body::HolsterPlate's; the shoulders' unused), the body
// turned `yaw`, `weapon` in it (its own Holstered pose, as yours). Returns the sheath's model (none for the shoulders, or with vr_leg_holster_model_enabled 0).
qmodel_t* holsterOnOther(int holster, const glm::vec3& pos, const glm::vec3& out, const glm::vec3& surfaceUp, float clearance,
    float yaw, const qmodel_t* weapon, struct GearDraw& sheath, struct GearDraw& gun);

// A player's power-ups as drawn on him (syncfix: yours from cl.items, another player's copy from his avatar's gear byte,
// one code for both): the glow taking turns (its shader code and strength), its light from the hands, the quad's arcs,
// the ring's fade (vr_body_powerups: none at all when off).
struct PowerupLook
{
    float level{0.f};   // the glow's strength this frame (0 none), pulsing
    float code{0.f};    // its colour coded for the shader (r + 16g + 256b, 16 levels a channel)
    int shown{0};       // how many power-ups glow (taking turns)
    int kind{-1};       // the one glowing now (or fading out)
    float light{0.f};   // the hands' light's strength (0 none) and colour
    glm::vec3 lightColor{0.f};
    bool arcs{false};   // Quad's arcs on the hands and forearms (vr_quad_look not 1)
    float alpha{1.f};   // the ring's fade on his body, hands and worn gear (1 none)
};
// The look of power-ups `items` (IT_QUAD, IT_INVULNERABILITY, IT_SUIT, IT_INVISIBILITY bits) at cl.time.
[[nodiscard]] PowerupLook powerupLookOf(int items);
// The shader's glow code of `look` on a body (`body`) or a hand (as powerupGlow's).
[[nodiscard]] float powerupGlowCode(const PowerupLook& look, bool body);
// Quad's arcs on another player's hands: each wrist ([0] off, [1] main), the way his hands point; once a frame.
void otherQuadArcs(int player, const glm::vec3 wrist[2], const glm::vec3 along[2]);

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
// A point of its barrels (v_shot2.mdl's space) as their own part is drawn open this frame (setupSsgParts); false: not
// drawn open (vr_gunfx.cpp's check of its muzzle wisps).
[[nodiscard]] bool ssgBarrelsDrawn(int hand, const glm::vec3& p, glm::vec3& out);
// Something clipped on the super shotgun in `hand` (the flashlight: his words 2026-11-12, it seemed to go through the
// opened barrels), placed on the gun as drawn closed, `on` its world point that decides which part it is on: if the gun
// is drawn broken open and `on` is on the barrels (in front of the hinge), the hinge's world point and the barrels' turn
// in the world (the same as setupSsgParts draws them by: ssgWorldTurn), to carry it down with them about the hinge
// (pos' = hinge + turn (pos - hinge), its rotation turned by `turn`). False: shut, not a super shotgun, or on the frame.
[[nodiscard]] bool ssgBarrelsCarry(int hand, const glm::vec3& on, glm::vec3& hinge, glm::mat3& turn);
// The heavy revolver in `hand` (QC vr_revolver.qc): how far (degrees, 0..95) its crane is drawn swung out now (0: home,
// or not a revolver), and a point of its model (its Body's frame) on the crane turned with it (vr_shells.cpp: its spent
// shells out of the chambers). Its load point (the cylinder's back face's middle) turns with it (modelLoadPort's
// `ssgOpen` for the revolver).
[[nodiscard]] float revolverSwing(int hand);
[[nodiscard]] glm::vec3 revolverTurned(const glm::vec3& p, float swing);
// Chamber `chamber` (0..5) of the heavy revolver in `hand` as drawn now (setupRevolverParts: its crane's swing, its
// cylinder's own turn): the point on its axis `back` units along the bore from the cylinder's back face (its model's
// space). The way from the bore's axis out to the cylinder's, the crane swung `swing` degrees (a unit vector, across the
// bore: the side the cylinder hangs out on).
[[nodiscard]] glm::vec3 revolverChamber(int hand, int chamber, float back);
[[nodiscard]] glm::vec3 revolverOutward(float swing);

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
// The heavy revolver swung out in `hand`: loadPath's way moved off the cylinder's axis to chamber `chamber` (0..5) as
// drawn now (its crane's swing, its cylinder's turn), into that chamber's hole (a shell loaded there: vr_collectfx.cpp).
// False: no revolver there, shut, or no such chamber.
[[nodiscard]] bool revolverChamberPath(int hand, int chamber, glm::vec3& port, glm::vec3& deep, glm::vec3& end);
// The shell drawn in chamber `chamber` of the heavy revolver in `hand` this frame (setupRevolverParts): its origin in the
// world, and the bore's way (a unit vector, the world's). False: none drawn there.
[[nodiscard]] bool revolverShellDrawn(int hand, int chamber, glm::vec3& at, glm::vec3& bore);
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
    int hand{-1}; // the hand it is in (ssgBarrelsCarry: the super shotgun broken open)
};
[[nodiscard]] bool weaponMount(int hand, WeaponMount& out);

// The muzzle of the weapon drawn in `hand` this frame with Muzzle X/Y/Z `offset` instead of its own (the blade shape
// editor's tip handle: how the offset moves it, vr_bladeedit.cpp). False when the hand holds no weapon.
[[nodiscard]] bool muzzleWithOffset(int hand, const glm::vec3& offset, glm::vec3& out);

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
// The hand whose held gun `e` is drawn as: the gun, its morph, or a part drawn in its place (the auto pump's two pieces,
// the open super shotgun's frame and barrels, the heavy revolver's frame, crane and cylinder); else -1.
[[nodiscard]] int gunPartHand(const entity_t* e);
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

// The second grip of a weapon model in its own frame (as weaponLocalGeometry's), where an armed humanoid's off hand holds
// it: its first Grip hotspot's point (type 1); a weapon whose first is a Cup, its Two-Handed Hand Anchor if that is a
// handle well ahead of the main one (vr_agarms_cup_handle: the Widowmaker's front handle; type 1), else 2 (a cup under
// and round the hand on the handle, no point); 0 none (false).
bool secondGrip(qmodel_t* model, glm::vec3& second, int& type);

// A weapon model's points in its own frame (as a weapon lying about or a monster's held one is drawn off its origin and
// angles; x forward, y left, z up, units): its muzzle (a blade's tip), its butt (its rearmost along the handle-muzzle
// line), its second grip (secondGrip's: secondType 1 a point; 2 a Cup's, no point; 0 none). False: no weapon's model.
// Armed humanoids hold weapons as you do from these (QC vr_armedgrunt_pose.qc, weapondrawnpose 8..10).
bool weaponLocalGeometry(qmodel_t* model, glm::vec3& muzzle, glm::vec3& butt, glm::vec3& second, int& secondType);

// A weapon carried off its handle by `hand` (vr_twohand.cpp: the hand-off, a hotspot, anywhere), as drawn last: the pose
// it is drawn from (`pos`: its handle's place; `rot`: its entity's angles, the pitch negated, as QC gives a thrown weapon
// (MakeThrown negates it back); `mirrored`: as the off hand holds it) and its tip (the muzzle's anchor: the melee's line, QC's throw from where it is). False if it carries none.
[[nodiscard]] bool carriedWeaponPose(int hand, glm::vec3& pos, glm::vec3& rot, bool& mirrored, glm::vec3& tip);

// The reverse grip (ROUND25.md, "revgrip"; QC weapondrawnpose 43): how the local player's `hand` takes the loose weapon drawn as
// `e` (its model, origin, angles) now: -1 it is no weapon that may be held reversed (Weapon Offsets' Reverse Grip, vr_revgrip;
// none in flat-screen play), 0 upright (its blade to the thumb's side), 1 reversed (to the little finger's); `cosine`: of the
// blade's way and the thumb's side's (the normal grip's blade, at the hand's turn now).
[[nodiscard]] int reverseGripChoice(const entity_t& e, int hand, float& cosine);
// The same for a hand at angles `handRot` (world) that may hold a weapon reversed or not (`mayReverse`: his vr_revgrip on, not
// in flat-screen play): another player's on a server (MP_PLAN.md, B3f: his move's hand, his own vr_revgrip).
[[nodiscard]] int reverseGripChoiceAt(const entity_t& e, int hand, const glm::vec3& handRot, bool mayReverse, float& cosine);
// Whether the weapon the local player's `hand` holds may be held reversed (its eject button flips the grip: QC weapondrawnpose 46).
[[nodiscard]] bool heldMayReverse(int hand);
// The same in flat play too: the predicate of the eject button's flip (heldMayReverse is its VR half) and of the flat
// holster menu's FLIP GRIP button (weapondrawnpose 46's z).
[[nodiscard]] bool heldGripFlippable(int hand);
// The flip grip's gate (ROUND25.md "flipgate"; vr_revgrip_2h_gate): the weapon in `hand` may be flipped but its other hand holds
// it away from its handle (anywhere, or a sword's blade grip): the flip is refused (weapondrawnpose 46's z: -1).
[[nodiscard]] bool gripFlipBlocked(int hand);
// The weapon in `hand` as drawn last, in the hand's frame (forward, left, up): its axes (forward, up) and its share of the
// reverse grip now (gripFlip). False: none drawn (tests: QC weapondrawnpose 44, 45).
[[nodiscard]] bool heldWeaponAxes(int hand, glm::vec3& forward, glm::vec3& up, float& flip);
// The flip grip (ROUND25.md, "flipgrip"): one pose for a blade held either way, through every way out of and into a hand.
// The reverse grip's turn in `hand`'s frame for its weapon of `slot` as drawn now (the identity upright or carried): the two-handed
// hold's blade line (vr_twohand.cpp bladeDirection) turns with it.
[[nodiscard]] glm::mat3 heldReverseTurn(int slot, int hand);
// Where `hand`'s shots go from its aim `rot`: turned by its weapon's Shot Pitch and Yaw; a pole weapon held reversed (the
// Crystal Lance's grip flipped; lance3, his note vrtesthall_2026-11-23_23-23-39: it shot backwards) the way it is drawn,
// that aim turned by heldReverseTurn too (as vr_twohand's blade line): along its crystal head. The move's shots, your beam
// as drawn, vr_status.
[[nodiscard]] glm::vec3 heldShotAngles(const glm::vec3& rot, int hand);
// The two-handed hold by the grip below the hand (ROUND25.md "flipgrip2"): held reversed and mirrored (vr_revgrip_2h_mirror), the
// other hand's grip is not where the half turn takes it but mirrored along the handle; the turn Q in `hand`'s frame that takes the
// upright hold's line to it (the least turn after the half turn): the hold worked out for the hand turned by Q holds the weapon by
// the mirrored grip. heldReverseTurn otherwise.
[[nodiscard]] glm::mat3 twoHandTurn(int slot, int hand);
// How the local player's `hand` takes the weapon its other hand carries off its handle now (QC weapondrawnpose 47, the hand-off
// taken back: VRTakeCarriedWeapon): as reverseGripChoice, by the carried weapon as drawn.
[[nodiscard]] int carriedGripChoice(int hand, float& cosine);
// Where the point of a hand taking the loose weapon `e` (a model of its own, lying at its origin and angles) reversed is (its
// handle the other way round: the reverse grip's hand point); false: none (a weapon that may not be held reversed). The grab's
// "at its handle" (vr_physics.cpp, updateGroundSpots) takes the nearer of it and the origin (QC weapondrawnpose 48: tests).
[[nodiscard]] bool looseReversedHandPoint(const entity_t& e, glm::vec3& point);
// The same for the weapon `hand` carries off its handle as drawn now (twohand's HS_CARRIED_GRIP: taken back either way).
[[nodiscard]] bool carriedReversedHandPoint(int hand, glm::vec3& point);
// A weapon of `slot` leaving a player's hand (`mirrored`: his off hand) held with `flip` of the reverse grip (leavingFlip: as
// his view drew it; a drop, a throw: QC weapondrawnpose 49, 50): the angles (a view's, as the hand's `handRot`) that the weapon
// lying about keeps the way it is drawn by (the reverse grip's turn baked in: rot Rev), and the move of its origin from the
// hand's point (reverseShift). `handRot` and nothing for a weapon held upright.
[[nodiscard]] glm::vec3 leavingTurn(int slot, bool mirrored, float flip, const glm::vec3& handRot, glm::vec3& shift);
// The reverse grip's share the weapon in the local player's `hand` is drawn with (0 upright; a carried one's: 0).
[[nodiscard]] float leavingFlip(int hand);

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

// The two-handed hotspot of `hand`'s weapon a hand free to go anywhere goes to (Weapon Offsets' own, as placed this frame): a
// grip it aims by if there is one, else a cup; the one furthest along the gun. Its index, its place in `at`; -1: none.
// (A tracked hand takes the one nearest it instead: chooseGrip.)
[[nodiscard]] int twoHandSpot(const hands::State& s, int hand, glm::vec3& at);

// The grip hotspot the other hand takes on `hand`'s weapon (chooseGrip, as of the last frame drawn), and its type
// (weapons::HotspotType as an int); -1: none.
[[nodiscard]] int chosenGripSpot(int hand, int& type);

// Where `hand` was drawn this or last frame, and how turned; false: it wasn't.
[[nodiscard]] bool drawnHandPose(int hand, glm::vec3& pos, glm::vec3& rot);

// A hand going onto a blade stuck in a body (embedDrawnHand, easing in): how far it has still to go to be on it, as last drawn
// (world units; 0 on it or off it). The avatar's embedArmPastCm measures the arm from where it will be (embedhold2).
[[nodiscard]] glm::vec3 embedPinLead(int hand);
// The blade it is on as that frame drew it (its origin; the client draws entities a frame or more behind the server).
[[nodiscard]] glm::vec3 embedPinSeen(int hand);

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
// Where `hand`'s weapon `model` was placed last (its origin, its entity's angles). False: another model, none, not the local
// player's view (a server without one).
[[nodiscard]] bool drawnWeaponPlace(int hand, const qmodel_t* model, glm::vec3& origin, glm::vec3& angles);

// The jointed hand's index fingertip as `hand` was drawn last (its pose then: pointing, curled, holding), from its
// tracked place (hands::State::pos) along its tracked turn's forward, right and up (units): the gadget's side button
// (vr_gearlights.cpp). False without the jointed hand drawn in the last two frames.
[[nodiscard]] bool drawnIndexTip(int hand, glm::vec3& local);
// (ropes12) The jointed hand `hand` as drawn: finger `finger`'s (handrig::Finger) joints, the knuckle first, and its tip's
// end (world), and the middle of its palm's side (`palm`). False when it isn't drawn.
[[nodiscard]] bool drawnFingerPoints(int hand, int finger, glm::vec3 out[4], glm::vec3& palm);
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

// Multiplayer (MP_PLAN.md, B2b): the hands and held weapons as drawn last ([0] off, [1] main), for the move (the other
// players draw them: vr_mpavatar.cpp). A weapon whose model the server has not precached is left out (others can't name it).
void netHands(VrDrawnHand out[2]);
// Another player's hands placed by the code that places yours (setupWeapon, setupHand; MP_PLAN.md, B11 part 3): a bot's,
// which no client draws, from its controllers (`pos`, `rot`, world; [0] off, [1] main) and the weapons in them
// (`model`: model_precache indices, 0 an empty hand; `frame`), written as netHands writes yours (world). His own
// drawing state (his hands' fit eased from frame to frame) is kept per player entity `player`, swapped in for it.
// `placed` (may be null): each hand's weapon where the server placed it (its avatar's weaponOrigin, weaponAngles with
// weaponBit: placeHeld's, bots3), drawn there and the hand fitted to it.
void solveHands(int player, const glm::vec3 pos[2], const glm::vec3 rot[2], const int model[2], const int frame[2],
    VrDrawnHand out[2], const VrDrawnHand* placed = nullptr);
// The weapon `model` in another player's hand `hand` (0 off: drawn mirrored, 1 main) at `pos`, `rot` (world; the hands'
// angles), placed as setupWeapon places another's (the weapon's angle and model offsets, no reverse grip, no carry): its
// entity's origin and angles (the pitch as entities have it), its muzzle and its hotspots (world; a blade hotspot's middle,
// as weaponHotspot gives it). Pure math on the model and the Weapon Offsets: the server runs it for a bot (bots3: its
// disarm volume, its muzzle, its avatar's weapon place, one placement drawn by every client). False: no weapon (an empty
// hand, a fist, a model with no Weapon Offsets slot).
struct HeldPlace
{
    int slot{-1};
    bool mirrored{false};
    glm::vec3 origin{0.f}, angles{0.f};
    glm::vec3 muzzle{0.f};
    int spotType[4]{};
    glm::vec3 spot[4]{};
    float spotBias[4]{}, spotShare[4]{};
};
[[nodiscard]] bool placeHeld(qmodel_t* model, int hand, const glm::vec3& pos, const glm::vec3& rot, HeldPlace& out);
// ... and the holsters as drawn last (B6b): their points and the weapons in them, in body::Holster's order (their stats').
void netHolsters(VrDrawnHolsters& out);

// The pauldrons' look (vr_body_pauldron*): yours from your settings, another player's from his calibration and the armour
// the server says he wears (MP_PLAN.md, B6b).
struct PauldronLook
{
    bool on{false};
    int style{0};          // vr_body_pauldron_style
    float size{1.f};       // vr_body_pauldron_size
    float follow{0.f};     // vr_body_pauldron_follow
    glm::vec3 offset{0.f}; // vr_body_pauldron_forward, _out, _up
    int build{1};          // his body's build
    int armour{0};         // the armour worn: 0 none, 1 green, 2 yellow, 3 red
    bool plates{false};    // the armour's plates drawn on the body (a plate on a shoulder: no pauldron there)
    unsigned platePieces{0};
};
// The pauldrons of `body` (posed this frame: yours, `player` 0, or player `player`'s, avatar::otherShoulder) placed into
// `cap` and `arm` ([0] left, [1] right; not visible: none). One code path for yours and the others'.
void placePauldrons(const ViewEntity& body, const PauldronLook& look, int player, ViewEntity cap[2], ViewEntity arm[2]);
// vr_mp_avatars: "<what>: ent= side= skin= scale= at=" for each cap drawn; yours as "mp_pauldron_own".
void printPauldrons(const char* what, int ent, const ViewEntity cap[2]);
void printOwnPauldrons(int ent);
// One player's jointed hands, held weapons and holstered weapons as drawn (MP_PLAN.md, B11c part 2): yours and every other
// player's copy through the same routines on per-entity data. drawHand: a jointed hand, its fingers posed, skinned and the
// rig placed; drawGear: a weapon in a hand or a holster, placed with its frame and skin. Yours at the end of the live rig's
// solve (setupRigHand, setupWeapon, setupHolsters): what is the own view's alone (the grasp and the palm's fit, a gun's
// kick, shake and turns, the holsters' places on your body) stays there and comes in as the place, the pose and the skin.
// Another's through drawHands, from his snapshot (his hands, weapons and holsters as his client drew them: VrDrawnHand,
// VrDrawnHolsters, from his entity as drawn here).
struct HandDraw
{
    bool shown{false};        // false: its entity hidden (posed all the same)
    glm::vec3 origin{0.f};    // the rig entity's place and angles (entity_t's)
    glm::vec3 angles{0.f};
    bool mirrored{false};     // the off hand: the right hand's rig mirrored
    int skin{0};              // its skin (yours: a wound's blood level)
    handrig::Pose* pose{nullptr};   // its fingers' shifts, curls and thumb
    handrig::Posed* posed{nullptr}; // out: posed
    float* bones{nullptr};          // out: the skinning matrices (handrig::data::numJoints * 12 floats)
};
// The rig's model drawn (nullptr: no jointed hand; `ve` hidden then).
qmodel_t* drawHand(const HandDraw& h, ViewEntity& ve);
struct GearDraw
{
    qmodel_t* model{nullptr}; // nullptr: none (its entity hidden)
    glm::vec3 origin{0.f};
    glm::vec3 angles{0.f};    // entity_t's
    int frame{0};
    int skin{0};
    bool mirrored{false};     // a right-handed model drawn left-handed
};
void drawGear(const GearDraw& g, ViewEntity& ve);
// Another player's whole set from his snapshot: `hands` ([0] off, [1] main) and `holsters`, their places from `origin`
// (his entity as drawn); his fingers shaped by your rig's settings (rigShifts).
struct HandsDraw
{
    glm::vec3 origin{0.f};
    const VrDrawnHand* hands{nullptr};
    const VrDrawnHolsters* holsters{nullptr};
};
struct HandsEntities
{
    ViewEntity* hand{nullptr};   // [2]
    ViewEntity* weapon{nullptr}; // [2]
    ViewEntity* holster{nullptr}; // [VrDrawnHolsters::count]
    float* bones[2]{nullptr, nullptr}; // each hand's skinning matrices (out)
};
void drawHands(const HandsDraw& in, const HandsEntities& out);
// Another player's drawn jointed hand (its entity, placed by vr_mpavatar.cpp): its wrist and orientation as your own drawn
// hands' are taken for your body's arms (his body's arms reach it).
void remoteHandPose(const ViewEntity& hand, avatar::HandPose& out);
// The body model of a build (vr_body_build's; `bare`: its bare forearms', vr_body_bracers 0), falling back as your own body
// does (the athletic build's): usable as the VR body, or nullptr.
[[nodiscard]] qmodel_t* bodyModel(int build, bool bare);
// The armour's plates on your body (vr_player_plates; vr_view_body.cpp drawBody): `e`'s bones if it is one of their
// entities (0: not), the body's as this pass draws them, a hidden piece's collapsed, the helmet only where the head is
// drawn (View: as VR_AliasBonePoses has the body; Shadow: as the shadow maps have it; Clone: its mirror image, with its head).
enum class PlatePass : int
{
    View,
    Shadow,
    Clone,
};
[[nodiscard]] int plateBonePoses(const entity_t* e, const float** matrices, PlatePass pass);
// One player's body as drawn (MP_PLAN.md, B11c): yours (vr_view_body.cpp, from the live rig) and every other player's
// copy (vr_mpavatar.cpp, from his snapshots) through drawBody, on per-entity data. Its model (the build, bare forearms or
// bracers, falling back as bodyModel), posed (avatar::pose; another's avatar::poseOther: his body's own motion), placed,
// shaded, the armour's plates and their joints' leather on it, the pauldrons. What is the own view's alone comes in as
// data: the legs (your vr_body_mode 3), the drawn view's ride (flat play's bob), the shading (your armour, wounds and
// power-ups: showPlayerState), the plates' morphs (yours come in and go out over vr_player_plates_morph_time).
struct BodyPlates
{
    static constexpr int maxMorphs = 4;
    bool on{false}; // plates drawn at all (vr_player_plates: yours and how you see the others')
    int type{0};    // the armour (1-3; 0 none) and its pieces worn (the server's word)
    int pieces{0};
    int coming{0};  // those still coming in (drawn by a morph, not settled)
    int morphPieces[maxMorphs]{}; // each morph's pieces (0 none), armour and the alias shader's morph value
    int morphType[maxMorphs]{};
    float morphValue[maxMorphs]{};
};
// One player's plates' morphs (BodyPlates' coming and morph fields) as the server's word on him changes, over your
// vr_player_plates_morph_time: a piece gained comes in, another armour's pieces go out where the new one's come in, a piece
// lost goes at once (its debris flies from QC). Yours (vr_view_body.cpp) and each other player's copy (vr_mpavatar.cpp),
// a state each; `resync` (a new level, a load, his copy first drawn): the next word taken as it comes, for a second.
struct PlateMorphs
{
    struct Morph
    {
        int pieces{0}, type{0}; // (0 pieces: a free one)
        double start{0.0};
        bool in{true};
    };
    int type{0}, pieces{0}; // the server's word as of the last frame
    Morph morphs[BodyPlates::maxMorphs];
    bool resync{true};
    double settleUntil{0.0};
    int begun{0}; // morphs begun so far (the tests)
};
void morphPlates(PlateMorphs& m, bool on, int type, int pieces, double now, BodyPlates& out);
struct BodyDraw
{
    bool shown{false};                      // false: none drawn (the entities below hidden)
    int player{0};                          // his entity or a doll slot (poseOther's key); yours: cl.viewentity
    const avatar::Subject* who{nullptr};    // his measures and motion; nullptr: this game's own player (the live ones)
    hands::State s;                         // the head and hands it is posed from
    const avatar::HandPose* poses{nullptr}; // the drawn hands' wrists, [HAND_OFF] and [HAND_MAIN]
    int build{1};
    bool bare{false};    // its bare forearms wanted (both its jointed hands drawn)
    bool legs{true};     // with legs (seen from outside: always)
    glm::vec3 ride{0.f}; // the whole body moved with the drawn view (flatplay::bodyRide)
    void (*shade)(ViewEntity& ve, const hands::State& s){nullptr}; // its skin, tint, see-through (nullptr: plain)
    BodyPlates plates;
    PauldronLook pauldrons; // (its plates' fields: drawBody's)
};
struct BodyEntities
{
    ViewEntity* body{nullptr};
    ViewEntity* plates{nullptr}; // [0] the settled pieces, then one per morph (numPlates in all)
    int numPlates{0};
    int* platePieces{nullptr};   // out: each plates entity's pieces drawn (0 none), or nullptr
    ViewEntity* bridge{nullptr}; // the joints' leather
    ViewEntity* cap{nullptr};    // the pauldrons, [0] left, [1] right
    ViewEntity* arm{nullptr};
};
// The body model drawn (nullptr: none; everything in `out` hidden then).
qmodel_t* drawBody(const BodyDraw& b, const BodyEntities& out);
// Another player's plates' bones (VR_AliasBonePoses' count; with `matrices`, made into `out`: agarmor::playerPlatesMostBones, or for the
// leather (`bridge`) playerBridgeMostBones, times 12 floats).
[[nodiscard]] int otherPlateBonePoses(const entity_t& body, bool bridge, int pieces, float* out, const float** matrices);
// vr_mp_avatars: "mp_plates_own: ent= type= pieces= skin= model=" (your plates as drawn).
void printOwnPlates(int ent);
// vr_mp_avatars: a player's jointed hands, held and holstered weapons as drawn, a line each ("<what>: ent= kind=hand|weapon|
// holster index= model= mirrored= frame= skin= at= ang= bones=": the hand's skinning matrices summed); yours as "mp_hands_own".
void printHands(const char* what, int ent, const ViewEntity hand[2], const float* const skin[2], const ViewEntity weapon[2],
    const ViewEntity* holster, int numHolsters);
void printOwnHands(int ent);
// A new level or a load's armour coming (agarmor::parseEvent): the next word on the plates taken as it comes, no morph.
void platesResync();
// Whether the armour's plates show on your body this frame (vr_player_plates on, a piece worn, the body drawn): what the
// ammo pouch and the belt torch ask to move forward off them (vr_ammo_pouch_armor_x, vr_flashlight_armor_forward).
[[nodiscard]] bool playerArmourShown();
// vr_player_plates_info: what's drawn this frame.
void platesInfo_f();
// vr_player_plates_model_check [end]: the plates drawn as the drawn body's own armour model (`ppmodel: ok|FAIL ...`); `end`
// the tally (`ppmodel: N passed, M failed`). Misc/quakevr/plates_model_test.sh.
void platesModelCheck_f();
// The tests (ragdollbone's QVR_ARMOR_CLIENT, `what`): 10 the settled pieces drawn, 11 those coming in, 12 those going out, 13
// the newest morph coming in's progress (per mille; 0 none), 14 morphs begun so far, 15 the pieces drawn from the eyes last
// frame (a view without the head), 16 those drawn with the head (a view away from the eyes: a gate's, a spectator's), 17 the
// plates' slots (0: no shell), 18 the settled ones' colour, 19 those drawn in the body's mirror image last frame (vr_bodyclone).
[[nodiscard]] int platesState(int what);

void dumpView_f();

// The view's debug and test commands (vr_view_diag.cpp: vr_dumpview, vr_grasp_*, vr_ragdoll_hand_probe,
// vr_model_reload, vr_anchor_where, vr_weapon_hotspot_here, vr_hotspot_fit), registered (VR_Init).
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

// A map's load (VR_NewMap), VR or flat: the view's own models loaded (the hands' rig, the body, its
// armour shells and pauldrons, the leg holsters, the wrist gadget), not in the first frame drawn (0.3 s the first time).
void prepareModels();

// A map's load (VR_NewMap's prewarm): the precached guns' button and screen anchors bound, as the
// first gun lying near you would (~10 ms in that frame: ROUND22.md "Frame spikes").
void prepareWorldWeapons();

// Forgets every cache keyed by a model (the view models, clip sizes, the jointed hand's check, the grasp shapes): for
// a game directory change, which reuses the models' slots.
void resetCaches();

// vr_model_reload [model ...]: models edited in Blender (docs/vr-port/MODELS_IN_BLENDER.md) read again from their files,
// and what the engine worked out from them forgotten (the anchors' bindings, the grasp's shapes, the collision
// triangles, the body's bones). Without arguments: the weapons (progs/v_*.mdl), the body (progs/vrbody*.mdl: its
// .md5mesh and skins) and the wrist gadget (progs/vrgadget*.mdl) that are loaded.
void modelReload_f();
// The same for these models (progs/<name>.mdl, as the engine knows them; empty: the editable ones): vr_model_reload's
// work and lines, also vr_model_watch's (vr_modelwatch.cpp).
void reloadModels(const za::Vector<za::String>& names);

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
