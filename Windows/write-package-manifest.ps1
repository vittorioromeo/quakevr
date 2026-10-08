# Writes manifest.json at the root of a Quake VR package folder (dist\QuakeVR): the version and every file's path
# (relative, forward slashes), size and SHA-256. The installer (Installer/, docs/vr-port/INSTALLER.md) copies exactly
# these files and checks each hash while copying; package-quakevr.ps1 calls this before zipping.
#
#   Windows\write-package-manifest.ps1 -Dir <package folder> [-Version <text>]
#
# -Version defaults to a dev build's own form (quakevr.props): the repository's VERSION with "-dev", then the last
# commit's date and short hash, "-dirty" when tracked files differ from it ("1.0.0-dev (2026-10-07 afd53921)").

param(
    [Parameter(Mandatory = $true)][string]$Dir,
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"
$Dir = (Resolve-Path $Dir).Path
if (-not $Version) {
    $repo = Split-Path -Parent $PSScriptRoot
    $stamp = (& git -C $repo log -1 --date=format:%Y-%m-%d --format="%cd %h" --abbrev=8) -join ""
    if ($LASTEXITCODE -ne 0 -or -not $stamp) { $stamp = "unknown build" }
    elseif (& git -C $repo --no-optional-locks status --porcelain --untracked-files=no) { $stamp += "-dirty" }
    $Version = "$((Get-Content -LiteralPath (Join-Path $repo 'VERSION') -TotalCount 1).Trim())-dev ($stamp)"
}

$files = New-Object System.Collections.Generic.List[object]
$paths = Get-ChildItem -LiteralPath $Dir -Recurse -File |
    ForEach-Object { $_.FullName.Substring($Dir.Length).TrimStart('\', '/').Replace('\', '/') } |
    Where-Object { $_ -ne "manifest.json" -and $_ -ne "install.json" }
# Ordinal order, as the installer's own manifests (stable diffs between releases).
$sorted = [string[]]@($paths)
[Array]::Sort($sorted, [StringComparer]::Ordinal)
foreach ($rel in $sorted) {
    $full = Join-Path $Dir ($rel.Replace('/', '\'))
    $files.Add([ordered]@{
        path   = $rel
        size   = (Get-Item -LiteralPath $full).Length
        sha256 = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash.ToLowerInvariant()
    })
}
$manifest = [ordered]@{ schema = 1; product = "Quake VR"; version = $Version; files = $files.ToArray() }
$json = ConvertTo-Json -InputObject $manifest -Depth 4
[System.IO.File]::WriteAllText((Join-Path $Dir "manifest.json"), $json, (New-Object System.Text.UTF8Encoding $false))
"manifest.json: $($files.Count) files, version $Version"
