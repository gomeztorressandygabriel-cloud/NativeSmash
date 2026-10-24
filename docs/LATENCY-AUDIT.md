# Auditoría del retraso del runtime

[← README](../README.md) · [Multijugador](MULTIPLAYER.md) · [Guía técnica](TECHNICAL.md)

Qué retraso añade **nuestro runtime** (`runtime/`) entre que se pulsa un botón y se ve el resultado, y entre que el juego envía o recibe un paquete en línea y el paquete sale o llega. Los números de línea son los de la rama `cloud/latency-audit`.

> **Qué es y qué no es este informe.** Se hizo leyendo el código, sin el juego ni un compilador de Windows: **nada de lo que sigue se ha ejecutado**. Lo que se afirma del código (qué hilo espera a qué, qué cerrojo se retiene mientras se presenta) se ha leído tal cual; lo que se dice de sus *efectos* es una hipótesis hasta que el contador de la sección 5 la confirme o la descarte en un PC real. Los cambios que se hicieron son opcionales y están apagados por defecto.

## Contenido

1. [Resumen](#1-resumen)
2. [El mando](#2-el-mando-y-cuándo-lo-lee-el-juego)
3. [La salida de imagen](#3-la-salida-de-imagen)
4. [El ritmo de la simulación](#4-el-ritmo-de-la-simulación)
5. [Paquetes: envío y recepción](#5-paquetes-envío-y-recepción)
6. [Cómo medir: `latency.on`](#6-cómo-medir-latencyon)
7. [Lo que se cambió y cómo probarlo](#7-lo-que-se-cambió-y-cómo-probarlo)
8. [Lo que no se hizo y por qué](#8-lo-que-no-se-hizo-y-por-qué)
9. [Plan de pruebas en un PC](#9-plan-de-pruebas-en-un-pc)

---

## 1. Resumen

Por orden de lo que más probablemente pesa (hipótesis, a medir):

1. **`vsync = on` (el valor por defecto) presenta con intervalo 1 *dentro del cerrojo del renderizador*.** `gfx_present` entra en `enter()` (`gpu_d3d11.c:5234`), y llama a `Present(display_present_interval() = 1, …)` (`:5568`) antes de `leave()`. Ese `Present` espera al siguiente refresco del monitor, o a que haya sitio en la cola de DXGI, **con el cerrojo cogido**; el hilo gráfico, que dibuja los cuadros del juego bajo ese mismo cerrojo (16 puntos de entrada con `enter()`), se queda parado ese tiempo. Es lo único en el camino de imagen que puede convertir "el monitor va a 59,94 Hz y nuestro reloj a 60,000" en cuadros irregulares.
2. **La cola de presentación puede mantenerse llena.** `latency = 3` por defecto (`gpu_display.c:120`, `gpu_d3d11.c:850`) deja hasta 3 cuadros en la cola de DXGI; con `vsync = on` cada cuadro de la cola se enseña en un refresco distinto (no se descartan). Nuestro reloj marca 60,000 cuadros/s y un monitor de 60 Hz real refresca a 59,94: la cola crece un cuadro cada ~17 s hasta llenarse, y a partir de ahí el retraso fijo son varios cuadros y cada `Present` bloquea. Esto es un cálculo, no una medida: el contador `Present` (sección 6) lo muestra.
3. **El juego se entera de que tiene un búfer libre *después* de que el `Present` vuelva** (`ipc_vi.c:180`, antes del cambio). El refresco (evento vsync) sí se señala a tiempo (`ipc_vi.c:159`), pero el juego que espera un búfer para dibujar su siguiente cuadro (`dequeueBuffer` devuelve "esperar", `ipc_vi.c:260`) no se libera hasta que `gpu_queue_wait` + `present_frame` terminan. Un `Present` que tarda 10 ms retrasa 10 ms el inicio del cuadro siguiente del juego: cuadros de 28-36 ms y después una racha para recuperar (el juego tiene entradas y envíos pendientes). Es la explicación que mejor encaja con lo medido (~55 paquetes/s, ~15 % de intervalos largos seguidos de ráfagas).
4. **El mando no es el cuello de botella**: el estado que recibe el juego tiene como mucho ~1-2 ms más la cadencia propia del mando (sección 2).
5. **La red del runtime no añade retraso apreciable**: el envío es síncrono en el hilo del juego, la recepción llega a la cola del juego en decenas de microsegundos; lo que el juego ve como "retraso de red" es el propio cuadro (hasta 16,7 ms hasta que llama a `recvfrom`) y el retraso de entrada del juego (sección 5).

**Lo que se puede hacer ya, sin recompilar nada, en `display.ini`:** `latency = 1`, y o bien `vsync = off` (menos retraso, puede haber desgarro), o bien `pacing = display` en un monitor de 60/120/240 Hz (sin desgarro, y el reloj y el monitor dejan de "batir"). Ver la sección 9.

---

## 2. El mando y cuándo lo lee el juego

### Cómo llega el estado al juego

| Pieza | Dónde | Qué hace |
|---|---|---|
| Hilo de lectura `input` | `hle_pad.c:937` | Prioridad `ABOVE_NORMAL` (`:940`), rol `CPU_ROLE_GAME` (`cpu_sched.h`: sin ahorro de energía). Bucle: `sdl_poll` (`:712`, que llama a `SDL_UpdateGamepads`, `:715`) o `xinput_poll`, `publish` (`:526`), espera **1 ms** en un temporizador de alta resolución (`:955-956`). |
| `publish` | `hle_pad.c:526` | Copia el estado de los 8 slots bajo un `SRWLOCK` exclusivo. |
| `pad_get` | `hle_pad.c:973` | Copia un slot bajo el mismo cerrojo, compartido. |
| `npad_state` | `hle_hid.c:126` | Lo que el juego recibe por cada `GetNpadState…`: el estado **publicado** en ese momento (`pad_get`, `:131`) más el teclado y el guion de pruebas. |
| Teclado | `hle_hid.c:93` (`keyboard_buttons`), `:140` | **`GetAsyncKeyState` en el instante de la llamada**: siempre el más reciente posible. |

### ¿Es el más reciente posible?

- **Teclado: sí.** Se lee en la propia llamada del juego.
- **Mando: casi.** No hay cola ni copia "por cuadro": cada llamada del juego devuelve lo último publicado. La antigüedad máxima añadida por nosotros es el periodo del hilo (1 ms, más la imprecisión del temporizador, ~0,5 ms) + lo que tarde `SDL_UpdateGamepads` en ver el informe del mando. Después manda el propio mando: USB 1-8 ms, Bluetooth 7-15 ms o más (esto no se puede arreglar desde el runtime).
- `lat_on` añade un contador, **`pad state age`**, que mide exactamente esto: antigüedad del estado cuando el juego lo pide (sección 6). Se espera < 2 ms en casi todo.

### La idea del mod "Less-Delay"

En la Switch el mod quita la espera de sincronía vertical y *lee el mando justo antes de actualizar la escena*. Aquí la lectura ya es lo más tardía posible respecto a la llamada del juego; **lo que no controlamos es cuándo llama el juego a `GetNpadState` dentro de su cuadro**. Lo que sí controlamos es:

1. **Cuándo empieza el cuadro del juego** (el evento vsync, `ipc_vi.c:159`, y el búfer libre, `ipc_vi.c:164/180`).
2. **Cuántos cuadros terminados pueden esperar** entre el juego y la pantalla (sección 3).
3. **Cuánto tarda el cuadro en enseñarse** una vez presentado (`vsync`, `latency`, `present_wait`).

Lo que haría el equivalente a "leer lo más tarde posible" es retrasar el *inicio* del cuadro del juego para que termine justo antes del refresco (`espera = 16,7 − duración_típica_del_cuadro − margen`): así la entrada leída al principio del cuadro tiene la menor edad posible al enseñarse. **No se implementó** (sección 8): necesita la duración real del trabajo del juego por cuadro (el contador `vsync event → game queues its frame`), y un cálculo mal puesto hace que se pierdan refrescos.

---

## 3. La salida de imagen

### Cadena de presentación

- Se crea con `FLIP_DISCARD`, **2 buffers** (`gpu_d3d11.c:827`), `ALLOW_TEARING` si el sistema lo permite (`:823`), y desde este informe, opcionalmente `FRAME_LATENCY_WAITABLE_OBJECT` (`:824`).
- `latency` (`display.ini`, 1-3, **3 por defecto**, `gpu_display.c:60/120`) → `IDXGIDevice1::SetMaximumFrameLatency` (`gpu_d3d11.c:850`; en caliente, desde el menú Esc, en `:5419-5422`): cuántos cuadros puede dejar la CPU en la cola de DXGI antes de que `Present` bloquee.
- `Present(display_present_interval(), display_present_flags())` (`gpu_d3d11.c:5568`):
  - `display_present_interval()` (`gpu_display.c:607`) es **1** salvo `vsync = off` o `pacing = vrr` con desgarro permitido. Por defecto `vsync = on` → intervalo 1.
  - `display_present_flags()` añade `ALLOW_TEARING` solo con `pacing = vrr`.
- `Present` se hace dentro de `enter()`…`leave()` de `gfx_present` (`:5234` … final de la función): **el cerrojo del renderizador está cogido mientras `Present` espera**. El mismo cerrojo lo toman los puntos de entrada con los que el hilo gráfico (`gpu_queue.c`) dibuja el cuadro siguiente.

### Cuántos cuadros pueden esperar entre el juego y la pantalla

De mayor a menor cercanía al juego (cada uno es un cuadro de 16,7 ms de edad añadida si está lleno):

| Cola | Tamaño | Dónde |
|---|---|---|
| Cuadros terminados esperando al hilo vsync | los búferes del juego menos el que se enseña (el juego usa 3 en la consola: hasta 2; **no verificado aquí**, `frames waiting…` lo cuenta) | `ipc_vi.c` (`Slot`, `QUEUED`) |
| Cola de DXGI | `latency` (1-3) | `gpu_d3d11.c:850` |
| Compositor de Windows | ventana: ~1 cuadro (DWM); borderless/pantalla completa con *independent flip*: 0-1 | fuera del runtime |
| Pantalla | exploración + tiempo de respuesta | fuera del runtime |

Con los valores por defecto (`latency = 3`, `vsync = on`, ventana) el peor caso estable es de **varios cuadros** entre que el juego termina un cuadro y se ve.

### ¿Cadena con objeto de espera? ¿Presentar antes del refresco?

- **Presentar sin esperar al refresco ya existe**: `vsync = off` (intervalo 0) y `pacing = vrr` (`ALLOW_TEARING`). Es lo que más retraso quita; el precio es desgarro.
- **Cadena con objeto de espera: se implementó, opcional** (`present_wait = on`, sección 7). `Present` sigue siendo el mismo, pero antes de coger el cerrojo se espera (40 ms como máximo) a que la cadena tenga sitio; así, cuando el hilo vsync entra, `Present` vuelve enseguida y **no retiene el cerrojo mientras espera al monitor**. Con `latency = 1` además limita a un cuadro la cola de DXGI. No cambia lo que se muestra.
- Lo que **no** se puede decir sin probar: si `ResizeBuffers` mantiene válido el *handle* (se vuelve a pasar `swapchain_flags`, que incluye el indicador, `gpu_d3d11.c`, en el `ResizeBuffers` de `gfx_present`). Si dejara de serlo, la espera expira a los 40 ms y se vuelve al comportamiento anterior (más lento), no rompe nada.

---

## 4. El ritmo de la simulación

### Quién marca los cuadros del juego

El juego no tiene reloj propio: cada cuadro nace del **hilo vsync** (`ipc_vi.c:122`). Su bucle, en orden:

1. `display_wait_tick()` (`:132`, `gpu_display.c:685`): espera al siguiente instante de un reloj de 60,000 Hz (`pacing = clock`; QPC + temporizador de alta resolución, y gira sobre la CPU el último medio milisegundo, `:728`), o al refresco del monitor (`pacing = display`).
2. Coge el cuadro en cola más antiguo (`ipc_vi.c`, bajo `queue_lock`), libera el búfer que se estaba enseñando.
3. **Señala el evento vsync** (`:159`): el juego "ve" el refresco.
4. `gpu_queue_wait(gpu_mark)` (`:168`): espera a que el hilo gráfico haya ejecutado todo lo enviado antes de ese cuadro.
5. `present_frame` (`:175`) → `gfx_present` → `Present` (cola de DXGI, cerrojo del renderizador).
6. **Señala el evento de búfer libre** (`:180`, antes del cambio incondicional) → el juego puede pedir un búfer para su siguiente cuadro.

### Qué hace que los cuadros salgan irregulares

El reloj y el trabajo van **en serie en el mismo hilo**. Si 4 + 5 tardan más de lo que queda hasta el siguiente instante, el siguiente `display_wait_tick` llega tarde:

- Con el código actual, si llega **más de medio refresco tarde** (`gpu_display.c:712`), el reloj se re-ancla a "ahora" y **se pierde ese refresco** (no se recupera con una ráfaga: antes, en la versión original, sí). Cada parada de > 8,3 ms resta un refresco al ritmo medio: 60 → ~55 es compatible con unas 5 paradas por segundo.
- El juego no depende de ese retraso para *ver* el refresco (se señala antes), **pero sí para tener búfer** (punto 6): el `Present` que bloquea (por `vsync = on`, por la cola llena, o esperando al cerrojo que tiene el hilo gráfico) retrasa el inicio del cuadro siguiente del juego. Un cuadro tardío y los siguientes más seguidos (porque la cola vuelve a tener búferes) = la ráfaga observada.
- Las tres causas candidatas del retraso del hilo vsync, en orden de sospecha: `Present(1)` esperando al monitor; `gpu_queue_wait` (el hilo gráfico va por detrás: `GPU wait` en el contador); esperar al cerrojo del renderizador (`renderer lock` en el contador).
- `frameskip` (`ipc_vi.c`, ~`:320-330`; `display.ini`, apagado por defecto) no es la causa por defecto: solo actúa si está activado.
- El hilo vsync tiene prioridad normal (el proceso va en prioridad alta). Si otro hilo le quita la CPU en el último medio milisegundo del giro, el instante se pasa; `boost_threads` lo sube.

### Cómo repartirlos de forma uniforme

1. **Que ni `Present` ni el hilo gráfico retrasen el inicio del cuadro del juego**: `release_early = on` (sección 7).
2. **Que `Present` no espere con el cerrojo**: `present_wait = on` + `latency = 1`.
3. **Que el reloj y el monitor no batan**: `pacing = display` (en 60/120/240 Hz) con `vsync = on`, o `vsync = off`.
4. **Que haya como mucho un cuadro esperando**: `queue_limit = 1`.
5. **A medio plazo (no hecho)**: sacar `gpu_queue_wait` + `present_frame` del hilo del reloj a un hilo "presentador" propio, de modo que el reloj nunca espere al trabajo. Cambio mayor; ver sección 8.

---

## 5. Paquetes: envío y recepción

### Camino de un paquete

```
juego → nn::socket::SendTo → do_sendto (hle_net.c:840) → net_send (net_relay.c:767) → sendto (send_udp, net_relay.c:100)
UDP del peer → client_thread (net_relay.c:888) → recvfrom (:933) → direct_packet (:694) / T_DATA → vsock_deliver (hle_net.c:718)
            → cola del socket virtual → do_recvfrom (hle_net.c:899), cuando el juego lo pide
```

| Qué | Dónde | Veredicto |
|---|---|---|
| Envío | `do_sendto` → `net_send` → `sendto` | **Síncrono en el hilo del juego**, sin cola ni `Sleep`; el único cerrojo es `peers_lock` compartido (`net_relay.c:782`), que el hilo de red solo coge exclusivo en `peers_tick` (cada 50 ms, `:930`) y al cambiar de estado un peer. Coste: unas decenas de µs. |
| Recepción | `client_thread` bloquea en `recvfrom` (`:933`, `SO_RCVTIMEO` 100 ms, `:1010`) | Despierta en cuanto llega el paquete. `peers_tick` (cada 50 ms, en el mismo hilo) puede retrasar un paquete lo que dure ese tick (µs). |
| Entrega | `vsock_deliver` (`hle_net.c:718`) | Un `malloc` + `memcpy` bajo el cerrojo de sockets (`SRWLOCK` exclusivo) y `WakeAllConditionVariable`. El juego toma el mismo cerrojo en `do_recvfrom`; las retenciones son de µs. **No es compartido con el hilo gráfico** (cerrojo propio). |
| Cola del socket | `hle_net.c:635` | 1024 datagramas / 1 MiB por socket; si se llena, se descarta (como una LAN). |
| `Sleep` | `hle_net.c:235` (`ldn_agent`, 250 ms), `net_relay.c:891/961` | Solo gestión de sala/reconexión, nunca en el camino de datos. |
| `TCP_NODELAY` | `hle_online.c:291` | Ya puesto en el TCP del modo Online. El juego local usa UDP: no aplica Nagle. |
| Búferes de socket | `client_sock`: `SO_RCVBUF` 1 MiB (`net_relay.c:1044`); `SO_SNDBUF` el del sistema (`big_sends` solo hace algo en POSIX) | Suficiente a 60 paquetes/s. |
| Resolución del temporizador de Windows | `timeBeginPeriod(1)` (`ipc_vi.c:127`) y `cpu_sched_process` (`host.c:1172`; quita el ahorro de energía y el "ignorar resolución del temporizador" del proceso) | Bien. No se hace espera por temporizador en el camino de red. |
| Prioridad de hilos de red | normal (el proceso va en prioridad alta) | `boost_threads` los sube a `ABOVE_NORMAL`. |
| `IP_TOS` / DSCP | no existe en el código | **No se hizo**: en Windows, `setsockopt(IP_TOS)` lo ignora el sistema (hace falta una política de QoS o `qWAVE`), y casi ningún router doméstico respeta DSCP. |
| Ahorro de energía | del *adaptador*, no del proceso | Fuera del alcance del runtime. Para Wi-Fi: modo de ahorro "Máximo rendimiento" en las propiedades del adaptador; mejor, cable. |

### Lo que sí añade retraso de red (y no es nuestro código de red)

1. **El propio cuadro**: el juego lee los paquetes una vez por cuadro (`recvfrom` sin bloquear); un paquete que llega justo después espera hasta 16,7 ms. El contador `waiting in the game's socket` lo mide. Si los cuadros son irregulares, esa espera también lo es.
2. **El retraso de entrada del juego** (`online_delay.txt`, `HLE_WRAP(71016ccae0)`, `hle_net.c:122`): ya se puede elegir; no se ha tocado.
3. **Camino por la sala (relay) en vez de directo**: añade la ida y vuelta al servidor. El estado `direct`/`relayed` por jugador sale en F1/F2 y en el log (`peer …: direct`).
4. **Cuadros irregulares → envíos irregulares**: el juego manda un paquete por paso de simulación, así que el patrón de envío de la partida medida (55/s, 15 % de 28-36 ms y ráfagas) refleja los cuadros, no la red. El contador `datagram sent, gap…` se compara con `frame queued, gap…` y `vsync tick, gap…` (sección 6) para confirmarlo.

---

## 6. Cómo medir: `latency.on`

### Activarlo

Crear un archivo **vacío** llamado `latency.on` junto a `Play SSBU.bat` (la carpeta desde la que arranca el juego; también vale `SSBU_LATENCY=1` en el entorno). Sin el archivo no se cuenta nada: cada punto de medida cuesta una lectura y un salto. El log (`build\play.log`) saca cada **5 segundos**:

```
[latency] counters on (latency.on): a report every 5 seconds
[latency] 5.0 s: vsync ticks 300 (60.0/s); ticks with no new frame 0 (0.0/s); frames queued 300 (60.0/s); datagrams sent 302 (60.4/s); datagrams received 301 (60.2/s); clock restarts (a tick late by half a refresh or more) 0 (0.0/s);
[latency] frames waiting for the display when the game queued one: 1: 250  2: 50  3: 0  4+: 0
[latency] <nombre> (ms; n 300, avg 16.67, max 17.4): <2: 0  2-8: 0  8-14: 0  14-19: 300  19-28: 0  28-40: 0  40-60: 0  >=60: 0
```

Cada línea de histograma lleva el recuento por cubo (en ms), `n`, la media y el máximo de esos 5 s.

### Qué mide cada una

| Línea | Mide | Qué se espera / qué indica |
|---|---|---|
| `vsync tick, gap to the previous one` | Entre dos eventos vsync | Todo en `14-19`: el reloj va bien. Cubos de `28-40` = el hilo vsync se atrasa (sigue con `Present`, `GPU wait`, `renderer lock`). |
| `vsync tick, wake-up after it was due` | Lo que se pasa el reloj de su instante | `< 0.1-0.5`: bien. Más de 2 ms: la CPU está ocupada (probar `boost_threads`). |
| `vsync thread waiting for the GPU thread` | `gpu_queue_wait` | Si pasa de 2 ms con frecuencia: el hilo gráfico va por detrás (no es un asunto de presentación). |
| `vsync thread waiting for the renderer's lock` | Tiempo para entrar en `gfx_present` | Si es alto: el hilo gráfico retiene el cerrojo. |
| `vsync thread waiting for a free swap chain slot (present_wait)` | La espera nueva, **fuera del cerrojo** | Con `present_wait = on`, aquí es donde ahora se espera al monitor. |
| `IDXGISwapChain::Present` | La llamada | Con `vsync = on` y `latency = 3`, hasta 16,7 ms por llamada. Con `present_wait = on` debería caer a < 1 ms. |
| `vsync thread in present_frame` | Todo `gfx_present` (cerrojo + dibujo + `Present`) | La suma de lo anterior. |
| `vsync event -> game queues its frame` | Trabajo del juego por cuadro (simulación + dibujo) | Su p95 dice cuánto margen hay: > 14 ms = la CPU no da. Es el dato que necesitaría una "salida tardía". |
| `frame queued, gap to the previous one` | Cadencia con que el juego termina cuadros | Debe parecerse a `vsync tick, gap`. Si el tick es regular y esto no: el juego espera búferes. |
| `frames waiting for the display when the game queued one` | Cuántos cuadros terminados esperan | Mucho en `2`/`3`: hay cuadros de retraso fijo (probar `queue_limit`). |
| `frame queued -> handed to the display` | De que el juego encola a que se entrega a DXGI | Cuánto tiempo pasa un cuadro en la cola de ipc_vi. |
| `pad read -> frame handed to the display` | De la última lectura del mando antes de encolar, a la entrega | **El retraso botón → pantalla que causa el runtime** (sin contar la cola de DXGI, el compositor y la pantalla: +1-3 cuadros). |
| `pad state age when the game reads it` | Antigüedad del estado del mando | < 2 ms. |
| `datagram sent, gap to the previous one to the same place` | Intervalo entre envíos al mismo destino | Lo que se midió en la partida: 55/s, 15 % entre 28 y 40 ms. |
| `datagram received, gap…` | Intervalo entre llegadas del mismo origen | Si es regular y el envío no, el problema es del lado que envía. |
| `datagram waiting in the game's socket until read` | Entre la llegada y el `recvfrom` del juego | Hasta 16,7 ms por la lectura por cuadro; más: cuadros irregulares. |
| contadores `vsync ticks`, `ticks with no new frame`, `frames queued`, `clock restarts` | Cuadros nuevos frente a repetidos y reanclajes | `clock restarts` > 0 cada segundo = el reloj pierde refrescos (ritmo medio < 60). |

### Cómo leerlos (árbol de decisión)

1. ¿`vsync tick, gap` tiene cubos de `28-40`? **Sí** → el hilo vsync se retrasa: mirar `Present`, `renderer lock` y `GPU thread` para ver cuál. **No** → siguiente.
2. ¿`frame queued, gap` los tiene? **Sí** → el juego tiene búferes irregulares o la CPU no da (mirar `game queues its frame`, `frames waiting`). **No** → siguiente.
3. ¿`datagram sent, gap` los tiene? **Sí** con cuadros regulares → es el juego (recupera pasos de simulación). **No** → la partida es regular desde el origen: lo medido venía de otra versión o de la red.

Esto se hace sin conectarse a nadie (una partida de entrenamiento contra la CPU basta para las líneas de imagen; las de paquetes necesitan partida en línea).

---

## 7. Lo que se cambió y cómo probarlo

Cinco commits de código (más este informe). **Ningún `HLE(...)` ni `HLE_WRAP(...)` cambió** (nombre, firma o cuerpo; `HLE_WRAP(71016ccae0)` queda como estaba): la huella que usa `tools/recomp.py` (345 nombres, 8 envolturas) es idéntica a antes. Los cuatro ajustes de `display.ini` están **apagados por defecto**; los contadores solo funcionan con `latency.on`. Los ajustes no aparecen en los `display.ini` que ya existen (solo en uno nuevo); se ponen a mano o con la variable de entorno entre paréntesis. Para deshacer cualquiera, borrar la línea.

### 7.1 Contadores (`latency.on`) — commit "Latency counters…"

- Archivos: `runtime/latency.h`, `runtime/gpu_latency.c` (nuevo; el nombre `gpu_*` lo recogen las listas de compilación de Windows, Linux, macOS y Android sin tocarlas), y ganchos en `hle_pad.c`, `hle_hid.c`, `ipc_vi.c`, `gpu_display.c`, `gpu_d3d11.c`, `hle_net.c`.
- Probar: crear `latency.on`, jugar 30 s, abrir `build\play.log`, buscar `[latency]`. Debe haber una línea de contadores y los histogramas cada 5 s. Quitar el archivo: no debe salir nada.

### 7.2 `present_wait = on` — commit "display.ini present_wait…"

- Dónde: `display.ini` (`SSBU_PRESENT_WAIT`), `gpu_display.c`, `gpu_d3d11.c:824-845` (creación), `:5226-5232` (espera antes del cerrojo), `:5419` (latencia en caliente).
- Solo Direct3D de Windows (no DXVK). Si el sistema no crea la cadena con el indicador, se crea como antes y el log lo dice.
- Probar: `present_wait = on` y `latency = 1`. En `build\play.log`: `[gfx] present_wait: waitable swap chain, up to 1 frame queued…`. Con `latency.on`: `IDXGISwapChain::Present` debe bajar a < 1 ms y `vsync thread waiting for a free swap chain slot` pasa a tener la espera; `renderer lock` debe bajar. En F2: `Present blocked` debe bajar.
- Riesgos a vigilar: pantalla negra o sin imagen al cambiar el tamaño de la ventana (el *handle* tras `ResizeBuffers`); un tirón de 40 ms por cuadro (la espera expira siempre) significaría que el *handle* no se señala: poner `present_wait = off`.

### 7.3 `boost_threads = on` — commit "display.ini boost_threads…"

- Dónde: `display.ini` (`SSBU_BOOST_THREADS`), `ipc_vi.c:126` (hilo vsync → `THREAD_PRIORITY_HIGHEST`), `net_relay.c` (hilos `netplay client` y `netplay relay` → `ABOVE_NORMAL`).
- Probar: `boost_threads = on`; con `latency.on`, `vsync tick, wake-up after it was due` debe mejorar si había cubos > 2 ms. Si el PC está al límite de CPU, vigilar que el audio o el hilo del juego no se resientan.

### 7.4 `queue_limit = 1` — commit "display.ini queue_limit…"

- Dónde: `ipc_vi.c:260-275` (`dequeueBuffer` responde "esperar" mientras haya `queue_limit` cuadros esperando; el juego ya maneja esa respuesta, es la misma que cuando no hay búfer libre), `display.ini` (`SSBU_QUEUE_LIMIT`).
- No hay interbloqueo: si hay `queue_limit ≥ 1` cuadros esperando, el siguiente refresco toma uno y señala el evento de búfer.
- Probar: `queue_limit = 1`. `frames waiting for the display…` debe pasar a casi todo en `1`; `pad read -> frame handed to the display` baja del orden de un cuadro si antes había cuadros en `2`. Si el juego va más lento o a tirones, quitar la línea.

### 7.5 `release_early = on` — commit "display.ini release_early…"

- Dónde: `ipc_vi.c:164` y `:180`, `display.ini` (`SSBU_RELEASE_EARLY`).
- Libera el búfer del juego al coger el cuadro, no al volver `Present`. Lo que la pantalla sigue leyendo (el cuadro recién cogido) sigue en `ACQUIRED`.
- Probar: `release_early = on`. Con `latency.on`, `frame queued, gap` debe seguir a `vsync tick, gap`, y los cubos de `28-40` en `datagram sent, gap` deben disminuir si la causa era el punto 3 del resumen. En F2 no debería haber cambios visibles de imagen.

---

## 8. Lo que no se hizo y por qué

| No hecho | Por qué |
|---|---|
| **Salida tardía del cuadro del juego** (la lectura "lo más tarde posible", como Less-Delay) | Necesita la duración real del trabajo del juego por cuadro, que ahora se podrá medir (`game queues its frame`); un retraso mal calculado hace perder refrescos. Primero hay que medir. Diseño: retrasar la señal del evento vsync y del búfer libre `espera = 16,7 − p95(trabajo) − margen(1-2 ms)`, y volver a 0 si se pierde un refresco. |
| **Hilo presentador** (sacar `gpu_queue_wait` + `present_frame` del hilo del reloj) | Es el arreglo de fondo del punto 3-4 de la sección 4, pero cambia la sincronización entre tres hilos y el orden de los búferes: no se puede probar sin el juego. Los cuatro ajustes de la sección 7 intentan lo mismo con cambios más pequeños. |
| **Soltar el cerrojo del renderizador durante `Present`** | Hace falta saber que `Present` no usa el contexto inmediato a la vez que el hilo gráfico; con el D3D11 del *runtime* de Windows, el propio `Present` puede cogerlo internamente. `present_wait` consigue lo principal (que `Present` no tenga que esperar) sin tocar el cerrojo. |
| **Cambiar `vsync`, `latency` o `pacing` por defecto** | Cambiar lo que ven todos los usuarios sin haberlo medido. Se recomiendan (sección 9); cambiar el valor por defecto es una línea en `DEFAULT_INI` (`gpu_display.c`) cuando las medidas lo apoyen. |
| **MMCSS (`AvSetMmThreadCharacteristics`)** | Requiere `avrt.dll` (carga dinámica) y cambia el reparto de CPU de todo el proceso; la prioridad de hilo de `boost_threads` hace lo básico. |
| **`IP_TOS` / DSCP, `SO_SNDBUF`** | Sin efecto en Windows sin política de QoS, y casi ningún router doméstico respeta DSCP; el envío no está limitado por búferes (60 paquetes/s). |
| **Ahorro de energía del Wi-Fi** | Es del adaptador, no del proceso. Se aconseja en la guía de usuario (cable, "Máximo rendimiento"). |
| **Leer el mando en el hilo del juego en vez de en un hilo aparte** | Daría 0 ms de antigüedad en lugar de ≤ 1-2 ms, pero poner SDL en el hilo del juego añade latencia propia a cada llamada y SDL no está pensado para ello; el contador `pad state age` dirá si hace falta. |
| **Compilar o ejecutar nada** | No hay compilador de Windows ni el juego en la nube. El módulo de contadores (`gpu_latency.c`) se compiló y probó con gcc en Linux con un `host.h` de mentira; los demás cambios se revisaron a mano, línea por línea, y hay que compilarlos en Windows. |

---

## 9. Plan de pruebas en un PC

Dos PC con partida en línea, o una partida de entrenamiento para lo de imagen. Siempre con `latency.on`. Cambiar **una cosa cada vez**, 2 minutos por prueba (no hace falta partida ganada: basta con mover los personajes), y guardar el trozo `[latency]` de `build\play.log`.

| # | `display.ini` | Qué mirar |
|---|---|---|
| 0 | tal cual está (referencia) | `vsync tick, gap`, `Present`, `renderer lock`, `frames waiting…`, `pad read -> …`, `datagram sent, gap` |
| 1 | `latency = 1` | `Present` y `pad read -> frame handed…` (el cambio más sencillo) |
| 2 | 1 + `vsync = off` | `pad read -> …` baja un cuadro; puede haber desgarro |
| 3 | 1 + `present_wait = on` | `Present` < 1 ms, `renderer lock` baja, `vsync tick, gap` más regular |
| 4 | 3 + `release_early = on` | `frame queued, gap` y `datagram sent, gap` más regulares; menos ráfagas |
| 5 | 4 + `queue_limit = 1` | `frames waiting…` casi todo en `1` |
| 6 | 5 + `boost_threads = on` | `wake-up after it was due` |
| 7 | alternativa sin desgarro: `pacing = display` + `vsync = on` + `latency = 1` (60/120/240 Hz) | `clock restarts` = 0 y `Present` constante |

**Criterio de éxito** (el que propone este informe): `datagram sent, gap` con casi todos los envíos en `14-19` y `pad read -> frame handed to the display` por debajo de ~25 ms; todo lo demás es diagnóstico. En F1/F2 se ve `Present blocked X ms/frame` y los fotogramas por segundo.

**Si algo va mal:** borrar la línea correspondiente de `display.ini` (o el archivo `latency.on`); todo vuelve a como estaba. Los cambios no tocan el formato de las partidas guardadas ni los módulos recompilados del juego.
