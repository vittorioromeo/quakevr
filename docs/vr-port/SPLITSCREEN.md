# Local splitscreen in flatscreen mode: survey, inventory, options, plan

Phase 0 (agent `split`, 2026-11-20): research and design only, nothing built. His request (2026-11-20): splitscreen in
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

## 4. Recommendation

**(c), seat processes**, because our client is ~2,800 file-scope variables of single-player VR state over 393 files:
(a1) would touch ~250 of them and tax every future feature, with bleed bugs that only play finds, for a feature few
will use daily; (c) gets the same screen, controls and modes with the gameplay code untouched and the tested
multiplayer path, in a third of the time. Its price is memory and load time (4 x 2-3 GB); if that proves too much on
his PC, (a1) is the step after, and phases 2-4 below (pad controls, profiles, join screen, layouts, menus) carry over
to it unchanged; only phase 1's process management would be replaced.

### Phases (c)

1. **Seat processes and layouts (2-3 days).** `-splitseat`/`-splitpad` roles (no KB/M, one controller, own sounds
   only), the host spawning and placing them (CreateProcess, borderless, no focus, following the host), the layout
   function (2 side by side or stacked, 3, 4 as a grid; a pure function of the screen and seat count), lifecycle (quit
   together, a dead seat dropped). Console entry `vr_split <n> [layout]` (Debug menu row too). Measure RAM, VRAM and
   frame time at 1-4 seats (`--exclusive`, LTO). Tests: `Misc/quakevr/mp/split_test.py` (or `mp_test.py` scenarios),
   `-loopbackonly`, fast mode: 2 and 4 seats come up and spawn (verdict `split_up: N passed, M failed`); the layout
   rectangles for every layout and size (in-engine test, no windows); a seat killed is dropped; host quit ends all.
2. **Flat play on a controller (2-3 days).** A controller layout for every flat action (most `vr_flat_key_*` keys can
   name a pad button already, being `keydown[]` names; crouch is `K_CTRL` in code and the hands' fire and ironsight are
   `+attack` binds; a second name per action, `_pad`, for the defaults), right-stick aim
   (Ironwail's `joy_*`), per-seat profiles (`seat2.cfg`..: name, colours, sensitivity, invert, hand and flat settings;
   seat 1 is the normal config). Useful on its own: a pad in single-player flat play. Tests: each flat action through a
   synthetic pad press (a test command feeding `Key_Event` with pad keys), verdict line.
3. **Splitscreen menu and join screen (2-3 days).** Multiplayer > Splitscreen: one tile per seat, "Press A / Start to
   join" (keyboard and mouse is seat 1; a pad may take seat 1 if he wants: question 3), the layout choice, coop or
   deathmatch, map and skill, start. In game: seat 1's Esc is the game's menu; another seat's Start opens its seat menu
   (its settings, leave). The pause rule and drop-in/out per his answers. Calibration boards: a board line if he wants
   one. Tests: the join screen driven by synthetic pad presses, a game started with 3 seats, a seat leaving.
4. **Online, docs, polish (1-2 days).** Splitscreen group joining a remote host (the join flow starts the seats with
   the server's address); a splitscreen host with network guests (maxplayers); MULTIPLAYER.md and FLATPLAY.md sections;
   checklist lines.
5. **One window (optional, 3-4 days).** Hidden seat processes, images shared through GPU memory, the host composing;
   falls back to phase 1's windows where the extension is missing.

Every phase keeps VR untouched (the splitscreen menu and flags only in flat mode; `-splitseat` implies
`-flatscreenmode`) and runs the single-player and `mp_test.py suite` regressions.

## 5. Questions for him

1. **Seat processes (c) or in-process seats (a1)?** (c): 8-11 days, ~2-3 GB of RAM per seat; (a1): 25-35 days, one
   process, a per-seat rule on all client code from then on.
2. **Layout defaults**: 2 players side by side (vertical split) or stacked (horizontal split)? 3 players: one wide tile
   on top and two below, or three columns, or a 2x2 grid with the fourth tile showing the map or scores?
3. **Who is seat 1**: keyboard and mouse always, or whoever presses first (so four controllers work with no keyboard)?
4. **Menu and pause**: does seat 1's menu pause the game for everyone in a splitscreen coop game (single player's
   habit), or never (multiplayer's)? Can the other seats open the full menu or only their seat menu?
5. **Saving**: splitscreen coop under the multiplayer rules (level-start autosave only), or save at will when every
   player is on this PC?
6. **Drop-in/out**: may a controller join a running game (the layout changes from 2 to 3 tiles), and a seat leave?
7. **The controller layout for flat play** (phase 2): he approves a proposed mapping (fire per hand on the triggers,
   ironsight/guard, lean, crouch, grab, shove, holster menu, swap, reload, punch: more actions than buttons, so a held
   modifier or a radial); aim assist on a stick, or none?
8. **A VR host with flat seats on the monitor** (asymmetric couch co-op: one player in the headset, the others on the
   PC's screen): wanted later? (c) makes it possible; the brief keeps VR out of scope.
