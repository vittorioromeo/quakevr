# Quake VR: UNLEASHED 1.1.1: patch notes

**Version 1.1.1**, compared with 1.1.0 (whose notes, PATCH_NOTES_1.1.md, are final). This file grows with every change
that ships in 1.1.1.

## Campaigns

- **Dwell (experimental).** Dwell's two episodes, and custom maps made for Copper (such as Underdark Overbright), now
  play, by Copper's rules: Select Campaign > Dwell, and the Map Library's Copper maps. The installer offers Dwell
  (free, by Fairweather and the Dwell team), ticked by default. Dwell's own monsters (the Kamikaze, the Brute, the
  Construct and both bosses) fight in its maps, and, with Dwell installed, in any map and from the toolgun; its
  weapons and powerups work for now as variants of ours (their own VR weapons come in a later update).

## Graphics

- **Relighting about twice as fast.** Quake VR now ships its own build of ericw-tools' `light` (2.0.0-alpha11-qvr1)
  for the relighting (Graphics > Relighting, the installer's relight of the official maps, Download ericw-tools). With
  Bounced Light on (the default) it traces the bounced light from fewer samples and shares it between neighbours:
  e1m1 relights in 4 s instead of 8, e3m5 in 8.5 s instead of 21, Arcane Dimensions' ad_mountain in 50 s instead of
  95 to 113, the installer's relight of Quake's and the mission packs' maps in 6 minutes instead of 9, and the look is the same (a change of under one step in 255 on average). Maps you relit before stay as
  they are and are not relit again. Its source, with the changes and how to build it, is offered beside every release
  (GPL-3). The setting `vr_relight_bounceextra` (2; 0 = as before) is for comparisons.
- **Bloom without black or coloured blocks.** One extremely bright pixel can no longer spread a block of black or
  colour through the bloom.
- **Safe mode only after a real failed start.** The automatic GL safe mode (optional graphics features off for a
  start) now turns on only when the last start crashed, or died within seconds of building its shaders. Closing the
  game while it compiles its shaders on the first start, or after a slow compile, no longer puts the next start in
  safe mode; the start-up log (`quakevr/crash/gl_startup.log`) and the console say why either way.

## Movement

- **Sliding in VR: the desktop screen's slide, view and body.** While you slide in VR (Sliding (VR, Experimental)),
  your view now moves quickly but smoothly down to the desktop screen's sliding eye height and back up when the slide
  ends, on top of your own head's movement; your drawn body takes exactly the desktop screen's slide pose, its legs out
  on the floor. VR Settings > Locomotion: Slide View (`vr_slide_camera`, on; off keeps the view at your real head) and
  Slide View Time (`vr_slide_camera_time`, 0.15 s). The comfort vignette counts the view's move as movement.
- **Climbing: choose when you pull up onto a ledge.** Advanced VR Options > Movement > Climbing > Mantle When
  (`vr_climb_mantle_trigger`): High Enough (the default, as before: at once), Let Go (hang and look over the top, pull
  up when your last hand lets go; let go lower and you drop), Jump, or Let Go or Jump. Flatscreen mantles and ladders
  are unchanged.
- **Sprint by clicking the stick.** VR Settings > Locomotion and Controls > Speed Button (`vr_sprint_toggle`): Toggle
  makes a click of the speed button (the left stick's click as shipped, or Shift) switch between walking and running
  instead of holding it. The switch ends by itself, back to your Default Speed, when you stop moving for a moment, die,
  change level or open the menu. Hold (the default) is as before.

## Menus

- **Search filter (development builds).** The Search page has a "Show: Both / Cvars / Tunables" row over its text
  box: the menus' settings alone, the Tunables pages' constants alone, or both (the default). The results update
  at once; the choice lasts until the game quits. Release builds have no tunables, so they show no row and search
  as before.
- **No attract-mode demos.** Quake's demos no longer play by themselves at start-up or from a mod's start-up
  script, on the PC or the Quest, in VR or flat: flat goes straight to the main menu (VR starts in the hub as
  before). Options > Interface's "Start Demos" row is gone. `playdemo` and `timedemo` still play a demo.
