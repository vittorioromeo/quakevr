# Before the first release: Vittorio's to-do

Short list. The full procedure is in [RELEASING.md](RELEASING.md).

## Decide

- [ ] **Version:** `VERSION` says `0.9.0`. Keep it, or bump to `1.0.0`.
- [ ] **Release branch:** make `vr-ironwail` the main branch now or later. The script follows whatever branch you're on.
- [ ] **Second feed:** keep `vittorioromeo.com/quakevr/latest.json` as a fallback, or drop it from the installer. GitHub alone is enough.
- [ ] **HD texture pack:** ship it in this release (`-Textures <zip>`) or not.
- [ ] **Debug symbols:** `ironwail.pdb` is inside the game zip. Keep it there, or move it to a separate symbols zip.
- [ ] **Discord icon:** the installer's icon is hand-drawn. Keep it, or swap in Discord's official asset.
- [ ] **Dawn of the Machine (MG3):** still shows "detected, not yet supported". Ship as is, or wait for its last steps (performance pass, route sweep, readiness flip).

## Prepare once

- [ ] **ericw-tools source zip** (GPL; required because `light.exe` ships): `git clone --recursive --branch 2.0.0-alpha11 https://github.com/ericwa/ericw-tools`, zip it as `ericw-tools-2.0.0-alpha11-src.zip`, then set `QVR_ERICW_SRC` to its path.
- [ ] **Smoke-test Quake folder:** set `QVR_QUAKE_DIR` to your Quake folder (the one with `id1\pak0.pak`).
- [ ] **GitHub CLI:** check `gh auth status` shows you logged in.
- [ ] **Your site** (only if you keep the second feed): create the `/quakevr/` folder.

## Check in VR before tagging

- [ ] Work through the in-game checklist (VR menu > Checklist), at least the "New this week" section.
- [ ] Record new melee takes. Without them the melee regression check (`eval.sh`) can't run.
- [ ] Review the installer: the Statement wording, the Thanks/donation text, and every page at your usual scaling.
- [ ] Do a fresh install with the built `QuakeVR-Setup.exe` on a clean folder: detection, install, first start (relight), play.

## Release

1. [ ] Commit and push. The working tree must be clean.
2. [ ] Dry run: `Misc\release\make_release.ps1 -DryRun`. Fix every `PROBLEM` line.
3. [ ] Build and check: `Misc\release\make_release.ps1`. Read `out\release\<v>\PUBLISH.txt`.
4. [ ] Write `out\release\<v>\release-notes.md` by hand. Your `v0.8.x` tags exist only locally, so generated notes would cover about 1,900 commits.
5. [ ] Publish as a draft: `Misc\release\make_release.ps1 -Publish -Notes out\release\<v>\release-notes.md`. This pushes only the tag.
6. [ ] Review the draft on GitHub, then publish it.
7. [ ] Optional: upload `out\release\<v>\assets\latest.json` to `vittorioromeo.com/quakevr/latest.json`.
8. [ ] Check the feed: `dotnet run --project Installer\src\QuakeVR.Installer.Cli -- feed --url https://github.com/vittorioromeo/quakevr/releases/latest/download/latest.json`.

## Known and accepted for this release

- **Multiplayer:** untested; the menu shows a notice saying so.
- **SmartScreen:** there's no code-signing certificate, so Windows shows "Windows protected your PC" until the installer builds reputation. The release notes explain this.
- **Prereleases:** a version like `x.y.z-beta.N` becomes a GitHub prerelease, which the installer's GitHub feed doesn't serve. Testers need a direct link.
