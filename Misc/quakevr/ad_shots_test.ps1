param([string]$Name = 'adshots', [switch]$Full, [switch]$VolleysOnly, [switch]$CheckLog)
if ($Full -and $VolleysOnly) { throw 'Choose Full or VolleysOnly' }
# Headless actual-flight regression. Damage and pops come from moving projectile touches.
$lines = @('developer 0','vr_ragdoll_inherit 0','sv_gravity 0','vr_debug_shots 1','vr_ad_projectiles 2','vr_ragdoll 1','vr_limbs 1','vr_decap_pop_chance 0','vr_decap_pop_laser 1','vr_ragdoll_start 0','god')
$sizes = if ($VolleysOnly) { @() } elseif ($Full) { @(0,1) } else { @(1) }
foreach ($small in $sizes) {
    $modes = if ($small) { 50..53 } else { 50..57 }
    foreach ($mode in $modes) {
        foreach ($region in 0..2) {
            $lines += @('map vrmelee-recording','wait30','sv_gravity 800',"scratch1 $region",'scratch2 0',"scratch4 $small",'scratch3 0',"vr_ad_test $mode",'wait270')
        }
    }
}
foreach ($count in @(6,14)) {
    foreach ($mode in 50..53) {
        foreach ($region in 0..2) {
            $lines += @('map vrmelee-recording','wait30','sv_gravity 800',"scratch1 $region",'scratch2 0','scratch4 1',"scratch3 $count", "vr_ad_test $mode",'wait270')
        }
    }
}
if ($Full) {
    foreach ($mode in @(50,52,54,56)) {
        foreach ($region in 0..2) {
            $lines += @('map vrmelee-recording','wait30','sv_gravity 800',"scratch1 $region",'scratch2 1','scratch4 0','scratch3 0',"vr_ad_test $mode",'wait60')
        }
    }
    $lines += @('map vrmelee-recording','wait30','sv_gravity 800','scratch4 0','scratch3 0','vr_ad_pellet_speed 1','vr_ad_enemy_pellet_speed 1','vr_ad_test 58','wait5','vr_ad_pellet_speed 2','vr_ad_test 58','wait5','vr_ad_pellet_speed 1','vr_ad_enemy_pellet_speed 2','vr_ad_test 58','wait5','vr_ad_test 59','wait5','vr_ad_test 60','wait60')
}
$lines += @('map e1m1','wait120','scratch3 0','scratch4 0','vr_campaign_native ad','wait60','map ad_crucial','wait120','vr_ad_info','toggleconsole','quit')
$output = if ($CheckLog) { Get-Content 'scratch/ad_shots_regression.log' } else {
    & 'C:/Program Files/Git/bin/bash.exe' C:/OHWorkspace/qvr-kit/run.sh $Name -Script ($lines -join ';') -Timeout 300 -ExtraArgs '-nomapindex' -Filter 'adshots-test|adshots:|Host_Error|SV_Error|Fatal|ENGINE ERROR|Unknown command|exit=|AD rules on this map'
}
$output | Set-Content "scratch/ad_shots_regression.log"
$records = @($output | Select-String '^adshots-test: mode ')
$expected = if ($VolleysOnly) { 24 } elseif ($Full) { 72 } else { 36 }
if ($records.Count -ne $expected) { throw "Expected $expected flight reports, found $($records.Count)" }
if ($output | Select-String 'FAIL|Host_Error|SV_Error|Fatal|ENGINE ERROR|Unknown command|exit=[1-9]') { throw 'Flight run failed; inspect scratch/ad_shots_regression.log' }
foreach ($count in @(6,14)) {
    $volleys = @($output | Select-String "^adshots-test: pellet movetype 5 shot [0-9]+ pellets $count$")
    if ($volleys.Count -ne 12) { throw "Expected 12 real $count-pellet launches, found $($volleys.Count)" }
}
foreach ($record in $records) {
    $m = [regex]::Match($record.Line, 'mode (\d+) region (\d+) health (-?[\d.]+) damage (-?[\d.]+) head (\d+) limbs (\d+) gib (\d+)')
    if (!$m.Success) { throw "Unrecognized report: $record" }
    $region = [int]$m.Groups[2].Value
    $health = [double]$m.Groups[3].Value
    $head = [int]$m.Groups[5].Value
    $limbs = [int]$m.Groups[6].Value
    if ($region -ne 2 -and [int]$m.Groups[7].Value) { throw "Selective region gibbed whole corpse: $record" }
    if ($health -le 0 -and (($region -eq 0 -and !$head) -or ($region -eq 1 -and !($limbs -band 32)))) { throw "Missing selective pop: $record" }
    if ($region -eq 2 -and ($head -or $limbs)) { throw "Torso popped a remote part: $record" }
}
"adshots regression passed: $($records.Count) actual flight reports; see scratch/ad_shots_regression.log"

