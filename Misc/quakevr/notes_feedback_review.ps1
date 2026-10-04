param([Parameter(Mandatory=$true)][string]$Base,
    [Parameter(Mandatory=$true)][string]$Exe)
# Disposable game directory only: replaces its autoexec.cfg and writes test saves.
$reviewBase=(Resolve-Path -LiteralPath $Base).Path
$engine=(Resolve-Path -LiteralPath $Exe).Path
$steps=[System.Collections.Generic.List[string]]::new()
function Cmd([string]$text) { $steps.Add($text) }
function Frames([int]$n) { for($i=0;$i -lt $n;$i++) { Cmd 'wait' } }
function Case([string]$text) { Cmd "echo CASE_$text" }
foreach($c in @('vr_backend mock','vr_enabled 1','vr_fixed_frames 1','vr_mock_fast 1','sv_autosave 0',
    'developer 1','vr_tips 0','vr_fire_particles 0','vr_flashlight 1')) { Cmd $c }
Frames 80
Cmd 'map e1m1'
Frames 140
Cmd 'notarget'
Cmd 'vr_flashlight_clip_head right'
Cmd 'vr_flashlight_toggle'
Cmd 'save feedback_head'
Frames 30
Cmd 'load feedback_head'
Frames 180
Cmd 'vr_flashlight_probe head_load_on'
Cmd 'vr_flashlight_toggle'
Cmd 'save feedback_off'
Frames 30
Cmd 'load feedback_off'
Frames 180
Cmd 'vr_flashlight_probe head_load_off'
Cmd 'changelevel e1m2'
Frames 180
Cmd 'vr_flashlight_probe head_transition_off'
Cmd 'vr_mock_hand off -0.6 1.0 -0.4 0 0 0'
Cmd 'vr_mock_button off grip 1'
Cmd 'vr_flashlight_give left'
Frames 10
Cmd 'vr_mock_button off secondary 1'
Frames 2
Cmd 'vr_mock_button off secondary 0'
Frames 35
Cmd 'vr_flashlight_probe held_before'
Cmd 'save feedback_held'
Frames 30
Cmd 'load feedback_held'
Frames 180
Cmd 'vr_flashlight_probe held_load'
Cmd 'vr_mock_button off grip 0'
Frames 60
Cmd 'vr_flashlight_probe held_released'
Cmd 'vr_weapon_grip_mode 1'
Cmd 'vr_mock_hand main 0.25 1.3 -0.35 0 0 0'
Cmd 'vr_mock_button main grip 1'
Cmd 'impulse 154'
Frames 40
Cmd 'vr_flashlight_clip_gun right'
Cmd 'vr_flashlight_toggle'
Frames 10
Cmd 'vr_flashlight_probe gun_before'
Cmd 'save feedback_gun'
Frames 30
Cmd 'load feedback_gun'
Frames 180
Cmd 'vr_flashlight_probe gun_load_on'
Cmd 'changelevel e1m1'
Frames 180
Cmd 'vr_flashlight_probe gun_transition_on'
foreach($c in @('notarget','vr_ragdoll 1','vr_ragdoll_max 2','vr_debug_ragdoll 1','vr_test_spawn 8',
    'vr_test_spawn_dist 64','vr_test_spawn_dead 0','impulse 241')) { Cmd $c }
Frames 50
Case 'nail_death'
Cmd 'vr_knockdown_test 8'
Frames 120
Cmd 'vr_knockdown_test 6'
foreach($distance in @(96,128,160)) {
    Cmd "vr_test_spawn_dist $distance"
    Cmd 'vr_test_spawn_dead 1'
    Cmd 'impulse 241'
    Frames 120
    Case "cap_$distance"
    Cmd 'vr_knockdown_test 6'
}
Cmd 'vr_ragdoll_grab 2'
Cmd 'vr_physics_forcegrab monster_enforcer'
Case 'corpse_head'
Cmd 'map e1m1'
Frames 140
foreach($c in @('notarget','vr_test_spawn 0','vr_test_spawn_dist 96','vr_test_spawn_dead 1','impulse 241')) { Cmd $c }
Frames 120
foreach($c in @('vr_hit_precise 1','vr_decap 1','vr_decap_shotgun 1','vr_decap_super_shotgun 1','vr_decap_lightning 1',
    'vr_debug_shots 1','vr_hit_head_priority 1','vr_decap_test 21')) { Cmd $c }
Frames 15
Cmd 'vr_knockdown_test 6'
Case 'pickups'
foreach($weapon in @('shotgun','supershotgun','nailgun','supernailgun','grenadelauncher','rocketlauncher','lightning',
    'crowbar','mjolnir','laser_gun','proximity_gun')) {
    Cmd "vr_physics_spawn weapon_$weapon 64"
    Frames 5
}
Frames 20
Cmd 'vr_physics_list props'
Cmd 'vr_walltorch_tilt_test'
Cmd 'map e1m1'
Frames 140
foreach($c in @('notarget','vr_test_spawn 0','vr_test_spawn_dead 0','vr_test_spawn_dist 64',
    'vr_knockdown_time_min 60','vr_knockdown_time_max 60','vr_knockdown_wiggle 0','impulse 241')) { Cmd $c }
Frames 40
Cmd 'vr_knockdown_test 0'
Frames 1000
Case 'wiggle_off'
Cmd 'vr_knockdown_test 9'
Cmd 'vr_knockdown_wiggle 0.7'
Case 'wiggle_on'
for($i=0;$i -lt 18;$i++) { Frames 10; Cmd 'vr_knockdown_test 9' }
Cmd 'map e2m1'
Frames 140
foreach($c in @('notarget','god','noclip','setpos 1688 300 -8 0 90 0','vr_test_spawn 0','vr_test_spawn_dead 0',
    'vr_test_spawn_dist 64','vr_knockdown_time_min 60','vr_knockdown_time_max 60','impulse 241')) { Cmd $c }
Frames 25
Cmd 'vr_knockdown_test 0'
Frames 90
Cmd 'setpos 1688 397 -120 0 90 0'
Frames 5
Cmd 'vr_knockdown_test 7'
Frames 90
Case 'drowning_breath'
Cmd 'vr_knockdown_test 6'
Frames 1400
Case 'drowned_knockdown'
Cmd 'vr_knockdown_test 6'
Cmd 'echo REVIEW_DONE'
Cmd 'disconnect'
Frames 5
Cmd 'quit'
Set-Content -Encoding ascii "$reviewBase/quakevr/autoexec.cfg" $steps
$env:QVR_TEST_HIDDEN='1'; $env:QVR_TEST_BACKGROUND='1'; $env:QVR_NO_ERROR_DIALOG='1'
$proc=Start-Process -FilePath $engine -WorkingDirectory $reviewBase -ArgumentList "-basedir `"$reviewBase`" -game hipnotic -game rogue -game quakevr -condebug -window -width 960 -height 540 -nosound -noconfigwrite -noaddons" -WindowStyle Hidden -PassThru
if(-not $proc.WaitForExit(55000)) { "Review process continues: $($proc.Id)"; exit 0 }
"Exit code: $($proc.ExitCode)"
Copy-Item "$reviewBase/qconsole.log" "$reviewBase/feedback.log"
Get-Content "$reviewBase/qconsole.log" | Select-String 'CASE_|bodycheck|breathcheck|torchprobe .*mode|restored state|limp at frame|head popped|decaptest|never force|vr_physics_spawn|tilttest|REVIEW_DONE|Error'
