# Making a release

One script builds, checks, packages and publishes a release of Quake VR: Unleashed:
`Misc\release\make_release.ps1` (PowerShell, Windows). It uses the branch that is checked out (no branch name is
written in it). Everything it writes goes under `out\release\<version>\` (git-ignored). From Git Bash,
`bash Misc/release/make_release.sh ...` runs the same script. The version released is the repository's `VERSION` file
("Versions" below).

## Branches

Since 2026-10-10:

| Branch | What |
|---|---|
| `vr-ironwail` | staging: all work lands here, and releases are made from it |
| `master` | the release branch (GitHub's default): it moves only at releases, always to the commit of the last final release |
| `quakevr-old` | the original Quake VR, never touched |

A final release (`-Publish -Final`) fast-forwards `origin/master` to the released commit right after `gh release
create`: `git push origin "v<version>^{commit}:refs/heads/master"`, never forced (`-ReleaseBranch <name>` for another
branch). Prereleases, rehearsals, drafts and `-NoDraft` releases never move it (`PUBLISH.txt` gives the command for a
draft once it is published). Because it is never forced, `master` must be an ancestor of the commit released: the
checks stop the release **before building** when it is not (someone committed to `master` directly, e.g. on GitHub):
merge `origin/master` into `vr-ironwail`, push, release again; or `-NoReleaseBranch` to release without moving it and
move it by hand later. If `master` diverges between the checks and the push, the script stops with `STOPPED (master)`
after publishing and prints the merge to run instead ("If it stops"). `-CheckOnline` reports whether `master` holds the
release (read only) and prints the fast-forward when it is behind.

## One command

```
make_release.ps1 -DraftNotes -Bump patch                       # 1. the notes: out\release\<v>\release-notes.md, to edit
make_release.ps1 -Bump patch -Publish -Final -RunTests -DryRun  # 2. the plan: every command below, in order (nothing runs)
make_release.ps1 -Bump patch -Publish -Final -RunTests          # 3. the release, end to end
```

(`powershell -ExecutionPolicy Bypass -File Misc\release\make_release.ps1 ...`; `-Bump minor`/`major`, or
`-Version x.y.z -BumpVersion`, or neither to release `VERSION` as it is.) Step 3 runs, stopping at the first failure:

1. **Checks** (nothing changed yet): the tree is clean, HEAD is on its upstream or ahead of it (not diverged), the tag
   and the GitHub release don't exist yet, `gh auth status`, the tools, the notes are edited (no DRAFT line), the test
   worktree holds this commit's files, `origin/master` is an ancestor of HEAD ("Branches"), the Quake for the smoke
   launch (below).
2. **Tests** (`-RunTests`, "Tests before publishing"): the headless suite, one test at a time; a failure stops here.
3. **VERSION** (with `-Bump`/`-BumpVersion`): `VERSION` = the new version, committed alone ("Version x.y.z"), locally.
4. **Build and check** ("What the script does"): engine, QuakeC, package, installer, their self-tests, the zip against
   the allowlist, latest.json, the packaged installer's install, the smoke launch.
5. **Push the branch** to its upstream when HEAD is ahead of it (the version commit): a fast-forward, never forced.
   Nothing leaves the PC before the build and its checks passed.
6. **Tag** `v<version>` (annotated) and push the tag alone.
7. **Publish**: `gh release create` with the assets and the notes, public and marked **Latest** (`-Final`; without it a
   draft).
8. **Release branch** ("Branches"): `git push origin "v<version>^{commit}:refs/heads/master"`, a fast-forward of
   `master` to the release, never forced (`-NoReleaseBranch` skips it).
9. **Latest guard** ("Which release is Latest"): `gh release list`: the new release must be the only Latest and no
   `assets-*`/`textures-*` release is; otherwise `gh release edit v<version> --latest`, listed again, reported.
10. **Online check**: `qvr-setup feed --url https://github.com/vittorioromeo/quakevr/releases/latest/download/latest.json
   --assets out\release\<v>\assets` (the installer's own reader: the version, each file's size and SHA-256 against
   this release's; retried 20 s apart while GitHub's "latest" lags, `-OnlineTries`), the served latest.json byte for
   byte, then a sandboxed install through that feed (`qvr-setup install --feed ... --sandbox
   out\release\<v>\checks\online-<time>\sandbox`, then `qvr-setup verify`; `-OnlineHd` adds the 0.6 GB HD textures,
   `-SkipOnlineInstall` leaves the install out). Nothing is written outside the sandbox.

`out\release\<v>\PUBLISH.txt` sums it up. `-DryRun` with the same options runs the read-only checks (including
`gh auth status`, `gh release view`, `gh release list`) and prints the numbered list of commands the real run would
run.

### The release notes

`-DraftNotes` (with the same `-Bump`/`-Version`) writes `out\release\<v>\release-notes.md`
(`Misc\release\draft_release_notes.py`): the commits since the last `v*` tag, filed by their words and files under
Weapons and reloading, AI and stealth, Teleporters, Menus and interface, Physics/props/ragdolls, Rendering and
performance, Installer and releases, Expansions and maps, Fixes, Other; each subject cut to a short bullet (its lead
clause), 30 per area (the rest counted with their hashes). Commits that only change tests, docs or the checklist, and
version bumps, are counted but not listed. Its first line is a `<!-- DRAFT ... -->` comment: edit the bullets, then
delete that line. `-Publish` reads that file (or `-Notes <file>`) and **refuses notes that still have the DRAFT line**
(`-AutoNotes` publishes the draft as it is). A run without `-Publish` drafts it too when it is missing. The body
uploaded is `release-body.md`: the notes plus the files' sizes and SHA-256.

### Tests before publishing

`-RunTests` runs `Misc\release\run_test_suite.py` before anything is built: the Misc\quakevr test scripts that give a
verdict (41: reloading, melee, carry and grabs, parry, throws, teleporter chase, stealth, the update notice, the OpenXR
runtime choice...; `python Misc\release\run_test_suite.py --list`), one at a time through the kit's headless mock
headset, on a **kit worktree** holding this commit's files: `-TestAgent <name>` (or `QVR_TEST_AGENT`; default this
checkout's name when it is one: `C:\OHWorkspace\qvr-agents\<name>`, made by the kit's `new_agent.sh <name> <commit>`).
The worktree is built first (kit `build.sh`). A test fails on a non-zero exit, a `FAIL` / `FAILURES n` / `n failed`
line, or its timeout; each test's output is in `out\release\<v>\tests\<name>.log`, the summary in `summary.txt`. Any
failure stops the release (nothing committed, built or pushed). `-AllowFlaky` lets the known-flaky ones (marked in the
runner's list: the teleporter chase, Quake's random AI) only warn, `-FlakyTests a,b` adds more; `-TestOnly <regex>`
runs a part. The whole suite takes over an hour. Alone: `python Misc\release\run_test_suite.py <agent> --build`.

### If it stops

Every failure prints `STOPPED (<stage>)` and what to run next. Nothing is ever deleted or forced.

| Stopped at | State | Go on with |
|---|---|---|
| checks, tests | nothing changed | fix it, the same command again |
| build | with a bump: the "Version x.y.z" commit, local only | fix it, then `-Version x.y.z` instead of `-Bump` (same other options); or drop the commit: `git reset --keep HEAD~1` |
| push | built and checked; the branch not pushed (upstream moved?) | pull/rebase, then `-Version x.y.z ...` (builds again) |
| tag | the branch pushed | `-Version x.y.z ...` (a tag already on HEAD is reused) |
| release | tag pushed; `gh release create` failed | a partial release on GitHub: delete it, or `gh release upload` the missing assets and `gh release edit v<x.y.z> --draft=false --latest`; then `-Version x.y.z -CheckOnline` |
| master | the release is published; `master` not moved | only behind (the push refused or failed): `git push origin "v<x.y.z>^{commit}:refs/heads/master"`; diverged: merge the release into it as printed (`git switch master; git pull --ff-only; git merge v<x.y.z>; git push origin master`, never forced); then `-Version x.y.z -CheckOnline` |
| online | the release is published (and `master` moved) | `make_release.ps1 -Version x.y.z -CheckOnline` (the Latest guard and the online check only) |

`-CheckOnline` checks `out\release\<version>\assets` (`-ReleaseDir` for another folder) against the feed
(`-FeedUrl`; with `-Local`, the local server's, no Latest guard; for a prerelease, its own feed, "Prereleases" below).

### Prereleases and rehearsals

A prerelease (`x.y.z-suffix`) is never GitHub's Latest, so `releases/latest/download/latest.json` (the installer's and
the update check's feed) never serves it; its own feed is
`https://github.com/vittorioromeo/quakevr/releases/download/v<version>/latest.json`.
`make_release.ps1 -Version 1.1.0-beta.1 -BumpVersion -Publish -NoDraft` publishes it as a prerelease (`-Final` refuses
one), then checks that it is a published prerelease and not Latest (`gh release list`; nothing edited) and runs the
online check against its own feed (step 10's checks). `-CheckOnline` does the same for a prerelease. Testers point the
installer at that feed (`QVR_SETUP_FEED`, `qvr-setup install --feed`). GitHub notifies the repository's release
watchers of a published prerelease too.

`-NoBranchPush` (prereleases only) never pushes the branch: the tag, pushed alone, carries its commit (the version
commit stays local). A **rehearsal** of the whole path (done 2026-10-10: build, checks, tag, upload, online check) from
a kit worktree's branch, then removed:

```
make_release.ps1 -Version 1.0.0-rehearsal.1 -BumpVersion -Publish -NoDraft -NoBranchPush -Notes <a "not a release" note>
gh release delete v1.0.0-rehearsal.1 --repo vittorioromeo/quakevr --yes --cleanup-tag
git tag -d v1.0.0-rehearsal.1; git reset --keep HEAD~1      # the local tag and the "Version" commit
```

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

**The Latest guard** does it after every `-Publish -Final` (and `-CheckOnline`): it lists the releases
(`gh release list --json tagName,isLatest`), and when the new game release is not the only Latest (an `assets-*` or
`textures-*` one, or an older game release, still is) it runs `gh release edit v<version> --latest`, lists them again
and reports it (the console and PUBLISH.txt). `-DryRun` shows what is Latest now and warns when it is a support release.

The game's update notice reads `version` (compared with its `VERSION` as semantic versions: a prerelease is older
than its release) and `page` (the release's page, which make_release.py writes; the notice opens
`releases/latest` without one). Test it against a local release: `qvr-setup serve` (or `test_local_release.ps1`'s
server), then in the game `vr_update_url http://127.0.0.1:<port>/latest.json; vr_update_check_now; vr_update_status`
(an older local build: `vr_update_test_version 9.9.9` shows the notice without a feed).
`Misc\quakevr\update_notice_test.py` checks it with its own server.

## Step by step

1. **Once:** `gh auth login` (`gh auth status` shows it; `-DryRun` checks it). The smoke launch's Quake is found by
   itself (the installer's Steam/GOG/Epic detection: `qvr-setup detect`, the folder it picks), or `-QuakeDir <folder>`
   / `QVR_QUAKE_DIR`; without one the launch is skipped with a warning. Its paks are linked (or copied) into
   `out\release\<version>\checks\smoke`, never uploaded; nothing is written into the Quake folder. For `-RunTests`, a
   kit worktree at the commit to release (`-TestAgent`, "Tests before publishing").
2. **Commit and push** the branch you release from (`vr-ironwail`, "Branches"; the script reads the current branch and
   its upstream, and moves `master` itself). The tree must be clean (tracked files): the build names its commit. Commits not pushed
   yet are pushed by `-Publish` (a fast-forward, after the checks).
3. **Pick the version** ("Versions" below): `-Bump patch|minor|major` computes it from `VERSION` (a prerelease bumps to
   the release it leads to: `1.1.0-rc.1` patch or minor is `1.1.0`); `-Version x.y.z -BumpVersion` names it. Either
   commits `VERSION` alone ("Version x.y.z"; the tree must be otherwise clean) after the tests, before the build.
   Without them the script releases `VERSION` as it is; a `-Version` other than `VERSION`'s without `-BumpVersion`
   stops it.
4. **Draft the notes**: `make_release.ps1 -DraftNotes -Bump patch`, edit `out\release\<v>\release-notes.md`, delete its
   DRAFT line ("The release notes"). For the first release (the ~1900 commits since the local-only `v0.8.2` tag)
   rewrite it rather than trim it.
5. **Dry run**: `make_release.ps1 -Bump patch -Publish -Final -RunTests -DryRun`: the read-only checks and the
   numbered list of commands. Fix every `PROBLEM` line (warnings in a dry run).
6. **Release**: the same without `-DryRun` ("One command"). It takes the tests' hour plus a few minutes; read
   `out\release\<v>\PUBLISH.txt` at the end. If it stops: "If it stops".
7. **Afterwards**: the games already installed show their update notice within the hour (they read the same
   latest.json). Run the released `QuakeVR-Setup.exe` once by hand with no local package if you like.

**Without `-Final`** (the older route, a draft first): `make_release.ps1 -Publish` creates a **draft**; check it on
https://github.com/vittorioromeo/quakevr/releases and publish it there (or `gh release edit v1.0.0 --repo
vittorioromeo/quakevr --draft=false --latest`), then `make_release.ps1 -Version 1.0.0 -CheckOnline` (the Latest guard
and the online check). `-NoDraft` publishes at once without the checks after it (a prerelease: with them, against its own feed). The installer's only feed,
`https://github.com/vittorioromeo/quakevr/releases/latest/download/latest.json`, serves a release only once it is
published, not a prerelease, and marked Latest. Without `-Publish` nothing leaves the PC (build and check only).

## What the script does

| Step | What |
|---|---|
| Preconditions | the version (`-Version`, default `VERSION`'s) is `x.y.z` or `x.y.z-suffix` (a prerelease) and `VERSION`'s (else `-BumpVersion`/`-Bump` commits it after the tests, refusing an older one); the tree is clean; HEAD is on the current branch's upstream or ahead of it (fetched first; ahead: pushed by `-Publish`/`-PushTag`, diverged: a problem); the tag `v<version>` is not on another commit, here or on the remote; the remote is `-Repo` (default `vittorioromeo/quakevr`, the installer's feeds); with `-Final`, the remote's `master` (`-ReleaseBranch`) is an ancestor of HEAD or missing ("Branches"); `gh` is installed and logged in (`gh auth status`, `-DryRun` too), and has no release of that tag (and, `-DryRun`/`-Final`, what is Latest now); the notes are not a DRAFT (`-Publish`); the test worktree (`-RunTests`); the Quake for the smoke launch (`-QuakeDir`, `QVR_QUAKE_DIR`, else `qvr-setup detect`); MSBuild with the ClangCL toolset (vswhere), the .NET SDK of `Installer\global.json`, Python, fteqcc (`-Fteqcc`, `FTEQCC`, `QC\fteqcc64.exe`, the author's copy, `PATH`). Without `-Publish`/`-PushTag` the ones only publishing needs (upstream, gh, ericw source) are warnings; `-DryRun` turns every problem into a warning |
| Tests | `-RunTests`: `run_test_suite.py <agent> --build` ("Tests before publishing"); then the version commit (`-Bump`/`-BumpVersion`) |
| Source checks | `check_statics.py`, `check_qc_precedence.py`, `fgdgen.py --check` (as `build.sh`) |
| Engine + QuakeC | `MSBuild ironwail.sln` Release x64 (incremental; `-Rebuild` for a full one) with `/p:QvrReleaseVersion=<version>` and the fteqcc found (the build compiles `QC\progs.src`) |
| Package | `Windows\package-quakevr.ps1 -Dist out\release\<v>\package\QuakeVR -NoZip -Version "<v> (<date> <hash>)"`: the allowlist (tracked files under `quakevr\`, less development data, plus progs.dat), the engine files, the relighting tools, ericw-tools' light.exe, `manifest.json` |
| Installer | `dotnet publish` of `QuakeVR.Installer`, Release, win-x64, self-contained, single file (native libraries inside, compressed), `/p:Version=<version>`; fails if anything but `QuakeVR-Setup.exe` (and its .pdb) is left beside it; then the installer's self-tests (`tests\QuakeVR.Installer.SelfTest`) |
| Assets | `Misc\quakevr\make_release.py`: `QuakeVR.zip` (zipped from the package after checking every file against the manifest), `QuakeVR-Setup.exe`, `-Textures` / `-EricwSource` copies and `-Assets` if given, and `latest.json` (schema 1, the installer's `ReleaseFeed`; `hdtextures` the hosted pack of `support_assets.json` by default) |
| Checks | the zip holds exactly `package-quakevr.ps1 -DryRun`'s list (and none of id's files: no `id1/`, `hipnotic/`, `rogue/`, `pak*.pak`, `gfx.wad`); `qvr-setup feed --file latest.json --assets <folder>` parses it as the installer does and checks each file's size and SHA-256; the packaged `QuakeVR-Setup.exe` installs the zip offline with its off-screen harness into `checks\setup` (no registry, no real shortcuts) and `qvr-setup verify` checks the install; the packaged `ironwail.exe`, unpacked from the zip, loads `start` with the mock headset (hidden window, `vr_mock_fast`), quits cleanly and names the build in its console |
| Notes | `-Notes <file>`, else `out\release\<v>\release-notes.md` (`-DraftNotes`, edited), else a draft made now (`draft_release_notes.py`, "The release notes"); `release-body.md` = the notes without the DRAFT line plus a table of the files with their sizes and SHA-256 and the SmartScreen note; `SHA256SUMS.txt` is an asset too |
| Publish | `-PushTag` or `-Publish`: the branch pushed to its upstream when HEAD is ahead (fast-forward); an annotated tag `v<version>` on HEAD (reused when it is already there), pushed alone (`git push <remote> refs/tags/v<version>`); `-Publish`: `gh release create v<version> <assets> --verify-tag --draft` (`--prerelease` for `x.y.z-suffix`; `--latest` instead of `--draft` with `-Final` or `-NoDraft`) |
| After publishing | `-Final`: `master` fast-forwarded to the tag ("Branches"; checked to be an ancestor of HEAD before building); `-Final` (or `-CheckOnline` alone, which only reports where `master` is): the Latest guard (`gh release list`, `gh release edit --latest` when needed), then the online check ("One command", step 10); a prerelease published with `-NoDraft`: checked not Latest, then the online check against its own feed ("Prereleases and rehearsals") |

`out\release\<version>\` then holds: `assets\` (exactly what is uploaded), `release-notes.md`, `PUBLISH.txt` (the
summary and what is left to do), `logs\` (each tool's output), `package\QuakeVR\` and `installer\` (the build outputs)
and `checks\` (the self-tests' scratch, the harness's install and pages, the smoke folder).

## Versions

One source of truth: the `VERSION` file at the repository's root, one line, `MAJOR.MINOR.PATCH` (semantic versioning),
a prerelease as `MAJOR.MINOR.PATCH-beta.N` (or `-rc.N`). Every build reads it:

- the engine (`Windows\VisualStudio\quakevr.props`, QvrBuildVersion, into `qvr_buildver.h`; `Quake\vr\vr.cmake` and
  `vr.mk` for the CMake and Makefile builds): `VR_Version` ("1.0.0", the menus' corner label) and `VR_BuildVersion`
  (the console's "Quake VR" line, the Advanced VR Options page's last line, crash reports, saves);
- the installer (`Installer\Directory.Build.props`): `QuakeVR-Setup.exe`'s file and assembly version (its window's
  footer, "Installer 1.0.0");
- a package made by hand (`Windows\package-quakevr.ps1`, `write-package-manifest.ps1`): `manifest.json`'s version;
- the release script: the version it releases and tags (`v<VERSION>`).

**Dev and release builds.** Every build is a dev build except the release script's: `VR_BuildVersion` reads
`1.0.0-dev (2026-10-07 afd53921)` (the commit's date and short hash, `-dirty` with uncommitted changes), and the menus'
corner label "Quake VR: Unleashed - v1.0-dev" (the "-dev" fainter). The release script builds with
`/p:QvrReleaseVersion=<VERSION>` (the build refuses one that is not `VERSION`'s): `1.0.0 (2026-10-07 afd53921)` and
"v1.0"; `manifest.json` and `latest.json` carry the same text, the installer's version is `1.0.0`, and the annotated
tag `v1.0.0` records it. Between releases `VERSION` names the last release (or the one being prepared): a dev build's
`-dev` plus its commit tell it apart.

**The corner label** (`vr_menu_version`, VR Settings > Advanced VR Options > HUD and Menus > Menu: "Version Label")
shows `vMAJOR.MINOR` while PATCH is 0 ("v1.0", "v1.1") and the whole version otherwise ("v1.0.1", "v1.1.0-beta.1").

**When to bump** (in the release's own commit: `-Bump patch|minor|major`, or `-Version x.y.z -BumpVersion`):

- **PATCH** (1.0.0 to 1.0.1): fixes only: crashes, bugs, balance tweaks, docs; nothing a player must relearn and no
  setting renamed; saves and configs keep working.
- **MINOR** (1.0.x to 1.1.0): new features, new settings or pages, changed defaults, new content; old saves and configs
  still load (or are migrated).
- **MAJOR** (0.x to 1.0.0, then 1.x to 2.0.0): 1.0.0 is the first release called finished; after it, a break: saves or
  configs that no longer load, a removed or reworked system, a new minimum (a runtime, a Quake data set).
- **Prereleases** (`1.0.0-beta.1`, `-beta.2`, `-rc.1`): test builds of the version named; GitHub marks them as
  prereleases. The final release drops the suffix (1.0.0-rc.2 to 1.0.0).

The first Ironwail-based release is **1.0.0** (decided 2026-10-08): the old Quake VR's tags reached v0.8.2, and Unleashed
is the release called finished, so it starts at 1.0.0 rather than below it.

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
- `-DryRun` builds, commits, pushes and tags nothing; its only `gh` calls read (`auth status`, `release view`,
  `release list`). Without `-Publish` or `-PushTag` nothing leaves the PC.
- Pushes are the tag, the branch when HEAD is ahead of its upstream, and (`-Final`, after the release is created)
  `master` to the tag: fast-forwards only, never forced, after the build and its checks. A re-run with the same version reuses a tag already on HEAD and stops if the tag is on another
  commit or the GitHub release exists (finish that one with `-CheckOnline`, or delete it on GitHub first).
- The online check installs into `out\release\<v>\checks\online-<time>\sandbox` only (no registry, shortcuts in the
  sandbox, Quake only read).
- Nothing is deleted: an earlier `out\release\<version>\` is moved aside to `<version>.old-<time>` (delete those
  yourself when done); so is an earlier `release-notes.md` on `-DraftNotes` (`release-notes.md.old-<time>`).
- `-AllowDirty` builds from uncommitted changes, for testing the script only (refused with `-Publish`/`-PushTag`).
- `ironwail.pdb` ships in the package on purpose (crash reports name the functions with it beside the exe:
  `package-quakevr.ps1`); the installer's .pdb is not uploaded.
