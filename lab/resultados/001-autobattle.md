# Resultado del pedido 001: combate automatico con la sonda

**Estado: terminado, con exito en el intento 2 de 3.**

## Codigo y compilacion
- Commit usado: `e62a7e0` (rama `rollback-probe`; fast-forward desde `1ee5d48`, solo cambio `docs/ROLLBACK.md`).
- `git status` sin cambios en archivos con seguimiento antes de empezar.
- Compilo: `launchers\Build NativeSmash.bat` termino bien (`build\ssbu.exe` ya estaba al dia con el codigo de la sonda v4, "nothing to do"). Ningun cambio de codigo.

## Como se ejecuto
- PowerShell (`Start-Process`) en vez de Git Bash, equivalente a `tools/dev/autobattle.sh`: `.\build\ssbu.exe dump/modules --status 30`.
- Variables: `SSBU_DATA_ROOT=C:\WindowSmash\build\testdata` (copia de `save/`, 8 MB; `build\testdata` no existia, no se borro nada), `SSBU_RB_PROBE=1`, `SSBU_LATENCY=1`, `SSBU_CAPTURE=<carpeta de salida>`, `SSBU_INPUT` (abajo).
- Nota: este PC es lento. El juego tarda ~133 s en llegar a la primera imagen (carga de modulos), y el reloj del guion cuenta desde el arranque del proceso.
- Nota: el log del juego sale por **stderr**, no por stdout (hay que redirigir ambos, como hace `2>&1` en el script).
- Nota: `SSBU_CAPTURE` guarda como maximo 40 imagenes, una cada ~3 s (cubre ~2 minutos desde la primera).

## Intentos
1. **Intento 1** (150 s, guion original sin retraso): **no llego al combate.** Solo 6 capturas; la ultima era la intro del juego (ilustracion de luchadores). El juego aun cargaba cuando el guion ya habia pasado (el log se quedo en la carga de modulos).
2. **Intento 2** (330 s, todos los tiempos del guion desplazados +125 s): **llego al combate.**
   - Capturas: menu principal ("Combate normal") -> seleccion de personajes (Bayonetta vs. CPU nivel 3) -> combate 1 contra 1 en un escenario con plataformas (Bayonetta contra Estela/Rosalina) con marcadores de porcentaje.
   - Segundo de inicio del combate (cuenta atras "1"): ~171 s desde el arranque del proceso.
   - Duracion: el combate empezo a ~171 s y la ultima captura (a +252 s, limite de 40 capturas) aun muestra combate en curso, con porcentajes en pantalla; una captura intermedia muestra un marcador grande "2 - 3" con efecto de golpe (no se pudo determinar su significado exacto). Se detuvo el proceso a los 330 s: el combate duro al menos ~80 s en capturas y el proceso corrio ~160 s tras empezar.
   - Sin `host exception`, `stopped during` ni `fatal` en el log.
3. No hizo falta el intento 3.

## SSBU_INPUT que funciono (intento 2)
```
133:A,135:A,138:B,140:A,143:A,147:A,149.5:A,151:A,152.5:A,154:A,155.5:A,157:SUP/0.35,158:A,160:SUP/0.25,161:A,163:SUP/0.2,164:A,166:A,168:PLUS,169.5:PLUS,171:PLUS,177:A,179:A,181:A
```
(Es el del script original con +125 s en cada tiempo.)

## Lineas del log (solo las pedidas)
Lineas `[rbprobe]` (con las pilas indentadas tras `stack at`/`thread ... is in`), `[latency]` "vsync event -> game queues its frame", `[status]` y excepciones (ninguna). Sin direcciones IP (comprobado).

```
[rbprobe] on (rollback_probe.on, version 4): the alias area's, the heap's and the stack's writes are counted; a report every 5 seconds
[status] 30 s into nnMain; GPU: 16 submissions, 59 command lists, 0 draws, 0 clears, 0 compute launches (0 run, 0 GPU-resident buffers made, 0 given up); 1 frames presented
[status] renderer: 0 draws drawn, 0 skipped (0 shader, 0 topology, 0 target); 0 shaders (0 failed), 0 textures (0 unpacked); 0 ms watching writes; 0 buffers dropped after unmaps
[status] memory: the game 3833 MB; the PC 0.9 of 7.7 GB free; graphics memory 50 MB (budget 3556) + 0 MB shared (budget 0); textures 0 held, 0 MB of 1024 MB allowed, 0 dropped, 0 made smaller (about 0 MB left out)
[status] shaders: 0 made, 0 let go for being idle, 0 with an object now
[status] graphics: 0 images drawn to, 0 MB (0 of 16 MB or more, the biggest 0 MB); 0 buffers kept, 0 MB
[status] frameskip: the game made 3 frames, 0 left out (0%), 0 draws
[status] fibers: 7 started, 13 host fibers (0 finalized, kept for the next ones)
[latency] vsync event -> game queues its frame (ms; n 2, avg 25.60, max 44.48): <2: 0  2-8: 1  8-14: 0  14-19: 0  19-28: 0  28-40: 0  40-60: 1  >=60: 0
[rbprobe] 120.1 s, 0 frames. Pages written per frame (4 KB): 481 nvmap allocations known;
[rbprobe] alias regions written most (1 MB each: pages written in these 120 s, the CPU's own only): none
[rbprobe] heap regions written most (1 MB each: pages written in these 120 s): none
[rbprobe] pad reads per frame: avg 0.0, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 13 (13 slots, 0 finalized); 7 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 1; waiting: median 11, max 18; sleeping: median 1, max 3; in a service request: median 0, max 1; none running at 87 of 207 ticks (42%); kernel busy at 4 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 57/3/40/0), 8 (p44, c-2: 0/100/0/0), 9 (p36, c-2: 0/1/0/99), 2 (p29, c-2: 0/0/0/100), 3 (p36, c-2: 0/0/0/100), 4 (p36, c-2: 0/0/0/100), 5 (p51, c-2: 0/0/0/100), 6 (p51, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 57% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_item+0xe7c70
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 203, avg 2.96, max 26.79): <2: 58  2-8: 138  8-14: 4  14-19: 1  19-28: 2  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 198 frames. Pages written per frame (4 KB): alias median 2611, p95 5215, max 17611 (10.2 / 20.4 / 68.8 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 18, p95 49, max 468 (0.1 / 0.2 / 1.8 MB); alias CPU's own median 2373, p95 5132, max 17506 (9.3 / 20.0 / 68.4 MB); alias handed to the GPU median 101, p95 388, max 1471 (0.4 / 1.5 / 5.7 MB); 949 nvmap allocations known; finding them: avg 8.26 ms, max 16.64 ms; 13 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6088500000 9043, 0x6060c00000 9029, 0x6060a00000 8746, 0x6060900000 8614, 0x6046300000 8438, 0x6045f00000 7533, 0x6040600000 6911, 0x6046200000 6878
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.1, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 13 (13 slots, 0 finalized); 7 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 2, max 7; waiting: median 29, max 38; sleeping: median 3, max 3; in a service request: median 0, max 2; none running at 14 of 288 ticks (5%); kernel busy at 9 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 24 (p44, c-2: 100/0/0/0), 25 (p44, c-2: 100/0/0/0), 38 (p44, c-2: 100/0/0/0), 1 (p44, c0: 44/0/26/29), 10 (p44, c-2: 29/0/0/71), 7 (p51, c-2: 12/6/0/82), 9 (p36, c-2: 12/2/0/86), 22 (p51, c-2: 5/15/0/81)
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 297, avg 3.07, max 13.73): <2: 40  2-8: 255  8-14: 2  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 221 frames. Pages written per frame (4 KB): alias median 1661, p95 3011, max 9909 (6.5 / 11.8 / 38.7 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 13, p95 15, max 24 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 1309, p95 1832, max 9296 (5.1 / 7.2 / 36.3 MB); alias handed to the GPU median 281, p95 931, max 1371 (1.1 / 3.6 / 5.4 MB); 1788 nvmap allocations known; finding them: avg 6.08 ms, max 18.09 ms; 61 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6060c00000 12763, 0x6060a00000 12366, 0x6060900000 10926, 0x6088500000 9986, 0x608a800000 8433, 0x6040400000 7869, 0x6040600000 7458, 0x6086400000 7127
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.3, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 13 (13 slots, 0 finalized); 7 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 3; waiting: median 20, max 20; sleeping: median 3, max 3; in a service request: median 0, max 2; none running at 127 of 296 ticks (43%); kernel busy at 2 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 54/0/46/0), 19 (p29, c2: 4/0/1/95), 10 (p44, c-2: 2/0/0/98), 7 (p51, c-2: 1/2/0/96), 9 (p36, c-2: 1/0/0/98), 8 (p44, c-2: 1/99/0/0), 20 (p29, c-2: 1/0/0/99), 21 (p29, c2: 1/99/0/0)
[rbprobe] thread 1 (running at 54% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 299, avg 3.46, max 12.70): <2: 22  2-8: 274  8-14: 3  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 234 frames. Pages written per frame (4 KB): alias median 1961, p95 3123, max 7220 (7.7 / 12.2 / 28.2 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 14, p95 17, max 21 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 1594, p95 2587, max 6065 (6.2 / 10.1 / 23.7 MB); alias handed to the GPU median 368, p95 1308, max 4418 (1.4 / 5.1 / 17.3 MB); 2036 nvmap allocations known; finding them: avg 5.66 ms, max 16.40 ms; 82 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6060c00000 14035, 0x6060a00000 13544, 0x6060900000 11706, 0x60ad300000 11007, 0x6040400000 10983, 0x6088500000 10593, 0x6097a00000 10257, 0x6060d00000 9659
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.3, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 13 (13 slots, 0 finalized); 7 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 5; waiting: median 20, max 20; sleeping: median 3, max 4; in a service request: median 0, max 3; none running at 108 of 298 ticks (36%); kernel busy at 2 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 58/0/42/0), 19 (p29, c2: 6/0/4/90), 10 (p44, c-2: 5/0/0/95), 9 (p36, c-2: 3/0/0/97), 7 (p51, c-2: 2/2/0/96), 20 (p29, c-2: 2/0/0/98), 45 (p29, c2: 0/98/1/0), 2 (p29, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 58% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 272, avg 4.40, max 127.63): <2: 47  2-8: 208  8-14: 15  14-19: 1  19-28: 0  28-40: 0  40-60: 0  >=60: 1
[rbprobe] 5.0 s, 247 frames. Pages written per frame (4 KB): alias median 2002, p95 8146, max 44607 (7.8 / 31.8 / 174.2 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 14, p95 19, max 22 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 1743, p95 7251, max 42072 (6.8 / 28.3 / 164.3 MB); alias handed to the GPU median 257, p95 1117, max 4828 (1.0 / 4.4 / 18.9 MB); 3209 nvmap allocations known; finding them: avg 7.14 ms, max 25.03 ms; 28 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x60b2200000 14250, 0x6060c00000 11540, 0x6088500000 11226, 0x6060a00000 11138, 0x6060d00000 10880, 0x6060900000 10663, 0x6040400000 9887, 0x60ad300000 9783
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.1, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 13 (13 slots, 0 finalized); 7 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 5; waiting: median 20, max 20; sleeping: median 3, max 4; in a service request: median 0, max 3; none running at 70 of 280 ticks (25%); kernel busy at 4 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 59/0/41/0), 7 (p51, c-2: 20/1/0/79), 19 (p29, c2: 19/0/6/75), 10 (p44, c-2: 14/0/0/86), 9 (p36, c-2: 7/1/0/92), 3 (p36, c-2: 4/0/0/96), 5 (p51, c-2: 1/0/0/99), 20 (p29, c-2: 1/0/0/99)
[rbprobe] thread 1 (running at 59% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 250, avg 7.11, max 16.35): <2: 41  2-8: 83  8-14: 118  14-19: 8  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 230 frames. Pages written per frame (4 KB): alias median 4475, p95 5118, max 15126 (17.5 / 20.0 / 59.1 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 14, p95 16, max 20 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 3621, p95 3757, max 13238 (14.1 / 14.7 / 51.7 MB); alias handed to the GPU median 857, p95 1108, max 1888 (3.3 / 4.3 / 7.4 MB); 3211 nvmap allocations known; finding them: avg 8.95 ms, max 19.44 ms; 28 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x609b800000 36003, 0x609b600000 31970, 0x609b700000 25530, 0x60b2200000 25070, 0x6099b00000 18400, 0x609a700000 18400, 0x609d100000 18400, 0x609d200000 18400
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.1, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 13 (13 slots, 0 finalized); 7 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 2, max 5; waiting: median 19, max 20; sleeping: median 3, max 4; in a service request: median 0, max 3; none running at 22 of 288 ticks (8%); kernel busy at 1 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 91/0/9/0), 3 (p36, c-2: 44/0/0/56), 19 (p29, c2: 19/0/33/48), 10 (p44, c-2: 5/0/0/95), 4 (p36, c-2: 4/0/0/96), 13 (p44, c-2: 3/0/0/97), 12 (p44, c-2: 3/0/0/97), 22 (p51, c-2: 2/5/0/93)
[rbprobe] thread 1 (running at 91% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0

[status] 164 s into nnMain; GPU: 3167 submissions, 144339 command lists, 1891405 draws, 19715 clears, 916 compute launches (916 run, 2 GPU-resident buffers made, 0 given up); 1576 frames presented
[status] renderer: 1888508 draws drawn, 2897 skipped (2897 shader, 0 topology, 0 target); 206 shaders (0 failed), 758 textures (239 unpacked); 183 ms watching writes; 0 buffers dropped after unmaps
[status] first draws with a shader: 115, 0 of them over 4 ms (0 ms in all); longest 0.1 ms (vs_02fe5c225f10059a)
[status] memory: the game 5281 MB; the PC 0.7 of 7.7 GB free; graphics memory 428 MB (budget 3556) + 0 MB shared (budget 0); textures 719 held, 134 MB of 1024 MB allowed, 0 dropped, 54 made smaller (about 43 MB left out)
[status] shaders: 206 made, 0 let go for being idle, 206 with an object now
[status] graphics: 23 images drawn to, 24 MB (0 of 16 MB or more, the biggest 3 MB); 2 buffers kept, 2 MB
[status] frameskip: the game made 1578 frames, 0 left out (0%), 0 draws
[status] fibers: 8 started, 14 host fibers (0 finalized, kept for the next ones)
[latency] vsync event -> game queues its frame (ms; n 225, avg 7.02, max 28.05): <2: 34  2-8: 90  8-14: 88  14-19: 10  19-28: 1  28-40: 2  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 188 frames. Pages written per frame (4 KB): alias median 4492, p95 5160, max 17726 (17.5 / 20.2 / 69.2 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 14, p95 16, max 39 (0.1 / 0.1 / 0.2 MB); alias CPU's own median 3627, p95 3780, max 10332 (14.2 / 14.8 / 40.4 MB); alias handed to the GPU median 864, p95 1364, max 7394 (3.4 / 5.3 / 28.9 MB); 3780 nvmap allocations known; finding them: avg 8.73 ms, max 15.34 ms; 49 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x609b800000 21929, 0x609b600000 19378, 0x60b2200000 16170, 0x609b700000 15338, 0x609d100000 13171, 0x609d200000 11342, 0x6099b00000 11218, 0x609a700000 11180
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.3, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 14 (14 slots, 0 finalized); 8 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 5; waiting: median 19, max 20; sleeping: median 3, max 5; in a service request: median 0, max 3; none running at 31 of 258 ticks (12%); kernel busy at 11 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 79/3/16/1), 3 (p36, c-2: 29/0/0/71), 19 (p43, c2: 19/0/30/50), 7 (p51, c-2: 11/1/0/88), 4 (p36, c-2: 4/0/0/96), 13 (p44, c-2: 3/0/0/97), 12 (p44, c-2: 2/0/0/98), 10 (p44, c-2: 2/0/0/98)
[rbprobe] thread 1 (running at 79% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 236, avg 7.91, max 20.10): <2: 10  2-8: 120  8-14: 97  14-19: 8  19-28: 1  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 131 frames. Pages written per frame (4 KB): alias median 3346, p95 4471, max 5068 (13.1 / 17.5 / 19.8 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 13, p95 17, max 19 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2524, p95 3072, max 3340 (9.9 / 12.0 / 13.0 MB); alias handed to the GPU median 904, p95 1512, max 1765 (3.5 / 5.9 / 6.9 MB); 3785 nvmap allocations known; finding them: avg 13.74 ms, max 26.95 ms; 118 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 15311, 0x6086500000 13246, 0x6086400000 11426, 0x6087400000 8616, 0x60afe00000 8574, 0x60b2200000 8468, 0x60b0700000 7598, 0x60ad000000 6734
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.9, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (20 slots, 2 finalized); 15 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 5; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 58 of 249 ticks (23%); kernel busy at 4 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 67/0/33/0), 19 (p43, c2: 30/0/41/29), 3 (p36, c-2: 4/0/0/96), 4 (p36, c-2: 2/0/0/98), 12 (p44, c-2: 0/0/0/100), 13 (p44, c-2: 0/0/0/100), 20 (p29, c-2: 0/0/0/100), 2 (p29, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 67% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 181, avg 11.37, max 49.24): <2: 6  2-8: 18  8-14: 139  14-19: 13  19-28: 2  28-40: 2  40-60: 1  >=60: 0
[rbprobe] 5.0 s, 155 frames. Pages written per frame (4 KB): alias median 3488, p95 3757, max 5365 (13.6 / 14.7 / 21.0 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 17 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2534, p95 2796, max 3427 (9.9 / 10.9 / 13.4 MB); alias handed to the GPU median 925, p95 1000, max 1938 (3.6 / 3.9 / 7.6 MB); 3785 nvmap allocations known; finding them: avg 16.04 ms, max 21.27 ms; 27 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 17454, 0x6086500000 15567, 0x6086400000 11172, 0x60afe00000 10385, 0x60b2200000 9138, 0x60b0700000 8990, 0x60a3d00000 8374, 0x60ad000000 8062
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.2, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 16 (23 slots, 7 finalized); 32 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 55 of 196 ticks (28%); kernel busy at 0 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 71/0/29/0), 19 (p43, c2: 16/0/64/20), 3 (p36, c-2: 4/0/0/96), 4 (p36, c-2: 3/0/0/97), 12 (p44, c-2: 3/0/0/97), 13 (p44, c-2: 2/0/0/98), 14 (p37, c-2: 1/0/0/99), 20 (p29, c-2: 1/0/0/99)
[rbprobe] thread 1 (running at 71% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 170, avg 11.05, max 22.81): <2: 10  2-8: 18  8-14: 127  14-19: 14  19-28: 1  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 146 frames. Pages written per frame (4 KB): alias median 3317, p95 4297, max 4812 (13.0 / 16.8 / 18.8 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 18 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2378, p95 3129, max 3305 (9.3 / 12.2 / 12.9 MB); alias handed to the GPU median 911, p95 1128, max 1507 (3.6 / 4.4 / 5.9 MB); 3786 nvmap allocations known; finding them: avg 17.30 ms, max 22.19 ms; 28 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 17870, 0x6086500000 15747, 0x6086400000 11021, 0x60afe00000 9782, 0x60b0700000 8468, 0x60a3d00000 7896, 0x6087400000 7717, 0x60b2200000 7698
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.2, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 17 (23 slots, 6 finalized); 83 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 67 of 195 ticks (34%); kernel busy at 0 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 65/0/35/0), 19 (p43, c2: 4/0/75/22), 3 (p36, c-2: 3/0/0/97), 13 (p44, c-2: 2/0/0/98), 4 (p36, c-2: 1/0/0/99), 12 (p44, c-2: 1/0/0/99), 47 (p29, c2: 1/95/4/0), 20 (p29, c-2: 1/0/0/99)
[rbprobe] thread 1 (running at 65% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 177, avg 11.05, max 22.49): <2: 9  2-8: 17  8-14: 130  14-19: 20  19-28: 1  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 155 frames. Pages written per frame (4 KB): alias median 3429, p95 4206, max 4793 (13.4 / 16.4 / 18.7 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 18, max 20 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2517, p95 3051, max 3309 (9.8 / 11.9 / 12.9 MB); alias handed to the GPU median 929, p95 1121, max 1484 (3.6 / 4.4 / 5.8 MB); 3786 nvmap allocations known; finding them: avg 15.96 ms, max 20.29 ms; 25 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 19197, 0x6086500000 17445, 0x6086400000 11697, 0x60afe00000 10385, 0x60b0700000 8990, 0x60b2200000 8604, 0x60a3d00000 8374, 0x60ad000000 8064
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.2, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 22 (24 slots, 2 finalized); 183 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 59 of 199 ticks (30%); kernel busy at 0 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 70/0/30/0), 19 (p43, c2: 10/0/69/22), 3 (p36, c-2: 4/0/0/96), 4 (p36, c-2: 1/0/0/99), 12 (p44, c-2: 1/0/0/99), 13 (p44, c-2: 1/0/0/99), 21 (p29, c2: 1/99/0/0), 47 (p29, c2: 1/92/8/0)
[rbprobe] thread 1 (running at 70% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 176, avg 12.15, max 43.45): <2: 2  2-8: 13  8-14: 140  14-19: 16  19-28: 2  28-40: 2  40-60: 1  >=60: 0
[rbprobe] 5.0 s, 154 frames. Pages written per frame (4 KB): alias median 3447, p95 4080, max 4803 (13.5 / 15.9 / 18.8 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 19 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2525, p95 3024, max 3284 (9.9 / 11.8 / 12.8 MB); alias handed to the GPU median 924, p95 1060, max 1522 (3.6 / 4.1 / 5.9 MB); 3786 nvmap allocations known; finding them: avg 16.67 ms, max 21.61 ms; 23 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 19133, 0x6086500000 16558, 0x6086400000 11178, 0x60afe00000 10318, 0x60b0700000 8932, 0x60b2200000 8606, 0x60a3d00000 8322, 0x60ad000000 8014
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.1, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 17 (24 slots, 7 finalized); 215 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 64 of 186 ticks (34%); kernel busy at 0 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 66/0/34/0), 19 (p43, c2: 9/0/78/13), 3 (p36, c-2: 2/0/0/98), 4 (p36, c-2: 2/0/0/98), 12 (p44, c-2: 1/0/0/99), 13 (p44, c-2: 1/0/0/99), 14 (p37, c-2: 1/0/0/99), 47 (p29, c2: 1/95/5/0)
[rbprobe] thread 1 (running at 66% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0

[status] 194 s into nnMain; GPU: 5444 submissions, 231032 command lists, 2816647 draws, 31301 clears, 23545 compute launches (23545 run, 57 GPU-resident buffers made, 9 given up); 2715 frames presented
[status] renderer: 2793553 draws drawn, 23143 skipped (23143 shader, 0 topology, 0 target); 465 shaders (0 failed), 916 textures (245 unpacked); 717 ms watching writes; 4 buffers dropped after unmaps
[status] first draws with a shader: 264, 0 of them over 4 ms (0 ms in all); longest 0.1 ms (vs_02fe5c225f10059a)
[status] memory: the game 5780 MB; the PC 0.8 of 7.7 GB free; graphics memory 534 MB (budget 3556) + 0 MB shared (budget 0); textures 875 held, 149 MB of 1024 MB allowed, 0 dropped, 71 made smaller (about 48 MB left out)
[status] shaders: 465 made, 0 let go for being idle, 465 with an object now
[status] graphics: 25 images drawn to, 26 MB (0 of 16 MB or more, the biggest 3 MB); 3 buffers kept, 2 MB
[status] frameskip: the game made 2716 frames, 0 left out (0%), 0 draws
[status] fibers: 273 started, 24 host fibers (2 finalized, kept for the next ones)
[latency] vsync event -> game queues its frame (ms; n 173, avg 11.17, max 25.79): <2: 9  2-8: 17  8-14: 132  14-19: 10  19-28: 5  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 152 frames. Pages written per frame (4 KB): alias median 3446, p95 4249, max 4819 (13.5 / 16.6 / 18.8 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 17 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2514, p95 3066, max 3268 (9.8 / 12.0 / 12.8 MB); alias handed to the GPU median 925, p95 1108, max 1551 (3.6 / 4.3 / 6.1 MB); 3786 nvmap allocations known; finding them: avg 16.83 ms, max 22.57 ms; 25 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 17558, 0x6086500000 16185, 0x6086400000 11447, 0x60afe00000 10184, 0x60b0700000 8816, 0x60b2200000 8585, 0x60a3d00000 8216, 0x60ad000000 7923
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.2, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 20 (24 slots, 4 finalized); 276 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 3; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 52 of 191 ticks (27%); kernel busy at 1 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 72/0/28/0), 19 (p43, c2: 4/0/79/17), 3 (p36, c-2: 3/0/0/97), 4 (p36, c-2: 1/0/0/99), 12 (p44, c-2: 1/0/0/99), 13 (p44, c-2: 1/0/0/99), 47 (p29, c2: 1/97/3/0), 2 (p29, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 72% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 181, avg 11.71, max 40.94): <2: 6  2-8: 16  8-14: 131  14-19: 24  19-28: 1  28-40: 2  40-60: 1  >=60: 0
[rbprobe] 5.0 s, 146 frames. Pages written per frame (4 KB): alias median 3449, p95 4436, max 5108 (13.5 / 17.3 / 20.0 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 20 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2483, p95 3207, max 3385 (9.7 / 12.5 / 13.2 MB); alias handed to the GPU median 945, p95 1258, max 1723 (3.7 / 4.9 / 6.7 MB); 3786 nvmap allocations known; finding them: avg 16.48 ms, max 21.68 ms; 37 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 17475, 0x6086500000 14355, 0x6086400000 10915, 0x60afe00000 9782, 0x60b0700000 8468, 0x6087400000 8096, 0x60b2200000 8086, 0x60a3d00000 7892
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.3, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 17 (26 slots, 9 finalized); 364 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 61 of 199 ticks (31%); kernel busy at 0 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 68/0/32/0), 19 (p43, c2: 21/0/58/21), 3 (p36, c-2: 4/0/0/96), 4 (p36, c-2: 2/0/0/98), 13 (p44, c-2: 2/0/0/98), 12 (p44, c-2: 1/0/0/99), 2 (p29, c-2: 0/0/0/100), 5 (p51, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 68% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 193, avg 10.83, max 16.02): <2: 3  2-8: 14  8-14: 168  14-19: 8  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 158 frames. Pages written per frame (4 KB): alias median 3447, p95 4257, max 4650 (13.5 / 16.6 / 18.2 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 18 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2504, p95 3107, max 3210 (9.8 / 12.1 / 12.5 MB); alias handed to the GPU median 926, p95 1125, max 1450 (3.6 / 4.4 / 5.7 MB); 3786 nvmap allocations known; finding them: avg 15.85 ms, max 20.62 ms; 38 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 15972, 0x6086500000 13764, 0x6086400000 11632, 0x60afe00000 10586, 0x60b0700000 9164, 0x60b2200000 9101, 0x60a3d00000 8542, 0x60ad000000 8235
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.2, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (26 slots, 8 finalized); 404 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 51 of 204 ticks (25%); kernel busy at 1 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 73/0/27/0), 19 (p43, c2: 17/0/65/19), 3 (p36, c-2: 3/0/0/97), 4 (p36, c-2: 2/0/0/98), 13 (p44, c-2: 2/0/0/98), 12 (p44, c-2: 1/0/0/99), 20 (p29, c-2: 0/0/0/100), 2 (p29, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 73% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 173, avg 11.26, max 60.53): <2: 8  2-8: 17  8-14: 137  14-19: 9  19-28: 1  28-40: 0  40-60: 0  >=60: 1
[rbprobe] 5.0 s, 163 frames. Pages written per frame (4 KB): alias median 3424, p95 3715, max 4737 (13.4 / 14.5 / 18.5 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 18 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2500, p95 2721, max 3242 (9.8 / 10.6 / 12.7 MB); alias handed to the GPU median 926, p95 996, max 1495 (3.6 / 3.9 / 5.8 MB); 3786 nvmap allocations known; finding them: avg 16.55 ms, max 23.19 ms; 11 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 17696, 0x6086500000 15441, 0x6086400000 11740, 0x60afe00000 10921, 0x60b0700000 9454, 0x60b2200000 8953, 0x60a3d00000 8814, 0x60ad000000 8535
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.1, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 21 (26 slots, 5 finalized); 450 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 3; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 61 of 192 ticks (32%); kernel busy at 1 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 68/0/32/0), 19 (p43, c2: 8/0/77/16), 3 (p36, c-2: 3/0/0/97), 4 (p36, c-2: 1/0/0/99), 12 (p44, c-2: 1/0/0/99), 13 (p44, c-2: 1/0/0/99), 2 (p29, c-2: 0/0/0/100), 5 (p51, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 68% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 177, avg 12.27, max 75.36): <2: 2  2-8: 13  8-14: 143  14-19: 17  19-28: 0  28-40: 0  40-60: 0  >=60: 2
[rbprobe] 5.0 s, 166 frames. Pages written per frame (4 KB): alias median 3457, p95 3801, max 4720 (13.5 / 14.8 / 18.4 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 16, max 17 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2506, p95 2803, max 3241 (9.8 / 10.9 / 12.7 MB); alias handed to the GPU median 930, p95 999, max 1479 (3.6 / 3.9 / 5.8 MB); 3786 nvmap allocations known; finding them: avg 15.34 ms, max 23.20 ms; 13 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 18837, 0x6086500000 16546, 0x6086400000 12014, 0x60afe00000 11122, 0x60b0700000 9628, 0x60b2200000 9328, 0x60a3d00000 9033, 0x60ad000000 8685
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.1, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (26 slots, 8 finalized); 497 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 3; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 62 of 184 ticks (34%); kernel busy at 2 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 65/0/35/0), 19 (p43, c2: 18/0/71/11), 3 (p36, c-2: 3/0/0/97), 12 (p44, c-2: 2/0/0/98), 13 (p44, c-2: 2/0/0/98), 20 (p29, c-2: 1/0/0/99), 21 (p29, c2: 1/99/0/0), 47 (p29, c2: 1/92/7/0)
[rbprobe] thread 1 (running at 65% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 187, avg 9.96, max 15.70): <2: 14  2-8: 25  8-14: 141  14-19: 7  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 152 frames. Pages written per frame (4 KB): alias median 3403, p95 4366, max 4751 (13.3 / 17.1 / 18.6 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 16, max 18 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2478, p95 3110, max 3255 (9.7 / 12.1 / 12.7 MB); alias handed to the GPU median 943, p95 1247, max 1496 (3.7 / 4.9 / 5.8 MB); 3787 nvmap allocations known; finding them: avg 16.48 ms, max 21.82 ms; 35 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 18183, 0x6086500000 16250, 0x6086400000 11436, 0x60afe00000 10184, 0x60b0700000 8816, 0x60a3d00000 8335, 0x60b2200000 8278, 0x60ad000000 7924
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.2, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 17 (26 slots, 9 finalized); 546 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 54 of 214 ticks (25%); kernel busy at 0 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 74/0/26/0), 19 (p43, c2: 15/0/59/26), 3 (p36, c-2: 3/0/0/97), 4 (p36, c-2: 2/0/0/98), 12 (p44, c-2: 1/0/0/99), 13 (p44, c-2: 1/0/0/99), 47 (p29, c2: 0/95/5/0), 2 (p29, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 74% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0

[status] 224 s into nnMain; GPU: 7667 submissions, 320439 command lists, 3768887 draws, 42432 clears, 45700 compute launches (45700 run, 57 GPU-resident buffers made, 9 given up); 3828 frames presented
[status] renderer: 3744138 draws drawn, 26352 skipped (26352 shader, 0 topology, 0 target); 539 shaders (0 failed), 985 textures (245 unpacked); 1269 ms watching writes; 4 buffers dropped after unmaps
[status] first draws with a shader: 371, 0 of them over 4 ms (0 ms in all); longest 0.1 ms (vs_02fe5c225f10059a)
[status] memory: the game 5896 MB; the PC 0.7 of 7.7 GB free; graphics memory 568 MB (budget 3556) + 0 MB shared (budget 0); textures 944 held, 150 MB of 1024 MB allowed, 0 dropped, 79 made smaller (about 48 MB left out)
[status] shaders: 540 made, 2 let go for being idle, 538 with an object now
[status] graphics: 27 images drawn to, 26 MB (0 of 16 MB or more, the biggest 3 MB); 3 buffers kept, 2 MB
[status] frameskip: the game made 3834 frames, 0 left out (0%), 0 draws
[rbprobe] 5.0 s, 132 frames. Pages written per frame (4 KB): alias median 3440, p95 4644, max 4925 (13.4 / 18.1 / 19.2 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 19 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2495, p95 3251, max 3325 (9.7 / 12.7 / 13.0 MB); alias handed to the GPU median 941, p95 1418, max 1600 (3.7 / 5.5 / 6.2 MB); 3787 nvmap allocations known; finding them: avg 17.08 ms, max 23.35 ms; 58 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 16660, 0x6086500000 14857, 0x6086400000 10382, 0x60afe00000 8844, 0x60b0700000 7656, 0x60b2200000 7247, 0x6087400000 7211, 0x60a3d00000 7166
[status] fibers: 602 started, 26 host fibers (9 finalized, kept for the next ones)
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.5, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 17 (26 slots, 9 finalized); 602 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 51 of 204 ticks (25%); kernel busy at 0 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 74/0/26/0), 19 (p43, c2: 13/0/59/28), 3 (p36, c-2: 5/0/0/95), 4 (p36, c-2: 4/0/0/96), 12 (p44, c-2: 1/0/0/99), 13 (p44, c-2: 1/0/0/99), 47 (p29, c2: 1/94/5/0), 8 (p44, c-2: 0/100/0/0)
[latency] vsync event -> game queues its frame (ms; n 191, avg 10.49, max 22.23): <2: 11  2-8: 24  8-14: 135  14-19: 19  19-28: 2  28-40: 0  40-60: 0  >=60: 0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] 5.0 s, 119 frames. Pages written per frame (4 KB): alias median 3671, p95 4827, max 5244 (14.3 / 18.9 / 20.5 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 19 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2726, p95 3347, max 3558 (10.6 / 13.1 / 13.9 MB); alias handed to the GPU median 961, p95 1494, max 1686 (3.8 / 5.8 / 6.6 MB); 3787 nvmap allocations known; finding them: avg 15.58 ms, max 25.01 ms; 104 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 16375, 0x6086500000 15271, 0x6086400000 10204, 0x60afe00000 7973, 0x60b2200000 7050, 0x60b0700000 6916, 0x6087400000 6884, 0x60a3d00000 6539
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.8, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 20 (26 slots, 6 finalized); 646 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 0, max 3; none running at 56 of 229 ticks (24%); kernel busy at 3 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 75/0/25/0), 3 (p36, c-2: 11/0/0/89), 19 (p43, c2: 9/0/41/50), 12 (p44, c-2: 5/0/0/95), 13 (p44, c-2: 5/0/0/95), 4 (p36, c-2: 3/0/0/97), 21 (p29, c2: 1/99/0/0), 47 (p29, c2: 0/98/1/0)
[rbprobe] thread 1 (running at 75% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 208, avg 9.00, max 44.88): <2: 26  2-8: 70  8-14: 86  14-19: 16  19-28: 6  28-40: 3  40-60: 1  >=60: 0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] 5.0 s, 272 frames. Pages written per frame (4 KB): alias median 3742, p95 3905, max 4740 (14.6 / 15.3 / 18.5 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 16, max 18 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2780, p95 2903, max 3334 (10.9 / 11.3 / 13.0 MB); alias handed to the GPU median 965, p95 1009, max 1406 (3.8 / 3.9 / 5.5 MB); 3788 nvmap allocations known; finding them: avg 9.35 ms, max 12.55 ms; 14 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 31274, 0x6086500000 27162, 0x6086400000 19334, 0x60b2200000 18404, 0x60afe00000 18224, 0x6060c00000 16274, 0x6060a00000 16234, 0x60b0700000 15776
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.1, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 22 (26 slots, 4 finalized); 713 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 71 of 287 ticks (25%); kernel busy at 1 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 66/0/34/0), 19 (p43, c2: 17/0/61/23), 3 (p36, c-2: 7/0/0/93), 4 (p36, c-2: 6/0/0/94), 13 (p44, c-2: 2/0/0/98), 12 (p44, c-2: 2/0/0/98), 47 (p29, c2: 1/93/6/0), 14 (p37, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 66% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 280, avg 7.22, max 15.37): <2: 17  2-8: 164  8-14: 98  14-19: 1  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] 5.0 s, 153 frames. Pages written per frame (4 KB): alias median 3359, p95 4337, max 5003 (13.1 / 16.9 / 19.5 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 21 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2439, p95 3177, max 3343 (9.5 / 12.4 / 13.1 MB); alias handed to the GPU median 935, p95 1156, max 1660 (3.7 / 4.5 / 6.5 MB); 3788 nvmap allocations known; finding them: avg 16.23 ms, max 21.66 ms; 35 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 17098, 0x6086500000 14913, 0x6086400000 11426, 0x60afe00000 10251, 0x60b0700000 8874, 0x60b2200000 8318, 0x60a3d00000 8268, 0x6087400000 7982
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.2, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (26 slots, 8 finalized); 804 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 49 of 202 ticks (24%); kernel busy at 0 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 76/0/24/0), 19 (p43, c2: 13/0/65/21), 3 (p36, c-2: 4/0/0/96), 4 (p36, c-2: 2/0/0/98), 14 (p37, c-2: 1/0/0/99), 12 (p44, c-2: 0/0/0/100), 13 (p44, c-2: 0/0/0/100), 47 (p29, c2: 0/94/6/0)
[rbprobe] thread 1 (running at 76% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 183, avg 10.74, max 16.83): <2: 15  2-8: 16  8-14: 133  14-19: 19  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] stack at a pad read (the game reading its input):
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] 5.0 s, 242 frames. Pages written per frame (4 KB): alias median 3696, p95 3869, max 4588 (14.4 / 15.1 / 17.9 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 19 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2738, p95 2861, max 3254 (10.7 / 11.2 / 12.7 MB); alias handed to the GPU median 964, p95 1012, max 1345 (3.8 / 4.0 / 5.3 MB); 3788 nvmap allocations known; finding them: avg 10.78 ms, max 19.22 ms; 13 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 28260, 0x6086500000 24747, 0x6086400000 17307, 0x60afe00000 16214, 0x60b2200000 15563, 0x60b0700000 14036, 0x60a3d00000 13213, 0x6060a00000 12621
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.1, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 17 (26 slots, 9 finalized); 947 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 50 of 256 ticks (20%); kernel busy at 5 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 79/0/20/1), 19 (p43, c2: 12/0/70/17), 3 (p36, c-2: 8/0/0/92), 14 (p37, c-2: 4/0/0/96), 4 (p36, c-2: 4/0/0/96), 8 (p44, c-2: 0/100/0/0), 12 (p44, c-2: 0/0/0/100), 13 (p44, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 79% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 252, avg 8.07, max 16.05): <2: 13  2-8: 106  8-14: 129  14-19: 4  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] 5.0 s, 260 frames. Pages written per frame (4 KB): alias median 3681, p95 3865, max 4649 (14.4 / 15.1 / 18.2 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 19 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2740, p95 2854, max 3244 (10.7 / 11.1 / 12.7 MB); alias handed to the GPU median 956, p95 1005, max 1419 (3.7 / 3.9 / 5.5 MB); 3788 nvmap allocations known; finding them: avg 9.83 ms, max 13.42 ms; 19 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 29850, 0x6086500000 26287, 0x6086400000 18639, 0x60afe00000 17420, 0x60b2200000 17259, 0x60b0700000 15080, 0x6060a00000 14698, 0x6060c00000 14284
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.1, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 20 (26 slots, 6 finalized); 1016 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 3; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 62 of 280 ticks (22%); kernel busy at 3 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 74/0/26/0), 19 (p43, c2: 10/0/60/30), 3 (p36, c-2: 6/0/0/94), 12 (p44, c-2: 2/0/0/98), 13 (p44, c-2: 1/0/0/99), 4 (p36, c-2: 1/0/0/99), 14 (p37, c-2: 1/0/0/99), 20 (p29, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 74% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 275, avg 7.25, max 12.82): <2: 17  2-8: 159  8-14: 99  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] stack at a pad read (the game reading its input):
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0

[status] 254 s into nnMain; GPU: 10683 submissions, 438261 command lists, 5139295 draws, 57534 clears, 75289 compute launches (75289 run, 102 GPU-resident buffers made, 11 given up); 5334 frames presented
[status] renderer: 5110855 draws drawn, 28440 skipped (28440 shader, 0 topology, 0 target); 593 shaders (0 failed), 1021 textures (247 unpacked); 1807 ms watching writes; 4 buffers dropped after unmaps
[status] first draws with a shader: 405, 0 of them over 4 ms (0 ms in all); longest 0.1 ms (vs_02fe5c225f10059a)
[rbprobe] 5.0 s, 262 frames. Pages written per frame (4 KB): alias median 3677, p95 3820, max 4593 (14.4 / 14.9 / 17.9 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 16, max 17 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2742, p95 2836, max 3261 (10.7 / 11.1 / 12.7 MB); alias handed to the GPU median 954, p95 990, max 1339 (3.7 / 3.9 / 5.2 MB); 3788 nvmap allocations known; finding them: avg 9.82 ms, max 13.67 ms; 15 frames left out, their pages counted with the next (renderer busy)
[status] memory: the game 5919 MB; the PC 0.8 of 7.7 GB free; graphics memory 571 MB (budget 3556) + 0 MB shared (budget 0); textures 980 held, 151 MB of 1024 MB allowed, 0 dropped, 82 made smaller (about 49 MB left out)
[status] shaders: 593 made, 63 let go for being idle, 530 with an object now
[status] graphics: 27 images drawn to, 26 MB (0 of 16 MB or more, the biggest 3 MB); 4 buffers kept, 2 MB
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 30988, 0x6086500000 27644, 0x6086400000 18826, 0x60b2200000 17604, 0x60afe00000 17554, 0x6060a00000 15365, 0x60b0700000 15196, 0x6060c00000 15052
[status] frameskip: the game made 5335 frames, 0 left out (0%), 0 draws
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.1, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 22 (26 slots, 4 finalized); 1079 started since the game began
[status] fibers: 1079 started, 26 host fibers (4 finalized, kept for the next ones)
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 61 of 278 ticks (22%); kernel busy at 1 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 74/0/26/0), 19 (p43, c2: 15/0/67/17), 3 (p36, c-2: 7/0/0/93), 4 (p36, c-2: 4/0/0/96), 21 (p29, c2: 0/100/0/0), 2 (p29, c-2: 0/0/0/100), 5 (p51, c-2: 0/0/0/100), 6 (p51, c-2: 0/0/0/100)
[latency] vsync event -> game queues its frame (ms; n 274, avg 7.84, max 38.69): <2: 5  2-8: 141  8-14: 126  14-19: 1  19-28: 0  28-40: 1  40-60: 0  >=60: 0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] 5.0 s, 185 frames. Pages written per frame (4 KB): alias median 3779, p95 4779, max 5094 (14.8 / 18.7 / 19.9 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 17 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2798, p95 3315, max 3492 (10.9 / 12.9 / 13.6 MB); alias handed to the GPU median 983, p95 1465, max 1683 (3.8 / 5.7 / 6.6 MB); 3788 nvmap allocations known; finding them: avg 10.55 ms, max 16.70 ms; 99 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 23219, 0x6086500000 20549, 0x6086400000 14723, 0x60afe00000 12395, 0x60b2200000 12334, 0x60b0700000 10730, 0x6087400000 10369, 0x6060a00000 10327
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.5, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (26 slots, 8 finalized); 1127 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 89 of 284 ticks (31%); kernel busy at 2 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 65/0/35/0), 19 (p43, c2: 12/0/43/45), 3 (p36, c-2: 2/0/0/98), 4 (p36, c-2: 1/0/0/99), 21 (p29, c2: 1/99/0/0), 8 (p44, c-2: 1/99/0/0), 14 (p37, c-2: 0/0/0/100), 20 (p29, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 65% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 273, avg 7.27, max 14.30): <2: 15  2-8: 166  8-14: 91  14-19: 1  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 238 frames. Pages written per frame (4 KB): alias median 3713, p95 4330, max 4717 (14.5 / 16.9 / 18.4 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 16, max 18 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2757, p95 3191, max 3318 (10.8 / 12.5 / 13.0 MB); alias handed to the GPU median 956, p95 1118, max 1451 (3.7 / 4.4 / 5.7 MB); 3788 nvmap allocations known; finding them: avg 10.47 ms, max 13.79 ms; 27 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 28081, 0x6086500000 25562, 0x6086400000 17905, 0x60afe00000 15946, 0x60b2200000 15589, 0x60b0700000 13804, 0x60a3d00000 13008, 0x6060a00000 12789
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.1, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 20 (26 slots, 6 finalized); 1177 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 39 of 271 ticks (14%); kernel busy at 2 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 80/0/20/0), 19 (p43, c2: 21/0/55/24), 3 (p36, c-2: 7/0/0/93), 4 (p36, c-2: 7/0/0/93), 12 (p44, c-2: 1/0/0/99), 13 (p44, c-2: 1/0/0/99), 14 (p37, c-2: 0/0/0/100), 2 (p29, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 80% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 258, avg 7.92, max 13.47): <2: 14  2-8: 102  8-14: 142  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 277 frames. Pages written per frame (4 KB): alias median 3697, p95 3835, max 4567 (14.4 / 15.0 / 17.8 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 18 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2756, p95 2845, max 3265 (10.8 / 11.1 / 12.8 MB); alias handed to the GPU median 952, p95 994, max 1364 (3.7 / 3.9 / 5.3 MB); 3788 nvmap allocations known; finding them: avg 9.39 ms, max 14.80 ms; 11 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 32586, 0x6086500000 28651, 0x6086400000 19614, 0x60b2200000 18669, 0x60afe00000 18559, 0x6060a00000 16467, 0x6060c00000 16250, 0x60b0700000 16066
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.0, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (26 slots, 8 finalized); 1283 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 5; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 73 of 287 ticks (25%); kernel busy at 2 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 70/0/30/0), 19 (p43, c2: 13/0/71/16), 3 (p36, c-2: 5/0/0/95), 4 (p36, c-2: 2/0/0/98), 13 (p44, c-2: 1/0/0/99), 14 (p37, c-2: 1/0/0/99), 12 (p44, c-2: 0/0/0/100), 20 (p29, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 70% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 282, avg 7.57, max 11.61): <2: 10  2-8: 169  8-14: 103  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 236 frames. Pages written per frame (4 KB): alias median 3695, p95 4181, max 4715 (14.4 / 16.3 / 18.4 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 18 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2745, p95 3062, max 3286 (10.7 / 12.0 / 12.8 MB); alias handed to the GPU median 966, p95 1089, max 1429 (3.8 / 4.3 / 5.6 MB); 3789 nvmap allocations known; finding them: avg 11.01 ms, max 19.03 ms; 21 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6087500000 29227, 0x6086500000 25479, 0x6086400000 17108, 0x60afe00000 15812, 0x60b2200000 15229, 0x60b0700000 13688, 0x60a3d00000 12758, 0x6060a00000 12286
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.1, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (27 slots, 9 finalized); 1385 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 36 of 266 ticks (14%); kernel busy at 1 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 82/0/16/2), 19 (p43, c2: 19/0/55/26), 3 (p36, c-2: 10/0/0/90), 4 (p36, c-2: 6/0/0/94), 12 (p44, c-2: 0/0/0/100), 13 (p44, c-2: 0/0/0/100), 21 (p29, c2: 0/100/0/0), 2 (p29, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 82% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 252, avg 7.61, max 14.42): <2: 21  2-8: 95  8-14: 135  14-19: 1  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 238 frames. Pages written per frame (4 KB): alias median 3479, p95 5159, max 10450 (13.6 / 20.2 / 40.8 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 29 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2562, p95 3238, max 5338 (10.0 / 12.6 / 20.9 MB); alias handed to the GPU median 910, p95 1911, max 5112 (3.6 / 7.5 / 20.0 MB); 3872 nvmap allocations known; finding them: avg 9.49 ms, max 14.52 ms; 41 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6086400000 18186, 0x6087500000 16520, 0x6086500000 14728, 0x6087400000 14305, 0x60b2200000 13342, 0x6060a00000 12446, 0x6060c00000 11959, 0x6060900000 11553
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.2, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 20 (27 slots, 7 finalized); 1398 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 3; waiting: median 19, max 20; sleeping: median 3, max 4; in a service request: median 1, max 3; none running at 59 of 286 ticks (21%); kernel busy at 1 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 69/1/30/0), 19 (p43, c2: 23/0/49/28), 10 (p44, c-2: 4/0/0/96), 3 (p36, c-2: 1/0/0/99), 7 (p51, c-2: 0/0/0/100), 9 (p36, c-2: 0/1/0/99), 20 (p29, c-2: 0/0/0/100), 21 (p29, c2: 0/100/0/0)
[rbprobe] thread 1 (running at 69% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 274, avg 6.97, max 53.94): <2: 6  2-8: 195  8-14: 69  14-19: 2  19-28: 1  28-40: 0  40-60: 1  >=60: 0
[status] 284 s into nnMain; GPU: 13837 submissions, 566294 command lists, 6567028 draws, 77480 clears, 103476 compute launches (103476 run, 192 GPU-resident buffers made, 15 given up); 6912 frames presented
[status] renderer: 6527387 draws drawn, 39641 skipped (39641 shader, 0 topology, 0 target); 667 shaders (0 failed), 1101 textures (249 unpacked); 2302 ms watching writes; 4 buffers dropped after unmaps
[status] first draws with a shader: 466, 0 of them over 4 ms (0 ms in all); longest 0.1 ms (vs_02fe5c225f10059a)
[status] memory: the game 6038 MB; the PC 0.8 of 7.7 GB free; graphics memory 499 MB (budget 3556) + 0 MB shared (budget 0); textures 1057 held, 198 MB of 1024 MB allowed, 0 dropped, 103 made smaller (about 60 MB left out)
[status] shaders: 667 made, 131 let go for being idle, 536 with an object now
[status] graphics: 43 images drawn to, 30 MB (0 of 16 MB or more, the biggest 3 MB); 6 buffers kept, 2 MB
[status] frameskip: the game made 6913 frames, 0 left out (0%), 0 draws
[status] fibers: 1399 started, 27 host fibers (9 finalized, kept for the next ones)
[rbprobe] 5.0 s, 189 frames. Pages written per frame (4 KB): alias median 3051, p95 3502, max 4254 (11.9 / 13.7 / 16.6 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 17, max 18 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2176, p95 2443, max 2712 (8.5 / 9.5 / 10.6 MB); alias handed to the GPU median 877, p95 1029, max 1804 (3.4 / 4.0 / 7.0 MB); 3877 nvmap allocations known; finding them: avg 14.70 ms, max 31.02 ms; 16 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6086400000 13747, 0x6087500000 12906, 0x6086500000 10405, 0x60a3d00000 10017, 0x6087400000 9663, 0x6060a00000 8783, 0x6060900000 8578, 0x6088500000 8570
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.1, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (27 slots, 9 finalized); 1399 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 46 of 217 ticks (21%); kernel busy at 1 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 76/0/24/0), 19 (p43, c2: 16/0/71/13), 3 (p36, c-2: 4/0/0/96), 4 (p36, c-2: 4/0/0/96), 6 (p51, c-2: 1/0/0/99), 12 (p44, c-2: 1/0/0/99), 13 (p44, c-2: 1/0/0/99), 20 (p29, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 76% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 202, avg 9.65, max 76.29): <2: 11  2-8: 48  8-14: 138  14-19: 4  19-28: 0  28-40: 0  40-60: 0  >=60: 1
[rbprobe] 5.0 s, 259 frames. Pages written per frame (4 KB): alias median 3151, p95 3181, max 3217 (12.3 / 12.4 / 12.6 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 15, max 25 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2301, p95 2327, max 2351 (9.0 / 9.1 / 9.2 MB); alias handed to the GPU median 847, p95 864, max 873 (3.3 / 3.4 / 3.4 MB); 3877 nvmap allocations known; finding them: avg 10.86 ms, max 15.15 ms
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6086400000 17995, 0x6087500000 16641, 0x6060a00000 15048, 0x6060c00000 14651, 0x60a3d00000 13727, 0x6086500000 13390, 0x6060900000 12942, 0x6087400000 12253
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.0, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (27 slots, 9 finalized); 1399 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 58 of 261 ticks (22%); kernel busy at 3 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 74/0/26/0), 19 (p43, c2: 20/0/71/9), 3 (p36, c-2: 8/0/0/92), 4 (p36, c-2: 5/0/0/95), 12 (p44, c-2: 1/0/0/99), 13 (p44, c-2: 1/0/0/99), 49 (p43, c2: 1/88/11/0), 21 (p29, c2: 0/100/0/0)
[rbprobe] thread 1 (running at 74% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 258, avg 7.73, max 11.44): <2: 8  2-8: 116  8-14: 134  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 291 frames. Pages written per frame (4 KB): alias median 3158, p95 3175, max 3568 (12.3 / 12.4 / 13.9 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 15, max 16 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2305, p95 2322, max 2582 (9.0 / 9.1 / 10.1 MB); alias handed to the GPU median 851, p95 862, max 986 (3.3 / 3.4 / 3.9 MB); 3877 nvmap allocations known; finding them: avg 9.43 ms, max 17.76 ms; 2 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6086400000 20217, 0x6087500000 18963, 0x6060c00000 18048, 0x6060a00000 17907, 0x60a3d00000 15423, 0x6086500000 15038, 0x6060900000 15008, 0x6087400000 13965
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.0, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (27 slots, 9 finalized); 1400 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 73 of 288 ticks (25%); kernel busy at 5 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 62/0/38/0), 19 (p43, c2: 25/0/66/9), 3 (p36, c-2: 3/0/0/97), 4 (p36, c-2: 2/0/0/98), 49 (p43, c2: 1/91/8/0), 12 (p44, c-2: 0/0/0/100), 13 (p44, c-2: 0/0/0/100), 20 (p29, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 62% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 292, avg 7.28, max 11.36): <2: 4  2-8: 227  8-14: 61  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 268 frames. Pages written per frame (4 KB): alias median 3145, p95 3172, max 3184 (12.3 / 12.4 / 12.4 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 15, max 15 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2295, p95 2319, max 2327 (9.0 / 9.1 / 9.1 MB); alias handed to the GPU median 844, p95 860, max 870 (3.3 / 3.4 / 3.4 MB); 3877 nvmap allocations known; finding them: avg 10.28 ms, max 15.33 ms
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6086400000 18557, 0x6087500000 17286, 0x6060a00000 15773, 0x6060c00000 14329, 0x60a3d00000 14204, 0x6086500000 13802, 0x6060900000 13349, 0x6087400000 12729
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.0, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (27 slots, 9 finalized); 1400 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 52 of 272 ticks (19%); kernel busy at 6 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 79/0/21/0), 19 (p43, c2: 22/0/67/11), 3 (p36, c-2: 13/0/0/87), 4 (p36, c-2: 9/0/0/91), 13 (p44, c-2: 1/0/0/99), 12 (p44, c-2: 1/0/0/99), 14 (p37, c-2: 1/0/0/99), 49 (p43, c2: 1/92/7/0)
[rbprobe] thread 1 (running at 79% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 267, avg 7.02, max 12.65): <2: 22  2-8: 136  8-14: 109  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 273 frames. Pages written per frame (4 KB): alias median 3151, p95 3175, max 3726 (12.3 / 12.4 / 14.6 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 15, max 16 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2302, p95 2324, max 2610 (9.0 / 9.1 / 10.2 MB); alias handed to the GPU median 847, p95 860, max 1116 (3.3 / 3.4 / 4.4 MB); 3877 nvmap allocations known; finding them: avg 9.97 ms, max 15.85 ms; 6 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6086400000 19337, 0x6087500000 17802, 0x6060a00000 15895, 0x6060c00000 15134, 0x60a3d00000 14469, 0x6086500000 14420, 0x6060900000 13521, 0x6087400000 13112
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.0, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (27 slots, 9 finalized); 1401 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 79 of 276 ticks (29%); kernel busy at 5 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 70/0/30/0), 19 (p43, c2: 10/0/85/5), 3 (p36, c-2: 3/0/0/97), 4 (p36, c-2: 3/0/0/97), 14 (p37, c-2: 1/0/0/99), 49 (p43, c2: 0/89/10/0), 2 (p29, c-2: 0/0/0/100), 5 (p51, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 70% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 277, avg 7.74, max 12.09): <2: 4  2-8: 162  8-14: 111  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 293 frames. Pages written per frame (4 KB): alias median 3156, p95 3175, max 3561 (12.3 / 12.4 / 13.9 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 15, max 16 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2305, p95 2324, max 2579 (9.0 / 9.1 / 10.1 MB); alias handed to the GPU median 850, p95 861, max 982 (3.3 / 3.4 / 3.8 MB); 3877 nvmap allocations known; finding them: avg 9.31 ms, max 12.53 ms; 1 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6086400000 20354, 0x6087500000 18963, 0x6060c00000 17755, 0x6060a00000 17479, 0x60a3d00000 15529, 0x6086500000 15141, 0x6060900000 14846, 0x6087400000 13964
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.0, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (27 slots, 9 finalized); 1402 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 99 of 292 ticks (34%); kernel busy at 1 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 62/0/38/0), 19 (p43, c2: 14/0/78/8), 3 (p36, c-2: 2/0/0/98), 4 (p36, c-2: 2/0/0/98), 14 (p37, c-2: 0/0/0/100), 2 (p29, c-2: 0/0/0/100), 5 (p51, c-2: 0/0/0/100), 6 (p51, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 62% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 291, avg 7.23, max 12.01): <2: 15  2-8: 200  8-14: 76  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[status] 314 s into nnMain; GPU: 17181 submissions, 722507 command lists, 7720172 draws, 117608 clears, 136916 compute launches (136916 run, 192 GPU-resident buffers made, 15 given up); 8584 frames presented
[status] renderer: 7680538 draws drawn, 39641 skipped (39641 shader, 0 topology, 0 target); 667 shaders (0 failed), 1101 textures (249 unpacked); 2759 ms watching writes; 4 buffers dropped after unmaps
[status] first draws with a shader: 470, 0 of them over 4 ms (0 ms in all); longest 0.1 ms (vs_02fe5c225f10059a)
[status] memory: the game 6014 MB; the PC 0.8 of 7.7 GB free; graphics memory 495 MB (budget 3556) + 0 MB shared (budget 0); textures 1057 held, 198 MB of 1024 MB allowed, 0 dropped, 103 made smaller (about 60 MB left out)
[status] shaders: 667 made, 159 let go for being idle, 508 with an object now
[status] graphics: 43 images drawn to, 30 MB (0 of 16 MB or more, the biggest 3 MB); 6 buffers kept, 2 MB
[status] frameskip: the game made 8585 frames, 0 left out (0%), 0 draws
[status] fibers: 1402 started, 27 host fibers (9 finalized, kept for the next ones)
[rbprobe] 5.0 s, 279 frames. Pages written per frame (4 KB): alias median 3153, p95 3172, max 3550 (12.3 / 12.4 / 13.9 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 15, max 15 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2302, p95 2321, max 2567 (9.0 / 9.1 / 10.0 MB); alias handed to the GPU median 848, p95 860, max 983 (3.3 / 3.4 / 3.8 MB); 3877 nvmap allocations known; finding them: avg 9.71 ms, max 12.71 ms; 1 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6086400000 19382, 0x6087500000 18060, 0x6060a00000 16508, 0x6060c00000 15474, 0x60a3d00000 14787, 0x6086500000 14420, 0x6060900000 13991, 0x6087400000 13299
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.0, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (27 slots, 9 finalized); 1402 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 55 of 280 ticks (20%); kernel busy at 4 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 79/0/21/0), 19 (p43, c2: 18/0/66/16), 3 (p36, c-2: 11/0/0/89), 4 (p36, c-2: 6/0/0/94), 13 (p44, c-2: 1/0/0/99), 12 (p44, c-2: 1/0/0/99), 49 (p43, c2: 1/92/7/0), 21 (p29, c2: 0/100/0/0)
[rbprobe] thread 1 (running at 79% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 280, avg 6.96, max 12.76): <2: 17  2-8: 158  8-14: 105  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 287 frames. Pages written per frame (4 KB): alias median 3151, p95 3171, max 3570 (12.3 / 12.4 / 13.9 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 15, max 16 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2301, p95 2320, max 2598 (9.0 / 9.1 / 10.1 MB); alias handed to the GPU median 847, p95 859, max 972 (3.3 / 3.4 / 3.8 MB); 3877 nvmap allocations known; finding them: avg 9.57 ms, max 12.96 ms; 1 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6086400000 19948, 0x6087500000 18576, 0x6060a00000 16337, 0x6060c00000 15446, 0x60a3d00000 15211, 0x6086500000 14832, 0x6060900000 14056, 0x6087400000 13679
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.0, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (27 slots, 9 finalized); 1403 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 5; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 64 of 285 ticks (22%); kernel busy at 4 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 75/0/25/0), 19 (p43, c2: 19/0/72/9), 3 (p36, c-2: 9/0/0/91), 4 (p36, c-2: 9/0/0/91), 14 (p37, c-2: 0/0/0/100), 20 (p29, c-2: 0/0/0/100), 49 (p43, c2: 0/90/9/0), 2 (p29, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 75% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 286, avg 7.26, max 10.63): <2: 7  2-8: 185  8-14: 94  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 287 frames. Pages written per frame (4 KB): alias median 3155, p95 3174, max 3581 (12.3 / 12.4 / 14.0 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 15, max 15 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2306, p95 2327, max 2611 (9.0 / 9.1 / 10.2 MB); alias handed to the GPU median 847, p95 858, max 995 (3.3 / 3.4 / 3.9 MB); 3877 nvmap allocations known; finding them: avg 9.38 ms, max 13.36 ms; 2 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6086400000 19944, 0x6087500000 18705, 0x6060a00000 17150, 0x6060c00000 16949, 0x60a3d00000 15211, 0x6086500000 14832, 0x6060900000 14561, 0x6087400000 13774
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.0, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (27 slots, 9 finalized); 1403 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 57 of 291 ticks (20%); kernel busy at 1 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 70/0/30/0), 19 (p43, c2: 29/0/62/9), 3 (p36, c-2: 4/0/0/96), 4 (p36, c-2: 4/0/0/96), 20 (p29, c-2: 1/0/0/99), 12 (p44, c-2: 1/0/0/99), 13 (p44, c-2: 1/0/0/99), 8 (p44, c-2: 0/100/0/0)
[rbprobe] thread 1 (running at 70% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 288, avg 7.14, max 24.15): <2: 10  2-8: 208  8-14: 69  14-19: 0  19-28: 1  28-40: 0  40-60: 0  >=60: 0
[rbprobe] 5.0 s, 292 frames. Pages written per frame (4 KB): alias median 3156, p95 3175, max 3842 (12.3 / 12.4 / 15.0 MB); heap median 0, p95 0, max 0 (0.0 / 0.0 / 0.0 MB); stack median 15, p95 15, max 16 (0.1 / 0.1 / 0.1 MB); alias CPU's own median 2305, p95 2325, max 2636 (9.0 / 9.1 / 10.3 MB); alias handed to the GPU median 850, p95 860, max 1206 (3.3 / 3.4 / 4.7 MB); 3877 nvmap allocations known; finding them: avg 9.12 ms, max 11.65 ms; 4 frames left out, their pages counted with the next (renderer busy)
[rbprobe] alias regions written most (1 MB each: pages written in these 5 s, the CPU's own only): 0x6086400000 20362, 0x6087500000 18963, 0x6060c00000 17909, 0x6060a00000 17684, 0x60a3d00000 15476, 0x6086500000 15141, 0x6060900000 14896, 0x6087400000 13968
[rbprobe] heap regions written most (1 MB each: pages written in these 5 s): none
[rbprobe] pad reads per frame: avg 1.0, max 1; by 1 thread(s): 23332
[rbprobe] fibers alive: 18 (27 slots, 9 finalized); 1404 started since the game began
[rbprobe] at the vsync tick, guest threads running: median 1, max 4; waiting: median 19, max 20; sleeping: median 3, max 3; in a service request: median 1, max 3; none running at 60 of 292 ticks (21%); kernel busy at 3 ticks
[rbprobe] threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %): 1 (p44, c0: 63/0/37/0), 19 (p43, c2: 38/0/47/14), 4 (p36, c-2: 3/0/0/97), 3 (p36, c-2: 3/0/0/97), 49 (p43, c2: 1/95/4/0), 14 (p37, c-2: 0/0/0/100), 2 (p29, c-2: 0/0/0/100), 5 (p51, c-2: 0/0/0/100)
[rbprobe] thread 1 (running at 63% of the ticks) is in (it is the thread writing this report, seen while it queues a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a pad read (the game reading its input):
guest backtrace (innermost first):
  lua2cpp_gamewatch+0x8420
  main+0x3666880
  main+0x3667300
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[rbprobe] stack at a frame queued (the game handing the display a frame):
guest backtrace (innermost first):
  sdk+0x367650
  sdk+0x79a70
  sdk+0x79600
  sdk+0xe0b0
  sdk+0x15f0
  sdk+0x1ec50
  sdk+0x27220
  sdk+0x40f880
  sdk+0x42a280
  main+0x386f850
  main+0x3746e90
  main+0x2c5620
  sdk+0x29d9e0
[latency] vsync event -> game queues its frame (ms; n 295, avg 7.22, max 11.33): <2: 0  2-8: 247  8-14: 48  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
``````

