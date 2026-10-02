param(
    [string[]]$Suites = @('Automation','Art','Pose','Quick1','Quick42','Quick4242','Tactical','FiveSet','Rematch'),
    [string]$Executable = 'D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe',
    [switch]$Packaged,
    [string]$Label = 'final',
    [string]$AdditionalArguments = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$logRoot = Join-Path $projectRoot 'Saved\Logs'
$shotRoot = if ($Packaged) { Join-Path (Split-Path $Executable -Parent) 'SpikeElite\Saved\Screenshots\Windows' } else { Join-Path $projectRoot 'Saved\Screenshots\WindowsEditor' }
$common = '-game -devauto -windowed -ResX=1280 -ResY=720 -unattended -nosplash'
foreach ($suite in $Suites) {
    $extra = switch ($suite) {
        'Automation' { '-NullRHI -unattended -nopause -nosplash -ExecCmds="Automation RunTests SpikeElite.Tests; Quit"' }
        'Art' { "$common -ArtSuite -QuickMatch -FastFlow -SEED=42" }
        'Pose' { "$common -PoseSuite -QuickMatch -FastFlow -SEED=42" }
        'Quick1' { "$common -QuickMatch -FastFlow -SEED=1" }
        'Quick42' { "$common -QuickMatch -FastFlow -SEED=42" }
        'Quick4242' { "$common -QuickMatch -FastFlow -SEED=4242" }
        'Tactical' { "$common -TacticalTest -QuickMatch -FastFlow -SEED=42" }
        'FiveSet' { "$($common.Replace('-devauto ','')) -FiveSetTest -FastFlow -SEED=42" }
        'Rematch' { "$common -RematchStress -QuickMatch -FastFlow -SEED=42" }
        default { throw "Unknown suite $suite" }
    }
    if ($Packaged -and $suite -eq 'Automation') { throw 'Automation must use Editor target' }
    $log = Join-Path $logRoot "Codex_M11g_${Label}_${suite}.log"
    $argsText = if ($Packaged) { $extra.Replace('-game ', '') } else { '"' + (Join-Path $projectRoot 'SpikeElite.uproject') + '" ' + $extra }
    $argsText += ' -ABSLOG="' + $log + '"'
    if ($AdditionalArguments) { $argsText += ' ' + $AdditionalArguments }
    $began = Get-Date
    $process = Start-Process -FilePath $Executable -ArgumentList $argsText -WindowStyle Hidden -PassThru
    Write-Output "START $suite pid=$($process.Id) log=$log"
    $limit = if ($suite -eq 'Tactical') { 300 } elseif ($suite -eq 'Rematch') { 300 } else { 180 }
    $timeout = -not $process.WaitForExit($limit * 1000)
    if ($timeout) { Stop-Process -Id $process.Id; $process.WaitForExit() }
    $process.Refresh()
    $exitCode = $process.ExitCode
    $contents = Get-Content -LiteralPath $log -Raw
    $evidence = switch ($suite) {
        'Automation' { $contents -match 'TEST COMPLETE\. EXIT CODE: 0' -and $contents -notmatch 'Test Completed\. Result=\{Fail\}' }
        'Art' { $contents -match 'DEV ART SUITE: completed 6' }
        'Pose' { $contents -match 'DEV POSE SUITE: done' }
        'Tactical' { $contents -match 'DEV TACTICAL TEST: PASS \(failures=0\)' }
        'FiveSet' { $contents -match 'FIVE SET TEST: RESULT=PASS' }
        'Rematch' { $contents -match 'STRESS.*PASS|Stress.*PASS|REMATCH STRESS:.*done' }
        default { $contents -match 'quitting \(DevVerifyFailures=0\)' }
    }
    $fatal = $contents -match 'Fatal error:|Assertion failed:|Ensure condition failed:'
    $passed = -not $timeout -and $exitCode -eq 0 -and $evidence -and -not $fatal
    $shots = @(Get-ChildItem -LiteralPath $shotRoot -Filter *.png -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -ge $began } | ForEach-Object { $_.FullName })
    Write-Output "END $suite exit=$exitCode timeout=$timeout evidence=$evidence fatal=$fatal result=$passed shots=$($shots.Count)"
    if (-not $passed) { throw "$suite failed; inspect $log. No automatic retries." }
}
