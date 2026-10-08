<#
.SYNOPSIS
Makes a Quake VR: Unleashed release: builds the game and the installer (Release), checks them, packages them, writes
latest.json, and (with -Publish) tags the commit and creates the GitHub release. docs/vr-port/RELEASING.md has the steps.

.DESCRIPTION
  Misc\release\make_release.ps1 -DryRun                    # the checks and the plan; builds and writes nothing
  Misc\release\make_release.ps1                            # build, check, package into out\release\<VERSION> (no tag, nothing online)
  Misc\release\make_release.ps1 -Publish                   # ... then tag v<VERSION>, push the tag, create a DRAFT GitHub release
  Misc\release\make_release.ps1 -Publish -NoDraft          # ... a public release straight away
  Misc\release\make_release.ps1 -Version 1.0.0 -BumpVersion   # first commit VERSION = 1.0.0 ("Version 1.0.0"), then as above
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

Works on whatever branch is checked out (its upstream is where the tag goes); never pushes a branch. Everything it
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
    # Release notes (Markdown). Default: the commit subjects since the previous v* tag. The files' SHA-256 table is added.
    [string]$Notes = "",
    # With -Publish: create the release as a draft (the default). -Draft:$false publishes it at once.
    [switch]$Draft = $true,
    # The same as -Draft:$false (which "powershell -File" and make_release.sh cannot pass).
    [switch]$NoDraft,
    # Create the tag, push it (only the tag) and create the GitHub release with the assets.
    [switch]$Publish,
    # Create and push the tag only (no GitHub release).
    [switch]$PushTag,
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
    [switch]$SkipInstallerTests
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$versionFile = Join-Path $root "VERSION"
$fileVersion = if (Test-Path -LiteralPath $versionFile -PathType Leaf) { "$(Get-Content -LiteralPath $versionFile -TotalCount 1)".Trim() } else { "" }
if (-not $fileVersion) { throw "no version in $versionFile (one line: MAJOR.MINOR.PATCH; RELEASING.md, 'Versions')" }
if (-not $Version) { $Version = $fileVersion }
$tag = "v$Version"
if ($NoDraft) { $Draft = $false }
if ($RunInstaller) { $Local = $true }
if ($Local -and ($Publish -or $PushTag)) { throw "-Local is a local test release: never with -Publish or -PushTag" }
if ($Local -and -not $UrlBase) { $UrlBase = @("http://127.0.0.1:$LocalPort/{file}") }
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
    if ($DryRun) { Warn "-BumpVersion: would commit VERSION = $Version (now $fileVersion) before building; the checks below are of the tree as it is" }
    elseif ((GitRun @("--no-optional-locks", "status", "--porcelain", "--untracked-files=no")).Text) {
        throw "-BumpVersion: commit or stash your other changes first (the version commit holds VERSION alone)"
    }
    else {
        [System.IO.File]::WriteAllText($versionFile, "$Version`n", (New-Object System.Text.UTF8Encoding($false)))
        if ((GitRun @("commit", "-q", "-m", "Version $Version", "--", "VERSION")).Code -ne 0) { throw "-BumpVersion: git commit of VERSION failed" }
        Say "committed VERSION = $Version ($((GitRun @("rev-parse", "--short=8", "HEAD")).Text); not pushed)"
    }
}
$prerelease = $Version.Contains("-")
if ($Publish -and $DryRun) { throw "-Publish and -DryRun together: pick one" }
if ($AllowDirty -and $online) { throw "-AllowDirty is for testing the script: never with -Publish or -PushTag" }
if ($Notes -and -not (Test-Path -LiteralPath $Notes -PathType Leaf)) { throw "-Notes: no file $Notes" }

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

# The current branch's upstream (whatever the branch is called): the tag goes to its remote.
$remote = ""
if ($branch -eq "HEAD") { Problem "detached HEAD: check out the branch to release" -OnlineOnly }
else {
    $upstream = (GitRun @("rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}"))
    if ($upstream.Code -ne 0 -or -not $upstream.Text) { Problem "branch $branch has no upstream (git push -u <remote> $branch)" -OnlineOnly }
    else {
        $remote = (GitRun @("config", "branch.$branch.remote")).Text
        if ($Local) { Say "-Local: no git fetch (the pushed check uses the last fetched state)" }
        elseif ((GitRun @("fetch", "--quiet", $remote)).Code -ne 0) { Warn "git fetch $remote failed: the pushed check uses the last fetched state" }
        if ((GitRun @("merge-base", "--is-ancestor", "HEAD", $upstream.Text)).Code -ne 0) { Problem "HEAD ($short) is not on $($upstream.Text): push $branch first" -OnlineOnly }
        else { Say "HEAD is on $($upstream.Text)" }
        $remoteUrl = (GitRun @("remote", "get-url", $remote)).Text
        if ($remoteUrl -notmatch [regex]::Escape($Repo) + '(\.git)?/?$') { Problem "remote $remote is $remoteUrl, not $Repo (-Repo): the tag and the release would be in different places" -OnlineOnly }
    }
}

# The tag: new, or (a re-run after a failed publish) already on this very commit.
$tagLocal = GitRun @("rev-parse", "-q", "--verify", "refs/tags/$tag^{commit}")
$reuseTag = $false
if ($tagLocal.Code -eq 0) {
    if ($tagLocal.Text -eq $commit) { $reuseTag = $true; Warn "tag $tag already exists here on ${short}: it is reused" }
    else { Problem "tag $tag already exists on another commit ($($tagLocal.Text.Substring(0, 8)))" }
}
$tagRemote = ""
if ($remote -and -not $Local) {
    $ls = GitRun @("ls-remote", "--tags", $remote, "refs/tags/$tag", "refs/tags/$tag^{}")
    if ($ls.Code -eq 0 -and $ls.Text) {
        $tagRemote = @(@($ls.Out | Where-Object { $_ -match '\^\{\}$' }) + @($ls.Out))[0] -replace '\s.*$', ''   # (the peeled commit first)
        if ($tagRemote -ne $commit) { Problem "tag $tag already exists on $remote on another commit" } else { Warn "tag $tag is already on $remote (this commit)" }
    }
}

# Tools.
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { Problem "gh (GitHub CLI) not found" -OnlineOnly }
elseif ($DryRun -or $Local) { Say "gh found (not called: $(if ($Local) { '-Local' } else { '-DryRun' }))" }
else {
    if ((Run "gh" @("auth", "status")).Code -ne 0) { Problem "gh is not logged in (gh auth login)" -OnlineOnly }
    elseif ((Run "gh" @("release", "view", $tag, "--repo", $Repo, "--json", "tagName")).Code -eq 0) { Problem "GitHub release $tag already exists in $Repo (edit or delete it on GitHub)" -OnlineOnly }
    else { Say "gh is logged in; no release $tag in $Repo yet" }
}
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
# The Visual Studio that has the engine's toolset (ClangCL: "C++ Clang tools for Windows"; quakevr.toolset.props).
if (-not $MSBuild -and (Test-Path $vswhere)) {
    $MSBuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset `
        -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
}
if (-not $MSBuild -or -not (Test-Path -LiteralPath $MSBuild)) { Problem "MSBuild with the ClangCL toolset not found (Visual Studio 2022 with C++ and 'C++ Clang tools for Windows'; or -MSBuild)" } else { Say "MSBuild: $MSBuild" }
$dotnet = Run "dotnet" @("--version")   # (in the repository root; the installer's global.json is checked below)
$installerDir = Join-Path $root "Installer"
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
$quakeOk = $QuakeDir -and (Test-Path (Join-Path $QuakeDir "id1\pak0.pak"))
if ($SkipSmoke) { Warn "-SkipSmoke: no launch of the packaged game" }
elseif (-not $quakeOk) { Warn "no Quake folder with id1\pak0.pak (-QuakeDir or QVR_QUAKE_DIR): the packaged game's smoke launch is skipped" }

$outBase = Join-Path $root "out\release"
$outDir = Join-Path $outBase "$Version$(if ($Local) { '-local' })"

if ($problems.Count) { throw "$($problems.Count) problem(s) above: nothing was built or published" }

if ($DryRun) {
    Step "Plan (-DryRun: nothing is built, tagged or published)"
    Say "version text   $versionText"
    Say "output         $outDir$(if (Test-Path $outDir) { ' (exists: it would be moved aside)' })"
    Say "engine         MSBuild ironwail.sln Release|x64 $(if ($Rebuild) { '(rebuild) ' })/p:QvrReleaseVersion=$Version"
    Say "installer      dotnet publish QuakeVR.Installer -c Release -r win-x64 single-file, /p:Version=$Version"
    Say "checks         statics, QC precedence, FGD, installer self-tests, latest.json, packaged installer (install harness), smoke launch$(if (-not $quakeOk -or $SkipSmoke) { ' (skipped)' })"
    Say "assets         QuakeVR.zip, QuakeVR-Setup.exe, latest.json, SHA256SUMS.txt$(if ($Textures) { ', ' + (Split-Path -Leaf $Textures) })$(if ($EricwSource) { ', ' + (Split-Path -Leaf $EricwSource) })$(foreach ($a in $Assets) { ', ' + (Split-Path -Leaf $a) })"
    Say "hdtextures     $(if ($hostedTextures) { "hosted: $(SupportUrl 'hdtextures') (not uploaded; checked online when building, not with -DryRun)" } elseif ($Textures) { 'uploaded with this release (-Textures)' } else { 'none (-NoTextures)' })"
    if ($shipsEricw) { Say "ericw source   $(if ($EricwSource) { 'uploaded with this release (-EricwSource)' } else { "linked from the notes: $(SupportUrl 'ericw_source')" })" }
    Say "tag            $tag on $short$(if ($online) { ", pushed to $remote" } else { ' (only with -Publish or -PushTag)' })"
    if ($Local) { Say "local test     latest.json -> $($UrlBase -join ', ')$(if ($RunInstaller) { '; then the server and QuakeVR-Setup.exe in a sandbox' })" }
    Say "release        $(if ($Publish) { "gh release create $tag --repo $Repo$(if ($Draft) { ' --draft' })$(if ($prerelease) { ' --prerelease' })" } else { 'none (-Publish creates it)' })"
    $list = & (Join-Path $root "Windows\package-quakevr.ps1") -DryRun
    Say "package        $(@($list).Count) files (Windows\package-quakevr.ps1 -DryRun lists them)"
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

$notesPath = Join-Path $outDir "release-notes.md"
if ($Notes) { $body = Get-Content -Raw $Notes }
else {
    $prev = GitRun @("describe", "--tags", "--abbrev=0", "--match", "v[0-9]*", "HEAD")
    $logArgs = @("log", "--no-merges", "--format=%s")
    if ($prev.Code -eq 0 -and $prev.Text -and $prev.Text -ne $tag) { $logArgs += "$($prev.Text)..HEAD"; $since = "since $($prev.Text)" }
    else { $logArgs += @("-n", "60"); $since = "(the last 60 commits; no earlier v* tag)" }
    $subjects = @((GitRun $logArgs).Out | ForEach-Object { if ($_.Length -gt 280) { $_.Substring(0, 277) + "..." } else { $_ } })
    $shown = @($subjects | Select-Object -First 150)
    $body = "## Changes $since`n`n" + (($shown | ForEach-Object { "- $_" }) -join "`n")
    if ($subjects.Count -gt $shown.Count) { $body += "`n- ...and $($subjects.Count - $shown.Count) more" }
}
$table = "## Files`n`nBuild: Quake VR $versionText, commit $commit.`n`n| File | Size | SHA-256 |`n|---|---|---|`n" +
    (($files | Where-Object { $_.Name -ne "SHA256SUMS.txt" } | ForEach-Object { "| ``$($_.Name)`` | $(Size $_.Length) | ``$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant())`` |" }) -join "`n") +
    "`n`n" + $(if ($hostedTextures) { "HD texture pack (the installer's optional HD textures; not attached here): [``$((SupportFile 'hdtextures').file)``]($(SupportUrl 'hdtextures')), $(Size (SupportFile 'hdtextures').size), SHA-256 ``$((SupportFile 'hdtextures').sha256)``.`n`n" } else { "" }) +
    $(if ($shipsEricw) { "Source of ericw-tools' light.exe (GPL-3; QuakeVR.zip ships light.exe): $(if ($EricwSource) { "https://github.com/$Repo/releases/download/$tag/$(Split-Path -Leaf $EricwSource)" } else { SupportUrl 'ericw_source' })`n`n" } else { "" }) +
    "The installer and the game are not code-signed: Windows SmartScreen may say ""Windows protected your PC"" (More info > Run anyway). Check a download with ``Get-FileHash <file>`` against the table."
[System.IO.File]::WriteAllText($notesPath, ($body.TrimEnd() + "`n`n" + $table + "`n"), (New-Object System.Text.UTF8Encoding $false))
Say $notesPath

# ------------------------------------------------------------------------------------------------------------------
$assetArgs = @($files | ForEach-Object { $_.FullName })
$title = "Quake VR: Unleashed $Version"
$ghArgs = @("release", "create", $tag) + $assetArgs + @("--repo", $Repo, "--verify-tag", "--title", $title, "--notes-file", $notesPath)
if ($Draft) { $ghArgs += "--draft" }
if ($prerelease) { $ghArgs += "--prerelease" } elseif (-not $Draft) { $ghArgs += "--latest" }

if ($online) {
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
    Step "GitHub release ($(if ($Draft) { 'draft' } else { 'public' }))"
    $r = Run "gh" $ghArgs
    if ($r.Code -ne 0) { throw "gh release create failed (the tag is pushed: fix the cause and re-run with the same -Version)" }
    Say $r.Text
}

# ------------------------------------------------------------------------------------------------------------------
$quoted = ($ghArgs | ForEach-Object { if ($_ -match '[\s"]') { '"' + $_ + '"' } else { $_ } }) -join " "
$siteFeed = "https://vittorioromeo.com/quakevr/latest.json"
$lines = @(
    "Quake VR: Unleashed $Version ($tag, commit $short): $(if ($Publish) { if ($Draft) { 'DRAFT release created' } else { 'release published' } } else { 'built, nothing published' })",
    "",
    "Assets ($assetsDir):"
) + @($files | ForEach-Object { "  {0,-45} {1,10}  {2}" -f $_.Name, (Size $_.Length), (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }) + @(
    "",
    "Release notes: $notesPath (edit before publishing if you like)",
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
if (-not $Local -and (-not $Publish -or $Draft)) {
    $lines += "  $((++$n)). Check the draft on https://github.com/$Repo/releases, then publish it (button, or: gh release edit $tag --repo $Repo --draft=false$(if (-not $prerelease) { ' --latest' })). Until it is published (and not a prerelease) https://github.com/$Repo/releases/latest/download/latest.json still serves the previous release."
}
if (-not $Local) { $lines += @(
    "  $((++$n)). Upload $assetsDir\latest.json to $siteFeed (the installer's second feed; same file as the release's asset).",
    "  $((++$n)). Check: qvr-setup feed --url https://github.com/$Repo/releases/latest/download/latest.json   and   --url $siteFeed",
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
