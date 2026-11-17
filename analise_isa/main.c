// Programa bare-metal (Spike, RV64GV, VLEN=128) que executa os kernels de kernels.S em IEEE e em Zvtakum.
// Objetivos: (1) provar que os kernels estao corretos (senao a contagem de instrucoes nao significa nada);
//            (2) MEDIR o comportamento de CSRs: fflags apos os kernels IEEE e apos os kernels Takum;
//            (3) rodar os kernels Takum com mstatus.FS = Off (FPU desligada).
// Codigo de saida = numero de verificacoes que falharam (0 = tudo OK).
#include <stdint.h>
#include "test_data_analise.h"

float    dot_f32(long n, const float* x, const float* y);
float    dot_f32w(long n, const float* x, const float* y);
uint32_t dot_t32(long n, const uint32_t* x, const uint32_t* y);
void     fma_f32(long n, const float* a, const float* b, float* y);
void     fma_t32(long n, const uint32_t* a, const uint32_t* b, uint32_t* y);
void     spmv_f32(long nr, const uint32_t* rp, const uint32_t* col, const float* v, const float* x, float* y);
void     spmv_t32(long nr, const uint32_t* rp, const uint32_t* col, const uint32_t* v, const uint32_t* x, uint32_t* y);

static double ab(double v) { return v < 0 ? -v : v; }
#define RD_FFLAGS() ({ unsigned long r_; __asm__ volatile("csrr %0, fflags" : "=r"(r_)); r_; })
#define CLR_FFLAGS() __asm__ volatile("csrw fflags, zero")
#define FS_MASK 0x6000UL

// buffers de trabalho (em .bss; main nao usa libc)
static float    yf[N];
static uint32_t yt[N];
static float    sy[NROWS];
static uint32_t st[NROWS];

// ---------- IEEE (usa a FPU) ----------
static int run_ieee(void) {
    int bad = 0;
    float r = dot_f32(N, XF, YF);
    bad += ab((double)r - DOT_REF) > 1e-5 * DOT_ABS;
    r = dot_f32w(N, XF, YF);
    bad += ab((double)r - DOT_REF) > 1e-6 * DOT_ABS;
    for (int i = 0; i < N; i++) yf[i] = Y0F[i];
    fma_f32(N, AF, BF, yf);
    for (int i = 0; i < N; i++) bad += (yf[i] != FMA_F[i]);
    spmv_f32(NROWS, ROWPTR, COL, VALF, XVF, sy);
    for (int i = 0; i < NROWS; i++) bad += ab((double)sy[i] - SPMV_REF[i]) > 1e-5 * SPMV_ABS[i];
    return bad;
}

// ---------- Zvtakum (nenhuma instrucao de ponto flutuante) ----------
static int run_takum(void) {
    int bad = 0;
    bad += dot_t32(N, XT, YT) != DOT_T32;
    for (int i = 0; i < N; i++) yt[i] = Y0T[i];
    fma_t32(N, AT, BT, yt);
    for (int i = 0; i < N; i++) bad += (yt[i] != FMA_T[i]);
    spmv_t32(NROWS, ROWPTR, COL, VALT, XVT, st);
    for (int i = 0; i < NROWS; i++) bad += (st[i] != SPMV_T32[i]);
    return bad;
}

int main(void) {
    int fails = 0;
    // 1) Takum primeiro, com fflags zerado: nenhum flag pode aparecer
    CLR_FFLAGS();
    fails += run_takum();
    fails += (RD_FFLAGS() != 0);                 // MEDIDO: Takum nao toca em fflags
    // 2) IEEE: corretude e flags
    CLR_FFLAGS();
    fails += run_ieee();
    fails += (RD_FFLAGS() == 0);                 // MEDIDO: IEEE acumula flags (ex.: NX/inexato) -> deve ser != 0
    // 3) Takum com a FPU desligada (mstatus.FS = Off): se algum kernel precisasse da FPU, o Spike levantaria
    //    excecao de instrucao ilegal e este programa nao terminaria com codigo 0.
    __asm__ volatile("csrc mstatus, %0" ::"r"(FS_MASK));
    fails += run_takum();
    __asm__ volatile("csrs mstatus, %0" ::"r"(FS_MASK));
    return fails > 100 ? 100 : fails;
}
