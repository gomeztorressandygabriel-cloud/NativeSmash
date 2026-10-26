# Resultado del pedido 003: prueba de rebobinar con el juego real

**Estado: ejecutado. El juego se cerro con host exception durante el primer rebobinado (round 1, 1 cuadro).** Tal como avisaba el pedido, este fallo es un resultado directo y util de la prueba.

## Codigo y compilacion
- Commit usado: `27ece57` ("runtime: rollback rewind test (SSBU_RB_TEST): save, record, rewind, replay, compare").
- Compilacion: `launchers\Build NativeSmash.bat` compilo sin errores.
  - Al enganchar las funciones con `HLE_WRAP`, encontro correctamente la firma:
    `main: HLE_WRAP(71036149d0) is at 71036f5a70 in this version (found by its signature)`
  - Regenero codigo C para los modulos y enlazo `build\ssbu.exe` (946 MB).
  - Tiempo de compilacion: ~12 minutos y 28 segundos (~748 s).

## Comando y variables usadas
- Ejecutado con PowerShell (`Start-Process`): `.\build\ssbu.exe dump/modules --status 30`
- Variables:
  - `SSBU_DATA_ROOT=C:\WindowSmash\build\testdata` (copia limpia y aislada de `save/`)
  - `SSBU_INPUT`: guion con tiempos +125 s (Terry Bogard vs CPU Jigglypuff)
  - `SSBU_CAPTURE=build/ab3`
  - `SSBU_RB_TEST=1`
  - `SSBU_RB_TEST_AT=200`
  - `SSBU_LATENCY=1`
  - Sin `SSBU_RB_PROBE`
  - Duracion programada: 300 s (termino antes, a los ~201 s, por la excepcion).

## Lo visto en las capturas (paso 4)
- Se generaron 23 capturas (`frame_000.bmp` a `frame_022.bmp`) entre las 21:02:11 y las 21:03:17.
- La secuencia transcurrio con total normalidad hasta el combate:
  - Pantallas de carga y menu principal.
  - Seleccion de personajes y escenario.
  - Cuenta atras "2" en pantalla a las 21:02:47.
  - Combate 1 contra 1 en escenario de ciudad/azoteas (Terry vs Jigglypuff), con movimiento fluido, animaciones de golpe y acumulacion de porcentaje (Terry a 43%, Jigglypuff a 9%).
- En la ultima captura (`frame_022.bmp`, a las 21:03:17, segundo ~200 del proceso), el combate seguia en curso normal.
- Exactamente al segundo siguiente (21:03:18, segundo ~201), se disparo el primer rebobinado (round 1), la memoria fue restaurada y el juego se cerro de inmediato por excepcion de lectura de memoria.

## Lineas [rbtest]
```
[rbtest] on: rewind test rounds of 1, 10, 30 and 60 frames, the first 200 s after start
[rbtest] round 1 (1 frames): 4697 pages found written in 60 frames (18.3 MB), 4697 saved in 14.1 ms
[rbtest] round 1: recorded 1 frames; pages written outside the saved ones meanwhile: 49; fibers started meanwhile: 1; rewinding and replaying
```

## Lineas [latency] (vsync event -> game queues its frame)
```
[latency] vsync event -> game queues its frame (ms; n 2, avg 8.58, max 11.48): <2: 0  2-8: 1  8-14: 1  14-19: 0  19-28: 0  28-40: 0  40-60: 0  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 186, avg 3.16, max 40.61): <2: 92  2-8: 83  8-14: 8  14-19: 1  19-28: 1  28-40: 0  40-60: 1  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 257, avg 4.21, max 27.13): <2: 92  2-8: 126  8-14: 25  14-19: 12  19-28: 2  28-40: 0  40-60: 0  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 285, avg 4.76, max 29.40): <2: 26  2-8: 227  8-14: 28  14-19: 3  19-28: 0  28-40: 1  40-60: 0  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 280, avg 4.21, max 58.02): <2: 59  2-8: 198  8-14: 18  14-19: 3  19-28: 0  28-40: 1  40-60: 1  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 213, avg 9.93, max 277.29): <2: 57  2-8: 65  8-14: 54  14-19: 10  19-28: 15  28-40: 10  40-60: 1  >=60: 1
[latency] vsync event -> game queues its frame (ms; n 194, avg 10.79, max 203.55): <2: 27  2-8: 75  8-14: 55  14-19: 10  19-28: 11  28-40: 13  40-60: 2  >=60: 1
[latency] vsync event -> game queues its frame (ms; n 199, avg 12.33, max 124.22): <2: 10  2-8: 86  8-14: 48  14-19: 27  19-28: 13  28-40: 8  40-60: 5  >=60: 2
[latency] vsync event -> game queues its frame (ms; n 290, avg 5.31, max 21.75): <2: 5  2-8: 247  8-14: 31  14-19: 6  19-28: 1  28-40: 0  40-60: 0  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 283, avg 4.68, max 32.40): <2: 3  2-8: 256  8-14: 20  14-19: 1  19-28: 1  28-40: 2  40-60: 0  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 267, avg 7.61, max 68.40): <2: 5  2-8: 180  8-14: 63  14-19: 15  19-28: 2  28-40: 1  40-60: 0  >=60: 1
[latency] vsync event -> game queues its frame (ms; n 266, avg 7.66, max 77.30): <2: 7  2-8: 189  8-14: 43  14-19: 16  19-28: 7  28-40: 2  40-60: 1  >=60: 1
[latency] vsync event -> game queues its frame (ms; n 262, avg 7.53, max 39.99): <2: 12  2-8: 164  8-14: 67  14-19: 12  19-28: 4  28-40: 3  40-60: 0  >=60: 0
[latency] vsync event -> game queues its frame (ms; n 299, avg 5.59, max 14.42): <2: 0  2-8: 288  8-14: 10  14-19: 1  19-28: 0  28-40: 0  40-60: 0  >=60: 0
```

## Excepcion y backtrace en build\ab3.err
```
*** host exception 0xc0000005 during nnMain: reading address 0xffffffffffffffff
host pc 0x7ff761cab4ff: ssbu.exe +0x5b4ff (thread 26460, guest main thread)
  host frame ssbu.exe +0xa6341b2
  host frame ssbu.exe +0x877e5b1
  host frame ssbu.exe +0xe9e7d70
  host frame ssbu.exe +0x4da8885
  host frame ssbu.exe +0x13ae0f0
  host frame ssbu.exe +0x13961d7
  host frame ssbu.exe +0xa8753d
  host frame ssbu.exe +0x119ed45
  host frame ssbu.exe +0xa9ecbb
  host frame ssbu.exe +0x30c5c77
  host frame ssbu.exe +0x308048f
  host frame ssbu.exe +0x30879e8
  host frame ssbu.exe +0x3c9b871
  host frame ssbu.exe +0x3c9b4fb
  host frame ssbu.exe +0x8de977a
  host frame ssbu.exe +0x55a2370
  host frame ssbu.exe +0x8de977a
  host frame ssbu.exe +0x55a2370
  host frame ssbu.exe +0x8de977a
  host frame ssbu.exe +0x853f7
  host frame ssbu.exe +0x8e66dff
  host frame ssbu.exe +0x7fbf0f
  host frame ssbu.exe +0x78b00
  host frame ssbu.exe +0xa4ec97e
guest backtrace (innermost first):
  lua2cpp_item+0xe7c70
  sdk+0x32f170
  main+0x353c670
  lua2cpp_common+0x52df70
  main+0x1f44910
  main+0x6e4ac0
  main+0x6df9f0
  main+0x3a84e0
  main+0x616580
  main+0x3ad190
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
report: reports\crash_20261025_210318.txt; minidump: reports\crash_20261025_210318.dmp
```

## Archivo de crash generado
- Archivo: `reports\crash_20261025_210318.txt` (minidump: `reports\crash_20261025_210318.dmp`).
- Lineas filtradas (main+, sdk+, lua2cpp, exception o fault; sin rutas personales):
```
*** host exception 0xc0000005 during nnMain: reading address 0xffffffffffffffff
  lua2cpp_item+0xe7c70
  sdk+0x32f170
  main+0x353c670
  lua2cpp_common+0x52df70
  main+0x1f44910
  main+0x6e4ac0
  main+0x6df9f0
  main+0x3a84e0
  main+0x616580
  main+0x3ad190
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
```
