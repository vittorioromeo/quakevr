param([Parameter(Mandatory=$true)][string]$Base,[Parameter(Mandatory=$true)][string]$Exe,
    [string]$Progs='quakevr/progs.dat')
$reviewBase=(Resolve-Path -LiteralPath $Base).Path
$engine=(Resolve-Path -LiteralPath $Exe).Path
$steps=[System.Collections.Generic.List[string]]::new()
function Cmd([string]$s){$steps.Add($s)}
function Frames([int]$n){for($i=0;$i -lt $n;$i++){Cmd 'wait'}}
foreach($c in @('vr_backend mock','vr_enabled 1','vr_fixed_frames 1','vr_mock_fast 1','sv_autosave 0','developer 1','vr_tips 0')){Cmd $c}
Frames 80
Cmd 'map start'; Frames 140
foreach($c in @('god','notarget','vr_teleporters 1','vr_portals 1','vr_portals_walk 1','vr_grab_gibs 2',
 'vr_roomscale_move_mult 0','vr_mock_hand head 0 1.7 0 0 0 0','setpos 544 1376 24 0 90 0')){Cmd $c}
Frames 20; Cmd 'setpos 544 1376 24 0 90 0'; Frames 10; Cmd 'noclip 0'; Frames 10; Cmd 'vr_roomscale_move_mult 1'
Cmd 'echo CASE_room_before'; Cmd 'vr_portals_info'
for($i=1;$i -le 12;$i++){Cmd ('vr_mock_hand head 0 1.7 '+(-$i*0.1).ToString([Globalization.CultureInfo]::InvariantCulture)+' 0 0 0'); Frames 3}
Frames 10; Cmd 'echo CASE_room_after'; Cmd 'vr_portals_info'
Cmd 'vr_mock_hand head 0 1.7 0 0 0 0'; Frames 5
Cmd 'vr_roomscale_move_mult 0'; Cmd 'setpos 544 1320 24 0 90 0'; Frames 10
Cmd 'vr_mock_hand main 0 1.2 -0.35 63 0 0'; Frames 20
Cmd 'echo CASE_portal_pull'; Cmd 'vr_portals_pulltest'; Frames 20; Cmd '+grabright'; Cmd 'vr_mock_button main grip 1'; Frames 60; Cmd 'vr_mock_button main grip 0'; Cmd '-grabright'; Frames 90
Cmd 'echo CASE_portal_cancel'; Cmd 'vr_portals_pulltest'; Frames 5; Cmd 'vr_teleporters 0'; Frames 45; Cmd 'vr_teleporters 1'; Frames 10
Cmd 'map e1m1'; Frames 140
foreach($c in @('notarget','god','vr_decap 1','vr_decap_corpses 1','vr_hit_precise 1','vr_knockdown_chance 0',
 'vr_test_spawn 0','vr_test_spawn_dist 96','vr_decap_test 30')){Cmd $c}
foreach($test in @(23,24,25,26,27)) {
 Cmd "echo CASE_blunt_$test"
 Cmd ('vr_test_spawn_dead '+[int]($test -eq 23)); Cmd 'impulse 241'; Frames 120
 Cmd "vr_decap_test $test"; Frames 10
 Cmd 'map e1m1'; Frames 140; Cmd 'notarget'; Cmd 'god'
}
Cmd 'echo CASE_gremlin_flip'; Cmd 'vr_test_spawn 12'; Cmd 'vr_test_spawn_dead 1'; Cmd 'impulse 241'; Frames 3
Cmd 'vr_decap_test 31'; Frames 160; Cmd 'vr_knockdown_test 6'; Cmd 'vr_debug_ragdoll 1'; Cmd 'setpos 485 -285 28 0 90 0'; Frames 10; Cmd 'vr_mock_hand_to main ragdoll 0 0'; Cmd '+grabright'; Cmd 'vr_mock_button main grip 1'; Frames 25; Cmd 'vr_decap_test 32'; Cmd 'vr_mock_button main grip 0'; Cmd '-grabright'; Frames 20; Cmd 'vr_decap_test 23'; Frames 20
Cmd 'map e1m1'; Frames 140; Cmd 'notarget'; Cmd 'god'; Cmd 'echo CASE_gremlin_gun'; Cmd 'impulse 241'; Frames 3; Cmd 'vr_decap_test 31'; Frames 160; Cmd 'vr_decap_test 21'; Frames 15
Cmd 'echo REVIEW_DONE'; Cmd 'disconnect'; Frames 5; Cmd 'quit'
Set-Content -Encoding ascii "$reviewBase/quakevr/autoexec.cfg" $steps
Copy-Item -LiteralPath $Progs -Destination "$reviewBase/quakevr/progs.dat"
$env:QVR_TEST_HIDDEN='1';$env:QVR_TEST_BACKGROUND='1';$env:QVR_NO_ERROR_DIALOG='1'
$proc=Start-Process -FilePath $engine -WorkingDirectory $reviewBase -ArgumentList "-basedir `"$reviewBase`" -game hipnotic -game rogue -game quakevr -condebug -window -width 960 -height 540 -nosound -noconfigwrite -noaddons" -WindowStyle Hidden -PassThru
if(-not $proc.WaitForExit(55000)){"Review process continues: $($proc.Id)"; exit 0}
"Exit code: $($proc.ExitCode)"
Copy-Item "$reviewBase/qconsole.log" "$reviewBase/feedback_oct5.log"
Get-Content "$reviewBase/qconsole.log" | Select-String 'CASE_|portalpulltest|force grab:|carried edict|player at|decaptest:|buttongibtest|bodycheck: monster_gremlin|REVIEW_DONE|Error'
