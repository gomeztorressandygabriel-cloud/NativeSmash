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
1. La **misma versión** del juego y NativeSmash **compilado desde la misma rama** (`cloud/rollback-probe`),
   con `launchers\Build NativeSmash.bat`. No uses `Install NativeSmash.bat`: se actualiza solo y deshace
   estos cambios.
2. **Los mismos luchadores desbloqueados**. Lo más fácil es que los dos usen `Unlock All Fighters.bat`
   antes. Si las pantallas de selección no son iguales, los juegos se separan desde el menú.
3. Estar en la misma red: una **VPN** como ZeroTier, Tailscale o Radmin VPN (lo más fácil), o que el host
   abra el **puerto UDP 7810** en su router.

**Pasos:**
1. El **host** abre `launchers\Host VS Netplay (test).bat`. Elige el puerto (Enter: 7810) y el retraso
   (Enter: 2). Le pasa a su amigo la dirección que sale en pantalla (la de la VPN, si usáis una).
2. El amigo abre `launchers\Join VS Netplay (test).bat` y escribe esa dirección.
3. **Abrid los dos juegos más o menos a la vez**: cada uno espera al otro en su primera pantalla (hasta 5
   minutos). Los dos tienen que empezar desde cero, porque la sincronización empieza al arrancar.
4. Id juntos a **Smash (VS)**. El host es el jugador 1 y el amigo el jugador 2, cada uno con su propio mando.
   Usad solo mando o teclado normal (como "Pro Controller"). Un mando de GameCube funciona, pero sin la
   presión analógica de los gatillos.
5. Si va a tirones, subid el retraso (3 o 4). Si va bien, probad a bajarlo (1).

**Qué mirar y mandarme:** las líneas de `build\play.log` que empiecen por `[netplay]`. Las importantes
dicen `frame N: pages the simulation wrote: same X, different Y`. Si `different` se queda en unos pocos
y no crece, es ruido normal: hay páginas que el sonido o los efectos escriben a su ritmo. Si crece
mucho, o el combate se ve distinto en cada pantalla, es un desync, y el número de cuadro donde empieza
es la pista para arreglarlo.

## Lo que todavía no hace (o puede fallar)

- **Puede desincronizarse.** Es lo primero que hay que medir. Hay una prueba en un solo PC que lo mide
  (abajo), y el laboratorio la va a hacer.
- No tiene botones en el juego (Host/Join con código). De momento se configura con los dos `.bat`.
- Si el otro deja de responder 15 s, el netplay se apaga y el jugador 2 se queda quieto. Todavía no sale
  el mensaje de desconexión del propio juego.
- Solo 1 contra 1.

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

## Detalles técnicos

`runtime/net_lockstep.c` contiene todo. Las variables de entorno son `SSBU_NETPLAY` (`host`/`join`),
`SSBU_NETPLAY_PEER`, `SSBU_NETPLAY_PORT`, `SSBU_NETPLAY_DELAY`, `SSBU_NETPLAY_WAIT`, `SSBU_LOCKSTEP`
(`record`/`replay`), `SSBU_LOCKSTEP_FILE` y `SSBU_LOCKSTEP_FRAMES`. El protocolo va por UDP: `HELLO` y
`WELCOME` (el retraso y el reloj son los del host), `INPUT` (los últimos 8 cuadros de botones en cada
paquete, para que perder uno no cueste nada), `HASH` (la lista de páginas de un cuadro, a trozos) y `BYE`.
