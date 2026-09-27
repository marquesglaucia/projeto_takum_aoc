#include "takum_decoder.h" // Importa o nosso "menu"
#include <iostream>

// ==========================================
// 2. Lógica de Decodificação (T8, T16, T32)
// ==========================================

TakumFormat decodificar_T8(uint8_t bits_entrada) {
    TakumFormat decodificado;
    
    decodificado.sinal = (bits_entrada >> 7) & 0x01; 
    decodificado.direcao = (bits_entrada >> 6) & 0x01; 
    
    int posicao_bit = 5;
    uint8_t primeiro_bit_regime = (bits_entrada >> posicao_bit) & 0x01;
    int tamanho_regime = 0;
    
    while (posicao_bit >= 0) {
        uint8_t bit_atual = (bits_entrada >> posicao_bit) & 0x01;
        if (bit_atual == primeiro_bit_regime) {
            tamanho_regime++;
            posicao_bit--;
        } else {
            tamanho_regime++; 
            posicao_bit--;
            break; 
        }
    }
    
    decodificado.regime = tamanho_regime; 
    
    // CÁLCULO DA ESCALA (Valor k) 
    if (primeiro_bit_regime == 1) {
        decodificado.valor_k = tamanho_regime - 1;
    } else {
        decodificado.valor_k = -tamanho_regime;
    }
    
    int bits_restantes = 8 - 2 - tamanho_regime; 
    
    if (bits_restantes > 0) {
        uint8_t mascara = (1 << bits_restantes) - 1; 
        decodificado.fracao = bits_entrada & mascara; 
    } else {
        decodificado.fracao = 0;
    }
    
    decodificado.caracteristica = 0; 
    return decodificado;
}

TakumFormat decodificar_T16(uint16_t bits_entrada) {
    TakumFormat decodificado;
    // ... (A tua lógica de T16 fica aqui)
    return decodificado;
}

TakumFormat decodificar_T32(uint32_t bits_entrada) {
    TakumFormat decodificado;
    // ... (A tua lógica de T32 fica aqui)
    return decodificado;
}

// ==========================================
// 3. Funções de Codificação Inversa (Estrutura -> Bits)
// ==========================================

uint8_t codificar_T8(TakumFormat valor_calculado) {
    uint8_t bits_finais = 0;
    // O código de remontar os bits será feito aqui depois
    return bits_finais;
}

uint16_t codificar_T16(TakumFormat valor_calculado) {
    uint16_t bits_finais = 0;
    return bits_finais;
}

uint32_t codificar_T32(TakumFormat valor_calculado) {
    uint32_t bits_finais = 0;
    return bits_finais;
}

// ==========================================
// 4. Operações Aritméticas (Fase 1)
// ==========================================

TakumFormat takum_adicao(TakumFormat operadorA, TakumFormat operadorB) {
    TakumFormat resultado;
    
    uint32_t fracao_A_real = (1 << 24) | operadorA.fracao; 
    uint32_t fracao_B_real = (1 << 24) | operadorB.fracao;

    int diferenca_escala = operadorA.valor_k - operadorB.valor_k;
    
    if (diferenca_escala > 0) {
        fracao_B_real = fracao_B_real >> diferenca_escala;
        resultado.valor_k = operadorA.valor_k; 
    } else if (diferenca_escala < 0) {
        fracao_A_real = fracao_A_real >> (-diferenca_escala);
        resultado.valor_k = operadorB.valor_k;
    } else {
        resultado.valor_k = operadorA.valor_k;
    }

    uint32_t fracao_soma = 0;
    if (operadorA.sinal == operadorB.sinal) {
        fracao_soma = fracao_A_real + fracao_B_real;
        resultado.sinal = operadorA.sinal;
    } else {
        // Lógica de subtração pendente
    }

    if (fracao_soma & (1 << 25)) { 
        fracao_soma = fracao_soma >> 1;
        resultado.valor_k++; 
    }

    resultado.fracao = fracao_soma & ((1 << 24) - 1);
    return resultado;
}

TakumFormat takum_subtracao(TakumFormat operadorA, TakumFormat operadorB) {
    TakumFormat resultado;
    return resultado;
}

TakumFormat takum_multiplicacao(TakumFormat operadorA, TakumFormat operadorB) {
    TakumFormat resultado;
    return resultado;
}