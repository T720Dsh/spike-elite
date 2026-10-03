@echo off
REM ============================================================
REM  SPIKE ELITE - launcher
REM  Prefer the verified M11h Codex gameplay build; retain older fallbacks.
REM  --editor opts into Unreal Editor game mode (developer).
REM ============================================================
cd /d "%~dp0"

set "PACKAGED=%~dp0Dist\M11h-Codex\Windows\SpikeElite.exe"
if not exist "%PACKAGED%" set "PACKAGED=%~dp0Dist\M11h\Windows\SpikeElite.exe"
if not exist "%PACKAGED%" set "PACKAGED=%~dp0Dist\M11g\Windows\SpikeElite.exe"
if not exist "%PACKAGED%" set "PACKAGED=%~dp0Dist\Windows\SpikeElite.exe"
set "UE=D:\Epic\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
set "PROJ=%~dp0SpikeElite.uproject"

if /i "%~1"=="--editor" goto editor

:packaged
if exist "%PACKAGED%" (
  echo Starting packaged SPIKE ELITE ...
  "%PACKAGED%"
  exit /b 0
)

echo [INFO] No packaged build found at:
echo        %PACKAGED%
echo.
echo To create it once, open a terminal in this folder and run:
echo   "D:\Epic\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%~dp0SpikeElite.uproject" -noP4 -platform=Win64 -clientconfig=Development -cook -allmaps -build -stage -pak -archive -archivedirectory="%~dp0Dist"
echo.
echo Falling back to Unreal Editor game mode (editor required) ...
:editor
if not exist "%UE%" (
  echo [ERROR] Unreal Engine 5.8 not found at %UE%
  pause
  exit /b 1
)
echo Starting SPIKE ELITE via Unreal Editor (developer mode) ...
"%UE%" "%PROJ%" -game -windowed -ResX=1600 -ResY=900
