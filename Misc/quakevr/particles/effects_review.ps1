param([ValidateSet('debris','fire','sources')][string]$Mode='debris',
    [Parameter(Mandatory=$true)][string]$Base,
    [Parameter(Mandatory=$true)][string]$Exe)
# Use a disposable game base: this replaces its quakevr/autoexec.cfg.
$reviewBase=(Resolve-Path -LiteralPath $Base).Path
$exe=(Resolve-Path -LiteralPath $Exe).Path
$steps= [System.Collections.Generic.List[string]]::new()
function Add-Cmd([string]$cmd) { $steps.Add($cmd) }
function Wait-Frames([int]$count) { for($i=0;$i -lt $count;$i++) { $steps.Add('wait') } }
foreach($cmd in @('vr_backend mock','vr_enabled 1','vr_fixed_frames 1','vr_mock_fast 1','sv_autosave 0','vr_retro 0','vr_particle_seed 7','vr_particles 1','vr_fire_particles 0','developer 1')) { Add-Cmd $cmd }
Wait-Frames 80
if($Mode -eq 'fire') { Add-Cmd 'map start' } elseif($Mode -eq 'sources') { Add-Cmd 'map e1m2' } else { Add-Cmd 'map e1m1' }
Wait-Frames 100
Add-Cmd 'notarget'
Add-Cmd 'vr_mock_look 0 0'
if($Mode -eq 'debris') {
    foreach($cmd in @('vr_explosion_debris_test clear','vr_explosion_debris_count 8','vr_explosion_debris_max 10','vr_explosion_debris_speed_min 9','vr_explosion_debris_speed_max 3','vr_explosion_debris_life_min 3','vr_explosion_debris_life_max 2','vr_explosion_particles 3','vr_particle_mult 2')) { Add-Cmd $cmd }
    foreach($kind in @('normal','colored','tar','preset')) {
        Add-Cmd "echo CASE_$kind"
        Add-Cmd "vr_explosion_debris_test $kind"
        Wait-Frames 3
        Add-Cmd 'vr_explosion_debris_stats'
    }
    Wait-Frames 45
    Add-Cmd 'screenshot'
    Add-Cmd 'vr_explosion_debris_stats'
    Wait-Frames 260
    Add-Cmd 'echo CASE_expired'
    Add-Cmd 'vr_explosion_debris_stats'
    Add-Cmd 'vr_explosion_debris_test clear'
    Add-Cmd 'vr_particles 0'
    Add-Cmd 'echo CASE_particles_off'
    Add-Cmd 'vr_explosion_debris_test normal'
    Wait-Frames 3
    Add-Cmd 'vr_explosion_debris_stats'
    Add-Cmd 'vr_explosion_debris_max 4'
    Wait-Frames 3
    Add-Cmd 'echo CASE_reduce_cap'
    Add-Cmd 'vr_explosion_debris_stats'
    Add-Cmd 'vr_explosion_debris 0'
    Wait-Frames 3
    Add-Cmd 'echo CASE_disabled'
    Add-Cmd 'vr_explosion_debris_stats'
    Add-Cmd 'vr_explosion_debris 1'
    Add-Cmd 'vr_explosion_debris_max 0'
    Add-Cmd 'vr_explosion_debris_test normal'
    Wait-Frames 3
    Add-Cmd 'echo CASE_zero_cap'
    Add-Cmd 'vr_explosion_debris_stats'
    Add-Cmd 'map e1m1'
    Wait-Frames 80
    Add-Cmd 'echo CASE_map_reset'
    Add-Cmd 'vr_explosion_debris_stats'
} elseif($Mode -eq 'fire') {
    Add-Cmd 'vr_explosion_debris 0'
    Add-Cmd 'vr_fire_particles 1'
    Add-Cmd 'vr_walltorch_inv_size 0.15'
    Wait-Frames 90
    Add-Cmd 'echo CASE_fire_on'
    Add-Cmd 'vr_fire_particles_stats'
    Add-Cmd 'screenshot'
    Add-Cmd 'vr_fire_particles 0'
    Wait-Frames 90
    Add-Cmd 'echo CASE_fire_off'
    Add-Cmd 'vr_fire_particles_stats'
    Add-Cmd 'vr_fire_particles 1'
    Add-Cmd 'vr_fire_particles_frequency 0'
    Wait-Frames 90
    Add-Cmd 'echo CASE_zero_frequency'
    Add-Cmd 'vr_fire_particles_stats'
    Add-Cmd 'vr_fire_particles_frequency 10'
    Wait-Frames 90
    Add-Cmd 'echo CASE_fire_resumed'
    Add-Cmd 'vr_fire_particles_stats'
    Add-Cmd 'vr_walltorch_tilt_test'
} else {
    foreach($cmd in @('vr_fire_particles 1','vr_burn_self 0','vr_burn_drop 0','vr_walltorch_inv_size 0.15','vr_walltorch_pull 0','vr_walltorch_debug 1','setpos 1714 -190 312 0 243 0','noclip','vr_mock_hand off -0.35 1.1 -0.2 0 0 0','vr_mock_hand main 0.0 0.8 -0.6 70 0 0','+grabright','vr_mock_button main grip 1')) { Add-Cmd $cmd }
    Wait-Frames 60
    Add-Cmd 'vr_mock_hand main 0.0 0.8 -0.5 70 0 0'
    Wait-Frames 60
    Add-Cmd 'echo CASE_held_torch'
    Add-Cmd 'vr_fire_particles_stats'
    Add-Cmd 'screenshot'
    Add-Cmd 'vr_walltorch_debug 2'
    foreach($gain in @(0,1)) {
        Add-Cmd "vr_walltorch_hand_motion $gain"
        Add-Cmd "echo CASE_hand_motion_$gain"
        for($j=0;$j -lt 60;$j++) {
            $x=(0.45*[Math]::Sin($j*0.3)).ToString('F4',[Globalization.CultureInfo]::InvariantCulture)
            Add-Cmd "vr_mock_hand main $x 0.8 -0.5 70 0 0"
            Wait-Frames 1
        }
    }
    Add-Cmd 'vr_walltorch_debug 1'
    foreach($angle in @(0,90,180)) {
        $handPitch=$angle+70
        Add-Cmd "vr_mock_hand main 0.2 1.2 -0.4 $handPitch 0 0"
        Wait-Frames 60
        Add-Cmd "echo CASE_hand_$angle"
        Add-Cmd 'screenshot'
    }
    Add-Cmd '-grabright'
    Add-Cmd 'vr_mock_button main grip 0'
    Add-Cmd 'map e1m1'
    Wait-Frames 100
    Add-Cmd 'notarget'
    Add-Cmd 'vr_test_spawn 0'
    Add-Cmd 'vr_test_spawn_dist 64'
    Add-Cmd 'impulse 241'
    Wait-Frames 60
    Add-Cmd 'vr_burn_test 1'
    Wait-Frames 90
    Add-Cmd 'echo CASE_burning_monster'
    Add-Cmd 'vr_fire_particles_stats'
    Add-Cmd 'screenshot'
}
Add-Cmd 'echo REVIEW_DONE'
Add-Cmd 'disconnect'
Wait-Frames 5
Add-Cmd 'quit'
Set-Content -Encoding ascii "$reviewBase/quakevr/autoexec.cfg" $steps
$env:QVR_TEST_HIDDEN='1'
$env:QVR_TEST_BACKGROUND='1'
$env:QVR_NO_ERROR_DIALOG='1'
$proc=Start-Process -FilePath $exe -WorkingDirectory $reviewBase -ArgumentList "-basedir `"$reviewBase`" -game hipnotic -game rogue -game quakevr -condebug -window -width 960 -height 540 -nosound -noconfigwrite -noaddons" -WindowStyle Hidden -PassThru
if(-not $proc.WaitForExit(55000)) { "Review process continues: $($proc.Id)"; exit 0 }
"Exit code: $($proc.ExitCode)"
Copy-Item "$reviewBase/qconsole.log" "$reviewBase/$Mode.log"
Get-Content "$reviewBase/qconsole.log" | Select-String 'CASE_|debris:|fireparticles:|tilttest|REVIEW_DONE|rror|missing|too big'
