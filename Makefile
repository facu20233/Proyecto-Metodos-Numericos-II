# ==============================================================================
# PROYECTO - RANDOM WALK nD / HPC CON OPENMPI
# ==============================================================================

MPICC   ?= mpicc
GCC      ?= gcc
PYTHON   ?= python3
MPIRUN   ?= mpirun

CFLAGS_OPT  ?= -O3 -march=native -Wall -Wextra
CFLAGS_BASE ?= -O0 -Wall -Wextra
LDFLAGS     ?= -lm

SRC_DIR     := src
SCRIPTS_DIR := scripts
BENCH_DIR   := resultados_benchmark
PLOTS_DIR   := graficos

BASELINES := random_walk_1d_baseline random_walk_2d_baseline
OPT_BINS  := random_walk_1d random_walk_2d random_walk_nd

# Parámetros para pruebas rápidas / nD
NP ?= 4
D  ?= 3
M  ?= 1000
N  ?= 1000000

# Parámetros del protocolo de benchmark
RUNS        ?= 5
PROCS       ?= 1 2 4 8
DIMS        ?= 3
BENCH_M     ?= 1000
BENCH_N     ?= 10000000
MPI_FLAGS   ?=
TEST_MPI_FLAGS ?= --oversubscribe

.PHONY: all directories baseline opt test_3d test_nd test_interactivo \
        benchmark plots run_all clean clean_results distclean help

all: directories baseline opt

directories:
	@mkdir -p $(BENCH_DIR) $(PLOTS_DIR)

# ------------------------------------------------------------------------------
# Compilación
# ------------------------------------------------------------------------------
baseline: $(BASELINES)

random_walk_1d_baseline: $(SRC_DIR)/random_walk_1d_baseline.c
	$(GCC) $(CFLAGS_BASE) $< -o $@ $(LDFLAGS)

random_walk_2d_baseline: $(SRC_DIR)/random_walk_2d_baseline.c
	$(GCC) $(CFLAGS_BASE) $< -o $@ $(LDFLAGS)

opt: $(OPT_BINS)

random_walk_1d: $(SRC_DIR)/random_walk_1d.c
	$(MPICC) $(CFLAGS_OPT) $< -o $@ $(LDFLAGS)

random_walk_2d: $(SRC_DIR)/random_walk_2d.c
	$(MPICC) $(CFLAGS_OPT) $< -o $@ $(LDFLAGS)

random_walk_nd: $(SRC_DIR)/random_walk_nd.c
	$(MPICC) $(CFLAGS_OPT) $< -o $@ $(LDFLAGS)

# ------------------------------------------------------------------------------
# Pruebas funcionales. --oversubscribe queda reservado para tests, no benchmark.
# ------------------------------------------------------------------------------
test_3d: directories random_walk_nd
	@echo ">>> Prueba 3D: D=3 M=1000 N=1000000 NP=$(NP)"
	$(MPIRUN) $(TEST_MPI_FLAGS) -np $(NP) ./random_walk_nd 3 1000 1000000

test_nd: directories random_walk_nd
	@echo ">>> Prueba nD: D=$(D) M=$(M) N=$(N) NP=$(NP)"
	$(MPIRUN) $(TEST_MPI_FLAGS) -np $(NP) ./random_walk_nd $(D) $(M) $(N)

test_interactivo: directories random_walk_nd
	@echo ">>> Modo interactivo con NP=$(NP)"
	$(MPIRUN) $(TEST_MPI_FLAGS) -np $(NP) ./random_walk_nd

# ------------------------------------------------------------------------------
# Benchmark y gráficos
# ------------------------------------------------------------------------------
benchmark: all
	@echo ">>> Benchmark: RUNS=$(RUNS), PROCS='$(PROCS)', DIMS='$(DIMS)'"
	RUNS="$(RUNS)" \
	PROCS="$(PROCS)" \
	MPI_RUN="$(MPIRUN)" \
	MPI_FLAGS="$(MPI_FLAGS)" \
	ND_STEPS="$(BENCH_M)" \
	ND_WALKS="$(BENCH_N)" \
	bash $(SCRIPTS_DIR)/benchmark.sh "$(DIMS)"

plots: directories
	@echo ">>> Generando gráficos..."
	$(PYTHON) $(SCRIPTS_DIR)/generar_graficos.py

run_all: benchmark plots
	@echo "========================================================================"
	@echo " Flujo completo finalizado."
	@echo "========================================================================"

# ------------------------------------------------------------------------------
# Limpieza
# ------------------------------------------------------------------------------
clean:
	@echo ">>> Eliminando ejecutables..."
	rm -f $(BASELINES) $(OPT_BINS)
	rm -f posiciones_*.csv

clean_results:
	@echo ">>> Eliminando resultados y gráficos..."
	rm -rf $(BENCH_DIR) $(PLOTS_DIR)

distclean: clean clean_results

help:
	@echo "Objetivos principales:"
	@echo "  make                       Compila baseline y versiones MPI"
	@echo "  make test_nd D=3 M=500 N=2000000 NP=8"
	@echo "  make benchmark             Benchmark 1D, 2D y DIMS (por defecto 3D)"
	@echo "  make benchmark DIMS='3 4' PROCS='1 2 4 8 12' RUNS=5"
	@echo "  make plots                 Genera gráficos desde resultados_benchmark/"
	@echo "  make run_all               Compila, mide y grafica"
	@echo "  make clean                 Elimina sólo binarios"
	@echo "  make clean_results         Elimina métricas y gráficos"
	@echo "  make distclean             Limpieza total"
