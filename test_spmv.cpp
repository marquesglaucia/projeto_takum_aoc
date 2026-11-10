// Benchmark SpMV (CSR) e GEMV: Takum (T8/T16/T32) vs IEEE (FP32/FP16) e OFP8 (E4M3/E5M2).
#include "takum_decoder.h"
#include "takum_tipo.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <random>
#include <vector>
#include <algorithm>

static inline __float128 abs128(__float128 x) { return x < 0 ? -x : x; }

static double quant_fp(double x, int ebits, int mbits, int emax, double max_val, bool sat) {
    if (x == 0.0 || std::isnan(x)) return x;
    if (std::isinf(x)) return sat ? (x < 0 ? -max_val : max_val) : x;
    double s = x < 0 ? -1.0 : 1.0;
    x = std::fabs(x);
    int bias = (1 << (ebits - 1)) - 1;
    int emin = 1 - bias;
    int emin_sub = emin - mbits;
    double min_norm = std::ldexp(1.0, emin);

    if (x < min_norm) {
        double step = std::ldexp(1.0, emin_sub);
        double q = std::rint(x / step);
        return s * std::ldexp(q, emin_sub);
    }
    int e;
    std::frexp(x, &e);
    int exp_unb = e - 1;
    double scaled = std::ldexp(x, mbits - exp_unb);
    long long q = std::llrint(scaled);
    if (q == (1LL << (mbits + 1))) {
        q >>= 1;
        exp_unb++;
    }
    double res = std::ldexp((double)q, exp_unb - mbits);
    if (exp_unb > emax || res > max_val) {
        return sat ? (s * max_val) : (s * INFINITY);
    }
    return s * res;
}

static inline double q_fp16(double x)     { return quant_fp(x, 5, 10, 15, 65504.0, false); }
static inline double q_fp8_e5m2(double x) { return quant_fp(x, 5, 2,  15, 57344.0, true);  }
static inline double q_fp8_e4m3(double x) { return quant_fp(x, 4, 3,  8,  448.0,   true);  }

struct CSR {
    int nrows, ncols;
    std::vector<int> row_ptr;
    std::vector<int> col_idx;
    std::vector<long double> val;
};

static CSR gerar_csr(int n, double densidade, bool faixa_ampla, std::mt19937& g) {
    CSR A{n, n, std::vector<int>(n + 1, 0), {}, {}};
    std::normal_distribution<double> N01(0.0, 1.0);
    std::uniform_real_distribution<double> U01(0.0, 1.0);
    std::uniform_int_distribution<int> K(-18, 18);

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (densidade >= 1.0 || U01(g) < densidade) {
                long double v = (long double)N01(g);
                if (faixa_ampla) v = std::ldexp(v, K(g));
                A.col_idx.push_back(j);
                A.val.push_back(v);
            }
        }
        if (A.row_ptr[i] == (int)A.col_idx.size()) {
            int j = g() % n;
            long double v = (long double)N01(g);
            if (faixa_ampla) v = std::ldexp(v, K(g));
            A.col_idx.push_back(j);
            A.val.push_back(v);
        }
        A.row_ptr[i + 1] = (int)A.col_idx.size();
    }
    return A;
}

static std::vector<long double> gerar_vetor(int n, bool faixa_ampla, std::mt19937& g) {
    std::vector<long double> x(n);
    std::normal_distribution<double> N01(0.0, 1.0);
    std::uniform_int_distribution<int> K(-18, 18);
    for (int i = 0; i < n; ++i) {
        long double v = (long double)N01(g);
        if (faixa_ampla) v = std::ldexp(v, K(g));
        x[i] = v;
    }
    return x;
}

static std::vector<__float128> spmv_ref128(const CSR& A, const std::vector<long double>& x) {
    std::vector<__float128> y(A.nrows, 0);
    for (int i = 0; i < A.nrows; ++i) {
        __float128 acc = 0;
        for (int k = A.row_ptr[i]; k < A.row_ptr[i + 1]; ++k) {
            acc += (__float128)A.val[k] * (__float128)x[A.col_idx[k]];
        }
        y[i] = acc;
    }
    return y;
}

template<int N>
static std::vector<long double> spmv_takum(const CSR& A, const std::vector<long double>& x, int modo) {
    std::vector<Takum<N>> x_tk(x.size());
    for (size_t j = 0; j < x.size(); ++j) x_tk[j] = Takum<N>(x[j]);

    std::vector<long double> y(A.nrows, 0.0L);
    const size_t vl_max = 128 / N;

    for (int i = 0; i < A.nrows; ++i) {
        int ini = A.row_ptr[i], fim = A.row_ptr[i + 1], len = fim - ini;
        std::vector<Takum<N>> a_row(len), b_row(len);
        for (int k = 0; k < len; ++k) {
            a_row[k] = Takum<N>(A.val[ini + k]);
            b_row[k] = x_tk[A.col_idx[ini + k]];
        }
        Takum<N> res(0.0L);
        if (modo == 0) {
            for (int k = 0; k < len; ++k) res = res + (a_row[k] * b_row[k]);
        } else if (modo == 1) {
            res = dot<N>(a_row.data(), b_row.data(), len);
        } else {
            res = dot_strips<N>(a_row.data(), b_row.data(), len, vl_max);
        }
        y[i] = res.ld();
    }
    return y;
}

template<typename QFunc>
static std::vector<long double> spmv_ieee_sim(const CSR& A, const std::vector<long double>& x, QFunc qf) {
    std::vector<double> x_q(x.size());
    for (size_t j = 0; j < x.size(); ++j) x_q[j] = qf((double)x[j]);

    std::vector<long double> y(A.nrows, 0.0L);
    for (int i = 0; i < A.nrows; ++i) {
        double acc = 0.0;
        for (int k = A.row_ptr[i]; k < A.row_ptr[i + 1]; ++k) {
            double a_val = qf((double)A.val[k]);
            double prod  = qf(a_val * x_q[A.col_idx[k]]);
            acc = qf(acc + prod);
        }
        y[i] = (long double)acc;
    }
    return y;
}

static double calc_erro_rel(long double aprox, __float128 exato) {
    if (std::isnan(aprox) || std::isinf(aprox)) return 1.0;
    __float128 diff = abs128((__float128)aprox - exato);
    __float128 rel  = diff / abs128(exato);
    double r = (double)rel;
    return r < 1e-16 ? 1e-16 : r;
}

static double mediana(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

static double percentil95(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v[(size_t)(0.95 * (v.size() - 1))];
}

int main() {
    printf("=== Benchmark SpMV e GEMV: Takum (T8/T16/T32) vs FP32/FP16/OFP8 ===\n");

    {
        std::mt19937 g(42);
        CSR A = gerar_csr(100, 0.10, false, g);
        auto x = gerar_vetor(100, false, g);
        auto y_ref = spmv_ref128(A, x);

        auto y_t8_dot  = spmv_takum<8>(A, x, 1);
        auto y_t16_dot = spmv_takum<16>(A, x, 1);
        auto y_t32_dot = spmv_takum<32>(A, x, 1);
        auto y_t8_cad  = spmv_takum<8>(A, x, 0);
        auto y_t16_cad = spmv_takum<16>(A, x, 0);
        auto y_t32_cad = spmv_takum<32>(A, x, 0);
        auto y_e4m3    = spmv_ieee_sim(A, x, q_fp8_e4m3);
        auto y_e5m2    = spmv_ieee_sim(A, x, q_fp8_e5m2);
        auto y_fp16    = spmv_ieee_sim(A, x, q_fp16);
        auto y_fp32    = spmv_ieee_sim(A, x, [](double v){ return (double)(float)v; });

        FILE* f = fopen("spmv_vetor_100.csv", "w");
        fprintf(f, "idx,T8_dot,T16_dot,T32_dot,T8_chain,T16_chain,T32_chain,OFP8_E4M3,OFP8_E5M2,FP16,FP32\n");
        for (int i = 0; i < 100; ++i) {
            if (abs128(y_ref[i]) < (__float128)1e-12) continue;
            fprintf(f, "%d,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e\n",
                    i,
                    calc_erro_rel(y_t8_dot[i], y_ref[i]),
                    calc_erro_rel(y_t16_dot[i], y_ref[i]),
                    calc_erro_rel(y_t32_dot[i], y_ref[i]),
                    calc_erro_rel(y_t8_cad[i], y_ref[i]),
                    calc_erro_rel(y_t16_cad[i], y_ref[i]),
                    calc_erro_rel(y_t32_cad[i], y_ref[i]),
                    calc_erro_rel(y_e4m3[i], y_ref[i]),
                    calc_erro_rel(y_e5m2[i], y_ref[i]),
                    calc_erro_rel(y_fp16[i], y_ref[i]),
                    calc_erro_rel(y_fp32[i], y_ref[i]));
        }
        fclose(f);
        printf("[OK] Arquivo 'spmv_vetor_100.csv' gerado (100x100, d=0.10).\n");
    }

    {
        FILE* fcdf = fopen("spmv_cdf_dados.csv", "w");
        fprintf(fcdf, "regime,T8_dot,T16_dot,T32_dot,T8_chain,T16_chain,T32_chain,T8_strips,T16_strips,T32_strips,OFP8_E4M3,OFP8_E5M2,FP16,FP32\n");

        for (int reg = 0; reg < 2; ++reg) {
            bool ampla = (reg == 1);
            const char* nome_reg = ampla ? "ampla" : "estreita";
            for (int seed = 1; seed <= 15; ++seed) {
                std::mt19937 g(2026 + seed * 17);
                CSR A = gerar_csr(200, 0.10, ampla, g);
                auto x = gerar_vetor(200, ampla, g);
                auto y_ref = spmv_ref128(A, x);

                __float128 max_y = 0;
                for (auto v : y_ref) if (abs128(v) > max_y) max_y = abs128(v);
                __float128 limiar = max_y * (__float128)1e-12;

                auto y_t8_d  = spmv_takum<8>(A, x, 1);
                auto y_t16_d = spmv_takum<16>(A, x, 1);
                auto y_t32_d = spmv_takum<32>(A, x, 1);
                auto y_t8_c  = spmv_takum<8>(A, x, 0);
                auto y_t16_c = spmv_takum<16>(A, x, 0);
                auto y_t32_c = spmv_takum<32>(A, x, 0);
                auto y_t8_s  = spmv_takum<8>(A, x, 2);
                auto y_t16_s = spmv_takum<16>(A, x, 2);
                auto y_t32_s = spmv_takum<32>(A, x, 2);
                auto y_e4m3  = spmv_ieee_sim(A, x, q_fp8_e4m3);
                auto y_e5m2  = spmv_ieee_sim(A, x, q_fp8_e5m2);
                auto y_fp16  = spmv_ieee_sim(A, x, q_fp16);
                auto y_fp32  = spmv_ieee_sim(A, x, [](double v){ return (double)(float)v; });

                for (int i = 0; i < A.nrows; ++i) {
                    if (abs128(y_ref[i]) <= limiar) continue;
                    fprintf(fcdf, "%s,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e\n",
                            nome_reg,
                            calc_erro_rel(y_t8_d[i], y_ref[i]),
                            calc_erro_rel(y_t16_d[i], y_ref[i]),
                            calc_erro_rel(y_t32_d[i], y_ref[i]),
                            calc_erro_rel(y_t8_c[i], y_ref[i]),
                            calc_erro_rel(y_t16_c[i], y_ref[i]),
                            calc_erro_rel(y_t32_c[i], y_ref[i]),
                            calc_erro_rel(y_t8_s[i], y_ref[i]),
                            calc_erro_rel(y_t16_s[i], y_ref[i]),
                            calc_erro_rel(y_t32_s[i], y_ref[i]),
                            calc_erro_rel(y_e4m3[i], y_ref[i]),
                            calc_erro_rel(y_e5m2[i], y_ref[i]),
                            calc_erro_rel(y_fp16[i], y_ref[i]),
                            calc_erro_rel(y_fp32[i], y_ref[i]));
                }
            }
        }
        fclose(fcdf);
        printf("[OK] Arquivo 'spmv_cdf_dados.csv' gerado (15 sementes x 200 linhas x 2 regimes).\n");
    }

    {
        struct Config { const char* nome; int n; double d; bool ampla; };
        Config cfgs[] = {
            {"SpMV_100_d05_Estreita", 100, 0.05, false},
            {"SpMV_100_d10_Estreita", 100, 0.10, false},
            {"SpMV_256_d10_Estreita", 256, 0.10, false},
            {"SpMV_256_d20_Estreita", 256, 0.20, false},
            {"SpMV_256_d10_Ampla",    256, 0.10, true},
            {"GEMV_128_Denso_ML",     128, 1.00, false}
        };

        FILE* fres = fopen("spmv_resumo.csv", "w");
        fprintf(fres, "cenario,formato,mediana,p95\n");

        printf("\n%-22s | %-10s | %-10s | %-10s | %-10s | %-10s | %-10s\n",
               "Cenario", "T8 (vtdot)", "OFP8 E4M3", "T16(vtdot)", "FP16", "T32(vtdot)", "FP32");
        printf("----------------------------------------------------------------------------------------------------\n");

        for (const auto& c : cfgs) {
            std::vector<double> e_t8d, e_t16d, e_t32d, e_t8c, e_t16c, e_t32c, e_t8s, e_t16s, e_t32s, e_e4m3, e_e5m2, e_fp16, e_fp32;
            for (int s = 1; s <= 5; ++s) {
                std::mt19937 g(1000 + s * 31);
                CSR A = gerar_csr(c.n, c.d, c.ampla, g);
                auto x = gerar_vetor(c.n, c.ampla, g);
                auto y_ref = spmv_ref128(A, x);
                __float128 max_y = 0;
                for (auto v : y_ref) if (abs128(v) > max_y) max_y = abs128(v);
                __float128 limiar = max_y * (__float128)1e-12;

                auto y_t8_d  = spmv_takum<8>(A, x, 1);
                auto y_t16_d = spmv_takum<16>(A, x, 1);
                auto y_t32_d = spmv_takum<32>(A, x, 1);
                auto y_t8_c  = spmv_takum<8>(A, x, 0);
                auto y_t16_c = spmv_takum<16>(A, x, 0);
                auto y_t32_c = spmv_takum<32>(A, x, 0);
                auto y_t8_s  = spmv_takum<8>(A, x, 2);
                auto y_t16_s = spmv_takum<16>(A, x, 2);
                auto y_t32_s = spmv_takum<32>(A, x, 2);
                auto y_e4m3  = spmv_ieee_sim(A, x, q_fp8_e4m3);
                auto y_e5m2  = spmv_ieee_sim(A, x, q_fp8_e5m2);
                auto y_fp16  = spmv_ieee_sim(A, x, q_fp16);
                auto y_fp32  = spmv_ieee_sim(A, x, [](double v){ return (double)(float)v; });

                for (int i = 0; i < A.nrows; ++i) {
                    if (abs128(y_ref[i]) <= limiar) continue;
                    e_t8d.push_back(calc_erro_rel(y_t8_d[i], y_ref[i]));
                    e_t16d.push_back(calc_erro_rel(y_t16_d[i], y_ref[i]));
                    e_t32d.push_back(calc_erro_rel(y_t32_d[i], y_ref[i]));
                    e_t8c.push_back(calc_erro_rel(y_t8_c[i], y_ref[i]));
                    e_t16c.push_back(calc_erro_rel(y_t16_c[i], y_ref[i]));
                    e_t32c.push_back(calc_erro_rel(y_t32_c[i], y_ref[i]));
                    e_t8s.push_back(calc_erro_rel(y_t8_s[i], y_ref[i]));
                    e_t16s.push_back(calc_erro_rel(y_t16_s[i], y_ref[i]));
                    e_t32s.push_back(calc_erro_rel(y_t32_s[i], y_ref[i]));
                    e_e4m3.push_back(calc_erro_rel(y_e4m3[i], y_ref[i]));
                    e_e5m2.push_back(calc_erro_rel(y_e5m2[i], y_ref[i]));
                    e_fp16.push_back(calc_erro_rel(y_fp16[i], y_ref[i]));
                    e_fp32.push_back(calc_erro_rel(y_fp32[i], y_ref[i]));
                }
            }
            auto gravar = [&](const char* fmt, const std::vector<double>& v) {
                fprintf(fres, "%s,%s,%.6e,%.6e\n", c.nome, fmt, mediana(v), percentil95(v));
            };
            gravar("T8_dot", e_t8d);     gravar("T16_dot", e_t16d);     gravar("T32_dot", e_t32d);
            gravar("T8_chain", e_t8c);   gravar("T16_chain", e_t16c);   gravar("T32_chain", e_t32c);
            gravar("T8_strips", e_t8s);  gravar("T16_strips", e_t16s);  gravar("T32_strips", e_t32s);
            gravar("OFP8_E4M3", e_e4m3); gravar("OFP8_E5M2", e_e5m2);
            gravar("FP16", e_fp16);      gravar("FP32", e_fp32);

            printf("%-22s | %10.2e | %10.2e | %10.2e | %10.2e | %10.2e | %10.2e\n",
                   c.nome, mediana(e_t8d), mediana(e_e4m3), mediana(e_t16d),
                   mediana(e_fp16), mediana(e_t32d), mediana(e_fp32));
        }
        fclose(fres);
        printf("\n[OK] Arquivo 'spmv_resumo.csv' gerado.\n");
    }
    return 0;
}
