param([Parameter(Mandatory=$true)][string]$Scenario,[switch]$Interactive,[int]$StartupTimeoutSeconds=60)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$scenarioPath=(Resolve-Path -LiteralPath $Scenario).Path
$data=Get-Content -Raw -LiteralPath $scenarioPath | ConvertFrom-Json
if($data.name -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Invalid evidence file name'}
$evidencePath=Join-Path $projectRoot 'docs/development/test-evidence'
$stdoutPath=Join-Path $evidencePath ($data.name+'-console.log')
$stderrPath=Join-Path $evidencePath ($data.name+'-stderr.log')
$resultPath=Join-Path $evidencePath ($data.name+'.json')
$executablePath=Join-Path $projectRoot 'out/build/release/FlyMe2TheMoon.exe'
$runStarted=Get-Date
$metadata=[ordered]@{name=$data.name;started=$runStarted.ToString('o');executable_sha256=(Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash;scenario_sha256=(Get-FileHash -LiteralPath $scenarioPath -Algorithm SHA256).Hash;interactive=[bool]$Interactive;startup_timeout_seconds=$StartupTimeoutSeconds;exit_code=$null;passed=$false}
# Keep earlier failed attempts and tie each new report to its actual build.
$oldEvidence=@('.json','.bmp','.png','-console.log','-stderr.log','-run.json') | ForEach-Object {Join-Path $evidencePath ($data.name+$_)} | Where-Object {Test-Path -LiteralPath $_}
if($oldEvidence.Count -gt 0){
    $historyPath=Join-Path $evidencePath ('history/'+$data.name+'-'+$runStarted.ToString('yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $historyPath -Force | Out-Null
    foreach($oldPath in $oldEvidence){Move-Item -LiteralPath $oldPath -Destination $historyPath}
}
$oldScenario=$env:FLY_REPLAY_SCENARIO
try {
    $env:FLY_REPLAY_SCENARIO=$scenarioPath
    $windowStyle=if($Interactive){'Normal'}else{'Hidden'}
    $process=Start-Process -FilePath $executablePath -WorkingDirectory $projectRoot -WindowStyle $windowStyle -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    if(!$process.WaitForExit([int](($data.duration+$StartupTimeoutSeconds)*1000))){Stop-Process -Id $process.Id -Force;throw 'Replay timed out; runtime failure scene may be open'}
    $process.Refresh()
    $metadata.exit_code=$process.ExitCode
    if($process.ExitCode -ne 0){throw "Game process exited with code $($process.ExitCode)"}
    $result=Get-Content -Raw -LiteralPath $resultPath | ConvertFrom-Json
    if(!$result.passed){throw 'Replay assertions failed; inspect JSON evidence'}
    $metadata.passed=$true
    Write-Output "PASS: $($data.name), process exit 0"
} finally {
    $metadata.ended=(Get-Date).ToString('o')
    $metadata | ConvertTo-Json | Set-Content -Encoding utf8 -LiteralPath (Join-Path $evidencePath ($data.name+'-run.json'))
    if($null -eq $oldScenario){Remove-Item Env:FLY_REPLAY_SCENARIO -ErrorAction SilentlyContinue}else{$env:FLY_REPLAY_SCENARIO=$oldScenario}
}
