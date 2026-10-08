# Projeto Integrador AOC: Takum na extensão vetorial RISC-V (Zvtakum)

As extensões de instrução Single Instruction, Multiple Data (SIMD) e as unidades vetoriais modernas evoluíram
para suportar uma ampla variedade de formatos de ponto flutuante de baixa precisão (como IEEE 754 FP16,
bfloat16, OFP8 E4M3 e E5M2) voltados para inteligência artificial e aprendizado profundo. No entanto,
a proliferação desses formatos baseados no padrão IEEE 754 introduziu inconsistências, cantos de exceção
complexos e sobrecarga na lógica de decodificação no hardware.
A aritmética Takum surge como uma alternativa de precisão afunilada (tapered precision), garantindo alto
alcance dinâmico mesmo em representações de 8 e 16 bits e compartilhando uma estrutura de decodificação
comum de no máximo 12 bits mais significativos. Este projeto propõe a codificação e avaliação de uma extensão
vetorial simplificada para a arquitetura aberta RISC-V utilizando o formato Takum 


Grupo: Rafael Estevão; Glaucia; Lucas. UFMG, AOC 2026/02.


## Requisitos
- Linux ou WSL2 (Ubuntu). **Não compila com MSVC**, pois o código usa `__int128`.
- `g++` (C++17), `gcc`, `make`, `git`, em arquitetura 64 bits.
- <Python 3.x e bibliotecas, se avaliacao_takum.py usar matplotlib/numpy: liste aqui>

Instalação no Ubuntu/WSL:
```bash
sudo apt install build-essential git
```

## Como clonar
A libtakum (referência oficial) vem como submódulo, então clone com:
```bash
git clone --recurse-submodules https://github.com/marquesglaucia/projeto_takum_aoc.git
```
Se você já clonou sem a flag:
```bash
git submodule update --init
```

## Como rodar os testes
Na raiz do projeto:
```bash
make test LIBTAKUM=third_party/libtakum
```

Saída esperada (termina assim):
```
[decode/encode] falhas ate aqui: 0
[T8 exaustivo, 196608 ops] falhas: 0
[T16 aleatorio, 9M ops] falhas: 0
[T32 aleatorio, 9M ops] falhas: 0
RESULTADO: TUDO OK (0)
```
Em caso de falha, o programa imprime até 15 casos (bits de entrada, resultado
obtido e resultado da referência). O teste leva alguns segundos na nossa máquina.

## Estrutura do repositório
| Caminho | O que é |
|---|---|
| `takum_decoder.h/.cpp` | Decodificação, codificação e aritmética Takum |
| `test_takum.cpp` | Testes contra a libtakum |
| `Makefile` | Compila a libtakum e roda os testes |
| `third_party/libtakum` | Submódulo, usado **somente como oráculo de teste** |
| `riscv-isa-sim/` | <descrever: simulador e o que foi modificado> |
| `avaliacao_takum.py` | <descrever: o que gera, como rodar> |
| `docs/` | Slides, artigo IEEE e `DECISIONS.md` |

## Como usar a biblioteca
Exemplo mínimo (decodificar e somar em T16):
```cpp
#include "takum_decoder.h"
uint16_t a = takum_from_ld(1.5L, 16), b = takum_from_ld(2.25L, 16);
long double s = takum_to_ld(takum_adicao_T16(a, b), 16);   // 3.75
```
Compilar com: `g++ -O2 -std=c++17 main.cpp takum_decoder.cpp -o main`

## Decisões de projeto
Ver `docs/DECISIONS.md` (layout, arredondamento, saturação, valores especiais).

## Problemas comuns
- `No such file or directory` na libtakum: o submódulo está vazio, rode `git submodule update --init`.

## Referências e licenças
- [1] L. Hunhold, "Streamlining SIMD ISA extensions with takum arithmetic: ..." MOCAST 2025.
- [2] L. Hunhold, *Beating Posits at Their Own Game: Takum Arithmetic*, Springer, 2024.
- libtakum: https://github.com/takum-arithmetic/libtakum (licença: Copyright 2024-2025 Laslo Hunhold <laslo@hunhold.de>)
- riscv-isa-sim (Spike): Copyright (c) 2010-2017, The Regents of the University of California
(Regents).  All Rights Reserved.