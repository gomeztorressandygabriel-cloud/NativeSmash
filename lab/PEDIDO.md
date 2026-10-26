# Pedido 009: la prueba de rebobinar, versión 5 (dejar fuera lo que escriben otros hilos)

**Para qué:** el pedido 008 encontró que otros hilos del juego (11, 19 y 47) escriben en las páginas que no cuadraban, y que
rebobinarlas tumbó al hilo 19. La versión 5 deja fuera de los rebobinados las páginas que escribe otro hilo, vigila todo el tiempo
desde que se devuelven las páginas hasta que se miden, y también el siguiente cuadro normal del juego. Explicado en `docs/ROLLBACK.md`
("Resultado del pedido 008").

**Resultado del pedido 008:** perfecto, era justo lo que hacía falta. Gracias.

**Aviso:** igual que antes, puede colgarse o cerrarse; si pasa, apúntalo y no insistas más de una vez.

## Pasos

1. En `C:\WindowSmash`: `git status` (sin cambios locales en archivos con seguimiento), `git pull mine cloud/rollback-probe`
   (fast-forward). El último commit debe ser `e68d5b4` ("Rollback test v5: leave out pages other threads write ...") o posterior. Apunta el commit.
2. Compila con `launchers\Build NativeSmash.bat`. Si falla, escribe el error completo y para.
3. Ejecuta el combate automático exactamente como en el pedido 008 (la copia de la partida guardada en `SSBU_DATA_ROOT`, el mismo
   `SSBU_INPUT`, los tiempos +125 s, la carpeta de capturas), con `SSBU_RB_TEST=1`, `SSBU_RB_TEST_AT=200`, `SSBU_LATENCY=1`, **sin**
   `SSBU_RB_PROBE`, durante **330 segundos**.
4. Mira las capturas desde el segundo ~195 hasta el final y describe lo que veas (sin subir imágenes): ¿el combate sigue normal
   después de cada ronda?
5. Escribe `lab/resultados/009-dejar-fuera.md` con:
   - el commit, si compiló y cuánto tardó;
   - el comando y las variables usadas;
   - **todas** las líneas de `<salida>.log` que empiecen por `[rbtest]` (son las importantes: `RESULT`, `differs in`, `first written by` y `written by`), **y las líneas de traza que
     siguen a cada `first written by`** (las que empiezan por espacios, como `  main+0x...` o `  guest backtrace`, hasta la siguiente
     línea que no empiece por espacio);
   - las líneas `[latency]` que empiecen por `[latency] 5.0 s:` desde el segundo ~180 en adelante (solo esas, no todas);
   - cualquier línea con `host exception`, `stopped during`, `fatal`, `[watchdog]` o `guest backtrace`, y las 30 líneas siguientes a cada
     una;
   - si se creó algún `reports\crash_*.txt`, `reports\stopped_*.txt` o `reports\freeze_*.txt` durante la prueba: su nombre y, de dentro,
     solo las líneas que contengan `main+`, `sdk+`, `lua2cpp`, `exception`, `fault` o `rbf_` (sin rutas de carpetas personales);
   - lo que viste en las capturas (paso 4).
6. Sube el resultado (procedimiento de `lab/README.md`) y avisa al usuario.
