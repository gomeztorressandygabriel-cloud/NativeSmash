# Rollback netplay: estudio de viabilidad y diseño de integración

[← Documentación técnica](TECHNICAL.md) · [Multijugador](MULTIPLAYER.md) · [Código del núcleo: `rollback/`](../rollback/README.md) · [Ideas de integración](ROLLBACK-IDEAS.md)

> **Estado: experimento aparte, NO integrado.** Este documento y la carpeta `rollback/` no cambian
> nada de `runtime/`, `tools/` ni `site/`, y no añaden ningún `HLE(...)` ni `HLE_WRAP(...)`.
> La nube donde se escribió no tiene el juego, ni sus archivos, ni compilador de Windows, ni el
> binario de Nintendo. Por eso lo que hay es (1) un núcleo de rollback independiente, probado
> con un juego sintético, y (2) este análisis. **Nada de lo que sigue demuestra que funcionará
> en el juego real.** Lo que se sabe, lo que se supone y lo que hay que medir va separado y
> marcado en cada apartado.

## Contenido

1. [Resumen y veredicto](#1-resumen-y-veredicto)
2. [Punto de partida: qué hace hoy el netcode](#2-punto-de-partida-qué-hace-hoy-el-netcode)
3. [Qué exige un rollback sobre este juego](#3-qué-exige-un-rollback-sobre-este-juego)
   - [(a) Guardar y restaurar el estado](#a-guardar-y-restaurar-el-estado)
   - [(b) Determinismo](#b-determinismo)
   - [(c) Re-simular: presupuesto de CPU](#c-re-simular-presupuesto-de-cpu)
   - [(d) Suprimir los efectos secundarios](#d-suprimir-los-efectos-secundarios)
   - [(e) La capa de sincronización del propio juego](#e-la-capa-de-sincronización-del-propio-juego)
   - [(f) El protocolo de entradas](#f-el-protocolo-de-entradas)
   - [(g) Riesgos y plan por fases](#g-riesgos-y-plan-por-fases)
4. [Lo que se construyó y se probó en la nube](#4-lo-que-se-construyó-y-se-probó-en-la-nube)
5. [Qué hacer después, en un PC con el juego (lista priorizada)](#5-qué-hacer-después-en-un-pc-con-el-juego-lista-priorizada)
6. [Apéndice: mediciones](#6-apéndice-mediciones)

## 1. Resumen y veredicto

**Qué es rollback.** Cada jugador simula el juego sin esperar al otro: para el cuadro que aún no
tiene la entrada remota, *supone* que el otro sigue pulsando lo mismo que en el último cuadro
conocido. Si la entrada real llega y era distinta, el juego vuelve al estado guardado de ese
cuadro y re-simula hasta el presente con la entrada correcta, todo antes de dibujar el siguiente
cuadro. Quien juega siente sus propios mandos sin retraso de red.

**Veredicto honesto.**

- El **algoritmo** funciona y está probado aquí con un juego sintético: con hasta 250 ms de ida,
  10 % de pérdida, reordenación y duplicados, dos instancias convergen exactamente al estado de
  la partida sin red, y las desincronizaciones se detectan ([§4](#4-lo-que-se-construyó-y-se-probó-en-la-nube)).
- **Aplicarlo a Super Smash Bros. Ultimate recompilado es un proyecto grande y con riesgo real de
  no salir.** El juego no fue escrito para rollback y NativeSmash no controla su código, solo
  su sistema operativo emulado. Las cuatro cosas que pueden matarlo, de más a menos graves:
  1. **El estado no está solo en la memoria del juego.** Las fibras del juego corren en fibras de
     Windows con pila propia en el *host* (`runtime/hle_fiber.c`: "a guest call is a host call, so
     the host stack holds the real frames"), y hay estado del kernel emulado en memoria del host
     (hilos, eventos, mutex, contextos `Cpu`). Copiar la memoria del juego **no basta**; habría
     que capturar también las pilas del host de cada fibra suspendida y los objetos del kernel
     — o conseguir guardar solo en puntos donde no haya fibras a medias (no sabemos si existen).
  2. **Determinismo.** Si la simulación del juego no es determinista de un cuadro a otro (hilos que
     compiten, relojes, aleatorios, diferencias de coma flotante entre Windows/Linux/macOS), no hay
     rollback posible hasta que se localice y arregle cada fuente.
  3. **No se conoce dónde acaba "un cuadro de simulación"** ni qué función lo ejecuta, ni si la
     simulación y la preparación del dibujo van mezcladas. Sin eso no se puede re-simular.
  4. **Presupuesto de CPU.** Re-simular 4–7 cuadros en un cuadro de pantalla multiplica el coste
     de simulación; si la simulación ocupa varios ms por cuadro, un portátil de gama media ya va
     justo y el teléfono (puerto Android en pre-alfa) no llega ([§3c](#c-re-simular-presupuesto-de-cpu)).
- La **vía barata ya existe** en NativeSmash: `HLE_WRAP(71016ccae0)` sustituye el retraso de entrada que elige el
  juego por el de `online_delay.txt` (3 cuadros por defecto; ver [§2](#2-punto-de-partida-qué-hace-hoy-el-netcode) y
  [ONLINE-TUNING.md](ONLINE-TUNING.md)). Lo que rollback podría ganar sobre eso es pasar de 2–3 cuadros de retraso
  (~33–50 ms) a 0–1. Antes de invertir, conviene medir cómo se siente hoy con 2 y 3 cuadros en partidas reales.

**Recomendación.** Hacer primero las fases 1–3 del [plan](#g-riesgos-y-plan-por-fases) (observar,
probar determinismo, probar guardar/restaurar con *sync test* en un solo PC). Son baratas, no
tocan la red y responden a las preguntas que deciden todo. **Si la fase 3 falla (fibras u hilos
imposibles de capturar), parar ahí.**

## 2. Punto de partida: qué hace hoy el netcode

**Hechos del repositorio** (leídos de este checkout):

- [`TECHNICAL.md`](TECHNICAL.md) dice: "The game continues to run its own Local Wireless session logic.
  NativeSmash presents a virtual LAN and carries UDP payloads through a relay. This does not replace the
  game's netcode with rollback networking."
- `runtime/hle_net.c` emula `nn::ldn` (Local Wireless) sobre una LAN virtual (10.13.0.0/16) y sockets virtuales.
  `runtime/net_relay.c` es el cliente del relay y de las conexiones directas por UDP con perforación de NAT
  (paquetes `DATA` por el lobby, tipo 13 para difusión y tipo 15 directos con el secreto del par; un par vuelve
  al lobby si una ruta directa no responde en 1,2 s).
- El juego **no sabe nada de esto**: ve una red local con otras consolas. El juego cifra sus propios paquetes.
- Los mandos llegan al juego por `nn::hid::GetNpadState*` (`runtime/hle_hid.c`, función `npad_state()`): cada
  llamada devuelve un contador de muestreo, los botones (`u64`), cuatro ejes de stick y, en mandos GameCube,
  dos gatillos analógicos. **Ese es el punto natural para inyectar entradas** (también existe el guion
  `SSBU_INPUT`, `scripted_buttons()`).
- La presentación está dirigida por un hilo de vsync nativo a 60 Hz (`runtime/ipc_vi.c`, `vsync_thread`):
  presenta el búfer más antiguo de la cola y devuelve el anterior (`present_frame` en `gpu_present.c`).
- La vigilancia de escrituras: `runtime/kernel.c` reserva `HEAP` (8 GiB), `ALIAS` (64 GiB reservados; el juego
  usa ~62) y `STACK` (2 GiB) y **solo `ALIAS` lleva `MEM_WRITE_WATCH`**. `gpu_d3d11.c` (`watch_take`) lee y
  *reinicia* esos bits con `GetWriteWatch(WRITE_WATCH_FLAG_RESET, …)` para saber qué texturas cambió la CPU.
  En Linux/macOS se emula con `mprotect` + `SIGSEGV` (`rt_ww_fault`, `runtime/linux/win32.c`,
  `runtime/macos/memory.c`).

**Hechos medidos que da el encargo** (no verificados aquí): el juego manda 1 paquete UDP por cuadro de simulación
(~144 bytes, número de secuencia en claro, resto cifrado), simula a 60 cuadros por segundo, y en modo inalámbrico
local sincroniza por turnos con retraso de entrada (cada jugador retiene su botón N cuadros).

**El retraso de entrada que ya controla NativeSmash** (`runtime/hle_net.c`, `HLE_WRAP(71016ccae0)` y `net_input_delay()`):
la función del juego que calcula el retraso de entrada en línea (11.0.0–13.0.x; la misma que cambian los mods "latency slider"
de la consola y de yuzu) guarda el número de cuadros en `this + 0x41` (máximo 25). El envoltorio deja que el juego lo calcule y
luego lo sustituye por el de `online_delay.txt` (1–25, o `auto` para el del juego; **3 sin archivo**), solo con la red activa. El
comentario del código dice que 2 es lo que aguanta una conexión directa entre dos PC en buena red y que menos hace que el juego
espere al otro (tirones). Guía para jugadores: [ONLINE-TUNING.md](ONLINE-TUNING.md); auditoría del retraso del runtime (mando,
imagen, ritmo, paquetes, `latency.on`): [LATENCY-AUDIT.md](LATENCY-AUDIT.md). Esto es **retraso de entrada ajustable, no rollback**:
el juego sigue esperando las entradas de todos para avanzar, así que "el PC más lento marca el ritmo de todos".

## 3. Qué exige un rollback sobre este juego

Para cada requisito: qué se sabe, qué se supone, qué cuesta (con números y su razonamiento) y qué hay que averiguar.

### (a) Guardar y restaurar el estado

**Qué es "el estado".** Todo lo que, si no se restaura, hace que re-simular dé otro resultado:

| Parte | Dónde vive | ¿Se captura copiando memoria del juego? |
| :--- | :--- | :--- |
| Datos de la simulación (luchadores, objetos, golpes, RNG, contadores) | memoria del juego (`HEAP`, quizá `ALIAS`) | Sí |
| Pilas de las **fibras** del juego (guiones/corrutinas de luchadores) | pilas del *host* de cada fibra de Windows (`hle_fiber.c`, `Fib.host`) | **No** |
| Contexto de registros de cada fibra e hilo | memoria del host (`Cpu`) | **No** |
| Hilos, eventos, mutex, semáforos emulados | tablas del kernel (`kernel.c`) en memoria del host | **No** |
| Estado de dispositivos HLE (mando, audio, red, GPU) | memoria del host | No (y casi nunca debe restaurarse) |

**Dos estrategias para la memoria del juego:**

1. **Copia completa cada cuadro.** Simple y robusta. Medido en la VM de la nube (`make bench`): ≈ 11–14 GB/s,
   es decir **1 MB ≈ 0,03–0,08 ms, 16 MB ≈ 1,3 ms, 64 MB ≈ 5,5 ms, 256 MB ≈ 22 ms, 512 MB ≈ 48 ms**. Con un
   presupuesto de 16,7 ms por cuadro, copiar más de ~30 MB por cuadro es inviable. Sin saber cuánto de la memoria es
   estado de simulación, **no se puede descartar ni aceptar**: es la medición nº 1 de la [lista](#5-qué-hacer-después-en-un-pc-con-el-juego-lista-priorizada).
2. **Solo las páginas modificadas** (`rollback/src/rb_pages.c`). Guarda copias de las páginas que cambiaron desde el
   guardado anterior y, para volver atrás, deshace los guardados más nuevos. Medido (región de 256 MB, anillo de 12):

   | páginas sucias por cuadro | guardar (ms) | cargar 1 cuadro | 4 cuadros | 8 cuadros | memoria retenida |
   | ---: | ---: | ---: | ---: | ---: | ---: |
   | 50 | 0,06 | 0,04 | 0,12 | 0,27 | 14 MB |
   | 200 | 0,23 | 0,13 | 0,52 | 1,03 | 52 MB |
   | 1 000 | 1,0 | 0,65 | 2,5 | 5,0 | 189 MB |
   | 4 000 | 3,5 | 2,3 | 9,8 | 19,9 | 345 MB |
   | 16 000 | 23 | 11 | 46 | 92 | 596 MB |

   Razonamiento: cuesta ≈ 0,9–1,4 µs por página sucia (reserva + 4 KB de copia + contabilidad), más la memoria
   `páginas × 4 KB × anillo` (la "retenida" incluye también la última versión de cada página tocada alguna vez).
   **Cuántas páginas ensucia el juego por cuadro es DESCONOCIDO.** Hipótesis de trabajo: 200–4 000 (0,8–16 MB por cuadro);
   hay que medirlo. Si fueran >10 000, el rollback por páginas deja de compensar.
   También hay que guardar una **línea base** de toda la región al empezar (copia completa una vez, con el tamaño de
   la memoria viva escribible: potencialmente GBs; es una pausa al empezar el combate).

**Cómo obtener las páginas sucias, y los problemas concretos de este runtime:**

- Windows: `GetWriteWatch` no cuesta nada por escritura (usa los bits del hardware); "preguntar es barato, reiniciar no"
  (comentario del propio `gpu_d3d11.c`). Reiniciar los bits una vez por cuadro tiene un coste que hay que medir.
- **Conflicto con la GPU:** `watch_take` ya consume y reinicia los mismos bits para las texturas. Un segundo consumidor
  (rollback) los dejaría ciegos. Haría falta una capa que lea los bits una vez y reparta la noticia a los dos
  (la GPU ya guarda un "sello" por página, `watch_chunks`; el rollback tendría su propio sello).
- **Solo `ALIAS` está vigilada.** `HEAP` y `STACK` se reservan sin `MEM_WRITE_WATCH`; habría que cambiarlo en
  `kernel_init` (es una modificación de `runtime/`, fuera del alcance de este encargo). No se sabe si la simulación
  vive en `HEAP`, `ALIAS` o ambas.
- Linux/macOS: la vigilancia se emula con `mprotect` y un fallo (`SIGSEGV`) en la **primera escritura de cada página
  tras cada reinicio**. Supuesto razonado: ≈ 1–3 µs por fallo → 1 000 páginas sucias ≈ 1–3 ms por cuadro solo en
  seguimiento. Por eso lo realista es probar primero en Windows.
- Alternativa en Linux: bits "soft-dirty" de `/proc/self/pagemap` (no usada hoy en el runtime; no evaluada).

**Lo que la memoria no cubre (fibras, hilos, kernel):** ver [§1](#1-resumen-y-veredicto), punto 1. Opciones, de más a
menos prometedora, todas por investigar con el juego:

1. Que existan **puntos seguros** (p. ej. justo tras la presentación de un cuadro) en los que todas las fibras del
   combate estén terminadas o en una espera conocida, y guardar solo ahí. Hay que averiguar cuántas fibras vivas hay
   en ese instante y en qué función están suspendidas.
2. Sustituir `Fib`/fibras de Windows por una implementación propia de cambio de pila (la de Linux ya es "a few
   lines": `rt_fiber_switch`) cuyas pilas **sí** se puedan copiar y devolver a la misma dirección, incluyendo el
   registro guardado. Es la única forma general; es trabajo grande en `hle_fiber.c`/`win32.c`.
3. Reescribir el método de re-simulación para no necesitar restaurar fibras (descartado: requeriría conocer el motor).

**Sumas de verificación del estado.** Una suma de toda la memoria por cuadro es inviable (medido: 256 MB ≈ 94 ms con
el hash de prueba). Hay que sumar un subconjunto bien escogido (posiciones, daño, estados de acción, RNG de cada luchador)
cuya dirección se averigua con el juego. El núcleo acepta cualquier función `checksum`.

### (b) Determinismo

Rollback exige que, **dado el mismo estado y las mismas entradas, la simulación produzca siempre el mismo resultado**,
en las dos máquinas. Fuentes de no determinismo a descartar, dónde mirar en este repo y cómo detectarlas:

| Fuente | Dónde mirar | Riesgo / observación |
| :--- | :--- | :--- |
| Temporizadores y reloj | `runtime/clock.h`, `rt_clock_ns`, el "system tick" del juego | La simulación no debería leerlos, pero el juego lee el tick decenas de miles de veces por segundo; ¿alguno influye en el estado? |
| Aleatorios | `nn::crypto::GenerateCryptographicallyRandomBytes` (`hle_offline.c`), `ipc_mii.c` (`rand_s`) | Si el RNG del combate se siembra con algo local, **ambos PC deben acordar la semilla**; en rollback el RNG debe estar dentro del estado guardado (casi seguro, pero hay que verlo) |
| Hilos y fibras | `kernel.c` (`CreateThread`), `hle_fiber.c` | Si dos hilos escriben el estado de la simulación y el orden depende del planificador del SO, no hay determinismo. **El mayor interrogante.** Las fibras cambian de hilo (`hle_fiber.c`: "may resume on a different thread") |
| Coma flotante | código recompilado de AArch64 a C | Contracción FMA, `-ffast-math`, redondeo, flush-to-zero y distintas librerías matemáticas entre MSVC/clang/gcc y entre Windows/Linux/macOS (el proyecto asegura que "Windows and Linux players play together"). Un mismo ejecutable en dos PC con CPU distintas suele ser determinista; **compilaciones distintas, no necesariamente** |
| GPU | `gpu_*.c` | La simulación no debería depender de resultados de la GPU; hay que comprobar que no hay lecturas de vuelta (*readback*) que alimenten la lógica |
| Audio | `ipc_audio.c` (el juego mezcla él mismo; el dispositivo marca el ritmo) | Si la lógica espera al audio para avanzar (p. ej. fin de efecto de sonido que dispara algo), el ritmo del dispositivo se cuela en la simulación |
| E/S y carga de recursos | `hle_fs.c`, carga asíncrona | Si un recurso llega "a tiempo" o "tarde" cambia cuándo ocurre algo en el combate, hay divergencia. Hay que ver que la simulación espera a que esté todo cargado |
| Memoria sin inicializar, direcciones | recompilado; punteros | Hash de punteros o iteración por dirección de objetos puede variar entre ejecuciones (ASLR del host) |

**Cómo detectarlo, de más barato a más caro** (el núcleo implementa las dos últimas ideas ya probadas con el juego sintético):

1. **Determinismo de una sola máquina:** reproducir la misma grabación de entradas (`SSBU_INPUT`) tres veces y comparar la
   suma de verificación de la zona de simulación cuadro a cuadro. Si ya difieren aquí, hay no determinismo local. Las páginas
   que difieren entre ejecuciones dicen *qué objeto* es.
2. **Sync test** (`synctest_depth` en `rollback/`): tras cada cuadro, volver `N` cuadros atrás, re-simular y comparar. Encuentra
   a la vez *estado que no se guarda* y *no determinismo*, sin red. Probado: detecta el contador no guardado del juego sintético.
3. **Sumas de verificación entre jugadores** (`checksum_interval`): cada `N` cuadros, cada lado envía la suma del estado ya
   *definitivo* (todas las entradas anteriores confirmadas) y compara con la suya. Probado: una corrupción forzada de un bit
   en un solo jugador se señala en el siguiente cuadro múltiplo del intervalo (≤ 8 cuadros, ~0,15–0,4 s en las pruebas).
4. **Pruebas cruzadas** Windows↔Linux↔macOS con las mismas entradas.

### (c) Re-simular: presupuesto de CPU

**Modelo.** En un cuadro de pantalla con un rollback de profundidad `d`, el juego hace: cargar estado + `d` re-simulaciones
(cada una con su guardado) + el cuadro nuevo. Coste ≈ `carga(d) + d × (sim + guardar) + (sim + guardar)` ms, contra
**16,7 ms** a 60 Hz, donde además hay que dibujar y mezclar audio.

La siguiente tabla **combina números medidos (carga/guardado de páginas, ver §3a) con tiempos de simulación SUPUESTOS**
(`sim` = ms que cuesta un cuadro de simulación; no lo conocemos). Entre paréntesis, la estimación para un teléfono con un factor
supuesto ×2,5 de lentitud (supuesto: no hay mediciones de CPU del puerto Android). Cualquier valor por encima de 16,7 ms es un cuadro perdido.

| páginas sucias | `sim` | d = 2 | d = 4 | d = 7 |
| ---: | ---: | ---: | ---: | ---: |
| 200 | 2 ms | 6,9 (17,4) | 11,7 (29) | 18,7 (47) |
| 200 | 4 ms | 12,9 (32) | 21,7 (54) | 34,7 (87) |
| 200 | 8 ms | 24,9 (62) | 41,7 (104) | 66,7 (167) |
| 1 000 | 2 ms | 10,3 (26) | 17,6 (44) | 28,6 (72) |
| 1 000 | 4 ms | 16,3 (41) | 27,6 (69) | 44,6 (112) |

**Lectura honesta:** si un cuadro de simulación del juego cuesta ~2 ms en un portátil de gama media y ensucia ≲200 páginas,
profundidades de 2–4 caben; con 4 ms ya solo cabe d ≤ 2; en un teléfono casi nada cabe. El puerto Android ([ANDROID.md](ANDROID.md),
pre-alfa en un Pixel 9 Pro XL) aún no sostiene 60 cuadros en combate (≈11 cuadros/s el primer minuto por la compilación de shaders;
menús a 57–59): **no hay margen para re-simular en el teléfono hasta que el juego vaya holgado a 60**. **No se sabe cuánto cuesta un
cuadro de simulación frente a uno de dibujo**: es la medición nº 2.

**Cuántos cuadros hay que re-simular de verdad.** En las pruebas con el juego sintético (entradas que cambian cada 8 cuadros,
mucho menos nerviosas que un jugador real, y con un predictor "repetir la última"), el promedio de cuadros re-simulados por cuadro
fue **0,10** a 1 ms de ida, **0,20** a 25 ms, **0,50** a 100 ms (3 % pérdida) y **1,04** a 200 ms (5 %), con profundidad máxima 2, 3, 7 y 13
([§6](#6-apéndice-mediciones)). Un jugador real cambia de entrada más a menudo, así que habría más. **Mitigaciones previstas** (algunas no
implementadas): retraso de entrada local pequeño y fijo de 1–2 cuadros (implementado: `input_delay`) para recortar la profundidad típica;
tope de predicción `max_prediction` (implementado: si se llena, el juego *espera*, no se pierde la sincronía); no re-simular si la
predicción acertó (implementado y probado: coste cero); predecir "soltado" para botones de pulsación corta (no implementado);
dibujar solo el último cuadro re-simulado (es lo normal); y, si `d × sim` no cabe, esperar en lugar de re-simular.

### (d) Suprimir los efectos secundarios

Durante una re-simulación el juego **no debe** producir de nuevo sonido, vibración, partículas visibles ni salida a disco. El núcleo ya
avisa al juego (`advance(..., resimulating=1)`). En el juego real todo salida pasa por el runtime, lo que ayuda, pero hay un límite:

| Efecto | Dónde pasa por el runtime | Cómo suprimirlo | Incógnita |
| :--- | :--- | :--- | :--- |
| Dibujo / envíos a la GPU | `gpu_queue.c`, `gpu_d3d11.c`, presentación en `ipc_vi.c` | Una bandera global "re-simulando": descartar envíos de dibujo y no presentar | Si la simulación y la generación de comandos de dibujo comparten el mismo hilo/función, descartar comandos puede romper estado del renderer |
| Audio | `ipc_audio.c` (el juego mezcla; el runtime solo recibe PCM) | El juego decide qué suena; hay que cortar en el origen (la función del juego que dispara un sonido) o filtrar en el mezclador | **No se puede suprimir en `ipc_audio.c`**: ya llega mezclado. Hace falta localizar las funciones de disparo de sonido del juego |
| Vibración | `hle_hid.c` (`VibrationValue` por actuador, ~línea 230) | Ignorar durante re-simulación | Es lo más fácil |
| Disco / guardado | `hle_fs.c` | Ignorar escrituras | Baja |
| Red | `hle_net.c` | No aplica si se evita la red del juego | — |
| Efectos "visibles" de cuadros que luego se revierten (sonido de un golpe que no ocurrió) | — | Aplicar la técnica habitual: retrasar 1–3 cuadros los efectos puramente cosméticos hasta confirmarlos | Requiere conocer el motor |

**Probado en el sintético:** la bandera `resimulating` es 1 exactamente en los cuadros repetidos (el contador coincide con
`resim_frames`) y los efectos se emiten solo en la primera simulación.

### (e) La capa de sincronización del propio juego

Hoy el juego espera las entradas del otro jugador, las retiene N cuadros y cifra sus paquetes. Tres maneras de integrarse, de la
más limpia a la más frágil:

**Opción A — "juego local con mandos virtuales" (recomendada para explorar).** Se evita por completo la red del juego: ambos PC
ejecutan el modo **VS local** (varios mandos en una sola consola). El rollback rellena la entrada del mando remoto *dentro de*
`npad_state()` (el punto de §2: `GetNpadState` ya es nuestro) y guarda/restaura/re-simula alrededor de la función que avanza un
cuadro. Ventajas: no hay que entender Pia ni su cifrado; no hay "esperar al otro" que desactivar. Costes y preguntas:
cómo llevar a los dos PC al **mismo estado inicial** del combate (personajes, reglas, escenario, semilla del RNG) sin pasar por el
menú de red del juego —por ejemplo, ambos juegan guiones de entrada idénticos en el menú local, o se restaura un estado guardado
común—; y cómo se hace el menú previo. Sirve además para repetir el mismo cuadro en las dos máquinas, que es lo que se necesita para
las pruebas de determinismo.

**Opción B — puentear la capa de sincronización.** Mantener el modo inalámbrico, pero sustituir la función que gestiona el retraso/espera
de entrada para que no espere y que lea las entradas de nuestro módulo. Requiere localizar y entender esa capa: de ella solo se conoce
la función que calcula el retraso (`71016ccae0`, ya envuelta), no la que retiene y espera las entradas; el cifrado y los números de secuencia son del juego. Más frágil: toca la lógica de
sesión del juego y habría que reproducir lo que Pia entrega a la simulación.

**Opción C — "retraso de entrada mínimo", sin rollback. Ya está hecha** (`online_delay.txt`, 3 por defecto). Sirve de **referencia**:
cualquier rollback debe sentirse claramente mejor que jugar con 2 cuadros en una conexión directa para que valga su coste.
Falta medir en partidas reales entre dos PC (ONLINE-TUNING.md dice que aún no hay cifras medidas).

**Qué NO se puede saber sin el binario del juego** (y hay que averiguarlo localmente con él):

1. La dirección y la firma de la función que ejecuta **un cuadro de simulación**, y qué hilo la llama.
2. Si la simulación es de un solo hilo, o si hay trabajo en paralelo cuyo orden afecta al resultado.
3. Qué datos forman el estado (regiones, tamaño, punteros entre regiones) y cuáles son del dibujo/audio.
4. Cuántas fibras del juego están vivas en un combate y dónde se suspenden; si hay puntos sin fibras a medias.
5. La fuente y el momento de siembra del RNG del combate.
6. Qué más viaja por los paquetes de Pia además de las entradas (semillas, selección, eventos de partida).
7. Cómo reacciona el juego al saltarse sus esperas de sincronización (¿se cuelga, se desincroniza, compensa?).
8. Dónde lee el juego el mando (`GetNpadState` con qué cadencia) y si lee el estado una vez por cuadro de simulación o varias.

### (f) El protocolo de entradas

**Implementado** (`rollback/src/rb_net.c`, probado en la red simulada): un datagrama por cuadro, formato fijo de 40 bytes de
cabecera + 8 bytes por entrada (cabecera descrita en `rollback/include/rb_net.h`):

- **Redundancia de entradas pasadas:** cada paquete lleva *todas* las entradas locales que el otro aún no ha reconocido (hasta 32),
  así que un paquete perdido no cuesta nada mientras llegue uno posterior. Es idempotente y no depende del orden.
- **Reconocimientos:** cada paquete lleva la entrada contigua más alta recibida del otro. Un paquete duplicado o reordenado se
  descarta sin efecto (se cuentan los redundantes).
- **Predicción:** para un cuadro sin entrada remota se usa la última entrada contigua conocida del jugador.
- **Entradas tardías:** si la real difiere de la usada, se marca el cuadro más antiguo erróneo y en el siguiente `tick` se carga ese
  estado y se re-simula hasta el presente. Si coincide, no se hace nada.
- **Ritmo:** cada lado anuncia su "ventaja de cuadros" (su cuadro menos el último cuadro del otro que ha oído); quien va por delante
  espera `(mía − suya)/2` cuadros si ese valor es ≥ 2. El retraso de ida se cancela en la resta. Probado con un reloj 3 % más lento en un
  lado: sin ritmo, el lado rápido se bloquea 46 veces por ventana llena y las reversiones llegan a profundidad 10; con ritmo, 0 bloqueos y
  profundidad 4.
- **Sumas de verificación:** el último cuadro "definitivo" y su suma van en cada paquete.
- **Validación:** se rechazan paquetes con magia/versión/sesión/remitente erróneos, cuenta inconsistente o truncados (probado).

**Ancho de banda medido en el simulador:** ~3–15 KB/s por sentido (2,9 KB/s sin pérdida; 15 KB/s con 200 ms y 5 % de pérdida), 55–110 bytes de
media por paquete (máx. 296). Comparable al juego actual (~144 B × 60 = 8,6 KB/s). Con entradas de 8 bytes; las reales (botones + 4 ejes +
gatillos analógicos del mando GameCube) caben en 64 bits solo si se cuantizan; si no, ampliar `RbInput`.

**No implementado, a decidir:** autenticación y cifrado (hoy solo id de sesión; el relay ya autentica por par con secreto en los paquetes
de tipo 15), medición de RTT, compresión por diferencias, más de 2 jugadores por malla completa (el núcleo admite hasta 4 y está probado
con 3 de forma manual, pero el arnés de red solo prueba 2), espectadores, y un predictor mejor que "repetir la última".

**Enchufe al runtime:** el transporte son dos funciones (`send`/`recv`). La integración natural es un canal propio sobre `net_relay.c`
(por la conexión directa y, si falla, por el lobby), sin tocar los puertos virtuales que usa el juego.

### (g) Riesgos y plan por fases

**Riesgos** (probabilidad e impacto son juicios míos, no datos):

| Riesgo | Prob. | Impacto | Cómo se descubre |
| :--- | :--- | :--- | :--- |
| Pilas de fibras/estado del kernel imposibles de capturar con coste razonable | media-alta | mata el proyecto | Fase 3 |
| Simulación no determinista (hilos, flotantes, relojes) | media | mata o retrasa meses | Fase 2 |
| Demasiadas páginas sucias por cuadro (> ~10 000) | media | obliga a copia completa (inviable) | Fase 1 |
| `sim` demasiado caro para re-simular | media | limita a d ≤ 2 o a PC potentes | Fase 1 |
| Dibujo y simulación inseparables (no se puede suprimir efectos) | media | artefactos visuales/sonoros | Fase 5 |
| Diferencias entre Windows/Linux/macOS | media | desincronizaciones entre plataformas | Fase 4 |
| Entradas reales más nerviosas que el modelo | alta | más rollbacks de lo estimado | Fase 4 |
| Legal/ético: modificar el juego en tiempo de ejecución | baja (ya se hace) | — | — |

**Plan con hitos comprobables** (cada fase tiene criterio de paso o parada):

| Fase | Dónde | Qué | Hito / criterio |
| :--- | :--- | :--- | :--- |
| 0 | Nube (hecha) | Núcleo, protocolo, instantáneas por páginas, juego sintético, simulador de red, pruebas, este estudio | `make test` verde, 680 comprobaciones |
| 1 | PC con el juego | **Solo observar:** medir páginas sucias por cuadro y su origen, tiempo de simulación vs. dibujo, cadencia de `GetNpadState`, número de fibras e hilos en combate | Tabla de números; **Parada** si >10 000 páginas/cuadro o `sim` >6 ms |
| 2 | PC con el juego | Determinismo local: misma grabación de entradas ×3, sumas de zonas por cuadro; luego Windows↔otro SO | 3 ejecuciones idénticas en 3 personajes y 3 escenarios; localizadas todas las fuentes que no lo sean |
| 3 | PC con el juego | **Guardar/restaurar con sync test** (profundidad 1…7) en un combate normal, incluyendo fibras e hilos | 10 000 cuadros sin fallo. **Es la puerta definitiva** |
| 4 | 2 instancias en un PC (`tools/dev/netpair.sh`) | Núcleo + entradas por loopback con latencia simulada; luego dos PC | Convergencia con 100 ms/3 % pérdida; sin falsas alarmas de suma |
| 5 | Dos PC | Supresión de efectos (audio, vibración, dibujo), ritmo, interfaz | Partida de prueba sin audio duplicado ni parpadeos |
| 6 | Internet | Sintonía, alternativa al netcode original, métricas | Decidir si se publica como opción |

## 4. Lo que se construyó y se probó en la nube

Código en [`rollback/`](../rollback/README.md) (C99, `make test`, también con clang, AddressSanitizer y UBSan):

- **Núcleo** (`rollback.c`): anillo de 32 estados, predicción "última entrada conocida", detección de entrada tardía, rollback
  y re-simulación, ventana de predicción con bloqueo, sumas de verificación finales y comparación con las del otro lado, sync test.
- **Protocolo** (`rb_net.c`): §3f. **Instantáneas por páginas** (`rb_pages.c`): §3a.
- **Pruebas** (680 comprobaciones, 0 fallos):
  - Con entradas tardías de 0 a 7 cuadros (y 0–2 de retraso local), el estado final de una sesión es **idéntico** al de la partida sin red.
  - Predicción acertada = 0 rollbacks. Ventana llena = bloqueo y nada consumido. Duplicados idempotentes, conflictos contados, entradas
    fuera de orden, 3 jugadores, configuraciones inválidas rechazadas.
  - 7 escenarios de red (1 a 250 ms de ida; jitter hasta 40 ms; pérdida 0–10 %; duplicados; reordenación) × 3 semillas × 1 800 cuadros:
    **ambas sesiones terminan en el estado exacto de la referencia** y no hay falsas alarmas de desincronización.
  - Una desincronización forzada (un valor corrompido en el cuadro 600 de un solo jugador) se detecta en ambos lados, en el siguiente
    múltiplo del intervalo, con 0, 50 y 100 ms de ida. El error "estado que la simulación lee pero no se guarda" se detecta con la red
    y, sin red, con el sync test.
  - Paquetes mal formados rechazados; instantáneas por páginas idénticas a copias completas, también dentro de un rollback real.

**Lo que NO demuestran:** nada del juego real. El juego sintético tiene ~4 KB de estado, una sola hebra, ningún reloj y ninguna fibra.
Los tiempos medidos son de la VM de la nube, no de un portátil ni de un teléfono. Las entradas son más tranquilas que las de una persona.
El simulador de red es independiente por sentido y sin cola limitada (no modela *bufferbloat*). Los resultados anteriores no son
un pronóstico de rendimiento en el juego.

## 5. Qué hacer después, en un PC con el juego (lista priorizada)

Para el usuario y su asistente local. Cada punto dice qué buscar, con qué herramienta del repositorio y qué resultado esperamos obtener.

### La sonda de la fase 1: `rollback_probe.on`

Los puntos 1, 4 y 6 ya tienen herramienta: `runtime/net_rbprobe.c`. Está **apagada** salvo que exista un archivo vacío llamado
`rollback_probe.on` junto a `Play SSBU.bat` (o `SSBU_RB_PROBE=1` en el entorno). Apagada, no cambia nada: cada enganche es una lectura y un salto.

**Cómo usarla:**

1. Compila esta rama como siempre (no añade dependencias).
2. Crea `rollback_probe.on` junto a `Play SSBU.bat`. Si quieres también el tiempo de cada cuadro del juego, crea además `latency.on`
   ([LATENCY-AUDIT.md](LATENCY-AUDIT.md), línea `vsync event -> game queues its frame`).
3. Juega **un combate normal de 2 jugadores de al menos 2 minutos** (sin red primero; luego, si se puede, uno en red), con mucha acción.
4. Cierra el juego y manda las líneas `[rbprobe]` de `build\play.log` (cada 5 s salen cinco líneas). Léelas antes: no llevan nombres ni
   direcciones de internet, solo números y direcciones de memoria del juego.
5. Borra `rollback_probe.on` para jugar normal: con ella, el juego vigila más memoria y va un poco más lento.

**Qué dice cada línea y qué se espera:**

| Línea | Qué mide | Cómo leerla |
| :--- | :--- | :--- |
| `Pages written per frame … heap median / p95 / max (MB)` | Páginas de memoria del juego escritas entre dos cuadros: lo que un rollback copiaría por cuadro | ≲ 1 000 (≈ 4 MB) en combate: viable. 1 000–10 000: caro, mirar las regiones. > 10 000 de forma constante: el enfoque por páginas no compensa (parada de la fase 1) |
| `… stack …` | Lo mismo en las pilas de los hilos del juego | Suele ser poco; si es mucho, las pilas cambian a cada cuadro |
| `reading and resetting the watch: avg / max ms` | Lo que cuesta solo *saber* qué páginas cambiaron | Debe quedar por debajo de ~1 ms; si no, el seguimiento ya se come el presupuesto |
| `heap regions written most` | Las zonas de 1 MB más escritas | Dónde vive probablemente el estado de la simulación: el punto de partida para la suma de verificación y para buscar el RNG |
| `pad reads per frame … by N thread(s)` | Cuántas veces y desde cuántos hilos lee el juego el mando | Lo ideal: 1–2 lecturas por cuadro desde 1 hilo (un solo punto donde inyectar la entrada remota) |
| `guest threads alive; fibers alive` | Hilos y fibras vivos | Cada fibra viva es una pila del host que el rollback tendría que restaurar ([§3a](#a-guardar-y-restaurar-el-estado)). Decenas: abordable; miles: muy difícil |
| `at the vsync tick, guest threads not waiting … none at N of M ticks` | Hilos del juego que **no** esperan justo antes de despertarlo para un cuadro nuevo | Cerca del 100 % "none": hay un momento quieto por cuadro donde guardar y restaurar ([ROLLBACK-IDEAS.md §3](ROLLBACK-IDEAS.md#3-la-medición-que-falta-hay-un-momento-quieto)) |

**Límites de la sonda:** no mide la zona `ALIAS` (la usa la GPU y leerla la dejaría ciega para sus texturas), así que si la simulación
escribe ahí, no aparece. El cuadro se corta cuando el juego entrega su imagen, no en el límite exacto de la simulación. En Linux, si el log dice
`[mem] write watch: page protection (fallback)`, la sonda es mucho más lenta y podría fallar al recibir datos de la red: medir en Windows.

1. **Medir el tamaño real del estado y las páginas sucias por cuadro** *(decide si el enfoque es viable)*.
   Durante un combate de 2 jugadores, activar `MEM_WRITE_WATCH` también en `HEAP` (o medir con una copia de prueba de `kernel.c`),
   leer `GetWriteWatch` una vez por cuadro de vsync **sin reiniciarlo para la GPU** (coordinar con `watch_take`), y registrar:
   páginas sucias por cuadro (mediana, p95, máximo) y a qué región pertenecen. Ver cuántos MB de `HEAP`/`ALIAS` están realmente en uso.
   **Herramienta lista:** la sonda `rollback_probe.on` (arriba) lo hace para `HEAP` y `STACK`.
2. **Medir el tiempo de un cuadro de simulación frente a uno de dibujo.** Con `SSBU_HITCH` y `debug_profile.c` (ya nombran funciones
   del programa) en un combate: ¿cuánto de los 16,7 ms usa el hilo que simula? Es el `sim` de la tabla de §3c.
3. **Localizar la función que avanza un cuadro de simulación.** Perfilar el hilo principal en combate: la función que se llama exactamente
   una vez por cuadro y de la que cuelga la simulación de luchadores. Anotar su dirección y su firma (con `tools/recomp.py wrap-sig`
   para el patrón de `HLE_WRAP`, cuando se llegue a integrar). Confirmar si dibujo y simulación van en la misma función.
4. **Localizar la lectura del mando:** traza de llamadas a `GetNpadState*` por cuadro y por hilo (¿una vez por cuadro?, ¿qué hilo?).
   Hecho en parte: el punto es `hle_hid.c: npad_state`. **La sonda** cuenta lecturas por cuadro y los hilos que leen.
5. **Prueba de determinismo local (fase 2):** reproducir una grabación (`SSBU_INPUT`) 3 veces; escribir las sumas de zonas de memoria por cuadro
   y compararlas. Si difieren, comparar páginas para encontrar el objeto y buscar qué lo cambia. Repetir en otro SO.
6. **Inventario de fibras e hilos en combate:** cuántas fibras vivas, sus estados (`fiber_counts` de `hle_fiber.c` ya da ranuras y finalizadas),
   en qué funciones están suspendidas, y si existe un instante por cuadro en que están todas quietas. Decide §3a (estrategia de pilas).
   **La sonda** da ya cuántas fibras e hilos están vivos; dónde están suspendidas queda por hacer.
7. **Capa de sincronización del juego:** la función que calcula el retraso ya está localizada y envuelta (`71016ccae0`). Falta la que
   **retiene y espera** las entradas: buscar quién lee `this + 0x41` y dónde se bloquea el hilo principal cuando falta la entrada del otro
   (con `SSBU_HITCH` en una partida con `online_delay.txt` = 1, que fuerza esperas). Medir también, con `latency.on` (LATENCY-AUDIT.md) y dos
   PC reales, cómo se siente 2 frente a 3 cuadros: es la referencia que el rollback tendría que mejorar.
8. **Origen y siembra del RNG del combate:** ¿de `GenerateCryptographicallyRandomBytes`, del tick del sistema, de un valor acordado por Pia?
   Dónde vive su estado (¿en una región que el guardado cubre?).
9. **Prueba de ida y vuelta de memoria:** guardar la memoria del juego (copia completa, solo para pruebas) en el cuadro N, seguir 120 cuadros con
   entradas grabadas, restaurar y repetir: comparar sumas. Con fibras sin capturar se espera que falle: mide justo lo que falta capturar.
10. **Sync test del núcleo sobre el juego real (fase 3)**, tras 1–9: implementar `RbGame{save,load,advance,checksum}` para el juego
    (con `advance` = la función del punto 3 y sin red), activar `synctest_depth` = 1…7 y jugar 10 000 cuadros.
11. **Contraste Windows/Linux/macOS de coma flotante:** mismas entradas, mismas sumas, en las tres compilaciones.
12. **Dónde se disparan sonido y vibración:** funciones del juego que encolan un sonido, para poder anularlas en re-simulación.
13. **Ancho de banda y latencia reales** del relay y de la conexión directa (`net_relay.c`) con un canal de prueba de ~100–300 bytes por cuadro,
    medir pérdida y jitter reales antes de ajustar `max_prediction`/`input_delay`.
14. Solo entonces: transporte sobre `net_relay.c`, supresión de efectos, interfaz, y cómo ambos PC arrancan el mismo combate (opción A de §3e).

## 6. Apéndice: mediciones

Obtenidas con `cd rollback && make bench` en la VM de la nube (Linux, gcc, `-O2`, 4 vCPU; **no** es un portátil ni un teléfono).
Se repiten al ejecutarlo; los tiempos varían ~±20 %.

**Copia completa de memoria** (`memcpy`) y suma de verificación de prueba:

| MB | copiar (ms) | GB/s | suma (ms) |
| ---: | ---: | ---: | ---: |
| 1 | 0,03 | 35 | 0,20 |
| 16 | 1,3 | 13 | 3,8 |
| 64 | 5,5 | 12 | 15 |
| 256 | 22,5 | 12 | 94 |
| 512 | 48 | 11 | 197 |

**Instantáneas por páginas:** tabla de §3a.

**Partida sintética por red simulada** (1 800 cuadros, 4 KB de estado; "profundidad" = rollback más profundo; "re-sim/cuadro" = cuadros
re-simulados por cuadro jugado; todos convergieron al estado de referencia):

| Retraso de ida | Pérdida | Prof. máx. | Re-sim/cuadro | µs por cuadro | B/s por sentido |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 ms | 0 % | 2 | 0,10 | 0,3 | 2 911 |
| 25 ms | 0 % | 3 | 0,20 | 0,3 | 4 363 |
| 50 ms | 1 % | 4 | 0,30 | 0,3 | 5 994 |
| 75 ms | 2 % | 5 | 0,33 | 0,3 | 7 431 |
| 100 ms | 3 % | 7 | 0,51 | 0,3 | 9 092 |
| 150 ms | 5 % | 11 | 0,73 | 0,3 | 12 202 |
| 200 ms | 5 % | 13 | 1,04 | 0,4 | 15 377 |

(Los µs por cuadro son del juego sintético, irrelevantes para el juego real: solo muestran que el núcleo en sí cuesta muy poco.)
