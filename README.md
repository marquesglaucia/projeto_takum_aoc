# Projeto Integrador AOC: Takum na extensão vetorial RISC-V (Zvtakum)

Unidades vetoriais e extensões SIMD hoje suportam muitos formatos de ponto flutuante de baixa precisão
(FP16, bfloat16, OFP8 E4M3 e E5M2), criados para inteligência artificial. Cada formato IEEE 754 novo traz
seus próprios casos especiais (NaNs, infinitos, subnormais, modos de arredondamento e flags de exceção),
o que complica o decodificador e a ISA [1].

O **takum** [2] é um formato de precisão afunilada (*tapered precision*) em que o número de bits de expoente
e de fração varia com a magnitude. Com isso, mesmo em 8 e 16 bits ele cobre uma faixa dinâmica muito
grande, e todas as larguras compartilham um mesmo início de decodificação (os 12 bits mais significativos
bastam para extrair sinal, regime e característica).

Neste projeto implementamos o takum de 8, 16 e 32 bits (T8, T16, T32) em um modelo funcional em C++,
validado contra a biblioteca de referência (libtakum), e propomos uma extensão vetorial **Zvtakum** para o
RISC-V (`vtadd.vv`, `vtsub.vv`, `vtmul.vv`, `vtdot.vv`, `vncvt.t.t`), implementada no simulador Spike. Depois
comparamos o takum com FP8/FP16/FP32 em multiplicação matriz-vetor esparsa (erro relativo contra precisão
quádrupla) e analisamos o efeito na contagem de instruções e nos registradores de controle (CSRs).

Grupo: Rafael Estevão, Glaucia e Lucas. UFMG, Arquitetura e Organização de Computadores (AOC) 2026/02.

## Requisitos
- Linux ou WSL2 (Ubuntu). **Não compila com MSVC**, pois o código usa `__int128`.
- `g++` (C++17), `gcc`, `make`, `git`, em arquitetura 64 bits.
- Python 3.x e as bibliotecas de `requirements.txt`.
- Só para o Spike e para a análise de ISA: `gcc-riscv64-unknown-elf`, `device-tree-compiler`, Boost (veja `docs/SPIKE.md`).

Instalação no Ubuntu/WSL:
```bash
sudo apt install build-essential git python3-pip
pip install -r requirements.txt      # numpy, scipy, matplotlib
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

## Como rodar tudo (sem o Spike)
Na raiz do projeto, depois de clonar com `--recurse-submodules`:
```bash
make all        # = make test integracao dot spmv
```
Os alvos também podem ser rodados separadamente, como abaixo. `make clean` remove o que foi gerado.
`make spmv` regrava os gráficos `grafico_*.png`.

## Como rodar os testes
Na raiz do projeto:
```bash
make test
```
(a libtakum é procurada em `third_party/libtakum`; use `LIBTAKUM=/outro/caminho` para mudar.)

Saída esperada (termina assim):
```
[decode/encode] falhas ate aqui: 0
[T8 exaustivo, 262144 ops] falhas: 0
[T16 aleatorio, 12M ops] falhas: 0
[T32 aleatorio, 12M ops] falhas: 0
RESULTADO: TUDO OK (0)
```
Em caso de falha, o programa imprime até 15 casos (bits de entrada, resultado
obtido e resultado da referência). O teste leva alguns segundos na nossa máquina.

## Como rodar a integração numérica
Na raiz do projeto (não depende da libtakum):
```bash
make integracao
```

Imprime um autoteste da aritmética (soma, multiplicação, divisão e negação em T8/T16/T32),
uma tabela de erro relativo de cada método de integração (ponto médio, trapézio, Simpson 1/3 e 3/8,
Gauss-Legendre 2/3/5 pontos, Romberg e Simpson adaptativo) em T8, T16, float, T32, double e
long double para 7 integrais de teste, e a comparação entre soma ingênua e soma de Kahan.

## Spike e análise de instruções/CSRs
Com o Spike compilado (`docs/SPIKE.md`):
```bash
make spike-test SPIKE=riscv-isa-sim/build/spike     # 5 instruções Zvtakum, 22 testes
make analise-isa SPIKE=riscv-isa-sim/build/spike    # contagem de instruções e CSRs, RVV/IEEE x Zvtakum
make demo SPIKE=riscv-isa-sim/build/spike           # roteiro da demonstração (docs/DEMO.md)
```
Os resultados da análise estão em `docs/ANALISE_ISA.md`.

## Integração contínua
`.github/workflows/ci.yml` roda `make all` em um clone limpo (com submódulos) a cada *push* e *pull request*, e
compila o Spike para rodar `make spike-test` e `make analise-isa`.

## Estrutura do repositório
| Caminho | O que é |
|---|---|
| `takum_decoder.h/.cpp` | Decodificação, codificação e aritmética Takum |
| `test_takum.cpp` | Testes contra a libtakum (soma, subtração, multiplicação e divisão) |
| `takum_tipo.h` | Tipo `Takum<N>` (`T8`/`T16`/`T32`) com operadores `+ - * /` |
| `takum_integracao.h` | Métodos de integração numérica (templates: rodam em takum e IEEE) |
| `test_integracao.cpp` | Compara as integrais em takum e IEEE (`make integracao`) |
| `Makefile` | `make all`, `test`, `integracao`, `dot`, `spmv`, `spike-test`, `analise-isa`, `demo`, `clean` |
| `test_dot.cpp`, `verifica_dot.py` | Validação do `takum_dot` (vtdot) vs `__float128` e modelo Python exato |
| `spike_test/` | Programa bare-metal que executa as 5 instruções Zvtakum no Spike |
| `docs/ZVTAKUM.md`, `docs/SPIKE.md`, `docs/DEMO.md` | Especificação da ISA, alterações no Spike, roteiro da demo e plano B |
| `docs/artigo/secao_arquitetura.tex` | Seção "Proposed Architecture" do artigo |
| `third_party/libtakum` | Submódulo, usado **somente como oráculo de teste** |
| `riscv-isa-sim/` | Spike modificado com as instruções Zvtakum no espaço custom-1 (lista exata de arquivos alterados em `docs/SPIKE.md`) |
| `test_spmv.cpp`, `avaliacao_takum.py` | Benchmark SpMV/GEMV (Takum x IEEE/OFP8, referência `__float128`) e gráficos (`make spmv`) |
| `grafico_*.png` | Figuras geradas por `make spmv` e usadas no artigo |
| `analise_isa/` | Kernels RVV/IEEE e Zvtakum, contagem estática/dinâmica de instruções e medidas de CSR (`make analise-isa`) |
| `docs/ANALISE_ISA.md` | Resultados e conclusões da análise de instruções e CSRs |
| `docs/` | `DECISIONS.md` (decisões de projeto), especificação e demo (acima) e `artigo/` |
| `.github/workflows/ci.yml` | Integração contínua |

## Como usar a biblioteca
Exemplo mínimo (decodificar e somar em T16):
```cpp
#include "takum_decoder.h"
uint16_t a = takum_from_ld(1.5L, 16), b = takum_from_ld(2.25L, 16);
long double s = takum_to_ld(takum_adicao_T16(a, b), 16);   // 3.75
long double q = takum_to_ld(takum_divisao_T16(a, b), 16);  // ~0.6667
```
Compilar com: `g++ -O2 -std=c++17 main.cpp takum_decoder.cpp -o main`

## Decisões de projeto
Ver `docs/DECISIONS.md` (layout, arredondamento, saturação, valores especiais).

## Problemas comuns
- `libtakum ausente` ou `No such file or directory` na libtakum: o submódulo está vazio, rode `git submodule update --init`.
- `cannot find -lquadmath`: instale as bibliotecas do GCC (`sudo apt install build-essential`).
- `riscv64-unknown-elf-gcc: not found` ao rodar `make spike-test`: `sudo apt install gcc-riscv64-unknown-elf`.

## Referências e licenças
- [1] L. Hunhold, "Streamlining SIMD ISA Extensions with Takum Arithmetic: A Case Study on Intel AVX10.2",
  *2025 14th International Conference on Modern Circuits and Systems Technologies (MOCAST)*, jun. 2025, pp. 1-6.
- [2] L. Hunhold, *Beating Posits at Their Own Game: Takum Arithmetic*, Springer Nature Switzerland, 2024, pp. 1-51.
  doi: 10.1007/978-3-031-72709-2_1
- libtakum: https://github.com/takum-arithmetic/libtakum (licença: Copyright 2024-2025 Laslo Hunhold <laslo@hunhold.de>)
- riscv-isa-sim (Spike): Copyright (c) 2010-2017, The Regents of the University of California
(Regents).  All Rights Reserved.
