# Pedido 002: las "firmas" de las funciones del bucle principal

**Para qué:** el siguiente paso grande (la prueba de "rebobinar") necesita enganchar funciones del juego con `HLE_WRAP(...)`. Cada
enganche lleva una **firma**: las primeras instrucciones de la función con las direcciones tapadas, que el compilador del proyecto
comprueba para no enganchar la función equivocada. Las firmas solo se pueden sacar con el juego, así que las pido aquí. Este pedido
**no compila ni ejecuta el juego**: es corto.

**Resultado del pedido 001:** funcionó (combate automático en el intento 2, con los tiempos del guion +125 s). Gracias.

## Pasos

1. En `C:\WindowSmash`, mira cómo se usa `tools/recomp.py wrap-sig <nso> <dirección>` (su ayuda: `python tools/recomp.py`, y
   `docs/TECHNICAL.md`, apartado de los `HLE_WRAP`). El `<nso>` es el módulo principal del juego (`main`) tal como lo usa la compilación
   (por ejemplo `dump/exefs/main` o el que use `tools/build_game.py`; búscalo, no lo copies a ningún sitio).
2. Ejecuta `wrap-sig` para estas direcciones del módulo `main`:
   - `7103746e90` (el bucle principal)
   - `7103667300` (lee los mandos)
   - `71037246a0` (la simulación, candidata)
   - `71038fe2b0` (paso sin identificar)
   - `710386f850` (el dibujo)
3. Escribe `lab/resultados/002-firmas.md` con, para cada dirección, exactamente lo que imprime `wrap-sig`: la línea
   `/* signature: ... */` y la línea `(N match(es) in the module's code)`. Si alguna dice que no es el comienzo de una función, o da un
   error, cópialo tal cual.
4. Añade también: la ruta del `<nso>` que usaste y la versión del juego que dice la compilación (13.0.5 esperado).
5. No subas nada más (ni el módulo, ni desensamblados). Sube el resultado (procedimiento de `lab/README.md`) y avisa al usuario.
