<#
.SYNOPSIS
Tests a local release (make_release.ps1 -Local) as a player would get it: serves its assets on 127.0.0.1 and runs its
QuakeVR-Setup.exe against that feed, installing into a sandbox folder. docs/vr-port/RELEASING.md, "Test a release locally".

.DESCRIPTION
  Misc\release\test_local_release.ps1                        # server (its own window) + the installer, sandbox in %TEMP%
  Misc\release\test_local_release.ps1 -Sandbox D:\qvr-test   # the same, that sandbox (again: an update of it)
  Misc\release\test_local_release.ps1 -ServerOnly            # only the server
  Misc\release\test_local_release.ps1 -StopServer            # stop the server on the release's port
  Misc\release\test_local_release.ps1 -DropAfter 5000000     # the server cuts each file's first download (resume test)
  Misc\release\test_local_release.ps1 -Cli                   # no window: qvr-setup installs from the feed, then verifies
  Misc\release\test_local_release.ps1 -Screenshots <dir>     # no window: the installer's off-screen harness installs

The release is out\release\<VERSION>-local (or -Release <folder>). Its latest.json names the server's address
(make_release.ps1 -LocalPort, default 8517): the port is read from it. The installer runs from a copy in
<sandbox>\_installer (a QuakeVR.zip beside it would be installed without the feed), with
"--feed http://127.0.0.1:<port>/latest.json --sandbox <sandbox>": the game goes into <sandbox>\QuakeVR, the shortcuts
into <sandbox>\_shortcuts, the downloads into <sandbox>\_downloads; no Apps & Features entry, the VC++ runtime is only
checked. Your Quake is detected and read as usual (never written). The game started from the sandbox keeps its config
and saves in <sandbox>\QuakeVR\quakevr. To throw the test away, delete the sandbox folder (nothing is outside it).
#>

[CmdletBinding()]
param(
    # The local release's folder (out\release\<version>-local). Default: from -Version (default: the VERSION file).
    [string]$Release = "",
    [string]$Version = "",
    # The install's sandbox. Default: %TEMP%\QuakeVR-test-<version>-<time>.
    [string]$Sandbox = "",
    # Only start the server (and leave it running).
    [switch]$ServerOnly,
    # Stop the server listening on the release's port, and nothing else.
    [switch]$StopServer,
    # Use the server that is already running (it must serve this release's latest.json).
    [switch]$NoServer,
    # The server cuts the first whole download of each file after this many bytes (0: never): the installer resumes it.
    [long]$DropAfter = 0,
    # The server stops by itself after this many minutes.
    [double]$ServerMinutes = 240,
    # The server without a window: its log goes to <release>\logs\server-<time>.log (scripts, tests).
    [switch]$Hidden,
    # Instead of the window: qvr-setup install --feed ... --sandbox ... (--relight --hd), then qvr-setup verify.
    [switch]$Cli,
    # Instead of the window: QuakeVR-Setup.exe --screenshots <dir> (its off-screen harness installs from the feed).
    [string]$Screenshots = "",
    # More arguments for QuakeVR-Setup.exe.
    [string[]]$SetupArgs = @()
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
function Say([string]$text) { Write-Host $text }
function Step([string]$text) { Write-Host ""; Write-Host "==> $text" -ForegroundColor Cyan }

if (-not $Release) {
    if (-not $Version) { $Version = "$(Get-Content -LiteralPath (Join-Path $root 'VERSION') -TotalCount 1)".Trim() }
    $Release = Join-Path $root "out\release\$Version-local"
}
$Release = (Resolve-Path -LiteralPath $Release -ErrorAction SilentlyContinue).Path
if (-not $Release) { throw "no local release: build one with Misc\release\make_release.ps1 -Local" }
$assets = Join-Path $Release "assets"
$feedFile = Join-Path $assets "latest.json"
if (-not (Test-Path -LiteralPath $feedFile)) { throw "no $feedFile (make_release.ps1 -Local makes it)" }
$feed = Get-Content -Raw -LiteralPath $feedFile | ConvertFrom-Json
$first = [Uri]$feed.package.urls[0]
if ($first.Host -ne "127.0.0.1") { throw "$feedFile points at $($first.Host), not a local server: is it a -Local build?" }
$port = $first.Port
$base = "http://127.0.0.1:$port"
$feedUrl = "$base/latest.json"
if (-not $Version) { $Version = ($feed.version -split ' ')[0] }

function Get-Listener() {
    try { Get-NetTCPConnection -LocalAddress 127.0.0.1 -LocalPort $port -State Listen -ErrorAction Stop | Select-Object -First 1 } catch { $null }
}

if ($StopServer) {
    $l = Get-Listener
    if (-not $l) { Say "no server on 127.0.0.1:$port"; return }
    $p = Get-Process -Id $l.OwningProcess
    if ($p.ProcessName -ne "qvr-setup") { throw "127.0.0.1:$port is $($p.ProcessName) (pid $($p.Id)), not qvr-setup serve: not stopped" }
    Stop-Process -Id $p.Id
    Say "stopped the server on 127.0.0.1:$port (qvr-setup, pid $($p.Id))"
    return
}

# qvr-setup: the copy beside the release (this build's), else the repository's Release build.
$cli = Join-Path $Release "tools\qvr-setup\qvr-setup.exe"
if (-not (Test-Path -LiteralPath $cli)) { $cli = Join-Path $root "Installer\src\QuakeVR.Installer.Cli\bin\Release\net9.0-windows\qvr-setup.exe" }
if (-not (Test-Path -LiteralPath $cli)) { throw "no qvr-setup.exe (dotnet build Installer\QuakeVR.Installer.sln -c Release)" }

# ---------------------------------------------------------------------------------------------------------------------
Step "Local server ($base)"
$sha = [System.Security.Cryptography.SHA256]::Create()
$expected = [BitConverter]::ToString($sha.ComputeHash([System.IO.File]::ReadAllBytes($feedFile)))
function Test-Serving() {
    $wc = New-Object System.Net.WebClient
    try { $got = [BitConverter]::ToString($sha.ComputeHash($wc.DownloadData($feedUrl))); if ($got -eq $expected) { "this" } else { "other" } }
    catch { "none" }
    finally { $wc.Dispose() }
}
$state = Test-Serving
$serverPid = $null
if ($state -eq "this") {
    $l = Get-Listener
    if ($l) { $serverPid = $l.OwningProcess }
    Say "already serving this release ($feedUrl$(if ($serverPid) { ", pid $serverPid" }))"
}
elseif ($state -eq "other" -or (Get-Listener)) {
    throw "127.0.0.1:$port already serves something else: stop it (test_local_release.ps1 -StopServer, if it is qvr-setup), or rebuild with make_release.ps1 -Local -LocalPort <another port>"
}
elseif ($NoServer) { throw "-NoServer: nothing serves $feedUrl" }
else {
    $serveArgs = "serve --dir `"$assets`" --port $port --minutes $([string]::Format([System.Globalization.CultureInfo]::InvariantCulture, '{0}', $ServerMinutes))"
    if ($DropAfter -gt 0) { $serveArgs += " --drop-after $DropAfter" }
    if ($Hidden) {
        New-Item -ItemType Directory -Force (Join-Path $Release "logs") | Out-Null
        $serverLog = Join-Path $Release "logs\server-$(Get-Date -Format yyyyMMdd-HHmmss).log"
        $proc = Start-Process -FilePath $cli -ArgumentList $serveArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput $serverLog -RedirectStandardError "$serverLog.err"
    }
    else {
        $proc = Start-Process -FilePath $cli -ArgumentList $serveArgs -PassThru   # its own console window: the requests scroll there
    }
    $serverPid = $proc.Id
    for ($i = 0; $i -lt 50 -and (Test-Serving) -ne "this"; $i++) { Start-Sleep -Milliseconds 200 }
    if ((Test-Serving) -ne "this") { throw "the server did not start (qvr-setup serve, pid $serverPid$(if ($Hidden) { "; log $serverLog" }))" }
    Say "serving $assets"
    Say "  on $base (feed $feedUrl), pid $serverPid, $(if ($Hidden) { "no window, log $serverLog" } else { 'in its own window (the requests scroll there)' })$(if ($DropAfter -gt 0) { "; cuts each file's first download after $DropAfter bytes" })"
    Say "  stops by itself after $ServerMinutes minutes; to stop it now: close its window, or test_local_release.ps1 -StopServer (Stop-Process -Id $serverPid)"
}
if ($ServerOnly) { return }

# ---------------------------------------------------------------------------------------------------------------------
if (-not $Sandbox) { $Sandbox = Join-Path ([System.IO.Path]::GetTempPath()) "QuakeVR-test-$Version-$(Get-Date -Format yyyyMMdd-HHmmss)" }
$installerDir = Join-Path $Sandbox "_installer"
New-Item -ItemType Directory -Force $installerDir | Out-Null
$Sandbox = (Resolve-Path -LiteralPath $Sandbox).Path
$setup = Join-Path $installerDir "QuakeVR-Setup.exe"
Copy-Item -LiteralPath (Join-Path $assets "QuakeVR-Setup.exe") $setup -Force
Step "Sandbox $Sandbox"
Say "  game       $Sandbox\QuakeVR   (its config and saves: $Sandbox\QuakeVR\quakevr)"
Say "  shortcuts  $Sandbox\_shortcuts\Desktop and \Programs (not your desktop or Start menu)"
Say "  downloads  $Sandbox\_downloads"
Say "  no Apps & Features entry; the VC++ runtime is only checked. Throw the test away: delete $Sandbox"

if ($Cli) {
    Step "qvr-setup install --feed $feedUrl --sandbox (no window)"
    & $cli install --feed $feedUrl --sandbox $Sandbox --relight --hd --setup-from $setup --accept-statement
    if ($LASTEXITCODE -ne 0) { throw "qvr-setup install failed (exit $LASTEXITCODE)" }
    & $cli verify --target (Join-Path $Sandbox "QuakeVR")
    if ($LASTEXITCODE -ne 0) { throw "qvr-setup verify failed" }
    return
}
$setupArgs = @("--feed", $feedUrl, "--sandbox", $Sandbox) + $SetupArgs
if ($Screenshots) {
    Step "QuakeVR-Setup.exe --screenshots $Screenshots (off screen: installs from the feed into the sandbox)"
    $setupArgs = @("--screenshots", $Screenshots, "--silent") + $setupArgs
}
$line = ($setupArgs | ForEach-Object { if ($_ -match '[\s"]') { '"' + $_.TrimEnd('\') + '"' } else { $_ } }) -join " "
if ($Screenshots) {
    $p = Start-Process -FilePath $setup -ArgumentList $line -PassThru -Wait
    $report = Join-Path $Screenshots "report.txt"
    if ($p.ExitCode -ne 0) { throw "the harness failed (exit $($p.ExitCode); $report)" }
    Say "pages in $Screenshots"
    return
}
Step "QuakeVR-Setup.exe (the window; a yellow TEST FEED / SANDBOX bar on every page)"
Say "  $setup $line"
Start-Process -FilePath $setup -ArgumentList $line | Out-Null
Say ""
Say "After the install: Play on its last page, or the shortcuts in $Sandbox\_shortcuts\Desktop."
Say "Again (an update of this sandbox): test_local_release.ps1 -Sandbox `"$Sandbox`""
Say "Stop the server: close its window, or test_local_release.ps1 -StopServer"
