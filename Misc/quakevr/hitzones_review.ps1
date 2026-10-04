param([Parameter(Mandatory=$true)][string]$Base,
    [Parameter(Mandatory=$true)][string]$Exe)
# A disposable game base with this branch's progs.dat and game data. Replaces quakevr/autoexec.cfg.
$reviewBase=(Resolve-Path -LiteralPath $Base).Path
$engine=(Resolve-Path -LiteralPath $Exe).Path
$steps=[System.Collections.Generic.List[string]]::new()
function Cmd([string]$text) { $steps.Add($text) }
function Frames([int]$n) { for($i=0;$i -lt $n;$i++) { Cmd 'wait' } }
function Check([string]$label) { Cmd "echo CASE_$label"; Cmd 'vr_hitzones_check' }
foreach($c in @('vr_backend mock','vr_enabled 1','vr_fixed_frames 1','vr_mock_fast 1','sv_autosave 0',
    'vr_retro 0','developer 1','vr_tips 0','vr_fire_particles 0')) { Cmd $c }
Frames 80
Cmd 'map e1m1'
Frames 100
foreach($c in @('notarget','vr_mock_look 25 0','vr_hit_precise 1','vr_melee_positional 1','vr_positional_damage 1',
    'vr_debug_hitzones 1','vr_hit_head_priority 1','vr_test_spawn 0','vr_test_spawn_dist 64','vr_test_spawn_dead 0',
    'impulse 241')) { Cmd $c }
Frames 60
Check 'stand'
Cmd 'screenshot'
Frames 9
Check 'animated'
Cmd 'vr_decap_test 7'
Frames 8
Check 'pain'
Cmd 'screenshot'
Frames 30
Cmd 'vr_hit_head_priority 0'
Frames 5
Check 'priority_off'
Cmd 'vr_debug_hitzones_xray 1'
Frames 5
Check 'xray'
Cmd 'screenshot'
Cmd 'vr_debug_hitzones 3'
Frames 5
Check 'both'
Cmd 'vr_debug_hitzones 2'
Frames 5
Check 'decap_only'
Cmd 'vr_debug_hitzones 1'
Cmd 'vr_hit_precise 0'
Frames 5
Check 'fallback'
Cmd 'vr_debug_hitzones 0'
Frames 5
Check 'off'
foreach($c in @('vr_hit_precise 1','vr_hit_head_priority 1','vr_debug_hitzones 1','vr_debug_hitzones_xray 0',
    'vr_ragdoll 0','vr_test_spawn_dist 96','vr_test_spawn_dead 1','impulse 241')) { Cmd $c }
Frames 60
Check 'corpse'
Cmd 'screenshot'
Cmd 'vr_hitmodel_check print'
Cmd 'map start'
Frames 100
Check 'map_reset'
Cmd 'echo REVIEW_DONE'
Cmd 'disconnect'
Frames 5
Cmd 'quit'
Set-Content -Encoding ascii "$reviewBase/quakevr/autoexec.cfg" $steps
$env:QVR_TEST_HIDDEN='1'; $env:QVR_TEST_BACKGROUND='1'; $env:QVR_NO_ERROR_DIALOG='1'
$proc=Start-Process -FilePath $engine -WorkingDirectory $reviewBase -ArgumentList "-basedir `"$reviewBase`" -game hipnotic -game rogue -game quakevr -condebug -window -width 960 -height 540 -nosound -noconfigwrite -noaddons" -WindowStyle Hidden -PassThru
if(-not $proc.WaitForExit(55000)) { throw "Review still running: PID $($proc.Id)" }
if($proc.ExitCode -ne 0) { throw "Engine exited with $($proc.ExitCode)" }
Copy-Item "$reviewBase/qconsole.log" "$reviewBase/hitzones.log"
Get-Content "$reviewBase/hitzones.log" | Select-String 'CASE_|vr_hitzones_check:|vr_hitmodel_check:|REVIEW_DONE|Host_Error|decaptest:'
