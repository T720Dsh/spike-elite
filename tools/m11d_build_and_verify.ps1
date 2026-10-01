# SPDX-License-Identifier: MIT
# M11d — 编译 + 自动化测试 + 冒烟 + 打包 一键脚本
# 用法: powershell -ExecutionPolicy Bypass -File tools\m11d_build_and_verify.ps1
# 说明: 每步输出 PASS/FAIL 与判据；任一步 FAIL 时停止，避免堆叠。

$ErrorActionPreference = "Continue"
$Proj   = "D:\projects\spike-elite\SpikeElite.uproject"
$UE     = "D:\Epic\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe"
$UBT    = "D:\Epic\UE_5.8\Engine\Binaries\DotNet\UnrealBuildTool\UnrealBuildTool.dll"
$Editor = "D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$RunUAT = "D:\Epic\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat"
$LogDir = "D:\projects\spike-elite\Saved\Logs"
Set-Location "D:\projects\spike-elite"

function Check-Log([string]$Log, [string[]]$BadPatterns, [string]$Step) {
    if (-not (Test-Path $Log)) { Write-Host "FAIL [$Step]: log missing $Log"; exit 1 }
    $text = Get-Content -Raw -Encoding utf8 $Log
    foreach ($p in $BadPatterns) {
        if ($text -match $p) { Write-Host "FAIL [$Step]: found '$p' in $Log"; exit 1 }
    }
    Write-Host "PASS [$Step]: $Log clean"
}

# 1. Editor build --------------------------------------------------------------
Write-Host "=== 1. Editor build ==="
& $UE $UBT SpikeEliteEditor Win64 Development -Project=$Proj -WaitMutex -log="$LogDir\ubt_m11d_editor.log"
Check-Log "$LogDir\ubt_m11d_editor.log" @("error C", "error LNK", "Error:") "Editor build"

# 2. Game build ----------------------------------------------------------------
Write-Host "=== 2. Game build (no Unreal5_6 include-order warning) ==="
& $UE $UBT SpikeElite Win64 Development -Project=$Proj -WaitMutex -log="$LogDir\ubt_m11d_game.log"
Check-Log "$LogDir\ubt_m11d_game.log" @("error C", "error LNK", "Error:", "Unreal5_6") "Game build"

# 3. Automation tests (55/55) --------------------------------------------------
Write-Host "=== 3. Automation tests SpikeElite.Tests ==="
& $Editor $Proj -ExecCmds="Automation RunTests SpikeElite.Tests; Quit" -unattended -nopause -nosplash -log -abslog="$LogDir\Automation_M11d.log"
$test = Get-Content -Raw -Encoding utf8 "$LogDir\Automation_M11d.log"
$succ = ([regex]::Matches($test, "Test Completed\. Result=\{Success\}")).Count
$fail = ([regex]::Matches($test, "Test Completed\. Result=\{Fail")).Count
Write-Host "Success=$succ Fail=$fail"
if ($succ -lt 55 -or $fail -gt 0) { Write-Host "FAIL [Automation]: expected 55/55"; exit 1 }
Write-Host "PASS [Automation]: $succ/$succ 0 failed"

# 4. TacticalTest --------------------------------------------------------------
Write-Host "=== 4. TacticalTest ==="
& $Editor $Proj -game -windowed -ResX=1280 -ResY=720 -QuickMatch -ShotSuite -devauto -FastFlow -SEED=1 -TacticalTest -unattended -nopause -nosplash -log
Copy-Item "$LogDir\SpikeElite.log" "$LogDir\M11d_tactical.log" -Force
$tac = Get-Content -Raw -Encoding utf8 "$LogDir\M11d_tactical.log"
if ($tac -notmatch "DEV TACTICAL TEST: PASS") { Write-Host "FAIL [TacticalTest]"; exit 1 }
Write-Host "PASS [TacticalTest]"

# 5. ShotSuite screenshots -----------------------------------------------------
Write-Host "=== 5. ShotSuite ==="
& $Editor $Proj -game -windowed -ResX=1280 -ResY=720 -QuickMatch -ShotSuite -devauto -FastFlow -SEED=1 -unattended -nopause -nosplash -log
Copy-Item "$LogDir\SpikeElite.log" "$LogDir\M11d_shotsuite.log" -Force
$ss = Get-Content -Raw -Encoding utf8 "$LogDir\M11d_shotsuite.log"
if ($ss -notmatch "DEV SHOT SUITE: all") { Write-Host "FAIL [ShotSuite]"; exit 1 }
Write-Host "PASS [ShotSuite]"

# 6. Seed smoke (1 / 42 / 4242) ------------------------------------------------
Write-Host "=== 6. Seed smoke ==="
foreach ($seed in @("1","42","4242")) {
    & $Editor $Proj -game -windowed -ResX=1280 -ResY=720 -QuickMatch -devauto -FastFlow -SEED=$seed -unattended -nopause -nosplash -log
    Copy-Item "$LogDir\SpikeElite.log" "$LogDir\M11d_seed${seed}.log" -Force
    $s = Get-Content -Raw -Encoding utf8 "$LogDir\M11d_seed${seed}.log"
    $quit = ([regex]::Matches($s, "DEV QUIT")).Count
    $fatal = ([regex]::Matches($s, "Fatal")).Count
    Write-Host "Seed $seed quit=$quit fatal=$fatal"
    if ($quit -lt 1 -or $fatal -gt 0) { Write-Host "FAIL [Seed $seed]"; exit 1 }
}
Write-Host "PASS [Seeds 1/42/4242]"

# 7. BuildCookRun Win64 Development --------------------------------------------
Write-Host "=== 7. BuildCookRun ==="
& $RunUAT BuildCookRun -project=$Proj -noP4 -platform=Win64 -clientconfig=Development -cook -allmaps -build -stage -pak -archive -archivedirectory="D:\projects\spike-elite\Dist"
$exe = "D:\projects\spike-elite\Dist\Windows\SpikeElite\Binaries\Win64\SpikeElite.exe"
if (-not (Test-Path $exe)) { Write-Host "FAIL [Package]: exe missing"; exit 1 }
$size = (Get-Item $exe).Length / 1MB
$dirSize = (Get-ChildItem "D:\projects\spike-elite\Dist\Windows\SpikeElite" -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
Write-Host "PASS [Package]: exe=$([math]::Round($size,1))MB total=$([math]::Round($dirSize,0))MB"

# 8. Packaged smoke (Seed 42) ---------------------------------------------------
Write-Host "=== 8. Packaged smoke Seed 42 ==="
& $exe -game -windowed -ResX=1280 -ResY=720 -QuickMatch -devauto -FastFlow -SEED=42 -unattended -nopause -nosplash -log
Copy-Item "D:\projects\spike-elite\Dist\Windows\SpikeElite\Saved\Logs\SpikeElite.log" "$LogDir\M11d_pkg_seed42.log" -Force
$p = Get-Content -Raw -Encoding utf8 "$LogDir\M11d_pkg_seed42.log"
if (($p -match "Fatal") -or ($p -match "Missing Package")) { Write-Host "FAIL [Packaged smoke]"; exit 1 }
Write-Host "PASS [Packaged smoke]"

Write-Host "`n=== ALL M11d VERIFICATION STEPS DONE ==="
