# 1.1.0 readiness: audit of 2026-11-16 (agent/rel11audit)

Run on `origin/next` at 27a775e90 + the patch-notes line. Nothing was tagged, pushed or published.

## Blockers

- None release-blocking found, but two test regressions against the refactor baseline.tsv (reproduced on a second
  run, not bisected; both likely older than the RC, e.g. the arena director default of 11-11, 7984c2e8d):
  `console arena_6` FAIL 6/2 ("the map grows no faster than a wave clear's drops", "no broken guns"; was PASS 8/0) and
  `console arena_10` TIMEOUT at 1600 s (was PASS 160/0). `suite teleporters_chase` FAIL 5/1 (flaky list; failed twice).
  `scripts ragdoll_stagger` FAIL 54/3 (known, was FAIL 52/5).

## Needs his hands

- [ ] VR pass of the in-game checklist (sections 4-10), a packaged-game smoke, `make_release.ps1 -Local -RunInstaller`
  (fresh install, then an update over 1.0.1), `-flatscreenmode` on a map (RELEASE_1_1.md, "What's left for you").
- [ ] The installer's new desktop flat-screen shortcut checkbox (a9ec1997e): check it in the installer UI.
- [ ] RELEASE_NOTES_1.1.md: delete the "Release candidate: please test" section (header and paragraph, lines 5-7ff)
  and the DRAFT comment (if still present) before `-Publish`.
- [ ] Decision to confirm: the brief expected the player disarm default OFF, but `vr_player_disarm` ships 1 (ON;
  ROUND25 "pdisarm", PATCH_NOTES says "on by default"). If it should be off, change `Quake/vr/vr_cvars.inc` and the
  patch-notes line 69.

## Ok

- VERSION 1.1.0. README links all resolve to tracked paths.
- Cvar/tunable changes since v1.1.0-rc.1: only `vr_player_disarm_sound` 1, `vr_flat_key_ammo` b, `vr_revgrip_2h_gate`
  1, `vr_revgrip_blocked_pulse` 0.25: none personal. No calibration, toolgun or comfort default changed since the RC.
- PATCH_NOTES_1.1.md already covered every user-facing commit since the RC (teleporter exits, installer shortcut,
  command buttons, dagger/throw fixes, flip gate, disarm sounds, flat ammo key, Burst Rifle naming); one line added
  for the Resolve title kit.
- Dev build (build.sh): QC 0 warnings, source checks and FGD pass, built. ReleaseLTO with
  `-p:QvrReleaseVersion=1.1.0`: exit 0, 0 warnings.
- Baseline (suite, agrunt, console, scripts, eval; 306 cases): other changes against baseline.tsv are higher check
  counts or fixed cases (agarmor_5, rigstudio_subjects, ragdoll_climb, limb_wear, judo all now PASS).
