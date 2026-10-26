# Pedido 006: la prueba de rebobinar, versión 2 (dos repeticiones, y el juego no se toca)

**Para qué:** en el pedido 005 el rebobinado ya funcionó sin caerse (¡gran avance!), pero la comparación mezclaba causas y el juego se
congeló después. La versión 2 repite los cuadros **dos veces** desde el mismo guardado y compara una repetición con la otra, separa
lo que escribe la simulación de lo que escribe el dibujo, y al terminar **devuelve al juego su estado**, así que debería seguir normal
y llegar a las 4 rondas (1, 10, 30 y 60 cuadros). Explicado en `docs/ROLLBACK.md` ("Resultado del pedido 005").

**Resultado del pedido 005:** muy útil, gracias.

**Aviso:** igual que antes, puede colgarse o cerrarse; si pasa, apúntalo y no insistas más de una vez.

## Pasos

1. En `C:\WindowSmash`: `git status` (sin cambios locales en archivos con seguimiento), `git pull mine cloud/rollback-probe`
   (fast-forward). El último commit debe ser `78fa5d8` ("Rollback test v2: replay twice, ...") o posterior. Apunta el commit.
2. Compila con `launchers\Build NativeSmash.bat`. Si falla, escribe el error completo y para.
3. Ejecuta el combate automático exactamente como en el pedido 005 (la copia de la partida guardada en `SSBU_DATA_ROOT`, el mismo
   `SSBU_INPUT`, los tiempos +125 s, la carpeta de capturas), con `SSBU_RB_TEST=1`, `SSBU_RB_TEST_AT=200`, `SSBU_LATENCY=1`, **sin**
   `SSBU_RB_PROBE`, durante **330 segundos** (un poco más que antes, para que lleguen las 4 rondas: la última empieza hacia el s 245).
4. Mira las capturas desde el segundo ~195 hasta el final y describe lo que veas (sin subir imágenes): ¿el combate sigue normal
   después de cada ronda?
5. Escribe `lab/resultados/006-rebobinar-v2.md` con:
   - el commit, si compiló y cuánto tardó;
   - el comando y las variables usadas;
   - **todas** las líneas de `<salida>.log` que empiecen por `[rbtest]` (son las importantes: las que dicen `RESULT`);
   - las líneas `[latency]` que empiecen por `[latency] 5.0 s:` desde el segundo ~180 en adelante (solo esas, no todas);
   - cualquier línea con `host exception`, `stopped during`, `fatal`, `[watchdog]` o `guest backtrace`, y las 30 líneas siguientes a cada
     una;
   - si se creó algún `reports\crash_*.txt`, `reports\stopped_*.txt` o `reports\freeze_*.txt` durante la prueba: su nombre y, de dentro,
     solo las líneas que contengan `main+`, `sdk+`, `lua2cpp`, `exception`, `fault` o `rbf_` (sin rutas de carpetas personales);
   - lo que viste en las capturas (paso 4).
6. Sube el resultado (procedimiento de `lab/README.md`) y avisa al usuario.
