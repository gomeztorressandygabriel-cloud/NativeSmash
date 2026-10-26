# Resultado del pedido 009: prueba de rebobinar v5 (dejar fuera lo que escriben otros hilos)

**Estado: la prueba arranco normalmente, entro al combate automatico y ejecuto el juego fluido a 60 fps hasta el segundo ~203, donde ocurrio un crash (host exception 0xc0000005) en el bucle principal al intentar leer memoria del area alias.**

## Hallazgos y observacion del cierre
- El juego inicio y corrio el combate normalmente (Bayonetta vs Corrin en Pueblo Smash) hasta el segundo 200+.
- Al llegar al segundo 200 (`SSBU_RB_TEST_AT=200`), la prueba inicio la primera ronda (`begin_round()`), entrando en fase `DISCOVER` (180 cuadros de rastreo de paginas escritas).
- A los ~203-206 s (justo al completarse la fase de descubrimiento / transicion al guardado y repeticion de la ronda 1), se produjo una excepcion de host en el hilo principal:
  - `*** host exception 0xc0000005 during nnMain: reading address 0x6095a9e000`
  - Direccion host: `ssbu.exe +0x321781ad` (hilo principal guest).
  - Traza guest: `main+0x3746e90 -> main+0x2c5620 -> sdk+0x29d9e0`.
  - El reporte de fallo quedo registrado en `reports\crash_20261025_222545.txt`.
- Debido a este fallo prematuro antes de completar la primera repeticion, no se alcanzaron a emitir lineas de resultado de repeticion (`RESULT`, `differs in`, etc.). Siguiendo la regla de PEDIDO.md ("puede colgarse o cerrarse; si pasa, apuntalo y no insistas mas de una vez"), se documenta el comportamiento exacto y los reportes generados.

## Codigo y compilacion
- Commit usado: `e68d5b4` ("Rollback test v5: leave out pages other threads write, watch the whole replay").
- Compilacion con `launchers\Build NativeSmash.bat`:
  - Compilo exitosamente sin errores en 35.2 segundos.
  - Genero `build\ssbu.exe` (946 MB).

## Comando y variables usadas
- Ejecutado con PowerShell (`Start-Process`): `.\build\ssbu.exe dump/modules --status 30`
- Variables de entorno:
  - `SSBU_DATA_ROOT=C:\WindowSmash\build\testdata` (copia limpia de `save/`)
  - `SSBU_INPUT=125.0:p1=a 125.1:p1= 128.0:p1=a 128.1:p1= 131.0:p1=a 131.1:p1= 134.0:p1=a 134.1:p1= 137.0:p1=a 137.1:p1= 140.0:p1=a 140.1:p1= 143.0:p1=a 143.1:p1= 148.0:p1=a 148.1:p1= 153.0:p1=dpad_down 153.1:p1= 155.0:p1=a 155.1:p1=` (tiempos +125 s)
  - `SSBU_CAPTURE=build/ab9`
  - `SSBU_RB_TEST=1`
  - `SSBU_RB_TEST_AT=200`
  - `SSBU_LATENCY=1`
  - Sin `SSBU_RB_PROBE`
  - Tiempo configurado: 330 segundos (el proceso finalizo a los ~206 s por el crash).

## Lo visto en las capturas (paso 4)
- Se generaron 22 capturas (`frame_000.bmp` a `frame_021.bmp`) en `build/ab9/`:
  - `frame_000.bmp` a `frame_007.bmp`: secuencia de navegacion de menus (pantalla de inicio Smash, seleccion Smash, seleccion de luchador Bayonetta vs CPU Corrin, pantalla de seleccion de escenario Pueblo Smash).
  - `frame_008.bmp` a `frame_010.bmp`: pantallas de carga e inicio de partida.
  - `frame_011.bmp` a `frame_021.bmp`: combate activo y en tiempo real en Pueblo Smash (Smashville).
- Del segundo ~195 al ~203 (`frame_018.bmp` a `frame_021.bmp`):
  - El combate transcurrio de forma completamente fluida y normal.
  - `frame_018.bmp` (~195 s): Bayonetta y Corrin combatiendo en la plataforma central.
  - `frame_019.bmp` (~198 s): intercambio de golpes e impactos.
  - `frame_020.bmp` (~201 s): combate activo, animaciones de ataque y porcentajes acumulados.
  - `frame_021.bmp` (~203 s): combate normal en pantalla justo antes de que el proceso se cerrara repentinamente por la excepcion de acceso de lectura a `0x6095a9e000`.

## Todas las lineas [rbtest] y trazas asociadas
```
[rbtest] on: rewind test rounds of 1 1 1 5 10 frames, the first 200 s after start
```
*(No se llegaron a emitir mas lineas [rbtest] debido a la excepcion de host ocurrida en torno al segundo 203).*

## Lineas [latency] 5.0 s desde el segundo ~180 en adelante
```
[latency] 5.0 s: vsync ticks 290 (58.0/s); ticks with no new frame 0 (0.0/s); frames queued 290 (58.0/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 6 (1.2/s);
[latency] 5.0 s: vsync ticks 295 (58.9/s); ticks with no new frame 0 (0.0/s); frames queued 295 (58.9/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 5 (1.0/s);
[latency] 5.0 s: vsync ticks 292 (58.3/s); ticks with no new frame 0 (0.0/s); frames queued 292 (58.3/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 5 (1.0/s);
[latency] 5.0 s: vsync ticks 299 (59.7/s); ticks with no new frame 0 (0.0/s); frames queued 299 (59.7/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 297 (59.3/s); ticks with no new frame 0 (0.0/s); frames queued 296 (59.1/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 4 (0.8/s);
```

## Excepciones, detenciones y trazas (host exception y siguientes 30 lineas)
```
*** host exception 0xc0000005 during nnMain: reading address 0x6095a9e000
host pc 0x7ff6fad581ad: ssbu.exe +0x321781ad (thread 34232, guest main thread)
  host frame ssbu.exe +0x85fd6
  host frame ssbu.exe +0x8e6a705
  host frame ssbu.exe +0x7ff47f
  host frame ssbu.exe +0x79230
  host frame ssbu.exe +0xa4efeee
  host frame ssbu.exe +0x75f46
  host frame KERNEL32.DLL +0x2dda7
  host frame ntdll.dll +0x7ee4c
guest backtrace (innermost first):
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
report: reports\crash_20261025_222545.txt; minidump: reports\crash_20261025_222545.dmp
```

## Archivo de crash generado durante la prueba
Archivo: `reports\crash_20261025_222545.txt`
Lineas filtradas con `main+`, `sdk+`, `lua2cpp`, `exception`, `fault`, `rbf_`:
```
*** host exception 0xc0000005 during nnMain: reading address 0x6095a9e000
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
```
