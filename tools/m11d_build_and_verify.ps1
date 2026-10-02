# SPDX-License-Identifier: MIT
param([switch]$SkipPackage)
$ErrorActionPreference = 'Stop'
$Repo = Split-Path -Parent $PSScriptRoot
$Proj = Join-Path $Repo 'SpikeElite.uproject'
$Engine = 'D:\Epic\UE_5.8\Engine'
$DotNet = Join-Path $Engine 'Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe'
$UBT = Join-Path $Engine 'Binaries\DotNet\UnrealBuildTool\UnrealBuildTool.dll'
$Editor = Join-Path $Engine 'Binaries\Win64\UnrealEditor.exe'
$RunUAT = Join-Path $Engine 'Build\BatchFiles\RunUAT.bat'
$LogDir = Join-Path $Repo ('Saved\Logs\M11d_verify_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
New-Item -ItemType Directory -Path $LogDir | Out-Null
Set-Location $Repo

function Assert-Log([string]$Path, [string]$Required) {
    if (!(Test-Path -LiteralPath $Path)) { throw "Missing log: $Path" }
    $Content = Get-Content -LiteralPath $Path -Raw -Encoding utf8
    if ($Content -notmatch $Required) { throw "Completion marker missing: $Required in $Path" }
    if ($Content -match '(?im)Fatal error:|Ensure condition failed:|Missing Package|DEV VERIFY:.*-> FAIL|DevVerifyFailures=[1-9]|DEV SHOT SUITE: timeout|Test Completed\. Result=\{Fail') {
        throw "Failure in $Path"
    }
    return $Content
}

function Invoke-Game([string]$Exe, [string]$Name, [string[]]$Flags, [int]$Timeout = 180) {
    $LogPath = Join-Path $LogDir ($Name + '.log')
    $Args = @()
    if ($Exe -eq $Editor) { $Args += $Proj }
    $Args += $Flags
    $Args += @('-unattended', '-nopause', '-nosplash', "-ABSLOG=$LogPath")
    $Quoted = ($Args | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $Proc = Start-Process -FilePath $Exe -ArgumentList $Quoted -PassThru -WindowStyle Hidden
    if (!$Proc.WaitForExit($Timeout * 1000)) {
        Stop-Process -Id $Proc.Id -ErrorAction SilentlyContinue
        throw "$Name timed out after ${Timeout}s; log: $LogPath"
    }
    $Proc.Refresh()
    if ($Proc.ExitCode -ne 0) { throw "$Name exit code $($Proc.ExitCode); log: $LogPath" }
    return $LogPath
}

foreach ($Target in @('SpikeEliteEditor', 'SpikeElite')) {
    $LogPath = Join-Path $LogDir ($Target + '_build.log')
    & $DotNet $UBT $Target Win64 Development "-Project=$Proj" -WaitMutex -NoUBA "-log=$LogPath"
    if ($LASTEXITCODE -ne 0) { throw "$Target build failed: $LASTEXITCODE" }
    $null = Assert-Log $LogPath 'Result: Succeeded'
}

$Path = Invoke-Game $Editor 'automation' @('-NullRHI', '-ExecCmds=Automation RunTests SpikeElite.Tests; Quit')
$Text = Assert-Log $Path 'Test Completed\. Result=\{Success\}'
$Count = [regex]::Matches($Text, 'Test Completed\. Result=\{Success\}').Count
if ($Count -lt 57) { throw "Expected at least 57 successful tests, got $Count" }
Write-Host "PASS Automation: $Count tests"

$Common = @('-game', '-windowed', '-ResX=1280', '-ResY=720', '-QuickMatch', '-devauto', '-FastFlow')
# DevAutoStart checks ShotSuite BEFORE TacticalTest: never combine the flags.
$Path = Invoke-Game $Editor 'tactical' ($Common + @('-SEED=1', '-TacticalTest'))
$null = Assert-Log $Path 'DEV TACTICAL TEST: PASS'
$Path = Invoke-Game $Editor 'shotsuite' ($Common + @('-SEED=1', '-ShotSuite')) 450
$null = Assert-Log $Path 'DEV SHOT SUITE: all \d+ shots captured, quitting'
foreach ($Seed in @(1,42,4242)) {
    $Path = Invoke-Game $Editor "seed$Seed" ($Common + @("-SEED=$Seed"))
    $Text = Assert-Log $Path 'DEV QUICK MATCH: quitting \(DevVerifyFailures=0\)'
    if ($Text -notmatch 'Rematch requested') { throw "Seed $Seed never rematched" }
}

if (!$SkipPackage) {
    # Unique output prevents an executable from an older package counting as success.
    $Archive = Join-Path $Repo ('Dist\verify_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
    & $RunUAT BuildCookRun "-project=$Proj" -noP4 -platform=Win64 -clientconfig=Development -cook -allmaps -build -stage -pak -archive "-archivedirectory=$Archive"
    if ($LASTEXITCODE -ne 0) { throw "BuildCookRun failed: $LASTEXITCODE" }
    $Exe = Join-Path $Archive 'Windows\SpikeElite.exe'
    if (!(Test-Path -LiteralPath $Exe)) { throw "New package missing: $Exe" }
    # Rendered startup is mandatory. NullRHI is a separate logic diagnostic.
    $Path = Invoke-Game $Exe 'packaged_seed42' ($Common + @('-SEED=42'))
    $Text = Assert-Log $Path 'DEV QUICK MATCH: quitting \(DevVerifyFailures=0\)'
    if ($Text -notmatch 'Game Engine Initialized' -or $Text -notmatch 'Rematch requested') {
        throw 'Packaged rendering/match lifecycle did not complete'
    }
    Write-Host "PASS rendered package: $Exe"
}
Write-Host "PASS requested verification steps. Logs: $LogDir"
