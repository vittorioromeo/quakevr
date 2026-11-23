# Climbable ropes and chains

Physical ropes and chains: placed in maps, spawned with the toolgun, swinging and pushed by what meets them, the same on
every client. His spec of 2026-11-21 (ROUND25.md, "ropes"); built in phases:

| Phase | What | State |
|---|---|---|
| P1 | The physical rope and chain: map entities, the toolgun's (with its settings page), disturbance, MP sync, the test map | done |
| P2 | VR: grab with either hand, hand over hand up and down, the legs' IK round the rope, letting go keeps the momentum | done |
| P3 | Flat-screen auto-climb (as the flat ladders), pumping the swing (both modes, to a cap: rope to rope), the climber's pose in MP | pumping done; the rest to do |

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
- **Chains stop shots (ropes8c):** a gun's shot (MOVE_HITMODEL), a melee blow's line (MOVE_HITMODEL_MELEE) and a missile
  (MOVE_MISSILE: nails, rockets) meet a chain's links (world.c SV_Move after the entities: `VR_ChainShotClip`,
  `climbrope::chainClip`, each link a cylinder of thickness / 2 + 1 unit, a missile's box as a sphere): the trace ends
  there with the chain entity as its `trace_ent`. The chain takes damage (QC `VR_Rope_Setup`: DAMAGE_YES, never dealt any:
  `T_DamageImpl`, `VR_Rope_Is`) and is metal (`VR_MaterialOf`, `VR_Rope_IsChain`): bullets and blows spark and tink off
  it, nails spark (spike_touch's `spawn_touchblood` for a rope or a pole: poles' nails now splinter or spark too). It
  still swings (the shot push kicks it). Ropes, bodies, sight lines, the grappling hook and thrown things pass chains;
  so does anything of a player climbing a rope within 24 units of the chain (his own chain in his hands).

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

## Climbing (VR)

A grip at a rope takes it, with either hand (`vr_rope_climb` 1): the drawn hand (or the controller, where the level holds
the drawn hand off) within `vr_climb_touch` and a palm's breadth (4 cm) of its surface, plus Grab Leniency; a rope in
the hand comes before a ledge. At least `vr_climb_min_height` over the feet, as a ledge. The climbing rules are the
ledges' (`vr_climb.cpp`, "Holds on ropes"): the body hangs, a hand pulled down lifts it; hand over hand up (a higher
grip, pulled down), down (a lower grip, pushed up); stamina drains as on any hold. No mantle from a rope.

- **His weight on it:** a climber whose hands are only on one rope rides it: his weight is a bob swinging round the
  rope's top, 24 units below his highest hand; the rope runs straight from its top through his hands (its pieces above
  his lowest hand laid on that line), the rest hangs free below him. The holds are on that line, and their motion
  carries the body (as a moving brush's holds do): he swings with it. A jump onto a rope swings it (his speed as he took
  hold, shared with the rope's by their weights); explosions and shots that reach the pieces he hangs on push his swing
  too (shared with his 80 kg). A body that can't follow the swing (a wall) stops it that way. The swing dies down at
  `vr_rope_swing_drag` (0.15) a second. A pinned rope (both ends held) doesn't swing: it is climbed as a fixed line.
- **Pumping the swing** (P3, VR and flat alike): the stick or WASD while riding a rope pushes the swing its way (the
  view's yaw), `vr_rope_pump` (300) units/s/s at full move, only while it swings that way (not back: held, each swing
  out builds on the last, as a swing pumped), up to `vr_rope_swing_cap` (350) units/s at the hands: rope to rope.
- **Letting go** of everything keeps the rope's speed at the hands, plus the hands' fling (`vr_climb_fling`); up to 1000
  units/s. **A jump** (the jump button) while holding one lets go and adds `vr_rope_jump` (200) up. (ropes10) Off a
  **pole** a jump is a normal jump: `vr_pole_jump` (270) up and `vr_pole_jump_push` (200) level away from it (square out
  from its axis to the body; under a level pole the view's way across it), VR and flat alike (`poleJumpPush`).
- **The legs:** the feet go round the rope under the hands (the left instep 0.75 of the leg under its hip, the right a
  little lower: the rope pinched between them), easing on as the hands take it and off as they let go
  (`vr_avatar_climb.cpp` ropeFoot); no footholds are looked for on a rope. (ropes6) A rope too short for it (its lowest
  point less than 0.6 of a leg under the hip: a short rope taken overhead) leaves the feet hanging; they wrap it fully
  from 0.85 of a leg, eased between, and never take its end as a foothold above where they look.
- **Sounds** (ropes6): a hand taking a rope makes a soft thud (the soft material's impacts, `vr_physsound_grab`), a chain
  the metal grip, a headset's grip and flat play's alike; each pull up (once a grip, when the body has risen 15 cm over
  its hold: not a mere grab, not the swing) a short exertion grunt (`vr_rope_grunt` 0.4, `vr_rope_grunt_sound` 2: the
  jump's, deeper; at most every 0.6 s; Climbing page). Flat play's keys climb steadily: no grunt there.
- **Regrabs barely move it** (ropes6): a body a little short of where the pull took it (`riderBlocked`'s miss) moves the
  bob back with its last place (it had kicked the swing 30-60 units/s at each regrab).
- A hand on a rope and the other on a ledge: the ledge holds him (no ride); the rope's hold follows its points.
- One rider a rope: a second climber on it follows its points (he doesn't swing it).
- Multiplayer: the holds move, so a remote client's hang runs in the server's frame, as on a prop's or a lift's holds
  (no move-by-move prediction while he hangs on a rope; the server's position is sent each frame).

## Poles (ropes7)

`vr_pole` (map; toolgun Spawn > Props > Ropes > Pole): a rope whose every point is held. It stands from its origin (its
top) down to the floor, or `rope_length` down, or to its `target` / `rope_end`; `rope_thickness` (3), `rope_style` 1
metal (else wood). It never moves but with its entity (a mover, the physgun). Climbed as a rope (the same holds, hand
over hand, the legs round it, flat play's walk-in climb); `climbrope::ride` refuses it, so the holds sit on its fixed
points (no swing). Drawn as a straight pole (dark wood or steel, flags `flagPole` / `flagMetal` in QVR_SVC_CLIMBROPE).
Its grip: the wood or metal grab sound (`physsound::RopeMaterial`), a metal one's haptic short and hard (0.04 s, 160 Hz),
a wooden one's duller (0.07 s, 60 Hz); ropes and chains keep theirs. Toolgun: Rope and Pole Settings > More: Pole
Thickness (`vr_rope_spawn_pole_thickness` 3), Metal Pole (`vr_rope_spawn_metal` 0); it stands from the ghost up to the
ceiling or its Length.

**Plan: any thin brush as a pole (not built).** A pole is only a record of two end points, a thickness and a material:
`make()` could take them from a brush entity (func_ or world) whose bounds are long on one axis and under a thickness
limit (say 8 units) across the other two, its axis through the middle of its two short faces, its material from its
texture (debris::materialOf: wood, metal). A map flag (`_qvr_poles 1`) or a key on the brush entity would opt in; world
brushes would need a pass over the BSP's brushes at map load (their hull 0 planes) and an entity-less record (a negative
id, never sent as gone). Nothing else changes: the holds, the legs and the flat climb already work on any pinned line.

**ropes8a (his flat notes 2026-11-23):** a pole is strung as a rope is (`target` / `rope_end`; the toolgun's Span with
Pole: taut, level or slanted): past `vr_rope_monkey_angle` a monkey traverse, else climbed (vrropes: a level metal pole
88 high, south, and a slanted wooden one 27 degrees off upright, south-east). **Solid:** boxes along it (QC
`vr_pole_solid`, `placeSolids`: one upright or level, up to 32 slanted, within its thickness of it): players and
monsters stop at it, the climber on it passes (their owner, `passSolids`: holding a rope within 24 units, or inside a
box until out), shots, nails and blows hit it (takedamage, never dealt any: `T_DamageImpl`) and `VR_MaterialOf` makes
it metal (sparks) or wood (splinters); the navmesh cuts each box out (vr_nav.cpp, 8 units across at least). (ropes9)
**The physics props** (crates, barrels, thrown things) and corpses meet it as the world: a Box3D capsule along it, its
thickness across (vr_box3d_props.cpp `syncPoles`, from `climbrope::poleLines`; the world's category and mask), static
(fast props' continuous collision), kinematic while its entity moves it (a mover, the physgun) until still a second;
Box3D's player and monster bodies are kinematic and never meet it (they keep the Quake boxes). Chains: not (they swing).
**Drawn**
with a per-pixel surface (the tube shader's `Pattern`): wood grain along it, or brushed steel with a joint every 48.
**The hands round it** (VR and flat, `vr_view_hands.cpp` ropeGrip / ropeHeld; ropes too): the hand's grip channel laid
on the rope's axis, out along the palm's normal by the radius less the channel's, then by what of the palm and thumb is
still in it; the fingers wrap a cylinder of its thickness (the grasp's solve, a kept shape per quarter unit).

## Strung ropes and the monkey traverse (ropes7)

A rope between two points: `vr_rope` with `target` or `rope_end` ("x y z"), `rope_slack` its length over the line
between (0.1: 10% longer; with neither slack nor `rope_length`: taut). Toolgun: Rope and Pole Settings > More: Span
(`vr_rope_spawn_span` 0: hung as before), End Rise (`_rise` 0), Slack (`_slack` 0.02): strung from 80 units over the
ghost to Span away from you, its end that much higher.

Mostly upright (the line between its ends within `vr_rope_monkey_angle` 45 degrees of vertical): the rope climb as
before, untouched. Past it (and a pole likewise): a **monkey traverse**. Grabbed, the climber's weight is a bob at his
hands' middle along it (`sMid`, eased to the hands' middle at 60 units/s), the rope laid taut from its top through the
bob to its bottom (`layMonkey`); the bob falls, sways round the line between the ends (gravity, the stick's pump, the
swing's drag) and is held within the rope's reach of both ends (it pulls, never pushes), so a slack rope sags under him
and swings sideways. Its length is as it lay when taken (the Verlet steps stretch it a little), so a grab doesn't
jump. (ropes8b) The hands under it overhand (`climb::ropePalm`: out from it downwards, square to it; `monkeyRopeUp`:
the knuckles up over it, the palm forward and in, the back of the hand towards him), yours (VR, flat) and another
player's copy (his hands' places from `ropePalm`, their turn from his snapshot). VR: the ledge rules (a hand pulled along it moves the body along: hand over hand), letting go drops, jump jumps
off (`vr_rope_jump`), the legs hang (`ropeFoot` and `ropeHips` skip a monkey rope: `clientRopeMonkey`, flag
`flagMonkey`; another player's copy alike, the same code). Flat play: met overhead in the air (a jump up to it), both
hands a step apart, the main hand the view's way; the move keys' part along it goes along at `vr_flat_ladder_speed`,
hand over hand (`flatMonkeyFrame`); the move-down key drops, jump jumps off; no pump. (ropes10) Faced across it (the view
within about 37 degrees of square to it, `flatMonkeyAcross`) the hands keep their sides, the right hand on his right: they
shuffle (the hand on the way's side a step on, then the other up to a step behind it), and hands found crossed (taken
facing along it, then turned) change holds.

## Climbing (flat-screen)

Flat-screen play (`vr_flat_rope` 1, Options > Flat-Screen; needs `vr_climb` and `vr_rope_climb`; `vr_climb.cpp`
flatRope, flatRopeFrame), the flat ladder's way (`flatLadder`) on a rope:

- **Taking it:** walking forward into a rope (on the ground, not looking down past `vr_flat_rope_look`), or meeting one in
  the air (a jump, a fall, a swing off another rope): a rope within `vr_flat_ladder_reach` (16) of the box's side at the
  shoulders, at least `vr_climb_min_height` over the feet, at most an arm over the shoulders. Both hands take it: the
  main hand half an arm over the shoulders, the off hand 12 units under it. The hang is the headset's rope hang (his
  weight rides it, `climbrope::ride`; the swing he brings to it swings it: walking in at full speed swings it well).
- **(ropes9, his correction 2026-11-23) The keys:** looking up or down past `vr_flat_rope_climb_pitch` (30 degrees),
  forward climbs up and back down; nearer level forward and back pump the swing along the view (eased over 5 degrees
  either side of it: `flatRopeKeys`); the strafe keys always pump it (sideways to the view). A pole (rigid) is only
  climbed, at any pitch. `vr_flat_rope_keys 2` is ropes8a's (forward always climbs, whatever the pitch);
  `vr_flat_rope_keys 1` brings back the old keys:
- **The old keys:** looking up past `vr_flat_rope_look` (25 degrees), forward climbs up at `vr_flat_ladder_speed` (100;
  looking down, down; back the other way; the move up / down keys too); the hands go hand over hand along it (the lower
  one, once under the shoulders, 12 units over the other; going down the higher, once half an arm over them, 12 under).
  Looking nearer level the move keys pump its swing (the pump's rules above, the view's yaw). Jump lets go keeping the
  swing and `vr_rope_jump` up; down at the floor he lets go standing; at the rope's end over a drop he lets go (falling,
  the swing kept). The rope the body leaves isn't taken again for a second (so a swing off one catches the next), nor
  (ropes10) in the air until he has stood on the ground (`flatRopeLeftAir`: a fall off a tall pole took it again a second
  in, held there or climbed down at the keys' speed: the slow slide he saw).
- **The body:** under the hands, the rope 10 units ahead of its middle the view's way (turning goes round the rope, at
  most 120 units/s), carried by the holds' motion; the shoulders no higher than the top hand, no lower than an arm under
  the lowest. A body that can't follow the swing (a wall) stops it (`climbrope::riderBlocked`). Hands holding a weapon
  keep it, as on the flat ladder (the other hand is drawn on the rope).

## Multiplayer

The server's ropes go to every client (`QVR_SVC_CLIMBROPE` 55, `Quake/vr/vr_protocol.hpp`): its entity, chain or rope,
thickness, the first point as a coordinate and each next as eighths of a unit from the one before (as the grappling
hook rope's corners). Unreliable, whenever a point moved over a quarter of a unit since the last send, else every half
second; a new client gets every rope with his spawn state; a rope removed is sent reliably as gone. The client draws
each rope between its last two messages' points as the entities are drawn (P3, `shownOf`: CL_LerpPoint's fraction),
so a player hanging on it, drawn between his entity's last two places, stays with it as it swings (the feet round it,
`clientRopeAt`, read the same points); a rope the latest message didn't move stands as sent. A listen server's own
client draws from the same message, through the loopback: one path. Another player's holds come with his avatar
(`avatarHold*`), from his entity's origin as sent (ropes4: they were from his client's origin, which the rope had moved
him from). His places (head, hands) come from his client's origin, which on a rope isn't where the server has him (his
hands read 15-60 units off the server's holds): holding a rope, his copy is moved by his held palms' mean way to the
server's holds (`vr_mpavatar.cpp` ropeShift, at most 48 units, eased over 0.15 s), then each held hand pinned with its
palm's middle on its hold (ropePinHands; a new hold, hand over hand, eased to as yours is). A hand on a rope, yours
or his, has its palm's middle 2.5 cm this side of the rope (`climb::ropeHandOffset`), not `vr_climb_hand_*`'s ledge
offsets (which put it beside the rope). On a rope the trunk turns about the neck so the hips come in to 12 cm from it
(`vr_avatar_climb.cpp` ropeHips, at most 45 degrees), the legs then round it within reach, yours and his. ropepose:
hanging on a rope (`detail::ropeHang`) the trunk hangs off the neck under the eye's level look, as your own view poses it
(another flat player's copy turned it with his look and its eye shift: ~4 units further off the rope), with no crouch
(his copy took a fifth of one: the trunk further back); a foot the rope is out of reach for at its height (a swing's
turn, the rope slanting away under the hands) takes it higher up, rising a tenth of the leg at a time (ropeFoot).
His copy's pelvis now hangs within half a unit of where his own game has his (`mp_ropepose_own`).

## Defaults where the spec is silent (his call; each a setting or key)

- A map rope hangs from its origin, 256 long unless `rope_length`; `target` pins its bottom; pieces about 16 units.
- 5 kg a rope, 20 a chain; stiffness 1 with 16 passes; damping 0.5 a second; thickness 1.5 units.
- The toolgun hangs it from the ceiling above the ghost (not at the ghost): you aim at the floor where it is to hang.
- The ropes don't push the props or monsters back (they are light; the bodies push them).
- P2: a climber's weight hangs on the rope as one bob 24 units under his highest hand (the rope taut through his hands),
  not spread over its pieces; his weight 80 kg (`vr_box3d_player_mass`); the swing's drag 0.15/s; a jump off adds 200
  up; letting go keeps at most 1000 units/s; one rider a rope; a pinned rope doesn't swing; no mantle from a rope; the
  grab's reach is the ledges' (touch + 4 cm + leniency).
- P3 (part): the pump pushes only while the swing goes its way, 300 units/s/s to a cap of 350 units/s at the hands.
- P3, flat-screen (2026-11-22, the author asleep: each a setting or a constant to change): the climb at the flat
  ladder's speed and reach (`vr_flat_ladder_speed`, `_reach`); looking up or down past 25 degrees climbs, nearer level
  the keys pump (`vr_flat_rope_look`), so one key set does both; walking in takes it only looking no further down than
  that (no retake while climbing down off it); the hands 12 units apart, the rope 10 units ahead of the body, the body
  turning round it at 120 units/s at most; the rope let go of not taken again for 1 s; the swing a run brings to it is
  kept (the physics' rule: a jump onto a rope swings it); the drawn hands fling nothing (only the rope's swing).

## Files

- `Quake/vr/vr_climbrope.cpp` / `.hpp`: the server's ropes, the pushes, the messages, the client's drawing, the
  commands. `vr_verlet.cpp` `collide`'s `serverWorld` (the server's traces); `vr_geom.hpp` `segmentsDistance` (shots).
- QC: `QC/vr_ropes.qc` (`vr_rope`, `vr_chain`: the settings only), the fields in `QC/vr_sys_fields.qc` (`rope_*`; engine
  side `vr_fields.inc`).
- The toolgun: `vr_toolgun_packs.inc` (the entries), `vr_toolgun_pick.inc` / `vr_toolgun.cpp` (hung after the spawn),
  `vr_toolgun_panel.cpp` `ropePage`.
- Climbing: `vr_climb.cpp` ("Holds on ropes": ropeHold, followRopes, reportRopes, ropeJump, fallFlung), the rider in
  `vr_climbrope.cpp` (startRider, layRider, ride, riderBlocked), the feet `vr_avatar_climb.cpp` (ropeFoot); the plays
  `Misc/quakevr/climb/rope_plays.py`, `rope_climb_test.sh`.
- Hooks: `vr_server.cpp` (frame end, spawn state), `vr_client.cpp` (parse), `vr_text3d.cpp` (drawn), `vr_builtins.cpp`
  (an edict freed), `vr_builtins_melee.inc` (physicsblast), `vr_builtins_ragdoll.inc` (physicsshot).
- The test map: `quakevr/maps/vrropes` from `Misc/quakevr/make_vrropes_map.py` (`--compile`): four swinging ropes 192
  apart (rope, chain, rope, chain) and a ledge to jump from, a pinned rope, a heavy chain, a rope longer than the drop;
  (ropes7) a wooden and a metal pole (south-west, ceiling to floor), a rope strung level 100 high (far north, 2% slack)
  and a diagonal one (east, 230 down to 90): monkey traverses.

## Commands and settings

- `vr_rope_info`: the server's ropes (`rope_sv:` lines: entity, points, style, bottom, speed, settings) and the client's
  copies (`rope_cl:`), then `rope_info: done`. Debug > Tests > Climbable Ropes > Print the Climbable Ropes.
- `vr_rope_spawn [chain] [length]`: one hung 96 units ahead of the first player (a test aid, the MP test's).
- `vr_rope_push <x> <y> <z>`: every rope's free points given that speed (units/s).
- `vr_rope_climb_test <play>` / `vr_rope_climb_check <what> [<n>]`: the climbing test (below). Debug > Tests > Climbable
  Ropes > Hang a Rope Ahead / Hang a Chain Ahead (`vr_rope_spawn`): one to climb anywhere.
- `vr_rope_climb` 1, `vr_rope_swing_drag` 0.15, `vr_rope_jump` 200, (ropes10) `vr_pole_jump` 270, `vr_pole_jump_push`
  200 (rules); tunables `vr_rope_hand_pitch` 10, `vr_rope_elbow_out` 1 (Tunables > Movement > Other).
- `vr_rope_iterations` 16 (rule), `vr_rope_hands` 1 (rule), `vr_rope_debug` 0 (prints each rope made),
  `vr_rope_spawn_length` 256, `_thickness` 1.5, `_segments` 0, `_mass` 0, `_stiffness` 1, `_damping` 0.5, `_pinned` 0.

## Tests

- `vr_rope_test` (Debug > Tests > Climbable Ropes > Test the Climbable Ropes; best in vrropes:
  `map vrropes; wait60; vr_rope_test`, `-QuitOn "vr_rope_test: [0-9]+ passed"`): a rope from map keys (10 pieces) and a
  chain placed as the toolgun places one (6 pieces, pinned, 2 thick) made 128 units ahead; vrropes' own counted (ropes7:
  11, 3 chains, 5 pinned, 2 poles, 2 monkey traverses); settled (counted frames): the rope hangs straight its length, the chain's ends held; the client's
  copy within half a unit of the server's, the chain drawn as a chain; an explosion throws it, a shot through it pushes
  it, a crate set down on its end and a dog standing in it push it aside; removed, it is gone from the server and the
  client; the toolgun's aim meets the chain (Remove, the physgun); Clear Props removes a rope made later and keeps the
  map's. 16 passed, 0 failed.
- `Misc/quakevr/climb/rope_climb_test.sh <agent>` (`vr_rope_climb_test` with `rope_plays.py`'s mock-hand plays, in
  vrropes; the test hangs a rope half a metre ahead, the plays' `cmd vr_rope_climb_check` lines check at their own
  times): `rope_climb` hand over hand up (37.8 units, at least 30), down (18.9, at least 15), a nudged swing carries the
  body (41 units), let go at the rope's speed at the hands (125 of 125 units/s): 8 passed; `rope_jump` up, nudged, a
  jump off (180 up over the rope's 14, 127 level of its 129): 6 passed. ropes6: `rope_climb` also checks both hands drawn
  gripping it, the knuckles outwards (`grip`), and no pull grunt climbing down or on a mere grab; `rope_calm` four pulls
  up close to the rope: the swing at the hands 2.5 units/s (bound 25; was 61), its free end 8.3 (bound 30), 4 grunts;
  `rope_short` a 425-long rope (its end near the legs; `vr_rope_climb_test <play> [length]`) taken overhead and pulled up:
  the higher ankle 28 units under the pelvis (bound 10; was 13 over it).
- (ropes7) `rope_climb_test.sh`'s `pole_wood` / `pole_metal` (`vr_rope_climb_test <play> 0 wood|metal`: a pole standing
  ahead): up 37.8, the further ankle 2.3 units from it (bound 14), the grip `grab_metal3` / wood, down 18.9, never moved
  (0.000): 9 passed each. `monkey_drop` / `monkey_jump` (`... 0 strung`: a rope strung across him 2 m up, 0.3% slack,
  strung again higher by its settled sag): both hands take it, pulled down (a monkey traverse, the feet 12.1 up, the
  higher ankle 27 under the pelvis), hand over hand to his right (18.7 units), let go (dropped 11.7) or jump off: 6
  passed each. `rope_flat_test.sh <agent> pole`: walked into, up 80.6, down, never moved: 4 passed.
  `rope_flat_test.sh <agent> strung` (a rope strung 110 up along his way, 1% slack): jumped up to (a monkey traverse,
  sagging under him), forward along it 80 units, back 40, dropped with the move-down key.
- (ropes9) `vr_pole_solid_test` then the physics props (2 more, 14 in all): a crate dropped 30 units over the level
  pole's middle rests on it (its bottom at the pole's top), one thrown level at 400 units/s at an upright pole bounces
  back off it (its middle 14 short of the axis, back at 55 units/s).
- (ropes8c) `vr_pole_solid_test` also checks the chains (5 more, 12 in all): a shot's, a blow's and a nail's line across
  each of vrropes' three chains meet it within a unit of its links, metal, taking damage; a player's box, a sight line
  and a plain trace pass; a nail (QC launch_spike) fired at one stops at it with a metal hit.
- (ropes8a) `vr_pole_solid_test` (vrropes; Debug > Tests > Climbable Ropes > Test the Poles' Solidity): 4 poles, 35
  boxes (the slanted one 32), on their poles, a player's box and a shot's line stopped by each, wood 4 / metal 3, walking
  under the level pole free, the navmesh's obstacles rebuilt: 7 passed. `rope_climb_test.sh` `pole_monkey` (a level pole
  across him): 6 passed; `vr_rope_climb_check sink <n>` (rope_climb, pole_wood, pole_metal at 0.3): the drawn hands'
  deepest vertex 0.00 units in (was 1.42 on a pole), on it. `rope_flat_test.sh <agent> strungpole`: 4 passed; the flat
  rope test climbs with forward looking level, down with back looking up, and pumps with the strafe key facing across
  the row (let go passing the next rope); `pole` also checks the hands hanging still (fingertips 0.44 in, bound 0.5).
- (ropes10) `rope_flat_test.sh <agent> polejump`: up a pole 300 units, jump: 259 up, 200 level away from it, a free fall
  (0 frames held back, 1.25 s), not taken again (with the old jump it was taken again 1.00 s in); `slantjump` the same off
  a pole slanted 27 degrees (its upper side); `strungside`: the level pole faced across, strafed right 80 and left 40: 0
  frames with the hands crossed (48 frames, 12 units, the old way); `pole` also checks the arms: the knuckles up 10.0
  degrees, the elbows bowed out of the arms' lines. `rope_climb_test.sh pole_jump` (VR, the grips held through the jump):
  259 up, 200 away, 55 airborne frames, 0 held back, not taken again.
- `mp_test.py ropes` (`Misc/quakevr/mp`; dedicated server, A, and B through the proxy): the server hangs a rope and a
  chain (`vr_rope_spawn`); B has both with the server's points and style (bottom within a unit); pushed
  (`vr_rope_push`), B sees both swing (over 20 units); B's copies within a unit of the server's after. 9 passed.
- `Misc/quakevr/climb/rope_flat_test.sh <agent>` (`vr_rope_flat_test`, flat-screen, in vrropes, climbing stamina off):
  a rope hung near the floor walked into (taken), climbed looking up (the feet up 80, 5 hand changes, both hands drawn),
  climbed down looking down (off it standing, 9 hand changes); rope to rope on the swinging row: set in the air by its
  first rope (taken), the forward key pumps it (355 units/s at the hands in 6.2 s), jump at the swing's forward peak,
  the next rope caught 0.75 s later: 6 passed. Debug > Tests > Climbable Ropes > Test the Flat-Screen Rope Climb.
- `mp_test.py ropeclimb` (in the MP suite since ropes4; `--base` as `ropes`): flat A walks into a rope the server hangs
  on vrropes and climbs it, hangs still (the server's A at rest between two reads), B (VR, proxy) checks A's drawn
  wrists (10) and ankles (12) against the rope as B has it (`vr_mp_avatars`' `mp_ropepose`, with each held wrist from
  the server's hold, each hold, the pelvis, head and origin from the rope), then swinging (6 more each), then A jumps
  off. ropes4: hanging 1.6-3.3 and 1.8-7.8, swinging 10.5-12.5 and 6.4-14.1 (3 runs; were 17-35 and 27-46).
  ropepose: hanging still, B's copy's pelvis off the rope within 4 of A's own (A's `mp_ropepose_own`: his own body as
  his game draws it; the neck too); the swing's pelvis maxima noted. 10 runs 12/12: pelvis 12.4-16.9 against A's
  12.5-16.7 (0.4 at most apart; was 6), swinging ankles 9.8-13.2 (bound 18; were 13.9-21.7). `MP_ROPECLIMB_SHOTS=1`:
  B floats beside A and screenshots him hanging and swinging (not in the suite).
- `Misc/quakevr/climb/rope_hands_test.sh <agent>` (ropes5; `vr_rope_hands_test`, flat-screen, vrropes, the client at
  144 Hz against the server's 72): the row's rope, its chain and the heavy chain, each taken in the air, hanging 2 s then
  pumped 3 s; each drawn frame each hand's palm and arm (palm less shoulder) second difference and the palm's middle off
  the rope as drawn. Hanging: jitter at most 1 unit (0.27 / 0.27 / 0.54), off at most 1 (0.47 / 0.49 / 0.49); swinging:
  means checked (0.06 jitter, 0.2 off; were 1.9 and 5.5, hanging 4.8 off). 6 passed. A screenshot of each grip.
- `Misc/quakevr/climb/rope_pause_test.sh <agent>` (ropes5; `vr_rope_pause_test`): the map's ropes pushed swinging;
  `pause`, then single player's menu: their points (the server's and as drawn) still over a second (0.000), swinging on
  after each. 4 passed.
- `Misc/quakevr/climb/rope_sound_test.sh <agent>` (ropes5b; `vr_rope_sound_test`): a rope and a chain made ahead; still:
  quiet; a grip on each (the rope's soft_m thud, the chain's grab_metal; ropes6: a headset's and flat play's alike),
  again (a second at once on the chain held back); both pushed swinging (the chain's metal_l clinks: 5 in 2 s; the rope
  quiet). 6 passed.
- (ropes8b) `rope_flat_test.sh` hanging still also checks the hip holsters stay on the hips (35.4 cm from the hip
  joints, standing 35.5; were 42.4, the guns turned out with the thighs); `strung` / `strungpole` the hands overhand
  under it (knuckles up 0.97, palms 0.8 under the axis, the body's arms reaching them: 0.0 cm past); `chain` a chain
  climbed (1 clink) and held still 2 s (0; were 4). `vr_rope_climb_check monkeygrip <n>` the same hands check in VR
  (`monkey_drop`, `pole_monkey`: 7 passed). `vr_rope_flat_test <mode> shots`: outside screenshots while hanging.
  `vr_rope_sound_test`'s chain is shoved after the grips' gap and clinks as knocked (1 to 9), no longer on and on.
- Checklist scene `ropes` (vrropes, the toolgun given).

## Known limits (P1)

- The body on a rope hangs under the eye (the neck where the view has it), so at a swing's turn, the rope slanting
  under the hands, the pelvis is 15-22 units off it (yours and another's alike, ropepose); the feet then take the rope
  higher up, the knees bent, rather than straight down.
- The toolgun's Remove and Physgun aim at a rope by its pieces (P2; `climbrope::pick`); the physgun moves its top. The
  aimed rope isn't outlined (no model to glow). Clear Props (`vr_scene_clean props`) takes the ropes made after the
  map's first 1.5 s (QC VR_Scene_IsRuntimeProp), as the props.
- The hands on a rope (ropes5): drawn on the rope as drawn this frame (`climb::ropePalm`: the drawn curve's point as far
  from its top as the server's hold, out by 2.5 cm or 0.4 of a thick chain's thickness), yours and another player's;
  gripping it (`vr_rope_hand_grip` 2, the default since ropes6: the rope through the fist, the knuckles outwards, the
  right hand's to the right, the left's to the left, in VR and flat-screen play; 1 flat-screen play only; 0 as on a
  ledge, whose `vr_climb_hand_pitch` 60 points them up). (ropes10) The fist pitched up `vr_rope_hand_pitch` (10
  degrees, tunable) off square to the rope, towards the world's up (`ropeTurn`; a monkey traverse's knuckles, already
  up, stay): yours and, through his snapshot, another player's. The arms on a rope or pole (`ropeHang`) have their
  elbows out at the sides, a little down and back (`vr_rope_elbow_out` 1, tunable: that share of the elbow's pole, the
  wrist's swing in eased off as much; vr_avatar.cpp solveArm), yours and another player's copy: on a pole, flat,
  bowed in 10 cm off the arm's line before, 0.1 out now.
  The drawn rope is made again whenever the client's time moves (it was once a host frame, sometimes before the time
  moved: a client faster than the server drew it a tick at a time, the hands' jitter).
- Pumped hard on the heavy chain, a hand change can still jump a hand for a frame (the test reports its maxima).
- Sounds (ropes5b, `vr_physsound.cpp` ropeGrab, chainMoves): a hand taking a chain makes the metal hold's grip
  (`vr_physsound_chain` 0.4 loud), and a chain swung, climbed or pushed (its fastest point from `vr_physsound_chain_speed`
  60 u/s) clinks with the light metal knocks, quieter and every 0.6 s near it, louder and every 0.25 s at four times it.
  (ropes8b, his note: it clinked on and on held still) Now only as its links knock (`Rope::jostle`, step): a point's
  move against its neighbours' mean (a swing moves them alike), a hold taken along it (climbing: 90 u/s at most), a
  kick (shot, explosion); ridden, the free end under his hands is left out (it trails his swing). From
  `vr_physsound_chain_knock` (30 u/s), louder and more often to four times it; `vr_physsound_chain_speed` still gates.
  A swing pumped on a chain is quiet (only climbing and knocks clink). `vr_rope_debug 2` prints `chainjostle` lines.
  A rope's grip (ropes6): the soft material's thud (`vr_physsound_grab`), a headset's or flat play's (it was the stone
  slap, silent to a headset). All are Quake VR's
  own recordings (make_physics_sounds.py). A grip's sound plays once in 0.15 s per rope (both hands at once: one).
- Paused (`pause`, single player's menu or console) the ropes stand (ropes5: `climbrope::serverFrame(worldRan)`,
  Host_ServerFrame's SV_Physics); before, only `pause` stopped them.
- Remote clients draw the latest points, no interpolation between messages (at 72 Hz sends it looked smooth over the
  loopback; behind real lag it may step).
