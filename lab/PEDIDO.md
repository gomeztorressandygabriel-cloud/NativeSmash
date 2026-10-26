# Pedido 003: la prueba de "rebobinar" con el juego real

**Para qué:** es la prueba que decide si el rollback es posible. El juego guarda la memoria del combate en un cuadro, juega N cuadros,
vuelve atrás, repite esos N cuadros con los mismos botones y compara si llega exactamente al mismo estado. Está explicada en
`docs/ROLLBACK.md` ("La prueba de rebobinar") y el código está en `runtime/net_rbtest.c`.

**Resultado del pedido 002:** las 5 firmas, una coincidencia cada una. Gracias; dos de ellas ya están en el código.

**Aviso:** esta prueba ya no solo mide: engancha funciones del juego y le cambia la memoria (solo con `SSBU_RB_TEST=1`). Es posible que
el juego se cuelgue o se cierre al repetir los cuadros. **Eso también es un resultado útil**: apúntalo y no insistas más de una vez.

## Pasos

1. En `C:\WindowSmash`: `git status` (sin cambios locales en archivos con seguimiento), `git pull mine cloud/rollback-probe`
   (fast-forward). El último commit debe ser `27ece57` ("runtime: rollback rewind test ...") o posterior. Apunta el commit.
2. Compila con `launchers\Build NativeSmash.bat`. **Puede tardar bastante más que otras veces**: el código nuevo engancha dos funciones
   del juego (`HLE_WRAP`) y la compilación tiene que regenerar el módulo principal. Si falla, escribe el error completo y para. Si el
   error habla de una firma (`signature`) que no coincide, cópialo tal cual: es importante.
3. Ejecuta el combate automático igual que en el pedido 001 (la copia de la partida guardada en `SSBU_DATA_ROOT`, el mismo `SSBU_INPUT`
   que funcionó, con los tiempos +125 s, y la carpeta de capturas), con estas variables:
   - `SSBU_RB_TEST=1` y `SSBU_RB_TEST_AT=200` (la prueba empieza a los 200 s, con el combate ya en marcha).
   - **No** pongas `SSBU_RB_PROBE` esta vez.
   - `SSBU_LATENCY=1` sí.
   - Duración: 300 segundos.
4. Mira las capturas desde el segundo ~195 hasta el final: ¿el combate sigue con normalidad? ¿Algo raro (personajes que saltan de sitio,
   imagen congelada, el juego cerrado)? Descríbelo (sin subir imágenes).
5. Escribe `lab/resultados/003-rebobinar.md` con:
   - el commit, si compiló y cuánto tardó la compilación;
   - el comando y las variables usadas;
   - **todas** las líneas de `<salida>.log` que empiecen por `[rbtest]`;
   - las líneas `[latency]` que contengan `vsync event -> game queues its frame` desde el segundo ~180 en adelante;
   - cualquier línea con `host exception`, `stopped during`, `fatal` o `guest backtrace`, y las 30 líneas siguientes a cada una;
   - si se creó algún `reports\crash_*.txt` o `reports\stopped_*.txt` durante la prueba: su nombre y, de dentro, solo las líneas que
     contengan `main+`, `sdk+`, `lua2cpp`, `exception` o `fault` (sin rutas de carpetas personales);
   - lo que viste en las capturas (paso 4).
6. Sube el resultado (procedimiento de `lab/README.md`) y avisa al usuario.
