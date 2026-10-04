#!/usr/bin/env python3
import glob
import math
import os
import re

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

BENCH_DIR = "resultados_benchmark"
PLOTS_DIR = "graficos"
os.makedirs(PLOTS_DIR, exist_ok=True)


def theoretical_mean_distance(dim: int, steps: int) -> float:
    """Aproximación asintótica: TCL multivariado + distribución Chi_D."""
    half_d = 0.5 * dim
    gamma_ratio = math.exp(math.lgamma(half_d + 0.5) - math.lgamma(half_d))
    return math.sqrt(2.0 * steps / dim) * gamma_ratio


def load_metrics_by_dimension():
    result = {}
    pattern = os.path.join(BENCH_DIR, "metricas_random_walk_*d.csv")
    for path in sorted(glob.glob(pattern)):
        df = pd.read_csv(path)
        if df.empty:
            continue

        if "dimension" in df.columns:
            dim = int(df["dimension"].iloc[0])
        else:
            match = re.search(r"_(\d+)d\.csv$", path)
            if not match:
                continue
            dim = int(match.group(1))

        result[dim] = (path, df)
    return result


def plot_performance(metrics):
    for dim, (_, df) in sorted(metrics.items()):
        procs = df["procesos"].to_numpy(dtype=float)
        speedup = df["speedup_paralelo"].to_numpy(dtype=float)
        efficiency = df["eficiencia"].to_numpy(dtype=float)

        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5), dpi=180)

        ax1.plot(procs, speedup, "o-", linewidth=2.2, label=f"Speedup paralelo ({dim}D)")
        if "speedup_global" in df.columns and df["speedup_global"].notna().any():
            ax1.plot(
                procs,
                df["speedup_global"].to_numpy(dtype=float),
                "s-.",
                linewidth=2.0,
                label="Speedup global",
            )
        ax1.plot(procs, procs, "--", linewidth=1.5, label="Ideal: $S_p=p$")
        ax1.set_xlabel("Cantidad de procesos ($p$)")
        ax1.set_ylabel("Factor de aceleración")
        ax1.set_title(f"Aceleración - Random Walk {dim}D")
        ax1.set_xticks(procs.astype(int))
        ax1.grid(True, linestyle=":", alpha=0.6)
        ax1.legend()

        ax2.plot(procs, efficiency, "s-", linewidth=2.2, label="Eficiencia experimental")
        ax2.axhline(1.0, linestyle="--", linewidth=1.5, label="Ideal: 100 %")
        ax2.set_xlabel("Cantidad de procesos ($p$)")
        ax2.set_ylabel("Eficiencia ($S_p/p$)")
        ax2.set_title(f"Eficiencia paralela - Random Walk {dim}D")
        ax2.set_xticks(procs.astype(int))
        ax2.set_ylim(0.0, max(1.1, float(np.nanmax(efficiency)) * 1.05))
        ax2.grid(True, linestyle=":", alpha=0.6)
        ax2.legend()

        fig.suptitle(f"Evaluación HPC - Random Walk {dim}D")
        fig.tight_layout()
        out = os.path.join(PLOTS_DIR, f"rendimiento_hpc_{dim}d.png")
        fig.savefig(out, bbox_inches="tight")
        plt.close(fig)
        print(f"-> Gráfico guardado: {out}")


def plot_combined_speedup(metrics):
    if len(metrics) < 2:
        return

    fig, ax = plt.subplots(figsize=(8, 5), dpi=180)
    max_p = 1
    all_p = set()

    for dim, (_, df) in sorted(metrics.items()):
        procs = df["procesos"].to_numpy(dtype=float)
        speedup = df["speedup_paralelo"].to_numpy(dtype=float)
        ax.plot(procs, speedup, "o-", linewidth=2.0, label=f"{dim}D")
        max_p = max(max_p, int(np.max(procs)))
        all_p.update(int(p) for p in procs)

    ideal = np.array(sorted(all_p), dtype=float)
    ax.plot(ideal, ideal, "--", linewidth=1.5, label="Ideal: $S_p=p$")
    ax.set_xlabel("Cantidad de procesos ($p$)")
    ax.set_ylabel("Speedup paralelo ($S_p$)")
    ax.set_title("Comparación de escalabilidad entre dimensiones")
    ax.set_xticks(sorted(all_p))
    ax.grid(True, linestyle=":", alpha=0.6)
    ax.legend()
    fig.tight_layout()

    out = os.path.join(PLOTS_DIR, "comparacion_speedup_dimensiones.png")
    fig.savefig(out, bbox_inches="tight")
    plt.close(fig)
    print(f"-> Gráfico guardado: {out}")


def steps_for_dim(metrics, dim):
    if dim not in metrics:
        return None
    df = metrics[dim][1]
    if "steps" not in df.columns or df.empty:
        return None
    return int(df["steps"].iloc[0])


def validate_1d(metrics):
    path = os.path.join(BENCH_DIR, "posiciones_1d.csv")
    steps = steps_for_dim(metrics, 1)
    if not os.path.exists(path) or steps is None:
        return

    df = pd.read_csv(path)
    sigma = math.sqrt(steps)
    x = np.linspace(-4.0 * sigma, 4.0 * sigma, 600)
    normal_pdf = np.exp(-0.5 * (x / sigma) ** 2) / (sigma * math.sqrt(2.0 * math.pi))

    fig, ax = plt.subplots(figsize=(9, 5), dpi=180)
    ax.hist(df["x"], bins=50, density=True, alpha=0.65, edgecolor="black", label="Muestra experimental")
    ax.plot(x, normal_pdf, "--", linewidth=2.2, label=fr"Normal aprox. $\sigma=\sqrt{{{steps}}}$")
    ax.set_title("Random Walk 1D: distribución de la posición final")
    ax.set_xlabel("Posición final ($x$)")
    ax.set_ylabel("Densidad de probabilidad")
    ax.grid(True, linestyle=":", alpha=0.5)
    ax.legend()
    fig.tight_layout()

    out = os.path.join(PLOTS_DIR, "histograma_1d.png")
    fig.savefig(out, bbox_inches="tight")
    plt.close(fig)
    print(f"-> Gráfico guardado: {out}")


def validate_2d(metrics):
    path = os.path.join(BENCH_DIR, "posiciones_2d.csv")
    steps = steps_for_dim(metrics, 2)
    if not os.path.exists(path) or steps is None:
        return

    df = pd.read_csv(path)
    mean_r = theoretical_mean_distance(2, steps)

    fig, ax = plt.subplots(figsize=(8, 7), dpi=180)
    hb = ax.hexbin(df["x"], df["y"], gridsize=40, mincnt=1)
    cb = fig.colorbar(hb, ax=ax)
    cb.set_label("Frecuencia de ocurrencia")

    circle = plt.Circle((0, 0), mean_r, fill=False, linestyle="--", linewidth=2.0,
                        label=fr"Radio $E[R]\approx {mean_r:.2f}$")
    ax.add_patch(circle)
    ax.set_title("Random Walk 2D: densidad espacial de posiciones finales")
    ax.set_xlabel("Coordenada X")
    ax.set_ylabel("Coordenada Y")
    ax.set_aspect("equal", adjustable="box")
    ax.legend(loc="upper right")
    fig.tight_layout()

    out = os.path.join(PLOTS_DIR, "densidad_2d.png")
    fig.savefig(out, bbox_inches="tight")
    plt.close(fig)
    print(f"-> Gráfico guardado: {out}")


def validate_nd_radial(metrics):
    rows = []

    for dim, (_, df_metrics) in sorted(metrics.items()):
        path = os.path.join(BENCH_DIR, f"posiciones_{dim}d.csv")
        if not os.path.exists(path):
            continue

        steps = int(df_metrics["steps"].iloc[0])
        df = pd.read_csv(path)
        if "distancia" not in df.columns or df.empty:
            continue

        distances = df["distancia"].to_numpy(dtype=float)
        experimental = float(np.mean(distances))
        theoretical = theoretical_mean_distance(dim, steps)
        rel_error = abs(experimental - theoretical) / theoretical * 100.0

        rows.append({
            "dimension": dim,
            "steps": steps,
            "muestras": len(distances),
            "media_experimental": experimental,
            "media_teorica_aprox": theoretical,
            "error_relativo_pct": rel_error,
        })

        # Para D>=3 se agrega una validación radial específica nD.
        if dim < 3:
            continue

        sigma = math.sqrt(steps / dim)
        r_max = max(float(np.max(distances)), theoretical * 2.0)
        r = np.linspace(max(1e-9, r_max / 2000.0), r_max, 700)

        log_coeff = -((dim / 2.0 - 1.0) * math.log(2.0) + math.lgamma(dim / 2.0) + dim * math.log(sigma))
        log_pdf = log_coeff + (dim - 1.0) * np.log(r) - (r * r) / (2.0 * sigma * sigma)
        chi_pdf = np.exp(log_pdf)

        fig, ax = plt.subplots(figsize=(9, 5), dpi=180)
        ax.hist(distances, bins=50, density=True, alpha=0.65, edgecolor="black", label="Muestra experimental")
        ax.plot(r, chi_pdf, "--", linewidth=2.2, label=fr"Aprox. $\chi_{{{dim}}}$ escalada")
        ax.axvline(theoretical, linestyle=":", linewidth=2.0, label=fr"$E[R]\approx {theoretical:.2f}$")
        ax.set_title(f"Random Walk {dim}D: distribución radial")
        ax.set_xlabel("Distancia final al origen ($R$)")
        ax.set_ylabel("Densidad de probabilidad")
        ax.grid(True, linestyle=":", alpha=0.5)
        ax.legend()
        fig.tight_layout()

        out = os.path.join(PLOTS_DIR, f"distribucion_radial_{dim}d.png")
        fig.savefig(out, bbox_inches="tight")
        plt.close(fig)
        print(f"-> Gráfico guardado: {out}")

    if rows:
        summary = pd.DataFrame(rows)
        out_csv = os.path.join(BENCH_DIR, "resumen_validacion_estocastica.csv")
        summary.to_csv(out_csv, index=False)
        print(f"-> Resumen de validación guardado: {out_csv}")


def main():
    metrics = load_metrics_by_dimension()
    if not metrics:
        print("No se encontraron métricas. Ejecute primero el benchmark.")
        return

    plot_performance(metrics)
    plot_combined_speedup(metrics)
    validate_1d(metrics)
    validate_2d(metrics)
    validate_nd_radial(metrics)


if __name__ == "__main__":
    main()
