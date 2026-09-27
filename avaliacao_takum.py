import numpy as np
from scipy.sparse import random

# ==========================================
# 1. Configurações do Ambiente de Teste
# ==========================================
dimensao = 100  # Matriz de 100x100
densidade = 0.1 # Apenas 10% da matriz terá números (90% são zeros)

print(f"--- Iniciando Teste SpMV ---")
print(f"Dimensão da Matriz: {dimensao}x{dimensao}")
print(f"Densidade: {densidade*100}% (Esparsa)")

# ==========================================
# 2. Geração dos Dados
# ==========================================
# Correção: Utilizamos uma função lambda para garantir que o tamanho do vetor gerado está correto
matriz_esparsa = random(dimensao, dimensao, density=densidade, format='csr', data_rvs=lambda n: np.random.randn(n))

# Cria o vetor multiplicador (denso)
vetor = np.random.randn(dimensao)

# ==========================================
# 3. O Padrão de Ouro (IEEE 754 - Float128)
# ==========================================
# Converte os dados para a precisão máxima suportada nativamente no Linux (Float128)
matriz_f128 = matriz_esparsa.astype(np.float128)
vetor_f128 = vetor.astype(np.float128)

# Executa a Multiplicação Matriz-Vetor (@)
resultado_ouro = matriz_f128 @ vetor_f128

# ==========================================
# 4. Exibição dos Resultados
# ==========================================
print("\n[SUCESSO] Multiplicação base em Float128 concluída!")
print("\nAmostra dos 5 primeiros valores do Vetor Resultado:")
for i in range(5):
    print(f"Índice {i}: {resultado_ouro[i]}")

    import matplotlib.pyplot as plt

# ==========================================
# 5. Simulação do Formato Takum e Cálculo de Erro
# ==========================================
def simular_takum(matriz, bits):
    # Simulação da perda de precisão e quantização do formato Takum
    niveis = 2 ** (bits - 2)
    return np.round(matriz * niveis) / niveis

# Simulação da SpMV para os tamanhos T8, T16 e T32
resultado_T8 = simular_takum(matriz_esparsa, 8) @ simular_takum(vetor, 8)
resultado_T16 = simular_takum(matriz_esparsa, 16) @ simular_takum(vetor, 16)
resultado_T32 = simular_takum(matriz_esparsa, 32) @ simular_takum(vetor, 32)

# Cálculo do Erro Relativo: |(Valor_Exato - Valor_Aproximado) / Valor_Exato|
erro_T8 = np.abs((resultado_ouro - resultado_T8) / resultado_ouro)
erro_T16 = np.abs((resultado_ouro - resultado_T16) / resultado_ouro)
erro_T32 = np.abs((resultado_ouro - resultado_T32) / resultado_ouro)

print("\n--- Erro Relativo Médio ---")
print(f"Takum T8:  {np.mean(erro_T8):.6f}")
print(f"Takum T16: {np.mean(erro_T16):.6f}")
print(f"Takum T32: {np.mean(erro_T32):.6f}")

# ==========================================
# 6. Geração de Gráficos para o Artigo
# ==========================================
plt.figure(figsize=(10, 6))
plt.plot(erro_T8, label="Takum T8 (8 bits)", alpha=0.7)
plt.plot(erro_T16, label="Takum T16 (16 bits)", alpha=0.7)
plt.plot(erro_T32, label="Takum T32 (32 bits)", alpha=0.7)
plt.yscale('log')
plt.title("Erro Relativo na Multiplicação de Matriz Esparsa (SpMV)")
plt.xlabel("Índice do Vetor")
plt.ylabel("Erro Relativo (Escala Logarítmica)")
plt.legend()
plt.grid(True, which="both", ls="--", alpha=0.5)
plt.savefig("grafico_erro_takum.png")
print("\n[SUCESSO] Gráfico 'grafico_erro_takum.png' gerado para o artigo!")