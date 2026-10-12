// Integracao numerica em aritmetica takum (T8/T16/T32) comparada com IEEE (float/double/long double).
// Compilar e rodar:  make integracao   (nao depende da libtakum)
#include "takum_integracao.h"
#include <cstdio>
#include <cmath>

static const long double PI = 3.141592653589793238462643383279502884L;

// ---------------------------------------------------------------- autoteste da aritmetica
// mul e exata em long double (mantissas <= 28 bits): o resultado deve bater sempre.
// add/div passam por um arredondamento intermediario em 64 bits (raras discrepancias possiveis).
static void autoteste(int n, int amostras) {
    uint64_t s = 0x9E3779B97F4A7C15ull;
    auto rnd = [&]() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)s; };
    uint32_t msk = n == 32 ? 0xFFFFFFFFu : ((1u << n) - 1u), nar = 1u << (n - 1);
    int e_add = 0, e_mul = 0, e_div = 0, e_neg = 0;
    for (int i = 0; i < amostras; ++i) {
        uint32_t a = rnd() & msk, b = rnd() & msk;
        if (a == nar || b == nar) continue;
        long double x = takum_to_ld(a, n), y = takum_to_ld(b, n);
        if (takum_add(a, b, n) != takum_from_ld(x + y, n)) ++e_add;
        if (takum_mul(a, b, n) != takum_from_ld(x * y, n)) ++e_mul;
        if (b && takum_div(a, b, n) != takum_from_ld(x / y, n)) ++e_div;
        if (takum_neg(a, n) != takum_from_ld(-x, n)) ++e_neg;
    }
    std::printf("autoteste T%-2d (%d amostras): divergencias add=%d mul=%d div=%d neg=%d\n",
                n, amostras, e_add, e_mul, e_div, e_neg);
}

// ---------------------------------------------------------------- metodos
enum Metodo { PM, TRAP, SIMP13, SIMP38, GAUSS2, GAUSS3, GAUSS5, ROMB, ADAPT, NUM_METODOS };
static const char* nome_metodo[] = {
    "Ponto medio", "Trapezio", "Simpson 1/3", "Simpson 3/8",
    "Gauss-Leg 2p", "Gauss-Leg 3p", "Gauss-Leg 5p", "Romberg", "Simpson adapt."
};

template<class T, class F>
long double aplicar(int m, F f, long double a, long double b, int n, Soma soma = Soma::INGENUA) {
    T A(a), B(b);
    switch (m) {
        case PM:     return para_ld(ponto_medio(f, A, B, n, soma));
        case TRAP:   return para_ld(trapezio(f, A, B, n, soma));
        case SIMP13: return para_ld(simpson13(f, A, B, n, soma));
        case SIMP38: return para_ld(simpson38(f, A, B, n, soma));
        case GAUSS2: return para_ld(gauss_legendre(f, A, B, n, 2, soma));
        case GAUSS3: return para_ld(gauss_legendre(f, A, B, n, 3, soma));
        case GAUSS5: return para_ld(gauss_legendre(f, A, B, n, 5, soma));
        case ROMB:   return para_ld(romberg(f, A, B, 16, 0.0L, soma));
        case ADAPT:  return para_ld(simpson_adaptativo(f, A, B, 1e-10L, 18));
    }
    return NAN;
}

static void imprime_erro(long double v, long double exato) {
    if (std::isnan(v))           std::printf(" %11s", "NaR/NaN");
    else if (std::isinf(v))      std::printf(" %11s", "overflow");
    else {
        long double e = fabsl(v - exato) / fabsl(exato);
        if (e == 0) std::printf(" %11s", "0 (exato)");
        else        std::printf(" %11.2e", (double)e);
    }
}

template<class F>
void problema(const char* nome, F f, long double a, long double b, long double exato, int n) {
    std::printf("\n=== %s   [%.6g, %.6g]   exato = %.15g   (n = %d)\n",
                nome, (double)a, (double)b, (double)exato, n);
    std::printf("%-15s %11s %11s %11s %11s %11s %11s\n", "erro relativo",
                "T8", "T16", "float", "T32", "double", "long double");
    for (int m = 0; m < NUM_METODOS; ++m) {
        std::printf("%-15s", nome_metodo[m]);
        imprime_erro(aplicar<T8>         (m, f, a, b, n), exato);
        imprime_erro(aplicar<T16>        (m, f, a, b, n), exato);
        imprime_erro(aplicar<float>      (m, f, a, b, n), exato);
        imprime_erro(aplicar<T32>        (m, f, a, b, n), exato);
        imprime_erro(aplicar<double>     (m, f, a, b, n), exato);
        imprime_erro(aplicar<long double>(m, f, a, b, n), exato);
        std::printf("\n");
    }
}

int main() {
    autoteste(8,  200000);
    autoteste(16, 1000000);
    autoteste(32, 1000000);

    // Integrandos genericos: o mesmo codigo e instanciado para cada tipo numerico.
    // Constantes sempre escritas como T(...) para que o arredondamento ocorra no formato testado.
    const int n = 16;

    problema("x^2", [](auto x) { return x * x; },
             0.0L, 1.0L, 1.0L / 3, n);

    problema("4/(1+x^2)  (= pi)", [](auto x) { using T = decltype(x); return T(4) / (T(1) + x * x); },
             0.0L, 1.0L, PI, n);

    problema("sin(x)", [](auto x) { using std::sin; return sin(x); },
             0.0L, PI, 2.0L, n);

    problema("exp(x)", [](auto x) { using std::exp; return exp(x); },
             0.0L, 1.0L, expl(1.0L) - 1, n);

    problema("1/x  (= ln 2)", [](auto x) { using T = decltype(x); return T(1) / x; },
             1.0L, 2.0L, logl(2.0L), n);

    problema("sqrt(x)  (derivada singular em 0)", [](auto x) { using std::sqrt; return sqrt(x); },
             0.0L, 1.0L, 2.0L / 3, n);

    // faixa dinamica: float estoura em 1e40, takum vai ate ~2^254 (~3e76)
    problema("1e40 * x^2  (faixa dinamica)",
             [](auto x) { using T = decltype(x); return T(1e40L) * x * x; },
             0.0L, 1.0L, 1e40L / 3, n);

    // efeito da soma compensada (Kahan) quando n e grande
    std::printf("\n=== Soma ingenua x Kahan: trapezio de exp(x) em [0,1]\n");
    std::printf("%-8s %-8s %11s %11s\n", "tipo", "n", "ingenua", "Kahan");
    auto fexp = [](auto x) { using std::exp; return exp(x); };
    const long double ex = expl(1.0L) - 1;
    for (int nn : {100, 1000, 10000, 100000}) {
        std::printf("%-8s %-8d", "T16", nn);
        imprime_erro(aplicar<T16>(TRAP, fexp, 0, 1, nn, Soma::INGENUA), ex);
        imprime_erro(aplicar<T16>(TRAP, fexp, 0, 1, nn, Soma::KAHAN),   ex);
        std::printf("\n%-8s %-8d", "T32", nn);
        imprime_erro(aplicar<T32>(TRAP, fexp, 0, 1, nn, Soma::INGENUA), ex);
        imprime_erro(aplicar<T32>(TRAP, fexp, 0, 1, nn, Soma::KAHAN),   ex);
        std::printf("\n%-8s %-8d", "float", nn);
        imprime_erro(aplicar<float>(TRAP, fexp, 0, 1, nn, Soma::INGENUA), ex);
        imprime_erro(aplicar<float>(TRAP, fexp, 0, 1, nn, Soma::KAHAN),   ex);
        std::printf("\n");
    }
    return 0;
}
