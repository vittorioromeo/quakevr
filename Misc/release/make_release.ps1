<#
.SYNOPSIS
Makes a Quake VR: Unleashed release: builds the game and the installer (Release), checks them, packages them, writes
latest.json, and (with -Publish) tags the commit and creates the GitHub release. docs/vr-port/RELEASING.md has the steps.

.DESCRIPTION
  Misc\release\make_release.ps1 -Bump patch -Publish -Final -RunTests   # THE ONE COMMAND: the tests, VERSION bumped and
                                                           # committed, build and checks, the branch pushed, tag, the
                                                           # release published as Latest, master fast-forwarded to it,
                                                           # the Latest guard, the online check (RELEASING.md, "One command")
  Misc\release\make_release.ps1 -Bump patch -Publish -Final -DryRun     # ... its plan: every command, in order
  Misc\release\make_release.ps1 -Version 1.0.1 -CheckOnline # only the steps after publishing (resuming)
  Misc\release\make_release.ps1 -DryRun                    # the checks and the plan; builds and writes nothing
  Misc\release\make_release.ps1                            # build, check, package into out\release\<VERSION> (no tag, nothing online)
  Misc\release\make_release.ps1 -Publish                   # ... then tag v<VERSION>, push the tag, create a DRAFT GitHub release
  Misc\release\make_release.ps1 -Publish -NoDraft          # ... a public release straight away
  Misc\release\make_release.ps1 -Version 1.0.0 -BumpVersion   # first commit VERSION = 1.0.0 ("Version 1.0.0"), then as above
  Misc\release\make_release.ps1 -Bump patch                # the same with the next version computed from VERSION (patch|minor|major)
  Misc\release\make_release.ps1 -Local -RunInstaller      # a LOCAL TEST release: the same build and checks into
                                                           # out\release\<VERSION>-local, latest.json pointing at a server on
                                                           # 127.0.0.1; then that server and the built QuakeVR-Setup.exe in a
                                                           # sandbox (Misc\release\test_local_release.ps1 reruns it)

The support files (the HD texture pack, ericw-tools' source and Windows zips) are hosted once on their own GitHub
release, listed with their sizes and SHA-256 in Misc\release\support_assets.json: latest.json's "hdtextures" points
at the hosted pack and the release notes link ericw-tools' source; nothing of them is uploaded again (RELEASING.md,
"Support files"). -Textures / -EricwSource attach a copy to this release instead; -NoTextures leaves HD textures out.

The version is the repository's VERSION file (docs/vr-port/RELEASING.md, "Versions"). -Version is optional: it must be
VERSION's, or with -BumpVersion the script first commits VERSION = -Version (that file alone, on a tree without other
changes; not pushed: it goes with the branch); with -DryRun it only says it would.

Works on whatever branch is checked out (its upstream is where the tag goes); with -Publish or -PushTag it pushes the
branch to its upstream first when HEAD is ahead of it (a fast-forward, never forced; the version commit). A -Final
release then fast-forwards the release branch (-ReleaseBranch, default master; RELEASING.md, "Branches") to the tag,
never forced: it stops before building when that branch is not an ancestor of HEAD. Everything it
writes is under out\release\<version>\ (git-ignored); a folder left by an earlier run is moved aside to
out\release\<version>.old-<time>, never deleted.

The build is a release build (/p:QvrReleaseVersion): the engine reports "<version> (<date> <hash>)", without a dev
build's "-dev" (VR_BuildVersion: the console, the VR Settings page, crash reports; VR_Version: the menus' corner label),
the package's manifest.json and latest.json carry the same text, and QuakeVR-Setup.exe's file and assembly version is
<version>. The annotated tag v<version> records it.
#>

[CmdletBinding()]
param(
    # x.y.z, or x.y.z-suffix for a prerelease (e.g. 1.0.0-beta.1): the tag is v<Version>. Default: the VERSION file's;
    # another one needs -BumpVersion.
    [string]$Version = "",
    # With a -Version other than VERSION's: commit VERSION = -Version ("Version x.y.z") before building.
    [switch]$BumpVersion,
    # The next version computed from VERSION (patch: 1.0.0 -> 1.0.1, minor: -> 1.1.0, major: -> 2.0.0; from a
    # prerelease 1.1.0-rc.1, the release it leads to: 1.1.0 for patch and minor, 2.0.0 for major unless it is x.0.0-...),
    # then as -Version <it> -BumpVersion. Not with -Version.
    [ValidateSet("", "patch", "minor", "major")]
    [string]$Bump = "",
    # Release notes (Markdown). Default: out\release\<version>\release-notes.md when it is there (-DraftNotes wrote it, he
    # edited it), else a draft made while building (draft_release_notes.py). The files' SHA-256 table is added.
    [string]$Notes = "",
    # Only draft the release notes: the commits since the last v* tag grouped by area, into
    # out\release\<version>\release-notes.md (an earlier one moved aside), to edit before -Publish. Nothing else.
    [switch]$DraftNotes,
    # Publish notes that still have the draft's DRAFT line (it is dropped): -Publish refuses them otherwise.
    [switch]$AutoNotes,
    # With -Publish: create the release as a draft (the default). -Draft:$false publishes it at once.
    [switch]$Draft = $true,
    # The same as -Draft:$false (which "powershell -File" and make_release.sh cannot pass).
    [switch]$NoDraft,
    # Create the tag, push it (only the tag) and create the GitHub release with the assets.
    [switch]$Publish,
    # Create and push the tag only (no GitHub release).
    [switch]$PushTag,
    # A prerelease only (x.y.z-suffix), with -Publish/-PushTag: never push the branch; the tag, pushed alone, carries its
    # commit to GitHub. For a rehearsal of the release path or a test build from a local branch (no upstream needed: the
    # remote is the one whose URL is -Repo). The version commit stays local.
    [switch]$NoBranchPush,
    # With -Publish: the release published at once and marked Latest (no draft), then the online check: GitHub's
    # latest.json read by the installer's own code (qvr-setup feed: the version, each file's size and SHA-256 against
    # this release's), the file itself byte for byte, and a sandboxed install through it (qvr-setup install --feed).
    [switch]$Final,
    # The release branch (RELEASING.md, "Branches"): with -Final, after the release is created, it is moved on the remote
    # to the released commit as a fast-forward (git push <remote> v<version>^{commit}:refs/heads/<it>; never forced: when
    # it is not an ancestor of the release the script stops and says what to run). Prereleases and drafts never move it.
    [string]$ReleaseBranch = "master",
    # With -Final: leave the release branch where it is (move it by hand later).
    [switch]$NoReleaseBranch,
    # Only the steps after publishing, for a release already on GitHub (resuming after a failure there): the online
    # check of out\release\<version> (-ReleaseDir) against the feed. No build, no tag, no gh release create.
    [switch]$CheckOnline,
    # The feed the online check reads. Default https://github.com/<Repo>/releases/latest/download/latest.json (-Local:
    # http://127.0.0.1:<LocalPort>/latest.json, the local server's).
    [string]$FeedUrl = "",
    # The release folder -CheckOnline checks (default out\release\<version>, with -Local out\release\<version>-local).
    [string]$ReleaseDir = "",
    # The online check's sandboxed install takes the HD textures too (a 0.6 GB download).
    [switch]$OnlineHd,
    # The online check without its sandboxed install (the feed only).
    [switch]$SkipOnlineInstall,
    # How many times the online check reads the feed before giving up (20 s apart: GitHub's latest can lag).
    [int]$OnlineTries = 10,
    # Only the precondition checks and the plan (and the package's file list): no build, no tag, no gh call.
    [switch]$DryRun,
    # A full rebuild of the engine instead of an incremental build.
    [switch]$Rebuild,
    # A Quake folder (with id1\pak0.pak) for the smoke launch; default $env:QVR_QUAKE_DIR. Without one the launch is skipped.
    [string]$QuakeDir = $env:QVR_QUAKE_DIR,
    # An HD texture pack zip uploaded WITH this release as latest.json's "hdtextures" component (an override). Default:
    # the pack hosted on the support-files release (support_assets.json), not uploaded again.
    [string]$Textures = "",
    # No "hdtextures" component in latest.json (the installer then downloads its built-in pinned pack).
    [switch]$NoTextures,
    # ericw-tools' source zip uploaded WITH this release (an override). Default: the release notes link the hosted one
    # (support_assets.json; GPL-3: the package ships light.exe).
    [string]$EricwSource = "",
    # Other files to attach to the release.
    [string[]]$Assets = @(),
    # Download address templates for latest.json ({tag}, {file}), in order. Default: the GitHub release's own assets.
    [string[]]$UrlBase = @(),
    # fteqcc64.exe. Default: $env:FTEQCC, QC\fteqcc64.exe, C:\OHWorkspace\quakevr\QC\fteqcc64.exe, then PATH.
    [string]$Fteqcc = "",
    # MSBuild.exe. Default: the newest Visual Studio with MSBuild and the ClangCL toolset (vswhere).
    [string]$MSBuild = "",
    # The GitHub repository (owner/name) of the release; the installer's feeds point at vittorioromeo/quakevr.
    [string]$Repo = "vittorioromeo/quakevr",
    # Testing the script itself: build from a tree with uncommitted changes (never with -Publish or -PushTag).
    [switch]$AllowDirty,
    # A local test release (RELEASING.md, "Test a release locally"): the same build, checks and package, into
    # out\release\<version>-local; latest.json's addresses are http://127.0.0.1:<LocalPort>/<file> (a local server,
    # Misc\release\test_local_release.ps1); nothing online (no fetch, no gh, no tag). Never with -Publish or -PushTag.
    [switch]$Local,
    # The local server's port, written into latest.json's addresses (-Local).
    [int]$LocalPort = 8517,
    # After a -Local build (implies -Local): start the local server and run the built QuakeVR-Setup.exe against it, in a
    # sandbox folder under %TEMP% (test_local_release.ps1).
    [switch]$RunInstaller,
    [switch]$SkipSmoke,
    [switch]$SkipInstallerTests,
    # Before building: the headless test suite (Misc\release\run_test_suite.py: the Misc\quakevr test scripts that give a
    # verdict, one at a time) on a kit worktree holding this commit's files; a failure stops the release (nothing built,
    # tagged or published).
    [switch]$RunTests,
    # The kit worktree the tests run on (C:\OHWorkspace\qvr-agents\<name>; the kit's new_agent.sh <name> <commit>).
    # Default: QVR_TEST_AGENT, else this checkout's own name when it is one.
    [string]$TestAgent = $env:QVR_TEST_AGENT,
    # With -RunTests: the known-flaky tests (run_test_suite.py's list) and -FlakyTests only warn when they fail.
    [switch]$AllowFlaky,
    # With -RunTests -AllowFlaky: more tests (names from run_test_suite.py --list) that only warn.
    [string[]]$FlakyTests = @(),
    # With -RunTests: only the tests whose name matches this regex.
    [string]$TestOnly = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2
# The GitHub releases (gh release list): tag, isLatest, isDraft, isPrerelease.
function Get-Releases() {
    $r = Run "gh" @("release", "list", "--repo", $Repo, "--json", "tagName,isLatest,isDraft,isPrerelease", "--limit", "200")
    if ($r.Code -ne 0) { throw "gh release list --repo $Repo failed" }
    $list = @()
    foreach ($x in ($r.Text | ConvertFrom-Json)) { $list += $x }   # (PowerShell 5.1 hands a JSON array over whole)
    , $list
}
# The Latest guard (RELEASING.md, "Which release is Latest"): the game release $want is GitHub's Latest, so
# releases/latest/download/latest.json serves it, and no asset or texture release (assets-*, textures-*) is. Fixed with
# gh release edit <want> --latest when not; the result as a line for the report.
function Invoke-LatestGuard([string]$want) {
    $latest = @((Get-Releases) | Where-Object { $_.isLatest })
    $names = if ($latest.Count) { ($latest | ForEach-Object { $_.tagName }) -join ", " } else { "none" }
    if ($latest.Count -eq 1 -and $latest[0].tagName -eq $want) { Say "Latest: $want (no assets-*/textures-* release marked Latest)"; return "Latest is $want" }
    Warn "GitHub's Latest is $names, not ${want}$(if (@($latest | Where-Object { $_.tagName -match '^(assets|textures)-' }).Count) { ' (an asset/texture release: the installer and the update check would get a 404)' }): gh release edit $want --repo $Repo --latest"
    if ((Run "gh" @("release", "edit", $want, "--repo", $Repo, "--latest")).Code -ne 0) { throw "Latest guard: gh release edit $want --repo $Repo --latest failed" }
    $after = @((Get-Releases) | Where-Object { $_.isLatest })
    if ($after.Count -ne 1 -or $after[0].tagName -ne $want) { throw "Latest guard: after gh release edit --latest, Latest is $(($after | ForEach-Object { $_.tagName }) -join ', '), not $want" }
    Say "Latest fixed: $want (was $names)"
    "Latest fixed: $want (was $names; gh release edit --latest)"
}
# The prerelease's side of the guard: $want is on GitHub as a published prerelease and is not Latest (so neither the
# installer's feed nor the game's update check, both releases/latest, ever sees it). Nothing is edited.
function Assert-PrereleaseNotLatest([string]$want) {
    $all = Get-Releases
    $me = @($all | Where-Object { $_.tagName -eq $want }) | Select-Object -First 1
    if (-not $me) { throw "no GitHub release $want in $Repo" }
    if ($me.isDraft -or -not $me.isPrerelease -or $me.isLatest) { throw "$want is draft=$($me.isDraft), prerelease=$($me.isPrerelease), latest=$($me.isLatest): expected a published prerelease that is not Latest" }
    $names = (@($all | Where-Object { $_.isLatest } | ForEach-Object { $_.tagName })) -join ", "
    Say "$want is a published prerelease, not Latest (Latest is still $(if ($names) { $names } else { 'none' }): releases/latest does not serve $want)"
    "prerelease $want not Latest (Latest: $(if ($names) { $names } else { 'none' }))"
}
# The release branch (RELEASING.md, "Branches"; $ReleaseBranch, default master) on $remote: the commit it is at there
# ("" when it does not exist) and whether that commit is an ancestor of $target (Ancestor: 0 yes, 1 no, else unknown).
function Get-ReleaseBranchState([string]$target) {
    $ref = "refs/heads/$ReleaseBranch"
    $ls = GitRun @("ls-remote", $remote, $ref)
    if ($ls.Code -ne 0) { throw "git ls-remote $remote $ref failed" }
    $line = @($ls.Out | Where-Object { "$_" -match ('\s' + [regex]::Escape($ref) + '$') }) | Select-Object -First 1
    $sha = if ($line) { "$line" -replace '\s.*$', '' } else { "" }
    $anc = 0
    if ($sha) {
        # (Normally fetched already; a commit pushed there since is fetched now, into FETCH_HEAD only.)
        if ((GitRun @("cat-file", "-e", "$sha^{commit}")).Code -ne 0) { GitRun @("fetch", "--quiet", $remote, $ref) | Out-Null }
        $anc = (GitRun @("merge-base", "--is-ancestor", $sha, $target)).Code
    }
    [pscustomobject]@{ Sha = $sha; Ancestor = $anc }
}
# The fast-forward command, quoted for PowerShell and bash alike.
function Get-ReleaseBranchPush() { "git push $(if ($remote) { $remote } else { 'origin' }) ""$tag^{commit}:refs/heads/$ReleaseBranch""" }
# Why the release branch cannot be fast-forwarded to $what, and what to run (never a force).
function Get-ReleaseBranchDiverged([string]$sha, [string]$what, [switch]$Released) {
    $from = Get-Variable branch -ValueOnly -ErrorAction SilentlyContinue   # (-CheckOnline has none)
    $from = if ($from -and $from -ne 'HEAD') { $from } else { 'the branch you release from' }
    $head = "$remote $ReleaseBranch ($($sha.Substring(0, 8))) is not an ancestor of $what`: it has commits the release does not " +
        "(git fetch $remote; git log --oneline $what..$sha), and it is never forced. "
    if ($Released) {
        $head + "The release is out; bring $ReleaseBranch to it with a merge (not a fast-forward, still not forced): git switch $ReleaseBranch; " +
            "git pull --ff-only; git merge $tag; git push $remote $ReleaseBranch (and merge $ReleaseBranch into $from so the next release fast-forwards it)"
    }
    else {
        $head + "Merge it into $from first (git merge $remote/$ReleaseBranch; push), then release; or -NoReleaseBranch to release " +
            "without moving it (by hand afterwards)"
    }
}
# -Final, after the release is created: $ReleaseBranch on $remote fast-forwarded to the tag's commit; a line for the report.
function Invoke-ReleaseBranchFastForward() {
    $s = Get-ReleaseBranchState $tag
    if ($s.Sha -eq $commit) { Say "$remote $ReleaseBranch is already $short ($tag)"; return "$ReleaseBranch already at $tag" }
    if ($s.Sha -and $s.Ancestor -ne 0) {
        if ($s.Ancestor -eq 1) { throw (Get-ReleaseBranchDiverged $s.Sha $tag -Released) }
        throw "could not tell whether $remote $ReleaseBranch ($($s.Sha.Substring(0, 8))) is an ancestor of $tag (git fetch $remote, then: $(Get-ReleaseBranchPush))"
    }
    $r = GitRun @("push", $remote, "$tag^{commit}:refs/heads/$ReleaseBranch")   # (git refuses anything but a fast-forward)
    if ($r.Code -ne 0) { throw "$(Get-ReleaseBranchPush) failed (refused: $remote $ReleaseBranch moved meanwhile? never forced; git fetch $remote and look)" }
    $after = Get-ReleaseBranchState $tag
    if ($after.Sha -ne $commit) { throw "after the push, $remote $ReleaseBranch is $(if ($after.Sha) { $after.Sha.Substring(0, 8) } else { 'missing' }), not $short ($tag)" }
    $was = if ($s.Sha) { "was $($s.Sha.Substring(0, 8))" } else { "created" }
    Say "$remote $ReleaseBranch fast-forwarded to $short ($tag; $was)"
    "$ReleaseBranch fast-forwarded to $tag ($was)"
}

# Where a failure leaves things, and how to go on (RELEASING.md, "If it stops"): printed by the trap below.
$script:stage = "checks"
function Get-ResumeHint() {
    $again = "make_release.ps1 -Version $Version <the same options, without -Bump/-BumpVersion>"
    switch ($script:stage) {
        "checks" { @("Nothing was changed (no commit, nothing pushed): fix the cause and run the same command again.") }
        "tests" { @("Nothing was changed (no commit, nothing pushed): fix the failing test (its log is in the tests folder),", "or -AllowFlaky for a known-flaky one, and run the same command again.") }
        "build" { if ($bumpPending -or $script:bumped) { @("VERSION = $Version is committed here (HEAD, not pushed). Fix the cause, then: $again", "(or drop that commit: git reset --keep HEAD~1)") } else { @("Nothing was pushed or tagged: fix the cause and run the same command again.") } }
        "push" { @("The build passed; pushing the branch failed (nothing tagged). Fix it (git pull --rebase? then the build is redone), then: $again") }
        "tag" { @("The branch is pushed$(if ($script:bumped) { " (VERSION = $Version)" }); the tag failed. Then: $again (a tag $tag already on HEAD is reused)") }
        "release" { $h = @("The tag $tag is pushed; gh release create failed. If GitHub shows a partial release $tag, delete it there (or finish it:",
                      "gh release upload $tag <the missing files of out\release\$Version\assets> --repo $Repo; gh release edit $tag --repo $Repo --draft=false --latest)",
                      "and run make_release.ps1 -Version $Version -CheckOnline; else: $again")
                    if ($Final -and -not $NoReleaseBranch) { $h += "(Finishing it by hand: then move $ReleaseBranch too, a fast-forward: $(Get-ReleaseBranchPush))" }
                    $h }
        "master" { @("The release $tag is published; $remote $ReleaseBranch was not moved (the reason above). Move it, never forced:",
                     "diverged: the merge above; otherwise (a refused or failed push) the fast-forward: $(Get-ReleaseBranchPush)",
                     "Then finish the checks: make_release.ps1 -Version $Version -CheckOnline (the Latest guard and the online check)") }
        "online" { @("The release $tag is published. Finish the checks with: make_release.ps1 -Version $Version -CheckOnline") }
        default { @("Run the same command again.") }
    }
}
trap {
    Write-Host ""
    Write-Host "STOPPED ($($script:stage)): $($_.Exception.Message)" -ForegroundColor Red
    Get-ResumeHint | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
    break
}

$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$versionFile = Join-Path $root "VERSION"
$fileVersion = if (Test-Path -LiteralPath $versionFile -PathType Leaf) { "$(Get-Content -LiteralPath $versionFile -TotalCount 1)".Trim() } else { "" }
if (-not $fileVersion) { throw "no version in $versionFile (one line: MAJOR.MINOR.PATCH; RELEASING.md, 'Versions')" }
# The version after $from by semantic versioning: a prerelease's patch (and its minor/major when those are already the
# ones it numbers) is the release it leads to (1.1.0-rc.1: patch and minor 1.1.0, major 2.0.0; 2.0.0-beta.1: 2.0.0).
function Get-NextVersion([string]$from, [string]$part) {
    if ($from -notmatch '^(\d+)\.(\d+)\.(\d+)(-[0-9A-Za-z][0-9A-Za-z.]*)?$') { throw "-Bump: VERSION must hold x.y.z or x.y.z-suffix (got '$from')" }
    $ma = [int]$Matches[1]; $mi = [int]$Matches[2]; $pa = [int]$Matches[3]; $pre = [bool]$Matches[4]
    switch ($part) {
        "patch" { if ($pre) { "$ma.$mi.$pa" } else { "$ma.$mi.$($pa + 1)" } }
        "minor" { if ($pre -and $pa -eq 0) { "$ma.$mi.0" } else { "$ma.$($mi + 1).0" } }
        "major" { if ($pre -and $pa -eq 0 -and $mi -eq 0) { "$ma.0.0" } else { "$($ma + 1).0.0" } }
    }
}
if ($Bump) {
    if ($Version) { throw "-Bump and -Version together: pick one (-Bump computes the version from VERSION's $fileVersion)" }
    $Version = Get-NextVersion $fileVersion $Bump
    $BumpVersion = [switch]$true
    Write-Host "-Bump ${Bump}: VERSION $fileVersion -> $Version"
}
if (-not $Version) { $Version = $fileVersion }
$tag = "v$Version"
if ($NoDraft) { $Draft = $false }
if ($Final) {
    if (-not $Publish -and -not $CheckOnline) { throw "-Final goes with -Publish: the release published as Latest, then checked online" }
    $Draft = $false
}
if ($CheckOnline -and ($Publish -or $PushTag -or $DryRun -or $DraftNotes -or $BumpVersion)) { throw "-CheckOnline only checks a published release: not with -Publish, -PushTag, -DryRun, -DraftNotes or a version bump (give -Version)" }
if ($RunInstaller) { $Local = $true }
if ($Local -and ($Publish -or $PushTag)) { throw "-Local is a local test release: never with -Publish or -PushTag" }
if ($Local -and -not $UrlBase) { $UrlBase = @("http://127.0.0.1:$LocalPort/{file}") }
$bumpPending = $false   # (-BumpVersion: VERSION committed after the tests)
$problems = New-Object System.Collections.Generic.List[string]   # fatal for this mode
$warnings = New-Object System.Collections.Generic.List[string]
$online = $Publish -or $PushTag
if ($Textures -and $NoTextures) { throw "-Textures and -NoTextures together: pick one" }
# The support-files release (Misc\release\support_assets.json): its tag, URL template, files, sizes and SHA-256.
$supportPath = Join-Path $PSScriptRoot "support_assets.json"
$support = Get-Content -Raw -LiteralPath $supportPath | ConvertFrom-Json
function SupportFile([string]$key) { $support.files.$key }
function SupportUrl([string]$key) { $support.url.Replace("{tag}", $support.tag).Replace("{file}", (SupportFile $key).file) }
$hostedTextures = -not $Textures -and -not $NoTextures

function Say([string]$text) { Write-Host $text }
function Step([string]$text) { Write-Host ""; Write-Host "==> $text" -ForegroundColor Cyan }
function Warn([string]$text) { $warnings.Add($text); Write-Host "warning: $text" -ForegroundColor Yellow }
# A failed precondition: fatal unless -DryRun (which reports them all) or, for $onlineOnly ones, unless publishing.
function Problem([string]$text, [switch]$OnlineOnly) {
    if ($DryRun -or ($OnlineOnly -and -not $online)) { Warn $text } else { $problems.Add($text); Write-Host "PROBLEM: $text" -ForegroundColor Red }
}

# A native command's output lines and exit code, stderr dropped (PowerShell 5.1 would turn stderr into errors).
function Run([string]$exe, [string[]]$arguments) {
    $old = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    try { $out = & $exe @arguments 2>$null; $code = $LASTEXITCODE } finally { $ErrorActionPreference = $old }
    [pscustomobject]@{ Out = @($out); Code = $code; Text = (@($out) -join "`n").Trim() }
}
function GitRun([string[]]$arguments) { Run "git" (@("-C", $root) + $arguments) }

# A long native step: output into $log (and $log.err), the tail shown when it fails.
function Invoke-Logged([string]$exe, [string[]]$arguments, [string]$log, [string]$dir = $root, [switch]$NoThrow) {
    $line = ($arguments | ForEach-Object { if ($_ -eq "" -or $_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ } }) -join " "
    $p = Start-Process -FilePath $exe -ArgumentList $line -WorkingDirectory $dir -NoNewWindow -PassThru `
        -RedirectStandardOutput $log -RedirectStandardError "$log.err"
    $null = $p.Handle   # (keeps ExitCode readable after the exit)
    $p.WaitForExit()
    if ($p.ExitCode -ne 0 -and -not $NoThrow) {
        Get-Content $log, "$log.err" -ErrorAction SilentlyContinue | Select-Object -Last 30 | ForEach-Object { Write-Host "    $_" }
        throw "$([System.IO.Path]::GetFileName($exe)) failed (exit $($p.ExitCode)); full log: $log"
    }
    $p.ExitCode
}

$installerDir = Join-Path $root "Installer"
# The installer's console tool already built (Release, else Debug), for qvr-setup detect and the online check.
function Find-QvrSetup() {
    foreach ($c in "Release", "Debug") {
        $p = Join-Path $installerDir "src\QuakeVR.Installer.Cli\bin\$c\net9.0-windows\qvr-setup.exe"
        if (Test-Path -LiteralPath $p) { return $p }
    }
    $null
}
# The Quake the installer would pick (DetectionReport.DefaultQuake: its "Expansions (for <name>, base <dir>):" line).
function Find-QuakeDir([string]$exe) {
    $r = Run $exe @("detect")
    foreach ($l in $r.Out) { if ("$l" -match '^Expansions \(for (.+), base (.+)\):\s*$') { return $Matches[2].Trim() } }
    $null
}

# The online check of a published release (-Final, -CheckOnline): the feed through the installer's own reader until it
# names this release (qvr-setup feed --url: the version; each file's size and SHA-256 against the release's assets\),
# the served latest.json byte for byte, then a sandboxed install through the feed (qvr-setup install --feed --sandbox:
# the real download path; nothing outside the sandbox) and qvr-setup verify. Throws on any difference.
function Invoke-OnlineCheck([string]$relDir, [string]$feed, [string]$quake) {
    $assetsHere = Join-Path $relDir "assets"
    $localFeed = Join-Path $assetsHere "latest.json"
    if (-not (Test-Path -LiteralPath $localFeed)) { throw "online check: no $localFeed (the release folder of the published release: -ReleaseDir)" }
    $expect = (Get-Content -Raw -LiteralPath $localFeed | ConvertFrom-Json).version
    $qvrSetup = Find-QvrSetup
    if (-not $qvrSetup) { throw "online check: no qvr-setup.exe built (dotnet build Installer\QuakeVR.Installer.sln -c Release)" }
    $checkDir = Join-Path $relDir "checks\online-$(Get-Date -Format yyyyMMdd-HHmmss)"
    New-Item -ItemType Directory -Force $checkDir | Out-Null
    Say "feed $feed; expecting version $expect; this release's files: $assetsHere"
    $feedArgs = @("feed", "--url", $feed, "--assets", $assetsHere)
    $hd = (Get-Content -Raw -LiteralPath $localFeed | ConvertFrom-Json).components.PSObject.Properties["hdtextures"]
    if ($hd -and -not (Test-Path -LiteralPath (Join-Path $assetsHere $hd.Value.file))) { $feedArgs += @("--hosted", "hdtextures") }
    for ($try = 1; ; $try++) {
        $r = Run $qvrSetup $feedArgs
        # (Its "version ..." line: a feed other than the built-in one is announced first, "TEST FEED: <url>".)
        $first = "$(@($r.Out | Where-Object { "$_".StartsWith('version ') }) | Select-Object -First 1)"
        if (-not $first) { $first = if ($r.Out.Count) { "$($r.Out[-1])" } else { "(no answer)" } }
        if ($r.Code -eq 0 -and $first.StartsWith("version $expect;")) { break }
        if ($try -ge $OnlineTries) {
            $r.Out | ForEach-Object { Say "    $_" }
            throw "online check: $feed does not serve this release after $try tries (last: $first)"
        }
        Say "  try ${try}: $first$(if ($r.Code -ne 0 -and $first.StartsWith("version $expect;")) { ' (a file differs)' }): again in 20 s"
        Start-Sleep -Seconds 20
    }
    $r.Out | ForEach-Object { Say "  $_" }
    $served = Join-Path $checkDir "latest.json"
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -UseBasicParsing -TimeoutSec 60 -Uri $feed -OutFile $served
    $hServed = (Get-FileHash -LiteralPath $served -Algorithm SHA256).Hash; $hLocal = (Get-FileHash -LiteralPath $localFeed -Algorithm SHA256).Hash
    if ($hServed -ne $hLocal) { throw "online check: the served latest.json ($served) is not this release's ($localFeed)" }
    Say "  latest.json served = this release's (sha256 $($hLocal.ToLowerInvariant().Substring(0, 16))...)"
    if ($SkipOnlineInstall) { Warn "-SkipOnlineInstall: no sandboxed install through the feed"; return }
    if (-not $quake) { Warn "online check: no Quake folder (-QuakeDir, QVR_QUAKE_DIR, qvr-setup detect): no sandboxed install through the feed"; return }
    $sandbox = Join-Path $checkDir "sandbox"
    $iArgs = @("install", "--feed", $feed, "--sandbox", $sandbox, "--quake", $quake, "--accept-statement", "--setup-from", (Join-Path $assetsHere "QuakeVR-Setup.exe"))
    if ($OnlineHd) { $iArgs += "--hd" }
    Say "  installing through the feed into $sandbox (the real download; nothing outside the sandbox)"
    Invoke-Logged $qvrSetup $iArgs (Join-Path $checkDir "install.log") | Out-Null
    Invoke-Logged $qvrSetup @("verify", "--target", (Join-Path $sandbox "QuakeVR")) (Join-Path $checkDir "verify.log") | Out-Null
    $rec = Get-Content -Raw -LiteralPath (Join-Path $sandbox "QuakeVR\install.json") | ConvertFrom-Json
    if ($rec.version -ne $expect) { throw "online check: the sandbox installed '$($rec.version)', expected '$expect'" }
    Say "  installed $($rec.version) through the feed; verify: $((Get-Content (Join-Path $checkDir 'verify.log') | Select-Object -Last 1))"
    Say "online check passed ($checkDir)"
}

function Size([long]$bytes) {
    $c = [System.Globalization.CultureInfo]::InvariantCulture
    if ($bytes -ge 1MB) { [string]::Format($c, "{0:N1} MB", $bytes / 1MB) }
    elseif ($bytes -ge 10KB) { [string]::Format($c, "{0:N0} KB", $bytes / 1KB) }
    else { [string]::Format($c, "{0:N0} bytes", $bytes) }
}

# ------------------------------------------------------------------------------------------------------------------
Step "Preconditions ($tag)"

$semver = '^\d+\.\d+\.\d+(-[0-9A-Za-z][0-9A-Za-z.]*)?$'
if ($Version -notmatch $semver) { throw "-Version must be x.y.z or x.y.z-suffix (got '$Version')" }
if ($fileVersion -notmatch $semver) { throw "VERSION must hold x.y.z or x.y.z-suffix (got '$fileVersion')" }
Say "version $Version (VERSION: $fileVersion)"
if ($Version -ne $fileVersion) {
    if (-not $BumpVersion) {
        throw "-Version $Version is not VERSION's ${fileVersion}: leave -Version out to release $fileVersion, or add -BumpVersion to commit VERSION = $Version first"
    }
    $numeric = { param($v) [version]($v -replace '-.*$', '') }
    if ((& $numeric $Version) -lt (& $numeric $fileVersion)) { throw "-BumpVersion: $Version is older than VERSION's $fileVersion" }
    if ($DraftNotes) { Say "-DraftNotes: the notes for $Version (VERSION is not committed)" }
    elseif ((GitRun @("--no-optional-locks", "status", "--porcelain", "--untracked-files=no")).Text -and -not $DryRun) {
        throw "-BumpVersion: commit or stash your other changes first (the version commit holds VERSION alone)"
    }
    else {
        # Committed after the tests, before the build (which bakes VERSION in); the checks below are of HEAD before it.
        $bumpPending = $true
        if ($DryRun) { Warn "-BumpVersion: would commit VERSION = $Version (now $fileVersion) after the tests, before building; the checks below are of the tree as it is" }
        else { Say "VERSION = $Version is committed after the tests (if any), before the build" }
    }
}
$prerelease = $Version.Contains("-")
if ($Final -and $prerelease) { throw "-Final marks the release Latest, which a prerelease ($Version) never is: publish it with -Publish -NoDraft (then checked against its own latest.json)" }
if ($NoBranchPush -and -not $prerelease) { throw "-NoBranchPush tags a commit that no branch on GitHub holds: prereleases only (x.y.z-suffix), never $Version" }
# A prerelease is never GitHub's Latest, so releases/latest/download/latest.json never serves it: its own feed is the
# latest.json asset of its tag.
$tagFeed = "https://github.com/$Repo/releases/download/$tag/latest.json"
if ($AllowDirty -and $online) { throw "-AllowDirty is for testing the script: never with -Publish or -PushTag" }
if ($Notes -and -not (Test-Path -LiteralPath $Notes -PathType Leaf)) { throw "-Notes: no file $Notes" }
$outBase = Join-Path $root "out\release"
$outDir = Join-Path $outBase "$Version$(if ($Local) { '-local' })"
$notesDraftPath = Join-Path $outDir "release-notes.md"
$draftMarker = "<!-- DRAFT"
$notesDrafter = Join-Path $PSScriptRoot "draft_release_notes.py"

if ($DraftNotes) {
    Step "Release notes draft ($notesDraftPath)"
    if (Test-Path -LiteralPath $notesDraftPath) {
        $aside = "$notesDraftPath.old-$(Get-Date -Format yyyyMMdd-HHmmss)"
        Move-Item -LiteralPath $notesDraftPath $aside
        Say "the earlier notes moved aside: $aside"
    }
    & python $notesDrafter --root $root --version $Version --out $notesDraftPath
    if ($LASTEXITCODE -ne 0) { throw "draft_release_notes.py failed" }
    Say "Edit $notesDraftPath (one bullet per change a player notices), delete its first line (DRAFT), then publish:"
    Say "  make_release.ps1 $(if ($Bump) { "-Bump $Bump " } elseif ($Version -ne $fileVersion) { "-Version $Version -BumpVersion " })-Publish ...   (it reads that file; -Notes <file> for another)"
    return
}

if ($CheckOnline) {
    $script:stage = "online"
    if (-not $ReleaseDir) { $ReleaseDir = $outDir }
    if (-not $FeedUrl) { $FeedUrl = if ($Local) { "http://127.0.0.1:$LocalPort/latest.json" } elseif ($prerelease) { $tagFeed } else { "https://github.com/$Repo/releases/latest/download/latest.json" } }
    if (-not $QuakeDir -and -not $SkipOnlineInstall -and (Find-QvrSetup)) { $QuakeDir = Find-QuakeDir (Find-QvrSetup) }
    if (-not $Local) {
        Step "Latest guard ($Repo)"
        if ($prerelease) { Assert-PrereleaseNotLatest $tag | Out-Null } else { Invoke-LatestGuard $tag | Out-Null }
        if (-not $prerelease -and -not $NoReleaseBranch) {
            # (Read only: whether the release branch holds the release; a resume after STOPPED (master) is told the push.)
            Step "Release branch ($ReleaseBranch)"
            $repoUrl = [regex]::Escape($Repo) + '(\.git)?/?$'
            $remote = "$(@((GitRun @("remote")).Out | Where-Object { "$_" -and (GitRun @("remote", "get-url", "$_")).Text -match $repoUrl }) | Select-Object -First 1)"
            $commit = (GitRun @("rev-parse", "-q", "--verify", "refs/tags/$tag^{commit}")).Text
            if (-not $remote -or -not $commit) { Warn "release branch not checked: $(if (-not $remote) { "no git remote points at $Repo" } else { "no tag $tag here (git fetch --tags)" })" }
            else {
                $s = Get-ReleaseBranchState $tag
                if ($s.Sha -eq $commit) { Say "$remote $ReleaseBranch is $tag's commit" }
                elseif ($s.Sha -and (GitRun @("merge-base", "--is-ancestor", $commit, $s.Sha)).Code -eq 0) { Say "$remote $ReleaseBranch ($($s.Sha.Substring(0, 8))) holds $tag (and later commits)" }
                elseif (-not $s.Sha -or $s.Ancestor -eq 0) { Warn "$remote $ReleaseBranch $(if ($s.Sha) { "($($s.Sha.Substring(0, 8))) is behind $tag" } else { 'does not exist' }): move it (fast-forward): $(Get-ReleaseBranchPush)" }
                else { Warn (Get-ReleaseBranchDiverged $s.Sha $tag -Released) }
            }
        }
    }
    Step "Online check of $tag ($ReleaseDir)"
    Invoke-OnlineCheck $ReleaseDir $FeedUrl $QuakeDir
    if ($warnings.Count) { Say ""; Say "$($warnings.Count) warning(s) above." }
    return
}

# The notes, read now: the build moves an earlier out\release\<version> aside (and -Notes may be in it).
$notesText = ""; $notesSource = ""
if ($Notes) { $notesSource = (Resolve-Path -LiteralPath $Notes).Path }
elseif (Test-Path -LiteralPath $notesDraftPath -PathType Leaf) { $notesSource = $notesDraftPath }
if ($notesSource) { $notesText = [System.IO.File]::ReadAllText($notesSource) }
if ($notesText.Contains($draftMarker)) {
    if ($AutoNotes) { Warn "-AutoNotes: $notesSource is still the draft: published as it is (its DRAFT line dropped)" }
    elseif ($Publish) { Problem "$notesSource is still the draft (its first line, DRAFT): edit it and delete that line, or pass -AutoNotes" }
    else { Say "notes: $notesSource (still the draft: edit it and delete its DRAFT line before -Publish)" }
}
elseif ($notesSource) { Say "notes: $notesSource" }
elseif ($Publish -and -not $AutoNotes) {
    Problem "no release notes: -DraftNotes drafts $notesDraftPath (the commits since the last v* tag by area): edit it, delete its DRAFT line, then -Publish (or -Notes <file>, or -AutoNotes to publish the draft as made)"
}
else { Say "notes: drafted while building into $notesDraftPath (edit it before -Publish)" }

$branch = (GitRun @("rev-parse", "--abbrev-ref", "HEAD")).Text
$commit = (GitRun @("rev-parse", "HEAD")).Text
$short = (GitRun @("rev-parse", "--short=8", "HEAD")).Text
$buildStamp = (GitRun @("log", "-1", "--date=format:%Y-%m-%d", "--format=%cd %h", "--abbrev=8")).Text
Say "branch $branch, commit $short ($buildStamp)"

$dirty = (GitRun @("--no-optional-locks", "status", "--porcelain", "--untracked-files=no")).Text
if ($dirty) {
    if ($AllowDirty) { Warn "uncommitted changes (-AllowDirty): the build says -dirty and is not this commit" }
    else { Problem "uncommitted changes to tracked files: commit or stash them first`n$dirty" }
}
$versionText = "$Version ($buildStamp$(if ($dirty) { '-dirty' }))"

# The current branch's upstream (whatever the branch is called): the tag goes to its remote, HEAD to it first.
$remote = ""; $upstreamRef = ""; $pushBranch = $false
if ($branch -eq "HEAD") { Problem "detached HEAD: check out the branch to release" -OnlineOnly }
else {
    $upstream = (GitRun @("rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}"))
    if ($NoBranchPush) {
        # (The tag alone goes to the remote that is -Repo; the branch, its upstream and the version commit stay as they are.)
        $repoUrl = [regex]::Escape($Repo) + '(\.git)?/?$'
        $remote = "$(@((GitRun @("remote")).Out | Where-Object { "$_" -and (GitRun @("remote", "get-url", "$_")).Text -match $repoUrl }) | Select-Object -First 1)"
        if (-not $remote) { Problem "-NoBranchPush: no git remote points at $Repo" -OnlineOnly }
        else {
            if (-not $Local -and (GitRun @("fetch", "--quiet", $remote)).Code -ne 0) { Warn "git fetch $remote failed: the tag check uses the last fetched state" }
            Say "-NoBranchPush: $branch is not pushed$(if ($upstream.Code -eq 0 -and $upstream.Text) { " (upstream $($upstream.Text))" }); the tag $tag alone goes to $remote, with its commit$(if ($bumpPending) { ' (the version commit, local otherwise)' })"
        }
    }
    elseif ($upstream.Code -ne 0 -or -not $upstream.Text) { Problem "branch $branch has no upstream (git push -u <remote> $branch)" -OnlineOnly }
    else {
        $remote = (GitRun @("config", "branch.$branch.remote")).Text
        if ($Local) { Say "-Local: no git fetch (the pushed check uses the last fetched state)" }
        elseif ((GitRun @("fetch", "--quiet", $remote)).Code -ne 0) { Warn "git fetch $remote failed: the pushed check uses the last fetched state" }
        $upstreamRef = (GitRun @("config", "branch.$branch.merge")).Text   # (refs/heads/<the upstream branch>)
        if ((GitRun @("merge-base", "--is-ancestor", "HEAD", $upstream.Text)).Code -eq 0) {
            Say "HEAD is on $($upstream.Text)$(if ($bumpPending) { ': the version commit is pushed to it before tagging' })"
            $pushBranch = $bumpPending
        }
        elseif ((GitRun @("merge-base", "--is-ancestor", $upstream.Text, "HEAD")).Code -eq 0) {
            $ahead = (GitRun @("rev-list", "--count", "$($upstream.Text)..HEAD")).Text
            $pushBranch = $true
            if ($online) { Say "HEAD is $ahead commit(s) ahead of $($upstream.Text): pushed to it (fast-forward) after the checks, before tagging" }
            else { Warn "HEAD is $ahead commit(s) ahead of $($upstream.Text) (-Publish pushes them)" }
        }
        else { Problem "HEAD ($short) and $($upstream.Text) have diverged: merge or rebase, then run again" -OnlineOnly }
        $remoteUrl = (GitRun @("remote", "get-url", $remote)).Text
        if ($remoteUrl -notmatch [regex]::Escape($Repo) + '(\.git)?/?$') { Problem "remote $remote is $remoteUrl, not $Repo (-Repo): the tag and the release would be in different places" -OnlineOnly }
    }
}

# The tag: new, or (a re-run after a failed publish) already on this very commit.
$tagLocal = GitRun @("rev-parse", "-q", "--verify", "refs/tags/$tag^{commit}")
$reuseTag = $false
if ($tagLocal.Code -eq 0 -and $bumpPending) { Problem "tag $tag already exists ($($tagLocal.Text.Substring(0, 8))) and -BumpVersion would make a new commit for it" }
elseif ($tagLocal.Code -eq 0) {
    if ($tagLocal.Text -eq $commit) { $reuseTag = $true; Warn "tag $tag already exists here on ${short}: it is reused" }
    else { Problem "tag $tag already exists on another commit ($($tagLocal.Text.Substring(0, 8)))" }
}
$tagRemote = ""
if ($remote -and -not $Local) {
    $ls = GitRun @("ls-remote", "--tags", $remote, "refs/tags/$tag", "refs/tags/$tag^{}")
    if ($ls.Code -eq 0 -and $ls.Text) {
        $tagRemote = @(@($ls.Out | Where-Object { $_ -match '\^\{\}$' }) + @($ls.Out))[0] -replace '\s.*$', ''   # (the peeled commit first)
        if ($tagRemote -ne $commit -or $bumpPending) { Problem "tag $tag already exists on $remote on another commit" } else { Warn "tag $tag is already on $remote (this commit)" }
    }
}
# The release branch (-Final): fast-forwarded to the release after it is created, so it must be an ancestor of HEAD now
# (the version commit is HEAD's child): stopping here is better than after publishing.
$moveReleaseBranch = $Final -and -not $NoReleaseBranch
if ($Final -and $NoReleaseBranch) { Warn "-NoReleaseBranch: $ReleaseBranch is not moved to $tag (by hand: $(Get-ReleaseBranchPush))" }
elseif ($moveReleaseBranch -and $remote) {
    $rb = Get-ReleaseBranchState $commit
    if (-not $rb.Sha) { Say "$remote $ReleaseBranch does not exist: created at $tag after the release" }
    elseif ($rb.Ancestor -eq 0) { Say "$remote $ReleaseBranch ($($rb.Sha.Substring(0, 8))) is an ancestor of HEAD: fast-forwarded to $tag after the release" }
    elseif ($rb.Ancestor -eq 1) { Problem (Get-ReleaseBranchDiverged $rb.Sha $short) -OnlineOnly }
    else { Warn "could not tell whether $remote $ReleaseBranch ($($rb.Sha.Substring(0, 8))) is an ancestor of HEAD: checked again after the release" }
}

# Tools.
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { Problem "gh (GitHub CLI) not found" -OnlineOnly }
elseif ($Local) { Say "gh found (not called: -Local)" }
else {
    # (Read-only calls, -DryRun's too: whether gh is logged in, the release is still to be made, what is Latest now.)
    if ((Run "gh" @("auth", "status")).Code -ne 0) { Problem "gh is not logged in (gh auth login; gh auth status says why)" -OnlineOnly }
    elseif ((Run "gh" @("release", "view", $tag, "--repo", $Repo, "--json", "tagName")).Code -eq 0) { Problem "GitHub release $tag already exists in $Repo (edit or delete it on GitHub)" -OnlineOnly }
    else {
        Say "gh is logged in; no release $tag in $Repo yet"
        if ($Final -or $DryRun) {
            try {
                $nowLatest = @((Get-Releases) | Where-Object { $_.isLatest } | ForEach-Object { $_.tagName })
                if (@($nowLatest | Where-Object { $_ -match '^(assets|textures)-' }).Count) { Warn "GitHub's Latest is now $($nowLatest -join ', '), an asset/texture release (the installer's feed 404s until a game release is Latest)$(if ($Final) { ': -Final marks ' + $tag + ' Latest' })" }
                else { Say "GitHub's Latest now: $(if ($nowLatest) { $nowLatest -join ', ' } else { 'none' })" }
            } catch { Warn "could not list the releases (gh release list): $($_.Exception.Message)" }
        }
    }
}
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
# The Visual Studio that has the engine's toolset (ClangCL: "C++ Clang tools for Windows"; quakevr.toolset.props).
if (-not $MSBuild -and (Test-Path $vswhere)) {
    $MSBuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset `
        -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
}
if (-not $MSBuild -or -not (Test-Path -LiteralPath $MSBuild)) { Problem "MSBuild with the ClangCL toolset not found (Visual Studio 2022 with C++ and 'C++ Clang tools for Windows'; or -MSBuild)" } else { Say "MSBuild: $MSBuild" }
$dotnet = Run "dotnet" @("--version")   # (in the repository root; the installer's global.json is checked below)
Push-Location $installerDir; try { $dotnetSdk = Run "dotnet" @("--version") } finally { Pop-Location }
if ($dotnet.Code -ne 0 -and $dotnetSdk.Code -ne 0) { Problem ".NET SDK not found (dotnet)" }
elseif ($dotnetSdk.Code -ne 0) { Problem "no .NET SDK matching Installer\global.json (9.0.305 or a newer 9.0 band)" }
else { Say ".NET SDK $($dotnetSdk.Text)" }
$python = if (Get-Command python -ErrorAction SilentlyContinue) { "python" } else { $null }
if (-not $python) { Problem "python not found (the checks and make_release.py need Python 3)" }
if (-not $Fteqcc) {
    $Fteqcc = @($env:FTEQCC, (Join-Path $root "QC\fteqcc64.exe"), "C:\OHWorkspace\quakevr\QC\fteqcc64.exe") |
        Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -First 1
    if (-not $Fteqcc) { $Fteqcc = (Get-Command fteqcc64 -ErrorAction SilentlyContinue | Select-Object -First 1).Source }
}
if (-not $Fteqcc -or -not (Test-Path -LiteralPath $Fteqcc)) { Problem "fteqcc64.exe not found (-Fteqcc, FTEQCC, QC\fteqcc64.exe or PATH)" } else { $Fteqcc = (Resolve-Path $Fteqcc).Path; Say "fteqcc: $Fteqcc" }

# The test suite's worktree: a kit worktree whose tracked files are this commit's (VERSION aside: -BumpVersion's commit
# comes after the tests).
$testRunner = Join-Path $PSScriptRoot "run_test_suite.py"
$testArgs = @()
if ($RunTests) {
    if (-not $TestAgent -and $root -match '\\qvr-agents\\([^\\]+)$') { $TestAgent = $Matches[1] }
    if ($TestOnly) { $testArgs += @("--only", $TestOnly) }
    if ($AllowFlaky) { $testArgs += "--allow-flaky" }
    if ($FlakyTests) { $testArgs += @("--flaky", ($FlakyTests -join ",")) }
    $testList = Run $python (@($testRunner, "--list") + $testArgs)
    if ($testList.Code -ne 0) { Problem "-RunTests: $($testList.Text)" }
    $testCount = "$(@($testList.Out | Where-Object { "$_" -match ' tests$' }) | Select-Object -Last 1)"
    $testTree = if ($TestAgent -eq "cleanup") { "C:\OHWorkspace\quakevr-iw-cleanup" } elseif ($TestAgent) { "C:\OHWorkspace\qvr-agents\$TestAgent" } else { "" }
    if (-not $TestAgent) { Problem "-RunTests: which kit worktree? -TestAgent <name> or QVR_TEST_AGENT (the kit's new_agent.sh <name> $short makes one at this commit)" }
    elseif (-not (Test-Path -LiteralPath $testTree)) { Problem "-RunTests: no kit worktree $testTree (the kit's new_agent.sh $TestAgent $short)" }
    else {
        $testHead = (Run "git" @("-C", $testTree, "rev-parse", "HEAD")).Text
        $testDirty = (Run "git" @("-C", $testTree, "--no-optional-locks", "status", "--porcelain", "--untracked-files=no")).Text
        $testSame = $testHead -and (GitRun @("diff", "--quiet", $testHead, "HEAD", "--", ".", ":(exclude)VERSION")).Code -eq 0
        if ($testDirty) { Problem "-RunTests: $testTree has uncommitted changes (the tests must run this commit's files)" }
        elseif (-not $testSame) { Problem "-RunTests: $testTree ($(if ($testHead) { $testHead.Substring(0, 8) } else { 'no HEAD' })) has other files than ${short}: git -C $testTree checkout --detach $short" }
        else { Say "tests: $testCount on $testTree ($TestAgent, $($testHead.Substring(0, 8)): this commit's files), built first (kit build.sh)" }
    }
}

# Optional inputs.
if ($Textures -and -not (Test-Path -LiteralPath $Textures -PathType Leaf)) { Problem "-Textures: no file $Textures" }
if ($hostedTextures) { Say "HD textures: hosted $((SupportFile 'hdtextures').file) ($(Size (SupportFile 'hdtextures').size), $($support.tag)): latest.json points at it, nothing uploaded" }
elseif ($NoTextures) { Say "HD textures: none (-NoTextures): latest.json has no hdtextures component" }
foreach ($a in $Assets) { if (-not (Test-Path -LiteralPath $a -PathType Leaf)) { Problem "-Assets: no file $a" } }
$ericwTools = if ($env:QVR_ERICW_TOOLS) { $env:QVR_ERICW_TOOLS } else { "C:\OHWorkspace\ericw-tools-2.0.0-alpha11-win64" }
$shipsEricw = Test-Path (Join-Path $ericwTools "light.exe")
if (-not $shipsEricw) { Warn "ericw-tools not found in $ericwTools (QVR_ERICW_TOOLS): the package will have no light.exe" }
elseif (-not $EricwSource) { Say "ericw-tools' source (GPL-3, light.exe ships): hosted, linked from the release notes: $(SupportUrl 'ericw_source')" }
elseif (-not (Test-Path -LiteralPath $EricwSource -PathType Leaf)) { Problem "-EricwSource: no file $EricwSource" }
# The hosted files this release relies on, checked against GitHub (sizes and digests from the API, then a HEAD request
# per URL; nothing downloaded). Not with -DryRun or -Local (nothing online).
$hostedKeys = @(@($(if ($hostedTextures) { "hdtextures" })) + @($(if ($shipsEricw -and -not $EricwSource) { "ericw_source" })) | Where-Object { $_ })
if ($hostedKeys.Count -and -not $DryRun -and -not $Local) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    try {
        $rel = Invoke-RestMethod -UseBasicParsing -TimeoutSec 30 -Headers @{ Accept = "application/vnd.github+json" } `
            -Uri "https://api.github.com/repos/$($support.repo)/releases/tags/$($support.tag)"
        foreach ($k in $hostedKeys) {
            $e = SupportFile $k
            $a = @($rel.assets | Where-Object { $_.name -eq $e.file }) | Select-Object -First 1
            if (-not $a) { Problem "support release $($support.tag) has no $($e.file)" -OnlineOnly; continue }
            $digest = if ($a.PSObject.Properties["digest"] -and $a.digest) { "$($a.digest)" } else { "" }
            if ([long]$a.size -ne [long]$e.size -or ($digest -and $digest -ne "sha256:$($e.sha256)")) {
                Problem "$($e.file) on $($support.tag) is $($a.size) bytes, $digest; support_assets.json says $($e.size), sha256:$($e.sha256)" -OnlineOnly
                continue
            }
            $u = SupportUrl $k
            try {
                $r = Invoke-WebRequest -UseBasicParsing -Method Head -TimeoutSec 30 -Uri $u
                Say "hosted ${k}: $u answers ($($r.StatusCode)); $($a.size) bytes, $(if ($digest) { 'digest matches' } else { 'no digest from GitHub' })"
            } catch { Problem "hosted ${k}: $u does not answer ($($_.Exception.Message))" -OnlineOnly }
        }
    } catch { Problem "could not read the support release $($support.tag) from GitHub's API ($($_.Exception.Message))" -OnlineOnly }
}
# The Quake for the smoke launch: -QuakeDir, QVR_QUAKE_DIR, else the one the installer picks by itself (its Steam, GOG
# and Epic detection: qvr-setup detect, the folder it names for its expansions). Only read: the paks are linked.
$quakeFrom = if (-not $QuakeDir) { "" } elseif ($PSBoundParameters.ContainsKey("QuakeDir")) { "-QuakeDir" } else { "QVR_QUAKE_DIR" }
$detectAfterBuild = $false
if (-not $QuakeDir -and -not $SkipSmoke) {
    $qvrSetupExe = Find-QvrSetup
    if ($qvrSetupExe) { $QuakeDir = Find-QuakeDir $qvrSetupExe; if ($QuakeDir) { $quakeFrom = "detected by $qvrSetupExe detect" } }
    else { $detectAfterBuild = $true }   # (-DryRun: said so; the real run detects it after building qvr-setup)
}
$quakeOk = $QuakeDir -and (Test-Path (Join-Path $QuakeDir "id1\pak0.pak"))
if ($SkipSmoke) { Warn "-SkipSmoke: no launch of the packaged game" }
elseif ($quakeOk) { Say "Quake for the smoke launch: $QuakeDir ($quakeFrom)" }
elseif ($detectAfterBuild) { Say "Quake for the smoke launch: detected after the installer's build (qvr-setup detect; -QuakeDir or QVR_QUAKE_DIR to name it)" }
elseif ($QuakeDir) { Warn "$QuakeDir ($quakeFrom) has no id1\pak0.pak: the packaged game's smoke launch is skipped" }
else { Warn "no Quake folder: none given (-QuakeDir, QVR_QUAKE_DIR) and $(if (Find-QvrSetup) { 'qvr-setup detect found none' } else { 'no qvr-setup built to detect one (it is after the build)' }): the packaged game's smoke launch is skipped" }

if ($problems.Count) { throw "$($problems.Count) problem(s) above: nothing was built or published" }

if ($DryRun) {
    Step "Plan (-DryRun: nothing is built, tagged or published)"
    Say "version text   $versionText"
    Say "output         $outDir$(if (Test-Path $outDir) { ' (exists: it would be moved aside)' })"
    Say "engine         MSBuild ironwail.sln Release|x64 $(if ($Rebuild) { '(rebuild) ' })/p:QvrReleaseVersion=$Version"
    Say "installer      dotnet publish QuakeVR.Installer -c Release -r win-x64 single-file, /p:Version=$Version"
    Say "checks         statics, QC precedence, FGD, installer self-tests, latest.json, packaged installer (install harness), smoke launch$(if ((-not $quakeOk -and -not $detectAfterBuild) -or $SkipSmoke) { ' (skipped)' })"
    Say "assets         QuakeVR.zip, QuakeVR-Setup.exe, latest.json, SHA256SUMS.txt$(if ($Textures) { ', ' + (Split-Path -Leaf $Textures) })$(if ($EricwSource) { ', ' + (Split-Path -Leaf $EricwSource) })$(foreach ($a in $Assets) { ', ' + (Split-Path -Leaf $a) })"
    Say "hdtextures     $(if ($hostedTextures) { "hosted: $(SupportUrl 'hdtextures') (not uploaded; checked online when building, not with -DryRun)" } elseif ($Textures) { 'uploaded with this release (-Textures)' } else { 'none (-NoTextures)' })"
    if ($shipsEricw) { Say "ericw source   $(if ($EricwSource) { 'uploaded with this release (-EricwSource)' } else { "linked from the notes: $(SupportUrl 'ericw_source')" })" }
    Say "tag            $tag on $short$(if ($online) { ", pushed to $remote" } else { ' (only with -Publish or -PushTag)' })"
    if ($Local) { Say "local test     latest.json -> $($UrlBase -join ', ')$(if ($RunInstaller) { '; then the server and QuakeVR-Setup.exe in a sandbox' })" }
    Say "tests          $(if ($RunTests) { "$testCount on $TestAgent, one at a time, before building (run_test_suite.py $($testArgs -join ' ')); a failure stops everything$(if ($AllowFlaky) { ' (known-flaky ones warn)' })" } else { 'not run (-RunTests runs the headless suite first)' })"
    Say "release        $(if ($Publish) { "gh release create $tag --repo $Repo$(if ($Draft) { ' --draft' })$(if ($prerelease) { ' --prerelease' } elseif (-not $Draft) { ' --latest' })$(if ($Final) { "$(if ($moveReleaseBranch) { ", then $ReleaseBranch fast-forwarded to it" }), then the Latest guard and the online check" })" } else { 'none (-Publish creates it)' })"
    Say "$("{0,-15}" -f $ReleaseBranch)$(if ($moveReleaseBranch) { "fast-forwarded on $(if ($remote) { $remote } else { '<the remote>' }) to $tag after the release (never forced)" } elseif ($Final) { 'not moved (-NoReleaseBranch)' } else { "not moved (only a -Final release moves it)" })"
    Say "notes          $(if ($notesSource) { "$notesSource$(if ($notesText.Contains($draftMarker)) { ' (still the DRAFT)' })" } else { "drafted while building into $notesDraftPath (-DraftNotes drafts it now)" })"
    $list = & (Join-Path $root "Windows\package-quakevr.ps1") -DryRun
    Say "package        $(@($list).Count) files (Windows\package-quakevr.ps1 -DryRun lists them)"
    # Every command of the real run, in order (what -DryRun leaves out of this mode is marked).
    $feedPlan = if ($FeedUrl) { $FeedUrl } elseif ($prerelease) { $tagFeed } else { "https://github.com/$Repo/releases/latest/download/latest.json" }
    $title = "Quake VR: Unleashed $Version"
    $commitPlan = if ($bumpPending) { "<the version commit>" } else { $short }
    $seq = New-Object System.Collections.Generic.List[string]
    if ($RunTests) { $seq.Add("tests: python Misc\release\run_test_suite.py $TestAgent --build --log-dir $outDir\tests $($testArgs -join ' ')   (kit build.sh $TestAgent, then $testCount one at a time; a failure stops here)") }
    if ($bumpPending) { $seq.Add("write VERSION = $Version; git commit -m ""Version $Version"" -- VERSION   (local)") }
    $seq.Add("build: out\release\$Version moved aside if there; check_statics.py, check_qc_precedence.py, fgdgen.py --check; MSBuild ironwail.sln Release|x64 /p:QvrReleaseVersion=$Version; package-quakevr.ps1; dotnet publish QuakeVR.Installer; dotnet build QuakeVR.Installer.sln; the installer self-tests")
    $seq.Add("assets: make_release.py (QuakeVR.zip, QuakeVR-Setup.exe, latest.json); checks: the zip = the allowlist, qvr-setup feed --file latest.json --assets, QuakeVR-Setup.exe's install harness + qvr-setup verify$(if ($quakeOk -and -not $SkipSmoke) { ", the smoke launch with $QuakeDir" } elseif ($detectAfterBuild) { ', the smoke launch with the Quake qvr-setup detect finds after the build' } else { ' (no smoke launch)' })")
    $seq.Add("notes: $(if ($notesSource) { $notesSource } else { "draft_release_notes.py -> $notesDraftPath" }) -> $outDir\release-body.md (+ the files' table, SHA256SUMS.txt)")
    if ($online) {
        if ($pushBranch) { $seq.Add("git push $remote HEAD:$upstreamRef   (fast-forward; never forced)") }
        if ($NoBranchPush) { $seq.Add("(-NoBranchPush: $branch is not pushed; the tag below carries $commitPlan to $remote)") }
        $seq.Add("git tag -a $tag -m ""$title (<date> <hash>)"" $commitPlan; git push $remote refs/tags/$tag")
    }
    if ($Publish) {
        $seq.Add("gh release create $tag <the files of $outDir\assets> --repo $Repo --verify-tag --title ""$title"" --notes-file $outDir\release-body.md$(if ($Draft) { ' --draft' })$(if ($prerelease) { ' --prerelease' } elseif (-not $Draft) { ' --latest' })")
    }
    if ($Final) {
        if ($moveReleaseBranch) { $seq.Add("release branch: $(Get-ReleaseBranchPush)   (fast-forward only, never forced; stops if $(if ($remote) { $remote } else { '<the remote>' }) $ReleaseBranch is not an ancestor of $tag)") }
        else { $seq.Add("(-NoReleaseBranch: $ReleaseBranch is not moved)") }
    }
    $checkPublished = $Final -or ($Publish -and $prerelease -and -not $Draft)
    if ($checkPublished -and $prerelease) { $seq.Add("Latest: gh release list --repo $Repo --json tagName,isLatest,isDraft,isPrerelease; $tag must be a published prerelease, not Latest (nothing edited)") }
    if ($checkPublished) {
        if (-not $prerelease) { $seq.Add("Latest guard: gh release list --repo $Repo --json tagName,isLatest,isDraft,isPrerelease; $tag must be the only Latest (no assets-*/textures-*): else gh release edit $tag --repo $Repo --latest, listed again") }
        $seq.Add("online check: qvr-setup feed --url $feedPlan --assets $outDir\assets$(if ($hostedTextures) { ' --hosted hdtextures' })   (until it names $Version, up to $OnlineTries tries 20 s apart; each file's size and SHA-256)")
        $seq.Add("online check: GET $feedPlan = assets\latest.json byte for byte")
        if (-not $SkipOnlineInstall) { $seq.Add("online check: qvr-setup install --feed $feedPlan --sandbox $outDir\checks\online-<time>\sandbox --quake $(if ($QuakeDir) { $QuakeDir } elseif ($detectAfterBuild) { '<qvr-setup detect, after the build>' } else { '<none found: skipped>' }) --accept-statement --setup-from assets\QuakeVR-Setup.exe$(if ($OnlineHd) { ' --hd' }); qvr-setup verify --target <sandbox>\QuakeVR") }
    }
    Say ""
    Say "The run, in order (none of it with -DryRun):"
    $i = 0
    foreach ($l in $seq) { Say ("  {0,2}. {1}" -f (++$i), $l) }
    if ($warnings.Count) { Say ""; Say "$($warnings.Count) warning(s) above." }
    return
}

# ------------------------------------------------------------------------------------------------------------------
Step "Output folder"
if (Test-Path $outDir) {
    $aside = "$outDir.old-$(Get-Date -Format yyyyMMdd-HHmmss)"
    Move-Item -LiteralPath $outDir $aside
    Say "moved the previous run aside: $aside"
}
$logs = Join-Path $outDir "logs"
$checks = Join-Path $outDir "checks"
$assetsDir = Join-Path $outDir "assets"
$packageDir = Join-Path $outDir "package\QuakeVR"
$installerOut = Join-Path $outDir "installer"
New-Item -ItemType Directory -Force $logs, $checks, $installerOut, (Split-Path $packageDir) | Out-Null
Say $outDir

$script:stage = "tests"
if ($RunTests) {
    Step "Tests: run_test_suite.py on $TestAgent ($testCount, one at a time; before anything is built)"
    $testLogs = Join-Path $outDir "tests"
    & $python (@($testRunner, $TestAgent, "--build", "--log-dir", $testLogs) + $testArgs) | ForEach-Object { Say "  $_" }
    $testCode = $LASTEXITCODE
    if ($testCode -ne 0) { throw "the test suite failed (exit $testCode; $testLogs\summary.txt): nothing was built, tagged or published" }
    foreach ($l in @(Get-Content (Join-Path $testLogs "summary.txt") | Where-Object { $_ -match '^\s+FLAKY ' })) { Warn "a known-flaky test failed (-AllowFlaky): $($l.Trim())" }
}

$script:stage = "build"
$script:bumped = $false
if ($bumpPending) {
    Step "VERSION = $Version (its own commit)"
    if ((GitRun @("--no-optional-locks", "status", "--porcelain", "--untracked-files=no")).Text) { throw "-BumpVersion: the tree changed during the tests: commit or stash those changes, then run again" }
    [System.IO.File]::WriteAllText($versionFile, "$Version`n", (New-Object System.Text.UTF8Encoding($false)))
    if ((GitRun @("commit", "-q", "-m", "Version $Version", "--", "VERSION")).Code -ne 0) { throw "-BumpVersion: git commit of VERSION failed" }
    $script:bumped = $true; $bumpPending = $false; $fileVersion = $Version
    $commit = (GitRun @("rev-parse", "HEAD")).Text
    $short = (GitRun @("rev-parse", "--short=8", "HEAD")).Text
    $buildStamp = (GitRun @("log", "-1", "--date=format:%Y-%m-%d", "--format=%cd %h", "--abbrev=8")).Text
    $versionText = "$Version ($buildStamp)"
    Say "committed VERSION = $Version ($short; not pushed yet)"
}

# ------------------------------------------------------------------------------------------------------------------
Step "Source checks (statics, QC precedence, FGD)"
Invoke-Logged $python @("Misc\quakevr\check_statics.py") (Join-Path $logs "check_statics.log") | Out-Null
Say "statics: ok"
if (Test-Path (Join-Path $root "Misc\quakevr\check_qc_precedence.py")) {
    Invoke-Logged $python @("Misc\quakevr\check_qc_precedence.py", "--fteqcc", $Fteqcc) (Join-Path $logs "check_qc_precedence.log") | Out-Null
    Say "QC precedence: ok"
}
Invoke-Logged $python @("Misc\trenchbroom\fgdgen.py", "--check") (Join-Path $logs "fgd_check.log") | Out-Null
Say "FGD: ok"

# ------------------------------------------------------------------------------------------------------------------
Step "Engine and QuakeC (Release x64, $versionText)"
$msbuildArgs = @("Windows\VisualStudio\ironwail.sln", "-p:Configuration=Release", "-p:Platform=x64", "-m", "-v:m", "-nologo",
    "-p:QvrQcCompiler=$Fteqcc", "-p:QvrReleaseVersion=$Version")
if ($Rebuild) { $msbuildArgs += "-t:Rebuild" }
Invoke-Logged $msbuild $msbuildArgs (Join-Path $logs "msbuild.log") | Out-Null
$exe = Join-Path $root "Windows\VisualStudio\Build-ironwail\bin\x64\Release\ironwail.exe"
$built = Get-Item $exe
if ($built.LastWriteTime -lt (Get-Date).AddDays(-30)) { Warn "ironwail.exe is from $($built.LastWriteTime): an incremental build left it (use -Rebuild if in doubt)" }
$warnCount = @(Select-String -Path (Join-Path $logs "msbuild.log") -Pattern "warning C\d+" -ErrorAction SilentlyContinue).Count
Say "built $exe ($warnCount compiler warning lines)"

# ------------------------------------------------------------------------------------------------------------------
Step "Package (Windows\package-quakevr.ps1, the allowlist)"
$pkgLog = Join-Path $logs "package.log"
& (Join-Path $root "Windows\package-quakevr.ps1") -Fteqcc $Fteqcc -Dist $packageDir -NoZip -Version $versionText *>&1 |
    ForEach-Object { "$_" } | Set-Content -Encoding utf8 $pkgLog
Get-Content $pkgLog | Select-Object -Last 3 | ForEach-Object { Say "  $_" }
$manifest = Get-Content -Raw (Join-Path $packageDir "manifest.json") | ConvertFrom-Json
if ($manifest.version -ne $versionText) { throw "manifest.json's version is '$($manifest.version)', expected '$versionText'" }
if ($shipsEricw -and -not (Test-Path (Join-Path $packageDir "quakevr\tools\ericw-tools\light.exe"))) { throw "the package has no ericw-tools light.exe" }

# ------------------------------------------------------------------------------------------------------------------
Step "Installer (dotnet publish, Release, single file)"
$csproj = Join-Path $installerDir "src\QuakeVR.Installer\QuakeVR.Installer.csproj"
Invoke-Logged "dotnet" @("publish", $csproj, "-c", "Release", "-r", "win-x64", "--self-contained", "-p:PublishSingleFile=true",
    "-p:IncludeNativeLibrariesForSelfExtract=true", "-p:EnableCompressionInSingleFile=true", "-p:Version=$Version",
    "-o", $installerOut, "-nologo") (Join-Path $logs "installer_publish.log") $installerDir | Out-Null
$setup = Join-Path $installerOut "QuakeVR-Setup.exe"
$extra = @(Get-ChildItem $installerOut -File | Where-Object { $_.Extension -ne ".pdb" -and $_.Name -ne "QuakeVR-Setup.exe" })
if ($extra.Count) { throw "dotnet publish left files beside QuakeVR-Setup.exe that a single download would miss: $($extra.Name -join ', ')" }
$setupVersion = (Get-Item $setup).VersionInfo.ProductVersion
if (-not $setupVersion.StartsWith($Version)) { throw "QuakeVR-Setup.exe's version is $setupVersion, expected $Version" }
Say "$setup ($(Size (Get-Item $setup).Length), version $setupVersion)"
# The rest of the solution (qvr-setup, the self-tests) in Release, for the checks below (warnings are errors).
Invoke-Logged "dotnet" @("build", "QuakeVR.Installer.sln", "-c", "Release", "-p:Version=$Version", "-nologo") (Join-Path $logs "installer_build.log") $installerDir | Out-Null
if ($Local) {
    # This build's qvr-setup beside the release: test_local_release.ps1 serves the assets with it ("qvr-setup serve").
    $cliBin = Join-Path $installerDir "src\QuakeVR.Installer.Cli\bin\Release\net9.0-windows"
    $tools = Join-Path $outDir "tools\qvr-setup"
    New-Item -ItemType Directory -Force $tools | Out-Null
    Copy-Item (Join-Path $cliBin "*") $tools -Recurse -Force
    Say "qvr-setup (the local server): $tools"
}
if ($detectAfterBuild) {
    $QuakeDir = Find-QuakeDir (Find-QvrSetup)
    $quakeOk = $QuakeDir -and (Test-Path (Join-Path $QuakeDir "id1\pak0.pak"))
    if ($quakeOk) { Say "Quake for the smoke launch: $QuakeDir (detected by qvr-setup detect)" }
    else { Warn "no Quake folder: none given (-QuakeDir, QVR_QUAKE_DIR) and qvr-setup detect found none: the packaged game's smoke launch is skipped" }
}

if ($SkipInstallerTests) { Warn "-SkipInstallerTests: the installer's self-tests were not run" }
else {
    Step "Installer self-tests"
    $selfLog = Join-Path $logs "installer_selftest.log"
    Invoke-Logged "dotnet" @("run", "--project", "tests\QuakeVR.Installer.SelfTest", "-c", "Release", "--no-build", "--", (Join-Path $checks "selftest")) $selfLog $installerDir | Out-Null
    Say "  $((Get-Content $selfLog | Select-String 'passed' | Select-Object -Last 1).Line)"
}

# ------------------------------------------------------------------------------------------------------------------
Step "Release assets and latest.json (Misc\quakevr\make_release.py)"
$mrArgs = @("Misc\quakevr\make_release.py", "--package", $packageDir, "--setup", $setup, "--tag", $tag, "--out", $assetsDir)
$mrArgs += @("--support-assets", $supportPath)
if ($Textures) { $mrArgs += @("--textures", (Resolve-Path $Textures).Path) }
elseif ($NoTextures) { $mrArgs += "--no-textures" }
if ($EricwSource) { $mrArgs += @("--asset", (Resolve-Path $EricwSource).Path) }
foreach ($a in $Assets) { $mrArgs += @("--asset", (Resolve-Path $a).Path) }
foreach ($u in $UrlBase) { $mrArgs += @("--url-base", $u) }
Invoke-Logged $python $mrArgs (Join-Path $logs "make_release.log") | Out-Null
# Its PUBLISH.txt describes the manual route; this script's own instructions replace it, and assets\ is exactly the upload.
Move-Item (Join-Path $assetsDir "PUBLISH.txt") (Join-Path $logs "make_release-PUBLISH.txt")

# The zip against the allowlist: exactly package-quakevr.ps1's list (manifest.json included), nothing of id's.
Step "Checks: the zip against the allowlist"
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipPath = Join-Path $assetsDir "QuakeVR.zip"
$z = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
try { $entries = @($z.Entries | Where-Object { $_.Name } | ForEach-Object { $_.FullName.Replace('\', '/') }) } finally { $z.Dispose() }
$allow = @(& (Join-Path $root "Windows\package-quakevr.ps1") -DryRun | ForEach-Object { "$_" })
$extraInZip = @($entries | Where-Object { $allow -notcontains $_ })
$missingFromZip = @($allow | Where-Object { $entries -notcontains $_ })
if ($extraInZip.Count -or $missingFromZip.Count) {
    throw "QuakeVR.zip differs from the allowlist: extra [$($extraInZip -join ', ')], missing [$($missingFromZip -join ', ')]"
}
$idFiles = @($entries | Where-Object { ($_ -match '^(id1|hipnotic|rogue)/') -or ($_ -match '(^|/)(pak\d+\.pak|gfx\.wad)$') })
if ($idFiles.Count) { throw "QuakeVR.zip has id Software's files: $($idFiles -join ', ')" }
$pdbs = @($entries | Where-Object { $_ -like "*.pdb" })
Say "$($entries.Count) files, exactly the allowlist; no id files; PDBs: $($pdbs -join ', ') (ironwail.pdb ships on purpose: crash reports name functions with it)"

Step "Checks: latest.json as the installer reads it"
$feedLog = Join-Path $logs "feed_check.log"
$feedArgs = @("run", "--project", "src\QuakeVR.Installer.Cli", "-c", "Release", "--no-build", "--", "feed", "--file", (Join-Path $assetsDir "latest.json"), "--assets", $assetsDir)
if ($hostedTextures) { $feedArgs += @("--hosted", "hdtextures") }   # (on the support-files release, not in assets\)
Invoke-Logged "dotnet" $feedArgs $feedLog $installerDir | Out-Null
Get-Content $feedLog | ForEach-Object { Say "  $_" }
$feed = Get-Content -Raw (Join-Path $assetsDir "latest.json") | ConvertFrom-Json
if ($feed.version -ne $versionText) { throw "latest.json's version is '$($feed.version)', expected '$versionText'" }
if ($hostedTextures) {
    $hd = $feed.components.hdtextures; $e = SupportFile "hdtextures"
    if (-not $hd -or $hd.file -ne $e.file -or [long]$hd.size -ne [long]$e.size -or $hd.sha256 -ne $e.sha256 -or $hd.urls[0] -ne (SupportUrl "hdtextures")) {
        throw "latest.json's hdtextures is not the hosted $($e.file) of support_assets.json"
    }
    Say "  hdtextures: the hosted $($hd.file) ($(Size $hd.size)), $(@($hd.urls).Count) url(s), first $($hd.urls[0])"
}

if ($SkipInstallerTests) { Warn "-SkipInstallerTests: the packaged installer was not run" }
else {
    Step "Checks: the packaged QuakeVR-Setup.exe installs QuakeVR.zip (its off-screen harness, offline, into checks\)"
    $h = Join-Path $checks "setup"
    $hArgs = @("--screenshots", (Join-Path $h "pages"), "--package", $zipPath, "--target", (Join-Path $h "install"),
        "--shortcuts-dir", (Join-Path $h "shortcuts"), "--registry-file", (Join-Path $h "registry.json"),
        "--offline", "--silent", "--no-prerequisites", "--downloads", (Join-Path $h "downloads"))
    $code = Invoke-Logged (Join-Path $assetsDir "QuakeVR-Setup.exe") $hArgs (Join-Path $logs "setup_harness.log") -NoThrow
    $report = Join-Path $h "pages\report.txt"
    if ($code -ne 0) {
        if (Test-Path $report) { Get-Content $report | Select-Object -Last 15 | ForEach-Object { Say "    $_" } }
        throw "QuakeVR-Setup.exe's install harness failed (exit $code; $report)"
    }
    $verifyLog = Join-Path $logs "setup_verify.log"
    Invoke-Logged "dotnet" @("run", "--project", "src\QuakeVR.Installer.Cli", "-c", "Release", "--no-build", "--", "verify", "--target", (Join-Path $h "install")) $verifyLog $installerDir | Out-Null
    $record = Get-Content -Raw (Join-Path $h "install\install.json") | ConvertFrom-Json
    if ($record.version -ne $versionText) { throw "the harness installed version '$($record.version)', expected '$versionText'" }
    Say "  installed $($record.version) into $(Join-Path $h 'install') (pages: $(Join-Path $h 'pages'))"
    Say "  verify: $((Get-Content $verifyLog | Select-Object -Last 1))"
}

# ------------------------------------------------------------------------------------------------------------------
if (-not $SkipSmoke -and $quakeOk) {
    Step "Checks: smoke launch of the packaged game (headless mock headset)"
    # The zip unpacked into its own folder, with Quake's paks linked (or copied) beside it: never written into the Quake
    # folder. The id paks stay in checks\ (git-ignored, never uploaded).
    $smoke = Join-Path $checks "smoke"
    Expand-Archive -LiteralPath $zipPath -DestinationPath $smoke
    New-Item -ItemType Directory -Force (Join-Path $smoke "id1") | Out-Null
    foreach ($pak in "pak0.pak", "pak1.pak") {
        $src = Join-Path $QuakeDir "id1\$pak"
        if (-not (Test-Path $src)) { continue }
        $dst = Join-Path $smoke "id1\$pak"
        try { New-Item -ItemType HardLink -Path $dst -Target (Resolve-Path $src).Path -ErrorAction Stop | Out-Null } catch { Copy-Item $src $dst }
    }
    $script = @("vr_backend mock", "sv_autosave 0", "vr_mock_fast 1", "version", "map start") + (1..120 | ForEach-Object { "wait" }) +
        @("echo QVR_SMOKE_OK", "disconnect") + (1..5 | ForEach-Object { "wait" }) + @("quit")
    Set-Content -Encoding ascii (Join-Path $smoke "quakevr\autoexec.cfg") ($script -join "`n")
    $env:QVR_NO_ERROR_DIALOG = "1"; $env:QVR_TEST_HIDDEN = "1"; $env:QVR_TEST_BACKGROUND = "1"
    try {
        $p = Start-Process (Join-Path $smoke "ironwail.exe") -WorkingDirectory $smoke -PassThru -ArgumentList `
            "-basedir `"$smoke`" -game quakevr -condebug -window -width 960 -height 540 -nosound -noconfigwrite -noaddons"
        $null = $p.Handle
        if (-not $p.WaitForExit(240000)) { $p.Kill(); $p.WaitForExit(); throw "smoke launch: no exit after 240 s" }
    } finally { $env:QVR_NO_ERROR_DIALOG = $null; $env:QVR_TEST_HIDDEN = $null; $env:QVR_TEST_BACKGROUND = $null }
    $conlog = Get-ChildItem $smoke -Recurse -Filter qconsole.log | Select-Object -First 1
    $text = if ($conlog) { Get-Content -Raw $conlog.FullName } else { "" }
    $fails = @()
    if ($p.ExitCode -ne 0) { $fails += "exit code $($p.ExitCode)" }
    foreach ($f in "qvr_error.txt", "qvr_crash.txt") { if (Get-ChildItem $smoke -Recurse -Filter $f) { $fails += "$f written" } }
    if ($text -notmatch 'QVR_SMOKE_OK') { $fails += "the map did not run to the end of the script" }
    if (-not $text.Contains("Quake VR   $versionText")) { $fails += "the console does not name the build '$versionText'" }
    if ($fails.Count) { throw "smoke launch failed: $($fails -join '; ') (log: $(if ($conlog) { $conlog.FullName } else { 'none' }))" }
    Say "ironwail.exe from the zip ran map start with the mock headset and quit cleanly; console: Quake VR   $versionText"
}

# A file edited while this ran would make the engine say "-dirty" (its build reads git status) and the tag lie.
$dirtyNow = (GitRun @("--no-optional-locks", "status", "--porcelain", "--untracked-files=no")).Text
if ($dirtyNow -ne $dirty -or (GitRun @("rev-parse", "HEAD")).Text -ne $commit) { throw "the tree or HEAD changed while the release was being made: run it again`n$dirtyNow" }

# ------------------------------------------------------------------------------------------------------------------
Step "SHA-256 and release notes"
$files = @(Get-ChildItem $assetsDir -File | Sort-Object Name)
$sums = foreach ($f in $files) { "$((Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash.ToLowerInvariant())  $($f.Name)" }
$sumsPath = Join-Path $assetsDir "SHA256SUMS.txt"
[System.IO.File]::WriteAllText($sumsPath, (($sums -join "`n") + "`n"), (New-Object System.Text.UTF8Encoding $false))
$files = @(Get-ChildItem $assetsDir -File | Sort-Object Name)

# The notes: -Notes, his edited out\release\<version>\release-notes.md (read before the folder was moved aside: written
# back), else a draft made now (draft_release_notes.py) to edit before -Publish. The body uploaded is release-body.md:
# the notes without the DRAFT line, plus the files' table.
$notesPath = $notesDraftPath
if ($notesText) { [System.IO.File]::WriteAllText($notesPath, $notesText, (New-Object System.Text.UTF8Encoding $false)) }
else {
    Invoke-Logged $python @($notesDrafter, "--root", $root, "--version", $Version, "--out", $notesPath) (Join-Path $logs "draft_release_notes.log") | Out-Null
    $notesText = [System.IO.File]::ReadAllText($notesPath)
}
$body = (($notesText -split "\r?\n") | Where-Object { -not $_.StartsWith($draftMarker) }) -join "`n"
$table = "## Files`n`nBuild: Quake VR $versionText, commit $commit.`n`n| File | Size | SHA-256 |`n|---|---|---|`n" +
    (($files | Where-Object { $_.Name -ne "SHA256SUMS.txt" } | ForEach-Object { "| ``$($_.Name)`` | $(Size $_.Length) | ``$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant())`` |" }) -join "`n") +
    "`n`n" + $(if ($hostedTextures) { "HD texture pack (the installer's optional HD textures; not attached here): [``$((SupportFile 'hdtextures').file)``]($(SupportUrl 'hdtextures')), $(Size (SupportFile 'hdtextures').size), SHA-256 ``$((SupportFile 'hdtextures').sha256)``.`n`n" } else { "" }) +
    $(if ($shipsEricw) { "Source of ericw-tools' light.exe (GPL-3; QuakeVR.zip ships light.exe): $(if ($EricwSource) { "https://github.com/$Repo/releases/download/$tag/$(Split-Path -Leaf $EricwSource)" } else { SupportUrl 'ericw_source' })`n`n" } else { "" }) +
    "The installer and the game are not code-signed: Windows SmartScreen may say ""Windows protected your PC"" (More info > Run anyway). Check a download with ``Get-FileHash <file>`` against the table."
$bodyPath = Join-Path $outDir "release-body.md"
[System.IO.File]::WriteAllText($bodyPath, ($body.Trim() + "`n`n" + $table + "`n"), (New-Object System.Text.UTF8Encoding $false))
Say "notes $notesPath$(if ($notesText.Contains($draftMarker)) { ' (the draft: edit it, delete its DRAFT line)' }); the release's body: $bodyPath"

# ------------------------------------------------------------------------------------------------------------------
$assetArgs = @($files | ForEach-Object { $_.FullName })
$title = "Quake VR: Unleashed $Version"
$ghArgs = @("release", "create", $tag) + $assetArgs + @("--repo", $Repo, "--verify-tag", "--title", $title, "--notes-file", $bodyPath)
if ($Draft) { $ghArgs += "--draft" }
if ($prerelease) { $ghArgs += "--prerelease" } elseif (-not $Draft) { $ghArgs += "--latest" }

if ($online -and $pushBranch) {
    $script:stage = "push"
    Step "Push $branch to $remote ($upstreamRef)"
    $r = GitRun @("push", $remote, "HEAD:$upstreamRef")   # a fast-forward: git refuses anything else (never forced)
    if ($r.Code -ne 0) { throw "git push $remote HEAD:$upstreamRef failed" }
    Say "pushed $short to $remote $upstreamRef"
}
if ($online) {
    $script:stage = "tag"
    Step "Tag $tag"
    if (-not $reuseTag) {
        $r = GitRun @("tag", "-a", $tag, "-m", "$title ($buildStamp)", $commit)
        if ($r.Code -ne 0) { throw "git tag failed" }
        Say "created $tag on $short"
    }
    if ($tagRemote -ne $commit) {
        $r = GitRun @("push", $remote, "refs/tags/$tag")   # only the tag; never a branch
        if ($r.Code -ne 0) { throw "git push $remote $tag failed (the tag exists locally: re-run to retry)" }
        Say "pushed $tag to $remote"
    }
}
if ($Publish) {
    $script:stage = "release"
    Step "GitHub release ($(if ($Draft) { 'draft' } elseif ($prerelease) { 'public prerelease, never Latest' } else { 'public, marked Latest' }))"
    $r = Run "gh" $ghArgs
    if ($r.Code -ne 0) { throw "gh release create failed" }
    Say $r.Text
}
$onlineResult = ""; $latestResult = ""; $masterResult = ""
if ($Final -and $moveReleaseBranch) {
    # The release branch follows final releases only (RELEASING.md, "Branches"): a fast-forward to the tag's commit.
    $script:stage = "master"
    Step "Release branch: $remote $ReleaseBranch -> $tag (fast-forward)"
    $masterResult = Invoke-ReleaseBranchFastForward
}
if ($Final) {
    $script:stage = "online"
    Step "Latest guard ($Repo)"
    $latestResult = Invoke-LatestGuard $tag
    if (-not $FeedUrl) { $FeedUrl = "https://github.com/$Repo/releases/latest/download/latest.json" }
    Step "Online check ($FeedUrl)"
    Invoke-OnlineCheck $outDir $FeedUrl $QuakeDir
    $onlineResult = "online check passed: $FeedUrl serves $versionText (sizes and SHA-256 of every file, latest.json byte for byte$(if (-not $SkipOnlineInstall -and $QuakeDir) { ', a sandboxed install through it' }))"
}
elseif ($Publish -and $prerelease -and -not $Draft) {
    # A published prerelease: never Latest (checked, nothing edited), then the same online check against its own feed.
    $script:stage = "online"
    Step "Latest ($Repo): $tag must not be it"
    $latestResult = Assert-PrereleaseNotLatest $tag
    if (-not $FeedUrl) { $FeedUrl = $tagFeed }
    Step "Online check ($FeedUrl)"
    Invoke-OnlineCheck $outDir $FeedUrl $QuakeDir
    $onlineResult = "online check passed: $FeedUrl serves $versionText (sizes and SHA-256 of every file, latest.json byte for byte$(if (-not $SkipOnlineInstall -and $QuakeDir) { ', a sandboxed install through it' }))"
}
$script:stage = "done"

# ------------------------------------------------------------------------------------------------------------------
$quoted = ($ghArgs | ForEach-Object { if ($_ -match '[\s"]') { '"' + $_ + '"' } else { $_ } }) -join " "
$lines = @(
    "Quake VR: Unleashed $Version ($tag, commit $short): $(if ($Publish) { if ($Draft) { 'DRAFT release created' } else { 'release published' } } else { 'built, nothing published' })",
    "",
    "Assets ($assetsDir):"
) + @($files | ForEach-Object { "  {0,-45} {1,10}  {2}" -f $_.Name, (Size $_.Length), (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }) + @(
    "",
    "Release notes: $notesPath$(if ($notesText.Contains($draftMarker)) { ' (a DRAFT: edit it and delete its first line before -Publish, or -AutoNotes)' }); the body uploaded: $bodyPath",
    "",
    "What to do:"
)
$n = 0
if ($Local) {
    $lines[0] = "Quake VR: Unleashed $Version LOCAL TEST release (commit $short): built, nothing published; its latest.json points at $($UrlBase -join ', ')"
    $lines += @(
        "  1. Test it: powershell -ExecutionPolicy Bypass -File Misc\release\test_local_release.ps1$(if ($Version -ne $fileVersion) { " -Version $Version" })",
        "       (starts the local server on 127.0.0.1:$LocalPort in its own window, then QuakeVR-Setup.exe --feed http://127.0.0.1:$LocalPort/latest.json",
        "        --sandbox %TEMP%\QuakeVR-test-$Version-<time>: the install, shortcuts and downloads stay in that folder; no Apps & Features entry)",
        "  2. Never upload these assets: latest.json names 127.0.0.1. For the real release, run without -Local."
    )
    if ($hostedTextures) { $lines += "  Note: latest.json's hdtextures is the hosted $((SupportFile 'hdtextures').file) on GitHub (a local server does not have it): ticking HD textures downloads the real $(Size (SupportFile 'hdtextures').size). -Textures <zip> serves a copy locally; -NoTextures leaves it out." }
}
elseif (-not $Publish) {
    $lines += @(
        "  $((++$n)). Publish: run again with -Publish (tags $tag, pushes only the tag to $(if ($remote) { $remote } else { '<the upstream remote>' }), creates a draft release), or by hand:",
        "       git tag -a $tag -m ""$title"" $commit; git push $(if ($remote) { $remote } else { 'origin' }) refs/tags/$tag",
        "       $($quoted -replace '^', 'gh ')"
    )
}
if ($onlineResult) { $lines += "  $((++$n)). Nothing: published and checked. $(if ($masterResult) { "$masterResult; " })$latestResult; $onlineResult." }
if ($Final -and -not $moveReleaseBranch) { $lines += "  $((++$n)). -NoReleaseBranch: move $ReleaseBranch to the release when ready (fast-forward, never forced): $(Get-ReleaseBranchPush)" }
elseif ($Publish -and -not $Final -and -not $prerelease) { $lines += "  $((++$n)). Once it is published as Latest: move $ReleaseBranch to it (fast-forward, never forced; -Final does it): $(Get-ReleaseBranchPush)" }
if (-not $Local -and (-not $Publish -or $Draft)) {
    $lines += "  $((++$n)). Check the draft on https://github.com/$Repo/releases, then publish it (button, or: gh release edit $tag --repo $Repo --draft=false$(if (-not $prerelease) { ' --latest' })). Until it is published (and not a prerelease) https://github.com/$Repo/releases/latest/download/latest.json still serves the previous release."
}
if (-not $Local -and -not $onlineResult) { $lines += @(
    "  $((++$n)). Check: make_release.ps1 -Version $Version -CheckOnline once it is published (or by hand: qvr-setup feed --url $(if ($prerelease) { "$tagFeed   (a prerelease's own feed: releases/latest never serves it)" } else { "https://github.com/$Repo/releases/latest/download/latest.json   (the installer's only feed)" })",
    "             (dotnet run --project Installer\src\QuakeVR.Installer.Cli -- feed --url <...>: the version and the package's size)."
) }
if ($warnings.Count) { $lines += @("", "Warnings:") + @($warnings | ForEach-Object { "  - $_" }) }
$publishTxt = Join-Path $outDir "PUBLISH.txt"
[System.IO.File]::WriteAllText($publishTxt, (($lines -join "`r`n") + "`r`n"), (New-Object System.Text.UTF8Encoding $false))
Step "Done ($publishTxt)"
$lines | ForEach-Object { Say $_ }

if ($RunInstaller) {
    Step "Local test: the server and the installer (test_local_release.ps1)"
    & (Join-Path $PSScriptRoot "test_local_release.ps1") -Release $outDir
}
