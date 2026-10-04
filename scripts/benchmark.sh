#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# Benchmark reproducible: baseline vs. optimizado/MPI (1D, 2D y nD)
# No compila: la compilación queda centralizada en el Makefile.
# ==============================================================================

RUNS="${RUNS:-5}"
PROCS_TEXT="${PROCS:-1 2 4 8}"
MPI_RUN="${MPI_RUN:-mpirun}"
MPI_FLAGS_TEXT="${MPI_FLAGS:-}"
OUTPUT_DIR="${OUTPUT_DIR:-resultados_benchmark}"
ND_STEPS="${ND_STEPS:-1000}"
ND_WALKS="${ND_WALKS:-10000000}"
EXTRA_DIMS="${1:-${EXTRA_DIMS:-}}"

read -r -a PROCS_ARRAY <<< "$PROCS_TEXT"
MPI_FLAGS_ARRAY=()
if [[ -n "$MPI_FLAGS_TEXT" ]]; then
    read -r -a MPI_FLAGS_ARRAY <<< "$MPI_FLAGS_TEXT"
fi

mkdir -p "$OUTPUT_DIR"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

[[ "$RUNS" =~ ^[1-9][0-9]*$ ]] || fail "RUNS debe ser un entero positivo."
[[ "$ND_STEPS" =~ ^[1-9][0-9]*$ ]] || fail "ND_STEPS debe ser un entero positivo."
[[ "$ND_WALKS" =~ ^[1-9][0-9]*$ ]] || fail "ND_WALKS debe ser un entero positivo."

for p in "${PROCS_ARRAY[@]}"; do
    [[ "$p" =~ ^[1-9][0-9]*$ ]] || fail "PROCS contiene un valor inválido: $p"
done

for bin in random_walk_1d_baseline random_walk_2d_baseline random_walk_1d random_walk_2d random_walk_nd; do
    [[ -x "./$bin" ]] || fail "Falta ./$bin. Ejecute 'make' antes del benchmark."
done

calc_median() {
    sort -n | awk '
    { values[NR] = $1 }
    END {
        if (NR == 0) exit 1;
        if (NR % 2 == 1) print values[(NR + 1) / 2];
        else print (values[NR / 2] + values[NR / 2 + 1]) / 2.0;
    }'
}

calc_ratio() {
    awk -v a="$1" -v b="$2" 'BEGIN { if (b == 0) exit 1; printf "%.4f", a / b }'
}

extract_time() {
    sed -nE 's/.*Tiempo[^:]*:[[:space:]]*([0-9]+([.][0-9]+)?).*/\1/p' | tail -n 1
}

run_mpi() {
    local p="$1"
    local bin="$2"
    shift 2
    "$MPI_RUN" "${MPI_FLAGS_ARRAY[@]}" -np "$p" "./$bin" "$@"
}

capture_metadata() {
    local file="$OUTPUT_DIR/entorno_benchmark.txt"
    {
        echo "fecha=$(date -Iseconds 2>/dev/null || date)"
        echo "hostname=$(hostname 2>/dev/null || echo desconocido)"
        echo "uname=$(uname -a 2>/dev/null || true)"
        echo "runs=$RUNS"
        echo "procesos=$PROCS_TEXT"
        echo "mpi_run=$MPI_RUN"
        echo "mpi_flags=$MPI_FLAGS_TEXT"
        echo "dimensiones_extra=${EXTRA_DIMS:-ninguna}"
        echo "nd_steps=$ND_STEPS"
        echo "nd_walks=$ND_WALKS"
        if command -v mpicc >/dev/null 2>&1; then
            echo "mpicc=$(mpicc --version 2>/dev/null | head -n 1)"
        fi
        if command -v gcc >/dev/null 2>&1; then
            echo "gcc=$(gcc --version 2>/dev/null | head -n 1)"
        fi
        if command -v lscpu >/dev/null 2>&1; then
            echo "--- lscpu ---"
            lscpu
        fi
    } > "$file"
}

capture_metadata

echo "========================================================================"
echo " PASO 1: BASELINES SECUENCIALES (-O0)"
echo "========================================================================"

for model in 1d 2d; do
    bin="random_walk_${model}_baseline"
    raw="$OUTPUT_DIR/raw_${bin}.csv"
    tmp=$(mktemp)
    trap 'rm -f "$tmp"' EXIT

    echo "corrida,tiempo" > "$raw"
    echo -n "-> $bin ($RUNS corridas): "

    for ((r=1; r<=RUNS; r++)); do
        out=$("./$bin")
        t=$(printf '%s\n' "$out" | extract_time)
        [[ -n "$t" ]] || fail "No se pudo extraer el tiempo de $bin (corrida $r)."
        echo "$t" >> "$tmp"
        echo "$r,$t" >> "$raw"
        echo -n "."
    done

    median=$(calc_median < "$tmp")
    rm -f "$tmp"
    trap - EXIT

    printf " Mediana: %s s\n" "$median"
    printf "%s\n" "$median" > "$OUTPUT_DIR/t_base_${model}.txt"
done

echo
echo "========================================================================"
echo " PASO 2: VERSIONES OPTIMIZADAS + MPI (1D y 2D)"
echo "========================================================================"

for model in 1d 2d; do
    bin="random_walk_${model}"
    csv="$OUTPUT_DIR/metricas_${bin}.csv"
    raw="$OUTPUT_DIR/raw_${bin}.csv"
    t_base=$(tr -d '[:space:]' < "$OUTPUT_DIR/t_base_${model}.txt")

    # Las versiones especializadas usan M=1000 y N=10.000.000 por diseño.
    dim=${model%d}
    steps=1000
    walks=10000000

    echo "dimension,steps,total_walks,procesos,tiempo_mediana,speedup_paralelo,eficiencia,speedup_global" > "$csv"
    echo "procesos,corrida,tiempo" > "$raw"
    echo ">>> $bin | T_base=$t_base s"

    t1=""
    for p in "${PROCS_ARRAY[@]}"; do
        tmp=$(mktemp)
        trap 'rm -f "$tmp"' EXIT
        echo -n "  -> p=$p [$RUNS corridas]: "

        for ((r=1; r<=RUNS; r++)); do
            out=$(run_mpi "$p" "$bin")
            t=$(printf '%s\n' "$out" | extract_time)
            [[ -n "$t" ]] || fail "No se pudo extraer el tiempo de $bin con p=$p (corrida $r)."
            echo "$t" >> "$tmp"
            echo "$p,$r,$t" >> "$raw"
            echo -n "."
        done

        median=$(calc_median < "$tmp")
        rm -f "$tmp"
        trap - EXIT

        if [[ "$p" == "1" ]]; then
            t1="$median"
            sp="1.0000"
            eff="1.0000"
        else
            [[ -n "$t1" ]] || fail "El conjunto PROCS debe comenzar por 1 para calcular speedup."
            sp=$(calc_ratio "$t1" "$median")
            eff=$(calc_ratio "$sp" "$p")
        fi

        global=$(calc_ratio "$t_base" "$median")
        printf " Mediana=%ss | Sp=%sx | E=%s | Sglobal=%sx\n" "$median" "$sp" "$eff" "$global"
        echo "$dim,$steps,$walks,$p,$median,$sp,$eff,$global" >> "$csv"
    done

    if [[ -f "posiciones_${model}.csv" ]]; then
        mv -f "posiciones_${model}.csv" "$OUTPUT_DIR/posiciones_${model}.csv"
    fi
    echo "Resultados: $csv"
    echo
done

if [[ -n "$EXTRA_DIMS" ]]; then
    echo "========================================================================"
    echo " PASO 3: RANDOM WALK nD"
    echo "========================================================================"

    for dim in $EXTRA_DIMS; do
        [[ "$dim" =~ ^[1-9][0-9]*$ ]] || fail "Dimensión inválida: $dim"

        csv="$OUTPUT_DIR/metricas_random_walk_${dim}d.csv"
        raw="$OUTPUT_DIR/raw_random_walk_${dim}d.csv"
        echo "dimension,steps,total_walks,procesos,tiempo_mediana,speedup_paralelo,eficiencia,speedup_global" > "$csv"
        echo "procesos,corrida,tiempo" > "$raw"
        echo ">>> random_walk_nd | D=$dim M=$ND_STEPS N=$ND_WALKS"

        t1=""
        for p in "${PROCS_ARRAY[@]}"; do
            tmp=$(mktemp)
            trap 'rm -f "$tmp"' EXIT
            echo -n "  -> p=$p [$RUNS corridas]: "

            for ((r=1; r<=RUNS; r++)); do
                out=$(run_mpi "$p" random_walk_nd "$dim" "$ND_STEPS" "$ND_WALKS")
                t=$(printf '%s\n' "$out" | extract_time)
                [[ -n "$t" ]] || fail "No se pudo extraer el tiempo nD para D=$dim p=$p corrida=$r."
                echo "$t" >> "$tmp"
                echo "$p,$r,$t" >> "$raw"
                echo -n "."
            done

            median=$(calc_median < "$tmp")
            rm -f "$tmp"
            trap - EXIT

            if [[ "$p" == "1" ]]; then
                t1="$median"
                sp="1.0000"
                eff="1.0000"
            else
                [[ -n "$t1" ]] || fail "El conjunto PROCS debe comenzar por 1 para calcular speedup."
                sp=$(calc_ratio "$t1" "$median")
                eff=$(calc_ratio "$sp" "$p")
            fi

            printf " Mediana=%ss | Sp=%sx | E=%s\n" "$median" "$sp" "$eff"
            echo "$dim,$ND_STEPS,$ND_WALKS,$p,$median,$sp,$eff," >> "$csv"
        done

        if [[ -f "posiciones_${dim}d.csv" ]]; then
            mv -f "posiciones_${dim}d.csv" "$OUTPUT_DIR/posiciones_${dim}d.csv"
        fi
        echo "Resultados: $csv"
        echo
    done
fi

echo "========================================================================"
echo " BENCHMARK FINALIZADO"
echo " Resultados guardados en: $OUTPUT_DIR"
echo "========================================================================"
