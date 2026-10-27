param(
    [Parameter(Mandatory=$true)][string]$RunRoot,
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$taskRun = (Resolve-Path -LiteralPath $RunRoot).Path
$taskConfig = Join-Path $taskRun 'campaign-config.json'
$taskRunner = Join-Path $taskRun 'snapshot/tools/m6_campaign.py'
$taskPython = (Get-Command $Python).Source
foreach ($taskFile in @($taskConfig,$taskRunner)) {
    if (-not (Test-Path -LiteralPath $taskFile -PathType Leaf)) { throw "Missing frozen campaign file: $taskFile" }
}
$taskDeps = Join-Path (Split-Path $PSScriptRoot -Parent) 'build/m6-root/pydeps'
$env:PYTHONPATH = $taskDeps + [IO.Path]::PathSeparator + $env:PYTHONPATH
& $taskPython -c 'import numpy'
if ($LASTEXITCODE -ne 0) { throw 'Python numpy dependency unavailable.' }
$taskArgs = @($taskRunner, '--config', $taskConfig)
$taskQuoted = ($taskArgs | ForEach-Object {
    if ($_.Contains('"')) { throw 'Invalid argument quote.' }
    '"' + $_ + '"'
}) -join ' '
$taskStamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$taskOut = Join-Path $taskRun ($taskStamp + '.stdout.log')
$taskErr = Join-Path $taskRun ($taskStamp + '.stderr.log')
$taskProcess = Start-Process -FilePath $taskPython -ArgumentList $taskQuoted -WorkingDirectory $taskRun `
    -WindowStyle Hidden -RedirectStandardOutput $taskOut -RedirectStandardError $taskErr -PassThru
$taskRecord = [ordered]@{ pid=$taskProcess.Id; started=(Get-Date).ToString('o');
    status=(Join-Path $taskRun 'campaign-status.json'); stdout=$taskOut; stderr=$taskErr }
$taskRecord | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $taskRun 'launch.json') -Encoding utf8
$taskRecord | ConvertTo-Json
