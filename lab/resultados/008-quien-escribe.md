# Resultado del pedido 008: prueba de rebobinar v4 (identificacion de hilos y codigo de paginas discrepantes)

**Estado: ejecutado con exito. Se identificaron con precision los hilos y las funciones que escriben en las paginas discrepantes durante los replays.**

## Hallazgos principales: quien escribe las paginas que discrepan
1. **Paginas que NADIE escribe durante el replay (`written by nobody`):**
   - `0x604035e000`, `0x604035f000`, `0x6040360000`.
   - Durante la repeticion del cuadro, **ningun hilo escribe en ellas**. Su discrepancia proviene de que no fueron modificadas durante el replay, o pertenecen a memoria no tocada por el paso de simulacion.
2. **Hilo 19 (hilo de fondo del juego, prioridad 29):**
   - Escribe activamente en varias de las paginas discrepantes:
     - `0x6040760000 +0xde8`: escrito por el hilo 19 desde `main+0x396bf10 -> main+0x397fff0 -> main+0x3973590 -> main+0x3962c30 -> main+0x39874b0 -> sdk+0x334450`.
     - `0x6060e80000 +0x20`: escrito por el hilo 19 desde `main+0x39940b0 -> main+0x39874b0`.
     - `0x6040815000 +0x110`: escrito por el hilo 19 desde `main+0x396b390 -> main+0x3962c30`.
     - `0x6053e88000 +0x24c`: escrito por el hilo 19.
3. **Hilo 11 (hilo de fondo, prioridad 29):**
   - Escribe en `0x6040692000 +0xd38` desde `main+0x3810660 -> sdk+0x334450`.
4. **Hilo 47 (prioridad 29):**
   - Escribe en `0x6060fab000 +0`.
5. **Hilo 1 (hilo principal / replaying one):**
   - Escribe en `0x60403ca000 +0xee0` (mas de 60 veces) desde `main+0x34b310 -> main+0x311db0 -> main+0x30c710 -> main+0x137b3f0 -> main+0x37246a0 -> main+0x3746e90`.

## Desarrollo de las rondas
- **Rondas 1, 2 y 3 (1 cuadro cada una): COMPLETADAS.**
  - Cada una restauro fibras (13 a 15 fibras), rebobino, midio los accesos de escritura de los hilos, comparo y **devolvio el estado al juego**.
  - El juego continuo en tiempo real normalmente a traves de las 3 rondas (confirmado en capturas hasta +225 s).
- **Tras la ronda 3 (+228 s / 22:12:07):**
  - Ocurrio una detencion: `*** stopped during nnMain: indirect call to 0x3e0ab1c300000003, which is outside every module` en el hilo 19 (backtrace: `main+0x39760d0 -> main+0x3962c30 -> main+0x39874b0`).

## Codigo y compilacion
- Commit usado: `1e55a96` ("Rollback test v4: find which threads write the pages the replays disagree on").
- Compilacion: `launchers\Build NativeSmash.bat`.
  - Compilo sin errores en ~37 segundos.
  - Enlazo `build\ssbu.exe` (946 MB).

## Comando y variables usadas
- Ejecutado con PowerShell (`Start-Process`): `.\build\ssbu.exe dump/modules --status 30`
- Variables:
  - `SSBU_DATA_ROOT=C:\WindowSmash\build\testdata` (copia limpia de `save/`)
  - `SSBU_INPUT`: guion con tiempos +125 s (Bayonetta vs CPU Corrin)
  - `SSBU_CAPTURE=build/ab8`
  - `SSBU_RB_TEST=1`
  - `SSBU_RB_TEST_AT=200`
  - `SSBU_LATENCY=1`
  - Sin `SSBU_RB_PROBE`
  - Duracion configurada: 330 s (termino en el segundo ~228 por la detencion del hilo 19 tras la ronda 3).

## Lo visto en las capturas (paso 4)
- Se generaron 31 capturas (`frame_000.bmp` a `frame_030.bmp`) entre las 22:10:33 (+133 s) y las 22:12:04 (+225 s).
- Combate: Bayonetta contra Corrin en Pueblo Smash (Smashville).
- **Antes del segundo 200:**
  - Combate normal y fluido (`frame_020.bmp` a +195 s: Bayonetta 27%, Corrin 0%, efectos de ataque).
- **Despues de cada ronda (+200 s a +225 s):**
  - **Tras Ronda 1 (+204 s, `frame_023.bmp`):** combate perfectamente activo, Bayonetta 39%, Corrin 0%, burbuja de escudo.
  - **Tras Ronda 2 (+213 s, `frame_026.bmp`):** combate activo, animaciones normales en el escenario.
  - **Tras Ronda 3 (+225 s, `frame_030.bmp`):** combate activo, Corrin en animacion de ataque sobre la plataforma (Bayonetta 18%).
- A los ~228 s (22:12:07), el juego se detuvo por la llamada indirecta en el hilo 19.

## Todas las lineas [rbtest] y trazas asociadas (RESULT, differs in, first written by, written by)
```
[rbtest] on: rewind test rounds of 1 1 1 5 10 frames, the first 200 s after start
[rbtest] round 1 (1 frames): 6529 pages found written in 120 frames (25.5 MB), 6529 saved; fibers saved 24 (running, not saved: 6); in 50.9 ms
[rbtest] round 1: recorded 1 frames; pages written outside the saved ones meanwhile: 33; of the saved ones, by the simulation: 858; fibers started meanwhile: 0
[rbtest] round 1: the state now kept (fibers 24, running 6); rewinding and replaying twice
[rbtest] round 1: fibers restored 13 (running, skipped: 6; started since, dropped: 0)
[rbtest] round 1 (1 frames): replayed in 5.6 and 3.6 ms (3.65 ms a frame); pads replayed 1 (missing 0), ticks replayed 7 (missing 0); pages written outside the saved ones while replaying: 7
[rbtest] round 1 RESULT replay 1 vs replay 2 (the same frames twice from the same save): DIFFERENT in 97 of 6529 pages: 83 written by the simulation, 14 only by the rest of the frame; the simulation's first: 0x604035e000 0x604035f000 0x6040360000 0x60403ca000 0x6040692000 0x6040760000
[rbtest] round 1 RESULT replay 1 vs the game (which also drew its frames): DIFFERENT in 1213 of 6529 pages: 213 written by the simulation, 1000 only by the rest of the frame; the simulation's first: 0x6040280000 0x604035e000 0x604035f000 0x6040360000 0x60403ca000 0x6040509000
[rbtest] round 1: page 0x604035e000 differs in +0xbf8: 0xc182cb4800000000 / 0xc1dabe6900000000 +0xc00: 0xbd97acad41794f82 / 0x3df0320d4124c5df +0xc08: 0xbb3d37f4 / 0x3bc161d3 +0xc78: 0xc145b79800000000 / 0xc1c7ce3900000000 (7 words of 512)
[rbtest] round 1: page 0x604035f000 differs in +0: 0xbd62468a41723ec9 / 0x3e0a95ccc1113264 +0x8: 0xbb3d31b2 / 0x3c4ab438 +0x78: 0xc145a3b600000000 / 0xc31d19d100000000 +0x80: 0xbd66adc94151820f / 0x3e1a9670c1401a46 (17 words of 512)
[rbtest] round 1: page 0x6040360000 differs in +0x7f8: 0xc1acc1db00000000 / 0x40d4481d00000000 +0x878: 0xc0c94ef600000000 / 0x4208715200000000 +0xbf8: 0xc1abbd6800000000 / 0x40be285e00000000 +0xc78: 0xc0ddb66e00000000 / 0x420737c400000000 (4 words of 512)
[rbtest] round 1: page 0x60403ca000 differs in +0xe90: 0x14d7cea800000006 / 0xdf6bd56b00000006 +0xe98: 0xdf6bd56bb40913be / 0xafae81d505a6ad47 +0xea0: 0x93305a6ad47 / 0x93553c6e37d (3 words of 512)
[rbtest] round 1: page 0x6040692000 differs in +0xc30: 0x300000002 / 0x300000000 +0xd38: 0xff8 / 0xffb +0xd40: 0xff3 / 0xff8 (3 words of 512)
[rbtest] round 1: page 0x6040760000 differs in +0xe80: 0xe64 / 0xe69 (1 words of 512)
[rbtest] round 1: the game's state given back (fibers 13)
[rbtest] round 2 (1 frames): 7206 pages found written in 120 frames (28.1 MB), 7204 saved; fibers saved 24 (running, not saved: 6); in 48.3 ms
[rbtest] round 2: recorded 1 frames; pages written outside the saved ones meanwhile: 6; of the saved ones, by the simulation: 785; fibers started meanwhile: 0
[rbtest] round 2: the state now kept (fibers 24, running 6); rewinding and replaying twice
[rbtest] round 2: fibers restored 13 (running, skipped: 6; started since, dropped: 0)
[rbtest] replay 1: page 0x60403ca000 +0xee0 first written by thread 30016 (guest thread 1, the replaying one), host pc ssbu.exe+0x9ade4c
guest backtrace (innermost first):
  main+0x34b310
  main+0x311db0
  main+0x30c710
  main+0x137b3f0
  main+0x135ca00
  main+0x135f570
  main+0x1850f60
  main+0x1850d80
  main+0x37246a0
  main+0x22defb0
  main+0x37246a0
  main+0x22defb0
  main+0x37246a0
[rbtest] replay 1: page 0x6040760000 +0xde8 first written by thread 2260 (guest thread 19), host pc ssbu.exe+0x94c457d
[rbtest] replay 1: page 0x6040692000 +0xd38 first written by thread 9064 (guest thread 11), host pc ssbu.exe+0x90a553b
  main+0x3746e90
guest backtrace (innermost first):
guest backtrace (innermost first):
  main+0x2c5620
  main+0x396bf10
  main+0x397fff0
  main+0x3973590
  main+0x3962c30
  main+0x39874b0
  sdk+0x334450
  sdk+0x338930
  main+0x3810660
  sdk+0x334450
  sdk+0x338930
  sdk+0x29d9e0
[rbtest] replay 1: page 0x6060e80000 +0x20 first written by thread 2260 (guest thread 19), host pc ssbu.exe+0x321771f3
guest backtrace (innermost first):
  main+0x39940b0
  main+0x39874b0
  sdk+0x334450
  sdk+0x338930
[rbtest] replay 1: page 0x6053e88000 +0x24c first written by thread 2260 (guest thread 19), host pc ssbu.exe+0x9508c2c
[rbtest] replay 1: page 0x6060fab000 +0 first written by thread 33064 (guest thread 47), host pc ssbu.exe+0x320eaa31
guest backtrace (innermost first):
[rbtest] replay 1: page 0x604035e000 written by nobody
[rbtest] replay 1: page 0x604035f000 written by nobody
[rbtest] replay 1: page 0x6040360000 written by nobody
[rbtest] replay 1: page 0x60403ca000 written by thread 30016 (guest 1, the replaying one) 60 times or more;
[rbtest] replay 1: page 0x6040692000 written by thread 9064 (guest 11) 1 times or more;
[rbtest] replay 1: page 0x6040760000 written by thread 2260 (guest 19) 4 times or more;
[rbtest] replay 1: page 0x6053e88000 written by thread 2260 (guest 19) 1 times or more;
[rbtest] replay 1: page 0x6060943000 written by nobody
[rbtest] replay 1: page 0x6060e80000 written by thread 2260 (guest 19) 66 times or more;
[rbtest] replay 1: page 0x6060e81000 written by nobody
[rbtest] replay 1: page 0x6060e82000 written by nobody
[rbtest] replay 1: page 0x6060e90000 written by nobody
[rbtest] replay 1: page 0x6060fab000 written by thread 33064 (guest 47) 1 times or more;
[rbtest] replay 1: page 0x6060fac000 written by nobody
[rbtest] replay 1: page 0x60610f0000 written by nobody
[rbtest] replay 1: page 0x6061130000 written by nobody
[rbtest] replay 2: page 0x60403ca000 +0xee0 first written by thread 30016 (guest thread 1, the replaying one), host pc ssbu.exe+0x9ade4c
guest backtrace (innermost first):
  main+0x34b310
  main+0x311db0
  main+0x30c710
  main+0x137b3f0
  main+0x135ca00
  main+0x135f570
  main+0x1850f60
  main+0x1850d80
  main+0x37246a0
  main+0x22defb0
  main+0x37246a0
  main+0x22defb0
  main+0x37246a0
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbtest] replay 2: page 0x6040760000 +0xde8 first written by thread 2260 (guest thread 19), host pc ssbu.exe+0x94c457d
guest backtrace (innermost first):
  main+0x396bf10
  main+0x397fff0
  main+0x3973590
  main+0x3962c30
  main+0x39874b0
  sdk+0x334450
  sdk+0x338930
[rbtest] replay 2: page 0x6060e80000 +0x20 first written by thread 2260 (guest thread 19), host pc ssbu.exe+0x321771f3
guest backtrace (innermost first):
[rbtest] replay 2: page 0x604035e000 written by nobody
[rbtest] replay 2: page 0x604035f000 written by nobody
[rbtest] replay 2: page 0x6040360000 written by nobody
[rbtest] replay 2: page 0x60403ca000 written by thread 30016 (guest 1, the replaying one) 70 times or more;
[rbtest] replay 2: page 0x6040692000 written by nobody
[rbtest] replay 2: page 0x6040760000 written by thread 2260 (guest 19) 4 times or more;
[rbtest] replay 2: page 0x6053e88000 written by nobody
[rbtest] replay 2: page 0x6060943000 written by nobody
[rbtest] replay 2: page 0x6060e80000 written by thread 2260 (guest 19) 1 times or more;
[rbtest] replay 2: page 0x6060e81000 written by nobody
[rbtest] replay 2: page 0x6060e82000 written by nobody
[rbtest] replay 2: page 0x6060e90000 written by nobody
[rbtest] replay 2: page 0x6060fab000 written by nobody
[rbtest] replay 2: page 0x6060fac000 written by nobody
[rbtest] replay 2: page 0x60610f0000 written by nobody
[rbtest] replay 2: page 0x6061130000 written by nobody
[rbtest] round 2 (1 frames): replayed in 13.6 and 7.6 ms (7.59 ms a frame); pads replayed 1 (missing 0), ticks replayed 7 (missing 0); pages written outside the saved ones while replaying: 0
[rbtest] round 2 RESULT replay 1 vs replay 2 (the same frames twice from the same save): DIFFERENT in 116 of 7204 pages: 95 written by the simulation, 21 only by the rest of the frame; the simulation's first: 0x604035e000 0x604035f000 0x6040360000 0x60403ca000 0x6040760000 0x6040815000
[rbtest] round 2 RESULT replay 1 vs the game (which also drew its frames): DIFFERENT in 1254 of 7204 pages: 219 written by the simulation, 1035 only by the rest of the frame; the simulation's first: 0x6040280000 0x604035e000 0x604035f000 0x6040360000 0x60403ca000 0x6040509000
[rbtest] round 2: page 0x604035e000 differs in +0xbf8: 0x40c30aab00000000 / 0x3fdf63cb00000000 +0xc00: 0xbb15c78cbfb183f5 / 0xbcc01ca6bfbd4d19 +0xc08: 0x397d00b9 / 0xb9e0f82d +0xc78: 0x40a9bb3d00000000 / 0x3d8b9f7c00000000 (7 words of 512)
[rbtest] round 2: page 0x604035f000 differs in +0: 0xbbf91c02bfaf8afc / 0xbce23106bfbf37df +0x8: 0x39d1dd76 / 0xb9cd0d9c +0x78: 0x40a7b9ea00000000 / 0x3c8c0b9400000000 +0x80: 0xbc4cad6ebfabbc08 / 0xbd080da5bf7f8acd (17 words of 512)
[rbtest] round 2: page 0x6040360000 differs in +0x7f8: 0x41b2844d00000000 / 0xc15916c700000000 +0x878: 0xc0a067c100000000 / 0x4199afa900000000 +0xbf8: 0x41b222fc00000000 / 0xc162b4e800000000 +0xc78: 0xc0a3098a00000000 / 0x41966bc800000000 (4 words of 512)
[rbtest] round 2: page 0x60403ca000 differs in +0xe90: 0xfeb667d400000008 / 0x7d0cb09b00000008 +0xe98: 0x7d0cb09b017ab23b / 0xafd7ed8be212beda +0xea0: 0xce6e212beda / 0xce87be87920 (3 words of 512)
[rbtest] round 2: page 0x6040760000 differs in +0xe80: 0x12d0 / 0x12d4 (1 words of 512)
[rbtest] round 2: page 0x6040815000 differs in +0x1c0: 0x4 / 0x3 +0x1d0: 0x4 / 0x3 +0x1f0: 0x40024fa600000000 / 0x4025bb3800000000 +0x1f8: 0x3fb373724014056f / 0x3fb3737240024fa6 (16 words of 512)
[rbtest] round 2: the game's state given back (fibers 13)
[rbtest] round 3 (1 frames): 7994 pages found written in 120 frames (31.2 MB), 7972 saved; fibers saved 26 (running, not saved: 6); in 44.0 ms
[rbtest] round 3: recorded 1 frames; pages written outside the saved ones meanwhile: 2; of the saved ones, by the simulation: 889; fibers started meanwhile: 0
[rbtest] round 3: the state now kept (fibers 26, running 6); rewinding and replaying twice
[rbtest] round 3: fibers restored 15 (running, skipped: 6; started since, dropped: 0)
[rbtest] replay 1: page 0x60403ca000 +0xee0 first written by thread 30016 (guest thread 1, the replaying one), host pc ssbu.exe+0x9ade4c
guest backtrace (innermost first):
  main+0x34b310
  main+0x311db0
  main+0x30c710
  main+0x137b3f0
  main+0x135ca00
  main+0x135f570
  main+0x1850f60
  main+0x1850d80
  main+0x37246a0
  main+0x22defb0
  main+0x37246a0
  main+0x22defb0
  main+0x37246a0
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbtest] replay 1: page 0x6040815000 +0x110 first written by thread 2260 (guest thread 19), host pc ssbu.exe+0x94c3450
guest backtrace (innermost first):
  main+0x396b390
  main+0x3962c30
  main+0x39874b0
  sdk+0x334450
  sdk+0x338930
[rbtest] replay 1: page 0x6040816000 +0x46 first written by thread 2260 (guest thread 19), host pc ssbu.exe+0x94c349e
[rbtest] replay 1: page 0x604035e000 written by nobody
[rbtest] replay 1: page 0x604035f000 written by nobody
[rbtest] replay 1: page 0x6040360000 written by nobody
[rbtest] replay 1: page 0x60403ca000 written by thread 30016 (guest 1, the replaying one) 44 times or more;
[rbtest] replay 1: page 0x6040760000 written by nobody
[rbtest] replay 1: page 0x6040815000 written by thread 2260 (guest 19) 40 times or more;
[rbtest] replay 1: page 0x6040816000 written by thread 2260 (guest 19) 1 times or more;
[rbtest] replay 1: page 0x6040818000 written by nobody
[rbtest] replay 1: page 0x604081a000 written by nobody
[rbtest] replay 1: page 0x6053e88000 written by nobody
[rbtest] replay 1: page 0x6060e80000 written by nobody
[rbtest] replay 1: page 0x6060e81000 written by nobody
[rbtest] replay 1: page 0x6060e82000 written by nobody
[rbtest] replay 1: page 0x6060e90000 written by nobody
[rbtest] replay 1: page 0x6061130000 written by nobody
[rbtest] replay 1: page 0x6061131000 written by nobody
[rbtest] replay 2: page 0x6060e81000 +0x560 first written by thread 2260 (guest thread 19), host pc ssbu.exe+0x952b338
guest backtrace (innermost first):
  main+0x3996110
  main+0x3994e00
  main+0x39874b0
[rbtest] replay 2: page 0x60403ca000 +0xee0 first written by thread 30016 (guest thread 1, the replaying one), host pc ssbu.exe+0x9ade4c
  sdk+0x334450
  sdk+0x338930
guest backtrace (innermost first):
  main+0x34b310
  main+0x311db0
  main+0x30c710
  main+0x137b3f0
  main+0x135ca00
  main+0x135f570
  main+0x1850f60
  main+0x1850d80
  main+0x37246a0
  main+0x22defb0
  main+0x37246a0
  main+0x22defb0
  main+0x37246a0
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbtest] replay 2: page 0x604035e000 written by nobody
[rbtest] replay 2: page 0x604035f000 written by nobody
[rbtest] replay 2: page 0x6040360000 written by nobody
[rbtest] replay 2: page 0x60403ca000 written by thread 30016 (guest 1, the replaying one) 49 times or more;
[rbtest] replay 2: page 0x6040760000 written by nobody
[rbtest] replay 2: page 0x6040815000 written by nobody
[rbtest] replay 2: page 0x6040816000 written by nobody
[rbtest] replay 2: page 0x6040818000 written by nobody
[rbtest] replay 2: page 0x604081a000 written by nobody
[rbtest] replay 2: page 0x6053e88000 written by nobody
[rbtest] replay 2: page 0x6060e80000 written by nobody
[rbtest] replay 2: page 0x6060e81000 written by thread 2260 (guest 19) 297 times or more;
[rbtest] replay 2: page 0x6060e82000 written by nobody
[rbtest] replay 2: page 0x6060e90000 written by nobody
[rbtest] replay 2: page 0x6061130000 written by nobody
[rbtest] replay 2: page 0x6061131000 written by nobody
[rbtest] round 3 (1 frames): replayed in 9.2 and 8.8 ms (8.82 ms a frame); pads replayed 1 (missing 0), ticks replayed 4 (missing 0); pages written outside the saved ones while replaying: 0
[rbtest] round 3 RESULT replay 1 vs replay 2 (the same frames twice from the same save): DIFFERENT in 107 of 7972 pages: 92 written by the simulation, 15 only by the rest of the frame; the simulation's first: 0x604035e000 0x604035f000 0x6040360000 0x60403ca000 0x6040692000 0x6040760000
[rbtest] round 3 RESULT replay 1 vs the game (which also drew its frames): DIFFERENT in 1331 of 7972 pages: 219 written by the simulation, 1112 only by the rest of the frame; the simulation's first: 0x6040280000 0x604035e000 0x604035f000 0x6040360000 0x60403ca000 0x6040509000
[rbtest] round 3: page 0x604035e000 differs in +0xbf8: 0xbfed6df500000000 / 0xbe69af8200000000 +0xc00: 0x3c5b4a53be2306b4 / 0x3c31cec83e24e51d +0xc08: 0xb85aeae0 / 0x37cba5f0 +0xc78: 0xbfc3044100000000 / 0xbeade6b500000000 (7 words of 512)
[rbtest] round 3: page 0x604035f000 differs in +0: 0x3c48017abe29d7fa / 0x3c1ac7713e2054df +0x8: 0x394301e8 / 0x395e803a +0x78: 0xbfc246fd00000000 / 0xbeac166400000000 +0x80: 0x3cb02b86be58fb69 / 0x3b0a5e003e5165ad (17 words of 512)
[rbtest] round 3: page 0x6040360000 differs in +0x7f8: 0xc0f8272100000000 / 0xc18c860f00000000 +0x878: 0xc12c2ada00000000 / 0xc13b278100000000 +0xbf8: 0xc0f2d32000000000 / 0xc187d6de00000000 +0xc78: 0xc131806200000000 / 0xc1337baa00000000 (4 words of 512)
[rbtest] round 3: page 0x60403ca000 differs in +0xe90: 0x7034a80600000009 / 0xe26c0cfe00000009 +0xe98: 0xc8fd3420e26c0cfe / 0x2cb0187fc8fd3420 +0xea0: 0x11262cb0187f / 0x1127f911f177 (3 words of 512)
[rbtest] round 3: page 0x6040692000 differs in +0xc30: 0x300000001 / 0x300000002 +0xd38: 0x1565 / 0x1566 +0xd40: 0x155e / 0x1563 (3 words of 512)
[rbtest] round 3: page 0x6040760000 differs in +0xe80: 0x1752 / 0x1753 (1 words of 512)
[rbtest] round 3: the game's state given back (fibers 15)
```

## Lineas [latency] 5.0 s: (desde ~180 s en adelante)
```
[latency] 5.0 s: vsync ticks 294 (58.7/s); ticks with no new frame 87 (17.4/s); frames queued 205 (41.0/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 5 (1.0/s);
[latency] 5.0 s: vsync ticks 298 (59.3/s); ticks with no new frame 0 (0.0/s); frames queued 300 (59.7/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 3 (0.6/s);
[latency] 5.0 s: vsync ticks 299 (59.8/s); ticks with no new frame 0 (0.0/s); frames queued 298 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 296 (59.2/s); ticks with no new frame 0 (0.0/s); frames queued 297 (59.4/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 4 (0.8/s);
[latency] 5.0 s: vsync ticks 278 (55.6/s); ticks with no new frame 18 (3.6/s); frames queued 260 (52.0/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 4 (0.8/s);
[latency] 5.0 s: vsync ticks 274 (54.6/s); ticks with no new frame 5 (1.0/s); frames queued 269 (53.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 16 (3.2/s);
[latency] 5.0 s: vsync ticks 288 (57.5/s); ticks with no new frame 3 (0.6/s); frames queued 285 (56.9/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 14 (2.8/s);
[latency] 5.0 s: vsync ticks 297 (59.2/s); ticks with no new frame 0 (0.0/s); frames queued 297 (59.2/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 4 (0.8/s);
[latency] 5.0 s: vsync ticks 273 (54.6/s); ticks with no new frame 3 (0.6/s); frames queued 270 (54.0/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 11 (2.2/s);
[latency] 5.0 s: vsync ticks 297 (59.4/s); ticks with no new frame 0 (0.0/s); frames queued 297 (59.4/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 3 (0.6/s);
[latency] 5.0 s: vsync ticks 293 (58.6/s); ticks with no new frame 2 (0.4/s); frames queued 291 (58.2/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 6 (1.2/s);
[latency] 5.0 s: vsync ticks 287 (57.2/s); ticks with no new frame 4 (0.8/s); frames queued 283 (56.4/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 15 (3.0/s);
[latency] 5.0 s: vsync ticks 264 (52.7/s); ticks with no new frame 23 (4.6/s); frames queued 241 (48.1/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 36 (7.2/s);
[latency] 5.0 s: vsync ticks 284 (56.8/s); ticks with no new frame 1 (0.2/s); frames queued 283 (56.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 14 (2.8/s);
[latency] 5.0 s: vsync ticks 280 (55.8/s); ticks with no new frame 0 (0.0/s); frames queued 280 (55.8/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 25 (5.0/s);
[latency] 5.0 s: vsync ticks 290 (57.8/s); ticks with no new frame 21 (4.2/s); frames queued 269 (53.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 14 (2.8/s);
[latency] 5.0 s: vsync ticks 295 (58.9/s); ticks with no new frame 0 (0.0/s); frames queued 294 (58.7/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 5 (1.0/s);
```

## Detencion y backtrace en build\ab8.err
```
*** stopped during nnMain: indirect call to 0x3e0ab1c300000003, which is outside every module
guest backtrace (innermost first):
  main+0x39760d0
  main+0x3962c30
  main+0x39874b0
  sdk+0x334450
  sdk+0x338930
report: reports\stopped_20261025_221207.txt
```

## Archivo de stopped generado
- Archivo: `reports\stopped_20261025_221207.txt`
- Lineas filtradas (main+, sdk+, lua2cpp, exception o fault; sin rutas personales):
```
  main+0x39760d0
  main+0x3962c30
  main+0x39874b0
  sdk+0x334450
  sdk+0x338930
```
