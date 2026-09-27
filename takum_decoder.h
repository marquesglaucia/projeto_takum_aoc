#ifndef TAKUM_DECODER_H
#define TAKUM_DECODER_H

#include <cstdint>

// ==========================================
// 1. A Estrutura de Dados Takum
// ==========================================
struct TakumFormat {
    uint8_t sinal;         // S
    uint8_t direcao;       // D
    uint8_t regime;        // R
    int valor_k;           // Guarda o valor matemático do regime (escala)
    uint8_t caracteristica;// C
    uint32_t fracao;       // F
};

// ==========================================
// 2. Funções de Decodificação (Bits -> Estrutura)
// Exigência da Fase 1 para tamanhos T8, T16 e T32
// ==========================================
TakumFormat decodificar_T8(uint8_t bits_entrada);
TakumFormat decodificar_T16(uint16_t bits_entrada);
TakumFormat decodificar_T32(uint32_t bits_entrada);

// ==========================================
// 3. Funções de Codificação Inversa (Estrutura -> Bits)
// Necessárias para guardar o resultado de volta no processador
// ==========================================
uint8_t codificar_T8(TakumFormat valor_calculado);
uint16_t codificar_T16(TakumFormat valor_calculado);
uint32_t codificar_T32(TakumFormat valor_calculado);

// ==========================================
// 4. Operações Aritméticas Fundamentais
// ==========================================
TakumFormat takum_adicao(TakumFormat operadorA, TakumFormat operadorB);
TakumFormat takum_subtracao(TakumFormat operadorA, TakumFormat operadorB);
TakumFormat takum_multiplicacao(TakumFormat operadorA, TakumFormat operadorB);

#endif // TAKUM_DECODER_H