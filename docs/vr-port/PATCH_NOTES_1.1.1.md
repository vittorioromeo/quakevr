# Quake VR: UNLEASHED 1.1.1: patch notes

**Version 1.1.1**, compared with 1.1.0 (whose notes, PATCH_NOTES_1.1.md, are final). This file grows with every change
that ships in 1.1.1.

## Graphics

- **Relighting about twice as fast.** Quake VR now ships its own build of ericw-tools' `light` (2.0.0-alpha11-qvr1)
  for the relighting (Graphics > Relighting, the installer's relight of the official maps, Download ericw-tools). With
  Bounced Light on (the default) it traces the bounced light from fewer samples and shares it between neighbours:
  e1m1 relights in 4 s instead of 8, e3m5 in 8.5 s instead of 21, Arcane Dimensions' ad_mountain in 50 s instead of
  95 to 113, the installer's relight of Quake's and the mission packs' maps in 6 minutes instead of 9, and the look is the same (a change of under one step in 255 on average). Maps you relit before stay as
  they are and are not relit again. Its source, with the changes and how to build it, is offered beside every release
  (GPL-3). The setting `vr_relight_bounceextra` (2; 0 = as before) is for comparisons.

## Menus

- **Search filter (development builds).** The Search page has a "Show: Both / Cvars / Tunables" row over its text
  box: the menus' settings alone, the Tunables pages' constants alone, or both (the default). The results update
  at once; the choice lasts until the game quits. Release builds have no tunables, so they show no row and search
  as before.
