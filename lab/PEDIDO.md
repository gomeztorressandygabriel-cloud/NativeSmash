# Pedido 001: combate automático con la sonda

**Para qué:** comprobar que el laboratorio funciona sin que el usuario juegue (un combate que empieza solo), y repetir la medición de la
sonda versión 4 de forma automática. Si esto funciona, las próximas pruebas (la de "rebobinar") se harán igual.

**Código a usar:** la rama `cloud/rollback-probe` (en `C:\WindowSmash` es la rama local `rollback-probe`).

## Pasos

1. En `C:\WindowSmash`: `git status` (sin cambios locales en archivos con seguimiento; si los hay, para), luego
   `git pull mine cloud/rollback-probe` (debe ser fast-forward). Apunta el commit (`git rev-parse --short HEAD`).
2. Compila con `launchers\Build NativeSmash.bat`. Si falla, escribe el error completo en el resultado y para.
3. Mira cómo arranca `Play SSBU.bat` el juego (el ejecutable y sus argumentos) y compáralo con `tools/dev/autobattle.sh`, que arranca
   `./build/ssbu.exe dump/modules --status 30` con botones guionizados (`SSBU_INPUT`) que van del título a un combate 1 contra 1.
   Ejecútalo con Git Bash (o tradúcelo a PowerShell), con estas variables además de las del script:
   - `SSBU_DATA_ROOT` apuntando a una **copia** de la partida guardada, como hace `tools/dev/menurun.sh`
     (`build/testdata` con una copia de `save/`). **Nunca** la partida real.
   - `SSBU_RB_PROBE=1` (la sonda) y `SSBU_LATENCY=1`.
   - Duración: 150 segundos (el PC es lento; los tiempos del guion están pensados para uno más rápido).
4. **Comprueba que llegó al combate**: el script guarda capturas en la carpeta de salida (`SSBU_CAPTURE`). Conviértelas con
   `tools/dev/shot.py` si hace falta y mira las últimas: ¿se ve un combate (dos luchadores en un escenario)?
   - Si no llegó, mira en qué pantalla se quedó y ajusta los tiempos de `SSBU_INPUT` (por ejemplo, retrasando todo unos segundos o
     repitiendo pulsaciones de A). Como mucho **3 intentos**.
   - Las capturas **no** se suben a GitHub: solo describe lo que se ve.
5. Escribe `lab/resultados/001-autobattle.md` con:
   - el commit usado y si compiló;
   - el comando y el `SSBU_INPUT` que funcionaron (o los 3 intentos y dónde se quedó cada uno);
   - en qué segundo empezó el combate y cuánto duró;
   - de `<salida>.log`, **solo**: todas las líneas que empiezan por `[rbprobe]` y, tras cada `[rbprobe] stack at ...` y cada
     `[rbprobe] thread ... is in`, las líneas siguientes hasta la próxima que empiece por `[`; las líneas `[latency]` que contengan
     `vsync event -> game queues its frame`; las líneas `[status]`; y cualquier línea con `host exception`, `stopped during` o `fatal`.
6. Sube el resultado (procedimiento de `lab/README.md`) y avisa al usuario.
