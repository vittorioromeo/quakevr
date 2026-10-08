# Stealth AI: Idle / Alert / Hostile (plan, 2026-10-08)

The author's request (kit brief `stealth_request.md`, with his four answers): optional, on-by-default stealth mechanics
in the shared monster AI. Everything below is QC (`QC/vr_stealth.qc`, hooks in ai.qc, combat.qc, weapons.qc,
vr_melee.qc, client.qc) plus two small engine builtins (`Quake/vr/vr_stealth.cpp`: the light at a point, a player's
flashlight beam) and the physics sounds' call into QC (props' impacts are noises).

## One switch over everything: `vr_ai_enhanced`

`vr_ai_enhanced 1` (default) turns every rule below on; `0` is Quake's AI, exactly. It is the first row of
Combat > Stealth AI (searchable as "Enhanced AI"). It is read live: switching it off mid-map returns every monster to
vanilla at its next think (`VR_Stealth_Reset`: meter cleared, an Alert monster's investigation dropped, its walk
goal put back to its path or none, its stand/walk animation resumed, no marks); switching it on mid-map is safe (each
monster records its post at its first stealth think). Each rule has its own `vr_stealth_*` cvar under it.

## How Quake's AI maps onto the three states

| Quake's | Stealth's |
|---|---|
| th_stand / th_walk with `enemy == world` (ai_stand, ai_walk, ai_turn call FindTarget) | **Idle** |
| (new) `stl_state == STL_ALERT`: turning to, walking to, searching a point, then walking back | **Alert** |
| `enemy` set, th_run (FoundTarget -> HuntTarget) | **Hostile** |

- **FindTarget** (ai.qc) is the only place an idle monster sees the player. Vanilla: visible, not far, in front (or
  nearer than 120 units: always). With stealth, for a real player (not charmed, not the `sight_entity` relay), the
  sight is handed to `VR_Stealth_See(client)`: it fills the monster's **suspicion meter** and returns TRUE only when the
  meter is full (then FoundTarget runs as vanilla: sight sound, hunt). Everything else in FindTarget is unchanged
  (charm, invisibility, notarget, the portal route, the `sight_entity` hand-off between monsters).
- **Map-scripted wakeups stay vanilla** (his answer 2): `monster_use` (triggers, monster closets, `targetname`d
  monsters), `SPAWNFLAG_TRIGGER_SPAWN_ANGRY`, damage (T_Damage's "get mad"), MG3 aggro groups: they set `enemy` and
  call FoundTarget directly, never through the meter. FoundTarget clears any Alert state (`VR_Stealth_Hostile`).
- **Ambush** (spawnflags & 1, "deaf"): such a monster keeps Quake's meaning: it is not woken by other monsters (no
  propagation to it) and does not hear noises (`vr_stealth_ambush_deaf 1`); it still sees (the meter), feels contact and
  grazing shots, and an Alert ambusher only turns to look, it never leaves its post.
- **Sight sounds**: unchanged (SightSound at Hostile). Alert plays the monster's idle sound, quietly (its "huh?").

## Suspicion meter (his answer 1)

Per monster, `stl_meter` 0..1, filled while FindTarget's client is visible (its checks; through portals too):

`rate = light * near * facing * moving * crouch * senses * sensitive / vr_stealth_meter_time` per second, where
- **light**: the player's light level (below), smoothstep from `vr_stealth_light_dark` (20; Quake's lightmap scale, 128
  full) to `vr_stealth_light_bright` (80; the monsters and items of e1m1-e1m3 stand mostly in 32-96). Dark: 0 (never seen, however close: only touch, noise or a beam gives
  him away).
- **near**: 1 within 128 units, down to 0 at `vr_stealth_sight_range` (1500).
- **facing**: 1 in the front cone (Quake's `infront`, dot > 0.3), `vr_stealth_peripheral` (0.3) to the side, 0 behind
  (dot < -0.3).
- **moving**: `vr_stealth_still` (0.5) standing still, 1 walking, 2 running.
- **crouch**: `vr_stealth_crouch` (0.4) at a full crouch (eyes 40% under his standing eye height, tracked per player as
  the highest eye seen recently), scaled between.
- **senses** (per class, `vr_stealth_senses`), **sensitive**: `vr_stealth_sensitive` (1.5) for
  `vr_stealth_sensitive_time` (30 s) after an investigation that found nothing (his answer 3).

Unseen, the meter drains at `vr_stealth_meter_decay` (0.2/s). Above `vr_stealth_glimpse` (0.3) the monster goes
**Alert** and turns to look at where it saw him; full: **Hostile**. Coop: each player's light, crouch, speed and noise
are his own; a monster's meter is on the most suspicious player in its sight (`stl_suspect`, `stl_rate`): FindTarget's
sightings take the players in turn (a tenth of a second each), and another player's sighting while its suspect, seen
within 0.25 s, fills it faster is passed over (the suspect's next sighting counts the time between); a faster one takes
the meter over. So a lit player is spotted as fast with a dark one beside him as alone (coop test: 3.2-4.0 s either way
round), and the dark one doesn't speed it up.

### Player light level
`stealthlight(vector)` (engine): the world's lightmap at the point (R_LightPoint, current light styles) plus the
dynamic lights there (muzzle flashes, rockets, explosions, held torches: every `cl_dlights` entry by Quake's falloff),
**not** his own flashlight's lights (they light what he points at, not him). Sampled at the player's chest, at most
every 0.1 s, cached on the player (`stl_light`). Each client measures the light on its own player the same way
(`stealth::lightAt`, ten times a second, his own lamp's lights left out) and sends it in its VR move (`VrMove::light`);
the server takes it first (`clientlight(player)`), so a coop client's level is his own (his map's lightmaps, the dynamic
lights he sees: his muzzle flashes, rockets by him), on a dedicated server too. With none (a client that sends no VR
move: a bot) it falls back to `stealthlight` (the host's view), and with no client world to "lit" (128).

### Flashlight and torches
`flashlightbeam(entity player)` (engine): on, the lens, the direction, range and cone of that player's lamp, as his
own client lit it (`flashlight::beamNow`, sent in his VR move while lit: `VrMove::lampLit` and the beam; the lens moved
along with the hands when the server moves him, `rebaseHands`). The host's and every coop client's lamp count alike.
- A monster **in the beam** (in the cone, in range, traced clear from the lens): Alert, turns to the lens and walks
  towards it (it knows where the light comes from). Being lit also counts the lens as bright light for the meter, for
  the lamp's holder only (`stl_lit_by`: another player beside him is seen by his own light).
- A monster that **sees the beam's spot** (where the beam lands, visible to it, in front, within 1000): Alert, but
  only a rough guess at the source (the lens plus up to 256 units of error): it turns and searches there.
- A **held or thrown torch** (`vr_stealth_torch` 400 units: a torch the monster can see in front of it within that):
  Alert at the torch.

## Contact

A player touching an Idle/Alert monster spots him (Hostile) at once (`vr_stealth_contact`): his body's box within 4
units of the monster's, either hand (`handpos`/`offhandpos`) inside its box grown by 4, his held weapons (melee hits
are damage anyway) and a held prop's box overlapping. Checked in PlayerPostThink over `findradius(origin, 160)`.

## Noise

One entry point, called by every source: `VR_Stealth_Noise(vector org, float range, entity src, float kind)`.
`range`: units at which a monster with clear air hears it. Kinds: STEP (running), PROP (an impact), MELEE, GUN,
BLAST, VOICE (a monster's alarm, for propagation). Each source's range:

| source | range |
|---|---|
| running at full speed (horizontal speed over `vr_stealth_run_speed` 250, on the ground, not climbing/hanging) | `vr_stealth_noise_run` 400, every 0.25 s |
| a hard landing (a fall's thud) | 300 |
| a prop's impact (the physics sounds' played knocks, by volume, which already scales with mass and speed) | `vr_stealth_noise_props` (1200) times its volume |
| a melee blow landing (strength times the held thing's weight, a fist 1) / a whoosh | 250 * strength * weight / 120 |
| a shot, by weapon (axe 0, shotgun 1100, super shotgun 1300, nailguns 800/900, grenade launcher 600 (the launch; the blast is its own), rockets 900, lightning 900, enemy guns 1000) | times `vr_stealth_noise_guns` 1 |
| an explosion (T_RadiusDamage, every explosion: `VR_Stealth_Blast`) | 8 * damage, at least 800 (a rocket's or a grenade's 960), times `vr_stealth_noise_blasts` 1 |

**Propagation** (`VR_Stealth_Hear`): for each monster within the range (findradius), not dead, not excluded, not
Hostile: the distance, then the path: a clear line (traceline, monsters ignored): full; blocked but the point is in the
monster's PVS (around a corner, through a doorway): times `vr_stealth_noise_wall` (0.5); not in its PVS (thick walls,
other rooms): times `vr_stealth_noise_solid` (0.15). A solid prop (crate) in the line: times `vr_stealth_noise_absorb`
(0.6). A water surface between them (the noise in water and the monster's ears not, or the other way round): times
`vr_stealth_noise_wall` too. Times the class's hearing. Heard when the result is at least the distance (each monster
notes the last noise it heard: `stl_heard`, its kind and source, for the tests and the log). A notarget player's own
noises (running, blows, shots) aren't heard at all (Quake's cheat: the monsters pay him no heed; his explosions are).

**What a heard noise does**: the player's own noises (STEP, MELEE, GUN) from where he is: if the monster can also see
him (visible, any facing: it turns to the noise): Hostile (running is the one way to be spotted in the dark, per the
brief); else Alert at his position. Other noises (PROP, BLAST, VOICE): Alert at the noise's point: a thrown crate's
landing, not where it was thrown from (distractions).

## Alert: investigate (his answer 3)

Phases (`stl_phase`):
1. **TURN**: stand animation, `ideal_yaw` to the point, ChangeYaw at the monster's yaw speed times
   `vr_stealth_turn` (0.6: slower, wary) until facing (FacingIdeal) or 2 s.
2. **WALK**: walk animation, `movetogoal` towards a marker entity at the point (the monster's own `stl_marker`), until
   within 64 units, stuck 3 s or out of time (10 s plus a second per 10 units of the way; `vr_stealth_investigate 0`: no walking, it searches where it stands).
   Lava or slime just ahead of its feet (a pool level with the floor, which Quake's step check lets a monster walk
   into) ends the walk as stuck: it searches from the edge. A kind whose walk is its stand (Rogue's invisible swordsman)
   searches where it stands.
3. **SEARCH**: stand animation, looking about (a new random yaw within 120 degrees every 1-1.5 s) for
   `vr_stealth_search_time` (5 s).
4. **RETURN**: walk back to its post (where it stood when it was first alerted, and its yaw); a path walker resumes its
   path (`movetarget`) instead. Back: Idle, sensitive for a while.

A new disturbance while Alert restarts at TURN with the new point (a stronger one only, within 1 s). The meter keeps
working throughout; full: Hostile.

## Hostile and Alert propagation (monster to monster)

When a monster goes **Hostile** by its own senses or damage: every other idle/alert monster within
`vr_stealth_share_near` (256) or within `vr_stealth_share_view` (1000) and visible to it goes Hostile at the same enemy
(FoundTarget). Ambushers excepted. When one goes **Alert**: those within `vr_stealth_share_near`, or seeing it within
`vr_stealth_share_view` / 2, go Alert at the same point (no further hand-on from those: one hop).

## Grazing shots

A player's shot (FireBullets' trace, the lightning beam, a launched nail / rocket / grenade's straight line to the
first wall) passing within `vr_stealth_graze` (64) units of a monster it didn't hit, at any distance: Alert, turning to
where the shot came from (its point of origin).

## Corpses and gibs

An idle/alert monster that sees (in front, visible, within `vr_stealth_corpses` 600) a corpse (a dead monster, a
ragdoll) or a gib: Alert at it. A body raises the alarm once: after the first monster notices it (`stl_found` time),
others ignore it 30 s later (it is "known").

## Sneak attack

T_Damage: any damage from a player to an Idle monster (enemy none, not Alert) times `vr_stealth_sneak` (1.25).

## Climbing

Climbing and shimmying make no noise (no STEP while a hand holds a ledge or rung, or 0.5 s after).

## Exclusions (per class; `VR_Stealth_Class`)

Excluded (vanilla AI): **bosses** (monster_boss, monster_boss_final, monster_oldone, monster_oldone_new, monster_shub_*,
monster_dragon, monster_lava_man, monster_armagon, monster_super_shambler), **flyers** (FL_FLY: scrag, wrath, orb...),
**swimmers** (FL_SWIM: rotfish, eels), charmed monsters, monsters with no th_stand/th_walk. **Dormant** monsters too,
while they are (no damage taken or not solid: a statue knight before its map wakes it, Rogue's Guardian (monster_morph)
before it rises, a monster waiting to be spawned in): no noise, touch, beam or alarm wakes them; only their map does.
Everything else, id1's and the expansions' (hipnotic, rogue, MG1, MG3, Honey, the dopa monsters), shares it: the hooks
are in the shared ai.qc (`vr_stealth_test 106` puts down every kind the kit has: id1's, hipnotic's and rogue's).

## Senses (`vr_stealth_senses 1`)

| class | hearing | sight |
|---|---|---|
| dog (and MG3's demon dog) | 1.6 | 1 |
| zombie, mummy | 0.4 | 0.6 |
| fiend (demon1), shambler | 1.4 | 1 |
| knights, hell knights | 1 | 1 |
| ogre | 0.8 | 0.9 |
| everything else | 1 | 1 |

## Extras

- **Visibility gem** (`vr_stealth_gem 1`): a small gem on the wrist gadget, dark when unseen, bright when lit, ringed
  by the last noise's loudness (the player's `stl_light` and `stl_loud`, sent as client stats).
- **Meters over monsters** (`vr_stealth_debug_meters 0`, Debug > Tests > Stealth AI > Meters Over Monsters; tuning):
  over each living monster within `vr_stealth_debug_meters_range` (2000) of the host's head, through walls, its state
  (idle; ALERT and its step: turn, walk, search, return; HOSTILE and how long it hasn't seen you; "quake's ai" for one the
  rules leave alone) and its meter as a 20-cell bar (green idle, yellow Alert, red Hostile). Engine-drawn
  (`stealth::debugFrame`, the server's fields read on the host): a coop client doesn't see them.
- **Marks** (`vr_stealth_marks 0`): a "?" over a monster going Alert, "!" going Hostile (`floattext`).
- **Lose the player** (`vr_stealth_lose 1`): a Hostile monster that hasn't seen him for 6 s goes to where it last saw
  him and searches there (Alert), then returns.

## Slipgates (`vr_stealth_gates 1`; seamless slipgates, PORTAL_AI.md)

Paired gates (a gate that comes out of another gate's face, `vr_slipgate_pair_exits`) are openings to the stealth AI:
- **Sight**: every monster under the rules sees a player through a paired gate (Quake's AI and `vr_stealth_gates 0`:
  only ranged monsters, through every gate). The meter's distance and facing are to his image in the gate.
- **The point is his image** (where he shows in the gate, beyond its face in the monster's room's terms), with the gate
  it is seen through (`stl_via`): an Alert monster walks into the gate's middle and through (`vr_portals_monsters`;
  a gate it can't walk through: up to its face, then it searches there). A melee monster gone Hostile at a player it
  sees only through a gate runs through to him (ranged ones stay and shoot through, PORTAL_AI.md).
- **Crossing** (the engine's VR_Portal_Crossed, which now gets the gate's yaw, the face gone into and the face back):
  its point, post (and its yaw), last sighting and walk marker go through with it: the point it walked through for is
  in its room now; the others are beyond the face back (going back to its post it walks back through). No back and
  forth.
- **Noise**: a noise beyond a paired gate's exit is heard by the monsters on the gate's side at its image: the way's
  length through the aperture (the straight line to the image; held to the aperture's edge: round a corner,
  `vr_stealth_noise_wall`), each room's part of the way muffled by its walls and props as in the room. An Alert one walks
  to the gate, through, and on to the noise.
- One hop: a point two gates away is walked to through the first gate only.

## See-through walls (`vr_stealth_seethrough 1`)

Grates, fences, webs and glass let sight, light and sound through: the stealth AI's lines (visible()'s sight line,
point_visible; a noise's way, VR_Stealth_NoiseLine; the flashlight's beam on a monster and its spot; another monster, a
torch or a body in sight) are `traceseethrough` (engine, vr_stealth.cpp): traceline, but a hit on a face with a `{`
(alpha-tested) texture, the world's or a brush entity's, or on an entity drawn see-through (`alpha` under 1: a glass
func_wall) goes on from the far side of it (at most 8 in a line; out within 64 units). So a monster sees you through a
grate (its meter as in the open), hears a knock or a shot through it at full reach (not the wall's 0.5) and your beam
through it reaches it. Shots and bodies still stop at them (traceline). Off, or `vr_ai_enhanced 0`: traceline (Quake's).
A noise of the world's (a blast, a test's knock) is now traced ignoring the hearer: a trace ignoring world skipped every
entity world owns (every brush entity and prop: a door between, a crate in the way).

## Cvars (all CVAR_ARCHIVE, Combat > Stealth AI)

`vr_ai_enhanced 1`, `vr_stealth_meter 1`, `vr_stealth_meter_time 1.5`, `vr_stealth_meter_decay 0.2`,
`vr_stealth_glimpse 0.3`, `vr_stealth_light_dark 20`, `vr_stealth_light_bright 80`, `vr_stealth_crouch 0.4`,
`vr_stealth_still 0.5`, `vr_stealth_sight_range 1500`, `vr_stealth_peripheral 0.3`, `vr_stealth_contact 1`,
`vr_stealth_noise 1`, `vr_stealth_run_speed 250`, `vr_stealth_noise_run 400`, `vr_stealth_noise_props 1200`,
`vr_stealth_noise_guns 1`, `vr_stealth_noise_melee 1`, `vr_stealth_noise_blasts 1`, `vr_stealth_noise_wall 0.5`, `vr_stealth_noise_solid 0.15`,
`vr_stealth_noise_absorb 0.6`, `vr_stealth_gates 1`, `vr_stealth_seethrough 1`, `vr_stealth_investigate 1`, `vr_stealth_turn 0.6`, `vr_stealth_search_time 5`,
`vr_stealth_sensitive 1.5`, `vr_stealth_sensitive_time 30`, `vr_stealth_share_near 256`, `vr_stealth_share_view 1000`,
`vr_stealth_graze 64`, `vr_stealth_flashlight 1`, `vr_stealth_torch 400`, `vr_stealth_corpses 600`,
`vr_stealth_sneak 1.25`, `vr_stealth_ambush_deaf 1`, `vr_stealth_senses 1`, `vr_stealth_lose 1`, `vr_stealth_gem 1`,
`vr_stealth_marks 0`, `vr_stealth_debug 0` (1: state changes, 2: each meter step and noise heard).

## Status (2026-10-08)

All phases below are in (the tests: ROUND21.md, "Stealth AI" and "Stealth AI: the gaps closed"). Not done / limits:
- The meter is one per monster, on the most suspicious player in its sight (not a meter per player: the dark player's
  own suspicion isn't kept while the lit one holds it).
- A client's light and lamp are his own client's word (sent in his VR move): a modified client could send "dark".
- Lose the player: after a fixed 6 s out of sight (STL_LOSE_TIME), no cvar for the time.
- Alert monsters walk with Quake's movetogoal (no path finding): a point across a gap or up a ledge ends their walk when
  stuck (3 s without headway), then they search where they are.
- Quake's own relay stays: an idle monster that sees another turn Hostile (FoundTarget's `sight_entity`, a tenth of a
  second) turns Hostile at its enemy too, whatever the light on him (as `vr_stealth_share_*`, but with Quake's sight
  ranges).
- Cost (`vr_stealth_test 103`, vr_profile's "stealth" scope under quakec, e1m1, Release): 45 grunts with you in sight
  and your steps heard (16 spotted you) 0.018 ms a server frame (worst 0.11); 60 grunts, one in six dead (their bodies noticed: 50 investigating), a
  knock every 2 s: 0.062 ms (worst 0.51, the frame a knock is heard by them all); bodies off 0.042.

## Phases

1. Core: states, meter, light builtin, contact, vanilla scripted wakeups preserved, the master switch and its reset,
   sneak multiplier. Test `vr_stealth_test`.
2. Noise API + propagation; running, landing, props (physsound), melee, gunfire, explosions; climbing silent.
3. Alert investigation (turn, walk to the point, search, return, sensitive).
4. Hostile / Alert propagation between monsters.
5. Grazing shots.
6. Flashlight beam on / seen; torches.
7. Corpses and gibs.
8. Extras: marks, senses, lose the player, visibility gem.
9. Menu: Combat > Stealth AI.

## Tests (`vr_stealth_test <n>`, QC; Debug > Tests > Stealth)

Each test spawns a grunt near the player on e1m1 (or uses the nearest monster), sets the scene and prints
`stealthtest: <n> <verdict> <numbers>`:
1 dark, crouched, still, 100 units in front: stays Idle (meter 0) for 5 s.
2 the same, lit (a light test's level forced: `vr_stealth_light_force`): meter fills, Hostile within ~2 s.
3 running 300 units behind it, clear air: Hostile/Alert; the same through a thick wall: Idle.
4 a crate dropped 250 units from it: Alert, walks to the crate (distance to the impact falls under 80).
5 touching it (the player's box against it): Hostile.
6 `use` on a targeted monster (monster_use): Hostile at once (vanilla).
7 sneak hit: a 10-damage T_Damage on an Idle grunt takes 12.5 (health before/after).
8 `vr_ai_enhanced 0`: test 1's scene gives vanilla's instant sight (Hostile) and an Alert monster is reset.

Further scenes (`vr_stealth_test 100`-`108`, `vr_stealth_test2.qc`: their own grunts, put down and taken away;
`Misc/quakevr/stealth_tests.sh <agent> [gun|blast|kinds|infight|saveload|liquid|gates|horde|all]` runs them headless,
`Misc/quakevr/multiplayer/stealth_mp_test.sh <agent>` the coop one):
- 100 a real shot (the script pulls the trigger, `+attack` with the weapon in the hand: `vr_weapon_grip_mode 1; impulse
  9; impulse 150+id`): three grunts behind him at 0.8 and 1.2 of the weapon's reach in the open and 0.8 behind a wall
  (`vr_stealth_noise_guns` scaled to the room): only the first hears it; the noise's reach is the weapon's.
- 101 explosions of 50 and 200 damage at his feet: 800 and 1600 units (scaled), the same three.
- 102 coop: each client's light and lamp known to the host, the meter on the lit one (either way round), the client's
  lamp on a grunt's back: Alert at his lens.
- 103 the horde (cost); 104/105 an investigation saved mid-walk and carried on after the load; 106 every kind; 107
  infighting; 108 lava's edge (e1m7).
- 110-113 through slipgates (`vr_stealth_test3.qc`, vrslipgates, the player in room U: `stealth_tests.sh gates`): a
  grunt in T sees him through T's north gate (Alert at his image, not at him), walks through, on to his spot, searches
  and comes back through to its post (2 crossings); a knock in U heard only through the gate, the same walk (not heard
  with `vr_stealth_gates 0`, nor at 0.85 of the way's length); a dog sees him through the gate and comes through (not
  with `vr_stealth_gates 0`); `vr_ai_enhanced 0`: the dog doesn't see him through it, the grunt does.
- 120 see-through walls (`vr_stealth_test4.qc`, e1m1: `stealth_tests.sh seethrough`): a lit grunt 300 units off and an
  e1m1 door model put between: opaque, it neither sees him nor hears a knock where he stands (1.15 times its distance);
  alpha 0.5: both; `vr_stealth_seethrough 0`: neither; opaque but its textures taken for fences
  (`vr_stealth_test_fence`: the kit's maps have no solid `{` brush): both; not: neither.
