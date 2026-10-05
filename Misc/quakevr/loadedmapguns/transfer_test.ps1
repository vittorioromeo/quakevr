param([string]$Name = 'loadedmapguns')
# Real e1m3 pickup, immediate fire, then magazine identity through all transfers.
$ErrorActionPreference = 'Stop'
$script = 'external_ents 0; vr_fixed_frames 1; vr_item_objects 0; vr_body_interactions 1; vr_holster_mode 0; vr_weapon_grip_mode 1; vr_reload_mode 2; map e1m3; wait90; god; notarget; noclip; setpos -712 -1240 56; wait60; vr_test_weaponinst 0; impulse 120; wait5; +attack; wait12; -attack; wait30; vr_mock_hand main 0.8 1.7 -0.8; vr_mock_hand off -0.8 1.7 -0.8; vr_test_weaponinst 0; impulse 120; wait5; vr_test_weaponinst_slot 4; vr_test_weaponinst 1; impulse 120; wait5; vr_test_weaponinst 2; impulse 120; wait5; vr_test_weaponinst 3; impulse 120; wait5; vr_test_weaponinst 4; impulse 120; wait5; vr_test_weaponinst 5; impulse 120; wait5; vr_test_weaponinst 6; impulse 120; wait5; vr_test_weaponinst 7; impulse 120; wait5; vr_test_weaponinst 8; impulse 120; wait5; edict 1; wait3; echo TRANSFER_END; toggleconsole; quit'
$output = (& 'C:/Program Files/Git/bin/bash.exe' C:/OHWorkspace/qvr-kit/run.sh $Name -Script $script -ExtraArgs '-nomapindex -noaddons' -Filter 'weaponinst|ammo_nails|You got|TRANSFER_END|ERROR|ENGINE|Unknown' -Timeout 90 2>&1 | Out-String)
$output | Set-Content -Encoding ascii (Join-Path $PSScriptRoot 'transfers.log')
Write-Output $output
if ($output -match 'ENGINE|ERROR|Unknown|TIMEOUT|exit=[^0]|weaponinst test:') { throw 'Transfer run failed' }
$clips = [regex]::Matches($output, 'weapon 6 #(\d+) clip\s+(\d+)')
if ($clips.Count -ne 10) { throw "Expected 10 nailgun locations, got $($clips.Count)" }
$uid = $clips[0].Groups[1].Value
if ($clips[0].Groups[2].Value -ne '24') { throw 'Fresh nailgun not loaded' }
$used = [int]$clips[1].Groups[2].Value
if ($used -ge 24 -or $used -le 0) { throw 'Fresh nailgun did not fire immediately' }
foreach ($clip in @($clips)[1..9]) {
    if ($clip.Groups[1].Value -ne $uid -or [int]$clip.Groups[2].Value -ne $used) { throw 'Transfer changed magazine or identity' }
}
if ($output -notmatch 'ammo_nails\s+6\s') { throw 'Transfer duplicated reserve ammo' }
Write-Output "PASS real e1m3 nailgun #$uid : fresh clip=24 fired clip=$used reserve=6; 8 transfers preserve identity/ammo"
