# Releasing 1.1: the checklist

The release-readiness audit of 2026-11-14 (branch `agent/relaudit`; ROUND25.md, "relaudit"). Everything needed to
release 1.1.0, ticked where done. The procedure is [RELEASING.md](RELEASING.md); the short to-do and the decisions so
far are [RELEASE_TODO.md](RELEASE_TODO.md). Nothing was pushed, tagged or published.

## What's left for you

- [ ] **Your VR pass:** the in-game checklist (VR menu > Checklist: 68 items in 10 sections, flat-screen first; 48
  with a Test button). At least sections 4-10 (the headset's).
- [ ] **Confirm the shipped defaults** (section 2 below): the Run Setup's (Style Unleashed, Gore My Settings, Armed
  Humanoids and Smarter AI As Style, Shotgun and Axe, every mutator Off, Enemy Count 1x) and two Menu Look values your
  config changes (Big Menu Highlight 1, shipped 2; Shortcut Group Gap 6, shipped 5): promote them or keep the shipped.
  Also `vr_flat_chest_down` (shipped 0.3, yours 0; BACKLOG.md section 2).
- [ ] **Release notes:** read `RELEASE_NOTES_1.1.md` (brought up to 11-14 in this audit), edit, delete its DRAFT
  comment.
- [ ] **The installer's settings page:** rows, order, labels (BACKLOG.md section 2).
- [ ] **Installer UI:** `Misc\release\make_release.ps1 -Local -RunInstaller` (a fresh install, then an update over a
  1.0.1 install); start once from your own 1.0.1 or beta config; `-flatscreenmode` and play a map.
- [ ] **Release:** RELEASE_TODO.md, "Release" (the `-DryRun` plan, then the real one). The `-FlakyTests` list there
  (autopump, cellcord, xr_runtime, secret_hits, animsmooth) is from 11-08: all five were fixed on 11-12 (ROUND25.md,
  "Always-failing tests, part b"), so try `-RunTests` without `-AllowFlaky` first.

## 1. Upgrade safety from beta 4

How: `scratch/ra_oldcfg.py <tag>` writes the tag's full config (every archived cvar at the tag's default, its shipped
`vr_defaults.cfg` over it, the tag's `vr_cfg_version`), `ra_upgrade.sh` runs the game on it (`exec`,
`vr_migrate_config`, with `-zone 4096 -heapsize 256000 -minmemory`) and `ra_cmp.py` lists every setting whose default
changed since the tag and is still at the old one ("stale").

- [x] **Beta 4 config** (v1.1.0-beta4, config 207, 3,885 settings): before the fix, "Unknown command" for
  `vr_handgrenade_fuse` and 3 of the 46 changed defaults stale (`vr_decap_head_size` 1.15, `vr_decap_neck` 3,
  `vr_gore_mist_near` 40: changed on 11-10/11-11, after the 11-08 `fullConfigChanges` table). **Fixed:** the three
  rows added to `fullConfigChanges`, `vr_handgrenade_fuse` retired. After: 0 unknown, 0 of 46 stale.
- [x] **1.0.1 config** (config 126, 2,569 settings): 0 unknown, 0 of 37 stale.
- [x] **Retired names** in an old or a dev config stay quiet: `vr_portals_monsters_all`, the stir tunables
  (`vr_knockdown_stir_rate`: a tunable, its line taken as tuned), and `vr_nav_gate_chase` / `vr_nav_tele_stuck`
  (printed "Unknown command": now retired too).
- [x] **`-zone`, `-heapsize`, `-minmemory`** on the command line: no error (ignored since the memory rewrite).
- [x] `config_fullupgrade_test.sh`: 9 passed, 0 failed (two new checks: the beheading neck moves, the old fuse quiet).
- [x] **configVersion:** 224, no bump needed: configs are sparse since 218 (a changed default reaches every player who
  never set it); full configs (1.0.x, the betas) take `fullConfigChanges`.
- [x] **Old saves:** `vr_runsetup_test` 78/0 (a save without the run line loads with the mutators at their defaults
  and the presets left alone); `vr_continue_rows_test` 20/0 (Continue rows from autosaves). No beta 4 build or save is
  at hand: the 11-08 release pass's synthetic old save (beta 4's removed fields skipped) stands.
- [ ] Coop saves' format 2 (single player 1): not on `next` (agent/mp's work); check when it merges.
- Not real issues: `in_disablemacosxmouseaccel` (macOS only, never in a Windows config) and an empty
  `gl_texturemode` (the generator's, not a real config's).

## 2. Tuning and shipped defaults

- [x] His `quakevr/tunables.cfg` against the shipped tunables (`tune_promote.py --check`): 1 value
  (`vr_knockdown_stir_rate` 12), already the default: nothing to promote.
- [ ] To confirm (his call): the Gameplay preset's default Unleashed; Run Setup's defaults (above); Menu Look's Big
  Menu Highlight (his 1, shipped 2) and Shortcut Group Gap (his 6, shipped 5); `vr_flat_chest_down`. His other config
  lines are personal (Menu Detail Developer, menu positions and folds, the flat window's place, the last Run Setup
  pick): not defaults.

## 3. Docs

- [x] FEATURES.md: new sections Starting a run (Run Setup, presets, mutators), Flat-screen play, Armed humanoids,
  Enemy AI (stealth, navmesh); bullets for the dagger, the reverse grip, stirring, Menu Look and the holster drag,
  Select Campaign's Continue, Arcane Dimensions, the Survival Arena (it had none of these; its "Flat screen:
  `vr_enabled 0`" line was stale).
- [x] README.md: armed humanoids, AD and the arena, Run Setup, flat-screen play (`-flatscreenmode`).
- [x] INSTALL.md: the Flat Screen shortcut.
- [x] CREDITS.md: vkQuake (the pusher grid, the freed-edict physics fixes) and FTE QuakeWorld (the hull trace, off).
  Slipseer and Quaddicted are services, credited where the Map Library is described.
- [x] RELEASE_NOTES_1.1.md: Run Setup and mutators, Classic Quake, the dagger and reverse grip, stirring, the navmesh
  and hunting, Menu Look, Select Campaign, the Map Library, flat keys, the memory rewrite, the AD decal freeze;
  multiplayer under Known issues.
- `-memguard` is a debugging aid (ROUND25.md "uafsweep"): not player-facing, left out of the docs.

## 4. Release mechanics

`make_release.ps1 -Local -Rebuild -QuakeDir <kit qbase> -Notes docs\vr-port\RELEASE_NOTES_1.1.md` on this branch
(everything but the installer's UI; `-RunInstaller` not run):

- [x] Preconditions: VERSION 1.1.0; his tunables.cfg: nothing to promote; the HD pack and ericw-tools' source hosted.
- [x] Source checks: statics, QC precedence, the release QC (`-DQVR_NO_TESTS`), FGD: ok.
- [x] Release-config build (ReleaseLTO, ThinLTO, QVR_RELEASE), 0 compiler warnings. **Fixed:** the first build
  stopped on "live tuning" strings in the exe: two Debug-page rows (`tune vr_flat_aim_point 0` /
  `tune_reset vr_flat_aim_point`) were compiled into the release; now development-only.
- [x] Package: 4,096 files, exactly the allowlist, no id files, `ironwail.pdb` included on purpose.
- [x] Installer self-tests 41/41; latest.json read as the installer reads it; the packaged QuakeVR-Setup.exe's
  off-screen install: all files intact.
- [~] Smoke launch of the packaged game: it ran and quit cleanly, but the check stopped because the console named
  the build `5403d23a-dirty` (my edits to this tree during the build): rerun on a clean tree. Expect a pass.
- [ ] `-RunInstaller` (the installer's UI): his.
- [ ] `-RunTests -TestAgent <name>` (the headless release suite): the coordinator's or his, at the release commit.
- Note: `make_release.ps1` picks Visual Studio 2022's MSBuild (vswhere's newest with ClangCL); pass
  `-MSBuild F:\VS2026\MSBuild\Current\Bin\MSBuild.exe` to build as the kit does. With VS 2022 it also rebuilt
  `build/qvr_checks/qvr_checks.exe`, which then exited 3 with no output under the kit's `build.sh` ("BUILD FAILED",
  an empty build.log) until rebuilt with VS 2026.
- Release notes draft: `docs/vr-port/RELEASE_NOTES_1.1.md` (still DRAFT: `-Publish` refuses it until its comment is
  deleted).

## 5. Known issues for the notes

From BACKLOG.md section 1 and the flaky tests; the player-facing ones are in the notes' Known issues already.

- Arcane Dimensions: experimental; sky and light flicker in VR when turning; the hub's lower slipgates go nowhere.
- Armed humanoids: a knocked-down one aiming high may pull its arm in.
- Flat screen: wrists bend sharply on some holds; a gun's end can poke a few cm into a wall at the limit.
- Multiplayer: untested with the 1.1 systems (the menu says so).
- Embedded blades: heavy monsters, saves, level change and multiplayer unproven (`vr_embed_test` 14-20 written, 7, 13
  and 21 to recheck).
- Not player-facing (tests only): ragdoll_stagger's backpedal, ragdoll_climb at seed 8, the random-run cases
  (`vr_systems_test 15`, ragdoll_ragtest's catch, reload), `vrtutorial_playtest.py` room 8, agrunt 16 / 3 / 91 / 92,
  stealth props_round, vrtrailer, r2m1's inherited monster count.

## 6. The checklist

- [x] Sorted for the release (CHECKLIST_REORG.md, "2026-11-14"): the [New] section placed, near-duplicates merged,
  one short line each, 94 -> 68 items in 10 sections (3 flat-screen, 7 VR), 48 with a Test; no ticked line was left
  to retire (the 11-13 sort retired them; no voice notes since 11-12).
- [x] New scenes `run_pickups`, `run_setup_newgame`, `ad_tears_window` (each run once headless: SCNRUN OK);
  `checklist_tests_check.py` 61 passed, 0 failed; `vr_scenario validate` 132 files, 0 with errors, 48 tags, 0
  missing; `vr_checklist` 68 items, 10 sections.
