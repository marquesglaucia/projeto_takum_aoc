# Decisões de projeto

## Formato e núcleo aritmético (Fase 1)
- **Simulador funcional em C++, não RTL.** *Alternativa:* VHDL/Verilog. O enunciado aceita os dois; o simulador
  permite validar milhões de casos contra uma referência e reaproveitar o mesmo código no Spike.
- **Takum linear, não logarítmico.** *Alternativa:* takum logarítmico (multiplicação vira soma). As instruções do
  Zvtakum substituem `vfadd`/`vfmul` (ponto flutuante), cujo equivalente direto é o takum linear
  (`takum8/16/32` na libtakum).
- **Layout** (n bits): `S | D | R(3) | C(r bits) | M(p bits)`, com `r = D ? R : 7-R` e `p = n-5-r`.
  O regime tem **3 bits fixos** (não é uma sequência de bits iguais como nos posits).
  Em T8 a característica pode não caber nos bits restantes: fica truncada e é completada com zeros.
- **Uma implementação genérica em `n` bits** com *wrappers* T8/T16/T32, em vez de três cópias. A aritmética
  opera sobre padrões de bits; isso torna `vncvt` (conversão entre larguras) quase gratuito (`takum_cvt`).
- **Duas camadas na decodificação:** `TakumFormat` guarda os campos (S, D, R, C, M) e os derivados (`r`, `c`, `p`)
  — é a extração da Fase 1; a aritmética usa uma forma interna `(sinal, expoente, mantissa 1.f)` de 64 bits.
- **Precisão intermediária larga:** multiplicação exata em 128 bits; a soma alinha expoentes em 128 bits com
  62 bits de guarda e bit *sticky*; a subtração normaliza pelo bit mais significativo após o cancelamento.
  O resultado sai corretamente arredondado, sem truques por formato. Em hardware a largura seria menor; achar
  a largura mínima é ponto de discussão no artigo.
- **Arredondamento:** ao mais próximo, empate para o par, sobre a cadeia de bits `(D R C M)`. *Alternativa:*
  arredondar o valor linear. Escolhido porque é o que a libtakum faz (arredonda em `l = c + f`) e assim o
  resultado coincide bit a bit com a referência.
- **Saturação**, nunca overflow para NaR nem underflow para 0.
- **Especiais:** um único zero (`00…0`) e um único NaR (`10…0`); sem infinitos nem NaNs com *payload*.
  NaR se propaga em todas as operações. Consequência de projeto: sem modo de arredondamento configurável e sem
  flags de exceção (veja a análise de CSRs).

## Extensão de ISA Zvtakum (Fase 2)
Especificação completa (codificação, semântica, limites e alternativas): [`ZVTAKUM.md`](ZVTAKUM.md).
- **Codificação no espaço custom-1 (`0x2b`)**, não em slots reservados do OP-V (que pertencem à RISC-V International).
  *Custo:* convive mal com o RoCC do Spike (usa o mesmo opcode), que só é ativo com `--extension`.
- **`vtsub` calcula `vs2 - vs1`**, a mesma ordem de `vfsub.vv` e `vsub.vv`. (O rascunho anterior calculava `vs1 - vs2`.)
- **`vtdot.vv`:** `vd` é acumulador (convenção do `vmacc`), resultado escalar em `vd[0]`; produtos exatos;
  acumulador de **mantissa de 64 bits** (mais de 2× a do T32); **um só arredondamento final** para SEW bits.
  *Alternativas:* destino alargado (T8→T16, T16→T32; exigiria um T64 inexistente no projeto) e acumulador exato
  tipo *quire* (~1,2 kbit; custo de hardware muito maior). *Custo da escolha:* cancelamento de mais de ~64 bits
  perde o resíduo, o que só ocorre com dados de faixa dinâmica extrema.
- **Strip-mining:** cada `vtdot` grava o parcial arredondado para SEW bits em `vd[0]`; `dot_strips<N>`
  modela isso para que o benchmark reflita a ISA.
- **`vncvt.t.t`:** SEW=8 → T16→T8, SEW=16 → T32→T16; SEW=32 é ilegal (não há T64).
- **Integração no Spike com fonte única:** o Spike compila `../takum_decoder.cpp` por uma ponte
  (`riscv/takum_glue.{h,cc}`); não há segunda cópia do decodificador.

## Validação
- `make test` compara decodificação, codificação, `+ − × ÷` com a **libtakum** (T8 exaustivo; milhões de casos em T16/T32).
- `make dot` valida o `takum_dot`: contra `__float128` (0 divergências em dados realistas) e **bit a bit** contra um
  modelo independente em Python com racionais exatos (`verifica_dot.py`).
- `make spike-test` roda um programa bare-metal no Spike que executa as 5 instruções e compara com os resultados
  esperados da biblioteca (testa decodificação, ordem de operandos, máscara, `vl`, SEW e grupos de registradores).
  Um controle negativo (valores esperados adulterados) faz o teste falhar, como deve.

## Divisão (`takum_div`, `takum_divisao_T8/T16/T32`)
- **Arredondamento:** ao mais próximo, empate para par. Usa a mesma rotina de codificação (`pack`)
  da soma e da multiplicação; o resto da divisão inteira das mantissas entra como bit *sticky*.
- **Valores especiais:** `x / 0 = NaR`; `NaR / x = x / NaR = NaR`; `0 / x = 0` (x diferente de 0 e de NaR).
- **Saturação:** como nas demais operações, nunca há overflow para NaR nem underflow para 0;
  o resultado fica no maior (ou no menor positivo) takum representável.
- **Validação:** idêntica à libtakum (`takum8/16/32_division`) em `test_takum.cpp`:
  T8 exaustivo (65.536 pares) e 3 milhões de pares aleatórios em T16 e em T32.

## Negação (`takum_neg`)
- Complemento de dois do padrão de bits (operação exata); 0 e NaR são pontos fixos.

## Integração numérica (`takum_tipo.h`, `takum_integracao.h`)
- **Templates (só cabeçalho):** o mesmo algoritmo roda em T8/T16/T32, float, double e long double,
  o que permite comparar o erro de cada formato com o mesmo código.
- Toda a aritmética do método (nós, pesos, somas) é feita no formato testado; `long double` é usado
  só nos critérios de parada (Romberg e Simpson adaptativo), onde a comparação é exata.
- Os nós são calculados como `a + i*h`, e não somando `h` repetidamente, para não acumular erro.
- **Funções elementares** (sin, exp, sqrt, log, ...) em takum: calculadas em `long double` e
  arredondadas uma única vez para takum. A libtakum não é usada aqui porque, neste projeto,
  ela serve apenas como oráculo de teste.
- **Soma de Kahan** opcional: em T16 a soma ingênua estagna quando há muitos termos
  (trapézio de e^x com n = 100.000: erro de 99% com soma ingênua e 0,34% com Kahan).
