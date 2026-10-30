@echo off
rem Compiles the game's shaders ahead of time (tools\shader_aot.py) for the version you pick (the
rem launcher's Prepare button passes the one picked on its Game page as SSBU_TREE): an effect you
rem have not seen yet then shows at once the first time, instead of a moment later.
rem It runs once per game version, from minutes to about two hours, at low priority (the PC stays
rem usable). It can be stopped (close this window) and started again: what is done stays done.
rem Needs Python and the build tools, as for building (docs\BUILDING.md). Everything it prints also
rem goes to reports\prepare_shaders.log.
cd /d "%~dp0.."
setlocal
echo Preparando los shaders del juego / Preparing the game's shaders.
echo Puede tardar de minutos a un par de horas. Puedes seguir usando el PC, o cerrar esta ventana
echo y seguir otro dia: lo hecho queda hecho.
echo.
call tools\find_python.bat || goto failed
call tools\pick_version.bat || goto failed
if not exist reports mkdir reports
set "SSBU_AOT_LOG=reports\prepare_shaders.log"
%PY% tools\shader_aot.py --tree "%SSBU_TREE%"
findstr /b /c:"done in" reports\prepare_shaders.log >nul || goto failed
echo.
echo Listo: los shaders estan preparados. / Done: the shaders are ready.
pause
exit /b 0
:failed
echo.
echo Algo fallo. Haz una captura de esta ventana y mandala al grupo, junto con el archivo
echo reports\prepare_shaders.log de tu carpeta de NativeSmash.
echo Something went wrong: send a screenshot of this window and reports\prepare_shaders.log.
pause
exit /b 1
