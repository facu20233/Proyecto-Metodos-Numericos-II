# Random Walk nD — Guía rápida de uso

Proyecto de simulación **Random Walk en 1D, 2D y nD** con versiones secuenciales baseline y versiones optimizadas/paralelas con **OpenMPI**.

## Requisitos

Se necesita tener instalados:

- `gcc`
- `mpicc`
- `mpirun`
- `make`
- `python3`
- Python: `numpy`, `pandas` y `matplotlib`

---

## 1. Compilar el proyecto

Desde la raíz del proyecto:

```bash
make
```

Esto compila:

- `random_walk_1d_baseline`
- `random_walk_2d_baseline`
- `random_walk_1d`
- `random_walk_2d`
- `random_walk_nd`

También crea, si no existen:

```text
resultados_benchmark/
graficos/
```

---

## 2. Prueba rápida de Random Walk nD

Ejemplo:

```bash
make test_nd D=3 M=500 N=2000000 NP=8
```

Donde:

- `D`: dimensión del espacio.
- `M`: pasos por caminata.
- `N`: cantidad total de caminatas.
- `NP`: cantidad de procesos MPI.

Otro ejemplo:

```bash
make test_nd D=4 M=1000 N=1000000 NP=4
```

Existe también una prueba 3D preconfigurada:

```bash
make test_3d
```

Y un modo interactivo:

```bash
make test_interactivo NP=4
```

> Las pruebas rápidas usan `--oversubscribe` para facilitar la ejecución local. El benchmark no lo utiliza automáticamente.

---

## 3. Ejecutar el benchmark

### Configuración por defecto

```bash
make benchmark
```

Por defecto utiliza:

```text
RUNS  = 5
PROCS = 1 2 4 8
DIMS  = 3
M     = 1000
N     = 10000000
```

El benchmark ejecuta:

1. Baseline secuencial 1D.
2. Baseline secuencial 2D.
3. Versión MPI optimizada 1D.
4. Versión MPI optimizada 2D.
5. Random Walk nD para las dimensiones indicadas en `DIMS`.

Cada escenario se ejecuta varias veces y se utiliza la **mediana** como tiempo representativo.

### Benchmark personalizado

Ejemplo con 3D y 4D:

```bash
make benchmark DIMS="3 4" PROCS="1 2 4 8" RUNS=5
```

En un nodo con 12 cores físicos:

```bash
make benchmark DIMS="3 4" PROCS="1 2 4 8 12" RUNS=5
```

También pueden modificarse los parámetros del Random Walk nD:

```bash
make benchmark \
    DIMS="3 4" \
    PROCS="1 2 4 8" \
    RUNS=5 \
    BENCH_M=2000 \
    BENCH_N=5000000
```

Donde:

- `BENCH_M`: pasos por caminata para nD.
- `BENCH_N`: cantidad total de caminatas para nD.

### Afinidad MPI opcional

El benchmark permite pasar opciones adicionales a `mpirun` mediante `MPI_FLAGS`.

Ejemplo:

```bash
make benchmark \
    PROCS="1 2 4 8" \
    MPI_FLAGS="--bind-to core --map-by core"
```

La política adecuada depende de la configuración del equipo o clúster.

---

## 4. Ejecutar `benchmark.sh` directamente

Primero debe compilarse el proyecto:

```bash
make
```

Luego puede ejecutarse:

```bash
./scripts/benchmark.sh "3"
```

Para varias dimensiones:

```bash
./scripts/benchmark.sh "3 4"
```

También se pueden definir variables de entorno:

```bash
PROCS="1 2 4 8" RUNS=5 ./scripts/benchmark.sh "3"
```

Ejemplo más completo:

```bash
PROCS="1 2 4 8" \
RUNS=5 \
ND_STEPS=1000 \
ND_WALKS=10000000 \
./scripts/benchmark.sh "3 4"
```

---

## 5. Generar los gráficos

Después de ejecutar el benchmark:

```bash
make plots
```

También puede ejecutarse directamente:

```bash
python3 scripts/generar_graficos.py
```

Los gráficos se guardan en:

```text
graficos/
```

Las métricas y mediciones se guardan en:

```text
resultados_benchmark/
```

Entre los resultados se almacenan:

- tiempos crudos de cada corrida;
- medianas;
- speedup paralelo;
- eficiencia;
- speedup global en 1D y 2D;
- información del entorno de ejecución;
- muestras de posiciones para validación estadística.

---

## 6. Ejecutar todo el flujo

Para compilar, ejecutar los benchmarks y generar los gráficos:

```bash
make run_all
```

También puede personalizarse:

```bash
make run_all DIMS="3 4" PROCS="1 2 4 8" RUNS=5
```

---

## 7. Limpieza

Eliminar sólo los ejecutables:

```bash
make clean
```

Eliminar resultados y gráficos:

```bash
make clean_results
```

Eliminar ejecutables, resultados y gráficos:

```bash
make distclean
```

---

## Flujo recomendado para pruebas

### PC local

```bash
make
make test_nd D=3 M=500 N=2000000 NP=4
make benchmark DIMS="3 4" PROCS="1 2 4 8" RUNS=5
make plots
```

### Clúster de 12 cores

Si se dispone realmente de 12 cores físicos asignados:

```bash
make
make benchmark DIMS="3 4" PROCS="1 2 4 8 12" RUNS=5
make plots
```

Si el clúster requiere binding explícito de procesos:

```bash
make benchmark \
    DIMS="3 4" \
    PROCS="1 2 4 8 12" \
    RUNS=5 \
    MPI_FLAGS="--bind-to core --map-by core"
```

---

## Ayuda rápida del Makefile

Para consultar los comandos disponibles:

```bash
make help
```
