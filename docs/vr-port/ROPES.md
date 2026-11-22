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
  units/s. **A jump** (the jump button) while holding one lets go and adds `vr_rope_jump` (200) up.
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

## Climbing (flat-screen)

Flat-screen play (`vr_flat_rope` 1, Options > Flat-Screen; needs `vr_climb` and `vr_rope_climb`; `vr_climb.cpp`
flatRope, flatRopeFrame), the flat ladder's way (`flatLadder`) on a rope:

- **Taking it:** walking forward into a rope (on the ground, not looking down past `vr_flat_rope_look`), or meeting one in
  the air (a jump, a fall, a swing off another rope): a rope within `vr_flat_ladder_reach` (16) of the box's side at the
  shoulders, at least `vr_climb_min_height` over the feet, at most an arm over the shoulders. Both hands take it: the
  main hand half an arm over the shoulders, the off hand 12 units under it. The hang is the headset's rope hang (his
  weight rides it, `climbrope::ride`; the swing he brings to it swings it: walking in at full speed swings it well).
- **The keys:** looking up past `vr_flat_rope_look` (25 degrees), forward climbs up at `vr_flat_ladder_speed` (100;
  looking down, down; back the other way; the move up / down keys too); the hands go hand over hand along it (the lower
  one, once under the shoulders, 12 units over the other; going down the higher, once half an arm over them, 12 under).
  Looking nearer level the move keys pump its swing (the pump's rules above, the view's yaw). Jump lets go keeping the
  swing and `vr_rope_jump` up; down at the floor he lets go standing; at the rope's end over a drop he lets go (falling,
  the swing kept). The rope the body leaves isn't taken again for a second (so a swing off one catches the next).
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
  apart (rope, chain, rope, chain) and a ledge to jump from, a pinned rope, a heavy chain, a rope longer than the drop.

## Commands and settings

- `vr_rope_info`: the server's ropes (`rope_sv:` lines: entity, points, style, bottom, speed, settings) and the client's
  copies (`rope_cl:`), then `rope_info: done`. Debug > Tests > Climbable Ropes > Print the Climbable Ropes.
- `vr_rope_spawn [chain] [length]`: one hung 96 units ahead of the first player (a test aid, the MP test's).
- `vr_rope_push <x> <y> <z>`: every rope's free points given that speed (units/s).
- `vr_rope_climb_test <play>` / `vr_rope_climb_check <what> [<n>]`: the climbing test (below). Debug > Tests > Climbable
  Ropes > Hang a Rope Ahead / Hang a Chain Ahead (`vr_rope_spawn`): one to climb anywhere.
- `vr_rope_climb` 1, `vr_rope_swing_drag` 0.15, `vr_rope_jump` 200 (rules).
- `vr_rope_iterations` 16 (rule), `vr_rope_hands` 1 (rule), `vr_rope_debug` 0 (prints each rope made),
  `vr_rope_spawn_length` 256, `_thickness` 1.5, `_segments` 0, `_mass` 0, `_stiffness` 1, `_damping` 0.5, `_pinned` 0.

## Tests

- `vr_rope_test` (Debug > Tests > Climbable Ropes > Test the Climbable Ropes; best in vrropes:
  `map vrropes; wait60; vr_rope_test`, `-QuitOn "vr_rope_test: [0-9]+ passed"`): a rope from map keys (10 pieces) and a
  chain placed as the toolgun places one (6 pieces, pinned, 2 thick) made 128 units ahead; vrropes' own counted (7, 3
  chains, 1 pinned); settled (counted frames): the rope hangs straight its length, the chain's ends held; the client's
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
  ledge, whose `vr_climb_hand_pitch` 60 points them up).
  The drawn rope is made again whenever the client's time moves (it was once a host frame, sometimes before the time
  moved: a client faster than the server drew it a tick at a time, the hands' jitter).
- Pumped hard on the heavy chain, a hand change can still jump a hand for a frame (the test reports its maxima).
- Sounds (ropes5b, `vr_physsound.cpp` ropeGrab, chainMoves): a hand taking a chain makes the metal hold's grip
  (`vr_physsound_chain` 0.4 loud), and a chain swung, climbed or pushed (its fastest point from `vr_physsound_chain_speed`
  60 u/s) clinks with the light metal knocks, quieter and every 0.6 s near it, louder and every 0.25 s at four times it.
  A rope's grip (ropes6): the soft material's thud (`vr_physsound_grab`), a headset's or flat play's (it was the stone
  slap, silent to a headset). All are Quake VR's
  own recordings (make_physics_sounds.py). A grip's sound plays once in 0.15 s per rope (both hands at once: one).
- Paused (`pause`, single player's menu or console) the ropes stand (ropes5: `climbrope::serverFrame(worldRan)`,
  Host_ServerFrame's SV_Physics); before, only `pause` stopped them.
- Remote clients draw the latest points, no interpolation between messages (at 72 Hz sends it looked smooth over the
  loopback; behind real lag it may step).
