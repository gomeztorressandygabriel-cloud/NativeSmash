# VS en red con poco retraso (experimental)

> **Estado: experimental, sin probar todavía con el juego real.** Es el "paso A" del plan del rollback
> ([ROLLBACK.md](ROLLBACK.md)): jugar el VS local con un amigo por internet, con muy poco retraso, sin
> el online de Nintendo. El rollback vendrá encima de esto más adelante.

## Qué es

Cada uno ejecuta su propio juego. El que crea la partida (**host**) es el jugador 1 y el que se une
(**join**) es el jugador 2. Los dos ven **el mismo VS local**: los mismos menús y el mismo combate.
Cada cuadro, cada juego manda los botones de su jugador al otro, y los dos juegos avanzan juntos, cuadro
a cuadro. Los botones se aplican unos pocos cuadros más tarde en los dos PCs (2 por defecto, unos 33 ms),
así hay tiempo de que lleguen por internet. Si llegan tarde, el juego espera un instante (se nota como un
tirón).

Para que funcione, los dos juegos tienen que calcular exactamente lo mismo con los mismos botones. Eso es
justo lo que todavía no está demostrado con el juego real. Por eso cada 60 cuadros los dos juegos
comparan lo que calculó la simulación y lo escriben en `build\play.log`. Si se separan ("desync"), cada
uno verá el combate de forma distinta: los personajes en sitios distintos.

## Cómo probarlo con un amigo

**Los dos necesitan:**
1. La **misma versión** del juego y **NativeSmash al día**: en el launcher, página **Juego** → **Buscar
   actualizaciones** → **Actualizar ahora**.
2. **El mismo savegame.** El juego lo lee al empezar (luchadores desbloqueados, últimas reglas, posición del
   cursor, música...): con savegames distintos, los menús ya empiezan distintos y los juegos se separan. Desde
   el launcher, en la sección del VS en red:
   - el **host** pulsa **Crear el savegame para el netplay**: deja `savegame-netplay.zip` en su Escritorio y se
     lo pasa a su amigo **en privado** (Discord, WhatsApp...), nunca por GitHub;
   - el amigo pulsa **Usar el savegame del host (ZIP)** y lo elige.
   Las partidas en red empiezan siempre desde una copia nueva de ese savegame (en `build\netplay_data`), en los
   dos PCs. El savegame propio de cada uno **no se toca**. Si el host juega mucho y su savegame cambia, que lo
   vuelva a crear y a pasar.
3. Internet, sin nada más: **no hace falta Radmin, ZeroTier ni abrir puertos.** Los dos juegos se encuentran en el
   servidor de salas del online (el de `online_server.txt`; el público si no hay otro) con un **código de sala**. El
   servidor los presenta y los dos juegos hablan **directo** de un PC al otro si sus routers lo permiten; si no, a
   través del servidor (con algo más de retraso).

**Pasos (desde el launcher, `Play SSBU.bat`):**
1. El **host** va a la página **En línea**, baja hasta **VS en red con un amigo (prueba)** y elige **Crear sala
   (jugador 1)**. Sale **el código de su sala** (6 letras y números): **Copiar el código** y se lo pasa a su amigo.
   (**Otro código** hace uno nuevo.)
2. El amigo, en la misma página, elige **Unirme a una sala (jugador 2)** y escribe ese código.
3. El host elige el retraso (2 para empezar; manda el del host) y los dos pulsan **JUGAR**. El launcher recuerda la elección para la
   próxima vez (en `vs_netplay.txt`). Para jugar normal otra vez, elegid **Apagado**.

Sigue funcionando la forma anterior, por dirección: si el amigo escribe una dirección (por ejemplo `26.12.34.56`, la de
Radmin) en vez de un código, su juego va directo a ella (entonces el host necesita la VPN o el puerto UDP 7810 abierto).

También se puede sin el launcher: `launchers\Host VS Netplay (test).bat` y `launchers\Join VS Netplay (test).bat`.

**Después:**
1. **No hace falta abrirlos a la vez**: cada juego espera al otro en su primera pantalla todo lo que haga falta (con el
   código de sala; por dirección, hasta 5 minutos), así que el que carga antes (por ejemplo, el que no tiene que
   precalentar shaders) simplemente espera. Los dos tienen que empezar desde cero, porque la sincronización empieza al
   arrancar. En `build\play.log`: `in the lobby as ...`, luego `room ... made in the lobby: waiting for player 2` (host) o
   `joined room ... in the lobby` (amigo), y `player 2 joined` / `joined: player 2`.
2. Id juntos a **Smash (VS)**. El host es el jugador 1 y el amigo el jugador 2, cada uno con su propio mando.
   Usad solo mando o teclado normal (como "Pro Controller"). Un mando de GameCube funciona, pero sin la
   presión analógica de los gatillos.
3. Si va a tirones, subid el retraso (3 o 4). Si va bien, probad a bajarlo (1).

**Qué mirar y mandarme:** las líneas de `build\play.log` que empiecen por `[netplay]`. Las importantes
dicen `frame N: pages the simulation wrote: same X, different Y`. Si `different` se queda en unos pocos
y no crece, es ruido normal: hay páginas que el sonido o los efectos escriben a su ritmo. Si crece
mucho, o el combate se ve distinto en cada pantalla, es un desync, y el número de cuadro donde empieza
es la pista para arreglarlo.

## Novedades de la 0.1.14 (probadas solo a medias)

- **Hasta 4 jugadores** (el host los elige en el launcher; todos entran con el mismo código de sala).
- **El savegame del host viaja solo** a los demás antes de empezar; los savegames propios no se tocan.
- **Estado en pantalla** (y en el título de la ventana): esperando, conectado, jugando, con el ping y el retraso.
- **Retraso "Auto"**: sube enseguida si la red lo pide y baja despacio. Se puede fijar un número como antes.
- **La intro se salta sola** en el host, y si un PC va justo el otro lo espera un poco en vez de pararse de golpe.
- **Aviso de desincronización** en pantalla cuando las páginas del juego dejan de coincidir de forma sostenida.
- Si el otro deja de responder 60 s (antes 15), el netplay se apaga.

**Qué se ha comprobado y qué no:** el código de sala, la entrada de los jugadores y el envío del savegame
funcionan entre dos juegos en un PC. La partida larga con retraso Auto y varios jugadores NO se ha podido
medir en un solo PC de 8 GB (dos juegos a la vez se quedan sin memoria y se congelan minutos), así que la
prueba real son dos PC distintos. Si algo falla, el registro (`[netplay]`) dice dónde.

## Esperar a los shaders (0.1.16)

Nadie empieza la partida hasta que **todos** los juegos han terminado su pantalla de carga de shaders, tarde lo que tarde. Cada juego avisa de que está listo dentro de su paquete de "sigo vivo" (cada 250 ms, desde un hilo propio, así que un juego cargando sigue contestando). El host manda la salida solo cuando todos estan listos, y el titulo de la ventana dice quien falta. Protocolo 4: todos deben tener la misma version.

## Lo que todavía no hace (o puede fallar)

- **Puede desincronizarse.** Es lo primero que hay que medir. Hay una prueba en un solo PC que lo mide
  (abajo), y el laboratorio la va a hacer.
- No tiene botones dentro del juego: se elige en el launcher.
- Todavía no sale el mensaje de desconexión del propio juego.

## La prueba en un solo PC (para el laboratorio)

Juega dos veces seguidas lo mismo, con los mismos botones cuadro a cuadro, y compara. Si las dos partidas
salen iguales, dos PCs también saldrán iguales.

- `SSBU_LOCKSTEP=record`: graba los botones de cada cuadro y lo que calculó la simulación en
  `SSBU_LOCKSTEP_FILE` (`lockstep.bin` por defecto; se queda en el PC, no se sube). Termina tras
  `SSBU_LOCKSTEP_FRAMES` cuadros (15000).
- `SSBU_LOCKSTEP=replay`: otra partida con los botones grabados, cuadro a cuadro. Compara y escribe líneas
  `[lockstep]`: los primeros cuadros, uno de cada 300, el primero en el que difieren y, al final, un
  resumen con las páginas que más difieren.

En las dos partidas el reloj del hilo principal es un reloj de cuadros, igual que en el netplay.
Con `SSBU_CAPTURE=<carpeta>`, cada `SSBU_LOCKSTEP_PICTURES` cuadros (600 por defecto) guarda una foto
`lockstep_<cuadro>.bmp`: las fotos del mismo cuadro de las dos partidas se pueden comparar directamente. Es la
comprobación más clara, porque la comparación por páginas incluye páginas que otros hilos escriben a su ritmo.

**Resultado del pedido 012:** compiló bien, una partida normal fue perfecta (240 s a 60 fps, sin errores), y la
grabación y la repetición pasaron por los mismos menús y llegaron al mismo combate (Bayonetta contra Corrin en
Pueblo Smash) sin fallos, en más de 8 400 cuadros. La comparación por páginas salió con mucho ruido desde el
cuadro 2 (la "simulación" también corre en los menús, y en ella escriben otros hilos), así que no sirve para
decidir. Por eso se añadieron las fotos por cuadro.

## Detalles técnicos

`runtime/net_lockstep.c` contiene todo. Las variables de entorno son `SSBU_NETPLAY` (`host`/`join`),
`SSBU_NETPLAY_PEER`, `SSBU_NETPLAY_PORT`, `SSBU_NETPLAY_DELAY`, `SSBU_NETPLAY_WAIT`, `SSBU_LOCKSTEP`
(`record`/`replay`), `SSBU_LOCKSTEP_FILE` y `SSBU_LOCKSTEP_FRAMES`. El protocolo va por UDP: `HELLO` y
`WELCOME` (el retraso y el reloj son los del host), `INPUT` (los últimos 8 cuadros de botones en cada
paquete, para que perder uno no cueste nada), `HASH` (la lista de páginas de un cuadro, a trozos) y `BYE`.
