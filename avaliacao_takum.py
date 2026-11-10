#!/usr/bin/env python3
import os
import numpy as np
import matplotlib.pyplot as plt

print("Gerando gráficos científicos do artigo...")

# Gráfico 1
dados_100 = np.genfromtxt("spmv_vetor_100.csv", delimiter=",", names=True)
plt.figure(figsize=(10, 5.5))
plt.plot(dados_100["idx"], dados_100["T8_dot"],    label="Takum T8 (vtdot)",  color="#1f77b4", lw=1.5)
plt.plot(dados_100["idx"], dados_100["OFP8_E4M3"], label="OFP8 E4M3",         color="#1f77b4", lw=1.0, ls="--", alpha=0.6)
plt.plot(dados_100["idx"], dados_100["T16_dot"],   label="Takum T16 (vtdot)", color="#ff7f0e", lw=1.5)
plt.plot(dados_100["idx"], dados_100["FP16"],      label="IEEE FP16",         color="#ff7f0e", lw=1.0, ls="--", alpha=0.6)
plt.plot(dados_100["idx"], dados_100["T32_dot"],   label="Takum T32 (vtdot)", color="#2ca02c", lw=1.5)
plt.plot(dados_100["idx"], dados_100["FP32"],      label="IEEE FP32",         color="#2ca02c", lw=1.0, ls="--", alpha=0.6)
plt.yscale("log")
plt.title("Relative Error in Sparse Matrix-Vector Multiplication (SpMV 100x100, d=10%)")
plt.xlabel("Vector Index (i)")
plt.ylabel("Relative Error vs. Float128 Reference (Log Scale)")
plt.legend(ncol=3, fontsize=9, loc="upper right")
plt.grid(True, which="both", ls="--", alpha=0.4)
plt.tight_layout()
plt.savefig("grafico_erro_takum.png", dpi=300)
plt.close()
print("[OK] 'grafico_erro_takum.png' gerado com sucesso.")

# Gráfico 2
with open("spmv_cdf_dados.csv", "r") as f:
    header = f.readline().strip().split(",")
    cols = {name: i for i, name in enumerate(header)}
    estreita = {k: [] for k in header[1:]}
    ampla    = {k: [] for k in header[1:]}
    for line in f:
        parts = line.strip().split(",")
        if not parts or len(parts) < len(header): continue
        alvo = estreita if parts[0] == "estreita" else ampla
        for k in header[1:]: alvo[k].append(float(parts[cols[k]]))

fig, axes = plt.subplots(1, 2, figsize=(12, 5), sharey=True)
series = [("T8_dot", "Takum T8 (vtdot)", "#1f77b4", "-"), ("OFP8_E4M3", "OFP8 E4M3", "#1f77b4", "--"), ("OFP8_E5M2", "OFP8 E5M2", "#17becf", ":"), ("T16_dot", "Takum T16 (vtdot)", "#ff7f0e", "-"), ("FP16", "IEEE FP16", "#ff7f0e", "--"), ("T32_dot", "Takum T32 (vtdot)", "#2ca02c", "-"), ("FP32", "IEEE FP32", "#2ca02c", "--")]
for ax, dados_reg, titulo in [(axes[0], estreita, "(a) Narrow Dynamic Range: N(0, 1)"), (axes[1], ampla, "(b) Wide Dynamic Range: N(0, 1) * 2^U(-18, 18)")]:
    for chave, rotulo, cor, estilo in series:
        arr = np.sort(np.array(dados_reg[chave]))
        cdf = np.arange(1, len(arr) + 1) / len(arr)
        ax.plot(arr, cdf, label=rotulo, color=cor, ls=estilo, lw=1.8)
    ax.set_xscale("log")
    ax.set_xlim(1e-12, 2.0)
    ax.set_title(titulo, fontsize=11)
    ax.set_xlabel("Relative Error (Log Scale)")
    ax.grid(True, which="both", ls="--", alpha=0.4)
axes[0].set_ylabel("Cumulative Distribution Function (CDF)")
axes[0].legend(fontsize=8.5, loc="lower right")
plt.tight_layout()
plt.savefig("grafico_cdf_spmv.png", dpi=300)
plt.close()
print("[OK] 'grafico_cdf_spmv.png' gerado com sucesso.")

# Gráfico 3
resumo = {}
with open("spmv_resumo.csv", "r") as f:
    f.readline()
    for line in f:
        cen, fmt, med, p95 = line.strip().split(",")
        resumo[(cen, fmt)] = float(med)
cenarios = ["SpMV_256_d10_Estreita", "SpMV_256_d20_Estreita", "GEMV_128_Denso_ML"]
rotulos_cen = ["SpMV 256x256 (d=10%)", "SpMV 256x256 (d=20%)", "Dense GEMV 128x128 (ML)"]
x_pos = np.arange(len(cenarios))
larg = 0.25
fig, axes = plt.subplots(1, 3, figsize=(12, 4.2), sharey=False)
for idx_b, bits in enumerate([8, 16, 32]):
    ax = axes[idx_b]
    v_chain  = [resumo[(c, f"T{bits}_chain")]  for c in cenarios]
    v_strips = [resumo[(c, f"T{bits}_strips")] for c in cenarios]
    v_dot    = [resumo[(c, f"T{bits}_dot")]    for c in cenarios]
    ax.bar(x_pos - larg, v_chain,  larg, label="Chain (vtmul+vtadd)", color="#d62728", alpha=0.85)
    ax.bar(x_pos,        v_strips, larg, label="vtdot (Strip-Mined)", color="#ff7f0e", alpha=0.85)
    ax.bar(x_pos + larg, v_dot,    larg, label="vtdot (Full Acc)",    color="#2ca02c", alpha=0.85)
    ax.set_yscale("log")
    ax.set_title(f"Takum T{bits}")
    ax.set_xticks(x_pos)
    ax.set_xticklabels(rotulos_cen, rotation=15, fontsize=8.5)
    ax.grid(True, axis="y", which="both", ls="--", alpha=0.4)
    if idx_b == 0:
        ax.set_ylabel("Median Relative Error (Log Scale)")
        ax.legend(fontsize=8)
plt.tight_layout()
plt.savefig("grafico_dot_vs_chain.png", dpi=300)
plt.close()
print("[OK] 'grafico_dot_vs_chain.png' gerado com sucesso.")
