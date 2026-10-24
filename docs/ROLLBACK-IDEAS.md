# Ideas para meter el rollback en NativeSmash

[← Estudio de viabilidad](ROLLBACK.md)

Este documento sale de leer el runtime con el rollback en mente. Primero: lo que el código dice, con el archivo donde se ve.
Después: un diseño concreto, sus piezas, las alternativas descartadas y el orden de trabajo. **Son ideas para discutir, no
un plan cerrado**: varias dependen de mediciones que solo se pueden hacer con el juego (la sonda de [ROLLBACK.md §5](ROLLBACK.md#la-sonda-de-la-fase-1-rollback_probeon)).

## 1. Lo que dice el código

| Hecho | Dónde | Qué significa para el rollback |
| :--- | :--- | :--- |
| Cada función del juego es una función de C que recibe `Cpu *` (los registros del juego). La memoria del juego son direcciones normales del proceso. | `runtime/recomp.h` | Guardar la memoria del juego es copiar memoria del proceso; no hay un emulador en medio. |
| Una llamada del juego es una llamada de C. Los registros x19–x28 viven en variables locales de C, en registros y pila del **host**. | `tools/regcache.py` | Una llamada a medias (una función que aún no ha vuelto) tiene parte de su estado fuera de la memoria del juego. **No se puede "volver atrás" en mitad de una llamada copiando solo memoria del juego.** |
| Cada fibra del juego corre en una fibra del host con **su propia pila del host** (64 MB reservados) y su propio `Cpu`. | `runtime/hle_fiber.c` (`CreateFiberEx(64 << 10, 64 << 20, …)`) | Las corrutinas de los luchadores (guiones) suspendidas guardan su punto de continuación en esas pilas. Para restaurar el combate hay que restaurar también esas pilas. |
| En Linux, Android y macOS la conmutación de fibras es código propio (`rt_fiber_switch`, pila con `mmap`, puntero de pila guardado en `Fiber.sp`). En Windows son fibras de Windows. | `runtime/linux/win32.c`, `runtime/macos/fiber_arm64.S`, `runtime/android/` | Fuera de Windows sabemos exactamente dónde está cada pila y dónde se paró: copiar `[sp, tope)` la guarda. En Windows habría que pasar a la misma implementación propia (o copiar estructuras internas no documentadas). |
| Los hilos del juego son objetos `KThread` del kernel propio, con su estado de espera (mutex, condición, árbitro, `WaitSynchronization`). | `runtime/kernel.c` | Se puede saber en qué momento están todos parados esperando. La sonda ya lo mide ([§3](#3-la-medición-que-falta-hay-un-momento-quieto)). |
| El juego lee el mando por `npad_state()`; el vsync lo marca un hilo propio que despierta al juego 60 veces por segundo; el juego entrega cada cuadro con `QUEUE_BUFFER`. | `runtime/hle_hid.c`, `runtime/ipc_vi.c` | Tenemos en la mano los tres puntos que importan: la entrada, el "empieza un cuadro" y el "acabó un cuadro". |
| El retraso de entrada en red ya se controla (`HLE_WRAP(71016ccae0)`, `online_delay.txt`). | `runtime/hle_net.c` | Es la referencia que el rollback debe mejorar. |
| El tiempo del juego sale de `rt_ticks()`; los aleatorios "seguros" de `GenerateCryptographicallyRandomBytes`. | `runtime/host.c`, `runtime/hle_offline.c` | Los dos se pueden hacer deterministas desde el runtime sin tocar el juego. |

## 2. El diseño que propongo: envolver la función de "actualizar un cuadro"

La idea central sale del segundo y tercer hecho de la tabla. Si no se puede volver atrás en mitad de una llamada, **se vuelve atrás
siempre en el mismo sitio: justo antes de llamar a la función del juego que simula un cuadro**. En ese punto, el que llama
(el bucle principal del juego) está siempre igual, y todo lo que la función deja de un cuadro a otro está en la memoria del juego
o en las fibras.

```
bucle principal del juego (no lo tocamos)
   └─ actualizar_cuadro(this)       ← HLE_WRAP: aquí vive el rollback
         1. ¿llegó una entrada que contradice lo predicho para el cuadro k?
               → restaurar el estado guardado del cuadro k
               → para f = k … actual-1:  fijar entradas de f;  orig(this)  (re-simulación, sin sonido)
         2. fijar las entradas del cuadro actual (la local y la predicha del rival)
         3. guardar el estado del cuadro actual (páginas escritas + fibras)
         4. orig(this)                (el cuadro normal)
   └─ dibujar(...)                   ← solo se dibuja el resultado final
```

Ventajas de este diseño:

- **El dibujo no se repite**: la re-simulación solo llama a la función de actualizar, no a la de dibujar. La GPU no ve nada.
- **No hay que entender el netcode del juego (Pia)**: si se juega como VS local con el mando del rival virtual (opción A del estudio), el
  juego cree que hay dos mandos en la misma consola.
- **El núcleo `rollback/` ya hace los pasos 1–4** (`rb_tick`, `rb_add_remote_input`, `RbGame{save, load, advance, checksum}`): `advance`
  sería `orig(this)`, `save`/`load` las piezas de la sección siguiente.

Lo que exige: **encontrar `actualizar_cuadro`**, la función del juego que avanza la simulación una vez por cuadro y que no dibuja. Es la
tarea nº 3 de [ROLLBACK.md §5](ROLLBACK.md#5-qué-hacer-después-en-un-pc-con-el-juego-lista-priorizada) y no se puede hacer sin el juego.

### Qué guardar y qué no

| Parte | ¿Guardar? | Cómo |
| :--- | :--- | :--- |
| Heap del juego | Sí | Páginas escritas desde el último guardado (vigilancia de escrituras, `rollback/src/rb_pages.c`). La sonda mide cuántas son. |
| Fibras: tabla `fibs[]`/`spare[]`, cada `Cpu`, la parte usada de cada pila del host | Sí | En Linux/Android/macOS: copiar `[Fiber.sp, tope)` de cada fibra suspendida; normalmente unos KB. **No borrar** una fibra finalizada mientras pueda hacer falta en un rollback (retrasar su `DeleteFiber` `max_prediction` cuadros). |
| Pila del hilo principal (zona `STACK`) | **No** | Son los marcos del bucle principal: en el punto de envoltura están igual, y restaurarlos desharía contadores del propio bucle. |
| Zona `ALIAS` (memoria de la GPU) | Probablemente no | Solo si la sonda muestra que la simulación escribe ahí. |
| Objetos del kernel (eventos, mutex) | Probablemente no | En el punto de envoltura no debería haber esperas a medias de la simulación; comprobarlo con la sonda. |
| Reloj (`rt_ticks`) | Virtualizar | Mientras corre `actualizar_cuadro`, devolver `inicio + cuadro × (19 200 000 / 60)`: igual en ambas máquinas y en la re-simulación. |
| Aleatorios de `GenerateCryptographicallyRandomBytes` | Virtualizar | Durante el combate, sacarlos de una semilla acordada por la red. |

### Entradas

`npad_state()` devolvería, para el mando de cada jugador, **la entrada del cuadro que se está simulando** (no la del mando físico). El
envoltorio fija "cuadro actual" antes de cada `orig(this)`. Riesgo: si el juego lee los mandos en **otro hilo** y los deja en un búfer, hay
que escribir en ese búfer en lugar de en `npad_state`. La sonda dice cuántos hilos leen el mando.

### Sonido, vibración, efectos

- **Vibración:** no enviar a los mandos durante la re-simulación (`hle_hid.c`). Fácil.
- **Sonido:** el motor de audio del juego vive en el heap y lo mezcla otro hilo. Si se restaura el heap, también se restaura el estado del
  audio (bien para el determinismo), pero hay que evitar que el hilo de audio lea memoria a medias de restaurar: restaurar solo mientras ese
  hilo espera. Para no oír sonidos duplicados al re-simular, lo más limpio es anular la función del juego que dispara un sonido mientras
  `resimulating` (hay que encontrarla).
- **Partículas y efectos visuales:** se recalculan en la re-simulación y se dibuja el resultado final; no deberían duplicarse.

## 3. La medición que falta: ¿hay un momento quieto?

Todo lo anterior supone que, al empezar `actualizar_cuadro`, **los demás hilos del juego (dibujo, audio, carga, trabajadores) no están
tocando el estado de la simulación**. Si alguno lo lee a la vez, restaurar el heap bajo sus pies puede colgar el juego.

La sonda (PR #4) ahora mide esto: justo antes de que el hilo de vsync despierte al juego, cuenta cuántos hilos del juego **no** están
esperando. Línea nueva en el log:

```
[rbprobe] at the vsync tick, guest threads not waiting: median 0, p95 1, max 2; none at 280 of 300 ticks (93%); kernel busy at 0 ticks
```

- Si "none at" está cerca del 100 %, existe un momento quieto por cuadro: buen pronóstico.
- Si siempre hay hilos activos, hay que saber cuáles (dibujo, audio) y si tocan el estado de la simulación; el diseño se complica.
- Un hilo que duerme (`SleepThread`) cuenta como activo: los números son prudentes.

## 4. Alternativas que consideré

| Alternativa | Por qué no la propongo primero |
| :--- | :--- |
| **Guardar el proceso entero** (como un *savestate* de emulador) en el momento quieto | Incluye el estado del renderer, de los servicios, de la GPU, de los hilos del host… Ni yuzu ni Ryujinx llegaron a tener *savestates* fiables de Switch por esto. |
| **Puentear la sincronización de Pia** (opción B del estudio) y re-simular dentro del modo inalámbrico | Exige entender cómo Pia entrega las entradas del rival a la simulación, sin símbolos. Es mejor para el final (menús, elección de personajes y semilla ya sincronizados por el juego), pero muy caro como primer paso. |
| **`fork()` como guardado barato** (copia al escribir del sistema) | Solo copia el hilo que llama; el juego tiene muchos hilos. No sirve. |
| **Solo bajar el retraso de entrada** | Ya existe (`online_delay.txt`). Es la referencia, no el objetivo. |

### El arranque sincronizado: el problema que queda abierto

Con el "VS local con mando virtual", los dos PC tienen que empezar el combate **en el mismo estado exacto** (personajes, escenario, reglas,
semilla). Ideas, de más fácil a más robusta:

1. **Guion de menús idéntico en los dos PC**: ambos navegan el menú VS con las mismas entradas (el runtime ya tiene guiones de entrada,
   `SSBU_INPUT`) y la semilla se fija por la red. Sirve para las pruebas; frágil para jugar de verdad.
2. **Menús en "lockstep" con retraso** (como ahora), y rollback **solo dentro del combate**: los menús toleran retraso.
3. **Usar el modo inalámbrico del juego para los menús** y cambiar a rollback al empezar el combate (necesita la opción B parcialmente).

## 5. Orden de trabajo propuesto

| Paso | Dónde | Qué | Se sabe que funciona cuando… |
| :--- | :--- | :--- | :--- |
| 1 | PC | Sonda (`rollback_probe.on`): páginas por cuadro, lecturas de mando, hilos, fibras, **momento quieto** | Hay números de un combate real |
| 2 | PC | Encontrar `actualizar_cuadro` (perfilador `SSBU_HITCH`, F6) | Se tiene su dirección y firma, y se confirma que no dibuja |
| 3 | Nube | Medir fibras por dentro en Linux: cuántos KB de pila usa cada fibra suspendida (ampliar la sonda) | — |
| 4 | Nube + PC | **Prueba de "volver atrás" en un solo PC**: con la sonda en modo prueba, guardar en el cuadro N (heap + fibras), seguir 60 cuadros, restaurar, repetir esos 60 con las mismas entradas grabadas y comparar la suma del heap | Sumas idénticas 100 veces seguidas. **Es la puerta de todo el proyecto** |
| 5 | Nube + PC | Tiempo y aleatorios virtualizados; entrada por cuadro en `npad_state` | El paso 4 sigue pasando con entradas que cambian |
| 6 | Nube + PC | Envoltorio con el núcleo `rollback/` y el transporte de `net_relay.c`, dos instancias en un PC (`tools/dev/netpair.sh`) | Dos ventanas jugando con 100 ms simulados sin desincronizarse |
| 7 | PC + PC | Sonido, vibración, arranque sincronizado, interfaz | Una partida de verdad entre dos PC |

Los pasos 3–6 los puedo escribir yo desde la nube. Los pasos 1, 2 y las pruebas de cada paso necesitan el juego.
