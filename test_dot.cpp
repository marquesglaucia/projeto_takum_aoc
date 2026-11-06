// Testes do takum_dot (semantica do vtdot.vv): acumulo em precisao ampliada, 1 arredondamento final.
#include "takum_decoder.h"
#include "takum_tipo.h"
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>
#include <algorithm>
#include <quadmath.h>

static int falhas = 0;
#define CHECK(cond, ...) do { if (!(cond)) { if (falhas++ < 15) { printf("FALHA: "); printf(__VA_ARGS__); printf("\n"); } } } while (0)

static uint32_t rnd_bits(std::mt19937& g, int n) { uint32_t b = g(); return n == 32 ? b : (b & ((1u << n) - 1u)); }

int main() {
    std::mt19937 g(2026);

    // 1) len=1, init=0  ==  takum_mul (produto exato, 1 arredondamento)
    for (uint32_t a = 0; a < 256; a++) for (uint32_t b = 0; b < 256; b++) {
        uint32_t da = takum_dot(&a, &b, 1, 0, 8);
        CHECK(da == takum_mul(a, b, 8), "dot==mul T8 %02x %02x: %02x vs %02x", a, b, da, takum_mul(a, b, 8));
    }
    for (int n : {16, 32}) for (int i = 0; i < 3000000; i++) {
        uint32_t a = rnd_bits(g, n), b = rnd_bits(g, n);
        CHECK(takum_dot(&a, &b, 1, 0, n) == takum_mul(a, b, n), "dot==mul T%d %08x %08x", n, a, b);
    }
    printf("[1] dot(len=1) == takum_mul (T8 exaustivo, T16/T32 6M casos): falhas=%d\n", falhas);

    // 2) dot([a,b],[1,1]) == takum_add(a,b)   (soma de 2 termos: sem dupla arredondagem possivel)
    int f0 = falhas;
    for (int n : {8, 16, 32}) {
        uint32_t um = takum_from_ld(1.0L, n);
        long cnt = (n == 8) ? 65536 : 3000000;
        for (long i = 0; i < cnt; i++) {
            uint32_t ab[2], uu[2] = {um, um};
            if (n == 8) { ab[0] = i & 255; ab[1] = i >> 8; } else { ab[0] = rnd_bits(g, n); ab[1] = rnd_bits(g, n); }
            CHECK(takum_dot(ab, uu, 2, 0, n) == takum_add(ab[0], ab[1], n), "dot==add T%d %08x %08x", n, ab[0], ab[1]);
        }
    }
    printf("[2] dot([a,b],[1,1]) == takum_add: falhas=%d\n", falhas - f0); f0 = falhas;

    // 3) NaR / zero
    for (int n : {8, 16, 32}) {
        uint32_t nar = 1u << (n - 1), um = takum_from_ld(1.0L, n), dois = takum_from_ld(2.0L, n);
        uint32_t v1[3] = {um, nar, dois}, v2[3] = {um, um, um}, z[3] = {0, 0, 0};
        CHECK(takum_dot(v1, v2, 3, 0, n) == nar, "NaR no vetor T%d", n);
        CHECK(takum_dot(v2, v2, 3, nar, n) == nar, "NaR no acc_init T%d", n);
        CHECK(takum_dot(v1, z, 3, 0, n) == nar, "0*NaR deve ser NaR T%d", n);   // consistente com takum_mul
        CHECK(takum_dot(z, v2, 3, 0, n) == 0, "dot com zeros T%d", n);
        CHECK(takum_dot(v2, v2, 0, dois, n) == dois, "len=0 devolve acc_init T%d", n);
    }
    printf("[3] NaR/zero/len=0: falhas=%d\n", falhas - f0); f0 = falhas;

    // 4) contra referencia independente em __float128 (113 bits).
    //    4a) dados realistas (|x| ~ N(0,1) * 2^k, k em [-8,8]): exige diferenca <= 1 ulp
    //    4b) padroes de bits aleatorios (expoentes de ate +-255): informativo. Cancelamentos > 64 bits
    //        excedem o acumulador de 64 bits de mantissa (limite do projeto, nao bug).
    std::normal_distribution<double> N01(0.0, 1.0);
    std::uniform_int_distribution<int> K(-8, 8);
    f0 = falhas;
    for (int modo = 0; modo < 2; modo++) for (int n : {8, 16, 32}) {
        long diff = 0, tot = 200000, maxulp = 0;
        for (long t = 0; t < tot; t++) {
            int len = 1 + g() % 64;
            std::vector<uint32_t> a(len), b(len);
            __float128 s = 0; bool nar = false;
            for (int i = 0; i < len; i++) {
                if (modo == 0) { a[i] = takum_from_ld(ldexpl(N01(g), K(g)), n); b[i] = takum_from_ld(ldexpl(N01(g), K(g)), n); }
                else           { a[i] = rnd_bits(g, n); b[i] = rnd_bits(g, n); }
                long double x = takum_to_ld(a[i], n), y = takum_to_ld(b[i], n);
                if (x != x || y != y) nar = true; else s += (__float128)x * (__float128)y;
            }
            if (nar) continue;
            uint32_t me = takum_dot(a.data(), b.data(), len, 0, n);
            uint32_t ref = takum_from_ld((long double)s, n);
            if (me != ref) {
                diff++;
                long d = std::labs((long)(int32_t)(me << (32 - n)) - (long)(int32_t)(ref << (32 - n))) >> (32 - n);
                maxulp = std::max(maxulp, d);
            }
        }
        if (modo == 0) CHECK(maxulp <= 1, "T%d dados realistas: diferenca maxima %ld ulp (>1)", n, maxulp);
        printf("[4%c] T%-2d vs __float128 (%s): %ld/%ld diferentes (%.4f%%), max %ld ulp\n", 'a' + modo, n,
               modo == 0 ? "realista" : "bits aleatorios", diff, tot, 100.0 * diff / tot, maxulp);
    }
    printf("[4] falhas=%d\n", falhas - f0);

    // 4c) dump de vetores (incluindo extremos) p/ validar o ACUMULADOR bit a bit contra o modelo Python
    {
        FILE* fo = fopen("dot_vectors.txt", "w");
        for (int n : {8, 16, 32}) for (int t = 0; t < 4000; t++) {
            int len = 1 + g() % 48; bool ext = t & 1;
            std::vector<uint32_t> a(len), b(len); uint32_t init = ext ? rnd_bits(g, n) : takum_from_ld(N01(g), n);
            for (int i = 0; i < len; i++) {
                if (ext) { a[i] = rnd_bits(g, n); b[i] = rnd_bits(g, n); }
                else     { a[i] = takum_from_ld(ldexpl(N01(g), K(g)), n); b[i] = takum_from_ld(ldexpl(N01(g), K(g)), n); }
            }
            TakumAcc acc = takum_acc_init(init, n);
            for (int i = 0; i < len; i++) takum_acc_fma(acc, a[i], b[i], n);
            if (acc.nar) continue;                                   // NaR ja testado em [3]
            fprintf(fo, "CASE %d %d %La\n", n, len, takum_to_ld(init, n));
            for (int i = 0; i < len; i++) fprintf(fo, "%La %La\n", takum_to_ld(a[i], n), takum_to_ld(b[i], n));
            fprintf(fo, "ACC %La\n", acc.v);
        }
        fclose(fo);
    }
    f0 = falhas;

    // 5) dot_strips: vl>=len == dot ; vl=1 == encadear (arredonda a cada passo)
    for (int t = 0; t < 20000; t++) {
        int len = 1 + g() % 40;
        std::vector<T16> x(len), y(len);
        for (int i = 0; i < len; i++) { x[i] = T16::raw(rnd_bits(g, 16)); y[i] = T16::raw(rnd_bits(g, 16)); }
        CHECK(dot_strips<16>(x.data(), y.data(), len, len + 5).bits == dot<16>(x.data(), y.data(), len).bits, "strips(vl>=len)");
        T16 r = T16();
        for (int i = 0; i < len; i++) r = dot<16>(&x[i], &y[i], 1, r);
        CHECK(dot_strips<16>(x.data(), y.data(), len, 1).bits == r.bits, "strips(vl=1)");
    }
    printf("[5] dot_strips (vl>=len e vl=1): falhas=%d\n", falhas - f0);

    // 6) Beneficio do acumulo ampliado: erro relativo mediano, vetores de 64 elementos, valores em [-1,1]
    //    cadeia = mul e add arredondados em T-n a cada passo ; vtdot = acumulo ampliado + 1 arredondamento
    std::uniform_real_distribution<double> U(-1.0, 1.0);
    for (int n : {8, 16, 32}) {
        std::vector<double> ec, ed;
        for (int t = 0; t < 5000; t++) {
            const int len = 64; uint32_t a[len], b[len]; long double exato = 0, acc = 0; uint32_t cad = 0;
            for (int i = 0; i < len; i++) {
                a[i] = takum_from_ld(U(g), n); b[i] = takum_from_ld(U(g), n);
                exato += takum_to_ld(a[i], n) * takum_to_ld(b[i], n);
                cad = takum_add(cad, takum_mul(a[i], b[i], n), n);
            }
            (void)acc;
            if (fabsl(exato) < 1e-3L) continue;                 // evita erro relativo explosivo
            uint32_t d = takum_dot(a, b, len, 0, n);
            ec.push_back((double)fabsl((takum_to_ld(cad, n) - exato) / exato));
            ed.push_back((double)fabsl((takum_to_ld(d,   n) - exato) / exato));
        }
        std::sort(ec.begin(), ec.end()); std::sort(ed.begin(), ed.end());
        printf("[6] T%-2d len=64: erro relativo mediano  cadeia(mul+add)=%.3e   vtdot=%.3e   (n=%zu)\n",
               n, ec[ec.size()/2], ed[ed.size()/2], ec.size());
    }

    printf(falhas ? "RESULTADO: %d FALHAS\n" : "RESULTADO: TUDO OK (%d)\n", falhas);
    return falhas != 0;
}
