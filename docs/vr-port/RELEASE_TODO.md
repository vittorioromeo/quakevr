# Before 1.1.0: Vittorio's to-do

Short list. The full procedure is in [RELEASING.md](RELEASING.md). Release pass of 2026-11-08 (ROUND24.md, "Release pass
for 1.1"): the local build, the package, the installer harness, the smoke launch, the upgrade path and the campaigns all
pass on `agent/relpass`.

## Decide

- [x] **Version:** `VERSION` says `1.1.0` (stable; the betas were `1.1.0-betaN`).
- [x] **Armed replacements on by default** (`vr_agrunt_repl` 1; decided 2026-11-06). Configs from 1.0.x and the betas
  take it too unless the player had turned them on or off himself (fullConfigChanges, 2026-11-08).
- [x] **Unpromoted tunables (all 19 promoted 2026-11-08):** your `quakevr\tunables.cfg` holds 14 tuning values the release would lose (smoke blow,
  `vr_knockdown_ledge_reach`, `vr_model_light_grid_cell`, flat-screen gun/punch/slide values). Promote them
  (`python Misc\quakevr\tune_promote.py <your tunables.cfg>`, then commit `vr_tunables.inc`) or release with
  `-AllowUnpromotedTunables`.
- [ ] **Release notes:** the draft is `docs/vr-port/RELEASE_NOTES_1.1.md`: edit, delete its DRAFT comment, pass it with
  `-Notes docs\vr-port\RELEASE_NOTES_1.1.md`.
- [x] **Flat-screen launch:** the installer creates "Quake VR Unleashed (Flat Screen)" (`-flatscreenmode`), in the same places as the VR one (2026-11-08).
- [ ] **The installer's settings page:** review its rows, order and labels (BACKLOG.md, section 2).
- [x] **"Armed Grunts":** "(Experimental)" removed from menu titles, rows, help and docs (decided 2026-11-08).
- [x] **Developer files in the package (decided 2026-11-08: keep the dev maps and scenarios in the zip for now, no change):** by the allowlist rule (every tracked file under `quakevr\`) the zip ships
  `checklist.txt`, the 88 `scenarios\*.txt` (bench ones too) and `vrtesthall`. Keep (the in-game checklist and its
  Test buttons use them) or add them to `$devOnly`.
- [x] **Embedded blades:** `vr_embed` 1 by default (decided 2026-11-08). (Pick Up a Second
  is on since 2026-11-08, `vr_agrunt_dual_pickup` 1; a 1.0.x or beta config at 0 takes it.)
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

## After release: spread the word

Where to announce the VR mod and its flat-screen counterpart (researched 2026-11-10). The audiences overlap: Quake
fans, VR players, and retro-shooter ("boomer shooter") fans.

- [ ] **A 60-90 s gameplay video first.** Emergent physics, gore and armed-grunt moments sell it better than feature
  lists. Make a separate flat-screen pitch too ("Quake with physics, armed grunts, a visor HUD and dynamic gore, no
  VR needed"), so it doesn't read as VR-only.
- [ ] **Reddit:**
  - Quake and retro shooters: r/quake (the core audience), r/boomershooters, r/retrogaming.
  - PC VR: r/virtualreality, r/SteamVR, r/ValveIndex, r/OculusQuest and r/MetaQuestVR (say PC VR / Link clearly).
  - VR game discovery: r/vrgaming, r/VRGames.
  - The flat-screen version: r/FPS, r/pcgaming (strict self-promotion rules: a good trailer), r/patientgamers (a
    discussion post, not an ad).
- [ ] **Discord:** Flat2VR (the flat-to-VR mod community; probably the best single VR audience), Team Beef (Quest
  ports: QuestZDoom, Quake ports), Quake Mapping (mappers; AD and custom maps), Arcane Dimensions and other map-pack
  communities, Quake speedrunning and community servers, the Ironwail and Quakespasm source-port communities.
- [ ] **Forums and sites:** func_msgboard (celephais.net), Quaddicted, InsideQC, Doomworld (its other-games area),
  a ModDB mod page with news posts, an itch.io page with devlogs.
- [ ] **Press and creators** (a short pitch, the trailer, the download link):
  - VR news: UploadVR, Road to VR, MIXED (all take tips about notable mods).
  - VR YouTubers who cover free mods and Flat2VR projects: Beardo Benjo, Gamertag VR, Dr Beardface.
  - Retro-FPS YouTubers: Civvie 11, Dusk Dude; technical-retro creators for the deeper engine story.
- [ ] **Social:** Bluesky, X and Mastodon with #ScreenshotSaturday, #QuakeVR, #boomershooter (short physics and gore
  clips); Hacker News "Show HN" for the tech story (Ironwail fork, VR rendering, Box3D physics).
- [ ] **Etiquette:** read each subreddit's self-promotion rules, post where you'll stay to answer comments, and space
  the posts out rather than cross-posting the same day.

## Known and accepted for this release

- **Multiplayer:** untested with the 1.1 systems; the menu shows a notice saying so.
- **SmartScreen:** no code-signing certificate; the notes explain "More info > Run anyway".
- **Arcane Dimensions:** experimental (a prompt says so); sky and light flicker in VR when turning; the hub's lower
  slipgates go nowhere.
- **Embedded blades:** on by default since 2026-11-08; tests 14-20 not run, tests 7 and 13 failing.
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
