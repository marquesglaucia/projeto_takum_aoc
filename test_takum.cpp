// Testes: compara a nossa implementacao com a libtakum (referencia oficial)
#include "takum_decoder.h"
#include <cstdio>
#include <cstdlib>
#include <random>
extern "C" {
#include "takum.h"
}
static int falhas = 0;
#define CHECK(cond, ...) do { if (!(cond)) { if (falhas++ < 15) { printf("FALHA: "); printf(__VA_ARGS__); printf("\n"); } } } while (0)

int main() {
    // 1) decode -> encode identidade + valor igual ao da libtakum (T8 e T16 exaustivo)
    for (uint32_t b = 0; b < 256; b++) {
        TakumFormat t = decodificar_T8(b);
        CHECK(codificar_T8(t) == b, "roundtrip T8 %02x", b);
        double ref = takum8_to_float64((takum8)b), me = (double)takum_to_ld(b, 8);
        CHECK((ref != ref && me != me) || ref == me, "valor T8 %02x ref=%g me=%g", b, ref, me);
        CHECK(takum_from_ld(takum_to_ld(b, 8), 8) == b || b == 0x80, "from_ld T8 %02x", b);
    }
    for (uint32_t b = 0; b < 65536; b++) {
        TakumFormat t = decodificar_T16(b);
        CHECK(codificar_T16(t) == b, "roundtrip T16 %04x", b);
        double ref = takum16_to_float64((takum16)b), me = (double)takum_to_ld(b, 16);
        CHECK((ref != ref && me != me) || ref == me, "valor T16 %04x ref=%g me=%g", b, ref, me);
    }
    std::mt19937 rng(42);
    for (int i = 0; i < 2000000; i++) {
        uint32_t b = rng();
        CHECK(codificar_T32(decodificar_T32(b)) == b, "roundtrip T32 %08x", b);
        double ref = takum32_to_float64((takum32)b), me = (double)takum_to_ld(b, 32);
        CHECK((ref != ref && me != me) || ref == me, "valor T32 %08x ref=%.17g me=%.17g", b, ref, me);
    }
    printf("[decode/encode] falhas ate aqui: %d\n", falhas);

    // 2) aritmetica vs libtakum
    int f0 = falhas; long total = 0;
    for (uint32_t a = 0; a < 256; a++) for (uint32_t b = 0; b < 256; b++) {
        total += 3;
        CHECK(takum_adicao_T8(a,b)==(uint8_t)takum8_addition(a,b), "add8 %02x %02x me=%02x ref=%02x", a,b,takum_adicao_T8(a,b),(uint8_t)takum8_addition(a,b));
        CHECK(takum_subtracao_T8(a,b)==(uint8_t)takum8_subtraction(a,b), "sub8 %02x %02x me=%02x ref=%02x", a,b,takum_subtracao_T8(a,b),(uint8_t)takum8_subtraction(a,b));
        CHECK(takum_multiplicacao_T8(a,b)==(uint8_t)takum8_multiplication(a,b), "mul8 %02x %02x me=%02x ref=%02x", a,b,takum_multiplicacao_T8(a,b),(uint8_t)takum8_multiplication(a,b));
    }
    printf("[T8 exaustivo, %ld ops] falhas: %d\n", total, falhas - f0); f0 = falhas;
    for (int i = 0; i < 3000000; i++) {
        uint16_t a = rng(), b = rng();
        CHECK(takum_adicao_T16(a,b)==(uint16_t)takum16_addition(a,b), "add16 %04x %04x me=%04x ref=%04x", a,b,takum_adicao_T16(a,b),(uint16_t)takum16_addition(a,b));
        CHECK(takum_subtracao_T16(a,b)==(uint16_t)takum16_subtraction(a,b), "sub16 %04x %04x me=%04x ref=%04x", a,b,takum_subtracao_T16(a,b),(uint16_t)takum16_subtraction(a,b));
        CHECK(takum_multiplicacao_T16(a,b)==(uint16_t)takum16_multiplication(a,b), "mul16 %04x %04x me=%04x ref=%04x", a,b,takum_multiplicacao_T16(a,b),(uint16_t)takum16_multiplication(a,b));
    }
    printf("[T16 aleatorio, 9M ops] falhas: %d\n", falhas - f0); f0 = falhas;
    for (int i = 0; i < 3000000; i++) {
        uint32_t a = rng(), b = rng();
        CHECK(takum_adicao_T32(a,b)==(uint32_t)takum32_addition(a,b), "add32 %08x %08x me=%08x ref=%08x", a,b,takum_adicao_T32(a,b),(uint32_t)takum32_addition(a,b));
        CHECK(takum_subtracao_T32(a,b)==(uint32_t)takum32_subtraction(a,b), "sub32 %08x %08x me=%08x ref=%08x", a,b,takum_subtracao_T32(a,b),(uint32_t)takum32_subtraction(a,b));
        CHECK(takum_multiplicacao_T32(a,b)==(uint32_t)takum32_multiplication(a,b), "mul32 %08x %08x me=%08x ref=%08x", a,b,takum_multiplicacao_T32(a,b),(uint32_t)takum32_multiplication(a,b));
    }
    printf("[T32 aleatorio, 9M ops] falhas: %d\n", falhas - f0);

    // --- TESTES DE CASOS EXTREMOS (EDGE CASES) EM T32 ---
    uint32_t zero = 0x00000000;
    uint32_t nar = 0x80000000; // Not a Real
    uint32_t val1 = takum_from_ld(1.5L, 32);

    // 1. Contaminação por NaR (Not a Real)
    CHECK(takum_adicao_T32(val1, nar) == nar, "Falha: Add com NaR deve propagar NaR");
    CHECK(takum_subtracao_T32(nar, val1) == nar, "Falha: Sub com NaR deve propagar NaR");

    // 2. Operações com o Zero exato do Takum
    CHECK(takum_adicao_T32(val1, zero) == val1, "Falha: Add com Zero");
    CHECK(takum_subtracao_T32(val1, zero) == val1, "Falha: Sub com Zero");

    // 3. O Teste Supremo da Subtração: Cancelamento Exato (X - X = 0)
    CHECK(takum_subtracao_T32(val1, val1) == zero, "Falha: Subtracao de valores iguais deve resultar no Zero exato");

    printf(falhas ? "RESULTADO: %d FALHAS\n" : "RESULTADO: TUDO OK (%d)\n", falhas);
    return falhas != 0;
}