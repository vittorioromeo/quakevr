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
  `seat::inPlay()` (the seats playing: seat 0 always, the others joining with a pad since phase 6), `seat::Scope` (makes a seat current for its lifetime, the previous one after:
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
| 3 | View and HUD (`view.c`, `sbar.c`, `gl_screen.c`'s centre print and crosshair, the visor, hit marks, speed FOV, melee HUD) | `seatview_t view`, `seathud_t hud` (C); the VR files' parts (`seat::part<T>`) | **passed** inside the draws, read once at their entries (`V_SetupView`, `V_RenderView`, `SCR_UpdateScreen`), which take the seat with phase 7's loop; messages and commands: current (section 10) | a draw call per seat: shallow; the rectangle is a parameter anyway |
| 4a | Flat play (`vr_flatplay`'s `FlatState`, `vr_flatholster`, `vr_flatsight`, `vr_flattp`, `vr_visor`, the toolgun's keys) | `SeatFlat` and the files' parts (`seat::part<T>`, section 11) | current | ~1,300 uses deep in the VR frame |
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
when a phase is done and tested; it merges into next only when he says so: once the join screen works and he has
tried it (2026-11-21).

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
| 2 | Keys, bindings and devices per seat; up to 4 controllers opened, each a seat's; the mouse and stick accumulators | done (1) | ~~3~~ answered: seat 1 is always the keyboard and mouse |
| 3 | View and HUD per seat, drawn into a rectangle (one rectangle, the window, until phase 7) | done (1) | - |
| 4a | Flat play's state (`FlatState`, holsters, sights, third person, toolgun keys; the visor and the lean went in phase 3) | done (1) | - |
| 4b | Hands, held, two-hand, grips, throw, the toolgun; the motion recorder marked the headset's (the drawn body and climbing's pins: phase 7) | done (1) | - |
| 5 | `cl`, `cls`, the entity tables, prediction, snapshots, the move, the client's VR net state | done (1) | - |
| 6 | The loopback grown to 4, the seat loop (each seat's client frame in its `Scope`), own slots: two seats join a listen server and play (headless test: two seats move apart) | done (1) | ~~5, 6~~ answered: autosave only; drop-in/out (a seat joins or leaves a running game) |
| 7 | Per-seat render: rectangles, per-view caches; the effects (each seat's client world); the drawn body (`vr_avatar*`) and climbing's client pins (4b's leftovers); sounds per seat listener (phase 8's, brought in) | done (3: tiles and layouts; bodies, pins, effects; sounds, the effects' spawners, the visor) | ~~2~~ answered: the tiles follow the chosen layout (phase 11) |
| 8 | The shared effects' other memories fed once (decals and shells from Quake VR's messages, gore's state), the static sounds and underwater muffling per seat, the shadow and portal caches checked with several views (flat TAA per tile moved to phase 11) | done (1; section 18) | - |
| 9 | Menus and console per seat (`key_dest` as each seat sees it, `Key_SeatDest`): seat 1 the full menu and the console, seats 2-4 their seat menu in their tile, each pad its own seat's; the menu pauses coop (`scr_splitmenu`, `scr_splitpause`) | done (1; section 19) | ~~4~~ answered: options; coop's default: the menu pauses, only seat 1's is the full menu |
| 10 | **Next.** UX: Multiplayer > Splitscreen and the join screen (**the phase he tries before `agent/split` merges into next**) | 2-3 | ~~3, 4, 6~~ answered (drop-in/out: the join screen also joins a running game) |
| 11 | UX: layouts (2 side by side or stacked, 3, 4): all switchable, the default from the monitor's aspect; flat TAA per tile (a history a seat, sized as its tile: phase 8's leftover) | 2-3 | ~~2~~ answered: customizable; ultrawide side by side; 3: one wide on top, two below |
| 12 | UX: KB/M plus up to 4 controllers: the flat controller layout, per-seat profiles (the cvars a player owns) | 3-4 | ~~7~~ answered: a default controller layout; aim assist, off by default |
| 13 | UX: per-seat HUD scale and seat menus, pause | 1-2 | ~~4~~ answered (pause and menu options) |
| 14 | Online (network guests with a splitscreen host; a splitscreen group joining a remote host), docs, checklist | 1-2 | ~~8~~ answered: a VR host with flat seats, yes (the last part) |

**Effort left after phase 1: 33-44 worker-days** (phase 0's (a1) was 25-35 with the swap registry; moving the state
for real costs the difference, mostly in phases 4-5). **After phase 2: 31-41 worker-days** (phase 2 took one; phase
10's join screen is a little cheaper: the pads, their seats and the per-seat command buffer exist). **After phase 3:
28-38 worker-days** (phase 3 took one; 4a lost the visor and the lean to it, and has the seat's parts to follow;
phase 7 starts from a seat's window area that `SCR_CalcRefdef` already lays out in). **After phase 4a: 26-35
worker-days** (4a took one of its three: the seat's parts made it mechanical; 4b is unchanged, its hands state being
larger and shared with the headset's code paths). **After phase 4b: 22-31 worker-days** (4b took one of its four to
five: the seat's parts again; the drawn body (`vr_avatar*`) and climbing's client pins, left, go with phase 7's per-seat
render: about a day more there). **After phase 5: 18-27 worker-days** (5 took one of its four to five: one pointer
for `cl`/`cls` made the 4,600 uses mechanical; the other players' copies (`vr_mpavatar`) and the drawn effects'
memories, left shared, add half a day to phase 7; the demo stays seat 0's: phase 6 or later says whether a seat may
record). His answers (section 7, 2026-11-21) add about two days: every layout and the pause and menu rules made
options (phases 11, 13), aim assist (12), drop-in/out on the join screen (10): **20-29 worker-days left**, the VR host
with flat seats (14) last and not yet costed. **After phase 6: 18-26 worker-days** (6 took one of its two to three: the
seats' client state of phases 1-5 made the seat loop a loop; phase 7 gains half a day: the headless seats' gates, section 14,
become each seat's view, sound and effects). **After phase 7's first part: 16-24 worker-days** (it took one: each seat
drawn in its tile by the code that draws the window; left of 7: each seat's particles and lights, the drawn body and
climbing's pins per seat, other players' copies posed once per frame, the visor per seat; section 15). **After phase
7: 13-20 worker-days** (parts 2 and 3 took one each of the 3-4 left; part 3 took phase 8's listeners, so phase 8 is now
what the shared memories left: section 17). **After phase 8: 12-19 worker-days** (8 took one; flat TAA per tile, half a
day to a day, moved to phase 11 with the tiles' sizes; next: phase 9, the menus per seat; then phase 10's join screen,
which he tries before the merge into next). **After phase 9: 11-17 worker-days** (9 took one of its two: one question
per key, "where do this seat's keys go", answered in one place; phase 13's pause and seat-menu options exist as cvars,
leaving it the per-seat HUD scale and the options' menu rows; next: phase 10's join screen).

## 7. Questions for him (the phase that needs each)

1. ~~Seat processes or in-process seats?~~ In-process, true splitscreen (2026-11-20).
2. ~~**Layout defaults** (phases 7, 11)~~: **customizable, every layout switchable**; the default picked from the
   monitor's aspect ratio (an ultrawide: always side by side); 3 players: one wide tile on top and two below by default
   (2026-11-21).
3. ~~**Who is seat 1** (phases 2, 10)~~: the keyboard and mouse, always; controllers join as seats 2-4 (2026-11-21).
4. ~~**Menu and pause** (phases 9, 10, 13)~~: **customizable, each an option**; by default in coop opening the menu
   pauses the game, and only seat 1 opens the full menu (the other seats their seat menu) (2026-11-21).
5. ~~**Saving** (phase 6)~~: multiplayer's rules, the level-start **autosave only** (2026-11-21).
6. ~~**Drop-in/out** (phases 6, 10)~~: **yes**, a controller may join a running game and a seat may leave (2026-11-21).
7. ~~**The flat controller layout** (phase 12)~~: **a default flat-play controller layout: yes; aim assist: yes, off
   by default** (2026-11-21). The mapping itself is the phase's to propose.
8. ~~**A VR host with flat seats on the monitor** (phase 14)~~: **yes, last** (2026-11-21).

All eight answered. **Merging into next** (2026-11-21): once the join screen works (phase 10) and he has tried it;
until then `agent/split` keeps merging origin/next in (each phase does).

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
- **Before and after** (the base `87e0b44c7` and phase 1, same machine, release builds; Dev and ThinLTO 0 warnings,
  QC 0 warnings): scenario_check PASS / PASS; flatplay 31 / 31; flatparry 43 / 43; flathands 76 / 76; flatmelee 12 /
  12; visor 37 / 37; flatxhair 31 / 31; flatinput 19 / 19; flatkeys 37 / 37; flatmove 10 / 10; flatkeybind 8 / 8;
  flatflycam 3 / 3; load_models 4 / 4; held/grip (held_over_props, grip_gap, grip_state) 3 / 3; active ragdolls: 2
  FAIL rows before (ogre crawl, hknight decap), none after (code untouched: the physics' run-to-run spread); vr_agrunt
  not run (no code of it touched); `vr_seat_test` 16 / 16.
- **The MP plain suite**: the same result on both: the first 18 scenarios clean (netsafe to campaign), then from
  `mp_campaign_listen` on most fail at their start ("server exited (code 0) waiting for the server on e1m1", a NET
  SAFETY refusal of the room service in the server's log). An order effect already on the base, not phase 1's: four
  of them alone (`--only mp_flow,mp_moves,mp_predict,mp_lagcomp`) pass on both exes (17, 20, 31, 17 checks). To look
  into separately (BACKLOG).
- **Effort left**: 33-44 worker-days (section 6).

## 9. Phase 2 (2026-11-21): keys, bindings, devices

His answer to question 3 (2026-11-21): seat 1 is always the keyboard and mouse; controllers join as seats 2-4.
(Below, seats are numbered from 0 as in the code: seat 0 is his "seat 1".)

- **The seat's keys** (`seatkeys_t`, keys.h; a `Seat`'s `keys`): the keys held and the bindings (Ironwail's `keydown[]`
  and `keybindings[]`), the gyro and alt-modifier buttons (`+gyroaction`, `+altmodifier`), the mouse's motion since
  the last move. C reaches the current seat's through `Key_Seat()`, any seat's through `Key_SeatOf(i)`; the ~80
  readers (console, menus, demo keys, flat play's keys, the VR menus) read the current seat's. `bind`, `unbind`,
  `unbindall`, `bindlist` act on the current seat's; the config writes seat 0's (the other seats' are not saved: the
  profiles' phase, 12). `Key_ClearStates` lets go of every seat's keys, each as its own seat.
- **The command buffer runs text as the seat that queued it** (cmd.c). A pad's bind runs later than its key event (the
  buffer runs in the host frame): where the queueing seat changes, the buffer gets a marker line `_seat <n>`;
  `Cbuf_Execute` runs each line as the seat of the last marker before it and makes the caller's seat current again
  after; an insert from another seat is wrapped in markers; `Cbuf_TailToFront` (vr_mock_key's) keeps them right.
  **One seat: no marker is ever written: the buffer is byte for byte as before.** A bind with `wait` in it carries its
  seat over the frame (the head's seat is kept).
- **Devices** (in_sdl.c): up to four game controllers open at once (`joypad_t joy_pads[4]`: Ironwail's
  `joy_active_*` globals, the axes, button timers, gyro, gyro centring and flick stick are now each pad's). Pad 0 is
  the controller Ironwail would use (`joy_device`, the Options menu's gamepad, `IN_UseNextGamepad`: chosen exactly as
  before); pads 1-3 are the others plugged in (hot-plug opens and closes them). **Which seat a pad drives**
  (`IN_PadSeat`, the one place): with n seats in play, pads 0..n-2 drive seats 1..n-1 and pad n-1 drives seat 0 (the
  keyboard player's own pad); the rest none. **With one seat: pad 0 drives seat 0, as it always did, and the other
  pads are open but drive nothing.** The keyboard and mouse events make seat 0 current (`IN_SendKeyEvents`); each pad
  is polled with its seat current (`IN_Commands`), so its key events and binds are that seat's; a pad whose seat
  changes lets go of what it held there first. The move reads the current seat's pad (`IN_JoyMove`, `IN_GyroMove`) and
  mouse motion (`IN_MouseMove`; the mouse's motion always goes to seat 0's). The menus' gamepad (name, glyphs, rumble,
  gyro calibration) stays pad 0's: one menu until phase 9. A controller seat with no bindings gets a copy of seat 0's
  when its pad first drives it.
- **Tools**: `seat <1-4> <command>` runs a command as that seat (`seat 2 bind YBUTTON +jump`); `in_pads` (Debug >
  Flatscreen Play > Splitscreen: Gamepads and Seats) lists the pads and the seat each drives.
- **`vr_seat_test`** (no map; verdict `seat_test: N passed, M failed`, 35 checks): phase 1's 16, plus bindings per seat
  (through `seat`, a copy of seat 0's is the seat's own strings, unbind and unbindall per seat); with two seats in
  play two mock pads (polled by `IN_Commands` as real ones): pad 0's button is seat 1's key only, pad 1's seat 0's,
  the keyboard's key (a real SDL event, handled while seat 1 is current) seat 0's only; pad 0's stick moves seat 1
  only, the mouse's motion stays seat 0's; back to one seat pad 0 lets go in seat 1 and drives seat 0, pad 1 lets go;
  the queued binds then run as the seats that pressed them, in order, and the next command as seat 0 after a queue
  ending in seat 1's. (Its first run caught a real bug: an insert holding markers of its own did not restore the
  seat after it.)
- **`seat_state` allowlist: 4,130 -> 4,099** (31 gone: keys.c's `keydown`, `keybindings` and in_sdl.c's 21 pad,
  gyro, flick and mouse globals moved into the seat or the pads; in_sdl.c's 6 others (the window's focus and text
  mode, the mouse's presence, the calibration's two, the menus' last device) and keys.c's two init tables marked
  shared; the new pad table and the command buffer's two seats are marked shared).
  Merging next brought 4 newcomers: navfollow's server state marked shared; its client state and vr_mptest's `heard`
  accepted (phases 4a/5 judge them).
- **Before and after**: see ROUND25.md "split2" (the same set as phase 1, on the merged base `12c955bd4` and on phase 2).
- **Effort left: 31-41 worker-days** (section 6).

## 10. Phase 3 (2026-11-21): the view and the HUD

(Seats numbered from 0 as in the code.)

- **The seat's view** (`seatview_t`, client.h; a `Seat`'s `view`; C: `V_SeatView()`, `V_SeatViewOf(i)`): view.c's
  `v_punchangles`, the hit's kick (`v_dmg_time/roll/pitch`), `v_blend`, the `v_cshift` contents tint (`cshift_empty`:
  a fresh seat's is Quake's 130 80 50, 0; the demo rewind keeps the current seat's), and the two function statics of
  the view's smoothing (`CalcGunAngle`'s old yaw and pitch, `V_CalcRefdef`'s stair step `oldz`); the seat's **window
  area** (`rect`) and its 3D view's part of it (`viewrect`, Ironwail's `scr_vrect`). Readers moved: cl_main.c,
  cl_parse.c, cl_demo.c, gl_rmain.c, gl_texmgr.c, gl_draw.c, vr_ad, vr_comfortfade. `cshift_water/slime/lava` are
  `const` now (they never changed).
- **The seat's HUD** (`seathud_t`; a `Seat`'s `hud`; C: `SCR_SeatHud()`, `SCR_SeatHudOf(i)`): the centre print (text,
  start, time left, lines, columns, erase counts) and `+showscores` (`sb_showscores`). A level's start and the loading
  plaque clear every seat's centre print.
- **The access rule, as built**: the draws take the seat's state as a parameter inside the files (`V_CalcRefdef`,
  `V_CalcViewRoll`, `CalcGunAngle`, `V_PolyBlend`, the centre print's draw chain from `SCR_UpdateScreen`), read once at
  the entry (`V_SetupView`, `V_RenderView`, `SCR_UpdateScreen`) from the current seat. The entries themselves are not
  given a seat yet: the VR render calls `V_RenderView` from deep in its eye loop and the stereo set-up, so phase 7's
  per-seat render (which owns the rectangles' loop) passes the seat there or makes it current around each view.
  Messages and commands (a hit's kick, `v_cshift`, a centre print, `+showscores`) change the current seat's.
- **Layouts: not decided, nothing committed** (question 2). `SCR_SeatRect(seat, &rect)` is the one place a seat's area
  comes from: today every seat's is the whole window. `SCR_CalcRefdef` lays the 3D view and the status bar's room out
  inside the current seat's area (`rect` in place of `glwidth`/`glheight`; with the whole window, the same numbers).
  The FOV's aspect (`vid.width/height`) and `sb_lines` stay the window's: phase 7's, with the rectangles.
- **The VR files' view and HUD state: the seat's parts.** A file-private state struct stays private to its file; the
  seat owns its instance (`seat::part<T>(seat, Part::X)`: made on first use, one slot a subsystem in `seat::Part`), the
  file reading the current seat's through a one-line accessor: the flat visor (`vr_visor`'s sway, effects, centre-print
  popup, panels), the flat crosshair's shot line and marks (`vr_flatxhair`), the hit marks (`vr_flathit`), the speed
  FOV (`vr_flatspeedfov`), the melee HUD's parry glow and embers (`vr_meleehud`), the death view (`vr_deathview`), the
  flat camera's lean, slide view and last seen view (`vr_flatmove`; its frame-driven tests stay shared), the net HUD
  (`vr_nethud`) and the flat crosshair's queued frame (`vr_crosshair`). A new subsystem's per-seat state follows the
  same pattern (phases 4a, 4b).
- **Marked shared**: the window's (`glheight`, the loading plaque and dialog, the turtle and net pictures, the HUD style
  cvar's), the status bar's pictures, redraw count and sort scratch, the crosshair character (a cvar's), the visor's GL
  passes and glass grid (phase 7's per-view caches), the tests' own state machines. Left for their phases: the console
  (`scr_con_current`, notify: phase 9), `sb_lines` (phase 7), `r_refdef` (phase 7), `cl.punchangle`, `cl.cshifts`,
  the pitch drift (`cl`: phase 5); the flat hands, holsters, sights, third person (4a, 4b).
- **`vr_seat_test`** (no map; verdict `seat_test: N passed, M failed`, 48 checks): phases 1 and 2's 35, plus 13: the
  accessors (each seat its own, seat 0's current); a fresh seat's contents tint; as seat 1 a centre print (text, lines,
  time), its clear, `+showscores`, `v_cshift`, a hit on the visor, `+flatleanright` land in seat 1's alone, and seat 0's
  `+flatleanleft` in seat 0's alone; `V_ResetEffects` per seat; every seat's area is the whole window.
- **`seat_state` allowlist: 4,099 -> 4,024** (75 gone: view.c 8, gl_screen.c 19, sbar.c 27, the VR files 21; the
  check counts 63 `seat-shared` marks now). Merging next brought 2 newcomers: `vr_input`'s `clickDebounce` and `vr_slide`'s
  `headsetLegsState`, both marked shared (the headset's controllers and wearer: one headset, one VR player).
- **Before and after**: ROUND25.md "split3".

## 11. Phase 4a (2026-11-21): flat play's state

(Seats numbered from 0 as in the code.)

- **Flat play's per-player state: the seat's parts** (phase 3's pattern, `seat::part<T>`; the access rule: current, the
  code deep in the VR frame reading the current seat's through each file's one-line accessors):
  - `vr_flatplay` (`Part::FlatPlay`, one `SeatFlat`): `FlatState` (the hands' pose, punches, the shove and its
    smoothing, the parry guard, the two-handed stance, wall backs, shoulders, the arm probe), `Feel` (the drawn hands'
    lag, sway, bob, gun turn, wall tilt), `FlatKeys` (the mode's keys' edges, the crouch and prone eases and their
    requests, the counts), `CtrlBind` (the CTRL bind the mode takes: a seat's binding since phase 2), the slide's turn
    of the body (`slide::LegsTurn`) and the toolgun's keys.
  - `vr_flatholster` (`FlatHolster`: the holster menu), `vr_flatsight` (`FlatSight`: the ironsight, grips, stance,
    the weapon at walls, and the crosshair's memo), `vr_flattp` (`FlatTp`: the third-person camera and its memo),
    `vr_flatreload` (`FlatReload`), `vr_flatfgrab` (`FlatGrab`: E's carry), `vr_flatgrenade` (`FlatGrenade`),
    `vr_flatswap` (`FlatSwap`: the hands' swap).
  - Phase 3 had already moved the lean, the slide's view and the visor (vr_flatmove's `FlatView`).
- **The process's own, kept shared**: `FlatSession` (vr_flatplay: the `-flatscreenmode` flag, its console lines printed
  once, the headset's taking the game back), split out of `FlatState`.
- **Marked shared**: the tests' and probes' own (crouch trace, the slide ramp's trace points, flick test, melee probe,
  route probe, recoil probe, the swap's watch, the sweep and walk probes, the aim sensitivity, holster and third-person
  tests, the lean hit test), the orbit tool's camera, the server's bot being posed (for that call only), the model FOV
  counters (phase 7's), the look pass's GL objects (phase 7's per-view caches); vr_slide's server slots and reset hook,
  its headset's view drop and turning-stick gesture (one headset, one VR player) and its tests.
- **Left for their phases**: the slide's scrape sound (vr_slidesound: phase 8's listeners), the toolgun itself
  (vr_toolgun's state, shared with the headset's: 4b with the hands), the server's crouch and prone slots (vr_hull:
  per player slot already, server-side).
- **Resets**: the files' own resets (the mode turning off, a weapon changing) act on the current seat's, as phase 3's
  do; with the seat loop (phase 6) each seat's frame runs in its `Scope`.
- **`vr_seat_test`** (verdict `seat_test: N passed, M failed`, 56 checks): phases 1-3's 48 plus 8: a fresh seat's flat
  play is the defaults (no crouch or holster command, the sight's base FOV 90, standing); as seat 1 `+flatcrouch`,
  `+flatholsters` and the ironsight's base field of view are seat 1's and seat 0's are unchanged; seat 1 letting go
  leaves seat 0's crouch held; flat play's, the holsters' and the sight's parts are each seat's own, and a seat no code
  ran for has none made. Read-outs for it: `flatplay::crouchCommand()`, `flatholster::holdCommand()`.
- **`seat_state` allowlist: 4,025 -> 3,975** (50 gone: 15 moved into the seat's parts, 35 marked shared; the check
  counts 103 `seat-shared` marks now). Merging next brought 6 newcomers: the gadget's page (the headset's), the server's
  ragdoll scratch, the body and lift logs marked shared; the client's net ragdolls (`clientRags`, `netDraw`) accepted
  (phase 5's). The merge's view.c conflict: next's lift-rise height is the seat view's (`seatview_t.liftz`).
- **Before and after**: ROUND25.md "split4a".

## 12. Phase 4b (2026-11-21): the hands

(Seats numbered from 0 as in the code.)

- **The hands' per-player state: the seat's parts** (phase 3's pattern; the access rule: current, `hands::current()`
  and the rest reading the current seat's through each file's one-line accessor; 14 `seat::Part`s):
  - `vr_hands` (`Part::Hands`, `Rig`): `hands::current()`'s state and its frame, the seated tracking, the calibrated
    offsets, the play space's turn and its pending yaw, the velocities' previous samples, the stairs' easing, the
    room-scale walk, the lean and its sense, Reset Position, the teleporter walk.
  - `vr_held` (`HeldSeat`): the fists, the frames placed, what each hand holds and its ease, the hold in both hands,
    the hands' eases off a grip, the releases, the meets, the weapons' boxes, the hands drawn from, the remote
    server's places, the skewered gibs. Its types moved up beside the fists (qvr::held's); the code in the file's
    global unnamed namespace reaches them through using-declarations.
  - `vr_twohand` (`TwoHand`): the aim and stock transitions, the grips, the help and carry records, the handles, the
    ground spots, the free grips, the retakes, the grip's feedback, the frame time.
  - `vr_view_hands` (`ViewHands`: a hand on an embedded blade, on a skewered gib, on a long gun's hotspot),
    `vr_view_weapon` (`ViewWeaponSeat`: the reverse grips as drawn), `vr_throw` (`ThrowSeat`: the motion histories,
    the controllers' own, the physgun's point, the grips' clicks), `vr_grip` (the hands' frames), `vr_gripfit` (the
    rig frames), `vr_handsource` (each hand's source this frame), `vr_heldpose` (what went up: its `State` split into
    the client's `Sent`, a seat's, and the server's `Slots`, shared).
  - The toolgun (4a left it here): `vr_toolgun`'s state (`tg()`) and its two ghosts (held, looked at), the Brush
    tool's (`bt()`), the panel's (`ps()`).
- **Marked shared** (181): the server's per-slot state (fists, frames, rig frames, held props, heldpose's slots), the
  caches of the shared models (boxes, meshes, centres, posed spheres, reverse geometry, the drawn pose's), scratch,
  the reset hooks (they reset the current seat's), the tests' and debug readouts' own, other players' rigs (by
  player), the grip clicks' random pitch, the toolgun's armed catalogue and its ghost (QC's data and a memo), the
  icons, the weapons' settings table; and the **motion recorder** (`vr_motion*`, 109: the headset's tool, one
  recording, playback, evaluation or review at a time).
- **Left for phase 7** (in the allowlist): the drawn body (`vr_avatar*`: the posed body, its skeleton, the arms' and
  legs' eases, gait and stance; it poses every player's body, the local one among them) and climbing's client pins
  (`vr_climb`); the server's climbers are already per slot.
- **`vr_seat_test`** (verdict `seat_test: N passed, M failed`, 61 checks): phases 1-4a's 56 plus 5: a fresh seat's
  hands (no turn, tracked, no measured frame, no throw's samples); as seat 1 the play space's turn
  (`hands::setPlaySpaceYaw`), a hand's source (`handsource::mark`), its frame (`grip::setHandFrame`) and a throw's
  sample (`throwing::sample`) are seat 1's and seat 0's are unchanged; each seat its own rig, sources, frames and
  throws; seat 3 has none of the hands' parts made.
- **`seat_state` allowlist: 3,975 -> 3,697** (278 gone: 97 moved into the seat's parts, 181 marked shared; the
  check counts 289 `seat-shared` marks now). Merging next brought 4 newcomers, all marked shared: the hit zones'
  debug readout, navfollow's server bots and their clock, the scenario runner's counts.
- **Before and after**: ROUND25.md "split4b".

## 13. Phase 5 (2026-11-21): the client, its connection and its world

(Seats numbered from 0 as in the code.)

- **The seat's client** (`clientseat_t`, client.h; a `Seat`'s `net`, made by `seat::netOf`): Ironwail's `cls`
  (`client_static_t`: the connection, its message buffer, the demo fields), `cl` (`client_state_t`) and the client
  world's tables, once globals of cl_main.c, cl_tent.c and cl_parse.c: the static and temporary entities, the light
  styles, the dynamic lights, the beams, the visible entities, `cl_entities` and `cl_max_edicts`, the entity update
  count, the Nehahra warning. **The access rule as planned: current.** `cl_seat` points at the current seat's;
  `seat::Scope` and C's `CL_SeatSet` move it with the seat. `cls`, `cl` and the tables' old names are macros
  (`#define cl (cl_seat->cstate)`...), so the ~4,600 uses read as before; **nothing else may be named `cl` or `cls`**
  (a local, a parameter or a member so named fails to compile, never silently: 424 `cls` became `klass`, four `cl`
  locals renamed). Seat 0's is the game's own (CL_Init's message buffer on the hunk, Host_Init's state: as before);
  seats 1-3's are made on first use (~3 MB each, the entity tables), disconnected, out of the demo loop, their message
  buffer their own (`CL_SeatNetReady`). **With one seat `cl_seat` is seat 0's, always.**
- **The VR client's net state: the seat's parts** (phase 3's pattern; 14 new `seat::Part`s): the prediction
  (`vr_predict`'s `ClientPredict` and its correction totals), the snapshots read (`vr_netsnap`'s client state, the
  interpolation, the bytes read by command, the entries' tallies), the moves sent and acknowledged (`vr_netmove`), the
  ragdolls from the server (`vr_netragdoll`), `vr_client`'s hands' buttons as the move reads them, the grabs and
  throws' estimates, the client world's VR entity data and late brush models, the posing mode's held move, the
  preferences sent (`vr_clientprefs`), what the voice chat told the server it hears (`vr_voicechat`). The net HUD
  was phase 3's. Each file's state became a struct the seat owns, read through a one-line accessor.
- **Marked shared**: the server's halves (each slot's prediction, moves, snapshots sent and their encode, lag
  compensation, the voice relay), scratch and the self-tests', the reset hooks (they clear the current seat's), the
  demo (`demo_head`, `demo_rewind`: one demo recorded or played, seat 0's), the temporary entities' sounds, the PC's one
  microphone (`vr_voicechat`'s capture and push-to-talk), the voices played (phase 8's mixer), the voice menu (phase
  9), the weapons' muzzle settings, the old particles' random stream, a test sprite, the MP tests' own (`vr_mptest`);
  and, for phase 7 with the per-seat effects, the other players' copies (`vr_mpavatar`, the pump strokes, barrel
  heats, slides and wounds by player), the thrown gun's rope memory and the wounds' paint queue.
- **`vr_seat_test`** (verdict `seat_test: N passed, M failed`): phases 1-4b's 61 plus 9 (70 of 70): seat 0's client is current
  outside a seat loop; a scope makes seat 1's current; a fresh seat's client is disconnected, out of the demo loop,
  with its own message buffer, and its world empty; seat 1's time, visible entities, a light and a message byte leave
  seat 0's as they were; `CL_SeatSet` moves the client with the seat both ways; the prediction's totals and the
  interpolation are each seat's part.
- **`seat_state` allowlist: 3,697 -> 3,591** (106 gone: the client's 14 globals into `clientseat_t`, 24 into the
  seat's parts, 68 marked shared; 362 `seat-shared` marks now). Merging next brought 4 newcomers, all marked shared
  (other players' copies, phase 7: the pump strokes, barrel heats, slides, wounds).
- **Before and after**: ROUND25.md "split5".

## 14. Phase 6 (2026-11-22): the loopback grown to 4, the seat loop, server slots

(Seats numbered from 0 as in the code; the console and the Debug menu say seats 1-4.)

- **The loopback, a pair a seat** (net_loop.c): `clientseat_t` holds the seat's client and server sockets and its pending
  connect (`loopclient`, `loopserver`, `loopconnect`): net_loop.c's three globals gone. `Loop_Connect` makes the current
  seat's pair; the server takes the pending connects in seat order. Seat 0's server side is `"LOCAL"` as before, seat n's
  `"LOCAL <n+1>"`: `NET_QSocketIsLocal` (the transport: packet sizes, no map package, every frame sent) is true for any
  seat's, `NET_QSocketLocalSeat` says which. `progs::hostSlot()` (so `clientprefs::ownSlot()`) is the **current seat's**
  slot: each seat's client finds its own player and writes its hands into its own slot.
- **Slots like a network player's**: only seat 0's connection takes slot 0 (`VR_CoopPickSlot`'s host rule); seats 1-3
  take free slots under their own names (`CL_SeatName`: `<name> 2`...; their server sockets carry the name as a connect
  request does), so coop's level-start things, deathmatch's spawn and coop's "back by name" (B15's leavers: his old slot,
  what he had) apply to them unchanged.
- **The seats in play** (`vr_seat.cpp`): a bit each (seat 0's always) and the pad that joined each of seats 1-3.
  `IN_PadSeat`: a pad that joined a seat drives it, the lowest pad slot no seat has drives seat 0 (one seat: pad 0, as
  before). `seat::setInPlay(n)` (vr_seat_test's) keeps phase 2's mapping.
- **Joining and leaving** (`vr_seatplay.cpp`): `seat_join <pad>` (the next free seat; refused with no local multiplayer
  game or no free slot), `seat_leave [n]` (it disconnects as a network player does: `CL_SeatDisconnect`, never
  `CL_Disconnect`, which ends the server), `seat_status`, `seat_mockpad` (a pretend controller); the Debug menu's
  Network page has Join a Seat, Leave the Last Seat, Seats Status. `CL_SeatFrame` (once a host frame, before the seat
  loop): a seat in play and out of the game connects once the local game is in play (a server running, seat 0 signed
  on): after a new map or a load (the server dropped everyone) it joins the next game by itself; with no room it is told
  and leaves. A server message `svc_disconnect` ends a seat 1-3's connection only (`Host_EndGame` stays seat 0's), and a
  host error makes seat 0 current first.
- **The seat loop** (host.c `Host_NextSeat`): each playing seat's prespawn, look and move gathered (`CL_AccumulateCmd`),
  move sent each tick (`CL_SendCmd`, its own frame's pending move) and messages read (`CL_ReadFromServer`) with that seat
  current, seat 0 first, then seat 0 current again; the server frame runs once. With one seat: seat 0's alone, as before.
- **Headless seats 1-3 until phase 7** (`CL_SeatHeadless`): they parse into their own client world but start no sound
  (`S_StartSoundPitch`, `S_StaticSound`, `S_LocalSound`), no music or pause of it, no particles (`R_AllocParticle`), no
  sky load, no `R_NewMap`, no static entities linked into the shared world's leaves, no loading plaque, no relink of their
  entities (the shared effects: trails, torches, flames, ragdolls' models); their signons leave seat 0's demo head alone.
  Known: a seat joining runs the client's `ClientState` reset hooks, which also clear the shared ones (the decals,
  effects memories) once; its entity table is a hunk allocation per join until the next map (as a network client's).
- **Saving**: while more than one seat plays, only the level-start coop autosave (`autosave/coop~...`) saves
  (`Host_SeatsMaySave`); other saves are refused with a line.
- **`vr_seat_play_test`** (`Misc/quakevr/seat_play_test.sh`: coop and deathmatch on e1m1, maxplayers 4, flatscreen; verdict
  `seat_play_test: N passed, M failed`): seat 2 joins the running game by a mock pad (its own slot, name, client
  player, `hostSlot`; seat 1 still slot 0; a save refused, the coop autosave allowed), walks by the pad's stick (seat 1
  stays), is seen by seat 1 where the server has him and sees seat 1 (deathmatch: put beside seat 1 first, the server
  sending only what a player may see), leaves (its slot freed, the pad back to seat 1), joins again: coop, his old slot
  with health 70 and 50 nails as he left; deathmatch, spawned again. Coop 28 of 28, deathmatch 27 of 27.
- **`seat_state` allowlist: 3,591 -> 3,588** (net_loop.c's three into the seat's client). Merging next brought 14
  newcomers, all marked shared (the looks' table and the server's sends, the client's drawn looks: phase 7; navfollow's
  server bots and its debug paths; the hull holes' pool site and timings; the port check's text; a test's); two more
  merges (2026-11-22) 20, all shared: the Texture Studio (one editor: seat 1's), the headset's centre panel and visor
  pass (the headset is seat 1's), the ropes (the server's; the drawn ones: phase 7), other players' gore copies (phase
  7), the hole blockers' settings, tests, scratch and reset hooks.
- **Before and after**: ROUND25.md "split6".

## 15. Phase 7, part 1 (2026-11-22): each seat drawn in its tile

(Seats numbered from 0 as in the code; the console says seats 1-4.)

- **Tiles and layouts** (gl_screen.c): in flatscreen play (no headset: `VR_IsActive` off) every seat in play is drawn in
  its tile of the window. `SCR_SeatsDrawn()` (the seats in play; 1 with a headset: a VR host with flat seats is phase
  14's), `SCR_SeatRect(seat)` (the tile, top-left origin; one seat: the whole window), `SCR_SplitLayout()`.
  **`scr_splitlayout`** (archived, default 0): 0 from the window's aspect (his answer: an ultrawide, 2:1 or wider, side
  by side; else 2 seats stacked, 3 one wide on top and two below, 4 a 2x2 grid); 1 side by side, 2 stacked, 3 one wide
  on top and the others below, 4 a grid (3 seats in it leave the fourth cell dark). A seat's place is its rank among the
  seats in play (seat 3 is the second tile while seat 2 is out). Phase 11's menu offers it; the console has it now.
- **The seat's pass** (`SCR_DrawSeats`, from `SCR_UpdateScreen` in place of the one view): for each seat in play, that seat
  current (`CL_SeatSet`), **its tile made the window** (`SCR_SeatPassBegin`: `glx`, `gly`, `glwidth`, `glheight` and vid's
  sizes scaled to the tile), so the code that lays the window out lays the tile out unchanged: `SCR_CalcRefdef` (the
  refdef, `viewsize`, the status bar's room, the FOV from the tile's aspect), the flat render (`VR_FlatRenderView`: the
  seat's client world's entities, its view model, its hands and flat play's parts), the 2D canvases (the status bar,
  crosshair, hit marks, centre print and intermission scaled to the tile: `SCR_DrawSeatHud`). The scene targets stay the
  window's (a view renders into their corner and is warped into its tile); `R_Clear` clears only the view's tile while
  tiles are drawn. Then the window back, seat 0 current, and the shared 2D over the whole window (console, menus,
  notices, the demo bar, the net HUD). A seat joining shows its tile dark with "seat n: joining".
- **One seat: the frame is drawn as before** (no pass: the same calls in the same order; the tile code is skipped).
- **The seats' client worlds**: a drawn seat relinks its own entities and temporary entities each frame
  (`CL_SeatDrawn`: seat 0, or any seat while tiles are drawn), so each tile shows what its own client knows (deathmatch's
  per-player visibility included); its dynamic lights and beams are its own (phase 5's tables).
- **Kept seat 0's this part** (shared pools or memories, drawn in every tile): the particles (one pool: a second seat
  spawning the same explosion would draw it twice), the sounds and music (phase 8's listeners), the static entities'
  links into the world's leaves, the ragdolls' models, the torch and flame lights, the taken torches' flames, the
  loading plaque, `R_NewMap`. So a trail or explosion only seat 1-3's client sees (deathmatch, out of seat 0's sight)
  is not drawn yet.
- **Turned off while tiles are drawn**: the flat visor HUD (`visor::active`: its panels and passes are one view's; each
  tile has the classic status bar: phase 13's per-seat HUD) and flat TAA (`temporal::flatWanted`: one history, one view).
  The toolgun panel and the holster menu (drawn over the window at `VR_End2D`) stay seat 0's.
- **A phase 6 bug fixed**: the loopback had one client socket beyond `maxclientslimit` (seat 0's), so seats 3 and 4
  found none ("seat 3: no connection to the local game"): `net_numsockets` now has one a seat (net_main.c).
- **Debug aid**: `scr_seatmarkers` 1 marks each tile with its seat's colour and number (1 red, 2 green, 3 blue, 4
  yellow); 2 also reads each tile back in its pass (the marker's pixel, a checksum of its middle: the test's).
- **`vr_seat_render_test <n>`** (`Misc/quakevr/seat_render_test.sh <agent> [shots]`: coop on e1m1, maxplayers 4,
  flatscreen, n = 2, 3, 4; verdict `seat_render_test: N passed, M failed`): seats join by mock pads, each turned its own
  way; once every seat's view is drawn four times: the tiles are the layout's, inside the window, apart and covering
  it; each tile drawn from its own seat's player's eye (the nearest seat player to the drawn eye is its own, 0 units);
  its marker read back in its colour; its middle unlike the other tiles'; a screenshot; the seats leave and seat 0 is
  drawn in the whole window again. 2 seats 13/13, 3 seats 17/17, 4 seats 22/22.
- **Left of phase 7** (3-4 days): each seat's own particles and effects (a pool a seat, or one pool fed once per
  event); the drawn body (`vr_avatar*`: its posed body, eases, gait) and climbing's client pins per seat (now posed once
  a seat's frame on shared state); other players' copies (`vr_mpavatar`, gore, ropes drawn) posed once per frame, not
  per seat's relink; the visor per seat; the shadow and portal caches per view checked with several views.
- **Before and after**: ROUND25.md "split7".

## 16. Phase 7, part 2 (2026-11-22): each seat's body, pins and effects

(Seats numbered from 0 as in the code.)

- **Your drawn body, the current seat's** (vr_avatar.cpp): the posed body's globals (the pose, the skin, the skeleton,
  the arms' eases and forearms, the gait, stance, wading, climbing feet and body pitch: what `swapOther` already swapped
  for another player's body) are the current seat's. `seat::makeCurrent` calls `avatar::seatSwitched(from, to)` as the
  current seat changes: two swaps park the leaving seat's body in its part (`Part::AvatarSelf`) and bring the next one's
  in. One seat: never switched. The other players' and dolls' bodies are each seat's (`Part::AvatarOthers`): each
  seat's client poses the bodies it sees, its own entities, with its own memory (one seat: seat 0's, as before).
- **Climbing's drawn pins** (vr_climb.cpp: each hand eased onto its hold, `Part::ClimbPins`) and **your plates' morphs**
  (vr_view_body.cpp, `Part::ViewPlates`; a new level resyncs every seat's) are each seat's. The climbers themselves are
  the server's (shared).
- **The shared effects fed once per event** (`CL_SeatEffectFirst`, vr_seat.cpp; client.h's `SEATFX_*`): the particles
  are one pool, simulated once and drawn in every tile. Each seat's client reads an event its seat can see (the server
  sends it to every client in sight), so the first drawn seat this frame to feed it does and the others skip it (its
  kind, a key and its place rounded to a unit; a later seat's own event, out of the earlier seats' sight, is fed). Fed so:
  Quake's effects (r_part.c: the explosions, the blob, the particle effects, lava and teleport splashes, the bright
  field) with Quake VR's particles behind them, the entities' trails and the explosion debris's (by entity number: the
  server's, the same in every client). A seat not drawn feeds nothing (`R_AllocParticle` too); one seat: always fed.
- **Each drawn seat's lights**: its relink runs with its own last drawn eye in `r_refdef.vieworg` (tiles only), so the
  torch and flame lights (`VR_TorchLights`, `VR_FlameLights`: now every drawn seat's, into its own client's dynamic
  lights) and the projectiles' lights are chosen by its own eye. The taken torches' flames and the test sprite stay seat
  0's (shared memories).
- **The toolgun panel and the holster menu** (`VR_FlatSeatPanels`): one seat, from `VR_End2D` as before; seats in tiles,
  each seat's in its tile's pass (gl_screen.c's `SCR_DrawSeatHud`), from its own parts.
- **`vr_seat_render_test`** grew: each seat's body posed (`avatar::skeleton` in its scope) at its own player (the
  nearest seat player to the posed head); an explosion every seat reads in a frame adds one explosion's particles (372,
  as one seat's), one only the last seat reads adds its own; an entity's trail fed by one seat. 2 seats 20/20, 3 seats
  26/26, 4 seats 33/33.
- **`seat_state` allowlist: 3,588 -> 3,585** (the others' bodies, the pins, the plates). The swapped body globals stay
  listed (they hold the current seat's; the parked ones are the seats').
- **Left of phase 7** (2-3 days): the other Quake VR effect spawners (gore and blood, impacts, the gun effects, debris,
  drips, water: ~20 files calling `particles::` directly) not fed through `CL_SeatEffectFirst` yet (with tiles they
  spawn once per seat that sees them); sounds per seat listener (phase 8: each seat's listener mixed, the shared world
  sounds started once: `CL_SeatEffectFirst`'s `SEATFX_SOUND` is reserved for it; the sound starts still headless for
  seats 1-3); the flat visor per seat scaled to its tile and flat TAA per tile (still off while tiles are drawn); the
  shadow and portal caches per view checked with several views; the torch lights per seat checked on a map with torches
  near the start (e1m1's has none).
- **Before and after**: ROUND25.md "split7b".

## 17. Phase 7, part 3 (2026-11-22): sounds per seat, the effects' spawners, the visor per tile

(Seats numbered from 0 as in the code.) Phase 7 is done with this part; what it leaves is phase 8's (the table).

- **Sounds from each drawn seat's listener** (snd_dma.c; phase 8's first half): with seats in tiles, `S_Update` takes
  the drawn seats' listeners (`CL_SeatListeners`: each seat's last drawn eye and its right, `seatview_t`'s `drawnorg` and
  `drawnright`, and its player's entity). A channel is spatialized from each listener with Quake's panning and keeps
  the loudest (`SND_Spatialize`: the nearest seat hears it best, panned as that seat faces); a seat's own player's
  sounds play at full volume, as the one player's do. The ambient sounds take the loudest seat's leaf's levels. A sound
  every seat's client reads (the server sends it to all in hearing) starts once: `S_StartSound` is gated by
  `CL_SeatEffectFirst(SEATFX_SOUND, ...)` (the sound, the entity and channel, the place); a sound only a later seat
  reads starts too; a seat not drawn starts none (as before); `S_LocalSound` plays for any drawn seat. Steam Audio's
  spatial voices are off while there are several listeners (one head; `wanted()`), so Quake's panning does it. The
  static sounds and the underwater muffling stay seat 0's (phase 8). One seat: no listeners taken (0), the same path.
- **Quake VR's effect spawners fed once per server event** (`CL_SeatFxBegin` / `CL_SeatFxEnd`, vr_seat.cpp): around a
  temporary entity's parse (cl_parse.c) and a Quake VR message's (`VR_ParseServerMessage`: gore and wounds, the gun
  effects, tracers, ejected shells' smoke, impacts, sparks, ...), the bytes read are the event, the same in every seat's
  client that reads it: a later seat's identical one this frame has the particles it spawned taken back (both pools:
  `particles::truncate`, `R_ParticleTruncate`). This covers the ~20 files calling `particles::` from those handlers
  without touching each. The temporary entities' decals, material impacts and heat haze are fed once by their type and
  place (cl_tent.c's `CL_SeatDecalTE`, `CL_SeatImpactTE`, `CL_SeatHazeTE`). The per-frame emitters on shared memories
  run once a frame (`seat::sharedFxFirst`: the blood streams, the torches' sparks and smoke per torch; smoulder, the
  fire sources and the flames' wobble already ran once a host frame). One seat: nothing taken back.
- **The flat visor per seat in its tile**: `visor::active` no longer off in tiles; each seat's pass renders its visor's
  panels (`VR_VisorSeatPanels`, from its last HUD's input) before its view composites them into its tile (the panels'
  images are one set, each seat's in turn); `VR_End2D` renders them only with one seat. Flat TAA stays off in tiles
  (one history: a history a seat is phase 8's, not cheap).
- **Torch lights per seat checked** on e1m6 (torches by every start): each seat's own client's torch lights, chosen by
  its own eye (`emissive::torchLightsLit`).
- **`vr_seat_render_test`** grew: the sound listeners (one a drawn seat), a sound all seats read started once, one only
  the last seat reads at its eye at full volume from its listener; a Quake VR event all seats read spawning its
  particles once; each seat's visor on (the screenshot once every visor has powered on); `vr_seat_render_test 2
  torches` (seat_render_test.sh runs it on e1m6). seat_render_test.sh runs with sound on at volume 0. 2 seats 27/27, 3
  seats 34/34, 4 seats 42/42, torches 29/29.
- **Left (phase 8)**: decals and shells made by Quake VR's messages (their particles are fed once; the marks and the
  shells, shared memories, not yet), gore's other state; the static sounds and the underwater muffling per seat; flat TAA
  per tile; the shadow and portal caches checked with several views.
- **Before and after**: ROUND25.md "split7c".

## 18. Phase 8 (2026-11-22): the shared memories fed once, muffling per seat, shadows per seat's pass

(Seats numbered from 0 as in the code.) One seat: every path below is skipped or the same as before.

- **A repeat known as its parse begins** (vr_seat.cpp `CL_SeatFxBegin`): the drawn seats' server events this frame are
  remembered with their lengths; a later seat's parse looks ahead in its message for the same bytes (a repeat: the same
  event its client reads too). `CL_SeatFxRepeat()` says so (0: a first). Part 3's taking back of particles stays.
- **Quake VR messages whose memories are all shared skip a repeat whole** (vr_client.cpp `repeatSkipped`: particle2,
  wounds and their clears, blade wounds, weapons gone, hit reactions, gore sounds): the wounds painted, their blood
  streams and drips, the gore's landings and decals they make, the hit kicks and the gore sounds are fed once.
- **Others keep their seat's own part**: a tracer (the seat's own crosshair probe kept, the tracer drawn once), a shock
  (your own hit kept; arcs and burns once), a monster's muzzle flash once.
- **Casings per seat** (vr_shells.cpp): each seat's own ejections queue with its seat and leave its own weapons in its
  pass (its ports' speeds its own: `tracks` a seat); another player's casings are thrown by one drawn seat's client and
  never for a local seat's player (`seat::seatOfPlayer`: his own seat throws them); the casings are run once a frame
  (`SharedFx::Shells`) and drawn into every seat's scene (part 2 drew them in seat 0's tile only).
- **Static sounds**: the level's, started once (seat 0's client; the others start none), are spatialized from every
  listener as the other channels are (part 3's loudest-kept): checked (e1m6: 21 statics, none twice).
- **Underwater muffling per seat** (snd_dma.c, snd_mix.c): each listener's water (its leaf's contents; seat 0's
  `cl.forceunderwater`); a channel is muffled as the seat hearing it best hears it (`channel_t`'s `seatwater`; an
  ambient as the seat whose leaf gives its level). With several listeners and one under water, the mix paints those
  channels first, apart, filters them (snd_waterfx's strength) and adds them to the others: the seat in the water hears
  its sounds muffled, the one on land its own clear. One listener: the whole mix filtered as before.
- **Shadows with several views**: the blob shadows (vr_shadows.cpp) and the shadow maps (vr_lighting.cpp
  `VR_RenderShadowMaps`) were built once a frame from the first view: now for each drawn seat's pass (its eye's lights,
  its client's dlights and casters). The cost: the shadow maps rendered once a seat. The alias set-up cache keys by
  entity (each seat's client has its own) and the portals are chosen per camera already (`portals::update` per view):
  nothing to change; their targets are made anew when tiles of two sizes alternate (3 seats, gates on the map): a cost,
  not a fault (phase 11 may keep one a size).
- **Flat TAA per tile: not done, moved to phase 11.** Its history (`EyeState`), the entities' previous transforms
  (`slots`, by entity: each seat's own) and the motion target are one view's; a set a seat sized as its tile is half a
  day to a day, best with phase 11's tile sizes. It stays off in tiles.
- **`vr_seat_render_test`** grew: a Quake VR message all seats read is a repeat in seats 2-n and read whole by each,
  another is no repeat; each seat's player found as its seat's; one listener under water (seat 1's), its own sound
  muffled alone and another seat's clear; the static sounds none twice. 2 seats 33/33, 3 seats 40/40, 4 seats 48/48,
  torches 35/35.
- **Before and after**: ROUND25.md "split8".

## 19. Phase 9 (2026-11-22): menus and console per seat

(Seats numbered from 0 as in the code; the console, the menus and his answers say seats 1-4.)

His answer (section 7, question 4): customizable; by default in coop the menu pauses the game, and only seat 1 opens the
full menu, the other seats their own small menu.

- **Where a seat's keys go** (`Key_SeatDest`, keys.c; the phase's `SeatMenu` is the seat's part `Part::SeatMenu`,
  vr_seatmenu.cpp): seat 0's is `key_dest`, as always (the full menu, the console, the message line are his). A seat
  1-3's is `key_menu` while its own seat menu is open, else `key_game`: never the full menu or the console. With one
  seat in play, or `scr_splitmenu 1`, every seat's is `key_dest` (as before this phase). `Key_EventWithKeycode` routes by
  it (its 15 `key_dest` reads are the seat's `dest`; `con_forcedup` and the full menu's key grab are seat 0's only); so
  do the pad's stick arrows (`IN_PadEmit`), the stick's move and look (`IN_JoyMove`, `IN_GyroMove`: a seat with its menu
  open stops moving, the others play on), the console's stick scroll, the centre print and the intermission overlay in
  a tile. The mouse and the mouse wheel stay seat 0's (`key_dest`).
- **The seat menu**: a seat 1-3's Start (escape) or `togglemenu` opens it; drawn in its tile (`VR_SeatMenuDraw`, the
  seat's pass after its HUD) and driven by its own pad only (d-pad or stick arrows, A, B; each pad's events are its
  seat's since phase 2): Resume; Look Sensitivity (x0.25-x4, the seat's own on its stick, `VR_SeatLookScale`); Invert
  Look (its own, on top of `joy_invert`); Controls (its pad's buttons for attack, jump, the weapons, run, scores); Leave
  Seat (asked first; the seat leaves on A's release, so the press never reaches seat 0 when the pad goes back to him).
  Opening it lets go of the seat's held buttons (`CL_ReleaseMoveButtons`, the current seat's); a seat joining or
  leaving comes in with its menu shut. Its sensitivity and invert stay in the process (not saved): phase 12's profiles
  give each seat its own cvars. Seat 0's look reads exactly 1 and no invert.
- **The console is seat 0's**: `Con_ToggleConsole_f` does nothing for seats 1-3 (a pad's bind, or `togglemenu` while
  the console is up with `scr_splitmenu 1`).
- **The pause** (`VR_SeatMenusPause`, in `Host_ServerFrame`'s physics and `SV_RunClients`' think conditions beside the
  single-player menu pause): while more than one seat plays a local game, seat 0's full menu or any seat menu open holds
  the server. **`scr_splitpause`** (archived, default 1): 0 never, 1 in coop (a game without deathmatch), 2 in coop and
  deathmatch. A network player in the game: never (the game runs on for him; phase 14). Each tile but the menu's shows
  "paused: seat n's menu" (or "paused" for seat 1's full menu, drawn over the window anyway).
- **`scr_splitmenu`** (archived, default 0): 1 gives every seat's Start the full menu (one menu, driven by any pad, as
  before this phase). Both cvars are phase 13's menu rows; the console and the Debug menu have them now (Debug >
  Network: "Splitscreen: Seat 2's Menu", `seat_menu <n>`, its help naming both cvars).
- **One seat in play: nothing changes** (`Key_SeatDest` is `key_dest`, `VR_SeatMenusPause` is 0, the look's scale exactly
  1, the console and menus as before).
- **`vr_seat_menu_test`** (`Misc/quakevr/seat_menu_test.sh <agent> [shots] [prefix]`: coop and deathmatch on e1m1,
  maxplayers 4, flatscreen; verdict `seat_menu_test: N passed, M failed`, 31 checks a run): one seat, pad 0's Start opens
  and shuts the full menu (no pause); seat 2 joins by pad 0 (pad 1 then drives seat 1); seat 2's Start opens its own menu
  and not the full one; coop holds the server's time 12 frames, deathmatch runs on; its d-pad moves its cursor, raises
  its sensitivity (the stick's scale is seat 2's, seat 1's exactly 1), toggles its invert; its stick walks no one while
  the menu is open and its arrow moves only its menu; seat 1's pad opens the full menu meanwhile, pad 0 still drives
  seat 2's menu under it and pad 1's d-pad leaves it alone; seat 2's B shuts its menu only; the full menu pauses coop and
  not deathmatch; shut, the game runs; seat 2's toggleconsole does nothing; `scr_splitpause 0` runs coop on, 2 pauses
  deathmatch; `scr_splitmenu 1` gives seat 2 the full menu; a picture of seat 2's menu in its tile; Leave Seat asks, then
  seat 2 leaves on A's release and pad 0 drives seat 1 again. Coop 31/31, deathmatch 31/31.
- **Before and after**: ROUND25.md "split9".
