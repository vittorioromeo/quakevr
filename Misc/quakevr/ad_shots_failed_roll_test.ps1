param([string]$Name = 'adshots')
# Actual projectile impacts with guaranteed failed rolls must preserve the whole corpse.
$lines = @('developer 0','sv_gravity 0','vr_ragdoll_inherit 0','vr_debug_shots 1','vr_ad_projectiles 2','vr_ragdoll 1','vr_limbs 1','vr_ragdoll_start 0','vr_decap_pop_chance 1','vr_decap_pop_laser 1','vr_decap_pop_roll 1','vr_decap_chance_scale 0.5','vr_limbs_chance_scale 0.5','vr_decap_pop_quad 0','god')
foreach ($case in @(@(51,0),@(57,1))) {
    $lines += @('map vrmelee-recording','wait30','sv_gravity 800',"scratch1 $($case[1])",'scratch2 0','scratch3 14','scratch4 1',"vr_ad_test $($case[0])",'wait270')
}
$lines += @('scratch3 0','scratch4 0','vr_decap_pop_roll -1','vr_decap_chance_scale 1','vr_limbs_chance_scale 1','vr_decap_pop_quad 1','toggleconsole','quit')
$output = & 'C:/Program Files/Git/bin/bash.exe' C:/OHWorkspace/qvr-kit/run.sh $Name -Script ($lines -join ';') -Timeout 90 -ExtraArgs '-nomapindex' -Filter 'adshots-test|adshots:|Host_Error|SV_Error|Fatal|ENGINE ERROR|Unknown command|exit='
$output | Set-Content scratch/failed_roll_probe.log
$records = @($output | Select-String '^adshots-test: mode ')
if ($records.Count -ne 2) { throw 'Expected two failed-roll reports' }
if ($output | Select-String 'FAIL|Host_Error|SV_Error|Fatal|ENGINE ERROR|Unknown command|exit=[1-9]') { throw 'Failed-roll run failed' }
foreach ($record in $records) {
    if ($record.Line -notmatch 'damage 0 head 0 limbs 0 gib 0$') { throw "Failed roll changed whole corpse: $record" }
}
$records.Line
'failed-roll protection passed: two actual corpse impacts'
