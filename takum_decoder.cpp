#include "takum_decoder.h"
#include <cmath>

typedef unsigned __int128 u128;

static inline uint32_t mask_n(int n) { return n == 32 ? 0xFFFFFFFFu : ((1u << n) - 1u); }

// ---------------------------------------------------------------- decodificacao
static TakumFormat decodificar(uint32_t bits, int n) {
    TakumFormat t{};
    t.n = n;
    bits &= mask_n(n);
    if (bits == 0)                { t.tipo = TakumKind::ZERO; return t; }
    if (bits == (1u << (n - 1)))  { t.tipo = TakumKind::NAR;  return t; }
    t.tipo    = TakumKind::NORMAL;
    t.sinal   = (bits >> (n - 1)) & 1;
    t.direcao = (bits >> (n - 2)) & 1;
    t.regime  = (bits >> (n - 5)) & 7;
    int r = t.direcao ? t.regime : 7 - t.regime;
    t.valor_k = r;

    int w = n - 5;                                   // bits depois de S D R
    uint32_t rest = bits & ((1u << w) - 1u);
    uint32_t C, M; int p;
    if (w >= r) { p = w - r; C = rest >> p; M = p ? (rest & ((1u << p) - 1u)) : 0; }
    else        { p = 0;     C = rest << (r - w); M = 0; }   // C truncado: zeros a direita
    t.bits_C = (uint8_t)C; t.fracao = M; t.bits_fracao = p;
    t.caracteristica = t.direcao ? (int)((1u << r) - 1u + C)
                                 : (int)(-(int)(1u << (r + 1)) + 1 + (int)C);
    return t;
}
TakumFormat decodificar_T8 (uint8_t  b) { return decodificar(b, 8);  }
TakumFormat decodificar_T16(uint16_t b) { return decodificar(b, 16); }
TakumFormat decodificar_T32(uint32_t b) { return decodificar(b, 32); }

// ---------------------------------------------------------------- representacao interna
// valor = (-1)^s * (mant / 2^63) * 2^h , mant com bit 63 = 1 (1.f)
struct Unp { TakumKind k; int s; int h; uint64_t mant; };

static Unp unpack(uint32_t bits, int n) {
    TakumFormat t = decodificar(bits, n);
    Unp u{t.tipo, t.sinal, 0, 0};
    if (t.tipo != TakumKind::NORMAL) return u;
    uint64_t f63 = t.bits_fracao ? ((uint64_t)t.fracao << (63 - t.bits_fracao)) : 0;
    int c = t.caracteristica;
    if (!t.sinal)            { u.h = c;      u.mant = (1ull << 63) | f63; }
    else if (f63 == 0)       { u.h = -c;     u.mant = (1ull << 63); }
    else                     { u.h = -c - 1; u.mant = (1ull << 63) | ((1ull << 63) - f63); }
    return u;
}

// ---------------------------------------------------------------- codificacao (com arredondamento)
static uint32_t pack(int s, int h, uint64_t mant, bool sticky, int n) {
    uint64_t frac = mant & ((1ull << 63) - 1);
    int c; uint64_t m;
    if (!s)                          { c = h;      m = frac; }
    else if (frac == 0 && !sticky)   { c = -h;     m = 0; }
    else                             { c = -h - 1; m = (1ull << 63) - frac - (sticky ? 1 : 0); }

    uint32_t maxq = (1u << (n - 1)) - 1u, q;
    if (c > 254)       q = maxq;            // saturacao
    else if (c < -255) q = 1;
    else {
        int D = c >= 0, r, R; uint32_t C;
        if (D) { int v = c + 1; r = 31 - __builtin_clz(v);  R = r;     C = c - ((1 << r) - 1); }
        else   { int v = -c;    r = 31 - __builtin_clz(v);  R = 7 - r; C = c + (1 << (r + 1)) - 1; }
        u128 X = ((u128)((((uint32_t)D << 3) | R) << r | C) << 63) | m;
        int L = 4 + r + 63, sh = L - (n - 1);
        u128 qq = X >> sh, rem = X & (((u128)1 << sh) - 1), half = (u128)1 << (sh - 1);
        if (rem > half || (rem == half && (sticky || (qq & 1)))) qq += 1;
        q = qq > maxq ? maxq : (uint32_t)qq;
        if (q == 0) q = 1;
    }
    return ((uint32_t)s << (n - 1)) | q;
}

static uint32_t pack_u(const Unp& u, int n) {
    if (u.k == TakumKind::ZERO) return 0;
    if (u.k == TakumKind::NAR)  return 1u << (n - 1);
    return pack(u.s, u.h, u.mant, false, n);
}

static uint32_t codificar(const TakumFormat& t, int n) {
    if (t.tipo == TakumKind::ZERO) return 0;
    if (t.tipo == TakumKind::NAR)  return 1u << (n - 1);
    int r = t.valor_k, p = t.bits_fracao, w = n - 5;
    uint32_t rest = (w >= r) ? (((uint32_t)t.bits_C << p) | t.fracao) : (t.bits_C >> (r - w));
    return ((uint32_t)t.sinal << (n - 1)) | ((uint32_t)t.direcao << (n - 2)) |
           ((uint32_t)t.regime << (n - 5)) | rest;
}
uint8_t  codificar_T8 (const TakumFormat& t) { return (uint8_t) codificar(t, 8);  }
uint16_t codificar_T16(const TakumFormat& t) { return (uint16_t)codificar(t, 16); }
uint32_t codificar_T32(const TakumFormat& t) { return codificar(t, 32); }

// ---------------------------------------------------------------- aritmetica
uint32_t takum_mul(uint32_t a, uint32_t b, int n) {
    Unp x = unpack(a, n), y = unpack(b, n);
    if (x.k == TakumKind::NAR || y.k == TakumKind::NAR) return 1u << (n - 1);
    if (x.k == TakumKind::ZERO || y.k == TakumKind::ZERO) return 0;
    u128 P = (u128)x.mant * y.mant;                 // em [2^126, 2^128)
    int h = x.h + y.h; uint64_t mant; bool st;
    if (P >> 127) { h += 1; mant = (uint64_t)(P >> 64); st = (uint64_t)P != 0; }
    else          {         mant = (uint64_t)(P >> 63); st = (P & ((((u128)1) << 63) - 1)) != 0; }
    return pack(x.s ^ y.s, h, mant, st, n);
}

static uint32_t add_unp(Unp x, Unp y, int n) {
    if (x.k == TakumKind::NAR || y.k == TakumKind::NAR) return 1u << (n - 1);
    if (x.k == TakumKind::ZERO) return pack_u(y, n);
    if (y.k == TakumKind::ZERO) return pack_u(x, n);
    // garante |x| >= |y|
    if (y.h > x.h || (y.h == x.h && y.mant > x.mant)) { Unp t = x; x = y; y = t; }
    u128 A = (u128)x.mant << 62, B = (u128)y.mant << 62;
    int d = x.h - y.h; bool st = false;
    if (d >= 127) { B = 0; st = true; }
    else if (d > 0) { st = (B & ((((u128)1) << d) - 1)) != 0; B >>= d; }
    int h = x.h; u128 S;
    if (x.s == y.s) {
        S = A + B;
        if (S >> 126) { st = st || (S & 1); S >>= 1; h++; }
    } else {
        S = A - B - (st ? 1 : 0);
        if (S == 0 && !st) return 0;                 // cancelamento exato
        int msb = 127; while (!((S >> msb) & 1)) msb--;
        int lz = 125 - msb; S <<= lz; h -= lz;
    }
    uint64_t mant = (uint64_t)(S >> 62);
    st = st || (S & ((((u128)1) << 62) - 1)) != 0;
    return pack(x.s, h, mant, st, n);
}
uint32_t takum_add(uint32_t a, uint32_t b, int n) { return add_unp(unpack(a, n), unpack(b, n), n); }
uint32_t takum_sub(uint32_t a, uint32_t b, int n) {
    Unp y = unpack(b, n); if (y.k == TakumKind::NORMAL) y.s ^= 1;
    return add_unp(unpack(a, n), y, n);
}
uint32_t takum_cvt(uint32_t a, int nf, int nt) { return pack_u(unpack(a, nf), nt); }

uint8_t  takum_adicao_T8 (uint8_t a, uint8_t b)   { return (uint8_t) takum_add(a, b, 8);  }
uint16_t takum_adicao_T16(uint16_t a, uint16_t b) { return (uint16_t)takum_add(a, b, 16); }
uint32_t takum_adicao_T32(uint32_t a, uint32_t b) { return takum_add(a, b, 32); }
uint8_t  takum_subtracao_T8 (uint8_t a, uint8_t b)   { return (uint8_t) takum_sub(a, b, 8);  }
uint16_t takum_subtracao_T16(uint16_t a, uint16_t b) { return (uint16_t)takum_sub(a, b, 16); }
uint32_t takum_subtracao_T32(uint32_t a, uint32_t b) { return takum_sub(a, b, 32); }
uint8_t  takum_multiplicacao_T8 (uint8_t a, uint8_t b)   { return (uint8_t) takum_mul(a, b, 8);  }
uint16_t takum_multiplicacao_T16(uint16_t a, uint16_t b) { return (uint16_t)takum_mul(a, b, 16); }
uint32_t takum_multiplicacao_T32(uint32_t a, uint32_t b) { return takum_mul(a, b, 32); }

// ---------------------------------------------------------------- conversao p/ long double (testes)
long double takum_to_ld(uint32_t bits, int n) {
    Unp u = unpack(bits, n);
    if (u.k == TakumKind::ZERO) return 0.0L;
    if (u.k == TakumKind::NAR)  return NAN;
    long double v = ldexpl((long double)u.mant, u.h - 63);
    return u.s ? -v : v;
}
uint32_t takum_from_ld(long double x, int n) {
    if (x == 0.0L) return 0;
    if (!std::isfinite(x)) return 1u << (n - 1);
    int s = x < 0; if (s) x = -x;
    int e; long double g = frexpl(x, &e);            // g em [0.5,1)
    uint64_t mant = (uint64_t)ldexpl(g, 64);         // bit 63 = 1
    return pack(s, e - 1, mant, false, n);
}