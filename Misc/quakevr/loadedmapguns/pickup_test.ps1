param([string]$Name = 'loadedmapguns', [switch]$OnlyExisting)
# Focused mapper-pickup regression. Uses original e1m3 geometry; all replacement entities are synthetic.
# Run from this worktree: powershell -File Misc/quakevr/loadedmapguns/pickup_test.ps1
$ErrorActionPreference = 'Stop'
$tree = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$fixture = Join-Path $tree 'quakevr/maps/e1m3.ent'
if (Test-Path $fixture) { throw "Refusing to overwrite existing $fixture" }
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
$passed = 0
try {
    if ($OnlyExisting) { $cases = @($cases | Where-Object { $_.Count -gt 7 }) }
    foreach ($case in $cases) {
        $kind,$pool,$before,$expectedClip,$expectedReserve = $case[0..4]
        $reload = if ($case.Count -gt 5) { $case[5] } else { 2 }
        $holsters = if ($case.Count -gt 6) { $case[6] } else { 0 }
        $letter = $pool.Substring(0,1)
        $setup = ''
        $expectedHand = 'main'
        if ($case.Count -gt 7) {
            $setup = 'impulse 156; wait5; +attack; wait12; -attack; wait30;'
            if ($case[7] -eq 'holster') { $setup += 'vr_test_weaponinst_slot 4; vr_test_weaponinst 1; impulse 120; wait5;' }
            else { $expectedHand = 'off' }
        }
        @"
{
"classname" "worldspawn"
"message" "Synthetic mapper pickup test"
}
{
"classname" "info_player_start"
"origin" "-712 -1350 56"
"angle" "0"
}
{
"classname" "$kind"
"origin" "-712 -1240 60"
}
"@ | Set-Content -Encoding ascii $fixture
        $script = "external_ents 1; vr_fixed_frames 1; vr_item_objects 0; vr_body_interactions 1; vr_holster_mode $holsters; vr_weapon_grip_mode 1; vr_reload_mode $reload; map e1m3; wait90; god; notarget; noclip; $setup give $letter $before; wait5; setpos -712 -1240 0; wait60; vr_test_weaponinst 0; impulse 120; wait5; edict 1; wait3; echo PICKUP_END; toggleconsole; quit"
        $output = (& 'C:/Program Files/Git/bin/bash.exe' C:/OHWorkspace/qvr-kit/run.sh $Name -Script $script -ExtraArgs '-nomapindex -noaddons' -Filter 'weaponinst:|ammo_(shells|nails|rockets|cells)|You got|PICKUP_END|ERROR|ENGINE|Unknown' -Timeout 90 2>&1 | Out-String)
        $output | Set-Content -Encoding ascii (Join-Path $PSScriptRoot "$kind-$reload-$holsters-$before.log")
        if ($output -match 'ENGINE|ERROR|Unknown|TIMEOUT|exit=[^0]') { throw $output }
        if ($output -notmatch "$expectedHand weapon \d+ #\d+ clip\s+(\d+)") { throw "No pickup: $output" }
        $clip = [int]$Matches[1]
        $reserve = 0 # edict omits zero-valued fields
        if ($output -match "ammo_$pool\s+(\d+)") { $reserve = [int]$Matches[1] }
        if ($clip -ne $expectedClip -or $reserve -ne $expectedReserve) { throw "$kind reload=$reload mode=$holsters : clip $clip/$expectedClip reserve $reserve/$expectedReserve`n$output" }
        if ($case.Count -gt 7) {
            $location = if ($case[7] -eq 'holster') { 'holster 4' } else { 'main' }
            if ($output -notmatch "$location weapon 6 #\d+ clip\s+22\b") { throw "Existing used magazine changed: $output" }
        }
        $passed++
        Write-Output "PASS $kind reload=$reload mode=$holsters : clip=$clip reserve=$reserve total=$($clip+$reserve)"
    }
} finally {
    if (Test-Path $fixture) { Move-Item -LiteralPath $fixture -Destination (Join-Path $PSScriptRoot 'e1m3.ent.from-tests') -Force }
}
Write-Output "PASS $passed/$($cases.Count) mapper pickups"
