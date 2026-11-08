# Before 1.1.0: Vittorio's to-do

Short list. The full procedure is in [RELEASING.md](RELEASING.md). Release pass of 2026-11-08 (ROUND24.md, "Release pass
for 1.1"): the local build, the package, the installer harness, the smoke launch, the upgrade path and the campaigns all
pass on `agent/relpass`.

## Decide

- [x] **Version:** `VERSION` says `1.1.0` (stable; the betas were `1.1.0-betaN`).
- [x] **Armed replacements on by default** (`vr_agrunt_repl` 1; decided 2026-11-06). Configs from 1.0.x and the betas
  take it too unless the player had turned them on or off himself (fullConfigChanges, 2026-11-08).
- [ ] **Unpromoted tunables:** your `quakevr\tunables.cfg` holds 14 tuning values the release would lose (smoke blow,
  `vr_knockdown_ledge_reach`, `vr_model_light_grid_cell`, flat-screen gun/punch/slide values). Promote them
  (`python Misc\quakevr\tune_promote.py <your tunables.cfg>`, then commit `vr_tunables.inc`) or release with
  `-AllowUnpromotedTunables`.
- [ ] **Release notes:** the draft is `docs/vr-port/RELEASE_NOTES_1.1.md`: edit, delete its DRAFT comment, pass it with
  `-Notes docs\vr-port\RELEASE_NOTES_1.1.md`.
- [ ] **Flat-screen launch:** only `-flatscreenmode` starts it; no shortcut, no installer choice. Ship a "Quake VR
  (flat screen)" shortcut, or leave it to the notes?
- [ ] **The installer's settings page:** review its rows, order and labels (BACKLOG.md, section 2).
- [x] **"Armed Grunts":** "(Experimental)" removed from menu titles, rows, help and docs (decided 2026-11-08).
- [x] **Developer files in the package (decided 2026-11-08: keep the dev maps and scenarios in the zip for now, no change):** by the allowlist rule (every tracked file under `quakevr\`) the zip ships
  `checklist.txt`, the 88 `scenarios\*.txt` (bench ones too) and `vrtesthall`. Keep (the in-game checklist and its
  Test buttons use them) or add them to `$devOnly`.
- [ ] **Pick Up a Second** (`vr_agrunt_dual_pickup` 0): off for 1.1 as listed in the notes' Experimental section.
- [x] **Embedded blades:** `vr_embed` 1 by default (decided 2026-11-08); in the notes' Armed humanoids section.
- [x] **Tunables:** all 19 of his `tunables.cfg` values promoted to `vr_tunables.inc` (2026-11-08).
- [ ] **Release branch:** `master` is fast-forwarded by `-Final` (RELEASING.md, "Branches").
- [ ] **Debug symbols:** `ironwail.pdb` stays inside the game zip (crash reports name functions with it).

## Prepare once

- Nothing new since 1.0.1: the HD pack and ericw-tools' source are hosted on `assets-2026-10-08`; `gh auth status`
  logged in; the smoke test's Quake is found as the installer finds it (or `-QuakeDir`).
- **Test worktree** for `-RunTests`: a kit worktree at the release commit (`bash <kit>/new_agent.sh <name> <commit>`),
  passed as `-TestAgent <name>`.

## Check in VR before tagging

- [ ] The in-game checklist (VR menu > Checklist), at least its first sections.
- [ ] A fresh install with the built `QuakeVR-Setup.exe`: `Misc\release\make_release.ps1 -Local -RunInstaller` (a local
  server and a sandbox in `%TEMP%`; RELEASING.md, "Test a release locally"). Then an update over a 1.0.1 install.
- [ ] Start once from your own 1.0.1 or beta config: armed humanoids on, your own values kept.
- [ ] Flat screen: start with `-flatscreenmode` and play a map.

## Release

The one command and what it does: [RELEASING.md](RELEASING.md), "One command".

1. [ ] Merge to the release commit, push; the working tree must be clean.
2. [ ] Plan: `Misc\release\make_release.ps1 -Publish -Final -RunTests -TestAgent <name> -Notes docs\vr-port\RELEASE_NOTES_1.1.md -DryRun`.
   Fix every `PROBLEM` line (VERSION is already 1.1.0: no `-Bump`).
3. [ ] Release: the same without `-DryRun`.

## Known and accepted for this release

- **Multiplayer:** untested with the 1.1 systems; the menu shows a notice saying so.
- **SmartScreen:** no code-signing certificate; the notes explain "More info > Run anyway".
- **Arcane Dimensions:** experimental (a prompt says so); sky and light flicker in VR when turning; the hub's lower
  slipgates go nowhere.
- **Embedded blades:** on by default since 2026-11-08; tests 14-20 not run.
- **Known test failures, accepted 2026-11-08:** autopump, cellcord, xr_runtime, secret_hits, animsmooth. The release command is `-RunTests -AllowFlaky -FlakyTests autopump,cellcord,xr_runtime,secret_hits,animsmooth`.
- **Flat screen:** wrists bend 44-51 degrees on some holds; a gun's end can poke 4-7 cm into a wall at the limit.
- **Armed humanoids:** a knocked-down one aiming high may pull its arm in.
- **Old saves:** a save from a build before 2026-10-05 (no campaign keys) is refused with a message; saves from the
  betas load (fields they lack start at 0, fields since removed are skipped).
- **Tests:** the release suite (`run_test_suite.py`, ThinLTO Release build of f81f96c59, 2026-11-08): 39 passed, 5
  failed, all five known on next (QUEUE.md, "Pre-existing"): autopump 7/7 (shell timing, the same seven before the gun
  looks), cellcord 7/3, xr_runtime 2 (timing lines while unfocused), secret_hits (the quad shot's secret door),
  animsmooth 7/2. `-RunTests` stops on them: fix them first, or accept them with
  `-RunTests -AllowFlaky -FlakyTests autopump,cellcord,xr_runtime,secret_hits,animsmooth`.
