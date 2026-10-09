# Before the first release: Vittorio's to-do

Short list. The full procedure is in [RELEASING.md](RELEASING.md).

## Decide

- [x] **Version:** `VERSION` says `1.0.0` (decided 2026-10-08: the first release is 1.0.0). The menus' corner label reads "v1.0" (the `v` prefix is his commit; RELEASING.md, "The corner label").
- [ ] **Release branch:** make `vr-ironwail` the main branch now or later. The script follows whatever branch you're on.
- [x] **Second feed:** dropped (Vittorio, 2026-10-08): the installer reads only GitHub's `latest.json`.
- [ ] **HD texture pack:** offered by default from the hosted `assets-2026-10-08` release (no upload); `-NoTextures` to leave it out.
- [ ] **Debug symbols:** `ironwail.pdb` is inside the game zip. Keep it there, or move it to a separate symbols zip.
- [x] **Discord icon:** keep the hand-drawn one (decided 2026-10-09).
- [x] **Dawn of the Machine (MG3):** unlocked and ready, single player only like Dopa and MG1 (commit 405e4213; revert it to undo).

## Prepare once

- **ericw-tools source zip** (GPL; required because `light.exe` ships): nothing to prepare, it is hosted on the `assets-2026-10-08` release; the release notes link it (RELEASING.md, "Support files"). Nothing to set.
- [ ] **Smoke-test Quake folder:** nothing to set: the script finds it as the installer does (Steam/GOG/Epic); `QVR_QUAKE_DIR` or `-QuakeDir` only for another one.
- [ ] **GitHub CLI:** `gh auth status` shows you logged in (the dry run checks it).
- [ ] **Test worktree:** for `-RunTests`, a kit worktree at the commit you release (`bash <kit>/new_agent.sh release <commit>`), passed as `-TestAgent release`.
- [x] **Your site:** nothing needed (the second feed was dropped 2026-10-08).

## Check in VR before tagging

- [ ] Work through the in-game checklist (VR menu > Checklist), at least the "New this week" section.
- [ ] Record new melee takes. Without them the melee regression check (`eval.sh`) can't run.
- [ ] Review the installer: the Statement wording, the Thanks/donation text, and every page at your usual scaling.
- [ ] Do a fresh install with the built `QuakeVR-Setup.exe` on a clean folder: detection, install, first start (relight), play.
  One command: `Misc\release\make_release.ps1 -Local -RunInstaller` (a local server plus a sandbox in `%TEMP%`; RELEASING.md, "Test a release locally").

## Release

The one command and what it does: [RELEASING.md](RELEASING.md), "One command".

1. [ ] Commit and push. The working tree must be clean.
2. [ ] Notes: `Misc\release\make_release.ps1 -DraftNotes` drafts `out\release\<v>\release-notes.md` by area. Your `v0.8.x` tags exist only locally, so the draft covers about 1,900 commits: rewrite it, then delete its first (DRAFT) line.
3. [ ] Plan: `Misc\release\make_release.ps1 -Publish -Final -RunTests -TestAgent <name> -DryRun` (later releases: add `-Bump patch|minor|major`). Fix every `PROBLEM` line.
4. [ ] Release: the same without `-DryRun`. Tests, build and checks, the tag, the release published and marked Latest, the Latest guard, the online check (GitHub's latest.json read by the installer and a sandboxed install through it). If it stops, it prints how to go on.

## Known and accepted for this release

- **Multiplayer:** untested; the menu shows a notice saying so.
- **SmartScreen:** there's no code-signing certificate, so Windows shows "Windows protected your PC" until the installer builds reputation. The release notes explain this.
- **Prereleases:** a version like `x.y.z-beta.N` becomes a GitHub prerelease, which the installer's GitHub feed doesn't serve. Testers need a direct link.
