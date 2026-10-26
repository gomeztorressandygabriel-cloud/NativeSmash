# Resultado del pedido 011: prueba de rebobinar v6 (censo de quien escribe, antes de guardar)

**Estado: el censo previo de 10 cuadros se ejecuto por completo, clasifico a todos los hilos del juego y dejo fuera 3293 paginas escritas por hilos que van a su ritmo. Inmediatamente despues, la ronda 1 guardo las paginas (27.2 ms), pero el juego sufrio una excepcion en el hilo 3 (escritura en 0x60864751b8).**

## Hallazgos principales
1. **El censo de 10 cuadros funciono exitosamente:**
   - Detecto 5732 paginas activas.
   - **Hilos auxiliares de la simulacion (escriben solo durante la simulacion, 0 fuera):**
     - **Hilo 34000 (guest 12):** escribio 382 paginas en la simulacion, 0 fuera.
     - **Hilo 28932 (guest 13):** escribio 356 paginas en la simulacion, 0 fuera.
   - **Hilos que van a su propio ritmo (escriben dentro y fuera de la simulacion):**
     - Hilos 32, 37, 19, 11, 26, host (-1), 3, 28, 34, 29, 30, 35, 4, 36, 33, 38, 31, 44, 40 y 27.
   - **Total dejado fuera:** 3293 paginas (110 de ellas escritas por el hilo principal o un auxiliar tambien).
   - El hilo principal escribio 29 paginas durante la simulacion y 1297 fuera de ella.
2. **Guardado de la ronda 1:**
   - `[rbtest] round 1: saving`
   - `[rbtest] round 1 (1 frames): 5732 pages found written in 120 frames (22.4 MB), 5732 saved; fibers saved 13 (running, not saved: 6); in 27.2 ms`
3. **Cierre por excepcion en el hilo 3:**
   - Durante la transicion / ejecucion del censo o guardado, el **hilo 25772 (guest thread 3, prioridad 36)** tuvo una excepcion de host al escribir en `0x60864751b8`:
     - `*** host exception 0xc0000005 during nnMain: writing address 0x60864751b8`
     - Host pc: `ssbu.exe +0x90d29db`
     - Traza guest: `main+0x381c260 -> main+0x35abc70 -> main+0x35484b0 -> main+0x354a390 -> main+0x353d1b0 -> main+0x37cc0e0 -> sdk+0x337f14 -> sdk+0x32f0d0 -> lua2cpp_item+0xe7c70`.
     - Probable causa: la pagina `0x6086475000` estaba en modo de solo lectura por la proteccion del censo, pero el manejador `on_write` no la reconocio o no pudo desprotegerla antes de que se registrara el fallo.

## Codigo y compilacion
- Commit usado: `9f23709` ("Rollback test v6: a census of who writes the pages, before the first save").
- Compilacion con `launchers\Build NativeSmash.bat`:
  - Compilo exitosamente sin errores en 31 segundos de enlace.
  - Genero `build\ssbu.exe` (946 MB).

## Comando y variables usadas
- Ejecutado con PowerShell (`Start-Process`): `.\build\ssbu.exe dump/modules --status 30`
- Variables de entorno:
  - `SSBU_DATA_ROOT=C:\WindowSmash\build\testdata` (copia limpia de `save/`)
  - `SSBU_INPUT=125.0:p1=a 125.1:p1= 128.0:p1=a 128.1:p1= 131.0:p1=a 131.1:p1= 134.0:p1=a 134.1:p1= 137.0:p1=a 137.1:p1= 140.0:p1=a 140.1:p1= 143.0:p1=a 143.1:p1= 148.0:p1=a 148.1:p1= 153.0:p1=dpad_down 153.1:p1= 155.0:p1=a 155.1:p1=` (tiempos +125 s)
  - `SSBU_CAPTURE=build/ab11`
  - `SSBU_RB_TEST=1`
  - `SSBU_RB_TEST_AT=200`
  - `SSBU_LATENCY=1`
  - Sin `SSBU_RB_PROBE`
  - Duracion configurada: 330 segundos (el proceso finalizo a los ~205 s por la excepcion en el hilo 3).

## Lo visto en las capturas (paso 4)
- Se generaron 23 capturas (`frame_000.bmp` a `frame_022.bmp`) en `build/ab11/`:
  - `frame_000.bmp` a `frame_007.bmp`: secuencia de seleccion (Bayonetta vs CPU Corrin en Pueblo Smash).
  - `frame_008.bmp` a `frame_010.bmp`: pantallas de carga.
  - `frame_011.bmp` a `frame_022.bmp`: combate activo a 60 fps.
- Del segundo ~195 al final (`frame_018.bmp` a `frame_022.bmp`):
  - El combate transcurrio de forma activa y fluida.
  - `frame_018.bmp` (~195 s): intercambio de golpes en la plataforma central.
  - `frame_019.bmp` (~198 s) y `frame_020.bmp` (~201 s): animaciones de combate activas.
  - `frame_021.bmp` (~203 s) y `frame_022.bmp` (~204 s): combate activo durante el censo hasta la detencion en el hilo 3.

## Todas las lineas [rbtest] y trazas asociadas
```
[rbtest] on: rewind test rounds of 1 1 1 5 10 frames, the first 200 s after start
[rbtest] round 1: finding the pages the game writes (120 frames)
[rbtest] round 1: census of who writes the pages (10 frames)
[rbtest] census: thread 9364 (guest 32) wrote 77 pages while the simulation ran, 87 outside it: its own pace, its pages left out
[rbtest] census: thread 15008 (guest 37) wrote 150 pages while the simulation ran, 96 outside it: its own pace, its pages left out
[rbtest] census: thread 17076 (guest 19) wrote 174 pages while the simulation ran, 185 outside it: its own pace, its pages left out
[rbtest] census: thread 17204 (guest 11) wrote 1 pages while the simulation ran, 1 outside it: its own pace, its pages left out
[rbtest] census: thread 30160 (guest 26) wrote 4 pages while the simulation ran, 19 outside it: its own pace, its pages left out
[rbtest] census: thread 30216 (guest -1) wrote 0 pages while the simulation ran, 341 outside it: its own pace, its pages left out
[rbtest] census: thread 25772 (guest 3) wrote 117 pages while the simulation ran, 94 outside it: its own pace, its pages left out
[rbtest] census: thread 29420 (guest 28) wrote 27 pages while the simulation ran, 32 outside it: its own pace, its pages left out
[rbtest] census: thread 5420 (guest 34) wrote 57 pages while the simulation ran, 73 outside it: its own pace, its pages left out
[rbtest] census: thread 32112 (guest 29) wrote 23 pages while the simulation ran, 32 outside it: its own pace, its pages left out
[rbtest] census: thread 13300 (guest 30) wrote 857 pages while the simulation ran, 803 outside it: its own pace, its pages left out
[rbtest] census: thread 2060 (guest 35) wrote 26 pages while the simulation ran, 42 outside it: its own pace, its pages left out
[rbtest] census: thread 11268 (guest 4) wrote 0 pages while the simulation ran, 25 outside it: its own pace, its pages left out
[rbtest] census: thread 8064 (guest 36) wrote 21 pages while the simulation ran, 37 outside it: its own pace, its pages left out
[rbtest] census: thread 32132 (guest 33) wrote 12 pages while the simulation ran, 8 outside it: its own pace, its pages left out
[rbtest] census: thread 24004 (guest 38) wrote 20 pages while the simulation ran, 19 outside it: its own pace, its pages left out
[rbtest] census: thread 34000 (guest 12) wrote 382 pages while the simulation ran, 0 outside it
[rbtest] census: thread 28932 (guest 13) wrote 356 pages while the simulation ran, 0 outside it
[rbtest] census: thread 8340 (guest 31) wrote 11 pages while the simulation ran, 15 outside it: its own pace, its pages left out
[rbtest] census: thread 29848 (guest 44) wrote 1045 pages while the simulation ran, 548 outside it: its own pace, its pages left out
[rbtest] census: thread 23608 (guest 40) wrote 64 pages while the simulation ran, 111 outside it: its own pace, its pages left out
[rbtest] census: thread 29136 (guest 27) wrote 8 pages while the simulation ran, 7 outside it: its own pace, its pages left out
[rbtest] census (10 frames, 5732 pages): the main thread wrote 29 pages while the simulation ran, 1297 outside it; pages left out: 3293, 110 of them written by the main thread or a helper too
[rbtest] round 1: saving
[rbtest] round 1 (1 frames): 5732 pages found written in 120 frames (22.4 MB), 5732 saved; fibers saved 13 (running, not saved: 6); in 27.2 ms
```

## Lineas [latency] 5.0 s desde el segundo ~180 en adelante
```
[latency] 5.0 s: vsync ticks 298 (59.5/s); ticks with no new frame 0 (0.0/s); frames queued 298 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 300 (59.8/s); ticks with no new frame 0 (0.0/s); frames queued 300 (59.8/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 1 (0.2/s);
[latency] 5.0 s: vsync ticks 298 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 298 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 298 (59.4/s); ticks with no new frame 0 (0.0/s); frames queued 298 (59.4/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 3 (0.6/s);
```

## Excepciones, detenciones y trazas (host exception y siguientes 30 lineas)
```
*** host exception 0xc0000005 during nnMain: writing address 0x60864751b8
host pc 0x7ff7894f29db: ssbu.exe +0x90d29db (thread 25772, guest thread 3 (sdk+0x338930, priority 36))
  host frame ssbu.exe +0x88eb5f1
  host frame ssbu.exe +0x87a1b77
  host frame ssbu.exe +0x87aacb3
  host frame ssbu.exe +0x8784274
  host frame ssbu.exe +0x8fd2a2e
  host frame ssbu.exe +0xa64d42c
  host frame ssbu.exe +0xa63812b
  host frame ssbu.exe +0x5b9be
  host frame ssbu.exe +0x3217a0ce
[rbtest] census: thread 9364 (guest 32) wrote 77 pages while the simulation ran, 87 outside it: its own pace, its pages left out
[rbtest] census: thread 15008 (guest 37) wrote 150 pages while the simulation ran, 96 outside it: its own pace, its pages left out
  host frame ssbu.exe +0x82f45
[rbtest] census: thread 17076 (guest 19) wrote 174 pages while the simulation ran, 185 outside it: its own pace, its pages left out
[rbtest] census: thread 17204 (guest 11) wrote 1 pages while the simulation ran, 1 outside it: its own pace, its pages left out
[rbtest] census: thread 30160 (guest 26) wrote 4 pages while the simulation ran, 19 outside it: its own pace, its pages left out
shader compiler threads at the time:
[rbtest] census: thread 30216 (guest -1) wrote 0 pages while the simulation ran, 341 outside it: its own pace, its pages left out
[rbtest] census: thread 25772 (guest 3) wrote 117 pages while the simulation ran, 94 outside it: its own pace, its pages left out
[rbtest] census: thread 29420 (guest 28) wrote 27 pages while the simulation ran, 32 outside it: its own pace, its pages left out
[rbtest] census: thread 5420 (guest 34) wrote 57 pages while the simulation ran, 73 outside it: its own pace, its pages left out
[rbtest] census: thread 32112 (guest 29) wrote 23 pages while the simulation ran, 32 outside it: its own pace, its pages left out
[rbtest] census: thread 13300 (guest 30) wrote 857 pages while the simulation ran, 803 outside it: its own pace, its pages left out
[rbtest] census: thread 2060 (guest 35) wrote 26 pages while the simulation ran, 42 outside it: its own pace, its pages left out
[rbtest] census: thread 11268 (guest 4) wrote 0 pages while the simulation ran, 25 outside it: its own pace, its pages left out
[rbtest] census: thread 8064 (guest 36) wrote 21 pages while the simulation ran, 37 outside it: its own pace, its pages left out
[rbtest] census: thread 32132 (guest 33) wrote 12 pages while the simulation ran, 8 outside it: its own pace, its pages left out
[rbtest] census: thread 24004 (guest 38) wrote 20 pages while the simulation ran, 19 outside it: its own pace, its pages left out
[rbtest] census: thread 34000 (guest 12) wrote 382 pages while the simulation ran, 0 outside it
[rbtest] census: thread 28932 (guest 13) wrote 356 pages while the simulation ran, 0 outside it
[rbtest] census: thread 8340 (guest 31) wrote 11 pages while the simulation ran, 15 outside it: its own pace, its pages left out
[rbtest] census: thread 29848 (guest 44) wrote 1045 pages while the simulation ran, 548 outside it: its own pace, its pages left out
[rbtest] census: thread 23608 (guest 40) wrote 64 pages while the simulation ran, 111 outside it: its own pace, its pages left out
[rbtest] census: thread 29136 (guest 27) wrote 8 pages while the simulation ran, 7 outside it: its own pace, its pages left out
  thread 6532: vs_8e35703e5089ae8c - the driver compiling its DXBC, for 0.3 s; DXBC saved
[rbtest] census (10 frames, 5732 pages): the main thread wrote 29 pages while the simulation ran, 1297 outside it; pages left out: 3293, 110 of them written by the main thread or a helper too
[rbtest] round 1: saving
  (programs in dump/modules/../shadercrash)
guest backtrace (innermost first):
  main+0x381c260
  main+0x35abc70
  main+0x35484b0
  main+0x354a390
  main+0x353d1b0
  main+0x37cc0e0
  sdk+0x337f14
  sdk+0x32f0d0
  lua2cpp_item+0xe7c70
[rbtest] round 1 (1 frames): 5732 pages found written in 120 frames (22.4 MB), 5732 saved; fibers saved 13 (running, not saved: 6); in 27.2 ms
report: reports\crash_20261025_230404.txt; minidump: reports\crash_20261025_230404.dmp
```

## Archivo de crash generado durante la prueba
Archivo: `reports\crash_20261025_230404.txt`
Lineas filtradas con `main+`, `sdk+`, `lua2cpp`, `exception`, `fault`, `rbf_`:
```
*** host exception 0xc0000005 during nnMain: writing address 0x60864751b8
host pc 0x7ff7894f29db: ssbu.exe +0x90d29db (thread 25772, guest thread 3 (sdk+0x338930, priority 36))
  main+0x381c260
  main+0x35abc70
  main+0x35484b0
  main+0x354a390
  main+0x353d1b0
  main+0x37cc0e0
  sdk+0x337f14
  sdk+0x32f0d0
  lua2cpp_item+0xe7c70
```
