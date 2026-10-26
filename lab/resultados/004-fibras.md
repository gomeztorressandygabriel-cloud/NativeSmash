# Resultado del pedido 004: rebobinar guardando las fibras

**Estado: fallo de compilacion en el paso 2 (error esperado en el ensamblador inline de `runtime/net_rbfiber.c`).** Siguiendo las instrucciones de PEDIDO.md ("Si falla, escribe el error completo y para (si menciona net_rbfiber.c, el ensamblador o __asm__, copialo tal cual: es importante)"), se detiene la ejecucion aqui sin modificar el codigo fuente.

## Codigo y compilacion
- Commit usado: `0cc5b7f` ("Rollback test, phase 3b: save and restore the game's fibers").
- Compilacion: `launchers\Build NativeSmash.bat`.
- Resultado: **Fallo al compilar `runtime/net_rbfiber.c`**.
- Tiempo transcurrido hasta el fallo: ~75 segundos.
- Toolchain detectado: `deps\llvm\bin\clang-cl.exe` (LLVM 23.1.3) con `deps\msvc` (MSVC 14.51.36231, Windows SDK 10.0.26100.0).

### Causa tecnica del error
En Windows, el compilador del proyecto es `clang-cl`. En este modo (emulacion de MSVC), el bloque `__asm__` o el ensamblador integrado asume sintaxis Intel por defecto. Al encontrar instrucciones en sintaxis AT&T con prefijo de registro `%` (como `pushq %rbp`, `subq , %rsp` o `movaps %xmm6, ...`), el parser de expresiones rechaza el caracter `%` con `error: unknown token in expression`. Para compilar sintaxis AT&T en `__asm__` bajo `clang-cl` se requiere la directiva `.att_syntax prefix` al inicio del bloque de ensamblador (o escribir el bloque en sintaxis Intel).

### Error completo del compilador
```
FAILED net_rbfiber.c:
<inline asm>(4,11): error: unknown token in expression
    4 |     pushq %rbp
      |           ^
<inline asm>(5,11): error: unknown token in expression
    5 |     pushq %rbx
      |           ^
<inline asm>(6,11): error: unknown token in expression
    6 |     pushq %rdi
      |           ^
<inline asm>(7,11): error: unknown token in expression
    7 |     pushq %rsi
      |           ^
<inline asm>(8,11): error: unknown token in expression
    8 |     pushq %r12
      |           ^
<inline asm>(9,11): error: unknown token in expression
    9 |     pushq %r13
      |           ^
<inline asm>(10,11): error: unknown token in expression
   10 |     pushq %r14
      |           ^
<inline asm>(11,11): error: unknown token in expression
   11 |     pushq %r15
      |           ^
<inline asm>(12,10): error: unknown token in expression
   12 |     subq $0xA8, %rsp
      |          ^
<inline asm>(13,12): error: unknown token in expression
   13 |     movaps %xmm6, 0x00(%rsp)
      |            ^
<inline asm>(14,12): error: unknown token in expression
   14 |     movaps %xmm7, 0x10(%rsp)
      |            ^
<inline asm>(15,12): error: unknown token in expression
   15 |     movaps %xmm8, 0x20(%rsp)
      |            ^
<inline asm>(16,12): error: unknown token in expression
   16 |     movaps %xmm9, 0x30(%rsp)
      |            ^
<inline asm>(17,12): error: unknown token in expression
   17 |     movaps %xmm10, 0x40(%rsp)
      |            ^
<inline asm>(18,12): error: unknown token in expression
   18 |     movaps %xmm11, 0x50(%rsp)
      |            ^
<inline asm>(19,12): error: unknown token in expression
   19 |     movaps %xmm12, 0x60(%rsp)
      |            ^
<inline asm>(20,12): error: unknown token in expression
   20 |     movaps %xmm13, 0x70(%rsp)
      |            ^
<inline asm>(21,12): error: unknown token in expression
   21 |     movaps %xmm14, 0x80(%rsp)
      |            ^
<inline asm>(22,12): error: unknown token in expression
   22 |     movaps %xmm15, 0x90(%rsp)
      |            ^
fatal error: too many errors emitted, stopping now [-ferror-limit=]
20 errors generated.
```

## Ejecucion y capturas
No se ejecutaron las pruebas automaticas (pasos 3 y 4) debido al fallo de compilacion del ejecutable.
