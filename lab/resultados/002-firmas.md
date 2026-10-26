# Resultado del pedido 002: firmas de las funciones del bucle principal

**Estado: terminado.** No se compilo ni se ejecuto el juego; solo `wrap-sig`. Ningun cambio de codigo.

- `<nso>` usado: `dump/exefs/main` (relativo a `C:\WindowSmash`, el modulo principal que usa la compilacion del puerto; no se copio a ningun sitio).
- Comando: `python tools/recomp.py wrap-sig dump/exefs/main <direccion>`. Todas terminaron con codigo de salida 0, sin errores.
- Las direcciones se leyeron como direcciones completas (`7103746e90` = `main+0x3746e90`, como en las pilas de la sonda).

## Salida exacta de `wrap-sig`

### 7103746e90 (el bucle principal)
```
/* signature: 6db63bef 6d0133ed 6d022beb 6d0323e9 a9046ffc a90567fa a9065ff8 a90757f6 a9084ff4 a9097bfd 910243fd d1400bff d11703ff f9400008 f9400d08 aa0003f5 */
(1 match(es) in the module's code)
```

### 7103667300 (lee los mandos)
```
/* signature: fc1a0fe8 a90167fa a9025ff8 a90357f6 a9044ff4 a9057bfd 910143fd .0.....8 91....08 08dffd08 360....8 .0.....8 39....08 34.....8 .0.....8 b9....08 */
(1 match(es) in the module's code)
```

### 71037246a0 (la simulacion, candidata)
```
/* signature: d10243ff a9036ffc a90467fa a9055ff8 a90657f6 a9074ff4 a9087bfd 910203fd b9403008 aa0003f3 7100051f 54.....0 7100091f 54.....1 aa1303f4 f8438e80 */
(1 match(es) in the module's code)
```

### 71038fe2b0 (paso sin identificar)
```
/* signature: f81d0ff5 a9014ff4 a9027bfd 910083fd f9400c14 f9400e88 aa0003f3 f100051f 54.....b b980ba89 d29eb86b f2ab850b d343fd0a f2d851eb f2e51eab 9bcb7d4a */
(1 match(es) in the module's code)
```

### 710386f850 (el dibujo)
```
/* signature: f81b0ff9 a9015ff8 a90257f6 a9034ff4 a9047bfd 910103fd f900005f f9405437 3943b828 aa0203f5 aa0103f3 aa0003f4 34.....8 9100c2f6 1....... aa1703f6 */
(1 match(es) in the module's code)
```

Las cinco son comienzo de funcion y su firma tiene exactamente 1 coincidencia en el codigo del modulo.

## Version del juego
**No puedo confirmar 13.0.5.** La compilacion no la dice: la carpeta principal `dump/` (el puerto) contiene el juego base y no tiene `version.txt` ni `game_source.txt` (segun `docs/VERSIONS.md`, `version.txt` solo existe en las carpetas de otras versiones, p. ej. `v13.0.5/`, y en este PC no hay ninguna). No he adivinado la version. Lo unico comprobable: las pilas de la sonda v4 (`main+0x3746e90`, `main+0x3667300`, `main+0x386f850`) coinciden con estas direcciones, asi que las firmas son de este mismo `main`. Si se necesita el numero de version exacto, puedo sacar el build ID de `main` para compararlo con el de 13.0.5.
