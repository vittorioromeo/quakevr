# Other mods in VR (compatibility mode)

Quake VR's gameplay — weapons in both hands, holsters, throwing, force grab, melee, reloading, grabbing items by
hand — lives in its own QuakeC (`QC/`, compiled to `quakevr/progs.dat`). Other mods (Arcane Dimensions, Copper,
Alkaline, Quoth, the id1 and mission-pack progs, ...) bring their own progs.dat, without any of it. Until now that
meant no VR at all. The engine now runs them in **compatibility mode**.

## Running a mod

```
ironwail.exe -game quakevr -game <mod>
```

The mod's progs.dat and data win; Quake VR's assets (hands, body, weapon offsets, particles, the gadget) stay
available under it. `developer 1` prints "VR: progs without Quake VR's gameplay: compatibility mode" when a map
loads.

## What works

| | Quake VR's progs | A mod's progs (compatibility mode) |
|---|---|---|
| Headset rendering, tracking, hands and fingers, body IK, wrist gadget (from the standard stats), menus | ✅ | ✅ |
| Locomotion by the head, snap/smooth turning, room-scale walking, teleport, roomscale jump | ✅ | ✅ (server-side from each client's VR move) |
| Aiming with the main hand | ✅ | ✅: the hand's angles are `.v_angle` |
| Shots from the gun | ✅ | ✅: the player is moved to the muzzle while `PlayerPostThink` runs its weapon code (`vr_compat_muzzle`); the player's lightning beam starts there too |
| Weapon models in the main hand | ✅ | ✅ (by model name, the same offsets table) |
| Quake VR particles for impacts, blood and explosions | ✅ | ✅ |
| Off-hand weapons, holsters, throwing, force grab, melee, reloading, hand pickups, weapon buttons, haptics | ✅ | ❌ (the off hand is empty; items are picked up by walking over them) |

A dedicated server keeps the plain protocol for other mods, so non-VR clients can join; compatibility mode is for
the player's own (listen) server.

## How it is built

- Two protocol flags: `PRFL_QUAKEVR` (the VR protocol: the VR move block, VR entity data, `svc_quakevr`) and
  `PRFL_QUAKEVR_PROGS` (the progs implement Quake VR's gameplay; they also send beam ids). The first is on for any
  progs on a listen server, the second only with Quake VR's.
- The server keeps each client's latest VR move in the engine (`server::clientMove`): head, hands, muzzles, VR bits,
  teleport target, room-scale move. The engine's VR physics read it, not QC fields. Quake VR's progs still get the
  fields (`.handpos`, `.v_viewangle`, ...).
- `VR_BeforePlayerPostThink`/`VR_AfterPlayerPostThink` (sv_phys.c) move the player so that the mod's shot point
  (`origin + '0 0 16'`, rockets 8 units further) is the muzzle, without relinking it, and move it back afterwards
  (unless the progs moved it). `developer 2` prints it.
- The QC tells a VR player from a flat-screen one (or a bot) by `QVR_VRBITS0_HANDSTRACKED`, set by the engine per
  client, not by server-wide cvars.

## Next stages

Ordered by value for effort. (A) needs nothing from the progs; (A\*) calls standard QuakeC functions by name when
the progs has them; (B) would be an optional QC API the engine calls when a progs defines it.

| # | Feature | Kind | Effort |
|---|---|---|---|
| 1 | Hand pickups and buttons: a gripping hand touching an `FL_ITEM` entity or a `func_button` calls its `.touch` with the player as `other` | A | 1–2 days |
| 2 | Haptics from events (damage, firing, pickups) on the client, instead of QC calls | A | 1 day |
| 3 | Virtual holsters as weapon selection: owned weapons from `STAT_ITEMS`, a grip at a holster sends `impulse N` | A | 2 days |
| 4 | Off-hand trigger: configurable (nothing, next weapon, fire) | A | small |
| 5 | Melee (and headbutt) in C++: the swing detection moves to the engine; a hit calls `T_Damage` (and `SpawnBlood`) by name | A\* | 3 days |
| 6 | Force grab for mods: the object's flight in the client, then its `.touch` at its own place | A | 2–3 days |
| 7 | A QC adapter API (`QVR_API_VERSION` global; hooks such as `QVR_OnMeleeHit`, `QVR_Throw`, `QVR_Holster`) so that a mod can opt in to more | B | 4–6 days |

## Moving Quake VR's own QuakeC into the engine

The same work also shrinks Quake VR's QC. Candidates, from the audit of `QC/`:

- **Grab edge state** (`vr_handgrabutil.qc`: when a grip started or ended): the engine already builds the previous
  bits; expose "started grabbing" as a bit.
- **Force grab** (`vr_wpnforcegrab.qc`, about 450 lines): targeting, flick detection, flight and the catch in C++;
  the QC keeps eligibility (`FL_FORCEGRABBABLE`) and `.handtouch`.
- **Holster hover haptics** (`UpdateHolsterHover`, `.holsterhover`): the client knows the hotspots and the holsters'
  contents already.
- **Melee detection and traces** (`client.qc` `PlayerVRMeleeImpl`, `weapons.qc` `VRDoMeleeAttackTracelineFor`): to
  the engine, with the damage in QC through a hook.
- **Throw kinematics** (`VRThrowGain`, the weight, the back-projected origin, aim assist, the spin cap): a builtin
  returning the throw's velocity.
- **Positional damage geometry** (the per-classname head table): a builtin; the QC keeps the damage factors.
- **One weapon ID table** shared by the QC and the engine (today duplicated in `vr_server.cpp` and
  `vr_twohand.cpp`).

What must stay in QC: the game's rules — damage, ammunition, items and their IDs, monsters, drops.
