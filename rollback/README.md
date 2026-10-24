# rollback/ — núcleo de rollback netplay (experimento aparte)

Biblioteca en C (C99, sin dependencias de Windows ni de `runtime/`) para probar la idea de
**rollback netcode**: guardar el estado cada cuadro, predecir las entradas del otro jugador, y
si llega una entrada que contradice la predicción, volver al cuadro erróneo y re-simular hasta
el presente. **No está integrada en NativeSmash/WindowSmash** y no toca nada de `runtime/`.
El estudio de viabilidad, el diseño de integración y la lista de lo que falta por hacer en un
PC con el juego están en [`../docs/ROLLBACK.md`](../docs/ROLLBACK.md).

*(English: a standalone C99 rollback-netcode core with a synthetic deterministic game and a
network simulator. It is not wired into the game. See `docs/ROLLBACK.md` for the feasibility
study and integration plan.)*

## Cómo ejecutar las pruebas

Hace falta `make` y `gcc` o `clang` (Linux). Nada más.

```sh
cd rollback
make test      # compila y ejecuta todas las pruebas (unos segundos)
make bench     # mide costes: copia de estado, instantáneas por páginas, partida sintética por red simulada
make asan      # las mismas pruebas con AddressSanitizer + UBSan
CC=clang make test
```

`make test` termina con `N checks, 0 failures` y código de salida 0 si todo va bien.

## Qué hay

| Archivo | Para qué |
| :--- | :--- |
| `include/rollback.h`, `src/rollback.c` | El núcleo: anillo de estados, predicción, entradas tardías, rollback y re-simulación, sumas de verificación y detección de desincronización. El juego se ve por 4 funciones (guardar, cargar, avanzar un cuadro, suma). |
| `include/rb_net.h`, `src/rb_net.c` | Protocolo de entradas sobre datagramas (redundancia, acuses, pulso de cuadros para el ritmo, suma de verificación). El transporte son dos funciones (`send`/`recv`): ahí se enchufará más adelante `runtime/net_relay.c`. |
| `include/rb_pages.h`, `src/rb_pages.c` | Instantáneas por páginas modificadas (en lugar de copiar toda la memoria); la lista de páginas sucias la da quien llama (en Windows saldría de `GetWriteWatch`). |
| `tests/toygame.*` | "Juego" sintético determinista: hasta 4 luchadores con posición, velocidad, daño y un generador de números aleatorios. |
| `tests/simnet.*` | Red simulada: retraso fijo, jitter, pérdida, duplicados y reordenación, en tiempo virtual. |
| `tests/harness.*` | Dos sesiones jugando por la red simulada y comparadas con la misma partida sin red. |
| `tests/test_*.c` | Las pruebas. `tests/bench.c`: las mediciones. |

## Qué comprueban las pruebas

- Con entradas tardías (0–7 cuadros, también 3 jugadores) el estado final es **idéntico** al de la partida sin red.
- Una predicción acertada no cuesta ningún rollback; la ventana de predicción se respeta (se para al llenarse).
- Dos instancias por red simulada (hasta 250 ms de ida, 10 % de pérdida, 10 % de reordenación, duplicados) convergen al estado de referencia, sin falsas alarmas de desincronización.
- Una desincronización forzada (un valor corrompido en un solo jugador) y el error clásico "estado que la simulación lee pero no se guarda" se **detectan** con las sumas de verificación.
- El protocolo rechaza paquetes mal formados, de otra sesión o de remitente incorrecto.
- Las instantáneas por páginas restauran la memoria exactamente como una copia completa, también dentro de un rollback real.

Lo que **no** prueban: nada sobre el juego real (no hay binario, ni sus archivos, ni Windows en la nube).
El "juego" sintético es mucho más sencillo que el real; los tiempos medidos no son los de un portátil ni un teléfono.
