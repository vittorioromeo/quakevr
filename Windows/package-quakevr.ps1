# Packages Quake VR into dist\QuakeVR (and dist\QuakeVR.zip): the engine from a Release x64
# build, its DLLs, and the quakevr game folder with freshly compiled progs (and the relighting
# scripts in quakevr\tools; never the relit id maps). Copy the contents of
# dist\QuakeVR into a Quake folder (the one with id1) and run QuakeVR.bat.
#
#   Windows\package-quakevr.ps1 [-Build] [-Fteqcc <path to fteqcc64.exe>] [-DryRun] [-Root <checkout>]
#
# -Build builds ironwail.sln (Release|x64) first; FTEQCC (or -Fteqcc, or fteqcc64 on PATH)
# compiles QC\progs.src. -DryRun builds, compiles, copies and writes nothing: it prints the
# package's file list, one path per line. -Root packages (or lists) another checkout.
#
# The game folder is an allowlist: the files git tracks under quakevr\ (less the development
# data in $devOnly) plus the build outputs in $generated. Nothing untracked ever ships: custom
# maps, mod folders, saves, configs, screenshots, notes, caches and test dumps stay out however
# they got into the folder.

param(
    [switch]$Build,
    [string]$Fteqcc = $env:FTEQCC,
    [switch]$DryRun,
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
$root = if ($Root) { (Resolve-Path $Root).Path } else { Split-Path -Parent $PSScriptRoot }
$bin = Join-Path $root "Windows\VisualStudio\Build-ironwail\bin\x64\Release"
$dist = Join-Path $root "dist\QuakeVR"

if ($Build -and -not $DryRun) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
    & $msbuild (Join-Path $root "Windows\VisualStudio\ironwail.sln") -p:Configuration=Release -p:Platform=x64 -m -v:m -nologo
    if ($LASTEXITCODE -ne 0) { throw "build failed" }
}

if (-not $Fteqcc) { $Fteqcc = "fteqcc64" }
if (-not $DryRun) {
    Push-Location (Join-Path $root "QC")
    try {
        # fteqcc prints its banner on stderr, which PowerShell would turn into an error.
        $ErrorActionPreference = "Continue"
        & $Fteqcc -O3 -Fautoproto -Olo -Fiffloat -Fifvector -Fvectorlogic -Flo -Fsubscope -Wall -Wextra -Wno-F209 -Wno-F208 2>&1 | Out-Null
        $qcResult = $LASTEXITCODE
        $ErrorActionPreference = "Stop"
        if ($qcResult -ne 0) { throw "QC compilation failed" }
        # TrenchBroom's entity definitions must cover every spawn function (docs/vr-port/MAPPING.md).
        if (Get-Command python -ErrorAction SilentlyContinue) {
            & python (Join-Path $root "Misc\trenchbroom\fgdgen.py") --check
            if ($LASTEXITCODE -ne 0) { throw "Misc\trenchbroom\quakevr.fgd is out of date with the QuakeC" }
        }
    } finally {
        Pop-Location
    }
}

# The game folder's files, relative to quakevr\ with forward slashes: what git tracks, less the
# development data, plus the build's outputs.
$game = Join-Path $root "quakevr"
# Tracked but for development only: git's own file, and the motion recorder's verdicts for the
# author's archived melee takes (docs/vr-port/MOTIONS.md).
$devOnly = @(".gitignore", "motions/*")
# Made by the build, not tracked: the compiled QuakeC.
$generated = @("progs.dat")
$tracked = & git -C $root -c core.quotepath=off ls-files -z -- quakevr
if ($LASTEXITCODE -ne 0) { throw "git ls-files failed in $root (packaging needs a git checkout)" }
$gameFiles = New-Object System.Collections.Generic.List[string]
foreach ($path in ($tracked -split "`0")) {
    if (-not $path) { continue }
    $rel = $path.Substring("quakevr/".Length)
    if ($devOnly | Where-Object { $rel -like $_ }) { continue }
    if (-not (Test-Path -LiteralPath (Join-Path $game $rel) -PathType Leaf)) { throw "tracked file missing from the working tree: quakevr/$rel" }
    $gameFiles.Add($rel)
}
foreach ($rel in $generated) {
    if (-not $DryRun -and -not (Test-Path -LiteralPath (Join-Path $game $rel) -PathType Leaf)) { throw "build output missing: quakevr/$rel" }
    $gameFiles.Add($rel)
}
$modified = & git -C $root --no-optional-locks status --porcelain --untracked-files=no -- quakevr
if ($modified) { Write-Warning "tracked game files differ from the commit; their working-tree content ships:`n$($modified -join "`n")" }

# Engine: the build's executable, its DLLs and ironwail.pak, and the exe's full .pdb: a player's crash report
# (qvr_crash.txt) names the functions on the stack only with it beside the exe, and qvr_crash.dmp opens in a
# debugger with it. The build names itself (VR_BuildVersion: the console, the VR Settings page, the report).
$engineFiles = @(Get-ChildItem $bin -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in ".exe", ".dll", ".pak" -or $_.Name -eq "ironwail.pdb" })
$toolFiles = @("relight_maps.py", "vis_maps.py", "quakepak.py", "relight_textures.cfg")

if ($DryRun) {
    $engineFiles | ForEach-Object { $_.Name }
    $gameFiles | ForEach-Object { "quakevr/$_" }
    $toolFiles | ForEach-Object { "quakevr/tools/$_" }
    "QuakeVR.bat"
    "README-QuakeVR.txt"
    return
}

if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
New-Item -ItemType Directory -Force $dist | Out-Null

# Engine.
if (-not ($engineFiles | Where-Object { $_.Name -eq "ironwail.exe" })) { throw "no Release build in $bin (use -Build)" }
if (-not ($engineFiles | Where-Object { $_.Name -eq "ironwail.pdb" })) { throw "no ironwail.pdb in $bin: crash reports would have no function names" }
foreach ($f in $engineFiles) { Copy-Item $f.FullName $dist }

# Game folder: the allowlist above.
foreach ($rel in $gameFiles) {
    $target = Join-Path (Join-Path $dist "quakevr") $rel
    New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
    Copy-Item -LiteralPath (Join-Path $game $rel) $target
}

# The relighting scripts (docs/RELIGHTING.md), so players can relight their own maps without the
# repository. From quakevr\tools, relight_maps.py's default output is <Quake>\quakevr\relit.
$tools = Join-Path $dist "quakevr\tools"
New-Item -ItemType Directory -Force $tools | Out-Null
foreach ($f in $toolFiles) {
    Copy-Item (Join-Path $root "Misc\quakevr\$f") $tools
}

Set-Content -Encoding ascii (Join-Path $dist "QuakeVR.bat") "@echo off`r`nstart `"`" `"%~dp0ironwail.exe`" -game quakevr %*`r`n"

Set-Content -Encoding ascii (Join-Path $dist "README-QuakeVR.txt") @"
Quake VR (Ironwail + OpenXR)

1. Copy everything here into your Quake folder (the one containing id1). Scourge of Armagon
   (hipnotic) and Dissolution of Eternity (rogue) are used automatically if installed.
2. Start your OpenXR runtime (SteamVR, Oculus, Virtual Desktop...) and put the headset on.
3. Run QuakeVR.bat.

If the game crashes, it writes qvr_crash.txt and qvr_crash.dmp in the folder it was started
from: please attach both to the bug report, with the build named at the bottom of VR Settings
(ironwail.pdb, beside the exe, is what lets the report name the functions; keep it there).

Options > VR Settings has the comfort, body, weapon and display settings. The controller
buttons are ordinary keys (RTRIGGER, LSHOULDER, ABUTTON...) that can be rebound in
Options > Key Setup or with "bind" in the console. "vr_enabled 0" plays on the monitor.

Optional: relit maps and see-through water. id Software's maps can't be distributed, so
you relight your own copy once, with quakevr\tools\relight_maps.py (Python 3) and
ericw-tools. The steps are in docs/RELIGHTING.md in the Quake VR repository.
"@

$zip = Join-Path $root "dist\QuakeVR.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $dist "*") -DestinationPath $zip
"Packaged $dist and $zip"
