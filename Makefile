# ==============================================================================
# Makefile para Simulación Random Walk HPC (1D y 2D)
# Compila baselines (-O0), versiones paralelas (-O3) y ejecuta scripts
# ==============================================================================

# Compiladores
CC = mpicc
GCC = gcc

# Banderas de optimización
CFLAGS_OPT  = -O3 -march=native -Wall
CFLAGS_BASE = -O0 -Wall
LDFLAGS     = -lm

# Directorios del proyecto
SRC_DIR     = src
SCRIPTS_DIR = scripts
BENCH_DIR   = resultados_benchmark
PLOTS_DIR   = graficos

# Binarios objetivo
BASELINES = random_walk_1d_baseline random_walk_2d_baseline
OPT_BINS  = random_walk_1d random_walk_2d

# ------------------------------------------------------------------------------
# Regla predeterminada: Compila todos los ejecutables
# ------------------------------------------------------------------------------
all: baseline opt

# Compilación de los códigos no optimizados (Baseline)
baseline: $(BASELINES)

random_walk_1d_baseline: $(SRC_DIR)/random_walk_1d_baseline.c
	$(GCC) $(CFLAGS_BASE) $< -o $@ $(LDFLAGS)

random_walk_2d_baseline: $(SRC_DIR)/random_walk_2d_baseline.c
	$(GCC) $(CFLAGS_BASE) $< -o $@ $(LDFLAGS)

# Compilación de los códigos optimizados en C + MPI
opt: $(OPT_BINS)

random_walk_1d: $(SRC_DIR)/random_walk_1d.c
	$(CC) $(CFLAGS_OPT) $< -o $@ $(LDFLAGS)

random_walk_2d: $(SRC_DIR)/random_walk_2d.c
	$(CC) $(CFLAGS_OPT) $< -o $@ $(LDFLAGS)

# ------------------------------------------------------------------------------
# Automatización de Pruebas y Gráficos
# ------------------------------------------------------------------------------

# Ejecuta el protocolo de benchmarking (calcula medianas y Speedup)
benchmark: all
	@echo ">>> Ejecutando benchmark y generando archivos de métricas..."
	chmod +x $(SCRIPTS_DIR)/benchmark.sh
	./$(SCRIPTS_DIR)/benchmark.sh

# Ejecuta el script de Python para procesar CSVs y exportar gráficos
plots:
	@echo ">>> Generando figuras e histogramas con Python..."
	python3 $(SCRIPTS_DIR)/generar_graficos.py

# Regla integral: Compila todo, corre el benchmark completo y genera los gráficos
run_all: all benchmark plots
	@echo "========================================================================"
	@echo " Flujo completo finalizado: Códigos compilados, pruebas ejecutadas"
	@echo " y figuras listas en la carpeta '$(PLOTS_DIR)/'."
	@echo "========================================================================"

# Limpieza de binarios y archivos temporales/generados
clean:
	rm -f $(BASELINES) $(OPT_BINS)
	rm -rf $(BENCH_DIR) $(PLOTS_DIR) *.csv

.PHONY: all baseline opt benchmark plots run_all clean
