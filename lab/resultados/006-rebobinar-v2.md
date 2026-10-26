# Resultado del pedido 006: prueba de rebobinar v2 (dos repeticiones y devolucion de estado)

**Estado: ejecutado. Gran avance con resultados de comparacion en el Round 1; fallo con host exception al iniciar el replay del Round 2.**

## Resumen de los resultados de rebobinado
1. **Round 1 (1 cuadro): COMPLETO Y EXITOSO.**
   - Guardo 5.792 paginas (22,6 MB) y 23 fibras en 29,8 ms.
   - Grabo 1 cuadro. De las paginas modificadas, 784 fueron de la simulacion y 16 fuera de las guardadas.
   - **Rebobino y reprodujo dos veces** (en 3,1 ms y 2,3 ms, 2,34 ms/cuadro).
   - **Comparacion Replay 1 vs Replay 2 (determinismo de la simulacion):**
     - Diferentes en **94 de 5.792 paginas** (81 escritas por la simulacion, 13 por el resto del cuadro).
     - Primeras paginas de la simulacion: `0x604035e000`, `0x604035f000`, `0x6040360000`, `0x6040692000`, `0x604075e000`, `0x6040815000`.
   - **Comparacion Replay 1 vs Juego real (simulacion vs renderizado):**
     - Diferentes en **1.212 de 5.792 paginas** (solo 195 escritas por la simulacion, 1.017 solo por el dibujo/resto del cuadro).
   - **Devolucion de estado:** devolvio el estado al juego (12 fibras).
   - **Continuidad del juego:** tras el Round 1, el combate continuo en tiempo real de forma totalmente normal, fluida y sin artefactos (confirmado en capturas).

2. **Round 2 (10 cuadros): INICIADO, FALLO EN REPLAY.**
   - Guardo 5.791 paginas (22,6 MB) y 23 fibras en 45,9 ms.
   - Grabo 10 cuadros. Se iniciaron 4 fibras durante esos 10 cuadros.
   - Al rebobinar/restaurar fibras e iniciar el replay, ocurrio una excepcion: `host exception 0xc0000005: reading address 0x688a3a2838` en el hilo principal del juego (relacionado con rutinas de `lua2cpp_krool`).

## Codigo y compilacion
- Commit usado: `78fa5d8` ("Rollback test v2: replay twice, sort differences, give the state back").
- Compilacion: `launchers\Build NativeSmash.bat`.
  - Compilo sin errores en ~37 segundos.
  - Enlazo `build\ssbu.exe` (946 MB).

## Comando y variables usadas
- Ejecutado con PowerShell (`Start-Process`): `.\build\ssbu.exe dump/modules --status 30`
- Variables:
  - `SSBU_DATA_ROOT=C:\WindowSmash\build\testdata` (copia limpia de `save/`)
  - `SSBU_INPUT`: guion con tiempos +125 s (Bayonetta vs CPU King K. Rool)
  - `SSBU_CAPTURE=build/ab6`
  - `SSBU_RB_TEST=1`
  - `SSBU_RB_TEST_AT=200`
  - `SSBU_LATENCY=1`
  - Sin `SSBU_RB_PROBE`
  - Duracion configurada: 330 s (termino en el segundo ~214 por el crash del round 2).

## Lo visto en las capturas (paso 4)
- Se generaron 26 capturas (`frame_000.bmp` a `frame_025.bmp`) entre las 21:44:22 (+135 s) y las 21:45:39 (+212 s).
- **Antes del segundo 200:**
  - Carga, seleccion y combate transcurren con total normalidad (Bayonetta vs King K. Rool en la Liga Pokemon de Kalos).
  - `frame_019.bmp` (+194 s): combate activo, Bayonetta 40%, K. Rool 33%, trofeo ayudante Scizor en pantalla.
  - `frame_021.bmp` (+200 s): combate activo, Bayonetta 50%, K. Rool 42%, K. Rool en estado metalico. Aqui se activa la prueba.
- **Despues del Round 1 (+200 s a +212 s):**
  - `frame_023.bmp` (+206 s): tras completarse el Round 1 y restaurar el estado del juego, **el combate sigue perfectamente activo y normal**: Bayonetta 66%, K. Rool 47%, efectos de ataque giratorio en pantalla.
  - `frame_025.bmp` (+212 s): combate sigue con total fluidez, Bayonetta 83%, K. Rool 66%, animacion de impacto de Steelix.
- En el segundo ~214 (21:45:41), al ejecutar el rebobinado del Round 2, el juego se cerro por excepcion.

## Todas las lineas [rbtest]
```
[rbtest] on: rewind test rounds of 1, 10, 30 and 60 frames, the first 200 s after start
[rbtest] round 1 (1 frames): 5792 pages found written in 60 frames (22.6 MB), 5792 saved; fibers saved 23 (running, not saved: 6); in 29.8 ms
[rbtest] round 1: recorded 1 frames; pages written outside the saved ones meanwhile: 16; of the saved ones, by the simulation: 784; fibers started meanwhile: 0
[rbtest] round 1: the state now kept (fibers 23, running 6); rewinding and replaying twice
[rbtest] round 1: fibers restored 12 (running, skipped: 6; started since, dropped: 0)
[rbtest] round 1 (1 frames): replayed in 3.1 and 2.3 ms (2.34 ms a frame); pads replayed 1 (missing 0), ticks replayed 4 (missing 0); pages written outside the saved ones while replaying: 0
[rbtest] round 1 RESULT replay 1 vs replay 2 (the same frames twice from the same save): DIFFERENT in 94 of 5792 pages: 81 written by the simulation, 13 only by the rest of the frame; the simulation's first: 0x604035e000 0x604035f000 0x6040360000 0x6040692000 0x604075e000 0x6040815000
[rbtest] round 1 RESULT replay 1 vs the game (which also drew its frames): DIFFERENT in 1212 of 5792 pages: 195 written by the simulation, 1017 only by the rest of the frame; the simulation's first: 0x6040202000 0x6040280000 0x604035e000 0x604035f000 0x6040360000 0x60403c9000
[rbtest] round 1: the game's state given back (fibers 12)
[rbtest] round 2 (10 frames): 5791 pages found written in 60 frames (22.6 MB), 5791 saved; fibers saved 23 (running, not saved: 6); in 45.9 ms
[rbtest] round 2: recorded 10 frames; pages written outside the saved ones meanwhile: 116; of the saved ones, by the simulation: 990; fibers started meanwhile: 4
[rbtest] round 2: the state now kept (fibers 23, running 6); rewinding and replaying twice
[rbtest] round 2: fibers restored 12 (running, skipped: 6; started since, dropped: 0)
```

## Lineas [latency] 5.0 s: (desde ~180 s en adelante)
```
[latency] 5.0 s: vsync ticks 293 (58.6/s); ticks with no new frame 86 (17.2/s); frames queued 206 (41.2/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 7 (1.4/s);
[latency] 5.0 s: vsync ticks 297 (59.4/s); ticks with no new frame 3 (0.6/s); frames queued 294 (58.8/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 3 (0.6/s);
[latency] 5.0 s: vsync ticks 299 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 299 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 267 (53.1/s); ticks with no new frame 3 (0.6/s); frames queued 266 (52.9/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 9 (1.8/s);
[latency] 5.0 s: vsync ticks 271 (54.1/s); ticks with no new frame 19 (3.8/s); frames queued 251 (50.1/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 9 (1.8/s);
[latency] 5.0 s: vsync ticks 286 (57.1/s); ticks with no new frame 3 (0.6/s); frames queued 283 (56.5/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 13 (2.6/s);
[latency] 5.0 s: vsync ticks 292 (58.4/s); ticks with no new frame 2 (0.4/s); frames queued 290 (58.0/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 7 (1.4/s);
[latency] 5.0 s: vsync ticks 294 (58.8/s); ticks with no new frame 0 (0.0/s); frames queued 294 (58.8/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 6 (1.2/s);
[latency] 5.0 s: vsync ticks 291 (58.2/s); ticks with no new frame 3 (0.6/s); frames queued 288 (57.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 6 (1.2/s);
[latency] 5.0 s: vsync ticks 295 (58.9/s); ticks with no new frame 2 (0.4/s); frames queued 293 (58.5/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 8 (1.6/s);
[latency] 5.0 s: vsync ticks 267 (53.4/s); ticks with no new frame 0 (0.0/s); frames queued 267 (53.4/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 6 (1.2/s);
[latency] 5.0 s: vsync ticks 299 (59.3/s); ticks with no new frame 0 (0.0/s); frames queued 298 (59.1/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 299 (59.6/s); ticks with no new frame 9 (1.8/s); frames queued 291 (58.0/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 3 (0.6/s);
[latency] 5.0 s: vsync ticks 298 (59.5/s); ticks with no new frame 0 (0.0/s); frames queued 298 (59.5/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
```

## Excepcion y backtrace en build\ab6.err
```
*** host exception 0xc0000005 during nnMain: reading address 0x688a3a2838
host pc 0x7ff691f3b089: ssbu.exe +0x944b089 (thread 12868, guest main thread)
  host frame ssbu.exe +0x944ac5a
  host frame ssbu.exe +0x944a3c2
  host frame ssbu.exe +0x9499e5f
  host frame ssbu.exe +0x944fe75
  host frame ssbu.exe +0x944f591
  host frame ssbu.exe +0x944f430
  host frame ssbu.exe +0x943a6a7
  host frame ssbu.exe +0x943a319
  host frame ssbu.exe +0x94394c0
  host frame ssbu.exe +0x9438ce7
  host frame ssbu.exe +0x943b971
  host frame ssbu.exe +0x8e29fb5
  host frame ssbu.exe +0x94aa235
  host frame ssbu.exe +0x94c8e7e
  host frame ssbu.exe +0x94c66ee
  host frame ssbu.exe +0x8e1eac4
  host frame ssbu.exe +0xdc865b
  host frame ssbu.exe +0x13be7d5
  host frame ssbu.exe +0xdafe94
  host frame ssbu.exe +0x553171c
  host frame ssbu.exe +0x1d579283
  host frame ssbu.exe +0x1d578ed2
  host frame ssbu.exe +0xe9ea2b4
  host frame ssbu.exe +0x8781354
shader compiler threads at the time:
  thread 10536: vs_55be925cff1ee95f - the driver compiling its DXBC, for 0.8 s; HLSL and DXBC saved
  thread 32248: cs_4306dea49bc92620 - fxc compiling its HLSL, for 1.9 s; HLSL saved
  (programs in dump/modules/../shadercrash)
guest backtrace (innermost first):
  main+0x39327c0
  main+0x3931b20
  main+0x39318d0
  main+0x395c0b0
  main+0x3933650
  main+0x3933530
  main+0x3933410
  main+0x392cef0
  main+0x392ccb0
  main+0x392c750
  main+0x392c5c0
  main+0x392d900
  main+0x373db70
  main+0x3961990
  main+0x396dbf0
  main+0x396cbf0
  main+0x37384d0
  main+0x4d8c70
  main+0x6ea040
  main+0x4dc290
```

## Archivo de crash generado
- Archivo: `reports\crash_20261025_214541.txt` (minidump: `reports\crash_20261025_214541.dmp`).
- Lineas filtradas (main+, sdk+, lua2cpp, exception o fault; sin rutas personales):
```
*** host exception 0xc0000005 during nnMain: reading address 0x688a3a2838
  main+0x39327c0
  main+0x3931b20
  main+0x39318d0
  main+0x395c0b0
  main+0x3933650
  main+0x3933530
  main+0x3933410
  main+0x392cef0
  main+0x392ccb0
  main+0x392c750
  main+0x392c5c0
  main+0x392d900
  main+0x373db70
  main+0x3961990
  main+0x396dbf0
  main+0x396cbf0
  main+0x37384d0
  main+0x4d8c70
  main+0x6ea040
  main+0x4dc290
  main+0x22b3f20
  lua2cpp_krool+0x10fbf0
  lua2cpp_krool+0xf5050
  lua2cpp_common+0x52e2f0
  main+0x353d1b0
  main+0x37cc0e0
  sdk+0x337f14
  sdk+0x32f0d0
```
