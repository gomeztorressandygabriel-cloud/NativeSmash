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
