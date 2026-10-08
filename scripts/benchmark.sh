#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# Protocolo experimental completo
#   1) Baseline secuencial 1D/2D
#   2) Strong scaling MPI 1D/2D
#   3) Strong scaling MPI nD (dimensiones adicionales)
#   4) Barrido físico: distancia promedio vs. cantidad de pasos
#   5) Generación de muestras espaciales para histogramas/densidades
# ============================================================================== 

RUNS="${RUNS:-5}"
PROCS_TEXT="${PROCS:-1 2 4 6 8 10 12}"
MPI_RUN="${MPI_RUN:-mpirun}"
MPI_FLAGS_TEXT="${MPI_FLAGS:-}"
OUTPUT_DIR="${OUTPUT_DIR:-resultados_benchmark}"
BENCH_STEPS="${BENCH_STEPS:-1000}"
BENCH_WALKS="${BENCH_WALKS:-10000000}"
PHYSICS_STEPS_TEXT="${PHYSICS_STEPS:-100 500 1000 2000 5000}"
PHYSICS_WALKS="${PHYSICS_WALKS:-1000000}"
PHYSICS_NP="${PHYSICS_NP:-}"
SAMPLE_STEPS="${SAMPLE_STEPS:-$BENCH_STEPS}"
SAMPLE_WALKS="${SAMPLE_WALKS:-1000000}"
SAMPLE_NP="${SAMPLE_NP:-}"
RUN_PHYSICS="${RUN_PHYSICS:-1}"
RUN_SAMPLES="${RUN_SAMPLES:-1}"
EXTRA_DIMS="${1:-${EXTRA_DIMS:-}}"

read -r -a PROCS_ARRAY <<< "$PROCS_TEXT"
read -r -a PHYSICS_STEPS_ARRAY <<< "$PHYSICS_STEPS_TEXT"
MPI_FLAGS_ARRAY=()
if [[ -n "$MPI_FLAGS_TEXT" ]]; then
    read -r -a MPI_FLAGS_ARRAY <<< "$MPI_FLAGS_TEXT"
fi

mkdir -p "$OUTPUT_DIR"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

is_positive_int() {
    [[ "$1" =~ ^[1-9][0-9]*$ ]]
}

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
    awk -v a="$1" -v b="$2" 'BEGIN { if (b == 0) exit 1; printf "%.6f", a / b }'
}

calc_rel_error_pct() {
    awk -v a="$1" -v t="$2" 'BEGIN { if (t == 0) exit 1; d=a-t; if(d<0)d=-d; printf "%.6f", 100.0*d/t }'
}

extract_time() {
    sed -nE 's/.*Tiempo[^:]*:[[:space:]]*([0-9]+([.][0-9]+)?).*/\1/p' | tail -n 1
}

extract_avg_distance() {
    sed -nE 's/.*Distancia Promedio Calc:[[:space:]]*([0-9]+([.][0-9]+)?).*/\1/p' | tail -n 1
}

extract_theoretical_distance() {
    sed -nE 's/.*Distancia Teorica[^:]*:[[:space:]]*([0-9]+([.][0-9]+)?).*/\1/p' | tail -n 1
}

run_mpi() {
    local p="$1"
    local bin="$2"
    shift 2
    "$MPI_RUN" "${MPI_FLAGS_ARRAY[@]}" -np "$p" "./$bin" "$@"
}

max_from_array() {
    local max=0 value
    for value in "$@"; do
        (( value > max )) && max=$value
    done
    echo "$max"
}

unique_dimensions() {
    awk 'BEGIN{seen[1]=1; seen[2]=1; printf "1 2"} {
        for(i=1;i<=NF;i++) if($i>=3 && !seen[$i]++){printf " %d",$i}
    } END{print ""}' <<< "$EXTRA_DIMS"
}

# ------------------------------ Validación -------------------------------------
is_positive_int "$RUNS" || fail "RUNS debe ser un entero positivo."
is_positive_int "$BENCH_STEPS" || fail "BENCH_STEPS debe ser un entero positivo."
is_positive_int "$BENCH_WALKS" || fail "BENCH_WALKS debe ser un entero positivo."
is_positive_int "$PHYSICS_WALKS" || fail "PHYSICS_WALKS debe ser un entero positivo."
is_positive_int "$SAMPLE_STEPS" || fail "SAMPLE_STEPS debe ser un entero positivo."
is_positive_int "$SAMPLE_WALKS" || fail "SAMPLE_WALKS debe ser un entero positivo."

[[ ${#PROCS_ARRAY[@]} -gt 0 ]] || fail "PROCS no puede estar vacío."
[[ "${PROCS_ARRAY[0]}" == "1" ]] || fail "PROCS debe comenzar por 1 para calcular speedup."
for p in "${PROCS_ARRAY[@]}"; do is_positive_int "$p" || fail "PROCS inválido: $p"; done
for m in "${PHYSICS_STEPS_ARRAY[@]}"; do is_positive_int "$m" || fail "PHYSICS_STEPS inválido: $m"; done
for d in $EXTRA_DIMS; do is_positive_int "$d" || fail "Dimensión inválida: $d"; done

MAX_P=$(max_from_array "${PROCS_ARRAY[@]}")
[[ -n "$PHYSICS_NP" ]] || PHYSICS_NP="$MAX_P"
[[ -n "$SAMPLE_NP" ]] || SAMPLE_NP="$PHYSICS_NP"
is_positive_int "$PHYSICS_NP" || fail "PHYSICS_NP inválido."
is_positive_int "$SAMPLE_NP" || fail "SAMPLE_NP inválido."

REQUESTED_MAX="$MAX_P"
(( PHYSICS_NP > REQUESTED_MAX )) && REQUESTED_MAX="$PHYSICS_NP"
(( SAMPLE_NP > REQUESTED_MAX )) && REQUESTED_MAX="$SAMPLE_NP"

if command -v nproc >/dev/null 2>&1; then
    AVAILABLE_CPUS=$(nproc)
    if (( REQUESTED_MAX > AVAILABLE_CPUS )); then
        fail "Se solicitaron hasta $REQUESTED_MAX procesos, pero nproc informa $AVAILABLE_CPUS CPUs disponibles. No se permite oversubscription en benchmark."
    fi
fi

for bin in random_walk_1d_baseline random_walk_2d_baseline random_walk_1d random_walk_2d random_walk_nd; do
    [[ -x "./$bin" ]] || fail "Falta ./$bin. Ejecute 'make' antes del benchmark."
done

# ------------------------------ Metadata ---------------------------------------
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
        echo "bench_steps=$BENCH_STEPS"
        echo "bench_walks=$BENCH_WALKS"
        echo "physics_steps=$PHYSICS_STEPS_TEXT"
        echo "physics_walks=$PHYSICS_WALKS"
        echo "physics_np=$PHYSICS_NP"
        echo "sample_steps=$SAMPLE_STEPS"
        echo "sample_walks=$SAMPLE_WALKS"
        echo "sample_np=$SAMPLE_NP"
        if command -v nproc >/dev/null 2>&1; then echo "nproc=$(nproc)"; fi
        if [[ -r /proc/self/status ]]; then
            grep -E '^Cpus_allowed_list:' /proc/self/status || true
        fi
        if [[ -r /sys/fs/cgroup/cpuset.cpus.effective ]]; then
            echo "cpuset_effective=$(cat /sys/fs/cgroup/cpuset.cpus.effective)"
        fi
        if [[ -r /sys/fs/cgroup/cpu.max ]]; then
            echo "cpu_max=$(cat /sys/fs/cgroup/cpu.max)"
        fi
        if command -v mpicc >/dev/null 2>&1; then echo "mpicc=$(mpicc --version 2>/dev/null | head -n 1)"; fi
        if command -v gcc >/dev/null 2>&1; then echo "gcc=$(gcc --version 2>/dev/null | head -n 1)"; fi
        if command -v lscpu >/dev/null 2>&1; then
            echo "--- lscpu ---"
            lscpu
        fi
    } > "$file"
}

capture_metadata

echo "========================================================================"
echo " CONFIGURACIÓN DEL EXPERIMENTO"
echo "========================================================================"
echo " CPUs disponibles (nproc): ${AVAILABLE_CPUS:-desconocido}"
echo " Procesos strong scaling:   $PROCS_TEXT"
echo " D adicionales:             ${EXTRA_DIMS:-ninguna}"
echo " Benchmark:                 M=$BENCH_STEPS N=$BENCH_WALKS, RUNS=$RUNS"
echo " Barrido físico:            M={$PHYSICS_STEPS_TEXT}, N=$PHYSICS_WALKS, p=$PHYSICS_NP"
echo

# ------------------------------ PASO 1 -----------------------------------------
echo "========================================================================"
echo " PASO 1: BASELINES SECUENCIALES (-O0)"
echo "========================================================================"

for model in 1d 2d; do
    bin="random_walk_${model}_baseline"
    raw="$OUTPUT_DIR/raw_${bin}.csv"
    tmp=$(mktemp)
    trap 'rm -f "$tmp"' EXIT

    echo "dimension,steps,total_walks,corrida,tiempo" > "$raw"
    dim=${model%d}
    echo -n "-> $bin ($RUNS corridas): "

    for ((r=1; r<=RUNS; r++)); do
        out=$("./$bin" "$BENCH_STEPS" "$BENCH_WALKS")
        t=$(printf '%s\n' "$out" | extract_time)
        [[ -n "$t" ]] || fail "No se pudo extraer tiempo de $bin (corrida $r)."
        echo "$t" >> "$tmp"
        echo "$dim,$BENCH_STEPS,$BENCH_WALKS,$r,$t" >> "$raw"
        echo -n "."
    done

    median=$(calc_median < "$tmp")
    rm -f "$tmp"; trap - EXIT
    printf " Mediana: %s s\n" "$median"
    printf "%s\n" "$median" > "$OUTPUT_DIR/t_base_${model}.txt"
done

# ------------------------------ PASO 2 -----------------------------------------
echo
echo "========================================================================"
echo " PASO 2: STRONG SCALING MPI - 1D y 2D"
echo "========================================================================"

for model in 1d 2d; do
    bin="random_walk_${model}"
    csv="$OUTPUT_DIR/metricas_${bin}.csv"
    raw="$OUTPUT_DIR/raw_${bin}.csv"
    t_base=$(tr -d '[:space:]' < "$OUTPUT_DIR/t_base_${model}.txt")
    dim=${model%d}

    echo "dimension,steps,total_walks,procesos,tiempo_mediana,speedup_paralelo,eficiencia,speedup_global" > "$csv"
    echo "dimension,steps,total_walks,procesos,corrida,tiempo" > "$raw"
    echo ">>> $bin | M=$BENCH_STEPS N=$BENCH_WALKS | T_base=$t_base s"

    t1=""
    for p in "${PROCS_ARRAY[@]}"; do
        tmp=$(mktemp); trap 'rm -f "$tmp"' EXIT
        echo -n "  -> p=$p [$RUNS corridas]: "

        for ((r=1; r<=RUNS; r++)); do
            out=$(run_mpi "$p" "$bin" "$BENCH_STEPS" "$BENCH_WALKS")
            t=$(printf '%s\n' "$out" | extract_time)
            [[ -n "$t" ]] || fail "No se pudo extraer tiempo de $bin p=$p corrida=$r."
            echo "$t" >> "$tmp"
            echo "$dim,$BENCH_STEPS,$BENCH_WALKS,$p,$r,$t" >> "$raw"
            echo -n "."
        done

        median=$(calc_median < "$tmp")
        rm -f "$tmp"; trap - EXIT

        if [[ "$p" == "1" ]]; then
            t1="$median"; sp="1.000000"; eff="1.000000"
        else
            sp=$(calc_ratio "$t1" "$median")
            eff=$(calc_ratio "$sp" "$p")
        fi
        global=$(calc_ratio "$t_base" "$median")
        printf " Mediana=%ss | Sp=%sx | E=%s | Sglobal=%sx\n" "$median" "$sp" "$eff" "$global"
        echo "$dim,$BENCH_STEPS,$BENCH_WALKS,$p,$median,$sp,$eff,$global" >> "$csv"
    done
    echo "Resultados: $csv"
    echo
done

# ------------------------------ PASO 3 -----------------------------------------
if [[ -n "$EXTRA_DIMS" ]]; then
    echo "========================================================================"
    echo " PASO 3: STRONG SCALING MPI - GENERALIZACIÓN nD"
    echo "========================================================================"

    for dim in $EXTRA_DIMS; do
        (( dim >= 3 )) || continue
        csv="$OUTPUT_DIR/metricas_random_walk_${dim}d.csv"
        raw="$OUTPUT_DIR/raw_random_walk_${dim}d.csv"
        echo "dimension,steps,total_walks,procesos,tiempo_mediana,speedup_paralelo,eficiencia,speedup_global" > "$csv"
        echo "dimension,steps,total_walks,procesos,corrida,tiempo" > "$raw"
        echo ">>> random_walk_nd | D=$dim M=$BENCH_STEPS N=$BENCH_WALKS"

        t1=""
        for p in "${PROCS_ARRAY[@]}"; do
            tmp=$(mktemp); trap 'rm -f "$tmp"' EXIT
            echo -n "  -> p=$p [$RUNS corridas]: "
            for ((r=1; r<=RUNS; r++)); do
                out=$(run_mpi "$p" random_walk_nd "$dim" "$BENCH_STEPS" "$BENCH_WALKS")
                t=$(printf '%s\n' "$out" | extract_time)
                [[ -n "$t" ]] || fail "No se pudo extraer tiempo nD D=$dim p=$p corrida=$r."
                echo "$t" >> "$tmp"
                echo "$dim,$BENCH_STEPS,$BENCH_WALKS,$p,$r,$t" >> "$raw"
                echo -n "."
            done

            median=$(calc_median < "$tmp")
            rm -f "$tmp"; trap - EXIT
            if [[ "$p" == "1" ]]; then
                t1="$median"; sp="1.000000"; eff="1.000000"
            else
                sp=$(calc_ratio "$t1" "$median")
                eff=$(calc_ratio "$sp" "$p")
            fi
            printf " Mediana=%ss | Sp=%sx | E=%s\n" "$median" "$sp" "$eff"
            echo "$dim,$BENCH_STEPS,$BENCH_WALKS,$p,$median,$sp,$eff," >> "$csv"
        done
        echo "Resultados: $csv"
        echo
    done
fi

# ------------------------------ PASO 4 -----------------------------------------
if [[ "$RUN_PHYSICS" == "1" ]]; then
    echo "========================================================================"
    echo " PASO 4: BARRIDO FÍSICO - DISTANCIA PROMEDIO VS. PASOS"
    echo "========================================================================"
    physics_csv="$OUTPUT_DIR/distancia_vs_pasos.csv"
    echo "dimension,steps,total_walks,procesos,distancia_promedio,distancia_teorica,error_relativo_pct,tiempo" > "$physics_csv"

    ALL_DIMS=$(unique_dimensions)
    for dim in $ALL_DIMS; do
        echo ">>> Dimensión ${dim}D"
        for steps in "${PHYSICS_STEPS_ARRAY[@]}"; do
            if (( dim == 1 )); then
                out=$(run_mpi "$PHYSICS_NP" random_walk_1d "$steps" "$PHYSICS_WALKS")
            elif (( dim == 2 )); then
                out=$(run_mpi "$PHYSICS_NP" random_walk_2d "$steps" "$PHYSICS_WALKS")
            else
                out=$(run_mpi "$PHYSICS_NP" random_walk_nd "$dim" "$steps" "$PHYSICS_WALKS")
            fi

            avg=$(printf '%s\n' "$out" | extract_avg_distance)
            theoretical=$(printf '%s\n' "$out" | extract_theoretical_distance)
            t=$(printf '%s\n' "$out" | extract_time)
            [[ -n "$avg" && -n "$theoretical" && -n "$t" ]] || fail "No se pudo extraer resultado físico D=$dim M=$steps."
            error=$(calc_rel_error_pct "$avg" "$theoretical")
            echo "$dim,$steps,$PHYSICS_WALKS,$PHYSICS_NP,$avg,$theoretical,$error,$t" >> "$physics_csv"
            printf "  M=%-5s | E[R]=%-12s | teoría=%-12s | error=%s %%\n" "$steps" "$avg" "$theoretical" "$error"
        done
    done
    echo "Resultados físicos: $physics_csv"
    echo
fi

# ------------------------------ PASO 5 -----------------------------------------
if [[ "$RUN_SAMPLES" == "1" ]]; then
    echo "========================================================================"
    echo " PASO 5: MUESTRAS ESPACIALES PARA HISTOGRAMAS / DENSIDADES"
    echo "========================================================================"
    sample_meta="$OUTPUT_DIR/muestras_config.csv"
    echo "dimension,steps,total_walks,procesos" > "$sample_meta"

    echo -n "-> Muestra 1D: "
    run_mpi "$SAMPLE_NP" random_walk_1d "$SAMPLE_STEPS" "$SAMPLE_WALKS" --sample >/dev/null
    mv -f posiciones_1d.csv "$OUTPUT_DIR/posiciones_1d.csv"
    echo "1,$SAMPLE_STEPS,$SAMPLE_WALKS,$SAMPLE_NP" >> "$sample_meta"
    echo "OK"

    echo -n "-> Muestra 2D: "
    run_mpi "$SAMPLE_NP" random_walk_2d "$SAMPLE_STEPS" "$SAMPLE_WALKS" --sample >/dev/null
    mv -f posiciones_2d.csv "$OUTPUT_DIR/posiciones_2d.csv"
    echo "2,$SAMPLE_STEPS,$SAMPLE_WALKS,$SAMPLE_NP" >> "$sample_meta"
    echo "OK"

    for dim in $EXTRA_DIMS; do
        (( dim >= 3 )) || continue
        echo -n "-> Muestra ${dim}D: "
        run_mpi "$SAMPLE_NP" random_walk_nd "$dim" "$SAMPLE_STEPS" "$SAMPLE_WALKS" --sample >/dev/null
        mv -f "posiciones_${dim}d.csv" "$OUTPUT_DIR/posiciones_${dim}d.csv"
        echo "$dim,$SAMPLE_STEPS,$SAMPLE_WALKS,$SAMPLE_NP" >> "$sample_meta"
        echo "OK"
    done
    echo
fi

echo "========================================================================"
echo " EXPERIMENTOS FINALIZADOS"
echo " Resultados guardados en: $OUTPUT_DIR"
echo "========================================================================"
