# Online tuning / Afinado en línea

[← README](../README.md) · [Multiplayer](MULTIPLAYER.md) · [Playing](PLAYING.md) · [Technical guide](TECHNICAL.md)

**Español** · [English](#english)

---

## Español

Qué hacer cuando una partida en línea se siente lenta o se traba. Todo lo de aquí vale para los cuartos de **Local Wireless** (el lobby público, un código de lobby, una sala privada). Para el modo **Online** (Quickplay) el ajuste del retraso de entrada no está verificado.

> [!NOTE]
> **Sin cifras.** Esta guía dice qué hace cada ajuste y cómo probarlo, pero no promete números: no se han medido retrasos ni tasas de cuadros en partidas reales entre dos PC, y cada conexión es distinta. Lo que sirve es probar con tu rival y ajustar.

### Contenido

- [El retraso de entrada](#el-retraso-de-entrada)
- [Cómo cambiarlo](#cómo-cambiarlo)
- [Cómo elegir el número](#cómo-elegir-el-número)
- [Leer F1 y F2](#leer-f1-y-f2)
- [Ajustes de display.ini](#ajustes-de-displayini)
- [Consejos de red](#consejos-de-red)
- [Los dos PC deben sostener 60 cuadros](#los-dos-pc-deben-sostener-60-cuadros)
- [Si algo va mal: netdump y play.log](#si-algo-va-mal-netdump-y-playlog)

### El retraso de entrada

En línea, el juego no aplica tu botón en el mismo cuadro en que lo pulsas: lo guarda unos cuadros (el **retraso de entrada**) para que las entradas de los otros jugadores lleguen a tiempo. A 60 cuadros por segundo, cada cuadro son unos 16,7 ms.

- **Poco retraso:** el juego responde antes, pero si el número es demasiado bajo para tu conexión, el juego **espera** la entrada del otro jugador y se **traba**.
- **Más retraso:** más fluido, pero los botones tardan más en notarse.
- **Cada jugador elige el suyo.** El ajuste es local: no hace falta que los dos pongan el mismo número.

Sin ningún ajuste, NativeSmash usa **3 cuadros**. Además, dentro de una sala de Local Wireless el runtime deja **un solo cuadro esperando** a la pantalla (fuera de la sala, lo que diga `display.ini`): medido en una partida real, del cuadro hecho a la pantalla pasaban 28 ms, y así unos 11. `auto` deja el número que el propio juego calcula a partir de la latencia que mide en la sala.

### Cómo cambiarlo

Hay dos sitios. Los dos escriben el mismo archivo, **`online_delay.txt`**, junto a `Play SSBU.bat`:

| Dónde | Cómo |
| :--- | :--- |
| **El launcher** (Windows) | La ventana de `Play SSBU.bat`, página **En línea**: **Auto** o de **0 a 8**. |
| **Dentro del juego** | <kbd>Esc</kbd> → página **Online** → **Input delay (frames)**. Cambia al instante; el juego lo usa en la siguiente partida. (Esta fila existe en Windows y Linux; la versión de macOS no tiene el menú, así que ahí edita el archivo.) |
| **A mano** | Un archivo de texto `online_delay.txt` con una sola línea: un número de **0 a 25**, o `auto`. Sin archivo, son 3. |

Un número por encima de 25 se queda en 25. El menú y el launcher ofrecen de 0 a 8 (0 solo sirve con una conexión muy rápida, en la misma casa o ciudad: si no, el juego se traba esperando); un número más grande que escribas en el archivo se respeta y se muestra tal cual.

### Cómo elegir el número

1. Empieza con el valor por omisión (3).
2. Si el juego se traba o se detiene unos instantes, **súbelo** de uno en uno.
3. Si todo va fluido y quieres que responda antes, **bájalo** de uno en uno hasta que empiece a trabarse, y vuelve al número anterior.
4. Prueba con la persona con la que vas a jugar y en la misma conexión: un número bueno con un rival puede ser bajo con otro.

El valor que el juego habría elegido y el que se usó quedan en `build\play.log`, en una línea como `[net] input delay: the game chose N frames, …`. Sirve para comparar tu número con el del juego.

### Leer F1 y F2

<kbd>F1</kbd> muestra u oculta el panel; <kbd>F2</kbd> añade los detalles. Ambos incluyen las líneas de red:

```text
Online: 37 players, 4 rooms - you are Alpha, ping 48 ms
Room: Bravo direct 12 ms, Kim relayed
```

- **`ping 48 ms`** es el tiempo de ida y vuelta de **tu PC al lobby**, no a tu rival.
- **`direct 12 ms`** es el tiempo de ida y vuelta **directo a ese jugador**. Es la cifra que cuenta para la partida.
- **`relayed`** significa que el tráfico de ese jugador pasa por el lobby. Entonces el retraso entre ustedes es, aproximadamente, la suma de los pings de los dos al lobby. Mira la tabla de [conexiones directas](MULTIPLAYER.md#direct-connections-and-your-internet-address) para ver qué significa cada texto (`trying direct`, `direct off`…).

En el modo Online (Quickplay) todo el tráfico de la partida pasa por el lobby; las conexiones directas son, por ahora, solo para los cuartos de Local Wireless.

### Ajustes de display.ini

Estos ajustes de [`display.ini`](PLAYING.md#window-and-display) no cambian la red: bajan el retraso **local** entre tu botón y la imagen.

| Ajuste | Para qué |
| :--- | :--- |
| `latency = 1` | Cuántos cuadros puede adelantarse la tarjeta gráfica (1 a 3; 3 por omisión). Menos es más reactivo. |
| `vsync = off` | Quita un cuadro de retraso de imagen (16,7 ms), a cambio de que pueda verse una línea de corte. |
| `pacing` | `clock` (60 fijos), `vrr` (monitor G-Sync/FreeSync, mejor en `borderless`) o `display` (sigue el monitor). |

`latency` y `vsync` cuestan algo de fluidez en un PC justo; cámbialos de uno en uno y comprueba que el juego sigue a 60.

### Consejos de red

- **Ethernet** si puedes. Si no, Wi-Fi de **5 GHz**, cerca del router.
- **Sin descargas ni vídeo** en tu red mientras juegas (actualizaciones, nube, otras personas).
- **Desactiva el ahorro de energía del adaptador Wi-Fi** (Administrador de dispositivos → tu adaptador → Administración de energía).
- **Firewall:** permite `ssbu.exe` (Windows pregunta la primera vez; pulsa **Permitir**).
- **Conexión directa o lobby:** con `direct N ms` el retraso es el de los dos PC entre sí; con `relayed` suma los dos pings al lobby. Si ves `relayed`, prueba un retraso mayor o revisa [por qué no hay conexión directa](MULTIPLAYER.md#direct-connections-and-your-internet-address). Desactivar las conexiones directas (por privacidad) te deja siempre en `relayed`.

### Los dos PC deben sostener 60 cuadros

El juego avanza un cuadro solo cuando tiene las entradas de todos para él, así que **el PC más lento marca el ritmo de todos**. Si el tuyo baja de 60 cuadros por segundo, tu rival lo nota como tirones aunque su PC vaya sobrado, y subir el retraso no lo arregla. Antes de culpar a la red, comprueba el contador de cuadros (<kbd>F1</kbd>) en ambos PC; la guía [Low frame rate](PLAYING.md#low-frame-rate) y el ajuste `monitor` ayudan a encontrar qué lo frena.

### Si algo va mal: netdump y play.log

**Siempre manda `build\play.log`** (incluye la línea `[net] input delay: …`). Léelo antes: puede contener nombres de jugador, carpetas y direcciones de internet.

Para un análisis más fino, el juego puede volcar cada paquete UDP de la partida:

1. Con el juego cerrado, crea un archivo vacío llamado **`netdump.on`** junto a `Play SSBU.bat` (o inicia con la variable `SSBU_NETDUMP=1`).
2. Juega la partida que falla.
3. Se escribe **`netdump.txt`** en la misma carpeta: la hora en ms, el número de cuadros presentados, `tx`/`rx`, el tamaño, el otro jugador y los primeros 96 bytes.
4. **Borra `netdump.on` cuando termines.** Sin límite de tamaño, el archivo crece mientras juegas; además el volcado ralentiza algo el juego.

Detalles: se vuelve a crear vacío en cada inicio; con dos juegos en el mismo PC escriben en el mismo archivo y se pisan; solo recoge los paquetes UDP del juego (no las conexiones TCP, como las del modo Online hacia el servidor del juego); y contiene las direcciones de internet de los otros jugadores y parte de lo que se envían, así que no lo publiques sin leerlo.

---

## English

What to do when an online match feels slow or stutters. Everything here applies to **Local Wireless** rooms (the public lobby, a lobby code, a private room). Whether the input-delay setting also applies to **Online** mode (Quickplay) hasn't been verified.

> [!NOTE]
> **No numbers promised.** This guide says what each setting does and how to try it, but no delay or frame-rate figures have been measured in real matches between two PCs, and every connection differs. What works is testing with your opponent and adjusting.

### Contents

- [Input delay](#input-delay)
- [How to change it](#how-to-change-it)
- [Choosing the number](#choosing-the-number)
- [Reading F1 and F2](#reading-f1-and-f2)
- [display.ini settings](#displayini-settings)
- [Network tips](#network-tips)
- [Both PCs must hold 60 frames](#both-pcs-must-hold-60-frames)
- [When something goes wrong: netdump and play.log](#when-something-goes-wrong-netdump-and-playlog)

### Input delay

Online, the game doesn't apply your button the frame you press it: it holds it for a few frames (the **input delay**) so the other players' inputs for that frame arrive in time. At 60 frames a second, a frame is about 16.7 ms.

- **A low number:** the game responds sooner, but if the number is too low for your connection the game **waits** for the other player's input and **stutters**.
- **A higher number:** smoother, but buttons take longer to show.
- **Each player picks their own.** The setting is local: the two players don't need the same number.

With no setting, NativeSmash uses **3 frames**. Also, inside a Local Wireless room the runtime keeps **a single frame waiting** for the screen (outside the room, whatever `display.ini` says): measured in a real match, 28 ms passed from a frame made to the screen, about 11 this way. `auto` keeps the number the game works out itself from the latency it measures in the room.

### How to change it

Two places. Both write the same file, **`online_delay.txt`**, next to `Play SSBU.bat`:

| Where | How |
| :--- | :--- |
| **The launcher** (Windows) | The `Play SSBU.bat` window, **Online** page: **Auto** or **0 to 8**. |
| **In the game** | <kbd>Esc</kbd> → **Online** page → **Input delay (frames)**. It changes at once; the game uses it from the next match. (This row exists on Windows and Linux; the macOS version has no such menu, so edit the file there.) |
| **By hand** | A text file `online_delay.txt` with one line: a number from **0 to 25**, or `auto`. Without the file it is 3. |

A number above 25 stays at 25. The menu and the launcher offer 0 to 8 (0 only works on a very fast connection, same house or city: otherwise the game stutters waiting); a larger number you type into the file is kept and shown as it is.

### Choosing the number

1. Start with the default (3).
2. If the game stutters or freezes for moments, **raise it** one step at a time.
3. If it's smooth and you want it more responsive, **lower it** one step at a time until it starts to stutter, then go back one.
4. Test with the person you'll play with, on the same connection: a good number against one opponent can be too low against another.

What the game would have chosen and what was used go to `build\play.log`, in a line like `[net] input delay: the game chose N frames, …`. Use it to compare your number with the game's.

### Reading F1 and F2

<kbd>F1</kbd> shows or hides the panel; <kbd>F2</kbd> adds details. Both include the network lines:

```text
Online: 37 players, 4 rooms - you are Alpha, ping 48 ms
Room: Bravo direct 12 ms, Kim relayed
```

- **`ping 48 ms`** is the round trip from **your PC to the lobby**, not to your opponent.
- **`direct 12 ms`** is the round trip **straight to that player**. This is the figure that matters for the match.
- **`relayed`** means that player's traffic goes through the lobby, so the delay between you is roughly both players' pings to the lobby added together. The table under [direct connections](MULTIPLAYER.md#direct-connections-and-your-internet-address) explains each text (`trying direct`, `direct off`…).

In Online mode (Quickplay) all the match's traffic goes through the lobby; direct connections are only for Local Wireless rooms so far.

### display.ini settings

These [`display.ini`](PLAYING.md#window-and-display) settings don't change the network: they lower the **local** delay between your button and the picture.

| Setting | What for |
| :--- | :--- |
| `latency = 1` | How many frames the graphics card may work ahead (1 to 3; 3 by default). Lower is more responsive. |
| `vsync = off` | Removes one frame of picture delay (16.7 ms), at the price of a possible tear line. |
| `pacing` | `clock` (a steady 60), `vrr` (a G-Sync/FreeSync monitor, best with `borderless`) or `display` (follows the monitor). |

`latency` and `vsync` can cost smoothness on a PC with no spare power; change them one at a time and check the game still holds 60.

### Network tips

- **Ethernet** if you can. Otherwise **5 GHz** Wi-Fi, close to the router.
- **No downloads or video** on your network while you play (updates, cloud sync, other people).
- **Turn off the Wi-Fi adapter's power saving** (Device Manager → your adapter → Power Management).
- **Firewall:** allow `ssbu.exe` (Windows asks the first time; click **Allow**).
- **Direct or lobby:** with `direct N ms` the delay is that between the two PCs; with `relayed` it adds both pings to the lobby. If you see `relayed`, try a higher delay or check [why there's no direct connection](MULTIPLAYER.md#direct-connections-and-your-internet-address). Turning direct connections off (for privacy) keeps you on `relayed`.

### Both PCs must hold 60 frames

The game moves on a frame only when it has everyone's input for it, so **the slowest PC sets the pace for everyone**. If yours drops under 60 frames a second, your opponent sees stutter even if their PC has power to spare, and raising the delay doesn't fix it. Before blaming the network, check the frame counter (<kbd>F1</kbd>) on both PCs; the [Low frame rate](PLAYING.md#low-frame-rate) section and the `monitor` setting help find what slows it down.

### When something goes wrong: netdump and play.log

**Always send `build\play.log`** (it includes the `[net] input delay: …` line). Read it first: it can hold player names, folder names and internet addresses.

For a finer look, the game can dump every UDP packet of the match:

1. With the game closed, create an empty file named **`netdump.on`** next to `Play SSBU.bat` (or start with the variable `SSBU_NETDUMP=1`).
2. Play the match that goes wrong.
3. **`netdump.txt`** is written in the same folder: the time in ms, the number of frames presented, `tx`/`rx`, the size, the other player and the first 96 bytes.
4. **Delete `netdump.on` when you're done.** The file has no size limit and grows while you play; the dump also slows the game a little.

Details: it starts empty at each launch; two games on one PC write to the same file and overwrite each other; it records the game's UDP packets only (not TCP connections, such as Online mode's to the game server); and it holds the other players' internet addresses and part of what they send, so read it before sharing.
