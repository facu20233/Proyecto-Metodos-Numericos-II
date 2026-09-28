import os
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy.stats import norm

plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')
fig_dir = "graficos"
os.makedirs(fig_dir, exist_ok=True)

# ------------------------------------------------------------------------------
# 1. HISTOGRAMA 1D
# ------------------------------------------------------------------------------
if os.path.exists("posiciones_1d.csv"):
    df_1d = pd.read_csv("posiciones_1d.csv")
    x_vals = df_1d["x"].values
    N_steps = 1000

    plt.figure(figsize=(8, 5))
    count, bins, _ = plt.hist(x_vals, bins=60, density=True, alpha=0.6, color='royalblue', edgecolor='black', label="Muestra Experimental")
    sigma = np.sqrt(N_steps)
    x_axis = np.linspace(bins.min(), bins.max(), 300)
    plt.plot(x_axis, norm.pdf(x_axis, 0, sigma), 'r--', lw=2.5, 
             label=rf"Gaussiana Teórica ($\sigma=\sqrt{{{N_steps}}} \approx {sigma:.1f}$)")
    plt.title("Random Walk 1D: Distribución Espacial de Posición Final", fontsize=13, fontweight='bold')
    plt.xlabel(r"Posición Final ($x$)", fontsize=11)
    plt.ylabel("Densidad de Probabilidad", fontsize=11)
    plt.legend(frameon=True)
    plt.tight_layout()
    plt.savefig(f"{fig_dir}/histograma_1d.png", dpi=300)
    plt.close()

# ------------------------------------------------------------------------------
# 2. DENSIDAD ESPACIAL 2D
# ------------------------------------------------------------------------------
if os.path.exists("posiciones_2d.csv"):
    df_2d = pd.read_csv("posiciones_2d.csv")
    plt.figure(figsize=(7, 6))
    hb = plt.hexbin(df_2d["x"].values, df_2d["y"].values, gridsize=50, cmap='inferno', mincnt=1)
    cb = plt.colorbar(hb)
    cb.set_label("Frecuencia de Ocurrencia", fontsize=10)
    r_mean = 0.8862 * np.sqrt(1000)
    circle = plt.Circle((0, 0), r_mean, color='cyan', fill=False, linestyle='--', lw=2, 
                        label=rf"Radio Teórico ($R \approx {r_mean:.1f}$)")
    plt.gca().add_patch(circle)
    plt.title("Random Walk 2D: Densidad Espacial de Posiciones Finales", fontsize=13, fontweight='bold')
    plt.xlabel(r"Coordenada $X$", fontsize=11)
    plt.ylabel(r"Coordenada $Y$", fontsize=11)
    plt.legend(loc="upper right", frameon=True)
    plt.axis('equal')
    plt.tight_layout()
    plt.savefig(f"{fig_dir}/densidad_2d.png", dpi=300)
    plt.close()

# ------------------------------------------------------------------------------
# 3. COMPARATIVA BASELINE VS. OPTIMIZADO (BARRAS DE TIEMPO)
# ------------------------------------------------------------------------------
for model in ["1d", "2d"]:
    t_base_file = f"resultados_benchmark/t_base_{model}.txt"
    csv_file = f"resultados_benchmark/metricas_random_walk_{model}.csv"

    if os.path.exists(t_base_file) and os.path.exists(csv_file):
        with open(t_base_file, "r") as f:
            t_base = float(f.read().strip())
        df = pd.read_csv(csv_file)
        t_opt_1p = df.loc[df["procesos"] == 1, "tiempo_mediana"].values[0]
        ganancia_algo = t_base / t_opt_1p

        plt.figure(figsize=(6, 5))
        bars = plt.bar(["Baseline\n(gcc -O0, rand)", "Optimizado p=1\n(PCG32, -O3, Bitwise)"], 
                       [t_base, t_opt_1p], color=['dimgray', 'forestgreen'], width=0.5, edgecolor='black')
        plt.ylabel("Tiempo de Ejecución (segundos)", fontsize=11)
        plt.title(f"Impacto de la Optimización Monoproceso ({model.upper()})\nGanancia Algorítmica: {ganancia_algo:.2f}x", 
                  fontsize=12, fontweight='bold')
        for bar in bars:
            yval = bar.get_height()
            plt.text(bar.get_x() + bar.get_width()/2.0, yval + (t_base*0.02), f"{yval:.2f} s", ha='center', va='bottom', fontweight='bold')
        plt.tight_layout()
        plt.savefig(f"{fig_dir}/comparativa_baseline_{model}.png", dpi=300)
        plt.close()

# ------------------------------------------------------------------------------
# 4. RENDIMIENTO HPC: SPEEDUP PARALELO, GLOBAL Y EFICIENCIA
# ------------------------------------------------------------------------------
for model in ["1d", "2d"]:
    csv_file = f"resultados_benchmark/metricas_random_walk_{model}.csv"
    if os.path.exists(csv_file):
        df_m = pd.read_csv(csv_file)
        p = df_m["procesos"].values
        sp_par = df_m["speedup_paralelo"].values
        sp_glob = df_m["speedup_global"].values
        ep = df_m["eficiencia"].values

        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))

        # Speedups
        ax1.plot(p, sp_par, 'o-', color='darkblue', lw=2, ms=6, label=r"Speedup Paralelo ($S_p = T_1 / T_p$)")
        ax1.plot(p, sp_glob, 'd-.', color='darkgreen', lw=2, ms=6, label=r"Speedup Global ($S_{global} = T_{base} / T_p$)")
        ax1.plot(p, p, 'k--', lw=1.5, label=r"Speedup Lineal Ideal ($S_p = p$)")
        ax1.set_title(f"Curvas de Aceleración ({model.upper()})", fontsize=12, fontweight='bold')
        ax1.set_xlabel(r"Cantidad de Procesos ($p$)", fontsize=10)
        ax1.set_ylabel("Factor de Aceleración", fontsize=10)
        ax1.set_xticks(p)
        ax1.legend(frameon=True)
        ax1.grid(True, linestyle=':')

        # Eficiencia
        ax2.plot(p, ep, 's-', color='crimson', lw=2, ms=6, label=r"Eficiencia Experimental ($E_p$)")
        ax2.axhline(1.0, color='k', linestyle='--', lw=1.5, label="Eficiencia Ideal (100%)")
        ax2.set_title(f"Eficiencia Paralela ({model.upper()})", fontsize=12, fontweight='bold')
        ax2.set_xlabel(r"Cantidad de Procesos ($p$)", fontsize=10)
        ax2.set_ylabel(r"Eficiencia ($S_p / p$)", fontsize=10)
        ax2.set_ylim(0, 1.15)
        ax2.set_xticks(p)
        ax2.legend(frameon=True)
        ax2.grid(True, linestyle=':')

        plt.suptitle(f"Evaluación Integral HPC - Random Walk {model.upper()}", fontsize=14, fontweight='bold', y=1.02)
        plt.tight_layout()
        plt.savefig(f"{fig_dir}/rendimiento_hpc_{model}.png", dpi=300, bbox_inches='tight')
        plt.close()

print("-> Todos los gráficos generados exitosamente en la carpeta 'graficos/'.")