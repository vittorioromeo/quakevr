// vr_selfcollide.hpp -- the drawn body against itself (round 21, "Body collisions"): a hand, or the weapon it holds,
// pushed into the other hand, the other arm, the wrist gadget on it or the body (the torso, the neck and head, the
// legs when drawn) is drawn stopped at its surface, and passes through only once pushed most of the way through.
//
// Proxies (vr_selfcollide.cpp), cheap and a frame old where they must be: each hand as the grasp's spheres of the
// jointed hand as it was drawn, each held weapon as a few capsules fitted to its model (along its length,
// flat ones as two or three side by side), the arms from the body as it was posed (the upper arm and two
// for the forearm, sized as make_vrbody.py's), the gadget as two capsules round its casing, and the torso, neck, head
// and legs as capsules on the body's frames (the torso's from the head, this frame).
//
// Each hand is moved (with what it holds, and so its fingers and its arm, whose IK reaches the drawn hand) by the least
// push along each contact's way out: the way it came in, kept while the tracked hand goes deeper (a hand going through
// an arm isn't turned out of the far side half-way). Two hands against each other share the push. Once the hand (all of
// it: the hand's spheres, the weapon's capsules) is vr_body_collide_pass of the way through (from touching on the side
// it came in to touching on the far side), or held out further than that times 15 cm, the contact lets go: the hand
// eases to where it is tracked, and stays let go until it is clear again (no flicker at the threshold).
//
// Pairs meant to touch are left alone, or eased out as they come together: a hand and its own arm, weapon and gadget; a
// two-handed grip (the helping hand, the gun it holds, both arms) as it is taken; an empty hand at or coming to the
// other gun's grip or cup, or to the torch in the other hand; a prop carried in both hands; a hand holding a ledge or
// carrying a gun by its foregrip; the body as a hand comes to a holster; a weapon's stock against the torso (a
// shouldered gun) and any weapon against the head (aiming down the sights). A free hand against the other hand's weapon
// is vr_hand_collide's (triangle exact, the fingers brushing it), not this.
//
// Drawn only: the game reads the tracked hands, as it does past vr_model_collide (melee, shots, aim, grabs, holsters).

#pragma once

#include "vr_hands.hpp"

#include "Zancle/Container/Vector.hpp"


struct entity_s;

namespace qvr::selfcollide
{

// What the view drew this frame (vr_view.cpp), for the next frame's proxies.
struct Drawn
{
    const za::Vector<glm::vec4>* hand[2]{nullptr, nullptr}; // each drawn hand's spheres (world: centre, radius)
    const entity_s* weapon[2]{nullptr, nullptr};               // the weapon drawn in the hand (its model's shape)
    bool mirrored[2]{false, false};
};

// The view (VR_SetupViewEntities), after vr_model_collide's beginView: each hand moved by its push (`s.pos`). Once per
// frame; again in the same frame (a redraw), the same push.
void beginView(hands::State& s);

// Before vr_model_collide's endView: what was drawn recorded (`drawn`), and the push taken back out of what the game
// reads (the hands, the muzzles and the two-handed grips as tracked).
void endView(hands::State& s, const Drawn& drawn);

// How far `hand` (and its weapon) is drawn from where it is tracked by this (world units).
[[nodiscard]] glm::vec3 drawnOffset(int hand);

// The arms' IK (vr_avatar.cpp, vr_body_collide_elbows): the least swing of `elbow` about the line from `shoulder` to
// `wrist` (radians) that takes it out of the torso, as this frame's view made its capsules (none: 0). The elbow's joint
// is kept `radius` (world units) less a centimetre outside them.
[[nodiscard]] float elbowSwing(const glm::vec3& shoulder, const glm::vec3& elbow, const glm::vec3& wrist, float radius);

// A new map: nothing pushed, nothing recorded.
void reset();

// vr_body_collide_bench [n] [list]: the solve as it is now, n times (1000): min, median and max microseconds, and the
// contacts; "list" also prints every capsule.
void bench_f();

} // namespace qvr::selfcollide
