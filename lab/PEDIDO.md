# Pedido 005: rebobinar guardando las fibras (repetición del 004, ya corregido)

**Para qué:** en el pedido 003 el juego se cerró al repetir el primer cuadro porque la memoria volvía atrás pero las "fibras" del juego
(tareas internas con su propia pila) no. El código nuevo (`runtime/net_rbfiber.c`, y cambios en `runtime/hle_fiber.c` y
`runtime/net_rbtest.c`) las guarda y las devuelve a su sitio junto con la memoria. Solo se activa con `SSBU_RB_TEST=1`; sin esa variable
el juego usa las fibras de Windows como siempre.

**Resultado del pedido 004:** perfecto, gracias: el error que copiaste era justo lo que hacía falta. El compilador del PC leía el
ensamblador con otra sintaxis; ya está corregido (`.att_syntax prefix`). Mismos pasos que el 004.

**Aviso:** igual que antes, es posible que el juego se cuelgue o se cierre. **Eso también es un resultado útil**: apúntalo y no insistas
más de una vez. Si el juego se cierra **antes** del segundo 200 (antes de que la prueba empiece), también apúntalo: querría decir que
las fibras nuevas fallan por sí solas.

## Pasos

1. En `C:\WindowSmash`: `git status` (sin cambios locales en archivos con seguimiento), `git pull mine cloud/rollback-probe`
   (fast-forward). El último commit debe ser `6661b03` ("Rollback fibers: state the assembly syntax ...") o posterior.
   Apunta el commit.
2. Compila con `launchers\Build NativeSmash.bat`. Si falla, escribe el error completo y para (si menciona `net_rbfiber.c`, el
   ensamblador o `__asm__`, cópialo tal cual: es importante).
3. Ejecuta el combate automático exactamente como en el pedido 003 (la copia de la partida guardada en `SSBU_DATA_ROOT`, el mismo
   `SSBU_INPUT`, los tiempos +125 s, la carpeta de capturas), con `SSBU_RB_TEST=1`, `SSBU_RB_TEST_AT=200`, `SSBU_LATENCY=1`, **sin**
   `SSBU_RB_PROBE`, durante 300 segundos.
4. Mira las capturas desde el segundo ~195 hasta el final y describe lo que veas (sin subir imágenes). Mira también una captura de
   antes del segundo 200: ¿el juego iba normal hasta ahí?
5. Escribe `lab/resultados/005-fibras.md` con lo mismo que el 003:
   - el commit, si compiló y cuánto tardó;
   - el comando y las variables usadas;
   - **todas** las líneas de `<salida>.log` que empiecen por `[rbtest]`;
   - las líneas `[latency]` que contengan `vsync event -> game queues its frame` desde el segundo ~180 en adelante;
   - cualquier línea con `host exception`, `stopped during`, `fatal`, `fiber` o `guest backtrace`, y las 30 líneas siguientes a cada una;
   - si se creó algún `reports\crash_*.txt` o `reports\stopped_*.txt` durante la prueba: su nombre y, de dentro, solo las líneas que
     contengan `main+`, `sdk+`, `lua2cpp`, `exception`, `fault` o `rbf_` (sin rutas de carpetas personales);
   - lo que viste en las capturas (paso 4).
6. Sube el resultado (procedimiento de `lab/README.md`) y avisa al usuario.
