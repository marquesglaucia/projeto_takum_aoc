# Projeto Takum-RVV (AOC 2026/02). Alvos principais:
#   make all           tudo que NAO precisa do Spike: test, integracao, dot, spmv   (e o alvo padrao)
#   make test          decodificacao/codificacao e + - * / contra a libtakum
#   make integracao    integracao numerica em Takum x IEEE
#   make dot           vtdot (takum_dot) contra __float128 e modelo Python exato
#   make spmv          benchmark SpMV/GEMV (Fase 3) e graficos
#   make spike-test    5 instrucoes Zvtakum no Spike      (precisa do Spike compilado: docs/SPIKE.md)
#   make analise-isa   contagem de instrucoes/CSRs        (precisa do Spike: docs/ANALISE_ISA.md)
#   make demo          demonstracao (test + dot + Spike)
#   make clean         remove executaveis e arquivos gerados
# libtakum e submodulo (usada so como oraculo de teste): git submodule update --init
CXX=g++
CXXFLAGS=-O2 -std=c++17 -Wall
LIBTAKUM?=third_party/libtakum
.DEFAULT_GOAL := all

all: test integracao dot spmv

lib/libtakum.a:
	@test -f $(LIBTAKUM)/takum.h || { echo "libtakum ausente em $(LIBTAKUM): rode 'git submodule update --init'"; exit 1; }
	mkdir -p lib && for f in $(LIBTAKUM)/src/*.c; do gcc -O2 -std=c99 -c $$f -o lib/$$(basename $$f .c).o; done && ar rcs $@ lib/*.o
test: lib/libtakum.a
	$(CXX) $(CXXFLAGS) -I$(LIBTAKUM) test_takum.cpp takum_decoder.cpp lib/libtakum.a -lm -o test_takum && ./test_takum

# Integracao numerica em takum (T8/T16/T32) x IEEE. Uso: make integracao
integracao: test_integracao.cpp takum_decoder.cpp takum_decoder.h takum_tipo.h takum_integracao.h
	$(CXX) $(CXXFLAGS) test_integracao.cpp takum_decoder.cpp -o test_integracao && ./test_integracao

# vtdot: produto escalar com acumulo ampliado. Uso: make dot   (requer libquadmath; python3 p/ o modelo)
dot: test_dot.cpp takum_decoder.cpp takum_decoder.h takum_tipo.h verifica_dot.py
	$(CXX) $(CXXFLAGS) test_dot.cpp takum_decoder.cpp -lquadmath -lm -o test_dot && ./test_dot && python3 verifica_dot.py

SPIKE ?= riscv-isa-sim/build/spike
# Teste das instrucoes Zvtakum no Spike. Uso: make spike-test SPIKE=/caminho/para/spike   (veja docs/SPIKE.md)
spike-test:
	$(MAKE) -C spike_test run SPIKE=$(abspath $(SPIKE))

# Demonstracao (15 min): testes, Spike e trilha de execucao com as instrucoes Zvtakum. Uso: make demo SPIKE=...
demo: test dot
	$(MAKE) spike-test SPIKE=$(SPIKE)
	$(MAKE) -C spike_test trace SPIKE=$(abspath $(SPIKE)) >/dev/null
	@echo "--- instrucoes Zvtakum na trilha (disassembly do Spike) ---"
	@grep -E "vt(add|sub|mul|dot)\.vv|vncvt\.t\.t" spike_test/trace.txt | head -8 | cut -c1-90
	@echo "instrucoes executadas: $$(grep -c 'core   0: 0x' spike_test/trace.txt)"

.PHONY: test integracao dot spike-test demo

# Benchmark SpMV e GEMV (Fase 3) + geracao dos graficos. Uso: make spmv
spmv: test_spmv.cpp takum_decoder.cpp takum_decoder.h takum_tipo.h avaliacao_takum.py
	$(CXX) $(CXXFLAGS) test_spmv.cpp takum_decoder.cpp -lquadmath -lm -o test_spmv && ./test_spmv && python3 avaliacao_takum.py

.PHONY: spmv

# Analise de contagem de instrucoes e CSRs (RVV/IEEE x Zvtakum). Uso: make analise-isa SPIKE=...
analise-isa:
	$(MAKE) -C analise_isa run SPIKE=$(abspath $(SPIKE))
	$(MAKE) -C analise_isa controle SPIKE=$(abspath $(SPIKE))
	$(MAKE) -C analise_isa report SPIKE=$(abspath $(SPIKE))

.PHONY: all analise-isa clean

# Remove o que o make gera (nao mexe nos graficos/figuras versionados)
clean:
	rm -rf lib test_takum test_integracao test_dot test_spmv *.csv dot_vectors.txt
	$(MAKE) -C spike_test clean
	$(MAKE) -C analise_isa clean
