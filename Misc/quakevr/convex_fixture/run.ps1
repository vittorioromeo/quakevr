# Frozen inputs are private engine triangles; never check them in or distribute them.
param([Parameter(Mandatory=$true)][string]$Corpus, [string]$Baseline='a381daa432260157d4ddfd59d65f0f81298e2337')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Push-Location $repo
try {
    & python "$PSScriptRoot/prepare.py" --baseline $Baseline
    if($LASTEXITCODE -ne 0){throw 'Fixture preparation failed'}
    Copy-Item "$PSScriptRoot/build.cmd" scratch/gunhullopt/build_fixture.cmd -Force
    & cmd /c .\scratch\gunhullopt\build_fixture.cmd *> scratch/gunhullopt/fixture_build.log
    if($LASTEXITCODE -ne 0){
        Get-Content scratch/gunhullopt/fixture_build.log | Select-String 'error:|error LNK|undefined' | Select-Object -First 12
        throw 'Fixture build failed'
    }
    & ./scratch/gunhullopt/fixture.exe $Corpus | Tee-Object scratch/gunhullopt/fixture_results.log
    if($LASTEXITCODE -ne 0){throw 'Convex equivalence failed'}
} finally {Pop-Location}
