param(
    [string]$Name = 'adpbrload',
    [string]$Report = 'docs/vr-port/pbr_stock_audit.json',
    [string]$Tag = 'stock-fixed',
    [string]$ExtraArgs = '',
    [int]$Smooth = 1,
    [int]$External = 1,
    [switch]$Representative,
    [switch]$WorldOnly
)
# Read-only game installation. run.sh writes test state only in the assigned kit/worktree.
$audit = Get-Content $Report -Raw | ConvertFrom-Json
$tree = "C:/OHWorkspace/qvr-agents/$Name"
foreach ($campaign in 'id1','hipnotic','rogue') {
    $maps = @($audit.records | Where-Object { $_.campaign -eq $campaign -and $_.kind -eq 'bsp' -and $_.asset -notmatch '^maps/b_' } | Select-Object -ExpandProperty asset -Unique | Sort-Object)
    if ($Representative) {
        $map = switch ($campaign) { id1 {'maps/e1m1.bsp'} hipnotic {'maps/hip1m1.bsp'} rogue {'maps/r1m1.bsp'} }
        $maps = @($map)
    }
    $commands = @("vr_texture_smooth $Smooth", 'gl_texturemode GL_NEAREST_MIPMAP_LINEAR', "vr_extmaps $External", 'vr_pbr_strength 8', "vr_campaign_select $campaign", 'wait2')
    foreach ($map in $maps) {
        $base = $map.Substring(5, $map.Length - 9)
        foreach ($mode in 0,1) {
            $commands += "vr_pbr $mode"
            $commands += "map $base"
            $commands += 'wait2'
            $commands += $(if ($WorldOnly) { 'vr_pbr_audit world' } else { 'vr_pbr_audit' })
        }
    }
    $commands += 'toggleconsole'
    $commands += 'quit'
    & 'C:/Program Files/Git/bin/bash.exe' C:/OHWorkspace/qvr-kit/run.sh $Name -Script ($commands -join ';') -Timeout 900 -ExtraArgs $ExtraArgs -Filter 'ENGINE|CRASH|ERROR|Host_Error|not found|Unknown command'
    Copy-Item "C:/OHWorkspace/qvr-kit/bases/$Name/qbase/qconsole.log" "$tree/scratch/$Tag-$campaign.log" -Force
    "$Tag $campaign`: $($maps.Count) maps captured"
}
