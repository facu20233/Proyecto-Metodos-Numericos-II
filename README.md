# Random Walk 1D, 2D y nD — OpenMPI

Proyecto HPC para simular caminatas aleatorias independientes, validar el modelo físico-estocástico y estudiar strong scaling con MPI.

## Compilar

```bash
make
```

## Prueba rápida

```bash
make test_nd D=3 M=500 N=2000000 NP=4
```

## Evaluación definitiva en el clúster de 12 núcleos

El protocolo completo ejecuta:

1. Baseline secuencial `-O0` para 1D y 2D.
2. Strong scaling para 1D y 2D.
3. Strong scaling para las dimensiones adicionales indicadas en `DIMS`.
4. Barrido de cantidad de pasos para obtener distancia promedio vs. pasos.
5. Una muestra espacial por dimensión para histogramas/densidades.

Comando recomendado:

```bash
make benchmark DIMS="3 4" PROCS="1 2 4 6 8 10 12" RUNS=5
```

Valores por defecto del strong scaling:

```text
M = 1000 pasos
N = 10.000.000 caminatas
```

Valores por defecto del barrido físico:

```text
M = 100 500 1000 2000 5000
N = 1.000.000 caminatas
p = máximo valor de PROCS (12 en la corrida recomendada)
```

El benchmark no permite oversubscription: si se solicitan más procesos que CPUs visibles mediante `nproc`, aborta antes de medir.

Por defecto MPI utiliza:

```text
--bind-to core --map-by core
```

Si el scheduler del clúster ya realiza binding y fuera necesario desactivarlo:

```bash
make benchmark DIMS="3 4" PROCS="1 2 4 6 8 10 12" RUNS=5 MPI_FLAGS=""
```

## Archivos de resultados

Los datos se guardan en `resultados_benchmark/`:

```text
metricas_random_walk_1d.csv ... 4d.csv   # medianas, speedup y eficiencia
raw_random_walk_*.csv                    # cinco tiempos crudos por escenario
distancia_vs_pasos.csv                   # E[R] experimental y teórica al variar M
posiciones_1d.csv ... 4d.csv             # muestras para histogramas/densidades
muestras_config.csv                      # parámetros usados para las muestras
entorno_benchmark.txt                    # hardware, CPUs visibles, MPI y compilador
```

## Generar gráficos en la PC personal

Copiar completa la carpeta `resultados_benchmark/` desde el clúster y ejecutar:

```bash
make plots
```

El graficador produce:

- tiempo, speedup y eficiencia por dimensión;
- comparación de speedup, eficiencia y tiempos entre dimensiones;
- speedup global para 1D/2D;
- distancia promedio vs. cantidad de pasos para 1D/2D;
- distancia promedio vs. pasos para la extensión nD;
- histograma de probabilidad 1D;
- histograma 2D/densidad de probabilidad 2D;
- distribuciones radiales para D >= 3;
- error experimental respecto de la aproximación teórica.

También puede ejecutarse directamente:

```bash
python3 scripts/generar_graficos.py \
  --bench-dir resultados_benchmark \
  --plots-dir graficos
```

## Comparar PC local vs. clúster

Si se guardan las dos carpetas, por ejemplo:

```text
resultados_pc/
resultados_cluster/
```

se pueden generar gráficos comparativos con:

```bash
python3 scripts/generar_graficos.py \
  --bench-dir resultados_cluster \
  --plots-dir graficos \
  --compare "PC=resultados_pc" "Cluster=resultados_cluster"
```

## Ejecutar los programas manualmente

### 1D

```bash
mpirun -np 4 ./random_walk_1d 1000 10000000
```

### 2D

```bash
mpirun -np 4 ./random_walk_2d 1000 10000000
```

### nD

```bash
mpirun -np 4 ./random_walk_nd 3 1000 10000000
```

Para exportar una muestra espacial se agrega `--sample`:

```bash
mpirun -np 4 ./random_walk_nd 3 1000 1000000 --sample
```

## Limpieza

```bash
make clean
make clean_results
make distclean
```
