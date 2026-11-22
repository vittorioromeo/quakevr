# Climbable ropes and chains

Physical ropes and chains: placed in maps, spawned with the toolgun, swinging and pushed by what meets them, the same on
every client. His spec of 2026-11-21 (ROUND25.md, "ropes"); built in phases:

| Phase | What | State |
|---|---|---|
| P1 | The physical rope and chain: map entities, the toolgun's (with its settings page), disturbance, MP sync, the test map | done |
| P2 | VR: grab with either hand, hand over hand up and down, the legs' IK round the rope, letting go keeps the momentum | to do |
| P3 | Flat-screen auto-climb (as the flat ladders), pumping the swing (both modes, to a cap: rope to rope), the climber's pose in MP | to do |

## What it is

A rope is a chain of points hanging from its top (the entity's origin), `pieces` pieces each `length / pieces` long,
stepped by the server with the grappling hook rope's and the magnetic holsters' Verlet steps (`Quake/vr/vr_verlet.hpp`):
each step it falls and is pulled back to its pieces' lengths (`vr_rope_iterations` passes, its stiffness of each
correction), then the frame's pushes, then the world (the level, doors and lifts: traced through the server's world, so a
dedicated server's ropes collide too). A frame is cut into steps of at most 1/120 s. It never pushes back on the
world: it lies on the floor where it is longer than the drop, slack.

Two styles, rope and chain: they look different and behave the same. A rope is drawn as a plain cable (brown, its
thickness across); a chain as the magnetic holsters' chain of square-bar iron links (`vr_coil.hpp`, rigid style; links
sized from its thickness). Both through a Catmull-Rom curve over the points (four cuts a piece), lit by the world.

## What pushes it

- **Hands:** each player's hands (a 3-unit sphere round each, `vr_rope_hands` 1): a point inside is put out to its
  surface and takes the hand's speed along the way out (its speed from its move since the last frame; a jump over
  1000 units/s, a teleport, counts as still).
- **Bodies:** players, monsters (`SOLID_BBOX` / `SOLID_SLIDEBOX`) and the physics props (`box3d::isBox3DProp`): a point in
  a box (grown by the rope's thickness) is put out along its shortest way and takes the box's speed along it.
- **Explosions:** T_RadiusDamage's `physicsblast` (the props' push) kicks every point within damage + 40 units, away from
  the blast, damage x 4 x (1 - distance / reach) units/s, times 5 / mass (0.25..2: a heavy chain moves less).
- **Shots:** the shot push (`physicsshot`, weapons.qc VR_ShotPush) kicks the first piece each rope's shot line crosses
  (within its thickness / 2 + 2 units): the shot's velocity times its mass over the piece's points' mass, 400 units/s at
  most.

## Placing one

**In a map:** `vr_rope` (drawn as a rope; `rope_style` 1: a chain) and `vr_chain` (a chain). It hangs from its origin:
put it just under a ceiling or beam. Keys (0 or unset: the default):

| Key | Default | |
|---|---|---|
| `rope_length` | 256 | units, from the origin down (longer than the drop: the rest lies on the floor) |
| `rope_thickness` | 1.5 | units across (0.25-8) |
| `rope_segments` | one every 16 units | its pieces (2-64) |
| `rope_mass` | 5 (a chain 20) | kg, the whole rope |
| `rope_stiffness` | 1 | 0.05-1, how little it stretches |
| `rope_damping` | 0.5 | per second, how fast its swing dies down |
| `rope_style` | 0 | `vr_rope` only: 1 drawn as a chain |
| `target` | | its bottom pinned at that entity's origin (an info_notnull) |
| spawnflag 1 PINNED | | its bottom pinned where it first hangs |

TrenchBroom: `Misc/trenchbroom/entities.fgd` (merged into the generated `quakevr.fgd`).

**With the toolgun:** Spawn > Props > the Ropes group (Rope, Chain; their own page: `>>` jumps to it). Under the tiles,
**Rope Settings (length, thickness, ...)** opens the settings page for the next one made: Length, Thickness, Pieces
(0: auto), Mass (0: auto), Stiffness, Damping, Pin the Bottom (the `vr_rope_spawn_*` cvars, each player's own); Back.
The trigger hangs it from the ceiling above where the ghost stands (aim at the floor under a ceiling), or its length up
where there is no ceiling within reach; the ghost is a single link. A scenario's or the Rag Tool's spawn
(`toolgun::spawnEntryAt`) hangs it the same way.

The rope follows its entity: moved by a map's mover or the physgun, its top goes with it.

## Multiplayer

The server's ropes go to every client (`QVR_SVC_CLIMBROPE` 55, `Quake/vr/vr_protocol.hpp`): its entity, chain or rope,
thickness, the first point as a coordinate and each next as eighths of a unit from the one before (as the grappling
hook rope's corners). Unreliable, whenever a point moved over a quarter of a unit since the last send, else every half
second; a new client gets every rope with his spawn state; a rope removed is sent reliably as gone. The client draws
the last points it got (no interpolation: the server sends every frame a rope moves). A listen server's own client
draws from the same message, through the loopback: one path.

## Defaults where the spec is silent (his call; each a setting or key)

- A map rope hangs from its origin, 256 long unless `rope_length`; `target` pins its bottom; pieces about 16 units.
- 5 kg a rope, 20 a chain; stiffness 1 with 16 passes; damping 0.5 a second; thickness 1.5 units.
- The toolgun hangs it from the ceiling above the ghost (not at the ghost): you aim at the floor where it is to hang.
- The ropes don't push the props or monsters back (they are light; the bodies push them).
- Not yet (P2/P3): grabbing adds the player's mass to the segment held; climbing at the ladders' speed; the swing cap
  from a cvar.

## Files

- `Quake/vr/vr_climbrope.cpp` / `.hpp`: the server's ropes, the pushes, the messages, the client's drawing, the
  commands. `vr_verlet.cpp` `collide`'s `serverWorld` (the server's traces); `vr_geom.hpp` `segmentsDistance` (shots).
- QC: `QC/vr_ropes.qc` (`vr_rope`, `vr_chain`: the settings only), the fields in `QC/vr_sys_fields.qc` (`rope_*`; engine
  side `vr_fields.inc`).
- The toolgun: `vr_toolgun_packs.inc` (the entries), `vr_toolgun_pick.inc` / `vr_toolgun.cpp` (hung after the spawn),
  `vr_toolgun_panel.cpp` `ropePage`.
- Hooks: `vr_server.cpp` (frame end, spawn state), `vr_client.cpp` (parse), `vr_text3d.cpp` (drawn), `vr_builtins.cpp`
  (an edict freed), `vr_builtins_melee.inc` (physicsblast), `vr_builtins_ragdoll.inc` (physicsshot).
- The test map: `quakevr/maps/vrropes` from `Misc/quakevr/make_vrropes_map.py` (`--compile`): four swinging ropes 192
  apart (rope, chain, rope, chain) and a ledge to jump from, a pinned rope, a heavy chain, a rope longer than the drop.

## Commands and settings

- `vr_rope_info`: the server's ropes (`rope_sv:` lines: entity, points, style, bottom, speed, settings) and the client's
  copies (`rope_cl:`), then `rope_info: done`. Debug > Tests > Climbable Ropes > Print the Climbable Ropes.
- `vr_rope_spawn [chain] [length]`: one hung 96 units ahead of the first player (a test aid, the MP test's).
- `vr_rope_push <x> <y> <z>`: every rope's free points given that speed (units/s).
- `vr_rope_iterations` 16 (rule), `vr_rope_hands` 1 (rule), `vr_rope_debug` 0 (prints each rope made),
  `vr_rope_spawn_length` 256, `_thickness` 1.5, `_segments` 0, `_mass` 0, `_stiffness` 1, `_damping` 0.5, `_pinned` 0.

## Tests

- `vr_rope_test` (Debug > Tests > Climbable Ropes > Test the Climbable Ropes; best in vrropes:
  `map vrropes; wait60; vr_rope_test`, `-QuitOn "vr_rope_test: [0-9]+ passed"`): a rope from map keys (10 pieces) and a
  chain placed as the toolgun places one (6 pieces, pinned, 2 thick) made 128 units ahead; vrropes' own counted (7, 3
  chains, 1 pinned); settled (counted frames): the rope hangs straight its length, the chain's ends held; the client's
  copy within half a unit of the server's, the chain drawn as a chain; an explosion throws it, a shot through it pushes
  it, a crate set down on its end and a dog standing in it push it aside; removed, it is gone from the server and the
  client. 12 passed, 0 failed.
- `mp_test.py ropes` (`Misc/quakevr/mp`; dedicated server, A, and B through the proxy): the server hangs a rope and a
  chain (`vr_rope_spawn`); B has both with the server's points and style (bottom within a unit); pushed
  (`vr_rope_push`), B sees both swing (over 20 units); B's copies within a unit of the server's after. 9 passed.
- Checklist scene `ropes` (vrropes, the toolgun given).

## Known limits (P1)

- Nothing climbs it yet (P2/P3).
- The toolgun's Remove and Physgun don't aim at a rope (it has no box to hit); removing the map's or toolgun's ropes:
  the Remove tool's Clear Props doesn't take them either. To come with P2's grab (the hands find the rope by its points).
- Remote clients draw the latest points, no interpolation between messages (at 72 Hz sends it looked smooth over the
  loopback; behind real lag it may step).
