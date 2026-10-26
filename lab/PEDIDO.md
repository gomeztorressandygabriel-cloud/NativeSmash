# Pedido 011: la prueba de rebobinar, versión 6 (censo de quién escribe, antes de guardar)

**Para qué:** en el pedido 010 la ronda 1 salió casi idéntica (solo 4 páginas de la simulación distintas), pero luego el juego se
cerró en otro hilo que va a su ritmo, porque la prueba le devolvió datos viejos. La versión 6 hace primero un "censo" de 10 cuadros
para saber qué hilos escriben cada página y deja fuera, desde la primera ronda, las de los hilos que van a su ritmo. **Durante esos 10
cuadros el juego puede ir a tirones un momento: es normal.** Explicado en `docs/ROLLBACK.md` ("Resultado del pedido 010").

**Resultado del pedido 010:** muy útil, gracias.

**Aviso:** igual que antes, puede colgarse o cerrarse; si pasa, apúntalo y no insistas: no lo repitas otra vez.

## Pasos

1. En `C:\WindowSmash`: `git status` (sin cambios locales en archivos con seguimiento), `git pull mine cloud/rollback-probe`
   (fast-forward). El último commit debe ser `9f23709` ("Rollback test v6: a census of who writes the pages ...") o posterior. Apunta el commit.
2. Compila con `launchers\Build NativeSmash.bat`. Si falla, escribe el error completo y para.
3. Ejecuta el combate automático exactamente como en el pedido 010 (la copia de la partida guardada en `SSBU_DATA_ROOT`, el mismo
   `SSBU_INPUT`, los tiempos +125 s, la carpeta de capturas), con `SSBU_RB_TEST=1`, `SSBU_RB_TEST_AT=200`, `SSBU_LATENCY=1`, **sin**
   `SSBU_RB_PROBE`, durante **330 segundos**.
4. Mira las capturas desde el segundo ~195 hasta el final y describe lo que veas (sin subir imágenes): ¿el combate sigue normal
   después de cada ronda?
5. Escribe `lab/resultados/011-censo.md` con:
   - el commit, si compiló y cuánto tardó;
   - el comando y las variables usadas;
   - **todas** las líneas de `<salida>.log` que empiecen por `[rbtest]` (son las importantes: `census`, `RESULT`, `differs in`, `first written by` y `written by`), **y las líneas de traza que
     siguen a cada `first written by`** (las que empiezan por espacios, como `  main+0x...` o `  guest backtrace`, hasta la siguiente
     línea que no empiece por espacio);
   - las líneas `[latency]` que empiecen por `[latency] 5.0 s:` desde el segundo ~180 en adelante (solo esas, no todas);
   - cualquier línea con `host exception`, `stopped during`, `fatal`, `[watchdog]` o `guest backtrace`, y las 30 líneas siguientes a cada
     una;
   - si se creó algún `reports\crash_*.txt`, `reports\stopped_*.txt` o `reports\freeze_*.txt` durante la prueba: su nombre y, de dentro,
     solo las líneas que contengan `main+`, `sdk+`, `lua2cpp`, `exception`, `fault` o `rbf_` (sin rutas de carpetas personales);
   - lo que viste en las capturas (paso 4).
6. Sube el resultado (procedimiento de `lab/README.md`) y avisa al usuario.
