# Resultado del pedido 010: repeticion del pedido 009 (prueba de rebobinar v5)

**Estado: la ronda 1 se ejecuto y completo las dos repeticiones exitosamente (discrepancia en solo 4 paginas de la simulacion de 5775 comparadas). Se devolvio el estado al juego y se vigilaron las escrituras en el siguiente cuadro. Justo despues, el juego se cerro por una excepcion en el hilo 37 (acceso a 0x4), mientras los hilos 28 y 29 escribian en una de las paginas vigiladas.**

## Hallazgos principales
1. **Ronda 1 completada:**
   - La fase `DISCOVER` (120 cuadros) encontro 5775 paginas escritas (22.6 MB), las cuales se guardaron en 21.5 ms junto con 13 fibras.
   - Se grabo 1 cuadro y se realizaron las 2 repeticiones: cada repeticion tardo **0.1 a 0.4 ms** (0.38 ms por cuadro).
   - **Replay 1 vs Replay 2:** difirieron en **solo 4 paginas escritas por la simulacion** (y 37 del resto del cuadro) de 5775 paginas:
     - `0x6040476000`
     - `0x6062601000`
     - `0x6062602000`
     - `0x6075a01000`
   - El estado se devolvio al juego (`the game's state given back (fibers 2)`).
2. **Vigilancia del siguiente cuadro normal del juego (`the game's next frame`):**
   - Paginas escritas por **nadie**: `0x6040476000`, `0x6062601000`, `0x6062602000`.
   - Pagina `0x6075a01000`:
     - Escrita 18 veces o mas por el **hilo 3712 (guest thread 28)** desde `host pc ssbu.exe+0x9e922c9` (traza: `sdk+0xfeb0 -> sdk+0x33600 -> subsdk0+0x158130 ... -> sdk+0x334450 -> sdk+0x338930`).
     - Escrita 36 veces o mas por el **hilo 34752 (guest thread 29)** desde `host pc ssbu.exe+0x99381ee` (traza: `subsdk0+0x156fa0 -> subsdk0+0x2255c0 -> sdk+0x334450 -> sdk+0x338930`).
3. **Cierre por excepcion en el hilo 37:**
   - Durante la transicion al siguiente cuadro, el **hilo 3676 (guest thread 37, prioridad 44)** tuvo un fallo `0xc0000005` al intentar leer la direccion `0x4` (puntero nulo / offset 4):
     - `host pc ssbu.exe +0x9e922a8` (misma funcion en la que el hilo 28 escribe a `+0x9e922c9`).
     - Traza guest: `sdk+0xfeb0 -> sdk+0x333a0 -> subsdk0+0x20ae40 -> subsdk0+0x2a60 -> main+0x36f3f20 -> main+0x36f3e60 -> sdk+0x334450 -> sdk+0x338930`.

## Codigo y compilacion
- Commit usado: `6341d9e` ("Rollback test: log when a round starts finding pages and saving").
- Compilacion con `launchers\Build NativeSmash.bat`:
  - Compilo exitosamente sin errores en 27 segundos de enlace.
  - Genero `build\ssbu.exe` (946 MB).

## Comando y variables usadas
- Ejecutado con PowerShell (`Start-Process`): `.\build\ssbu.exe dump/modules --status 30`
- Variables de entorno:
  - `SSBU_DATA_ROOT=C:\WindowSmash\build\testdata` (copia limpia de `save/`)
  - `SSBU_INPUT=125.0:p1=a 125.1:p1= 128.0:p1=a 128.1:p1= 131.0:p1=a 131.1:p1= 134.0:p1=a 134.1:p1= 137.0:p1=a 137.1:p1= 140.0:p1=a 140.1:p1= 143.0:p1=a 143.1:p1= 148.0:p1=a 148.1:p1= 153.0:p1=dpad_down 153.1:p1= 155.0:p1=a 155.1:p1=` (tiempos +125 s)
  - `SSBU_CAPTURE=build/ab10`
  - `SSBU_RB_TEST=1`
  - `SSBU_RB_TEST_AT=200`
  - `SSBU_LATENCY=1`
  - Sin `SSBU_RB_PROBE`
  - Duracion configurada: 330 segundos (finalizo a los ~205 s por la excepcion en el hilo 37).

## Lo visto en las capturas (paso 4)
- Se generaron 23 capturas (`frame_000.bmp` a `frame_022.bmp`) en `build/ab10/`:
  - `frame_000.bmp` a `frame_007.bmp`: menus de inicio, seleccion de Bayonetta vs CPU Corrin y escenario Pueblo Smash.
  - `frame_008.bmp` a `frame_010.bmp`: pantallas de carga.
  - `frame_011.bmp` a `frame_022.bmp`: combate activo a 60 fps en tiempo real.
- Del segundo ~195 al final (`frame_018.bmp` a `frame_022.bmp`):
  - El combate transcurrio de forma totalmente normal y activa.
  - `frame_018.bmp` (~195 s): ambos personajes en la plataforma central intercambiando ataques.
  - `frame_019.bmp` (~198 s) y `frame_020.bmp` (~201 s): animaciones de golpe, movimiento fluido.
  - `frame_021.bmp` (~203 s) y `frame_022.bmp` (~204 s): combate activo, efectos graficos de ataque justo en el momento en que se devolvio el estado tras la ronda 1 y antes de cerrarse por la excepcion en el hilo 37.

## Todas las lineas [rbtest] y trazas asociadas
```
[rbtest] on: rewind test rounds of 1 1 1 5 10 frames, the first 200 s after start
[rbtest] round 1: finding the pages the game writes (120 frames)
[rbtest] round 1: saving
[rbtest] round 1 (1 frames): 5775 pages found written in 120 frames (22.6 MB), 5775 saved; fibers saved 13 (running, not saved: 6); in 21.5 ms
[rbtest] round 1: recorded 1 frames; pages written outside the saved ones meanwhile: 2; of the saved ones, by the simulation: 49; fibers started meanwhile: 0
[rbtest] round 1: the state now kept (fibers 13, running 6); rewinding and replaying twice
[rbtest] round 1, replay 1: fibers restored 2 (running, skipped: 6; started since, dropped: 0); of 0 suspect pages, not as saved after the put-back: 0; changed before the replay: 0, during it: 0, after it: 0; pages left out (other threads write them): 0, 0 of them written by the replaying thread too
[rbtest] round 1, replay 2: fibers restored 2 (running, skipped: 6; started since, dropped: 0); of 0 suspect pages, not as saved after the put-back: 0; changed before the replay: 0, during it: 0, after it: 0; pages left out (other threads write them): 0, 0 of them written by the replaying thread too
[rbtest] round 1 (1 frames): replayed in 0.1 and 0.4 ms (0.38 ms a frame); pads replayed 1 (missing 0), ticks replayed 8 (missing 0); pages written outside the saved ones while replaying: 0
[rbtest] round 1 RESULT replay 1 vs replay 2 (the same frames twice from the same save): DIFFERENT in 41 of 5775 pages: 4 written by the simulation, 37 only by the rest of the frame; the simulation's first: 0x6040476000 0x6062601000 0x6062602000 0x6075a01000
[rbtest] round 1 RESULT replay 1 vs the game (which also drew its frames): DIFFERENT in 712 of 5775 pages: 20 written by the simulation, 692 only by the rest of the frame; the simulation's first: 0x6040476000 0x6040711000 0x6060cc7000 0x60623a4000 0x6062601000 0x6062602000
[rbtest] round 1: page 0x6040476000 differs in +0xe30: 0x6089409f80 / 0x6089409f90 +0xe70: 0x6089409f50 / 0x6089409f80 +0xf40: 0xffffffff / 0 +0xf80: 0x696e3332 / 0 (10 words of 512)
[rbtest] round 1: page 0x6062601000 differs in +0x1d0: 0x9e00000020 / 0x9e0000007d +0x1f0: 0x1700000016 / 0x1200000011 +0x208: 0x1f0000001d / 0x1e0000001c +0x238: 0x300000002 / 0x200000001 (9 words of 512)
[rbtest] round 1: page 0x6062602000 differs in +0xdd0: 0x9e0000004f / 0x9e0000009c +0xdf0: 0x5d00000056 / 0x5f00000056 +0xe08: 0x700000001 / 0x800000001 +0xe50: 0x4e0000004e / 0x4f0000004e (7 words of 512)
[rbtest] round 1: page 0x6075a01000 differs in +0x780: 0x100000001 / 0 +0xa00: 0x100000001 / 0 +0xa08: 0x60774f1eb0 / 0x60556b0140 +0xe00: 0 / 0x36aca45aca (6 words of 512)
[rbtest] round 1: the game's state given back (fibers 2)
[rbtest] the game's next frame: page 0x6075a01000 +0x1c4 first written by thread 3712 (guest thread 28), host pc ssbu.exe+0x9e922c9
guest backtrace (innermost first):
  sdk+0xfeb0
  sdk+0x33600
  subsdk0+0x158130
  subsdk0+0x1580b0
  subsdk0+0x159af0
  subsdk0+0x1e3320
  subsdk0+0x1df290
  subsdk0+0x158250
  subsdk0+0x156fa0
  subsdk0+0x2255c0
  sdk+0x334450
  sdk+0x338930
[rbtest] the game's next frame: page 0x6075a01000 +0x1f8 first written by thread 34752 (guest thread 29), host pc ssbu.exe+0x99381ee
guest backtrace (innermost first):
  subsdk0+0x156fa0
  subsdk0+0x2255c0
  sdk+0x334450
  sdk+0x338930
[rbtest] the game's next frame: page 0x6040476000 written by nobody
[rbtest] the game's next frame: page 0x6062601000 written by nobody
[rbtest] the game's next frame: page 0x6062602000 written by nobody
[rbtest] the game's next frame: page 0x6075a01000 written by thread 3712 (guest 28) 18 times or more; thread 34752 (guest 29) 36 times or more;
```

## Lineas [latency] 5.0 s desde el segundo ~180 en adelante
```
[latency] 5.0 s: vsync ticks 298 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 298 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 299 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 299 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 298 (59.5/s); ticks with no new frame 0 (0.0/s); frames queued 298 (59.5/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 298 (59.1/s); ticks with no new frame 0 (0.0/s); frames queued 298 (59.1/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 1 (0.2/s);
[latency] 5.0 s: vsync ticks 299 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 299 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 3 (0.6/s);
```

## Excepciones, detenciones y trazas (host exception y siguientes 30 lineas)
```
*** host exception 0xc0000005 during nnMain: reading address 0x4
host pc 0x7ff7328b22a8: ssbu.exe +0x9e922a8 (thread 3676, guest thread 37 (sdk+0x338930, priority 44))
shader compiler threads at the time:
  thread 3064: vs_83b813554a7815f5 - the driver compiling its DXBC, for 0.2 s; DXBC saved
[rbtest] round 1, replay 1: fibers restored 2 (running, skipped: 6; started since, dropped: 0); of 0 suspect pages, not as saved after the put-back: 0; changed before the replay: 0, during it: 0, after it: 0; pages left out (other threads write them): 0, 0 of them written by the replaying thread too
  (programs in dump/modules/../shadercrash)
guest backtrace (innermost first):
  sdk+0xfeb0
  sdk+0x333a0
  subsdk0+0x20ae40
  subsdk0+0x2a60
  main+0x36f3f20
  main+0x36f3e60
  sdk+0x334450
  sdk+0x338930
[rbtest] round 1, replay 2: fibers restored 2 (running, skipped: 6; started since, dropped: 0); of 0 suspect pages, not as saved after the put-back: 0; changed before the replay: 0, during it: 0, after it: 0; pages left out (other threads write them): 0, 0 of them written by the replaying thread too
[rbtest] round 1 (1 frames): replayed in 0.1 and 0.4 ms (0.38 ms a frame); pads replayed 1 (missing 0), ticks replayed 8 (missing 0); pages written outside the saved ones while replaying: 0
[rbtest] round 1 RESULT replay 1 vs replay 2 (the same frames twice from the same save): DIFFERENT in 41 of 5775 pages: 4 written by the simulation, 37 only by the rest of the frame; the simulation's first: 0x6040476000 0x6062601000 0x6062602000 0x6075a01000
[rbtest] round 1 RESULT replay 1 vs the game (which also drew its frames): DIFFERENT in 712 of 5775 pages: 20 written by the simulation, 692 only by the rest of the frame; the simulation's first: 0x6040476000 0x6040711000 0x6060cc7000 0x60623a4000 0x6062601000 0x6062602000
[rbtest] round 1: page 0x6040476000 differs in +0xe30: 0x6089409f80 / 0x6089409f90 +0xe70: 0x6089409f50 / 0x6089409f80 +0xf40: 0xffffffff / 0 +0xf80: 0x696e3332 / 0 (10 words of 512)
[rbtest] round 1: page 0x6062601000 differs in +0x1d0: 0x9e00000020 / 0x9e0000007d +0x1f0: 0x1700000016 / 0x1200000011 +0x208: 0x1f0000001d / 0x1e0000001c +0x238: 0x300000002 / 0x200000001 (9 words of 512)
[rbtest] round 1: page 0x6062602000 differs in +0xdd0: 0x9e0000004f / 0x9e0000009c +0xdf0: 0x5d00000056 / 0x5f00000056 +0xe08: 0x700000001 / 0x800000001 +0xe50: 0x4e0000004e / 0x4f0000004e (7 words of 512)
[rbtest] round 1: page 0x6075a01000 differs in +0x780: 0x100000001 / 0 +0xa00: 0x100000001 / 0 +0xa08: 0x60774f1eb0 / 0x60556b0140 +0xe00: 0 / 0x36aca45aca (6 words of 512)
[rbtest] round 1: the game's state given back (fibers 2)
[rbtest] the game's next frame: page 0x6075a01000 +0x1c4 first written by thread 3712 (guest thread 28), host pc ssbu.exe+0x9e922c9
guest backtrace (innermost first):
  sdk+0xfeb0
  sdk+0x33600
  subsdk0+0x158130
  subsdk0+0x1580b0
  subsdk0+0x159af0
  subsdk0+0x1e3320
  subsdk0+0x1df290
  subsdk0+0x158250
  subsdk0+0x156fa0
  subsdk0+0x2255c0
  sdk+0x334450
  sdk+0x338930
[rbtest] the game's next frame: page 0x6075a01000 +0x1f8 first written by thread 34752 (guest thread 29), host pc ssbu.exe+0x99381ee
guest backtrace (innermost first):
  subsdk0+0x156fa0
  subsdk0+0x2255c0
  sdk+0x334450
  sdk+0x338930
[rbtest] the game's next frame: page 0x6040476000 written by nobody
[rbtest] the game's next frame: page 0x6062601000 written by nobody
[rbtest] the game's next frame: page 0x6062602000 written by nobody
[rbtest] the game's next frame: page 0x6075a01000 written by thread 3712 (guest 28) 18 times or more; thread 34752 (guest 29) 36 times or more;
report: reports\crash_20261025_224412.txt; minidump: reports\crash_20261025_224412.dmp
```

## Archivo de crash generado durante la prueba
Archivo: `reports\crash_20261025_224412.txt`
Lineas filtradas con `main+`, `sdk+`, `lua2cpp`, `exception`, `fault`, `rbf_`:
```
*** host exception 0xc0000005 during nnMain: reading address 0x4
host pc 0x7ff7328b22a8: ssbu.exe +0x9e922a8 (thread 3676, guest thread 37 (sdk+0x338930, priority 44))
  sdk+0xfeb0
  sdk+0x333a0
  main+0x36f3f20
  main+0x36f3e60
  sdk+0x334450
  sdk+0x338930
  sdk+0xfeb0
  sdk+0x33600
  sdk+0x334450
  sdk+0x338930
  sdk+0x334450
  sdk+0x338930
```
