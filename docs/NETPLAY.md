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
3. Estar en la misma red: una **VPN** como ZeroTier, Tailscale o Radmin VPN (lo más fácil), o que el host
   abra el **puerto UDP 7810** en su router.

**Pasos (desde el launcher, `Play SSBU.bat`):**
1. El **host** va a la página **En línea**, baja hasta **VS en red con un amigo (prueba)** y elige **Soy el host
   (jugador 1)**. Ahí mismo sale su dirección de Radmin VPN (la que empieza por 26.): se la pasa a su amigo.
2. El amigo, en la misma página, elige **Me uno (jugador 2)** y escribe esa dirección.
3. El host elige el retraso (2 para empezar; manda el del host) y los dos pulsan **JUGAR**. El launcher recuerda la elección para la
   próxima vez (en `vs_netplay.txt`). Para jugar normal otra vez, elegid **Apagado**.

También se puede sin el launcher: `launchers\Host VS Netplay (test).bat` y `launchers\Join VS Netplay (test).bat`.

**Después:**
1. **Abrid los dos juegos más o menos a la vez**: cada uno espera al otro en su primera pantalla (hasta 5
   minutos). Los dos tienen que empezar desde cero, porque la sincronización empieza al arrancar.
2. Id juntos a **Smash (VS)**. El host es el jugador 1 y el amigo el jugador 2, cada uno con su propio mando.
   Usad solo mando o teclado normal (como "Pro Controller"). Un mando de GameCube funciona, pero sin la
   presión analógica de los gatillos.
3. Si va a tirones, subid el retraso (3 o 4). Si va bien, probad a bajarlo (1).

**Qué mirar y mandarme:** las líneas de `build\play.log` que empiecen por `[netplay]`. Las importantes
dicen `frame N: pages the simulation wrote: same X, different Y`. Si `different` se queda en unos pocos
y no crece, es ruido normal: hay páginas que el sonido o los efectos escriben a su ritmo. Si crece
mucho, o el combate se ve distinto en cada pantalla, es un desync, y el número de cuadro donde empieza
es la pista para arreglarlo.

## Lo que todavía no hace (o puede fallar)

- **Puede desincronizarse.** Es lo primero que hay que medir. Hay una prueba en un solo PC que lo mide
  (abajo), y el laboratorio la va a hacer.
- No tiene botones dentro del juego ni código de sala: se elige en el launcher (o con los dos `.bat`) con la dirección del host.
- Si el otro deja de responder 15 s, el netplay se apaga y el jugador 2 se queda quieto. Todavía no sale
  el mensaje de desconexión del propio juego.
- Solo 1 contra 1.

## Para después de las pruebas reales (pedido por el usuario)

- **Si uno va más lento, el otro también.** Ya ocurre por cómo funciona: cada juego espera los botones del otro
  antes de avanzar. Falta que se note menos, con un ritmo común suave en vez de tirones.
- **Aviso en pantalla si se desincronizan** (ahora solo queda en `build\play.log`), y a ser posible arreglarlo solo:
  el host manda su estado y el otro se pone al día.
- **Mensaje de desconexión** como el del propio juego, en vez de que el jugador 2 se quede quieto.
- Botones Host/Join dentro del juego y código de sala, y más de 2 jugadores.

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

**Resultado del pedido 013 (fotos del mismo cuadro):** sin fallos en las dos partidas. En **el combate** las fotos
coinciden: en el cuadro 7 200 difiere el 5,13 % de los píxeles, en el 7 800 un 0,31 % (una franja pequeña) y en el
8 400 son **prácticamente idénticas**, con los mismos personajes en las mismas posiciones y los mismos porcentajes,
el luchador de la CPU incluido. Antes del combate (cuadros 600–6 600: menús, carga, presentación) las fotos sí
difieren mucho (8–65 %). La explicación más probable es que esas pantallas se animan al ritmo del dibujo y no al
de los cuadros, y que las cargas tardan distinto en cada partida. No es seguro, pero en el combate no queda
rastro de esas diferencias. **Lectura: con el mismo savegame y los mismos botones cuadro a cuadro, el combate sale
igual.** Falta comprobarlo entre dos PCs distintos. Las operaciones de coma flotante que podrían dar distinto
entre procesadores Intel y AMD (las estimaciones de recíproco y raíz) el runtime las calcula en software, bit a bit,
así que no deberían separar dos PCs.

## Detalles técnicos

`runtime/net_lockstep.c` contiene todo. Las variables de entorno son `SSBU_NETPLAY` (`host`/`join`),
`SSBU_NETPLAY_PEER`, `SSBU_NETPLAY_PORT`, `SSBU_NETPLAY_DELAY`, `SSBU_NETPLAY_WAIT`, `SSBU_LOCKSTEP`
(`record`/`replay`), `SSBU_LOCKSTEP_FILE` y `SSBU_LOCKSTEP_FRAMES`. El protocolo va por UDP: `HELLO` y
`WELCOME` (el retraso y el reloj son los del host), `INPUT` (los últimos 8 cuadros de botones en cada
paquete, para que perder uno no cueste nada), `HASH` (la lista de páginas de un cuadro, a trozos) y `BYE`.
