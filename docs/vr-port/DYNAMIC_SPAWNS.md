# Dynamic map spawns: ideas by map style

Started 2026-11-09 from the author's request: spawn more interesting things in maps depending on the map's style,
geometry and textures, beyond today's crates, barrels and unique weapons (flare gun, crowbar). The best entities have
several uses (trap, hazard, grabbable, weapon, tool) and still feel like they belong in the place.

This file is an idea list. Nothing here is decided: a BACKLOG.md item ("Dynamic spawns: pick ideas") says to go
through it and pick the ideas to build. Every idea is kept, good or bad, so the choice is made here once.

The shared code the ideas need is already built; see "Prerequisites available" at the end before writing any of them.

## Base / tech (E1 style)

- **Gas canisters** (wall racks or loose). Force-grab and throw as an improvised grenade. Shot, they vent a jet of
  flame and spin on the floor like a rocket. They explode when fire reaches them. Armed humanoids can grab them too.
  Reuses the flamethrower and poison-tank effects and fire spread.
- **Electrical junction boxes** on tech wall panels. Shot, they arc lightning at nearby entities for a few seconds,
  harder on things standing in connected water. Breaking one by a door or lift stalls it briefly. Pairs with the
  lightning gun and water conduction.
- **Ceiling lights and hanging lamps.** Shoot one down: the light goes out (stealth, if NPCs perceive light), the lamp
  falls as a physics prop that hurts what it lands on, and its sparks can ignite gas.
- **Steam pipes** along walls. Shot, they leak scalding steam that blocks NPC sight and hurts whoever passes. Grab and
  twist the valve to shut it.
- **Fire extinguishers.** Grab and squeeze to put out fires on props, ragdolls, NPCs or yourself. Shot, they burst
  into a blinding cloud. Throwable as a blunt weapon.
- **Ammo and supply lockers.** Open with a melee strike, a blade pry (blade embedding) or a shot to the lock.
- **Proximity mine and tripwire kits.** Armed humanoids plant them on patrol routes; the player can disarm, pick up and
  re-place them.

## Medieval / castle

- **Wall-mounted weapons:** swords, axes and maces on racks, pulled off by hand or force grab.
- **Wall-mounted shields.** They become an off-hand shield that blocks projectiles and parries, takes dents (armour
  dents) and can be thrown. NPCs grab them when disarmed.
- **Torches in sconces** (exist today as wall torches): take one to light your way, ignite things, swing it as a club,
  throw it to start fires at range. A ragdoll or an explosion knocks one onto straw or crates; shooting one out
  darkens the area.
- **Chandeliers.** Shoot the chain and it falls, crushing what is below and spilling burning candles. Climbable and
  swingable (climbing on props).
- **Hay and straw piles.** Very flammable fire spreaders; they soften falls, hide you, and hold embedded blades.
- **Oil cauldrons and lamps.** Tip a cauldron to pour burning or slick oil down stairs; a thrown lamp is a fire bomb.
- **Portcullis winches and drawbridge chains.** Cut the rope with a blade to drop a portcullis (a trap, or a crush);
  wind it back by hand.
- **Ropes** (bundles, hanging): climbable, cuttable, tie a ragdoll or a prop. Needs a rope system.
- **Armour stands.** Take a pauldron or a helmet as dynamic armour; a "dormant" armoured knight may be standing there;
  knocked over, a stand clatters (a noise distraction).

## Runic / metal (E2-E4 style)

- **Rune braziers.** Magical flame that heals or harms nearby; knocked over, it spills a fire pool; grab the coal as a
  hot projectile.
- **Spiked wall traps.** Spikes press out of wall panels when a nearby plate is stepped on; they impale ragdolls
  (embedding, dismemberment) and can be triggered with a thrown body.
- **Crystal shards.** Shot or shattered, they split lightning or nails in several directions (a ricochet multiplier).
  Wieldable as a short-lived stabbing weapon that shatters on impact.
- **Chained prisoners or corpses.** Cut one down to drop gore or a hidden item; swing it as a pendulum into enemies.
- **Bone piles.** Throw bones or club with them; rattle them as a noise distraction.

## Sewers / water (slime and water areas)

- **Floating debris:** barrels and planks that float and bob (buoyancy for props and ragdolls). Stand on them; they
  block shots.
- **Slime drums.** Puncture one to pour spreading, damaging slime; dunk enemies' heads; drown a held ragdoll.
- **Sluice gates and valves.** Turn a wheel to raise or lower the water: flood an area to drown or extinguish, drain it
  to reach items.
- **Rats and small vermin.** Ambient prey that scatters from light and noise, eats corpses, and can be thrown.

## Outdoor / AD forest and ruins

- **Beehives and wasp nests** in trees. Knock one down and the swarm attacks whoever is nearest (a third faction); fire
  drives them off.
- **Dead trees and logs.** Push one over for a bridge or a roadblock; roll it downhill into enemies.
- **Rock piles on slopes.** Shoot the keystone for a small landslide of physics rocks.

## Cross-style

- **Barrel variants:** oil (burns), gunpowder (explodes), water (extinguishes, steam).
- **Breakable windows and grates:** shortcuts; glass shards to wield or throw.
- **Corpse loot:** armed humanoids drop their actual weapons and armour; any NPC can pick up dropped gear (the player
  is not special).

## Systems ideas from the 2026-11-09 audit

From the systems audit (duplication, wider use, synergies, missing systems). Status: **done** (built in
`agent/systems1`, see "Prerequisites available"), **partly**, or **open**. Pick from these together with the spawn
ideas above: most spawn ideas lean on one of them.

### Ranked proposals

| # | proposal | status |
|---|---|---|
| 1 | Momentum-based impacts for any fast rigid body (thrown, dropped or blast-flung bodies and props hurt, topple, knock torches, press buttons; credit the last impulse source) | see "Prerequisites available" |
| 2 | Generalised fire spread and ignition (burning bodies ignite flammables, explosions ignite, one `VR_Burn_FromThing`) | see "Prerequisites available"; explosive-box cook-off **not wanted** (author, 2026-11-09: keep explosive boxes as they are) |
| 3 | One material and property table | see "Prerequisites available" |
| 4 | Server-side status component (wet, burning, shocked, bleeding, stunned) | see "Prerequisites available" |
| 5 | Armour from the worn plates everywhere (buoyancy and material from `vr_ar_mask`); more wearers (ogres, AD plated monsters, zombies in mail); optional per-plate rule for the player | buoyancy/material: see "Prerequisites available"; more wearers and the player rule **open** |
| 6 | Symmetric disarm, parry and clash through one held-line accessor `VR_HeldLine(e, hand)` (grunt vs grunt in infighting, optional rare player disarm) | **open** (only the thrown-thing disarm between grunts is in) |
| 7 | NPCs use the world: grunts throw back grenades, pick up and throw props and torches, carry torches in the dark, kick burning crates | **open** |
| 8 | One conduction function for every lightning source, plus wet and metal | see "Prerequisites available" |
| 9 | Unified liquid tests and hazards (one breath model, one drowning for player and monsters) | tests: see "Prerequisites available"; one hazard/drowning path **open** |
| 10 | Perception of smoke and world events (smoke blocks sight, monsters hear monster steps and fights, a burning ally alerts) | **open** |

### Duplication still to merge (open)

- Blood drips: `bodyblood`'s own drop simulation folded into `drips` as a hand/forearm source.
- Footsteps: one "step event (entity, foot, speed)" sink for enemies and the player; floor material picks the sound;
  bloody prints for anyone; noise for stealth.
- Weapon lines, parry and clash: four line representations, two parry systems (see proposal 6).
- Flat shove: pass reach and pad as parameters instead of mutating `vr_bash_*` globals.
- Player hit geometry: a head capsule so headshots on the player paint wounds.
- Dynamic lights: one light-source registry, server-visible for gameplay sources; rendering picks by budget, stealth
  sums without it.

### Wider use (open unless marked)

- Armour on more kinds: ogres (helmet, pauldrons), AD's plated monsters' real plates, zombies in mail, the dummy.
- Climbing for NPCs: grunts mantling low walls; active ragdolls catching ledges as they fall.
- Disarming the player (a grunt's parry or tug, low chance, cvar-gated); grunts disarming each other; blasts knocking
  weapons out of hands.
- Throwing as an NPC attack (grenades back, torches, axes that stick, props).
- Embedding for grunts' blades (stabs leaving blades in corpses or props; thrown axes sticking).
- Hot blades for anyone (a knight passing through fire, a grunt's blade near a torch).
- Torches carried by armed grunts in dark maps (light AI can use, a fire spreader).
- Bloody footprints for monsters walking through pools; dragged ragdolls leaving smears.
- Stealth noise from monster footsteps, burning crackle, falling bodies, armour clanking (`VR_Stealth_Noise`).
- Enemy shove between monsters (out of a doorway, into hazards) and on props.
- Floor and wall buttons pressed by monsters and objects: **done behind `vr_monsters_press_buttons` (default 0)**.

### Missed synergies (open unless marked)

- Fire and armour: burning scorches plates and skin.
- Fire and ropes (needs world ropes, below).
- Wet things resist ignition: the wet status exists now (see "Prerequisites available"), the rule is **open**.
- Lightning ignites (the shock chars but never calls `VR_Burn_Ignite`).
- Smoke blocks NPC sight.
- AI notices a burning ally or a fire.
- A thrown prop blocking an incoming melee line.

### Missing systems (open)

- World ropes and chains (hanging lamps, chandeliers, bridges) with Box3D joints; they burn and can be cut.
- Destructible brush geometry beyond crates (planks, doors splintering), as "func_breakable as props".

## Shared building blocks

1. Style detection per area from the surrounding textures, plus placement rules ("on a wall of tech texture, 64 units
   above the floor, near a door, at most 2 per room"). Built: see "Style detection" below. Placement rules are not.
2. One material and property table and a server-side status component on every entity. Built: see below.
3. Wall mounts: a generic "attached to a wall" state that a grab, a hit or an impact detaches (swords, shields,
   torches, extinguishers, chandeliers, lamps). Built: see below.

## Prerequisites available

Built 2026-11-09 (branch `agent/systems1`). Use these instead of new classname checks.

(Filled in below as each piece lands.)
