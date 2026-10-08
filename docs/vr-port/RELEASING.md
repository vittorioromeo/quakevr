# Making a release

One script builds, checks, packages and publishes a release of Quake VR: Unleashed:
`Misc\release\make_release.ps1` (PowerShell, Windows). It uses the branch that is checked out (no branch name is
written in it), pushes only the release's tag, and creates the GitHub release as a **draft** unless told otherwise.
Everything it writes goes under `out\release\<version>\` (git-ignored).

```
powershell -ExecutionPolicy Bypass -File Misc\release\make_release.ps1 -DryRun     # checks and plan only
powershell -ExecutionPolicy Bypass -File Misc\release\make_release.ps1             # build + check + package, nothing online
powershell -ExecutionPolicy Bypass -File Misc\release\make_release.ps1 -Publish    # ... + tag v<VERSION> + draft GitHub release
powershell -ExecutionPolicy Bypass -File Misc\release\make_release.ps1 -Version 1.0.0 -BumpVersion -DryRun   # a new version
```

The version released is the repository's `VERSION` file ("Versions" below). From Git Bash,
`bash Misc/release/make_release.sh ...` runs the same script.

## Test a release locally

Before publishing, install the release the way a player gets it, without GitHub or your site:

1. **Build a local test release** (the same build, checks and package as a real one; nothing online, no tag):
   `powershell -ExecutionPolicy Bypass -File Misc\release\make_release.ps1 -Local -RunInstaller -QuakeDir <Quake>`
   It writes `out\release\<v>-local\`, whose `latest.json` points at `http://127.0.0.1:8517/<file>` (`-LocalPort` to
   change it). Never upload those assets. Its `hdtextures` still points at the hosted pack on GitHub ("Support files"
   below: the local server does not have it), so ticking HD textures downloads the real 0.6 GB; `-Textures <zip>`
   serves a copy locally instead, `-NoTextures` leaves it out (the installer then downloads its built-in pack, the same hosted zip).
2. With `-RunInstaller` it then runs `Misc\release\test_local_release.ps1`: a small server (`qvr-setup serve`, its own
   window, 127.0.0.1 only) serves `assets\`, and the built `QuakeVR-Setup.exe` starts with
   `--feed http://127.0.0.1:8517/latest.json --sandbox %TEMP%\QuakeVR-test-<v>-<time>`. A yellow **TEST FEED /
   SANDBOX** bar is on every page and the title says `[TEST]`.
3. **Install** as a player would: the download goes through the real path (HTTP, resume, SHA-256). The sandbox keeps
   everything: the game in `<sandbox>\QuakeVR` (its config and saves in `QuakeVR\quakevr`), the shortcuts in
   `<sandbox>\_shortcuts\Desktop` and `\Programs`, the downloads in `_downloads`. No Apps & Features entry, nothing on
   your desktop or Start menu, the VC++ runtime only checked. Your Quake is detected and read, never written; HD
   textures, the first-start relight and Play are the real ones.
4. **Play** from the last page (or a shortcut in `_shortcuts\Desktop`).
5. **Again, without rebuilding**: `Misc\release\test_local_release.ps1` (a new sandbox), `-Sandbox <dir>` (an update of
   that one), `-DropAfter 5000000` (the server cuts each file's first download: the installer must resume it),
   `-Cli` (no window: `qvr-setup install --feed ... --sandbox ...` then `verify`). Stop the server by closing its window
   or with `test_local_release.ps1 -StopServer` (it also stops by itself after 4 hours).
6. **Throw it away**: delete the sandbox folder (nothing is outside it) and, when done, `out\release\<v>-local`.

The installer takes the same feed from `QVR_SETUP_FEED` (one URL, or several separated by `;`) and `qvr-setup` too
(`feed`, `install --feed`).

## Support files

The HD texture pack and ericw-tools' zips are hosted once, on their own GitHub release
[`assets-2026-10-08`](https://github.com/vittorioromeo/quakevr/releases/tag/assets-2026-10-08) (not marked latest),
and every game release links them instead of uploading them again. `Misc\release\support_assets.json` lists that
release's tag, its URL template and each file's size and SHA-256 (as GitHub reports them):

| Key | File | Used by |
|---|---|---|
| `hdtextures` | `quakevr-hq-textures-png-2026-10-03.zip` (614,919,925 bytes) | latest.json's `hdtextures` component (the installer's HD textures), and the installer's built-in copy of it (below) |
| `ericw_source` | `ericw-tools-2.0.0-alpha11-src.zip` (86,236,331 bytes) | the release notes' "Source of ericw-tools' light.exe" link (GPL-3: the package ships `light.exe`) |
| `ericw_win64` | `ericw-tools-2.0.0-alpha11-win64.zip` (27,503,991 bytes, = ericw's own download) | the in-game Download ericw-tools' first mirror (`vr_relight_tool.cpp` pins it separately) |

- By default the script writes latest.json's `hdtextures` from that file (the hosted URL first, then each `-UrlBase`
  that is not GitHub's or local, with the support tag), and links the source zip from the release notes. Before a
  build (not `-DryRun`, not `-Local`) it reads the support release from GitHub's API (each file's size and `digest`)
  and sends a HEAD request to each URL: nothing is downloaded. A mismatch stops `-Publish`.
- `-Textures <zip>` / `-EricwSource <zip>` upload a copy with this release instead (overrides); `-NoTextures` leaves
  the `hdtextures` component out (the installer then falls back on its built-in pinned pack, below).
- A new texture pack: create a new support release (`assets-<date>`) with the new zip, and update
  `support_assets.json` (tag, file, size, sha256). Never replace a file on an existing support release: old
  latest.json files, the game and the installer pin its hash.
- **The installer's built-in pack.** The installer also pins the pack itself (`BuiltInComponents.HdTextures` in
  `Installer/src/QuakeVR.Installer.Core/Packaging/ReleaseFeed.cs`: file, size, SHA-256 and its URLs, the support
  release first, then `textures-2026-10-03` where it was first published). It is used whenever latest.json cannot be
  read or names no `hdtextures`; a latest.json that names one always wins. To bump the pack: after updating
  `support_assets.json`, change `HdTextures()` the same way (file, size, sha256; the new `assets-<date>` URL first,
  the older releases' URLs may stay after it only if they serve the same file), rebuild the installer and run the
  self-test: its "HD textures: no feed" test compares the pinned pack with `support_assets.json` (file, size, SHA-256,
  tag of the first URL) and fails on any difference. `qvr-setup install --package <zip> --hd --no-feed --dry-run`
  prints the pack and URLs a build would download. Installers already released keep their old pin: never remove
  a file from a support release.

## Which release is Latest

**Rule: a game release (`v<version>`) is always marked Latest on GitHub; an asset or texture release
(`assets-*`, `textures-*`) never is.** `https://github.com/vittorioromeo/quakevr/releases/latest/download/latest.json`
is the installer's first feed and the game's update check's only one (Quake/vr/vr_update.cpp): it serves the Latest
release's `latest.json`. A support release marked Latest has no `latest.json`, so both get a 404 there: the game shows
no update notice, and the installer's HD textures broke this way once. When you create a support release, untick "Set as the
latest release" (`gh release create ... --latest=false`); if one was marked by mistake,
`gh release edit v<version> --repo vittorioromeo/quakevr --latest` puts the game release back.

The game's update notice reads `version` (compared with its `VERSION` as semantic versions: a prerelease is older
than its release) and `page` (the release's page, which make_release.py writes; the notice opens
`releases/latest` without one). Test it against a local release: `qvr-setup serve` (or `test_local_release.ps1`'s
server), then in the game `vr_update_url http://127.0.0.1:<port>/latest.json; vr_update_check_now; vr_update_status`
(an older local build: `vr_update_test_version 9.9.9` shows the notice without a feed).
`Misc\quakevr\update_notice_test.py` checks it with its own server.

## Step by step

1. **Commit and push** the branch you release from (`vr-ironwail` today, `master` later: the script reads the
   current branch and its upstream). The tree must be clean (tracked files): the build names its commit.
2. **Have the extra files at hand** (once):
   - Nothing for the HD textures or ericw-tools' source: they are hosted ("Support files" above).
   - A Quake folder (with `id1\pak0.pak`) for the smoke launch: `-QuakeDir <folder>`, or set `QVR_QUAKE_DIR`. Without
     one the launch is skipped with a warning. The paks are linked (or copied) into `out\release\<version>\checks\smoke`,
     never uploaded, and nothing is written into the Quake folder.
3. **Pick the version** ("Versions" below): `make_release.ps1 -Version 1.0.0 -BumpVersion` commits `VERSION` = 1.0.0
   (alone, as "Version 1.0.0"; the tree must be otherwise clean) and goes on; push that commit with the branch before
   `-Publish` (the script checks HEAD is on the upstream). Or edit `VERSION`, commit and push it yourself. A `-Version`
   other than `VERSION`'s without `-BumpVersion` stops the script; without `-Version` it releases `VERSION`'s.
4. **Dry run**: `make_release.ps1 -DryRun` checks everything below and prints the plan, without
   building or calling `gh`. Fix any `PROBLEM` line.
5. **Build and check** without publishing: `make_release.ps1 -QuakeDir <Quake>`. It
   takes a few minutes; read the summary (also in `out\release\1.0.0\PUBLISH.txt`) and, if you like, edit
   `out\release\1.0.0\release-notes.md`, then pass it back with `-Notes` in the next step.
6. **Publish (draft)**: the same command with `-Publish` (and `-Notes out\release\1.0.0\release-notes.md` if you edited
   them). It builds again from scratch into a fresh folder (the previous one is moved aside to `1.0.0.old-<time>`),
   tags `v1.0.0`, pushes **only the tag**, and creates a draft release with the assets.
7. **Check the draft** on https://github.com/vittorioromeo/quakevr/releases and publish it there (or
   `gh release edit v1.0.0 --repo vittorioromeo/quakevr --draft=false --latest`). `-NoDraft` (or `-Draft:$false` from PowerShell itself) skips the draft.
   The installer's first feed, `https://github.com/vittorioromeo/quakevr/releases/latest/download/latest.json`, serves
   the new release only once it is published and not a prerelease.
8. **Check online**: `dotnet run --project Installer\src\QuakeVR.Installer.Cli -- feed --url https://github.com/vittorioromeo/quakevr/releases/latest/download/latest.json`
   (the installer's only feed: the vittorioromeo.com one was dropped on 2026-10-08) prints the version and the package's size; then run the released `QuakeVR-Setup.exe` with no local package.
   the new release only once it is published, not a prerelease, and marked Latest (always mark a game release Latest:
   "Which release is Latest" above). The games already installed then show their update notice within the hour.
8. **Upload `latest.json` to your site**: `out\release\1.0.0\assets\latest.json` to
   `https://vittorioromeo.com/quakevr/latest.json` (the installer's second feed; the `/quakevr/` folder must exist).
   It is the same file as the release's asset; its download addresses are the GitHub release's own files. For a mirror
   on the site too, upload the other assets (e.g. to `/quakevr/releases/v1.0.0/`) and build with
   `-UrlBase "https://github.com/vittorioromeo/quakevr/releases/download/{tag}/{file}","https://vittorioromeo.com/quakevr/releases/{tag}/{file}"`
   before publishing, so latest.json lists both.
9. **Check online**: `dotnet run --project Installer\src\QuakeVR.Installer.Cli -- feed --url <feed>` for both feeds
   prints the version and the package's size; then run the released `QuakeVR-Setup.exe` with no local package.

For the first release: the tags `v0.8.0` to `v0.8.2` exist only in your local repository, so the generated notes
would list the ~1900 commits since `v0.8.2` (cut at 150): write those notes yourself and pass them with `-Notes`.

## What the script does

| Step | What |
|---|---|
| Preconditions | the version (`-Version`, default `VERSION`'s) is `x.y.z` or `x.y.z-suffix` (a prerelease) and `VERSION`'s (else `-BumpVersion` commits it, refusing an older one); the tree is clean; HEAD is on the current branch's upstream (fetched first); the tag `v<version>` is not on another commit, here or on the remote; the remote is `-Repo` (default `vittorioromeo/quakevr`, the installer's feeds); `gh` is installed and logged in, and has no release of that tag; MSBuild with the ClangCL toolset (vswhere), the .NET SDK of `Installer\global.json`, Python, fteqcc (`-Fteqcc`, `FTEQCC`, `QC\fteqcc64.exe`, the author's copy, `PATH`). Without `-Publish`/`-PushTag` the ones only publishing needs (upstream, gh, ericw source) are warnings; `-DryRun` turns every problem into a warning |
| Source checks | `check_statics.py`, `check_qc_precedence.py`, `fgdgen.py --check` (as `build.sh`) |
| Engine + QuakeC | `MSBuild ironwail.sln` Release x64 (incremental; `-Rebuild` for a full one) with `/p:QvrReleaseVersion=<version>` and the fteqcc found (the build compiles `QC\progs.src`) |
| Package | `Windows\package-quakevr.ps1 -Dist out\release\<v>\package\QuakeVR -NoZip -Version "<v> (<date> <hash>)"`: the allowlist (tracked files under `quakevr\`, less development data, plus progs.dat), the engine files, the relighting tools, ericw-tools' light.exe, `manifest.json` |
| Installer | `dotnet publish` of `QuakeVR.Installer`, Release, win-x64, self-contained, single file (native libraries inside, compressed), `/p:Version=<version>`; fails if anything but `QuakeVR-Setup.exe` (and its .pdb) is left beside it; then the installer's self-tests (`tests\QuakeVR.Installer.SelfTest`) |
| Assets | `Misc\quakevr\make_release.py`: `QuakeVR.zip` (zipped from the package after checking every file against the manifest), `QuakeVR-Setup.exe`, `-Textures` / `-EricwSource` copies and `-Assets` if given, and `latest.json` (schema 1, the installer's `ReleaseFeed`; `hdtextures` the hosted pack of `support_assets.json` by default) |
| Checks | the zip holds exactly `package-quakevr.ps1 -DryRun`'s list (and none of id's files: no `id1/`, `hipnotic/`, `rogue/`, `pak*.pak`, `gfx.wad`); `qvr-setup feed --file latest.json --assets <folder>` parses it as the installer does and checks each file's size and SHA-256; the packaged `QuakeVR-Setup.exe` installs the zip offline with its off-screen harness into `checks\setup` (no registry, no real shortcuts) and `qvr-setup verify` checks the install; the packaged `ironwail.exe`, unpacked from the zip, loads `start` with the mock headset (hidden window, `vr_mock_fast`), quits cleanly and names the build in its console |
| Notes | `-Notes <file>`, or the commit subjects since the previous `v*` tag (the last 60 commits for the first release), plus a table of the files with their sizes and SHA-256 and the SmartScreen note; `SHA256SUMS.txt` is an asset too |
| Publish | `-PushTag` or `-Publish`: an annotated tag `v<version>` on HEAD (reused when it is already there), pushed alone (`git push <remote> refs/tags/v<version>`); `-Publish`: `gh release create v<version> <assets> --verify-tag --draft` (`--prerelease` for `x.y.z-suffix`; `--latest` with `-NoDraft`) |

`out\release\<version>\` then holds: `assets\` (exactly what is uploaded), `release-notes.md`, `PUBLISH.txt` (the
summary and what is left to do), `logs\` (each tool's output), `package\QuakeVR\` and `installer\` (the build outputs)
and `checks\` (the self-tests' scratch, the harness's install and pages, the smoke folder).

## Versions

One source of truth: the `VERSION` file at the repository's root, one line, `MAJOR.MINOR.PATCH` (semantic versioning),
a prerelease as `MAJOR.MINOR.PATCH-beta.N` (or `-rc.N`). Every build reads it:

- the engine (`Windows\VisualStudio\quakevr.props`, QvrBuildVersion, into `qvr_buildver.h`; `Quake\vr\vr.cmake` and
  `vr.mk` for the CMake and Makefile builds): `VR_Version` ("0.9.0", the menus' corner label) and `VR_BuildVersion`
  (the console's "Quake VR" line, the Advanced VR Options page's last line, crash reports, saves);
- the installer (`Installer\Directory.Build.props`): `QuakeVR-Setup.exe`'s file and assembly version (its window's
  footer, "Installer 0.9.0");
- a package made by hand (`Windows\package-quakevr.ps1`, `write-package-manifest.ps1`): `manifest.json`'s version;
- the release script: the version it releases and tags (`v<VERSION>`).

**Dev and release builds.** Every build is a dev build except the release script's: `VR_BuildVersion` reads
`0.9.0-dev (2026-10-07 afd53921)` (the commit's date and short hash, `-dirty` with uncommitted changes), and the menus'
corner label "Quake VR: Unleashed - v0.9-dev" (the "-dev" fainter). The release script builds with
`/p:QvrReleaseVersion=<VERSION>` (the build refuses one that is not `VERSION`'s): `0.9.0 (2026-10-07 afd53921)` and
"v0.9"; `manifest.json` and `latest.json` carry the same text, the installer's version is `0.9.0`, and the annotated
tag `v0.9.0` records it. Between releases `VERSION` names the last release (or the one being prepared): a dev build's
`-dev` plus its commit tell it apart.

**The corner label** (`vr_menu_version`, VR Settings > Advanced VR Options > HUD and Menus > Menu: "Version Label")
shows `vMAJOR.MINOR` while PATCH is 0 ("v0.9", "v1.0") and the whole version otherwise ("v0.9.1", "v1.0.0-beta.1").

**When to bump** (in the release's own commit, `-BumpVersion`):

- **PATCH** (0.9.0 to 0.9.1): fixes only: crashes, bugs, balance tweaks, docs; nothing a player must relearn and no
  setting renamed; saves and configs keep working.
- **MINOR** (0.9.x to 0.10.0): new features, new settings or pages, changed defaults, new content; old saves and configs
  still load (or are migrated).
- **MAJOR** (0.x to 1.0.0, then 1.x to 2.0.0): 1.0.0 is the first release called finished; after it, a break: saves or
  configs that no longer load, a removed or reworked system, a new minimum (a runtime, a Quake data set).
- **Prereleases** (`1.0.0-beta.1`, `-beta.2`, `-rc.1`): test builds of the version named; GitHub marks them as
  prereleases. The final release drops the suffix (1.0.0-rc.2 to 1.0.0).

The first Ironwail-based release is **0.9.0** (2026-10-07): the old Quake VR's tags reached v0.8.2, and Unleashed
continues from it below 1.0 until it is called finished.

## What an update replaces

Setup started over an install opens on the Update screen (`qvr-setup update` from the console; INSTALLER.md, section 5,
"Update mode"): a newer release **updates** (1.0.0 to 1.0.1), the same or an older one **repairs** (never a downgrade).

- **Program files are exactly `manifest.json`'s files** (plus the HD textures, the VisPatch data and Setup's copy, which
  install.json records by component). An update hashes each one in the install and copies only those that differ from
  the new package; files the old release shipped and the new one does not are removed when unchanged. Everything else
  in the install folder is the player's and is never opened: configs, saves, screenshots, voice notes, Map Library maps,
  relit maps, checklist ticks, caches.
- **Marking a file as a program file** means shipping it: `Windows\package-quakevr.ps1`'s allowlist (git-tracked files
  under `quakevr\`, less `$devOnly`, plus `$generated`; the engine's exe, DLLs, pak and pdb; `$toolFiles`; ericw-tools'
  files) writes it into `manifest.json`. So:
  - **never ship a file the game writes** (the paths in `quakevr\.gitignore`: `ironwail.cfg`, `*.sav`, `checklist_ticks.txt`,
    `retro_overrides.txt`, `bodycal\`, `relit_custom\`, `cache\`...): it would become a program file, replaced by every
    update. Settings the release changes go in the shipped defaults (`vr_defaults.cfg`, `quakevr.cfg`), which the player
    is not meant to edit;
  - a shipped file the player edited anyway, or the player's own file at a path a new release starts shipping, is copied
    into `<QVR>\backups\<date> update\` (listed with its SHA-256 in `backup.json`) before it is replaced;
  - a file dropped from the allowlist is removed from installs at their next update (kept when the player changed it).
- **The relight's inputs** (`MaintenancePlanner.IsRelightInput`): `quakevr\tools\ericw-tools\*`,
  `quakevr\tools\vispatch\*` and `quakevr\relight_textures.cfg`. A release that changes one of them (or a new HD pack)
  makes the update ask the game for the first-start relight again (players who chose it at install); otherwise none. If a
  release changes the relight in the engine itself (light's options, the settings hashed in `.relight` notes), the game's
  batch notices per map when the player relights; add a file to the list above only if the installer must force it.
- **HD textures:** install.json records the pack's SHA-256; an update downloads the pack only when the target's
  (`latest.json`'s `hdtextures`, else the built-in pin) differs. Bumping the pinned pack (above, "Support files") is what
  makes updates fetch it.
- **Versions:** the order is MAJOR.MINOR.PATCH, then the prerelease (`1.0.0-rc.1` < `1.0.0`; `beta.9` < `beta.10`), then
  the build's date: `manifest.json`'s version text must keep the `x.y.z (<date> <hash>)` form the script writes.
- **Check before publishing:** install the previous release into a sandbox, then
  `qvr-setup update --sandbox <dir> --package out\release\<v>\package\QuakeVR --no-feed --dry-run` lists every program
  file the update adds, replaces or removes, what it backs up, and whether it asks for the relight.

## Safety

- `-Local` builds a test release into `out\release\<version>-local` whose `latest.json` names 127.0.0.1: it never
  fetches, calls `gh` or tags, and is refused with `-Publish`/`-PushTag`.

- `-DryRun` builds, tags and calls `gh` for nothing. Without `-Publish` or `-PushTag` nothing leaves the PC.
- Only the tag is ever pushed, never a branch. A re-run with the same version reuses a tag already on HEAD and stops
  if the tag is on another commit or the GitHub release exists (delete or edit it on GitHub first).
- Nothing is deleted: an earlier `out\release\<version>\` is moved aside to `<version>.old-<time>` (delete those
  yourself when done).
- `-AllowDirty` builds from uncommitted changes, for testing the script only (refused with `-Publish`/`-PushTag`).
- `ironwail.pdb` ships in the package on purpose (crash reports name the functions with it beside the exe:
  `package-quakevr.ps1`); the installer's .pdb is not uploaded.
