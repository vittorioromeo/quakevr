# Round 24: from 2026-11-05, towards 1.1

A per-feature reference: current behaviour only. Earlier work: `ROUND23.md` (1.1.0-beta4 to 2026-11-05: 8,600 lines, grep it, never
read it whole), `ROUND22.md` (1.0.1 to beta4). Add new work as a short section per feature, or fold it into its section: what it does,
settings and defaults, files, tests, open issues. Keep sections short: this file is meant to stay small.

## Lightning on water

His note: lightning shot into water (thunderbolt, Mjolnir hit, Shambler) should electrify the surface; "used to work".

**What the history says.** No commit broke it. `git log -S QVR_SHOCK_SURFACE` and `-S VR_LGWater_Beam` have one hit,
`35f25522f` (2026-09-30), and nothing since removed a call: the surface's arcs only ever came from the thunderbolt, fired
from the open. Mjolnir's water code (`7ab734e25`) was only the shock (self, burst), the Shambler never had any. The
thunderbolt's own path still works on the old code: fired by `W_FireLightning` the client draws the surface (test below;
the old code: 1 passed, 3 failed). It was never seen for a bolt that shocks you (hand, muzzle or waist in water): burst only.

**Now.** `VR_LGWater_Zap(from, end)` (weapons.qc; `vr_lg_water`, off with it): the look only, where the line crosses a
liquid's surface. `LightningDamage` and AD's `ad_LightningDamage` call it, so every bolt has it: thunderbolt, Shambler, Super
Shambler, Death Lord, traps. `VR_LGWater_ZapBelow(at)` is for a point: in a liquid the surface above it, else within 64 below:
Mjolnir's blow, its arcs wandering over water, and any shock (`VR_LGWater_ShockAt`, also the thunderbolt's). Damage through
the water is unchanged: still only the thunderbolt's (`VR_LGWater_Beam`) and the shock's, a monster's bolt is only the look.
Not done: a hammer slapping the surface with nothing solid is still no blow (melee traces ignore water).

**Test.** `vr_waterzap_test 1` (Debug > Tools > Test Effects > Lightning into Water; `vr_shock.cpp` counts the surface's
drawn frames, QC `watershockdrawn`): `waterzap: 4 passed, 0 failed` on vrfiringrange, e1m3, e1m7, e2m3, e3m4, e4m3, and with
`-RealTime` on vrfiringrange. Checklist scene: `water_lightning`.
