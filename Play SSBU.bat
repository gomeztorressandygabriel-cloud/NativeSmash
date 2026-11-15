@echo off
rem NativeSmash: the desktop shortcut "NativeSmash" made before 0.2 runs this (through tools\run_hidden.vbs).
rem It opens the launcher, OpenSmash.exe (play, settings, online, mods, updates); without it, tools\launcher.ps1;
rem without either, the game itself.
cd /d "%~dp0"
if not defined NATIVESMASH_HIDDEN if exist tools\run_hidden.vbs (wscript //nologo tools\run_hidden.vbs "%~f0" & exit /b 0)
if exist OpenSmash.exe (start "" OpenSmash.exe & exit /b 0)
if exist tools\launcher.ps1 (powershell -NoProfile -ExecutionPolicy Bypass -STA -File tools\launcher.ps1 & exit /b 0)
if not exist build\ssbu.exe (echo NativeSmash is not built yet: run OpenSmash.exe or tools\install_windowsmash.ps1 & pause & exit /b 1)
start "SSBU log" /min cmd /c "build\ssbu.exe dump\modules --status 30 --online > build\play.log 2>&1"
exit /b 0
