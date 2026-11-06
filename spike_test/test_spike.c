// Teste de integracao do Zvtakum no Spike (bare-metal, RV64GV, VLEN=128).
// Executa vtadd/vtsub/vtmul/vtdot/vncvt.t.t (codificacao custom-1) e compara com os resultados
// esperados gerados pela biblioteca Takum do projeto (test_data.h).
// Saida: codigo de saida do Spike = numero de falhas (0 = tudo OK).
#include <stdint.h>
#include "test_data.h"

// funct6[31:26] vm[25] vs2[24:20] vs1[19:15] funct3=000 vd[11:7] opcode=0x2b (custom-1)
#define ENC(f6, vm, vs2, vs1, vd) (((f6) << 26) | ((vm) << 25) | ((vs2) << 20) | ((vs1) << 15) | ((vd) << 7) | 0x2b)
#define TK(f6, vm, vs2, vs1, vd) __asm__ volatile(".word %0" ::"i"(ENC(f6, vm, vs2, vs1, vd)) : "memory")
enum { F_ADD = 0, F_SUB = 1, F_MUL = 2, F_DOT = 3, F_CVT = 4 };

// binarias elemento a elemento: v16 = a, v24 = b, v8 = a op b (vs2 = a, vs1 = b => vtsub = a - b)
#define BIN(NAME, SEW, VLE, VSE, T, F6, N, EXP)                                      \
  static int NAME(void) {                                                             \
    T out[N]; long vl;                                                                \
    __asm__ volatile("vsetvli %0, %1, e" #SEW ", m1, ta, ma" : "=r"(vl) : "r"(N));    \
    __asm__ volatile(VLE " v16, (%0)" ::"r"(A##SEW) : "memory");                      \
    __asm__ volatile(VLE " v24, (%0)" ::"r"(B##SEW) : "memory");                      \
    TK(F6, 1, 16, 24, 8);                                                             \
    __asm__ volatile(VSE " v8, (%0)" ::"r"(out) : "memory");                          \
    int bad = (vl != N);                                                              \
    for (int i = 0; i < N; i++) bad += (out[i] != EXP[i]);                            \
    return bad;                                                                       \
  }
BIN(add8,  8,  "vle8.v",  "vse8.v",  uint8_t,  F_ADD, 16, ADD8)
BIN(sub8,  8,  "vle8.v",  "vse8.v",  uint8_t,  F_SUB, 16, SUB8)
BIN(mul8,  8,  "vle8.v",  "vse8.v",  uint8_t,  F_MUL, 16, MUL8)
BIN(add16, 16, "vle16.v", "vse16.v", uint16_t, F_ADD, 8,  ADD16)
BIN(sub16, 16, "vle16.v", "vse16.v", uint16_t, F_SUB, 8,  SUB16)
BIN(mul16, 16, "vle16.v", "vse16.v", uint16_t, F_MUL, 8,  MUL16)
BIN(add32, 32, "vle32.v", "vse32.v", uint32_t, F_ADD, 4,  ADD32)
BIN(sub32, 32, "vle32.v", "vse32.v", uint32_t, F_SUB, 4,  SUB32)
BIN(mul32, 32, "vle32.v", "vse32.v", uint32_t, F_MUL, 4,  MUL32)

// vtdot: v8[0] = init, v16 = a, v24 = b ; resultado em v8[0]. vl = N (completo) ou VLP (parcial)
#define DOT(NAME, SEW, VLE, VSE, T, N, VL, A, B, I, EXPECT)                           \
  static int NAME(void) {                                                             \
    T out[N]; long vl;                                                                \
    __asm__ volatile("vsetvli %0, %1, e" #SEW ", m1, ta, ma" : "=r"(vl) : "r"(VL));   \
    __asm__ volatile(VLE " v8,  (%0)" ::"r"(I) : "memory");                           \
    __asm__ volatile(VLE " v16, (%0)" ::"r"(A) : "memory");                           \
    __asm__ volatile(VLE " v24, (%0)" ::"r"(B) : "memory");                           \
    TK(F_DOT, 1, 16, 24, 8);                                                          \
    __asm__ volatile(VSE " v8, (%0)" ::"r"(out) : "memory");                          \
    return (vl != (VL)) + (out[0] != (EXPECT));                                       \
  }
DOT(dotf8,  8,  "vle8.v",  "vse8.v",  uint8_t,  16, 16,    DA8,  DB8,  DI8,  DOTF8)
DOT(dotp8,  8,  "vle8.v",  "vse8.v",  uint8_t,  16, VLP8,  DA8,  DB8,  DI8,  DOTP8)
DOT(dotn8,  8,  "vle8.v",  "vse8.v",  uint8_t,  16, 16,    DN8,  DB8,  DI8,  DOTNAR8)
DOT(dotf16, 16, "vle16.v", "vse16.v", uint16_t, 8,  8,     DA16, DB16, DI16, DOTF16)
DOT(dotp16, 16, "vle16.v", "vse16.v", uint16_t, 8,  VLP16, DA16, DB16, DI16, DOTP16)
DOT(dotn16, 16, "vle16.v", "vse16.v", uint16_t, 8,  8,     DN16, DB16, DI16, DOTNAR16)
DOT(dotf32, 32, "vle32.v", "vse32.v", uint32_t, 4,  4,     DA32, DB32, DI32, DOTF32)
DOT(dotp32, 32, "vle32.v", "vse32.v", uint32_t, 4,  VLP32, DA32, DB32, DI32, DOTP32)
DOT(dotn32, 32, "vle32.v", "vse32.v", uint32_t, 4,  4,     DN32, DB32, DI32, DOTNAR32)

// mascarado (e16, tu/mu): v0 = mascara, v8 = sentinela; inativos preservados
static int masked_add16(void) {
  uint16_t out[8]; long vl;
  __asm__ volatile("vsetvli %0, %1, e16, m1, tu, mu" : "=r"(vl) : "r"(8));
  __asm__ volatile("vlm.v v0, (%0)" ::"r"(&MASK16) : "memory");
  __asm__ volatile("vle16.v v8,  (%0)" ::"r"(MS) : "memory");
  __asm__ volatile("vle16.v v16, (%0)" ::"r"(MA) : "memory");
  __asm__ volatile("vle16.v v24, (%0)" ::"r"(MB) : "memory");
  TK(F_ADD, 0, 16, 24, 8);                       // vm = 0  => executa so onde v0 = 1
  __asm__ volatile("vse16.v v8, (%0)" ::"r"(out) : "memory");
  int bad = 0; for (int i = 0; i < 8; i++) bad += (out[i] != MEXP[i]);
  return bad;
}
static int masked_dot16(void) {
  uint16_t out[8]; long vl;
  __asm__ volatile("vsetvli %0, %1, e16, m1, tu, mu" : "=r"(vl) : "r"(8));
  __asm__ volatile("vlm.v v0, (%0)" ::"r"(&MASK16) : "memory");
  __asm__ volatile("vle16.v v8,  (%0)" ::"r"(MDI) : "memory");
  __asm__ volatile("vle16.v v16, (%0)" ::"r"(MA) : "memory");
  __asm__ volatile("vle16.v v24, (%0)" ::"r"(MB) : "memory");
  TK(F_DOT, 0, 16, 24, 8);
  __asm__ volatile("vse16.v v8, (%0)" ::"r"(out) : "memory");
  return out[0] != MDOT;
}

// vncvt.t.t: fonte com EEW = 2*SEW em grupo de 2 registradores (v16,v17), destino v8
static int ncvt_16_8(void) {
  uint8_t out[16]; long vl;
  __asm__ volatile("vsetvli %0, %1, e16, m2, ta, ma" : "=r"(vl) : "r"(16));
  __asm__ volatile("vle16.v v16, (%0)" ::"r"(CS16) : "memory");
  __asm__ volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(16));
  TK(F_CVT, 1, 16, 0, 8);
  __asm__ volatile("vse8.v v8, (%0)" ::"r"(out) : "memory");
  int bad = 0; for (int i = 0; i < 16; i++) bad += (out[i] != CE8[i]);
  return bad;
}
static int ncvt_32_16(void) {
  uint16_t out[8]; long vl;
  __asm__ volatile("vsetvli %0, %1, e32, m2, ta, ma" : "=r"(vl) : "r"(8));
  __asm__ volatile("vle32.v v16, (%0)" ::"r"(CS32) : "memory");
  __asm__ volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(8));
  TK(F_CVT, 1, 16, 0, 8);
  __asm__ volatile("vse16.v v8, (%0)" ::"r"(out) : "memory");
  int bad = 0; for (int i = 0; i < 8; i++) bad += (out[i] != CE16[i]);
  return bad;
}

int main(void) {
  int fails = 0;
  fails += add8()  + sub8()  + mul8();
  fails += add16() + sub16() + mul16();
  fails += add32() + sub32() + mul32();
  fails += dotf8()  + dotp8()  + dotn8();
  fails += dotf16() + dotp16() + dotn16();
  fails += dotf32() + dotp32() + dotn32();
  fails += masked_add16() + masked_dot16();
  fails += ncvt_16_8() + ncvt_32_16();
  return fails > 100 ? 100 : fails;
}
