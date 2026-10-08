param([Parameter(Mandatory=$true)][string]$Base,[Parameter(Mandatory=$true)][string]$Exe)
$reviewBase=(Resolve-Path -LiteralPath $Base).Path
$engine=(Resolve-Path -LiteralPath $Exe).Path
$steps=[System.Collections.Generic.List[string]]::new()
function Cmd([string]$s){$steps.Add($s)}
function Frames([int]$n){for($i=0;$i -lt $n;$i++){Cmd 'wait'}}
foreach($c in @('vr_backend mock','vr_enabled 1','vr_fixed_frames 1','vr_mock_fast 1','sv_autosave 0','developer 1','vr_tips 0')){Cmd $c}
Frames 80
Cmd 'map start'; Frames 140
foreach($c in @('notarget 1','god 0','vr_teleporters 1','vr_portals 1','vr_portals_walk 1','vr_portals_ai 1',
 'vr_roomscale_move_mult 0','vr_grunt_burst 1','vr_grunt_burst_damage 4','vr_mock_hand head 0 1.7 0 0 0 0',
 'setpos 544 1640 24 0 270 0','vr_test_spawn 0','vr_test_spawn_dist 0','vr_test_spawn_dead 0','impulse 241')){Cmd $c}
Frames 30; Cmd 'setpos 544 1320 24 0 90 0'; Frames 20; Cmd 'noclip 0'; Frames 30; Cmd 'notarget 0'; Frames 1
Cmd 'vr_portals_info'; Cmd 'echo CASE_acquire'; Cmd 'vr_portals_ai_test 1'; Frames 2
Cmd 'echo CASE_muzzle'; Cmd 'vr_portals_ai_test 11'; Frames 2
Cmd 'echo CASE_bullet'; Cmd 'vr_portals_ai_test 2'; Frames 10; Cmd 'vr_portals_ai_test 6'; Frames 2
Cmd 'echo CASE_laser'; Cmd 'vr_portals_ai_test 3'; Frames 45; Cmd 'vr_portals_ai_test 6'; Frames 2
Cmd 'echo CASE_lightning'; Cmd 'vr_portals_ai_test 4'; Frames 10; Cmd 'vr_portals_ai_test 6'; Frames 2
Cmd 'echo CASE_wizard'; Cmd 'vr_portals_ai_test 5'; Frames 110; Cmd 'vr_portals_ai_test 6'; Frames 2
Cmd 'echo CASE_delayed_closed'; Cmd 'vr_portals_ai_test 5'; Frames 2; Cmd 'vr_teleporters 0'; Frames 110; Cmd 'vr_portals_ai_test 6'; Frames 2
Cmd 'echo CASE_closed_acquire'; Cmd 'vr_portals_ai_test 7'; Frames 2
Cmd 'vr_teleporters 1'; Cmd 'vr_portals_ai_test 7'; Frames 2
Cmd 'echo CASE_near_blocked'; Cmd 'vr_portals_ai_test 8'; Frames 2; Cmd 'vr_portals_ai_test 11'; Frames 2; Cmd 'vr_portals_ai_test 7'; Frames 2; Cmd 'vr_portals_ai_test 10'; Frames 2
Cmd 'vr_portals_ai_test 7'; Frames 2
Cmd 'echo CASE_far_blocked'; Cmd 'vr_portals_ai_test 9'; Frames 2; Cmd 'vr_portals_ai_test 11'; Frames 2; Cmd 'vr_portals_ai_test 7'; Frames 2; Cmd 'vr_portals_ai_test 10'; Frames 2
Cmd 'echo CASE_behind'; Cmd 'vr_portals_ai_test 16'; Frames 2
Cmd 'echo CASE_ai_off'; Cmd 'vr_portals_ai 0'; Cmd 'vr_portals_ai_test 7'; Frames 2; Cmd 'vr_portals_ai 1'
Cmd 'echo CASE_outside'; Cmd 'setpos 700 1320 24 0 90 0'; Frames 2; Cmd 'vr_portals_ai_test 7'; Frames 2
Cmd 'setpos 544 1320 24 0 90 0'; Frames 2; Cmd 'vr_portals_ai_test 7'; Frames 2
Cmd 'echo CASE_native_attack'; Cmd 'vr_portals_ai_test 12'; Frames 500; Cmd 'vr_portals_ai_test 13'; Frames 2; Cmd 'vr_portals_ai_test 6'; Frames 2
Cmd 'echo PORTAL_AI_REVIEW_DONE'; Cmd 'disconnect'; Frames 5; Cmd 'quit'
Set-Content -Encoding ascii "$reviewBase/quakevr/autoexec.cfg" $steps
Copy-Item -LiteralPath quakevr/progs.dat -Destination "$reviewBase/quakevr/progs.dat"
$env:QVR_TEST_HIDDEN='1';$env:QVR_TEST_BACKGROUND='1';$env:QVR_NO_ERROR_DIALOG='1'
$proc=Start-Process -FilePath $engine -WorkingDirectory $reviewBase -ArgumentList "-basedir `"$reviewBase`" -game hipnotic -game rogue -game quakevr -condebug -window -width 960 -height 540 -nosound -noconfigwrite -noaddons" -WindowStyle Hidden -PassThru
if(-not $proc.WaitForExit(55000)){"Review process continues: $($proc.Id)";exit 0}
"Exit code: $($proc.ExitCode)"
Copy-Item "$reviewBase/qconsole.log" "$reviewBase/portal_ai_reverse.log"
Get-Content "$reviewBase/portal_ai_reverse.log" | Select-String 'CASE_|portalaitest:|PORTAL_AI_REVIEW_DONE|Error|GLSL'
