# Zvtakum: especificação das instruções (versão do simulador)

Extensão vetorial Takum para o RVV. Operandos em formato **Takum linear** com largura = **SEW** (T8, T16, T32).
SEW = 64 é ilegal (não há T64 no projeto). Todas as instruções seguem as regras do RVV para `vl`, `vstart`, máscara (`vm`)
e agrupamento de registradores (LMUL), e usam o mesmo estado `vtype`/`vl` das instruções vetoriais padrão.

## Codificação

Opcode **custom-1 (`0x2b`)**, campos no formato do OP-V; `funct3 = 000` (forma `.vv`).

```
31      26 25  24   20 19   15 14  12 11   7 6      0
| funct6 | vm | vs2   | vs1   | 000  | vd   | 0101011 |
```

| Instrução | funct6 | vs1 | `MATCH` | `MASK` |
|---|---|---|---|---|
| `vtadd.vv  vd, vs2, vs1, vm` | `000000` | registrador | `0x0000002b` | `0xfc00707f` |
| `vtsub.vv  vd, vs2, vs1, vm` | `000001` | registrador | `0x0400002b` | `0xfc00707f` |
| `vtmul.vv  vd, vs2, vs1, vm` | `000010` | registrador | `0x0800002b` | `0xfc00707f` |
| `vtdot.vv  vd, vs2, vs1, vm` | `000011` | registrador | `0x0c00002b` | `0xfc00707f` |
| `vncvt.t.t vd, vs2, vm`      | `000100` | `00000`     | `0x1000002b` | `0xfc0ff07f` |

**Por que custom-1 e não um slot reservado do OP-V:** os encodings reservados do espaço padrão pertencem à RISC-V International;
uma extensão não padrão deve usar os espaços *custom*. (Limitação: o RoCC do Spike também usa `0x2b`, mas só quando
carregado com `--extension`; não use os dois juntos.) Um rascunho anterior usava `MATCH_VTSUB_VV = 0x84001057` (OPFVV,
`funct6 = 100001`) em `riscv/opcodes.h`; ele nunca foi registrado e foi removido.

## Semântica

| Instrução | Operação |
|---|---|
| `vtadd.vv`  | `vd[i] = vs2[i] + vs1[i]` |
| `vtsub.vv`  | `vd[i] = vs2[i] - vs1[i]` (mesma ordem do `vfsub.vv`) |
| `vtmul.vv`  | `vd[i] = vs2[i] * vs1[i]` |
| `vtdot.vv`  | `vd[0] = RN( vd[0] + Σ_{i<vl, ativo} vs2[i]·vs1[i] )` |
| `vncvt.t.t` | `vd[i] = RN_SEW( vs2[i] )`, com `vs2` de largura `2·SEW`: SEW=8 → T16→T8; SEW=16 → T32→T16 |

* **Arredondamento:** ao mais próximo, empate para o par, sobre a cadeia de bits (D R C M). Um único modo, sem registrador
  de modo de arredondamento (`frm`) e sem flags de exceção (`fflags`).
* **Saturação:** resultado fora da faixa vira o maior/menor valor representável não nulo; nunca vira NaR nem 0 por estouro.
* **Especiais:** zero = `00…0`, NaR = `10…0`. NaR em qualquer operando produz NaR (inclusive `0 × NaR`).

### `vtdot.vv` em detalhe

* `vd` é **acumulador**, como no `vmacc`: lido e escrito. Só o elemento 0 é lido/escrito; o resultado é escalar.
* Cada produto é **exato** (mantissa do produto ≤ 56 bits).
* O acumulador tem **mantissa de 64 bits**, com arredondamento ao mais próximo-par a cada soma (a "precisão ampliada",
  mais de 2× a maior mantissa do T32, que tem 27 bits de fração).
* **Um único arredondamento final** para SEW bits.
* Como as reduções do RVV: exige `vstart = 0`; se `vl = 0`, `vd` não é escrito; elementos mascarados não contribuem.
* Em *strip-mining*, cada `vtdot` grava o parcial em `vd[0]` **arredondado para SEW bits**; veja `dot_strips<N>` em
  `takum_tipo.h`, que modela exatamente isso para o benchmark.

**Limite conhecido (acumulador finito):** se houver cancelamento de mais de ~64 bits entre os termos, o resíduo se perde.
Isso só aparece com dados de faixa dinâmica extrema (padrões de bits aleatórios em T8, com expoentes de ±255). Com dados
realistas, o resultado coincide com a referência de 113 bits (`__float128`) em 100% dos casos testados.

**Alternativas consideradas:** (A) destino alargado (T8→T16, T16→T32): exigiria grupos de registradores de destino e um
T64 inexistente no projeto; (B) acumulador exato tipo *quire* (~1,2 kbit): mais fiel, mas custo de hardware muito maior.
Escolhemos destino de largura SEW com acumulador interno largo porque é uniforme para T8/T16/T32 e mede-se no
experimento quanto o acúmulo ampliado melhora o erro (mediana ~7× menor que mul+add encadeados, T8/T16/T32).

## Como é validado

| Camada | Teste | Referência |
|---|---|---|
| Decodificação/codificação, `+ − ×` | `make test LIBTAKUM=third_party/libtakum` | libtakum (oficial) |
| `takum_dot` | `make dot` | `__float128` + modelo Python com racionais exatos (bit a bit) |
| Instruções no Spike | `make spike-test SPIKE=...` | resultados esperados da biblioteca do projeto |
