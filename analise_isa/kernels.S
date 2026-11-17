# Kernels de comparacao para a analise de instrucoes/CSRs (RV64GV, sem compressao, VLEN-agnostico).
#
# Cada kernel existe em duas versoes com a MESMA estrutura (strip-mining, mesmos registradores):
#   *_f32 : RVV padrao com IEEE 754 binary32
#   *_t32 : Zvtakum com T32 (instrucoes custom-1, veja docs/ZVTAKUM.md)
# Os rotulos *_loop e *_end delimitam o laco interno: a analise estatica conta as instrucoes entre eles
# (analise_isa/contagem.py). Nao renomeie esses rotulos.
#
# Convencao de chamada (ABI padrao): a0..a7 = argumentos inteiros; resultado float em fa0, inteiro em a0.

# ---- Zvtakum: o assembler GNU nao conhece as instrucoes; montamos a palavra a partir de docs/ZVTAKUM.md
#   funct6[31:26] vm[25]=1 (sem mascara) vs2[24:20] vs1[19:15] funct3=000 vd[11:7] opcode=0x2b (custom-1)
        .macro vtadd_vv vd, vs2, vs1
        .word 0x0000002b | (1<<25) | (\vs2<<20) | (\vs1<<15) | (\vd<<7)
        .endm
        .macro vtmul_vv vd, vs2, vs1
        .word 0x0800002b | (1<<25) | (\vs2<<20) | (\vs1<<15) | (\vd<<7)
        .endm
        .macro vtdot_vv vd, vs2, vs1
        .word 0x0c00002b | (1<<25) | (\vs2<<20) | (\vs1<<15) | (\vd<<7)
        .endm

        .text

# =====================================================================================================
# 1) Produto escalar   r = sum x[i]*y[i]         a0 = n, a1 = x, a2 = y
# =====================================================================================================

# --- IEEE binary32: vfmul + reducao nao ordenada (acumulador escalar em v24[0], em binary32)
        .globl dot_f32
dot_f32:
        vsetivli zero, 1, e32, m1, ta, ma
        vmv.v.i  v24, 0
dot_f32_loop:
        vsetvli  t0, a0, e32, m1, ta, ma
        vle32.v  v8, (a1)
        vle32.v  v16, (a2)
        vfmul.vv v8, v8, v16
        vfredusum.vs v24, v8, v24
        slli     t1, t0, 2
        add      a1, a1, t1
        add      a2, a2, t1
        sub      a0, a0, t0
        bnez     a0, dot_f32_loop
dot_f32_end:
        vfmv.f.s fa0, v24
        ret
        .size dot_f32, .-dot_f32

# --- IEEE binary32 com acumulo em binary64 (equivalente em INTENCAO ao vtdot: acumular em precisao maior).
#     vfwmacc acumula em vetor de binary64 (grupo de 2 registradores); a reducao e a volta a binary32 ficam fora do laco.
        .globl dot_f32w
dot_f32w:
        vsetvli  t2, zero, e64, m2, ta, ma
        vmv.v.i  v24, 0
dot_f32w_loop:
        vsetvli  t0, a0, e32, m1, tu, ma
        vle32.v  v8, (a1)
        vle32.v  v16, (a2)
        vfwmacc.vv v24, v8, v16
        slli     t1, t0, 2
        add      a1, a1, t1
        add      a2, a2, t1
        sub      a0, a0, t0
        bnez     a0, dot_f32w_loop
dot_f32w_end:
        vsetvli  t2, zero, e64, m2, ta, ma
        vmv.v.i  v28, 0
        vfredusum.vs v28, v24, v28
        vfmv.f.s ft0, v28
        fcvt.s.d fa0, ft0
        ret
        .size dot_f32w, .-dot_f32w

# --- Zvtakum T32: um unico vtdot.vv (acumulador largo interno, arredondamento unico por strip)
        .globl dot_t32
dot_t32:
        vsetivli zero, 1, e32, m1, ta, ma
        vmv.v.i  v24, 0                  # o zero do takum e o padrao 00..0
dot_t32_loop:
        vsetvli  t0, a0, e32, m1, ta, ma
        vle32.v  v8, (a1)
        vle32.v  v16, (a2)
        vtdot_vv 24, 16, 8
        slli     t1, t0, 2
        add      a1, a1, t1
        add      a2, a2, t1
        sub      a0, a0, t0
        bnez     a0, dot_t32_loop
dot_t32_end:
        vmv.x.s  a0, v24
        ret
        .size dot_t32, .-dot_t32

# =====================================================================================================
# 2) Multiplicacao-acumulacao elemento a elemento   y[i] += a[i]*b[i]     a0 = n, a1 = a, a2 = b, a3 = y
#    (o RVV tem vfmacc fundido; o Zvtakum NAO tem vtmacc, entao precisa de vtmul + vtadd)
# =====================================================================================================
        .globl fma_f32
fma_f32:
fma_f32_loop:
        vsetvli  t0, a0, e32, m1, ta, ma
        vle32.v  v8, (a1)
        vle32.v  v16, (a2)
        vle32.v  v24, (a3)
        vfmacc.vv v24, v8, v16
        vse32.v  v24, (a3)
        slli     t1, t0, 2
        add      a1, a1, t1
        add      a2, a2, t1
        add      a3, a3, t1
        sub      a0, a0, t0
        bnez     a0, fma_f32_loop
fma_f32_end:
        ret
        .size fma_f32, .-fma_f32

        .globl fma_t32
fma_t32:
fma_t32_loop:
        vsetvli  t0, a0, e32, m1, ta, ma
        vle32.v  v8, (a1)
        vle32.v  v16, (a2)
        vle32.v  v24, (a3)
        vtmul_vv 8, 8, 16
        vtadd_vv 24, 24, 8
        vse32.v  v24, (a3)
        slli     t1, t0, 2
        add      a1, a1, t1
        add      a2, a2, t1
        add      a3, a3, t1
        sub      a0, a0, t0
        bnez     a0, fma_t32_loop
fma_t32_end:
        ret
        .size fma_t32, .-fma_t32

# =====================================================================================================
# 3) SpMV em CSR (y = A x), laco interno de cada linha: carrega valores e indices, gather de x, produto, soma
#    a0 = nlinhas, a1 = rowptr (u32), a2 = col (u32), a3 = val, a4 = x, a5 = y
# =====================================================================================================
        .globl spmv_f32
spmv_f32:
        beqz     a0, spmv_f32_ret
spmv_f32_row:
        lw       t3, 0(a1)
        lw       t4, 4(a1)
        sub      t5, t4, t3              # nnz da linha
        slli     t3, t3, 2
        add      a6, a2, t3              # ponteiro de colunas
        add      a7, a3, t3              # ponteiro de valores
        vsetivli zero, 1, e32, m1, ta, ma
        vmv.v.i  v24, 0
        beqz     t5, spmv_f32_store      # linha vazia
spmv_f32_loop:
        vsetvli  t0, t5, e32, m1, ta, ma
        vle32.v  v8, (a7)                # valores
        vle32.v  v16, (a6)               # indices de coluna
        vsll.vi  v16, v16, 2             # indice -> deslocamento em bytes
        vluxei32.v v20, (a4), v16        # gather de x
        vfmul.vv v8, v8, v20
        vfredusum.vs v24, v8, v24
        slli     t1, t0, 2
        add      a6, a6, t1
        add      a7, a7, t1
        sub      t5, t5, t0
        bnez     t5, spmv_f32_loop
spmv_f32_end:
spmv_f32_store:
        vfmv.f.s ft0, v24
        fsw      ft0, 0(a5)
        addi     a5, a5, 4
        addi     a1, a1, 4
        addi     a0, a0, -1
        bnez     a0, spmv_f32_row
spmv_f32_ret:
        ret
        .size spmv_f32, .-spmv_f32

        .globl spmv_t32
spmv_t32:
        beqz     a0, spmv_t32_ret
spmv_t32_row:
        lw       t3, 0(a1)
        lw       t4, 4(a1)
        sub      t5, t4, t3
        slli     t3, t3, 2
        add      a6, a2, t3
        add      a7, a3, t3
        vsetivli zero, 1, e32, m1, ta, ma
        vmv.v.i  v24, 0
        beqz     t5, spmv_t32_store
spmv_t32_loop:
        vsetvli  t0, t5, e32, m1, ta, ma
        vle32.v  v8, (a7)
        vle32.v  v16, (a6)
        vsll.vi  v16, v16, 2
        vluxei32.v v20, (a4), v16
        vtdot_vv 24, 8, 20               # substitui vfmul + vfredusum
        slli     t1, t0, 2
        add      a6, a6, t1
        add      a7, a7, t1
        sub      t5, t5, t0
        bnez     t5, spmv_t32_loop
spmv_t32_end:
spmv_t32_store:
        vmv.x.s  t1, v24
        sw       t1, 0(a5)
        addi     a5, a5, 4
        addi     a1, a1, 4
        addi     a0, a0, -1
        bnez     a0, spmv_t32_row
spmv_t32_ret:
        ret
        .size spmv_t32, .-spmv_t32
