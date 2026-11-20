# mp_local.ps1 -- two Quake VR games on this PC for a hands-on multiplayer test (docs/vr-port/MULTIPLAYER.md): a host
# (flat, in a window) and a second game that joins it over 127.0.0.1, so the second window plays through the network
# path (snapshots, prediction, lag compensation). Bots are added on the host once its map is up. Nothing is sent off
# this PC unless -Room is given (the host then registers a room code with the room service).
#
#   powershell -File Misc/quakevr/mp/mp_local.ps1                          co-op e1m1, one co-op bot
#   powershell -File Misc/quakevr/mp/mp_local.ps1 -Mode dm -Map dm3 -Bots 2
#   powershell -File Misc/quakevr/mp/mp_local.ps1 -Mode dm -Melee          a melee duel: guns refused, the bot parries every blow it sees
#   powershell -File Misc/quakevr/mp/mp_local.ps1 -Room                    the host gets a room code (Host Game page; vr_mp_roomcode)
#
# -BaseDir: the Quake folder (id1, and quakevr/ or a link to it); default the Steam one. -Exe: default this checkout's
# newest build (ReleaseLTO or Release). The joining game writes no config (-noconfigwrite) and opens no microphone.
# Each game reads commands appended to its inbox file (printed at the end): e.g. "impulse 100" adds a bot on the host;
# close the windows as usual when done.
param(
    [ValidateSet("coop", "dm")][string]$Mode = "coop",
    [string]$Map = "",
    [int]$Bots = 1,
    [int]$Skill = 1,
    [switch]$Melee,
    [switch]$Room,
    [switch]$NoClient,
    [int]$Width = 1280,
    [int]$Height = 720,
    [int]$Port = 26000,
    [int]$BotDelay = 10,
    [string]$BaseDir = "C:\Program Files (x86)\Steam\steamapps\common\Quake",
    [string]$Exe = "",
    [string[]]$ExtraArgs = @(),
    [string]$InboxDir = $env:TEMP
)
$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
if (-not $Exe) {
    # (the newest build of this checkout: ReleaseLTO or Release)
    $bin = Join-Path $repo "Windows\VisualStudio\Build-ironwail\bin\x64"
    $found = @("ReleaseLTO", "Release") | ForEach-Object { Join-Path $bin "$_\ironwail.exe" } | Where-Object { Test-Path $_ }
    if ($found) { $Exe = ($found | Get-Item | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName }
    else { $Exe = Join-Path $bin "Release\ironwail.exe" }
}
if (-not (Test-Path $Exe)) { throw "no ironwail.exe at $Exe (build it, or pass -Exe)" }
if (-not (Test-Path (Join-Path $BaseDir "id1"))) { throw "no id1 folder in $BaseDir (pass -BaseDir <your Quake folder>)" }
if (-not $Map) { $Map = if ($Mode -eq "dm") { "dm3" } else { "e1m1" } }

# Each game's command inbox (-cmdinbox: lines appended to it run in its console): the bots go in through the host's.
$inbox = Join-Path $InboxDir "qvr_mp_local_inbox.txt"
$clientInbox = Join-Path $InboxDir "qvr_mp_local_inbox_client.txt"
Set-Content -Path $inbox -Value "" -Encoding ascii
Set-Content -Path $clientInbox -Value "" -Encoding ascii

$games = @("-game", "hipnotic", "-game", "rogue", "-game", "quakevr")
$common = @("-basedir", "`"$BaseDir`"") + $games + @("-flatscreenmode", "-window", "-width", $Width, "-height", $Height)
# (both on 127.0.0.1, as the test harness's games: nothing listens outside this PC; -Room: the host on every address)
if (-not $Room) { $common += @("-ip", "127.0.0.1") }
$common += $ExtraArgs
$modeArgs = if ($Mode -eq "dm") { @("+coop", "0", "+deathmatch", "1") } else { @("+deathmatch", "0", "+coop", "1") }
$hostArgs = $common + @("-port", $Port, "-cmdinbox", "`"$inbox`"",
    "+vr_mp_room", $(if ($Room) { "1" } else { "0" }), "+maxplayers", "4", "+skill", $Skill) + $modeArgs
if ($Melee) { $hostArgs += @("+vr_run_meleeonly", "1", "+vr_bot_parry_chance", "100") }
$hostArgs += @("+map", $Map)

Write-Host "host: $Exe $($hostArgs -join ' ')"
Start-Process -FilePath $Exe -ArgumentList $hostArgs -WorkingDirectory (Split-Path $Exe) | Out-Null
if (-not $NoClient) {
    # (started at once, it connects once both are up: a "+connect" on its command line would run before its own
    # start-up map and fail)
    $clientArgs = $common + @("-port", ($Port + 1), "-noconfigwrite", "-nomic", "-cmdinbox", "`"$clientInbox`"")
    Write-Host "client: $Exe $($clientArgs -join ' ')"
    Start-Process -FilePath $Exe -ArgumentList $clientArgs -WorkingDirectory (Split-Path $Exe) | Out-Null
}

Write-Host "waiting $BotDelay s for both games to start..."
Start-Sleep -Seconds $BotDelay
for ($i = 0; $i -lt $Bots; $i++) {
    Add-Content -Path $inbox -Value "impulse 100" -Encoding ascii # (a frame or two apart: each adds one bot)
    Start-Sleep -Milliseconds 500
}
if ($Bots -gt 0) { Write-Host "host: $Bots bot(s) added (impulse 100; 'status' lists only the human players)" }
if (-not $NoClient) {
    Add-Content -Path $clientInbox -Value "connect 127.0.0.1:$Port" -Encoding ascii
    Write-Host "client: connecting to 127.0.0.1:$Port"
}
Write-Host "Commands for a game: append a line to its inbox ($inbox, ${clientInbox}), e.g. 'impulse 100' for one more bot."
