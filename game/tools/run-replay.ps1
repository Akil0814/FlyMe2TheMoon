param([Parameter(Mandatory=$true)][string]$Scenario)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$scenarioPath=(Resolve-Path -LiteralPath $Scenario).Path
$data=Get-Content -Raw -LiteralPath $scenarioPath | ConvertFrom-Json
if($data.name -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Invalid evidence file name'}
$evidencePath=Join-Path $projectRoot 'docs/development/test-evidence'
$stdoutPath=Join-Path $evidencePath ($data.name+'-console.log')
$stderrPath=Join-Path $evidencePath ($data.name+'-stderr.log')
$oldScenario=$env:FLY_REPLAY_SCENARIO
try {
    $env:FLY_REPLAY_SCENARIO=$scenarioPath
    $process=Start-Process -FilePath (Join-Path $projectRoot 'out/build/release/FlyMe2TheMoon.exe') -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    if(!$process.WaitForExit([int](($data.duration+25)*1000))){Stop-Process -Id $process.Id -Force;throw 'Replay timed out; runtime failure scene may be open'}
    $process.Refresh()
    if($process.ExitCode -ne 0){throw "Game process exited with code $($process.ExitCode)"}
    $result=Get-Content -Raw -LiteralPath (Join-Path $evidencePath ($data.name+'.json')) | ConvertFrom-Json
    if(!$result.passed){throw 'Replay assertions failed; inspect JSON evidence'}
    Write-Output "PASS: $($data.name), process exit 0"
} finally {
    if($null -eq $oldScenario){Remove-Item Env:FLY_REPLAY_SCENARIO -ErrorAction SilentlyContinue}else{$env:FLY_REPLAY_SCENARIO=$oldScenario}
}
