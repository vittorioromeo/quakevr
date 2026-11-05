# flatsweep_sheet.ps1 -Shots <folder> -Label <label> -Out <file.png> [-Label2 <label>] [-Only "long_stand_hip"]: the contact sheet of a
# vr_flat_sweep run (docs/vr-port/FLATPLAY.md, "The pose sweep"): a row per stance, a column per look (up, level, down; standing
# then crouching). -Label2: a second run's pictures on the row below each of the first's, for a before and after.
param(
    [Parameter(Mandatory = $true)][string]$Shots,
    [Parameter(Mandatory = $true)][string]$Label,
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$Label2 = "",
    [string]$Only = ""
)
Add-Type -AssemblyName System.Drawing
$stances = @("melee_hip", "pistol_hip", "pistol_ads", "long_hip", "long_ads")
$labels = @($Label)
if ($Label2) { $labels += $Label2 }
$looks = @("up", "level", "down")
$w = 320; $h = 180; $gutter = 110
$rowsWanted = @()
foreach ($s in $stances) {
    if ($Only -and $s -notmatch $Only) { continue }
    foreach ($l in $labels) { $rowsWanted += , @($s, $l) }
}
$bmp = New-Object System.Drawing.Bitmap(($gutter + 6 * $w), (20 + $rowsWanted.Count * $h))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::Black)
$font = New-Object System.Drawing.Font("Arial", 9)
$white = [System.Drawing.Brushes]::White
$col = 0
foreach ($c in @("stand", "crouch")) {
    foreach ($l in $looks) {
        $g.DrawString("$c $l", $font, $white, ($gutter + $col * $w + 4), 3); $col++
    }
}
$row = 0
foreach ($r in $rowsWanted) {
    $parts = $r[0] -split "_"
    $y = 20 + $row * $h
    $g.DrawString("$($r[1])`n$($parts[0]) $($parts[1])", $font, $white, 2, ($y + 4))
    $col = 0
    foreach ($c in @("stand", "crouch")) {
        foreach ($l in $looks) {
            $name = "fs_$($r[1])_$($parts[0])_$($c)_$($parts[1])_$l"
            $file = Get-ChildItem $Shots -Filter "$name*.png" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
            # (A suite's run.sh -Clean, flatpose's for one, deletes the folder's pictures, the CSVs staying: no frame is an error, not a black cell.)
            if (-not $file) { throw "no frame $name in $Shots" }
            if ($file) {
                $img = [System.Drawing.Image]::FromFile($file.FullName)
                $g.DrawImage($img, (New-Object System.Drawing.Rectangle(($gutter + $col * $w), $y, ($w - 2), ($h - 2))))
                $img.Dispose()
            }
            $col++
        }
    }
    $row++
}
New-Item -ItemType Directory -Force (Split-Path $Out -Parent) | Out-Null
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
"sheet: $Out"
