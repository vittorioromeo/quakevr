
## Stacking (own solver)

Your request: physics props that collide with each other and stack, in Quake VR's own rigid-body solver
(`vr_physics_engine 0`, the default; the Box3D engine is the alternative being built beside it). Until now each prop
collided with the world alone. Props are touch triggers (a hand or your feet take them), traces go through them, so
boxes, backpacks, armour and thrown weapons passed through each other.

### What it does

- **Stacks.** Boxes rest flat on each other, and a stack stays still once it has settled (asleep), for as long as
  nothing touches it. A pyramid stands; a box dropped on a stack lands on it; a box or a gun thrown into a stack knocks
  it over, the boxes taking the throw's momentum by their masses.
- **Things on things.** A backpack or armour dropped on boxes rests on them. A thing too big for what it lands on
  (a backpack on one 8-unit box) tips off, as it would.
- **Your hands.** What you carry, in one hand or both, pushes the props it meets and nothing pushes it back: sweep a
  carried box through a stack and it knocks the top off. Lower a box onto a stack and let go: it stays there.
- **Waking.** A stack asleep wakes, all of it, when anything moves one of its boxes: a hand's push, a throw, a box
  taken from it (the ones above fall), one knocked by another prop.
- **Lifts.** A stack asleep on a lift rides it, up and down, asleep: the boxes keep their places on each other.
- **Water.** Props afloat push each other apart instead of overlapping, and a box set on a floating box rides it
  lower in the water.
- **Slopes.** A stack stands on a slope within the friction's angle (the 14° ramp at e1m1's start: five boxes, 30 s).

### How it works (`vr_rigid.cpp`, `vr_boxbox.hpp`)

With `vr_props_collide` on, all the props that collide are stepped together once a server frame, at the first one's
turn in `SV_Physics` (`stack()`); the rest of each prop's `SV_Physics_Toss` (its think, its water transitions) is
unchanged.

1. **Bodies.** Every rigid prop (`.vr_rigid`, toss or bounce), and every carried one (QC's `.carry_player`, one or
   both hands) as a **kinematic** body: infinite mass, its velocity and spin from its pose's change since the last
   frame. A body's mass is its box's volume times a density by kind: ammo and health boxes 1, backpacks 0.35 (cloth),
   armour 1.5 and weapons 2.5 (steel), gibs and heads 1.
2. **Broad phase.** Each body's world box, grown by its motion over the frame, sorted and swept along x (tens of
   bodies). Pairs of two sleeping bodies, or of a sleeping body and a still hand, are skipped.
3. **Waking.** A sleeping body that an awake one (or a moving hand's prop) really touches (the narrow phase below,
   within the motion's reach) wakes, with its whole island. Repeated until nothing more wakes.
4. **Islands.** Awake bodies joined by pairs are solved together (union-find); carried props join none (nothing moves
   them). **A prop with no neighbour is moved by the old single-body code (`rigidToss`) exactly as before**, so a
   thrown gun or a box alone behaves as it did.
5. **Narrow phase** (`vr_boxbox.hpp`). Oriented box against oriented box: the separating-axis test over the 15 axes
   (3 face normals of each box, 9 edge pairs), face axes preferred to edge axes (and A's faces to B's) unless clearly
   better, as Box2D does, so nearly equal choices don't flip from frame to frame. On a face axis, the other box's face
   most turned against it is clipped by the reference face's four side planes (Sutherland-Hodgman): every corner of
   the overlap within the margin is a contact, up to 8, reduced to the 4 spanning the largest area, the deepest
   first. On an edge axis, the closest points of the two edges. Contacts are speculative within a margin of the pair's
   motion over the step (0.5 units plus how far they approach), so a fast box doesn't pass through. The world's
   contacts are the old corner contacts (a corner in a surface, or reaching one within the step), unchanged.
6. **Solver.** Per island, in substeps (the most any of its bodies needs, at most 8, as before): gravity, water,
   spin drag; the contacts; the last step's impulses applied again (**warm starting**: the world's matched by corner,
   two bodies' by the contact's place on the body, within 15% of its size, and the same normal); then sequential
   impulses, each contact's normal impulse accumulated and clamped (pushes only), Coulomb friction in its cone, equal
   and opposite on both bodies, weighed by their masses and inertias. Bounces as before (restitution on real
   impacts). Iterations: 8 + 2 per body up to 8 bodies (a 5-stack gets 18), 16 for bigger piles, stopping early
   once no contact's velocity changes by more than 0.02 u/s in an iteration.
7. **Penetration.** Two bodies may overlap by 0.3 units (the slop) at rest; beyond it they are pushed apart by a
   quarter of the excess per substep, by split impulses as the world's contacts are (a velocity used for that move
   only, never kept): nothing is gained by getting out of an overlap, so no pops.
8. **Sleep.** A body rests when it touches something and is slow (0.15 m/s, 1 rad/s) for 0.3 s; an island sleeps
   when all its bodies rest, none touches a hand's prop, and one of them stands on the world. Asleep, its bodies are
   Quake's `FL_ONGROUND`, with their island kept: each frame costs a check that none of them moved. A stack resting
   on the world by one box is settled as one body alone is (flat on the floor within 8°, not sunk), the stack with it.
9. **Asleep on a lift.** Every body of the island takes the lift as its ground entity, so Quake's pusher moves them
   all, the same way, in the same frame (it takes their `FL_ONGROUND`, which is put back): the island stays asleep.
   Moved unevenly (one body pushed or blocked), it wakes. The world under a stack's base is checked as before (the
   rest memo; a lift or door leaving wakes it; water rising too).

**What isn't touched.** Props stay touch triggers (pickups and hand touches are unchanged); the collision between
them is inside `vr_rigid.cpp`. Players are still left out of the props' traces. The single-body path was only split
into two functions (a corner's contact, the wedge release), the arithmetic unchanged: with the setting Off, the traces
are identical to the base's.

### Settings (Throwing and Physics page, Physics)

| Cvar | Default | Menu | |
|---|---|---|---|
| `vr_props_collide` | 1 | Props Collide With Each Other: Off / Boxes and Items / Everything | Off: props pass through each other, as before. Boxes and Items: ammo and health boxes, backpacks, armour, thrown and dropped weapons. Everything: gibs and heads too. |

**Why Boxes and Items by default.** They are what you stack and throw, and there are few of them in a level (tens).
Gibs come by the dozen from each gib burst and fly apart: colliding with each other they cost an island solve while
they settle, and piled they rarely look better than overlapping. Everything is there for gib play (a head on a
crate).

Debugging: `vr_debug_throw 3` prints each island body's motion (`(island of N, S steps)`) and `stack: island N
sleeps / wakes`; `4` also each contact (`sB-A step r n target push impulse`) and each body's rest state.
`vr_rigid_dump [classname]` prints every rigid body: place, turn, speed, spin, asleep or awake, and its island.
`vr_profile` has `props collide` (inside `rigid bodies`) with `props world contacts`, `props pair contacts` and
`props solve`.

### Costs

COSTS

### Tests (mock headset; composites in the scratchpad's `stack/`)

TESTS

### Limitations

LIMITS

### In the headset

- [ ] Stack five boxes by hand, one on another. Do they stand still once settled, without shivering or creeping?
- [ ] Build a pyramid (3, 2, 1); put a backpack on two boxes side by side; armour on four.
- [ ] Drop a box on a stack from above; throw one into it; throw a gun into it. Does it fall as you'd expect, the
      boxes flying about as heavy as they look?
- [ ] Carry a box through a stack at waist height: it knocks them aside. Lower a box onto a stack, let go: it stays.
- [ ] Hold a box in both hands and push a box along the floor with it.
- [ ] Take the bottom box from a stack (grip it): the ones above fall.
- [ ] Stand on e1m1's lift (at -544 2656) with a stack on it: does it ride up and down without a shiver?
- [ ] Boxes in water: they bob and bump each other; a box on a floating box.
- [ ] Is the default right (Boxes and Items), and would you like gibs in (Everything)?
