// vr_flatprocreload.hpp -- flatscreen play's procedural reload (flatprocreload; his words 2026-11-28: "adding actual
// procedural reloading animations to flatscreen play, gated behind an option, on by default"): the shotgun held in one
// hand, the other empty, reloaded as a headset reloads it, the free hand driven between the ammo pouch and the gun's
// loading port (vr_flatprocreload.cpp; the game's side is QC vr_flatreload.qc's procedural reload, which takes and loads
// the rounds by VR's own paths in vr_reload.qc; docs/vr-port/FLATPLAY.md, "Reloading").

#pragma once

#include "vr_hands.hpp"

#include <glm/glm.hpp>

namespace qvr::flatprocreload
{

// vr_flatprocreload_report (Debug > Reports > Flatscreen Play; the tests).
void init();
// Once a frame, from flatplay::frame (`on`: the mode runs, the fly camera isn't flying): the server's bits read (the
// procedural reload of a hand's gun up or down), the free hand's trip stepped, the pouch's take sent (impulse 99) as the
// hand gets there, a punch's or the failsafe's cancel sent (impulse 100).
void frame(bool on);
// After flatplay's pose and the force grab's (flatplay::poseHands): the gun tilted to present its port, the free hand moved
// and turned along its trip.
void apply(hands::State& s);
// Whether hand `h` is the free hand of a trip (going, at the pouch, carrying, coming back) or of the key-pose lock: it lets go
// of a two-handed grip (vr_twohand.cpp, vr_flatsight.cpp), the gun keeps its stance, and neither hand pushes the other
// (vr_selfcollide.cpp, vr_view_hands.cpp pushOut): its way is drawn through the gun.
[[nodiscard]] bool busy(int h);
// The loading port of the gun in hand `g` the move sends while a trip reloads it (vr_client.cpp): from the gun hand of `s`
// (position and turn) and the port as measured in that hand's frame, the very place the free hand aims the round at (so
// the server's test, round against port, is the hands' relative placement: the player's motion, the view's sway, a turn
// since the last render don't come between them). False: no trip on that gun (the port carried as for every gun).
// How much hand `h`'s fingers take the grip a headset's hand has on a round from the pouch (0..1, eased; vr_view.cpp
// updateFingerFrames: the grip squeezed, the index closing with it, wrapped round the round by vr_hand_fit as a headset's):
// the free hand of a trip while it holds the round (the take to the insert), or of the key-pose lock with its pair.
[[nodiscard]] float gripShare(int h);
// The view, each hand as it is drawn (vr_view_hands.cpp setupHand, before drawHand): noteDrawn keeps the gun hand's game
// pose `s` and its drawn pose; attachDrawn moves the free hand of a trip, from the carry to the insert (eased in and out),
// onto the gun as drawn: the game's free hand relative to the game's gun hand, put on the drawn gun hand (the drawn gun
// sways off the game's on the move; the pair, aimed in the game's frame, then goes into the drawn port too). Drawn only.
void noteDrawn(const hands::State& s, int hand, const glm::vec3& pos, const glm::vec3& rot);
void attachDrawn(const hands::State& s, int hand, glm::vec3& pos, glm::vec3& rot);
[[nodiscard]] bool portNow(const hands::State& s, int g, glm::vec3& port);
// reloadstudio2 (his note 2026-11-28 item 10): the reload's free hand `h` on its way back (a reload's-end key to its own pose,
// the last insert done; the lock on such a key too): the gun's two-handed grip may take it now (vr_flatsight.cpp gripWanted),
// so that key's own pose is the grip and the hand goes there straight, not to the body's side first. One-handed guns: no grip,
// as before.
[[nodiscard]] bool freesForGrip(int h);
// The gun in hand `h` posed by a procedural reload (a trip's or the lock's): flat play's open pose of a broken-open super
// shotgun or a swung-out revolver (vr_flatreload.cpp openFrame) eases out for it, the keys' tilt alone, as the studio shows.
[[nodiscard]] bool posesGun(int h);
// The lock's gun in hand `h` drawn open (reloadstudio3): the super shotgun broken open, the revolver's cylinder swung out,
// on the keys a real reload has it open (from an Open key played to a Close key; or open already), as the game draws it.
[[nodiscard]] bool lockOpens(int h);
// The weapon flags (STAT_QVR_WEAPONFLAGS) the game would have for the lock's gun in `h` on the locked key: broken open (the
// super shotgun), its magazine out (an Eject key played, no Insert since: the magazine drawn out, the magazine's grip gone,
// so the gun is held as in a real reload); `flags` as they are for any other gun.
[[nodiscard]] int lockWeaponFlags(int h, int flags);
// The revolver's state bits (STAT_QVR_REVOLVER) the same: swung out, its spent shells out (flat play's swing-out).
[[nodiscard]] int lockRevolverBits(int h, int bits);
// The key-pose lock (vr_flat_prl_lock, prlfix: to tune the poses) holding a taped pair in the free hand: that hand and the
// pair's size (vr_view.cpp prlPair draws it); false: no pair to draw.
[[nodiscard]] bool lockPair(int& hand, float& scale);
// Its skin (reloadmuzzle: the revolver's slugs, 1).
[[nodiscard]] int lockItemSkin();
// Where that pair is for the hand drawn at `handPos` turned `handRot` (hands::State angles), and its model's axes (forward,
// left, up): as it sat in the hand at the last real take (a default before one).
[[nodiscard]] glm::vec3 lockPairPlace(const glm::vec3& handPos, const glm::vec3& handRot, glm::mat3& axes);
// The model drawn for it: the taped pair, a magazine gun's magazine (reloadmags; QC VR_Reload_MagModel's), a launcher's round
// or the revolver's slugs (reloadmuzzle).
[[nodiscard]] const char* lockItemModel();
// Whether flat-screen play shows the ammo pouch (the server says the procedural reload is on: STAT_QVR_FLATRELOAD's 64),
// vr_body.cpp ammoPouchEnabled.
[[nodiscard]] bool pouchShown();
// Flat-screen play's own nudge of the pouch (vr_flat_ammo_pouch_x/y/z in the body's frame of yaw `bodyYaw`) while it is
// shown there; zero otherwise (vr_body.cpp ammoPouchPosition).
[[nodiscard]] glm::vec3 pouchOffset(float bodyYaw);
// And its turn (vr_flat_ammo_pouch_pitch/yaw/roll) and its size (vr_flat_ammo_pouch_scale in place of the headset's
// `headsetScale`), vr_view.cpp setupAmmoPouch.
[[nodiscard]] glm::vec3 pouchTurn();
[[nodiscard]] float pouchScale(float headsetScale);

// The Reload Studio's readouts (vr_reloadstudio.cpp): the lock (its key from 0, its share; the gun it would hold and its
// weapon ID), the trip (its gun's weapon, its key, share), the item's gap to its destination now and as the last insert's
// key reached its end (the success readout), and the counts.
struct Status
{
    bool locked{false};
    int lockKey{-1};
    float lockShare{1.f};
    int lockGun{-1};
    int lockWeapon{0};
    bool tripOn{false};
    int tripWeapon{0};
    int tripKey{-1};
    float tripShare{0.f};
    float portGap{-1.f};
    float insertGap{-1.f};
    int trips{0}, seated{0}, dropped{0}, cancels{0}, failsafes{0};
    const char* lastCancel{"-"};
};
[[nodiscard]] Status status();
// The locked key's free hand as drawn (its place and turn) and the frame its offsets are in (a place and its axes: forward,
// left, up; the gun hand's for the destination's keys, the body's otherwise) and the key's target (reloadsys::Target): the
// studio's gizmo. False: not locked.
bool lockFrames(glm::vec3& framePos, glm::mat3& frameAxes, glm::vec3& handPos, glm::vec3& handRot, int& kind);
// reloadstudio2: the locked key's gun hand as drawn (its place and turn), the body's frame its moves are in (forward, right;
// up +z) and its side (1: the right hand's; -1 the left's: its turns' yaw and roll and its "in" mirrored). False: not locked.
bool lockGunFrame(glm::vec3& handPos, glm::vec3& handRot, glm::vec3& bodyFwd, glm::vec3& bodyRight, float& side);

// reloadstudio2 (his note item 11): the free hand and the item in it as drawn this frame -- the lock's or a real reload's --
// in the drawn gun hand's frame (forward, left, up; turns as the angles of the relative axes): the hand placed (before the
// grasp), the hand entity drawn (after the grasp's palm move), the item, the item in the placed hand's own frame, the grasp's
// palm move (rig units) and turn (degrees), what it wraps (0 nothing, 1 a held thing, 2 a weapon), the five fingers' curls.
// vr_flatprocreload_pose prints it; the studio's pose test compares the lock's with a held real reload's (vr_flat_prl_hold).
struct PoseSample
{
    bool lock{false};
    int key{-1};
    float t{0.f};
    glm::vec3 handPos{0.f}, handRot{0.f}, drawnPos{0.f}, drawnRot{0.f};
    bool item{false};
    glm::vec3 itemPos{0.f}, itemRot{0.f}, inHandPos{0.f}, inHandRot{0.f};
    float palmMove{0.f}, palmTurn{0.f};
    int wraps{0};
    float curl[5]{};
    const char* itemModel{""}; // (the item's model's name)
    int itemFrame{0};
    glm::vec3 gunAt{0.f}, gunRot{0.f}, freeAt{0.f}; // (the gun hand and the free hand off the player's origin; the gun hand's angles)
};
bool poseSample(PoseSample& out);

} // namespace qvr::flatprocreload
