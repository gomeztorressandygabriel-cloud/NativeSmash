# Resultado del pedido 005: rebobinar guardando las fibras (repeticion del 004 corregido)

**Estado: ejecutado con exito rotundo en la prueba de rebobinado.**
A diferencia del pedido 003 (donde el juego se cerro de inmediato en el cuadro 1 por fallo de fibras), en este pedido **el rebobinado funciono**:
- **Round 1 (1 cuadro):** guardo 5.695 paginas y 26 fibras en 22,3 ms. Rebobino, restauro 15 fibras y reprodujo el cuadro en 15,6 ms sin cerrarse.
- **Round 2 (10 cuadros):** guardo 5.556 paginas y 26 fibras en 41,6 ms. Rebobino, restauro 15 fibras y reprodujo 10 cuadros en 53,2 ms (5,32 ms/cuadro) sin cerrarse.
- Tras completar el replay del round 2 (~213 s), el hilo de render/juego dejo de encolar cuadros nuevos (congelamiento/freeze detectado por el watchdog a los 5 s y 30 s); el proceso continuo vivo hasta los 300 s donde fue detenido.

## Codigo y compilacion
- Commit usado: `6661b03` ("Rollback fibers: state the assembly syntax (clang-cl 23 read it as Intel)").
- Compilacion: `launchers\Build NativeSmash.bat`.
  - Compilo perfectamente con la directiva `.att_syntax prefix`.
  - Enlazo `build\ssbu.exe` (946 MB).
  - Tiempo de compilacion: ~35 segundos (solo recompilo `net_rbfiber.c` y relinkeo).

## Comando y variables usadas
- Ejecutado con PowerShell (`Start-Process`): `.\build\ssbu.exe dump/modules --status 30`
- Variables:
  - `SSBU_DATA_ROOT=C:\WindowSmash\build\testdata` (copia limpia y aislada de `save/`)
  - `SSBU_INPUT`: guion con tiempos +125 s (Bayonetta vs CPU Samus)
  - `SSBU_CAPTURE=build/ab5`
  - `SSBU_RB_TEST=1`
  - `SSBU_RB_TEST_AT=200`
  - `SSBU_LATENCY=1`
  - Sin `SSBU_RB_PROBE`
  - Duracion: 300 s (detenido normalmente al cumplirse los 300 s).

## Lo visto en las capturas (paso 4)
- Se generaron 27 capturas (`frame_000.bmp` a `frame_026.bmp`) entre las 21:27:19 (+133 s) y las 21:28:39 (+212 s).
- **Antes del segundo 200:**
  - El juego arranco, entro a menus, selecciono personajes y escenario (Fragata Orpheon) y comenzo el combate normalmente.
  - En `frame_015.bmp` (+179 s) y `frame_020.bmp` (+194 s) el combate transcurre con total normalidad (animaciones, disparos de Samus, golpes de Bayonetta, porcentajes subiendo).
  - En `frame_022.bmp` (+200 s, justo al activarse el test), el combate sigue activo (Bayonetta 27%, Samus 0%, efecto de impacto en pantalla).
- **Durante y tras el segundo 200:**
  - En `frame_024.bmp` (+206 s, tras completarse el round 1), el combate sigue mostrandose en pantalla con elementos dinamicos (Bayonetta y Samus en accion). **No hubo caida ni cierre del juego en el round 1**.
  - En `frame_026.bmp` (+212 s, tras el round 2), los personajes se encuentran de pie en la plataforma.
  - A partir de los ~213 s, el juego dejo de encolar cuadros nuevos (la imagen quedo congelada en ese estado); no hubo cierre intempestivo ni crash inmediato.

## Lineas [rbtest]
```
[rbtest] on: rewind test rounds of 1, 10, 30 and 60 frames, the first 200 s after start
[rbtest] round 1 (1 frames): 5697 pages found written in 60 frames (22.3 MB), 5695 saved; fibers saved 26 (running, not saved: 6); in 22.3 ms
[rbtest] round 1: recorded 1 frames; pages written outside the saved ones meanwhile: 16; fibers started meanwhile: 0; rewinding and replaying
[rbtest] round 1: fibers restored 15 (running, skipped: 6; started since, dropped: 0)
[rbtest] round 1 (1 frames): replayed in 15.6 ms (15.63 ms a frame); pads replayed 1 (missing 0), ticks replayed 4 (missing 0); RESULT: DIFFERENT in 1105 of 5695 pages, the first: 0x6040200000 0x604020a000 0x6040218000 0x604021c000 0x6040280000 0x6040281000 0x6040282000 0x6040283000
[rbtest] round 2 (10 frames): 5556 pages found written in 60 frames (21.7 MB), 5556 saved; fibers saved 26 (running, not saved: 6); in 41.6 ms
[rbtest] round 2: recorded 10 frames; pages written outside the saved ones meanwhile: 173; fibers started meanwhile: 2; rewinding and replaying
[rbtest] round 2: fibers restored 15 (running, skipped: 6; started since, dropped: 0)
[rbtest] round 2 (10 frames): replayed in 53.2 ms (5.32 ms a frame); pads replayed 10 (missing 0), ticks replayed 46 (missing 0); RESULT: DIFFERENT in 1768 of 5556 pages, the first: 0x6040000000 0x6040004000 0x6040006000 0x604008f000 0x60400a5000 0x604016c000 0x6040182000 0x6040201000
```

## Lineas [latency] (vsync event -> game queues its frame)
```
[latency] vsync event -> game queues its frame (ms; n 2, avg 8.32, max 11.15): <2: 0  2-8: 1  8-14: 1  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 202, avg 3.85, max 16.27): <2: 78  2-8: 99  8-14: 19  14-19: 6  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 296, avg 2.55, max 14.32): <2: 126  2-8: 166  8-14: 3  14-19: 1  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 300, avg 3.59, max 13.42): <2: 9  2-8: 288  8-14: 3  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 289, avg 2.67, max 22.58): <2: 58  2-8: 226  8-14: 4  14-19: 0  19-28: 1  28-40: 0  40-60: 0  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 286, avg 9.06, max 118.46): <2: 1  2-8: 97  8-14: 183  14-19: 4  19-28: 0  28-40: 0  40-60: 0  >=60: 1
[latency] vsync event -> game queues its frame (ms; n 257, avg 8.15, max 33.71): <2: 18  2-8: 69  8-14: 165  14-19: 3  19-28: 1  28-40: 1  40-60: 0  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 265, avg 9.24, max 351.73): <2: 8  2-8: 138  8-14: 111  14-19: 5  19-28: 1  28-40: 1  40-60: 0  >=60: 1
[latency] vsync event -> game queues its frame (ms; n 273, avg 10.19, max 76.60): <2: 1  2-8: 97  8-14: 149  14-19: 19  19-28: 4  28-40: 1  40-60: 1  >=60: 1
[latency] vsync event -> game queues its frame (ms; n 208, avg 9.58, max 42.68): <2: 18  2-8: 90  8-14: 62  14-19: 16  19-28: 17  28-40: 4  40-60: 1  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 199, avg 11.15, max 47.01): <2: 13  2-8: 58  8-14: 80  14-19: 20  19-28: 21  28-40: 6  40-60: 1  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 216, avg 10.23, max 48.36): <2: 22  2-8: 55  8-14: 92  14-19: 20  19-28: 25  28-40: 1  40-60: 1  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 209, avg 11.87, max 89.68): <2: 13  2-8: 52  8-14: 88  14-19: 22  19-28: 30  28-40: 2  40-60: 1  >=60: 1
[latency] vsync event -> game queues its frame (ms; n 217, avg 9.32, max 51.64): <2: 23  2-8: 84  8-14: 73  14-19: 11  19-28: 23  28-40: 1  40-60: 2  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 278, avg 7.87, max 41.99): <2: 2  2-8: 183  8-14: 86  14-19: 5  19-28: 1  28-40: 0  40-60: 1  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 293, avg 8.64, max 27.19): <2: 1  2-8: 135  8-14: 152  14-19: 3  19-28: 2  28-40: 0  40-60: 0  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 162, avg 14.08, max 52.50): <2: 3  2-8: 7  8-14: 89  14-19: 35  19-28: 23  28-40: 4  40-60: 1  >=60: 0
```

## Lineas con fiber y bloques siguientes
```
--- Bloque en linea 35 (mencion de fiber/status) ---
[status] fibers: 7 started, 13 host fibers (0 finalized, kept for the next ones)
  thread 1 (priority 44): running or in a system call
[latency] vsync tick, gap to the previous one (ms; n 180, avg 16.67, max 20.15): <2: 0  2-8: 0  8-14: 1  14-19: 177  19-28: 2  28-40: 0  40-60: 0  >=60: 0
[latency] vsync tick, wake-up after it was due (ms; n 181, avg 0.08, max 5.96): <0.1: 176  0.1-0.5: 2  0.5-1: 0  1-2: 0  2-4: 1  4-8: 2  8-16: 0  >=16: 0
[latency] vsync thread waiting for the GPU thread (ms; n 1, avg 0.00, max 0.00): <0.5: 1  0.5-1: 0  1-2: 0  2-4: 0  4-8: 0  8-16: 0  16-33: 0  >=33: 0
[latency] vsync thread in present_frame (ms; n 1, avg 120039.73, max 120039.73): <0.5: 0  0.5-1: 0  1-2: 0  2-4: 0  4-8: 0  8-16: 0  16-33: 0  >=33: 1
[latency] IDXGISwapChain::Present (ms; n 1, avg 0.84, max 0.84): <0.5: 0  0.5-1: 1  1-2: 0  2-4: 0  4-8: 0  8-16: 0  16-33: 0  >=33: 0
[latency] vsync thread waiting for the renderer's lock (ms; n 1, avg 0.00, max 0.00): <0.5: 1  0.5-1: 0  1-2: 0  2-4: 0  4-8: 0  8-16: 0  16-33: 0  >=33: 0
      sdk+0x3898fc
      sdk+0x339410
      sdk+0x26ea0
      sdk+0x40f0c0
      sdk+0x40e2c0
      sdk+0x429fe0
  thread 2 (priority 29): waiting on a condition
      sdk+0x339950
      sdk+0x32e850
      main+0x3754560
      sdk+0x334450
      sdk+0x338930
  thread 3 (priority 36): waiting on an address
      sdk+0x32ec00
      main+0x354a390
      main+0x353d1b0
      main+0x37cc0e0
      sdk+0x337f14
      sdk+0x32f0d0
  thread 4 (priority 36): waiting on an address
      sdk+0x32ec00
      main+0x354a390
      main+0x353d1b0

--- Bloque en linea 268 (mencion de fiber/status) ---
[status] fibers: 7 started, 13 host fibers (0 finalized, kept for the next ones)
  thread 1 (priority 44): running or in a system call
      sdk+0x3898fc
      sdk+0x338440
      sdk+0x335b10
      sdk+0x335dd0
      sdk+0x3cece0
      sdk+0x4076c0
  thread 2 (priority 29): waiting on a condition
      sdk+0x339950
      sdk+0x32e850
      main+0x3754560
      sdk+0x334450
      sdk+0x338930
  thread 3 (priority 36): waiting on an address
      sdk+0x32ec00
      main+0x354a390
      main+0x353d1b0
      main+0x37cc0e0
      sdk+0x337f14
      sdk+0x32f0d0
  thread 4 (priority 36): waiting on an address
      sdk+0x32ec00
      main+0x354a390
      main+0x353d1b0
      main+0x37cc0e0
      sdk+0x337f14
      sdk+0x32f0d0
  thread 5 (priority 51): waiting on an address
      sdk+0x32ec00
      main+0x354a390

--- Bloque en linea 512 (mencion de fiber/status) ---
[status] fibers: 297 started, 26 host fibers (5 finalized, kept for the next ones)
  thread 1 (priority 44): running or in a system call
      sdk+0x536e30
      sdk+0x3ce7d0
      sdk+0x427010
      main+0x386f850
      main+0x3746e90
      main+0x2c5620
  thread 2 (priority 29): waiting on a condition
      sdk+0x339950
      sdk+0x32e850
      main+0x3754560
      sdk+0x334450
      sdk+0x338930
  thread 3 (priority 36): waiting on an address
      sdk+0x32ec00
      main+0x354a390
      main+0x353d1b0
      main+0x37cc0e0
      sdk+0x337f14
      sdk+0x32f0d0
  thread 4 (priority 36): waiting on an address
      sdk+0x32ec00
      main+0x354a390
      main+0x353d1b0
      main+0x37cc0e0
      sdk+0x337f14
      sdk+0x32f0d0
  thread 5 (priority 51): waiting on an address
      sdk+0x32ec00
      main+0x354a390

--- Bloque en linea 690 (mencion de fiber/status) ---
[rbtest] round 1 (1 frames): 5697 pages found written in 60 frames (22.3 MB), 5695 saved; fibers saved 26 (running, not saved: 6); in 22.3 ms
[rbtest] round 1: recorded 1 frames; pages written outside the saved ones meanwhile: 16; fibers started meanwhile: 0; rewinding and replaying
[rbtest] round 1: fibers restored 15 (running, skipped: 6; started since, dropped: 0)
[rbtest] round 1 (1 frames): replayed in 15.6 ms (15.63 ms a frame); pads replayed 1 (missing 0), ticks replayed 4 (missing 0); RESULT: DIFFERENT in 1105 of 5695 pages, the first: 0x6040200000 0x604020a000 0x6040218000 0x604021c000 0x6040280000 0x6040281000 0x6040282000 0x6040283000
[latency] 5.0 s: vsync ticks 290 (57.9/s); ticks with no new frame 4 (0.8/s); frames queued 285 (56.9/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 13 (2.6/s);
[latency] frames waiting for the display when the game queued one: 1: 153  2: 132  3: 0  4+: 0
[latency] pad state age when the game reads it (ms; n 286, avg 0.85, max 1.89): <1: 172  1-2: 114  2-4: 0  4-8: 0  8-16: 0  16-33: 0  33-66: 0  >=66: 0
[latency] pad read -> frame handed to the display (ms; n 286, avg 24.81, max 81.66): <8: 0  8-16: 111  16-25: 32  25-33: 75  33-50: 66  50-75: 0  75-100: 2  >=100: 0
[latency] frame queued -> handed to the display (ms; n 286, avg 19.59, max 77.30): <8: 22  8-16: 125  16-25: 16  25-33: 95  33-50: 27  50-75: 0  75-100: 1  >=100: 0
[latency] frame queued, gap to the previous one (ms; n 285, avg 17.56, max 68.90): <2: 0  2-8: 2  8-14: 31  14-19: 202  19-28: 42  28-40: 4  40-60: 2  >=60: 2
[latency] vsync event -> game queues its frame (ms; n 278, avg 7.87, max 41.99): <2: 2  2-8: 183  8-14: 86  14-19: 5  19-28: 1  28-40: 0  40-60: 1  >=60: 0
[latency] vsync tick, gap to the previous one (ms; n 290, avg 17.28, max 67.08): <2: 0  2-8: 0  8-14: 0  14-19: 277  19-28: 11  28-40: 0  40-60: 1  >=60: 1
[latency] vsync tick, wake-up after it was due (ms; n 290, avg 0.03, max 0.50): <0.1: 248  0.1-0.5: 41  0.5-1: 1  1-2: 0  2-4: 0  4-8: 0  8-16: 0  >=16: 0
[latency] vsync thread waiting for the GPU thread (ms; n 286, avg 0.01, max 3.16): <0.5: 285  0.5-1: 0  1-2: 0  2-4: 1  4-8: 0  8-16: 0  16-33: 0  >=33: 0
[latency] vsync thread in present_frame (ms; n 286, avg 2.23, max 50.39): <0.5: 137  0.5-1: 38  1-2: 13  2-4: 34  4-8: 49  8-16: 13  16-33: 1  >=33: 1
[latency] IDXGISwapChain::Present (ms; n 286, avg 1.76, max 9.89): <0.5: 172  0.5-1: 8  1-2: 15  2-4: 33  4-8: 46  8-16: 12  16-33: 0  >=33: 0
[latency] vsync thread waiting for the renderer's lock (ms; n 286, avg 0.14, max 38.05): <0.5: 285  0.5-1: 0  1-2: 0  2-4: 0  4-8: 0  8-16: 0  16-33: 0  >=33: 1
[latency] 5.0 s: vsync ticks 295 (58.9/s); ticks with no new frame 2 (0.4/s); frames queued 293 (58.5/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 7 (1.4/s);
[latency] frames waiting for the display when the game queued one: 1: 293  2: 0  3: 0  4+: 0
[latency] pad state age when the game reads it (ms; n 293, avg 0.78, max 1.88): <1: 187  1-2: 106  2-4: 0  4-8: 0  8-16: 0  16-33: 0  33-66: 0  >=66: 0
[latency] pad read -> frame handed to the display (ms; n 293, avg 14.51, max 31.84): <8: 0  8-16: 270  16-25: 14  25-33: 9  33-50: 0  50-75: 0  75-100: 0  >=100: 0
[latency] frame queued -> handed to the display (ms; n 293, avg 9.43, max 24.01): <8: 70  8-16: 212  16-25: 11  25-33: 0  33-50: 0  50-75: 0  75-100: 0  >=100: 0
[latency] frame queued, gap to the previous one (ms; n 293, avg 17.05, max 34.60): <2: 0  2-8: 0  8-14: 27  14-19: 217  19-28: 46  28-40: 3  40-60: 0  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 293, avg 8.64, max 27.19): <2: 1  2-8: 135  8-14: 152  14-19: 3  19-28: 2  28-40: 0  40-60: 0  >=60: 0
[latency] vsync tick, gap to the previous one (ms; n 295, avg 16.98, max 36.01): <2: 0  2-8: 0  8-14: 1  14-19: 286  19-28: 4  28-40: 4  40-60: 0  >=60: 0
[latency] vsync tick, wake-up after it was due (ms; n 295, avg 0.03, max 0.59): <0.1: 258  0.1-0.5: 36  0.5-1: 1  1-2: 0  2-4: 0  4-8: 0  8-16: 0  >=16: 0
[latency] vsync thread waiting for the GPU thread (ms; n 293, avg 0.15, max 18.19): <0.5: 287  0.5-1: 0  1-2: 0  2-4: 3  4-8: 1  8-16: 1  16-33: 1  >=33: 0
[latency] vsync thread in present_frame (ms; n 293, avg 0.96, max 14.63): <0.5: 112  0.5-1: 118  1-2: 45  2-4: 4  4-8: 10  8-16: 4  16-33: 0  >=33: 0
[latency] IDXGISwapChain::Present (ms; n 293, avg 0.62, max 10.28): <0.5: 219  0.5-1: 52  1-2: 6  2-4: 4  4-8: 9  8-16: 3  16-33: 0  >=33: 0
[latency] vsync thread waiting for the renderer's lock (ms; n 293, avg 0.00, max 0.01): <0.5: 293  0.5-1: 0  1-2: 0  2-4: 0  4-8: 0  8-16: 0  16-33: 0  >=33: 0
[rbtest] round 2 (10 frames): 5556 pages found written in 60 frames (21.7 MB), 5556 saved; fibers saved 26 (running, not saved: 6); in 41.6 ms

--- Bloque en linea 720 (mencion de fiber/status) ---
[rbtest] round 2 (10 frames): 5556 pages found written in 60 frames (21.7 MB), 5556 saved; fibers saved 26 (running, not saved: 6); in 41.6 ms
[rbtest] round 2: recorded 10 frames; pages written outside the saved ones meanwhile: 173; fibers started meanwhile: 2; rewinding and replaying
[rbtest] round 2: fibers restored 15 (running, skipped: 6; started since, dropped: 0)
[rbtest] round 2 (10 frames): replayed in 53.2 ms (5.32 ms a frame); pads replayed 10 (missing 0), ticks replayed 46 (missing 0); RESULT: DIFFERENT in 1768 of 5556 pages, the first: 0x6040000000 0x6040004000 0x6040006000 0x604008f000 0x60400a5000 0x604016c000 0x6040182000 0x6040201000
[latency] 5.0 s: vsync ticks 244 (48.7/s); ticks with no new frame 82 (16.4/s); frames queued 162 (32.4/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 70 (14.0/s);
[latency] frames waiting for the display when the game queued one: 1: 162  2: 0  3: 0  4+: 0
[latency] pad state age when the game reads it (ms; n 171, avg 0.81, max 1.57): <1: 105  1-2: 66  2-4: 0  4-8: 0  8-16: 0  16-33: 0  33-66: 0  >=66: 0
[latency] pad read -> frame handed to the display (ms; n 162, avg 25.47, max 79.20): <8: 0  8-16: 14  16-25: 60  25-33: 79  33-50: 5  50-75: 3  75-100: 1  >=100: 0
[latency] frame queued -> handed to the display (ms; n 162, avg 16.86, max 51.82): <8: 14  8-16: 49  16-25: 93  25-33: 3  33-50: 2  50-75: 1  75-100: 0  >=100: 0
[latency] frame queued, gap to the previous one (ms; n 162, avg 24.84, max 144.64): <2: 0  2-8: 0  8-14: 6  14-19: 36  19-28: 89  28-40: 22  40-60: 6  >=60: 3
[latency] vsync event -> game queues its frame (ms; n 162, avg 14.08, max 52.50): <2: 3  2-8: 7  8-14: 89  14-19: 35  19-28: 23  28-40: 4  40-60: 1  >=60: 0
[latency] vsync tick, gap to the previous one (ms; n 244, avg 20.54, max 58.09): <2: 0  2-8: 0  8-14: 0  14-19: 174  19-28: 26  28-40: 41  40-60: 3  >=60: 0
[latency] vsync tick, wake-up after it was due (ms; n 244, avg 0.02, max 0.54): <0.1: 230  0.1-0.5: 13  0.5-1: 1  1-2: 0  2-4: 0  4-8: 0  8-16: 0  >=16: 0
[latency] vsync thread waiting for the GPU thread (ms; n 162, avg 2.85, max 40.84): <0.5: 74  0.5-1: 8  1-2: 20  2-4: 16  4-8: 29  8-16: 11  16-33: 3  >=33: 1
[latency] vsync thread in present_frame (ms; n 162, avg 5.73, max 37.78): <0.5: 6  0.5-1: 18  1-2: 9  2-4: 11  4-8: 85  8-16: 31  16-33: 1  >=33: 1
[latency] IDXGISwapChain::Present (ms; n 162, avg 5.13, max 11.26): <0.5: 21  0.5-1: 11  1-2: 4  2-4: 13  4-8: 90  8-16: 23  16-33: 0  >=33: 0
[latency] vsync thread waiting for the renderer's lock (ms; n 162, avg 0.00, max 0.03): <0.5: 162  0.5-1: 0  1-2: 0  2-4: 0  4-8: 0  8-16: 0  16-33: 0  >=33: 0
[watchdog] no frame presented for 5 s: report in reports\freeze_20261025_212844.txt, minidump reports\freeze_20261025_212844.dmp
[latency] 5.0 s: vsync ticks 291 (58.1/s); ticks with no new frame 291 (58.1/s); frames queued 0 (0.0/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 1 (0.2/s);
[latency] vsync tick, gap to the previous one (ms; n 291, avg 17.20, max 172.78): <2: 0  2-8: 0  8-14: 0  14-19: 290  19-28: 0  28-40: 0  40-60: 0  >=60: 1
[latency] vsync tick, wake-up after it was due (ms; n 291, avg 0.57, max 156.13): <0.1: 250  0.1-0.5: 39  0.5-1: 1  1-2: 0  2-4: 0  4-8: 0  8-16: 0  >=16: 1

[status] 223 s into nnMain; GPU: 8164 submissions, 294360 command lists, 5424821 draws, 44446 clears, 45373 compute launches (45363 run, 58 GPU-resident buffers made, 9 given up); 4076 frames presented
[status] renderer: 4932510 draws drawn, 492311 skipped (492321 shader, 0 topology, 0 target); 563 shaders (0 failed), 1039 textures (247 unpacked); 1330 ms watching writes; 5 buffers dropped after unmaps
[status] first draws with a shader: 347, 0 of them over 4 ms (0 ms in all); longest 0.3 ms (vs_a9bf496803d23601)
[status] memory: the game 5986 MB; the PC 0.8 of 7.7 GB free; graphics memory 600 MB (budget 3556) + 0 MB shared (budget 0); textures 946 held, 145 MB of 1024 MB allowed, 0 dropped, 113 made smaller (about 43 MB left out)
[status] shaders: 563 made, 11 let go for being idle, 552 with an object now
[status] graphics: 25 images drawn to, 26 MB (0 of 16 MB or more, the biggest 3 MB); 6 buffers kept, 3 MB
[status] frameskip: the game made 4076 frames, 0 left out (0%), 0 draws
[status] fibers: 527 started, 26 host fibers (8 finalized, kept for the next ones)
  thread 1 (priority 44): running or in a system call

--- Bloque en linea 749 (mencion de fiber/status) ---
[status] fibers: 527 started, 26 host fibers (8 finalized, kept for the next ones)
  thread 1 (priority 44): running or in a system call
      main+0x3746e90
      main+0x2c5620
      sdk+0x29d9e0
  thread 2 (priority 29): waiting on a condition
      sdk+0x339950
      sdk+0x32e850
      main+0x3754560
      sdk+0x334450
      sdk+0x338930
  thread 3 (priority 36): waiting on an address
      sdk+0x32ec00
      main+0x354a390
      main+0x353d1b0
      main+0x37cc0e0
      sdk+0x337f14
      sdk+0x32f0d0
  thread 4 (priority 36): waiting on an address
      sdk+0x32ec00
      main+0x354a390
      main+0x353d1b0
      main+0x37cc0e0
      sdk+0x337f14
      sdk+0x32f0d0
  thread 5 (priority 51): waiting on an address
      sdk+0x32ec00
      main+0x354a390
      main+0x353d1b0
      main+0x37cc0e0
      sdk+0x337f14


```

## Informes de crash / freeze generados
No se generaron archivos `reports\crash_*.txt` ni `reports\stopped_*.txt` (el juego no crasheo).
Se generaron informes de watchdog por congelamiento: `reports\freeze_20261025_212844.txt` y `reports\freeze_20261025_212909.txt`.
Lineas filtradas (main+, sdk+, lua2cpp, exception, fault o rbf_):
```
    main+0x3746e90
    main+0x2c5620
    sdk+0x29d9e0
thread 14208 guest thread 2 (sdk+0x338930, priority 29) (CPU 0.0 s)
    sdk+0x339950
    sdk+0x32e850
    main+0x3754560
    sdk+0x334450
    sdk+0x338930
thread 15268 guest thread 3 (sdk+0x338930, priority 36) (CPU 13.1 s)
    sdk+0x32ec00
    main+0x354a390
    main+0x353d1b0
    main+0x37cc0e0
    sdk+0x337f14
    sdk+0x32f0d0
    lua2cpp_item+0xe7c70
thread 22352 guest thread 4 (sdk+0x338930, priority 36) (CPU 5.3 s)
    sdk+0x32ec00
    main+0x354a390
    main+0x353d1b0
    main+0x37cc0e0
    sdk+0x337f14
    sdk+0x32f0d0
    lua2cpp_item+0xe7c70
thread 4184 guest thread 5 (sdk+0x338930, priority 51) (CPU 0.2 s)
    sdk+0x32ec00
    main+0x354a390
    main+0x353d1b0
    main+0x37cc0e0
    sdk+0x337f14
    sdk+0x32f0d0
    lua2cpp_item+0xe7c70
thread 34436 guest thread 6 (sdk+0x338930, priority 51) (CPU 0.1 s)
    sdk+0x32ec00
    main+0x354a390
    main+0x353d1b0
    main+0x37cc0e0
    sdk+0x337f14
    sdk+0x32f0d0
    lua2cpp_item+0xe7c70
thread 10192 guest thread 7 (sdk+0x338930, priority 51) (CPU 2.2 s)
    sdk+0x32ec00
    main+0x354a390
    main+0x353d1b0
    main+0x37cc0e0
    sdk+0x337f14
    sdk+0x32f0d0
    lua2cpp_item+0xe7c70
thread 3108 guest thread 8 (sdk+0x338930, priority 44) (CPU 0.3 s)
    main+0x3543210
    sdk+0x334450
    sdk+0x338930
thread 20572 guest thread 9 (sdk+0x338930, priority 36) (CPU 0.6 s)
    sdk+0x339950
    sdk+0x32e850
    main+0x3543300
    sdk+0x334450
    sdk+0x338930
thread 29816 guest thread 10 (sdk+0x338930, priority 44) (CPU 3.1 s)
    sdk+0x339950
    sdk+0x32e850
    main+0x3544730
    sdk+0x334450
    sdk+0x338930
    sdk+0x2da320
thread 29596 guest thread 11 (sdk+0x338930, priority 29) (CPU 0.0 s)
    sdk+0x3898fc
    sdk+0x339410
    main+0x3810660
    sdk+0x334450
    sdk+0x338930
thread 24796 guest thread 12 (sdk+0x338930, priority 44) (CPU 3.6 s)
    sdk+0x339950
    sdk+0x32e420
    sdk+0x4b1178
    sdk+0x460600
    main+0x3815da0
    main+0x35a3a70
    sdk+0x334450
    sdk+0x338930
thread 29220 guest thread 13 (sdk+0x338930, priority 44) (CPU 3.3 s)
    sdk+0x339950
    sdk+0x32e420
    sdk+0x4b1178
    sdk+0x460600
    main+0x3815da0
    main+0x35a3a70
    sdk+0x334450
    sdk+0x338930
thread 27688 guest thread 14 (sdk+0x338930, priority 37) (CPU 0.1 s)
    sdk+0x339950
    sdk+0x32e420
    sdk+0x4b1178
    sdk+0x460600
    main+0x359edc0
    sdk+0x334450
    sdk+0x338930
thread 26856 guest thread 15 (sdk+0x338930, priority 59) (CPU 0.0 s)
    sdk+0x339950
    sdk+0x32e850
    main+0x36f9690
    sdk+0x334450
    sdk+0x338930
thread 9764 guest thread 16 (sdk+0x338930, priority 44) (CPU 0.0 s)
    sdk+0x339950
    sdk+0x32e850
    main+0x27da0
    main+0x27bf0
    sdk+0x334450
    sdk+0x338930
thread 20560 guest thread 17 (sdk+0x338930, priority 44) (CPU 0.0 s)
    sdk+0x339950
    sdk+0x32e850
    main+0x24147b0
    main+0x2414560
    sdk+0x334450
    sdk+0x338930
thread 26456 guest thread 18 (sdk+0x338930, priority 29) (CPU 0.0 s)
    sdk+0x339950
    sdk+0x32e420
    sdk+0x4b1178
    sdk+0x460600
    main+0x3985ff0
    main+0x399d230
    sdk+0x334450
    sdk+0x338930
thread 7380 guest thread 19 (sdk+0x338930, priority 29) (CPU 30.2 s)
    main+0x396bf10
    main+0x397fff0
    main+0x3962c30
    main+0x39874b0
    sdk+0x334450
    sdk+0x338930
thread 11492 guest thread 20 (sdk+0x338930, priority 29) (CPU 0.2 s)
    sdk+0x3898fc
    sdk+0x339550
    main+0x37401c0
    sdk+0x334450
    sdk+0x338930
thread 8400 guest thread 21 (sdk+0x338930, priority 29) (CPU 0.8 s)
    main+0x398af10
    sdk+0x334450
    sdk+0x338930
thread 21984 guest thread 22 (sdk+0x338930, priority 51) (CPU 0.3 s)
    sdk+0x339950
    sdk+0x32e850
    main+0x22b6620
    sdk+0x334450
    sdk+0x338930
thread 25420 guest thread 23 (sdk+0x338930, priority 59) (CPU 0.1 s)
    sdk+0x339950
    sdk+0x32e850
    main+0x3721660
    sdk+0x334450
    sdk+0x338930
thread 11068 guest thread 47 (sdk+0x338930, priority 29) (CPU 0.8 s)
    main+0x3994570
    main+0x399d230
    sdk+0x334450
    sdk+0x338930
```
