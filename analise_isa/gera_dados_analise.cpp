// Gera test_data_analise.h: entradas (float e T32 com os MESMOS valores) e resultados esperados dos kernels de
// analise_isa/kernels.S.
//   - kernels Takum: esperado bit a bit com a biblioteca do projeto (dot_strips<32> com vl = VLEN/32 = 4,
//     takum_mul/takum_add), que e o mesmo modelo usado pelo Spike;
//   - kernels IEEE: a reducao do RVV (vfredusum) tem ordem NAO especificada, entao o esperado e um valor de
//     referencia em long double e a tolerancia e proporcional a sum|termos|.
#include "../takum_tipo.h"
#include <cstdio>
#include <cmath>
#include <random>
#include <vector>
#include <cstdint>
static std::mt19937 g(2026);
static std::normal_distribution<double> N01(0, 1);
static float rf() { return (float)ldexp(N01(g), (int)(g() % 7) - 3); }
static uint32_t tk(float f) { return takum_from_ld((long double)f, 32); }
static void arrf(const char* n, const std::vector<float>& v) {
    printf("static const float %s[%zu] = {", n, v.size());
    for (size_t i = 0; i < v.size(); i++) printf("%s%af", i ? "," : "", (double)v[i]);
    printf("};\n");
}
static void arru(const char* n, const std::vector<uint32_t>& v) {
    printf("static const uint32_t %s[%zu] = {", n, v.size());
    for (size_t i = 0; i < v.size(); i++) printf("%s0x%x", i ? "," : "", v[i]);
    printf("};\n");
}
int main() {
    const size_t VL = 4;                                   // e32, m1, VLEN = 128 (padrao do Spike)
    printf("/* GERADO por gera_dados_analise.cpp - nao edite */\n#include <stdint.h>\n");
    // ---- produto escalar e fma elemento a elemento: N nao multiplo de VL (exercita o strip final)
    const int N = 37;
    std::vector<float> x(N), y(N), a(N), b(N), y0(N);
    for (int i = 0; i < N; i++) { x[i] = rf(); y[i] = rf(); a[i] = rf(); b[i] = rf(); y0[i] = rf(); }
    std::vector<uint32_t> xt(N), yt(N), at(N), bt(N), y0t(N);
    for (int i = 0; i < N; i++) { xt[i] = tk(x[i]); yt[i] = tk(y[i]); at[i] = tk(a[i]); bt[i] = tk(b[i]); y0t[i] = tk(y0[i]); }
    long double s = 0, sabs = 0;
    for (int i = 0; i < N; i++) { s += (long double)x[i] * y[i]; sabs += fabsl((long double)x[i] * y[i]); }
    std::vector<T32> X(N), Y(N);
    for (int i = 0; i < N; i++) { X[i] = T32::raw(xt[i]); Y[i] = T32::raw(yt[i]); }
    uint32_t dot_t = dot_strips<32>(X.data(), Y.data(), N, VL).bits;
    printf("enum { N = %d };\n", N);
    arrf("XF", x); arrf("YF", y); arru("XT", xt); arru("YT", yt);
    printf("static const double DOT_REF = %a, DOT_ABS = %a;\n", (double)s, (double)sabs);
    printf("static const uint32_t DOT_T32 = 0x%x;\n", dot_t);
    arrf("AF", a); arrf("BF", b); arrf("Y0F", y0); arru("AT", at); arru("BT", bt); arru("Y0T", y0t);
    std::vector<float> yf(N); std::vector<uint32_t> ytr(N);
    for (int i = 0; i < N; i++) {
        yf[i] = fmaf(a[i], b[i], y0[i]);                    // vfmacc e fundido, RNE
        ytr[i] = (T32::raw(y0t[i]) + T32::raw(at[i]) * T32::raw(bt[i])).bits;  // vtmul seguido de vtadd
    }
    arrf("FMA_F", yf); arru("FMA_T", ytr);
    // ---- SpMV CSR 16x16 (~30%), com linhas vazias, curtas e longas (> VL: varios strips)
    const int R = 16, C = 16;
    std::vector<uint32_t> rp(1, 0), col; std::vector<float> val; std::vector<float> xv(C);
    for (int j = 0; j < C; j++) xv[j] = rf();
    std::vector<double> yref(R), yabs(R);
    for (int i = 0; i < R; i++) {
        double dens = (i == 3 || i == 10) ? 0.0 : (i == 5 ? 1.0 : 0.3);   // linhas 3 e 10 vazias, linha 5 cheia
        long double ss = 0, sa = 0;
        for (int j = 0; j < C; j++) if ((g() % 1000) / 1000.0 < dens || dens == 1.0) {
            float v = rf(); col.push_back(j); val.push_back(v);
            ss += (long double)v * xv[j]; sa += fabsl((long double)v * xv[j]);
        }
        rp.push_back(col.size()); yref[i] = (double)ss; yabs[i] = (double)sa;
    }
    std::vector<uint32_t> valt(val.size()), xvt(C), yexp(R);
    for (size_t k = 0; k < val.size(); k++) valt[k] = tk(val[k]);
    for (int j = 0; j < C; j++) xvt[j] = tk(xv[j]);
    for (int i = 0; i < R; i++) {
        std::vector<T32> p, q;
        for (uint32_t k = rp[i]; k < rp[i + 1]; k++) { p.push_back(T32::raw(valt[k])); q.push_back(T32::raw(xvt[col[k]])); }
        yexp[i] = p.empty() ? 0u : dot_strips<32>(p.data(), q.data(), p.size(), VL).bits;
    }
    printf("enum { NROWS = %d, NNZ = %zu };\n", R, val.size());
    arru("ROWPTR", rp); arru("COL", col); arrf("VALF", val); arru("VALT", valt); arrf("XVF", xv); arru("XVT", xvt);
    std::vector<float> dummy; 
    printf("static const double SPMV_REF[%d] = {", R); for (int i = 0; i < R; i++) printf("%s%a", i ? "," : "", yref[i]); printf("};\n");
    printf("static const double SPMV_ABS[%d] = {", R); for (int i = 0; i < R; i++) printf("%s%a", i ? "," : "", yabs[i]); printf("};\n");
    arru("SPMV_T32", yexp);
    return 0;
}
