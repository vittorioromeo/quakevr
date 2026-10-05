param([Parameter(Mandatory=$true)][string]$Base,[Parameter(Mandatory=$true)][string]$Exe)
$reviewBase=(Resolve-Path -LiteralPath $Base).Path
$engine=(Resolve-Path -LiteralPath $Exe).Path
$steps=[System.Collections.Generic.List[string]]::new()
function Cmd([string]$s){$steps.Add($s)}
function Frames([int]$n){for($i=0;$i -lt $n;$i++){Cmd 'wait'}}
foreach($c in @('vr_backend mock','vr_enabled 1','vr_fixed_frames 1','vr_mock_fast 1','sv_autosave 0','developer 1','vr_tips 0')){Cmd $c}
Frames 80
Cmd 'map e1m1'; Frames 140; Cmd 'god'; Cmd 'notarget'; Cmd 'vr_decap 0'
Cmd 'vr_test_spawn 0'; Cmd 'vr_test_spawn_dead 0'; Cmd 'vr_test_spawn_dist 96'; Cmd 'impulse 241'; Frames 30
Cmd 'echo CASE_live_shock'; Cmd 'vr_decap_test 33'; Frames 8; Cmd 'vr_shock_info'; Frames 80; Cmd 'echo CASE_live_expired'; Cmd 'vr_shock_info'
Cmd 'map e1m1'; Frames 140; Cmd 'god'; Cmd 'notarget'; Cmd 'vr_test_spawn_dead 1'; Cmd 'impulse 241'; Frames 140
Cmd 'echo CASE_corpse_shock'; Cmd 'vr_decap_test 22'; Frames 8; Cmd 'vr_shock_info'; Frames 80; Cmd 'echo CASE_corpse_expired'; Cmd 'vr_shock_info'
Cmd 'echo SHOCK_REVIEW_DONE'; Cmd 'disconnect'; Frames 5; Cmd 'quit'
Set-Content -Encoding ascii "$reviewBase/quakevr/autoexec.cfg" $steps
Copy-Item -LiteralPath quakevr/progs.dat -Destination "$reviewBase/quakevr/progs.dat"
$env:QVR_TEST_HIDDEN='1';$env:QVR_TEST_BACKGROUND='1';$env:QVR_NO_ERROR_DIALOG='1'
$proc=Start-Process -FilePath $engine -WorkingDirectory $reviewBase -ArgumentList "-basedir `"$reviewBase`" -game hipnotic -game rogue -game quakevr -condebug -window -width 960 -height 540 -nosound -noconfigwrite -noaddons" -WindowStyle Hidden -PassThru
if(-not $proc.WaitForExit(55000)){"Review process continues: $($proc.Id)";exit 0}
"Exit code: $($proc.ExitCode)"
Copy-Item "$reviewBase/qconsole.log" "$reviewBase/body_shock.log"
Get-Content "$reviewBase/body_shock.log" | Select-String 'CASE_|bodyshock|SHOCK_REVIEW_DONE|Error|GLSL'
