# Packages Quake VR into dist\QuakeVR (and dist\QuakeVR.zip): the engine from a Release x64
# build, its DLLs, and the quakevr game folder with freshly compiled progs. Copy the contents of
# dist\QuakeVR into a Quake folder (the one with id1) and run QuakeVR.bat.
#
#   Windows\package-quakevr.ps1 [-Build] [-Fteqcc <path to fteqcc64.exe>]
#
# -Build builds ironwail.sln (Release|x64) first; FTEQCC (or -Fteqcc, or fteqcc64 on PATH)
# compiles QC\progs.src.

param(
    [switch]$Build,
    [string]$Fteqcc = $env:FTEQCC
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$bin = Join-Path $root "Windows\VisualStudio\Build-ironwail\bin\x64\Release"
$dist = Join-Path $root "dist\QuakeVR"

if ($Build) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
    & $msbuild (Join-Path $root "Windows\VisualStudio\ironwail.sln") -p:Configuration=Release -p:Platform=x64 -m -v:m -nologo
    if ($LASTEXITCODE -ne 0) { throw "build failed" }
}

if (-not $Fteqcc) { $Fteqcc = "fteqcc64" }
Push-Location (Join-Path $root "QC")
try {
    # fteqcc prints its banner on stderr, which PowerShell would turn into an error.
    $ErrorActionPreference = "Continue"
    & $Fteqcc -O3 -Fautoproto -Olo -Fiffloat -Fifvector -Fvectorlogic -Flo -Fsubscope -Wall -Wextra -Wno-F209 -Wno-F208 2>&1 | Out-Null
    $qcResult = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    if ($qcResult -ne 0) { throw "QC compilation failed" }
} finally {
    Pop-Location
}

if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
New-Item -ItemType Directory -Force $dist | Out-Null

# Engine.
foreach ($f in Get-ChildItem $bin -File) {
    if ($f.Extension -in ".exe", ".dll", ".pak") { Copy-Item $f.FullName $dist }
}

# Game folder, without anything a player creates.
$game = Join-Path $root "quakevr"
$exclude = @("ironwail.cfg", "config.cfg", "autoexec.cfg", "history.txt", "qconsole.log")
Get-ChildItem $game -Recurse -File | Where-Object {
    $rel = $_.FullName.Substring($game.Length + 1)
    -not ($exclude -contains $_.Name) -and $_.Extension -notin ".sav", ".dem" -and -not $rel.StartsWith("screenshots")
} | ForEach-Object {
    $target = Join-Path (Join-Path $dist "quakevr") $_.FullName.Substring($game.Length + 1)
    New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
    Copy-Item $_.FullName $target
}

Set-Content -Encoding ascii (Join-Path $dist "QuakeVR.bat") "@echo off`r`nstart `"`" `"%~dp0ironwail.exe`" -game quakevr %*`r`n"

Set-Content -Encoding ascii (Join-Path $dist "README-QuakeVR.txt") @"
Quake VR (Ironwail + OpenXR)

1. Copy everything here into your Quake folder (the one containing id1). Scourge of Armagon
   (hipnotic) and Dissolution of Eternity (rogue) are used automatically if installed.
2. Start your OpenXR runtime (SteamVR, Oculus, Virtual Desktop...) and put the headset on.
3. Run QuakeVR.bat.

Options > VR Settings has the comfort, body, weapon and display settings. The controller
buttons are ordinary keys (RTRIGGER, LSHOULDER, ABUTTON...) that can be rebound in
Options > Key Setup or with "bind" in the console. "vr_enabled 0" plays on the monitor.
"@

$zip = Join-Path $root "dist\QuakeVR.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $dist "*") -DestinationPath $zip
"Packaged $dist and $zip"
