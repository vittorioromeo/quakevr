<#
.SYNOPSIS
    Installs Quake VR's TrenchBroom game configuration ("Quake VR") for the current user.

.DESCRIPTION
    - Copies Misc\trenchbroom\QuakeVR (GameConfig.cfg, quakevr.fgd, Icon.png, the initial maps) into
      %APPDATA%\TrenchBroom\games\QuakeVR.
    - Installs the compile and engine profiles (CompilationProfiles.cfg, GameEngineProfiles.cfg) there, unless the
      folder already has its own (TrenchBroom writes them when you edit profiles): -Force replaces them.
    - Sets "Quake VR"'s game path and its qbsp, vis and light paths in %APPDATA%\TrenchBroom\Preferences.json, after
      backing it up (Preferences.json.<date>.bak). Only the "Games/Quake VR/..." keys are added or changed: the
      other games' settings (the "Quake" game included) are left as they are.

    TrenchBroom must be closed (it rewrites Preferences.json when it quits). Run it again after pulling a change
    to the configuration or the FGD.

.PARAMETER Quake
    The Quake folder (id1, hipnotic, rogue and the quakevr game folder). Default: the Steam Quake that has a
    quakevr folder.

.PARAMETER EricwTools
    The ericw-tools 2.0 folder (qbsp.exe, vis.exe, light.exe). Default: $env:ERICW_TOOLS, else
    C:\OHWorkspace\ericw-tools-2.0.0-alpha11-win64.

.PARAMETER Engine
    ironwail.exe for the engine profiles. Default: the build of the checkout that <Quake>\quakevr belongs to
    (Windows\VisualStudio\Build-ironwail\bin\x64\Release\ironwail.exe), else this checkout's.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Misc\trenchbroom\install.ps1
#>
param(
    [string]$Quake = "",
    [string]$EricwTools = "",
    [string]$Engine = "",
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = (Resolve-Path (Join-Path $here "..\..")).Path
$gameName = "Quake VR"
$userData = Join-Path $env:APPDATA "TrenchBroom"
$dest = Join-Path $userData "games\QuakeVR"
$prefsPath = Join-Path $userData "Preferences.json"

function Fail([string]$msg) { Write-Host "install.ps1: $msg" -ForegroundColor Red; exit 1 }

# TrenchBroom running would overwrite Preferences.json when it quits.
if (Get-Process -Name "TrenchBroom" -ErrorAction SilentlyContinue) {
    Fail "TrenchBroom is running: close it first."
}

# --- The Quake folder --------------------------------------------------------------------------------------------
function Find-SteamQuake {
    $steam = $null
    try { $steam = (Get-ItemProperty "HKCU:\Software\Valve\Steam" -ErrorAction Stop).SteamPath } catch { }
    $libraries = @()
    if ($steam) {
        $vdf = Join-Path $steam "steamapps\libraryfolders.vdf"
        if (Test-Path $vdf) {
            foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) {
                $libraries += $m.Groups[1].Value.Replace("\\", "\")
            }
        }
    }
    if ($steam) { $libraries += $steam.Replace("/", "\") }   # the libraries' own spelling first (Steam's is lower case)
    $libraries += "C:\Program Files (x86)\Steam"
    $found = @()
    foreach ($lib in ($libraries | Select-Object -Unique)) {
        $q = Join-Path $lib "steamapps\common\Quake"
        if (Test-Path (Join-Path $q "id1")) { $found += $q }
    }
    $withVr = $found | Where-Object { Test-Path (Join-Path $_ "quakevr") } | Select-Object -First 1
    if ($withVr) { return $withVr }
    return ($found | Select-Object -First 1)
}

if (-not $Quake) { $Quake = Find-SteamQuake }
if (-not $Quake -or -not (Test-Path (Join-Path $Quake "id1"))) {
    Fail "no Quake folder with id1 found: pass -Quake <folder>."
}
$Quake = (Resolve-Path $Quake).Path
$gameDir = Join-Path $Quake "quakevr"
if (-not (Test-Path $gameDir)) {
    Write-Host "warning: $gameDir does not exist: link it to this checkout's quakevr folder (README.md)." -ForegroundColor Yellow
}

# --- ericw-tools -------------------------------------------------------------------------------------------------
if (-not $EricwTools) { $EricwTools = $env:ERICW_TOOLS }
if (-not $EricwTools) { $EricwTools = "C:\OHWorkspace\ericw-tools-2.0.0-alpha11-win64" }
$tools = @{}
foreach ($t in "qbsp", "vis", "light") {
    $p = Join-Path $EricwTools "$t.exe"
    if (-not (Test-Path $p)) { Fail "no ${p}: pass -EricwTools <ericw-tools 2.0 folder>." }
    $tools[$t] = (Resolve-Path $p).Path
}

# --- The engine --------------------------------------------------------------------------------------------------
$engineRel = "Windows\VisualStudio\Build-ironwail\bin\x64\Release\ironwail.exe"
if (-not $Engine) {
    $item = Get-Item $gameDir -ErrorAction SilentlyContinue
    if ($item -and $item.LinkType -and $item.Target) {
        $target = @($item.Target)[0]
        $candidate = Join-Path (Split-Path -Parent $target) $engineRel
        if (Test-Path $candidate) { $Engine = $candidate }
    }
    if (-not $Engine) { $Engine = Join-Path $repo $engineRel }
}
if (-not (Test-Path $Engine)) {
    Write-Host "warning: no engine at $Engine (build it, or pass -Engine): the engine profiles point there anyway." -ForegroundColor Yellow
}

# --- The game configuration --------------------------------------------------------------------------------------
New-Item -ItemType Directory -Force $dest | Out-Null
foreach ($f in "GameConfig.cfg", "quakevr.fgd", "Icon.png", "initial_valve.map", "initial_standard.map") {
    Copy-Item (Join-Path $here "QuakeVR\$f") (Join-Path $dest $f) -Force
}
$utf8 = New-Object System.Text.UTF8Encoding($false)
foreach ($f in "CompilationProfiles.cfg", "GameEngineProfiles.cfg") {
    $to = Join-Path $dest $f
    if ((Test-Path $to) -and -not $Force) {
        Write-Host "kept your $f (-Force replaces it)"
        continue
    }
    $text = [IO.File]::ReadAllText((Join-Path $here "QuakeVR\$f"))
    $text = $text.Replace("@ENGINE@", $Engine.Replace("\", "\\"))
    [IO.File]::WriteAllText($to, $text, $utf8)
}

# --- Preferences.json: only the "Games/Quake VR/..." keys ---------------------------------------------------------
$prefs = [ordered]@{}
if (Test-Path $prefsPath) {
    $backup = "$prefsPath.$(Get-Date -Format 'yyyyMMdd-HHmmss').bak"
    Copy-Item $prefsPath $backup
    Write-Host "backed up Preferences.json to $backup"
    $raw = [IO.File]::ReadAllText($prefsPath)
    if ($raw.Trim()) {
        $obj = $raw | ConvertFrom-Json
        foreach ($p in $obj.PSObject.Properties) { $prefs[$p.Name] = $p.Value }
    }
}
$before = $prefs.Count
$ours = [ordered]@{
    "Games/$gameName/Path"            = $Quake
    "Games/$gameName/Tool Path/qbsp"  = $tools["qbsp"]
    "Games/$gameName/Tool Path/vis"   = $tools["vis"]
    "Games/$gameName/Tool Path/light" = $tools["light"]
}
foreach ($k in $ours.Keys) { $prefs[$k] = $ours[$k] }
$json = ($prefs | ConvertTo-Json -Depth 20)
$null = $json | ConvertFrom-Json   # it must read back
[IO.File]::WriteAllText($prefsPath, $json, $utf8)

Write-Host ""
Write-Host "Installed TrenchBroom's '$gameName' game into $dest"
Write-Host "  game path  $Quake (mods of new maps: hipnotic, rogue, quakevr)"
Write-Host "  qbsp       $($tools['qbsp'])"
Write-Host "  vis        $($tools['vis'])"
Write-Host "  light      $($tools['light'])"
Write-Host "  engine     $Engine"
Write-Host "  Preferences.json: the 4 Quake VR keys set, $($prefs.Count - 4) other key(s) kept as they were"
