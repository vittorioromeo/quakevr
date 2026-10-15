param([string]$Name='lightaudit',[int]$Runs=5)
$kit='C:/OHWorkspace/qvr-kit'
$tree="C:/OHWorkspace/qvr-agents/$Name"
$script='developer 1;map vrmelee-recording;wait60;map e1m1;wait60;map e1m2;wait60;echo MAP_PARSER_CONSOLE_PASS;toggleconsole;quit'
for ($i=1;$i -le $Runs;$i++) {
    $output=& 'C:/Program Files/Git/bin/bash.exe' "$kit/run.sh" $Name -Script $script -ExtraArgs '-nomapindex +developer 1' -Filter 'MAP_PARSER_CONSOLE|ENGINE (ERROR|CRASH)|Host_Error|exit=' -Timeout 180
    $output | Set-Content "$tree/scratch/map_parser_console_$i.log"
    $text=$output -join [Environment]::NewLine
    if ($text -notmatch 'exit=0' -or $text -notmatch 'MAP_PARSER_CONSOLE_PASS' -or $text -match 'ENGINE (ERROR|CRASH)|Host_Error') { throw "parser console run $i failed; inspect scratch log" }
    $full=Get-Content "$kit/bases/$Name/qbase/qconsole.log" -Raw
    $finds=([regex]::Matches($full,'FindFile:')).Count
    if ($finds -lt 1) { throw "parser console run $i did not exercise developer file logging" }
    Write-Output "map parser console run $i PASS ($finds file-log lines, 3 map loads)"
}
