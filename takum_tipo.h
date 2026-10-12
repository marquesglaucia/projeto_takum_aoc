#ifndef TAKUM_TIPO_H
#define TAKUM_TIPO_H
// Tipo Takum<N> com operadores: permite escrever os algoritmos numericos
// uma unica vez (templates) e roda-los em T8/T16/T32, float, double, long double.
#include "takum_decoder.h"
#include <cmath>

template<int N>
struct Takum {
    static_assert(N == 8 || N == 16 || N == 32, "Takum: N deve ser 8, 16 ou 32");
    uint32_t bits = 0;                                   // padrao = 0

    Takum() = default;
    explicit Takum(long double x) : bits(takum_from_ld(x, N)) {}
    explicit Takum(double x)      : bits(takum_from_ld(x, N)) {}
    explicit Takum(int k)         : bits(takum_from_int(k, N)) {}
    explicit Takum(long long k)   : bits(takum_from_int(k, N)) {}

    static Takum raw(uint32_t b) { Takum t; t.bits = b & (N == 32 ? 0xFFFFFFFFu : ((1u << N) - 1u)); return t; }
    static Takum nar()           { return raw(1u << (N - 1)); }

    long double ld()  const { return takum_to_ld(bits, N); }
    bool eh_nar()     const { return bits == (1u << (N - 1)); }
    bool eh_zero()    const { return bits == 0; }

    friend Takum operator+(Takum a, Takum b) { return raw(takum_add(a.bits, b.bits, N)); }
    friend Takum operator-(Takum a, Takum b) { return raw(takum_sub(a.bits, b.bits, N)); }
    friend Takum operator*(Takum a, Takum b) { return raw(takum_mul(a.bits, b.bits, N)); }
    friend Takum operator/(Takum a, Takum b) { return raw(takum_div(a.bits, b.bits, N)); }
    friend Takum operator-(Takum a)          { return raw(takum_neg(a.bits, N)); }
    Takum& operator+=(Takum b) { return *this = *this + b; }
    Takum& operator-=(Takum b) { return *this = *this - b; }
    Takum& operator*=(Takum b) { return *this = *this * b; }
    Takum& operator/=(Takum b) { return *this = *this / b; }

    // comparacoes: todo takum de ate 32 bits e exato em long double (NaR -> NaN, comparacao falsa)
    friend bool operator< (Takum a, Takum b) { return a.ld() <  b.ld(); }
    friend bool operator> (Takum a, Takum b) { return a.ld() >  b.ld(); }
    friend bool operator<=(Takum a, Takum b) { return a.ld() <= b.ld(); }
    friend bool operator>=(Takum a, Takum b) { return a.ld() >= b.ld(); }
    friend bool operator==(Takum a, Takum b) { return a.bits == b.bits; }
    friend bool operator!=(Takum a, Takum b) { return a.bits != b.bits; }

    // funcoes elementares: avaliadas em long double e arredondadas uma vez para takum
    // (simula uma implementacao com arredondamento correto)
    friend Takum fabs(Takum a) { return (((a.bits >> (N - 1)) & 1) && !a.eh_nar()) ? -a : a; }
    friend Takum sqrt(Takum a) { return Takum(sqrtl(a.ld())); }
    friend Takum exp (Takum a) { return Takum(expl (a.ld())); }
    friend Takum log (Takum a) { return Takum(logl (a.ld())); }
    friend Takum sin (Takum a) { return Takum(sinl (a.ld())); }
    friend Takum cos (Takum a) { return Takum(cosl (a.ld())); }
    friend Takum atan(Takum a) { return Takum(atanl(a.ld())); }
};

typedef Takum<8>  T8;
typedef Takum<16> T16;
typedef Takum<32> T32;

// conversao uniforme p/ long double (relatorios / criterios de parada)
inline long double para_ld(float x)       { return x; }
inline long double para_ld(double x)      { return x; }
inline long double para_ld(long double x) { return x; }
template<int N> long double para_ld(Takum<N> x) { return x.ld(); }

inline const char* nome_tipo(float)       { return "float"; }
inline const char* nome_tipo(double)      { return "double"; }
inline const char* nome_tipo(long double) { return "long double"; }
inline const char* nome_tipo(T8)          { return "T8"; }
inline const char* nome_tipo(T16)         { return "T16"; }
inline const char* nome_tipo(T32)         { return "T32"; }

#endif
