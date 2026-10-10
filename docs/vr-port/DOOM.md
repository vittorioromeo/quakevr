# Doom on the wrist gadget, and the gadget's touchscreen

A real game of Doom on the wrist gadget's screen, running in real time beside Quake (which keeps running: monsters,
physics, sound), played with your other hand's fingertip on the screen and, while Doom has the focus, the gadget
hand's stick and buttons. Its first purpose is fun; its second, as important, is the **touchscreen**: a general input
layer for the gadget that Doom is the first user of (a later map, menu or settings app on the gadget would use it the
same way).

Branch `agent/wristdoom` (round 22). Code: `Quake/vr/vr_doom.cpp` (the host), `Quake/vr/vr_touch.cpp` (the
touchscreen), `Quake/vr/doom/` (the DLL's interface and glue), `Quake/vr/external/doomgeneric/` (Doom itself),
`quakevr/doom/` (Freedoom, Doom's settings and saves, the WAD drop-in folder).

## How to play

1. VR Settings > *Doom on the Wrist Gadget* (or Wrist Gadget > *Doom*, or the console: `vr_doom`; bindable) > *Open or
   Close Doom*. The gadget's screen shows Doom instead of the HUD; Freedoom's title (or your own DOOM's) starts.
2. Raise the gadget and look at it (half a second): Doom has the focus. The gadget hand's stick walks and turns,
   trigger fires (in Doom's menu: selects; a question: yes), grip opens doors (in the menu: back), X next weapon (a question: no), Y Doom's menu, stick
   press the automap. Your other hand keeps its Quake weapon. Lower the wrist or look away: half a second later the
   stick and buttons are Quake's again, and Doom keeps running.
3. Or touch: bring your other hand's index fingertip to the screen. The touch controls appear while your finger is near
   (see "Touch controls"); every touch buzzes your hand. A touch also gives Doom the focus.
4. Close it the same way, or with the touch bar's X: the HUD is back, Doom waits where it was. Doom's own *Quit Game*
   ends it (its settings saved). Quake's menu, the console or the runtime's menu pause Doom with Quake.

Doom's saves and settings: `quakevr/doom/` (`default.cfg`, `savegame/<iwad>/`). An empty save slot is named after the
level, so Enter saves at once (no keyboard in the headset).

## WADs: your own DOOM, else Freedoom

Freedoom 0.13.0 (Phase 1: four episodes like DOOM's; Phase 2: thirty-two levels like DOOM II's) ships in
`quakevr/doom/`. id's DOOM WADs are never shipped; your own are found and preferred:

- **Steam**: The Ultimate DOOM / DOOM + DOOM II (`steamapps/common/Ultimate Doom`, its `base/` and `rerelease/`),
  DOOM II (`Doom 2`), Final DOOM (`Final Doom`), in every Steam library (`libraryfolders.vdf`).
- **GOG**: every installed GOG game with DOOM in its name (the registry's install paths).
- **The drop-in folder** `quakevr/doom/wads/`: put `DOOM.WAD`, `DOOM1.WAD`, `DOOM2.WAD`, `TNT.WAD` or `PLUTONIA.WAD`
  there (any letter case; any IWAD under any name is taken from this folder). The Doom page's *Open the WAD Folder*
  opens it on the desktop. `vr_doom_wad_dir` adds another folder.

Each IWAD is checked (its header) and an official release is recognised by its MD5 (DOOM 1.9 and shareware, The
Ultimate DOOM, DOOM II, TNT and Plutonia, the BFG and id Anthology versions) and named by it; another version (the 2024
re-release's, which this Doom engine may not run) is listed as "an unlisted version". The automatic pick (empty
`vr_doom_wad`, the page's *Automatic* row) takes a known release first (DOOM before DOOM II before Final DOOM), then
another version of id's, then Freedoom: Phase 1. A WAD that fails to start (or stops with an error in its first 20
seconds) is marked and the next is tried. The page lists every WAD found to pick one; `vr_doom_wads` prints them.

## Touch controls

The overlay is laid out on the screen's own pixels: 200 high and as wide as the screen (about 323 on the gadget,
which is 11.2 x 6.9 cm: 29 pixels to a centimetre). No target is under 1.3 cm. Drawn while your fingertip is near
(within 2 x `vr_touch_hover`) and for two seconds after a touch (*Touch Controls Shown*: always, or never); with Touch
Only always (faint while the finger is away). Touches count whether or not it is drawn.

```
Playing:                                              Doom's menu:
+------+------+------+------+------+------+------+    +------+------+------+------+------+------+------+
| MENU | MAP  |  W-  |  W+  | RUN  | LOCK |  X   |    | (the same top bar)                               |
+------+------+------+------+------+------+------+    +--------+                         +--------+
|                 |                    |   USE  |    |   UP   |   tap an item: it is    |   OK   |
|   MOVE (a drag  |   tap: use         +--------+    +--------+   selected and chosen   +--------+
|   stick: from   |   drag sideways:   |  FIRE  |    |  DOWN  |   (a slider's row: the  |  BACK  |
|   where you     |   turn             | (hold) |    |        |   half tapped)          |        |
|   touch)        |                    |        |    +--------+                         +--------+
+-----------------+--------------------+--------+    A question: YES and NO.
```

- **MOVE**: touch anywhere in the left part and drag: up and down walk, left and right turn (from where the touch
  began). **Pressing through the screen** (`vr_touch_hard`, 1.5 cm) while steering fires: one finger moves and
  shoots (*Hard Press Fires*). **LOCK** latches fire on (tap again to stop).
- **FIRE** fires while held; **USE** opens doors; a quick tap on the picture's middle uses too, a drag across it turns.
- **MENU** Doom's menu (Escape), **MAP** the automap, **W- / W+** the previous and next weapon, **RUN** always run,
  **X** closes Doom.
- In Doom's menu: tap a menu item to choose it (or UP, DOWN, OK, BACK); a yes-or-no question gets YES and NO.

*Controls* (`vr_doom_controls`): **Touch Only** (0: the sticks and buttons always Quake's), **Controller and Touch**
(1, the default), **Controller Only** (2: no touch controls; a touch still gives the focus).

## The touchscreen (for another app)

`vr_touch.hpp`. An app that wants touches calls, once a frame:

```cpp
touch::Request r;          // r.gadget = true: the gadget's screen; false: r.panel, a Surface of the app's own
r.pixels = {320.f, 200.f}; // its coordinates: touch positions come in these, from the top left
touch::request(r);
```

The view tests the other hand's index fingertip (`gearlights::pressingFingertip`, tuned by `vr_gadget_fingertip_*`)
against that screen right after it places the gadget (`vr_view.cpp` `gadgetTouches`: the hands and gadget as drawn
this frame) and the app reads `touch::contact()` the next frame: `hover`, `down`, `pressed` / `released` (edges),
`hard`, `pos`, `downPos`, `height` (cm in front of the face). No request: nothing is tested.

A touch begins with the fingertip within `vr_touch_press` (0.5 cm) of the face or through it (to `vr_touch_depth`,
4 cm) over the screen (`vr_touch_margin` past its edges), having come from in front (a finger poked through from
behind, or the screen moved onto a resting finger, never touches); it ends `vr_touch_release` (1.2 cm) off the face,
past the depth or off the edge; a new touch no sooner than `vr_touch_debounce` (0.06 s) after one ended. The hand
buzzes as it begins (`vr_touch_haptic`). Hovering: in front within `vr_touch_hover` (4 cm).

Widgets, immediate mode: each frame `Ui::begin(contact, pixels, alpha)`, then `button(id, rect, label, look)` (held,
pressed, released, clicked, hover), `stick(id, zone, radius, label)` (a drag stick: value -1..1, hard), `loose()` (a
touch that began on no widget: the app's own, such as Doom's menu taps), and `draw()` in the app's 2D pass
(`gfx::begin2D` on its target, its pixels as the virtual screen): plates, frames, labels, a held stick's line and knob,
and the fingertip's cursor. A touch is captured by the widget it began on until it ends.

**Priority on the gadget.** While an app requests the gadget's screen, the screen tap (bullet time,
`vr_bullettime_tap`) is off (`touch::takesGadgetScreen()`): a touch is the app's. The side button (the gear lights)
keeps working: it is off the screen. Without a request, everything is as before.

## Doom, the DLL

Doom is `qvr_doom.dll`, built by the solution (`Windows/VisualStudio/qvr_doom.vcxproj`) from doomgeneric (Chocolate
Doom cut down, GPL-2) and `Quake/vr/doom/qvr_doom_dll.c`, beside `ironwail.exe`. A DLL of its own so that Doom's names
(Z_Malloc, I_Error, M_Init, ...) never meet Quake's: it exports only `qvr_doom_*` (`qvr_doom_api.h`): init (the IWAD,
the data folder, more arguments: `vr_doom_args`), tick (ms), the frame (320 x 200 RGBA), keys, a status (menu, level,
health, ammo, place: `vr_doom_info`), a menu tap, a setting by name, typing (cheats: `vr_doom_type iddqd`), the error,
shutdown. It is loaded when Doom is first opened; until then nothing of it costs anything.

- **Its clock is the engine's.** `qvr_doom_tick(ms)` adds the frame's time and runs Doom's loop only when a new tic
  (1/35 s) is due; Doom's own waits move its clock on (never a busy wait). Closed or paused, its clock stops.
- **Its exits come back.** `I_Error` and `I_Quit` longjmp to the call that ran Doom (patched in `i_system.c`; any
  other `exit()` too, by the forced include): an error is reported, Doom shut down and unloaded. Its memory is a
  private heap and its files its own C runtime's (static): shutdown frees all, a new start loads it afresh.
- **The screen melt** runs a frame at a time (patched in `d_main.c`), the game's tics waiting for it as vanilla's.
- **Sound effects** (`DG_sound_module`): each started sound hands its DMX lump to the engine, which makes it a WAV in
  memory (`VR_SndMemory`, from `S_LoadSound`) and plays it as a sound of its own entity (one channel each of Doom's
  eight: a new sound cuts its channel's last, as Doom's) at the gadget's screen, a few centimetres to the side by
  Doom's stereo separation, following the arm; through the engine's mixer and Steam Audio like any other sound,
  quieter (`vr_doom_volume` 0.5) and fading faster (`vr_doom_attenuation` 2): a small speaker on your wrist.
- **Music is not played** (the music module is silent). The way to it: Chocolate Doom's OPL music player with
  Nuked-OPL3 (LGPL-2.1) in the DLL, mixed into a stream the engine plays at the gadget (`S_RawSamples`-like).
- **The picture**: the frame is a texture of the texture manager (`gfx::Picture`, nearest filtering), drawn 4:3 (as
  Doom was shown; `vr_doom_aspect` 0 stretches it) with the touch overlay into a target of three times the screen's
  pixels, only when it can be seen (the gadget shown, or the panel) and uploaded only when Doom drew a new frame; the
  gadget's screen shows it with the CRT look (`vr_doom_crt`, half the HUD's strength, a scanline a pixel, no glow).
  `vr_gadget_screen_dump <name>` writes it.
- **The big panel** (`vr_doom_panel 1`): the same picture on a panel `vr_doom_panel_size` (0.5 m) wide, placed
  `vr_doom_panel_distance` in front of you when turned on (*Place the Panel in Front* again), fixed in the play space
  (it moves and turns with you); touched the same way.

## Decisions

- **Quake never pauses for Doom** (Vittorio: "as if the game is actually being played in real time on the player's
  wrist gadget"). Quake's own pause (its menu, the console, the runtime's menu, `pause`) pauses Doom too; deaths,
  level changes, saves and loads leave Doom as it is. Multiplayer: the same (nothing of Doom touches the server).
- **The focus** is given by a look (the screen raised, facing you within `vr_doom_focus_angle` 25 degrees, for
  `vr_doom_focus_look` 0.5 s) or a touch, and taken back after `vr_doom_focus_grace` (0.5 s) out of view or by Quake's
  menu. Only the gadget hand's stick and buttons go to Doom (its menu button stays Quake's); the other hand is never
  taken. Quake sees them let go while Doom has them. `vr_doom_pause_unfocused 1` pauses Doom while it hasn't the focus.
- **Doom shows instead of the HUD** while open (the gadget's one screen). The hologram, the notify log and the side
  button keep working.
- **The installer** needs no step: `quakevr/doom/wads/README.txt` is tracked, so the package (an allowlist of tracked
  files) creates the folder; the game creates it too when it first looks.
- **The 2024 re-release's IWADs** are listed as unlisted versions and ranked after the classic ones (Steam's
  DOOM + DOOM II keeps the classic DOOM.WAD and DOOM2.WAD in `base/`, which are preferred); one that fails falls back.

## Testing (the mock)

The pose that raises the gadget and looks at it: `vr_mock_hand off 0 1.3 -0.35 -40 -80 0; vr_mock_look 30 15`
(`vr_doom_info` prints the screen's distance, angle and whether it is looked at). A touch: `vr_mock_hand_to main touch
<x> <y> <cm in front>` (3, then a few frames later -0.3 to touch, 3 to lift; with a weapon's spring the hand settles
over about 25 frames, and `vr_gadget_fingertip_drawn 0` keeps the fingertip fixed). Keys straight to Doom:
`vr_doom_key <name> [1|0|tap]`. `vr_debug_touch 1`, `vr_debug_doom 1` print the touches, the focus and the menu taps.
Round 22's checks (ROUND22.md, "Doom on the wrist gadget"): Doom started on Freedoom from a touch on MENU, a tap on
New Game, OK, OK; the MOVE stick walked it 535 units; FIRE spent two bullets; the off stick moved Quake's player, not
Doom's, until the look gave Doom the focus, then Doom's and not Quake's; Quake's menu froze both clocks; looking away
gave the stick back and Doom kept running; an init error (`vr_doom_args "-warp 9 9"`) fell back to Phase 2, and a
restart loaded the DLL afresh.
