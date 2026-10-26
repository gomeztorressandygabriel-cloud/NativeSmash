# Pedido 013: el launcher nuevo (rápido) y fotos del mismo cuadro en dos partidas

**Para qué:** dos cosas. **(A)** Comprobar que el launcher nuevo se abre y funciona en Windows: es lo único que falta para fusionar
el netplay en GitHub y que los amigos del usuario puedan actualizar. **(B)** Repetir la prueba de "jugar dos veces lo mismo" del
pedido 012, pero ahora el juego guarda una foto **en el mismo cuadro** de cada partida, para compararlas de verdad.

**Resultado del pedido 012:** excelente, gracias: compiló, la partida normal fue perfecta y la grabación/repetición no falló.
Explicado en `docs/NETPLAY.md`.

**Reglas de siempre:** nada de imágenes ni de `build\` a GitHub; si algo se cuelga, apúntalo y no lo repitas.

## Parte A: el launcher (sube el resultado en cuanto la acabes)

1. En `C:\WindowSmash`: `git status` (sin cambios locales en archivos con seguimiento), `git pull mine cloud/rollback-probe`
   (fast-forward). El último commit debe ser `cc2b00e` ("Lockstep check: pictures at the same frames ...") o posterior.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File tools\launcher_wpf.ps1 -Shot build\launcher-online.png -Page online -Lang es`
   Abre `build\launcher-online.png` y **describe** la página "En línea" (sin subir la imagen): ¿se ve la sección "VS en red con un
   amigo (prueba)" con las opciones Apagado / Soy el host / Me uno? ¿Algo cortado o encimado?
3. Escribe en `vs_netplay.txt` (en `C:\WindowSmash`) una sola línea: `host 2`. Ejecuta
   `powershell -NoProfile -ExecutionPolicy Bypass -File tools\launcher_wpf.ps1 -DryRun -ClickPlay -Lang es` y copia todo lo que
   imprime (debe decir `would start ... SSBU_NETPLAY=host SSBU_NETPLAY_DELAY=2`, sin `--online`).
4. Cambia `vs_netplay.txt` a `join 26.1.2.3 2` y repite el paso 3. Copia lo que imprime.
5. Abre el launcher normal (`Play SSBU.bat`, **sin** pulsar Jugar), ve a "En línea", elige **Soy el host** y pulsa **Crear el
   savegame para el netplay**. ¿Qué mensaje sale? ¿Aparece `savegame-netplay.zip` en el Escritorio y una carpeta `netplay_save` en
   `C:\WindowSmash`? Luego elige **Apagado** y cierra con la ✕ (no Jugar).
6. **Borra** `vs_netplay.txt`, `netplay_save\` y `savegame-netplay.zip` del Escritorio (son de esta prueba).
7. Escribe `lab/resultados/013a-launcher.md` con lo de los pasos 2 a 5 y cualquier error, **súbelo** y avisa al usuario. Luego sigue con
   la parte B.

## Parte B: fotos del mismo cuadro

8. Compila con `launchers\Build NativeSmash.bat` (cambió `gpu_d3d11.c`). Apunta si compiló.
9. **Grabar:** como el paso 4 del pedido 012 (`SSBU_LOCKSTEP=record`, `SSBU_LOCKSTEP_FILE=build\lockstep.bin`,
   `SSBU_LOCKSTEP_FRAMES=16000`, la copia de la partida en `SSBU_DATA_ROOT`, el mismo `SSBU_INPUT`, `SSBU_LATENCY=1`), 300 s, con
   `SSBU_CAPTURE=build/ab13r`.
10. **Repetir:** como el paso 5 del pedido 012 (`SSBU_LOCKSTEP=replay`, el mismo archivo), 300 s, con `SSBU_CAPTURE=build/ab13p`.
    Usa **una copia nueva** de la partida guardada para `SSBU_DATA_ROOT` (la misma de la que partió la grabación), no la que dejó
    la grabación.
11. Compara las fotos: `git show mine/cloud/rollback-lab:lab/tools/compare_pictures.py > build\compare_pictures.py` y luego
    `deps\python\python.exe -I build\compare_pictures.py build\ab13r build\ab13p` (o `python -I ...` si no existe
    `deps\python`). Copia **toda** la salida.
12. Mira tú también 3 o 4 parejas de fotos `lockstep_*.bmp` del combate (las de cuadros más altos): ¿los personajes en el mismo
    sitio, con los mismos porcentajes? Descríbelo (sin subir imágenes).
13. Escribe `lab/resultados/013b-fotos.md` con: si compiló, las variables usadas, todas las líneas `[lockstep]` de cada partida, la
    salida completa del paso 11, lo visto en el paso 12, y cualquier línea con `host exception`, `stopped during`, `fatal` o
    `[watchdog]` (y las 30 siguientes). Súbelo y avisa al usuario.
