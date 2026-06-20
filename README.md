# Simulador RISC-V (RV32I)

Simulador del ISA base de RISC-V de 32 bits (RV32I), implementado en C++. Permite cargar programas en binario crudo, ejecutarlos paso a paso o de forma continua, e inspeccionar el estado del procesador (PC, registros, memoria) en cualquier punto de la ejecución.

Desarrollado para el curso **CS3051 - Arquitectura de Computadores** (HW#6 - Tarea 4: Simulador de RISC-V).

## Características

- Soporte completo del ISA base RV32I (ver tabla de instrucciones abajo)
- Memoria plana byte-addressable de 1 MB, little-endian
- Carga de binarios crudos (`.bin`) en la dirección `0x00000000`
- Ejecución paso a paso (`step`) o continua (`run`)
- Inspección de PC, registros (por nombre `x0`-`x31` o ABI `ra`, `sp`, `a0`, etc.) y memoria en cualquier rango
- **Desensamblador integrado**: muestra cada instrucción en formato assembly legible al ejecutarla, y permite desensamblar cualquier rango de memoria sin ejecutar (`disasm`)
- Detección automática de loops infinitos (instrucción que salta a sí misma), deteniendo `run` sin intervención del usuario
- Soporte parcial de `ecall` (syscalls estilo SPIM): `exit`, `print_int`, `print_string`, `print_char`
- Manejo de errores explícito: accesos fuera de rango, opcodes desconocidos

## Estructura del proyecto

```
riscv-sim/
├── src/
│   └── main.cpp       # Código fuente completo del simulador
├── tests/             # Binarios de prueba (ver sección de pruebas)
│   ├── riscvtest.bin
│   ├── quicksort.bin
│   └── tree.bin
├── Makefile
└── README.md
```

## Compilación

Requiere un compilador con soporte de C++17 (g++, clang++).

```bash
make
```

o manualmente:

```bash
g++ -std=c++17 -O2 -o riscv-sim src/main.cpp
```

En Windows, con [MSYS2](https://www.msys2.org/) o [MinGW](https://github.com/niXman/mingw-builds-binaries) instalado:

```powershell
g++ -std=c++17 -O2 -o riscv-sim src/main.cpp
```

## Uso

```bash
./riscv-sim <programa.bin>
```

Esto carga el binario en memoria y abre una consola interactiva de comandos.

### Comandos disponibles

| Comando | Descripción |
|---|---|
| `step` | Ejecuta una instrucción y muestra su desensamblado |
| `run` | Ejecuta hasta terminar (detección automática de loop infinito o instrucción nula) |
| `pc` | Muestra el valor actual del PC |
| `regs [x0 x1 ...]` | Muestra registros específicos, o todos si no se especifica |
| `mem <start> <end>` | Muestra el contenido de memoria en un rango (direcciones en hex, sin `0x`) |
| `disasm <start> <end>` | Desensambla instrucciones en un rango de memoria, sin ejecutarlas |
| `reset` | Reinicia PC y registros a su estado inicial (la memoria no se reinicia) |
| `exit` | Sale del simulador |

### Ejemplo de sesión

```
$ ./riscv-sim quicksort.bin
"quicksort.bin" cargado a memoria (1024 bytes).

> disasm 0 10
0x00000000:  lui sp, 16
0x00000004:  lui a0, 1
0x00000008:  addi t1, zero, 6
0x0000000c:  sw t1, 0(a0)
0x00000010:  addi t1, zero, 4

> run
Ejecutando programa...
[INFO] Loop infinito detectado en 0x0000004c (instrucción salta a sí misma). Deteniendo.
Total de instrucciones ejecutadas: 588
Programa terminado con código 0.

> mem 1000 101c
Memoria (0x1000-0x101c): 01 00 00 00 02 00 00 00 03 00 00 00 04 00 00 00 06 00 00 00 08 00 00 00 09 00 00 00 00

> exit
See you next time...
Program exited with code 0.
```

## Instrucciones soportadas

| Tipo | Instrucciones |
|---|---|
| Loads | `lb`, `lh`, `lw`, `lbu`, `lhu` |
| Stores | `sb`, `sh`, `sw` |
| Aritmética/lógica inmediata | `addi`, `slti`, `sltiu`, `xori`, `ori`, `andi`, `slli`, `srli`, `srai` |
| Aritmética/lógica registro-registro | `add`, `sub`, `sll`, `slt`, `sltu`, `xor`, `srl`, `sra`, `or`, `and` |
| Upper immediate | `lui`, `auipc` |
| Branches | `beq`, `bne`, `blt`, `bge`, `bltu`, `bgeu` |
| Saltos | `jal`, `jalr` |
| Sistema | `ecall` |

## Pruebas

El simulador fue verificado con los tres programas recomendados en el enunciado:

| Programa | Comportamiento esperado | Resultado |
|---|---|---|
| `riscvtest.bin` | Escribe `25` (`0x19`) en la dirección `100` (`0x64`) | ✅ `mem[0x64] = 0x19` |
| `quicksort.bin` | Ordena `{6,4,3,2,1,8,9}` → `{1,2,3,4,6,8,9}` en `0x1000` | ✅ `01 02 03 04 06 08 09` |
| `tree.bin` | Evalúa si `{6,4,4,1,2,2,1}` es un árbol simétrico → `a0 = 1` | ✅ `x10 = 0x00000001` |

Para reproducir las pruebas:

```bash
./riscv-sim tests/riscvtest.bin
> run
> mem 60 67

./riscv-sim tests/quicksort.bin
> run
> mem 1000 101c

./riscv-sim tests/tree.bin
> run
> regs x10
```

## Autor

Kamilo — UTEC, CS3051 Arquitectura de Computadores, 2025-II.
