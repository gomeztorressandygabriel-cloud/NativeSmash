# Pedido 007: la prueba de rebobinar, versión 3 (el conjunto guardado crece de ronda en ronda)

**Para qué:** en el pedido 006 la ronda 1 salió completa y el juego siguió normal después (¡bien!). Las dos repeticiones salieron
casi iguales (94 páginas distintas de 5 792), y en la ronda 2 el juego se cerró. La causa más probable: páginas que el juego escribe
fuera de las que se guardan (116 en 10 cuadros). La versión 3 las va añadiendo de ronda en ronda, y hace 6 rondas (1, 1, 5, 10, 30 y
60 cuadros). Explicado en `docs/ROLLBACK.md` ("Resultado del pedido 006").

**Resultado del pedido 006:** perfecto, gracias.

**Aviso:** igual que antes, puede colgarse o cerrarse; si pasa, apúntalo y no insistas más de una vez.

## Pasos

1. En `C:\WindowSmash`: `git status` (sin cambios locales en archivos con seguimiento), `git pull mine cloud/rollback-probe`
   (fast-forward). El último commit debe ser `1f24c9d` ("Rollback test v3: grow the hot set ...") o posterior. Apunta el commit.
2. Compila con `launchers\Build NativeSmash.bat`. Si falla, escribe el error completo y para.
3. Ejecuta el combate automático exactamente como en el pedido 006 (la copia de la partida guardada en `SSBU_DATA_ROOT`, el mismo
   `SSBU_INPUT`, los tiempos +125 s, la carpeta de capturas), con `SSBU_RB_TEST=1`, `SSBU_RB_TEST_AT=200`, `SSBU_LATENCY=1`, **sin**
   `SSBU_RB_PROBE`, durante **380 segundos** (más que antes, para que lleguen las 6 rondas).
4. Mira las capturas desde el segundo ~195 hasta el final y describe lo que veas (sin subir imágenes): ¿el combate sigue normal
   después de cada ronda?
5. Escribe `lab/resultados/007-rebobinar-v3.md` con:
   - el commit, si compiló y cuánto tardó;
   - el comando y las variables usadas;
   - **todas** las líneas de `<salida>.log` que empiecen por `[rbtest]` (son las importantes: las que dicen `RESULT` y las que dicen `differs in`);
   - las líneas `[latency]` que empiecen por `[latency] 5.0 s:` desde el segundo ~180 en adelante (solo esas, no todas);
   - cualquier línea con `host exception`, `stopped during`, `fatal`, `[watchdog]` o `guest backtrace`, y las 30 líneas siguientes a cada
     una;
   - si se creó algún `reports\crash_*.txt`, `reports\stopped_*.txt` o `reports\freeze_*.txt` durante la prueba: su nombre y, de dentro,
     solo las líneas que contengan `main+`, `sdk+`, `lua2cpp`, `exception`, `fault` o `rbf_` (sin rutas de carpetas personales);
   - lo que viste en las capturas (paso 4).
6. Sube el resultado (procedimiento de `lab/README.md`) y avisa al usuario.
