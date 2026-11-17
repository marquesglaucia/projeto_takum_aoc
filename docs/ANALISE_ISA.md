# Análise de contagem de instruções e CSRs: RVV/IEEE × Zvtakum

Último item da Fase 3 do enunciado: *"analisar a redução na complexidade de instruções e do registro de controle e status (CSRs)"*.
Tudo o que está na seção **Medido** foi obtido executando os kernels no Spike deste repositório (com as 5 instruções Zvtakum).
O que vem só da especificação RISC-V ou do projeto, sem medida, está separado na seção **Não medido**.

Reproduzir (precisa do Spike compilado, veja `SPIKE.md`):
```bash
make analise-isa SPIKE=riscv-isa-sim/build/spike
```

## Método
* `analise_isa/kernels.S`: três kernels em assembly RV64GV, cada um em duas versões **com a mesma estrutura**
  (strip-mining, mesmos registradores, mesmo laço de ponteiros), SEW = 32:
  * **produto escalar**: `dot_f32` (RVV IEEE: `vfmul` + `vfredusum`), `dot_f32w` (RVV IEEE com acúmulo em binary64:
    `vfwmacc`, redução e `fcvt.s.d` no fim) e `dot_t32` (Zvtakum: um `vtdot.vv`);
  * **y += a·b** elemento a elemento: `fma_f32` (`vfmacc`) e `fma_t32` (`vtmul` + `vtadd`);
  * **SpMV em CSR**, laço interno de cada linha (carrega valores e índices, `vsll`, *gather* `vluxei32`, produto e soma):
    `spmv_f32` e `spmv_t32`.
* `analise_isa/main.c` roda todos os kernels no Spike (RV64GV, VLEN = 128, ou seja 4 elementos de 32 bits por registrador)
  com dados reais (N = 37, não múltiplo de 4, para exercitar o último strip; matriz 16×16 com linhas vazias, curtas e cheia).
  **A corretude é verificada antes de qualquer contagem**: os kernels Takum batem **bit a bit** com o modelo do projeto
  (`dot_strips<32>`, `takum_mul`/`takum_add`); os kernels IEEE batem com uma referência em `long double` dentro de tolerância
  (a ordem da redução `vfredusum` não é especificada, então não há resultado bit a bit único). Teste manual de controle:
  adulterando um valor esperado, o programa sai com código ≠ 0.
* `analise_isa/contagem.py` lê o ELF (`objdump`, `nm`) e a trilha do Spike (`spike -l`):
  * **estática**: instruções entre os rótulos `<kernel>_loop` e `<kernel>_end` (laço interno, por strip) e total do kernel;
  * **dinâmica**: instruções realmente executadas, por chamada, contadas pelo PC na trilha.
  O desmontador do Spike mostra `vtdot.vv`, `vtmul.vv` e `vtadd.vv` pelo nome, o que confirma que as instruções Zvtakum
  foram executadas. Os kernels são assembly escrito à mão, com o mesmo estilo nos dois lados, não saída de compilador.

## Medido

### 1. Instruções (SEW = 32, VLEN = 128, N = 37 → 10 strips; SpMV: 26 strips no total)

| kernel | laço interno (instr./strip) | vetoriais no laço | total estático | dinâmico por chamada |
|---|---|---|---|---|
| `dot_f32` (vfmul + vfredusum) | 10 | 4 | 14 | 104 |
| `dot_f32w` (vfwmacc, acúmulo em binary64) | 9 | 3 | 17 | 98 |
| **`dot_t32` (vtdot)** | **9** | **3** | **13** | **94** |
| `fma_f32` (vfmacc) | 12 | 5 | 13 | 121 |
| `fma_t32` (vtmul + vtadd) | 13 | 6 | 14 | 131 |
| `spmv_f32` | 12 | 6 | 29 | 554 |
| **`spmv_t32`** | **11** | **5** | **28** | **528** |

"Vetoriais" não conta o `vsetvli`. Verificação de consistência: `dot_f32` = 10 instr. × 10 strips + 2 (prólogo) + 2 (epílogo) = 104.

| comparação (Zvtakum vs RVV/IEEE) | laço interno | total dinâmico |
|---|---|---|
| produto escalar, `vtdot` vs `vfmul`+`vfredusum` | 10 → 9 (**−10,0%**) | 104 → 94 (**−9,6%**) |
| produto escalar, `vtdot` vs `vfwmacc` (acúmulo largo) | 9 → 9 (igual) | 98 → 94 (−4,1%) |
| SpMV, `vtdot` vs `vfmul`+`vfredusum` | 12 → 11 (**−8,3%**) | 554 → 528 (**−4,7%**) |
| y += a·b, `vtmul`+`vtadd` vs `vfmacc` | 12 → 13 (**+8,3%**, pior) | 121 → 131 (**+8,3%**, pior) |

Leitura:
* O ganho do `vtdot` é **uma instrução vetorial por strip** (ele substitui `vfmul` + `vfredusum`). As demais instruções do laço
  (carga, ajuste de ponteiros, contador, desvio) são idênticas, então o ganho relativo é pequeno (≈ 8–10% do laço).
  As instruções escalares executadas são exatamente as mesmas nos dois lados (51 e 51 no produto escalar; 324 e 324 na SpMV).
* Contra o RVV com acúmulo largo (`vfwmacc`), o laço empata. A diferença está fora do laço: o IEEE precisa de acumulador em
  grupo de 2 registradores (LMUL = 2), de um `vsetvli` extra, da redução e da conversão final (epílogo de 6 instruções contra
  2 do Takum), e o resultado é binary32 só depois do `fcvt.s.d`.
* **O Zvtakum perde onde o RVV tem uma instrução fundida:** não há `vtmacc`, então y += a·b custa 1 instrução a mais por strip
  e arredonda duas vezes (multiplicação e soma), enquanto `vfmacc` arredonda uma. Isso é uma limitação da ISA proposta e
  deve aparecer no artigo (e é um candidato a trabalho futuro: um `vtmacc.vv`).

### 2. CSRs e estado de controle

| medida | IEEE (RVV) | Zvtakum | como foi medido |
|---|---|---|---|
| instruções de CSR explícitas dentro dos kernels (`csrr`, `frflags`, ...) | 0 | 0 | trilha do Spike |
| instruções que leem/escrevem `fflags`/`frm`/`fcsr` dentro dos kernels | 0 | 0 | trilha do Spike |
| `vsetvli` por strip (escreve `vl`/`vtype`) | 1 | 1 | trilha do Spike |
| valor de `fflags` depois de rodar os kernels (zerado antes) | **`0x1`** (NX, inexato) | **`0x0`** | `frflags` no programa e `--log-commits` |
| kernels rodam com `mstatus.FS = Off` (FPU desligada) | **não rodam**: o primeiro `vfmul.vv` dá `trap_illegal_instruction` (as cargas vetoriais e o `vsetvli` passam) | **rodam** (resultado correto) | `make controle` e `make run` |

Leitura: o **código** dos kernels não muda em CSRs (os dois usam só `vsetvli`, que existe nos dois). A diferença está no
**estado implícito**: toda instrução vetorial de ponto flutuante do RVV acumula flags em `fflags` (medimos `NX` = 1 depois de
um simples produto escalar) e exige a FPU habilitada, enquanto as instruções Zvtakum não tocam em `fflags` nem exigem
`mstatus.FS` (só `mstatus.VS`, como as instruções vetoriais inteiras). Isso confirma, no simulador, o argumento do plano
(um só modo de arredondamento, sem flags de exceção, logo sem necessidade de `frm`/`fflags`).

## Não medido (vem da especificação, sem medida nossa)
* **Tamanho do estado de FP:** `fcsr` do RISC-V tem 8 bits (`frm` 3 + `fflags` 5) mais o campo `mstatus.FS` (2 bits) para
  controlar a FPU e marcar estado sujo na troca de contexto. As instruções Zvtakum não usam nenhum deles. Não medimos custo de
  troca de contexto.
* **`frm` (modo de arredondamento):** instruções FP do RVV lêem `frm` quando o modo é dinâmico; o Takum tem um único modo
  (ao mais próximo, empate para o par; `ZVTAKUM.md`). Os kernels medidos não escrevem `frm`, então não medimos custo de
  trocar de modo (`frrm`/`fsrm`) em código que precisa dele.
* **CSRs vetoriais** (`vl`, `vtype`, `vstart`, `vlenb`, `vxrm`, `vxsat`, `vcsr`) são os mesmos nos dois casos; o Zvtakum não
  cria nem remove nenhum.

## Limitações e ressalvas honestas
1. **Contagem de instruções, não de tempo.** O Spike é funcional: não há latência. Não sabemos se um `vtdot` (redução com
   acumulador largo) em hardware custa mais ciclos ou mais área do que `vfmul` + `vfredusum`; menos instruções não significa
   automaticamente mais rápido.
2. **Só SEW = 32 (T32 contra binary32).** T8 e T16 não foram medidos. O laço do `vtdot` tem a mesma forma em T8/T16/T32,
   mas não afirmamos contagens para eles.
3. **A ausência de `fflags` tem um preço.** Sem flags não há como detectar saturação nem inexatidão; só o NaR, que viaja
   com o dado. IEEE detecta estouro e operação inválida por flag. É uma troca de projeto, não só vantagem.
4. **"Sem FS" é decisão nossa, não propriedade do formato.** No nosso Spike as instruções Zvtakum exigem só o VS porque foram
   registradas como vetoriais de inteiros; seria possível exigir FS e o argumento sumiria. Convém escrever isso no
   `ZVTAKUM.md` (sugestão para o Rafael).
5. **Numérica não é o foco aqui.** `dot_t32` acumula em precisão larga e `dot_f32` em binary32, então a contagem compara
   instruções, não erro (o erro está na Seção de resultados da Glaucia). `dot_f32w` é a comparação mais justa em *intenção*
   (acumular em precisão maior), mas não é numericamente idêntica ao `vtdot`.
6. **Kernels escritos à mão**; um compilador poderia gerar laços diferentes (por exemplo, desenrolando). A comparação vale
   porque os dois lados têm a mesma estrutura.

## Texto sugerido para o artigo e os slides
* Para um laço de produto escalar/SpMV, o `vtdot.vv` substitui `vfmul.vv` + `vfredusum.vs`, economizando 1 de 10–12 instruções
  do laço interno (8–10%); as instruções escalares não mudam.
* O Zvtakum não tem multiplicação-acúmulo fundida: y += a·b custa 1 instrução a mais por strip (+8,3%) que o `vfmacc`.
* Nenhuma instrução Zvtakum lê ou escreve `frm`/`fflags` nem exige `mstatus.FS`; medimos `fflags = 0x1` após um produto
  escalar IEEE e `0x0` após os kernels Takum, e os kernels Takum rodam com a FPU desligada.
