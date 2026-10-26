# Pedido 012: VS en red, paso A — compilar, comprobar que el juego normal sigue bien, y "jugar dos veces lo mismo"

**Para qué:** el proyecto pasa al "paso A": VS en red con poco retraso, sin rebobinar (`docs/NETPLAY.md`). Este pedido hace tres
cosas: (1) compilar la versión nueva, que es lo que falta para poder fusionarla en GitHub; (2) comprobar que una partida normal, sin
nada encendido, sigue igual; y (3) la prueba clave del paso A: jugar **dos veces seguidas** el mismo combate con **los mismos botones
cuadro a cuadro**, y comparar si las dos partidas calculan lo mismo. Si salen iguales, dos PCs también lo harán.

**Resultado del pedido 011:** muy útil, gracias. Explicado en `docs/ROLLBACK.md` ("Resultado del pedido 011").

**Aviso:** como siempre, si algo se cuelga o se cierra, apúntalo y no lo repitas. El archivo `build\lockstep.bin` que crea la prueba
**no se sube nunca** (se queda en el PC, dentro de `build\`).

## Pasos

1. En `C:\WindowSmash`: `git status` (sin cambios locales en archivos con seguimiento), `git pull mine cloud/rollback-probe`
   (fast-forward). El último commit debe ser `3658e1f` ("Rollback test: end the census safely ...") o posterior. Apunta el commit.
2. Compila con `launchers\Build NativeSmash.bat`. Apunta si compiló y cuánto tardó. Si falla, escribe el error completo y para (si
   menciona `net_lockstep.c`, `hle_hid.c` o `winsock`, cópialo tal cual).
3. **Partida normal (sin nada encendido):** el combate automático igual que en el pedido 011 (la copia de la partida guardada en
   `SSBU_DATA_ROOT`, el mismo `SSBU_INPUT`, la carpeta de capturas), pero **sin** `SSBU_RB_TEST`, **sin** `SSBU_RB_PROBE`, **sin**
   `SSBU_LOCKSTEP` y **sin** `SSBU_NETPLAY`; con `SSBU_LATENCY=1`. Duración: 240 segundos. ¿Llega al combate y se juega normal?
4. **Grabar:** el mismo combate automático, con `SSBU_LOCKSTEP=record`, `SSBU_LOCKSTEP_FILE=build\lockstep.bin`,
   `SSBU_LOCKSTEP_FRAMES=16000` y `SSBU_LATENCY=1` (sin las otras). Duración: 300 segundos. Carpeta de capturas nueva (p. ej.
   `build/ab12r`).
5. **Repetir:** otra vez, con `SSBU_LOCKSTEP=replay`, el mismo `SSBU_LOCKSTEP_FILE=build\lockstep.bin`, `SSBU_LATENCY=1`, y el
   mismo `SSBU_INPUT`. Duración: 300 segundos. Otra carpeta de capturas (p. ej. `build/ab12p`).
6. Compara las capturas de los pasos 4 y 5 **en los mismos momentos**: ¿los mismos menús, los mismos personajes, el mismo escenario?
   En el combate, ¿los personajes están en los mismos sitios y con los mismos porcentajes en capturas del mismo segundo? Descríbelo
   (sin subir imágenes).
7. Escribe `lab/resultados/012-paso-a.md` con:
   - el commit, si compiló y cuánto tardó;
   - paso 3: si llegó al combate y fue normal (y cualquier línea con `host exception`, `stopped during`, `fatal` o `[watchdog]`);
   - pasos 4 y 5: los comandos y variables usadas, y **todas** las líneas de cada `<salida>.log` que empiecen por `[lockstep]`;
   - de cada uno de los tres, las líneas `[latency] 5.0 s:` desde el segundo ~180;
   - cualquier línea con `host exception`, `stopped during`, `fatal`, `[watchdog]` o `guest backtrace`, y las 30 líneas siguientes;
   - lo que viste al comparar las capturas (paso 6).
8. Sube el resultado (procedimiento de `lab/README.md`) y avisa al usuario.
