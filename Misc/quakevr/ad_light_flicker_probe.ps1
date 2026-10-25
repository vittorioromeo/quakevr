# Hidden mock camera-neighbour sweep at the author's ad_crucial 2026-10-12 11:23:22 note.
# Uses the worktree's current graphics settings. For exact author comparison, copy only desired render cvars
# to a worker-local .cfg and pass its game-relative name via RenderConfig. No map/assets are rewritten.
param([string]$Name='lightaudit',[string]$RenderConfig='')
$kit='C:/OHWorkspace/qvr-kit'
$tree="C:/OHWorkspace/qvr-agents/$Name"
$commands=@('developer 0','vr_mock_fast 0','vr_enabled 1','vr_tips 0','vr_fixed_frames 1','sv_random_seed 7',
    'vr_particle_seed 7','vr_freeze_monsters 1','vr_particle_freeze 1')
if ($RenderConfig) { $commands += "exec $RenderConfig" }
$commands += @('map ad_crucial','wait90','god','notarget','vr_mock_look -24 0',
    'setpos -215 -2287 136 0 -35 0','wait90','vr_shadow_stats 2')
$positions=@('-215 -2287 136','-223 -2287 136','-215 -2287 136','-207 -2287 136',
    '-215 -2287 136','-215 -2295 136','-215 -2287 136','-215 -2279 136',
    '-215 -2287 136','-215 -2287 128','-215 -2287 136','-215 -2287 144','-215 -2287 136')
foreach ($phase in @('authored','flat')) {
    $commands += 'r_flatlightstyles '+$(if ($phase -eq 'authored') { '0' } else { '1' })
    foreach ($position in $positions) {
        $commands += @("setpos $position 0 -35 0","echo LIGHT_PROBE $phase $position",'wait24','vr_eyeshot 1','wait2')
    }
}
$commands += @('vr_shadow_stats 0','echo LIGHT_PROBE_DONE','toggleconsole','quit')
$output=& 'C:/Program Files/Git/bin/bash.exe' "$kit/run.sh" $Name -Games '-game ad -game quakevr' -RealTime -Script ($commands -join ';') -Filter 'LIGHT_PROBE|shadowsel|ENGINE (ERROR|CRASH)|Host_Error|exit=' -Timeout 180 -ExtraArgs '-nomapindex'
$output | Set-Content "$tree/scratch/ad_light_flicker_probe.log"
$text=$output -join [Environment]::NewLine
if ($text -notmatch 'exit=0' -or $text -notmatch 'LIGHT_PROBE_DONE' -or $text -match 'ENGINE (ERROR|CRASH)|Host_Error') {
    throw 'light probe failed; inspect scratch/ad_light_flicker_probe.log'
}
Write-Output "Light probe captured 26 pairs in $tree/quakevr/eyeshots; compare center returns and both eyes."
Write-Output 'Flattened styles are a diagnostic control; the normal run preserves authored flicker.'
