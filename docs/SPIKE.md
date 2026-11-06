# Spike com Zvtakum: como compilar e testar

## O que foi alterado no `riscv-isa-sim`
| Arquivo | Alteração |
|---|---|
| `riscv/insns/vtadd_vv.h`, `vtsub_vv.h`, `vtmul_vv.h`, `vtdot_vv.h`, `vncvt_t_t.h` | **novos**: as 5 instruções |
| `riscv/takum_glue.h`, `riscv/takum_glue.cc` | **novos**: ponte para a fonte única `../takum_decoder.*` |
| `riscv/encoding.h` | `MATCH`/`MASK` e `DECLARE_INSN` das 5 instruções (custom-1, `0x2b`) |
| `riscv/riscv.mk.in` | `takum_glue.cc` em `riscv_srcs`; lista `riscv_insn_ext_zvtakum` em `riscv_insn_list` (junto do V) |
| `riscv/insn_template.cc` | `#include "takum_glue.h"` (uma vez, no topo) |
| `disasm/disasm.cc` | disassembly das 5 instruções |
| `riscv/opcodes.h` | removido o resquício `MATCH/MASK_VTSUB_VV` (conflitava com `encoding.h`) |
| `riscv/interactive.cc` | `socketif.h` antes de `decode_macros.h` (conserto para Boost ≥ 1.83, veja abaixo) |
| removidos | `riscv/insns/vtadd_vv.cc`, `vtsub_vv.cc` (rascunhos), `riscv-isa-sim/takum_decoder.*` (cópia velha), `teste` (executável) |

As instruções exigem `HAVE_INT128` (como o V) e o ISA `rv64gv` (ou `gcv`). Não há ainda uma *flag* de extensão `zvtakum`
no `--isa`: as instruções valem sempre que o V está habilitado.

## Compilar o Spike
```bash
sudo apt install build-essential device-tree-compiler libboost-regex-dev libboost-system-dev \
                 autoconf automake libtool gcc-riscv64-unknown-elf
mkdir -p riscv-isa-sim/build && cd riscv-isa-sim/build
../configure --prefix=$PWD/install
make -j$(nproc)          # demora (~1500 arquivos); use -j com o numero de nucleos
```

**Boost ≥ 1.83 (Ubuntu 24.04):** o Spike define a macro `yield()` em `decode_macros.h`, que colide com uma função
do Boost se o Boost for incluído depois. O conserto (1 linha) já está em `riscv/interactive.cc`. Sem ele o erro é
`macro "yield" passed 1 arguments, but takes just 0`.

## Rodar o teste das instruções
```bash
make spike-test SPIKE=riscv-isa-sim/build/spike
```
Saída esperada: `SPIKE: Zvtakum OK (22 testes de instrucao)`. O código de saída do Spike é o número de falhas.

Trilha de execução (para contar instruções dinâmicas, útil na análise de CSRs/instruções):
```bash
make -C spike_test trace SPIKE=../riscv-isa-sim/build/spike   # gera spike_test/trace.txt
grep -c "core   0: 0x" spike_test/trace.txt                     # instruções executadas
```
O programa de teste executa 1.373 instruções, das quais 22 são Zvtakum.

## Limitações
* O teste cobre `vl` completo e parcial, máscara (`vm = 0`), NaR e zero, SEW = 8/16/32 e agrupamento para o `vncvt`
  (fonte em 2 registradores). **Não** cobre LMUL > 2, `vstart ≠ 0` nem tentativas ilegais (SEW = 64); esses caminhos
  foram escritos seguindo as macros do Spike mas não são exercitados.
* O VLEN usado é o padrão do Spike (128 bits).
