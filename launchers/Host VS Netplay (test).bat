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
rem you are player 1, your friend player 2. See docs\NETPLAY.md.
cd /d "%~dp0.."
call tools\pick_version.bat || (pause & exit /b 1)
echo Your friend needs this same game version, built from the same branch, and the same unlocked fighters.
set "SSBU_NETPLAY=host"
set "SSBU_NETPLAY_PORT=7810"
set /p "SSBU_NETPLAY_PORT=UDP port [7810]: "
set "SSBU_NETPLAY_DELAY=2"
set /p "SSBU_NETPLAY_DELAY=Frames of delay, 0-8 (more if it stutters) [2]: "
echo.
echo Give your friend one of these addresses (your VPN's - ZeroTier, Tailscale, Radmin - or your public one,
echo with UDP port %SSBU_NETPLAY_PORT% forwarded on your router):
ipconfig | findstr /c:"IPv4"
echo.
powershell -NoProfile -ExecutionPolicy Bypass -File "%SSBU_TREE%\tools\keep_logs.ps1" "%SSBU_TREE%\build"
start "SSBU VS netplay host log" /min cmd /c "%SSBU_TREE%\build\ssbu.exe %SSBU_TREE%\dump\modules --status 30 > %SSBU_TREE%\build\play.log 2>&1"
echo The game waits at its first screen until your friend's game joins (up to 5 minutes).
echo Then go to Smash ^(VS^) together: both games show the same menus.
echo.
pause
