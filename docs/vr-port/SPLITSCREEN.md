# Local splitscreen in flatscreen mode: survey, inventory, options, plan

Phase 0 (agent `split`, 2026-11-20): research and design (sections 1-3, its recommendation kept in 4.1). His
decision and the design he chose (true splitscreen, in process): section 4; the phases: section 6. His request (2026-11-20): splitscreen in
flatscreen mode, up to 4 players (keyboard and mouse plus several controllers), vertical or horizontal split, and the
rest. VR stays as it is: splitscreen is a flat-mode feature.

## 1. Survey: what the other engines have

Sources read (shallow clones in `qvr-kit/downloads/split/`, nothing copied into the repo):

| Engine (head read) | Splitscreen? | How | Licence | What ports to us |
|---|---|---|---|---|
| **Ironwail** (`3fa96fe`, 2026-11-17) | **No.** No seat, splitscreen or second-view code anywhere (only SDL's headers match). | - | GPL-2+ | nothing |
| **QuakeSpasm-Spiked** (`4c0c55f`, 2025-06-22) | **No.** It understands FTE's protocol names (`svc_selectsplitscreen_fte`, `svc_updateseats_fte` in `cl_parse.c`'s table), but `PEXT1_SPLITSCREEN` is commented out in `protocol.h` ("too much hassle for not enough users") and CSQC's `VF_ACTIVESEAT` is a stub that prints "splitscreen not supported". | - | GPL-2+ | nothing (option (b) does not exist) |
| **vkQuake** (`1e573d7`, 2026-11-15) | **No.** | - | GPL-2+ | nothing |
| **DarkPlaces** (`6d754e4`, 2026-11-05) | **No** (no seat or splitscreen code). | - | GPL-2+ | nothing |
| **FTEQW** (`f09ac7c`, 2026-11-02) | **Yes**, up to 4 seats (`MAX_SPLITS 4` in `config_fteqw.h`; 1 in minimal builds). | below | GPL-2+ (compatible with ours) | the design; not the code (QW client architecture, CSQC) |
| Quake (2021 Nightdive/KEX remaster) | Yes, 4 players, controllers join from a menu | closed | proprietary | the UX only (join screen, layouts) |
| Spearmint (ioquake3 fork) | Yes, 4 local players (from memory, not read) | per-player cvars (`2name`...), one window | GPL-3 | the UX only (GPL-3 code can't come into our GPL-2+ tree without relicensing it) |

**How FTE does it** (the only open engine with real splitscreen):

- **One process, one connection, N seats.** `cl_splitscreen N` (0-3 extra seats). The server ties the extra seats'
  `client_t`s to the first one (`controller`/`controlled` chain, `seat` index; `SV_AddSplit`, `server.h:719`): one
  netchan, one packet each way. Moves go up for all seats in one packet; the server addresses seat-specific messages
  with a seat prefix (`svc_selectsplitscreen`) and sends **one world**: the entities are culled against the **union of
  the seats' PVS** (`SV_Snapshot_SetupPVS` loops over the chain, `sv_ents.c:4198`).
- **One client world, N player views.** `cl.playerview[MAX_SPLITS]` (`playerview_t`, `client.h:666`): each seat's view
  entity, angles, stats, punch, damage kick, centre print, view model, prediction state. Entities, particles, temp
  entities and sounds are parsed once.
- **Rendering** (`view.c:2561` `V_RenderView`): entities linked and predicted once, then a loop over seats, each with its
  rectangle (`SCR_VRectForPlayer`: columns and rows grown until the seats fit, preferring 16:10-ish tiles), its own
  refdef, its own status bar drawn in its rectangle (`Sbar_Draw(playerview)`).
- **Input**: buttons are per seat (`kbutton_t.down[MAX_SPLITS]`), commands are routed by a `p1`..`p4` prefix (`p2
  +attack`), devices are given to seats (`in_generic.c`: a device's seat, `cl_forceseat` for the keyboard).
- **Sound**: one mixer with a listener per seat; each sound is spatialised against the **nearest** seat's listener
  (`snd_dma.c:2660`), so a sound plays once.
- **Menus and console**: one, shared; whoever has the keyboard drives them.

What would conflict with ours if we followed FTE's design (section 3, option (a2)): every part of our netcode is keyed by
one player per connection (snapshot rings and acks, the VR player-state block, prediction only of `cl.viewentity`, lag
compensation per shooter, the QVR move message, `clientprefs::ownSlot` for the host's own hands); flat play's hands,
holsters, visor, crosshair, ironsight, lean and crouch are file-scope singletons reading one `keydown[]`; one menu stack
and one `key_dest`; and the renderer's per-frame work shared by the two eyes (shadow maps, env cube, portals, TAA
history) assumes one viewpoint.

## 2. Our engine: what a second local player needs its own copy of

Counted on `next` (`87e0b44c7`) with two throwaway scripts: file-scope mutable variables at namespace scope (a
heuristic: a struct like flat play's `FlatState fs` counts once however many fields it has; vendored code left out),
and regex counts of uses:

- **The whole of it, roughly**: 2,847 file-scope mutable variables in 393 files under `Quake/vr` (680 of them in 94
  files whose names say player, hands, flat, view, input, HUD, prediction...), and about 1,090 in 67 of the engine's C
  files. Plus ~3,500 `QVR_CVAR`s and 316 engine cvars, many of them one player's preferences (handedness, sensitivity,
  the flat looks, holster modes).
- **`cl` and `cls`** (`client_state_t`, `client_static_t`): 3,920 `cl.` uses in 288 files (2,771 in 253 VR files), 675
  `cls.` uses in 143. The ones a seat must have its own of: `cl.viewentity` (316 uses, 86 files), `cl.stats` (440, 86),
  `cl.viewangles` (260, 39), `cl.items`, the punch and damage, the intermission, the move commands.
- **The view**: `view.c` (35: kicks, colour shifts, bob, step easing, idle), `r_refdef.vieworg/viewangles` (404 uses,
  63 files), the flat camera hooks (`VR_FlatViewDrop`, `..Lean`, `..ThirdPersonView`, `..DeathView`, `..FlycamView`).
- **The HUD**: `sbar.c` (27), `gl_screen.c` (56: centre print, crosshair, net HUD, loading), flat play's visor, visor
  crosshair, hit marks, speed FOV, the melee HUD; all drawn at the window's size today.
- **Flat hands and holsters**: `vr_flatplay.cpp` (19, among them `FlatState fs`, the feel, keys, crouch trace, slide
  turn, toolgun keys, orbit), `vr_hands.cpp` 26, `vr_held.cpp` 27, `vr_twohand.cpp` 36, `vr_view_hands.cpp` 24,
  `vr_motion*.cpp` ~100, `vr_flatholster`, `vr_flatsight`, `vr_flattp`, `vr_visor`, `vr_throw`, `vr_climb`, `vr_grip*`,
  `vr_avatar*` (~25); `hands::current()` is one rig.
- **Prediction and the move**: `vr_predict.cpp` 5, `vr_netsnap.cpp` 18, `vr_netmove.cpp` 5, `vr_client.cpp` 14.
- **Input**: `cl_input.c` (10: the `kbutton_t`s), `keys.c` (15: one `keydown[]`, `key_dest`, one set of bindings),
  `in_sdl.c` (53: **one** active game controller, `joy_active_controller`; the mouse), `vr_input.cpp` 24. Flat play's
  own keys (lean, crouch, swap, grab, shove, holster menu, reload, punch) read `keydown[]` by the key their
  `vr_flat_key_*` cvar names: keyboard keys by default, **no controller layout yet**.
- **Menus**: `key_dest` (322 uses, 76 files), `menu.c` (71), the VR settings menus (`vr_menu.cpp` 33): one stack.
- **Sound**: `snd_dma.c` (41), one listener (`listener_*`, 37 uses in 4 files).
- **Effects**: `r_part.c` 11, `cl_tent.c` 10, `vr_particles.cpp` 42, `vr_decals.cpp` 42: the world's, shared, but
  spawned by the client parse.
- **Renderer per view**: `gl_rmain.c` (84), 60 VR files with work done "once a frame" or "for both eyes" (shadow maps,
  env cube, portals, dynamic resolution, TAA history). The spectator camera (`vr_window.cpp`, own `SceneTargets`)
  already draws a second, different viewpoint in the same frame: the model for a seat's view.
- **The server side is already N players**: the multiplayer work made every reader go through the player's slot (MP_PLAN
  B9, B11). One thing stays single: the host's own client is one loopback pair (`net_loop.c`: `loop_client`,
  `loop_server`) and one `progs::hostSlot()` / `clientprefs::ownSlot()` (34 uses).

**Rough total for in-process seats: 700-900 variables to make per seat, or to judge shared, across ~250 files.**

## 3. Options

All options: up to 4 seats; a listen server with `maxplayers` = seats (+ network guests); coop or deathmatch under the
multiplayer rules (MULTIPLAYER.md "Single player only": no bullet time, no save at will, the host's level-start coop
autosave). VR untouched: offered only in flat mode (`-flatscreenmode`).

### (a1) In-process seats, each a full client (the "proper" one, the cheaper variant of it)

One process, one window. Seats 2-4 are extra loopback clients (`net_loop.c` grown to 4 pairs), each a complete client:
its own `cl`, entities, prediction, snapshots, flat hands, HUD, input. Every client-side file-scope variable becomes a
per-seat one through a swap registry (`seat::Local<T> fs;`: the declaration changes, the 3,900 uses don't; the seat
loop swaps the live copies in for each seat's client frame and render). Renderer resources (models, textures,
lightmaps, sound buffers) are shared; per-view caches (TAA history, portals, the env cube's "once for both eyes")
become per seat. Sound: each seat's client starts its sounds, the mixer drops a duplicate start (same entity, channel,
sample and server time) and spatialises each against the nearest seat's listener (FTE's rule).

- Effort: **25-35 worker-days** (swap registry and checks 2; engine client globals 2-3; VR client files 8-12; loopback
  and slots 1; per-seat views and render caches 4-5; per-seat input and pads 2-3; menus, join screen, layouts, HUD 3-4;
  sound 1-2; tests 3-4).
- Risk: **high**. A missed variable is a bleed between seats (one seat's hand or HUD state showing in another's),
  found only by play. A **permanent tax**: every new client-side variable must be declared per seat or shared (a
  `qvr_checks` rule can enforce it, but each new feature pays it). Per-seat cvars (the preferences) need a profile layer.
- Gains: one process (one map load, one copy of the textures, one mixer), exclusive fullscreen, one console.

### (a2) FTE's design: one connection, one client world, N player views

As (a1) but seats share one `cl` world and one connection (union PVS, per-seat messages multiplexed). Cheaper at run
time (effects and entities once), but it rebuilds the per-connection netcode (snapshot acks, VR blocks, prediction,
lag compensation, the move message) for several players per connection. **35-50 worker-days, very high risk.** Not
recommended.

### (b) Port QSS's splitscreen

**Does not exist** (QSS stubbed it out, section 1). FTE's code is GPL-compatible but its client is QuakeWorld-shaped
(`playerview_t`, CSQC, QW prediction): its design is (a2); none of its code drops into ours.

### (c) Seat processes: one game per seat, shown as one screen (recommended)

The host's game (seat 1, keyboard and mouse) hosts a listen server and starts one extra game process per extra seat:
`-flatscreenmode -splitseat k -splitpad <controller> -connect 127.0.0.1:<port> -noconfigwrite` with its window
borderless and placed on its tile of the host's screen (`-windowpos` exists since `13cfa6cb0`, as `mp_local.ps1` tiles
two games today). Each seat process is an ordinary remote client: snapshots, prediction (0 ms over 127.0.0.1),
lag compensation, avatars: the multiplayer path that `mp_test.py suite` already covers. What is new is small and
outside the gameplay code:

- **Seat role**: a seat process takes no keyboard or mouse, opens only its controller (SDL's background-joystick hint,
  the controller picked by its instance; `in_sdl.c` opens one controller today: chosen instead of first), plays only
  its own sounds (those of its view entity and local sounds); the host plays the world's (one copy of each sound;
  heard from seat 1's place: the cost of this option, (a1) does better).
- **Window**: the seats' windows are borderless, without taskbar entries, never take the focus, and follow the host's
  window (moved, minimised, closed with it); the host runs borderless fullscreen (exclusive fullscreen can't show
  other windows over it). A later polish (phase 5) makes it truly one window: the seat processes render hidden (as the
  tests' hidden windows) and hand their images to the host through shared GPU memory (`GL_EXT_memory_object_win32`),
  which draws them into its tiles; exclusive fullscreen and one focus then work.
- **Lifecycle**: seat processes quit with the host; a seat that dies is dropped and the layout redone; level changes
  take everyone along (the multiplayer path).
- **HUD per seat**: each seat draws in its own window, so the status bar, visor and crosshair are laid out for its
  tile already; a seat sets its HUD scale from its tile's height.

- Effort: **8-11 worker-days** (phases below), +3-4 for the one-window phase.
- Risk: **low-medium**: no gameplay code changes; the netcode is the tested remote path. Window focus and z-order
  across processes on Windows need care (prototype first in phase 1).
- **Not native in resources**, said plainly: each seat is a whole game: **2-3 GB of RAM each** (MP_PLAN 2.4), so 4
  players ~8-12 GB, its own copy of the textures on the GPU, its own map load (in parallel), its own console. On screen
  and in play it looks and feels native (one screen, tiles, a join screen, per-seat HUD and controller); without phase 5
  the tiles are separate windows the user never sees as such unless he alt-tabs.
- Free with it: **online both ways**: network guests join a splitscreen host as usual, and a splitscreen group can join
  a remote host (each seat process is just another remote client); a seat's crash costs only that seat.

## 4. Decision and design (2026-11-20)

Phase 0 recommended (c), seat processes. **His decision** (2026-11-20): "I would prefer true splitscreen, it feels like
it's also going to improve the code hygiene by removing the global state. Let's proceed in phases and test each one
thoroughly." So: (a1)'s goal, one process, one window, up to 4 seats, each a full client over its own loopback
connection, **without** (a1)'s swap registry: a player's state really moves out of file scope into his seat.

- **The seat** (`Quake/vr/vr_seat.hpp`, `qvr::seat`): `struct Seat` owns one struct per subsystem (`input` today; the
  view, the HUD, the flat hands, `cl`/`cls`... as their phases land). `seat::at(i)` (0..3), `seat::current()`,
  `seat::inPlay()` (1 until phase 6), `seat::Scope` (makes a seat current for its lifetime, the previous one after:
  the seat loop, the tests). C code reaches its parts through accessors (`CL_SeatInput()`: the current seat's,
  `CL_SeatInputOf(i)`), the engine's C files staying C.
- **Shared, explicitly**: the server and progs, models, textures, lightmaps, sound buffers and the mixer, the
  console, cvars (until the profiles' phase), the window and GL context, worker threads and their pools. A shared
  file-scope variable says so (`// seat-shared: <why>`, the check below).
- **The access rule**: code reaches a seat's state through the seat it runs for. Where the call depth is shallow the
  seat is **passed** (`Seat&`, or a seat index in C); where it is not, the seat loop makes the seat **current** and
  the code reads `seat::current()`. Per subsystem:

| Phase | Subsystem | The seat's struct | Reached by | Why |
|---|---|---|---|---|
| 1 | Command buttons, impulse (`cl_input.c`) | `seatinput_t input` (C) | current (`CL_SeatInput()`, `seat::current().input`) | `+forward`... run from `Cmd_ExecuteString` (no context to pass) and the move is built deep in the host frame |
| 2 | Keys, bindings, devices (`keys.c`, `in_sdl.c`, `vr_input.cpp`'s flat keys) | `SeatKeys` (`keydown[]`, bindings, the seat's devices, mouse/stick accumulators) | **passed** from the SDL event (the device names its seat), then current while its binds run | the event is the shallow end; the binds are commands (as phase 1) |
| 3 | View and HUD (`view.c`, `sbar.c`, `gl_screen.c`'s centre print and crosshair, the visor, hit marks, speed FOV, melee HUD) | `SeatView`, `SeatHud` | **passed** (`Seat&` into `V_RenderView`, `Sbar_Draw`, `SCR_Draw*`: one call per seat and rectangle) | a draw call per seat: shallow; the rectangle is a parameter anyway |
| 4a | Flat play (`vr_flatplay`'s `FlatState`, `vr_flatholster`, `vr_flatsight`, `vr_flattp`, `vr_visor`, the toolgun's keys) | `SeatFlat` | current | ~200 call sites deep in the VR frame |
| 4b | Hands (`vr_hands`, `vr_held`, `vr_twohand`, `vr_view_hands`, `vr_motion*`, `vr_throw`, `vr_climb`, `vr_grip*`, `vr_avatar*`) | `SeatHands` (`hands::current()` becomes the seat's rig) | current | the same depth; `hands::current()` already is the one door |
| 5 | Client state and netcode (`cl`, `cls`, `vr_predict`, `vr_netsnap`, `vr_netmove`, `vr_client`) | `client_state_t`, `client_static_t`, `SeatNet` | current (`cl`/`cls` become the current seat's: one pointer the seat loop sets, the 4,600 uses unchanged in form) | 3,900 `cl.` uses in 288 files: passing is not an option |
| 6 | Loopback grown to 4, the seat loop, own slots (`net_loop.c`, `progs::hostSlot`, `clientprefs::ownSlot`) | the seat's loopback pair and slot | **passed** (seat index) | a handful of call sites |
| 7 | Per-seat render (`r_refdef`, TAA history, portals, env cube, shadow "once a frame", dynamic resolution) | `SeatRender` | **passed** with the view (phase 3) | the spectator camera's `SceneTargets` is the model |
| 8 | Sound listeners (`snd_dma.c`'s `listener_*`) | the seat's listener | **passed** (the mixer loops the seats) | one mixer, nearest listener (FTE's rule), duplicate starts dropped |
| 9 | Menus, console focus (`key_dest`, the menu stack, the VR settings menus) | `SeatMenu` | current | 322 `key_dest` uses |

### 4.1 Phase 0's recommendation (not taken)

Phase 0 recommended (c), seat processes (8-11 days, 2-3 GB of RAM a seat, the gameplay code untouched), over
(a1) (25-35 days, a per-seat rule on all client code from then on). He chose true splitscreen for the code
hygiene: the per-player state leaves file scope.

## 5. The seat_state check

`qvr_checks seat_state` (Tools/qvr_checks/statics.cpp, run by the build with the other checks): a **new mutable
file-scope variable** in client-side code (`Quake/vr` without `external/`, and the engine's own `Quake/*.c`, `*.h`,
`*.cpp` without the vendored lodepng, miniz, strlcat, strlcpy) **fails the build** unless it is a seat's (a member of a
seat struct) or marked `// seat-shared: <why>` on its line or the line above. Today's are in
`Misc/quakevr/seat_state_allowlist.txt` (`path|name`): **4,128 after phase 1**. Each phase moves its group into the
seat or marks it shared, then `seat_state --tighten` drops the gone lines: the list only shrinks. Mutable: not
`constexpr`, not `const` (a pointer only with `const` after its last `*`); cvars (`cvar_t`, `QVR_CVAR`) are left out
(preferences: the profiles' phase), as are functions, `extern`, `typedef`, `using`, templates and class members.
Merging next into the branch: `seat_state` lists next's new ones; each is judged (made a seat's, marked shared, or,
only when it truly is not a player's and the merge is not the place to decide, `--accept`ed with a line in the phase's
notes).

## 6. Phases

**Branch** (his decision, 2026-11-20): the splitscreen work is one long-lived branch, `agent/split` (as `agent/mp`
was): origin/next merged in often, each phase's commits and its full test runs kept, `agent/split` pushed to origin
when a phase is done and tested; it merges into next only when he says so.

**Every phase** changes no behaviour with one seat, proven by the same set **before and after** (on the phase's base
and on its result, same machine): `scenario_check`, the flat suites (flatplay, flatparry, flathands, flatmelee, the
visor and crosshair tests, plus the phase's own: flatinput, flatkeys, flatmove, flatkeybind, flatflycam for input),
the `mp_test.py` plain suite (`suite --base <kit>/bases/mp/qbase --reset-autoexec`), the VR mock suites its code
touches (vr_agrunt, active ragdolls, held/grip), `load_models_test`, Dev and ThinLTO builds at 0 warnings; and, where a
subsystem became the seat's, a test that two seats' copies stay apart (`vr_seat_test`, grown each phase). Each phase
ends with the effort left re-estimated (below).

| # | Phase | Days | Needs his answer |
|---|---|---|---|
| 1 | The seat type, the `seat_state` check, the command buttons and impulse | done (1) | - |
| 2 | Keys, bindings and devices per seat; up to 4 controllers opened, each a seat's; the mouse and stick accumulators | 2-3 | 3 (who is seat 1: the default device-to-seat map) |
| 3 | View and HUD per seat, drawn into a rectangle (one rectangle, the window, until phase 7) | 3-4 | - |
| 4a | Flat play's state (`FlatState`, holsters, sights, third person, visor, toolgun keys) | 3-4 | - |
| 4b | Hands, held, two-hand, grips, motion, throw, climb, avatar | 4-5 | - |
| 5 | `cl`, `cls`, prediction, snapshots, the move | 4-5 | - |
| 6 | The loopback grown to 4, the seat loop (each seat's client frame in its `Scope`), own slots: two seats join a listen server and play (headless test: two seats move apart) | 2-3 | 5 (saving), 6 (drop-in/out: whether a seat may join a running game) |
| 7 | Per-seat render: rectangles, per-view caches; the effects (each seat's client world) | 4-5 | 2 (layouts: what the tiles are) |
| 8 | Sound: a listener per seat, nearest-listener spatialisation, duplicate starts dropped | 1-2 | - |
| 9 | Menus and console per seat (`key_dest`, the stack) | 2 | 4 (menu and pause) |
| 10 | UX: Multiplayer > Splitscreen and the join screen | 2-3 | 3, 4, 6 |
| 11 | UX: layouts (2 side by side or stacked, 3, 4) | 1-2 | 2 |
| 12 | UX: KB/M plus up to 4 controllers: the flat controller layout, per-seat profiles (the cvars a player owns) | 3-4 | 7 (the controller mapping, aim assist) |
| 13 | UX: per-seat HUD scale and seat menus, pause | 1-2 | 4 |
| 14 | Online (network guests with a splitscreen host; a splitscreen group joining a remote host), docs, checklist | 1-2 | 8 (a VR host with flat seats: later or never) |

**Effort left after phase 1: 33-44 worker-days** (phase 0's (a1) was 25-35 with the swap registry; moving the state
for real costs the difference, mostly in phases 4-5).

## 7. Questions for him (the phase that needs each)

1. ~~Seat processes or in-process seats?~~ In-process, true splitscreen (2026-11-20).
2. **Layout defaults** (phases 7, 11): 2 players side by side or stacked? 3 players: one wide tile on top and two below,
   three columns, or a 2x2 grid with the fourth tile showing the map or scores?
3. **Who is seat 1** (phases 2, 10): keyboard and mouse always, or whoever presses first (four controllers, no keyboard)?
4. **Menu and pause** (phases 9, 10, 13): does seat 1's menu pause everyone (single player's habit) or never
   (multiplayer's)? Can the other seats open the full menu or only their seat menu?
5. **Saving** (phase 6): multiplayer rules (level-start autosave only), or save at will when every player is local?
6. **Drop-in/out** (phases 6, 10): may a controller join a running game (2 tiles become 3), and a seat leave?
7. **The flat controller layout** (phase 12): the mapping (fire per hand on the triggers, ironsight/guard, lean,
   crouch, grab, shove, holster menu, swap, reload, punch: more actions than buttons), aim assist or none?
8. **A VR host with flat seats on the monitor** (phase 14): wanted later?

## 8. Phase 1 (2026-11-21): the seat, the check, the command buttons

- `Quake/vr/vr_seat.hpp/.cpp`: `qvr::seat::Seat` (index, `seatinput_t input`), `at`, `current`, `inPlay`, `Scope`;
  `CL_SeatInput()` / `CL_SeatInputOf(i)` for C.
- `client.h`: `seatinput_t` (Ironwail's `in_mlook`... `in_down` buttons and `in_impulse`, 18 globals, now one struct a
  seat). `cl_input.c`: the +commands, `CL_AdjustAngles`, `CL_BaseMove`, `CL_SendMove` act on `CL_SeatInput()`; the
  menu's release (`CL_ReleaseMoveButtons`, `CL_MoveButtonsHeld`) takes offsets into the seat's buttons. Readers moved:
  `host.c` (the config's `+mlook`: seat 0's), `in_sdl.c`, `menu.c`, `vr_client`, `vr_flatholster`, `vr_flycam`,
  `vr_input`, `vr_inputlag`, `vr_teleport`, `vr_weaponstudio`; `vr_engine.hpp`'s button externs gone.
- `qvr_checks seat_state` and its allowlist (section 5); its rules checked on a case file (7 of 7 flagged as expected,
  constants, functions, declarations, cvars and a marked one left alone).
- `vr_seat_test` (no map needed; verdict `seat_test: N passed, M failed`): in seat 1 (a `Scope`) `+forward`, `+attack`
  and `impulse 7` land in seat 1's buttons and none of seat 0's, and the reverse for seat 0's `+back`; `CL_KeyState`
  per seat; the menu's release lets go of the current seat's only; scopes nest and restore.
