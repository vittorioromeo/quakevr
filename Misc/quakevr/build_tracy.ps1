# Dedicated optimized build; normal Release outputs and intermediate files are preserved.
param([ValidateRange(0, 32)][int]$CallstackDepth = 0, [switch]$SystemTracing, [string]$MSBuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe')
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$log = Join-Path $repo 'scratch/build_tracy.log'
& $MSBuild (Join-Path $repo 'Windows/VisualStudio/ironwail.sln') '-p:Configuration=Release-Tracy' '-p:Platform=x64' "-p:QvrTracyCallstackDepth=$CallstackDepth" "-p:QvrTracySystemTracing=$($SystemTracing.IsPresent.ToString().ToLowerInvariant())" '-m' '-v:q' '-nologo' *> $log
$buildExit = $LASTEXITCODE
Get-Content $log | Select-String ' error |warning C' | Select-Object -First 20
if ($buildExit -ne 0) { throw "Tracy build failed ($buildExit), see $log" }
Write-Output 'built Release-Tracy: Windows/VisualStudio/Build-ironwail/bin/x64/Release-Tracy/ironwail.exe'
