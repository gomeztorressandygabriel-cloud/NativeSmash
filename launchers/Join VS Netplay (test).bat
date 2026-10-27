@echo off
setlocal
set "SSBU_DATA_ROOT="
set "SSBU_INPUT="
set "SSBU_NET_HOST="
set "SSBU_NET_RELAY="
set "SSBU_NET_BIND="
set "SSBU_NET_ROOM="
set "SSBU_NET_NAME="
set "SSBU_LOCKSTEP="
set "SSBU_RB_TEST="
set "SSBU_RB_PROBE="
rem EXPERIMENTAL: VS netplay with little delay (no rollback yet). Both players see the same local VS:
rem the host is player 1, you player 2. See docs\NETPLAY.md.
cd /d "%~dp0.."
call tools\pick_version.bat || (pause & exit /b 1)
echo You need the host's same game version, built from the same branch, and the same unlocked fighters.
set "SSBU_NETPLAY=join"
set "SSBU_NETPLAY_PEER="
set /p "SSBU_NETPLAY_PEER=The host's address (like 10.147.17.5 or 10.147.17.5:7810): "
if "%SSBU_NETPLAY_PEER%"=="" (echo No address given. & pause & exit /b 1)
powershell -NoProfile -ExecutionPolicy Bypass -File "%SSBU_TREE%\tools\keep_logs.ps1" "%SSBU_TREE%\build"
start "SSBU VS netplay join log" /min cmd /c "%SSBU_TREE%\build\ssbu.exe %SSBU_TREE%\dump\modules --status 30 > %SSBU_TREE%\build\play.log 2>&1 & powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File tools\send_report.ps1 %SSBU_TREE%"
echo The game waits at its first screen until it reaches the host's game (up to 5 minutes).
echo Your controller is player 2.
echo.
pause
