#!/usr/bin/env python3
"""Contagem de instrucoes dos kernels de kernels.S: ESTATICA (do ELF) e DINAMICA (da trilha do Spike).

  Estatica : instrucoes entre o rotulo <kernel>_loop e <kernel>_end (laco interno, por strip)
             e entre o simbolo do kernel e o fim da funcao (total estatico).
  Dinamica : instrucoes realmente executadas em cada kernel, lidas de `spike -l` (cada linha tem o PC).
Nada e estimado: tudo vem do binario e da execucao.
"""
import argparse, re, subprocess, sys
from collections import Counter

KERNELS = [  # (nome, descricao do par de comparacao)
    ("dot_f32", "dot: RVV IEEE binary32 (vfmul + vfredusum)"),
    ("dot_f32w", "dot: RVV IEEE binary32, acumulo em binary64 (vfwmacc)"),
    ("dot_t32", "dot: Zvtakum T32 (vtdot)"),
    ("fma_f32", "y+=a*b: RVV IEEE (vfmacc)"),
    ("fma_t32", "y+=a*b: Zvtakum (vtmul + vtadd)"),
    ("spmv_f32", "SpMV CSR: RVV IEEE (vfmul + vfredusum)"),
    ("spmv_t32", "SpMV CSR: Zvtakum (vtdot)"),
]
CSR_EXPL = re.compile(r"^(csr[rws]c?i?|csrr|csrw|csrs|csrc|fr(csr|rm|flags|flagsi)|fs(csr|rm|flags|rmi|flagsi)|fscsr|fsrmi)\b")
FP_CSR_NAMES = ("fflags", "frm", "fcsr")


def run(cmd):
    return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout


def symbols(nm, elf):
    syms = {}
    for line in run([nm, "-S", "-n", elf]).splitlines():
        p = line.split()
        if len(p) == 4:            # addr size type name
            syms[p[3]] = (int(p[0], 16), int(p[1], 16))
        elif len(p) == 3:          # addr type name (sem tamanho: rotulos locais)
            syms[p[2]] = (int(p[0], 16), 0)
    return syms


def disasm(objdump, elf):
    ins = {}                       # pc -> mnemonico
    for line in run([objdump, "-d", "-M", "no-aliases", elf]).splitlines():
        m = re.match(r"\s*([0-9a-f]+):\s+([0-9a-f]+)\s+(\S+)?", line)
        if m:
            ins[int(m.group(1), 16)] = (m.group(3) or ".word", int(m.group(2), 16))
    return ins


def classify(mn):
    if mn.startswith("vset"):
        return "vset (escreve vl/vtype)"
    if mn.startswith("v") or mn.startswith("."):      # ".4byte/.word/.insn" = instrucao custom do Zvtakum
        return "vetorial"
    if CSR_EXPL.match(mn):
        return "CSR explicito"
    return "escalar"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--elf", required=True); ap.add_argument("--trace", required=True)
    ap.add_argument("--objdump", default="riscv64-unknown-elf-objdump"); ap.add_argument("--nm", default="riscv64-unknown-elf-nm")
    a = ap.parse_args()
    syms = symbols(a.nm, a.elf)
    dis = disasm(a.objdump, a.elf)

    # PCs executados (formato: "core   0: 0x0000000080000104 (0x...) mnemonico ...")
    pcs = []
    mn_dyn = {}
    for line in open(a.trace):
        m = re.match(r"core\s+\d+:\s+0x([0-9a-f]+)\s+\(0x[0-9a-f]+\)\s+(\S+)", line)
        if m:
            pcs.append(int(m.group(1), 16)); mn_dyn[pcs[-1]] = m.group(2)

    # o programa chama cada kernel Takum 2x (a 2a com mstatus.FS = Off); todas as medidas dinamicas sao POR CHAMADA
    print("| kernel | laco interno (instr./strip) | vetoriais no laco | total estatico | chamadas | dinamico por chamada | strips por chamada |")
    print("|---|---|---|---|---|---|---|")
    rows = {}
    for k, desc in KERNELS:
        start, size = syms[k]
        loop, _ = syms[k + "_loop"]; end, _ = syms[k + "_end"]
        body = [dis[pc] for pc in range(loop, end, 4)]
        vec = sum(1 for mn, _ in body if classify(mn) == "vetorial")
        # vsetvli do laco = 1 por strip; vset nao entra em 'vetoriais'
        dyn = [pc for pc in pcs if start <= pc < start + size]
        # chamada = entrada em `start` vindo de fora do kernel (kernels sem prologo tem _loop == start)
        calls = sum(1 for i, pc in enumerate(pcs) if pc == start and (i == 0 or not (start <= pcs[i - 1] < start + size)))
        strips = sum(1 for pc in dyn if loop <= pc < end and mn_dyn[pc].startswith("vsetvli"))
        assert len(dyn) % calls == 0 and strips % calls == 0, "chamadas com contagens diferentes"
        rows[k] = (len(body), vec, size // 4, calls, len(dyn) // calls, strips // calls)
        print(f"| `{k}` | {len(body)} | {vec} | {size // 4} | {calls} | {len(dyn) // calls} | {strips // calls} |")

    print()
    print("Classificacao das instrucoes executadas, por chamada (dinamica):")
    print("| kernel | vetoriais | vset | escalares | CSR explicito | instrucoes que acessam fflags/frm/fcsr |")
    print("|---|---|---|---|---|---|")
    for k, _ in KERNELS:
        start, size = syms[k]
        calls = rows[k][3]
        c = Counter(classify(mn_dyn[pc]) for pc in pcs if start <= pc < start + size)
        fpcsr = sum(1 for pc in pcs if start <= pc < start + size and any(n in mn_dyn[pc] for n in FP_CSR_NAMES))
        print(f"| `{k}` | {c['vetorial'] // calls} | {c['vset (escreve vl/vtype)'] // calls} | {c['escalar'] // calls} | {c['CSR explicito'] // calls} | {fpcsr} |")

    print()
    print("Mnemonicos executados dentro do laco de cada kernel (1a iteracao), como o Spike os desmonta:")
    for k, _ in KERNELS:
        loop, _ = syms[k + "_loop"]; end, _ = syms[k + "_end"]
        seq = []
        for pc in pcs:
            if loop <= pc < end:
                if pc == loop and seq:
                    break
                seq.append(mn_dyn[pc])
        print(f"- `{k}`: " + ", ".join(seq))


if __name__ == "__main__":
    sys.exit(main())
