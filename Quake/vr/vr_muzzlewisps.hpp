// vr_muzzlewisps.hpp -- smoke wisps off your guns' barrels after you stop firing (ROUND23.md, "Persistent muzzle
// wisps"): once a gun has not fired for a moment (vr_gunfx_wisp_delay), each of its barrels (the shotgun's one, the
// super shotgun's and the nailgun's two; the thunderbolt's one, faint and blue-grey) trails a very thin, faint, soft line of smoke for a few seconds
// (vr_gunfx_wisp_time), following the muzzle as the gun moves: the smoke left behind rises and curls in a slow
// turbulent flow (a divergence-free field: it curls, never bunches up), widening and fading over vr_gunfx_wisp_fade.
// Each barrel's trail is a chain of points let go at the muzzle (a ring of them: nothing allocated), drawn as one soft
// ribbon (vr_ribbons.hpp), all of them in one draw. Your guns only; settings: Weapons > Weapon Effects > Gunfire
// Particles (vr_gunfx_wisp*). vr_debug_gunfx 1 prints "gunfx wisps start ..." as a gun's start (and how long after its
// last shot) and "gunfx wisps faded ..." once they are gone.

#pragma once

#include <glm/glm.hpp>

namespace qvr::muzzlewisps
{

constexpr int maxBarrels = 2;

// Your hand's gun this frame (gunfx::handFrame): its barrels' muzzles, the way they point, and how its shots charge them
// (a shot's share of full smoke, 0 none: the gun leaves no wisps), how thick the smoke is (its opacity at full) and its
// tint (times its grey: the thunderbolt's blue-grey).
struct Gun
{
    glm::vec3 at[maxBarrels]{};
    int barrels{0};
    glm::vec3 fwd{1.f, 0.f, 0.f};
    float charge{0.f};
    float alpha{0.f};
    glm::vec3 tint{1.f};
};

// `hand`'s gun as drawn this frame (`fired`: it fired this frame). Not called for a frame: no gun in that hand (its
// wisps already let go keep fading).
void handFrame(int hand, const Gun& gun, bool fired);

// Whether `hand`'s barrels still hold smoke to let go (its gun is wanted each frame till then).
[[nodiscard]] bool smoking(int hand);

// A gun that left a hand still smoking (thrown, dropped, holstered): its wisps go on for their time, following it on the
// prop lying about or in its holster (vr_gunfx.cpp finds it). maxLoose such guns at once (the least smoke given up).
constexpr int maxLoose = 4;
// `hand`'s smoke handed to a loose gun (its trails and their time go with it; the hand starts afresh): its index, -1 if
// the hand held none.
[[nodiscard]] int release(int hand);
// Loose gun `index` as drawn this frame (its barrels where it is now). Not called for a frame: it is not drawn (its
// wisps let go keep fading, none let go).
void looseFrame(int index, const Gun& gun);
// Whether loose gun `index` still holds smoke to let go.
[[nodiscard]] bool looseSmoking(int index);

// The scene's translucent pass (vr_particles.cpp): moved on once a frame, drawn in every view.
void drawTranslucent();

// Everything forgotten (a new map, a disconnect).
void clear();

} // namespace qvr::muzzlewisps
