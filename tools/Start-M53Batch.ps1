param(
    [Parameter(Mandatory=$true)][string]$Corpus,
    [Parameter(Mandatory=$true)][string]$RunRoot,
    [string]$Qps = '40,100,160,220',
    [ValidateRange(1,128)][int]$Jobs = 1,
    [ValidateRange(1,100)][int]$Repeats = 3,
    [string]$Python = 'python',
    [string]$App,
    [string]$Decoder,
    [string]$FFmpeg,
    [string]$VmafModel,
    [switch]$Resume,
    [switch]$CrossPlatform
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskRun = [IO.Path]::GetFullPath($RunRoot)
$taskCorpus = (Resolve-Path -LiteralPath $Corpus).Path
$taskPython = (Get-Command $Python).Source
if (-not $App) { $App = Join-Path $taskRoot 'build/m53-root/Windows-final/bin/SvtAv2EncApp.exe' }
if (-not $Decoder) { $Decoder = Join-Path $taskRoot '../AVM-AV2-TEST/tools/bin/avmdec.exe' }
if (-not $FFmpeg) { $FFmpeg = (Get-Command ffmpeg).Source }
if (-not $VmafModel) { $VmafModel = Join-Path $taskRoot 'artifacts/m53-root/deps/vmaf_v0.6.1.json' }
$taskDeps = Join-Path $taskRoot 'build/m5-root/pydeps'
if (Test-Path -LiteralPath $taskDeps) { $env:PYTHONPATH = $taskDeps + [IO.Path]::PathSeparator + $env:PYTHONPATH }
& $taskPython -c 'import numpy'
if ($LASTEXITCODE -ne 0) { throw 'Python requires numpy; install it before launching.' }
if ((Test-Path -LiteralPath $taskRun) -and -not $Resume) { throw 'RunRoot exists; choose a fresh directory or use -Resume.' }
$taskSnapshot = Join-Path $taskRun 'snapshot'
if (-not $Resume) {
    New-Item -ItemType Directory -Path (Join-Path $taskSnapshot 'tools') -Force | Out-Null
    foreach ($taskFile in (Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.py' -File)) {
        Copy-Item -LiteralPath $taskFile.FullName -Destination (Join-Path $taskSnapshot 'tools')
    }
    Copy-Item -LiteralPath (Resolve-Path -LiteralPath $App).Path -Destination (Join-Path $taskSnapshot 'encoder.exe')
    Copy-Item -LiteralPath (Resolve-Path -LiteralPath $Decoder).Path -Destination (Join-Path $taskSnapshot 'decoder.exe')
    Copy-Item -LiteralPath (Resolve-Path -LiteralPath $FFmpeg).Path -Destination (Join-Path $taskSnapshot 'ffmpeg.exe')
    Copy-Item -LiteralPath (Resolve-Path -LiteralPath $VmafModel).Path -Destination (Join-Path $taskSnapshot 'vmaf-model.json')
}
foreach ($taskRequired in @('encoder.exe','decoder.exe','ffmpeg.exe','vmaf-model.json','tools/m53_batch.py')) {
    if (-not (Test-Path -LiteralPath (Join-Path $taskSnapshot $taskRequired))) { throw 'Incomplete task snapshot; choose a fresh RunRoot.' }
}
$taskArgs = @((Join-Path $taskSnapshot 'tools/m53_batch.py'),
    '--app', (Join-Path $taskSnapshot 'encoder.exe'), '--decoder', (Join-Path $taskSnapshot 'decoder.exe'),
    '--ffmpeg', (Join-Path $taskSnapshot 'ffmpeg.exe'), '--vmaf-model', (Join-Path $taskSnapshot 'vmaf-model.json'),
    '--corpus', $taskCorpus, '--out-dir', (Join-Path $taskRun 'results'),
    '--qps', $Qps, '--jobs', [string]$Jobs, '--repeats', [string]$Repeats)
if ($Resume) { $taskArgs += '--resume' }
$taskCampaignConfig = Join-Path $taskRun 'campaign-config.json'
if (-not $Resume) {
    [ordered]@{batch_arguments=$taskArgs; jobs=$Jobs; cross_platform=[bool]$CrossPlatform} |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $taskCampaignConfig -Encoding utf8
}
if (-not (Test-Path -LiteralPath $taskCampaignConfig)) { throw 'Missing campaign configuration.' }
if ($Resume) {
    $taskStored = Get-Content -LiteralPath $taskCampaignConfig -Raw | ConvertFrom-Json
    $taskOptions = @{Corpus='--corpus'; Qps='--qps'; Jobs='--jobs'; Repeats='--repeats'}
    foreach ($taskName in $taskOptions.Keys) {
        if ($PSBoundParameters.ContainsKey($taskName)) {
            $taskIndex = [Array]::IndexOf([object[]]$taskStored.batch_arguments, $taskOptions[$taskName])
            $taskValue = if ($taskName -eq 'Corpus') { $taskCorpus } else { [string](Get-Variable -Name $taskName -ValueOnly) }
            if ($taskIndex -lt 0 -or $taskStored.batch_arguments[$taskIndex+1] -ne $taskValue) {
                throw 'Resume settings differ from the frozen campaign; choose a fresh RunRoot.'
            }
        }
    }
    if ($PSBoundParameters.ContainsKey('CrossPlatform') -and [bool]$CrossPlatform -ne $taskStored.cross_platform) {
        throw 'Resume platform settings differ from the frozen campaign.'
    }
}
$taskArgs = @((Join-Path $taskSnapshot 'tools/m53_campaign.py'), '--config', $taskCampaignConfig)
# Windows paths cannot contain double quotes. Avoid a trailing backslash before
# the closing CRT argument quote by using the equivalent directory/. spelling.
$taskQuoted = ($taskArgs | ForEach-Object {
    if ($_.Contains('"')) { throw 'An argument contains a double quote.' }
    $taskArgument = if ($_.EndsWith('\')) { $_ + '.' } else { $_ }
    '"' + $taskArgument + '"'
}) -join ' '
$taskStamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$taskOut = Join-Path $taskRun ($taskStamp + '.stdout.log')
$taskErr = Join-Path $taskRun ($taskStamp + '.stderr.log')
$taskProcess = Start-Process -FilePath $taskPython -ArgumentList $taskQuoted -WorkingDirectory $taskRoot `
    -WindowStyle Hidden -RedirectStandardOutput $taskOut -RedirectStandardError $taskErr -PassThru
$taskRecord = [ordered]@{ pid=$taskProcess.Id; started=(Get-Date).ToString('o'); corpus=$taskCorpus;
    results=(Join-Path $taskRun 'results'); stdout=$taskOut; stderr=$taskErr; arguments=$taskArgs }
$taskRecord | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskRun 'launch.json') -Encoding utf8
$taskRecord | ConvertTo-Json -Depth 5
