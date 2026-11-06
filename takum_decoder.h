#ifndef TAKUM_DECODER_H
#define TAKUM_DECODER_H
// Takum linear (Hunhold) - T8/T16/T32.
// Layout (n bits): S | D | R(3) | C(r bits) | M(p bits),  p = n-5-r
//   r = D ? R : 7-R
//   c = D ? 2^r-1 + C : -2^(r+1)+1 + C        (C truncado => completado com zeros)
//   f = M / 2^p
// 000...0 = zero ; 100...0 = NaR
#include <cstdint>
#include <cstddef>

enum class TakumKind : uint8_t { ZERO, NAR, NORMAL };

// Campos extraidos (Fase 1, item 1)
struct TakumFormat {
    TakumKind tipo;
    int       n;               // largura (8, 16 ou 32)
    uint8_t   sinal;           // S
    uint8_t   direcao;         // D
    uint8_t   regime;          // R (3 bits brutos, 0..7)
    int       valor_k;         // r = nº de bits da caracteristica
    uint8_t   bits_C;          // C bruto (ja completado com zeros se truncado)
    int       caracteristica;  // c com sinal, em [-255, 254]
    uint32_t  fracao;          // M bruto
    int       bits_fracao;     // p
};

TakumFormat decodificar_T8 (uint8_t  bits);
TakumFormat decodificar_T16(uint16_t bits);
TakumFormat decodificar_T32(uint32_t bits);

// Estrutura -> bits (reencoda exatamente o que decodificar_* produziu)
uint8_t  codificar_T8 (const TakumFormat& t);
uint16_t codificar_T16(const TakumFormat& t);
uint32_t codificar_T32(const TakumFormat& t);

// Aritmetica (Fase 1, item 2): operam em padroes de bits, arredondamento
// para o mais proximo (empate -> par), saturando (nunca overflow p/ NaR, nunca underflow p/ 0).
uint32_t takum_add(uint32_t a, uint32_t b, int n);
uint32_t takum_sub(uint32_t a, uint32_t b, int n);
uint32_t takum_mul(uint32_t a, uint32_t b, int n);
uint32_t takum_div(uint32_t a, uint32_t b, int n);       // x/0 = NaR
uint32_t takum_neg(uint32_t a, int n);                   // complemento de dois (exata)
uint32_t takum_cvt(uint32_t a, int n_from, int n_to);   // base do vncvt.t.t / vwcvt

uint8_t  takum_adicao_T8 (uint8_t a, uint8_t b);
uint16_t takum_adicao_T16(uint16_t a, uint16_t b);
uint32_t takum_adicao_T32(uint32_t a, uint32_t b);
uint8_t  takum_subtracao_T8 (uint8_t a, uint8_t b);
uint16_t takum_subtracao_T16(uint16_t a, uint16_t b);
uint32_t takum_subtracao_T32(uint32_t a, uint32_t b);
uint8_t  takum_multiplicacao_T8 (uint8_t a, uint8_t b);
uint16_t takum_multiplicacao_T16(uint16_t a, uint16_t b);
uint32_t takum_multiplicacao_T32(uint32_t a, uint32_t b);
uint8_t  takum_divisao_T8 (uint8_t a, uint8_t b);
uint16_t takum_divisao_T16(uint16_t a, uint16_t b);
uint32_t takum_divisao_T32(uint32_t a, uint32_t b);


// ---------------------------------------------------------------------------
// Produto escalar com acumulo em precisao ampliada  (semantica do vtdot.vv)
//
//   vtdot.vv vd, vs2, vs1 :   vd[0] = RN_n( vd[0] + sum_{i<vl} vs2[i] * vs1[i] )
//
//  - cada produto e EXATO (mantissa do produto <= 56 bits);
//  - o acumulador tem mantissa de 64 bits (long double x87), arredondado ao mais
//    proximo-par a cada soma ("precisao ampliada": > 2x a mantissa maxima do T32);
//  - UM unico arredondamento final para o formato do elemento (n bits).
//  - NaR em qualquer operando (inclusive vd[0]) => resultado NaR.
// Exige long double com mantissa >= 64 bits (x86/x86-64).
struct TakumAcc {
    long double v;   // acumulador (precisao ampliada)
    bool        nar; // NaR "pegajoso"
};
TakumAcc takum_acc_init(uint32_t init_bits, int n);              // acumulador := init
void     takum_acc_fma (TakumAcc& acc, uint32_t a, uint32_t b, int n); // acc += a*b (exato)
uint32_t takum_acc_result(const TakumAcc& acc, int n);          // arredonda 1x p/ n bits

uint32_t takum_dot(const uint32_t* a, const uint32_t* b, size_t len, uint32_t acc_init, int n);
uint8_t  takum_dot_T8 (const uint8_t*  a, const uint8_t*  b, size_t len, uint8_t  acc_init = 0);
uint16_t takum_dot_T16(const uint16_t* a, const uint16_t* b, size_t len, uint16_t acc_init = 0);
uint32_t takum_dot_T32(const uint32_t* a, const uint32_t* b, size_t len, uint32_t acc_init = 0);

// Utilitarios para testes / experimentos de erro relativo
long double takum_to_ld(uint32_t bits, int n);
uint32_t    takum_from_ld(long double x, int n);
uint32_t    takum_from_int(long long k, int n);           // inteiro -> takum (arredondado)

#endif