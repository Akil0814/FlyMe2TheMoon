param([int]$First=1,[int]$Last=60,[string]$Run='entry01')
$ErrorActionPreference='Stop'
if($First -lt 1 -or $Last -gt 60 -or $First -gt $Last -or $Run -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Invalid sweep range / name'}
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$scenarioDirectory=Join-Path $projectRoot 'game/testing/sweeps'
New-Item -ItemType Directory -Path $scenarioDirectory -Force | Out-Null
$results=@()
for($level=$First;$level -le $Last;$level++){
    $name=('{0}-level{1:000}' -f $Run,$level)
    $scenario=Join-Path $scenarioDirectory ($name+'.json')
    @{name=$name;level=$level;duration=2.5;events=@();expect=@{level=$level}} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $scenario -Encoding utf8
    $status='passed';$detail=''
    try { & (Join-Path $PSScriptRoot 'run-replay.ps1') $scenario }
    catch { $status='failed';$detail=$_.Exception.Message;Write-Output "FAIL: $name $detail" }
    $results+=@{level=$level;status=$status;detail=$detail;evidence=$name}
    ConvertTo-Json -InputObject $results -Depth 8 | Set-Content -LiteralPath (Join-Path $projectRoot "docs/development/test-evidence/$Run-summary.json") -Encoding utf8
}
if(@($results | Where-Object {$_.status -eq 'failed'}).Count -gt 0){throw 'At least one level entry failed; see sweep summary'}
