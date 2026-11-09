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

### Material and property table (built)

- **What:** one material id per entity (`VR_MaterialOf(e)`, QC/vr_crates.qc: `VR_MAT_*` from vr_impact_shared.h) and
  one property row per material (`Quake/vr/vr_material_shared.h`: `VR_MATP_FLAMMABLE`, `_CONDUCTIVE`, `_BLEEDS`,
  `_SINKS`, `_SPLINTERS`, `_SPARKS`; the armour plates' buoyancy rate). Both the QC and the engine include the header.
- **Use (QC, QC/vr_material.qc):** `VR_Mat_Has(e, VR_MATP_FLAMMABLE)`, `VR_Mat_Is(m, bit)`, `VR_Mat_Row(m)`,
  `VR_Mat_Plates(e)` (plates worn). A new kind of thing gets a material in `VR_MaterialOf` and nothing else; a new
  property is a bit in the header and a column in the rows.
- **Consumers switched:** fire (`VR_Burn_Can`: the flammable bit replaces `VR_Burn_Metal`), hits (`VR_Material_Fleshy`,
  `VR_ImpactFx`'s sparks and splinters), armour (`VR_Hit_Armoured`: plates worn count), buoyancy (`ownFloat`,
  vr_box3d_step.cpp: plates worn times the header's rate). Not yet: `physsound::materialOf` (model recordings),
  `debris::materialOf` and `audiosim` (world textures): they classify models and wall textures, not entities.
- **Proof:** `vr_systems_test 1` (Debug > Systems Tests) prints every entity's answers; identical before and after
  the switch on e1m1, e2m1 and e4m1 with `sv_random_seed 1` (1142 lines).

### Wall mounts (built)

- **What:** QC/vr_mount.qc. `.vr_mount` (the engine reads it), `.vr_mount_detach` (the thing's own detach),
  `VR_Mount_Attach(e, fn)`, `VR_Mount_Detach(e, by, dir)`. One detach rule: a physics object of any origin (thrown,
  dropped, flung by a blast) whose mass and speed towards it pass a throw's speed for that mass (`VR_Mount_Strong`,
  the same curve as `VR_Thrown_HitSpeed`), and a ragdoll's body by the same rule through the engine's body hits
  (`VR_Mount_BodyHit`). Never a player or a living monster walking past. A hand's grab and shots stay the thing's own.
- **Users:** wall torches (`VR_WallTorch_MountOff`). A new mounted thing calls `VR_Mount_Attach` at spawn and
  `VR_Mount_Touch(self, other)` from its touch.
- **Test:** `vr_test_walltorch_shot 25` (a crate flung, not thrown, at a torch: knocked off), `26` (the same at a
  fifth of the speed: stays), `20` (a hand's throw: knocked off, as before).

### Momentum, not "thrown by a hand" (built)

- Ragdolls: `vr_kdtrig_flung 1` (Gameplay > Knockdowns, "Flung Bodies"; engine, vr_box3d_step.cpp callBodyHits): a
  body no hand holds, flung by anything, strikes monsters (knockdowns: QC `VR_KdTrig_BodyHit`) and wall-mounted things
  by its momentum, as a thrown one.
- Props: `VR_Prop_Flung` (vr_carry.qc) hurts by speed and mass whatever moved the prop, and now unbalances the monster
  it hits the same way whoever moved it (`VR_HitStagger_Throw` sets `vr_hst_impact`: no player needed). Credit: the
  holder, the hand that let go, a monster that threw it, else the last blast's attacker (`.vr_impulse_by`,
  `.vr_impulse_t`, set by `T_RadiusDamage`).
- Tests: `vr_systems_test 16` (a dropped crate and one let go of by your hand roll the same stagger chance, 0.405),
  `vr_systems_test 15` (a body flung into a wall torch knocks it off; the torch lights the crates),
  `vr_test_walltorch_shot 25/26`.

### Status component (built: burning, wet, shocked, stunned)

- **What:** QC/vr_status.qc. `VR_Status(e)` gives `VR_STATUS_*` bits for any entity, the player included, server-side.
  Burning is the fire's own `.burn_fire`; shocked is `.vr_shock_until`; stunned is a knockdown or a parry stagger;
  wet is kept here (`.st_wet_t`, `.st_wet_level`) by the client drips' rule (soaked in water or slime, full for a
  quarter of `vr_wounds_wet_time`, then fading). The player's `WaterMove` and the monsters' liquid look call
  `VR_Status_Soak`; `VR_Status_Wet(e)` also looks itself. Nothing new is networked: the clients draw wetness as before.
- **Use:** `VR_Status(e) & VR_STATUS_WET`, `VR_Status_Wet(e)` (0..1), `VR_Status_Ignite(e, by)`,
  `VR_Status_Douse(e, why)` (through the fire's own code). Bleeding is a reserved bit, not wired.
- **Test:** `vr_systems_test 13`.

### Other rules built on these (2026-11-09 quick wins)

- Fire spreads from anything burning to anything flammable it touches (`VR_Burn_CrateCatch`; flesh by
  `vr_burn_flesh_catch` 0.25 each half second): `vr_systems_test 10`.
- Explosions light flammable wood within `vr_burn_blast` (0.5) of their radius; bodies by `vr_burn_blast_flesh` (0):
  `vr_systems_test 11`.
- Floor buttons pressed by monsters and objects of `vr_monsters_press_buttons_mass` kg or more, only with
  `vr_monsters_press_buttons 1` (default 0): `vr_systems_test 12`.
- A grunt's thrown thing disarms another grunt, credited to the thrower: `vr_systems_test 14`.
- Every monster's lightning (`VR_AI_Lightning`) conducts into water as the player's beam does (`VR_LGWater_Beam`):
  `vr_waterzap_test 4` logs "lgwater: electrified".

### Style detection (open)

Planned: per-area map style from the surrounding textures (base, medieval, runic, liquids), a debug command and a
builtin. Not built in this round.
