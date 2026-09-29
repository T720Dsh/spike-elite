@echo off
REM ============================================================
REM  SPIKE ELITE - one-click launcher (standalone game, windowed)
REM  Double-click this file to play. No Unreal Editor needed.
REM ============================================================
cd /d "%~dp0"

set "UE=D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
set "PROJ=%~dp0SpikeElite.uproject"

if not exist "%UE%" (
  echo [ERROR] Unreal Engine 5.8 not found at %UE%
  pause
  exit /b 1
)

echo Starting SPIKE ELITE ...
"%UE%" "%PROJ%" /Engine/Maps/Templates/OpenWorld -game -windowed -ResX=1600 -ResY=900
