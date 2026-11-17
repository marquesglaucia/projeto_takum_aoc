// Controle negativo da analise de CSRs: um kernel IEEE (vfmul/vfredusum) com mstatus.FS = Off precisa dar excecao
// de instrucao ilegal. Sem tratador de excecao o programa NAO termina; o Makefile usa `timeout` e espera falha.
// (main.c prova o lado Takum: os kernels Zvtakum rodam normalmente com FS = Off.)
#include <stdint.h>
float dot_f32(long n, const float* x, const float* y);
static const float a[4] = {1, 2, 3, 4};
int main(void) {
    __asm__ volatile("csrc mstatus, %0" ::"r"(0x6000UL));   // FS = Off
    float r = dot_f32(4, a, a);
    return r == 30.0f ? 0 : 1;                              // so chega aqui se a FPU NAO estiver realmente desligada
}
