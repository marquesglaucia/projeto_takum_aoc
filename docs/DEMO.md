# Demonstração ao vivo (15 min) e plano B

## Decisão sobre o plano B (modelo C++ próprio)
O Spike compilou com as 5 instruções e passa nos 22 testes (`make spike-test`). **Ponto de corte atingido: não é
necessário um modelo C++ próprio de vetores.** Como contingência, o núcleo (`takum_decoder.cpp`, `takum_dot_*`,
`dot_strips`) roda sem o Spike e já é validado por `make test` e `make dot`. Se o Spike falhar ao vivo:
1. mostrar a saída gravada de `make demo` (gravar antes; tarefa do Lucas), e
2. rodar só `make test` e `make dot` (poucos segundos, sem Spike).

## Roteiro (trecho do Rafael: ~6 min)
| Min | O que mostrar | Comando |
|---|---|---|
| 0–1 | Layout do takum e decodificação (S D R C M), 1 slide | — |
| 1–2 | Fase 1 contra a libtakum: T8 exaustivo, 12 M ops em T16/T32, 0 falhas | `make test LIBTAKUM=third_party/libtakum` |
| 2–3 | Semântica do `vtdot` e por que acumulador de 64 bits; erro mediano ~7× menor que mul+add | `make dot` |
| 3–5 | Zvtakum no Spike: encoding custom-1, disassembly, 22/22 | `make demo SPIKE=riscv-isa-sim/build/spike` |
| 5–6 | Contagem de instruções da trilha (1.373 no teste; 22 Zvtakum) e ponte para a análise de CSRs (Lucas) | `grep -c "core   0: 0x" spike_test/trace.txt` |

Antes de apresentar: Spike já compilado (`docs/SPIKE.md`, ~25 min com 2 núcleos), `make demo` rodado uma vez.
