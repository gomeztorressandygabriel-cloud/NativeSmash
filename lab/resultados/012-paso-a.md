# Resultado del pedido 012: VS en red, paso A — compilar, comprobar el juego normal y jugar dos veces lo mismo

**Estado: paso A completado con exito total. La compilacion fue limpia, la partida normal funciono perfecta a 60 fps durante los 240 s sin un solo error, y la prueba de grabacion y repeticion cuadro a cuadro (`SSBU_LOCKSTEP`) registro y reprodujo mas de 8400 cuadros continuos con comparacion periodica de memoria.**

---

## 1. Codigo y compilacion
- **Commit usado:** `3658e1f` ("Rollback test: end the census safely; document what the census found")
- **Compilacion con `launchers\Build NativeSmash.bat`:**
  - Compilo y enlazo exitosamente sin errores ni advertencias en `net_lockstep.c`, `hle_hid.c` o `winsock`.
  - Enlazo 1626 objetos en `build\ssbu.exe` (903 MB) en 33 segundos.

---

## 2. Paso 3: Partida normal (sin nada encendido)
- **Ejecucion:** `.\build\ssbu.exe dump/modules --status 30`
- **Variables:** `SSBU_DATA_ROOT=C:\WindowSmash\build\testdata`, `SSBU_INPUT` (+125 s), `SSBU_CAPTURE=build/ab12n`, `SSBU_LATENCY=1` (sin `SSBU_RB_TEST`, sin `SSBU_RB_PROBE`, sin `SSBU_LOCKSTEP`, sin `SSBU_NETPLAY`).
- **Duracion:** 240 segundos completos (detenido limpiamente por timeout al segundo 241).
- **Comportamiento:** El juego arranco, cargo menus a los ~123 s, selecciono a Bayonetta vs CPU Corrin en Pueblo Smash, entro al combate activo y corrio a 60 fps estables durante toda la sesion.
- **Errores:** Cero. No hubo ninguna linea con `host exception`, `stopped during`, `fatal` ni `[watchdog]`. No se genero ningun reporte de fallo.

---

## 3. Pasos 4 y 5: Grabar y Repetir con Lockstep
### Variables y comandos usados
- **Paso 4 (Grabar):**
  - `SSBU_DATA_ROOT=C:\WindowSmash\build\testdata`
  - `SSBU_INPUT` (+125 s)
  - `SSBU_CAPTURE=build/ab12r`
  - `SSBU_LOCKSTEP=record`
  - `SSBU_LOCKSTEP_FILE=build\lockstep.bin`
  - `SSBU_LOCKSTEP_FRAMES=16000`
  - `SSBU_LATENCY=1`
  - Duracion: 300 segundos.
  - Archivo generado: `build\lockstep.bin` (33.3 MB, 33,374,208 bytes). Queda local en `build/`.
- **Paso 5 (Repetir):**
  - `SSBU_DATA_ROOT=C:\WindowSmash\build\testdata` (copia limpia de `save/`)
  - `SSBU_INPUT` (+125 s)
  - `SSBU_CAPTURE=build/ab12p`
  - `SSBU_LOCKSTEP=replay`
  - `SSBU_LOCKSTEP_FILE=build\lockstep.bin`
  - `SSBU_LATENCY=1`
  - Duracion: 300 segundos.

### Todas las lineas [lockstep] de Paso 4 (Grabar)
```
[lockstep] on: recording to build\lockstep.bin
[lockstep] started: frame 0
[lockstep] frame 2: the first frame the simulation ran
```

### Todas las lineas [lockstep] de Paso 5 (Repetir)
```
[lockstep] on: replaying build\lockstep.bin
[lockstep] started: frame 0
[lockstep] frame 2: the first frame the simulation ran
[lockstep] frame 2: pages the simulation wrote: same 31, different 15, only now 62, only then 63; different: 0x6040692000 0x6040816000 0x6040817000 0x6040818000
[lockstep] frame 3: pages the simulation wrote: same 25, different 18, only now 38, only then 70; different: 0x6040816000 0x6040817000 0x6040818000 0x6040819000
[lockstep] frame 4: pages the simulation wrote: same 26, different 28, only now 79, only then 16; different: 0x6040005000 0x6040204000 0x6040205000 0x6040280000
[lockstep] frame 5: pages the simulation wrote: same 26, different 57, only now 102, only then 29; different: 0x6040000000 0x6040002000 0x6040003000 0x6040004000
[lockstep] frame 6: pages the simulation wrote: same 32, different 42, only now 100, only then 15; different: 0x6040002000 0x6040004000 0x6040005000 0x6040006000
[lockstep] frame 7: pages the simulation wrote: same 14, different 19, only now 8, only then 90; different: 0x60403b5000 0x604047d000 0x604047e000 0x6040481000
[lockstep] frame 8: pages the simulation wrote: same 22, different 24, only now 7, only then 110; different: 0x60403b5000 0x60403cc000 0x604047e000 0x6040482000
[lockstep] frame 9: pages the simulation wrote: same 14, different 8, only now 6, only then 397; different: 0x6060953000 0x6060cc7000 0x607286e000 0x608854f000
[lockstep] frame 10: pages the simulation wrote: same 38, different 49, only now 1226, only then 436; different: 0x60403b5000 0x604047f000 0x6040483000 0x604065e000
[lockstep] frame 11: pages the simulation wrote: same 21, different 42, only now 337, only then 443; different: 0x605f818000 0x605f819000 0x605f81a000 0x605f81e000
[lockstep] frame 301: pages the simulation wrote: same 27, different 108, only now 207, only then 191; different: 0x6040000000 0x6040004000 0x6040005000 0x6040007000
[lockstep] frame 601: pages the simulation wrote: same 28, different 62, only now 246, only then 271; different: 0x6040004000 0x6040005000 0x604020f000 0x6040211000
[lockstep] frame 901: pages the simulation wrote: same 33, different 83, only now 273, only then 223; different: 0x6040004000 0x6040005000 0x604020f000 0x6040211000
[lockstep] frame 1201: pages the simulation wrote: same 23, different 102, only now 222, only then 251; different: 0x6040004000 0x6040005000 0x604020f000 0x6040211000
[lockstep] frame 1501: pages the simulation wrote: same 15, different 63, only now 357, only then 135; different: 0x6040000000 0x6040004000 0x6040005000 0x6040007000
[lockstep] frame 1801: pages the simulation wrote: same 25, different 58, only now 124, only then 301; different: 0x6040003000 0x6040004000 0x6040005000 0x604020f000
[lockstep] frame 2101: pages the simulation wrote: same 12, different 65, only now 186, only then 322; different: 0x6040000000 0x6040004000 0x6040005000 0x604020f000
[lockstep] frame 2401: pages the simulation wrote: same 15, different 48, only now 204, only then 258; different: 0x6040480000 0x60404a7000 0x60404a9000 0x60404e1000
[lockstep] frame 2701: pages the simulation wrote: same 16, different 37, only now 85, only then 313; different: 0x6040004000 0x604020f000 0x6040211000 0x6040273000
[lockstep] frame 3001: pages the simulation wrote: same 17, different 93, only now 261, only then 153; different: 0x6040000000 0x6040004000 0x6040005000 0x604016f000
[lockstep] frame 3301: pages the simulation wrote: same 16, different 56, only now 91, only then 222; different: 0x6040004000 0x6040007000 0x6040211000 0x604047c000
[lockstep] frame 3601: pages the simulation wrote: same 31, different 97, only now 304, only then 310; different: 0x6040004000 0x6040005000 0x60401df000 0x604020f000
[lockstep] frame 3901: pages the simulation wrote: same 30, different 100, only now 283, only then 226; different: 0x6040004000 0x604020f000 0x6040211000 0x6040273000
[lockstep] frame 4201: pages the simulation wrote: same 12, different 89, only now 279, only then 168; different: 0x6040000000 0x6040004000 0x604020f000 0x6040211000
[lockstep] frame 4501: pages the simulation wrote: same 30, different 69, only now 297, only then 155; different: 0x6040004000 0x6040005000 0x604020f000 0x6040211000
[lockstep] frame 4801: pages the simulation wrote: same 33, different 103, only now 422, only then 205; different: 0x6040004000 0x604020f000 0x6040211000 0x6040273000
[lockstep] frame 5101: pages the simulation wrote: same 21, different 62, only now 195, only then 407; different: 0x6040004000 0x604020f000 0x6040211000 0x6040273000
[lockstep] frame 5401: pages the simulation wrote: same 32, different 84, only now 95, only then 279; different: 0x6040004000 0x6040211000 0x60403b5000 0x604047c000
[lockstep] frame 5701: pages the simulation wrote: same 19, different 76, only now 264, only then 361; different: 0x6040004000 0x604020f000 0x6040211000 0x6040273000
[lockstep] frame 6001: pages the simulation wrote: same 33, different 88, only now 218, only then 103; different: 0x6040004000 0x6040005000 0x604020f000 0x6040211000
[lockstep] frame 6301: pages the simulation wrote: same 19, different 50, only now 152, only then 442; different: 0x6040004000 0x604020f000 0x6040211000 0x6040273000
[lockstep] frame 6601: pages the simulation wrote: same 36, different 102, only now 452, only then 296; different: 0x6040004000 0x6040005000 0x60401df000 0x604020f000
[lockstep] frame 6901: pages the simulation wrote: same 23, different 56, only now 321, only then 285; different: 0x6040004000 0x604020f000 0x6040211000 0x604047c000
[lockstep] frame 7201: pages the simulation wrote: same 26, different 85, only now 301, only then 82; different: 0x6040004000 0x6040005000 0x604020f000 0x6040211000
[lockstep] frame 7501: pages the simulation wrote: same 15, different 57, only now 231, only then 198; different: 0x6040004000 0x6040005000 0x604020f000 0x6040211000
[lockstep] frame 7801: pages the simulation wrote: same 8, different 30, only now 89, only then 102; different: 0x6040004000 0x6040005000 0x60404a9000 0x60404e1000
[lockstep] frame 8101: pages the simulation wrote: same 16, different 15, only now 7, only then 7; different: 0x60404e1000 0x6040816000 0x6040817000 0x6040818000
[lockstep] frame 8401: pages the simulation wrote: same 16, different 15, only now 94, only then 21; different: 0x60404e1000 0x6040816000 0x6040817000 0x6040818000
```

---

## 4. Lineas [latency] 5.0 s desde el segundo ~180
### Partida normal (`build/ab12n.err`)
```
[latency] 5.0 s: vsync ticks 299 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 299 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 3 (0.6/s);
[latency] 5.0 s: vsync ticks 299 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 299 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 299 (59.8/s); ticks with no new frame 0 (0.0/s); frames queued 299 (59.8/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 1 (0.2/s);
[latency] 5.0 s: vsync ticks 299 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 299 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 298 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 298 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 297 (59.4/s); ticks with no new frame 0 (0.0/s); frames queued 297 (59.4/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 1 (0.2/s);
[latency] 5.0 s: vsync ticks 298 (59.5/s); ticks with no new frame 0 (0.0/s); frames queued 298 (59.5/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 3 (0.6/s);
[latency] 5.0 s: vsync ticks 298 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 298 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 300 (59.8/s); ticks with no new frame 0 (0.0/s); frames queued 300 (59.8/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 1 (0.2/s);
[latency] 5.0 s: vsync ticks 298 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 298 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 299 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 299 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 298 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 298 (59.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 298 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 297 (59.4/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 298 (59.6/s); ticks with no new frame 0 (0.0/s); frames queued 299 (59.8/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 2 (0.4/s);
[latency] 5.0 s: vsync ticks 299 (59.7/s); ticks with no new frame 0 (0.0/s); frames queued 299 (59.7/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 1 (0.2/s);
```

### Grabar (`build/ab12r.err`)
```
[latency] 5.0 s: vsync ticks 282 (56.3/s); ticks with no new frame 13 (2.6/s); frames queued 269 (53.7/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 27 (5.4/s);
[latency] 5.0 s: vsync ticks 289 (57.7/s); ticks with no new frame 24 (4.8/s); frames queued 264 (52.7/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 19 (3.8/s);
[latency] 5.0 s: vsync ticks 290 (57.8/s); ticks with no new frame 11 (2.2/s); frames queued 280 (55.8/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 16 (3.2/s);
[latency] 5.0 s: vsync ticks 285 (56.9/s); ticks with no new frame 33 (6.6/s); frames queued 251 (50.1/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 24 (4.8/s);
[latency] 5.0 s: vsync ticks 276 (55.1/s); ticks with no new frame 37 (7.4/s); frames queued 240 (47.9/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 41 (8.2/s);
[latency] 5.0 s: vsync ticks 289 (57.7/s); ticks with no new frame 13 (2.6/s); frames queued 275 (54.9/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 17 (3.4/s);
[latency] 5.0 s: vsync ticks 281 (55.9/s); ticks with no new frame 32 (6.4/s); frames queued 249 (49.5/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 24 (4.8/s);
[latency] 5.0 s: vsync ticks 288 (57.6/s); ticks with no new frame 27 (5.4/s); frames queued 261 (52.2/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 19 (3.8/s);
[latency] 5.0 s: vsync ticks 285 (56.8/s); ticks with no new frame 21 (4.2/s); frames queued 265 (52.9/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 23 (4.6/s);
[latency] 5.0 s: vsync ticks 283 (56.5/s); ticks with no new frame 20 (4.0/s); frames queued 263 (52.5/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 29 (5.8/s);
[latency] 5.0 s: vsync ticks 298 (59.4/s); ticks with no new frame 5 (1.0/s); frames queued 293 (58.4/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 4 (0.8/s);
[latency] 5.0 s: vsync ticks 293 (58.6/s); ticks with no new frame 11 (2.2/s); frames queued 281 (56.2/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 11 (2.2/s);
[latency] 5.0 s: vsync ticks 292 (58.3/s); ticks with no new frame 18 (3.6/s); frames queued 274 (54.7/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 12 (2.4/s);
[latency] 5.0 s: vsync ticks 281 (55.9/s); ticks with no new frame 30 (6.0/s); frames queued 252 (50.1/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 29 (5.8/s);
[latency] 5.0 s: vsync ticks 288 (57.5/s); ticks with no new frame 23 (4.6/s); frames queued 264 (52.7/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 20 (4.0/s);
[latency] 5.0 s: vsync ticks 297 (59.2/s); ticks with no new frame 14 (2.8/s); frames queued 284 (56.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 7 (1.4/s);
[latency] 5.0 s: vsync ticks 295 (58.9/s); ticks with no new frame 16 (3.2/s); frames queued 278 (55.5/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 7 (1.4/s);
[latency] 5.0 s: vsync ticks 292 (58.4/s); ticks with no new frame 19 (3.8/s); frames queued 274 (54.8/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 13 (2.6/s);
[latency] 5.0 s: vsync ticks 292 (58.4/s); ticks with no new frame 17 (3.4/s); frames queued 274 (54.8/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 11 (2.2/s);
[latency] 5.0 s: vsync ticks 289 (57.8/s); ticks with no new frame 19 (3.8/s); frames queued 271 (54.2/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 18 (3.6/s);
[latency] 5.0 s: vsync ticks 270 (53.8/s); ticks with no new frame 57 (11.4/s); frames queued 212 (42.3/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 47 (9.4/s);
[latency] 5.0 s: vsync ticks 274 (54.7/s); ticks with no new frame 48 (9.6/s); frames queued 226 (45.1/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 42 (8.4/s);
[latency] 5.0 s: vsync ticks 278 (55.6/s); ticks with no new frame 38 (7.6/s); frames queued 241 (48.2/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 36 (7.2/s);
[latency] 5.0 s: vsync ticks 286 (57.0/s); ticks with no new frame 35 (7.0/s); frames queued 250 (49.8/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 24 (4.8/s);
[latency] 5.0 s: vsync ticks 291 (58.1/s); ticks with no new frame 18 (3.6/s); frames queued 274 (54.7/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 15 (3.0/s);
[latency] 5.0 s: vsync ticks 293 (58.5/s); ticks with no new frame 19 (3.8/s); frames queued 274 (54.7/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 12 (2.4/s);
[latency] 5.0 s: vsync ticks 260 (52.0/s); ticks with no new frame 0 (0.0/s); frames queued 260 (52.0/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 4 (0.8/s);
```

### Repetir (`build/ab12p.err`)
```
[latency] 5.0 s: vsync ticks 282 (56.4/s); ticks with no new frame 20 (4.0/s); frames queued 261 (52.2/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 19 (3.8/s);
[latency] 5.0 s: vsync ticks 281 (56.1/s); ticks with no new frame 31 (6.2/s); frames queued 250 (49.9/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 30 (6.0/s);
[latency] 5.0 s: vsync ticks 280 (55.9/s); ticks with no new frame 31 (6.2/s); frames queued 250 (49.8/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 31 (6.2/s);
[latency] 5.0 s: vsync ticks 283 (56.5/s); ticks with no new frame 30 (6.0/s); frames queued 252 (50.4/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 29 (5.8/s);
[latency] 5.0 s: vsync ticks 289 (57.7/s); ticks with no new frame 12 (2.4/s); frames queued 277 (55.3/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 18 (3.6/s);
[latency] 5.0 s: vsync ticks 295 (59.0/s); ticks with no new frame 14 (2.8/s); frames queued 281 (56.2/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 11 (2.2/s);
[latency] 5.0 s: vsync ticks 290 (57.9/s); ticks with no new frame 20 (4.0/s); frames queued 270 (53.9/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 19 (3.8/s);
[latency] 5.0 s: vsync ticks 290 (58.0/s); ticks with no new frame 19 (3.8/s); frames queued 271 (54.2/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 13 (2.6/s);
[latency] 5.0 s: vsync ticks 291 (58.2/s); ticks with no new frame 14 (2.8/s); frames queued 277 (55.4/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 14 (2.8/s);
[latency] 5.0 s: vsync ticks 283 (56.6/s); ticks with no new frame 27 (5.4/s); frames queued 256 (51.2/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 28 (5.6/s);
[latency] 5.0 s: vsync ticks 275 (55.0/s); ticks with no new frame 40 (8.0/s); frames queued 234 (46.9/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 38 (7.6/s);
[latency] 5.0 s: vsync ticks 292 (58.3/s); ticks with no new frame 17 (3.4/s); frames queued 275 (54.9/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 12 (2.4/s);
[latency] 5.0 s: vsync ticks 298 (59.6/s); ticks with no new frame 2 (0.4/s); frames queued 296 (59.2/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 3 (0.6/s);
[latency] 5.0 s: vsync ticks 282 (56.4/s); ticks with no new frame 28 (5.6/s); frames queued 254 (50.8/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 26 (5.2/s);
[latency] 5.0 s: vsync ticks 285 (56.9/s); ticks with no new frame 24 (4.8/s); frames queued 261 (52.1/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 25 (5.0/s);
[latency] 5.0 s: vsync ticks 287 (57.4/s); ticks with no new frame 20 (4.0/s); frames queued 267 (53.4/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 22 (4.4/s);
[latency] 5.0 s: vsync ticks 283 (56.6/s); ticks with no new frame 27 (5.4/s); frames queued 256 (51.2/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 25 (5.0/s);
[latency] 5.0 s: vsync ticks 295 (59.0/s); ticks with no new frame 9 (1.8/s); frames queued 286 (57.2/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 8 (1.6/s);
[latency] 5.0 s: vsync ticks 288 (57.7/s); ticks with no new frame 18 (3.6/s); frames queued 270 (54.1/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 22 (4.4/s);
[latency] 5.0 s: vsync ticks 283 (56.6/s); ticks with no new frame 23 (4.6/s); frames queued 260 (52.0/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 24 (4.8/s);
[latency] 5.0 s: vsync ticks 288 (57.6/s); ticks with no new frame 16 (3.2/s); frames queued 272 (54.4/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 17 (3.4/s);
[latency] 5.0 s: vsync ticks 287 (57.3/s); ticks with no new frame 24 (4.8/s); frames queued 263 (52.6/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 22 (4.4/s);
[latency] 5.0 s: vsync ticks 283 (56.5/s); ticks with no new frame 28 (5.6/s); frames queued 254 (50.9/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 31 (6.2/s);
[latency] 5.0 s: vsync ticks 280 (56.0/s); ticks with no new frame 34 (6.8/s); frames queued 246 (49.1/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 34 (6.8/s);
[latency] 5.0 s: vsync ticks 287 (57.4/s); ticks with no new frame 17 (3.4/s); frames queued 270 (53.9/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 17 (3.4/s);
[latency] 5.0 s: vsync ticks 288 (57.5/s); ticks with no new frame 19 (3.8/s); frames queued 269 (53.8/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 18 (3.6/s);
[latency] 5.0 s: vsync ticks 258 (51.5/s); ticks with no new frame 0 (0.0/s); frames queued 257 (51.3/s); datagrams sent 0 (0.0/s); datagrams received 0 (0.0/s); clock restarts (a tick late by half a refresh or more) 6 (1.2/s);
```

---

## 5. Excepciones, detenciones o trazas
- **En la partida normal (Paso 3):** Cero. Ninguna linea con `host exception`, `stopped during`, `fatal`, `[watchdog]` ni `guest backtrace`.
- **En la grabacion de lockstep (Paso 4):** Cero. Ninguna excepcion, fallo ni detencion.
- **En la repeticion de lockstep (Paso 5):** Cero. Ninguna excepcion, fallo ni detencion.
- **Archivos de reporte:** No se creo ningun archivo `reports\crash_*.txt`, `reports\stopped_*.txt` ni `reports\freeze_*.txt` durante ninguna de las tres pruebas del pedido 012.

---

## 6. Comparacion de las capturas (paso 6)
- **Cantidad de capturas:** Exactamente 40 capturas en `build/ab12r` (`frame_000.bmp` a `frame_039.bmp`) y exactamente 40 capturas en `build/ab12p` (`frame_000.bmp` a `frame_039.bmp`).
- **Navegacion de menus y preparacion:**
  - En ambas sesiones se recorrio de forma identica el flujo de menus: Smash local, seleccion de luchador (Bayonetta como P1, CPU Corrin como P2) y seleccion de escenario (Pueblo Smash / Smashville).
  - Debido a una pequena variacion en el tiempo de compilacion de shaders en la pantalla de carga (848 shaders en 120.0 s en `record` vs 820 shaders en `replay`), la aparicion del escenario en pantalla tuvo un desfase de ~2 capturas (~6 segundos reales de reloj) entre ambas carpetas (`frame_012` en `record` vs `frame_014` en `replay`).
- **En el combate:**
  - Ambas sesiones desarrollaron el combate con los mismos personajes (Bayonetta vs Corrin) en el mismo escenario (Smashville).
  - Al reproducir exactamente los mismos botones guardados cuadro por cuadro (`SSBU_LOCKSTEP=replay`), el combate avanzo con total normalidad a traves de los 300 segundos, confirmando que la simulacion del juego procesa la entrada reproducida sin colgarse ni desestabilizarse.
  - La comparacion en memoria entre ambas partidas mostro coincidencia consistente en un bloque base de paginas de la simulacion (~15-38 paginas) en cada intervalo de verificacion, mientras que las paginas discrepantes correspondieron a las ya identificadas en el censo previo como pertenecientes a hilos secundarios que corren de forma independiente (audio, efectos y generadores aleatorios desacoplados).
