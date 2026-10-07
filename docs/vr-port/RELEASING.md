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

## Step by step

1. **Commit and push** the branch you release from (`vr-ironwail` today, `master` later: the script reads the
   current branch and its upstream). The tree must be clean (tracked files): the build names its commit.
2. **Have the extra files at hand** (once):
   - ericw-tools' source zip, `ericw-tools-2.0.0-alpha11-src.zip`: the package ships ericw-tools' `light.exe` (GPL-3),
     so its source goes beside it on the release page. Make it once with
     `git clone --recursive --branch 2.0.0-alpha11 https://github.com/ericwa/ericw-tools` and zip the folder
     (INSTALLER.md, "Release checklist"). Pass it with `-EricwSource <zip>`, or set `QVR_ERICW_SRC`.
   - The HD texture pack's zip, if the release offers it: `-Textures <zip>` (latest.json's `hdtextures` component).
   - A Quake folder (with `id1\pak0.pak`) for the smoke launch: `-QuakeDir <folder>`, or set `QVR_QUAKE_DIR`. Without
     one the launch is skipped with a warning. The paks are linked (or copied) into `out\release\<version>\checks\smoke`,
     never uploaded, and nothing is written into the Quake folder.
3. **Pick the version** ("Versions" below): `make_release.ps1 -Version 1.0.0 -BumpVersion` commits `VERSION` = 1.0.0
   (alone, as "Version 1.0.0"; the tree must be otherwise clean) and goes on; push that commit with the branch before
   `-Publish` (the script checks HEAD is on the upstream). Or edit `VERSION`, commit and push it yourself. A `-Version`
   other than `VERSION`'s without `-BumpVersion` stops the script; without `-Version` it releases `VERSION`'s.
4. **Dry run**: `make_release.ps1 -DryRun` checks everything below and prints the plan, without
   building or calling `gh`. Fix any `PROBLEM` line.
5. **Build and check** without publishing: `make_release.ps1 -EricwSource <zip> -QuakeDir <Quake>`. It
   takes a few minutes; read the summary (also in `out\release\1.0.0\PUBLISH.txt`) and, if you like, edit
   `out\release\1.0.0\release-notes.md`, then pass it back with `-Notes` in the next step.
6. **Publish (draft)**: the same command with `-Publish` (and `-Notes out\release\1.0.0\release-notes.md` if you edited
   them). It builds again from scratch into a fresh folder (the previous one is moved aside to `1.0.0.old-<time>`),
   tags `v1.0.0`, pushes **only the tag**, and creates a draft release with the assets.
7. **Check the draft** on https://github.com/vittorioromeo/quakevr/releases and publish it there (or
   `gh release edit v1.0.0 --repo vittorioromeo/quakevr --draft=false --latest`). `-NoDraft` (or `-Draft:$false` from PowerShell itself) skips the draft.
   The installer's first feed, `https://github.com/vittorioromeo/quakevr/releases/latest/download/latest.json`, serves
   the new release only once it is published and not a prerelease.
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
| Assets | `Misc\quakevr\make_release.py`: `QuakeVR.zip` (zipped from the package after checking every file against the manifest), `QuakeVR-Setup.exe`, the texture pack, the ericw-tools source and `-Assets`, and `latest.json` (schema 1, the installer's `ReleaseFeed`) |
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

## Safety

- `-DryRun` builds, tags and calls `gh` for nothing. Without `-Publish` or `-PushTag` nothing leaves the PC.
- Only the tag is ever pushed, never a branch. A re-run with the same version reuses a tag already on HEAD and stops
  if the tag is on another commit or the GitHub release exists (delete or edit it on GitHub first).
- Nothing is deleted: an earlier `out\release\<version>\` is moved aside to `<version>.old-<time>` (delete those
  yourself when done).
- `-AllowDirty` builds from uncommitted changes, for testing the script only (refused with `-Publish`/`-PushTag`).
- `ironwail.pdb` ships in the package on purpose (crash reports name the functions with it beside the exe:
  `package-quakevr.ps1`); the installer's .pdb is not uploaded.
