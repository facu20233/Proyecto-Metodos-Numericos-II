#!/usr/bin/env python3
"""
Analisis de resultados del Proyecto 5 (caminata del borracho).

Entradas (en el directorio actual):
  tiempos.csv                       (run_experiments.sh)
  datos/<dim>d_S<S>/*.csv           (run_datos.sh)

Salidas:
  figuras/*.png
  resumen_speedup.csv   (mediana, speedup, eficiencia por p)
  resumen_amdahl.csv    (fraccion serial estimada y speedup maximo teorico)

Requiere: numpy, pandas, matplotlib
"""
import glob
import os
import re

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

os.makedirs("figuras", exist_ok=True)


# ----------------------------------------------------------------------------
# 1) Speedup, eficiencia y ley de Amdahl
# ----------------------------------------------------------------------------
def amdahl_fit(p, sp):
    """Ajusta la fraccion serial f por minimos cuadrados sobre 1/S = f + (1-f)/p.

    Con x = 1/p, y = 1/S:  y - x = f (1 - x)  ->  f = sum((y-x)(1-x)) / sum((1-x)^2)
    El punto p=1 no aporta (1-x = 0). Se acota f a [0, 1].
    """
    x = 1.0 / np.asarray(p, float)
    y = 1.0 / np.asarray(sp, float)
    den = np.sum((1 - x) ** 2)
    if den == 0:
        return np.nan
    f = np.sum((y - x) * (1 - x)) / den
    return float(min(max(f, 0.0), 1.0))


def amdahl(p, f):
    p = np.asarray(p, float)
    return 1.0 / (f + (1 - f) / p)


def analisis_tiempos(path="tiempos.csv"):
    df = pd.read_csv(path)
    # MEDIANA de las repeticiones (lo pide la consigna)
    med = (df.groupby(["dim", "N", "S", "P"])["tiempo"]
             .agg(mediana="median", q1=lambda s: s.quantile(.25), q3=lambda s: s.quantile(.75))
             .reset_index())
    t1 = med[med.P == 1].set_index(["dim", "N", "S"])["mediana"]
    med["T1"] = [t1[(r.dim, r.N, r.S)] for r in med.itertuples()]
    med["speedup"] = med["T1"] / med["mediana"]
    med["eficiencia"] = med["speedup"] / med["P"]
    med.to_csv("resumen_speedup.csv", index=False)

    filas = []
    for dim in sorted(med.dim.unique()):
        sub = med[med.dim == dim]
        maxp = int(sub.P.max())
        grupos = list(sub.groupby(["N", "S"]))
        colores = plt.cm.tab10(np.arange(len(grupos)))

        fig_s, ax_s = plt.subplots(figsize=(9, 6))
        fig_e, ax_e = plt.subplots(figsize=(9, 5))
        fig_t, ax_t = plt.subplots(figsize=(9, 5))

        for c, ((N, S), g) in zip(colores, grupos):
            g = g.sort_values("P")
            f = amdahl_fit(g.P, g.speedup)
            filas.append({"dim": dim, "N": N, "S": S, "f_serial": f,
                          "speedup_max_amdahl": (1 / f) if f > 0 else np.inf,
                          "speedup_medido_maxP": g.speedup.iloc[-1],
                          "eficiencia_maxP": g.eficiencia.iloc[-1]})
            lab = f"N={int(N):,}, S={int(S):,}".replace(",", ".")
            ax_s.plot(g.P, g.speedup, "o-", color=c, ms=4, label=lab)
            pp = np.linspace(1, maxp, 200)
            ax_s.plot(pp, amdahl(pp, f), "--", color=c, alpha=.6, lw=1)
            ax_e.plot(g.P, g.eficiencia, "o-", color=c, ms=4, label=lab)
            ax_t.plot(g.P, g.mediana, "o-", color=c, ms=4, label=lab)
            ax_t.fill_between(g.P, g.q1, g.q3, color=c, alpha=.2)

        ax_s.plot([1, maxp], [1, maxp], "-", color="gray", alpha=.6, label="Speedup ideal (lineal)")
        ax_s.set_title(f"Speedup vs procesos - caminata {dim}D\n"
                       "(lineas punteadas: ajuste de la ley de Amdahl)")
        ax_s.set_xlabel("Numero de procesos (p)"); ax_s.set_ylabel("Speedup  Sp = T1 / Tp")
        ax_s.set_xticks(range(1, maxp + 1)); ax_s.grid(alpha=.3); ax_s.legend()
        fig_s.tight_layout(); fig_s.savefig(f"figuras/speedup_{dim}d.png", dpi=150)

        ax_e.axhline(1, color="gray", ls="--", alpha=.6)
        ax_e.set_title(f"Eficiencia paralela vs procesos - caminata {dim}D")
        ax_e.set_xlabel("Numero de procesos (p)"); ax_e.set_ylabel("Eficiencia  E = Sp / p")
        ax_e.set_xticks(range(1, maxp + 1)); ax_e.set_ylim(0, 1.15)
        ax_e.grid(alpha=.3); ax_e.legend()
        fig_e.tight_layout(); fig_e.savefig(f"figuras/eficiencia_{dim}d.png", dpi=150)

        ax_t.set_yscale("log")
        ax_t.set_title(f"Tiempo de ejecucion (mediana, banda = rango intercuartil) - {dim}D")
        ax_t.set_xlabel("Numero de procesos (p)"); ax_t.set_ylabel("Tiempo [s] (escala log)")
        ax_t.set_xticks(range(1, maxp + 1)); ax_t.grid(alpha=.3, which="both"); ax_t.legend()
        fig_t.tight_layout(); fig_t.savefig(f"figuras/tiempo_{dim}d.png", dpi=150)
        plt.close("all")

    res = pd.DataFrame(filas)
    res.to_csv("resumen_amdahl.csv", index=False)
    print("\n== Ajuste de Amdahl ==")
    print(res.to_string(index=False, float_format=lambda v: f"{v:.5f}"))


# ----------------------------------------------------------------------------
# 2) Resultados fisicos: distancia media e histogramas
# ----------------------------------------------------------------------------
def analisis_fisico():
    for d in sorted(glob.glob("datos/*d_S*")):
        m = re.search(r"(\d)d_S(\d+)", d)
        if not m:
            continue
        dim, S = int(m.group(1)), int(m.group(2))

        # ---- distancia media vs pasos ----
        f_md = os.path.join(d, f"mean_dist_{dim}d.csv")
        if os.path.exists(f_md):
            md = pd.read_csv(f_md)
            n = md.paso.values.astype(float)
            teo = np.sqrt(2 * n / np.pi) if dim == 1 else np.sqrt(np.pi * n) / 2
            fig, ax = plt.subplots(1, 2, figsize=(12, 4.5))
            ax[0].plot(n, md.dist_media, label="Simulacion")
            ax[0].plot(n, teo, "--", label=("sqrt(2n/pi)" if dim == 1 else "sqrt(pi n)/2"))
            ax[0].set_xlabel("Cantidad de pasos n"); ax[0].set_ylabel("Distancia media al origen")
            ax[0].set_title(f"Distancia media vs pasos ({dim}D)"); ax[0].legend(); ax[0].grid(alpha=.3)
            ok = n >= 10
            slope = np.polyfit(np.log(n[ok]), np.log(md.dist_media.values[ok]), 1)[0]
            ax[1].loglog(n[n > 0], md.dist_media[n > 0], label=f"Simulacion (pendiente = {slope:.3f})")
            ax[1].loglog(n[n > 0], teo[n > 0], "--", label="Teoria (pendiente = 0.5)")
            ax[1].set_xlabel("n"); ax[1].set_ylabel("Distancia media")
            ax[1].set_title("Escala log-log: ley de difusion"); ax[1].legend(); ax[1].grid(alpha=.3, which="both")
            fig.tight_layout(); fig.savefig(f"figuras/dist_media_{dim}d_S{S}.png", dpi=150); plt.close(fig)
            print(f"[{dim}D, S={S}] dist. media final = {md.dist_media.iloc[-1]:.3f} "
                  f"(teoria {teo[-1]:.3f}); pendiente log-log = {slope:.3f}")

        # ---- histograma de la posicion final ----
        f_h = os.path.join(d, f"hist_{dim}d.csv")
        if not os.path.exists(f_h):
            continue
        h = pd.read_csv(f_h)
        if dim == 1:
            x = h.x.values
            xs = np.arange(x.min(), x.max() + 1)
            xs = xs[(xs - S) % 2 == 0]                      # solo posiciones con la paridad de S
            teo = 2 / np.sqrt(2 * np.pi * S) * np.exp(-xs ** 2 / (2 * S))
            fig, ax = plt.subplots(figsize=(9, 4.5))
            ax.bar(x, h.prob, width=1.6, alpha=.7, label="Simulacion")
            ax.plot(xs, teo, "r-", lw=2, label="Gaussiana teorica (N(0, S))")
            ax.set_xlabel("Posicion final x"); ax.set_ylabel("Probabilidad")
            ax.set_title(f"Histograma de la posicion final - 1D, S={S} pasos")
            ax.legend(); ax.grid(alpha=.3)
            fig.tight_layout(); fig.savefig(f"figuras/hist_1d_S{S}.png", dpi=150); plt.close(fig)
        else:
            R = int(max(h.x.abs().max(), h.y.abs().max()))
            M = np.zeros((2 * R + 1, 2 * R + 1))
            M[h.y.values + R, h.x.values + R] = h.prob.values
            fig, ax = plt.subplots(1, 2, figsize=(13, 5))
            im = ax[0].imshow(M, origin="lower", extent=[-R - .5, R + .5, -R - .5, R + .5], cmap="viridis")
            ax[0].set_xlabel("x"); ax[0].set_ylabel("y")
            ax[0].set_title(f"Probabilidad de la posicion final - 2D, S={S}")
            fig.colorbar(im, ax=ax[0], label="Probabilidad")
            xs = np.arange(-R, R + 1)
            corte = M[R, :]
            teo = 2 / (np.pi * S) * np.exp(-xs ** 2 / S)
            mask = (xs - S) % 2 == 0                         # y = 0 -> x debe tener la paridad de S
            ax[1].plot(xs[mask], corte[mask], "o", ms=3, label="Simulacion (corte y = 0)")
            ax[1].plot(xs[mask], teo[mask], "r-", label="Teoria 2/(pi S) exp(-x^2/S)")
            ax[1].set_xlabel("x"); ax[1].set_ylabel("Probabilidad"); ax[1].legend(); ax[1].grid(alpha=.3)
            ax[1].set_title("Corte del histograma 2D en y = 0")
            fig.tight_layout(); fig.savefig(f"figuras/hist_2d_S{S}.png", dpi=150); plt.close(fig)


if __name__ == "__main__":
    if os.path.exists("tiempos.csv"):
        analisis_tiempos()
    else:
        print("No se encontro tiempos.csv (corre run_experiments.sh)")
    analisis_fisico()
    print("\nFiguras en ./figuras  |  tablas: resumen_speedup.csv, resumen_amdahl.csv")
