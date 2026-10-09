# Caminata del Borracho (Random Walk 1D y 2D) con MPI

Proyecto 5 – Evaluación Parcial 1  
Métodos Numéricos II – Computación Científica · FACET – UNT · 2026

**Integrantes:** _[completar: apellido, nombre y carrera]_

## Descripción

Simulación de la caminata aleatoria en una y dos dimensiones, paralelizada con MPI en C.

- **Modelo 1D:** el sujeto parte de 0 y en cada paso se mueve +1 o −1 con igual probabilidad.
- **Modelo 2D:** el sujeto parte de (0, 0) y en cada paso se mueve una unidad en x o en y (4 direcciones equiprobables).

El programa calcula:

1. La **distancia al origen** de cada caminata.
2. La **distancia promedio acumulada globalmente** en función de la cantidad de pasos.
3. El **histograma de probabilidad** de la posición final.

Y mide el tiempo de ejecución para calcular **speedup**, **eficiencia** y ajustar la **ley de Amdahl**.

### Estrategia de paralelización

Los caminantes son independientes entre sí (problema "vergonzosamente paralelo"), por lo que se reparten los N caminantes entre los p procesos (N/p cada uno, y el resto a los primeros). Cada proceso usa su propia semilla, simula sus caminantes y acumula resultados locales. Al final se combinan con `MPI_Reduce` (`MPI_SUM` para distancias e histograma, `MPI_MAX` para el tiempo). No hay comunicación durante la simulación ni entrada/salida dentro de la parte cronometrada.

El mismo código sirve para 1D y 2D: la dimensión es un argumento.

## Contenido del repositorio

| Archivo | Para qué sirve |
|---|---|
| `random_walk.c` | Programa principal (MPI). |
| `run_experiments.sh` | Mide tiempos con p = 1, 2, 3, …, MAXP (todos consecutivos) y varias repeticiones. Genera `tiempos.csv`. |
| `run_datos.sh` | Genera los CSV de distancia media e histogramas (corrida aparte, no sirve para medir tiempos). |
| `analizar.py` | Calcula mediana, speedup, eficiencia y ajuste de Amdahl, y genera los gráficos. |

## Requisitos

- Compilador C y una implementación de MPI (se usó OpenMPI): `mpicc`, `mpirun`.
- Python 3 con `numpy`, `pandas` y `matplotlib` (solo para `analizar.py`).

```bash
pip install numpy pandas matplotlib
```

## Compilación

```bash
mpicc -O3 -o random_walk random_walk.c -lm
```

## Uso del programa

```bash
mpirun -np P ./random_walk dim N S [seed] [stride] [R] [write]
```

| Argumento | Significado | Valor por defecto |
|---|---|---|
| `dim` | Dimensión: `1` o `2` | (obligatorio) |
| `N` | Cantidad **total** de caminantes (se reparten entre los procesos) | (obligatorio) |
| `S` | Cantidad de pasos de cada caminata | (obligatorio) |
| `seed` | Semilla base del generador | `12345` |
| `stride` | Cada cuántos pasos se registra la distancia | `1` |
| `R` | Radio del histograma: se cuentan posiciones finales en [−R, R] | `⌈4·√S⌉` (acotado a S) |
| `write` | `1` escribe los CSV, `0` no escribe nada | `0` |

Los argumentos son **posicionales**: para pasar `R` hay que pasar también `seed` y `stride`, y para pasar `write` hay que pasar antes `R`.

**Ejemplo:** 2D, 10 millones de caminantes, 1000 pasos, 4 procesos, R = 150, escribiendo archivos:

```bash
mpirun -np 4 ./random_walk 2 10000000 1000 12345 1 150 1
```

### Salida por pantalla

El proceso 0 imprime una línea CSV:

```
dim,N,S,P,tiempo_max_seg,dist_final_media
```

El tiempo es el **máximo entre procesos** (el programa termina cuando termina el último) y abarca simulación y reducciones, sin escritura de archivos.

### Archivos generados (con `write=1`)

- `mean_dist_<dim>d.csv`: columnas `paso,dist_media`.
- `hist_<dim>d.csv`: en 1D `x,cuentas,prob`; en 2D `x,y,cuentas,prob`. Solo se escriben las celdas con cuentas.

> **Nota sobre el histograma:** la posición final tiene siempre la paridad de S (en 1D) o x + y ≡ S (mod 2) (en 2D), por eso la mitad de las celdas queda vacía. Es una propiedad del modelo, no un error. Los caminantes que terminan fuera de [−R, R] no se cuentan en el histograma (sí en la distancia media).

## Reproducir los experimentos

### 1. Tiempos y speedup

```bash
chmod +x run_experiments.sh run_datos.sh
./run_experiments.sh [MAXP] [REPS]
```

- `MAXP`: máximo de procesos (por defecto `nproc`). Se prueban **todos** los valores 1, 2, 3, …, MAXP.
- `REPS`: repeticiones por medición (por defecto 10). Con ellas se calcula la **mediana**.

Variables de entorno opcionales:

| Variable | Descripción | Por defecto |
|---|---|---|
| `CONFIGS` | Pares `"N S"` separados por `;` | `"1000000 1000;10000000 1000;1000000 10000"` |
| `DIMS` | Dimensiones a medir | `"1 2"` |
| `MPI_OPTS` | Opciones de `mpirun` | `--bind-to core --map-by core` |
| `OUT` | Archivo de salida | `tiempos.csv` |

Ejemplo: 28 procesos, 10 repeticiones, otras configuraciones:

```bash
CONFIGS="1000000 1000;10000000 1000" ./run_experiments.sh 28 10
```

Para probar rápido que todo funciona: `./run_experiments.sh 4 2`.

Salida: `tiempos.csv` con columnas `dim,N,S,P,tiempo,dist_media,rep`.

Consideraciones de la medición:

- El bucle de repeticiones es el más externo, así el ruido del sistema se reparte entre todos los valores de p.
- Los procesos se fijan a núcleos físicos (`--bind-to core`). No usar más procesos que núcleos físicos. Si tu MPI no acepta esas opciones (por ejemplo MPICH), cambiá `MPI_OPTS` (por ejemplo `MPI_OPTS="-bind-to core"` o `MPI_OPTS=""`).
- El número de corridas es `REPS × dimensiones × configuraciones × MAXP`, así que la corrida completa puede llevar mucho tiempo.

### 2. Datos físicos (distancia media e histogramas)

```bash
./run_datos.sh [P] [N]
```

Genera `datos/<dim>d_S<pasos>/` para 1D y 2D con S = 100 y S = 1000 (por defecto P = `nproc`, N = 10⁷).

### 3. Análisis y gráficos

```bash
python3 analizar.py
```

Genera en `figuras/`:

| Figura | Contenido |
|---|---|
| `speedup_<dim>d.png` | Speedup vs. procesos, con recta ideal y curva de Amdahl ajustada |
| `eficiencia_<dim>d.png` | Eficiencia paralela vs. procesos |
| `tiempo_<dim>d.png` | Tiempo (mediana y rango intercuartil) vs. procesos |
| `dist_media_<dim>d_S<S>.png` | Distancia media vs. pasos, con teoría y escala log-log |
| `hist_1d_S<S>.png` | Histograma de la posición final 1D con la gaussiana teórica |
| `hist_2d_S<S>.png` | Mapa de calor de la posición final 2D y corte en y = 0 con la teoría |

Y las tablas:

- `resumen_speedup.csv`: mediana, speedup y eficiencia por (dim, N, S, p).
- `resumen_amdahl.csv`: fracción serial efectiva `f` estimada, speedup máximo teórico `1/f` y valores medidos con el p máximo.

## Teoría de referencia

| Magnitud | 1D | 2D |
|---|---|---|
| Distancia media tras n pasos | √(2n/π) | √(πn)/2 |
| E[distancia²] | n | n |
| Probabilidad de la posición final (sitios de paridad compatible) | 2/√(2πn) · exp(−x²/(2n)) | 2/(πn) · exp(−(x²+y²)/n) |

- **Speedup:** S_p = T₁ / T_p (con la mediana de las repeticiones).
- **Eficiencia:** E_p = S_p / p.
- **Ley de Amdahl:** S_p = 1 / (f + (1 − f)/p), con techo S_∞ = 1/f. `analizar.py` estima `f` por mínimos cuadrados sobre 1/S_p = f + (1 − f)/p. Es una fracción *efectiva*: incluye la parte serial real y también el costo de comunicación y las limitaciones de hardware.

## Sistemas en los que se probó

_[completar: modelo de CPU, núcleos físicos, sockets, RAM y versión de MPI de cada sistema, por ejemplo con la salida de `lscpu`]_

## Resultados

_[completar: link al informe y resumen breve de los resultados principales]_

## Referencias

- Documento de la cátedra: *Computación de Alto Rendimiento para aplicaciones numéricas* (FACET-UNT, 2026).
- Blackman, D. y Vigna, S. (2021). *Scrambled linear pseudorandom number generators*. ACM TOMS (generador xoshiro256**).
- Amdahl, G. M. (1967). *Validity of the single processor approach to achieving large scale computing capabilities*.
- Open MPI: https://www.open-mpi.org
