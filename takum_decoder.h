#ifndef TAKUM_DECODER_H
#define TAKUM_DECODER_H
// Takum linear (Hunhold) - T8/T16/T32.
// Layout (n bits): S | D | R(3) | C(r bits) | M(p bits),  p = n-5-r
//   r = D ? R : 7-R
//   c = D ? 2^r-1 + C : -2^(r+1)+1 + C        (C truncado => completado com zeros)
//   f = M / 2^p
// 000...0 = zero ; 100...0 = NaR
#include <cstdint>

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

// Utilitarios para testes / experimentos de erro relativo
long double takum_to_ld(uint32_t bits, int n);
uint32_t    takum_from_ld(long double x, int n);

#endif