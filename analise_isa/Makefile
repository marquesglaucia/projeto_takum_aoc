# Analise de contagem de instrucoes e CSRs: RVV/IEEE x Zvtakum (docs/ANALISE_ISA.md).
#   make run    SPIKE=/caminho/para/spike    -> verifica os kernels e as medidas de CSR (codigo de saida 0 = OK)
#   make report SPIKE=/caminho/para/spike    -> tabelas de contagem estatica e dinamica
#   make controle SPIKE=/caminho/para/spike  -> controle negativo: kernel IEEE nao roda com FS=Off
# Requer: g++, riscv64-unknown-elf-gcc/objdump/nm, python3 e um Spike compilado com este repositorio (docs/SPIKE.md).
SPIKE ?= spike
CXX   ?= g++
RVCC  ?= riscv64-unknown-elf-gcc
OBJDUMP ?= riscv64-unknown-elf-objdump
NM    ?= riscv64-unknown-elf-nm
BOOT  = ../spike_test

test_data_analise.h: gera_dados_analise.cpp ../takum_decoder.cpp ../takum_decoder.h ../takum_tipo.h
	$(CXX) -O2 -std=c++17 gera_dados_analise.cpp ../takum_decoder.cpp -lm -o gera_dados_analise && ./gera_dados_analise > $@

# sem compressao (-march=rv64gv): toda instrucao tem 4 bytes, o que simplifica a contagem por endereco
analise.elf: main.c kernels.S test_data_analise.h $(BOOT)/crt0.S $(BOOT)/link.ld
	$(RVCC) -march=rv64gv -mabi=lp64d -O1 -fno-tree-vectorize -ffreestanding -nostdlib -static \
	        -mcmodel=medany -T $(BOOT)/link.ld $(BOOT)/crt0.S main.c kernels.S -o $@

# o codigo de saida do Spike e o numero de verificacoes que falharam
run: analise.elf
	@$(SPIKE) --isa=rv64gv $<; rc=$$?; \
	 if [ $$rc -eq 0 ]; then echo "ANALISE ISA: kernels corretos; fflags: IEEE != 0, Takum == 0; Takum roda com FS=Off"; \
	 else echo "ANALISE ISA: $$rc FALHA(S)"; fi; exit $$rc

# controle negativo: kernel IEEE com FS=Off nao pode terminar (excecao de instrucao ilegal, sem tratador => timeout)
controle_fs_off.elf: controle_fs_off.c kernels.S $(BOOT)/crt0.S $(BOOT)/link.ld
	$(RVCC) -march=rv64gv -mabi=lp64d -O1 -fno-tree-vectorize -ffreestanding -nostdlib -static \
	        -mcmodel=medany -T $(BOOT)/link.ld $(BOOT)/crt0.S controle_fs_off.c kernels.S -o $@

controle: controle_fs_off.elf
	@if timeout 20 $(SPIKE) --isa=rv64gv $< >/dev/null 2>&1; then \
	   echo "CONTROLE FALHOU: o kernel IEEE terminou com FS=Off"; exit 1; \
	 else echo "CONTROLE OK: o kernel IEEE nao roda com FS=Off (os kernels Takum rodam: make run)"; fi

trace.txt: analise.elf
	$(SPIKE) -l --log=$@ --isa=rv64gv $<

report: analise.elf trace.txt
	python3 contagem.py --elf analise.elf --trace trace.txt --objdump $(OBJDUMP) --nm $(NM)

clean:
	rm -f gera_dados_analise test_data_analise.h analise.elf controle_fs_off.elf trace.txt
.PHONY: run report controle clean
