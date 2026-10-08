# ==============================================================================
# PROYECTO - RANDOM WALK 1D / 2D / nD - HPC CON OPENMPI
# ==============================================================================

MPICC   ?= mpicc
GCC     ?= gcc
PYTHON  ?= python3
MPIRUN  ?= mpirun

CFLAGS_OPT  ?= -O3 -march=native -Wall -Wextra
CFLAGS_BASE ?= -O0 -Wall -Wextra
LDFLAGS     ?= -lm

SRC_DIR     := src
SCRIPTS_DIR := scripts
BENCH_DIR   := resultados_benchmark
PLOTS_DIR   := graficos

BASELINES := random_walk_1d_baseline random_walk_2d_baseline
OPT_BINS  := random_walk_1d random_walk_2d random_walk_nd

# Pruebas funcionales
NP ?= 4
D  ?= 3
M  ?= 1000
N  ?= 1000000

# Strong scaling definitivo
RUNS    ?= 5
PROCS   ?= 1 2 4 6 8 10 12
DIMS    ?= 3 4
BENCH_M ?= 1000
BENCH_N ?= 10000000

# Barrido físico: E[R] en función de M
PHYSICS_STEPS ?= 100 500 1000 2000 5000
PHYSICS_N     ?= 1000000
PHYSICS_NP    ?=

# Muestras para histogramas / densidades
SAMPLE_M  ?= $(BENCH_M)
SAMPLE_N  ?= 1000000
SAMPLE_NP ?=

# Benchmark: binding explícito, sin oversubscription.
# Si el entorno MPI/scheduler ya impone binding, puede sobrescribirse:
#   make benchmark MPI_FLAGS=""
MPI_FLAGS ?= --bind-to core --map-by core
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
# Pruebas funcionales
# ------------------------------------------------------------------------------
test_3d: directories random_walk_nd
	@echo ">>> Prueba 3D: D=3 M=1000 N=1000000 NP=$(NP)"
	$(MPIRUN) $(TEST_MPI_FLAGS) -np $(NP) ./random_walk_nd 3 1000 1000000

test_nd: directories random_walk_nd
	@echo ">>> Prueba nD: D=$(D) M=$(M) N=$(N) NP=$(NP)"
	$(MPIRUN) $(TEST_MPI_FLAGS) -np $(NP) ./random_walk_nd $(D) $(M) $(N)

test_interactivo: directories random_walk_nd
	$(MPIRUN) $(TEST_MPI_FLAGS) -np $(NP) ./random_walk_nd

# ------------------------------------------------------------------------------
# Protocolo experimental completo
# ------------------------------------------------------------------------------
benchmark: all
	@echo ">>> Strong scaling: PROCS='$(PROCS)', RUNS=$(RUNS), DIMS='$(DIMS)'"
	@echo ">>> Barrido físico: M='$(PHYSICS_STEPS)', N=$(PHYSICS_N)"
	RUNS="$(RUNS)" \
	PROCS="$(PROCS)" \
	MPI_RUN="$(MPIRUN)" \
	MPI_FLAGS="$(MPI_FLAGS)" \
	BENCH_STEPS="$(BENCH_M)" \
	BENCH_WALKS="$(BENCH_N)" \
	PHYSICS_STEPS="$(PHYSICS_STEPS)" \
	PHYSICS_WALKS="$(PHYSICS_N)" \
	PHYSICS_NP="$(PHYSICS_NP)" \
	SAMPLE_STEPS="$(SAMPLE_M)" \
	SAMPLE_WALKS="$(SAMPLE_N)" \
	SAMPLE_NP="$(SAMPLE_NP)" \
	bash $(SCRIPTS_DIR)/benchmark.sh "$(DIMS)"

plots: directories
	@echo ">>> Generando gráficos desde $(BENCH_DIR)/..."
	$(PYTHON) $(SCRIPTS_DIR)/generar_graficos.py --bench-dir $(BENCH_DIR) --plots-dir $(PLOTS_DIR)

run_all: benchmark plots
	@echo "========================================================================"
	@echo " Flujo completo finalizado."
	@echo "========================================================================"

# ------------------------------------------------------------------------------
# Limpieza
# ------------------------------------------------------------------------------
clean:
	@echo ">>> Eliminando ejecutables..."
	rm -f $(BASELINES) $(OPT_BINS) posiciones_*.csv

clean_results:
	@echo ">>> Eliminando resultados y gráficos..."
	rm -rf $(BENCH_DIR) $(PLOTS_DIR)

distclean: clean clean_results

help:
	@echo "Uso principal:"
	@echo "  make"
	@echo "  make test_nd D=3 M=500 N=2000000 NP=8"
	@echo "  make benchmark DIMS='3 4' PROCS='1 2 4 6 8 10 12' RUNS=5"
	@echo "  make plots"
	@echo ""
	@echo "Parámetros del benchmark:"
	@echo "  BENCH_M=1000 BENCH_N=10000000"
	@echo "  PHYSICS_STEPS='100 500 1000 2000 5000' PHYSICS_N=1000000"
	@echo "  PHYSICS_NP=<p>    (vacío = máximo de PROCS)"
	@echo "  SAMPLE_M=1000 SAMPLE_N=1000000 SAMPLE_NP=<p>"
	@echo "  MPI_FLAGS='--bind-to core --map-by core'"
	@echo ""
	@echo "Limpieza: make clean | make clean_results | make distclean"
