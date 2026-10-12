# Decisões de projeto

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
