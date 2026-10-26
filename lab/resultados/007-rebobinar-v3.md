# Resultado del pedido 007: prueba de rebobinar v3 (crecimiento del conjunto guardado y palabras disidentes)

**Estado: ejecutado con resultados historicos. Se completaron 4 rondas consecutivas de rebobinado (1, 1, 5 y 10 cuadros) con determinismo casi perfecto y continuidad real del combate; fallo en la ronda 5 (30 cuadros).**

## Resumen de los resultados
1. **Rondas 1 a 4 (1, 1, 5 y 10 cuadros) COMPLETADAS CON EXITO:**
   - **Ronda 1 (1 cuadro):** guardo 6.744 paginas (26,3 MB) y 27 fibras. Rebobino y reprodujo 2 veces (4,6 y 4,0 ms). Replay 1 vs 2 difirio en 101 paginas (92 de simulacion). Estado devuelto al juego; continuo combate fluido.
   - **Ronda 2 (1 cuadro):** el conjunto guardado crecio a 7.525 paginas (29,4 MB). Rebobino y reprodujo 2 veces (3,9 y 3,2 ms). Replay 1 vs 2 difirio en 103 paginas (87 de simulacion). Estado devuelto al juego; combate continuo sin inmutarse.
   - **Ronda 3 (5 cuadros):** el conjunto crecio a 8.136 paginas (31,8 MB). Rebobino y reprodujo 2 veces en 16,8 y 16,4 ms (3,28 ms/cuadro). Replay 1 vs 2 difirio en 105 paginas (95 de simulacion). Estado devuelto al juego.
   - **Ronda 4 (10 cuadros):** el conjunto guardado crecio a 8.382 paginas (32,8 MB). Rebobino y reprodujo 10 cuadros dos veces en 31,0 y 30,2 ms (**3,02 ms por cuadro**!).
     - **Determinismo en 10 cuadros:** Replay 1 vs Replay 2 difirio en **solo 93 de 8.382 paginas** (89 de la simulacion y 4 del resto).
     - Estado devuelto al juego; el combate siguio en tiempo real.
2. **Ronda 5 (30 cuadros):**
   - Guardo 8.656 paginas (33,9 MB). Grabo 30 cuadros.
   - Al rebobinar e iniciar el replay de 30 cuadros, ocurrio: `host exception 0xc0000005 during nnMain: writing address 0xffffffffffffffa0` en `main+0x3667300` (lectura de entrada/mandos).

## Codigo y compilacion
- Commit usado: `1f24c9d` ("Rollback test v3: grow the hot set from round to round, show differing words").
- Compilacion: `launchers\Build NativeSmash.bat`.
  - Compilo sin errores en ~35 segundos.
  - Enlazo `build\ssbu.exe` (946 MB).

## Comando y variables usadas
- Ejecutado con PowerShell (`Start-Process`): `.\build\ssbu.exe dump/modules --status 30`
- Variables:
  - `SSBU_DATA_ROOT=C:\WindowSmash\build\testdata` (copia limpia de `save/`)
  - `SSBU_INPUT`: guion con tiempos +125 s (Bayonetta vs CPU Rosalina & Luma)
  - `SSBU_CAPTURE=build/ab7`
  - `SSBU_RB_TEST=1`
  - `SSBU_RB_TEST_AT=200`
  - `SSBU_LATENCY=1`
  - Sin `SSBU_RB_PROBE`
  - Duracion configurada: 380 s (el juego llego hasta el s ~262 donde ocurrio la excepcion de la ronda 5).

## Lo visto en las capturas (paso 4)
- Se generaron las 40 capturas maximas (`frame_000.bmp` a `frame_039.bmp`) entre las 21:57:27 (+134 s) y las 21:59:25 (+252 s).
- Combate: Bayonetta contra Rosalina y Destello en el Dojo del Castillo Suzaku.
- **Antes del segundo 200:** combate transcurre normal (`frame_020.bmp` a +195 s: Bayonetta 41%, Rosalina 0%).
- **Despues de cada ronda (+200 s a +252 s):**
  - **Tras Ronda 1 y 2 (+210 s, `frame_025.bmp`):** el combate sigue normal y fluido, personajes interactuando.
  - **Tras Ronda 3 (+225 s, `frame_030.bmp`):** combate activo, animaciones de golpes normales (Bayonetta 8%).
  - **Tras Ronda 4 (+240 s, `frame_035.bmp`):** el combate sigue perfectamente activo tras rebobinar y repetir 10 cuadros (Bayonetta 28%).
  - **Durante Ronda 5 (+252 s, `frame_039.bmp`):** combate aun en ejecucion, ataques y proyectiles estelares en pantalla (Bayonetta 46%).
- Hacia el segundo ~262 (+21:59:30), durante el replay de 30 cuadros de la ronda 5, el proceso se detuvo por la excepcion de escritura en memoria.

## Todas las lineas [rbtest] (con RESULT y differs in)
```
[rbtest] on: rewind test rounds of 1 1 5 10 30 60 frames, the first 200 s after start
[rbtest] round 1 (1 frames): 6744 pages found written in 120 frames (26.3 MB), 6744 saved; fibers saved 27 (running, not saved: 6); in 63.8 ms
[rbtest] round 1: recorded 1 frames; pages written outside the saved ones meanwhile: 10; of the saved ones, by the simulation: 842; fibers started meanwhile: 0
[rbtest] round 1: the state now kept (fibers 27, running 6); rewinding and replaying twice
[rbtest] round 1: fibers restored 16 (running, skipped: 6; started since, dropped: 0)
[rbtest] round 1 (1 frames): replayed in 4.6 and 4.0 ms (3.98 ms a frame); pads replayed 1 (missing 0), ticks replayed 7 (missing 0); pages written outside the saved ones while replaying: 1
[rbtest] round 1 RESULT replay 1 vs replay 2 (the same frames twice from the same save): DIFFERENT in 101 of 6744 pages: 92 written by the simulation, 9 only by the rest of the frame; the simulation's first: 0x604035e000 0x604035f000 0x6040360000 0x60403c8000 0x6040692000 0x6040758000
[rbtest] round 1 RESULT replay 1 vs the game (which also drew its frames): DIFFERENT in 1245 of 6744 pages: 228 written by the simulation, 1017 only by the rest of the frame; the simulation's first: 0x6040280000 0x604035e000 0x604035f000 0x6040360000 0x60403c8000 0x6040609000
[rbtest] round 1: page 0x604035e000 differs in +0xbf8: 0x407d425f00000000 / 0xc2c4dbec00000000 +0xc00: 0xbba5c8923ee1f40f / 0x3d176e6840a364d6 +0xc08: 0xb98d8506 / 0x3ae4ba30 +0xc78: 0x3d3463d200000000 / 0xc2e37ede00000000 (7 words of 512)
[rbtest] round 1: page 0x604035f000 differs in +0: 0xbb8c10c23ee2e439 / 0x3d3baef640a32524 +0x8: 0xb9707f50 / 0x3acd08ba +0x78: 0xbd0b405800000000 / 0xc2e3c1f900000000 +0x80: 0xbb24fe173eb94022 / 0x3d56e94e40aad7af (17 words of 512)
[rbtest] round 1: page 0x6040360000 differs in +0x7f8: 0xc129a8c300000000 / 0x4116f3de00000000 +0x878: 0xbee6932200000000 / 0xc0b3588300000000 +0xbf8: 0xc12950f900000000 / 0x410c5af400000000 +0xc78: 0x3e4033d800000000 / 0xc0a6c24500000000 (4 words of 512)
[rbtest] round 1: page 0x60403c8000 differs in +0xcd0: 0x3fede94a00000007 / 0xa05536e900000007 +0xcd8: 0x21a4d1dba225a18b / 0xa225a18b3fede94a +0xce0: 0x68f284f4978 / 0x68e21a4d1db (3 words of 512)
[rbtest] round 1: page 0x6040692000 differs in +0xc30: 0x300000002 / 0x300000001 +0xd38: 0xf49 / 0xf4f +0xd40: 0xf43 / 0xf4a (3 words of 512)
[rbtest] round 1: page 0x6040758000 differs in +0x280: 0xe89 / 0xe8f (1 words of 512)
[rbtest] round 1: the game's state given back (fibers 16)
[rbtest] round 2 (1 frames): 7530 pages found written in 120 frames (29.4 MB), 7525 saved; fibers saved 27 (running, not saved: 6); in 80.5 ms
[rbtest] round 2: recorded 1 frames; pages written outside the saved ones meanwhile: 2; of the saved ones, by the simulation: 840; fibers started meanwhile: 0
[rbtest] round 2: the state now kept (fibers 27, running 6); rewinding and replaying twice
[rbtest] round 2: fibers restored 16 (running, skipped: 6; started since, dropped: 0)
[rbtest] round 2 (1 frames): replayed in 3.9 and 3.2 ms (3.21 ms a frame); pads replayed 1 (missing 0), ticks replayed 7 (missing 0); pages written outside the saved ones while replaying: 0
[rbtest] round 2 RESULT replay 1 vs replay 2 (the same frames twice from the same save): DIFFERENT in 103 of 7525 pages: 87 written by the simulation, 16 only by the rest of the frame; the simulation's first: 0x604035e000 0x604035f000 0x6040360000 0x60403c8000 0x6040692000 0x6040758000
[rbtest] round 2 RESULT replay 1 vs the game (which also drew its frames): DIFFERENT in 1309 of 7525 pages: 231 written by the simulation, 1078 only by the rest of the frame; the simulation's first: 0x6040280000 0x604035e000 0x604035f000 0x6040360000 0x60403c8000 0x6040609000
[rbtest] round 2: page 0x604035e000 differs in +0xbf8: 0xc0a623f200000000 / 0xc16b416000000000 +0xc00: 0x3b855c4c40f0b018 / 0x3d6bb3fe40175c9d +0xc08: 0x3aa0a2d0 / 0x3a94d235 +0xc78: 0xc0d3611600000000 / 0xc1b2051f00000000 (7 words of 512)
[rbtest] round 2: page 0x604035f000 differs in +0: 0x3ba753e140e9693b / 0x3d9649d9401a9715 +0x8: 0xba4b1cae / 0x3a9a84db +0x78: 0xc0d4e2f600000000 / 0xc1aef4b700000000 +0x80: 0x3ce4c2a041405f7f / 0x3de459384076964e (17 words of 512)
[rbtest] round 2: page 0x6040360000 differs in +0x7f8: 0xc0eea7ba00000000 / 0xc0b3e86600000000 +0x878: 0x4143b93500000000 / 0xbf8c167e00000000 +0xbf8: 0xc102528700000000 / 0xc0b147a000000000 +0xc78: 0x4139e6ae00000000 / 0xbf2f732b00000000 (4 words of 512)
[rbtest] round 2: page 0x60403c8000 differs in +0xcd0: 0x91ed69f60000000a / 0x1ca6ab740000000a +0xcd8: 0x2a0a9cbf03de6abb / 0x91ed69f699bbc3aa +0xce0: 0x9126debacc7 / 0x91003de6abb (3 words of 512)
[rbtest] round 2: page 0x6040692000 differs in +0xc30: 0x300000000 / 0x300000002 +0xd38: 0x11df / 0x11e4 +0xd40: 0x11da / 0x11e1 (3 words of 512)
[rbtest] round 2: page 0x6040758000 differs in +0x280: 0x130a / 0x1313 (1 words of 512)
[rbtest] round 2: the game's state given back (fibers 16)
[rbtest] round 3 (5 frames): 7990 pages found written in 120 frames (31.2 MB), 7965 saved; fibers saved 27 (running, not saved: 6); in 84.2 ms
[rbtest] round 3: recorded 5 frames; pages written outside the saved ones meanwhile: 12; of the saved ones, by the simulation: 939; fibers started meanwhile: 2
[rbtest] round 3: the state now kept (fibers 27, running 6); rewinding and replaying twice
[rbtest] round 3: fibers restored 16 (running, skipped: 6; started since, dropped: 0)
[rbtest] round 3 (5 frames): replayed in 17.5 and 43.4 ms (8.67 ms a frame); pads replayed 5 (missing 0), ticks replayed 29 (missing 0); pages written outside the saved ones while replaying: 0
[rbtest] round 3 RESULT replay 1 vs replay 2 (the same frames twice from the same save): DIFFERENT in 83 of 7965 pages: 78 written by the simulation, 5 only by the rest of the frame; the simulation's first: 0x604035e000 0x604035f000 0x6040360000 0x60403c8000 0x6040692000 0x6040758000
[rbtest] round 3 RESULT replay 1 vs the game (which also drew its frames): DIFFERENT in 1837 of 7965 pages: 456 written by the simulation, 1381 only by the rest of the frame; the simulation's first: 0x6040202000 0x6040203000 0x6040205000 0x6040206000 0x6040280000 0x604035d000
[rbtest] round 3: page 0x604035e000 differs in +0xbf8: 0xc0b9b2a800000000 / 0x40cc27c500000000 +0xc00: 0xbb25d2e83ee2e2fd / 0xbb5ca8cf3e92579b +0xc08: 0xba0d3682 / 0xb946f78e +0xc78: 0xc12672af00000000 / 0x411e728700000000 (7 words of 512)
[rbtest] round 3: page 0x604035f000 differs in +0: 0xbb61ed763edbbbc5 / 0xbb58f5d83eab1efd +0x8: 0xba1c0244 / 0xb906817e +0x78: 0xc12642d900000000 / 0x411f3fe600000000 +0x80: 0x3be462213f874275 / 0xbaf425bfbe1afa02 (17 words of 512)
[rbtest] round 3: page 0x6040360000 differs in +0x7f8: 0x4169b60500000000 / 0x41b506b300000000 +0x878: 0x3fedcba800000000 / 0xc11c2d1800000000 +0xbf8: 0x416aaadf00000000 / 0x41bb0c3000000000 +0xc78: 0x40043ff000000000 / 0xc11723b800000000 (4 words of 512)
[rbtest] round 3: page 0x60403c8000 differs in +0xcd0: 0x875f1a9d0000000e / 0xf7a233720000000e +0xcd8: 0xf7a233728036d57d / 0x3957d8cd46a3a976 +0xce0: 0xb9746a3a976 / 0xb990ffc7fa7 +0xd10: 0x1be47b7beb401c2a / 0x7d03aae285055e67 (6 words of 512)
[rbtest] round 3: page 0x6040692000 differs in +0xc30: 0x300000000 / 0x300000001 +0xd38: 0x145c / 0x146f +0xd40: 0x1455 / 0x146a (3 words of 512)
[rbtest] round 3: page 0x6040758000 differs in +0x280: 0x1797 / 0x17b9 (1 words of 512)
[rbtest] round 3: the game's state given back (fibers 16)
[rbtest] round 4 (10 frames): 8407 pages found written in 120 frames (32.8 MB), 8382 saved; fibers saved 27 (running, not saved: 6); in 160.6 ms
[rbtest] round 4: recorded 10 frames; pages written outside the saved ones meanwhile: 28; of the saved ones, by the simulation: 1639; fibers started meanwhile: 4
[rbtest] round 4: the state now kept (fibers 27, running 6); rewinding and replaying twice
[rbtest] round 4: fibers restored 16 (running, skipped: 6; started since, dropped: 0)
[rbtest] round 4 (10 frames): replayed in 31.0 and 30.2 ms (3.02 ms a frame); pads replayed 10 (missing 0), ticks replayed 55 (missing 0); pages written outside the saved ones while replaying: 6
[rbtest] round 4 RESULT replay 1 vs replay 2 (the same frames twice from the same save): DIFFERENT in 93 of 8382 pages: 89 written by the simulation, 4 only by the rest of the frame; the simulation's first: 0x6040202000 0x6040280000 0x604035e000 0x604035f000 0x6040360000 0x60403c8000
[rbtest] round 4 RESULT replay 1 vs the game (which also drew its frames): DIFFERENT in 2863 of 8382 pages: 967 written by the simulation, 1896 only by the rest of the frame; the simulation's first: 0x6040204000 0x6040205000 0x6040270000 0x6040280000 0x6040281000 0x6040282000
[rbtest] round 4: page 0x6040202000 differs in (0 words of 512)
[rbtest] round 4: page 0x6040280000 differs in +0x1d0: 0x9e00000082 / 0x9e00000081 +0x268: 0x300000000 / 0x400000000 +0x298: 0x200000001 / 0x100000000 +0x2f8: 0x1100000009 / 0xe0000000c (8 words of 512)
[rbtest] round 4: page 0x604035e000 differs in +0xbf8: 0x4071805f00000000 / 0xbefa38fe00000000 +0xc00: 0xbd2c0de23ec74764 / 0x3bf057a63ebe871b +0xc08: 0x3977864c / 0x3ac7c900 +0xc78: 0x40bc138900000000 / 0xc044a77400000000 (7 words of 512)
[rbtest] round 4: page 0x604035f000 differs in +0: 0xbd26c4a83ed80d70 / 0x3bf8f86a3f195e9f +0x8: 0xba977d3c / 0x3ac85aea +0x78: 0x40bde05c00000000 / 0xc044d01200000000 +0x80: 0xbd8e8ef6bf09d57a / 0x3b5560e0be9d8508 (17 words of 512)
[rbtest] round 4: page 0x6040360000 differs in +0x7f8: 0x4095786a00000000 / 0xc1c322f800000000 +0x878: 0xc11b036b00000000 / 0xc186f00800000000 +0xbf8: 0x408b02d200000000 / 0xc1c3e01c00000000 +0xc78: 0xc117ccf600000000 / 0xc189e73600000000 (4 words of 512)
[rbtest] round 4: page 0x60403c8000 differs in +0xcd0: 0x768037cc00000011 / 0x2377e52f00000011 +0xcd8: 0x2377e52faf3d98ff / 0xf067211d872e5863 +0xce0: 0xe56872e5863 / 0xe58b3dea58e +0xd10: 0xe18f0d8bc7246d1d / 0x786cad026a5d043b (6 words of 512)
[rbtest] round 4: the game's state given back (fibers 16)
[rbtest] round 5 (30 frames): 8681 pages found written in 120 frames (33.9 MB), 8656 saved; fibers saved 27 (running, not saved: 6); in 211.0 ms
[rbtest] round 5: recorded 30 frames; pages written outside the saved ones meanwhile: 78; of the saved ones, by the simulation: 1257; fibers started meanwhile: 8
[rbtest] round 5: the state now kept (fibers 27, running 6); rewinding and replaying twice
[rbtest] round 5: fibers restored 16 (running, skipped: 6; started since, dropped: 0)
```

## Lineas [latency] 5.0 s: (desde ~180 s en adelante)
```
[latency] 5.0 s: vsync ticks 294 (58.8/s); ticks with no new frame 87 (17.4/s); frames queued 206 (41.2/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 6 (1.2/s);
[latency] 5.0 s: vsync ticks 299 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 299 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 299 (59.7/s); ticks with no new frame 0 (0.0/s); frames queued 299 (59.7/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 286 (57.1/s); ticks with no new frame 3 (0.6/s); frames queued 283 (56.5/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 5 (1.0/s);
[latency] 5.0 s: vsync ticks 297 (59.3/s); ticks with no new frame 1 (0.2/s); frames queued 296 (59.1/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 5 (1.0/s);
[latency] 5.0 s: vsync ticks 268 (53.5/s); ticks with no new frame 16 (3.2/s); frames queued 252 (50.3/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 7 (1.4/s);
[latency] 5.0 s: vsync ticks 276 (55.2/s); ticks with no new frame 8 (1.6/s); frames queued 267 (53.4/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 26 (5.2/s);
[latency] 5.0 s: vsync ticks 262 (52.2/s); ticks with no new frame 9 (1.8/s); frames queued 254 (50.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 58 (11.6/s);
[latency] 5.0 s: vsync ticks 248 (49.5/s); ticks with no new frame 3 (0.6/s); frames queued 244 (48.7/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 70 (14.0/s);
[latency] 5.0 s: vsync ticks 256 (51.0/s); ticks with no new frame 29 (5.8/s); frames queued 228 (45.4/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 44 (8.8/s);
[latency] 5.0 s: vsync ticks 273 (54.6/s); ticks with no new frame 17 (3.4/s); frames queued 255 (51.0/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 36 (7.2/s);
[latency] 5.0 s: vsync ticks 253 (50.1/s); ticks with no new frame 9 (1.8/s); frames queued 244 (48.3/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 44 (8.7/s);
[latency] 5.0 s: vsync ticks 275 (54.9/s); ticks with no new frame 16 (3.2/s); frames queued 259 (51.7/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 32 (6.4/s);
[latency] 5.0 s: vsync ticks 263 (52.5/s); ticks with no new frame 68 (13.6/s); frames queued 195 (39.0/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 27 (5.4/s);
[latency] 5.0 s: vsync ticks 257 (51.3/s); ticks with no new frame 16 (3.2/s); frames queued 241 (48.1/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 49 (9.8/s);
[latency] 5.0 s: vsync ticks 279 (55.8/s); ticks with no new frame 39 (7.8/s); frames queued 240 (48.0/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 23 (4.6/s);
[latency] 5.0 s: vsync ticks 264 (52.7/s); ticks with no new frame 33 (6.6/s); frames queued 232 (46.3/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 52 (10.4/s);
[latency] 5.0 s: vsync ticks 248 (49.5/s); ticks with no new frame 10 (2.0/s); frames queued 237 (47.3/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 48 (9.6/s);
[latency] 5.0 s: vsync ticks 268 (53.6/s); ticks with no new frame 55 (11.0/s); frames queued 213 (42.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 34 (6.8/s);
[latency] 5.0 s: vsync ticks 277 (55.2/s); ticks with no new frame 21 (4.2/s); frames queued 256 (51.0/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 30 (6.0/s);
[latency] 5.0 s: vsync ticks 267 (53.2/s); ticks with no new frame 21 (4.2/s); frames queued 246 (49.1/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 35 (7.0/s);
[latency] 5.0 s: vsync ticks 274 (54.7/s); ticks with no new frame 85 (17.0/s); frames queued 190 (37.9/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 29 (5.8/s);
[latency] 5.0 s: vsync ticks 263 (52.6/s); ticks with no new frame 14 (2.8/s); frames queued 248 (49.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 42 (8.4/s);
[latency] 5.0 s: vsync ticks 271 (54.1/s); ticks with no new frame 25 (5.0/s); frames queued 246 (49.1/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 39 (7.8/s);
```

## Excepcion y backtrace en build\ab7.err
```
*** host exception 0xc0000005 during nnMain: writing address 0xffffffffffffffa0
host pc 0x7ff640540f64: ssbu.exe +0x8b70f64 (thread 23252, guest main thread)
  host frame ssbu.exe +0x8747d
  host frame ssbu.exe +0x86437
  host frame ssbu.exe +0x8e68a95
  host frame ssbu.exe +0x7fd80f
  host frame ssbu.exe +0x79230
  host frame ssbu.exe +0xa4ee27e
  host frame ssbu.exe +0x75f46
  host frame KERNEL32.DLL +0x2dda7
  host frame ntdll.dll +0x7ee4c
guest backtrace (innermost first):
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
report: reports\crash_20261025_215930.txt; minidump: reports\crash_20261025_215930.dmp
```

## Archivo de crash generado
- Archivo: `reports\crash_20261025_215930.txt` (minidump: `reports\crash_20261025_215930.dmp`).
- Lineas filtradas (main+, sdk+, lua2cpp, exception o fault; sin rutas personales):
```
*** host exception 0xc0000005 during nnMain: writing address 0xffffffffffffffa0
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
```
