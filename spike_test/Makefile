# Teste de integracao do Zvtakum no Spike.
#   make run SPIKE=/caminho/para/spike        (padrao: spike no PATH)
# Requer: g++, riscv64-unknown-elf-gcc (ex.: apt install gcc-riscv64-unknown-elf) e um Spike compilado
# com este repositorio (veja docs/SPIKE.md).
SPIKE ?= spike
CXX   ?= g++
RVCC  ?= riscv64-unknown-elf-gcc

test_data.h: gera_dados.cpp ../takum_decoder.cpp ../takum_decoder.h
	$(CXX) -O2 -std=c++17 gera_dados.cpp ../takum_decoder.cpp -lm -o gera_dados && ./gera_dados > $@

test_spike.elf: test_spike.c test_data.h crt0.S link.ld
	$(RVCC) -march=rv64gv -mabi=lp64d -O1 -fno-tree-vectorize -ffreestanding -nostdlib -static \
	        -mcmodel=medany -T link.ld crt0.S test_spike.c -o $@

# o codigo de saida do Spike e o numero de falhas (0 = OK)
run: test_spike.elf
	@$(SPIKE) --isa=rv64gv $<; rc=$$?; \
	 if [ $$rc -eq 0 ]; then echo "SPIKE: Zvtakum OK (22 testes de instrucao)"; else echo "SPIKE: $$rc FALHA(S)"; fi; exit $$rc

# trilha de execucao (para contar instrucoes dinamicas): trace.txt
trace: test_spike.elf
	$(SPIKE) -l --log=trace.txt --isa=rv64gv $<

clean:
	rm -f gera_dados test_data.h test_spike.elf trace.txt
.PHONY: run trace clean
