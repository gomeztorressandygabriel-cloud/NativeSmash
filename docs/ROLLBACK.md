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
4. Cierra el juego y manda las líneas `[rbprobe]` de `build\play.log` (cada 5 s salen siete líneas). Léelas antes: no llevan nombres ni
   direcciones de internet, solo números y direcciones de memoria del juego.
5. Borra `rollback_probe.on` para jugar normal: con ella, el juego vigila más memoria y va un poco más lento.

**Qué dice cada línea y qué se espera (versión 3 de la sonda):**

| Línea | Qué mide | Cómo leerla |
| :--- | :--- | :--- |
| `Pages written per frame … alias / heap / stack median / p95 / max (MB)` | Páginas de memoria del juego escritas entre dos cuadros: lo que un rollback copiaría por cuadro. **La del juego es `alias`**: el juego pide su memoria con `MapPhysicalMemory`, que NativeSmash pone en la zona ALIAS | ≲ 1 000 (≈ 4 MB) en combate: viable. 1 000–10 000: caro, mirar las regiones. > 10 000 de forma constante: el enfoque por páginas no compensa (parada de la fase 1) |
| `alias CPU's own …`, `alias handed to the GPU …` (versión 3) | Las páginas de ALIAS separadas: las que están en memoria que el juego entregó a la GPU (asignaciones nvmap: texturas, vértices, constantes), que el rollback no tiene que guardar porque se rehacen al dibujar, y el resto, **la memoria propia de la CPU, donde tiene que estar el estado del combate** | La cifra que decide: `CPU's own` ≲ 1 000 páginas (≈ 4 MB) por cuadro, viable |
| `finding them: avg / max ms` | Lo que cuesta solo *saber* qué páginas cambiaron | Debe quedar por debajo de ~1 ms; si no, el seguimiento ya se come el presupuesto |
| `… frames left out … (renderer busy)` | Cuadros en que la pregunta por ALIAS esperó porque el renderer estaba ocupado; sus páginas cuentan con el cuadro siguiente | Pocos: normal |
| `alias regions written most` (solo memoria propia de la CPU, desde la versión 3), `heap regions written most` | Las zonas de 1 MB más escritas | Dónde vive probablemente el estado de la simulación (y dónde la CPU sube datos a la GPU): el punto de partida para la suma de verificación y para buscar el RNG |
| `pad reads per frame … by N thread(s)` | Cuántas veces y desde cuántos hilos lee el juego el mando | Lo ideal: 1 por cuadro desde 1 hilo |
| `fibers alive` | Fibras vivas | Cada fibra viva es una pila del host que el rollback tendría que restaurar ([§3a](#a-guardar-y-restaurar-el-estado)) |
| `at the vsync tick, guest threads running / waiting / sleeping / in a service request … none running at N of M ticks` | Qué hacen los hilos del juego justo antes de despertarlo para un cuadro nuevo | "none running" cerca del 100 %: hay un momento quieto por cuadro. Hilos "sleeping" o "in a service request" no tocan la memoria en ese instante |
| `threads running at the tick most often (id, priority, core: running / sleeping / in a request / waiting %)` | Los hilos que más a menudo trabajan en ese instante | Quiénes son los que impiden el momento quieto (por su prioridad se puede adivinar si son sonido, carga o dibujo) |
| `stack at a pad read`, `stack at a frame queued` y las líneas que siguen (versión 4) | La cadena de llamadas del juego en una lectura del mando y en una entrega de cuadro, una vez por informe (`main+0x37246a0` es `f_71037246a0` del perfilador) | Qué rama del bucle principal lee la entrada (la simulación) y cuál entrega la imagen (el dibujo) |
| `thread N (running at X% of the ticks) is in:` y las líneas sangradas que siguen (versión 3) | Para los (hasta 3) hilos que trabajan en más de la mitad de los vsync: en qué funciones del juego están en ese instante (`main+0x…`) | Dice qué hacen. Con varias tandas se ve si siempre están en el mismo sitio (un bucle de espera activa, el sonido, la carga) |

**Límites de la sonda:** solo pregunta por la memoria que el juego tiene asignada (no por toda la reserva, que costaba milisegundos). La zona
ALIAS se pregunta a través del renderer, que pone un "sello" a cada página escrita: así la sonda cuenta también las páginas que el renderer
consultó antes, y el renderer no pierde nada. Las páginas que la CPU escribe para la GPU (texturas, vértices) también cuentan en ALIAS. El cuadro
se corta cuando el juego entrega su imagen, no en el límite exacto de la simulación. En macOS la zona ALIAS no se mide. En Linux, si el log dice
`[mem] write watch: page protection (fallback)`, la sonda es mucho más lenta y podría fallar al recibir datos de la red: medir en Windows.

### Primera medición (sonda versión 1, combate 1 contra 1 contra la CPU, Windows 11)

| Dato | Resultado | Lectura |
| :--- | :--- | :--- |
| Lecturas del mando | 1 por cuadro, siempre desde el mismo hilo | Un único punto donde inyectar el mando del rival |
| Fibras vivas | 13–24 (se crearon ~3 900 en el combate, ~18 por segundo) | Pocas pilas a la vez: abordable |
| Trabajo del juego por cuadro (`latency.on`) | ~6 ms de media, casi siempre < 14 ms | Hay margen para re-simular varios cuadros |
| Páginas escritas en el heap | 0 | El juego no usa el heap: su memoria está en ALIAS, que la versión 1 no medía. Corregido en la versión 2 |
| Coste de la sonda | 6–13 ms por cuadro (picos de 20 ms), el juego bajó de 60 cuadros/s | Preguntaba por 10 GB de reserva cada cuadro. Corregido en la versión 2 |
| Momento quieto | Ningún cuadro con todos los hilos esperando (6–7 de 27 activos) | No hay pausa global: confirma el diseño "solo simulación". La versión 2 dice qué hilos son y si duermen |

### Resultado de la prueba de rebobinar (pedido 003): el juego se cae en las fibras

- Compiló (~12,5 min) y el combate automático llegó a la prueba. Ronda 1 (1 cuadro): **4 697 páginas (18,3 MB) guardadas en 14 ms**;
  en ese cuadro se escribieron **49 páginas fuera de las guardadas** y **empezó 1 fibra**.
- **Al repetir el primer cuadro, el juego se cerró**: lectura de la dirección `0xffffffffffffffff` en el hilo principal, con esta pila:
  bucle principal → simulación (`main+0x37246a0`, tres niveles) → … → `main+0x353c670` → **`sdk+0x32f170`** (el cambio de fibra de
  `nn::os`, junto a `FinalizeFiber` en `sdk+0x32f130`) → `DispatchFiberContext` (la función nativa de `hle_fiber.c`).
- **Lectura:** es el obstáculo previsto en [§1](#1-resumen-y-veredicto). La memoria del juego volvió al cuadro guardado, pero las fibras
  (cada una con su pila del host y su `Cpu`, fuera de esa memoria) no: el juego intentó continuar una fibra cuyo estado ya no
  correspondía a la memoria restaurada. **Rebobinar solo la memoria no basta; hay que guardar y restaurar también las fibras.**
- Lo que sí funcionó: encontrar y guardar el estado (18 MB en 14 ms), grabar los mandos y el reloj, y enganchar las funciones del bucle.
- Nota aparte: la compilación movió otro enganche ya existente (`HLE_WRAP(71036149d0)`, de `hle_movie.c`) a `71036f5a70` "por su firma":
  el `main` de este PC no es exactamente el de 13.0.5. Las direcciones del rollback (sacadas de este mismo `main`) son las suyas.

**Lo que haría falta (fase 3b, "guardar las fibras"):** que en Windows las fibras del juego usen la conmutación propia del runtime (como
ya hacen Linux, Android y macOS con `rt_fiber_switch`) en lugar de las fibras de Windows, para poder copiar la parte usada de la pila de
cada fibra, su `Cpu` y la tabla de fibras de `hle_fiber.c` junto con la memoria, y devolverlas a su sitio al rebobinar. En Windows x64
hay que mantener además los límites de pila del hilo (`StackBase`/`StackLimit` del TEB) en cada cambio, como hacen bibliotecas como
Boost.Context. Solo se activaría en el modo rollback; el juego normal seguiría con las fibras de Windows.

**Hecho (fase 3b, en la nube):** `runtime/net_rbfiber.c` son esas fibras propias para Windows x64 (cambio en ensamblador que guarda
rbx, rbp, rdi, rsi, r12–r15, xmm6–xmm15, MXCSR y la palabra de control x87; pila reservada como la de `CreateFiberEx`, con página de
guarda; límites del TEB cambiados en cada salto). Se probaron en Wine: 3 fibras, recursión de 6 MB, excepciones de C++, migración de
una fibra a otro hilo, y guardar → seguir → restaurar → repetir dio el mismo resultado. `hle_fiber.c` las usa **solo** con la prueba
encendida (`SSBU_RB_TEST=1`), y `fibers_save`/`fibers_restore` guardan y devuelven la tabla de fibras, el `Cpu` de cada una y la parte
usada de su pila. `net_rbtest.c` las llama junto con la memoria e informa `fibers saved N (running, not saved: M)` y
`fibers restored N (running, skipped: M; started since, dropped: K)`. Una fibra que esté corriendo en el momento de guardar no se
puede guardar (queda contada). Falta medirlo en el PC: pedido 004 del laboratorio.

**Pedido 004:** no compiló en el PC. El `clang-cl` del proyecto (LLVM 23.1.3) leyó el ensamblador de `net_rbfiber.c` como sintaxis
Intel (`error: unknown token in expression` en cada `%registro`); el clang 18 de la nube lo leía como AT&T. Se arregló diciendo la
sintaxis al principio del bloque (`.att_syntax prefix`), y la prueba en Wine sigue saliendo igual. Se repite como pedido 005.

### Resultado del pedido 005: rebobinar ya no tumba el juego

- Compiló y **el rebobinado funcionó sin caerse**, en 1 cuadro y en 10 cuadros: 5 700 páginas (22 MB) y 26 fibras guardadas en 22–42 ms;
  15 fibras devueltas a su sitio (6 estaban corriendo y no se pueden guardar en ese momento). Es el obstáculo del pedido 003, superado.
- **Repetir un cuadro costó ~5,3 ms** (10 cuadros en 53 ms) en el portátil, sin dibujar. Es la primera medida real del precio de
  re-simular: volver 2 cuadros atrás costaría ~11 ms extra en un cuadro de 16,7 ms. Cabe muy justo; con 3 o más no cabe en ese PC.
- **El estado no salió igual:** distinto en 1 105 de 5 695 páginas (1 cuadro) y 1 768 de 5 556 (10 cuadros). Pero esa comparación no
  separaba causas: la repetición se salta el dibujo y el paso `f_71038fe2b0`, que también escriben memoria (el dibujo es el 66 % del
  cuadro). Así que esas diferencias pueden ser normales y no dicen todavía si la simulación es repetible.
- **Después de la ronda 2 el juego se congeló** (dejó de entregar cuadros, sin cerrarse). La prueba dejaba al juego con el estado
  repetido, sin dibujar; la memoria del dibujo (colas y contadores de la GPU) quedaba atrás respecto a la GPU real, y el bucle principal
  se quedó esperando.

**Cambio para el pedido 006 (la prueba, versión 2):**
1. Repite **dos veces** desde el mismo guardado y compara una repetición con la otra. Eso es lo que necesita el rollback: que el mismo
   guardado más los mismos botones den siempre lo mismo. El dibujo no influye, porque ninguna de las dos dibuja.
2. Separa las páginas distintas según quién las escribió al grabar: la simulación o el resto del cuadro.
3. Al terminar, **devuelve al juego el estado que tenía** (memoria y fibras), como si la prueba no hubiera ocurrido. Así no debería
   congelarse y llegan las 4 rondas (1, 10, 30 y 60 cuadros).
Comprobado en el juego de mentira: sale IDENTICAL entre repeticiones, separa bien lo del dibujo, detecta estado escondido fuera de la
memoria, y el juego acaba exactamente igual que sin la prueba.

### Resultado del pedido 006: la prueba v2, ronda 1 completa

- **Ronda 1 (1 cuadro), completa:** 5 792 páginas y 23 fibras guardadas en 30 ms; las dos repeticiones tardaron **3,1 y 2,3 ms**
  (más barato que en el 005: entonces la primera vez se pagaba la caché fría). **El juego siguió con total normalidad después**: la
  devolución del estado funciona, y el congelamiento del 005 desapareció.
- **Repetición 1 contra repetición 2: distintas en 94 de 5 792 páginas** (81 escritas por la simulación). Muy cerca, pero no
  idénticas: algo de lo que la simulación usa no se guarda todavía.
- **Repetición contra el juego: 1 212 páginas distintas, 1 017 solo del dibujo.** Confirma que casi toda la diferencia del 005 era el
  dibujo que la repetición se salta, no la simulación.
- **Ronda 2 (10 cuadros): el juego se cerró en la primera repetición**, leyendo `0x688a3a2838` (dentro de la zona de memoria del
  juego, pero sin memoria detrás) desde el código de los personajes (`lua2cpp_krool`, ejecutado como tarea del sistema de trabajos
  del juego, `main+0x353d1b0`). En esos 10 cuadros se escribieron **116 páginas fuera de las guardadas**, y empezaron 4 fibras.

**Lectura:** Smash ya es determinista de por sí: su propio online manda solo los botones y cada consola simula lo mismo. Así que las
diferencias entre repeticiones no son azar del juego, sino **estado que la prueba no devuelve**. Las causas más probables, en orden:
(1) **las páginas que se escribieron fuera del conjunto guardado** (16 en un cuadro, 116 en diez: con un puntero viejo ahí, el código
lee memoria que ya no existe, como en el cierre); (2) las 6 fibras que estaban corriendo en los hilos de trabajo y no se pueden
guardar; (3) objetos del núcleo (eventos, esperas) fuera de la memoria del juego.

**Cambio para el pedido 007 (la prueba, versión 3):** el conjunto de páginas guardadas **crece de ronda en ronda**: las que una
ronda vio escritas fuera se guardan en la siguiente, y el descubrimiento dura 120 cuadros en vez de 60. Hay 6 rondas (1, 1, 5, 10,
30 y 60 cuadros). Si las páginas que se escapan bajan a 0 y las repeticiones salen idénticas, la causa era la (1). Además, donde las
repeticiones difieran, la prueba escribe las primeras palabras distintas (repetición 1 / repetición 2), para ver si son punteros,
contadores o números de la física.

### Resultado del pedido 007: cuatro rondas seguidas, y una pista clara

- **Rondas 1 a 4 (1, 1, 5 y 10 cuadros) completas**, con el combate siguiendo normal después de cada una. El conjunto guardado creció de
  6 744 a 8 382 páginas (26 → 33 MB), y las páginas que se escapan bajaron a 2–28 por ronda. **Repetir 10 cuadros: 30 ms, 3 ms por
  cuadro.** La ronda 5 (30 cuadros) se cerró al empezar a repetir (escritura en `0xffffffffffffffa0` en la lectura de mandos,
  `main+0x3667300`): con 30 cuadros se escaparon 78 páginas y empezaron 8 fibras.
- **Las repeticiones siguen discrepando en ~90 páginas, y siempre en las mismas** (`0x604035e000`, `…35f000`, `…360000`, `…3c8000`,
  `…692000`, `…758000`), fuera cual fuera la ronda o el número de cuadros. Las palabras distintas:
  - `0x6040758000+0x280` y `0x6040692000+0xd38/+0xd40` son **contadores** que crecen ronda a ronda (`0xe89`, `0x130a`, `0x1797`…), y la
    repetición 2 lleva unos pocos más que la 1: **6–9** cuando las dos tardaron lo mismo, **34** en la ronda 3, donde la repetición 2
    tardó 43 ms frente a 17. **Cuentan tiempo real, no cuadros.**
  - `0x60403c8000+0xcd0…` es el estado de un **generador de números aleatorios** (de tipo xorshift: las cuatro palabras de 32 bits se
    corren una posición en cada número sacado). Las dos repeticiones lo dejan en puntos distintos.
  - `0x604035e000…0x6040360000` son números de coma flotante completamente distintos (posiciones o velocidades de algo).
- **Lectura:** algo que **no es la simulación que se repite** sigue tocando esas páginas mientras se repite: otro hilo del juego que va
  a su ritmo (efectos, sonido, la cámara…) y que además saca números del mismo generador aleatorio. Al repetir, ese hilo no vuelve atrás,
  y la cantidad de veces que actúa depende de cuánto tarda la repetición. Eso explica las diferencias, y también que no desaparezcan
  guardando más páginas.

**Cambio para el pedido 008 (la prueba, versión 4):** desde la segunda ronda, las páginas en las que las repeticiones discreparon se
ponen **de solo lectura** mientras se repite. Cada escritura salta a un manejador que apunta **qué hilo escribe y desde qué código**
(con la traza del juego), deja pasar esa única instrucción (paso a paso) y vuelve a proteger la página. Así sabremos qué hilo es y si
se puede pausar o repetir con él. Rondas: 1, 1, 1, 5 y 10 cuadros (las de 30 y 60 se quitan hasta resolver esto). Probado en Wine: con
un solo hilo cuenta todas las escrituras; con dos hilos a la vez se le escapan algunas, pero nombra a los dos.

### Resultado del pedido 008: hay hilos de fuera de la simulación escribiendo ahí

- Tres rondas de 1 cuadro completas, con el juego normal después de cada una. Repetir: 3,6–8,8 ms por cuadro.
- **Quién escribe las páginas en las que las repeticiones discrepan, mientras se repite:**
  - **Hilo 19** (prioridad 29, el más ocupado después del principal: ~30 s de CPU en el pedido 005; código en `main+0x3962c30`,
    `main+0x39874b0`…): `0x6040760000`, `0x6040815000`, `0x6040816000`, `0x6053e88000`, `0x6060e80000`, `0x6060e81000` (hasta ~300
    escrituras en una repetición).
  - **Hilo 11** (prioridad 29, `main+0x3810660`): `0x6040692000` (los contadores del 007). **Hilo 47** (prioridad 29): `0x6060fab000`.
  - **Hilo 1** (el principal, el que repite): `0x60403ca000`, más de 60 veces (es el generador aleatorio que se vio en el 007). Esa sí
    es la simulación: si ahí sale distinto es porque **lee** algo que otro hilo cambió.
  - **Nadie** escribe durante la repetición `0x604035e000`–`0x6040360000` (los números de coma flotante del 007) ni otras ~10. Pero
    salen distintas: alguien las cambia **fuera** del momento vigilado (entre devolver las páginas y empezar a repetir, o entre
    terminar y medir), o por un camino que no pasa por esa dirección.
- **Después de la ronda 3 el juego se paró en el hilo 19**: "llamada indirecta a `0x3e0ab1c300000003`" (`main+0x39760d0` ←
  `main+0x3962c30` ← `main+0x39874b0`). La prueba le cambió la memoria por debajo al devolver las páginas guardadas: sus datos
  volvieron atrás mientras él seguía adelante, y acabó saltando a una dirección basura.
- **Lectura:** los hilos 11, 19 y 47 no son la simulación. Van a su ritmo (probablemente sonido, efectos o similar) y escriben en
  páginas que la prueba rebobinaba. Rebobinar esas páginas **no tiene sentido** (no son estado del combate) y además **es peligroso**
  (lo que tumbó al hilo 19). Las páginas que solo escribe un hilo así hay que dejarlas fuera, igual que el dibujo.

**Cambio para el pedido 009 (la prueba, versión 5):**
1. Una página en la que se ve escribir a otro hilo **queda fuera** de los rebobinados y de las comparaciones a partir de ese momento
   (se cuenta, y también cuántas de esas escribe además el hilo que repite, porque en esas los datos de la simulación tampoco se
   rebobinarían).
2. La vigilancia cubre todo el tiempo desde que se devuelven las páginas hasta que se miden, no solo la repetición. Además se vigila
   **el siguiente cuadro normal del juego**, para ver quién escribe ahí sin repetición de por medio.
3. Para las páginas sospechosas, la prueba dice **en qué momento** cambian: al devolverlas, antes de la repetición, durante o después.
Probado en Wine con un juego de mentira que tiene un "hilo de sonido" escribiendo sin parar: la ronda 1 lo detecta, lo deja fuera, y
de la ronda 2 en adelante las repeticiones salen **IDENTICAL**.

**Pedido 009:** el juego se cerró a los ~203 s, **antes de que la prueba guardara nada** (solo salió la línea de inicio): lectura de
`0x6095a9e000` (zona de memoria del juego sin memoria detrás) en el propio bucle principal (`main+0x3746e90` ← `main+0x2c5620`). En
ese momento la prueba solo estaba mirando qué páginas se escriben, lo mismo que en los pedidos 007 y 008, que pasaron de ahí sin
problema; lo nuevo de la versión 5 no se ejecuta todavía en ese punto. Puede ser un cierre del juego que no tiene que ver con la prueba,
o algo de la prueba en ese momento. Se repite igual (pedido 010), con dos líneas más en el registro (`finding the pages` y `saving`)
para saber en qué paso exacto ocurre si vuelve a pasar.

### Resultado del pedido 010: casi idéntico en la ronda 1, y otra vez un hilo de fuera

- Esta vez la ronda 1 sí llegó: 5 775 páginas y 13 fibras guardadas en 21,5 ms, y las dos repeticiones **solo discreparon en 4
  páginas escritas durante la simulación** (`0x6040476000`, `0x6062601000`, `0x6062602000`, `0x6075a01000`) y 37 del resto del cuadro.
  (Repetir tardó 0,1–0,4 ms: en ese cuadro la simulación hizo poco, quizá un momento tranquilo del combate.)
- En el cuadro normal siguiente, `0x6075a01000` la escriben **los hilos 28 y 29** desde código de `subsdk0` (una biblioteca del sistema,
  no la simulación). Las otras tres no las escribió nadie en ese cuadro.
- **Justo después el juego se cerró en el hilo 37** (prioridad 44, código de `subsdk0`, `main+0x36f3f20`): lectura de la dirección
  `0x4`, en la misma función de copia (`sdk+0xfeb0`) con la que el hilo 28 escribe esa página. Es el mismo mecanismo que tumbó al hilo
  19 en el 008: al devolver el estado, la prueba le puso a una biblioteca que va a su ritmo datos de unos milisegundos antes (punteros
  de `0x6075a01000` distintos entre repeticiones), y uno quedó vacío.
- El cierre del 009, en el bucle principal, puede ser el mismo problema por otro lado.

**Cambio para el pedido 011 (la prueba, versión 6): un censo antes del primer guardado.** Durante 10 cuadros todas las páginas
guardadas se ponen de solo lectura, una vez durante la simulación y otra fuera de ella en cada cuadro. La primera escritura de cada
página en cada tramo apunta qué hilo fue y la deja escribir. Un hilo que no sea el principal y que escriba **fuera** de la simulación va
a su ritmo (sonido, efectos, bibliotecas del sistema): todas las páginas que toca quedan fuera de los rebobinados **desde la ronda 1**.
Los hilos que solo escriben mientras corre la simulación (los ayudantes del sistema de trabajos) se mantienen. Probado en Wine con un
juego de mentira con un ayudante y un "hilo de sonido": el censo separa bien los dos, y **la ronda 1 ya sale IDENTICAL**.

### Resultado del pedido 011: el censo dice que separar por hilos no basta

- El censo de 10 cuadros funcionó y dio la foto más completa hasta ahora de quién escribe la memoria del combate (5 732 páginas):
  - **El hilo principal escribe 29 páginas mientras corre la simulación y 1 297 fuera de ella.** Casi todo lo que hace en un cuadro
    (el dibujo, entre otras cosas) va fuera de la función que se tomó como "la simulación".
  - **Solo dos hilos escriben únicamente durante la simulación**: el 12 (382 páginas) y el 13 (356). Son ayudantes de verdad.
  - **Otros 20 hilos escriben dentro y fuera.** Entre ellos están **el 3 y el 4, los trabajadores del sistema de tareas** del juego
    (`main+0x353d1b0`), que ejecutan el código de los personajes (`lua2cpp_*`), y el 30 y el 44, con más de 1 000 páginas cada uno.
  - Con la regla de la versión 6 ("quien escribe fuera de la simulación va a su ritmo") quedaron fuera **3 293 de 5 732 páginas**.
    Eso incluye trabajo de la simulación: los mismos hilos hacen tareas del combate y tareas de otras cosas.
- Después del censo el juego se cerró en el hilo 3 al escribir en `0x60864751b8`. Fue un **fallo de la prueba**: al terminar el censo
  se apagaba el manejador antes de volver a dejar escribir en todas las páginas, y una escritura en ese instante no tenía quien la
  dejara pasar. Está corregido (primero se desprotege, luego se apaga).

**Conclusión de la línea "rebobinar memoria por páginas":** las páginas y los hilos están demasiado mezclados en este juego. Un mismo hilo
hace trabajo del combate y de otras cosas, y una misma página puede tener datos de los dos. Seguir afinando reglas por página o por hilo
es dar vueltas al mismo tema. Lo que sí quedó demostrado: el juego se deja guardar y rebobinar (fibras incluidas) sin romperse, y
repetir un cuadro cuesta 3–4 ms en el portátil. **El siguiente método para el rollback**, cuando se retome, será guardar **los objetos
del combate** (luchadores, escenario, objetos, el generador aleatorio de la partida), localizados por sus estructuras, en vez de la
memoria por páginas. Mientras tanto el proyecto sigue con el **paso A** ([NETPLAY.md](NETPLAY.md)): VS en red con poco retraso, que no
necesita rebobinar.

### 2026-10-27, tarde: buscando por qué rebobinar tumba el juego

Corridas `every10`–`every24` (5 vueltas atrás de 1–7 cuadros cada una, combate contra la CPU a 144p). Terminaron sin caídas 5 de 15;
las demás se cayeron (casi siempre en una tarea de luchador: excepción de su script `L2CE`, o leer un puntero nulo en
`main+0x37ae210`, siempre desde el sistema de trabajos `main+0x353d1b0` / `main+0x37cc0e0`) o se colgaron.

Lo que se cambió, en orden:

- **Despertar a un hilo retenido.** La repetición despierta 4–80 veces a hilos retenidos (sobre todo el 11). Saltárselos al despertar
  colgaba la repetición. Ahora sí se los despierta, pero al devolver cada guardado su espera vuelve a como estaba al retenerlos
  (`kernel_hold_undo`): el aviso no pasó en la memoria devuelta.
- **"Mismo sitio" ahora incluye los registros.** Además de la espera y la fibra, la clave de cada hilo incluye sus registros
  guardados por el llamado (x19–x30 y sp). x0–x18 cambian de un cuadro a otro (son temporales y no se usan al volver de la espera).
  Con todos los registros nunca coincidía nada. Si al ir a rebobinar algún hilo no está donde estaba en ese guardado, la vuelta atrás
  se aplaza al cuadro siguiente; casi siempre coincide a la primera.
- **Eventos del kernel** guardados y devueltos con cada guardado.
- **Sin cerrojo** al copiar las páginas de vuelta (una página protegida de más ya no puede dejar al hilo principal esperándose a sí mismo).
- **Diagnóstico:** si una vuelta atrás tarda más de 5 s se escribe dónde está el hilo principal; se cuentan las peticiones a servicios
  durante las repeticiones.

Lo que se descartó:

- **No es copiar solo lo que cambió:** con eso casi apagado (`SSBU_RB_IDLE=100000`) también se cae.
- **No son hilos en otro sitio:** las caídas pasan también después de vueltas atrás en las que todos los hilos estaban exactamente en
  el mismo sitio (espera, fibra y registros).

**Lo que queda sin rebobinar** y es sospechoso:

- Las **peticiones a servicios** durante las repeticiones (7–30 por vuelta atrás: `mii`, `audout` —búferes de sonido—, `hwopus`).
  Cambian el estado del servicio en el runtime, que no se devuelve.
- El estado del **runtime** que el código del juego toca sin pasar por la memoria del juego (funciones reemplazadas por el runtime, las
  fibras de los hilos que corrían).
- Los **registros de coma flotante** guardados por el llamado (d8–d15), que la clave no mira.

Hasta resolver esto, la prueba sirve para medir y comparar, pero rebobinar todavía no es seguro.

### 2026-10-27: copiar solo lo que cambió, y lo que falla al rebobinar

**Copiar solo lo que cambió.** Una página guardada está "activa" (se copia en cada guardado) o "vigilada" (de solo lectura, no se
copia). Cada cuadro, 1/16 de las activas pasan a vigiladas (`SSBU_RB_IDLE`) justo después de copiarse. La primera escritura a una
vigilada la vuelve activa, y antes su contenido va a los guardados hechos desde que se vigila, que no la copiaron. Al devolver un
guardado, una página vigilada desde antes de ese guardado ya está como entonces y no se toca.

- **Resultado:** se copian ~3 400–4 100 páginas (13–16 MB) de ~6 000–8 000. Copiar baja de 5,5–13 ms a **2,7–4 ms**; vigilar cuesta
  0,2–1,2 ms (~250 páginas por cuadro van y vuelven). El guardado entero queda en **5–7 ms** de media (antes 8–13), con picos de
  10–35 ms. Congelar los hilos sigue costando ~1,5–2 ms. En corridas seguidas, con el PC más cargado, subió a 8–9 ms.
- **Las correcciones siguen exactas:** la repetición 1 y la 3 difieren solo en las 2 páginas de siempre.
- **Sin rebobinar es estable:** 100 s guardando en cada cuadro (2 600 guardados) sin caídas (`reports/every6.log`).
- **Arreglado por el camino:**
  - `ReadFile` de Windows falla (en vez de avisar) si escribe en una página de solo lectura. La lectura de archivos del juego
    (`hle_fs.c`) toca antes cada página de destino mientras la prueba está encendida.
  - Dos carreras en la copia al escribir: ahora la página se hace escribible antes de borrar su marca.

**Lo que falla: rebobinar a veces tumba el juego**, segundos después o durante una repetición, siempre en tareas de luchadores que
corren los hilos trabajadores (`lua2cpp_*`, desde `main+0x353d1b0` / `main+0x37cc0e0`). Pasaba también antes de copiar solo lo que
cambió (`every1.log`). Lo que se encontró:

- Los hilos retenidos mientras se repite se **despiertan** igual: 4–80 veces por vuelta atrás, casi siempre el hilo 11. Al soltarlos
  siguen como si los hubieran avisado, aunque la memoria devuelta dice que nadie lo hizo. Ahora quien despierta (mutex, árbitro,
  variable de condición) se salta a los hilos retenidos, pero las esperas por objetos (`WaitSynchronization`) aún los despiertan, y
  devolver el guardado sigue esperando a veces 300–900 ms a que los hilos vuelvan a sus esperas de entonces.
- Los **eventos del kernel** (encendido/apagado) no están en la memoria del juego: ahora se guardan y se devuelven con cada guardado
  (`kernel_events`).
- En una corrida (`every9.log`) un luchador lanzó una excepción de su script (`lua2cpp_pit`) en un trabajador durante la repetición.

**Lo siguiente:** que rebobinar sea estable. Hay que decidir qué hilos corren mientras se repite y cómo vuelven exactamente a sus
esperas; es la parte del "estado fuera de la memoria" que aún no está entera.

### 2026-10-27: **guardar en cada cuadro** (`SSBU_RB_EVERY=1`)

Ya no hay 120 cuadros de búsqueda. Un anillo de `SSBU_RB_RING` guardados (8 por defecto) toma uno al empezar la simulación de cada
cuadro, con los demás hilos quietos: las páginas, las zonas por hilo y las fibras.

- **Qué páginas:** desde el principio toda la memoria del juego (la zona alias y las variables globales de los módulos) es de solo
  lectura. La primera escritura a una página la suma a las que se guardan; su contenido de ese momento va también a los guardados ya
  hechos, porque nadie la había escrito. Arranca en ~5 900 páginas y crece a ~8 800 (34 MB) en un minuto de combate.
- **Fibras:** en el anillo, una fibra terminada solo guarda sus campos, sin la pila (nunca vuelve a correr).
- **Rebobinar:** cada `SSBU_RB_GAP` s (5 por defecto) se vuelve 1, 2, 4, 7 y 7 cuadros atrás. Se repite con los botones grabados (1),
  con otros (2) y otra vez con los grabados (3), y el juego sigue desde su propio guardado (`SSBU_RB_CONTINUE=1`: desde la repetición 3).
  Al devolver un guardado, primero se espera hasta 300 ms a que los hilos estén en las mismas esperas que entonces.
- **Arreglado:** el hilo gráfico del PC no se congela y escribió en una página justo entre hacerla de solo lectura y encender el
  manejador. Ahora la marca se pone antes de proteger, y al terminar se desprotege antes de apagar.

**Resultado** (`reports/every3.log`, combate contra la CPU a 144p): las 5 vueltas atrás completas, **sin caídas, y el juego siguió
jugando**. La repetición 1 y la 3 difieren solo en las 2 páginas de siempre (`0x60406a0000`, `0x605eea0000`, que no son de la
simulación). Con otros botones el combate cambia (4 a 6 páginas en 1–7 cuadros). Repetir cuesta ~2 ms por cuadro en combate.

**El problema: guardar cuesta demasiado en este portátil.** Copiar 24–34 MB cada cuadro tarda **5,5–13 ms de media**, con picos de
30–500 ms. Congelar los hilos suma 1,5 ms, y a veces 6–18 ms. El PC estaba sin memoria libre (el juego usa 6 GB de 7,7), así que la copia
va a ~3 GB/s. Con 16,7 ms por cuadro no cabe. Faltan dos caminos, y quizá los dos:

1. **Guardar solo el estado del combate:** la repetición y el juego "en vivo" difieren en 700–1 250 páginas, que el dibujo escribe y la
   simulación no rehace. Si la simulación no las lee, sobran en el guardado, y son más o menos la mitad.
2. **Copiar solo lo escrito desde el último cuadro**, en vez de todo cada vez.

### 2026-10-27: **una predicción equivocada se corrige exacta** (repetir con otros botones)

La repetición 2 se juega con otros botones (A pulsado y la palanca a la izquierda en cada lectura de mandos), como una predicción
equivocada del mando del otro; la 3 vuelve a los botones grabados. Rondas de 1, 10, 30, 60 y 60 cuadros:

- Con otros botones el combate se va por otro camino: 0, 11, 164, 297 y 225 páginas de la simulación distintas.
- **La repetición 3 sale idéntica a la 1 en todas las rondas** (0 páginas de la simulación distintas), aun después de 60 cuadros de
  predicción equivocada. Es exactamente lo que hace un rollback: rebobinar y volver a simular con el botón correcto.
- Después de la última ronda de esa prueba el juego se cayó en una tarea de un luchador (`lua2cpp_gamewatch`, leyendo
  `0xffffffffffffffff`). Sospecha: un ayudante que, tras repetir, queda esperando en el mismo sitio pero **en otra fibra**. Desde
  entonces la comparación de "en qué espera cada hilo" incluye la fibra en la que está (`kernel_set_fiber`, desde `hle_fiber.c`).
  Con eso, dos corridas más (una en pleno combate: 10, 134, 304 y 204 páginas distintas con otros botones, **0 al corregir** en todas
  las rondas) terminaron sin caídas y el juego siguió jugando. No se volvió a ver, pero tampoco está demostrado que fuera eso.

### 2026-10-27, de madrugada: **la simulación se rebobina y se repite idéntica**, hasta 60 cuadros

La pieza que faltaba: **las variables globales de los módulos** (`.data` y `.bss` de `main`, `sdk`, `subsdk0` y los `lua2cpp_*`, las
regiones `GM_CODE_DATA` del kernel, ~20 000 páginas en 12 regiones) no estaban en el rebobinado: la prueba solo miraba el montón y las
pilas. Ahí viven contadores, objetos únicos y estados aleatorios del juego; el bucle que colgaba el juego después de una ronda de 30
cuadros (`main+0x356cb80`) empieza sumando 1 a uno de ellos (`0x7105334a08`). Entran con la misma **copia al escribir** que el resto.

Resultado (combate automático contra la CPU, `SSBU_RB_FREEZE=1 SSBU_RB_ROUNDS=1,10,30,30,60`):

| Ronda | Cuadros | Repetir | Páginas de la simulación distintas (1 vs 2, 2 vs 3, 1 vs 3) |
|---|---|---|---|
| 1 | 1 | 1,6 ms | 1, 0, 1 (una página que cambió entre medirla y copiarla: 0 palabras distintas) |
| 2 | 10 | 28–33 ms | **0, 0, 0** |
| 3 | 30 | 29 ms | **0, 0, 0** |
| 4 | 30 | 16–18 ms | **0, 0, 0** |
| 5 | 60 | 23–25 ms | **0, 0, 0** |

Sin caídas, sin cuelgues, y el juego siguió normal después. Quedan 2 páginas que el resto del cuadro (no la simulación) deja distintas
en todas las rondas, siempre las mismas.

**Lo que esto demuestra:** el combate de Smash Ultimate recompilado se puede guardar, rebobinar y volver a simular de forma
determinista en el mismo PC, a menos de 1 ms por cuadro. Es la base de un rollback.

**Lo que falta para un rollback de verdad:** guardar en cada cuadro (hoy la prueba guarda cada 10 s, con 120 cuadros para descubrir
qué páginas se escriben; con copia al escribir ese descubrimiento sobra), sin congelar más de lo necesario; repetir cuadros con
**entradas distintas** (la prueba repite con las mismas); y, sobre todo, conectarlo al juego en red (Local Wireless o el VS propio) y
comprobar que dos PCs distintos simulan igual.

### 2026-10-27: congelar todos los hilos y rebobinar todo junto (`SSBU_RB_FREEZE=1`)

Como hace un emulador con la consola entera: en vez de separar páginas por hilo, se rebobina **todo** lo que el juego escribe, con los
demás hilos quietos.

- **Congelar** (`kernel_freeze` en `kernel.c`): un hilo del juego que espera en el kernel necesita el candado del kernel para seguir;
  el hilo principal lo toma cuando todos los demás están esperando (o durmiendo en `SleepThread`, o dentro de una petición al sistema:
  al salir de esas dos se quedan en una compuerta). Los 23 hilos quedan quietos en ~1 ms.
- **Retener mientras se repite** (`kernel_hold` / `kernel_release`): durante la repetición solo corren el hilo principal y los
  ayudantes de la simulación (`SSBU_RB_HELPERS`, por defecto 3, 4, 12 y 13); los demás, aunque los despierten, vuelven a esperar.
  Con un vigilante: si en 3 s no acaba, los suelta y apunta dónde estaba cada hilo.
- **Comparar con todo quieto**: los ayudantes pueden seguir terminando tareas del cuadro después de que la simulación vuelve.
- **Las zonas por hilo del kernel (TLS)** se guardan y se devuelven también: un valor por hilo que el juego cambia cada cuadro no se
  rebobinaba.
- La lectura de mandos: un cuadro que se simula sin leer los mandos antes ya no tumba la repetición.
- El azar del sistema (`GenerateCryptographicallyRandomBytes`) se graba y se repite, aunque el juego no lo pidió en ningún cuadro.

**Resultado** (combate contra la CPU, rondas de 1, 1, 1, 5 y 10 cuadros): **las 5 rondas completas, sin caídas y sin que el juego note
nada después**. Repetir cuesta **0,4–2 ms por cuadro** (10 cuadros en 4–15 ms). Las dos repeticiones del mismo cuadro difieren en
**0 a 4 páginas** de la simulación (sin congelar eran ~70), y ninguna página escrita queda sin guardar. Una de las rondas salió
idéntica.

**Lo que falta:** en las demás rondas, un arreglo de vectores (paso 0x70, escrito por el hilo principal desde `main+0x36b5350` ←
`main+0x4a9280` ← `main+0x4a6820` ← `main+0x616580` ← `main+0x3ad190`) sale 0 en la primera repetición y con valor en la segunda:
algo que no está en la memoria del juego cambia durante la primera. Sospechosos: las 6 fibras que corren en el momento del guardado
(no se guardan), estado del runtime (servicios emulados) o valores que el hilo gráfico del PC escribe en la memoria del juego.

#### Las últimas diferencias, y el siguiente obstáculo: las fibras que nacen dentro de la ventana

- **Una tercera repetición** separa las diferencias que quedan en tres tipos: un generador aleatorio que saca **un número de más** en
  una repetición sí y en otra no (la 1.ª y la 3.ª coinciden), algo que **se inicializa una vez** durante la 1.ª repetición (la 2.ª y la
  3.ª coinciden), y unas pocas páginas distintas en cada repetición.
- **No es la función del azar del combate:** envuelta (`main+0x366f660`), las tres repeticiones la llaman las mismas veces, desde los
  mismos sitios y en el mismo orden. Tampoco el reloj de los ayudantes (no lo leen al repetir) ni el azar del sistema (no se pide).
- **No crecen:** con 10 cuadros siguen siendo 2–7 páginas, como con 1. Una diferencia del combate (posición, daño) crecería cuadro a
  cuadro; esto apunta a efectos o caché, no a la lógica del combate. Falta confirmarlo con 30 y 60 cuadros.
- **El obstáculo nuevo:** en una ronda de 10 cuadros el juego creó **10 fibras nuevas** dentro de la ventana (`phx::Fiber::setup`,
  `main+0x353c820`, llamado desde el código de un luchador). Al repetir, crear esas fibras otra vez **abortó el juego** (`abort` desde
  `main+0x37c97f0`, junto a `nn::os::Mutex`). La tabla de fibras del runtime (`hle_fiber.c`) reutiliza los huecos de las fibras
  terminadas, y una fibra que corría en un hilo retenido al guardar no se guarda: si termina dentro de la ventana y su hueco se reutiliza,
  al devolver el estado la tabla no cuadra con la memoria del juego. En las rondas sin fibras nuevas no pasa.
- **Lo siguiente:** guardar también las fibras que corren en otros hilos (están quietas mientras los hilos esperan) y no reutilizar
  huecos de fibras mientras dura una ventana; después, repetir 30 y 60 cuadros para ver si las diferencias crecen.

#### Copia al escribir, y ventanas de 10 y 30 cuadros

- **Los huecos de fibras ya no se reutilizan** mientras corre la prueba (`hle_fiber.c`): una fibra nacida dentro de la ventana tomaba
  el hueco de una terminada, y al rebobinar la tabla de fibras no cuadraba con la memoria del juego.
- **Copia al escribir** (`cow_begin` en `net_rbtest.c`): al guardar, toda la memoria del juego que no está en la lista de guardado
  (~480 000 páginas, en ~4 000 tramos) queda de solo lectura; la primera escritura de cualquier hilo copia la página tal como estaba y
  la deja escribir. Se devuelve con las demás. En ventanas con efectos nuevos el juego escribe páginas que no había tocado en los 120
  cuadros anteriores (32 en 10 cuadros, 100 en 30), y sin esto la segunda repetición leía basura y se caía.
- **Resultado:** rondas de 1, 10 y 30 cuadros completas, con 17 y 47 fibras nuevas dentro de la ventana. Repetir cuesta ~3 ms por
  cuadro. Las repeticiones difieren en 2, 4–6 y 8–10 páginas: crecen poco con la longitud de la ventana.
- **El siguiente obstáculo:** después de devolver el estado real tras la ronda de 30 cuadros, el hilo principal se quedó esperando un
  candado (`nn::os::Mutex`) para siempre, dentro del código de un luchador (`lua2cpp_dolly`, en una tarea del sistema de trabajos
  `main+0x353d1b0`). Un candado tomado dentro de la ventana queda marcado en la memoria devuelta sin que nadie lo vaya a soltar, o al
  revés: la memoria del juego y las colas de espera del kernel (en el runtime) se desacompasan.

### 2026-10-26: ¿vive el combate en zonas de memoria propias? No

Prueba: un combate automático contra la CPU (ventana de 256x144, `SSBU_RB_TEST=1`), con dos sondas nuevas en `net_rbtest.c`:

- **Los allocators de Nintendo** (`nn::mem::StandardAllocator::Initialize`, envuelto en `sdk+0x2e7600`): el juego crea **uno solo, de 1 MB**
  (desde `main+0x36f83a4`). Ninguna de las ~5 700 páginas que escribe un cuadro de combate cae dentro. El juego reparte su memoria con
  un allocator propio, dentro de `main`, sobre la memoria que pide al sistema.
- **El censo por zonas de 1 MB** (quién escribe cada página en 10 cuadros): de 547 páginas que solo escribe la simulación, **442 están
  en zonas de 1 MB donde también escriben hilos que van a su ritmo** (sonido, efectos, sistema). Solo 105 viven en zonas limpias.
  Los objetos del combate están intercalados con los de otras cosas.

**Conclusión:** guardar "las zonas del combate" no funciona tal cual: no hay zonas del combate. Quedan dos caminos:

1. **Parar todos los hilos del juego y rebobinar todo junto** (como un emulador): exige guardar también el estado de cada hilo (su pila
   del host y sus registros), como ya se hace con las fibras. Es la idea 2 de la lista; nada la descarta todavía.
2. **Localizar los objetos del combate uno por uno** (luchadores, artículos, escenario, generador aleatorio) por sus estructuras:
   lento, pero no depende de los hilos.

Además, en las tres rondas las dos repeticiones del mismo cuadro **siguieron difiriendo en 30–70 páginas**, con números de coma flotante
(posiciones) distintos: los hilos 3 y 4 (los trabajadores de tareas del juego) hacen trabajo del combate y también de otras cosas, y al
dejar sus páginas fuera, parte del combate no se rebobina. Cualquier rollback tiene que incluirlos.

### La prueba de "rebobinar" (`runtime/net_rbtest.c`, fase 3)

Apagada salvo con `SSBU_RB_TEST=1` (o un archivo `rollback_test.on`). Engancha las dos funciones del bucle principal con `HLE_WRAP`
(firmas sacadas en el pedido 002 del laboratorio, cada una con una sola coincidencia): `7103667300` (lee los mandos) y `71037246a0`
(simula; se llama a sí misma, así que solo cuenta la llamada exterior). A partir de `SSBU_RB_TEST_AT` segundos (200 por defecto) hace
4 rondas, de 1, 10, 30 y 60 cuadros:

1. Durante 60 cuadros apunta qué páginas de ALIAS escribe el juego (el "conjunto caliente", hasta 64 MB).
2. Al empezar la simulación de un cuadro, copia esas páginas.
3. Graba N cuadros: lo que devuelve cada lectura del mando y cada lectura del reloj (`rt_ticks`) en el hilo principal, y con qué
   argumentos se llamó al paso de mandos y a la simulación.
4. Al llegar al cuadro N: huella de cada página guardada, y cuántas páginas fuera del conjunto se escribieron (las que el guardado no cubre).
5. Rebobina (vuelve a copiar las páginas guardadas) y repite los N cuadros (simulación y paso de mandos, sin dibujar), con los mandos y el
   reloj grabados.
6. Compara las huellas: **IDÉNTICO** o **DIFERENTE** en cuántas páginas y cuáles.

No guarda el estado del host (pilas de las fibras, objetos del kernel) ni lo que hacen los hilos trabajadores, ni repite el paso 3 del
bucle (`f_71038fe2b0`): justo eso es lo que la prueba descubre. Si sale diferente, o el juego se cae al repetir, el guardado no basta.
Probada en la nube con un juego de mentira (idéntico si es determinista; diferente si tiene estado escondido); con el juego real, en el
pedido 003 del laboratorio.

### El laboratorio automático (rama `cloud/rollback-lab`)

Desde el pedido 001, las pruebas en el PC del usuario las hace un agente local (Antigravity) sin que el usuario juegue: lee el pedido en
`lab/PEDIDO.md` de la rama `cloud/rollback-lab`, arranca el juego con botones guionizados (`tools/dev/autobattle.sh`, con una copia de la
partida guardada) y sube el resultado a `lab/resultados/`. Reglas en `lab/README.md` de esa rama.

**Pedido 001 (combate automático con la sonda v4):** funcionó al segundo intento. En este PC el juego tarda ~133 s en dar su primera imagen,
así que el guion de `autobattle.sh` necesita sus tiempos **+125 s**; el combate empieza hacia el segundo 171. Los números coinciden con las
mediciones a mano (ALIAS ~2 400–2 600 páginas por cuadro, 1 lectura del mando por cuadro, mismas pilas). La primera función de la pila de la
lectura del mando sale unas veces como `lua2cpp_item+…` y otras como `lua2cpp_gamewatch+…`: es la función nativa `GetNpadState` (fuera de los
módulos del juego) nombrada por el módulo más cercano, no código de esos módulos.

### Cuarta medición: dónde lee el mando y dónde entrega el cuadro (sonda versión 4)

Las 46 pilas de cada tipo fueron idénticas:

- **Lectura del mando:** bucle principal `main+0x3746e90` → **`main+0x3667300`** → `main+0x3666880` → `GetNpadState`. El mando se lee en **una rama propia del bucle principal** (`f_7103667300`, ligera: no sale en el perfil), no dentro de `f_71037246a0`.
- **Entrega del cuadro:** bucle principal → **`main+0x386f850`** (`f_710386f850`) → cadena del SDK → `QueueBuffer`. Confirma que la rama del 66 % es **el dibujo**.
- (`sdk+0x29d9e0` → `main+0x2c5620` → `main+0x3746e90`: el arranque del hilo principal y su bucle.)

**Cómo queda el bucle principal de cada cuadro (hipótesis):**

| Paso | Función | Papel |
| :--- | :--- | :--- |
| 1 | `f_7103667300` | Leer los mandos y guardarlos en la memoria del juego |
| 2 | `f_71037246a0` | **Simular** (recorre un árbol de tareas: se llama a sí misma a través de `f_71022defb0`) |
| 3 | `f_71038fe2b0` | Sin identificar (5 %) |
| 4 | `f_710386f850` | Dibujar y entregar la imagen |

**Consecuencia para el diseño:** re-simular un cuadro sería llamar a los pasos 1 y 2 con la entrada de ese cuadro (el paso 1 vuelve a leer el mando, y `npad_state` le daría la entrada guardada), sin el 4. El envoltorio tendría que vigilar solo la llamada exterior de `f_71037246a0` (la que hace el bucle principal), porque la función se llama a sí misma. Falta saber qué hace el paso 3.

### Segundo perfil, con el árbol de llamadas (F6, 10 s)

El bucle principal (`f_7103746e90`, 91 % del hilo principal) llama cada cuadro a tres ramas:

| Rama (hijo directo del bucle principal) | Tiempo del hilo | Qué parece |
| :--- | :--- | :--- |
| `f_710386f850` → cadena del SDK (`f_7109429fe0` … `f_710901e7d0`) → vuelve al juego (`f_71037fdc50`, funciones `f_710392…`) | **66 %** | **El dibujo**: recorre la escena y llama al controlador gráfico (NVN, en el SDK) |
| **`f_71037246a0`** → `f_71022defb0` / `f_7101850d80` → `f_7101850f60` → `f_710135f570` → `f_710135ca00` → `f_710137b3f0` → muchas funciones pequeñas | **14 %** | **La simulación**: una cadena de un solo hijo que acaba repartiéndose en muchas funciones cortas, como un bucle "actualizar cada objeto" |
| `f_71038fe2b0` → `f_71038fe460` → `f_71038ff0f0` | 5 % | Sin identificar |

Además, ~9 % del hilo principal corre dentro de fibras (`f_710354a390` → `f_71035c3bf0` → `f_7103601290` → `f_7103818540`), y los hilos trabajadores 3 y 4 ejecutan **las mismas funciones** (`f_7103818540`, `f_71035c3bf0`, `f_7103600b50`). Eso indica que parte de la simulación se reparte entre hilos (un sistema de tareas): **riesgo de determinismo** a vigilar.

**Candidata a envolver: `f_71037246a0`**, llamada directamente por el bucle principal. En este portátil la simulación (esa rama + las fibras) sería ~23 % del hilo principal, unos 3–4 ms por cuadro, con el hilo ya ocupado al 90 %: aquí no hay sitio para re-simular. En un PC 2–3 veces más rápido, quizá 2–3 cuadros.

Para confirmarlo, la sonda versión 4 guarda la pila de llamadas en una lectura del mando y en una entrega de cuadro: si la lectura del mando pasa por `f_71037246a0`, es la simulación.

### Primer perfil del hilo principal en combate (F6, 10 s)

- **El hilo principal está ocupado el 90 % del tiempo** en este portátil (i5-1335U); también lo estaba midiendo el perfilador. Queda poco margen para re-simular en este PC.
- **~59 % de su tiempo** cuelga de `f_710901e7d0`, en la zona del SDK (direcciones `0x7109…`, donde también están las funciones de fibras): casi seguro **la preparación del dibujo** (el driver gráfico NVN del SDK).
- **~15 %** cuelga de una cadena del propio juego en direcciones bajas (`f_710135ca00`, `f_710137b3f0`, `f_7101850d80`, `f_7101850f60`, `f_710135f570`, `f_71022defb0`): **candidata a la simulación del combate**.
- **~12 %** corre dentro de fibras (`DispatchFiberContext` → `f_710354a390` → …): probablemente los guiones de los luchadores, que también son simulación.
- Por encima de todo: `main` → `f_710929d9e0` → `f_71002c5620` → `f_7103746e90` (85 %), probablemente el bucle principal.
- Si la simulación es ~15–30 % del hilo principal, cuesta **~2–4 ms por cuadro** en este portátil: re-simular 3 cuadros costaría 6–12 ms más, demasiado aquí; 1–2 cuadros, quizá.
- El informe solo daba los 3 llamadores de cada función: desde esta rama el perfilador añade **"qué llama cada función"** (el árbol de llamadas, un nivel cada vez) y guarda pilas de 40 niveles. Con eso se puede encontrar la función que llama a la simulación y al dibujo, la que hay que envolver.

### Tercera medición (sonda versión 3, combate 1 contra 1 contra la CPU, Windows 11, Antigravity cerrado)

| Dato | Resultado | Lectura |
| :--- | :--- | :--- |
| Páginas escritas por cuadro en combate, memoria **propia de la CPU** | mediana ~2 600 (≈ 10,5 MB), p95 ~3 200 (≈ 12,5 MB) | El 75 % de lo escrito no es memoria entregada a la GPU. Aun así incluye datos de dibujo que el juego prepara en su propia memoria (listas de dibujo, matrices) antes de copiarlos: el estado del combate es una parte, aún sin separar |
| Páginas escritas en memoria entregada a la GPU (nvmap) | mediana ~900 (≈ 3,6 MB) | Esto no hay que guardarlo |
| Coste de encontrarlas | 9–15 ms por cuadro; en la mitad de los cuadros la pregunta esperó al renderer | La sonda sigue siendo cara: pedir a Windows qué cambió en varios GB no sirve para el rollback real ([ROLLBACK-IDEAS.md](ROLLBACK-IDEAS.md): copiar un "conjunto caliente" fijo de páginas en lugar de preguntar cada cuadro) |
| Hilo 26 (prioridad 36) | Trabaja el 97 % de los vsync; en 24 de 25 muestras, **exactamente en la misma instrucción** (`lua2cpp_item+0xe7c70`), llamado desde `main+0x1bd7c0` ← `main+0x1be910` ← `main+0x1f7360` ← `main+0x1f7710` ← `main+0x1b2510` (arranque de hilo) | Siempre en el mismo punto: casi seguro **una espera activa** (un hilo trabajador que comprueba en bucle si hay trabajo). Mientras espera no toca el combate. La dirección cae en el módulo `lua2cpp_item`, pero la función puede ser compartida |
| Hilos 29 y 43 (prioridad 44) | Solo en la primera tanda (carga), dentro de funciones del SDK | Hilos de carga: no afectan al combate |
| Hilo 1 (prioridad 44, núcleo 0: el principal) | Trabajando 54–78 %, en una petición al sistema 21–42 % (probablemente pedir o entregar un búfer de imagen) | El hilo principal no espera al vsync: trabaja casi todo el tiempo y se frena al pedir búfer. Es el que hay que perfilar para encontrar la función que simula un cuadro |
| Mando, fibras, trabajo por cuadro | Igual: 1 lectura por cuadro desde 1 hilo; 13–20 fibras vivas; ~3–6 ms de trabajo por cuadro | Sin cambios |

### Segunda medición (sonda versión 2, combate 1 contra 1 contra la CPU, Windows 11)

| Dato | Resultado | Lectura |
| :--- | :--- | :--- |
| Páginas escritas por cuadro en ALIAS, en combate | mediana ~3 500 (≈ 14 MB), p95 ~4 400 (≈ 18 MB), máx. ~4 800; en menús ~1 500–2 600; picos de 24 000–35 000 (≈ 95–136 MB) en las transiciones (cargas) | Zona "cara" (1 000–10 000). Incluye lo que la CPU escribe para la GPU (vértices, constantes, texturas), que el rollback no tendría que guardar: falta separar esa parte del estado de la simulación |
| Dónde | Las 8 zonas de 1 MB más escritas suman solo ~600 páginas por cuadro; el resto está repartido por muchas zonas | El estado no está concentrado en un sitio pequeño, o lo está pero mezclado con datos para la GPU |
| Heap y pila | Heap 0; pila ~16–40 páginas por cuadro | Confirmado: la memoria del juego es ALIAS |
| Coste de encontrarlas | 6–14 ms por cuadro (picos de 25 ms), y en combate la mitad de los cuadros la pregunta esperó al renderer | Preguntar por toda la memoria del juego cada cuadro (varios GB) es **demasiado caro** para un rollback real: hará falta vigilar solo las zonas de la simulación, cuando se conozcan |
| Hilos en el vsync | 2–4 trabajando de ~45; "ninguno trabajando" en 0–2 % de los cuadros. El hilo 26 (prioridad 36) trabaja el 97 % de las veces; el hilo 1 (prioridad 44, núcleo 0) trabaja ~70 % y está en una petición al sistema ~25 % | No hay momento quieto. Hay que averiguar qué hacen los hilos 26 y 1 (perfilador F6 / `SSBU_HITCH`) y si tocan la memoria del combate |
| Mando, fibras, trabajo por cuadro | Igual que en la primera medición: 1 lectura por cuadro desde 1 hilo; 13–20 fibras vivas; ~5–7 ms de trabajo por cuadro | Sin cambios: siguen siendo buenas noticias |

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
