# Laboratorio del rollback (NativeSmash)

Esta rama (`cloud/rollback-lab`) es el buzón entre la sesión de Claude en la nube, que diseña y escribe el código del rollback,
y el agente que trabaja en el PC del usuario (Antigravity o Claude Code local), que compila, ejecuta el juego y mide.
**No contiene código del juego ni del runtime**: solo pedidos y resultados en texto.

- `lab/PEDIDO.md`: el pedido actual, numerado. Lo escribe Claude (nube).
- `lab/resultados/NNN-*.md`: la respuesta a cada pedido. La escribe el agente local.

## Reglas permanentes para el agente local

1. **No rompas el juego del usuario.** Trabaja en `C:\WindowSmash`. Antes de cambiar de rama o compilar, `git status`: si hay cambios
   locales en archivos con seguimiento, para y avisa.
2. **Nunca** fusiones nada en `nativesmash` ni en `main`; nunca `force-push`, `git reset --hard` ni `git clean`.
3. **Nunca subas** a GitHub archivos del juego, claves (`prod.keys`, `title.keys`), firmware, partidas guardadas, capturas de pantalla,
   ni las carpetas `dump/`, `build/`, `deps/`, `save/`. Del log, **solo** las líneas que pida el pedido (el log completo puede llevar nombres
   de jugador y direcciones de internet).
4. Compila solo con `launchers\Build NativeSmash.bat` (nunca `Install NativeSmash.bat`, que se actualiza solo).
5. Las pruebas automáticas usan **una copia de la partida guardada** (`SSBU_DATA_ROOT`, como `tools/dev/menurun.sh`): nunca la del usuario.
6. No cambies código salvo para arreglar un error de compilación; si lo haces, explícalo en el resultado (qué, dónde y por qué).
7. Si algo falla o no está claro, **no improvises**: escribe en el resultado qué pasó, con el error exacto, y para.

## Procedimiento cuando el usuario diga "haz el pedido del laboratorio"

1. `git fetch mine cloud/rollback-lab` (el remoto `mine` es `gomeztorressandygabriel-cloud/NativeSmash`).
2. Si no existe todavía, crea una copia de trabajo aparte para el buzón (no toca la carpeta del juego):
   `git worktree add C:\WindowSmash-lab mine/cloud/rollback-lab` (y `git -C C:\WindowSmash-lab switch -c rollback-lab --track mine/cloud/rollback-lab`).
   Si ya existe: `git -C C:\WindowSmash-lab pull`.
3. Lee `C:\WindowSmash-lab\lab\PEDIDO.md`. Si ya hay un `lab/resultados/` con su número, di que ese pedido ya está hecho y para.
4. Haz el pedido en `C:\WindowSmash`.
5. Escribe el resultado en `C:\WindowSmash-lab\lab\resultados\NNN-<nombre>.md` (NNN = número del pedido), haz commit
   (mensaje: `lab: resultado del pedido NNN`) y `git -C C:\WindowSmash-lab push mine HEAD:cloud/rollback-lab`.
6. Dile al usuario en una línea: "Pedido NNN terminado" (o "Pedido NNN falló: <motivo>").
