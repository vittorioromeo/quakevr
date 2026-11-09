#pragma once

// vr_drips.hpp -- drops dripping off things: water (and slime) off what was in it once it is out, and blood off wounded
// living monsters (vr_drips, vr_gore_wound_drips; ROUND24.md, "Water drips"). One drip source a thing, of one liquid,
// its drops the gore's (gore::dropFrom: a trace down, the falling particle, its landing later) in the liquid's colour:
// blood lands as the gore's drops do (specks, a mark now and then, a drip heard); water and slime land as `landed` here
// (a few tiny specks, a soft rate-limited drip, no decal), into a liquid on the way a ripple.
//
// Wetness, 1 soaked .. 0 dry, for every alias model the client has (monsters, corpses, gibs, props, dropped weapons;
// looked at ten times a second: its box's bottom in water or slime) and for your own body, hands, held and holstered
// weapons (each frame): 1 while in the liquid, the waterline it came up to kept (a thing half in drips under it). Out of
// it, it dries in vr_wounds_wet_time (faster moving: vr_drips_dry_motion) and drips vr_drips_rate * wetness^vr_drips_curve
// drops a second from points under the waterline: a model's downward-facing triangles there (wounds::dripPoint), a
// gun's barrel towards its muzzle, a hand, your body's lower part (none close under the eyes; vr_drips_view of the
// rate). Swinging a wet weapon or hand fast throws drops off it (vr_drips_fling). Lava dries at once; nothing drips in
// or under a liquid.
//
// A wounded monster (vr_gore_wound_drips; nothing with vr_gore 0) drips blood from where it was hit (its last four
// wounds, bound to its model as drawn: bloodstream::anchorPoint, a ragdoll's too) all the while its health is under
// vr_drips_blood_health of full: vr_drips_blood_rate times (vr_drips_blood_min + the rest times the share lost), more for
// a couple of seconds after a hit (vr_drips_blood_hit); out of sight it waits (10 s, then forgotten); its corpse drips on
// for vr_drips_blood_ragdoll s, tapering. Everything shares one budget: vr_drips_max drops a frame, round robin.

#include <glm/glm.hpp>

struct entity_s;

namespace qvr::drips
{

// A drop's liquid (gore::dropFrom's `liquid`).
enum class Liquid : int
{
    Blood = 0,
    Water = 1,
    Slime = 2
};

// Once a frame (vr_view.cpp, after the wounds): the wet things looked at, dried, their drops; the wounded's.
// `head`: the eyes (your own drops none close under them).
void frame(const glm::vec3& head);

// A wound event received (vr_wounds_net.cpp parseEvent; the QC's VR_Wound_Hit): entity `num` hit at `org` for `amount`
// (wounds Kind `kind`): a living monster's wound drips while it is hurt.
void onWound(int num, const glm::vec3& org, int amount, int kind);

// A water or slime drop from `dropFrom` landing at `where` (normal `normal`), `color` its: specks (vr_drips_splash) and
// a soft drip heard (vr_drips_volume).
void landed(int liquid, const glm::vec3& where, const glm::vec3& normal, const glm::vec4& color);

// A blood drop of the gore's landed (the test's: blood still lands as blood).
void noteBloodLanding();

// How wet `e` is (0 dry .. 1 soaked; a thing not looked at: 0).
[[nodiscard]] float wetness(const entity_s* e);

// All gone (a new map, vr_drips 0).
void clear();

// vr_drips_info, vr_drips_soak, vr_drips_test, vr_drips_bleed_test.
void registerCommands();

} // namespace qvr::drips
