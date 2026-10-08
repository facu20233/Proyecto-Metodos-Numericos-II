#!/usr/bin/env python3
import argparse
import glob
import math
import os
import re

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def theoretical_mean_distance(dim: int, steps: int) -> float:
    half_d = 0.5 * dim
    gamma_ratio = math.exp(math.lgamma(half_d + 0.5) - math.lgamma(half_d))
    return math.sqrt(2.0 * steps / dim) * gamma_ratio


def load_metrics_by_dimension(bench_dir):
    result = {}
    pattern = os.path.join(bench_dir, "metricas_random_walk_*d.csv")
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


def save(fig, path):
    fig.tight_layout()
    fig.savefig(path, dpi=220, bbox_inches="tight")
    plt.close(fig)
    print(f"-> Gráfico guardado: {path}")


def plot_per_dimension(metrics, plots_dir):
    """Cada dimensión: tiempo absoluto + speedup + eficiencia."""
    for dim, (_, df) in sorted(metrics.items()):
        p = df["procesos"].to_numpy(dtype=int)
        time = df["tiempo_mediana"].to_numpy(dtype=float)
        speedup = df["speedup_paralelo"].to_numpy(dtype=float)
        efficiency_pct = 100.0 * df["eficiencia"].to_numpy(dtype=float)

        fig, axes = plt.subplots(1, 3, figsize=(16, 4.8))
        ax = axes[0]
        ax.plot(p, time, "o-", linewidth=2)
        ax.set_title("Tiempo mediano")
        ax.set_xlabel("Procesos ($p$)")
        ax.set_ylabel("Tiempo [s]")
        ax.set_xticks(p)
        ax.grid(True, linestyle=":", alpha=0.5)

        ax = axes[1]
        ax.plot(p, speedup, "o-", linewidth=2, label="Experimental")
        ax.plot(p, p, "--", linewidth=1.5, label="Ideal $S_p=p$")
        ax.set_title("Speedup paralelo")
        ax.set_xlabel("Procesos ($p$)")
        ax.set_ylabel("$S_p=T_1/T_p$")
        ax.set_xticks(p)
        ax.grid(True, linestyle=":", alpha=0.5)
        ax.legend()

        ax = axes[2]
        ax.plot(p, efficiency_pct, "s-", linewidth=2, label="Experimental")
        ax.axhline(100.0, linestyle="--", linewidth=1.5, label="Ideal")
        ax.set_title("Eficiencia paralela")
        ax.set_xlabel("Procesos ($p$)")
        ax.set_ylabel("Eficiencia [%]")
        ax.set_xticks(p)
        ax.grid(True, linestyle=":", alpha=0.5)
        ax.legend()

        steps = int(df["steps"].iloc[0]) if "steps" in df.columns else None
        walks = int(df["total_walks"].iloc[0]) if "total_walks" in df.columns else None
        suffix = f" — M={steps}, N={walks:,}" if steps and walks else ""
        fig.suptitle(f"Strong scaling — Random Walk {dim}D{suffix}")
        save(fig, os.path.join(plots_dir, f"rendimiento_hpc_{dim}d.png"))


def common_processes(metrics):
    sets = [set(df["procesos"].astype(int)) for _, df in metrics.values()]
    return sorted(set.intersection(*sets)) if sets else []


def plot_grouped_metric(metrics, column, ylabel, title, filename, plots_dir, percentage=False, ideal=None):
    if len(metrics) < 2:
        return
    processes = common_processes(metrics)
    if not processes:
        return

    dims = sorted(metrics)
    x = np.arange(len(processes), dtype=float)
    total_width = 0.78
    width = total_width / len(dims)

    fig, ax = plt.subplots(figsize=(10, 5.6))
    for i, dim in enumerate(dims):
        df = metrics[dim][1].set_index("procesos")
        values = np.array([float(df.loc[p, column]) for p in processes])
        if percentage:
            values *= 100.0
        offset = (i - (len(dims) - 1) / 2.0) * width
        ax.bar(x + offset, values, width=width * 0.92, label=f"{dim}D")

    if ideal is not None:
        if ideal == "speedup":
            ax.plot(x, processes, "k--", linewidth=1.5, marker="o", label="Ideal $S_p=p$")
        elif ideal == "efficiency":
            ax.axhline(100.0, linestyle="--", linewidth=1.5, label="Ideal 100 %")

    ax.set_xticks(x, [str(p) for p in processes])
    ax.set_xlabel("Cantidad de procesos ($p$)")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.grid(True, axis="y", linestyle=":", alpha=0.5)
    ax.legend()
    save(fig, os.path.join(plots_dir, filename))


def plot_combined_performance(metrics, plots_dir):
    plot_grouped_metric(
        metrics, "speedup_paralelo", "Speedup paralelo ($S_p$)",
        "Comparación de strong scaling entre dimensiones",
        "comparacion_speedup_dimensiones.png", plots_dir, ideal="speedup"
    )
    plot_grouped_metric(
        metrics, "eficiencia", "Eficiencia [%]",
        "Comparación de eficiencia entre dimensiones",
        "comparacion_eficiencia_dimensiones.png", plots_dir,
        percentage=True, ideal="efficiency"
    )

    fig, ax = plt.subplots(figsize=(9, 5.5))
    for dim, (_, df) in sorted(metrics.items()):
        ax.plot(df["procesos"], df["tiempo_mediana"], "o-", linewidth=2, label=f"{dim}D")
    ax.set_xlabel("Cantidad de procesos ($p$)")
    ax.set_ylabel("Tiempo mediano [s]")
    ax.set_title("Tiempo absoluto por dimensión")
    ax.set_yscale("log")
    ax.grid(True, which="both", linestyle=":", alpha=0.5)
    ax.legend()
    save(fig, os.path.join(plots_dir, "comparacion_tiempos_dimensiones.png"))

    # El speedup global sólo existe donde hay baseline comparable (1D y 2D).
    fig, ax = plt.subplots(figsize=(8.5, 5.2))
    plotted = False
    for dim, (_, df) in sorted(metrics.items()):
        if "speedup_global" not in df.columns or not df["speedup_global"].notna().any():
            continue
        vals = pd.to_numeric(df["speedup_global"], errors="coerce")
        if vals.notna().any():
            ax.plot(df["procesos"], vals, "o-", linewidth=2, label=f"{dim}D")
            plotted = True
    if plotted:
        ax.set_xlabel("Cantidad de procesos ($p$)")
        ax.set_ylabel("Speedup global ($T_{base}/T_p$)")
        ax.set_title("Aceleración total: optimización secuencial + MPI")
        ax.grid(True, linestyle=":", alpha=0.5)
        ax.legend()
        save(fig, os.path.join(plots_dir, "speedup_global_1d_2d.png"))
    else:
        plt.close(fig)


def load_sample_steps(bench_dir, dim, metrics):
    meta_path = os.path.join(bench_dir, "muestras_config.csv")
    if os.path.exists(meta_path):
        meta = pd.read_csv(meta_path)
        row = meta[meta["dimension"] == dim]
        if not row.empty:
            return int(row["steps"].iloc[0])
    if dim in metrics and "steps" in metrics[dim][1].columns:
        return int(metrics[dim][1]["steps"].iloc[0])
    return None


def validate_1d(bench_dir, plots_dir, metrics):
    path = os.path.join(bench_dir, "posiciones_1d.csv")
    steps = load_sample_steps(bench_dir, 1, metrics)
    if not os.path.exists(path) or steps is None:
        return None

    df = pd.read_csv(path)
    sigma = math.sqrt(steps)
    x = np.linspace(-4.2 * sigma, 4.2 * sigma, 700)
    normal_pdf = np.exp(-0.5 * (x / sigma) ** 2) / (sigma * math.sqrt(2.0 * math.pi))

    fig, ax = plt.subplots(figsize=(9, 5.2))
    ax.hist(df["x"], bins=55, density=True, alpha=0.7, edgecolor="black", label="Probabilidad experimental")
    ax.plot(x, normal_pdf, "--", linewidth=2.2, label=fr"Aprox. normal: $\sigma=\sqrt{{{steps}}}$")
    ax.set_title("Random Walk 1D — histograma de probabilidad de la posición final")
    ax.set_xlabel("Posición final ($x$)")
    ax.set_ylabel("Densidad de probabilidad")
    ax.grid(True, linestyle=":", alpha=0.45)
    ax.legend()
    save(fig, os.path.join(plots_dir, "histograma_probabilidad_1d.png"))

    experimental = float(df["distancia"].mean())
    theoretical = theoretical_mean_distance(1, steps)
    return 1, steps, len(df), experimental, theoretical


def validate_2d(bench_dir, plots_dir, metrics):
    path = os.path.join(bench_dir, "posiciones_2d.csv")
    steps = load_sample_steps(bench_dir, 2, metrics)
    if not os.path.exists(path) or steps is None:
        return None

    df = pd.read_csv(path)
    mean_r = theoretical_mean_distance(2, steps)

    fig, ax = plt.subplots(figsize=(8, 7))
    h = ax.hist2d(df["x"], df["y"], bins=48, density=True)
    cb = fig.colorbar(h[3], ax=ax)
    cb.set_label("Densidad de probabilidad")
    circle = plt.Circle((0, 0), mean_r, fill=False, linestyle="--", linewidth=2,
                        label=fr"$E[R]\approx {mean_r:.2f}$")
    ax.add_patch(circle)
    ax.set_title("Random Walk 2D — histograma 2D de la posición final")
    ax.set_xlabel("Coordenada $x$")
    ax.set_ylabel("Coordenada $y$")
    ax.set_aspect("equal", adjustable="box")
    ax.legend(loc="upper right")
    save(fig, os.path.join(plots_dir, "histograma_probabilidad_2d.png"))

    experimental = float(df["distancia"].mean())
    theoretical = mean_r
    return 2, steps, len(df), experimental, theoretical


def validate_nd_radial(bench_dir, plots_dir, metrics):
    rows = []
    for dim in sorted(metrics):
        if dim < 3:
            continue
        path = os.path.join(bench_dir, f"posiciones_{dim}d.csv")
        steps = load_sample_steps(bench_dir, dim, metrics)
        if not os.path.exists(path) or steps is None:
            continue

        df = pd.read_csv(path)
        if "distancia" not in df.columns or df.empty:
            continue
        distances = df["distancia"].to_numpy(dtype=float)
        experimental = float(np.mean(distances))
        theoretical = theoretical_mean_distance(dim, steps)
        rows.append((dim, steps, len(df), experimental, theoretical))

        sigma = math.sqrt(steps / dim)
        r_max = max(float(np.max(distances)), theoretical * 2.0)
        r = np.linspace(max(1e-9, r_max / 2500.0), r_max, 800)
        log_coeff = -((dim / 2.0 - 1.0) * math.log(2.0) + math.lgamma(dim / 2.0) + dim * math.log(sigma))
        log_pdf = log_coeff + (dim - 1.0) * np.log(r) - (r * r) / (2.0 * sigma * sigma)

        fig, ax = plt.subplots(figsize=(9, 5.2))
        ax.hist(distances, bins=55, density=True, alpha=0.7, edgecolor="black", label="Muestra experimental")
        ax.plot(r, np.exp(log_pdf), "--", linewidth=2.2, label=fr"Aprox. $\chi_{{{dim}}}$ escalada")
        ax.axvline(theoretical, linestyle=":", linewidth=2, label=fr"$E[R]\approx {theoretical:.2f}$")
        ax.set_title(f"Random Walk {dim}D — distribución de la distancia final")
        ax.set_xlabel("Distancia al origen ($R$)")
        ax.set_ylabel("Densidad de probabilidad")
        ax.grid(True, linestyle=":", alpha=0.45)
        ax.legend()
        save(fig, os.path.join(plots_dir, f"distribucion_radial_{dim}d.png"))
    return rows


def plot_distance_vs_steps(bench_dir, plots_dir):
    path = os.path.join(bench_dir, "distancia_vs_pasos.csv")
    if not os.path.exists(path):
        print("Aviso: no existe distancia_vs_pasos.csv; se omite E[R] vs M.")
        return
    df = pd.read_csv(path)
    if df.empty:
        return

    def make_plot(dims, filename, title):
        selected = df[df["dimension"].isin(dims)]
        if selected.empty:
            return
        fig, ax = plt.subplots(figsize=(9, 5.5))
        for dim in sorted(selected["dimension"].unique()):
            part = selected[selected["dimension"] == dim].sort_values("steps")
            line = ax.plot(part["steps"], part["distancia_promedio"], "o-", linewidth=2,
                           label=f"{dim}D experimental")[0]
            ax.plot(part["steps"], part["distancia_teorica"], "--", linewidth=1.8,
                    color=line.get_color(), label=f"{dim}D teórica")
        ax.set_xlabel("Cantidad de pasos por caminata ($M$)")
        ax.set_ylabel("Distancia promedio al origen $E[R]$")
        ax.set_title(title)
        ax.grid(True, linestyle=":", alpha=0.5)
        ax.legend(ncol=2)
        save(fig, os.path.join(plots_dir, filename))

    make_plot([1, 2], "distancia_vs_pasos_1d_2d.png",
              "Distancia promedio en función de los pasos — casos solicitados")
    nd_dims = sorted(d for d in df["dimension"].unique() if d >= 3)
    if nd_dims:
        make_plot(nd_dims, "distancia_vs_pasos_nd.png",
                  "Distancia promedio en función de los pasos — generalización nD")

    fig, ax = plt.subplots(figsize=(8.5, 5))
    for dim in sorted(df["dimension"].unique()):
        part = df[df["dimension"] == dim].sort_values("steps")
        ax.plot(part["steps"], part["error_relativo_pct"], "o-", label=f"{dim}D")
    ax.set_xlabel("Cantidad de pasos ($M$)")
    ax.set_ylabel("Error relativo respecto a teoría [%]")
    ax.set_title("Validación físico-estocástica del modelo")
    ax.grid(True, linestyle=":", alpha=0.5)
    ax.legend()
    save(fig, os.path.join(plots_dir, "error_teorico_vs_pasos.png"))


def write_validation_summary(bench_dir, validations):
    rows = []
    for item in validations:
        if item is None:
            continue
        dim, steps, samples, experimental, theoretical = item
        error = abs(experimental - theoretical) / theoretical * 100.0
        rows.append({
            "dimension": dim,
            "steps": steps,
            "muestras": samples,
            "media_experimental": experimental,
            "media_teorica_aprox": theoretical,
            "error_relativo_pct": error,
        })
    if rows:
        path = os.path.join(bench_dir, "resumen_validacion_estocastica.csv")
        pd.DataFrame(rows).sort_values("dimension").to_csv(path, index=False)
        print(f"-> Resumen de validación guardado: {path}")


def parse_compare_specs(specs):
    result = {}
    for spec in specs or []:
        if "=" not in spec:
            raise ValueError(f"Formato inválido para --compare: {spec}. Use ETIQUETA=CARPETA")
        label, path = spec.split("=", 1)
        result[label] = load_metrics_by_dimension(path)
    return result


def plot_system_comparison(compare_data, plots_dir):
    if len(compare_data) < 2:
        return
    common_dims = None
    for metrics in compare_data.values():
        dims = set(metrics)
        common_dims = dims if common_dims is None else common_dims & dims
    for dim in sorted(common_dims or []):
        fig, ax = plt.subplots(figsize=(8.5, 5.2))
        for label, metrics in compare_data.items():
            df = metrics[dim][1]
            ax.plot(df["procesos"], df["speedup_paralelo"], "o-", linewidth=2, label=label)
        p_all = sorted(set().union(*[set(m[dim][1]["procesos"].astype(int)) for m in compare_data.values()]))
        ax.plot(p_all, p_all, "--", linewidth=1.5, label="Ideal $S_p=p$")
        ax.set_xlabel("Cantidad de procesos ($p$)")
        ax.set_ylabel("Speedup paralelo")
        ax.set_title(f"Comparación entre sistemas — Random Walk {dim}D")
        ax.grid(True, linestyle=":", alpha=0.5)
        ax.legend()
        save(fig, os.path.join(plots_dir, f"comparacion_sistemas_{dim}d.png"))


def main():
    parser = argparse.ArgumentParser(description="Genera gráficos del proyecto Random Walk HPC")
    parser.add_argument("--bench-dir", default="resultados_benchmark")
    parser.add_argument("--plots-dir", default="graficos")
    parser.add_argument("--compare", nargs="*", default=[], metavar="ETIQUETA=CARPETA",
                        help="Opcional: comparar resultados de distintos sistemas")
    args = parser.parse_args()

    os.makedirs(args.plots_dir, exist_ok=True)
    metrics = load_metrics_by_dimension(args.bench_dir)
    if not metrics:
        raise SystemExit(f"No se encontraron metricas_random_walk_*d.csv en {args.bench_dir}")

    plot_per_dimension(metrics, args.plots_dir)
    plot_combined_performance(metrics, args.plots_dir)
    plot_distance_vs_steps(args.bench_dir, args.plots_dir)

    validations = []
    validations.append(validate_1d(args.bench_dir, args.plots_dir, metrics))
    validations.append(validate_2d(args.bench_dir, args.plots_dir, metrics))
    validations.extend(validate_nd_radial(args.bench_dir, args.plots_dir, metrics))
    write_validation_summary(args.bench_dir, validations)

    compare_data = parse_compare_specs(args.compare)
    plot_system_comparison(compare_data, args.plots_dir)


if __name__ == "__main__":
    main()
