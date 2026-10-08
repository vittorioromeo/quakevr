param([Parameter(Mandatory=$true)][string]$Base,[Parameter(Mandatory=$true)][string]$Exe)
$reviewBase=(Resolve-Path -LiteralPath $Base).Path
$engine=(Resolve-Path -LiteralPath $Exe).Path
$steps=[System.Collections.Generic.List[string]]::new()
function Cmd([string]$s){$steps.Add($s)}
function Frames([int]$n){for($i=0;$i -lt $n;$i++){Cmd 'wait'}}
foreach($c in @('vr_backend mock','vr_enabled 1','vr_fixed_frames 1','vr_mock_fast 1','sv_autosave 0','developer 1','vr_tips 0')){Cmd $c}
Frames 80
Cmd 'map start'; Frames 140
foreach($c in @('god','notarget','vr_teleporters 1','vr_portals 1','vr_portals_walk 1','vr_roomscale_move_mult 0',
 'vr_mock_hand head 0 1.7 0 0 0 0','setpos 544 1360 24 0 90 0','vr_mock_hand main 0 1.2 -0.35 63 0 0')){Cmd $c}
Frames 20
Cmd 'setpos 544 1360 24 0 90 0'; Cmd 'noclip 0'; Frames 10
Cmd 'vr_mock_hand_to main 544 1378 48'; Frames 20
Cmd 'echo CASE_reach_before'; Cmd 'vr_portals_reachtest'; Cmd 'vr_physics_portals'
Cmd 'vr_mock_hand_to main 544 1398 48'; Frames 20
Cmd 'echo CASE_reach_across'; Cmd 'vr_portals_reachtest'; Cmd 'vr_physics_portals'; Cmd 'vr_portals_shot'; Frames 5
Cmd 'vr_mock_hand_to main 544 1370 48'; Frames 20
Cmd 'echo CASE_reach_withdraw'; Cmd 'vr_portals_reachtest'; Cmd 'vr_physics_portals'
Cmd '+grabright'; Cmd 'vr_mock_button main grip 1'; Cmd 'vr_test_spawn 100'; Cmd 'vr_test_spawn_hold 1'; Cmd 'vr_test_spawn_dead 0'; Cmd 'impulse 241'; Frames 20
Cmd 'vr_mock_hand_to main 544 1383 48'; Frames 15
Cmd 'echo CASE_prop_straddles'; Cmd 'vr_portals_reachtest'; Cmd 'vr_physics_portals'
Cmd 'vr_mock_hand_to main 544 1398 48'; Frames 20
Cmd 'echo CASE_prop_across'; Cmd 'vr_portals_reachtest'; Cmd 'vr_physics_portals'
Cmd 'vr_mock_hand_to main 544 1374 48'; Frames 20
Cmd 'echo CASE_prop_back'; Cmd 'vr_portals_reachtest'; Cmd 'vr_physics_portals'
Cmd 'vr_mock_hand_to main 544 1400 48'; Frames 20
Cmd 'vr_mock_button main grip 0'; Cmd '-grabright'; Frames 5
Cmd 'echo CASE_prop_release'; Cmd 'vr_physics_info'; Frames 50
for($i=0;$i -lt 3;$i++){Cmd 'map start'; Frames 120; Cmd 'echo CASE_reload'; Cmd 'vr_physics_portals'}
Cmd 'echo REACH_REVIEW_DONE'; Cmd 'disconnect'; Frames 5; Cmd 'quit'
Set-Content -Encoding ascii "$reviewBase/quakevr/autoexec.cfg" $steps
Copy-Item -LiteralPath quakevr/progs.dat -Destination "$reviewBase/quakevr/progs.dat"
$env:QVR_TEST_HIDDEN='1';$env:QVR_TEST_BACKGROUND='1';$env:QVR_NO_ERROR_DIALOG='1'
$proc=Start-Process -FilePath $engine -WorkingDirectory $reviewBase -ArgumentList "-basedir `"$reviewBase`" -game hipnotic -game rogue -game quakevr -condebug -window -width 960 -height 540 -nosound -noconfigwrite -noaddons" -WindowStyle Hidden -PassThru
if(-not $proc.WaitForExit(55000)){"Review process continues: $($proc.Id)";exit 0}
"Exit code: $($proc.ExitCode)"
Copy-Item "$reviewBase/qconsole.log" "$reviewBase/reach.log"
Get-Content "$reviewBase/reach.log" | Select-String 'CASE_|reachtest:|physicsportals:|portal reach:|carry:|REACH_REVIEW_DONE|Error|GLSL'
