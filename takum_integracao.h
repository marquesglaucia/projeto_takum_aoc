#ifndef TAKUM_INTEGRACAO_H
#define TAKUM_INTEGRACAO_H
// Integracao numerica generica. T pode ser T8/T16/T32 (takum), float, double ou long double.
// Toda a aritmetica (nos, pesos, somas) e feita no tipo T; long double so e usado
// para os criterios de parada (comparacoes de modulo), que sao exatos.
#include "takum_tipo.h"
#include <cmath>
#include <vector>

enum class Soma { INGENUA, KAHAN };

// ---------------------------------------------------------------- acumulador
template<class T>
struct Acumulador {
    Soma modo; T s, c;                                 // c = compensacao de Kahan
    explicit Acumulador(Soma m) : modo(m), s(0), c(0) {}
    void add(T v) {
        if (modo == Soma::INGENUA) { s = s + v; return; }
        T y = v - c, t = s + y;
        c = (t - s) - y;
        s = t;
    }
    T valor() const { return s; }
};

// x_i = a + i*h (calculado a partir de i, sem acumular h repetidamente)
template<class T> inline T no(T a, T h, long long i) { return a + T(i) * h; }

// ---------------------------------------------------------------- ponto medio composto  O(h^2)
template<class T, class F>
T ponto_medio(F f, T a, T b, int n, Soma modo = Soma::INGENUA) {
    T h = (b - a) / T(n), meio = h / T(2);
    Acumulador<T> acc(modo);
    for (int i = 0; i < n; ++i) acc.add(f(a + T(2LL * i + 1) * meio));
    return h * acc.valor();
}

// ---------------------------------------------------------------- trapezio composto  O(h^2)
template<class T, class F>
T trapezio(F f, T a, T b, int n, Soma modo = Soma::INGENUA) {
    T h = (b - a) / T(n);
    Acumulador<T> acc(modo);
    acc.add((f(a) + f(b)) / T(2));
    for (int i = 1; i < n; ++i) acc.add(f(no(a, h, i)));
    return h * acc.valor();
}

// ---------------------------------------------------------------- Simpson 1/3 composto  O(h^4), n par
template<class T, class F>
T simpson13(F f, T a, T b, int n, Soma modo = Soma::INGENUA) {
    if (n % 2) ++n;
    T h = (b - a) / T(n);
    Acumulador<T> impar(modo), par(modo);
    for (int i = 1; i < n; ++i) (i % 2 ? impar : par).add(f(no(a, h, i)));
    T s = f(a) + f(b) + T(4) * impar.valor() + T(2) * par.valor();
    return h / T(3) * s;
}

// ---------------------------------------------------------------- Simpson 3/8 composto  O(h^4), n multiplo de 3
template<class T, class F>
T simpson38(F f, T a, T b, int n, Soma modo = Soma::INGENUA) {
    while (n % 3) ++n;
    T h = (b - a) / T(n);
    Acumulador<T> s3(modo), s2(modo);
    for (int i = 1; i < n; ++i) (i % 3 ? s3 : s2).add(f(no(a, h, i)));
    T s = f(a) + f(b) + T(3) * s3.valor() + T(2) * s2.valor();
    return T(3) * h / T(8) * s;
}

// ---------------------------------------------------------------- Gauss-Legendre composto (1..5 pontos)
struct GaussTabela { int p; long double t[5], w[5]; };
inline const GaussTabela& gauss_tabela(int p) {
    static const GaussTabela tab[5] = {
        {1, {0.0L}, {2.0L}},
        {2, {-0.577350269189625764509148780502L, 0.577350269189625764509148780502L},
            { 1.0L, 1.0L}},
        {3, {-0.774596669241483377035853079956L, 0.0L, 0.774596669241483377035853079956L},
            { 0.555555555555555555555555555556L, 0.888888888888888888888888888889L,
              0.555555555555555555555555555556L}},
        {4, {-0.861136311594052575223946488893L, -0.339981043584856264802665759103L,
              0.339981043584856264802665759103L,  0.861136311594052575223946488893L},
            { 0.347854845137453857373063949222L,  0.652145154862546142626936050778L,
              0.652145154862546142626936050778L,  0.347854845137453857373063949222L}},
        {5, {-0.906179845938663992797626878299L, -0.538469310105683091036314420700L, 0.0L,
              0.538469310105683091036314420700L,  0.906179845938663992797626878299L},
            { 0.236926885056189087514264040720L,  0.478628670499366468041291514836L,
              0.568888888888888888888888888889L,
              0.478628670499366468041291514836L,  0.236926885056189087514264040720L}},
    };
    if (p < 1) p = 1;
    if (p > 5) p = 5;
    return tab[p - 1];
}

template<class T, class F>
T gauss_legendre(F f, T a, T b, int n, int pontos = 3, Soma modo = Soma::INGENUA) {
    const GaussTabela& g = gauss_tabela(pontos);
    T t[5], w[5];
    for (int j = 0; j < g.p; ++j) { t[j] = T(g.t[j]); w[j] = T(g.w[j]); }
    T h = (b - a) / T(n), meia = h / T(2);
    Acumulador<T> acc(modo);
    for (int i = 0; i < n; ++i) {
        T centro = no(a, h, i) + meia;
        for (int j = 0; j < g.p; ++j) acc.add(w[j] * f(centro + meia * t[j]));
    }
    return meia * acc.valor();
}

// ---------------------------------------------------------------- Romberg
// R[i][0] = trapezio com 2^i subintervalos; R[i][j] = extrapolacao de Richardson.
// Para quando |R[i][i] - R[i-1][i-1]| <= tol ou a diferenca deixa de diminuir
// (em formatos de baixa precisao o arredondamento domina e insistir piora).
template<class T, class F>
T romberg(F f, T a, T b, int max_niveis = 12, long double tol = 0.0L,
          Soma modo = Soma::INGENUA, int* niveis_usados = nullptr) {
    std::vector<T> ant(1), cur;
    T h = b - a;
    ant[0] = h * (f(a) + f(b)) / T(2);
    long double dif_ant = INFINITY;
    int i = 1;
    for (; i < max_niveis; ++i) {
        cur.assign(i + 1, T(0));
        h = h / T(2);
        Acumulador<T> acc(modo);
        long long m = 1LL << (i - 1);                    // novos pontos: impares
        for (long long k = 1; k <= m; ++k) acc.add(f(no(a, h, 2 * k - 1)));
        cur[0] = ant[0] / T(2) + h * acc.valor();
        long long q = 1;
        for (int j = 1; j <= i; ++j) {
            q *= 4;
            cur[j] = cur[j - 1] + (cur[j - 1] - ant[j - 1]) / T(q - 1);
        }
        long double dif = fabsl(para_ld(cur[i]) - para_ld(ant[i - 1]));
        if (dif <= tol)     { ant.swap(cur); ++i; break; }
        if (dif >= dif_ant) { ant.swap(cur); ++i; break; }   // estagnou (erro de arredondamento)
        dif_ant = dif;
        ant.swap(cur);
    }
    if (niveis_usados) *niveis_usados = i;
    return ant.back();
}

// ---------------------------------------------------------------- Simpson adaptativo
template<class T, class F>
T simpson_adapt_rec(F& f, T a, T b, T fa, T fm, T fb, T S, long double tol, int prof, int* aval) {
    T m  = (a + b) / T(2);
    T lm = (a + m) / T(2), rm = (m + b) / T(2);
    T flm = f(lm), frm = f(rm);
    if (aval) *aval += 2;
    T h6 = (b - a) / T(12);                          // (m-a)/6
    T Sl = h6 * (fa + T(4) * flm + fm);
    T Sr = h6 * (fm + T(4) * frm + fb);
    T S2 = Sl + Sr;
    long double err = fabsl(para_ld(S2) - para_ld(S));
    // prof==0 ou intervalo que nao se subdivide mais no formato (m == a ou m == b)
    if (prof <= 0 || err <= 15.0L * tol || m == a || m == b)
        return S2 + (S2 - S) / T(15);                // correcao de Richardson
    return simpson_adapt_rec(f, a, m, fa, flm, fm, Sl, tol / 2, prof - 1, aval)
         + simpson_adapt_rec(f, m, b, fm, frm, fb, Sr, tol / 2, prof - 1, aval);
}

template<class T, class F>
T simpson_adaptativo(F f, T a, T b, long double tol = 1e-6L, int prof_max = 30, int* aval = nullptr) {
    T fa = f(a), fb = f(b), m = (a + b) / T(2), fm = f(m);
    if (aval) *aval = 3;
    T S = (b - a) / T(6) * (fa + T(4) * fm + fb);
    return simpson_adapt_rec(f, a, b, fa, fm, fb, S, tol, prof_max, aval);
}

#endif
