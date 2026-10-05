param()
# Install the tested engine and matching QC once the current game has closed.
$repoRoot=(Resolve-Path -LiteralPath "$PSScriptRoot/../..").Path
$releaseDir=Join-Path $repoRoot 'Windows/VisualStudio/Build-ironwail/bin/x64/Release'
$engineSource=Join-Path $repoRoot 'build-cmake/notes-oct5-ready/bin'
$qcSource=Join-Path $repoRoot 'build-cmake/notes-oct5-ready/progs.dat'
$running=Get-Process ironwail -ErrorAction SilentlyContinue
if($running){throw 'Close Quake VR before installing the tested build. No process has been stopped.'}
if(!(Test-Path -LiteralPath "$engineSource/ironwail.exe") -or !(Test-Path -LiteralPath $qcSource)){
 throw 'The staged October 5 build is missing. Build Release and compile QC first.'
}
Get-ChildItem -LiteralPath $engineSource -File | Where-Object { $_.Extension -in '.exe','.dll' } |
 ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $releaseDir }
Copy-Item -LiteralPath $qcSource -Destination (Join-Path $repoRoot 'quakevr/progs.dat')
Write-Output 'Installed the tested engine and matching QuakeC.'
