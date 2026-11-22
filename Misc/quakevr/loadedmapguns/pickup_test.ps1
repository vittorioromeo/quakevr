param([string]$Name = 'loadedmapguns', [switch]$OnlyExisting)
# Focused mapper-pickup regression. Uses original e1m3 geometry; all replacement entities are synthetic.
# Run from this worktree: powershell -File Misc/quakevr/loadedmapguns/pickup_test.ps1
# One game plays every case (2026-11-22: was a game per case, 295 s; the fixture now holds one mapper weapon of each
# kind, each on its own floor spot of e1m3, an original item's; each case loads the map again with its own settings and
# is set beside its kind's weapon). The fixture is the worktree's quakevr/maps/e1m3.ent while it runs: every game of
# this worktree that loads e1m3 meanwhile sees it (run_test_suite.py runs this test alone, before its lanes).
$ErrorActionPreference = 'Stop'
$tree = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$fixture = Join-Path $tree 'quakevr/maps/e1m3.ent'
if (Test-Path $fixture) { throw "Refusing to overwrite existing $fixture" }
# Each kind's spot: e1m3's own item origins (the nailgun's first, as before), 150+ units apart.
$spots = [ordered]@{
    'weapon_nailgun'         = '-712 -1240 60'
    'weapon_shotgun'         = '184 -1432 -56'
    'weapon_supershotgun'    = '-408 -1800 88'
    'weapon_supernailgun'    = '-352 -1128 48'
    'weapon_grenadelauncher' = '-288 -992 48'
    'weapon_proximity_gun'   = '536 -1464 -64'
    'weapon_rocketlauncher'  = '344 -1288 -152'
    'weapon_lightning'       = '-960 -1616 64'
}
$cases = @(
    @('weapon_shotgun','shells',0,5,0),
    @('weapon_supershotgun','shells',0,2,3),
    @('weapon_nailgun','nails',0,24,6),
    @('weapon_supernailgun','nails',0,30,0),
    @('weapon_grenadelauncher','rockets',0,4,1),
    @('weapon_proximity_gun','rockets',0,4,2),
    @('weapon_rocketlauncher','rockets',0,4,1),
    @('weapon_lightning','cells',0,15,0),
    @('weapon_shotgun','shells',100,8,92),
    @('weapon_supernailgun','nails',200,36,164),
    @('weapon_lightning','cells',100,36,64),
    @('weapon_nailgun','nails',0,0,30,0),
    @('weapon_lightning','cells',0,0,15,0),
    @('weapon_nailgun','nails',0,0,30,2,1),
    @('weapon_nailgun','nails',0,24,6,2,0,'main'),
    @('weapon_nailgun','nails',0,24,6,2,0,'holster')
)
if ($OnlyExisting) { $cases = @($cases | Where-Object { $_.Count -gt 7 }) }
$ents = "{`n`"classname`" `"worldspawn`"`n`"message`" `"Synthetic mapper pickup test`"`n}`n{`n`"classname`" `"info_player_start`"`n`"origin`" `"-712 -1350 56`"`n`"angle`" `"0`"`n}`n"
foreach ($k in $spots.Keys) { $ents += "{`n`"classname`" `"$k`"`n`"origin`" `"$($spots[$k])`"`n}`n" }
$passed = 0
$failed = 0
try {
    Set-Content -Encoding ascii -NoNewline -Value $ents $fixture
    $script = "external_ents 1; vr_fixed_frames 1; vr_item_objects 0; vr_body_interactions 1; vr_weapon_grip_mode 1"
    for ($i = 0; $i -lt $cases.Count; $i++) {
        $case = $cases[$i]
        $kind,$pool,$before = $case[0..2]
        $reload = if ($case.Count -gt 5) { $case[5] } else { 2 }
        $holsters = if ($case.Count -gt 6) { $case[6] } else { 0 }
        $setup = ''
        if ($case.Count -gt 7) {
            $setup = 'impulse 156; wait5; +attack; wait12; -attack; wait30;'
            if ($case[7] -eq 'holster') { $setup += 'vr_test_weaponinst_slot 4; vr_test_weaponinst 1; impulse 120; wait5;' }
        }
        $x, $y, $z = $spots[$kind] -split ' '
        # (The player 24 units below the spot: as before at the nailgun's, 60 below its origin, his box in the weapon's.)
        $script += "; echo PICKUP_BEGIN $i; reset vr_test_weaponinst_slot; reset vr_test_weaponinst; vr_holster_mode $holsters; vr_reload_mode $reload; map e1m3; wait90; god; notarget; noclip; $setup give $($pool.Substring(0,1)) $before; wait5; setpos $x $y $([int]$z - 24); wait60; vr_test_weaponinst 0; impulse 120; wait5; edict 1; wait3; echo PICKUP_END $i"
    }
    $script += "; toggleconsole; quit"
    $output = (& 'C:/Program Files/Git/bin/bash.exe' C:/OHWorkspace/qvr-kit/run.sh $Name -Script $script -ExtraArgs '-nomapindex -noaddons' -Filter 'weaponinst:|ammo_(shells|nails|rockets|cells)|You got|PICKUP_(BEGIN|END)|ERROR|ENGINE|Unknown' -Timeout 600 2>&1 | Out-String)
    $output | Set-Content -Encoding ascii (Join-Path $PSScriptRoot "pickups.log")
    if ($output -cmatch 'ENGINE|ERROR|Unknown|TIMEOUT|exit=[^0]') { throw $output }
    for ($i = 0; $i -lt $cases.Count; $i++) {
        $case = $cases[$i]
        $kind,$pool,$before,$expectedClip,$expectedReserve = $case[0..4]
        $reload = if ($case.Count -gt 5) { $case[5] } else { 2 }
        $holsters = if ($case.Count -gt 6) { $case[6] } else { 0 }
        $expectedHand = if ($case.Count -gt 7 -and $case[7] -ne 'holster') { 'off' } else { 'main' }
        $what = "$kind reload=$reload mode=$holsters before=$before"
        $m = [regex]::Match($output, "(?s)PICKUP_BEGIN $i\b(.*?)PICKUP_END $i\b")
        $seg = $m.Groups[1].Value
        $why = ''
        if (-not $m.Success) { $why = 'its run never ended' }
        elseif ($seg -notmatch "$expectedHand weapon \d+ #\d+ clip\s+(\d+)") { $why = "no pickup: $seg" }
        else {
            $clip = [int]$Matches[1]
            $reserve = 0 # edict omits zero-valued fields
            if ($seg -match "ammo_$pool\s+(\d+)") { $reserve = [int]$Matches[1] }
            if ($clip -ne $expectedClip -or $reserve -ne $expectedReserve) { $why = "clip $clip/$expectedClip reserve $reserve/$expectedReserve" }
            elseif ($case.Count -gt 7) {
                $location = if ($case[7] -eq 'holster') { 'holster 4' } else { 'main' }
                if ($seg -notmatch "$location weapon 6 #\d+ clip\s+22\b") { $why = "existing used magazine changed: $seg" }
            }
        }
        if ($why) { $failed++; Write-Output "mapgun_pickup: FAIL $what : $why" }
        else { $passed++; Write-Output "mapgun_pickup: PASS $what : clip=$clip reserve=$reserve total=$($clip+$reserve)" }
    }
} finally {
    if (Test-Path $fixture) { Move-Item -LiteralPath $fixture -Destination (Join-Path $PSScriptRoot 'e1m3.ent.from-tests') -Force }
}
Write-Output "mapgun_pickup: $passed passed, $failed failed"
if ($failed -gt 0) { exit 1 }
