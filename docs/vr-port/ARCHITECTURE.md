# Architecture: where things live, how a frame runs, where new code goes

For agents first, humans second. Read this before adding code: it says which subsystem owns a feature, which helper
is the canonical one, and how to test the change. Details live in the linked pages; this page is the map.
Companions: [CODE_STYLE.md](CODE_STYLE.md) (rules and the canonical helper tables), [SOURCE_MAP.md](SOURCE_MAP.md)
(the split files' parts), [TESTING.md](TESTING.md) (the mock headset, test scripts), [BENCHMARKS.md](BENCHMARKS.md),
[IRONWAIL_DIFF.md](IRONWAIL_DIFF.md) (what the port changes in Ironwail's own files).

## 1. The layers

| layer | where | what |
|---|---|---|
| Ironwail engine (C) | `Quake/*.c` | Quake's host, client, server, renderer, VM. VR edits are marked `// QVR` and call into the VR module through `vr_api.h` / `vr_api_render.h` (C linkage, `VR_*` functions) |
| VR module (C++20, Zancle, glm) | `Quake/vr/` (~600 files; `external/` is third party) | everything VR: headset, hands, body, weapons, physics, ragdolls, gore, menus, tools, diagnostics. Namespaces per subsystem (`qvr::hands`, `qvr::progs`, `qvr::box3d`, ...) |
| QuakeC game code | `QC/*.qc` (list and order: `QC/progs.src`; compiled by fteqcc to `quakevr/progs.dat`) | id1's game plus the VR rules: weapons, melee, monsters, campaigns, arena, tests (`*_test.qc`, inside `#ifdef QVR_TESTS`) |
| Third-party QC | `QC/ad/` (Arcane Dimensions), `QC/frikbot/` | kept close to upstream; VR edits marked |
| Data | `quakevr/` (models, sounds, maps, `checklist.txt`, prop/offset files) | shipped assets of our own (never id's) |
| Tools | `Misc/quakevr/` (checks, generators, test scripts), `Misc/release/` (packaging, `run_test_suite.py`), `Misc/trenchbroom/` | Python and shell; the kit (`C:/OHWorkspace/qvr-kit`, outside the repo) wraps build, run, eval and bench |

## 2. Subsystem map (Quake/vr areas)

Seven areas (the refactor inventory's classifier; one owner area per file). The C++ stems drop `vr_` and the
extension; `.cpp`, `.hpp`, `.inc` and `_diag` parts of a stem go together; the QC column is the area's game code.

| area | owns | C++ anchors | QC anchors |
|---|---|---|---|
| **W** hands, weapons, held things, locomotion | tracked hands and their poses, guns (models, offsets, recoil, reload ports), two-handed grips, foe grabs, gadget, toolgun, flashlight, climbing, weight, input | hands, held, view_hands, view_weapon, weapons(.inc), twohand, foegrab, gadget, toolgun(_panel/_brush/_pick), flashlight, climb, ledges, weight, input, carry2h | weapons.qc, client.qc, player.qc, items.qc, vr_carry, vr_grapple, vr_reload, vr_revolver, vr_grenade, vr_weaponutil |
| **S** platform, runtime, QC bridge, menus | OpenXR and the mock backend, the main loop hooks, cvars and config migration, builtins and fields, protocol, memory, jobs, files, profiling, crash, menus | main, backend(_openxr/_mock), xr_runtime, cvars(.inc), builtins(_*.inc), progs, fields.inc, server, client, protocol, mem, jobs, files, profile, crash, menu(_pages_*, *.inc), menuui | vr_cvars(_gen), vr_defs, vr_fields, vr_sys_fields, vr_util, vr_cheats, builtins.qc |
| **B** body, rigs, armed humanoids, motion tools | the player's body and IK, body calibration, monster rigs (rig2, rigdata), armed grunts' arms/legs/pose/armour, Rig Studio, Pose Studio, motion recorder | avatar, body*, bodycal, rig2*, rigdata, handrig, agarms/aglegs/agpose/agarmor, animsmooth, mdldequant, rigstudio*, posestudio, motion* | vr_armedgrunt*, vr_agarmor, vr_armedrepl, vr_enemyguns, vr_ad_enemyguns, vr_motion |
| **R** rendering, fx, audio | stereo views and post, relighting, particles, water, portals, 3D text, spatial audio, voice | stereo, view, particles, relight*, lighting, ao, water, glsl*.h, glutil, brushes, portals, text3d, audio*, voice | vr_mg_lights/particles/fog, vr_highlights |
| **P** ragdoll, physics, props | Box3D world, ragdolls (passive and active), hulls and collision, ropes, debris, crates, props | box3d_*, ragdoll, ragactive, ragstruggle, ragtool, physics, hull, selfcollide, modelcollide, rope, debris, crates, props | vr_knockdown, vr_ragimpact, vr_kdtrig, vr_crates, vr_debris, vr_dummy, vr_walltorch |
| **C** campaigns, monsters, base QC | AD, mission packs, MG/MG3, arena, horde, tutorial; monster changes; loc/pack tables | ad, monstermods, arenacards, loc_/pack_ .inc | vr_mg*/vr_mg3_*, vr_arena*, vr_ad_*, vr_tutorial, id/hipnotic/rogue/honey QC |
| **M** melee, hits, gore, stealth | blows and parries, hit models and reactions, wounds, gore, blood, decals, dents, shock, chainsaw, hot blades, stealth | hitreact, hitmodel, admelee, wounds, gore, blood*, decals, permdecals, dents, shock, chainsaw, hotblade, axestick, stealth | vr_melee(_params/_bash), combat.qc, fight.qc, vr_decap, vr_limbs, vr_burning, vr_stealth, vr_smallgibs |

Finding a feature: grep its cvar in `Quake/vr/vr_cvars.inc` (every VR cvar, grouped by feature with a comment), then
the cvar's readers (`vr_x.value` in C++, `cvarh_vr_x` in QC). Its menu row is in a `vr_menu_*.inc` page; its notes are
in the round file that shipped it (`docs/vr-port/ROUND*.md`) or its own page in `docs/vr-port/`.
Split files (a module over several `.cpp`): [SOURCE_MAP.md](SOURCE_MAP.md); a module's shared state is in its
`_internal.hpp`, its diagnostics and debug commands in `_diag.cpp`.

## 3. The QC <-> C++ bridge

One owner per rule: the QC owns game rules (damage, AI, items, stamina pool); the C++ owns what needs the headset,
physics, rendering or speed. Where both read a number, it lives in a shared header (below), not in two copies.

| channel | C++ side | QC side | rule |
|---|---|---|---|
| **builtins** (QC calls C++) | `vrBuiltins[]` in `vr_builtins.cpp` (the table) with the `PF_*` bodies in `vr_builtins_<group>.inc` (held, hitmodel, melee, nav, ragdoll) | `name = #0;` prototypes, mostly `QC/builtins.qc` | bound by name at load; `Misc/quakevr/check_builtins.py` fails the build on an unbound prototype or a dead entry |
| **entity fields** (C++ reads/writes edicts) | `QVR_FIELD(name)` rows in `vr_fields.inc` -> `progs::fields().name`, `progs::handFields(hand)` | `.float name;` in `vr_sys_fields.qc` / `vr_fields.qc` | add the row; no `ED_FindFieldOffset("...")` for a fixed name |
| **QC functions** (C++ calls QC) | `progs::Bindings` (resolved once a map: `progs::bindings().x`), `progs::callQc/qcFloat/qcText/qcVec`, `progs::findFunction` for rare calls | ordinary QC functions (`VR_Carry_Handtouch`, `VR_RagImpact_Hit`, ...) | never `findFunction("...")` per frame |
| **cvars** | `QVR_CVAR(name, default, flags)` rows in `vr_cvars.inc`; read `vr_x.value` | handles `cvarh_<name>` generated into `QC/vr_cvars_gen.qc` by `Misc/quakevr/gen_vr_cvars_qc.py`; read `cvar_hget(cvarh_x)` | the build fails on a stale generated file; QC `cvar("...")` only in tests |
| **shared constants** | `vr_melee_shared.h`, `vr_weapon_ids.h`, `vr_agpose_shared.h` | `#include "../Quake/vr/<header>"` (vr_melee_params.qc, vr_defs.qc, vr_armedgrunt_defs.qc) | a number both sides use goes in one of these (or a new `*_shared.h`), never two literals |
| **network** | `vr_protocol.hpp`; client writes hand poses and VR input with `VR_WriteMoveExtras` (vr_client.cpp), the server reads them in `VR_ReadMoveExtras` (vr_server.cpp); entity extras `VR_WriteEntityUpdate`, client messages `VR_ParseServerMessage` | fields the server fills from the move (hand pos/rot, ...) | the server's copy of the hands is what game rules use |
| **engine constants** | server.h, world.h | defs.qc | MOVETYPE_*, SOLID_*, ...: same values, both Quake's |

The VM: the server's QC runs in `sv.qcvm`; code outside a server frame that touches edicts or calls QC makes it current
with `progs::ServerVm vm;` (RAII). The player: `progs::localPlayer()` / `progs::playerEdict()` (CODE_STYLE.md says
which). The VM's interpreter loop: [QC_VM.md](QC_VM.md).

## 4. A frame

`_Host_Frame` (Quake/host.c), once per rendered frame:

1. **Input**: SDL events, then `VR_BeginFrame` (vr_main.cpp): restart the backend if asked, `beginFrame` (xrWaitFrame and
   the tracking; the mock backend fakes both), the motion recorder's copy or playback, slow motion's hand filter, the
   runtime menu, then the per-frame texts and tools (`lines::clear`, `text3d::clear`, tips, Rig/Pose Studio, body
   calibration, scenario, ...).
2. **Commands**: `Cbuf_Execute` (console, test scripts, `wait`), `CL_AccumulateCmd`.
3. **Server ticks** (0..n a frame: `host_fixedtick`, `host_netinterval`; a motion take's recorded frames via
   `VR_ServerFrameOverride`; slow motion scales `host_frametime` by `VR_TimeScale`). Each tick: `CL_SendCmd` (with
   `VR_WriteMoveExtras`), then with `sv.qcvm` current `Host_ServerFrame`:
   - `SV_RunClients` (wrapped in `VR_PlayerRunBegin/End`): reads each move (`VR_ReadMoveExtras`), runs the player's
     think with VR's move hooks (sv_user.c: `VR_MoveAngles`, `VR_CarryMoveScale`, `VR_HeavyMoveScale`, ...).
   - `SV_Physics` (sv_phys.c), unless paused (menus, Rig Studio, ragtool holds): QC `StartFrame`, then every edict
     between `VR_PhysicsEntityBegin/End` (slow motion's per-entity clock). The player (`SV_Physics_Client`), in
     order: `VR_ClimbPreThink`, `VR_FoeGrabPreThink`, QC `PlayerPreThink`, `VR_ClientPreMove` (hand and weapon
     touches), `VR_ClientSpecialMove` (teleport, ledge hang, mantle), the movetype's move (`VR_GroundGravity`),
     `VR_ClientRoomscaleMove`, then QC `PlayerPostThink` between `VR_BeforePlayerPostThink` and
     `VR_AfterPlayerPostThink` (vr_physics.cpp).
     Tossed things: `VR_RigidToss`, `VR_PortalToss`. Then `VR_PhysicsFrameEnd` (vr_box3d_step.cpp): the Box3D world
     steps (ragdolls, props, debris), synced back to their edicts.
   - Messages out: `VR_WriteEntityUpdate`, `VR_CalcStats` (sv_main.c).
4. **Client read**: `CL_ReadFromServer` (`VR_ParseServerMessage` for VR messages, `VR_OnClientClearState` on a new map).
5. **Screen**: `SCR_UpdateScreen` -> `VR_RenderView` (vr_stereo.cpp): the window's view, queued crosshair, HUDs and
   blade fx, the env map, both eyes (the view entities from `vr_view*.cpp`: hands, weapons, body, holsters, in the
   order of `Entities::members`), post, submit (xrEndFrame). Test runs that skip drawing (`vr_mock_fast 2`) set the
   view up alone (`VR_HeadlessView`).
6. **Particles, sound**: `CL_RunParticles`, `S_Update` (the mix started on the job pool before the screen).

Per map: `VR_NewMap` (vr_main.cpp) runs the `prewarmSteps` rows in order; client state resets go in
`VR_OnClientClearState`. Threads: the job pool (`vr_jobs.hpp`) for mixing, Box3D steps and per-frame batches; the
server runs on the main thread ([SERVER_THREAD.md](SERVER_THREAD.md) is a shelved design).

## 5. Canonical helpers (the short list)

One implementation each; the full tables with signatures are in CODE_STYLE.md, "Canonical helpers". The build's
`check_canonical.py` (section 7) fails on a new copy of the families marked (g).

| need | use | where |
|---|---|---|
| `vec3_t` <-> `glm::vec3` (g) | `geom::vec`, `geom::store` | vr_geom.hpp |
| normalise, wrap angles, yaw, smoothstep, ease, segment/triangle distance (g) | `geom::safeNormalize`, `wrap180`, `yawDelta`, `yawOf`, `smooth01`, `smoothstep`, `approachExp`, `closestOnSegment`, `segmentDistance`, `closestOnTriangle`, `quakeFromTracking` | vr_geom.hpp |
| angles <-> axes, forward vectors | `held::axesFromAngles` / `anglesFromAxes`; `hands::angleVectors`, `anglesFromVectors` | vr_held.hpp, vr_hands.hpp |
| world traces, contents, liquids | `worldtrace::serverLine`, `move`, `contents`, `clientContents`, `isLiquid` | vr_trace.hpp |
| the server's VM for a scope (g) | `progs::ServerVm vm;` | vr_progs.hpp |
| the player's edict (g) | `progs::localPlayer()` / `progs::playerEdict()` | vr_progs.hpp |
| edicts by number, loops, nearest | `progs::edictAt`, `forEachEdict`, `nearestBy` | vr_progs.hpp |
| entity fields (g: no string lookups) | `progs::fields().x`, `progs::handFields(hand)` | vr_fields.inc, vr_progs.hpp |
| controller buzz (g) | `haptics::pulse(hand, seconds, amplitude, hz)` | vr_backend.hpp |
| cvars for a scope / saved and restored | `cvars::Override`, `cvars::Snapshot`; read `vr_x.value` (g: no `Cvar_VariableValue("vr_...")`) | vr_cvars.hpp |
| random floats (g: no `rnd` wrapper) | `vrand::range(rng, lo, hi)`, `vrand::xorshift*`, `vrand::lcg*`; server determinism: `srvrandom` | vr_random.hpp, vr_srvrandom.hpp |
| strings, quantiles, LE bytes, MDL normals (g) | `strings::startsWith/endsWith/lower/trim/appendf`, `vrstats::quantileSorted`, `bytes::View`, `mdlbuild::nearestNormal` | vr_strings.hpp, vr_stats.hpp, vr_bytes.hpp, vr_mdlbuild.hpp |
| files | `files::readText`, `writeText`, `forLines`, `join` | vr_files.hpp |
| scratch memory, caches | `mem::Scratch`, `mem::Cache` (no function-local statics: build.sh fails on them) | vr_mem.hpp, CODE_STYLE.md |
| client sounds; UI clicks | `audio::playAt`, `audio::playLocal`; `menuui::uiSound(UiSound::...)` | vr_audio.hpp, vr_menuui.hpp |
| GL targets and programs | `gfx::RenderTarget`, `gfx::linkProgram` | vr_glutil.hpp |
| in-world tool panels | `toolpanel::Frame/Grid/buttonUnder/pushUndo/say/textFit` | vr_toolpanel.hpp |
| per-slot cvars | `SlotTable<Key, Slots, Keys>` | vr_slottable.hpp |
| particles, debris, decals, wounds | `particles::` emitters, `debris::add`, `decals::`, `wounds::` | their headers |
| C++ test verdicts | `testkit::Tally`, `testkit::verdict` | vr_testkit.hpp |
| QC: the player (g) | `VR_Player()`, `VR_NextPlayer(p)`, `VR_PlayerAlive()` (never `find(world, classname, "player")`) | QC/vr_util.qc |
| QC: math (g) | `clamp`, `VR_Wrap180`, `VR_Cross`, `VR_Lerp`, `VR_Smooth`, `VR_PointSegmentDistance`, `VR_SegmentDistance(Eps)` | QC/vr_util.qc |
| QC: a hand's field of a pair | `VR_HAND_FIELD(e, hand, mainField, offField)` | QC/vr_util.qc |
| QC: weapon numbers, names, models | the `VR_WEAPONS` table rows (`WeaponIdTo*` read them) | QC/vr_weaponutil.qc |
| QC: test checks and verdicts (g) | `VR_Test_Begin/Check/Verdict`, or the armed grunts' `VR_AGT_*` | QC/vr_testutil.qc, vr_armedgrunt_test.qc |

## 6. Where to put new code

- **Reuse first.** Look the need up in section 5 and CODE_STYLE.md before writing a helper. A file's own wrapper is
  fine only when it bakes in that site's rule, and its body then calls the canonical one.
- **A new general helper** goes into the canonical header of its family (math: vr_geom.hpp; engine access:
  vr_progs.hpp / vr_trace.hpp; strings: vr_strings.hpp; QC: vr_util.qc), with a line in CODE_STYLE.md's tables,
  and (when copies are likely) a family in `Misc/quakevr/check_canonical.py`.
- **A feature** goes in its area's files (section 2). A new module: `vr_<name>.cpp/.hpp` in `qvr::<name>`; its
  diagnostics in `vr_<name>_diag.cpp`; a file nearing 3000 lines is split by its clusters (SOURCE_MAP.md's pattern).
- **Engine hooks**: a `VR_*` function declared in `vr_api.h` (or `vr_api_render.h`), called from the Ironwail file
  with a `// QVR` comment; keep Ironwail edits to the call (IRONWAIL_DIFF.md).
- **Per frame / per map / per client reset**: a call in `VR_BeginFrame`'s list, a `prewarmSteps` row, a call in
  `VR_OnClientClearState`. A new view entity: an `Entities` member (vr_view_internal.hpp).
- **Game rules** in QC unless they need the headset, physics or speed; a number both sides use goes in a shared `.h`.
- **Settings**: a `QVR_CVAR` row in vr_cvars.inc (the QC handle file is regenerated), a menu row in its
  `vr_menu_*.inc` page (rows via the `menu_detail` builders), a `defaultChanges` row (vr_cvars.cpp) when a shipped
  default changes. Renamed or moved menu rows: run `vr_menu_path_check maps/vrcalibration.map` and update
  `Misc/quakevr/make_vrcalibration_map.py`.
- **Debug and test aids**: in the Debug menu pages; tests in QC `*_test.qc` under `#ifdef QVR_TESTS`, or C++
  `_diag.cpp` commands; a numbered test claims its number in `QC/test_numbers.txt`.
- **Tables, not if-chains** (QC struct arrays with initializers; C++ constexpr arrays).
- **No** function-local statics, per-frame allocations, `std::` containers (Zancle), or new dynamic allocation where a
  scratch buffer serves (CODE_STYLE.md).

## 7. Checks the build runs (the kit's build.sh)

| check | fails on |
|---|---|
| QC compile (fteqcc) | errors; the fteqcc self-test |
| `Misc/trenchbroom/fgdgen.py --check` | a QC spawn function missing from quakevr.fgd (prints only) |
| `Misc/quakevr/check_statics.py` | a mutable function-local static in Quake/vr |
| `Misc/quakevr/check_test_numbers.py` | a test number claimed or dispatched twice, or not registered |
| `Misc/quakevr/check_builtins.py` | a `#0` prototype with no builtin, a builtin with no prototype |
| `Misc/quakevr/gen_vr_cvars_qc.py --check` | a stale `QC/vr_cvars_gen.qc` |
| `Misc/quakevr/check_canonical.py` | a NEW copy of a canonical helper (families: `geom-vec`, `geom-math`, `rnd-wrapper`, `strings-copy`, `haptic-direct`, `qcvm-push`, `player-edict`, `cvar-by-name`, `field-by-name`, `qc-find-player`, `qc-math-copy`, `qc-test-check`): a file holding more sites than `Misc/quakevr/canonical_allowlist.txt` allows. `--list <family>` shows the sites; after migrating some, `--tighten` lowers the allowlist; a meant new site is a hand-written allowlist line with its reason |
| `Misc/quakevr/check_qc_precedence.py` | QC that compiles differently under C's operator priorities |
| MSBuild | errors; warnings are listed (keep 0) |

## 8. Tests and benches

- **Headless runs**: the mock headset (`vr_backend_mock.cpp`), fixed game clock (`vr_fixed_frames 1`: every frame
  1/72 s), uncapped frames; the kit's `run.sh <agent> -Script "...;toggleconsole;quit" -QuitOn "<verdict>"`. Every
  test ends on its own verdict line (`<name>: N passed, M failed`); a timeout is a safety net, not an end.
- **Kinds**: QC console tests (`vr_<x>_test N`, a state machine in a `*_test.qc`); C++ console tests and diagnostics
  (`_diag.cpp`, `testkit::Tally`); script suites in `Misc/quakevr/` (ragdoll, climb, foegrab, teleporters, ...); the
  release suite `Misc/release/run_test_suite.py` (its verdict scripts); batched cases (`vr_testbatch.cpp`: many cases
  a game); checklist scenarios (`vr_scenario.cpp`, TESTING.md); the melee canary `eval.sh` (archived takes).
- **Test numbers**: `QC/test_numbers.txt` is the registry (dispatch lines and one line per number or block); claim a
  number there in the same commit as its dispatch branch (TESTING.md, "Test numbers").
- **The full baseline** (coordinator): `qvr-kit/scratch/refactor/run_baseline.py <agent> --jobs 8 --compare
  baseline.tsv` runs every suite, agrunt number, console test, script suite, eval and bench as one pool in
  lanes (`QVR_LANE=k`: each lane its own game folder; cases that read the agent's own folder stay in lane 0), then
  diffs verdicts and counts against `baseline.tsv`. Cases known to vary run to run (its `FLAKY` set) are reported
  apart as "FLAKY CHANGED" and do not fail the compare. `baseline_phase0.tsv` is the refactor's starting point.
- **Benches**: `bench.sh <agent>` (exclusive, ThinLTO build: `build.sh --lto`); scenarios and records in
  BENCHMARKS.md. **A/B method** for a change's cost: build both sides once (exe + progs.dat saved per side), run them
  interleaved (`ABBAABBAAB`, one rep each: `qvr-kit/scratch/refactor/ab_bench.sh`) so drift and thermal state hit
  both alike, then compare medians per scenario (`ab_stats.py`); a single before/after pair is not evidence.
- **Profiling**: `vr_profile` (in-game zones), Tracy builds (TRACY.md), `host_tickstats`.
