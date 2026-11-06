// Gera test_data.h: entradas e resultados ESPERADOS (calculados com a biblioteca Takum do projeto)
// para o teste bare-metal que roda no Spike. A aritmetica ja e validada contra a libtakum; aqui o
// objetivo e testar a INTEGRACAO (decodificacao, ordem vs2/vs1, mascara, vl/SEW, grupos de registradores).
#include "../takum_decoder.h"
#include <cstdio>
#include <random>
#include <vector>
#include <cmath>
static std::mt19937 g(7);
static std::normal_distribution<double> N01(0, 1);
static uint32_t rv(int n) { return takum_from_ld(ldexpl((long double)N01(g), (int)(g() % 9) - 4), n); }
static const char* ct(int n) { return n == 8 ? "uint8_t" : n == 16 ? "uint16_t" : "uint32_t"; }
static void arr(const char* name, int n, const std::vector<uint32_t>& v) {
    printf("static const %s %s[%zu] = {", ct(n), name, v.size());
    for (size_t i = 0; i < v.size(); i++) printf("%s0x%x", i ? "," : "", v[i]);
    printf("};\n");
}
int main() {
    printf("/* GERADO por gera_dados.cpp - nao edite */\n#include <stdint.h>\n");
    for (int n : {8, 16, 32}) {
        int L = 128 / n;                                   // elementos em 1 registrador (VLEN=128)
        std::vector<uint32_t> a(L), b(L), ad, sb, mu;
        for (int i = 0; i < L; i++) { a[i] = rv(n); b[i] = rv(n); }
        a[0] = 1u << (n - 1);            // NaR
        b[1] = 0; a[2 % L] = 0;          // zeros
        char nm[32];
        sprintf(nm, "A%d", n); arr(nm, n, a);  sprintf(nm, "B%d", n); arr(nm, n, b);
        for (int op = 0; op < 3; op++) {
            std::vector<uint32_t> e(L);
            for (int i = 0; i < L; i++) e[i] = op == 0 ? takum_add(a[i], b[i], n) : op == 1 ? takum_sub(a[i], b[i], n) : takum_mul(a[i], b[i], n);
            sprintf(nm, "%s%d", op == 0 ? "ADD" : op == 1 ? "SUB" : "MUL", n); arr(nm, n, e);
        }
        // produto escalar: vetores sem NaR, vl = L e vl parcial (3 ou L-1)
        std::vector<uint32_t> da(L), db(L); for (int i = 0; i < L; i++) { da[i] = rv(n); db[i] = rv(n); }
        uint32_t init = rv(n); std::vector<uint32_t> iv(L, 0); iv[0] = init;
        sprintf(nm, "DA%d", n); arr(nm, n, da); sprintf(nm, "DB%d", n); arr(nm, n, db); sprintf(nm, "DI%d", n); arr(nm, n, iv);
        int vlp = L > 4 ? 3 : L - 1;
        uint32_t d_full = takum_dot(da.data(), db.data(), L, init, n), d_part = takum_dot(da.data(), db.data(), vlp, init, n);
        printf("static const %s DOTF%d = 0x%x, DOTP%d = 0x%x; enum { VLP%d = %d };\n", ct(n), n, d_full, n, d_part, n, vlp);
        std::vector<uint32_t> dn = da; dn[L / 2] = 1u << (n - 1);          // NaR no vetor => resultado NaR
        sprintf(nm, "DN%d", n); arr(nm, n, dn);
        printf("static const %s DOTNAR%d = 0x%x;\n", ct(n), n, 1u << (n - 1));
    }
    // mascarado (e16, L=8): vtadd com v0 = 0xB5 e vd preenchido com sentinela (tu,mu)
    { int n = 16, L = 8; std::vector<uint32_t> s(L), e(L), a(L), b(L); unsigned m = 0xB5;
      for (int i = 0; i < L; i++) { a[i] = rv(n); b[i] = rv(n); s[i] = 0xBEEF; e[i] = ((m >> i) & 1) ? takum_add(a[i], b[i], n) : s[i]; }
      arr("MA", n, a); arr("MB", n, b); arr("MS", n, s); arr("MEXP", n, e); printf("static const uint8_t MASK16 = 0x%x;\n", m);
      // dot mascarado
      std::vector<uint32_t> ma; std::vector<uint32_t> mb; for (int i = 0; i < L; i++) if ((m >> i) & 1) { ma.push_back(a[i]); mb.push_back(b[i]); }
      uint32_t init = rv(n); std::vector<uint32_t> iv(L, 0); iv[0] = init; arr("MDI", n, iv);
      printf("static const uint16_t MDOT = 0x%x;\n", takum_dot(ma.data(), mb.data(), ma.size(), init, n)); }
    // vncvt: T16->T8 (16 elementos) e T32->T16 (8 elementos)
    { std::vector<uint32_t> s16(16), e8(16), s32(8), e16(8);
      for (int i = 0; i < 16; i++) { s16[i] = (i == 3) ? 0x8000u : (i == 5) ? 0u : g() & 0xFFFF; e8[i] = takum_cvt(s16[i], 16, 8); }
      for (int i = 0; i < 8; i++)  { s32[i] = (i == 1) ? 0x80000000u : g(); e16[i] = takum_cvt(s32[i], 32, 16); }
      arr("CS16", 16, s16); arr("CE8", 8, e8); arr("CS32", 32, s32); arr("CE16", 16, e16); }
    return 0;
}
